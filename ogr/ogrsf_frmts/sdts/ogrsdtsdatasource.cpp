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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.11  2006/03/28 23:17:06  fwarmerdam
 * updated contact info
 *
 * Revision 1.10  2005/09/21 00:54:43  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.9  2003/12/11 21:10:25  warmerda
 * avoid SRS leak
 *
 * Revision 1.8  2003/02/06 03:21:59  warmerda
 * Added OGRSpatialReference.Fixup() call to set linear units
 * as per http://bugzilla.remotesensing.org/show_bug.cgi?id=279.
 *
 * Revision 1.7  2002/04/17 15:17:49  warmerda
 * Initialize poTransfer to NULL.
 *
 * Revision 1.6  2002/04/15 13:18:39  warmerda
 * Free transfer in destructor!
 *
 * Revision 1.5  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.4  2001/01/19 21:14:22  warmerda
 * expanded tabs
 *
 * Revision 1.3  2000/02/20 21:17:56  warmerda
 * added projection support
 *
 * Revision 1.2  1999/11/04 21:12:31  warmerda
 * added TestCapability() support
 *
 * Revision 1.1  1999/09/22 13:32:16  warmerda
 * New
 *
 */

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
    if( bTestOpen && !EQUAL(pszFilename+strlen(pszFilename)-4,".ddf") )
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

