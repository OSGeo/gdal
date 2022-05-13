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
