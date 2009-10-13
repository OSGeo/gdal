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
 ****************************************************************************/

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
        && strlen(pszFilename) > 4
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

        if (EQUAL(CPLGetExtension(oSubFilename),"csvt"))
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

    const char* pszLine = CPLReadLine( fp );
    if (pszLine == NULL)
    {
        VSIFClose( fp );
        return FALSE;
    }
    char chDelimiter = CSVDetectSeperator(pszLine);
    VSIRewind( fp );

    char **papszFields = CSVReadParseLine2( fp, chDelimiter );
						
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
        new OGRCSVLayer( CPLGetBasename(pszFilename), fp, pszFilename, FALSE, bUpdate, chDelimiter );

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
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if (!bUpdate)
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

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
                  pszLayerName, pszFilename );
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


    const char *pszDelimiter = CSLFetchNameValue( papszOptions, "SEPARATOR");
    char chDelimiter = ',';
    if (pszDelimiter != NULL)
    {
        if (EQUAL(pszDelimiter, "COMMA"))
            chDelimiter = ',';
        else if (EQUAL(pszDelimiter, "SEMICOLON"))
            chDelimiter = ';';
        else if (EQUAL(pszDelimiter, "TAB"))
            chDelimiter = '\t';
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                  "SEPARATOR=%s not understood, use one of COMMA, SEMICOLON or TAB.",
                  pszDelimiter );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    nLayers++;
    papoLayers = (OGRCSVLayer **) CPLRealloc(papoLayers, 
                                             sizeof(void*) * nLayers);
    
    papoLayers[nLayers-1] = new OGRCSVLayer( pszLayerName, fp, pszFilename, TRUE, TRUE, chDelimiter );

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

/* -------------------------------------------------------------------- */
/*      Should we write the geometry ?                                  */
/* -------------------------------------------------------------------- */
    const char *pszGeometry = CSLFetchNameValue( papszOptions, "GEOMETRY");
    if (pszGeometry != NULL)
    {
        if (EQUAL(pszGeometry, "AS_WKT"))
        {
            papoLayers[nLayers-1]->SetWriteGeometry(OGR_CSV_GEOM_AS_WKT);
        }
        else if (EQUAL(pszGeometry, "AS_XYZ") ||
                 EQUAL(pszGeometry, "AS_XY") ||
                 EQUAL(pszGeometry, "AS_YX"))
        {
            if (eGType == wkbUnknown || wkbFlatten(eGType) == wkbPoint)
            {
                papoLayers[nLayers-1]->SetWriteGeometry(EQUAL(pszGeometry, "AS_XYZ") ? OGR_CSV_GEOM_AS_XYZ :
                                                        EQUAL(pszGeometry, "AS_XY") ?  OGR_CSV_GEOM_AS_XY :
                                                                                       OGR_CSV_GEOM_AS_YX);
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Geometry type %s is not compatible with GEOMETRY=AS_XYZ.",
                          OGRGeometryTypeToName(eGType) );
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Unsupported value %s for creation option GEOMETRY",
                       pszGeometry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we create a CSVT file ?                                  */
/* -------------------------------------------------------------------- */

    const char *pszCreateCSVT = CSLFetchNameValue( papszOptions, "CREATE_CSVT");
    if (pszCreateCSVT)
        papoLayers[nLayers-1]->SetCreateCSVT(CSLTestBoolean(pszCreateCSVT));

    return papoLayers[nLayers-1];
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRCSVDataSource::DeleteLayer( int iLayer )

{
    char *pszFilename;
    char *pszFilenameCSVT;

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "Layer %d cannot be deleted.\n",
                  pszName, iLayer );

        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Layer %d not in legal range of 0 to %d.", 
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    pszFilename = 
        CPLStrdup(CPLFormFilename(pszName,papoLayers[iLayer]->GetLayerDefn()->GetName(),"csv"));
    pszFilenameCSVT = 
        CPLStrdup(CPLFormFilename(pszName,papoLayers[iLayer]->GetLayerDefn()->GetName(),"csvt"));

    delete papoLayers[iLayer];

    while( iLayer < nLayers - 1 )
    {
        papoLayers[iLayer] = papoLayers[iLayer+1];
        iLayer++;
    }

    nLayers--;

    VSIUnlink( pszFilename );
    CPLFree( pszFilename );
    VSIUnlink( pszFilenameCSVT );
    CPLFree( pszFilenameCSVT );

    return OGRERR_NONE;
}
