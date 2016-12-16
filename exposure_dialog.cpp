/*
 *  exposure_dialog.cpp
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

#include "phd.h"
#include "mount.h"
#include "exposure_dialog.h"
#include <string>

ExposureDialog::ExposureDialog(void)
    : wxDialog(pFrame, wxID_ANY, _("Gamma"), wxDefaultPosition, wxDefaultSize, 0)
{
    wxSizer * outerBox = new wxBoxSizer(wxVERTICAL);
    mainBox = new wxBoxSizer(wxHORIZONTAL);
    
    wxStaticText *introText = new wxStaticText(this, -1, wxT("Exposure time"),
                                               wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL, wxT("Explanation"));
    outerBox->Add(introText, 1, wxALIGN_CENTER | wxALL, 5);
    outerBox->Add(mainBox);

    buttonSizer = new wxGridSizer(3, 7, 5, 5); // Rows, columns, 
    
    mainBox->Add(buttonSizer, 1, wxALIGN_TOP | wxALL, 5);

    SetSizer(outerBox);
    Fit();

}

void ExposureDialog::UpdateValues(wxArrayString durations) {

    buttonSizer->SetCols(3);
    buttonSizer->SetRows(7);

    while (numButtons > 0) {
        buttonSizer->Hide(numButtons - 1);
        buttonSizer->Remove(numButtons - 1);
        numButtons --;
    }

    Fit();

    // This always returned a blank string. I am not at all sure why.
    int currentDuration = pFrame->RequestedExposureDuration();

    bool first;
    for (wxString s : durations) {
        wxRadioButton * button = new wxRadioButton(this, -1, s, wxDefaultPosition, wxDefaultSize, ( first ? wxRB_GROUP : 0), 
                                                   wxDefaultValidator, s);
        if (s != _("Auto")){
            if (currentDuration == int(stod(s.ToStdString())*1000)) {
                button->SetValue(true);
                button->SetFocus();
            }    
        }
        
        button->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &ExposureDialog::OnExposureClicked, this);
        button->Bind(wxEVT_KILL_FOCUS, &ExposureDialog::OnKillFocus, this);
        buttonSizer->Add(button);
        numButtons ++;
        first = false;
    }
    Fit();
    Refresh();
}

void ExposureDialog::OnExposureClicked(wxCommandEvent& event)
{
    pFrame->UpdateExposureDuration(wxDynamicCast(event.GetEventObject(), wxRadioButton)->GetLabel());
    wxCommandEvent blank = wxCommandEvent(wxEVT_NULL, 0);
    pFrame->OnExposureDurationSelected(blank);

}

void ExposureDialog::OnKillFocus(wxFocusEvent& evt) {
    wxPoint clickPos = wxGetMousePosition();
    wxPoint windowPos = this->GetPosition();
    int width, height;
    this->GetSize(&width, &height);
    if ( (clickPos.x < windowPos.x || clickPos.x > windowPos.x + width) or (clickPos.y < windowPos.y || clickPos.y > windowPos.y + height) ) {
        Hide();    
    }
    Debug.AddLine(wxString::Format("Exposure: onKillFocus with clickPos %d %d windowPos %d %d width %d height %d", clickPos.x, clickPos.y, windowPos.x, windowPos.y, width, height));
}

ExposureDialog::~ExposureDialog(void)
{
}
