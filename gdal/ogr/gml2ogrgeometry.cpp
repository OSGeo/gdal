/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Code to translate between GML and OGR geometry forms.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_api.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ogr_geo_utils.h"

CPL_CVSID("$Id$")

constexpr double kdfD2R = M_PI / 180.0;
constexpr double kdf2PI = 2.0 * M_PI;

/************************************************************************/
/*                        GMLGetCoordTokenPos()                         */
/************************************************************************/

static const char* GMLGetCoordTokenPos( const char* pszStr,
                                        const char** ppszNextToken )
{
    char ch;
    while( true )
    {
        ch = *pszStr;
        if( ch == '\0' )
        {
            *ppszNextToken = nullptr;
            return nullptr;
        }
        else if( !(ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ' ||
                   ch == ',') )
            break;
        pszStr++;
    }

    const char* pszToken = pszStr;
    while( (ch = *pszStr) != '\0' )
    {
        if( ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ' || ch == ',' )
        {
            *ppszNextToken = pszStr;
            return pszToken;
        }
        pszStr++;
    }
    *ppszNextToken = nullptr;
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
    const char *pszReturn = strchr( pszInput, ':' );
    if( pszReturn == nullptr )
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

    while( psCandidate != nullptr )
    {
        if( psCandidate->eType == CXT_Element
            && EQUAL(BareGMLElement(psCandidate->pszValue), pszBareName) )
            return psCandidate;

        psCandidate = psCandidate->psNext;
    }

    return nullptr;
}

/************************************************************************/
/*                           GetElementText()                           */
/************************************************************************/

static const char *GetElementText( const CPLXMLNode *psElement )

{
    if( psElement == nullptr )
        return nullptr;

    const CPLXMLNode *psChild = psElement->psChild;

    while( psChild != nullptr )
    {
        if( psChild->eType == CXT_Text )
            return psChild->pszValue;

        psChild = psChild->psNext;
    }

    return nullptr;
}

/************************************************************************/
/*                           GetChildElement()                          */
/************************************************************************/

static const CPLXMLNode *GetChildElement( const CPLXMLNode *psElement )

{
    if( psElement == nullptr )
        return nullptr;

    const CPLXMLNode *psChild = psElement->psChild;

    while( psChild != nullptr )
    {
        if( psChild->eType == CXT_Element )
            return psChild;

        psChild = psChild->psNext;
    }

    return nullptr;
}

/************************************************************************/
/*                    GetElementOrientation()                           */
/*     Returns true for positive orientation.                           */
/************************************************************************/

static bool GetElementOrientation( const CPLXMLNode *psElement )
{
    if( psElement == nullptr )
        return true;

    const CPLXMLNode *psChild = psElement->psChild;

    while( psChild != nullptr )
    {
        if( psChild->eType == CXT_Attribute &&
            EQUAL(psChild->pszValue, "orientation") )
                return EQUAL(psChild->psChild->pszValue, "+");

        psChild = psChild->psNext;
    }

    return true;
}

/************************************************************************/
/*                              AddPoint()                              */
/*                                                                      */
/*      Add a point to the passed geometry.                             */
/************************************************************************/

static bool AddPoint( OGRGeometry *poGeometry,
                      double dfX, double dfY, double dfZ, int nDimension )

{
    const OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());
    if( eType == wkbPoint )
    {
        OGRPoint *poPoint = poGeometry->toPoint();

        if( !poPoint->IsEmpty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "More than one coordinate for <Point> element.");
            return false;
        }

        poPoint->setX( dfX );
        poPoint->setY( dfY );
        if( nDimension == 3 )
            poPoint->setZ( dfZ );

        return true;
    }
    else if( eType == wkbLineString ||
             eType == wkbCircularString )
    {
        OGRSimpleCurve *poCurve = poGeometry->toSimpleCurve();
        if( nDimension == 3 )
            poCurve->addPoint(dfX, dfY, dfZ);
        else
            poCurve->addPoint(dfX, dfY);

        return true;
    }

    CPLAssert( false );
    return false;
}

/************************************************************************/
/*                        ParseGMLCoordinates()                         */
/************************************************************************/

static bool ParseGMLCoordinates( const CPLXMLNode *psGeomNode,
                                 OGRGeometry *poGeometry,
                                 int nSRSDimension )

{
    const CPLXMLNode *psCoordinates =
        FindBareXMLChild( psGeomNode, "coordinates" );

/* -------------------------------------------------------------------- */
/*      Handle <coordinates> case.                                      */
/*      Note that we don't do a strict validation, so we accept and     */
/*      sometimes generate output whereas we should just reject it.     */
/* -------------------------------------------------------------------- */
    if( psCoordinates != nullptr )
    {
        const char *pszCoordString = GetElementText( psCoordinates );

        const char *pszDecimal =
            CPLGetXMLValue(psCoordinates,
                           "decimal", nullptr);
        char chDecimal = '.';
        if( pszDecimal != nullptr )
        {
            if( strlen(pszDecimal) != 1 ||
                (pszDecimal[0] >= '0' && pszDecimal[0] <= '9') )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for decimal attribute");
                return false;
            }
            chDecimal = pszDecimal[0];
        }

        const char *pszCS =
            CPLGetXMLValue(psCoordinates, "cs", nullptr);
        char chCS = ',';
        if( pszCS != nullptr )
        {
            if( strlen(pszCS) != 1 || (pszCS[0] >= '0' && pszCS[0] <= '9') )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for cs attribute");
                return false;
            }
            chCS = pszCS[0];
        }
        const char *pszTS =
            CPLGetXMLValue(psCoordinates, "ts", nullptr);
        char chTS = ' ';
        if( pszTS != nullptr )
        {
            if( strlen(pszTS) != 1 || (pszTS[0] >= '0' && pszTS[0] <= '9') )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for ts attribute");
                return false;
            }
            chTS = pszTS[0];
        }

        if( pszCoordString == nullptr )
        {
            poGeometry->empty();
            return true;
        }

        int iCoord = 0;
        const OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());
        OGRSimpleCurve *poCurve =
            (eType == wkbLineString || eType == wkbCircularString) ?
                poGeometry->toSimpleCurve() : nullptr;
        for( int iter = (eType == wkbPoint ? 1 : 0); iter < 2; iter++ )
        {
            const char* pszStr = pszCoordString;
            double dfX = 0;
            double dfY = 0;
            double dfZ = 0;
            iCoord = 0;
            while( *pszStr != '\0' )
            {
                int nDimension = 2;
                // parse out 2 or 3 tuple.
                if( iter == 1 )
                {
                    if( chDecimal == '.' )
                        dfX = OGRFastAtof( pszStr );
                    else
                        dfX = CPLAtofDelim( pszStr, chDecimal);
                }
                while( *pszStr != '\0'
                    && *pszStr != chCS
                    && !isspace(static_cast<unsigned char>(*pszStr)) )
                    pszStr++;

                if( *pszStr == '\0' )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Corrupt <coordinates> value.");
                    return false;
                }
                else if( chCS == ',' && pszCS == nullptr &&
                        isspace(static_cast<unsigned char>(*pszStr)) )
                {
                    // In theory, the coordinates inside a coordinate tuple should
                    // be separated by a comma. However it has been found in the
                    // wild that the coordinates are in rare cases separated by a
                    // space, and the tuples by a comma.
                    // See:
                    // https://52north.org/twiki/bin/view/Processing/WPS-IDWExtension-ObservationCollectionExample
                    // or
                    // http://agisdemo.faa.gov/aixmServices/getAllFeaturesByLocatorId?locatorId=DFW
                    chCS = ' ';
                    chTS = ',';
                }

                pszStr++;

                if( iter == 1 )
                {
                    if( chDecimal == '.' )
                        dfY = OGRFastAtof( pszStr );
                    else
                        dfY = CPLAtofDelim( pszStr, chDecimal);
                }
                while( *pszStr != '\0'
                    && *pszStr != chCS
                    && *pszStr != chTS
                    && !isspace(static_cast<unsigned char>(*pszStr)) )
                    pszStr++;

                dfZ = 0.0;
                if( *pszStr == chCS )
                {
                    pszStr++;
                    if( iter == 1 )
                    {
                        if( chDecimal == '.' )
                            dfZ = OGRFastAtof( pszStr );
                        else
                            dfZ = CPLAtofDelim( pszStr, chDecimal);
                    }
                    nDimension = 3;
                    while( *pszStr != '\0'
                        && *pszStr != chCS
                        && *pszStr != chTS
                        && !isspace(static_cast<unsigned char>(*pszStr)) )
                    pszStr++;
                }

                if( *pszStr == chTS )
                {
                    pszStr++;
                }

                while( isspace(static_cast<unsigned char>(*pszStr)) )
                    pszStr++;

                if( iter == 1 )
                {
                    if( poCurve )
                    {
                        if( nDimension == 3 )
                            poCurve->setPoint(iCoord, dfX, dfY, dfZ);
                        else
                            poCurve->setPoint(iCoord, dfX, dfY);
                    }
                    else if( !AddPoint( poGeometry, dfX, dfY, dfZ, nDimension ) )
                        return false;
                }

                iCoord++;
            }

            if( poCurve && iter == 0 )
            {
                poCurve->setNumPoints(iCoord);
            }
        }

        return iCoord > 0;
    }

/* -------------------------------------------------------------------- */
/*      Is this a "pos"?  GML 3 construct.                              */
/*      Parse if it exist a series of pos elements (this would allow    */
/*      the correct parsing of gml3.1.1 geometries such as linestring    */
/*      defined with pos elements.                                      */
/* -------------------------------------------------------------------- */
    bool bHasFoundPosElement = false;
    for( const CPLXMLNode *psPos = psGeomNode->psChild;
         psPos != nullptr;
         psPos = psPos->psNext )
    {
        if( psPos->eType != CXT_Element )
            continue;

        const char* pszSubElement = BareGMLElement(psPos->pszValue);

        if( EQUAL(pszSubElement, "pointProperty") )
        {
            for( const CPLXMLNode *psPointPropertyIter = psPos->psChild;
                 psPointPropertyIter != nullptr;
                 psPointPropertyIter = psPointPropertyIter->psNext )
            {
                if( psPointPropertyIter->eType != CXT_Element )
                    continue;

                const char* pszBareElement =
                    BareGMLElement(psPointPropertyIter->pszValue);
                if( EQUAL(pszBareElement, "Point") ||
                    EQUAL(pszBareElement, "ElevatedPoint") )
                {
                    OGRPoint oPoint;
                    if( ParseGMLCoordinates( psPointPropertyIter, &oPoint,
                                             nSRSDimension ) )
                    {
                        const bool bSuccess =
                            AddPoint( poGeometry, oPoint.getX(),
                                      oPoint.getY(), oPoint.getZ(),
                                      oPoint.getCoordinateDimension() );
                        if( bSuccess )
                            bHasFoundPosElement = true;
                        else
                            return false;
                    }
                }
            }

            if( psPos->psChild && psPos->psChild->eType == CXT_Attribute &&
                psPos->psChild->psNext == nullptr &&
                strcmp(psPos->psChild->pszValue, "xlink:href") == 0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot resolve xlink:href='%s'. "
                         "Try setting GML_SKIP_RESOLVE_ELEMS=NONE",
                         psPos->psChild->psChild->pszValue);
            }

            continue;
        }

        if( !EQUAL(pszSubElement, "pos") )
            continue;

        const char* pszPos = GetElementText( psPos );
        if( pszPos == nullptr )
        {
            poGeometry->empty();
            return true;
        }

        const char* pszCur = pszPos;
        const char* pszX = GMLGetCoordTokenPos(pszCur, &pszCur);
        const char* pszY = (pszCur != nullptr) ?
                            GMLGetCoordTokenPos(pszCur, &pszCur) : nullptr;
        const char* pszZ = (pszCur != nullptr) ?
                            GMLGetCoordTokenPos(pszCur, &pszCur) : nullptr;

        if( pszY == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Did not get 2+ values in <gml:pos>%s</gml:pos> tuple.",
                      pszPos );
            return false;
        }

        const double dfX = OGRFastAtof(pszX);
        const double dfY = OGRFastAtof(pszY);
        const double dfZ = (pszZ != nullptr) ? OGRFastAtof(pszZ) : 0.0;
        const bool bSuccess =
            AddPoint( poGeometry, dfX, dfY, dfZ, (pszZ != nullptr) ? 3 : 2 );

        if( bSuccess )
            bHasFoundPosElement = true;
        else
            return false;
    }

    if( bHasFoundPosElement )
        return true;

/* -------------------------------------------------------------------- */
/*      Is this a "posList"?  GML 3 construct (SF profile).             */
/* -------------------------------------------------------------------- */
    const CPLXMLNode *psPosList = FindBareXMLChild( psGeomNode, "posList" );

    if( psPosList != nullptr )
    {
        int nDimension = 2;

        // Try to detect the presence of an srsDimension attribute
        // This attribute is only available for gml3.1.1 but not
        // available for gml3.1 SF.
        const char* pszSRSDimension =
            CPLGetXMLValue(psPosList,
                           "srsDimension", nullptr);
        // If not found at the posList level, try on the enclosing element.
        if( pszSRSDimension == nullptr )
            pszSRSDimension =
                CPLGetXMLValue(psGeomNode,
                               "srsDimension", nullptr);
        if( pszSRSDimension != nullptr )
            nDimension = atoi(pszSRSDimension);
        else if( nSRSDimension != 0 )
            // Or use one coming from a still higher level element (#5606).
            nDimension = nSRSDimension;

        if( nDimension != 2 && nDimension != 3 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "srsDimension = %d not supported", nDimension);
            return false;
        }

        const char* pszPosList = GetElementText( psPosList );
        if( pszPosList == nullptr )
        {
            poGeometry->empty();
            return true;
        }

        bool bSuccess = false;
        const char* pszCur = pszPosList;
        while( true )
        {
            const char* pszX = GMLGetCoordTokenPos(pszCur, &pszCur);
            if( pszX == nullptr && bSuccess )
                break;
            const char* pszY = (pszCur != nullptr) ?
                    GMLGetCoordTokenPos(pszCur, &pszCur) : nullptr;
            const char* pszZ = (nDimension == 3 && pszCur != nullptr) ?
                    GMLGetCoordTokenPos(pszCur, &pszCur) : nullptr;

            if( pszY == nullptr || (nDimension == 3 && pszZ == nullptr) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Did not get at least %d values or invalid number of "
                        "set of coordinates <gml:posList>%s</gml:posList>",
                        nDimension, pszPosList);
                return false;
            }

            double dfX = OGRFastAtof(pszX);
            double dfY = OGRFastAtof(pszY);
            double dfZ = (pszZ != nullptr) ? OGRFastAtof(pszZ) : 0.0;
            bSuccess = AddPoint( poGeometry, dfX, dfY, dfZ, nDimension );

            if( !bSuccess || pszCur == nullptr )
                break;
        }

        return bSuccess;
    }

/* -------------------------------------------------------------------- */
/*      Handle form with a list of <coord> items each with an <X>,      */
/*      and <Y> element.                                                */
/* -------------------------------------------------------------------- */
    int iCoord = 0;
    for( const CPLXMLNode *psCoordNode = psGeomNode->psChild;
         psCoordNode != nullptr;
         psCoordNode = psCoordNode->psNext )
    {
        if( psCoordNode->eType != CXT_Element
            || !EQUAL(BareGMLElement(psCoordNode->pszValue), "coord") )
            continue;

        const CPLXMLNode *psXNode = FindBareXMLChild( psCoordNode, "X" );
        const CPLXMLNode *psYNode = FindBareXMLChild( psCoordNode, "Y" );
        const CPLXMLNode *psZNode = FindBareXMLChild( psCoordNode, "Z" );

        if( psXNode == nullptr || psYNode == nullptr
            || GetElementText(psXNode) == nullptr
            || GetElementText(psYNode) == nullptr
            || (psZNode != nullptr && GetElementText(psZNode) == nullptr) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Corrupt <coord> element, missing <X> or <Y> element?" );
            return false;
        }

        double dfX = OGRFastAtof( GetElementText(psXNode) );
        double dfY = OGRFastAtof( GetElementText(psYNode) );

        int nDimension = 2;
        double dfZ = 0.0;
        if( psZNode != nullptr && GetElementText(psZNode) != nullptr )
        {
            dfZ = OGRFastAtof( GetElementText(psZNode) );
            nDimension = 3;
        }

        if( !AddPoint( poGeometry, dfX, dfY, dfZ, nDimension ) )
            return false;

        iCoord++;
    }

    return iCoord > 0;
}

#ifdef HAVE_GEOS
/************************************************************************/
/*                         GML2FaceExtRing()                            */
/*                                                                      */
/*      Identifies the "good" Polygon within the collection returned    */
/*      by GEOSPolygonize()                                             */
/*      short rationale: GEOSPolygonize() will possibly return a        */
/*      collection of many Polygons; only one is the "good" one,        */
/*      (including both exterior- and interior-rings)                   */
/*      any other simply represents a single "hole", and should be      */
/*      consequently ignored at all.                                    */
/************************************************************************/

static OGRPolygon *GML2FaceExtRing( const OGRGeometry *poGeom )
{
    const OGRGeometryCollection *poColl =
        dynamic_cast<const OGRGeometryCollection *>(poGeom);
    if( poColl == nullptr )
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRGeometryCollection.");
        return nullptr;
    }

    OGRPolygon *poPolygon = nullptr;
    bool bError = false;
    int iCount = poColl->getNumGeometries();
    int iExterior = 0;
    int iInterior = 0;

    for( int ig = 0; ig < iCount; ig++)
    {
        // A collection of Polygons is expected to be found.
        const OGRGeometry *poChild = poColl->getGeometryRef(ig);
        if( poChild == nullptr)
        {
            bError = true;
            continue;
        }
        if( wkbFlatten( poChild->getGeometryType()) == wkbPolygon )
        {
            const OGRPolygon *poPg = poChild->toPolygon();
            if( poPg->getNumInteriorRings() > 0 )
                iExterior++;
            else
                iInterior++;
        }
        else
        {
            bError = true;
        }
    }

    if( !bError && iCount > 0 )
    {
       if( iCount == 1 && iExterior == 0 && iInterior == 1)
        {
            // There is a single Polygon within the collection.
            const OGRPolygon *poPg = poColl->getGeometryRef(0)->toPolygon();
            poPolygon = poPg->clone();
        }
        else
        {
            if( iExterior == 1 && iInterior == iCount - 1 )
            {
                // Searching the unique Polygon containing holes.
                for( int ig = 0; ig < iCount; ig++ )
                {
                    const OGRPolygon *poPg =
                        poColl->getGeometryRef(ig)->toPolygon();
                    if( poPg->getNumInteriorRings() > 0 )
                    {
                        poPolygon = poPg->clone();
                    }
                }
            }
        }
    }

    return poPolygon;
}
#endif

/************************************************************************/
/*                   GML2OGRGeometry_AddToCompositeCurve()              */
/************************************************************************/

static
bool GML2OGRGeometry_AddToCompositeCurve( OGRCompoundCurve* poCC,
                                          OGRGeometry* poGeom,
                                          bool& bChildrenAreAllLineString )
{
    if( poGeom == nullptr ||
        !OGR_GT_IsCurve(poGeom->getGeometryType()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CompositeCurve: Got %.500s geometry as Member instead of a "
                 "curve.",
                 poGeom ? poGeom->getGeometryName() : "NULL" );
        return false;
    }

    // Crazy but allowed by GML: composite in composite.
    if( wkbFlatten(poGeom->getGeometryType()) == wkbCompoundCurve )
    {
        OGRCompoundCurve* poCCChild = poGeom->toCompoundCurve();
        while( poCCChild->getNumCurves() != 0 )
        {
            OGRCurve* poCurve = poCCChild->stealCurve(0);
            if( wkbFlatten(poCurve->getGeometryType()) != wkbLineString )
                bChildrenAreAllLineString = false;
            if( poCC->addCurveDirectly(poCurve) != OGRERR_NONE )
            {
                delete poCurve;
                return false;
            }
        }
        delete poCCChild;
    }
    else
    {
        if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
            bChildrenAreAllLineString = false;

        OGRCurve *poCurve = poGeom->toCurve();
        if( poCC->addCurveDirectly( poCurve ) != OGRERR_NONE )
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                   GML2OGRGeometry_AddToCompositeCurve()              */
/************************************************************************/

static
bool GML2OGRGeometry_AddToMultiSurface( OGRMultiSurface* poMS,
                                        OGRGeometry*& poGeom,
                                        const char* pszMemberElement,
                                        bool& bChildrenAreAllPolygons )
{
    if( poGeom == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Invalid %s",
                  pszMemberElement );
        return false;
    }

    OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    if( eType == wkbPolygon || eType == wkbCurvePolygon )
    {
        if( eType != wkbPolygon )
            bChildrenAreAllPolygons = false;

        if( poMS->addGeometryDirectly( poGeom ) != OGRERR_NONE )
        {
            return false;
        }
    }
    else if( eType == wkbMultiPolygon || eType == wkbMultiSurface )
    {
        OGRMultiSurface* poMS2 = poGeom->toMultiSurface();
        for( int i = 0; i < poMS2->getNumGeometries(); i++ )
        {
            if( wkbFlatten(poMS2->getGeometryRef(i)->getGeometryType()) !=
                wkbPolygon )
                bChildrenAreAllPolygons = false;

            if( poMS->addGeometry(poMS2->getGeometryRef(i)) != OGRERR_NONE )
            {
                return false;
            }
        }
        delete poGeom;
        poGeom = nullptr;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Got %.500s geometry as %s.",
                    poGeom->getGeometryName(), pszMemberElement );
        return false;
    }
    return true;
}

/************************************************************************/
/*                        GetDistanceInMetre()                          */
/************************************************************************/

static double GetDistanceInMetre(double dfDistance, const char* pszUnits)
{
    if( EQUAL(pszUnits, "m") )
        return dfDistance;

    if( EQUAL(pszUnits, "km") )
        return dfDistance * 1000;

    if( EQUAL(pszUnits, "nm") || EQUAL(pszUnits, "[nmi_i]") )
        return dfDistance * CPLAtof(SRS_UL_INTL_NAUT_MILE_CONV);

    if( EQUAL(pszUnits, "mi") )
        return dfDistance * CPLAtof(SRS_UL_INTL_STAT_MILE_CONV);

    if( EQUAL(pszUnits, "ft") )
        return dfDistance * CPLAtof(SRS_UL_INTL_FOOT_CONV);

    CPLDebug("GML2OGRGeometry", "Unhandled unit: %s", pszUnits);
    return -1;
}

/************************************************************************/
/*                      GML2OGRGeometry_XMLNode()                       */
/*                                                                      */
/*      Translates the passed XMLnode and its children into an         */
/*      OGRGeometry.  This is used recursively for geometry             */
/*      collections.                                                    */
/************************************************************************/

static
OGRGeometry *GML2OGRGeometry_XMLNode_Internal(
    const CPLXMLNode *psNode,
    int nPseudoBoolGetSecondaryGeometryOption,
    int nRecLevel,
    int nSRSDimension,
    const char* pszSRSName,
    bool bIgnoreGSG = false,
    bool bOrientation = true,
    bool bFaceHoleNegative = false );

OGRGeometry *GML2OGRGeometry_XMLNode( const CPLXMLNode *psNode,
                                      int nPseudoBoolGetSecondaryGeometryOption,
                                      int nRecLevel,
                                      int nSRSDimension,
                                      bool bIgnoreGSG,
                                      bool bOrientation,
                                      bool bFaceHoleNegative )

{
    return
        GML2OGRGeometry_XMLNode_Internal(
            psNode,
            nPseudoBoolGetSecondaryGeometryOption,
            nRecLevel, nSRSDimension,
            nullptr,
            bIgnoreGSG, bOrientation,
            bFaceHoleNegative);
}

static
OGRGeometry *GML2OGRGeometry_XMLNode_Internal(
    const CPLXMLNode *psNode,
    int nPseudoBoolGetSecondaryGeometryOption,
    int nRecLevel,
    int nSRSDimension,
    const char* pszSRSName,
    bool bIgnoreGSG,
    bool bOrientation,
    bool bFaceHoleNegative )
{
    // constexpr bool bCastToLinearTypeIfPossible = true;  // Hard-coded for now.

    // We need this nRecLevel == 0 check, otherwise this could result in multiple
    // revist of the same node, and exponential complexity.
    if( nRecLevel == 0 && psNode != nullptr && strcmp(psNode->pszValue, "?xml") == 0 )
        psNode = psNode->psNext;
    while( psNode != nullptr && psNode->eType == CXT_Comment )
        psNode = psNode->psNext;
    if( psNode == nullptr )
        return nullptr;

    const char* pszSRSDimension =
        CPLGetXMLValue(psNode, "srsDimension", nullptr);
    if( pszSRSDimension != nullptr )
        nSRSDimension = atoi(pszSRSDimension);

    if( pszSRSName == nullptr )
        pszSRSName =
            CPLGetXMLValue(psNode, "srsName", nullptr);

    const char *pszBaseGeometry = BareGMLElement( psNode->pszValue );
    if( nPseudoBoolGetSecondaryGeometryOption < 0 )
        nPseudoBoolGetSecondaryGeometryOption =
            CPLTestBool(CPLGetConfigOption("GML_GET_SECONDARY_GEOM", "NO"));
    bool bGetSecondaryGeometry =
        bIgnoreGSG ? false : CPL_TO_BOOL(nPseudoBoolGetSecondaryGeometryOption);

    // Arbitrary value, but certainly large enough for reasonable usages.
    if( nRecLevel == 32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too many recursion levels (%d) while parsing GML geometry.",
                  nRecLevel );
        return nullptr;
    }

    if( bGetSecondaryGeometry )
        if( !( EQUAL(pszBaseGeometry, "directedEdge") ||
               EQUAL(pszBaseGeometry, "TopoCurve") ) )
            return nullptr;

/* -------------------------------------------------------------------- */
/*      Polygon / PolygonPatch / Rectangle                              */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Polygon") ||
        EQUAL(pszBaseGeometry, "PolygonPatch") ||
        EQUAL(pszBaseGeometry, "Rectangle"))
    {
        // Find outer ring.
        const CPLXMLNode *psChild =
            FindBareXMLChild( psNode, "outerBoundaryIs" );
        if( psChild == nullptr )
           psChild = FindBareXMLChild( psNode, "exterior");

        psChild = GetChildElement(psChild);
        if( psChild == nullptr )
        {
            // <gml:Polygon/> is invalid GML2, but valid GML3, so be tolerant.
            return new OGRPolygon();
        }

        // Translate outer ring and add to polygon.
        OGRGeometry* poGeom =
            GML2OGRGeometry_XMLNode_Internal(
                psChild,
                nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension,
                pszSRSName );
        if( poGeom == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            return nullptr;
        }

        if( !OGR_GT_IsCurve(poGeom->getGeometryType()) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: Got %.500s geometry as outerBoundaryIs.",
                      pszBaseGeometry, poGeom->getGeometryName() );
            delete poGeom;
            return nullptr;
        }

        if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
            !EQUAL(poGeom->getGeometryName(), "LINEARRING") )
        {
            OGRCurve *poCurve = poGeom->toCurve();
            poGeom = OGRCurve::CastToLinearRing(poCurve);
            if( poGeom == nullptr )
                return nullptr;
        }

        OGRCurvePolygon *poCP = nullptr;
        bool bIsPolygon = false;
        if( EQUAL(poGeom->getGeometryName(), "LINEARRING") )
        {
            poCP = new OGRPolygon();
            bIsPolygon = true;
        }
        else
        {
            poCP = new OGRCurvePolygon();
            bIsPolygon = false;
        }

        {
            OGRCurve *poCurve = poGeom->toCurve();
            if( poCP->addRingDirectly(poCurve) != OGRERR_NONE )
            {
                delete poCP;
                delete poGeom;
                return nullptr;
            }
        }

        // Find all inner rings
        for( psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue),
                          "innerBoundaryIs") ||
                    EQUAL(BareGMLElement(psChild->pszValue), "interior")))
            {
                const CPLXMLNode* psInteriorChild = GetChildElement(psChild);
                if( psInteriorChild != nullptr )
                    poGeom =
                        GML2OGRGeometry_XMLNode_Internal(
                            psInteriorChild,
                            nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1,
                            nSRSDimension,
                            pszSRSName );
                else
                    poGeom = nullptr;
                if( poGeom == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Invalid interior ring");
                    delete poCP;
                    return nullptr;
                }

                if( !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "%s: Got %.500s geometry as innerBoundaryIs.",
                            pszBaseGeometry, poGeom->getGeometryName() );
                    delete poCP;
                    delete poGeom;
                    return nullptr;
                }

                if( bIsPolygon )
                {
                    if( !EQUAL(poGeom->getGeometryName(), "LINEARRING") )
                    {
                        if( wkbFlatten(poGeom->getGeometryType()) ==
                            wkbLineString )
                        {
                            OGRLineString* poLS = poGeom->toLineString();
                            poGeom = OGRCurve::CastToLinearRing(poLS);
                            if( poGeom == nullptr )
                            {
                                delete poCP;
                                return nullptr;
                            }
                        }
                        else
                        {
                            // Might fail if some rings are not closed.
                            // We used to be tolerant about that with Polygon.
                            // but we have become stricter with CurvePolygon.
                            poCP = OGRSurface::CastToCurvePolygon(poCP);
                            if( poCP == nullptr )
                            {
                                delete poGeom;
                                return nullptr;
                            }
                            bIsPolygon = false;
                        }
                    }
                }
                else
                {
                    if( EQUAL(poGeom->getGeometryName(), "LINEARRING") )
                    {
                        OGRCurve *poCurve = poGeom->toCurve();
                        poGeom = OGRCurve::CastToLineString(poCurve);
                    }
                }
                OGRCurve *poCurve = poGeom->toCurve();
                if( poCP->addRingDirectly(poCurve) != OGRERR_NONE )
                {
                    delete poCP;
                    delete poGeom;
                    return nullptr;
                }
            }
        }

        return poCP;
    }

/* -------------------------------------------------------------------- */
/*      Triangle                                                        */
/* -------------------------------------------------------------------- */

    if( EQUAL(pszBaseGeometry, "Triangle"))
    {
        // Find outer ring.
        const CPLXMLNode *psChild =
            FindBareXMLChild( psNode, "outerBoundaryIs" );
        if( psChild == nullptr )
           psChild = FindBareXMLChild( psNode, "exterior");

        psChild = GetChildElement(psChild);
        if( psChild == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Empty Triangle");
            return new OGRTriangle();
        }

        // Translate outer ring and add to Triangle.
        OGRGeometry* poGeom =
            GML2OGRGeometry_XMLNode_Internal(
                psChild,
                nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension,
                pszSRSName);
        if( poGeom == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            return nullptr;
        }

        if( !OGR_GT_IsCurve(poGeom->getGeometryType()) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: Got %.500s geometry as outerBoundaryIs.",
                      pszBaseGeometry, poGeom->getGeometryName() );
            delete poGeom;
            return nullptr;
        }

        if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
            !EQUAL(poGeom->getGeometryName(), "LINEARRING") )
        {
            poGeom = OGRCurve::CastToLinearRing(poGeom->toCurve());
        }

        if( poGeom == nullptr || !EQUAL(poGeom->getGeometryName(), "LINEARRING") )
        {
            delete poGeom;
            return nullptr;
        }

        OGRTriangle *poTriangle = new OGRTriangle();

        if( poTriangle->addRingDirectly( poGeom->toCurve() ) != OGRERR_NONE )
        {
            delete poTriangle;
            delete poGeom;
            return nullptr;
        }

        return poTriangle;
    }

/* -------------------------------------------------------------------- */
/*      LinearRing                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "LinearRing") )
    {
        OGRLinearRing *poLinearRing = new OGRLinearRing();

        if( !ParseGMLCoordinates( psNode, poLinearRing, nSRSDimension ) )
        {
            delete poLinearRing;
            return nullptr;
        }

        return poLinearRing;
    }

    const auto storeArcByCenterPointParameters = [](
        const CPLXMLNode* psChild,
        const char* l_pszSRSName,
        bool &bIsApproximateArc,
        double &dfLastCurveApproximateArcRadius,
        bool &bLastCurveWasApproximateArcInvertedAxisOrder)
    {
        const CPLXMLNode* psRadius =
            FindBareXMLChild(psChild, "radius");
        if( psRadius && psRadius->eType == CXT_Element )
        {
            double dfRadius = CPLAtof(CPLGetXMLValue(
                psRadius, nullptr, "0"));
            const char* pszUnits = CPLGetXMLValue(
                psRadius, "uom", nullptr);
            bool bSRSUnitIsDegree = false;
            bool bInvertedAxisOrder = false;
            if( l_pszSRSName != nullptr )
            {
                OGRSpatialReference oSRS;
                if( oSRS.SetFromUserInput(l_pszSRSName)
                    == OGRERR_NONE )
                {
                    if( oSRS.IsGeographic() )
                    {
                        bInvertedAxisOrder =
                            CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                        bSRSUnitIsDegree =
                            fabs(oSRS.GetAngularUnits(nullptr) -
                                    CPLAtof(SRS_UA_DEGREE_CONV))
                            < 1e-8;
                    }
                }
            }
            if( bSRSUnitIsDegree && pszUnits != nullptr &&
                (dfRadius = GetDistanceInMetre(dfRadius, pszUnits)) > 0 )
            {
                bIsApproximateArc = true;
                dfLastCurveApproximateArcRadius = dfRadius;
                bLastCurveWasApproximateArcInvertedAxisOrder =
                    bInvertedAxisOrder;
            }
        }
    };

    const auto connectArcByCenterPointToOtherSegments = [](
        OGRGeometry* poGeom,
        OGRCompoundCurve* poCC,
        const bool bIsApproximateArc,
        const bool bLastCurveWasApproximateArc,
        const double dfLastCurveApproximateArcRadius,
        const bool bLastCurveWasApproximateArcInvertedAxisOrder)
    {
        if( bIsApproximateArc )
        {
            if( poGeom->getGeometryType() == wkbLineString )
            {
                OGRCurve* poPreviousCurve =
                    poCC->getCurve(poCC->getNumCurves()-1);
                OGRLineString* poLS = poGeom->toLineString();
                if( poPreviousCurve->getNumPoints() >= 2 &&
                    poLS->getNumPoints() >= 2 )
                {
                    OGRPoint p;
                    OGRPoint p2;
                    poPreviousCurve->EndPoint(&p);
                    poLS->StartPoint(&p2);
                    double dfDistance = 0.0;
                    if( bLastCurveWasApproximateArcInvertedAxisOrder )
                        dfDistance =
                            OGR_GreatCircle_Distance(
                                p.getX(), p.getY(),
                                p2.getX(), p2.getY());
                    else
                        dfDistance =
                            OGR_GreatCircle_Distance(
                                p.getY(), p.getX(),
                                p2.getY(), p2.getX());
                    // CPLDebug("OGR", "%f %f",
                    //          dfDistance,
                    //          dfLastCurveApproximateArcRadius
                    //          / 10.0 );
                    if( dfDistance <
                        dfLastCurveApproximateArcRadius / 5.0 )
                    {
                        CPLDebug("OGR",
                                    "Moving approximate start of "
                                    "ArcByCenterPoint to end of "
                                    "previous curve");
                        poLS->setPoint(0, &p);
                    }
                }
            }
        }
        else if( bLastCurveWasApproximateArc )
        {
            OGRCurve* poPreviousCurve =
                poCC->getCurve(poCC->getNumCurves()-1);
            if( poPreviousCurve->getGeometryType() ==
                wkbLineString )
            {
                OGRLineString* poLS = poPreviousCurve->toLineString();
                OGRCurve *poAsCurve = poGeom->toCurve();

                if( poLS->getNumPoints() >= 2 &&
                    poAsCurve->getNumPoints() >= 2 )
                {
                    OGRPoint p;
                    OGRPoint p2;
                    poAsCurve->StartPoint(&p);
                    poLS->EndPoint(&p2);
                    double dfDistance = 0.0;
                    if( bLastCurveWasApproximateArcInvertedAxisOrder )
                        dfDistance =
                            OGR_GreatCircle_Distance(
                                p.getX(), p.getY(),
                                p2.getX(), p2.getY());
                    else
                        dfDistance =
                            OGR_GreatCircle_Distance(
                                p.getY(), p.getX(),
                                p2.getY(), p2.getX());
                    // CPLDebug(
                    //    "OGR", "%f %f",
                    //    dfDistance,
                    //    dfLastCurveApproximateArcRadius / 10.0 );

                    // "A-311 WHEELER AFB OAHU, HI.xml" needs more
                    // than 10%.
                    if( dfDistance <
                        dfLastCurveApproximateArcRadius / 5.0 )
                    {
                        CPLDebug(
                            "OGR",
                            "Moving approximate end of last "
                            "ArcByCenterPoint to start of the "
                            "current curve");
                        poLS->setPoint(poLS->getNumPoints() - 1,
                                        &p);
                    }
                }
            }
        }
    };

/* -------------------------------------------------------------------- */
/*      Ring GML3                                                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Ring") )
    {
        OGRCurve* poRing = nullptr;
        OGRCompoundCurve *poCC = nullptr;
        bool bChildrenAreAllLineString = true;

        bool bLastCurveWasApproximateArc = false;
        bool bLastCurveWasApproximateArcInvertedAxisOrder = false;
        double dfLastCurveApproximateArcRadius = 0.0;

        bool bIsFirstChild = true;
        bool bFirstChildIsApproximateArc = false;
        double dfFirstChildApproximateArcRadius = 0.0;
        bool bFirstChildWasApproximateArcInvertedAxisOrder = false;

        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "curveMember") )
            {
                const CPLXMLNode* psCurveChild = GetChildElement(psChild);
                OGRGeometry* poGeom = nullptr;
                if( psCurveChild != nullptr )
                {
                    poGeom =
                        GML2OGRGeometry_XMLNode_Internal(
                            psCurveChild,
                            nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1,
                            nSRSDimension,
                            pszSRSName );
                }
                else
                {
                    if( psChild->psChild && psChild->psChild->eType ==
                        CXT_Attribute &&
                        psChild->psChild->psNext == nullptr &&
                        strcmp(psChild->psChild->pszValue, "xlink:href") == 0 )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot resolve xlink:href='%s'. "
                                 "Try setting GML_SKIP_RESOLVE_ELEMS=NONE",
                                 psChild->psChild->psChild->pszValue);
                    }
                    delete poRing;
                    delete poCC;
                    delete poGeom;
                    return nullptr;
                }

                // Try to join multiline string to one linestring.
                if( poGeom && wkbFlatten(poGeom->getGeometryType()) ==
                    wkbMultiLineString )
                {
                    poGeom =
                        OGRGeometryFactory::forceToLineString(poGeom, false);
                }

                if( poGeom == nullptr
                    || !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                {
                    delete poGeom;
                    delete poRing;
                    delete poCC;
                    return nullptr;
                }

                if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                    bChildrenAreAllLineString = false;

                // Ad-hoc logic to handle nicely connecting ArcByCenterPoint
                // with consecutive curves, as found in some AIXM files.
                bool bIsApproximateArc = false;
                const CPLXMLNode* psChild2, *psChild3;
                if( strcmp(BareGMLElement(psCurveChild->pszValue), "Curve") == 0 &&
                    (psChild2 = GetChildElement(psCurveChild)) != nullptr &&
                    strcmp(BareGMLElement(psChild2->pszValue), "segments") == 0 &&
                    (psChild3 = GetChildElement(psChild2)) != nullptr &&
                    strcmp(BareGMLElement(psChild3->pszValue), "ArcByCenterPoint") == 0 )
                {
                    storeArcByCenterPointParameters(psChild3,
                                               pszSRSName,
                                               bIsApproximateArc,
                                               dfLastCurveApproximateArcRadius,
                                               bLastCurveWasApproximateArcInvertedAxisOrder);
                    if( bIsFirstChild && bIsApproximateArc )
                    {
                        bFirstChildIsApproximateArc = true;
                        dfFirstChildApproximateArcRadius = dfLastCurveApproximateArcRadius;
                        bFirstChildWasApproximateArcInvertedAxisOrder = bLastCurveWasApproximateArcInvertedAxisOrder;
                    }
                    else if( psChild3->psNext )
                    {
                        bIsApproximateArc = false;
                    }
                }
                bIsFirstChild = false;

                if( poCC == nullptr && poRing == nullptr )
                {
                    poRing = poGeom->toCurve();
                }
                else
                {
                    if( poCC == nullptr )
                    {
                        poCC = new OGRCompoundCurve();
                        bool bIgnored = false;
                        if( !GML2OGRGeometry_AddToCompositeCurve(poCC, poRing, bIgnored) )
                        {
                            delete poGeom;
                            delete poRing;
                            delete poCC;
                            return nullptr;
                        }
                        poRing = nullptr;
                    }

                    connectArcByCenterPointToOtherSegments(poGeom, poCC,
                                                           bIsApproximateArc,
                                                           bLastCurveWasApproximateArc,
                                                           dfLastCurveApproximateArcRadius,
                                                           bLastCurveWasApproximateArcInvertedAxisOrder);

                    OGRCurve *poCurve = poGeom->toCurve();

                    bool bIgnored = false;
                    if( !GML2OGRGeometry_AddToCompositeCurve( poCC,
                                                              poCurve,
                                                              bIgnored ) )
                    {
                        delete poGeom;
                        delete poCC;
                        delete poRing;
                        return nullptr;
                    }
                }

                bLastCurveWasApproximateArc = bIsApproximateArc;
            }
        }

        if( poRing )
        {
            if( poRing->getNumPoints() >= 2 &&
                bFirstChildIsApproximateArc && !poRing->get_IsClosed() &&
                wkbFlatten(poRing->getGeometryType()) == wkbLineString )
            {
                OGRLineString* poLS = poRing->toLineString();

                OGRPoint p;
                OGRPoint p2;
                poLS->StartPoint(&p);
                poLS->EndPoint(&p2);
                double dfDistance = 0.0;
                if( bFirstChildWasApproximateArcInvertedAxisOrder )
                    dfDistance =
                        OGR_GreatCircle_Distance(
                            p.getX(), p.getY(),
                            p2.getX(), p2.getY());
                else
                    dfDistance =
                        OGR_GreatCircle_Distance(
                            p.getY(), p.getX(),
                            p2.getY(), p2.getX());
                if( dfDistance <
                    dfFirstChildApproximateArcRadius / 5.0 )
                {
                    CPLDebug("OGR",
                             "Moving approximate start of "
                             "ArcByCenterPoint to end of "
                             "curve");
                    poLS->setPoint(0, &p2);
                }
            }

            if( poRing->getNumPoints() < 2 || !poRing->get_IsClosed() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Non-closed ring");
                delete poRing;
                return nullptr;
            }
            return poRing;
        }

        if( poCC == nullptr )
            return nullptr;

        else if( /* bCastToLinearTypeIfPossible &&*/ bChildrenAreAllLineString )
        {
            return OGRCurve::CastToLinearRing(poCC);
        }
        else
        {
            if( poCC->getNumPoints() < 2 || !poCC->get_IsClosed() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Non-closed ring");
                delete poCC;
                return nullptr;
            }
            return poCC;
        }
    }

/* -------------------------------------------------------------------- */
/*      LineString                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "LineString")
        || EQUAL(pszBaseGeometry, "LineStringSegment")
        || EQUAL(pszBaseGeometry, "GeodesicString") )
    {
        OGRLineString *poLine = new OGRLineString();

        if( !ParseGMLCoordinates( psNode, poLine, nSRSDimension ) )
        {
            delete poLine;
            return nullptr;
        }

        return poLine;
    }

/* -------------------------------------------------------------------- */
/*      Arc                                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Arc") )
    {
        OGRCircularString *poCC = new OGRCircularString();

        if( !ParseGMLCoordinates( psNode, poCC, nSRSDimension ) )
        {
            delete poCC;
            return nullptr;
        }

        // Normally a gml:Arc has only 3 points of controls, but in the
        // wild we sometimes find GML with 5 points, so accept any odd
        // number >= 3 (ArcString should be used for > 3 points)
        if( poCC->getNumPoints() < 3 || (poCC->getNumPoints() % 2) != 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in Arc");
            delete poCC;
            return nullptr;
        }

        return poCC;
    }

/* -------------------------------------------------------------------- */
/*     ArcString                                                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "ArcString") )
    {
        OGRCircularString *poCC = new OGRCircularString();

        if( !ParseGMLCoordinates( psNode, poCC, nSRSDimension ) )
        {
            delete poCC;
            return nullptr;
        }

        if( poCC->getNumPoints() < 3 || (poCC->getNumPoints() % 2) != 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in ArcString");
            delete poCC;
            return nullptr;
        }

        return poCC;
    }

/* -------------------------------------------------------------------- */
/*      Circle                                                          */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Circle") )
    {
        OGRLineString *poLine = new OGRLineString();

        if( !ParseGMLCoordinates( psNode, poLine, nSRSDimension ) )
        {
            delete poLine;
            return nullptr;
        }

        if( poLine->getNumPoints() != 3 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in Circle");
            delete poLine;
            return nullptr;
        }

        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;
        if( !OGRGeometryFactory::GetCurveParameters(
                               poLine->getX(0), poLine->getY(0),
                               poLine->getX(1), poLine->getY(1),
                               poLine->getX(2), poLine->getY(2),
                               R, cx, cy, alpha0, alpha1, alpha2 ) )
        {
            delete poLine;
            return nullptr;
        }

        OGRCircularString *poCC = new OGRCircularString();
        OGRPoint p;
        poLine->getPoint(0, &p);
        poCC->addPoint(&p);
        poLine->getPoint(1, &p);
        poCC->addPoint(&p);
        poLine->getPoint(2, &p);
        poCC->addPoint(&p);
        const double alpha4 =
            alpha2 > alpha0 ? alpha0 + kdf2PI : alpha0 - kdf2PI;
        const double alpha3 = (alpha2 + alpha4) / 2.0;
        const double x = cx + R * cos(alpha3);
        const double y = cy + R * sin(alpha3);
        if( poCC->getCoordinateDimension() == 3 )
            poCC->addPoint( x, y, p.getZ() );
        else
            poCC->addPoint( x, y );
        poLine->getPoint(0, &p);
        poCC->addPoint(&p);
        delete poLine;
        return poCC;
    }

/* -------------------------------------------------------------------- */
/*      ArcByBulge                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "ArcByBulge") )
    {
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "bulge");
        if( psChild == nullptr || psChild->eType != CXT_Element ||
            psChild->psChild == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing bulge element." );
            return nullptr;
        }
        const double dfBulge = CPLAtof(psChild->psChild->pszValue);

        psChild = FindBareXMLChild( psNode, "normal");
        if( psChild == nullptr || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing normal element." );
            return nullptr;
        }
        double dfNormal = CPLAtof(psChild->psChild->pszValue);

        OGRLineString* poLS = new OGRLineString();
        if( !ParseGMLCoordinates( psNode, poLS, nSRSDimension ) )
        {
            delete poLS;
            return nullptr;
        }

        if( poLS->getNumPoints() != 2 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in ArcByBulge");
            delete poLS;
            return nullptr;
        }

        OGRCircularString *poCC = new OGRCircularString();
        OGRPoint p;
        poLS->getPoint(0, &p);
        poCC->addPoint(&p);

        const double dfMidX = (poLS->getX(0) + poLS->getX(1)) / 2.0;
        const double dfMidY = (poLS->getY(0) + poLS->getY(1)) / 2.0;
        const double dfDirX = (poLS->getX(1) - poLS->getX(0)) / 2.0;
        const double dfDirY = (poLS->getY(1) - poLS->getY(0)) / 2.0;
        double dfNormX = -dfDirY;
        double dfNormY = dfDirX;
        const double dfNorm = sqrt(dfNormX * dfNormX + dfNormY * dfNormY);
        if( dfNorm != 0.0 )
        {
            dfNormX /= dfNorm;
            dfNormY /= dfNorm;
        }
        const double dfNewX = dfMidX + dfNormX * dfBulge * dfNormal;
        const double dfNewY = dfMidY + dfNormY * dfBulge * dfNormal;

        if( poCC->getCoordinateDimension() == 3 )
            poCC->addPoint( dfNewX, dfNewY, p.getZ() );
        else
            poCC->addPoint( dfNewX, dfNewY );

        poLS->getPoint(1, &p);
        poCC->addPoint(&p);

        delete poLS;
        return poCC;
    }

/* -------------------------------------------------------------------- */
/*      ArcByCenterPoint                                                */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "ArcByCenterPoint") )
    {
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "radius");
        if( psChild == nullptr || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing radius element." );
            return nullptr;
        }
        const double dfRadius =
            CPLAtof(CPLGetXMLValue(psChild,
                                   nullptr, "0"));
        const char* pszUnits =
            CPLGetXMLValue(psChild, "uom", nullptr);

        psChild = FindBareXMLChild( psNode, "startAngle");
        if( psChild == nullptr || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing startAngle element." );
            return nullptr;
        }
        const double dfStartAngle =
            CPLAtof(CPLGetXMLValue(psChild,
                                   nullptr, "0"));

        psChild = FindBareXMLChild( psNode, "endAngle");
        if( psChild == nullptr || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing endAngle element." );
            return nullptr;
        }
        const double dfEndAngle =
            CPLAtof(CPLGetXMLValue(psChild,
                                   nullptr, "0"));

        OGRPoint p;
        if( !ParseGMLCoordinates( psNode, &p, nSRSDimension ) )
        {
            return nullptr;
        }

        bool bSRSUnitIsDegree = false;
        bool bInvertedAxisOrder = false;
        if( pszSRSName != nullptr )
        {
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput(pszSRSName) == OGRERR_NONE )
            {
                if( oSRS.IsGeographic() )
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                    bSRSUnitIsDegree = fabs(oSRS.GetAngularUnits(nullptr) -
                                            CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
                }
            }
        }

        const double dfCenterX = p.getX();
        const double dfCenterY = p.getY();

        double dfDistance;
        if( bSRSUnitIsDegree && pszUnits != nullptr &&
            (dfDistance = GetDistanceInMetre(dfRadius, pszUnits)) > 0 )
        {
            OGRLineString* poLS = new OGRLineString();
            // coverity[tainted_data]
            const double dfStep =
                CPLAtof(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
            const double dfSign = dfStartAngle < dfEndAngle ? 1 : -1;
            for( double dfAngle = dfStartAngle;
                 (dfAngle - dfEndAngle) * dfSign < 0;
                 dfAngle += dfSign * dfStep)
            {
                double dfLong = 0.0;
                double dfLat = 0.0;
                if( bInvertedAxisOrder )
                {
                    OGR_GreatCircle_ExtendPosition(
                       dfCenterX, dfCenterY,
                       dfDistance,
                       // See https://ext.eurocontrol.int/aixm_confluence/display/ACG/ArcByCenterPoint+Interpretation+Summary
                       dfAngle,
                       &dfLat, &dfLong);
                    p.setX( dfLat ); // yes, external code will do the swap later
                    p.setY( dfLong );
                }
                else
                {
                    OGR_GreatCircle_ExtendPosition(dfCenterY, dfCenterX,
                                             dfDistance, 90-dfAngle,
                                             &dfLat, &dfLong);
                    p.setX( dfLong );
                    p.setY( dfLat );
                }
                poLS->addPoint(&p);
            }

            double dfLong = 0.0;
            double dfLat = 0.0;
            if( bInvertedAxisOrder )
            {
                OGR_GreatCircle_ExtendPosition(dfCenterX, dfCenterY,
                                         dfDistance,
                                         dfEndAngle,
                                         &dfLat, &dfLong);
                p.setX( dfLat ); // yes, external code will do the swap later
                p.setY( dfLong );
            }
            else
            {
                OGR_GreatCircle_ExtendPosition(dfCenterY, dfCenterX,
                                         dfDistance, 90-dfEndAngle,
                                         &dfLat, &dfLong);
                p.setX( dfLong );
                p.setY( dfLat );
            }
            poLS->addPoint(&p);

            return poLS;
        }

        OGRCircularString *poCC = new OGRCircularString();
        p.setX(dfCenterX + dfRadius * cos(dfStartAngle * kdfD2R));
        p.setY(dfCenterY + dfRadius * sin(dfStartAngle * kdfD2R));
        poCC->addPoint(&p);
        const double dfAverageAngle = (dfStartAngle + dfEndAngle) / 2.0;
        p.setX(dfCenterX + dfRadius * cos(dfAverageAngle * kdfD2R));
        p.setY(dfCenterY + dfRadius * sin(dfAverageAngle * kdfD2R));
        poCC->addPoint(&p);
        p.setX(dfCenterX + dfRadius * cos(dfEndAngle * kdfD2R));
        p.setY(dfCenterY + dfRadius * sin(dfEndAngle * kdfD2R));
        poCC->addPoint(&p);
        return poCC;
    }

/* -------------------------------------------------------------------- */
/*      CircleByCenterPoint                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "CircleByCenterPoint") )
    {
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "radius");
        if( psChild == nullptr || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing radius element." );
            return nullptr;
        }
        const double dfRadius =
            CPLAtof(CPLGetXMLValue(psChild,
                                   nullptr, "0"));
        const char* pszUnits =
            CPLGetXMLValue(psChild, "uom", nullptr);

        OGRPoint p;
        if( !ParseGMLCoordinates( psNode, &p, nSRSDimension ) )
        {
            return nullptr;
        }

        bool bSRSUnitIsDegree = false;
        bool bInvertedAxisOrder = false;
        if( pszSRSName != nullptr )
        {
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput(pszSRSName) == OGRERR_NONE )
            {
                if( oSRS.IsGeographic() )
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                    bSRSUnitIsDegree = fabs(oSRS.GetAngularUnits(nullptr) -
                                            CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
                }
            }
        }

        const double dfCenterX = p.getX();
        const double dfCenterY = p.getY();

        double dfDistance;
        if( bSRSUnitIsDegree && pszUnits != nullptr &&
            (dfDistance = GetDistanceInMetre(dfRadius, pszUnits)) > 0 )
        {
            OGRLineString* poLS = new OGRLineString();
            const double dfStep =
                CPLAtof(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
            for( double dfAngle = 0; dfAngle < 360; dfAngle += dfStep )
            {
                double dfLong = 0.0;
                double dfLat = 0.0;
                if( bInvertedAxisOrder )
                {
                    OGR_GreatCircle_ExtendPosition(dfCenterX, dfCenterY,
                                             dfDistance, dfAngle,
                                             &dfLat, &dfLong);
                    p.setX( dfLat ); // yes, external code will do the swap later
                    p.setY( dfLong );
                }
                else
                {
                    OGR_GreatCircle_ExtendPosition(dfCenterY, dfCenterX,
                                             dfDistance, dfAngle,
                                             &dfLat, &dfLong);
                    p.setX( dfLong );
                    p.setY( dfLat );
                }
                poLS->addPoint(&p);
            }
            poLS->getPoint(0, &p);
            poLS->addPoint(&p);
            return poLS;
        }

        OGRCircularString *poCC = new OGRCircularString();
        p.setX( dfCenterX - dfRadius );
        p.setY( dfCenterY );
        poCC->addPoint(&p);
        p.setX( dfCenterX + dfRadius);
        p.setY( dfCenterY );
        poCC->addPoint(&p);
        p.setX( dfCenterX - dfRadius );
        p.setY( dfCenterY );
        poCC->addPoint(&p);
        return poCC;
    }

/* -------------------------------------------------------------------- */
/*      PointType                                                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "PointType")
        || EQUAL(pszBaseGeometry, "Point")
        || EQUAL(pszBaseGeometry, "ConnectionPoint") )
    {
        OGRPoint *poPoint = new OGRPoint();

        if( !ParseGMLCoordinates( psNode, poPoint, nSRSDimension ) )
        {
            delete poPoint;
            return nullptr;
        }

        return poPoint;
    }

/* -------------------------------------------------------------------- */
/*      Box                                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "BoxType") || EQUAL(pszBaseGeometry, "Box") )
    {
        OGRLineString oPoints;

        if( !ParseGMLCoordinates( psNode, &oPoints, nSRSDimension ) )
            return nullptr;

        if( oPoints.getNumPoints() < 2 )
            return nullptr;

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
    if( EQUAL(pszBaseGeometry, "Envelope") )
    {
        const CPLXMLNode* psLowerCorner =
            FindBareXMLChild( psNode, "lowerCorner");
        const CPLXMLNode* psUpperCorner =
            FindBareXMLChild( psNode, "upperCorner");
        if( psLowerCorner == nullptr || psUpperCorner == nullptr )
            return nullptr;
        const char* pszLowerCorner = GetElementText(psLowerCorner);
        const char* pszUpperCorner = GetElementText(psUpperCorner);
        if( pszLowerCorner == nullptr || pszUpperCorner == nullptr )
            return nullptr;
        char** papszLowerCorner = CSLTokenizeString(pszLowerCorner);
        char** papszUpperCorner = CSLTokenizeString(pszUpperCorner);
        const int nTokenCountLC = CSLCount(papszLowerCorner);
        const int nTokenCountUC = CSLCount(papszUpperCorner);
        if( nTokenCountLC < 2 || nTokenCountUC < 2 )
        {
            CSLDestroy(papszLowerCorner);
            CSLDestroy(papszUpperCorner);
            return nullptr;
        }

        const double dfLLX = CPLAtof(papszLowerCorner[0]);
        const double dfLLY = CPLAtof(papszLowerCorner[1]);
        const double dfURX = CPLAtof(papszUpperCorner[0]);
        const double dfURY = CPLAtof(papszUpperCorner[1]);
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

/* --------------------------------------------------------------------- */
/*      MultiPolygon / MultiSurface / CompositeSurface                   */
/*                                                                       */
/* For CompositeSurface, this is a very rough approximation to deal with */
/* it as a MultiPolygon, because it can several faces of a 3D volume.    */
/* --------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "MultiPolygon") ||
        EQUAL(pszBaseGeometry, "MultiSurface") ||
        EQUAL(pszBaseGeometry, "CompositeSurface") )
    {
        OGRMultiSurface* poMS =
            EQUAL(pszBaseGeometry, "MultiPolygon")
            ? new OGRMultiPolygon()
            : new OGRMultiSurface();
        bool bReconstructTopology = false;
        bool bChildrenAreAllPolygons = true;

        // Iterate over children.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            const char* pszMemberElement = BareGMLElement(psChild->pszValue);
            if( psChild->eType == CXT_Element
                && (EQUAL(pszMemberElement, "polygonMember") ||
                    EQUAL(pszMemberElement, "surfaceMember")) )
            {
                const CPLXMLNode* psSurfaceChild = GetChildElement(psChild);

                if( psSurfaceChild != nullptr )
                {
                    // Cf #5421 where there are PolygonPatch with only inner
                    // rings.
                    const CPLXMLNode* psPolygonPatch =
                        GetChildElement(GetChildElement(psSurfaceChild));
                    const CPLXMLNode* psPolygonPatchChild = nullptr;
                    if( psPolygonPatch != nullptr &&
                        psPolygonPatch->eType == CXT_Element &&
                        EQUAL(BareGMLElement(psPolygonPatch->pszValue),
                              "PolygonPatch") &&
                        (psPolygonPatchChild = GetChildElement(psPolygonPatch))
                        != nullptr &&
                        EQUAL(BareGMLElement(psPolygonPatchChild->pszValue),
                              "interior") )
                    {
                        // Find all inner rings
                        for( const CPLXMLNode* psChild2 =
                                 psPolygonPatch->psChild;
                             psChild2 != nullptr;
                             psChild2 = psChild2->psNext )
                        {
                            if( psChild2->eType == CXT_Element
                                && (EQUAL(BareGMLElement(psChild2->pszValue),
                                          "interior")))
                            {
                                const CPLXMLNode* psInteriorChild =
                                    GetChildElement(psChild2);
                                OGRGeometry* poRing =
                                    psInteriorChild == nullptr
                                    ? nullptr
                                    : GML2OGRGeometry_XMLNode_Internal(
                                          psInteriorChild,
                                          nPseudoBoolGetSecondaryGeometryOption,
                                          nRecLevel + 1,
                                          nSRSDimension, pszSRSName );
                                if( poRing == nullptr )
                                {
                                    CPLError( CE_Failure, CPLE_AppDefined,
                                              "Invalid interior ring");
                                    delete poMS;
                                    return nullptr;
                                }
                                if( !EQUAL(poRing->getGeometryName(),
                                           "LINEARRING") )
                                {
                                    CPLError(
                                        CE_Failure, CPLE_AppDefined,
                                        "%s: Got %.500s geometry as "
                                        "innerBoundaryIs instead of "
                                        "LINEARRING.",
                                        pszBaseGeometry,
                                        poRing->getGeometryName());
                                    delete poRing;
                                    delete poMS;
                                    return nullptr;
                                }

                                bReconstructTopology = true;
                                OGRPolygon *poPolygon = new OGRPolygon();
                                OGRLinearRing *poLinearRing =
                                    poRing->toLinearRing();
                                poPolygon->addRingDirectly(poLinearRing);
                                poMS->addGeometryDirectly( poPolygon );
                            }
                        }
                    }
                    else
                    {
                        OGRGeometry* poGeom =
                            GML2OGRGeometry_XMLNode_Internal( psSurfaceChild,
                                  nPseudoBoolGetSecondaryGeometryOption,
                                  nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( !GML2OGRGeometry_AddToMultiSurface(
                                 poMS, poGeom,
                                 pszMemberElement,
                                 bChildrenAreAllPolygons) )
                        {
                            delete poGeom;
                            delete poMS;
                            return nullptr;
                        }
                    }
                }
            }
            else if( psChild->eType == CXT_Element
                     && EQUAL(pszMemberElement, "surfaceMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr;
                     psChild2 = psChild2->psNext )
                {
                    pszMemberElement = BareGMLElement(psChild2->pszValue);
                    if( psChild2->eType == CXT_Element
                        && (EQUAL(pszMemberElement, "Surface") ||
                            EQUAL(pszMemberElement, "Polygon") ||
                            EQUAL(pszMemberElement, "PolygonPatch") ||
                            EQUAL(pszMemberElement, "CompositeSurface")) )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( !GML2OGRGeometry_AddToMultiSurface(
                                poMS, poGeom,
                                pszMemberElement,
                                bChildrenAreAllPolygons) )
                        {
                            delete poGeom;
                            delete poMS;
                            return nullptr;
                        }
                    }
                }
            }
        }

        if( bReconstructTopology && bChildrenAreAllPolygons )
        {
            OGRMultiPolygon* poMPoly =
                wkbFlatten(poMS->getGeometryType()) == wkbMultiSurface
                ? OGRMultiSurface::CastToMultiPolygon(poMS)
                : poMS->toMultiPolygon();
            const int nPolygonCount = poMPoly->getNumGeometries();
            OGRGeometry** papoPolygons = new OGRGeometry*[ nPolygonCount ];
            for( int i = 0; i < nPolygonCount; i++ )
            {
                papoPolygons[i] = poMPoly->getGeometryRef(0);
                poMPoly->removeGeometry(0, FALSE);
            }
            delete poMPoly;
            int bResultValidGeometry = FALSE;
            OGRGeometry* poRet = OGRGeometryFactory::organizePolygons(
                papoPolygons, nPolygonCount, &bResultValidGeometry );
            delete[] papoPolygons;
            return poRet;
        }
        else
        {
            if( /* bCastToLinearTypeIfPossible && */
                wkbFlatten(poMS->getGeometryType()) == wkbMultiSurface &&
                bChildrenAreAllPolygons )
            {
                return OGRMultiSurface::CastToMultiPolygon(poMS);
            }

            return poMS;
        }
    }

/* -------------------------------------------------------------------- */
/*      MultiPoint                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "MultiPoint") )
    {
        OGRMultiPoint *poMP = new OGRMultiPoint();

        // Collect points.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "pointMember") )
            {
                const CPLXMLNode* psPointChild = GetChildElement(psChild);

                if( psPointChild != nullptr )
                {
                    OGRGeometry *poPointMember =
                        GML2OGRGeometry_XMLNode_Internal( psPointChild,
                              nPseudoBoolGetSecondaryGeometryOption,
                              nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poPointMember == nullptr ||
                        wkbFlatten(poPointMember->getGeometryType()) !=
                        wkbPoint )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "MultiPoint: Got %.500s geometry as "
                                  "pointMember instead of POINT",
                                  poPointMember
                                  ? poPointMember->getGeometryName() : "NULL" );
                        delete poPointMember;
                        delete poMP;
                        return nullptr;
                    }

                    poMP->addGeometryDirectly( poPointMember );
                }
            }
            else if( psChild->eType == CXT_Element
                     && EQUAL(BareGMLElement(psChild->pszValue),
                              "pointMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element
                        && (EQUAL(BareGMLElement(psChild2->pszValue),
                                  "Point")) )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( poGeom == nullptr )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined, "Invalid %s",
                                    BareGMLElement(psChild2->pszValue));
                            delete poMP;
                            return nullptr;
                        }

                        if( wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
                        {
                            OGRPoint *poPoint = poGeom->toPoint();
                            poMP->addGeometryDirectly( poPoint );
                        }
                        else
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "Got %.500s geometry as pointMember "
                                      "instead of POINT.",
                                      poGeom->getGeometryName() );
                            delete poGeom;
                            delete poMP;
                            return nullptr;
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
    if( EQUAL(pszBaseGeometry, "MultiLineString") )
    {
        OGRMultiLineString *poMLS = new OGRMultiLineString();

        // Collect lines.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),
                         "lineStringMember") )
            {
                const CPLXMLNode* psLineStringChild = GetChildElement(psChild);
                OGRGeometry *poGeom =
                    psLineStringChild == nullptr
                    ? nullptr
                    : GML2OGRGeometry_XMLNode_Internal(
                          psLineStringChild,
                          nPseudoBoolGetSecondaryGeometryOption,
                          nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poGeom == nullptr
                    || wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MultiLineString: Got %.500s geometry as Member "
                             "instead of LINESTRING.",
                             poGeom ? poGeom->getGeometryName() : "NULL" );
                    delete poGeom;
                    delete poMLS;
                    return nullptr;
                }

                poMLS->addGeometryDirectly( poGeom );
            }
        }

        return poMLS;
    }

/* -------------------------------------------------------------------- */
/*      MultiCurve                                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "MultiCurve") )
    {
        OGRMultiCurve *poMC = new OGRMultiCurve();
        bool bChildrenAreAllLineString = true;

        // Collect curveMembers.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "curveMember") )
            {
                const CPLXMLNode *psChild2 = GetChildElement(psChild);
                if( psChild2 != nullptr )  // Empty curveMember is valid.
                {
                    OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psChild2, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poGeom == nullptr ||
                        !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "MultiCurve: Got %.500s geometry as Member "
                                 "instead of a curve.",
                                 poGeom ? poGeom->getGeometryName() : "NULL" );
                        if( poGeom != nullptr ) delete poGeom;
                        delete poMC;
                        return nullptr;
                    }

                    if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                        bChildrenAreAllLineString = false;

                    if( poMC->addGeometryDirectly( poGeom ) != OGRERR_NONE )
                    {
                        delete poGeom;
                    }
            }
            }
            else if( psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue), "curveMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( poGeom == nullptr ||
                            !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "MultiCurve: Got %.500s geometry as "
                                "Member instead of a curve.",
                                poGeom ? poGeom->getGeometryName() : "NULL" );
                            if( poGeom != nullptr ) delete poGeom;
                            delete poMC;
                            return nullptr;
                        }

                        if( wkbFlatten(poGeom->getGeometryType()) !=
                            wkbLineString )
                            bChildrenAreAllLineString = false;

                        if( poMC->addGeometryDirectly( poGeom ) != OGRERR_NONE )
                        {
                            delete poGeom;
                        }
                    }
                }
            }
        }

        if( /* bCastToLinearTypeIfPossible && */ bChildrenAreAllLineString )
        {
            return OGRMultiCurve::CastToMultiLineString(poMC);
        }

        return poMC;
    }

/* -------------------------------------------------------------------- */
/*      CompositeCurve                                                  */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "CompositeCurve") )
    {
        OGRCompoundCurve *poCC = new OGRCompoundCurve();
        bool bChildrenAreAllLineString = true;

        // Collect curveMembers.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "curveMember") )
            {
                const CPLXMLNode *psChild2 = GetChildElement(psChild);
                if( psChild2 != nullptr )  // Empty curveMember is valid.
                {
                    OGRGeometry*poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psChild2, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( !GML2OGRGeometry_AddToCompositeCurve(
                             poCC, poGeom, bChildrenAreAllLineString) )
                    {
                        delete poGeom;
                        delete poCC;
                        return nullptr;
                    }
                }
            }
            else if( psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue), "curveMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( !GML2OGRGeometry_AddToCompositeCurve(
                                poCC, poGeom, bChildrenAreAllLineString) )
                        {
                            delete poGeom;
                            delete poCC;
                            return nullptr;
                        }
                    }
                }
            }
        }

        if( /* bCastToLinearTypeIfPossible && */ bChildrenAreAllLineString )
        {
            return OGRCurve::CastToLineString(poCC);
        }

        return poCC;
    }

/* -------------------------------------------------------------------- */
/*      Curve                                                           */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Curve") )
    {
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "segments");
        if( psChild == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GML3 Curve geometry lacks segments element." );
            return nullptr;
        }

        OGRGeometry *poGeom =
            GML2OGRGeometry_XMLNode_Internal(
                psChild, nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension, pszSRSName );
        if( poGeom == nullptr ||
            !OGR_GT_IsCurve(poGeom->getGeometryType()) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                "Curve: Got %.500s geometry as Member instead of segments.",
                poGeom ? poGeom->getGeometryName() : "NULL" );
            if( poGeom != nullptr ) delete poGeom;
            return nullptr;
        }

        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      segments                                                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "segments") )
    {
        OGRCurve* poCurve = nullptr;
        OGRCompoundCurve *poCC = nullptr;
        bool bChildrenAreAllLineString = true;

        bool bLastCurveWasApproximateArc = false;
        bool bLastCurveWasApproximateArcInvertedAxisOrder = false;
        double dfLastCurveApproximateArcRadius = 0.0;

        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )

        {
            if( psChild->eType == CXT_Element
                // && (EQUAL(BareGMLElement(psChild->pszValue),
                //           "LineStringSegment") ||
                //     EQUAL(BareGMLElement(psChild->pszValue),
                //           "GeodesicString") ||
                //    EQUAL(BareGMLElement(psChild->pszValue), "Arc") ||
                //    EQUAL(BareGMLElement(psChild->pszValue), "Circle"))
                )
            {
                OGRGeometry *poGeom =
                    GML2OGRGeometry_XMLNode_Internal(
                        psChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poGeom == nullptr ||
                    !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "segments: Got %.500s geometry as Member "
                             "instead of curve.",
                             poGeom ? poGeom->getGeometryName() : "NULL");
                    delete poGeom;
                    delete poCurve;
                    delete poCC;
                    return nullptr;
                }


                // Ad-hoc logic to handle nicely connecting ArcByCenterPoint
                // with consecutive curves, as found in some AIXM files.
                bool bIsApproximateArc = false;
                if( strcmp(BareGMLElement(psChild->pszValue), "ArcByCenterPoint") == 0 )
                {
                    storeArcByCenterPointParameters(psChild,
                                               pszSRSName,
                                               bIsApproximateArc,
                                               dfLastCurveApproximateArcRadius,
                                               bLastCurveWasApproximateArcInvertedAxisOrder);
                }

                if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                    bChildrenAreAllLineString = false;

                if( poCC == nullptr && poCurve == nullptr )
                {
                    poCurve = poGeom->toCurve();
                }
                else
                {
                    if( poCC == nullptr )
                    {
                        poCC = new OGRCompoundCurve();
                        if( poCC->addCurveDirectly(poCurve) != OGRERR_NONE )
                        {
                            delete poGeom;
                            delete poCurve;
                            delete poCC;
                            return nullptr;
                        }
                        poCurve = nullptr;
                    }

                    connectArcByCenterPointToOtherSegments(poGeom, poCC,
                                                           bIsApproximateArc,
                                                           bLastCurveWasApproximateArc,
                                                           dfLastCurveApproximateArcRadius,
                                                           bLastCurveWasApproximateArcInvertedAxisOrder);

                    OGRCurve *poAsCurve = poGeom->toCurve();
                    if( poCC->addCurveDirectly(poAsCurve) != OGRERR_NONE )
                    {
                        delete poGeom;
                        delete poCC;
                        return nullptr;
                    }
                }

                bLastCurveWasApproximateArc = bIsApproximateArc;
            }
        }

        if( poCurve != nullptr )
            return poCurve;
        if( poCC == nullptr )
            return nullptr;

        if( /* bCastToLinearTypeIfPossible && */ bChildrenAreAllLineString )
        {
            return OGRCurve::CastToLineString(poCC);
        }

        return poCC;
    }

/* -------------------------------------------------------------------- */
/*      MultiGeometry                                                   */
/* CAUTION: OGR < 1.8.0 produced GML with GeometryCollection, which is  */
/* not a valid GML 2 keyword! The right name is MultiGeometry. Let's be */
/* tolerant with the non compliant files we produced.                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "MultiGeometry") ||
        EQUAL(pszBaseGeometry, "GeometryCollection") )
    {
        OGRGeometryCollection *poGC = new OGRGeometryCollection();

        // Collect geoms.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "geometryMember") )
            {
                const CPLXMLNode* psGeometryChild = GetChildElement(psChild);

                if( psGeometryChild != nullptr )
                {
                    OGRGeometry *poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psGeometryChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poGeom == nullptr )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "GeometryCollection: Failed to get geometry "
                                  "in geometryMember" );
                        delete poGeom;
                        delete poGC;
                        return nullptr;
                    }

                    poGC->addGeometryDirectly( poGeom );
                }
            }
            else if( psChild->eType == CXT_Element
                     && EQUAL(BareGMLElement(psChild->pszValue), "geometryMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( poGeom == nullptr )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                    "GeometryCollection: Failed to get geometry "
                                    "in geometryMember" );
                            delete poGeom;
                            delete poGC;
                            return nullptr;
                        }

                        poGC->addGeometryDirectly( poGeom );
                    }
                }
            }
        }

        return poGC;
    }

/* -------------------------------------------------------------------- */
/*      Directed Edge                                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "directedEdge") )
    {
        // Collect edge.
        const CPLXMLNode *psEdge = FindBareXMLChild(psNode, "Edge");
        if( psEdge == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to get Edge element in directedEdge" );
            return nullptr;
        }

        // TODO(schwehr): Localize vars after removing gotos.
        OGRGeometry *poGeom = nullptr;
        const CPLXMLNode *psNodeElement = nullptr;
        const CPLXMLNode *psPointProperty = nullptr;
        const CPLXMLNode *psPoint = nullptr;
        bool bNodeOrientation = true;
        OGRPoint *poPositiveNode = nullptr;
        OGRPoint *poNegativeNode = nullptr;

        const bool bEdgeOrientation = GetElementOrientation(psNode);

        if( bGetSecondaryGeometry )
        {
            const CPLXMLNode *psdirectedNode =
                FindBareXMLChild(psEdge, "directedNode");
            if( psdirectedNode == nullptr ) goto nonode;

            bNodeOrientation = GetElementOrientation( psdirectedNode );

            psNodeElement = FindBareXMLChild(psdirectedNode, "Node");
            if( psNodeElement == nullptr ) goto nonode;

            psPointProperty = FindBareXMLChild(psNodeElement, "pointProperty");
            if( psPointProperty == nullptr )
                psPointProperty = FindBareXMLChild(psNodeElement,
                                                   "connectionPointProperty");
            if( psPointProperty == nullptr ) goto nonode;

            psPoint = FindBareXMLChild(psPointProperty, "Point");
            if( psPoint == nullptr )
                psPoint = FindBareXMLChild(psPointProperty, "ConnectionPoint");
            if( psPoint == nullptr ) goto nonode;

            poGeom = GML2OGRGeometry_XMLNode_Internal(
                psPoint, nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension, pszSRSName, true );
            if( poGeom == nullptr
                || wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
            {
                // CPLError( CE_Failure, CPLE_AppDefined,
                //           "Got %.500s geometry as Member instead of POINT.",
                //           poGeom ? poGeom->getGeometryName() : "NULL" );
                if( poGeom != nullptr) delete poGeom;
                goto nonode;
            }

            {
                OGRPoint *poPoint = poGeom->toPoint();
                if( ( bNodeOrientation == bEdgeOrientation ) != bOrientation )
                    poPositiveNode = poPoint;
                else
                    poNegativeNode = poPoint;
            }

            // Look for the other node.
            psdirectedNode = psdirectedNode->psNext;
            while( psdirectedNode != nullptr &&
                   !EQUAL( psdirectedNode->pszValue, "directedNode" ) )
                psdirectedNode = psdirectedNode->psNext;
            if( psdirectedNode == nullptr ) goto nonode;

            if( GetElementOrientation( psdirectedNode ) == bNodeOrientation )
                goto nonode;

            psNodeElement = FindBareXMLChild(psEdge, "Node");
            if( psNodeElement == nullptr ) goto nonode;

            psPointProperty = FindBareXMLChild(psNodeElement, "pointProperty");
            if( psPointProperty == nullptr )
                psPointProperty =
                    FindBareXMLChild(psNodeElement, "connectionPointProperty");
            if( psPointProperty == nullptr ) goto nonode;

            psPoint = FindBareXMLChild(psPointProperty, "Point");
            if( psPoint == nullptr )
                psPoint = FindBareXMLChild(psPointProperty, "ConnectionPoint");
            if( psPoint == nullptr ) goto nonode;

            poGeom = GML2OGRGeometry_XMLNode_Internal(
                psPoint, nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension, pszSRSName, true );
            if( poGeom == nullptr
                || wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
            {
                // CPLError( CE_Failure, CPLE_AppDefined,
                //           "Got %.500s geometry as Member instead of POINT.",
                //           poGeom ? poGeom->getGeometryName() : "NULL" );
                if( poGeom != nullptr) delete poGeom;
                goto nonode;
            }

            {
                OGRPoint *poPoint = poGeom->toPoint();
                if( ( bNodeOrientation == bEdgeOrientation ) != bOrientation )
                    poNegativeNode = poPoint;
                else
                    poPositiveNode = poPoint;
             }

            {
                // Create a scope so that poMP can be initialized with goto
                // above and label below.
                OGRMultiPoint *poMP = new OGRMultiPoint();
                poMP->addGeometryDirectly( poNegativeNode );
                poMP->addGeometryDirectly( poPositiveNode );

                return poMP;
            }
            nonode:;
        }

        // Collect curveproperty.
        const CPLXMLNode *psCurveProperty =
            FindBareXMLChild(psEdge, "curveProperty");
        if( psCurveProperty == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "directedEdge: Failed to get curveProperty in Edge" );
            return nullptr;
        }

        const CPLXMLNode *psCurve =
            FindBareXMLChild(psCurveProperty, "LineString");
        if( psCurve == nullptr )
            psCurve = FindBareXMLChild(psCurveProperty, "Curve");
        if( psCurve == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "directedEdge: Failed to get LineString or "
                     "Curve tag in curveProperty");
            return nullptr;
        }

        OGRGeometry* poLineStringBeforeCast = GML2OGRGeometry_XMLNode_Internal(
            psCurve, nPseudoBoolGetSecondaryGeometryOption,
            nRecLevel + 1, nSRSDimension, pszSRSName, true );
        if( poLineStringBeforeCast == nullptr
            || wkbFlatten(poLineStringBeforeCast->getGeometryType()) !=
            wkbLineString )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Got %.500s geometry as Member instead of LINESTRING.",
                     poLineStringBeforeCast
                     ? poLineStringBeforeCast->getGeometryName()
                     : "NULL" );
            delete poLineStringBeforeCast;
            return nullptr;
        }
        OGRLineString *poLineString = poLineStringBeforeCast->toLineString();

        if( bGetSecondaryGeometry )
        {
            // Choose a point based on the orientation.
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

            OGRMultiPoint *poMP = new OGRMultiPoint();
            poMP->addGeometryDirectly( poNegativeNode );
            poMP->addGeometryDirectly( poPositiveNode );

            return poMP;
        }

        // correct orientation of the line string
        if( bEdgeOrientation != bOrientation )
        {
            int iStartCoord = 0;
            int iEndCoord = poLineString->getNumPoints() - 1;
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
    if( EQUAL(pszBaseGeometry, "TopoCurve") )
    {
        OGRMultiLineString *poMLS = nullptr;
        OGRMultiPoint *poMP = nullptr;

        if( bGetSecondaryGeometry )
            poMP = new OGRMultiPoint();
        else
            poMLS = new OGRMultiLineString();

        // Collect directedEdges.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "directedEdge"))
            {
                OGRGeometry *poGeom = GML2OGRGeometry_XMLNode_Internal(
                    psChild, nPseudoBoolGetSecondaryGeometryOption,
                    nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poGeom == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Failed to get geometry in directedEdge" );
                    delete poGeom;
                    if( bGetSecondaryGeometry )
                        delete poMP;
                    else
                        delete poMLS;
                    return nullptr;
                }

                // Add the two points corresponding to the two nodes to poMP.
                if( bGetSecondaryGeometry &&
                     wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
                {
                    OGRMultiPoint *poMultiPoint = poGeom->toMultiPoint();

                    //TODO: TopoCurve geometries with more than one
                    //      directedEdge elements were not tested.
                    if( poMP->getNumGeometries() <= 0 ||
                        !(poMP->getGeometryRef( poMP->getNumGeometries() - 1 )->
                          Equals(poMultiPoint->getGeometryRef(0))) )
                    {
                        poMP->addGeometry(poMultiPoint->getGeometryRef(0) );
                    }
                    poMP->addGeometry(poMultiPoint->getGeometryRef(1) );
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
                              poGeom->getGeometryName(),
                              bGetSecondaryGeometry?"MULTIPOINT":"LINESTRING");
                    delete poGeom;
                    if( bGetSecondaryGeometry )
                        delete poMP;
                    else
                        delete poMLS;
                    return nullptr;
                }
            }
        }

        if( bGetSecondaryGeometry )
            return poMP;

        return poMLS;
    }

/* -------------------------------------------------------------------- */
/*      TopoSurface                                                     */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "TopoSurface") )
    {
        /****************************************************************/
        /* applying the FaceHoleNegative = false rules                  */
        /*                                                              */
        /* - each <TopoSurface> is expected to represent a MultiPolygon */
        /* - each <Face> is expected to represent a distinct Polygon,   */
        /*   this including any possible Interior Ring (holes);         */
        /*   orientation="+/-" plays no role at all to identify "holes" */
        /* - each <Edge> within a <Face> may indifferently represent    */
        /*   an element of the Exterior or Interior Boundary; relative  */
        /*   order of <Edges> is absolutely irrelevant.                 */
        /****************************************************************/
        /* Contributor: Alessandro Furieri, a.furieri@lqt.it            */
        /* Developed for Faunalia (http://www.faunalia.it)              */
        /* with funding from Regione Toscana -                          */
        /* Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE       */
        /****************************************************************/
        if( !bFaceHoleNegative )
        {
            if( bGetSecondaryGeometry )
                return nullptr;

#ifndef HAVE_GEOS
            static bool bWarningAlreadyEmitted = false;
            if( !bWarningAlreadyEmitted )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Interpreating that GML TopoSurface geometry requires GDAL "
                    "to be built with GEOS support.  As a workaround, you can "
                    "try defining the GML_FACE_HOLE_NEGATIVE configuration "
                    "option to YES, so that the 'old' interpretation algorithm "
                    "is used. But be warned that the result might be "
                    "incorrect.");
                bWarningAlreadyEmitted = true;
            }
            return nullptr;
#else
            OGRMultiPolygon *poTS = new OGRMultiPolygon();

            // Collect directed faces.
            for( const CPLXMLNode *psChild = psNode->psChild;
                 psChild != nullptr;
                 psChild = psChild->psNext )
            {
              if( psChild->eType == CXT_Element
              && EQUAL(BareGMLElement(psChild->pszValue), "directedFace") )
              {
                // Collect next face (psChild->psChild).
                const CPLXMLNode *psFaceChild = GetChildElement(psChild);

                while( psFaceChild != nullptr &&
                       !(psFaceChild->eType == CXT_Element &&
                         EQUAL(BareGMLElement(psFaceChild->pszValue), "Face")) )
                        psFaceChild = psFaceChild->psNext;

                if( psFaceChild == nullptr )
                    continue;

                OGRMultiLineString *poCollectedGeom = new OGRMultiLineString();

                // Collect directed edges of the face.
                for( const CPLXMLNode *psDirectedEdgeChild =
                         psFaceChild->psChild;
                     psDirectedEdgeChild != nullptr;
                     psDirectedEdgeChild = psDirectedEdgeChild->psNext )
                {
                  if( psDirectedEdgeChild->eType == CXT_Element &&
                      EQUAL(BareGMLElement(psDirectedEdgeChild->pszValue),
                            "directedEdge") )
                  {
                      OGRGeometry *poEdgeGeom =
                          GML2OGRGeometry_XMLNode_Internal(
                              psDirectedEdgeChild,
                              nPseudoBoolGetSecondaryGeometryOption,
                              nRecLevel + 1,
                              nSRSDimension,
                              pszSRSName,
                              true);

                      if( poEdgeGeom == nullptr ||
                          wkbFlatten(poEdgeGeom->getGeometryType()) !=
                          wkbLineString )
                      {
                          CPLError( CE_Failure, CPLE_AppDefined,
                                    "Failed to get geometry in directedEdge" );
                          delete poEdgeGeom;
                          delete poCollectedGeom;
                          delete poTS;
                          return nullptr;
                      }

                      poCollectedGeom->addGeometryDirectly( poEdgeGeom );
                  }
                }

                OGRGeometry *poFaceCollectionGeom =
                    poCollectedGeom->Polygonize();
                if( poFaceCollectionGeom == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Failed to assemble Edges in Face" );
                    delete poCollectedGeom;
                    delete poTS;
                    return nullptr;
                }

                OGRPolygon *poFaceGeom = GML2FaceExtRing(poFaceCollectionGeom);

                if( poFaceGeom == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                                "Failed to build Polygon for Face" );
                    delete poCollectedGeom;
                    delete poTS;
                    return nullptr;
                }
                else
                {
                    int iCount = poTS->getNumGeometries();
                    if( iCount == 0)
                    {
                        // Inserting the first Polygon.
                        poTS->addGeometryDirectly( poFaceGeom );
                    }
                    else
                    {
                        // Using Union to add the current Polygon.
                        OGRGeometry *poUnion = poTS->Union( poFaceGeom );
                        delete poFaceGeom;
                        delete poTS;
                        if( poUnion == nullptr )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "Failed Union for TopoSurface" );
                            return nullptr;
                        }
                        if( wkbFlatten(poUnion->getGeometryType()) ==
                            wkbPolygon )
                        {
                            // Forcing to be a MultiPolygon.
                            poTS = new OGRMultiPolygon();
                            poTS->addGeometryDirectly(poUnion);
                        }
                        else if( wkbFlatten(poUnion->getGeometryType()) ==
                                 wkbMultiPolygon )
                        {
                            poTS = poUnion->toMultiPolygon();
                        }
                        else
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "Unexpected geometry type resulting "
                                      "from Union for TopoSurface" );
                            delete poUnion;
                            return nullptr;
                        }
                    }
                }
                delete poFaceCollectionGeom;
                delete poCollectedGeom;
              }
            }

            return poTS;
#endif // HAVE_GEOS
        }

        /****************************************************************/
        /* applying the FaceHoleNegative = true rules                   */
        /*                                                              */
        /* - each <TopoSurface> is expected to represent a MultiPolygon */
        /* - any <Face> declaring orientation="+" is expected to        */
        /*   represent an Exterior Ring (no holes are allowed)          */
        /* - any <Face> declaring orientation="-" is expected to        */
        /*   represent an Interior Ring (hole) belonging to the latest  */
        /*   Exterior Ring.                                             */
        /* - <Edges> within the same <Face> are expected to be          */
        /*   arranged in geometrically adjacent and consecutive         */
        /*   sequence.                                                  */
        /****************************************************************/
        if( bGetSecondaryGeometry )
            return nullptr;
        bool bFaceOrientation = true;
        OGRPolygon *poTS = new OGRPolygon();

        // Collect directed faces.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
          if( psChild->eType == CXT_Element
              && EQUAL(BareGMLElement(psChild->pszValue), "directedFace") )
          {
            bFaceOrientation = GetElementOrientation(psChild);

            // Collect next face (psChild->psChild).
            const CPLXMLNode *psFaceChild = GetChildElement(psChild);
            while( psFaceChild != nullptr &&
                   !EQUAL(BareGMLElement(psFaceChild->pszValue), "Face") )
                    psFaceChild = psFaceChild->psNext;

            if( psFaceChild == nullptr )
              continue;

            OGRLinearRing *poFaceGeom = new OGRLinearRing();

            // Collect directed edges of the face.
            for( const CPLXMLNode *psDirectedEdgeChild = psFaceChild->psChild;
                 psDirectedEdgeChild != nullptr;
                 psDirectedEdgeChild = psDirectedEdgeChild->psNext )
            {
              if( psDirectedEdgeChild->eType == CXT_Element &&
                  EQUAL(BareGMLElement(psDirectedEdgeChild->pszValue),
                        "directedEdge") )
              {
                OGRGeometry *poEdgeGeom =
                    GML2OGRGeometry_XMLNode_Internal(
                        psDirectedEdgeChild,
                        nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1,
                        nSRSDimension,
                        pszSRSName,
                        true,
                        bFaceOrientation );

                if( poEdgeGeom == nullptr ||
                    wkbFlatten(poEdgeGeom->getGeometryType()) != wkbLineString )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Failed to get geometry in directedEdge" );
                    delete poEdgeGeom;
                    delete poFaceGeom;
                    delete poTS;
                    return nullptr;
                }

                OGRLineString *poEdgeGeomLS = poEdgeGeom->toLineString();
                if( !bFaceOrientation )
                {
                    OGRLineString *poLS = poEdgeGeomLS;
                    OGRLineString *poAddLS = poFaceGeom;

                    // TODO(schwehr): Use AlmostEqual.
                    const double epsilon = 1.0e-14;
                    if( poAddLS->getNumPoints() < 2 )
                    {
                        // Skip it.
                    }
                    else if( poLS->getNumPoints() > 0
                             && fabs(poLS->getX(poLS->getNumPoints() - 1)
                                     - poAddLS->getX(0)) < epsilon
                             && fabs(poLS->getY(poLS->getNumPoints() - 1)
                                     - poAddLS->getY(0)) < epsilon
                             && fabs(poLS->getZ(poLS->getNumPoints() - 1)
                                     - poAddLS->getZ(0)) < epsilon )
                    {
                        // Skip the first point of the new linestring to avoid
                        // invalidate duplicate points.
                        poLS->addSubLineString( poAddLS, 1 );
                    }
                    else
                    {
                        // Add the whole new line string.
                        poLS->addSubLineString( poAddLS );
                    }
                    poFaceGeom->empty();
                }
                // TODO(schwehr): Suspicious that poLS overwritten without else.
                OGRLineString *poLS = poFaceGeom;
                OGRLineString *poAddLS = poEdgeGeomLS;
                if( poAddLS->getNumPoints() < 2 )
                {
                    // Skip it.
                }
                else if( poLS->getNumPoints() > 0
                    && fabs(poLS->getX(poLS->getNumPoints()-1)
                            - poAddLS->getX(0)) < 1e-14
                    && fabs(poLS->getY(poLS->getNumPoints()-1)
                            - poAddLS->getY(0)) < 1e-14
                    && fabs(poLS->getZ(poLS->getNumPoints()-1)
                            - poAddLS->getZ(0)) < 1e-14)
                {
                    // Skip the first point of the new linestring to avoid
                    // invalidate duplicate points.
                    poLS->addSubLineString( poAddLS, 1 );
                }
                else
                {
                    // Add the whole new line string.
                    poLS->addSubLineString( poAddLS );
                }
                delete poEdgeGeom;
              }
            }

            // if( poFaceGeom == NULL )
            // {
            //     CPLError( CE_Failure, CPLE_AppDefined,
            //               "Failed to get Face geometry in directedFace" );
            //     delete poFaceGeom;
            //     return NULL;
            // }

            poTS->addRingDirectly( poFaceGeom );
          }
        }

        // if( poTS == NULL )
        // {
        //     CPLError( CE_Failure, CPLE_AppDefined,
        //               "Failed to get TopoSurface geometry" );
        //     delete poTS;
        //     return NULL;
        // }

        return poTS;
    }

/* -------------------------------------------------------------------- */
/*      Surface                                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Surface") ||
        EQUAL(pszBaseGeometry, "ElevatedSurface") /* AIXM */ )
    {
        // Find outer ring.
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "patches" );
        if( psChild == nullptr )
            psChild = FindBareXMLChild( psNode, "polygonPatches" );
        if( psChild == nullptr )
            psChild = FindBareXMLChild( psNode, "trianglePatches" );

        psChild = GetChildElement(psChild);
        if( psChild == nullptr )
        {
            // <gml:Surface/> and <gml:Surface><gml:patches/></gml:Surface> are
            // valid GML.
            return new OGRPolygon();
        }

        OGRMultiSurface* poMS = nullptr;
        OGRGeometry* poResultPoly = nullptr;
        OGRGeometry* poResultTri = nullptr;
        OGRTriangulatedSurface *poTIN = nullptr;
        OGRGeometryCollection *poGC = nullptr;
        for( ; psChild != nullptr; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch") ||
                    EQUAL(BareGMLElement(psChild->pszValue), "Rectangle")))
            {
                OGRGeometry *poGeom =
                    GML2OGRGeometry_XMLNode_Internal(
                        psChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poGeom == nullptr )
                {
                    delete poResultPoly;
                    return nullptr;
                }

                const OGRwkbGeometryType eGeomType =
                    wkbFlatten(poGeom->getGeometryType());

                if( poResultPoly == nullptr )
                    poResultPoly = poGeom;
                else
                {
                    if( poMS == nullptr )
                    {
                        if( wkbFlatten(poResultPoly->getGeometryType()) ==
                            wkbPolygon &&
                            eGeomType == wkbPolygon )
                            poMS = new OGRMultiPolygon();
                        else
                            poMS = new OGRMultiSurface();
                        OGRErr eErr =
                          poMS->addGeometryDirectly( poResultPoly );
                        CPL_IGNORE_RET_VAL(eErr);
                        CPLAssert(eErr == OGRERR_NONE);
                        poResultPoly = poMS;
                    }
                    else if( eGeomType != wkbPolygon &&
                             wkbFlatten(poMS->getGeometryType()) ==
                             wkbMultiPolygon )
                    {
                        OGRMultiPolygon *poMultiPoly = poMS->toMultiPolygon();
                        poMS = OGRMultiPolygon::CastToMultiSurface(poMultiPoly);
                        poResultPoly = poMS;
                    }
                    OGRErr eErr =
                      poMS->addGeometryDirectly( poGeom );
                    CPL_IGNORE_RET_VAL(eErr);
                    CPLAssert(eErr == OGRERR_NONE);
                }
            }
            else if( psChild->eType == CXT_Element
                    && EQUAL(BareGMLElement(psChild->pszValue), "Triangle"))
            {
                OGRGeometry *poGeom =
                    GML2OGRGeometry_XMLNode_Internal(
                        psChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poGeom == nullptr )
                {
                    delete poResultTri;
                    return nullptr;
                }

                if( poResultTri == nullptr )
                    poResultTri = poGeom;
                else
                {
                    if( poTIN == nullptr )
                    {
                        poTIN = new OGRTriangulatedSurface();
                        OGRErr eErr =
                          poTIN->addGeometryDirectly( poResultTri );
                        CPL_IGNORE_RET_VAL(eErr);
                        CPLAssert(eErr == OGRERR_NONE);
                        poResultTri = poTIN;
                    }
                    OGRErr eErr =
                      poTIN->addGeometryDirectly( poGeom );
                    CPL_IGNORE_RET_VAL(eErr);
                    CPLAssert(eErr == OGRERR_NONE);
                }
            }
        }

        if( poResultTri == nullptr && poResultPoly == nullptr )
            return nullptr;

        if( poResultTri == nullptr )
            return poResultPoly;
        else if( poResultPoly == nullptr )
            return poResultTri;
        else
        {
            poGC = new OGRGeometryCollection();
            poGC->addGeometryDirectly(poResultTri);
            poGC->addGeometryDirectly(poResultPoly);
            return poGC;
        }
    }

/* -------------------------------------------------------------------- */
/*      TriangulatedSurface                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "TriangulatedSurface") ||
        EQUAL(pszBaseGeometry, "Tin") )
    {
        // Find trianglePatches.
        const CPLXMLNode *psChild =
            FindBareXMLChild( psNode, "trianglePatches" );
        if( psChild == nullptr )
            psChild = FindBareXMLChild( psNode, "patches" );

        psChild = GetChildElement(psChild);
        if( psChild == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <trianglePatches> for %s.", pszBaseGeometry );
            return nullptr;
        }

        OGRTriangulatedSurface *poTIN = new OGRTriangulatedSurface();
        for( ; psChild != nullptr; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "Triangle") )
            {
                OGRGeometry *poTriangle =
                    GML2OGRGeometry_XMLNode_Internal(
                        psChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poTriangle == nullptr )
                {
                    delete poTIN;
                    return nullptr;
                }
                else
                {
                    poTIN->addGeometryDirectly( poTriangle );
                }
            }
        }

        return poTIN;
    }

/* -------------------------------------------------------------------- */
/*      PolyhedralSurface                                               */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "PolyhedralSurface") )
    {
        // Find polygonPatches.
        const CPLXMLNode *psParent =
            FindBareXMLChild( psNode, "polygonPatches" );
        if( psParent == nullptr )
        {
            if( GetChildElement(psNode) == nullptr )
            {
                // This is empty PolyhedralSurface.
                return new OGRPolyhedralSurface();
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Missing <polygonPatches> for %s.", pszBaseGeometry );
                return nullptr;
            }
        }

        const CPLXMLNode *psChild = GetChildElement(psParent);
        if( psChild == nullptr )
        {
            // This is empty PolyhedralSurface.
            return new OGRPolyhedralSurface();
        }
        else if( !EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch") )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <PolygonPatch> for %s.", pszBaseGeometry );
            return nullptr;
        }

        // Each psParent has the tags corresponding to <gml:polygonPatches>
        // Each psChild has the tags corresponding to <gml:PolygonPatch>
        // Each PolygonPatch has a set of polygons enclosed in a
        // OGRPolyhedralSurface.
        OGRPolyhedralSurface *poPS = nullptr;
        OGRGeometryCollection *poGC = new OGRGeometryCollection();
        OGRGeometry *poResult = nullptr;
        for( ; psParent != nullptr; psParent = psParent->psNext )
        {
            psChild = GetChildElement(psParent);
            if( psChild == nullptr )
                continue;
            poPS = new OGRPolyhedralSurface();
            for( ; psChild != nullptr; psChild = psChild->psNext )
            {
                if( psChild->eType == CXT_Element
                    && EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch") )
                {
                    OGRGeometry *poPolygon =
                        GML2OGRGeometry_XMLNode_Internal(
                            psChild, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poPolygon == nullptr )
                    {
                        delete poPS;
                        delete poGC;
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "Wrong geometry type for %s.",
                                  pszBaseGeometry );
                        return nullptr;
                    }

                    else if( wkbFlatten(poPolygon->getGeometryType()) ==
                             wkbPolygon )
                    {
                        poPS->addGeometryDirectly( poPolygon );
                    }
                    else if( wkbFlatten(poPolygon->getGeometryType()) ==
                             wkbCurvePolygon )
                    {
                        poPS->addGeometryDirectly(
                            OGRGeometryFactory::forceToPolygon(poPolygon) );
                    }
                    else
                    {
                        delete poPS;
                        delete poGC;
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "Wrong geometry type for %s.",
                                  pszBaseGeometry );
                        return nullptr;
                    }
                }
            }
            poGC->addGeometryDirectly(poPS);
        }

        if( poGC->getNumGeometries() == 0 )
        {
            delete poGC;
            return nullptr;
        }
        else if( poPS != nullptr && poGC->getNumGeometries() == 1 )
        {
            poResult = poPS->clone();
            delete poGC;
            return poResult;
        }
        else
        {
            poResult = poGC;
            return poResult;
        }
    }

/* -------------------------------------------------------------------- */
/*      Solid                                                           */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Solid") )
    {
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "interior");
        if( psChild != nullptr )
        {
            static bool bWarnedOnce = false;
            if( !bWarnedOnce )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "<interior> elements of <Solid> are ignored");
                bWarnedOnce = true;
            }
        }

        // Find exterior element.
        psChild = FindBareXMLChild( psNode, "exterior");

        if( nSRSDimension == 0 )
            nSRSDimension = 3;

        psChild = GetChildElement(psChild);
        if( psChild == nullptr )
        {
            // <gml:Solid/> and <gml:Solid><gml:exterior/></gml:Solid> are valid
            // GML.
            return new OGRPolyhedralSurface();
        }

        if( EQUAL(BareGMLElement(psChild->pszValue), "CompositeSurface") )
        {
            OGRPolyhedralSurface* poPS = new OGRPolyhedralSurface();

            // Iterate over children.
            for( psChild = psChild->psChild;
                 psChild != nullptr;
                 psChild = psChild->psNext )
            {
                const char* pszMemberElement = BareGMLElement(psChild->pszValue);
                if( psChild->eType == CXT_Element
                    && (EQUAL(pszMemberElement, "polygonMember") ||
                        EQUAL(pszMemberElement, "surfaceMember")) )
                {
                    const CPLXMLNode* psSurfaceChild = GetChildElement(psChild);

                    if( psSurfaceChild != nullptr )
                    {
                        OGRGeometry* poGeom =
                            GML2OGRGeometry_XMLNode_Internal( psSurfaceChild,
                                nPseudoBoolGetSecondaryGeometryOption,
                                nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( poGeom != nullptr &&
                            wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
                        {
                            poPS->addGeometryDirectly(poGeom);
                        }
                        else
                        {
                            delete poGeom;
                        }
                    }
                }
            }
            return poPS;
        }

        // Get the geometry inside <exterior>.
        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
            psChild, nPseudoBoolGetSecondaryGeometryOption,
            nRecLevel + 1, nSRSDimension, pszSRSName );
        if( poGeom == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Invalid exterior element");
            delete poGeom;
            return nullptr;
        }

        return poGeom;
    }

/* -------------------------------------------------------------------- */
/*      OrientableSurface                                               */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "OrientableSurface") )
    {
        // Find baseSurface.
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "baseSurface" );

        psChild = GetChildElement(psChild);
        if( psChild == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <baseSurface> for OrientableSurface." );
            return nullptr;
        }

        return GML2OGRGeometry_XMLNode_Internal(
            psChild, nPseudoBoolGetSecondaryGeometryOption,
            nRecLevel + 1, nSRSDimension, pszSRSName );
    }

/* -------------------------------------------------------------------- */
/*      SimplePolygon, SimpleRectangle, SimpleTriangle                  */
/*      (GML 3.3 compact encoding)                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "SimplePolygon") ||
        EQUAL(pszBaseGeometry, "SimpleRectangle") )
    {
        OGRLinearRing *poRing = new OGRLinearRing();

        if( !ParseGMLCoordinates( psNode, poRing, nSRSDimension ) )
        {
            delete poRing;
            return nullptr;
        }

        poRing->closeRings();

        OGRPolygon* poPolygon = new OGRPolygon();
        poPolygon->addRingDirectly(poRing);
        return poPolygon;
    }

    if( EQUAL(pszBaseGeometry, "SimpleTriangle") )
    {
        OGRLinearRing *poRing = new OGRLinearRing();

        if( !ParseGMLCoordinates( psNode, poRing, nSRSDimension ) )
        {
            delete poRing;
            return nullptr;
        }

        poRing->closeRings();

        OGRTriangle* poTriangle = new OGRTriangle();
        poTriangle->addRingDirectly(poRing);
        return poTriangle;
    }

/* -------------------------------------------------------------------- */
/*      SimpleMultiPoint (GML 3.3 compact encoding)                     */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "SimpleMultiPoint") )
    {
        OGRLineString *poLS = new OGRLineString();

        if( !ParseGMLCoordinates( psNode, poLS, nSRSDimension ) )
        {
            delete poLS;
            return nullptr;
        }

        OGRMultiPoint* poMP = new OGRMultiPoint();
        int nPoints = poLS->getNumPoints();
        for( int i = 0; i < nPoints; i++ )
        {
            OGRPoint* poPoint = new OGRPoint();
            poLS->getPoint(i, poPoint);
            poMP->addGeometryDirectly(poPoint);
        }
        delete poLS;
        return poMP;
    }

    CPLError( CE_Failure, CPLE_AppDefined,
              "Unrecognized geometry type <%.500s>.",
              pszBaseGeometry );

    return nullptr;
}

/************************************************************************/
/*                      OGR_G_CreateFromGMLTree()                       */
/************************************************************************/

/** Create geometry from GML */
OGRGeometryH OGR_G_CreateFromGMLTree( const CPLXMLNode *psTree )

{
    return reinterpret_cast<OGRGeometryH>(GML2OGRGeometry_XMLNode(psTree, -1));
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
 * (OGR >= 1.8.0) The following GML3 elements are parsed : Surface,
 * MultiSurface, PolygonPatch, Triangle, Rectangle, Curve, MultiCurve,
 * CompositeCurve, LineStringSegment, Arc, Circle, CompositeSurface,
 * OrientableSurface, Solid, Tin, TriangulatedSurface.
 *
 * Arc and Circle elements are stroked to linestring, by using a
 * 4 degrees step, unless the user has overridden the value with the
 * OGR_ARC_STEPSIZE configuration variable.
 *
 * The C++ method OGRGeometryFactory::createFromGML() is the same as
 * this function.
 *
 * @param pszGML The GML fragment for the geometry.
 *
 * @return a geometry on success, or NULL on error.
 */

OGRGeometryH OGR_G_CreateFromGML( const char *pszGML )

{
    if( pszGML == nullptr || strlen(pszGML) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GML Geometry is empty in OGR_G_CreateFromGML()." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to parse the XML snippet using the MiniXML API.  If this    */
/*      fails, we assume the minixml api has already posted a CPL       */
/*      error, and just return NULL.                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGML = CPLParseXMLString( pszGML );

    if( psGML == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Convert geometry recursively.                                   */
/* -------------------------------------------------------------------- */
    // Must be in synced in OGR_G_CreateFromGML(), OGRGMLLayer::OGRGMLLayer()
    // and GMLReader::GMLReader().
    const bool bFaceHoleNegative =
         CPLTestBool(CPLGetConfigOption("GML_FACE_HOLE_NEGATIVE", "NO"));
    OGRGeometry *poGeometry =
        GML2OGRGeometry_XMLNode( psGML, -1, 0, 0,
                                 false, true, bFaceHoleNegative );

    CPLDestroyXMLNode( psGML );

    return reinterpret_cast<OGRGeometryH>(poGeometry);
}
