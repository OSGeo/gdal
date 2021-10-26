/******************************************************************************
 *
 * Project:  GRIB Driver
 * Purpose:  GDALDataset driver for GRIB translator for write support
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
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
 ******************************************************************************
 *
 */

/* Support for GRIB2 write capabilities has been funded by Meteorological */
/* Service of Canada */

#include "cpl_port.h"
#include "gribdataset.h"
#include "gdal_priv_templates.hpp"

#include <limits>

#include "degrib/degrib/meta.h"
CPL_C_START
#include "degrib/g2clib/grib2.h"
CPL_C_END

/************************************************************************/
/*                         Lon180to360()                                */
/************************************************************************/

static inline double Lon180to360(double lon)
{
    if (lon == 180) return 180;
    return fmod(fmod(lon, 360) + 360, 360);
}

/************************************************************************/
/*                             WriteByte()                              */
/************************************************************************/

static bool WriteByte( VSILFILE* fp, int nVal )
{
    GByte byVal = static_cast<GByte>(nVal);
    return VSIFWriteL(&byVal, 1, sizeof(byVal), fp) == sizeof(byVal);
}

/************************************************************************/
/*                            WriteSByte()                              */
/************************************************************************/

static bool WriteSByte( VSILFILE* fp, int nVal )
{
    signed char sVal = static_cast<signed char>(nVal);
    if( sVal == std::numeric_limits<signed char>::min() )
        sVal = std::numeric_limits<signed char>::min() + 1;
    GByte nUnsignedVal = (sVal < 0) ?
        static_cast<GByte>(-sVal) | 0x80U : static_cast<GByte>(sVal);
    return VSIFWriteL(&nUnsignedVal, 1, sizeof(nUnsignedVal), fp) ==
                                                        sizeof(nUnsignedVal);
}

/************************************************************************/
/*                            WriteUInt16()                             */
/************************************************************************/

static bool WriteUInt16( VSILFILE* fp, int nVal )
{
    GUInt16 usVal = static_cast<GUInt16>(nVal);
    CPL_MSBPTR16(&usVal);
    return VSIFWriteL(&usVal, 1, sizeof(usVal), fp) == sizeof(usVal);
}

/************************************************************************/
/*                             WriteInt16()                             */
/************************************************************************/

static bool WriteInt16( VSILFILE* fp, int nVal )
{
    GInt16 sVal = static_cast<GInt16>(nVal);
    if( sVal == std::numeric_limits<GInt16>::min() )
        sVal = std::numeric_limits<GInt16>::min() + 1;
    GUInt16 nUnsignedVal = (sVal < 0) ?
        static_cast<GUInt16>(-sVal) | 0x8000U : static_cast<GUInt16>(sVal);
    CPL_MSBPTR16(&nUnsignedVal);
    return VSIFWriteL(&nUnsignedVal, 1, sizeof(nUnsignedVal), fp)
                                                    == sizeof(nUnsignedVal);
}

/************************************************************************/
/*                             WriteUInt32()                            */
/************************************************************************/

static bool WriteUInt32( VSILFILE* fp, GUInt32 nVal )
{
    CPL_MSBPTR32(&nVal);
    return VSIFWriteL(&nVal, 1, sizeof(nVal), fp) == sizeof(nVal);
}

/************************************************************************/
/*                             WriteInt32()                             */
/************************************************************************/

static bool WriteInt32( VSILFILE* fp, GInt32 nVal )
{
    if( nVal == std::numeric_limits<GInt32>::min() )
        nVal = std::numeric_limits<GInt32>::min() + 1;
    GUInt32 nUnsignedVal = (nVal < 0) ?
        static_cast<GUInt32>(-nVal) | 0x80000000U : static_cast<GUInt32>(nVal);
    CPL_MSBPTR32(&nUnsignedVal);
    return VSIFWriteL(&nUnsignedVal, 1, sizeof(nUnsignedVal), fp)
                                                    == sizeof(nUnsignedVal);
}

/************************************************************************/
/*                            WriteFloat32()                            */
/************************************************************************/

static bool WriteFloat32( VSILFILE* fp, float fVal )
{
    CPL_MSBPTR32(&fVal);
    return VSIFWriteL(&fVal, 1, sizeof(fVal), fp) == sizeof(fVal);
}

/************************************************************************/
/*                         PatchSectionSize()                           */
/************************************************************************/

static void PatchSectionSize( VSILFILE* fp, vsi_l_offset nStartSection )
{
    vsi_l_offset nCurOffset = VSIFTellL(fp);
    VSIFSeekL(fp, nStartSection, SEEK_SET);
    GUInt32 nSect3Size = static_cast<GUInt32>(nCurOffset - nStartSection);
    WriteUInt32(fp, nSect3Size);
    VSIFSeekL(fp, nCurOffset, SEEK_SET);
}

/************************************************************************/
/*                         GRIB2Section3Writer                          */
/************************************************************************/

class GRIB2Section3Writer
{
        VSILFILE* fp;
        GDALDataset *poSrcDS;
        OGRSpatialReference oSRS;
        const char* pszProjection;
        double dfLLX, dfLLY, dfURX, dfURY;
        double adfGeoTransform[6];
        int nSplitAndSwapColumn = 0;

        bool WriteScaled(double dfVal, double dfUnit);
        bool TransformToGeo(double& dfX, double& dfY);
        bool WriteEllipsoidAndRasterSize();

        bool WriteGeographic();
        bool WriteMercator1SP();
        bool WriteMercator2SP(OGRSpatialReference* poSRS = nullptr);
        bool WriteTransverseMercator();
        bool WritePolarSteregraphic();
        bool WriteLCC1SP();
        bool WriteLCC2SPOrAEA(OGRSpatialReference* poSRS = nullptr);
        bool WriteLAEA();

    public:
        GRIB2Section3Writer( VSILFILE* fpIn, GDALDataset *poSrcDSIn );
        inline int SplitAndSwap() const { return nSplitAndSwapColumn; }

        bool Write();
};

/************************************************************************/
/*                        GRIB2Section3Writer()                         */
/************************************************************************/

GRIB2Section3Writer::GRIB2Section3Writer( VSILFILE* fpIn,
                                          GDALDataset *poSrcDSIn ) :
    fp(fpIn),
    poSrcDS(poSrcDSIn)
{
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    oSRS.importFromWkt( poSrcDS->GetProjectionRef() );
    pszProjection = oSRS.GetAttrValue("PROJECTION");

    poSrcDS->GetGeoTransform(adfGeoTransform);

    dfLLX = adfGeoTransform[0] + adfGeoTransform[1] / 2;
    dfLLY = adfGeoTransform[3] + adfGeoTransform[5] / 2 +
                    (poSrcDS->GetRasterYSize() - 1) * adfGeoTransform[5];
    dfURX = adfGeoTransform[0] + adfGeoTransform[1] / 2 +
                    (poSrcDS->GetRasterXSize() - 1) * adfGeoTransform[1];
    dfURY = adfGeoTransform[3] + adfGeoTransform[5] / 2;
    if( dfURY < dfLLY )
    {
        double dfTemp = dfURY;
        dfURY = dfLLY;
        dfLLY = dfTemp;
    }
}

/************************************************************************/
/*                     WriteEllipsoidAndRasterSize()                    */
/************************************************************************/

bool GRIB2Section3Writer::WriteEllipsoidAndRasterSize()
{
    const double dfSemiMajor = oSRS.GetSemiMajor();
    const double dfSemiMinor = oSRS.GetSemiMinor();
    const double dfInvFlattening = oSRS.GetInvFlattening();
    if( std::abs(dfSemiMajor-6378137.0) < 0.01
             && std::abs(dfInvFlattening-298.257223563) < 1e-9 ) // WGS84
    {
        WriteByte(fp, 5); // WGS84
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
    }
    else if( std::abs(dfSemiMajor-6378137.0) < 0.01
             && std::abs(dfInvFlattening-298.257222101) < 1e-9 ) // GRS80
    {
        WriteByte(fp, 4); // GRS80
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
    }
    else if( dfInvFlattening == 0 )
    {
        // Earth assumed spherical with radius specified (in m)
        // by data producer
        WriteByte(fp, 1);
        WriteByte(fp, 2); // scale = * 100
        WriteUInt32(fp, static_cast<GUInt32>(dfSemiMajor * 100 + 0.5));
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
    }
    else
    {
        // Earth assumed oblate spheroid with major and minor axes
        // specified (in m) by data producer
        WriteByte(fp, 7);
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt32(fp, GRIB2MISSING_u4);
        WriteByte(fp, 2); // scale = * 100
        WriteUInt32(fp, static_cast<GUInt32>(dfSemiMajor * 100 + 0.5));
        WriteByte(fp, 2); // scale = * 100
        WriteUInt32(fp, static_cast<GUInt32>(dfSemiMinor * 100 + 0.5));
    }
    WriteUInt32(fp, poSrcDS->GetRasterXSize());
    WriteUInt32(fp, poSrcDS->GetRasterYSize());

    return true;
}

/************************************************************************/
/*                            WriteScaled()                             */
/************************************************************************/

bool GRIB2Section3Writer::WriteScaled(double dfVal, double dfUnit)
{
    return WriteInt32( fp, static_cast<GInt32>(floor(dfVal / dfUnit + 0.5)) );
}

/************************************************************************/
/*                          WriteGeographic()                           */
/************************************************************************/

bool GRIB2Section3Writer::WriteGeographic()
{
    WriteUInt16(fp, GS3_LATLON); // Grid template number

    WriteEllipsoidAndRasterSize();

    if (dfLLX < 0 &&
        CPLTestBool(CPLGetConfigOption("GRIB_ADJUST_LONGITUDE_RANGE", "YES")))
    {
        CPLDebug("GRIB", "Source longitude range is %lf to %lf", dfLLX, dfURX);
        double dfOrigLLX = dfLLX;
        dfLLX = Lon180to360(dfLLX);
        dfURX = Lon180to360(dfURX);

        if (dfLLX > dfURX)
        {
            if (fabs(360 - poSrcDS->GetRasterXSize() * adfGeoTransform[1]) <
                adfGeoTransform[1] / 4)
            {
                // Find the first row number east of the prime meridian
                nSplitAndSwapColumn =
                  static_cast<int>(ceil((0 - dfOrigLLX) / adfGeoTransform[1]));
                CPLDebug("GRIB",
                         "Rewrapping around the prime meridian at column %d",
                         nSplitAndSwapColumn);
                dfLLX = 0;
                dfURX = 360 - adfGeoTransform[1];
            }
            else
            {
                CPLDebug("GRIB",
                         "Writing a GRIB with 0-360 longitudes crossing the prime meridian");
            }
        }
        CPLDebug("GRIB", "Target longitudes range is %lf %lf", dfLLX, dfURX);
    }

    WriteUInt32(fp, 0); // Basic angle. 0 equivalent of 1
    // Subdivisions of basic angle used. ~0 equivalent of 10^6
    WriteUInt32(fp, GRIB2MISSING_u4);
    const double dfAngUnit = 1e-6;
    WriteScaled(dfLLY, dfAngUnit);
    WriteScaled(dfLLX, dfAngUnit);
    WriteByte(fp, GRIB2BIT_3 | GRIB2BIT_4); // Resolution and component flags
    WriteScaled(dfURY, dfAngUnit);
    WriteScaled(dfURX, dfAngUnit);
    WriteScaled(adfGeoTransform[1], dfAngUnit);
    WriteScaled(fabs(adfGeoTransform[5]), dfAngUnit);
    WriteByte(fp, GRIB2BIT_2); // Scanning mode: bottom-to-top

    return true;
}

/************************************************************************/
/*                           TransformToGeo()                           */
/************************************************************************/

bool GRIB2Section3Writer::TransformToGeo(double& dfX, double& dfY)
{
    OGRSpatialReference oLL;  // Construct the "geographic" part of oSRS.
    oLL.CopyGeogCSFrom(&oSRS);
    oLL.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRCoordinateTransformation *poTransformSRSToLL =
        OGRCreateCoordinateTransformation( &(oSRS), &(oLL));
    if( poTransformSRSToLL == nullptr ||
        !poTransformSRSToLL->Transform(1, &dfX, &dfY) )
    {
        delete poTransformSRSToLL;
        return false;
    }
    delete poTransformSRSToLL;
    if( dfX < 0.0 )
        dfX += 360.0;
    return true;
}

/************************************************************************/
/*                           WriteMercator1SP()                         */
/************************************************************************/

bool GRIB2Section3Writer::WriteMercator1SP()
{
    if( oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0) != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Mercator_1SP with central_meridian != 0 not supported");
        return false;
    }
    if( oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Mercator_1SP with latitude_of_origin != 0 not supported");
        return false;
    }

    OGRSpatialReference* poMerc2SP =
        oSRS.convertToOtherProjection(SRS_PT_MERCATOR_2SP);
    if( poMerc2SP == nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot get Mercator_2SP formulation");
        return false;
    }

    bool bRet = WriteMercator2SP(poMerc2SP);
    delete poMerc2SP;
    return bRet;
}

/************************************************************************/
/*                           WriteMercator2SP()                         */
/************************************************************************/

bool GRIB2Section3Writer::WriteMercator2SP(OGRSpatialReference* poSRS)
{
    if( poSRS == nullptr )
        poSRS = &oSRS;

    if( poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0) != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Mercator_2SP with central_meridian != 0 not supported");
        return false;
    }
    if( poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Mercator_2SP with latitude_of_origin != 0 not supported");
        return false;
    }

    WriteUInt16(fp, GS3_MERCATOR); // Grid template number

    WriteEllipsoidAndRasterSize();

    if( !TransformToGeo(dfLLX, dfLLY) || !TransformToGeo(dfURX, dfURY) )
        return false;

    const double dfAngUnit = 1e-6;
    WriteScaled(dfLLY, dfAngUnit);
    WriteScaled(dfLLX, dfAngUnit);
    WriteByte(fp, GRIB2BIT_3 | GRIB2BIT_4); // Resolution and component flags
    WriteScaled(
        poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0), dfAngUnit);
    WriteScaled(dfURY, dfAngUnit);
    WriteScaled(dfURX, dfAngUnit);
    WriteByte(fp , GRIB2BIT_2 ); // Scanning mode: bottom-to-top
    WriteInt32(fp, 0); // angle of the grid
    const double dfLinearUnit = 1e-3;
    WriteScaled(adfGeoTransform[1], dfLinearUnit);
    WriteScaled(fabs(adfGeoTransform[5]), dfLinearUnit);

    return true;
}

/************************************************************************/
/*                      WriteTransverseMercator()                       */
/************************************************************************/

bool GRIB2Section3Writer::WriteTransverseMercator()
{
    WriteUInt16(fp, GS3_TRANSVERSE_MERCATOR); // Grid template number
    WriteEllipsoidAndRasterSize();

    const double dfAngUnit = 1e-6;
    WriteScaled(
        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0), dfAngUnit);
    WriteScaled(oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0), dfAngUnit);
    WriteByte(fp, GRIB2BIT_3 | GRIB2BIT_4); // Resolution and component flags
    float fScale = static_cast<float>(oSRS.GetNormProjParm(
        SRS_PP_SCALE_FACTOR, 0.0));
    WriteFloat32( fp, fScale );
    const double dfLinearUnit = 1e-2;
    WriteScaled(
        oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0), dfLinearUnit);
    WriteScaled(
        oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0), dfLinearUnit);
    WriteByte(fp, GRIB2BIT_2); // Scanning mode: bottom-to-top
    WriteScaled(adfGeoTransform[1], dfLinearUnit);
    WriteScaled(fabs(adfGeoTransform[5]), dfLinearUnit);
    WriteScaled(dfLLX, dfLinearUnit);
    WriteScaled(dfLLY, dfLinearUnit);
    WriteScaled(dfURX, dfLinearUnit);
    WriteScaled(dfURY, dfLinearUnit);

    return true;
}

/************************************************************************/
/*                       WritePolarSteregraphic()                       */
/************************************************************************/

bool GRIB2Section3Writer::WritePolarSteregraphic()
{
    WriteUInt16(fp, GS3_POLAR); // Grid template number
    WriteEllipsoidAndRasterSize();

    if( !TransformToGeo(dfLLX, dfLLY) )
        return false;

    const double dfAngUnit = 1e-6;
    WriteScaled(dfLLY, dfAngUnit);
    WriteScaled(dfLLX, dfAngUnit);
    WriteByte(fp, GRIB2BIT_3 | GRIB2BIT_4); // Resolution and component flags
    const double dfLatOrigin = oSRS.GetNormProjParm(
                                    SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
    WriteScaled(dfLatOrigin, dfAngUnit);
    WriteScaled(fmod(oSRS.GetNormProjParm(
                     SRS_PP_CENTRAL_MERIDIAN, 0.0) + 360.0, 360.0), dfAngUnit);
    const double dfLinearUnit = 1e-3;
    WriteScaled(adfGeoTransform[1], dfLinearUnit);
    WriteScaled(fabs(adfGeoTransform[5]), dfLinearUnit);
    // Projection center flag: BIT1=0 North Pole, BIT1=1 South Pole
    WriteByte(fp, (dfLatOrigin < 0) ? GRIB2BIT_1 : 0);
    WriteByte(fp, GRIB2BIT_2); // Scanning mode: bottom-to-top

    return true;
}

/************************************************************************/
/*                            WriteLCC1SP()                             */
/************************************************************************/

bool GRIB2Section3Writer::WriteLCC1SP()
{
    OGRSpatialReference* poLCC2SP =
        oSRS.convertToOtherProjection(SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP);
    if( poLCC2SP == nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot get Lambert_Conformal_Conic_2SP formulation");
        return false;
    }

    bool bRet = WriteLCC2SPOrAEA(poLCC2SP);
    delete poLCC2SP;
    return bRet;
}

/************************************************************************/
/*                            WriteLCC2SPOrAEA()                        */
/************************************************************************/

bool GRIB2Section3Writer::WriteLCC2SPOrAEA(OGRSpatialReference* poSRS)
{
    if( poSRS == nullptr )
        poSRS = &oSRS;
    if( EQUAL(poSRS->GetAttrValue("PROJECTION"),
              SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
        WriteUInt16(fp, GS3_LAMBERT); // Grid template number
    else
        WriteUInt16(fp, GS3_ALBERS_EQUAL_AREA); // Grid template number

    WriteEllipsoidAndRasterSize();

    if( !TransformToGeo(dfLLX, dfLLY) )
        return false;

    const double dfAngUnit = 1e-6;
    WriteScaled(dfLLY, dfAngUnit);
    WriteScaled(dfLLX, dfAngUnit);
    // Resolution and component flags. "not applicable" ==> 0 ?
    WriteByte( fp, 0);
    WriteScaled(
        poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0), dfAngUnit);
    WriteScaled(fmod(oSRS.GetNormProjParm(
                     SRS_PP_CENTRAL_MERIDIAN, 0.0) + 360.0, 360.0), dfAngUnit);
    const double dfLinearUnit = 1e-3;
    WriteScaled(adfGeoTransform[1], dfLinearUnit);
    WriteScaled(fabs(adfGeoTransform[5]), dfLinearUnit);
    WriteByte(fp, 0); // Projection centre flag
    WriteByte(fp, GRIB2BIT_2); // Scanning mode: bottom-to-top
    WriteScaled(
        poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0), dfAngUnit);
    WriteScaled(
        poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0), dfAngUnit);
    // Latitude of the southern pole of projection
    WriteUInt32( fp, GRIB2MISSING_u4 );
    // Longitude of the southern pole of projection
    WriteUInt32( fp, GRIB2MISSING_u4 );
    return true;
}

/************************************************************************/
/*                              WriteLAEA()                             */
/************************************************************************/

bool GRIB2Section3Writer::WriteLAEA()
{
    WriteUInt16(fp, GS3_LAMBERT_AZIMUTHAL); // Grid template number

    WriteEllipsoidAndRasterSize();

    if( !TransformToGeo(dfLLX, dfLLY) || !TransformToGeo(dfURX, dfURY) )
        return false;

    const double dfAngUnit = 1e-6;
    WriteScaled(dfLLY, dfAngUnit);
    WriteScaled(dfLLX, dfAngUnit);
    WriteScaled(
        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER, 0.0), dfAngUnit);
    WriteScaled(fmod(oSRS.GetNormProjParm(
                SRS_PP_LONGITUDE_OF_CENTER, 0.0) + 360.0, 360.0), dfAngUnit);
    WriteByte( fp, GRIB2BIT_3 | GRIB2BIT_4); // Resolution and component flags
    const double dfLinearUnit = 1e-3;
    WriteScaled(adfGeoTransform[1], dfLinearUnit);
    WriteScaled(fabs(adfGeoTransform[5]), dfLinearUnit);
    WriteByte(fp, GRIB2BIT_2); // Scanning mode: bottom-to-top
    return true;
}

/************************************************************************/
/*                                Write()                               */
/************************************************************************/

bool GRIB2Section3Writer::Write()
{
    // Section 3: Grid Definition Section
    vsi_l_offset nStartSection = VSIFTellL(fp);

    WriteUInt32(fp, GRIB2MISSING_u4); // section size

    WriteByte(fp, 3); // section number

    // Source of grid definition = Specified in Code Table 3.1
    WriteByte(fp, 0);

    const GUInt32 nDataPoints =
        static_cast<GUInt32>(poSrcDS->GetRasterXSize()) *
                             poSrcDS->GetRasterYSize();
    WriteUInt32(fp, nDataPoints);

    // Number of octets for optional list of numbers defining number of points
    WriteByte(fp, 0);

    // Interpretation of list of numbers defining number of points =
    // No appended list
    WriteByte(fp, 0);

    bool bRet = false;
    if( oSRS.IsGeographic() )
    {
        bRet = WriteGeographic();
    }
    else if( pszProjection && EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        bRet = WriteMercator1SP();
    }
    else if( pszProjection && EQUAL(pszProjection, SRS_PT_MERCATOR_2SP) )
    {
        bRet = WriteMercator2SP();
    }
    else if( pszProjection && EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        bRet = WriteTransverseMercator();
    }
    else if( pszProjection &&
             EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        bRet = WritePolarSteregraphic();
    }
    else if( pszProjection != nullptr &&
             EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
        bRet = WriteLCC1SP();
    }
    else if( pszProjection != nullptr &&
             (EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) ||
              EQUAL(pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA)) )
    {
        bRet = WriteLCC2SPOrAEA();
    }
    else if( pszProjection &&
             EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        bRet = WriteLAEA();
    }

    PatchSectionSize( fp, nStartSection );

    return bRet;
}

/************************************************************************/
/*                         GetBandOption()                              */
/************************************************************************/

static const char* GetBandOption(char** papszOptions,
                                 GDALDataset* poSrcDS,
                                 int nBand,
                                 const char* pszKey, const char* pszDefault)
{
    const char* pszVal = CSLFetchNameValue(papszOptions,
                                CPLSPrintf("BAND_%d_%s", nBand, pszKey));
    if( pszVal == nullptr )
    {
        pszVal = CSLFetchNameValue(papszOptions, pszKey);
    }
    if( pszVal == nullptr && poSrcDS != nullptr )
    {
        pszVal = poSrcDS->GetRasterBand(nBand)->GetMetadataItem(
                    (CPLString("GRIB_") + pszKey).c_str());
    }
    if( pszVal == nullptr )
    {
        pszVal = pszDefault;
    }
    return pszVal;
}

/************************************************************************/
/*                        GRIB2Section567Writer                         */
/************************************************************************/

class GRIB2Section567Writer
{
        VSILFILE        *m_fp;
        GDALDataset     *m_poSrcDS;
        int              m_nBand;
        int              m_nXSize;
        int              m_nYSize;
        GUInt32          m_nDataPoints;
        GDALDataType     m_eDT;
        double           m_adfGeoTransform[6];
        int              m_nDecimalScaleFactor;
        double           m_dfDecimalScale;
        float            m_fMin;
        float            m_fMax;
        double           m_dfMinScaled;
        int              m_nBits;
        bool             m_bUseZeroBits;
        float            m_fValOffset;
        int              m_bHasNoData;
        double           m_dfNoData;
        int              m_nSplitAndSwap;

        float*           GetFloatData();
        bool             WriteSimplePacking();
        bool             WriteComplexPacking(int nSpatialDifferencingOrder);
        bool             WriteIEEE(GDALProgressFunc pfnProgress,
                                   void * pProgressData);
        bool             WritePNG();
        bool             WriteJPEG2000(char** papszOptions);

    public:
        GRIB2Section567Writer( VSILFILE* fp,
                               GDALDataset *poSrcDS,
                               int nBand,
                               int nSplitAndSwap );

        bool Write(float fValOffset,
                   char** papszOptions,
                   GDALProgressFunc pfnProgress, void * pProgressData);
        void WriteComplexPackingNoData();
};

/************************************************************************/
/*                      GRIB2Section567Writer()                         */
/************************************************************************/

GRIB2Section567Writer::GRIB2Section567Writer( VSILFILE* fp,
                                              GDALDataset *poSrcDS,
                                              int nBand,
                                              int nSplitAndSwap ):
    m_fp(fp),
    m_poSrcDS(poSrcDS),
    m_nBand(nBand),
    m_nXSize(poSrcDS->GetRasterXSize()),
    m_nYSize(poSrcDS->GetRasterYSize()),
    m_nDataPoints(static_cast<GUInt32>(m_nXSize) * m_nYSize),
    m_eDT(m_poSrcDS->GetRasterBand(m_nBand)->GetRasterDataType()),
    m_nDecimalScaleFactor(0),
    m_dfDecimalScale(1.0),
    m_fMin(0.0f),
    m_fMax(0.0f),
    m_dfMinScaled(0.0),
    m_nBits(0),
    m_bUseZeroBits(false),
    m_fValOffset(0.0),
    m_bHasNoData(false),
    m_dfNoData(0.0),
    m_nSplitAndSwap(nSplitAndSwap)
{
    m_poSrcDS->GetGeoTransform(m_adfGeoTransform);
    m_dfNoData = m_poSrcDS->GetRasterBand(nBand)->GetNoDataValue(&m_bHasNoData);
}

/************************************************************************/
/*                          GetFloatData()                              */
/************************************************************************/

float* GRIB2Section567Writer::GetFloatData()
{
    float* pafData =
        static_cast<float*>(VSI_MALLOC2_VERBOSE(m_nDataPoints, sizeof(float)));
    if( pafData == nullptr )
    {
        return nullptr;
    }
    CPLErr eErr = m_poSrcDS->GetRasterBand(m_nBand)->RasterIO(
        GF_Read,
        m_nSplitAndSwap, 0,
        m_nXSize - m_nSplitAndSwap, m_nYSize,
        pafData + (m_adfGeoTransform[5] < 0 ? (m_nYSize - 1) * m_nXSize : 0),
        m_nXSize - m_nSplitAndSwap, m_nYSize,
        GDT_Float32,
        sizeof(float),
        m_adfGeoTransform[5] < 0 ?
            -static_cast<GSpacing>(m_nXSize * sizeof(float)):
            static_cast<GSpacing>(m_nXSize * sizeof(float)),
        nullptr);
    if (eErr != CE_None)
    {
        VSIFree(pafData);
        return nullptr;
    }
    if (m_nSplitAndSwap > 0)
    {
        eErr = m_poSrcDS->GetRasterBand(m_nBand)->RasterIO(
        GF_Read,
        0, 0,
        m_nSplitAndSwap, m_nYSize,
        pafData + (m_adfGeoTransform[5] < 0 ? (m_nYSize - 1) * m_nXSize : 0) +
            (m_nXSize - m_nSplitAndSwap),
        m_nSplitAndSwap, m_nYSize,
        GDT_Float32,
        sizeof(float),
        m_adfGeoTransform[5] < 0
            ? -static_cast<GSpacing>(m_nXSize * sizeof(float))
            : static_cast<GSpacing>(m_nXSize * sizeof(float)),
        nullptr);
        if (eErr != CE_None)
        {
            VSIFree(pafData);
            return nullptr;
        }
    }

    m_fMin = std::numeric_limits<float>::max();
    m_fMax = -std::numeric_limits<float>::max();
    bool bHasNoDataValuePoint = false;
    bool bHasDataValuePoint = false;
    for( GUInt32 i = 0; i < m_nDataPoints; i++ )
    {
        if( m_bHasNoData && pafData[i] == static_cast<float>(m_dfNoData) )
        {
            if (!bHasNoDataValuePoint) bHasNoDataValuePoint = true;
            continue;
        }
        if( !CPLIsFinite( pafData[i] ) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                        "Non-finite values not supported for "
                        "this data encoding");
            VSIFree(pafData);
            return nullptr;
        }
        if (!bHasDataValuePoint) bHasDataValuePoint = true;
        pafData[i] += m_fValOffset;
        if( pafData[i] < m_fMin ) m_fMin = pafData[i];
        if( pafData[i] > m_fMax ) m_fMax = pafData[i];
    }
    if( m_fMin > m_fMax )
    {
        m_fMin = m_fMax = static_cast<float>(m_dfNoData);
    }

    // We check that the actual range of values got from the above RasterIO
    // request does not go over the expected range of the datatype, as we
    // later assume that for computing nMaxBitsPerElt.
    // This shouldn't happen for well-behaved drivers, but this can still
    // happen in practice, if some drivers don't completely fill buffers etc.
    if( m_fMax > m_fMin &&
        GDALDataTypeIsInteger(m_eDT) &&
        ceil(log(m_fMax - m_fMin) / log(2.0)) > GDALGetDataTypeSize(m_eDT) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Garbage values found when requesting input dataset");
        VSIFree(pafData);
        return nullptr;
    }

    m_dfMinScaled =
        m_dfDecimalScale == 1.0 ? m_fMin : floor(m_fMin * m_dfDecimalScale);
    if( !(m_dfMinScaled >= -std::numeric_limits<float>::max() &&
          m_dfMinScaled < std::numeric_limits<float>::max()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Scaled min value not representable on IEEE754 "
                 "single precision float");
        VSIFree(pafData);
        return nullptr;
    }

    const double dfScaledMaxDiff = (m_fMax-m_fMin)* m_dfDecimalScale;
    if( GDALDataTypeIsFloating(m_eDT) && m_nBits == 0 &&
        dfScaledMaxDiff > 0 && dfScaledMaxDiff <= 256 )
    {
        m_nBits = 8;
    }

    m_bUseZeroBits = ( m_fMin == m_fMax &&  !(bHasDataValuePoint && bHasNoDataValuePoint) )  ||
        (!GDALDataTypeIsFloating(m_eDT) && dfScaledMaxDiff < 1.0);

    return pafData;
}

/************************************************************************/
/*                        WriteSimplePacking()                          */
/************************************************************************/

// See http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_temp5-0.shtml
bool GRIB2Section567Writer::WriteSimplePacking()
{
    float* pafData = GetFloatData();
    if( pafData == nullptr )
        return false;

    const int nBitCorrectionForDec = static_cast<int>(
        ceil(m_nDecimalScaleFactor * log(10.0) / log(2.0)));
    const int nMaxBitsPerElt = std::max(1, std::min(31, (m_nBits > 0) ? m_nBits:
                GDALGetDataTypeSize(m_eDT)+ nBitCorrectionForDec));
    if( nMaxBitsPerElt > 0 &&
        m_nDataPoints > static_cast<GUInt32>(INT_MAX) / nMaxBitsPerElt )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Int overflow while computing maximum number of bits");
        VSIFree(pafData);
        return false;
    }

    const int nMaxSize = (m_nDataPoints * nMaxBitsPerElt + 7) / 8;
    void* pabyData = VSI_MALLOC_VERBOSE(nMaxSize);
    if( pabyData == nullptr )
    {
        VSIFree(pafData);
        VSIFree(pabyData);
        return false;
    }

    // Indices expected by simpack()
    enum
    {
        TMPL5_R_IDX = 0, // Reference value (R)
        TMPL5_E_IDX = 1, // Binary scale factor (E)
        TMPL5_D_IDX = 2, // Decimal scale factor (D)
        TMPL5_NBITS_IDX = 3, // Number of bits used for each packed value
        TMPL5_TYPE_IDX = 4 // type of original data
    };

    g2int idrstmpl[TMPL5_TYPE_IDX+1]= { 0 };
    idrstmpl[TMPL5_R_IDX] = 0; // reference value, to be filled by simpack
    idrstmpl[TMPL5_E_IDX] = 0; // binary scale factor, to be filled by simpack
    idrstmpl[TMPL5_D_IDX] = m_nDecimalScaleFactor;
    // to be filled by simpack if set to 0
    idrstmpl[TMPL5_NBITS_IDX] = m_nBits;
    // to be filled by simpack (and we will ignore it)
    idrstmpl[TMPL5_TYPE_IDX] = 0;
    g2int nLengthPacked = 0;
    simpack(pafData,m_nDataPoints,idrstmpl,
            static_cast<unsigned char*>(pabyData),&nLengthPacked);
    CPLAssert(nLengthPacked <= nMaxSize);
    if( nLengthPacked < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Error while packing");
        VSIFree(pafData);
        VSIFree(pabyData);
        return false;
    }

    // Section 5: Data Representation Section
    WriteUInt32(m_fp, 21); // section size
    WriteByte(m_fp, 5); // section number
    WriteUInt32(m_fp, m_nDataPoints);
    WriteUInt16(m_fp, GS5_SIMPLE);
    float fRefValue;
    memcpy(&fRefValue, &idrstmpl[TMPL5_R_IDX], 4);
    WriteFloat32(m_fp, fRefValue);
    WriteInt16(m_fp, idrstmpl[TMPL5_E_IDX]);
    WriteInt16(m_fp, idrstmpl[TMPL5_D_IDX]);
    WriteByte(m_fp, idrstmpl[TMPL5_NBITS_IDX]);
    // Type of original data: 0=Floating, 1=Integer
    WriteByte(m_fp, GDALDataTypeIsFloating(m_eDT) ? 0 : 1);

    // Section 6: Bitmap section
#ifdef DEBUG
    if( CPLTestBool(CPLGetConfigOption("GRIB_WRITE_BITMAP_TEST", "NO")) )
    {
        // Just for the purpose of generating a test product !
        static int counter = 0;
        counter++;
        if( counter == 1 )
        {
            WriteUInt32(m_fp, 6 + ((m_nDataPoints + 7) / 8)); // section size
            WriteByte(m_fp, 6); // section number
            WriteByte(m_fp, 0); // bitmap
            for( GUInt32 i = 0; i < (m_nDataPoints + 7) / 8; i++)
                WriteByte(m_fp, 255);
        }
        else
        {
            WriteUInt32(m_fp, 6); // section size
            WriteByte(m_fp, 6); // section number
            WriteByte(m_fp, 254); // reuse previous bitmap
        }
    }
    else
#endif
    {
        WriteUInt32(m_fp, 6); // section size
        WriteByte(m_fp, 6); // section number
        WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap
    }

    // Section 7: Data Section
    WriteUInt32(m_fp, 5 + nLengthPacked); // section size
    WriteByte(m_fp, 7); // section number
    if( static_cast<int>(VSIFWriteL( pabyData, 1, nLengthPacked, m_fp ))
                                                        != nLengthPacked )
    {
        VSIFree(pafData);
        VSIFree(pabyData);
        return false;
    }

    VSIFree(pafData);
    VSIFree(pabyData);

    return true;
}

/************************************************************************/
/*                      WriteComplexPackingNoData()                     */
/************************************************************************/

void GRIB2Section567Writer::WriteComplexPackingNoData()
{
    if( !m_bHasNoData )
    {
        WriteUInt32(m_fp, GRIB2MISSING_u4);
    }
    else if( GDALDataTypeIsFloating(m_eDT) )
    {
        WriteFloat32(m_fp, static_cast<float>(m_dfNoData));
    }
    else
    {
        if( GDALIsValueInRange<int>(m_dfNoData) )
        {
            WriteInt32(m_fp, static_cast<int>(m_dfNoData));
        }
        else
        {
            WriteUInt32(m_fp, GRIB2MISSING_u4);
        }
    }
}

/************************************************************************/
/*                       WriteComplexPacking()                          */
/************************************************************************/

// See http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_temp5-2.shtml
// and http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_temp5-3.shtml

bool GRIB2Section567Writer::WriteComplexPacking(int nSpatialDifferencingOrder)
{
    if( nSpatialDifferencingOrder < 0 || nSpatialDifferencingOrder > 2 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported value for SPATIAL_DIFFERENCING_ORDER");
        return false;
    }

    float* pafData = GetFloatData();
    if( pafData == nullptr )
        return false;

    const float fNoData = static_cast<float>(m_dfNoData);
    if( m_bUseZeroBits )
    {
        // Case where all values are at nodata or a single value
        VSIFree(pafData);

        // Section 5: Data Representation Section
        WriteUInt32(m_fp, 47); // section size
        WriteByte(m_fp, 5); // section number
        WriteUInt32(m_fp, m_nDataPoints);
        WriteUInt16(m_fp, GS5_CMPLX);
        WriteFloat32(m_fp, m_fMin); // ref value = nodata or single data
        WriteInt16(m_fp, 0); // binary scale factor
        WriteInt16(m_fp, 0); // decimal scale factor
        WriteByte(m_fp, 0); // number of bits
        // Type of original data: 0=Floating, 1=Integer
        WriteByte(m_fp, GDALDataTypeIsFloating(m_eDT) ? 0 : 1);
        WriteByte(m_fp, 0);
        WriteByte(m_fp, m_bHasNoData ? 1 : 0); // 1 missing value
        WriteComplexPackingNoData();
        WriteUInt32(m_fp, GRIB2MISSING_u4);
        WriteUInt32(m_fp, 0);
        WriteByte(m_fp, 0);
        WriteByte(m_fp, 0);
        WriteUInt32(m_fp, 0);
        WriteByte(m_fp, 0);
        WriteUInt32(m_fp, 0);
        WriteByte(m_fp, 0);

        // Section 6: Bitmap section
        WriteUInt32(m_fp, 6); // section size
        WriteByte(m_fp, 6); // section number
        WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap

        // Section 7: Data Section
        WriteUInt32(m_fp, 5); // section size
        WriteByte(m_fp, 7); // section number

        return true;
    }

    const int nBitCorrectionForDec = static_cast<int>(
        ceil(m_nDecimalScaleFactor * log(10.0) / log(2.0)));
    const int nMaxBitsPerElt = std::max(1, std::min(31, (m_nBits > 0) ? m_nBits:
                GDALGetDataTypeSize(m_eDT)+ nBitCorrectionForDec));
    if( nMaxBitsPerElt > 0 &&
        m_nDataPoints > static_cast<GUInt32>(INT_MAX) / nMaxBitsPerElt )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Int overflow while computing maximum number of bits");
        VSIFree(pafData);
        return false;
    }

    // No idea what is the exact maximum bound... Take the value of simple
    // packing and multiply by 2, plus some constant...
    const int nMaxSize = 10000 + 2 * ((m_nDataPoints * nMaxBitsPerElt + 7) / 8);
    void* pabyData = VSI_MALLOC_VERBOSE(nMaxSize);
    if( pabyData == nullptr )
    {
        VSIFree(pafData);
        VSIFree(pabyData);
        return false;
    }

    const double dfScaledMaxDiff = (m_fMax == m_fMin) ? 1 : (m_fMax-m_fMin)* m_dfDecimalScale;
    if( m_nBits == 0 )
    {
        double dfTemp = log(ceil(dfScaledMaxDiff))/log(2.0);
        m_nBits = std::max(1, std::min(31, static_cast<int>(ceil(dfTemp))));
    }
    const int nMaxNum = (m_nBits == 31) ? INT_MAX : ((1 << m_nBits) - 1);
    double dfTemp = log(nMaxNum/dfScaledMaxDiff)/log(2.0);
    int nBinaryScaleFactor = static_cast<GInt16>(ceil(-dfTemp));

    // Indices expected by cmplxpack()
    enum
    {
        TMPL5_R_IDX = 0, // reference value
        TMPL5_E_IDX = 1, // binary scale factor
        TMPL5_D_IDX = 2, // decimal scale factor
        TMPL5_NBITS_IDX = 3, // number of bits
        TMPL5_TYPE_IDX = 4, // type of original data
        TMPL5_GROUP_SPLITTING_IDX = 5, // Group splitting method used
        TMPL5_MISSING_VALUE_MGNT_IDX = 6, // Missing value management used
        TMPL5_PRIMARY_MISSING_VALUE_IDX = 7, // Primary missing value
        TMPL5_SECONDARY_MISSING_VALUE_IDX = 8, // Secondary missing value
        TMPL5_NG_IDX = 9, // number of groups of data values
        TMPL5_REF_GROUP_WIDTHS_IDX = 10, // Reference for group widths
        // Number of bits used for the group widths
        TMPL5_NBITS_GROUP_WIDTHS_IDX = 11,
        TMPL5_REF_GROUP_LENGTHS_IDX = 12, // Reference for group lengths
        // Length increment for the group lengths
        TMPL5_LENGTH_INCR_GROUP_LENGTHS_IDX = 13,
        TMPL5_TRUE_LENGTH_LAST_GROUP_IDX = 14, // True length of last group
        // Number of bits used for the scaled group lengths
        TMPL5_NBITS_SCALED_GROUP_LENGTHS_IDX = 15,
        // Order of spatial differencing
        TMPL5_ORDER_SPATIAL_DIFFERENCE_IDX = 16,
        // Number of octets required in the data section to specify extra
        //descriptors needed for spatial differencing
        TMPL5_NB_OCTETS_EXTRA_DESCR_IDX = 17
    };

    g2int idrstmpl[TMPL5_NB_OCTETS_EXTRA_DESCR_IDX+1] = { 0 };
    idrstmpl[TMPL5_E_IDX] = nBinaryScaleFactor;
    idrstmpl[TMPL5_D_IDX] = m_nDecimalScaleFactor;
    idrstmpl[TMPL5_MISSING_VALUE_MGNT_IDX] = m_bHasNoData ? 1 : 0;
    idrstmpl[TMPL5_ORDER_SPATIAL_DIFFERENCE_IDX] = nSpatialDifferencingOrder;
    if( m_bHasNoData )
    {
        memcpy(&idrstmpl[TMPL5_PRIMARY_MISSING_VALUE_IDX], &fNoData, 4);
    }
    g2int nLengthPacked = 0;
    const int nTemplateNumber =
        (nSpatialDifferencingOrder > 0) ? GS5_CMPLXSEC : GS5_CMPLX;
    cmplxpack(pafData,m_nDataPoints,nTemplateNumber,idrstmpl,
            static_cast<unsigned char*>(pabyData),&nLengthPacked);
    CPLAssert(nLengthPacked <= nMaxSize);
    if( nLengthPacked < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Error while packing");
        VSIFree(pafData);
        VSIFree(pabyData);
        return false;
    }

    // Section 5: Data Representation Section
    WriteUInt32(m_fp, nTemplateNumber == GS5_CMPLX ? 47 : 49); // section size
    WriteByte(m_fp, 5); // section number
    WriteUInt32(m_fp, m_nDataPoints);
    WriteUInt16(m_fp, nTemplateNumber);
    float fRefValue;
    memcpy(&fRefValue, &idrstmpl[TMPL5_R_IDX], 4);
    WriteFloat32(m_fp, fRefValue);
    WriteInt16(m_fp, idrstmpl[TMPL5_E_IDX]);
    WriteInt16(m_fp, idrstmpl[TMPL5_D_IDX]);
    WriteByte(m_fp, idrstmpl[TMPL5_NBITS_IDX]);
    // Type of original data: 0=Floating, 1=Integer
    WriteByte(m_fp, GDALDataTypeIsFloating(m_eDT) ? 0 : 1);
    WriteByte(m_fp, idrstmpl[TMPL5_GROUP_SPLITTING_IDX]);
    WriteByte(m_fp, idrstmpl[TMPL5_MISSING_VALUE_MGNT_IDX]);
    WriteComplexPackingNoData();
    WriteUInt32(m_fp, GRIB2MISSING_u4);
    WriteUInt32(m_fp, idrstmpl[TMPL5_NG_IDX]);
    WriteByte(m_fp, idrstmpl[TMPL5_REF_GROUP_WIDTHS_IDX]);
    WriteByte(m_fp, idrstmpl[TMPL5_NBITS_GROUP_WIDTHS_IDX]);
    WriteUInt32(m_fp, idrstmpl[TMPL5_REF_GROUP_LENGTHS_IDX]);
    WriteByte(m_fp, idrstmpl[TMPL5_LENGTH_INCR_GROUP_LENGTHS_IDX]);
    WriteUInt32(m_fp, idrstmpl[TMPL5_TRUE_LENGTH_LAST_GROUP_IDX]);
    WriteByte(m_fp, idrstmpl[TMPL5_NBITS_SCALED_GROUP_LENGTHS_IDX]);
    if( nTemplateNumber == GS5_CMPLXSEC )
    {
        WriteByte(m_fp, nSpatialDifferencingOrder);
        WriteByte(m_fp, idrstmpl[TMPL5_NB_OCTETS_EXTRA_DESCR_IDX]);
    }

    // Section 6: Bitmap section
    WriteUInt32(m_fp, 6); // section size
    WriteByte(m_fp, 6); // section number
    WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap

    // Section 7: Data Section
    WriteUInt32(m_fp, 5 + nLengthPacked); // section size
    WriteByte(m_fp, 7); // section number
    if( static_cast<int>(VSIFWriteL( pabyData, 1, nLengthPacked, m_fp ))
                                                        != nLengthPacked )
    {
        VSIFree(pafData);
        VSIFree(pabyData);
        return false;
    }

    VSIFree(pafData);
    VSIFree(pabyData);

    return true;
}

/************************************************************************/
/*                             WriteIEEE()                              */
/************************************************************************/

// See http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_temp5-4.shtml
bool GRIB2Section567Writer::WriteIEEE(GDALProgressFunc pfnProgress,
                                      void * pProgressData )
{
    GDALDataType eReqDT;
    if( GDALGetDataTypeSize(m_eDT) <= 2 || m_eDT == GDT_Float32 )
        eReqDT = GDT_Float32;
    else
        eReqDT = GDT_Float64;

    // Section 5: Data Representation Section
    WriteUInt32(m_fp, 12); // section size
    WriteByte(m_fp, 5); // section number
    WriteUInt32(m_fp, m_nDataPoints);
    WriteUInt16(m_fp, GS5_IEEE);
    WriteByte(m_fp, (eReqDT == GDT_Float32) ? 1 : 2); // Precision

    // Section 6: Bitmap section
    WriteUInt32(m_fp, 6); // section size
    WriteByte(m_fp, 6); // section number
    WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap

    // Section 7: Data Section
    const size_t nBufferSize = m_nXSize * GDALGetDataTypeSizeBytes(eReqDT);
    // section size
    WriteUInt32(m_fp, static_cast<GUInt32>(5 + nBufferSize * m_nYSize));
    WriteByte(m_fp, 7); // section number
    void* pData = CPLMalloc( nBufferSize );
    // coverity[divide_by_zero]
    void *pScaledProgressData =
        GDALCreateScaledProgress(
            static_cast<double>(m_nBand - 1) / m_poSrcDS->GetRasterCount(),
            static_cast<double>(m_nBand) / m_poSrcDS->GetRasterCount(),
            pfnProgress, pProgressData );
    for( int i = 0; i < m_nYSize; i++ )
    {
        int iSrcLine = m_adfGeoTransform[5] < 0 ? m_nYSize - 1 - i: i;
        CPLErr eErr = m_poSrcDS->GetRasterBand(m_nBand)->RasterIO(
            GF_Read,
            m_nSplitAndSwap, iSrcLine,
            m_nXSize - m_nSplitAndSwap, 1,
            pData,
            m_nXSize - m_nSplitAndSwap, 1,
            eReqDT, 0, 0, nullptr);
        if ( eErr != CE_None )
        {
            CPLFree(pData);
            GDALDestroyScaledProgress(pScaledProgressData);
            return false;
        }
        if (m_nSplitAndSwap > 0)
        {
            eErr = m_poSrcDS->GetRasterBand(m_nBand)->RasterIO(
                GF_Read,
                0, iSrcLine,
                m_nSplitAndSwap, 1,
                reinterpret_cast<void*>(reinterpret_cast<GByte*>(pData) +
                     (m_nXSize - m_nSplitAndSwap) * GDALGetDataTypeSizeBytes(eReqDT)),
                m_nSplitAndSwap, 1,
                eReqDT, 0, 0, nullptr);
            if ( eErr != CE_None )
            {
                CPLFree(pData);
                GDALDestroyScaledProgress(pScaledProgressData);
                return false;
            }
        }
        if( m_fValOffset != 0.0 )
        {
            if( eReqDT == GDT_Float32 )
            {
                for( int j = 0; j < m_nXSize; j++ )
                {
                    static_cast<float*>(pData)[j] += m_fValOffset;
                }
            }
            else
            {
                for( int j = 0; j < m_nXSize; j++ )
                {
                    static_cast<double*>(pData)[j] += m_fValOffset;
                }
            }
        }
#ifdef CPL_LSB
        GDALSwapWords( pData, GDALGetDataTypeSizeBytes(eReqDT), m_nXSize,
                       GDALGetDataTypeSizeBytes(eReqDT) );
#endif
        if( VSIFWriteL( pData, 1, nBufferSize, m_fp ) != nBufferSize )
        {
            CPLFree(pData);
            GDALDestroyScaledProgress(pScaledProgressData);
            return false;
        }
        if( !GDALScaledProgress(
                static_cast<double>(i+1) / m_nYSize,
                nullptr, pScaledProgressData ) )
        {
            CPLFree(pData);
            GDALDestroyScaledProgress(pScaledProgressData);
            return false;
        }
    }
    GDALDestroyScaledProgress(pScaledProgressData);
    CPLFree(pData);

    return true;
}

/************************************************************************/
/*                        WrapArrayAsMemDataset()                       */
/************************************************************************/

static GDALDataset* WrapArrayAsMemDataset(int nXSize, int nYSize,
                                          GDALDataType eReducedDT,
                                          void* pData)
{
    CPLAssert( eReducedDT == GDT_Byte || eReducedDT == GDT_UInt16 );
    GDALDriver* poMEMDrv = reinterpret_cast<GDALDriver*>(
                                    GDALGetDriverByName("MEM"));
    GDALDataset* poMEMDS = poMEMDrv->Create("",
        nXSize, nYSize, 0, eReducedDT , nullptr);
    char** papszMEMOptions = nullptr;
    char szDataPointer[32];
    {
         GByte* pabyData = reinterpret_cast<GByte*>(pData);
        int nRet = CPLPrintPointer(szDataPointer,
#ifdef CPL_LSB
            pabyData,
#else
            (eReducedDT == GDT_Byte) ? pabyData + 1 : pabyData,
#endif
            sizeof(szDataPointer));
        szDataPointer[nRet] = 0;
    }
    papszMEMOptions = CSLSetNameValue(papszMEMOptions,
                                        "DATAPOINTER", szDataPointer);
    papszMEMOptions = CSLSetNameValue(papszMEMOptions,
                                        "PIXELOFFSET", "2");
    poMEMDS->AddBand( eReducedDT, papszMEMOptions );
    CSLDestroy(papszMEMOptions);
    return poMEMDS;
}

/************************************************************************/
/*                      GetRoundedToUpperPowerOfTwo()                   */
/************************************************************************/

static int GetRoundedToUpperPowerOfTwo(int nBits)
{
    if( nBits == 3 )
        nBits = 4;
    else if( nBits > 4 && nBits < 8 )
        nBits = 8;
    else if( nBits > 8 && nBits < 15 )
        nBits = 16;
    return nBits;
}

/************************************************************************/
/*                             GetScaledData()                          */
/************************************************************************/

static
GUInt16* GetScaledData(GUInt32 nDataPoints, const float* pafData,
                       float fMin, float fMax,
                       double dfDecimalScale, double dfMinScaled,
                       bool bOnlyPowerOfTwoDepthAllowed,
                       int& nBits,
                       GInt16& nBinaryScaleFactor)
{
    bool bDone = false;
    nBinaryScaleFactor = 0;
    GUInt16* panData = static_cast<GUInt16*>(
            VSI_MALLOC2_VERBOSE(nDataPoints, sizeof(GUInt16)));
    if( panData == nullptr )
    {
        return nullptr;
    }

    const double dfScaledMaxDiff = (fMax-fMin)* dfDecimalScale;
    if (nBits==0 )
    {
        nBits=(g2int)ceil(log(ceil(dfScaledMaxDiff))/log(2.0));
        if( nBits > 16 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "More than 16 bits of integer precision would be "
                     "required. Dropping precision to fit on 16 bits");
            nBits = 16;
        }
        else
        {
            bDone = true;
            for( GUInt32 i = 0; i < nDataPoints; i++ )
            {
                panData[i] = static_cast<GUInt16>(
                            0.5 + (pafData[i] * dfDecimalScale - dfMinScaled));
            }
        }
    }

    if( bOnlyPowerOfTwoDepthAllowed )
        nBits = GetRoundedToUpperPowerOfTwo(nBits);

    if (!bDone && nBits != 0)
    {
        if( nBits > 16 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Maximum bit depth supported is 16. Using that");
            nBits = 16;
        }
        const int nMaxNum = (1 << nBits) - 1;
        double dfTemp = log(nMaxNum/dfScaledMaxDiff)/log(2.0);
        nBinaryScaleFactor = static_cast<GInt16>(ceil(-dfTemp));
        double dfBinaryScale = pow(2.0, -1.0 * nBinaryScaleFactor);
        for( GUInt32 i = 0; i < nDataPoints; i++ )
        {
            panData[i] = static_cast<GUInt16>(
                0.5 +
                (pafData[i] * dfDecimalScale - dfMinScaled) * dfBinaryScale);
        }
    }

    return panData;
}

/************************************************************************/
/*                              WritePNG()                              */
/************************************************************************/

// See http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_temp5-41.shtml
bool GRIB2Section567Writer::WritePNG()
{
    float* pafData = GetFloatData();
    if( pafData == nullptr )
        return false;

    if( m_bUseZeroBits )
    {
        // Section 5: Data Representation Section
        WriteUInt32(m_fp, 21); // section size
        WriteByte(m_fp, 5); // section number
        WriteUInt32(m_fp, m_nDataPoints);
        WriteUInt16(m_fp, GS5_PNG);
        WriteFloat32(m_fp,
            static_cast<float>(m_dfMinScaled / m_dfDecimalScale)); // ref value
        WriteInt16(m_fp, 0); // Binary scale factor (E)
        WriteInt16(m_fp, 0); // Decimal scale factor (D)
        WriteByte(m_fp, 0); // Number of bits
        // Type of original data: 0=Floating, 1=Integer
        WriteByte(m_fp, GDALDataTypeIsFloating(m_eDT) ? 0 : 1);

        // Section 6: Bitmap section
        WriteUInt32(m_fp, 6); // section size
        WriteByte(m_fp, 6); // section number
        WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap

        // Section 7: Data Section
        WriteUInt32(m_fp, 5); // section size
        WriteByte(m_fp, 7); // section number

        CPLFree(pafData);

        return true;
    }

    GDALDriver* poPNGDriver = reinterpret_cast<GDALDriver*>(
                                GDALGetDriverByName("PNG"));
    if( poPNGDriver == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find PNG driver");
        return false;
    }

    GInt16 nBinaryScaleFactor = 0;
    GUInt16* panData = GetScaledData(m_nDataPoints, pafData, m_fMin, m_fMax,
                                     m_dfDecimalScale, m_dfMinScaled,
                                     true, m_nBits, nBinaryScaleFactor);
    if( panData == nullptr )
    {
        VSIFree(pafData);
        return false;
    }

    CPLFree(pafData);

    CPLStringList aosPNGOptions;
    aosPNGOptions.SetNameValue("NBITS", CPLSPrintf("%d", m_nBits));

    const GDALDataType eReducedDT = (m_nBits <= 8) ? GDT_Byte : GDT_UInt16;
    GDALDataset* poMEMDS = WrapArrayAsMemDataset(m_nXSize, m_nYSize, eReducedDT,
                                                 panData);

    CPLString osTmpFile(CPLSPrintf("/vsimem/grib_driver_%p.png", m_poSrcDS));
    GDALDataset* poPNGDS = poPNGDriver->CreateCopy(
        osTmpFile, poMEMDS, FALSE, aosPNGOptions.List(), nullptr, nullptr);
    if( poPNGDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "PNG compression failed");
        VSIUnlink(osTmpFile);
        delete poMEMDS;
        CPLFree(panData);
        return false;
    }
    delete poPNGDS;
    delete poMEMDS;
    CPLFree(panData);

    // Section 5: Data Representation Section
    WriteUInt32(m_fp, 21); // section size
    WriteByte(m_fp, 5); // section number
    WriteUInt32(m_fp, m_nDataPoints);
    WriteUInt16(m_fp, GS5_PNG);
    WriteFloat32(m_fp, static_cast<float>(m_dfMinScaled));
    WriteInt16(m_fp, nBinaryScaleFactor); // Binary scale factor (E)
    WriteInt16(m_fp, m_nDecimalScaleFactor); // Decimal scale factor (D)
    WriteByte(m_fp, m_nBits); // Number of bits
    // Type of original data: 0=Floating, 1=Integer
    WriteByte(m_fp, GDALDataTypeIsFloating(m_eDT) ? 0 : 1);

    // Section 6: Bitmap section
    WriteUInt32(m_fp, 6); // section size
    WriteByte(m_fp, 6); // section number
    WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap

    // Section 7: Data Section
    vsi_l_offset nDataLength = 0;
    GByte* pabyData = VSIGetMemFileBuffer(osTmpFile, &nDataLength, FALSE);
    WriteUInt32(m_fp, static_cast<GUInt32>(5 + nDataLength)); // section size
    WriteByte(m_fp, 7); // section number
    const size_t nDataLengthSize = static_cast<size_t>(nDataLength);
    const bool bOK = VSIFWriteL( pabyData, 1, nDataLengthSize, m_fp ) ==
                                                            nDataLengthSize;

    VSIUnlink(osTmpFile);
    VSIUnlink((osTmpFile + ".aux.xml").c_str());

    return bOK;
}

/************************************************************************/
/*                             WriteJPEG2000()                          */
/************************************************************************/

// See http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_temp5-40.shtml
bool GRIB2Section567Writer::WriteJPEG2000(char** papszOptions)
{
    float* pafData = GetFloatData();
    if( pafData == nullptr )
        return false;

    if( m_bUseZeroBits )
    {
        // Section 5: Data Representation Section
        WriteUInt32(m_fp, 23); // section size
        WriteByte(m_fp, 5); // section number
        WriteUInt32(m_fp, m_nDataPoints);
        WriteUInt16(m_fp, GS5_JPEG2000);
        WriteFloat32(m_fp,
            static_cast<float>(m_dfMinScaled / m_dfDecimalScale)); // ref val
        WriteInt16(m_fp, 0); // Binary scale factor (E)
        WriteInt16(m_fp, 0); // Decimal scale factor (D)
        WriteByte(m_fp, 0); // Number of bits
        // Type of original data: 0=Floating, 1=Integer
        WriteByte(m_fp, GDALDataTypeIsFloating(m_eDT) ? 0 : 1);
        WriteByte(m_fp, 0); // compression type: lossless
        WriteByte(m_fp, GRIB2MISSING_u1); // compression ratio

        // Section 6: Bitmap section
        WriteUInt32(m_fp, 6); // section size
        WriteByte(m_fp, 6); // section number
        WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap

        // Section 7: Data Section
        WriteUInt32(m_fp, 5); // section size
        WriteByte(m_fp, 7); // section number

        CPLFree(pafData);

        return true;
    }

    GDALDriver* poJ2KDriver = nullptr;
    const char* pszJ2KDriver = GetBandOption(
        papszOptions, nullptr, m_nBand, "JPEG2000_DRIVER", nullptr);
    if( pszJ2KDriver )
    {
        poJ2KDriver = reinterpret_cast<GDALDriver*>(
                                GDALGetDriverByName(pszJ2KDriver));
    }
    else
    {
        for( size_t i = 0; i < CPL_ARRAYSIZE(apszJ2KDrivers); i++ )
        {
            poJ2KDriver = reinterpret_cast<GDALDriver*>(
                                GDALGetDriverByName(apszJ2KDrivers[i]));
            if( poJ2KDriver )
            {
                CPLDebug("GRIB", "Using %s",
                            poJ2KDriver->GetDescription());
                break;
            }
        }
    }
    if( poJ2KDriver == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find JPEG2000 driver");
        VSIFree(pafData);
        return false;
    }

    GInt16 nBinaryScaleFactor = 0;
    GUInt16* panData = GetScaledData(m_nDataPoints, pafData, m_fMin, m_fMax,
                                     m_dfDecimalScale, m_dfMinScaled,
                                     false, m_nBits, nBinaryScaleFactor);
    if( panData == nullptr )
    {
        VSIFree(pafData);
        return false;
    }

    CPLFree(pafData);

    CPLStringList aosJ2KOptions;
    int nCompressionRatio = atoi(GetBandOption(papszOptions,
                                    nullptr, m_nBand, "COMPRESSION_RATIO", "1"));
    if( m_nDataPoints < 10000 && nCompressionRatio > 1 )
    {
        // Lossy compression with too few pixels is really lossy due to how
        // codec work
        CPLDebug("GRIB",
                 "Forcing JPEG2000 lossless mode given "
                 "the low number of pixels");
        nCompressionRatio = 1;
    }
    const bool bLossLess = nCompressionRatio <= 1;
    if( EQUAL(poJ2KDriver->GetDescription(), "JP2KAK") )
    {
        if( bLossLess )
        {
            aosJ2KOptions.SetNameValue("QUALITY", "100");
        }
        else
        {
            aosJ2KOptions.SetNameValue("QUALITY",
                CPLSPrintf("%d", std::max(1, 100 / nCompressionRatio) ));
        }
    }
    else if( EQUAL(poJ2KDriver->GetDescription(), "JP2OPENJPEG") )
    {
        if( bLossLess )
        {
            aosJ2KOptions.SetNameValue("QUALITY", "100");
            aosJ2KOptions.SetNameValue("REVERSIBLE", "YES");
        }
        else
        {
            aosJ2KOptions.SetNameValue("QUALITY",
                        CPLSPrintf("%f", 100.0 / nCompressionRatio ));
        }
    }
    else if( EQUAL(poJ2KDriver->GetDescription(), "JPEG2000") )
    {
        if( !bLossLess )
        {
            aosJ2KOptions.SetNameValue("mode", "real");
            aosJ2KOptions.SetNameValue("rate",
                        CPLSPrintf("%f", 1.0 / nCompressionRatio ));
        }
    }
    else if( EQUAL(poJ2KDriver->GetDescription(), "JP2ECW") )
    {
        if( bLossLess )
        {
            aosJ2KOptions.SetNameValue("TARGET", "0");
        }
        else
        {
            aosJ2KOptions.SetNameValue("TARGET",
                CPLSPrintf("%f", 100.0 - 100.0 / nCompressionRatio));
        }
    }
    aosJ2KOptions.SetNameValue("NBITS", CPLSPrintf("%d", m_nBits));

    const GDALDataType eReducedDT = (m_nBits <= 8) ? GDT_Byte : GDT_UInt16;
    GDALDataset* poMEMDS = WrapArrayAsMemDataset(
                                m_nXSize, m_nYSize, eReducedDT, panData);

    CPLString osTmpFile(CPLSPrintf("/vsimem/grib_driver_%p.j2k", m_poSrcDS));
    GDALDataset* poJ2KDS = poJ2KDriver->CreateCopy(
        osTmpFile, poMEMDS, FALSE, aosJ2KOptions.List(), nullptr, nullptr);
    if( poJ2KDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "JPEG2000 compression failed");
        VSIUnlink(osTmpFile);
        delete poMEMDS;
        CPLFree(panData);
        return false;
    }
    delete poJ2KDS;
    delete poMEMDS;
    CPLFree(panData);

    // Section 5: Data Representation Section
    WriteUInt32(m_fp, 23); // section size
    WriteByte(m_fp, 5); // section number
    WriteUInt32(m_fp, m_nDataPoints);
    WriteUInt16(m_fp, GS5_JPEG2000);
    WriteFloat32(m_fp, static_cast<float>(m_dfMinScaled));
    WriteInt16(m_fp, nBinaryScaleFactor); // Binary scale factor (E)
    WriteInt16(m_fp, m_nDecimalScaleFactor); // Decimal scale factor (D)
    WriteByte(m_fp, m_nBits); // Number of bits
    // Type of original data: 0=Floating, 1=Integer
    WriteByte(m_fp, GDALDataTypeIsFloating(m_eDT) ? 0 : 1);
    // compression type: lossless(0) or lossy(1)
    WriteByte(m_fp, bLossLess ? 0 : 1);
    WriteByte(m_fp, bLossLess ?
              GRIB2MISSING_u1 : nCompressionRatio); // compression ratio

    // Section 6: Bitmap section
    WriteUInt32(m_fp, 6); // section size
    WriteByte(m_fp, 6); // section number
    WriteByte(m_fp, GRIB2MISSING_u1); // no bitmap

    // Section 7: Data Section
    vsi_l_offset nDataLength = 0;
    GByte* pabyData = VSIGetMemFileBuffer(osTmpFile, &nDataLength, FALSE);
    WriteUInt32(m_fp, static_cast<GUInt32>(5 + nDataLength)); // section size
    WriteByte(m_fp, 7); // section number
    const size_t nDataLengthSize = static_cast<size_t>(nDataLength);
    const bool bOK = VSIFWriteL( pabyData, 1, nDataLengthSize, m_fp ) ==
                                                            nDataLengthSize;

    VSIUnlink(osTmpFile);
    VSIUnlink((osTmpFile + ".aux.xml").c_str());

    return bOK;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

bool GRIB2Section567Writer::Write(float fValOffset,
                                  char** papszOptions,
                                  GDALProgressFunc pfnProgress,
                                  void * pProgressData )
{
    m_fValOffset = fValOffset;

    typedef enum
    {
        SIMPLE_PACKING,
        COMPLEX_PACKING,
        IEEE_FLOATING_POINT,
        PNG,
        JPEG2000
    } GRIBDataEncoding;

    if( m_eDT != GDT_Byte &&
        m_eDT != GDT_UInt16 && m_eDT != GDT_Int16 &&
        m_eDT != GDT_UInt32 && m_eDT != GDT_Int32 &&
        m_eDT != GDT_Float32 && m_eDT != GDT_Float64 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported data type: %s", GDALGetDataTypeName(m_eDT));
        return false;
    }
    const char* pszDataEncoding =
        GetBandOption(papszOptions, nullptr, m_nBand, "DATA_ENCODING", "AUTO");
    GRIBDataEncoding eDataEncoding(SIMPLE_PACKING);
    const char* pszJ2KDriver = GetBandOption(
            papszOptions, nullptr, m_nBand, "JPEG2000_DRIVER", nullptr);
    const char* pszSpatialDifferencingOrder = GetBandOption(
            papszOptions, nullptr, m_nBand, "SPATIAL_DIFFERENCING_ORDER", nullptr);
    if( pszJ2KDriver && pszSpatialDifferencingOrder)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "JPEG2000_DRIVER and SPATIAL_DIFFERENCING_ORDER are not "
                 "compatible");
        return false;
    }

    if( m_bHasNoData && !EQUAL(pszDataEncoding, "COMPLEX_PACKING") &&
        pszSpatialDifferencingOrder == nullptr )
    {
        double* padfVals = static_cast<double*>(
                VSI_MALLOC2_VERBOSE(m_nXSize, sizeof(double)));
        if( padfVals == nullptr )
            return false;
        bool bFoundNoData = false;
        for( int j = 0; j < m_nYSize; j++ )
        {
            CPLErr eErr = m_poSrcDS->GetRasterBand(m_nBand)->RasterIO(
                GF_Read,
                0, j,
                m_nXSize, 1,
                padfVals,
                m_nXSize, 1,
                GDT_Float64, 0, 0, nullptr);
            if( eErr != CE_None )
            {
                VSIFree(padfVals);
                return false;
            }
            for( int i = 0; i < m_nXSize; i++ )
            {
                if( padfVals[i] == m_dfNoData )
                {
                    bFoundNoData = true;
                    break;
                }
            }
            if( bFoundNoData )
                break;
        }
        VSIFree(padfVals);

        if( !bFoundNoData )
        {
            m_bHasNoData = false;
        }
    }

    if( EQUAL(pszDataEncoding, "AUTO") )
    {
        if( m_bHasNoData || pszSpatialDifferencingOrder != nullptr )
        {
            eDataEncoding = COMPLEX_PACKING;
            CPLDebug("GRIB", "Using COMPLEX_PACKING");
        }
        else if( pszJ2KDriver != nullptr )
        {
            eDataEncoding = JPEG2000;
            CPLDebug("GRIB", "Using JPEG2000");
        }
        else if( m_eDT == GDT_Float32 || m_eDT == GDT_Float64 )
        {
            eDataEncoding = IEEE_FLOATING_POINT;
            CPLDebug("GRIB", "Using IEEE_FLOATING_POINT");
        }
        else
        {
            CPLDebug("GRIB", "Using SIMPLE_PACKING");
        }
    }
    else if( EQUAL(pszDataEncoding, "SIMPLE_PACKING") )
    {
        eDataEncoding = SIMPLE_PACKING;
    }
    else if( EQUAL(pszDataEncoding, "COMPLEX_PACKING") )
    {
        eDataEncoding = COMPLEX_PACKING;
    }
    else if( EQUAL(pszDataEncoding, "IEEE_FLOATING_POINT") )
    {
        eDataEncoding = IEEE_FLOATING_POINT;
    }
    else if( EQUAL(pszDataEncoding, "PNG") )
    {
        eDataEncoding = PNG;
    }
    else if( EQUAL(pszDataEncoding, "JPEG2000") )
    {
        eDataEncoding = JPEG2000;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported DATA_ENCODING=%s", pszDataEncoding);
        return false;
    }

    const char* pszBits = GetBandOption(
        papszOptions, nullptr, m_nBand, "NBITS", nullptr);
    if( pszBits == nullptr && eDataEncoding != IEEE_FLOATING_POINT )
    {
        pszBits = m_poSrcDS->GetRasterBand(m_nBand)->GetMetadataItem(
                "DRS_NBITS", "GRIB");
    }
    else if( pszBits != nullptr && eDataEncoding == IEEE_FLOATING_POINT )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "NBITS ignored for DATA_ENCODING = IEEE_FLOATING_POINT");
    }
    if( pszBits == nullptr )
    {
        pszBits = "0";
    }
    m_nBits = std::max(0, atoi(pszBits));
    if( m_nBits > 31 )
    {
        CPLError(CE_Warning, CPLE_NotSupported, "NBITS clamped to 31");
        m_nBits = 31;
    }


    const char* pszDecimalScaleFactor = GetBandOption(
        papszOptions, nullptr, m_nBand, "DECIMAL_SCALE_FACTOR", nullptr);
    if( pszDecimalScaleFactor != nullptr )
    {
        m_nDecimalScaleFactor = atoi(pszDecimalScaleFactor);
        if( m_nDecimalScaleFactor != 0 && eDataEncoding == IEEE_FLOATING_POINT )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "DECIMAL_SCALE_FACTOR ignored for "
                    "DATA_ENCODING = IEEE_FLOATING_POINT");
        }
        else if( m_nDecimalScaleFactor > 0 && !GDALDataTypeIsFloating(m_eDT) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "DECIMAL_SCALE_FACTOR > 0 makes no sense for integer "
                    "data types. Ignored");
            m_nDecimalScaleFactor = 0;
        }
    }
    else if( eDataEncoding != IEEE_FLOATING_POINT )
    {
        pszDecimalScaleFactor =
            m_poSrcDS->GetRasterBand(m_nBand)->GetMetadataItem(
                "DRS_DECIMAL_SCALE_FACTOR", "GRIB");
        if( pszDecimalScaleFactor != nullptr )
        {
            m_nDecimalScaleFactor = atoi(pszDecimalScaleFactor);
        }
    }
    m_dfDecimalScale = pow(10.0,static_cast<double>(m_nDecimalScaleFactor));

    if( pszJ2KDriver != nullptr && eDataEncoding != JPEG2000 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "JPEG2000_DRIVER option ignored for "
                 "non-JPEG2000 DATA_ENCODING");
    }
    if( pszSpatialDifferencingOrder && eDataEncoding != COMPLEX_PACKING )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "SPATIAL_DIFFERENCING_ORDER option ignored for "
                 "non-COMPLEX_PACKING DATA_ENCODING");
    }
    if( m_bHasNoData && eDataEncoding != COMPLEX_PACKING )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "non-COMPLEX_PACKING DATA_ENCODING cannot preserve nodata");
    }

    if( eDataEncoding == SIMPLE_PACKING )
    {
        return WriteSimplePacking();
    }
    else if( eDataEncoding == COMPLEX_PACKING )
    {
        const int nSpatialDifferencingOrder =
            pszSpatialDifferencingOrder ? atoi(pszSpatialDifferencingOrder) : 0;
        return WriteComplexPacking(nSpatialDifferencingOrder);
    }
    else if( eDataEncoding == IEEE_FLOATING_POINT )
    {
        return WriteIEEE(pfnProgress, pProgressData );
    }
    else if( eDataEncoding == PNG )
    {
        return WritePNG();
    }
    else /* if( eDataEncoding == JPEG2000 ) */
    {
        return WriteJPEG2000(papszOptions );
    }
}

/************************************************************************/
/*                           GetIDSOption()                             */
/************************************************************************/

static const char* GetIDSOption(char** papszOptions,
                                GDALDataset* poSrcDS,
                                int nBand,
                                const char* pszKey, const char* pszDefault)
{
    const char* pszValue = GetBandOption(
            papszOptions, nullptr,
            nBand, (CPLString("IDS_") + pszKey).c_str(), nullptr);
    if( pszValue == nullptr )
    {
        const char* pszIDS = GetBandOption(papszOptions, poSrcDS,
                                           nBand, "IDS", nullptr);
        if( pszIDS != nullptr )
        {
            char** papszTokens = CSLTokenizeString2(pszIDS, " ", 0);
            pszValue = CSLFetchNameValue(papszTokens, pszKey);
            if( pszValue )
                pszValue = CPLSPrintf("%s", pszValue);
            CSLDestroy(papszTokens);
        }
    }
    if( pszValue == nullptr )
        pszValue = pszDefault;
    return pszValue;
}

/************************************************************************/
/*                           WriteSection1()                            */
/************************************************************************/

static void WriteSection1( VSILFILE* fp, GDALDataset* poSrcDS, int nBand,
                           char** papszOptions )
{
    // Section 1: Identification Section
    WriteUInt32(fp, 21); // section size
    WriteByte(fp, 1); // section number

    GUInt16 nCenter = static_cast<GUInt16>(atoi(GetIDSOption(
            papszOptions, poSrcDS, nBand,
            "CENTER", CPLSPrintf("%d", GRIB2MISSING_u1))));
    WriteUInt16(fp, nCenter);

    GUInt16 nSubCenter = static_cast<GUInt16>(atoi(GetIDSOption(
        papszOptions, poSrcDS, nBand,
        "SUBCENTER", CPLSPrintf("%d", GRIB2MISSING_u2))));
    WriteUInt16(fp, nSubCenter);

    GByte nMasterTable = static_cast<GByte>(atoi(GetIDSOption(
        papszOptions, poSrcDS, nBand,
        "MASTER_TABLE", "2")));
    WriteByte(fp, nMasterTable);

    WriteByte(fp, 0); // local table

    GByte nSignfRefTime = static_cast<GByte>(atoi(GetIDSOption(
        papszOptions, poSrcDS, nBand, "SIGNF_REF_TIME", "0")));
    WriteByte(fp, nSignfRefTime); // Significance of reference time

    const char* pszRefTime = GetIDSOption(
        papszOptions, poSrcDS, nBand, "REF_TIME", "");
    int nYear = 1970, nMonth = 1, nDay = 1, nHour = 0, nMinute = 0, nSecond = 0;
    sscanf(pszRefTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                         &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond);
    WriteUInt16(fp, nYear);
    WriteByte(fp, nMonth);
    WriteByte(fp, nDay);
    WriteByte(fp, nHour);
    WriteByte(fp, nMinute);
    WriteByte(fp, nSecond);

    GByte nProdStatus = static_cast<GByte>(atoi(GetIDSOption(
        papszOptions, poSrcDS, nBand,
        "PROD_STATUS", CPLSPrintf("%d", GRIB2MISSING_u1))));
    WriteByte(fp, nProdStatus);

    GByte nType = static_cast<GByte>(atoi(GetIDSOption(
        papszOptions, poSrcDS, nBand,
        "TYPE", CPLSPrintf("%d", GRIB2MISSING_u1))));
    WriteByte(fp, nType);
}

/************************************************************************/
/*                        WriteAssembledPDS()                           */
/************************************************************************/

static void WriteAssembledPDS( VSILFILE* fp,
                               const gtemplate* mappds,
                               bool bWriteExt,
                               char** papszTokens,
                               std::vector<int>& anVals )
{
    const int iStart = bWriteExt ? mappds->maplen : 0;
    const int iEnd   = bWriteExt ? mappds->maplen + mappds->extlen :
                                   mappds->maplen;
    for( int i = iStart; i < iEnd; i++ )
    {
        const int nVal = atoi(papszTokens[i]);
        anVals.push_back(nVal);
        const int nEltSize = bWriteExt ?
                            mappds->ext[i-mappds->maplen] : mappds->map[i];
        if( nEltSize == 1 )
        {
            if( nVal < 0 || nVal > 255 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Value %d of index %d in PDS should be in [0,255] "
                    "range",
                    nVal, i);
            }
            WriteByte(fp, nVal);
        }
        else if( nEltSize == 2 )
        {
            if( nVal < 0 || nVal > 65535 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Value %d of index %d in PDS should be in [0,65535] "
                    "range",
                    nVal, i);
            }
            WriteUInt16(fp, nVal);
        }
        else if( nEltSize == 4 )
        {
            GIntBig nBigVal = CPLAtoGIntBig(papszTokens[i]);
            anVals[anVals.size()-1] = static_cast<int>(nBigVal);
            if( nBigVal < 0 || nBigVal > static_cast<GIntBig>(UINT_MAX) )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Value " CPL_FRMT_GIB " of index %d in PDS should be "
                    "in [0,%d] range",
                    nBigVal, i, INT_MAX);
            }
            WriteUInt32(fp, static_cast<GUInt32>(nBigVal));
        }
        else if( nEltSize == -1 )
        {
            if( nVal < -128 || nVal > 127 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Value %d of index %d in PDS should be in [-128,127] "
                    "range",
                    nVal, i);
            }
            WriteSByte(fp, nVal);
        }
        else if( nEltSize == -2 )
        {
            if( nVal < -32768 || nVal > 32767 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Value %d of index %d in PDS should be in "
                    "[-32768,32767] range",
                    nVal, i);
            }
            WriteInt16(fp, nVal);
        }
        else if( nEltSize == -4 )
        {
            GIntBig nBigVal = CPLAtoGIntBig(papszTokens[i]);
            if( nBigVal < INT_MIN || nBigVal > INT_MAX )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Value " CPL_FRMT_GIB " of index %d in PDS should be "
                    "in [%d,%d] range",
                    nBigVal, i, INT_MIN, INT_MAX);
            }
            WriteInt32(fp, atoi(papszTokens[i]));
        }
        else
        {
            CPLAssert( false );
        }
    }
}

/************************************************************************/
/*                         ComputeValOffset()                           */
/************************************************************************/

static float ComputeValOffset(int nTokens, char** papszTokens,
                              const char* pszInputUnit)
{
    float fValOffset = 0.0f;

    // Parameter category 0 = Temperature
    if( nTokens >= 2 && atoi(papszTokens[0]) == 0 )
    {
        // Cf https://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table4-2-0-0.shtml
        // PARAMETERS FOR DISCIPLINE 0 CATEGORY 0
        int nParamNumber = atoi(papszTokens[1]);
        if( (nParamNumber >= 0 && nParamNumber <= 18 &&
            nParamNumber != 8 && nParamNumber != 10 && nParamNumber != 11 &&
            nParamNumber != 16) ||
            nParamNumber == 21 ||
            nParamNumber == 27 )
        {
            if( pszInputUnit == nullptr || EQUAL(pszInputUnit, "C") ||
                EQUAL(pszInputUnit, "[C]") )
            {
                fValOffset = 273.15f;
                CPLDebug("GRIB",
                         "Applying a %f offset to convert from "
                         "Celsius to Kelvin",
                         fValOffset);
            }
        }
    }

    return fValOffset;
}

/************************************************************************/
/*                           WriteSection4()                            */
/************************************************************************/

static bool WriteSection4( VSILFILE* fp,
                           GDALDataset* poSrcDS,
                           int nBand,
                           char** papszOptions,
                           float& fValOffset )
{
    // Section 4: Product Definition Section
    vsi_l_offset nStartSection4 = VSIFTellL(fp);
    WriteUInt32(fp, GRIB2MISSING_u4); // section size
    WriteByte(fp, 4); // section number
    WriteUInt16(fp, 0); // Number of coordinate values after template

    // 0 = Analysis or forecast at a horizontal level or in a horizontal
    // layer at a point in time
    int nPDTN = atoi(GetBandOption(
            papszOptions, poSrcDS, nBand, "PDS_PDTN", "0"));
    const char* pszPDSTemplateNumbers = GetBandOption(
            papszOptions, nullptr, nBand, "PDS_TEMPLATE_NUMBERS", nullptr);
    const char* pszPDSTemplateAssembledValues = GetBandOption(
            papszOptions, nullptr, nBand, "PDS_TEMPLATE_ASSEMBLED_VALUES", nullptr);
    if( pszPDSTemplateNumbers == nullptr && pszPDSTemplateAssembledValues == nullptr )
    {
        pszPDSTemplateNumbers = GetBandOption(
            papszOptions, poSrcDS, nBand, "PDS_TEMPLATE_NUMBERS", nullptr);
    }
    CPLString osInputUnit;
    const char* pszInputUnit = GetBandOption(
            papszOptions, nullptr, nBand, "INPUT_UNIT", nullptr);
    if( pszInputUnit == nullptr )
    {
        const char* pszGribUnit =
            poSrcDS->GetRasterBand(nBand)->GetMetadataItem("GRIB_UNIT");
        if( pszGribUnit != nullptr )
        {
            osInputUnit = pszGribUnit;
            pszInputUnit = osInputUnit.c_str();
        }
    }
    WriteUInt16(fp, nPDTN); // PDTN
    if( nPDTN == 0 && pszPDSTemplateNumbers == nullptr &&
        pszPDSTemplateAssembledValues == nullptr )
    {
        // See http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_temp4-0.shtml
        WriteByte(fp, GRIB2MISSING_u1); // Parameter category = Missing
        WriteByte(fp, GRIB2MISSING_u1);// Parameter number = Missing
        WriteByte(fp, GRIB2MISSING_u1); // Type of generating process = Missing
        WriteByte(fp, 0); // Background generating process identifier
        // Analysis or forecast generating process identified
        WriteByte(fp, GRIB2MISSING_u1);
        WriteUInt16(fp, 0); // Hours
        WriteByte(fp, 0); // Minutes
        WriteByte(fp, 0); // Indicator of unit of time range: 0=Minute
        WriteUInt32(fp, 0); // Forecast time in units
        WriteByte(fp, 0); // Type of first fixed surface
        WriteByte(fp, 0); // Scale factor of first fixed surface
        WriteUInt32(fp, 0); // Type of second fixed surface
        WriteByte(fp, GRIB2MISSING_u1); // Type of second fixed surface
        WriteByte(fp, GRIB2MISSING_u1); // Scale factor of second fixed surface
        // Scaled value of second fixed surface
        WriteUInt32(fp, GRIB2MISSING_u4);
    }
    else if( pszPDSTemplateNumbers == nullptr &&
             pszPDSTemplateAssembledValues == nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "PDS_PDTN != 0 specified but both PDS_TEMPLATE_NUMBERS and "
                 "PDS_TEMPLATE_ASSEMBLED_VALUES missing");
        return false;
    }
    else if( pszPDSTemplateNumbers != nullptr &&
             pszPDSTemplateAssembledValues != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "PDS_TEMPLATE_NUMBERS and "
                 "PDS_TEMPLATE_ASSEMBLED_VALUES are exclusive");
        return false;
    }
    else if( pszPDSTemplateNumbers != nullptr )
    {
        char** papszTokens = CSLTokenizeString2(pszPDSTemplateNumbers, " ", 0);
        const int nTokens = CSLCount(papszTokens);

        fValOffset = ComputeValOffset(nTokens, papszTokens, pszInputUnit);

        for( int i = 0; papszTokens[i] != nullptr; i++ )
        {
            int nVal = atoi(papszTokens[i]);
            if( nVal < 0 || nVal > 255 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Value %d of index %d in PDS should be in [0,255] "
                    "range",
                    nVal, i);
            }
            WriteByte(fp, nVal);
        }
        CSLDestroy(papszTokens);

        // Read back section
        PatchSectionSize(fp, nStartSection4);

        vsi_l_offset nCurOffset = VSIFTellL(fp);
        VSIFSeekL(fp, nStartSection4, SEEK_SET);
        size_t nSizeSect4 = static_cast<size_t>(nCurOffset-nStartSection4);
        GByte* pabySect4 = static_cast<GByte*>(CPLMalloc(nSizeSect4));
        VSIFReadL(pabySect4, 1, nSizeSect4, fp);
        VSIFSeekL(fp, nCurOffset, SEEK_SET);

        // Check consistency with template definition
        g2int iofst = 0;
        g2int pdsnum = 0;
        g2int *pdstempl = nullptr;
        g2int mappdslen = 0;
        g2float *coordlist = nullptr;
        g2int numcoord = 0;
        int ret = g2_unpack4(pabySect4,static_cast<g2int>(nSizeSect4),&iofst,
                        &pdsnum,&pdstempl,&mappdslen,
                        &coordlist,&numcoord);
        CPLFree(pabySect4);
        if( ret == 0 )
        {
            gtemplate* mappds=extpdstemplate(pdsnum,pdstempl);
            free(pdstempl);
            free(coordlist);
            if( mappds )
            {
                int nTemplateByteCount = 0;
                for( int i = 0; i < mappds->maplen; i++ )
                    nTemplateByteCount += abs(mappds->map[i]);
                for( int i = 0; i < mappds->extlen; i++ )
                    nTemplateByteCount += abs(mappds->ext[i]);
                if( nTokens < nTemplateByteCount )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "PDS_PDTN = %d (with provided elements) requires "
                            "%d bytes in PDS_TEMPLATE_NUMBERS. "
                            "Only %d provided",
                            nPDTN,
                            nTemplateByteCount,
                            nTokens);
                    free(mappds->ext);
                    free(mappds);
                    return false;
                }
                else if( nTokens > nTemplateByteCount )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "PDS_PDTN = %d (with provided elements) requires "
                            "%d bytes in PDS_TEMPLATE_NUMBERS. "
                            "But %d provided. Extra bytes will be ignored "
                            "by readers",
                            nPDTN,
                            nTemplateByteCount,
                            nTokens);
                }

                free( mappds->ext );
                free( mappds );
            }
        }
        else
        {
            free(pdstempl);
            free(coordlist);
            CPLError(CE_Warning, CPLE_AppDefined,
                     "PDS_PDTN = %d is unknown. Product will not be "
                     "correctly read by this driver (but potentially valid "
                     "for other readers)",
                     nPDTN);
        }
    }
    else
    {
        gtemplate* mappds = getpdstemplate(nPDTN);
        if( mappds == nullptr )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "PDS_PDTN = %d is unknown, so it is not possible to use "
                    "PDS_TEMPLATE_ASSEMBLED_VALUES. Use PDS_TEMPLATE_NUMBERS "
                    "instead",
                     nPDTN);
            return false;
        }
        char** papszTokens =
            CSLTokenizeString2(pszPDSTemplateAssembledValues, " ", 0);
        const int nTokens = CSLCount(papszTokens);
        if( nTokens < mappds->maplen )
        {
             CPLError(CE_Failure, CPLE_AppDefined,
                      "PDS_PDTN = %d requires at least %d elements in "
                      "PDS_TEMPLATE_ASSEMBLED_VALUES. Only %d provided",
                      nPDTN, mappds->maplen, nTokens);
            free(mappds);
            CSLDestroy(papszTokens);
            return false;
        }

        fValOffset = ComputeValOffset(nTokens, papszTokens, pszInputUnit);

        std::vector<int> anVals;
        WriteAssembledPDS( fp, mappds, false, papszTokens, anVals);

        if( mappds->needext && !anVals.empty() )
        {
            free(mappds);
            mappds=extpdstemplate(nPDTN,&anVals[0]);
            if( mappds == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not get extended template definition");
                CSLDestroy(papszTokens);
                return false;
            }
            if( nTokens < mappds->maplen + mappds->extlen )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "PDS_PDTN = %d (with provided elements) requires "
                         "%d elements in PDS_TEMPLATE_ASSEMBLED_VALUES. "
                         "Only %d provided",
                         nPDTN,
                         mappds->maplen + mappds->extlen,
                         nTokens);
                free(mappds->ext);
                free(mappds);
                CSLDestroy(papszTokens);
                return false;
            }
            else if( nTokens > mappds->maplen + mappds->extlen )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "PDS_PDTN = %d (with provided elements) requires"
                         "%d elements in PDS_TEMPLATE_ASSEMBLED_VALUES. "
                         "But %d provided. Extra elements will be ignored",
                         nPDTN,
                         mappds->maplen + mappds->extlen,
                         nTokens);
            }

            WriteAssembledPDS( fp, mappds, true,
                               papszTokens, anVals);
        }

        free(mappds->ext);
        free(mappds);
        CSLDestroy(papszTokens);
    }
    PatchSectionSize(fp, nStartSection4);
    return true;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
GRIBDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                          int /* bStrict */, char ** papszOptions,
                          GDALProgressFunc pfnProgress, void * pProgressData )

{
    if( poSrcDS->GetRasterYSize() == 0 ||
        poSrcDS->GetRasterXSize() >
                INT_MAX / poSrcDS->GetRasterXSize() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create GRIB2 rasters with more than 2 billion pixels");
        return nullptr;
    }

    double adfGeoTransform[6];
    if( poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset must have a geotransform");
        return nullptr;
    }
    if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Geotransform with rotation terms not supported");
        return nullptr;
    }

    OGRSpatialReference oSRS;
    oSRS.importFromWkt(poSrcDS->GetProjectionRef());
    if( oSRS.IsProjected() )
    {
        const char *pszProjection = oSRS.GetAttrValue("PROJECTION");
        if( pszProjection == nullptr ||
            !(EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) ||
              EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) ||
              EQUAL(pszProjection, SRS_PT_MERCATOR_2SP) ||
              EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) ||
              EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) ||
              EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) ||
              EQUAL(pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA) ||
              EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA)) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported projection: %s",
                     pszProjection ? pszProjection : "");
            return nullptr;
        }
    }
    else if( !oSRS.IsGeographic() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported or missing spatial reference system");
        return nullptr;
    }

    const bool bAppendSubdataset =
        CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "APPEND_SUBDATASET", "NO"));
    VSILFILE* fp = VSIFOpenL(pszFilename, bAppendSubdataset ? "rb+" : "wb+");
    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszFilename);
        return nullptr;
    }
    VSIFSeekL(fp, 0, SEEK_END);

    vsi_l_offset nStartOffset = 0;
    vsi_l_offset nTotalSizeOffset = 0;
    int nSplitAndSwapColumn = 0;
    // Note: WRITE_SUBGRIDS=YES should not be used blindly currently, as it
    // does not check that the content of the DISCIPLINE and IDS are the same.
    // A smarter behavior would be to break into separate messages if needed
    const bool bWriteSubGrids = CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "WRITE_SUBGRIDS", "NO"));
    for( int nBand = 1; nBand <= poSrcDS->GetRasterCount(); nBand++ )
    {
        if( nBand == 1 || !bWriteSubGrids )
        {
            // Section 0: Indicator section
            nStartOffset = VSIFTellL(fp);
            VSIFWriteL( "GRIB", 4, 1, fp );
            WriteByte(fp, 0); // reserved
            WriteByte(fp, 0); // reserved
            int nDiscipline = atoi(GetBandOption(
                    papszOptions, poSrcDS, nBand, "DISCIPLINE", "0")); // 0 = Meteorological
            WriteByte(fp, nDiscipline); // discipline
            WriteByte(fp, 2); // GRIB edition number
            nTotalSizeOffset = VSIFTellL(fp);
            WriteUInt32(fp, GRIB2MISSING_u4); // dummy file size (high 32 bits)
            WriteUInt32(fp, GRIB2MISSING_u4); // dummy file size (low 32 bits)

            // Section 1: Identification Section
            WriteSection1( fp, poSrcDS, nBand, papszOptions );

            // Section 2: Local use section
            WriteUInt32(fp, 5); // section size
            WriteByte(fp, 2); // section number

            // Section 3: Grid Definition Section
            GRIB2Section3Writer oSection3(fp, poSrcDS);
            if (!oSection3.Write())
            {
                VSIFCloseL(fp);
                return nullptr;
            }
            nSplitAndSwapColumn = oSection3.SplitAndSwap();
        }

        // Section 4: Product Definition Section
        float fValOffset = 0.0f;
        if( !WriteSection4( fp, poSrcDS, nBand, papszOptions, fValOffset ) )
        {
            VSIFCloseL(fp);
            return nullptr;
        }

        // Section 5, 6 and 7
        if( !GRIB2Section567Writer(fp, poSrcDS, nBand, nSplitAndSwapColumn).
                Write(fValOffset, papszOptions, pfnProgress, pProgressData) )
        {
            VSIFCloseL(fp);
            return nullptr;
        }

        if( nBand == poSrcDS->GetRasterCount() || !bWriteSubGrids )
        {
            // Section 8: End section
            VSIFWriteL( "7777", 4, 1, fp );

            // Patch total message size at end of section 0
            vsi_l_offset nCurOffset = VSIFTellL(fp);
            if( nCurOffset - nStartOffset > INT_MAX )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "GRIB message larger than 2 GB");
                VSIFCloseL(fp);
                return nullptr;
            }
            GUInt32 nTotalSize = static_cast<GUInt32>(nCurOffset - nStartOffset);
            VSIFSeekL(fp, nTotalSizeOffset, SEEK_SET );
            WriteUInt32(fp, 0); // file size (high 32 bits)
            WriteUInt32(fp, nTotalSize); // file size (low 32 bits)

            VSIFSeekL(fp, nCurOffset, SEEK_SET );
        }

        if( pfnProgress &&
            !pfnProgress(static_cast<double>(nBand) / poSrcDS->GetRasterCount(),
                    nullptr, pProgressData ) )
        {
            VSIFCloseL(fp);
            return nullptr;
        }
    }

    VSIFCloseL(fp);

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}
