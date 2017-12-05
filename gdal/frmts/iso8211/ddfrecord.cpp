/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFRecord class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "iso8211.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

static const int nLeaderSize = 24;

/************************************************************************/
/*                             DDFRecord()                              */
/************************************************************************/

DDFRecord::DDFRecord( DDFModule * poModuleIn ) :
    poModule(poModuleIn),
    nReuseHeader(FALSE),
    nFieldOffset(0),
    _sizeFieldTag(poModuleIn->GetSizeFieldTag()),
    _sizeFieldPos(5),
    _sizeFieldLength(5),
    nDataSize(0),
    pachData(NULL),
    nFieldCount(0),
    paoFields(NULL),
    bIsClone(FALSE)
{}

/************************************************************************/
/*                             ~DDFRecord()                             */
/************************************************************************/

DDFRecord::~DDFRecord()

{
    Clear();

    if( bIsClone )
        poModule->RemoveCloneRecord( this );
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out record contents to debugging file.
 *
 * A variety of information about this record, and all its fields and
 * subfields is written to the given debugging file handle.  Note that
 * field definition information (ala DDFFieldDefn) isn't written.
 *
 * @param fp The standard IO file handle to write to.  i.e. stderr
 */

void DDFRecord::Dump( FILE * fp )

{
    fprintf( fp, "DDFRecord:\n" );
    fprintf( fp, "    nReuseHeader = %d\n", nReuseHeader );
    fprintf( fp, "    nDataSize = %d\n", nDataSize );
    fprintf( fp,
             "    _sizeFieldLength=%d, _sizeFieldPos=%d, _sizeFieldTag=%d\n",
             _sizeFieldLength, _sizeFieldPos, _sizeFieldTag );

    for( int i = 0; i < nFieldCount; i++ )
    {
        paoFields[i].Dump( fp );
    }
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record of data from the file, and parse the header to    */
/*      build a field list for the record (or reuse the existing one    */
/*      if reusing headers).  It is expected that the file pointer      */
/*      will be positioned at the beginning of a data record.  It is    */
/*      the DDFModule's responsibility to do so.                        */
/*                                                                      */
/*      This method should only be called by the DDFModule class.       */
/************************************************************************/

int DDFRecord::Read()

{
/* -------------------------------------------------------------------- */
/*      Redefine the record on the basis of the header if needed.       */
/*      As a side effect this will read the data for the record as well.*/
/* -------------------------------------------------------------------- */
    if( !nReuseHeader )
    {
        return ReadHeader();
    }
    if( nFieldOffset < 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Otherwise we read just the data and carefully overlay it on     */
/*      the previous records data without disturbing the rest of the    */
/*      record.                                                         */
/* -------------------------------------------------------------------- */
    size_t      nReadBytes;

    CPLAssert( nFieldOffset <= nDataSize );
    nReadBytes = VSIFReadL( pachData + nFieldOffset, 1,
                            nDataSize - nFieldOffset,
                            poModule->GetFP() );
    if( nReadBytes != (size_t) (nDataSize - nFieldOffset)
        && nReadBytes == 0
        && VSIFEofL( poModule->GetFP() ) )
    {
        return FALSE;
    }
    else if( nReadBytes != (size_t) (nDataSize - nFieldOffset) )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Data record is short on DDF file.\n" );

        return FALSE;
    }

    // notdef: eventually we may have to do something at this point to
    // notify the DDFField's that their data values have changed.

    return TRUE;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/**
 * Write record out to module.
 *
 * This method writes the current record to the module to which it is
 * attached.  Normally this would be at the end of the file, and only used
 * for modules newly created with DDFModule::Create().  Rewriting existing
 * records is not supported at this time.  Calling Write() multiple times
 * on a DDFRecord will result it multiple copies being written at the end of
 * the module.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::Write()

{
    if( !ResetDirectory() )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Prepare leader.                                                 */
/* -------------------------------------------------------------------- */
    char szLeader[nLeaderSize+1];

    memset( szLeader, ' ', nLeaderSize );

    snprintf( szLeader+0, sizeof(szLeader)-0, "%05d", (int) (nDataSize + nLeaderSize) );
    szLeader[5] = ' ';
    szLeader[6] = 'D';

    snprintf( szLeader+12, sizeof(szLeader)-12, "%05d", (int) (nFieldOffset + nLeaderSize) );
    szLeader[17] = ' ';

    szLeader[20] = (char) ('0' + _sizeFieldLength);
    szLeader[21] = (char) ('0' + _sizeFieldPos);
    szLeader[22] = '0';
    szLeader[23] = (char) ('0' + _sizeFieldTag);

    /* notdef: lots of stuff missing */

/* -------------------------------------------------------------------- */
/*      Write the leader.                                               */
/* -------------------------------------------------------------------- */
    int bRet = VSIFWriteL( szLeader, nLeaderSize, 1, poModule->GetFP() ) > 0;

/* -------------------------------------------------------------------- */
/*      Write the remainder of the record.                              */
/* -------------------------------------------------------------------- */
    bRet &= VSIFWriteL( pachData, nDataSize, 1, poModule->GetFP() ) > 0;

    return bRet ? TRUE : FALSE;
}

/************************************************************************/
/*                               Clear()                                */
/*                                                                      */
/*      Clear any information associated with the last header in        */
/*      preparation for reading a new header.                           */
/************************************************************************/

void DDFRecord::Clear()

{
    if( paoFields != NULL )
        delete[] paoFields;

    paoFields = NULL;
    nFieldCount = 0;

    if( pachData != NULL )
        CPLFree( pachData );

    pachData = NULL;
    nDataSize = 0;
    nReuseHeader = FALSE;
}

/************************************************************************/
/*                             ReadHeader()                             */
/*                                                                      */
/*      This perform the header reading and parsing job for the         */
/*      Read() method.  It reads the header, and builds a field         */
/*      list.                                                           */
/************************************************************************/

int DDFRecord::ReadHeader()

{
/* -------------------------------------------------------------------- */
/*      Clear any existing information.                                 */
/* -------------------------------------------------------------------- */
    Clear();

/* -------------------------------------------------------------------- */
/*      Read the 24 byte leader.                                        */
/* -------------------------------------------------------------------- */
    char        achLeader[nLeaderSize];
    int         nReadBytes;

    nReadBytes = static_cast<int>(VSIFReadL(achLeader,1,nLeaderSize,poModule->GetFP()));
    if( nReadBytes == 0 && VSIFEofL( poModule->GetFP() ) )
    {
        nFieldOffset = -1;
        return FALSE;
    }
    else if( nReadBytes != (int) nLeaderSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Leader is short on DDF file." );
        nFieldOffset = -1;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract information from leader.                                */
/* -------------------------------------------------------------------- */
    int         _recLength, _fieldAreaStart;
    char        _leaderIden;

    _recLength                    = DDFScanInt( achLeader+0, 5 );
    _leaderIden                   = achLeader[6];
    _fieldAreaStart               = DDFScanInt(achLeader+12,5);

    _sizeFieldLength = achLeader[20] - '0';
    _sizeFieldPos = achLeader[21] - '0';
    _sizeFieldTag = achLeader[23] - '0';

    if( _sizeFieldLength <= 0 || _sizeFieldLength > 9
        || _sizeFieldPos <= 0 || _sizeFieldPos > 9
        || _sizeFieldTag <= 0 || _sizeFieldTag > 9 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "ISO8211 record leader appears to be corrupt." );
        nFieldOffset = -1;
        return FALSE;
    }

    if( _leaderIden == 'R' )
        nReuseHeader = TRUE;

    nFieldOffset = _fieldAreaStart - nLeaderSize;

/* -------------------------------------------------------------------- */
/*      Is there anything seemly screwy about this record?              */
/* -------------------------------------------------------------------- */
    if( ((_recLength <= 24 || _recLength > 100000000) && (_recLength != 0))
         || _fieldAreaStart < 24 || _fieldAreaStart > 100000 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Data record appears to be corrupt on DDF file.\n"
                  " -- ensure that the files were uncompressed without modifying\n"
                  "carriage return/linefeeds (by default WINZIP does this)." );
        nFieldOffset = -1;
        return FALSE;
    }

/* ==================================================================== */
/*      Handle the normal case with the record length available.        */
/* ==================================================================== */
    if(_recLength != 0) {
/* -------------------------------------------------------------------- */
/*      Read the remainder of the record.                               */
/* -------------------------------------------------------------------- */
        nDataSize = _recLength - nLeaderSize;
        pachData = (char *) CPLMalloc(nDataSize+1);
        pachData[nDataSize] = '\0';

        if( VSIFReadL( pachData, 1, nDataSize, poModule->GetFP()) !=
            (size_t) nDataSize )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Data record is short on DDF file." );
            nFieldOffset = -1;
            return FALSE;
        }

/* -------------------------------------------------------------------- */
/*      If we don't find a field terminator at the end of the record    */
/*      we will read extra bytes till we get to it.                     */
/* -------------------------------------------------------------------- */
        while( pachData[nDataSize-1] != DDF_FIELD_TERMINATOR
               && (nDataSize < 2 || pachData[nDataSize-2] != DDF_FIELD_TERMINATOR) )
        {
            nDataSize++;
            pachData = (char *) CPLRealloc(pachData,nDataSize+1);
            pachData[nDataSize] = '\0';

            if( VSIFReadL( pachData + nDataSize - 1, 1, 1, poModule->GetFP() )
                != 1 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Data record is short on DDF file." );
                nFieldOffset = -1;
                return FALSE;
            }
            CPLDebug( "ISO8211",
                      "Didn't find field terminator, read one more byte." );
        }

        if( nFieldOffset >= nDataSize )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed, "nFieldOffset < nDataSize");
            nFieldOffset = -1;
            return FALSE;
        }

/* -------------------------------------------------------------------- */
/*      Loop over the directory entries, making a pass counting them.   */
/* -------------------------------------------------------------------- */
        int         i;
        int         nFieldEntryWidth;

        nFieldEntryWidth = _sizeFieldLength + _sizeFieldPos + _sizeFieldTag;
        if( nFieldEntryWidth <= 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Invalid entry width = %d", nFieldEntryWidth);
            nFieldOffset = -1;
            return FALSE;
        }

        nFieldCount = 0;
        for( i = 0; i + nFieldEntryWidth <= nDataSize; i += nFieldEntryWidth )
        {
            if( pachData[i] == DDF_FIELD_TERMINATOR )
                break;

            nFieldCount++;
        }

/* -------------------------------------------------------------------- */
/*      Allocate, and read field definitions.                           */
/* -------------------------------------------------------------------- */
        paoFields = new DDFField[nFieldCount];

        for( i = 0; i < nFieldCount; i++ )
        {
            char    szTag[128];
            int     nEntryOffset = i*nFieldEntryWidth;
            int     nFieldLength, nFieldPos;

/* -------------------------------------------------------------------- */
/*      Read the position information and tag.                          */
/* -------------------------------------------------------------------- */
            strncpy( szTag, pachData+nEntryOffset, _sizeFieldTag );
            szTag[_sizeFieldTag] = '\0';

            nEntryOffset += _sizeFieldTag;
            nFieldLength = DDFScanInt( pachData+nEntryOffset, _sizeFieldLength );

            nEntryOffset += _sizeFieldLength;
            nFieldPos = DDFScanInt( pachData+nEntryOffset, _sizeFieldPos );

/* -------------------------------------------------------------------- */
/*      Find the corresponding field in the module directory.           */
/* -------------------------------------------------------------------- */
            DDFFieldDefn    *poFieldDefn = poModule->FindFieldDefn( szTag );

            if( poFieldDefn == NULL || nFieldLength < 0 || nFieldPos < 0  )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Undefined field `%s' encountered in data record.",
                          szTag );
                return FALSE;
            }

            if (_fieldAreaStart + nFieldPos - nLeaderSize < 0 ||
                nDataSize - (_fieldAreaStart + nFieldPos - nLeaderSize) < nFieldLength)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Not enough byte to initialize field `%s'.",
                          szTag );
                nFieldOffset = -1;
                return FALSE;
            }

/* -------------------------------------------------------------------- */
/*      Assign info the DDFField.                                       */
/* -------------------------------------------------------------------- */
            paoFields[i].Initialize( poFieldDefn,
                                     pachData + _fieldAreaStart + nFieldPos - nLeaderSize,
                                     nFieldLength );
        }

        return TRUE;
    }
/* ==================================================================== */
/*      Handle the exceptional case where the record length is          */
/*      zero.  In this case we have to read all the data based on       */
/*      the size of data items as per ISO8211 spec Annex C, 1.5.1.      */
/*                                                                      */
/*      See Bugzilla bug 181 and test with file US4CN21M.000.           */
/* ==================================================================== */
    else {
        CPLDebug( "ISO8211",
                  "Record with zero length, use variant (C.1.5.1) logic." );

        /* ----------------------------------------------------------------- */
        /*   _recLength == 0, handle the large record.                       */
        /*                                                                   */
        /*   Read the remainder of the record.                               */
        /* ----------------------------------------------------------------- */
        nDataSize = 0;
        pachData = NULL;

        /* ----------------------------------------------------------------- */
        /*   Loop over the directory entries, making a pass counting them.   */
        /* ----------------------------------------------------------------- */
        int nFieldEntryWidth = _sizeFieldLength + _sizeFieldPos + _sizeFieldTag;
        nFieldCount = 0;
        int i=0;

        if (nFieldEntryWidth == 0)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Invalid record buffer size : %d.",
                      nFieldEntryWidth );
            nFieldOffset = -1;
            return FALSE;
        }

        char *tmpBuf = (char*)VSI_MALLOC_VERBOSE(nFieldEntryWidth);

        if( tmpBuf == NULL )
        {
            nFieldOffset = -1;
            return FALSE;
        }

        // while we're not at the end, store this entry,
        // and keep on reading...
        do {
            // read an Entry:
            if(nFieldEntryWidth !=
               (int) VSIFReadL(tmpBuf, 1, nFieldEntryWidth, poModule->GetFP())) {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Data record is short on DDF file.");
                CPLFree(tmpBuf);
                nFieldOffset = -1;
                return FALSE;
            }

            // move this temp buffer into more permanent storage:
            char *newBuf = (char*)CPLMalloc(nDataSize+nFieldEntryWidth+1);
            newBuf[nDataSize+nFieldEntryWidth] = '\0';
            if(pachData!=NULL) {
                memcpy(newBuf, pachData, nDataSize);
                CPLFree(pachData);
            }
            memcpy(&newBuf[nDataSize], tmpBuf, nFieldEntryWidth);
            pachData = newBuf;
            nDataSize += nFieldEntryWidth;

            if(DDF_FIELD_TERMINATOR != tmpBuf[0]) {
                nFieldCount++;
            }
        }
        while(DDF_FIELD_TERMINATOR != tmpBuf[0]);

        CPLFree(tmpBuf);
        tmpBuf = NULL;

        // --------------------------------------------------------------------
        // Now, rewind a little.  Only the TERMINATOR should have been read
        // --------------------------------------------------------------------
        int rewindSize = nFieldEntryWidth - 1;
        VSILFILE *fp = poModule->GetFP();
        vsi_l_offset pos = VSIFTellL(fp) - rewindSize;
        if( VSIFSeekL(fp, pos, SEEK_SET) < 0 )
            return FALSE;
        nDataSize -= rewindSize;

        // --------------------------------------------------------------------
        // Okay, now let's populate the heck out of pachData...
        // --------------------------------------------------------------------
        for(i=0; i<nFieldCount; i++) {
            int nEntryOffset = (i*nFieldEntryWidth) + _sizeFieldTag;
            int nFieldLength = DDFScanInt(pachData + nEntryOffset,
                                          _sizeFieldLength);
            tmpBuf = NULL;
            if( nFieldLength >= 0 )
                tmpBuf = (char*)VSI_MALLOC_VERBOSE(nFieldLength);
            if( tmpBuf == NULL )
            {
                nFieldOffset = -1;
                return FALSE;
            }

            // read an Entry:
            if(nFieldLength !=
               (int) VSIFReadL(tmpBuf, 1, nFieldLength, poModule->GetFP())) {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Data record is short on DDF file.");
                CPLFree(tmpBuf);
                nFieldOffset = -1;
                return FALSE;
            }

            // move this temp buffer into more permanent storage:
            char *newBuf = (char*)VSI_MALLOC_VERBOSE(nDataSize+nFieldLength+1);
            if( newBuf == NULL )
            {
                CPLFree(tmpBuf);
                nFieldOffset = -1;
                return FALSE;
            }
            newBuf[nDataSize+nFieldLength] = '\0';
            memcpy(newBuf, pachData, nDataSize);
            CPLFree(pachData);
            memcpy(&newBuf[nDataSize], tmpBuf, nFieldLength);
            CPLFree(tmpBuf);
            pachData = newBuf;
            nDataSize += nFieldLength;
        }

        if( nFieldOffset >= nDataSize )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed, "nFieldOffset < nDataSize");
            nFieldOffset = -1;
            return FALSE;
        }

        /* ----------------------------------------------------------------- */
        /*     Allocate, and read field definitions.                         */
        /* ----------------------------------------------------------------- */
        paoFields = new DDFField[nFieldCount];

        for( i = 0; i < nFieldCount; i++ )
        {
            char    szTag[128];
            int     nEntryOffset = i*nFieldEntryWidth;
            int     nFieldLength, nFieldPos;

            /* ------------------------------------------------------------- */
            /* Read the position information and tag.                        */
            /* ------------------------------------------------------------- */
            strncpy( szTag, pachData+nEntryOffset, _sizeFieldTag );
            szTag[_sizeFieldTag] = '\0';

            nEntryOffset += _sizeFieldTag;
            nFieldLength = DDFScanInt( pachData+nEntryOffset, _sizeFieldLength );

            nEntryOffset += _sizeFieldLength;
            nFieldPos = DDFScanInt( pachData+nEntryOffset, _sizeFieldPos );

            /* ------------------------------------------------------------- */
            /* Find the corresponding field in the module directory.         */
            /* ------------------------------------------------------------- */
            DDFFieldDefn    *poFieldDefn = poModule->FindFieldDefn( szTag );

            if( poFieldDefn == NULL || nFieldLength < 0 || nFieldPos < 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Undefined field `%s' encountered in data record.",
                          szTag );
                nFieldOffset = -1;
                return FALSE;
            }

            if (_fieldAreaStart + nFieldPos - nLeaderSize < 0 ||
                nDataSize - (_fieldAreaStart + nFieldPos - nLeaderSize) < nFieldLength)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Not enough byte to initialize field `%s'.",
                          szTag );
                nFieldOffset = -1;
                return FALSE;
            }

            /* ------------------------------------------------------------- */
            /* Assign info the DDFField.                                     */
            /* ------------------------------------------------------------- */

            paoFields[i].Initialize( poFieldDefn,
                                     pachData + _fieldAreaStart
                                     + nFieldPos - nLeaderSize,
                                     nFieldLength );
        }

        return TRUE;
    }
}

/************************************************************************/
/*                             FindField()                              */
/************************************************************************/

/**
 * Find the named field within this record.
 *
 * @param pszName The name of the field to fetch.  The comparison is
 * case insensitive.
 * @param iFieldIndex The instance of this field to fetch.  Use zero (the
 * default) for the first instance.
 *
 * @return Pointer to the requested DDFField.  This pointer is to an
 * internal object, and should not be freed.  It remains valid until
 * the next record read.
 */

DDFField * DDFRecord::FindField( const char * pszName, int iFieldIndex )

{
    for( int i = 0; i < nFieldCount; i++ )
    {
        DDFFieldDefn* poFieldDefn = paoFields[i].GetFieldDefn();
        if( poFieldDefn && EQUAL(poFieldDefn->GetName(),pszName) )
        {
            if( iFieldIndex == 0 )
                return paoFields + i;
            else
                iFieldIndex--;
        }
    }

    return NULL;
}

/************************************************************************/
/*                              GetField()                              */
/************************************************************************/

/**
 * Fetch field object based on index.
 *
 * @param i The index of the field to fetch.  Between 0 and GetFieldCount()-1.
 *
 * @return A DDFField pointer, or NULL if the index is out of range.
 */

DDFField *DDFRecord::GetField( int i )

{
    if( i < 0 || i >= nFieldCount )
        return NULL;
    else
        return paoFields + i;
}

/************************************************************************/
/*                           GetIntSubfield()                           */
/************************************************************************/

/**
 * Fetch value of a subfield as an integer.  This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param pszField The name of the field containing the subfield.
 * @param iFieldIndex The instance of this field within the record.  Use
 * zero for the first instance of this field.
 * @param pszSubfield The name of the subfield within the selected field.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails.  Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or zero if it failed for some reason.
 */

int DDFRecord::GetIntSubfield( const char * pszField, int iFieldIndex,
                               const char * pszSubfield, int iSubfieldIndex,
                               int * pnSuccess )

{
    int nDummyErr = FALSE;

    if( pnSuccess == NULL )
        pnSuccess = &nDummyErr;

    *pnSuccess = FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch the field. If this fails, return zero.                    */
/* -------------------------------------------------------------------- */
    DDFField *poField = FindField( pszField, iFieldIndex );
    if( poField == NULL )
        return 0;

/* -------------------------------------------------------------------- */
/*      Get the subfield definition                                     */
/* -------------------------------------------------------------------- */
    DDFSubfieldDefn *poSFDefn =
        poField->GetFieldDefn()->FindSubfieldDefn( pszSubfield );
    if( poSFDefn == NULL )
        return 0;

/* -------------------------------------------------------------------- */
/*      Get a pointer to the data.                                      */
/* -------------------------------------------------------------------- */
    int         nBytesRemaining;

    const char *l_pachData = poField->GetSubfieldData(poSFDefn,
                                                    &nBytesRemaining,
                                                    iSubfieldIndex);
    if( l_pachData == NULL )
        return 0;

/* -------------------------------------------------------------------- */
/*      Return the extracted value.                                     */
/*                                                                      */
/*      Assume an error has occurred if no bytes are consumed.           */
/* -------------------------------------------------------------------- */
    int nConsumedBytes = 0;
    int nResult = poSFDefn->ExtractIntData( l_pachData, nBytesRemaining,
                                            &nConsumedBytes );

    if( nConsumedBytes > 0 )
        *pnSuccess = TRUE;

    return nResult;
}

/************************************************************************/
/*                          GetFloatSubfield()                          */
/************************************************************************/

/**
 * Fetch value of a subfield as a float (double).  This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param pszField The name of the field containing the subfield.
 * @param iFieldIndex The instance of this field within the record.  Use
 * zero for the first instance of this field.
 * @param pszSubfield The name of the subfield within the selected field.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails.  Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or zero if it failed for some reason.
 */

double DDFRecord::GetFloatSubfield( const char * pszField, int iFieldIndex,
                                    const char * pszSubfield,
                                    int iSubfieldIndex,
                                    int * pnSuccess )

{
    int nDummyErr = FALSE;

    if( pnSuccess == NULL )
        pnSuccess = &nDummyErr;

    *pnSuccess = FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch the field. If this fails, return zero.                    */
/* -------------------------------------------------------------------- */
    DDFField *poField = FindField( pszField, iFieldIndex );
    if( poField == NULL )
        return 0;

/* -------------------------------------------------------------------- */
/*      Get the subfield definition                                     */
/* -------------------------------------------------------------------- */
    DDFSubfieldDefn *poSFDefn =
        poField->GetFieldDefn()->FindSubfieldDefn( pszSubfield );
    if( poSFDefn == NULL )
        return 0;

/* -------------------------------------------------------------------- */
/*      Get a pointer to the data.                                      */
/* -------------------------------------------------------------------- */
    int         nBytesRemaining;

    const char *l_pachData = poField->GetSubfieldData(poSFDefn,
                                                    &nBytesRemaining,
                                                    iSubfieldIndex);
    if( l_pachData == NULL )
        return 0;

/* -------------------------------------------------------------------- */
/*      Return the extracted value.                                     */
/* -------------------------------------------------------------------- */
    int nConsumedBytes = 0;
    double dfResult = poSFDefn->ExtractFloatData( l_pachData, nBytesRemaining,
                                                  &nConsumedBytes );

    if( nConsumedBytes > 0 )
        *pnSuccess = TRUE;

    return dfResult;
}

/************************************************************************/
/*                         GetStringSubfield()                          */
/************************************************************************/

/**
 * Fetch value of a subfield as a string.  This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param pszField The name of the field containing the subfield.
 * @param iFieldIndex The instance of this field within the record.  Use
 * zero for the first instance of this field.
 * @param pszSubfield The name of the subfield within the selected field.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails.  Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or NULL if it failed for some reason.
 * The returned pointer is to internal data and should not be modified or
 * freed by the application.
 */

const char *
DDFRecord::GetStringSubfield( const char * pszField, int iFieldIndex,
                              const char * pszSubfield, int iSubfieldIndex,
                              int * pnSuccess )

{
    int nDummyErr = FALSE;

    if( pnSuccess == NULL )
        pnSuccess = &nDummyErr;

    *pnSuccess = FALSE;

/* -------------------------------------------------------------------- */
/*      Fetch the field. If this fails, return zero.                    */
/* -------------------------------------------------------------------- */
    DDFField *poField = FindField( pszField, iFieldIndex );
    if( poField == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get the subfield definition                                     */
/* -------------------------------------------------------------------- */
    DDFSubfieldDefn *poSFDefn =
        poField->GetFieldDefn()->FindSubfieldDefn( pszSubfield );
    if( poSFDefn == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get a pointer to the data.                                      */
/* -------------------------------------------------------------------- */
    int         nBytesRemaining;

    const char *l_pachData = poField->GetSubfieldData(poSFDefn,
                                                    &nBytesRemaining,
                                                    iSubfieldIndex);
    if( l_pachData == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Return the extracted value.                                     */
/* -------------------------------------------------------------------- */
    *pnSuccess = TRUE;

    return poSFDefn->ExtractStringData( l_pachData, nBytesRemaining, NULL );
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * Make a copy of a record.
 *
 * This method is used to make a copy of a record that will become (mostly)
 * the properly of application.  However, it is automatically destroyed if
 * the DDFModule it was created relative to is destroyed, as its field
 * and subfield definitions relate to that DDFModule.  However, it does
 * persist even when the record returned by DDFModule::ReadRecord() is
 * invalidated, such as when reading a new record.  This allows an application
 * to cache whole DDFRecords.
 *
 * @return A new copy of the DDFRecord.  This can be delete'd by the
 * application when no longer needed, otherwise it will be cleaned up when
 * the DDFModule it relates to is destroyed or closed.
 */

DDFRecord * DDFRecord::Clone()

{
    DDFRecord *poNR = new DDFRecord( poModule );

    poNR->nReuseHeader = FALSE;
    poNR->nFieldOffset = nFieldOffset;

    poNR->nDataSize = nDataSize;
    poNR->pachData = (char *) CPLMalloc(nDataSize + 1);
    memcpy( poNR->pachData, pachData, nDataSize );
    poNR->pachData[nDataSize] = '\0';

    poNR->nFieldCount = nFieldCount;
    poNR->paoFields = new DDFField[nFieldCount];
    for( int i = 0; i < nFieldCount; i++ )
    {
        int     nOffset;

        nOffset = static_cast<int>(paoFields[i].GetData() - pachData);
        poNR->paoFields[i].Initialize( paoFields[i].GetFieldDefn(),
                                       poNR->pachData + nOffset,
                                       paoFields[i].GetDataSize() );
    }

    poNR->bIsClone = TRUE;
    poModule->AddCloneRecord( poNR );

    return poNR;
}

/************************************************************************/
/*                              CloneOn()                               */
/************************************************************************/

/**
 * Recreate a record referencing another module.
 *
 * Works similarly to the DDFRecord::Clone() method, but creates the
 * new record with reference to a different DDFModule.  All DDFFieldDefn
 * references are transcribed onto the new module based on field names.
 * If any fields don't have a similarly named field on the target module
 * the operation will fail.  No validation of field types and properties
 * is done, but this operation is intended only to be used between
 * modules with matching definitions of all affected fields.
 *
 * The new record will be managed as a clone by the target module in
 * a manner similar to regular clones.
 *
 * @param poTargetModule the module on which the record copy should be
 * created.
 *
 * @return NULL on failure or a pointer to the cloned record.
 */

DDFRecord *DDFRecord::CloneOn( DDFModule *poTargetModule )

{
/* -------------------------------------------------------------------- */
/*      Verify that all fields have a corresponding field definition    */
/*      on the target module.                                           */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nFieldCount; i++ )
    {
        DDFFieldDefn    *poDefn = paoFields[i].GetFieldDefn();

        if( poTargetModule->FindFieldDefn( poDefn->GetName() ) == NULL )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a clone.                                                 */
/* -------------------------------------------------------------------- */
    DDFRecord *poClone = Clone();

/* -------------------------------------------------------------------- */
/*      Update all internal information to reference other module.      */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nFieldCount; i++ )
    {
        DDFField *poField = poClone->paoFields + i;
        DDFFieldDefn *poDefn =
            poTargetModule->FindFieldDefn( poField->GetFieldDefn()->GetName() );

        poField->Initialize( poDefn, poField->GetData(),
                             poField->GetDataSize() );
    }

    poModule->RemoveCloneRecord( poClone );
    poClone->poModule = poTargetModule;
    poTargetModule->AddCloneRecord( poClone );

    return poClone;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

/**
 * Delete a field instance from a record.
 *
 * Remove a field from this record, cleaning up the data
 * portion and repacking the fields list.  We don't try to
 * reallocate the data area of the record to be smaller.
 *
 * NOTE: This method doesn't actually remove the header
 * information for this field from the record tag list yet.
 * This should be added if the resulting record is even to be
 * written back to disk!
 *
 * @param poTarget the field instance on this record to delete.
 *
 * @return TRUE on success, or FALSE on failure.  Failure can occur if
 * poTarget isn't really a field on this record.
 */

int DDFRecord::DeleteField( DDFField *poTarget )

{
    int         iTarget, i;

/* -------------------------------------------------------------------- */
/*      Find which field we are to delete.                              */
/* -------------------------------------------------------------------- */
    for( iTarget = 0; iTarget < nFieldCount; iTarget++ )
    {
        if( paoFields + iTarget == poTarget )
            break;
    }

    if( iTarget == nFieldCount )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Change the target fields data size to zero.  This takes care    */
/*      of repacking the data array, and updating all the following     */
/*      field data pointers.                                            */
/* -------------------------------------------------------------------- */
    ResizeField( poTarget, 0 );

/* -------------------------------------------------------------------- */
/*      remove the target field, moving down all the other fields       */
/*      one step in the field list.                                     */
/* -------------------------------------------------------------------- */
    for( i = iTarget; i < nFieldCount-1; i++ )
    {
        paoFields[i] = paoFields[i+1];
    }

    nFieldCount--;

    return TRUE;
}

/************************************************************************/
/*                            ResizeField()                             */
/************************************************************************/

/**
 * Alter field data size within record.
 *
 * This method will rearrange a DDFRecord altering the amount of space
 * reserved for one of the existing fields.  All following fields will
 * be shifted accordingly.  This includes updating the DDFField infos,
 * and actually moving stuff within the data array after reallocating
 * to the desired size.
 *
 * @param poField the field to alter.
 * @param nNewDataSize the number of data bytes to be reserved for the field.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::ResizeField( DDFField *poField, int nNewDataSize )

{
    int         iTarget, i;
    int         nBytesToMove;

/* -------------------------------------------------------------------- */
/*      Find which field we are to resize.                              */
/* -------------------------------------------------------------------- */
    for( iTarget = 0; iTarget < nFieldCount; iTarget++ )
    {
        if( paoFields + iTarget == poField )
            break;
    }

    if( iTarget == nFieldCount )
    {
        CPLAssert( false );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Reallocate the data buffer accordingly.                         */
/* -------------------------------------------------------------------- */
    int nBytesToAdd = nNewDataSize - poField->GetDataSize();
    const char *pachOldData = pachData;

    // Don't realloc things smaller ... we will cut off some data.
    if( nBytesToAdd > 0 )
    {
        pachData = (char *) CPLRealloc(pachData, nDataSize + nBytesToAdd + 1);
        pachData[nDataSize+nBytesToAdd] = '\0';
    }

    nDataSize += nBytesToAdd;

/* -------------------------------------------------------------------- */
/*      How much data needs to be shifted up or down after this field?  */
/* -------------------------------------------------------------------- */
    nBytesToMove = nDataSize
        - static_cast<int>(poField->GetData()+poField->GetDataSize()-pachOldData+nBytesToAdd);

/* -------------------------------------------------------------------- */
/*      Update fields to point into newly allocated buffer.             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nFieldCount; i++ )
    {
        int     nOffset;

        nOffset = static_cast<int>(paoFields[i].GetData() - pachOldData);
        paoFields[i].Initialize( paoFields[i].GetFieldDefn(),
                                 pachData + nOffset,
                                 paoFields[i].GetDataSize() );
    }

/* -------------------------------------------------------------------- */
/*      Shift the data beyond this field up or down as needed.          */
/* -------------------------------------------------------------------- */
    if( nBytesToMove > 0 )
        memmove( (char *)poField->GetData()+poField->GetDataSize()+nBytesToAdd,
                 (char *)poField->GetData()+poField->GetDataSize(),
                 nBytesToMove );

/* -------------------------------------------------------------------- */
/*      Update the target fields info.                                  */
/* -------------------------------------------------------------------- */
    poField->Initialize( poField->GetFieldDefn(),
                         poField->GetData(),
                         poField->GetDataSize() + nBytesToAdd );

/* -------------------------------------------------------------------- */
/*      Shift all following fields down, and update their data          */
/*      locations.                                                      */
/* -------------------------------------------------------------------- */
    if( nBytesToAdd < 0 )
    {
        for( i = iTarget+1; i < nFieldCount; i++ )
        {
            char *pszOldDataLocation = (char *) paoFields[i].GetData();

            paoFields[i].Initialize( paoFields[i].GetFieldDefn(),
                                     pszOldDataLocation + nBytesToAdd,
                                     paoFields[i].GetDataSize() );
        }
    }
    else
    {
        for( i = nFieldCount-1; i > iTarget; i-- )
        {
            char *pszOldDataLocation = (char *) paoFields[i].GetData();

            paoFields[i].Initialize( paoFields[i].GetFieldDefn(),
                                     pszOldDataLocation + nBytesToAdd,
                                     paoFields[i].GetDataSize() );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                              AddField()                              */
/************************************************************************/

/**
 * Add a new field to record.
 *
 * Add a new zero sized field to the record.  The new field is always
 * added at the end of the record.
 *
 * NOTE: This method doesn't currently update the header information for
 * the record to include the field information for this field, so the
 * resulting record image isn't suitable for writing to disk.  However,
 * everything else about the record state should be updated properly to
 * reflect the new field.
 *
 * @param poDefn the definition of the field to be added.
 *
 * @return the field object on success, or NULL on failure.
 */

DDFField *DDFRecord::AddField( DDFFieldDefn *poDefn )

{
/* -------------------------------------------------------------------- */
/*      Reallocate the fields array larger by one, and initialize       */
/*      the new field.                                                  */
/* -------------------------------------------------------------------- */
    DDFField *paoNewFields = new DDFField[nFieldCount+1];
    if( nFieldCount > 0 )
    {
        memcpy( paoNewFields, paoFields, sizeof(DDFField) * nFieldCount );
        delete[] paoFields;
    }
    paoFields = paoNewFields;
    nFieldCount++;

/* -------------------------------------------------------------------- */
/*      Initialize the new field properly.                              */
/* -------------------------------------------------------------------- */
    if( nFieldCount == 1 )
    {
        paoFields[0].Initialize( poDefn, GetData(), 0 );
    }
    else
    {
        paoFields[nFieldCount-1].Initialize(
            poDefn,
            paoFields[nFieldCount-2].GetData()
            + paoFields[nFieldCount-2].GetDataSize(),
            0 );
    }

/* -------------------------------------------------------------------- */
/*      Initialize field.                                               */
/* -------------------------------------------------------------------- */
    CreateDefaultFieldInstance( paoFields + nFieldCount-1, 0 );

    return paoFields + (nFieldCount - 1);
}

/************************************************************************/
/*                            SetFieldRaw()                             */
/************************************************************************/

/**
 * Set the raw contents of a field instance.
 *
 * @param poField the field to set data within.
 * @param iIndexWithinField The instance of this field to replace.  Must
 * be a value between 0 and GetRepeatCount().  If GetRepeatCount() is used, a
 * new instance of the field is appended.
 * @param pachRawData the raw data to replace this field instance with.
 * @param nRawDataSize the number of bytes pointed to by pachRawData.
 *
 * @return TRUE on success or FALSE on failure.
 */

int
DDFRecord::SetFieldRaw( DDFField *poField, int iIndexWithinField,
                        const char *pachRawData, int nRawDataSize )

{
    int         iTarget, nRepeatCount;

/* -------------------------------------------------------------------- */
/*      Find which field we are to update.                              */
/* -------------------------------------------------------------------- */
    for( iTarget = 0; iTarget < nFieldCount; iTarget++ )
    {
        if( paoFields + iTarget == poField )
            break;
    }

    if( iTarget == nFieldCount )
        return FALSE;

    nRepeatCount = poField->GetRepeatCount();

    if( iIndexWithinField < 0 || iIndexWithinField > nRepeatCount )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Are we adding an instance?  This is easier and different        */
/*      than replacing an existing instance.                            */
/* -------------------------------------------------------------------- */
    if( iIndexWithinField == nRepeatCount
        || !poField->GetFieldDefn()->IsRepeating() )
    {
        if( !poField->GetFieldDefn()->IsRepeating() && iIndexWithinField != 0 )
            return FALSE;

        int nOldSize = poField->GetDataSize();
        if( nOldSize == 0 )
            nOldSize++; // for added DDF_FIELD_TERMINATOR.

        if( !ResizeField( poField, nOldSize + nRawDataSize ) )
            return FALSE;

        char *pachFieldData = (char *) poField->GetData();
        memcpy( pachFieldData + nOldSize - 1,
                pachRawData, nRawDataSize );
        pachFieldData[nOldSize+nRawDataSize-1] = DDF_FIELD_TERMINATOR;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Get a pointer to the start of the existing data for this        */
/*      iteration of the field.                                         */
/* -------------------------------------------------------------------- */
    const char *pachWrkData = NULL;
    int nInstanceSize = 0;

    // We special case this to avoid a lot of warnings when initializing
    // the field the first time.
    if( poField->GetDataSize() == 0 )
    {
        pachWrkData = poField->GetData();
    }
    else
    {
        pachWrkData = poField->GetInstanceData( iIndexWithinField,
                                                &nInstanceSize );
    }

/* -------------------------------------------------------------------- */
/*      Create new image of this whole field.                           */
/* -------------------------------------------------------------------- */
    int nNewFieldSize = poField->GetDataSize() - nInstanceSize + nRawDataSize;

    char *pachNewImage = (char *) CPLMalloc(nNewFieldSize);

    int nPreBytes = static_cast<int>(pachWrkData - poField->GetData());
    int nPostBytes = poField->GetDataSize() - nPreBytes - nInstanceSize;

    memcpy( pachNewImage, poField->GetData(), nPreBytes );
    memcpy( pachNewImage + nPreBytes + nRawDataSize,
            poField->GetData() + nPreBytes + nInstanceSize,
            nPostBytes );
    memcpy( pachNewImage + nPreBytes, pachRawData, nRawDataSize );

/* -------------------------------------------------------------------- */
/*      Resize the field to the desired new size.                       */
/* -------------------------------------------------------------------- */
    ResizeField( poField, nNewFieldSize );

    memcpy( (void *) poField->GetData(), pachNewImage, nNewFieldSize );
    CPLFree( pachNewImage );

    return TRUE;
}

/************************************************************************/
/*                           UpdateFieldRaw()                           */
/************************************************************************/

int
DDFRecord::UpdateFieldRaw( DDFField *poField, int iIndexWithinField,
                           int nStartOffset, int nOldSize,
                           const char *pachRawData, int nRawDataSize )

{
    int         iTarget, nRepeatCount;

/* -------------------------------------------------------------------- */
/*      Find which field we are to update.                              */
/* -------------------------------------------------------------------- */
    for( iTarget = 0; iTarget < nFieldCount; iTarget++ )
    {
        if( paoFields + iTarget == poField )
            break;
    }

    if( iTarget == nFieldCount )
        return FALSE;

    nRepeatCount = poField->GetRepeatCount();

    if( iIndexWithinField < 0 || iIndexWithinField >= nRepeatCount )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Figure out how much pre and post data there is.                 */
/* -------------------------------------------------------------------- */
    int nInstanceSize = 0;

    char *pachWrkData = (char *) poField->GetInstanceData( iIndexWithinField,
                                                           &nInstanceSize );
    int nPreBytes = static_cast<int>(pachWrkData - poField->GetData() + nStartOffset);
    int nPostBytes = poField->GetDataSize() - nPreBytes - nOldSize;

/* -------------------------------------------------------------------- */
/*      If we aren't changing the size, just copy over the existing     */
/*      data.                                                           */
/* -------------------------------------------------------------------- */
    if( nOldSize == nRawDataSize )
    {
        memcpy( pachWrkData + nStartOffset, pachRawData, nRawDataSize );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      If we are shrinking, move in the new data, and shuffle down     */
/*      the old before resizing.                                        */
/* -------------------------------------------------------------------- */
    if( nRawDataSize < nOldSize )
    {
        memcpy( ((char*) poField->GetData()) + nPreBytes,
                pachRawData, nRawDataSize );
        memmove( ((char *) poField->GetData()) + nPreBytes + nRawDataSize,
                 ((char *) poField->GetData()) + nPreBytes + nOldSize,
                 nPostBytes );
    }

/* -------------------------------------------------------------------- */
/*      Resize the whole buffer.                                        */
/* -------------------------------------------------------------------- */
    if( !ResizeField( poField,
                      poField->GetDataSize() - nOldSize + nRawDataSize ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      If we growing the buffer, shuffle up the post data, and         */
/*      move in our new values.                                         */
/* -------------------------------------------------------------------- */
    if( nRawDataSize >= nOldSize )
    {
        memmove( ((char *) poField->GetData()) + nPreBytes + nRawDataSize,
                 ((char *) poField->GetData()) + nPreBytes + nOldSize,
                 nPostBytes );
        memcpy( ((char*) poField->GetData()) + nPreBytes,
                pachRawData, nRawDataSize );
    }

    return TRUE;
}

/************************************************************************/
/*                           ResetDirectory()                           */
/*                                                                      */
/*      Re-prepares the directory information for the record.           */
/************************************************************************/

int DDFRecord::ResetDirectory()

{
    int iField;

/* -------------------------------------------------------------------- */
/*      Eventually we should try to optimize the size of offset and     */
/*      field length.                                                   */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Compute how large the directory needs to be.                    */
/* -------------------------------------------------------------------- */
    int nEntrySize, nDirSize;

    nEntrySize = _sizeFieldPos + _sizeFieldLength + _sizeFieldTag;
    nDirSize = nEntrySize * nFieldCount + 1;

/* -------------------------------------------------------------------- */
/*      If the directory size is different than what is currently       */
/*      reserved for it, we must resize.                                */
/* -------------------------------------------------------------------- */
    if( nDirSize != nFieldOffset )
    {
        const int nNewDataSize = nDataSize - nFieldOffset + nDirSize;
        char *pachNewData = (char *) CPLMalloc(nNewDataSize+1);
        pachNewData[nNewDataSize] = '\0';
        memcpy( pachNewData + nDirSize,
                pachData + nFieldOffset,
                nNewDataSize - nDirSize );

        for( iField = 0; paoFields != NULL && iField < nFieldCount; iField++ )
        {
            int nOffset;
            DDFField *poField = /*GetField( iField )*/ paoFields + iField;

            nOffset = static_cast<int>(poField->GetData() - pachData - nFieldOffset + nDirSize);
            poField->Initialize( poField->GetFieldDefn(),
                                 pachNewData + nOffset,
                                 poField->GetDataSize() );
        }

        CPLFree( pachData );
        pachData = pachNewData;
        nDataSize = nNewDataSize;
        nFieldOffset = nDirSize;
    }

/* -------------------------------------------------------------------- */
/*      Now set each directory entry.                                   */
/* -------------------------------------------------------------------- */
    for( iField = 0; paoFields != NULL && iField < nFieldCount; iField++ )
    {
        DDFField *poField = /*GetField( iField )*/ paoFields + iField;
        DDFFieldDefn *poDefn = poField->GetFieldDefn();
        char      szFormat[128];

        snprintf( szFormat, sizeof(szFormat), "%%%ds%%0%dd%%0%dd",
                 _sizeFieldTag, _sizeFieldLength, _sizeFieldPos );

        snprintf( pachData + nEntrySize * iField,
                  nEntrySize+1,
                  szFormat,
                 poDefn->GetName(), poField->GetDataSize(),
                 poField->GetData() - pachData - nFieldOffset );
    }

    pachData[nEntrySize * nFieldCount] = DDF_FIELD_TERMINATOR;

    return TRUE;
}

/************************************************************************/
/*                     CreateDefaultFieldInstance()                     */
/************************************************************************/

/**
 * Initialize default instance.
 *
 * This method is normally only used internally by the AddField() method
 * to initialize the new field instance with default subfield values.  It
 * installs default data for one instance of the field in the record
 * using the DDFFieldDefn::GetDefaultValue() method and
 * DDFRecord::SetFieldRaw().
 *
 * @param poField the field within the record to be assign a default
 * instance.
 * @param iIndexWithinField the instance to set (may not have been tested with
 * values other than 0).
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::CreateDefaultFieldInstance( DDFField *poField,
                                           int iIndexWithinField )

{
    int nRawSize = 0;
    char *pachRawData = poField->GetFieldDefn()->GetDefaultValue( &nRawSize );
    if( pachRawData == NULL )
        return FALSE;

    const int nSuccess =
        SetFieldRaw( poField, iIndexWithinField, pachRawData, nRawSize);

    CPLFree( pachRawData );

    return nSuccess;
}

/************************************************************************/
/*                         SetStringSubfield()                          */
/************************************************************************/

/**
 * Set a string subfield in record.
 *
 * The value of a given subfield is replaced with a new string value
 * formatted appropriately.
 *
 * @param pszField the field name to operate on.
 * @param iFieldIndex the field index to operate on (zero based).
 * @param pszSubfield the subfield name to operate on.
 * @param iSubfieldIndex the subfield index to operate on (zero based).
 * @param pszValue the new string to place in the subfield.  This may be
 * arbitrary binary bytes if nValueLength is specified.
 * @param nValueLength the number of valid bytes in pszValue, may be -1 to
 * internally fetch with strlen().
 *
 * @return TRUE if successful, and FALSE if not.
 */

int DDFRecord::SetStringSubfield( const char *pszField, int iFieldIndex,
                                  const char *pszSubfield, int iSubfieldIndex,
                                  const char *pszValue, int nValueLength )

{
/* -------------------------------------------------------------------- */
/*      Fetch the field. If this fails, return zero.                    */
/* -------------------------------------------------------------------- */
    DDFField *poField = FindField( pszField, iFieldIndex );
    if( poField == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the subfield definition                                     */
/* -------------------------------------------------------------------- */
    DDFSubfieldDefn *poSFDefn =
        poField->GetFieldDefn()->FindSubfieldDefn( pszSubfield );
    if( poSFDefn == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      How long will the formatted value be?                           */
/* -------------------------------------------------------------------- */
    int nFormattedLen;

    if( !poSFDefn->FormatStringValue( NULL, 0, &nFormattedLen, pszValue,
                                      nValueLength ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get a pointer to the data.                                      */
/* -------------------------------------------------------------------- */
    int         nMaxBytes;
    char *pachSubfieldData = (char *)
        poField->GetSubfieldData(poSFDefn, &nMaxBytes,
                                 iSubfieldIndex);
    if( pachSubfieldData == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Add new instance if we have run out of data.                    */
/* -------------------------------------------------------------------- */
    if( nMaxBytes == 0
        || (nMaxBytes == 1 && pachSubfieldData[0] == DDF_FIELD_TERMINATOR) )
    {
        CreateDefaultFieldInstance( poField, iSubfieldIndex );

        // Refetch.
        pachSubfieldData = (char *)
            poField->GetSubfieldData(poSFDefn, &nMaxBytes,
                                     iSubfieldIndex);
        if( pachSubfieldData == NULL )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If the new length matches the existing length, just overlay     */
/*      and return.                                                     */
/* -------------------------------------------------------------------- */
    int nExistingLength;

    poSFDefn->GetDataLength( pachSubfieldData, nMaxBytes, &nExistingLength );

    if( nExistingLength == nFormattedLen )
    {
        return poSFDefn->FormatStringValue( pachSubfieldData, nFormattedLen,
                                            NULL, pszValue, nValueLength );
    }

/* -------------------------------------------------------------------- */
/*      We will need to resize the raw data.                            */
/* -------------------------------------------------------------------- */
    int nInstanceSize = 0;

    const char *pachFieldInstData = poField->GetInstanceData( iFieldIndex,
                                                              &nInstanceSize );

    const int nStartOffset =
        static_cast<int>(pachSubfieldData - pachFieldInstData);

    char *pachNewData = (char *) CPLMalloc(nFormattedLen);
    poSFDefn->FormatStringValue( pachNewData, nFormattedLen, NULL,
                                 pszValue, nValueLength );

    const int nSuccess =
        UpdateFieldRaw( poField, iFieldIndex,
                        nStartOffset, nExistingLength,
                        pachNewData, nFormattedLen );

    CPLFree( pachNewData );

    return nSuccess;
}

/************************************************************************/
/*                           SetIntSubfield()                           */
/************************************************************************/

/**
 * Set an integer subfield in record.
 *
 * The value of a given subfield is replaced with a new integer value
 * formatted appropriately.
 *
 * @param pszField the field name to operate on.
 * @param iFieldIndex the field index to operate on (zero based).
 * @param pszSubfield the subfield name to operate on.
 * @param iSubfieldIndex the subfield index to operate on (zero based).
 * @param nNewValue the new value to place in the subfield.
 *
 * @return TRUE if successful, and FALSE if not.
 */

int DDFRecord::SetIntSubfield( const char *pszField, int iFieldIndex,
                               const char *pszSubfield, int iSubfieldIndex,
                               int nNewValue )

{
/* -------------------------------------------------------------------- */
/*      Fetch the field. If this fails, return zero.                    */
/* -------------------------------------------------------------------- */
    DDFField *poField = FindField( pszField, iFieldIndex );
    if( poField == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the subfield definition                                     */
/* -------------------------------------------------------------------- */
    DDFSubfieldDefn *poSFDefn =
        poField->GetFieldDefn()->FindSubfieldDefn( pszSubfield );
    if( poSFDefn == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      How long will the formatted value be?                           */
/* -------------------------------------------------------------------- */
    int nFormattedLen;

    if( !poSFDefn->FormatIntValue( NULL, 0, &nFormattedLen, nNewValue ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get a pointer to the data.                                      */
/* -------------------------------------------------------------------- */
    int         nMaxBytes;
    char *pachSubfieldData = (char *)
        poField->GetSubfieldData(poSFDefn, &nMaxBytes,
                                 iSubfieldIndex);
    if( pachSubfieldData == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Add new instance if we have run out of data.                    */
/* -------------------------------------------------------------------- */
    if( nMaxBytes == 0
        || (nMaxBytes == 1 && pachSubfieldData[0] == DDF_FIELD_TERMINATOR) )
    {
        CreateDefaultFieldInstance( poField, iSubfieldIndex );

        // Refetch.
        pachSubfieldData = (char *)
            poField->GetSubfieldData(poSFDefn, &nMaxBytes,
                                     iSubfieldIndex);
        if( pachSubfieldData == NULL )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If the new length matches the existing length, just overlay     */
/*      and return.                                                     */
/* -------------------------------------------------------------------- */
    int nExistingLength;

    poSFDefn->GetDataLength( pachSubfieldData, nMaxBytes, &nExistingLength );

    if( nExistingLength == nFormattedLen )
    {
        return poSFDefn->FormatIntValue( pachSubfieldData, nFormattedLen,
                                         NULL, nNewValue );
    }

/* -------------------------------------------------------------------- */
/*      We will need to resize the raw data.                            */
/* -------------------------------------------------------------------- */
    int nInstanceSize = 0;

    const char *pachFieldInstData = poField->GetInstanceData( iFieldIndex,
                                                              &nInstanceSize );

    const int nStartOffset =
        static_cast<int>(pachSubfieldData - pachFieldInstData);

    char *pachNewData = (char *) CPLMalloc(nFormattedLen);
    poSFDefn->FormatIntValue( pachNewData, nFormattedLen, NULL,
                              nNewValue );

    const int nSuccess =
        UpdateFieldRaw( poField, iFieldIndex,
                        nStartOffset, nExistingLength,
                        pachNewData, nFormattedLen );

    CPLFree( pachNewData );

    return nSuccess;
}

/************************************************************************/
/*                          SetFloatSubfield()                          */
/************************************************************************/

/**
 * Set a float subfield in record.
 *
 * The value of a given subfield is replaced with a new float value
 * formatted appropriately.
 *
 * @param pszField the field name to operate on.
 * @param iFieldIndex the field index to operate on (zero based).
 * @param pszSubfield the subfield name to operate on.
 * @param iSubfieldIndex the subfield index to operate on (zero based).
 * @param dfNewValue the new value to place in the subfield.
 *
 * @return TRUE if successful, and FALSE if not.
 */

int DDFRecord::SetFloatSubfield( const char *pszField, int iFieldIndex,
                                 const char *pszSubfield, int iSubfieldIndex,
                                 double dfNewValue )

{
/* -------------------------------------------------------------------- */
/*      Fetch the field. If this fails, return zero.                    */
/* -------------------------------------------------------------------- */
    DDFField *poField = FindField( pszField, iFieldIndex );
    if( poField == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the subfield definition                                     */
/* -------------------------------------------------------------------- */
    DDFSubfieldDefn *poSFDefn =
        poField->GetFieldDefn()->FindSubfieldDefn( pszSubfield );
    if( poSFDefn == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      How long will the formatted value be?                           */
/* -------------------------------------------------------------------- */
    int nFormattedLen;

    if( !poSFDefn->FormatFloatValue( NULL, 0, &nFormattedLen, dfNewValue ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get a pointer to the data.                                      */
/* -------------------------------------------------------------------- */
    int         nMaxBytes;
    char *pachSubfieldData = (char *)
        poField->GetSubfieldData(poSFDefn, &nMaxBytes,
                                 iSubfieldIndex);
    if( pachSubfieldData == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Add new instance if we have run out of data.                    */
/* -------------------------------------------------------------------- */
    if( nMaxBytes == 0
        || (nMaxBytes == 1 && pachSubfieldData[0] == DDF_FIELD_TERMINATOR) )
    {
        CreateDefaultFieldInstance( poField, iSubfieldIndex );

        // Refetch.
        pachSubfieldData = (char *)
            poField->GetSubfieldData(poSFDefn, &nMaxBytes,
                                     iSubfieldIndex);
        if( pachSubfieldData == NULL )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If the new length matches the existing length, just overlay     */
/*      and return.                                                     */
/* -------------------------------------------------------------------- */
    int nExistingLength;

    poSFDefn->GetDataLength( pachSubfieldData, nMaxBytes, &nExistingLength );

    if( nExistingLength == nFormattedLen )
    {
        return poSFDefn->FormatFloatValue( pachSubfieldData, nFormattedLen,
                                         NULL, dfNewValue );
    }

/* -------------------------------------------------------------------- */
/*      We will need to resize the raw data.                            */
/* -------------------------------------------------------------------- */
    int nInstanceSize = 0;

    const char *pachFieldInstData =
        poField->GetInstanceData( iFieldIndex, &nInstanceSize );

    const int nStartOffset = (int) (pachSubfieldData - pachFieldInstData);

    char *pachNewData = (char *) CPLMalloc(nFormattedLen);
    poSFDefn->FormatFloatValue( pachNewData, nFormattedLen, NULL,
                              dfNewValue );

    const int nSuccess =
        UpdateFieldRaw( poField, iFieldIndex,
                        nStartOffset, nExistingLength,
                        pachNewData, nFormattedLen );

    CPLFree( pachNewData );

    return nSuccess;
}
