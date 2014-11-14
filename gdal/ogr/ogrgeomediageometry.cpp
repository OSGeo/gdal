/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements decoder of geomedia geometry blobs
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogrgeomediageometry.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

#define GEOMEDIA_POINT          0xC0
#define GEOMEDIA_ORIENTED_POINT 0xC8
#define GEOMEDIA_POLYLINE       0xC2
#define GEOMEDIA_POLYGON        0xC3
#define GEOMEDIA_BOUNDARY       0xC5
#define GEOMEDIA_COLLECTION     0xC6
#define GEOMEDIA_MULTILINE      0xCB
#define GEOMEDIA_MULTIPOLYGON   0xCC

/************************************************************************/
/*                       OGRCreateFromGeomedia()                        */
/************************************************************************/

OGRErr OGRCreateFromGeomedia( GByte *pabyGeom,
                              OGRGeometry **ppoGeom,
                              int nBytes )

{
    *ppoGeom = NULL;

    if( nBytes < 16 )
        return OGRERR_FAILURE;

    if( !(pabyGeom[1] == 0xFF && pabyGeom[2] == 0xD2 && pabyGeom[3] == 0x0F) )
        return OGRERR_FAILURE;

    int nGeomType = pabyGeom[0];
    pabyGeom += 16;
    nBytes -= 16;

    if( nGeomType == GEOMEDIA_POINT ||
        nGeomType == GEOMEDIA_ORIENTED_POINT )
    {
        if (nBytes < 3 * 8)
            return OGRERR_FAILURE;

        double dfX, dfY, dfZ;
        memcpy(&dfX, pabyGeom, 8);
        CPL_LSBPTR64(&dfX);
        memcpy(&dfY, pabyGeom + 8, 8);
        CPL_LSBPTR64(&dfY);
        memcpy(&dfZ, pabyGeom + 16, 8);
        CPL_LSBPTR64(&dfZ);

        *ppoGeom = new OGRPoint( dfX, dfY, dfZ );

         return OGRERR_NONE;
    }
    else if ( nGeomType == GEOMEDIA_POLYLINE )
    {
        if (nBytes < 4)
            return OGRERR_FAILURE;

        int nPoints;
        memcpy(&nPoints, pabyGeom, 4);
        CPL_LSBPTR32(&nPoints);

        pabyGeom += 4;
        nBytes -= 4;

        if (nPoints < 0 || nPoints > INT_MAX / 24 || nBytes < nPoints * 24)
            return OGRERR_FAILURE;

        OGRLineString* poLS = new OGRLineString();
        poLS->setNumPoints(nPoints);
        int i;
        for(i=0;i<nPoints;i++)
        {
            double dfX, dfY, dfZ;
            memcpy(&dfX, pabyGeom, 8);
            CPL_LSBPTR64(&dfX);
            memcpy(&dfY, pabyGeom + 8, 8);
            CPL_LSBPTR64(&dfY);
            memcpy(&dfZ, pabyGeom + 16, 8);
            CPL_LSBPTR64(&dfZ);

            poLS->setPoint(i, dfX, dfY, dfZ);

            pabyGeom += 24;
        }

        *ppoGeom = poLS;

        return OGRERR_NONE;
    }
    else if ( nGeomType == GEOMEDIA_POLYGON )
    {
        if (nBytes < 4)
            return OGRERR_FAILURE;

        int nPoints;
        memcpy(&nPoints, pabyGeom, 4);
        CPL_LSBPTR32(&nPoints);

        pabyGeom += 4;
        nBytes -= 4;

        if (nPoints < 0 || nPoints > INT_MAX / 24 || nBytes < nPoints * 24)
            return OGRERR_FAILURE;

        OGRLinearRing* poRing = new OGRLinearRing();
        poRing->setNumPoints(nPoints);
        int i;
        for(i=0;i<nPoints;i++)
        {
            double dfX, dfY, dfZ;
            memcpy(&dfX, pabyGeom, 8);
            CPL_LSBPTR64(&dfX);
            memcpy(&dfY, pabyGeom + 8, 8);
            CPL_LSBPTR64(&dfY);
            memcpy(&dfZ, pabyGeom + 16, 8);
            CPL_LSBPTR64(&dfZ);

            poRing->setPoint(i, dfX, dfY, dfZ);

            pabyGeom += 24;
        }

        OGRPolygon* poPoly = new OGRPolygon();
        poPoly->addRingDirectly(poRing);
        *ppoGeom = poPoly;

        return OGRERR_NONE;
    }
    else if ( nGeomType == GEOMEDIA_BOUNDARY )
    {
        if (nBytes < 4)
            return OGRERR_FAILURE;

        int nExteriorSize;
        memcpy(&nExteriorSize, pabyGeom, 4);
        CPL_LSBPTR32(&nExteriorSize);

        pabyGeom += 4;
        nBytes -= 4;

        if (nBytes < nExteriorSize)
            return OGRERR_FAILURE;

        OGRGeometry* poExteriorGeom = NULL;
        if (OGRCreateFromGeomedia( pabyGeom, &poExteriorGeom, nExteriorSize ) != OGRERR_NONE)
            return OGRERR_FAILURE;

        if ( wkbFlatten( poExteriorGeom->getGeometryType() ) != wkbPolygon )
        {
            delete poExteriorGeom;
            return OGRERR_FAILURE;
        }

        pabyGeom += nExteriorSize;
        nBytes -= nExteriorSize;

        if (nBytes < 4)
        {
            delete poExteriorGeom;
            return OGRERR_FAILURE;
        }

        int nInteriorSize;
        memcpy(&nInteriorSize, pabyGeom, 4);
        CPL_LSBPTR32(&nInteriorSize);

        pabyGeom += 4;
        nBytes -= 4;

        if (nBytes < nInteriorSize)
        {
            delete poExteriorGeom;
            return OGRERR_FAILURE;
        }

        OGRGeometry* poInteriorGeom = NULL;
        if (OGRCreateFromGeomedia( pabyGeom, &poInteriorGeom, nInteriorSize ) != OGRERR_NONE)
        {
            delete poExteriorGeom;
            return OGRERR_FAILURE;
        }

        OGRwkbGeometryType interiorGeomType = wkbFlatten( poInteriorGeom->getGeometryType() );
        if ( interiorGeomType == wkbPolygon )
        {
            ((OGRPolygon*)poExteriorGeom)->addRing(((OGRPolygon*)poInteriorGeom)->getExteriorRing());
        }
        else if ( interiorGeomType == wkbMultiPolygon )
        {
            int numGeom = ((OGRMultiPolygon*)poInteriorGeom)->getNumGeometries();
            for ( int i = 0; i < numGeom; ++i )
            {
                OGRPolygon* poInteriorPolygon = 
                    (OGRPolygon*)((OGRMultiPolygon*)poInteriorGeom)->getGeometryRef(i);
                ((OGRPolygon*)poExteriorGeom)->addRing( poInteriorPolygon->getExteriorRing() );
            }
        }
        else
        {
            delete poExteriorGeom;
            delete poInteriorGeom;
            return OGRERR_FAILURE;
        }

        delete poInteriorGeom;
        *ppoGeom = poExteriorGeom;

        return OGRERR_NONE;
    }
    else if ( nGeomType == GEOMEDIA_COLLECTION ||
              nGeomType == GEOMEDIA_MULTILINE ||
              nGeomType == GEOMEDIA_MULTIPOLYGON )
    {
        if (nBytes < 4)
            return OGRERR_FAILURE;

        int i;
        int nParts;
        memcpy(&nParts, pabyGeom, 4);
        CPL_LSBPTR32(&nParts);

        pabyGeom += 4;
        nBytes -= 4;

        if (nParts < 0 || nParts > INT_MAX / (4 + 16) || nBytes < nParts * (4 + 16))
            return OGRERR_FAILURE;

        /* Can this collection be considered as a multipolyline or multipolygon ? */
        if ( nGeomType == GEOMEDIA_COLLECTION )
        {
            GByte* pabyGeomBackup = pabyGeom;
            int nBytesBackup = nBytes;

            int bAllPolyline = TRUE;
            int bAllPolygon = TRUE;

            for(i=0;i<nParts;i++)
            {
                if (nBytes < 4)
                    return OGRERR_FAILURE;
                int nSubBytes;
                memcpy(&nSubBytes, pabyGeom, 4);
                CPL_LSBPTR32(&nSubBytes);

                if (nSubBytes < 0)
                {
                    return OGRERR_FAILURE;
                }

                pabyGeom += 4;
                nBytes -= 4;

                if (nBytes < nSubBytes)
                {
                    return OGRERR_FAILURE;
                }

                if( nSubBytes < 16 )
                    return OGRERR_FAILURE;

                if( !(pabyGeom[1] == 0xFF && pabyGeom[2] == 0xD2 && pabyGeom[3] == 0x0F) )
                    return OGRERR_FAILURE;

                int nSubGeomType = pabyGeom[0];
                if ( nSubGeomType != GEOMEDIA_POLYLINE )
                    bAllPolyline = FALSE;
                if ( nSubGeomType != GEOMEDIA_POLYGON )
                    bAllPolygon = FALSE;

                pabyGeom += nSubBytes;
                nBytes -= nSubBytes;
            }

            pabyGeom = pabyGeomBackup;
            nBytes = nBytesBackup;

            if (bAllPolyline)
                nGeomType = GEOMEDIA_MULTILINE;
            else if (bAllPolygon)
                nGeomType = GEOMEDIA_MULTIPOLYGON;
        }

        OGRGeometryCollection* poColl = (nGeomType == GEOMEDIA_MULTILINE) ? new OGRMultiLineString() :
                                        (nGeomType == GEOMEDIA_MULTIPOLYGON) ? new OGRMultiPolygon() :
                                                              new OGRGeometryCollection();

        for(i=0;i<nParts;i++)
        {
            if (nBytes < 4)
                return OGRERR_FAILURE;
            int nSubBytes;
            memcpy(&nSubBytes, pabyGeom, 4);
            CPL_LSBPTR32(&nSubBytes);

            if (nSubBytes < 0)
            {
                delete poColl;
                return OGRERR_FAILURE;
            }

            pabyGeom += 4;
            nBytes -= 4;

            if (nBytes < nSubBytes)
            {
                delete poColl;
                return OGRERR_FAILURE;
            }

            OGRGeometry* poSubGeom = NULL;
            if (OGRCreateFromGeomedia( pabyGeom, &poSubGeom, nSubBytes ) == OGRERR_NONE)
            {
                if (wkbFlatten(poColl->getGeometryType()) == wkbMultiPolygon &&
                    wkbFlatten(poSubGeom->getGeometryType()) == wkbLineString)
                {
                    OGRPolygon* poPoly = new OGRPolygon();
                    OGRLinearRing* poRing = new OGRLinearRing();
                    poRing->addSubLineString((OGRLineString*)poSubGeom);
                    poPoly->addRingDirectly(poRing);
                    delete poSubGeom;
                    poSubGeom = poPoly;
                }

                if (poColl->addGeometryDirectly(poSubGeom) != OGRERR_NONE)
                {
                    delete poSubGeom;
                }
            }

            pabyGeom += nSubBytes;
            nBytes -= nSubBytes;
        }

        *ppoGeom = poColl;

        return OGRERR_NONE;
    }
    else
    {
        CPLDebug("GEOMEDIA", "Unhandled type %d", nGeomType);
    }

    return OGRERR_FAILURE;
}


/************************************************************************/
/*                         OGRGetGeomediaSRS()                          */
/************************************************************************/

OGRSpatialReference* OGRGetGeomediaSRS(OGRFeature* poFeature)
{
    if (poFeature == NULL)
        return NULL;

    int nGeodeticDatum = poFeature->GetFieldAsInteger("GeodeticDatum");
    int nEllipsoid = poFeature->GetFieldAsInteger("Ellipsoid");
    int nProjAlgorithm = poFeature->GetFieldAsInteger("ProjAlgorithm");

    if (nGeodeticDatum == 17 && nEllipsoid == 22)
    {
        if (nProjAlgorithm == 12)
        {
            OGRSpatialReference* poSRS = new OGRSpatialReference();

            const char* pszDescription = poFeature->GetFieldAsString("Description");
            if (pszDescription && pszDescription[0] != 0)
                poSRS->SetNode( "PROJCS", pszDescription );
            poSRS->SetWellKnownGeogCS("WGS84");

            double dfStdP1 = poFeature->GetFieldAsDouble("StandPar1");
            double dfStdP2 = poFeature->GetFieldAsDouble("StandPar2");
            double dfCenterLat = poFeature->GetFieldAsDouble("LatOfOrigin");
            double dfCenterLong = poFeature->GetFieldAsDouble("LonOfOrigin");
            double dfFalseEasting = poFeature->GetFieldAsDouble("FalseX");
            double dfFalseNorthing = poFeature->GetFieldAsDouble("FalseY");
            poSRS->SetACEA( dfStdP1, dfStdP2,
                            dfCenterLat, dfCenterLong,
                            dfFalseEasting, dfFalseNorthing );
            return poSRS;
        }
    }

    return NULL;
}
