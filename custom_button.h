#ifndef CUSTOM_BUTTON_H_INCLUDED
#define CUSTOM_BUTTON_H_INCLUDED

#include "point.h"

class CustomButton {
    bool wasClicked;
    public:
    wxBitmap normalImage;
    wxBitmap clickedImage;
    PHD_Point position;
    int imageWidth;
    int imageHeight;
    CustomButton();
    CustomButton(double x, double y, int imageWidth, int imageHeight);
    void SetImage(wxBitmap normal, wxBitmap clicked);
    PHD_Point GetCenter();
    wxBitmap& GetImage();
    void SetClickedStatus();
    void SetSizeAndPosition(double x, double y, int width, int height);
    bool GetClickedStatus();
} ;



#endif /* GUIDER_MULTISTAR_H_INCLUDED */