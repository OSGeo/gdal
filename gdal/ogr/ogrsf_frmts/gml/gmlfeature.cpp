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
 ****************************************************************************/

#include "gmlreader.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                             GMLFeature()                             */
/************************************************************************/

GMLFeature::GMLFeature( GMLFeatureClass *poClass )

{
    m_poClass = poClass;
    m_pszFID = NULL;
    m_pszGeometry = NULL;
    
    m_nPropertyCount = 0;
    m_pasProperties = NULL;
    
    m_papszOBProperties = NULL;
}

/************************************************************************/
/*                            ~GMLFeature()                             */
/************************************************************************/

GMLFeature::~GMLFeature()

{
    CPLFree( m_pszFID );
    
    for( int i = 0; i < m_nPropertyCount; i++ )
    {
        for( int j = 0; j < m_pasProperties[i].nSubProperties; j++)
            CPLFree( m_pasProperties[i].papszSubProperties[j] );
        CPLFree( m_pasProperties[i].papszSubProperties );
    }

    CPLFree( m_pasProperties );
    CPLFree( m_pszGeometry );
    CSLDestroy( m_papszOBProperties );
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

const GMLProperty *GMLFeature::GetProperty( int iIndex ) const

{
    if( iIndex < 0 || iIndex >= m_nPropertyCount )
        return NULL;
    else
        return &m_pasProperties[iIndex];
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
        m_pasProperties = (GMLProperty*) 
            CPLRealloc( m_pasProperties, 
                        sizeof(GMLProperty) * m_poClass->GetPropertyCount() );
        for( int i = m_nPropertyCount; i < m_poClass->GetPropertyCount(); i++ )
        {
            m_pasProperties[i].nSubProperties = 0;
            m_pasProperties[i].papszSubProperties = NULL;
        }
        m_nPropertyCount = m_poClass->GetPropertyCount();
    }

    int nSubProperties = m_pasProperties[iIndex].nSubProperties;
    m_pasProperties[iIndex].papszSubProperties = (char**) CPLRealloc(
                        m_pasProperties[iIndex].papszSubProperties,
                        sizeof(char*) * (nSubProperties + 2) );
    m_pasProperties[iIndex].papszSubProperties[nSubProperties] = CPLStrdup( pszValue );
    m_pasProperties[iIndex].papszSubProperties[nSubProperties + 1] = NULL;
    m_pasProperties[iIndex].nSubProperties ++;
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
    {
        const GMLProperty * psGMLProperty = GetProperty( i );
        printf( "  %s = ", m_poClass->GetProperty( i )->GetName());
        for ( int j = 0; j < psGMLProperty->nSubProperties; j ++)
        {
            if (j > 0) printf(", ");
            printf("%s", psGMLProperty->papszSubProperties[j]);
        }
        printf("\n");
    }

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

/************************************************************************/
/*                           AddOBProperty()                            */
/************************************************************************/

void GMLFeature::AddOBProperty( const char *pszName, const char *pszValue )

{
    m_papszOBProperties = 
        CSLAddNameValue( m_papszOBProperties, pszName, pszValue );
}

/************************************************************************/
/*                           GetOBProperty()                            */
/************************************************************************/

const char *GMLFeature::GetOBProperty( const char *pszName )

{
    return CSLFetchNameValue( m_papszOBProperties, pszName );
}

/************************************************************************/
/*                          GetOBProperties()                           */
/************************************************************************/

char **GMLFeature::GetOBProperties()

{
    return m_papszOBProperties;
}
