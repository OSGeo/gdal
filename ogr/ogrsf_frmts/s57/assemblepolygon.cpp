/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements polygon assembly for S57Reader.  This code
 *           should eventually move into the generic OGR directory, and the
 *           prototype into ogr_geometry.h.
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
 * Revision 1.2  1999/11/10 14:17:26  warmerda
 * Fixed defaulting of peErr parameter.
 *
 * Revision 1.1  1999/11/04 21:18:46  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "cpl_conv.h"

OGRPolygon *OGRBuildPolygonFromEdges( OGRGeometryCollection * poLines,
                                      int bBestEffort, OGRErr * peErr = NULL );

/************************************************************************/
/*                           AddEdgeToRing()                            */
/************************************************************************/

static void AddEdgeToRing( OGRLinearRing * poRing, OGRLineString * poLine,
                           int bReverse, int bDropVertex )

{
/* -------------------------------------------------------------------- */
/*      Establish order and range of traverse.                          */
/* -------------------------------------------------------------------- */
    int		iStart, iEnd, iStep;
    int		nVertToAdd = poLine->getNumPoints();

    if( bDropVertex && bReverse )
    {
        iStart = nVertToAdd - 2;
        iEnd = 0;
        iStep = -1;
    }
    else if( bDropVertex && !bReverse )
    {
        iStart = 1;
        iEnd = nVertToAdd - 1;
        iStep = 1;
    }
    else if( !bDropVertex && !bReverse )
    {
        iStart = 0;
        iEnd = nVertToAdd - 1;
        iStep = 1;
    }
    else if( !bDropVertex && bReverse )
    {
        iStart = nVertToAdd - 1;
        iEnd = 0;
        iStep = -1;
    }

/* -------------------------------------------------------------------- */
/*      Append points to ring.                                          */
/* -------------------------------------------------------------------- */
    int		iOutVertex = poRing->getNumPoints();

    poRing->setNumPoints( iOutVertex + ABS(iEnd-iStart) + 1 );
    
    for( int i = iStart; i != (iEnd+iStep); i += iStep )
    {
        poRing->setPoint( iOutVertex++,
                          poLine->getX(i), poLine->getY(i), poLine->getZ(i) );
    }
}

/************************************************************************/
/*                             PointEqual()                             */
/*                                                                      */
/*      Compare points on two linear strings.                           */
/************************************************************************/

static int PointsEqual( OGRLineString *poLine1, int iPoint1,
                        OGRLineString *poLine2, int iPoint2 )

{
    if( poLine1->getX(iPoint1) == poLine2->getX(iPoint2)
        && poLine1->getY(iPoint1) == poLine2->getY(iPoint2) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                      OGRBuildPolygonFromEdges()                      */
/************************************************************************/

OGRPolygon *OGRBuildPolygonFromEdges( OGRGeometryCollection * poLines,
                                      int bBestEffort, OGRErr * peErr )

{
    int		bSuccess = TRUE;
    OGRPolygon  *poPolygon = new OGRPolygon();
    
/* -------------------------------------------------------------------- */
/*      Setup array of line markers indicating if they have been        */
/*      added to a ring yet.                                            */
/* -------------------------------------------------------------------- */
    int		nEdges = poLines->getNumGeometries();
    int		*panEdgeConsumed, nRemainingEdges = nEdges;

    panEdgeConsumed = (int *) CPLCalloc(sizeof(int),nEdges);

/* ==================================================================== */
/*      Loop generating rings.                                          */
/* ==================================================================== */
    while( nRemainingEdges > 0 )
    {
        int		iEdge;
        OGRLineString	*poLine;
        
/* -------------------------------------------------------------------- */
/*      Find the first unconsumed edge.                                 */
/* -------------------------------------------------------------------- */
        for( iEdge = 0; panEdgeConsumed[iEdge]; iEdge++ ) {}

        poLine = (OGRLineString *) poLines->getGeometryRef(iEdge);

/* -------------------------------------------------------------------- */
/*      Start a new ring, copying in the current line directly          */
/* -------------------------------------------------------------------- */
        OGRLinearRing	*poRing = new OGRLinearRing();
        
        AddEdgeToRing( poRing, poLine,
                       FALSE, FALSE );

        panEdgeConsumed[iEdge] = TRUE;
        nRemainingEdges--;
        
/* ==================================================================== */
/*      Loop adding edges to this ring until we make a whole pass       */
/*      within finding anything to add.                                 */
/* ==================================================================== */
        int		bWorkDone = TRUE;

        while( !PointsEqual(poRing,0,poRing,poRing->getNumPoints()-1)
               && nRemainingEdges > 0
               && bWorkDone )
        {
            bWorkDone = FALSE;
            
            for( iEdge = 0; iEdge < nEdges; iEdge++ )
            {
                if( panEdgeConsumed[iEdge] )
                    continue;

                poLine = (OGRLineString *) poLines->getGeometryRef(iEdge);
                
                if( PointsEqual(poLine,0,poRing,poRing->getNumPoints()-1) )
                {
                    AddEdgeToRing( poRing, poLine, FALSE, TRUE );
                }
                else if( PointsEqual(poLine,poLine->getNumPoints()-1,
                                     poRing,poRing->getNumPoints()-1) )
                {
                    AddEdgeToRing( poRing, poLine, TRUE, TRUE );
                }
                else
                {
                    continue;
                }
                    
                panEdgeConsumed[iEdge] = TRUE;
                nRemainingEdges--;
                bWorkDone = TRUE;
            }
        }

/* -------------------------------------------------------------------- */
/*      Did we fail to complete the ring?                               */
/* -------------------------------------------------------------------- */
        if( !PointsEqual(poRing,0,poRing,poRing->getNumPoints()-1) )
            bSuccess = FALSE;

        poPolygon->addRingDirectly( poRing );
    } /* next ring */

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CPLFree( panEdgeConsumed );

// Eventually we should at least identify the external ring properly,
// perhaps even ordering the direction of rings, though this isn't
// required by the OGC geometry model.

    return poPolygon;
}

