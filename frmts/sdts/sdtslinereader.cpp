/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSLineReader and SDTSRawLine classes.
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
 * Revision 1.7  1999/08/10 02:52:13  warmerda
 * introduce use of SDTSApplyModIdList to capture multi-attributes
 *
 * Revision 1.6  1999/07/30 19:15:56  warmerda
 * added module reference counting
 *
 * Revision 1.5  1999/06/03 14:11:42  warmerda
 * Avoid redeclaration of i.
 *
 * Revision 1.4  1999/05/07 13:45:01  warmerda
 * major upgrade to use iso8211lib
 *
 * Revision 1.3  1999/04/21 04:39:17  warmerda
 * converter related fixes.
 *
 * Revision 1.2  1999/03/23 16:00:05  warmerda
 * doc typo fixed
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"

/************************************************************************/
/* ==================================================================== */
/*			      SDTSRawLine				*/
/*									*/
/*	This is a simple class for holding the data related with a 	*/
/*	line feature.							*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            SDTSRawLine()                             */
/************************************************************************/

SDTSRawLine::SDTSRawLine()

{
    nVertices = 0;
    padfX = padfY = padfZ = NULL;
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
    CPLAssert( poRecord->GetStringSubfield( "LINE", 0, "MODN", 0 ) != NULL );
    
/* ==================================================================== */
/*      Loop over fields in this record, looking for those we           */
/*      recognise, and need.  I don't use the getSubfield()             */
/*      interface on the record in order to retain some slight bit      */
/*      of efficiency.                                                  */
/* ==================================================================== */
    for( int iField = 0; iField < poRecord->GetFieldCount(); iField++ )
    {
        DDFField	*poField = poRecord->GetField( iField );
        const char	*pszFieldName;

        CPLAssert( poField != NULL );
        pszFieldName = poField->GetFieldDefn()->GetName();

        if( EQUAL(pszFieldName,"LINE") )
            oLine.Set( poField );

        else if( EQUAL(pszFieldName,"ATID") )
            SDTSApplyModIdList( poField, MAX_RAWLINE_ATID,
                                &nAttributes, aoATID );

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
            nVertices = poField->GetDataSize() / SDTS_SIZEOF_SADR;

            padfX = (double *) CPLRealloc(padfX, sizeof(double)*nVertices*3);
            padfY = padfX + nVertices;
            padfZ = padfX + 2*nVertices;

            SDTSGetSADR( poIREF, poField, nVertices, padfX, padfY, padfZ );
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
    int    i;

    fprintf( fp, "SDTSRawLine\n" );
    fprintf( fp, "  Module=%s, Record#=%ld\n",
             oLine.szModule, oLine.nRecord );
    if( oLeftPoly.nRecord != -1 )
        fprintf( fp, "  LeftPoly (Module=%s, Record=%ld)\n", 
                 oLeftPoly.szModule, oLeftPoly.nRecord );
    if( oRightPoly.nRecord != -1 )
        fprintf( fp, "  RightPoly (Module=%s, Record=%ld)\n", 
                 oRightPoly.szModule, oRightPoly.nRecord );
    if( oStartNode.nRecord != -1 )
        fprintf( fp, "  StartNode (Module=%s, Record=%ld)\n", 
                 oStartNode.szModule, oStartNode.nRecord );
    if( oEndNode.nRecord != -1 )
        fprintf( fp, "  EndNode (Module=%s, Record=%ld)\n", 
                 oEndNode.szModule, oEndNode.nRecord );
    for( i = 0; i < nAttributes; i++ )
        fprintf( fp, "  Attribute (Module=%s, Record=%ld)\n", 
                 aoATID[i].szModule, aoATID[i].nRecord );

    for( i = 0; i < nVertices; i++ )
    {
        fprintf( fp, "  Vertex[%3d] = (%.2f,%.2f,%.2f)\n",
                 i, padfX[i], padfY[i], padfZ[i] );
    }
}

/************************************************************************/
/* ==================================================================== */
/*			       SDTSLineReader				*/
/*									*/
/*	This is the class used to read a line module.			*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSLineReader()                           */
/************************************************************************/

SDTSLineReader::SDTSLineReader( SDTS_IREF * poIREFIn )

{
    poIREF = poIREFIn;
}

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
    return( oDDFModule.Open( pszFilename ) );
}

/************************************************************************/
/*                            GetNextLine()                             */
/*                                                                      */
/*      Fetch the next line feature as an STDSRawLine.                  */
/************************************************************************/

SDTSRawLine * SDTSLineReader::GetNextLine()

{
/* -------------------------------------------------------------------- */
/*      Are we initialized?                                             */
/* -------------------------------------------------------------------- */
    if( oDDFModule.GetFP() == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read the record.                                                */
/* -------------------------------------------------------------------- */
    DDFRecord	*poRecord = oDDFModule.ReadRecord();
    
    if( poRecord == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Transform into a line feature.                                  */
/* -------------------------------------------------------------------- */
    SDTSRawLine		*poRawLine = new SDTSRawLine();

    if( poRawLine->Read( poIREF, poRecord ) )
    {
        return( poRawLine );
    }
    else
    {
        delete poRawLine;
        return NULL;
    }
}

/************************************************************************/
/*                        ScanModuleReferences()                        */
/************************************************************************/

char ** SDTSLineReader::ScanModuleReferences( const char * pszFName )

{
    return SDTSScanModuleReferences( &oDDFModule, pszFName );
}
