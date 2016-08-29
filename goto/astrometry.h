#ifndef ASTROMETRY_H_INCLUDED
#define ASTROMETRY_H_INCLUDED

int getAstroLocation(double &outRa, double outDec);

struct eqPosition {
    double ra;
    double dec;
} ;

#endif // ASTROMETRY_H_INCLUDED
