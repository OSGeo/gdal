    /******************************************************************************
 *
 * Project:  ENVI .hdr Driver
 * Purpose:  Implementation of ENVI .hdr labelled raw raster support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Maintainer: Chris Padwick (cpadwick at ittvis.com)
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "envidataset.h"
#include "rawdataset.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <limits>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

// TODO(schwehr): This really should be defined in port/somewhere.h.
constexpr double kdfDegToRad = M_PI / 180.0;
constexpr double kdfRadToDeg = 180.0 / M_PI;

constexpr int anUsgsEsriZones[] =
{
  101, 3101,
  102, 3126,
  201, 3151,
  202, 3176,
  203, 3201,
  301, 3226,
  302, 3251,
  401, 3276,
  402, 3301,
  403, 3326,
  404, 3351,
  405, 3376,
  406, 3401,
  407, 3426,
  501, 3451,
  502, 3476,
  503, 3501,
  600, 3526,
  700, 3551,
  901, 3601,
  902, 3626,
  903, 3576,
 1001, 3651,
 1002, 3676,
 1101, 3701,
 1102, 3726,
 1103, 3751,
 1201, 3776,
 1202, 3801,
 1301, 3826,
 1302, 3851,
 1401, 3876,
 1402, 3901,
 1501, 3926,
 1502, 3951,
 1601, 3976,
 1602, 4001,
 1701, 4026,
 1702, 4051,
 1703, 6426,
 1801, 4076,
 1802, 4101,
 1900, 4126,
 2001, 4151,
 2002, 4176,
 2101, 4201,
 2102, 4226,
 2103, 4251,
 2111, 6351,
 2112, 6376,
 2113, 6401,
 2201, 4276,
 2202, 4301,
 2203, 4326,
 2301, 4351,
 2302, 4376,
 2401, 4401,
 2402, 4426,
 2403, 4451,
 2500,    0,
 2501, 4476,
 2502, 4501,
 2503, 4526,
 2600,    0,
 2601, 4551,
 2602, 4576,
 2701, 4601,
 2702, 4626,
 2703, 4651,
 2800, 4676,
 2900, 4701,
 3001, 4726,
 3002, 4751,
 3003, 4776,
 3101, 4801,
 3102, 4826,
 3103, 4851,
 3104, 4876,
 3200, 4901,
 3301, 4926,
 3302, 4951,
 3401, 4976,
 3402, 5001,
 3501, 5026,
 3502, 5051,
 3601, 5076,
 3602, 5101,
 3701, 5126,
 3702, 5151,
 3800, 5176,
 3900,    0,
 3901, 5201,
 3902, 5226,
 4001, 5251,
 4002, 5276,
 4100, 5301,
 4201, 5326,
 4202, 5351,
 4203, 5376,
 4204, 5401,
 4205, 5426,
 4301, 5451,
 4302, 5476,
 4303, 5501,
 4400, 5526,
 4501, 5551,
 4502, 5576,
 4601, 5601,
 4602, 5626,
 4701, 5651,
 4702, 5676,
 4801, 5701,
 4802, 5726,
 4803, 5751,
 4901, 5776,
 4902, 5801,
 4903, 5826,
 4904, 5851,
 5001, 6101,
 5002, 6126,
 5003, 6151,
 5004, 6176,
 5005, 6201,
 5006, 6226,
 5007, 6251,
 5008, 6276,
 5009, 6301,
 5010, 6326,
 5101, 5876,
 5102, 5901,
 5103, 5926,
 5104, 5951,
 5105, 5976,
 5201, 6001,
 5200, 6026,
 5200, 6076,
 5201, 6051,
 5202, 6051,
 5300,    0,
 5400,    0
};

/************************************************************************/
/*                           ITTVISToUSGSZone()                         */
/*                                                                      */
/*      Convert ITTVIS style state plane zones to NOS style state       */
/*      plane zones.  The ENVI default is to use the new NOS zones,     */
/*      but the old state plane zones can be used.  Handle this.        */
/************************************************************************/

static int ITTVISToUSGSZone( int nITTVISZone )

{
    // TODO(schwehr): int anUsgsEsriZones[] -> a std::set and std::map.
    const int nPairs = sizeof(anUsgsEsriZones) / (2 * sizeof(int));

    // Default is to use the zone as-is, as long as it is in the
    // available list
    for( int i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i * 2] == nITTVISZone )
            return anUsgsEsriZones[i * 2];
    }

    // If not found in the new style, see if it is present in the
    // old style list and convert it.  We don't expect to see this
    // often, but older files allowed it and may still exist.
    for( int i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i * 2 + 1] == nITTVISZone )
            return anUsgsEsriZones[i * 2];
    }

    return nITTVISZone;  // Perhaps it *is* the USGS zone?
}

/************************************************************************/
/*                            ENVIDataset()                             */
/************************************************************************/

ENVIDataset::ENVIDataset() :
    fpImage(nullptr),
    fp(nullptr),
    pszHDRFilename(nullptr),
    bFoundMapinfo(false),
    bHeaderDirty(false),
    bFillFile(false),
    interleave(BSQ)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ENVIDataset()                            */
/************************************************************************/

ENVIDataset::~ENVIDataset()

{
    ENVIDataset::FlushCache(true);
    if( fpImage )
    {
        // Make sure the binary file has the expected size
        if( !bSuppressOnClose && bFillFile && nBands > 0)
        {
            const int nDataSize =
                GDALGetDataTypeSizeBytes(GetRasterBand(1)->GetRasterDataType());
            const vsi_l_offset nExpectedFileSize =
                static_cast<vsi_l_offset>(nRasterXSize) *
                nRasterYSize * nBands * nDataSize;
            if( VSIFSeekL(fpImage, 0, SEEK_END) != 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            }
            if( VSIFTellL(fpImage) < nExpectedFileSize)
            {
                GByte byVal = 0;
                if( VSIFSeekL(fpImage, nExpectedFileSize - 1, SEEK_SET) != 0 ||
                    VSIFWriteL(&byVal, 1, 1, fpImage) == 0 )
                {
                    CPLError(CE_Failure, CPLE_FileIO, "I/O error");
                }
            }
        }
        if( VSIFCloseL(fpImage) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }
    if( fp )
    {
        if( VSIFCloseL(fp) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }
    if( !m_asGCPs.empty() )
    {
        GDALDeinitGCPs(static_cast<int>(m_asGCPs.size()), m_asGCPs.data());
    }

    // Should be called before pszHDRFilename is freed.
    CleanupPostFileClosing();

    CPLFree(pszHDRFilename);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void ENVIDataset::FlushCache(bool bAtClosing)

{
    RawDataset::FlushCache(bAtClosing);

    GDALRasterBand *band = GetRasterCount() > 0 ? GetRasterBand(1) : nullptr;

    if ( band == nullptr || !bHeaderDirty || (bAtClosing && bSuppressOnClose) )
        return;

    // If opening an existing file in Update mode (i.e. "r+") we need to make
    // sure any existing content is cleared, otherwise the file may contain
    // trailing content from the previous write.
    if( VSIFTruncateL(fp, 0) != 0 )
        return;

    if( VSIFSeekL(fp, 0, SEEK_SET) != 0 )
        return;

    // Rewrite out the header.
    bool bOK = VSIFPrintfL(fp, "ENVI\n") >= 0;
    if ("" != sDescription)
        bOK &= VSIFPrintfL(fp, "description = {\n%s}\n",
                           sDescription.c_str()) >= 0;
    bOK &= VSIFPrintfL(fp, "samples = %d\nlines   = %d\nbands   = %d\n",
                       nRasterXSize, nRasterYSize, nBands) >= 0;

    char **catNames = band->GetCategoryNames();

    bOK &= VSIFPrintfL(fp, "header offset = 0\n") >= 0;
    if (nullptr == catNames)
        bOK &= VSIFPrintfL(fp, "file type = ENVI Standard\n") >= 0;
    else
        bOK &= VSIFPrintfL(fp, "file type = ENVI Classification\n") >= 0;

    const int iENVIType = GetEnviType(band->GetRasterDataType());
    bOK &= VSIFPrintfL(fp, "data type = %d\n", iENVIType) >= 0;
    const char *pszInterleaving = nullptr;
    switch( interleave )
    {
    case BIP:
        pszInterleaving = "bip";  // Interleaved by pixel.
        break;
    case BIL:
        pszInterleaving = "bil";  // Interleaved by line.
        break;
    case BSQ:
        pszInterleaving = "bsq";  // Band sequential by default.
        break;
    default:
        pszInterleaving = "bsq";
        break;
    }
    bOK &= VSIFPrintfL(fp, "interleave = %s\n", pszInterleaving) >= 0;

    const char* pszByteOrder = m_aosHeader["byte_order"];
    if( pszByteOrder )
    {
        // Supposed to be required
        bOK &= VSIFPrintfL(fp, "byte order = %s\n", pszByteOrder) >= 0;
    }

    // Write class and color information.
    catNames = band->GetCategoryNames();
    if (nullptr != catNames)
    {
        int nrClasses = 0;
        while (*catNames++)
            ++nrClasses;

        if (nrClasses > 0)
        {
            bOK &= VSIFPrintfL(fp, "classes = %d\n", nrClasses) >= 0;

            GDALColorTable *colorTable = band->GetColorTable();
            if (nullptr != colorTable)
            {
                const int nrColors =
                    std::min(nrClasses, colorTable->GetColorEntryCount());
                bOK &= VSIFPrintfL(fp, "class lookup = {\n") >= 0;
                for (int i = 0; i < nrColors; ++i)
                {
                    const GDALColorEntry *color = colorTable->GetColorEntry(i);
                    bOK &= VSIFPrintfL(fp, "%d, %d, %d",
                                       color->c1, color->c2, color->c3) >= 0;
                    if (i < nrColors - 1)
                    {
                        bOK &= VSIFPrintfL(fp, ", ") >= 0;
                        if( 0 == (i + 1) % 5 )
                            bOK &= VSIFPrintfL(fp, "\n") >= 0;
                    }
                }
                bOK &= VSIFPrintfL(fp, "}\n") >= 0;
            }

            catNames = band->GetCategoryNames();
            if (nullptr != *catNames)
            {
                bOK &= VSIFPrintfL(fp, "class names = {\n%s", *catNames) >= 0;
                catNames++;
                int i = 0;
                while (*catNames)
                {
                    bOK &= VSIFPrintfL(fp, ",") >= 0;
                    if (0 == (++i) % 5)
                        bOK &= VSIFPrintfL(fp, "\n") >= 0;
                    bOK &= VSIFPrintfL(fp, " %s", *catNames) >= 0;
                    catNames++;
                }
                bOK &= VSIFPrintfL(fp, "}\n") >= 0;
            }
        }
    }

    // Write the rest of header.

    // Only one map info type should be set:
    //     - rpc
    //     - pseudo/gcp
    //     - standard
    if ( !WriteRpcInfo() )  // Are rpcs in the metadata?
    {
        if ( !WritePseudoGcpInfo() )  // are gcps in the metadata
        {
            WriteProjectionInfo();  // standard - affine xform/coord sys str
        }
    }

    bOK &= VSIFPrintfL(fp, "band names = {\n") >= 0;
    for ( int i = 1; i <= nBands; i++ )
    {
        CPLString sBandDesc = GetRasterBand(i)->GetDescription();

        if ( sBandDesc == "" )
            sBandDesc = CPLSPrintf("Band %d", i);
        bOK &= VSIFPrintfL(fp, "%s", sBandDesc.c_str()) >= 0;
        if ( i != nBands )
            bOK &= VSIFPrintfL(fp, ",\n") >= 0;
    }
    bOK &= VSIFPrintfL(fp, "}\n") >= 0;

    int bHasNoData = FALSE;
    double dfNoDataValue = band->GetNoDataValue(&bHasNoData);
    if( bHasNoData )
    {
        bOK &= VSIFPrintfL(fp, "data ignore value = %.18g\n", dfNoDataValue) >= 0;
    }

    // Write the metadata that was read into the ENVI domain.
    char **papszENVIMetadata = GetMetadata("ENVI");

    const int count = CSLCount(papszENVIMetadata);

    // For every item of metadata in the ENVI domain.
    for (int i = 0; i < count; i++)
    {
        // Split the entry into two parts at the = character.
        char *pszEntry = papszENVIMetadata[i];
        char **papszTokens = CSLTokenizeString2(
            pszEntry, "=", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);

        if (CSLCount(papszTokens) != 2)
        {
            CPLDebug("ENVI",
                     "Line of header file could not be split at = into "
                     "two elements: %s", papszENVIMetadata[i]);
            CSLDestroy(papszTokens);
            continue;
        }
        // Replace _'s in the string with spaces
        std::string poKey(papszTokens[0]);
        std::replace(poKey.begin(), poKey.end(), '_', ' ');

        // Don't write it out if it is one of the bits of metadata that is
        // written out elsewhere in this routine.
        if ( poKey == "description" ||
             poKey == "samples" ||
             poKey == "lines" ||
             poKey == "bands" ||
             poKey == "header offset" ||
             poKey == "file type" ||
             poKey == "data type" ||
             poKey == "interleave" ||
             poKey == "byte order" ||
             poKey == "class names" ||
             poKey == "band names" ||
             poKey == "map info" ||
             poKey == "projection info" ||
             poKey == "data ignore value" )
        {
            CSLDestroy(papszTokens);
            continue;
        }
        bOK &= VSIFPrintfL(fp, "%s = %s\n", poKey.c_str(), papszTokens[1]) >= 0;
        CSLDestroy(papszTokens);
    }

    if( !bOK )
        return;

    bHeaderDirty = false;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ENVIDataset::GetFileList()

{
    // Main data file, etc.
    char **papszFileList = RawDataset::GetFileList();

    // Header file.
    papszFileList = CSLAddString(papszFileList, pszHDRFilename);

    // Statistics file
    if (!osStaFilename.empty())
        papszFileList = CSLAddString(papszFileList, osStaFilename);

    return papszFileList;
}

/************************************************************************/
/*                           GetEPSGGeogCS()                            */
/*                                                                      */
/*      Try to establish what the EPSG code for this coordinate         */
/*      systems GEOGCS might be.  Returns -1 if no reasonable guess     */
/*      can be made.                                                    */
/*                                                                      */
/*      TODO: We really need to do some name lookups.                   */
/************************************************************************/

static int ENVIGetEPSGGeogCS( const OGRSpatialReference *poThis )

{
    const char *pszAuthName = poThis->GetAuthorityName("GEOGCS");

    // Do we already have it?
    if( pszAuthName != nullptr && EQUAL(pszAuthName, "epsg") )
        return atoi(poThis->GetAuthorityCode("GEOGCS"));

    // Get the datum and geogcs names.
    const char *pszGEOGCS = poThis->GetAttrValue("GEOGCS");
    const char *pszDatum = poThis->GetAttrValue("DATUM");

    // We can only operate on coordinate systems with a geogcs.
    if( pszGEOGCS == nullptr || pszDatum == nullptr )
        return -1;

    // Is this a "well known" geographic coordinate system?
    const bool bWGS = strstr(pszGEOGCS, "WGS") ||
                      strstr(pszDatum, "WGS") ||
                      strstr(pszGEOGCS, "World Geodetic System") ||
                      strstr(pszGEOGCS, "World_Geodetic_System") ||
                      strstr(pszDatum, "World Geodetic System") ||
                      strstr(pszDatum, "World_Geodetic_System");

    const bool bNAD = strstr(pszGEOGCS, "NAD") ||
                      strstr(pszDatum, "NAD") ||
                      strstr(pszGEOGCS, "North American") ||
                      strstr(pszGEOGCS, "North_American") ||
                      strstr(pszDatum, "North American") ||
                      strstr(pszDatum, "North_American");

    if( bWGS && (strstr(pszGEOGCS, "84") || strstr(pszDatum, "84")) )
        return 4326;

    if( bWGS && (strstr(pszGEOGCS, "72") || strstr(pszDatum, "72")) )
        return 4322;

    if( bNAD && (strstr(pszGEOGCS, "83") || strstr(pszDatum, "83")) )
        return 4269;

    if( bNAD && (strstr(pszGEOGCS, "27") || strstr(pszDatum, "27")) )
        return 4267;

    // If we know the datum, associate the most likely GCS with it.
    pszAuthName = poThis->GetAuthorityName("GEOGCS|DATUM");

    if( pszAuthName != nullptr && EQUAL(pszAuthName, "epsg") &&
        poThis->GetPrimeMeridian() == 0.0 )
    {
        const int nDatum = atoi(poThis->GetAuthorityCode("GEOGCS|DATUM"));

        if( nDatum >= 6000 && nDatum <= 6999 )
            return nDatum - 2000;
    }

    return -1;
}

/************************************************************************/
/*                        WriteProjectionInfo()                         */
/************************************************************************/

void ENVIDataset::WriteProjectionInfo()

{
    // Format the location (geotransform) portion of the map info line.
    CPLString osLocation;
    CPLString osRotation;

    const double dfPixelXSize = sqrt(adfGeoTransform[1] * adfGeoTransform[1] +
                                     adfGeoTransform[2] * adfGeoTransform[2]);
    const double dfPixelYSize = sqrt(adfGeoTransform[4] * adfGeoTransform[4] +
                                     adfGeoTransform[5] * adfGeoTransform[5]);
    const bool bHasNonDefaultGT =
        adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0 ||
        adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0 ||
        adfGeoTransform[4] != 0.0 || adfGeoTransform[5] != 1.0;
    if( adfGeoTransform[1] > 0.0 && adfGeoTransform[2] == 0.0 &&
        adfGeoTransform[4] == 0.0 && adfGeoTransform[5] > 0.0 )
    {
        osRotation = ", rotation=180";
    }
    else if( bHasNonDefaultGT )
    {
        const double dfRotation1 =
            -atan2(-adfGeoTransform[2], adfGeoTransform[1]) * kdfRadToDeg;
        const double dfRotation2 =
            -atan2(-adfGeoTransform[4], -adfGeoTransform[5]) * kdfRadToDeg;
        const double dfRotation = (dfRotation1 + dfRotation2) / 2.0;

        if( fabs(dfRotation1 - dfRotation2) > 1e-5 )
        {
            CPLDebug("ENVI", "rot1 = %.15g, rot2 = %.15g",
                     dfRotation1, dfRotation2);
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geotransform matrix has non rotational terms");
        }
        if( fabs(dfRotation) > 1e-5 )
        {
            osRotation.Printf(", rotation=%.15g", dfRotation);
        }
    }

    osLocation.Printf("1, 1, %.15g, %.15g, %.15g, %.15g",
                      adfGeoTransform[0], adfGeoTransform[3],
                      dfPixelXSize, dfPixelYSize);

    // Minimal case - write out simple geotransform if we have a
    // non-default geotransform.
    if( m_oSRS.IsEmpty() || m_oSRS.IsLocal() )
    {
        if( bHasNonDefaultGT )
        {
            const char *pszHemisphere = "North";
            if( VSIFPrintfL(fp, "map info = {Arbitrary, %s, %d, %s%s}\n",
                            osLocation.c_str(), 0, pszHemisphere,
                            osRotation.c_str()) < 0)
                return;
        }
        return;
    }

    // Try to translate the datum and get major/minor ellipsoid values.
    const OGRSpatialReference& oSRS = m_oSRS;
    const int nEPSG_GCS = ENVIGetEPSGGeogCS(&oSRS);
    CPLString osDatum;

    if( nEPSG_GCS == 4326 )
        osDatum = "WGS-84";
    else if( nEPSG_GCS == 4322 )
        osDatum = "WGS-72";
    else if( nEPSG_GCS == 4269 )
        osDatum = "North America 1983";
    else if( nEPSG_GCS == 4267 )
        osDatum = "North America 1927";
    else if( nEPSG_GCS == 4230 )
        osDatum = "European 1950";
    else if( nEPSG_GCS == 4277 )
        osDatum = "Ordnance Survey of Great Britain '36";
    else if( nEPSG_GCS == 4291 )
        osDatum = "SAD-69/Brazil";
    else if( nEPSG_GCS == 4283 )
        osDatum = "Geocentric Datum of Australia 1994";
    else if( nEPSG_GCS == 4275 )
        osDatum = "Nouvelle Triangulation Francaise IGN";

    const CPLString osCommaDatum =
        osDatum.empty() ? "" : ("," + osDatum);

    const double dfA = oSRS.GetSemiMajor();
    const double dfB = oSRS.GetSemiMinor();

    // Do we have unusual linear units?
    const double dfFeetPerMeter = 0.3048;
    const CPLString osOptionalUnits =
        fabs(oSRS.GetLinearUnits() - dfFeetPerMeter) < 0.0001
        ? ", units=Feet" : "";

    // Handle UTM case.
    const char *pszProjName = oSRS.GetAttrValue("PROJECTION");
    int bNorth = FALSE;
    const int iUTMZone = oSRS.GetUTMZone(&bNorth);
    bool bOK = true;
    if ( iUTMZone )
    {
        const char *pszHemisphere = bNorth ? "North" : "South";

        bOK &= VSIFPrintfL(fp, "map info = {UTM, %s, %d, %s%s%s%s}\n",
                           osLocation.c_str(), iUTMZone, pszHemisphere,
                           osCommaDatum.c_str(), osOptionalUnits.c_str(),
                           osRotation.c_str()) >= 0;
    }
    else if( oSRS.IsGeographic() )
    {
        bOK &= VSIFPrintfL(fp, "map info = {Geographic Lat/Lon, %s%s%s}\n",
                           osLocation.c_str(), osCommaDatum.c_str(),
                           osRotation.c_str()) >= 0;
    }
    else if( pszProjName == nullptr )
    {
        // What to do?
    }
    else if( EQUAL(pszProjName, SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
        bOK &= VSIFPrintfL(fp, "map info = {New Zealand Map Grid, %s%s%s%s}\n",
                           osLocation.c_str(),
                           osCommaDatum.c_str(), osOptionalUnits.c_str(),
                           osRotation.c_str()) >= 0;

        bOK &= VSIFPrintfL(fp,
                           "projection info = {39, %.16g, %.16g, %.16g, %.16g, "
                           "%.16g, %.16g%s, New Zealand Map Grid}\n",
                           dfA, dfB,
                           oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                           oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                           oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                           oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                           osCommaDatum.c_str()) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        bOK &= VSIFPrintfL(fp, "map info = {Transverse Mercator, %s%s%s%s}\n",
                           osLocation.c_str(),
                           osCommaDatum.c_str(), osOptionalUnits.c_str(),
                           osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {3, %.16g, %.16g, %.16g, ""%.16g, %.16g, "
                "%.16g, %.16g%s, Transverse Mercator}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0),
                osCommaDatum.c_str()) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) ||
             EQUAL(pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM) )
    {
        bOK &= VSIFPrintfL(fp, "map info = {Lambert Conformal Conic, %s%s%s%s}\n",
                           osLocation.c_str(), osCommaDatum.c_str(),
                           osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {4, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g, %.16g, %.16g%s, Lambert Conformal Conic}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0),
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0),
                osCommaDatum.c_str()) >= 0;
    }
    else if( EQUAL(pszProjName,
                   SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        bOK &=
            VSIFPrintfL(fp, "map info = {Hotine Oblique Mercator A, %s%s%s%s}\n",
                        osLocation.c_str(), osCommaDatum.c_str(),
                        osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {5, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g, %.16g, %.16g, %.16g, %.16g%s, "
                "Hotine Oblique Mercator A}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0),
                oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0),
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_2, 0.0),
                oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0),
                osCommaDatum.c_str()) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        bOK &=
            VSIFPrintfL(fp, "map info = {Hotine Oblique Mercator B, %s%s%s%s}\n",
                        osLocation.c_str(), osCommaDatum.c_str(),
                        osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {6, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g, %.16g, %.16g%s, Hotine Oblique Mercator B}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_AZIMUTH, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0),
                osCommaDatum.c_str() ) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_STEREOGRAPHIC) ||
             EQUAL(pszProjName, SRS_PT_OBLIQUE_STEREOGRAPHIC) )
    {
        bOK &=
            VSIFPrintfL(fp, "map info = {Stereographic (ellipsoid), %s%s%s%s}\n",
                        osLocation.c_str(), osCommaDatum.c_str(),
                        osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {7, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g, %.16g, %s, Stereographic (ellipsoid)}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0),
                osCommaDatum.c_str()) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        bOK &=
            VSIFPrintfL(fp, "map info = {Albers Conical Equal Area, %s%s%s%s}\n",
                        osLocation.c_str(), osCommaDatum.c_str(),
                        osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {9, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g, %.16g, %.16g%s, Albers Conical Equal Area}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0),
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0),
                osCommaDatum.c_str()) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_POLYCONIC) )
    {
        bOK &= VSIFPrintfL(fp, "map info = {Polyconic, %s%s%s%s}\n",
                           osLocation.c_str(),
                           osCommaDatum.c_str(), osOptionalUnits.c_str(),
                           osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {10, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g%s, Polyconic}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                osCommaDatum.c_str() ) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        bOK &= VSIFPrintfL(
                   fp, "map info = {Lambert Azimuthal Equal Area, %s%s%s%s}\n",
                   osLocation.c_str(), osCommaDatum.c_str(),
                   osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {11, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g%s, Lambert Azimuthal Equal Area}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                osCommaDatum.c_str() ) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        bOK &= VSIFPrintfL(fp, "map info = {Azimuthal Equadistant, %s%s%s%s}\n",
                           osLocation.c_str(), osCommaDatum.c_str(),
                           osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {12, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g%s, Azimuthal Equadistant}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                osCommaDatum.c_str() ) >= 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        bOK &= VSIFPrintfL(fp, "map info = {Polar Stereographic, %s%s%s%s}\n",
                           osLocation.c_str(), osCommaDatum.c_str(),
                           osOptionalUnits.c_str(), osRotation.c_str()) >= 0;

        bOK &=
            VSIFPrintfL(
                fp,
                "projection info = {31, %.16g, %.16g, %.16g, %.16g, %.16g, "
                "%.16g%s, Polar Stereographic}\n",
                dfA, dfB,
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 90.0),
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0),
                osCommaDatum.c_str()) >= 0;
    }
    else
    {
        bOK &= VSIFPrintfL(fp, "map info = {%s, %s}\n",
                           pszProjName, osLocation.c_str()) >= 0;
    }

    // write out coordinate system string
    char *pszProjESRI = nullptr;
    const char* const apszOptions[] = { "FORMAT=WKT1_ESRI", nullptr };
    if ( oSRS.exportToWkt(&pszProjESRI, apszOptions) == OGRERR_NONE )
    {
        if ( strlen(pszProjESRI) )
            bOK &= VSIFPrintfL(fp, "coordinate system string = {%s}\n",
                               pszProjESRI) >= 0;
    }
    CPLFree(pszProjESRI);
    pszProjESRI = nullptr;

    if( !bOK )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Write error");
    }
}

/************************************************************************/
/*                ParseRpcCoeffsMetaDataString()                        */
/************************************************************************/

bool ENVIDataset::ParseRpcCoeffsMetaDataString(
    const char *psName, char **papszVal, int &idx)
{
    // Separate one string with 20 coefficients into an array of 20 strings.
    const char *psz20Vals = GetMetadataItem(psName, "RPC");
    if (!psz20Vals)
        return false;

    char **papszArr = CSLTokenizeString2(psz20Vals, " ", 0);
    if (!papszArr)
        return false;

    int x = 0;
    while ( (x < 20) && (papszArr[x] != nullptr) )
    {
        papszVal[idx++] = CPLStrdup(papszArr[x]);
        x++;
    }

    CSLDestroy(papszArr);

    return x == 20;
}

static char *CPLStrdupIfNotNull( const char *pszString )
{
  if (!pszString )
      return nullptr;

  return CPLStrdup(pszString);
}

/************************************************************************/
/*                          WriteRpcInfo()                              */
/************************************************************************/

// TODO: This whole function needs to be cleaned up.
bool ENVIDataset::WriteRpcInfo()
{
    // Write out 90 rpc coeffs into the envi header plus 3 envi specific rpc
    // values returns 0 if the coeffs are not present or not valid.
    int idx = 0;
    // TODO(schwehr): Make 93 a constant.
    char *papszVal[93] = { nullptr };

    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LINE_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("SAMP_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LAT_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LONG_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("HEIGHT_OFF", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LINE_SCALE", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("SAMP_SCALE", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LAT_SCALE", "RPC"));
    papszVal[idx++] = CPLStrdupIfNotNull(GetMetadataItem("LONG_SCALE", "RPC"));
    papszVal[idx++] =
        CPLStrdupIfNotNull(GetMetadataItem("HEIGHT_SCALE", "RPC"));

    bool bRet = false;

    for ( int x = 0; x < 10; x++ )  // If we do not have 10 values we return 0.
    {
        if (!papszVal[x])
            goto end;
    }

    if (!ParseRpcCoeffsMetaDataString("LINE_NUM_COEFF", papszVal, idx))
        goto end;

    if (!ParseRpcCoeffsMetaDataString("LINE_DEN_COEFF", papszVal, idx))
        goto end;

    if (!ParseRpcCoeffsMetaDataString("SAMP_NUM_COEFF", papszVal, idx))
        goto end;

    if (!ParseRpcCoeffsMetaDataString("SAMP_DEN_COEFF", papszVal, idx))
        goto end;

    papszVal[idx++] =
        CPLStrdupIfNotNull(GetMetadataItem("TILE_ROW_OFFSET", "RPC"));
    papszVal[idx++] =
        CPLStrdupIfNotNull(GetMetadataItem("TILE_COL_OFFSET", "RPC"));
    papszVal[idx++] =
        CPLStrdupIfNotNull(GetMetadataItem("ENVI_RPC_EMULATION", "RPC"));
    CPLAssert(idx == 93);
    for( int x = 90; x < 93; x++ )
    {
        if( !papszVal[x] )
            goto end;
    }

    // All the needed 93 values are present so write the rpcs into the envi
    // header.
    bRet = true;
    {
        int x = 1;
        bRet &= VSIFPrintfL(fp, "rpc info = {\n") >= 0;
        for( int iR = 0; iR < 93; iR++ )
        {
            if( papszVal[iR][0] == '-' )
                bRet &= VSIFPrintfL(fp, " %s", papszVal[iR]) >= 0;
            else
                bRet &= VSIFPrintfL(fp, "  %s", papszVal[iR]) >= 0;

            if( iR < 92 )
                bRet &= VSIFPrintfL(fp, ",") >= 0;

            if( (x % 4) == 0 )
                bRet &= VSIFPrintfL(fp, "\n") >= 0;

            x++;
            if( x > 4 )
                x = 1;
        }
    }
    bRet &= VSIFPrintfL(fp, "}\n") >= 0;

    // TODO(schwehr): Rewrite without goto.
end:
    for( int i = 0; i < idx; i++ )
        CPLFree(papszVal[i]);

    return bRet;
}

/************************************************************************/
/*                        WritePseudoGcpInfo()                          */
/************************************************************************/

bool ENVIDataset::WritePseudoGcpInfo()
{
    // Write out gcps into the envi header
    // returns 0 if the gcps are not present.

    const int iNum = std::min(GetGCPCount(), 4);
    if (iNum == 0)
        return false;

    const GDAL_GCP *pGcpStructs = GetGCPs();

    // double dfGCPPixel; /** Pixel (x) location of GCP on raster */
    // double dfGCPLine;  /** Line (y) location of GCP on raster */
    // double dfGCPX;     /** X position of GCP in georeferenced space */
    // double dfGCPY;     /** Y position of GCP in georeferenced space */

    bool bRet = VSIFPrintfL(fp, "geo points = {\n") >= 0;
    for( int iR = 0; iR < iNum; iR++ )
    {
        // Add 1 to pixel and line for ENVI convention
        bRet &= VSIFPrintfL(
            fp, " %#0.4f, %#0.4f, %#0.8f, %#0.8f",
            1 + pGcpStructs[iR].dfGCPPixel,
            1 + pGcpStructs[iR].dfGCPLine,
            pGcpStructs[iR].dfGCPY, pGcpStructs[iR].dfGCPX) >= 0;
        if( iR < iNum - 1 )
            bRet &= VSIFPrintfL(fp, ",\n") >= 0;
    }

    bRet &= VSIFPrintfL(fp, "}\n") >= 0;

    return bRet;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *ENVIDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                          SetSpatialRef()                             */
/************************************************************************/

CPLErr ENVIDataset::SetSpatialRef( const OGRSpatialReference *poSRS )

{
    m_oSRS.Clear();
    if( poSRS )
        m_oSRS = *poSRS;

    bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ENVIDataset::GetGeoTransform( double *padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

    if( bFoundMapinfo )
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ENVIDataset::SetGeoTransform( double *padfTransform )
{
    memcpy(adfGeoTransform, padfTransform, sizeof(double) * 6);

    bHeaderDirty = true;
    bFoundMapinfo = true;

    return CE_None;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void ENVIDataset::SetDescription( const char *pszDescription )
{
    bHeaderDirty = true;
    RawDataset::SetDescription(pszDescription);
}

/************************************************************************/
/*                             SetMetadata()                            */
/************************************************************************/

CPLErr ENVIDataset::SetMetadata( char **papszMetadata,
                                 const char *pszDomain )
{
    if( pszDomain && (EQUAL(pszDomain, "RPC") || EQUAL(pszDomain, "ENVI")) )
    {
        bHeaderDirty = true;
    }
    return RawDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                             SetMetadataItem()                        */
/************************************************************************/

CPLErr ENVIDataset::SetMetadataItem( const char *pszName,
                                     const char *pszValue,
                                     const char *pszDomain )
{
    if( pszDomain && (EQUAL(pszDomain, "RPC") || EQUAL(pszDomain, "ENVI")) )
    {
        bHeaderDirty = true;
    }
    return RawDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr ENVIDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const OGRSpatialReference *poSRS )
{
    bHeaderDirty = true;

    return RawDataset::SetGCPs(nGCPCount, pasGCPList, poSRS);
}

/************************************************************************/
/*                             SplitList()                              */
/*                                                                      */
/*      Split an ENVI value list into component fields, and strip       */
/*      white space.                                                    */
/************************************************************************/

// TODO: Why is this not a part of port/cpl_list.cpp?

char **ENVIDataset::SplitList( const char *pszCleanInput )

{
    char *pszInput = CPLStrdup(pszCleanInput);

    if( pszInput[0] != '{' )
    {
        CPLFree(pszInput);
        return nullptr;
    }

    int iChar = 1;
    CPLStringList aosList;
    while( pszInput[iChar] != '}' && pszInput[iChar] != '\0' )
    {
        // Find start of token.
        int iFStart = iChar;
        while( pszInput[iFStart] == ' ' )
            iFStart++;

        int iFEnd = iFStart;
        while( pszInput[iFEnd] != ',' &&
               pszInput[iFEnd] != '}' &&
               pszInput[iFEnd] != '\0' )
            iFEnd++;

        if( pszInput[iFEnd] == '\0' )
            break;

        iChar = iFEnd + 1;
        iFEnd = iFEnd - 1;

        while( iFEnd > iFStart && pszInput[iFEnd] == ' ' )
            iFEnd--;

        pszInput[iFEnd + 1] = '\0';
        aosList.AddString(pszInput + iFStart);
    }

    CPLFree(pszInput);

    return aosList.StealList();
}

/************************************************************************/
/*                            SetENVIDatum()                            */
/************************************************************************/

void ENVIDataset::SetENVIDatum( OGRSpatialReference *poSRS,
                                const char *pszENVIDatumName )

{
    // Datums.
    if( EQUAL(pszENVIDatumName, "WGS-84") )
        poSRS->SetWellKnownGeogCS("WGS84");
    else if( EQUAL(pszENVIDatumName, "WGS-72") )
        poSRS->SetWellKnownGeogCS("WGS72");
    else if( EQUAL(pszENVIDatumName, "North America 1983") )
        poSRS->SetWellKnownGeogCS("NAD83");
    else if( EQUAL(pszENVIDatumName, "North America 1927") ||
             strstr(pszENVIDatumName, "NAD27") ||
             strstr(pszENVIDatumName, "NAD-27") )
        poSRS->SetWellKnownGeogCS("NAD27");
    else if( STARTS_WITH_CI(pszENVIDatumName, "European 1950") )
        poSRS->SetWellKnownGeogCS("EPSG:4230");
    else if( EQUAL(pszENVIDatumName, "Ordnance Survey of Great Britain '36") )
        poSRS->SetWellKnownGeogCS("EPSG:4277");
    else if( EQUAL(pszENVIDatumName, "SAD-69/Brazil") )
        poSRS->SetWellKnownGeogCS("EPSG:4291");
    else if( EQUAL(pszENVIDatumName, "Geocentric Datum of Australia 1994") )
        poSRS->SetWellKnownGeogCS("EPSG:4283");
    else if( EQUAL(pszENVIDatumName, "Australian Geodetic 1984") )
        poSRS->SetWellKnownGeogCS("EPSG:4203");
    else if( EQUAL(pszENVIDatumName, "Nouvelle Triangulation Francaise IGN") )
        poSRS->SetWellKnownGeogCS("EPSG:4275");

    // Ellipsoids
    else if( EQUAL(pszENVIDatumName, "GRS 80") )
        poSRS->SetWellKnownGeogCS("NAD83");
    else if( EQUAL(pszENVIDatumName, "Airy") )
        poSRS->SetWellKnownGeogCS("EPSG:4001");
    else if( EQUAL(pszENVIDatumName, "Australian National") )
        poSRS->SetWellKnownGeogCS("EPSG:4003");
    else if( EQUAL(pszENVIDatumName, "Bessel 1841") )
        poSRS->SetWellKnownGeogCS("EPSG:4004");
    else if( EQUAL(pszENVIDatumName, "Clark 1866") )
        poSRS->SetWellKnownGeogCS("EPSG:4008");
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unrecognized datum '%s', defaulting to WGS84.",
                 pszENVIDatumName);
        poSRS->SetWellKnownGeogCS("WGS84");
    }
}

/************************************************************************/
/*                           SetENVIEllipse()                           */
/************************************************************************/

void ENVIDataset::SetENVIEllipse( OGRSpatialReference *poSRS,
                                  char **papszPI_EI )

{
    const double dfA = CPLAtofM(papszPI_EI[0]);
    const double dfB = CPLAtofM(papszPI_EI[1]);

    double dfInvF = 0.0;
    if( fabs(dfA - dfB) >= 0.1 )
        dfInvF = dfA / (dfA - dfB);

    poSRS->SetGeogCS("Ellipse Based", "Ellipse Based", "Unnamed", dfA, dfInvF);
}

/************************************************************************/
/*                           ProcessMapinfo()                           */
/*                                                                      */
/*      Extract projection, and geotransform from a mapinfo value in    */
/*      the header.                                                     */
/************************************************************************/

bool ENVIDataset::ProcessMapinfo( const char *pszMapinfo )

{
    char **papszFields = SplitList(pszMapinfo);
    const char *pszUnits = nullptr;
    double dfRotation = 0.0;
    bool bUpsideDown = false;
    const int nCount = CSLCount(papszFields);

    if( nCount < 7 )
    {
        CSLDestroy(papszFields);
        return false;
    }

    // Retrieve named values
    for (int i = 0; i < nCount; ++i)
    {
        if ( STARTS_WITH(papszFields[i], "units=") )
        {
            pszUnits = papszFields[i] + strlen("units=");
        }
        else if ( STARTS_WITH(papszFields[i], "rotation=") )
        {
            dfRotation =
                CPLAtof(papszFields[i] + strlen("rotation="));
            bUpsideDown = fabs(dfRotation) == 180.0;
            dfRotation *= kdfDegToRad * -1.0;
        }
    }

    // Check if we have coordinate system string, and if so parse it.
    char **papszCSS = nullptr;
    const char * pszCSS = m_aosHeader["coordinate_system_string"];
    if( pszCSS != nullptr )
    {
        papszCSS = CSLTokenizeString2(pszCSS,
            "{}", CSLT_PRESERVEQUOTES);
    }

    // Check if we have projection info, and if so parse it.
    char **papszPI = nullptr;
    int nPICount = 0;
    const char * pszPI = m_aosHeader["projection_info"];
    if( pszPI != nullptr )
    {
        papszPI = SplitList(pszPI);
        nPICount = CSLCount(papszPI);
    }

    // Capture geotransform.
    const double xReference = CPLAtof(papszFields[1]);
    const double yReference = CPLAtof(papszFields[2]);
    const double pixelEasting = CPLAtof(papszFields[3]);
    const double pixelNorthing = CPLAtof(papszFields[4]);
    const double xPixelSize = CPLAtof(papszFields[5]);
    const double yPixelSize = CPLAtof(papszFields[6]);

    adfGeoTransform[0] = pixelEasting - (xReference - 1) * xPixelSize;
    adfGeoTransform[1] = cos(dfRotation) * xPixelSize;
    adfGeoTransform[2] = -sin(dfRotation) * xPixelSize;
    adfGeoTransform[3] = pixelNorthing + (yReference - 1) * yPixelSize;
    adfGeoTransform[4] = -sin(dfRotation) * yPixelSize;
    adfGeoTransform[5] = -cos(dfRotation) * yPixelSize;
    if( bUpsideDown ) // to avoid numeric approximations
    {
        adfGeoTransform[1] = xPixelSize;
        adfGeoTransform[2] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = yPixelSize;
    }

    // TODO(schwehr): Symbolic constants for the fields.
    // Capture projection.
    OGRSpatialReference oSRS;
    bool bGeogCRSSet = false;
    if ( oSRS.importFromESRI(papszCSS) != OGRERR_NONE )
    {
        oSRS.Clear();

        if( STARTS_WITH_CI(papszFields[0], "UTM") && nCount >= 9 )
        {
            oSRS.SetUTM(atoi(papszFields[7]), !EQUAL(papszFields[8], "South"));
            if( nCount >= 10 && strstr(papszFields[9], "=") == nullptr )
                SetENVIDatum(&oSRS, papszFields[9]);
            else
                oSRS.SetWellKnownGeogCS("NAD27");
            bGeogCRSSet = true;
        }
        else if( STARTS_WITH_CI(papszFields[0], "State Plane (NAD 27)") &&
                 nCount > 7 )
        {
            oSRS.SetStatePlane(ITTVISToUSGSZone(atoi(papszFields[7])), FALSE);
            bGeogCRSSet = true;
        }
        else if( STARTS_WITH_CI(papszFields[0], "State Plane (NAD 83)") &&
                 nCount > 7 )
        {
            oSRS.SetStatePlane(ITTVISToUSGSZone(atoi(papszFields[7])), TRUE);
            bGeogCRSSet = true;
        }
        else if( STARTS_WITH_CI(papszFields[0], "Geographic Lat") &&
                 nCount > 7 )
        {
            if( strstr(papszFields[7], "=") == nullptr )
                SetENVIDatum(&oSRS, papszFields[7]);
            else
                oSRS.SetWellKnownGeogCS("WGS84");
            bGeogCRSSet = true;
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 3 )  // TM
        {
            oSRS.SetTM(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                       CPLAtofM(papszPI[7]),
                       CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]));
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 4 )
        {
            // Lambert Conformal Conic
            oSRS.SetLCC(CPLAtofM(papszPI[7]), CPLAtofM(papszPI[8]),
                        CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                        CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]));
        }
        else if( nPICount > 10 && atoi(papszPI[0]) == 5 )
        {
            // Oblique Merc (2 point).
            oSRS.SetHOM2PNO(CPLAtofM(papszPI[3]),
                            CPLAtofM(papszPI[4]), CPLAtofM(papszPI[5]),
                            CPLAtofM(papszPI[6]), CPLAtofM(papszPI[7]),
                            CPLAtofM(papszPI[10]),
                            CPLAtofM(papszPI[8]), CPLAtofM(papszPI[9]));
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 6 )  // Oblique Merc
        {
            oSRS.SetHOM(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                        CPLAtofM(papszPI[5]), 0.0,
                        CPLAtofM(papszPI[8]),
                        CPLAtofM(papszPI[6]), CPLAtofM(papszPI[7]) );
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 7 ) // Stereographic
        {
            oSRS.SetStereographic(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                                  CPLAtofM(papszPI[7]),
                                  CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]));
        }
        else if( nPICount > 8 && atoi(papszPI[0]) == 9 )  // Albers Equal Area
        {
            oSRS.SetACEA(CPLAtofM(papszPI[7]), CPLAtofM(papszPI[8]),
                         CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                         CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]));
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 10 )  // Polyconic
        {
            oSRS.SetPolyconic(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                              CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 11 )  // LAEA
        {
            oSRS.SetLAEA(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                         CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 12 )  // Azimuthal Equid.
        {
            oSRS.SetAE(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                       CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
        else if( nPICount > 6 && atoi(papszPI[0]) == 31 )  // Polar Stereographic
        {
            oSRS.SetPS(CPLAtofM(papszPI[3]), CPLAtofM(papszPI[4]),
                       1.0,
                       CPLAtofM(papszPI[5]), CPLAtofM(papszPI[6]) );
        }
    }
    else
    {
        bGeogCRSSet = CPL_TO_BOOL(oSRS.IsProjected());
    }

    CSLDestroy(papszCSS);

    // Still lots more that could be added for someone with the patience.

    // Fallback to localcs if we don't recognise things.
    if( oSRS.IsEmpty() )
        oSRS.SetLocalCS(papszFields[0]);

    // Try to set datum from projection info line if we have a
    // projected coordinate system without a GEOGCS explicitly set.
    if( oSRS.IsProjected() && !bGeogCRSSet && nPICount > 3 )
    {
        // Do we have a datum on the projection info line?
        int iDatum = nPICount - 1;

        // Ignore units= items.
        if( strstr(papszPI[iDatum], "=") != nullptr )
            iDatum--;

        // Skip past the name.
        iDatum--;

        const CPLString osDatumName = papszPI[iDatum];
        if( osDatumName.find_first_of("abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ")
            != CPLString::npos )
        {
            SetENVIDatum(&oSRS, osDatumName);
        }
        else
        {
            SetENVIEllipse(&oSRS, papszPI + 1);
        }
    }

    // Try to process specialized units.
    if( pszUnits != nullptr )
    {
        // Handle linear units first.
        if( EQUAL(pszUnits, "Feet") )
            oSRS.SetLinearUnitsAndUpdateParameters(
                SRS_UL_FOOT, CPLAtof(SRS_UL_FOOT_CONV));
        else if( EQUAL(pszUnits, "Meters") )
            oSRS.SetLinearUnitsAndUpdateParameters(SRS_UL_METER, 1.0);
        else if( EQUAL(pszUnits, "Km") )
            oSRS.SetLinearUnitsAndUpdateParameters("Kilometer", 1000.0);
        else if( EQUAL(pszUnits, "Yards") )
            oSRS.SetLinearUnitsAndUpdateParameters("Yard", 0.9144);
        else if( EQUAL(pszUnits, "Miles") )
            oSRS.SetLinearUnitsAndUpdateParameters("Mile", 1609.344);
        else if( EQUAL(pszUnits, "Nautical Miles") )
            oSRS.SetLinearUnitsAndUpdateParameters(
                SRS_UL_NAUTICAL_MILE, CPLAtof(SRS_UL_NAUTICAL_MILE_CONV));

        // Only handle angular units if we know the projection is geographic.
        if (oSRS.IsGeographic())
        {
            if (EQUAL(pszUnits, "Radians") )
            {
                oSRS.SetAngularUnits(SRS_UA_RADIAN, 1.0);
            }
            else
            {
                // Degrees, minutes and seconds will all be represented
                // as degrees.
                oSRS.SetAngularUnits(
                    SRS_UA_DEGREE, CPLAtof(SRS_UA_DEGREE_CONV));

                double conversionFactor = 1.0;
                if( EQUAL(pszUnits, "Minutes") )
                    conversionFactor = 60.0;
                else if( EQUAL(pszUnits, "Seconds") )
                    conversionFactor = 3600.0;
                adfGeoTransform[0] /= conversionFactor;
                adfGeoTransform[1] /= conversionFactor;
                adfGeoTransform[2] /= conversionFactor;
                adfGeoTransform[3] /= conversionFactor;
                adfGeoTransform[4] /= conversionFactor;
                adfGeoTransform[5] /= conversionFactor;
            }
        }
    }

    m_oSRS = oSRS;
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    CSLDestroy(papszFields);
    CSLDestroy(papszPI);
    return true;
}

/************************************************************************/
/*                           ProcessRPCinfo()                           */
/*                                                                      */
/*      Extract RPC transformation coefficients if they are present     */
/*      and sets into the standard metadata fields for RPC.             */
/************************************************************************/

void ENVIDataset::ProcessRPCinfo( const char *pszRPCinfo,
                                  int numCols, int numRows)
{
    char **papszFields = SplitList(pszRPCinfo);
    const int nCount = CSLCount(papszFields);

    if( nCount < 90 )
    {
        CSLDestroy(papszFields);
        return;
    }

    char sVal[1280] = { '\0' };
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[0]));
    SetMetadataItem("LINE_OFF", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[5]));
    SetMetadataItem("LINE_SCALE", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[1]));
    SetMetadataItem("SAMP_OFF", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[6]));
    SetMetadataItem("SAMP_SCALE", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[2]));
    SetMetadataItem("LAT_OFF", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[7]));
    SetMetadataItem("LAT_SCALE", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[3]));
    SetMetadataItem("LONG_OFF", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[8]));
    SetMetadataItem("LONG_SCALE", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[4]));
    SetMetadataItem("HEIGHT_OFF", sVal, "RPC");
    CPLsnprintf(sVal, sizeof(sVal), "%.16g", CPLAtof(papszFields[9]));
    SetMetadataItem("HEIGHT_SCALE", sVal, "RPC");

    sVal[0] = '\0';
    for( int i = 0; i < 20; i++ )
        CPLsnprintf(sVal + strlen(sVal), sizeof(sVal) - strlen(sVal), "%.16g ",
                    CPLAtof(papszFields[10 + i]));
    SetMetadataItem("LINE_NUM_COEFF", sVal, "RPC");

    sVal[0] = '\0';
    for( int i = 0; i < 20; i++ )
        CPLsnprintf(sVal + strlen(sVal), sizeof(sVal) - strlen(sVal), "%.16g ",
                    CPLAtof(papszFields[30 + i]));
    SetMetadataItem("LINE_DEN_COEFF", sVal, "RPC");

    sVal[0] = '\0';
    for( int i = 0; i < 20; i++ )
        CPLsnprintf(sVal + strlen(sVal), sizeof(sVal) - strlen(sVal), "%.16g ",
                    CPLAtof(papszFields[50 + i]));
    SetMetadataItem("SAMP_NUM_COEFF", sVal, "RPC");

    sVal[0] = '\0';
    for( int i = 0; i < 20; i++ )
        CPLsnprintf(sVal + strlen(sVal), sizeof(sVal) - strlen(sVal), "%.16g ",
                    CPLAtof(papszFields[70 + i]));
    SetMetadataItem("SAMP_DEN_COEFF", sVal, "RPC");

    CPLsnprintf(sVal, sizeof(sVal), "%.16g",
                CPLAtof(papszFields[3]) - CPLAtof(papszFields[8]));
    SetMetadataItem("MIN_LONG", sVal, "RPC");

    CPLsnprintf(sVal, sizeof(sVal), "%.16g",
                CPLAtof(papszFields[3]) + CPLAtof(papszFields[8]));
    SetMetadataItem("MAX_LONG", sVal, "RPC");

    CPLsnprintf(sVal, sizeof(sVal), "%.16g",
                CPLAtof(papszFields[2]) - CPLAtof(papszFields[7]));
    SetMetadataItem("MIN_LAT", sVal, "RPC");

    CPLsnprintf(sVal, sizeof(sVal), "%.16g",
                CPLAtof(papszFields[2]) + CPLAtof(papszFields[7]));
    SetMetadataItem("MAX_LAT", sVal, "RPC");

    if (nCount == 93)
    {
        SetMetadataItem("TILE_ROW_OFFSET", papszFields[90], "RPC");
        SetMetadataItem("TILE_COL_OFFSET", papszFields[91], "RPC");
        SetMetadataItem("ENVI_RPC_EMULATION", papszFields[92], "RPC");
    }

    // Handle the chipping case where the image is a subset.
    const double rowOffset = (nCount == 93) ? CPLAtof(papszFields[90]) : 0;
    const double colOffset = (nCount == 93) ? CPLAtof(papszFields[91]) : 0;
    if (rowOffset != 0.0 || colOffset != 0.0)
    {
        SetMetadataItem("ICHIP_SCALE_FACTOR", "1");
        SetMetadataItem("ICHIP_ANAMORPH_CORR", "0");
        SetMetadataItem("ICHIP_SCANBLK_NUM", "0");

        SetMetadataItem("ICHIP_OP_ROW_11", "0.5");
        SetMetadataItem("ICHIP_OP_COL_11", "0.5");
        SetMetadataItem("ICHIP_OP_ROW_12", "0.5");
        SetMetadataItem("ICHIP_OP_COL_21", "0.5");
        CPLsnprintf(sVal, sizeof(sVal), "%.16g", numCols - 0.5);
        SetMetadataItem("ICHIP_OP_COL_12", sVal);
        SetMetadataItem("ICHIP_OP_COL_22", sVal);
        CPLsnprintf(sVal, sizeof(sVal), "%.16g", numRows - 0.5);
        SetMetadataItem("ICHIP_OP_ROW_21", sVal);
        SetMetadataItem("ICHIP_OP_ROW_22", sVal);

        CPLsnprintf(sVal, sizeof(sVal), "%.16g", rowOffset + 0.5);
        SetMetadataItem("ICHIP_FI_ROW_11", sVal);
        SetMetadataItem("ICHIP_FI_ROW_12", sVal);
        CPLsnprintf(sVal, sizeof(sVal), "%.16g", colOffset + 0.5);
        SetMetadataItem("ICHIP_FI_COL_11", sVal);
        SetMetadataItem("ICHIP_FI_COL_21", sVal);
        CPLsnprintf(sVal, sizeof(sVal), "%.16g", colOffset + numCols - 0.5);
        SetMetadataItem("ICHIP_FI_COL_12", sVal);
        SetMetadataItem("ICHIP_FI_COL_22", sVal);
        CPLsnprintf(sVal, sizeof(sVal), "%.16g", rowOffset + numRows - 0.5);
        SetMetadataItem("ICHIP_FI_ROW_21", sVal);
        SetMetadataItem("ICHIP_FI_ROW_22", sVal);
    }
    CSLDestroy(papszFields);
}

/************************************************************************/
/*                             GetGCPCount()                            */
/************************************************************************/

int ENVIDataset::GetGCPCount()
{
    int nGCPCount = RawDataset::GetGCPCount();
    if( nGCPCount )
        return nGCPCount;
    return static_cast<int>(m_asGCPs.size());
}

/************************************************************************/
/*                              GetGCPs()                               */
/************************************************************************/

const GDAL_GCP *ENVIDataset::GetGCPs()
{
    int nGCPCount = RawDataset::GetGCPCount();
    if( nGCPCount )
        return RawDataset::GetGCPs();
    if( !m_asGCPs.empty() )
        return m_asGCPs.data();
    return nullptr;
}

/************************************************************************/
/*                         ProcessGeoPoints()                           */
/*                                                                      */
/*      Extract GCPs                                                    */
/************************************************************************/

void ENVIDataset::ProcessGeoPoints( const char *pszGeoPoints )
{
    char **papszFields = SplitList(pszGeoPoints);
    const int nCount = CSLCount(papszFields);

    if( (nCount % 4) != 0 )
    {
        CSLDestroy(papszFields);
        return;
    }
    m_asGCPs.resize(nCount / 4);
    if( !m_asGCPs.empty() )
    {
        GDALInitGCPs(static_cast<int>(m_asGCPs.size()), m_asGCPs.data());
    }
    for( int i = 0; i < static_cast<int>(m_asGCPs.size()); i++ )
    {
        // Subtract 1 to pixel and line for ENVI convention
        m_asGCPs[i].dfGCPPixel = CPLAtof( papszFields[i * 4 + 0] ) - 1;
        m_asGCPs[i].dfGCPLine = CPLAtof( papszFields[i * 4 + 1] ) - 1;
        m_asGCPs[i].dfGCPY = CPLAtof( papszFields[i * 4 + 2] );
        m_asGCPs[i].dfGCPX = CPLAtof( papszFields[i * 4 + 3] );
        m_asGCPs[i].dfGCPZ = 0;
    }
    CSLDestroy(papszFields);
}

static unsigned byteSwapUInt(unsigned swapMe)
{
    CPL_MSBPTR32(&swapMe);
    return swapMe;
}

void ENVIDataset::ProcessStatsFile()
{
    osStaFilename = CPLResetExtension(pszHDRFilename, "sta");
    VSILFILE *fpStaFile = VSIFOpenL(osStaFilename, "rb");

    if (!fpStaFile)
    {
        osStaFilename = "";
        return;
    }

    int lTestHeader[10] = { 0 };
    if( VSIFReadL(lTestHeader, sizeof(int), 10, fpStaFile) != 10 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpStaFile));
        osStaFilename = "";
        return;
    }

    const bool isFloat = byteSwapInt(lTestHeader[0]) == 1111838282;

    int nb = byteSwapInt(lTestHeader[3]);

    if (nb < 0 || nb > nBands)
    {
        CPLDebug("ENVI", ".sta file has statistics for %d bands, "
                         "whereas the dataset has only %d bands",
                 nb, nBands);
        nb = nBands;
    }

    // TODO(schwehr): What are 1, 4, 8, and 40?
    unsigned lOffset = 0;
    if( VSIFSeekL(fpStaFile, 40 + static_cast<vsi_l_offset>(nb + 1) * 4, SEEK_SET) == 0 &&
        VSIFReadL(&lOffset, sizeof(lOffset), 1, fpStaFile) == 1 &&
        VSIFSeekL(fpStaFile, 40 + static_cast<vsi_l_offset>(nb + 1) * 8 + byteSwapUInt(lOffset) + nb,
                  SEEK_SET) == 0)
    {
        // This should be the beginning of the statistics.
        if (isFloat)
        {
            float *fStats = static_cast<float *>(CPLCalloc( nb * 4, 4 ));
            if ( static_cast<int>(VSIFReadL(fStats, 4, nb * 4, fpStaFile)) ==
                 nb * 4)
            {
                for( int i = 0; i < nb; i++ )
                {
                    GetRasterBand(i + 1)->SetStatistics(
                        byteSwapFloat(fStats[i]),
                        byteSwapFloat(fStats[nb + i]),
                        byteSwapFloat(fStats[2 * nb + i]),
                        byteSwapFloat(fStats[3 * nb + i]));
                }
            }
            CPLFree(fStats);
        }
        else
        {
            double *dStats = static_cast<double *>(CPLCalloc(nb * 4, 8));
            if ( static_cast<int>(VSIFReadL(dStats, 8, nb * 4, fpStaFile)) ==
                 nb * 4)
            {
                for( int i = 0; i < nb; i++ )
                {
                    const double dMin = byteSwapDouble(dStats[i]);
                    const double dMax = byteSwapDouble(dStats[nb + i]);
                    const double dMean = byteSwapDouble(dStats[2 * nb + i]);
                    const double dStd = byteSwapDouble(dStats[3 * nb + i]);
                    if (dMin != dMax && dStd != 0)
                        GetRasterBand(i + 1)->
                            SetStatistics(dMin, dMax, dMean, dStd);
                }
            }
            CPLFree(dStats);
        }
    }
    CPL_IGNORE_RET_VAL(VSIFCloseL(fpStaFile));
}

int ENVIDataset::byteSwapInt(int swapMe)
{
    CPL_MSBPTR32(&swapMe);
    return swapMe;
}

float ENVIDataset::byteSwapFloat(float swapMe)
{
    CPL_MSBPTR32(&swapMe);
    return swapMe;
}

double ENVIDataset::byteSwapDouble(double swapMe)
{
    CPL_MSBPTR64(&swapMe);
    return swapMe;
}

/************************************************************************/
/*                             ReadHeader()                             */
/************************************************************************/

// Always returns true.
bool ENVIDataset::ReadHeader( VSILFILE *fpHdr )

{
    CPLReadLine2L(fpHdr, 10000, nullptr);

    // Start forming sets of name/value pairs.
    while( true )
    {
        const char *pszNewLine = CPLReadLine2L(fpHdr, 10000, nullptr);
        if( pszNewLine == nullptr )
            break;

        if( strstr(pszNewLine, "=") == nullptr )
            continue;

        CPLString osWorkingLine(pszNewLine);

        // Collect additional lines if we have open sqiggly bracket.
        if( osWorkingLine.find("{") != std::string::npos &&
            osWorkingLine.find("}") == std::string::npos )
        {
            do {
                pszNewLine = CPLReadLine2L(fpHdr, 10000, nullptr);
                if( pszNewLine )
                {
                    osWorkingLine += pszNewLine;
                }
                if( osWorkingLine.size() > 10 * 1024 * 1024 )
                    return false;
            } while( pszNewLine != nullptr && strstr(pszNewLine, "}") == nullptr );
        }

        // Try to break input into name and value portions.  Trim whitespace.
        size_t iEqual = osWorkingLine.find("=");

        if( iEqual != std::string::npos && iEqual > 0 )
        {
            CPLString osValue(osWorkingLine.substr(iEqual + 1));
            auto found = osValue.find_first_not_of(" \t");
            if( found != std::string::npos )
                osValue = osValue.substr(found);
            else
                osValue.clear();

            osWorkingLine.resize(iEqual);
            iEqual --;
            while( iEqual > 0
                   && (osWorkingLine[iEqual] == ' ' ||
                       osWorkingLine[iEqual] == '\t') )
            {
                osWorkingLine.resize(iEqual);
                iEqual --;
            }

            // Convert spaces in the name to underscores.
            for( int i = 0; osWorkingLine[i] != '\0'; i++ )
            {
                if( osWorkingLine[i] == ' ' )
                    osWorkingLine[i] = '_';
            }

            m_aosHeader.SetNameValue(osWorkingLine, osValue);
        }
    }

    return true;
}

/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool ENVIDataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout& sLayout)
{
    const bool bIsCompressed = atoi(
        m_aosHeader.FetchNameValueDef("file_compression", "0")) != 0;
    if( bIsCompressed )
        return false;
    if( !RawDataset::GetRawBinaryLayout(sLayout) )
        return false;
    sLayout.osRawFilename = GetDescription();
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ENVIDataset::Open( GDALOpenInfo *poOpenInfo )
{
    return Open(poOpenInfo, true);
}

ENVIDataset *ENVIDataset::Open( GDALOpenInfo *poOpenInfo, bool bFileSizeCheck )

{
    // Assume the caller is pointing to the binary (i.e. .bil) file.
    if( poOpenInfo->nHeaderBytes < 2 )
        return nullptr;

    // Do we have a .hdr file?  Try upper and lower case, and
    // replacing the extension as well as appending the extension
    // to whatever we currently have.

    const char *pszMode = nullptr;
    if( poOpenInfo->eAccess == GA_Update )
        pszMode = "r+";
    else
        pszMode = "r";

    CPLString osHdrFilename;
    VSILFILE *fpHeader = nullptr;
    char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if (papszSiblingFiles == nullptr)
    {
        // First try hdr as an extra extension
        osHdrFilename =
            CPLFormFilename(nullptr, poOpenInfo->pszFilename, "hdr");
        fpHeader = VSIFOpenL(osHdrFilename, pszMode);

        if( fpHeader == nullptr && VSIIsCaseSensitiveFS(osHdrFilename) )
        {
            osHdrFilename =
                CPLFormFilename(nullptr, poOpenInfo->pszFilename, "HDR");
            fpHeader = VSIFOpenL(osHdrFilename, pszMode);
        }

        // Otherwise, try .hdr as a replacement extension
        if( fpHeader == nullptr )
        {
            osHdrFilename = CPLResetExtension(poOpenInfo->pszFilename, "hdr");
            fpHeader = VSIFOpenL(osHdrFilename, pszMode);
        }

        if( fpHeader == nullptr && VSIIsCaseSensitiveFS(osHdrFilename) )
        {
            osHdrFilename = CPLResetExtension(poOpenInfo->pszFilename, "HDR");
            fpHeader = VSIFOpenL(osHdrFilename, pszMode);
        }

    }
    else
    {
        // Now we need to tear apart the filename to form a .HDR filename.
        CPLString osPath = CPLGetPath(poOpenInfo->pszFilename);
        CPLString osName = CPLGetFilename(poOpenInfo->pszFilename);

        // First try hdr as an extra extension
        int iFile = CSLFindString(papszSiblingFiles,
                                  CPLFormFilename(nullptr, osName, "hdr"));
        if( iFile < 0 )
        {
            // Otherwise, try .hdr as a replacement extension
            iFile =
                CSLFindString(papszSiblingFiles, CPLResetExtension(osName, "hdr"));
        }

        if( iFile >= 0 )
        {
            osHdrFilename =
                CPLFormFilename(osPath, papszSiblingFiles[iFile], nullptr);
            fpHeader = VSIFOpenL(osHdrFilename, pszMode);
        }
    }

    if( fpHeader == nullptr )
        return nullptr;

    // Check that the first line says "ENVI".
    char szTestHdr[4] = { '\0' };

    if( VSIFReadL(szTestHdr, 4, 1, fpHeader) != 1 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpHeader));
        return nullptr;
    }
    if( !STARTS_WITH(szTestHdr, "ENVI") )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpHeader));
        return nullptr;
    }

    // Create a corresponding GDALDataset.
    ENVIDataset *poDS = new ENVIDataset();
    poDS->pszHDRFilename = CPLStrdup(osHdrFilename);
    poDS->fp = fpHeader;

    // Read the header.
    if( !poDS->ReadHeader(fpHeader) )
    {
        delete poDS;
        return nullptr;
    }

    // Has the user selected the .hdr file to open?
    if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "hdr") )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The selected file is an ENVI header file, but to "
                 "open ENVI datasets, the data file should be selected "
                 "instead of the .hdr file.  Please try again selecting "
                 "the data file corresponding to the header file:  "
                 "%s",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // Has the user selected the .sta (stats) file to open?
    if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "sta") )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The selected file is an ENVI statistics file. "
                 "To open ENVI datasets, the data file should be selected "
                 "instead of the .sta file.  Please try again selecting "
                 "the data file corresponding to the statistics file:  "
                 "%s",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // Extract required values from the .hdr.
    int nLines = atoi(poDS->m_aosHeader.FetchNameValueDef("lines", "0"));

    int nSamples =
        atoi(poDS->m_aosHeader.FetchNameValueDef("samples", "0"));

    int nBands = atoi(poDS->m_aosHeader.FetchNameValueDef("bands", "0"));

    // In case, there is no interleave keyword, we try to derive it from the
    // file extension.
    CPLString osInterleave =
        poDS->m_aosHeader.FetchNameValueDef("interleave",
                             CPLGetExtension(poOpenInfo->pszFilename));

    if ( !STARTS_WITH_CI(osInterleave, "BSQ") &&
         !STARTS_WITH_CI(osInterleave, "BIP") &&
         !STARTS_WITH_CI(osInterleave, "BIL") )
    {
        CPLDebug("ENVI",
                 "Unset or unknown value for 'interleave' keyword --> "
                 "assuming BSQ interleaving");
        osInterleave = "bsq";
    }

    if( !GDALCheckDatasetDimensions(nSamples, nLines) ||
        !GDALCheckBandCount(nBands, FALSE) )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The file appears to have an associated ENVI header, but "
                 "one or more of the samples, lines and bands "
                 "keywords appears to be missing or invalid.");
        return nullptr;
    }

    int nHeaderSize =
        atoi(poDS->m_aosHeader.FetchNameValueDef("header_offset", "0"));

    // Translate the datatype.
    GDALDataType eType = GDT_Byte;

    const char* pszDataType = poDS->m_aosHeader["data_type"];
    if( pszDataType != nullptr )
    {
        switch( atoi(pszDataType) )
        {
          case 1:
            eType = GDT_Byte;
            break;

          case 2:
            eType = GDT_Int16;
            break;

          case 3:
            eType = GDT_Int32;
            break;

          case 4:
            eType = GDT_Float32;
            break;

          case 5:
            eType = GDT_Float64;
            break;

          case 6:
            eType = GDT_CFloat32;
            break;

          case 9:
            eType = GDT_CFloat64;
            break;

          case 12:
            eType = GDT_UInt16;
            break;

          case 13:
            eType = GDT_UInt32;
            break;

            // 14=Int64, 15=UInt64

          default:
            delete poDS;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The file does not have a value for the data_type "
                     "that is recognised by the GDAL ENVI driver.");
            return nullptr;
        }
    }

    // Translate the byte order.
    bool bNativeOrder = true;

    const char* pszByteOrder = poDS->m_aosHeader["byte_order"];
    if( pszByteOrder != nullptr )
    {
#ifdef CPL_LSB
        bNativeOrder = atoi(pszByteOrder) == 0;
#else
        bNativeOrder = atoi(pszByteOrder) != 0;
#endif
    }

    // Warn about unsupported file types virtual mosaic and meta file.
    const char *pszEnviFileType = poDS->m_aosHeader["file_type"];
    if( pszEnviFileType != nullptr )
    {
        // When the file type is one of these we return an invalid file type err
        // 'envi meta file'
        // 'envi virtual mosaic'
        // 'envi spectral library'
        // 'envi fft result'

        // When the file type is one of these we open it
        // 'envi standard'
        // 'envi classification'

        // When the file type is anything else we attempt to open it as a
        // raster.


        // envi gdal does not support any of these
        // all others we will attempt to open
        if( EQUAL(pszEnviFileType, "envi meta file") ||
            EQUAL(pszEnviFileType, "envi virtual mosaic") ||
            EQUAL(pszEnviFileType, "envi spectral library") )
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "File %s contains an invalid file type in the ENVI .hdr "
                     "GDAL does not support '%s' type files.",
                     poOpenInfo->pszFilename, pszEnviFileType);
            delete poDS;
            return nullptr;
        }
    }

    // Detect (gzipped) compressed datasets.
    const bool bIsCompressed = atoi(
        poDS->m_aosHeader.FetchNameValueDef("file_compression", "0")) != 0;

    // Capture some information from the file that is of interest.
    poDS->nRasterXSize = nSamples;
    poDS->nRasterYSize = nLines;
    poDS->eAccess = poOpenInfo->eAccess;

    // Reopen file in update mode if necessary.
    CPLString osImageFilename(poOpenInfo->pszFilename);
    if ( bIsCompressed )
        osImageFilename = "/vsigzip/" + osImageFilename;
    if( poOpenInfo->eAccess == GA_Update )
    {
        if( bIsCompressed )
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Cannot open compressed file in update mode.");
            return nullptr;
        }
        poDS->fpImage = VSIFOpenL(osImageFilename, "rb+");
    }
    else
    {
        poDS->fpImage = VSIFOpenL(osImageFilename, "rb");
    }

    if( poDS->fpImage == nullptr )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to re-open %s within ENVI driver.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // Compute the line offset.
    const int nDataSize = GDALGetDataTypeSizeBytes(eType);
    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;
    CPLAssert(nDataSize != 0);
    CPLAssert(nBands != 0);

    if( STARTS_WITH_CI(osInterleave, "bil") )
    {
        poDS->interleave = BIL;
        poDS->SetMetadataItem("INTERLEAVE", "LINE", "IMAGE_STRUCTURE");
        if (nSamples > std::numeric_limits<int>::max() / (nDataSize * nBands))
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
            return nullptr;
        }
        nLineOffset = nDataSize * nSamples * nBands;
        nPixelOffset = nDataSize;
        nBandOffset = static_cast<vsi_l_offset>(nDataSize) * nSamples;
    }
    else if( STARTS_WITH_CI(osInterleave, "bip") )
    {
        poDS->interleave = BIP;
        poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        if (nSamples > std::numeric_limits<int>::max() / (nDataSize * nBands))
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
            return nullptr;
        }
        nLineOffset = nDataSize * nSamples * nBands;
        nPixelOffset = nDataSize * nBands;
        nBandOffset = nDataSize;
    }
    else
    {
        poDS->interleave = BSQ;
        poDS->SetMetadataItem("INTERLEAVE", "BAND", "IMAGE_STRUCTURE");
        if (nSamples > std::numeric_limits<int>::max() / nDataSize)
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
            return nullptr;
        }
        nLineOffset = nDataSize * nSamples;
        nPixelOffset = nDataSize;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nLines;
    }

    const char* pszMajorFrameOffset = poDS->m_aosHeader["major_frame_offsets"];
    if (pszMajorFrameOffset != nullptr)
    {
        char **papszMajorFrameOffsets = poDS->SplitList(pszMajorFrameOffset);

        const int nTempCount = CSLCount(papszMajorFrameOffsets);
        if (nTempCount == 2)
        {
            int nOffset1 = atoi(papszMajorFrameOffsets[0]);
            int nOffset2 = atoi(papszMajorFrameOffsets[1]);
            if( nOffset1 >= 0 && nOffset2 >= 0 &&
                nHeaderSize < INT_MAX - nOffset1 &&
                nOffset1 < INT_MAX - nOffset2 &&
                nOffset1 + nOffset2 < INT_MAX - nLineOffset )
            {
                nHeaderSize += nOffset1;
                nLineOffset += nOffset1 + nOffset2;
            }
        }
        CSLDestroy(papszMajorFrameOffsets);
    }

    // Currently each ENVIRasterBand allocates nPixelOffset * nRasterXSize bytes
    // so for a pixel interleaved scheme, this will allocate lots of memory!
    // Actually this is quadratic in the number of bands!
    // Do a few sanity checks to avoid excessive memory allocation on
    // small files.
    // But ultimately we should fix RawRasterBand to have a shared buffer
    // among bands.
    if( bFileSizeCheck &&
        !RAWDatasetCheckMemoryUsage(
                        poDS->nRasterXSize, poDS->nRasterYSize, nBands,
                        nDataSize,
                        nPixelOffset, nLineOffset, nHeaderSize, nBandOffset,
                        poDS->fpImage) )
    {
        delete poDS;
        return nullptr;
    }

    // Create band information objects.
    CPLErrorReset();
    for( int i = 0; i < nBands; i++ )
    {
        poDS->SetBand(i + 1,
                      new ENVIRasterBand(poDS, i + 1, poDS->fpImage,
                                         nHeaderSize + nBandOffset * i,
                                         nPixelOffset, nLineOffset, eType,
                                         bNativeOrder));
        if( CPLGetLastErrorType() != CE_None )
        {
            delete poDS;
            return nullptr;
        }
    }

    // Apply band names if we have them.
    // Use wavelength for more descriptive information if possible.
    const char* pszBandNames = poDS->m_aosHeader["band_names"];
    const char* pszWaveLength = poDS->m_aosHeader["wavelength"];
    if( pszBandNames != nullptr || pszWaveLength != nullptr)
    {
        char **papszBandNames =
            poDS->SplitList(pszBandNames);
        char **papszWL =
            poDS->SplitList(pszWaveLength);

        const char *pszWLUnits = nullptr;
        const int nWLCount = CSLCount(papszWL);
        if (papszWL)
        {
            // If WL information is present, process wavelength units.
            pszWLUnits = poDS->m_aosHeader["wavelength_units"];
            if (pszWLUnits)
            {
                // Don't show unknown or index units.
                if( EQUAL(pszWLUnits, "Unknown") || EQUAL(pszWLUnits, "Index") )
                    pszWLUnits = nullptr;
            }
            if( pszWLUnits )
            {
                // Set wavelength units to dataset metadata.
                poDS->SetMetadataItem("wavelength_units", pszWLUnits);
            }
        }

        for( int i = 0; i < nBands; i++ )
        {
            // First set up the wavelength names and units if available.
            CPLString osWavelength;
            if (papszWL && nWLCount > i)
            {
                osWavelength = papszWL[i];
                if (pszWLUnits)
                {
                    osWavelength += " ";
                    osWavelength += pszWLUnits;
                }
            }

            // Build the final name for this band.
            CPLString osBandName;
            if (papszBandNames && CSLCount(papszBandNames) > i)
            {
                osBandName = papszBandNames[i];
                if( !osWavelength.empty() )
                {
                    osBandName += " (";
                    osBandName += osWavelength;
                    osBandName += ")";
                }
            }
            else
            {
                // WL but no band names.
                osBandName = osWavelength;
            }

            // Description is for internal GDAL usage.
            poDS->GetRasterBand(i + 1)->SetDescription(osBandName);

            // Metadata field named Band_1, etc. Needed for ArcGIS integration.
            CPLString osBandId = CPLSPrintf("Band_%i", i + 1);
            poDS->SetMetadataItem(osBandId, osBandName);

            // Set wavelength metadata to band.
            if (papszWL && nWLCount > i)
            {
                poDS->GetRasterBand(i + 1)->SetMetadataItem("wavelength",
                                                            papszWL[i]);

                if (pszWLUnits)
                {
                    poDS->GetRasterBand(i + 1)->SetMetadataItem(
                        "wavelength_units", pszWLUnits);
                }
            }
        }
        CSLDestroy(papszWL);
        CSLDestroy(papszBandNames);
    }

    // Apply class names if we have them.
    const char* pszClassNames = poDS->m_aosHeader["class_names"];
    if( pszClassNames != nullptr )
    {
        char **papszClassNames = poDS->SplitList(pszClassNames);

        poDS->GetRasterBand(1)->SetCategoryNames(papszClassNames);
        CSLDestroy(papszClassNames);
    }

    // Apply colormap if we have one.
    const char* pszClassLookup = poDS->m_aosHeader["class_lookup"];
    if( pszClassLookup != nullptr )
    {
        char **papszClassColors = poDS->SplitList(pszClassLookup);
        const int nColorValueCount = CSLCount(papszClassColors);
        GDALColorTable oCT;

        for( int i = 0; i * 3 + 2 < nColorValueCount; i++ )
        {
            GDALColorEntry sEntry;

            sEntry.c1 = static_cast<short>(atoi(papszClassColors[i * 3 + 0]));
            sEntry.c2 = static_cast<short>(atoi(papszClassColors[i * 3 + 1]));
            sEntry.c3 = static_cast<short>(atoi(papszClassColors[i * 3 + 2]));
            sEntry.c4 = 255;
            oCT.SetColorEntry(i, &sEntry);
        }

        CSLDestroy(papszClassColors);

        poDS->GetRasterBand(1)->SetColorTable(&oCT);
        poDS->GetRasterBand(1)->SetColorInterpretation(GCI_PaletteIndex);
    }

    // Set the nodata value if it is present.
    const char* pszDataIgnoreValue = poDS->m_aosHeader["data_ignore_value"];
    if( pszDataIgnoreValue != nullptr )
    {
        for( int i = 0; i < poDS->nBands; i++ )
            reinterpret_cast<RawRasterBand *>(poDS->GetRasterBand(i + 1))
                ->SetNoDataValue(CPLAtof(pszDataIgnoreValue));
    }

    // Set all the header metadata into the ENVI domain.
    {
        char **pTmp = poDS->m_aosHeader.List();
        while( *pTmp != nullptr )
        {
            char **pTokens = CSLTokenizeString2(
                *pTmp, "=", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
            if (pTokens[0] != nullptr && pTokens[1] != nullptr && pTokens[2] == nullptr)
            {
                poDS->SetMetadataItem(pTokens[0], pTokens[1], "ENVI");
            }
            CSLDestroy(pTokens);
            pTmp++;
        }
    }

    // Read the stats file if it is present.
    poDS->ProcessStatsFile();

    // Look for mapinfo.
    const char* pszMapInfo = poDS->m_aosHeader["map_info"];
    if( pszMapInfo != nullptr )
    {
        poDS->bFoundMapinfo = CPL_TO_BOOL(poDS->ProcessMapinfo(
            pszMapInfo));
    }

    // Look for RPC.
    const char* pszRPCInfo = poDS->m_aosHeader["rpc_info"];
    if( !poDS->bFoundMapinfo && pszRPCInfo != nullptr )
    {
        poDS->ProcessRPCinfo(pszRPCInfo,
                             poDS->nRasterXSize, poDS->nRasterYSize);
    }

    // Look for geo_points / GCP
    const char* pszGeoPoints = poDS->m_aosHeader["geo_points"];
    if( !poDS->bFoundMapinfo && pszGeoPoints != nullptr )
    {
        poDS->ProcessGeoPoints(pszGeoPoints);
    }

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Check for overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    // SetMetadata() calls in Open() makes the header dirty.
    // Don't re-write the header if nothing external has changed the metadata.
    poDS->bHeaderDirty = false;

    return poDS;
}

int ENVIDataset::GetEnviType(GDALDataType eType)
{
    int iENVIType = 1;
    switch(eType)
    {
    case GDT_Byte:
        iENVIType = 1;
        break;
    case GDT_Int16:
        iENVIType = 2;
        break;
    case GDT_Int32:
        iENVIType = 3;
        break;
    case GDT_Float32:
        iENVIType = 4;
        break;
    case GDT_Float64:
        iENVIType = 5;
        break;
    case GDT_CFloat32:
        iENVIType = 6;
        break;
    case GDT_CFloat64:
        iENVIType = 9;
        break;
    case GDT_UInt16:
        iENVIType = 12;
        break;
    case GDT_UInt32:
        iENVIType = 13;
        break;

    // 14=Int64, 15=UInt64

    default:
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create ENVI .hdr labelled dataset with an "
                 "illegal data type (%s).",
                 GDALGetDataTypeName(eType));
        return 1;
    }
    return iENVIType;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *ENVIDataset::Create( const char *pszFilename,
                                  int nXSize, int nYSize, int nBandsIn,
                                  GDALDataType eType,
                                  char **papszOptions )

{
    // Verify input options.
    int iENVIType = GetEnviType(eType);
    if( 0 == iENVIType )
        return nullptr;

    // Try to create the file.
    VSILFILE *fp = VSIFOpenL(pszFilename, "wb");

    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create file `%s' failed.", pszFilename);
        return nullptr;
    }

    // Just write out a couple of bytes to establish the binary
    // file, and then close it.
    {
        const bool bRet = VSIFWriteL(
            static_cast<void *>(const_cast<char *>("\0\0")), 2, 1, fp) == 1;
        if( VSIFCloseL(fp) != 0 || !bRet )
            return nullptr;
    }

    // Create the .hdr filename.
    const char *pszHDRFilename = nullptr;
    const char *pszSuffix = CSLFetchNameValue(papszOptions, "SUFFIX");
    if ( pszSuffix && STARTS_WITH_CI(pszSuffix, "ADD"))
        pszHDRFilename = CPLFormFilename(nullptr, pszFilename, "hdr");
    else
        pszHDRFilename = CPLResetExtension(pszFilename, "hdr");

    // Open the file.
    fp = VSIFOpenL(pszHDRFilename, "wt");
    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create file `%s' failed.",
                 pszHDRFilename);
        return nullptr;
    }

    // Write out the header.
#ifdef CPL_LSB
    const int iBigEndian = 0;
#else
    const int iBigEndian = 1;
#endif

    bool bRet = VSIFPrintfL(fp, "ENVI\n") > 0;
    bRet &= VSIFPrintfL(fp, "samples = %d\nlines   = %d\nbands   = %d\n",
                        nXSize, nYSize, nBandsIn) > 0;
    bRet &=
        VSIFPrintfL(fp, "header offset = 0\nfile type = ENVI Standard\n") > 0;
    bRet &= VSIFPrintfL(fp, "data type = %d\n", iENVIType) > 0;
    const char *pszInterleaving = CSLFetchNameValue(papszOptions, "INTERLEAVE");
    if ( pszInterleaving )
    {
        if( STARTS_WITH_CI(pszInterleaving, "bip") )
            pszInterleaving = "bip";  // interleaved by pixel
        else if( STARTS_WITH_CI(pszInterleaving, "bil") )
            pszInterleaving = "bil";  // interleaved by line
        else
            pszInterleaving = "bsq";  // band sequential by default
    }
    else
    {
        pszInterleaving = "bsq";
    }
    bRet &= VSIFPrintfL(fp, "interleave = %s\n", pszInterleaving) > 0;
    bRet &= VSIFPrintfL(fp, "byte order = %d\n", iBigEndian) > 0;

    if( VSIFCloseL(fp) != 0 || !bRet )
        return nullptr;

    GDALOpenInfo oOpenInfo(pszFilename, GA_Update);
    ENVIDataset *poDS = Open(&oOpenInfo, false);
    if( poDS )
    {
        poDS->SetFillFile();
    }
    return poDS;
}

/************************************************************************/
/*                           ENVIRasterBand()                           */
/************************************************************************/

ENVIRasterBand::ENVIRasterBand( GDALDataset *poDSIn, int nBandIn,
                                VSILFILE *fpRawIn,
                                vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                                int nLineOffsetIn,
                                GDALDataType eDataTypeIn, int bNativeOrderIn) :
    RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                  nLineOffsetIn, eDataTypeIn, bNativeOrderIn, RawRasterBand::OwnFP::NO)
{}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void ENVIRasterBand::SetDescription( const char *pszDescription )
{
    reinterpret_cast<ENVIDataset *>(poDS)->bHeaderDirty = true;
    RawRasterBand::SetDescription(pszDescription);
}

/************************************************************************/
/*                           SetCategoryNames()                         */
/************************************************************************/

CPLErr ENVIRasterBand::SetCategoryNames( char **papszCategoryNamesIn )
{
    reinterpret_cast<ENVIDataset *>(poDS)->bHeaderDirty = true;
    return RawRasterBand::SetCategoryNames(papszCategoryNamesIn);
}

/************************************************************************/
/*                            SetNoDataValue()                          */
/************************************************************************/

CPLErr ENVIRasterBand::SetNoDataValue( double dfNoDataValue )
{
    reinterpret_cast<ENVIDataset *>(poDS)->bHeaderDirty = true;
    return RawRasterBand::SetNoDataValue(dfNoDataValue);
}

/************************************************************************/
/*                         GDALRegister_ENVI()                          */
/************************************************************************/

void GDALRegister_ENVI()
{
    if( GDALGetDriverByName("ENVI") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("ENVI");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ENVI .hdr Labelled");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/envi.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 "
                              "Float32 Float64 CFloat32 CFloat64");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='SUFFIX' type='string-select'>"
        "       <Value>ADD</Value>"
        "   </Option>"
        "   <Option name='INTERLEAVE' type='string-select'>"
        "       <Value>BIP</Value>"
        "       <Value>BIL</Value>"
        "       <Value>BSQ</Value>"
        "   </Option>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->pfnOpen = ENVIDataset::Open;
    poDriver->pfnCreate = ENVIDataset::Create;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
