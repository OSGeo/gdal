/******************************************************************************
 *
 * Project:  Epiinfo .REC Translator
 * Purpose:  Implements OGRRECDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_rec.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRRECDataSource()                          */
/************************************************************************/

OGRRECDataSource::OGRRECDataSource() :
    pszName(NULL),
    poLayer(NULL)
{}

/************************************************************************/
/*                         ~OGRRECDataSource()                          */
/************************************************************************/

OGRRECDataSource::~OGRRECDataSource()

{
    if( poLayer != NULL )
        delete poLayer;

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRRECDataSource::TestCapability( const char * )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRRECDataSource::GetLayer( int iLayer )

{
    if( iLayer == 0 )
        return poLayer;

    return NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRRECDataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Verify that the extension is REC.                               */
/* -------------------------------------------------------------------- */
    if( !(strlen(pszFilename) > 4 &&
          EQUAL(pszFilename+strlen(pszFilename)-4,".rec") ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    FILE *fp = VSIFOpen( pszFilename, "rb" );
    if( fp == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Read a line, and verify that it consists of at least one        */
/*      field that is a number greater than zero.                       */
/* -------------------------------------------------------------------- */
    const char * pszLine = CPLReadLine( fp );
    if( pszLine == NULL )
    {
        VSIFClose( fp );
        return FALSE;
    }

    const int nFieldCount = atoi(pszLine);
    if( nFieldCount < 1 || nFieldCount > 1000 )
    {
        VSIFClose( fp );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    poLayer = new OGRRECLayer( CPLGetBasename(pszFilename), fp, nFieldCount );

    return poLayer->IsValid();
}
