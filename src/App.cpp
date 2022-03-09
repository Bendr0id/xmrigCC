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


#include <cstdlib>
#include <uv.h>


#include "App.h"
#include "backend/cpu/Cpu.h"
#include "base/io/Console.h"
#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"
#include "base/io/Signals.h"
#include "base/kernel/Platform.h"
#include "core/config/Config.h"
#include "core/Controller.h"
#include "cc/ControlCommand.h"
#include "cc/XMRigd.h"
#include "Summary.h"
#include "version.h"


xmrig::App::App(Process *process)
{
    m_controller = std::make_shared<Controller>(process);
}


xmrig::App::~App()
{
    Cpu::release();
}


int xmrig::App::exec()
{
    if (!m_controller->isReady()) {
        LOG_EMERG("no valid configuration found.");

        return 2;
    }

#   ifdef XMRIG_FEATURE_CC_CLIENT
    if (!m_controller->config()->isDaemonized()) {
        LOG_EMERG(APP_ID " is compiled with CC support, please start the daemon instead.\n");
        return 2;
    }
#   endif

    m_signals = std::make_shared<Signals>(this);

    int rc = 0;
    if (background(rc)) {
        return rc;
    }

    rc = m_controller->init();
    if (rc != 0) {
        return rc;
    }

    if (!m_controller->isBackground()) {
        m_console = std::make_shared<Console>(this);
    }

    Summary::print(m_controller.get());

    if (m_controller->config()->isDryRun()) {
        LOG_NOTICE("%s " WHITE_BOLD("OK"), Tags::config());

        return 0;
    }

    m_controller->start();

#   if XMRIG_FEATURE_CC_CLIENT
    m_controller->ccClient()->addCommandListener(this);
#   endif

    rc = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());

    return m_rc > 0 ? m_rc : rc;
}


void xmrig::App::onConsoleCommand(char command)
{
    if (command == 3) {
        LOG_WARN("%s " YELLOW("Ctrl+C received, exiting"), Tags::signal());
        close(RC_OK);
    }
    else {
        m_controller->execCommand(command);
    }
}


void xmrig::App::onSignal(int signum)
{
    switch (signum)
    {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        return close(RC_OK);

    default:
        break;
    }
}


void xmrig::App::onCommandReceived(ControlCommand& command)
{
#   ifdef XMRIG_FEATURE_CC_CLIENT
    switch (command.getCommand()) {
        case ControlCommand::START:
            m_controller->execCommand('r');
            break;
        case ControlCommand::STOP:
            m_controller->execCommand('p');
            break;
        case ControlCommand::UPDATE:
        case ControlCommand::RESTART:
            close(RC_RESTART);
            break;
        case ControlCommand::SHUTDOWN:
            close(RC_OK);
            break;
        case ControlCommand::REBOOT:
            reboot();
            break;
        case ControlCommand::EXECUTE:
            execute(command.getPayload());
            break;
        case ControlCommand::UPDATE_CONFIG:;
        case ControlCommand::PUBLISH_CONFIG:;
            break;
    }
#   endif
}


void xmrig::App::close(int rc)
{
    m_rc = rc;

    m_controller->stop();
    m_controller.reset();

    m_signals.reset();
    m_console.reset();

    Log::destroy();

    uv_stop(uv_default_loop());
}


#   ifdef XMRIG_FEATURE_CC_CLIENT
void xmrig::App::reboot()
{
#   ifdef XMRIG_FEATURE_CC_CLIENT_SHELL_EXECUTE
  auto rebootCmd = m_controller->config()->ccClient().rebootCmd();
  if (rebootCmd) {
    system(rebootCmd);
    close(RC_OK);
  }
#   else
  LOG_EMERG("Shell execute disabled. Skipping REBOOT.");
#   endif
}

void xmrig::App::execute(const std::string& command)
{
#   ifdef XMRIG_FEATURE_CC_CLIENT_SHELL_EXECUTE
  if (!command.empty()) {
    system(command.c_str());
  }
#   else
  LOG_EMERG("Shell execute disabled. Skipping %s", command.c_str());
#   endif
}
#   endif
