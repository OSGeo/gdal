/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSLineReader and SDTSRawLine classes.
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

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                            SDTSRawLine                               */
/*                                                                      */
/*      This is a simple class for holding the data related with a      */
/*      line feature.                                                   */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            SDTSRawLine()                             */
/************************************************************************/

SDTSRawLine::SDTSRawLine() :
    nVertices(0),
    padfX(nullptr),
    padfY(nullptr),
    padfZ(nullptr)
{
    nAttributes = 0;
}

/************************************************************************/
/*                            ~STDSRawLine()                            */
/************************************************************************/

SDTSRawLine::~SDTSRawLine()

{
    CPLFree( padfX );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record from the passed SDTSLineReader, and assign the    */
/*      values from that record to this line.  This is the bulk of      */
/*      the work in this whole file.                                    */
/************************************************************************/

int SDTSRawLine::Read( SDTS_IREF * poIREF, DDFRecord * poRecord )

{
    // E.Rouault: Not sure if this test is really useful
    if( poRecord->GetStringSubfield( "LINE", 0, "MODN", 0 ) == nullptr )
        return FALSE;

/* ==================================================================== */
/*      Loop over fields in this record, looking for those we           */
/*      recognise, and need.  I don't use the getSubfield()             */
/*      interface on the record in order to retain some slight bit      */
/*      of efficiency.                                                  */
/* ==================================================================== */
    for( int iField = 0; iField < poRecord->GetFieldCount(); iField++ )
    {
        DDFField        *poField = poRecord->GetField( iField );
        if( poField == nullptr )
            return FALSE;
        DDFFieldDefn* poFieldDefn = poField->GetFieldDefn();
        if( poFieldDefn == nullptr )
            return FALSE;

        const char *pszFieldName = poFieldDefn->GetName();

        if( EQUAL(pszFieldName,"LINE") )
            oModId.Set( poField );

        else if( EQUAL(pszFieldName,"ATID") )
            ApplyATID( poField );

        else if( EQUAL(pszFieldName,"PIDL") )
            oLeftPoly.Set( poField );

        else if( EQUAL(pszFieldName,"PIDR") )
            oRightPoly.Set( poField );

        else if( EQUAL(pszFieldName,"SNID") )
            oStartNode.Set( poField );

        else if( EQUAL(pszFieldName,"ENID") )
            oEndNode.Set( poField );

        else if( EQUAL(pszFieldName,"SADR") )
        {
            nVertices = poIREF->GetSADRCount( poField );

            padfX = reinterpret_cast<double *>(
                CPLRealloc( padfX, sizeof(double) * nVertices*3 ) );
            padfY = padfX + nVertices;
            padfZ = padfX + 2*nVertices;

            if( !poIREF->GetSADR( poField, nVertices, padfX, padfY, padfZ ) )
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                                Dump()                                */
/*                                                                      */
/*      Write info about this object to a text file.                    */
/************************************************************************/

void SDTSRawLine::Dump( FILE * fp )

{
    fprintf( fp, "SDTSRawLine\n" );
    fprintf( fp, "  Module=%s, Record#=%d\n",
             oModId.szModule, oModId.nRecord );
    if( oLeftPoly.nRecord != -1 )
        fprintf( fp, "  LeftPoly (Module=%s, Record=%d)\n",
                 oLeftPoly.szModule, oLeftPoly.nRecord );
    if( oRightPoly.nRecord != -1 )
        fprintf( fp, "  RightPoly (Module=%s, Record=%d)\n",
                 oRightPoly.szModule, oRightPoly.nRecord );
    if( oStartNode.nRecord != -1 )
        fprintf( fp, "  StartNode (Module=%s, Record=%d)\n",
                 oStartNode.szModule, oStartNode.nRecord );
    if( oEndNode.nRecord != -1 )
        fprintf( fp, "  EndNode (Module=%s, Record=%d)\n",
                 oEndNode.szModule, oEndNode.nRecord );
    for( int i = 0; i < nAttributes; i++ )
        fprintf( fp, "  Attribute (Module=%s, Record=%d)\n",
                 paoATID[i].szModule, paoATID[i].nRecord );

    for( int i = 0; i < nVertices; i++ )
    {
        fprintf( fp, "  Vertex[%3d] = (%.2f,%.2f,%.2f)\n",
                 i, padfX[i], padfY[i], padfZ[i] );
    }
}

/************************************************************************/
/* ==================================================================== */
/*                             SDTSLineReader                           */
/*                                                                      */
/*      This is the class used to read a line module.                   */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSLineReader()                           */
/************************************************************************/

SDTSLineReader::SDTSLineReader( SDTS_IREF * poIREFIn ) :
    poIREF(poIREFIn)
{}

/************************************************************************/
/*                             ~SDTSLineReader()                        */
/************************************************************************/

SDTSLineReader::~SDTSLineReader()
{
    Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSLineReader::Close()

{
    oDDFModule.Close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested line file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSLineReader::Open( const char * pszFilename )

{
    return oDDFModule.Open( pszFilename );
}

/************************************************************************/
/*                            GetNextLine()                             */
/*                                                                      */
/*      Fetch the next line feature as an STDSRawLine.                  */
/************************************************************************/

SDTSRawLine *SDTSLineReader::GetNextLine()

{
/* -------------------------------------------------------------------- */
/*      Are we initialized?                                             */
/* -------------------------------------------------------------------- */
    if( oDDFModule.GetFP() == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Read the record.                                                */
/* -------------------------------------------------------------------- */
    DDFRecord *poRecord = oDDFModule.ReadRecord();

    if( poRecord == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Transform into a line feature.                                  */
/* -------------------------------------------------------------------- */
    SDTSRawLine *poRawLine = new SDTSRawLine();

    if( poRawLine->Read( poIREF, poRecord ) )
    {
        return poRawLine;
    }

    delete poRawLine;
    return nullptr;
}

/************************************************************************/
/*                          AttachToPolygons()                          */
/*                                                                      */
/*      Attach line features to all the polygon features they relate    */
/*      to.                                                             */
/************************************************************************/

/**
  Attach lines in this module to their polygons as the first step in
  polygon formation.

  See also the SDTSRawPolygon::AssembleRings() method.

  @param poTransfer the SDTSTransfer of this SDTSLineReader, and from
  which the related SDTSPolygonReader will be instantiated.
  @param iTargetPolyLayer the polygon reader instance number, used to avoid
  processing lines for other layers.

*/

void SDTSLineReader::AttachToPolygons( SDTSTransfer * poTransfer,
                                       int iTargetPolyLayer )

{
    if( !IsIndexed() )
        return;

/* -------------------------------------------------------------------- */
/*      We force a filling of the index because when we attach the      */
/*      lines we are just providing a pointer back to the line          */
/*      features in this readers index.  If they aren't cached in       */
/*      the index then the pointer will be invalid.                     */
/* -------------------------------------------------------------------- */
    FillIndex();

/* ==================================================================== */
/*      Loop over all lines, attaching them to the polygons they        */
/*      have as right and left faces.                                   */
/* ==================================================================== */
    Rewind();
    SDTSRawLine *poLine = nullptr;
    SDTSPolygonReader *poPolyReader = nullptr;
    while( (poLine = reinterpret_cast<SDTSRawLine *>( GetNextFeature()) )
           != nullptr )
    {
/* -------------------------------------------------------------------- */
/*      Skip lines with the same left and right polygon face.  These    */
/*      are dangles, and will not contribute in any useful fashion      */
/*      to the resulting polygon.                                       */
/* -------------------------------------------------------------------- */
        if( poLine->oLeftPoly.nRecord == poLine->oRightPoly.nRecord )
            continue;

/* -------------------------------------------------------------------- */
/*      If we don't have our indexed polygon reader yet, try to get     */
/*      it now.                                                         */
/* -------------------------------------------------------------------- */
        if( poPolyReader == nullptr )
        {
            int         iPolyLayer = -1;

            if( poLine->oLeftPoly.nRecord != -1 )
            {
                iPolyLayer = poTransfer->FindLayer(poLine->oLeftPoly.szModule);
            }
            else if( poLine->oRightPoly.nRecord != -1 )
            {
               iPolyLayer = poTransfer->FindLayer(poLine->oRightPoly.szModule);
            }

            if( iPolyLayer == -1 )
                continue;

            if( iPolyLayer != iTargetPolyLayer )
                continue;

            poPolyReader = reinterpret_cast<SDTSPolygonReader *>(
                poTransfer->GetLayerIndexedReader(iPolyLayer) );

            if( poPolyReader == nullptr )
                return;
        }

/* -------------------------------------------------------------------- */
/*      Attach line to right and/or left polygons.                      */
/* -------------------------------------------------------------------- */
        if( poLine->oLeftPoly.nRecord != -1 )
        {
          SDTSRawPolygon *poPoly = reinterpret_cast<SDTSRawPolygon *>(
              poPolyReader->GetIndexedFeatureRef( poLine->oLeftPoly.nRecord ) );
            if( poPoly != nullptr )
                poPoly->AddEdge( poLine );
        }

        if( poLine->oRightPoly.nRecord != -1 )
        {
            SDTSRawPolygon *poPoly = reinterpret_cast<SDTSRawPolygon *>(
                poPolyReader->GetIndexedFeatureRef(
                    poLine->oRightPoly.nRecord ) );

            if( poPoly != nullptr )
                poPoly->AddEdge( poLine );
        }
    }
}
