/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57FileCollector() function.  This function collects
 *           a list of S-57 data files based on the contents of a directory,
 *           catalog file, or direct reference to an S-57 file.
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
 * Revision 1.1  1999/11/18 18:57:13  warmerda
 * New
 *
 */

#include "s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                          S57FileCollector()                          */
/************************************************************************/

char **S57FileCollector( const char *pszDataset )

{
    VSIStatBuf	sStatBuf;
    char	**papszRetList = NULL;

/* -------------------------------------------------------------------- */
/*      Stat the dataset, and fail if it isn't a file or directory.     */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszDataset, &sStatBuf ) )
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
        char	**papszDirFiles = CPLReadDir( pszDataset );
        int	iFile;
        DDFModule oModule;

        for( iFile = 0;
             papszDirFiles != NULL && papszDirFiles[iFile] != NULL;
             iFile++ )
        {
            char	*pszFullFile;

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
/*      If this is a regular file, but not a catalog just return it.  	*/
/*	Note that the caller may still open it and fail.		*/
/* -------------------------------------------------------------------- */
    DDFModule	oModule;

    if( !oModule.Open( pszDataset, TRUE )
        || oModule.FindFieldDefn( "CATD" ) == NULL
        || oModule.FindFieldDefn("CATD")->FindSubfieldDefn( "IMPL" ) == NULL )
    {
        papszRetList = CSLAddString( papszRetList, pszDataset );
        return papszRetList;
    }

/* -------------------------------------------------------------------- */
/*      We have a catalog.  Scan it for data files, those with an       */
/*      IMPL of BIN.  Shouldn't there be a better way of testing        */
/*      whether a file is a data file or another catalog file?          */
/* -------------------------------------------------------------------- */
    DDFRecord	*poRecord;
    char	*pszDir = CPLStrdup( CPLGetPath( pszDataset ) );
    
    for( poRecord = oModule.ReadRecord();
         poRecord != NULL;
         poRecord = oModule.ReadRecord() )
    {
        if( poRecord->FindField( "CATD" ) != NULL
            && EQUAL(poRecord->GetStringSubfield("CATD",0,"IMPL",0),"BIN") )
        {
            const char	*pszFile;

            pszFile = poRecord->GetStringSubfield("CATD",0,"FILE",0);
            
            papszRetList =
                CSLAddString( papszRetList, 
                              CPLFormFilename( pszDir, pszFile, NULL ) );
        }
    }

    CPLFree( pszDir );

    return papszRetList;
}

