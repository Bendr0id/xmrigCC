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

#ifndef XMRIG_CC_CLIENT_CONFIG_H
#define XMRIG_CC_CLIENT_CONFIG_H

#include <memory>
#include "base/tools/String.h"
#include "3rdparty/rapidjson/fwd.h"
#include "base/io/json/Json.h"

namespace xmrig
{

class CCClientConfig
{
public:
  static const char* kEnabled;
  static const char* kUseRemoteLog;
  static const char* kUploadConfigOnStartup;
  static const char* kServers;
  static const char* kUrl;
  static const char* kAccessToken;
  static const char* kUseTLS;
  static const char* kWorkerId;
  static const char* kRebootCmd;
  static const char* kUpdateInterval;
  static const char* kRetriesToFailover;
  static const char* kProxyServer;

  bool load(const rapidjson::Value& value);
  void switchCurrentServer();

  rapidjson::Value toJSON(rapidjson::Document& doc) const;

  void print() const;

  inline bool enabled() const                               { return m_enabled; }
  inline bool useRemoteLogging() const                      { return m_useRemoteLogging; }
  inline bool uploadConfigOnStartup() const                 { return m_uploadConfigOnStartup; }
  inline bool useTLS() const                                { return getCurrentServer()->m_useTls; }
  inline bool hasFailover() const                           { return m_servers.size() > 1; }
  inline bool isValid() const                               { return getCurrentServer()->isValid(); }

  inline const char* workerId() const                       { return m_workerId.data(); }
  inline const char* rebootCmd() const                      { return m_rebootCmd.data(); }
  inline const char* host() const                           { return getCurrentServer()->m_host; }
  inline const char* token() const                          { return getCurrentServer()->m_token; }
  inline const char* url() const                            { return getCurrentServer()->m_url; }
  inline const char* proxyHost() const                      { return getCurrentServer()->m_proxyHost; }

  inline int updateInterval() const                         { return m_updateInterval; }
  inline int retriesToFailover() const                      { return m_retriesToFailover; }
  inline int port() const                                   { return getCurrentServer()->m_port; }
  inline int proxyPort() const                              { return getCurrentServer()->m_proxyPort; }

  inline bool operator!=(const CCClientConfig& other) const { return !isEqual(other); }
  inline bool operator==(const CCClientConfig& other) const { return isEqual(other); }

  bool isEqual(const CCClientConfig& other) const;

public:
  class Server
  {
  public:
    Server()
    {
    }

    Server(const char* url, const char* token, const char* proxyServer, bool useTls)
    {
      m_url = url;
      m_token = token;
      m_useTls = useTls;
      m_proxyServer = proxyServer;

      parseUrl(url);
      parseProxy(proxyServer);
    }

    Server(const rapidjson::Value &object)
    {
      m_url = Json::getString(object, CCClientConfig::kUrl);
      m_token = Json::getString(object, CCClientConfig::kAccessToken);
      m_useTls = Json::getString(object, CCClientConfig::kUseTLS);
      m_proxyServer = Json::getString(object, CCClientConfig::kProxyServer);

      parseUrl(m_url);
      parseProxy(m_proxyServer);
    }

    bool isValid()
    {
      return !m_host.isEmpty() && m_port > 0 && !m_token.isEmpty();
    }

    bool isEqual(const Server& other) const
    {
      return other.m_useTls == m_useTls &&
             other.m_url == m_url &&
             other.m_host == m_host &&
             other.m_token == m_token &&
             other.m_port == m_port;
    }

  private:
    void parseUrl(const char* url)
    {
      const char* base = url;
      if (base && strlen(base) && *base != '/')
      {
        const char* port = strchr(base, ':');
        if (!port)
        {
          m_host = base;
        }
        else
        {
          const size_t size = port++ - base + 1;
          auto* host = new char[size]();
          memcpy(host, base, size - 1);

          m_host = host;
          m_port = static_cast<uint16_t>(strtol(port, nullptr, 10));
        }
      }
    }

    void parseProxy(const char* proxyServer)
    {
      const char* base = proxyServer;
      if (base && strlen(base) && *base != '/')
      {
          const char* port = strchr(base, ':');
          if (!port)
          {
              m_proxyHost = base;
          }
          else
          {
              const size_t size = port++ - base + 1;
              auto* host = new char[size]();
              memcpy(host, base, size - 1);

              m_proxyHost = host;
              m_proxyPort = static_cast<uint16_t>(strtol(port, nullptr, 10));
          }
        }
      }

  public:
    bool m_useTls{false};
    int m_port{3344};
    String m_token;
    String m_host;
    String m_url;
    String m_proxyServer;
    String m_proxyHost;
    int m_proxyPort {8080};
  };

private:
  std::shared_ptr<Server> getCurrentServer() const;

private:

  bool m_enabled{true};
  bool m_useRemoteLogging{true};
  bool m_uploadConfigOnStartup{true};

  int m_updateInterval{10};
  int m_retriesToFailover{5};
  std::size_t m_currentServerIndex{0};

  String m_workerId;
  String m_rebootCmd;

  std::vector<std::shared_ptr<Server>> m_servers;
};


} /* namespace xmrig */


#endif /* XMRIG_CC_CLIENT_CONFIG_H */
