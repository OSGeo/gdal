/******************************************************************************
 * $Id$
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFDataSource class
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/10/07 18:19:21  warmerda
 * New
 *
 * Revision 1.6  1999/10/04 03:08:52  warmerda
 * added raster support
 *
 * Revision 1.5  1999/10/01 14:47:51  warmerda
 * major upgrade: generic, string feature codes, etc
 *
 * Revision 1.4  1999/09/29 16:43:43  warmerda
 * added spatial ref, improved test open for non-os files
 *
 * Revision 1.3  1999/09/08 00:58:40  warmerda
 * Added limiting list of files for FME.
 *
 * Revision 1.2  1999/08/30 16:49:26  warmerda
 * added feature class layer support
 *
 * Revision 1.1  1999/08/28 03:13:35  warmerda
 * New
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include <ctype.h>

/************************************************************************/
/*                          OGRTigerDataSource()                          */
/************************************************************************/

OGRTigerDataSource::OGRTigerDataSource()

{
    nLayers = 0;
    papoLayers = NULL;

    nModules = 0;
    papszModules = NULL;

    pszName = NULL;

    papszOptions = NULL;

    poSpatialRef = new OGRSpatialReference( "GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\",SPHEROID[\"GRS 1980\",6378137,298.257222101]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]" );
}

/************************************************************************/
/*                         ~OGRTigerDataSource()                          */
/************************************************************************/

OGRTigerDataSource::~OGRTigerDataSource()

{
    int		i;

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    CPLFree( pszName );

    CSLDestroy( papszOptions );

    delete poSpatialRef;
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void OGRTigerDataSource::AddLayer( OGRTigerLayer * poNewLayer )

{
    papoLayers = (OGRTigerLayer **)
        CPLRealloc( papoLayers, sizeof(void*) * ++nLayers );
    
    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRTigerDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRTigerDataSource::GetLayerCount()

{
    return nLayers;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRTigerDataSource::Open( const char * pszFilename, int bTestOpen,
                              char ** papszLimitedFileList )

{
    VSIStatBuf      stat;
    char	    **papszFileList = NULL;
    int		    i;

    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszFilename, &stat ) != 0 
        || (!VSI_ISDIR(stat.st_mode) && !VSI_ISREG(stat.st_mode)) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                   "%s is neither a file or directory, Tiger access failed.\n",
                      pszFilename );

        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Build a list of filenames we figure are Tiger files.            */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(stat.st_mode) )
    {
        // notdef: add parsing of full filename into path and module.
        CPLAssert( FALSE );
    }
    else
    {
        char      **candidateFileList = CPLReadDir( pszFilename );
        int         i;

        pszPath = CPLStrdup( pszFilename );

        for( i = 0; 
             candidateFileList != NULL && candidateFileList[i] != NULL; 
             i++ ) 
        {
            if( papszLimitedFileList != NULL
                && CSLFindString(papszLimitedFileList,
                                 candidateFileList[i]) == -1 )
            {
                continue;
            }
            
            if( EQUALN(candidateFileList[i] + strlen(candidateFileList[i])-4,
                       ".RT1",4) )
            {
                char       szModule[128];

                strncpy( szModule, candidateFileList[i],
                         strlen(candidateFileList[i])-4 );

                szModule[strlen(candidateFileList[i])-4] = '\0';

                papszFileList = CSLAddString(papszFileList, szModule);
            }
        }

        if( CSLCount(papszFileList) == 0 )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "No candidate Tiger files (.RT1) found in\n"
                          "directory: %s",
                          pszFilename );

            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop over all these files trying to open them.  In testopen     */
/*      mode we first read the first 80 characters, to verify that      */
/*      it looks like an Tiger file.  Note that we don't keep the file  */
/*      open ... we don't want to occupy alot of file handles when      */
/*      handling a whole directory.                                     */
/* -------------------------------------------------------------------- */
    papszModules = NULL;
    
    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        if( bTestOpen )
        {
            char	szHeader[80];
            FILE	*fp;
            int		nVersion;
            char	*pszFilename;

            pszFilename = BuildFilename( papszFileList[i], "RT1" );

            fp = VSIFOpen( pszFilename, "rb" );
            CPLFree( pszFilename );
            if( fp == NULL )
                continue;
            
            if( VSIFRead( szHeader, 80, 1, fp ) < 1 )
            {
                VSIFClose( fp );
                continue;
            }

            VSIFClose( fp );
            
            if( szHeader[0] != '1' )
                continue;

            if( !isdigit(szHeader[1]) || !isdigit(szHeader[2])
                || !isdigit(szHeader[3]) || !isdigit(szHeader[4]) )
                continue;

            nVersion = atoi(TigerFileBase::GetField( szHeader, 2, 5 ));

            if( nVersion != 0 && nVersion != 2 && nVersion != 3
                && nVersion != 5 && nVersion != 21 && nVersion != 24
                && szHeader[3] != '9' && szHeader[3] != '0' )
                continue;

            // we could (and should) add a bunch more validation here.
        }

        papszModules = CSLAddString( papszModules, papszFileList[i] );
    }

    CSLDestroy( papszFileList );

    nModules = CSLCount( papszModules );

    if( nModules == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create the layers which appear to exist.                        */
/* -------------------------------------------------------------------- */
    AddLayer( new OGRTigerLayer( this,
                                 new TigerCompleteChain( this,
                                                         papszModules[0]) ));
    
    return TRUE;
}

/************************************************************************/
/*                             SetOptions()                             */
/************************************************************************/

void OGRTigerDataSource::SetOptionList( char ** papszNewOptions )

{
    CSLDestroy( papszOptions );
    papszOptions = CSLDuplicate( papszNewOptions );
}

/************************************************************************/
/*                             GetOption()                              */
/************************************************************************/

const char *OGRTigerDataSource::GetOption( const char * pszOption )

{
    return CSLFetchNameValue( papszOptions, pszOption );
}

/************************************************************************/
/*                             GetModule()                              */
/************************************************************************/

const char *OGRTigerDataSource::GetModule( int iModule )

{
    if( iModule < 0 || iModule >= nModules )
        return NULL;
    else
        return papszModules[iModule];
}

/************************************************************************/
/*                           BuildFilename()                            */
/************************************************************************/

char *OGRTigerDataSource::BuildFilename( const char *pszModuleName,
                                    const char *pszExtension )

{
    char	*pszFilename;

    pszFilename = (char *) CPLMalloc(strlen(GetDirPath())
                                     + strlen(pszModuleName)
                                     + strlen(pszExtension) + 10);

    sprintf( pszFilename, "%s/%s.%s",
             GetDirPath(), pszModuleName, pszExtension );

    return pszFilename;
}

