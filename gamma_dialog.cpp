/*
 *  manualcal_dialog.cpp
 *  PHD Guiding
 *
 *  Created by Sylvain Girard
 *  Copyright (c) 2013 Sylvain Girard
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
    : wxDialog(pFrame, wxID_ANY, _("Gamma"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    wxBoxSizer *mainBox = new wxBoxSizer(wxVERTICAL);
    int currentGamma, gammaMin, gammaMax, gammaDefault;
    pFrame->GetGammaSettings(currentGamma, gammaMin, gammaMax, gammaDefault); 

    wxStaticText *introText = new wxStaticText(this, -1, wxT("Drag the bar below to change the brightness of the guide scope image."),
                                                wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL, wxT("Explanation"));

    mainBox->Add(introText, 1, wxALIGN_CENTER | wxALL, 5);
    gammaSlider = new wxSlider(this, wxID_ANY, currentGamma, gammaMin, gammaMax, wxDefaultPosition, wxSize(400, -1), 
                               wxSL_HORIZONTAL, wxDefaultValidator, wxT("Gamma_Slider"));
    gammaSlider->SetBackgroundColour(wxColor(60, 60, 60));         // Slightly darker than toolbar background
    gammaSlider->SetToolTip(_("Screen gamma (brightness)"));
    gammaSlider->Bind(wxEVT_SLIDER, &GammaDialog::OnGammaSlider, this);
    mainBox->Add(gammaSlider, 1, wxEXPAND | wxALL, 5);
    wxSizer * okCancelSizer = CreateButtonSizer(wxOK);
    mainBox->Add(okCancelSizer, 1, wxALIGN_CENTER | wxALL, 5);
    SetSizer(mainBox);
    Fit();
}

void GammaDialog::OnGammaSlider(wxCommandEvent& WXUNUSED(event))
{
    pFrame->SetGamma(gammaSlider->GetValue());
    //int val = Gamma_Slider->GetValue();
    //pConfig->Profile.SetInt("/Gamma", val);
    //Stretch_gamma = (double) val / 100.0;
    //pGuider->UpdateImageDisplay();
}

GammaDialog::~GammaDialog(void)
{
}