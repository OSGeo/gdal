/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Code to translate between GML and OGR geometry forms.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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
    if( poGeometry->getGeometryType() == wkbPoint 
        || poGeometry->getGeometryType() == wkbPoint25D )
    {
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        if( poPoint->getX() != 0.0 || poPoint->getY() != 0.0 )
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
                
    else if( poGeometry->getGeometryType() == wkbLineString
             || poGeometry->getGeometryType() == wkbLineString25D )
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

        if( pszCoordString == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "<coordinates> element missing value." );
            return FALSE;
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

            if( *pszCoordString == '\0' || isspace((unsigned char)*pszCoordString) )
            {
                /* In theory, the coordinates inside a 2 or 3 tuple should be */
                /* separated by a comma. However it has been found in the wild */
                /* that for <gml:Point>, the coordinates are in rare cases separated by a space */
                /* See https://52north.org/twiki/bin/view/Processing/WPS-IDWExtension-ObservationCollectionExample */
                /* or http://agisdemo.faa.gov/aixmServices/getAllFeaturesByLocatorId?locatorId=DFW */
                if ( poGeometry->getGeometryType() == wkbPoint )
                {
                    char **papszTokens = CSLTokenizeStringComplex(
                        GetElementText( psCoordinates ), " ,", FALSE, FALSE );
                    int bSuccess = FALSE;

                    if( CSLCount( papszTokens ) == 3 )
                    {
                        bSuccess = AddPoint( poGeometry,
                                            OGRFastAtof(papszTokens[0]),
                                            OGRFastAtof(papszTokens[1]),
                                            OGRFastAtof(papszTokens[2]), 3 );
                    }
                    else if( CSLCount( papszTokens ) == 2 )
                    {
                        bSuccess = AddPoint( poGeometry,
                                            OGRFastAtof(papszTokens[0]),
                                            OGRFastAtof(papszTokens[1]),
                                            0.0, 2 );
                    }

                    CSLDestroy(papszTokens);
                    if (bSuccess)
                        return TRUE;
                }

                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Corrupt <coordinates> value." );
                return FALSE;
            }

            pszCoordString++;
            dfY = OGRFastAtof( pszCoordString );
            while( *pszCoordString != '\0' 
                   && *pszCoordString != ','
                   && !isspace((unsigned char)*pszCoordString) )
                pszCoordString++;

            if( *pszCoordString == ',' )
            {
                pszCoordString++;
                dfZ = OGRFastAtof( pszCoordString );
                nDimension = 3;
                while( *pszCoordString != '\0' 
                       && *pszCoordString != ','
                       && !isspace((unsigned char)*pszCoordString) )
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
        if( psPos->eType != CXT_Element 
            || !EQUAL(BareGMLElement(psPos->pszValue),"pos") )
            continue;
        
        char **papszTokens = CSLTokenizeStringComplex( 
            GetElementText( psPos ), " ,", FALSE, FALSE );
        int bSuccess = FALSE;

        if( CSLCount( papszTokens ) > 2 )
        {
            bSuccess = AddPoint( poGeometry, 
                                 OGRFastAtof(papszTokens[0]), 
                                 OGRFastAtof(papszTokens[1]),
                                 OGRFastAtof(papszTokens[2]), 3 );
        }
        else if( CSLCount( papszTokens ) > 1 )
        {
            bSuccess = AddPoint( poGeometry, 
                                 OGRFastAtof(papszTokens[0]), 
                                 OGRFastAtof(papszTokens[1]),
                                 0.0, 2 );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Did not get 2+ values in <gml:pos>%s</gml:pos> tuple.",
                      GetElementText( psPos ) );
        }

        CSLDestroy( papszTokens );
        
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
        char **papszTokens;
        int bSuccess = FALSE;
        int i=0, nCount=0;
        const CPLXMLNode* psChild;
        int nDimension = 2;

        /* Try to detect the presence of an srsDimension attribute */
        /* This attribute is only availabe for gml3.1.1 but not */
        /* available for gml3.1 SF*/
        psChild = psPosList->psChild;
        while (psChild != NULL)
        {
            if (psChild->eType == CXT_Attribute &&
                EQUAL(psChild->pszValue, "srsDimension"))
            {
                nDimension = atoi(psChild->psChild->pszValue);
                break;
            }
            else if (psChild->eType != CXT_Attribute)
            {
                break;
            }
            psChild = psChild->psNext;
        }

        if (nDimension != 2 && nDimension != 3)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "srsDimension = %d not supported", nDimension);
            return FALSE;
        }

        papszTokens = CSLTokenizeStringComplex( 
            GetElementText( psPosList ), " ,\t", FALSE, FALSE );

        nCount = CSLCount( papszTokens );

        if (nCount < nDimension  || (nCount % nDimension) != 0)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Did not get at least %d values or invalid number of \n"
                      "set of coordinates <gml:posList>%s</gml:posList>",
                      nDimension, GetElementText( psPosList ) );
        }
        else
        {
            i=0;
            while (i<nCount)
            {
                bSuccess = AddPoint( poGeometry, 
                                     OGRFastAtof(papszTokens[i]), 
                                     OGRFastAtof(papszTokens[i+1]),
                                     (nDimension == 3) ? OGRFastAtof(papszTokens[i+2]) : 0.0, nDimension );
                i+=nDimension;
            }
        }
        CSLDestroy( papszTokens );

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

/************************************************************************/
/*                      GML2OGRGeometry_XMLNode()                       */
/*                                                                      */
/*      Translates the passed XMLnode and it's children into an         */
/*      OGRGeometry.  This is used recursively for geometry             */
/*      collections.                                                    */
/************************************************************************/

static OGRGeometry *GML2OGRGeometry_XMLNode( const CPLXMLNode *psNode,
                                             int bIgnoreGSG = FALSE,
                                             int bOrientation = TRUE )

{
    const char *pszBaseGeometry = BareGMLElement( psNode->pszValue );
    int bGetSecondaryGeometry =
            bIgnoreGSG ? FALSE :
            CSLTestBoolean(CPLGetConfigOption("GML_GET_SECONDARY_GEOM", "NO"));

    if( bGetSecondaryGeometry )
        if( !( EQUAL(pszBaseGeometry,"directedEdge") ||
               EQUAL(pszBaseGeometry,"TopoCurve") ) )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Polygon") ||
        EQUAL(pszBaseGeometry,"PolygonPatch") )
    {
        const CPLXMLNode *psChild;
        OGRPolygon *poPolygon = new OGRPolygon();
        OGRLinearRing *poRing;

        // Find outer ring.
        psChild = FindBareXMLChild( psNode, "outerBoundaryIs" );
        if (psChild == NULL)
           psChild = FindBareXMLChild( psNode, "exterior");

        if( psChild == NULL || psChild->psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Missing outerBoundaryIs property on Polygon." );
            delete poPolygon;
            return NULL;
        }

        // Translate outer ring and add to polygon.
        poRing = (OGRLinearRing *) 
            GML2OGRGeometry_XMLNode( psChild->psChild );
        if( poRing == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            delete poPolygon;
            return NULL;
        }

        if( !EQUAL(poRing->getGeometryName(),"LINEARRING") )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Polygon: Got %.500s geometry as outerBoundaryIs instead of LINEARRING.",
                      poRing->getGeometryName() );
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
                if (psChild->psChild != NULL)
                    poRing = (OGRLinearRing *) 
                        GML2OGRGeometry_XMLNode( psChild->psChild );
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
                              "Polygon: Got %.500s geometry as innerBoundaryIs instead of LINEARRING.",
                              poRing->getGeometryName() );
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
                OGRLineString *poLS;
                if (psChild->psChild)
                    poLS = (OGRLineString *) 
                        GML2OGRGeometry_XMLNode( psChild->psChild );
                else
                    poLS = NULL;

                if( poLS == NULL 
                    || wkbFlatten(poLS->getGeometryType()) != wkbLineString )
                {
                    delete poLS;
                    delete poLinearRing;
                    return NULL;
                }

                // we might need to take steps to avoid duplicate points...
                poLinearRing->addSubLineString( poLS );
                delete poLS;
            }
        }

        return poLinearRing;
    }

/* -------------------------------------------------------------------- */
/*      LineString                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"LineString")
        || EQUAL(pszBaseGeometry,"LineStringSegment") )
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
                OGRPolygon *poPolygon;

                if (psChild->psChild != NULL)
                    poPolygon = (OGRPolygon *) 
                        GML2OGRGeometry_XMLNode( psChild->psChild );
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
                            EQUAL(BareGMLElement(psChild2->pszValue),"PolygonPatch")) )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode( psChild2 );
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
                OGRPoint *poPoint;

                if (psChild->psChild != NULL)
                    poPoint = (OGRPoint *) 
                        GML2OGRGeometry_XMLNode( psChild->psChild );
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
                OGRGeometry *poGeom;

                if (psChild->psChild != NULL)
                    poGeom = GML2OGRGeometry_XMLNode( psChild->psChild );
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
/*      MultiCurve                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"MultiCurve") )
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
                poGeom = GML2OGRGeometry_XMLNode( psCurve );
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
        }
        return poMLS;
    }

/* -------------------------------------------------------------------- */
/*      Curve                                                      */
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

        poGeom = GML2OGRGeometry_XMLNode( psChild );
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
                && EQUAL(BareGMLElement(psChild->pszValue),"LineStringSegment") )
            {
                OGRGeometry *poGeom;

                poGeom = GML2OGRGeometry_XMLNode( psChild );
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
                    poLS->addSubLineString( (OGRLineString *)poGeom );
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
                OGRGeometry *poGeom;

                if (psChild->psChild != NULL)
                    poGeom = GML2OGRGeometry_XMLNode( psChild->psChild );
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
/*      Directed Edge                                              */
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

            poGeom = GML2OGRGeometry_XMLNode( psPoint, TRUE );
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

            poGeom = GML2OGRGeometry_XMLNode( psPoint, TRUE );
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
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "directedEdge: Failed to get LineString geometry in curveProperty" );
            return NULL;
        }

        poLineString = (OGRLineString *)GML2OGRGeometry_XMLNode( psCurve, TRUE );
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

                poGeom = GML2OGRGeometry_XMLNode( psChild );
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
            OGRLinearRing *poFaceGeom = new OGRLinearRing();

            bFaceOrientation = GetElementOrientation(psChild);

            // collect next face (psChild->psChild)
            psFaceChild = psChild->psChild;
            while( psFaceChild != NULL &&
                   !EQUAL(BareGMLElement(psFaceChild->pszValue),"Face") )
                    psFaceChild = psFaceChild->psNext;

            if( psFaceChild == NULL )
              continue;

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
                                                      TRUE,
                                                      bFaceOrientation );

                if( poEdgeGeom == NULL ||
                    wkbFlatten(poEdgeGeom->getGeometryType()) != wkbLineString )
                {
                  CPLError( CE_Failure, CPLE_AppDefined, 
                            "Failed to get geometry in directedEdge" );
                  delete poEdgeGeom;
                  return NULL;
                }

                if( !bFaceOrientation )
                {
                  if( poFaceGeom->getNumPoints() > 0 )
                    ((OGRLinearRing *)poEdgeGeom)->addSubLineString( (OGRLineString *)poFaceGeom );
                  poFaceGeom->empty();
                }
                poFaceGeom->addSubLineString( (OGRLinearRing *)poEdgeGeom );
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

        if( psChild == NULL || psChild->psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Missing <patches> for Surface." );
            return NULL;
        }

        for( psChild = psChild->psChild; 
             psChild != NULL; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"PolygonPatch") )
            {
                OGRPolygon *poPolygon = (OGRPolygon *) 
                    GML2OGRGeometry_XMLNode( psChild );
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

        if( psChild == NULL || psChild->psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing exterior property on Solid." );
            return NULL;
        }

        // Get the geometry inside <exterior>
        poGeom = GML2OGRGeometry_XMLNode( psChild->psChild );
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

        if( psChild == NULL || psChild->psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <baseSurface> for OrientableSurface." );
            return NULL;
        }

        return GML2OGRGeometry_XMLNode( psChild->psChild );
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
    return (OGRGeometryH) GML2OGRGeometry_XMLNode( psTree );
}

/************************************************************************/
/*                        OGR_G_CreateFromGML()                         */
/************************************************************************/

OGRGeometryH OGR_G_CreateFromGML( const char *pszGML )

{
    if( pszGML == NULL || strlen(pszGML) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GML Geometry is empty in GML2OGRGeometry()." );
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

    poGeometry = GML2OGRGeometry_XMLNode( psGML );

    CPLDestroyXMLNode( psGML );
    
    return (OGRGeometryH) poGeometry;
}


