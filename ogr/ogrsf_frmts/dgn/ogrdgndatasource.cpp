/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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
 * Revision 1.2  2000/12/28 21:27:11  warmerda
 * updated email address
 *
 * Revision 1.1  2000/11/28 19:03:47  warmerda
 * New
 *
 */

#include "ogr_dgn.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                         OGRDGNDataSource()                           */
/************************************************************************/

OGRDGNDataSource::OGRDGNDataSource()

{
    papoLayers = NULL;
    nLayers = 0;
    hDGN = NULL;
    pszName = NULL;
}

/************************************************************************/
/*                        ~OGRDGNDataSource()                           */
/************************************************************************/

OGRDGNDataSource::~OGRDGNDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );
    CPLFree( pszName );

    if( hDGN != NULL )
        DGNClose( hDGN );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDGNDataSource::Open( const char * pszNewName, int bTestOpen )

{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      For now we require files to have the .dgn or .DGN               */
/*      extension.  Eventually we will implement a more                 */
/*      sophisticated test to see if it is a dgn file.                  */
/* -------------------------------------------------------------------- */
    if( bTestOpen 
        && strstr(pszNewName,".dgn") == NULL
        && strstr(pszNewName,".DGN") == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to open the file as a DGN file.                             */
/* -------------------------------------------------------------------- */
    hDGN = DGNOpen( pszNewName );
    if( hDGN == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to open %s as a Microstation .dgn file.\n", 
                      pszNewName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRDGNLayer	*poLayer;

    poLayer = new OGRDGNLayer( "elements", hDGN );
    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRDGNLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRDGNLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDGNDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return FALSE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDGNDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

