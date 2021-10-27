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
#include "cpl_http.h"
#include "cpl_vsi_error.h"

#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonwriter.h"

#include <algorithm>
#include <memory>

CPL_CVSID("$Id$")

constexpr char RS = '\x1e';

/************************************************************************/
/*                        OGRGeoJSONSeqDataSource                       */
/************************************************************************/

class OGRGeoJSONSeqDataSource final: public GDALDataset
{
        std::unique_ptr<OGRLayer> m_poLayer;
        CPLString m_osTmpFile;
        VSILFILE* m_fpOut = nullptr;

    public:
        OGRGeoJSONSeqDataSource();
        ~OGRGeoJSONSeqDataSource();

        int GetLayerCount() override { return m_poLayer ? 1 : 0; }
        OGRLayer* GetLayer(int) override;
        OGRLayer* ICreateLayer( const char* pszName,
                                OGRSpatialReference* poSRS = nullptr,
                                OGRwkbGeometryType eGType = wkbUnknown,
                                char** papszOptions = nullptr ) override;
        int TestCapability( const char* pszCap ) override;

        bool Open( GDALOpenInfo* poOpenInfo, GeoJSONSourceType nSrcType);
        bool Create( const char* pszName, char** papszOptions );

        VSILFILE* GetOutputFile() const { return m_fpOut; }
};

/************************************************************************/
/*                           OGRGeoJSONSeqLayer                         */
/************************************************************************/

class OGRGeoJSONSeqLayer final: public OGRLayer
{
        OGRGeoJSONSeqDataSource* m_poDS = nullptr;
        OGRFeatureDefn* m_poFeatureDefn = nullptr;

        VSILFILE* m_fp = nullptr;
        OGRGeoJSONBaseReader m_oReader;
        CPLString m_osFIDColumn;

        std::string m_osBuffer;
        std::string m_osFeatureBuffer;
        size_t m_nPosInBuffer = 0;
        size_t m_nBufferValidSize = 0;

        vsi_l_offset m_nFileSize = 0;
        GIntBig m_nIter = 0;

        bool m_bIsRSSeparated = false;

        GIntBig m_nTotalFeatures = 0;
        GIntBig m_nNextFID = 0;

        json_object* GetNextObject(bool bLooseIdentification);

    public:
        OGRGeoJSONSeqLayer(OGRGeoJSONSeqDataSource* poDS,
                           const char* pszName,
                           VSILFILE* fp);
        ~OGRGeoJSONSeqLayer();

        bool Init(bool bLooseIdentification);

        void ResetReading() override;
        OGRFeature* GetNextFeature() override;
        OGRFeatureDefn* GetLayerDefn() override { return m_poFeatureDefn; }
        const char* GetFIDColumn() override { return m_osFIDColumn.c_str(); }
        GIntBig GetFeatureCount(int) override;
        int TestCapability(const char*) override;
};

/************************************************************************/
/*                       OGRGeoJSONSeqWriteLayer                        */
/************************************************************************/

class OGRGeoJSONSeqWriteLayer final: public OGRLayer
{
  public:
    OGRGeoJSONSeqWriteLayer( OGRGeoJSONSeqDataSource* poDS,
                             const char* pszName,
                             CSLConstList papszOptions,
                             OGRCoordinateTransformation* poCT);
    ~OGRGeoJSONSeqWriteLayer();

    OGRFeatureDefn* GetLayerDefn() override { return m_poFeatureDefn; }

    void ResetReading() override { }
    OGRFeature* GetNextFeature() override { return nullptr; }
    OGRErr ICreateFeature( OGRFeature* poFeature ) override;
    OGRErr CreateField( OGRFieldDefn* poField, int bApproxOK ) override;
    int TestCapability( const char* pszCap ) override;

  private:
    OGRGeoJSONSeqDataSource* m_poDS = nullptr;
    OGRFeatureDefn* m_poFeatureDefn = nullptr;

    OGRCoordinateTransformation* m_poCT = nullptr;
    OGRGeometryFactory::TransformWithOptionsCache m_oTransformCache;
    OGRGeoJSONWriteOptions m_oWriteOptions;
    bool m_bRS = false;
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
    if( m_fpOut )
    {
        VSIFCloseL(m_fpOut);
    }
    if( !m_osTmpFile.empty() )
    {
        VSIUnlink(m_osTmpFile);
    }
}

/************************************************************************/
/*                               GetLayer()                             */
/************************************************************************/

OGRLayer* OGRGeoJSONSeqDataSource::GetLayer(int nIndex)
{
    return nIndex == 0 ? m_poLayer.get() : nullptr;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer* OGRGeoJSONSeqDataSource::ICreateLayer( const char* pszNameIn,
                                              OGRSpatialReference* poSRS,
                                              OGRwkbGeometryType /*eGType*/,
                                              char** papszOptions )
{
    if( nullptr == m_fpOut )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSONSeq driver doesn't support creating a layer "
                 "on a read-only datasource");
        return nullptr;
    }

    if( m_poLayer.get() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSONSeq driver doesn't support creating more than one layer");
        return nullptr;
    }

    OGRCoordinateTransformation* poCT = nullptr;
    if( poSRS == nullptr )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                    "No SRS set on layer. Assuming it is long/lat on WGS84 ellipsoid");
    }
    else
    {
        OGRSpatialReference oSRSWGS84;
        oSRSWGS84.SetWellKnownGeogCS( "WGS84" );
        oSRSWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        const char* const apszOptions[] = {
            "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES", nullptr };
        if( !poSRS->IsSame(&oSRSWGS84, apszOptions) )
        {
            poCT = OGRCreateCoordinateTransformation( poSRS, &oSRSWGS84 );
            if( poCT == nullptr )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Failed to create coordinate transformation between the "
                    "input coordinate system and WGS84." );

                return nullptr;
            }
        }
    }

    m_poLayer.reset(
        new OGRGeoJSONSeqWriteLayer( this, pszNameIn, papszOptions, poCT ));
    return m_poLayer.get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONSeqDataSource::TestCapability( const char* pszCap )
{
    if( EQUAL( pszCap, ODsCCreateLayer ) )
        return m_fpOut != nullptr && m_poLayer.get() == nullptr;

    return FALSE;
}

/************************************************************************/
/*                           OGRGeoJSONSeqLayer()                       */
/************************************************************************/

OGRGeoJSONSeqLayer::OGRGeoJSONSeqLayer(OGRGeoJSONSeqDataSource* poDS,
                                       const char* pszName, VSILFILE* fp):
    m_poDS(poDS),
    m_fp(fp)
{
    SetDescription(pszName);
    m_poFeatureDefn = new OGRFeatureDefn(pszName);
    m_poFeatureDefn->Reference();

    OGRSpatialReference* poSRSWGS84 = new OGRSpatialReference();
    poSRSWGS84->SetWellKnownGeogCS( "WGS84" );
    poSRSWGS84->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRSWGS84);
    poSRSWGS84->Release();
}

/************************************************************************/
/*                          ~OGRGeoJSONSeqLayer()                       */
/************************************************************************/

OGRGeoJSONSeqLayer::~OGRGeoJSONSeqLayer()
{
    VSIFCloseL(m_fp);
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                               Init()                                 */
/************************************************************************/

bool OGRGeoJSONSeqLayer::Init(bool bLooseIdentification)
{
    if( STARTS_WITH(m_poDS->GetDescription(), "/vsimem/") ||
        !STARTS_WITH(m_poDS->GetDescription(), "/vsi") )
    {
        VSIFSeekL(m_fp, 0, SEEK_END);
        m_nFileSize = VSIFTellL(m_fp);
    }

    ResetReading();

    std::map<std::string, int> oMapFieldNameToIdx;
    std::vector<std::unique_ptr<OGRFieldDefn>> apoFieldDefn;
    gdal::DirectedAcyclicGraph<int, std::string> dag;

    while( true )
    {
        auto poObject = GetNextObject(bLooseIdentification);
        if( !poObject )
            break;
        if( OGRGeoJSONGetType(poObject) == GeoJSONObject::eFeature )
        {
            m_oReader.GenerateFeatureDefn(oMapFieldNameToIdx,
                                          apoFieldDefn,
                                          dag,
                                          this, poObject);
        }
        json_object_put(poObject);
        m_nTotalFeatures ++;
    }

    OGRFeatureDefn* poDefn = GetLayerDefn();
    const auto sortedFields = dag.getTopologicalOrdering();
    CPLAssert( sortedFields.size() == apoFieldDefn.size() );
    for( int idx: sortedFields )
    {
        poDefn->AddFieldDefn(apoFieldDefn[idx].get());
    }

    ResetReading();

    m_nFileSize = 0;
    m_nIter = 0;
    m_oReader.FinalizeLayerDefn( this, m_osFIDColumn );

    return m_nTotalFeatures > 0;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoJSONSeqLayer::ResetReading()
{
    VSIFSeekL(m_fp, 0, SEEK_SET);
    // Undocumented: for testing purposes only
    const size_t nBufferSize = static_cast<size_t>(std::max(1,
        atoi(CPLGetConfigOption("OGR_GEOJSONSEQ_CHUNK_SIZE", "40960"))));
    const size_t nBufferSizeValidated =
        nBufferSize > static_cast<size_t>(100 * 1000 * 1000) ?
            static_cast<size_t>(100 * 1000 * 1000) : nBufferSize;
    m_osBuffer.resize(nBufferSizeValidated);
    m_osFeatureBuffer.clear();
    m_nPosInBuffer = nBufferSizeValidated;
    m_nBufferValidSize = nBufferSizeValidated;
    m_nNextFID = 0;
}

/************************************************************************/
/*                           GetNextObject()                            */
/************************************************************************/

json_object* OGRGeoJSONSeqLayer::GetNextObject(bool bLooseIdentification)
{
    m_osFeatureBuffer.clear();
    while( true )
    {
        // If we read all the buffer, then reload it from file
        if( m_nPosInBuffer >= m_nBufferValidSize )
        {
            if( m_nBufferValidSize < m_osBuffer.size() )
            {
                return nullptr;
            }
            m_nBufferValidSize = VSIFReadL(&m_osBuffer[0], 1,
                                           m_osBuffer.size(), m_fp);
            m_nPosInBuffer = 0;
            if( VSIFTellL(m_fp) == m_nBufferValidSize && m_nBufferValidSize > 0 )
            {
                m_bIsRSSeparated = (m_osBuffer[0] == RS);
                if( m_bIsRSSeparated )
                {
                    m_nPosInBuffer ++;
                }
            }
            m_nIter ++;

            if( m_nFileSize > 0 &&
                (m_nBufferValidSize < m_osBuffer.size() ||
                 (m_nIter % 100) == 0) )
            {
                CPLDebug("GeoJSONSeq", "First pass: %.2f %%",
                        100.0 * VSIFTellL(m_fp) / m_nFileSize);
            }
            if( m_nPosInBuffer >= m_nBufferValidSize )
            {
                return nullptr;
            }
        }

        // Find next feature separator in buffer
        const size_t nNextSepPos = m_osBuffer.find(
            m_bIsRSSeparated ? RS : '\n', m_nPosInBuffer);
        if( nNextSepPos != std::string::npos )
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
            if( m_osFeatureBuffer.size() > 100 * 1024 * 1024 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                            "Too large feature");
                return nullptr;
            }
            m_nPosInBuffer = m_nBufferValidSize;
            if( m_nBufferValidSize == m_osBuffer.size() )
            {
                continue;
            }
        }

        while( !m_osFeatureBuffer.empty() &&
               (m_osFeatureBuffer.back() == '\r' ||
                m_osFeatureBuffer.back() == '\n')  )
        {
            m_osFeatureBuffer.resize(m_osFeatureBuffer.size()-1);
        }
        if( !m_osFeatureBuffer.empty() )
        {
            json_object* poObject = nullptr;
            CPL_IGNORE_RET_VAL(
                OGRJSonParse(m_osFeatureBuffer.c_str(), &poObject));
            m_osFeatureBuffer.clear();
            if( json_object_get_type(poObject) == json_type_object )
            {
                return poObject;
            }
            json_object_put(poObject);
            if( bLooseIdentification )
            {
                return nullptr;
            }
        }
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRGeoJSONSeqLayer::GetNextFeature()
{
    while( true )
    {
        auto poObject = GetNextObject(false);
        if( !poObject )
            return nullptr;
        OGRFeature* poFeature;
        auto type = OGRGeoJSONGetType(poObject);
        if( type == GeoJSONObject::eFeature )
        {
            poFeature = m_oReader.ReadFeature(
                this, poObject, m_osFeatureBuffer.c_str() );
            json_object_put(poObject);
        }
        else if( type == GeoJSONObject::eFeatureCollection ||
                 type == GeoJSONObject::eUnknown )
        {
            json_object_put(poObject);
            continue;
        }
        else
        {
            OGRGeometry* poGeom = m_oReader.ReadGeometry(poObject,
                                                         GetSpatialRef());
            json_object_put(poObject);
            if( !poGeom )
            {
                continue;
            }
            poFeature = new OGRFeature(m_poFeatureDefn);
            poFeature->SetGeometryDirectly(poGeom);
        }

        if( poFeature->GetFID() == OGRNullFID )
        {
            poFeature->SetFID(m_nNextFID);
            m_nNextFID ++;
        }
        if( (m_poFilterGeom == nullptr ||
            FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter)) )
            && (m_poAttrQuery == nullptr ||
                m_poAttrQuery->Evaluate(poFeature)) )
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
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
    {
        return m_nTotalFeatures;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONSeqLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return true;
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr &&
        EQUAL(pszCap, OLCFastFeatureCount) )
    {
        return true;
    }
    return false;
}

/************************************************************************/
/*                        OGRGeoJSONSeqWriteLayer()                     */
/************************************************************************/

OGRGeoJSONSeqWriteLayer::OGRGeoJSONSeqWriteLayer(
                                        OGRGeoJSONSeqDataSource* poDS,
                                        const char* pszName,
                                        CSLConstList papszOptions,
                                        OGRCoordinateTransformation* poCT):
    m_poDS(poDS)
{
    SetDescription(pszName);
    m_poFeatureDefn = new OGRFeatureDefn(pszName);
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(
        OGRSpatialReference::GetWGS84SRS());
    m_poCT = poCT;

    m_oWriteOptions.SetRFC7946Settings();
    m_oWriteOptions.SetIDOptions(papszOptions);
    m_oWriteOptions.nCoordPrecision = atoi(
        CSLFetchNameValueDef(papszOptions, "COORDINATE_PRECISION", "7"));
    m_oWriteOptions.nSignificantFigures = atoi(
        CSLFetchNameValueDef(papszOptions, "SIGNIFICANT_FIGURES", "-1"));

    m_bRS = EQUAL(CPLGetExtension(poDS->GetDescription()), "GEOJSONS");
    const char* pszRS = CSLFetchNameValue(papszOptions, "RS");
    if( pszRS )
    {
        m_bRS = CPLTestBool(pszRS);
    }
}

/************************************************************************/
/*                       ~OGRGeoJSONSeqWriteLayer()                     */
/************************************************************************/

OGRGeoJSONSeqWriteLayer::~OGRGeoJSONSeqWriteLayer()
{
    m_poFeatureDefn->Release();
    delete m_poCT;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRGeoJSONSeqWriteLayer::ICreateFeature( OGRFeature* poFeature )
{
    VSILFILE* fp = m_poDS->GetOutputFile();

    std::unique_ptr<OGRFeature> poFeatureToWrite;
    if( m_poCT != nullptr )
    {
        poFeatureToWrite.reset(new OGRFeature(m_poFeatureDefn));
        poFeatureToWrite->SetFrom( poFeature );
        poFeatureToWrite->SetFID( poFeature->GetFID() );
        OGRGeometry* poGeometry = poFeatureToWrite->GetGeometryRef();
        if( poGeometry )
        {
            const char* const apszOptions[] = { "WRAPDATELINE=YES", nullptr };
            OGRGeometry* poNewGeom =
                OGRGeometryFactory::transformWithOptions(
                    poGeometry, m_poCT, const_cast<char**>(apszOptions), m_oTransformCache);
            if( poNewGeom == nullptr )
            {
                return OGRERR_FAILURE;
            }

            OGREnvelope sEnvelope;
            poNewGeom->getEnvelope(&sEnvelope);
            if( sEnvelope.MinX < -180.0 || sEnvelope.MaxX > 180.0 ||
                sEnvelope.MinY < -90.0 || sEnvelope.MaxY > 90.0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry extent outside of [-180.0,180.0]x[-90.0,90.0] bounds");
                return OGRERR_FAILURE;
            }

            poFeatureToWrite->SetGeometryDirectly( poNewGeom );
        }
    }

    json_object* poObj =
        OGRGeoJSONWriteFeature(
            poFeatureToWrite.get() ? poFeatureToWrite.get() : poFeature,
            m_oWriteOptions );
    CPLAssert( nullptr != poObj );

    if( m_bRS )
    {
        VSIFPrintfL( fp, "%c", RS);
    }
    VSIFPrintfL( fp, "%s\n", json_object_to_json_string( poObj ) );

    json_object_put( poObj );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr OGRGeoJSONSeqWriteLayer::CreateField( OGRFieldDefn* poField,
                                             int /* bApproxOK */  )
{
    m_poFeatureDefn->AddFieldDefn( poField );
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONSeqWriteLayer::TestCapability( const char* pszCap )
{
    if( EQUAL(pszCap, OLCCreateField) )
        return TRUE;
    else if( EQUAL(pszCap, OLCSequentialWrite) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool OGRGeoJSONSeqDataSource::Open( GDALOpenInfo* poOpenInfo,
                                    GeoJSONSourceType nSrcType)
{
    VSILFILE* fp = nullptr;
    CPLString osLayerName("GeoJSONSeq");

    const char* pszUnprefixedFilename = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSONSeq:") )
    {
        pszUnprefixedFilename = poOpenInfo->pszFilename + strlen("GeoJSONSeq:");
    }

    if( nSrcType == eGeoJSONSourceFile )
    {
        if (pszUnprefixedFilename != poOpenInfo->pszFilename)
        {
            osLayerName = CPLGetBasename(pszUnprefixedFilename);
            fp = VSIFOpenL( pszUnprefixedFilename, "rb");
        }
        else
        {
            osLayerName = CPLGetBasename(poOpenInfo->pszFilename);
            fp = poOpenInfo->fpL;
            poOpenInfo->fpL = nullptr;
        }
    }
    else if( nSrcType == eGeoJSONSourceText )
    {
        m_osTmpFile = CPLSPrintf("/vsimem/geojsonseq/%p", this);
        fp = VSIFileFromMemBuffer( m_osTmpFile.c_str(),
            reinterpret_cast<GByte*>(CPLStrdup(poOpenInfo->pszFilename)),
            strlen(poOpenInfo->pszFilename),
            true );
    }
    else if( nSrcType == eGeoJSONSourceService )
    {
        char* pszStoredContent =
            OGRGeoJSONDriverStealStoredContent(pszUnprefixedFilename);
        if( pszStoredContent )
        {
            if( !GeoJSONSeqIsObject( pszStoredContent) )
            {
                OGRGeoJSONDriverStoreContent(
                    poOpenInfo->pszFilename, pszStoredContent );
                return false;
            }
            else
            {
                m_osTmpFile = CPLSPrintf("/vsimem/geojsonseq/%p", this);
                fp = VSIFileFromMemBuffer( m_osTmpFile.c_str(),
                    reinterpret_cast<GByte*>(pszStoredContent),
                    strlen(pszStoredContent),
                    true );
            }
        }
        else
        {
            const char* const papsOptions[] = {
                "HEADERS=Accept: text/plain, application/json",
                nullptr
            };

            CPLHTTPResult* pResult = CPLHTTPFetch( pszUnprefixedFilename,
                                                   papsOptions );

            if( nullptr == pResult
                || 0 == pResult->nDataLen || 0 != CPLGetLastErrorNo() )
            {
                CPLHTTPDestroyResult( pResult );
                return false;
            }

            if( 0 != pResult->nStatus )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Curl reports error: %d: %s",
                        pResult->nStatus, pResult->pszErrBuf );
                CPLHTTPDestroyResult( pResult );
                return false;
            }

            m_osTmpFile = CPLSPrintf("/vsimem/geojsonseq/%p", this);
            fp = VSIFileFromMemBuffer( m_osTmpFile.c_str(),
                pResult->pabyData,
                pResult->nDataLen,
                true );
            pResult->pabyData = nullptr;
            pResult->nDataLen = 0;
            CPLHTTPDestroyResult( pResult );
        }
    }
    if( fp == nullptr )
    {
        return false;
    }
    SetDescription( poOpenInfo->pszFilename );
    auto poLayer = new OGRGeoJSONSeqLayer(this, osLayerName.c_str(), fp);
    const bool bLooseIdentification =
        nSrcType == eGeoJSONSourceService &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSONSeq:");
    if( bLooseIdentification )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
    }
    auto ret = poLayer->Init(bLooseIdentification);
    if( bLooseIdentification )
    {
        CPLPopErrorHandler();
        CPLErrorReset();
    }
    if( !ret )
    {
        delete poLayer;
        return false;
    }
    m_poLayer.reset(poLayer);
    return true;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

bool OGRGeoJSONSeqDataSource::Create( const char* pszName,
                                     char** /* papszOptions */ )
{
    CPLAssert( nullptr == m_fpOut );

    if( strcmp(pszName, "/dev/stdout") == 0 )
        pszName = "/vsistdout/";

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    m_fpOut = VSIFOpenExL( pszName, "w", true );
    if( nullptr == m_fpOut )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create %s: %s",
                  pszName, VSIGetLastErrorMsg() );
        return false;
    }

    return true;
}


/************************************************************************/
/*                       OGRGeoJSONSeqDriverIdentify()                  */
/************************************************************************/

static int OGRGeoJSONSeqDriverIdentifyInternal( GDALOpenInfo* poOpenInfo,
                                                GeoJSONSourceType& nSrcType )
{
    nSrcType = GeoJSONSeqGetSourceType( poOpenInfo );
    if( nSrcType == eGeoJSONSourceUnknown )
        return FALSE;
    if( nSrcType == eGeoJSONSourceService &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSONSeq:") )
    {
        return -1;
    }
    return TRUE;
}

/************************************************************************/
/*                      OGRGeoJSONSeqDriverIdentify()                   */
/************************************************************************/

static int OGRGeoJSONSeqDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType;
    return OGRGeoJSONSeqDriverIdentifyInternal(poOpenInfo, nSrcType);
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset* OGRGeoJSONSeqDriverOpen( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType;
    if( OGRGeoJSONSeqDriverIdentifyInternal(poOpenInfo, nSrcType) == FALSE )
    {
        return nullptr;
    }
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSONSeq driver does not support update");
        return nullptr;
    }

    OGRGeoJSONSeqDataSource* poDS = new OGRGeoJSONSeqDataSource();

    if( !poDS->Open( poOpenInfo, nSrcType ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGeoJSONSeqDriverCreate( const char * pszName,
                                            int /* nBands */,
                                            int /* nXSize */,
                                            int /* nYSize */,
                                            GDALDataType /* eDT */,
                                            char **papszOptions )
{
    OGRGeoJSONSeqDataSource* poDS = new OGRGeoJSONSeqDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
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
    if( GDALGetDriverByName( "GeoJSONSeq" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GeoJSONSeq" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GeoJSON Sequence" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "geojsonl geojsons" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/geojsonseq.html" );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='RS' type='boolean' description='whether to prefix records with RS=0x1e character' default='NO'/>"
"  <Option name='COORDINATE_PRECISION' type='int' description='Number of decimal for coordinates. Default is 7'/>"
"  <Option name='SIGNIFICANT_FIGURES' type='int' description='Number of significant figures for floating-point values' default='17'/>"
"  <Option name='ID_FIELD' type='string' description='Name of the source field that must be used as the id member of Feature features'/>"
"  <Option name='ID_TYPE' type='string-select' description='Type of the id member of Feature features'>"
"    <Value>AUTO</Value>"
"    <Value>String</Value>"
"    <Value>Integer</Value>"
"  </Option>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String IntegerList "
                               "Integer64List RealList StringList" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean" );

    poDriver->pfnOpen = OGRGeoJSONSeqDriverOpen;
    poDriver->pfnIdentify = OGRGeoJSONSeqDriverIdentify;
    poDriver->pfnCreate = OGRGeoJSONSeqDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
