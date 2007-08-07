/******************************************************************************
 * $Id: ogrbnadatasource.cpp
 *
 * Project:  BNA Translator
 * Purpose:  Implements OGRBNADataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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

#include "ogr_bna.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "ogrbnaparser.h"

/************************************************************************/
/*                          OGRBNADataSource()                          */
/************************************************************************/

OGRBNADataSource::OGRBNADataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;

    bUpdate = FALSE;
}

/************************************************************************/
/*                         ~OGRBNADataSource()                          */
/************************************************************************/

OGRBNADataSource::~OGRBNADataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRBNADataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return FALSE;
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return FALSE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRBNADataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRBNADataSource::Open( const char * pszFilename, int bUpdateIn)

{
    int ok = FALSE;

    pszName = CPLStrdup( pszFilename );
    bUpdate = bUpdateIn;

/* -------------------------------------------------------------------- */
/*      Determine what sort of object this is.                          */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) != 0 )
        return FALSE;
    
// -------------------------------------------------------------------- 
//      Does this appear to be an .bna file?
// --------------------------------------------------------------------
    if( !EQUAL( CPLGetExtension(pszFilename), "bna" ) )
        return FALSE;

    FILE* f = VSIFOpen(pszFilename, "rb");
    if (f)
    {
        BNARecord* record;
        int curLine = 0;
        record = BNA_GetNextRecord(f, &ok, &curLine, 0, BNA_READ_NONE);
        BNA_FreeRecord(record);

        if (ok)
        {
            nLayers = 4;

            papoLayers = (OGRBNALayer **) CPLMalloc(nLayers * sizeof(OGRBNALayer*));
            papoLayers[0] = new OGRBNALayer( pszFilename, "points", BNA_POINT, wkbPoint );
            papoLayers[1] = new OGRBNALayer( pszFilename, "lines", BNA_POLYLINE, wkbLineString );
            papoLayers[2] = new OGRBNALayer( pszFilename, "polygons", BNA_POLYGON, wkbMultiPolygon );
            papoLayers[3] = new OGRBNALayer( pszFilename, "ellipses", BNA_ELLIPSE, wkbPolygon );
        }

        VSIFClose(f);
    }

    return ok;
}

