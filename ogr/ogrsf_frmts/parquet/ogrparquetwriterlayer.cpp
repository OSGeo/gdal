/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#undef DO_NOT_DEFINE_GDAL_DATE_NAME
#include "gdal_version_full/gdal_version.h"

#include "ogr_parquet.h"

#include "../arrow_common/ograrrowwriterlayer.hpp"

#include "ogr_wkb.h"

#include <utility>

/************************************************************************/
/*                      OGRParquetWriterLayer()                         */
/************************************************************************/

OGRParquetWriterLayer::OGRParquetWriterLayer(
    OGRParquetWriterDataset *poDataset, arrow::MemoryPool *poMemoryPool,
    const std::shared_ptr<arrow::io::OutputStream> &poOutputStream,
    const char *pszLayerName)
    : OGRArrowWriterLayer(poMemoryPool, poOutputStream, pszLayerName),
      m_poDataset(poDataset)
{
    m_bWriteFieldArrowExtensionName = CPLTestBool(
        CPLGetConfigOption("OGR_PARQUET_WRITE_ARROW_EXTENSION_NAME", "NO"));
}

/************************************************************************/
/*                                Close()                               */
/************************************************************************/

bool OGRParquetWriterLayer::Close()
{
    if (m_poTmpGPKGLayer)
    {
        if (!CopyTmpGpkgLayerToFinalFile())
            return false;
    }

    if (m_bInitializationOK)
    {
        if (!FinalizeWriting())
            return false;
    }

    return true;
}

/************************************************************************/
/*                     CopyTmpGpkgLayerToFinalFile()                    */
/************************************************************************/

bool OGRParquetWriterLayer::CopyTmpGpkgLayerToFinalFile()
{
    if (!m_poTmpGPKGLayer)
    {
        return true;
    }

    CPLDebug("PARQUET", "CopyTmpGpkgLayerToFinalFile(): start...");

    VSIUnlink(m_poTmpGPKG->GetDescription());

    OGRFeature oFeat(m_poFeatureDefn);

    // Interval in terms of features between 2 debug progress report messages
    constexpr int PROGRESS_FC_INTERVAL = 100 * 1000;

    // First, write features without geometries
    {
        auto poTmpLayer = std::unique_ptr<OGRLayer>(m_poTmpGPKG->ExecuteSQL(
            "SELECT serialized_feature FROM tmp WHERE fid NOT IN (SELECT id "
            "FROM rtree_tmp_geom)",
            nullptr, nullptr));
        if (!poTmpLayer)
            return false;
        for (const auto &poSrcFeature : poTmpLayer.get())
        {
            int nBytesFeature = 0;
            const GByte *pabyFeatureData =
                poSrcFeature->GetFieldAsBinary(0, &nBytesFeature);
            if (!oFeat.DeserializeFromBinary(pabyFeatureData, nBytesFeature))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot deserialize feature");
                return false;
            }
            if (OGRArrowWriterLayer::ICreateFeature(&oFeat) != OGRERR_NONE)
            {
                return false;
            }

            if ((m_nFeatureCount % PROGRESS_FC_INTERVAL) == 0)
            {
                CPLDebugProgress(
                    "PARQUET",
                    "CopyTmpGpkgLayerToFinalFile(): %.02f%% progress",
                    100.0 * double(m_nFeatureCount) /
                        double(m_nTmpFeatureCount));
            }
        }

        if (!FlushFeatures())
        {
            return false;
        }
    }

    // Now walk through the GPKG RTree for features with geometries
    // Cf https://github.com/sqlite/sqlite/blob/master/ext/rtree/rtree.c
    // for the description of the content of the rtree _node table
    std::vector<std::pair<int64_t, int>> aNodeNoDepthPair;
    int nTreeDepth = 0;
    // Queue the root node
    aNodeNoDepthPair.emplace_back(
        std::make_pair(/* nodeNo = */ 1, /* depth = */ 0));
    int nCountWrittenFeaturesSinceLastFlush = 0;
    while (!aNodeNoDepthPair.empty())
    {
        const auto &oLastPair = aNodeNoDepthPair.back();
        const int64_t nNodeNo = oLastPair.first;
        const int nCurDepth = oLastPair.second;
        //CPLDebug("PARQUET", "Reading nodeNode=%d, curDepth=%d", int(nNodeNo), nCurDepth);
        aNodeNoDepthPair.pop_back();

        auto poRTreeLayer = std::unique_ptr<OGRLayer>(m_poTmpGPKG->ExecuteSQL(
            CPLSPrintf("SELECT data FROM rtree_tmp_geom_node WHERE nodeno "
                       "= " CPL_FRMT_GIB,
                       static_cast<GIntBig>(nNodeNo)),
            nullptr, nullptr));
        if (!poRTreeLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read node " CPL_FRMT_GIB,
                     static_cast<GIntBig>(nNodeNo));
            return false;
        }
        const auto poRTreeFeature =
            std::unique_ptr<const OGRFeature>(poRTreeLayer->GetNextFeature());
        if (!poRTreeFeature)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read node " CPL_FRMT_GIB,
                     static_cast<GIntBig>(nNodeNo));
            return false;
        }

        int nNodeBytes = 0;
        const GByte *pabyNodeData =
            poRTreeFeature->GetFieldAsBinary(0, &nNodeBytes);
        constexpr int BLOB_HEADER_SIZE = 4;
        if (nNodeBytes < BLOB_HEADER_SIZE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Not enough bytes when reading node " CPL_FRMT_GIB,
                     static_cast<GIntBig>(nNodeNo));
            return false;
        }
        if (nNodeNo == 1)
        {
            // Get the RTree depth from the root node
            nTreeDepth = (pabyNodeData[0] << 8) | pabyNodeData[1];
            //CPLDebug("PARQUET", "nTreeDepth = %d", nTreeDepth);
        }

        const int nCellCount = (pabyNodeData[2] << 8) | pabyNodeData[3];
        constexpr int SIZEOF_CELL = 24;  // int64_t + 4 float
        if (nNodeBytes < BLOB_HEADER_SIZE + SIZEOF_CELL * nCellCount)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Not enough bytes when reading node " CPL_FRMT_GIB,
                     static_cast<GIntBig>(nNodeNo));
            return false;
        }

        size_t nOffset = BLOB_HEADER_SIZE;
        if (nCurDepth == nTreeDepth)
        {
            // Leaf node: it references feature IDs.

            // If we are about to go above m_nRowGroupSize, flush past
            // features now, to improve the spatial compacity of the row group.
            if (m_nRowGroupSize > nCellCount &&
                nCountWrittenFeaturesSinceLastFlush + nCellCount >
                    m_nRowGroupSize)
            {
                nCountWrittenFeaturesSinceLastFlush = 0;
                if (!FlushFeatures())
                {
                    return false;
                }
            }

            // nCellCount shouldn't be over 51 normally, but even 65535
            // would be fine...
            // coverity[tainted_data]
            for (int i = 0; i < nCellCount; ++i)
            {
                int64_t nFID;
                memcpy(&nFID, pabyNodeData + nOffset, sizeof(int64_t));
                CPL_MSBPTR64(&nFID);

                const auto poSrcFeature = std::unique_ptr<const OGRFeature>(
                    m_poTmpGPKGLayer->GetFeature(nFID));
                if (!poSrcFeature)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot get feature " CPL_FRMT_GIB,
                             static_cast<GIntBig>(nFID));
                    return false;
                }

                int nBytesFeature = 0;
                const GByte *pabyFeatureData =
                    poSrcFeature->GetFieldAsBinary(0, &nBytesFeature);
                if (!oFeat.DeserializeFromBinary(pabyFeatureData,
                                                 nBytesFeature))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot deserialize feature");
                    return false;
                }
                if (OGRArrowWriterLayer::ICreateFeature(&oFeat) != OGRERR_NONE)
                {
                    return false;
                }

                nOffset += SIZEOF_CELL;

                ++nCountWrittenFeaturesSinceLastFlush;

                if ((m_nFeatureCount % PROGRESS_FC_INTERVAL) == 0 ||
                    m_nFeatureCount == m_nTmpFeatureCount / 2)
                {
                    CPLDebugProgress(
                        "PARQUET",
                        "CopyTmpGpkgLayerToFinalFile(): %.02f%% progress",
                        100.0 * double(m_nFeatureCount) /
                            double(m_nTmpFeatureCount));
                }
            }
        }
        else
        {
            // Non-leaf node: it references child nodes.

            // nCellCount shouldn't be over 51 normally, but even 65535
            // would be fine...
            // coverity[tainted_data]
            for (int i = 0; i < nCellCount; ++i)
            {
                int64_t nNode;
                memcpy(&nNode, pabyNodeData + nOffset, sizeof(int64_t));
                CPL_MSBPTR64(&nNode);
                aNodeNoDepthPair.emplace_back(
                    std::make_pair(nNode, nCurDepth + 1));
                nOffset += SIZEOF_CELL;
            }
        }
    }

    CPLDebug("PARQUET",
             "CopyTmpGpkgLayerToFinalFile(): 100%%, successfully finished");
    return true;
}

/************************************************************************/
/*                       IsSupportedGeometryType()                      */
/************************************************************************/

bool OGRParquetWriterLayer::IsSupportedGeometryType(
    OGRwkbGeometryType eGType) const
{
    const auto eFlattenType = wkbFlatten(eGType);
    if (!OGR_GT_HasM(eGType) && eFlattenType <= wkbGeometryCollection)
    {
        return true;
    }

    const auto osConfigOptionName =
        "OGR_" + GetDriverUCName() + "_ALLOW_ALL_DIMS";
    if (CPLTestBool(CPLGetConfigOption(osConfigOptionName.c_str(), "NO")))
    {
        return true;
    }

    CPLError(CE_Failure, CPLE_NotSupported,
             "Only 2D and Z geometry types are supported (unless the "
             "%s configuration option is set to YES)",
             osConfigOptionName.c_str());
    return false;
}

/************************************************************************/
/*                           SetOptions()                               */
/************************************************************************/

bool OGRParquetWriterLayer::SetOptions(CSLConstList papszOptions,
                                       const OGRSpatialReference *poSpatialRef,
                                       OGRwkbGeometryType eGType)
{
    m_bWriteBBoxStruct = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "WRITE_COVERING_BBOX",
        CPLGetConfigOption("OGR_PARQUET_WRITE_COVERING_BBOX", "YES")));

    if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "SORT_BY_BBOX", "NO")))
    {
        const std::string osTmpGPKG(std::string(m_poDataset->GetDescription()) +
                                    ".tmp.gpkg");
        auto poGPKGDrv = GetGDALDriverManager()->GetDriverByName("GPKG");
        if (!poGPKGDrv)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Driver GPKG required for SORT_BY_BBOX layer creation option");
            return false;
        }
        m_poTmpGPKG.reset(poGPKGDrv->Create(osTmpGPKG.c_str(), 0, 0, 0,
                                            GDT_Unknown, nullptr));
        if (!m_poTmpGPKG)
            return false;
        m_poTmpGPKG->MarkSuppressOnClose();
        m_poTmpGPKGLayer = m_poTmpGPKG->CreateLayer("tmp");
        if (!m_poTmpGPKGLayer)
            return false;
        // Serialized feature
        m_poTmpGPKGLayer->CreateField(
            std::make_unique<OGRFieldDefn>("serialized_feature", OFTBinary)
                .get());
        CPL_IGNORE_RET_VAL(m_poTmpGPKGLayer->StartTransaction());
    }

    const char *pszGeomEncoding =
        CSLFetchNameValue(papszOptions, "GEOMETRY_ENCODING");
    m_eGeomEncoding = OGRArrowGeomEncoding::WKB;
    if (pszGeomEncoding)
    {
        if (EQUAL(pszGeomEncoding, "WKB"))
            m_eGeomEncoding = OGRArrowGeomEncoding::WKB;
        else if (EQUAL(pszGeomEncoding, "WKT"))
            m_eGeomEncoding = OGRArrowGeomEncoding::WKT;
        else if (EQUAL(pszGeomEncoding, "GEOARROW_INTERLEAVED"))
        {
            static bool bHasWarned = false;
            if (!bHasWarned)
            {
                bHasWarned = true;
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Use of GEOMETRY_ENCODING=GEOARROW_INTERLEAVED is not "
                    "recommended. "
                    "GeoParquet 1.1 uses GEOMETRY_ENCODING=GEOARROW (struct) "
                    "instead.");
            }
            m_eGeomEncoding = OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC;
        }
        else if (EQUAL(pszGeomEncoding, "GEOARROW") ||
                 EQUAL(pszGeomEncoding, "GEOARROW_STRUCT"))
            m_eGeomEncoding = OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC;
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported GEOMETRY_ENCODING = %s", pszGeomEncoding);
            return false;
        }
    }

    const char *pszCoordPrecision =
        CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION");
    if (pszCoordPrecision)
        m_nWKTCoordinatePrecision = atoi(pszCoordPrecision);

    m_bForceCounterClockwiseOrientation =
        EQUAL(CSLFetchNameValueDef(papszOptions, "POLYGON_ORIENTATION",
                                   "COUNTERCLOCKWISE"),
              "COUNTERCLOCKWISE");

    if (eGType != wkbNone)
    {
        if (!IsSupportedGeometryType(eGType))
        {
            return false;
        }

        m_poFeatureDefn->SetGeomType(eGType);
        auto eGeomEncoding = m_eGeomEncoding;
        if (eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_FSL_GENERIC ||
            eGeomEncoding == OGRArrowGeomEncoding::GEOARROW_STRUCT_GENERIC)
        {
            const auto eEncodingType = eGeomEncoding;
            eGeomEncoding = GetPreciseArrowGeomEncoding(eEncodingType, eGType);
            if (eGeomEncoding == eEncodingType)
                return false;
        }
        m_aeGeomEncoding.push_back(eGeomEncoding);
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetName(
            CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "geometry"));
        if (poSpatialRef)
        {
            auto poSRS = poSpatialRef->Clone();
            m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
            poSRS->Release();
        }
    }

    m_osFIDColumn = CSLFetchNameValueDef(papszOptions, "FID", "");

    const char *pszCompression = CSLFetchNameValue(papszOptions, "COMPRESSION");
    if (pszCompression == nullptr)
    {
        auto oResult = arrow::util::Codec::GetCompressionType("snappy");
        if (oResult.ok() && arrow::util::Codec::IsAvailable(*oResult))
        {
            pszCompression = "SNAPPY";
        }
        else
        {
            pszCompression = "NONE";
        }
    }

    if (EQUAL(pszCompression, "NONE"))
        pszCompression = "UNCOMPRESSED";
    auto oResult = arrow::util::Codec::GetCompressionType(
        CPLString(pszCompression).tolower());
    if (!oResult.ok())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unrecognized compression method: %s", pszCompression);
        return false;
    }
    m_eCompression = *oResult;
    if (!arrow::util::Codec::IsAvailable(m_eCompression))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Compression method %s is known, but libarrow has not "
                 "been built with support for it",
                 pszCompression);
        return false;
    }

    m_oWriterPropertiesBuilder.compression(m_eCompression);
    const std::string osCreator =
        CSLFetchNameValueDef(papszOptions, "CREATOR", "");
    if (!osCreator.empty())
        m_oWriterPropertiesBuilder.created_by(osCreator);
    else
        m_oWriterPropertiesBuilder.created_by("GDAL " GDAL_RELEASE_NAME
                                              ", using " CREATED_BY_VERSION);

    // Undocumented option. Not clear it is useful besides unit test purposes
    if (!CPLTestBool(CSLFetchNameValueDef(papszOptions, "STATISTICS", "YES")))
        m_oWriterPropertiesBuilder.disable_statistics();

    if (m_eGeomEncoding == OGRArrowGeomEncoding::WKB && eGType != wkbNone)
    {
        m_oWriterPropertiesBuilder.disable_statistics(
            parquet::schema::ColumnPath::FromDotString(
                m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()));
    }

    const char *pszRowGroupSize =
        CSLFetchNameValue(papszOptions, "ROW_GROUP_SIZE");
    if (pszRowGroupSize)
    {
        auto nRowGroupSize = static_cast<int64_t>(atoll(pszRowGroupSize));
        if (nRowGroupSize > 0)
        {
            if (nRowGroupSize > INT_MAX)
                nRowGroupSize = INT_MAX;
            m_nRowGroupSize = nRowGroupSize;
        }
    }

    m_bEdgesSpherical = EQUAL(
        CSLFetchNameValueDef(papszOptions, "EDGES", "PLANAR"), "SPHERICAL");

    m_bInitializationOK = true;
    return true;
}

/************************************************************************/
/*                         CloseFileWriter()                            */
/************************************************************************/

bool OGRParquetWriterLayer::CloseFileWriter()
{
    auto status = m_poFileWriter->Close();
    if (!status.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FileWriter::Close() failed with %s",
                 status.message().c_str());
    }
    return status.ok();
}

/************************************************************************/
/*                            IdentifyCRS()                             */
/************************************************************************/

static OGRSpatialReference IdentifyCRS(const OGRSpatialReference *poSRS)
{
    OGRSpatialReference oSRSIdentified(*poSRS);

    if (poSRS->GetAuthorityName(nullptr) == nullptr)
    {
        // Try to find a registered CRS that matches the input one
        int nEntries = 0;
        int *panConfidence = nullptr;
        OGRSpatialReferenceH *pahSRS =
            poSRS->FindMatches(nullptr, &nEntries, &panConfidence);

        // If there are several matches >= 90%, take the only one
        // that is EPSG
        int iOtherAuthority = -1;
        int iEPSG = -1;
        const char *const apszOptions[] = {
            "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES", nullptr};
        int iConfidenceBestMatch = -1;
        for (int iSRS = 0; iSRS < nEntries; iSRS++)
        {
            auto poCandidateCRS = OGRSpatialReference::FromHandle(pahSRS[iSRS]);
            if (panConfidence[iSRS] < iConfidenceBestMatch ||
                panConfidence[iSRS] < 70)
            {
                break;
            }
            if (poSRS->IsSame(poCandidateCRS, apszOptions))
            {
                const char *pszAuthName =
                    poCandidateCRS->GetAuthorityName(nullptr);
                if (pszAuthName != nullptr && EQUAL(pszAuthName, "EPSG"))
                {
                    iOtherAuthority = -2;
                    if (iEPSG < 0)
                    {
                        iConfidenceBestMatch = panConfidence[iSRS];
                        iEPSG = iSRS;
                    }
                    else
                    {
                        iEPSG = -1;
                        break;
                    }
                }
                else if (iEPSG < 0 && pszAuthName != nullptr)
                {
                    if (EQUAL(pszAuthName, "OGC"))
                    {
                        const char *pszAuthCode =
                            poCandidateCRS->GetAuthorityCode(nullptr);
                        if (pszAuthCode && EQUAL(pszAuthCode, "CRS84"))
                        {
                            iOtherAuthority = iSRS;
                            break;
                        }
                    }
                    else if (iOtherAuthority == -1)
                    {
                        iConfidenceBestMatch = panConfidence[iSRS];
                        iOtherAuthority = iSRS;
                    }
                    else
                        iOtherAuthority = -2;
                }
            }
        }
        if (iEPSG >= 0)
        {
            oSRSIdentified = *OGRSpatialReference::FromHandle(pahSRS[iEPSG]);
        }
        else if (iOtherAuthority >= 0)
        {
            oSRSIdentified =
                *OGRSpatialReference::FromHandle(pahSRS[iOtherAuthority]);
        }
        OSRFreeSRSArray(pahSRS);
        CPLFree(panConfidence);
    }

    return oSRSIdentified;
}

/************************************************************************/
/*                      RemoveIDFromMemberOfEnsembles()                 */
/************************************************************************/

static void RemoveIDFromMemberOfEnsembles(CPLJSONObject &obj)
{
    // Remove "id" from members of datum ensembles for compatibility with
    // older PROJ versions
    // Cf https://github.com/opengeospatial/geoparquet/discussions/110
    // and https://github.com/OSGeo/PROJ/pull/3221
    if (obj.GetType() == CPLJSONObject::Type::Object)
    {
        for (auto &subObj : obj.GetChildren())
        {
            RemoveIDFromMemberOfEnsembles(subObj);
        }
    }
    else if (obj.GetType() == CPLJSONObject::Type::Array &&
             obj.GetName() == "members")
    {
        for (auto &subObj : obj.ToArray())
        {
            subObj.Delete("id");
        }
    }
}

/************************************************************************/
/*                            GetGeoMetadata()                          */
/************************************************************************/

std::string OGRParquetWriterLayer::GetGeoMetadata() const
{
    // Just for unit testing purposes
    const char *pszGeoMetadata =
        CPLGetConfigOption("OGR_PARQUET_GEO_METADATA", nullptr);
    if (pszGeoMetadata)
        return pszGeoMetadata;

    if (m_poFeatureDefn->GetGeomFieldCount() != 0 &&
        CPLTestBool(CPLGetConfigOption("OGR_PARQUET_WRITE_GEO", "YES")))
    {
        CPLJSONObject oRoot;
        oRoot.Add("version", "1.1.0");
        oRoot.Add("primary_column",
                  m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        CPLJSONObject oColumns;
        oRoot.Add("columns", oColumns);
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
        {
            const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            CPLJSONObject oColumn;
            oColumns.Add(poGeomFieldDefn->GetNameRef(), oColumn);
            oColumn.Add("encoding",
                        GetGeomEncodingAsString(m_aeGeomEncoding[i], true));

            if (CPLTestBool(CPLGetConfigOption("OGR_PARQUET_WRITE_CRS", "YES")))
            {
                const auto poSRS = poGeomFieldDefn->GetSpatialRef();
                if (poSRS)
                {
                    OGRSpatialReference oSRSIdentified(IdentifyCRS(poSRS));

                    const char *pszAuthName =
                        oSRSIdentified.GetAuthorityName(nullptr);
                    const char *pszAuthCode =
                        oSRSIdentified.GetAuthorityCode(nullptr);

                    bool bOmitCRS = false;
                    if (pszAuthName != nullptr && pszAuthCode != nullptr &&
                        ((EQUAL(pszAuthName, "EPSG") &&
                          EQUAL(pszAuthCode, "4326")) ||
                         (EQUAL(pszAuthName, "OGC") &&
                          EQUAL(pszAuthCode, "CRS84"))))
                    {
                        // To make things less confusing for non-geo-aware
                        // consumers, omit EPSG:4326 / OGC:CRS84 CRS by default
                        bOmitCRS = CPLTestBool(CPLGetConfigOption(
                            "OGR_PARQUET_CRS_OMIT_IF_WGS84", "YES"));
                    }

                    if (bOmitCRS)
                    {
                        // do nothing
                    }
                    else if (EQUAL(CPLGetConfigOption(
                                       "OGR_PARQUET_CRS_ENCODING", "PROJJSON"),
                                   "PROJJSON"))
                    {
                        // CRS encoded as PROJJSON for GeoParquet >= 0.4.0
                        char *pszPROJJSON = nullptr;
                        oSRSIdentified.exportToPROJJSON(&pszPROJJSON, nullptr);
                        CPLJSONDocument oCRSDoc;
                        CPL_IGNORE_RET_VAL(oCRSDoc.LoadMemory(pszPROJJSON));
                        CPLFree(pszPROJJSON);
                        CPLJSONObject oCRSRoot = oCRSDoc.GetRoot();
                        RemoveIDFromMemberOfEnsembles(oCRSRoot);
                        oColumn.Add("crs", oCRSRoot);
                    }
                    else
                    {
                        // WKT was used in GeoParquet <= 0.3.0
                        const char *const apszOptions[] = {
                            "FORMAT=WKT2_2019", "MULTILINE=NO", nullptr};
                        char *pszWKT = nullptr;
                        oSRSIdentified.exportToWkt(&pszWKT, apszOptions);
                        if (pszWKT)
                            oColumn.Add("crs", pszWKT);
                        CPLFree(pszWKT);
                    }

                    const double dfCoordEpoch = poSRS->GetCoordinateEpoch();
                    if (dfCoordEpoch > 0)
                        oColumn.Add("epoch", dfCoordEpoch);
                }
                else
                {
                    oColumn.AddNull("crs");
                }
            }

            if (m_bEdgesSpherical)
            {
                oColumn.Add("edges", "spherical");
            }

            if (m_aoEnvelopes[i].IsInit() &&
                CPLTestBool(
                    CPLGetConfigOption("OGR_PARQUET_WRITE_BBOX", "YES")))
            {
                bool bHasZ = false;
                for (const auto eGeomType : m_oSetWrittenGeometryTypes[i])
                {
                    bHasZ = OGR_GT_HasZ(eGeomType);
                    if (bHasZ)
                        break;
                }
                CPLJSONArray oBBOX;
                oBBOX.Add(m_aoEnvelopes[i].MinX);
                oBBOX.Add(m_aoEnvelopes[i].MinY);
                if (bHasZ)
                    oBBOX.Add(m_aoEnvelopes[i].MinZ);
                oBBOX.Add(m_aoEnvelopes[i].MaxX);
                oBBOX.Add(m_aoEnvelopes[i].MaxY);
                if (bHasZ)
                    oBBOX.Add(m_aoEnvelopes[i].MaxZ);
                oColumn.Add("bbox", oBBOX);
            }

            // Bounding box column definition
            if (m_bWriteBBoxStruct &&
                CPLTestBool(CPLGetConfigOption(
                    "OGR_PARQUET_WRITE_COVERING_BBOX_IN_METADATA", "YES")))
            {
                CPLJSONObject oCovering;
                oColumn.Add("covering", oCovering);
                CPLJSONObject oBBOX;
                oCovering.Add("bbox", oBBOX);
                const auto AddComponent =
                    [this, i, &oBBOX](const char *pszComponent)
                {
                    CPLJSONArray oArray;
                    oArray.Add(m_apoFieldsBBOX[i]->name());
                    oArray.Add(pszComponent);
                    oBBOX.Add(pszComponent, oArray);
                };
                AddComponent("xmin");
                AddComponent("ymin");
                AddComponent("xmax");
                AddComponent("ymax");
            }

            const auto GetStringGeometryType = [](OGRwkbGeometryType eType)
            {
                const auto eFlattenType = wkbFlatten(eType);
                std::string osType = "Unknown";
                if (wkbPoint == eFlattenType)
                    osType = "Point";
                else if (wkbLineString == eFlattenType)
                    osType = "LineString";
                else if (wkbPolygon == eFlattenType)
                    osType = "Polygon";
                else if (wkbMultiPoint == eFlattenType)
                    osType = "MultiPoint";
                else if (wkbMultiLineString == eFlattenType)
                    osType = "MultiLineString";
                else if (wkbMultiPolygon == eFlattenType)
                    osType = "MultiPolygon";
                else if (wkbGeometryCollection == eFlattenType)
                    osType = "GeometryCollection";
                if (osType != "Unknown")
                {
                    // M and ZM not supported officially currently, but it
                    // doesn't hurt to anticipate
                    if (OGR_GT_HasZ(eType) && OGR_GT_HasM(eType))
                        osType += " ZM";
                    else if (OGR_GT_HasZ(eType))
                        osType += " Z";
                    else if (OGR_GT_HasM(eType))
                        osType += " M";
                }
                return osType;
            };

            if (m_bForceCounterClockwiseOrientation)
                oColumn.Add("orientation", "counterclockwise");

            CPLJSONArray oArray;
            for (const auto eType : m_oSetWrittenGeometryTypes[i])
            {
                oArray.Add(GetStringGeometryType(eType));
            }
            oColumn.Add("geometry_types", oArray);
        }

        return oRoot.Format(CPLJSONObject::PrettyFormat::Plain);
    }
    return std::string();
}

/************************************************************************/
/*               PerformStepsBeforeFinalFlushGroup()                    */
/************************************************************************/

void OGRParquetWriterLayer::PerformStepsBeforeFinalFlushGroup()
{
    if (m_poKeyValueMetadata)
    {
        const std::string osGeoMetadata = GetGeoMetadata();
        auto poTmpSchema = m_poSchema;
        if (!osGeoMetadata.empty())
        {
            // HACK: it would be good for Arrow to provide a clean way to alter
            // key value metadata before finalizing.
            // We need to write metadata at end to write the bounding box.
            const_cast<arrow::KeyValueMetadata *>(m_poKeyValueMetadata.get())
                ->Append("geo", osGeoMetadata);

            auto kvMetadata = poTmpSchema->metadata()
                                  ? poTmpSchema->metadata()->Copy()
                                  : std::make_shared<arrow::KeyValueMetadata>();
            kvMetadata->Append("geo", osGeoMetadata);
            poTmpSchema = poTmpSchema->WithMetadata(kvMetadata);
        }

        if (CPLTestBool(
                CPLGetConfigOption("OGR_PARQUET_WRITE_ARROW_SCHEMA", "YES")))
        {
            auto status =
                ::arrow::ipc::SerializeSchema(*poTmpSchema, m_poMemoryPool);
            if (status.ok())
            {
                // The serialized schema is not UTF-8, which is required for
                // Thrift
                const std::string schema_as_string = (*status)->ToString();
                const std::string schema_base64 =
                    ::arrow::util::base64_encode(schema_as_string);
                static const std::string kArrowSchemaKey = "ARROW:schema";
                const_cast<arrow::KeyValueMetadata *>(
                    m_poKeyValueMetadata.get())
                    ->Append(kArrowSchemaKey, schema_base64);
            }
        }

        // Put GDAL metadata into a gdal:metadata domain
        CPLJSONObject oMultiMetadata;
        bool bHasMultiMetadata = false;
        auto &l_oMDMD = oMDMD.GetDomainList() && *(oMDMD.GetDomainList())
                            ? oMDMD
                            : m_poDataset->GetMultiDomainMetadata();
        for (CSLConstList papszDomainIter = l_oMDMD.GetDomainList();
             papszDomainIter && *papszDomainIter; ++papszDomainIter)
        {
            const char *pszDomain = *papszDomainIter;
            CSLConstList papszMD = l_oMDMD.GetMetadata(pszDomain);
            if (STARTS_WITH(pszDomain, "json:") && papszMD && papszMD[0])
            {
                CPLJSONDocument oDoc;
                if (oDoc.LoadMemory(papszMD[0]))
                {
                    bHasMultiMetadata = true;
                    oMultiMetadata.Add(pszDomain, oDoc.GetRoot());
                    continue;
                }
            }
            else if (STARTS_WITH(pszDomain, "xml:") && papszMD && papszMD[0])
            {
                bHasMultiMetadata = true;
                oMultiMetadata.Add(pszDomain, papszMD[0]);
                continue;
            }
            CPLJSONObject oMetadata;
            bool bHasMetadata = false;
            for (CSLConstList papszMDIter = papszMD;
                 papszMDIter && *papszMDIter; ++papszMDIter)
            {
                char *pszKey = nullptr;
                const char *pszValue = CPLParseNameValue(*papszMDIter, &pszKey);
                if (pszKey && pszValue)
                {
                    bHasMetadata = true;
                    bHasMultiMetadata = true;
                    oMetadata.Add(pszKey, pszValue);
                }
                CPLFree(pszKey);
            }
            if (bHasMetadata)
                oMultiMetadata.Add(pszDomain, oMetadata);
        }
        if (bHasMultiMetadata)
        {
            const_cast<arrow::KeyValueMetadata *>(m_poKeyValueMetadata.get())
                ->Append(
                    "gdal:metadata",
                    oMultiMetadata.Format(CPLJSONObject::PrettyFormat::Plain));
        }
    }
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

// Same as parquet::arrow::FileWriter::Open(), except we also
// return KeyValueMetadata
static arrow::Status
Open(const ::arrow::Schema &schema, ::arrow::MemoryPool *pool,
     std::shared_ptr<::arrow::io::OutputStream> sink,
     std::shared_ptr<parquet::WriterProperties> properties,
     std::shared_ptr<parquet::ArrowWriterProperties> arrow_properties,
     std::unique_ptr<parquet::arrow::FileWriter> *writer,
     std::shared_ptr<const arrow::KeyValueMetadata> *outMetadata)
{
    std::shared_ptr<parquet::SchemaDescriptor> parquet_schema;
    RETURN_NOT_OK(parquet::arrow::ToParquetSchema(
        &schema, *properties, *arrow_properties, &parquet_schema));

    auto schema_node = std::static_pointer_cast<parquet::schema::GroupNode>(
        parquet_schema->schema_root());

    auto metadata = schema.metadata()
                        ? schema.metadata()->Copy()
                        : std::make_shared<arrow::KeyValueMetadata>();
    *outMetadata = metadata;

    std::unique_ptr<parquet::ParquetFileWriter> base_writer;
    PARQUET_CATCH_NOT_OK(base_writer = parquet::ParquetFileWriter::Open(
                             std::move(sink), std::move(schema_node),
                             std::move(properties), metadata));

    auto schema_ptr = std::make_shared<::arrow::Schema>(schema);
    return parquet::arrow::FileWriter::Make(
        pool, std::move(base_writer), std::move(schema_ptr),
        std::move(arrow_properties), writer);
}

/************************************************************************/
/*                          CreateSchema()                              */
/************************************************************************/

void OGRParquetWriterLayer::CreateSchema()
{
    CreateSchemaCommon();
}

/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRParquetWriterLayer::CreateGeomField(const OGRGeomFieldDefn *poField,
                                              int bApproxOK)
{
    OGRErr eErr = OGRArrowWriterLayer::CreateGeomField(poField, bApproxOK);
    if (eErr == OGRERR_NONE &&
        m_aeGeomEncoding.back() == OGRArrowGeomEncoding::WKB)
    {
        m_oWriterPropertiesBuilder.disable_statistics(
            parquet::schema::ColumnPath::FromDotString(
                m_poFeatureDefn
                    ->GetGeomFieldDefn(m_poFeatureDefn->GetGeomFieldCount() - 1)
                    ->GetNameRef()));
    }
    return eErr;
}

/************************************************************************/
/*                          CreateWriter()                              */
/************************************************************************/

void OGRParquetWriterLayer::CreateWriter()
{
    CPLAssert(m_poFileWriter == nullptr);

    if (m_poSchema == nullptr)
    {
        CreateSchema();
    }
    else
    {
        FinalizeSchema();
    }

    auto arrowWriterProperties =
        parquet::ArrowWriterProperties::Builder().store_schema()->build();
    CPL_IGNORE_RET_VAL(Open(*m_poSchema, m_poMemoryPool, m_poOutputStream,
                            m_oWriterPropertiesBuilder.build(),
                            std::move(arrowWriterProperties), &m_poFileWriter,
                            &m_poKeyValueMetadata));
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr OGRParquetWriterLayer::ICreateFeature(OGRFeature *poFeature)
{
    // If not using SORT_BY_BBOX=YES layer creation option, we can directly
    // write features to the final Parquet file
    if (!m_poTmpGPKGLayer)
        return OGRArrowWriterLayer::ICreateFeature(poFeature);

    // SORT_BY_BBOX=YES case: we write for now a serialized version of poFeature
    // in a temporary GeoPackage file.

    GIntBig nFID = poFeature->GetFID();
    if (!m_osFIDColumn.empty() && nFID == OGRNullFID)
    {
        nFID = m_nTmpFeatureCount;
        poFeature->SetFID(nFID);
    }
    ++m_nTmpFeatureCount;

    std::vector<GByte> abyBuffer;
    // Serialize the source feature as a single array of bytes to preserve it
    // fully
    if (!poFeature->SerializeToBinary(abyBuffer))
    {
        return OGRERR_FAILURE;
    }

    // SQLite3 limitation: a row must fit in slightly less than 1 GB.
    constexpr int SOME_MARGIN = 128;
    if (abyBuffer.size() > 1024 * 1024 * 1024 - SOME_MARGIN)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Features larger than 1 GB are not supported");
        return OGRERR_FAILURE;
    }

    OGRFeature oFeat(m_poTmpGPKGLayer->GetLayerDefn());
    oFeat.SetFID(nFID);
    oFeat.SetField(0, static_cast<int>(abyBuffer.size()), abyBuffer.data());
    const auto poSrcGeom = poFeature->GetGeometryRef();
    if (poSrcGeom && !poSrcGeom->IsEmpty())
    {
        // For the purpose of building an RTree, just use the bounding box of
        // the geometry as the geometry.
        OGREnvelope sEnvelope;
        poSrcGeom->getEnvelope(&sEnvelope);
        auto poPoly = std::make_unique<OGRPolygon>();
        auto poLR = std::make_unique<OGRLinearRing>();
        poLR->addPoint(sEnvelope.MinX, sEnvelope.MinY);
        poLR->addPoint(sEnvelope.MinX, sEnvelope.MaxY);
        poLR->addPoint(sEnvelope.MaxX, sEnvelope.MaxY);
        poLR->addPoint(sEnvelope.MaxX, sEnvelope.MinY);
        poLR->addPoint(sEnvelope.MinX, sEnvelope.MinY);
        poPoly->addRingDirectly(poLR.release());
        oFeat.SetGeometryDirectly(poPoly.release());
    }
    return m_poTmpGPKGLayer->CreateFeature(&oFeat);
}

/************************************************************************/
/*                            FlushGroup()                              */
/************************************************************************/

bool OGRParquetWriterLayer::FlushGroup()
{
    auto status = m_poFileWriter->NewRowGroup(m_apoBuilders[0]->length());
    if (!status.ok())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "NewRowGroup() failed with %s",
                 status.message().c_str());
        ClearArrayBuilers();
        return false;
    }

    auto ret = WriteArrays(
        [this](const std::shared_ptr<arrow::Field> &field,
               const std::shared_ptr<arrow::Array> &array)
        {
            auto l_status = m_poFileWriter->WriteColumnChunk(*array);
            if (!l_status.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "WriteColumnChunk() failed for field %s: %s",
                         field->name().c_str(), l_status.message().c_str());
                return false;
            }
            return true;
        });

    ClearArrayBuilers();
    return ret;
}

/************************************************************************/
/*                    FixupWKBGeometryBeforeWriting()                   */
/************************************************************************/

void OGRParquetWriterLayer::FixupWKBGeometryBeforeWriting(GByte *pabyWkb,
                                                          size_t nLen)
{
    if (!m_bForceCounterClockwiseOrientation)
        return;

    OGRWKBFixupCounterClockWiseExternalRing(pabyWkb, nLen);
}

/************************************************************************/
/*                     FixupGeometryBeforeWriting()                     */
/************************************************************************/

void OGRParquetWriterLayer::FixupGeometryBeforeWriting(OGRGeometry *poGeom)
{
    if (!m_bForceCounterClockwiseOrientation)
        return;

    const auto eFlattenType = wkbFlatten(poGeom->getGeometryType());
    // Polygon rings MUST follow the right-hand rule for orientation
    // (counterclockwise external rings, clockwise internal rings)
    if (eFlattenType == wkbPolygon)
    {
        bool bFirstRing = true;
        for (auto poRing : poGeom->toPolygon())
        {
            if ((bFirstRing && poRing->isClockwise()) ||
                (!bFirstRing && !poRing->isClockwise()))
            {
                poRing->reverseWindingOrder();
            }
            bFirstRing = false;
        }
    }
    else if (eFlattenType == wkbMultiPolygon ||
             eFlattenType == wkbGeometryCollection)
    {
        for (auto poSubGeom : poGeom->toGeometryCollection())
        {
            FixupGeometryBeforeWriting(poSubGeom);
        }
    }
}

/************************************************************************/
/*                          WriteArrowBatch()                           */
/************************************************************************/

#if PARQUET_VERSION_MAJOR > 10
inline bool
OGRParquetWriterLayer::WriteArrowBatch(const struct ArrowSchema *schema,
                                       struct ArrowArray *array,
                                       CSLConstList papszOptions)
{
    if (m_poTmpGPKGLayer)
    {
        // When using SORT_BY_BBOX=YES option, we can't directly write the
        // input array, because we need to sort features. Hence we fallback
        // to the OGRLayer base implementation, which will ultimately call
        // OGRParquetWriterLayer::ICreateFeature()
        return OGRLayer::WriteArrowBatch(schema, array, papszOptions);
    }

    return WriteArrowBatchInternal(
        schema, array, papszOptions,
        [this](const std::shared_ptr<arrow::RecordBatch> &poBatch)
        {
            auto status = m_poFileWriter->NewBufferedRowGroup();
            if (!status.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "NewBufferedRowGroup() failed with %s",
                         status.message().c_str());
                return false;
            }

            status = m_poFileWriter->WriteRecordBatch(*poBatch);
            if (!status.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "WriteRecordBatch() failed: %s",
                         status.message().c_str());
                return false;
            }

            return true;
        });
}
#endif

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

inline int OGRParquetWriterLayer::TestCapability(const char *pszCap)
{
#if PARQUET_VERSION_MAJOR <= 10
    if (EQUAL(pszCap, OLCFastWriteArrowBatch))
        return false;
#endif

    if (m_poTmpGPKGLayer && EQUAL(pszCap, OLCFastWriteArrowBatch))
    {
        // When using SORT_BY_BBOX=YES option, we can't directly write the
        // input array, because we need to sort features. So this is not
        // fast
        return false;
    }

    return OGRArrowWriterLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                        CreateFieldFromArrowSchema()                  */
/************************************************************************/

#if PARQUET_VERSION_MAJOR > 10
bool OGRParquetWriterLayer::CreateFieldFromArrowSchema(
    const struct ArrowSchema *schema, CSLConstList papszOptions)
{
    if (m_poTmpGPKGLayer)
    {
        // When using SORT_BY_BBOX=YES option, we can't directly write the
        // input array, because we need to sort features. But this process
        // only supports the base Arrow types supported by
        // OGRLayer::WriteArrowBatch()
        return OGRLayer::CreateFieldFromArrowSchema(schema, papszOptions);
    }

    return OGRArrowWriterLayer::CreateFieldFromArrowSchema(schema,
                                                           papszOptions);
}
#endif

/************************************************************************/
/*                        IsArrowSchemaSupported()                      */
/************************************************************************/

#if PARQUET_VERSION_MAJOR > 10
bool OGRParquetWriterLayer::IsArrowSchemaSupported(
    const struct ArrowSchema *schema, CSLConstList papszOptions,
    std::string &osErrorMsg) const
{
    if (m_poTmpGPKGLayer)
    {
        // When using SORT_BY_BBOX=YES option, we can't directly write the
        // input array, because we need to sort features. But this process
        // only supports the base Arrow types supported by
        // OGRLayer::WriteArrowBatch()
        return OGRLayer::IsArrowSchemaSupported(schema, papszOptions,
                                                osErrorMsg);
    }

    if (schema->format[0] == 'e' && schema->format[1] == 0)
    {
        osErrorMsg = "float16 not supported";
        return false;
    }
    for (int64_t i = 0; i < schema->n_children; ++i)
    {
        if (!IsArrowSchemaSupported(schema->children[i], papszOptions,
                                    osErrorMsg))
        {
            return false;
        }
    }
    return true;
}
#endif

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr OGRParquetWriterLayer::SetMetadata(char **papszMetadata,
                                          const char *pszDomain)
{
    if (!pszDomain || !EQUAL(pszDomain, "SHAPEFILE"))
    {
        return OGRLayer::SetMetadata(papszMetadata, pszDomain);
    }
    return CE_None;
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRParquetWriterLayer::GetDataset()
{
    return m_poDataset;
}
