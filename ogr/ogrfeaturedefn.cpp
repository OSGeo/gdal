/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFeatureDefn class implementation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.1  1999/06/11 19:21:02  warmerda
 * New
 *
 */

#include "ogr_feature.h"
#include "ogr_p.h"

/************************************************************************/
/*                           OGRFeatureDefn()                           */
/************************************************************************/

OGRFeatureDefn::OGRFeatureDefn( const char * pszName )

{
    pszFeatureClassName = CPLStrdup( pszName );
    nRefCount = 0;
    nFieldCount = 0;
    papoFieldDefn = NULL;
    eGeomType = wkbUnknown;
}

/************************************************************************/
/*                          ~OGRFeatureDefn()                           */
/************************************************************************/

OGRFeatureDefn::~OGRFeatureDefn()

{
    if( nRefCount != 0 )
    {
        CPLDebug( "OGRFeatureDefn",
                  "OGRFeatureDefn with a ref count of %d deleted!\n",
                  nRefCount );
    }
    
    CPLFree( pszFeatureClassName );

    for( int i = 0; i < nFieldCount; i++ )
    {
        delete papoFieldDefn[i];
    }

    CPLFree( papoFieldDefn );
}

/************************************************************************/
/*                            GetFieldDefn()                            */
/************************************************************************/

OGRFieldDefn *OGRFeatureDefn::GetFieldDefn( int i )

{
    if( i < 0 || i >= nFieldCount )
    {
        CPLAssert( FALSE );
        return NULL;
    }

    return papoFieldDefn[i];
}

/************************************************************************/
/*                            AddFieldDefn()                            */
/************************************************************************/

void OGRFeatureDefn::AddFieldDefn( OGRFieldDefn * poNewDefn )

{
    papoFieldDefn = (OGRFieldDefn **)
        CPLRealloc( papoFieldDefn, sizeof(void*)*(nFieldCount+1) );

    papoFieldDefn[nFieldCount] = new OGRFieldDefn( poNewDefn );
    nFieldCount++;
}

/************************************************************************/
/*                            SetGeomType()                             */
/************************************************************************/

void OGRFeatureDefn::SetGeomType( OGRwkbGeometryType eNewType )

{
    eGeomType = eNewType;
}

/************************************************************************/
/*                           GetFieldIndex()                            */
/************************************************************************/

int OGRFeatureDefn::GetFieldIndex( const char * pszFieldName )

{
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( EQUAL(pszFieldName, papoFieldDefn[i]->GetNameRef() ) )
            return i;
    }

    return -1;
}

