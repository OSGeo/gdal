/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLFeatureClass.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log$
 * Revision 1.1  2002/01/04 19:46:30  warmerda
 * New
 *
 *
 **********************************************************************/

#include "gmlreader.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          GMLFeatureClass()                           */
/************************************************************************/

GMLFeatureClass::GMLFeatureClass( const char *pszName )

{
    m_pszName = CPLStrdup( pszName );
    m_pszElementName = NULL;
    m_nPropertyCount = 0;
    m_papoProperty = NULL;
    m_bSchemaLocked = FALSE;
}

/************************************************************************/
/*                          ~GMLFeatureClass()                          */
/************************************************************************/

GMLFeatureClass::~GMLFeatureClass()

{
    CPLFree( m_pszName );
    CPLFree( m_pszElementName );

    for( int i = 0; i < m_nPropertyCount; i++ )
        delete m_papoProperty[i];
    CPLFree( m_papoProperty );
}

/************************************************************************/
/*                           GetProperty(int)                           */
/************************************************************************/

GMLPropertyDefn *GMLFeatureClass::GetProperty( int iIndex ) const

{
    if( iIndex < 0 || iIndex >= m_nPropertyCount )
        return NULL;
    else
        return m_papoProperty[iIndex];
}

/************************************************************************/
/*                          GetPropertyIndex()                          */
/************************************************************************/

int GMLFeatureClass::GetPropertyIndex( const char *pszName ) const

{
    for( int i = 0; i < m_nPropertyCount; i++ )
        if( EQUAL(pszName,m_papoProperty[i]->GetName()) )
            return i;

    return -1;
}

/************************************************************************/
/*                            AddProperty()                             */
/************************************************************************/

int GMLFeatureClass::AddProperty( GMLPropertyDefn *poDefn )

{
    CPLAssert( GetProperty(poDefn->GetName()) == NULL );

    m_nPropertyCount++;
    m_papoProperty = (GMLPropertyDefn **)
        CPLRealloc( m_papoProperty, sizeof(void*) * m_nPropertyCount );

    m_papoProperty[m_nPropertyCount-1] = poDefn;

    return m_nPropertyCount-1;
}

/************************************************************************/
/*                           SetElementName()                           */
/************************************************************************/

void GMLFeatureClass::SetElementName( const char *pszElementName )

{
    CPLFree( m_pszElementName );
    m_pszElementName = CPLStrdup( pszElementName );
}

/************************************************************************/
/*                           GetElementName()                           */
/************************************************************************/

const char *GMLFeatureClass::GetElementName() const

{
    if( m_pszElementName == NULL )
        return m_pszName;
    else
        m_pszElementName;
}
