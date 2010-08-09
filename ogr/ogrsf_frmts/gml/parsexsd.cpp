/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader::ParseXSD() method.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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
#include "cpl_error.h"

#if HAVE_XERCES != 0 || defined(HAVE_EXPAT)

#include "gmlreaderp.h"
#include "cpl_conv.h"

/************************************************************************/
/*                              StripNS()                               */
/*                                                                      */
/*      Return potentially shortened form of string with namespace      */
/*      stripped off if there is one.  Returns pointer into             */
/*      original string.                                                */
/************************************************************************/

const char *StripNS( const char *pszFullValue )

{
    const char *pszColon = strstr( pszFullValue, ":" );
    if( pszColon == NULL )
        return pszFullValue;
    else
        return pszColon + 1;
}

/************************************************************************/
/*                              ParseXSD()                              */
/************************************************************************/

int GMLReader::ParseXSD( const char *pszFile )

{
    if( pszFile == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Load the raw XML file.                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psXSDTree = CPLParseXMLFile( pszFile );
    
    if( psXSDTree == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Strip off any namespace qualifiers.                             */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psXSDTree, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Find <schema> root element.                                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psSchemaNode = CPLGetXMLNode( psXSDTree, "=schema" );
    if( psSchemaNode == NULL )
    {
        CPLDestroyXMLNode( psXSDTree );
        return FALSE;
    }

/* ==================================================================== */
/*      Process each feature class definition.                          */
/* ==================================================================== */
    CPLXMLNode *psThis;
    int         bIsLevel0 = TRUE;

    for( psThis = psSchemaNode->psChild; 
         psThis != NULL; psThis = psThis->psNext )
    {
/* -------------------------------------------------------------------- */
/*      Check for <xs:element> node.                                    */
/* -------------------------------------------------------------------- */
        if( psThis->eType != CXT_Element 
            || !EQUAL(psThis->pszValue,"element") )
            continue;

/* -------------------------------------------------------------------- */
/*      Check the substitution group.                                   */
/* -------------------------------------------------------------------- */
        const char *pszSubGroup = 
            StripNS(CPLGetXMLValue(psThis,"substitutionGroup",""));

        // Old OGR produced elements for the feature collection.
        if( EQUAL(pszSubGroup, "_FeatureCollection") )
            continue;

        if( !EQUAL(pszSubGroup, "_Feature") )
        {
            bIsLevel0 = FALSE;
            break;
        }
        
/* -------------------------------------------------------------------- */
/*      Get name                                                        */
/* -------------------------------------------------------------------- */
        const char *pszName;

        pszName = CPLGetXMLValue( psThis, "name", NULL );
        if( pszName == NULL )
        {
            bIsLevel0 = FALSE;
            break;
        }

/* -------------------------------------------------------------------- */
/*      Get type and verify relationship with name.                     */
/* -------------------------------------------------------------------- */
        const char *pszType;

        pszType = CPLGetXMLValue( psThis, "type", NULL );
        if( strstr( pszType, ":" ) != NULL )
            pszType = strstr( pszType, ":" ) + 1; 
        if( pszType == NULL || !EQUALN(pszType,pszName,strlen(pszName)) 
            || !(EQUAL(pszType+strlen(pszName),"_Type") ||
                    EQUAL(pszType+strlen(pszName),"Type")) )
        {
            bIsLevel0 = FALSE;
            break;
        }

/* -------------------------------------------------------------------- */
/*      The very next element should be the corresponding               */
/*      complexType declaration for the element.                        */
/* -------------------------------------------------------------------- */
        psThis = psThis->psNext;
        while( psThis != NULL && psThis->eType == CXT_Comment )
            psThis = psThis->psNext;

        if( psThis == NULL 
            || psThis->eType != CXT_Element
            || !EQUAL(psThis->pszValue,"complexType") 
            || !EQUAL(CPLGetXMLValue(psThis,"name",""),pszType) )
        {
            bIsLevel0 = FALSE;
            break;
        }

/* -------------------------------------------------------------------- */
/*      Grab the sequence of extensions greatgrandchild.                */
/* -------------------------------------------------------------------- */
        CPLXMLNode *psAttrSeq = 
            CPLGetXMLNode( psThis, 
                           "complexContent.extension.sequence" );

        if( psAttrSeq == NULL )
        {
            bIsLevel0 = FALSE;
            break;
        }

/* -------------------------------------------------------------------- */
/*      We are pretty sure this going to be a valid Feature class       */
/*      now, so create it.                                              */
/* -------------------------------------------------------------------- */
        GMLFeatureClass *poClass = new GMLFeatureClass( pszName );

/* -------------------------------------------------------------------- */
/*      Loop over each of the attribute elements being defined for      */
/*      this feature class.                                             */
/* -------------------------------------------------------------------- */
        CPLXMLNode *psAttrDef;

        for( psAttrDef = psAttrSeq->psChild; 
             psAttrDef != NULL; 
             psAttrDef = psAttrDef->psNext )
        {
            if( !EQUAL(psAttrDef->pszValue,"element") )
                continue;

            /* MapServer WFS writes element type as an attribute of element */
            /* not as a simpleType definition */
            const char* pszType = CPLGetXMLValue( psAttrDef, "type", NULL );
            if (pszType != NULL)
            {
                GMLPropertyType gmlType = GMLPT_Untyped;
                if (EQUAL(pszType, "string") ||
                    EQUAL(pszType, "Character"))
                    gmlType = GMLPT_String;
                else if (EQUAL(pszType, "Real"))
                    gmlType = GMLPT_Real;
                else if (EQUAL(pszType, "Integer"))
                    gmlType = GMLPT_Integer;

                if (gmlType != GMLPT_Untyped)
                {
                    GMLPropertyDefn *poProp = new GMLPropertyDefn(
                        CPLGetXMLValue( psAttrDef, "name", "unnamed" ),
                        CPLGetXMLValue( psAttrDef, "name", "unnamed" ) );

                    poProp->SetType( gmlType );
                    poClass->AddProperty( poProp );
                    continue;
                }
            }

            // For now we skip geometries .. fixup later.
            if( CPLGetXMLNode( psAttrDef, "simpleType" ) == NULL )
                continue;

            GMLPropertyDefn *poProp = new GMLPropertyDefn( 
                CPLGetXMLValue( psAttrDef, "name", "unnamed" ),
                CPLGetXMLValue( psAttrDef, "name", "unnamed" ) );

            const char *pszBase = 
                StripNS( CPLGetXMLValue( psAttrDef, 
                                         "simpleType.restriction.base", "" ));

            if( EQUAL(pszBase,"decimal") )
            {
                poProp->SetType( GMLPT_Real );
                const char *pszWidth = 
                    CPLGetXMLValue( psAttrDef, 
                              "simpleType.restriction.totalDigits.value", "0" );
                const char *pszPrecision = 
                    CPLGetXMLValue( psAttrDef, 
                              "simpleType.restriction.fractionDigits.value", "0" );
                poProp->SetWidth( atoi(pszWidth) );
                poProp->SetPrecision( atoi(pszPrecision) );
            }
            
            else if( EQUAL(pszBase,"float") 
                     || EQUAL(pszBase,"double") )
                poProp->SetType( GMLPT_Real );

            else if( EQUAL(pszBase,"integer") )
            {
                poProp->SetType( GMLPT_Integer );
                const char *pszWidth = 
                    CPLGetXMLValue( psAttrDef, 
                              "simpleType.restriction.totalDigits.value", "0" );
                poProp->SetWidth( atoi(pszWidth) );
            }

            else if( EQUAL(pszBase,"string") )
            {
                poProp->SetType( GMLPT_String );
                const char *pszWidth = 
                    CPLGetXMLValue( psAttrDef, 
                              "simpleType.restriction.maxLength.value", "0" );
                poProp->SetWidth( atoi(pszWidth) );
            }

            else
                poProp->SetType( GMLPT_Untyped );

            poClass->AddProperty( poProp );
        }

/* -------------------------------------------------------------------- */
/*      Class complete, add to reader class list.                       */
/* -------------------------------------------------------------------- */
        poClass->SetSchemaLocked( TRUE );

        AddClass( poClass );
    }

    CPLDestroyXMLNode( psXSDTree );

    if( m_nClassCount > 0 )
    {
        SetClassListLocked( TRUE );
        return TRUE;
    }
    else
        return FALSE;
}

#endif /* HAVE_XERCES == 1  || defined(HAVE_EXPAT) */

