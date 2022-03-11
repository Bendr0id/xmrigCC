/* XMRigCC
 * Copyright 2017-     BenDr0id    <https://github.com/BenDr0id>, <ben@graef.in>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <memory>
#include <chrono>
#include <cstring>
#include <iostream>
#include <utility>
#include <regex>

#ifdef WIN32
#include "win_ports/dirent.h"
#else

#include <dirent.h>

#endif

#include "3rdparty/rapidjson/document.h"
#include "3rdparty/rapidjson/stringbuffer.h"
#include "3rdparty/rapidjson/writer.h"
#include "3rdparty/rapidjson/filewritestream.h"
#include "3rdparty/rapidjson/prettywriter.h"
#include "3rdparty/cpp-httplib/httplib.h"
#include "base/io/log/Log.h"
#include "version.h"
#include "Service.h"
#include "fmt/format.h"

namespace
{
std::string htmlEncode(const std::string& data)
{
  auto result = std::regex_replace(data, std::regex("&"), "&amp;");
  result = std::regex_replace(result, std::regex("<"), "&lt;");
  result = std::regex_replace(result, std::regex(">"), "&gt;");

  return result;
}

std::string sanitize(const std::string& data)
{
  return std::regex_replace(data, std::regex(R"(([^\x20-~]+)|([\\/:?"<>|~;]+))"), "_");
}
};

constexpr static char DEFAULT_MINER[] = "default_miner";

Service::Service(std::shared_ptr<CCServerConfig> config)
  : m_config(std::move(config))
{

}

Service::~Service()
{
  stop();
}

bool Service::start()
{
  m_timer = std::make_shared<Timer>([&]()
  {
    auto time_point = std::chrono::system_clock::now();
    auto now = static_cast<uint64_t>(std::chrono::system_clock::to_time_t(time_point) * 1000);

#ifdef XMRIG_FEATURE_TLS
    if (m_config->usePushover() || m_config->useTelegram() || m_config->useDiscord())
    {
      if (m_config->pushOfflineMiners())
      {
        sendMinerOfflinePush(now);
      }

      if (m_config->pushZeroHashrateMiners())
      {
        sendMinerZeroHashratePush(now);
      }

      if (m_config->pushPeriodicStatus())
      {
        if (now > (m_lastStatusUpdateTime + STATUS_UPDATE_INTERVAL))
        {
          sendServerStatusPush(now);
          m_lastStatusUpdateTime = now;
        }
      }
    }
#endif

    if (now > (m_lastStatisticsUpdateTime + STATISTICS_UPDATE_INTERVAL))
    {
      updateStatistics(now);
      m_lastStatisticsUpdateTime = now;
    }
  }, TIMER_INTERVAL);

  m_timer->start();

  return true;
}

void Service::stop()
{
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_timer)
  {
    m_timer->stop();
  }

  m_clientCommand.clear();
  m_clientStatus.clear();
  m_clientLog.clear();
}

int Service::handleGET(const httplib::Request& req, httplib::Response& res)
{
  int resultCode = HTTP_NOT_FOUND;

  const auto clientId = req.get_param_value("clientId");
  const auto removeAddr = req.get_header_value("REMOTE_ADDR");

  if (req.path == "/")
  {
    resultCode = getAdminPage(res);
  }
  else if (req.path.rfind("/admin/getClientStatusList", 0) == 0)
  {
    resultCode = getClientStatusList(res);
  }
  else if (req.path.rfind("/admin/getClientConfigTemplates", 0) == 0)
  {
    resultCode = getClientConfigTemplates(res);
  }
  else if (req.path.rfind("/admin/getClientStatistics", 0) == 0)
  {
    resultCode = getClientStatistics(res);
  }
  else
  {
    if (!clientId.empty())
    {
      if (req.path.rfind("/client/getConfig", 0) == 0 || req.path.rfind("/admin/getClientConfig", 0) == 0)
      {
        resultCode = getClientConfig(clientId, res);
      }
      else if (req.path.rfind("/admin/getClientCommand", 0) == 0)
      {
        resultCode = getClientCommand(clientId, res);
      }
      else if (req.path.rfind("/admin/getClientLog", 0) == 0)
      {
        resultCode = getClientLog(clientId, res);
      }
      else
      {
        LOG_WARN("[%s] 404 NOT FOUND (%s)", removeAddr.c_str(), req.path.c_str());
      }
    }
    else
    {
      resultCode = HTTP_BAD_REQUEST;
      LOG_ERR("[%s] 400 BAD REQUEST - Request does not contain clientId (%s)",
              removeAddr.c_str(), req.path.c_str());
    }
  }

  return resultCode;
}

int Service::handlePOST(const httplib::Request& req, httplib::Response& res)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  int resultCode = HTTP_NOT_FOUND;

  const auto clientId = req.get_param_value("clientId");
  const auto remoteAddr = req.get_header_value("REMOTE_ADDR");

  if (!clientId.empty())
  {
    if (req.path.rfind("/client/setClientStatus", 0) == 0)
    {
      resultCode = setClientStatus(req, clientId, res);
    }
    else if (req.path.rfind("/admin/setClientConfig", 0) == 0 || req.path.rfind("/client/setClientConfig", 0) == 0)
    {
      resultCode = setClientConfig(req, clientId, res);
    }
    else if (req.path.rfind("/admin/setClientCommand", 0) == 0)
    {
      resultCode = setClientCommand(req, clientId, res);
    }
    else if (req.path.rfind("/admin/deleteClientConfig", 0) == 0)
    {
      resultCode = deleteClientConfig(clientId);
    }
    else
    {
      resultCode = HTTP_BAD_REQUEST;
      LOG_WARN("[%s] 400 BAD REQUEST - Request does not contain clientId (%s)", remoteAddr.c_str(), req.path.c_str());
    }
  }
  else
  {
    if (req.path.rfind("/admin/resetClientStatusList", 0) == 0)
    {
      resultCode = resetClientStatusList();
    }
    else
    {
      LOG_WARN("[%s] 404 NOT FOUND (%s)", remoteAddr.c_str(), req.path.c_str());
    }
  }

  return resultCode;
}

int Service::getAdminPage(httplib::Response& res)
{
  std::stringstream data;

  std::ifstream customDashboard(m_config->customDashboard());
  if (customDashboard)
  {
    data << customDashboard.rdbuf();
    customDashboard.close();
  }

  if (!data.rdbuf()->in_avail())
  {
    data << "<!DOCTYPE html>";
    data << "<html lang=\"en\">";
    data << "<head>";
    data << "<meta charset=\"utf-8\">";
    data << "<title>XMRigCC Dashboard</title>";
    data << "</head>";
    data << "<body>";
    data << "    <div style=\"text-align: center;\">";
    data << "       <h1>Please configure a Dashboard</h1>";
    data << "    </div>";
    data << "</body>";
    data << "</html>";
  }

  res.set_content(data.str(), CONTENT_TYPE_HTML);

  return HTTP_OK;
}

int Service::getClientStatusList(httplib::Response& res)
{
  rapidjson::Document respDocument;
  respDocument.SetObject();

  auto& allocator = respDocument.GetAllocator();

  rapidjson::Value clientStatusList(rapidjson::kArrayType);
  for (auto& clientStatus: m_clientStatus)
  {
    rapidjson::Value clientStatusEntry(rapidjson::kObjectType);
    clientStatusEntry.AddMember("client_status", clientStatus.second.toJson(allocator), allocator);
    clientStatusList.PushBack(clientStatusEntry, allocator);
  }

  auto time_point = std::chrono::system_clock::now();
  m_currentServerTime = static_cast<uint64_t>(std::chrono::system_clock::to_time_t(time_point));

  respDocument.AddMember("current_server_time", m_currentServerTime, allocator);
  respDocument.AddMember("current_version", rapidjson::StringRef(APP_VERSION), allocator);
  respDocument.AddMember("client_status_list", clientStatusList, allocator);

  rapidjson::StringBuffer buffer(0, 4096);
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.SetMaxDecimalPlaces(10);
  respDocument.Accept(writer);

  res.set_content(buffer.GetString(), CONTENT_TYPE_JSON);

  return HTTP_OK;
}

int Service::getClientStatistics(httplib::Response& res)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  rapidjson::Document respDocument;
  respDocument.SetObject();

  auto& allocator = respDocument.GetAllocator();

  rapidjson::Value clientStatistics(rapidjson::kArrayType);

  for (const auto& statistics: m_statistics)
  {
    rapidjson::Value algoStatistics(rapidjson::kObjectType);
    algoStatistics.AddMember("algo", rapidjson::StringRef(statistics.first.c_str()), allocator);

    rapidjson::Value algoStatisticEntries(rapidjson::kArrayType);
    for (const auto& algoStatistic: statistics.second)
    {
      rapidjson::Value algoStatisticEntry(rapidjson::kObjectType);
      algoStatisticEntry.AddMember("timestamp", algoStatistic.first, allocator);
      algoStatisticEntry.AddMember("hashrate", algoStatistic.second.first, allocator);
      algoStatisticEntry.AddMember("miner", algoStatistic.second.second, allocator);

      algoStatisticEntries.PushBack(algoStatisticEntry, allocator);
    }

    algoStatistics.AddMember("statistics", algoStatisticEntries, allocator);
    clientStatistics.PushBack(algoStatistics, allocator);
  }

  respDocument.AddMember("client_statistics", clientStatistics, allocator);

  rapidjson::StringBuffer buffer(0, 4096);
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.SetMaxDecimalPlaces(10);
  respDocument.Accept(writer);

  res.set_content(buffer.GetString(), CONTENT_TYPE_JSON);

  return HTTP_OK;
}

int Service::setClientStatus(const httplib::Request& req, const std::string& clientId, httplib::Response& res)
{
  int resultCode = HTTP_BAD_REQUEST;

  const auto remoteAddr = req.get_header_value("REMOTE_ADDR");

  auto payload = htmlEncode(req.body);

  rapidjson::Document respDocument;
  if (!respDocument.Parse(payload.c_str()).HasParseError())
  {
    ClientStatus clientStatus;
    clientStatus.parseFromJson(respDocument);
    clientStatus.setExternalIp(remoteAddr);

    setClientLog(static_cast<size_t>(m_config->clientLogHistory()), clientId, clientStatus.getLog());

    clientStatus.clearLog();

    m_clientStatus[clientId] = clientStatus;

    resultCode = getClientCommand(clientId, res);

    if (m_clientCommand[clientId].isOneTimeCommand())
    {
      m_clientCommand.erase(clientId);
    }
  }
  else
  {
    LOG_ERR("[%s] ClientStatus for client '%s' - Parse Error Occured: %d",
            remoteAddr.c_str(), clientId.c_str(), respDocument.GetParseError());
  }

  return resultCode;
}

void Service::setClientLog(size_t maxRows, const std::string& clientId, const std::string& log)
{
  if (m_clientLog.find(clientId) == m_clientLog.end())
  {
    m_clientLog[clientId] = std::list<std::string>();
  }

  auto* clientLog = &m_clientLog[clientId];
  std::istringstream logStream(log);

  std::string logLine;
  while (std::getline(logStream, logLine))
  {
    if (clientLog->size() == maxRows)
    {
      clientLog->pop_front();
    }

    clientLog->push_back(logLine);
  }
}

int Service::getClientCommand(const std::string& clientId, httplib::Response& res)
{
  if (m_clientCommand.find(clientId) == m_clientCommand.end())
  {
    m_clientCommand[clientId] = ControlCommand();
  }

  rapidjson::Document respDocument;
  respDocument.SetObject();

  auto& allocator = respDocument.GetAllocator();

  rapidjson::Value controlCommand = m_clientCommand[clientId].toJson(allocator);
  respDocument.AddMember("control_command", controlCommand, allocator);

  rapidjson::StringBuffer buffer(0, 4096);
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.SetMaxDecimalPlaces(10);
  respDocument.Accept(writer);

  res.set_content(buffer.GetString(), CONTENT_TYPE_JSON);

  return HTTP_OK;
}

int Service::getClientConfigTemplates(httplib::Response& res)
{
  std::string configFolder(".");

  if (!m_config->clientConfigFolder().empty())
  {
    configFolder = m_config->clientConfigFolder();
#       ifdef WIN32
    configFolder += '\\';
#       else
    configFolder += '/';
#       endif
  }

  std::vector<std::string> templateFiles;

  DIR* dirp = opendir(configFolder.c_str());
  if (dirp)
  {
    struct dirent* entry;
    while ((entry = readdir(dirp)) != NULL)
    {
      if (entry->d_type == DT_REG)
      {
        std::string filename = entry->d_name;
        std::string starting = "template_";
        std::string ending = "_config.json";

        if (filename.rfind(starting, 0) == 0 &&
            filename.find(ending, (filename.length() - ending.length())) != std::string::npos)
        {
          filename.erase(0, starting.length());
          filename.erase(filename.length() - ending.length());

          templateFiles.push_back(filename);
        }
      }
    }

    closedir(dirp);
  }

  rapidjson::Document respDocument;
  respDocument.SetObject();

  auto& allocator = respDocument.GetAllocator();

  rapidjson::Value templateList(rapidjson::kArrayType);
  for (auto& templateFile: templateFiles)
  {
    rapidjson::Value templateEntry(templateFile.c_str(), allocator);
    templateList.PushBack(templateEntry, allocator);
  }

  respDocument.AddMember("templates", templateList, allocator);

  rapidjson::StringBuffer buffer(0, 4096);
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.SetMaxDecimalPlaces(10);
  respDocument.Accept(writer);

  res.set_content(buffer.GetString(), CONTENT_TYPE_JSON);

  return HTTP_OK;
}

int Service::getClientConfig(const std::string& clientId, httplib::Response& res)
{
  int resultCode = HTTP_INTERNAL_ERROR;

  std::string clientConfigFileName = getClientConfigFileName(clientId);

  std::stringstream data;
  std::ifstream clientConfig(clientConfigFileName);
  if (clientConfig)
  {
    data << clientConfig.rdbuf();
    clientConfig.close();
  }
  else
  {
    std::string defaultConfigFileName = getClientConfigFileName(DEFAULT_MINER);
    std::ifstream defaultConfig(defaultConfigFileName);
    if (defaultConfig)
    {
      data << defaultConfig.rdbuf();
      defaultConfig.close();
    }
  }

  if (data.tellp() > 0)
  {
    rapidjson::Document respDocument;
    respDocument.Parse(data.str().c_str());

    if (!respDocument.HasParseError())
    {
      rapidjson::StringBuffer buffer(0, 4096);
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      writer.SetMaxDecimalPlaces(10);
      respDocument.Accept(writer);

      res.set_content(buffer.GetString(), CONTENT_TYPE_JSON);

      resultCode = HTTP_OK;
    }
    else
    {
      LOG_ERR("Not able to send client config. Client config %s is broken!", clientConfigFileName.c_str());
    }
  }
  else
  {
    LOG_ERR("Not able to load a client config. Please check your configuration!");
  }

  return resultCode;
}

int Service::getClientLog(const std::string& clientId, httplib::Response& res)
{
  if (m_clientLog.find(clientId) != m_clientLog.end())
  {
    rapidjson::Document respDocument;
    respDocument.SetObject();

    auto& allocator = respDocument.GetAllocator();

    std::stringstream data;
    for (auto& m_row: m_clientLog[clientId])
    {
      data << m_row.c_str() << std::endl;
    }

    std::string log = data.str();
    respDocument.AddMember("client_log", rapidjson::StringRef(log.c_str()), allocator);

    rapidjson::StringBuffer buffer(0, 4096);
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.SetMaxDecimalPlaces(10);
    respDocument.Accept(writer);

    res.set_content(buffer.GetString(), CONTENT_TYPE_JSON);
  }

  return HTTP_OK;
}

int Service::setClientConfig(const httplib::Request& req, const std::string& clientId, httplib::Response& res)
{
  int resultCode = HTTP_BAD_REQUEST;

  std::string remoteAddr = req.get_header_value("REMOTE_ADDR");

  rapidjson::Document respDocument;
  if (!respDocument.Parse(req.body.c_str()).HasParseError())
  {
    std::string clientConfigFileName = getClientConfigFileName(clientId);
    std::string defaultMinerConfigFileName = getClientConfigFileName(DEFAULT_MINER);

    if (clientConfigFileName != defaultMinerConfigFileName)
    {
      std::ofstream clientConfigFile(clientConfigFileName);

      if (clientConfigFile)
      {
        rapidjson::StringBuffer buffer(0, 4096);
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetMaxDecimalPlaces(10);
        respDocument.Accept(writer);

        clientConfigFile << buffer.GetString();
        clientConfigFile.close();

        resultCode = HTTP_OK;
      }
      else
      {
        LOG_ERR("[%s] Not able to store client config to file %s.", remoteAddr.c_str(), clientConfigFileName.c_str());
      }
    }
    else
    {
      LOG_WARN("[%s] Someone is trying to override our %s file. Rejected!", remoteAddr.c_str(),
               defaultMinerConfigFileName.c_str());
    }
  }
  else
  {
    LOG_ERR("[%s] Not able to store client config. The received client config for client %s is broken!",
            remoteAddr.c_str(), clientId.c_str());
  }

  return resultCode;
}

int Service::setClientCommand(const httplib::Request& req, const std::string& clientId, httplib::Response& res)
{
  int resultCode = HTTP_BAD_REQUEST;

  ControlCommand controlCommand;

  rapidjson::Document respDocument;
  if (!respDocument.Parse(req.body.c_str()).HasParseError())
  {
    controlCommand.parseFromJson(respDocument);

    m_clientCommand[clientId] = controlCommand;

    resultCode = HTTP_OK;
  }

  return resultCode;
}


int Service::deleteClientConfig(const std::string& clientId)
{
  int resultCode = HTTP_BAD_REQUEST;


  if (!clientId.empty())
  {
    std::string clientConfigFileName = getClientConfigFileName(clientId);
    if (!clientConfigFileName.empty() && remove(clientConfigFileName.c_str()) == 0)
    {
      resultCode = HTTP_OK;
    }
    else
    {
      resultCode = HTTP_NOT_FOUND;
    }
  }

  return resultCode;
}

int Service::resetClientStatusList()
{
  m_clientStatus.clear();

  return HTTP_OK;
}

std::string Service::getClientConfigFileName(const std::string& clientId)
{
  std::string clientConfigFileName;

  if (!m_config->clientConfigFolder().empty())
  {
    clientConfigFileName += m_config->clientConfigFolder();
#       ifdef WIN32
    clientConfigFileName += '\\';
#       else
    clientConfigFileName += '/';
#       endif
  }

  clientConfigFileName += sanitize(clientId) + std::string("_config.json");

  return clientConfigFileName;
}

void Service::sendMinerOfflinePush(uint64_t now)
{
  uint64_t offlineThreshold = now - OFFLINE_TRESHOLD_IN_MS;

  for (const auto& clientStatus: m_clientStatus)
  {
    uint64_t lastStatus = clientStatus.second.getLastStatusUpdate() * 1000;
    if (lastStatus < offlineThreshold)
    {
      if (std::find(m_offlineNotified.begin(), m_offlineNotified.end(), clientStatus.first) == m_offlineNotified.end())
      {
        std::stringstream message;
        message << "Miner: " << httplib::detail::encode_url(clientStatus.first) << " just went offline!";

        LOG_WARN("Send miner %s went offline push", httplib::detail::encode_url(clientStatus.first).c_str());
        triggerPush(APP_NAME " Onlinestatus Monitor", message.str());

        m_offlineNotified.push_back(clientStatus.first);
      }
    }
    else
    {
      if (std::find(m_offlineNotified.begin(), m_offlineNotified.end(), clientStatus.first) != m_offlineNotified.end())
      {
        std::stringstream message;
        message << "Miner: " << httplib::detail::encode_url(clientStatus.first) << " is back online!";

        LOG_WARN("Send miner %s back online push", httplib::detail::encode_url(clientStatus.first).c_str());
        triggerPush(APP_NAME " Onlinestatus Monitor", message.str());

        m_offlineNotified.remove(clientStatus.first);
      }
    }
  }
}

void Service::sendMinerZeroHashratePush(uint64_t now)
{
  uint64_t offlineThreshold = now - OFFLINE_TRESHOLD_IN_MS;

  for (const auto& clientStatus: m_clientStatus)
  {
    if (offlineThreshold < clientStatus.second.getLastStatusUpdate() * 1000)
    {
      if (clientStatus.second.getHashrateShort() == 0 && clientStatus.second.getHashrateMedium() == 0)
      {
        if (m_zeroHashNotified.find(clientStatus.first) == m_zeroHashNotified.end())
        {
          // lets put the miner to the list, on the next iteration it will be either notified or removed if it recovers
          m_zeroHashNotified[clientStatus.first] = now + ZERO_HASHRATE_TRESHOLD_IN_MS;
        }
        else
        {
          if (m_zeroHashNotified[clientStatus.first] > 0 && m_zeroHashNotified[clientStatus.first] < now)
          {
            std::stringstream message;
            message << "Miner: " << httplib::detail::encode_url(clientStatus.first)
                    << " reported 0 h/s for over a minute!";

            LOG_WARN("Send miner %s 0 hashrate push", httplib::detail::encode_url(clientStatus.first).c_str());
            triggerPush(APP_NAME " Hashrate Monitor", message.str());

            m_zeroHashNotified[clientStatus.first] = 0;
          }
        }
      }
      else if (clientStatus.second.getHashrateMedium() > 0)
      {
        if (m_zeroHashNotified.find(clientStatus.first) != m_zeroHashNotified.end())
        {
          if (m_zeroHashNotified[clientStatus.first] == 0)
          {
            std::stringstream message;
            message << "Miner: " << httplib::detail::encode_url(clientStatus.first) << " hashrate recovered. Reported "
                    << clientStatus.second.getHashrateMedium()
                    << " h/s within the last minute!";

            LOG_WARN("Send miner %s hashrate recovered push", httplib::detail::encode_url(clientStatus.first).c_str());
            triggerPush(APP_NAME " Hashrate Monitor", message.str());
          }

          m_zeroHashNotified.erase(clientStatus.first);
        }
      }
      else if (clientStatus.second.getHashrateShort() > 0)
      {
        m_zeroHashNotified.erase(clientStatus.first);
      }
    }
  }
}

void Service::sendServerStatusPush(uint64_t now)
{
  size_t onlineMiner = 0;
  size_t offlineMiner = 0;

  double hashrateMedium = 0;
  double hashrateLong = 0;
  double avgTime = 0;

  uint64_t sharesGood = 0;
  uint64_t sharesTotal = 0;
  uint64_t offlineThreshold = now - OFFLINE_TRESHOLD_IN_MS;

  for (const auto& clientStatus: m_clientStatus)
  {
    if (offlineThreshold < clientStatus.second.getLastStatusUpdate() * 1000)
    {
      onlineMiner++;

      hashrateMedium += clientStatus.second.getHashrateMedium();
      hashrateLong += clientStatus.second.getHashrateLong();

      sharesGood += clientStatus.second.getSharesGood();
      sharesTotal += clientStatus.second.getSharesTotal();
      avgTime += clientStatus.second.getAvgTime();
    }
    else
    {
      offlineMiner++;
    }
  }

  if (!m_clientStatus.empty())
  {
    avgTime = avgTime / m_clientStatus.size();
  }

  std::stringstream message;
  message << "Miners: " << onlineMiner << " (Online), " << offlineMiner << " (Offline)\n"
          << "Shares: " << sharesGood << " (Good), " << sharesTotal - sharesGood << " (Bad)\n"
          << "Hashrates: " << hashrateMedium << "h/s (1min), " << hashrateLong << "h/s (15min)\n"
          << "Avg. Time: " << avgTime << "s";

  LOG_WARN("Send Server status push");
  triggerPush(APP_NAME " Status", message.str());
}

void Service::triggerPush(const std::string& title, const std::string& message)
{
  if (m_config->usePushover())
  {
    sendViaPushover(title, message);
  }

  if (m_config->useTelegram())
  {
    sendViaTelegram(title, message);
  }

  if (m_config->useDiscord())
  {
    sendViaDiscord(title, message);
  }
}

void Service::sendViaPushover(const std::string& title, const std::string& message)
{
  auto cli = std::make_shared<httplib::SSLClient>("api.pushover.net", 443);
  cli->enable_server_certificate_verification(false);

  httplib::Params params;
  params.emplace("token", m_config->pushoverApiToken());
  params.emplace("user", m_config->pushoverUserKey());
  params.emplace("title", title);
  params.emplace("message", message);

  auto res = cli->Post("/1/messages.json", params);
  if (res)
  {
    LOG_WARN("Pushover response: %s", res->body.c_str());
  }
  else
  {
    LOG_ERR("Failed to connect to the Pushover API");
  }
}

void Service::sendViaTelegram(const std::string& title, const std::string& message)
{
  auto cli = std::make_shared<httplib::SSLClient>("api.telegram.org", 443);
  cli->enable_server_certificate_verification(false);

  std::string text = "<b>" + title + "</b>\n\n" + message;
  std::string path = std::string("/bot") + m_config->telegramBotToken() + std::string("/sendMessage");

  httplib::Params params;
  params.emplace("chat_id", m_config->telegramChatId());
  params.emplace("text", text);
  params.emplace("parse_mode", "HTML");

  auto res = cli->Post(path.c_str(), params);
  if (res)
  {
    LOG_WARN("Telegram response: %s", res->body.c_str());
  }
  else
  {
    LOG_ERR("Failed to connect to the Telegram API");
  }
}

void Service::sendViaDiscord(const std::string& title, const std::string& message)
{
  std::smatch matcher;

  auto webHookUrl = m_config->discordWebhookUrl();
  if (std::regex_match(webHookUrl, matcher, std::regex(R"((?:(https?):)?(?://(discord.com:\[([\d:]+)\]|([^:/?#]+))(?::(\d+))?)?([^?#]*(?:\?[^#]*)?)(?:#.*)?)")))
  {
    auto cli = std::make_shared<httplib::SSLClient>("discord.com", 443);
    cli->enable_server_certificate_verification(false);

    auto description = std::regex_replace(message, std::regex("\n"), "\\n");
    auto body = fmt::format(R"({{"username": "{}", "embeds": [{{ "title": "{}", "description": "{}"}}]}})",
                               APP_NAME,
                               title,
                               description);

    auto path = matcher[5].str();
    if (!path.empty())
    {
      auto res = cli->Post(path.c_str(), body, CONTENT_TYPE_JSON);
      if (res)
      {
        LOG_WARN("Discord response: %s [%d]", res->body.c_str(), res->status);
      }
      else
      {
        LOG_ERR("Failed to connect to the Discord API");
      }
    }
  }
  else
  {
    LOG_ERR("Malformed Discord WebHook URL");
  }
}

void Service::updateStatistics(uint64_t now)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto offlineThreshold = now - OFFLINE_TRESHOLD_IN_MS;

  for (const auto& clientStatus: m_clientStatus)
  {
    uint64_t lastStatus = clientStatus.second.getLastStatusUpdate() * 1000;
    if (lastStatus > offlineThreshold)
    {
      auto& algoStatistics = m_statistics[clientStatus.second.getCurrentAlgoName()];
      if (algoStatistics.find(now) == algoStatistics.end())
      {
        algoStatistics[now].first = 0;
        algoStatistics[now].second = 0;
      }

      algoStatistics[now].first += clientStatus.second.getHashrateMedium();
      algoStatistics[now].second++;
    }
  }

  for (auto statisticsIt = m_statistics.begin(); statisticsIt != m_statistics.end();)
  {
    for (auto algoStatisticsIt = statisticsIt->second.begin(); algoStatisticsIt != statisticsIt->second.end();)
    {
      if (algoStatisticsIt->first < (now - 86400000))
      {
        algoStatisticsIt = statisticsIt->second.erase(algoStatisticsIt);
      }
      else
      {
        ++algoStatisticsIt;
      }
    }

    if (statisticsIt->second.empty())
    {
      statisticsIt = m_statistics.erase(statisticsIt);
    }
    else
    {
      ++statisticsIt;
    }
  }
}
