/******************************************************************************
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM file based generic driver.
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

#include "gnmfile.h"
#include "gnm_priv.h"

CPL_CVSID("$Id$")

GNMFileNetwork::GNMFileNetwork() : GNMGenericNetwork()
{
    m_pMetadataDS = NULL;
    m_pGraphDS = NULL;
    m_pFeaturesDS = NULL;
}

GNMFileNetwork::~GNMFileNetwork()
{
    FlushCache();

    for (std::map<OGRLayer*, GDALDataset*>::iterator
            it = m_mpLayerDatasetMap.begin();
            it != m_mpLayerDatasetMap.end(); ++it)
    {
            GDALClose(it->second);
    }

    m_mpLayerDatasetMap.clear();

    GDALClose(m_pGraphDS);
    GDALClose(m_pFeaturesDS);
    GDALClose(m_pMetadataDS);
}

CPLErr GNMFileNetwork::Open(GDALOpenInfo *poOpenInfo)
{
    m_soNetworkFullName = poOpenInfo->pszFilename;
    char **papszFiles = VSIReadDir( m_soNetworkFullName );
    if( CSLCount(papszFiles) == 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Open '%s' file failed",
                  m_soNetworkFullName.c_str() );
        return CE_Failure;
    }

    // search for metadata file
    CPLString soMetadatafile;
    for( int i = 0; papszFiles[i] != NULL; i++ )
    {
        if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
            continue;

        if( EQUAL(CPLGetBasename(papszFiles[i]), GNM_SYSLAYER_META) )
        {
            soMetadatafile = CPLFormFilename(m_soNetworkFullName, papszFiles[i],
                                             NULL);
            break;
        }
    }

    CSLDestroy( papszFiles );

    m_pMetadataDS = (GDALDataset*) GDALOpenEx( soMetadatafile, GDAL_OF_VECTOR |
                                               GDAL_OF_UPDATE, NULL, NULL, NULL );
    if( NULL == m_pMetadataDS )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Open '%s' file failed",
                  m_soNetworkFullName.c_str() );
        return CE_Failure;
    }

    if(LoadMetadataLayer(m_pMetadataDS) != CE_None)
    {
        return CE_Failure;
    }

    m_poLayerDriver = m_pMetadataDS->GetDriver();
    const char* pszExt = CPLGetExtension(soMetadatafile);

    CPLString soGraphfile = CPLFormFilename(m_soNetworkFullName,
                                            GNM_SYSLAYER_GRAPH, pszExt);
    m_pGraphDS = (GDALDataset*) GDALOpenEx( soGraphfile, GDAL_OF_VECTOR |
                                            GDAL_OF_UPDATE, NULL, NULL, NULL );
    if( NULL == m_pGraphDS )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Open '%s' file failed",
                  m_soNetworkFullName.c_str() );
        return CE_Failure;
    }

    if(LoadGraphLayer(m_pGraphDS) != CE_None)
    {
        return CE_Failure;
    }

    CPLString soFeaturesfile = CPLFormFilename(m_soNetworkFullName,
                                            GNM_SYSLAYER_FEATURES, pszExt);
    m_pFeaturesDS = (GDALDataset*) GDALOpenEx( soFeaturesfile, GDAL_OF_VECTOR |
                                               GDAL_OF_UPDATE, NULL, NULL, NULL );
    if( NULL == m_pFeaturesDS )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Open '%s' file failed",
                  m_soNetworkFullName.c_str() );
        return CE_Failure;
    }

    if(LoadFeaturesLayer(m_pFeaturesDS) != CE_None)
    {
        return CE_Failure;
    }

    return CE_None;
}

int GNMFileNetwork::CheckNetworkExist(const char *pszFilename, char **papszOptions)
{
    // check if path exist
    // if path exist check if network already present and OVERWRITE option
    // else create the path

    const bool bOverwrite = CPLFetchBool(papszOptions, "OVERWRITE", false);

    if(m_soName.empty())
    {
        const char* pszNetworkName = CSLFetchNameValue(papszOptions, GNM_MD_NAME);

        if( NULL != pszNetworkName )
        {
            m_soName = pszNetworkName;
        }
    }

    if(FormPath(pszFilename, papszOptions) != CE_None)
    {
        return TRUE;
    }

    if (CPLCheckForFile((char*)m_soNetworkFullName.c_str(), NULL))
    {
        char **papszFiles = VSIReadDir( m_soNetworkFullName );
        if( CSLCount(papszFiles) == 0 )
        {
            return FALSE;
        }

        // search for base GNM files
        for(int i = 0; papszFiles[i] != NULL; i++ )
        {
            if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
                continue;

            if( EQUAL(CPLGetBasename(papszFiles[i]), GNM_SYSLAYER_META) ||
                EQUAL(CPLGetBasename(papszFiles[i]), GNM_SYSLAYER_GRAPH) ||
                EQUAL(CPLGetBasename(papszFiles[i]), GNM_SYSLAYER_FEATURES) ||
                EQUAL(papszFiles[i], GNM_SRSFILENAME) )
            {
                if( bOverwrite )
                {
                    const char* pszDeleteFile = CPLFormFilename(
                                m_soNetworkFullName, papszFiles[i], NULL);
                    CPLDebug("GNM", "Delete file: %s", pszDeleteFile);
                    if( VSIUnlink(pszDeleteFile) != 0 )
                    {
                        return TRUE;
                    }
                }
                else
                {
                    return TRUE;
                }
            }
        }
        CSLDestroy( papszFiles );
    }
    else
    {
        if (VSIMkdir(m_soNetworkFullName, 0755) != 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

CPLErr GNMFileNetwork::Delete()
{
    CPLErr eResult = GNMGenericNetwork::Delete();
    if(eResult != CE_None)
       return eResult;

    // check if folder empty
    char **papszFiles = VSIReadDir( m_soNetworkFullName );
    bool bIsEmpty = true;
    for(int i = 0; papszFiles[i] != NULL; ++i)
    {
        if( !(EQUAL(papszFiles[i], "..") || EQUAL(papszFiles[i], ".")) )
        {
            bIsEmpty = false;
            break;
        }
    }

    CSLDestroy( papszFiles );

    if( !bIsEmpty )
    {
        return eResult;
    }
    return VSIRmdir(m_soNetworkFullName) == 0 ? CE_None : CE_Failure;
}

CPLErr GNMFileNetwork::CreateMetadataLayerFromFile( const char* pszFilename, int nVersion,
                                         char** papszOptions )
{
    CPLErr eResult = CheckLayerDriver(GNM_MD_DEFAULT_FILE_FORMAT, papszOptions);
    if(CE_None != eResult)
        return eResult;

    eResult = FormPath(pszFilename, papszOptions);
    if(CE_None != eResult)
        return eResult;

    const char* pszExt = m_poLayerDriver->GetMetadataItem(GDAL_DMD_EXTENSION);
    CPLString osDSFileName = CPLFormFilename(m_soNetworkFullName,
                                                GNM_SYSLAYER_META, pszExt);

    m_pMetadataDS = m_poLayerDriver->Create(osDSFileName, 0, 0, 0, GDT_Unknown,
                                            NULL );
    if( NULL == m_pMetadataDS )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Creation of '%s' file failed",
                  osDSFileName.c_str() );
        return CE_Failure;
    }

    return GNMGenericNetwork::CreateMetadataLayer(m_pMetadataDS, nVersion, 254);
}

CPLErr GNMFileNetwork::StoreNetworkSrs()
{
    const char* pszSrsFileName = CPLFormFilename(m_soNetworkFullName,
                                                 GNM_SRSFILENAME, NULL);
    VSILFILE *fpSrsPrj = VSIFOpenL(pszSrsFileName, "w");
    if (fpSrsPrj != NULL)
    {
        if(VSIFWriteL(m_soSRS, (int)m_soSRS.size(), 1, fpSrsPrj) != 1)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Write SRS failed, disk full?" );
            VSIFCloseL(fpSrsPrj);
            return CE_Failure;
        }
        VSIFCloseL(fpSrsPrj);
    }
    return CE_None;
}

CPLErr GNMFileNetwork::LoadNetworkSrs()
{
    const char* pszSrsFileName = CPLFormFilename(m_soNetworkFullName,
                                                 GNM_SRSFILENAME, NULL);
    char** papszLines = CSLLoad(pszSrsFileName);
    if ( NULL == papszLines )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Loading of '%s' layer failed",
                  GNM_SYSLAYER_META );
        return CE_Failure;
    }

    m_soSRS = papszLines[0];

    CSLDestroy( papszLines );

    return CE_None;
}

CPLErr GNMFileNetwork::DeleteMetadataLayer()
{
    if(NULL != m_pMetadataDS)
    {
        const char* pszSrsFileName = CPLFormFilename(m_soNetworkFullName,
                                                     GNM_SRSFILENAME, NULL);
        VSIUnlink(pszSrsFileName); // just try to delete as file may not be existed
        return m_pMetadataDS->DeleteLayer(0) == OGRERR_NONE ? CE_None : CE_Failure;
    }
    return CE_Failure;
}

CPLErr GNMFileNetwork::CreateGraphLayerFromFile( const char* pszFilename, char** papszOptions )
{
    CPLErr eResult = CheckLayerDriver(GNM_MD_DEFAULT_FILE_FORMAT, papszOptions);
    if(CE_None != eResult)
        return eResult;

    eResult = FormPath(pszFilename, papszOptions);
    if(CE_None != eResult)
        return eResult;

    const char* pszExt = m_poLayerDriver->GetMetadataItem(GDAL_DMD_EXTENSION);
    CPLString osDSFileName = CPLFormFilename(m_soNetworkFullName,
                                                GNM_SYSLAYER_GRAPH, pszExt);

    m_pGraphDS = m_poLayerDriver->Create(osDSFileName, 0, 0, 0, GDT_Unknown,
                                            NULL );

    if( m_pGraphDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Creation of '%s' file failed",
                  osDSFileName.c_str() );
        return CE_Failure;
    }

    return GNMGenericNetwork::CreateGraphLayer(m_pGraphDS);
}

CPLErr GNMFileNetwork::DeleteGraphLayer()
{
    if(NULL != m_pGraphDS)
    {
        return m_pGraphDS->DeleteLayer(0) == OGRERR_NONE ? CE_None : CE_Failure;
    }
    return CE_Failure;
}

CPLErr GNMFileNetwork::CreateFeaturesLayerFromFile( const char* pszFilename, char** papszOptions )
{
    CPLErr eResult = CheckLayerDriver(GNM_MD_DEFAULT_FILE_FORMAT, papszOptions);
    if(CE_None != eResult)
        return eResult;

    eResult = FormPath(pszFilename, papszOptions);
    if(CE_None != eResult)
        return eResult;

    const char* pszExt = m_poLayerDriver->GetMetadataItem(GDAL_DMD_EXTENSION);
    CPLString osDSFileName = CPLFormFilename(m_soNetworkFullName,
                                                GNM_SYSLAYER_FEATURES, pszExt);

    m_pFeaturesDS = m_poLayerDriver->Create(osDSFileName, 0, 0, 0, GDT_Unknown,
                                            NULL );

    if( m_pFeaturesDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Creation of '%s' file failed",
                  osDSFileName.c_str() );
        return CE_Failure;
    }

    return GNMGenericNetwork::CreateFeaturesLayer(m_pFeaturesDS);
}

CPLErr GNMFileNetwork::DeleteFeaturesLayer()
{
    if(NULL != m_pFeaturesDS)
    {
        return m_pFeaturesDS->DeleteLayer(0) == OGRERR_NONE ? CE_None : CE_Failure;
    }
    return CE_Failure;
}

CPLErr GNMFileNetwork::DeleteNetworkLayers()
{
    while(GetLayerCount() > 0)
    {
        OGRErr eErr = DeleteLayer(0);
        if(eErr != OGRERR_NONE)
            return CE_Failure;
    }
    return CE_None;
}

CPLErr GNMFileNetwork::LoadNetworkLayer(const char *pszLayername)
{
    // check if not loaded
    for(size_t i = 0; i < m_apoLayers.size(); ++i)
    {
        if(EQUAL(m_apoLayers[i]->GetName(), pszLayername))
            return CE_None;
    }

    const char* pszExt = m_poLayerDriver->GetMetadataItem(GDAL_DMD_EXTENSION);

    CPLString soFile = CPLFormFilename(m_soNetworkFullName, pszLayername, pszExt);
    GDALDataset *poDS = (GDALDataset*) GDALOpenEx( soFile, GDAL_OF_VECTOR |
                                            GDAL_OF_UPDATE, NULL, NULL, NULL );
    if( NULL == poDS )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Open '%s' file failed",
                  soFile.c_str() );
        return CE_Failure;
    }

    OGRLayer* poLayer = poDS->GetLayer(0);
    if(NULL == poLayer)
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Layer '%s' is not exist",
                  pszLayername );
        return CE_Failure;
    }

    CPLDebug("GNM", "Layer '%s' loaded", poLayer->GetName());

    GNMGenericLayer* pGNMLayer = new GNMGenericLayer(poLayer, this);
    m_apoLayers.push_back(pGNMLayer);
    m_mpLayerDatasetMap[pGNMLayer] = poDS;

    return CE_None;
}

bool GNMFileNetwork::CheckStorageDriverSupport(const char *pszDriverName)
{
    if(EQUAL(pszDriverName, GNM_MD_DEFAULT_FILE_FORMAT))
        return true;
    //TODO: expand this list with supported OGR direvers
    return false;
}

CPLErr GNMFileNetwork::FormPath(const char *pszFilename, char **papszOptions)
{
    if(m_soNetworkFullName.empty())
    {
        const char* pszNetworkName = CSLFetchNameValue(papszOptions, GNM_MD_NAME);
        if(NULL == pszNetworkName)
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "The network name should be present" );
            return CE_Failure;
        }
        m_soNetworkFullName = CPLFormFilename(pszFilename, pszNetworkName, NULL);

        CPLDebug( "GNM", "Network name: %s", m_soNetworkFullName.c_str() );
    }
    return CE_None;
}

int GNMFileNetwork::CloseDependentDatasets()
{
    size_t nCount = m_mpLayerDatasetMap.size();
    for(std::map<OGRLayer*, GDALDataset*>::iterator
        it = m_mpLayerDatasetMap.begin();
        it != m_mpLayerDatasetMap.end(); ++it)
    {
        GDALClose(it->second);
    }

    m_mpLayerDatasetMap.clear();

    GNMGenericNetwork::CloseDependentDatasets();

    return nCount > 0 ? TRUE : FALSE;
}

OGRErr GNMFileNetwork::DeleteLayer(int nIndex)
{
    OGRLayer* pLayer = GetLayer(nIndex);

    GDALDataset* poDS = m_mpLayerDatasetMap[pLayer];
    if(NULL == poDS)
    {
        return OGRERR_FAILURE;
    }

    CPLDebug("GNM", "Delete network layer '%s'", pLayer->GetName());

    if(poDS->DeleteLayer(0) != OGRERR_NONE)
    {
        return OGRERR_FAILURE;
    }

    GDALClose(poDS);

    // remove pointer from map
    m_mpLayerDatasetMap.erase(pLayer);

    return GNMGenericNetwork::DeleteLayer(nIndex);
}

OGRLayer *GNMFileNetwork::ICreateLayer(const char *pszName,
                                CPL_UNUSED OGRSpatialReference *poSpatialRef,
                                OGRwkbGeometryType eGType, char **papszOptions)
{
    if(NULL == m_poLayerDriver)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "The network storage format driver is not defined." );
        return NULL;
    }

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

    //form path
    const char* pszExt = m_poLayerDriver->GetMetadataItem(GDAL_DMD_EXTENSION);
    CPLString soPath = CPLFormFilename(m_soNetworkFullName, pszName ,pszExt);

    GDALDataset *poDS = m_poLayerDriver->Create( soPath, 0, 0, 0, GDT_Unknown,
                                                 papszOptions );
    if( poDS == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Creation of output file failed." );
        return NULL;
    }

    OGRSpatialReference oSpaRef(m_soSRS);

    OGRLayer *poLayer = poDS->CreateLayer( pszName, &oSpaRef, eGType, papszOptions );
    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Layer creation failed." );
        GDALClose(poDS);
        return NULL;
    }

    OGRFieldDefn oField( GNM_SYSFIELD_GFID, GNMGFIDInt );
    if( poLayer->CreateField( &oField ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Creating global identificator field failed." );
        GDALClose(poDS);
        return NULL;
    }

    OGRFieldDefn oFieldBlock(GNM_SYSFIELD_BLOCKED, OFTInteger);
    if( poLayer->CreateField( &oFieldBlock ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Creating is blocking field failed." );
        GDALClose(poDS);
        return NULL;
    }

    GNMGenericLayer* pGNMLayer = new GNMGenericLayer(poLayer, this);
    m_apoLayers.push_back(pGNMLayer);
    m_mpLayerDatasetMap[pGNMLayer] = poDS;
    return pGNMLayer;
}

CPLErr GNMFileNetwork::Create( const char* pszFilename, char** papszOptions )
{
    // check required options

    // check name
    const char* pszNetworkName = CSLFetchNameValue(papszOptions, GNM_MD_NAME);

    if( NULL == pszNetworkName )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "The network name should be present" );
        return CE_Failure;
    }
    else
    {
        m_soName = pszNetworkName;
    }

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

    CPLErr eResult = CreateMetadataLayerFromFile(pszFilename, GNM_VERSION_NUM, papszOptions);

    if(CE_None != eResult)
    {
        //an error message should come from function
        return CE_Failure;
    }

    // Create graph layer

    eResult = CreateGraphLayerFromFile(pszFilename, papszOptions);

    if(CE_None != eResult)
    {
        DeleteMetadataLayer();
        return CE_Failure;
    }

    // Create features layer

    eResult = CreateFeaturesLayerFromFile(pszFilename, papszOptions);

    if(CE_None != eResult)
    {
        DeleteMetadataLayer();
        DeleteGraphLayer();
        return CE_Failure;
    }

    return CE_None;
}
