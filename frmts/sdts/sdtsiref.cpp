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
 * Revision 1.2  1999/05/07 13:45:01  warmerda
 * major upgrade to use iso8211lib
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"

/************************************************************************/
/*                             SDTS_IREF()                              */
/************************************************************************/

SDTS_IREF::SDTS_IREF()

{
    dfXScale = 1.0;
    dfYScale = 1.0;

    pszXAxisName = CPLStrdup("");
    pszYAxisName = CPLStrdup("");
    pszCoordinateFormat = CPLStrdup("");
}

/************************************************************************/
/*                             ~SDTS_IREF()                             */
/************************************************************************/

SDTS_IREF::~SDTS_IREF()
{
    CPLFree( pszXAxisName );
    CPLFree( pszYAxisName );
    CPLFree( pszCoordinateFormat );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read the named file to initialize this structure.               */
/************************************************************************/

int SDTS_IREF::Read( const char * pszFilename )

{
    DDFModule	oIREFFile;
    DDFRecord	*poRecord;

/* -------------------------------------------------------------------- */
/*      Open the file, and read the header.                             */
/* -------------------------------------------------------------------- */
    if( !oIREFFile.Open( pszFilename ) )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Read the first record, and verify that this is an IREF record.  */
/* -------------------------------------------------------------------- */
    poRecord = oIREFFile.ReadRecord();
    if( poRecord == NULL )
        return FALSE;

    if( poRecord->GetStringSubfield( "IREF", 0, "MODN", 0 ) == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the labels.                                                 */
/* -------------------------------------------------------------------- */
    CPLFree( pszXAxisName );
    pszXAxisName = CPLStrdup( poRecord->GetStringSubfield( "IREF", 0,
                                                           "XLBL", 0 ) );
    CPLFree( pszYAxisName );
    pszYAxisName = CPLStrdup( poRecord->GetStringSubfield( "IREF", 0,
                                                           "YLBL", 0 ) );
    
/* -------------------------------------------------------------------- */
/*	Get the coordinate encoding.					*/
/* -------------------------------------------------------------------- */
    CPLFree( pszCoordinateFormat );
    pszCoordinateFormat =
        CPLStrdup( poRecord->GetStringSubfield( "IREF", 0, "HFMT", 0 ) );

/* -------------------------------------------------------------------- */
/*      Get the scaling factors.                                        */
/* -------------------------------------------------------------------- */
    dfXScale = poRecord->GetFloatSubfield( "IREF", 0, "SFAX", 0 );
    dfYScale = poRecord->GetFloatSubfield( "IREF", 0, "SFAY", 0 );

    return TRUE;
}
