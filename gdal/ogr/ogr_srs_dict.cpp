/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implement importFromDict() method to read a WKT SRS from a 
 *           coordinate system dictionary in a simple text format. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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

#include "ogr_spatialref.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");


/************************************************************************/
/*                           importFromDict()                           */
/************************************************************************/

/**
 * Read SRS from WKT dictionary.
 *
 * This method will attempt to find the indicated coordinate system identity 
 * in the indicated dictionary file.  If found, the WKT representation is
 * imported and used to initialize this OGRSpatialReference.  
 *
 * More complete information on the format of the dictionary files can
 * be found in the epsg.wkt file in the GDAL data tree.  The dictionary
 * files are searched for in the "GDAL" domain using CPLFindFile().  Normally
 * this results in searching /usr/local/share/gdal or somewhere similar. 
 *
 * This method is the same as the C function OSRImportFromDict().
 *
 * @param pszDictFile the name of the dictionary file to load.  
 *
 * @param pszCode the code to lookup in the dictionary.
 *
 * @return OGRERR_NONE on success, or OGRERR_SRS_UNSUPPORTED if the code isn't
 * found, and OGRERR_SRS_FAILURE if something more dramatic goes wrong.
 */

OGRErr OGRSpatialReference::importFromDict( const char *pszDictFile, 
                                            const char *pszCode )

{
    const char *pszFilename;
    FILE *fp;
    OGRErr eErr = OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Find and open file.                                             */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFindFile( "gdal", pszDictFile );
    if( pszFilename == NULL )
        return OGRERR_UNSUPPORTED_SRS;

    fp = VSIFOpen( pszFilename, "rb" );
    if( fp == NULL )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Process lines.                                                  */
/* -------------------------------------------------------------------- */
    const char *pszLine;

    while( (pszLine = CPLReadLine(fp)) != NULL )

    {
        if( pszLine[0] == '#' )
            /* do nothing */;

        else if( EQUALN(pszLine,"include ",8) )
        {
            eErr = importFromDict( pszLine + 8, pszCode );
            if( eErr != OGRERR_UNSUPPORTED_SRS )
                break;
        }

        else if( strstr(pszLine,",") == NULL )
            /* do nothing */;

        else if( EQUALN(pszLine,pszCode,strlen(pszCode))
                 && pszLine[strlen(pszCode)] == ',' )
        {
            char *pszWKT = (char *) pszLine + strlen(pszCode)+1;

            eErr = importFromWkt( &pszWKT );
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );
    
    return eErr;
}

/************************************************************************/
/*                         OSRImportFromDict()                          */
/************************************************************************/

OGRErr OSRImportFromDict( OGRSpatialReferenceH hSRS, 
                          const char *pszDictFile, 
                          const char *pszCode )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromDict", CE_Failure );

    return ((OGRSpatialReference *) hSRS)->importFromDict( pszDictFile,
                                                           pszCode );
}
