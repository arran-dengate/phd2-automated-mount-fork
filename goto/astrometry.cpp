#include "astrometry.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <regex>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

const char INPUT_IMAGE_DIRECTORY[]         = "/dev/shm/phd2/goto";
const char INPUT_IMAGE_PARENT_DIRECTORY[]  = "/dev/shm/phd2/goto";
const char SIGNAL_FILENAME[]               = "/dev/shm/phd2/goto/signal.txt";
const char IMAGE_FILENAME[]                = "/dev/shm/phd2/goto/guide-scope-image.fits";

int getAstroLocation(double &outRa, double &outDec) {

    // Run astrometry, identify where we are,
    // and pipe the textual results from stdin into pos.ra and pos.dec.
    
    string astOutput;
    FILE *in;
    char buff[512];

    // Create directory if does not exist
    struct stat info;
    if( stat( INPUT_IMAGE_DIRECTORY, &info ) != 0 ) {
        mkdir(INPUT_IMAGE_PARENT_DIRECTORY, 0755);
        mkdir(INPUT_IMAGE_DIRECTORY, 0755);
    }
    
    // Clean up any old guide scope image.

    remove(IMAGE_FILENAME);

    // Create a file signalling to PHD2 that Goto wants an image.

    std::ofstream outfile (SIGNAL_FILENAME);
    outfile << "The existence of this file is a signal to PHD2 to save a FITS image in ../input/";
    outfile.close();

    struct stat fileinfo;
    cout << "\nWaiting for guide scope image...\n";
    while(stat(IMAGE_FILENAME, &fileinfo)) {
        sleep(1);
    }

    char inputFilename[] = "/usr/local/astrometry/bin/solve-field --overwrite /usr/local/goto/input/guide-scope-image.fits";
    if(!(in = popen(inputFilename, "r"))){
        return 1;
    }
    while(fgets(buff, sizeof(buff), in)!=NULL){
        cout << buff;
        astOutput += buff;
    }
    pclose(in);
    
    unsigned int strLocation = astOutput.find("(RA,Dec)");
    const string line = astOutput.substr(strLocation, strLocation+30);
    // const string line = "Field center: (RA,Dec) = (104.757911, -3.500760) deg.";
    
    regex raReg("\\(-?[[:digit:]]+(\\.[[:digit:]]+)?");
    regex decReg("-?[[:digit:]]+(\\.[[:digit:]]+)?\\)");
    smatch raMatch;
    smatch decMatch;
    regex_search(line.begin(), line.end(), raMatch, raReg); 
    regex_search(line.begin(), line.end(), decMatch, decReg);
    string raResult = raMatch[0];
    string decResult = decMatch[0];
    outRa = stod(raResult.substr(1, raResult.size())); // Trim leading bracket
    outDec = stod(decResult.substr(0, decResult.size()-1)); // Trim ending bracket
    
    // printf("Ra %f Dec %f", ra, dec);

    return(0); // todo: return error code if failed
}
    
