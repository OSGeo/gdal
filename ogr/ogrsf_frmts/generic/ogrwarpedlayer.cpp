/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRWarpedLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef DOXYGEN_SKIP

#include <cmath>

#include "ogrwarpedlayer.h"

/************************************************************************/
/*                          OGRWarpedLayer()                            */
/************************************************************************/

OGRWarpedLayer::OGRWarpedLayer(OGRLayer *poDecoratedLayer, int iGeomField,
                               int bTakeOwnership,
                               OGRCoordinateTransformation *poCT,
                               OGRCoordinateTransformation *poReversedCT)
    : OGRLayerDecorator(poDecoratedLayer, bTakeOwnership),
      m_poFeatureDefn(nullptr), m_iGeomField(iGeomField), m_poCT(poCT),
      m_poReversedCT(poReversedCT),
      m_poSRS(const_cast<OGRSpatialReference *>(m_poCT->GetTargetCS()))
{
    CPLAssert(poCT != nullptr);
    SetDescription(poDecoratedLayer->GetDescription());

    if (m_poSRS != nullptr)
    {
        m_poSRS->Reference();
    }
}

/************************************************************************/
/*                         ~OGRWarpedLayer()                            */
/************************************************************************/

OGRWarpedLayer::~OGRWarpedLayer()
{
    if (m_poFeatureDefn != nullptr)
        m_poFeatureDefn->Release();
    if (m_poSRS != nullptr)
        m_poSRS->Release();
    delete m_poCT;
    delete m_poReversedCT;
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRWarpedLayer::SetSpatialFilter(OGRGeometry *poGeom)
{
    SetSpatialFilter(0, poGeom);
}

/************************************************************************/
/*                          SetSpatialFilterRect()                      */
/************************************************************************/

void OGRWarpedLayer::SetSpatialFilterRect(double dfMinX, double dfMinY,
                                          double dfMaxX, double dfMaxY)
{
    OGRLayer::SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRWarpedLayer::SetSpatialFilter(int iGeomField, OGRGeometry *poGeom)
{
    if (iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid geometry field index : %d", iGeomField);
        return;
    }

    m_iGeomFieldFilter = iGeomField;
    if (InstallFilter(poGeom))
        ResetReading();

    if (m_iGeomFieldFilter == m_iGeomField)
    {
        if (poGeom == nullptr || m_poReversedCT == nullptr)
        {
            m_poDecoratedLayer->SetSpatialFilter(m_iGeomFieldFilter, nullptr);
        }
        else
        {
            OGREnvelope sEnvelope;
            poGeom->getEnvelope(&sEnvelope);
            if (std::isinf(sEnvelope.MinX) && std::isinf(sEnvelope.MinY) &&
                std::isinf(sEnvelope.MaxX) && std::isinf(sEnvelope.MaxY))
            {
                m_poDecoratedLayer->SetSpatialFilterRect(
                    m_iGeomFieldFilter, sEnvelope.MinX, sEnvelope.MinY,
                    sEnvelope.MaxX, sEnvelope.MaxY);
            }
            else if (ReprojectEnvelope(&sEnvelope, m_poReversedCT))
            {
                m_poDecoratedLayer->SetSpatialFilterRect(
                    m_iGeomFieldFilter, sEnvelope.MinX, sEnvelope.MinY,
                    sEnvelope.MaxX, sEnvelope.MaxY);
            }
            else
            {
                m_poDecoratedLayer->SetSpatialFilter(m_iGeomFieldFilter,
                                                     nullptr);
            }
        }
    }
    else
    {
        m_poDecoratedLayer->SetSpatialFilter(m_iGeomFieldFilter, poGeom);
    }
}

/************************************************************************/
/*                        SetSpatialFilterRect()                        */
/************************************************************************/

void OGRWarpedLayer::SetSpatialFilterRect(int iGeomField, double dfMinX,
                                          double dfMinY, double dfMaxX,
                                          double dfMaxY)
{
    OGRLayer::SetSpatialFilterRect(iGeomField, dfMinX, dfMinY, dfMaxX, dfMaxY);
}

/************************************************************************/
/*                     SrcFeatureToWarpedFeature()                      */
/************************************************************************/

std::unique_ptr<OGRFeature>
OGRWarpedLayer::SrcFeatureToWarpedFeature(std::unique_ptr<OGRFeature> poFeature)
{
    // This is safe to do here as they have matching attribute and geometry
    // fields
    poFeature->SetFDefnUnsafe(GetLayerDefn());

    OGRGeometry *poGeom = poFeature->GetGeomFieldRef(m_iGeomField);
    if (poGeom && poGeom->transform(m_poCT) != OGRERR_NONE)
    {
        delete poFeature->StealGeometry(m_iGeomField);
    }

    return poFeature;
}

/************************************************************************/
/*                     WarpedFeatureToSrcFeature()                      */
/************************************************************************/

std::unique_ptr<OGRFeature>
OGRWarpedLayer::WarpedFeatureToSrcFeature(std::unique_ptr<OGRFeature> poFeature)
{
    // This is safe to do here as they have matching attribute and geometry
    // fields
    poFeature->SetFDefnUnsafe(m_poDecoratedLayer->GetLayerDefn());

    OGRGeometry *poGeom = poFeature->GetGeomFieldRef(m_iGeomField);
    if (poGeom &&
        (!m_poReversedCT || poGeom->transform(m_poReversedCT) != OGRERR_NONE))
    {
        return nullptr;
    }

    return poFeature;
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature *OGRWarpedLayer::GetNextFeature()
{
    while (true)
    {
        auto poFeature =
            std::unique_ptr<OGRFeature>(m_poDecoratedLayer->GetNextFeature());
        if (!poFeature)
            return nullptr;

        auto poFeatureNew = SrcFeatureToWarpedFeature(std::move(poFeature));
        const OGRGeometry *poGeom = poFeatureNew->GetGeomFieldRef(m_iGeomField);
        if (m_poFilterGeom != nullptr && !FilterGeometry(poGeom))
        {
            continue;
        }

        return poFeatureNew.release();
    }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRWarpedLayer::GetFeature(GIntBig nFID)
{
    auto poFeature =
        std::unique_ptr<OGRFeature>(m_poDecoratedLayer->GetFeature(nFID));
    if (poFeature)
    {
        poFeature = SrcFeatureToWarpedFeature(std::move(poFeature));
    }
    return poFeature.release();
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRWarpedLayer::ISetFeature(OGRFeature *poFeature)
{
    auto poFeatureNew = WarpedFeatureToSrcFeature(
        std::unique_ptr<OGRFeature>(poFeature->Clone()));
    if (!poFeatureNew)
        return OGRERR_FAILURE;

    return m_poDecoratedLayer->SetFeature(poFeatureNew.get());
}

/************************************************************************/
/*                            ICreateFeature()                           */
/************************************************************************/

OGRErr OGRWarpedLayer::ICreateFeature(OGRFeature *poFeature)
{
    auto poFeatureNew = WarpedFeatureToSrcFeature(
        std::unique_ptr<OGRFeature>(poFeature->Clone()));
    if (!poFeatureNew)
        return OGRERR_FAILURE;

    return m_poDecoratedLayer->CreateFeature(poFeatureNew.get());
}

/************************************************************************/
/*                            IUpsertFeature()                          */
/************************************************************************/

OGRErr OGRWarpedLayer::IUpsertFeature(OGRFeature *poFeature)
{
    auto poFeatureNew = WarpedFeatureToSrcFeature(
        std::unique_ptr<OGRFeature>(poFeature->Clone()));
    if (poFeatureNew == nullptr)
        return OGRERR_FAILURE;

    return m_poDecoratedLayer->UpsertFeature(poFeatureNew.get());
}

/************************************************************************/
/*                            IUpdateFeature()                          */
/************************************************************************/

OGRErr OGRWarpedLayer::IUpdateFeature(OGRFeature *poFeature,
                                      int nUpdatedFieldsCount,
                                      const int *panUpdatedFieldsIdx,
                                      int nUpdatedGeomFieldsCount,
                                      const int *panUpdatedGeomFieldsIdx,
                                      bool bUpdateStyleString)
{
    auto poFeatureNew = WarpedFeatureToSrcFeature(
        std::unique_ptr<OGRFeature>(poFeature->Clone()));
    if (!poFeatureNew)
        return OGRERR_FAILURE;

    return m_poDecoratedLayer->UpdateFeature(
        poFeatureNew.get(), nUpdatedFieldsCount, panUpdatedFieldsIdx,
        nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx, bUpdateStyleString);
}

/************************************************************************/
/*                            GetLayerDefn()                           */
/************************************************************************/

OGRFeatureDefn *OGRWarpedLayer::GetLayerDefn()
{
    if (m_poFeatureDefn != nullptr)
        return m_poFeatureDefn;

    m_poFeatureDefn = m_poDecoratedLayer->GetLayerDefn()->Clone();
    m_poFeatureDefn->Reference();
    if (m_poFeatureDefn->GetGeomFieldCount() > 0)
        m_poFeatureDefn->GetGeomFieldDefn(m_iGeomField)->SetSpatialRef(m_poSRS);

    return m_poFeatureDefn;
}

/************************************************************************/
/*                            GetSpatialRef()                           */
/************************************************************************/

OGRSpatialReference *OGRWarpedLayer::GetSpatialRef()
{
    if (m_iGeomField == 0)
        return m_poSRS;
    else
        return OGRLayer::GetSpatialRef();
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRWarpedLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == nullptr)
        return m_poDecoratedLayer->GetFeatureCount(bForce);

    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRWarpedLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return GetExtent(0, psExtent, bForce);
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRWarpedLayer::GetExtent(int iGeomField, OGREnvelope *psExtent,
                                 int bForce)
{
    if (iGeomField == m_iGeomField)
    {
        if (sStaticEnvelope.IsInit())
        {
            *psExtent = sStaticEnvelope;
            return OGRERR_NONE;
        }

        OGREnvelope sExtent;
        OGRErr eErr =
            m_poDecoratedLayer->GetExtent(m_iGeomField, &sExtent, bForce);
        if (eErr != OGRERR_NONE)
            return eErr;

        if (ReprojectEnvelope(&sExtent, m_poCT))
        {
            *psExtent = sExtent;
            return OGRERR_NONE;
        }
        else
            return OGRERR_FAILURE;
    }
    else
        return m_poDecoratedLayer->GetExtent(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                    TransformAndUpdateBBAndReturnX()                  */
/************************************************************************/

static double TransformAndUpdateBBAndReturnX(OGRCoordinateTransformation *poCT,
                                             double dfX, double dfY,
                                             double &dfMinX, double &dfMinY,
                                             double &dfMaxX, double &dfMaxY)
{
    int bSuccess = FALSE;
    poCT->Transform(1, &dfX, &dfY, nullptr, nullptr, &bSuccess);
    if (bSuccess)
    {
        if (dfX < dfMinX)
            dfMinX = dfX;
        if (dfY < dfMinY)
            dfMinY = dfY;
        if (dfX > dfMaxX)
            dfMaxX = dfX;
        if (dfY > dfMaxY)
            dfMaxY = dfY;
        return dfX;
    }

    return 0.0;
}

/************************************************************************/
/*                            FindXDiscontinuity()                      */
/************************************************************************/

static void FindXDiscontinuity(OGRCoordinateTransformation *poCT, double dfX1,
                               double dfX2, double dfY, double &dfMinX,
                               double &dfMinY, double &dfMaxX, double &dfMaxY,
                               int nRecLevel = 0)
{
    double dfXMid = (dfX1 + dfX2) / 2;

    double dfWrkX1 = TransformAndUpdateBBAndReturnX(poCT, dfX1, dfY, dfMinX,
                                                    dfMinY, dfMaxX, dfMaxY);
    double dfWrkXMid = TransformAndUpdateBBAndReturnX(poCT, dfXMid, dfY, dfMinX,
                                                      dfMinY, dfMaxX, dfMaxY);
    double dfWrkX2 = TransformAndUpdateBBAndReturnX(poCT, dfX2, dfY, dfMinX,
                                                    dfMinY, dfMaxX, dfMaxY);

    double dfDX1 = dfWrkXMid - dfWrkX1;
    double dfDX2 = dfWrkX2 - dfWrkXMid;

    if (dfDX1 * dfDX2 < 0 && nRecLevel < 30)
    {
        FindXDiscontinuity(poCT, dfX1, dfXMid, dfY, dfMinX, dfMinY, dfMaxX,
                           dfMaxY, nRecLevel + 1);
        FindXDiscontinuity(poCT, dfXMid, dfX2, dfY, dfMinX, dfMinY, dfMaxX,
                           dfMaxY, nRecLevel + 1);
    }
}

/************************************************************************/
/*                            ReprojectEnvelope()                       */
/************************************************************************/

int OGRWarpedLayer::ReprojectEnvelope(OGREnvelope *psEnvelope,
                                      OGRCoordinateTransformation *poCT)
{
    const int NSTEP = 20;
    double dfXStep = (psEnvelope->MaxX - psEnvelope->MinX) / NSTEP;
    double dfYStep = (psEnvelope->MaxY - psEnvelope->MinY) / NSTEP;

    double *padfX = static_cast<double *>(
        VSI_MALLOC_VERBOSE((NSTEP + 1) * (NSTEP + 1) * sizeof(double)));
    double *padfY = static_cast<double *>(
        VSI_MALLOC_VERBOSE((NSTEP + 1) * (NSTEP + 1) * sizeof(double)));
    int *pabSuccess = static_cast<int *>(
        VSI_MALLOC_VERBOSE((NSTEP + 1) * (NSTEP + 1) * sizeof(int)));
    if (padfX == nullptr || padfY == nullptr || pabSuccess == nullptr)
    {
        VSIFree(padfX);
        VSIFree(padfY);
        VSIFree(pabSuccess);
        return FALSE;
    }

    for (int j = 0; j <= NSTEP; j++)
    {
        for (int i = 0; i <= NSTEP; i++)
        {
            padfX[j * (NSTEP + 1) + i] = psEnvelope->MinX + i * dfXStep;
            padfY[j * (NSTEP + 1) + i] = psEnvelope->MinY + j * dfYStep;
        }
    }

    int bRet = FALSE;

    if (poCT->Transform((NSTEP + 1) * (NSTEP + 1), padfX, padfY, nullptr,
                        nullptr, pabSuccess))
    {
        double dfMinX = 0.0;
        double dfMinY = 0.0;
        double dfMaxX = 0.0;
        double dfMaxY = 0.0;
        int bSet = FALSE;
        for (int j = 0; j <= NSTEP; j++)
        {
            double dfXOld = 0.0;
            double dfDXOld = 0.0;
            int iOld = -1;
            int iOldOld = -1;
            for (int i = 0; i <= NSTEP; i++)
            {
                if (pabSuccess[j * (NSTEP + 1) + i])
                {
                    double dfX = padfX[j * (NSTEP + 1) + i];
                    double dfY = padfY[j * (NSTEP + 1) + i];

                    if (!bSet)
                    {
                        dfMinX = dfX;
                        dfMaxX = dfX;
                        dfMinY = dfY;
                        dfMaxY = dfY;
                        bSet = TRUE;
                    }
                    else
                    {
                        if (dfX < dfMinX)
                            dfMinX = dfX;
                        if (dfY < dfMinY)
                            dfMinY = dfY;
                        if (dfX > dfMaxX)
                            dfMaxX = dfX;
                        if (dfY > dfMaxY)
                            dfMaxY = dfY;
                    }

                    if (iOld >= 0)
                    {
                        double dfDXNew = dfX - dfXOld;
                        if (iOldOld >= 0 && dfDXNew * dfDXOld < 0)
                        {
                            FindXDiscontinuity(
                                poCT, psEnvelope->MinX + iOldOld * dfXStep,
                                psEnvelope->MinX + i * dfXStep,
                                psEnvelope->MinY + j * dfYStep, dfMinX, dfMinY,
                                dfMaxX, dfMaxY);
                        }
                        dfDXOld = dfDXNew;
                    }

                    dfXOld = dfX;
                    iOldOld = iOld;
                    iOld = i;
                }
            }
        }
        if (bSet)
        {
            psEnvelope->MinX = dfMinX;
            psEnvelope->MinY = dfMinY;
            psEnvelope->MaxX = dfMaxX;
            psEnvelope->MaxY = dfMaxY;
            bRet = TRUE;
        }
    }

    VSIFree(padfX);
    VSIFree(padfY);
    VSIFree(pabSuccess);

    return bRet;
}

/************************************************************************/
/*                             TestCapability()                         */
/************************************************************************/

int OGRWarpedLayer::TestCapability(const char *pszCapability)
{
    if (EQUAL(pszCapability, OLCFastGetExtent) && sStaticEnvelope.IsInit())
        return TRUE;

    int bVal = m_poDecoratedLayer->TestCapability(pszCapability);

    if (EQUAL(pszCapability, OLCFastGetArrowStream))
        return false;

    if (EQUAL(pszCapability, OLCFastSpatialFilter) ||
        EQUAL(pszCapability, OLCRandomWrite) ||
        EQUAL(pszCapability, OLCSequentialWrite))
    {
        if (bVal)
            bVal = m_poReversedCT != nullptr;
    }
    else if (EQUAL(pszCapability, OLCFastFeatureCount))
    {
        if (bVal)
            bVal = m_poFilterGeom == nullptr;
    }

    return bVal;
}

/************************************************************************/
/*                              SetExtent()                             */
/************************************************************************/

void OGRWarpedLayer::SetExtent(double dfXMin, double dfYMin, double dfXMax,
                               double dfYMax)
{
    sStaticEnvelope.MinX = dfXMin;
    sStaticEnvelope.MinY = dfYMin;
    sStaticEnvelope.MaxX = dfXMax;
    sStaticEnvelope.MaxY = dfYMax;
}

/************************************************************************/
/*                            GetArrowStream()                          */
/************************************************************************/

bool OGRWarpedLayer::GetArrowStream(struct ArrowArrayStream *out_stream,
                                    CSLConstList papszOptions)
{
    return OGRLayer::GetArrowStream(out_stream, papszOptions);
}

#endif /* #ifndef DOXYGEN_SKIP */
