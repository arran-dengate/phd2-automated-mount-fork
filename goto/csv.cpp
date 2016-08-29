#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <stdexcept>
#include "csv.h"   
using namespace std;

const int PARTIAL_MATCHES_TO_DISPLAY = 500;
const char DATA_DIRECTORY[] = "./data";

matches searchAll(string searchString) {

    if (searchString == "") {
        cout << "Type search string and press enter..." << endl;
        // string searchString;
        getline(cin, searchString);    
    }    

    if (searchString == "") {
        cout << "Searching for empty string! What?";
        exit(1);
    }

    vector<string> fileNames = getFileNames();

    cout << "The following files will be searched: ";
    for (int i = 0; i < fileNames.size(); i++) {
        cout << fileNames[i] << " ";
    }

    cout << endl;
     
    matches masterResults;

    for (int i = 0; i < fileNames.size(); i++) {
        
        string fileNameString = string("./data/") + fileNames[i];   
        const char * fileName = fileNameString.c_str();
        ifstream data(fileName);
        matches results = searchCSV(searchString, data);

        if (not results.exact.empty()) {
            cout << results.exact << endl;
            masterResults.exact = results.exact;
            masterResults.pos.ra = results.pos.ra;
            masterResults.pos.dec = results.pos.dec;
          //  printf("Right ascension %g, declination %g\n", 
          //         results.ra, 
          //         results.dec);
            return masterResults;
        } else if (results.partials.size() == 0) {
            // cout << "No matches";
        } else if (results.partials.size() > 0) {
            // cout << "Partial matches:\n";
            sort (results.partials.begin(), results.partials.end());
            masterResults.partials.insert(masterResults.partials.end(),
                                          results.partials.begin(),
                                          results.partials.end());
        }    
    }
    //for (int i = 0; i < masterResults.partials.size(); i++) {
    //    cout << masterResults.partials[i] << endl;
    //}
    return masterResults;
}

vector<string> getFileNames () {

    // Returns a list of filenames in the data folder.

    vector<string> fileNames;   
    DIR *dpdf;
    struct dirent *epdf;

    dpdf = opendir(DATA_DIRECTORY);
    if (dpdf != NULL){
        while (epdf = readdir(dpdf)){
            string fileName = epdf->d_name;
            string suffix = ".csv";
            if (fileName.rfind(suffix) == (fileName.size()-suffix.size())) {
                fileNames.push_back(fileName);
            }
        }
    } else {
        cout << "Cannot find data directory.";
    }
    return fileNames;
}

matches searchCSV (string searchString, ifstream& data) {

    // Searches one CSV file for a given string.

    matches results;
    string line;
    string cell;

    getline(data,line); // Skip header row.

    while(getline(data,line))
    {
        stringstream  lineStream(line);
        getline(lineStream,cell,'\t'); // Gets the first line up to the tab.

        if (cell.find(searchString) != string::npos
            && results.partials.size() < PARTIAL_MATCHES_TO_DISPLAY) {
            results.partials.push_back(cell);

            if (searchString == cell) {
                results.exact = cell;
                try {
                    getline(lineStream,cell,'\t');
                    results.pos.ra = stod(cell);
                    getline(lineStream,cell,'\t');
                    results.pos.dec = stod(cell);    
                } catch (const invalid_argument&) {
                    cerr << "\nWhile reading CSV file, unable to parse "
                    "RA/DEC from the following line: \n" << line;
                    throw;
                }
                
            }
        }
    }    
    return results;
}