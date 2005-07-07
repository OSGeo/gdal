/* $Id$
*/

#include "msg_basic_types.h"

#include <netinet/in.h>
#include <stdio.h>

namespace msg_native_format {

#ifndef SQR 
#define SQR(x) ((x)*(x))
#endif

// endian conversion routines
void to_native(GP_PK_HEADER& h) {
    h.sourceSUId    = ntohl(h.sourceSUId);
    h.sequenceCount = ntohs(h.sequenceCount);
    h.packetLength  = ntohl(h.packetLength);
}

void to_native(GP_PK_SH1& h) {
    h.spacecraftId  = ntohs(h.spacecraftId);
}

void to_native(SUB_VISIRLINE& v) {
    v.satelliteId   = ntohs(v.satelliteId);
    v.lineNumberInVisirGrid = ntohl(v.lineNumberInVisirGrid);
}

static void swap_64_bits(unsigned char* b) {
    for (int i=0; i < 4; i++) {
        unsigned char t = b[i];
        b[i] = b[7-i];
        b[7-i] = t;
    }
}

void to_native(RADIOMETRIC_PROCCESSING_RECORD& r) {
    for (int i=0; i < 12; i++) {
        swap_64_bits((unsigned char*)&r.level1_5ImageCalibration[i].cal_slope);
        swap_64_bits((unsigned char*)&r.level1_5ImageCalibration[i].cal_offset);
    }
}

void to_native(IMAGE_DESCRIPTION_RECORD& r) {
    r.referencegrid_visir.numberOfLines = ntohl(r.referencegrid_visir.numberOfLines);
    r.referencegrid_visir.numberOfColumns = ntohl(r.referencegrid_visir.numberOfColumns);
    // should floats be swapped too?
    unsigned int t;
    
    // convert float using ntohl
    t = *(unsigned int *)&r.referencegrid_visir.lineDirGridStep;
    t = ntohl(t);
    r.referencegrid_visir.lineDirGridStep = *(float *)&t;
    
    // convert float using ntohl
    t = *(unsigned int *)&r.referencegrid_visir.columnDirGridStep;
    t = ntohl(t);
    r.referencegrid_visir.columnDirGridStep = *(float *)&t;
}

void to_string(PH_DATA& d) {
    d.name[29] = 0;
    d.value[49] = 0;
}

// unit tests on structures
bool perform_type_size_check(void) {
    bool success = true;
    if (sizeof(MAIN_PROD_HEADER) != 3674) {
        fprintf(stderr, "MAIN_PROD_HEADER size not 3674 (%d)\n",sizeof(MAIN_PROD_HEADER));
        success = false;
    }
    if (sizeof(SECONDARY_PROD_HEADER) != 1120) {
        fprintf(stderr, "SECONDARY_PROD_HEADER size not 1120 (%d)\n",sizeof(SECONDARY_PROD_HEADER));
        success = false;
    }
    if (sizeof(SUB_VISIRLINE) != 27) {
        fprintf(stderr, "SUB_VISIRLINE size not 17 (%d)\n", sizeof(SUB_VISIRLINE));
        success = false;
    }
    if (sizeof(GP_PK_HEADER) != 22) {
        fprintf(stderr, "GP_PK_HEADER size not 22 (%d)\n", sizeof(GP_PK_HEADER));
        success = false;
    }
    if (sizeof(GP_PK_SH1) != 16) {
        fprintf(stderr, "GP_PK_SH1 size not 16 (%d)\n", sizeof(GP_PK_SH1));
        success = false;
    }
    return success;
}

const double Conversions::altitude      =   42164;          // from origin
const double Conversions::req           =   6378.1690;       // earthequatorial radius
const double Conversions::rpol          =   6356.5838;       // earth polar radius
const double Conversions::oblate        =   1.0/298.257;    // oblateness of earth
const double Conversions::deg_to_rad    =   M_PI/180.0; 
const double Conversions::rad_to_deg    =   180.0/M_PI; 
const double Conversions::nlines        =   3712;           // number of lines in an image
const double Conversions::step          =   17.83/nlines;    // pixel / line step in degrees

const int Conversions::CFAC    = -781648343;
const int Conversions::LFAC    = -781648343;
const int Conversions::COFF    = 1856;
const int Conversions::LOFF    = 1856;

#define SQR(x) ((x)*(x))

void Conversions::convert_pixel_to_geo(double line, double column, double&longitude, double& latitude) {
    double x = (column - COFF - 0.0) / double(CFAC >> 16);
    double y = (line - LOFF - 0.0) / double(LFAC >> 16);
    
    double sd = sqrt(SQR(altitude*cos(x)*cos(y)) - (SQR(cos(y)) + 1.006803*SQR(sin(y)))*1737121856); 
    double sn = (altitude*cos(x)*cos(y) - sd)/(SQR(cos(y)) + 1.006803*SQR(sin(y)));
    double s1 = altitude - sn*cos(x)*cos(y);
    double s2 = sn*sin(x)*cos(y);
    double s3 = -sn*sin(y);
    double sxy = sqrt(s1*s1 + s2*s2);
    
    longitude = atan(s2/s1);
    latitude  = atan(1.006803*s3/sxy);
    
    longitude = longitude / M_PI * 180.0;
    latitude  = latitude  / M_PI * 180.0;
}

void Conversions::compute_pixel_xyz(double line, double column, double& x,double& y, double& z) {
    double asamp = -(column - (nlines/2.0 + 0.5)) * step;
    double aline = (line - (nlines/2.0 + 0.5)) * step;
    
    asamp *= deg_to_rad;
    aline *= deg_to_rad;
    
    double tanal = tan(aline);
    double tanas = tan(asamp);
    
    double p = -1;
    double q = tanas;
    double r = tanal * sqrt(1 + q*q);
    
   double a = q*q + (r*req/rpol)*(r*req/rpol) + p*p;
    double b = 2 * altitude * p;
    double c = altitude * altitude  - req*req;
    
    double det = b*b - 4*a*c;
     
    if (det > 0) {
        double k = (-b - sqrt(det))/(2*a);
        x = altitude + k*p;
        y = k * q;
        z = k * r;
        
    } else {
        fprintf(stderr, "Warning: pixel not visible\n");
    }
}

double Conversions::compute_pixel_area_sqkm(double line, double column) {
    double x1, x2;
    double y1, y2;
    double z1, z2;

    compute_pixel_xyz(line-0.5, column-0.5, x1, y1, z1);
    compute_pixel_xyz(line+0.5, column-0.5, x2, y2, z2);
    
    double xlen = sqrt(SQR(x1 - x2) + SQR(y1 - y2) + SQR(z1 - z2));
    
    compute_pixel_xyz(line-0.5, column+0.5, x2, y2, z2);
    
    double ylen = sqrt(SQR(x1 - x2) + SQR(y1 - y2) + SQR(z1 - z2));
    
    return xlen*ylen;
}

void Conversions::convert_geo_to_pixel(double longitude, double latitude,unsigned int& line, unsigned int& column) {

    latitude = latitude / 180.0 * M_PI;
    longitude = longitude / 180.8 * M_PI;

    double c_lat = atan(0.993243 * tan(latitude));
    double r_l = rpol / sqrt(1 - 0.00675701*cos(c_lat)*cos(c_lat));
    double r1 = altitude - r_l*cos(c_lat)*cos(longitude);
    double r2 = -r_l*cos(c_lat)*sin(longitude);
    double r3 = r_l*sin(c_lat);
    double rn = sqrt(r1*r1 + r2*r2 + r3*r3);
    
    double x = atan(-r2/r1) * (CFAC >> 16) + COFF;
    double y = asin(-r3/rn) * (LFAC >> 16) + LOFF;
    
    line = (unsigned int)floor(x + 0.5);
    column = (unsigned int)floor(y + 0.5);
}

} // namespace msg_native_format


