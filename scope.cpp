/*
 *  scope.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
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
 *    Neither the name of Craig Stark, Stark Labs nor the names of its
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

#include "backlash_comp.h"
#include "calreview_dialog.h"
#include "calstep_dialog.h"
#include "image_math.h"
#include "socket_server.h"

#include <wx/textfile.h>

#include <cmath>
#define PI 3.14159265


static const int DefaultCalibrationDuration = 750;
static const int DefaultMaxDecDuration = 2500;
static const int DefaultMaxRaDuration = 2500;
enum { MAX_DURATION_MIN = 50, MAX_DURATION_MAX = 8000, };

static const DEC_GUIDE_MODE DefaultDecGuideMode = DEC_AUTO;
static const GUIDE_ALGORITHM DefaultRaGuideAlgorithm = GUIDE_ALGORITHM_HYSTERESIS;
static const GUIDE_ALGORITHM DefaultDecGuideAlgorithm = GUIDE_ALGORITHM_RESIST_SWITCH;

static const double DEC_BACKLASH_DISTANCE = 3.0;
static const int MAX_CALIBRATION_STEPS = 60;
static const double MAX_CALIBRATION_DISTANCE = 25.0;
static const int CAL_ALERT_MINSTEPS = 4;
static const double CAL_ALERT_ORTHOGONALITY_TOLERANCE = 12.5;               // Degrees
static const double CAL_ALERT_DECRATE_DIFFERENCE = 0.20;                    // Ratio tolerance
static const double CAL_ALERT_AXISRATES_TOLERANCE = 0.20;                   // Ratio tolerance
static const bool SANITY_CHECKING_ACTIVE = true;                            // Control calibration sanity checking

static int LIMIT_REACHED_WARN_COUNT = 5;
static int MAX_NUDGES = 3;
static double NUDGE_TOLERANCE = 2.0;

// enable dec compensation when calibration declination is less than this
const double Scope::DEC_COMP_LIMIT = M_PI / 2.0 * 2.0 / 3.0;   // 60 degrees

Scope::Scope(void)
    : m_raLimitReachedDirection(NONE),
      m_raLimitReachedCount(0),
      m_decLimitReachedDirection(NONE),
      m_decLimitReachedCount(0)
{
    m_calibrationSteps = 0;
    m_graphControlPane = NULL;

    wxString prefix = "/" + GetMountClassName();
    int calibrationDuration = pConfig->Profile.GetInt(prefix + "/CalibrationDuration", DefaultCalibrationDuration);
    SetCalibrationDuration(calibrationDuration);

    int maxRaDuration  = pConfig->Profile.GetInt(prefix + "/MaxRaDuration", DefaultMaxRaDuration);
    SetMaxRaDuration(maxRaDuration);

    int maxDecDuration = pConfig->Profile.GetInt(prefix + "/MaxDecDuration", DefaultMaxDecDuration);
    SetMaxDecDuration(maxDecDuration);

    int decGuideMode = pConfig->Profile.GetInt(prefix + "/DecGuideMode", DefaultDecGuideMode);
    SetDecGuideMode(decGuideMode);

    int raGuideAlgorithm = pConfig->Profile.GetInt(prefix + "/XGuideAlgorithm", DefaultRaGuideAlgorithm);
    SetXGuideAlgorithm(raGuideAlgorithm);

    int decGuideAlgorithm = pConfig->Profile.GetInt(prefix + "/YGuideAlgorithm", DefaultDecGuideAlgorithm);
    SetYGuideAlgorithm(decGuideAlgorithm);

    bool val = pConfig->Profile.GetBoolean(prefix + "/CalFlipRequiresDecFlip", false);
    SetCalibrationFlipRequiresDecFlip(val);

    val = pConfig->Profile.GetBoolean(prefix + "/AssumeOrthogonal", false);
    SetAssumeOrthogonal(val);

    val = pConfig->Profile.GetBoolean(prefix + "/UseDecComp", true);
    EnableDecCompensation(val);

    m_backlashComp = new BacklashComp(this);
}

Scope::~Scope(void)
{
    if (m_graphControlPane)
    {
        m_graphControlPane->m_pScope = NULL;
    }
}

bool Scope::SetCalibrationDuration(int calibrationDuration)
{
    bool bError = false;

    try
    {
        if (calibrationDuration <= 0)
        {
            throw ERROR_INFO("invalid calibrationDuration");
        }

        m_calibrationDuration = calibrationDuration;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_calibrationDuration = DefaultCalibrationDuration;
    }

    pConfig->Profile.SetInt("/scope/CalibrationDuration", m_calibrationDuration);

    return bError;
}

bool Scope::SetMaxDecDuration(int maxDecDuration)
{
    bool bError = false;

    try
    {
        if (maxDecDuration < 0)
        {
            throw ERROR_INFO("maxDecDuration < 0");
        }

        if (m_maxDecDuration != maxDecDuration)
            GuideLog.SetGuidingParam("Dec Max Duration", maxDecDuration);

        m_maxDecDuration = maxDecDuration;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_maxDecDuration = DefaultMaxDecDuration;
    }

    pConfig->Profile.SetInt("/scope/MaxDecDuration", m_maxDecDuration);

    return bError;
}

bool Scope::SetMaxRaDuration(double maxRaDuration)
{
    bool bError = false;

    try
    {
        if (maxRaDuration < 0)
        {
            throw ERROR_INFO("maxRaDuration < 0");
        }

        if (m_maxRaDuration != maxRaDuration)
            GuideLog.SetGuidingParam("RA Max Duration", maxRaDuration);
        m_maxRaDuration =  maxRaDuration;

    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_maxRaDuration = DefaultMaxRaDuration;
    }

    pConfig->Profile.SetInt("/scope/MaxRaDuration", m_maxRaDuration);

    return bError;
}

bool Scope::SetDecGuideMode(int decGuideMode)
{
    bool bError = false;

    try
    {
        switch (decGuideMode)
        {
            case DEC_NONE:
            case DEC_AUTO:
            case DEC_NORTH:
            case DEC_SOUTH:
                break;
            default:
                throw ERROR_INFO("invalid decGuideMode");
                break;
        }

        if (m_decGuideMode != decGuideMode)
        {
            const char *dec_modes[] = {
              "Off", "Auto", "North", "South"
            };

            Debug.Write(wxString::Format("DecGuideMode set to %s (%d)\n", dec_modes[decGuideMode], decGuideMode));
            GuideLog.SetGuidingParam("Dec Guide Mode", dec_modes[decGuideMode]);

            m_decGuideMode = (DEC_GUIDE_MODE) decGuideMode;

            pConfig->Profile.SetInt("/scope/DecGuideMode", m_decGuideMode);
            if (pFrame)
                pFrame->UpdateCalibrationStatus();
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

static int CompareNoCase(const wxString& first, const wxString& second)
{
    return first.CmpNoCase(second);
}

wxArrayString Scope::List(void)
{
    wxArrayString ScopeList;

    ScopeList.Add(_("None"));
#ifdef GUIDE_ASCOM
    wxArrayString ascomScopes = ScopeASCOM::EnumAscomScopes();
    for (unsigned int i = 0; i < ascomScopes.Count(); i++)
        ScopeList.Add(ascomScopes[i]);
#endif
#ifdef GUIDE_ONCAMERA
    ScopeList.Add(_T("On-camera"));
#endif
#ifdef GUIDE_ONSTEPGUIDER
    ScopeList.Add(_T("On-AO"));
#endif
#ifdef GUIDE_GPUSB
    ScopeList.Add(_T("GPUSB"));
#endif
#ifdef GUIDE_GPINT
    ScopeList.Add(_T("GPINT 3BC"));
    ScopeList.Add(_T("GPINT 378"));
    ScopeList.Add(_T("GPINT 278"));
#endif
#ifdef GUIDE_VOYAGER
    ScopeList.Add(_T("Voyager"));
#endif
#ifdef GUIDE_EQUINOX
    ScopeList.Add(_T("Equinox 6"));
    ScopeList.Add(_T("EQMAC"));
#endif
#ifdef GUIDE_GCUSBST4
    ScopeList.Add(_T("GC USB ST4"));
#endif
#ifdef GUIDE_INDI
    ScopeList.Add(_("INDI Mount"));
#endif

    ScopeList.Sort(&CompareNoCase);

    return ScopeList;
}

wxArrayString Scope::AuxMountList()
{
    wxArrayString scopeList;
    scopeList.Add(_("None"));      // Keep this at the top of the list

#ifdef GUIDE_ASCOM
    wxArrayString positionAwareScopes = ScopeASCOM::EnumAscomScopes();
    positionAwareScopes.Sort(&CompareNoCase);
    for (unsigned int i = 0; i < positionAwareScopes.Count(); i++)
        scopeList.Add(positionAwareScopes[i]);
#endif

#ifdef GUIDE_INDI
    scopeList.Add(_("INDI Mount"));
#endif

    scopeList.Add(ScopeManualPointing::GetDisplayName());

    return scopeList;
}

Scope *Scope::Factory(const wxString& choice)
{
    Scope *pReturn = NULL;

    try
    {
        if (choice.IsEmpty())
        {
            throw ERROR_INFO("ScopeFactory called with choice.IsEmpty()");
        }

        Debug.AddLine(wxString::Format("ScopeFactory(%s)", choice));

        if (false) // so else ifs can follow
        {
        }
#ifdef GUIDE_ASCOM
        // do ASCOM first since it includes choices that could match stings belop like Simulator
        else if (choice.Find(_T("ASCOM")) != wxNOT_FOUND) {
            pReturn = new ScopeASCOM(choice);
        }
#endif
        else if (choice.Find(_("None")) + 1) {
        }
#ifdef GUIDE_ONCAMERA
        else if (choice.Find(_T("On-camera")) + 1) {
            pReturn = new ScopeOnCamera();
        }
#endif
#ifdef GUIDE_ONSTEPGUIDER
        else if (choice.Find(_T("On-AO")) + 1) {
            pReturn = new ScopeOnStepGuider();
        }
#endif
#ifdef GUIDE_GPUSB
        else if (choice.Find(_T("GPUSB")) + 1) {
            pReturn = new ScopeGpUsb();
        }
#endif
#ifdef GUIDE_GPINT
        else if (choice.Find(_T("GPINT 3BC")) + 1) {
            pReturn = new ScopeGpInt((short) 0x3BC);
        }
        else if (choice.Find(_T("GPINT 378")) + 1) {
            pReturn = new ScopeGpInt((short) 0x378);
        }
        else if (choice.Find(_T("GPINT 278")) + 1) {
            pReturn = new ScopeGpInt((short) 0x278);
        }
#endif
#ifdef GUIDE_VOYAGER
        else if (choice.Find(_T("Voyager")) + 1) {
            This needs work.  We have to move the setting of the IP address
                into the connect routine
            ScopeVoyager *pVoyager = new ScopeVoyager();
        }
#endif
#ifdef GUIDE_EQUINOX
        else if (choice.Find(_T("Equinox 6")) + 1) {
            pReturn = new ScopeEquinox();
        }
#endif
#ifdef GUIDE_EQMAC
        else if (choice.Find(_T("EQMAC")) + 1) {
            pReturn = new ScopeEQMac();
        }
#endif
#ifdef GUIDE_GCUSBST4
        else if (choice.Find(_T("GC USB ST4")) + 1) {
            pReturn = new ScopeGCUSBST4();
        }
#endif
#ifdef GUIDE_INDI
        else if (choice.Find(_T("INDI")) + 1) {
            pReturn = new ScopeINDI();
        }
#endif
        else if (choice.Find(ScopeManualPointing::GetDisplayName()) != wxNOT_FOUND) {
            pReturn = new ScopeManualPointing();
        }
        else {
            throw ERROR_INFO("ScopeFactory: Unknown Scope choice");
        }

        if (pReturn)
        {
            // virtual function call means we cannot do this in the Scope constructor
            pReturn->EnableStopGuidingWhenSlewing(pConfig->Profile.GetBoolean("/scope/StopGuidingWhenSlewing",
                pReturn->CanCheckSlewing()));
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        if (pReturn)
        {
            delete pReturn;
            pReturn = NULL;
        }
    }

    return pReturn;
}

bool Scope::RequiresCamera(void)
{
    return false;
}

bool Scope::RequiresStepGuider(void)
{
    return false;
}

bool Scope::CalibrationFlipRequiresDecFlip(void)
{
    return m_calibrationFlipRequiresDecFlip;
}

void Scope::SetCalibrationFlipRequiresDecFlip(bool val)
{
    m_calibrationFlipRequiresDecFlip = val;
    pConfig->Profile.SetBoolean("/scope/CalFlipRequiresDecFlip", val);
}

void Scope::SetAssumeOrthogonal(bool val)
{
    m_assumeOrthogonal = val;
    pConfig->Profile.SetBoolean("/scope/AssumeOrthogonal", val);
}

void Scope::EnableStopGuidingWhenSlewing(bool enable)
{
    if (enable)
        Debug.AddLine("Scope: enabling slew check, guiding will stop when slew is detected");
    else
        Debug.AddLine("Scope: slew check disabled");

    pConfig->Profile.SetBoolean("/scope/StopGuidingWhenSlewing", enable);
    m_stopGuidingWhenSlewing = enable;
}

void Scope::StartDecDrift(void)
{
    m_saveDecGuideMode = m_decGuideMode;
    m_decGuideMode = DEC_NONE;
    if (m_graphControlPane)
    {
        m_graphControlPane->m_pDecMode->SetSelection(DEC_NONE);
        m_graphControlPane->m_pDecMode->Enable(false);
    }
}

void Scope::EndDecDrift(void)
{
    m_decGuideMode = m_saveDecGuideMode;
    if (m_graphControlPane)
    {
        m_graphControlPane->m_pDecMode->SetSelection(m_decGuideMode);
        m_graphControlPane->m_pDecMode->Enable(true);
    }
}

bool Scope::IsDecDrifting(void) const
{
    return m_decGuideMode == DEC_NONE;
}

// Useful utility functions
#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))

Mount::MOVE_RESULT Scope::CalibrationMove(GUIDE_DIRECTION direction, int duration)
{
    MOVE_RESULT result = MOVE_OK;

    Debug.AddLine(wxString::Format("scope calibration move dir= %d dur= %d", direction, duration));

    try
    {
        MoveResultInfo move;
        result = Move(direction, duration, MOVETYPE_DIRECT, &move);

        if (result != MOVE_OK)
        {
            throw THROW_INFO("Move failed");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    return result;
}

int Scope::CalibrationMoveSize(void)
{
    return m_calibrationDuration;
}

static wxString LimitReachedWarningKey(long axis)
{
    // we want the key to be under "/Confirm" so ConfirmDialog::ResetAllDontAskAgain() resets it, but we also want the setting to be per-profile
    return wxString::Format("/Confirm/%d/Max%sLimitWarningEnabled", pConfig->GetCurrentProfileId(), axis == GUIDE_RA ? "RA" : "Dec");
}

static void SuppressLimitReachedWarning(long axis)
{
    pConfig->Global.SetBoolean(LimitReachedWarningKey(axis), false);
}

void Scope::AlertLimitReached(int duration, GuideAxis axis)
{
    static time_t s_lastLogged;
    time_t now = wxDateTime::GetTimeNow();
    if (s_lastLogged == 0 || now - s_lastLogged > 30)
    {
        s_lastLogged = now;
        if (duration < MAX_DURATION_MAX)
        {
            wxString s = axis == GUIDE_RA ? _("Max RA Duration setting") : _("Max Dec Duration setting");
            pFrame->SuppressableAlert(LimitReachedWarningKey(axis),
                wxString::Format(_("Your %s is preventing PHD from making adequate corrections to keep the guide star locked. "
                "Increase the %s to allow PHD2 to make the needed corrections."), s, s),
                SuppressLimitReachedWarning, axis, false, wxICON_INFORMATION);
        }
        else
        {
            wxString which_axis = axis == GUIDE_RA ? _("RA") : _("Dec");
            pFrame->SuppressableAlert(LimitReachedWarningKey(axis),
                wxString::Format(_("Even using the maximum moves, PHD2 can't properly correct for the large guide star movements in %s. "
                "Guiding will be impaired until you can eliminate the source of these problems."), which_axis),
                SuppressLimitReachedWarning, axis, false, wxICON_INFORMATION);
        }
    }
}

Mount::MOVE_RESULT Scope::Move(GUIDE_DIRECTION direction, int duration, MountMoveType moveType, MoveResultInfo *moveResult)
{
    MOVE_RESULT result = MOVE_OK;
    bool limitReached = false;

    Debug.Write(wxString::Format("desh: Scope move(%d, %d, %d)\n", direction, duration, moveType));
    PHD_Point movePoint(2,2);
    pMount->HexMove(movePoint, 0);
    
    try
    {
        if (!m_guidingEnabled)
        {
            throw THROW_INFO("Guiding disabled");
        }

        // Compute the actual guide durations

        switch (direction)
        {
            case NORTH:
            case SOUTH:

                // Do not enforce dec guiding mode and max dec duration for direct moves
                if (moveType != MOVETYPE_DIRECT)
                {
                    if ((m_decGuideMode == DEC_NONE) ||
                        (direction == SOUTH && m_decGuideMode == DEC_NORTH) ||
                        (direction == NORTH && m_decGuideMode == DEC_SOUTH))
                    {
                        duration = 0;
                        Debug.AddLine("duration set to 0 by GuideMode");
                    }

                    if (duration > m_maxDecDuration)
                    {
                        duration = m_maxDecDuration;
                        Debug.Write(wxString::Format("duration set to %d by maxDecDuration\n", duration));
                        limitReached = true;
                    }

                    if (limitReached && direction == m_decLimitReachedDirection)
                    {
                        if (++m_decLimitReachedCount >= LIMIT_REACHED_WARN_COUNT)
                            AlertLimitReached(duration, GUIDE_DEC);
                    }
                    else
                        m_decLimitReachedCount = 0;

                    if (limitReached)
                        m_decLimitReachedDirection = direction;
                    else
                        m_decLimitReachedDirection = NONE;
                }
                break;
            case EAST:
            case WEST:

                // Do not enforce max dec duration for direct moves
                if (moveType != MOVETYPE_DIRECT)
                {
                    if (duration > m_maxRaDuration)
                    {
                        duration = m_maxRaDuration;
                        Debug.Write(wxString::Format("duration set to %d by maxRaDuration\n", duration));
                        limitReached = true;
                    }

                    if (limitReached && direction == m_raLimitReachedDirection)
                    {
                        if (++m_raLimitReachedCount >= LIMIT_REACHED_WARN_COUNT)
                            AlertLimitReached(duration, GUIDE_RA);
                    }
                    else
                        m_raLimitReachedCount = 0;

                    if (limitReached)
                        m_raLimitReachedDirection = direction;
                    else
                        m_raLimitReachedDirection = NONE;
                }
                break;

            case NONE:
                break;
        }

        // Actually do the guide
        //assert(duration >= 0);
        duration = 1; // Placeholder, none of this is used anymore
        if (duration > 0)
        {
            result = Guide(direction, duration);
            if (result != MOVE_OK)
            {
                throw ERROR_INFO("guide failed");
            }
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        if (result == MOVE_OK)
            result = MOVE_ERROR;
        duration = 0;
    }

    Debug.Write(wxString::Format("Move returns status %d, amount %d\n", result, duration));

    if (moveResult)
    {
        moveResult->amountMoved = duration;
        moveResult->limited = limitReached;
    }
    
    return result;
}

static wxString CalibrationWarningKey(CalibrationIssueType etype)
{
    wxString qual;
    switch (etype)
    {
    case CI_Angle:
        qual = "Angle";
        break;
    case CI_Different:
        qual = "Diff";
        break;
    case CI_Steps:
        qual = "Steps";
        break;
    case CI_Rates:
        qual = "Rates";
        break;
    case CI_None:
        qual = "Bogus";
        break;

    }
    wxString rtn = wxString::Format("/Confirm/%d/CalWarning_%s", pConfig->GetCurrentProfileId(), qual);
    return rtn;
}

void Scope::SetCalibrationWarning(CalibrationIssueType etype, bool val)
{
    pConfig->Global.SetBoolean(CalibrationWarningKey(etype), val);
}

// Generic hook for "details" button in calibration sanity check alert
static void ShowCalibrationIssues(long scopeptr)
{
    Scope *pscope = reinterpret_cast<Scope *>(scopeptr);
    pscope->HandleSanityCheckDialog();
}

// Handle the "details" dialog for the calibration sanity check
void Scope::HandleSanityCheckDialog()
{
    if (pFrame->pCalSanityCheckDlg)
        pFrame->pCalSanityCheckDlg->Destroy();

    pFrame->pCalSanityCheckDlg = new CalSanityDialog(pFrame, m_prevCalibration, m_prevCalibrationDetails, m_lastCalibrationIssue);
    pFrame->pCalSanityCheckDlg->Show();
}

// Do some basic sanity checking on the just-completed calibration, looking for things that are fishy.  Do the checking in the order of
// importance/confidence, since we only alert about a single condition
void Scope::SanityCheckCalibration(const Calibration& oldCal, const CalibrationDetails& oldDetails)
{
    Calibration newCal;
    GetLastCalibration(&newCal);

    CalibrationDetails newDetails;
    GetCalibrationDetails(&newDetails);

    m_lastCalibrationIssue = CI_None;
    int xSteps = newDetails.raStepCount;
    int ySteps = newDetails.decStepCount;

    wxString detailInfo;

    // Too few steps
    if (xSteps < CAL_ALERT_MINSTEPS || (ySteps < CAL_ALERT_MINSTEPS && ySteps > 0))            // Dec guiding might be disabled
    {
        m_lastCalibrationIssue = CI_Steps;
        detailInfo = wxString::Format("Actual RA calibration steps = %d, Dec calibration steps = %d", xSteps, ySteps);
    }
    else
    {
        // Non-orthogonal RA/Dec axes
        double nonOrtho = degrees(fabs(fabs(norm_angle(newCal.xAngle - newCal.yAngle)) - M_PI / 2.));         // Delta from the nearest multiple of 90 degrees
        if (nonOrtho >  CAL_ALERT_ORTHOGONALITY_TOLERANCE)
        {
            m_lastCalibrationIssue = CI_Angle;
            detailInfo = wxString::Format("Non-orthogonality = %0.3f", nonOrtho);
        }
        else
        {
            // RA/Dec rates should be related by cos(dec) but don't check if Dec is too high or Dec guiding is disabled
            if (newCal.declination != UNKNOWN_DECLINATION && newCal.yRate != CALIBRATION_RATE_UNCALIBRATED && fabs(newCal.declination) <= Scope::DEC_COMP_LIMIT)
            {
                double expectedRatio = cos(newCal.declination);
                double speedRatio;
                if (newDetails.raGuideSpeed > 0.)                   // for mounts that may have different guide speeds on RA and Dec axes
                    speedRatio = newDetails.decGuideSpeed / newDetails.raGuideSpeed;
                else
                    speedRatio = 1.0;
                double actualRatio = newCal.xRate * speedRatio / newCal.yRate;
                if (fabs(expectedRatio - actualRatio) > CAL_ALERT_AXISRATES_TOLERANCE)
                {
                    m_lastCalibrationIssue = CI_Rates;
                    detailInfo = wxString::Format("Expected ratio at dec=%0.1f is %0.3f, actual is %0.3f", degrees(newCal.declination), expectedRatio, actualRatio);
                }
            }
        }

        // Finally check for a significantly different result but don't be stupid - ignore differences if the configuration looks quite different
        // Can't do straight equality checks because of rounding - the "old" values have passed through the registry get/set routines
        if (m_lastCalibrationIssue == CI_None && oldCal.isValid &&
            fabs(oldDetails.imageScale - newDetails.imageScale) < 0.1 && (fabs(degrees(oldCal.xAngle - newCal.xAngle)) < 5.0))
        {
            double newDecRate = newCal.yRate;
            if (newDecRate != 0.)
            {
                if (fabs(1.0 - (oldCal.yRate / newDecRate)) > CAL_ALERT_DECRATE_DIFFERENCE)
                {
                    m_lastCalibrationIssue = CI_Different;
                    detailInfo = wxString::Format("Current/previous Dec rate ratio is %0.3f", oldCal.yRate / newDecRate);
                }
            }
            else
                if (oldCal.yRate != 0.)                   // Might have had Dec guiding disabled
                    m_lastCalibrationIssue = CI_Different;
        }
    }

    if (m_lastCalibrationIssue != CI_None)
    {
        wxString alertMsg;

        FlagCalibrationIssue(newDetails, m_lastCalibrationIssue);
        switch (m_lastCalibrationIssue)
        {
        case CI_Steps:
            alertMsg = _("Calibration is based on very few steps, so accuracy is questionable");
            break;
        case CI_Angle:
            alertMsg = _("Calibration computed RA/Dec axis angles that are questionable");
            break;
        case CI_Different:
            alertMsg = _("This calibration is substantially different from the previous one - have you changed configurations?");
            break;
        case CI_Rates:
            alertMsg = _("The RA and Dec rates vary by an unexpected amount");
        default:
            break;
        }

        // Suppression of calibration alerts is handled in the 'Details' dialog - a special case
        if (pConfig->Global.GetBoolean(CalibrationWarningKey(m_lastCalibrationIssue), true))        // User hasn't disabled this type of alert
        {
            // Generate alert with 'Help' button that will lead to trouble-shooting section
            pFrame->Alert(alertMsg, 0,
                _("Details..."), ShowCalibrationIssues, (long)this, true);
        }
        else
        {
            Debug.AddLine(wxString::Format("Alert detected in scope calibration but not shown to user - suppressed message was: %s", alertMsg));
        }

        Debug.AddLine(wxString::Format("Calibration alert details: %s", detailInfo));
    }
    else
    {
        Debug.AddLine("Calibration passed sanity checks...");
    }
}

void Scope::ClearCalibration(void)
{
    Mount::ClearCalibration();

    m_calibrationState = CALIBRATION_STATE_CLEARED;
}

bool Scope::BeginCalibration(const PHD_Point& currentLocation)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("Not connected");
        }

        if (!currentLocation.IsValid())
        {
            throw ERROR_INFO("Must have a valid lock position");
        }

        ClearCalibration();
        m_calibrationStepsRemaining = M_INITIAL_CALIBRATION_STEPS;
        m_calibrationSteps = 0;
        m_calibrationInitialLocation = currentLocation;
        m_calibrationStartingLocation.Invalidate();
        m_calibrationState = CALIBRATION_STATE_GO_NORTH;
        m_calibrationDetails.raSteps.clear();
        m_calibrationDetails.decSteps.clear();
        m_calibrationDetails.lastIssue = CI_None;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void Scope::SetCalibration(const Calibration& cal)
{
    m_calibration = cal;
    Mount::SetCalibration(cal);
}

void Scope::SetCalibrationDetails(const CalibrationDetails& calDetails, double xAngle, double yAngle, double binning)
{
    m_calibrationDetails = calDetails;
    double ra_rate;
    double dec_rate;
    if (pPointingSource->GetGuideRates(&ra_rate, &dec_rate))        // true means error
    {
        ra_rate = -1.0;
        dec_rate = -1.0;
    }
    m_calibrationDetails.raGuideSpeed = ra_rate;
    m_calibrationDetails.decGuideSpeed = dec_rate;
    m_calibrationDetails.focalLength = pFrame->GetFocalLength();
    m_calibrationDetails.imageScale = pFrame->GetCameraPixelScale();
    m_calibrationDetails.orthoError = degrees(fabs(fabs(norm_angle(xAngle - yAngle)) - M_PI / 2.));         // Delta from the nearest multiple of 90 degrees
    m_calibrationDetails.origBinning = binning;
    m_calibrationDetails.origTimestamp = wxDateTime::Now().Format();
    Mount::SetCalibrationDetails(m_calibrationDetails);
}

void Scope::FlagCalibrationIssue(const CalibrationDetails& calDetails, CalibrationIssueType issue)
{
    m_calibrationDetails = calDetails;
    m_calibrationDetails.lastIssue = issue;
    Mount::SetCalibrationDetails(m_calibrationDetails);
}

bool Scope::IsCalibrated(void)
{
    if (!Mount::IsCalibrated())
        return false;

    switch (m_decGuideMode)
    {
    case DEC_NONE:
        return true;
    case DEC_AUTO:
    case DEC_NORTH:
    case DEC_SOUTH:
        {
            bool have_ns_calibration = m_calibration.yRate != CALIBRATION_RATE_UNCALIBRATED;
            return have_ns_calibration;
        }
    default:
        assert(false);
        return true;
    }
}

void Scope::EnableDecCompensation(bool enable)
{
    m_useDecCompensation = enable;
    wxString prefix = "/" + GetMountClassName();
    pConfig->Profile.SetBoolean(prefix + "/UseDecComp", enable);
}

bool Scope::DecCompensationActive(void) const
{
    return DecCompensationEnabled() &&
        MountCal().declination != UNKNOWN_DECLINATION &&
        pPointingSource && pPointingSource->IsConnected() && pPointingSource->CanReportPosition();
}

static double CalibrationDistance(void)
{
    return wxMin(pCamera->FullSize.GetHeight() * 0.05, MAX_CALIBRATION_DISTANCE);
}

int Scope::CalibrationTotDistance(void)
{
    return (int) ceil(CalibrationDistance());
}

// Convert camera coords to mount coords
static PHD_Point MountCoords(const PHD_Point& cameraVector, double xCalibAngle, double yCalibAngle)
{
    double hyp = cameraVector.Distance();
    double cameraTheta = cameraVector.Angle();
    double yAngleError = norm_angle((xCalibAngle - yCalibAngle) + M_PI / 2);
    double xAngle = cameraTheta - xCalibAngle;
    double yAngle = cameraTheta - (xCalibAngle + yAngleError);
    return PHD_Point(hyp * cos(xAngle), hyp * sin(yAngle));
}

static void GetRADecCoordinates(PHD_Point *coords)
{
    double ra, dec, lst;
    bool err = pPointingSource->GetCoordinates(&ra, &dec, &lst);
    if (err)
        coords->Invalidate();
    else
        coords->SetXY(ra, dec);
}

bool Scope::UpdateCalibrationState(const PHD_Point& currentLocation)
{
    // New calibration works like this.
    // 1. We start at m_calibrationStartingLocation.
    // 2. We try to move the mount north. This is captured as m_calibrationNorthLocation.
    // 3. We try to move the mount back south by the same amount. (m_calibrationNorthReturnLocation.)
    //    Due to sky movement, this will likely be different to 1. 
    // 4. We calculate the midpoint between points 1 and 3.
    //    
    // Then we get the angle from 4 to 2 compared to the horizontal.
    // This should give us the rotation of the camera with respect to the mount.
    //       
    //         m_calibrationNorthLocation -> 2 
    //                                      /| \
    //                                     / |  \
    //                                    /  |   \
    //                                   /   |    \
    // m_calibrationStartingLocation -> 1____|_____3 <- m_calibrationNorthReturnLocation;
    //                                       ^4, midpoint


    bool bError = false;

    try
    {
        if (!m_calibrationStartingLocation.IsValid())
        {
            m_calibrationStartingLocation = currentLocation;
            GetRADecCoordinates(&m_calibrationStartingCoords);

            Debug.Write(wxString::Format("Scope::UpdateCalibrationstate: starting location = %.2f,%.2f coords = %s\n",
                currentLocation.X, currentLocation.Y,
                m_calibrationStartingCoords.IsValid() ?
                    wxString::Format("%.2f,%.1f", m_calibrationStartingCoords.X, m_calibrationStartingCoords.Y) : wxString("N/A")));

        }

        double dX = m_calibrationStartingLocation.dX(currentLocation);
        double dY = m_calibrationStartingLocation.dY(currentLocation);
        double dist = m_calibrationStartingLocation.Distance(currentLocation);
        double dist_crit = CalibrationDistance();
        double blDelta;
        double blCumDelta;
        double nudge_amt;
        double nudgeDirCosX;
        double nudgeDirCosY;
        double cos_theta;
        double theta;

        switch (m_calibrationState)
        {
            case CALIBRATION_STATE_CLEARED:
                assert(false);
                break;

            case CALIBRATION_STATE_GO_NORTH:

                GuideLog.CalibrationStep(this, "North", m_calibrationSteps, dX, dY, currentLocation, dist);
                m_calibrationDetails.decSteps.push_back(wxRealPoint(dX, dY));

                if (m_calibrationStepsRemaining > 0) {
                    //status0.Printf(_("North step %3d"), m_calibrationSteps);
                    Debug.AddLine(wxString::Format("desh: moving north, cal steps remaining %d, m_calibrationDuration %d", m_calibrationStepsRemaining, m_calibrationDuration));
                    Debug.AddLine(wxString::Format("desh: dX %f dY %f", dX, dY));
                    m_calibrationStepsRemaining -= 1;
                    pFrame->ScheduleCalibrationMove(this, NORTH, m_calibrationDuration);
                    break;
                }

                m_calibrationStepsRemaining = M_INITIAL_CALIBRATION_STEPS;
                m_calibrationNorthLocation = PHD_Point(dX, dY);

                m_calibration.decGuideParity = GUIDE_PARITY_UNKNOWN;
                if (m_calibrationStartingCoords.IsValid())
                {
                    PHD_Point endingCoords;
                    GetRADecCoordinates(&endingCoords);
                    if (endingCoords.IsValid())
                    {
                        // real Northward motion increases Dec
                        double ONE_ARCSEC = 1.0 / (60. * 60.); // degrees
                        double ddec = endingCoords.Y - m_calibrationStartingCoords.Y;
                        if (ddec > ONE_ARCSEC)
                            m_calibration.decGuideParity = GUIDE_PARITY_EVEN;
                        else if (ddec < -ONE_ARCSEC)
                            m_calibration.decGuideParity = GUIDE_PARITY_ODD;
                    }
                }

                Debug.AddLine(wxString::Format("NORTH calibration completes with angle=%.1f rate=%.3f parity=%d",
                    degrees(m_calibration.yAngle), m_calibration.yRate * 1000.0, m_calibration.decGuideParity));

                GuideLog.CalibrationDirectComplete(this, "North", m_calibration.yAngle, m_calibration.yRate, m_calibration.decGuideParity);

                m_calibrationState = CALIBRATION_STATE_RETURN_FROM_NORTH;
                //m_southStartingLocation = currentLocation;

                // fall through
                Debug.AddLine("desh: Falling Through to state RETURN_FROM_NORTH");

            case CALIBRATION_STATE_RETURN_FROM_NORTH:

                GuideLog.CalibrationStep(this, "South", m_calibrationSteps, dX, dY, currentLocation, dist);
                m_calibrationDetails.decSteps.push_back(wxRealPoint(dX, dY));

                if (m_calibrationStepsRemaining > 0) {
                    //status0.Printf(_("South step %3d"), m_calibrationSteps);
                    Debug.AddLine(wxString::Format("desh: moving south, cal steps remaining %d, m_calibrationDuration %d", m_calibrationStepsRemaining, m_calibrationDuration));
                    Debug.AddLine(wxString::Format("desh: dX %f dY %f", dX, dY));
                    m_calibrationStepsRemaining -= 1;
                    pFrame->ScheduleCalibrationMove(this, SOUTH, m_calibrationDuration);
                    break;
                }

                m_calibrationNorthReturnLocation = PHD_Point(dX, dY);

                m_calibrationState = CALIBRATION_STATE_COMPLETE;
                m_calibrationSteps = 0;

            case CALIBRATION_STATE_COMPLETE:

                // Work out camera correction angle 
                Debug.AddLine("desh: Calibration complete.");
                Debug.AddLine(wxString::Format("desh: dX %f dY %f", dX, dY));
                Debug.AddLine(wxString::Format("desh: North - x %f y %f", m_calibrationNorthLocation.X, m_calibrationNorthLocation.Y));
                Debug.AddLine(wxString::Format("desh: North return - x %f y %f", m_calibrationNorthReturnLocation.X, m_calibrationNorthReturnLocation.Y));
                
                // Because the starting location is defined as 0,0 , the average of that and the 
                // final location is just final location / 2.
                double midPointX = m_calibrationNorthReturnLocation.X / 2;
                double midPointY = m_calibrationNorthReturnLocation.Y / 2;

                double deltaX = m_calibrationNorthLocation.X - midPointX;
                double deltaY = m_calibrationNorthLocation.Y - midPointY;
                double cameraAngle = std::atan2(deltaX, deltaY) * 180 / PI;
                cameraAngle -= 180;
                if ( cameraAngle < -360 ) { 
                    cameraAngle += 360; 
                } else if ( cameraAngle > 360 ) {
                    cameraAngle -= 360;
                }
                cameraAngle *= -1;
                
                Debug.AddLine(wxString::Format("desh: angle %f", cameraAngle));

                GetLastCalibration(&m_prevCalibration);
                GetCalibrationDetails(&m_prevCalibrationDetails);
                Calibration cal(m_calibration);
                cal.declination = pPointingSource->GetDeclination();
                cal.pierSide = pPointingSource->SideOfPier();
                cal.rotatorAngle = Rotator::RotatorPosition();
                cal.binning = pCamera->Binning;
                SetCalibration(cal);
                m_calibrationDetails.raStepCount = m_raSteps;
                m_calibrationDetails.decStepCount = m_decSteps;
                m_calibrationDetails.cameraAngle = cameraAngle;
                SetCalibrationDetails(m_calibrationDetails, m_calibration.xAngle, m_calibration.yAngle, pCamera->Binning);
                if (SANITY_CHECKING_ACTIVE)
                    SanityCheckCalibration(m_prevCalibration, m_prevCalibrationDetails);  // method gets "new" info itself
                pFrame->StatusMsg(_("Calibration complete"));
                GuideLog.CalibrationComplete(this);
                EvtServer.NotifyCalibrationComplete(this);
                Debug.AddLine("Calibration Complete");
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);

        ClearCalibration();

        bError = true;
    }

    return bError;
}

// Get a value of declination, in radians, that can be used for adjusting the RA guide rate,
// or UNKNOWN_DECLINATION if the declination is not known.
double Scope::GetDeclination(void)
{
    return UNKNOWN_DECLINATION;
}

// Baseline implementations for non-ASCOM subclasses.  Methods will
// return a sensible default or an error (true)
bool Scope::GetGuideRates(double *pRAGuideRate, double *pDecGuideRate)
{
    return true; // error, not implemented
}

bool Scope::GetCoordinates(double *ra, double *dec, double *siderealTime)
{
    return true; // error
}

bool Scope::GetSiteLatLong(double *latitude, double *longitude)
{
    return true; // error
}

bool Scope::CanSlew(void)
{
    return false;
}

bool Scope::CanSlewAsync(void)
{
    return false;
}

bool Scope::PreparePositionInteractive(void)
{
    return false; // no error
}

bool Scope::CanReportPosition()
{
    return false;
}

bool Scope::CanPulseGuide()
{
    return false;
}

bool Scope::SlewToCoordinates(double ra, double dec)
{
    return true; // error
}

bool Scope::SlewToCoordinatesAsync(double ra, double dec)
{
    return true; // error
}

void Scope::AbortSlew(void)
{
}

bool Scope::CanCheckSlewing(void)
{
    return false;
}

bool Scope::Slewing(void)
{
    return false;
}

PierSide Scope::SideOfPier(void)
{
    return PIER_SIDE_UNKNOWN;
}

wxString Scope::GetSettingsSummary()
{
    Calibration calInfo;
    GetLastCalibration(&calInfo);

    CalibrationDetails calDetails;
    GetCalibrationDetails(&calDetails);

    // return a loggable summary of current mount settings
    wxString rtnVal = Mount::GetSettingsSummary() +
        wxString::Format
            ("Calibration step = phdlab_placeholder, Max RA duration = %d, Max DEC duration = %d, DEC guide mode = %s\n",
            GetMaxRaDuration(),
            GetMaxDecDuration(),
            GetDecGuideMode() == DEC_NONE ? "Off" : GetDecGuideMode() == DEC_AUTO ? "Auto" :
            GetDecGuideMode() == DEC_NORTH ? "North" : "South"
            );
    if (calDetails.raGuideSpeed != -1.0)
    {
        rtnVal += wxString::Format("RA Guide Speed = %0.1f a-s/s, Dec Guide Speed = %0.1f a-s/s, ",
            3600.0 * calDetails.raGuideSpeed, 3600.0 * calDetails.decGuideSpeed);
    }
    else
        rtnVal += "RA Guide Speed = Unknown, Dec Guide Speed = Unknown, ";

    rtnVal += wxString::Format("Cal Dec = %s, Last Cal Issue = %s, Timestamp = %s\n",
        DeclinationStr(calInfo.declination, "%0.1f"), Mount::GetIssueString(calDetails.lastIssue),
        calDetails.origTimestamp);

    return rtnVal;
}

wxString Scope::CalibrationSettingsSummary()
{
    return wxString::Format("Calibration Step = %d ms, Assume orthogonal axes = %s", GetCalibrationDuration(),
        IsAssumeOrthogonal() ? "yes" : "no");
}

wxString Scope::GetMountClassName() const
{
    return wxString("scope");
}

Mount::MountConfigDialogPane *Scope::GetConfigDialogPane(wxWindow *pParent)
{
    return new ScopeConfigDialogPane(pParent, this);
}

Scope::ScopeConfigDialogPane::ScopeConfigDialogPane(wxWindow *pParent, Scope *pScope)
    : MountConfigDialogPane(pParent, _("Mount Guide Algorithms"), pScope)
{
    m_pScope = pScope;
}

void Scope::ScopeConfigDialogPane::LayoutControls(wxPanel *pParent, BrainCtrlIdMap& CtrlMap)
{
    // All of the scope UI controls are hosted in the parent
    MountConfigDialogPane::LayoutControls(pParent, CtrlMap);
}

void Scope::ScopeConfigDialogPane::LoadValues(void)
{
    MountConfigDialogPane::LoadValues();
}

void Scope::ScopeConfigDialogPane::UnloadValues(void)
{
    MountConfigDialogPane::UnloadValues();
}

MountConfigDialogCtrlSet *Scope::GetConfigDialogCtrlSet(wxWindow *pParent, Mount *pScope, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
{
    return new ScopeConfigDialogCtrlSet(pParent, (Scope *) pScope, pAdvancedDialog, CtrlMap);
}

ScopeConfigDialogCtrlSet::ScopeConfigDialogCtrlSet(wxWindow *pParent, Scope *pScope, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
    : MountConfigDialogCtrlSet(pParent, pScope, pAdvancedDialog, CtrlMap)
{
    int width;
    bool enableCtrls = pScope != NULL;

    m_pScope = pScope;
    width = StringWidth(_T("00000"));

    wxBoxSizer* pCalibSizer = new wxBoxSizer(wxHORIZONTAL);
    m_pCalibrationDuration = new wxSpinCtrl(GetParentWindow(AD_szCalibrationDuration), wxID_ANY, wxEmptyString, wxPoint(-1, -1),
            wxSize(width+30, -1), wxSP_ARROW_KEYS, 0, 10000, 1000,_T("Cal_Dur"));
    pCalibSizer->Add(MakeLabeledControl(AD_szCalibrationDuration, _("Calibration step (ms)"), m_pCalibrationDuration, 
        _("How long a guide pulse should be used during calibration? Click \"Calculate\" to compute a suitable value.")));
    m_pCalibrationDuration->Enable(enableCtrls);

    // create the 'auto' button and bind it to the associated event-handler
    wxButton *pAutoDuration = new wxButton(GetParentWindow(AD_szCalibrationDuration), wxID_OK, _("Calculate...") );
    pAutoDuration->SetToolTip(_("Click to open the Calibration Step Calculator to help find a good calibration step size"));
    pAutoDuration->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ScopeConfigDialogCtrlSet::OnCalcCalibrationStep, this);
    pAutoDuration->Enable(enableCtrls);

    pCalibSizer->Add(pAutoDuration);
    AddGroup(CtrlMap, AD_szCalibrationDuration, pCalibSizer);

    m_pNeedFlipDec = new wxCheckBox(GetParentWindow(AD_cbReverseDecOnFlip), wxID_ANY, _("Reverse Dec output after meridian flip"));
    AddCtrl(CtrlMap, AD_cbReverseDecOnFlip, m_pNeedFlipDec,
        _("Check if your mount needs Dec output reversed after a meridian flip"));
    m_pNeedFlipDec->Enable(enableCtrls);


    bool usingAO = TheAO() != NULL;
    if (pScope && pScope->CanCheckSlewing())
    {
        m_pStopGuidingWhenSlewing = new wxCheckBox(GetParentWindow(AD_cbSlewDetection), wxID_ANY, _("Stop guiding when mount slews"));
        AddCtrl(CtrlMap, AD_cbSlewDetection, m_pStopGuidingWhenSlewing,
            _("When checked, PHD will stop guiding if the mount starts slewing"));
    }
    else
        m_pStopGuidingWhenSlewing = 0;

    m_assumeOrthogonal = new wxCheckBox(GetParentWindow(AD_cbAssumeOrthogonal), wxID_ANY,
        _("Assume Dec orthogonal to RA"));
    m_assumeOrthogonal->Enable(enableCtrls);
    AddCtrl(CtrlMap, AD_cbAssumeOrthogonal, m_assumeOrthogonal,
        _("Assume Dec axis is perpendicular to RA axis, regardless of calibration. Prevents RA periodic error from affecting Dec calibration. Option takes effect when calibrating DEC."));

    if (pScope && !usingAO)
    {
        m_pUseBacklashComp = new wxCheckBox(GetParentWindow(AD_cbDecComp), wxID_ANY, _("Use backlash comp"));
        AddCtrl(CtrlMap, AD_cbDecComp, m_pUseBacklashComp, _("Check this if you want to apply a backlash compensation guide pulse when declination direction is reversed."));
        m_pBacklashPulse = new wxSpinCtrlDouble(GetParentWindow(AD_szDecCompAmt), wxID_ANY, wxEmptyString, wxDefaultPosition,
            wxSize(width + 30, -1), wxSP_ARROW_KEYS, 0, pScope->m_backlashComp->GetBacklashPulseLimit(), 450, 50);
        AddGroup(CtrlMap, AD_szDecCompAmt, (MakeLabeledControl(AD_szDecCompAmt, _("Amount"), m_pBacklashPulse, _("Length of backlash correction pulse (mSec). This will be automatically adjusted based on observed performance."))));

        m_pUseDecComp = new wxCheckBox(GetParentWindow(AD_cbUseDecComp), wxID_ANY, _("Use Dec compensation"));
        m_pUseDecComp->Enable(enableCtrls && pPointingSource != NULL);
        AddCtrl(CtrlMap, AD_cbUseDecComp, m_pUseDecComp, _("Automatically adjust RA guide rate based on scope declination"));

        width = StringWidth(_T("00000"));
        m_pMaxRaDuration = new wxSpinCtrl(GetParentWindow(AD_szMaxRAAmt), wxID_ANY, _T("foo"), wxPoint(-1, -1),
            wxSize(width + 30, -1), wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 150, _T("MaxRA_Dur"));
        AddLabeledCtrl(CtrlMap, AD_szMaxRAAmt, _("Max RA duration"), m_pMaxRaDuration, _("Longest length of pulse to send in RA\nDefault = 2500 ms."));

        m_pMaxDecDuration = new wxSpinCtrl(GetParentWindow(AD_szMaxDecAmt), wxID_ANY, _T("foo"), wxPoint(-1, -1),
            wxSize(width + 30, -1), wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 150, _T("MaxDec_Dur"));
        AddLabeledCtrl(CtrlMap, AD_szMaxDecAmt, _("Max Dec duration"), m_pMaxDecDuration, _("Longest length of pulse to send in declination\nDefault = 2500 ms.  Increase if drift is fast."));

        wxString dec_choices[] = {
          _("Off"), _("Auto"), _("North"), _("South")
        };
        width = StringArrayWidth(dec_choices, WXSIZEOF(dec_choices));
        m_pDecMode = new wxChoice(GetParentWindow(AD_szDecGuideMode), wxID_ANY, wxPoint(-1, -1),
            wxSize(width + 35, -1), WXSIZEOF(dec_choices), dec_choices);
        AddLabeledCtrl(CtrlMap, AD_szDecGuideMode, _("Dec guide mode"), m_pDecMode, _("Directions in which Dec guide commands will be issued"));
        m_pScope->currConfigDialogCtrlSet = this;
    }
}

void ScopeConfigDialogCtrlSet::LoadValues()
{
    MountConfigDialogCtrlSet::LoadValues();
    m_pCalibrationDuration->SetValue(m_pScope->GetCalibrationDuration());
    m_pNeedFlipDec->SetValue(m_pScope->CalibrationFlipRequiresDecFlip());
    if (m_pStopGuidingWhenSlewing)
        m_pStopGuidingWhenSlewing->SetValue(m_pScope->IsStopGuidingWhenSlewingEnabled());
    m_assumeOrthogonal->SetValue(m_pScope->IsAssumeOrthogonal());
    bool usingAO = TheAO() != NULL;
    if (!usingAO)
    {
        m_pMaxRaDuration->SetValue(m_pScope->GetMaxRaDuration());
        m_pMaxDecDuration->SetValue(m_pScope->GetMaxDecDuration());
        m_pDecMode->SetSelection(m_pScope->GetDecGuideMode());
        m_pUseBacklashComp->SetValue(m_pScope->m_backlashComp->IsEnabled());
        m_pBacklashPulse->SetValue(m_pScope->m_backlashComp->GetBacklashPulse());
        m_pUseDecComp->SetValue(m_pScope->DecCompensationEnabled());
    }
}

void ScopeConfigDialogCtrlSet::UnloadValues()
{
    m_pScope->SetCalibrationDuration(m_pCalibrationDuration->GetValue());
    m_pScope->SetCalibrationFlipRequiresDecFlip(m_pNeedFlipDec->GetValue());
    if (m_pStopGuidingWhenSlewing)
        m_pScope->EnableStopGuidingWhenSlewing(m_pStopGuidingWhenSlewing->GetValue());
    m_pScope->SetAssumeOrthogonal(m_assumeOrthogonal->GetValue());
    bool usingAO = TheAO() != NULL;
    if (!usingAO)
    {
        m_pScope->SetMaxRaDuration(m_pMaxRaDuration->GetValue());
        m_pScope->SetMaxDecDuration(m_pMaxDecDuration->GetValue());
        m_pScope->SetDecGuideMode(m_pDecMode->GetSelection());
        int oldBC = m_pScope->m_backlashComp->GetBacklashPulse();
        int newBC = m_pBacklashPulse->GetValue();
        if (oldBC != newBC)
            m_pScope->m_backlashComp->SetBacklashPulse(newBC);
        m_pScope->m_backlashComp->EnableBacklashComp(m_pUseBacklashComp->GetValue());
        m_pScope->EnableDecCompensation(m_pUseDecComp->GetValue());
        // Following needed in case user changes max_duration with blc value already set
        if (m_pScope->m_backlashComp->IsEnabled() && m_pScope->GetMaxDecDuration() < newBC)
            m_pScope->SetMaxDecDuration(newBC);
        if (pFrame)
            pFrame->UpdateCalibrationStatus();
    }
    MountConfigDialogCtrlSet::UnloadValues();
}

void ScopeConfigDialogCtrlSet::ResetRAParameterUI()
{
    m_pMaxRaDuration->SetValue(DefaultMaxRaDuration);
}

void ScopeConfigDialogCtrlSet::ResetDecParameterUI()
{
    m_pMaxDecDuration->SetValue(DefaultMaxDecDuration);
    m_pDecMode->SetSelection(1);                // 'Auto'
    m_pUseBacklashComp->SetValue(false);
}

void ScopeConfigDialogCtrlSet::OnCalcCalibrationStep(wxCommandEvent& evt)
{
    int focalLength = 0;
    double pixelSize = 0;
    int binning = 1;
    AdvancedDialog *pAdvancedDlg = pFrame->pAdvancedDialog;

    if (pAdvancedDlg)
    {
        pixelSize = pAdvancedDlg->GetPixelSize();
        binning = pAdvancedDlg->GetBinning();
        focalLength = pAdvancedDlg->GetFocalLength();
    }

    CalstepDialog calc(m_pParent, focalLength, pixelSize, binning);
    if (calc.ShowModal() == wxID_OK)
    {
        int calibrationStep;
        if (calc.GetResults(&focalLength, &pixelSize, &binning, &calibrationStep))
        {
            // Following sets values in the UI controls of the various dialog tabs - not underlying data values
            pAdvancedDlg->SetFocalLength(focalLength);
            pAdvancedDlg->SetPixelSize(pixelSize);
            pAdvancedDlg->SetBinning(binning);
            m_pCalibrationDuration->SetValue(calibrationStep);
        }
    }
}

GraphControlPane *Scope::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return new ScopeGraphControlPane(pParent, this, label);
}

Scope::ScopeGraphControlPane::ScopeGraphControlPane(wxWindow *pParent, Scope *pScope, const wxString& label)
    : GraphControlPane(pParent, label)
{
    int width;
    m_pScope = pScope;
    pScope->m_graphControlPane = this;

    width = StringWidth(_T("0000"));
    m_pMaxRaDuration = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width+30, -1),
        wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 0);
    m_pMaxRaDuration->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &Scope::ScopeGraphControlPane::OnMaxRaDurationSpinCtrl, this);
    DoAdd(m_pMaxRaDuration, _("Mx RA"));

    width = StringWidth(_T("0000"));
    m_pMaxDecDuration = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width+30, -1),
        wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 0);
    m_pMaxDecDuration->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &Scope::ScopeGraphControlPane::OnMaxDecDurationSpinCtrl, this);
    DoAdd(m_pMaxDecDuration, _("Mx DEC"));

    wxString dec_choices[] = { _("Off"),_("Auto"),_("North"),_("South") };
    m_pDecMode = new wxChoice(this, wxID_ANY,
        wxDefaultPosition,wxDefaultSize, WXSIZEOF(dec_choices), dec_choices );
    m_pDecMode->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &Scope::ScopeGraphControlPane::OnDecModeChoice, this);
    m_pControlSizer->Add(m_pDecMode);

    m_pMaxRaDuration->SetValue(m_pScope->GetMaxRaDuration());
    m_pMaxDecDuration->SetValue(m_pScope->GetMaxDecDuration());
    m_pDecMode->SetSelection(m_pScope->GetDecGuideMode());
}

Scope::ScopeGraphControlPane::~ScopeGraphControlPane()
{
    if (m_pScope)
    {
        m_pScope->m_graphControlPane = NULL;
    }
}

void Scope::ScopeGraphControlPane::OnMaxRaDurationSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pScope->SetMaxRaDuration(m_pMaxRaDuration->GetValue());
    GuideLog.SetGuidingParam("Max RA duration", m_pMaxRaDuration->GetValue());
}

void Scope::ScopeGraphControlPane::OnMaxDecDurationSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pScope->SetMaxDecDuration(m_pMaxDecDuration->GetValue());
    GuideLog.SetGuidingParam("Max DEC duration", m_pMaxDecDuration->GetValue());
}

void Scope::ScopeGraphControlPane::OnDecModeChoice(wxCommandEvent& WXUNUSED(evt))
{
    m_pScope->SetDecGuideMode(m_pDecMode->GetSelection());
    wxString dec_choices[] = { _("Off"),_("Auto"),_("North"),_("South") };
    GuideLog.SetGuidingParam("DEC guide mode", dec_choices[m_pDecMode->GetSelection()]);
}
