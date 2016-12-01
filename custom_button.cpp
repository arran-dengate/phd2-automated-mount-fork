#include "phd.h"
#include "point.h"

CustomButton::CustomButton() {
    position.X = 0;
    position.Y = 0;
    wasClicked = false;
}

CustomButton::CustomButton(double x, double y, int width, int height) {
    position.X = x;
    position.Y = y;
    imageWidth   = width;
    imageHeight  = height;
    wasClicked = false;
}

void CustomButton::SetSizeAndPosition(double x, double y, int width, int height) {
    position.X = x;
    position.Y = y;
    imageWidth   = width;
    imageHeight  = height;
}

PHD_Point CustomButton::GetCenter() {
    return PHD_Point(position.X + imageWidth / 2, position.Y + imageHeight / 2);
}

void CustomButton::SetImage(wxBitmap normal, wxBitmap clicked) {
    normalImage  = normal;
    clickedImage = clicked;
}

wxBitmap& CustomButton::GetImage() {
    if ( wasClicked ) { 
        wasClicked = false;
        return clickedImage;
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