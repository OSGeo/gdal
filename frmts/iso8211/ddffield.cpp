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
        if( pachData[i] < 32 )
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
 * This method only fetches data for the first instance of repeating
 * subfields.  Use the DDFSubfieldDefn methods such as
 * DDFSubfieldDefn::ExtractIntData() directly to step through the record
 * data if you want to extract repeated fields.  That is also the most
 * efficient means to extract all the data from a field with many subfields.
 *
 * @param poSFDefn The definition of the subfield for which the raw
 * data pointer is desired.
 *
 * @return A pointer into the DDFField's data that belongs to the subfield.
 * This returned pointer is invalidated by the next record read
 * (DDFRecord::ReadRecord()) and the returned pointer should not be freed
 * by the application.
 */

const char *DDFField::GetSubfieldData( DDFSubfieldDefn *poSFDefn )

{
    int		iOffset = 0;
    
    if( poSFDefn == NULL )
        return NULL;

    for( int iSF = 0; iSF < poDefn->GetSubfieldCount(); iSF++ )
    {
        int	nBytesConsumed;
        DDFSubfieldDefn * poThisSFDefn = poDefn->GetSubfield( iSF );
        
        if( poThisSFDefn == poSFDefn )
            return pachData + iOffset;

        poThisSFDefn->GetDataLength( pachData+iOffset, nDataSize - iOffset,
                                     &nBytesConsumed);
        iOffset += nBytesConsumed;
    }

    // We didn't find our target subfield!
    CPLAssert( FALSE );
    
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
/*      Not that it may be legal to have repeating variable width       */
/*      subfields, but I don't have any samples, so I ignore it for now.*/
/* -------------------------------------------------------------------- */
    CPLAssert( poDefn->GetFixedWidth() > 0 );
    if( poDefn->GetFixedWidth() ==  0 )
        return 1;

/* -------------------------------------------------------------------- */
/*      The occurance count depends on how many copies of this          */
/*      field's list of subfields can fit into the data space.          */
/* -------------------------------------------------------------------- */
    return nDataSize / poDefn->GetFixedWidth();
}

