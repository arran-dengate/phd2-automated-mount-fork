#include "phd.h"
#include "point.h"
#include <functional>

CustomButton::CustomButton() {
    imageWidth    = 0;
    imageHeight   = 0;
    wasClicked    = false;
    enabled       = true;
    displayAlt    = false;
}

CustomButton::CustomButton(std::function<void()> functionToStore, std::string buttonName, bool isEnabled, bool overflow, int width, int height, wxBitmap normal, wxBitmap disabled) {
    name           = buttonName;
    imageWidth     = width;
    imageHeight    = height;
    wasClicked     = false;
    normalImage    = normal;
    disabledImage  = disabled;
    storedFunction = functionToStore;
    enabled        = isEnabled;
    displayAlt     = false;
    isOverflow     = overflow;
}

CustomButton::CustomButton(std::function<void()> functionToStore, std::string buttonName, bool isEnabled, bool overflow, int width, int height, wxBitmap normal, wxBitmap disabled, wxBitmap alt) {
    name            = buttonName;
    imageWidth      = width;
    imageHeight     = height;
    wasClicked      = false;
    normalImage     = normal;
    disabledImage   = disabled;
    altImage        = alt;
    storedFunction  = functionToStore;
    enabled         = isEnabled;
    displayAlt      = false;
    isOverflow      = overflow;
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
    // We're not using the click image at the moment - will sort out how to handle this later
}

void CustomButton::DisplayAltImage(bool alt) {
    displayAlt = alt;
}

wxBitmap& CustomButton::GetImage() {
    if (! enabled) {
        return disabledImage;   
    } else if (displayAlt) {
        return altImage;
    } else {
        return normalImage;
    }
    
}

void CustomButton::SetClickedStatus() {
    wasClicked = true;
}

bool CustomButton::GetClickedStatus() {
    return wasClicked;
}

//&wxBitmap CustomButton::GetImage( 