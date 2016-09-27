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
#include <unordered_map>
#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <wx/progdlg.h>
#include "cam_simulator.h" // To determine if current camera is simulator

const char IMAGE_DIRECTORY[]         = "/dev/shm/phd2/goto";
const char IMAGE_PARENT_DIRECTORY[]  = "/dev/shm/phd2";
const char IMAGE_FILENAME[]          = "/dev/shm/phd2/goto/guide-scope-image.fits";
const char SOLVER_FILENAME[]         = "/usr/local/astrometry/bin/solve-field";

GotoDialog::GotoDialog(void)
    : wxDialog(pFrame, wxID_ANY, _("Go to..."), wxDefaultPosition, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX)
{   
    // Obtain the catalog data from a CSV file...

    
    if ( ! GetCatalogData(m_catalog) ) {
        wxMessageDialog * alert = new wxMessageDialog(pFrame, wxString::Format("Unable to locate or read star catalog file! Goto will not work."), 
                                                      wxString::Format("Error"), wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();
    }

    // Now set up GUI.

    wxBoxSizer *containBox           = new wxBoxSizer(wxVERTICAL);
    //wxFlexGridSizer *containBox = new wxFlexGridSizer(2, 2, 2, 9);
    //wxBoxSizer *mainBox              = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *lowerBox             = new wxBoxSizer(wxHORIZONTAL);
    wxStaticBoxSizer *searchBox      = new wxStaticBoxSizer(wxVERTICAL, this, "Search");
    wxStaticBoxSizer *statusBox      = new wxStaticBoxSizer(wxVERTICAL, this, "Status");
    wxStaticBoxSizer *destinationBox = new wxStaticBoxSizer(wxVERTICAL, this, "Destination");

    containBox->Add(searchBox, 1, wxEXPAND);
    containBox->Add(lowerBox, 1, wxEXPAND);
    lowerBox->Add(statusBox, 1, wxEXPAND);
    lowerBox->Add(destinationBox, 1, wxEXPAND);
    
    int borderSize = 5;
    
    // Autocomplete is broken for searchCtrl, so I had to use a textCtrl.
    m_searchBar = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 
                                                0, wxDefaultValidator, wxTextCtrlNameStr);
    m_searchBar->Bind(wxEVT_COMMAND_TEXT_UPDATED, &GotoDialog::OnSearchTextChanged, this, wxID_ANY);

    wxArrayString catalog_keys;
    for ( auto kv : m_catalog ) {
        catalog_keys.Add(kv.first);
    }
    
    m_searchBar->AutoComplete( catalog_keys );

    searchBox->Add(m_searchBar, 0, wxALL | wxALIGN_TOP | wxEXPAND, borderSize);

    // Get a bold font

    wxStaticText *tempText = new wxStaticText(this, -1, "");
    wxFont boldFont = tempText->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);

    // Status sizer
    
    wxFlexGridSizer *statusGrid = new wxFlexGridSizer(2, 2, 2, 9);
    statusBox->Add(statusGrid);
    
    m_skyPosText                 = new wxStaticText(this, -1, "Waiting for astrometry");
    m_gpsLocText                 = new wxStaticText(this, -1, "35.2809° S, 149.1300° E");
    m_timeText                   = new wxStaticText(this, -1, "-");
    
    wxStaticText *skyPosHeading  = new wxStaticText(this, -1, "Sky position");
    wxStaticText *gpsLocHeading  = new wxStaticText(this, -1, "GPS location");
    wxStaticText *timeHeading    = new wxStaticText(this, -1, "Time");
    skyPosHeading->SetFont(boldFont);
    gpsLocHeading->SetFont(boldFont);
    timeHeading->SetFont(boldFont);

    statusGrid->Add(skyPosHeading, 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_skyPosText, 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(gpsLocHeading, 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_gpsLocText, 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(timeHeading, 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_timeText, 0, wxALL | wxALIGN_TOP, borderSize);
    
    // Destination sizer

    wxFlexGridSizer *destinationGrid = new wxFlexGridSizer(4, 4, 2, 4);
    destinationBox->Add(destinationGrid);

    m_destinationRa   = new wxStaticText(this, -1, "-                      ");
    m_destinationDec  = new wxStaticText(this, -1, "-                      ");
    m_destinationAlt  = new wxStaticText(this, -1, "-                      ");
    m_destinationAz   = new wxStaticText(this, -1, "-                      ");
    m_destinationType = new wxStaticText(this, -1, "-                      ");
    wxStaticText *destinationTypeHeading = new wxStaticText(this, -1, "Type");
    wxStaticText *destinationRaHeading   = new wxStaticText(this, -1, "RA");
    wxStaticText *destinationDecHeading  = new wxStaticText(this, -1, "Dec");
    wxStaticText *destinationAltHeading  = new wxStaticText(this, -1, "Alt");
    wxStaticText *destinationAzHeading   = new wxStaticText(this, -1, "Az");
    destinationRaHeading->SetFont(boldFont);
    destinationDecHeading->SetFont(boldFont);
    destinationAltHeading->SetFont(boldFont);
    destinationAzHeading->SetFont(boldFont);
    destinationTypeHeading->SetFont(boldFont);

    destinationGrid->Add(destinationTypeHeading, 0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationType,      0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->AddSpacer(0); // An empty cell
    destinationGrid->AddSpacer(0); // An empty cell
    destinationGrid->Add(destinationRaHeading,   0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationRa,        0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(destinationDecHeading,  0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationDec,       0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(destinationAltHeading,  0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationAlt,       0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(destinationAzHeading,   0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationAz,        0, wxALL | wxALIGN_TOP, borderSize);

    // Button sizer along the bottom with 'Cancel' and 'Goto' buttons

    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    containBox->Add(buttonSizer, wxSizerFlags(0).Right().Border(wxALL, 10));

    wxButton *cancelButton = new wxButton(this, wxID_ANY, _("Cancel"));
    cancelButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnClose, this);
    buttonSizer->Add(cancelButton);

    m_gotoButton = new wxButton(this, wxID_ANY, _("Go to"));
    m_gotoButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnGoto, this);
    m_gotoButton->SetToolTip(_("Traverse mount to selected astronomical feature"));
    m_gotoButton->Disable();
    buttonSizer->Add(m_gotoButton);

    //containBox->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags(0).Right().Border(wxALL, 10));
 
    /*wxStaticText m_label_info = new wxStaticText(this, -1, "blablabla");
    wxFont font = m_label_info->GetFont();
    font.SetPointSize(10);
    font.SetWeight(wxFONTWEIGHT_BOLD);
    m_label_info->SetFont(font);*/
   
    SetSizer(containBox);

    /*
    //wxTimer timer;
    m_timer = new wxTimer();
    m_timer->SetOwner(this);
    //timer.Connect( wxEVT_TIMER, wxTimerEventHandler(GotoDialog::OnTimer), NULL, this);
    m_timer->Bind(wxEVT_TIMER, &GotoDialog::OnTimer, this, m_timer->GetId());
    m_timer->Start(100, wxTIMER_CONTINUOUS);
    */

    m_timer = new wxTimer();
    m_timer->SetOwner(this);
    Bind(wxEVT_TIMER, &GotoDialog::OnTimer, this);
    m_timer->Start(1500); // Timer goes off every x milliseconds

}

void GotoDialog::OnTimer(wxTimerEvent& event) {
    UpdateLocationText();
}


void GotoDialog::OnSearchTextChanged(wxCommandEvent&) {   
    UpdateLocationText();
}

void GotoDialog::UpdateLocationText(void) {
    if ( m_catalog.count(string(m_searchBar->GetValue())) ) {
        string target = string(m_searchBar->GetValue());
        //m_destinationEquatorial->SetLabel(m_catalog[string(m_searchBar->GetValue())]);

        string word;
        string result = m_catalog[string(m_searchBar->GetValue())];
        stringstream resultStream(result);
        getline(resultStream, word, ',');

        double ra;
        double dec;
        double alt;
        double az;

        if (! isdigit(word[0])) {
            m_destinationType->SetLabel(word);
            Debug.AddLine("Planet!");
            LookupEphemeral(target, ra, dec, alt, az);
        } else {
            m_destinationType->SetLabel("Star");
            ra = stod(word);
            getline(resultStream, word, ',');
            dec = stod(word);
            EquatorialToHorizontal(ra, dec, alt, az, false);
        }
        
        //Debug.AddLine(wxString::Format("ra %f dec %f alt %f az %f", ra, dec, alt, az));
        
        m_destinationRa->SetLabel(std::to_string(ra)); 
        m_destinationDec->SetLabel(std::to_string(dec));
        m_destinationAlt->SetLabel(std::to_string(alt));
        m_destinationAz->SetLabel(std::to_string(az));
        m_gotoButton->Enable();
    } else {
        m_destinationRa->SetLabel("-");
        m_destinationDec->SetLabel("-");
        m_destinationAlt->SetLabel("-");
        m_destinationAz->SetLabel("-");
        m_destinationType->SetLabel("-");
        m_gotoButton->Disable();
    }
}

bool GotoDialog::GetCatalogData(std::unordered_map<string,string>& outCatalog) {

    Debug.AddLine(wxString::Format("Preparing to read catalog"));
    string line;
    string cell;

    ifstream f ("/home/pi/src/phd2/goto/catalog.csv"); // TODO: Get application executable path & use that, rather than absolute path! 
    if (!f.is_open()) {
        perror("error while opening file");
        return false;
    }
    while(getline(f, line)) 
        {
            // If it's a star, expect format:   name,ra,dec (eg Rigel,21.04,67.32)
            // If it's a planet, expect format: name,planet (eg Mars,planet)

            stringstream  lineStream(line);
            getline(lineStream,cell,',');
            string celestial = cell;
            getline(lineStream,cell,',');
            if (cell == "planet") {
                outCatalog[celestial] = cell;
            } else {
                string ra = cell;
                getline(lineStream,cell,',');
                string dec = cell;
                //Debug.AddLine(wxString::Format("Line %s", line));
                //Debug.AddLine(wxString::Format("celestial %s", celestial));
                //Debug.AddLine(wxString::Format("RA %s", ra));
                //Debug.AddLine(wxString::Format("Dec %s", dec));
                outCatalog[celestial] = ra + "," + dec;    
            }
            
        }
    if (f.bad()) {
        perror("error while reading file");
        return false;
    }

    Debug.AddLine(wxString::Format("Finished reading catalog"));
    //Debug.AddLine(wxString::Format("Catalogue contents for Wezen: %s", catalog["Wezen"]));

} 

void GotoDialog::OnGoto(wxCommandEvent& )
{
    // TODO - integrate the altitude check as a warning into the main dialog and disasbled goto button, rather than modal dialog.
    if (std::stod(string(m_destinationAlt->GetLabel())) < 0 ) {
        wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                                      wxString::Format("Destination is below the horizon and cannot be viewed!\n"
                                                                       "If you believe this is not the case, check that the time and GPS location are correct."), 
                                                      wxString::Format("Cannot goto"), 
                                                      wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();
        return;
    }

    // --------------------
    // Perform calibration!
    // --------------------

    // Create directory if does not exist
    struct stat info;
    if( stat( IMAGE_DIRECTORY, &info ) != 0 ) {
        mkdir(IMAGE_PARENT_DIRECTORY, 0755);
        mkdir(IMAGE_DIRECTORY, 0755);
    }
    //pFrame->pGuider->SaveCurrentImage(IMAGE_FILENAME); TEMP DISABLED FOR DEBUGGING - we are saving every image at the moment
    double startRa = 0;
    double startDec = 0;
    double startAlt = 0;
    double startAz = 0;
    if ( AstroSolveCurrentLocation(startRa, startDec) ) {
        EquatorialToHorizontal(startRa, startDec, startAlt, startAz, true);
        PHD_Point rotationCenter;
        rotationCenter.X = 0;
        rotationCenter.Y = 0; 
        //pFrame->pGuider->GetRotationCenter(rotationCenter); DISABLED FOR DEBUGGING ONLY, TURN THIS BACK ON!
        Debug.AddLine(wxString::Format("Goto: rotationcenter %f %f", rotationCenter.X, rotationCenter.Y));
        pMount->HexCalibrate(startAlt, startAz, 0.0, rotationCenter, 0.0); // TODO - fill in missing figures!
        wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                                      wxString::Format("Astrometry finished! Current location:\n"
                                                                       "RA %f, Dec %f\n"
                                                                       "Alt %f Az %f", startRa, startDec, startAlt, startAz), 
                                                      wxString::Format("Goto"), 
                                                      wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();
    } else {
        wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                                      wxString::Format("Unable to work out position with astrometry!\n"
                                                                       "Please check that the image is in focus, lens cap is off, and no clouds are occluding stars.\n"
                                                                       "Goto cannot proceed."), 
                                                      wxString::Format("Goto"), 
                                                      wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();                                                              
    }

    // --------------------
    // Goto!
    // --------------------

    UpdateLocationText(); // TODO, get the time x seconds from now (calculate how long move is likely to take)
    double destAlt = std::stod(string(m_destinationAlt->GetLabel())); // Also, getting the time from the text string is not ideal.
    double destAz  = std::stod(string(m_destinationAz->GetLabel())); 

    wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                                  wxString::Format("Traversing mount to destination:\n"  
                                                                    "Alt %f, az %f", destAlt, destAz), 
                                                  wxString::Format("Goto"), 
                                                  wxOK|wxCENTRE, wxDefaultPosition);
    alert->ShowModal();

    pMount->HexGoto(destAlt, destAz);


}

bool GotoDialog::EquatorialToHorizontal(double ra, double dec, double &outAlt, double &outAz, bool useStoredTimestamp) {
    std::string command = "/usr/local/skyfield/sky.py --ra=" + std::to_string(ra) + " --dec=" + std::to_string(dec);
    if ( useStoredTimestamp ) {
        command += " --use-stored";
    }
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

bool GotoDialog::LookupEphemeral(string &ephemeral, double &outRa, double &outDec, double &outAlt, double &outAz) {
    std::string command = "/usr/local/skyfield/sky.py --planet=" + ephemeral;
    string skyOutput;
    FILE *in;
    char buf[200];

    if(!(in = popen(command.c_str(), "r"))) {
        Debug.AddLine(wxString::Format("Guider: Skyfield threw error while looking up ephemeral for horizontal coordinates"));
        return 1;
    }
    while(fgets(buf, sizeof(buf), in)!=NULL) {
        cout << buf;
        skyOutput += buf;
    }
    pclose(in);

    string word;
    stringstream resultStream(skyOutput);
    getline(resultStream, word, ',');
    outAlt  = stod(word);
    getline(resultStream, word, ',');
    outAz   = stod(word);
    getline(resultStream, word, ',');
    outRa   = stod(word);
    getline(resultStream, word, ',');
    outDec  = stod(word);

    return 0;
}

bool GotoDialog::AstroSolveCurrentLocation(double &outRa, double &outDec) {

    // Run astrometry, identify where we are,
    // and pipe the textual results from stdin into pos.ra and pos.dec.
    
    // The result is to call something like "/usr/local/astrometry/bin/solve-field --overwrite /dev/shm/phd2/goto/guide-image.fits"
    char inputFilename[200];
    strcpy(inputFilename, SOLVER_FILENAME);
    strcat(inputFilename, " --overwrite ");
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
    
    wxProgressDialog(wxString::Format("Solving current location..."), wxString::Format("Solving..."), 100, this, wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME); 
    
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
    
    return true; // todo: return error code if failed
}

int GotoDialog::StringWidth(const wxString& string)
{
    int width, height;

    GetTextExtent(string, &width, &height);

    return width;
}

void GotoDialog::OnClose(wxCommandEvent& event) {
    this->EndModal(0);
}

GotoDialog::~GotoDialog(void)
{
    m_timer->Stop();
}
