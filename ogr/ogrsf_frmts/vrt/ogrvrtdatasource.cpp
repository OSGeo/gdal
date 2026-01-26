/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVRTDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_vrt.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <set>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_spatialref.h"
#include "ogrlayerpool.h"
#include "ogrunionlayer.h"
#include "ogrwarpedlayer.h"
#include "ogrsf_frmts.h"
#include "ogrvrtgeometrytypes.h"

/************************************************************************/
/*                          OGRVRTDataSource()                          */
/************************************************************************/

OGRVRTDataSource::OGRVRTDataSource(GDALDriver *poDriverIn)
{
    poDriver = poDriverIn;
}

/************************************************************************/
/*                         ~OGRVRTDataSource()                          */
/************************************************************************/

OGRVRTDataSource::~OGRVRTDataSource()

{
    OGRVRTDataSource::CloseDependentDatasets();

    CPLFree(paeLayerType);

    if (psTree != nullptr)
        CPLDestroyXMLNode(psTree);
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int OGRVRTDataSource::CloseDependentDatasets()
{
    const int bHasClosedDependentDatasets = nLayers > 0;
    for (int i = 0; i < nLayers; i++)
    {
        delete papoLayers[i];
    }
    CPLFree(papoLayers);
    nLayers = 0;
    papoLayers = nullptr;
    return bHasClosedDependentDatasets;
}

/************************************************************************/
/*                       InstantiateWarpedLayer()                       */
/************************************************************************/

OGRLayer *OGRVRTDataSource::InstantiateWarpedLayer(CPLXMLNode *psLTree,
                                                   const char *pszVRTDirectory,
                                                   int bUpdate, int nRecLevel)
{
    if (!EQUAL(psLTree->pszValue, "OGRVRTWarpedLayer"))
        return nullptr;

    std::unique_ptr<OGRLayer> poSrcLayer;

    for (CPLXMLNode *psSubNode = psLTree->psChild; psSubNode != nullptr;
         psSubNode = psSubNode->psNext)
    {
        if (psSubNode->eType != CXT_Element)
            continue;

        poSrcLayer.reset(InstantiateLayer(psSubNode, pszVRTDirectory, bUpdate,
                                          nRecLevel + 1));
        if (poSrcLayer != nullptr)
            break;
    }

    if (poSrcLayer == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot instantiate source layer");
        return nullptr;
    }

    const char *pszTargetSRS = CPLGetXMLValue(psLTree, "TargetSRS", nullptr);
    if (pszTargetSRS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing TargetSRS element within OGRVRTWarpedLayer");
        return nullptr;
    }

    const char *pszGeomFieldName =
        CPLGetXMLValue(psLTree, "WarpedGeomFieldName", nullptr);
    int iGeomField = 0;
    if (pszGeomFieldName != nullptr)
    {
        iGeomField =
            poSrcLayer->GetLayerDefn()->GetGeomFieldIndex(pszGeomFieldName);
        if (iGeomField < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find source geometry field '%s'",
                     pszGeomFieldName);
            return nullptr;
        }
    }

    std::unique_ptr<OGRSpatialReference> poSrcSRS;
    const char *pszSourceSRS = CPLGetXMLValue(psLTree, "SrcSRS", nullptr);

    if (pszSourceSRS == nullptr)
    {
        if (iGeomField < poSrcLayer->GetLayerDefn()->GetGeomFieldCount())
        {
            const auto *poSrcLayerSRS = poSrcLayer->GetLayerDefn()
                                            ->GetGeomFieldDefn(iGeomField)
                                            ->GetSpatialRef();
            if (poSrcLayerSRS)
                poSrcSRS.reset(poSrcLayerSRS->Clone());
        }
    }
    else
    {
        poSrcSRS = std::make_unique<OGRSpatialReference>();
        poSrcSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSrcSRS->SetFromUserInput(
                pszSourceSRS,
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
            OGRERR_NONE)
        {
            poSrcSRS.reset();
        }
    }

    if (poSrcSRS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to import source SRS");
        return nullptr;
    }

    auto poTargetSRS = std::make_unique<OGRSpatialReference>();
    poTargetSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (poTargetSRS->SetFromUserInput(
            pszTargetSRS,
            OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
        OGRERR_NONE)
    {
        poTargetSRS.reset();
    }

    if (poTargetSRS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to import target SRS");
        return nullptr;
    }

    if (pszSourceSRS == nullptr && poSrcSRS->IsSame(poTargetSRS.get()))
    {
        return poSrcLayer.release();
    }

    std::unique_ptr<OGRCoordinateTransformation> poCT(
        OGRCreateCoordinateTransformation(poSrcSRS.get(), poTargetSRS.get()));
    std::unique_ptr<OGRCoordinateTransformation> poReversedCT(
        poCT != nullptr ? OGRCreateCoordinateTransformation(poTargetSRS.get(),
                                                            poSrcSRS.get())
                        : nullptr);

    if (poCT == nullptr)
    {
        return nullptr;
    }

    // Build the OGRWarpedLayer.
    auto poLayer = std::make_unique<OGRWarpedLayer>(
        poSrcLayer.release(), iGeomField, TRUE, std::move(poCT),
        std::move(poReversedCT));

    // Set Extent if provided.
    const char *pszExtentXMin = CPLGetXMLValue(psLTree, "ExtentXMin", nullptr);
    const char *pszExtentYMin = CPLGetXMLValue(psLTree, "ExtentYMin", nullptr);
    const char *pszExtentXMax = CPLGetXMLValue(psLTree, "ExtentXMax", nullptr);
    const char *pszExtentYMax = CPLGetXMLValue(psLTree, "ExtentYMax", nullptr);
    if (pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
        pszExtentXMax != nullptr && pszExtentYMax != nullptr)
    {
        poLayer->SetExtent(CPLAtof(pszExtentXMin), CPLAtof(pszExtentYMin),
                           CPLAtof(pszExtentXMax), CPLAtof(pszExtentYMax));
    }

    return poLayer.release();
}

/************************************************************************/
/*                       InstantiateUnionLayer()                        */
/************************************************************************/

OGRLayer *OGRVRTDataSource::InstantiateUnionLayer(CPLXMLNode *psLTree,
                                                  const char *pszVRTDirectory,
                                                  int bUpdate, int nRecLevel)
{
    if (!EQUAL(psLTree->pszValue, "OGRVRTUnionLayer"))
        return nullptr;

    // Get layer name.
    const char *pszLayerName = CPLGetXMLValue(psLTree, "name", nullptr);

    if (pszLayerName == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing name attribute on OGRVRTUnionLayer");
        return nullptr;
    }

    // Do we have a fixed geometry type?  If not derive from the
    // source layer.
    const char *pszGType = CPLGetXMLValue(psLTree, "GeometryType", nullptr);
    bool bGlobalGeomTypeSet = false;
    OGRwkbGeometryType eGlobalGeomType = wkbUnknown;
    if (pszGType != nullptr)
    {
        bGlobalGeomTypeSet = true;
        int bError = FALSE;
        eGlobalGeomType = OGRVRTGetGeometryType(pszGType, &bError);
        if (bError)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GeometryType %s not recognised.", pszGType);
            return nullptr;
        }
    }

    // Apply a spatial reference system if provided.
    const char *pszLayerSRS = CPLGetXMLValue(psLTree, "LayerSRS", nullptr);
    OGRSpatialReference *poGlobalSRS = nullptr;
    bool bGlobalSRSSet = false;
    if (pszLayerSRS != nullptr)
    {
        bGlobalSRSSet = true;
        if (!EQUAL(pszLayerSRS, "NULL"))
        {
            OGRSpatialReference oSRS;
            oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            if (oSRS.SetFromUserInput(
                    pszLayerSRS,
                    OGRSpatialReference::
                        SET_FROM_USER_INPUT_LIMITATIONS_get()) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to import LayerSRS `%s'.", pszLayerSRS);
                return nullptr;
            }
            poGlobalSRS = oSRS.Clone();
        }
    }

    // Find field declarations.
    std::vector<OGRFieldDefn> aoFields;
    std::vector<OGRUnionLayerGeomFieldDefn> aoGeomFields;

    for (CPLXMLNode *psSubNode = psLTree->psChild; psSubNode != nullptr;
         psSubNode = psSubNode->psNext)
    {
        if (psSubNode->eType != CXT_Element)
            continue;

        if (EQUAL(psSubNode->pszValue, "Field"))
        {
            // Field name.
            const char *l_pszName = CPLGetXMLValue(psSubNode, "name", nullptr);
            if (l_pszName == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to identify Field name.");
                break;
            }

            OGRFieldDefn oFieldDefn(l_pszName, OFTString);

            // Type.
            const char *pszArg = CPLGetXMLValue(psSubNode, "type", nullptr);

            if (pszArg != nullptr)
            {
                int iType = 0;  // Used after for.

                for (; iType <= static_cast<int>(OFTMaxType); iType++)
                {
                    if (EQUAL(pszArg, OGRFieldDefn::GetFieldTypeName(
                                          static_cast<OGRFieldType>(iType))))
                    {
                        oFieldDefn.SetType(static_cast<OGRFieldType>(iType));
                        break;
                    }
                }

                if (iType > static_cast<int>(OFTMaxType))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to identify Field type '%s'.", pszArg);
                    break;
                }
            }

            // Width and precision.
            const int nWidth = atoi(CPLGetXMLValue(psSubNode, "width", "0"));
            if (nWidth < 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid width for field %s.", l_pszName);
                break;
            }
            oFieldDefn.SetWidth(nWidth);

            const int nPrecision =
                atoi(CPLGetXMLValue(psSubNode, "precision", "0"));
            if (nPrecision < 0 || nPrecision > 1024)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid precision for field %s.", l_pszName);
                break;
            }
            oFieldDefn.SetPrecision(nPrecision);

            aoFields.emplace_back(&oFieldDefn);
        }
        else if (EQUAL(psSubNode->pszValue, "GeometryField"))
        {
            const char *l_pszName = CPLGetXMLValue(psSubNode, "name", nullptr);
            if (l_pszName == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to identify GeometryField name.");
                break;
            }

            pszGType = CPLGetXMLValue(psSubNode, "GeometryType", nullptr);
            if (pszGType == nullptr && aoGeomFields.empty())
                pszGType = CPLGetXMLValue(psLTree, "GeometryType", nullptr);
            OGRwkbGeometryType eGeomType = wkbUnknown;
            bool bGeomTypeSet = false;
            if (pszGType != nullptr)
            {
                int bError = FALSE;
                eGeomType = OGRVRTGetGeometryType(pszGType, &bError);
                bGeomTypeSet = true;
                if (bError || eGeomType == wkbNone)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "GeometryType %s not recognised.", pszGType);
                    break;
                }
            }

            const char *pszSRS = CPLGetXMLValue(psSubNode, "SRS", nullptr);
            if (pszSRS == nullptr && aoGeomFields.empty())
                pszSRS = CPLGetXMLValue(psLTree, "LayerSRS", nullptr);
            OGRSpatialReference *poSRS = nullptr;
            bool bSRSSet = false;
            if (pszSRS != nullptr)
            {
                bSRSSet = true;
                if (!EQUAL(pszSRS, "NULL"))
                {
                    OGRSpatialReference oSRS;
                    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                    if (oSRS.SetFromUserInput(
                            pszSRS,
                            OGRSpatialReference::
                                SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
                        OGRERR_NONE)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to import SRS `%s'.", pszSRS);
                        break;
                    }
                    poSRS = oSRS.Clone();
                }
            }

            aoGeomFields.emplace_back(l_pszName, eGeomType);
            OGRUnionLayerGeomFieldDefn &oFieldDefn = aoGeomFields.back();
            if (poSRS != nullptr)
            {
                oFieldDefn.SetSpatialRef(poSRS);
                poSRS->Dereference();
            }
            oFieldDefn.bGeomTypeSet = bGeomTypeSet;
            oFieldDefn.bSRSSet = bSRSSet;

            const char *pszExtentXMin =
                CPLGetXMLValue(psSubNode, "ExtentXMin", nullptr);
            const char *pszExtentYMin =
                CPLGetXMLValue(psSubNode, "ExtentYMin", nullptr);
            const char *pszExtentXMax =
                CPLGetXMLValue(psSubNode, "ExtentXMax", nullptr);
            const char *pszExtentYMax =
                CPLGetXMLValue(psSubNode, "ExtentYMax", nullptr);
            if (pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
                pszExtentXMax != nullptr && pszExtentYMax != nullptr)
            {
                oFieldDefn.sStaticEnvelope.MinX = CPLAtof(pszExtentXMin);
                oFieldDefn.sStaticEnvelope.MinY = CPLAtof(pszExtentYMin);
                oFieldDefn.sStaticEnvelope.MaxX = CPLAtof(pszExtentXMax);
                oFieldDefn.sStaticEnvelope.MaxY = CPLAtof(pszExtentYMax);
            }
        }
    }

    // Set Extent if provided.
    const char *pszExtentXMin = CPLGetXMLValue(psLTree, "ExtentXMin", nullptr);
    const char *pszExtentYMin = CPLGetXMLValue(psLTree, "ExtentYMin", nullptr);
    const char *pszExtentXMax = CPLGetXMLValue(psLTree, "ExtentXMax", nullptr);
    const char *pszExtentYMax = CPLGetXMLValue(psLTree, "ExtentYMax", nullptr);

    if (eGlobalGeomType != wkbNone && aoGeomFields.empty() &&
        (bGlobalGeomTypeSet || bGlobalSRSSet ||
         (pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
          pszExtentXMax != nullptr && pszExtentYMax != nullptr)))
    {
        aoGeomFields.emplace_back("", eGlobalGeomType);
        auto &oFieldDefn = aoGeomFields.back();
        if (poGlobalSRS != nullptr)
        {
            oFieldDefn.SetSpatialRef(poGlobalSRS);
            poGlobalSRS->Dereference();
            poGlobalSRS = nullptr;
        }
        oFieldDefn.bGeomTypeSet = bGlobalGeomTypeSet;
        oFieldDefn.bSRSSet = bGlobalSRSSet;
        if (pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
            pszExtentXMax != nullptr && pszExtentYMax != nullptr)
        {
            oFieldDefn.sStaticEnvelope.MinX = CPLAtof(pszExtentXMin);
            oFieldDefn.sStaticEnvelope.MinY = CPLAtof(pszExtentYMin);
            oFieldDefn.sStaticEnvelope.MaxX = CPLAtof(pszExtentXMax);
            oFieldDefn.sStaticEnvelope.MaxY = CPLAtof(pszExtentYMax);
        }
    }
    else
    {
        delete poGlobalSRS;
        poGlobalSRS = nullptr;
    }

    // Find source layers.
    int nSrcLayers = 0;
    OGRLayer **papoSrcLayers = nullptr;

    for (CPLXMLNode *psSubNode = psLTree->psChild; psSubNode != nullptr;
         psSubNode = psSubNode->psNext)
    {
        if (psSubNode->eType != CXT_Element)
            continue;

        OGRLayer *poSrcLayer = InstantiateLayer(psSubNode, pszVRTDirectory,
                                                bUpdate, nRecLevel + 1);
        if (poSrcLayer != nullptr)
        {
            papoSrcLayers = static_cast<OGRLayer **>(CPLRealloc(
                papoSrcLayers, sizeof(OGRLayer *) * (nSrcLayers + 1)));
            papoSrcLayers[nSrcLayers] = poSrcLayer;
            nSrcLayers++;
        }
    }

    if (nSrcLayers == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find source layers");
        return nullptr;
    }

    // Build the OGRUnionLayer.
    OGRUnionLayer *poLayer =
        new OGRUnionLayer(pszLayerName, nSrcLayers, papoSrcLayers, TRUE);

    // Set the source layer field name attribute.
    const char *pszSourceLayerFieldName =
        CPLGetXMLValue(psLTree, "SourceLayerFieldName", nullptr);
    poLayer->SetSourceLayerFieldName(pszSourceLayerFieldName);

    // Set the PreserveSrcFID attribute.
    bool bPreserveSrcFID = false;
    const char *pszPreserveFID =
        CPLGetXMLValue(psLTree, "PreserveSrcFID", nullptr);
    if (pszPreserveFID != nullptr)
        bPreserveSrcFID = CPLTestBool(pszPreserveFID);
    poLayer->SetPreserveSrcFID(bPreserveSrcFID);

    // Set fields.
    FieldUnionStrategy eFieldStrategy = FIELD_UNION_ALL_LAYERS;
    const char *pszFieldStrategy =
        CPLGetXMLValue(psLTree, "FieldStrategy", nullptr);
    if (pszFieldStrategy != nullptr)
    {
        if (EQUAL(pszFieldStrategy, "FirstLayer"))
            eFieldStrategy = FIELD_FROM_FIRST_LAYER;
        else if (EQUAL(pszFieldStrategy, "Union"))
            eFieldStrategy = FIELD_UNION_ALL_LAYERS;
        else if (EQUAL(pszFieldStrategy, "Intersection"))
            eFieldStrategy = FIELD_INTERSECTION_ALL_LAYERS;
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unhandled value for FieldStrategy `%s'.",
                     pszFieldStrategy);
        }
    }
    if (!aoFields.empty() || aoGeomFields.size() > 1)
    {
        if (pszFieldStrategy != nullptr)
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ignoring FieldStrategy value, "
                     "because explicit Field or GeometryField is provided");
        eFieldStrategy = FIELD_SPECIFIED;
    }

    poLayer->SetFields(eFieldStrategy, static_cast<int>(aoFields.size()),
                       aoFields.data(),
                       (aoGeomFields.empty() && eGlobalGeomType == wkbNone)
                           ? -1
                           : static_cast<int>(aoGeomFields.size()),
                       aoGeomFields.data());

    // Set FeatureCount if provided.
    const char *pszFeatureCount =
        CPLGetXMLValue(psLTree, "FeatureCount", nullptr);
    if (pszFeatureCount != nullptr)
    {
        poLayer->SetFeatureCount(atoi(pszFeatureCount));
    }

    return poLayer;
}

/************************************************************************/
/*                      InstantiateLayerInternal()                      */
/************************************************************************/

OGRLayer *
OGRVRTDataSource::InstantiateLayerInternal(CPLXMLNode *psLTree,
                                           const char *pszVRTDirectory,
                                           int bUpdate, int nRecLevel)
{
    // Create the layer object.
    if (EQUAL(psLTree->pszValue, "OGRVRTLayer"))
    {
        OGRVRTLayer *poVRTLayer = new OGRVRTLayer(this);

        if (!poVRTLayer->FastInitialize(psLTree, pszVRTDirectory, bUpdate))
        {
            delete poVRTLayer;
            return nullptr;
        }

        return poVRTLayer;
    }
    else if (EQUAL(psLTree->pszValue, "OGRVRTWarpedLayer") && nRecLevel < 30)
    {
        return InstantiateWarpedLayer(psLTree, pszVRTDirectory, bUpdate,
                                      nRecLevel + 1);
    }
    else if (EQUAL(psLTree->pszValue, "OGRVRTUnionLayer") && nRecLevel < 30)
    {
        return InstantiateUnionLayer(psLTree, pszVRTDirectory, bUpdate,
                                     nRecLevel + 1);
    }

    return nullptr;
}

/************************************************************************/
/*                       OGRVRTOpenProxiedLayer()                       */
/************************************************************************/

struct PooledInitData
{
    OGRVRTDataSource *poDS = nullptr;
    CPLXMLNode *psNode = nullptr;
    std::string osVRTDirectory{};
    bool bUpdate = false;
};

static OGRLayer *OGRVRTOpenProxiedLayer(void *pUserData)
{
    const PooledInitData *pData =
        static_cast<const PooledInitData *>(pUserData);
    return pData->poDS->InstantiateLayerInternal(
        pData->psNode, pData->osVRTDirectory.c_str(), pData->bUpdate, 0);
}

/************************************************************************/
/*                   OGRVRTFreeProxiedLayerUserData()                   */
/************************************************************************/

static void OGRVRTFreeProxiedLayerUserData(void *pUserData)
{
    delete static_cast<PooledInitData *>(pUserData);
}

/************************************************************************/
/*                          InstantiateLayer()                          */
/************************************************************************/

OGRLayer *OGRVRTDataSource::InstantiateLayer(CPLXMLNode *psLTree,
                                             const char *pszVRTDirectory,
                                             int bUpdate, int nRecLevel)
{
    if (poLayerPool != nullptr && EQUAL(psLTree->pszValue, "OGRVRTLayer"))
    {
        auto pData = std::make_unique<PooledInitData>();
        pData->poDS = this;
        pData->psNode = psLTree;
        pData->osVRTDirectory = pszVRTDirectory ? pszVRTDirectory : "";
        pData->bUpdate = CPL_TO_BOOL(bUpdate);
        return new OGRProxiedLayer(poLayerPool.get(), OGRVRTOpenProxiedLayer,
                                   OGRVRTFreeProxiedLayerUserData,
                                   pData.release());
    }

    return InstantiateLayerInternal(psLTree, pszVRTDirectory, bUpdate,
                                    nRecLevel);
}

/************************************************************************/
/*                         CountOGRVRTLayers()                          */
/************************************************************************/

static int CountOGRVRTLayers(const CPLXMLNode *psTree)
{
    if (psTree->eType != CXT_Element)
        return 0;

    int nCount = 0;
    if (EQUAL(psTree->pszValue, "OGRVRTLayer"))
        ++nCount;

    for (const CPLXMLNode *psNode = psTree->psChild; psNode != nullptr;
         psNode = psNode->psNext)
    {
        nCount += CountOGRVRTLayers(psNode);
    }

    return nCount;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

bool OGRVRTDataSource::Initialize(CPLXMLNode *psTreeIn, const char *pszNewName,
                                  int bUpdate)

{
    CPLAssert(nLayers == 0);

    AddForbiddenNames(pszNewName);

    psTree = psTreeIn;

    // Set name, and capture the directory path so we can use it
    // for relative datasources.
    CPLString osVRTDirectory = CPLGetPathSafe(pszNewName);

    // Look for the OGRVRTDataSource node, it might be after an <xml> node.
    CPLXMLNode *psVRTDSXML = CPLGetXMLNode(psTree, "=OGRVRTDataSource");
    if (psVRTDSXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Did not find the <OGRVRTDataSource> node in the root of the "
                 "document, this is not really an OGR VRT.");
        return false;
    }

    // Determine if we must proxy layers.
    const int nOGRVRTLayerCount = CountOGRVRTLayers(psVRTDSXML);

    const int nMaxSimultaneouslyOpened =
        std::max(atoi(CPLGetConfigOption("OGR_VRT_MAX_OPENED", "100")), 1);
    if (nOGRVRTLayerCount > nMaxSimultaneouslyOpened)
        poLayerPool = std::make_unique<OGRLayerPool>(nMaxSimultaneouslyOpened);

    // Apply any dataset level metadata.
    oMDMD.XMLInit(psVRTDSXML, TRUE);

    // Look for layers.
    for (CPLXMLNode *psLTree = psVRTDSXML->psChild; psLTree != nullptr;
         psLTree = psLTree->psNext)
    {
        if (psLTree->eType != CXT_Element)
            continue;

        // Create the layer object.
        OGRLayer *poLayer = InstantiateLayer(psLTree, osVRTDirectory, bUpdate);
        if (poLayer == nullptr)
            continue;

        // Add layer to data source layer list.
        nLayers++;
        papoLayers = static_cast<OGRLayer **>(
            CPLRealloc(papoLayers, sizeof(OGRLayer *) * nLayers));
        papoLayers[nLayers - 1] = poLayer;

        paeLayerType = static_cast<OGRLayerType *>(
            CPLRealloc(paeLayerType, sizeof(int) * nLayers));
        if (poLayerPool != nullptr && EQUAL(psLTree->pszValue, "OGRVRTLayer"))
        {
            paeLayerType[nLayers - 1] = OGR_VRT_PROXIED_LAYER;
        }
        else if (EQUAL(psLTree->pszValue, "OGRVRTLayer"))
        {
            paeLayerType[nLayers - 1] = OGR_VRT_LAYER;
        }
        else
        {
            paeLayerType[nLayers - 1] = OGR_VRT_OTHER_LAYER;
        }
    }

    return true;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRVRTDataSource::TestCapability(const char *pszCap) const
{
    if (EQUAL(pszCap, ODsCCurveGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;

    return false;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

const OGRLayer *OGRVRTDataSource::GetLayer(int iLayer) const

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                         AddForbiddenNames()                          */
/************************************************************************/

void OGRVRTDataSource::AddForbiddenNames(const char *pszOtherDSName)
{
    aosOtherDSNameSet.insert(pszOtherDSName);
}

/************************************************************************/
/*                         IsInForbiddenNames()                         */
/************************************************************************/

bool OGRVRTDataSource::IsInForbiddenNames(const char *pszOtherDSName) const
{
    return aosOtherDSNameSet.find(pszOtherDSName) != aosOtherDSNameSet.end();
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **OGRVRTDataSource::GetFileList()
{
    CPLStringList oList;
    oList.AddString(GetDescription());
    for (int i = 0; i < nLayers; i++)
    {
        OGRLayer *poLayer = papoLayers[i];
        OGRVRTLayer *poVRTLayer = nullptr;
        switch (paeLayerType[nLayers - 1])
        {
            case OGR_VRT_PROXIED_LAYER:
                poVRTLayer = cpl::down_cast<OGRVRTLayer *>(
                    cpl::down_cast<OGRProxiedLayer *>(poLayer)
                        ->GetUnderlyingLayer());
                break;
            case OGR_VRT_LAYER:
                poVRTLayer = cpl::down_cast<OGRVRTLayer *>(poLayer);
                break;
            default:
                break;
        }
        if (poVRTLayer != nullptr)
        {
            GDALDataset *poSrcDS = poVRTLayer->GetSrcDataset();
            if (poSrcDS != nullptr)
            {
                char **papszFileList = poSrcDS->GetFileList();
                char **papszIter = papszFileList;
                for (; papszIter != nullptr && *papszIter != nullptr;
                     papszIter++)
                {
                    if (oList.FindString(*papszIter) < 0)
                        oList.AddString(*papszIter);
                }
                CSLDestroy(papszFileList);
            }
        }
    }
    return oList.StealList();
}
