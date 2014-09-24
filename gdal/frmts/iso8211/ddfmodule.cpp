/******************************************************************************
 * $Id$
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFModule class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "iso8211.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             DDFModule()                              */
/************************************************************************/

/**
 * The constructor.
 */

DDFModule::DDFModule()

{
    nFieldDefnCount = 0;
    papoFieldDefns = NULL;
    poRecord = NULL;

    papoClones = NULL;
    nCloneCount = nMaxCloneCount = 0;

    fpDDF = NULL;
    bReadOnly = TRUE;

    _interchangeLevel = '\0';
    _inlineCodeExtensionIndicator = '\0';
    _versionNumber = '\0';
    _appIndicator = '\0';
    _fieldControlLength = '\0';
    strcpy( _extendedCharSet, " ! " );

    _recLength = 0;
    _leaderIden = 'L';
    _fieldAreaStart = 0;
    _sizeFieldLength = 0;
    _sizeFieldPos = 0;
    _sizeFieldTag = 0;
}

/************************************************************************/
/*                             ~DDFModule()                             */
/************************************************************************/

/**
 * The destructor.
 */

DDFModule::~DDFModule()

{
    Close();
}

/************************************************************************/
/*                               Close()                                */
/*                                                                      */
/*      Note that closing a file also destroys essentially all other    */
/*      module datastructures.                                          */
/************************************************************************/

/**
 * Close an ISO 8211 file.
 */

void DDFModule::Close()

{
/* -------------------------------------------------------------------- */
/*      Close the file.                                                 */
/* -------------------------------------------------------------------- */
    if( fpDDF != NULL )
    {
        VSIFCloseL( fpDDF );
        fpDDF = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup the working record.                                     */
/* -------------------------------------------------------------------- */
    if( poRecord != NULL )
    {
        delete poRecord;
        poRecord = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup the clones.  Deleting them will cause a callback to     */
/*      remove them from the list.                                      */
/* -------------------------------------------------------------------- */
    while( nCloneCount > 0 )
        delete papoClones[0];

    nMaxCloneCount = 0;
    CPLFree( papoClones );
    papoClones = NULL;
    
/* -------------------------------------------------------------------- */
/*      Cleanup the field definitions.                                  */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; i < nFieldDefnCount; i++ )
        delete papoFieldDefns[i];
    CPLFree( papoFieldDefns );
    papoFieldDefns = NULL;
    nFieldDefnCount = 0;
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open an ISO 8211 file, and read the DDR record to build the     */
/*      field definitions.                                              */
/************************************************************************/

/**
 * Open a ISO 8211 (DDF) file for reading.
 *
 * If the open succeeds the data descriptive record (DDR) will have been
 * read, and all the field and subfield definitions will be available.
 *
 * @param pszFilename   The name of the file to open.
 * @param bFailQuietly If FALSE a CPL Error is issued for non-8211 files, 
 * otherwise quietly return NULL.
 *
 * @return FALSE if the open fails or TRUE if it succeeds.  Errors messages
 * are issued internally with CPLError().
 */

int DDFModule::Open( const char * pszFilename, int bFailQuietly )

{
    static const int nLeaderSize = 24;

/* -------------------------------------------------------------------- */
/*      Close the existing file if there is one.                        */
/* -------------------------------------------------------------------- */
    if( fpDDF != NULL )
        Close();
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fpDDF = VSIFOpenL( pszFilename, "rb" );

    if( fpDDF == NULL )
    {
        if( !bFailQuietly )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open DDF file `%s'.",
                      pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read the 24 byte leader.                                        */
/* -------------------------------------------------------------------- */
    char        achLeader[nLeaderSize];
    
    if( (int)VSIFReadL( achLeader, 1, nLeaderSize, fpDDF ) != nLeaderSize )
    {
        VSIFCloseL( fpDDF );
        fpDDF = NULL;

        if( !bFailQuietly )
            CPLError( CE_Failure, CPLE_FileIO,
                      "Leader is short on DDF file `%s'.",
                      pszFilename );
        
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Verify that this appears to be a valid DDF file.                */
/* -------------------------------------------------------------------- */
    int         i, bValid = TRUE;

    for( i = 0; i < nLeaderSize; i++ )
    {
        if( achLeader[i] < 32 || achLeader[i] > 126 )
            bValid = FALSE;
    }

    if( achLeader[5] != '1' && achLeader[5] != '2' && achLeader[5] != '3' )
        bValid = FALSE;

    if( achLeader[6] != 'L' )
        bValid = FALSE;
    if( achLeader[8] != '1' && achLeader[8] != ' ' )
        bValid = FALSE;

/* -------------------------------------------------------------------- */
/*      Extract information from leader.                                */
/* -------------------------------------------------------------------- */

    if( bValid )
    {
        _recLength                        = DDFScanInt( achLeader+0, 5 );
        _interchangeLevel                 = achLeader[5];
        _leaderIden                   = achLeader[6];
        _inlineCodeExtensionIndicator = achLeader[7];
        _versionNumber                = achLeader[8];
        _appIndicator                 = achLeader[9];
        _fieldControlLength           = DDFScanInt(achLeader+10,2);
        _fieldAreaStart               = DDFScanInt(achLeader+12,5);
        _extendedCharSet[0]           = achLeader[17];
        _extendedCharSet[1]           = achLeader[18];
        _extendedCharSet[2]           = achLeader[19];
        _extendedCharSet[3]           = '\0';
        _sizeFieldLength              = DDFScanInt(achLeader+20,1);
        _sizeFieldPos                 = DDFScanInt(achLeader+21,1);
        _sizeFieldTag                 = DDFScanInt(achLeader+23,1);

        if( _recLength < nLeaderSize || _fieldControlLength == 0
            || _fieldAreaStart < 24 || _sizeFieldLength == 0
            || _sizeFieldPos == 0 || _sizeFieldTag == 0 )
        {
            bValid = FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      If the header is invalid, then clean up, report the error       */
/*      and return.                                                     */
/* -------------------------------------------------------------------- */
    if( !bValid )
    {
        VSIFCloseL( fpDDF );
        fpDDF = NULL;

        if( !bFailQuietly )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "File `%s' does not appear to have\n"
                      "a valid ISO 8211 header.\n",
                      pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read the whole record info memory.                              */
/* -------------------------------------------------------------------- */
    char        *pachRecord;

    pachRecord = (char *) CPLMalloc(_recLength);
    memcpy( pachRecord, achLeader, nLeaderSize );

    if( (int)VSIFReadL( pachRecord+nLeaderSize, 1, _recLength-nLeaderSize, fpDDF )
        != _recLength - nLeaderSize )
    {
        if( !bFailQuietly )
            CPLError( CE_Failure, CPLE_FileIO,
                      "Header record is short on DDF file `%s'.",
                      pszFilename );
        
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      First make a pass counting the directory entries.               */
/* -------------------------------------------------------------------- */
    int         nFieldEntryWidth, nFDCount = 0;

    nFieldEntryWidth = _sizeFieldLength + _sizeFieldPos + _sizeFieldTag;

    for( i = nLeaderSize; i < _recLength; i += nFieldEntryWidth )
    {
        if( pachRecord[i] == DDF_FIELD_TERMINATOR )
            break;

        nFDCount++;
    }

/* -------------------------------------------------------------------- */
/*      Allocate, and read field definitions.                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nFDCount; i++ )
    {
        char    szTag[128];
        int     nEntryOffset = nLeaderSize + i*nFieldEntryWidth;
        int     nFieldLength, nFieldPos;
        DDFFieldDefn *poFDefn;
        
        strncpy( szTag, pachRecord+nEntryOffset, _sizeFieldTag );
        szTag[_sizeFieldTag] = '\0';

        nEntryOffset += _sizeFieldTag;
        nFieldLength = DDFScanInt( pachRecord+nEntryOffset, _sizeFieldLength );
        
        nEntryOffset += _sizeFieldLength;
        nFieldPos = DDFScanInt( pachRecord+nEntryOffset, _sizeFieldPos );

        if (_fieldAreaStart+nFieldPos < 0 ||
            _recLength - (_fieldAreaStart+nFieldPos) < nFieldLength)
        {
            if( !bFailQuietly )
                CPLError( CE_Failure, CPLE_FileIO,
                        "Header record invalid on DDF file `%s'.",
                        pszFilename );

            CPLFree( pachRecord );
            return FALSE;
        }
        
        poFDefn = new DDFFieldDefn();
        if( poFDefn->Initialize( this, szTag, nFieldLength,
                                 pachRecord+_fieldAreaStart+nFieldPos ) )
            AddField( poFDefn );
        else
            delete poFDefn;
    }

    CPLFree( pachRecord );
    
/* -------------------------------------------------------------------- */
/*      Record the current file offset, the beginning of the first      */
/*      data record.                                                    */
/* -------------------------------------------------------------------- */
    nFirstRecordOffset = (long)VSIFTellL( fpDDF );
    
    return TRUE;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int DDFModule::Initialize( char chInterchangeLevel,
                           char chLeaderIden, 
                           char chCodeExtensionIndicator,
                           char chVersionNumber,
                           char chAppIndicator,
                           const char *pszExtendedCharSet,
                           int nSizeFieldLength,
                           int nSizeFieldPos,
                           int nSizeFieldTag )

{
    _interchangeLevel = chInterchangeLevel;
    _leaderIden = chLeaderIden;
    _inlineCodeExtensionIndicator = chCodeExtensionIndicator;
    _versionNumber = chVersionNumber;
    _appIndicator = chAppIndicator;
    strcpy( _extendedCharSet, pszExtendedCharSet );
    _sizeFieldLength = nSizeFieldLength;
    _sizeFieldPos = nSizeFieldPos;
    _sizeFieldTag = nSizeFieldTag;

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int DDFModule::Create( const char *pszFilename )

{
    CPLAssert( fpDDF == NULL );
    
/* -------------------------------------------------------------------- */
/*      Create the file on disk.                                        */
/* -------------------------------------------------------------------- */
    fpDDF = VSIFOpenL( pszFilename, "wb+" );
    if( fpDDF == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create file %s, check path and permissions.",
                  pszFilename );
        return FALSE;
    }
    
    bReadOnly = FALSE;

/* -------------------------------------------------------------------- */
/*      Prepare all the field definition information.                   */
/* -------------------------------------------------------------------- */
    int iField;

    _fieldControlLength = 9;
    _recLength = 24 
        + nFieldDefnCount * (_sizeFieldLength+_sizeFieldPos+_sizeFieldTag) 
        + 1;
    
    _fieldAreaStart = _recLength;

    for( iField=0; iField < nFieldDefnCount; iField++ )
    {
        int nLength;

        papoFieldDefns[iField]->GenerateDDREntry( NULL, &nLength );
        _recLength += nLength;
    }

/* -------------------------------------------------------------------- */
/*      Setup 24 byte leader.                                           */
/* -------------------------------------------------------------------- */
    char achLeader[25];

    sprintf( achLeader+0, "%05d", (int) _recLength );
    achLeader[5] = _interchangeLevel;
    achLeader[6] = _leaderIden;
    achLeader[7] = _inlineCodeExtensionIndicator;
    achLeader[8] = _versionNumber;
    achLeader[9] = _appIndicator;
    sprintf( achLeader+10, "%02d", (int) _fieldControlLength );
    sprintf( achLeader+12, "%05d", (int) _fieldAreaStart );
    strncpy( achLeader+17, _extendedCharSet, 3 );
    sprintf( achLeader+20, "%1d", (int) _sizeFieldLength );
    sprintf( achLeader+21, "%1d", (int) _sizeFieldPos );
    achLeader[22] = '0';
    sprintf( achLeader+23, "%1d", (int) _sizeFieldTag );
    VSIFWriteL( achLeader, 24, 1, fpDDF );

/* -------------------------------------------------------------------- */
/*      Write out directory entries.                                    */
/* -------------------------------------------------------------------- */
    int nOffset = 0;
    for( iField=0; iField < nFieldDefnCount; iField++ )
    {
        char achDirEntry[255];
        char szFormat[32];
        int nLength;

        CPLAssert(_sizeFieldLength + _sizeFieldPos + _sizeFieldTag < (int)sizeof(achDirEntry));

        papoFieldDefns[iField]->GenerateDDREntry( NULL, &nLength );

        CPLAssert( (int)strlen(papoFieldDefns[iField]->GetName()) == _sizeFieldTag );
        strcpy( achDirEntry, papoFieldDefns[iField]->GetName() );
        sprintf(szFormat, "%%0%dd", (int)_sizeFieldLength);
        sprintf( achDirEntry + _sizeFieldTag, szFormat, nLength );
        sprintf(szFormat, "%%0%dd", (int)_sizeFieldTag);
        sprintf( achDirEntry + _sizeFieldTag + _sizeFieldLength, 
                 szFormat, nOffset );
        nOffset += nLength;

        VSIFWriteL( achDirEntry, _sizeFieldLength + _sizeFieldPos + _sizeFieldTag, 1, fpDDF );
    }

    char chUT = DDF_FIELD_TERMINATOR;
    VSIFWriteL( &chUT, 1, 1, fpDDF );

/* -------------------------------------------------------------------- */
/*      Write out the field descriptions themselves.                    */
/* -------------------------------------------------------------------- */
    for( iField=0; iField < nFieldDefnCount; iField++ )
    {
        char *pachData;
        int nLength;

        papoFieldDefns[iField]->GenerateDDREntry( &pachData, &nLength );
        VSIFWriteL( pachData, nLength, 1, fpDDF );
        CPLFree( pachData );
    }
    
    return TRUE;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out module info to debugging file.
 *
 * A variety of information about the module is written to the debugging
 * file.  This includes all the field and subfield definitions read from
 * the header. 
 *
 * @param fp The standard io file handle to write to.  ie. stderr.
 */

void DDFModule::Dump( FILE * fp )

{
    fprintf( fp, "DDFModule:\n" );
    fprintf( fp, "    _recLength = %ld\n", _recLength );
    fprintf( fp, "    _interchangeLevel = %c\n", _interchangeLevel );
    fprintf( fp, "    _leaderIden = %c\n", _leaderIden );
    fprintf( fp, "    _inlineCodeExtensionIndicator = %c\n",
             _inlineCodeExtensionIndicator );
    fprintf( fp, "    _versionNumber = %c\n", _versionNumber );
    fprintf( fp, "    _appIndicator = %c\n", _appIndicator );
    fprintf( fp, "    _extendedCharSet = `%s'\n", _extendedCharSet );
    fprintf( fp, "    _fieldControlLength = %d\n", _fieldControlLength );
    fprintf( fp, "    _fieldAreaStart = %ld\n", _fieldAreaStart );
    fprintf( fp, "    _sizeFieldLength = %ld\n", _sizeFieldLength );
    fprintf( fp, "    _sizeFieldPos = %ld\n", _sizeFieldPos );
    fprintf( fp, "    _sizeFieldTag = %ld\n", _sizeFieldTag );

    for( int i = 0; i < nFieldDefnCount; i++ )
    {
        papoFieldDefns[i]->Dump( fp );
    }
}

/************************************************************************/
/*                           FindFieldDefn()                            */
/************************************************************************/

/**
 * Fetch the definition of the named field.
 *
 * This function will scan the DDFFieldDefn's on this module, to find
 * one with the indicated field name.
 *
 * @param pszFieldName The name of the field to search for.  The comparison is
 *                     case insensitive.
 *
 * @return A pointer to the request DDFFieldDefn object is returned, or NULL
 * if none matching the name are found.  The return object remains owned by
 * the DDFModule, and should not be deleted by application code.
 */

DDFFieldDefn *DDFModule::FindFieldDefn( const char *pszFieldName )

{
    int         i;
    
/* -------------------------------------------------------------------- */
/*      This pass tries to reduce the cost of comparing strings by      */
/*      first checking the first character, and by using strcmp()       */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nFieldDefnCount; i++ )
    {
        const char *pszThisName = papoFieldDefns[i]->GetName();
        
        if( *pszThisName == *pszFieldName
            && strcmp( pszFieldName+1, pszThisName+1) == 0 )
            return papoFieldDefns[i];
    }

/* -------------------------------------------------------------------- */
/*      Now do a more general check.  Application code may not          */
/*      always use the correct name case.                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nFieldDefnCount; i++ )
    {
        if( EQUAL(pszFieldName, papoFieldDefns[i]->GetName()) )
            return papoFieldDefns[i];
    }

    return NULL;
}

/************************************************************************/
/*                             ReadRecord()                             */
/*                                                                      */
/*      Read one record from the file, and return to the                */
/*      application.  The returned record is owned by the module,       */
/*      and is reused from call to call in order to preserve headers    */
/*      when they aren't being re-read from record to record.           */
/************************************************************************/

/**
 * Read one record from the file.
 *
 * @return A pointer to a DDFRecord object is returned, or NULL if a read
 * error, or end of file occurs.  The returned record is owned by the
 * module, and should not be deleted by the application.  The record is
 * only valid untill the next ReadRecord() at which point it is overwritten.
 */

DDFRecord *DDFModule::ReadRecord()

{
    if( poRecord == NULL )
        poRecord = new DDFRecord( this );

    if( poRecord->Read() )
        return poRecord;
    else
        return NULL;
}

/************************************************************************/
/*                              AddField()                              */
/************************************************************************/

/**
 * Add new field definition.
 *
 * Field definitions may only be added to DDFModules being used for 
 * writing, not those being used for reading.  Ownership of the 
 * DDFFieldDefn object is taken by the DDFModule.
 *
 * @param poNewFDefn definition to be added to the module. 
 */

void DDFModule::AddField( DDFFieldDefn *poNewFDefn )

{
    nFieldDefnCount++;
    papoFieldDefns = (DDFFieldDefn **) 
        CPLRealloc(papoFieldDefns, sizeof(void*)*nFieldDefnCount);
    papoFieldDefns[nFieldDefnCount-1] = poNewFDefn;
}

/************************************************************************/
/*                              GetField()                              */
/************************************************************************/

/**
 * Fetch a field definition by index.
 *
 * @param i (from 0 to GetFieldCount() - 1.
 * @return the returned field pointer or NULL if the index is out of range.
 */

DDFFieldDefn *DDFModule::GetField(int i)

{
    if( i < 0 || i >= nFieldDefnCount )
        return NULL;
    else
        return papoFieldDefns[i];
}
    
/************************************************************************/
/*                           AddCloneRecord()                           */
/*                                                                      */
/*      We want to keep track of cloned records, so we can clean        */
/*      them up when the module is destroyed.                           */
/************************************************************************/

void DDFModule::AddCloneRecord( DDFRecord * poRecord )

{
/* -------------------------------------------------------------------- */
/*      Do we need to grow the container array?                         */
/* -------------------------------------------------------------------- */
    if( nCloneCount == nMaxCloneCount )
    {
        nMaxCloneCount = nCloneCount*2 + 20;
        papoClones = (DDFRecord **) CPLRealloc(papoClones,
                                               nMaxCloneCount * sizeof(void*));
    }

/* -------------------------------------------------------------------- */
/*      Add to the list.                                                */
/* -------------------------------------------------------------------- */
    papoClones[nCloneCount++] = poRecord;
}

/************************************************************************/
/*                         RemoveCloneRecord()                          */
/************************************************************************/

void DDFModule::RemoveCloneRecord( DDFRecord * poRecord )

{
    int         i;
 
    for( i = 0; i < nCloneCount; i++ )
    {
        if( papoClones[i] == poRecord )
        {
            papoClones[i] = papoClones[nCloneCount-1];
            nCloneCount--;
            return;
        }
    }

    CPLAssert( FALSE );
}

/************************************************************************/
/*                               Rewind()                               */
/************************************************************************/

/**
 * Return to first record.
 * 
 * The next call to ReadRecord() will read the first data record in the file.
 *
 * @param nOffset the offset in the file to return to.  By default this is
 * -1, a special value indicating that reading should return to the first
 * data record.  Otherwise it is an absolute byte offset in the file.
 */

void DDFModule::Rewind( long nOffset )

{
    if( nOffset == -1 )
        nOffset = nFirstRecordOffset;

    if( fpDDF == NULL )
        return;
    
    VSIFSeekL( fpDDF, nOffset, SEEK_SET );

    if( nOffset == nFirstRecordOffset && poRecord != NULL )
        poRecord->Clear();
        
}
