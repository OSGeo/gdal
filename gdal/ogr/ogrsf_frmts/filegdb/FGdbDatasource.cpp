/******************************************************************************
 * $Id$
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

FGdbDataSource::FGdbDataSource(FGdbDriver* poDriver):
OGRDataSource(),
m_poDriver(poDriver), m_pszName(0), m_pGeodatabase(NULL), m_bUpdate(false)
{
}

/************************************************************************/
/*                          ~FGdbDataSource()                          */
/************************************************************************/

FGdbDataSource::~FGdbDataSource()
{
    CPLMutexHolderOptionalLockD(m_poDriver->GetMutex());

    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
        delete m_layers[i];

    m_poDriver->Release( m_pszName );
    CPLFree( m_pszName );
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int FGdbDataSource::Open(Geodatabase* pGeodatabase, const char * pszNewName, int bUpdate )
{
    m_pszName = CPLStrdup( pszNewName );
    m_pGeodatabase = pGeodatabase;
    m_bUpdate = bUpdate;

    std::vector<std::wstring> typesRequested;

	// We're only interested in Tables, Feature Datasets and Feature Classes
	typesRequested.push_back(L"Feature Class");
	typesRequested.push_back(L"Table");
	typesRequested.push_back(L"Feature Dataset");
	
    bool rv = LoadLayers(L"\\");

    return rv;
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
            GDBDebug(hr, "Error opening " + WStringToString(layers[i]) + ". Skipping it");
            continue;
        }
        FGdbLayer* pLayer = new FGdbLayer();
        if (!pLayer->Initialize(this, pTable, layers[i], type))
        {
            delete pLayer;
            return GDBErr(hr, "Error initializing OGRLayer for " + WStringToString(layers[i]));
        }

        m_layers.push_back(new OGRMutexedLayer(pLayer, TRUE, m_poDriver->GetMutex()));
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

// Flattens out hierarchichal GDB structure
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
                //std::wcout << datasetTypes[dsTypeIndex] << L" " << childDatasets[childDatasetIndex] << std::endl;

                if (!LoadLayersOld(datasetTypes, childDatasets[childDatasetIndex]))
                    errorsEncountered = true;
            }
        }
    }

    //it is a full fledged dataset itself without children - open it (except the root)

    if ((!childrenFound) && parent != L"\\")
    {
        //wcout << "Opening " << parent << "...";
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
    if( !m_bUpdate )
        return OGRERR_FAILURE;

    if( iLayer < 0 || iLayer >= static_cast<int>(m_layers.size()) )
        return OGRERR_FAILURE;
    
    FGdbLayer* poBaseLayer = (FGdbLayer*) m_layers[iLayer]->GetBaseLayer();

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
    if( !m_bUpdate )
        return NULL;

    FGdbLayer* pLayer = new FGdbLayer();
    if (!pLayer->Create(this, pszLayerName, poSRS, eType, papszOptions))
    {
        delete pLayer;
        return NULL;
    }
    
    OGRMutexedLayer* pMutexedLayer = new OGRMutexedLayer(pLayer, TRUE, m_poDriver->GetMutex());

    m_layers.push_back(pMutexedLayer);

    return pMutexedLayer;  
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
                                                     const char *pszVal )
{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( "FIELD_1", OFTString );
    poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    this->pszVal = pszVal ? CPLStrdup(pszVal) : NULL;
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
    if (EQUALN(pszSQLCommand, "GetLayerDefinition ", strlen("GetLayerDefinition ")))
    {
        OGRMutexedLayer* poMutexedLayer = (OGRMutexedLayer*) GetLayerByName(pszSQLCommand + strlen("GetLayerDefinition "));
        if (poMutexedLayer)
        {
            FGdbLayer* poLayer = (FGdbLayer*) poMutexedLayer->GetBaseLayer();
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
    if (EQUALN(pszSQLCommand, "GetLayerMetadata ", strlen("GetLayerMetadata ")))
    {
        OGRMutexedLayer* poMutexedLayer = (OGRMutexedLayer*) GetLayerByName(pszSQLCommand + strlen("GetLayerMetadata "));
        if (poMutexedLayer)
        {
            FGdbLayer* poLayer = (FGdbLayer*) poMutexedLayer->GetBaseLayer();
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
    if( EQUALN(pszSQLCommand, "SELECT ", 7) && pszDialect == NULL )
    {
        CPLDebug("FGDB", "Support for SELECT is known to be partially "
                         "non-compliant with FileGDB SDK API v1.2.\n"
                         "So for now, we use default OGR SQL engine. "
                         "Explicitely specify -dialect FileGDB\n"
                         "to use the SQL engine from the FileGDB SDK API");
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                        poSpatialFilter,
                                        pszDialect );
    }

/* -------------------------------------------------------------------- */
/*      Run the SQL                                                     */
/* -------------------------------------------------------------------- */
    EnumRows* pEnumRows = new EnumRows;
    long hr;
    if (FAILED(hr = m_pGeodatabase->ExecuteSQL(
                                StringToWString(pszSQLCommand), true, *pEnumRows)))
    {
        GDBErr(hr, CPLSPrintf("Failed at executing '%s'", pszSQLCommand));
        delete pEnumRows;
        return NULL;
    }

    if( EQUALN(pszSQLCommand, "SELECT ", 7) )
    {
        OGRLayer* pLayer = new FGdbResultLayer(this, pszSQLCommand, pEnumRows);
        return new OGRMutexedLayer(pLayer, TRUE, m_poDriver->GetMutex());
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
    delete poResultsSet;
}
