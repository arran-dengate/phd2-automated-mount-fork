#include <iostream>
#include <stdio.h>
#include <ctime>
#include "csv.h"
#include "astrometry.h"
#include "units.h"
#include "conversion.h"

using namespace std;

int main() {
    
    cout << "Started application.\n";

    // Seek a match based on input.
    
    // cout << raToString(double(101.287215)) << endl;
    // cout << decToString(double(-16.716116)) << endl;

    matches results; 

    while (results.exact.empty()) {
        //results = searchAll();
        results = searchAll("Canopus");
        if (not results.exact.empty()) {
            printf("Right ascension\t %5.3f° %12s H:M:S\n", results.pos.ra, raToString(results.pos.ra).c_str());
            printf("Declination\t %5.3f° %12s D:M:S\n", results.pos.dec, decToString(results.pos.dec).c_str());
        } else {
            cout << "Partial matches:\n";
            for (int i = 0; i < results.partials.size(); i++) {
                cout << results.partials[i] << endl;
            }
        }
    }
    cout << "\nSearch complete\n";
    cout << "Exact match was " << results.exact << endl;

    // Where is this expected location?

    double destAlt, destAz;
    equatorialToHorizontal(results.pos.ra, results.pos.dec, destAlt, destAz);
    printf("Destination is %g, %g equatorial, %g, %g horizontal.",
           results.pos.ra, results.pos.dec, destAlt, destAz);

    // Call astrometry library to find current location.

    eqPosition currentPos;

    while (true) {
        getAstroLocation(currentPos);
        cout << "\n=================================================\n";
        printf("Current position:\n");
        printf("Right ascension\t %5.3f° %12s H:M:S\n", currentPos.ra, raToString(currentPos.ra).c_str());
        printf("Declination\t %5.3f° %12s D:M:S\n", currentPos.dec, decToString(currentPos.dec).c_str());        
        cout << "---------------------------------------------------\n";
        printf("Destination:\n");
        printf("Right ascension\t %5.3f° %12s H:M:S\n", results.pos.ra, raToString(results.pos.ra).c_str());
        printf("Declination\t %5.3f° %12s D:M:S\n", results.pos.dec, decToString(results.pos.dec).c_str());    
        cout << "===================================================\n";
    }
    
}
