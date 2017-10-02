/******************************************************************************
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

#if HAVE_CXX11
constexpr double kdfD2R = M_PI / 180.0;
constexpr double kdf2PI = 2.0 * M_PI;
#else
static const double kdfD2R = M_PI / 180.0;
static const double kdf2PI = 2.0 * M_PI;
#endif

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
            *ppszNextToken = NULL;
            return NULL;
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
    const char *pszReturn = strchr( pszInput, ':' );
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

static bool GetElementOrientation( const CPLXMLNode *psElement )
{
    if( psElement == NULL )
        return true;

    const CPLXMLNode *psChild = psElement->psChild;

    while( psChild != NULL )
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
        OGRPoint *poPoint = dynamic_cast<OGRPoint *>(poGeometry);
        if( poPoint == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPoint.");
            return false;
        }

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
        OGRSimpleCurve *poCurve = dynamic_cast<OGRSimpleCurve *>(poGeometry);
        if( poCurve == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRSimpleCurve.");
            return false;
        }
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
    int iCoord = 0;

/* -------------------------------------------------------------------- */
/*      Handle <coordinates> case.                                      */
/*      Note that we don't do a strict validation, so we accept and     */
/*      sometimes generate output whereas we should just reject it.     */
/* -------------------------------------------------------------------- */
    if( psCoordinates != NULL )
    {
        const char *pszCoordString = GetElementText( psCoordinates );

        const char *pszDecimal =
            CPLGetXMLValue(const_cast<CPLXMLNode *>(psCoordinates),
                           "decimal", NULL);
        char chDecimal = '.';
        if( pszDecimal != NULL )
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
            CPLGetXMLValue(const_cast<CPLXMLNode *>(psCoordinates), "cs", NULL);
        char chCS = ',';
        if( pszCS != NULL )
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
            CPLGetXMLValue(const_cast<CPLXMLNode *>(psCoordinates), "ts", NULL);
        char chTS = ' ';
        if( pszTS != NULL )
        {
            if( strlen(pszTS) != 1 || (pszTS[0] >= '0' && pszTS[0] <= '9') )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for tes attribute");
                return false;
            }
            chTS = pszTS[0];
        }

        if( pszCoordString == NULL )
        {
            poGeometry->empty();
            return true;
        }

        while( *pszCoordString != '\0' )
        {
            double dfX = 0.0;
            int nDimension = 2;

            // parse out 2 or 3 tuple.
            if( chDecimal == '.' )
                dfX = OGRFastAtof( pszCoordString );
            else
                dfX = CPLAtofDelim( pszCoordString, chDecimal);
            while( *pszCoordString != '\0'
                   && *pszCoordString != chCS
                   && !isspace((unsigned char)*pszCoordString) )
                pszCoordString++;

            if( *pszCoordString == '\0' )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Corrupt <coordinates> value.");
                return false;
            }
            else if( chCS == ',' && pszCS == NULL &&
                     isspace((unsigned char)*pszCoordString) )
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

            pszCoordString++;

            double dfY = 0.0;
            if( chDecimal == '.' )
                dfY = OGRFastAtof( pszCoordString );
            else
                dfY = CPLAtofDelim( pszCoordString, chDecimal);
            while( *pszCoordString != '\0'
                   && *pszCoordString != chCS
                   && *pszCoordString != chTS
                   && !isspace(static_cast<unsigned char>(*pszCoordString)) )
                pszCoordString++;

            double dfZ = 0.0;
            if( *pszCoordString == chCS )
            {
                pszCoordString++;
                if( chDecimal == '.' )
                    dfZ = OGRFastAtof( pszCoordString );
                else
                    dfZ = CPLAtofDelim( pszCoordString, chDecimal);
                nDimension = 3;
                while( *pszCoordString != '\0'
                       && *pszCoordString != chCS
                       && *pszCoordString != chTS
                       && !isspace((unsigned char)*pszCoordString) )
                pszCoordString++;
            }

            if( *pszCoordString == chTS )
            {
                pszCoordString++;
            }

            while( isspace(static_cast<unsigned char>(*pszCoordString)) )
                pszCoordString++;

            if( !AddPoint( poGeometry, dfX, dfY, dfZ, nDimension ) )
                return false;

            iCoord++;
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
         psPos != NULL;
         psPos = psPos->psNext )
    {
        if( psPos->eType != CXT_Element )
            continue;

        const char* pszSubElement = BareGMLElement(psPos->pszValue);

        if( EQUAL(pszSubElement, "pointProperty") )
        {
            for( const CPLXMLNode *psPointPropertyIter = psPos->psChild;
                 psPointPropertyIter != NULL;
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
                psPos->psChild->psNext == NULL &&
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
        if( pszPos == NULL )
        {
            poGeometry->empty();
            return true;
        }

        const char* pszCur = pszPos;
        const char* pszX = GMLGetCoordTokenPos(pszCur, &pszCur);
        const char* pszY = (pszCur != NULL) ?
                            GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;
        const char* pszZ = (pszCur != NULL) ?
                            GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;

        if( pszY == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Did not get 2+ values in <gml:pos>%s</gml:pos> tuple.",
                      pszPos );
            return false;
        }

        const double dfX = OGRFastAtof(pszX);
        const double dfY = OGRFastAtof(pszY);
        const double dfZ = (pszZ != NULL) ? OGRFastAtof(pszZ) : 0.0;
        const bool bSuccess =
            AddPoint( poGeometry, dfX, dfY, dfZ, (pszZ != NULL) ? 3 : 2 );

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

    if( psPosList != NULL )
    {
        int nDimension = 2;

        // Try to detect the presence of an srsDimension attribute
        // This attribute is only available for gml3.1.1 but not
        // available for gml3.1 SF.
        const char* pszSRSDimension =
            CPLGetXMLValue(const_cast<CPLXMLNode *>(psPosList),
                           "srsDimension", NULL);
        // If not found at the posList level, try on the enclosing element.
        if( pszSRSDimension == NULL )
            pszSRSDimension =
                CPLGetXMLValue(const_cast<CPLXMLNode *>(psGeomNode),
                               "srsDimension", NULL);
        if( pszSRSDimension != NULL )
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
        if( pszPosList == NULL )
        {
            poGeometry->empty();
            return true;
        }

        bool bSuccess = false;
        const char* pszCur = pszPosList;
        while( true )
        {
            const char* pszX = GMLGetCoordTokenPos(pszCur, &pszCur);
            if( pszX == NULL && bSuccess )
                break;
            const char* pszY = (pszCur != NULL) ?
                    GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;
            const char* pszZ = (nDimension == 3 && pszCur != NULL) ?
                    GMLGetCoordTokenPos(pszCur, &pszCur) : NULL;

            if( pszY == NULL || (nDimension == 3 && pszZ == NULL) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Did not get at least %d values or invalid number of "
                        "set of coordinates <gml:posList>%s</gml:posList>",
                        nDimension, pszPosList);
                return false;
            }

            double dfX = OGRFastAtof(pszX);
            double dfY = OGRFastAtof(pszY);
            double dfZ = (pszZ != NULL) ? OGRFastAtof(pszZ) : 0.0;
            bSuccess = AddPoint( poGeometry, dfX, dfY, dfZ, nDimension );

            if( !bSuccess || pszCur == NULL )
                break;
        }

        return bSuccess;
    }

/* -------------------------------------------------------------------- */
/*      Handle form with a list of <coord> items each with an <X>,      */
/*      and <Y> element.                                                */
/* -------------------------------------------------------------------- */
    for( const CPLXMLNode *psCoordNode = psGeomNode->psChild;
         psCoordNode != NULL;
         psCoordNode = psCoordNode->psNext )
    {
        if( psCoordNode->eType != CXT_Element
            || !EQUAL(BareGMLElement(psCoordNode->pszValue), "coord") )
            continue;

        const CPLXMLNode *psXNode = FindBareXMLChild( psCoordNode, "X" );
        const CPLXMLNode *psYNode = FindBareXMLChild( psCoordNode, "Y" );
        const CPLXMLNode *psZNode = FindBareXMLChild( psCoordNode, "Z" );

        if( psXNode == NULL || psYNode == NULL
            || GetElementText(psXNode) == NULL
            || GetElementText(psYNode) == NULL
            || (psZNode != NULL && GetElementText(psZNode) == NULL) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Corrupt <coord> element, missing <X> or <Y> element?" );
            return false;
        }

        double dfX = OGRFastAtof( GetElementText(psXNode) );
        double dfY = OGRFastAtof( GetElementText(psYNode) );

        int nDimension = 2;
        double dfZ = 0.0;
        if( psZNode != NULL && GetElementText(psZNode) != NULL )
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
/*      Identifies the "good" Polygon whithin the collection returned   */
/*      by GEOSPolygonize()                                             */
/*      short rationale: GEOSPolygonize() will possibly return a        */
/*      collection of many Polygons; only one is the "good" one,        */
/*      (including both exterior- and interior-rings)                   */
/*      any other simply represents a single "hole", and should be      */
/*      consequently ignored at all.                                    */
/************************************************************************/

static OGRPolygon *GML2FaceExtRing( OGRGeometry *poGeom )
{
    OGRGeometryCollection *poColl =
        dynamic_cast<OGRGeometryCollection *>(poGeom);
    if( poColl == NULL )
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRGeometryCollection.");
        return NULL;
    }

    OGRPolygon *poPolygon = NULL;
    bool bError = false;
    int iCount = poColl->getNumGeometries();
    int iExterior = 0;
    int iInterior = 0;

    for( int ig = 0; ig < iCount; ig++)
    {
        // A collection of Polygons is expected to be found.
        OGRGeometry *poChild = poColl->getGeometryRef(ig);
        if( poChild == NULL)
        {
            bError = true;
            continue;
        }
        if( wkbFlatten( poChild->getGeometryType()) == wkbPolygon )
        {
            OGRPolygon *poPg = dynamic_cast<OGRPolygon *>(poChild);
            if( poPg == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRPolygon.");
                return NULL;
            }
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
            OGRPolygon *poPg =
                dynamic_cast<OGRPolygon *>(poColl->getGeometryRef(0));
            if( poPg == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRPolygon.");
                return NULL;
            }
            poPolygon = dynamic_cast<OGRPolygon *>(poPg->clone());
            if( poPolygon == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRPolygon.");
                return NULL;
            }
        }
        else
        {
            if( iExterior == 1 && iInterior == iCount - 1 )
            {
                // Searching the unique Polygon containing holes.
                for( int ig = 0; ig < iCount; ig++ )
                {
                    OGRPolygon *poPg =
                        dynamic_cast<OGRPolygon *>(poColl->getGeometryRef(ig));
                    if( poPg == NULL )
                    {
                        CPLError(CE_Fatal, CPLE_AppDefined,
                                 "dynamic_cast failed.  Expected OGRPolygon.");
                        return NULL;
                    }
                    if( poPg->getNumInteriorRings() > 0 )
                    {
                        poPolygon = dynamic_cast<OGRPolygon *>(poPg->clone());
                        if( poPolygon == NULL )
                        {
                            CPLError(
                                CE_Fatal, CPLE_AppDefined,
                                "dynamic_cast failed.  Expected OGRPolygon.");
                            return NULL;
                        }
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
    if( poGeom == NULL ||
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
        OGRCompoundCurve* poCCChild = dynamic_cast<OGRCompoundCurve *>(poGeom);
        if( poCCChild == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRCompoundCurve.");
            return false;
        }
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

        OGRCurve *poCurve = dynamic_cast<OGRCurve *>(poGeom);
        if( poCurve == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRCurve.");
            return false;
        }

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
    if( poGeom == NULL )
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
        OGRMultiSurface* poMS2 = dynamic_cast<OGRMultiSurface *>(poGeom);
        if( poMS2 == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRMultiSurface.");
            return false;
        }
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
        poGeom = NULL;
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
/*                      GML2OGRGeometry_XMLNode()                       */
/*                                                                      */
/*      Translates the passed XMLnode and it's children into an         */
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
            NULL,
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
    const bool bCastToLinearTypeIfPossible = true;  // Hard-coded for now.

    if( psNode != NULL && strcmp(psNode->pszValue, "?xml") == 0 )
        psNode = psNode->psNext;
    while( psNode != NULL && psNode->eType == CXT_Comment )
        psNode = psNode->psNext;
    if( psNode == NULL )
        return NULL;

    const char* pszSRSDimension =
        CPLGetXMLValue(const_cast<CPLXMLNode *>(psNode), "srsDimension", NULL);
    if( pszSRSDimension != NULL )
        nSRSDimension = atoi(pszSRSDimension);

    if( pszSRSName == NULL )
        pszSRSName =
            CPLGetXMLValue(const_cast<CPLXMLNode *>(psNode), "srsName", NULL);

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
        return NULL;
    }

    if( bGetSecondaryGeometry )
        if( !( EQUAL(pszBaseGeometry, "directedEdge") ||
               EQUAL(pszBaseGeometry, "TopoCurve") ) )
            return NULL;

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
        if( psChild == NULL )
           psChild = FindBareXMLChild( psNode, "exterior");

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
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
        if( poGeom == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            return NULL;
        }

        if( !OGR_GT_IsCurve(poGeom->getGeometryType()) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: Got %.500s geometry as outerBoundaryIs.",
                      pszBaseGeometry, poGeom->getGeometryName() );
            delete poGeom;
            return NULL;
        }

        if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
            !EQUAL(poGeom->getGeometryName(), "LINEARRING") )
        {
            OGRCurve *poCurve = dynamic_cast<OGRCurve *>(poGeom);
            if( poCurve == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRCurve.");
            }
            poGeom = OGRCurve::CastToLinearRing(poCurve);
        }

        OGRCurvePolygon *poCP = NULL;
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
            OGRCurve *poCurve = dynamic_cast<OGRCurve *>(poGeom);
            if( poCurve == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRCurve.");
            }
            if( poCP->addRingDirectly(poCurve) != OGRERR_NONE )
            {
                delete poCP;
                delete poGeom;
                return NULL;
            }
        }

        // Find all inner rings
        for( psChild = psNode->psChild;
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue),
                          "innerBoundaryIs") ||
                    EQUAL(BareGMLElement(psChild->pszValue), "interior")))
            {
                const CPLXMLNode* psInteriorChild = GetChildElement(psChild);
                if( psInteriorChild != NULL )
                    poGeom =
                        GML2OGRGeometry_XMLNode_Internal(
                            psInteriorChild,
                            nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1,
                            nSRSDimension,
                            pszSRSName );
                else
                    poGeom = NULL;
                if( poGeom == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Invalid interior ring");
                    delete poCP;
                    return NULL;
                }

                if( !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "%s: Got %.500s geometry as innerBoundaryIs.",
                            pszBaseGeometry, poGeom->getGeometryName() );
                    delete poCP;
                    delete poGeom;
                    return NULL;
                }

                if( bIsPolygon )
                {
                    if( !EQUAL(poGeom->getGeometryName(), "LINEARRING") )
                    {
                        if( wkbFlatten(poGeom->getGeometryType()) ==
                            wkbLineString )
                        {
                            OGRLineString* poLS =
                                dynamic_cast<OGRLineString *>(poGeom);
                            if( poLS == NULL )
                            {
                                CPLError(CE_Fatal, CPLE_AppDefined,
                                         "dynamic_cast failed.  "
                                         "Expected OGRLineString.");
                            }
                            poGeom = OGRCurve::CastToLinearRing(poLS);
                        }
                        else
                        {
                            OGRPolygon *poPolygon =
                                dynamic_cast<OGRPolygon *>(poCP);
                            if( poPolygon == NULL )
                            {
                                CPLError(CE_Fatal, CPLE_AppDefined,
                                         "dynamic_cast failed.  "
                                         "Expected OGRPolygon.");
                            }
                            // Might fail if some rings are not closed.
                            // We used to be tolerant about that with Polygon.
                            // but we have become stricter with CurvePolygon.
                            poCP = OGRSurface::CastToCurvePolygon(poPolygon);
                            if( poCP == NULL )
                            {
                                delete poGeom;
                                return NULL;
                            }
                            bIsPolygon = false;
                        }
                    }
                }
                else
                {
                    if( EQUAL(poGeom->getGeometryName(), "LINEARRING") )
                    {
                        OGRCurve *poCurve = dynamic_cast<OGRCurve*>(poGeom);
                        if( poCurve == NULL )
                        {
                            CPLError(CE_Fatal, CPLE_AppDefined,
                                     "dynamic_cast failed.  "
                                     "Expected OGRCurve.");
                        }
                        poGeom = OGRCurve::CastToLineString(poCurve);
                    }
                }
                OGRCurve *poCurve = dynamic_cast<OGRCurve*>(poGeom);
                if( poCurve == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                             "dynamic_cast failed.  "
                             "Expected OGRCurve.");
                }
                if( poCP->addRingDirectly(poCurve) != OGRERR_NONE )
                {
                    delete poCP;
                    delete poGeom;
                    return NULL;
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
        if( psChild == NULL )
           psChild = FindBareXMLChild( psNode, "exterior");

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
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
        if( poGeom == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            return NULL;
        }

        if( !OGR_GT_IsCurve(poGeom->getGeometryType()) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: Got %.500s geometry as outerBoundaryIs.",
                      pszBaseGeometry, poGeom->getGeometryName() );
            delete poGeom;
            return NULL;
        }

        if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
            !EQUAL(poGeom->getGeometryName(), "LINEARRING") )
        {
            poGeom = OGRCurve::CastToLinearRing((OGRCurve*)poGeom);
        }

        OGRTriangle *poTriangle;
        if( EQUAL(poGeom->getGeometryName(), "LINEARRING") )
        {
            poTriangle = new OGRTriangle();
        }
        else
        {
            delete poGeom;
            return NULL;
        }

        if( poTriangle->addRingDirectly( (OGRCurve*)poGeom ) != OGRERR_NONE )
        {
            delete poTriangle;
            delete poGeom;
            return NULL;
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
            return NULL;
        }

        return poLinearRing;
    }

/* -------------------------------------------------------------------- */
/*      Ring GML3                                                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszBaseGeometry, "Ring") )
    {
        OGRCurve* poRing = NULL;
        OGRCompoundCurve *poCC = NULL;
        bool bChildrenAreAllLineString = true;

        bool bLastCurveWasApproximateArc = false;
        bool bLastCurveWasApproximateArcInvertedAxisOrder = false;
        double dfLastCurveApproximateArcRadius = 0.0;

        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "curveMember") )
            {
                const CPLXMLNode* psCurveChild = GetChildElement(psChild);
                OGRGeometry* poGeom = NULL;
                if( psCurveChild != NULL )
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
                        psChild->psChild->psNext == NULL &&
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
                    return NULL;
                }

                // Try to join multiline string to one linestring.
                if( poGeom && wkbFlatten(poGeom->getGeometryType()) ==
                    wkbMultiLineString )
                {
                    poGeom =
                        OGRGeometryFactory::forceToLineString(poGeom, false);
                }

                if( poGeom == NULL
                    || !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                {
                    delete poGeom;
                    delete poRing;
                    delete poCC;
                    return NULL;
                }

                if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                    bChildrenAreAllLineString = false;

                // Ad-hoc logic to handle nicely connecting ArcByCenterPoint
                // with consecutive curves, as found in some AIXM files.
                bool bIsApproximateArc = false;
                const CPLXMLNode* psChild2, *psChild3;
                if( strcmp(psCurveChild->pszValue, "Curve") == 0 &&
                    (psChild2 = GetChildElement(psCurveChild)) != NULL &&
                    strcmp(psChild2->pszValue, "segments") == 0 &&
                    (psChild3 = GetChildElement(psChild2)) != NULL &&
                    strcmp(psChild3->pszValue, "ArcByCenterPoint") == 0 )
                {
                    const CPLXMLNode* psRadius =
                        FindBareXMLChild(psChild3, "radius");
                    if( psRadius && psRadius->eType == CXT_Element )
                    {
                        double dfRadius = CPLAtof(CPLGetXMLValue(
                            const_cast<CPLXMLNode *>(psRadius), NULL, "0"));
                        const char* pszUnits = CPLGetXMLValue(
                            const_cast<CPLXMLNode *>(psRadius), "uom", NULL);
                        bool bSRSUnitIsDegree = false;
                        bool bInvertedAxisOrder = false;
                        if( pszSRSName != NULL )
                        {
                            OGRSpatialReference oSRS;
                            if( oSRS.SetFromUserInput(pszSRSName)
                                == OGRERR_NONE )
                            {
                                if( oSRS.IsGeographic() )
                                {
                                    bInvertedAxisOrder =
                                        CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                                    bSRSUnitIsDegree =
                                        fabs(oSRS.GetAngularUnits(NULL) -
                                             CPLAtof(SRS_UA_DEGREE_CONV))
                                        < 1e-8;
                                }
                            }
                        }
                        if( bSRSUnitIsDegree && pszUnits != NULL &&
                            (EQUAL(pszUnits, "m") || EQUAL(pszUnits, "nm") ||
                             EQUAL(pszUnits, "mi") || EQUAL(pszUnits, "ft")) )
                        {
                            bIsApproximateArc = true;
                            if( EQUAL(pszUnits, "nm") )
                                dfRadius *= CPLAtof(SRS_UL_INTL_NAUT_MILE_CONV);
                            else if( EQUAL(pszUnits, "mi") )
                                dfRadius *= CPLAtof(SRS_UL_INTL_STAT_MILE_CONV);
                            else if( EQUAL(pszUnits, "ft") )
                                dfRadius *= CPLAtof(SRS_UL_INTL_FOOT_CONV);
                            dfLastCurveApproximateArcRadius = dfRadius;
                            bLastCurveWasApproximateArcInvertedAxisOrder =
                                bInvertedAxisOrder;
                        }
                    }
                }

                if( poCC == NULL && poRing == NULL )
                {
                    poRing = dynamic_cast<OGRCurve *>(poGeom);
                    if( poRing == NULL )
                    {
                        CPLError(CE_Fatal, CPLE_AppDefined,
                                 "dynamic_cast failed.  "
                                 "Expected OGRCurve.");
                    }
                }
                else
                {
                    if( poCC == NULL )
                    {
                        poCC = new OGRCompoundCurve();
                        bool bIgnored = false;
                        if( !GML2OGRGeometry_AddToCompositeCurve(poCC, poRing, bIgnored) )
                        {
                            delete poGeom;
                            delete poRing;
                            delete poCC;
                            return NULL;
                        }
                        poRing = NULL;
                    }

                    if( bIsApproximateArc )
                    {
                        if( poGeom->getGeometryType() == wkbLineString )
                        {
                            OGRCurve* poPreviousCurve =
                                poCC->getCurve(poCC->getNumCurves()-1);
                            OGRLineString* poLS =
                                dynamic_cast<OGRLineString *>(poGeom);
                            if( poLS == NULL )
                            {
                                CPLError(CE_Fatal, CPLE_AppDefined,
                                         "dynamic_cast failed.  "
                                         "Expected OGRLineString.");
                            }
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
                            OGRLineString* poLS =
                                dynamic_cast<OGRLineString *>(poPreviousCurve);
                            if( poLS == NULL )
                            {
                                CPLError(CE_Fatal, CPLE_AppDefined,
                                         "dynamic_cast failed.  "
                                         "Expected OGRLineString.");
                            }
                            OGRCurve *poCurve =
                                dynamic_cast<OGRCurve *>(poGeom);
                            if( poCurve == NULL )
                            {
                                CPLError(CE_Fatal, CPLE_AppDefined,
                                         "dynamic_cast failed.  "
                                         "Expected OGRCurve.");
                            }
                            if( poLS->getNumPoints() >= 2 &&
                                poCurve->getNumPoints() >= 2 )
                            {
                                OGRPoint p;
                                OGRPoint p2;
                                poCurve->StartPoint(&p);
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

                    OGRCurve *poCurve = dynamic_cast<OGRCurve *>(poGeom);
                    if( poCurve == NULL )
                    {
                        CPLError(CE_Fatal, CPLE_AppDefined,
                                 "dynamic_cast failed.  Expected OGRCurve.");
                    }

                    bool bIgnored = false;
                    if( !GML2OGRGeometry_AddToCompositeCurve( poCC,
                                                              poCurve,
                                                              bIgnored ) )
                    {
                        delete poGeom;
                        delete poCC;
                        delete poRing;
                        return NULL;
                    }
                }

                bLastCurveWasApproximateArc = bIsApproximateArc;
            }
        }

        if( poRing )
        {
            if( poRing->getNumPoints() < 2 || !poRing->get_IsClosed() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Non-closed ring");
                delete poRing;
                return NULL;
            }
            return poRing;
        }

        if( poCC == NULL )
            return NULL;

        else if( bCastToLinearTypeIfPossible && bChildrenAreAllLineString )
        {
            return OGRCurve::CastToLinearRing(poCC);
        }
        else
        {
            if( poCC->getNumPoints() < 2 || !poCC->get_IsClosed() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Non-closed ring");
                delete poCC;
                return NULL;
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
            return NULL;
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
            return NULL;
        }

        // Normally a gml:Arc has only 3 points of controls, but in the
        // wild we sometimes find GML with 5 points, so accept any odd
        // number >= 3 (ArcString should be used for > 3 points)
        if( poCC->getNumPoints() < 3 || (poCC->getNumPoints() % 2) != 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in Arc");
            delete poCC;
            return NULL;
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
            return NULL;
        }

        if( poCC->getNumPoints() < 3 || (poCC->getNumPoints() % 2) != 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in ArcString");
            delete poCC;
            return NULL;
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
            return NULL;
        }

        if( poLine->getNumPoints() != 3 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in Circle");
            delete poLine;
            return NULL;
        }

        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;
        if( !OGRGeometryFactory::GetCurveParmeters(
                               poLine->getX(0), poLine->getY(0),
                               poLine->getX(1), poLine->getY(1),
                               poLine->getX(2), poLine->getY(2),
                               R, cx, cy, alpha0, alpha1, alpha2 ) )
        {
            delete poLine;
            return NULL;
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
        if( psChild == NULL || psChild->eType != CXT_Element ||
            psChild->psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing bulge element." );
            return NULL;
        }
        const double dfBulge = CPLAtof(psChild->psChild->pszValue);

        psChild = FindBareXMLChild( psNode, "normal");
        if( psChild == NULL || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing normal element." );
            return NULL;
        }
        double dfNormal = CPLAtof(psChild->psChild->pszValue);

        OGRLineString* poLS = new OGRLineString();
        if( !ParseGMLCoordinates( psNode, poLS, nSRSDimension ) )
        {
            delete poLS;
            return NULL;
        }

        if( poLS->getNumPoints() != 2 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in ArcByBulge");
            delete poLS;
            return NULL;
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
        if( psChild == NULL || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing radius element." );
            return NULL;
        }
        const double dfRadius =
            CPLAtof(CPLGetXMLValue(const_cast<CPLXMLNode *>(psChild),
                                   NULL, "0"));
        const char* pszUnits =
            CPLGetXMLValue(const_cast<CPLXMLNode *>(psChild), "uom", NULL);

        psChild = FindBareXMLChild( psNode, "startAngle");
        if( psChild == NULL || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing startAngle element." );
            return NULL;
        }
        const double dfStartAngle =
            CPLAtof(CPLGetXMLValue(const_cast<CPLXMLNode *>(psChild),
                                   NULL, "0"));

        psChild = FindBareXMLChild( psNode, "endAngle");
        if( psChild == NULL || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing endAngle element." );
            return NULL;
        }
        const double dfEndAngle =
            CPLAtof(CPLGetXMLValue(const_cast<CPLXMLNode *>(psChild),
                                   NULL, "0"));

        OGRPoint p;
        if( !ParseGMLCoordinates( psNode, &p, nSRSDimension ) )
        {
            return NULL;
        }

        bool bSRSUnitIsDegree = false;
        bool bInvertedAxisOrder = false;
        if( pszSRSName != NULL )
        {
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput(pszSRSName) == OGRERR_NONE )
            {
                if( oSRS.IsGeographic() )
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                    bSRSUnitIsDegree = fabs(oSRS.GetAngularUnits(NULL) -
                                            CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
                }
            }
        }

        const double dfCenterX = p.getX();
        const double dfCenterY = p.getY();

        if( bSRSUnitIsDegree && pszUnits != NULL &&
            (EQUAL(pszUnits, "m") || EQUAL(pszUnits, "nm") ||
             EQUAL(pszUnits, "mi") || EQUAL(pszUnits, "ft")) )
        {
            OGRLineString* poLS = new OGRLineString();
            const double dfStep =
                CPLAtof(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
            double dfDistance = dfRadius;
            if( EQUAL(pszUnits, "nm") )
                dfDistance *= CPLAtof(SRS_UL_INTL_NAUT_MILE_CONV);
            else if( EQUAL(pszUnits, "mi") )
                dfDistance *= CPLAtof(SRS_UL_INTL_STAT_MILE_CONV);
            else if( EQUAL(pszUnits, "ft") )
                dfDistance *= CPLAtof(SRS_UL_INTL_FOOT_CONV);
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
                       // Not sure of angle conversion here.
                       90.0 - dfAngle,
                       &dfLat, &dfLong);
                    p.setY( dfLat );
                    p.setX( dfLong );
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
                                         // Not sure of angle conversion here.
                                         90.0 - dfEndAngle,
                                         &dfLat, &dfLong);
                p.setY( dfLat );
                p.setX( dfLong );
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
        if( psChild == NULL || psChild->eType != CXT_Element )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing radius element." );
            return NULL;
        }
        const double dfRadius =
            CPLAtof(CPLGetXMLValue(const_cast<CPLXMLNode *>(psChild),
                                   NULL, "0"));
        const char* pszUnits =
            CPLGetXMLValue(const_cast<CPLXMLNode *>(psChild), "uom", NULL);

        OGRPoint p;
        if( !ParseGMLCoordinates( psNode, &p, nSRSDimension ) )
        {
            return NULL;
        }

        bool bSRSUnitIsDegree = false;
        bool bInvertedAxisOrder = false;
        if( pszSRSName != NULL )
        {
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput(pszSRSName) == OGRERR_NONE )
            {
                if( oSRS.IsGeographic() )
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                    bSRSUnitIsDegree = fabs(oSRS.GetAngularUnits(NULL) -
                                            CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
                }
            }
        }

        const double dfCenterX = p.getX();
        const double dfCenterY = p.getY();

        if( bSRSUnitIsDegree && pszUnits != NULL &&
            (EQUAL(pszUnits, "m") || EQUAL(pszUnits, "nm") ||
             EQUAL(pszUnits, "mi") || EQUAL(pszUnits, "ft")) )
        {
            OGRLineString* poLS = new OGRLineString();
            const double dfStep =
                CPLAtof(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
            double dfDistance = dfRadius;
            if( EQUAL(pszUnits, "nm") )
                dfDistance *= CPLAtof(SRS_UL_INTL_NAUT_MILE_CONV);
            else if( EQUAL(pszUnits, "mi") )
                dfDistance *= CPLAtof(SRS_UL_INTL_STAT_MILE_CONV);
            else if( EQUAL(pszUnits, "ft") )
                dfDistance *= CPLAtof(SRS_UL_INTL_FOOT_CONV);
            for( double dfAngle = 0; dfAngle < 360; dfAngle += dfStep )
            {
                double dfLong = 0.0;
                double dfLat = 0.0;
                if( bInvertedAxisOrder )
                {
                    OGR_GreatCircle_ExtendPosition(dfCenterX, dfCenterY,
                                             dfDistance, dfAngle,
                                             &dfLat, &dfLong);
                    p.setY( dfLat );
                    p.setX( dfLong );
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
            return NULL;
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
    if( EQUAL(pszBaseGeometry, "Envelope") )
    {
        const CPLXMLNode* psLowerCorner =
            FindBareXMLChild( psNode, "lowerCorner");
        const CPLXMLNode* psUpperCorner =
            FindBareXMLChild( psNode, "upperCorner");
        if( psLowerCorner == NULL || psUpperCorner == NULL )
            return NULL;
        const char* pszLowerCorner = GetElementText(psLowerCorner);
        const char* pszUpperCorner = GetElementText(psUpperCorner);
        if( pszLowerCorner == NULL || pszUpperCorner == NULL )
            return NULL;
        char** papszLowerCorner = CSLTokenizeString(pszLowerCorner);
        char** papszUpperCorner = CSLTokenizeString(pszUpperCorner);
        const int nTokenCountLC = CSLCount(papszLowerCorner);
        const int nTokenCountUC = CSLCount(papszUpperCorner);
        if( nTokenCountLC < 2 || nTokenCountUC < 2 )
        {
            CSLDestroy(papszLowerCorner);
            CSLDestroy(papszUpperCorner);
            return NULL;
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
             psChild != NULL;
             psChild = psChild->psNext )
        {
            const char* pszMemberElement = BareGMLElement(psChild->pszValue);
            if( psChild->eType == CXT_Element
                && (EQUAL(pszMemberElement, "polygonMember") ||
                    EQUAL(pszMemberElement, "surfaceMember")) )
            {
                const CPLXMLNode* psSurfaceChild = GetChildElement(psChild);

                if( psSurfaceChild != NULL )
                {
                    // Cf #5421 where there are PolygonPatch with only inner
                    // rings.
                    const CPLXMLNode* psPolygonPatch =
                        GetChildElement(GetChildElement(psSurfaceChild));
                    const CPLXMLNode* psPolygonPatchChild = NULL;
                    if( psPolygonPatch != NULL &&
                        psPolygonPatch->eType == CXT_Element &&
                        EQUAL(BareGMLElement(psPolygonPatch->pszValue),
                              "PolygonPatch") &&
                        (psPolygonPatchChild = GetChildElement(psPolygonPatch))
                        != NULL &&
                        EQUAL(BareGMLElement(psPolygonPatchChild->pszValue),
                              "interior") )
                    {
                        // Find all inner rings
                        for( const CPLXMLNode* psChild2 =
                                 psPolygonPatch->psChild;
                             psChild2 != NULL;
                             psChild2 = psChild2->psNext )
                        {
                            if( psChild2->eType == CXT_Element
                                && (EQUAL(BareGMLElement(psChild2->pszValue),
                                          "interior")))
                            {
                                const CPLXMLNode* psInteriorChild =
                                    GetChildElement(psChild2);
                                OGRGeometry* poRing =
                                    psInteriorChild == NULL
                                    ? NULL
                                    : GML2OGRGeometry_XMLNode_Internal(
                                          psInteriorChild,
                                          nPseudoBoolGetSecondaryGeometryOption,
                                          nRecLevel + 1,
                                          nSRSDimension, pszSRSName );
                                if( poRing == NULL )
                                {
                                    CPLError( CE_Failure, CPLE_AppDefined,
                                              "Invalid interior ring");
                                    delete poMS;
                                    return NULL;
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
                                    return NULL;
                                }

                                bReconstructTopology = true;
                                OGRPolygon *poPolygon = new OGRPolygon();
                                OGRLinearRing *poLinearRing =
                                    dynamic_cast<OGRLinearRing *>(poRing);
                                if( poLinearRing == NULL )
                                {
                                    CPLError(CE_Fatal, CPLE_AppDefined,
                                             "dynamic_cast failed.  "
                                             "Expected OGRLinearRing.");
                                }
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
                            return NULL;
                        }
                    }
                }
            }
            else if( psChild->eType == CXT_Element
                     && EQUAL(pszMemberElement, "surfaceMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != NULL;
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
                            return NULL;
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
                : dynamic_cast<OGRMultiPolygon *>(poMS);
            CPLAssert(poMPoly);  // Should not fail.
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
            if( bCastToLinearTypeIfPossible &&
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
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "pointMember") )
            {
                const CPLXMLNode* psPointChild = GetChildElement(psChild);

                if( psPointChild != NULL )
                {
                    OGRGeometry *poPointMember =
                        GML2OGRGeometry_XMLNode_Internal( psPointChild,
                              nPseudoBoolGetSecondaryGeometryOption,
                              nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poPointMember == NULL ||
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
                        return NULL;
                    }

                    poMP->addGeometryDirectly( poPointMember );
                }
            }
            else if( psChild->eType == CXT_Element
                     && EQUAL(BareGMLElement(psChild->pszValue),
                              "pointMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != NULL;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element
                        && (EQUAL(BareGMLElement(psChild2->pszValue),
                                  "Point")) )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( poGeom == NULL )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined, "Invalid %s",
                                    BareGMLElement(psChild2->pszValue));
                            delete poMP;
                            return NULL;
                        }

                        if( wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
                        {
                            OGRPoint *poPoint =
                                dynamic_cast<OGRPoint *>(poGeom);
                            if( poPoint == NULL )
                            {
                                CPLError(CE_Fatal, CPLE_AppDefined,
                                         "dynamic_cast failed.  "
                                         "Expected OGRCPoint.");
                            }
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
    if( EQUAL(pszBaseGeometry, "MultiLineString") )
    {
        OGRMultiLineString *poMLS = new OGRMultiLineString();

        // Collect lines.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue),
                         "lineStringMember") )
            {
                const CPLXMLNode* psLineStringChild = GetChildElement(psChild);
                OGRGeometry *poGeom =
                    psLineStringChild == NULL
                    ? NULL
                    : GML2OGRGeometry_XMLNode_Internal(
                          psLineStringChild,
                          nPseudoBoolGetSecondaryGeometryOption,
                          nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poGeom == NULL
                    || wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MultiLineString: Got %.500s geometry as Member "
                             "instead of LINESTRING.",
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
    if( EQUAL(pszBaseGeometry, "MultiCurve") )
    {
        OGRMultiCurve *poMC = new OGRMultiCurve();
        bool bChildrenAreAllLineString = true;

        // Collect curveMembers.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "curveMember") )
            {
                const CPLXMLNode *psChild2 = GetChildElement(psChild);
                if( psChild2 != NULL )  // Empty curveMember is valid.
                {
                    OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psChild2, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poGeom == NULL ||
                        !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "MultiCurve: Got %.500s geometry as Member "
                                 "instead of a curve.",
                                 poGeom ? poGeom->getGeometryName() : "NULL" );
                        if( poGeom != NULL ) delete poGeom;
                        delete poMC;
                        return NULL;
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
                     psChild2 != NULL;
                     psChild2 = psChild2->psNext )
                {
                    if( psChild2->eType == CXT_Element )
                    {
                        OGRGeometry* poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( poGeom == NULL ||
                            !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "MultiCurve: Got %.500s geometry as "
                                "Member instead of a curve.",
                                poGeom ? poGeom->getGeometryName() : "NULL" );
                            if( poGeom != NULL ) delete poGeom;
                            delete poMC;
                            return NULL;
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

        if( bCastToLinearTypeIfPossible && bChildrenAreAllLineString )
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
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "curveMember") )
            {
                const CPLXMLNode *psChild2 = GetChildElement(psChild);
                if( psChild2 != NULL )  // Empty curveMember is valid.
                {
                    OGRGeometry*poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psChild2, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( !GML2OGRGeometry_AddToCompositeCurve(
                             poCC, poGeom, bChildrenAreAllLineString) )
                    {
                        delete poGeom;
                        delete poCC;
                        return NULL;
                    }
                }
            }
            else if( psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue), "curveMembers") )
            {
                for( const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != NULL;
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
                            return NULL;
                        }
                    }
                }
            }
        }

        if( bCastToLinearTypeIfPossible && bChildrenAreAllLineString )
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
        if( psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GML3 Curve geometry lacks segments element." );
            return NULL;
        }

        OGRGeometry *poGeom =
            GML2OGRGeometry_XMLNode_Internal(
                psChild, nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension, pszSRSName );
        if( poGeom == NULL ||
            !OGR_GT_IsCurve(poGeom->getGeometryType()) )
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
    if( EQUAL(pszBaseGeometry, "segments") )
    {
        OGRCurve* poCurve = NULL;
        OGRCompoundCurve *poCC = NULL;
        bool bChildrenAreAllLineString = true;

        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != NULL;
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
                if( poGeom == NULL ||
                    !OGR_GT_IsCurve(poGeom->getGeometryType()) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "segments: Got %.500s geometry as Member "
                             "instead of curve.",
                             poGeom ? poGeom->getGeometryName() : "NULL");
                    delete poGeom;
                    delete poCurve;
                    delete poCC;
                    return NULL;
                }

                if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
                    bChildrenAreAllLineString = false;

                if( poCC == NULL && poCurve == NULL )
                {
                    poCurve = dynamic_cast<OGRCurve *>(poGeom);
                    if( poCurve == NULL )
                    {
                        CPLError(CE_Fatal, CPLE_AppDefined,
                                 "dynamic_cast failed.  Expected OGRCurve.");
                    }

                }
                else
                {
                    if( poCC == NULL )
                    {
                        poCC = new OGRCompoundCurve();
                        if( poCC->addCurveDirectly(poCurve) != OGRERR_NONE )
                        {
                            delete poGeom;
                            delete poCurve;
                            delete poCC;
                            return NULL;
                        }
                        poCurve = NULL;
                    }

                    OGRCurve *poAsCurve = dynamic_cast<OGRCurve *>(poGeom);
                    if( poAsCurve == NULL )
                    {
                        CPLError(CE_Fatal, CPLE_AppDefined,
                                 "dynamic_cast failed.  Expected OGRCurve.");
                    }

                    if( poCC->addCurveDirectly(poAsCurve) != OGRERR_NONE )
                    {
                        delete poGeom;
                        delete poCC;
                        return NULL;
                    }
                }
            }
        }

        if( poCurve != NULL )
            return poCurve;
        if( poCC == NULL )
            return NULL;

        if( bCastToLinearTypeIfPossible && bChildrenAreAllLineString )
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
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "geometryMember") )
            {
                const CPLXMLNode* psGeometryChild = GetChildElement(psChild);

                if( psGeometryChild != NULL )
                {
                    OGRGeometry *poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psGeometryChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poGeom == NULL )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "GeometryCollection: Failed to get geometry "
                                  "in geometryMember" );
                        delete poGeom;
                        delete poGC;
                        return NULL;
                    }

                    poGC->addGeometryDirectly( poGeom );
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
        if( psEdge == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to get Edge element in directedEdge" );
            return NULL;
        }

        // TODO(schwehr): Localize vars after removing gotos.
        OGRGeometry *poGeom = NULL;
        const CPLXMLNode *psNodeElement = NULL;
        const CPLXMLNode *psPointProperty = NULL;
        const CPLXMLNode *psPoint = NULL;
        bool bNodeOrientation = true;
        OGRPoint *poPositiveNode = NULL;
        OGRPoint *poNegativeNode = NULL;

        const bool bEdgeOrientation = GetElementOrientation(psNode);

        if( bGetSecondaryGeometry )
        {
            const CPLXMLNode *psdirectedNode =
                FindBareXMLChild(psEdge, "directedNode");
            if( psdirectedNode == NULL ) goto nonode;

            bNodeOrientation = GetElementOrientation( psdirectedNode );

            psNodeElement = FindBareXMLChild(psdirectedNode, "Node");
            if( psNodeElement == NULL ) goto nonode;

            psPointProperty = FindBareXMLChild(psNodeElement, "pointProperty");
            if( psPointProperty == NULL )
                psPointProperty = FindBareXMLChild(psNodeElement,
                                                   "connectionPointProperty");
            if( psPointProperty == NULL ) goto nonode;

            psPoint = FindBareXMLChild(psPointProperty, "Point");
            if( psPoint == NULL )
                psPoint = FindBareXMLChild(psPointProperty, "ConnectionPoint");
            if( psPoint == NULL ) goto nonode;

            poGeom = GML2OGRGeometry_XMLNode_Internal(
                psPoint, nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension, pszSRSName, true );
            if( poGeom == NULL
                || wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
            {
                // CPLError( CE_Failure, CPLE_AppDefined,
                //           "Got %.500s geometry as Member instead of POINT.",
                //           poGeom ? poGeom->getGeometryName() : "NULL" );
                if( poGeom != NULL) delete poGeom;
                goto nonode;
            }

            {
                OGRPoint *poPoint = dynamic_cast<OGRPoint *>(poGeom);
                if( poPoint == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                             "dynamic_cast failed.  Expected OGRPoint.");
                }
                if( ( bNodeOrientation == bEdgeOrientation ) != bOrientation )
                    poPositiveNode = poPoint;
                else
                    poNegativeNode = poPoint;
            }

            // Look for the other node.
            psdirectedNode = psdirectedNode->psNext;
            while( psdirectedNode != NULL &&
                   !EQUAL( psdirectedNode->pszValue, "directedNode" ) )
                psdirectedNode = psdirectedNode->psNext;
            if( psdirectedNode == NULL ) goto nonode;

            if( GetElementOrientation( psdirectedNode ) == bNodeOrientation )
                goto nonode;

            psNodeElement = FindBareXMLChild(psEdge, "Node");
            if( psNodeElement == NULL ) goto nonode;

            psPointProperty = FindBareXMLChild(psNodeElement, "pointProperty");
            if( psPointProperty == NULL )
                psPointProperty =
                    FindBareXMLChild(psNodeElement, "connectionPointProperty");
            if( psPointProperty == NULL ) goto nonode;

            psPoint = FindBareXMLChild(psPointProperty, "Point");
            if( psPoint == NULL )
                psPoint = FindBareXMLChild(psPointProperty, "ConnectionPoint");
            if( psPoint == NULL ) goto nonode;

            poGeom = GML2OGRGeometry_XMLNode_Internal(
                psPoint, nPseudoBoolGetSecondaryGeometryOption,
                nRecLevel + 1, nSRSDimension, pszSRSName, true );
            if( poGeom == NULL
                || wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
            {
                // CPLError( CE_Failure, CPLE_AppDefined,
                //           "Got %.500s geometry as Member instead of POINT.",
                //           poGeom ? poGeom->getGeometryName() : "NULL" );
                if( poGeom != NULL) delete poGeom;
                goto nonode;
            }

            {
                OGRPoint *poPoint = dynamic_cast<OGRPoint *>(poGeom);
                if( poPoint == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                             "dynamic_cast failed.  Expected OGRPoint.");
                }
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
        if( psCurveProperty == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "directedEdge: Failed to get curveProperty in Edge" );
            return NULL;
        }

        const CPLXMLNode *psCurve =
            FindBareXMLChild(psCurveProperty, "LineString");
        if( psCurve == NULL )
            psCurve = FindBareXMLChild(psCurveProperty, "Curve");
        if( psCurve == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "directedEdge: Failed to get LineString or "
                     "Curve tag in curveProperty");
            return NULL;
        }

        OGRGeometry* poLineStringBeforeCast = GML2OGRGeometry_XMLNode_Internal(
            psCurve, nPseudoBoolGetSecondaryGeometryOption,
            nRecLevel + 1, nSRSDimension, pszSRSName, true );
        if( poLineStringBeforeCast == NULL
            || wkbFlatten(poLineStringBeforeCast->getGeometryType()) !=
            wkbLineString )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Got %.500s geometry as Member instead of LINESTRING.",
                     poLineStringBeforeCast
                     ? poLineStringBeforeCast->getGeometryName()
                     : "NULL" );
            delete poLineStringBeforeCast;
            return NULL;
        }
        OGRLineString *poLineString =
            dynamic_cast<OGRLineString *>(poLineStringBeforeCast);
        if( poLineString == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRLineString.");
        }

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
        OGRMultiLineString *poMLS = NULL;
        OGRMultiPoint *poMP = NULL;

        if( bGetSecondaryGeometry )
            poMP = new OGRMultiPoint();
        else
            poMLS = new OGRMultiLineString();

        // Collect directedEdges.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != NULL;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "directedEdge"))
            {
                OGRGeometry *poGeom = GML2OGRGeometry_XMLNode_Internal(
                    psChild, nPseudoBoolGetSecondaryGeometryOption,
                    nRecLevel + 1, nSRSDimension, pszSRSName );
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

                // Add the two points corresponding to the two nodes to poMP.
                if( bGetSecondaryGeometry &&
                     wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
                {
                    OGRMultiPoint *poMultiPoint =
                        dynamic_cast<OGRMultiPoint *>(poGeom);
                    if( poMultiPoint == NULL )
                    {
                        CPLError(CE_Fatal, CPLE_AppDefined,
                                 "dynamic_cast failed.  "
                                 "Expected OGRMultiPoint.");
                    }
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
                return NULL;

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
            return NULL;
#else
            OGRMultiPolygon *poTS = new OGRMultiPolygon();

            // Collect directed faces.
            for( const CPLXMLNode *psChild = psNode->psChild;
                 psChild != NULL;
                 psChild = psChild->psNext )
            {
              if( psChild->eType == CXT_Element
              && EQUAL(BareGMLElement(psChild->pszValue), "directedFace") )
              {
                // Collect next face (psChild->psChild).
                const CPLXMLNode *psFaceChild = GetChildElement(psChild);

                while( psFaceChild != NULL &&
                       !(psFaceChild->eType == CXT_Element &&
                         EQUAL(BareGMLElement(psFaceChild->pszValue), "Face")) )
                        psFaceChild = psFaceChild->psNext;

                if( psFaceChild == NULL )
                    continue;

                OGRMultiLineString *poCollectedGeom = new OGRMultiLineString();

                // Collect directed edges of the face.
                for( const CPLXMLNode *psDirectedEdgeChild =
                         psFaceChild->psChild;
                     psDirectedEdgeChild != NULL;
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

                      if( poEdgeGeom == NULL ||
                          wkbFlatten(poEdgeGeom->getGeometryType()) !=
                          wkbLineString )
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

                OGRGeometry *poFaceCollectionGeom =
                    poCollectedGeom->Polygonize();
                if( poFaceCollectionGeom == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Failed to assemble Edges in Face" );
                    delete poCollectedGeom;
                    delete poTS;
                    return NULL;
                }

                OGRPolygon *poFaceGeom = GML2FaceExtRing(poFaceCollectionGeom);

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
                        // Inserting the first Polygon.
                        poTS->addGeometryDirectly( poFaceGeom );
                    }
                    else
                    {
                        // Using Union to add the current Polygon.
                        OGRGeometry *poUnion = poTS->Union( poFaceGeom );
                        delete poFaceGeom;
                        delete poTS;
                        if( poUnion == NULL )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "Failed Union for TopoSurface" );
                            return NULL;
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
                            poTS = dynamic_cast<OGRMultiPolygon *>(poUnion);
                            if( poTS == NULL )
                            {
                                CPLError(CE_Fatal, CPLE_AppDefined,
                                         "dynamic_cast failed.  "
                                         "Expected OGRMultiPolygon.");
                            }
                        }
                        else
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "Unexpected geometry type resulting "
                                      "from Union for TopoSurface" );
                            delete poUnion;
                            return NULL;
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
            return NULL;
        bool bFaceOrientation = true;
        OGRPolygon *poTS = new OGRPolygon();

        // Collect directed faces.
        for( const CPLXMLNode *psChild = psNode->psChild;
             psChild != NULL;
             psChild = psChild->psNext )
        {
          if( psChild->eType == CXT_Element
              && EQUAL(BareGMLElement(psChild->pszValue), "directedFace") )
          {
            bFaceOrientation = GetElementOrientation(psChild);

            // Collect next face (psChild->psChild).
            const CPLXMLNode *psFaceChild = GetChildElement(psChild);
            while( psFaceChild != NULL &&
                   !EQUAL(BareGMLElement(psFaceChild->pszValue), "Face") )
                    psFaceChild = psFaceChild->psNext;

            if( psFaceChild == NULL )
              continue;

            OGRLinearRing *poFaceGeom = new OGRLinearRing();

            // Collect directed edges of the face.
            for( const CPLXMLNode *psDirectedEdgeChild = psFaceChild->psChild;
                 psDirectedEdgeChild != NULL;
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

                OGRLineString *poEdgeGeomLS =
                    dynamic_cast<OGRLineString *>(poEdgeGeom);
                if( poEdgeGeomLS == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                                "dynamic_cast failed.  "
                                "Expected OGRLineString.");
                    delete poEdgeGeom;
                    delete poFaceGeom;
                    delete poTS;
                    return NULL;
                }

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
    if( EQUAL(pszBaseGeometry, "Surface") )
    {
        // Find outer ring.
        const CPLXMLNode *psChild = FindBareXMLChild( psNode, "patches" );
        if( psChild == NULL )
            psChild = FindBareXMLChild( psNode, "polygonPatches" );
        if( psChild == NULL )
            psChild = FindBareXMLChild( psNode, "trianglePatches" );

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
        {
            // <gml:Surface/> and <gml:Surface><gml:patches/></gml:Surface> are
            // valid GML.
            return new OGRPolygon();
        }

        OGRMultiSurface* poMS = NULL;
        OGRGeometry* poResultPoly = NULL;
        OGRGeometry* poResultTri = NULL;
        OGRTriangulatedSurface *poTIN = NULL;
        OGRGeometryCollection *poGC = NULL;
        for( ; psChild != NULL; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && (EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch") ||
                    EQUAL(BareGMLElement(psChild->pszValue), "Rectangle")))
            {
                OGRGeometry *poGeom =
                    GML2OGRGeometry_XMLNode_Internal(
                        psChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poGeom == NULL )
                {
                    delete poResultPoly;
                    return NULL;
                }

                const OGRwkbGeometryType eGeomType =
                    wkbFlatten(poGeom->getGeometryType());

                if( poResultPoly == NULL )
                    poResultPoly = poGeom;
                else
                {
                    if( poMS == NULL )
                    {
                        if( wkbFlatten(poResultPoly->getGeometryType()) ==
                            wkbPolygon &&
                            eGeomType == wkbPolygon )
                            poMS = new OGRMultiPolygon();
                        else
                            poMS = new OGRMultiSurface();
#ifdef DEBUG
                        OGRErr eErr =
#endif
                          poMS->addGeometryDirectly( poResultPoly );
                        CPLAssert(eErr == OGRERR_NONE);
                        poResultPoly = poMS;
                    }
                    else if( eGeomType != wkbPolygon &&
                             wkbFlatten(poMS->getGeometryType()) ==
                             wkbMultiPolygon )
                    {
                        OGRMultiPolygon *poMultiPoly =
                            dynamic_cast<OGRMultiPolygon *>(poMS);
                        if( poMultiPoly == NULL )
                        {
                            CPLError(CE_Fatal, CPLE_AppDefined,
                                     "dynamic_cast failed.  "
                                     "Expected OGRMultiPolygon.");
                        }
                        poMS = OGRMultiPolygon::CastToMultiSurface(poMultiPoly);
                        poResultPoly = poMS;
                    }
#ifdef DEBUG
                    OGRErr eErr =
#endif
                      poMS->addGeometryDirectly( poGeom );
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
                if( poGeom == NULL )
                {
                    delete poResultTri;
                    return NULL;
                }

                if( poResultTri == NULL )
                    poResultTri = poGeom;
                else
                {
                    if( poTIN == NULL )
                    {
                        poTIN = new OGRTriangulatedSurface();
#ifdef DEBUG
                        OGRErr eErr =
#endif
                          poTIN->addGeometryDirectly( poResultTri );
                        CPLAssert(eErr == OGRERR_NONE);
                        poResultTri = poTIN;
                    }
#ifdef DEBUG
                    OGRErr eErr =
#endif
                      poTIN->addGeometryDirectly( poGeom );
                    CPLAssert(eErr == OGRERR_NONE);
                }
            }
        }

        if( poResultTri == NULL && poResultPoly == NULL )
            return NULL;

        if( poResultTri == NULL )
            return poResultPoly;
        else if( poResultPoly == NULL )
            return poResultTri;
        else if( poResultTri != NULL && poResultPoly != NULL )
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
        if( psChild == NULL )
            psChild = FindBareXMLChild( psNode, "patches" );

        psChild = GetChildElement(psChild);
        if( psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <trianglePatches> for %s.", pszBaseGeometry );
            return NULL;
        }

        OGRTriangulatedSurface *poTIN = new OGRTriangulatedSurface();
        for( ; psChild != NULL; psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Element
                && EQUAL(BareGMLElement(psChild->pszValue), "Triangle") )
            {
                OGRGeometry *poTriangle =
                    GML2OGRGeometry_XMLNode_Internal(
                        psChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName );
                if( poTriangle == NULL )
                {
                    delete poTIN;
                    return NULL;
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
        if( psParent == NULL )
        {
            if( GetChildElement(psNode) == NULL )
            {
                // This is empty PolyhedralSurface.
                return new OGRPolyhedralSurface();
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Missing <polygonPatches> for %s.", pszBaseGeometry );
                return NULL;
            }
        }

        const CPLXMLNode *psChild = GetChildElement(psParent);
        if( psChild == NULL )
        {
            // This is empty PolyhedralSurface.
            return new OGRPolyhedralSurface();
        }
        else if( psChild != NULL &&
                 !EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch") )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <PolygonPatch> for %s.", pszBaseGeometry );
            return NULL;
        }

        // Each psParent has the tags corresponding to <gml:polygonPatches>
        // Each psChild has the tags corresponding to <gml:PolygonPatch>
        // Each PolygonPatch has a set of polygons enclosed in a
        // OGRPolyhedralSurface.
        OGRPolyhedralSurface *poPS = NULL;
        OGRGeometryCollection *poGC = new OGRGeometryCollection();
        OGRGeometry *poResult = NULL;
        for( ; psParent != NULL; psParent = psParent->psNext )
        {
            poPS = new OGRPolyhedralSurface();
            for( ; psChild != NULL; psChild = psChild->psNext )
            {
                if( psChild->eType == CXT_Element
                    && EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch") )
                {
                    OGRGeometry *poPolygon =
                        GML2OGRGeometry_XMLNode_Internal(
                            psChild, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName );
                    if( poPolygon == NULL )
                    {
                        delete poPS;
                        delete poGC;
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "Wrong geometry type for %s.",
                                  pszBaseGeometry );
                        return NULL;
                    }

                    else if( wkbFlatten(poPolygon->getGeometryType()) ==
                             wkbPolygon )
                    {
                        poPS->addGeometryDirectly( poPolygon );
                    }
                    else
                    {
                        delete poPS;
                        delete poGC;
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "Wrong geometry type for %s.",
                                  pszBaseGeometry );
                        return NULL;
                    }
                }
            }
            poGC->addGeometryDirectly(poPS);
        }

        if( poGC->getNumGeometries() == 0 )
        {
            delete poGC;
            return NULL;
        }
        else if( poPS != NULL && poGC->getNumGeometries() == 1 )
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
        if( psChild != NULL )
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
        if( psChild == NULL )
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
                 psChild != NULL;
                 psChild = psChild->psNext )
            {
                const char* pszMemberElement = BareGMLElement(psChild->pszValue);
                if( psChild->eType == CXT_Element
                    && (EQUAL(pszMemberElement, "polygonMember") ||
                        EQUAL(pszMemberElement, "surfaceMember")) )
                {
                    const CPLXMLNode* psSurfaceChild = GetChildElement(psChild);

                    if( psSurfaceChild != NULL )
                    {
                        OGRGeometry* poGeom =
                            GML2OGRGeometry_XMLNode_Internal( psSurfaceChild,
                                nPseudoBoolGetSecondaryGeometryOption,
                                nRecLevel + 1, nSRSDimension, pszSRSName );
                        if( poGeom != NULL &&
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
        if( poGeom == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Invalid exterior element");
            delete poGeom;
            return NULL;
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
        if( psChild == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing <baseSurface> for OrientableSurface." );
            return NULL;
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
            return NULL;
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
            return NULL;
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
            return NULL;
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

    return NULL;
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
    if( pszGML == NULL || strlen(pszGML) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GML Geometry is empty in OGR_G_CreateFromGML()." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
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
