/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements RasterLite2 support class.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 *
 * CREDITS: The RasterLite2 module has been completely funded by:
 * Regione Toscana - Settore Sistema Informativo Territoriale ed
 * Ambientale (GDAL/RasterLite2 driver)
 * CIG: 644544015A
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
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
#include "ogr_sqlite.h"
#include "ogrsqliteutility.h"
#include "rasterlite2_header.h"

#include <cstring>
#include <algorithm>

#include "cpl_error.h"
#include "cpl_string.h"

#ifdef HAVE_RASTERLITE2

static CPLString EscapeNameAndQuoteIfNeeded(const char* pszName)
{
    if( strchr(pszName, '"') == nullptr && strchr(pszName, ':') == nullptr )
        return pszName;
    return '"' + SQLEscapeName(pszName) + '"';
}

#endif

/************************************************************************/
/*                            OpenRaster()                              */
/************************************************************************/

bool OGRSQLiteDataSource::OpenRaster()
{
#ifdef HAVE_RASTERLITE2
/* -------------------------------------------------------------------- */
/*      Detect RasterLite2 coverages.                                   */
/* -------------------------------------------------------------------- */
    char** papszResults = nullptr;
    int nRowCount = 0, nColCount = 0;
    int rc = sqlite3_get_table( hDB,
                           "SELECT name FROM sqlite_master WHERE "
                           "type = 'table' AND name = 'raster_coverages'",
                           &papszResults, &nRowCount,
                           &nColCount, nullptr );
    sqlite3_free_table(papszResults);
    if( !(rc == SQLITE_OK && nRowCount == 1) )
    {
        return false;
    }

    papszResults = nullptr;
    nRowCount = 0;
    nColCount = 0;
    rc = sqlite3_get_table( hDB,
                           "SELECT coverage_name, title, abstract "
                           "FROM raster_coverages "
                           "LIMIT 10000",
                           &papszResults, &nRowCount,
                           &nColCount, nullptr );
    if( !(rc == SQLITE_OK && nRowCount > 0) )
    {
        sqlite3_free_table(papszResults);
        return false;
    }
    for(int i=0;i<nRowCount;++i)
    {
        const char * const* papszRow = papszResults + i * 3 + 3;
        const char* pszCoverageName = papszRow[0];
        const char* pszTitle = papszRow[1];
        const char* pszAbstract = papszRow[2];
        if( pszCoverageName != nullptr )
        {
            rl2CoveragePtr cvg = rl2_create_coverage_from_dbms( 
                                                            hDB,
                                                            nullptr,
                                                            pszCoverageName );
            if( cvg != nullptr )
            {
                const int nIdx = m_aosSubDatasets.size() / 2 + 1;
                m_aosSubDatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", nIdx),
                    CPLSPrintf("RASTERLITE2:%s:%s",
                        EscapeNameAndQuoteIfNeeded(m_pszFilename).c_str(),
                        EscapeNameAndQuoteIfNeeded(pszCoverageName).c_str()));
                CPLString osDesc("Coverage ");
                osDesc += pszCoverageName;
                if( pszTitle != nullptr && pszTitle[0] != '\0' &&
                    !EQUAL(pszTitle, "*** missing Title ***") )
                {
                    osDesc += ", title = ";
                    osDesc += pszTitle;
                }
                if( pszAbstract != nullptr && pszAbstract[0] != '\0' &&
                    !EQUAL(pszAbstract, "*** missing Abstract ***") )
                {
                    osDesc += ", abstract = ";
                    osDesc += pszAbstract;
                }
                m_aosSubDatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_DESC", nIdx), osDesc.c_str());

                rl2_destroy_coverage(cvg);
            }
        }
    }
    sqlite3_free_table(papszResults);

    if( m_aosSubDatasets.size() == 2 )
    {
        const char* pszSubDSName = m_aosSubDatasets.FetchNameValue( "SUBDATASET_1_NAME" );
        if( pszSubDSName )
        {
            return OpenRasterSubDataset(pszSubDSName);
        }
    }

    return !m_aosSubDatasets.empty();
#else
    return false;
#endif
}

/************************************************************************/
/*                        OpenRasterSubDataset()                        */
/************************************************************************/

bool OGRSQLiteDataSource::OpenRasterSubDataset(CPL_UNUSED
                                               const char* pszConnectionId)
{
#ifdef HAVE_RASTERLITE2
    if( !STARTS_WITH_CI( pszConnectionId, "RASTERLITE2:" ) )
        return false;

    char** papszTokens =
        CSLTokenizeString2( pszConnectionId, ":", CSLT_HONOURSTRINGS );
    if( CSLCount(papszTokens) < 3 )
    {
        CSLDestroy(papszTokens);
        return false;
    }

    m_aosSubDatasets.Clear();

    m_osCoverageName = SQLUnescape( papszTokens[2] );
    m_nSectionId =
        (CSLCount(papszTokens) >= 4) ? CPLAtoGIntBig( papszTokens[3] ) : -1;

    CSLDestroy(papszTokens);

    m_pRL2Coverage = rl2_create_coverage_from_dbms( hDB,
                                                    nullptr,
                                                    m_osCoverageName );
    if( m_pRL2Coverage == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid coverage: %s", m_osCoverageName.c_str() );
        return false;
    }

    bool bSingleSection = false;
    if( m_nSectionId < 0 )
    {
        CPLString osSectionTableName( CPLSPrintf("%s_sections",
                                                 m_osCoverageName.c_str()) );
        int nRowCount2 = 0;
        int nColCount2 = 0;
        char** papszResults2 = nullptr;
        char* pszSQL = sqlite3_mprintf(
                "SELECT section_id, section_name FROM \"%w\" "
                "ORDER BY section_id "
                "LIMIT 1000000",
                osSectionTableName.c_str());
        int rc = sqlite3_get_table( hDB,
                pszSQL,
                &papszResults2, &nRowCount2,
                &nColCount2, nullptr );
        sqlite3_free(pszSQL);
        if( rc == SQLITE_OK )
        {
            for( int j=0; j<nRowCount2; ++j )
            {
                const char * const* papszRow2 = papszResults2 + j * 2 + 2;
                const char* pszSectionId = papszRow2[0];
                const char* pszSectionName = papszRow2[1];
                if( pszSectionName != nullptr && pszSectionId != nullptr )
                {
                    if( nRowCount2 > 1 )
                    {
                        const int nIdx = m_aosSubDatasets.size() / 2 + 1;
                        m_aosSubDatasets.AddNameValue(
                          CPLSPrintf("SUBDATASET_%d_NAME", nIdx),
                          CPLSPrintf("RASTERLITE2:%s:%s:%s:%s",
                            EscapeNameAndQuoteIfNeeded(m_pszFilename).c_str(),
                            EscapeNameAndQuoteIfNeeded(m_osCoverageName).
                                                                      c_str(),
                            pszSectionId,
                            EscapeNameAndQuoteIfNeeded(pszSectionName).
                                                                     c_str()));
                        m_aosSubDatasets.AddNameValue(
                            CPLSPrintf("SUBDATASET_%d_DESC", nIdx),
                            CPLSPrintf("Coverage %s, section %s / %s",
                                    m_osCoverageName.c_str(),
                                    pszSectionName,
                                    pszSectionId));
                    }
                    else
                    {
                        m_nSectionId = CPLAtoGIntBig( pszSectionId );
                        bSingleSection = true;
                    }
                }
            }
        }
        sqlite3_free_table(papszResults2);
    }

    double dfXRes = 0.0;
    double dfYRes = 0.0;

    double dfMinX = 0.0;
    double dfMinY = 0.0;
    double dfMaxX = 0.0;
    double dfMaxY = 0.0;
    unsigned int nWidth = 0;
    unsigned int nHeight = 0;

    // Get extent and resolution
    if( m_nSectionId >= 0 )
    {
        int ret = rl2_resolve_base_resolution_from_dbms(hDB,
                                                        nullptr,
                                                        m_osCoverageName,
                                                        TRUE, // by_section
                                                        m_nSectionId,
                                                        &dfXRes,
                                                        &dfYRes );
        if( ret != RL2_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "rl2_resolve_base_resolution_from_dbms() failed / "
                    "Invalid section: " CPL_FRMT_GIB, m_nSectionId );
            return false;
        }


        ret = rl2_resolve_full_section_from_dbms( hDB,
                                                  nullptr,
                                                  m_osCoverageName,
                                                  m_nSectionId,
                                                  dfXRes, dfYRes,
                                                  &dfMinX, &dfMinY,
                                                  &dfMaxX, &dfMaxY,
                                                  &nWidth, &nHeight );
        if( ret != RL2_OK || nWidth == 0 || nWidth > INT_MAX ||
            nHeight == 0 || nHeight > INT_MAX )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "rl2_resolve_full_section_from_dbms() failed / "
                    "Invalid section: " CPL_FRMT_GIB, m_nSectionId );
            return false;
        }
    }
    else
    {
        rl2_get_coverage_resolution (m_pRL2Coverage, &dfXRes, &dfYRes);

        char* pszSQL = sqlite3_mprintf(
            "SELECT extent_minx, extent_miny, extent_maxx, extent_maxy "
            "FROM raster_coverages WHERE "
            "Lower(coverage_name) = Lower('%q') "
            "LIMIT 1", m_osCoverageName.c_str() );
        char** papszResults = nullptr;
        int nRowCount = 0;
        int nColCount = 0;
        int rc = sqlite3_get_table( hDB, pszSQL,
                                    &papszResults, &nRowCount,
                                    &nColCount, nullptr );
        sqlite3_free( pszSQL );
        if( rc == SQLITE_OK )
        {
            if( nRowCount ==  1 )
            {
                const char* pszMinX = papszResults[4 + 0];
                const char* pszMinY = papszResults[4 + 1];
                const char* pszMaxX = papszResults[4 + 2];
                const char* pszMaxY = papszResults[4 + 3];
                if( pszMinX != nullptr && pszMinY != nullptr && pszMaxX != nullptr &&
                    pszMaxY != nullptr )
                {
                    dfMinX = CPLAtof(pszMinX);
                    dfMinY = CPLAtof(pszMinY);
                    dfMaxX = CPLAtof(pszMaxX);
                    dfMaxY = CPLAtof(pszMaxY);
                }
            }
            sqlite3_free_table(papszResults);
        }
        double dfWidth = 0.5 + (dfMaxX - dfMinX) / dfXRes;
        double dfHeight = 0.5 + (dfMaxY - dfMinY) / dfYRes;
        if( dfWidth <= 0.5 || dfHeight <= 0.5 || dfWidth > INT_MAX ||
            dfHeight > INT_MAX )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid dimensions");
            return false;
        }
        nWidth = static_cast<int>(dfWidth);
        nHeight = static_cast<int>(dfHeight);
    }

    // Compute dimension and geotransform
    nRasterXSize = static_cast<int>(nWidth);
    nRasterYSize = static_cast<int>(nHeight);
    m_bGeoTransformValid = true;
    m_adfGeoTransform[0] = dfMinX;
    m_adfGeoTransform[1] = (dfMaxX - dfMinX) / nRasterXSize;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = dfMaxY;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = -(dfMaxY - dfMinY) / nRasterYSize;

    // Get SRS
    int nSRID = 0;
    if( rl2_get_coverage_srid(m_pRL2Coverage, &nSRID) == RL2_OK )
    {
        OGRSpatialReference* poSRS = FetchSRS( nSRID );
        if( poSRS != nullptr )
        {
            OGRSpatialReference oSRS(*poSRS);
            char* pszWKT = nullptr;
            if( oSRS.EPSGTreatsAsLatLong() ||
                oSRS.EPSGTreatsAsNorthingEasting() )
            {
                oSRS.GetRoot()->StripNodes( "AXIS" );
            }
            if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
            {
                m_osProjection = pszWKT;
            }
            CPLFree(pszWKT);
        }
    }

    // Get pixel information and number of bands
    unsigned char nSampleType = 0;
    unsigned char nPixelType = 0;
    unsigned char l_nBands = 0;
    rl2_get_coverage_type (m_pRL2Coverage,
                           &nSampleType, &nPixelType, &l_nBands);
    if( !GDALCheckBandCount(l_nBands, FALSE) )
        return false;
    int nBits = 0;
    GDALDataType eDT = GDT_Unknown;
    bool bSigned = false;
    switch( nSampleType )
    {
        default:
        case RL2_SAMPLE_UNKNOWN:
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unknown sample type");
            return false;
        }
        case RL2_SAMPLE_1_BIT:
        {
            if( nPixelType == RL2_PIXEL_MONOCHROME )
            {
                m_bPromote1BitAs8Bit = CPLFetchBool( papszOpenOptions,
                                                     "1BIT_AS_8BIT", true );
            }
            nBits = 1;
            eDT = GDT_Byte;
            break;
        }
        case RL2_SAMPLE_2_BIT:
        {
            nBits = 2;
            eDT = GDT_Byte;
            break;
        }
        case RL2_SAMPLE_4_BIT:
        {
            nBits = 4;
            eDT = GDT_Byte;
            break;
        }
        case RL2_SAMPLE_INT8:
        {
            nBits = 8;
            eDT = GDT_Byte;
            bSigned = true;
            break;
        }
        case RL2_SAMPLE_UINT8:
        {
            nBits = 8;
            eDT = GDT_Byte;
            bSigned = false;
            break;
        }
        case RL2_SAMPLE_INT16:
        {
            nBits = 16;
            eDT = GDT_Int16;
            bSigned = true;
            break;
        }
        case RL2_SAMPLE_UINT16:
        {
            nBits = 16;
            eDT = GDT_UInt16;
            bSigned = false;
            break;
        }
        case RL2_SAMPLE_INT32:
        {
            nBits = 32;
            eDT = GDT_Int32;
            bSigned = true;
            break;
        }
        case RL2_SAMPLE_UINT32:
        {
            nBits = 32;
            eDT = GDT_UInt32;
            bSigned = false;
            break;
        }
        case RL2_SAMPLE_FLOAT:
        {
            nBits = 32;
            eDT = GDT_Float32;
            bSigned = true;
            break;
        }
        case RL2_SAMPLE_DOUBLE:
        {
            nBits = 64;
            eDT = GDT_Float64;
            bSigned = true;
            break;
        }
    }

    // Get information about compression (informative)
    unsigned char nCompression = 0;
    int nQuality = 0;
    rl2_get_coverage_compression (m_pRL2Coverage, &nCompression, &nQuality );
    const char* pszCompression = nullptr;
    switch( nCompression )
    {
        case RL2_COMPRESSION_DEFLATE:
        case RL2_COMPRESSION_DEFLATE_NO:
            pszCompression = "DEFLATE";
            break;
        case RL2_COMPRESSION_LZMA:
        case RL2_COMPRESSION_LZMA_NO:
            pszCompression = "LZMA";
            break;
        case RL2_COMPRESSION_GIF:
            pszCompression = "GIF";
            break;
        case RL2_COMPRESSION_JPEG:
            pszCompression = "JPEG";
            break;
        case RL2_COMPRESSION_PNG:
            pszCompression = "PNG";
            break;
        case RL2_COMPRESSION_LOSSY_WEBP:
            pszCompression = "WEBP";
            break;
        case RL2_COMPRESSION_LOSSLESS_WEBP:
            pszCompression = "WEBP_LOSSLESS";
            break;
        case RL2_COMPRESSION_CCITTFAX3:
            pszCompression = "CCITTFAX3";
            break;
        case RL2_COMPRESSION_CCITTFAX4:
            pszCompression = "CCITTFAX4";
            break;
        case RL2_COMPRESSION_LZW:
            pszCompression = "LZW";
            break;
        case RL2_COMPRESSION_LOSSY_JP2:
            pszCompression = "JPEG2000";
            break;
        case RL2_COMPRESSION_LOSSLESS_JP2:
            pszCompression = "JPEG2000_LOSSLESS";
            break;
        default:
            break;
    }

    if( pszCompression != nullptr )
    {
        GDALDataset::SetMetadataItem( "COMPRESSION", pszCompression,
                                      "IMAGE_STRUCTURE" );
    }

    if( nQuality != 0 &&
        (nCompression == RL2_COMPRESSION_JPEG ||
         nCompression == RL2_COMPRESSION_LOSSY_WEBP||
         nCompression == RL2_COMPRESSION_LOSSY_JP2 ) )
    {
        GDALDataset::SetMetadataItem( "QUALITY",
                                      CPLSPrintf("%d", nQuality),
                                      "IMAGE_STRUCTURE" );
    }

    // Get tile dimensions
    unsigned int nTileWidth = 0;
    unsigned int nTileHeight = 0;
    rl2_get_coverage_tile_size (m_pRL2Coverage, &nTileWidth, &nTileHeight);
    if( nTileWidth == 0 || nTileHeight == 0 || nTileWidth > INT_MAX ||
        nTileHeight > INT_MAX )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size");
        return false;
    }
    const int nBlockXSize = static_cast<int>(nTileWidth);
    const int nBlockYSize = static_cast<int>(nTileHeight);

    // Fetch nodata values
    std::vector<double> adfNoDataValues;
    rl2PixelPtr noDataPtr = rl2_get_coverage_no_data (m_pRL2Coverage);
    if( noDataPtr != nullptr )
    {
        unsigned char noDataSampleType = 0;
        unsigned char noDataPixelType = 0;
        unsigned char noDataBands = 0;
        if( rl2_get_pixel_type( noDataPtr, &noDataSampleType,
                                &noDataPixelType,
                                &noDataBands ) == RL2_OK &&
            noDataSampleType == nSampleType &&
            noDataPixelType == nPixelType &&
            noDataBands == l_nBands )
        {
            for( int i = 0; i < l_nBands; ++i )
            {
                double dfNoDataValue = 0.0;
                switch( nSampleType )
                {
                    default:
                    {
                        break;
                    }
                    case RL2_SAMPLE_1_BIT:
                    {
                        unsigned char nVal = 0;
                        rl2_get_pixel_sample_1bit( noDataPtr, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_2_BIT:
                    {
                        unsigned char nVal = 0;
                        rl2_get_pixel_sample_2bit( noDataPtr, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_4_BIT:
                    {
                        unsigned char nVal = 0;
                        rl2_get_pixel_sample_4bit( noDataPtr, &nVal );
                        dfNoDataValue = nVal;
                        break;

                    }
                    case RL2_SAMPLE_INT8:
                    {
                        char nVal = 0;
                        rl2_get_pixel_sample_int8( noDataPtr, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_UINT8:
                    {
                        unsigned char nVal = 0;
                        rl2_get_pixel_sample_uint8( noDataPtr, i, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_INT16:
                    {
                        short nVal = 0;
                        rl2_get_pixel_sample_int16( noDataPtr, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_UINT16:
                    {
                        unsigned short nVal = 0;
                        rl2_get_pixel_sample_uint16( noDataPtr, i, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_INT32:
                    {
                        int nVal = 0;
                        rl2_get_pixel_sample_int32( noDataPtr, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_UINT32:
                    {
                        unsigned int nVal = 0;
                        rl2_get_pixel_sample_uint32( noDataPtr, &nVal );
                        dfNoDataValue = nVal;
                        break;
                    }
                    case RL2_SAMPLE_FLOAT:
                    {
                        float fVal = 0.0f;
                        rl2_get_pixel_sample_float( noDataPtr, &fVal );
                        dfNoDataValue = fVal;
                        break;
                    }
                    case RL2_SAMPLE_DOUBLE:
                    {
                        double dfVal = 0.0;
                        rl2_get_pixel_sample_double( noDataPtr, &dfVal );
                        dfNoDataValue = dfVal;
                        break;
                    }
                }

                adfNoDataValues.push_back( dfNoDataValue );
            }

        }

        // Do not destroy noDataPtr. It belongs to m_pRL2Coverage
    }

    // The nodata concept in RasterLite2 is equivalent to the NODATA_VALUES
    // one of GDAL: the nodata value must be matched simultaneously on all
    // bands.
    if( adfNoDataValues.size() == l_nBands && l_nBands > 1 )
    {
        CPLString osNoDataValues;
        for( int i = 0; i < l_nBands; i++ )
        {
            if( !osNoDataValues.empty() )
                osNoDataValues += " ";
            osNoDataValues += CPLSPrintf("%g", adfNoDataValues[i]);
        }
        GDALDataset::SetMetadataItem( "NODATA_VALUES", osNoDataValues.c_str() );
    }

    for( int iBand = 1; iBand <= l_nBands; ++iBand )
    {
        const bool bHasNoData = adfNoDataValues.size() == 1 && l_nBands == 1;
        const double dfNoDataValue = bHasNoData ? adfNoDataValues[0] : 0.0;
        SetBand( iBand,
                 new RL2RasterBand( iBand, nPixelType,
                                    eDT, nBits, m_bPromote1BitAs8Bit,
                                    bSigned,
                                    nBlockXSize, nBlockYSize,
                                    bHasNoData,
                                    dfNoDataValue ) );
    }

    // Fetch statistics
    if( m_nSectionId < 0 || bSingleSection )
    {
        rl2RasterStatisticsPtr pStatistics =
            rl2_create_raster_statistics_from_dbms( hDB,
                                                    nullptr,
                                                    m_osCoverageName );
        if( pStatistics != nullptr )
        {
            for( int iBand = 1; iBand <= l_nBands; ++iBand )
            {
                GDALRasterBand* poBand = GetRasterBand(iBand);
                double dfMin = 0.0;
                double dfMax = 0.0;
                double dfMean = 0.0;
                double dfVariance = 0.0;
                double dfStdDev = 0.0;
                if( !(nBits == 1 && m_bPromote1BitAs8Bit) &&
                    rl2_get_band_statistics( pStatistics,
                                             static_cast<unsigned char>
                                                             (iBand - 1),
                                             &dfMin, &dfMax, &dfMean,
                                             &dfVariance,
                                             &dfStdDev ) == RL2_OK )
                {
                    poBand->GDALRasterBand::SetMetadataItem(
                        "STATISTICS_MINIMUM", CPLSPrintf("%.16g", dfMin) );
                    poBand->GDALRasterBand::SetMetadataItem(
                        "STATISTICS_MAXIMUM", CPLSPrintf("%.16g", dfMax) );
                    poBand->GDALRasterBand::SetMetadataItem(
                        "STATISTICS_MEAN", CPLSPrintf("%.16g", dfMean) );
                    poBand->GDALRasterBand::SetMetadataItem(
                        "STATISTICS_STDDEV", CPLSPrintf("%.16g", dfStdDev) );
                }
            }
            rl2_destroy_raster_statistics(pStatistics);
        }
    }

    // Fetch other metadata
    char* pszSQL = sqlite3_mprintf(
        "SELECT title, abstract FROM raster_coverages WHERE "
        "Lower(coverage_name) = Lower('%q') LIMIT 1",
        m_osCoverageName.c_str() );
    char** papszResults = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    int rc = sqlite3_get_table( hDB, pszSQL,
                                &papszResults, &nRowCount,
                                &nColCount, nullptr );
    sqlite3_free( pszSQL );
    if( rc == SQLITE_OK )
    {
        if( nRowCount ==  1 )
        {
            const char* pszTitle = papszResults[2 + 0];
            const char* pszAbstract = papszResults[2 + 1];
            if( pszTitle != nullptr && pszTitle[0] != '\0' &&
                !EQUAL(pszTitle, "*** missing Title ***") )
            {
                GDALDataset::SetMetadataItem( "COVERAGE_TITLE", pszTitle );
            }
            if( pszAbstract != nullptr && pszAbstract[0] != '\0' &&
                !EQUAL(pszAbstract, "*** missing Abstract ***") )
            {
                GDALDataset::SetMetadataItem( "COVERAGE_ABSTRACT",
                                              pszAbstract );
            }
        }
        sqlite3_free_table(papszResults);
    }

    if( m_nSectionId >= 0 )
    {
        papszResults = nullptr;
        nRowCount = 0;
        nColCount = 0;
        pszSQL = sqlite3_mprintf(
            "SELECT summary FROM \"%w\" WHERE "
            "section_id = %d LIMIT 1",
            CPLSPrintf( "%s_sections", m_osCoverageName.c_str() ),
            static_cast<int>(m_nSectionId) );
        rc = sqlite3_get_table( hDB, pszSQL,
                                    &papszResults, &nRowCount,
                                    &nColCount, nullptr );
        sqlite3_free( pszSQL );
        if( rc == SQLITE_OK )
        {
            if( nRowCount ==  1 )
            {
                const char* pszSummary = papszResults[1 + 0];
                if( pszSummary != nullptr && pszSummary[0] != '\0' )
                {
                    GDALDataset::SetMetadataItem( "SECTION_SUMMARY",
                                                  pszSummary );
                }
            }
            sqlite3_free_table(papszResults);
        }
    }

    // Instantiate overviews
    int nStrictResolution = 0;
    int nMixedResolutions = 0;
    int nSectionPaths = 0;
    int nSectionMD5 = 0;
    int nSectionSummary = 0;
    rl2_get_coverage_policies (m_pRL2Coverage,
                               &nStrictResolution,
                               &nMixedResolutions,
                               &nSectionPaths,
                               &nSectionMD5,
                               &nSectionSummary);
    m_bRL2MixedResolutions = CPL_TO_BOOL(nMixedResolutions);

    ListOverviews();

    return true;
#else // !defined(HAVE_RASTERLITE2)
    return false;
#endif // HAVE_RASTERLITE2
}

#ifdef HAVE_RASTERLITE2

/************************************************************************/
/*                          ListOverviews()                             */
/************************************************************************/

void OGRSQLiteDataSource::ListOverviews()
{
    if( !m_bRL2MixedResolutions || m_nSectionId >= 0 )
    {
        char* pszSQL;
        if( !m_bRL2MixedResolutions )
        {
            pszSQL = sqlite3_mprintf(
                "SELECT x_resolution_1_1, y_resolution_1_1, "
                "x_resolution_1_2, y_resolution_1_2, "
                "x_resolution_1_4, y_resolution_1_4,"
                "x_resolution_1_8, y_resolution_1_8 "
                "FROM \"%w\" ORDER BY pyramid_level "
                "LIMIT 1000",
                CPLSPrintf( "%s_levels", m_osCoverageName.c_str() ) );
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "SELECT x_resolution_1_1, y_resolution_1_1, "
                "x_resolution_1_2, y_resolution_1_2, "
                "x_resolution_1_4, y_resolution_1_4,"
                "x_resolution_1_8, y_resolution_1_8 "
                "FROM \"%w\" WHERE section_id = %d "
                "ORDER BY pyramid_level "
                "LIMIT 1000",
                CPLSPrintf( "%s_section_levels", m_osCoverageName.c_str() ),
                static_cast<int>(m_nSectionId) );
        }
        char** papszResults = nullptr;
        int nRowCount = 0;
        int nColCount = 0;
        char* pszErrMsg = nullptr;
        int rc = sqlite3_get_table( hDB, pszSQL,
                                &papszResults, &nRowCount,
                                &nColCount, &pszErrMsg );
        sqlite3_free( pszSQL );
        if( pszErrMsg )
            CPLDebug( "SQLite", "%s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        if( rc == SQLITE_OK )
        {
            for( int i=0; i<nRowCount; ++i )
            {
                const char* const* papszRow = papszResults + i * 8 + 8;
                const char* pszXRes1 = papszRow[0];
                const char* pszYRes1 = papszRow[1];
                const char* pszXRes2 = papszRow[2];
                const char* pszYRes2 = papszRow[3];
                const char* pszXRes4 = papszRow[4];
                const char* pszYRes4 = papszRow[5];
                const char* pszXRes8 = papszRow[6];
                const char* pszYRes8 = papszRow[7];
                if( pszXRes1 != nullptr && pszYRes1 != nullptr )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes1),
                                                      CPLAtof(pszYRes1) );
                }
                if( pszXRes2 != nullptr && pszYRes2 != nullptr )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes2),
                                                      CPLAtof(pszYRes2) );
                }
                if( pszXRes4 != nullptr && pszYRes4 != nullptr )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes4),
                                                      CPLAtof(pszYRes4) );
                }
                if( pszXRes8 != nullptr && pszYRes8 != nullptr )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes8),
                                                      CPLAtof(pszYRes8) );
                }
            }
            sqlite3_free_table(papszResults);
        }
    }
}

/************************************************************************/
/*                    CreateRL2OverviewDatasetIfNeeded()                   */
/************************************************************************/

void OGRSQLiteDataSource::CreateRL2OverviewDatasetIfNeeded( double dfXRes,
                                                            double dfYRes )
{
    if( fabs( dfXRes - m_adfGeoTransform[1] ) < 1e-5 * m_adfGeoTransform[1] )
        return;

    for( size_t i=0; i<m_apoOverviewDS.size(); ++i )
    {
        if( fabs( dfXRes - m_apoOverviewDS[i]->m_adfGeoTransform[1] ) <
                1e-5 * m_apoOverviewDS[i]->m_adfGeoTransform[1] )
        {
            return;
        }
    }

    OGRSQLiteDataSource* poOvrDS = new OGRSQLiteDataSource();
    poOvrDS->bIsInternal = true;
    poOvrDS->m_poParentDS = this;
    poOvrDS->m_osCoverageName = m_osCoverageName;
    poOvrDS->m_nSectionId = m_nSectionId;
    poOvrDS->m_bPromote1BitAs8Bit = m_bPromote1BitAs8Bit;
    poOvrDS->m_bRL2MixedResolutions = m_bRL2MixedResolutions;
    poOvrDS->m_adfGeoTransform[0] = m_adfGeoTransform[0];
    poOvrDS->m_adfGeoTransform[1] = dfXRes;
    poOvrDS->m_adfGeoTransform[3] = m_adfGeoTransform[3];
    poOvrDS->m_adfGeoTransform[5] = -dfYRes;
    const double dfMinX = m_adfGeoTransform[0];
    const double dfMaxX = dfMinX + m_adfGeoTransform[1] * nRasterXSize;
    const double dfMaxY = m_adfGeoTransform[3];
    const double dfMinY = dfMaxY + m_adfGeoTransform[5] * nRasterYSize;
    poOvrDS->nRasterXSize = static_cast<int>(0.5 + (dfMaxX - dfMinX) / dfXRes);
    poOvrDS->nRasterYSize = static_cast<int>(0.5 + (dfMaxY - dfMinY) / dfYRes);
    if( poOvrDS->nRasterXSize <= 1 || poOvrDS->nRasterYSize <= 1 ||
        (poOvrDS->nRasterXSize < 64 && poOvrDS->nRasterYSize < 64 &&
        !CPLTestBool(CPLGetConfigOption("RL2_SHOW_ALL_PYRAMID_LEVELS", "NO"))) )
    {
        delete poOvrDS;
        return;
    }
    for( int iBand = 1; iBand <= nBands; ++iBand )
    {
        poOvrDS->SetBand( iBand,
                 new RL2RasterBand(
                     reinterpret_cast<RL2RasterBand*>(GetRasterBand(iBand)) ) );
    }
    m_apoOverviewDS.push_back(poOvrDS);
}

/************************************************************************/
/*                            RL2RasterBand()                           */
/************************************************************************/

RL2RasterBand::RL2RasterBand( int nBandIn,
                              int nPixelType,
                              GDALDataType eDT,
                              int nBits,
                              bool bPromote1BitAs8Bit,
                              bool bSigned,
                              int nBlockXSizeIn,
                              int nBlockYSizeIn,
                              bool bHasNoDataIn,
                              double dfNoDataValueIn ) :
    m_bHasNoData( bHasNoDataIn ),
    m_dfNoDataValue( dfNoDataValueIn ),
    m_eColorInterp( GCI_Undefined ),
    m_poCT( nullptr )
{
    eDataType = eDT;
    nBlockXSize = nBlockXSizeIn;
    nBlockYSize = nBlockYSizeIn;
    if( (nBits % 8) != 0 )
    {
        GDALRasterBand::SetMetadataItem( (nBits == 1 && bPromote1BitAs8Bit) ?
                                                    "SOURCE_NBITS" : "NBITS",
                                         CPLSPrintf("%d", nBits),
                                         "IMAGE_STRUCTURE" );
    }
    if( nBits == 8 && bSigned )
    {
        GDALRasterBand::SetMetadataItem( "PIXELTYPE",
                                         "SIGNEDBYTE",
                                         "IMAGE_STRUCTURE" );
    }

    if( nPixelType == RL2_PIXEL_MONOCHROME ||
        nPixelType == RL2_PIXEL_GRAYSCALE )
    {
        m_eColorInterp = GCI_GrayIndex;
    }
    else if( nPixelType == RL2_PIXEL_PALETTE )
    {
        m_eColorInterp = GCI_PaletteIndex;
    }
    else if( nPixelType == RL2_PIXEL_RGB )
    {
        m_eColorInterp = static_cast<GDALColorInterp>(
                                                GCI_RedBand + nBandIn - 1 );
    }
}

/************************************************************************/
/*                            RL2RasterBand()                           */
/************************************************************************/

RL2RasterBand::RL2RasterBand(const RL2RasterBand* poOther)
{
    eDataType = poOther->eDataType;
    nBlockXSize = poOther->nBlockXSize;
    nBlockYSize = poOther->nBlockYSize;
    GDALRasterBand::SetMetadataItem( "NBITS",
        const_cast<RL2RasterBand*>(poOther)->
                    GetMetadataItem("NBITS", "IMAGE_STRUCTURE"),
        "IMAGE_STRUCTURE" );
    GDALRasterBand::SetMetadataItem( "PIXELTYPE",
        const_cast<RL2RasterBand*>(poOther)->
                    GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE"),
        "IMAGE_STRUCTURE" );
    m_eColorInterp = poOther->m_eColorInterp;
    m_bHasNoData = poOther->m_bHasNoData;
    m_dfNoDataValue = poOther->m_dfNoDataValue;
    m_poCT = nullptr;
}

/************************************************************************/
/*                           ~RL2RasterBand()                           */
/************************************************************************/

RL2RasterBand::~RL2RasterBand()
{
    delete m_poCT;
}

/************************************************************************/
/*                          GetColorTable()                             */
/************************************************************************/

GDALColorTable* RL2RasterBand::GetColorTable()
{
    OGRSQLiteDataSource* poGDS = reinterpret_cast<OGRSQLiteDataSource*>(poDS);
    if( m_poCT == nullptr && m_eColorInterp == GCI_PaletteIndex )
    {
        rl2PalettePtr palettePtr =
            rl2_get_dbms_palette(
                            poGDS->GetDB(),
                            nullptr,
                            rl2_get_coverage_name(poGDS->GetRL2CoveragePtr()) );
        if( palettePtr )
        {
            m_poCT = new GDALColorTable();
            unsigned short nEntries = 0;
            unsigned char* pabyR = nullptr;
            unsigned char* pabyG = nullptr;
            unsigned char* pabyB = nullptr;
            if( rl2_get_palette_colors( palettePtr, &nEntries,
                                        &pabyR, &pabyG, &pabyB ) == RL2_OK )
            {
                for( int i=0; i < nEntries; ++ i )
                {
                    GDALColorEntry sEntry;
                    sEntry.c1 = pabyR[i];
                    sEntry.c2 = pabyG[i];
                    sEntry.c3 = pabyB[i];
                    sEntry.c4 =
                            (m_bHasNoData && i == m_dfNoDataValue) ? 0 : 255;
                    m_poCT->SetColorEntry( i, &sEntry );
                }
                rl2_free(pabyR);
                rl2_free(pabyG);
                rl2_free(pabyB);
            }
            rl2_destroy_palette( palettePtr );
        }
    }
    return m_poCT;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int RL2RasterBand::GetOverviewCount()
{
    OGRSQLiteDataSource* poGDS = reinterpret_cast<OGRSQLiteDataSource*>(poDS);
    int nRet = static_cast<int>(poGDS->GetOverviews().size());
    if( nRet > 0 )
        return nRet;
    return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

GDALRasterBand* RL2RasterBand::GetOverview(int nIdx)
{
    OGRSQLiteDataSource* poGDS = reinterpret_cast<OGRSQLiteDataSource*>(poDS);
    int nOvr = static_cast<int>(poGDS->GetOverviews().size());
    if( nOvr > 0 )
    {
        if( nIdx < 0 || nIdx >= nOvr )
            return nullptr;
        return poGDS->GetOverviews()[nIdx]->GetRasterBand(nBand);
    }
    return GDALPamRasterBand::GetOverview(nIdx);
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double RL2RasterBand::GetNoDataValue( int* pbSuccess )
{
    if( m_bHasNoData )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfNoDataValue;
    }
    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RL2RasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, void* pData)
{
    OGRSQLiteDataSource* poGDS = reinterpret_cast<OGRSQLiteDataSource*>(poDS);
#ifdef DEBUG_VERBOSE
    CPLDebug("SQLite", "IReadBlock(ds=%p, band=%d, x=%d, y=%d)",
             poGDS, nBand, nBlockXOff, nBlockYOff);
#endif

    const int nMaxThreads = 1;
    const double* padfGeoTransform = poGDS->GetGeoTransform();
    const double dfMinX = padfGeoTransform[0] +
                          nBlockXOff * nBlockXSize * padfGeoTransform[1];
    const double dfMaxX = dfMinX + nBlockXSize * padfGeoTransform[1];
    const double dfMaxY = padfGeoTransform[3] +
                          nBlockYOff * nBlockYSize * padfGeoTransform[5];
    const double dfMinY = dfMaxY + nBlockYSize * padfGeoTransform[5];
    unsigned char* pBuffer = nullptr;
    int nBufSize = 0;

    sqlite3* hDB = poGDS->GetParentDS() ? poGDS->GetParentDS()->GetDB() :
                                          poGDS->GetDB();
    rl2CoveragePtr cov = poGDS->GetParentDS() ?
                                    poGDS->GetParentDS()->GetRL2CoveragePtr():
                                    poGDS->GetRL2CoveragePtr();
    unsigned char nSampleType = 0;
    unsigned char nPixelType = 0;
    unsigned char l_nBands = 0;
    rl2_get_coverage_type (cov,
                           &nSampleType, &nPixelType, &l_nBands);

    unsigned char nOutPixel = nPixelType;
    if( nPixelType == RL2_PIXEL_MONOCHROME &&
        nSampleType == RL2_SAMPLE_1_BIT )
    {
        nOutPixel = RL2_PIXEL_GRAYSCALE;
    }

    const GIntBig nSectionId = poGDS->GetSectionId();
    if( nSectionId >= 0 &&
        (poGDS->IsRL2MixedResolutions() || poGDS->GetParentDS() == nullptr) )
    {
        int ret = rl2_get_section_raw_raster_data( hDB,
                                                nMaxThreads,
                                                cov,
                                                nSectionId,
                                                nBlockXSize,
                                                nBlockYSize,
                                                dfMinX,
                                                dfMinY,
                                                dfMaxX,
                                                dfMaxY,
                                                padfGeoTransform[1],
                                                fabs(padfGeoTransform[5]),
                                                &pBuffer,
                                                &nBufSize,
                                                nullptr, // palette
                                                nOutPixel );
        if( ret != RL2_OK )
            return CE_Failure;
    }
    else
    {
        int ret = rl2_get_raw_raster_data( hDB,
                                                nMaxThreads,
                                                cov,
                                                nBlockXSize,
                                                nBlockYSize,
                                                dfMinX,
                                                dfMinY,
                                                dfMaxX,
                                                dfMaxY,
                                                padfGeoTransform[1],
                                                fabs(padfGeoTransform[5]),
                                                &pBuffer,
                                                &nBufSize,
                                                nullptr, // palette
                                                nOutPixel );
        if( ret != RL2_OK )
            return CE_Failure;
    }

    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const int nExpectedBytesOnBand = nBlockXSize * nBlockYSize * nDTSize;
    const int nBands = poGDS->GetRasterCount();
    const int nExpectedBytesAllBands = nExpectedBytesOnBand * nBands;
    if( nBufSize != nExpectedBytesAllBands )
    {
        CPLDebug("SQLite", "Got %d bytes instead of %d",
                 nBufSize, nExpectedBytesAllBands);
        rl2_free( pBuffer);
        return CE_Failure;
    }

    if( nPixelType == RL2_PIXEL_MONOCHROME &&
        nSampleType == RL2_SAMPLE_1_BIT &&
        !poGDS->HasPromote1BitAS8Bit() && poGDS->GetParentDS() != nullptr )
    {
        GByte* pabyDstData = static_cast<GByte*>(pData);
        for( int i = 0; i < nExpectedBytesAllBands; i++ )
        {
            pabyDstData[i] = ( pBuffer[i] > 127 ) ? 1 : 0;
        }
    }
    else
    {
        GDALCopyWords( pBuffer + (nBand - 1) * nDTSize,
                       eDataType, nDTSize * nBands,
                       pData, eDataType, nDTSize,
                       nBlockXSize * nBlockYSize );
    }

    if( nBands > 1 )
    {
        for( int iBand = 1; iBand <= nBands; ++iBand )
        {
            if( iBand == nBand )
                continue;

            GDALRasterBlock* poBlock = reinterpret_cast<RL2RasterBand*>(
                poGDS->GetRasterBand(iBand))->
                    TryGetLockedBlockRef( nBlockXOff, nBlockYOff );
            if( poBlock != nullptr )
            {
                poBlock->DropLock();
                continue;
            }
            poBlock = reinterpret_cast<RL2RasterBand*>(
                poGDS->GetRasterBand(iBand))->
                    GetLockedBlockRef( nBlockXOff, nBlockYOff, TRUE );
            if( poBlock == nullptr )
                continue;
            void* pDest = poBlock->GetDataRef();
            GDALCopyWords( pBuffer + (iBand - 1) * nDTSize,
                           eDataType, nDTSize * nBands,
                           pDest, eDataType, nDTSize,
                           nBlockXSize * nBlockYSize );

            poBlock->DropLock();
        }
    }

    rl2_free( pBuffer);

    return CE_None;
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

template<class T> static T GetNoDataValue( GDALDataset* poSrcDS,
                                           int nBand,
                                           T nDefault )
{
    int bHasNoData = FALSE;
    double dfNoData =
            poSrcDS->GetRasterBand(nBand)->GetNoDataValue(&bHasNoData);
    if( bHasNoData )
        return static_cast<T>(dfNoData);
    return static_cast<T>(nDefault);
}

/************************************************************************/
/*                          CreateNoData()                              */
/************************************************************************/

static rl2PixelPtr CreateNoData ( unsigned char nSampleType,
                                  unsigned char nPixelType,
                                  unsigned char nBandCount,
                                  GDALDataset* poSrcDS )
{
    // creating a default NO-DATA value
    rl2PixelPtr pxl = rl2_create_pixel (nSampleType, nPixelType, nBandCount);
    if (pxl == nullptr)
        return nullptr;
    switch (nPixelType)
    {
        case RL2_PIXEL_MONOCHROME:
            rl2_set_pixel_sample_1bit (pxl,
                                       GetNoDataValue<GByte>(poSrcDS, 1, 0));
            break;
        case RL2_PIXEL_PALETTE:
            switch (nSampleType)
            {
                case RL2_SAMPLE_1_BIT:
                    rl2_set_pixel_sample_1bit (pxl,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_2_BIT:
                    rl2_set_pixel_sample_2bit (pxl,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_4_BIT:
                    rl2_set_pixel_sample_4bit (pxl,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_UINT8:
                    rl2_set_pixel_sample_uint8 (pxl, 0,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 0));
                    break;
                default:
                    CPLAssert(false);
                    break;
            }
            break;
        case RL2_PIXEL_GRAYSCALE:
            switch (nSampleType)
            {
                case RL2_SAMPLE_1_BIT:
                    rl2_set_pixel_sample_1bit (pxl,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 1));
                    break;
                case RL2_SAMPLE_2_BIT:
                    rl2_set_pixel_sample_2bit (pxl,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 3));
                    break;
                case RL2_SAMPLE_4_BIT:
                    rl2_set_pixel_sample_4bit (pxl,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 15));
                    break;
                case RL2_SAMPLE_UINT8:
                    rl2_set_pixel_sample_uint8 (pxl, 0,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 255));
                    break;
                case RL2_SAMPLE_UINT16:
                    rl2_set_pixel_sample_uint16 (pxl, 0,
                                      GetNoDataValue<GUInt16>(poSrcDS, 1, 0));
                    break;
                default:
                    CPLAssert(false);
                    break;
            }
            break;
        case RL2_PIXEL_RGB:
            switch (nSampleType)
            {
                case RL2_SAMPLE_UINT8:
                    rl2_set_pixel_sample_uint8 (pxl, 0,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 255));
                    rl2_set_pixel_sample_uint8 (pxl, 1,
                                        GetNoDataValue<GByte>(poSrcDS, 2, 255));
                    rl2_set_pixel_sample_uint8 (pxl, 2,
                                        GetNoDataValue<GByte>(poSrcDS, 3, 255));
                    break;
                case RL2_SAMPLE_UINT16:
                    rl2_set_pixel_sample_uint16 (pxl, 0,
                                        GetNoDataValue<GUInt16>(poSrcDS, 1, 0));
                    rl2_set_pixel_sample_uint16 (pxl, 1,
                                        GetNoDataValue<GUInt16>(poSrcDS, 2, 0));
                    rl2_set_pixel_sample_uint16 (pxl, 2,
                                        GetNoDataValue<GUInt16>(poSrcDS, 3, 0));
                    break;
                default:
                    CPLAssert(false);
                    break;
            }
            break;
        case RL2_PIXEL_DATAGRID:
            switch (nSampleType)
            {
                case RL2_SAMPLE_INT8:
                    rl2_set_pixel_sample_int8 (pxl,
                                        GetNoDataValue<char>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_UINT8:
                    rl2_set_pixel_sample_uint8 (pxl, 0,
                                        GetNoDataValue<GByte>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_INT16:
                    rl2_set_pixel_sample_int16 (pxl,
                                        GetNoDataValue<GInt16>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_UINT16:
                    rl2_set_pixel_sample_uint16 (pxl, 0,
                                        GetNoDataValue<GUInt16>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_INT32:
                    rl2_set_pixel_sample_int32 (pxl,
                                        GetNoDataValue<GInt32>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_UINT32:
                    rl2_set_pixel_sample_uint32 (pxl,
                                        GetNoDataValue<GUInt32>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_FLOAT:
                    rl2_set_pixel_sample_float (pxl,
                                        GetNoDataValue<float>(poSrcDS, 1, 0));
                    break;
                case RL2_SAMPLE_DOUBLE:
                    rl2_set_pixel_sample_double (pxl,
                                        GetNoDataValue<double>(poSrcDS, 1, 0));
                    break;
                default:
                    CPLAssert(false);
                    break;
            }
            break;
        case RL2_PIXEL_MULTIBAND:
            switch (nSampleType)
            {
                case RL2_SAMPLE_UINT8:
                    for (unsigned int nb = 0; nb < nBandCount; nb++)
                        rl2_set_pixel_sample_uint8 (pxl, nb,
                            GetNoDataValue<GByte>(poSrcDS, nb+1, 255));
                    break;
                case RL2_SAMPLE_UINT16:
                    for (unsigned int nb = 0; nb < nBandCount; nb++)
                        rl2_set_pixel_sample_uint16 (pxl, nb,
                            GetNoDataValue<GUInt16>(poSrcDS, nb+1, 0));
                    break;
                default:
                    CPLAssert(false);
                    break;
            }
            break;
        default:
            CPLAssert(false);
            break;
    }
    return pxl;
}

/************************************************************************/
/*                       RasterLite2Callback()                          */
/************************************************************************/

typedef struct
{
    GDALDataset* poSrcDS;
    unsigned char nPixelType;
    unsigned char nSampleType;
    rl2PalettePtr pPalette;
    GDALProgressFunc pfnProgress;
    void * pProgressData;
    double adfGeoTransform[6];
} RasterLite2CallbackData;

static int RasterLite2Callback( void *data,
                                double dfTileMinX,
                                double dfTileMinY,
                                double dfTileMaxX,
                                double dfTileMaxY,
                                unsigned char *pabyBuffer,
                                rl2PalettePtr* pOutPalette )
{
#ifdef DEBUG_VERBOSE
    CPLDebug("SQLite", "RasterLite2Callback(%f %f %f %f)",
             dfTileMinX, dfTileMinY, dfTileMaxX, dfTileMaxY);
#endif
    RasterLite2CallbackData* pCbkData =
                            static_cast<RasterLite2CallbackData*>(data);
    if( pOutPalette )
    {
        if( pCbkData->pPalette )
            *pOutPalette = rl2_clone_palette( pCbkData->pPalette );
        else
            *pOutPalette = nullptr;
    }
    int nXOff = static_cast<int>(0.5 +
        (dfTileMinX - pCbkData->adfGeoTransform[0]) /
                                pCbkData->adfGeoTransform[1]);
    int nXOff2 = static_cast<int>(0.5 +
        (dfTileMaxX - pCbkData->adfGeoTransform[0]) /
                                pCbkData->adfGeoTransform[1]);
    int nYOff = static_cast<int>(0.5 +
        (dfTileMaxY - pCbkData->adfGeoTransform[3]) /
                                pCbkData->adfGeoTransform[5]);
    int nYOff2 = static_cast<int>(0.5 +
        (dfTileMinY - pCbkData->adfGeoTransform[3]) /
                                pCbkData->adfGeoTransform[5]);
    int nReqXSize = nXOff2 - nXOff;
    bool bZeroInitialize = false;
    if( nXOff2 > pCbkData->poSrcDS->GetRasterXSize() )
    {
        bZeroInitialize = true;
        nReqXSize = pCbkData->poSrcDS->GetRasterXSize() - nXOff;
    }
    int nReqYSize = nYOff2 - nYOff;
    if( nYOff2 > pCbkData->poSrcDS->GetRasterYSize() )
    {
        bZeroInitialize = true;
        nReqYSize = pCbkData->poSrcDS->GetRasterYSize() - nYOff;
    }

    GDALDataType eDT = pCbkData->poSrcDS->GetRasterBand(1)->GetRasterDataType();
    int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    int nBands = pCbkData->poSrcDS->GetRasterCount();
    if( bZeroInitialize )
    {
        memset( pabyBuffer, 0,
                static_cast<size_t>(nXOff2 - nXOff) *
                                   (nYOff2 - nYOff) * nBands * nDTSize );
    }

    const GSpacing nPixelSpacing = static_cast<GSpacing>(nDTSize) * nBands;
    const GSpacing nLineSpacing = nPixelSpacing * (nXOff2 - nXOff);
    CPLErr eErr = pCbkData->poSrcDS->RasterIO( GF_Read,
                                               nXOff, nYOff,
                                               nReqXSize, nReqYSize,
                                               pabyBuffer,
                                               nReqXSize, nReqYSize,
                                               eDT,
                                               nBands,
                                               nullptr,
                                               nPixelSpacing,
                                               nLineSpacing,
                                               nDTSize,
                                               nullptr );
    if( eErr != CE_None )
        return FALSE;

    if( pCbkData->pfnProgress &&
        !pCbkData->pfnProgress(static_cast<double>(nYOff + nReqYSize) /
                                    pCbkData->poSrcDS->GetRasterYSize(),
                               "", pCbkData->pProgressData) )
    {
        return FALSE;
    }

    int nMaxVal = 0;
    if( pCbkData->nSampleType == RL2_SAMPLE_1_BIT )
    {
        nMaxVal = 1;
    }
    else if( pCbkData->nSampleType == RL2_SAMPLE_2_BIT )
    {
        nMaxVal = 3;
    }
    else if( pCbkData->nSampleType == RL2_SAMPLE_4_BIT )
    {
        nMaxVal = 7;
    }
    if( nMaxVal != 0 )
    {
        bool bClamped = false;
        for( int iY = 0; iY < nReqYSize; ++iY )
        {
            for( int iX = 0; iX < nReqXSize; ++iX )
            {
                GByte* pbyVal = pabyBuffer +
                        static_cast<size_t>(iY) * (nXOff2 - nXOff) + iX;
                if( *pbyVal > nMaxVal )
                {
                    if( !bClamped )
                    {
                        bClamped = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "One or several values above %d have "
                                 "been clamped",
                                 nMaxVal);
                    }
                    *pbyVal = nMaxVal;
                }
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                    OGRSQLiteDriverCreateCopy()                       */
/************************************************************************/

GDALDataset *OGRSQLiteDriverCreateCopy( const char* pszName,
                                        GDALDataset* poSrcDS,
                                        int /* bStrict */,
                                        char ** papszOptions,
                                        GDALProgressFunc pfnProgress,
                                        void * pProgressData )
{
    if( poSrcDS->GetRasterCount() == 0 ||
        poSrcDS->GetRasterCount() > 255 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return nullptr;
    }

    double adfGeoTransform[6];
    if( poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None &&
        (adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Raster with rotation/shearing geotransform terms "
                 "are not supported");
        return nullptr;
    }

    if( CSLFetchNameValue(papszOptions, "APPEND_SUBDATASET") &&
        !CSLFetchNameValue(papszOptions, "COVERAGE") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "COVERAGE must be specified with APPEND_SUBDATASET=YES");
        return nullptr;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    unsigned char nSampleType = RL2_SAMPLE_UINT8;
    unsigned char nPixelType = RL2_PIXEL_GRAYSCALE;
    unsigned char nBandCount = static_cast<unsigned char>(
                                            poSrcDS->GetRasterCount());

    const char* pszPixelType = CSLFetchNameValue(papszOptions, "PIXEL_TYPE");
    if( pszPixelType )
    {
        if( EQUAL(pszPixelType, "MONOCHROME") )
            nPixelType = RL2_PIXEL_MONOCHROME;
        else if( EQUAL(pszPixelType, "PALETTE") )
            nPixelType = RL2_PIXEL_PALETTE;
        else if( EQUAL(pszPixelType, "GRAYSCALE") )
            nPixelType = RL2_PIXEL_GRAYSCALE;
        else if( EQUAL(pszPixelType, "RGB") )
            nPixelType = RL2_PIXEL_RGB;
        else if( EQUAL(pszPixelType, "MULTIBAND") )
            nPixelType = RL2_PIXEL_MULTIBAND;
        else if( EQUAL(pszPixelType, "DATAGRID") )
            nPixelType = RL2_PIXEL_DATAGRID;
    }
    else
    {
        // Guess a reasonable pixel type from band characteristics
        if( nBandCount == 1 &&
            poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr )
        {
            nPixelType = RL2_PIXEL_PALETTE;
        }
        else if( nBandCount == 3 && (eDT == GDT_Byte || eDT == GDT_UInt16) &&
                 poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
                                                            GCI_RedBand &&
                 poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
                                                            GCI_GreenBand &&
                 poSrcDS->GetRasterBand(3)->GetColorInterpretation() ==
                                                            GCI_BlueBand )
        {
            nPixelType = RL2_PIXEL_RGB;
        }
        else if( nBandCount > 1 && (eDT == GDT_Byte || eDT == GDT_UInt16) )
        {
            nPixelType = RL2_PIXEL_MULTIBAND;
        }
        else if( nBandCount == 1 && eDT != GDT_Byte )
        {
            nPixelType = RL2_PIXEL_DATAGRID;
        }
    }

    // Deal with NBITS
    const char* pszNBITS = CSLFetchNameValue(papszOptions, "NBITS");
    int nBITS = 0;
    if( pszNBITS != nullptr )
    {
        nBITS = atoi(pszNBITS);
        if( nBITS != 1 && nBITS != 2 && nBITS != 4 && nBITS != 8 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported NBITS value");
            return nullptr;
        }
    }
    else
    {
        pszNBITS = poSrcDS->GetRasterBand(1)->GetMetadataItem(
                                                "NBITS", "IMAGE_STRUCTURE");
        if( pszNBITS != nullptr )
        {
            nBITS = atoi(pszNBITS);
        }
    }

    if( nBITS > 0 && nBITS <= 8 && eDT != GDT_Byte )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "NBITS <= 8 only compatible with Byte data type");
        return nullptr;
    }

    if( nBITS == 1 )
    {
        nSampleType = RL2_SAMPLE_1_BIT;
        if( nPixelType != RL2_PIXEL_PALETTE && pszPixelType == nullptr )
            nPixelType = RL2_PIXEL_MONOCHROME;
    }
    else if( nBITS == 2 )
    {
        nSampleType = RL2_SAMPLE_2_BIT;
        if( nPixelType != RL2_PIXEL_PALETTE && pszPixelType == nullptr )
            nPixelType = RL2_PIXEL_GRAYSCALE;
    }
    else if( nBITS == 4 )
    {
        nSampleType = RL2_SAMPLE_4_BIT;
        if( nPixelType != RL2_PIXEL_PALETTE && pszPixelType == nullptr )
            nPixelType = RL2_PIXEL_GRAYSCALE;
    }

    if( nPixelType == RL2_PIXEL_MONOCHROME )
    {
        if( eDT != GDT_Byte )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Incompatible data type for MONOCHROME");
            return nullptr;
        }
        // Force 1 bit
        nSampleType = RL2_SAMPLE_1_BIT;
    }

    // Guess sample type in other cases
    if( eDT == GDT_UInt16 )
        nSampleType = RL2_SAMPLE_UINT16;
    else if( eDT == GDT_Int16 )
        nSampleType = RL2_SAMPLE_INT16;
    else if( eDT == GDT_UInt32 )
        nSampleType = RL2_SAMPLE_UINT32;
    else if( eDT == GDT_Int32 )
        nSampleType = RL2_SAMPLE_INT32;
    else if( eDT == GDT_Float32 )
        nSampleType = RL2_SAMPLE_FLOAT;
    else if( eDT == GDT_Float64 )
        nSampleType = RL2_SAMPLE_DOUBLE;
    else if( eDT != GDT_Byte )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type");
        return nullptr;
    }

    unsigned char nCompression = RL2_COMPRESSION_NONE;
    int nQuality = 100;
    const char* pszCompression = CSLFetchNameValue( papszOptions, "COMPRESS" );
    if( pszCompression )
    {
        if( EQUAL( pszCompression, "NONE") )
            nCompression = RL2_COMPRESSION_NONE;
        else if( EQUAL( pszCompression, "DEFLATE") )
            nCompression = RL2_COMPRESSION_DEFLATE;
        else if( EQUAL( pszCompression, "LZMA") )
            nCompression = RL2_COMPRESSION_LZMA;
        else if( EQUAL( pszCompression, "PNG") )
            nCompression = RL2_COMPRESSION_PNG;
        else if( EQUAL( pszCompression, "CCITTFAX4") )
            nCompression = RL2_COMPRESSION_CCITTFAX4;
        else if( EQUAL( pszCompression, "JPEG") )
        {
            nCompression = RL2_COMPRESSION_JPEG;
            nQuality = 75;
        }
        else if( EQUAL( pszCompression, "WEBP") )
        {
            nCompression = RL2_COMPRESSION_LOSSY_WEBP;
            nQuality = 75;
        }
        else if( EQUAL( pszCompression, "JPEG2000") )
        {
            nCompression = RL2_COMPRESSION_LOSSY_JP2;
            nQuality = 20;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unsupported compression");
            return nullptr;
        }
        if( !rl2_is_supported_codec(nCompression) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "librasterlite2 is not built with support for "
                     "this compression method.");
            return nullptr;
        }
    }

    // Compatibility checks:
    // see https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=reference_table
    if( nPixelType == RL2_PIXEL_MONOCHROME )
    {
        if( nBandCount != 1 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported band count with MONOCHROME");
            return nullptr;
        }
        CPLAssert( nSampleType == RL2_SAMPLE_1_BIT );
    }
    else if( nPixelType == RL2_PIXEL_PALETTE )
    {
        if( nBandCount != 1 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported band count with PALETTE");
            return nullptr;
        }
        if( nSampleType != RL2_SAMPLE_1_BIT &&
            nSampleType != RL2_SAMPLE_2_BIT &&
            nSampleType != RL2_SAMPLE_4_BIT &&
            nSampleType != RL2_SAMPLE_UINT8 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported sample type with PALETTE");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_GRAYSCALE )
    {
        if( nBandCount != 1 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported band count with GRAYSCALE");
            return nullptr;
        }
        if( nSampleType != RL2_SAMPLE_2_BIT &&
            nSampleType != RL2_SAMPLE_4_BIT &&
            nSampleType != RL2_SAMPLE_UINT8 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported sample type with GRAYSCALE");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_RGB )
    {
        if( nBandCount != 3 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported band count with RGB");
            return nullptr;
        }
        if( nSampleType != RL2_SAMPLE_UINT8 &&
            nSampleType != RL2_SAMPLE_UINT16 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported sample type with RGB");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_MULTIBAND )
    {
        if( nBandCount == 1 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported band count with MULTIBAND");
            return nullptr;
        }
        if( nSampleType != RL2_SAMPLE_UINT8 &&
            nSampleType != RL2_SAMPLE_UINT16 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported sample type with MULTIBAND");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_DATAGRID )
    {
        if( nBandCount != 1 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported band count with DATAGRID");
            return nullptr;
        }
        if( nSampleType != RL2_SAMPLE_INT8 &&
            nSampleType != RL2_SAMPLE_UINT8 &&
            nSampleType != RL2_SAMPLE_INT16 &&
            nSampleType != RL2_SAMPLE_UINT16 &&
            nSampleType != RL2_SAMPLE_INT32 &&
            nSampleType != RL2_SAMPLE_UINT32 &&
            nSampleType != RL2_SAMPLE_FLOAT &&
            nSampleType != RL2_SAMPLE_DOUBLE )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported sample type with DATAGRID");
            return nullptr;
        }
    }

    // Other compatibility checks based on compression
    if( nPixelType == RL2_PIXEL_MONOCHROME )
    {
        if( nCompression != RL2_COMPRESSION_NONE &&
            nCompression != RL2_COMPRESSION_DEFLATE &&
            nCompression != RL2_COMPRESSION_DEFLATE_NO &&
            nCompression != RL2_COMPRESSION_LZMA &&
            nCompression != RL2_COMPRESSION_LZMA_NO &&
            nCompression != RL2_COMPRESSION_CCITTFAX4 &&
            nCompression != RL2_COMPRESSION_PNG )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with MONOCHROME");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_PALETTE )
    {
        if( nCompression != RL2_COMPRESSION_NONE &&
            nCompression != RL2_COMPRESSION_DEFLATE &&
            nCompression != RL2_COMPRESSION_DEFLATE_NO &&
            nCompression != RL2_COMPRESSION_LZMA &&
            nCompression != RL2_COMPRESSION_LZMA_NO &&
            nCompression != RL2_COMPRESSION_PNG )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with PALETTE");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_GRAYSCALE )
    {
        if( nCompression == RL2_COMPRESSION_CCITTFAX4 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with GRAYSCALE");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_RGB && nSampleType == RL2_SAMPLE_UINT8 )
    {
        if( nCompression == RL2_COMPRESSION_CCITTFAX4 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with RGB UINT8");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_RGB && nSampleType == RL2_SAMPLE_UINT16 )
    {
        if( nCompression == RL2_COMPRESSION_CCITTFAX4 ||
            nCompression == RL2_COMPRESSION_JPEG ||
            nCompression == RL2_COMPRESSION_LOSSY_WEBP ||
            nCompression == RL2_COMPRESSION_LOSSLESS_WEBP )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with RGB UINT16");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_MULTIBAND &&
             nSampleType == RL2_SAMPLE_UINT8 &&
             (nBandCount == 3 || nBandCount == 4) )
    {
        if( nCompression == RL2_COMPRESSION_CCITTFAX4 ||
            nCompression == RL2_COMPRESSION_JPEG  )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with MULTIBAND UINT8 %d bands",
                     nBandCount);
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_MULTIBAND &&
             nSampleType == RL2_SAMPLE_UINT16 &&
             (nBandCount == 3 || nBandCount == 4) )
    {
        if( nCompression == RL2_COMPRESSION_CCITTFAX4 ||
            nCompression == RL2_COMPRESSION_JPEG ||
            nCompression == RL2_COMPRESSION_LOSSY_WEBP ||
            nCompression == RL2_COMPRESSION_LOSSLESS_WEBP  )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with MULTIBAND UINT16 %d bands",
                     nBandCount);
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_MULTIBAND )
    {
        if( nCompression != RL2_COMPRESSION_NONE &&
            nCompression != RL2_COMPRESSION_DEFLATE &&
            nCompression != RL2_COMPRESSION_DEFLATE_NO &&
            nCompression != RL2_COMPRESSION_LZMA &&
            nCompression != RL2_COMPRESSION_LZMA_NO )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with MULTIBAND %s %d bands",
                     (nSampleType == RL2_SAMPLE_UINT8) ? "UINT8" : "UINT16",
                     nBandCount);
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_DATAGRID &&
             (nSampleType == RL2_SAMPLE_UINT8 ||
              nSampleType == RL2_SAMPLE_UINT16) )
    {
        if( nCompression == RL2_COMPRESSION_CCITTFAX4 ||
            nCompression == RL2_COMPRESSION_JPEG ||
            nCompression == RL2_COMPRESSION_LOSSY_WEBP ||
            nCompression == RL2_COMPRESSION_LOSSLESS_WEBP  )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with DATAGRID %s",
                     (nSampleType == RL2_SAMPLE_UINT8) ? "UINT8" : "UINT16");
            return nullptr;
        }
    }
    else if( nPixelType == RL2_PIXEL_DATAGRID &&
             nSampleType != RL2_SAMPLE_UINT8 &&
             nSampleType != RL2_SAMPLE_UINT16 )
    {
        if( nCompression != RL2_COMPRESSION_NONE &&
            nCompression != RL2_COMPRESSION_DEFLATE &&
            nCompression != RL2_COMPRESSION_DEFLATE_NO &&
            nCompression != RL2_COMPRESSION_LZMA &&
            nCompression != RL2_COMPRESSION_LZMA_NO )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported compression with DATAGRID %s",
                     GDALGetDataTypeName(eDT));
            return nullptr;
        }
    }

    const char* pszQuality = CSLFetchNameValue( papszOptions, "QUALITY" );
    if( pszQuality )
    {
        nQuality = atoi(pszQuality);
        if( nQuality == 100 && nCompression == RL2_COMPRESSION_LOSSY_JP2 )
            nCompression = RL2_COMPRESSION_LOSSLESS_JP2;
        else if( nQuality == 100 && nCompression == RL2_COMPRESSION_LOSSY_WEBP )
            nCompression = RL2_COMPRESSION_LOSSLESS_WEBP;
    }

    unsigned int nTileWidth = atoi( CSLFetchNameValueDef(papszOptions,
                                                         "BLOCKXSIZE",
                                                         "512") );
    unsigned int nTileHeight = atoi( CSLFetchNameValueDef(papszOptions,
                                                         "BLOCKYSIZE",
                                                         "512") );

/* -------------------------------------------------------------------- */
/*      Try to create datasource.                                       */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource *poDS = new OGRSQLiteDataSource();

    if( CSLFetchNameValue(papszOptions, "APPEND_SUBDATASET") )
    {
        GDALOpenInfo oOpenInfo(pszName,
                               GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_UPDATE);
        if( !poDS->Open(&oOpenInfo) )
        {
            delete poDS;
            return nullptr;
        }
    }
    else
    {
        char** papszNewOptions = CSLDuplicate(papszOptions);
        papszNewOptions = CSLSetNameValue(papszNewOptions, "SPATIALITE", "YES");
        if( !poDS->Create( pszName, papszNewOptions ) )
        {
            CSLDestroy(papszNewOptions);
            delete poDS;
            return nullptr;
        }
        CSLDestroy(papszNewOptions);
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding to the srs table if needed.                              */
/* -------------------------------------------------------------------- */
    int nSRSId = 0;
    const char* pszSRID = CSLFetchNameValue(papszOptions, "SRID");

    if( pszSRID != nullptr )
    {
        nSRSId = atoi(pszSRID);
        if( nSRSId > 0 )
        {
            OGRSpatialReference* poSRSFetched = poDS->FetchSRS( nSRSId );
            if( poSRSFetched == nullptr )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "SRID %d will be used, but no matching SRS is "
                         "defined in spatial_ref_sys",
                         nSRSId);
            }
        }
    }
    else
    {
        const OGRSpatialReference* poSRS = poSrcDS->GetSpatialRef();
        if( poSRS )
        {
            nSRSId = poDS->FetchSRSId( poSRS );
        }
    }

    poDS->StartTransaction();

    char** papszResults = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    sqlite3_get_table( poDS->GetDB(),
                  "SELECT * FROM sqlite_master WHERE "
                  "name = 'raster_coverages' AND type = 'table'",
                   &papszResults, &nRowCount,
                   &nColCount, nullptr );
    sqlite3_free_table(papszResults);
    if( nRowCount == 0 )
    {
        char* pszErrMsg = nullptr;
        int ret = sqlite3_exec (poDS->GetDB(),
                                "SELECT CreateRasterCoveragesTable()", nullptr,
                                nullptr, &pszErrMsg);
        if (ret != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "CreateRasterCoveragesTable() failed: %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
            delete poDS;
            return nullptr;
        }
    }

    CPLString osCoverageName( CSLFetchNameValueDef(papszOptions,
                                                   "COVERAGE",
                                                   CPLGetBasename(pszName)) );
    // Check if the coverage already exists
    rl2CoveragePtr cvg = nullptr;
    char* pszSQL = sqlite3_mprintf(
            "SELECT coverage_name "
            "FROM raster_coverages WHERE coverage_name = '%q' LIMIT 1",
            osCoverageName.c_str());
    sqlite3_get_table( poDS->GetDB(), pszSQL,  &papszResults, &nRowCount,
                       &nColCount, nullptr );
    sqlite3_free(pszSQL);
    sqlite3_free_table(papszResults);
    if( nRowCount == 1 )
    {
        cvg = rl2_create_coverage_from_dbms( poDS->GetDB(),
                                             nullptr,
                                             osCoverageName );
        if( cvg == nullptr )
        {
            delete poDS;
            return nullptr;
        }
    }

    rl2PalettePtr pPalette = nullptr;
    if( nPixelType == RL2_PIXEL_PALETTE )
    {
        GDALColorTable* poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
        if( poCT == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing color table");
            delete poDS;
            return nullptr;
        }

        const int nColors = poCT->GetColorEntryCount();
        pPalette = rl2_create_palette( nColors );
        for( int i=0; i<nColors; ++i )
        {
            const GDALColorEntry* poCE = poCT->GetColorEntry(i);
            rl2_set_palette_color (pPalette, i,
                                    static_cast<GByte>(poCE->c1),
                                    static_cast<GByte>(poCE->c2),
                                    static_cast<GByte>(poCE->c3));
        }
    }

    if( cvg == nullptr )
    {
        const double dfXRes = adfGeoTransform[1];
        const double dfYRes = fabs(adfGeoTransform[5]);
        const bool bStrictResolution = true;
        const bool bMixedResolutions = false;
        const bool bSectionPaths = false;
        const bool bSectionMD5 = false;
        const bool bSectionSummary = false;
        const bool bIsQueryable = false;

        rl2PixelPtr pNoData =
            CreateNoData( nSampleType, nPixelType, nBandCount, poSrcDS );
        if( pNoData == nullptr )
        {
            delete poDS;
            if( pPalette )
                rl2_destroy_palette(pPalette);
            return nullptr;
        }

        if( rl2_create_dbms_coverage(poDS->GetDB(),
                                    osCoverageName,
                                    nSampleType,
                                    nPixelType,
                                    nBandCount,
                                    nCompression,
                                    nQuality,
                                    nTileWidth,
                                    nTileHeight,
                                    nSRSId,
                                    dfXRes,
                                    dfYRes,
                                    pNoData,
                                    pPalette,
                                    bStrictResolution,
                                    bMixedResolutions,
                                    bSectionPaths,
                                    bSectionMD5,
                                    bSectionSummary,
                                    bIsQueryable) != RL2_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "rl2_create_dbms_coverage() failed");
            rl2_destroy_pixel (pNoData);
            if( pPalette )
                rl2_destroy_palette(pPalette);
            delete poDS;
            return nullptr;
        }

        rl2_destroy_pixel (pNoData);
    }

    if( cvg == nullptr )
    {
        cvg = rl2_create_coverage_from_dbms( poDS->GetDB(),
                                             nullptr,
                                             osCoverageName );
        if (cvg == nullptr)
        {
            if( pPalette )
                rl2_destroy_palette(pPalette);
            delete poDS;
            return nullptr;
        }
    }

    if( adfGeoTransform[5] > 0 )
        adfGeoTransform[5] = -adfGeoTransform[5];
    double dfXMin = adfGeoTransform[0];
    double dfXMax = dfXMin + adfGeoTransform[1] * poSrcDS->GetRasterXSize();
    double dfYMax = adfGeoTransform[3];
    double dfYMin = dfYMax + adfGeoTransform[5] * poSrcDS->GetRasterYSize();

    CPLString osSectionName( CSLFetchNameValueDef(papszOptions,
                                                  "SECTION",
                                                  CPLGetBasename(pszName)) );
    const bool bPyramidize = CPLFetchBool( papszOptions, "PYRAMIDIZE", false );
    RasterLite2CallbackData cbk_data;
    cbk_data.poSrcDS = poSrcDS;
    cbk_data.nPixelType = nPixelType;
    cbk_data.nSampleType = nSampleType;
    cbk_data.pPalette = pPalette;
    cbk_data.pfnProgress = pfnProgress;
    cbk_data.pProgressData = pProgressData;
    memcpy( &cbk_data.adfGeoTransform, adfGeoTransform,
            sizeof(adfGeoTransform) );

    if( rl2_load_raw_tiles_into_dbms(poDS->GetDB(),
                                     poDS->GetRL2Context(),
                                     cvg,
                                     osSectionName,
                                     poSrcDS->GetRasterXSize(),
                                     poSrcDS->GetRasterYSize(),
                                     nSRSId,
                                     dfXMin, dfYMin, dfXMax, dfYMax,
                                     RasterLite2Callback,
                                     &cbk_data,
                                     bPyramidize) != RL2_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "rl2_load_raw_tiles_into_dbms() failed");
        delete poDS;
        rl2_destroy_coverage (cvg);
        if( pPalette )
            rl2_destroy_palette(pPalette);
        return nullptr;
    }

    rl2_destroy_coverage (cvg);
    if( pPalette )
        rl2_destroy_palette(pPalette);

    poDS->CommitTransaction();

    delete poDS;

    poDS = new OGRSQLiteDataSource();
    GDALOpenInfo oOpenInfo(
        CPLSPrintf("RASTERLITE2:%s:%s",
                           EscapeNameAndQuoteIfNeeded(pszName).c_str(),
                           EscapeNameAndQuoteIfNeeded(osCoverageName).c_str()),
        GDAL_OF_RASTER | GDAL_OF_UPDATE);
    poDS->Open(&oOpenInfo);
    return poDS;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr OGRSQLiteDataSource::IBuildOverviews(
    const char * pszResampling,
    int nOverviews, int * panOverviewList,
    int nBandsIn, int * /*panBandList */,
    GDALProgressFunc /*pfnProgress*/, void * /*pProgressData*/ )

{
    if( nBandsIn != nBands )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only build of all bands is supported");
    }

    if( nOverviews == 0 )
    {
        int ret;
        if( m_bRL2MixedResolutions && m_nSectionId >= 0 )
        {
            ret = rl2_delete_section_pyramid (hDB, m_osCoverageName,
                                              m_nSectionId);
        }
        else
        {
            ret = rl2_delete_all_pyramids (hDB, m_osCoverageName);
        }
        if( ret != RL2_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Deletion of pyramids failed");
            return CE_Failure;
        }
    }
    else
    {
        if( !STARTS_WITH_CI(pszResampling, "NEAR") )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                "Resampling method is ignored. Using librasterlite2 own method");
        }
        for( int i = 0; i < nOverviews; ++i )
        {
            if( !CPLIsPowerOfTwo(panOverviewList[i]) )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Only power-of-two overview factors are supported");
                return CE_Failure;
            }
        }

        const int bForcedRebuild = 1;
        const int bVerbose = 0;
        const int bVirtualLevels = 1;
        int ret;
        if( m_bRL2MixedResolutions )
        {
            if( m_nSectionId >= 0 )
            {
                ret = rl2_build_section_pyramid( hDB, GetRL2Context(),
                                           m_osCoverageName, m_nSectionId,
                                           bForcedRebuild, bVerbose);
            }
            else
            {
                ret = rl2_build_monolithic_pyramid (hDB, GetRL2Context(),
                                                    m_osCoverageName,
                                                    bVirtualLevels,
                                                    bVerbose);

            }
        }
        else
        {
            ret = rl2_build_monolithic_pyramid (hDB,
                                                GetRL2Context(),
                                                m_osCoverageName,
                                                bVirtualLevels,
                                                bVerbose);
        }
        if( ret != RL2_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Build of pyramids failed");
            return CE_Failure;
        }
    }

    for(size_t i=0;i<m_apoOverviewDS.size();++i)
        delete m_apoOverviewDS[i];
    m_apoOverviewDS.clear();
    ListOverviews();

    return CE_None;
}

#endif // HAVE_RASTERLITE2

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char** OGRSQLiteDataSource::GetMetadata(const char* pszDomain)
{
    if( pszDomain != nullptr && EQUAL( pszDomain, "SUBDATASETS" ) &&
        m_aosSubDatasets.size() > 2 )
    {
        return m_aosSubDatasets.List();
    }
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                           GetGeoTransform()                          */
/************************************************************************/

CPLErr OGRSQLiteDataSource::GetGeoTransform( double* padfGeoTransform )
{
    if( m_bGeoTransformValid )
    {
        memcpy( padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double) );
        return CE_None;
    }
    return GDALPamDataset::GetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                           GetProjectionRef()                         */
/************************************************************************/

const char* OGRSQLiteDataSource::_GetProjectionRef()
{
    if( !m_osProjection.empty() )
        return m_osProjection.c_str();
    return GDALPamDataset::_GetProjectionRef();
}
