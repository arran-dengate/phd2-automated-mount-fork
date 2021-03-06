/*
 *  cam_openssag.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2009 Craig Stark.
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

#ifdef OPENSSAG
#include "camera.h"
#include "image_math.h"
#include "cam_openssag.h"

#include <iostream>
#include <fstream>
#include "openssag.h"
#include <libusb.h>

using namespace OpenSSAG;

static bool s_libusb_init_done;

static bool init_libusb()
{
    if (s_libusb_init_done)
        return false;
    int ret = libusb_init(0);
    if (ret != 0)
        return true;
    s_libusb_init_done = true;
    return false;
}

static void uninit_libusb()
{
    if (s_libusb_init_done)
    {
        libusb_exit(0);
        s_libusb_init_done = false;
    }
}

Camera_OpenSSAGClass::Camera_OpenSSAGClass()
{
    Connected = false;
    Name = _T("StarShoot Autoguider (OpenSSAG)");
    FullSize = wxSize(1280,1024);  // Current size of a full frame
    m_hasGuideOutput = true;  // Do we have an ST4 port?
    HasGainControl = true;  // Can we adjust gain?

    ssag = new SSAG();
}

Camera_OpenSSAGClass::~Camera_OpenSSAGClass()
{
    uninit_libusb();
}

wxByte Camera_OpenSSAGClass::BitsPerPixel()
{
    return 8;
}

bool Camera_OpenSSAGClass::Connect(const wxString& camId)
{
    if (init_libusb())
    {
        wxMessageBox(_("Could not initialize USB library"), _("Error"));
        return true;
    }

    struct ConnectInBg : public ConnectCameraInBg
    {
        SSAG *ssag;
        ConnectInBg(SSAG *ssag_) : ssag(ssag_) { }
        bool Entry()
        {
            bool err = !ssag->Connect();
            return err;
        }
    };

    if (ConnectInBg(ssag).Run())
    {
        wxMessageBox(_T("Could not connect to StarShoot Autoguider"), _("Error"));
        return true;
    }

    Connected = true;  // Set global flag for being connected

    return false;
}

bool Camera_OpenSSAGClass::ST4PulseGuideScope(int direction, int duration)
{

    switch (direction) {
        case WEST:
            ssag->Guide(guide_west, duration);
            break;
        case NORTH:
            ssag->Guide(guide_north, duration);
            break;
        case SOUTH:
            ssag->Guide(guide_south, duration);
            break;
        case EAST:
            ssag->Guide(guide_east, duration);
            break;
        default: return true; // bad direction passed in
    }

    //wxMilliSleep(duration + 10);
    wxMilliSleep(300);
    return false;
}

bool Camera_OpenSSAGClass::Disconnect()
{
    Connected = false;
    ssag->Disconnect();
    return false;
}

bool Camera_OpenSSAGClass::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    int xsize = FullSize.GetWidth();
    int ysize = FullSize.GetHeight();

    if (img.Init(FullSize))
    {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }

    ssag->SetGain((int)(GuideCameraGain / 24));
    struct raw_image *raw = ssag->Expose(duration);

    if (!raw)
    {
        Debug.Write("ssag Expose returned null!\n");
        return true;
    }

    for (unsigned int i = 0; i < raw->width * raw->height; i++) {
        img.ImageData[i] = (unsigned short) raw->data[i];
    }

    ssag->FreeRawImage(raw);

    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);

    return false;
}

bool Camera_OpenSSAGClass::GetDevicePixelSize(double *devPixelSize)
{
    *devPixelSize = 5.2;
    return false;
}

#endif // Apple-only
