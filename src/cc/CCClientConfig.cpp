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

#include <string>

#include "base/io/log/Log.h"
#include "base/io/json/Json.h"
#include "cc/CCClientConfig.h"
#include "3rdparty/rapidjson/document.h"


namespace xmrig
{

const char* CCClientConfig::kEnabled = "enabled";
const char* CCClientConfig::kUseRemoteLog = "use-remote-logging";
const char* CCClientConfig::kUploadConfigOnStartup = "upload-config-on-start";

const char* CCClientConfig::kServers = "servers";
const char* CCClientConfig::kUrl = "url";
const char* CCClientConfig::kAccessToken = "access-token";
const char* CCClientConfig::kUseTLS = "use-tls";
const char* CCClientConfig::kProxyServer = "http-proxy";

const char* CCClientConfig::kWorkerId = "worker-id";
const char* CCClientConfig::kRebootCmd = "reboot-cmd";
const char* CCClientConfig::kUpdateInterval = "update-interval-s";
const char* CCClientConfig::kRetriesToFailover = "retries-to-failover";

}


rapidjson::Value xmrig::CCClientConfig::toJSON(rapidjson::Document& doc) const
{
  using namespace rapidjson;
  auto& allocator = doc.GetAllocator();

  Value obj(kObjectType);

  obj.AddMember(StringRef(kEnabled), m_enabled, allocator);
  obj.AddMember(StringRef(kUseRemoteLog), m_useRemoteLogging, allocator);
  obj.AddMember(StringRef(kUploadConfigOnStartup), m_uploadConfigOnStartup, allocator);

  Value serverArray(kArrayType);
  for (const auto& server : m_servers)
  {
    Value serverObj(kObjectType);
    serverObj.AddMember(StringRef(kUrl), server->m_url.toJSON(), allocator);
    serverObj.AddMember(StringRef(kAccessToken), server->m_token.toJSON(), allocator);
    serverObj.AddMember(StringRef(kProxyServer), server->m_proxyServer.toJSON(), allocator);
    serverObj.AddMember(StringRef(kUseTLS), server->m_useTls, allocator);

    serverArray.PushBack(serverObj, allocator);
  }

  obj.AddMember(StringRef(kServers), serverArray, allocator);
  obj.AddMember(StringRef(kWorkerId), m_workerId.toJSON(), allocator);
  obj.AddMember(StringRef(kRebootCmd), m_rebootCmd.toJSON(), allocator);

  obj.AddMember(StringRef(kUpdateInterval), m_updateInterval, allocator);
  obj.AddMember(StringRef(kRetriesToFailover), m_retriesToFailover, allocator);

  return obj;
}


bool xmrig::CCClientConfig::load(const rapidjson::Value& value)
{
  if (value.IsObject())
  {
    m_enabled = Json::getBool(value, kEnabled, m_enabled);
    m_useRemoteLogging = Json::getBool(value, kUseRemoteLog, m_useRemoteLogging);
    m_uploadConfigOnStartup = Json::getBool(value, kUploadConfigOnStartup, m_uploadConfigOnStartup);

    auto& servers = Json::getArray(value, kServers);
    if (servers.IsArray())
    {
      for (const rapidjson::Value &entry : servers.GetArray())
      {
        String url = Json::getString(entry, kUrl, "");
        String token = Json::getString(entry, kAccessToken, "");
        String proxyServer = Json::getString(entry, kProxyServer, "");
        bool useTls = Json::getBool(entry, kUseTLS, false);

        auto server = std::make_shared<Server>(url, token, proxyServer, useTls);
        if (server->isValid())
        {
          m_servers.emplace_back(server);
        }
      }
    }

    if (m_servers.empty())
    {
      String url = Json::getString(value, kUrl, "");
      String token = Json::getString(value, kAccessToken, "");
      String proxyServer = Json::getString(value, kProxyServer, "");
      bool useTls = Json::getBool(value, kUseTLS, false);

      auto server = std::make_shared<Server>(url, token, proxyServer, useTls);
      if (server->isValid())
      {
        m_servers.emplace_back(server);
      }
    }

    m_workerId = Json::getString(value, kWorkerId, m_workerId);
    m_rebootCmd = Json::getString(value, kRebootCmd, m_rebootCmd);

    m_updateInterval = Json::getInt(value, kUpdateInterval, m_updateInterval);
    m_retriesToFailover = Json::getInt(value, kRetriesToFailover, m_retriesToFailover);

    return !m_servers.empty();
  }

  return false;
}

void xmrig::CCClientConfig::print() const
{
  std::string ccServer;
  if (enabled() && getCurrentServer()->isValid())
  {
    ccServer = CSI "1;" + std::to_string(enabled() ? (useTLS() ? 32 : 36) : 31) + "m" + url() + CLEAR;
  }
  else
  {
    ccServer = RED_BOLD("disabled");
  }

  Log::print(GREEN_BOLD(" * ") WHITE_BOLD("%-13s") "%s",
             "CC Server",
             ccServer.c_str());
}

bool xmrig::CCClientConfig::isEqual(const CCClientConfig& other) const
{
  bool isEqual = other.m_enabled == m_enabled &&
                 other.m_useRemoteLogging == m_useRemoteLogging &&
                 other.m_uploadConfigOnStartup == m_uploadConfigOnStartup &&
                 other.m_workerId == m_workerId &&
                 other.m_rebootCmd == m_rebootCmd &&
                 other.m_updateInterval == m_updateInterval &&
                 other.m_retriesToFailover == m_retriesToFailover &&
                 other.m_servers.size() == m_servers.size();

  if (isEqual)
  {
    for (std::size_t i=0; i < other.m_servers.size(); ++i)
    {
      isEqual &= other.m_servers[i]->isEqual(*m_servers[i]);
    }
  }

  return isEqual;
}

std::shared_ptr<xmrig::CCClientConfig::Server> xmrig::CCClientConfig::getCurrentServer() const
{
  if (m_currentServerIndex < m_servers.size())
  {
    return m_servers.at(m_currentServerIndex);
  }
  else
  {
    return std::make_shared<Server>();
  }
}

void xmrig::CCClientConfig::switchCurrentServer()
{
  if (++m_currentServerIndex >= m_servers.size())
  {
    m_currentServerIndex = 0;
  }

  print();
}