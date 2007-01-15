/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.10  2005/11/03 21:06:41  fwarmerdam
 * Fixed last change.
 *
 * Revision 1.9  2005/11/03 14:22:48  fwarmerdam
 * Use large file API to stat file.
 *
 * Revision 1.8  2005/09/21 01:01:01  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.7  2005/06/20 17:54:04  fwarmerdam
 * added support for external csvt file
 *
 * Revision 1.6  2004/08/17 21:03:03  warmerda
 * Avoid leak of papoLayers array.
 *
 * Revision 1.5  2004/08/17 15:40:40  warmerda
 * track capabilities and update mode better
 *
 * Revision 1.4  2004/08/16 21:29:48  warmerda
 * added output support
 *
 * Revision 1.3  2004/07/31 04:50:22  warmerda
 * started write support
 *
 * Revision 1.2  2004/07/20 20:53:26  warmerda
 * added support for reading directories of CSV files
 *
 * Revision 1.1  2004/07/20 19:18:23  warmerda
 * New
 *
 */

#include "ogr_csv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::OGRCSVDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;

    bUpdate = FALSE;
}

/************************************************************************/
/*                         ~OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::~OGRCSVDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bUpdate;
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return bUpdate;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRCSVDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSVDataSource::Open( const char * pszFilename, int bUpdateIn,
                            int bForceOpen )

{
    pszName = CPLStrdup( pszFilename );
    bUpdate = bUpdateIn;

/* -------------------------------------------------------------------- */
/*      Determine what sort of object this is.                          */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) != 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Is this a single CSV file?                                      */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(sStatBuf.st_mode)
        && EQUAL(pszFilename+strlen(pszFilename)-4,".csv") )
        return OpenTable( pszFilename );

/* -------------------------------------------------------------------- */
/*      Otherwise it has to be a directory.                             */
/* -------------------------------------------------------------------- */
    if( !VSI_ISDIR(sStatBuf.st_mode) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Scan through for entries ending in .csv.                        */
/* -------------------------------------------------------------------- */
    int nNotCSVCount = 0, i;
    char **papszNames = CPLReadDir( pszFilename );

    for( i = 0; papszNames != NULL && papszNames[i] != NULL; i++ )
    {
        CPLString oSubFilename = 
            CPLFormFilename( pszFilename, papszNames[i], NULL );

        if( EQUAL(papszNames[i],".") || EQUAL(papszNames[i],"..") )
            continue;

        if( VSIStatL( oSubFilename, &sStatBuf ) != 0 
            || !VSI_ISREG(sStatBuf.st_mode) 
            || !EQUAL(CPLGetExtension(oSubFilename),"csv") )
        {
            nNotCSVCount++;
            continue;
        }

        if( !OpenTable( oSubFilename ) )
        {
            CSLDestroy( papszNames );
            nNotCSVCount++;
            return FALSE;
        }
    }

    CSLDestroy( papszNames );

/* -------------------------------------------------------------------- */
/*      We presume that this is indeed intended to be a CSV             */
/*      datasource if over half the files were .csv files.              */
/* -------------------------------------------------------------------- */
    return bForceOpen || nNotCSVCount < nLayers;
}

/************************************************************************/
/*                              OpenTable()                             */
/************************************************************************/

int OGRCSVDataSource::OpenTable( const char * pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    FILE       * fp;

    if( bUpdate )
        fp = VSIFOpen( pszFilename, "rb+" );
    else
        fp = VSIFOpen( pszFilename, "rb" );
    if( fp == NULL )
    {
        CPLError( CE_Warning, CPLE_OpenFailed, 
                  "Failed to open %s, %s.", 
                  pszFilename, VSIStrerror( errno ) );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read and parse a line.  Did we get multiple fields?             */
/* -------------------------------------------------------------------- */
    char **papszFields = CSVReadParseLine( fp );
						
    if( CSLCount(papszFields) < 2 )
    {
        VSIFClose( fp );
        CSLDestroy( papszFields );
        return FALSE;
    }

    VSIRewind( fp );
    CSLDestroy( papszFields );

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    nLayers++;
    papoLayers = (OGRCSVLayer **) CPLRealloc(papoLayers, 
                                             sizeof(void*) * nLayers);
    
    papoLayers[nLayers-1] = 
        new OGRCSVLayer( CPLGetBasename(pszFilename), fp, pszFilename, FALSE, bUpdate );

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRCSVDataSource::CreateLayer( const char *pszLayerName, 
                               OGRSpatialReference *poSpatialRef,
                               OGRwkbGeometryType eGType,
                               char ** papszOptions  )

{
/* -------------------------------------------------------------------- */
/*      Verify that the datasource is a directory.                      */
/* -------------------------------------------------------------------- */
    VSIStatBuf sStatBuf;

    if( VSIStat( pszName, &sStatBuf ) != 0 
        || !VSI_ISDIR( sStatBuf.st_mode ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create csv layer (file) against a non-directory datasource." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What filename would we use?                                     */
/* -------------------------------------------------------------------- */
    const char *pszFilename;

    pszFilename = CPLFormFilename( pszName, pszLayerName, "csv" );

/* -------------------------------------------------------------------- */
/*      does this file already exist?                                   */
/* -------------------------------------------------------------------- */
    
    if( VSIStat( pszName, &sStatBuf ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create layer %s, but file %s already exists.",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the empty file.                                          */
/* -------------------------------------------------------------------- */
    FILE *fp;

    fp = VSIFOpen( pszFilename, "w+b" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create %s:\n%s", 
                  pszFilename, VSIStrerror( errno ) );
                  
                  
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    nLayers++;
    papoLayers = (OGRCSVLayer **) CPLRealloc(papoLayers, 
                                             sizeof(void*) * nLayers);
    
    papoLayers[nLayers-1] = new OGRCSVLayer( pszLayerName, fp, pszFilename, TRUE, TRUE );

/* -------------------------------------------------------------------- */
/*      Was a partiuclar CRLF order requested?                          */
/* -------------------------------------------------------------------- */
    const char *pszCRLFFormat = CSLFetchNameValue( papszOptions, "LINEFORMAT");
    int bUseCRLF;

    if( pszCRLFFormat == NULL )
    {
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    else if( EQUAL(pszCRLFFormat,"CRLF") )
        bUseCRLF = TRUE;
    else if( EQUAL(pszCRLFFormat,"LF") )
        bUseCRLF = FALSE;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    
    papoLayers[nLayers-1]->SetCRLF( bUseCRLF );

    return papoLayers[nLayers-1];
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRCSVDataSource::DeleteLayer( int iLayer )

{
    char *pszFilename;

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Layer %d not in legal range of 0 to %d.", 
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    pszFilename = 
        CPLStrdup(CPLFormFilename(pszName,papoLayers[iLayer]->GetLayerDefn()->GetName(),"csv"));

    delete papoLayers[iLayer];

    while( iLayer < nLayers - 1 )
    {
        papoLayers[iLayer] = papoLayers[iLayer+1];
        iLayer++;
    }

    nLayers--;

    VSIUnlink( pszFilename );
    CPLFree( pszFilename );

    return OGRERR_NONE;
}
