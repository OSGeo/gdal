/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTS_IREF class for reading IREF module.
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

/************************************************************************/
/*                             SDTS_IREF()                              */
/************************************************************************/

SDTS_IREF::SDTS_IREF()

{
    dfXScale = 1.0;
    dfYScale = 1.0;
}

/************************************************************************/
/*                             ~SDTS_IREF()                             */
/************************************************************************/

SDTS_IREF::~SDTS_IREF()
{
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read the named file to initialize this structure.               */
/************************************************************************/

int SDTS_IREF::Read( string osFilename )

{
    sc_Subfield	*poSubfield;
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    ifstream	ifs;
    
    ifs.open( osFilename.c_str() );
    if( !ifs )
    {
        printf( "Unable to open `%s'\n", osFilename.c_str() );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Establish reader access to the file, and read the first         */
/*      (only) record in the IREF file.                                 */
/* -------------------------------------------------------------------- */
    sio_8211Reader	oReader( ifs, NULL );
    sio_8211ForwardIterator oIter( oReader );
    scal_Record		oRecord;
    
    if( !oIter )
        return FALSE;
    
    oIter.get( oRecord );

/* -------------------------------------------------------------------- */
/*      Verify that we have a proper IREF record.                       */
/* -------------------------------------------------------------------- */
    if( oRecord.getSubfield( "IREF", 0, "MODN", 0 ) == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the labels.                                                 */
/* -------------------------------------------------------------------- */
    poSubfield = oRecord.getSubfield( "IREF", 0, "XLBL", 0 );
    if( poSubfield != NULL )
        poSubfield->getA( osXAxisName );

    poSubfield = oRecord.getSubfield( "IREF", 0, "YLBL", 0 );
    if( poSubfield != NULL )
        poSubfield->getA( osYAxisName );
    
/* -------------------------------------------------------------------- */
/*	Get the coordinate encoding.					*/
/* -------------------------------------------------------------------- */
    poSubfield = oRecord.getSubfield( "IREF", 0, "HFMT", 0 );
    if( poSubfield != NULL )
        poSubfield->getA( osCoordinateFormat );

/* -------------------------------------------------------------------- */
/*      Get the scaling factors.                                        */
/* -------------------------------------------------------------------- */
    poSubfield = oRecord.getSubfield( "IREF", 0, "SFAX", 0 );
    if( poSubfield != NULL )
        poSubfield->getR( dfXScale );

    poSubfield = oRecord.getSubfield( "IREF", 0, "SFAY", 0 );
    if( poSubfield != NULL )
        poSubfield->getR( dfYScale );

    return TRUE;
}


