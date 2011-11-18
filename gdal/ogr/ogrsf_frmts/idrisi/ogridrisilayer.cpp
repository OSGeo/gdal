/******************************************************************************
 * $Id$
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_idrisi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRIdrisiLayer()                             */
/************************************************************************/

OGRIdrisiLayer::OGRIdrisiLayer( const char* pszLayerName, VSILFILE* fp,
                                OGRwkbGeometryType eGeomType,
                                const char* pszWTKString )

{
    this->fp = fp;
    this->eGeomType = eGeomType;
    nNextFID = 1;
    bEOF = FALSE;

    if (pszWTKString)
    {
        poSRS = new OGRSpatialReference();
        char* pszTmp = (char*)pszWTKString;
        poSRS->importFromWkt(&pszTmp);
    }
    else
        poSRS = NULL;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eGeomType );

    OGRFieldDefn oFieldDefn("id", OFTReal);
    poFeatureDefn->AddFieldDefn( &oFieldDefn );

    bExtentValid = FALSE;
    dfMinX = dfMinY = dfMaxX = dfMaxY = 0.0;

    VSIFSeekL( fp, 1, SEEK_SET );
    if (VSIFReadL( &nTotalFeatures, sizeof(unsigned int), 1, fp ) != 1)
        nTotalFeatures = 0;
    CPL_LSBPTR32(&nTotalFeatures);

    ResetReading();
}

/************************************************************************/
/*                          ~OGRIdrisiLayer()                           */
/************************************************************************/

OGRIdrisiLayer::~OGRIdrisiLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    poFeatureDefn->Release();

    VSIFCloseL( fp );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIdrisiLayer::ResetReading()

{
    nNextFID = 1;
    bEOF = FALSE;
    VSIFSeekL( fp, 0x105, SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRIdrisiLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(TRUE)
    {
        if (bEOF)
            return NULL;

        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
        {
            bEOF = TRUE;
            return NULL;
        }

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIdrisiLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    if (EQUAL(pszCap, OLCFastGetExtent))
        return bExtentValid;

    return FALSE;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRIdrisiLayer::GetNextRawFeature()
{
    while(TRUE)
    {
        if (eGeomType == wkbPoint)
        {
            double dfId;
            double dfX, dfY;
            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfX, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfY, sizeof(double), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfX);
            CPL_LSBPTR64(&dfY);

            if (m_poFilterGeom != NULL &&
                (dfX < m_sFilterEnvelope.MinX ||
                 dfX > m_sFilterEnvelope.MaxX ||
                 dfY < m_sFilterEnvelope.MinY ||
                 dfY > m_sFilterEnvelope.MaxY))
            {
                nNextFID ++;
                continue;
            }

            OGRPoint* poGeom = new OGRPoint(dfX, dfY);
            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetField(0, dfId);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetGeometryDirectly(poGeom);
            return poFeature;
        }
        else if (eGeomType == wkbLineString)
        {
            double dfId;
            double dfMinXShape, dfMaxXShape, dfMinYShape, dfMaxYShape;
            unsigned int nNodes;

            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinYShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxYShape, sizeof(double), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfMinXShape);
            CPL_LSBPTR64(&dfMaxXShape);
            CPL_LSBPTR64(&dfMinYShape);
            CPL_LSBPTR64(&dfMaxYShape);

            if (VSIFReadL(&nNodes, sizeof(unsigned int), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR32(&nNodes);

            if (nNodes > 100 * 1000 * 1000)
                return NULL;

            if (m_poFilterGeom != NULL &&
                (dfMaxXShape < m_sFilterEnvelope.MinX ||
                 dfMinXShape > m_sFilterEnvelope.MaxX ||
                 dfMaxYShape < m_sFilterEnvelope.MinY ||
                 dfMinYShape > m_sFilterEnvelope.MaxY))
            {
                nNextFID ++;
                VSIFSeekL(fp, sizeof(OGRRawPoint) * nNodes, SEEK_CUR);
                continue;
            }

            OGRRawPoint* poRawPoints = (OGRRawPoint*)VSIMalloc2(sizeof(OGRRawPoint), nNodes);
            if (poRawPoints == NULL)
            {
                return NULL;
            }

            if ((unsigned int)VSIFReadL(poRawPoints, sizeof(OGRRawPoint), nNodes, fp) != nNodes)
            {
                VSIFree(poRawPoints);
                return NULL;
            }

#if defined(CPL_MSB)
            for(unsigned int iNode=0; iNode<nNodes; iNode++)
            {
                CPL_LSBPTR64(&poRawPoints[iNode].x);
                CPL_LSBPTR64(&poRawPoints[iNode].y);
            }
#endif

            OGRLineString* poGeom = new OGRLineString();
            poGeom->setPoints(nNodes, poRawPoints, NULL);

            VSIFree(poRawPoints);

            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetField(0, dfId);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetGeometryDirectly(poGeom);
            return poFeature;
        }
        else /* if (eGeomType == wkbPolygon) */
        {
            double dfId;
            double dfMinXShape, dfMaxXShape, dfMinYShape, dfMaxYShape;
            unsigned int nParts;
            unsigned int nTotalNodes;

            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinYShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxYShape, sizeof(double), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfMinXShape);
            CPL_LSBPTR64(&dfMaxXShape);
            CPL_LSBPTR64(&dfMinYShape);
            CPL_LSBPTR64(&dfMaxYShape);
            if (VSIFReadL(&nParts, sizeof(unsigned int), 1, fp) != 1 ||
                VSIFReadL(&nTotalNodes, sizeof(unsigned int), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR32(&nParts);
            CPL_LSBPTR32(&nTotalNodes);

            if (nParts > 100000 || nTotalNodes > 100 * 1000 * 1000)
                return NULL;

            if (m_poFilterGeom != NULL &&
                (dfMaxXShape < m_sFilterEnvelope.MinX ||
                 dfMinXShape > m_sFilterEnvelope.MaxX ||
                 dfMaxYShape < m_sFilterEnvelope.MinY ||
                 dfMinYShape > m_sFilterEnvelope.MaxY))
            {
                unsigned int iPart;
                for(iPart = 0; iPart < nParts; iPart ++)
                {
                    unsigned int nNodes;
                    if (VSIFReadL(&nNodes, sizeof(unsigned int), 1, fp) != 1)
                        return NULL;
                    CPL_LSBPTR32(&nNodes);
                    if (nNodes > nTotalNodes)
                        return NULL;
                    VSIFSeekL(fp, sizeof(OGRRawPoint) * nNodes, SEEK_CUR);
                }
                nNextFID ++;
                continue;
            }

            OGRRawPoint* poRawPoints = (OGRRawPoint*)VSIMalloc2(sizeof(OGRRawPoint), nTotalNodes);
            if (poRawPoints == NULL)
            {
                return NULL;
            }

            unsigned int iPart;
            OGRPolygon* poGeom = new OGRPolygon();
            for(iPart = 0; iPart < nParts; iPart ++)
            {
                unsigned int nNodes;
                if (VSIFReadL(&nNodes, sizeof(unsigned int), 1, fp) != 1)
                {
                    VSIFree(poRawPoints);
                    delete poGeom;
                    return NULL;
                }
                CPL_LSBPTR32(&nNodes);

                if (nNodes > nTotalNodes ||
                    (unsigned int)VSIFReadL(poRawPoints, sizeof(OGRRawPoint), nNodes, fp) != nNodes)
                {
                    VSIFree(poRawPoints);
                    delete poGeom;
                    return NULL;
                }

#if defined(CPL_MSB)
                for(unsigned int iNode=0; iNode<nNodes; iNode++)
                {
                    CPL_LSBPTR64(&poRawPoints[iNode].x);
                    CPL_LSBPTR64(&poRawPoints[iNode].y);
                }
#endif

                OGRLinearRing* poLR = new OGRLinearRing();
                poGeom->addRingDirectly(poLR);
                poLR->setPoints(nNodes, poRawPoints, NULL);
            }

            VSIFree(poRawPoints);

            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetField(0, dfId);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetGeometryDirectly(poGeom);
            return poFeature;
        }
    }
}

/************************************************************************/
/*                             SetExtent()                              */
/************************************************************************/

void OGRIdrisiLayer::SetExtent(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY)
{
    bExtentValid = TRUE;
    this->dfMinX = dfMinX;
    this->dfMinY = dfMinY;
    this->dfMaxX = dfMaxX;
    this->dfMaxY = dfMaxY;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRIdrisiLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (!bExtentValid)
        return OGRLayer::GetExtent(psExtent, bForce);

    psExtent->MinX = dfMinX;
    psExtent->MinY = dfMinY;
    psExtent->MaxX = dfMaxX;
    psExtent->MaxY = dfMaxY;
    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRIdrisiLayer::GetFeatureCount( int bForce )
{
    if (nTotalFeatures > 0 && m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return nTotalFeatures;

    return OGRLayer::GetFeatureCount(bForce);
}
