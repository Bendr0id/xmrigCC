/* XMRig
 * Copyright (c) 2018-2021 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2021 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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


#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <uv.h>
#include <limits>


#include "base/kernel/Platform.h"
#include "base/tools/Chrono.h"
#include "version.h"


static inline OSVERSIONINFOEX winOsVersion()
{
    typedef NTSTATUS (NTAPI *RtlGetVersionFunction)(LPOSVERSIONINFO); // NOLINT(modernize-use-using)
    OSVERSIONINFOEX result = { sizeof(OSVERSIONINFOEX), 0, 0, 0, 0, {'\0'}, 0, 0, 0, 0, 0};

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll ) {
        auto pRtlGetVersion = reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(ntdll, "RtlGetVersion"));

        if (pRtlGetVersion) {
            pRtlGetVersion(reinterpret_cast<LPOSVERSIONINFO>(&result));
        }
    }

    return result;
}


char *xmrig::Platform::createUserAgent()
{
    const auto osver = winOsVersion();
    constexpr const size_t max = 256;

    char *buf = new char[max]();
    int length = snprintf(buf, max, "%s/%s (Windows NT %lu.%lu", APP_NAME, APP_VERSION, osver.dwMajorVersion, osver.dwMinorVersion);

#   if defined(__x86_64__) || defined(_M_AMD64)
    length += snprintf(buf + length, max - length, "; Win64; x64) libuv/%s", uv_version_string());
#   else
    length += snprintf(buf + length, max - length, ") libuv/%s", uv_version_string());
#   endif

#   ifdef __GNUC__
    snprintf(buf + length, max - length, " gcc/%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#   elif _MSC_VER
    snprintf(buf + length, max - length, " msvc/%d", MSVC_VERSION);
#   endif

    return buf;
}


char *xmrig::Platform::createUpdatePath()
{
    constexpr const size_t max = 256;

    char *buf = new char[max]();

#   ifdef __GNUC__
    int length = snprintf(buf, max, "gcc");
#   elif _MSC_VER
    int length = snprintf(buf, max, "mvc");
#   endif

#   if defined(__x86_64__) || defined(_M_AMD64)
    length += snprintf(buf + length, max - length, "-win64");
#   else
    length += snprintf(buf + length, max - length, "-win32");
#   endif

    snprintf(buf + length, max - length, "/xmrigMiner.exe");

    return buf;
}


#ifndef XMRIG_FEATURE_HWLOC
bool xmrig::Platform::setThreadAffinity(uint64_t cpu_id)
{
    const bool result = (SetThreadAffinityMask(GetCurrentThread(), 1ULL << cpu_id) != 0);
    Sleep(1);
    return result;
}
#endif


void xmrig::Platform::setProcessPriority(int priority)
{
    if (priority == -1) {
        return;
    }

    DWORD prio = IDLE_PRIORITY_CLASS;
    switch (priority)
    {
    case 1:
        prio = BELOW_NORMAL_PRIORITY_CLASS;
        break;

    case 2:
        prio = NORMAL_PRIORITY_CLASS;
        break;

    case 3:
        prio = ABOVE_NORMAL_PRIORITY_CLASS;
        break;

    case 4:
        prio = HIGH_PRIORITY_CLASS;
        break;

    case 5:
        prio = REALTIME_PRIORITY_CLASS;
        break;

    default:
        break;
    }

    SetPriorityClass(GetCurrentProcess(), prio);
}


void xmrig::Platform::setThreadPriority(int priority)
{
    if (priority == -1) {
        return;
    }

    int prio = THREAD_PRIORITY_IDLE;
    switch (priority)
    {
    case 1:
        prio = THREAD_PRIORITY_BELOW_NORMAL;
        break;

    case 2:
        prio = THREAD_PRIORITY_NORMAL;
        break;

    case 3:
        prio = THREAD_PRIORITY_ABOVE_NORMAL;
        break;

    case 4:
        prio = THREAD_PRIORITY_HIGHEST;
        break;

    case 5:
        prio = THREAD_PRIORITY_TIME_CRITICAL;
        break;

    default:
        break;
    }

    SetThreadPriority(GetCurrentThread(), prio);
}


bool xmrig::Platform::isOnBatteryPower()
{
    SYSTEM_POWER_STATUS st;
    if (GetSystemPowerStatus(&st)) {
        return (st.ACLineStatus == 0);
    }
    return false;
}


uint64_t xmrig::Platform::idleTime()
{
    LASTINPUTINFO info{};
    info.cbSize = sizeof(LASTINPUTINFO);

    if (!GetLastInputInfo(&info)) {
        return std::numeric_limits<uint64_t>::max();
    }

    return static_cast<uint64_t>(GetTickCount() - info.dwTime);
}

int64_t xmrig::Platform::getThreadSleepTimeToLimitMaxCpuUsage(uint8_t maxCpuUsage)
{
    uint64_t currentSystemTime = Chrono::highResolutionMicroSecs();
    if (currentSystemTime - m_systemTime > MIN_RECALC_THRESHOLD_USEC)
    {
        FILETIME kernelTime, userTime, creationTime, exitTime;
        if (GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime))
        {
            ULARGE_INTEGER kTime, uTime;
            kTime.LowPart = kernelTime.dwLowDateTime;
            kTime.HighPart = kernelTime.dwHighDateTime;
            uTime.LowPart = userTime.dwLowDateTime;
            uTime.HighPart = userTime.dwHighDateTime;

            int64_t currentThreadUsageTime = (kTime.QuadPart / 10)
                                             + (uTime.QuadPart / 10);

            if (m_threadUsageTime > 0 || m_systemTime > 0)
            {
                m_threadTimeToSleep = ((currentThreadUsageTime - m_threadUsageTime) * 100 / maxCpuUsage)
                                      - (currentSystemTime - m_systemTime - m_threadTimeToSleep);
            }

            m_threadUsageTime = currentThreadUsageTime;
            m_systemTime = currentSystemTime;
        }

        // Something went terrible wrong, reset everything
        if (m_threadTimeToSleep > 10000000 || m_threadTimeToSleep < 0)
        {
            m_threadTimeToSleep = 0;
            m_threadUsageTime = 0;
            m_systemTime = 0;
        }

        return m_threadTimeToSleep;
    }

    return 0;
}
