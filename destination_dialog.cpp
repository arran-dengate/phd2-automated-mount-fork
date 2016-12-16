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
#include "destination_dialog.h"
#include "destination.h"
#include <unordered_map>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unistd.h>
#include <stdlib.h> 
#include <algorithm>
const char CATALOG_FILENAME[]         = "/usr/local/phd2/goto/catalog.csv";


DestinationDialog::DestinationDialog(void)
    : wxDialog(pFrame, wxID_ANY, _("Destination"), wxDefaultPosition, wxDefaultSize, 0)
{
    wxBoxSizer * mainBox = new wxBoxSizer(wxVERTICAL);

    // -------------------------------------------
    // Get catalog data
    // -------------------------------------------

    {
        string line;
        string cell;

        ifstream f (CATALOG_FILENAME); // TODO: Get application executable path & use that, rather than absolute path! 
        if (!f.is_open()) {
            wxMessageDialog(pFrame, _("Unable to open star catalog file."), _("Error"), wxOK|wxCENTRE, wxDefaultPosition).ShowModal();
        }
        while(getline(f, line)) 
            {
                // If it's a star, expect format:   name,ra,dec (eg Rigel,21.04,67.32)
                // If it's a planet, expect format: name,type (eg Mars,planet)

                stringstream  lineStream(line);
                getline(lineStream,cell,',');
                string celestial = cell;
                getline(lineStream,cell,',');
                if (cell == "planet") {
                    catalog[celestial] = cell;
                } else {
                    string ra = cell;
                    getline(lineStream,cell,',');
                    string dec = cell;
                    catalog[celestial] = ra + "," + dec;    
                }
                
            }
        if (f.bad()) {
            wxMessageDialog(pFrame, _("Unable to read star catalog file."), _("Error"), wxOK|wxCENTRE, wxDefaultPosition).ShowModal();
        }
    }

    // -------------------------------------------
    // Create grid
    // -------------------------------------------

    destGrid = new wxGrid( this, -1, wxDefaultPosition, wxSize(300, 150));
    std::vector<string> catalogNames;

    for ( auto kv : catalog ) {
        //catalog_keys.Add(kv.first);
        catalogNames.push_back(kv.first);
        //destGrid->SetCellValue(i, 0, kv.first);
        //destGrid->SetCellValue(i, 1, kv.second);
    }

    std::sort(catalogNames.begin(), catalogNames.end());
    
    destGrid->CreateGrid(catalogNames.size(), 2);
    destGrid->Bind(wxEVT_GRID_CELL_LEFT_CLICK, &DestinationDialog::OnCellClicked, this);

    int i = 0;
    for (string s : catalogNames) {
        destGrid->SetCellValue(i, 0, s);
        i++;
    }

    destGrid->SetRowLabelSize(0);
    destGrid->SetColLabelSize(0);
    mainBox->Add(destGrid, 1, wxEXPAND | wxALL | wxCENTRE, 5);
    destGrid->AutoSizeColumns();
    destGrid->EnableGridLines(false);
    destGrid->EnableEditing(false);

    // -------------------------------------------
    // Make filter box
    // -------------------------------------------

    filterBar = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY, wxDefaultValidator, wxTextCtrlNameStr);
    mainBox->Add(filterBar, 0, wxALL | wxALIGN_TOP | wxEXPAND, 5);


    // -------------------------------------------
    // Set up keyboard
    // -------------------------------------------
    
    wxBoxSizer * keyRow1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer * keyRow2 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer * keyRow3 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer * keyRow4 = new wxBoxSizer(wxHORIZONTAL);

    mainBox->Add(keyRow1, 0.5, wxCENTRE | wxEXPAND, 5);
    mainBox->Add(keyRow2, 0.5, wxCENTRE | wxEXPAND, 5);
    mainBox->Add(keyRow3, 0.5, wxCENTRE | wxEXPAND, 5);
    mainBox->Add(keyRow4, 0.5, wxCENTRE | wxEXPAND, 5);
    
    std::vector<std::string> letters = {"1","2","3","4","5","6","7","8","9","0",
                                        "Q","W","E","R","T","Y","U","I","O","P",
                                        "A","S","D","F","G","H","J","K","L",
                                        "Z","X","C","V","B","N","M"};

    // We have to maintain a vector of button strings so we can work out which one was clicked when the event is triggered.
    int index = 0;
    for (std::string s : letters ) {
        wxButton * b = new wxButton(this, index, s, wxDefaultPosition, wxSize(50, -1), wxBU_EXACTFIT | wxBORDER_NONE, wxDefaultValidator, s);
        b->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &DestinationDialog::OnKeyClicked, this);
        keyMap.push_back(s);
        if (index < 10) {
            keyRow1->Add(b, 1, 0, 0); 
        } else if (index < 20) {
            keyRow2->Add(b, 1, 0, 0);
        } else if (index < 29) {
            keyRow3->Add(b, 1, 0, 0);
        } else {
            keyRow4->Add(b, 1, 0, 0);
        }
        index++;
    }

    wxBoxSizer * okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton   * cancelButton  = new wxButton(this, wxID_ANY, _("Cancel"));
    cancelButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &DestinationDialog::OnClose, this);
    okCancelSizer->Add(cancelButton);
    mainBox->Add(okCancelSizer, wxSizerFlags(0).Right().Border(wxALL, 10));

    chooseButton  = new wxButton(this, wxID_ANY, _("Choose"));
    chooseButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &DestinationDialog::OnChoose, this);
    chooseButton->Enable(false);
    okCancelSizer->Add(chooseButton);
    //gammaSlider->Bind(wxEVT_SLIDER, &DestinationDialog::OnGammaSlider, this);
    //gammaSlider->Bind(wxEVT_KILL_FOCUS, &DestinationDialog::OnKillFocus, this);

    wxButton * backspaceButton = new wxButton(this, index, _("⬅"), wxDefaultPosition, wxSize(75, -1), wxBU_EXACTFIT | wxBORDER_NONE, wxDefaultValidator, _("⇦"));
    backspaceButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &DestinationDialog::OnBackspace, this);
    keyRow4->Add(backspaceButton, 1, 0, 0);

    // wxBitmap * backspaceBmp = new wxBitmap(PHD2_FILE_PATH + "icons/backspace.png", wxBITMAP_TYPE_PNG);
    // wxBitmapButton * backspaceButton = new wxButton(this, wxID_ANY, _("Backspace"));

    SetSizer(mainBox);
    Fit();
}

void DestinationDialog::OnClose(wxCommandEvent& event) {
    this->EndModal(0);
}

void DestinationDialog::OnChoose(wxCommandEvent& event) {
    this->EndModal(0);
}

void DestinationDialog::OnBackspace(wxCommandEvent& event) {
    string text = std::string(filterBar->GetValue().mb_str());
    filterBar->SetValue(text.substr(0, text.length() - 1));
    chooseButton->Enable(false);
    RefreshGrid();
}

void DestinationDialog::OnCellClicked(wxGridEvent& event) {
    filterBar->SetValue(destGrid->GetCellValue(event.GetRow(), event.GetCol()));
    chooseButton->Enable(true);
}

void DestinationDialog::RefreshGrid() {
    std::vector<string> catalogNames;
    string filterString = std::string(filterBar->GetValue().mb_str());

    destGrid->ClearGrid();
    for ( auto kv : catalog ) {
        string upperString = kv.first;
        for(int i = 0; i < upperString.size(); i++) {
            upperString.at(i) = toupper(upperString.at(i));
        }

        //Debug.AddLine(wxString::Format("Upperstring %s kv first %s", filterString, upp));


        if (upperString.find(filterString) == 0) {//!= std::string::npos) {
            catalogNames.push_back(kv.first);
        }

        //if (kv.first.find(filterBar->GetText() != std::string::npos) ) {
        //catalog_keys.Add(kv.first);
        //}
    }
    

    std::sort(catalogNames.begin(), catalogNames.end());
    
    destGrid->CreateGrid(catalogNames.size(), 2);

    int i = 0;
    for (string s : catalogNames) {
        destGrid->SetCellValue(i, 0, s);
        i++;
    }
}

void DestinationDialog::OnKeyClicked(wxCommandEvent& event) {

    chooseButton->Enable(false);
    Debug.AddLine(wxString::Format("clicked id %d, key %s", event.GetId(), keyMap[event.GetId()]));
    //wxEVT_COMMAND_BUTTON_CLICKED
    filterBar->AppendText(keyMap[event.GetId()]);
    RefreshGrid();
}

bool DestinationDialog::GetDestination(Destination &result) {
    string destStr = filterBar->GetValue().ToStdString();
    if (catalog.count(destStr) == 1) {
        string inStr = destStr + "," + catalog[filterBar->GetValue().ToStdString()];
        result = Destination(destStr + "," + catalog[filterBar->GetValue().ToStdString()]);
        Debug.AddLine(wxString::Format("Destination: destStr %s name %s alt %d az %d", destStr, result.name, result.alt, result.az));
        Debug.AddLine(wxString::Format("Destination: result %s", inStr));
        return true;
    } else {
        return false;
    }
}

DestinationDialog::~DestinationDialog(void)
{
}
