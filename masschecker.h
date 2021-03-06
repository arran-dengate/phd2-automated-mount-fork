/*
 *  masschecker.h
 *  PHD Guiding
 *
 *  Created by Bret McKee
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
 *    Neither the name of Bret McKee, Dad Dog Development,
 *     Craig Stark, Stark Labs nor the names of its
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

#ifndef MASSCHECKER_H_INCLUDED
#define MASSCHECKER_H_INCLUDED

class MassChecker { 
    
    public: 
    enum { DefaultTimeWindowMs = 15000 };

    struct Entry
    {
        wxLongLong_t time;
        double mass;

        // So can be sorted by mass
        bool operator<( const Entry& other ) const { 
            return mass < other.mass; 
        }
    };

    std::deque<Entry> m_data;
    unsigned long m_timeWindow;
    double *m_tmp;
    size_t m_tmpSize;
    int m_lastExposure;
    int validationChances = 3; // If several invalid readings in close succession, star is discarded.
    bool currentlyValid = true;

    MassChecker();
    ~MassChecker();
    void SetTimeWindow(unsigned int milliseconds);
    void SetExposure(int exposure);
    void AppendData(double mass);
    bool CheckMass(double mass, double threshold, double limits[3]);
    void Reset(void);
}; 

#endif /* MASSCHECKER_H_INCLUDED */