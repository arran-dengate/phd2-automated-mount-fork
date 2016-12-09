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
    wxBitmap disabledImage;
    wxBitmap altImage;
    bool enabled;
    bool displayAlt;
    int imageWidth;
    int imageHeight;
    bool isOverflow;
    CustomButton();
    CustomButton(std::function<void()> functionToStore, std::string buttonName, bool isEnabled, bool overflow, int width, int height, wxBitmap normal, wxBitmap disabled);
    CustomButton(std::function<void()> functionToStore, std::string buttonName, bool isEnabled, bool overflow, int width, int height, wxBitmap normal, wxBitmap disabled, wxBitmap alt);
    void SetImage(wxBitmap normal, wxBitmap clicked);
    void DisplayAltImage(bool alt);
    PHD_Point GetCenter();
    wxBitmap& GetImage();
    void SetClickedStatus();
    void SetPos(int x, int y);
    bool GetClickedStatus();
    bool TriggerIfClicked(int clickX, int clickY, bool upClick);
} ;



#endif /* GUIDER_MULTISTAR_H_INCLUDED */