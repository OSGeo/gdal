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
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

using std::vector;
using std::wstring;

/************************************************************************/
/*                          FGdbDataSource()                           */
/************************************************************************/

FGdbDataSource::FGdbDataSource(FGdbDriver* poDriverIn, 
                               FGdbDatabaseConnection* pConnection):
OGRDataSource(),
m_poDriver(poDriverIn), m_pConnection(pConnection), m_pGeodatabase(NULL), m_bUpdate(false),
m_poOpenFileGDBDrv(NULL)
{
    bPerLayerCopyingForTransaction = -1;
}

/************************************************************************/
/*                          ~FGdbDataSource()                          */
/************************************************************************/

FGdbDataSource::~FGdbDataSource()
{
    CPLMutexHolderOptionalLockD(m_poDriver ? m_poDriver->GetMutex() : NULL);
    
    if( m_pConnection && m_pConnection->IsLocked() )
        CommitTransaction();

    //Close();
    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        m_layers[i]->CloseGDBObjects();
    }

    FixIndexes();

    if( m_poDriver )
        m_poDriver->Release( m_osPublicName );

    //size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        delete m_layers[i];
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

        char* apszDrivers[2];
        apszDrivers[0] = (char*) "OpenFileGDB";
        apszDrivers[1] = NULL;
        const char* pszSystemCatalog = CPLFormFilename(m_osFSName, "a00000001.gdbtable", NULL);
        GDALDataset* poOpenFileGDBDS = (GDALDataset*)
            GDALOpenEx(pszSystemCatalog, GDAL_OF_VECTOR,
                       apszDrivers, NULL, NULL);
        if( poOpenFileGDBDS == NULL || poOpenFileGDBDS->GetLayer(0) == NULL )
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
                if( m_layers[i]->m_oMapOGRFIDToFGDBFID.size() == 0)
                    continue;
                CPLString osFilter = "name = '";
                osFilter += m_layers[i]->GetName();
                osFilter += "'";
                poLayer->SetAttributeFilter(osFilter);
                poLayer->ResetReading();
                OGRFeature* poF = poLayer->GetNextFeature();
                if( poF == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot find filename for layer %s",
                             m_layers[i]->GetName());
                    bRet = FALSE;
                }
                else
                {
                    if( !m_layers[i]->EditIndexesForFIDHack(CPLFormFilename(m_osFSName,
                                        CPLSPrintf("a%08x", (int)poF->GetFID()), NULL)) )
                    {
                        bRet = FALSE;
                    }
                }
                delete poF;
            }
        }
        GDALClose(poOpenFileGDBDS);

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

    std::vector<std::wstring> typesRequested;

	// We're only interested in Tables, Feature Datasets and Feature Classes
	typesRequested.push_back(L"Feature Class");
	typesRequested.push_back(L"Table");
	typesRequested.push_back(L"Feature Dataset");
	
    bool rv = LoadLayers(L"\\");

    return rv;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int FGdbDataSource::Close(int bCloseGeodatabase)
{
    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        m_layers[i]->CloseGDBObjects();
    }

    int bRet = FixIndexes();
    if( m_pConnection && bCloseGeodatabase )
        m_pConnection->CloseGeodatabase();
    m_pGeodatabase = NULL;
    return bRet;
}

/************************************************************************/
/*                               ReOpen()                               */
/************************************************************************/

int FGdbDataSource::ReOpen()
{
    CPLAssert(m_pGeodatabase == NULL);

    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL_REOPEN", ""), "CASE1") ||
        !m_pConnection->OpenGeodatabase(m_osFSName) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen %s",
                 m_osFSName.c_str());
        return FALSE;
    }

    FGdbDataSource* pDS = new FGdbDataSource(m_poDriver, m_pConnection);
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL_REOPEN", ""), "CASE2") ||
        !pDS->Open(m_osPublicName, TRUE, m_osFSName) )
    {
        pDS->m_poDriver = NULL;
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
        if( pNewLayer &&
            !EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL_REOPEN", ""), "CASE3") )
        {
            m_layers[i]->m_pTable = pNewLayer->m_pTable;
            pNewLayer->m_pTable = NULL;
            m_layers[i]->m_pEnumRows = pNewLayer->m_pEnumRows;
            pNewLayer->m_pEnumRows = NULL;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen %s",
                     m_layers[i]->GetName());
            bRet = FALSE;
        }
        m_layers[i]->m_oMapOGRFIDToFGDBFID.clear();
        m_layers[i]->m_oMapFGDBFIDToOGRFID.clear();
    }
    
    m_pGeodatabase = pDS->m_pGeodatabase;
    pDS->m_pGeodatabase = NULL;

    pDS->m_poDriver = NULL;
    pDS->m_pConnection = NULL;
    delete pDS;
    
    return bRet;
}

/************************************************************************/
/*                          OpenFGDBTables()                            */
/************************************************************************/

bool FGdbDataSource::OpenFGDBTables(const std::wstring &type,
                                    const std::vector<std::wstring> &layers)
{
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
        FGdbLayer* pLayer = new FGdbLayer();
        if (!pLayer->Initialize(this, pTable, layers[i], type))
        {
            delete pLayer;
            return GDBErr(hr, "Error initializing OGRLayer for " + WStringToString(layers[i]));
        }

        m_layers.push_back(pLayer);
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

    /* Find all the Tables in the root */
    if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(root, L"Table", tables)) )
    {
        return GDBErr(hr, "Error reading Tables in " + WStringToString(root));
    }
    /* Open the tables we found */
    if ( tables.size() > 0 && ! OpenFGDBTables(L"Table", tables) )
        return false;

    /* Find all the Feature Classes in the root */
    if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(root, L"Feature Class", featureclasses)) )
    {
        return GDBErr(hr, "Error reading Feature Classes in " + WStringToString(root));
    }
    /* Open the tables we found */
    if ( featureclasses.size() > 0 && ! OpenFGDBTables(L"Feature Class", featureclasses) )
        return false;

    /* Find all the Feature Datasets in the root */
    if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(root, L"Feature Dataset", featuredatasets)) )
    {
        return GDBErr(hr, "Error reading Feature Datasets in " + WStringToString(root));
    }
    /* Look for Feature Classes inside the Feature Dataset */
    for ( unsigned int i = 0; i < featuredatasets.size(); i++ )
    {
        if ( FAILED(hr = m_pGeodatabase->GetChildDatasets(featuredatasets[i], L"Feature Class", featureclasses)) )
        {
            return GDBErr(hr, "Error reading Feature Classes in " + WStringToString(featuredatasets[i]));
        }
        if ( featureclasses.size() > 0 && ! OpenFGDBTables(L"Feature Class", featureclasses) )
            return false;
    }
    return true;
}


#if 0
/************************************************************************/
/*                            LoadLayersOld()                              */
/************************************************************************/

/* Old recursive LoadLayers. Removed in favor of simple one that only
   looks at FeatureClasses and Tables. */

// Flattens out hierarchical GDB structure.
bool FGdbDataSource::LoadLayersOld(const std::vector<wstring> & datasetTypes,
                                const wstring & parent)
{
    long hr = S_OK;

    // I didn't find an API to give me the type of the dataset based on name - I am *not*
    // parsing XML for something like this - in the meantime I can use this hack to see
    // if the dataset had any children whatsoever - if so, then I won't attempt to open it
    // otherwise, do attempt to do that

    bool childrenFound = false;
    bool errorsEncountered = false;

    for (size_t dsTypeIndex = 0; dsTypeIndex < datasetTypes.size(); dsTypeIndex++)
    {
        std::vector<wstring> childDatasets;
        m_pGeodatabase->GetChildDatasets( parent, datasetTypes[dsTypeIndex], childDatasets);

        if (childDatasets.size() > 0)
        {
            //it is a container of other datasets

            for (size_t childDatasetIndex = 0;
                 childDatasetIndex < childDatasets.size();
                 childDatasetIndex++)
            {
                childrenFound = true;

                // do something with it
                // For now, we just ignore dataset containers and only open the children
                // std::wcout << datasetTypes[dsTypeIndex] << L" " << childDatasets[childDatasetIndex] << std::endl;

                if (!LoadLayersOld(datasetTypes, childDatasets[childDatasetIndex]))
                    errorsEncountered = true;
            }
        }
    }

    //it is a full fledged dataset itself without children - open it (except the root)

    if ((!childrenFound) && parent != L"\\")
    {
        // wcout << "Opening " << parent << "...";
        Table* pTable = new Table;
        if (FAILED(hr = m_pGeodatabase->OpenTable(parent,*pTable)))
        {
            delete pTable;
            return GDBErr(hr, "Error opening " + WStringToString(parent));
        }

        FGdbLayer* pLayer = new FGdbLayer;

        //pLayer has ownership of the table pointer as soon Initialize is called
        if (!pLayer->Initialize(this, pTable, parent))
        {
            delete pLayer;

            return GDBErr(hr, "Error initializing OGRLayer for " +
                          WStringToString(parent));
        }

        m_layers.push_back(pLayer);
    }

    return !errorsEncountered;
}
#endif


/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr FGdbDataSource::DeleteLayer( int iLayer )
{
    if( !m_bUpdate || m_pGeodatabase == NULL )
        return OGRERR_FAILURE;

    if( iLayer < 0 || iLayer >= static_cast<int>(m_layers.size()) )
        return OGRERR_FAILURE;
    
    FGdbLayer* poBaseLayer = m_layers[iLayer];

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
    else if EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer)
        return TRUE;
    return FALSE;
}


/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *FGdbDataSource::GetLayer( int iLayer )
{ 
    int count = static_cast<int>(m_layers.size());

    if( iLayer < 0 || iLayer >= count )
        return NULL;
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
    if( !m_bUpdate || m_pGeodatabase == NULL )
        return NULL;

    FGdbLayer* pLayer = new FGdbLayer();
    if (!pLayer->Create(this, pszLayerName, poSRS, eType, papszOptions))
    {
        delete pLayer;
        return NULL;
    }

    m_layers.push_back(pLayer);

    return pLayer;  
}


/************************************************************************/
/*                   OGRFGdbSingleFeatureLayer                          */
/************************************************************************/

class OGRFGdbSingleFeatureLayer : public OGRLayer
{
  private:
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGRFGdbSingleFeatureLayer( const char* pszLayerName,
                                                   const char *pszVal );
                        ~OGRFGdbSingleFeatureLayer();

    virtual void        ResetReading() { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature();
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) { return FALSE; }
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
    this->pszVal = pszValIn ? CPLStrdup(pszValIn) : NULL;
}

/************************************************************************/
/*                   ~OGRFGdbSingleFeatureLayer()                       */
/************************************************************************/

OGRFGdbSingleFeatureLayer::~OGRFGdbSingleFeatureLayer()
{
    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
    CPLFree(pszVal);
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGRFGdbSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

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
    if( m_pGeodatabase == NULL )
         return NULL;

    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
    {
        m_layers[i]->EndBulkLoad();
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
            char* pszVal = NULL;
            poLayer->GetLayerXML(&pszVal);
            OGRLayer* poRet = new OGRFGdbSingleFeatureLayer( "LayerDefinition", pszVal );
            CPLFree(pszVal);
            return poRet;
        }
        else
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerMetadata                                   */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerMetadata "))
    {
        FGdbLayer* poLayer = (FGdbLayer*) GetLayerByName(pszSQLCommand + strlen("GetLayerMetadata "));
        if (poLayer)
        {
            char* pszVal = NULL;
            poLayer->GetLayerMetadataXML(&pszVal);
            OGRLayer* poRet = new OGRFGdbSingleFeatureLayer( "LayerMetadata", pszVal );
            CPLFree(pszVal);
            return poRet;
        }
        else
            return NULL;
    }

    /* TODO: remove that workaround when the SDK has finally a decent */
    /* SQL support ! */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") && pszDialect == NULL )
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
        return NULL;
    }

    if (FAILED(hr))
    {
        GDBErr(hr, CPLSPrintf("Failed at executing '%s'", pszSQLCommand));
        delete pEnumRows;
        return NULL;
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
        return NULL;
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
        m_poOpenFileGDBDrv != NULL &&
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
        m_layers[i]->SetSymlinkFlag();
    }
}
