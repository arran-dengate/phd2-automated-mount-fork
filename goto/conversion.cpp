#include <iostream>
#include <cmath>

using namespace std;

string getDate(string arguments) {
    string dateOutput;
    FILE *in;
    char buff[512];

    string command = "/bin/date " + arguments;
    const char *commandToRun = command.c_str();

    if(!(in = popen(commandToRun, "r"))){
        throw invalid_argument("Unable to check system date");
    }
    while(fgets(buff, sizeof(buff), in)!=NULL){
        cout << buff;
        dateOutput += buff;
    }
    pclose(in);

    return dateOutput;
}

double toRadians(double degrees) {
    return degrees * 0.01745329252;
}

double toDegrees(double radians) {
    return radians * 57.2957795129;
}

void equatorialToHorizontal(double ra, double dec, double &outAlt, double &outAz) {

    // Converting equatorial (RA/DEC) coordinates to horizontal (ALT/AZ)
    // This requires knowledge of local time and lat/long.
    // Based on http://www.stargazing.net/kepler/altaz.html

    // Get date

    double year   = stod(getDate("-u +%Y"));
    double month  = stod(getDate("-u +%m"));
    double day    = stod(getDate("-u +%d"));
    double hour   = stod(getDate("-u +%H"));
    double minute = stod(getDate("-u +%M"));
    double second = stod(getDate("-u +%S"));

    // Calculate days after J2000, including fractions of days 

    double daysAfterJ2K = 367*year-int(7*(year+int((month+9)/12))/4)+int(275*month/9)+day-730531.5;
    double fractionsAfterJ2K = (hour + (minute/60) + second/3600) / 24;
    daysAfterJ2K += fractionsAfterJ2K; 

    printf("Year %g month %g day %g hour %g minute %g second %g\n", year, month, day, hour, minute, second);
    printf("Days since J2000 to specified date: %f \n", daysAfterJ2K);

    // Get lat/long
    // TODO: get this with GPS... 

    double lat = -35.254981;
    double lon = 149.072606;

    // Calculate local siderial time
    // LST = 100.46 + 0.985647 * d + long + 15*UT
    // d    is the days from J2000, including the fraction of a day
    // UT   is the universal time in decimal hours
    // long is your longitude in decimal degrees, East positive.
    // Add or subtract multiples of 360 to bring LST in range 0 to 360 degrees.

    double lst = 100.46 + 0.985647 * daysAfterJ2K + lon + 15 * (hour + (minute / 60) + (second / 3600));
    while (lst > 360) {
        lst -= 360;
    }
    while (lst < 0) {
        lst += 360;
    }

    printf("Local siderial time: %g\n", lst / 15);

    // Calculate hour angle
    // HA = LST - RA

    double ha = lst - ra;

    while (ha > 360) {
        ha -= 360;
    }
    while (ha < 0) {
        ha += 360;
    }

    printf("Hour angle: %g\n", ha);

    // Calculate altitude
    // The trig functions in CPP only take radians, so have to convert.

    //ha = 54.382617; // Example only
    //lat = 52.5; // Example only
    //dec = 36.466667; // Example only

    ra = toRadians(ra);
    dec = toRadians(dec);
    ha = toRadians(ha);
    lat = toRadians(lat);
    lon = toRadians(lon);

    double alt = asin ( sin( dec ) * sin( lat ) + cos( dec ) * cos( lat ) * cos( ha ) ); 
    outAlt = toDegrees(alt);
    
    // Calculate azimth
    
    double a = acos ( ( sin(dec) - sin(alt) * sin(lat) ) / ( cos(alt) * cos(lat) ) );
    
    // double az;

    if (sin(ha) < 0) {
        outAz = toDegrees(a);    
    } else {
        outAz = 360 - toDegrees(a);
    }

    printf("Alt: %g\nAz: %g\n", outAlt, outAz);
}



