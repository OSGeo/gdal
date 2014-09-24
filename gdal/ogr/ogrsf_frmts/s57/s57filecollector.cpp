/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57FileCollector() function.  This function collects
 *           a list of S-57 data files based on the contents of a directory,
 *           catalog file, or direct reference to an S-57 file.
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

#include "s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          S57FileCollector()                          */
/************************************************************************/

char **S57FileCollector( const char *pszDataset )

{
    VSIStatBuf  sStatBuf;
    char        **papszRetList = NULL;

/* -------------------------------------------------------------------- */
/*      Stat the dataset, and fail if it isn't a file or directory.     */
/* -------------------------------------------------------------------- */
    if( CPLStat( pszDataset, &sStatBuf ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No S-57 files found, %s\nisn't a directory or a file.\n",
                  pszDataset );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We handle directories by scanning for all S-57 data files in    */
/*      them, but not for catalogs.                                     */
/* -------------------------------------------------------------------- */
    if( VSI_ISDIR(sStatBuf.st_mode) )
    {
        char    **papszDirFiles = CPLReadDir( pszDataset );
        int     iFile;
        DDFModule oModule;

        for( iFile = 0;
             papszDirFiles != NULL && papszDirFiles[iFile] != NULL;
             iFile++ )
        {
            char        *pszFullFile;

            pszFullFile = CPLStrdup(
                CPLFormFilename( pszDataset, papszDirFiles[iFile], NULL ) );

            // Add to list if it is an S-57 _data_ file.
            if( VSIStat( pszFullFile, &sStatBuf ) == 0 
                && VSI_ISREG( sStatBuf.st_mode )
                && oModule.Open( pszFullFile, TRUE ) )
            {
                if( oModule.FindFieldDefn("DSID") != NULL )
                    papszRetList = CSLAddString( papszRetList, pszFullFile );
            }

            CPLFree( pszFullFile );
        }

        return papszRetList;
    }

/* -------------------------------------------------------------------- */
/*      If this is a regular file, but not a catalog just return it.    */
/*      Note that the caller may still open it and fail.                */
/* -------------------------------------------------------------------- */
    DDFModule   oModule;
    DDFRecord   *poRecord;

    if( !oModule.Open(pszDataset) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "The file %s isn't an S-57 data file, or catalog.\n",
                  pszDataset );

        return NULL;
    }

    poRecord = oModule.ReadRecord();
    if( poRecord == NULL )
        return NULL;
    
    if( poRecord->FindField( "CATD" ) == NULL
        || oModule.FindFieldDefn("CATD")->FindSubfieldDefn( "IMPL" ) == NULL )
    {
        papszRetList = CSLAddString( papszRetList, pszDataset );
        return papszRetList;
    }


/* -------------------------------------------------------------------- */
/*      We presumably have a catalog.  It contains paths to files       */
/*      that generally lack the ENC_ROOT component.  Try to find the    */
/*      correct name for the ENC_ROOT directory if available and        */
/*      build a base path for our purposes.                             */
/* -------------------------------------------------------------------- */
    char        *pszCatDir = CPLStrdup( CPLGetPath( pszDataset ) );
    char        *pszRootDir = NULL;
    
    if( CPLStat( CPLFormFilename(pszCatDir,"ENC_ROOT",NULL), &sStatBuf ) == 0
        && VSI_ISDIR(sStatBuf.st_mode) )
    {
        pszRootDir = CPLStrdup(CPLFormFilename( pszCatDir, "ENC_ROOT", NULL ));
    }
    else if( CPLStat( CPLFormFilename( pszCatDir, "enc_root", NULL ), 
                      &sStatBuf ) == 0 && VSI_ISDIR(sStatBuf.st_mode) )
    {
        pszRootDir = CPLStrdup(CPLFormFilename( pszCatDir, "enc_root", NULL ));
    }

    if( pszRootDir )
        CPLDebug( "S57", "Found root directory to be %s.", 
                  pszRootDir );

/* -------------------------------------------------------------------- */
/*      We have a catalog.  Scan it for data files, those with an       */
/*      IMPL of BIN.  Shouldn't there be a better way of testing        */
/*      whether a file is a data file or another catalog file?          */
/* -------------------------------------------------------------------- */
    for( ; poRecord != NULL; poRecord = oModule.ReadRecord() )
    {
        if( poRecord->FindField( "CATD" ) != NULL
            && EQUAL(poRecord->GetStringSubfield("CATD",0,"IMPL",0),"BIN") )
        {
            const char  *pszFile, *pszWholePath;

            pszFile = poRecord->GetStringSubfield("CATD",0,"FILE",0);

            // Often there is an extra ENC_ROOT in the path, try finding 
            // this file. 

            pszWholePath = CPLFormFilename( pszCatDir, pszFile, NULL );
            if( CPLStat( pszWholePath, &sStatBuf ) != 0
                && pszRootDir != NULL )
            {
                pszWholePath = CPLFormFilename( pszRootDir, pszFile, NULL );
            }
                
            if( CPLStat( pszWholePath, &sStatBuf ) != 0 )
            {
                CPLError( CE_Warning, CPLE_OpenFailed,
                          "Can't find file %s from catalog %s.", 
                          pszFile, pszDataset );
                continue;
            }

            papszRetList = CSLAddString( papszRetList, pszWholePath );
            CPLDebug( "S57", "Got path %s from CATALOG.", pszWholePath );
        }
    }

    CPLFree( pszCatDir );
    CPLFree( pszRootDir );

    return papszRetList;
}

