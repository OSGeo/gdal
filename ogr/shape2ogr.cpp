/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements translation of Shapefile shapes into OGR
 *           representation.
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
 * Revision 1.1  1999/05/31 19:26:03  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "../frmts/shapelib/shapefil.h"

/************************************************************************/
/*                          SHPReadOGRObject()                          */
/*                                                                      */
/*      Read an item in a shapefile, and translate to OGR geometry      */
/*      representation.                                                 */
/************************************************************************/

OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape )

{
    SHPObject	*psShape;
    OGRGeometry *poOGR = NULL;

    psShape = SHPReadObject( hSHP, iShape );

    if( psShape == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Point.                                                          */
/* -------------------------------------------------------------------- */
    else if( psShape->nSHPType == SHPT_POINT
             || psShape->nSHPType == SHPT_POINTM
             || psShape->nSHPType == SHPT_POINTZ )
    {
        poOGR = new OGRPoint( psShape->padfX[0], psShape->padfY[0] );
    }

/* -------------------------------------------------------------------- */
/*      Arc (LineString)                                                */
/* -------------------------------------------------------------------- */
    else if( psShape->nSHPType == SHPT_ARC
             || psShape->nSHPType == SHPT_ARCM
             || psShape->nSHPType == SHPT_ARCZ )
    {
        OGRLineString *poOGRLine = new OGRLineString();

        poOGRLine->setPoints( psShape->nVertices,
                              psShape->padfX, psShape->padfY );
        
        poOGR = poOGRLine;
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/*                                                                      */
/*      For now we assume the first ring is an outer ring, and          */
/*      everything else is an inner ring.  This must smarten up in      */
/*      the future.                                                     */
/* -------------------------------------------------------------------- */
    else if( psShape->nSHPType == SHPT_POLYGON
             || psShape->nSHPType == SHPT_POLYGONM
             || psShape->nSHPType == SHPT_POLYGONZ )
    {
        int	iRing;
        OGRPolygon *poOGRPoly;
        
        poOGR = poOGRPoly = new OGRPolygon();

        for( iRing = 0; iRing < psShape->nParts; iRing++ )
        {
            OGRLinearRing	*poRing;
            int	nRingPoints;
            int	nRingStart;

            poRing = new OGRLinearRing();

            if( psShape->panPartStart == NULL )
            {
                nRingPoints = psShape->nVertices;
                nRingStart = 0;
            }
            else
            {

                if( iRing == psShape->nParts - 1 )
                    nRingPoints =
                        psShape->nVertices - psShape->panPartStart[iRing];
                else
                    nRingPoints = psShape->panPartStart[iRing+1]
                        - psShape->panPartStart[iRing];
                nRingStart = psShape->panPartStart[iRing];
            }
            
            poRing->setPoints( nRingPoints, 
                               psShape->padfX + nRingStart,
                               psShape->padfY + nRingStart );

            poOGRPoly->addRing( poRing );

            delete poRing;
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise for now we just ignore the object.  Eventually we     */
/*      should implement multipoints, and perhaps do something with     */
/*      multipatch.                                                     */
/* -------------------------------------------------------------------- */
    else
    {
        /* nothing returned */
    }
    
    SHPDestroyObject( psShape );

    return poOGR;
}
