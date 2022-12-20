/******************************************************************************
 *
 * Project:  MSG Native Reader
 * Purpose:  Basic types implementation.
 * Author:   Frans van den Bergh, fvdbergh@csir.co.za
 *
 ******************************************************************************
 * Copyright (c) 2005, Frans van den Bergh <fvdbergh@csir.co.za>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_error.h"
#include "msg_basic_types.h"

#include <stdio.h>

namespace msg_native_format
{

#ifndef SQR
#define SQR(x) ((x) * (x))
#endif

// endian conversion routines
void to_native(GP_PK_HEADER &h)
{
    h.sourceSUId = CPL_MSBWORD32(h.sourceSUId);
    h.sequenceCount = CPL_MSBWORD16(h.sequenceCount);
    h.packetLength = CPL_MSBWORD32(h.packetLength);
}

void to_native(GP_PK_SH1 &h)
{
    h.spacecraftId = CPL_MSBWORD16(h.spacecraftId);
    h.packetTime.day = CPL_MSBWORD16(h.packetTime.day);
    h.packetTime.ms = CPL_MSBWORD32(h.packetTime.ms);
}

void to_native(SUB_VISIRLINE &v)
{
    v.satelliteId = CPL_MSBWORD16(v.satelliteId);
    v.lineNumberInVisirGrid = CPL_MSBWORD32(v.lineNumberInVisirGrid);
}

static void swap_64_bits(unsigned char *b)
{
    CPL_SWAP64PTR(b);
}

void to_native(RADIOMETRIC_PROCESSING_RECORD &r)
{
    for (int i = 0; i < 12; i++)
    {
        swap_64_bits((unsigned char *)&r.level1_5ImageCalibration[i].cal_slope);
        swap_64_bits(
            (unsigned char *)&r.level1_5ImageCalibration[i].cal_offset);
    }
}

static void to_native(REFERENCEGRID_VISIR &r)
{
    r.numberOfLines = CPL_MSBWORD32(r.numberOfLines);
    r.numberOfColumns = CPL_MSBWORD32(r.numberOfColumns);
    // should floats be swapped too?
    float f;

    // convert float using CPL_MSBPTR32
    memcpy(&f, &r.lineDirGridStep, sizeof(f));
    CPL_MSBPTR32(&f);
    r.lineDirGridStep = f;

    // convert float using CPL_MSBPTR32
    memcpy(&f, &r.columnDirGridStep, sizeof(f));
    CPL_MSBPTR32(&f);
    r.columnDirGridStep = f;
}

static void to_native(PLANNED_COVERAGE_VISIR &r)
{
    r.southernLinePlanned = CPL_MSBWORD32(r.southernLinePlanned);
    r.northernLinePlanned = CPL_MSBWORD32(r.northernLinePlanned);
    r.easternColumnPlanned = CPL_MSBWORD32(r.easternColumnPlanned);
    r.westernColumnPlanned = CPL_MSBWORD32(r.westernColumnPlanned);
}

static void to_native(PLANNED_COVERAGE_HRV &r)
{
    r.lowerSouthLinePlanned = CPL_MSBWORD32(r.lowerSouthLinePlanned);
    r.lowerNorthLinePlanned = CPL_MSBWORD32(r.lowerNorthLinePlanned);
    r.lowerEastColumnPlanned = CPL_MSBWORD32(r.lowerEastColumnPlanned);
    r.lowerWestColumnPlanned = CPL_MSBWORD32(r.lowerWestColumnPlanned);
    r.upperSouthLinePlanned = CPL_MSBWORD32(r.upperSouthLinePlanned);
    r.upperNorthLinePlanned = CPL_MSBWORD32(r.upperNorthLinePlanned);
    r.upperEastColumnPlanned = CPL_MSBWORD32(r.upperEastColumnPlanned);
    r.upperWestColumnPlanned = CPL_MSBWORD32(r.upperWestColumnPlanned);
}

void to_native(IMAGE_DESCRIPTION_RECORD &r)
{
    float f;

    // convert float using CPL_MSBPTR32
    memcpy(&f, &r.longitudeOfSSP, sizeof(f));
    CPL_MSBPTR32(&f);
    r.longitudeOfSSP = f;

    to_native(r.referencegrid_visir);
    to_native(r.referencegrid_hrv);
    to_native(r.plannedCoverage_visir);
    to_native(r.plannedCoverage_hrv);
}

void to_native(ACTUAL_L15_COVERAGE_VISIR_RECORD &r)
{
    r.southernLineActual = CPL_MSBWORD32(r.southernLineActual);
    r.northernLineActual = CPL_MSBWORD32(r.northernLineActual);
    r.easternColumnActual = CPL_MSBWORD32(r.easternColumnActual);
    r.westernColumnActual = CPL_MSBWORD32(r.westernColumnActual);
}

void to_native(ACTUAL_L15_COVERAGE_HRV_RECORD &r)
{
    r.lowerSouthLineActual = CPL_MSBWORD32(r.lowerSouthLineActual);
    r.lowerNorthLineActual = CPL_MSBWORD32(r.lowerNorthLineActual);
    r.lowerEastColumnActual = CPL_MSBWORD32(r.lowerEastColumnActual);
    r.lowerWestColumnActual = CPL_MSBWORD32(r.lowerWestColumnActual);
    r.upperSouthLineActual = CPL_MSBWORD32(r.upperSouthLineActual);
    r.upperNorthLineActual = CPL_MSBWORD32(r.upperNorthLineActual);
    r.upperEastColumnActual = CPL_MSBWORD32(r.upperEastColumnActual);
    r.upperWestColumnActual = CPL_MSBWORD32(r.upperWestColumnActual);
}

void to_string(PH_DATA &d)
{
    d.name[29] = 0;
    d.value[49] = 0;
}

#ifdef notdef
// unit tests on structures
bool perform_type_size_check(void)
{
    bool success = true;
    if (sizeof(MAIN_PROD_HEADER) != 3674)
    {
        fprintf(stderr, "MAIN_PROD_HEADER size not 3674 (%lu)\n", /*ok*/
                (unsigned long)sizeof(MAIN_PROD_HEADER));
        success = false;
    }
    if (sizeof(SECONDARY_PROD_HEADER) != 1120)
    {
        fprintf(stderr, "SECONDARY_PROD_HEADER size not 1120 (%lu)\n", /*ok*/
                (unsigned long)sizeof(SECONDARY_PROD_HEADER));
        success = false;
    }
    if (sizeof(SUB_VISIRLINE) != 27)
    {
        fprintf(stderr, "SUB_VISIRLINE size not 17 (%lu)\n", /*ok*/
                (unsigned long)sizeof(SUB_VISIRLINE));
        success = false;
    }
    if (sizeof(GP_PK_HEADER) != 22)
    {
        fprintf(stderr, "GP_PK_HEADER size not 22 (%lu)\n", /*ok*/
                (unsigned long)sizeof(GP_PK_HEADER));
        success = false;
    }
    if (sizeof(GP_PK_SH1) != 16)
    {
        fprintf(stderr, "GP_PK_SH1 size not 16 (%lu)\n", /*ok*/
                (unsigned long)sizeof(GP_PK_SH1));
        success = false;
    }
    return success;
}
#endif

const double Conversions::altitude = 42164;  // km from origin
// the spheroid in CGMS 03 4.4.3.2 is unique - flattening is 1/295.488
// note the req and rpol were revised in issue 2.8 of CGMS/DOC/12/0017 - these
// are the revised values
const double Conversions::req = 6378.1370;   // earth equatorial radius
const double Conversions::rpol = 6356.7523;  // earth polar radius

// replace the magic constants in the paper with the calculated values in case
// of further change
const double Conversions::dtp2 =
    (SQR(altitude) -
     SQR(req));  // square of the distance to the equatorial tangent point
                 // first/last point sensed on the equator

// given req and rpol, oblate is already defined. Unused afaik in the gdal code
const double Conversions::oblate = ((req - rpol) / req);  // oblateness of earth
const double Conversions::eccentricity2 =
    (1.0 - (SQR(rpol) / SQR(req)));  // 0.00669438...
const double Conversions::ratio2 =
    SQR(rpol / req);  // 0.9933056   1/x = 1.006739501
const double Conversions::deg_to_rad = (M_PI / 180.0);
const double Conversions::rad_to_deg = (180.0 / M_PI);
const double Conversions::nlines = 3712;  // number of lines in an image
const double Conversions::step =
    17.83 / nlines;  // pixel / line step in degrees

const int Conversions::CFAC = -781648343;
const int Conversions::LFAC = -781648343;
const int Conversions::COFF = 1856;
const int Conversions::LOFF = 1856;
const double Conversions::CFAC_scaled = ((double)CFAC / (1 << 16));
const double Conversions::LFAC_scaled = ((double)LFAC / (1 << 16));

#define SQR(x) ((x) * (x))

void Conversions::convert_pixel_to_geo(double line, double column,
                                       double &longitude, double &latitude)
{
    // x and y are angles in radians
    double x = (column - COFF - 0.0) / CFAC_scaled;
    double y = (line - LOFF - 0.0) / LFAC_scaled;

    double sd = sqrt(SQR(altitude * cos(x) * cos(y)) -
                     (SQR(cos(y)) + SQR(sin(y)) / ratio2) * dtp2);
    double sn = (altitude * cos(x) * cos(y) - sd) /
                (SQR(cos(y)) + SQR(sin(y)) / ratio2);
    double s1 = altitude - sn * SQR(cos(y));
    double s2 = sn * sin(x) * cos(y);
    double s3 = -sn * sin(y);
    double sxy = sqrt(s1 * s1 + s2 * s2);

    longitude = atan(s2 / s1);
    latitude = atan((s3 / sxy) / ratio2);

    longitude = longitude * rad_to_deg;
    latitude = latitude * rad_to_deg;
}

void Conversions::compute_pixel_xyz(double line, double column, double &x,
                                    double &y, double &z)
{
    double asamp = -(column - (nlines / 2.0 + 0.5)) * step;
    double aline = (line - (nlines / 2.0 + 0.5)) * step;

    asamp *= deg_to_rad;
    aline *= deg_to_rad;

    double tanal = tan(aline);
    double tanas = tan(asamp);

    double p = -1;
    double q = tanas;
    double r = tanal * sqrt(1 + q * q);

    double a = q * q + SQR((r * req / rpol)) + p * p;
    double b = 2 * altitude * p;
    double c = altitude * altitude - req * req;

    double det = b * b - 4 * a * c;

    if (det > 0)
    {
        double k = (-b - sqrt(det)) / (2 * a);
        x = altitude + k * p;
        y = k * q;
        z = k * r;
    }
    else
    {
        x = y = z = 0;
        CPLError(CE_Warning, CPLE_AppDefined, "Warning: pixel not visible");
    }
}

double Conversions::compute_pixel_area_sqkm(double line, double column)
{
    double x1, x2;
    double y1, y2;
    double z1, z2;

    compute_pixel_xyz(line - 0.5, column - 0.5, x1, y1, z1);
    compute_pixel_xyz(line + 0.5, column - 0.5, x2, y2, z2);

    double xlen = sqrt(SQR(x1 - x2) + SQR(y1 - y2) + SQR(z1 - z2));

    compute_pixel_xyz(line - 0.5, column + 0.5, x2, y2, z2);

    double ylen = sqrt(SQR(x1 - x2) + SQR(y1 - y2) + SQR(z1 - z2));

    return xlen * ylen;
}

void Conversions::convert_geo_to_pixel(double longitude, double latitude,
                                       unsigned int &line, unsigned int &column)
{

    latitude = latitude * deg_to_rad;
    longitude = longitude * deg_to_rad;

    double c_lat = atan(ratio2 * tan(latitude));
    double r_l = rpol / sqrt(1 - eccentricity2 * cos(c_lat) * cos(c_lat));
    double r1 = altitude - r_l * cos(c_lat) * cos(longitude);
    double r2 = -r_l * cos(c_lat) * sin(longitude);
    double r3 = r_l * sin(c_lat);
    double rn = sqrt(r1 * r1 + r2 * r2 + r3 * r3);

    double x = atan(-r2 / r1) * CFAC_scaled + COFF;
    double y = asin(-r3 / rn) * LFAC_scaled + LOFF;

    line = (unsigned int)floor(x + 0.5);
    column = (unsigned int)floor(y + 0.5);
}

}  // namespace msg_native_format
