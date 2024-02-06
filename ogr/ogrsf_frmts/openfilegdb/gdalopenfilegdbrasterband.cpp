/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB raster driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_conv.h"
#include "cpl_minixml.h"

#include "ogr_openfilegdb.h"

#include "gdal_rat.h"
#include "filegdbtable_priv.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <new>
#include <utility>

using namespace OpenFileGDB;

/***********************************************************************/
/*                         OpenRaster()                                */
/***********************************************************************/

bool OGROpenFileGDBDataSource::OpenRaster(const GDALOpenInfo *poOpenInfo,
                                          const std::string &osLayerName,
                                          const std::string &osDefinition,
                                          const std::string &osDocumentation)
{
    m_osRasterLayerName = osLayerName;

    const std::string osBndTableName(
        std::string("fras_bnd_").append(osLayerName).c_str());
    const auto oIter = m_osMapNameToIdx.find(osBndTableName);
    if (oIter == m_osMapNameToIdx.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find table %s",
                 osBndTableName.c_str());
        return false;
    }
    const int nBndIdx = oIter->second;

    FileGDBTable oTable;

    const CPLString osBndFilename(CPLFormFilename(
        m_osDirName, CPLSPrintf("a%08x.gdbtable", nBndIdx), nullptr));
    if (!oTable.Open(osBndFilename, false))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open table %s",
                 osBndTableName.c_str());
        return false;
    }

    const int i_rasterband_id = oTable.GetFieldIdx("rasterband_id");
    const int i_sequence_nbr = oTable.GetFieldIdx("sequence_nbr");
    const int i_raster_id = oTable.GetFieldIdx("raster_id");
    const int i_band_width = oTable.GetFieldIdx("band_width");
    const int i_band_height = oTable.GetFieldIdx("band_height");
    const int i_band_types = oTable.GetFieldIdx("band_types");
    const int i_block_width = oTable.GetFieldIdx("block_width");
    const int i_block_height = oTable.GetFieldIdx("block_height");
    const int i_block_origin_x = oTable.GetFieldIdx("block_origin_x");
    const int i_block_origin_y = oTable.GetFieldIdx("block_origin_y");
    const int i_eminx = oTable.GetFieldIdx("eminx");
    const int i_eminy = oTable.GetFieldIdx("eminy");
    const int i_emaxx = oTable.GetFieldIdx("emaxx");
    const int i_emaxy = oTable.GetFieldIdx("emaxy");
    const int i_srid = oTable.GetFieldIdx("srid");
    if (i_rasterband_id < 0 || i_sequence_nbr < 0 || i_raster_id < 0 ||
        i_band_width < 0 || i_band_height < 0 || i_band_types < 0 ||
        i_block_width < 0 || i_block_height < 0 || i_block_origin_x < 0 ||
        i_block_origin_y < 0 || i_eminx < 0 || i_eminy < 0 || i_emaxx < 0 ||
        i_emaxy < 0 || i_srid < 0 ||
        oTable.GetField(i_rasterband_id)->GetType() != FGFT_OBJECTID ||
        oTable.GetField(i_sequence_nbr)->GetType() != FGFT_INT32 ||
        oTable.GetField(i_raster_id)->GetType() != FGFT_INT32 ||
        oTable.GetField(i_band_width)->GetType() != FGFT_INT32 ||
        oTable.GetField(i_band_height)->GetType() != FGFT_INT32 ||
        oTable.GetField(i_band_types)->GetType() != FGFT_INT32 ||
        oTable.GetField(i_block_width)->GetType() != FGFT_INT32 ||
        oTable.GetField(i_block_height)->GetType() != FGFT_INT32 ||
        oTable.GetField(i_block_origin_x)->GetType() != FGFT_FLOAT64 ||
        oTable.GetField(i_block_origin_y)->GetType() != FGFT_FLOAT64 ||
        oTable.GetField(i_eminx)->GetType() != FGFT_FLOAT64 ||
        oTable.GetField(i_eminy)->GetType() != FGFT_FLOAT64 ||
        oTable.GetField(i_emaxx)->GetType() != FGFT_FLOAT64 ||
        oTable.GetField(i_emaxy)->GetType() != FGFT_FLOAT64 ||
        oTable.GetField(i_srid)->GetType() != FGFT_INT32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong structure for %s table",
                 osBndTableName.c_str());
        return false;
    }

    int iRow = 0;
    while (iRow < oTable.GetTotalRecordCount() &&
           (iRow = oTable.GetAndSelectNextNonEmptyRow(iRow)) >= 0)
    {
        auto psField = oTable.GetFieldValue(i_raster_id);
        if (!psField)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read field %s in %s table", "raster_id",
                     osBndTableName.c_str());
            return false;
        }
        if (psField->Integer != 1)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Raster with raster_id = %d (!= 1) ignored",
                     psField->Integer);
            continue;
        }

        const int nGDBRasterBandId = iRow + 1;

        psField = oTable.GetFieldValue(i_sequence_nbr);
        if (!psField)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read field %s in %s table", "sequence_nbr",
                     osBndTableName.c_str());
            return false;
        }
        const int nSequenceNr = psField->Integer;

        m_oMapGDALBandToGDBBandId[nSequenceNr] = nGDBRasterBandId;

        ++iRow;
    }

    if (m_oMapGDALBandToGDBBandId.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot read record in %s table",
                 osBndTableName.c_str());
        return false;
    }

    auto psField = oTable.GetFieldValue(i_band_width);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "band_width",
                 osBndTableName.c_str());
        return false;
    }
    int nWidth = psField->Integer;

    psField = oTable.GetFieldValue(i_band_height);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "band_height",
                 osBndTableName.c_str());
        return false;
    }
    int nHeight = psField->Integer;

    const int l_nBands = static_cast<int>(m_oMapGDALBandToGDBBandId.size());
    if (!GDALCheckDatasetDimensions(nWidth, nHeight) ||
        !GDALCheckBandCount(l_nBands, /*bIsZeroAllowed=*/false))
    {
        return false;
    }

    psField = oTable.GetFieldValue(i_block_width);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "block_width",
                 osBndTableName.c_str());
        return false;
    }
    const int nBlockWidth = psField->Integer;

    // 32768 somewhat arbitrary
    if (nBlockWidth <= 0 || nBlockWidth > 32768)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s in %s table",
                 "block_width", osBndTableName.c_str());
        return false;
    }

    psField = oTable.GetFieldValue(i_block_height);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "block_height",
                 osBndTableName.c_str());
        return false;
    }
    const int nBlockHeight = psField->Integer;

    // 32768 somewhat arbitrary
    if (nBlockHeight <= 0 || nBlockHeight > 32768)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s in %s table",
                 "block_height", osBndTableName.c_str());
        return false;
    }

    psField = oTable.GetFieldValue(i_band_types);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "band_types",
                 osBndTableName.c_str());
        return false;
    }
    const int nBandTypes = psField->Integer;

    psField = oTable.GetFieldValue(i_eminx);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "eminx",
                 osBndTableName.c_str());
        return false;
    }
    const double dfMinX = psField->Real;

    psField = oTable.GetFieldValue(i_eminy);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "eminy",
                 osBndTableName.c_str());
        return false;
    }
    const double dfMinY = psField->Real;

    psField = oTable.GetFieldValue(i_emaxx);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "emaxx",
                 osBndTableName.c_str());
        return false;
    }
    const double dfMaxX = psField->Real;

    psField = oTable.GetFieldValue(i_emaxy);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "emaxy",
                 osBndTableName.c_str());
        return false;
    }
    const double dfMaxY = psField->Real;

    psField = oTable.GetFieldValue(i_block_origin_x);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "block_origin_x",
                 osBndTableName.c_str());
        return false;
    }
    const double dfBlockOriginX = psField->Real;

    psField = oTable.GetFieldValue(i_block_origin_y);
    if (!psField)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "block_origin_y",
                 osBndTableName.c_str());
        return false;
    }
    const double dfBlockOriginY = psField->Real;

    // Figure out data type
    GDALDataType eDT = GDT_Byte;
    const int nBitWidth = (nBandTypes >> 19) & ((1 << 7) - 1);
    const int nBitType = (nBandTypes >> 16) & ((1 << 2) - 1);
    constexpr int IS_UNSIGNED = 0;
    constexpr int IS_SIGNED = 1;
    constexpr int IS_FLOATING_POINT = 2;
    if ((nBitWidth >= 1 && nBitWidth < 8) && nBitType == IS_UNSIGNED)
    {
        eDT = GDT_Byte;
    }
    else if (nBitWidth == 8 && nBitType <= IS_SIGNED)
    {
        eDT = nBitType == IS_SIGNED ? GDT_Int8 : GDT_Byte;
    }
    else if (nBitWidth == 16 && nBitType <= IS_SIGNED)
    {
        eDT = nBitType == IS_SIGNED ? GDT_Int16 : GDT_UInt16;
    }
    else if (nBitWidth == 32 && nBitType <= IS_FLOATING_POINT)
    {
        eDT = nBitType == IS_FLOATING_POINT ? GDT_Float32
              : nBitType == IS_SIGNED       ? GDT_Int32
                                            : GDT_UInt32;
    }
    else if (nBitWidth == 64 && nBitType == 0)
    {
        eDT = GDT_Float64;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled nBitWidth=%d, nBitType=%d in %s table", nBitWidth,
                 nBitType, osBndTableName.c_str());
        return false;
    }

    // To avoid potential integer overflows in IReadBlock()
    if (nBlockWidth * nBlockHeight >
        std::numeric_limits<int>::max() / nBitWidth)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too large block %dx%d in %s table", nBlockWidth, nBlockHeight,
                 osBndTableName.c_str());
        return false;
    }

    // Figure out compression
    const int nCompression = (nBandTypes >> 8) & 0xff;
    switch (nCompression)
    {
        case 0:
            m_eRasterCompression = Compression::NONE;
            break;
        case 4:
            m_eRasterCompression = Compression::LZ77;
            SetMetadataItem("COMPRESSION", "DEFLATE", "IMAGE_STRUCTURE");
            break;
        case 8:
            m_eRasterCompression = Compression::JPEG;
            SetMetadataItem("COMPRESSION", "JPEG", "IMAGE_STRUCTURE");
            break;
        case 12:
            m_eRasterCompression = Compression::JPEG2000;
            SetMetadataItem("COMPRESSION", "JPEG2000", "IMAGE_STRUCTURE");
            break;
        default:
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unhandled compression %d in %s table", nCompression,
                     osBndTableName.c_str());
            return false;
        }
    }

    // Figure out geotransform

    if (!(dfMaxX > dfMinX && dfMaxY > dfMinY))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "!(dfMaxX > dfMinX && dfMaxY > dfMinY)");
        return false;
    }
    else if (nWidth == 1 || nHeight == 1)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "nWidth == 1 || nHeight == 1: cannot determine geotransform");
    }
    else
    {
        // FileGDB uses a center-of-pixel convention for georeferencing
        // Transform to GDAL's corner-of-pixel convention.
        const double dfResX = (dfMaxX - dfMinX) / (nWidth - 1);
        const double dfResY = (dfMaxY - dfMinY) / (nHeight - 1);
        m_bHasGeoTransform = true;
        const double dfBlockGeorefWidth = dfResX * nBlockWidth;
        if (dfMinX != dfBlockOriginX)
        {
            // Take into account MinX by making sure the raster origin is
            // close to it, while being shifted from an integer number of blocks
            // from BlockOriginX
            const double dfTmp =
                std::floor((dfMinX - dfBlockOriginX) / dfBlockGeorefWidth);
            if (std::fabs(dfTmp) > std::numeric_limits<int>::max())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Inconsistent eminx=%g and block_origin_x=%g", dfMinX,
                         dfBlockOriginX);
                return false;
            }
            m_nShiftBlockX = static_cast<int>(dfTmp);
            CPLDebug("OpenFileGDB", "m_nShiftBlockX = %d", m_nShiftBlockX);
            const double dfMinXAdjusted =
                dfBlockOriginX + m_nShiftBlockX * dfBlockGeorefWidth;
            nWidth = 1 + static_cast<int>(
                             std::round((dfMaxX - dfMinXAdjusted) / dfResX));
        }
        m_adfGeoTransform[0] =
            (dfBlockOriginX + m_nShiftBlockX * dfBlockGeorefWidth) - dfResX / 2;
        m_adfGeoTransform[1] = dfResX;
        m_adfGeoTransform[2] = 0.0;
        const double dfBlockGeorefHeight = dfResY * nBlockHeight;
        if (dfMaxY != dfBlockOriginY)
        {
            // Take into account MaxY by making sure the raster origin is
            // close to it, while being shifted from an integer number of blocks
            // from BlockOriginY
            const double dfTmp =
                std::floor((dfBlockOriginY - dfMaxY) / dfBlockGeorefHeight);
            if (std::fabs(dfTmp) > std::numeric_limits<int>::max())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Inconsistent emaxy=%g and block_origin_y=%g", dfMaxY,
                         dfBlockOriginY);
                return false;
            }
            m_nShiftBlockY = static_cast<int>(dfTmp);
            CPLDebug("OpenFileGDB", "m_nShiftBlockY = %d", m_nShiftBlockY);
            const double dfMaxYAdjusted =
                dfBlockOriginY - m_nShiftBlockY * dfBlockGeorefHeight;
            nHeight = 1 + static_cast<int>(
                              std::round((dfMaxYAdjusted - dfMinY) / dfResY));
        }
        m_adfGeoTransform[3] =
            (dfBlockOriginY - m_nShiftBlockY * dfBlockGeorefHeight) +
            dfResY / 2;
        m_adfGeoTransform[4] = 0.0;
        m_adfGeoTransform[5] = -dfResY;
    }

    // Two cases:
    // - osDefinition is empty (that is FileGDB v9): find the SRS by looking
    //   at the SRS attached to the RASTER field definition of the .gdbtable
    //   file of the main table of the raster (that is the one without fras_XXX
    //   prefixes)
    // - or osDefinition is not empty (that is FileGDB v10): get SRID from the
    //   "srid" field of the _fras_bnd table, and use that has the key to
    //   lookup the corresponding WKT from the GDBSpatialRefs table.
    //   In some cases srid might be 0 (invalid), then we try to get it from
    //   Definition column of the GDB_Items table, stored in osDefinition
    psField = oTable.GetFieldValue(i_srid);
    if (osDefinition.empty())
    {
        // osDefinition empty for FileGDB v9
        const auto oIter2 = m_osMapNameToIdx.find(osLayerName);
        if (oIter2 != m_osMapNameToIdx.end())
        {
            const int nTableIdx = oIter2->second;

            FileGDBTable oTableMain;

            const CPLString osTableMain(CPLFormFilename(
                m_osDirName, CPLSPrintf("a%08x.gdbtable", nTableIdx), nullptr));
            if (oTableMain.Open(osTableMain, false))
            {
                const int iRasterFieldIdx = oTableMain.GetFieldIdx("RASTER");
                if (iRasterFieldIdx >= 0)
                {
                    const auto poField = oTableMain.GetField(iRasterFieldIdx);
                    if (poField->GetType() == FGFT_RASTER)
                    {
                        const auto poFieldRaster =
                            static_cast<FileGDBRasterField *>(poField);
                        const auto &osWKT = poFieldRaster->GetWKT();
                        if (!osWKT.empty() && osWKT[0] != '{')
                        {
                            auto poSRS = BuildSRS(osWKT.c_str());
                            if (poSRS)
                            {
                                m_oRasterSRS = *poSRS;
                                poSRS->Release();
                            }
                        }
                    }
                }
            }
        }
    }
    else if (!psField)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Cannot read field %s in %s table", "srid",
                 osBndTableName.c_str());
    }
    else if (m_osGDBSpatialRefsFilename.empty())
    {
        CPLError(CE_Warning, CPLE_AppDefined, "No GDBSpatialRefs table");
    }
    else
    {
        // FileGDB v10 case
        const int nSRID = psField->Integer;
        FileGDBTable oTableSRS;
        if (oTableSRS.Open(m_osGDBSpatialRefsFilename.c_str(), false))
        {
            const int iSRTEXT = oTableSRS.GetFieldIdx("SRTEXT");
            if (iSRTEXT < 0 ||
                oTableSRS.GetField(iSRTEXT)->GetType() != FGFT_STRING)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Could not find field %s in table %s", "SRTEXT",
                         oTableSRS.GetFilename().c_str());
            }
            else if (nSRID == 0)
            {
                // BldgHeights.gdb is such. We must fetch the SRS from the
                // Definition column of the GDB_Items table
                CPLXMLTreeCloser psTree(
                    CPLParseXMLString(osDefinition.c_str()));
                if (psTree == nullptr)
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Cannot parse XML definition. SRS will be missing");
                }
                else
                {
                    CPLStripXMLNamespace(psTree.get(), nullptr, TRUE);
                    const CPLXMLNode *psInfo =
                        CPLSearchXMLNode(psTree.get(), "=DERasterDataset");
                    if (psInfo)
                    {
                        auto poSRS = BuildSRS(psInfo);
                        if (poSRS)
                            m_oRasterSRS = *poSRS;
                    }
                    if (m_oRasterSRS.IsEmpty())
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot get SRS from XML definition");
                    }
                }
            }
            else if (nSRID < 0 || !oTableSRS.SelectRow(nSRID - 1) ||
                     oTableSRS.HasGotError())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot find record corresponding to SRID = %d",
                         nSRID);
            }
            else
            {
                const auto psSRTEXT = oTableSRS.GetFieldValue(iSRTEXT);
                if (psSRTEXT && psSRTEXT->String)
                {
                    if (psSRTEXT->String[0] != '{')
                    {
                        auto poSRS = BuildSRS(psSRTEXT->String);
                        if (poSRS)
                        {
                            m_oRasterSRS = *poSRS;
                            poSRS->Release();
                        }
                    }
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot get SRTEXT corresponding to SRID = %d",
                             nSRID);
                }
            }
        }
    }

    // Open the fras_blk_XXX table, which contains pixel data, as a OGR layer
    const std::string osBlkTableName(
        std::string("fras_blk_").append(osLayerName).c_str());
    m_poBlkLayer = BuildLayerFromName(osBlkTableName.c_str());
    if (!m_poBlkLayer)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find table %s",
                 osBlkTableName.c_str());
        return false;
    }
    auto poFDefn = m_poBlkLayer->GetLayerDefn();
    if (poFDefn->GetFieldIndex("rasterband_id") < 0 ||
        poFDefn->GetFieldIndex("rrd_factor") < 0 ||
        poFDefn->GetFieldIndex("row_nbr") < 0 ||
        poFDefn->GetFieldIndex("col_nbr") < 0 ||
        poFDefn->GetFieldIndex("block_data") < 0 ||
        poFDefn->GetFieldIndex("block_key") < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong structure for %s table",
                 osBlkTableName.c_str());
        return false;
    }

    nRasterXSize = nWidth;
    nRasterYSize = nHeight;

    if (m_oMapGDALBandToGDBBandId.size() > 1)
    {
        SetMetadataItem("INTERLEAVE", "BAND", "IMAGE_STRUCTURE");
    }

    // Figure out number of overviews by looking at the biggest block_key
    // (should only involve looking in the corresponding index).
    int nOverviewCount = 0;
    CPLString osSQL;
    osSQL.Printf("SELECT MAX(block_key) FROM \"%s\"", osBlkTableName.c_str());
    auto poSQLLyr = ExecuteSQL(osSQL.c_str(), nullptr, nullptr);
    if (poSQLLyr)
    {
        auto poFeat = std::unique_ptr<OGRFeature>(poSQLLyr->GetNextFeature());
        if (poFeat)
        {
            const char *pszMaxKey = poFeat->GetFieldAsString(0);
            if (strlen(pszMaxKey) == strlen("0000BANDOVYYYYXXXX    ") ||
                strlen(pszMaxKey) == strlen("0000BANDOV-YYYYXXXX    ") ||
                strlen(pszMaxKey) == strlen("0000BANDOVYYYY-XXXX    ") ||
                strlen(pszMaxKey) == strlen("0000BANDOV-YYYY-XXXX    "))
            {
                char szHex[3] = {0};
                memcpy(szHex, pszMaxKey + 8, 2);
                unsigned nMaxRRD = 0;
                sscanf(szHex, "%02X", &nMaxRRD);
                nOverviewCount =
                    static_cast<int>(std::min<unsigned>(31, nMaxRRD));
            }
        }
        ReleaseResultSet(poSQLLyr);
    }

    if (m_eRasterCompression == Compression::JPEG)
    {
        GuessJPEGQuality(nOverviewCount);
    }

    // It seems that the top left corner of overviews is registered against
    // (eminx, emaxy), contrary to the full resolution layer which is registered
    // against (block_origin_x, block_origin_y).
    // At least, that's what was observed on the dataset
    // ftp://ftp.gisdata.mn.gov/pub/gdrs/data/pub/us_mn_state_dnr/water_lake_bathymetry/fgdb_water_lake_bathymetry.zip
    if ((dfBlockOriginX != dfMinX || dfBlockOriginY != dfMaxY) &&
        nOverviewCount > 0)
    {
        CPLDebug("OpenFileGDB",
                 "Ignoring overviews as block origin != (minx, maxy)");
        nOverviewCount = 0;
    }

    // Create raster bands

    // Create mask band of full resolution, if we don't assign a nodata value
    std::unique_ptr<GDALOpenFileGDBRasterBand> poMaskBand;

    // Default "nodata" padding in areas whose validity mask is 0 ?
    // Not reliable on integer data types.
    // Byte -> 0
    // Int8 -> -128 ?
    // Int16 -> 32767
    // UInt16 -> 0
    // (u)int10 -> 65535
    // (u)int12 -> 65535
    // Int32 -> 2147483647
    // UInt32 -> 2147483647
    // Float32 -> 3.4e+38
    // Float64 -> 1.79e+308

    bool bHasNoData = false;
    double dfNoData = 0.0;
    const char *pszNoDataOrMask = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "NODATA_OR_MASK", "AUTO");
    if (EQUAL(pszNoDataOrMask, "AUTO"))
    {
        // In AUTO mode, we only set nodata for Float32/Float64
        // For other data types, report a mask band.
        if (eDT == GDT_Float32)
        {
            bHasNoData = true;
            dfNoData = static_cast<double>(static_cast<float>(3.4e+38));
        }
        else if (eDT == GDT_Float64)
        {
            bHasNoData = true;
            dfNoData = 1.79e+308;
        }
        else
        {
            poMaskBand = std::make_unique<GDALOpenFileGDBRasterBand>(
                this, 1, GDT_Byte, 8, nBlockWidth, nBlockHeight, 0, true);
        }
    }
    else if (EQUAL(pszNoDataOrMask, "MASK"))
    {
        poMaskBand = std::make_unique<GDALOpenFileGDBRasterBand>(
            this, 1, GDT_Byte, 8, nBlockWidth, nBlockHeight, 0, true);
    }
    else if (!EQUAL(pszNoDataOrMask, "NONE"))
    {
        dfNoData = CPLAtof(pszNoDataOrMask);
        if (eDT == GDT_Float64)
        {
            bHasNoData = true;
        }
        else if (eDT == GDT_Float32)
        {
            if (std::fabs(dfNoData) > std::numeric_limits<float>::max())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid nodata value %.18g for Float32", dfNoData);
                return false;
            }
            bHasNoData = true;
        }
        else if (GDALDataTypeIsInteger(eDT))
        {
            double dfMin = 0, dfMax = 0;
            switch (eDT)
            {
                case GDT_Int8:
                    dfMin = std::numeric_limits<int8_t>::min();
                    dfMax = std::numeric_limits<int8_t>::max();
                    break;
                case GDT_Byte:
                    dfMin = std::numeric_limits<uint8_t>::min();
                    dfMax = std::numeric_limits<uint8_t>::max();
                    break;
                case GDT_Int16:
                    dfMin = std::numeric_limits<int16_t>::min();
                    dfMax = std::numeric_limits<int16_t>::max();
                    break;
                case GDT_UInt16:
                    dfMin = std::numeric_limits<uint16_t>::min();
                    dfMax = std::numeric_limits<uint16_t>::max();
                    break;
                case GDT_Int32:
                    dfMin = std::numeric_limits<int32_t>::min();
                    dfMax = std::numeric_limits<int32_t>::max();
                    break;
                case GDT_UInt32:
                    dfMin = std::numeric_limits<uint32_t>::min();
                    dfMax = std::numeric_limits<uint32_t>::max();
                    break;
                default:
                    CPLAssert(false);
                    return false;
            }
            if (!std::isfinite(dfNoData) || dfNoData < dfMin ||
                dfNoData > dfMax ||
                dfNoData != static_cast<double>(static_cast<int64_t>(dfNoData)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid nodata value %.18g for %s", dfNoData,
                         GDALGetDataTypeName(eDT));
                return false;
            }
            bHasNoData = true;
        }
    }

    GDALOpenFileGDBRasterBand *poMaskBandRef = poMaskBand.get();

    for (int iBand = 1; iBand <= l_nBands; ++iBand)
    {
        auto poBand = new GDALOpenFileGDBRasterBand(
            this, iBand, eDT, nBitWidth, nBlockWidth, nBlockHeight, 0, false);
        if (poMaskBandRef)
        {
            if (iBand == 1)
            {
                // Make the mask band owned by the first raster band
                poBand->m_poMaskBandOwned = std::move(poMaskBand);
                poMaskBandRef = poBand->m_poMaskBandOwned.get();
                poMaskBandRef->m_poMainBand = poBand;
            }
            poBand->m_poMaskBand = poMaskBandRef;
        }
        else if (bHasNoData)
        {
            poBand->m_dfNoData = dfNoData;
            poBand->m_bHasNoData = true;
        }

        // Create overview bands
        for (int iOvr = 0; iOvr < nOverviewCount; ++iOvr)
        {
            auto poOvrBand = std::make_unique<GDALOpenFileGDBRasterBand>(
                this, iBand, eDT, nBitWidth, nBlockWidth, nBlockHeight,
                iOvr + 1, false);
            if (poBand->m_bHasNoData)
            {
                poOvrBand->m_dfNoData = dfNoData;
                poOvrBand->m_bHasNoData = true;
            }
            poBand->m_apoOverviewBands.emplace_back(std::move(poOvrBand));
        }

        SetBand(iBand, poBand);
    }

    // Create mask band of overview bands
    if (poMaskBandRef)
    {
        for (int iOvr = 0; iOvr < nOverviewCount; ++iOvr)
        {
            for (int iBand = 1; iBand <= l_nBands; ++iBand)
            {
                auto poOvrBand = cpl::down_cast<GDALOpenFileGDBRasterBand *>(
                                     GetRasterBand(iBand))
                                     ->m_apoOverviewBands[iOvr]
                                     .get();
                if (iBand == 1)
                {
                    // Make the mask band owned by the first raster band
                    poOvrBand->m_poMaskBandOwned =
                        std::make_unique<GDALOpenFileGDBRasterBand>(
                            this, 1, GDT_Byte, 8, nBlockWidth, nBlockHeight,
                            iOvr + 1, true);
                    poMaskBandRef = poOvrBand->m_poMaskBandOwned.get();
                    poMaskBandRef->m_poMainBand = poOvrBand;
                }
                poOvrBand->m_poMaskBand = poMaskBandRef;
            }
        }
    }

    ReadAuxTable(osLayerName);

    SetMetadataItem("RASTER_DATASET", m_osRasterLayerName.c_str());

    if (!osDefinition.empty())
    {
        const char *const apszMD[] = {osDefinition.c_str(), nullptr};
        SetMetadata(const_cast<char **>(apszMD), "xml:definition");
    }

    if (!osDocumentation.empty())
    {
        const char *const apszMD[] = {osDocumentation.c_str(), nullptr};
        SetMetadata(const_cast<char **>(apszMD), "xml:documentation");
    }

    // We are all fine after all those preliminary checks and setups !
    return true;
}

/************************************************************************/
/*                       GuessJPEGQuality()                             */
/************************************************************************/

void OGROpenFileGDBDataSource::GuessJPEGQuality(int nOverviewCount)
{
    // For JPEG, fetch JPEG_QUALITY from the data of the smallest overview level
    CPLString osFilter;
    osFilter.Printf("block_key = '0000%04X%02X%04X%04X'",
                    1,  // band
                    nOverviewCount,
                    0,  // nBlockYOff
                    0   // nBlockXOff
    );

    CPLAssert(m_poBlkLayer);
    m_poBlkLayer->SetAttributeFilter(osFilter.c_str());
    auto poFeature =
        std::unique_ptr<OGRFeature>(m_poBlkLayer->GetNextFeature());
    if (poFeature)
    {
        const int nFieldIdx = poFeature->GetFieldIndex("block_data");
        CPLAssert(nFieldIdx >= 0);
        if (poFeature->IsFieldSetAndNotNull(nFieldIdx))
        {
            int nInBytes = 0;
            const GByte *pabyData =
                poFeature->GetFieldAsBinary(nFieldIdx, &nInBytes);
            if (nInBytes >= 5)
            {
                uint32_t nJPEGSize = nInBytes - 1;
                uint32_t nJPEGOffset = 1;
                if (pabyData[0] == 0xFE)
                {
                    // JPEG followed by binary mask
                    memcpy(&nJPEGSize, pabyData + 1, sizeof(uint32_t));
                    CPL_LSBPTR32(&nJPEGSize);
                    if (nJPEGSize > static_cast<unsigned>(nInBytes - 5))
                    {
                        nJPEGSize = 0;
                    }
                    nJPEGOffset = 5;
                }
                else if (pabyData[0] != 1)
                {
                    nJPEGSize = 0;
                }
                if (nJPEGSize)
                {
                    CPLString osTmpFilename;
                    osTmpFilename.Printf("/vsimem/_openfilegdb/%p.jpg", this);
                    VSIFCloseL(VSIFileFromMemBuffer(
                        osTmpFilename.c_str(),
                        const_cast<GByte *>(pabyData + nJPEGOffset), nJPEGSize,
                        false));
                    const char *const apszDrivers[] = {"JPEG", nullptr};
                    auto poJPEGDS = std::unique_ptr<GDALDataset>(
                        GDALDataset::Open(osTmpFilename.c_str(), GDAL_OF_RASTER,
                                          apszDrivers));
                    if (poJPEGDS)
                    {
                        const char *pszQuality = poJPEGDS->GetMetadataItem(
                            "JPEG_QUALITY", "IMAGE_STRUCTURE");
                        if (pszQuality)
                        {
                            SetMetadataItem("JPEG_QUALITY", pszQuality,
                                            "IMAGE_STRUCTURE");
                        }
                    }
                    VSIUnlink(osTmpFilename);
                }
            }
        }
    }
}

/************************************************************************/
/*                        ReadAuxTable()                                */
/************************************************************************/

// Record type=9 of table fras_ras_XXXX contains a PropertySet object,
// which may contain statistics
// For example on
// https://listdata.thelist.tas.gov.au/opendata/data/NCH_ES_WATER_LOGGING_HAZARD_STATEWIDE.zip
void OGROpenFileGDBDataSource::ReadAuxTable(const std::string &osLayerName)
{
    const std::string osAuxTableName(
        std::string("fras_aux_").append(osLayerName).c_str());
    auto poLayer = BuildLayerFromName(osAuxTableName.c_str());
    if (!poLayer)
    {
        CPLDebug("OpenFileGDB", "Cannot find table %s", osAuxTableName.c_str());
        return;
    }
    auto poFDefn = poLayer->GetLayerDefn();
    const int iFieldObjectIdx = poFDefn->GetFieldIndex("object");
    if (poFDefn->GetFieldIndex("type") < 0 || iFieldObjectIdx < 0)
    {
        CPLDebug("OpenFileGDB", "Wrong structure for %s table",
                 osAuxTableName.c_str());
        return;
    }
    poLayer->SetAttributeFilter("type = 9");
    auto poFeature = std::unique_ptr<OGRFeature>(poLayer->GetNextFeature());
    if (!poFeature)
        return;
    if (!poFeature->IsFieldSetAndNotNull(iFieldObjectIdx))
        return;
    int nBytes = 0;
    const GByte *pabyData =
        poFeature->GetFieldAsBinary(iFieldObjectIdx, &nBytes);
    if (!pabyData || nBytes == 0)
        return;
    int iOffset = 0;

    const auto ReadString = [pabyData, &iOffset, nBytes](std::string &osStr)
    {
        if (iOffset > nBytes - 4)
            return false;
        int nStrLength;
        memcpy(&nStrLength, pabyData + iOffset, 4);
        CPL_LSBPTR32(&nStrLength);
        iOffset += 4;
        if (nStrLength <= 2 || iOffset > nBytes - nStrLength)
            return false;
        if ((nStrLength % 2) != 0)
            return false;
        // nStrLength / 2 to get the number of characters
        // and - 1 to remove the null terminating one
        osStr = ReadUTF16String(pabyData + iOffset, nStrLength / 2 - 1);
        iOffset += nStrLength;
        return true;
    };

    // pabyData is an ArcObject "PropertySet" object, which is key/value
    // dictionary. This is hard to parse given there are variable-length value
    // whose size is not explicit. So let's use a heuristics by looking for
    // the beginning of a inner PropertySet with band properties that starts
    // with a KIND=BAND key value pair.
    constexpr GByte abyNeedle[] = {
        'K', 0, 'I', 0, 'N', 0, 'D', 0, 0, 0, 8, 0,  // 8 = string
        10,  0, 0,   0,  // number of bytes of following value
        'B', 0, 'A', 0, 'N', 0, 'D', 0, 0, 0};
    constexpr int nNeedleSize = static_cast<int>(sizeof(abyNeedle));

    for (int iBand = 1; iBand <= nBands; ++iBand)
    {
        int iNewOffset = -1;
        for (int i = iOffset; i < nBytes - nNeedleSize; ++i)
        {
            if (pabyData[i] == 'K' &&
                memcmp(pabyData + i, abyNeedle, nNeedleSize) == 0)
            {
                iNewOffset = i + nNeedleSize;
                break;
            }
        }
        if (iNewOffset < 0)
            return;
        iOffset = iNewOffset;

        // Try to read as many key/value pairs as possible
        while (true)
        {
            // Read key
            std::string osKey;
            if (!ReadString(osKey))
                return;

            // Read value type as a short
            uint16_t nValueType;
            if (iOffset > nBytes - 2)
                return;
            memcpy(&nValueType, pabyData + iOffset, 2);
            CPL_LSBPTR16(&nValueType);
            iOffset += 2;

            // Skip over non-string values
            if (nValueType == 0 || nValueType == 1)  // null / empty value
            {
                continue;
            }
            if (nValueType == 2)  // short value
            {
                if (iOffset > nBytes - 2)
                    return;
                iOffset += 2;
                continue;
            }

            if (nValueType == 3 || nValueType == 4)  // int or long value
            {
                if (iOffset > nBytes - 4)
                    return;
                iOffset += 4;
                continue;
            }

            if (nValueType == 5 || nValueType == 7)  // double or date value
            {
                if (iOffset > nBytes - 8)
                    return;
                iOffset += 8;
                continue;
            }

            if (nValueType != 8)  // 8 = string
            {
                // Give up with this band as the value type is not handled,
                // and we can't skip over it.
                break;
            }

            // Read string value
            std::string osValue;
            if (!ReadString(osValue))
                return;

            GetRasterBand(iBand)->SetMetadataItem(osKey.c_str(),
                                                  osValue.c_str());
        }
    }
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr OGROpenFileGDBDataSource::GetGeoTransform(double *padfGeoTransform)
{
    memcpy(padfGeoTransform, m_adfGeoTransform.data(),
           sizeof(m_adfGeoTransform));
    return m_bHasGeoTransform ? CE_None : CE_Failure;
}

/************************************************************************/
/*                         GetSpatialRef()                              */
/************************************************************************/

const OGRSpatialReference *OGROpenFileGDBDataSource::GetSpatialRef() const
{
    return m_oRasterSRS.IsEmpty() ? nullptr : &m_oRasterSRS;
}

/************************************************************************/
/*                     GDALOpenFileGDBRasterBand()                      */
/************************************************************************/

GDALOpenFileGDBRasterBand::GDALOpenFileGDBRasterBand(
    OGROpenFileGDBDataSource *poDSIn, int nBandIn, GDALDataType eDT,
    int nBitWidth, int nBlockWidth, int nBlockHeight, int nOverviewLevel,
    bool bIsMask)
    : m_nBitWidth(nBitWidth), m_nOverviewLevel(nOverviewLevel),
      m_bIsMask(bIsMask)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDT;
    nRasterXSize = std::max(1, poDSIn->GetRasterXSize() >> nOverviewLevel);
    nRasterYSize = std::max(1, poDSIn->GetRasterYSize() >> nOverviewLevel);
    nBlockXSize = nBlockWidth;
    nBlockYSize = nBlockHeight;
    if (nBitWidth < 8)
    {
        SetMetadataItem("NBITS", CPLSPrintf("%d", nBitWidth),
                        "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                         SetNoDataFromMask()                          */
/************************************************************************/

template <class T>
static void SetNoDataFromMask(void *pImage, const GByte *pabyMask,
                              size_t nPixels, double dfNoData)
{
    const T noData = static_cast<T>(dfNoData);
    const T noDataReplacement =
        noData == std::numeric_limits<T>::max() ? noData - 1 : noData + 1;
    bool bHasWarned = false;
    for (size_t i = 0; i < nPixels; ++i)
    {
        if (pabyMask && !(pabyMask[i / 8] & (0x80 >> (i & 7))))
        {
            static_cast<T *>(pImage)[i] = noData;
        }
        else if (static_cast<T *>(pImage)[i] == noData)
        {
            static_cast<T *>(pImage)[i] = noDataReplacement;
            if (!bHasWarned)
            {
                bHasWarned = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Valid data found with value equal to nodata (%.0f). "
                         "Got substituted with %.0f",
                         static_cast<double>(noData),
                         static_cast<double>(noDataReplacement));
            }
        }
    }
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr GDALOpenFileGDBRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                             void *pImage)
{
    auto poGDS = cpl::down_cast<OGROpenFileGDBDataSource *>(poDS);
    auto &poLyr = poGDS->m_poBlkLayer;

    // Return (pointer to image data, owner block). Works when called from main band
    // or mask band. owner block must be DropLock() once done (if not null)
    const auto GetImageData = [this, nBlockXOff, nBlockYOff, pImage]()
    {
        void *pImageData = nullptr;
        GDALRasterBlock *poBlock = nullptr;
        if (m_bIsMask)
        {
            CPLAssert(m_poMainBand);
            poBlock =
                m_poMainBand->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
            if (poBlock)
            {
                // The block is already in cache. Return (null, null)
                poBlock->DropLock();
                poBlock = nullptr;
            }
            else
            {
                poBlock = m_poMainBand->GetLockedBlockRef(nBlockXOff,
                                                          nBlockYOff, true);
                if (poBlock)
                    pImageData = poBlock->GetDataRef();
            }
        }
        else
        {
            pImageData = pImage;
        }
        return std::make_pair(pImageData, poBlock);
    };

    // Return (pointer to mask data, owner block). Works when called from main band
    // or mask band. owner block must be DropLock() once done (if not null)
    const auto GetMaskData = [this, nBlockXOff, nBlockYOff, pImage]()
    {
        void *pMaskData = nullptr;
        GDALRasterBlock *poBlock = nullptr;
        if (m_bIsMask)
        {
            pMaskData = pImage;
        }
        else
        {
            CPLAssert(m_poMaskBand);
            poBlock =
                m_poMaskBand->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
            if (poBlock)
            {
                // The block is already in cache. Return (null, null)
                poBlock->DropLock();
                poBlock = nullptr;
            }
            else
            {
                poBlock = m_poMaskBand->GetLockedBlockRef(nBlockXOff,
                                                          nBlockYOff, true);
                if (poBlock)
                    pMaskData = poBlock->GetDataRef();
            }
        }
        return std::make_pair(pMaskData, poBlock);
    };

    const GDALDataType eImageDT =
        m_poMainBand ? m_poMainBand->GetRasterDataType() : eDataType;
    const size_t nPixels = static_cast<size_t>(nBlockXSize) * nBlockYSize;

    const auto FillMissingBlock =
        [this, eImageDT, nPixels, &GetImageData, &GetMaskData]()
    {
        // Set image data to nodata / 0
        {
            auto imageDataAndBlock = GetImageData();
            auto pImageData = imageDataAndBlock.first;
            auto poBlock = imageDataAndBlock.second;
            if (pImageData)
            {
                const int nDTSize = GDALGetDataTypeSizeBytes(eImageDT);
                if (m_bHasNoData)
                {
                    GDALCopyWords64(&m_dfNoData, GDT_Float64, 0, pImageData,
                                    eImageDT, nDTSize, nPixels);
                }
                else
                {
                    memset(pImageData, 0, nPixels * nDTSize);
                }
            }
            if (poBlock)
                poBlock->DropLock();
        }

        // Set mask band to 0 (when it exists)
        if (m_poMaskBand || m_bIsMask)
        {
            auto maskDataAndBlock = GetMaskData();
            auto pMaskData = maskDataAndBlock.first;
            auto poBlock = maskDataAndBlock.second;
            if (pMaskData)
            {
                const size_t nSize =
                    static_cast<size_t>(nBlockXSize) * nBlockYSize;
                memset(pMaskData, 0, nSize);
            }
            if (poBlock)
                poBlock->DropLock();
        }
    };

    // Fetch block data from fras_blk_XXX layer
    const int nGDALBandId = m_bIsMask ? 1 : nBand;
    auto oIter = poGDS->m_oMapGDALBandToGDBBandId.find(nGDALBandId);
    if (oIter == poGDS->m_oMapGDALBandToGDBBandId.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "poGDS->m_oMapGDALBandToGDBBandId.find(%d) failed",
                 nGDALBandId);
        return CE_Failure;
    }
    const int nGDBRasterBandId = oIter->second;

    CPLString osFilter;
    /* osFilter.Printf("rasterband_id = %d AND rrd_factor = %d AND row_nbr = %d "
                    "AND col_nbr = %d",
                    nGDBRasterBandId,
                    m_nOverviewLevel, nBlockYOff, nBlockXOff);
    */
    const int nColNbr = nBlockXOff + poGDS->m_nShiftBlockX;
    const int nRowNbr = nBlockYOff + poGDS->m_nShiftBlockY;
    if (nRowNbr >= 0 && nColNbr >= 0)
    {
        osFilter.Printf("block_key = '0000%04X%02X%04X%04X'", nGDBRasterBandId,
                        m_nOverviewLevel, nRowNbr, nColNbr);
    }
    else if (nRowNbr < 0 && nColNbr >= 0)
    {
        osFilter.Printf("block_key = '0000%04X%02X-%04X%04X'", nGDBRasterBandId,
                        m_nOverviewLevel, -nRowNbr, nColNbr);
    }
    else if (nRowNbr >= 0 && nColNbr < 0)
    {
        osFilter.Printf("block_key = '0000%04X%02X%04X-%04X'", nGDBRasterBandId,
                        m_nOverviewLevel, nRowNbr, -nColNbr);
    }
    else /* if( nRowNbr < 0 && nColNbr < 0 ) */
    {
        osFilter.Printf("block_key = '0000%04X%02X-%04X-%04X'",
                        nGDBRasterBandId, m_nOverviewLevel, -nRowNbr, -nColNbr);
    }
    // CPLDebug("OpenFileGDB", "Request %s", osFilter.c_str());
    poLyr->SetAttributeFilter(osFilter.c_str());
    auto poFeature = std::unique_ptr<OGRFeature>(poLyr->GetNextFeature());
    const int nImageDTSize = GDALGetDataTypeSizeBytes(eImageDT);
    if (!poFeature)
    {
        // Missing blocks are legit
        FillMissingBlock();
        return CE_None;
    }
    const int nFieldIdx = poFeature->GetFieldIndex("block_data");
    CPLAssert(nFieldIdx >= 0);
    int nInBytes = 0;
    if (!poFeature->IsFieldSetAndNotNull(nFieldIdx))
    {
        // block_data unset found on ForestFalls.gdb
        FillMissingBlock();
        return CE_None;
    }
    const GByte *pabyData = poFeature->GetFieldAsBinary(nFieldIdx, &nInBytes);
    if (nInBytes == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Image block is empty");
        return CE_Failure;
    }

    // The input buffer may be concatenated with a 1-bit binary mask
    const size_t nImageSize = nPixels * nImageDTSize;
    const int nImageBitWidth =
        m_poMainBand ? m_poMainBand->m_nBitWidth : m_nBitWidth;
    const size_t nImageSizePacked = (nPixels * nImageBitWidth + 7) / 8;
    const size_t nBinaryMaskSize = (nPixels + 7) / 8;
    const size_t nImageSizeWithBinaryMask = nImageSizePacked + nBinaryMaskSize;

    // Unpack 1-bit, 2-bit, 4-bit data to full byte
    const auto ExpandSubByteData =
        [nPixels, nImageBitWidth](const GByte *pabyInput, void *pDstBuffer)
    {
        CPLAssert(nImageBitWidth < 8);

        size_t iBitOffset = 0;
        for (size_t i = 0; i < nPixels; ++i)
        {
            unsigned nOutWord = 0;

            for (int iBit = 0; iBit < nImageBitWidth; ++iBit)
            {
                if (pabyInput[iBitOffset >> 3] & (0x80 >> (iBitOffset & 7)))
                {
                    nOutWord |= (1 << (nImageBitWidth - 1 - iBit));
                }
                ++iBitOffset;
            }

            static_cast<GByte *>(pDstBuffer)[i] = static_cast<GByte>(nOutWord);
        }
    };

    const GByte *pabyMask = nullptr;
    auto &abyTmpBuffer =
        m_poMainBand ? m_poMainBand->m_abyTmpBuffer : m_abyTmpBuffer;

    switch (poGDS->m_eRasterCompression)
    {
        case OGROpenFileGDBDataSource::Compression::NONE:
        {
            if (static_cast<unsigned>(nInBytes) != nImageSizePacked &&
                static_cast<unsigned>(nInBytes) != nImageSizeWithBinaryMask)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Not expected number of input bytes: %d", nInBytes);
                return CE_Failure;
            }

            auto imageDataAndBlock = GetImageData();
            auto pImageData = imageDataAndBlock.first;
            auto poBlock = imageDataAndBlock.second;

            if (pImageData)
            {
                if (nImageSizePacked == nImageSize)
                {
                    memcpy(pImageData, pabyData, nImageSize);
#ifdef CPL_LSB
                    if (nImageDTSize > 1)
                    {
                        GDALSwapWordsEx(pImageData, nImageDTSize, nPixels,
                                        nImageDTSize);
                    }
#endif
                }
                else
                {
                    ExpandSubByteData(pabyData, pImageData);
                }
            }
            if (poBlock)
                poBlock->DropLock();

            if (static_cast<unsigned>(nInBytes) == nImageSizeWithBinaryMask)
                pabyMask = pabyData + nImageSizePacked;
            break;
        }

        case OGROpenFileGDBDataSource::Compression::LZ77:
        {
            if (abyTmpBuffer.empty())
            {
                try
                {
                    abyTmpBuffer.resize(nImageSizeWithBinaryMask);
                }
                catch (const std::bad_alloc &e)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
                    return CE_Failure;
                }
            }

            size_t nOutBytes = 0;
            GByte *outPtr = abyTmpBuffer.data();
            assert(outPtr != nullptr);  // For Coverity Scan
            if (!CPLZLibInflate(pabyData, nInBytes, outPtr, abyTmpBuffer.size(),
                                &nOutBytes) ||
                !(nOutBytes == nImageSizePacked ||
                  nOutBytes == nImageSizeWithBinaryMask))
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "CPLZLibInflate() failed: nInBytes = %u, nOutBytes = %u, "
                    "nImageSizePacked = %u, "
                    "nImageSizeWithBinaryMask = %u",
                    unsigned(nInBytes), unsigned(nOutBytes),
                    unsigned(nImageSizePacked),
                    unsigned(nImageSizeWithBinaryMask));
                return CE_Failure;
            }

            auto imageDataAndBlock = GetImageData();
            auto pImageData = imageDataAndBlock.first;
            auto poBlock = imageDataAndBlock.second;

            if (pImageData)
            {
                if (nImageSizePacked == nImageSize)
                {
                    memcpy(pImageData, abyTmpBuffer.data(), nImageSize);
#ifdef CPL_LSB
                    if (nImageDTSize > 1)
                    {
                        GDALSwapWordsEx(pImageData, nImageDTSize, nPixels,
                                        nImageDTSize);
                    }
#endif
                }
                else
                {
                    ExpandSubByteData(abyTmpBuffer.data(), pImageData);
                }
            }
            if (poBlock)
                poBlock->DropLock();

            if (nOutBytes == nImageSizeWithBinaryMask)
                pabyMask = abyTmpBuffer.data() + nImageSizePacked;
            break;
        }

        case OGROpenFileGDBDataSource::Compression::JPEG:
        {
            if (GDALGetDriverByName("JPEG") == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "JPEG driver missing");
                return CE_Failure;
            }

            if (static_cast<unsigned>(nInBytes) < 5)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Not expected number of input bytes: %d", nInBytes);
                return CE_Failure;
            }
            uint32_t nJPEGSize = nInBytes - 1;
            uint32_t nJPEGOffset = 1;
            if (pabyData[0] == 0xFE)
            {
                // JPEG followed by binary mask
                memcpy(&nJPEGSize, pabyData + 1, sizeof(uint32_t));
                CPL_LSBPTR32(&nJPEGSize);
                if (nJPEGSize > static_cast<unsigned>(nInBytes - 5))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid nJPEGSize = %u", nJPEGSize);
                    return CE_Failure;
                }
                nJPEGOffset = 5;

                if (abyTmpBuffer.empty())
                {
                    try
                    {
                        abyTmpBuffer.resize(nBinaryMaskSize);
                    }
                    catch (const std::bad_alloc &e)
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
                        return CE_Failure;
                    }
                }
                size_t nOutBytes = 0;
                GByte *outPtr = abyTmpBuffer.data();
                assert(outPtr != nullptr);  // For Coverity Scan
                if (CPLZLibInflate(pabyData + 5 + nJPEGSize,
                                   nInBytes - 5 - nJPEGSize, outPtr,
                                   nBinaryMaskSize, &nOutBytes) &&
                    nOutBytes == nBinaryMaskSize)
                {
                    pabyMask = abyTmpBuffer.data();
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot decompress binary mask");
                }
            }
            else if (pabyData[0] != 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid JPEG blob");
                return CE_Failure;
            }

            VSILFILE *fp = VSIFOpenL("tmp.jpg", "wb");
            VSIFWriteL(pabyData + nJPEGOffset, nJPEGSize, 1, fp);
            VSIFCloseL(fp);

            CPLString osTmpFilename;
            osTmpFilename.Printf("/vsimem/_openfilegdb/%p.jpg", this);
            VSIFCloseL(VSIFileFromMemBuffer(
                osTmpFilename.c_str(),
                const_cast<GByte *>(pabyData + nJPEGOffset), nJPEGSize, false));
            const char *const apszDrivers[] = {"JPEG", nullptr};
            auto poJPEGDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                osTmpFilename.c_str(), GDAL_OF_RASTER, apszDrivers));
            if (!poJPEGDS)
            {
                VSIUnlink(osTmpFilename.c_str());
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot open JPEG blob");
                return CE_Failure;
            }
            if (poJPEGDS->GetRasterCount() != 1 ||
                poJPEGDS->GetRasterXSize() != nBlockXSize ||
                poJPEGDS->GetRasterYSize() != nBlockYSize)
            {
                VSIUnlink(osTmpFilename.c_str());
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Inconsistent characteristics of JPEG blob");
                return CE_Failure;
            }

            auto imageDataAndBlock = GetImageData();
            auto pImageData = imageDataAndBlock.first;
            auto poBlock = imageDataAndBlock.second;

            const CPLErr eErr =
                pImageData
                    ? poJPEGDS->GetRasterBand(1)->RasterIO(
                          GF_Read, 0, 0, nBlockXSize, nBlockYSize, pImageData,
                          nBlockXSize, nBlockYSize, eImageDT, 0, 0, nullptr)
                    : CE_None;
            VSIUnlink(osTmpFilename.c_str());
            if (poBlock)
                poBlock->DropLock();

            if (eErr != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot read JPEG blob");
                return CE_Failure;
            }

            break;
        }

        case OGROpenFileGDBDataSource::Compression::JPEG2000:
        {
            const char *const apszDrivers[] = {"JP2KAK",      "JP2ECW",
                                               "JP2OpenJPEG", "JP2MrSID",
                                               "JP2Lura",     nullptr};
            bool bFoundJP2Driver = false;
            for (const char *pszDriver : apszDrivers)
            {
                if (pszDriver && GDALGetDriverByName(pszDriver))
                {
                    bFoundJP2Driver = true;
                    break;
                }
            }
            if (!bFoundJP2Driver)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Did not find any JPEG2000 capable driver");
                return CE_Failure;
            }

            if (static_cast<unsigned>(nInBytes) < 5)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Not expected number of input bytes: %d", nInBytes);
                return CE_Failure;
            }
            uint32_t nJPEGSize = nInBytes - 1;
            uint32_t nJPEGOffset = 1;
            if (pabyData[0] == 0xFF)
            {
                // JPEG2000 followed by binary mask
                memcpy(&nJPEGSize, pabyData + 1, sizeof(uint32_t));
                CPL_LSBPTR32(&nJPEGSize);
                if (nJPEGSize > static_cast<unsigned>(nInBytes - 5))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid nJPEGSize = %u", nJPEGSize);
                    return CE_Failure;
                }
                nJPEGOffset = 5;

                if (abyTmpBuffer.empty())
                {
                    try
                    {
                        abyTmpBuffer.resize(nBinaryMaskSize);
                    }
                    catch (const std::bad_alloc &e)
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
                        return CE_Failure;
                    }
                }
                size_t nOutBytes = 0;
                GByte *outPtr = abyTmpBuffer.data();
                assert(outPtr != nullptr);  // For Coverity Scan
                if (CPLZLibInflate(pabyData + 5 + nJPEGSize,
                                   nInBytes - 5 - nJPEGSize, outPtr,
                                   nBinaryMaskSize, &nOutBytes) &&
                    nOutBytes == nBinaryMaskSize)
                {
                    pabyMask = outPtr;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot decompress binary mask");
                }
            }
            else if (pabyData[0] != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid JPEG2000 blob");
                return CE_Failure;
            }

            CPLString osTmpFilename;
            osTmpFilename.Printf("/vsimem/_openfilegdb/%p.j2k", this);
            VSIFCloseL(VSIFileFromMemBuffer(
                osTmpFilename.c_str(),
                const_cast<GByte *>(pabyData + nJPEGOffset), nJPEGSize, false));
            auto poJP2KDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                osTmpFilename.c_str(), GDAL_OF_RASTER, apszDrivers));
            if (!poJP2KDS)
            {
                VSIUnlink(osTmpFilename.c_str());
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot open JPEG2000 blob");
                return CE_Failure;
            }
            if (poJP2KDS->GetRasterCount() != 1 ||
                poJP2KDS->GetRasterXSize() != nBlockXSize ||
                poJP2KDS->GetRasterYSize() != nBlockYSize)
            {
                VSIUnlink(osTmpFilename.c_str());
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Inconsistent characteristics of JPEG2000 blob");
                return CE_Failure;
            }

            auto imageDataAndBlock = GetImageData();
            auto pImageData = imageDataAndBlock.first;
            auto poBlock = imageDataAndBlock.second;

            const CPLErr eErr =
                pImageData
                    ? poJP2KDS->GetRasterBand(1)->RasterIO(
                          GF_Read, 0, 0, nBlockXSize, nBlockYSize, pImageData,
                          nBlockXSize, nBlockYSize, eImageDT, 0, 0, nullptr)
                    : CE_None;
            VSIUnlink(osTmpFilename.c_str());
            if (poBlock)
                poBlock->DropLock();

            if (eErr != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read JPEG2000 blob");
                return CE_Failure;
            }

            break;
        }
    }

    if (m_bIsMask || m_poMaskBand)
    {
        auto maskDataAndBlock = GetMaskData();
        auto pMaskData = maskDataAndBlock.first;
        auto poBlock = maskDataAndBlock.second;

        if (pMaskData)
        {
            if (pabyMask)
            {
                // Unpack 1-bit array
                for (size_t i = 0; i < nPixels; ++i)
                {
                    static_cast<GByte *>(pMaskData)[i] =
                        (pabyMask[i / 8] & (0x80 >> (i & 7))) ? 255 : 0;
                }
            }
            else
            {
                // No explicit mask in source block --> all valid
                memset(pMaskData, 255, nPixels);
            }
        }

        if (poBlock)
            poBlock->DropLock();
    }
    else if (m_bHasNoData)
    {
        if (eImageDT == GDT_Byte)
        {
            SetNoDataFromMask<uint8_t>(pImage, pabyMask, nPixels, m_dfNoData);
        }
        else if (eImageDT == GDT_Int8)
        {
            SetNoDataFromMask<int8_t>(pImage, pabyMask, nPixels, m_dfNoData);
        }
        else if (eImageDT == GDT_UInt16)
        {
            SetNoDataFromMask<uint16_t>(pImage, pabyMask, nPixels, m_dfNoData);
        }
        else if (eImageDT == GDT_Int16)
        {
            SetNoDataFromMask<int16_t>(pImage, pabyMask, nPixels, m_dfNoData);
        }
        else if (eImageDT == GDT_UInt32)
        {
            SetNoDataFromMask<uint32_t>(pImage, pabyMask, nPixels, m_dfNoData);
        }
        else if (eImageDT == GDT_Int32)
        {
            SetNoDataFromMask<int32_t>(pImage, pabyMask, nPixels, m_dfNoData);
        }
        else if (eImageDT == GDT_Float32)
        {
            if (pabyMask)
            {
                for (size_t i = 0; i < nPixels; ++i)
                {
                    if (!(pabyMask[i / 8] & (0x80 >> (i & 7))))
                    {
                        static_cast<float *>(pImage)[i] =
                            static_cast<float>(m_dfNoData);
                    }
                }
            }
        }
        else if (eImageDT == GDT_Float64)
        {
            if (pabyMask)
            {
                for (size_t i = 0; i < nPixels; ++i)
                {
                    if (!(pabyMask[i / 8] & (0x80 >> (i & 7))))
                    {
                        static_cast<double *>(pImage)[i] = m_dfNoData;
                    }
                }
            }
        }
        else
        {
            CPLAssert(false);
        }
    }

#if 0
    printf("Data:\n"); // ok
    if (eDataType == GDT_Byte)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%d ", // ok
                       static_cast<GByte *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
    else if (eDataType == GDT_Int8)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%d ", // ok
                       static_cast<int8_t *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
    else if (eDataType == GDT_UInt16)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%d ", // ok
                       static_cast<uint16_t *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
    else if (eDataType == GDT_Int16)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%d ", // ok
                       static_cast<int16_t *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
    else if (eDataType == GDT_UInt32)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%d ", // ok
                       static_cast<uint32_t *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
    else if (eDataType == GDT_Int32)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%d ", // ok
                       static_cast<int32_t *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
    else if (eDataType == GDT_Float32)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%.8g ", // ok
                       static_cast<float *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
    else if (eDataType == GDT_Float64)
    {
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%.18g ", // ok
                       static_cast<double *>(pImage)[y * nBlockXSize + x]);
            }
            printf("\n"); // ok
        }
    }
#endif

#if 0
    if (pabyMask)
    {
        printf("Mask:\n"); // ok
        for (int y = 0; y < nBlockYSize; ++y)
        {
            for (int x = 0; x < nBlockXSize; ++x)
            {
                printf("%d ", // ok
                       (pabyMask[(y * nBlockXSize + x) / 8] &
                        (0x80 >> ((y * nBlockXSize + x) & 7)))
                           ? 1
                           : 0);
            }
            printf("\n"); // ok
        }
    }
#endif

    return CE_None;
}

/************************************************************************/
/*                         GetDefaultRAT()                              */
/************************************************************************/

GDALRasterAttributeTable *GDALOpenFileGDBRasterBand::GetDefaultRAT()
{
    if (m_poRAT)
        return m_poRAT.get();
    if (poDS->GetRasterCount() > 1 || m_bIsMask)
        return nullptr;
    auto poGDS = cpl::down_cast<OGROpenFileGDBDataSource *>(poDS);
    const std::string osVATTableName(
        std::string("VAT_").append(poGDS->m_osRasterLayerName));
    // Instantiate a new dataset, os that the RAT is standalone
    auto poDSNew = std::make_unique<OGROpenFileGDBDataSource>();
    GDALOpenInfo oOpenInfo(poGDS->m_osDirName.c_str(), GA_ReadOnly);
    bool bRetryFileGDBUnused = false;
    if (!poDSNew->Open(&oOpenInfo, bRetryFileGDBUnused))
        return nullptr;
    auto poVatLayer = poDSNew->BuildLayerFromName(osVATTableName.c_str());
    if (!poVatLayer)
        return nullptr;
    m_poRAT = std::make_unique<GDALOpenFileGDBRasterAttributeTable>(
        std::move(poDSNew), osVATTableName, std::move(poVatLayer));
    return m_poRAT.get();
}
