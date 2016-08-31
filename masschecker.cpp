/*
 *  masschecker.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
 *  All rights reserved.
 *
 *  Greatly expanded by Bret McKee
 *  Copyright (c) 2012 Bret McKee
 *  All rights reserved.
 *
 *  This source code is distributed under the following "BSD" license
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *    Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *    Neither the name of Craig Stark, Stark Labs nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "phd.h"
#include <wx/dir.h>
#include <algorithm>
#include <cmath>
#include <string>

MassChecker::MassChecker() {
    m_tmp = 0;
    m_tmpSize = 0;
    m_lastExposure = 0;
    SetTimeWindow(DefaultTimeWindowMs);
}

MassChecker::~MassChecker()
{
    // delete[] m_tmp;
}

void MassChecker::SetTimeWindow(unsigned int milliseconds)
{
    // an abrupt change in mass will affect the median after approx m_timeWindow/2
    m_timeWindow = milliseconds * 2;
}

void MassChecker::SetExposure(int exposure)
{
    if (exposure != m_lastExposure)
    {
        m_lastExposure = exposure;
        Reset();
    }
}

void MassChecker::AppendData(double mass)
{
    Debug.Write(wxString::Format("Appending data\n"));
    wxLongLong_t now = ::wxGetUTCTimeMillis().GetValue();
    wxLongLong_t oldest = now - m_timeWindow;

    while (m_data.size() > 0 && m_data.front().time < oldest)
        m_data.pop_front();

    Entry entry;
    entry.time = now;
    entry.mass = mass;
    m_data.push_back(entry);

}

bool MassChecker::CheckMass(double mass, double threshold, double limits[3])
{
    if (m_data.size() < 3)
        return false;
    
    std::vector<Entry> entryList;
    for (Entry e : m_data) {
        entryList.push_back(e);
    }
    size_t n = entryList.size() / 2;
    nth_element(entryList.begin(), entryList.begin()+n, entryList.end());
    double median = entryList[n].mass;

    limits[0] = median * (1. - threshold);
    limits[1] = median;
    limits[2] = median * (1. + threshold);

    return mass < limits[0] || mass > limits[2];
    
}

void MassChecker::Reset(void)
{
    m_data.clear();
}