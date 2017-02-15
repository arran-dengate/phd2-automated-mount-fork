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
#include "cam_simulator.h" // To determine if current camera is simulator
#include "destination_dialog.h"
#include "destination.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <tuple>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <wx/checkbox.h>
#include <wx/progdlg.h>
#include <stdlib.h> 
#include <ctime>


const char IMAGE_DIRECTORY[]          = "/dev/shm/phd2/goto";
const char IMAGE_PARENT_DIRECTORY[]   = "/dev/shm/phd2";
const char IMAGE_FILENAME[]           = "/dev/shm/phd2/goto/guide-scope-image.fits";
const char SOLVER_FILENAME[]          = "/usr/local/astrometry/bin/solve-field";
const char CATALOG_FILENAME[]         = "/usr/local/phd2/goto/catalog.csv";
const char MOVE_COMPLETE_FILENAME[]   = "/dev/shm/phd2/goto/done";

GotoDialog::GotoDialog(void)
    : wxDialog(pFrame, wxID_ANY, _("Go to..."), wxDefaultPosition, wxSize(600, 265), wxCAPTION | wxCLOSE_BOX)
{   
    m_doAccuracyMap = false;
    gotoInProgress = false;
    calibrated = false;

    // Now set up GUI.

    wxBoxSizer *containBox           = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *lowerBox             = new wxBoxSizer(wxHORIZONTAL);
    wxStaticBoxSizer *statusBox      = new wxStaticBoxSizer(wxVERTICAL, this, "Status");
    wxStaticBoxSizer *destinationBox = new wxStaticBoxSizer(wxVERTICAL, this, "Destination");

    containBox->Add(lowerBox, 1, wxEXPAND);
    lowerBox->Add(statusBox, 1, wxEXPAND);
    lowerBox->Add(destinationBox, 1, wxEXPAND);
    
    int borderSize = 5;
    
    // Get a bold font

    wxStaticText *tempText = new wxStaticText(this, -1, "");
    wxFont boldFont = tempText->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);

    // Status sizer
    
    wxFlexGridSizer *statusGrid = new wxFlexGridSizer(2, 2, 2, 9);
    statusBox->Add(statusGrid);
    
    m_stateText                 = new wxStaticText(this, -1, "Idle");
    m_skyPosText                = new wxStaticText(this, -1, "-");
    m_gpsLocText                = new wxStaticText(this, -1, "35.2809° S,\n149.1300° E");
    m_timeText                  = new wxStaticText(this, -1, "-"); 
    wxStaticText *stateHeading  = new wxStaticText(this, -1, "State");
    wxStaticText *skyPosHeading = new wxStaticText(this, -1, "Sky position");
    wxStaticText *gpsLocHeading = new wxStaticText(this, -1, "GPS location");
    wxStaticText *timeHeading   = new wxStaticText(this, -1, "Time");
    stateHeading  ->SetFont(boldFont);
    skyPosHeading ->SetFont(boldFont);
    gpsLocHeading ->SetFont(boldFont);
    timeHeading   ->SetFont(boldFont);

    statusGrid->Add(stateHeading,                0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_stateText,                 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(skyPosHeading,               0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_skyPosText,                0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(gpsLocHeading,               0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_gpsLocText,                0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(timeHeading,                 0, wxALL | wxALIGN_TOP, borderSize);
    statusGrid->Add(m_timeText,                  0, wxALL | wxALIGN_TOP, borderSize);
    
    // Destination sizer

    wxFlexGridSizer *destinationGrid = new wxFlexGridSizer(4, 4, 2, 4);
    destinationBox->Add(destinationGrid);

    m_destinationName   = new wxStaticText(this, -1, "-");
    m_destinationType   = new wxStaticText(this, -1, "-");
    m_destinationRa     = new wxStaticText(this, -1, "-                      \n ");
    m_destinationDec    = new wxStaticText(this, -1, "-                      ");
    m_destinationAlt    = new wxStaticText(this, -1, "-                      ");
    m_destinationAz     = new wxStaticText(this, -1, "-                      \n");
    wxStaticText *destinationNameHeading = new wxStaticText(this, -1, "Name");
    wxStaticText *destinationTypeHeading = new wxStaticText(this, -1, "Type");
    wxStaticText *destinationRaHeading   = new wxStaticText(this, -1, "RA");
    wxStaticText *destinationDecHeading  = new wxStaticText(this, -1, "Dec");
    wxStaticText *destinationAltHeading  = new wxStaticText(this, -1, "Alt");
    wxStaticText *destinationAzHeading   = new wxStaticText(this, -1, "Az");
    destinationNameHeading ->SetFont(boldFont);
    destinationTypeHeading ->SetFont(boldFont);
    destinationRaHeading   ->SetFont(boldFont);
    destinationDecHeading  ->SetFont(boldFont);
    destinationAltHeading  ->SetFont(boldFont);
    destinationAzHeading   ->SetFont(boldFont);

    destinationGrid->Add(destinationNameHeading, 0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationName,      0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->AddSpacer(0); // An empty cell
    destinationGrid->AddSpacer(0); 
    destinationGrid->Add(destinationTypeHeading, 0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationType,      0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->AddSpacer(0); 
    destinationGrid->AddSpacer(0); 
    destinationGrid->Add(destinationRaHeading,   0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationRa,        0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(destinationDecHeading,  0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationDec,       0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(destinationAltHeading,  0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationAlt,       0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(destinationAzHeading,   0, wxALL | wxALIGN_TOP, borderSize);
    destinationGrid->Add(m_destinationAz,        0, wxALL | wxALIGN_TOP, borderSize);

    wxButton * changeDestinationButton = new wxButton(this, wxID_ANY, _("Change destination"));
    changeDestinationButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnChangeDestination, this);
    destinationBox->AddStretchSpacer(1);
    destinationBox->Add(changeDestinationButton, 0, wxEXPAND | wxALL, 5);

    // Iterative goto (currently disabled - needs more work)
    //m_recalibrateDuringGoto = new wxCheckBox(this, -1, "Recalibrate during goto\n(slower, more accurate)", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT, wxDefaultValidator, wxTextCtrlNameStr);
    //destinationBox->Add(m_recalibrateDuringGoto, 0, wxALL | wxEXPAND | wxALIGN_LEFT, borderSize); 

    // Button sizer along the bottom with 'Cancel' and 'Goto' buttons

    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    containBox->Add(buttonSizer, wxSizerFlags(0).Right().Border(wxALL, 10));

    wxButton *cancelButton = new wxButton(this, wxID_ANY, _("Cancel"));
    cancelButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnClose, this);
    buttonSizer->Add(cancelButton);

    //m_debugButton = new wxButton(this, wxID_ANY, _("Debug"));
    //m_debugButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnDebug, this);
    //m_debugButton->SetToolTip(_("Take a series of exposures at different positions to map goto error"));
    //buttonSizer->Add(m_debugButton);

    m_calibrateButton = new wxButton(this, wxID_ANY, _("Calibrate"));
    m_calibrateButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnCalibrate, this);
    m_calibrateButton->SetToolTip(_("Take an exposure and work out where mount is currently pointing"));
    statusBox->AddStretchSpacer(1);
    statusBox->Add(m_calibrateButton, 0, wxEXPAND | wxALL, 5);

    m_gotoButton = new wxButton(this, wxID_ANY, _("Go to"));
    m_gotoButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GotoDialog::OnGoto, this);
    m_gotoButton->SetToolTip(_("Traverse mount to selected astronomical feature"));
    //m_gotoButton->Disable(); TODO - fix this so it only disables when no valid destination
    buttonSizer->Add(m_gotoButton);

    SetSizer(containBox);

    m_timer = new wxTimer();
    m_timer->SetOwner(this);
    Bind(wxEVT_TIMER, &GotoDialog::OnTimer, this);
    m_timer->Start(1000); // Timer goes off every x milliseconds

    // Ask the guider to start saving images - astrometry may soon request one.
    pFrame->pGuider->RequestSaveImage();

    prevExposureDuration = 0;
    //int * prevExposureDuration_ptr = &prevExposureDuration;
    //bool * ignored_ptr = &ignored;

    // Store original exposure duration so we can go back to it.
    // Set high exposure duration to help Astrometry get a good image.
    // TODO: make this user-configurable (we don't know what their camera setup is like!)
    //pFrame->GetExposureInfo(&prevExposureDuration, &ignored);
    //if ( not pFrame->SetExposureDuration(2000) ) {
    //    Debug.AddLine("Goto: failed to increase exposure duration");
    //}
    
}

void GotoDialog::OnTimer(wxTimerEvent& event) {
    destination.NonBlockingUpdate();
    destination.CheckForUpdate();
    UpdateStatusText();
    UpdateDestinationText();

    if ( m_doAccuracyMap ) AccuracyMap();

    if (gotoInProgress) Goto();  

    /*if ( m_gotoInProgress ) {
        struct stat info;
        Debug.AddLine("Goto: in progress, waiting for 'done' file...");
        if ( stat( MOVE_COMPLETE_FILENAME , &info ) == 0 ) {  // TODO: Get application executable path & use that, rather than absolute path! 
            Calibrate();
            pMount->HexGoto(destination.alt, destination.az);
            m_gotoInProgress = false;
            Debug.AddLine("Goto: Sent second move command; complete!");
        } 
    }*/
}

void GotoDialog::UpdateStatusText(void) {
    std::time_t result = std::time(nullptr);
    m_timeText->SetLabel(std::ctime(&result));
    (calibrated) ? m_skyPosText->SetLabel("Calibrated") : m_skyPosText->SetLabel("Not calibrated");
    (gotoInProgress) ? m_stateText->SetLabel("Goto in progress") : m_stateText->SetLabel("Idle"); 

}

void GotoDialog::UpdateDestinationText(void) {
    if ( destination.initialised ) {

        if (destination.ephemeral) {
            m_destinationType->SetLabel(destination.type);
        } else {
            m_destinationType->SetLabel("Star");
        }

        m_destinationName->SetLabel(destination.name);

        double raHours;
        double raMinutes;
        double raSeconds;
        destination.GetRaHMS(raHours, raMinutes, raSeconds);
        char raBuffer[200]; 
        sprintf(raBuffer, "%f\n%.0fh %.0fm %.0fs", destination.ra, raHours, raMinutes, raSeconds);
        m_destinationRa->SetLabel(raBuffer);

        double decDegrees;
        double decArcMinutes;
        double decArcSeconds;
        destination.GetDecDMS(decDegrees, decArcMinutes, decArcSeconds);
        char decBuffer[200];
        sprintf(decBuffer, "%f\n%.0fd %.0fm %.0fs", destination.dec, decDegrees, abs(decArcMinutes), abs(decArcSeconds));
        m_destinationDec->SetLabel(decBuffer);

        double altDegrees;
        double altArcMinutes;
        double altArcSeconds;
        destination.GetAltDMS(altDegrees, altArcMinutes, altArcSeconds);
        char altBuffer[200];
        sprintf(altBuffer, "%f\n%.0fd %.0fm %.0fs", destination.alt, altDegrees, altArcMinutes, altArcSeconds);
        m_destinationAlt->SetLabel(altBuffer);

        double azDegrees;
        double azArcMinutes;
        double azArcSeconds;
        destination.GetAzDMS(azDegrees, azArcMinutes, azArcSeconds);
        char azBuffer[200];
        sprintf(azBuffer, "%f\n%.0fd %.0fm %.0fs", destination.az, azDegrees, azArcMinutes, azArcSeconds);
        m_destinationAz->SetLabel(azBuffer);

        m_gotoButton->Enable();
    } else {
        m_destinationName->SetLabel("-");
        m_destinationRa->SetLabel("-                      \n");
        m_destinationDec->SetLabel("-                      ");
        m_destinationAlt->SetLabel("-                      \n");
        m_destinationAz->SetLabel("-                      ");
        m_destinationType->SetLabel("-                      ");
    }
}

void GotoDialog::ShowDestinationDialog() {
    DestinationDialog * destDlg = new DestinationDialog();
    destDlg->ShowModal();
    Destination result = Destination();
    if (destDlg->GetDestination(result)) {
        Debug.AddLine(wxString::Format("Destination: name %s alt %d az %d", destination.name, destination.alt, destination.az));
        destination = result;
    }
}

void GotoDialog::OnChangeDestination(wxCommandEvent&) {
    ShowDestinationDialog();
}

void GotoDialog::AccuracyMap() {
    // I started writing something that would go to a variety of alt az locations, and test the actual astrometric alt az
    // results. If there's repeatable, systematic error, we could perhaps interpolate this map to compensate for the error.
/*    if ( not pFrame->pGuider->IsImageSaved() ) {
        pFrame->pGuider->RequestSaveImage();
        Debug.AddLine("Goto: image not ready yet");
        return;
    }

    double ra, dec, astroRotationAngle;
    if ( m_solveTriesRemaining > 0) {
        if ( ! AstroSolveCurrentLocation(ra, dec, astroRotationAngle) ) {
            m_solveTriesRemaining --;
            pFrame->pGuider->RequestSaveImage();
            Debug.AddLine("Goto accMap: failed to solve");
            return; 
        } else {
            Debug.AddLine(wxString::Format("Goto accMap: solved ra %f dec %f rot %f", ra, dec, astroRotationAngle));
            double alt, az;
            EquatorialToHorizontal(ra, dec, alt, az, false);
            Debug.AddLine(wxString::Format("Goto accMap: sky.py alt %f az %f", alt, az));
        }
    }

    m_solveTriesRemaining = 3;
    
    Debug.AddLine("Goto: points remaining:");
    for (std::pair<double,double> p : m_pointsToVisit) {
        Debug.AddLine(wxString::Format("Goto: point %f %f", std::get<0>(p), std::get<1>(p)));
    }

    if ( m_pointsToVisit.size() > 0 ) {
        std::pair<double,double> destination = m_pointsToVisit.back();
        m_pointsToVisit.pop_back();
        pMount->HexGoto(std::get<0>(destination), std::get<1>(destination));
        Debug.AddLine(wxString::Format("Goto accMap: Goto %f %f", std::get<0>(destination), std::get<1>(destination)));
        sleep(10);
        pFrame->pGuider->RequestSaveImage();
    } else {
        Debug.AddLine("Goto accMap: finished");
        m_doAccuracyMap = false;
        return;
    }
    */
}

bool GotoDialog::Calibrate() {
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

    std::vector<std::tuple<double, double>> calLocations;
    calLocations.emplace_back(90.0, 0.0);
    calLocations.emplace_back(80.0, 0.0);
    calLocations.emplace_back(80.0, 90.0);
    calLocations.emplace_back(80.0, 180.0);
    calLocations.emplace_back(80.0, 270.0);
    calLocations.emplace_back(70.0, 270.0);
    calLocations.emplace_back(70.0, 180.0);
    calLocations.emplace_back(70.0, 90.0);
    calLocations.emplace_back(70.0, 0.0);
    bool calibrateSuccess = false;

    for (int i = 0; i < calLocations.size(); i++) {
        Debug.AddLine(wxString::Format("Goto: trying location %f %f", get<0>(calLocations[i]), get<1>(calLocations[i])));
        pMount->HexGoto(get<0>(calLocations[i]), get<1>(calLocations[i]));
        sleep(15); // Allow some time for the goto to finish and a clear exposure to happen
        if ( AstroSolveCurrentLocation(startRa, startDec, astroRotationAngle)) {
            Destination::EquatorialToHorizontal(startRa, startDec, startAlt, startAz, true);
            wxString contents = wxString::Format("Astrometry finished! Current location: RA %f, Dec %f\n Alt %f Az %f", startRa, startDec, startAlt, startAz);
            wxMessageDialog * alert = new wxMessageDialog(pFrame, contents, wxString::Format("Goto"), wxOK|wxCENTRE, wxDefaultPosition);
            calibrateSuccess = true;
            break;
            //wxMessageDialog * alert = new wxMessageDialog(pFrame, contents, wxString::Format("Goto"), wxOK|wxCENTRE, wxDefaultPosition);
            //alert->ShowModal();
        } else {
            Debug.AddLine(wxString::Format("Goto: failed to calibrate at %f %f", get<0>(calLocations[i]), get<1>(calLocations[i])));                                                          
        }
    }

    if (not calibrateSuccess) {
        wxString contents = wxString("Unable to work out position with astrometry!\n"
                                         "Please check that the image is in focus, lens cap is off, and no clouds are occluding stars.\n"
                                         "Goto cannot proceed.");
            wxMessageDialog * alert = new wxMessageDialog(pFrame, contents, wxString::Format("Goto"), wxOK|wxCENTRE, wxDefaultPosition);
        alert->ShowModal(); 
        return false;   
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
    Destination::EquatorialToHorizontal(0, 90, northCelestialPoleAlt, northCelestialPoleAz, true);
    
    pMount->HexCalibrate(startAlt, startAz, calDetails.cameraAngle, rotationCenter, astroRotationAngle, northCelestialPoleAlt); // TODO - fill in missing angle
    Debug.AddLine("Goto: Ending onCalibrate");
    calibrated = true;
    return true;
}

void GotoDialog::OnDebug(wxCommandEvent&) {
    // Line up some points to visit
    for (int i = 90; i > 30; i -= 10) {
        m_pointsToVisit.push_back(std::make_pair(i, 0));
    }
    Debug.AddLine(wxString::Format("Goto accMap: Goto 90, 0"));
    pMount->HexGoto(90, 0);
    m_solveTriesRemaining = 3;
    m_doAccuracyMap = true;
}

void GotoDialog::OnCalibrate(wxCommandEvent& )
{
    Calibrate();   
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
    // Goto!
    // --------------------


    //wxMessageDialog * alert = new wxMessageDialog(pFrame, 
    //                                              wxString::Format("Traversing mount to destination:\n"  
    //                                                                "Alt %f, az %f", destination.alt, destination.az), 
    //                                              wxString::Format("Goto"), 
    //                                              wxOK|wxCENTRE, wxDefaultPosition);
    //alert->ShowModal(); 

    gotoInProgress = true;
    Goto();
}

void GotoDialog::Goto() {
    destination.CheckForUpdate();
    pMount->HexGoto(destination.alt, destination.az);
}

bool GotoDialog::AstroSolveCurrentLocation(double &outRa, double &outDec, double &outAstroRotationAngle) {

    // Run astrometry, identify where we are,
    // and pipe the textual results from stdin into pos.ra and pos.dec.
    
    // The result is to call something like "/usr/local/astrometry/bin/solve-field --overwrite /dev/shm/phd2/goto/guide-image.fits"
    char inputFilename[200];
    strcpy(inputFilename, SOLVER_FILENAME);
    // TODO: Detect arcsecperpix ratio from the first astrometry output, instead of hardcoding it here
    strcat(inputFilename, " --overwrite "); //--no-plots --scale-low=6.08 --scale-high=6.14 --scale-units=arcsecperpix --no-fits2fits --fits-image ");
    strcat(inputFilename, IMAGE_FILENAME);
    Debug.AddLine(wxString::Format("inputFilename %s", inputFilename));

    // If simulator is being used as the camera, override with test input image
    if (dynamic_cast<Camera_SimClass*>(pCamera)) {
        pFrame->Alert(wxString::Format("Since the current camera is the simulator, will use fake test image to determine position."));
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
    regex angleReg("-?[[:digit:]]+\\.?[[:digit:]]+");
    smatch angleMatch;
    regex_search(line2.begin(), line2.end(), angleMatch, angleReg);
    outAstroRotationAngle = stod(angleMatch[0]);

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
    pFrame->pGuider->InvalidateSavedImage();

}
