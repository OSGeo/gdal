/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  DGN Access functions related to writing DGN elements.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.1  2002/03/14 21:40:43  warmerda
 * New
 *
 */

#include "dgnlibp.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          DGNResizeElement()                          */
/************************************************************************/

int DGNResizeElement( DGNHandle hDGN, DGNElemCore *psElement, int nNewSize )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

/* -------------------------------------------------------------------- */
/*      Check various conditions.                                       */
/* -------------------------------------------------------------------- */
    if( psElement->raw_bytes == 0 
        || psElement->raw_bytes != psElement->size )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Raw bytes not loaded, or not matching element size." );
        return FALSE;
    }

    if( nNewSize % 2 == 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "DGNResizeElement(%d): "
                  "can't change to odd (not divisible by two) size.", 
                  nNewSize );
        return FALSE;
    }

    if( nNewSize == psElement->raw_bytes )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Mark the existing element as deleted if the element has to      */
/*      move to the end of the file.                                    */
/* -------------------------------------------------------------------- */

    if( psElement->offset != -1 )
    {
        int nOldFLoc = VSIFTell( psDGN->fp );
        unsigned char abyLeader[2];
        
        if( VSIFSeek( psDGN->fp, psElement->offset, SEEK_SET ) != 0
            || VSIFRead( abyLeader, sizeof(abyLeader), 1, psDGN->fp ) != 1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed seek or read when trying to mark existing\n"
                      "element as deleted in DGNResizeElement()\n" );
            return FALSE;
        }

        abyLeader[1] |= 0x80;
        
        if( VSIFSeek( psDGN->fp, psElement->offset, SEEK_SET ) != 0
            || VSIFWrite( abyLeader, sizeof(abyLeader), 1, psDGN->fp ) != 1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed seek or write when trying to mark existing\n"
                      "element as deleted in DGNResizeElement()\n" );
            return FALSE;
        }

        VSIFSeek( psDGN->fp, SEEK_SET, nOldFLoc );

        if( psElement->element_id != -1 && psDGN->index_built )
            psDGN->element_index[psElement->element_id].flags 
                |= DGNEIF_DELETED;
    }

    psElement->offset = -1; /* move to end of file. */
    psElement->element_id = -1;

/* -------------------------------------------------------------------- */
/*      Set the new size information, and realloc the raw data buffer.  */
/* -------------------------------------------------------------------- */
    psElement->size = nNewSize;
    psElement->raw_data = (unsigned char *) 
        CPLRealloc( psElement->raw_data, nNewSize );
    psElement->raw_bytes = nNewSize;

/* -------------------------------------------------------------------- */
/*      Update the size information within the raw buffer.              */
/* -------------------------------------------------------------------- */
    int nWords = (nNewSize / 2) - 2;

    psElement->raw_data[2] = nWords % 256;
    psElement->raw_data[3] = nWords / 256;

    return TRUE;
}

/************************************************************************/
/*                          DGNWriteElement()                           */
/************************************************************************/

int DGNWriteElement( DGNHandle hDGN, DGNElemCore *psElement )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

/* ==================================================================== */
/*      If this element hasn't been positioned yet, place it at the     */
/*      end of the file.                                                */
/* ==================================================================== */
    if( psElement->offset == -1 )
    {
        int nJunk;

        // We must have an index, in order to properly assign the 
        // element id of the newly written element.  Ensure it is built.
        if( !psDGN->index_built )
            DGNBuildIndex( psDGN );

        // Read the current "last" element.
        if( !DGNGotoElement( hDGN, psDGN->element_count-1 ) )
            return FALSE;

        if( !DGNLoadRawElement( psDGN, &nJunk, &nJunk ) )
            return FALSE;

        // Establish the position of the new element.
        psElement->offset = VSIFTell( psDGN->fp );
        psElement->element_id = psDGN->element_count;

        // Grow element buffer if needed.
        if( psDGN->element_count == psDGN->max_element_count )
        {
            psDGN->max_element_count += 500;
            
            psDGN->element_index = (DGNElementInfo *) 
                CPLRealloc( psDGN->element_index, 
                            psDGN->max_element_count * sizeof(DGNElementInfo));
        }

        // Set up the element info
        DGNElementInfo *psInfo;
        
        psInfo = psDGN->element_index + psDGN->element_count;
        psInfo->level = psElement->level;
        psInfo->type = psElement->type;
        psInfo->stype = psElement->stype;
        psInfo->offset = psElement->offset;
        if( psElement->complex )
            psInfo->flags = DGNEIF_COMPLEX;
        else
            psInfo->flags = 0;
    
        psDGN->element_count++;
    }

/* -------------------------------------------------------------------- */
/*      Write out the element.                                          */
/* -------------------------------------------------------------------- */
    if( VSIFSeek( psDGN->fp, psElement->offset, SEEK_SET ) != 0 
        || VSIFWrite( psElement->raw_data, psElement->raw_bytes, 
                      1, psDGN->fp) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error seeking or writing new element of %d bytes at %d.",
                  psElement->offset, 
                  psElement->raw_bytes );
        return FALSE;
    }

    psDGN->next_element_id = psElement->element_id + 1;

/* -------------------------------------------------------------------- */
/*      Write out the end of file 0xffff marker (if we were             */
/*      extending the file), but push the file pointer back before      */
/*      this EOF when done.                                             */
/* -------------------------------------------------------------------- */
    if( psDGN->next_element_id == psDGN->element_count )
    {
        unsigned char abyEOF[2];

        abyEOF[0] = 0xff;
        abyEOF[1] = 0xff;

        VSIFWrite( abyEOF, 2, 1, psDGN->fp );
        VSIFSeek( psDGN->fp, -2, SEEK_CUR );
    }
        
    return TRUE;
}
    
