/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerBaseFile class, providing common services to all
 *           the tiger file readers.
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
 * Revision 1.3  2000/01/13 05:18:11  warmerda
 * added support for multiple versions
 *
 * Revision 1.2  1999/12/22 15:38:15  warmerda
 * major update
 *
 * Revision 1.1  1999/10/07 18:19:21  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

/************************************************************************/
/*                           TigerFileBase()                            */
/************************************************************************/

TigerFileBase::TigerFileBase()

{
    pszShortModule = NULL;
    pszModule = NULL;
    fpPrimary = NULL;
    poFeatureDefn = NULL;
    nFeatures = 0;
}

/************************************************************************/
/*                           ~TigerFileBase()                           */
/************************************************************************/

TigerFileBase::~TigerFileBase()

{
    CPLFree( pszModule );
    CPLFree( pszShortModule );
}

/************************************************************************/
/*                              OpenFile()                              */
/************************************************************************/

int TigerFileBase::OpenFile( const char * pszModuleToOpen,
                             const char *pszExtension )

{
    char	*pszFilename;

    CPLFree( pszModule );
    pszModule = NULL;
    CPLFree( pszShortModule );
    pszShortModule = NULL;
    
    if( fpPrimary != NULL )
    {
        VSIFClose( fpPrimary );
        fpPrimary = NULL;
    }

    if( pszModuleToOpen == NULL )
        return TRUE;

    pszFilename = poDS->BuildFilename( pszModuleToOpen, pszExtension );

    fpPrimary = VSIFOpen( pszFilename, "rb" );

    CPLFree( pszFilename );

    if( fpPrimary != NULL )
    {
        pszModule = CPLStrdup(pszModuleToOpen);
        pszShortModule = CPLStrdup(pszModuleToOpen);
        for( int i = 0; pszShortModule[i] != '\0'; i++ )
        {
            if( pszShortModule[i] == '.' )
                pszShortModule[i] = '\0';
        }
        
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                       EstablishFeatureCount()                        */
/************************************************************************/

void TigerFileBase::EstablishFeatureCount()

{
    char	chCurrent;
    
    nRecordLength = 0;
    
    if( fpPrimary == NULL )
        return;

    if( VSIFSeek( fpPrimary, 0, SEEK_SET ) != 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Read through to the end of line.                                */
/* -------------------------------------------------------------------- */
    chCurrent = '\0';
    while( VSIFRead( &chCurrent, 1, 1, fpPrimary ) == 1
           && chCurrent != 10
           && chCurrent != 13 )
    {
        nRecordLength++;
    }

/* -------------------------------------------------------------------- */
/*      Is the file zero length?                                        */
/* -------------------------------------------------------------------- */
    if( nRecordLength == 0 )
    {
        nRecordLength = 1;
        nFeatures = 0;

        return;
    }
    
    nRecordLength++; /* for the 10 or 13 we encountered */

/* -------------------------------------------------------------------- */
/*      Read through line terminator characters.  We are trying to	*/
/*	handle cases of CR, CR/LF and LF/CR gracefully.			*/
/* -------------------------------------------------------------------- */
    while( VSIFRead( &chCurrent, 1, 1, fpPrimary ) == 1
           && (chCurrent == 10 || chCurrent == 13 ) )
    {
        nRecordLength++;
    }

/* -------------------------------------------------------------------- */
/*      Now we think we know the fixed record length for the file       */
/*      (including line terminators).  Get the total file size, and     */
/*      divide by this length to get the presumed number of records.    */
/* -------------------------------------------------------------------- */
    long	nFileSize;
    
    VSIFSeek( fpPrimary, 0, SEEK_END );
    nFileSize = VSIFTell( fpPrimary );

    if( (nFileSize % nRecordLength) != 0 )
    {
        CPLError( CE_Warning, CPLE_FileIO,
                  "TigerFileBase::EstablishFeatureCount(): "
                  "File length %d doesn't divide by record length %d.\n",
                  nFileSize, nRecordLength );
    }

    nFeatures = nFileSize / nRecordLength;
}

/************************************************************************/
/*                              GetField()                              */
/************************************************************************/

const char *TigerFileBase::GetField( const char * pachRawDataRecord,
                                     int nStartChar, int nEndChar )

{
    static char 	aszField[128];
    int			nLength = nEndChar - nStartChar + 1;
    
    CPLAssert( nEndChar - nStartChar + 2 < (int) sizeof(aszField) );

    strncpy( aszField, pachRawDataRecord + nStartChar - 1, nLength );

    aszField[nLength] = '\0';
    while( nLength > 0 && aszField[nLength-1] == ' ' )
        aszField[--nLength] = '\0';

    return aszField;
}

/************************************************************************/
/*                              SetField()                              */
/*                                                                      */
/*      Set a field on an OGRFeature from a tiger record, or leave      */
/*      NULL if the value isn't found.                                  */
/************************************************************************/

void TigerFileBase::SetField( OGRFeature *poFeature, const char *pszField,
                              const char *pachRecord, int nStart, int nEnd )

{
    const char *pszFieldValue = GetField( pachRecord, nStart, nEnd );

    if( pszFieldValue[0] == '\0' )
        return;

    poFeature->SetField( pszField, pszFieldValue );
}
