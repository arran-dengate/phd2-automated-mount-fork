#ifndef DESTINATION_H_INCLUDED
#define DESTINATION_H_INCLUDED

#include "point.h"
#include <string>
#include <functional>

class Destination {
    bool LookupEphemeral(const std::string &ephemeral, double &outRa, double &outDec, double &outAlt, double &outaz);
    void DegreesToDMS(double input, double &degrees, double &arcMinutes, double &arcSeconds);
    void DegreesToHMS(double degrees, double &hours, double &minutes, double &seconds);
    public:
    static void SplitString(const std::string &s, char delim, std::vector<std::string> &elems);
    static bool EquatorialToHorizontal(double inRa, double inDec, double &outAlt, double &outAz, bool useStoredTimestamp);
    void GetAltDMS(double &hours, double &minutes, double &seconds);
    void GetAzDMS(double &hours, double &minutes, double &seconds);
    void GetRaHMS(double &hours, double &minutes, double &seconds);
    void GetDecDMS(double &hours, double &minutes, double &seconds);
    Destination();
    Destination(const std::string &input);
    bool NonBlockingUpdate();
    bool CheckForUpdate();
    double ra;
    double dec;
    double alt;
    double az;
    std::string name;
    std::string type;
    bool ephemeral;
    bool initialised;
} ;



#endif /* DESTINATION_H_INCLUDED */