/*
 *  guider_multistar.h
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
 *  All rights reserved.
 *
 *  Refactored by Bret McKee
 *  Copyright (c) 2012 Bret McKee
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

#ifndef GUIDER_MULTISTAR_H_INCLUDED
#define GUIDER_MULTISTAR_H_INCLUDED

#include "masschecker.h"

class GuiderMultiStar;
class GuiderConfigDialogCtrlSet;

class GuiderMultiStarConfigDialogCtrlSet : public GuiderConfigDialogCtrlSet
{

public:
    GuiderMultiStarConfigDialogCtrlSet(wxWindow *pParent, Guider *pGuider, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap);
    virtual ~GuiderMultiStarConfigDialogCtrlSet();

    GuiderMultiStar *m_pGuiderMultiStar;
    wxSpinCtrl *m_pSearchRegion;
    wxCheckBox *m_pEnableStarMassChangeThresh;
    wxSpinCtrlDouble *m_pMassChangeThreshold;

    virtual void LoadValues(void);
    virtual void UnloadValues(void);
    void OnStarMassEnableChecked(wxCommandEvent& event);

};

class GuiderMultiStar : public Guider
{
private:

    // parameters
    bool m_massChangeThresholdEnabled;
    double m_massChangeThreshold;
    double m_originalRotationAngle;
    PHD_Point m_rotationCenter;


public:

    class GuiderMultiStarConfigDialogPane : public GuiderConfigDialogPane
    {
    protected:

        public:
        GuiderMultiStarConfigDialogPane(wxWindow *pParent, GuiderMultiStar *pGuider);
        ~GuiderMultiStarConfigDialogPane(void) {};

        virtual void LoadValues(void) {};
        virtual void UnloadValues(void) {};
        void LayoutControls(Guider *pGuider, BrainCtrlIdMap& CtrlMap);
    };

    void SetRotationCenter(const PHD_Point &rotationCenter);
    bool GetRotationCenter(PHD_Point &outRotationCenter);
    bool GetMassChangeThresholdEnabled(void);
    void SetMassChangeThresholdEnabled(bool enable);
    double GetMassChangeThreshold(void);
    bool SetMassChangeThreshold(double starMassChangeThreshold);
    bool SetSearchRegion(int searchRegion);
    bool GetRotationCenterRad(PHD_Point &outRotationCenter);

    friend class GuiderMultiStarConfigDialogPane;
    friend class GuiderMultiStarConfigDialogCtrlSet;

public:
    GuiderMultiStar(wxWindow *parent);
    virtual ~GuiderMultiStar(void);

    void OnPaint(wxPaintEvent& evt);

    bool IsLocked(void);
    bool AutoSelect(void);
    const PHD_Point& CurrentPosition(void);
    
    wxRect GetBoundingBox(void);
    int GetMaxMovePixels(void);
    double StarMass(void);
    unsigned int StarPeakADU(void);
    double SNR(void);
    double HFD(void);
    double RotationAngle(void);
    double RotationAngleDelta(void);
    int StarError(void);
    wxString GetSettingsSummary();

    Guider::GuiderConfigDialogPane *GetConfigDialogPane(wxWindow *pParent);
    GuiderConfigDialogCtrlSet *GetConfigDialogCtrlSet(wxWindow *pParent, Guider *pGuider, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap);

    void LoadProfileSettings(void);

private:
    bool IsValidLockPosition(const PHD_Point& pt);
    void InvalidateCurrentPosition(bool fullReset = false);
    bool UpdateCurrentPosition(usImage *pImage, FrameDroppedInfo *errorInfo);
    bool SetCurrentPosition(usImage *pImage, const PHD_Point& position);
    void UpdateStar(Star &s, Star &newStar);

    void OnLClick(wxMouseEvent& evt);

    void SaveStarFITS();

    DECLARE_EVENT_TABLE()
};

#endif /* GUIDER_MULTISTAR_H_INCLUDED */
