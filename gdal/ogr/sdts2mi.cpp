/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SDTS To Mapinfo Translator
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static void Usage();

static int TranslateLayer( OGRDataSource *poSrcDS, 
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char  *pszFormat = "Mapinfo File";
    const char  *pszDataSource = NULL;
    const char  *pszDestDataSource = NULL;
    char        **papszLayers = NULL;
    int         bList = FALSE;
    
/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    RegisterOGRSDTS();
    RegisterOGRTAB();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-l") )
        {
            bList = TRUE;
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage();
        }
        else if( pszDataSource == NULL )
            pszDataSource = papszArgv[iArg];
        else if( pszDestDataSource == NULL )
            pszDestDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString( papszLayers, papszArgv[iArg] );
    }

    if( pszDataSource == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poDS;

    poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE );

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
        
        printf( "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                pszDataSource );

        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            printf( "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
        }

        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    OGRSFDriverRegistrar        *poR = OGRSFDriverRegistrar::GetRegistrar();
    OGRSFDriver                 *poDriver = NULL;
    int                         iDriver;

    for( iDriver = 0;
         iDriver < poR->GetDriverCount() && poDriver == NULL;
         iDriver++ )
    {
        if( EQUAL(poR->GetDriver(iDriver)->GetName(),pszFormat) )
        {
            poDriver = poR->GetDriver(iDriver);
        }
    }

    if( !poDriver->TestCapability( ODrCCreateDataSource ) )
    {
        printf( "%s driver does not support data source creation.\n",
                pszFormat );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Create the output data source.                                  */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poODS;
    
    poODS = poDriver->CreateDataSource( pszDestDataSource );
    if( poODS == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
    {
        OGRLayer        *poLayer = poDS->GetLayer(iLayer);

        if( poLayer == NULL )
        {
            printf( "FAILURE: Couldn't fetch advertised layer %d!\n",
                    iLayer );
            exit( 1 );
        }

        /* skip attribute layers. */
        if( CSLCount(papszLayers) == 0 
            && poLayer->GetLayerDefn()->GetGeomType() == wkbNone )
            continue;

        /* skip node layers */
        if( CSLCount(papszLayers) == 0 
            && poLayer->GetLayerDefn()->GetGeomType() == wkbPoint )
            continue;

        /* skip polygon layers without attributes */
        if( CSLCount(papszLayers) == 0
            && poLayer->GetLayerDefn()->GetGeomType() == wkbPolygon
            && poLayer->GetLayerDefn()->GetFieldCount() == 0 )
            continue;

        if( CSLCount(papszLayers) == 0
            || CSLFindString( papszLayers,
                              poLayer->GetLayerDefn()->GetName() ) != -1 )
        {
            if( !TranslateLayer( poDS, poLayer, poODS ) )
                exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    delete poODS;
    delete poDS;

#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    return 0;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf(
    "Usage: sdts2mi [-l] ????CATD.DDF dest_mif_file [layer [layer...]]\n"
    "\n"
    "To select an SDTS transfer, select the *CATD.DDF (or *catd.ddf) file.\n"
    "By default all line (and if attributed polygon) layers are translated.\n"
    "List layer names (such as LE01) to get specific layer information.\n"
    "Use the -l option to list all available layers.\n"
    "\n"
    "Please contact Frank Warmerdam (warmerda@home.com) if you\n"
    "encounter problems.\n" );
    
    exit( 1 );
}

/************************************************************************/
/*                           TranslateLayer()                           */
/************************************************************************/

static int TranslateLayer( OGRDataSource *poSrcDS, 
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS )

{
    OGRLayer    *poDstLayer;
    OGRFeatureDefn *poFDefn;
    
/* -------------------------------------------------------------------- */
/*      Create the layer.                                               */
/* -------------------------------------------------------------------- */
    CPLAssert( poDstDS->TestCapability( ODsCCreateLayer ) );
    poFDefn = poSrcLayer->GetLayerDefn();

    poDstLayer = poDstDS->CreateLayer( poSrcLayer->GetLayerDefn()->GetName(),
                                       poSrcLayer->GetSpatialRef(),
                                       poFDefn->GetGeomType(),
                                       NULL );

    if( poDstLayer == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Add fields.                                                     */
/* -------------------------------------------------------------------- */
    int         iField;

    for( iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
    {
        if( poDstLayer->CreateField( poFDefn->GetFieldDefn(iField) )
            != OGRERR_NONE )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;
    
    poSrcLayer->ResetReading();
    
    while( (poFeature = poSrcLayer->GetNextFeature()) != NULL )
    {
        OGRFeature      *poDstFeature;

        poDstFeature = new OGRFeature( poDstLayer->GetLayerDefn() );

        if( poDstFeature->SetFrom( poFeature ) != OGRERR_NONE )
        {
            delete poFeature;
            
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to translate feature %d from layer %s.\n",
                      poFeature->GetFID(), poFDefn->GetName() );
            return FALSE;
        }
        
        delete poFeature;
        
        if( poDstLayer->CreateFeature( poDstFeature ) != OGRERR_NONE )
        {
            delete poDstFeature;
            return FALSE;
        }

        delete poDstFeature;
    }

    return TRUE;
}

