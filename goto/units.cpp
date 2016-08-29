#include <iostream>
#include <string>
#include "units.h"

using namespace std;

string raToString(double ra) {

    /*  RA is measured in hours, minutes and seconds. The maximum
        possible value is 24h (which is adjacent to zero).

        24 hours is 360 degrees.
        So one hour is 15 degrees.
        One minute is 0.25 degrees.
        One second is 0.004166667 degrees.
    
        To get decimal degrees from HMS, the following formula works:

        RA (degrees) = 15 * (HH + MM/60 + SS/3600)

        To go the other way, modulo math would be ideal but the below kludge
        works OK.

        As the least significant portion, the seconds gets any messy 
        floating-point stuff tacked onto it.

    */

    const int MAXLENGTH = 13;

    double hours = 0, minutes = 0, seconds = 0;

    while (ra > 15) {
        hours += 1;
        ra -= 15;
    }

    while (ra > 0.25) {
        minutes += 1;
        ra -= 0.25;
    }

    while (ra > 0.004166667) {
        seconds += 1;
        ra -= 0.004166667;
    }

    seconds += ra / 0.004166667;

    // Put results into the out parameter 'raChar' using printf syntax.

    char buffer[MAXLENGTH];
    snprintf ( buffer, 12, "%02g:%02g:%05.2f", hours, minutes, seconds);

    return string(buffer);

}

string decToString(double dec) {

    /*

    Declination is in degrees:arcmin:arcsec. It is signed. Max value is
    +90 at the north celesial pole, and min -90 at the south one.

    Degrees are worth one. Below that... 
    
    Degree: 1 degree
    Arcmin: 0.0167 degrees
    Arcsec: 0.0003

    Dec = + or - (DD + MM/60 + SS/3600)

    */
    
    const int MAXLENGTH = 13;

    signed int sign = 1;
    if (dec > 0) {
        sign = 1;
    } else {
        dec *= -1;
        sign = -1;
    }

    double degrees = 0, arcminutes = 0, arcseconds = 0;

    while (dec > 1) {
        degrees += 1;
        dec -= 1;
    }

    while (dec > 0.0167) {
        arcminutes += 1;
        dec -= 0.0167;
    }

    while (dec > 0.0003) {
        arcseconds += 1;
        dec -= 0.0003;
    }

    arcseconds += dec / 0.0003;

    degrees = degrees * sign;

    char buffer[MAXLENGTH];
    snprintf ( buffer, MAXLENGTH, "%02g:%02g:%05.2f", degrees, arcminutes, arcseconds);

    return string(buffer);

}