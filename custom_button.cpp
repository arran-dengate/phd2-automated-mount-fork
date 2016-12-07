#include "phd.h"
#include "point.h"
#include <functional>

CustomButton::CustomButton() {
    imageWidth    = 0;
    imageHeight   = 0;
    wasClicked    = false;
    enabled       = true;
}

CustomButton::CustomButton(std::function<void()> functionToStore, std::string buttonName, bool isEnabled, int width, int height, wxBitmap normal, wxBitmap clicked, wxBitmap disabled) {
    name           = buttonName;
    imageWidth     = width;
    imageHeight    = height;
    wasClicked     = false;
    normalImage    = normal;
    clickedImage   = clicked;
    disabledImage  = disabled;
    storedFunction = functionToStore;
    enabled        = isEnabled;
}

void CustomButton::SetPos(int x, int y) {
    xPos    = x;
    yPos    = y;
}

bool CustomButton::TriggerIfClicked(int clickX, int clickY, bool upClick) {
    Debug.AddLine(wxString::Format("Mouse clicked %d %d, button pos %d %d, imageHeight imageWidth %d %d", clickX, clickY, xPos, yPos, imageWidth, imageHeight));
    if ( (clickX > xPos && clickX < xPos + imageWidth) and (clickY > yPos && clickY < yPos + imageHeight ) ) {
        if (upClick) {
            Debug.Write("Mouse up on button!");
            storedFunction();   
        } else {
            Debug.Write("Mouse down on button!");
        }
    }
}

PHD_Point CustomButton::GetCenter() {
    return PHD_Point(xPos + imageWidth / 2, yPos + imageHeight / 2);
}

void CustomButton::SetImage(wxBitmap normal, wxBitmap clicked) {
    normalImage  = normal;
    clickedImage = clicked;
}

wxBitmap& CustomButton::GetImage() {
    if (! enabled) return disabledImage;
    
    return normalImage;
    
}

void CustomButton::SetClickedStatus() {
    wasClicked = true;
}

bool CustomButton::GetClickedStatus() {
    return wasClicked;
}

//&wxBitmap CustomButton::GetImage( 