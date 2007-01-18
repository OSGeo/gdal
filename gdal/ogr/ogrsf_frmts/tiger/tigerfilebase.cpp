/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerBaseFile class, providing common services to all
 *           the tiger file readers.
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

#include "ogr_tiger.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

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
    nVersionCode = 0;
    nVersion = TIGER_Unknown;
}

/************************************************************************/
/*                           ~TigerFileBase()                           */
/************************************************************************/

TigerFileBase::~TigerFileBase()

{
    CPLFree( pszModule );
    CPLFree( pszShortModule );

    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    if( fpPrimary != NULL )
    {
        VSIFClose( fpPrimary );
        fpPrimary = NULL;
    }
}

/************************************************************************/
/*                              OpenFile()                              */
/************************************************************************/

int TigerFileBase::OpenFile( const char * pszModuleToOpen,
                             const char *pszExtension )

{
    char        *pszFilename;

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

        SetupVersion();

        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                            SetupVersion()                            */
/************************************************************************/

void TigerFileBase::SetupVersion()

{
    char        aszRecordHead[6];

    VSIFSeek( fpPrimary, 0, SEEK_SET );
    VSIFRead( aszRecordHead, 1, 5, fpPrimary );
    aszRecordHead[5] = '\0';
    nVersionCode = atoi(aszRecordHead+1);
    VSIFSeek( fpPrimary, 0, SEEK_SET );

    nVersion = TigerClassifyVersion( nVersionCode );
}

/************************************************************************/
/*                       EstablishRecordLength()                        */
/************************************************************************/

int TigerFileBase::EstablishRecordLength( FILE * fp )

{
    char        chCurrent;
    int         nRecLen = 0;
    
    if( fp == NULL || VSIFSeek( fp, 0, SEEK_SET ) != 0 )
        return -1;

/* -------------------------------------------------------------------- */
/*      Read through to the end of line.                                */
/* -------------------------------------------------------------------- */
    chCurrent = '\0';
    while( VSIFRead( &chCurrent, 1, 1, fp ) == 1
           && chCurrent != 10
           && chCurrent != 13 )
    {
        nRecLen++;
    }

/* -------------------------------------------------------------------- */
/*      Is the file zero length?                                        */
/* -------------------------------------------------------------------- */
    if( nRecLen == 0 )
    {
        return -1;
    }
    
    nRecLen++; /* for the 10 or 13 we encountered */

/* -------------------------------------------------------------------- */
/*      Read through line terminator characters.  We are trying to      */
/*      handle cases of CR, CR/LF and LF/CR gracefully.                 */
/* -------------------------------------------------------------------- */
    while( VSIFRead( &chCurrent, 1, 1, fp ) == 1
           && (chCurrent == 10 || chCurrent == 13 ) )
    {
        nRecLen++;
    }

    VSIFSeek( fp, 0, SEEK_SET );

    return nRecLen;
}

/************************************************************************/
/*                       EstablishFeatureCount()                        */
/************************************************************************/

void TigerFileBase::EstablishFeatureCount()

{
    if( fpPrimary == NULL )
        return;

    nRecordLength = EstablishRecordLength( fpPrimary );

    if( nRecordLength == -1 )
    {
        nRecordLength = 1;
        nFeatures = 0;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Now we think we know the fixed record length for the file       */
/*      (including line terminators).  Get the total file size, and     */
/*      divide by this length to get the presumed number of records.    */
/* -------------------------------------------------------------------- */
    long        nFileSize;
    
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
    static char         aszField[128];
    int                 nLength = nEndChar - nStartChar + 1;
    
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

/************************************************************************/
/*                             WriteField()                             */
/*                                                                      */
/*      Write a field into a record buffer with the indicated           */
/*      formatting, or leave blank if not found.                        */
/************************************************************************/

int TigerFileBase::WriteField( OGRFeature *poFeature, const char *pszField, 
                               char *pachRecord, int nStart, int nEnd, 
                               char chFormat, char chType )

{
    int         iField = poFeature->GetFieldIndex( pszField );
    char        szValue[512], szFormat[32];

    CPLAssert( nEnd - nStart + 1 < (int) sizeof(szValue)-1 );

    if( iField < 0 || !poFeature->IsFieldSet( iField ) )
        return FALSE;

    if( chType == 'N' && chFormat == 'L' )
    {
        sprintf( szFormat, "%%0%dd", nEnd - nStart + 1 );
        sprintf( szValue, szFormat, poFeature->GetFieldAsInteger( iField ) );
    }
    else if( chType == 'N' && chFormat == 'R' )
    {
        sprintf( szFormat, "%%%dd", nEnd - nStart + 1 );
        sprintf( szValue, szFormat, poFeature->GetFieldAsInteger( iField ) );
    }
    else if( chType == 'A' && chFormat == 'L' )
    {
        strncpy( szValue, poFeature->GetFieldAsString( iField ), 
                 sizeof(szValue) - 1 );
        if( (int) strlen(szValue) < nEnd - nStart + 1 )
            memset( szValue + strlen(szValue), ' ', 
                    nEnd - nStart + 1 - strlen(szValue) );
    }
    else if( chType == 'A' && chFormat == 'R' )
    {
        sprintf( szFormat, "%%%ds", nEnd - nStart + 1 );
        sprintf( szValue, szFormat, poFeature->GetFieldAsString( iField ) );
    }
    else
    {
        CPLAssert( FALSE );
        return FALSE;
    }

    strncpy( pachRecord + nStart - 1, szValue, nEnd - nStart + 1 );

    return TRUE;
}

/************************************************************************/
/*                             WritePoint()                             */
/************************************************************************/

int TigerFileBase::WritePoint( char *pachRecord, int nStart, 
                               double dfX, double dfY )

{
    char        szTemp[20];

    if( dfX == 0.0 && dfY == 0.0 )
    {
        strncpy( pachRecord + nStart - 1, "+000000000+00000000", 19 );
    }
    else
    {
        sprintf( szTemp, "%+10d%+9d", 
                 (int) floor(dfX * 1000000 + 0.5),
                 (int) floor(dfY * 1000000 + 0.5) );
        strncpy( pachRecord + nStart - 1, szTemp, 19 );
    }

    return TRUE;
}

/************************************************************************/
/*                            WriteRecord()                             */
/************************************************************************/

int TigerFileBase::WriteRecord( char *pachRecord, int nRecLen, 
                                const char *pszType, FILE * fp )

{
    if( fp == NULL )
        fp = fpPrimary;

    pachRecord[0] = *pszType;


    /*
     * Prior to TIGER_2002, type 5 files lacked the version.  So write
     * the version in the record iff we're using TIGER_2002 or higher,
     * or if this is not type "5"
     */
    if ( (poDS->GetVersion() >= TIGER_2002) ||
         (!EQUAL(pszType, "5")) )
    {
        char    szVersion[5];
        sprintf( szVersion, "%04d", poDS->GetVersionCode() );
        strncpy( pachRecord + 1, szVersion, 4 );
    }

    VSIFWrite( pachRecord, nRecLen, 1, fp );
    VSIFWrite( (void *) "\r\n", 2, 1, fp );

    return TRUE;
}

/************************************************************************/
/*                           SetWriteModule()                           */
/*                                                                      */
/*      Setup our access to be to the module indicated in the feature.  */
/************************************************************************/

int TigerFileBase::SetWriteModule( const char *pszExtension, int nRecLen,
                                   OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      Work out what module we should be writing to.                   */
/* -------------------------------------------------------------------- */
    const char *pszTargetModule = poFeature->GetFieldAsString( "MODULE" );
    char        szFullModule[30];

    /* TODO/notdef: eventually more logic based on FILE and STATE/COUNTY can 
       be inserted here. */

    if( pszTargetModule == NULL )
        return FALSE;

    sprintf( szFullModule, "%s.RT", pszTargetModule );

/* -------------------------------------------------------------------- */
/*      Is this our current module?                                     */
/* -------------------------------------------------------------------- */
    if( pszModule != NULL && EQUAL(szFullModule,pszModule) )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Cleanup the previous file, if any.                              */
/* -------------------------------------------------------------------- */
    if( fpPrimary != NULL )
    {
        VSIFClose( fpPrimary );
        fpPrimary = NULL;
    }

    if( pszModule != NULL )
    {
        CPLFree( pszModule );
        pszModule = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is this a module we have never written to before?  If so, we    */
/*      will try to blow away any existing files in this file set.      */
/* -------------------------------------------------------------------- */
    if( !poDS->CheckModule( szFullModule ) )
    {
        poDS->DeleteModuleFiles( szFullModule );
        poDS->AddModule( szFullModule );
    }
    
/* -------------------------------------------------------------------- */
/*      Does this file already exist?                                   */
/* -------------------------------------------------------------------- */
    const char *pszFilename;

    pszFilename = poDS->BuildFilename( szFullModule, pszExtension );

    fpPrimary = VSIFOpen( pszFilename, "ab" );
    if( fpPrimary == NULL )
        return FALSE;

    pszModule = CPLStrdup( szFullModule );

    return TRUE;
}

/************************************************************************/
/*                           AddFieldDefns()                            */
/************************************************************************/
void TigerFileBase::AddFieldDefns(TigerRecordInfo *psRTInfo,
                                  OGRFeatureDefn  *poFeatureDefn)
{
    OGRFieldDefn        oField("",OFTInteger);
    int i, bLFieldHack;

    bLFieldHack = 
        CSLTestBoolean( CPLGetConfigOption( "TIGER_LFIELD_AS_STRING", "NO" ) );
    
    for (i=0; i<psRTInfo->nFieldCount; ++i) {
        if (psRTInfo->pasFields[i].bDefine) {
            OGRFieldType eFT = psRTInfo->pasFields[i].OGRtype;

            if( bLFieldHack 
                && psRTInfo->pasFields[i].cFmt == 'L' 
                && psRTInfo->pasFields[i].cType == 'N' )
                eFT = OFTString;

            oField.Set( psRTInfo->pasFields[i].pszFieldName, eFT, 
                        psRTInfo->pasFields[i].nLen );
            poFeatureDefn->AddFieldDefn( &oField );
        }
    }
}

/************************************************************************/
/*                             SetFields()                              */
/************************************************************************/

void TigerFileBase::SetFields(TigerRecordInfo *psRTInfo,
                              OGRFeature      *poFeature,
                              char            *achRecord)
{
  int i;
  for (i=0; i<psRTInfo->nFieldCount; ++i) {
    if (psRTInfo->pasFields[i].bSet) {
      SetField( poFeature,
                psRTInfo->pasFields[i].pszFieldName,
                achRecord, 
                psRTInfo->pasFields[i].nBeg,
                psRTInfo->pasFields[i].nEnd );
    }
  }
}

/************************************************************************/
/*                             WriteField()                             */
/************************************************************************/
void TigerFileBase::WriteFields(TigerRecordInfo *psRTInfo,
                                OGRFeature      *poFeature,
                                char            *szRecord)
{
  int i;
  for (i=0; i<psRTInfo->nFieldCount; ++i) {
    if (psRTInfo->pasFields[i].bWrite) {
      WriteField( poFeature,
                  psRTInfo->pasFields[i].pszFieldName,
                  szRecord, 
                  psRTInfo->pasFields[i].nBeg,
                  psRTInfo->pasFields[i].nEnd,
                  psRTInfo->pasFields[i].cFmt,
                  psRTInfo->pasFields[i].cType );
    }
  }
}
