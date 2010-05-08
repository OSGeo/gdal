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
 ****************************************************************************/

#include "gmlreader.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          GMLFeatureClass()                           */
/************************************************************************/

GMLFeatureClass::GMLFeatureClass( const char *pszName )

{
    m_pszName = CPLStrdup( pszName );
    m_pszElementName = NULL;
    m_pszGeometryElement = NULL;
    m_nPropertyCount = 0;
    m_papoProperty = NULL;
    m_bSchemaLocked = FALSE;

    m_pszExtraInfo = NULL;
    m_bHaveExtents = FALSE;
    m_nFeatureCount = -1; // unknown

    m_nGeometryType = 0; // wkbUnknown
}

/************************************************************************/
/*                          ~GMLFeatureClass()                          */
/************************************************************************/

GMLFeatureClass::~GMLFeatureClass()

{
    CPLFree( m_pszName );
    CPLFree( m_pszElementName );
    CPLFree( m_pszGeometryElement );

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
        return m_pszElementName;
}

/************************************************************************/
/*                         SetGeometryElement()                         */
/************************************************************************/

void GMLFeatureClass::SetGeometryElement( const char *pszElement )

{
    CPLFree( m_pszGeometryElement );
    m_pszGeometryElement = CPLStrdup( pszElement );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int GMLFeatureClass::GetFeatureCount()

{
    return m_nFeatureCount;
}

/************************************************************************/
/*                          SetFeatureCount()                           */
/************************************************************************/

void GMLFeatureClass::SetFeatureCount( int nNewCount )

{
    m_nFeatureCount = nNewCount;
}

/************************************************************************/
/*                            GetExtraInfo()                            */
/************************************************************************/

const char *GMLFeatureClass::GetExtraInfo()

{
    return m_pszExtraInfo;
}

/************************************************************************/
/*                            SetExtraInfo()                            */
/************************************************************************/

void GMLFeatureClass::SetExtraInfo( const char *pszExtraInfo )

{
    CPLFree( m_pszExtraInfo );
    m_pszExtraInfo = NULL;

    if( pszExtraInfo != NULL )
        m_pszExtraInfo = CPLStrdup( pszExtraInfo );
}

/************************************************************************/
/*                             SetExtents()                             */
/************************************************************************/

void GMLFeatureClass::SetExtents( double dfXMin, double dfXMax, 
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

int GMLFeatureClass::GetExtents( double *pdfXMin, double *pdfXMax, 
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

int GMLFeatureClass::InitializeFromXML( CPLXMLNode *psRoot )

{
/* -------------------------------------------------------------------- */
/*      Do some rudimentary checking that this is a well formed         */
/*      node.                                                           */
/* -------------------------------------------------------------------- */
    if( psRoot == NULL 
        || psRoot->eType != CXT_Element 
        || !EQUAL(psRoot->pszValue,"GMLFeatureClass") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GMLFeatureClass::InitializeFromXML() called on %s node!",
                  psRoot->pszValue );
        return FALSE;
    }

    if( CPLGetXMLValue( psRoot, "Name", NULL ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GMLFeatureClass has no <Name> element." );
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

    if( CPLGetXMLValue( psRoot, "GeometryType", NULL ) != NULL )
    {
        SetGeometryType( atoi(CPLGetXMLValue( psRoot, "GeometryType", NULL )) );
    }

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
            SetExtents( CPLAtof(CPLGetXMLValue( psDSI, "ExtentXMin", "0.0" )),
                        CPLAtof(CPLGetXMLValue( psDSI, "ExtentXMax", "0.0" )),
                        CPLAtof(CPLGetXMLValue( psDSI, "ExtentYMin", "0.0" )),
                        CPLAtof(CPLGetXMLValue( psDSI, "ExtentYMax", "0.0" )) );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Collect property definitions.                                   */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psThis = psRoot->psChild;
         psThis != NULL; psThis = psThis->psNext )
    {
        if( EQUAL(psThis->pszValue, "PropertyDefn") )
        {
            const char *pszName = CPLGetXMLValue( psThis, "Name", NULL );
            const char *pszType = CPLGetXMLValue( psThis, "Type", "Untyped" );
            GMLPropertyDefn *poPDefn;

            if( pszName == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "GMLFeatureClass %s has a PropertyDefn without a <Name>..",
                          m_pszName );
                return FALSE;
            }

            poPDefn = new GMLPropertyDefn( 
                pszName, CPLGetXMLValue( psThis, "ElementPath", NULL ) );
            
            if( EQUAL(pszType,"Untyped") )
                poPDefn->SetType( GMLPT_Untyped );
            else if( EQUAL(pszType,"String") ) 
            {
                poPDefn->SetType( GMLPT_String );
                poPDefn->SetWidth( atoi( CPLGetXMLValue( psThis, "Width", "0" ) ) );
            }
            else if( EQUAL(pszType,"Integer") )
            {
                poPDefn->SetType( GMLPT_Integer );
                poPDefn->SetWidth( atoi( CPLGetXMLValue( psThis, "Width", "0" ) ) );
            }
            else if( EQUAL(pszType,"Real") )
            {
                poPDefn->SetType( GMLPT_Real );
                poPDefn->SetWidth( atoi( CPLGetXMLValue( psThis, "Width", "0" ) ) );
                poPDefn->SetPrecision( atoi( CPLGetXMLValue( psThis, "Precision", "0" ) ) );
            }
            else if( EQUAL(pszType,"StringList") ) 
                poPDefn->SetType( GMLPT_StringList );
            else if( EQUAL(pszType,"IntegerList") )
                poPDefn->SetType( GMLPT_IntegerList );
            else if( EQUAL(pszType,"RealList") )
                poPDefn->SetType( GMLPT_RealList );
            else if( EQUAL(pszType,"Complex") )
                poPDefn->SetType( GMLPT_Complex );
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unrecognised property type %s.", 
                          pszType );
                return FALSE;
            }

            AddProperty( poPDefn );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *GMLFeatureClass::SerializeToXML()

{
    CPLXMLNode  *psRoot;
    int         iProperty;

/* -------------------------------------------------------------------- */
/*      Set feature class and core information.                         */
/* -------------------------------------------------------------------- */
    psRoot = CPLCreateXMLNode( NULL, CXT_Element, "GMLFeatureClass" );

    CPLCreateXMLElementAndValue( psRoot, "Name", GetName() );
    CPLCreateXMLElementAndValue( psRoot, "ElementPath", GetElementName() );
    if( GetGeometryElement() != NULL && strlen(GetGeometryElement()) > 0 )
        CPLCreateXMLElementAndValue( psRoot, "GeometryElementPath", 
                                     GetGeometryElement() );
    
    if( GetGeometryType() != 0 /* wkbUnknown */ )
    {
        char szValue[128];

        sprintf( szValue, "%d", GetGeometryType() );
        CPLCreateXMLElementAndValue( psRoot, "GeometryType", szValue );
    }

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
    
/* -------------------------------------------------------------------- */
/*      emit property information.                                      */
/* -------------------------------------------------------------------- */
    for( iProperty = 0; iProperty < GetPropertyCount(); iProperty++ )
    {
        GMLPropertyDefn *poPDefn = GetProperty( iProperty );
        CPLXMLNode *psPDefnNode;
        const char *pszTypeName = "Unknown";

        psPDefnNode = CPLCreateXMLNode( psRoot, CXT_Element, "PropertyDefn" );
        CPLCreateXMLElementAndValue( psPDefnNode, "Name", 
                                     poPDefn->GetName() );
        CPLCreateXMLElementAndValue( psPDefnNode, "ElementPath", 
                                     poPDefn->GetSrcElement() );
        switch( poPDefn->GetType() )
        {
          case GMLPT_Untyped:
            pszTypeName = "Untyped";
            break;
            
          case GMLPT_String:
            pszTypeName = "String";
            break;
            
          case GMLPT_Integer:
            pszTypeName = "Integer";
            break;
            
          case GMLPT_Real:
            pszTypeName = "Real";
            break;
            
          case GMLPT_Complex:
            pszTypeName = "Complex";
            break;

          case GMLPT_IntegerList:
            pszTypeName = "IntegerList";
            break;

          case GMLPT_RealList:
            pszTypeName = "RealList";
            break;

          case GMLPT_StringList:
            pszTypeName = "StringList";
            break;
        }
        CPLCreateXMLElementAndValue( psPDefnNode, "Type", pszTypeName );

        if( EQUAL(pszTypeName,"String") )
        {
            char szMaxLength[48];
            sprintf(szMaxLength, "%d", poPDefn->GetWidth());
            CPLCreateXMLElementAndValue ( psPDefnNode, "Width", szMaxLength );
        }
        if( poPDefn->GetWidth() > 0 && EQUAL(pszTypeName,"Integer") )
        {
            char szLength[48];
            sprintf(szLength, "%d", poPDefn->GetWidth());
            CPLCreateXMLElementAndValue ( psPDefnNode, "Width", szLength );
        }
        if( poPDefn->GetWidth() > 0 && EQUAL(pszTypeName,"Real") )
        {
            char szLength[48];
            sprintf(szLength, "%d", poPDefn->GetWidth());
            CPLCreateXMLElementAndValue ( psPDefnNode, "Width", szLength );
            char szPrecision[48];
            sprintf(szPrecision, "%d", poPDefn->GetPrecision());
            CPLCreateXMLElementAndValue ( psPDefnNode, "Precision", szPrecision );
        }
    }

    return psRoot;
}

