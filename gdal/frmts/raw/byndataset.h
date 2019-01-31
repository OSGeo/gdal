/******************************************************************************
 *
 * Project:  Natural Resources Canada's Geoid BYN file format
 * Purpose:  Implementation of BYN format
 * Author:   Ivan Lucena, ivan.lucena@outlook.com
 *
 ******************************************************************************
 * Copyright (c) 2018, Ivan Lucena
 * Copyright (c) 2018, Even Rouault
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

#ifndef GDAL_FRMTS_RAW_BYNDATASET_H_INCLUDED
#define GDAL_FRMTS_RAW_BYNDATASET_H_INCLUDED

#include "cpl_port.h"
#include "rawdataset.h"

/******************************************************************************
                            Format specification
 ******************************************************************************

Table 1: Header description (80 bytes)

#  Variable    Description        Type   Byte Sum Comments/(Units)
--:-----------:------------------:------:----:---:---------------------------
 1 South South Boundary           long   4    4   (arcsec.)
 2 North North Boundary           long   4    8   (arcsec.)
 3 West        West Boundary      long   4    12  (arcsec.)
 4 East        East Boundary      long   4    16  (arcsec.)
 5 DLat        NS Spacing         long   2    18  (arcsec.)
 6 DLon        EW Spacing         short  2    20  (arcsec.)
 7 Global      Global             short  2    22  0: Local/Regional/National grid
                                                  1: Global grid
 8 Type        Type               short  2    24  See Table 2
 9 Factor      Data factor        double 8    32  Transform data from integer to real
10 SizeOf      Data size in bytes short  2    34  2: short integer
                                                  4: long integer
11 VDatum      Vertical Datum     short  2    36  0: Unspecified
                                                  1: CGVD28
                                                  2: CGVD2013
                                                  3: NAVD 88
12             Spare                     6    40  Always zero
13 Data        Data description   short  2    42  0: Data (e.g., N)
                                                  1: Data error estimates (e.g., .N)
                                                  2: Data velocity (e.g., N-dot)
                                                  3: Velocity error estimates (e.g., .N-dot)
14 SubType     Sub-Type           short  2    44  See table 2 below
15 Datum       3-D Ref. Frame     short  2    46  0: ITRF / WGS84
                                                  1: NAD83(CSRS)
16 Ellipsoid   Ellipsoid          short  2    48  See Table 3
17 ByteOrder   Byte Order         short  2    50  0: Big-endian (e.g., HP Unix)
                                                  1: Little-endian (e.g., PC, linux)
18 Scale       Scale Boundaries   short  2    52  0: No scale applied to boundaries and spacing
                                                  1: Scale is applied (x1000)
19 Wo          Geopotential Wo    double 8    60  2ms-2 (e.g., W = 62636856.88)
20 GM          GM                 double 8    68  3ms-2 (e.g., GM = 3.986 E 14)
21 TideSystem  Tidal System       short  2    70  0: Tide free
                                                  1: Mean tide
                                                  2: Zero tide
22 RefRealiz.  Realization (3D)   short  2    72  Version number (e.g., 2005 for ITRF)
23 Epoch       Epoch              float  4    76  Decimal year (e.g., 2007.5)
24 PtType      Node               short  2    78  0: Point
                                                  1: Mean
25 Spare                                 2    80  Always zero
--:-----------:------------------:------:----:---:---------------------------

Items #18 to 22 must be defined if the grid is a geoid model.
 
Table 2: Sub-Type

# Type (item #8)                 # Sub-Type (item #13)
-:------------------------------:-:----------------------------------
0 Undefined                      0 NULL
1 Ellipsoid-Potential separation 0 Geoid Height
                                 1 Height Anomaly
                                 2 Height Transformation (Hybrid)
2 Deflections of the vertical NS 0 NULL
3 Deflections of the vertical EW 0 NULL
4 Gravity                        0 Undefined
                                 1 Absolute (m s**-2 instead of mGal)
                                 2 Free-Air
                                 3 Bouguer
                                 4 Complete Bouguer
                                 5 Helmert
                                 6 Isostatic
5 DEM                            0 MSL (General)
                                 1 Orthometric
                                 2 Normal
                                 3 Dynamic
                                 4 Ellipsoidal
                                 6 Sea Surface Height (SSH) 0 NULL
                                 7 Sea Surface Topography (SST) 0 NULL
                                 8 Ocean current velocity 0 NULL
                                 9 Others 0 NULL
-:------------------------------:-:----------------------------------

Table 3: Ellipsoids

# Name    Semi-major (m)  Inverse flattening GM (m3s2)       Angular velocity (rads s**-1)
-:-------:---------------:-------------------:---------------:-----------------------------
0 GRS80   6378137.0       298.257222101       3986005.0 E 8   7292115 E -11
1 WGS84   6378137.0       298.257223564       3986004.418 E 8 7292115 E -11
2 ALT1    6378136.3       298.256415099       3986004.415 E 8 7292115 E -11
3 GRS67   6378160.0       298.247167427       3986030.0 E 8   7292115.1467 E -11
4 ELLIP1  6378136.46      298.256415099       3986004.415 E 8 7292115 E -11
5 ALT2    6378136.3       298.257             3986004.415 E 8 7292115 E -11
6 ELLIP2  6378136.0       298.257             3986004.4 E 8   7292115 E -11
7 CLARKE  1866 6378206.4  294.9786982         3986004.4 E 8   7292115 E -11

Data (Row x Column x byte size)

The data are stored by rows starting from the north. Each row is stored from the west to 
the east. The data are either short (2 bytes) or long (4 bytes) integers. The size of the 
bytes is defined in the header (item #10). 

The total size of the file is 80 bytes + (Row x Column x (2 or 4) bytes)

where

    Row = (North - South)/nDLat + 1 and
    Column = (East - West)/DLon + 1

Undefined values

    Long int (4-byte data): 9999.0*Factor
    Short int (2 byte data): 32767

 ****************************************************************************/

/************************************************************************/
/*           BYNHeader                                                  */
/************************************************************************/

constexpr int BYN_HDR_SZ = 80; /* != sizeof(BYNHeader) */

/* "Spare" fields are not represented here, no direct read/write */

struct BYNHeader {
    GInt32 nSouth;
    GInt32 nNorth;
    GInt32 nWest;
    GInt32 nEast;
    GInt16 nDLat;
    GInt16 nDLon;
    GInt16 nGlobal;
    GInt16 nType;
    double dfFactor;
    GInt16 nSizeOf;
    GInt16 nVDatum;
    GInt16 nDescrip;
    GInt16 nSubType;
    GInt16 nDatum;
    GInt16 nEllipsoid;
    GInt16 nByteOrder;
    GInt16 nScale;
    double dfWo;
    double dfGM;
    GInt16 nTideSys;
    GInt16 nRealiz;
    float dEpoch;
    GInt16 nPtType;
};

struct BYNEllipsoids {
    const char* pszName;
    double dfSemiMajor;
    double dfInvFlattening;
};

/* Recognizeble EPSG codes */

constexpr int BYN_DATUM_1_VDATUM_2 = 6649;  /* Compounded NAD83(CSRS) + CGVD2013 */
constexpr int BYN_DATUM_0          = 4140;  /* ITRF2008 (GRS80 based WGS84) */
constexpr int BYN_DATUM_1          = 4617;  /* NAD83(CSRS) */
constexpr int BYN_VDATUM_1         = 5713;  /* CGVD28 */
constexpr int BYN_VDATUM_2         = 6647;  /* CGVD2013 */
constexpr int BYN_VDATUM_3         = 6357;  /* NAVD88 */

/* Maximum ordinates values for Identify() */

constexpr GInt32 BYN_SCALE         = 1000;
constexpr GInt32 BYN_MAX_LAT       =   90 * 3600 * 2;
constexpr GInt32 BYN_MAX_LON       =  180 * 3600 * 2;
constexpr GInt32 BYN_MAX_LAT_SCL   =  BYN_MAX_LAT / BYN_SCALE;
constexpr GInt32 BYN_MAX_LON_SCL   =  BYN_MAX_LON / BYN_SCALE;

/************************************************************************/
/* ==================================================================== */
/*                              BYNDataset                              */
/* ==================================================================== */
/************************************************************************/

class BYNDataset final: public RawDataset
{
    friend class BYNRasterBand;

    VSILFILE    *fpImage;
    double      adfGeoTransform[6];
    char*       pszProjection;
    BYNHeader   hHeader;

    void        UpdateHeader();

    CPL_DISALLOW_COPY_ASSIGN(BYNDataset)

    static void header2buffer( const BYNHeader* pohHeader, GByte* pabyBuf );
    static void buffer2header( const GByte* pabyBuf, BYNHeader* pohHeader );

  public:
    BYNDataset();
    ~BYNDataset();

    CPLErr GetGeoTransform( double * padfTransform ) override;
    CPLErr SetGeoTransform( double * padfTransform ) override;
    const char *_GetProjectionRef() override;
    CPLErr _SetProjection( const char* pszProjString ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                           BYNRasterBand                              */
/* ==================================================================== */
/************************************************************************/

class BYNRasterBand final: public RawRasterBand
{
    friend class BYNDataset;

    CPL_DISALLOW_COPY_ASSIGN(BYNRasterBand)

  public:
    BYNRasterBand( GDALDataset *poDS, int nBand, VSILFILE * fpRaw,
                   vsi_l_offset nImgOffset, int nPixelOffset,
                   int nLineOffset,
                   GDALDataType eDataType, int bNativeOrder );
    ~BYNRasterBand() override;

    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    double GetScale( int *pbSuccess = nullptr ) override;
    CPLErr SetScale( double dfNewValue ) override;
};

#endif  // GDAL_FRMTS_RAW_BYNDATASET_H_INCLUDED

