/*
 *  goto_dialog.cpp
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
#include "goto/astrometry.h"
#include "goto/csv.h"
#include "goto/units.h"
#include "goto/conversion.h"
#include "goto_dialog.h"
#include <wx/srchctrl.h>
#include <wx/listctrl.h>

GotoDialog::GotoDialog(void)
    : wxDialog(pFrame, wxID_ANY, _("Go to..."), wxDefaultPosition, wxSize(800, 400), wxCAPTION | wxCLOSE_BOX)
{   
    wxBoxSizer *containBox = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *mainBox = new wxBoxSizer(wxHORIZONTAL);
    wxStaticBoxSizer *leftBox = new wxStaticBoxSizer(wxVERTICAL, this, "Search");
    wxStaticBoxSizer *rightBox = new wxStaticBoxSizer(wxVERTICAL, this, "Description");
    mainBox->Add(leftBox, 1, wxEXPAND);
    mainBox->Add(rightBox, 1, wxEXPAND);
    containBox->Add(mainBox, 1, wxEXPAND);
    int borderSize = 5;
    
    wxButton *gotoButton = new wxButton(this, wxID_ANY, _("Goto"));
    gotoButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnGoto, this);
    gotoButton->SetToolTip(_("Traverse mount to selected astronomical feature"));
    Connect(wxID_EXIT, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(GotoDialog::OnGoto));
    rightBox->Add(gotoButton);
        

    wxTextCtrl * searchBar = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 
                                                0, wxDefaultValidator, wxTextCtrlNameStr);
    
    wxArrayString strings;
    strings.Add( "Pomegranate" );
    strings.Add( "Banana" );
    strings.Add( "Lemon" );
    strings.Add( "Melon" );
    strings.Add( "Coconut" );
    searchBar->AutoComplete( strings );

    leftBox->Add(searchBar, 
                 0, wxALL | wxALIGN_TOP | wxEXPAND, borderSize);

    rightBox->Add(new wxStaticText(this, -1, "Description"), 
                 0, wxALL | wxALIGN_TOP, borderSize);
    SetSizer(containBox);

    containBox->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags(0).Right().Border(wxALL, 10));

    /* Make bold 'description' label:
    
    m_label_info = new wxStaticText(this, -1, "blablabla");
    wxFont font = m_label_info->GetFont();
    font.SetPointSize(10);
    font.SetWeight(wxFONTWEIGHT_BOLD);
    m_label_info->SetFont(font);
    /* 

    /* Basic list ctrl code

    wxListCtrl * searchResults = new wxListCtrl (this, -1, wxDefaultPosition, wxDefaultSize, wxLC_ICON, 
                                                 wxDefaultValidator, wxListCtrlNameStr);
    // Add first column       
    wxListItem col0;
    col0.SetId(0);
    col0.SetText( _("Foo") );
    col0.SetWidth(50);
    searchResults->InsertColumn(0, col0);

    //searchResults->SetItem(1, 0, wxT("Foo"));
    searchResults->InsertItem(0, wxString("Balls!"));
    leftBox->Add(searchResults, 
                 1, wxALL | wxALIGN_TOP | wxEXPAND, borderSize);
    */
    

    //wxTextCtrl *MainEditBox;
    //MainEditBox = new wxTextCtrl(this, -1, "Hi!", wxDefaultPosition, wxDefaultSize,  
    //wxTE_MULTILINE | wxTE_RICH , wxDefaultValidator, wxTextCtrlNameStr);

    //mainBox->SetSizeHints(this); // If you want to set the window to the size of the contents.
    /*
    int width = StringWidth("0.0000") + 15;
    wxBoxSizer *pVSizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *pGridSizer = new wxFlexGridSizer(2, 10, 10);

    wxStaticText *pLabel = new wxStaticText(this, wxID_ANY, _("Camera binning:"));
    wxArrayString opts;
    pCamera->GetBinningOpts(&opts);
    m_binning = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, opts);
    m_binning->Select(cal.binning - 1);
    pGridSizer->Add(pLabel);
    pGridSizer->Add(m_binning);

    pLabel = new wxStaticText(this,wxID_ANY, _("RA rate, px/sec (e.g. 5.0):"));
    m_pXRate = new wxTextCtrl(this,wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1));
    m_pXRate->SetValue(wxString::Format("%.3f", cal.xRate * 1000.0));
    pGridSizer->Add(pLabel);
    pGridSizer->Add(m_pXRate);

    pLabel = new wxStaticText(this,wxID_ANY, _("Dec rate, px/sec (e.g. 5.0):"));
    m_pYRate = new wxTextCtrl(this,wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1));
    m_pYRate->SetValue(wxString::Format("%.3f", cal.yRate * 1000.0));
    pGridSizer->Add(pLabel);
    pGridSizer->Add(m_pYRate);

    pLabel = new wxStaticText(this,wxID_ANY, _("RA angle (degrees):"));
    m_pXAngle = new wxTextCtrl(this,wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1));
    m_pXAngle->SetValue(wxString::Format("%.1f", degrees(cal.xAngle)));
    pGridSizer->Add(pLabel);
    pGridSizer->Add(m_pXAngle);

    pLabel = new wxStaticText(this,wxID_ANY, _("Dec angle (degrees):"));
    m_pYAngle = new wxTextCtrl(this,wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1));
    m_pYAngle->SetValue(wxString::Format("%.1f", degrees(cal.yAngle)));
    pGridSizer->Add(pLabel);
    pGridSizer->Add(m_pYAngle);

    pLabel = new wxStaticText(this,wxID_ANY, _("Declination (e.g. 2.1):"));
    m_pDeclination = new wxTextCtrl(this,wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1));
    double dec = cal.declination == UNKNOWN_DECLINATION ? 0.0 : cal.declination;
    m_pDeclination->SetValue(wxString::Format("%.1f", dec));
    pGridSizer->Add(pLabel);
    pGridSizer->Add(m_pDeclination);

    pVSizer->Add(pGridSizer, wxSizerFlags(0).Border(wxALL, 10));
    pVSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags(0).Right().Border(wxALL, 10));

    SetSizerAndFit (pVSizer);

    m_pXRate->SetFocus();
    */
}

void GotoDialog::OnGoto(wxCommandEvent& )
{
    Close(true);
}

int GotoDialog::StringWidth(const wxString& string)
{
    int width, height;

    GetTextExtent(string, &width, &height);

    return width;
}

/*void GotoDialog::GetValues(Calibration *cal)
{
    double t;
    m_pXRate->GetValue().ToDouble(&t);
    cal->xRate = t / 1000.0;
    m_pYRate->GetValue().ToDouble(&t);
    cal->yRate = t / 1000.0;
    m_pXAngle->GetValue().ToDouble(&t);
    cal->xAngle = radians(t);
    m_pYAngle->GetValue().ToDouble(&t);
    cal->yAngle = radians(t);
    m_pDeclination->GetValue().ToDouble(&cal->declination);
    cal->binning = m_binning->GetSelection() + 1;
}*/

GotoDialog::~GotoDialog(void)
{
}
