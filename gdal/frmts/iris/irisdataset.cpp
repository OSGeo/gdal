/******************************************************************************
 *
 * Project:  IRIS Reader
 * Purpose:  All code for IRIS format Reader
 * Author:   Roger Veciana, rveciana@gmail.com
 *           Portions are adapted from code copyright (C) 2005-2012
 *           Chris Veness under a CC-BY 3.0 licence
 *
 ******************************************************************************
 * Copyright (c) 2012, Roger Veciana <rveciana@gmail.com>
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <sstream>

CPL_CVSID("$Id$")

static double DEG2RAD = M_PI / 180.0;
static double RAD2DEG = 180.0 / M_PI;

/************************************************************************/
/* ==================================================================== */
/*                                  IRISDataset                         */
/* ==================================================================== */
/************************************************************************/

class IRISRasterBand;

class IRISDataset : public GDALPamDataset
{
    friend class IRISRasterBand;

    VSILFILE              *fp;
    GByte                 abyHeader[640];
    bool                  bNoDataSet;
    double                dfNoDataValue;
    static const char* const   aszProductNames[];
    static const char* const   aszDataTypeCodes[];
    static const char* const   aszDataTypes[];
    static const char* const   aszProjections[];
    unsigned short        nProductCode;
    unsigned short        nDataTypeCode;
    unsigned char         nProjectionCode;
    float                 fNyquistVelocity;
    char*                 pszSRS_WKT;
    double                adfGeoTransform[6];
    bool                  bHasLoadedProjection;
    void                  LoadProjection();
    static std::pair<double, double> GeodesicCalculation(
        float fLat, float fLon, float fAngle, float fDist,
        float fEquatorialRadius, float fPolarRadius, float fFlattening );

public:
    IRISDataset();
    virtual ~IRISDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    CPLErr GetGeoTransform( double * padfTransform ) override;
    const char *GetProjectionRef() override;
};

const char* const IRISDataset::aszProductNames[] = {
    "", "PPI", "RHI", "CAPPI", "CROSS", "TOPS", "TRACK", "RAIN1", "RAINN",
    "VVP", "VIL", "SHEAR", "WARN", "CATCH", "RTI", "RAW", "MAX", "USER",
    "USERV", "OTHER", "STATUS", "SLINE", "WIND", "BEAM", "TEXT", "FCAST",
    "NDOP", "IMAGE", "COMP", "TDWR", "GAGE", "DWELL", "SRI", "BASE", "HMAX"};

const char* const IRISDataset::aszDataTypeCodes[] = {
    "XHDR", "DBT" ,"dBZ", "VEL", "WIDTH", "ZDR", "ORAIN", "dBZC", "DBT2",
    "dBZ2", "VEL2", "WIDTH2", "ZDR2", "RAINRATE2", "KDP", "KDP2", "PHIDP",
    "VELC", "SQI", "RHOHV", "RHOHV2", "dBZC2", "VELC2", "SQI2", "PHIDP2",
    "LDRH", "LDRH2", "LDRV", "LDRV2", "FLAGS", "FLAGS2", "FLOAT32", "HEIGHT",
    "VIL2", "NULL", "SHEAR", "DIVERGE2", "FLIQUID2", "USER", "OTHER", "DEFORM2",
    "VVEL2", "HVEL2", "HDIR2", "AXDIL2", "TIME2", "RHOH", "RHOH2", "RHOV",
    "RHOV2", "PHIH", "PHIH2", "PHIV", "PHIV2", "USER2", "HCLASS", "HCLASS2",
    "ZDRC", "ZDRC2", "TEMPERATURE16", "VIR16", "DBTV8", "DBTV16", "DBZV8",
    "DBZV16", "SNR8", "SNR16", "ALBEDO8", "ALBEDO16", "VILD16", "TURB16"};

const char* const IRISDataset::aszDataTypes[] = {
    "Extended Headers", "Total H power (1 byte)",
    "Clutter Corrected H reflectivity (1 byte)",
    "Velocity (1 byte)", "Width (1 byte)", "Differential reflectivity (1 byte)",
    "Old Rainfall rate (stored as dBZ)",
    "Fully corrected reflectivity (1 byte)",
    "Uncorrected reflectivity (2 byte)", "Corrected reflectivity (2 byte)",
    "Velocity (2 byte)","Width (2 byte)","Differential reflectivity (2 byte)",
    "Rainfall rate (2 byte)","Kdp (specific differential phase)(1 byte)",
    "Kdp (specific differential phase)(2 byte)","PHIdp (differential phase)(1 byte)",
    "Corrected Velocity (1 byte)","SQI (1 byte)","RhoHV(0) (1 byte)","RhoHV(0) (2 byte)",
    "Fully corrected reflectivity (2 byte)","Corrected Velocity (2 byte)","SQI (2 byte)",
    "PHIdp (differential phase)(2 byte)","LDR H to V (1 byte)","LDR H to V (2 byte)",
    "LDR V to H (1 byte)","LDR V to H (2 byte)","Individual flag bits for each bin","",
    "Test of floating format", "Height (1/10 km) (1 byte)", "Linear liquid (.001mm) (2 byte)",
    "Data type is not applicable", "Wind Shear (1 byte)", "Divergence (.001 10**-4) (2-byte)",
    "Floated liquid (2 byte)", "User type, unspecified data (1 byte)",
    "Unspecified data, no color legend", "Deformation (.001 10**-4) (2-byte)",
    "Vertical velocity (.01 m/s) (2-byte)", "Horizontal velocity (.01 m/s) (2-byte)",
    "Horizontal wind direction (.1 degree) (2-byte)", "Axis of Dillitation (.1 degree) (2-byte)",
    "Time of data (seconds) (2-byte)", "Rho H to V (1 byte)", "Rho H to V (2 byte)",
    "Rho V to H (1 byte)", "Rho V to H (2 byte)", "Phi H to V (1 byte)", "Phi H to V (2 byte)",
    "Phi V to H (1 byte)", "Phi V to H (2 byte)", "User type, unspecified data (2 byte)",
    "Hydrometeor class (1 byte)", "Hydrometeor class (2 byte)", "Corrected Differential reflectivity (1 byte)",
    "Corrected Differential reflectivity (2 byte)", "Temperature (2 byte)",
    "Vertically Integrated Reflectivity (2 byte)", "Total V Power (1 byte)", "Total V Power (2 byte)",
    "Clutter Corrected V Reflectivity (1 byte)", "Clutter Corrected V Reflectivity (2 byte)",
    "Signal to Noise ratio (1 byte)", "Signal to Noise ratio (2 byte)", "Albedo (1 byte)",
    "Albedo (2 byte)", "VIL Density (2 byte)", "Turbulence (2 byte)"};

const char* const IRISDataset::aszProjections[] = {
    "Azimutal equidistant","Mercator","Polar Stereographic", "UTM",
    // FIXME: is it a typo here or in IRIS itself: Perspective or Prespective ?
    "Perspective from geosync", "Equidistant cylindrical", "Gnomonic",
    "Gauss conformal", "Lambert conformal conic"};

/************************************************************************/
/* ==================================================================== */
/*                            IRISRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class IRISRasterBand : public GDALPamRasterBand
{
    friend class IRISDataset;

    unsigned char*        pszRecord;
    bool                  bBufferAllocFailed;

public:
    IRISRasterBand( IRISDataset *, int );
    virtual ~IRISRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;

    virtual double          GetNoDataValue( int * ) override;
    virtual CPLErr          SetNoDataValue( double ) override;
};

/************************************************************************/
/*                           IRISRasterBand()                           */
/************************************************************************/

IRISRasterBand::IRISRasterBand( IRISDataset *poDSIn, int nBandIn ) :
    pszRecord(NULL),
    bBufferAllocFailed(false)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Float32;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

IRISRasterBand::~IRISRasterBand()
{
    VSIFree(pszRecord);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr IRISRasterBand::IReadBlock( int /* nBlockXOff */,
                                   int nBlockYOff,
                                   void * pImage )

{
    IRISDataset *poGDS = static_cast<IRISDataset *>(poDS);

    // Every product type has its own size. TODO: Move it like dataType.
    int nDataLength = 1;
    if( poGDS->nDataTypeCode == 2 ) nDataLength = 1;
    else if( poGDS->nDataTypeCode == 8 ) nDataLength = 2;
    else if( poGDS->nDataTypeCode == 9 ) nDataLength = 2;
    else if( poGDS->nDataTypeCode == 37 ) nDataLength = 2;
    else if( poGDS->nDataTypeCode == 33 ) nDataLength = 2;
    else if( poGDS->nDataTypeCode == 32 ) nDataLength = 1;

    // We allocate space for storing a record:
    if( pszRecord == NULL )
    {
        if( bBufferAllocFailed )
            return CE_Failure;

        pszRecord = static_cast<unsigned char *>(
            VSI_MALLOC_VERBOSE(nBlockXSize*nDataLength));

        if( pszRecord == NULL )
        {
            bBufferAllocFailed = true;
            return CE_Failure;
        }
    }

    // Prepare to read (640 is the header size in bytes) and read (the
    // y axis in the IRIS files in the inverse direction).  The
    // previous bands are also added as an offset

    VSIFSeekL( poGDS->fp,
               640 +
               static_cast<vsi_l_offset>(nDataLength)*poGDS->GetRasterXSize()*poGDS->GetRasterYSize()*(this->nBand-1) +
               static_cast<vsi_l_offset>(nBlockXSize)*nDataLength*(poGDS->GetRasterYSize()-1-nBlockYOff), SEEK_SET );

    if( static_cast<int>(VSIFReadL( pszRecord, nBlockXSize*nDataLength, 1,
                                    poGDS->fp )) != 1 )
        return CE_Failure;

    // If datatype is dbZ or dBT:
    // See point 3.3.3 at page 3.33 of the manual.
    if( poGDS->nDataTypeCode == 2 || poGDS->nDataTypeCode == 1 )
    {
        for( int i = 0; i < nBlockXSize; i++)
        {
            float fVal = ((*(pszRecord + i * nDataLength)) - 64.0f) / 2.0f;
            if( fVal == 95.5f )
                fVal = -9999.0f;
            ((float *) pImage)[i] = fVal;
        }
    // If datatype is dbZ2 or dBT2:
    // See point 3.3.4 at page 3.33 of the manual.
    }
    else if( poGDS->nDataTypeCode == 8 || poGDS->nDataTypeCode == 9 )
    {
        for( int i = 0; i < nBlockXSize; i++)
        {
            float fVal =
                (CPL_LSBUINT16PTR(pszRecord + i * nDataLength) - 32768.0f) /
                100.0f;
            if( fVal == 327.67f )
                fVal = -9999.0f;
            ((float *) pImage)[i] = fVal;
        }
    // Fliquid2 (Rain1 & Rainn products)
    // See point 3.3.11 at page 3.43 of the manual.
    }
    else if( poGDS->nDataTypeCode == 37 )
    {
        for( int i = 0; i < nBlockXSize; i++)
        {
            const unsigned short nVal =
                CPL_LSBUINT16PTR(pszRecord+i*nDataLength);
            const unsigned short nExp = nVal>>12;
            const unsigned short nMantissa = nVal - (nExp<<12);
            float fVal2 = 0.0f;
            if( nVal == 65535 )
                fVal2 = -9999.0f;
            else if( nExp == 0 )
                fVal2 = nMantissa / 1000.0f;
            else
                fVal2 = ((nMantissa + 4096) << (nExp - 1)) / 1000.0f;
            ((float *) pImage)[i] = fVal2;
        }
    // VIL2 (VIL products)
    // See point 3.3.41 at page 3.54 of the manual.
    }
    else if( poGDS->nDataTypeCode == 33 )
    {
        for( int i = 0; i < nBlockXSize; i++)
        {
            float fVal =
                static_cast<float>(CPL_LSBUINT16PTR(pszRecord+i*nDataLength));
            if( fVal == 65535.0f )
                ((float *) pImage)[i] = -9999.0f;
            else if( fVal == 0.0f )
                ((float *) pImage)[i] = -1.0f;
            else
                ((float *) pImage)[i] = (fVal-1) / 1000.0f;
        }
    // HEIGHT (TOPS products)
    // See point 3.3.14 at page 3.46 of the manual.
    }
    else if( poGDS->nDataTypeCode == 32 )
    {
        for( int i = 0; i < nBlockXSize; i++)
        {
            unsigned char nVal = *(pszRecord+i*nDataLength);
            if( nVal == 255 )
                ((float *) pImage)[i] = -9999.0f;
            else if( nVal == 0 )
                ((float *) pImage)[i] = -1.0f;
            else
                ((float *) pImage)[i] = (nVal - 1.0f) / 10.0f;
        }
    // VEL (Velocity 1-Byte in PPI & others)
    // See point 3.3.37 at page 3.53 of the manual.
    }
    else if( poGDS->nDataTypeCode == 3 )
    {
        for( int i = 0; i < nBlockXSize; i++)
        {
            float fVal = static_cast<float>(*(pszRecord+i*nDataLength));
            if( fVal == 0.0f )
                fVal = -9997.0f;
            else if( fVal == 1.0f )
                fVal = -9998.0f;
            else if( fVal == 255.0f )
                fVal = -9999.0f;
            else
                fVal = poGDS->fNyquistVelocity * (fVal - 128.0f)/127.0f;
            ((float *) pImage)[i] = fVal;
        }
    // SHEAR (1-Byte Shear)
    // See point 3.3.23 at page 3.39 of the manual.
    }
    else if( poGDS->nDataTypeCode == 35 )
    {
        for( int i = 0; i < nBlockXSize; i++)
        {
            float fVal = static_cast<float>(*(pszRecord+i*nDataLength));
            if( fVal == 0.0f )
                fVal = -9998.0f;
            else if( fVal == 255.0f )
                fVal = -9999.0f;
            else
                fVal = (fVal - 128.0f) * 0.2f;
            ((float *) pImage)[i] = fVal;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr IRISRasterBand::SetNoDataValue( double dfNoData )

{
    IRISDataset *poGDS = static_cast<IRISDataset *>(poDS);

    poGDS->bNoDataSet = true;
    poGDS->dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double IRISRasterBand::GetNoDataValue( int * pbSuccess )

{
    IRISDataset *poGDS = static_cast<IRISDataset *>(poDS);

    if( poGDS->bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return poGDS->dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/* ==================================================================== */
/*                              IRISDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            IRISDataset()                             */
/************************************************************************/

IRISDataset::IRISDataset() :
    fp(NULL),
    bNoDataSet(false),
    dfNoDataValue(0.0),
    nProductCode(0),
    nDataTypeCode(0),
    nProjectionCode(0),
    fNyquistVelocity(0.0),
    pszSRS_WKT(NULL),
    bHasLoadedProjection(false)
{
    std::fill_n(abyHeader, CPL_ARRAYSIZE(abyHeader), 0);
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~IRISDataset()                             */
/************************************************************************/

IRISDataset::~IRISDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
    CPLFree( pszSRS_WKT );
}

/************************************************************************/
/*           Calculates the projection and Geotransform                 */
/************************************************************************/
void IRISDataset::LoadProjection()
{
    bHasLoadedProjection = true;
    // They give the radius in cm.
    float fEquatorialRadius = CPL_LSBUINT32PTR(abyHeader+220+320+12) / 100.0f;
    // Point 3.2.27 pag 3-15.
    float fInvFlattening = CPL_LSBUINT32PTR(abyHeader+224+320+12) / 1000000.0f;
    float fFlattening = 0.0f;
    float fPolarRadius = 0.0f;

    if( fEquatorialRadius == 0.0f )
    {
        // If Radius is 0, change to 6371000 Point 3.2.27 pag 3-15 (old IRIS
        // versions).
        fEquatorialRadius = 6371000.0f;
        fPolarRadius = fEquatorialRadius;
        fInvFlattening = 0.0f;
        fFlattening = 0.0f;
    }
    else
    {
        if( fInvFlattening == 0.0f )
        {
            // When inverse flattening is infinite, they use 0.
            fFlattening = 0.0f;
            fPolarRadius = fEquatorialRadius;
        }
        else
        {
            fFlattening = 1.0f / fInvFlattening;
            fPolarRadius = fEquatorialRadius * (1.0f - fFlattening);
        }
    }

    // TODO(schwehr): Document 4294967295.
    const float fCenterLon =
        static_cast<float>(CPL_LSBUINT32PTR(abyHeader+112+320+12) * 360.0 / 4294967295UL);
    const float fCenterLat =
        static_cast<float>(CPL_LSBUINT32PTR(abyHeader+108+320+12) * 360.0 / 4294967295UL);

    const float fProjRefLon =
        static_cast<float>(CPL_LSBUINT32PTR(abyHeader+244+320+12) * 360.0 / 4294967295UL);
    const float fProjRefLat =
        static_cast<float>(CPL_LSBUINT32PTR(abyHeader+240+320+12) * 360.0 / 4294967295UL);

    const float fRadarLocX = CPL_LSBSINT32PTR(abyHeader + 112 + 12 ) / 1000.0f;
    const float fRadarLocY = CPL_LSBSINT32PTR(abyHeader + 116 + 12 ) / 1000.0f;

    const float fScaleX = CPL_LSBSINT32PTR(abyHeader + 88 + 12 ) / 100.0f;
    const float fScaleY = CPL_LSBSINT32PTR(abyHeader + 92 + 12 ) / 100.0f;

    OGRSpatialReference oSRSOut;

    // Mercator projection.
    if( EQUAL(aszProjections[nProjectionCode],"Mercator") )
    {
        oSRSOut.SetGeogCS(
            "unnamed ellipse",
            "unknown",
            "unnamed",
            fEquatorialRadius, fInvFlattening,
            "Greenwich", 0.0,
            "degree", 0.0174532925199433);

        oSRSOut.SetMercator(fProjRefLat, fProjRefLon, 1.0, 0.0, 0.0);
        oSRSOut.exportToWkt(&pszSRS_WKT);

        // The center coordinates are given in LatLon on the defined
        // ellipsoid. Necessary to calculate geotransform.

        OGRSpatialReference oSRSLatLon;
        oSRSLatLon.SetGeogCS(
            "unnamed ellipse",
            "unknown",
            "unnamed",
            fEquatorialRadius, fInvFlattening,
            "Greenwich", 0.0,
            "degree", 0.0174532925199433);

        OGRCoordinateTransformation *poTransform =
            OGRCreateCoordinateTransformation( &oSRSLatLon, &oSRSOut );

        const std::pair<double, double> oPositionX2 =
            GeodesicCalculation(
                fCenterLat, fCenterLon, 90.0f, fScaleX,
                fEquatorialRadius, fPolarRadius, fFlattening);
        const std::pair<double, double> oPositionY2 =
            GeodesicCalculation(
                fCenterLat, fCenterLon, 0.0f, fScaleY,
                fEquatorialRadius, fPolarRadius, fFlattening);

        const double dfLon2 = oPositionX2.first;
        const double dfLat2 = oPositionY2.second;

        double dfX = fCenterLon ;
        double dfY = fCenterLat ;
        if( poTransform == NULL || !poTransform->Transform( 1, &dfX, &dfY ) )
             CPLError( CE_Failure, CPLE_None, "Transformation Failed" );

        double dfX2 = dfLon2;
        double dfY2 = dfLat2;
        if( poTransform == NULL || !poTransform->Transform( 1, &dfX2, &dfY2 ) )
             CPLError( CE_Failure, CPLE_None, "Transformation Failed" );

        adfGeoTransform[0] = dfX - (fRadarLocX * (dfX2 - dfX));
        adfGeoTransform[1] = dfX2 - dfX;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = dfY + (fRadarLocY * (dfY2 - dfY));
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -1*(dfY2 - dfY);

        delete poTransform;
    }
    else if( EQUAL(aszProjections[nProjectionCode],"Azimutal equidistant") )
    {
        oSRSOut.SetGeogCS(
            "unnamed ellipse",
            "unknown",
            "unnamed",
            fEquatorialRadius, fInvFlattening,
            "Greenwich", 0.0,
            "degree", 0.0174532925199433);
        oSRSOut.SetAE(fProjRefLat, fProjRefLon, 0.0, 0.0);
        oSRSOut.exportToWkt(&pszSRS_WKT) ;
        adfGeoTransform[0] = -1*(fRadarLocX*fScaleX);
        adfGeoTransform[1] = fScaleX;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = fRadarLocY*fScaleY;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -1*fScaleY;
        // When the projection is different from Mercator or Azimutal
        // equidistant, we set a standard geotransform.
    }
    else
    {
        adfGeoTransform[0] = -1*(fRadarLocX*fScaleX);
        adfGeoTransform[1] = fScaleX;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = fRadarLocY*fScaleY;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -1*fScaleY;
    }
}

/******************************************************************************/
/* The geotransform in Mercator projection must be calculated transforming    */
/* distance to degrees over the ellipsoid, using Vincenty's formula.          */
/* The following method is ported from a version for Javascript by Chris      */
/* Veness distributed under a CC-BY 3.0 licence, whose conditions is that the */
/* following copyright notice is retained as well as the link to :            */
/* http://www.movable-type.co.uk/scripts/latlong-vincenty-direct.html         */
/******************************************************************************/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
/* Vincenty Direct Solution of Geodesics on the Ellipsoid (c) Chris Veness 2005-2012              */
/*                                                                                                */
/* from: Vincenty direct formula - T Vincenty, "Direct and Inverse Solutions of Geodesics on the  */
/*       Ellipsoid with application of nested equations", Survey Review, vol XXII no 176, 1975    */
/*       http://www.ngs.noaa.gov/PUBS_LIB/inverse.pdf                                             */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

std::pair<double, double>
IRISDataset::GeodesicCalculation(
    float fLat, float fLon, float fAngle, float fDist, float fEquatorialRadius,
    float fPolarRadius, float fFlattening )
{
    const double dfAlpha1 = DEG2RAD * fAngle;
    const double dfSinAlpha1 = sin(dfAlpha1);
    const double dfCosAlpha1 = cos(dfAlpha1);

    const double dfTanU1 = (1-fFlattening) * tan(fLat*DEG2RAD);
    const double dfCosU1 = 1 / sqrt((1 + dfTanU1*dfTanU1));
    const double dfSinU1 = dfTanU1*dfCosU1;

    const double dfSigma1 = atan2(dfTanU1, dfCosAlpha1);
    const double dfSinAlpha = dfCosU1 * dfSinAlpha1;
    const double dfCosSqAlpha = 1 - dfSinAlpha*dfSinAlpha;
    const double dfUSq =
        dfCosSqAlpha * (fEquatorialRadius*fEquatorialRadius -
                        fPolarRadius*fPolarRadius) /
        (fPolarRadius*fPolarRadius);
    const double dfA =
        1 + dfUSq/16384*(4096+dfUSq*(-768+dfUSq*(320-175*dfUSq)));
    const double dfB = dfUSq/1024 * (256+dfUSq*(-128+dfUSq*(74-47*dfUSq)));

    double dfSigma = fDist / (fPolarRadius*dfA);
    double dfSigmaP = 2*M_PI;

    double dfSinSigma = 0.0;
    double dfCosSigma = 0.0;
    double dfCos2SigmaM = 0.0;

    while( fabs(dfSigma-dfSigmaP) > 1e-12 )
    {
        dfCos2SigmaM = cos(2*dfSigma1 + dfSigma);
        dfSinSigma = sin(dfSigma);
        dfCosSigma = cos(dfSigma);
        const double dfDeltaSigma =
            dfB*dfSinSigma*(dfCos2SigmaM+dfB/4*(
                dfCosSigma*(-1+2*dfCos2SigmaM*dfCos2SigmaM) -
                dfB/6*dfCos2SigmaM*(-3+4*dfSinSigma*dfSinSigma)*
                (-3+4*dfCos2SigmaM*dfCos2SigmaM)));
        dfSigmaP = dfSigma;
        dfSigma = fDist / (fPolarRadius*dfA) + dfDeltaSigma;
    }

    const double dfTmp = dfSinU1*dfSinSigma - dfCosU1*dfCosSigma*dfCosAlpha1;
    const double dfLat2 =
        atan2(dfSinU1*dfCosSigma + dfCosU1*dfSinSigma*dfCosAlpha1,
              (1-fFlattening)*sqrt(dfSinAlpha*dfSinAlpha + dfTmp*dfTmp));
    const double dfLambda =
        atan2(dfSinSigma*dfSinAlpha1,
              dfCosU1*dfCosSigma - dfSinU1*dfSinSigma*dfCosAlpha1);
    const double dfC =
        fFlattening/16*dfCosSqAlpha*(4+fFlattening*(4-3*dfCosSqAlpha));
    const double dfL = dfLambda - (1-dfC) * fFlattening * dfSinAlpha *
        (dfSigma + dfC*dfSinSigma*(dfCos2SigmaM+dfC*dfCosSigma*
                                   (-1+2*dfCos2SigmaM*dfCos2SigmaM)));
    double dfLon2 = fLon*DEG2RAD+dfL;

    if( dfLon2 > M_PI )
        dfLon2 = dfLon2 - 2 * M_PI;
    if( dfLon2 < -1*M_PI )
        dfLon2 = dfLon2 + 2 * M_PI;

    std::pair<double, double> oOutput(dfLon2 * RAD2DEG, dfLat2 * RAD2DEG);

    return oOutput;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr IRISDataset::GetGeoTransform( double * padfTransform )

{
    if( !bHasLoadedProjection )
        LoadProjection();
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *IRISDataset::GetProjectionRef()
{
    if( !bHasLoadedProjection )
        LoadProjection();
    return pszSRS_WKT;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int IRISDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Confirm that the file is an IRIS file                           */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 640 )
        return FALSE;

    const short nId1 = CPL_LSBSINT16PTR(poOpenInfo->pabyHeader);
    const short nId2 = CPL_LSBSINT16PTR(poOpenInfo->pabyHeader + 12);
    unsigned short nType = CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 24);

    // Check if the two headers are 27 (product hdr) & 26 (product
    // configuration), and the product type is in the range 1 -> 34.
    if( !(nId1 == 27 && nId2 == 26 && nType > 0 && nType < 35) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                             FillString()                             */
/************************************************************************/

static void FillString( char* szBuffer, size_t nBufferSize, void* pSrcBuffer )
{
    for( size_t i = 0; i < nBufferSize - 1; i++ )
        szBuffer[i] = static_cast<char *>(pSrcBuffer)[i];
    szBuffer[nBufferSize-1] = '\0';
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *IRISDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
        return NULL;
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The IRIS driver does not support update access to existing"
                  " datasets." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    IRISDataset *poDS = new IRISDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if( poDS->fp == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFReadL( poDS->abyHeader, 1, 640, poDS->fp );
    const int nXSize = CPL_LSBSINT32PTR(poDS->abyHeader+100+12);
    const int nYSize = CPL_LSBSINT32PTR(poDS->abyHeader+104+12);
    const int nNumBands = CPL_LSBSINT32PTR(poDS->abyHeader+108+12);

    poDS->nRasterXSize = nXSize;

    poDS->nRasterYSize = nYSize;
    if( poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid dimensions : %d x %d",
                  poDS->nRasterXSize, poDS->nRasterYSize);
        delete poDS;
        return NULL;
    }

    if( !GDALCheckBandCount(nNumBands, TRUE) )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setting the Metadata                                            */
/* -------------------------------------------------------------------- */
    // See point 3.2.26 at page 3.12 of the manual.
    poDS->nProductCode = CPL_LSBUINT16PTR(poDS->abyHeader + 12 + 12);
    poDS->SetMetadataItem( "PRODUCT_ID",
                           CPLString().Printf("%d", poDS->nProductCode ));
    if( poDS->nProductCode >= CPL_ARRAYSIZE(aszProductNames) )
    {
        delete poDS;
        return NULL;
    }

    poDS->SetMetadataItem("PRODUCT", aszProductNames[poDS->nProductCode]);

    poDS->nDataTypeCode = CPL_LSBUINT16PTR(poDS->abyHeader+130+12);
    if( poDS->nDataTypeCode >= CPL_ARRAYSIZE(aszDataTypeCodes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem("DATA_TYPE_CODE",
                          aszDataTypeCodes[poDS->nDataTypeCode]);

    if( poDS->nDataTypeCode >= CPL_ARRAYSIZE(aszDataTypes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem("DATA_TYPE",
                          aszDataTypes[poDS->nDataTypeCode]);

    const unsigned short nDataTypeInputCode =
        CPL_LSBUINT16PTR(poDS->abyHeader+144+12);
    if( nDataTypeInputCode >= CPL_ARRAYSIZE(aszDataTypeCodes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem("DATA_TYPE_INPUT_CODE",
                          aszDataTypeCodes[nDataTypeInputCode]);

    const unsigned short nDataTypeInput =
        CPL_LSBUINT16PTR(poDS->abyHeader+144+12);
    if( nDataTypeInput >= CPL_ARRAYSIZE(aszDataTypes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem("DATA_TYPE_INPUT",
                          aszDataTypes[nDataTypeInput]);

    poDS->nProjectionCode =
        * static_cast<unsigned char *>(poDS->abyHeader+146+12);
    if( poDS->nProjectionCode >= CPL_ARRAYSIZE(aszProjections) )
    {
        delete poDS;
        return NULL;
    }

    // Times.
    {
        const int nSeconds = CPL_LSBSINT32PTR(poDS->abyHeader+20+12);

        const int nHour = (nSeconds - (nSeconds % 3600)) / 3600;
        const int nMinute =
            ((nSeconds - nHour * 3600) - (nSeconds - nHour * 3600) % 60) / 60;
        const int nSecond = nSeconds - nHour * 3600 - nMinute * 60;

        const short nYear = CPL_LSBSINT16PTR(poDS->abyHeader+26+12);
        const short nMonth = CPL_LSBSINT16PTR(poDS->abyHeader+28+12);
        const short nDay = CPL_LSBSINT16PTR(poDS->abyHeader+30+12);

        poDS->SetMetadataItem(
            "TIME_PRODUCT_GENERATED",
            CPLString().Printf("%d-%02d-%02d %02d:%02d:%02d",
                               nYear, nMonth, nDay, nHour, nMinute, nSecond));
    }

    {
        const int nSeconds = CPL_LSBSINT32PTR(poDS->abyHeader+32+12);

        const int nHour = (nSeconds - (nSeconds % 3600)) / 3600;
        const int nMinute =
            ((nSeconds - nHour * 3600) - (nSeconds - nHour * 3600) % 60) / 60;
        const int nSecond = nSeconds - nHour * 3600 - nMinute * 60;

        const short nYear = CPL_LSBSINT16PTR(poDS->abyHeader+26+12);
        const short nMonth = CPL_LSBSINT16PTR(poDS->abyHeader+28+12);
        const short nDay = CPL_LSBSINT16PTR(poDS->abyHeader+30+12);

        poDS->SetMetadataItem(
            "TIME_INPUT_INGEST_SWEEP",
            CPLString().Printf("%d-%02d-%02d %02d:%02d:%02d",
                               nYear, nMonth, nDay, nHour, nMinute, nSecond));
    }

    // Site and task information.

    char szSiteName[17] = {};  // Must have one extra char for string end.
    char szVersionName[9] = {};

    FillString(szSiteName, sizeof(szSiteName), poDS->abyHeader+320+12);
    FillString(szVersionName, sizeof(szVersionName), poDS->abyHeader+16+320+12);
    poDS->SetMetadataItem("PRODUCT_SITE_NAME", szSiteName);
    poDS->SetMetadataItem("PRODUCT_SITE_IRIS_VERSION", szVersionName);

    FillString(szSiteName, sizeof(szSiteName), poDS->abyHeader+90+320+12);
    FillString(szVersionName, sizeof(szVersionName), poDS->abyHeader+24+320+12);
    poDS->SetMetadataItem("INGEST_SITE_NAME", szSiteName);
    poDS->SetMetadataItem("INGEST_SITE_IRIS_VERSION", szVersionName);

    FillString(szSiteName, sizeof(szSiteName), poDS->abyHeader+74+320+12);
    poDS->SetMetadataItem("INGEST_HARDWARE_NAME", szSiteName);

    char szConfigFile[13] = {};
    FillString(szConfigFile, sizeof(szConfigFile), poDS->abyHeader+62+12);
    poDS->SetMetadataItem("PRODUCT_CONFIGURATION_NAME", szConfigFile);

    char szTaskName[13] = {};
    FillString(szTaskName, sizeof(szTaskName), poDS->abyHeader+74+12);
    poDS->SetMetadataItem("TASK_NAME", szTaskName);

    const short nRadarHeight = CPL_LSBSINT16PTR(poDS->abyHeader+284+320+12);
    poDS->SetMetadataItem("RADAR_HEIGHT",
                          CPLString().Printf("%d m", nRadarHeight));
    // Ground height over the sea level.
    const short nGroundHeight = CPL_LSBSINT16PTR(poDS->abyHeader+118+320+12);
    poDS->SetMetadataItem(
        "GROUND_HEIGHT",
        CPLString().Printf("%d m", nRadarHeight-nGroundHeight));

    unsigned short nFlags = CPL_LSBUINT16PTR(poDS->abyHeader + 86 + 12);
    // Get eleventh bit.
    nFlags = nFlags << 4;
    nFlags = nFlags >> 15;
    if( nFlags == 1 )
    {
        poDS->SetMetadataItem("COMPOSITED_PRODUCT", "YES");
        const unsigned int compositedMask =
            CPL_LSBUINT32PTR(poDS->abyHeader + 232 + 320 + 12);
        poDS->SetMetadataItem(
            "COMPOSITED_PRODUCT_MASK",
            CPLString().Printf("0x%08x", compositedMask));
    }
    else
    {
        poDS->SetMetadataItem( "COMPOSITED_PRODUCT","NO");
    }

    // Wave values.
    poDS->SetMetadataItem(
        "PRF",
        CPLString().Printf("%d Hz",
                           CPL_LSBSINT32PTR(poDS->abyHeader+120+320+12)));
    poDS->SetMetadataItem(
        "WAVELENGTH",
        CPLString().Printf(
            "%4.2f cm", CPL_LSBSINT32PTR(poDS->abyHeader+148+320+12) / 100.0f));
    const unsigned short nPolarizationType =
        CPL_LSBUINT16PTR(poDS->abyHeader+172+320+12);

    // See section 3.3.37 & 3.2.54.
    float fNyquist =
        (CPL_LSBSINT32PTR(poDS->abyHeader+120+320+12)) *
        (static_cast<float>(CPL_LSBSINT32PTR(poDS->abyHeader+148+320+12))
         / 10000.0f) / 4.0f;
    if( nPolarizationType == 1 )
        fNyquist = fNyquist * 2.0f;
    else if( nPolarizationType == 2 )
        fNyquist = fNyquist * 3.0f;
    else if( nPolarizationType == 3 )
        fNyquist = fNyquist * 4.0f;
    poDS->fNyquistVelocity = fNyquist;
    poDS->SetMetadataItem("NYQUIST_VELOCITY",
                          CPLString().Printf("%.2f m/s", fNyquist));

    // Product dependent metadata (stored in 80 bytes from 162 bytes
    // at the product header) See point 3.2.30 at page 3.19 of the
    // manual.
    // See point 3.2.25 at page 3.12 of the manual.
    if( EQUAL(aszProductNames[poDS->nProductCode], "PPI") )
    {
        // Degrees = 360 * (Binary Angle)*2^N
        const float fElevation =
            CPL_LSBSINT16PTR(poDS->abyHeader+164+12) * 360.0f / 65536.0f;

        poDS->SetMetadataItem("PPI_ELEVATION_ANGLE",
                              CPLString().Printf("%f", fElevation));
        if( EQUAL(aszDataTypeCodes[poDS->nDataTypeCode],"dBZ") )
            poDS->SetMetadataItem( "DATA_TYPE_UNITS", "dBZ");
        else
            poDS->SetMetadataItem("DATA_TYPE_UNITS", "m/s");
        // See point 3.2.2 at page 3.2 of the manual.
    }
    else if( EQUAL(aszProductNames[poDS->nProductCode], "CAPPI") )
    {
        const float fElevation =
            CPL_LSBSINT32PTR(poDS->abyHeader+4+164+12) / 100.0f;
        poDS->SetMetadataItem("CAPPI_BOTTOM_HEIGHT",
                              CPLString().Printf("%.1f m", fElevation));
        const float fAzimuthSmoothingForShear =
            CPL_LSBUINT16PTR(poDS->abyHeader+10+164+12) * 360.0f / 65536.0f;
        poDS->SetMetadataItem(
            "AZIMUTH_SMOOTHING_FOR_SHEAR",
            CPLString().Printf("%.1f", fAzimuthSmoothingForShear));
        const unsigned int nMaxAgeVVPCorrection =
            CPL_LSBUINT32PTR(poDS->abyHeader+24+164+12);
        poDS->SetMetadataItem(
            "MAX_AGE_FOR_SHEAR_VVP_CORRECTION",
            CPLString().Printf("%d s", nMaxAgeVVPCorrection));
        if( EQUAL(aszDataTypeCodes[poDS->nDataTypeCode], "dBZ") )
            poDS->SetMetadataItem( "DATA_TYPE_UNITS", "dBZ");
        else
            poDS->SetMetadataItem( "DATA_TYPE_UNITS", "m/s");
        // See point 3.2.32 at page 3.19 of the manual.
    }
    else if( EQUAL(aszProductNames[poDS->nProductCode],"RAIN1") ||
             EQUAL(aszProductNames[poDS->nProductCode],"RAINN") )
    {
        const short nNumProducts = CPL_LSBSINT16PTR(poDS->abyHeader+170+320+12);
        poDS->SetMetadataItem("NUM_FILES_USED",
                              CPLString().Printf("%d", nNumProducts));

        const float fMinZAcum =
            (CPL_LSBUINT32PTR(poDS->abyHeader+164+12) - 32768.0f) / 10000.0f;
        poDS->SetMetadataItem("MINIMUM_Z_TO_ACUMULATE",
                              CPLString().Printf("%f", fMinZAcum));

        const unsigned short nSecondsOfAccumulation =
            CPL_LSBUINT16PTR(poDS->abyHeader+6+164+12);
        poDS->SetMetadataItem("SECONDS_OF_ACCUMULATION",
                              CPLString().Printf("%d s",
                                                 nSecondsOfAccumulation));

        const unsigned int nSpanInputFiles =
            CPL_LSBUINT32PTR(poDS->abyHeader+24+164+12);
        poDS->SetMetadataItem("SPAN_OF_INPUT_FILES",
                              CPLString().Printf("%d s", nSpanInputFiles));
        poDS->SetMetadataItem("DATA_TYPE_UNITS", "mm");

        char szInputProductName[13] = "";
        for( int k = 0; k < 12; k++ )
            szInputProductName[k] = *reinterpret_cast<char *>(
                poDS->abyHeader+k+12+164+12);

        poDS->SetMetadataItem("INPUT_PRODUCT_NAME",
                              CPLString().Printf("%s", szInputProductName));

        if( EQUAL(aszProductNames[poDS->nProductCode], "RAINN") )
             poDS->SetMetadataItem(
                 "NUM_HOURS_ACCUMULATE",
                 CPLString().Printf(
                     "%d", CPL_LSBUINT16PTR(poDS->abyHeader+10+164+12)));

    // See point 3.2.73 at page 3.36 of the manual.
    }
    else if( EQUAL(aszProductNames[poDS->nProductCode], "VIL") )
    {
        const float fBottomHeightInterval =
            CPL_LSBSINT32PTR(poDS->abyHeader+4+164+12) / 100.0f;
        // TYPO in metadata key: FIXME ?
        poDS->SetMetadataItem(
            "BOTTOM_OF_HEIGTH_INTERVAL",
            CPLString().Printf("%.1f m", fBottomHeightInterval));
        const float fTopHeightInterval =
            CPL_LSBSINT32PTR(poDS->abyHeader+8+164+12) / 100.0f;
        // TYPO in metadata key: FIXME ?
        poDS->SetMetadataItem("TOP_OF_HEIGTH_INTERVAL",
                              CPLString().Printf("%.1f m", fTopHeightInterval));
        poDS->SetMetadataItem("VIL_DENSITY_NOT_AVAILABLE_VALUE", "-1");
        poDS->SetMetadataItem("DATA_TYPE_UNITS","mm");
    // See point 3.2.68 at page 3.36 of the manual
    }
    else if( EQUAL(aszProductNames[poDS->nProductCode], "TOPS") )
    {
        const float fZThreshold =
            CPL_LSBSINT16PTR(poDS->abyHeader+4+164+12) / 16.0f;
        poDS->SetMetadataItem("Z_THRESHOLD",
                              CPLString().Printf("%.1f dBZ", fZThreshold));
        poDS->SetMetadataItem("ECHO_TOPS_NOT_AVAILABLE_VALUE", "-1");
        poDS->SetMetadataItem("DATA_TYPE_UNITS", "km");
    // See point 3.2.20 at page 3.10 of the manual.
    }
    else if( EQUAL(aszProductNames[poDS->nProductCode], "MAX") )
    {
        const float fBottomInterval =
            CPL_LSBSINT32PTR(poDS->abyHeader+4+164+12) / 100.0f;
        poDS->SetMetadataItem( "BOTTOM_OF_INTERVAL",
                               CPLString().Printf("%.1f m", fBottomInterval));
        const float fTopInterval =
            CPL_LSBSINT32PTR(poDS->abyHeader+8+164+12) / 100.0f;
        poDS->SetMetadataItem("TOP_OF_INTERVAL",
                              CPLString().Printf("%.1f m", fTopInterval));
        const int nNumPixelsSidePanels =
            CPL_LSBSINT32PTR(poDS->abyHeader+12+164+12);
        poDS->SetMetadataItem("NUM_PIXELS_SIDE_PANELS",
                              CPLString().Printf("%d", nNumPixelsSidePanels));
        const short nHorizontalSmootherSidePanels =
            CPL_LSBSINT16PTR(poDS->abyHeader+16+164+12);
        poDS->SetMetadataItem(
            "HORIZONTAL_SMOOTHER_SIDE_PANELS",
            CPLString().Printf("%d", nHorizontalSmootherSidePanels));
        const short nVerticalSmootherSidePanels =
            CPL_LSBSINT16PTR(poDS->abyHeader+18+164+12);
        poDS->SetMetadataItem(
            "VERTICAL_SMOOTHER_SIDE_PANELS",
            CPLString().Printf("%d", nVerticalSmootherSidePanels));
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    // coverity[tainted_data]
    for( int iBandNum = 1; iBandNum <= nNumBands; iBandNum++ )
    {
        poDS->SetBand( iBandNum, new IRISRasterBand( poDS, iBandNum ));

        poDS->GetRasterBand(iBandNum)->SetNoDataValue(-9999);
        // Calculating the band height to include it in the band metadata.  Only
        // for the CAPPI product.
        if( EQUAL(aszProductNames[poDS->nProductCode],"CAPPI") )
        {
            const float fScaleZ =
                CPL_LSBSINT32PTR(poDS->abyHeader + 96 + 12) / 100.0f;
            const float fOffset =
                CPL_LSBSINT32PTR(poDS->abyHeader + 4 + 164 + 12) / 100.0f;

            poDS->GetRasterBand(iBandNum)->
                SetMetadataItem(
                    "height",
                    CPLString().Printf("%.0f m",
                                       fOffset + fScaleZ*(iBandNum-1)));
        }
    }
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_IRIS()                         */
/************************************************************************/

void GDALRegister_IRIS()

{
    if( GDALGetDriverByName( "IRIS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "IRIS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "IRIS data (.PPI, .CAPPi etc)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#IRIS" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ppi" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = IRISDataset::Open;
    poDriver->pfnIdentify = IRISDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
