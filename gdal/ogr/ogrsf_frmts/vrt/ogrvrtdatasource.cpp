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

CPL_CVSID("$Id$")

/************************************************************************/
/*                       OGRVRTGetGeometryType()                        */
/************************************************************************/

#define STRINGIFY(x)  x, #x

static const struct {
    OGRwkbGeometryType eType;
    const char        *pszName;
    bool               bIsoFlags;
} asGeomTypeNames[] = {
    { STRINGIFY(wkbUnknown), false },

    { STRINGIFY(wkbPoint), false },
    { STRINGIFY(wkbLineString), false },
    { STRINGIFY(wkbPolygon), false },
    { STRINGIFY(wkbMultiPoint), false },
    { STRINGIFY(wkbMultiLineString), false },
    { STRINGIFY(wkbMultiPolygon), false },
    { STRINGIFY(wkbGeometryCollection), false },

    { STRINGIFY(wkbCircularString), true },
    { STRINGIFY(wkbCompoundCurve), true },
    { STRINGIFY(wkbCurvePolygon), true },
    { STRINGIFY(wkbMultiCurve), true },
    { STRINGIFY(wkbMultiSurface), true },
    { STRINGIFY(wkbCurve), true },
    { STRINGIFY(wkbSurface), true },
    { STRINGIFY(wkbPolyhedralSurface), true },
    { STRINGIFY(wkbTIN), true },
    { STRINGIFY(wkbTriangle), true },

    { STRINGIFY(wkbNone), false },
    { STRINGIFY(wkbLinearRing), false },
};

OGRwkbGeometryType OGRVRTGetGeometryType( const char *pszGType, int *pbError )
{
    if( pbError )
        *pbError = FALSE;

    for( const auto& entry: asGeomTypeNames )
    {
        if( EQUALN(pszGType, entry.pszName,
                   strlen(entry.pszName)) )
        {
            OGRwkbGeometryType eGeomType = entry.eType;

            if( strstr(pszGType, "25D") != nullptr ||
                strstr(pszGType, "Z") != nullptr )
                eGeomType = wkbSetZ(eGeomType);
            if( pszGType[strlen(pszGType) - 1] == 'M' ||
                pszGType[strlen(pszGType) - 2] == 'M' )
                eGeomType = wkbSetM(eGeomType);
            return eGeomType;
        }
    }

    if( pbError )
        *pbError = TRUE;
    return wkbUnknown;
}

/************************************************************************/
/*                     OGRVRTGetSerializedGeometryType()                */
/************************************************************************/

CPLString OGRVRTGetSerializedGeometryType(OGRwkbGeometryType eGeomType)
{
    for( const auto& entry: asGeomTypeNames )
    {
        if( entry.eType == wkbFlatten(eGeomType) )
        {
            CPLString osRet(entry.pszName);
            if( entry.bIsoFlags || OGR_GT_HasM(eGeomType) )
            {
                if( OGR_GT_HasZ(eGeomType) )
                {
                    osRet += "Z";
                }
                if( OGR_GT_HasM(eGeomType) )
                {
                    osRet += "M";
                }
            }
            else if(OGR_GT_HasZ(eGeomType) )
            {
                osRet += "25D";
            }
            return osRet;
        }
    }
    return CPLString();
}

/************************************************************************/
/*                          OGRVRTDataSource()                          */
/************************************************************************/

OGRVRTDataSource::OGRVRTDataSource( GDALDriver *poDriverIn ) :
    papoLayers(nullptr),
    paeLayerType(nullptr),
    nLayers(0),
    pszName(nullptr),
    psTree(nullptr),
    nCallLevel(0),
    poLayerPool(nullptr),
    poParentDS(nullptr),
    bRecursionDetected(false)
{
    poDriver = poDriverIn;
}

/************************************************************************/
/*                         ~OGRVRTDataSource()                         */
/************************************************************************/

OGRVRTDataSource::~OGRVRTDataSource()

{
    CPLFree(pszName);

    OGRVRTDataSource::CloseDependentDatasets();

    CPLFree(paeLayerType);

    if( psTree != nullptr)
        CPLDestroyXMLNode(psTree);

    delete poLayerPool;
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int OGRVRTDataSource::CloseDependentDatasets()
{
    const int bHasClosedDependentDatasets = nLayers > 0;
    for( int i = 0; i < nLayers; i++ )
    {
        delete papoLayers[i];
    }
    CPLFree(papoLayers);
    nLayers = 0;
    papoLayers = nullptr;
    return bHasClosedDependentDatasets;
}

/************************************************************************/
/*                        InstantiateWarpedLayer()                      */
/************************************************************************/

OGRLayer *OGRVRTDataSource::InstantiateWarpedLayer(
    CPLXMLNode *psLTree,
    const char *pszVRTDirectory,
    int bUpdate,
    int nRecLevel)
{
    if( !EQUAL(psLTree->pszValue,"OGRVRTWarpedLayer") )
        return nullptr;

    OGRLayer *poSrcLayer = nullptr;

    for( CPLXMLNode *psSubNode = psLTree->psChild; psSubNode != nullptr;
         psSubNode = psSubNode->psNext )
    {
        if( psSubNode->eType != CXT_Element )
            continue;

        poSrcLayer = InstantiateLayer(psSubNode, pszVRTDirectory,
                                      bUpdate, nRecLevel + 1);
        if( poSrcLayer != nullptr )
            break;
    }

    if( poSrcLayer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot instantiate source layer");
        return nullptr;
    }

    const char *pszTargetSRS = CPLGetXMLValue(psLTree, "TargetSRS", nullptr);
    if( pszTargetSRS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing TargetSRS element within OGRVRTWarpedLayer");
        delete poSrcLayer;
        return nullptr;
    }

    const char *pszGeomFieldName =
        CPLGetXMLValue(psLTree, "WarpedGeomFieldName", nullptr);
    int iGeomField = 0;
    if( pszGeomFieldName != nullptr )
    {
        iGeomField =
            poSrcLayer->GetLayerDefn()->GetGeomFieldIndex(pszGeomFieldName);
        if( iGeomField < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find source geometry field '%s'",
                     pszGeomFieldName);
            delete poSrcLayer;
            return nullptr;
        }
    }

    OGRSpatialReference *poSrcSRS = nullptr;
    const char *pszSourceSRS = CPLGetXMLValue(psLTree, "SrcSRS", nullptr);

    if( pszSourceSRS == nullptr )
    {
        if( iGeomField < poSrcLayer->GetLayerDefn()->GetGeomFieldCount() )
        {
            poSrcSRS = poSrcLayer->GetLayerDefn()
                           ->GetGeomFieldDefn(iGeomField)
                           ->GetSpatialRef();
            if( poSrcSRS != nullptr )
                poSrcSRS = poSrcSRS->Clone();
        }
    }
    else
    {
        poSrcSRS = new OGRSpatialReference();
        poSrcSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poSrcSRS->SetFromUserInput(pszSourceSRS, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS) != OGRERR_NONE )
        {
            delete poSrcSRS;
            poSrcSRS = nullptr;
        }
    }

    if( poSrcSRS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to import source SRS");
        delete poSrcLayer;
        return nullptr;
    }

    OGRSpatialReference *poTargetSRS = new OGRSpatialReference();
    poTargetSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( poTargetSRS->SetFromUserInput(pszTargetSRS, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS) != OGRERR_NONE )
    {
        delete poTargetSRS;
        poTargetSRS = nullptr;
    }

    if( poTargetSRS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to import target SRS");
        delete poSrcSRS;
        delete poSrcLayer;
        return nullptr;
    }

    if( pszSourceSRS == nullptr && poSrcSRS->IsSame(poTargetSRS) )
    {
        delete poSrcSRS;
        delete poTargetSRS;
        return poSrcLayer;
    }

    OGRCoordinateTransformation *poCT =
        OGRCreateCoordinateTransformation(poSrcSRS, poTargetSRS);
    OGRCoordinateTransformation *poReversedCT =
        poCT != nullptr ? OGRCreateCoordinateTransformation(poTargetSRS, poSrcSRS)
                     : nullptr;

    delete poSrcSRS;
    delete poTargetSRS;

    if( poCT == nullptr )
    {
        delete poSrcLayer;
        return nullptr;
    }

    // Build the OGRWarpedLayer.
    OGRWarpedLayer *poLayer =
        new OGRWarpedLayer(poSrcLayer, iGeomField, TRUE, poCT, poReversedCT);

    // Set Extent if provided.
    const char *pszExtentXMin = CPLGetXMLValue(psLTree, "ExtentXMin", nullptr);
    const char *pszExtentYMin = CPLGetXMLValue(psLTree, "ExtentYMin", nullptr);
    const char *pszExtentXMax = CPLGetXMLValue(psLTree, "ExtentXMax", nullptr);
    const char *pszExtentYMax = CPLGetXMLValue(psLTree, "ExtentYMax", nullptr);
    if( pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
        pszExtentXMax != nullptr && pszExtentYMax != nullptr )
    {
        poLayer->SetExtent(CPLAtof(pszExtentXMin),
                           CPLAtof(pszExtentYMin),
                           CPLAtof(pszExtentXMax),
                           CPLAtof(pszExtentYMax));
    }

    return poLayer;
}

/************************************************************************/
/*                        InstantiateUnionLayer()                       */
/************************************************************************/

OGRLayer *OGRVRTDataSource::InstantiateUnionLayer(
    CPLXMLNode *psLTree,
    const char *pszVRTDirectory,
    int bUpdate,
    int nRecLevel)
{
    if( !EQUAL(psLTree->pszValue, "OGRVRTUnionLayer") )
        return nullptr;

    // Get layer name.
    const char *pszLayerName = CPLGetXMLValue(psLTree, "name", nullptr);

    if( pszLayerName == nullptr )
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
    if( pszGType != nullptr )
    {
        bGlobalGeomTypeSet = true;
        int bError = FALSE;
        eGlobalGeomType = OGRVRTGetGeometryType(pszGType, &bError);
        if( bError )
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
    if( pszLayerSRS != nullptr )
    {
        bGlobalSRSSet = true;
        if( !EQUAL(pszLayerSRS, "NULL") )
        {
            OGRSpatialReference oSRS;
            oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            if( oSRS.SetFromUserInput(pszLayerSRS, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS) != OGRERR_NONE )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to import LayerSRS `%s'.", pszLayerSRS);
                return nullptr;
            }
            poGlobalSRS = oSRS.Clone();
        }
    }

    // Find field declarations.
    OGRFieldDefn **papoFields = nullptr;
    int nFields = 0;
    OGRUnionLayerGeomFieldDefn **papoGeomFields = nullptr;
    int nGeomFields = 0;

    for( CPLXMLNode *psSubNode = psLTree->psChild; psSubNode != nullptr;
         psSubNode = psSubNode->psNext )
    {
        if( psSubNode->eType != CXT_Element )
            continue;

        if( EQUAL(psSubNode->pszValue, "Field") )
        {
            // Field name.
            const char *l_pszName = CPLGetXMLValue(psSubNode, "name", nullptr);
            if( l_pszName == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to identify Field name.");
                break;
            }

            OGRFieldDefn oFieldDefn(l_pszName, OFTString);

            // Type.
            const char *pszArg = CPLGetXMLValue(psSubNode, "type", nullptr);

            if( pszArg != nullptr )
            {
                int iType = 0;  // Used after for.

                for( ; iType <= static_cast<int>(OFTMaxType); iType++ )
                {
                    if( EQUAL(pszArg, OGRFieldDefn::GetFieldTypeName(
                                          static_cast<OGRFieldType>(iType))) )
                    {
                        oFieldDefn.SetType(static_cast<OGRFieldType>(iType));
                        break;
                    }
                }

                if( iType > static_cast<int>(OFTMaxType) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to identify Field type '%s'.", pszArg);
                    break;
                }
            }

            // Width and precision.
            const int nWidth = atoi(CPLGetXMLValue(psSubNode, "width", "0"));
            if( nWidth < 0 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid width for field %s.", l_pszName);
                break;
            }
            oFieldDefn.SetWidth(nWidth);

            const int nPrecision =
                atoi(CPLGetXMLValue(psSubNode, "precision", "0"));
            if( nPrecision < 0 || nPrecision > 1024 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid precision for field %s.", l_pszName);
                break;
            }
            oFieldDefn.SetPrecision(nPrecision);

            papoFields = static_cast<OGRFieldDefn **>(CPLRealloc(
                papoFields, sizeof(OGRFieldDefn *) * (nFields + 1)));
            papoFields[nFields] = new OGRFieldDefn(&oFieldDefn);
            nFields++;
        }
        else if( psSubNode->eType == CXT_Element &&
                 EQUAL(psSubNode->pszValue, "GeometryField") )
        {
            const char *l_pszName = CPLGetXMLValue(psSubNode, "name", nullptr);
            if( l_pszName == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to identify GeometryField name.");
                break;
            }

            pszGType = CPLGetXMLValue(psSubNode, "GeometryType", nullptr);
            if( pszGType == nullptr && nGeomFields == 0 )
                pszGType = CPLGetXMLValue(psLTree, "GeometryType", nullptr);
            OGRwkbGeometryType eGeomType = wkbUnknown;
            bool bGeomTypeSet = false;
            if( pszGType != nullptr )
            {
                int bError = FALSE;
                eGeomType = OGRVRTGetGeometryType(pszGType, &bError);
                bGeomTypeSet = true;
                if( bError || eGeomType == wkbNone )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "GeometryType %s not recognised.", pszGType);
                    break;
                }
            }

            const char *pszSRS = CPLGetXMLValue(psSubNode, "SRS", nullptr);
            if( pszSRS == nullptr && nGeomFields == 0 )
                pszSRS = CPLGetXMLValue(psLTree, "LayerSRS", nullptr);
            OGRSpatialReference *poSRS = nullptr;
            bool bSRSSet = false;
            if( pszSRS != nullptr )
            {
                bSRSSet = true;
                if( !EQUAL(pszSRS, "NULL") )
                {
                    OGRSpatialReference oSRS;
                    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                    if( oSRS.SetFromUserInput(pszSRS, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS) != OGRERR_NONE )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to import SRS `%s'.", pszSRS);
                        break;
                    }
                    poSRS = oSRS.Clone();
                }
             }

            OGRUnionLayerGeomFieldDefn *poFieldDefn =
                new OGRUnionLayerGeomFieldDefn(l_pszName, eGeomType);
            if( poSRS != nullptr )
            {
                poFieldDefn->SetSpatialRef(poSRS);
                poSRS->Dereference();
            }
            poFieldDefn->bGeomTypeSet = bGeomTypeSet;
            poFieldDefn->bSRSSet = bSRSSet;

            const char *pszExtentXMin =
                CPLGetXMLValue(psSubNode, "ExtentXMin", nullptr);
            const char *pszExtentYMin =
                CPLGetXMLValue(psSubNode, "ExtentYMin", nullptr);
            const char *pszExtentXMax =
                CPLGetXMLValue(psSubNode, "ExtentXMax", nullptr);
            const char *pszExtentYMax =
                CPLGetXMLValue(psSubNode, "ExtentYMax", nullptr);
            if( pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
                pszExtentXMax != nullptr && pszExtentYMax != nullptr )
            {
                poFieldDefn->sStaticEnvelope.MinX = CPLAtof(pszExtentXMin);
                poFieldDefn->sStaticEnvelope.MinY = CPLAtof(pszExtentYMin);
                poFieldDefn->sStaticEnvelope.MaxX = CPLAtof(pszExtentXMax);
                poFieldDefn->sStaticEnvelope.MaxY = CPLAtof(pszExtentYMax);
            }

            papoGeomFields = static_cast<OGRUnionLayerGeomFieldDefn **>(
                CPLRealloc(
                    papoGeomFields,
                    sizeof(OGRUnionLayerGeomFieldDefn *) * (nGeomFields + 1)));
            papoGeomFields[nGeomFields] = poFieldDefn;
            nGeomFields++;
        }
    }

    // Set Extent if provided.
    const char *pszExtentXMin = CPLGetXMLValue(psLTree, "ExtentXMin", nullptr);
    const char *pszExtentYMin = CPLGetXMLValue(psLTree, "ExtentYMin", nullptr);
    const char *pszExtentXMax = CPLGetXMLValue(psLTree, "ExtentXMax", nullptr);
    const char *pszExtentYMax = CPLGetXMLValue(psLTree, "ExtentYMax", nullptr);

    if( eGlobalGeomType != wkbNone && nGeomFields == 0 &&
        (bGlobalGeomTypeSet || bGlobalSRSSet ||
         (pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
          pszExtentXMax != nullptr && pszExtentYMax != nullptr)) )
    {
        OGRUnionLayerGeomFieldDefn *poFieldDefn =
            new OGRUnionLayerGeomFieldDefn("", eGlobalGeomType);
        if( poGlobalSRS != nullptr )
        {
            poFieldDefn->SetSpatialRef(poGlobalSRS);
            poGlobalSRS->Dereference();
            poGlobalSRS = nullptr;
        }
        poFieldDefn->bGeomTypeSet = bGlobalGeomTypeSet;
        poFieldDefn->bSRSSet = bGlobalSRSSet;
        if( pszExtentXMin != nullptr && pszExtentYMin != nullptr &&
            pszExtentXMax != nullptr && pszExtentYMax != nullptr )
        {
            poFieldDefn->sStaticEnvelope.MinX = CPLAtof(pszExtentXMin);
            poFieldDefn->sStaticEnvelope.MinY = CPLAtof(pszExtentYMin);
            poFieldDefn->sStaticEnvelope.MaxX = CPLAtof(pszExtentXMax);
            poFieldDefn->sStaticEnvelope.MaxY = CPLAtof(pszExtentYMax);
        }

        papoGeomFields = static_cast<OGRUnionLayerGeomFieldDefn **>(CPLRealloc(
            papoGeomFields,
            sizeof(OGRUnionLayerGeomFieldDefn *) * (nGeomFields + 1)));
        papoGeomFields[nGeomFields] = poFieldDefn;
        nGeomFields++;
    }
    else
    {
        delete poGlobalSRS;
        poGlobalSRS = nullptr;
    }

    // Find source layers.
    int nSrcLayers = 0;
    OGRLayer **papoSrcLayers = nullptr;

    for( CPLXMLNode *psSubNode = psLTree->psChild; psSubNode != nullptr;
         psSubNode = psSubNode->psNext )
    {
        if( psSubNode->eType != CXT_Element )
            continue;

        OGRLayer *poSrcLayer = InstantiateLayer(psSubNode, pszVRTDirectory,
                                                bUpdate, nRecLevel + 1);
        if( poSrcLayer != nullptr )
        {
          papoSrcLayers = static_cast<OGRLayer **>(CPLRealloc(
                papoSrcLayers, sizeof(OGRLayer *) * (nSrcLayers + 1)));
            papoSrcLayers[nSrcLayers] = poSrcLayer;
            nSrcLayers++;
        }
    }

    if( nSrcLayers == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find source layers");
        for( int iField = 0; iField < nFields; iField++ )
            delete papoFields[iField];
        CPLFree(papoFields);
        for( int iField = 0; iField < nGeomFields; iField++ )
            delete papoGeomFields[iField];
        CPLFree(papoGeomFields);
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
    if( pszPreserveFID != nullptr )
        bPreserveSrcFID = CPLTestBool(pszPreserveFID);
    poLayer->SetPreserveSrcFID(bPreserveSrcFID);

    // Set fields.
    FieldUnionStrategy eFieldStrategy = FIELD_UNION_ALL_LAYERS;
    const char *pszFieldStrategy =
        CPLGetXMLValue(psLTree, "FieldStrategy", nullptr);
    if( pszFieldStrategy != nullptr )
    {
        if( EQUAL(pszFieldStrategy, "FirstLayer") )
            eFieldStrategy = FIELD_FROM_FIRST_LAYER;
        else if( EQUAL(pszFieldStrategy, "Union") )
            eFieldStrategy = FIELD_UNION_ALL_LAYERS;
        else if( EQUAL(pszFieldStrategy, "Intersection") )
            eFieldStrategy = FIELD_INTERSECTION_ALL_LAYERS;
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unhandled value for FieldStrategy `%s'.",
                     pszFieldStrategy);
        }
    }
    if( nFields != 0 || nGeomFields > 1 )
    {
        if( pszFieldStrategy != nullptr )
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ignoring FieldStrategy value, "
                     "because explicit Field or GeometryField is provided");
        eFieldStrategy = FIELD_SPECIFIED;
    }

    poLayer->SetFields(
        eFieldStrategy, nFields, papoFields,
        (nGeomFields == 0 && eGlobalGeomType == wkbNone) ? -1 : nGeomFields,
        papoGeomFields);

    for(int iField = 0; iField < nFields; iField++)
        delete papoFields[iField];
    CPLFree(papoFields);
    for(int iField = 0; iField < nGeomFields; iField++)
        delete papoGeomFields[iField];
    CPLFree(papoGeomFields);

    // Set FeatureCount if provided.
    const char *pszFeatureCount = CPLGetXMLValue(psLTree, "FeatureCount", nullptr);
    if( pszFeatureCount != nullptr )
    {
        poLayer->SetFeatureCount(atoi(pszFeatureCount));
    }

    return poLayer;
}

/************************************************************************/
/*                     InstantiateLayerInternal()                       */
/************************************************************************/

OGRLayer *OGRVRTDataSource::InstantiateLayerInternal(
    CPLXMLNode *psLTree,
    const char *pszVRTDirectory,
    int bUpdate,
    int nRecLevel )
{
    // Create the layer object.
    if( EQUAL(psLTree->pszValue, "OGRVRTLayer") )
    {
        OGRVRTLayer *poVRTLayer = new OGRVRTLayer(this);

        if( !poVRTLayer->FastInitialize(psLTree, pszVRTDirectory, bUpdate) )
        {
            delete poVRTLayer;
            return nullptr;
        }

        return poVRTLayer;
    }
    else if( EQUAL(psLTree->pszValue,"OGRVRTWarpedLayer") && nRecLevel < 30 )
    {
        return InstantiateWarpedLayer(psLTree, pszVRTDirectory,
                                      bUpdate, nRecLevel + 1);
    }
    else if( EQUAL(psLTree->pszValue,"OGRVRTUnionLayer") && nRecLevel < 30 )
    {
        return InstantiateUnionLayer(psLTree, pszVRTDirectory,
                                     bUpdate, nRecLevel + 1);
    }

    return nullptr;
}

/************************************************************************/
/*                        OGRVRTOpenProxiedLayer()                      */
/************************************************************************/

typedef struct
{
    OGRVRTDataSource *poDS;
    CPLXMLNode *psNode;
    char       *pszVRTDirectory;
    bool        bUpdate;
} PooledInitData;

static OGRLayer *OGRVRTOpenProxiedLayer(void *pUserData)
{
    PooledInitData *pData = static_cast<PooledInitData *>(pUserData);
    return pData->poDS->InstantiateLayerInternal(
        pData->psNode, pData->pszVRTDirectory, pData->bUpdate, 0);
}

/************************************************************************/
/*                     OGRVRTFreeProxiedLayerUserData()                 */
/************************************************************************/

static void OGRVRTFreeProxiedLayerUserData(void *pUserData)
{
    PooledInitData *pData = static_cast<PooledInitData *>(pUserData);
    CPLFree(pData->pszVRTDirectory);
    CPLFree(pData);
}

/************************************************************************/
/*                          InstantiateLayer()                          */
/************************************************************************/

OGRLayer *OGRVRTDataSource::InstantiateLayer(
    CPLXMLNode *psLTree,
    const char *pszVRTDirectory,
    int bUpdate,
    int nRecLevel )
{
    if( poLayerPool != nullptr && EQUAL(psLTree->pszValue,"OGRVRTLayer"))
    {
        PooledInitData *pData =
            (PooledInitData *)CPLMalloc(sizeof(PooledInitData));
        pData->poDS = this;
        pData->psNode = psLTree;
        pData->pszVRTDirectory = CPLStrdup(pszVRTDirectory);
        pData->bUpdate = CPL_TO_BOOL(bUpdate);
        return new OGRProxiedLayer(poLayerPool, OGRVRTOpenProxiedLayer,
                                   OGRVRTFreeProxiedLayerUserData, pData);
    }

    return InstantiateLayerInternal(psLTree, pszVRTDirectory,
                                    bUpdate, nRecLevel);
}

/************************************************************************/
/*                           CountOGRVRTLayers()                        */
/************************************************************************/

static int CountOGRVRTLayers(CPLXMLNode *psTree)
{
    if( psTree->eType != CXT_Element )
        return 0;

    int nCount = 0;
    if( EQUAL(psTree->pszValue, "OGRVRTLayer") )
        ++nCount;

    for( CPLXMLNode *psNode = psTree->psChild; psNode != nullptr;
         psNode = psNode->psNext )
    {
        nCount += CountOGRVRTLayers(psNode);
    }

    return nCount;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

bool OGRVRTDataSource::Initialize( CPLXMLNode *psTreeIn, const char *pszNewName,
                                   int bUpdate )

{
    CPLAssert(nLayers == 0);

    AddForbiddenNames(pszNewName);

    psTree = psTreeIn;

    // Set name, and capture the directory path so we can use it
    // for relative datasources.
    CPLString osVRTDirectory = CPLGetPath(pszNewName);

    pszName = CPLStrdup(pszNewName);

    // Look for the OGRVRTDataSource node, it might be after an <xml> node.
    CPLXMLNode *psVRTDSXML = CPLGetXMLNode(psTree, "=OGRVRTDataSource");
    if( psVRTDSXML == nullptr )
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
    if( nOGRVRTLayerCount > nMaxSimultaneouslyOpened )
        poLayerPool = new OGRLayerPool(nMaxSimultaneouslyOpened);

    // Apply any dataset level metadata.
    oMDMD.XMLInit(psVRTDSXML, TRUE);

    // Look for layers.
    for( CPLXMLNode *psLTree = psVRTDSXML->psChild; psLTree != nullptr;
         psLTree = psLTree->psNext )
    {
        if( psLTree->eType != CXT_Element )
            continue;

        // Create the layer object.
        OGRLayer *poLayer = InstantiateLayer(psLTree, osVRTDirectory, bUpdate);
        if( poLayer == nullptr )
            continue;

        // Add layer to data source layer list.
        nLayers++;
        papoLayers = static_cast<OGRLayer **>(
            CPLRealloc(papoLayers, sizeof(OGRLayer *) * nLayers));
        papoLayers[nLayers - 1] = poLayer;

        paeLayerType = static_cast<OGRLayerType *>(
            CPLRealloc(paeLayerType, sizeof(int) * nLayers));
        if( poLayerPool != nullptr && EQUAL(psLTree->pszValue, "OGRVRTLayer") )
        {
            paeLayerType[nLayers - 1] = OGR_VRT_PROXIED_LAYER;
        }
        else if( EQUAL(psLTree->pszValue, "OGRVRTLayer") )
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

int OGRVRTDataSource::TestCapability( const char *pszCap )
{
    if( EQUAL(pszCap, ODsCCurveGeometries) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRVRTDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
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

bool OGRVRTDataSource::IsInForbiddenNames( const char *pszOtherDSName ) const
{
    return aosOtherDSNameSet.find(pszOtherDSName) != aosOtherDSNameSet.end();
}

/************************************************************************/
/*                             GetFileList()                             */
/************************************************************************/

char **OGRVRTDataSource::GetFileList()
{
    CPLStringList oList;
    oList.AddString(GetName());
    for( int i = 0; i < nLayers; i++ )
    {
        OGRLayer *poLayer = papoLayers[i];
        OGRVRTLayer *poVRTLayer = nullptr;
        switch( paeLayerType[nLayers - 1] )
        {
        case OGR_VRT_PROXIED_LAYER:
            poVRTLayer = (OGRVRTLayer *)((OGRProxiedLayer *)poLayer)
                             ->GetUnderlyingLayer();
            break;
        case OGR_VRT_LAYER:
            poVRTLayer = (OGRVRTLayer *)poLayer;
            break;
        default:
            break;
        }
        if(poVRTLayer != nullptr)
        {
            GDALDataset *poSrcDS = poVRTLayer->GetSrcDataset();
            if( poSrcDS != nullptr )
            {
                char **papszFileList = poSrcDS->GetFileList();
                char **papszIter = papszFileList;
                for( ; papszIter != nullptr && *papszIter != nullptr; papszIter++ )
                {
                    if( oList.FindString(*papszIter) < 0 )
                        oList.AddString(*papszIter);
                }
                CSLDestroy(papszFileList);
            }
        }
    }
    return oList.StealList();
}
