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
 * $Log$
 * Revision 1.4  2002/03/11 17:09:42  warmerda
 * added multipolygon support
 *
 * Revision 1.3  2002/03/07 22:37:10  warmerda
 * use ogr_gml_geom.h
 *
 * Revision 1.2  2002/03/06 20:07:12  warmerda
 * fixed point reading
 *
 * Revision 1.1  2002/01/24 17:39:22  warmerda
 * New
 *
 */

#include "ogr_gml_geom.h"
#include "cpl_minixml.h"
#include "cpl_error.h"

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

static CPLXMLNode *FindBareXMLChild( CPLXMLNode *psParent, 
                                     const char *pszBareName )

{
    CPLXMLNode *psCandidate = psParent->psChild;

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

static const char *GetElementText( CPLXMLNode *psElement )

{
    if( psElement == NULL )
        return NULL;

    CPLXMLNode *psChild = psElement->psChild;

    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Text )
            return psChild->pszValue;

        psChild = psChild->psNext;
    }
    
    return NULL;
}

/************************************************************************/
/*                              AddPoint()                              */
/*                                                                      */
/*      Add a point to the passed geometry.                             */
/************************************************************************/

static int AddPoint( OGRGeometry *poGeometry, 
                     double dfX, double dfY, double dfZ )

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
        poPoint->setZ( dfZ );

        return TRUE;
    }
                
    else if( poGeometry->getGeometryType() == wkbLineString
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        ((OGRLineString *) poGeometry)->addPoint( dfX, dfY, dfZ );

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

int ParseGMLCoordinates( CPLXMLNode *psGeomNode, OGRGeometry *poGeometry )

{
    CPLXMLNode *psCoordinates = FindBareXMLChild( psGeomNode, "coordinates" );
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

            // parse out 2 or 3 tuple. 
            dfX = atof( pszCoordString );
            while( *pszCoordString != '\0' 
                   && *pszCoordString != ','
                   && *pszCoordString != ' ' ) 
                pszCoordString++;

            if( *pszCoordString == '\0' || *pszCoordString == ' ' )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Corrupt <coordinates> value." );
                return FALSE;
            }

            pszCoordString++;
            dfY = atof( pszCoordString );
            while( *pszCoordString != '\0' 
                   && *pszCoordString != ','
                   && *pszCoordString != ' ' )
                pszCoordString++;

            if( *pszCoordString == ',' )
            {
                pszCoordString++;
                dfZ = atof( pszCoordString );
                while( *pszCoordString != '\0' 
                       && *pszCoordString != ','
                       && *pszCoordString != ' ' )
                pszCoordString++;
            }

            while( *pszCoordString == ' ' )
                pszCoordString++;

            if( !AddPoint( poGeometry, dfX, dfY, dfZ ) )
                return FALSE;

            iCoord++;
        }

        return iCoord > 0;
    }

/* -------------------------------------------------------------------- */
/*      Handle form with a list of <coord> items each with an <X>,      */
/*      and <Y> element.                                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCoordNode;

    for( psCoordNode = psGeomNode->psChild; 
         psCoordNode != NULL;
         psCoordNode = psCoordNode->psNext )
    {
        if( psCoordNode->eType != CXT_Element 
            || !EQUAL(BareGMLElement(psCoordNode->pszValue),"coord") )
            continue;

        CPLXMLNode *psXNode, *psYNode, *psZNode;
        double dfX, dfY, dfZ = 0.0;

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

        dfX = atof( GetElementText(psXNode) );
        dfY = atof( GetElementText(psYNode) );

        if( psZNode != NULL )
            dfZ = atof( GetElementText(psZNode) );

        if( !AddPoint( poGeometry, dfX, dfY, dfZ ) )
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

static OGRGeometry *GML2OGRGeometry_XMLNode( CPLXMLNode *psNode )

{
    const char *pszBaseGeometry = BareGMLElement( psNode->pszValue );

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"Polygon") )
    {
        CPLXMLNode *psChild;
        OGRPolygon *poPolygon = new OGRPolygon();
        OGRLinearRing *poRing;

        // Find outer ring.
        psChild = FindBareXMLChild( psNode, "outerBoundaryIs" );
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
            delete poPolygon;
            return NULL;
        }

        if( !EQUAL(poRing->getGeometryName(),"LINEARRING") )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Got %s geometry as outerBoundaryIs instead of LINEARRING.", 
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
                && EQUAL(BareGMLElement(psChild->pszValue),"innerBoundaryIs") )
            {
                poRing = (OGRLinearRing *) 
                    GML2OGRGeometry_XMLNode( psChild->psChild );
                if( !EQUAL(poRing->getGeometryName(),"LINEARRING") )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Got %s geometry as innerBoundaryIs instead of LINEARRING.", 
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
        OGRLinearRing	*poLinearRing = new OGRLinearRing();
        
        if( !ParseGMLCoordinates( psNode, poLinearRing ) )
        {
            delete poLinearRing;
            return NULL;
        }

        return poLinearRing;
    }

/* -------------------------------------------------------------------- */
/*      LineString                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"LineString") )
    {
        OGRLineString	*poLine = new OGRLineString();
        
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
        || EQUAL(pszBaseGeometry,"Point") )
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
/*      MultiPolygon                                                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry,"MultiPolygon") )
    {
        CPLXMLNode *psChild;
        OGRMultiPolygon *poMPoly = new OGRMultiPolygon();

        // Find all inner rings 
        for( psChild = psNode->psChild; 
             psChild != NULL;
             psChild = psChild->psNext ) 
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),"polygonMember") )
            {
                OGRPolygon *poPolygon;

                poPolygon = (OGRPolygon *) 
                    GML2OGRGeometry_XMLNode( psChild->psChild );
                if( !EQUAL(poPolygon->getGeometryName(),"POLYGON") )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Got %s geometry as polygonMember instead of POLYGON.", 
                              poPolygon->getGeometryName() );
                    delete poPolygon;
                    delete poMPoly;
                    return NULL;
                }

                poMPoly->addGeometryDirectly( poPolygon );
            }
        }

        return poMPoly;
    }

    CPLError( CE_Failure, CPLE_AppDefined, 
              "Unrecognised geometry type <%s>.", 
              pszBaseGeometry );

    return NULL;
}

/************************************************************************/
/*                          GML2OGRGeometry()                           */
/************************************************************************/

OGRGeometry *GML2OGRGeometry( const char *pszGML )

{
    if( pszGML == NULL || strlen(pszGML) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GML Geometry is empty in GML2OGRGeometry()." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to parse the XML snippet using the MiniXML API.  If this    */
/*      fails, we assume the minixml api has already posted a CPL       */
/*      error, and just return NULL.                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *poGML = CPLParseXMLString( pszGML );

    if( poGML == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Convert geometry recursively.                                   */
/* -------------------------------------------------------------------- */
    OGRGeometry *poGeometry;

    poGeometry = GML2OGRGeometry_XMLNode( poGML );

    CPLDestroyXMLNode( poGML );
    
    return poGeometry;
}
