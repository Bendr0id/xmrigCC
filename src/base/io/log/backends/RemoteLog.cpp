/* XMRigCC
 * Copyright 2018-     BenDr0id    <https://github.com/BenDr0id>, <ben@graef.in>
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


#include <stdio.h>
#include <string.h>
#include <sstream>

#include "base/tools/Handle.h"
#include "base/io/log/backends/RemoteLog.h"
#include "base/io/log/Log.h"

xmrig::RemoteLog* xmrig::RemoteLog::m_self = nullptr;

xmrig::RemoteLog::RemoteLog(size_t maxRows)
    : m_maxRows(maxRows)
{
    m_self = this;
}

xmrig::RemoteLog::~RemoteLog()
{
    m_self = nullptr;
}

void xmrig::RemoteLog::print(int, const char *line, size_t, size_t size, bool colors)
{
    if (colors) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_rows.size() >= m_maxRows) {
        m_rows.pop_front();
    }

    m_rows.emplace_back(line, size);
}

std::string xmrig::RemoteLog::getRows()
{
    std::stringstream data;

    if (m_self) {
        std::lock_guard<std::mutex> lock(m_self->m_mutex);

        for (auto& m_row : m_self->m_rows) {
            data << m_row.c_str();
        }
        m_self->m_rows.clear();
    }

    return data.str();
}

