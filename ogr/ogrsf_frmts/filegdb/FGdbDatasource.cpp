/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB OGR Datasource.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *           Paul Ramsey, pramsey at cleverelephant.ca
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_fgdb.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"
#include "FGdbUtils.h"
#include "cpl_multiproc.h"

#include "filegdb_fielddomain.h"

CPL_CVSID("$Id$")

using std::vector;
using std::wstring;

/***********************************************************************/
/*                         OGRFileGDBGroup                             */
/***********************************************************************/

class OGRFileGDBGroup final: public GDALGroup
{
protected:
    friend class FGdbDataSource;
    std::vector<std::shared_ptr<GDALGroup>> m_apoSubGroups{};
    std::vector<OGRLayer*> m_apoLayers{};

public:
    OGRFileGDBGroup(const std::string& osParentName, const char* pszName):
        GDALGroup(osParentName, pszName) {}

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    std::vector<std::string> GetVectorLayerNames(CSLConstList papszOptions) const override;
    OGRLayer* OpenVectorLayer(const std::string& osName,
                              CSLConstList papszOptions) const override;
};

std::vector<std::string> OGRFileGDBGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> ret;
    for( const auto& poSubGroup: m_apoSubGroups )
        ret.emplace_back(poSubGroup->GetName());
    return ret;
}

std::shared_ptr<GDALGroup> OGRFileGDBGroup::OpenGroup(const std::string& osName,
                                                          CSLConstList) const
{
    for( const auto& poSubGroup: m_apoSubGroups )
    {
        if( poSubGroup->GetName() == osName )
            return poSubGroup;
    }
    return nullptr;
}

std::vector<std::string> OGRFileGDBGroup::GetVectorLayerNames(CSLConstList) const
{
    std::vector<std::string> ret;
    for( const auto& poLayer: m_apoLayers )
        ret.emplace_back(poLayer->GetName());
    return ret;
}

OGRLayer* OGRFileGDBGroup::OpenVectorLayer(const std::string& osName,
                                               CSLConstList) const
{
    for( const auto& poLayer: m_apoLayers )
    {
        if( poLayer->GetName() == osName )
            return poLayer;
    }
    return nullptr;
}

/************************************************************************/
/*                          FGdbDataSource()                           */
/************************************************************************/

FGdbDataSource::FGdbDataSource(bool bUseDriverMutex,
                               FGdbDatabaseConnection* pConnection):
OGRDataSource(),
m_bUseDriverMutex(bUseDriverMutex), m_pConnection(pConnection), m_pGeodatabase(nullptr), m_bUpdate(false),
m_poOpenFileGDBDrv(nullptr)
{
    bPerLayerCopyingForTransaction = -1;
}

/************************************************************************/
/*                          ~FGdbDataSource()                          */
/************************************************************************/

FGdbDataSource::~FGdbDataSource()
{
    CPLMutexHolderOptionalLockD( m_bUseDriverMutex ? FGdbDriver::hMutex : nullptr );

    if( m_pConnection && m_pConnection->IsLocked() )
        CommitTransaction();

    //Close();
    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        if (FGdbLayer* poFgdbLayer = dynamic_cast<FGdbLayer*>(m_layers[i]))
        {
           poFgdbLayer->CloseGDBObjects();
        }
    }

    FixIndexes();

    if ( m_bUseDriverMutex )
      FGdbDriver::Release( m_osPublicName );

    //size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        // only delete layers coming from this driver -- the tables opened by the OpenFileGDB driver will
        // be deleted when we close the OpenFileGDB datasource
        if (FGdbLayer* poFgdbLayer = dynamic_cast<FGdbLayer*>(m_layers[i]))
        {
          delete poFgdbLayer;
        }
    }
}

/************************************************************************/
/*                             FixIndexes()                             */
/************************************************************************/

int FGdbDataSource::FixIndexes()
{
    int bRet = TRUE;
    if( m_pConnection && m_pConnection->IsFIDHackInProgress() )
    {
        m_pConnection->CloseGeodatabase();

        const char* const apszDrivers[2] = { "OpenFileGDB", nullptr };
        const char* pszSystemCatalog = CPLFormFilename(m_osFSName, "a00000001.gdbtable", nullptr);
        auto poOpenFileGDBDS = std::unique_ptr<GDALDataset>(
            GDALDataset::Open(pszSystemCatalog, GDAL_OF_VECTOR,
                              apszDrivers, nullptr, nullptr));
        if( poOpenFileGDBDS == nullptr || poOpenFileGDBDS->GetLayer(0) == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot open %s with OpenFileGDB driver. "
                     "Should not happen. Some layers will be corrupted",
                     pszSystemCatalog);
            bRet = FALSE;
        }
        else
        {
            OGRLayer* poLayer = poOpenFileGDBDS->GetLayer(0);
            size_t count = m_layers.size();
            for(size_t i = 0; i < count; ++i )
            {
                FGdbLayer* poFgdbLayer = dynamic_cast<FGdbLayer*>(m_layers[i]);
                if ( !poFgdbLayer )
                    continue;

                if( poFgdbLayer->m_oMapOGRFIDToFGDBFID.empty())
                    continue;
                CPLString osFilter = "name = '";
                osFilter += m_layers[i]->GetName();
                osFilter += "'";
                poLayer->SetAttributeFilter(osFilter);
                poLayer->ResetReading();
                OGRFeature* poF = poLayer->GetNextFeature();
                if( poF == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot find filename for layer %s",
                             m_layers[i]->GetName());
                    bRet = FALSE;
                }
                else
                {
                    if( !poFgdbLayer->EditIndexesForFIDHack(CPLFormFilename(m_osFSName,
                                        CPLSPrintf("a%08x", (int)poF->GetFID()), nullptr)) )
                    {
                        bRet = FALSE;
                    }
                }
                delete poF;
            }
        }

        m_pConnection->SetFIDHackInProgress(FALSE);
    }
    return bRet;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int FGdbDataSource::Open(const char * pszNewName, int bUpdate,
                         const char* pszPublicName )
{
    m_osFSName = pszNewName;
    m_osPublicName = (pszPublicName) ? pszPublicName : pszNewName;
    m_pGeodatabase = m_pConnection->GetGDB();
    m_bUpdate = CPL_TO_BOOL(bUpdate);
    m_poOpenFileGDBDrv = (GDALDriver*) GDALGetDriverByName("OpenFileGDB");

    return LoadLayers(L"\\");
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int FGdbDataSource::Close(int bCloseGeodatabase)
{
    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        if (FGdbLayer* poFgdbLayer = dynamic_cast<FGdbLayer*>(m_layers[i]))
        {
            poFgdbLayer->CloseGDBObjects();
        }
    }

    int bRet = FixIndexes();
    if( m_pConnection && bCloseGeodatabase )
        m_pConnection->CloseGeodatabase();
    m_pGeodatabase = nullptr;
    return bRet;
}

/************************************************************************/
/*                               ReOpen()                               */
/************************************************************************/

int FGdbDataSource::ReOpen()
{
    CPLAssert(m_pGeodatabase == nullptr);

    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL_REOPEN", ""), "CASE1") ||
        !m_pConnection->OpenGeodatabase(m_osFSName) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen %s",
                 m_osFSName.c_str());
        return FALSE;
    }

    FGdbDataSource* pDS = new FGdbDataSource(m_bUseDriverMutex, m_pConnection);
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL_REOPEN", ""), "CASE2") ||
        !pDS->Open(m_osPublicName, TRUE, m_osFSName) )
    {
        pDS->m_bUseDriverMutex = false;
        delete pDS;
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen %s",
                 m_osFSName.c_str());
        return FALSE;
    }

    int bRet = TRUE;
    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        FGdbLayer* pNewLayer = (FGdbLayer*)pDS->GetLayerByName(m_layers[i]->GetName());
        FGdbLayer* poFgdbLayer = dynamic_cast<FGdbLayer*>(m_layers[i]);
        if( pNewLayer &&
            !EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL_REOPEN", ""), "CASE3") )
        {
            if ( poFgdbLayer )
            {
                poFgdbLayer->m_pTable = pNewLayer->m_pTable;
            }
            pNewLayer->m_pTable = nullptr;
            if ( poFgdbLayer )
            {
                poFgdbLayer->m_pEnumRows = pNewLayer->m_pEnumRows;
            }
            pNewLayer->m_pEnumRows = nullptr;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen %s",
                     m_layers[i]->GetName());
            bRet = FALSE;
        }
        if ( poFgdbLayer )
        {
            poFgdbLayer->m_oMapOGRFIDToFGDBFID.clear();
            poFgdbLayer->m_oMapFGDBFIDToOGRFID.clear();
        }
    }

    m_pGeodatabase = pDS->m_pGeodatabase;
    pDS->m_pGeodatabase = nullptr;

    pDS->m_bUseDriverMutex = false;
    pDS->m_pConnection = nullptr;
    delete pDS;

    return bRet;
}

/************************************************************************/
/*                          OpenFGDBTables()                            */
/************************************************************************/

bool FGdbDataSource::OpenFGDBTables(OGRFileGDBGroup* group,
                                    const std::wstring &type,
                                    const std::vector<std::wstring> &layers)
{
#ifdef DISPLAY_RELATED_DATASETS
    std::vector<std::wstring> relationshipTypes;
    m_pGeodatabase->GetDatasetRelationshipTypes(relationshipTypes);
    std::vector<std::wstring> datasetTypes;
    m_pGeodatabase->GetDatasetTypes(datasetTypes);
#endif

    fgdbError hr;
    for ( unsigned int i = 0; i < layers.size(); i++ )
    {
        Table* pTable = new Table;
        //CPLDebug("FGDB", "Opening %s", WStringToString(layers[i]).c_str());
        if (FAILED(hr = m_pGeodatabase->OpenTable(layers[i], *pTable)))
        {
            delete pTable;

            std::wstring fgdb_error_desc_w;
            fgdbError er;
            er = FileGDBAPI::ErrorInfo::GetErrorDescription(hr, fgdb_error_desc_w);
            const char* pszLikelyReason = "Might be due to unsupported spatial reference system. Using OpenFileGDB driver or FileGDB SDK >= 1.4 should solve it";
            if ( er == S_OK )
            {
                std::string fgdb_error_desc = WStringToString(fgdb_error_desc_w);
                if( fgdb_error_desc == "FileGDB compression is not installed." )
                {
                    pszLikelyReason = "Using FileGDB SDK 1.4 or later should solve this issue.";
                }
            }

            GDBErr(hr, "Error opening " + WStringToString(layers[i]),
                   CE_Warning,
                   (". Skipping it. " + CPLString(pszLikelyReason)).c_str());
            continue;
        }

#ifdef DISPLAY_RELATED_DATASETS
        CPLDebug("FGDB", "Finding datasets related to %s",
                 WStringToString(layers[i]).c_str());
        // Slow !
        for ( const auto& relType: relationshipTypes )
        {
            for ( const auto& itemType: datasetTypes )
            {
                std::vector<std::wstring> relatedDatasets;
                m_pGeodatabase->GetRelatedDatasets(
                    layers[i], relType, itemType, relatedDatasets);

                std::vector<std::string> relatedDatasetDefs;
                m_pGeodatabase->GetRelatedDatasetDefinitions(
                    layers[i], relType, itemType, relatedDatasetDefs);

                if( !relatedDatasets.empty() || !relatedDatasetDefs.empty())
                {
                    CPLDebug("FGDB", "relationshipType: %s",
                             WStringToString(relType).c_str());
                    CPLDebug("FGDB", "itemType: %s",
                             WStringToString(itemType).c_str());
                }
                for( const auto& name: relatedDatasets )
                {
                    CPLDebug("FGDB", "Related dataset: %s",
                             WStringToString(name).c_str());
                }
                for( const auto& xml: relatedDatasetDefs )
                {
                    CPLDebug("FGDB", "Related dataset def: %s", xml.c_str());
                }
            }
        }
#endif

        FGdbLayer* pLayer = new FGdbLayer();
        if (!pLayer->Initialize(this, pTable, layers[i], type))
        {
            delete pLayer;
            return GDBErr(hr, "Error initializing OGRLayer for " + WStringToString(layers[i]));
        }

        m_layers.push_back(pLayer);
        group->m_apoLayers.emplace_back(pLayer);
    }
    return true;
}

/************************************************************************/
/*                            LoadLayers()                             */
/************************************************************************/

bool FGdbDataSource::LoadLayers(const std::wstring &root)
{
    std::vector<wstring> tables;
    std::vector<wstring> featureclasses;
    std::vector<wstring> featuredatasets;
    fgdbError hr;

    auto poRootGroup = std::make_shared<OGRFileGDBGroup>(std::string(), "");
    m_poRootGroup = poRootGroup;

    /* Find all the Tables in the root */
    if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(root, L"Table", tables)) )
    {
        return GDBErr(hr, "Error reading Tables in " + WStringToString(root));
    }
    /* Open the tables we found */
    if ( !tables.empty() && ! OpenFGDBTables(poRootGroup.get(), L"Table", tables) )
        return false;

    /* Find all the Feature Classes in the root */
    if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(root, L"Feature Class", featureclasses)) )
    {
        return GDBErr(hr, "Error reading Feature Classes in " + WStringToString(root));
    }
    /* Open the tables we found */
    if ( !featureclasses.empty() && ! OpenFGDBTables(poRootGroup.get(), L"Feature Class", featureclasses) )
        return false;

    /* Find all the Feature Datasets in the root */
    if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(root, L"Feature Dataset", featuredatasets)) )
    {
        return GDBErr(hr, "Error reading Feature Datasets in " + WStringToString(root));
    }
    /* Look for Feature Classes inside the Feature Dataset */
    for ( unsigned int i = 0; i < featuredatasets.size(); i++ )
    {
        const std::string featureDatasetPath(WStringToString(featuredatasets[i]));
        if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(featuredatasets[i], L"Feature Class", featureclasses)) )
        {
            return GDBErr(hr, "Error reading Feature Classes in " + featureDatasetPath);
        }
        std::string featureDatasetName(featureDatasetPath);
        if( !featureDatasetName.empty() && featureDatasetPath[0] == '\\' )
            featureDatasetName = featureDatasetName.substr(1);
        auto poFeatureDatasetGroup = std::make_shared<OGRFileGDBGroup>(
            poRootGroup->GetName(), featureDatasetName.c_str());
        poRootGroup->m_apoSubGroups.emplace_back(poFeatureDatasetGroup);
        if ( !featureclasses.empty() && !
            OpenFGDBTables(poFeatureDatasetGroup.get(), L"Feature Class", featureclasses) )
            return false;
    }

    // Look for items which aren't present in the GDB_Items table (see https://github.com/OSGeo/gdal/issues/4463)
    // We do this by browsing the catalog table (using the OpenFileGDB driver) and looking for items we haven't yet found.
    // If we find any, we have no choice but to load these using the OpenFileGDB driver, as the ESRI SDK refuses to acknowledge that they
    // exist (despite ArcGIS itself showing them!)
    const char* const apszDrivers[2] = { "OpenFileGDB", nullptr };
    const char* pszSystemCatalog = CPLFormFilename(m_osFSName, "a00000001", "gdbtable");
    m_poOpenFileGDBDS.reset(
        GDALDataset::Open(pszSystemCatalog, GDAL_OF_VECTOR, apszDrivers, nullptr, nullptr) );
    if( m_poOpenFileGDBDS != nullptr && m_poOpenFileGDBDS->GetLayer(0) != nullptr )
    {
        OGRLayer* poCatalogLayer = m_poOpenFileGDBDS->GetLayer(0);
        const int iNameIndex = poCatalogLayer->GetLayerDefn()->GetFieldIndex( "Name" );
        for( auto& poFeat: poCatalogLayer )
        {
            const std::string osTableName = poFeat->GetFieldAsString( iNameIndex );

            // test if layer is already added
            if ( GDALDataset::GetLayerByName( osTableName.c_str() ) )
                continue;

            const CPLString osLCTableName(CPLString(osTableName).tolower());
            const bool bIsPrivateLayer = osLCTableName.size() >= 4 && osLCTableName.substr(0, 4) == "gdb_";
            if ( !bIsPrivateLayer )
            {
                OGRLayer* poLayer = m_poOpenFileGDBDS->GetLayerByName( osTableName.c_str() );
                m_layers.push_back(poLayer);
                poRootGroup->m_apoLayers.emplace_back(poLayer);
            }
        }
    }

    return true;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr FGdbDataSource::DeleteLayer( int iLayer )
{
    if( !m_bUpdate || m_pGeodatabase == nullptr )
        return OGRERR_FAILURE;

    if( iLayer < 0 || iLayer >= static_cast<int>(m_layers.size()) )
        return OGRERR_FAILURE;

    FGdbLayer* poBaseLayer = dynamic_cast< FGdbLayer* >( m_layers[iLayer] );
    if ( !poBaseLayer )
        return OGRERR_FAILURE;

    // Fetch FGDBAPI Table before deleting OGR layer object

    //Table* pTable = poBaseLayer->GetTable();

    std::string name = poBaseLayer->GetLayerDefn()->GetName();
    std::wstring strPath = poBaseLayer->GetTablePath();
    std::wstring strType = poBaseLayer->GetType();

    // delete OGR layer
    delete m_layers[iLayer];

    //pTable = NULL; // OGR Layer had ownership of FGDB Table

    m_layers.erase(m_layers.begin() + iLayer);

    long hr;

    if (FAILED(hr = m_pGeodatabase->Delete(strPath, strType)))
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                 "%s was not deleted however it has been closed", name.c_str());
        GDBErr(hr, "Failed deleting dataset");
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int FGdbDataSource::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return m_bUpdate;

    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return m_bUpdate;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return m_bUpdate;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *FGdbDataSource::GetLayer( int iLayer )
{
    int count = static_cast<int>(m_layers.size());

    if( iLayer < 0 || iLayer >= count )
        return nullptr;
    else
        return m_layers[iLayer];
}

/************************************************************************/
/*                             ICreateLayer()                           */
/*                                                                      */
/* See FGdbLayer::Create for creation options                           */
/************************************************************************/

OGRLayer *
FGdbDataSource::ICreateLayer( const char * pszLayerName,
                              OGRSpatialReference *poSRS,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )
{
    if( !m_bUpdate || m_pGeodatabase == nullptr )
        return nullptr;

    FGdbLayer* pLayer = new FGdbLayer();
    if (!pLayer->Create(this, pszLayerName, poSRS, eType, papszOptions))
    {
        delete pLayer;
        return nullptr;
    }

    m_layers.push_back(pLayer);

    return pLayer;
}

/************************************************************************/
/*                   OGRFGdbSingleFeatureLayer                          */
/************************************************************************/

class OGRFGdbSingleFeatureLayer final: public OGRLayer
{
  private:
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGRFGdbSingleFeatureLayer( const char* pszLayerName,
                                                   const char *pszVal );
               virtual ~OGRFGdbSingleFeatureLayer();

    virtual void        ResetReading() override { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeatureDefn *GetLayerDefn() override { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                    OGRFGdbSingleFeatureLayer()                       */
/************************************************************************/

OGRFGdbSingleFeatureLayer::OGRFGdbSingleFeatureLayer(const char* pszLayerName,
                                                     const char *pszValIn )
{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( "FIELD_1", OFTString );
    poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    pszVal = pszValIn ? CPLStrdup(pszValIn) : nullptr;
}

/************************************************************************/
/*                   ~OGRFGdbSingleFeatureLayer()                       */
/************************************************************************/

OGRFGdbSingleFeatureLayer::~OGRFGdbSingleFeatureLayer()
{
    if( poFeatureDefn != nullptr )
        poFeatureDefn->Release();
    CPLFree(pszVal);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGRFGdbSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return nullptr;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/************************************************************************/
/*                              ExecuteSQL()                            */
/************************************************************************/

OGRLayer * FGdbDataSource::ExecuteSQL( const char *pszSQLCommand,
                                       OGRGeometry *poSpatialFilter,
                                       const char *pszDialect )

{
    if( m_pConnection && m_pConnection->IsFIDHackInProgress() )
    {
        if( Close() )
            ReOpen();
    }
    if( m_pGeodatabase == nullptr )
         return nullptr;

    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        if (FGdbLayer* poFgdbLayer = dynamic_cast<FGdbLayer*>(m_layers[i]))
            poFgdbLayer->EndBulkLoad();
    }

/* -------------------------------------------------------------------- */
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case GetLayerDefinition                                 */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerDefinition "))
    {
        FGdbLayer* poLayer = (FGdbLayer*) GetLayerByName(pszSQLCommand + strlen("GetLayerDefinition "));
        if (poLayer)
        {
            char* pszVal = nullptr;
            poLayer->GetLayerXML(&pszVal);
            OGRLayer* poRet = new OGRFGdbSingleFeatureLayer( "LayerDefinition", pszVal );
            CPLFree(pszVal);
            return poRet;
        }
        else
            return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerMetadata                                   */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerMetadata "))
    {
        FGdbLayer* poLayer = (FGdbLayer*) GetLayerByName(pszSQLCommand + strlen("GetLayerMetadata "));
        if (poLayer)
        {
            char* pszVal = nullptr;
            poLayer->GetLayerMetadataXML(&pszVal);
            OGRLayer* poRet = new OGRFGdbSingleFeatureLayer( "LayerMetadata", pszVal );
            CPLFree(pszVal);
            return poRet;
        }
        else
            return nullptr;
    }

    /* TODO: remove that workaround when the SDK has finally a decent */
    /* SQL support ! */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") && pszDialect == nullptr )
    {
        CPLDebug("FGDB", "Support for SELECT is known to be partially "
                         "non-compliant with FileGDB SDK API v1.2.\n"
                         "So for now, we use default OGR SQL engine. "
                         "Explicitly specify -dialect FileGDB\n"
                         "to use the SQL engine from the FileGDB SDK API");
        OGRLayer* poLayer = OGRDataSource::ExecuteSQL( pszSQLCommand,
                                        poSpatialFilter,
                                        pszDialect );
        if( poLayer )
            m_oSetSelectLayers.insert(poLayer);
        return poLayer;
    }

/* -------------------------------------------------------------------- */
/*      Run the SQL                                                     */
/* -------------------------------------------------------------------- */
    EnumRows* pEnumRows = new EnumRows;
    long hr;
    try
    {
        hr = m_pGeodatabase->ExecuteSQL(
                                StringToWString(pszSQLCommand), true, *pEnumRows);
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Exception occurred at executing '%s'. Application may "
                  "become unstable", pszSQLCommand );
        delete pEnumRows;
        return nullptr;
    }

    if (FAILED(hr))
    {
        GDBErr(hr, CPLSPrintf("Failed at executing '%s'", pszSQLCommand));
        delete pEnumRows;
        return nullptr;
    }

    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") )
    {
        OGRLayer* poLayer = new FGdbResultLayer(this, pszSQLCommand, pEnumRows);
        m_oSetSelectLayers.insert(poLayer);
        return poLayer;
    }
    else
    {
        delete pEnumRows;
        return nullptr;
    }
}

/************************************************************************/
/*                           ReleaseResultSet()                         */
/************************************************************************/

void FGdbDataSource::ReleaseResultSet( OGRLayer * poResultsSet )
{
    if( poResultsSet )
        m_oSetSelectLayers.erase(poResultsSet);
    delete poResultsSet;
}

/************************************************************************/
/*                      HasPerLayerCopyingForTransaction()              */
/************************************************************************/

int FGdbDataSource::HasPerLayerCopyingForTransaction()
{
    if( bPerLayerCopyingForTransaction >= 0 )
        return bPerLayerCopyingForTransaction;
#ifdef WIN32
    bPerLayerCopyingForTransaction = FALSE;
#else
    bPerLayerCopyingForTransaction =
        m_poOpenFileGDBDrv != nullptr &&
        CPLTestBool(CPLGetConfigOption("FGDB_PER_LAYER_COPYING_TRANSACTION", "TRUE"));
#endif
    return bPerLayerCopyingForTransaction;
}

/************************************************************************/
/*                        SetSymlinkFlagOnAllLayers()                    */
/************************************************************************/

void FGdbDataSource::SetSymlinkFlagOnAllLayers()
{
    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        if (FGdbLayer* poFgdbLayer = dynamic_cast<FGdbLayer*>(m_layers[i]))
            poFgdbLayer->SetSymlinkFlag();
    }
}

/************************************************************************/
/*                        GetFieldDomain()                              */
/************************************************************************/

const OGRFieldDomain* FGdbDataSource::GetFieldDomain(const std::string& name) const
{
    const auto baseRet = GDALDataset::GetFieldDomain(name);
    if( baseRet )
        return baseRet;

    std::string domainDef;
    const auto hr = m_pGeodatabase->GetDomainDefinition(StringToWString(name), domainDef);
    if (FAILED(hr))
    {
        GDBErr(hr, "Failed in GetDomainDefinition()");
        return nullptr;
    }

    auto poDomain = ParseXMLFieldDomainDef(domainDef);
    if( !poDomain )
        return nullptr;
    const auto domainName = poDomain->GetName();
    m_oMapFieldDomains[domainName] = std::move(poDomain);
    return GDALDataset::GetFieldDomain(name);
}


/************************************************************************/
/*                        GetFieldDomainNames()                         */
/************************************************************************/

std::vector<std::string> FGdbDataSource::GetFieldDomainNames(CSLConstList) const
{
    std::vector<std::wstring> oDomainNamesWList;
    const auto hr = m_pGeodatabase->GetDomains(oDomainNamesWList);
    if (FAILED(hr))
    {
        GDBErr(hr, "Failed in GetDomains()");
        return std::vector<std::string>();
    }

    std::vector<std::string> oDomainNamesList;
    oDomainNamesList.reserve(oDomainNamesWList.size());
    for ( const auto& osNameW : oDomainNamesWList )
    {
        oDomainNamesList.emplace_back(WStringToString(osNameW));
    }
    return oDomainNamesList;
}
