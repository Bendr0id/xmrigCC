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

#include <sstream>
#include "3rdparty/rapidjson/prettywriter.h"
#include <crypto/common/VirtualMemory.h>
#include "base/io/Env.h"
#include "base/io/log/Tags.h"

#include "backend/cpu/Cpu.h"
#include "base/tools/Timer.h"
#include "base/tools/Chrono.h"
#include "base/kernel/Base.h"
#include "base/kernel/Platform.h"
#include "base/kernel/Process.h"

#include "base/cc/interfaces/IClientStatusListener.h"
#include "base/cc/interfaces/ICommandListener.h"

#include "CCClient.h"
#include "App.h"
#include "version.h"

#include "core/config/Config.h"
#include "base/io/log/Log.h"
#include "base/io/log/backends/RemoteLog.h"

#if _WIN32
#   include "winsock2.h"
#else
#   include "unistd.h"
#endif

namespace
{
  constexpr static int HTTP_OK = 200;

  static std::string VersionString()
  {
    std::string version = std::to_string(APP_VER_MAJOR) + std::string(".") + std::to_string(APP_VER_MINOR) +
                          std::string(".") + std::to_string(APP_VER_PATCH);
    return version;
  }
}

xmrig::CCClient::CCClient(Base* base)
  : m_base(base),
    m_startTime(Chrono::currentMSecsSinceEpoch()),
    m_configPublishedOnStart(false),
    m_timer(nullptr)
{
  base->addListener(this);

  m_timer = new Timer(this);
}


xmrig::CCClient::~CCClient()
{
  LOG_DEBUG("CCClient::~CCCLient()");
  delete m_timer;
}

void xmrig::CCClient::start()
{
  LOG_DEBUG("CCClient::start");

  updateClientInfo();

  m_timer->start(static_cast<uint64_t>(m_base->config()->ccClient().updateInterval() * 1000),
                 static_cast<uint64_t>(m_base->config()->ccClient().updateInterval() * 1000));
}

void xmrig::CCClient::updateClientInfo()
{
  LOG_DEBUG("CCClient::updateClientInfo");

  std::string clientId;
  if (m_base->config()->ccClient().workerId())
  {
    clientId = Env::expand(m_base->config()->ccClient().workerId());
  }
  else
  {
    clientId = Env::hostname();
  }

  auto cpuInfo = xmrig::Cpu::info();

  m_clientStatus.setClientId(clientId);
  m_clientStatus.setVersion(VersionString());
  m_clientStatus.setCpuBrand(cpuInfo->brand());
  m_clientStatus.setCpuAES(cpuInfo->hasAES());
  m_clientStatus.setCpuSockets(static_cast<int>(cpuInfo->packages()));
  m_clientStatus.setCpuCores(static_cast<int>(cpuInfo->cores()));
  m_clientStatus.setCpuThreads(static_cast<int>(cpuInfo->threads()));
  m_clientStatus.setCpuX64(ICpuInfo::is64bit());
  m_clientStatus.setVM(cpuInfo->isVM());
  m_clientStatus.setCpuL2(static_cast<int>(cpuInfo->L2() / 1024));
  m_clientStatus.setCpuL3(static_cast<int>(cpuInfo->L3() / 1024));
  m_clientStatus.setNodes(static_cast<int>(cpuInfo->nodes()));

#   ifdef XMRIG_FEATURE_ASM
  const xmrig::Assembly assembly = Cpu::assembly(cpuInfo->assembly());
  m_clientStatus.setAssembly(assembly.toString());
#   else
  m_clientStatus.setAssembly("none");
#   endif
}


void xmrig::CCClient::stop()
{
  LOG_DEBUG("CCClient::stop");

  m_configPublishedOnStart = false;

  if (m_timer)
  {
    m_timer->stop();
  }

  if (m_thread.joinable())
  {
    m_thread.join();
  }
}

void xmrig::CCClient::updateStatistics()
{
  LOG_DEBUG("CCClient::updateStatistics");

  for (IClientStatusListener* listener : m_ClientStatuslisteners)
  {
    listener->onUpdateRequest(m_clientStatus);
  }
}


void xmrig::CCClient::publishClientStatusReport()
{
  LOG_DEBUG("CCClient::publishClientStatusReport");

  std::string requestUrl = "/client/setClientStatus?clientId=" + m_clientStatus.getClientId();
  std::string requestBuffer = m_clientStatus.toJsonString();

  auto config = m_base->config()->ccClient();

  auto res = performRequest(requestUrl, requestBuffer, "POST");
  if (!res)
  {
    LOG_ERR(CLEAR "%s" RED("error:unable to performRequest POST [http%s://%s:%d%s]"), Tags::cc(),
            config.useTLS() ? "s" : "", config.host(), config.port(), requestUrl.c_str());
  }
  else if (res->status != HTTP_OK)
  {
    LOG_ERR(CLEAR "%s" RED("error:\"%d\" [http%s://%s:%d%s]"), Tags::cc(), res->status,
            config.useTLS() ? "s" : "", config.host(), config.port(), requestUrl.c_str());
  }
  else
  {
    LOG_DEBUG("CCClient::publishClientStatusReport received: '%s'", res->body.c_str());

    ControlCommand controlCommand;
    if (controlCommand.parseFromJsonString(res->body))
    {
      if (controlCommand.getCommand() == ControlCommand::START)
      {
        LOG_DEBUG(CLEAR "%s Command: RESUME received", Tags::cc());
      }
      else if (controlCommand.getCommand() == ControlCommand::STOP)
      {
        LOG_DEBUG(CLEAR "%s Command: PAUSE received", Tags::cc());
      }
      else if (controlCommand.getCommand() == ControlCommand::UPDATE_CONFIG)
      {
        LOG_WARN(CLEAR "%s" YELLOW("Command: UPDATE_CONFIG received"), Tags::cc());
        fetchConfig();
      }
      else if (controlCommand.getCommand() == ControlCommand::PUBLISH_CONFIG)
      {
        LOG_WARN(CLEAR "%s" YELLOW("Command: PUBLISH_CONFIG received"), Tags::cc());
        publishConfig();
      }
      else if (controlCommand.getCommand() == ControlCommand::RESTART)
      {
        LOG_WARN(CLEAR "%s" YELLOW("Command: RESTART received"), Tags::cc());
      }
      else if (controlCommand.getCommand() == ControlCommand::SHUTDOWN)
      {
        LOG_WARN(CLEAR "%s" YELLOW("Command: SHUTDOWN received"), Tags::cc());
      }
      else if (controlCommand.getCommand() == ControlCommand::REBOOT)
      {
        LOG_WARN(CLEAR "%s" YELLOW("Command: REBOOT received"), Tags::cc());
      }
      else if (controlCommand.getCommand() == ControlCommand::EXECUTE)
      {
        LOG_WARN(CLEAR "%s" YELLOW("Command: EXECUTE received"), Tags::cc());
      }
      else if (controlCommand.getCommand() == ControlCommand::UPDATE)
      {
        LOG_WARN(CLEAR "%s" YELLOW("Command: UPDATE received"), Tags::cc());
        fetchUpdate();
      }

      for (ICommandListener* listener : m_Commandlisteners)
      {
        listener->onCommandReceived(controlCommand);
      }
    }
    else
    {
      LOG_ERR(CLEAR "%s" RED("Unknown command received from CC Server."), Tags::cc());
    }
  }
}

void xmrig::CCClient::fetchConfig()
{
  LOG_DEBUG("CCClient::fetchConfig");

  auto config = m_base->config()->ccClient();

  std::string requestUrl = "/client/getConfig?clientId=" + m_clientStatus.getClientId();
  std::string requestBuffer;

  auto res = performRequest(requestUrl, requestBuffer, "GET");
  if (!res)
  {
    LOG_ERR(CLEAR "%s" RED("error:unable to performRequest GET [http%s://%s:%d%s]"), Tags::cc(),
            config.useTLS() ? "s" : "", config.host(), config.port(), requestUrl.c_str());
  }
  else if (res->status != HTTP_OK)
  {
    LOG_ERR(CLEAR "%s" RED("error:\"%d\" [http%s://%s:%d%s]"), Tags::cc(), res->status,
            config.useTLS() ? "s" : "", config.host(), config.port(), requestUrl.c_str());
  }
  else
  {
    LOG_DEBUG("CCClient::fetchConfig received: '%s'", res->body.c_str());

    rapidjson::Document document;
    if (!document.Parse(res->body.c_str()).HasParseError())
    {
      std::ofstream clientConfigFile(m_base->config()->fileName());
      if (clientConfigFile)
      {
        rapidjson::StringBuffer buffer(0, 4096);
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetMaxDecimalPlaces(10);
        document.Accept(writer);

        clientConfigFile << buffer.GetString();
        clientConfigFile.close();

        if (!m_base->config()->isWatch())
        {
          static_cast<IWatcherListener*>(m_base)->onFileChanged(m_base->config()->fileName());
        }

        LOG_WARN(CLEAR "%s" YELLOW("Config updated."), Tags::cc());
      }
      else
      {
        LOG_ERR(CLEAR "%s" RED("Not able to store client config to file %s."), Tags::cc(),
                m_base->config()->fileName().data());
      }
    }
    else
    {
      LOG_ERR(CLEAR "%s" RED("Not able to store client config. received client config is broken!"), Tags::cc());
    }
  }
}

void xmrig::CCClient::publishConfig()
{
  LOG_DEBUG("CCClient::publishConfig");

  auto config = m_base->config()->ccClient();

  std::string requestUrl = "/client/setClientConfig?clientId=" + m_clientStatus.getClientId();

  std::stringstream data;
  std::ifstream clientConfig(m_base->config()->fileName());

  if (clientConfig)
  {
    data << clientConfig.rdbuf();
    clientConfig.close();
  }

  if (data.tellp() > 0)
  {
    rapidjson::Document document;
    document.Parse(data.str().c_str());

    if (!document.HasParseError())
    {
      rapidjson::StringBuffer buffer(0, 4096);
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      writer.SetMaxDecimalPlaces(10);
      document.Accept(writer);

      auto res = performRequest(requestUrl, buffer.GetString(), "POST");
      if (!res)
      {
        LOG_ERR(CLEAR "%s" RED("error:unable to performRequest POST [http%s://%s:%d%s]"), Tags::cc(),
                config.useTLS() ? "s" : "", config.host(), config.port(), requestUrl.c_str());
      }
      else if (res->status != HTTP_OK)
      {
        LOG_ERR(CLEAR "%s" RED("error:\"%d\" POST [http%s://%s:%d%s]"), Tags::cc(), res->status, config.host(),
                config.useTLS() ? "s" : "", config.port(), requestUrl.c_str());
      }
      else
      {
        LOG_DEBUG("CCClient::publishConfig received: '%s'", res->body.c_str());
      }
    }
    else
    {
      LOG_ERR(CLEAR "%s" RED("Not able to send config. Client config %s is broken!"), Tags::cc(),
              m_base->config()->fileName().data());
    }
  }
  else
  {
    LOG_ERR(CLEAR "%s" RED("Not able to load client config %s. Please make sure it exists! Using embedded config."),
            Tags::cc(),
            m_base->config()->fileName().data());
  }
}


void xmrig::CCClient::fetchUpdate()
{
  LOG_DEBUG("CCClient::fetchUpdate");

  auto config = m_base->config()->ccClient();
  std::stringstream hostHeader;
  hostHeader << config.host()
             << ":"
             << config.port();

  auto updatePath = std::string("/client/updates/") + Platform::updatePath().data();

  httplib::Headers headers;
  headers.emplace("Host", hostHeader.str().c_str());
  headers.emplace("Accept", "*//*");
  headers.emplace("User-Agent", Platform::userAgent().data());

  std::uint32_t lastProgress{0};

  LOG_WARN(CLEAR "%s" YELLOW("Downloading update. [http%s://%s:%d%s]"), Tags::cc(),
           config.useTLS() ? "s" : "", config.host(), config.port(), updatePath.c_str());

  auto cli = getClient();
  auto res = cli->Get(updatePath.c_str(), headers, [&lastProgress](uint64_t len, uint64_t total)
  {
    if (total > 0)
    {
      auto progress = static_cast<uint32_t>(static_cast<float>(len) / static_cast<float>(total) * 100);
      if (lastProgress != progress && (progress % 10 == 0))
      {
        lastProgress = progress;
        LOG_WARN(CLEAR "%s" WHITE("[%lu%%]"), Tags::cc(), progress);
      }
    }
    return true;
  });

  if (!res)
  {
    LOG_ERR(CLEAR "%s" RED("error:unable to performRequest GET [http%s://%s:%d%s]"), Tags::cc(),
            config.useTLS() ? "s" : "", config.host(), config.port(), updatePath.c_str());
  }
  else if(res->status == HTTP_OK)
  {
    std::string updateFile = std::string(xmrig::Process::exepath()) + UPDATE_EXTENSION;
    std::ofstream os(updateFile, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if (os.is_open())
    {
      LOG_WARN(CLEAR "%s" YELLOW("Download completed. Trigger update."), Tags::cc());
      os << res->body;
    }
    else
    {
      LOG_ERR(CLEAR "%s" RED("error: failed to write update file [%s]"), Tags::cc(), updateFile.c_str());
    }
  }
  else
  {
    LOG_ERR(CLEAR "%s" RED("error:\"%d\" GET [http%s://%s:%d%s]"), Tags::cc(), res->status, config.host(),
            config.useTLS() ? "s" : "", config.port(), updatePath.c_str());
  }
}


std::shared_ptr<httplib::ClientImpl> xmrig::CCClient::getClient()
{
  std::shared_ptr<httplib::ClientImpl> cli;

  auto config = m_base->config()->ccClient();
# ifdef XMRIG_FEATURE_TLS
  if (config.useTLS())
  {
    cli = std::make_shared<httplib::SSLClient>(config.host(), config.port());
    cli->enable_server_certificate_verification(false);
  }
  else
  {
# endif
    cli = std::make_shared<httplib::ClientImpl>(config.host(), config.port());
# ifdef XMRIG_FEATURE_TLS
  }
#   endif

  if (config.token() != nullptr)
  {
    cli->set_bearer_token_auth(config.token());
  }

  return cli;
}


std::shared_ptr<httplib::Response> xmrig::CCClient::performRequest(const std::string& requestUrl,
                                                                   const std::string& requestBuffer,
                                                                   const std::string& operation)
{
  auto config = m_base->config()->ccClient();
  std::stringstream hostHeader;
  hostHeader << config.host()
             << ":"
             << config.port();

  LOG_DEBUG("CCClient::performRequest %s [%s%s] send: '%.2048s'", operation.c_str(), hostHeader.str().c_str(),
            requestUrl.c_str(), requestBuffer.c_str());

  httplib::Request req;
  req.method = operation;
  req.path = requestUrl;
  req.set_header("Host", hostHeader.str());
  req.set_header("Accept", "*//*");
  req.set_header("User-Agent", Platform::userAgent());
  req.set_header("Accept", "application/json");
  req.set_header("Content-Type", "application/json");

  if (!requestBuffer.empty())
  {
    req.body = requestBuffer;
  }

  auto err = httplib::Error::Success;
  auto res = std::make_shared<httplib::Response>();
  auto cli = getClient();

  return cli->send(req, *res, err) ? res : nullptr;
}

void xmrig::CCClient::updateUptime()
{
  LOG_DEBUG("CCClient::updateUptime");
  m_clientStatus.setUptime(Chrono::currentMSecsSinceEpoch() - m_startTime);
}

void xmrig::CCClient::updateLog()
{
  LOG_DEBUG("CCClient::updateLog");
  m_clientStatus.setLog(RemoteLog::getRows());
}

void xmrig::CCClient::onConfigChanged(Config* config, Config* previousConfig)
{
  LOG_DEBUG("CCClient::onConfigChanged");
  if (config->ccClient() != previousConfig->ccClient())
  {
    config->ccClient().print();

    stop();

    if (config->ccClient().enabled() && config->ccClient().host() && config->ccClient().port() > 0)
    {
      start();
    }
  }
}

void xmrig::CCClient::onTimer(const xmrig::Timer* timer)
{
  LOG_DEBUG("CCClient::onTimer");
  if (!m_thread.joinable())
  {
    m_thread = std::thread(&CCClient::publishThread, this);
    m_thread.detach();
  }
}

void xmrig::CCClient::publishThread()
{
  LOG_DEBUG("CCClient::publishThread()");
  if (!m_configPublishedOnStart && m_base->config()->ccClient().uploadConfigOnStartup())
  {
    m_configPublishedOnStart = true;
    publishConfig();
  }

  updateUptime();
  updateLog();
  updateStatistics();

  publishClientStatusReport();
}
