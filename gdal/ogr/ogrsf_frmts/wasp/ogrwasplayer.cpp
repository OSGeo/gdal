/******************************************************************************
 *
 * Project:  WAsP Translator
 * Purpose:  Implements OGRWAsPLayer class.
 * Author:   Vincent Mora, vincent dot mora at oslandia dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Oslandia <info at oslandia dot com>
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
 ****************************************************************************/

#include "ogrwasp.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogrsf_frmts.h"

#include <cassert>
#include <map>
#include <sstream>

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRWAsPLayer()                             */
/************************************************************************/

OGRWAsPLayer::OGRWAsPLayer( const char * pszName,
                            VSILFILE * hFileHandle,
                            OGRSpatialReference * poSpatialRef ) :
    bMerge(false),
    iFeatureCount(0),
    sName(pszName),
    hFile(hFileHandle),
    iFirstFieldIdx(0),
    iSecondFieldIdx(1),
    iGeomFieldIdx(0),
    poLayerDefn(new OGRFeatureDefn(pszName)),
    poSpatialReference(poSpatialRef),
    iOffsetFeatureBegin(VSIFTellL(hFile)),
    eMode(READ_ONLY)
{
    SetDescription( poLayerDefn->GetName() );
    poLayerDefn->Reference();
    poLayerDefn->GetGeomFieldDefn(0)->SetType( wkbLineString25D );
    poLayerDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSpatialReference );
    if( poSpatialReference ) poSpatialReference->Reference();
}

OGRWAsPLayer::OGRWAsPLayer( const char * pszName,
                            VSILFILE * hFileHandle,
                            OGRSpatialReference * poSpatialRef,
                            const CPLString & sFirstFieldParam,
                            const CPLString & sSecondFieldParam,
                            const CPLString & sGeomFieldParam,
                            bool bMergeParam,
                            double * pdfToleranceParam,
                            double * pdfAdjacentPointToleranceParam,
                            double * pdfPointToCircleRadiusParam ) :
    bMerge(bMergeParam),
    iFeatureCount(0),
    sName(pszName),
    hFile(hFileHandle),
    sFirstField(sFirstFieldParam),
    sSecondField(sSecondFieldParam),
    sGeomField(sGeomFieldParam),
    iFirstFieldIdx(-1),
    iSecondFieldIdx(-1),
    iGeomFieldIdx(sGeomFieldParam.empty() ? 0 : -1),
    poLayerDefn(new OGRFeatureDefn(pszName)),
    poSpatialReference(poSpatialRef),
    iOffsetFeatureBegin(VSIFTellL(hFile)),  // Avoids coverity warning.
    eMode(WRITE_ONLY),
    pdfTolerance(pdfToleranceParam),
    pdfAdjacentPointTolerance(pdfAdjacentPointToleranceParam),
    pdfPointToCircleRadius(pdfPointToCircleRadiusParam)
{
    SetDescription( poLayerDefn->GetName() );
    poLayerDefn->Reference();
    poLayerDefn->GetGeomFieldDefn(0)->SetType( wkbLineString25D );
    poLayerDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSpatialReference );
    if (poSpatialReference) poSpatialReference->Reference();
}

/************************************************************************/
/*                            ~OGRWAsPLayer()                            */
/************************************************************************/

OGRWAsPLayer::~OGRWAsPLayer()

{
    if ( bMerge )
    {
        /* If polygon where used, we have to merge lines before output */
        /* lines must be merged if they have the same left/right values */
        /* and touch at end points. */
        /* Those lines appear when polygon with the same roughness touch */
        /* since the boundary between them is not wanted */
        /* We do it here since we are sure we have all polygons */
        /* We first detect touching lines, then the kind of touching, */
        /* candidates for merging are pairs of neighbors with corresponding */
        /* left/right values. Finally we merge */

        typedef std::map< std::pair<double,double>, std::vector<int> > PointMap;
        PointMap oMap;
        for ( int i = 0; i < static_cast<int>(oBoundaries.size()); i++)
        {
            const Boundary & p = oBoundaries[i];
            OGRPoint startP, endP;
            p.poLine->StartPoint( &startP );
            p.poLine->EndPoint( &endP );
            oMap[ std::make_pair(startP.getX(), startP.getY()) ].push_back( i );
            oMap[ std::make_pair(endP.getX(), endP.getY()) ].push_back( i );
        }

        std::vector<int> endNeighbors( oBoundaries.size(), -1 );
        std::vector<int> startNeighbors( oBoundaries.size(), -1 );
        for ( PointMap::const_iterator it = oMap.begin(); it != oMap.end(); ++it )
        {
            if ( it->second.size() != 2 ) continue;
            int i = it->second[0];
            int j = it->second[1];

            const Boundary & p = oBoundaries[i];
            OGRPoint startP, endP;
            p.poLine->StartPoint( &startP );
            p.poLine->EndPoint( &endP );
            const Boundary & q = oBoundaries[j];
            OGRPoint startQ, endQ;
            q.poLine->StartPoint( &startQ );
            q.poLine->EndPoint( &endQ );
            if ( isEqual( p.dfRight, q.dfRight) && isEqual( p.dfLeft, q.dfLeft ) )
            {
                if ( endP.Equals( &startQ ) )
                {
                    endNeighbors[i] = j;
                    startNeighbors[j] = i;
                }
                if ( endQ.Equals( &startP ) )
                {
                    endNeighbors[j] = i;
                    startNeighbors[i] = j;
                }
            }
            if ( isEqual( p.dfRight, q.dfLeft) && isEqual( p.dfRight, q.dfLeft ) )
            {
                if ( startP.Equals( &startQ ) )
                {
                    startNeighbors[i] = j;
                    startNeighbors[j] = i;
                }
                if ( endP.Equals( &endQ ) )
                {
                    endNeighbors[j] = i;
                    endNeighbors[i] = j;
                }
            }
        }

        /* output all end lines (one neighbor only) and all their neighbors*/
        if( !oBoundaries.empty() )
        {
            std::vector<bool> oHasBeenMerged( oBoundaries.size(), false);
            for ( size_t i = 0; i < oBoundaries.size(); i++)
            {
                if ( !oHasBeenMerged[i] && ( startNeighbors[i] < 0 || endNeighbors[i] < 0 ) )
                {
                    oHasBeenMerged[i] = true;
                    Boundary * p = &oBoundaries[i];
                    int j =  startNeighbors[i] < 0 ? endNeighbors[i] : startNeighbors[i];
                    if ( startNeighbors[i] >= 0 )
                    {
                        /* reverse the line and left/right */
                        p->poLine->reversePoints();
                        std::swap( p->dfLeft, p->dfRight );
                    }
                    while ( j >= 0 )
                    {
                        assert( !oHasBeenMerged[j] );
                        oHasBeenMerged[j] = true;

                        OGRLineString * other = oBoundaries[j].poLine;
                        OGRPoint endP, startOther;
                        p->poLine->EndPoint( &endP );
                        other->StartPoint( &startOther );
                        if ( !endP.Equals( &startOther ) ) other->reversePoints();
                        p->poLine->addSubLineString( other, 1 );

                        /* next neighbor */
                        if ( endNeighbors[j] >= 0 && !oHasBeenMerged[endNeighbors[j]] )
                            j = endNeighbors[j];
                        else if ( startNeighbors[j] >= 0 && !oHasBeenMerged[startNeighbors[j]] )
                            j = startNeighbors[j];
                        else
                            j = -1;
                    }
                    WriteRoughness( p->poLine, p->dfLeft, p->dfRight );
                }
            }
            /* output all rings */
            for ( size_t i = 0; i < oBoundaries.size(); i++)
            {
                if ( oHasBeenMerged[i] ) continue;
                oHasBeenMerged[i] = true;
                Boundary * p = &oBoundaries[i];
                int j =  startNeighbors[i] < 0 ? endNeighbors[i] : startNeighbors[i];
                assert( j != -1 );
                if ( startNeighbors[i] >= 0 )
                {
                    /* reverse the line and left/right */
                    p->poLine->reversePoints();
                    std::swap( p->dfLeft, p->dfRight );
                }
                while ( !oHasBeenMerged[j] )
                {
                    oHasBeenMerged[j] = true;

                    OGRLineString * other = oBoundaries[j].poLine;
                    OGRPoint endP, startOther;
                    p->poLine->EndPoint( &endP );
                    other->StartPoint( &startOther );
                    if ( !endP.Equals( &startOther ) ) other->reversePoints();
                    p->poLine->addSubLineString( other, 1 );

                    /* next neighbor */
                    if ( endNeighbors[j] >= 0  )
                        j = endNeighbors[j];
                    else if ( startNeighbors[j] >= 0 )
                        j = startNeighbors[j];
                    else
                        assert(false); /* there must be a neighbor since it is a ring */
                }
                WriteRoughness( p->poLine, p->dfLeft, p->dfRight );
            }
        }
    }
    else
    {
        for ( size_t i = 0; i < oBoundaries.size(); i++)
        {
            Boundary * p = &oBoundaries[i];
            WriteRoughness( p->poLine, p->dfLeft, p->dfRight );
        }
    }
    poLayerDefn->Release();
    if (poSpatialReference) poSpatialReference->Release();
    for ( size_t i=0; i<oZones.size(); i++) delete oZones[i].poPolygon;
    for ( size_t i = 0; i < oBoundaries.size(); i++) delete oBoundaries[i].poLine;
}

/************************************************************************/
/*                            Simplify()                                */
/************************************************************************/
OGRLineString * OGRWAsPLayer::Simplify( const OGRLineString & line ) const
{
    if ( !line.getNumPoints() )
        return  line.clone();

    std::unique_ptr< OGRLineString > poLine(
        (
            pdfTolerance.get() && *pdfTolerance > 0
            ? line.Simplify( *pdfTolerance )->toLineString()
            : line.clone() ) );

    OGRPoint startPt, endPt;
    poLine->StartPoint( &startPt );
    poLine->EndPoint( &endPt );
    const bool isRing = CPL_TO_BOOL(startPt.Equals( &endPt ));

    if ( pdfAdjacentPointTolerance.get() && *pdfAdjacentPointTolerance > 0)
    {
        /* remove consecutive points that are too close */
        std::unique_ptr< OGRLineString > newLine( new OGRLineString );
        const double dist = *pdfAdjacentPointTolerance;
        OGRPoint pt;
        poLine->StartPoint( &pt );
        newLine->addPoint( &pt );
        const int iNumPoints= poLine->getNumPoints();
        for (int v=1; v<iNumPoints; v++)
        {
            if ( fabs(poLine->getX(v) - pt.getX()) > dist ||
                 fabs(poLine->getY(v) - pt.getY()) > dist )
            {
                poLine->getPoint( v, &pt );
                newLine->addPoint( &pt );
            }
        }

        /* force closed loop if initially closed */
        if ( isRing )
            newLine->setPoint( newLine->getNumPoints() - 1, &startPt );

        poLine.reset( newLine.release() );
    }

    if ( pdfPointToCircleRadius.get() && *pdfPointToCircleRadius > 0 )
    {
        const double radius = *pdfPointToCircleRadius;

#undef WASP_EXPERIMENTAL_CODE
#ifdef WASP_EXPERIMENTAL_CODE
        if ( 3 == poLine->getNumPoints() && isRing )
        {
            OGRPoint p0, p1;
            poLine->getPoint( 0, &p0 );
            poLine->getPoint( 1, &p1 );
            const double dir[2] = {
                p1.getX() - p0.getX(),
                p1.getY() - p0.getY() };
            const double dirNrm = sqrt( dir[0]*dir[0] + dir[1]*dir[1] );
            if ( dirNrm > radius )
            {
                /* convert to rectangle by finding the direction */
                /* and offsetting */
                const double ortho[2] = {-radius*dir[1]/dirNrm, radius*dir[0]/dirNrm};
                poLine->setNumPoints(5);
                poLine->setPoint(0, p0.getX() - ortho[0], p0.getY() - ortho[1]);
                poLine->setPoint(1, p1.getX() - ortho[0], p1.getY() - ortho[1]);
                poLine->setPoint(2, p1.getX() + ortho[0], p1.getY() + ortho[1]);
                poLine->setPoint(3, p0.getX() + ortho[0], p0.getY() + ortho[1]);
                poLine->setPoint(4, p0.getX() - ortho[0], p0.getY() - ortho[1]);
            }
            else
            {
                /* reduce to a point to be dealt with just after*/
                poLine->setNumPoints(1);
                poLine->setPoint(0,
                        0.5*(p0.getX()+p1.getX()),
                        0.5*(p0.getY()+p1.getY()));
            }
        }
#endif

        if ( 1 == poLine->getNumPoints() )
        {
            const int nbPt = 8;
            const double cx = poLine->getX(0);
            const double cy = poLine->getY(0);
            poLine->setNumPoints( nbPt + 1 );
            for ( int v = 0; v<=nbPt; v++ )
            {
                /* the % is necessary to make sure the ring */
                /* is really closed and not open due to     */
                /* roundoff error of cos(2pi) and sin(2pi)  */
                poLine->setPoint(v,
                        cx + radius*cos((v%nbPt)*(2*M_PI/nbPt)),
                        cy + radius*sin((v%nbPt)*(2*M_PI/nbPt)) );
            }
        }
    }

    return poLine.release();
}

/************************************************************************/
/*                            WriteElevation()                          */
/************************************************************************/

OGRErr OGRWAsPLayer::WriteElevation( OGRLineString * poGeom, const double & dfZ )

{
    std::unique_ptr< OGRLineString > poLine( Simplify( *poGeom ) );

    const int iNumPoints = poLine->getNumPoints();
    if ( !iNumPoints ) return OGRERR_NONE; /* empty geom */

    VSIFPrintfL( hFile, "%11.3f %11d", dfZ, iNumPoints );

    for (int v=0; v<iNumPoints; v++)
    {
        if (!(v%3)) VSIFPrintfL( hFile, "\n" );
        VSIFPrintfL( hFile, "%11.1f %11.1f ", poLine->getX(v), poLine->getY(v) );
    }
    VSIFPrintfL( hFile, "\n" );

    return OGRERR_NONE;
}

OGRErr OGRWAsPLayer::WriteElevation( OGRGeometry * poGeom, const double & dfZ )

{
    switch ( poGeom->getGeometryType() )
    {
    case wkbLineString:
    case wkbLineString25D:
        return WriteElevation( poGeom->toLineString(), dfZ );
    case wkbMultiLineString25D:
    case wkbMultiLineString:
    {
        for( auto&& poMember: poGeom->toGeometryCollection() )
        {
            const OGRErr err = WriteElevation( poMember, dfZ );
            if ( OGRERR_NONE != err ) return err;
        }
        return OGRERR_NONE;
    }
    default:
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot handle geometry of type %s",
                 OGRGeometryTypeToName( poGeom->getGeometryType() ) );
        break;
    }
    }
    return OGRERR_FAILURE; /* avoid visual warning */
}

/************************************************************************/
/*                            WriteRoughness()                          */
/************************************************************************/

OGRErr OGRWAsPLayer::WriteRoughness( OGRPolygon * poGeom, const double & dfZ )

{
    /* text intersection with polygons in the stack */
    /* for linestrings intersections, write linestring */
    /* for polygon intersection error */
    /* for point intersection do nothing */

    OGRErr err = OGRERR_NONE;
    OGREnvelope oEnvelope;
    poGeom->getEnvelope( &oEnvelope );
    for ( size_t i=0; i<oZones.size(); i++)
    {
        const bool bIntersects = CPL_TO_BOOL(oEnvelope.Intersects( oZones[i].oEnvelope ));
        if ( bIntersects && ( !bMerge || !isEqual( dfZ, oZones[i].dfZ ) ) ) /* boundary */
        {
            OGRGeometry * poIntersection = oZones[i].poPolygon->Intersection( poGeom );
            if ( poIntersection )
            {
                switch (poIntersection->getGeometryType())
                {
                case wkbLineString:
                case wkbLineString25D:
                {
                    Boundary oB = { poIntersection->toLineString()->clone(), dfZ, oZones[i].dfZ };
                    oBoundaries.push_back( oB );
                }
                break;
                case wkbMultiLineString:
                case wkbMultiLineString25D:
                {
                    /*TODO join the multilinestring into linestring*/
                    OGRLineString * poLine = nullptr;
                    OGRPoint * poStart = new OGRPoint;
                    OGRPoint * poEnd   = new OGRPoint;
                    for( auto&& poSubLine: poIntersection->toMultiLineString() )
                    {
                        poSubLine->StartPoint( poStart );

                        if( poLine == nullptr )
                        {
                            poLine = poSubLine->clone();
                        }
                        else if ( poLine->getNumPoints() == 0 || poStart->Equals( poEnd ) )
                        {
                            poLine->addSubLineString ( poSubLine, 1 );
                        }
                        else
                        {
                            Boundary oB = {poLine, dfZ, oZones[i].dfZ};
                            oBoundaries.push_back( oB );
                            poLine = poSubLine->clone();
                        }
                        poLine->EndPoint( poEnd );
                    }
                    Boundary oB = {poLine, dfZ, oZones[i].dfZ};
                    oBoundaries.push_back( oB );
                    delete poStart;
                    delete poEnd;
                }
                break;
                case wkbPolygon:
                case wkbPolygon25D:
                {
                            OGREnvelope oErrorRegion = oZones[i].oEnvelope;
                            oErrorRegion.Intersect( oEnvelope );
                            CPLError(CE_Failure, CPLE_NotSupported,
                                    "Overlapping polygons in rectangle (%.16g %.16g, %.16g %.16g))",
                                    oErrorRegion.MinX,
                                    oErrorRegion.MinY,
                                    oErrorRegion.MaxX,
                                    oErrorRegion.MaxY );
                            err = OGRERR_FAILURE;
                }
                break;
                case wkbGeometryCollection:
                case wkbGeometryCollection25D:
                {
                    for( auto&& poMember: poIntersection->toGeometryCollection() )
                    {
                        const OGRwkbGeometryType eType = poMember->getGeometryType();
                        if ( wkbFlatten(eType) == wkbPolygon )
                        {
                            OGREnvelope oErrorRegion = oZones[i].oEnvelope;
                            oErrorRegion.Intersect( oEnvelope );
                            CPLError(CE_Failure, CPLE_NotSupported,
                                    "Overlapping polygons in rectangle (%.16g %.16g, %.16g %.16g))",
                                    oErrorRegion.MinX,
                                    oErrorRegion.MinY,
                                    oErrorRegion.MaxX,
                                    oErrorRegion.MaxY );
                            err = OGRERR_FAILURE;
                        }
                    }
                }
                break;
                case wkbPoint:
                case wkbPoint25D:
                    /* do nothing */
                break;
                default:
                    CPLError(CE_Failure, CPLE_NotSupported,
                            "Unhandled polygon intersection of type %s",
                            OGRGeometryTypeToName( poIntersection->getGeometryType() ) );
                    err = OGRERR_FAILURE;
                }
            }
            delete poIntersection;
        }
    }

    Zone oZ =  { oEnvelope, poGeom->clone(), dfZ };
    oZones.push_back( oZ );
    return err;
}

OGRErr OGRWAsPLayer::WriteRoughness( OGRLineString * poGeom, const double & dfZleft,  const double & dfZright )

{
    std::unique_ptr< OGRLineString > poLine( Simplify( *poGeom ) );

    const int iNumPoints = poLine->getNumPoints();
    if ( !iNumPoints ) return OGRERR_NONE; /* empty geom */

    VSIFPrintfL( hFile, "%11.3f %11.3f %11d", dfZleft, dfZright, iNumPoints );

    for (int v=0; v<iNumPoints; v++)
    {
        if (!(v%3)) VSIFPrintfL( hFile, "\n  " );
        VSIFPrintfL( hFile, "%11.1f %11.1f ", poLine->getX(v), poLine->getY(v) );
    }
    VSIFPrintfL( hFile, "\n" );

    return OGRERR_NONE;
}

OGRErr OGRWAsPLayer::WriteRoughness( OGRGeometry * poGeom, const double & dfZleft,  const double & dfZright )

{
    switch ( poGeom->getGeometryType() )
    {
    case wkbLineString:
    case wkbLineString25D:
        return WriteRoughness( poGeom->toLineString(), dfZleft, dfZright );
    case wkbPolygon:
    case wkbPolygon25D:
        return WriteRoughness( poGeom->toPolygon(), dfZleft );
    case wkbMultiPolygon:
    case wkbMultiPolygon25D:
    case wkbMultiLineString25D:
    case wkbMultiLineString:
    {
        for( auto&& poMember: poGeom->toGeometryCollection() )
        {
            const OGRErr err = WriteRoughness( poMember, dfZleft, dfZright );
            if ( OGRERR_NONE != err ) return err;
        }
        return OGRERR_NONE;
    }
    default:
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot handle geometry of type %s",
                 OGRGeometryTypeToName( poGeom->getGeometryType() ) );
        break;
    }
    }
    return OGRERR_FAILURE; /* avoid visual warning */
}

/************************************************************************/
/*                            ICreateFeature()                            */
/************************************************************************/

OGRErr OGRWAsPLayer::ICreateFeature( OGRFeature * poFeature )

{
    if ( WRITE_ONLY != eMode)
    {
        CPLError(CE_Failure, CPLE_IllegalArg , "Layer is open read only" );
        return OGRERR_FAILURE;
    }

    /* This mainly checks for errors or inconsistencies */
    /* the real work is done by WriteElevation or WriteRoughness */
    if ( -1 == iFirstFieldIdx && !sFirstField.empty() )
    {
        CPLError(CE_Failure, CPLE_IllegalArg , "Cannot find field %s", sFirstField.c_str() );
        return OGRERR_FAILURE;
    }
    if ( -1 == iSecondFieldIdx && !sSecondField.empty() )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Cannot find field %s", sSecondField.c_str() );
        return OGRERR_FAILURE;
    }
    if ( -1 == iGeomFieldIdx && !sGeomField.empty() )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Cannot find field %s", sSecondField.c_str() );
        return OGRERR_FAILURE;
    }
    OGRGeometry * geom = poFeature->GetGeomFieldRef(iGeomFieldIdx);
    if ( !geom ) return OGRERR_NONE; /* null geom, nothing to do */

    const OGRwkbGeometryType geomType = geom->getGeometryType();
    const bool bPolygon = (geomType == wkbPolygon)
                         || (geomType == wkbPolygon25D)
                         || (geomType == wkbMultiPolygon)
                         || (geomType == wkbMultiPolygon25D);
    const bool bRoughness = (-1 != iSecondFieldIdx) || bPolygon ;

    double z1 = 0.0;
    if ( -1 != iFirstFieldIdx )
    {
        if (!poFeature->IsFieldSetAndNotNull(iFirstFieldIdx))
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Field %d %s is NULL", iFirstFieldIdx, sFirstField.c_str() );
            return OGRERR_FAILURE;
        }
        z1 = poFeature->GetFieldAsDouble(iFirstFieldIdx);
    }
    else
    {
        /* Case of z value for elevation or roughness, so we compute it */
        OGRPoint centroid;
        if ( geom->getCoordinateDimension() != 3 )
        {

            CPLError(CE_Failure, CPLE_NotSupported, "No field defined and no Z coordinate" );
            return OGRERR_FAILURE;
        }
        z1 = AvgZ( geom );
    }

    double z2 = 0.0;
    if ( -1 != iSecondFieldIdx )
    {
        if (!poFeature->IsFieldSetAndNotNull(iSecondFieldIdx))
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Field %d %s is NULL", iSecondFieldIdx, sSecondField.c_str() );
            return OGRERR_FAILURE;
        }
        z2 = poFeature->GetFieldAsDouble(iSecondFieldIdx);
    }
    else if ( bRoughness && !bPolygon )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "No right roughness field" );
        return OGRERR_FAILURE;
    }

    return bRoughness
        ? WriteRoughness( geom, z1, z2 )
        : WriteElevation( geom, z1 );
}

/************************************************************************/
/*                            CreateField()                            */
/************************************************************************/

OGRErr OGRWAsPLayer::CreateField( OGRFieldDefn *poField,
                                  CPL_UNUSED int bApproxOK )
{
    poLayerDefn->AddFieldDefn( poField );

    /* Update field indexes */
    if ( -1 == iFirstFieldIdx && ! sFirstField.empty() )
        iFirstFieldIdx = poLayerDefn->GetFieldIndex( sFirstField.c_str() );
    if ( -1 == iSecondFieldIdx && ! sSecondField.empty() )
        iSecondFieldIdx = poLayerDefn->GetFieldIndex( sSecondField.c_str() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRWAsPLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                      CPL_UNUSED int bApproxOK )
{
    OGRGeomFieldDefn oFieldDefn(poGeomFieldIn);
    if( oFieldDefn.GetSpatialRef() )
    {
        oFieldDefn.GetSpatialRef()->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    poLayerDefn->AddGeomFieldDefn( &oFieldDefn, FALSE );

    /* Update geom field index */
    if ( -1 == iGeomFieldIdx )
    {
        iGeomFieldIdx = poLayerDefn->GetGeomFieldIndex( sGeomField.c_str() );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature *OGRWAsPLayer::GetNextRawFeature()

{
    if ( READ_ONLY != eMode)
    {
        CPLError(CE_Failure, CPLE_IllegalArg , "Layer is open write only" );
        return nullptr;
    }

    const char * pszLine = CPLReadLineL( hFile );
    if ( !pszLine ) return nullptr;

    double dfValues[4];
    int iNumValues = 0;
    {
        std::istringstream iss(pszLine);
        while ( iNumValues < 4 && (iss >> dfValues[iNumValues] ) ){ ++iNumValues ;}

        if ( iNumValues < 2 )
        {
            if (iNumValues) CPLError(CE_Failure, CPLE_FileIO, "No enough values" );
            return nullptr;
        }
    }

    if( poLayerDefn->GetFieldCount() != iNumValues-1 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "looking for %d values and found %d on line: %s", poLayerDefn->GetFieldCount(), iNumValues-1, pszLine );
        return nullptr;
    }
    const double dfNumPairToRead = dfValues[iNumValues-1];
    if( !(dfNumPairToRead >= 0 && dfNumPairToRead < 1000000) ||
        static_cast<int>(dfNumPairToRead) != dfNumPairToRead )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Invalid coordinate number: %f", dfNumPairToRead );
        return nullptr;
    }

    std::unique_ptr< OGRFeature > poFeature( new OGRFeature( poLayerDefn ) );
    poFeature->SetFID( ++iFeatureCount );
    for ( int i=0; i<iNumValues-1; i++ ) poFeature->SetField( i, dfValues[i] );


    const int iNumValuesToRead = static_cast<int>(2*dfNumPairToRead);
    int iReadValues = 0;
    std::vector<double> values(iNumValuesToRead);
    for ( pszLine = CPLReadLineL( hFile );
            pszLine;
            pszLine = iNumValuesToRead > iReadValues ? CPLReadLineL( hFile ) : nullptr )
    {
        std::istringstream iss(pszLine);
        while ( iNumValuesToRead > iReadValues && (iss >> values[iReadValues] ) ){++iReadValues;}
    }
    if ( iNumValuesToRead != iReadValues )
    {
        CPLError(CE_Failure, CPLE_FileIO, "No enough values for linestring" );
        return nullptr;
    }
    std::unique_ptr< OGRLineString > poLine( new OGRLineString );
    poLine->setCoordinateDimension(3);
    poLine->assignSpatialReference( poSpatialReference );
    for ( int i=0; i<iNumValuesToRead; i+=2 )
    {
        poLine->addPoint( values[i], values[i+1], 0 );
    }
    poFeature->SetGeomFieldDirectly(0, poLine.release() );

    return poFeature.release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWAsPLayer::TestCapability( const char * pszCap )

{
    return ( WRITE_ONLY == eMode &&
       (EQUAL(pszCap,OLCSequentialWrite) ||
        EQUAL(pszCap,OLCCreateField) ||
        EQUAL(pszCap,OLCCreateGeomField) ) );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

void OGRWAsPLayer::ResetReading()
{
    iFeatureCount = 0;
    VSIFSeekL( hFile, iOffsetFeatureBegin, SEEK_SET );
}

/************************************************************************/
/*                           AvgZ()                                     */
/************************************************************************/

double OGRWAsPLayer::AvgZ( OGRLineString * poGeom )

{
    const int iNumPoints = poGeom->getNumPoints();
    double sum = 0;
    for (int v=0; v<iNumPoints; v++)
    {
        sum += poGeom->getZ(v);
    }
    return iNumPoints ? sum/iNumPoints : 0;
}

double OGRWAsPLayer::AvgZ( OGRPolygon * poGeom )

{
    return AvgZ( poGeom->getExteriorRing() );
}

double OGRWAsPLayer::AvgZ( OGRGeometryCollection * poGeom )

{
    return poGeom->getNumGeometries() ? AvgZ( poGeom->getGeometryRef(0) ) : 0;
}

double OGRWAsPLayer::AvgZ( OGRGeometry * poGeom )

{
    switch ( poGeom->getGeometryType() )
    {
    case wkbLineString:
    case wkbLineString25D:
        return AvgZ( static_cast< OGRLineString * >(poGeom) );
    case wkbPolygon:
    case wkbPolygon25D:
        return AvgZ( static_cast< OGRPolygon * >(poGeom) );
    case wkbMultiLineString:
    case wkbMultiLineString25D:

    case wkbMultiPolygon:
    case wkbMultiPolygon25D:
        return AvgZ( static_cast< OGRGeometryCollection * >(poGeom) );
    default:
        CPLError( CE_Warning, CPLE_NotSupported, "Unsupported geometry type in OGRWAsPLayer::AvgZ()");
        break;
    }
    return 0; /* avoid warning */
}

/************************************************************************/
/*                           DouglasPeucker()                           */
/************************************************************************/

//void DouglasPeucker(PointList[], epsilon)
//
//{
//    // Find the point with the maximum distance
//    double dmax = 0;
//    int index = 0;
//    int end = length(PointList).
//    for (int i = 1; i<end; i++)
//    {
//        const double d = shortestDistanceToSegment(PointList[i], Line(PointList[0], PointList[end]))
//        if ( d > dmax )
//        {
//            index = i
//            dmax = d
//        }
//    }
//    // If max distance is greater than epsilon, recursively simplify
//    if ( dmax > epsilon )
//    {
//        // Recursive call
//        recResults1[] = DouglasPeucker(PointList[1...index], epsilon)
//        recResults2[] = DouglasPeucker(PointList[index...end], epsilon)
//
//        // Build the result list
//        ResultList[] = {recResults1[1...end-1] recResults2[1...end]}
//    }
//    else
//    {
//        ResultList[] = {PointList[1], PointList[end]}
//    }
//    // Return the result
//    return ResultList[]
//}
