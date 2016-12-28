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

#include "ogr_sqlite.h"
#include "rasterlite2_header.h"

#ifdef HAVE_RASTERLITE2

static CPLString EscapeNameAndQuoteIfNeeded(const char* pszName)
{
    if( strchr(pszName, '"') == NULL && strchr(pszName, ':') == NULL )
        return pszName;
    return '"' + OGRSQLiteEscapeName(pszName) + '"';
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
    char** papszResults = NULL;
    int nRowCount = 0, nColCount = 0;
    int rc = sqlite3_get_table( hDB,
                           "SELECT name FROM sqlite_master WHERE "
                           "type = 'table' AND name = 'raster_coverages'",
                           &papszResults, &nRowCount,
                           &nColCount, NULL );
    sqlite3_free_table(papszResults);
    if( !(rc == SQLITE_OK && nRowCount == 1) )
    {
        return false;
    }

    papszResults = NULL;
    nRowCount = 0;
    nColCount = 0;
    rc = sqlite3_get_table( hDB,
                           "SELECT coverage_name, title, abstract "
                           "FROM raster_coverages",
                           &papszResults, &nRowCount,
                           &nColCount, NULL );
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
        if( pszCoverageName != NULL )
        {
            rl2CoveragePtr cvg = rl2_create_coverage_from_dbms( hDB, 
                                                            pszCoverageName );
            if( cvg != NULL )
            {
                const int nIdx = m_aosSubDatasets.size() / 2 + 1;
                m_aosSubDatasets.AddNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", nIdx),
                    CPLSPrintf("RASTERLITE2:%s:%s",
                        EscapeNameAndQuoteIfNeeded(m_pszFilename).c_str(),
                        EscapeNameAndQuoteIfNeeded(pszCoverageName).c_str()));
                CPLString osDesc("Coverage ");
                osDesc += pszCoverageName;
                if( pszTitle != NULL && pszTitle[0] != '\0' &&
                    !EQUAL(pszTitle, "*** missing Title ***") )
                {
                    osDesc += ", title = ";
                    osDesc += pszTitle;
                }
                if( pszAbstract != NULL && pszAbstract[0] != '\0' &&
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
        return OpenRasterSubDataset(
                    m_aosSubDatasets.FetchNameValue( "SUBDATASET_1_NAME" ));
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

    m_osCoverageName = OGRSQLiteParamsUnquote( papszTokens[2] );
    m_nSectionId =
        (CSLCount(papszTokens) >= 4) ? CPLAtoGIntBig( papszTokens[3] ) : -1;

    CSLDestroy(papszTokens);

    m_pRL2Coverage = rl2_create_coverage_from_dbms( hDB, 
                                                    m_osCoverageName );
    if( m_pRL2Coverage == NULL )
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
        char** papszResults2 = NULL;
        char* pszSQL = sqlite3_mprintf(
                "SELECT section_id, section_name FROM \"%w\" "
                "ORDER BY section_id",
                osSectionTableName.c_str());
        int rc = sqlite3_get_table( hDB,
                pszSQL,
                &papszResults2, &nRowCount2,
                &nColCount2, NULL );
        sqlite3_free(pszSQL);
        if( rc == SQLITE_OK )
        {
            for( int j=0; j<nRowCount2; ++j )
            {
                const char * const* papszRow2 = papszResults2 + j * 2 + 2;
                const char* pszSectionId = papszRow2[0];
                const char* pszSectionName = papszRow2[1];
                if( pszSectionName != NULL && pszSectionId != NULL )
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
        int ret = rl2_resolve_base_resolution_from_dbms( hDB,
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
            "Lower(coverage_name) = Lower('%q')", m_osCoverageName.c_str() );
        char** papszResults = NULL;
        int nRowCount = 0;
        int nColCount = 0;
        int rc = sqlite3_get_table( hDB, pszSQL,
                                    &papszResults, &nRowCount,
                                    &nColCount, NULL );
        sqlite3_free( pszSQL );
        if( rc == SQLITE_OK )
        {
            if( nRowCount ==  1 )
            {
                const char* pszMinX = papszResults[4 + 0];
                const char* pszMinY = papszResults[4 + 1];
                const char* pszMaxX = papszResults[4 + 2];
                const char* pszMaxY = papszResults[4 + 3];
                if( pszMinX != NULL && pszMinY != NULL && pszMaxX != NULL &&
                    pszMaxY != NULL )
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
        if( poSRS != NULL )
        {
            OGRSpatialReference oSRS(*poSRS);
            char* pszWKT = NULL;
            if( oSRS.EPSGTreatsAsLatLong() ||
                oSRS.EPSGTreatsAsNorthingEasting() )
            {
                oSRS.GetRoot()->StripNodes( "AXIS" );
            }
            if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
            {
                m_osProjection = pszWKT;
                CPLFree(pszWKT);
            }
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
    const char* pszCompression = NULL;
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
        case RL2_COMPRESSION_CHARLS:
            pszCompression = "JPEG_LOSSLESS";
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

    if( pszCompression != NULL )
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
    if( noDataPtr != NULL )
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
            rl2_create_raster_statistics_from_dbms( hDB, m_osCoverageName );
        if( pStatistics != NULL )
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
        "Lower(coverage_name) = Lower('%q')", m_osCoverageName.c_str() );
    char** papszResults = NULL;
    int nRowCount = 0;
    int nColCount = 0;
    int rc = sqlite3_get_table( hDB, pszSQL,
                                &papszResults, &nRowCount,
                                &nColCount, NULL );
    sqlite3_free( pszSQL );
    if( rc == SQLITE_OK )
    {
        if( nRowCount ==  1 )
        {
            const char* pszTitle = papszResults[2 + 0];
            const char* pszAbstract = papszResults[2 + 1];
            if( pszTitle != NULL && pszTitle[0] != '\0' &&
                !EQUAL(pszTitle, "*** missing Title ***") )
            {
                GDALDataset::SetMetadataItem( "COVERAGE_TITLE", pszTitle );
            }
            if( pszAbstract != NULL && pszAbstract[0] != '\0' &&
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
        papszResults = NULL;
        nRowCount = 0;
        nColCount = 0;
        pszSQL = sqlite3_mprintf(
            "SELECT summary FROM \"%w\" WHERE "
            "section_id = %d",
            CPLSPrintf( "%s_sections", m_osCoverageName.c_str() ),
            static_cast<int>(m_nSectionId) );
        rc = sqlite3_get_table( hDB, pszSQL,
                                    &papszResults, &nRowCount,
                                    &nColCount, NULL );
        sqlite3_free( pszSQL );
        if( rc == SQLITE_OK )
        {
            if( nRowCount ==  1 )
            {
                const char* pszSummary = papszResults[1 + 0];
                if( pszSummary != NULL && pszSummary[0] != '\0' )
                {
                    GDALDataset::SetMetadataItem( "SECTION_SUMMARY",
                                                  pszSummary );
                }
            }
            sqlite3_free_table(papszResults);
        }
    }

    // Instanciate overviews
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
    if( !nMixedResolutions || m_nSectionId >= 0 )
    {
        if( !nMixedResolutions )
        {
            pszSQL = sqlite3_mprintf(
                "SELECT x_resolution_1_1, y_resolution_1_1, "
                "x_resolution_1_2, y_resolution_1_2, "
                "x_resolution_1_4, y_resolution_1_4,"
                "x_resolution_1_8, y_resolution_1_8 "
                "FROM \"%w\" ORDER BY pyramid_level",
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
                "ORDER BY pyramid_level",
                CPLSPrintf( "%s_section_levels", m_osCoverageName.c_str() ),
                static_cast<int>(m_nSectionId) );
        }
        papszResults = NULL;
        nRowCount = 0;
        nColCount = 0;
        char* pszErrMsg = NULL;
        rc = sqlite3_get_table( hDB, pszSQL,
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
                if( pszXRes1 != NULL && pszYRes1 != NULL )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes1),
                                                      CPLAtof(pszYRes1) );
                }
                if( pszXRes2 != NULL && pszYRes2 != NULL )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes2),
                                                      CPLAtof(pszYRes2) );
                }
                if( pszXRes4 != NULL && pszYRes4 != NULL )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes4),
                                                      CPLAtof(pszYRes4) );
                }
                if( pszXRes8 != NULL && pszYRes8 != NULL )
                {
                    CreateRL2OverviewDatasetIfNeeded( CPLAtof(pszXRes8),
                                                      CPLAtof(pszYRes8) );
                }
            }
            sqlite3_free_table(papszResults);
        }
    }

    return true;
#else // !defined(HAVE_RASTERLITE2)
    return false;
#endif // HAVE_RASTERLITE2
}

#ifdef HAVE_RASTERLITE2

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
    if( poOvrDS->nRasterXSize <= 1 || poOvrDS->nRasterXSize <= 1 ||
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
    m_poCT( NULL )
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
    m_poCT = NULL;
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
    if( m_poCT == NULL && m_eColorInterp == GCI_PaletteIndex )
    {
        rl2PalettePtr palettePtr =
            rl2_get_dbms_palette( poGDS->GetDB(),
                            rl2_get_coverage_name(poGDS->GetRL2CoveragePtr()) );
        if( palettePtr )
        {
            m_poCT = new GDALColorTable();
            unsigned short nEntries = 0;
            unsigned char* pabyR = NULL;
            unsigned char* pabyG = NULL;
            unsigned char* pabyB = NULL;
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
            return NULL;
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
    unsigned char* pBuffer = NULL;
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
        (poGDS->IsRL2MixedResolutions() || poGDS->GetParentDS() == NULL) )
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
                                                NULL, // palette
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
                                                NULL, // palette
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
        !poGDS->HasPromote1BitAS8Bit() && poGDS->GetParentDS() != NULL )
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
            if( poBlock != NULL )
            {
                poBlock->DropLock();
                continue;
            }
            poBlock = reinterpret_cast<RL2RasterBand*>(
                poGDS->GetRasterBand(iBand))->
                    GetLockedBlockRef( nBlockXOff, nBlockYOff, TRUE );
            if( poBlock == NULL )
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

#endif // HAVE_RASTERLITE2

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char** OGRSQLiteDataSource::GetMetadata(const char* pszDomain)
{
    if( pszDomain != NULL && EQUAL( pszDomain, "SUBDATASETS" ) &&
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

const char* OGRSQLiteDataSource::GetProjectionRef()
{
    if( !m_osProjection.empty() )
        return m_osProjection.c_str();
    return GDALPamDataset::GetProjectionRef();
}
