/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements a few base methods on OGRGeometry.
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
 * Revision 1.1  1999/05/20 14:35:00  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

/************************************************************************/
/*                            OGRGeometry()                             */
/************************************************************************/

OGRGeometry::OGRGeometry()

{
    poSRS = NULL;
}

/************************************************************************/
/*                            ~OGRGeometry()                            */
/************************************************************************/

OGRGeometry::~OGRGeometry()

{
    if( poSRS != NULL )
        delete poSRS;
}


/************************************************************************/
/*                       assignSpatialReference()                       */
/*                                                                      */
/*      Assign a spatial reference without trying to reproject the      */
/*      points.                                                         */
/************************************************************************/

void OGRGeometry::assignSpatialReference( OGRSpatialReference * poSR )

{
    poSRS = poSR;
}


/************************************************************************/
/*                            dumpReadable()                            */
/************************************************************************/

void OGRGeometry::dumpReadable( FILE * fp, const char * pszPrefix )

{
    char	*pszWkt = NULL;
    
    if( pszPrefix == NULL )
        pszPrefix = "";

    if( exportToWkt( &pszWkt ) == OGRERR_NONE )
    {
        fprintf( fp, "%s%s\n", pszPrefix, pszWkt );
        CPLFree( pszWkt );
    }
}

