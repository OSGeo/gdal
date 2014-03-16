/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Code to translate between GML and OGR geometry forms.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 *****************************************************************************
 *
 * Independent Security Audit 2003/04/17 Andrey Kiselev:
 *   Completed audit of this module. All functions may be used without buffer
 *   overflows and stack corruptions with any kind of input data.
 *
 * Security Audit 2003/03/28 warmerda:
 *   Completed security audit.  I believe that this module may be safely used 
 *   to parse, arbitrary GML potentially provided by a hostile source without
 *   compromising the system.
 *
 */

#include "cpl_minixml.h"
#include "ogr_geometry.h"
#include "ogr_api.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include <ctype.h>
#include "ogr_p.h"

#ifndef PI
#define PI  3.14159265358979323846
#endif


/************************************************************************/
/*                        GMLGetCoordTokenPos()                         */
/************************************************************************/

static const char* GMLGetCoordTokenPos(const char* pszStr,
                                       const char** ppszNextToken)
{
    char ch;
    while(TRUE)
    {
        ch = *pszStr;
        if (ch == '\0')
        {
            *ppszNextToken = NULL;
            return NULL;
        }
        else if (!(ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ' || ch == ','))
            break;
        pszStr ++;
    }

    const char* pszToken = pszStr;
    while((ch = *pszStr) != '\0')
    {
        if (ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ' || ch == ',')
        {
            *ppszNextToken = pszStr;
            return pszToken;
        }
        pszStr ++;
    }
    *ppszNextToken = NULL;
    return pszToken;
}

/************************************************************************/
/*                           BareGMLElement()                           */
/*                                                                      */
/*      Returns the passed string with any namespace prefix             */
/*      stripped off.                                                   */
/************************************************************************/

static const char *BareGMLElement( const char *pszInput )

{
    const char *pszReturn;

    pszReturn = strchr( pszInput, ':' );
    if( pszReturn == NULL )
        pszReturn = pszInput;
    else
        pszReturn++;

    return pszReturn;
}

/************************************************************************/
/*                          FindBareXMLChild()                          */
/*                                                                      */
/*      Find a child node with the indicated "bare" name, that is       */
/*      after any namespace qualifiers have been stripped off.          */
/************************************************************************/

static const CPLXMLNode *FindBareXMLChild( const CPLXMLNode *psParent, 
                                           const char *pszBareName )

{
    const CPLXMLNode *psCandidate = psParent->psChild;

    while( psCandidate != NULL )
    {
        if( psCandidate->eType == CXT_Element
            && EQUAL(BareGMLElement(psCandidate->pszValue), pszBareName) )
            return psCandidate;

        psCandidate = psCandidate->psNext;
    }

    return NULL;
}

/************************************************************************/
/*                           GetElementText()                           */
/************************************************************************/

static const char *GetElementText( const CPLXMLNode *psElement )

{
    if( psElement == NULL )
        return NULL;

    const CPLXMLNode *psChild = psElement->psChild;

    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Text )
            return psChild->pszValue;

        psChild = psChild->psNext;
    }
    
    return NULL;
}

/************************************************************************/
/*                           GetChildElement()                          */
/************************************************************************/

static const CPLXMLNode *GetChildElement( const CPLXMLNode *psElement )

{
    if( psElement == NULL )
        return NULL;

    const CPLXMLNode *psChild = psElement->psChild;

    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Element )
            return psChild;

        psChild = psChild->psNext;
    }

    return NULL;
}

/************************************************************************/
/*                    GetElementOrientation()                           */
/*     Returns true for positive orientation.                           */
/************************************************************************/

int GetElementOrientation( const CPLXMLNode *psElement )
{
    if( psElement == NULL )
        return TRUE;

    const CPLXMLNode *psChild = psElement->psChild;

    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Attribute &&
            EQUAL(psChild->pszValue,"orientation") )
                return EQUAL(psChild->psChild->pszValue,"+");

        psChild = psChild->psNext;
    }
    
    return TRUE;
}

/************************************************************************/
/*                              AddPoint()                              */
/*                                                                      */
/*      Add a point to the passed geometry.                             */
/************************************************************************/

static int AddPoint( OGRGeometry *poGeometry, 
                     double dfX, double dfY, double dfZ, int nDimension )

{
    OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());
    if( eType == wkbPoint )
    {
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        if( !poPoint->IsEmpty() )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "More than one coordinate for <Point> element.");
            return FALSE;
        }
            
        poPoint->setX( dfX );
        poPoint->setY( dfY );
        if( nDimension == 3 )
            poPoint->setZ( dfZ );

        return TRUE;
    }
                
    else if( eType == wkbLineString )
    {
        if( nDimension == 3 )
            ((OGRLineString *) poGeometry)->addPoint( dfX, dfY, dfZ );
        else
            ((OGRLineString *) poGeometry)->addPoint( dfX, dfY );

        return TRUE;
    }

    else
    {
        CPLAssert( FALSE );
        return FALSE;                                                   
    }
}

/************************************************************************/
/*                        ParseGMLCoordinates()                         */
/************************************************************************/

static int ParseGMLCoordinates( const CPLXMLNode *psGeomNode, OGRGeometry *poGeometry )

{
    const CPLXMLNode *psCoordinates = FindBareXMLChild( psGeomNode, "coordinates" );
    int iCoord = 0;

/* -------------------------------------------------------------------- */
/*      Handle <coordinates> case.                                      */
/* -------------------------------------------------------------------- */
    if( psCoordinates != NULL )
    {
        const char *pszCoordString = GetElementText( psCoordinates );
        char chCS = ',';

        if( pszCoordString == NULL )
        {
            poGeometry->empty();
            return TRUE;
        }

        while( *pszCoordString != '\0' )
        {
            double dfX, dfY, dfZ = 0.0;
            int nDimension = 2;

            // parse out 2 or 3 tuple. 
            dfX = OGRFastAtof( pszCoordString );
            while( *pszCoordString != '\0'
                   && *pszCoordString != ','
                   && !isspace((unsigned char)*pszCoordString) )
                pszCoordString++;

            if( *pszCoordString == '\0' )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Corrupt <coordinates> value." );
                return FALSE;
            }
            else if( chCS == ',' && isspace((unsigned char)*pszCoordString) )
            {
                /* In theory, the coordinates inside a coordinate tuple should be */
                /* separated by a comma. However it has been found in the wild */
                /* that the coordinates are in rare cases separated by a space, and the tuples by a comma */
                /* See https://52north.org/twiki/bin/view/Processing/WPS-IDWExtension-ObservationCollectionExample */
                /* or http://agisdemo.faa.gov/aixmServices/getAllFeaturesByLocatorId?locatorId=DFW */
                chCS = ' ';
            }

            pszCoordString++;
            dfY = OGRFastAtof( pszCoordString );
            while( *pszCoordString != '\0' 
                   && *pszCoordString != ','
                   && !isspace((unsigned char)*pszCoordString) )
                pszCoordString++;

            if( *pszCoordString == chCS )
            {
                pszCoordString++;
                dfZ = OGRFastAtof( pszCoordString );
                nDimension = 3;
                while( *pszCoordString != '\0' 
                       && *pszCoordString != ','
                       && !isspace((unsigned char)*pszCoordString) )
                pszCoordString++;
            }

            if ( chCS == ' ' && *pszCoordString == ',' )
            {
                pszCoordString++;
            }

            while( isspace((unsigned char)*pszCoordString) )
                pszCoordString++;

            if( !AddPoint( poGeometry, dfX, dfY, dfZ, nDimension ) )
                return FALSE;

            iCoord++;
        }

        return iCoord > 0;
    }

/* -------------------------------------------------------------------- */
/*      Is this a "pos"?  GML 3 construct.                              */
/*      Parse if it exist a series of pos elements (this would allow    */
/*      the correct parsing of gml3.1.1 geomtries such as linestring    */
/*      defined with pos elements.                                      */
/* -------------------------------------------------------------------- */
    const CPLXMLNode *psPos;
    
    int bHasFoundPosElement = FALSE;
    for( psPos = psGeomNode->psChild;
         psPos != NULL;
         psPos = psPos->psNext )
    {
        if( psPos->eType != CXT_Element  )
            continue;

        const char* pszSubElement = BareGMLElement(psPos->pszValue);

        if( EQUAL(pszSubElement, "pointProperty") )
        {
            const CPLXMLNode *psPointPropertyIter;
            for( psPointPropertyIter = psPos->psChild;
                 psPointPropertyIter != NULL;
                 psPointPropertyIter = psPointPropertyIter->psNext )
            {
                if( psPointPropertyIter->eType != CXT_Element  )
                    continue;

                if (EQUAL(BareGMLElement(psPointPropertyIter->pszValue),"Point") )
                {
                    OGRPoint oPoint;
                    if( ParseGMLCoordinates( psPointPropertyIter, &oPoint ) )
                    {
                        int bSuccess = AddPoint( poGeometry, oPoint.getX(),
                                                 oPoint.getY(), oPoint.getZ(),
                                                 oPoint.getCoordinateDimension() );
                        if (bSuccess)
                            bHasFoundPosElement = TRUE;
                        else
                            return FALSE;
                    }
                }
            }
            continue;
        }

        if( !EQUAL(pszSubElement,"pos") )
            continue;

        const char* pszPos = GetElementText( psPos );
        if (pszPos == NULL)
        {
            poGeometry->empty();
            return TRUE;
        }

        const char* pszCur = pszPos;
        const char* pszX = (pszCur != NULL) ?
                            GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;
        const char* pszY = (pszCur != NULL) ?
                            GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;
        const char* pszZ = (pszCur != NULL) ?
                            GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;

        if (pszY == NULL)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Did not get 2+ values in <gml:pos>%s</gml:pos> tuple.",
                      pszPos ? pszPos : "" );
            return FALSE;
        }

        double dfX = OGRFastAtof(pszX);
        double dfY = OGRFastAtof(pszY);
        double dfZ = (pszZ != NULL) ? OGRFastAtof(pszZ) : 0.0;
        int bSuccess = AddPoint( poGeometry, dfX, dfY, dfZ, (pszZ != NULL) ? 3 : 2 );

        if (bSuccess)
            bHasFoundPosElement = TRUE;
        else
            return FALSE;
    }

    if (bHasFoundPosElement)
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Is this a "posList"?  GML 3 construct (SF profile).             */
/* -------------------------------------------------------------------- */
    const CPLXMLNode *psPosList = FindBareXMLChild( psGeomNode, "posList" );
    
    if( psPosList != NULL )
    {
        int bSuccess = FALSE;
        int nDimension = 2;

        /* Try to detect the presence of an srsDimension attribute */
        /* This attribute is only availabe for gml3.1.1 but not */
        /* available for gml3.1 SF*/
        const char* pszSRSDimension = CPLGetXMLValue( (CPLXMLNode*) psPosList, "srsDimension", NULL);
        /* If not found at the posList level, try on the enclosing element */
        if (pszSRSDimension == NULL)
            pszSRSDimension = CPLGetXMLValue( (CPLXMLNode*) psGeomNode, "srsDimension", NULL);
        if (pszSRSDimension != NULL)
            nDimension = atoi(pszSRSDimension);

        if (nDimension != 2 && nDimension != 3)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "srsDimension = %d not supported", nDimension);
            return FALSE;
        }

        const char* pszPosList = GetElementText( psPosList );
        if (pszPosList == NULL)
        {
            poGeometry->empty();
            return TRUE;
        }

        const char* pszCur = pszPosList;
        while (TRUE)
        {
            const char* pszX = GMLGetCoordTokenPos(pszCur, &pszCur);
            if (pszX == NULL && bSuccess)
                break;
            const char* pszY = (pszCur != NULL) ?
                    GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;
            const char* pszZ = (nDimension == 3 && pszCur != NULL) ?
                    GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;

            if (pszY == NULL || (nDimension == 3 && pszZ == NULL))
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Did not get at least %d values or invalid number of \n"
                        "set of coordinates <gml:posList>%s</gml:posList>",
                        nDimension, pszPosList ? pszPosList : "");
                return FALSE;
            }

            double dfX = OGRFastAtof(pszX);
            double dfY = OGRFastAtof(pszY);
            double dfZ = (pszZ != NULL) ? OGRFastAtof(pszZ) : 0.0;
            bSuccess = AddPoint( poGeometry, dfX, dfY, dfZ, nDimension );

            if (bSuccess == FALSE || pszCur == NULL)
                break;
        }

        return bSuccess;
    }
    

/* -------------------------------------------------------------------- */
/*      Handle form with a list of <coord> items each with an <X>,      */
/*      and <Y> element.                                                */
/* -------------------------------------------------------------------- */
    const CPLXMLNode *psCoordNode;

    for( psCoordNode = psGeomNode->psChild; 
         psCoordNode != NULL;
         psCoordNode = psCoordNode->psNext )
    {
        if( psCoordNode->eType != CXT_Element 
            || !EQUAL(BareGMLElement(psCoordNode->pszValue),"coord") )
            continue;

        const CPLXMLNode *psXNode, *psYNode, *psZNode;
        double dfX, dfY, dfZ = 0.0;
        int nDimension = 2;

        psXNode = FindBareXMLChild( psCoordNode, "X" );
        psYNode = FindBareXMLChild( psCoordNode, "Y" );
        psZNode = FindBareXMLChild( psCoordNode, "Z" );

        if( psXNode == NULL || psYNode == NULL 
            || GetElementText(psXNode) == NULL
            || GetElementText(psYNode) == NULL
            || (psZNode != NULL && GetElementText(psZNode) == NULL) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Corrupt <coord> element, missing <X> or <Y> element?" );
            return FALSE;
        }

        dfX = OGRFastAtof( GetElementText(psXNode) );
        dfY = OGRFastAtof( GetElementText(psYNode) );

        if( psZNode != NULL && GetElementText(psZNode) != NULL )
        {
            dfZ = OGRFastAtof( GetElementText(psZNode) );
            nDimension = 3;
        }

        if( !AddPoint( poGeometry, dfX, dfY, dfZ, nDimension ) )
            return FALSE;

        iCoord++;
    }

    return iCoord > 0.0;
}

#ifdef HAVE_GEOS
/************************************************************************/
/*                         GML2FaceExtRing()                            */
/*                                                                      */
/*      Identifies the "good" Polygon whithin the collection returned   */
/*      by GEOSPolygonize()                                             */
/*      short rationale: GEOSPolygonize() will possibily return a       */
/*      collection of many Polygons; only one is the "good" one,        */
/*      (including both exterior- and interior-rings)                   */
/*      any other simply represents a single "hole", and should be      */
/*      consequently ignored at all.                                    */
/************************************************************************/

static OGRPolygon *GML2FaceExtRing( OGRGeometry *poGeom )
{
    OGRPolygon *poPolygon = NULL;
    int bError = FALSE;
    OGRGeometryCollection *poColl = (OGRGeometryCollection *)poGeom;
    int iCount = poColl->getNumGeometries();
    int iExterior = 0;
    int iInterior = 0;

    for( int ig = 0; ig < iCount; ig++)
    {
        /* a collection of Polygons is expected to be found */
        OGRGeometry * poChild = (OGRGeometry*)poColl->getGeometryRef(ig);
        if( poChild == NULL)
        {
            bError = TRUE;
            continue;
        }
        if( wkbFlatten( poChild->getGeometryType()) == wkbPolygon )
        {
            OGRPolygon *poPg = (OGRPolygon *)poChild;
            if( poPg->getNumInteriorRings() > 0 )
                iExterior++;
            else
                iInterior++;
        }
        else
            bError = TRUE;
    }

    if( bError == FALSE && iCount > 0 )
    {
       if( iCount == 1 && iExterior == 0 && iInterior == 1)
        {
            /* there is a single Polygon within the collection */
            OGRPolygon * poPg = (OGRPolygon*)poColl->getGeometryRef(0 );
            poPolygon = (OGRPolygon *)poPg->clone();
        }
        else
        {
            if( iExterior == 1 && iInterior == iCount - 1 )
            {
                /* searching the unique Polygon containing holes */
                for ( int ig = 0; ig < iCount; ig++)
                {
                    OGRPolygon * poPg = (OGRPolygon*)poColl->getGeometryRef(ig);
                    if( poPg->getNumInteriorRings() > 0 )
                        poPolygon = (OGRPolygon *)poPg->clone();
                }
            }
        }
    }

    return poPolygon;
}
#endif

/************************************************************************/
/*                      GML2OGRGeometry_XMLNode()                       */
/*                                                                      */
/*      Translates the passed XMLnode and it's children into an         */
/*      OGRGeometry.  This is used recursively for geometry             */
/*      collections.                                                    */
/************************************************************************/

OGRGeometry *GML2OGRGeometry_XMLNode( const CPLXMLNode *psNode,
                                      int bGetSecondaryGeometryOption,
                                      int nRecLevel,
                                      int bIgnoreGSG,
                                      int bOrientation,
                                      int bFaceHoleNegative ) 

{
    const char *pszBaseGeometry = BareGMLElement( psNode->pszValue );
    if (bGetSecondaryGeometryOption < 0)
        bGetSecondaryGeometryOption = CSLTestBoolean(CPLGetConfigOption("GML_GET_SECONDARY_GEOM", "NO"));
    int bGetSecondaryGeometry = bIgnoreGSG ? FALSE : bGetSecondaryGeometryOption;

    /* Arbitrary value, but certainly large enough for reasonable usages ! */
    if( nRecLevel == 32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Too many recursion levels (%d) while parsing GML geometry.",
                    nRecLevel );
        return NULL;
    }

    if( bGetSecondaryGeometry )
        if( !( EQUAL(pszBaseGeometry,"directedEdge") ||
               EQUAL(pszBaseGeometry,"TopoCurve") ) )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Polygon / PolygonPatch / Triangle / Rectangle                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Polygon") ||
        EQUAL(pszBaseGeometry,"PolygonPatch") ||
        EQUAL(pszBaseGeometry,"Triangle") ||
        EQUAL(pszBaseGeometry,"Rectangle"))
    {
        const CPLXMLNode *psChild;
        OGRPolygon *poPolygon = new OGRPolygon();
        OGRLinearRing *poRing;

        // Find outer ring.
        psChild = FindBareXMLChild( psNode, "outerBoundaryIs" );
        if (psChild == NULL)
           psChild = FindBareXMLChild( psNode, "exterior");

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
        {
            /* <gml:Polygon/> is invalid GML2, but valid GML3, so be tolerant */
            return poPolygon;
        }

        // Translate outer ring and add to polygon.
        poRing = (OGRLinearRing *) 
            GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                     nRecLevel + 1 );
        if( poRing == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            delete poPolygon;
            return NULL;
        }

        if( !EQUAL(poRing->getGeometryName(),"LINEARRING") )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "%s: Got %.500s geometry as outerBoundaryIs instead of LINEARRING.",
                      pszBaseGeometry, poRing->getGeometryName() );
            delete poPolygon;
            delete poRing;
            return NULL;
        }

        poPolygon->addRingDirectly( poRing );

        // Find all inner rings 
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue),"innerBoundaryIs") ||
                    EQUAL(BareGMLElement(psChild->pszValue),"interior")))
            {
                const CPLXMLNode* psInteriorChild = GetChildElement(psChild);
                if (psInteriorChild != NULL)
                    poRing = (OGRLinearRing *) 
                        GML2OGRGeometry_XMLNode( psInteriorChild, bGetSecondaryGeometryOption,
                                                 nRecLevel + 1);
                else
                    poRing = NULL;
                if (poRing == NULL)
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Invalid interior ring");
                    delete poPolygon;
                    return NULL;
                }
                if( !EQUAL(poRing->getGeometryName(),"LINEARRING") )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "%s: Got %.500s geometry as innerBoundaryIs instead of LINEARRING.",
                              pszBaseGeometry, poRing->getGeometryName() );
                    delete poPolygon;
                    delete poRing;
                    return NULL;
                }

                poPolygon->addRingDirectly( poRing );
            }
        }

        return poPolygon;
    }

/* -------------------------------------------------------------------- */
/*      LinearRing                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"LinearRing") )
    {
        OGRLinearRing   *poLinearRing = new OGRLinearRing();
        
        if( !ParseGMLCoordinates( psNode, poLinearRing ) )
        {
            delete poLinearRing;
            return NULL;
        }

        return poLinearRing;
    }

/* -------------------------------------------------------------------- */
/*      Ring GML3                                                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Ring") )
    {
        OGRLinearRing   *poLinearRing = new OGRLinearRing();
        const CPLXMLNode *psChild;

        for( psChild = psNode->psChild; 
             psChild != NULL; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"curveMember") )
            {
                const CPLXMLNode* psCurveChild = GetChildElement(psChild);
                OGRGeometry* poGeom;
                if (psCurveChild != NULL)
                    poGeom =
                        GML2OGRGeometry_XMLNode( psCurveChild, bGetSecondaryGeometryOption,
                                                 nRecLevel + 1);
                else
                    poGeom = NULL;

                // try to join multiline string to one linestring
                if( poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString )
                {
                    poGeom = OGRGeometryFactory::forceToLineString( poGeom, false );
                }

                if( poGeom == NULL 
                    || wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                {
                    delete poGeom;
                    delete poLinearRing;
                    return NULL;
                }

                OGRLineString *poLS = (OGRLineString *) poGeom;
                if( poLS->getNumPoints() < 2 )
                {
                    // skip it
                }
                else if( poLinearRing->getNumPoints() > 0
                    && fabs(poLinearRing->getX(poLinearRing->getNumPoints()-1) - poLS->getX(0)) < 1e-14
                    && fabs(poLinearRing->getY(poLinearRing->getNumPoints()-1) - poLS->getY(0)) < 1e-14
                    && fabs(poLinearRing->getZ(poLinearRing->getNumPoints()-1) - poLS->getZ(0)) < 1e-14 )
                {
                    // Skip the first point of the new linestring to avoid
                    // invalidate duplicate points
                    poLinearRing->addSubLineString( poLS, 1 );
                }
                else
                {
                    // Add the whole new line string
                    poLinearRing->addSubLineString( poLS );
                }

                delete poLS;
            }
        }

        return poLinearRing;
    }

/* -------------------------------------------------------------------- */
/*      LineString                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"LineString")
        || EQUAL(pszBaseGeometry,"LineStringSegment")
        || EQUAL(pszBaseGeometry,"GeodesicString") )
    {
        OGRLineString   *poLine = new OGRLineString();
        
        if( !ParseGMLCoordinates( psNode, poLine ) )
        {
            delete poLine;
            return NULL;
        }

        return poLine;
    }

/* -------------------------------------------------------------------- */
/*      Arc/Circle : we approximate them by linear segments             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Arc") ||
        EQUAL(pszBaseGeometry,"Circle") )
    {
        OGRLineString   *poLine = new OGRLineString();

        if( !ParseGMLCoordinates( psNode, poLine ) ||
            poLine->getNumPoints() != 3 )
        {
            delete poLine;
            return NULL;
        }

        double x0 = poLine->getX(0);
        double y0 = poLine->getY(0);
        double x1 = poLine->getX(1);
        double y1 = poLine->getY(1);
        double x2 = poLine->getX(2);
        double y2 = poLine->getY(2);
        double dx01 = x1 - x0;
        double dy01 = y1 - y0;
        double dx12 = x2 - x1;
        double dy12 = y2 - y1;
        double c01 = dx01 * (x0 + x1) / 2 + dy01 * (y0 + y1) / 2;
        double c12 = dx12 * (x1 + x2) / 2 + dy12 * (y1 + y2) / 2;
        double det = dx01 * dy12 - dx12 * dy01;
        if (det == 0)
        {
            return poLine;
        }
        double cx =  (c01 * dy12 - c12 * dy01) / det;
        double cy =  (- c01 * dx12 + c12 * dx01) / det;

        double alpha0 = atan2(y0 - cy, x0 - cx);
        double alpha1 = atan2(y1 - cy, x1 - cx);
        double alpha2 = atan2(y2 - cy, x2 - cx);
        double alpha3;
        double R = sqrt((x0 - cx) * (x0 - cx) + (y0 - cy) * (y0 - cy));

        /* if det is negative, the orientation if clockwise */
        if (det < 0)
        {
            if (alpha1 > alpha0)
                alpha1 -= 2 * PI;
            if (alpha2 > alpha1)
                alpha2 -= 2 * PI;
            alpha3 = alpha0 - 2 * PI;
        }
        else
        {
            if (alpha1 < alpha0)
                alpha1 += 2 * PI;
            if (alpha2 < alpha1)
                alpha2 += 2 * PI;
            alpha3 = alpha0 + 2 * PI;
        }

        CPLAssert((alpha0 <= alpha1 && alpha1 <= alpha2 && alpha2 <= alpha3) ||
                  (alpha0 >= alpha1 && alpha1 >= alpha2 && alpha2 >= alpha3));

        int nSign = (det >= 0) ? 1 : -1;

        double alpha, dfRemainder;
        double dfStep = atof(CPLGetConfigOption("OGR_ARC_STEPSIZE","4")) / 180 * PI;

        // make sure the segments are not too short
        double dfMinStepLength = atof( CPLGetConfigOption("OGR_ARC_MINLENGTH","0") );
        if ( dfMinStepLength > 0.0 && dfStep * R < dfMinStepLength )
        {
            CPLDebug( "GML", "Increasing arc step to %lf° (was %lf° with segment length %lf at radius %lf; min segment length is %lf)",
                      dfMinStepLength * 180.0 / PI / R,
                      dfStep * 180.0 / PI,
                      dfStep * R,
                      R,
                      dfMinStepLength );
            dfStep = dfMinStepLength / R;
        }

        if (dfStep < 4. / 180 * PI)
        {
            CPLDebug( "GML", "Increasing arc step to %lf° (was %lf° with length %lf at radius %lf).",
                      4. / 180 * PI,
                      dfStep * 180.0 / PI,
                      dfStep * R,
                      R );
            dfStep = 4. / 180 * PI;
        }

        poLine->setNumPoints(0);

        dfStep *= nSign;

        dfRemainder = fmod(alpha1 - alpha0, dfStep) / 2.0;

        poLine->addPoint(x0, y0);

        for(alpha = alpha0 + dfStep + dfRemainder; (alpha + dfRemainder - alpha1) * nSign < 0; alpha += dfStep)
        {
            poLine->addPoint(cx + R * cos(alpha), cy + R * sin(alpha));
        }

        poLine->addPoint(x1, y1);

        dfRemainder = fmod(alpha2 - alpha1, dfStep) / 2.0;

        for(alpha = alpha1 + dfStep + dfRemainder; (alpha + dfRemainder - alpha2) * nSign < 0; alpha += dfStep)
        {
            poLine->addPoint(cx + R * cos(alpha), cy + R * sin(alpha));
        }

        if (EQUAL(pszBaseGeometry,"Circle"))
        {
            for(alpha = alpha2; (alpha - alpha3) * nSign < 0; alpha += dfStep)
            {
                poLine->addPoint(cx + R * cos(alpha), cy + R * sin(alpha));
            }
            poLine->addPoint(x0, y0);
        }
        else
        {
            poLine->addPoint(x2, y2);
        }

        return poLine;
    }

/* -------------------------------------------------------------------- */
/*      PointType                                                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"PointType") 
        || EQUAL(pszBaseGeometry,"Point")
        || EQUAL(pszBaseGeometry,"ConnectionPoint") )
    {
        OGRPoint *poPoint = new OGRPoint();
        
        if( !ParseGMLCoordinates( psNode, poPoint ) )
        {
            delete poPoint;
            return NULL;
        }

        return poPoint;
    }

/* -------------------------------------------------------------------- */
/*      Box                                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"BoxType") || EQUAL(pszBaseGeometry,"Box") )
    {
        OGRLineString  oPoints;

        if( !ParseGMLCoordinates( psNode, &oPoints ) )
            return NULL;

        if( oPoints.getNumPoints() < 2 )
            return NULL;

        OGRLinearRing *poBoxRing = new OGRLinearRing();
        OGRPolygon *poBoxPoly = new OGRPolygon();

        poBoxRing->setNumPoints( 5 );
        poBoxRing->setPoint( 
            0, oPoints.getX(0), oPoints.getY(0), oPoints.getZ(0) );
        poBoxRing->setPoint( 
            1, oPoints.getX(1), oPoints.getY(0), oPoints.getZ(0) );
        poBoxRing->setPoint( 
            2, oPoints.getX(1), oPoints.getY(1), oPoints.getZ(1) );
        poBoxRing->setPoint( 
            3, oPoints.getX(0), oPoints.getY(1), oPoints.getZ(0) );
        poBoxRing->setPoint( 
            4, oPoints.getX(0), oPoints.getY(0), oPoints.getZ(0) );

        poBoxPoly->addRingDirectly( poBoxRing );

        return poBoxPoly;
    }

/* -------------------------------------------------------------------- */
/*      Envelope                                                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Envelope") )
    {
        const CPLXMLNode* psLowerCorner = FindBareXMLChild( psNode, "lowerCorner");
        const CPLXMLNode* psUpperCorner = FindBareXMLChild( psNode, "upperCorner");
        if( psLowerCorner == NULL || psUpperCorner == NULL )
            return NULL;
        const char* pszLowerCorner = GetElementText(psLowerCorner);
        const char* pszUpperCorner = GetElementText(psUpperCorner);
        if( pszLowerCorner == NULL || pszUpperCorner == NULL )
            return NULL;
        char** papszLowerCorner = CSLTokenizeString(pszLowerCorner);
        char** papszUpperCorner = CSLTokenizeString(pszUpperCorner);
        int nTokenCountLC = CSLCount(papszLowerCorner);
        int nTokenCountUC = CSLCount(papszUpperCorner);
        if( nTokenCountLC < 2 || nTokenCountUC < 2 )
        {
            CSLDestroy(papszLowerCorner);
            CSLDestroy(papszUpperCorner);
            return NULL;
        }
        
        double dfLLX = CPLAtof(papszLowerCorner[0]);
        double dfLLY = CPLAtof(papszLowerCorner[1]);
        double dfURX = CPLAtof(papszUpperCorner[0]);
        double dfURY = CPLAtof(papszUpperCorner[1]);
        CSLDestroy(papszLowerCorner);
        CSLDestroy(papszUpperCorner);

        OGRLinearRing *poEnvelopeRing = new OGRLinearRing();
        OGRPolygon *poPoly = new OGRPolygon();

        poEnvelopeRing->setNumPoints( 5 );
        poEnvelopeRing->setPoint(0, dfLLX, dfLLY);
        poEnvelopeRing->setPoint(1, dfURX, dfLLY);
        poEnvelopeRing->setPoint(2, dfURX, dfURY);
        poEnvelopeRing->setPoint(3, dfLLX, dfURY);
        poEnvelopeRing->setPoint(4, dfLLX, dfLLY);
        poPoly->addRingDirectly(poEnvelopeRing );

        return poPoly;
    }

/* ------------------------const CPLXMLNode *psChild;-------------------------------------------- */
/*      MultiPolygon / MultiSurface / CompositeSurface                  */
/*                                                                      */
/* For CompositeSurface, this is a very rough approximation to deal with*/
/* it as a MultiPolygon, because it can several faces of a 3D volume... */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"MultiPolygon") ||
        EQUAL(pszBaseGeometry,"MultiSurface") ||
        EQUAL(pszBaseGeometry,"CompositeSurface") )
    {
        const CPLXMLNode *psChild;
        OGRMultiPolygon *poMPoly = new OGRMultiPolygon();

        // Iterate over children
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue),"polygonMember") ||
                    EQUAL(BareGMLElement(psChild->pszValue),"surfaceMember")) )
            {
                const CPLXMLNode* psSurfaceChild = GetChildElement(psChild);
                OGRPolygon *poPolygon;

                if (psSurfaceChild != NULL)
                    poPolygon = (OGRPolygon *) 
                        GML2OGRGeometry_XMLNode( psSurfaceChild, bGetSecondaryGeometryOption,
                                                 nRecLevel + 1);
                else
                    poPolygon = NULL;

                if( poPolygon == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Invalid %s",
                              BareGMLElement(psChild->pszValue));
                    delete poMPoly;
                    return NULL;
                }

                if( !EQUAL(poPolygon->getGeometryName(),"POLYGON") )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Got %.500s geometry as polygonMember instead of MULTIPOLYGON.",
                              poPolygon->getGeometryName() );
                    delete poPolygon;
                    delete poMPoly;
                    return NULL;
                }

                poMPoly->addGeometryDirectly( poPolygon );
            }
            else if (psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"surfaceMembers") )
            {
                const CPLXMLNode *psChild2;
                for( psChild2 = psChild->psChild;
                     psChild2 != NULL;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element
                        && (EQUAL(BareGMLElement(psChild2->pszValue),"Surface") ||
                            EQUAL(BareGMLElement(psChild2->pszValue),"Polygon") ||
                            EQUAL(BareGMLElement(psChild2->pszValue),"PolygonPatch") ||
                            EQUAL(BareGMLElement(psChild2->pszValue),"CompositeSurface")) )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode( psChild2, bGetSecondaryGeometryOption,
                                                                       nRecLevel + 1);
                        if (poGeom == NULL)
                        {
                            CPLError( CE_Failure, CPLE_AppDefined, "Invalid %s",
                                    BareGMLElement(psChild2->pszValue));
                            delete poMPoly;
                            return NULL;
                        }

                        if (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
                        {
                            poMPoly->addGeometryDirectly( (OGRPolygon*) poGeom );
                        }
                        else if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
                        {
                            OGRMultiPolygon* poMPoly2 = (OGRMultiPolygon*) poGeom;
                            int i;
                            for(i=0;i<poMPoly2->getNumGeometries();i++)
                            {
                                poMPoly->addGeometry(poMPoly2->getGeometryRef(i));
                            }
                            delete poGeom;
                        }
                        else
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                    "Got %.500s geometry as polygonMember instead of POLYGON/MULTIPOLYGON.",
                                    poGeom->getGeometryName() );
                            delete poGeom;
                            delete poMPoly;
                            return NULL;
                        }
                    }
                }
            }
        }

        return poMPoly;
    }

/* -------------------------------------------------------------------- */
/*      MultiPoint                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"MultiPoint") )
    {
        const CPLXMLNode *psChild;
        OGRMultiPoint *poMP = new OGRMultiPoint();

        // collect points.
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"pointMember") )
            {
                const CPLXMLNode* psPointChild = GetChildElement(psChild);
                OGRPoint *poPoint;

                if (psPointChild != NULL)
                    poPoint = (OGRPoint *) 
                        GML2OGRGeometry_XMLNode( psPointChild, bGetSecondaryGeometryOption,
                                                 nRecLevel + 1);
                else
                    poPoint = NULL;
                if( poPoint == NULL 
                    || wkbFlatten(poPoint->getGeometryType()) != wkbPoint )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "MultiPoint: Got %.500s geometry as pointMember instead of POINT",
                              poPoint ? poPoint->getGeometryName() : "NULL" );
                    delete poPoint;
                    delete poMP;
                    return NULL;
                }

                poMP->addGeometryDirectly( poPoint );
            }
            else if (psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"pointMembers") )
            {
                const CPLXMLNode *psChild2;
                for( psChild2 = psChild->psChild;
                     psChild2 != NULL;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element
                        && (EQUAL(BareGMLElement(psChild2->pszValue),"Point")) )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode( psChild2, bGetSecondaryGeometryOption,
                                                                       nRecLevel + 1);
                        if (poGeom == NULL)
                        {
                            CPLError( CE_Failure, CPLE_AppDefined, "Invalid %s",
                                    BareGMLElement(psChild2->pszValue));
                            delete poMP;
                            return NULL;
                        }

                        if (wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
                        {
                            poMP->addGeometryDirectly( (OGRPoint *)poGeom );
                        }
                        else
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                    "Got %.500s geometry as pointMember instead of POINT.",
                                    poGeom->getGeometryName() );
                            delete poGeom;
                            delete poMP;
                            return NULL;
                        }
                    }
                }
            }
        }

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      MultiLineString                                                 */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"MultiLineString") )
    {
        const CPLXMLNode *psChild;
        OGRMultiLineString *poMLS = new OGRMultiLineString();

        // collect lines
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"lineStringMember") )
            {
                const CPLXMLNode* psLineStringChild = GetChildElement(psChild);
                OGRGeometry *poGeom;

                if (psLineStringChild != NULL)
                    poGeom = GML2OGRGeometry_XMLNode( psLineStringChild, bGetSecondaryGeometryOption,
                                                      nRecLevel + 1);
                else
                    poGeom = NULL;
                if( poGeom == NULL 
                    || wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "MultiLineString: Got %.500s geometry as Member instead of LINESTRING.",
                              poGeom ? poGeom->getGeometryName() : "NULL" );
                    delete poGeom;
                    delete poMLS;
                    return NULL;
                }

                poMLS->addGeometryDirectly( poGeom );
            }
        }

        return poMLS;
    }


/* -------------------------------------------------------------------- */
/*      MultiCurve / CompositeCurve                                     */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"MultiCurve") ||
        EQUAL(pszBaseGeometry,"CompositeCurve") )
    {
        const CPLXMLNode *psChild, *psCurve;
        OGRMultiLineString *poMLS = new OGRMultiLineString();

        // collect curveMembers
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"curveMember") )
            {
                OGRGeometry *poGeom;

                // There can be only one curve under a curveMember.
                // Currently "Curve" and "LineString" are handled.
                psCurve = FindBareXMLChild( psChild, "Curve" );
                if( psCurve == NULL )
                    psCurve = FindBareXMLChild( psChild, "LineString" );
                if( psCurve == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Failed to get curve element in curveMember" );
                    delete poMLS;
                    return NULL;
                }
                poGeom = GML2OGRGeometry_XMLNode( psCurve, bGetSecondaryGeometryOption,
                                                  nRecLevel + 1);
                if( poGeom == NULL ||
                    ( wkbFlatten(poGeom->getGeometryType()) != wkbLineString ) )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "MultiCurve: Got %.500s geometry as Member instead of LINESTRING.",
                              poGeom ? poGeom->getGeometryName() : "NULL" );
                    if( poGeom != NULL ) delete poGeom;
                    delete poMLS;
                    return NULL;
                }

                poMLS->addGeometryDirectly( (OGRLineString *)poGeom );
            }
            else if (psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"curveMembers") )
            {
                const CPLXMLNode *psChild2;
                for( psChild2 = psChild->psChild;
                     psChild2 != NULL;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element
                        && (EQUAL(BareGMLElement(psChild2->pszValue),"LineString")) )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode( psChild2, bGetSecondaryGeometryOption,
                                                                       nRecLevel + 1);
                        if (poGeom == NULL)
                        {
                            CPLError( CE_Failure, CPLE_AppDefined, "Invalid %s",
                                    BareGMLElement(psChild2->pszValue));
                            delete poMLS;
                            return NULL;
                        }

                        if (wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
                        {
                            poMLS->addGeometryDirectly( (OGRLineString *)poGeom );
                        }
                        else
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                    "Got %.500s geometry as curveMember instead of LINESTRING.",
                                    poGeom->getGeometryName() );
                            delete poGeom;
                            delete poMLS;
                            return NULL;
                        }
                    }
                }
            }
        }
        return poMLS;
    }

/* -------------------------------------------------------------------- */
/*      Curve                                                           */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Curve") )
    {
        const CPLXMLNode *psChild;

        psChild = FindBareXMLChild( psNode, "segments");
        if( psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GML3 Curve geometry lacks segments element." );
            return NULL;
        }

        OGRGeometry *poGeom;

        poGeom = GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                          nRecLevel + 1);
        if( poGeom == NULL ||
            wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                "Curve: Got %.500s geometry as Member instead of segments.",
                poGeom ? poGeom->getGeometryName() : "NULL" );
            if( poGeom != NULL ) delete poGeom;
            return NULL;
        }

        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      segments                                                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"segments") )
    {
        const CPLXMLNode *psChild;
        OGRLineString *poLS = new OGRLineString();

        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 

        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue),"LineStringSegment") ||
                    EQUAL(BareGMLElement(psChild->pszValue),"GeodesicString") ||
                    EQUAL(BareGMLElement(psChild->pszValue),"Arc") ||
                    EQUAL(BareGMLElement(psChild->pszValue),"Circle")) )
            {
                OGRGeometry *poGeom;

                poGeom = GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                                  nRecLevel + 1);
                if( poGeom != NULL &&
                    wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "segments: Got %.500s geometry as Member instead of LINESTRING.",
                              poGeom ? poGeom->getGeometryName() : "NULL" );
                    delete poGeom;
                    delete poLS;
                    return NULL;
                }
                if( poGeom != NULL )
                {
                    OGRLineString *poAddLS = (OGRLineString *)poGeom;
                    if( poLS->getNumPoints() > 0 && poAddLS->getNumPoints() > 0
                        && fabs(poLS->getX(poLS->getNumPoints()-1)
                                - poAddLS->getX(0)) < 1e-14
                        && fabs(poLS->getY(poLS->getNumPoints()-1)
                                - poAddLS->getY(0)) < 1e-14
                        && fabs(poLS->getZ(poLS->getNumPoints()-1)
                                - poAddLS->getZ(0)) < 1e-14) 
                    {
                        // Skip the first point of the new linestring to avoid
                        // invalidate duplicate points (#4451)
                        poLS->addSubLineString( poAddLS, 1 );
                    }
                    else
                    {
                        // Add the whole new line string
                        poLS->addSubLineString( poAddLS );
                    }
                    delete poGeom;
                }
            }
        }

        return poLS;
    }

/* -------------------------------------------------------------------- */
/*      MultiGeometry                                                   */
/* CAUTION: OGR < 1.8.0 produced GML with GeometryCollection, which is  */
/* not a valid GML 2 keyword! The right name is MultiGeometry. Let's be */
/* tolerant with the non compliant files we produced...                 */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"MultiGeometry") ||
        EQUAL(pszBaseGeometry,"GeometryCollection") )
    {
        const CPLXMLNode *psChild;
        OGRGeometryCollection *poGC = new OGRGeometryCollection();

        // collect geoms
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"geometryMember") )
            {
                const CPLXMLNode* psGeometryChild = GetChildElement(psChild);
                OGRGeometry *poGeom;

                if (psGeometryChild != NULL)
                    poGeom = GML2OGRGeometry_XMLNode( psGeometryChild, bGetSecondaryGeometryOption,
                                                      nRecLevel + 1 );
                else
                    poGeom = NULL;
                if( poGeom == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "GeometryCollection: Failed to get geometry in geometryMember" );
                    delete poGeom;
                    delete poGC;
                    return NULL;
                }

                poGC->addGeometryDirectly( poGeom );
            }
        }

        return poGC;
    }

/* -------------------------------------------------------------------- */
/*      Directed Edge                                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"directedEdge") )
    {
        const CPLXMLNode *psEdge,
                         *psdirectedNode,
                         *psNodeElement,
                         *pspointProperty,
                         *psPoint,
                         *psCurveProperty,
                         *psCurve;
        int               bEdgeOrientation = TRUE,
                          bNodeOrientation = TRUE;
        OGRGeometry      *poGeom;
        OGRLineString    *poLineString;
        OGRPoint         *poPositiveNode = NULL, *poNegativeNode = NULL;
        OGRMultiPoint    *poMP;
    
        bEdgeOrientation = GetElementOrientation(psNode);

        //collect edge
        psEdge = FindBareXMLChild(psNode,"Edge");
        if( psEdge == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to get Edge element in directedEdge" );
            return NULL;
        }

        if( bGetSecondaryGeometry )
        {
            psdirectedNode = FindBareXMLChild(psEdge,"directedNode");
            if( psdirectedNode == NULL ) goto nonode;

            bNodeOrientation = GetElementOrientation( psdirectedNode );

            psNodeElement = FindBareXMLChild(psdirectedNode,"Node");
            if( psNodeElement == NULL ) goto nonode;

            pspointProperty = FindBareXMLChild(psNodeElement,"pointProperty");
            if( pspointProperty == NULL )
                pspointProperty = FindBareXMLChild(psNodeElement,"connectionPointProperty");
            if( pspointProperty == NULL ) goto nonode;

            psPoint = FindBareXMLChild(pspointProperty,"Point");
            if( psPoint == NULL )
                psPoint = FindBareXMLChild(pspointProperty,"ConnectionPoint");
            if( psPoint == NULL ) goto nonode;

            poGeom = GML2OGRGeometry_XMLNode( psPoint, bGetSecondaryGeometryOption,
                                              nRecLevel + 1, TRUE );
            if( poGeom == NULL
                || wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
            {
/*                CPLError( CE_Failure, CPLE_AppDefined, 
                      "Got %.500s geometry as Member instead of POINT.",
                      poGeom ? poGeom->getGeometryName() : "NULL" );*/
                if( poGeom != NULL) delete poGeom;
                goto nonode;
            }

            if( ( bNodeOrientation == bEdgeOrientation ) != bOrientation )
                poPositiveNode = (OGRPoint *)poGeom;
            else
                poNegativeNode = (OGRPoint *)poGeom;

            // look for the other node
            psdirectedNode = psdirectedNode->psNext;
            while( psdirectedNode != NULL &&
                   !EQUAL( psdirectedNode->pszValue, "directedNode" ) )
                psdirectedNode = psdirectedNode->psNext;
            if( psdirectedNode == NULL ) goto nonode;

            if( GetElementOrientation( psdirectedNode ) == bNodeOrientation )
                goto nonode;

            psNodeElement = FindBareXMLChild(psEdge,"Node");
            if( psNodeElement == NULL ) goto nonode;

            pspointProperty = FindBareXMLChild(psNodeElement,"pointProperty");
            if( pspointProperty == NULL )
                pspointProperty = FindBareXMLChild(psNodeElement,"connectionPointProperty");
            if( pspointProperty == NULL ) goto nonode;

            psPoint = FindBareXMLChild(pspointProperty,"Point");
            if( psPoint == NULL )
                psPoint = FindBareXMLChild(pspointProperty,"ConnectionPoint");
            if( psPoint == NULL ) goto nonode;

            poGeom = GML2OGRGeometry_XMLNode( psPoint, bGetSecondaryGeometryOption,
                                              nRecLevel + 1, TRUE );
            if( poGeom == NULL
                || wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
            {
/*                CPLError( CE_Failure, CPLE_AppDefined, 
                      "Got %.500s geometry as Member instead of POINT.",
                      poGeom ? poGeom->getGeometryName() : "NULL" );*/
                if( poGeom != NULL) delete poGeom;
                goto nonode;
            }

            if( ( bNodeOrientation == bEdgeOrientation ) != bOrientation )
                poNegativeNode = (OGRPoint *)poGeom;
            else
                poPositiveNode = (OGRPoint *)poGeom;

            poMP = new OGRMultiPoint();
            poMP->addGeometryDirectly( poNegativeNode );
            poMP->addGeometryDirectly( poPositiveNode );
            
            return poMP;

            nonode:;
        }

        // collect curveproperty
        psCurveProperty = FindBareXMLChild(psEdge,"curveProperty");
        if( psCurveProperty == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                        "directedEdge: Failed to get curveProperty in Edge" );
            return NULL;
        }

        psCurve = FindBareXMLChild(psCurveProperty,"LineString");
        if( psCurve == NULL )
            psCurve = FindBareXMLChild(psCurveProperty,"Curve");
        if( psCurve == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "directedEdge: Failed to get LineString or Curve tag in curveProperty" );
            return NULL;
        }

        poLineString = (OGRLineString *)GML2OGRGeometry_XMLNode( psCurve, bGetSecondaryGeometryOption,
                                                                 nRecLevel + 1, TRUE );
        if( poLineString == NULL 
            || wkbFlatten(poLineString->getGeometryType()) != wkbLineString )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Got %.500s geometry as Member instead of LINESTRING.",
                      poLineString ? poLineString->getGeometryName() : "NULL" );
            if( poLineString != NULL )
                delete poLineString;
            return NULL;
        }

        if( bGetSecondaryGeometry )
        {
            // choose a point based on the orientation
            poNegativeNode = new OGRPoint();
            poPositiveNode = new OGRPoint();
            if( bEdgeOrientation == bOrientation )
            {
                poLineString->StartPoint( poNegativeNode );
                poLineString->EndPoint( poPositiveNode );
            }
            else
            {
                poLineString->StartPoint( poPositiveNode );
                poLineString->EndPoint( poNegativeNode );
            }
            delete poLineString;

            poMP = new OGRMultiPoint();
            poMP->addGeometryDirectly( poNegativeNode );
            poMP->addGeometryDirectly( poPositiveNode );

            return poMP;
        }

        // correct orientation of the line string
        if( bEdgeOrientation != bOrientation )
        {
            int iStartCoord = 0, iEndCoord = poLineString->getNumPoints() - 1;
            OGRPoint *poTempStartPoint = new OGRPoint();
            OGRPoint *poTempEndPoint = new OGRPoint();
            while( iStartCoord < iEndCoord )
            {
                poLineString->getPoint( iStartCoord, poTempStartPoint );
                poLineString->getPoint( iEndCoord, poTempEndPoint );
                poLineString->setPoint( iStartCoord, poTempEndPoint );
                poLineString->setPoint( iEndCoord, poTempStartPoint );
                iStartCoord++;
                iEndCoord--;
            }
            delete poTempStartPoint;
            delete poTempEndPoint;
        }
        return poLineString;
    }

/* -------------------------------------------------------------------- */
/*      TopoCurve                                                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"TopoCurve") )
    {
        const CPLXMLNode *psChild;
        OGRMultiLineString *poMLS = NULL;
        OGRMultiPoint *poMP = NULL;

        if( bGetSecondaryGeometry )
            poMP = new OGRMultiPoint();
        else
            poMLS = new OGRMultiLineString();

        // collect directedEdges
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"directedEdge"))
            {
                OGRGeometry *poGeom;

                poGeom = GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                                  nRecLevel + 1 );
                if( poGeom == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Failed to get geometry in directedEdge" );
                    delete poGeom;
                    if( bGetSecondaryGeometry )
                        delete poMP;
                    else
                        delete poMLS;
                    return NULL;
                }

                //Add the two points corresponding to the two nodes to poMP
                if( bGetSecondaryGeometry &&
                     wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
                {
                    //TODO: TopoCurve geometries with more than one
                    //      directedEdge elements were not tested.
                    if( poMP->getNumGeometries() <= 0 ||
                        !(poMP->getGeometryRef( poMP->getNumGeometries() - 1 )->Equals(((OGRMultiPoint *)poGeom)->getGeometryRef( 0 ) ) ))
                    {
                        poMP->addGeometry(
                            ( (OGRMultiPoint *)poGeom )->getGeometryRef( 0 ) );
                    }
                    poMP->addGeometry(
                            ( (OGRMultiPoint *)poGeom )->getGeometryRef( 1 ) );
                    delete poGeom;
                }
                else if( !bGetSecondaryGeometry &&
                     wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
                {
                    poMLS->addGeometryDirectly( poGeom );
                }
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Got %.500s geometry as Member instead of %s.",
                              poGeom ? poGeom->getGeometryName() : "NULL",
                              bGetSecondaryGeometry?"MULTIPOINT":"LINESTRING");
                    delete poGeom;
                    if( bGetSecondaryGeometry )
                        delete poMP;
                    else
                        delete poMLS;
                    return NULL;
                }
            }
        }

        if( bGetSecondaryGeometry )
            return poMP;
        else
            return poMLS;
    }

/* -------------------------------------------------------------------- */
/*      TopoSurface                                                     */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"TopoSurface") )
    {
        /****************************************************************/
        /* applying the FaceHoleNegative = FALSE rules                  */
        /*                                                              */
        /* - each <TopoSurface> is expected to represent a MultiPolygon */
        /* - each <Face> is expected to represent a distinct Polygon,   */
        /*   this including any possible Interior Ring (holes);         */
        /*   orientation="+/-" plays no role at all to identify "holes" */
        /* - each <Edge> within a <Face> may indifferently represent    */
        /*   an element of the Exterior or Interior Boundary; relative  */
        /*   order of <Egdes> is absolutely irrelevant.                 */
        /****************************************************************/
        /* Contributor: Alessandro Furieri, a.furieri@lqt.it            */
        /* Developed for Faunalia (http://www.faunalia.it)              */
        /* with funding from Regione Toscana -                          */
        /* Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE       */
        /****************************************************************/
        if(bFaceHoleNegative != TRUE)
        {
            if( bGetSecondaryGeometry )
                return NULL;

#ifndef HAVE_GEOS
            static int bWarningAlreadyEmitted = FALSE;
            if (!bWarningAlreadyEmitted)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Interpreating that GML TopoSurface geometry requires GDAL to be built with GEOS support.\n"
                        "As a workaround, you can try defining the GML_FACE_HOLE_NEGATIVE configuration option\n"
                        "to YES, so that the 'old' interpretation algorithm is used. But be warned that\n"
                        "the result might be incorrect.\n");
                bWarningAlreadyEmitted = TRUE;
            }
            return NULL;
#else
            const CPLXMLNode *psChild, *psFaceChild, *psDirectedEdgeChild;
            OGRMultiPolygon *poTS = new OGRMultiPolygon();

            // collect directed faces
            for( psChild = psNode->psChild; 
                psChild != NULL;
            psChild = psChild->psNext ) 
            {
              if( psChild->eType == CXT_Element
              && EQUAL(BareGMLElement(psChild->pszValue),"directedFace") )
              {
                // collect next face (psChild->psChild)
                psFaceChild = GetChildElement(psChild);
	
                while( psFaceChild != NULL &&
                       !(psFaceChild->eType == CXT_Element &&
                         EQUAL(BareGMLElement(psFaceChild->pszValue),"Face")) )
                        psFaceChild = psFaceChild->psNext;

                if( psFaceChild == NULL )
                  continue;

                OGRMultiLineString *poCollectedGeom = new OGRMultiLineString();

                // collect directed edges of the face
                for( psDirectedEdgeChild = psFaceChild->psChild;
                     psDirectedEdgeChild != NULL;
                     psDirectedEdgeChild = psDirectedEdgeChild->psNext )
                {
                  if( psDirectedEdgeChild->eType == CXT_Element &&
                      EQUAL(BareGMLElement(psDirectedEdgeChild->pszValue),"directedEdge") )
                  {
                    OGRGeometry *poEdgeGeom;

                    poEdgeGeom = GML2OGRGeometry_XMLNode( psDirectedEdgeChild,
                                                          bGetSecondaryGeometryOption,
                                                          TRUE );

                    if( poEdgeGeom == NULL ||
                        wkbFlatten(poEdgeGeom->getGeometryType()) != wkbLineString )
                    {
                      CPLError( CE_Failure, CPLE_AppDefined, 
                                "Failed to get geometry in directedEdge" );
                      delete poEdgeGeom;
                      delete poCollectedGeom;
                      delete poTS;
                      return NULL;
                    }

                    poCollectedGeom->addGeometryDirectly( poEdgeGeom );
                  }
                }

                OGRGeometry *poFaceCollectionGeom = NULL;
                OGRPolygon *poFaceGeom = NULL;

//#ifdef HAVE_GEOS
                poFaceCollectionGeom = poCollectedGeom->Polygonize();
                if( poFaceCollectionGeom == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Failed to assemble Edges in Face" );
                    delete poCollectedGeom;
                    delete poTS;
                    return NULL;
                }

                poFaceGeom = GML2FaceExtRing( poFaceCollectionGeom );
//#else
//                poFaceGeom = (OGRPolygon*) OGRBuildPolygonFromEdges(
//                    (OGRGeometryH) poCollectedGeom,
//                    FALSE, TRUE, 0, NULL);
//#endif

                if( poFaceGeom == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                                "Failed to build Polygon for Face" );
                    delete poCollectedGeom;
                    delete poTS;
                    return NULL;
                }
                else
                {
                    int iCount = poTS->getNumGeometries();
                    if( iCount == 0)
                    {
                        /* inserting the first Polygon */
                        poTS->addGeometryDirectly( poFaceGeom );
                    }
                    else
                    {
                        /* using Union to add the current Polygon */
                        OGRGeometry *poUnion = poTS->Union( poFaceGeom );
                        delete poFaceGeom;
                        delete poTS;
                        if( poUnion == NULL )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                        "Failed Union for TopoSurface" );
                            return NULL;
                        }
                        poTS = (OGRMultiPolygon *)poUnion;
                    }
                }
                delete poFaceCollectionGeom;
                delete poCollectedGeom;
              }
            }

            if( wkbFlatten( poTS->getGeometryType()) == wkbPolygon )
            {
                /* forcing to be a MultiPolygon */
                OGRGeometry *poOldTS = poTS;
                poTS = new OGRMultiPolygon();
                poTS->addGeometryDirectly(poOldTS);
            }

            return poTS;
#endif // HAVE_GEOS
        }

        /****************************************************************/
        /* applying the FaceHoleNegative = TRUE rules                   */
        /*                                                              */
        /* - each <TopoSurface> is expected to represent a MultiPolygon */
        /* - any <Face> declaring orientation="+" is expected to        */
        /*   represent an Exterior Ring (no holes are allowed)          */
        /* - any <Face> declaring orientation="-" is expected to        */
        /*   represent an Interior Ring (hole) belonging to the latest  */
        /*   Exterior Ring.                                             */
        /* - <Egdes> within the same <Face> are expected to be          */
        /*   arranged in geometrically adjacent and consecutive         */
        /*   sequence.                                                  */
        /****************************************************************/
        if( bGetSecondaryGeometry )
            return NULL;
        const CPLXMLNode *psChild, *psFaceChild, *psDirectedEdgeChild;
        int bFaceOrientation = TRUE;
        OGRPolygon *poTS = new OGRPolygon();

        // collect directed faces
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
          if( psChild->eType == CXT_Element
              && EQUAL(BareGMLElement(psChild->pszValue),"directedFace") )
          {
            bFaceOrientation = GetElementOrientation(psChild);

            // collect next face (psChild->psChild)
            psFaceChild = GetChildElement(psChild);
            while( psFaceChild != NULL &&
                   !EQUAL(BareGMLElement(psFaceChild->pszValue),"Face") )
                    psFaceChild = psFaceChild->psNext;

            if( psFaceChild == NULL )
              continue;

            OGRLinearRing *poFaceGeom = new OGRLinearRing();

            // collect directed edges of the face
            for( psDirectedEdgeChild = psFaceChild->psChild;
                 psDirectedEdgeChild != NULL;
                 psDirectedEdgeChild = psDirectedEdgeChild->psNext )
            {
              if( psDirectedEdgeChild->eType == CXT_Element &&
                  EQUAL(BareGMLElement(psDirectedEdgeChild->pszValue),"directedEdge") )
              {
                OGRGeometry *poEdgeGeom;

                poEdgeGeom = GML2OGRGeometry_XMLNode( psDirectedEdgeChild,
                                                      bGetSecondaryGeometryOption,
                                                      nRecLevel + 1,
                                                      TRUE,
                                                      bFaceOrientation );

                if( poEdgeGeom == NULL ||
                    wkbFlatten(poEdgeGeom->getGeometryType()) != wkbLineString )
                {
                  CPLError( CE_Failure, CPLE_AppDefined, 
                            "Failed to get geometry in directedEdge" );
                  delete poEdgeGeom;
                  delete poFaceGeom;
                  delete poTS;
                  return NULL;
                }

                OGRLineString *poLS;
                OGRLineString *poAddLS;
                if( !bFaceOrientation )
                {
                  poLS = (OGRLineString *)poEdgeGeom;
                  poAddLS = (OGRLineString *)poFaceGeom;
                  if( poLS->getNumPoints() > 0 && poAddLS->getNumPoints() > 0
                      && fabs(poLS->getX(poLS->getNumPoints()-1)
                              - poAddLS->getX(0)) < 1e-14
                      && fabs(poLS->getY(poLS->getNumPoints()-1)
                              - poAddLS->getY(0)) < 1e-14
                      && fabs(poLS->getZ(poLS->getNumPoints()-1)
                              - poAddLS->getZ(0)) < 1e-14) 
                  {
                      // Skip the first point of the new linestring to avoid
                      // invalidate duplicate points
                      poLS->addSubLineString( poAddLS, 1 );
                  }
                  else
                  {
                      // Add the whole new line string
                      poLS->addSubLineString( poAddLS );
                  }
                  poFaceGeom->empty();
                }
                poLS = (OGRLineString *)poFaceGeom;
                poAddLS = (OGRLineString *)poEdgeGeom;
                if( poLS->getNumPoints() > 0 && poAddLS->getNumPoints() > 0
                    && fabs(poLS->getX(poLS->getNumPoints()-1)
                            - poAddLS->getX(0)) < 1e-14
                    && fabs(poLS->getY(poLS->getNumPoints()-1)
                            - poAddLS->getY(0)) < 1e-14
                    && fabs(poLS->getZ(poLS->getNumPoints()-1)
                            - poAddLS->getZ(0)) < 1e-14) 
                {
                    // Skip the first point of the new linestring to avoid
                    // invalidate duplicate points
                    poLS->addSubLineString( poAddLS, 1 );
                }
                else
                {
                    // Add the whole new line string
                    poLS->addSubLineString( poAddLS );
                }
                delete poEdgeGeom;
              }
            }

/*            if( poFaceGeom == NULL )
            {
              CPLError( CE_Failure, CPLE_AppDefined, 
                        "Failed to get Face geometry in directedFace" );
              delete poFaceGeom;
              return NULL;
            }*/

            poTS->addRingDirectly( poFaceGeom );
          }
        }

/*        if( poTS == NULL )
        {
          CPLError( CE_Failure, CPLE_AppDefined, 
                    "Failed to get TopoSurface geometry" );
          delete poTS;
          return NULL;
        }*/

        return poTS;
    }

/* -------------------------------------------------------------------- */
/*      Surface                                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Surface") )
    {
        const CPLXMLNode *psChild;
        OGRGeometry *poResult = NULL;

        // Find outer ring.
        psChild = FindBareXMLChild( psNode, "patches" );
        if( psChild == NULL )
            psChild = FindBareXMLChild( psNode, "polygonPatches" );
        if( psChild == NULL )
            psChild = FindBareXMLChild( psNode, "trianglePatches" );

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
        {
            /* <gml:Surface/> and <gml:Surface><gml:patches/></gml:Surface> are valid GML */
            return new OGRPolygon();
        }

        for( ; psChild != NULL; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue),"PolygonPatch") ||
                    EQUAL(BareGMLElement(psChild->pszValue),"Triangle") ||
                    EQUAL(BareGMLElement(psChild->pszValue),"Rectangle")))
            {
                OGRPolygon *poPolygon = (OGRPolygon *) 
                    GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                             nRecLevel + 1 );
                if( poPolygon == NULL )
                    return NULL;
                
                if( poResult == NULL )
                    poResult = poPolygon;
                else if( wkbFlatten(poResult->getGeometryType()) == wkbPolygon )
                {
                    OGRMultiPolygon *poMP = new OGRMultiPolygon();
                    poMP->addGeometryDirectly( poResult );
                    poMP->addGeometryDirectly( poPolygon );
                    poResult = poMP;
                }
                else
                {
                    ((OGRMultiPolygon *) poResult)->addGeometryDirectly( poPolygon );
                }
            }
        }
        
        return poResult;
    }

/* -------------------------------------------------------------------- */
/*      TriangulatedSurface                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"TriangulatedSurface") ||
        EQUAL(pszBaseGeometry,"Tin") )
    {
        const CPLXMLNode *psChild;
        OGRGeometry *poResult = NULL;

        // Find trianglePatches
        psChild = FindBareXMLChild( psNode, "trianglePatches" );
        if (psChild == NULL)
            psChild = FindBareXMLChild( psNode, "patches" );

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <trianglePatches> for %s.", pszBaseGeometry );
            return NULL;
        }

        for( ; psChild != NULL; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"Triangle") )
            {
                OGRPolygon *poPolygon = (OGRPolygon *)
                    GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                             nRecLevel + 1 );
                if( poPolygon == NULL )
                    return NULL;

                if( poResult == NULL )
                    poResult = poPolygon;
                else if( wkbFlatten(poResult->getGeometryType()) == wkbPolygon )
                {
                    OGRMultiPolygon *poMP = new OGRMultiPolygon();
                    poMP->addGeometryDirectly( poResult );
                    poMP->addGeometryDirectly( poPolygon );
                    poResult = poMP;
                }
                else
                {
                    ((OGRMultiPolygon *) poResult)->addGeometryDirectly( poPolygon );
                }
            }
        }

        return poResult;
    }

/* -------------------------------------------------------------------- */
/*      Solid                                                           */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Solid") )
    {
        const CPLXMLNode *psChild;
        OGRGeometry* poGeom;

        // Find exterior element
        psChild = FindBareXMLChild( psNode, "exterior");

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
        {
            /* <gml:Solid/> and <gml:Solid><gml:exterior/></gml:Solid> are valid GML */
            return new OGRPolygon();
        }

        // Get the geometry inside <exterior>
        poGeom = GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                          nRecLevel + 1 );
        if( poGeom == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Invalid exterior element");
            delete poGeom;
            return NULL;
        }

        psChild = FindBareXMLChild( psNode, "interior");
        if( psChild != NULL )
        {
            static int bWarnedOnce = FALSE;
            if (!bWarnedOnce)
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "<interior> elements of <Solid> are ignored");
                bWarnedOnce = TRUE;
            }
        }

        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      OrientableSurface                                               */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"OrientableSurface") )
    {
        const CPLXMLNode *psChild;

        // Find baseSurface.
        psChild = FindBareXMLChild( psNode, "baseSurface" );

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <baseSurface> for OrientableSurface." );
            return NULL;
        }

        return GML2OGRGeometry_XMLNode( psChild, bGetSecondaryGeometryOption,
                                        nRecLevel + 1 );
    }

/* -------------------------------------------------------------------- */
/*      SimplePolygon, SimpleRectangle, SimpleTriangle                  */
/*      (GML 3.3 compact encoding)                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"SimplePolygon") ||
        EQUAL(pszBaseGeometry,"SimpleRectangle") ||
        EQUAL(pszBaseGeometry,"SimpleTriangle") )
    {
        OGRLinearRing   *poRing = new OGRLinearRing();

        if( !ParseGMLCoordinates( psNode, poRing ) )
        {
            delete poRing;
            return NULL;
        }

        poRing->closeRings();

        OGRPolygon* poPolygon = new OGRPolygon();
        poPolygon->addRingDirectly(poRing);
        return poPolygon;
    }

/* -------------------------------------------------------------------- */
/*      SimpleMultiPoint (GML 3.3 compact encoding)                     */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"SimpleMultiPoint") )
    {
        OGRLineString   *poLS = new OGRLineString();

        if( !ParseGMLCoordinates( psNode, poLS ) )
        {
            delete poLS;
            return NULL;
        }

        OGRMultiPoint* poMP = new OGRMultiPoint();
        int nPoints = poLS->getNumPoints();
        for(int i = 0; i < nPoints; i++)
        {
            OGRPoint* poPoint = new OGRPoint();
            poLS->getPoint(i, poPoint);
            poMP->addGeometryDirectly(poPoint);
        }
        delete poLS;
        return poMP;
    }

    CPLError( CE_Failure, CPLE_AppDefined, 
              "Unrecognised geometry type <%.500s>.", 
              pszBaseGeometry );

    return NULL;
}

/************************************************************************/
/*                      OGR_G_CreateFromGMLTree()                       */
/************************************************************************/

OGRGeometryH OGR_G_CreateFromGMLTree( const CPLXMLNode *psTree )

{
    return (OGRGeometryH) GML2OGRGeometry_XMLNode( psTree, -1 );
}

/************************************************************************/
/*                        OGR_G_CreateFromGML()                         */
/************************************************************************/

/**
 * \brief Create geometry from GML.
 *
 * This method translates a fragment of GML containing only the geometry
 * portion into a corresponding OGRGeometry.  There are many limitations
 * on the forms of GML geometries supported by this parser, but they are
 * too numerous to list here.
 *
 * The following GML2 elements are parsed : Point, LineString, Polygon,
 * MultiPoint, MultiLineString, MultiPolygon, MultiGeometry.
 *
 * (OGR >= 1.8.0) The following GML3 elements are parsed : Surface, MultiSurface,
 * PolygonPatch, Triangle, Rectangle, Curve, MultiCurve, CompositeCurve,
 * LineStringSegment, Arc, Circle, CompositeSurface, OrientableSurface, Solid,
 * Tin, TriangulatedSurface.
 *
 * Arc and Circle elements are stroked to linestring, by using a
 * 4 degrees step, unless the user has overridden the value with the
 * OGR_ARC_STEPSIZE configuration variable.
 *
 * The C++ method OGRGeometryFactory::createFromGML() is the same as this function.
 *
 * @param pszGML The GML fragment for the geometry.
 *
 * @return a geometry on succes, or NULL on error.
 */

OGRGeometryH OGR_G_CreateFromGML( const char *pszGML )

{
    if( pszGML == NULL || strlen(pszGML) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GML Geometry is empty in OGR_G_CreateFromGML()." );
        return NULL;
    }

/* ------------------------------------------------------------ -------- */
/*      Try to parse the XML snippet using the MiniXML API.  If this    */
/*      fails, we assume the minixml api has already posted a CPL       */
/*      error, and just return NULL.                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGML = CPLParseXMLString( pszGML );

    if( psGML == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Convert geometry recursively.                                   */
/* -------------------------------------------------------------------- */
    OGRGeometry *poGeometry;

    /* Must be in synced in OGR_G_CreateFromGML(), OGRGMLLayer::OGRGMLLayer() and GMLReader::GMLReader() */
    int bFaceHoleNegative = CSLTestBoolean(CPLGetConfigOption("GML_FACE_HOLE_NEGATIVE", "NO"));
    poGeometry = GML2OGRGeometry_XMLNode( psGML, -1, 0, FALSE, TRUE, bFaceHoleNegative );

    CPLDestroyXMLNode( psGML );
    
    return (OGRGeometryH) poGeometry;
}


