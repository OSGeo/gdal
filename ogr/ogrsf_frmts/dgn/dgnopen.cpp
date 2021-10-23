/******************************************************************************
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  DGN Access Library file open code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Avenza Systems Inc, http://www.avenza.com/
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

#include "dgnlibp.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            DGNTestOpen()                             */
/************************************************************************/

/**
 * Test if header is DGN.
 *
 * @param pabyHeader block of header data from beginning of file.
 * @param nByteCount number of bytes in pabyHeader.
 *
 * @return TRUE if the header appears to be from a DGN file, otherwise FALSE.
 */

int DGNTestOpen( GByte *pabyHeader, int nByteCount )

{
    if( nByteCount < 4 )
        return FALSE;

    // Is it a cell library?
    if( pabyHeader[0] == 0x08
        && pabyHeader[1] == 0x05
        && pabyHeader[2] == 0x17
        && pabyHeader[3] == 0x00 )
        return TRUE;

    // Is it not a regular 2D or 3D file?
    if( (pabyHeader[0] != 0x08 && pabyHeader[0] != 0xC8)
        || pabyHeader[1] != 0x09
        || pabyHeader[2] != 0xFE || pabyHeader[3] != 0x02 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                              DGNOpen()                               */
/************************************************************************/

/**
 * Open a DGN file.
 *
 * The file is opened, and minimally verified to ensure it is a DGN (ISFF)
 * file.  If the file cannot be opened for read access an error with code
 * CPLE_OpenFailed with be reported via CPLError() and NULL returned.
 * If the file header does
 * not appear to be a DGN file, an error with code CPLE_AppDefined will be
 * reported via CPLError(), and NULL returned.
 *
 * If successful a handle for further access is returned.  This should be
 * closed with DGNClose() when no longer needed.
 *
 * DGNOpen() does not scan the file on open, and should be very fast even for
 * large files.
 *
 * @param pszFilename name of file to try opening.
 * @param bUpdate should the file be opened with read+update (r+) mode?
 *
 * @return handle to use for further access to file using DGN API, or NULL
 * if open fails.
 */

DGNHandle DGNOpen( const char * pszFilename, int bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, bUpdate ? "rb+" : "rb");
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open `%s' for read access.\n",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Verify the format ... add later.                                */
/* -------------------------------------------------------------------- */
    GByte abyHeader[512];
    const int nHeaderBytes =
        static_cast<int>(VSIFReadL( abyHeader, 1, sizeof(abyHeader), fp ));
    if( !DGNTestOpen( abyHeader, nHeaderBytes ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File `%s' does not have expected DGN header.\n",
                  pszFilename );
        VSIFCloseL( fp );
        return nullptr;
    }

    VSIRewindL( fp );

/* -------------------------------------------------------------------- */
/*      Create the info structure.                                      */
/* -------------------------------------------------------------------- */
    DGNInfo *psDGN = static_cast<DGNInfo *>(CPLCalloc(sizeof(DGNInfo), 1));
    psDGN->fp = fp;
    psDGN->next_element_id = 0;

    psDGN->got_tcb = false;
    psDGN->scale = 1.0;
    psDGN->origin_x = 0.0;
    psDGN->origin_y = 0.0;
    psDGN->origin_z = 0.0;

    psDGN->index_built = false;
    psDGN->element_count = 0;
    psDGN->element_index = nullptr;

    psDGN->got_bounds = false;

    if( abyHeader[0] == 0xC8 )
        psDGN->dimension = 3;
    else
        psDGN->dimension = 2;

    psDGN->has_spatial_filter = false;
    psDGN->sf_converted_to_uor = false;
    psDGN->select_complex_group = false;
    psDGN->in_complex_group = false;

    return (DGNHandle) psDGN;
}

/************************************************************************/
/*                           DGNSetOptions()                            */
/************************************************************************/

/**
 * Set file access options.
 *
 * Sets a flag affecting how the file is accessed.  Currently
 * there is only one support flag:
 *
 * DGNO_CAPTURE_RAW_DATA: If this is enabled (it is off by default),
 * then the raw binary data associated with elements will be kept in
 * the raw_data field within the DGNElemCore when they are read.  This
 * is required if the application needs to interpret the raw data itself.
 * It is also necessary if the element is to be written back to this file,
 * or another file using DGNWriteElement().  Off by default (to conserve
 * memory).
 *
 * @param hDGN handle to file returned by DGNOpen().
 * @param nOptions ORed option flags.
 */

void DGNSetOptions( DGNHandle hDGN, int nOptions )

{
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

    psDGN->options = nOptions;
}

/************************************************************************/
/*                        DGNSetSpatialFilter()                         */
/************************************************************************/

/**
 * Set rectangle for which features are desired.
 *
 * If a spatial filter is set with this function, DGNReadElement() will
 * only return spatial elements (elements with a known bounding box) and
 * only those elements for which this bounding box overlaps the requested
 * region.
 *
 * If all four values (dfXMin, dfXMax, dfYMin and dfYMax) are zero, the
 * spatial filter is disabled.   Note that installing a spatial filter
 * won't reduce the amount of data read from disk.  All elements are still
 * scanned, but the amount of processing work for elements outside the
 * spatial filter is minimized.
 *
 * @param hDGN Handle from DGNOpen() for file to update.
 * @param dfXMin minimum x coordinate for extents (georeferenced coordinates).
 * @param dfYMin minimum y coordinate for extents (georeferenced coordinates).
 * @param dfXMax maximum x coordinate for extents (georeferenced coordinates).
 * @param dfYMax maximum y coordinate for extents (georeferenced coordinates).
 */

void DGNSetSpatialFilter( DGNHandle hDGN,
                          double dfXMin, double dfYMin,
                          double dfXMax, double dfYMax )

{
    DGNInfo *psDGN = (DGNInfo *) hDGN;

    if( dfXMin == 0.0 && dfXMax == 0.0
        && dfYMin == 0.0 && dfYMax == 0.0 )
    {
        psDGN->has_spatial_filter = false;
        return;
    }

    psDGN->has_spatial_filter = true;
    psDGN->sf_converted_to_uor = false;

    psDGN->sf_min_x_geo = dfXMin;
    psDGN->sf_min_y_geo = dfYMin;
    psDGN->sf_max_x_geo = dfXMax;
    psDGN->sf_max_y_geo = dfYMax;

    DGNSpatialFilterToUOR( psDGN );
}

/************************************************************************/
/*                       DGNSpatialFilterToUOR()                        */
/************************************************************************/

void DGNSpatialFilterToUOR( DGNInfo *psDGN )

{
    if( psDGN->sf_converted_to_uor
        || !psDGN->has_spatial_filter
        || !psDGN->got_tcb )
        return;

    DGNPoint sMin = {
        psDGN->sf_min_x_geo,
        psDGN->sf_min_y_geo,
        0
    };

    DGNPoint sMax = {
        psDGN->sf_max_x_geo,
        psDGN->sf_max_y_geo,
        0
    };

    DGNInverseTransformPoint( psDGN, &sMin );
    DGNInverseTransformPoint( psDGN, &sMax );

    psDGN->sf_min_x = (GUInt32) (sMin.x + 2147483648.0);
    psDGN->sf_min_y = (GUInt32) (sMin.y + 2147483648.0);
    psDGN->sf_max_x = (GUInt32) (sMax.x + 2147483648.0);
    psDGN->sf_max_y = (GUInt32) (sMax.y + 2147483648.0);

    psDGN->sf_converted_to_uor = true;
}

/************************************************************************/
/*                              DGNClose()                              */
/************************************************************************/

/**
 * Close DGN file.
 *
 * @param hDGN Handle from DGNOpen() for file to close.
 */

void DGNClose( DGNHandle hDGN )

{
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

    VSIFCloseL( psDGN->fp );
    CPLFree( psDGN->element_index );
    CPLFree( psDGN );
}

/************************************************************************/
/*                          DGNGetDimension()                           */
/************************************************************************/

/**
 * Return 2D/3D dimension of file.
 *
 * Return 2 or 3 depending on the dimension value of the provided file.
 */

int DGNGetDimension( DGNHandle hDGN )

{
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

    return psDGN->dimension;
}
