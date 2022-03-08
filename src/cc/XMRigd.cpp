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

#include <cstdlib>
#include <string>
#include <fstream>
#include <thread>

#include "XMRigd.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN  /* avoid including junk */
#include <windows.h>
#include <signal.h>
#else

#include <sys/wait.h>
#include <errno.h>

#endif

#ifndef MINER_EXECUTABLE_NAME
#define MINER_EXECUTABLE_NAME xmrigMiner
#endif
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)

bool fileFound(const std::string& fullMinerBinaryPath)
{
  std::ifstream file(fullMinerBinaryPath.c_str());
  return file.good();
}

int main(int argc, char** argv)
{

  std::string ownPath(argv[0]);

#if defined(_WIN32) || defined(WIN32)
  int pos = ownPath.rfind('\\');
  std::string minerBinaryName(VALUE(MINER_EXECUTABLE_NAME) ".exe");
#else
  int pos = ownPath.rfind('/');
  std::string minerBinaryName(VALUE(MINER_EXECUTABLE_NAME));
#endif

  std::string fullMinerBinaryPath = ownPath.substr(0, pos + 1) + minerBinaryName;

#if defined(_WIN32) || defined(WIN32)
  fullMinerBinaryPath = "\"" + fullMinerBinaryPath + "\"";
#endif

  std::string params = " --daemonized";
  for (int i = 1; i < argc; i++)
  {
    params += " ";
    params += argv[i];
  }

  int status = 0;

  do
  {
    // apply update if we have one
    if (fileFound(fullMinerBinaryPath + UPDATE_EXTENSION))
    {
      if (!std::rename(fullMinerBinaryPath.c_str(), (fullMinerBinaryPath + BACKUP_EXTENSION).c_str()))
      {
        if (std::rename((fullMinerBinaryPath + UPDATE_EXTENSION).c_str(), fullMinerBinaryPath.c_str()))
        {
          // try to rollback
          std::rename((fullMinerBinaryPath + BACKUP_EXTENSION).c_str(), fullMinerBinaryPath.c_str());
        }
      }
    }

    status = system((fullMinerBinaryPath + params).c_str());
#if defined(_WIN32) || defined(WIN32)
    } while (status != EINVAL && status != SIGHUP && status != SIGINT && status != 0);

    if (status == EINVAL)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }
#else

  } while (WEXITSTATUS(status) != EINVAL && WEXITSTATUS(status) != SIGHUP && WEXITSTATUS(status) != SIGINT &&
           WEXITSTATUS(status) != 0);
#endif
}
