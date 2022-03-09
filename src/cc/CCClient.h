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

#ifndef __CC_CLIENT_H__
#define __CC_CLIENT_H__

#include <uv.h>
#include <chrono>
#include <ctime>
#include "3rdparty/cpp-httplib/httplib.h"

#include "ClientStatus.h"
#include "version.h"
#include "ControlCommand.h"

#include "base/kernel/interfaces/IBaseListener.h"
#include "base/kernel/interfaces/ITimerListener.h"

namespace xmrig
{

class Hashrate;
class NetworkState;
class Base;
class IClientStatusListener;
class ICommandListener;

class CCClient : public IBaseListener, public ITimerListener
{
public:
  CCClient(Base* base);
  ~CCClient();

  inline void addClientStatusListener(IClientStatusListener* listener)
  {
    m_ClientStatuslisteners.push_back(listener);
  }

  inline void addCommandListener(ICommandListener* listener)
  {
    m_Commandlisteners.push_back(listener);
  }

  void start();
  void stop();

protected:
  void onConfigChanged(Config* config, Config* previousConfig) override;

  void onTimer(const Timer* timer) override;

private:
  void publishThread();
  void publishClientStatusReport();

  void updateClientInfo();
  void updateUptime();
  void updateLog();
  void updateStatistics();

  void fetchConfig();
  void publishConfig();

  std::shared_ptr<httplib::Response> performRequest(const std::string& requestUrl,
                                                    const std::string& requestBuffer,
                                                    const std::string& operation);

  std::shared_ptr<httplib::ClientImpl> getClient();

private:
  Base* m_base;

  const uint64_t m_startTime;
  ClientStatus m_clientStatus;

  bool m_configPublishedOnStart;

  Timer* m_timer;
  std::thread m_thread;
  std::vector<ICommandListener*> m_Commandlisteners;
  std::vector<IClientStatusListener*> m_ClientStatuslisteners;

  void fetchUpdate();

};
}

#endif /* __CC_CLIENT_H__ */
