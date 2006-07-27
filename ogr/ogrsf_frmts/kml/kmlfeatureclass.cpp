/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Implementation of KMLFeatureClass class.
 * Author:   Christopher Condit, condit@sdsc.edu
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
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
 * Revision 1.2  2006/07/27 19:53:01  mloskot
 * Added common file header to KML driver source files.
 *
 *
 */
#include "kmlreader.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          KMLFeatureClass()                           */
/************************************************************************/
KMLFeatureClass::KMLFeatureClass( const char *pszName )
{
    m_pszName = CPLStrdup( pszName );
    m_pszElementName = NULL;
    m_pszGeometryElement = NULL;
    m_nPropertyCount = 0;    
    m_bSchemaLocked = FALSE;

    m_pszExtraInfo = NULL;
    m_bHaveExtents = FALSE;
    m_nFeatureCount = -1; // unknown
}

/************************************************************************/
/*                          ~KMLFeatureClass()                          */
/************************************************************************/
KMLFeatureClass::~KMLFeatureClass()
{
    CPLFree( m_pszName );
    CPLFree( m_pszElementName );
    CPLFree( m_pszGeometryElement );
}

/************************************************************************/
/*                           SetElementName()                           */
/************************************************************************/
void KMLFeatureClass::SetElementName( const char *pszElementName )
{
    CPLFree( m_pszElementName );
    m_pszElementName = CPLStrdup( pszElementName );
}

/************************************************************************/
/*                           GetElementName()                           */
/************************************************************************/
const char *KMLFeatureClass::GetElementName() const
{
    if( m_pszElementName == NULL )
        return m_pszName;
    else
        return m_pszElementName;
}

/************************************************************************/
/*                         SetGeometryElement()                         */
/************************************************************************/
void KMLFeatureClass::SetGeometryElement( const char *pszElement )
{
    CPLFree( m_pszGeometryElement );
    m_pszGeometryElement = CPLStrdup( pszElement );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/
int KMLFeatureClass::GetFeatureCount() const
{
    return m_nFeatureCount;
}

/************************************************************************/
/*                          SetFeatureCount()                           */
/************************************************************************/
void KMLFeatureClass::SetFeatureCount( int nNewCount )
{
    m_nFeatureCount = nNewCount;
}

/************************************************************************/
/*                            GetExtraInfo()                            */
/************************************************************************/
const char *KMLFeatureClass::GetExtraInfo() const
{
    return m_pszExtraInfo;
}

/************************************************************************/
/*                            SetExtraInfo()                            */
/************************************************************************/
void KMLFeatureClass::SetExtraInfo( const char *pszExtraInfo )
{
    CPLFree( m_pszExtraInfo );
    m_pszExtraInfo = NULL;

    if( pszExtraInfo != NULL )
        m_pszExtraInfo = CPLStrdup( pszExtraInfo );
}

/************************************************************************/
/*                             SetExtents()                             */
/************************************************************************/
void KMLFeatureClass::SetExtents( double dfXMin, double dfXMax, 
                                  double dfYMin, double dfYMax )
{
    m_dfXMin = dfXMin;
    m_dfXMax = dfXMax;
    m_dfYMin = dfYMin;
    m_dfYMax = dfYMax;

    m_bHaveExtents = TRUE;
}

/************************************************************************/
/*                             GetExtents()                             */
/************************************************************************/
int KMLFeatureClass::GetExtents( double *pdfXMin, double *pdfXMax, 
                                 double *pdfYMin, double *pdfYMax )
{
    if( m_bHaveExtents )
    {
        *pdfXMin = m_dfXMin;
        *pdfXMax = m_dfXMax;
        *pdfYMin = m_dfYMin;
        *pdfYMax = m_dfYMax;
    }

    return m_bHaveExtents;
}

/************************************************************************/
/*                         InitializeFromXML()                          */
/************************************************************************/
int KMLFeatureClass::InitializeFromXML( CPLXMLNode *psRoot )
{
	/* -------------------------------------------------------------------- */
	/*      Do some rudimentary checking that this is a well formed         */
	/*      node.                                                           */
	/* -------------------------------------------------------------------- */
    if( psRoot == NULL 
        || psRoot->eType != CXT_Element 
        || !EQUAL(psRoot->pszValue,"Placemark") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "KMLFeatureClass::InitializeFromXML() called on %s node!",
                  psRoot->pszValue );
        return FALSE;
    }

    if( CPLGetXMLValue( psRoot, "Name", NULL ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "KMLFeatureClass has no <Name> element." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Collect base info.                                              */
/* -------------------------------------------------------------------- */
    CPLFree( m_pszName );
    m_pszName = CPLStrdup( CPLGetXMLValue( psRoot, "Name", NULL ) );
    
    SetElementName( CPLGetXMLValue( psRoot, "ElementPath", m_pszName ) );

    const char *pszGPath = CPLGetXMLValue( psRoot, "GeometryElementPath", "" );
    
    if( strlen( pszGPath ) > 0 )
        SetGeometryElement( pszGPath );

/* -------------------------------------------------------------------- */
/*      Collect dataset specific info.                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDSI = CPLGetXMLNode( psRoot, "DatasetSpecificInfo" );
    if( psDSI != NULL )
    {
        const char *pszValue;

        pszValue = CPLGetXMLValue( psDSI, "FeatureCount", NULL );
        if( pszValue != NULL )
            SetFeatureCount( atoi(pszValue) );

        // Eventually we should support XML subtrees.
        pszValue = CPLGetXMLValue( psDSI, "ExtraInfo", NULL );
        if( pszValue != NULL )
            SetExtraInfo( pszValue );

        if( CPLGetXMLValue( psDSI, "ExtentXMin", NULL ) != NULL 
            && CPLGetXMLValue( psDSI, "ExtentXMax", NULL ) != NULL
            && CPLGetXMLValue( psDSI, "ExtentYMin", NULL ) != NULL
            && CPLGetXMLValue( psDSI, "ExtentYMax", NULL ) != NULL )
        {
            SetExtents( atof(CPLGetXMLValue( psDSI, "ExtentXMin", "0.0" )),
                        atof(CPLGetXMLValue( psDSI, "ExtentXMax", "0.0" )),
                        atof(CPLGetXMLValue( psDSI, "ExtentYMin", "0.0" )),
                        atof(CPLGetXMLValue( psDSI, "ExtentYMax", "0.0" )) );
        }
    }    

    return TRUE;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/
CPLXMLNode *KMLFeatureClass::SerializeToXML()
{
    CPLXMLNode  *psRoot;
    int         iProperty;

/* -------------------------------------------------------------------- */
/*      Set feature class and core information.                         */
/* -------------------------------------------------------------------- */
    psRoot = CPLCreateXMLNode( NULL, CXT_Element, "KMLFeatureClass" );

    CPLCreateXMLElementAndValue( psRoot, "Name", GetName() );
    CPLCreateXMLElementAndValue( psRoot, "ElementPath", GetElementName() );
    if( GetGeometryElement() != NULL && strlen(GetGeometryElement()) > 0 )
        CPLCreateXMLElementAndValue( psRoot, "GeometryElementPath", 
                                     GetGeometryElement() );

/* -------------------------------------------------------------------- */
/*      Write out dataset specific information.                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDSI;

    if( m_bHaveExtents || m_nFeatureCount != -1 || m_pszExtraInfo != NULL )
    {
        psDSI = CPLCreateXMLNode( psRoot, CXT_Element, "DatasetSpecificInfo" );

        if( m_nFeatureCount != -1 )
        {
            char szValue[128];

            sprintf( szValue, "%d", m_nFeatureCount );
            CPLCreateXMLElementAndValue( psDSI, "FeatureCount", szValue );
        }

        if( m_bHaveExtents )
        {
            char szValue[128];

            sprintf( szValue, "%.5f", m_dfXMin );
            CPLCreateXMLElementAndValue( psDSI, "ExtentXMin", szValue );

            sprintf( szValue, "%.5f", m_dfXMax );
            CPLCreateXMLElementAndValue( psDSI, "ExtentXMax", szValue );

            sprintf( szValue, "%.5f", m_dfYMin );
            CPLCreateXMLElementAndValue( psDSI, "ExtentYMin", szValue );

            sprintf( szValue, "%.5f", m_dfYMax );
            CPLCreateXMLElementAndValue( psDSI, "ExtentYMax", szValue );
        }

        if( m_pszExtraInfo )
            CPLCreateXMLElementAndValue( psDSI, "ExtraInfo", m_pszExtraInfo );
    }    

    return psRoot;
}

