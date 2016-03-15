/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM generic driver.
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

#include "gnm_frmts.h"
#include "gnmfile.h"

static int GNMFileDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( !poOpenInfo->bIsDirectory )
        return FALSE;
    if( (poOpenInfo->nOpenFlags & GDAL_OF_GNM) == 0 )
        return FALSE;

    char **papszFiles = VSIReadDir( poOpenInfo->pszFilename );
    if( CSLCount(papszFiles) == 0 )
    {
        return FALSE;
    }

    bool bHasMeta(false), bHasGraph(false), bHasFeatures(false);

    // search for base GNM files
    for( int i = 0; papszFiles[i] != NULL; i++ )
    {
        if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
            continue;

        if( EQUAL(CPLGetBasename(papszFiles[i]), GNM_SYSLAYER_META) )
            bHasMeta = true;
        else if( EQUAL(CPLGetBasename(papszFiles[i]), GNM_SYSLAYER_GRAPH) )
            bHasGraph = true;
        else if( EQUAL(CPLGetBasename(papszFiles[i]), GNM_SYSLAYER_FEATURES) )
            bHasFeatures = true;

        if(bHasMeta && bHasGraph && bHasFeatures)
            break;
    }

    CSLDestroy( papszFiles );

    return bHasMeta && bHasGraph && bHasFeatures;
}

static GDALDataset *GNMFileDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !GNMFileDriverIdentify(poOpenInfo) )
        return NULL;

    GNMFileNetwork* poFN = new GNMFileNetwork();

    if( poFN->Open( poOpenInfo ) != CE_None)
    {
        delete poFN;
        poFN = NULL;
    }

    return poFN;
}


static GDALDataset *GNMFileDriverCreate( const char * pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    CPLAssert( NULL != pszName );
    CPLDebug( "GNM", "Attempt to create network at: %s", pszName );

    GNMFileNetwork *poFN = new GNMFileNetwork();

    if( poFN->Create( pszName, papszOptions ) != CE_None )
    {
        delete poFN;
        poFN = NULL;
    }

    return poFN;
}

static CPLErr GNMFileDriverDelete( const char *pszDataSource )

{
    GDALOpenInfo oOpenInfo(pszDataSource, GA_Update);
    GNMFileNetwork oFN;

    if( oFN.Open( &oOpenInfo ) != CE_None)
    {
        return CE_Failure;
    }

    return oFN.Delete();
}

void RegisterGNMFile()
{
    if( GDALGetDriverByName( "GNMFile" ) == NULL )
    {
        GDALDriver  *poDriver = new GDALDriver();

        poDriver->SetDescription( "GNMFile" );
        poDriver->SetMetadataItem( GDAL_DCAP_GNM, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Geographic Network generic file based "
                                   "model" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, CPLSPrintf(
"<CreationOptionList>"
"  <Option name='%s' type='string' description='The network name. Also it will be a folder name, so the limits for folder name distribute on network name'/>"
"  <Option name='%s' type='string' description='The network description. Any text describes the network'/>"
"  <Option name='%s' type='string' description='The network Spatial reference. All network features will reproject to this spatial reference. May be a WKT text or EPSG code'/>"
"  <Option name='FORMAT' type='string' description='The OGR format to store network data.' default='%s'/>"
"  <Option name='OVERWRITE' type='boolean' description='Overwrite exist network or not' default='NO'/>"
"</CreationOptionList>", GNM_MD_NAME, GNM_MD_DESCR, GNM_MD_SRS,
                                       GNM_MD_DEFAULT_FILE_FORMAT) );

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
                                   "<LayerCreationOptionList/>" );
        poDriver->pfnOpen = GNMFileDriverOpen;
        poDriver->pfnIdentify = GNMFileDriverIdentify;
        poDriver->pfnCreate = GNMFileDriverCreate;
        poDriver->pfnDelete = GNMFileDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
