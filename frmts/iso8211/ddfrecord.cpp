/******************************************************************************
 * $Id$
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFRecord class.
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
 * Revision 1.3  1999/05/06 14:48:28  warmerda
 * Fixed EOF handling in files with reused headers
 *
 * Revision 1.2  1999/05/06 14:24:29  warmerda
 * minor optimization, don't emit an error on EOF
 *
 * Revision 1.1  1999/04/27 18:45:05  warmerda
 * New
 *
 */

#include "iso8211.h"
#include "cpl_conv.h"

/************************************************************************/
/*                             DDFRecord()                              */
/************************************************************************/

DDFRecord::DDFRecord( DDFModule * poModuleIn )

{
    poModule = poModuleIn;

    nReuseHeader = FALSE;

    nFieldOffset = -1;

    nDataSize = 0;
    pachData = NULL;

    nFieldCount = 0;
    paoFields = NULL;
}

/************************************************************************/
/*                             ~DDFRecord()                             */
/************************************************************************/

DDFRecord::~DDFRecord()

{
    Clear();
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out record contents to debugging file.
 *
 * A variety of information about this record, and all it's fields and
 * subfields is written to the given debugging file handle.  Note that
 * field definition information (ala DDFFieldDefn) isn't written.
 *
 * @param fp The standard io file handle to write to.  ie. stderr
 */

void DDFRecord::Dump( FILE * fp )

{
    fprintf( fp, "DDFRecord:\n" );
    fprintf( fp, "    nReuseHeader = %d\n", nReuseHeader );
    fprintf( fp, "    nDataSize = %d\n", nDataSize );

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
        return( ReadHeader() );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we read just the data and carefully overlay it on     */
/*      the previous records data without disturbing the rest of the    */
/*      record.                                                         */
/* -------------------------------------------------------------------- */
    size_t	nReadBytes;

    nReadBytes = VSIFRead( pachData + nFieldOffset, 1,
                           nDataSize - nFieldOffset,
                           poModule->GetFP() );
    if( nReadBytes != (size_t) (nDataSize - nFieldOffset)
        && nReadBytes == 0
        && VSIFEof( poModule->GetFP() ) )
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
    static const size_t nLeaderSize = 24;

/* -------------------------------------------------------------------- */
/*      Clear any existing information.                                 */
/* -------------------------------------------------------------------- */
    Clear();
    
/* -------------------------------------------------------------------- */
/*      Read the 24 byte leader.                                        */
/* -------------------------------------------------------------------- */
    char	achLeader[nLeaderSize];
    int		nReadBytes;

    nReadBytes = VSIFRead(achLeader,1,nLeaderSize,poModule->GetFP());
    if( nReadBytes == 0 && VSIFEof( poModule->GetFP() ) )
    {
        return FALSE;
    }
    else if( nReadBytes != (int) nLeaderSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Leader is short on DDF file.\n" );
        
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract information from leader.                                */
/* -------------------------------------------------------------------- */
    int		_recLength, _fieldAreaStart, _sizeFieldLength;
    int		_sizeFieldPos, _sizeFieldTag;
    char	_leaderIden;
    
    _recLength			  = DDFScanInt( achLeader+0, 5 );
    _leaderIden 		  = achLeader[6];
    _fieldAreaStart               = DDFScanInt(achLeader+12,5);
    
    _sizeFieldLength = achLeader[20] - '0';
    _sizeFieldPos = achLeader[21] - '0';
    _sizeFieldTag = achLeader[23] - '0';

    if( _leaderIden == 'R' )
        nReuseHeader = TRUE;

    nFieldOffset = _fieldAreaStart - nLeaderSize;

/* -------------------------------------------------------------------- */
/*      Read the remainder of the record.                               */
/* -------------------------------------------------------------------- */
    nDataSize = _recLength - nLeaderSize;
    pachData = (char *) CPLMalloc(nDataSize);

    if( VSIFRead( pachData, 1, nDataSize, poModule->GetFP()) !=
        (size_t) nDataSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Data record is short on DDF file." );
        
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Loop over the directory entries, making a pass counting them.   */
/* -------------------------------------------------------------------- */
    int		i;
    int		nFieldEntryWidth;

    nFieldEntryWidth = _sizeFieldLength + _sizeFieldPos + _sizeFieldTag;
    nFieldCount = 0;
    for( i = 0; i < nDataSize; i += nFieldEntryWidth )
    {
        if( pachData[i] == DDF_FIELD_TERMINATOR )
            break;

        nFieldCount++;
    }
    
/* ==================================================================== */
/*      Allocate, and read field definitions.                           */
/* ==================================================================== */
    paoFields = new DDFField[nFieldCount];
    
    for( i = 0; i < nFieldCount; i++ )
    {
        char	szTag[128];
        int	nEntryOffset = i*nFieldEntryWidth;
        int	nFieldLength, nFieldPos;
        
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
        DDFFieldDefn	*poFieldDefn = poModule->FindFieldDefn( szTag );

        if( poFieldDefn == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Undefined field `%s' encountered in data record.",
                      szTag );
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

/************************************************************************/
/*                             FindField()                              */
/************************************************************************/

/**
 * Find the named field within this record.
 *
 * @param pszName The name of the field to fetch.  The comparison is
 * case insensitive.
 *
 * @return Pointer to the requested DDFField.  This pointer is to an
 * internal object, and should not be freed.  It remains valid until
 * the next record read. 
 */

DDFField * DDFRecord::FindField( const char * pszName )

{
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( EQUAL(paoFields[i].GetFieldDefn()->GetName(),pszName) )
            return paoFields + i;
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
