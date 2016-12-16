#include "phd.h"
#include <string>
#include <sstream>
#include <vector>

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
        EquatorialToHorizontal(ra, dec, alt, az, false);
    }
}

bool Destination::EquatorialToHorizontal(double inRa, double inDec, double &outAlt, double &outAz, bool useStoredTimestamp) {
    std::string command = "/usr/local/skyfield/sky.py --ra=" + std::to_string(ra) + " --dec=" + std::to_string(dec);
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
