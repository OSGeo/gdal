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
 * Revision 1.2  1999/03/23 16:00:05  warmerda
 * doc typo fixed
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"

#include <iostream>
#include <fstream>
#include "io/sio_Reader.h"
#include "io/sio_8211Converter.h"

#include "container/sc_Record.h"

sio_8211Converter_BI8   converter_bi8;
sio_8211Converter_BI16	converter_bi16;
sio_8211Converter_BI24  converter_bi24;
sio_8211Converter_BI32	converter_bi32;
sio_8211Converter_BUI8	converter_bui8;
sio_8211Converter_BUI16	converter_bui16;
sio_8211Converter_BUI24	converter_bui24;
sio_8211Converter_BUI32	converter_bui32;
sio_8211Converter_BFP32	converter_bfp32;
sio_8211Converter_BFP64 converter_bfp64;

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
}

/************************************************************************/
/*                            ~STDSRawLine()                            */
/************************************************************************/

SDTSRawLine::~SDTSRawLine()

{
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record from the passed SDTSLineReader, and assign the    */
/*      values from that record to this line.  This is the bulk of      */
/*      the work in this whole file.                                    */
/************************************************************************/

int SDTSRawLine::Read( SDTS_IREF * poIREF, scal_Record * poRecord )

{

/* ==================================================================== */
/*      Loop over fields in this record, looking for those we           */
/*      recognise, and need.  I don't use the getSubfield()             */
/*      interface on the record in order to retain some slight bit      */
/*      of efficiency.                                                  */
/* ==================================================================== */
    sc_Record::const_iterator	oFieldIter;

    for( oFieldIter = poRecord->begin();
         oFieldIter != poRecord->end();
         ++oFieldIter )
    {
        const sc_Field	&oField = *oFieldIter;
        string		osTemp;

        if( oField.getMnemonic() == "LINE" )
            oLine.Set( &oField );

        else if( oField.getMnemonic() == "ATID" )
            oAttribute.Set( &oField );
        
        else if( oField.getMnemonic() == "PIDL" )
            oLeftPoly.Set( &oField );
        
        else if( oField.getMnemonic() == "PIDR" )
            oRightPoly.Set( &oField );
        
        else if( oField.getMnemonic() == "SNID" )
            oStartNode.Set( &oField );
        
        else if( oField.getMnemonic() == "ENID" )
            oEndNode.Set( &oField );

        else if( oField.getMnemonic() == "SADR" )
        {
            /* notdef: this is _really inefficient_! */
            nVertices++;
            padfX = (double *) CPLRealloc(padfX, sizeof(double)*nVertices);
            padfY = (double *) CPLRealloc(padfY, sizeof(double)*nVertices);
            padfZ = (double *) CPLRealloc(padfZ, sizeof(double)*nVertices);

            SDTSGetSADR( poIREF, &oField,
                         padfX + nVertices - 1,
                         padfY + nVertices - 1,
                         padfZ + nVertices - 1 );
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
    fprintf( fp, "  Module=%s, Record#=%ld\n",
             oLine.szModule, oLine.nRecord );
    fprintf( fp, "  Attribute (Module=%s, Record=%ld)\n", 
             oAttribute.szModule, oAttribute.nRecord );
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

    for( int i = 0; i < nVertices; i++ )
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
    po8211Reader = NULL;
    poIter = NULL;
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
    if( poIter != NULL )
    {
        delete poIter;
        poIter = NULL;
    }

    if( po8211Reader != NULL )
    {
        delete po8211Reader;
        po8211Reader = NULL;
    }

    if( ifs )
        ifs.close();
}


/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested line file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSLineReader::Open( string osFilename )

{
    converter_dictionary converters; // hints for reader for binary data
    
    converters["X"] = &converter_bi32; // set up default converter hints
    converters["Y"] = &converter_bi32; // for these mnemonics
    converters["ELEVATION"] = &converter_bi16;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    ifs.open( osFilename.c_str() );
    if( !ifs )
    {
        printf( "Unable to open `%s'\n", osFilename.c_str() );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Establish reader access to the file				*/
/* -------------------------------------------------------------------- */
    po8211Reader = new sio_8211Reader( ifs, &converters );
    poIter = new sio_8211ForwardIterator( *po8211Reader );
    
    if( !(*poIter) )
        return FALSE;
    else
        return TRUE;
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
    if( poIter == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Is the record iterator at the end of the file?                  */
/* -------------------------------------------------------------------- */
    if( ! *poIter )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read the record.                                                */
/* -------------------------------------------------------------------- */
    scal_Record		oRecord;
    
    poIter->get( oRecord );
    ++(*poIter);

/* -------------------------------------------------------------------- */
/*      Transform into a line feature.                                  */
/* -------------------------------------------------------------------- */
    SDTSRawLine		*poRawLine = new SDTSRawLine();

    if( poRawLine->Read( poIREF, &oRecord ) )
    {
        return( poRawLine );
    }
    else
    {
        delete poRawLine;
        return NULL;
    }
}

