/******************************************************************************
 * Project:  ogr linear referencing utility
 * Purpose:  main source file
 * Author:   Dmitry Baryshnikov (aka Bishop), polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (C) 2014 NextGIS
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogrsf_frmts.h"
#include "gdalargumentparser.h"

#include <limits>
#include <map>
#include <set>
#include <vector>

#define FIELD_START "beg"
#define FIELD_FINISH "end"
#define FIELD_SCALE_FACTOR "scale"
constexpr double DELTA = 0.00000001;  // - delta
#ifdef HAVE_GEOS
constexpr double TOLERANCE_DEGREE = 0.00008983153;
constexpr double TOLERANCE_METER = 10.0;
#endif

enum operation
{
    op_unknown = 0,
    op_create,
    op_get_pos,
    op_get_coord,
    op_get_subline
};

typedef struct _curve_data
{
    OGRLineString *pPart;
    double dfBeg, dfEnd, dfFactor;

    bool IsInside(const double &dfDist) const
    {
        return (dfDist + DELTA >= dfBeg) && (dfDist - DELTA <= dfEnd);
    }
} CURVE_DATA;

/************************************************************************/
/*                         SetupTargetLayer()                           */
/************************************************************************/

static OGRLayer *SetupTargetLayer(OGRLayer *poSrcLayer, GDALDataset *poDstDS,
                                  char **papszLCO, const char *pszNewLayerName,
                                  const char *pszOutputSepFieldName = nullptr)
{
    const CPLString szLayerName =
        pszNewLayerName == nullptr
            ? CPLGetBasenameSafe(poDstDS->GetDescription())
            : pszNewLayerName;

    /* -------------------------------------------------------------------- */
    /*      Get other info.                                                 */
    /* -------------------------------------------------------------------- */
    OGRFeatureDefn *poSrcFDefn = poSrcLayer->GetLayerDefn();

    /* -------------------------------------------------------------------- */
    /*      Find requested geometry fields.                                 */
    /* -------------------------------------------------------------------- */

    OGRSpatialReference *poOutputSRS = poSrcLayer->GetSpatialRef();

    /* -------------------------------------------------------------------- */
    /*      Find the layer.                                                 */
    /* -------------------------------------------------------------------- */

    // GetLayerByName() can instantiate layers that would have been
    // 'hidden' otherwise, for example, non-spatial tables in a
    // PostGIS-enabled database, so this apparently useless command is
    // not useless... (#4012)
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRLayer *poDstLayer = poDstDS->GetLayerByName(szLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    if (poDstLayer != nullptr)
    {
        const int nLayerCount = poDstDS->GetLayerCount();
        int iLayer = -1;  // Used after for.
        for (iLayer = 0; iLayer < nLayerCount; iLayer++)
        {
            OGRLayer *poLayer = poDstDS->GetLayer(iLayer);
            if (poLayer == poDstLayer)
                break;
        }

        if (iLayer == nLayerCount)
            // Should not happen with an ideal driver.
            poDstLayer = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      If the layer does not exist, then create it.                    */
    /* -------------------------------------------------------------------- */
    if (poDstLayer == nullptr)
    {
        if (!poDstDS->TestCapability(ODsCCreateLayer))
        {
            fprintf(stderr,
                    _("Layer %s not found, and "
                      "CreateLayer not supported by driver.\n"),
                    szLayerName.c_str());
            return nullptr;
        }

        OGRwkbGeometryType eGType = wkbLineString;

        CPLErrorReset();

        if (poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            eGType = wkbNone;
        }

        poDstLayer = poDstDS->CreateLayer(
            szLayerName, poOutputSRS, static_cast<OGRwkbGeometryType>(eGType),
            papszLCO);

        if (poDstLayer == nullptr)
            return nullptr;

        if (poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            OGRGeomFieldDefn oGFldDefn(poSrcFDefn->GetGeomFieldDefn(0));
            if (poOutputSRS != nullptr)
                oGFldDefn.SetSpatialRef(poOutputSRS);
            oGFldDefn.SetType(wkbLineString);
            poDstLayer->CreateGeomField(&oGFldDefn);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise we will append to it, if append was requested.        */
    /* -------------------------------------------------------------------- */
    else
    {
        fprintf(stderr, _("FAILED: Layer %s already exists.\n"),
                szLayerName.c_str());
        return nullptr;
    }

    // Create beg, end, scale factor fields.
    OGRFieldDefn oFieldDefn_Beg(FIELD_START, OFTReal);
    if (poDstLayer->CreateField(&oFieldDefn_Beg) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                 oFieldDefn_Beg.GetNameRef());
        return nullptr;
    }

    OGRFieldDefn oFieldDefn_End(FIELD_FINISH, OFTReal);
    if (poDstLayer->CreateField(&oFieldDefn_End) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                 oFieldDefn_End.GetNameRef());
        return nullptr;
    }

    OGRFieldDefn oFieldDefn_SF(FIELD_SCALE_FACTOR, OFTReal);
    if (poDstLayer->CreateField(&oFieldDefn_SF) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                 oFieldDefn_SF.GetNameRef());
        return nullptr;
    }

    if (pszOutputSepFieldName != nullptr)
    {
        OGRFieldDefn oSepField(pszOutputSepFieldName, OFTString);
        oSepField.SetWidth(254);
        if (poDstLayer->CreateField(&oSepField) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                     oSepField.GetNameRef());
            return nullptr;
        }
    }

    // Now that we've created a field, GetLayerDefn() won't return NULL.
    OGRFeatureDefn *poDstFDefn = poDstLayer->GetLayerDefn();

    // Sanity check: if it fails, the driver is buggy.
    if (poDstFDefn != nullptr && poDstFDefn->GetFieldCount() != 3)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The output driver has claimed to have added the %s field, "
                 "but it did not!",
                 oFieldDefn_Beg.GetNameRef());
    }

    return poDstLayer;
}

//------------------------------------------------------------------------
// AddFeature
//------------------------------------------------------------------------

static OGRErr AddFeature(OGRLayer *const poOutLayer, OGRLineString *pPart,
                         double dfFrom, double dfTo, double dfScaleFactor,
                         bool bQuiet,
                         const char *pszOutputSepFieldName = nullptr,
                         const char *pszOutputSepFieldValue = nullptr)
{
    OGRFeature *poFeature =
        OGRFeature::CreateFeature(poOutLayer->GetLayerDefn());

    poFeature->SetField(FIELD_START, dfFrom);
    poFeature->SetField(FIELD_FINISH, dfTo);
    poFeature->SetField(FIELD_SCALE_FACTOR, dfScaleFactor);

    if (pszOutputSepFieldName != nullptr)
    {
        poFeature->SetField(pszOutputSepFieldName, pszOutputSepFieldValue);
    }

    poFeature->SetGeometryDirectly(pPart);

    if (poOutLayer->CreateFeature(poFeature) != OGRERR_NONE)
    {
        if (!bQuiet)
            printf("Failed to create feature in shapefile.\n");
        return OGRERR_FAILURE;
    }

    OGRFeature::DestroyFeature(poFeature);

    return OGRERR_NONE;
}

//------------------------------------------------------------------------
// CreateSubline
//------------------------------------------------------------------------
static OGRErr CreateSubline(OGRLayer *const poPkLayer, double dfPosBeg,
                            double dfPosEnd, OGRLayer *const poOutLayer,
                            CPL_UNUSED int bDisplayProgress, bool bQuiet)
{
    // Get step
    poPkLayer->ResetReading();
    OGRFeature *pFeature = poPkLayer->GetNextFeature();
    if (nullptr != pFeature)
    {
        // FIXME: Clang Static Analyzer rightly found that the following
        // code is dead
        // dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
        // dfEnd = pFeature->GetFieldAsDouble(FIELD_FINISH);
        OGRFeature::DestroyFeature(pFeature);
    }
    else
    {
        fprintf(stderr, _("Get step for positions %f - %f failed\n"), dfPosBeg,
                dfPosEnd);
        return OGRERR_FAILURE;
    }
    // Get second part.
    double dfBeg = 0.0;
    double dfEnd = 0.0;
    pFeature = poPkLayer->GetNextFeature();
    if (nullptr != pFeature)
    {
        dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
        dfEnd = pFeature->GetFieldAsDouble(FIELD_FINISH);
        OGRFeature::DestroyFeature(pFeature);
    }
    else
    {
        fprintf(stderr, _("Get step for positions %f - %f failed\n"), dfPosBeg,
                dfPosEnd);
        return OGRERR_FAILURE;
    }
    const double dfStep = dfEnd - dfBeg;

    // Round input to step
    const double dfPosBegLow = floor(dfPosBeg / dfStep) * dfStep;
    const double dfPosEndHigh = ceil(dfPosEnd / dfStep) * dfStep;

    CPLString szAttributeFilter;
    szAttributeFilter.Printf("%s >= %f AND %s <= %f", FIELD_START, dfPosBegLow,
                             FIELD_FINISH, dfPosEndHigh);
    // TODO: ExecuteSQL should be faster.
    poPkLayer->SetAttributeFilter(szAttributeFilter);
    poPkLayer->ResetReading();

    std::map<double, OGRFeature *> moParts;

    while ((pFeature = poPkLayer->GetNextFeature()) != nullptr)
    {
        double dfStart = pFeature->GetFieldAsDouble(FIELD_START);
        moParts[dfStart] = pFeature;
    }

    if (moParts.empty())
    {
        fprintf(stderr, _("Get parts for positions %f - %f failed\n"), dfPosBeg,
                dfPosEnd);
        return OGRERR_FAILURE;
    }

    OGRLineString SubLine;
    if (moParts.size() == 1)
    {
        std::map<double, OGRFeature *>::iterator IT = moParts.begin();
        const double dfStart = IT->first;
        double dfPosBegCorr = dfPosBeg - dfStart;
        const double dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosBegCorr *= dfSF;

        const double dfPosEndCorr = (dfPosEnd - dfStart) * dfSF;

        OGRLineString *pLine = IT->second->GetGeometryRef()->toLineString();

        OGRLineString *pSubLine =
            pLine->getSubLine(dfPosBegCorr, dfPosEndCorr, FALSE);

        OGRFeature::DestroyFeature(IT->second);
        // Store.
        return AddFeature(poOutLayer, pSubLine, dfPosBeg, dfPosEnd, 1.0,
                          bQuiet);
    }
    else
    {
        int nCounter = static_cast<int>(moParts.size());
        std::map<double, OGRFeature *>::iterator IT = moParts.begin();
        OGRLineString *pOutLine = new OGRLineString();
        // Get first part.
        const double dfStart = IT->first;
        double dfPosBegCorr = dfPosBeg - dfStart;
        double dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosBegCorr *= dfSF;

        OGRLineString *pLine = IT->second->GetGeometryRef()->toLineString();

        OGRLineString *pSubLine =
            pLine->getSubLine(dfPosBegCorr, pLine->get_Length(), FALSE);

        pOutLine->addSubLineString(pSubLine);
        delete pSubLine;
        OGRFeature::DestroyFeature(IT->second);

        ++IT;
        nCounter--;

        while (nCounter > 1)
        {
            pLine = IT->second->GetGeometryRef()->toLineString();
            pOutLine->addSubLineString(pLine);
            OGRFeature::DestroyFeature(IT->second);
            ++IT;
            nCounter--;
        }

        // Get last part
        double dfPosEndCorr = dfPosEnd - IT->first;
        dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosEndCorr *= dfSF;

        pLine = IT->second->GetGeometryRef()->toLineString();

        pSubLine = pLine->getSubLine(0, dfPosEndCorr, FALSE);

        pOutLine->addSubLineString(pSubLine);
        delete pSubLine;

        OGRFeature::DestroyFeature(IT->second);
        // Store
        return AddFeature(poOutLayer, pOutLine, dfPosBeg, dfPosEnd, 1.0,
                          bQuiet);
    }
}

//------------------------------------------------------------------------
// Project
//------------------------------------------------------------------------
#ifdef HAVE_GEOS
static double Project(OGRLineString *pLine, OGRPoint *pPoint)
{
    if (nullptr == pLine || nullptr == pPoint)
        return -1;
    OGRPoint TestPoint;
    pLine->StartPoint(&TestPoint);
    if (TestPoint.Equals(pPoint))
        return 0.0;
    pLine->EndPoint(&TestPoint);
    if (TestPoint.Equals(pPoint))
        return pLine->get_Length();

    return pLine->Project(pPoint);
}
#endif

//------------------------------------------------------------------------
// CreatePartsFromLineString
//------------------------------------------------------------------------
#ifdef HAVE_GEOS
static OGRErr CreatePartsFromLineString(
    OGRLineString *pPathGeom, OGRLayer *const poPkLayer, int nMValField,
    double dfStep, OGRLayer *const poOutLayer, int bDisplayProgress,
    bool bQuiet, const char *pszOutputSepFieldName = nullptr,
    const char *pszOutputSepFieldValue = nullptr)
{
    // Check repers/milestones/reference points type
    OGRwkbGeometryType eGeomType = poPkLayer->GetGeomType();
    if (wkbFlatten(eGeomType) != wkbPoint)
    {
        fprintf(stderr, _("Unsupported geometry type %s for path\n"),
                OGRGeometryTypeToName(eGeomType));
        return OGRERR_FAILURE;
    }

    double dfTolerance = 1.0;
    const OGRSpatialReference *pSpaRef = pPathGeom->getSpatialReference();
    if (pSpaRef->IsGeographic())
    {
        dfTolerance = TOLERANCE_DEGREE;
    }
    else
    {
        dfTolerance = TOLERANCE_METER;
    }

    // Create sorted list of repers.
    std::map<double, OGRPoint *> moRepers;
    poPkLayer->ResetReading();
    OGRFeature *pReperFeature = nullptr;
    while ((pReperFeature = poPkLayer->GetNextFeature()) != nullptr)
    {
        const double dfReperPos = pReperFeature->GetFieldAsDouble(nMValField);
        OGRGeometry *pGeom = pReperFeature->GetGeometryRef();
        if (nullptr != pGeom)
        {
            OGRPoint *pPt = pGeom->clone()->toPoint();
            if (!bQuiet)
            {
                if (moRepers.find(dfReperPos) != moRepers.end())
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "The distance %f is already present in repers file!",
                        dfReperPos);
                }
            }
            // Check if reper is inside the path
            const double dfTestDistance = Project(pPathGeom, pPt);
            if (dfTestDistance < 0)
            {
                if (!bQuiet)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The distance %f is out of path!", dfReperPos);
                }
                delete pPt;
            }
            else
            {
                const double dfDist = pPathGeom->Distance(pPt);
                if (dfDist < dfTolerance)
                    moRepers[dfReperPos] = pPt;
                else
                    delete pPt;
            }
        }
        OGRFeature::DestroyFeature(pReperFeature);
    }

    if (moRepers.size() < 2)
    {
        fprintf(stderr, _("Not enough repers to proceed.\n"));
        return OGRERR_FAILURE;
    }

    // Check direction.
    if (!bQuiet)
    {
        fprintf(stdout, "Check path direction.\n");
    }

    // Get distance along path from pt1 and pt2.
    // If pt1 distance > pt2 distance, reverse path
    std::map<double, OGRPoint *>::const_iterator IT;
    IT = moRepers.begin();
    double dfPosition = IT->first;
    double dfBeginPosition = IT->first;
    OGRPoint *pt1 = IT->second;
    ++IT;
    OGRPoint *pt2 = IT->second;

    double dfDistance1 = Project(pPathGeom, pt1);
    double dfDistance2 = Project(pPathGeom, pt2);

    if (dfDistance1 > dfDistance2)
    {
        if (!bQuiet)
        {
            fprintf(stderr,
                    _("Warning: The path is opposite the repers direction. "
                      "Let's reverse path.\n"));
        }
        pPathGeom->reversePoints();

        dfDistance1 = Project(pPathGeom, pt1);
        dfDistance2 = Project(pPathGeom, pt2);
    }

    OGRLineString *pPart = nullptr;

    std::vector<CURVE_DATA> astSubLines;

    if (!bQuiet)
    {
        fprintf(stdout, "Create parts\n");
    }

    // Get first part
    // If first point is not at the beginning of the path
    // The first part should be from the beginning of the path
    // to the first point. length == part.getLength
    OGRPoint *pPtBeg = nullptr;
    OGRPoint *pPtEnd = nullptr;
    double dfPtBegPosition = 0.0;
    double dfPtEndPosition = 0.0;

    if (dfDistance1 > DELTA)
    {
        pPart = pPathGeom->getSubLine(0, dfDistance1, FALSE);
        if (nullptr != pPart)
        {
            double dfLen = pPart->get_Length();
            if (pSpaRef->IsGeographic())
            {
                // convert to UTM/WGS84
                OGRPoint pt;
                pPart->Value(dfLen / 2, &pt);
                const int nZoneEnv =
                    static_cast<int>(30 + (pt.getX() + 3.0) / 6.0 + 0.5);
                int nEPSG;
                if (pt.getY() > 0)
                {
                    nEPSG = 32600 + nZoneEnv;
                }
                else
                {
                    nEPSG = 32700 + nZoneEnv;
                }
                OGRSpatialReference SpatRef;
                SpatRef.importFromEPSG(nEPSG);
                OGRGeometry *pTransformPart = pPart->clone();
                if (pTransformPart->transformTo(&SpatRef) == OGRERR_NONE)
                {
                    OGRLineString *pTransformPartLS =
                        pTransformPart->toLineString();
                    dfLen = pTransformPartLS->get_Length();
                }

                CURVE_DATA data = {pPart, dfPosition - dfLen, dfPosition,
                                   pPart->get_Length() / dfLen};
                astSubLines.push_back(data);

                pPtBeg = new OGRPoint();
                pPart->getPoint(0, pPtBeg);
                dfPtBegPosition = dfPosition - dfLen;

                // AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition,
                //            pPart->get_Length() / dfLen, bQuiet);
                delete pTransformPart;
            }
            else
            {
                CURVE_DATA data = {pPart, dfPosition - dfLen, dfPosition, 1.0};
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition,
                //            1.0, bQuiet);
                pPtBeg = new OGRPoint();
                pPart->getPoint(0, pPtBeg);
                dfPtBegPosition = dfPosition - dfLen;
            }
        }
    }

    if (dfDistance2 - dfDistance1 > DELTA)
    {
        pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
        if (nullptr != pPart)
        {
            CURVE_DATA data = {pPart, dfPosition, IT->first,
                               pPart->get_Length() / (IT->first - dfPosition)};
            astSubLines.push_back(data);
            // AddFeature(poOutLayer, pPart, dfPosition, IT->first,
            //            pPart->get_Length() / (IT->first - dfPosition),
            //            bQuiet);
        }
    }

    GDALProgressFunc pfnProgress = nullptr;
    void *pProgressArg = nullptr;

    double dfFactor = 1.0 / moRepers.size();
    if (bDisplayProgress)
    {
        pfnProgress = GDALScaledProgress;
        pProgressArg =
            GDALCreateScaledProgress(0.0, 1.0, GDALTermProgress, nullptr);
    }

    int nCount = 2;
    dfDistance1 = dfDistance2;
    dfPosition = IT->first;
    ++IT;  // Get third point

    double dfEndPosition = 0.0;
    while (IT != moRepers.end())
    {
        if (bDisplayProgress)
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        dfEndPosition = IT->first;

        dfDistance2 = Project(pPathGeom, IT->second);

        if (dfDistance2 - dfDistance1 > DELTA)
        {
            pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
            if (nullptr != pPart)
            {
                CURVE_DATA data = {pPart, dfPosition, IT->first,
                                   pPart->get_Length() /
                                       (IT->first - dfPosition)};
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition, IT->first,
                //            pPart->get_Length() / (IT->first - dfPosition),
                //            bQuiet);
                dfDistance1 = dfDistance2;
                dfPosition = IT->first;
            }
        }

        ++IT;
    }

    // Get last part
    if (pPathGeom->get_Length() - dfDistance1 > DELTA)
    {
        pPart =
            pPathGeom->getSubLine(dfDistance1, pPathGeom->get_Length(), FALSE);
        if (nullptr != pPart)
        {
            double dfLen = pPart->get_Length();
            if (pSpaRef->IsGeographic())
            {
                // convert to UTM/WGS84
                OGRPoint pt;
                pPart->Value(dfLen / 2, &pt);
                const int nZoneEnv =
                    static_cast<int>(30 + (pt.getX() + 3.0) / 6.0 + 0.5);
                int nEPSG;
                if (pt.getY() > 0)
                {
                    nEPSG = 32600 + nZoneEnv;
                }
                else
                {
                    nEPSG = 32700 + nZoneEnv;
                }
                OGRSpatialReference SpatRef;
                SpatRef.importFromEPSG(nEPSG);
                OGRGeometry *pTransformPart = pPart->clone();
                if (pTransformPart->transformTo(&SpatRef) == OGRERR_NONE)
                {
                    OGRLineString *pTransformPartLS =
                        pTransformPart->toLineString();
                    dfLen = pTransformPartLS->get_Length();
                }
                CURVE_DATA data = {pPart, dfPosition, dfPosition + dfLen,
                                   pPart->get_Length() / dfLen};
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition, dfPosition + dfLen,
                //            pPart->get_Length() / dfLen, bQuiet);

                pPtEnd = new OGRPoint();
                pPart->getPoint(pPart->getNumPoints() - 1, pPtEnd);
                dfPtEndPosition = dfPosition + dfLen;

                delete pTransformPart;
            }
            else
            {
                CURVE_DATA data = {pPart, dfPosition, dfPosition + dfLen, 1.0};
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition,
                //            1.0, bQuiet);
                pPtEnd = new OGRPoint();
                pPart->getPoint(pPart->getNumPoints() - 1, pPtEnd);
                dfPtEndPosition = dfPosition + dfLen;
            }
        }
    }

    // Create pickets
    if (!bQuiet)
    {
        fprintf(stdout, "\nCreate pickets.\n");
    }

    const double dfRoundBeg = pPtBeg != nullptr
                                  ? ceil(dfPtBegPosition / dfStep) * dfStep
                                  : ceil(dfBeginPosition / dfStep) * dfStep;

    if (pPtEnd != nullptr)
        dfEndPosition = dfPtEndPosition;

    dfFactor = dfStep / (dfEndPosition - dfRoundBeg);
    nCount = 0;
    for (std::map<double, OGRPoint *>::iterator oIter = moRepers.begin();
         oIter != moRepers.end(); ++oIter)
    {
        delete oIter->second;
    }

    moRepers.clear();

    if (pPtBeg != nullptr)
        moRepers[dfPtBegPosition] = pPtBeg;
    if (pPtEnd != nullptr)
        moRepers[dfPtEndPosition] = pPtEnd;

    for (double dfDist = dfRoundBeg; dfDist <= dfEndPosition; dfDist += dfStep)
    {
        if (bDisplayProgress)
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        for (int j = 0; j < static_cast<int>(astSubLines.size()); j++)
        {
            if (astSubLines[j].IsInside(dfDist))
            {
                const double dfRealDist =
                    (dfDist - astSubLines[j].dfBeg) * astSubLines[j].dfFactor;
                OGRPoint *pReperPoint = new OGRPoint();
                astSubLines[j].pPart->Value(dfRealDist, pReperPoint);

                moRepers[dfDist] = pReperPoint;
                break;
            }
        }
    }

    for (int i = 0; i < static_cast<int>(astSubLines.size()); i++)
    {
        delete astSubLines[i].pPart;
    }
    astSubLines.clear();

    if (!bQuiet)
    {
        fprintf(stdout, "\nCreate sublines.\n");
    }

    IT = moRepers.begin();
    dfFactor = 1.0 / moRepers.size();
    nCount = 0;
    dfDistance1 = 0;
    dfPosition = IT->first;

    while (IT != moRepers.end())
    {
        if (bDisplayProgress)
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        dfDistance2 = Project(pPathGeom, IT->second);

        if (dfDistance2 - dfDistance1 > DELTA)
        {
            pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
            if (nullptr != pPart)
            {
                AddFeature(poOutLayer, pPart, dfPosition, IT->first,
                           pPart->get_Length() / (IT->first - dfPosition),
                           bQuiet, pszOutputSepFieldName,
                           pszOutputSepFieldValue);
                dfDistance1 = dfDistance2;
                dfPosition = IT->first;
            }
        }

        ++IT;
    }

    for (std::map<double, OGRPoint *>::iterator oIter = moRepers.begin();
         oIter != moRepers.end(); ++oIter)
    {
        delete oIter->second;
    }

    if (!bQuiet)
    {
        fprintf(stdout, "\nSuccess!\n\n");
    }

    if (nullptr != pProgressArg)
    {
        GDALDestroyScaledProgress(pProgressArg);
    }

    return OGRERR_NONE;
}
#endif

//------------------------------------------------------------------------
// CreateParts
//------------------------------------------------------------------------
#ifdef HAVE_GEOS
static OGRErr CreateParts(OGRLayer *const poLnLayer, OGRLayer *const poPkLayer,
                          int nMValField, double dfStep,
                          OGRLayer *const poOutLayer, int bDisplayProgress,
                          bool bQuiet,
                          const char *pszOutputSepFieldName = nullptr,
                          const char *pszOutputSepFieldValue = nullptr)
{
    OGRErr eRetCode = OGRERR_FAILURE;

    // check path and get first line
    OGRwkbGeometryType eGeomType = poLnLayer->GetGeomType();
    if (wkbFlatten(eGeomType) != wkbLineString &&
        wkbFlatten(eGeomType) != wkbMultiLineString)
    {
        fprintf(stderr, _("Unsupported geometry type %s for path.\n"),
                OGRGeometryTypeToName(eGeomType));
        return eRetCode;
    }

    poLnLayer->ResetReading();
    // Get first geometry
    // TODO: Attribute filter for path geometry.
    OGRFeature *pPathFeature = poLnLayer->GetNextFeature();
    if (nullptr != pPathFeature)
    {
        OGRGeometry *pGeom = pPathFeature->GetGeometryRef();

        if (pGeom != nullptr &&
            wkbFlatten(pGeom->getGeometryType()) == wkbMultiLineString)
        {
            if (!bQuiet)
            {
                fprintf(stdout,
                        _("\nThe geometry " CPL_FRMT_GIB
                          " is wkbMultiLineString type.\n"),
                        pPathFeature->GetFID());
            }

            OGRGeometryCollection *pGeomColl = pGeom->toGeometryCollection();
            for (int i = 0; i < pGeomColl->getNumGeometries(); ++i)
            {
                OGRLineString *pPath =
                    pGeomColl->getGeometryRef(i)->clone()->toLineString();
                pPath->assignSpatialReference(pGeomColl->getSpatialReference());
                eRetCode = CreatePartsFromLineString(
                    pPath, poPkLayer, nMValField, dfStep, poOutLayer,
                    bDisplayProgress, bQuiet, pszOutputSepFieldName,
                    pszOutputSepFieldValue);

                if (eRetCode != OGRERR_NONE)
                {
                    OGRFeature::DestroyFeature(pPathFeature);
                    return eRetCode;
                }
            }
        }
        else if (pGeom != nullptr &&
                 wkbFlatten(pGeom->getGeometryType()) == wkbLineString)
        {
            OGRLineString *pGeomClone = pGeom->clone()->toLineString();
            eRetCode = CreatePartsFromLineString(
                pGeomClone, poPkLayer, nMValField, dfStep, poOutLayer,
                bDisplayProgress, bQuiet, pszOutputSepFieldName,
                pszOutputSepFieldValue);
            delete pGeomClone;
        }

        OGRFeature::DestroyFeature(pPathFeature);
    }

    // Should never reach

    return eRetCode;
}
#endif

//------------------------------------------------------------------------
// CreatePartsMultiple
//------------------------------------------------------------------------
#ifdef HAVE_GEOS
static OGRErr CreatePartsMultiple(
    OGRLayer *const poLnLayer, const char *pszLineSepFieldName,
    OGRLayer *const poPkLayer, const char *pszPicketsSepFieldName,
    int nMValField, double dfStep, OGRLayer *const poOutLayer,
    const char *pszOutputSepFieldName, int bDisplayProgress, bool bQuiet)
{
    // Read all separate field values into array
    OGRFeatureDefn *pDefn = poLnLayer->GetLayerDefn();
    const int nLineSepFieldInd = pDefn->GetFieldIndex(pszLineSepFieldName);
    if (nLineSepFieldInd == -1)
    {
        fprintf(stderr, _("The field was %s not found.\n"),
                pszLineSepFieldName);
        return OGRERR_FAILURE;
    }

    poLnLayer->ResetReading();

    std::set<CPLString> asIDs;
    OGRFeature *pFeature = nullptr;
    while ((pFeature = poLnLayer->GetNextFeature()) != nullptr)
    {
        CPLString sID = pFeature->GetFieldAsString(nLineSepFieldInd);
        asIDs.insert(sID);

        OGRFeature::DestroyFeature(pFeature);
    }

    for (std::set<CPLString>::const_iterator it = asIDs.begin();
         it != asIDs.end(); ++it)
    {
        // Create select clause
        CPLString sLineWhere;
        sLineWhere.Printf("%s = '%s'", pszLineSepFieldName, it->c_str());
        poLnLayer->SetAttributeFilter(sLineWhere);

        CPLString sPkWhere;
        sPkWhere.Printf("%s = '%s'", pszPicketsSepFieldName, it->c_str());
        poPkLayer->SetAttributeFilter(sPkWhere);

        if (!bQuiet)
        {
            fprintf(stdout, "The %s %s\n", pszPicketsSepFieldName, it->c_str());
        }

        // Don't check success as we want to try all paths
        CreateParts(poLnLayer, poPkLayer, nMValField, dfStep, poOutLayer,
                    bDisplayProgress, bQuiet, pszOutputSepFieldName, *it);
    }

    return OGRERR_NONE;
}
#endif

//------------------------------------------------------------------------
// GetPosition
//------------------------------------------------------------------------
#ifdef HAVE_GEOS
static OGRErr GetPosition(OGRLayer *const poPkLayer, double dfX, double dfY,
                          int /* bDisplayProgress */, int bQuiet)
{
    // Create point
    OGRPoint pt;
    pt.setX(dfX);
    pt.setY(dfY);
    pt.assignSpatialReference(poPkLayer->GetSpatialRef());

    poPkLayer->ResetReading();
    OGRLineString *pCloserPart = nullptr;
    double dfBeg = 0.0;
    double dfScale = 0.0;
    double dfMinDistance = std::numeric_limits<double>::max();
    OGRFeature *pFeature = nullptr;
    while ((pFeature = poPkLayer->GetNextFeature()) != nullptr)
    {
        OGRGeometry *pCurrentGeom = pFeature->GetGeometryRef();
        if (pCurrentGeom != nullptr)
        {
            double dfCurrentDistance = pCurrentGeom->Distance(&pt);
            if (dfCurrentDistance < dfMinDistance)
            {
                dfMinDistance = dfCurrentDistance;
                if (pCloserPart != nullptr)
                    delete pCloserPart;
                pCloserPart = pFeature->StealGeometry()->toLineString();
                dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
                dfScale = pFeature->GetFieldAsDouble(FIELD_SCALE_FACTOR);
            }
        }
        OGRFeature::DestroyFeature(pFeature);
    }

    if (nullptr == pCloserPart)
    {
        fprintf(stderr, _("Failed to find closest part.\n"));
        return OGRERR_FAILURE;
    }
    // Now we have closest part
    // Get real distance
    const double dfRealDist = Project(pCloserPart, &pt);
    delete pCloserPart;
    if (dfScale == 0)
    {
        fprintf(stderr, _("dfScale == 0.\n"));
        return OGRERR_FAILURE;
    }
    // Compute reference distance
    // coverity[divide_by_zero]
    const double dfRefDist = dfBeg + dfRealDist / dfScale;
    if (bQuiet)
    {
        fprintf(stdout, "%s", CPLSPrintf("%f\n", dfRefDist));
    }
    else
    {
        fprintf(stdout, "%s",
                CPLSPrintf(
                    _("The position for coordinates lat:%f, long:%f is %f\n"),
                    dfY, dfX, dfRefDist));
    }

    return OGRERR_NONE;
}
#endif

//------------------------------------------------------------------------
// GetCoordinates
//------------------------------------------------------------------------
static OGRErr GetCoordinates(OGRLayer *const poPkLayer, double dfPos,
                             /* CPL_UNUSED */ int /* bDisplayProgress */,
                             bool bQuiet)
{
    CPLString szAttributeFilter;
    szAttributeFilter.Printf("%s < %f AND %s > %f", FIELD_START, dfPos,
                             FIELD_FINISH, dfPos);
    // TODO: ExecuteSQL should be faster.
    poPkLayer->SetAttributeFilter(szAttributeFilter);
    poPkLayer->ResetReading();

    bool bHaveCoords = false;
    for (auto &pFeature : poPkLayer)
    {
        bHaveCoords = true;
        const double dfStart = pFeature->GetFieldAsDouble(FIELD_START);
        double dfPosCorr = dfPos - dfStart;
        const double dfSF = pFeature->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosCorr *= dfSF;
        OGRLineString *pLine = pFeature->GetGeometryRef()->toLineString();

        OGRPoint pt;
        pLine->Value(dfPosCorr, &pt);

        if (bQuiet)
        {
            fprintf(stdout, "%s",
                    CPLSPrintf("%f,%f,%f\n", pt.getX(), pt.getY(), pt.getZ()));
        }
        else
        {
            fprintf(stdout, "%s",
                    CPLSPrintf(_("The position for distance %f is lat:%f, "
                                 "long:%f, height:%f\n"),
                               dfPos, pt.getY(), pt.getX(), pt.getZ()));
        }
    }

    if (bHaveCoords)
    {
        return OGRERR_NONE;
    }
    else
    {
        fprintf(stderr, _("Get coordinates for position %f failed.\n"), dfPos);
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                           OGRLineRefOptions                          */
/************************************************************************/

struct OGRLineRefOptions
{
    bool bQuiet = false;
    bool bDisplayProgress = false;
    std::string osFormat;

    std::string osSrcLineDataSourceName;
    std::string osSrcLineLayerName;
#ifdef HAVE_GEOS
    std::string osSrcLineSepFieldName;
#endif

    std::string osSrcPicketsDataSourceName;
#ifdef HAVE_GEOS
    std::string osSrcPicketsLayerName;
    std::string osSrcPicketsSepFieldName;
    std::string osSrcPicketsMFieldName;
#endif

    std::string osSrcPartsDataSourceName;
    std::string osSrcPartsLayerName;

#ifdef HAVE_GEOS
    std::string osOutputSepFieldName = "uniq_uid";
#endif
    std::string osOutputDataSourceName;
    std::string osOutputLayerName;

    CPLStringList aosDSCO;
    CPLStringList aosLCO;

    // Operations
    bool bCreate = false;
    bool bGetPos = false;
    bool bGetSubLine = false;
    bool bGetCoord = false;

#ifdef HAVE_GEOS
    double dfXPos = std::numeric_limits<double>::quiet_NaN();
    double dfYPos = std::numeric_limits<double>::quiet_NaN();
    double dfStep = std::numeric_limits<double>::quiet_NaN();
#endif
    double dfPosBeg = std::numeric_limits<double>::quiet_NaN();
    double dfPosEnd = std::numeric_limits<double>::quiet_NaN();
    double dfPos = std::numeric_limits<double>::quiet_NaN();
};

/************************************************************************/
/*                           OGRLineRefGetParser                        */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
OGRLineRefAppOptionsGetParser(OGRLineRefOptions *psOptions)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "ogrlineref", /* bForBinary */ true);

    argParser->add_description(
        _("Create linear reference and provide some calculations using it."));

    argParser->add_epilog(_("For more details, consult the full documentation "
                            "for the ogrlineref utility "
                            "https://gdal.org/programs/ogrlineref.html"));

    auto &quietArg{argParser->add_quiet_argument(&psOptions->bQuiet)};
    argParser->add_hidden_alias_for(quietArg, "-quiet");

    argParser->add_argument("-progress")
        .flag()
        .store_into(psOptions->bDisplayProgress)
        .help(_("Display progress."));

    argParser->add_output_format_argument(psOptions->osFormat);

    argParser->add_dataset_creation_options_argument(psOptions->aosDSCO);

    argParser->add_layer_creation_options_argument(psOptions->aosLCO);

#ifdef HAVE_GEOS
    argParser->add_argument("-create")
        .flag()
        .store_into(psOptions->bCreate)
        .help(_("Create the linear reference file (linestring of parts)."));
#endif

    argParser->add_argument("-l")
        .metavar("<src_line_datasource_name>")
        .store_into(psOptions->osSrcLineDataSourceName)
        .help(_("Name of the line path datasource."));

    argParser->add_argument("-ln")
        .metavar("<layer_name>")
        .store_into(psOptions->osSrcLineLayerName)
        .help(_("Layer name in the line path datasource."));

#ifdef HAVE_GEOS

    argParser->add_argument("-lf")
        .metavar("<field_name>")
        .store_into(psOptions->osSrcLineSepFieldName)
        .help(_("Field name for unique paths in layer."));
#endif

    argParser->add_argument("-p")
        .metavar("<src_repers_datasource_name>")
        .store_into(psOptions->osSrcPicketsDataSourceName)
        .help(_("Datasource of repers name."));

#ifdef HAVE_GEOS
    argParser->add_argument("-pn")
        .metavar("<layer_name>")
        .store_into(psOptions->osSrcPicketsLayerName)
        .help(_("Layer name in repers datasource."));

    argParser->add_argument("-pm")
        .metavar("<pos_field_name>")
        .store_into(psOptions->osSrcPicketsMFieldName)
        .help(_("Line position field name."));

    argParser->add_argument("-pf")
        .metavar("<field_name>")
        .store_into(psOptions->osSrcPicketsSepFieldName)
        .help(_("Field name of unique values to map input reference points "
                "to lines."));
#endif

    argParser->add_argument("-r")
        .metavar("<src_parts_datasource_name>")
        .store_into(psOptions->osSrcPartsDataSourceName)
        .help(_("Path to linear reference file."));

    argParser->add_argument("-rn")
        .metavar("<layer_name>")
        .store_into(psOptions->osSrcPartsLayerName)
        .help(_("Name of the layer in the input linear reference datasource."));

    argParser->add_argument("-o")
        .metavar("<dst_datasource_name>")
        .store_into(psOptions->osOutputDataSourceName)
        .help(_("Path to output linear reference file (linestring "
                "datasource)."));

    argParser->add_argument("-on")
        .metavar("<layer_name>")
        .store_into(psOptions->osOutputLayerName)
        .help(_("Name of the layer in the output linear reference "
                "datasource."));

#ifdef HAVE_GEOS
    argParser->add_argument("-of")
        .metavar("<field_name>")
        .store_into(psOptions->osOutputSepFieldName)
        .help(_(
            "Name of the field for storing the unique values of input lines."));

    argParser->add_argument("-s")
        .metavar("<step>")
        .scan<'g', double>()
        .store_into(psOptions->dfStep)
        .help(_("Part size in linear units."));

    argParser->add_argument("-get_pos")
        .flag()
        .store_into(psOptions->bGetPos)
        .help(_("Get the position for the given coordinates."));

    argParser->add_argument("-x")
        .metavar("<x>")
        .scan<'g', double>()
        .store_into(psOptions->dfXPos)
        .help(_("X coordinate."));

    argParser->add_argument("-y")
        .metavar("<y>")
        .scan<'g', double>()
        .store_into(psOptions->dfYPos)
        .help(_("Y coordinate."));
#endif

    argParser->add_argument("-get_coord")
        .flag()
        .store_into(psOptions->bGetCoord)
        .help(_("Return point on path for input linear distance."));

    argParser->add_argument("-m")
        .metavar("<position>")
        .scan<'g', double>()
        .store_into(psOptions->dfPos)
        .help(_("Input linear distance."));

    argParser->add_argument("-get_subline")
        .flag()
        .store_into(psOptions->bGetSubLine)
        .help(_("Return the portion of the input path from and to input linear "
                "positions."));

    argParser->add_argument("-mb")
        .metavar("<position>")
        .scan<'g', double>()
        .store_into(psOptions->dfPosBeg)
        .help(_("Input linear distance begin."));

    argParser->add_argument("-me")
        .metavar("<position>")
        .scan<'g', double>()
        .store_into(psOptions->dfPosEnd)
        .help(_("Input linear distance end."));

    return argParser;
}

/************************************************************************/
/*                              GetOutputDriver()                       */
/************************************************************************/

static GDALDriver *GetOutputDriver(OGRLineRefOptions &sOptions)
{
    if (sOptions.osFormat.empty())
    {
        const auto aoDrivers = GetOutputDriversFor(
            sOptions.osOutputDataSourceName.c_str(), GDAL_OF_VECTOR);
        if (aoDrivers.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot guess driver for %s",
                     sOptions.osOutputDataSourceName.c_str());
            return nullptr;
        }
        else
        {
            if (aoDrivers.size() > 1)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Several drivers matching %s extension. Using %s",
                    CPLGetExtensionSafe(sOptions.osOutputDataSourceName.c_str())
                        .c_str(),
                    aoDrivers[0].c_str());
            }
            sOptions.osFormat = aoDrivers[0];
        }
    }

    GDALDriver *poDriver =
        GetGDALDriverManager()->GetDriverByName(sOptions.osFormat.c_str());
    if (poDriver == nullptr)
    {
        fprintf(stderr, _("Unable to find driver `%s'.\n"),
                sOptions.osFormat.c_str());
        fprintf(stderr, _("The following drivers are available:\n"));

        GDALDriverManager *poDM = GetGDALDriverManager();
        for (int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++)
        {
            GDALDriver *poIter = poDM->GetDriver(iDriver);
            char **papszDriverMD = poIter->GetMetadata();
            if (CPLTestBool(CSLFetchNameValueDef(papszDriverMD,
                                                 GDAL_DCAP_VECTOR, "FALSE")) &&
                CPLTestBool(CSLFetchNameValueDef(papszDriverMD,
                                                 GDAL_DCAP_CREATE, "FALSE")))
            {
                fprintf(stderr, "  -> `%s'\n", poIter->GetDescription());
            }
        }
    }

    return poDriver;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    OGRErr eErr = OGRERR_NONE;

    operation stOper = op_unknown;

    EarlySetConfigOptions(argc, argv);

    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);

    if (argc < 1)
    {
        try
        {
            OGRLineRefOptions sOptions;
            auto argParser = OGRLineRefAppOptionsGetParser(&sOptions);
            fprintf(stderr, "%s\n", argParser->usage().c_str());
        }
        catch (const std::exception &err)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                     err.what());
        }
        exit(1);
    }

    OGRRegisterAll();

    OGRLineRefOptions psOptions;
    auto argParser = OGRLineRefAppOptionsGetParser(&psOptions);

    try
    {
        argParser->parse_args_without_binary_name(argv + 1);
        CSLDestroy(argv);
    }
    catch (const std::exception &error)
    {
        argParser->display_error_and_usage(error);
        CSLDestroy(argv);
        exit(1);
    }

    // Select operation mode
    if (psOptions.bCreate)
        stOper = op_create;

    if (psOptions.bGetPos)
    {
        if (stOper != op_unknown)
        {
            fprintf(stderr, _("Only one operation can be specified\n"));
            argParser->usage();
            exit(1);
        }
        stOper = op_get_pos;
    }

    if (psOptions.bGetCoord)
    {
        if (stOper != op_unknown)
        {
            fprintf(stderr, _("Only one operation can be specified\n"));
            argParser->usage();
            exit(1);
        }
        stOper = op_get_coord;
    }

    if (psOptions.bGetSubLine)
    {
        if (stOper != op_unknown)
        {
            fprintf(stderr, _("Only one operation can be specified\n"));
            argParser->usage();
            exit(1);
        }
        stOper = op_get_subline;
    }

    if (stOper == op_unknown)
    {
        fprintf(stderr, _("No operation specified\n"));
        argParser->usage();
        exit(1);
    }

    /* -------------------------------------------------------------------- */
    /*                     Create linear reference                          */
    /* -------------------------------------------------------------------- */

    switch (stOper)
    {
        case op_create:
        {
#ifdef HAVE_GEOS
            if (psOptions.osOutputDataSourceName.empty())
            {
                fprintf(stderr, _("No output datasource provided.\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.osSrcLineDataSourceName.empty())
            {
                fprintf(stderr, _("No path datasource provided.\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.osSrcPicketsMFieldName.empty())
            {
                fprintf(stderr, _("No repers position field provided.\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.osSrcPicketsDataSourceName.empty())
            {
                fprintf(stderr, _("No repers datasource provided.\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.dfStep == std::numeric_limits<double>::quiet_NaN())
            {
                fprintf(stderr, _("No step provided.\n"));
                argParser->usage();
                exit(1);
            }

            /* ------------------------------------------------------------- */
            /*      Open data source.                                        */
            /* ------------------------------------------------------------- */

            GDALDataset *poLnDS = GDALDataset::FromHandle(OGROpen(
                psOptions.osSrcLineDataSourceName.c_str(), FALSE, nullptr));

            /* ------------------------------------------------------------- */
            /*      Report failure                                           */
            /* ------------------------------------------------------------- */
            if (poLnDS == nullptr)
            {
                OGRSFDriverRegistrar *poR =
                    OGRSFDriverRegistrar::GetRegistrar();

                fprintf(stderr,
                        _("FAILURE:\n"
                          "Unable to open path datasource `%s' with "
                          "the following drivers.\n"),
                        psOptions.osSrcLineDataSourceName.c_str());

                for (int iDriver = 0; iDriver < poR->GetDriverCount();
                     iDriver++)
                {
                    fprintf(stderr, "  -> %s\n",
                            poR->GetDriver(iDriver)->GetDescription());
                }

                exit(1);
            }

            GDALDataset *poPkDS = GDALDataset::FromHandle(OGROpen(
                psOptions.osSrcPicketsDataSourceName.c_str(), FALSE, nullptr));

            /* --------------------------------------------------------------- */
            /*      Report failure                                             */
            /* --------------------------------------------------------------- */

            if (poPkDS == nullptr)
            {
                OGRSFDriverRegistrar *poR =
                    OGRSFDriverRegistrar::GetRegistrar();

                fprintf(stderr,
                        _("FAILURE:\n"
                          "Unable to open repers datasource `%s' "
                          "with the following drivers.\n"),
                        psOptions.osSrcPicketsDataSourceName.c_str());

                for (int iDriver = 0; iDriver < poR->GetDriverCount();
                     iDriver++)
                {
                    fprintf(stderr, "  -> %s\n",
                            poR->GetDriver(iDriver)->GetDescription());
                }

                exit(1);
            }

            /* ----------------------------------------------------------------- */
            /*      Find the output driver.                                      */
            /* ----------------------------------------------------------------- */

            GDALDriver *poDriver = GetOutputDriver(psOptions);
            if (poDriver == nullptr)
            {
                exit(1);
            }

            if (!CPLTestBool(CSLFetchNameValueDef(poDriver->GetMetadata(),
                                                  GDAL_DCAP_CREATE, "FALSE")))
            {
                fprintf(stderr,
                        _("%s driver does not support data source creation.\n"),
                        psOptions.osSrcPicketsDataSourceName.c_str());
                exit(1);
            }

            /* ---------------------------------------------------------------- */
            /*      Create the output data source.                              */
            /* ---------------------------------------------------------------- */
            GDALDataset *poODS =
                poDriver->Create(psOptions.osOutputDataSourceName.c_str(), 0, 0,
                                 0, GDT_Unknown, psOptions.aosDSCO);
            if (poODS == nullptr)
            {
                fprintf(stderr, _("%s driver failed to create %s.\n"),
                        psOptions.osFormat.c_str(),
                        psOptions.osOutputDataSourceName.c_str());
                exit(1);
            }

            OGRLayer *poLnLayer =
                psOptions.osSrcLineLayerName.empty()
                    ? poLnDS->GetLayer(0)
                    : poLnDS->GetLayerByName(
                          psOptions.osSrcLineLayerName.c_str());

            if (poLnLayer == nullptr)
            {
                fprintf(stderr, _("Get path layer failed.\n"));
                exit(1);
            }

            OGRLayer *poPkLayer =
                psOptions.osSrcPicketsLayerName.empty()
                    ? poPkDS->GetLayer(0)
                    : poPkDS->GetLayerByName(
                          psOptions.osSrcPicketsLayerName.c_str());

            if (poPkLayer == nullptr)
            {
                fprintf(stderr, _("Get repers layer failed.\n"));
                exit(1);
            }

            OGRFeatureDefn *poPkFDefn = poPkLayer->GetLayerDefn();
            int nMValField = poPkFDefn->GetFieldIndex(
                psOptions.osSrcPicketsMFieldName.c_str());

            OGRLayer *poOutLayer = nullptr;
            if (!psOptions.osSrcLineSepFieldName.empty() &&
                !psOptions.osSrcPicketsSepFieldName.empty())
            {
                poOutLayer =
                    SetupTargetLayer(poLnLayer, poODS, psOptions.aosLCO,
                                     psOptions.osOutputLayerName.c_str(),
                                     psOptions.osOutputSepFieldName.c_str());
                if (poOutLayer == nullptr)
                {
                    fprintf(stderr, _("Create output layer failed.\n"));
                    exit(1);
                }

                // Do the work
                eErr = CreatePartsMultiple(
                    poLnLayer, psOptions.osSrcLineSepFieldName.c_str(),
                    poPkLayer, psOptions.osSrcPicketsSepFieldName.c_str(),
                    nMValField, psOptions.dfStep, poOutLayer,
                    psOptions.osOutputSepFieldName.c_str(),
                    psOptions.bDisplayProgress, psOptions.bQuiet);
            }
            else
            {
                poOutLayer =
                    SetupTargetLayer(poLnLayer, poODS, psOptions.aosLCO,
                                     psOptions.osOutputLayerName.c_str());
                if (poOutLayer == nullptr)
                {
                    fprintf(stderr, _("Create output layer failed.\n"));
                    exit(1);
                }

                // Do the work
                eErr = CreateParts(
                    poLnLayer, poPkLayer, nMValField, psOptions.dfStep,
                    poOutLayer, psOptions.bDisplayProgress, psOptions.bQuiet);
            }

            GDALClose(poLnDS);
            GDALClose(poPkDS);
            if (GDALClose(poODS) != CE_None)
                eErr = CE_Failure;

#else   // HAVE_GEOS
            fprintf(stderr,
                    _("GEOS support not enabled or incompatible version.\n"));
            exit(1);
#endif  // HAVE_GEOS
            break;
        }
        case op_get_pos:
        {
#ifdef HAVE_GEOS

            if (psOptions.dfXPos == std::numeric_limits<double>::quiet_NaN() ||
                psOptions.dfYPos == std::numeric_limits<double>::quiet_NaN())
            {
                fprintf(stderr, _("no coordinates provided\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.osSrcPartsDataSourceName.empty())
            {
                fprintf(stderr, _("no parts datasource provided\n"));
                argParser->usage();
                exit(1);
            }

            GDALDataset *poPartsDS = GDALDataset::FromHandle(OGROpen(
                psOptions.osSrcPartsDataSourceName.c_str(), FALSE, nullptr));
            /* ------------------------------------------------------------------ */
            /*      Report failure                                                */
            /* ------------------------------------------------------------------ */
            if (poPartsDS == nullptr)
            {
                OGRSFDriverRegistrar *poR =
                    OGRSFDriverRegistrar::GetRegistrar();

                fprintf(stderr,
                        _("FAILURE:\n"
                          "Unable to open parts datasource `%s' with "
                          "the following drivers.\n"),
                        psOptions.osSrcPicketsDataSourceName.c_str());

                for (int iDriver = 0; iDriver < poR->GetDriverCount();
                     iDriver++)
                {
                    fprintf(stderr, "  -> %s\n",
                            poR->GetDriver(iDriver)->GetDescription());
                }

                exit(1);
            }

            OGRLayer *poPartsLayer =
                psOptions.osSrcPartsLayerName.empty()
                    ? poPartsDS->GetLayer(0)
                    : poPartsDS->GetLayerByName(
                          psOptions.osSrcPartsLayerName.c_str());

            if (poPartsLayer == nullptr)
            {
                fprintf(stderr, _("Get parts layer failed.\n"));
                exit(1);
            }

            // Do the work
            eErr = GetPosition(poPartsLayer, psOptions.dfXPos, psOptions.dfYPos,
                               psOptions.bDisplayProgress, psOptions.bQuiet);

            GDALClose(poPartsDS);

#else   // HAVE_GEOS
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif  // HAVE_GEOS
            break;
        }
        case op_get_coord:
        {
            if (psOptions.osSrcPartsDataSourceName.empty())
            {
                fprintf(stderr, _("No parts datasource provided.\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.dfPos == std::numeric_limits<double>::quiet_NaN())
            {
                fprintf(stderr, _("No position provided.\n"));
                argParser->usage();
                exit(1);
            }

            GDALDataset *poPartsDS = GDALDataset::FromHandle(OGROpen(
                psOptions.osSrcPartsDataSourceName.c_str(), FALSE, nullptr));
            /* ----------------------------------------------------------------- */
            /*      Report failure                                               */
            /* ----------------------------------------------------------------- */
            if (poPartsDS == nullptr)
            {
                OGRSFDriverRegistrar *poR =
                    OGRSFDriverRegistrar::GetRegistrar();

                fprintf(stderr,
                        _("FAILURE:\n"
                          "Unable to open parts datasource `%s' with "
                          "the following drivers.\n"),
                        psOptions.osSrcPicketsDataSourceName.c_str());

                for (int iDriver = 0; iDriver < poR->GetDriverCount();
                     iDriver++)
                {
                    fprintf(stderr, "  -> %s\n",
                            poR->GetDriver(iDriver)->GetDescription());
                }

                exit(1);
            }

            OGRLayer *poPartsLayer =
                psOptions.osSrcPartsLayerName.empty()
                    ? poPartsDS->GetLayer(0)
                    : poPartsDS->GetLayerByName(
                          psOptions.osSrcPartsLayerName.c_str());

            if (poPartsLayer == nullptr)
            {
                fprintf(stderr, _("Get parts layer failed.\n"));
                exit(1);
            }
            // Do the work
            eErr = GetCoordinates(poPartsLayer, psOptions.dfPos,
                                  psOptions.bDisplayProgress, psOptions.bQuiet);

            GDALClose(poPartsDS);

            break;
        }
        case op_get_subline:
        {
            if (psOptions.dfPosBeg == std::numeric_limits<double>::quiet_NaN())
            {
                fprintf(stderr, _("No begin position provided.\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.dfPosEnd == std::numeric_limits<double>::quiet_NaN())
            {
                fprintf(stderr, _("No end position provided.\n"));
                argParser->usage();
                exit(1);
            }
            if (psOptions.osSrcPartsDataSourceName.empty())
            {
                fprintf(stderr, _("No parts datasource provided.\n"));
                argParser->usage();
                exit(1);
            }

            GDALDataset *poPartsDS = GDALDataset::FromHandle(OGROpen(
                psOptions.osSrcPartsDataSourceName.c_str(), FALSE, nullptr));

            // Report failure.
            if (poPartsDS == nullptr)
            {
                OGRSFDriverRegistrar *poR =
                    OGRSFDriverRegistrar::GetRegistrar();

                fprintf(stderr,
                        _("FAILURE:\n"
                          "Unable to open parts datasource `%s' with "
                          "the following drivers.\n"),
                        psOptions.osSrcPicketsDataSourceName.c_str());

                for (int iDriver = 0; iDriver < poR->GetDriverCount();
                     iDriver++)
                {
                    fprintf(stderr, "  -> %s\n",
                            poR->GetDriver(iDriver)->GetDescription());
                }

                exit(1);
            }

            // Find the output driver.
            GDALDriver *poDriver = GetOutputDriver(psOptions);
            if (poDriver == nullptr)
            {
                exit(1);
            }

            if (!CPLTestBool(CSLFetchNameValueDef(poDriver->GetMetadata(),
                                                  GDAL_DCAP_CREATE, "FALSE")))
            {
                fprintf(stderr,
                        _("%s driver does not support data source creation.\n"),
                        psOptions.osFormat.c_str());
                exit(1);
            }

            // Create the output data source.

            GDALDataset *poODS =
                poDriver->Create(psOptions.osOutputDataSourceName.c_str(), 0, 0,
                                 0, GDT_Unknown, psOptions.aosDSCO);
            if (poODS == nullptr)
            {
                fprintf(stderr, _("%s driver failed to create %s\n"),
                        psOptions.osFormat.c_str(),
                        psOptions.osOutputDataSourceName.c_str());
                exit(1);
            }

            OGRLayer *poPartsLayer =
                psOptions.osSrcLineLayerName.empty()
                    ? poPartsDS->GetLayer(0)
                    : poPartsDS->GetLayerByName(
                          psOptions.osSrcLineLayerName.c_str());

            if (poPartsLayer == nullptr)
            {
                fprintf(stderr, _("Get parts layer failed.\n"));
                exit(1);
            }

            OGRLayer *poOutLayer =
                SetupTargetLayer(poPartsLayer, poODS, psOptions.aosLCO,
                                 psOptions.osOutputLayerName.c_str());

            if (poOutLayer == nullptr)
            {
                fprintf(stderr, _("Create output layer failed.\n"));
                exit(1);
            }

            // Do the work

            eErr = CreateSubline(poPartsLayer, psOptions.dfPosBeg,
                                 psOptions.dfPosEnd, poOutLayer,
                                 psOptions.bDisplayProgress, psOptions.bQuiet);

            GDALClose(poPartsDS);
            if (GDALClose(poODS) != CE_None)
                eErr = CE_Failure;

            break;
        }
        default:
            fprintf(stderr, _("Unknown operation.\n"));
            argParser->usage();
            exit(1);
    }

    OGRCleanupAll();

#ifdef DBMALLOC
    malloc_dump(1);
#endif

    return eErr == OGRERR_NONE ? 0 : 1;
}

MAIN_END
