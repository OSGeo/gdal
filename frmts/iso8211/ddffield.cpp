/******************************************************************************
 * $Id$
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFField class.
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
 * Revision 1.6  1999/06/23 02:14:08  warmerda
 * added support for variable width repeaters to GetRepeatCount()
 *
 * Revision 1.5  1999/06/01 19:10:38  warmerda
 * don't assert on variable length repeating fields
 *
 * Revision 1.4  1999/05/07 14:10:49  warmerda
 * added maxbytes to getsubfielddata()
 *
 * Revision 1.3  1999/05/06 15:39:26  warmerda
 * avoid printing non-ASCII characters
 *
 * Revision 1.2  1999/04/27 22:09:50  warmerda
 * updated docs
 *
 * Revision 1.1  1999/04/27 18:45:05  warmerda
 * New
 *
 */

#include "iso8211.h"
#include "cpl_conv.h"

// Note, we implement no constructor for this class to make instantiation
// cheaper.  It is required that the Initialize() be called before anything
// else.

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void DDFField::Initialize( DDFFieldDefn *poDefnIn, const char * pachDataIn,
                           int nDataSizeIn )

{
    pachData = pachDataIn;
    nDataSize = nDataSizeIn;
    poDefn = poDefnIn;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out field contents to debugging file.
 *
 * A variety of information about this field, and all it's
 * subfields is written to the given debugging file handle.  Note that
 * field definition information (ala DDFFieldDefn) isn't written.
 *
 * @param fp The standard io file handle to write to.  ie. stderr
 */

void DDFField::Dump( FILE * fp )

{
    fprintf( fp, "  DDFField:\n" );
    fprintf( fp, "      Tag = `%s'\n", poDefn->GetName() );
    fprintf( fp, "      DataSize = %d\n", nDataSize );

    fprintf( fp, "      Data = `" );
    for( int i = 0; i < MIN(nDataSize,40); i++ )
    {
        if( pachData[i] < 32 && pachData[i] > 126 )
            fprintf( fp, "%c", '^' );
        else
            fprintf( fp, "%c", pachData[i] );
    }

    if( nDataSize > 40 )
        fprintf( fp, "..." );
    fprintf( fp, "'\n" );

/* -------------------------------------------------------------------- */
/*      dump the data of the subfields.                                 */
/* -------------------------------------------------------------------- */
    int		iOffset = 0, nLoopCount;

    for( nLoopCount = 0; nLoopCount < GetRepeatCount(); nLoopCount++ )
    {
        if( nLoopCount > 8 )
        {
            fprintf( fp, "      ...\n" );
            break;
        }
        
        for( int i = 0; i < poDefn->GetSubfieldCount(); i++ )
        {
            int		nBytesConsumed;

            poDefn->GetSubfield(i)->DumpData( pachData + iOffset,
                                              nDataSize - iOffset, fp );
        
            poDefn->GetSubfield(i)->GetDataLength( pachData + iOffset,
                                                   nDataSize - iOffset,
                                                   &nBytesConsumed );

            iOffset += nBytesConsumed;
        }
    }
}

/************************************************************************/
/*                          GetSubfieldData()                           */
/************************************************************************/

/**
 * Fetch raw data pointer for a particular subfield of this field.
 *
 * The passed DDFSubfieldDefn (poSFDefn) should be acquired from the
 * DDFFieldDefn corresponding with this field.  This is normally done
 * once before reading any records.  This method involves a series of
 * calls to DDFSubfield::GetDataLength() in order to track through the
 * DDFField data to that belonging to the requested subfield.  This can
 * be relatively expensive.<p>
 *
 * @param poSFDefn The definition of the subfield for which the raw
 * data pointer is desired.
 * @param pnMaxBytes The maximum number of bytes that can be accessed from
 * the returned data pointer is placed in this int, unless it is NULL.
 * @param iSubfieldIndex The instance of this subfield to fetch.  Use zero
 * (the default) for the first instance.
 *
 * @return A pointer into the DDFField's data that belongs to the subfield.
 * This returned pointer is invalidated by the next record read
 * (DDFRecord::ReadRecord()) and the returned pointer should not be freed
 * by the application.
 */

const char *DDFField::GetSubfieldData( DDFSubfieldDefn *poSFDefn,
                                       int *pnMaxBytes, int iSubfieldIndex )

{
    int		iOffset = 0;
    
    if( poSFDefn == NULL )
        return NULL;

    while( iSubfieldIndex >= 0 )
    {
        for( int iSF = 0; iSF < poDefn->GetSubfieldCount(); iSF++ )
        {
            int	nBytesConsumed;
            DDFSubfieldDefn * poThisSFDefn = poDefn->GetSubfield( iSF );
            
            if( poThisSFDefn == poSFDefn && iSubfieldIndex == 0 )
            {
                if( pnMaxBytes != NULL )
                    *pnMaxBytes = nDataSize - iOffset;
                
                return pachData + iOffset;
            }
            
            poThisSFDefn->GetDataLength( pachData+iOffset, nDataSize - iOffset,
                                         &nBytesConsumed);
            iOffset += nBytesConsumed;
        }

        iSubfieldIndex--;
    }

    // We didn't find our target subfield or instance!
    return NULL;
}

/************************************************************************/
/*                           GetRepeatCount()                           */
/************************************************************************/

/**
 * How many times do the subfields of this record repeat?  This    
 * will always be one for non-repeating fields.
 *
 * @return The number of times that the subfields of this record occur
 * in this record.  This will be one for non-repeating fields.
 *
 * @see <a href="example.html">8211view example program</a>
 * for demonstation of handling repeated fields properly.
 */

int DDFField::GetRepeatCount()

{
    if( !poDefn->IsRepeating() )
        return 1;

/* -------------------------------------------------------------------- */
/*      The occurance count depends on how many copies of this          */
/*      field's list of subfields can fit into the data space.          */
/* -------------------------------------------------------------------- */
    if( poDefn->GetFixedWidth() )
    {
        return nDataSize / poDefn->GetFixedWidth();
    }

/* -------------------------------------------------------------------- */
/*      Note that it may be legal to have repeating variable width      */
/*      subfields, but I don't have any samples, so I ignore it for     */
/*      now.                                                            */
/*                                                                      */
/*      The file data/cape_royal_AZ_DEM/1183XREF.DDF has a repeating    */
/*      variable length field, but the count is one, so it isn't        */
/*      much value for testing.                                         */
/* -------------------------------------------------------------------- */
    int		iOffset = 0, iRepeatCount = 1;
    
    while( TRUE )
    {
        for( int iSF = 0; iSF < poDefn->GetSubfieldCount(); iSF++ )
        {
            int nBytesConsumed;
            DDFSubfieldDefn * poThisSFDefn = poDefn->GetSubfield( iSF );
            
            poThisSFDefn->GetDataLength( pachData+iOffset, nDataSize - iOffset,
                                         &nBytesConsumed);
            iOffset += nBytesConsumed;
            if( iOffset >= nDataSize )
                return iRepeatCount - 1;
        }

        if( iOffset > nDataSize - 2 )
            return iRepeatCount;

        iRepeatCount++;
    }
}

