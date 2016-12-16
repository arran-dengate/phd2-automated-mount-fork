/*
 *  manualcal_dialog.cpp
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
#include "gamma_dialog.h"

GammaDialog::GammaDialog(void)
    : wxDialog(pFrame, wxID_ANY, _("Gamma"), wxDefaultPosition, wxDefaultSize, 0)
{
    wxBoxSizer *mainBox = new wxBoxSizer(wxVERTICAL);
    wxStaticText *introText = new wxStaticText(this, -1, wxT("Drag the bar below to change the brightness of the guide scope image."),
                                                wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL, wxT("Explanation"));
    mainBox->Add(introText, 1, wxALIGN_CENTER | wxALL, 5);
    gammaSlider = new wxSlider(this, wxID_ANY, 0, 0, 255, wxDefaultPosition, wxSize(500, -1), 
                               wxSL_HORIZONTAL, wxDefaultValidator, wxT("Gamma_Slider"));
    gammaSlider->SetToolTip(_("Screen gamma (brightness)"));
    gammaSlider->Bind(wxEVT_SLIDER, &GammaDialog::OnGammaSlider, this);
    gammaSlider->Bind(wxEVT_KILL_FOCUS, &GammaDialog::OnKillFocus, this);
    mainBox->Add(gammaSlider, 1, wxEXPAND | wxALL, 5);
    SetSizer(mainBox);
    Fit();
    gammaSlider->SetFocus();
}

void GammaDialog::UpdateValues() {
    int currentGamma, gammaMin, gammaMax, gammaDefault;
    pFrame->GetGammaSettings(currentGamma, gammaMin, gammaMax, gammaDefault); 
    currentGamma = pConfig->Profile.GetInt("/Gamma", 20); // GetGammaSettings doesn't seem to accurately get this, so pull it from the profile.
    gammaSlider->SetMin(gammaMin);
    gammaSlider->SetMax(gammaMax);
    gammaSlider->SetValue(currentGamma);
}

void GammaDialog::OnGammaSlider(wxCommandEvent& WXUNUSED(event))
{
    int gammaValue = gammaSlider->GetValue();
    pFrame->SetGamma(gammaValue);
    //int val = Gamma_Slider->GetValue();
    pConfig->Profile.SetInt("/Gamma", gammaValue);
    //Stretch_gamma = (double) val / 100.0;
    pFrame->pGuider->UpdateImageDisplay();
}

void GammaDialog::OnKillFocus(wxFocusEvent& evt) {
    Hide();
}

GammaDialog::~GammaDialog(void)
{
}
