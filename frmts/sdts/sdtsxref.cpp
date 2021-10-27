/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTS_XREF class for reading XREF module.
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
/*                             SDTS_XREF()                              */
/************************************************************************/

SDTS_XREF::SDTS_XREF() :
    pszSystemName(CPLStrdup("")),
    pszDatum(CPLStrdup("")),
    nZone(0)
{}

/************************************************************************/
/*                             ~SDTS_XREF()                             */
/************************************************************************/

SDTS_XREF::~SDTS_XREF()
{
    CPLFree( pszSystemName );
    CPLFree( pszDatum );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read the named file to initialize this structure.               */
/************************************************************************/

int SDTS_XREF::Read( const char * pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Open the file, and read the header.                             */
/* -------------------------------------------------------------------- */
    DDFModule oXREFFile;
    if( !oXREFFile.Open( pszFilename ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Read the first record, and verify that this is an XREF record.  */
/* -------------------------------------------------------------------- */
    DDFRecord *poRecord = oXREFFile.ReadRecord();
    if( poRecord == nullptr )
        return FALSE;

    if( poRecord->GetStringSubfield( "XREF", 0, "MODN", 0 ) == nullptr )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Read fields of interest.                                        */
/* -------------------------------------------------------------------- */

    CPLFree( pszSystemName );
    pszSystemName =
        CPLStrdup( poRecord->GetStringSubfield( "XREF", 0, "RSNM", 0 ) );

    CPLFree( pszDatum );
    pszDatum =
        CPLStrdup( poRecord->GetStringSubfield( "XREF", 0, "HDAT", 0 ) );

    nZone = poRecord->GetIntSubfield( "XREF", 0, "ZONE", 0 );

    return TRUE;
}
