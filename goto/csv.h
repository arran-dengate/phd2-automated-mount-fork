#ifndef CSV_H_INCLUDED
#define CSV_H_INCLUDED

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include "astrometry.h"
using namespace std;

struct matches {
    vector<string> partials;
    string exact;
    eqPosition pos;
} ;

int main ();
matches searchAll(string searchString="");
matches searchCSV (string searchString, ifstream& data);
vector<string> getFileNames ();

#endif // CSV_H_INCLUDED
