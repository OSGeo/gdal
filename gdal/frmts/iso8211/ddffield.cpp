/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFField class.
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

#include "cpl_port.h"
#include "iso8211.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include "cpl_conv.h"

CPL_CVSID("$Id$");

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
 * A variety of information about this field, and all its
 * subfields is written to the given debugging file handle.  Note that
 * field definition information (ala DDFFieldDefn) isn't written.
 *
 * @param fp The standard IO file handle to write to.  i.e. stderr
 */

void DDFField::Dump( FILE * fp )

{
    int nMaxRepeat = 8;

    if( getenv("DDF_MAXDUMP") != NULL )
        nMaxRepeat = atoi(getenv("DDF_MAXDUMP"));

    fprintf( fp, "  DDFField:\n" );
    fprintf( fp, "      Tag = `%s'\n", poDefn->GetName() );
    fprintf( fp, "      DataSize = %d\n", nDataSize );

    fprintf( fp, "      Data = `" );
    for( int i = 0; i < std::min(nDataSize, 40); i++ )
    {
        if( pachData[i] < 32 || pachData[i] > 126 )
            fprintf( fp, "\\%02X", ((unsigned char *) pachData)[i] );
        else
            fprintf( fp, "%c", pachData[i] );
    }

    if( nDataSize > 40 )
        fprintf( fp, "..." );
    fprintf( fp, "'\n" );

/* -------------------------------------------------------------------- */
/*      dump the data of the subfields.                                 */
/* -------------------------------------------------------------------- */
    int iOffset = 0;

    for( int nLoopCount = 0; nLoopCount < GetRepeatCount(); nLoopCount++ )
    {
        if( nLoopCount > nMaxRepeat )
        {
            fprintf( fp, "      ...\n" );
            break;
        }

        for( int i = 0; i < poDefn->GetSubfieldCount(); i++ )
        {
            poDefn->GetSubfield(i)->DumpData( pachData + iOffset,
                                              nDataSize - iOffset, fp );

            int nBytesConsumed = 0;
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
    if( poSFDefn == NULL )
        return NULL;

    int iOffset = 0;
    if( iSubfieldIndex > 0 && poDefn->GetFixedWidth() > 0 )
    {
        iOffset = poDefn->GetFixedWidth() * iSubfieldIndex;
        iSubfieldIndex = 0;
    }

    while( iSubfieldIndex >= 0 )
    {
        for( int iSF = 0; iSF < poDefn->GetSubfieldCount(); iSF++ )
        {
            DDFSubfieldDefn * poThisSFDefn = poDefn->GetSubfield( iSF );

            if( nDataSize < iOffset )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid data size for subfield %s of %s",
                         poThisSFDefn->GetName(), poDefn->GetName());
                return NULL;
            }

            if( poThisSFDefn == poSFDefn && iSubfieldIndex == 0 )
            {
                if( pnMaxBytes != NULL )
                    *pnMaxBytes = nDataSize - iOffset;

                return pachData + iOffset;
            }

            int nBytesConsumed = 0;
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
 * for a demonstration of handling repeated fields properly.
 */

int DDFField::GetRepeatCount()

{
    if( !poDefn->IsRepeating() )
        return 1;

/* -------------------------------------------------------------------- */
/*      The occurrence count depends on how many copies of this         */
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
    int iOffset = 0;
    int iRepeatCount = 1;

    while( true )
    {
        const int iOffsetBefore = iOffset;
        for( int iSF = 0; iSF < poDefn->GetSubfieldCount(); iSF++ )
        {
            DDFSubfieldDefn * poThisSFDefn = poDefn->GetSubfield( iSF );

            int nBytesConsumed = 0;
            if( poThisSFDefn->GetWidth() > nDataSize - iOffset )
                nBytesConsumed = poThisSFDefn->GetWidth();
            else
                poThisSFDefn->GetDataLength( pachData+iOffset,
                                             nDataSize - iOffset,
                                             &nBytesConsumed);

            iOffset += nBytesConsumed;
            if( iOffset > nDataSize )
                return iRepeatCount - 1;
        }
        if( iOffset == iOffsetBefore )
        {
            // Should probably emit error
            return iRepeatCount - 1;
        }

        if( iOffset > nDataSize - 2 )
            return iRepeatCount;

        iRepeatCount++;
    }
}

/************************************************************************/
/*                          GetInstanceData()                           */
/************************************************************************/

/**
 * Get field instance data and size.
 *
 * The returned data pointer and size values are suitable for use with
 * DDFRecord::SetFieldRaw().
 *
 * @param nInstance a value from 0 to GetRepeatCount()-1.
 * @param pnInstanceSize a location to put the size (in bytes) of the
 * field instance data returned.  This size will include the unit terminator
 * (if any), but not the field terminator.  This size pointer may be NULL
 * if not needed.
 *
 * @return the data pointer, or NULL on error.
 */

const char *DDFField::GetInstanceData( int nInstance,
                                       int *pnInstanceSize )

{
    int nRepeatCount = GetRepeatCount();

    if( nInstance < 0 || nInstance >= nRepeatCount )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Special case for fields without subfields (like "0001").  We    */
/*      don't currently handle repeating simple fields.                 */
/* -------------------------------------------------------------------- */
    if( poDefn->GetSubfieldCount() == 0 )
    {
        const char *pachWrkData = GetData();
        if( pnInstanceSize != NULL )
            *pnInstanceSize = GetDataSize();
        return pachWrkData;
    }

/* -------------------------------------------------------------------- */
/*      Get a pointer to the start of the existing data for this        */
/*      iteration of the field.                                         */
/* -------------------------------------------------------------------- */
    int nBytesRemaining1 = 0;
    int nBytesRemaining2 = 0;
    DDFSubfieldDefn *poFirstSubfield = poDefn->GetSubfield(0);

    const char *pachWrkData =
        GetSubfieldData(poFirstSubfield, &nBytesRemaining1, nInstance);
    if( pachWrkData == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Figure out the size of the entire field instance, including     */
/*      unit terminators, but not any trailing field terminator.        */
/* -------------------------------------------------------------------- */
    if( pnInstanceSize != NULL )
    {
        DDFSubfieldDefn *poLastSubfield =
            poDefn->GetSubfield(poDefn->GetSubfieldCount()-1);

        const char *pachLastData =
            GetSubfieldData( poLastSubfield, &nBytesRemaining2, nInstance );
        if( pachLastData == NULL )
            return NULL;

        int nLastSubfieldWidth = 0;
        poLastSubfield->GetDataLength( pachLastData, nBytesRemaining2,
                                       &nLastSubfieldWidth );

        *pnInstanceSize =
            nBytesRemaining1 - (nBytesRemaining2 - nLastSubfieldWidth);
    }

    return pachWrkData;
}
