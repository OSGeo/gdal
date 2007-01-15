/******************************************************************************
 * $Id$
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.17  2006/04/04 04:24:06  fwarmerdam
 * update contact info
 *
 * Revision 1.16  2003/07/03 15:38:46  warmerda
 * some write capabilities added
 *
 * Revision 1.15  2001/08/30 21:08:19  warmerda
 * expand tabs
 *
 * Revision 1.14  2001/08/30 02:14:50  warmerda
 * Provide for control of max number of field instances dumped via environment.
 *
 * Revision 1.13  2001/08/27 19:09:00  warmerda
 * added GetInstanceData() method on DDFField
 *
 * Revision 1.12  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.11  2000/09/19 14:09:11  warmerda
 * fixed dump of binary info
 *
 * Revision 1.10  2000/06/13 13:38:39  warmerda
 * Improved reporting of binary data in Dump method.
 * Fixed GetRepeatCount() so that short field data can be properly detected.
 *
 * Revision 1.9  1999/11/18 19:03:04  warmerda
 * expanded tabs
 *
 * Revision 1.8  1999/11/03 14:04:57  warmerda
 * Made use of GetSubfieldData() with repeated fixed length
 * fields much more efficient.
 *
 * Revision 1.7  1999/09/21 16:25:32  warmerda
 * Fixed bug with repeating variable length fields and running out of data.
 *
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
 * A variety of information about this field, and all it's
 * subfields is written to the given debugging file handle.  Note that
 * field definition information (ala DDFFieldDefn) isn't written.
 *
 * @param fp The standard io file handle to write to.  ie. stderr
 */

void DDFField::Dump( FILE * fp )

{
    int         nMaxRepeat = 8;

    if( getenv("DDF_MAXDUMP") != NULL )
        nMaxRepeat = atoi(getenv("DDF_MAXDUMP"));

    fprintf( fp, "  DDFField:\n" );
    fprintf( fp, "      Tag = `%s'\n", poDefn->GetName() );
    fprintf( fp, "      DataSize = %d\n", nDataSize );

    fprintf( fp, "      Data = `" );
    for( int i = 0; i < MIN(nDataSize,40); i++ )
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
    int         iOffset = 0, nLoopCount;

    for( nLoopCount = 0; nLoopCount < GetRepeatCount(); nLoopCount++ )
    {
        if( nLoopCount > nMaxRepeat )
        {
            fprintf( fp, "      ...\n" );
            break;
        }
        
        for( int i = 0; i < poDefn->GetSubfieldCount(); i++ )
        {
            int         nBytesConsumed;

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
    int         iOffset = 0;
    
    if( poSFDefn == NULL )
        return NULL;

    if( iSubfieldIndex > 0 && poDefn->GetFixedWidth() > 0 )
    {
        iOffset = poDefn->GetFixedWidth() * iSubfieldIndex;
        iSubfieldIndex = 0;
    }

    while( iSubfieldIndex >= 0 )
    {
        for( int iSF = 0; iSF < poDefn->GetSubfieldCount(); iSF++ )
        {
            int nBytesConsumed;
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
    int         iOffset = 0, iRepeatCount = 1;
    
    while( TRUE )
    {
        for( int iSF = 0; iSF < poDefn->GetSubfieldCount(); iSF++ )
        {
            int nBytesConsumed;
            DDFSubfieldDefn * poThisSFDefn = poDefn->GetSubfield( iSF );

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
    const char *pachWrkData;

    if( nInstance < 0 || nInstance >= nRepeatCount )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Special case for fields without subfields (like "0001").  We    */
/*      don't currently handle repeating simple fields.                 */
/* -------------------------------------------------------------------- */
    if( poDefn->GetSubfieldCount() == 0 )
    {
        pachWrkData = GetData();
        if( pnInstanceSize != 0 )
            *pnInstanceSize = GetDataSize();
        return pachWrkData;
    }

/* -------------------------------------------------------------------- */
/*      Get a pointer to the start of the existing data for this        */
/*      iteration of the field.                                         */
/* -------------------------------------------------------------------- */
    int         nBytesRemaining1, nBytesRemaining2;
    DDFSubfieldDefn *poFirstSubfield;

    poFirstSubfield = poDefn->GetSubfield(0);

    pachWrkData = GetSubfieldData(poFirstSubfield, &nBytesRemaining1,
                               nInstance);

/* -------------------------------------------------------------------- */
/*      Figure out the size of the entire field instance, including     */
/*      unit terminators, but not any trailing field terminator.        */
/* -------------------------------------------------------------------- */
    if( pnInstanceSize != NULL )
    {
        DDFSubfieldDefn *poLastSubfield;
        int              nLastSubfieldWidth;
        const char          *pachLastData;
        
        poLastSubfield = poDefn->GetSubfield(poDefn->GetSubfieldCount()-1);
        
        pachLastData = GetSubfieldData( poLastSubfield, &nBytesRemaining2, 
                                        nInstance );
        poLastSubfield->GetDataLength( pachLastData, nBytesRemaining2, 
                                       &nLastSubfieldWidth );
        
        *pnInstanceSize = 
            nBytesRemaining1 - (nBytesRemaining2 - nLastSubfieldWidth);
    }

    return pachWrkData;
}
