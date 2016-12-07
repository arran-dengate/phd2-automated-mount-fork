#ifndef CUSTOM_BUTTON_H_INCLUDED
#define CUSTOM_BUTTON_H_INCLUDED

#include "point.h"
#include <string>
#include <functional>

class CustomButton {
    bool wasClicked;
    std::function<void()> storedFunction;
    public:
    std::string name;
    int xPos;
    int yPos;
    wxBitmap normalImage;
    wxBitmap clickedImage;
    wxBitmap disabledImage;
    bool enabled;
    bool altImage;
    int imageWidth;
    int imageHeight;
    CustomButton();
    CustomButton(std::function<void()> functionToStore, std::string buttonName, bool isEnabled, int width, int height, wxBitmap normal, wxBitmap clicked, wxBitmap disabled);
    void SetImage(wxBitmap normal, wxBitmap clicked);
    PHD_Point GetCenter();
    wxBitmap& GetImage();
    void SetClickedStatus();
    void SetPos(int x, int y);
    bool GetClickedStatus();
    bool TriggerIfClicked(int clickX, int clickY, bool upClick);
} ;



#endif /* GUIDER_MULTISTAR_H_INCLUDED */