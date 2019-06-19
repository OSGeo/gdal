/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerBaseFile class, providing common services to all
 *           the tiger file readers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                           TigerFileBase()                            */
/************************************************************************/

TigerFileBase::TigerFileBase( const TigerRecordInfo *psRTInfoIn,
                              const char *m_pszFileCodeIn ) :
    poDS(nullptr),
    pszModule(nullptr),
    pszShortModule(nullptr),
    fpPrimary(nullptr),
    poFeatureDefn(nullptr),
    nFeatures(0),
    nRecordLength(0),
    nVersionCode(0),
    nVersion(TIGER_Unknown),
    psRTInfo(psRTInfoIn),
    m_pszFileCode(m_pszFileCodeIn)
{}

/************************************************************************/
/*                           ~TigerFileBase()                           */
/************************************************************************/

TigerFileBase::~TigerFileBase()

{
    CPLFree( pszModule );
    CPLFree( pszShortModule );

    if( poFeatureDefn != nullptr )
    {
        poFeatureDefn->Release();
        poFeatureDefn = nullptr;
    }

    if( fpPrimary != nullptr )
    {
      VSIFCloseL( fpPrimary );
        fpPrimary = nullptr;
    }
}

/************************************************************************/
/*                              OpenFile()                              */
/************************************************************************/

int TigerFileBase::OpenFile( const char * pszModuleToOpen,
                             const char *pszExtension )

{

    CPLFree( pszModule );
    pszModule = nullptr;
    CPLFree( pszShortModule );
    pszShortModule = nullptr;

    if( fpPrimary != nullptr )
    {
        VSIFCloseL( fpPrimary );
        fpPrimary = nullptr;
    }

    if( pszModuleToOpen == nullptr )
        return TRUE;

    char *pszFilename = poDS->BuildFilename( pszModuleToOpen, pszExtension );

    fpPrimary = VSIFOpenL( pszFilename, "rb" );

    CPLFree( pszFilename );

    if( fpPrimary == nullptr )
        return FALSE;

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

/************************************************************************/
/*                            SetupVersion()                            */
/************************************************************************/

void TigerFileBase::SetupVersion()

{
    char        aszRecordHead[6];

    VSIFSeekL( fpPrimary, 0, SEEK_SET );
    VSIFReadL( aszRecordHead, 1, 5, fpPrimary );
    aszRecordHead[5] = '\0';
    nVersionCode = atoi(aszRecordHead+1);
    VSIFSeekL( fpPrimary, 0, SEEK_SET );

    nVersion = TigerClassifyVersion( nVersionCode );
}

/************************************************************************/
/*                       EstablishRecordLength()                        */
/************************************************************************/

int TigerFileBase::EstablishRecordLength( VSILFILE * fp )

{
    if( fp == nullptr || VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
        return -1;

/* -------------------------------------------------------------------- */
/*      Read through to the end of line.                                */
/* -------------------------------------------------------------------- */
    int nRecLen = 0;
    char chCurrent = '\0';
    while( VSIFReadL( &chCurrent, 1, 1, fp ) == 1
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
    while( VSIFReadL( &chCurrent, 1, 1, fp ) == 1
           && (chCurrent == 10 || chCurrent == 13 ) )
    {
        nRecLen++;
    }

    VSIFSeekL( fp, 0, SEEK_SET );

    return nRecLen;
}

/************************************************************************/
/*                       EstablishFeatureCount()                        */
/************************************************************************/

void TigerFileBase::EstablishFeatureCount()

{
    if( fpPrimary == nullptr )
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

    VSIFSeekL( fpPrimary, 0, SEEK_END );
    const vsi_l_offset nFileSize = VSIFTellL( fpPrimary );

    if( (nFileSize % (vsi_l_offset)nRecordLength) != 0 )
    {
        CPLError( CE_Warning, CPLE_FileIO,
                  "TigerFileBase::EstablishFeatureCount(): "
                  "File length %d doesn't divide by record length %d.\n",
                  (int) nFileSize, (int) nRecordLength );
    }

    if( nFileSize / (vsi_l_offset)nRecordLength > (vsi_l_offset)INT_MAX )
        nFeatures = INT_MAX;
    else
        nFeatures = static_cast<int>(nFileSize / (vsi_l_offset)nRecordLength);
}

/************************************************************************/
/*                              GetField()                              */
/************************************************************************/

const char* TigerFileBase::GetField( const char * pachRawDataRecord,
                                     int nStartChar, int nEndChar )

{
    char         aszField[128];
    int                 nLength = nEndChar - nStartChar + 1;

    CPLAssert( nEndChar - nStartChar + 2 < (int) sizeof(aszField) );

    strncpy( aszField, pachRawDataRecord + nStartChar - 1, nLength );

    aszField[nLength] = '\0';
    while( nLength > 0 && aszField[nLength-1] == ' ' )
        aszField[--nLength] = '\0';

    return CPLSPrintf("%s", aszField);
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

bool TigerFileBase::WriteField( OGRFeature *poFeature, const char *pszField,
                                char *pachRecord, int nStart, int nEnd,
                                char chFormat, char chType )

{
    const int iField = poFeature->GetFieldIndex( pszField );
    char szValue[512];

    CPLAssert( nEnd - nStart + 1 < (int) sizeof(szValue)-1 );

    if( iField < 0 || !poFeature->IsFieldSetAndNotNull( iField ) )
        return false;

    char szFormat[32];
    if( chType == 'N' && chFormat == 'L' )
    {
        snprintf( szFormat, sizeof(szFormat), "%%0%dd", nEnd - nStart + 1 );
        snprintf( szValue, sizeof(szValue), szFormat, poFeature->GetFieldAsInteger( iField ) );
    }
    else if( chType == 'N' && chFormat == 'R' )
    {
        snprintf( szFormat, sizeof(szFormat), "%%%dd", nEnd - nStart + 1 );
        snprintf( szValue, sizeof(szValue), szFormat, poFeature->GetFieldAsInteger( iField ) );
    }
    else if( chType == 'A' && chFormat == 'L' )
    {
        strncpy( szValue, poFeature->GetFieldAsString( iField ),
                 sizeof(szValue) - 1 );
        szValue[sizeof(szValue) - 1] = 0;
        if( (int) strlen(szValue) < nEnd - nStart + 1 )
            memset( szValue + strlen(szValue), ' ',
                    nEnd - nStart + 1 - strlen(szValue) );
    }
    else if( chType == 'A' && chFormat == 'R' )
    {
        snprintf( szFormat, sizeof(szFormat), "%%%ds", nEnd - nStart + 1 );
        snprintf( szValue, sizeof(szValue), szFormat, poFeature->GetFieldAsString( iField ) );
    }
    else
    {
        CPLAssert( false );
        return false;
    }

    memcpy( pachRecord + nStart - 1, szValue, nEnd - nStart + 1 );

    return true;
}

/************************************************************************/
/*                             WritePoint()                             */
/************************************************************************/

bool TigerFileBase::WritePoint( char *pachRecord, int nStart,
                                double dfX, double dfY )

{
    if( dfX == 0.0 && dfY == 0.0 )
    {
        memcpy( pachRecord + nStart - 1, "+000000000+00000000", 19 );
    }
    else
    {
        char szTemp[20] = { 0 };
        snprintf( szTemp, sizeof(szTemp), "%+10d%+9d",
                 (int) floor(dfX * 1000000 + 0.5),
                 (int) floor(dfY * 1000000 + 0.5) );
        memcpy( pachRecord + nStart - 1, szTemp, 19 );
    }

    return true;
}

/************************************************************************/
/*                            WriteRecord()                             */
/************************************************************************/

bool TigerFileBase::WriteRecord( char *pachRecord, int nRecLen,
                                 const char *pszType, VSILFILE * fp )

{
    if( fp == nullptr )
        fp = fpPrimary;

    pachRecord[0] = *pszType;

    /*
     * Prior to TIGER_2002, type 5 files lacked the version.  So write
     * the version in the record if we're using TIGER_2002 or higher,
     * or if this is not type "5"
     */
    if ( (poDS->GetVersion() >= TIGER_2002) ||
         (!EQUAL(pszType, "5")) )
    {
        char szVersion[5] = { 0 };
        snprintf( szVersion, sizeof(szVersion), "%04d", poDS->GetVersionCode() );
        memcpy( pachRecord + 1, szVersion, 4 );
    }

    VSIFWriteL( pachRecord, nRecLen, 1, fp );
    VSIFWriteL( (void *) "\r\n", 2, 1, fp );

    return true;
}

/************************************************************************/
/*                           SetWriteModule()                           */
/*                                                                      */
/*      Setup our access to be to the module indicated in the feature.  */
/************************************************************************/

bool TigerFileBase::SetWriteModule( const char *pszExtension,
                                    CPL_UNUSED int nRecLen,
                                    OGRFeature *poFeature )
{
/* -------------------------------------------------------------------- */
/*      Work out what module we should be writing to.                   */
/* -------------------------------------------------------------------- */
    const char *pszTargetModule = poFeature->GetFieldAsString( "MODULE" );

    /* TODO/notdef: eventually more logic based on FILE and STATE/COUNTY can
       be inserted here. */

    if( pszTargetModule == nullptr )
        return false;

    char szFullModule[30];
    snprintf( szFullModule, sizeof(szFullModule), "%s.RT", pszTargetModule );

/* -------------------------------------------------------------------- */
/*      Is this our current module?                                     */
/* -------------------------------------------------------------------- */
    if( pszModule != nullptr && EQUAL(szFullModule,pszModule) )
        return true;

/* -------------------------------------------------------------------- */
/*      Cleanup the previous file, if any.                              */
/* -------------------------------------------------------------------- */
    if( fpPrimary != nullptr )
    {
        VSIFCloseL( fpPrimary );
        fpPrimary = nullptr;
    }

    if( pszModule != nullptr )
    {
        CPLFree( pszModule );
        pszModule = nullptr;
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
    char *pszFilename = poDS->BuildFilename( szFullModule, pszExtension );

    fpPrimary = VSIFOpenL( pszFilename, "ab" );
    CPLFree(pszFilename);
    if( fpPrimary == nullptr )
        return false;

    pszModule = CPLStrdup( szFullModule );

    return true;
}

/************************************************************************/
/*                           AddFieldDefns()                            */
/************************************************************************/
void TigerFileBase::AddFieldDefns(const TigerRecordInfo *psRTInfoIn,
                                  OGRFeatureDefn  *poFeatureDefnIn)
{
    OGRFieldDefn        oField("",OFTInteger);
    int i, bLFieldHack;

    bLFieldHack =
        CPLTestBool( CPLGetConfigOption( "TIGER_LFIELD_AS_STRING", "NO" ) );

    for (i=0; i<psRTInfoIn->nFieldCount; ++i) {
        if (psRTInfoIn->pasFields[i].bDefine) {
            OGRFieldType eFT = (OGRFieldType)psRTInfoIn->pasFields[i].OGRtype;

            if( bLFieldHack
                && psRTInfoIn->pasFields[i].cFmt == 'L'
                && psRTInfoIn->pasFields[i].cType == 'N' )
                eFT = OFTString;

            oField.Set( psRTInfoIn->pasFields[i].pszFieldName, eFT,
                        psRTInfoIn->pasFields[i].nLen );
            poFeatureDefnIn->AddFieldDefn( &oField );
        }
    }
}

/************************************************************************/
/*                             SetFields()                              */
/************************************************************************/

void TigerFileBase::SetFields(const TigerRecordInfo *psRTInfoIn,
                              OGRFeature      *poFeature,
                              char            *achRecord)
{
  for( int i = 0; i < psRTInfoIn->nFieldCount; ++i )
  {
    if (psRTInfoIn->pasFields[i].bSet) {
      SetField( poFeature,
                psRTInfoIn->pasFields[i].pszFieldName,
                achRecord,
                psRTInfoIn->pasFields[i].nBeg,
                psRTInfoIn->pasFields[i].nEnd );
    }
  }
}

/************************************************************************/
/*                             WriteField()                             */
/************************************************************************/
void TigerFileBase::WriteFields(const TigerRecordInfo *psRTInfoIn,
                                OGRFeature      *poFeature,
                                char            *szRecord)
{
  for( int i = 0; i < psRTInfoIn->nFieldCount; ++i )
  {
    if (psRTInfoIn->pasFields[i].bWrite) {
      WriteField( poFeature,
                  psRTInfoIn->pasFields[i].pszFieldName,
                  szRecord,
                  psRTInfoIn->pasFields[i].nBeg,
                  psRTInfoIn->pasFields[i].nEnd,
                  psRTInfoIn->pasFields[i].cFmt,
                  psRTInfoIn->pasFields[i].cType );
    }
  }
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

bool TigerFileBase::SetModule( const char * pszModuleIn )

{
    if( m_pszFileCode == nullptr )
        return false;

    if( !OpenFile( pszModuleIn, m_pszFileCode ) )
        return false;

    EstablishFeatureCount();

    return true;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerFileBase::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if (psRTInfo == nullptr)
        return nullptr;

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s",
                  nRecordId, pszModule );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Read the raw record data from the file.                         */
/* -------------------------------------------------------------------- */
    if( fpPrimary == nullptr )
        return nullptr;

    if( VSIFSeekL( fpPrimary, nRecordId * nRecordLength, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to %d of %s",
                  nRecordId * nRecordLength, pszModule );
        return nullptr;
    }

    // Overflow cannot happen since psRTInfo->nRecordLength is unsigned
    // char and sizeof(achRecord) == OGR_TIGER_RECBUF_LEN > 255
    if( VSIFReadL( achRecord, psRTInfo->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s",
                  nRecordId, pszModule );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRTInfo, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerFileBase::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];

    if (psRTInfo == nullptr)
        return OGRERR_FAILURE;

    if( !SetWriteModule( m_pszFileCode, psRTInfo->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRTInfo->nRecordLength );

    WriteFields( psRTInfo, poFeature, szRecord );

    WriteRecord( szRecord, psRTInfo->nRecordLength, m_pszFileCode );

    return OGRERR_NONE;
}
