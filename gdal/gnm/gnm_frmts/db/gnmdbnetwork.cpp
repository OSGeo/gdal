/******************************************************************************
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM db based generic driver.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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

#include "gnmdb.h"
#include "gnm_priv.h"

CPL_CVSID("$Id$")

GNMDatabaseNetwork::GNMDatabaseNetwork() : GNMGenericNetwork()
{
    m_poDS = NULL;
}

GNMDatabaseNetwork::~GNMDatabaseNetwork()
{
    FlushCache();

    GDALClose(m_poDS);
}

CPLErr GNMDatabaseNetwork::Open(GDALOpenInfo *poOpenInfo)
{
    FormName(poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions);

    if(CSLFindName(poOpenInfo->papszOpenOptions, "LIST_ALL_TABLES") == -1)
        poOpenInfo->papszOpenOptions = CSLAddNameValue(
                    poOpenInfo->papszOpenOptions, "LIST_ALL_TABLES", "YES");

    m_poDS = (GDALDataset*) GDALOpenEx( m_soNetworkFullName, GDAL_OF_VECTOR |
                     GDAL_OF_UPDATE, NULL, NULL, poOpenInfo->papszOpenOptions );

    if( NULL == m_poDS )
    {
    CPLError( CE_Failure, CPLE_OpenFailed, "Open '%s' failed",
              m_soNetworkFullName.c_str() );
    return CE_Failure;
    }

    // There should be only one schema so no schema name can be in table name
    if(LoadMetadataLayer(m_poDS) != CE_None)
    {
        return CE_Failure;
    }

    if(LoadGraphLayer(m_poDS) != CE_None)
    {
        return CE_Failure;
    }

    if(LoadFeaturesLayer(m_poDS) != CE_None)
    {
        return CE_Failure;
    }

    return CE_None;
}

CPLErr GNMDatabaseNetwork::Create( const char* pszFilename, char** papszOptions )
{
    FormName(pszFilename, papszOptions);

    if(m_soName.empty() || m_soNetworkFullName.empty())
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "The network name should be present" );
        return CE_Failure;
    }

    if(NULL == m_poDS)
    {
        m_poDS = (GDALDataset*) GDALOpenEx( m_soNetworkFullName, GDAL_OF_VECTOR |
                                       GDAL_OF_UPDATE, NULL, NULL, papszOptions );
    }

    if( NULL == m_poDS )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Open '%s' failed",
                  m_soNetworkFullName.c_str() );
        return CE_Failure;
    }

    GDALDriver *l_poDriver = m_poDS->GetDriver();
    if(NULL == l_poDriver)
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Get dataset driver failed");
        return CE_Failure;
    }

    if(!CheckStorageDriverSupport(l_poDriver->GetDescription()))
    {
        return CE_Failure;
    }

    // check required options

    const char* pszNetworkDescription = CSLFetchNameValue(papszOptions,
                                                         GNM_MD_DESCR);
    if(NULL != pszNetworkDescription)
        sDescription = pszNetworkDescription;

    // check Spatial reference
    const char* pszSRS = CSLFetchNameValue(papszOptions, GNM_MD_SRS);
    if( NULL == pszSRS )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "The network spatial reference should be present" );
        return CE_Failure;
    }
    else
    {
        OGRSpatialReference spatialRef;
        if (spatialRef.SetFromUserInput(pszSRS) != OGRERR_NONE)
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "The network spatial reference should be present" );
            return CE_Failure;
        }

        char *wktSrs = NULL;
        if (spatialRef.exportToWkt(&wktSrs) != OGRERR_NONE)
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "The network spatial reference should be present" );
            return CE_Failure;
        }
        m_soSRS = wktSrs;

        CPLFree(wktSrs);
    }

    int nResult = CheckNetworkExist(pszFilename, papszOptions);

    if(TRUE == nResult)
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "The network already exist" );
        return CE_Failure;
    }

    // Create the necessary system layers and fields

    // Create meta layer

    CPLErr eResult = CreateMetadataLayer(m_poDS, GNM_VERSION_NUM);

    if(CE_None != eResult)
    {
        //an error message should come from function
        return CE_Failure;    }

    // Create graph layer

    eResult = CreateGraphLayer(m_poDS);

    if(CE_None != eResult)
    {
        DeleteMetadataLayer();
        return CE_Failure;
    }

    // Create features layer

    eResult = CreateFeaturesLayer(m_poDS);

    if(CE_None != eResult)
    {
        DeleteMetadataLayer();
        DeleteGraphLayer();
        return CE_Failure;
    }

    return CE_None;
}

int GNMDatabaseNetwork::CheckNetworkExist(const char *pszFilename, char **papszOptions)
{
    // check if path exist
    // if path exist check if network already present and OVERWRITE option
    // else create the path

    if(FormName(pszFilename, papszOptions) != CE_None)
    {
        return TRUE;
    }

    if(NULL == m_poDS)
    {
        m_poDS = (GDALDataset*) GDALOpenEx( m_soNetworkFullName, GDAL_OF_VECTOR |
                                      GDAL_OF_UPDATE, NULL, NULL, papszOptions );
    }

    const bool bOverwrite = CPLFetchBool(papszOptions, "OVERWRITE", false);

    std::vector<int> anDeleteLayers;
    int i;
    for(i = 0; i < m_poDS->GetLayerCount(); ++i)
    {
        OGRLayer* poLayer = m_poDS->GetLayer(i);
        if(NULL == poLayer)
            continue;

        if(EQUAL(poLayer->GetName(), GNM_SYSLAYER_META) ||
           EQUAL(poLayer->GetName(), GNM_SYSLAYER_GRAPH) ||
           EQUAL(poLayer->GetName(), GNM_SYSLAYER_FEATURES) )
        {
            anDeleteLayers.push_back(i);
        }
    }

    if(anDeleteLayers.empty())
        return FALSE;

    if( bOverwrite )
    {
        for(i = (int)anDeleteLayers.size(); i > 0; i--)
        {
            CPLDebug("GNM", "Delete layer: %d", i);
            if(m_poDS->DeleteLayer(anDeleteLayers[i - 1]) != OGRERR_NONE)
                return TRUE;
        }
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

CPLErr GNMDatabaseNetwork::DeleteMetadataLayer()
{
    return DeleteLayerByName(GNM_SYSLAYER_META);
}

CPLErr GNMDatabaseNetwork::DeleteGraphLayer()
{
    return DeleteLayerByName(GNM_SYSLAYER_GRAPH);
}

CPLErr GNMDatabaseNetwork::DeleteFeaturesLayer()
{
    return DeleteLayerByName(GNM_SYSLAYER_FEATURES);
}

CPLErr GNMDatabaseNetwork::DeleteLayerByName(const char* pszLayerName)
{
    if(NULL == m_poDS)
        return CE_Failure;

    for(int i = 0; i < m_poDS->GetLayerCount(); ++i)
    {
        OGRLayer* poLayer = m_poDS->GetLayer(i);
        if(NULL == poLayer)
            continue;

        if(EQUAL(poLayer->GetName(), pszLayerName))
            return m_poDS->DeleteLayer(i) == OGRERR_NONE ? CE_None : CE_Failure;
    }

    CPLError(CE_Failure, CPLE_IllegalArg, "The layer %s not exist",
             pszLayerName );
    return CE_Failure;
}

CPLErr GNMDatabaseNetwork::DeleteNetworkLayers()
{
    while(GetLayerCount() > 0)
    {
        OGRErr eErr = DeleteLayer(0);
        if(eErr != OGRERR_NONE)
            return CE_Failure;
    }
    return CE_None;
}

CPLErr GNMDatabaseNetwork::LoadNetworkLayer(const char *pszLayername)
{
    // check if not loaded
    for(size_t i = 0; i < m_apoLayers.size(); ++i)
    {
        if(EQUAL(m_apoLayers[i]->GetName(), pszLayername))
            return CE_None;
    }

    OGRLayer* poLayer = m_poDS->GetLayerByName(pszLayername);
    if(NULL == poLayer)
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Layer '%s' is not exist",
                  pszLayername );
        return CE_Failure;
    }

    CPLDebug("GNM", "Layer '%s' loaded", poLayer->GetName());

    GNMGenericLayer* pGNMLayer = new GNMGenericLayer(poLayer, this);
    m_apoLayers.push_back(pGNMLayer);

    return CE_None;
}

bool GNMDatabaseNetwork::CheckStorageDriverSupport(const char *pszDriverName)
{
    if(EQUAL(pszDriverName, "PostgreSQL"))
        return true;
    //TODO: expand this list with supported OGR direvers
    return false;
}

CPLErr GNMDatabaseNetwork::FormName(const char *pszFilename, char **papszOptions)
{
    if(m_soNetworkFullName.empty())
        m_soNetworkFullName = pszFilename;

    if(m_soName.empty())
    {
        const char* pszNetworkName = CSLFetchNameValue(papszOptions, GNM_MD_NAME);
        if(NULL != pszNetworkName)
        {
            m_soName = pszNetworkName;
        }

        char *pszActiveSchemaStart;
        pszActiveSchemaStart = (char *)strstr(pszFilename, "active_schema=");
        if (pszActiveSchemaStart == NULL)
            pszActiveSchemaStart = (char *)strstr(pszFilename, "ACTIVE_SCHEMA=");
        if (pszActiveSchemaStart != NULL)
        {
            char           *pszActiveSchema;
            const char     *pszEnd = NULL;

            pszActiveSchema = CPLStrdup( pszActiveSchemaStart + strlen("active_schema=") );

            pszEnd = strchr(pszActiveSchemaStart, ' ');
            if( pszEnd == NULL )
                pszEnd = pszFilename + strlen(pszFilename);

            pszActiveSchema[pszEnd - pszActiveSchemaStart - strlen("active_schema=")] = '\0';

            m_soName = pszActiveSchema;
            CPLFree(pszActiveSchema);
        }
        else
        {
            if(!m_soName.empty())
            {
                //add active schema
                m_soNetworkFullName += "ACTIVE_SCHEMA=" + m_soName;
            }
            else
            {
                m_soName = "public";
            }
        }

        CPLDebug( "GNM", "Network name: %s", m_soName.c_str() );
    }
    return CE_None;
}

OGRErr GNMDatabaseNetwork::DeleteLayer(int nIndex)
{
    if(NULL == m_poDS)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Network not opened." );
        return OGRERR_FAILURE;
    }

    OGRLayer* poNetworkLayer = GetLayer(nIndex);

    CPLDebug("GNM", "Delete network layer '%s'", poNetworkLayer->GetName());

    int nDeleteIndex = -1;
    for(int i = 0; i < m_poDS->GetLayerCount(); ++i)
    {
        OGRLayer* poLayer = m_poDS->GetLayer(i);
        if(EQUAL(poNetworkLayer->GetName(), poLayer->GetName()))
        {
            nDeleteIndex = i;
            break;
        }
    }

    if(m_poDS->DeleteLayer(nDeleteIndex) != OGRERR_NONE)
    {
        return OGRERR_FAILURE;
    }

    return GNMGenericNetwork::DeleteLayer(nIndex);
}

OGRLayer *GNMDatabaseNetwork::ICreateLayer(const char *pszName,
                                CPL_UNUSED OGRSpatialReference *poSpatialRef,
                                OGRwkbGeometryType eGType, char **papszOptions)
{
    //check if layer with such name exist
    for(int i = 0; i < GetLayerCount(); ++i)
    {
        OGRLayer* pLayer = GetLayer(i);
        if(NULL == pLayer)
            continue;
        if(EQUAL(pLayer->GetName(), pszName))
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "The network layer '%s' already exist.", pszName );
            return NULL;
        }
    }

    OGRSpatialReference oSpaRef(m_soSRS);

    OGRLayer *poLayer = m_poDS->CreateLayer( pszName, &oSpaRef, eGType, papszOptions );
    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Layer creation failed." );
        return NULL;
    }

    OGRFieldDefn oField( GNM_SYSFIELD_GFID, GNMGFIDInt );
    if( poLayer->CreateField( &oField ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Creating global identificator field failed." );
        return NULL;
    }

    OGRFieldDefn oFieldBlock(GNM_SYSFIELD_BLOCKED, OFTInteger);
    if( poLayer->CreateField( &oFieldBlock ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Creating is blocking field failed." );
        return NULL;
    }

    GNMGenericLayer* pGNMLayer = new GNMGenericLayer(poLayer, this);
    m_apoLayers.push_back(pGNMLayer);
    return pGNMLayer;
}
