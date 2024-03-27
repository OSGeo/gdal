/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  GeoJSON feature sequence driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
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
#include "cpl_vsi_virtual.h"
#include "cpl_http.h"
#include "cpl_vsi_error.h"

#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonwriter.h"

#include <algorithm>
#include <memory>

constexpr char RS = '\x1e';

/************************************************************************/
/*                        OGRGeoJSONSeqDataSource                       */
/************************************************************************/

class OGRGeoJSONSeqDataSource final : public GDALDataset
{
    friend class OGRGeoJSONSeqLayer;

    std::vector<std::unique_ptr<OGRLayer>> m_apoLayers{};
    CPLString m_osTmpFile;
    VSILFILE *m_fp = nullptr;
    bool m_bSupportsRead = true;
    bool m_bAtEOF = false;
    bool m_bIsRSSeparated = false;

  public:
    OGRGeoJSONSeqDataSource();
    ~OGRGeoJSONSeqDataSource();

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    int TestCapability(const char *pszCap) override;

    bool Open(GDALOpenInfo *poOpenInfo, GeoJSONSourceType nSrcType);
    bool Create(const char *pszName, char **papszOptions);
};

/************************************************************************/
/*                           OGRGeoJSONSeqLayer                         */
/************************************************************************/

class OGRGeoJSONSeqLayer final : public OGRLayer
{
    OGRGeoJSONSeqDataSource *m_poDS = nullptr;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    bool m_bLayerDefnEstablished = false;
    bool m_bWriteOnlyLayer = false;

    OGRGeoJSONBaseReader m_oReader;
    CPLString m_osFIDColumn;

    size_t m_nMaxObjectSize = 0;
    std::string m_osBuffer;
    std::string m_osFeatureBuffer;
    size_t m_nPosInBuffer = 0;
    size_t m_nBufferValidSize = 0;

    vsi_l_offset m_nFileSize = 0;
    GIntBig m_nIter = 0;

    GIntBig m_nTotalFeatures = 0;
    GIntBig m_nNextFID = 0;

    std::unique_ptr<OGRCoordinateTransformation> m_poCT{};
    OGRGeometryFactory::TransformWithOptionsCache m_oTransformCache;
    OGRGeoJSONWriteOptions m_oWriteOptions;

    json_object *GetNextObject(bool bLooseIdentification);

  public:
    OGRGeoJSONSeqLayer(OGRGeoJSONSeqDataSource *poDS, const char *pszName);

    // Write-only constructor
    OGRGeoJSONSeqLayer(OGRGeoJSONSeqDataSource *poDS, const char *pszName,
                       CSLConstList papszOptions,
                       std::unique_ptr<OGRCoordinateTransformation> &&poCT);

    ~OGRGeoJSONSeqLayer();

    bool Init(bool bLooseIdentification, bool bEstablishLayerDefn);

    const char *GetName() override
    {
        return GetDescription();
    }

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeatureDefn *GetLayerDefn() override;

    const char *GetFIDColumn() override
    {
        return m_osFIDColumn.c_str();
    }

    GIntBig GetFeatureCount(int) override;
    int TestCapability(const char *) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *, int) override;

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

/************************************************************************/
/*                       OGRGeoJSONSeqDataSource()                      */
/************************************************************************/

OGRGeoJSONSeqDataSource::OGRGeoJSONSeqDataSource()
{
}

/************************************************************************/
/*                      ~OGRGeoJSONSeqDataSource()                      */
/************************************************************************/

OGRGeoJSONSeqDataSource::~OGRGeoJSONSeqDataSource()
{
    if (m_fp)
    {
        VSIFCloseL(m_fp);
    }
    if (!m_osTmpFile.empty())
    {
        VSIUnlink(m_osTmpFile);
    }
}

/************************************************************************/
/*                               GetLayer()                             */
/************************************************************************/

OGRLayer *OGRGeoJSONSeqDataSource::GetLayer(int nIndex)
{
    if (nIndex < 0 || nIndex >= GetLayerCount())
        return nullptr;
    return m_apoLayers[nIndex].get();
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRGeoJSONSeqDataSource::ICreateLayer(
    const char *pszNameIn, const OGRGeomFieldDefn *poSrcGeomFieldDefn,
    CSLConstList papszOptions)
{
    if (!TestCapability(ODsCCreateLayer))
        return nullptr;

    const auto poSRS =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetSpatialRef() : nullptr;

    std::unique_ptr<OGRCoordinateTransformation> poCT;
    if (poSRS == nullptr)
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "No SRS set on layer. Assuming it is long/lat on WGS84 ellipsoid");
    }
    else
    {
        OGRSpatialReference oSRSWGS84;
        oSRSWGS84.SetWellKnownGeogCS("WGS84");
        oSRSWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        const char *const apszOptions[] = {
            "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES", nullptr};
        if (!poSRS->IsSame(&oSRSWGS84, apszOptions))
        {
            poCT.reset(OGRCreateCoordinateTransformation(poSRS, &oSRSWGS84));
            if (poCT == nullptr)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Failed to create coordinate transformation between the "
                    "input coordinate system and WGS84.");

                return nullptr;
            }
        }
    }

    const char *pszRS = CSLFetchNameValue(papszOptions, "RS");
    if (pszRS)
    {
        m_bIsRSSeparated = CPLTestBool(pszRS);
    }

    CPLStringList aosOptions(papszOptions);

    double dfXYResolution = OGRGeomCoordinatePrecision::UNKNOWN;
    double dfZResolution = OGRGeomCoordinatePrecision::UNKNOWN;
    if (const char *pszCoordPrecision =
            CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION"))
    {
        dfXYResolution = std::pow(10.0, -CPLAtof(pszCoordPrecision));
        dfZResolution = dfXYResolution;
    }
    else if (poSrcGeomFieldDefn)
    {
        const auto &oCoordPrec = poSrcGeomFieldDefn->GetCoordinatePrecision();
        OGRSpatialReference oSRSWGS84;
        oSRSWGS84.SetWellKnownGeogCS("WGS84");
        const auto oCoordPrecWGS84 =
            oCoordPrec.ConvertToOtherSRS(poSRS, &oSRSWGS84);

        if (oCoordPrec.dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            dfXYResolution = oCoordPrecWGS84.dfXYResolution;

            aosOptions.SetNameValue(
                "XY_COORD_PRECISION",
                CPLSPrintf("%d",
                           OGRGeomCoordinatePrecision::ResolutionToPrecision(
                               dfXYResolution)));
        }
        if (oCoordPrec.dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            dfZResolution = oCoordPrecWGS84.dfZResolution;

            aosOptions.SetNameValue(
                "Z_COORD_PRECISION",
                CPLSPrintf("%d",
                           OGRGeomCoordinatePrecision::ResolutionToPrecision(
                               dfZResolution)));
        }
    }

    m_apoLayers.emplace_back(std::make_unique<OGRGeoJSONSeqLayer>(
        this, pszNameIn, aosOptions.List(), std::move(poCT)));

    auto poLayer = m_apoLayers.back().get();
    if (poLayer->GetGeomType() != wkbNone &&
        dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
        OGRGeomCoordinatePrecision oCoordPrec(
            poGeomFieldDefn->GetCoordinatePrecision());
        oCoordPrec.dfXYResolution = dfXYResolution;
        poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
    }

    if (poLayer->GetGeomType() != wkbNone &&
        dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
        OGRGeomCoordinatePrecision oCoordPrec(
            poGeomFieldDefn->GetCoordinatePrecision());
        oCoordPrec.dfZResolution = dfZResolution;
        poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
    }

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONSeqDataSource::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return eAccess == GA_Update;

    return FALSE;
}

/************************************************************************/
/*                           OGRGeoJSONSeqLayer()                       */
/************************************************************************/

OGRGeoJSONSeqLayer::OGRGeoJSONSeqLayer(OGRGeoJSONSeqDataSource *poDS,
                                       const char *pszName)
    : m_poDS(poDS)
{
    SetDescription(pszName);
    m_poFeatureDefn = new OGRFeatureDefn(pszName);
    m_poFeatureDefn->Reference();

    OGRSpatialReference *poSRSWGS84 = new OGRSpatialReference();
    poSRSWGS84->SetWellKnownGeogCS("WGS84");
    poSRSWGS84->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRSWGS84);
    poSRSWGS84->Release();

    const double dfTmp =
        CPLAtof(CPLGetConfigOption("OGR_GEOJSON_MAX_OBJ_SIZE", "200"));
    m_nMaxObjectSize = dfTmp > 0 ? static_cast<size_t>(dfTmp * 1024 * 1024) : 0;
}

/************************************************************************/
/*                           OGRGeoJSONSeqLayer()                       */
/************************************************************************/

// Write-only constructor
OGRGeoJSONSeqLayer::OGRGeoJSONSeqLayer(
    OGRGeoJSONSeqDataSource *poDS, const char *pszName,
    CSLConstList papszOptions,
    std::unique_ptr<OGRCoordinateTransformation> &&poCT)
    : m_poDS(poDS), m_bWriteOnlyLayer(true)
{
    m_bLayerDefnEstablished = true;

    SetDescription(pszName);
    m_poFeatureDefn = new OGRFeatureDefn(pszName);
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(
        OGRSpatialReference::GetWGS84SRS());
    m_poCT = std::move(poCT);

    m_oWriteOptions.SetRFC7946Settings();
    m_oWriteOptions.SetIDOptions(papszOptions);

    const char *pszCoordPrecision =
        CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION");
    if (pszCoordPrecision)
    {
        m_oWriteOptions.nXYCoordPrecision = atoi(pszCoordPrecision);
        m_oWriteOptions.nZCoordPrecision = atoi(pszCoordPrecision);
    }
    else
    {
        m_oWriteOptions.nXYCoordPrecision =
            atoi(CSLFetchNameValueDef(papszOptions, "XY_COORD_PRECISION", "7"));
        m_oWriteOptions.nZCoordPrecision =
            atoi(CSLFetchNameValueDef(papszOptions, "Z_COORD_PRECISION", "3"));
    }

    m_oWriteOptions.nSignificantFigures =
        atoi(CSLFetchNameValueDef(papszOptions, "SIGNIFICANT_FIGURES", "-1"));
    m_oWriteOptions.bAllowNonFiniteValues = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_NON_FINITE_VALUES", "FALSE"));
    m_oWriteOptions.bAutodetectJsonStrings = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "AUTODETECT_JSON_STRINGS", "TRUE"));
}

/************************************************************************/
/*                          ~OGRGeoJSONSeqLayer()                       */
/************************************************************************/

OGRGeoJSONSeqLayer::~OGRGeoJSONSeqLayer()
{
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn *OGRGeoJSONSeqLayer::GetLayerDefn()
{
    if (!m_bLayerDefnEstablished)
    {
        Init(/* bLooseIdentification = */ false,
             /* bEstablishLayerDefn = */ true);
    }
    return m_poFeatureDefn;
}

/************************************************************************/
/*                               Init()                                 */
/************************************************************************/

bool OGRGeoJSONSeqLayer::Init(bool bLooseIdentification,
                              bool bEstablishLayerDefn)
{
    if (STARTS_WITH(m_poDS->GetDescription(), "/vsimem/") ||
        !STARTS_WITH(m_poDS->GetDescription(), "/vsi"))
    {
        VSIFSeekL(m_poDS->m_fp, 0, SEEK_END);
        m_nFileSize = VSIFTellL(m_poDS->m_fp);
    }

    // Set m_bLayerDefnEstablished = true early to avoid infinite recursive
    // calls.
    if (bEstablishLayerDefn)
        m_bLayerDefnEstablished = true;

    ResetReading();

    std::map<std::string, int> oMapFieldNameToIdx;
    std::vector<std::unique_ptr<OGRFieldDefn>> apoFieldDefn;
    gdal::DirectedAcyclicGraph<int, std::string> dag;
    bool bOK = false;

    while (true)
    {
        auto poObject = GetNextObject(bLooseIdentification);
        if (!poObject)
            break;
        const auto eObjectType = OGRGeoJSONGetType(poObject);
        if (bEstablishLayerDefn && eObjectType == GeoJSONObject::eFeature)
        {
            m_oReader.GenerateFeatureDefn(oMapFieldNameToIdx, apoFieldDefn, dag,
                                          this, poObject);
        }
        json_object_put(poObject);
        if (!bEstablishLayerDefn)
        {
            bOK = (eObjectType == GeoJSONObject::eFeature);
            break;
        }
        m_nTotalFeatures++;
    }

    if (bEstablishLayerDefn)
    {
        // CPLDebug("GEOJSONSEQ", "Establish layer definition");

        const auto sortedFields = dag.getTopologicalOrdering();
        CPLAssert(sortedFields.size() == apoFieldDefn.size());
        for (int idx : sortedFields)
        {
            m_poFeatureDefn->AddFieldDefn(apoFieldDefn[idx].get());
        }
        m_poFeatureDefn->Seal(true);
        m_oReader.FinalizeLayerDefn(this, m_osFIDColumn);
    }

    ResetReading();

    m_nFileSize = 0;
    m_nIter = 0;

    return bOK || m_nTotalFeatures > 0;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoJSONSeqLayer::ResetReading()
{
    if (!m_poDS->m_bSupportsRead ||
        (m_bWriteOnlyLayer && m_poDS->m_apoLayers.size() > 1))
    {
        return;
    }

    m_poDS->m_bAtEOF = false;
    VSIFSeekL(m_poDS->m_fp, 0, SEEK_SET);
    // Undocumented: for testing purposes only
    const size_t nBufferSize = static_cast<size_t>(std::max(
        1, atoi(CPLGetConfigOption("OGR_GEOJSONSEQ_CHUNK_SIZE", "40960"))));
    const size_t nBufferSizeValidated =
        nBufferSize > static_cast<size_t>(100 * 1000 * 1000)
            ? static_cast<size_t>(100 * 1000 * 1000)
            : nBufferSize;
    m_osBuffer.resize(nBufferSizeValidated);
    m_osFeatureBuffer.clear();
    m_nPosInBuffer = nBufferSizeValidated;
    m_nBufferValidSize = nBufferSizeValidated;
    m_nNextFID = 0;
}

/************************************************************************/
/*                           GetNextObject()                            */
/************************************************************************/

json_object *OGRGeoJSONSeqLayer::GetNextObject(bool bLooseIdentification)
{
    m_osFeatureBuffer.clear();
    while (true)
    {
        // If we read all the buffer, then reload it from file
        if (m_nPosInBuffer >= m_nBufferValidSize)
        {
            if (m_nBufferValidSize < m_osBuffer.size())
            {
                return nullptr;
            }
            m_nBufferValidSize =
                VSIFReadL(&m_osBuffer[0], 1, m_osBuffer.size(), m_poDS->m_fp);
            m_nPosInBuffer = 0;
            if (VSIFTellL(m_poDS->m_fp) == m_nBufferValidSize &&
                m_nBufferValidSize > 0)
            {
                m_poDS->m_bIsRSSeparated = (m_osBuffer[0] == RS);
                if (m_poDS->m_bIsRSSeparated)
                {
                    m_nPosInBuffer++;
                }
            }
            m_nIter++;

            if (m_nFileSize > 0 && (m_nBufferValidSize < m_osBuffer.size() ||
                                    (m_nIter % 100) == 0))
            {
                CPLDebug("GeoJSONSeq", "First pass: %.2f %%",
                         100.0 * VSIFTellL(m_poDS->m_fp) / m_nFileSize);
            }
            if (m_nPosInBuffer >= m_nBufferValidSize)
            {
                return nullptr;
            }
        }

        // Find next feature separator in buffer
        const size_t nNextSepPos = m_osBuffer.find(
            m_poDS->m_bIsRSSeparated ? RS : '\n', m_nPosInBuffer);
        if (nNextSepPos != std::string::npos)
        {
            m_osFeatureBuffer.append(m_osBuffer.data() + m_nPosInBuffer,
                                     nNextSepPos - m_nPosInBuffer);
            m_nPosInBuffer = nNextSepPos + 1;
        }
        else
        {
            // No separator ? then accummulate
            m_osFeatureBuffer.append(m_osBuffer.data() + m_nPosInBuffer,
                                     m_nBufferValidSize - m_nPosInBuffer);
            if (m_nMaxObjectSize > 0 &&
                m_osFeatureBuffer.size() > m_nMaxObjectSize)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Too large feature. You may define the "
                         "OGR_GEOJSON_MAX_OBJ_SIZE configuration option to "
                         "a value in megabytes (larger than %u) to allow "
                         "for larger features, or 0 to remove any size limit.",
                         static_cast<unsigned>(m_osFeatureBuffer.size() / 1024 /
                                               1024));
                return nullptr;
            }
            m_nPosInBuffer = m_nBufferValidSize;
            if (m_nBufferValidSize == m_osBuffer.size())
            {
                continue;
            }
        }

        while (!m_osFeatureBuffer.empty() &&
               (m_osFeatureBuffer.back() == '\r' ||
                m_osFeatureBuffer.back() == '\n'))
        {
            m_osFeatureBuffer.resize(m_osFeatureBuffer.size() - 1);
        }
        if (!m_osFeatureBuffer.empty())
        {
            json_object *poObject = nullptr;
            CPL_IGNORE_RET_VAL(
                OGRJSonParse(m_osFeatureBuffer.c_str(), &poObject));
            m_osFeatureBuffer.clear();
            if (json_object_get_type(poObject) == json_type_object)
            {
                return poObject;
            }
            json_object_put(poObject);
            if (bLooseIdentification)
            {
                return nullptr;
            }
        }
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoJSONSeqLayer::GetNextFeature()
{
    if (!m_poDS->m_bSupportsRead)
    {
        return nullptr;
    }
    if (m_bWriteOnlyLayer && m_poDS->m_apoLayers.size() > 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetNextFeature() not supported when appending a new layer");
        return nullptr;
    }

    GetLayerDefn();  // force scan if not already done
    while (true)
    {
        auto poObject = GetNextObject(false);
        if (!poObject)
            return nullptr;
        OGRFeature *poFeature;
        auto type = OGRGeoJSONGetType(poObject);
        if (type == GeoJSONObject::eFeature)
        {
            poFeature = m_oReader.ReadFeature(this, poObject,
                                              m_osFeatureBuffer.c_str());
            json_object_put(poObject);
        }
        else if (type == GeoJSONObject::eFeatureCollection ||
                 type == GeoJSONObject::eUnknown)
        {
            json_object_put(poObject);
            continue;
        }
        else
        {
            OGRGeometry *poGeom =
                m_oReader.ReadGeometry(poObject, GetSpatialRef());
            json_object_put(poObject);
            if (!poGeom)
            {
                continue;
            }
            poFeature = new OGRFeature(m_poFeatureDefn);
            poFeature->SetGeometryDirectly(poGeom);
        }

        if (poFeature->GetFID() == OGRNullFID)
        {
            poFeature->SetFID(m_nNextFID);
            m_nNextFID++;
        }
        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter))) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)))
        {
            return poFeature;
        }
        delete poFeature;
    }
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRGeoJSONSeqLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
    {
        GetLayerDefn();  // force scan if not already done
        return m_nTotalFeatures;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONSeqLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return true;
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr &&
        EQUAL(pszCap, OLCFastFeatureCount))
    {
        return true;
    }
    if (EQUAL(pszCap, OLCCreateField) || EQUAL(pszCap, OLCSequentialWrite))
    {
        return m_poDS->GetAccess() == GA_Update;
    }

    return false;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRGeoJSONSeqLayer::ICreateFeature(OGRFeature *poFeature)
{
    if (m_poDS->GetAccess() != GA_Update)
        return OGRERR_FAILURE;

    if (!m_poDS->m_bAtEOF)
    {
        m_poDS->m_bAtEOF = true;
        VSIFSeekL(m_poDS->m_fp, 0, SEEK_END);
    }

    std::unique_ptr<OGRFeature> poFeatureToWrite;
    if (m_poCT != nullptr)
    {
        poFeatureToWrite.reset(new OGRFeature(m_poFeatureDefn));
        poFeatureToWrite->SetFrom(poFeature);
        poFeatureToWrite->SetFID(poFeature->GetFID());
        OGRGeometry *poGeometry = poFeatureToWrite->GetGeometryRef();
        if (poGeometry)
        {
            const char *const apszOptions[] = {"WRAPDATELINE=YES", nullptr};
            OGRGeometry *poNewGeom = OGRGeometryFactory::transformWithOptions(
                poGeometry, m_poCT.get(), const_cast<char **>(apszOptions),
                m_oTransformCache);
            if (poNewGeom == nullptr)
            {
                return OGRERR_FAILURE;
            }

            OGREnvelope sEnvelope;
            poNewGeom->getEnvelope(&sEnvelope);
            if (sEnvelope.MinX < -180.0 || sEnvelope.MaxX > 180.0 ||
                sEnvelope.MinY < -90.0 || sEnvelope.MaxY > 90.0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry extent outside of "
                         "[-180.0,180.0]x[-90.0,90.0] bounds");
                return OGRERR_FAILURE;
            }

            poFeatureToWrite->SetGeometryDirectly(poNewGeom);
        }
    }

    ++m_nTotalFeatures;

    json_object *poObj = OGRGeoJSONWriteFeature(
        poFeatureToWrite.get() ? poFeatureToWrite.get() : poFeature,
        m_oWriteOptions);
    CPLAssert(nullptr != poObj);

    const char *pszJson = json_object_to_json_string(poObj);

    char chEOL = '\n';
    OGRErr eErr = OGRERR_NONE;
    if ((m_poDS->m_bIsRSSeparated &&
         VSIFWriteL(&RS, 1, 1, m_poDS->m_fp) != 1) ||
        VSIFWriteL(pszJson, strlen(pszJson), 1, m_poDS->m_fp) != 1 ||
        VSIFWriteL(&chEOL, 1, 1, m_poDS->m_fp) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot write feature");
        eErr = OGRERR_FAILURE;
    }

    json_object_put(poObj);

    return eErr;
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr OGRGeoJSONSeqLayer::CreateField(const OGRFieldDefn *poField,
                                       int /* bApproxOK */)
{
    if (m_poDS->GetAccess() != GA_Update)
        return OGRERR_FAILURE;
    m_poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool OGRGeoJSONSeqDataSource::Open(GDALOpenInfo *poOpenInfo,
                                   GeoJSONSourceType nSrcType)
{
    CPLAssert(nullptr == m_fp);

    CPLString osLayerName("GeoJSONSeq");

    const char *pszUnprefixedFilename = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSONSeq:"))
    {
        pszUnprefixedFilename = poOpenInfo->pszFilename + strlen("GeoJSONSeq:");
    }

    if (nSrcType == eGeoJSONSourceFile)
    {
        if (pszUnprefixedFilename != poOpenInfo->pszFilename)
        {
            osLayerName = CPLGetBasename(pszUnprefixedFilename);
            m_fp = VSIFOpenL(pszUnprefixedFilename,
                             poOpenInfo->eAccess == GA_Update ? "rb+" : "rb");
        }
        else
        {
            osLayerName = CPLGetBasename(poOpenInfo->pszFilename);
            std::swap(m_fp, poOpenInfo->fpL);
        }
    }
    else if (nSrcType == eGeoJSONSourceText)
    {
        if (poOpenInfo->eAccess == GA_Update)
            return false;

        m_osTmpFile = CPLSPrintf("/vsimem/geojsonseq/%p", this);
        m_fp = VSIFileFromMemBuffer(
            m_osTmpFile.c_str(),
            reinterpret_cast<GByte *>(CPLStrdup(poOpenInfo->pszFilename)),
            strlen(poOpenInfo->pszFilename), true);
    }
    else if (nSrcType == eGeoJSONSourceService)
    {
        if (poOpenInfo->eAccess == GA_Update)
            return false;

        char *pszStoredContent =
            OGRGeoJSONDriverStealStoredContent(pszUnprefixedFilename);
        if (pszStoredContent)
        {
            if (!GeoJSONSeqIsObject(pszStoredContent))
            {
                OGRGeoJSONDriverStoreContent(poOpenInfo->pszFilename,
                                             pszStoredContent);
                return false;
            }
            else
            {
                m_osTmpFile = CPLSPrintf("/vsimem/geojsonseq/%p", this);
                m_fp = VSIFileFromMemBuffer(
                    m_osTmpFile.c_str(),
                    reinterpret_cast<GByte *>(pszStoredContent),
                    strlen(pszStoredContent), true);
            }
        }
        else
        {
            const char *const papsOptions[] = {
                "HEADERS=Accept: text/plain, application/json", nullptr};

            CPLHTTPResult *pResult =
                CPLHTTPFetch(pszUnprefixedFilename, papsOptions);

            if (nullptr == pResult || 0 == pResult->nDataLen ||
                0 != CPLGetLastErrorNo())
            {
                CPLHTTPDestroyResult(pResult);
                return false;
            }

            if (0 != pResult->nStatus)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Curl reports error: %d: %s", pResult->nStatus,
                         pResult->pszErrBuf);
                CPLHTTPDestroyResult(pResult);
                return false;
            }

            m_osTmpFile = CPLSPrintf("/vsimem/geojsonseq/%p", this);
            m_fp = VSIFileFromMemBuffer(m_osTmpFile.c_str(), pResult->pabyData,
                                        pResult->nDataLen, true);
            pResult->pabyData = nullptr;
            pResult->nDataLen = 0;
            CPLHTTPDestroyResult(pResult);
        }
    }
    if (m_fp == nullptr)
    {
        return false;
    }
    SetDescription(poOpenInfo->pszFilename);
    auto poLayer = new OGRGeoJSONSeqLayer(this, osLayerName.c_str());
    const bool bLooseIdentification =
        nSrcType == eGeoJSONSourceService &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSONSeq:");
    if (bLooseIdentification)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
    }
    const bool bEstablishLayerDefn = poOpenInfo->eAccess != GA_Update;
    auto ret = poLayer->Init(bLooseIdentification, bEstablishLayerDefn);
    if (bLooseIdentification)
    {
        CPLPopErrorHandler();
        CPLErrorReset();
    }
    if (!ret)
    {
        delete poLayer;
        return false;
    }
    m_apoLayers.emplace_back(std::move(poLayer));
    eAccess = poOpenInfo->eAccess;
    return true;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

bool OGRGeoJSONSeqDataSource::Create(const char *pszName,
                                     char ** /* papszOptions */)
{
    CPLAssert(nullptr == m_fp);

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    m_bSupportsRead =
        VSIFileManager::GetHandler(pszName)->SupportsRead(pszName) &&
        VSIFileManager::GetHandler(pszName)->SupportsRandomWrite(pszName,
                                                                 false);
    m_bAtEOF = !m_bSupportsRead;
    m_fp = VSIFOpenExL(pszName, m_bSupportsRead ? "wb+" : "wb", true);
    if (nullptr == m_fp)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Failed to create %s: %s",
                 pszName, VSIGetLastErrorMsg());
        return false;
    }

    eAccess = GA_Update;

    m_bIsRSSeparated = EQUAL(CPLGetExtension(pszName), "GEOJSONS");

    return true;
}

/************************************************************************/
/*                       OGRGeoJSONSeqDriverIdentify()                  */
/************************************************************************/

static int OGRGeoJSONSeqDriverIdentifyInternal(GDALOpenInfo *poOpenInfo,
                                               GeoJSONSourceType &nSrcType)
{
    nSrcType = GeoJSONSeqGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
        return FALSE;
    if (nSrcType == eGeoJSONSourceService &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSONSeq:"))
    {
        return -1;
    }
    return TRUE;
}

/************************************************************************/
/*                      OGRGeoJSONSeqDriverIdentify()                   */
/************************************************************************/

static int OGRGeoJSONSeqDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType;
    return OGRGeoJSONSeqDriverIdentifyInternal(poOpenInfo, nSrcType);
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset *OGRGeoJSONSeqDriverOpen(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType;
    if (OGRGeoJSONSeqDriverIdentifyInternal(poOpenInfo, nSrcType) == FALSE)
    {
        return nullptr;
    }

    OGRGeoJSONSeqDataSource *poDS = new OGRGeoJSONSeqDataSource();

    if (!poDS->Open(poOpenInfo, nSrcType))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *
OGRGeoJSONSeqDriverCreate(const char *pszName, int /* nBands */,
                          int /* nXSize */, int /* nYSize */,
                          GDALDataType /* eDT */, char **papszOptions)
{
    OGRGeoJSONSeqDataSource *poDS = new OGRGeoJSONSeqDataSource();

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                        RegisterOGRGeoJSONSeq()                       */
/************************************************************************/

void RegisterOGRGeoJSONSeq()
{
    if (GDALGetDriverByName("GeoJSONSeq") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("GeoJSONSeq");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GeoJSON Sequence");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "geojsonl geojsons");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/geojsonseq.html");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='RS' type='boolean' description='whether to prefix "
        "records with RS=0x1e character' default='NO'/>"
        "  <Option name='COORDINATE_PRECISION' type='int' description='Number "
        "of decimal for coordinates. Default is 7'/>"
        "  <Option name='SIGNIFICANT_FIGURES' type='int' description='Number "
        "of significant figures for floating-point values' default='17'/>"
        "  <Option name='ID_FIELD' type='string' description='Name of the "
        "source field that must be used as the id member of Feature features'/>"
        "  <Option name='ID_TYPE' type='string-select' description='Type of "
        "the id member of Feature features'>"
        "    <Value>AUTO</Value>"
        "    <Value>String</Value>"
        "    <Value>Integer</Value>"
        "  </Option>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String IntegerList "
                              "Integer64List RealList StringList");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION, "YES");

    poDriver->pfnOpen = OGRGeoJSONSeqDriverOpen;
    poDriver->pfnIdentify = OGRGeoJSONSeqDriverIdentify;
    poDriver->pfnCreate = OGRGeoJSONSeqDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
