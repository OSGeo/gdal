/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLFeature.
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
 * Revision 1.3  2005/05/04 19:51:26  fwarmerdam
 * Avoid leaking in SetProperty() if the property is already set.
 *
 * Revision 1.2  2002/01/24 17:37:06  warmerda
 * added SetGeometryDirectly
 *
 * Revision 1.1  2002/01/04 19:46:30  warmerda
 * New
 *
 *
 **********************************************************************/

#include "gmlreader.h"
#include "cpl_conv.h"

/************************************************************************/
/*                             GMLFeature()                             */
/************************************************************************/

GMLFeature::GMLFeature( GMLFeatureClass *poClass )

{
    m_poClass = poClass;
    m_pszFID = NULL;
    m_pszGeometry = NULL;
    
    m_nPropertyCount = 0;
    m_papszProperty = NULL;
}

/************************************************************************/
/*                            ~GMLFeature()                             */
/************************************************************************/

GMLFeature::~GMLFeature()

{
    CPLFree( m_pszFID );
    
    for( int i = 0; i < m_nPropertyCount; i++ )
    {
        if( m_papszProperty[i] )
            CPLFree( m_papszProperty[i] );
    }

    CPLFree( m_papszProperty );
    CPLFree( m_pszGeometry );
}

/************************************************************************/
/*                               SetFID()                               */
/************************************************************************/

void GMLFeature::SetFID( const char *pszFID )

{
    CPLFree( m_pszFID );
    if( pszFID != NULL )
        m_pszFID = CPLStrdup( pszFID );
    else
        m_pszFID = NULL;
}

/************************************************************************/
/*                            GetProperty()                             */
/************************************************************************/

const char *GMLFeature::GetProperty( int iIndex ) const

{
    if( iIndex < 0 || iIndex >= m_nPropertyCount )
        return NULL;
    else
        return m_papszProperty[iIndex];
}

/************************************************************************/
/*                            SetProperty()                             */
/************************************************************************/

void GMLFeature::SetProperty( int iIndex, const char *pszValue )

{
    if( iIndex < 0 || iIndex >= m_poClass->GetPropertyCount() )
    {
        CPLAssert( FALSE );
        return;
    }

    if( iIndex >= m_nPropertyCount )
    {
        m_papszProperty = (char **) 
            CPLRealloc( m_papszProperty, 
                        sizeof(char *) * m_poClass->GetPropertyCount() );
        for( int i = m_nPropertyCount; i < m_poClass->GetPropertyCount(); i++ )
            m_papszProperty[i] = NULL;
        m_nPropertyCount = m_poClass->GetPropertyCount();
    }

    CPLFree( m_papszProperty[iIndex] );
    m_papszProperty[iIndex] = CPLStrdup( pszValue );
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void GMLFeature::Dump( FILE * fp )

{
    printf( "GMLFeature(%s):\n", m_poClass->GetName() );
    
    if( m_pszFID != NULL )
        printf( "  FID = %s\n", m_pszFID );
    
    for( int i = 0; i < m_nPropertyCount; i++ )
        printf( "  %s = %s\n", 
                m_poClass->GetProperty( i )->GetName(),
                GetProperty( i ) );

    if( m_pszGeometry )
        printf( "  %s\n", m_pszGeometry );
}

/************************************************************************/
/*                        SetGeometryDirectly()                         */
/************************************************************************/

void GMLFeature::SetGeometryDirectly( char *pszGeometry )

{
    if( m_pszGeometry )
        CPLFree( m_pszGeometry );

    m_pszGeometry = pszGeometry;
}
