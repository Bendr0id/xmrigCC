/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2017 XMRig       <support@xmrig.com>
 * Copyright 2018      Sebastian Stolzenberg <https://github.com/sebastianstolzenberg>
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

#ifndef __CPU_IMPL_H__
#define __CPU_IMPL_H__


#include <cstdint>
#include <vector>

#include "Options.h"

class CpuImpl
{
public:
    static CpuImpl& instance();
    CpuImpl();
    void init();

    void optimizeParameters(size_t& threadsCount, size_t& hashFactor, Options::Algo algo,
                            int maxCpuUsage, bool safeMode);
    void setAffinity(int id, uint64_t mask);

    bool hasAES();
    bool isX64();
    const char *brand() { return m_brand; }
    int cores()         { return m_totalCores; }
    int l2()            { return m_l2_cache; }
    int l3()            { return m_l3_cache; }
    int sockets()       { return m_sockets; }
    int threads()       { return m_totalThreads; }
    size_t availableCache();

private:
    void initCommon();

    bool m_l2_exclusive;
    char m_brand[64];
    int m_flags;
    int m_l2_cache;
    int m_l3_cache;
    size_t m_sockets;
    size_t m_totalCores;
    size_t m_totalThreads;
};

#endif /* __CPU_IMPL_H__ */
