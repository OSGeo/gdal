/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurveCollection class.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

CPL_CVSID("$Id");

/************************************************************************/
/*                         OGRCurveCollection()                         */
/************************************************************************/

OGRCurveCollection::OGRCurveCollection()

{
    nCurveCount = 0;
    papoCurves = NULL;
}

/************************************************************************/
/*                         ~OGRCurveCollection()                        */
/************************************************************************/

OGRCurveCollection::~OGRCurveCollection()

{
    empty(NULL);
}

/************************************************************************/
/*                              WkbSize()                               */
/************************************************************************/

int OGRCurveCollection::WkbSize() const
{
    int         nSize = 9;

    for( int i = 0; i < nCurveCount; i++ )
    {
        nSize += papoCurves[i]->WkbSize();
    }

    return nSize;
}

/************************************************************************/
/*                          addCurveDirectly()                          */
/************************************************************************/

OGRErr OGRCurveCollection::addCurveDirectly( OGRGeometry* poGeom,
                                             OGRCurve* poCurve,
                                             int bNeedRealloc )
{
    if( poCurve->getCoordinateDimension() == 3 && poGeom->getCoordinateDimension() != 3 )
        poGeom->setCoordinateDimension(3);
    else if( poCurve->getCoordinateDimension() != 3 && poGeom->getCoordinateDimension() == 3 )
        poCurve->setCoordinateDimension(3);

    if( bNeedRealloc )
    {
        papoCurves = (OGRCurve **) OGRRealloc( papoCurves,
                                             sizeof(OGRCurve*) * (nCurveCount+1) );
    }

    papoCurves[nCurveCount] = poCurve;

    nCurveCount++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        importPreambuleFromWkb()                      */
/************************************************************************/

OGRErr OGRCurveCollection::importPreambuleFromWkb( OGRGeometry* poGeom,
                                                   unsigned char * pabyData,
                                                   int& nSize,
                                                   int& nDataOffset,
                                                   OGRwkbByteOrder& eByteOrder,
                                                   int nMinSubGeomSize,
                                                   OGRwkbVariant eWkbVariant )
{
    OGRErr eErr = poGeom->importPreambuleOfCollectionFromWkb(
                                                        pabyData,
                                                        nSize,
                                                        nDataOffset,
                                                        eByteOrder,
                                                        nMinSubGeomSize,
                                                        nCurveCount,
                                                        eWkbVariant );
    if( eErr >= 0 )
        return eErr;

    papoCurves = (OGRCurve **) VSIMalloc2(sizeof(void*), nCurveCount);
    if (nCurveCount != 0 && papoCurves == NULL)
    {
        nCurveCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    return -1;
}

/************************************************************************/
/*                       importBodyFromWkb()                            */
/************************************************************************/

OGRErr OGRCurveCollection::importBodyFromWkb( OGRGeometry* poGeom,
                                       unsigned char * pabyData,
                                       int nSize,
                                       int nDataOffset,
                                       int bAcceptCompoundCurve,
                                       OGRErr (*pfnAddCurveDirectlyFromWkb)(OGRGeometry* poGeom, OGRCurve* poCurve),
                                       OGRwkbVariant eWkbVariant )
{

/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
    int nIter = nCurveCount;
    nCurveCount = 0;
    for( int iGeom = 0; iGeom < nIter; iGeom++ )
    {
        OGRErr  eErr;
        OGRGeometry* poSubGeom = NULL;

        /* Parses sub-geometry */
        unsigned char* pabySubData = pabyData + nDataOffset;
        if( nSize < 9 && nSize != -1 )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRwkbGeometryType eSubGeomType;
        OGRBoolean bIs3D;
        if ( OGRReadWKBGeometryType( pabySubData, eWkbVariant, &eSubGeomType, &bIs3D ) != OGRERR_NONE )
            return OGRERR_FAILURE;

        if( (eSubGeomType != wkbCompoundCurve && OGR_GT_IsCurve(eSubGeomType)) ||
            (bAcceptCompoundCurve && eSubGeomType == wkbCompoundCurve) )
        {
            eErr = OGRGeometryFactory::
                createFromWkb( pabySubData, NULL,
                               &poSubGeom, nSize, eWkbVariant );
        }
        else
        {
            CPLDebug("OGR", "Cannot add geometry of type (%d) to geometry of type (%d)",
                     eSubGeomType, poGeom->getGeometryType());
            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        if( eErr == OGRERR_NONE )
            eErr = pfnAddCurveDirectlyFromWkb(poGeom, (OGRCurve*)poSubGeom);
        if( eErr != OGRERR_NONE )
        {
            delete poSubGeom;
            return eErr;
        }

        int nSubGeomWkbSize = poSubGeom->WkbSize();
        if( nSize != -1 )
            nSize -= nSubGeomWkbSize;

        nDataOffset += nSubGeomWkbSize;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRCurveCollection::exportToWkt( const OGRGeometry* poGeom,
                                        char ** ppszDstText ) const

{
    char        **papszGeoms;
    int         iGeom, nCumulativeLength = 0;
    OGRErr      eErr;

    if( nCurveCount == 0 )
    {
        CPLString osEmpty;
        if( poGeom->getCoordinateDimension()  == 3 )
            osEmpty.Printf("%s Z EMPTY",poGeom->getGeometryName());
        else
            osEmpty.Printf("%s EMPTY",poGeom->getGeometryName());
        *ppszDstText = CPLStrdup(osEmpty);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each Geom.     */
/* -------------------------------------------------------------------- */
    papszGeoms = (char **) CPLCalloc(sizeof(char *),nCurveCount);

    for( iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        eErr = papoCurves[iGeom]->exportToWkt( &(papszGeoms[iGeom]), wkbVariantIso );
        if( eErr != OGRERR_NONE )
            goto error;

        nCumulativeLength += strlen(papszGeoms[iGeom]);
    }
    
/* -------------------------------------------------------------------- */
/*      Allocate the right amount of space for the aggregated string    */
/* -------------------------------------------------------------------- */
    *ppszDstText = (char *) VSIMalloc(nCumulativeLength + nCurveCount +
                                    strlen(poGeom->getGeometryName()) + 10);

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, poGeom->getGeometryName() );
    if( poGeom->getCoordinateDimension() == 3 )
        strcat( *ppszDstText, " Z" );
    strcat( *ppszDstText, " (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( iGeom > 0 )
            (*ppszDstText)[nCumulativeLength++] = ',';

        /* We must strip the explicit "LINESTRING " prefix */
        int nSkip = 0;
        if( !papoCurves[iGeom]->IsEmpty() &&
            EQUALN(papszGeoms[iGeom], "LINESTRING ", strlen("LINESTRING ")) )
        {
            nSkip = strlen("LINESTRING ");
            if( EQUALN(papszGeoms[iGeom] + nSkip, "Z ", 2) )
                nSkip += 2;
        }

        int nGeomLength = strlen(papszGeoms[iGeom] + nSkip);
        memcpy( *ppszDstText + nCumulativeLength, papszGeoms[iGeom] + nSkip, nGeomLength );
        nCumulativeLength += nGeomLength;
        VSIFree( papszGeoms[iGeom] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszGeoms );

    return OGRERR_NONE;

error:
    for( iGeom = 0; iGeom < nCurveCount; iGeom++ )
        CPLFree( papszGeoms[iGeom] );
    CPLFree( papszGeoms );
    return eErr;
}

/************************************************************************/
/*                            exportToWkb()                             */
/************************************************************************/

OGRErr OGRCurveCollection::exportToWkb( const OGRGeometry* poGeom,
                                        OGRwkbByteOrder eByteOrder,
                                        unsigned char * pabyData,
                                        OGRwkbVariant eWkbVariant ) const
{
    int         nOffset;
    
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type, ensuring that 3D flag is         */
/*      preserved.                                                      */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = poGeom->getIsoGeometryType();
    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        int bIs3D = wkbHasZ((OGRwkbGeometryType)nGType);
        nGType = wkbFlatten(nGType);
        if( nGType == wkbCurvePolygon )
            nGType = POSTGIS15_CURVEPOLYGON;
        if( bIs3D )
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse); /* yes we explicitely set wkb25DBit */
    }

    if( eByteOrder == wkbNDR )
        nGType = CPL_LSBWORD32( nGType );
    else
        nGType = CPL_MSBWORD32( nGType );

    memcpy( pabyData + 1, &nGType, 4 );
    
/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        int     nCount;

        nCount = CPL_SWAP32( nCurveCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &nCurveCount, 4 );
    }
    
    nOffset = 9;
    
/* ==================================================================== */
/*      Serialize each of the Geoms.                                    */
/* ==================================================================== */
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        papoCurves[iGeom]->exportToWkb( eByteOrder, pabyData + nOffset, eWkbVariant );

        nOffset += papoCurves[iGeom]->WkbSize();
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRCurveCollection::empty(OGRGeometry* poGeom)
{
    if( papoCurves != NULL )
    {
        for( int i = 0; i < nCurveCount; i++ )
        {
            delete papoCurves[i];
        }
        OGRFree( papoCurves );
    }

    nCurveCount = 0;
    papoCurves = NULL;
    if( poGeom )
        poGeom->setCoordinateDimension(2);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurveCollection::getEnvelope( OGREnvelope * psEnvelope ) const
{
    OGREnvelope3D         oEnv3D;
    getEnvelope(&oEnv3D);
    psEnvelope->MinX = oEnv3D.MinX;
    psEnvelope->MinY = oEnv3D.MinY;
    psEnvelope->MaxX = oEnv3D.MaxX;
    psEnvelope->MaxY = oEnv3D.MaxY;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurveCollection::getEnvelope( OGREnvelope3D * psEnvelope ) const
{
    OGREnvelope3D       oGeomEnv;
    int                 bExtentSet = FALSE;

    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if (!papoCurves[iGeom]->IsEmpty())
        {
            if (!bExtentSet)
            {
                papoCurves[iGeom]->getEnvelope( psEnvelope );
                bExtentSet = TRUE;
            }
            else
            {
                papoCurves[iGeom]->getEnvelope( &oGeomEnv );
                psEnvelope->Merge( oGeomEnv );
            }
        }
    }

    if (!bExtentSet)
    {
        psEnvelope->MinX = psEnvelope->MinY = psEnvelope->MinZ = 0;
        psEnvelope->MaxX = psEnvelope->MaxY = psEnvelope->MaxZ = 0;
    }
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRCurveCollection::IsEmpty() const
{
    return nCurveCount == 0;
}

/************************************************************************/
/*                               Equals()                                */
/************************************************************************/

OGRBoolean  OGRCurveCollection::Equals( OGRCurveCollection *poOCC ) const
{
    if( getNumCurves() != poOCC->getNumCurves() )
        return FALSE;
    
    // we should eventually test the SRS.

    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( !getCurve(iGeom)->Equals(poOCC->getCurve(iGeom)) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRCurveCollection::setCoordinateDimension( OGRGeometry* poGeom,
                                                 int nNewDimension )
{
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        papoCurves[iGeom]->setCoordinateDimension( nNewDimension );
    }

    poGeom->OGRGeometry::setCoordinateDimension( nNewDimension );
}

/************************************************************************/
/*                          getNumCurves()                              */
/************************************************************************/

int          OGRCurveCollection::getNumCurves() const
{
    return nCurveCount;
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

OGRCurve    *OGRCurveCollection::getCurve( int i )
{
    if( i < 0 || i >= nCurveCount )
        return NULL;
    return papoCurves[i];
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

const OGRCurve *OGRCurveCollection::getCurve( int i ) const
{
    if( i < 0 || i >= nCurveCount )
        return NULL;
    return papoCurves[i];
}

/************************************************************************/
/*                           stealCurve()                               */
/************************************************************************/

OGRCurve* OGRCurveCollection::stealCurve( int i )
{
    if( i < 0 || i >= nCurveCount )
        return NULL;
    OGRCurve* poRet = papoCurves[i];
    if( i < nCurveCount - 1 )
    {
        memmove(papoCurves + i, papoCurves + i + 1, (nCurveCount - i - 1) * sizeof(OGRCurve*));
    }
    nCurveCount --;
    return poRet;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr  OGRCurveCollection::transform( OGRGeometry* poGeom,
                                       OGRCoordinateTransformation *poCT )
{
#ifdef DISABLE_OGRGEOM_TRANSFORM
    return OGRERR_FAILURE;
#else
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        OGRErr  eErr;

        eErr = papoCurves[iGeom]->transform( poCT );
        if( eErr != OGRERR_NONE )
        {
            if( iGeom != 0 )
            {
                CPLDebug("OGR", 
                         "OGRCurveCollection::transform() failed for a geometry other\n"
                         "than the first, meaning some geometries are transformed\n"
                         "and some are not!\n" );

                return OGRERR_FAILURE;
            }

            return eErr;
        }
    }

    poGeom->assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRCurveCollection::flattenTo2D(OGRGeometry* poGeom)
{
    for( int i = 0; i < nCurveCount; i++ )
        papoCurves[i]->flattenTo2D();

    poGeom->setCoordinateDimension(2);
}

/************************************************************************/
/*                              segmentize()                            */
/************************************************************************/

void OGRCurveCollection::segmentize(double dfMaxLength)
{
    for( int i = 0; i < nCurveCount; i++ )
        papoCurves[i]->segmentize(dfMaxLength);
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRCurveCollection::swapXY()
{
    for( int i = 0; i < nCurveCount; i++ )
        papoCurves[i]->swapXY();
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRCurveCollection::hasCurveGeometry(int bLookForNonLinear) const
{
    for( int i = 0; i < nCurveCount; i++ )
    {
        if( papoCurves[i]->hasCurveGeometry(bLookForNonLinear) )
            return TRUE;
    }
    return FALSE;
}
