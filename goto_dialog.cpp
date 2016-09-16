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
#include "goto_dialog.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <wx/progdlg.h>
#include "cam_simulator.h" // To determine if current camera is simulator

const char IMAGE_DIRECTORY[]         = "/dev/shm/phd2/goto";
const char IMAGE_PARENT_DIRECTORY[]  = "/dev/shm/phd2";
const char IMAGE_FILENAME[]          = "/dev/shm/phd2/goto/guide-scope-image.fits";
const char SOLVER_FILENAME[]         = "/usr/local/astrometry/bin/solve-field";

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
        
    // Autocomplete is broken for searchCtrl, so I had to use a textCtrl.
    wxTextCtrl * searchBar = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 
                                                0, wxDefaultValidator, wxTextCtrlNameStr);
    
    wxArrayString strings;
    strings.Add( "Arcturus" );
    strings.Add( "Sirius" );
    strings.Add( "Rigel" );
    strings.Add( "Canopus" );
    strings.Add( "Achernar" );
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
    // Create directory if does not exist
    struct stat info;
    if( stat( IMAGE_DIRECTORY, &info ) != 0 ) {
        mkdir(IMAGE_PARENT_DIRECTORY, 0755);
        mkdir(IMAGE_DIRECTORY, 0755);
    }
    pFrame->pGuider->SaveCurrentImage(IMAGE_FILENAME);
    double ra = 0;
    double dec = 0;
    double alt = 0;
    double az = 0;
    if ( AstroSolveCurrentLocation(ra, dec) ) {
        EquatorialToHorizontal(ra, dec, alt, az);
        pMount->HexGoto(alt, az);
        wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                                      wxString::Format("Astrometry current location ra %f, dec %f ; alt %f az %f", ra, dec, alt, az), 
                                                      wxString::Format("Goto"), 
                                                      wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();

    } else {
        wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                                      wxString::Format("Unable to work out position with astrometry! Please check that the image is in focus and current camera is not the simulator.\nGoto cannot proceed.", ra, dec), 
                                                      wxString::Format("Goto"), 
                                                      wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();                                                              
    }

    //    Debug.AddLine("OK");
    //}
    //Close(true);
}

bool GotoDialog::EquatorialToHorizontal(double ra, double dec, double &outAlt, double &outAz) {
    std::string command = "/usr/local/skyfield/sky.py --ra=" + std::to_string(ra) + " --dec=" + std::to_string(dec);
    string skyOutput;
    FILE *in;
    char buf[200];

    if(!(in = popen(command.c_str(), "r"))) {
        Debug.AddLine(wxString::Format("Guider: Skyfield threw error while converting equatorial to horizontal coordinates"));
        return 1;
    }
    while(fgets(buf, sizeof(buf), in)!=NULL) {
        cout << buf;
        skyOutput += buf;
    }
    pclose(in);

    std::size_t found = skyOutput.find(",");
    outAlt = stod(skyOutput.substr(0, found - 1 ));
    outAz  = stod(skyOutput.substr(found+1, skyOutput.size()));
     
    return 0;
}

bool GotoDialog::AstroSolveCurrentLocation(double &outRa, double &outDec) {

    // Run astrometry, identify where we are,
    // and pipe the textual results from stdin into pos.ra and pos.dec.
    
    // The result is to call something like "/usr/local/astrometry/bin/solve-field --overwrite /dev/shm/phd2/goto/guide-image.fits"
    char inputFilename[200];
    strcpy(inputFilename, SOLVER_FILENAME);
    strcat(inputFilename, " --overwrite");
    strcat(inputFilename, IMAGE_FILENAME);
    Debug.AddLine(wxString::Format("inputFilename %s", inputFilename));

    // If simulator is being used as the camera, override with test input image
    if (dynamic_cast<Camera_SimClass*>(pCamera)) {
        wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                              wxString::Format("Since the current camera is the simulator, will use fake test image to determine position."), 
                                              wxString::Format("Goto"), 
                                              wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();
        strcpy(inputFilename, "/usr/local/astrometry/bin/solve-field --overwrite /usr/local/astrometry/examples/apod2.jpg");    
    }
    
    wxProgressDialog(wxString::Format("Solving..."), wxString::Format("Solving..."), 100, this, wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME); 
    
    string astOutput;
    FILE *in;
    char buff[512];

    if(!(in = popen(inputFilename, "r"))){
        return 1;
    }
    while(fgets(buff, sizeof(buff), in)!=NULL){
        cout << buff;
        astOutput += buff;
    }
    pclose(in);

    // FITS image is no longer needed, can be deleted.
    //remove(IMAGE_FILENAME);

    Debug.AddLine(wxString::Format("About to search string"));
    //raise(SIGINT);
     
    unsigned int strLocation = astOutput.find("(RA,Dec)");
    if ( astOutput.find("(RA,Dec)") == string::npos) {
        return false; // Astrometry failed to solve
    }
    const string line = astOutput.substr(strLocation, strLocation+30);
    // const string line = "Field center: (RA,Dec) = (104.757911, -3.500760) deg.";
    
    regex raReg("\\(-?[[:digit:]]+(\\.[[:digit:]]+)?");
    regex decReg("-?[[:digit:]]+(\\.[[:digit:]]+)?\\)");
    smatch raMatch;
    smatch decMatch;
    regex_search(line.begin(), line.end(), raMatch, raReg); 
    regex_search(line.begin(), line.end(), decMatch, decReg);
    string raResult = raMatch[0];
    string decResult = decMatch[0];
    outRa = stod(raResult.substr(1, raResult.size())); // Trim leading bracket
    outDec = stod(decResult.substr(0, decResult.size()-1)); // Trim ending bracket
    
    // printf("Ra %f Dec %f", ra, dec);

    return true; // todo: return error code if failed
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
