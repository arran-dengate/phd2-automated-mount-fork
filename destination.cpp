#include "phd.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

const char SKYFIELD_RESULT_FILENAME[] = "/dev/shm/phd2/goto/skyfield-result"; 

Destination::Destination() {
    initialised = false;
}

Destination::Destination(const std::string &input) {
    initialised = true;
        
    Debug.AddLine(wxString::Format("New destination: %s", input));
    vector<std::string> values;
    SplitString(input, ',', values);

    name = values[0];

    ra = 0;
    dec = 0;
    alt = 0;
    az = 0;

    if (! isdigit(values[1][0])) {
        type = values[1];
        LookupEphemeral(name, ra, dec, alt, az);
        ephemeral = true;
    } else {
        type = "Star";
        ra  = stod(values[1]);
        dec = stod(values[2]);
        ephemeral=false;
        EquatorialToHorizontal(ra, dec, alt, az, false);
    }
}

bool Destination::CheckForUpdate() {
    vector<string> skyfieldResults;
    std::ifstream file( SKYFIELD_RESULT_FILENAME );
    string tok;
    if ( file ) {
        std::stringstream ss;
        ss << file.rdbuf();
        file.close();
        while(getline(ss, tok, ',')) {
            skyfieldResults.push_back(tok);
        }
    }
    if (skyfieldResults.size() > 0) {
        alt = stod(skyfieldResults[0]);
        az  = stod(skyfieldResults[1]);
        if (ephemeral) {
            ra  = stod(skyfieldResults[2]);
            dec = stod(skyfieldResults[3]);
        }
    }
}

bool Destination::NonBlockingUpdate() {
    if (!initialised) return false;
    std::string command;
    if (ephemeral) {
        command = "/usr/local/skyfield/sky.py --planet=" + name + " &";
    } else {
        command = "/usr/local/skyfield/sky.py --ra=" + std::to_string(ra) + " --dec=" + std::to_string(dec) + " &";
    }
    system(command.c_str());
    return true;
}

bool Destination::EquatorialToHorizontal(double inRa, double inDec, double &outAlt, double &outAz, bool useStoredTimestamp) {
    std::string command = "/usr/local/skyfield/sky.py --ra=" + std::to_string(inRa) + " --dec=" + std::to_string(inDec);
    if ( useStoredTimestamp ) {
        // command += " --use-stored"; TODO re-enable this if it's not the cause of our problems...
    }
    std::string skyOutput;
    FILE *in;
    char buf[200];

    if(!(in = popen(command.c_str(), "r"))) {
        Debug.AddLine(wxString::Format("Guider: Skyfield threw error while converting equatorial to horizontal coordinates"));
        return 1;
    }
    while(fgets(buf, sizeof(buf), in)!=NULL) {
        cout << buf;
        skyOutput += buf;
    }
    pclose(in);

    std::vector<std::string> values;
    SplitString(skyOutput, ',', values);
    outAlt = stod(values[0]);
    outAz  = stod(values[1]);

    Debug.AddLine(wxString::Format("Finishing eq2horz with ra %f, dec %f, outAlt %f, outAz %f", inRa, inDec, outAlt, outAz));

    return 0;
}

bool Destination::LookupEphemeral(const std::string &ephemeral, double &outRa, double &outDec, double &outAlt, double &outAz) {
    std::string command = "/usr/local/skyfield/sky.py --planet=" + ephemeral;
    std::string skyOutput;
    FILE *in;
    char buf[200];

    if(!(in = popen(command.c_str(), "r"))) {
        Debug.AddLine(wxString::Format("Guider: Skyfield threw error while looking up ephemeral for horizontal coordinates"));
        return false;
    }
    while(fgets(buf, sizeof(buf), in)!=NULL) {
        cout << buf;
        skyOutput += buf;
    }
    pclose(in);

    std::vector<std::string> values;
    SplitString(skyOutput, ',', values);
    outAlt  = stod(values[0]);
    outAz   = stod(values[1]);
    outRa   = stod(values[2]);
    outDec  = stod(values[3]);
    
    return true;
}

void Destination::SplitString(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

void Destination::GetAltDMS(double &hours, double &minutes, double &seconds) {
    DegreesToDMS(alt, hours, minutes, seconds);
}

void Destination::GetAzDMS(double &hours, double &minutes, double &seconds) {
    DegreesToDMS(az, hours, minutes, seconds);
}

void Destination::GetRaHMS(double &hours, double &minutes, double &seconds) {
    DegreesToHMS(ra, hours, minutes, seconds);
}

void Destination::GetDecDMS(double &hours, double &minutes, double &seconds) {
    DegreesToDMS(ra, hours, minutes, seconds);
}

void Destination::DegreesToHMS(double degrees, double &hours, double &minutes, double &seconds) {

    /*  RA is measured in hours, minutes and seconds. The maximum possible value is 24h (which is adjacent to zero).
        24 hours is 360 degrees, so one hour is 15 degrees, one minute is 0.25 degrees, and one second is 0.004166667 degrees.
    */

    hours = degrees / 15.0;
    double fraction;
    fraction = modf(hours, &hours);
    minutes = fraction * 60.0;
    fraction = modf(minutes, &minutes);
    seconds = fraction * 60.0; 
}

void Destination::DegreesToDMS(double input, double &degrees, double &arcMinutes, double &arcSeconds) {
    
    /* Declination is in degrees:arcmin:arcsec. It is signed. Max value is
       +90 at the north celesial pole, and min -90 at the south one.
 
       1 degree is 60 arcminutes
       1 arcminute is 60 arcseconds.
    */

    double fraction;
    fraction = modf(input, &degrees);
    arcMinutes = fraction * 60.0;
    fraction = modf(arcMinutes, &arcMinutes);
    arcSeconds = fraction * 60.0;
}
