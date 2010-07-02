/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSPolygonReader and SDTSRawPolygon classes.
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

#include "sdts_al.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                            SDTSRawPolygon                            */
/*                                                                      */
/*      This is a simple class for holding the data related with a      */
/*      polygon feature.                                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSRawPolygon()                           */
/************************************************************************/

SDTSRawPolygon::SDTSRawPolygon()

{
    nAttributes = 0;
    nEdges = nRings = nVertices = 0;
    papoEdges = NULL;
    
    panRingStart = NULL;
    padfX = padfY = padfZ = NULL;
}

/************************************************************************/
/*                          ~SDTSRawPolygon()                           */
/************************************************************************/

SDTSRawPolygon::~SDTSRawPolygon()

{
    CPLFree( papoEdges );
    CPLFree( panRingStart );
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record from the passed SDTSPolygonReader, and assign the */
/*      values from that record to this object.  This is the bulk of    */
/*      the work in this whole file.                                    */
/************************************************************************/

int SDTSRawPolygon::Read( DDFRecord * poRecord )

{
/* ==================================================================== */
/*      Loop over fields in this record, looking for those we           */
/*      recognise, and need.                                            */
/* ==================================================================== */
    for( int iField = 0; iField < poRecord->GetFieldCount(); iField++ )
    {
        DDFField        *poField = poRecord->GetField( iField );
        const char      *pszFieldName;

        CPLAssert( poField != NULL );
        pszFieldName = poField->GetFieldDefn()->GetName();

        if( EQUAL(pszFieldName,"POLY") )
        {
            oModId.Set( poField );
        }

        else if( EQUAL(pszFieldName,"ATID") )
        {
            ApplyATID( poField );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                              AddEdge()                               */
/************************************************************************/

void SDTSRawPolygon::AddEdge( SDTSRawLine * poNewLine )

{
    nEdges++;
    
    papoEdges = (SDTSRawLine **) CPLRealloc(papoEdges, sizeof(void*)*nEdges );
    papoEdges[nEdges-1] = poNewLine;
}

/************************************************************************/
/*                           AddEdgeToRing()                            */
/************************************************************************/

void SDTSRawPolygon::AddEdgeToRing( int nVertToAdd,
                                    double * padfXToAdd,
                                    double * padfYToAdd,
                                    double * padfZToAdd,
                                    int bReverse, int bDropVertex )

{
    int         iStart=0, iEnd=nVertToAdd-1, iStep=1;

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

    for( int i = iStart; i != (iEnd+iStep); i += iStep )
    {
        padfX[nVertices] = padfXToAdd[i];
        padfY[nVertices] = padfYToAdd[i];
        padfZ[nVertices] = padfZToAdd[i];

        nVertices++;
    }
}

/************************************************************************/
/*                           AssembleRings()                            */
/************************************************************************/

/**
 * Form border lines (arcs) into outer and inner rings.
 *
 * See SDTSPolygonReader::AssemblePolygons() for a simple one step process
 * to assembling geometry for all polygons in a transfer.
 *
 * This method will assemble the lines attached to a polygon into
 * an outer ring, and zero or more inner rings.  Before calling it is
 * necessary that all the lines associated with this polygon have already
 * been attached.  Normally this is accomplished by calling
 * SDTSLineReader::AttachToPolygons() on all line layers that might
 * contain edges related to this layer.
 *
 * This method then forms the lines into rings.  Rings are formed by:
 * <ol>
 * <li> Take a previously unconsumed line, and start a ring with it.  Mark
 *      it as consumed, and keep track of it's start and end node ids as
 *      being the start and end node ids of the ring.
 * <li> If the rings start id is the same as the end node id then this ring
 *      is completely formed, return to step 1.
 * <li> Search all unconsumed lines for a line with the same start or end
 *      node id as the rings current node id.  If none are found then the
 *      assembly has failed.  Return to step 1 but report failure on
 *      completion.
 * <li> Once found, add the line to the current ring, dropping the duplicated
 *      vertex and reverse order if necessary.  Mark the line as consumed,
 *      and update the rings end node id accordingly.
 * <li> go to step 2.
 * </ol>
 *
 * Once ring assembly from lines is complete, another pass is made to
 * order the rings such that the exterior ring is first, the first ring
 * has counter-clockwise vertex ordering and the inner rings have clockwise
 * vertex ordering.  This is accomplished based on the assumption that the
 * outer ring has the largest area, and using the +/- sign of area to establish
 * direction of rings.
 *
 * @return TRUE if all rings assembled without problems or FALSE if a problem
 * occured.  If a problem occurs rings are still formed from all lines, but
 * some of the rings will not be closed, and rings will have no particular
 * order or direction.
 */

int SDTSRawPolygon::AssembleRings()

{
    int         iEdge;
    int         bSuccess = TRUE;
    
    if( nRings > 0 )
        return TRUE;

    if( nEdges == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Allocate ring arrays.                                           */
/* -------------------------------------------------------------------- */
    panRingStart = (int *) CPLMalloc(sizeof(int) * nEdges);

    nVertices = 0;
    for( iEdge = 0; iEdge < nEdges; iEdge++ )
    {
        nVertices += papoEdges[iEdge]->nVertices;
    }

    padfX = (double *) CPLMalloc(sizeof(double) * nVertices);
    padfY = (double *) CPLMalloc(sizeof(double) * nVertices);
    padfZ = (double *) CPLMalloc(sizeof(double) * nVertices);

    nVertices = 0;

/* -------------------------------------------------------------------- */
/*      Setup array of line markers indicating if they have been        */
/*      added to a ring yet.                                            */
/* -------------------------------------------------------------------- */
    int *panEdgeConsumed, nRemainingEdges = nEdges;

    panEdgeConsumed = (int *) CPLCalloc(sizeof(int),nEdges);

/* ==================================================================== */
/*      Loop generating rings.                                          */
/* ==================================================================== */
    while( nRemainingEdges > 0 )
    {
        int             nStartNode, nLinkNode;
        
/* -------------------------------------------------------------------- */
/*      Find the first unconsumed edge.                                 */
/* -------------------------------------------------------------------- */
        SDTSRawLine     *poEdge;
        
        for( iEdge = 0; panEdgeConsumed[iEdge]; iEdge++ ) {}

        poEdge = papoEdges[iEdge];

/* -------------------------------------------------------------------- */
/*      Start a new ring, copying in the current line directly          */
/* -------------------------------------------------------------------- */
        panRingStart[nRings++] = nVertices;

        AddEdgeToRing( poEdge->nVertices,
                       poEdge->padfX, poEdge->padfY, poEdge->padfZ,
                       FALSE, FALSE );

        panEdgeConsumed[iEdge] = TRUE;
        nRemainingEdges--;
        
        nStartNode = poEdge->oStartNode.nRecord;
        nLinkNode = poEdge->oEndNode.nRecord;

/* ==================================================================== */
/*      Loop adding edges to this ring until we make a whole pass       */
/*      within finding anything to add.                                 */
/* ==================================================================== */
        int             bWorkDone = TRUE;

        while( nLinkNode != nStartNode
               && nRemainingEdges > 0
               && bWorkDone )
        {
            bWorkDone = FALSE;
            
            for( iEdge = 0; iEdge < nEdges; iEdge++ )
            {
                if( panEdgeConsumed[iEdge] )
                    continue;

                poEdge = papoEdges[iEdge];
                if( poEdge->oStartNode.nRecord == nLinkNode )
                {
                    AddEdgeToRing( poEdge->nVertices,
                                   poEdge->padfX, poEdge->padfY, poEdge->padfZ,
                                   FALSE, TRUE );
                    nLinkNode = poEdge->oEndNode.nRecord;
                }
                else if( poEdge->oEndNode.nRecord == nLinkNode )
                {
                    AddEdgeToRing( poEdge->nVertices,
                                   poEdge->padfX, poEdge->padfY, poEdge->padfZ,
                                   TRUE, TRUE );
                    nLinkNode = poEdge->oStartNode.nRecord;
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
        if( nLinkNode != nStartNode )
            bSuccess = FALSE;
        
    } /* next ring */

    CPLFree( panEdgeConsumed );

    if( !bSuccess )
        return bSuccess;

/* ==================================================================== */
/*      Compute the area of each ring.  The sign will be positive       */
/*      for counter clockwise rings, otherwise negative.                */
/*                                                                      */
/*      The algorithm used in this function was taken from _Graphics    */
/*      Gems II_, James Arvo, 1991, Academic Press, Inc., section 1.1,  */
/*      "The Area of a Simple Polygon", Jon Rokne, pp. 5-6.             */
/* ==================================================================== */
    double      *padfRingArea, dfMaxArea = 0.0;
    int         iRing, iBiggestRing = -1;

    padfRingArea = (double *) CPLCalloc(sizeof(double),nRings);

    for( iRing = 0; iRing < nRings; iRing++ )
    {
        double  dfSum1 = 0.0, dfSum2 = 0.0;
        int     i, nRingVertices;

        if( iRing == nRings - 1 )
            nRingVertices = nVertices - panRingStart[iRing];
        else
            nRingVertices = panRingStart[iRing+1] - panRingStart[iRing];
        
        for( i = panRingStart[iRing];
             i < panRingStart[iRing] + nRingVertices - 1;
             i++)
        {
            dfSum1 += padfX[i] * padfY[i+1];
            dfSum2 += padfY[i] * padfX[i+1];
        }

        padfRingArea[iRing] = (dfSum1 - dfSum2) / 2;

        if( ABS(padfRingArea[iRing]) > dfMaxArea )
        {
            dfMaxArea = ABS(padfRingArea[iRing]);
            iBiggestRing = iRing;
        }
    }

/* ==================================================================== */
/*      Make a new set of vertices, and copy the largest ring into      */
/*      it, adjusting the direction if necessary to ensure that this    */
/*      outer ring is counter clockwise.                                */
/* ==================================================================== */
    double      *padfXRaw = padfX;
    double      *padfYRaw = padfY;
    double      *padfZRaw = padfZ;
    int         *panRawRingStart = panRingStart;
    int         nRawVertices = nVertices;
    int         nRawRings = nRings;
    int         nRingVertices;

    padfX = (double *) CPLMalloc(sizeof(double) * nVertices);
    padfY = (double *) CPLMalloc(sizeof(double) * nVertices);
    padfZ = (double *) CPLMalloc(sizeof(double) * nVertices);
    panRingStart = (int *) CPLMalloc(sizeof(int) * nRawRings);
    nVertices = 0;
    nRings = 0;

    if( iBiggestRing == nRawRings - 1 )
        nRingVertices = nRawVertices - panRawRingStart[iBiggestRing];
    else
        nRingVertices =
            panRawRingStart[iBiggestRing+1] - panRawRingStart[iBiggestRing];

    panRingStart[nRings++] = 0;
    AddEdgeToRing( nRingVertices,
                   padfXRaw + panRawRingStart[iBiggestRing],
                   padfYRaw + panRawRingStart[iBiggestRing],
                   padfZRaw + panRawRingStart[iBiggestRing],
                   padfRingArea[iBiggestRing] < 0.0, FALSE );

/* ==================================================================== */
/*      Add the rest of the rings, which must be holes, in clockwise    */
/*      order.                                                          */
/* ==================================================================== */
    for( iRing = 0; iRing < nRawRings; iRing++ )
    {
        if( iRing == iBiggestRing )
            continue;
        
        if( iRing == nRawRings - 1 )
            nRingVertices = nRawVertices - panRawRingStart[iRing];
        else
            nRingVertices = panRawRingStart[iRing+1] - panRawRingStart[iRing];

        panRingStart[nRings++] = nVertices;
        AddEdgeToRing( nRingVertices,
                       padfXRaw + panRawRingStart[iRing],
                       padfYRaw + panRawRingStart[iRing],
                       padfZRaw + panRawRingStart[iRing],
                       padfRingArea[iRing] > 0.0, FALSE );
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( padfXRaw );
    CPLFree( padfYRaw );
    CPLFree( padfZRaw );
    CPLFree( padfRingArea );
    CPLFree( panRawRingStart );

    CPLFree( papoEdges );
    papoEdges = NULL;
    nEdges = 0;

    return TRUE;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void SDTSRawPolygon::Dump( FILE * fp )

{
    int         i;
    
    fprintf( fp, "SDTSRawPolygon %s: ", oModId.GetName() );

    for( i = 0; i < nAttributes; i++ )
        fprintf( fp, "  ATID[%d]=%s", i, paoATID[i].GetName() );

    fprintf( fp, "\n" );
}

/************************************************************************/
/* ==================================================================== */
/*                             SDTSPolygonReader                        */
/*                                                                      */
/*      This is the class used to read a Polygon module.                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSPolygonReader()                          */
/************************************************************************/

SDTSPolygonReader::SDTSPolygonReader()

{
    bRingsAssembled = FALSE;
}

/************************************************************************/
/*                             ~SDTSPolygonReader()                     */
/************************************************************************/

SDTSPolygonReader::~SDTSPolygonReader()
{
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSPolygonReader::Close()

{
    oDDFModule.Close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested line file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSPolygonReader::Open( const char * pszFilename )

{
    return( oDDFModule.Open( pszFilename ) );
}

/************************************************************************/
/*                            GetNextPolygon()                          */
/*                                                                      */
/*      Fetch the next feature as an STDSRawPolygon.                    */
/************************************************************************/

SDTSRawPolygon * SDTSPolygonReader::GetNextPolygon()

{
    DDFRecord   *poRecord;
    
/* -------------------------------------------------------------------- */
/*      Read a record.                                                  */
/* -------------------------------------------------------------------- */
    if( oDDFModule.GetFP() == NULL )
        return NULL;

    poRecord = oDDFModule.ReadRecord();

    if( poRecord == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Transform into a Polygon feature.                                 */
/* -------------------------------------------------------------------- */
    SDTSRawPolygon      *poRawPolygon = new SDTSRawPolygon();

    if( poRawPolygon->Read( poRecord ) )
    {
        return( poRawPolygon );
    }
    else
    {
        delete poRawPolygon;
        return NULL;
    }
}

/************************************************************************/
/*                           AssembleRings()                            */
/************************************************************************/

/**
 * Assemble geometry for a polygon transfer.
 *
 * This method takes care of attaching lines from all the line layers in
 * this transfer to this polygon layer, assembling the lines into rings on
 * the polygons, and then cleaning up unnecessary intermediate results.
 *
 * Currently this method will leave the line layers rewound to the beginning
 * but indexed, and the polygon layer rewound but indexed.  In the future
 * it may restore reading positions, and possibly flush line indexes if they
 * were not previously indexed.
 *
 * This method does nothing if the rings have already been assembled on
 * this layer using this method.
 *
 * See SDTSRawPolygon::AssembleRings() for more information on how the lines
 * are assembled into rings.
 *
 * @param poTransfer the SDTSTransfer that this reader is a part of.  Used
 * to get a list of line layers that might be needed.
 * @param iPolyLayer the polygon reader instance number, used to avoid processing
 * lines for other layers.
 */

void SDTSPolygonReader::AssembleRings( SDTSTransfer * poTransfer, 
                                       int iPolyLayer )

{
    if( bRingsAssembled )
        return;

    bRingsAssembled = TRUE;
    
/* -------------------------------------------------------------------- */
/*      To write polygons we need to build them from their related      */
/*      arcs.  We don't know off hand which arc (line) layers           */
/*      contribute so we process all line layers, attaching them to     */
/*      polygons as appropriate.                                        */
/* -------------------------------------------------------------------- */
    for( int iLineLayer = 0;
         iLineLayer < poTransfer->GetLayerCount();
         iLineLayer++ )
    {
        SDTSLineReader  *poLineReader;
        
        if( poTransfer->GetLayerType(iLineLayer) != SLTLine )
            continue;

        poLineReader = (SDTSLineReader *)
            poTransfer->GetLayerIndexedReader( iLineLayer );
        if( poLineReader == NULL )
            continue;

        poLineReader->AttachToPolygons( poTransfer, iPolyLayer );
        poLineReader->Rewind();
    }

/* -------------------------------------------------------------------- */
/*      Scan all polygons indexed on this reader, and assemble their    */
/*      rings.                                                          */
/* -------------------------------------------------------------------- */
    SDTSFeature *poFeature;
    
    Rewind();
    while( (poFeature = GetNextFeature()) != NULL )
    {
        SDTSRawPolygon  *poPoly = (SDTSRawPolygon *) poFeature;

        poPoly->AssembleRings();
    }
    
    Rewind();
}
