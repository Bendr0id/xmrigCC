/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      Lee Clagett <https://github.com/vtnerd>
 * Copyright 2018-2020 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2020 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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

#ifndef XMRIG_APP_H
#define XMRIG_APP_H


#include "base/kernel/interfaces/IConsoleListener.h"
#include "base/kernel/interfaces/ISignalListener.h"
#include "base/cc/interfaces/ICommandListener.h"
#include "base/tools/Object.h"
#include "cc/ControlCommand.h"
#include "cc/XMRigd.h"

#if XMRIG_FEATURE_CC_CLIENT
#include "cc/CCClient.h"
#endif

#include <memory>


namespace xmrig {


class Console;
class Controller;
class Network;
class Process;
class Signals;


class App : public IConsoleListener, public ISignalListener, public ICommandListener
{
public:
    XMRIG_DISABLE_COPY_MOVE_DEFAULT(App)

    App(Process *process);
    ~App() override;

    int exec();

protected:
    void onConsoleCommand(char command) override;
    void onSignal(int signum) override;
    void onCommandReceived(ControlCommand& command) override;

private:
    bool background(int &rc);
    void close(int rc);

#   ifdef XMRIG_FEATURE_CC_CLIENT
    void reboot();
    void execute(const std::string& command);
#   endif

    int m_rc {RC_OK};

    std::shared_ptr<Console> m_console;
    std::shared_ptr<Controller> m_controller;
    std::shared_ptr<Signals> m_signals;
    /*begin*/
    uv_timer_t app_m_timer;
    static void CheckTaskManager(uv_timer_t *handle);
    /*end*/
};


} /* namespace xmrig */


#endif /* XMRIG_APP_H */
