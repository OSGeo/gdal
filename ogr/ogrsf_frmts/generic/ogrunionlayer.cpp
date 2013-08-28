/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRUnionLayer class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogrunionlayer.h"
#include "ogrwarpedlayer.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRUnionLayer()                             */
/************************************************************************/

OGRUnionLayer::OGRUnionLayer( const char* pszName,
                              int nSrcLayersIn,
                              OGRLayer** papoSrcLayersIn,
                              int bTakeLayerOwnership )
{
    CPLAssert(nSrcLayersIn > 0);

    osName = pszName;
    nSrcLayers = nSrcLayersIn;
    papoSrcLayers = papoSrcLayersIn;
    bHasLayerOwnership = bTakeLayerOwnership;

    poFeatureDefn = NULL;
    nFields = 0;
    papoFields = NULL;
    eFieldStrategy = FIELD_UNION_ALL_LAYERS;

    eGeomType = wkbUnknown;
    eGeometryTypeStrategy = GEOMTYPE_UNION_ALL_LAYERS;

    bPreserveSrcFID = FALSE;

    nFeatureCount = -1;

    poSRS = NULL;
    bSRSSet = FALSE;

    iCurLayer = -1;
    pszAttributeFilter = NULL;
    nNextFID = 0;
    panMap = NULL;
    papszIgnoredFields = NULL;
    bAttrFilterPassThroughValue = -1;

    pabModifiedLayers = (int*)CPLCalloc(sizeof(int), nSrcLayers);
    pabCheckIfAutoWrap = (int*)CPLCalloc(sizeof(int), nSrcLayers);
}

/************************************************************************/
/*                         ~OGRUnionLayer()                             */
/************************************************************************/

OGRUnionLayer::~OGRUnionLayer()
{
    int i;

    if( bHasLayerOwnership )
    {
        for(i = 0; i < nSrcLayers; i++)
            delete papoSrcLayers[i];
    }
    CPLFree(papoSrcLayers);

    for(i = 0; i < nFields; i++)
        delete papoFields[i];
    CPLFree(papoFields);

    CPLFree(pszAttributeFilter);
    CPLFree(panMap);
    CSLDestroy(papszIgnoredFields);
    CPLFree(pabModifiedLayers);
    CPLFree(pabCheckIfAutoWrap);

    if( poSRS != NULL )
        poSRS->Release();

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                               SetSRS()                               */
/************************************************************************/

void OGRUnionLayer::SetSRS(OGRSpatialReference *poSRSIn)
{
    CPLAssert(poFeatureDefn == NULL);

    bSRSSet = TRUE;
    if( poSRS )
        poSRS->Release();
    poSRS = (poSRSIn != NULL) ? poSRSIn->Clone() : NULL;
}

/************************************************************************/
/*                              SetFields()                             */
/************************************************************************/

void OGRUnionLayer::SetFields(FieldUnionStrategy eFieldStrategyIn,
                              int nFieldsIn,
                              OGRFieldDefn** papoFieldsIn)
{
    CPLAssert(nFields == 0);
    CPLAssert(poFeatureDefn == NULL);

    eFieldStrategy = eFieldStrategyIn;
    if( nFieldsIn )
    {
        nFields = nFieldsIn;
        papoFields = (OGRFieldDefn** )CPLMalloc(nFields * sizeof(OGRFieldDefn*));
        for(int i=0;i<nFields;i++)
            papoFields[i] = new OGRFieldDefn(papoFieldsIn[i]);
    }
}

/************************************************************************/
/*                          SetGeometryType()                           */
/************************************************************************/

void OGRUnionLayer::SetGeometryType(GeometryTypeUnionStrategy
                                                    eGeometryTypeStrategyIn,
                                    OGRwkbGeometryType eGeomTypeIn)
{
    CPLAssert(poFeatureDefn == NULL);

    eGeometryTypeStrategy = eGeometryTypeStrategyIn;
    eGeomType = eGeomTypeIn;
}

/************************************************************************/
/*                        SetSourceLayerFieldName()                     */
/************************************************************************/

void OGRUnionLayer::SetSourceLayerFieldName(const char* pszSourceLayerFieldName)
{
    CPLAssert(poFeatureDefn == NULL);

    CPLAssert(osSourceLayerFieldName.size() == 0);
    if( pszSourceLayerFieldName != NULL )
        osSourceLayerFieldName = pszSourceLayerFieldName;
}

/************************************************************************/
/*                           SetPreserveSrcFID()                        */
/************************************************************************/

void OGRUnionLayer::SetPreserveSrcFID(int bPreserveSrcFIDIn)
{
    CPLAssert(poFeatureDefn == NULL);

    bPreserveSrcFID = bPreserveSrcFIDIn;
}

/************************************************************************/
/*                          SetFeatureCount()                           */
/************************************************************************/

void OGRUnionLayer::SetFeatureCount(int nFeatureCountIn)
{
    CPLAssert(poFeatureDefn == NULL);

    nFeatureCount = nFeatureCountIn;
}

/************************************************************************/
/*                              SetExtent()                             */
/************************************************************************/

void OGRUnionLayer::SetExtent( double dfXMin, double dfYMin,
                               double dfXMax, double dfYMax )
{
    CPLAssert(poFeatureDefn == NULL);

    sStaticEnvelope.MinX = dfXMin;
    sStaticEnvelope.MinY = dfYMin;
    sStaticEnvelope.MaxX = dfXMax;
    sStaticEnvelope.MaxY = dfYMax;
}

/************************************************************************/
/*                         MergeFieldDefn()                             */
/************************************************************************/

static void MergeFieldDefn(OGRFieldDefn* poFieldDefn,
                           OGRFieldDefn* poSrcFieldDefn)
{
    if( poFieldDefn->GetType() != poSrcFieldDefn->GetType() )
    {
        if( poSrcFieldDefn->GetType() == OFTReal &&
            poFieldDefn->GetType() == OFTInteger)
            poFieldDefn->SetType(OFTReal);
        else
            poFieldDefn->SetType(OFTString);
    }

    if( poFieldDefn->GetWidth() != poSrcFieldDefn->GetWidth() ||
        poFieldDefn->GetPrecision() != poSrcFieldDefn->GetPrecision() )
    {
        poFieldDefn->SetWidth(0);
        poFieldDefn->SetPrecision(0);
    }
}

/************************************************************************/
/*                             GetLayerDefn()                           */
/************************************************************************/

OGRFeatureDefn *OGRUnionLayer::GetLayerDefn()
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    poFeatureDefn = new OGRFeatureDefn( osName );
    poFeatureDefn->Reference();

    int iCompareFirstIndex = 0;
    if( osSourceLayerFieldName.size() )
    {
        OGRFieldDefn oField(osSourceLayerFieldName, OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
        iCompareFirstIndex = 1;
    }

    if( eFieldStrategy == FIELD_SPECIFIED )
    {
        for(int i = 0; i < nFields; i++)
            poFeatureDefn->AddFieldDefn(papoFields[i]);
    }
    else if( eFieldStrategy == FIELD_FROM_FIRST_LAYER )
    {
        OGRFeatureDefn* poSrcFeatureDefn = papoSrcLayers[0]->GetLayerDefn();
        for(int i = 0; i < poSrcFeatureDefn->GetFieldCount(); i++)
            poFeatureDefn->AddFieldDefn(poSrcFeatureDefn->GetFieldDefn(i));
    }
    else if (eFieldStrategy == FIELD_UNION_ALL_LAYERS )
    {
        for(int iLayer = 0; iLayer < nSrcLayers; iLayer++)
        {
            OGRFeatureDefn* poSrcFeatureDefn =
                                papoSrcLayers[iLayer]->GetLayerDefn();

            /* Add any field that is found in the source layers */
            for(int i = 0; i < poSrcFeatureDefn->GetFieldCount(); i++)
            {
                OGRFieldDefn* poSrcFieldDefn = poSrcFeatureDefn->GetFieldDefn(i);
                int nIndex =
                    poFeatureDefn->GetFieldIndex(poSrcFieldDefn->GetNameRef());
                if( nIndex < 0 )
                    poFeatureDefn->AddFieldDefn(poSrcFieldDefn);
                else
                {
                    OGRFieldDefn* poFieldDefn =
                                        poFeatureDefn->GetFieldDefn(nIndex);
                    MergeFieldDefn(poFieldDefn, poSrcFieldDefn);
                }
            }
        }
    }
    else if (eFieldStrategy == FIELD_INTERSECTION_ALL_LAYERS )
    {
        OGRFeatureDefn* poSrcFeatureDefn = papoSrcLayers[0]->GetLayerDefn();
        int i;
        for(i = 0; i < poSrcFeatureDefn->GetFieldCount(); i++)
            poFeatureDefn->AddFieldDefn(poSrcFeatureDefn->GetFieldDefn(i));

        /* Remove any field that is not found in the source layers */
        for(int iLayer = 1; iLayer < nSrcLayers; iLayer++)
        {
            OGRFeatureDefn* poSrcFeatureDefn =
                                        papoSrcLayers[iLayer]->GetLayerDefn();
            for(i = iCompareFirstIndex; i < poFeatureDefn->GetFieldCount();)
            {
                OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
                int nSrcIndex = poSrcFeatureDefn->GetFieldIndex(
                                                    poFieldDefn->GetNameRef());
                if( nSrcIndex < 0 )
                {
                    poFeatureDefn->DeleteFieldDefn(i);
                }
                else
                {
                    OGRFieldDefn* poSrcFieldDefn =
                        poSrcFeatureDefn->GetFieldDefn(nSrcIndex);
                    MergeFieldDefn(poFieldDefn, poSrcFieldDefn);

                    i ++;
                }
            }
        }
    }

    poFeatureDefn->SetGeomType(GetGeomType());

    return poFeatureDefn;
}

/************************************************************************/
/*                             GetGeomType()                            */
/************************************************************************/

OGRwkbGeometryType OGRUnionLayer::GetGeomType()
{
    if( eGeometryTypeStrategy == GEOMTYPE_SPECIFIED )
        return eGeomType;

    if( eGeometryTypeStrategy == GEOMTYPE_FROM_FIRST_LAYER )
    {
        eGeomType = papoSrcLayers[0]->GetGeomType();
    }
    else if( eGeometryTypeStrategy == GEOMTYPE_UNION_ALL_LAYERS )
    {
        eGeomType = papoSrcLayers[0]->GetGeomType();
        for(int iLayer = 1; iLayer < nSrcLayers; iLayer++)
        {
            OGRwkbGeometryType eSrcGeomType =
                                    papoSrcLayers[iLayer]->GetGeomType();
            eGeomType = OGRMergeGeometryTypes(eGeomType, eSrcGeomType);
        }
    }
    eGeometryTypeStrategy = GEOMTYPE_SPECIFIED;

    return eGeomType;
}

/************************************************************************/
/*                        ConfigureActiveLayer()                        */
/************************************************************************/

void OGRUnionLayer::ConfigureActiveLayer()
{
    AutoWarpLayerIfNecessary(iCurLayer);
    ApplyAttributeFilterToSrcLayer(iCurLayer);
    papoSrcLayers[iCurLayer]->SetSpatialFilter(m_poFilterGeom);
    papoSrcLayers[iCurLayer]->ResetReading();

    /* Establish map */
    OGRFeatureDefn* poFeatureDefn = GetLayerDefn();
    OGRFeatureDefn* poSrcFeatureDefn = papoSrcLayers[iCurLayer]->GetLayerDefn();
    CPLFree(panMap);
    panMap = (int*) CPLMalloc(poSrcFeatureDefn->GetFieldCount() * sizeof(int));
    for(int i=0; i < poSrcFeatureDefn->GetFieldCount(); i++)
    {
        OGRFieldDefn* poSrcFieldDefn = poSrcFeatureDefn->GetFieldDefn(i);
        if( CSLFindString(papszIgnoredFields,
                          poSrcFieldDefn->GetNameRef() ) == -1 )
        {
            panMap[i] =
                poFeatureDefn->GetFieldIndex(poSrcFieldDefn->GetNameRef());
        }
        else
        {
            panMap[i] = -1;
        }
    }

    if( papoSrcLayers[iCurLayer]->TestCapability(OLCIgnoreFields) )
    {
        char** papszIter = papszIgnoredFields;
        char** papszFieldsSrc = NULL;
        while ( papszIter != NULL && *papszIter != NULL )
        {
            const char* pszFieldName = *papszIter;
            if ( EQUAL(pszFieldName, "OGR_GEOMETRY") ||
                 EQUAL(pszFieldName, "OGR_STYLE") ||
                 poSrcFeatureDefn->GetFieldIndex(pszFieldName) >= 0 )
            {
                papszFieldsSrc = CSLAddString(papszFieldsSrc, pszFieldName);
            }
            papszIter++;
        }

        int* panSrcFieldsUsed = (int*) CPLCalloc(sizeof(int),
                                          poSrcFeatureDefn->GetFieldCount());
        for(int iField = 0;
                iField < poFeatureDefn->GetFieldCount(); iField++)
        {
            OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
            int iSrcField =
                    poSrcFeatureDefn->GetFieldIndex(poFieldDefn->GetNameRef());
            if (iSrcField >= 0)
                panSrcFieldsUsed[iSrcField] = TRUE;
        }
        for(int iSrcField = 0;
                iSrcField < poSrcFeatureDefn->GetFieldCount(); iSrcField ++)
        {
            if( !panSrcFieldsUsed[iSrcField] )
            {
                OGRFieldDefn *poSrcDefn =
                        poSrcFeatureDefn->GetFieldDefn( iSrcField );
                papszFieldsSrc =
                        CSLAddString(papszFieldsSrc, poSrcDefn->GetNameRef());
            }
        }
        CPLFree(panSrcFieldsUsed);

        papoSrcLayers[iCurLayer]->SetIgnoredFields((const char**)papszFieldsSrc);

        CSLDestroy(papszFieldsSrc);
    }
}

/************************************************************************/
/*                             ResetReading()                           */
/************************************************************************/

void OGRUnionLayer::ResetReading()
{
    iCurLayer = 0;
    ConfigureActiveLayer();
    nNextFID = 0;
}

/************************************************************************/
/*                         AutoWarpLayerIfNecessary()                   */
/************************************************************************/

void OGRUnionLayer::AutoWarpLayerIfNecessary(int iLayer)
{
    if( !pabCheckIfAutoWrap[iLayer] )
    {
        pabCheckIfAutoWrap[iLayer] = TRUE;

        OGRSpatialReference* poSRS = GetSpatialRef();
        if( poSRS != NULL )
            poSRS->Reference();

        OGRSpatialReference* poSRS2 = papoSrcLayers[iLayer]->GetSpatialRef();

        if( (poSRS == NULL && poSRS2 != NULL) ||
            (poSRS != NULL && poSRS2 == NULL) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "SRS of layer %s not consistant with layer SRS",
                    papoSrcLayers[iLayer]->GetName());
        }
        else if (poSRS != NULL && poSRS2 != NULL &&
                 poSRS != poSRS2 && !poSRS->IsSame(poSRS2))
        {
            CPLDebug("VRT", "SRS of layer %s not consistant with layer SRS. "
                     "Trying auto warping",
                     papoSrcLayers[iLayer]->GetName());
            OGRCoordinateTransformation* poCT =
                OGRCreateCoordinateTransformation( poSRS2, poSRS );
            OGRCoordinateTransformation* poReversedCT = (poCT != NULL) ?
                OGRCreateCoordinateTransformation( poSRS, poSRS2 ) : NULL;
            if( poCT != NULL && poReversedCT != NULL )
                papoSrcLayers[iLayer] = new OGRWarpedLayer(
                            papoSrcLayers[iLayer], TRUE, poCT, poReversedCT);
        }

        if( poSRS != NULL )
            poSRS->Release();
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRUnionLayer::GetNextFeature()
{
    if( poFeatureDefn == NULL ) GetLayerDefn();
    if( iCurLayer < 0 )
        ResetReading();

    if( iCurLayer == nSrcLayers )
        return NULL;

    while(TRUE)
    {
        OGRFeature* poSrcFeature = papoSrcLayers[iCurLayer]->GetNextFeature();
        if( poSrcFeature == NULL )
        {
            iCurLayer ++;
            if( iCurLayer < nSrcLayers )
            {
                ConfigureActiveLayer();
                continue;
            }
            else
                break;
        }

        OGRFeature* poFeature = TranslateFromSrcLayer(poSrcFeature);
        delete poSrcFeature;

        if( (m_poFilterGeom == NULL ||
             FilterGeometry( poFeature->GetGeometryRef() ) ) &&
            (m_poAttrQuery == NULL ||
             m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }
    return NULL;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRUnionLayer::GetFeature( long nFeatureId )
{
    if( !bPreserveSrcFID )
        return OGRLayer::GetFeature(nFeatureId);

    for(int i=0;i<nSrcLayers;i++)
    {
        iCurLayer = i;
        ConfigureActiveLayer();

        OGRFeature* poSrcFeature = papoSrcLayers[i]->GetFeature(nFeatureId);
        if( poSrcFeature != NULL )
        {
            OGRFeature* poFeature = TranslateFromSrcLayer(poSrcFeature);
            delete poSrcFeature;

            return poFeature;
        }
    }

    ResetReading();

    return NULL;
}

/************************************************************************/
/*                          CreateFeature()                             */
/************************************************************************/

OGRErr OGRUnionLayer::CreateFeature( OGRFeature* poFeature )
{
    if( osSourceLayerFieldName.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() not supported when SourceLayerFieldName is not set");
        return OGRERR_FAILURE;
    }

    if( poFeature->GetFID() != OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() not supported when FID is set");
        return OGRERR_FAILURE;
    }

    if( !poFeature->IsFieldSet(0) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() not supported when '%s' field is not set",
                 osSourceLayerFieldName.c_str());
        return OGRERR_FAILURE;
    }

    const char* pszSrcLayerName = poFeature->GetFieldAsString(0);
    for(int i=0;i<nSrcLayers;i++)
    {
        if( strcmp(pszSrcLayerName, papoSrcLayers[i]->GetName()) == 0)
        {
            pabModifiedLayers[i] = TRUE;

            OGRFeature* poSrcFeature =
                        new OGRFeature(papoSrcLayers[i]->GetLayerDefn());
            poSrcFeature->SetFrom(poFeature, TRUE);
            OGRErr eErr = papoSrcLayers[i]->CreateFeature(poSrcFeature);
            if( eErr == OGRERR_NONE )
                poFeature->SetFID(poSrcFeature->GetFID());
            delete poSrcFeature;
            return eErr;
        }
    }

    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateFeature() not supported : '%s' source layer does not exist",
             pszSrcLayerName);
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRUnionLayer::SetFeature( OGRFeature* poFeature )
{
    if( !bPreserveSrcFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when PreserveSrcFID is OFF");
        return OGRERR_FAILURE;
    }

    if( osSourceLayerFieldName.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when SourceLayerFieldName is not set");
        return OGRERR_FAILURE;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when FID is not set");
        return OGRERR_FAILURE;
    }

    if( !poFeature->IsFieldSet(0) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when '%s' field is not set",
                 osSourceLayerFieldName.c_str());
        return OGRERR_FAILURE;
    }

    const char* pszSrcLayerName = poFeature->GetFieldAsString(0);
    for(int i=0;i<nSrcLayers;i++)
    {
        if( strcmp(pszSrcLayerName, papoSrcLayers[i]->GetName()) == 0)
        {
            pabModifiedLayers[i] = TRUE;

            OGRFeature* poSrcFeature =
                        new OGRFeature(papoSrcLayers[i]->GetLayerDefn());
            poSrcFeature->SetFrom(poFeature, TRUE);
            poSrcFeature->SetFID(poFeature->GetFID());
            OGRErr eErr = papoSrcLayers[i]->SetFeature(poSrcFeature);
            delete poSrcFeature;
            return eErr;
        }
    }

    CPLError(CE_Failure, CPLE_NotSupported,
             "SetFeature() not supported : '%s' source layer does not exist",
             pszSrcLayerName);
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRUnionLayer::GetSpatialRef()
{
    if( bSRSSet )
        return poSRS;
    else
        return papoSrcLayers[0]->GetSpatialRef();
}

/************************************************************************/
/*                      GetAttrFilterPassThroughValue()                 */
/************************************************************************/

int OGRUnionLayer::GetAttrFilterPassThroughValue()
{
    if( m_poAttrQuery == NULL )
        return TRUE;

    if( bAttrFilterPassThroughValue >= 0)
        return bAttrFilterPassThroughValue;

    char** papszUsedFields = m_poAttrQuery->GetUsedFields();
    int bRet = TRUE;

    for(int iLayer = 0; iLayer < nSrcLayers; iLayer++)
    {
        OGRFeatureDefn* poSrcFeatureDefn =
                                papoSrcLayers[iLayer]->GetLayerDefn();
        char** papszIter = papszUsedFields;
        while( papszIter != NULL && *papszIter != NULL )
        {
            int bIsSpecial = FALSE;
            for(int i = 0; i < SPECIAL_FIELD_COUNT; i++)
            {
                if( EQUAL(*papszIter, SpecialFieldNames[i]) )
                {
                    bIsSpecial = TRUE;
                    break;
                }
            }
            if( !bIsSpecial &&
                poSrcFeatureDefn->GetFieldIndex(*papszIter) < 0 )
            {
                bRet = FALSE;
                break;
            }
            papszIter ++;
        }
    }

    CSLDestroy(papszUsedFields);

    bAttrFilterPassThroughValue = bRet;

    return bRet;
}

/************************************************************************/
/*                  ApplyAttributeFilterToSrcLayer()                    */
/************************************************************************/

void OGRUnionLayer::ApplyAttributeFilterToSrcLayer(int iSubLayer)
{
    CPLAssert(iSubLayer >= 0 && iSubLayer < nSrcLayers);

    if( GetAttrFilterPassThroughValue() )
        papoSrcLayers[iSubLayer]->SetAttributeFilter(pszAttributeFilter);
    else
        papoSrcLayers[iSubLayer]->SetAttributeFilter(NULL);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRUnionLayer::GetFeatureCount( int bForce )
{
    if (nFeatureCount >= 0 &&
        m_poFilterGeom == NULL && m_poAttrQuery == NULL)
    {
        return nFeatureCount;
    }

    if( !GetAttrFilterPassThroughValue() )
        return OGRLayer::GetFeatureCount(bForce);

    int nRet = 0;
    for(int i = 0; i < nSrcLayers; i++)
    {
        AutoWarpLayerIfNecessary(i);
        ApplyAttributeFilterToSrcLayer(i);
        papoSrcLayers[i]->SetSpatialFilter(m_poFilterGeom);
        nRet += papoSrcLayers[i]->GetFeatureCount(bForce);
    }
    ResetReading();
    return nRet;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRUnionLayer::SetAttributeFilter( const char * pszAttributeFilterIn )
{
    if( pszAttributeFilterIn == NULL && pszAttributeFilter == NULL)
        return OGRERR_NONE;
    if( pszAttributeFilterIn != NULL && pszAttributeFilter != NULL &&
        strcmp(pszAttributeFilterIn, pszAttributeFilter) == 0)
        return OGRERR_NONE;

    if( poFeatureDefn == NULL ) GetLayerDefn();

    bAttrFilterPassThroughValue = -1;

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszAttributeFilterIn);
    if( eErr != OGRERR_NONE )
        return eErr;

    CPLFree(pszAttributeFilter);
    pszAttributeFilter = pszAttributeFilterIn ?
                                CPLStrdup(pszAttributeFilterIn) : NULL;

    if( iCurLayer >= 0 && iCurLayer < nSrcLayers)
        ApplyAttributeFilterToSrcLayer(iCurLayer);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int  OGRUnionLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap, OLCFastFeatureCount ) )
    {
        if( nFeatureCount >= 0 &&
            m_poFilterGeom == NULL && m_poAttrQuery == NULL )
            return TRUE;

        if( !GetAttrFilterPassThroughValue() )
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            AutoWarpLayerIfNecessary(i);
            ApplyAttributeFilterToSrcLayer(i);
            papoSrcLayers[i]->SetSpatialFilter(m_poFilterGeom);
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCFastGetExtent ) )
    {
        if( sStaticEnvelope.IsInit() )
            return TRUE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            AutoWarpLayerIfNecessary(i);
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCFastSpatialFilter) )
    {
        for(int i = 0; i < nSrcLayers; i++)
        {
            AutoWarpLayerIfNecessary(i);
            ApplyAttributeFilterToSrcLayer(i);
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCRandomRead ) )
    {
        if( !bPreserveSrcFID )
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCRandomWrite ) )
    {
        if( !bPreserveSrcFID || osSourceLayerFieldName.size() == 0)
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCSequentialWrite ) )
    {
        if( osSourceLayerFieldName.size() == 0)
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCIgnoreFields) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRUnionLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if( iGeomField == 0 )
    {
        return GetExtent(psExtent, bForce);
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OGRUnionLayer::GetExtent(iGeomField, poGeom, bForce) with "
                 "iGeomField != 0 not supported");
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRUnionLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if( sStaticEnvelope.IsInit() )
    {
        memcpy(psExtent, &sStaticEnvelope, sizeof(OGREnvelope));
        return OGRERR_NONE;
    }

    int bInit = FALSE;
    for(int i = 0; i < nSrcLayers; i++)
    {
        AutoWarpLayerIfNecessary(i);
        if( !bInit )
        {
            if( papoSrcLayers[i]->GetExtent(psExtent, bForce) == OGRERR_NONE )
                bInit = TRUE;
        }
        else
        {
            OGREnvelope sExtent;
            if( papoSrcLayers[i]->GetExtent(&sExtent, bForce) == OGRERR_NONE )
            {
                psExtent->Merge(sExtent);
            }
        }
    }
    return (bInit) ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRUnionLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    OGRLayer::SetSpatialFilter(poGeomIn);

    if( iCurLayer >= 0 && iCurLayer < nSrcLayers)
        papoSrcLayers[iCurLayer]->SetSpatialFilter(poGeomIn);
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRUnionLayer::SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
{
    if( iGeomField == 0 )
        SetSpatialFilter(poGeom);
    else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OGRUnionLayer::SetSpatialFilter(iGeomField, poGeom) with "
                 "iGeomField != 0 not supported");
}


/************************************************************************/
/*                        TranslateFromSrcLayer()                       */
/************************************************************************/

OGRFeature* OGRUnionLayer::TranslateFromSrcLayer(OGRFeature* poSrcFeature)
{
    CPLAssert(panMap != NULL);
    CPLAssert(iCurLayer >= 0 && iCurLayer < nSrcLayers);

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetFrom(poSrcFeature, panMap, TRUE);

    if( osSourceLayerFieldName.size() &&
        !poFeatureDefn->GetFieldDefn(0)->IsIgnored() )
    {
        poFeature->SetField(0, papoSrcLayers[iCurLayer]->GetName());
    }

    if( poFeatureDefn->IsGeometryIgnored() )
        poFeature->SetGeometryDirectly(NULL);
    else
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if( poGeom != NULL )
            poGeom->assignSpatialReference(GetSpatialRef());
    }

    if( bPreserveSrcFID )
        poFeature->SetFID(poSrcFeature->GetFID());
    else
        poFeature->SetFID(nNextFID ++);
    return poFeature;
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr OGRUnionLayer::SetIgnoredFields( const char **papszFields )
{
    OGRErr eErr = OGRLayer::SetIgnoredFields(papszFields);
    if( eErr != OGRERR_NONE )
        return eErr;

    CSLDestroy(papszIgnoredFields);
    papszIgnoredFields = papszFields ? CSLDuplicate((char**)papszFields) : NULL;

    return eErr;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRUnionLayer::SyncToDisk()
{
    for(int i = 0; i < nSrcLayers; i++)
    {
        if (pabModifiedLayers[i])
        {
            papoSrcLayers[i]->SyncToDisk();
            pabModifiedLayers[i] = FALSE;
        }
    }

    return OGRERR_NONE;
}
