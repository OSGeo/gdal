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
 * Revision 1.2  2002/01/24 17:37:33  warmerda
 * added to/from XML support
 *
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
    m_pszGeometryElement = NULL;
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
                poPDefn->SetType( GMLPT_String );
            else if( EQUAL(pszType,"Integer") )
                poPDefn->SetType( GMLPT_Integer );
            else if( EQUAL(pszType,"Real") )
                poPDefn->SetType( GMLPT_Real );
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
    CPLXMLNode	*psRoot, *psName;
    int		iProperty;

    psRoot = CPLCreateXMLNode( NULL, CXT_Element, "GMLFeatureClass" );

    CPLCreateXMLElementAndValue( psRoot, "Name", GetName() );
    CPLCreateXMLElementAndValue( psRoot, "ElementPath", GetElementName() );
    if( GetGeometryElement() != NULL && strlen(GetGeometryElement()) > 0 )
        CPLCreateXMLElementAndValue( psRoot, "GeometryElementPath", 
                                     GetGeometryElement() );
    
    for( iProperty = 0; iProperty < GetPropertyCount(); iProperty++ )
    {
        GMLPropertyDefn *poPDefn = GetProperty( iProperty );
        CPLXMLNode *psPDefnNode;
        const char *pszTypeName;

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
        }

        CPLCreateXMLElementAndValue( psPDefnNode, "Type", pszTypeName );
    }

    return psRoot;
}

