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
    
    m_nPropertyCount = 0;
    m_pasProperties = NULL;

    m_nGeometryCount = 0;
    m_papsGeometry = m_apsGeometry;
    m_apsGeometry[0] = NULL;
    m_apsGeometry[1] = NULL;
    
    m_papszOBProperties = NULL;
}

/************************************************************************/
/*                            ~GMLFeature()                             */
/************************************************************************/

GMLFeature::~GMLFeature()

{
    CPLFree( m_pszFID );

    int i;
    for( i = 0; i < m_nPropertyCount; i++ )
    {
        int nSubProperties = m_pasProperties[i].nSubProperties;
        if (nSubProperties == 1)
            CPLFree( m_pasProperties[i].aszSubProperties[0] );
        else if (nSubProperties > 1)
        {
            for( int j = 0; j < nSubProperties; j++)
                CPLFree( m_pasProperties[i].papszSubProperties[j] );
            CPLFree( m_pasProperties[i].papszSubProperties );
        }
    }

    if (m_nGeometryCount == 1)
    {
        CPLDestroyXMLNode(m_apsGeometry[0]);
    }
    else if (m_nGeometryCount > 1)
    {
        for(i=0;i<m_nGeometryCount;i++)
            CPLDestroyXMLNode(m_papsGeometry[i]);
        CPLFree(m_papsGeometry);
    }

    CPLFree( m_pasProperties );
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
/*                        SetPropertyDirectly()                         */
/************************************************************************/

void GMLFeature::SetPropertyDirectly( int iIndex, char *pszValue )

{
    CPLAssert(pszValue);
    if( iIndex >= m_nPropertyCount )
    {
        int nClassPropertyCount = m_poClass->GetPropertyCount();
        m_pasProperties = (GMLProperty*)
            CPLRealloc( m_pasProperties,
                        sizeof(GMLProperty) * nClassPropertyCount );
        int i;
        for( i = 0; i < m_nPropertyCount; i ++ )
        {
            /* Make sure papszSubProperties point to the right address in case */
            /* m_pasProperties has been relocated */
            if (m_pasProperties[i].nSubProperties <= 1)
                m_pasProperties[i].papszSubProperties = m_pasProperties[i].aszSubProperties;
        }
        for( i = m_nPropertyCount; i < nClassPropertyCount; i++ )
        {
            m_pasProperties[i].nSubProperties = 0;
            m_pasProperties[i].papszSubProperties = m_pasProperties[i].aszSubProperties;
            m_pasProperties[i].aszSubProperties[0] = NULL;
            m_pasProperties[i].aszSubProperties[1] = NULL;
        }
        m_nPropertyCount = nClassPropertyCount;
    }

    GMLProperty* psProperty = &m_pasProperties[iIndex];
    int nSubProperties = psProperty->nSubProperties;
    if (nSubProperties == 0)
        psProperty->aszSubProperties[0] = pszValue;
    else if (nSubProperties == 1)
    {
        psProperty->papszSubProperties = (char**) CPLMalloc(
                            sizeof(char*) * (nSubProperties + 2) );
        psProperty->papszSubProperties[0] = psProperty->aszSubProperties[0];
        psProperty->aszSubProperties[0] = NULL;
        psProperty->papszSubProperties[nSubProperties] = pszValue;
        psProperty->papszSubProperties[nSubProperties + 1] = NULL;
    }
    else
    {
        psProperty->papszSubProperties = (char**) CPLRealloc(
                            psProperty->papszSubProperties,
                            sizeof(char*) * (nSubProperties + 2) );
        psProperty->papszSubProperties[nSubProperties] = pszValue;
        psProperty->papszSubProperties[nSubProperties + 1] = NULL;
    }
    psProperty->nSubProperties ++;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void GMLFeature::Dump( FILE * fp )

{
    printf( "GMLFeature(%s):\n", m_poClass->GetName() );
    
    if( m_pszFID != NULL )
        printf( "  FID = %s\n", m_pszFID );

    int i;
    for( i = 0; i < m_nPropertyCount; i++ )
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

    for(i=0;i<m_nGeometryCount;i++)
    {
        char* pszXML = CPLSerializeXMLTree(m_papsGeometry[i]);
        printf( "  %s\n", pszXML );
        CPLFree(pszXML);
    }
}

/************************************************************************/
/*                        SetGeometryDirectly()                         */
/************************************************************************/

void GMLFeature::SetGeometryDirectly( CPLXMLNode* psGeom )

{
    if (m_apsGeometry[0] != NULL)
        CPLDestroyXMLNode(m_apsGeometry[0]);
    m_nGeometryCount = 1;
    m_apsGeometry[0] = psGeom;
}

/************************************************************************/
/*                        SetGeometryDirectly()                         */
/************************************************************************/

void GMLFeature::SetGeometryDirectly( int nIdx, CPLXMLNode* psGeom )

{
    if( nIdx == 0 && m_nGeometryCount <= 1 )
    {
        SetGeometryDirectly( psGeom );
        return;
    }
    else if( m_nGeometryCount == 1 && nIdx > 0 )
    {
        m_papsGeometry = (CPLXMLNode **) CPLMalloc(2 * sizeof(CPLXMLNode *));
        m_papsGeometry[0] = m_apsGeometry[0];
        m_papsGeometry[1] = NULL;
        m_apsGeometry[0] = NULL;
    }

    if( nIdx >= m_nGeometryCount )
    {
        m_papsGeometry = (CPLXMLNode **) CPLRealloc(m_papsGeometry,
            (nIdx + 2) * sizeof(CPLXMLNode *));
        for( int i = m_nGeometryCount; i <= nIdx + 1; i++ )
            m_papsGeometry[i] = NULL;
        m_nGeometryCount = nIdx + 1;
    }
    if (m_papsGeometry[nIdx] != NULL)
        CPLDestroyXMLNode(m_papsGeometry[nIdx]);
    m_papsGeometry[nIdx] = psGeom;
}

/************************************************************************/
/*                          GetGeometryRef()                            */
/************************************************************************/

const CPLXMLNode* GMLFeature::GetGeometryRef( int nIdx ) const
{
    if( nIdx < 0 || nIdx >= m_nGeometryCount )
        return NULL;
    return m_papsGeometry[nIdx];
}

/************************************************************************/
/*                             AddGeometry()                            */
/************************************************************************/

void GMLFeature::AddGeometry( CPLXMLNode* psGeom )

{
    if (m_nGeometryCount == 0)
    {
        m_apsGeometry[0] = psGeom;
    }
    else if (m_nGeometryCount == 1)
    {
        m_papsGeometry = (CPLXMLNode **) CPLMalloc(
            (m_nGeometryCount + 2) * sizeof(CPLXMLNode *));
        m_papsGeometry[0] = m_apsGeometry[0];
        m_apsGeometry[0] = NULL;
        m_papsGeometry[m_nGeometryCount] = psGeom;
        m_papsGeometry[m_nGeometryCount + 1] = NULL;
    }
    else
    {
        m_papsGeometry = (CPLXMLNode **) CPLRealloc(m_papsGeometry,
            (m_nGeometryCount + 2) * sizeof(CPLXMLNode *));
        m_papsGeometry[m_nGeometryCount] = psGeom;
        m_papsGeometry[m_nGeometryCount + 1] = NULL;
    }
    m_nGeometryCount ++;
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
