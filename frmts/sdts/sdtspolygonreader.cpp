/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSPolygonReader and SDTSRawPolygon classes.
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
 * Revision 1.6  1999/09/03 13:35:05  warmerda
 * cleanup array in assemblerings
 *
 * Revision 1.5  1999/09/03 13:01:39  warmerda
 * added docs
 *
 * Revision 1.4  1999/09/02 03:40:03  warmerda
 * added indexed readers
 *
 * Revision 1.3  1999/08/10 02:52:13  warmerda
 * introduce use of SDTSApplyModIdList to capture multi-attributes
 *
 * Revision 1.2  1999/07/30 19:15:56  warmerda
 * added module reference counting
 *
 * Revision 1.1  1999/05/11 14:04:42  warmerda
 * New
 *
 */

#include "sdts_al.h"

/************************************************************************/
/* ==================================================================== */
/*			      SDTSRawPolygon				*/
/*									*/
/*	This is a simple class for holding the data related with a 	*/
/*	polygon feature.						*/
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
/*      recognise, and need.						*/
/* ==================================================================== */
    for( int iField = 0; iField < poRecord->GetFieldCount(); iField++ )
    {
        DDFField	*poField = poRecord->GetField( iField );
        const char	*pszFieldName;

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

void SDTSRawPolygon::AddEdgeToRing( SDTSRawLine * poLine,
                                    int bReverse, int bDropVertex )

{
    int		iStart, iEnd, iStep;

    if( bDropVertex && bReverse )
    {
        iStart = poLine->nVertices - 2;
        iEnd = 0;
        iStep = -1;
    }
    else if( bDropVertex && !bReverse )
    {
        iStart = 1;
        iEnd = poLine->nVertices - 1;
        iStep = 1;
    }
    else 
    {
        CPLAssert( !bDropVertex && !bReverse );
        iStart = 0;
        iEnd = poLine->nVertices - 1;
        iStep = 1;
    }

    for( int i = iStart; i != (iEnd+iStep); i += iStep )
    {
        padfX[nVertices] = poLine->padfX[i];
        padfY[nVertices] = poLine->padfY[i];
        padfZ[nVertices] = poLine->padfZ[i];

        nVertices++;
    }
}

/************************************************************************/
/*                           AssembleRings()                            */
/************************************************************************/

int SDTSRawPolygon::AssembleRings()

{
    int		iEdge;
    int		bSuccess = TRUE;
    
    if( nRings > 0 )
        return TRUE;

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
    int	*panEdgeConsumed, nRemainingEdges = nEdges;

    panEdgeConsumed = (int *) CPLCalloc(sizeof(int),nEdges);

/* ==================================================================== */
/*      Loop generating rings.                                          */
/* ==================================================================== */
    while( nRemainingEdges > 0 )
    {
        int		nStartNode, nLinkNode;
        
/* -------------------------------------------------------------------- */
/*      Find the first unconsumed edge.                                 */
/* -------------------------------------------------------------------- */
        SDTSRawLine	*poEdge;
        
        for( iEdge = 0; panEdgeConsumed[iEdge]; iEdge++ ) {}

        poEdge = papoEdges[iEdge];

/* -------------------------------------------------------------------- */
/*      Start a new ring, copying in the current line directly          */
/* -------------------------------------------------------------------- */
        panRingStart[nRings++] = nVertices;

        AddEdgeToRing( poEdge, FALSE, FALSE );

        panEdgeConsumed[iEdge] = TRUE;
        nRemainingEdges--;
        
        nStartNode = poEdge->oStartNode.nRecord;
        nLinkNode = poEdge->oEndNode.nRecord;

/* ==================================================================== */
/*      Loop adding edges to this ring until we make a whole pass       */
/*      within finding anything to add.                                 */
/* ==================================================================== */
        int		bWorkDone = TRUE;

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
                    AddEdgeToRing( poEdge, FALSE, TRUE );
                    nLinkNode = poEdge->oEndNode.nRecord;
                }
                else if( poEdge->oEndNode.nRecord == nLinkNode )
                {
                    AddEdgeToRing( poEdge, TRUE, TRUE );
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

    return bSuccess;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void SDTSRawPolygon::Dump( FILE * fp )

{
    int		i;
    
    fprintf( fp, "SDTSRawPolygon %s: ", oModId.GetName() );

    for( i = 0; i < nAttributes; i++ )
        fprintf( fp, "  ATID[%d]=%s", i, aoATID[i].GetName() );

    fprintf( fp, "\n" );
}

/************************************************************************/
/* ==================================================================== */
/*			       SDTSPolygonReader			*/
/*									*/
/*	This is the class used to read a Polygon module.		*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSPolygonReader()                          */
/************************************************************************/

SDTSPolygonReader::SDTSPolygonReader()

{
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
    DDFRecord	*poRecord;
    
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
    SDTSRawPolygon	*poRawPolygon = new SDTSRawPolygon();

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
