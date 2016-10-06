/*
 *  goto_dialog.h
 *  PHD Guiding
 *
 *  Created by Arran Dengate
 *  Copyright (c) 2016 Arran Dengate
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

#ifndef GotoDialog_h_included
#define GotoDialog_h_included
#include <unordered_map>

class GotoDialog :
    public wxDialog
{
private:
    wxTextCtrl   *m_skyAltManualSet;
    wxTextCtrl   *m_skyAzManualSet;
    wxTextCtrl   *m_searchBar;
    wxStaticText *m_destinationRa;
    //wxStaticText *m_destinationRaHMS;
    wxStaticText *m_destinationDec;
    //wxStaticText *m_destinationDecDMS;
    wxStaticText *m_destinationAlt;
    wxStaticText *m_destinationAz;
    wxStaticText *m_destinationType;
    wxStaticText *m_skyPosText;
    wxStaticText *m_gpsLocText;
    wxStaticText *m_timeText; 
    std::unordered_map<string,string> m_catalog;

    wxTimer *m_timer; 

    wxButton *m_gotoButton;

    int prevExposureDuration;

    int StringWidth(const wxString& string);
    void OnGoto(wxCommandEvent& event);
    void OnClose(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);
    bool AstroSolveCurrentLocation(double &outRa, double &outDec, double &outAstroRotationAngle);
    bool EquatorialToHorizontal(double ra, double dec, double &outAlt, double &outAz, bool useStoredTimestamp);
    bool LookupEphemeral(string &ephemeral, double &outRa, double &outDec, double &outAlt, double &outAz);
    bool PlanetToHorizontal(string p);
    bool GetCatalogData(std::unordered_map<string,string>& catalog);
    void OnSearchTextChanged(wxCommandEvent&);
    void UpdateLocationText(void);
    void degreesToDMS(double input, double &degrees, double &arcMinutes, double &arcSeconds);
    void degreesToHMS(double ra, double &hours, double &minutes, double &seconds); 

public:
    GotoDialog(void);
    ~GotoDialog(void);

    void GetValues(Calibration *cal);
};

#endif