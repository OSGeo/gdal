/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implements OGRSDTSDataSource class
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

#include "ogr_sdts.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRSDTSDataSource()                          */
/************************************************************************/

OGRSDTSDataSource::OGRSDTSDataSource()

{
    nLayers = 0;
    papoLayers = NULL;

    pszName = NULL;
    poSRS = NULL;

    poTransfer = NULL;
}

/************************************************************************/
/*                         ~OGRSDTSDataSource()                          */
/************************************************************************/

OGRSDTSDataSource::~OGRSDTSDataSource()

{
    int         i;

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    CPLFree( pszName );

    if( poSRS )
        poSRS->Release();

    if( poTransfer )
        delete poTransfer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSDTSDataSource::TestCapability( const char * )

{
    return FALSE;
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
    if( bTestOpen && !(strlen(pszFilename) > 4 &&
        EQUAL(pszFilename+strlen(pszFilename)-4,".ddf")) )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Check a few bits of the header to see if it looks like an       */
/*      SDTS file (really, if it looks like an ISO8211 file).           */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        FILE    *fp;
        char    pachLeader[10];

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
/*      Initialize the projection.                                      */
/* -------------------------------------------------------------------- */
    SDTS_XREF   *poXREF = poTransfer->GetXREF();

    poSRS = new OGRSpatialReference();

    if( EQUAL(poXREF->pszSystemName,"UTM") )
    {
        poSRS->SetUTM( poXREF->nZone, TRUE );
    }

    if( EQUAL(poXREF->pszDatum,"NAS") )
        poSRS->SetGeogCS("NAD27", "North_American_Datum_1927",
                         "Clarke 1866", 6378206.4, 294.978698213901 );
    
    else if( EQUAL(poXREF->pszDatum,"NAX") )
        poSRS->SetGeogCS("NAD83", "North_American_Datum_1983",
                         "GRS 1980", 6378137, 298.257222101 );
    
    else if( EQUAL(poXREF->pszDatum,"WGC") )
        poSRS->SetGeogCS("WGS 72", "WGS_1972", "NWL 10D", 6378135, 298.26 );
    
    else if( EQUAL(poXREF->pszDatum,"WGE") )
        poSRS->SetGeogCS("WGS 84", "WGS_1984",
                         "WGS 84", 6378137, 298.257223563 );

    else
        poSRS->SetGeogCS("WGS 84", "WGS_1984",
                         "WGS 84", 6378137, 298.257223563 );

    poSRS->Fixup();

/* -------------------------------------------------------------------- */
/*      Initialize a layer for each source dataset layer.               */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < poTransfer->GetLayerCount(); iLayer++ )
    {
        SDTSIndexedReader       *poReader;
        
        if( poTransfer->GetLayerType( iLayer ) == SLTRaster )
            continue;

        poReader = poTransfer->GetLayerIndexedReader( iLayer );
        if( poReader == NULL )
            continue;
        
        papoLayers = (OGRSDTSLayer **)
            CPLRealloc( papoLayers, sizeof(void*) * ++nLayers );
        papoLayers[nLayers-1] = new OGRSDTSLayer( poTransfer, iLayer, this );
    }
    
    return TRUE;
}

