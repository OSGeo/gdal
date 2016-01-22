/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing OGR driver data.
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

#include "ogr_tiger.h"

CPL_CVSID("$Id$");

int     bReadOnly = FALSE;
int     bVerbose = TRUE;

static void Usage();

static void ReportOnLayer( OGRLayer * );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char  *pszDataSource = NULL;
    char        **papszLayers = NULL;
    
/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    RegisterOGRTiger();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-ro") )
            bReadOnly = TRUE;
        else if( EQUAL(papszArgv[iArg],"-q") )
            bVerbose = FALSE;
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage();
        }
        else if( pszDataSource == NULL )
            pszDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString( papszLayers, papszArgv[iArg] );
    }

    if( pszDataSource == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poDS;
    OGRSFDriver         *poDriver;

    poDS = OGRSFDriverRegistrar::Open( pszDataSource, !bReadOnly, &poDriver );
    if( poDS == NULL && !bReadOnly )
    {
        poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE, &poDriver );
        if( poDS != NULL && bVerbose )
        {
            printf( "Had to open data source read-only.\n" );
            bReadOnly = TRUE;
        }
    }

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
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( bVerbose )
    {
        printf( "INFO: Open of `%s'\n"
                "using driver `%s' successful.\n",
                pszDataSource, poDriver->GetName() );
        printf("Tiger Version: %s\n", 
               TigerVersionString(((OGRTigerDataSource*)poDS)->GetVersion()));
    }



    if( bVerbose && !EQUAL(pszDataSource,poDS->GetName()) )
    {
        printf( "INFO: Internal data source name `%s'\n"
                "      different from user name `%s'.\n",
                poDS->GetName(), pszDataSource );
    }


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

        if( CSLCount(papszLayers) == 0 )
        {
            printf( "%d: %s\n",
                    iLayer+1,
                    poLayer->GetLayerDefn()->GetName() );
        }
        else if( CSLFindString( papszLayers,
                                poLayer->GetLayerDefn()->GetName() ) != -1 )
        {
            ReportOnLayer( poLayer );
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
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
    printf( "Usage: ogrinfo [-ro] [-q] datasource_name [layer [layer ...]]\n");
    exit( 1 );
}

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

static void ReportOnLayer( OGRLayer * poLayer )

{
    OGRFeatureDefn      *poDefn = poLayer->GetLayerDefn();

    printf( "\n" );
    
    printf( "Layer name: %s\n", poDefn->GetName() );

    printf( "Feature Count: %d\n", poLayer->GetFeatureCount() );

    if( bVerbose )
    {
        char    *pszWKT;
        
        if( poLayer->GetSpatialRef() == NULL )
            pszWKT = CPLStrdup( "(NULL)" );
        else
            poLayer->GetSpatialRef()->exportToWkt( &pszWKT );

        printf( "Layer SRS WKT: %s\n", pszWKT );
        CPLFree( pszWKT );
    }
    
    for( int iAttr = 0; iAttr < poDefn->GetFieldCount(); iAttr++ )
    {
        OGRFieldDefn    *poField = poDefn->GetFieldDefn( iAttr );

        printf( "%s: %s (%d.%d)\n",
                poField->GetNameRef(),
                poField->GetFieldTypeName( poField->GetType() ),
                poField->GetWidth(),
                poField->GetPrecision() );
    }

/* -------------------------------------------------------------------- */
/*      Read, and dump features.                                        */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        poFeature->DumpReadable( stdout );
        delete poFeature;
    }
}
