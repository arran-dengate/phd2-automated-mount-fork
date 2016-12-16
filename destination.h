#ifndef DESTINATION_H_INCLUDED
#define DESTINATION_H_INCLUDED

#include "point.h"
#include <string>
#include <functional>

class Destination {
    void SplitString(const std::string &s, char delim, std::vector<std::string> &elems);
    bool LookupEphemeral(const std::string &ephemeral, double &outRa, double &outDec, double &outAlt, double &outaz);
    bool EquatorialToHorizontal(double inRa, double inDec, double &outAlt, double &outAz, bool useStoredTimestamp);
    public:
    Destination();
    Destination(const std::string &input);
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