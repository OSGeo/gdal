/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implements OGRSDTSDataSource class
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
 * Revision 1.1  1999/09/22 13:32:16  warmerda
 * New
 *
 */

#include "ogr_sdts.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                          OGRSDTSDataSource()                          */
/************************************************************************/

OGRSDTSDataSource::OGRSDTSDataSource()

{
    nLayers = 0;
    papoLayers = NULL;

    pszName = NULL;
}

/************************************************************************/
/*                         ~OGRSDTSDataSource()                          */
/************************************************************************/

OGRSDTSDataSource::~OGRSDTSDataSource()

{
    int		i;

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSDTSDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSDTSDataSource::Open( const char * pszFilename, int bTestOpen )

{
    pszName = CPLStrdup( pszFilename );
    
/* -------------------------------------------------------------------- */
/*      Verify that the extension is DDF if we are testopening.         */
/* -------------------------------------------------------------------- */
    if( bTestOpen && !EQUAL(pszFilename+strlen(pszFilename)-4,".ddf") )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Check a few bits of the header to see if it looks like an       */
/*      SDTS file (really, if it looks like an ISO8211 file).           */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        FILE	*fp;
        char	pachLeader[10];

        fp = VSIFOpen( pszFilename, "rb" );
        if( fp == NULL )
            return FALSE;
        
        if( VSIFRead( pachLeader, 1, 10, fp ) != 10
            || (pachLeader[5] != '1' && pachLeader[5] != '2'
                && pachLeader[5] != '3' )
            || pachLeader[6] != 'L'
            || (pachLeader[8] != '1' && pachLeader[8] != ' ') )
        {
            VSIFClose( fp );
            return FALSE;
        }

        VSIFClose( fp );
    }

/* -------------------------------------------------------------------- */
/*      Create a transfer, and open it.                                 */
/* -------------------------------------------------------------------- */
    poTransfer = new SDTSTransfer();

    if( !poTransfer->Open( pszFilename ) )
    {
        delete poTransfer;
        poTransfer = NULL;
        
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Initialize a layer for each source dataset layer.               */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < poTransfer->GetLayerCount(); iLayer++ )
    {
        SDTSIndexedReader	*poReader;
        
        if( poTransfer->GetLayerType( iLayer ) == SLTRaster )
            continue;

        poReader = poTransfer->GetLayerIndexedReader( iLayer );
        if( poReader == NULL )
            continue;
        
        papoLayers = (OGRSDTSLayer **)
            CPLRealloc( papoLayers, sizeof(void*) * ++nLayers );
        papoLayers[nLayers-1] = new OGRSDTSLayer( poTransfer, iLayer );
    }
    
    return TRUE;
}

