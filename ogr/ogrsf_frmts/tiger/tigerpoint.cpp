/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPoint class.
 * Author:   Mark Phillips, mbp@geomtech.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Mark Phillips
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                             TigerPoint()                             */
/************************************************************************/
TigerPoint::TigerPoint( const TigerRecordInfo *psRTInfoIn,
                        const char *m_pszFileCodeIn ) :
    TigerFileBase(psRTInfoIn, m_pszFileCodeIn)
{}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/
OGRFeature *TigerPoint::GetFeature( int nRecordId,
                                    int nX0, int nX1,
                                    int nY0, int nY1 )
{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures ) {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sP",
                  nRecordId, pszModule );
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the raw record data from the file.                         */
    /* -------------------------------------------------------------------- */

    if( fpPrimary == nullptr )
        return nullptr;

    if( VSIFSeekL( fpPrimary, nRecordId * nRecordLength, SEEK_SET ) != 0 ) {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to %d of %sP",
                  nRecordId * nRecordLength, pszModule );
        return nullptr;
    }

    // Overflow cannot happen since psRTInfo->nRecordLength is unsigned
    // char and sizeof(achRecord) == OGR_TIGER_RECBUF_LEN > 255
    if( VSIFReadL( achRecord, psRTInfo->nRecordLength, 1, fpPrimary ) != 1 ) {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sP",
                  nRecordId, pszModule );
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRTInfo, poFeature, achRecord);

    /* -------------------------------------------------------------------- */
    /*      Set geometry                                                    */
    /* -------------------------------------------------------------------- */

    const double dfX = atoi(GetField(achRecord, nX0, nX1)) / 1000000.0;
    const double dfY = atoi(GetField(achRecord, nY0, nY1)) / 1000000.0;

    if( dfX != 0.0 || dfY != 0.0 ) {
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY ) );
    }

    return poFeature;
}
