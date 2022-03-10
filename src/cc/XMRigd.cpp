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
#include <uv.h>

#include "XMRigd.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN  /* avoid including junk */
#include <windows.h>
#include <signal.h>
#else
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#ifndef MINER_EXECUTABLE_NAME
#define MINER_EXECUTABLE_NAME xmrigMiner
#endif
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)

bool fileFound(const std::string& filePath)
{
  std::ifstream file(filePath.c_str());
  return file.good();
}

int main(int argc, char** argv)
{
  std::string ownPath(argv[0]);
  std::string params = " --daemonized";
  for (int i = 1; i < argc; i++)
  {
    params += " ";
    params += argv[i];
  }

#if defined(_WIN32) || defined(WIN32)
  auto pos = ownPath.rfind('\\');
  std::string minerBinaryName(VALUE(MINER_EXECUTABLE_NAME) ".exe");
#else
  auto pos = ownPath.rfind('/');
  std::string minerBinaryName(VALUE(MINER_EXECUTABLE_NAME));
#endif

  auto fullMinerBinaryPath = ownPath.substr(0, pos + 1) + minerBinaryName;
  auto status = EXIT_SUCCESS;

  do
  {
    status = EXIT_SUCCESS;

    // apply update if we have one
    if (fileFound(fullMinerBinaryPath + UPDATE_EXTENSION))
    {
      // remove old backup file
      if (fileFound(fullMinerBinaryPath + BACKUP_EXTENSION))
      {
        status = std::remove((fullMinerBinaryPath + BACKUP_EXTENSION).c_str());
      }

      if (status == EXIT_SUCCESS)
      {
        // rename original to backup
        status = std::rename(fullMinerBinaryPath.c_str(), (fullMinerBinaryPath + BACKUP_EXTENSION).c_str());
        if (status == EXIT_SUCCESS)
        {
          // rename update to original
          status = std::rename((fullMinerBinaryPath + UPDATE_EXTENSION).c_str(), fullMinerBinaryPath.c_str());

#if !defined(_WIN32) && !defined(WIN32)
          if (status == EXIT_SUCCESS)
          {
            // on non-windows system make file executable
            status = chmod(fullMinerBinaryPath.c_str(), S_IRWXU);
          }
#endif
        }

        if (status != EXIT_SUCCESS)
        {
          // try to rollback
          std::rename((fullMinerBinaryPath + BACKUP_EXTENSION).c_str(), fullMinerBinaryPath.c_str());
        }
      }
      else
      {
        // update failed try to remove the update
        std::remove((fullMinerBinaryPath + UPDATE_EXTENSION).c_str());
      }
    }

    // execute miner and wait for result
    status = system(("\"" +fullMinerBinaryPath + "\""  + params).c_str());
#if defined(_WIN32) || defined(WIN32)
    } while (status != EINVAL && status != SIGHUP && status != SIGINT && status != EXIT_SUCCESS);

  if (status == EINVAL)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  }
#else
  } while (WEXITSTATUS(status) != EINVAL && WEXITSTATUS(status) != SIGHUP && WEXITSTATUS(status) != SIGINT &&
           WEXITSTATUS(status) != EXIT_SUCCESS);
#endif
}
