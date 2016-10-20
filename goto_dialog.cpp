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
const char CATALOG_FILENAME[]        = "/usr/local/phd2/goto/catalog.csv";

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
    m_searchBar = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxTextCtrlNameStr);
    m_searchBar->Bind(wxEVT_COMMAND_TEXT_UPDATED, &GotoDialog::OnSearchTextChanged, this, wxID_ANY);

    wxArrayString catalog_keys;
    for ( auto kv : m_catalog ) {
        catalog_keys.Add(kv.first);
    }
    
    m_searchBar->AutoComplete( catalog_keys );

    searchBox->Add(m_searchBar, 0, wxALL | wxALIGN_TOP | wxEXPAND, borderSize);

    // Debugging only - add text ctrl for manually setting current sky position.

    m_skyAltManualSet = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxTextCtrlNameStr);
    m_skyAzManualSet  = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxTextCtrlNameStr);

    // Get a bold font

    wxStaticText *tempText = new wxStaticText(this, -1, "");
    wxFont boldFont = tempText->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);

    // Status sizer
    
    wxFlexGridSizer *statusGrid = new wxFlexGridSizer(2, 2, 2, 9);
    statusBox->Add(statusGrid);
    
    m_skyPosText                 = new wxStaticText(this, -1, "-");
    m_gpsLocText                 = new wxStaticText(this, -1, "35.2809° S,\n149.1300° E");
    m_timeText                   = new wxStaticText(this, -1, "-");
    
    wxStaticText *skyPosHeading                = new wxStaticText(this, -1, "Sky position");
    wxStaticText *skyAltManualSetHeading       = new wxStaticText(this, -1, "Set sky alt (degrees)");
    wxStaticText *skyAzManualSetHeading       = new wxStaticText(this, -1, "Set sky az (degrees)");
    wxStaticText *gpsLocHeading                = new wxStaticText(this, -1, "GPS location");
    wxStaticText *timeHeading                  = new wxStaticText(this, -1, "Time");
    skyPosHeading            ->SetFont(boldFont);
    skyAltManualSetHeading   ->SetFont(boldFont);
    skyAzManualSetHeading   ->SetFont(boldFont);
    gpsLocHeading            ->SetFont(boldFont);
    timeHeading              ->SetFont(boldFont);

    statusGrid->Add(skyPosHeading,               0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_skyPosText,                0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(skyAltManualSetHeading,      0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_skyAltManualSet,           0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(skyAzManualSetHeading,      0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_skyAzManualSet,            0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(gpsLocHeading,               0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_gpsLocText,                0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(timeHeading,                 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_timeText,                  0, wxALL | wxALIGN_TOP, borderSize);
    
    // Destination sizer

    wxFlexGridSizer *destinationGrid = new wxFlexGridSizer(4, 4, 2, 4);
    destinationBox->Add(destinationGrid);

    m_destinationRa     = new wxStaticText(this, -1, "-                      \n ");
    m_destinationDec    = new wxStaticText(this, -1, "-                      ");
    m_destinationAlt    = new wxStaticText(this, -1, "-                      ");
    m_destinationAz     = new wxStaticText(this, -1, "-                      ");
    m_destinationType   = new wxStaticText(this, -1, "-                      ");
    wxStaticText *destinationTypeHeading = new wxStaticText(this, -1, "Type");
    wxStaticText *destinationRaHeading   = new wxStaticText(this, -1, "RA");
    wxStaticText *destinationDecHeading  = new wxStaticText(this, -1, "Dec");
    wxStaticText *destinationAltHeading  = new wxStaticText(this, -1, "Alt");
    wxStaticText *destinationAzHeading   = new wxStaticText(this, -1, "Az");
    destinationRaHeading   ->SetFont(boldFont);
    destinationDecHeading  ->SetFont(boldFont);
    destinationAltHeading  ->SetFont(boldFont);
    destinationAzHeading   ->SetFont(boldFont);
    destinationTypeHeading ->SetFont(boldFont);

    destinationGrid->Add(destinationTypeHeading, 0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationType,      0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->AddSpacer(0); // An empty cell
    destinationGrid->AddSpacer(0); 
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

    SetSizer(containBox);

    m_timer = new wxTimer();
    m_timer->SetOwner(this);
    Bind(wxEVT_TIMER, &GotoDialog::OnTimer, this);
    m_timer->Start(1500); // Timer goes off every x milliseconds

    // Ask the guider to start saving images - astrometry may soon request one.
    pFrame->pGuider->RequestSaveImage();

    prevExposureDuration = 0;
    //int * prevExposureDuration_ptr = &prevExposureDuration;
    bool ignored;
    //bool * ignored_ptr = &ignored;

    // Store original exposure duration so we can go back to it.
    // Set high exposure duration to help Astrometry get a good image.
    // TODO: make this user-configurable (we don't know what their camera setup is like!)
    pFrame->GetExposureInfo(&prevExposureDuration, &ignored);
    if ( not pFrame->SetExposureDuration(3000) ) {
        Debug.AddLine("Goto: failed to increase exposure duration");
    }
    
}

void GotoDialog::OnTimer(wxTimerEvent& event) {
    UpdateLocationText();

}

void GotoDialog::degreesToHMS(double degrees, double &hours, double &minutes, double &seconds) {

    /*  RA is measured in hours, minutes and seconds. The maximum possible value is 24h (which is adjacent to zero).

        24 hours is 360 degrees, so one hour is 15 degrees, one minute is 0.25 degrees, and one second is 0.004166667 degrees.
    */

    hours = degrees / 15.0;
    double fraction;
    fraction = modf(hours, &hours);
    minutes = fraction * 60.0;
    fraction = modf(minutes, &minutes);
    seconds = fraction * 60.0; 

}

void GotoDialog::degreesToDMS(double input, double &degrees, double &arcMinutes, double &arcSeconds) {
    
    /* Declination is in degrees:arcmin:arcsec. It is signed. Max value is
       +90 at the north celesial pole, and min -90 at the south one.
 
       1 degree is 60 arcminutes
       1 arcminute is 60 arcseconds.
    */

    double fraction;
    fraction = modf(input, &degrees);
    arcMinutes = fraction * 60.0;
    fraction = modf(arcMinutes, &arcMinutes);
    arcSeconds = fraction * 60.0;

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
            LookupEphemeral(target, ra, dec, alt, az);
        } else {
            m_destinationType->SetLabel("Star");
            ra = stod(word);
            getline(resultStream, word, ',');
            dec = stod(word);
            EquatorialToHorizontal(ra, dec, alt, az, false);
        }
        
        //Debug.AddLine(wxString::Format("ra %f dec %f alt %f az %f", ra, dec, alt, az));
        
        //m_destinationRa->SetLabel(std::to_string(ra)); 
        //m_destinationDec->SetLabel(std::to_string(dec));
        //m_destinationAlt->SetLabel(std::to_string(alt));
        //m_destinationAz->SetLabel(std::to_string(az));

        double raHours;
        double raMinutes;
        double raSeconds;
        degreesToHMS(ra, raHours, raMinutes, raSeconds);
        char raBuffer[200]; 
        sprintf(raBuffer, "%f\n%.0fh %.0fm %.0fs", ra, raHours, raMinutes, raSeconds);
        m_destinationRa->SetLabel(raBuffer);

        double decDegrees;
        double decArcMinutes;
        double decArcSeconds;
        degreesToDMS(dec, decDegrees, decArcMinutes, decArcSeconds);
        char decBuffer[200];
        sprintf(decBuffer, "%f\n%.0fd %.0fm %.0fs", dec, decDegrees, abs(decArcMinutes), abs(decArcSeconds));
        m_destinationDec->SetLabel(decBuffer);

        double altDegrees;
        double altArcMinutes;
        double altArcSeconds;
        degreesToDMS(alt, altDegrees, altArcMinutes, altArcSeconds);
        char altBuffer[200];
        sprintf(altBuffer, "%f\n%.0fd %.0fm %.0fs", alt, altDegrees, altArcMinutes, altArcSeconds);
        m_destinationAlt->SetLabel(altBuffer);

        double azDegrees;
        double azArcMinutes;
        double azArcSeconds;
        degreesToDMS(az, azDegrees, azArcMinutes, azArcSeconds);
        char azBuffer[200];
        sprintf(azBuffer, "%f\n%.0fd %.0fm %.0fs", az, azDegrees, azArcMinutes, azArcSeconds);
        m_destinationAz->SetLabel(azBuffer);

        m_gotoButton->Enable();
    } else {
        m_destinationRa->SetLabel("-");
        m_destinationDec->SetLabel("-");
        m_destinationAlt->SetLabel("-");
        m_destinationAz->SetLabel("-");
        m_destinationType->SetLabel("-");
        m_gotoButton->Disable();
    }

    if ( not pFrame->pGuider->IsImageSaved() ) {
        m_gotoButton->Disable();
    } else {
        m_gotoButton->Enable();
    }
    //Debug.AddLine(wxString::Format("Goto: %s", pFrame->pGuider->ImageSaved() ? "true" : "false"));
    
}

bool GotoDialog::GetCatalogData(std::unordered_map<string,string>& outCatalog) {

    Debug.AddLine(wxString::Format("Goto: Preparing to read catalog"));
    string line;
    string cell;

    ifstream f (CATALOG_FILENAME); // TODO: Get application executable path & use that, rather than absolute path! 
    if (!f.is_open()) {
        Debug.AddLine("Goto: error while opening star catalog file");
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
        Debug.AddLine("Goto: error while reading star catalog file");
        return false;
    }
    return true;
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
    double startRa  = 0;
    double startDec = 0;
    double startAlt = 0;
    double startAz  = 0;
    double astroRotationAngle = 0;

    if ( m_skyAltManualSet->GetValue().length() > 0 && m_skyAzManualSet->GetValue().length() > 0 ) 
    {
        wxString contents = wxString::Format("Bypassing astrometry and assuming sky position is alt %s az %s", m_skyAltManualSet->GetValue(), m_skyAzManualSet->GetValue());
        wxMessageDialog * alert = new wxMessageDialog(pFrame, contents, wxString::Format("Goto"), wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();
        startAlt = stod(string(m_skyAltManualSet->GetValue()));
        startAz  = stod(string(m_skyAzManualSet ->GetValue()));
    } else if ( AstroSolveCurrentLocation(startRa, startDec, astroRotationAngle)) {
        EquatorialToHorizontal(startRa, startDec, startAlt, startAz, true);
        wxString contents = wxString::Format("Astrometry finished! Current location:\n RA %f, Dec %f\n Alt %f Az %f", startRa, startDec, startAlt, startAz);
        wxMessageDialog * alert = new wxMessageDialog(pFrame, contents, wxString::Format("Goto"), wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();
    } else {
        wxString contents = wxString("Unable to work out position with astrometry!\n"
                                     "Please check that the image is in focus, lens cap is off, and no clouds are occluding stars.\n"
                                     "Goto cannot proceed.");
        wxMessageDialog * alert = new wxMessageDialog(pFrame, contents, wxString::Format("Goto"), wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal(); 
        return;                                                             
    } 

    // -- Center of rotation --
    // Determine rotation center of image
    PHD_Point rotationCenter;
    pFrame->pGuider->GetRotationCenter(rotationCenter);

    // Get rotation center's distance from center of image
    usImage *pImage = pFrame->pGuider->CurrentImage();
    double height = pImage->Size.GetHeight();
    double width  = pImage->Size.GetWidth();
    rotationCenter.X -= pImage->Size.GetWidth() / 2;
    rotationCenter.Y -= pImage->Size.GetHeight() / 2;
    
    // Convert to degrees, using the known ratio of pixels to degrees of FOV for this camera
    // TODO - use the GetPixelScale method on the camera, rather than relying on knowing the StarShoot Autoguide's ratios
    rotationCenter.X *= 0.001695391; 
    rotationCenter.Y *= 0.001695137;

    // -- Camera angle --
    CalibrationDetails calDetails; 
    pMount->GetCalibrationDetails(&calDetails);

    // -- North celestial pole alt az
    double northCelestialPoleAlt = 0;
    double northCelestialPoleAz  = 0;
    EquatorialToHorizontal(0, 90, northCelestialPoleAlt, northCelestialPoleAz, true);
    
    pMount->HexCalibrate(startAlt, startAz, calDetails.cameraAngle, rotationCenter, astroRotationAngle, northCelestialPoleAlt); // TODO - fill in missing angle

    // --------------------
    // Goto!
    // --------------------

    UpdateLocationText(); // TODO, get the time x seconds from now (calculate how long move is likely to take)
    double destAlt = std::stod(string(m_destinationAlt->GetLabelText())); // Also, getting the time from the text string is not ideal.
    double destAz  = std::stod(string(m_destinationAz->GetLabelText())); 

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

bool GotoDialog::AstroSolveCurrentLocation(double &outRa, double &outDec, double &outAstroRotationAngle) {

    // Run astrometry, identify where we are,
    // and pipe the textual results from stdin into pos.ra and pos.dec.
    
    // The result is to call something like "/usr/local/astrometry/bin/solve-field --overwrite /dev/shm/phd2/goto/guide-image.fits"
    char inputFilename[200];
    strcpy(inputFilename, SOLVER_FILENAME);
    strcat(inputFilename, " --overwrite --no-plots ");
    strcat(inputFilename, IMAGE_FILENAME);
    Debug.AddLine(wxString::Format("inputFilename %s", inputFilename));

    // If simulator is being used as the camera, override with test input image
    if (dynamic_cast<Camera_SimClass*>(pCamera)) {
        wxMessageDialog * alert = new wxMessageDialog(pFrame, 
                                              wxString::Format("Since the current camera is the simulator, will use fake test image to determine position."), 
                                              wxString::Format("Goto"), 
                                              wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal();
        strcpy(inputFilename, "/usr/local/astrometry/bin/solve-field --overwrite --no-plots /usr/local/astrometry/examples/apod2.jpg");    
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

    unsigned int strLocation = astOutput.find("(RA,Dec)");
    if ( astOutput.find("(RA,Dec)") == string::npos) {
        return false; // Astrometry failed to solve
    }
    const string line = astOutput.substr(strLocation, strLocation+100);
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
    
    strLocation = astOutput.find("Field rotation angle");
    const string line2 = astOutput.substr(strLocation, strLocation+30);
    regex angleReg("[[:digit:]]+\\.?[[:digit:]]+");
    smatch angleMatch;
    regex_search(line2.begin(), line2.end(), angleMatch, angleReg);
    outAstroRotationAngle = stod(angleMatch[0]);
    Debug.AddLine(wxString::Format("Goto: rotation angle was %f uncorrected", outAstroRotationAngle));
    if ( line2.find("W of N") != std::string::npos ) {
        outAstroRotationAngle *= -1;
    }
    Debug.AddLine(wxString::Format("Goto: rotation angle was %f corrected", outAstroRotationAngle));

    return true;
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
    pFrame->SetExposureDuration(prevExposureDuration);
    pFrame->pGuider->InvalidateSavedImage();

}
