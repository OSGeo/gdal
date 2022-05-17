/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Implements OGRFlatGeobufEditableLayer class.
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2020, Björn Harrtell <bjorn at wololo dot org>
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

#include "ogrsf_frmts.h"
#include "cpl_vsi_virtual.h"
#include "cpl_conv.h"
#include "cpl_json.h"
#include "cpl_http.h"
#include "ogr_p.h"

#include "ogr_flatgeobuf.h"
#include "geometryreader.h"
#include "geometrywriter.h"

#include <algorithm>
#include <stdexcept>

using namespace flatbuffers;
using namespace FlatGeobuf;
using namespace ogr_flatgeobuf;

class OGRFlatGeobufEditableLayerSynchronizer final: public IOGREditableLayerSynchronizer
{
    OGRFlatGeobufLayer *m_poFlatGeobufLayer;
    char        **m_papszOpenOptions;

  public:
    OGRFlatGeobufEditableLayerSynchronizer(OGRFlatGeobufLayer *poFlatGeobufLayer,
                                    char **papszOpenOptions) :
        m_poFlatGeobufLayer(poFlatGeobufLayer),
        m_papszOpenOptions(CSLDuplicate(papszOpenOptions)) { }
    virtual ~OGRFlatGeobufEditableLayerSynchronizer() override;

    virtual OGRErr EditableSyncToDisk(OGRLayer *poEditableLayer,
                                      OGRLayer **ppoDecoratedLayer) override;
};

OGRFlatGeobufEditableLayerSynchronizer::~OGRFlatGeobufEditableLayerSynchronizer()
{
    CSLDestroy(m_papszOpenOptions);
}

OGRErr OGRFlatGeobufEditableLayerSynchronizer::EditableSyncToDisk(
    OGRLayer *poEditableLayer, OGRLayer **ppoDecoratedLayer)
{
    CPLDebugOnly("FlatGeobuf", "EditableSyncToDisk called");

    CPLAssert(m_poFlatGeobufLayer == *ppoDecoratedLayer);

    const CPLString osLayerName(m_poFlatGeobufLayer->GetName());
    const CPLString osFilename(m_poFlatGeobufLayer->GetFilename());
    VSIStatBufL sStatBuf;
    CPLString osTmpFilename(osFilename);
    if( VSIStatL(osFilename, &sStatBuf) == 0 )
    {
        osTmpFilename += "_ogr_tmp.fgb";
    }
    OGRSpatialReference *spatialRef = m_poFlatGeobufLayer->GetSpatialRef();
    auto gType = m_poFlatGeobufLayer->getOGRwkbGeometryType();
    auto createIndex = m_poFlatGeobufLayer->GetIndexNodeSize() > 0;

    OGRFlatGeobufLayer *poFlatGeobufTmpLayer = OGRFlatGeobufLayer::Create(
        osLayerName.c_str(), osTmpFilename.c_str(), spatialRef, gType, createIndex, m_papszOpenOptions);
    if( poFlatGeobufTmpLayer == nullptr )
        return OGRERR_FAILURE;

    OGRErr eErr = OGRERR_NONE;
    OGRFeatureDefn *poEditableFDefn = poEditableLayer->GetLayerDefn();
    for (int i = 0; eErr == OGRERR_NONE && i < poEditableFDefn->GetFieldCount();
         i++ )
    {
        OGRFieldDefn oFieldDefn(poEditableFDefn->GetFieldDefn(i));
        eErr = poFlatGeobufTmpLayer->CreateField(&oFieldDefn);
    }

    poEditableLayer->ResetReading();

    // Disable all filters.
    const char* pszQueryStringConst = poEditableLayer->GetAttrQueryString();
    char* pszQueryStringBak = pszQueryStringConst ? CPLStrdup(pszQueryStringConst) : nullptr;
    poEditableLayer->SetAttributeFilter(nullptr);

    const int iFilterGeomIndexBak = poEditableLayer->GetGeomFieldFilter();
    OGRGeometry* poFilterGeomBak = poEditableLayer->GetSpatialFilter();
    if( poFilterGeomBak )
        poFilterGeomBak = poFilterGeomBak->clone();
    poEditableLayer->SetSpatialFilter(nullptr);

    auto aoMapSrcToTargetIdx = poFlatGeobufTmpLayer->GetLayerDefn()->
        ComputeMapForSetFrom(poEditableLayer->GetLayerDefn(), true);
    aoMapSrcToTargetIdx.push_back(-1); // add dummy entry to be sure that .data() is valid

    for( auto&& poFeature: poEditableLayer )
    {
        if( eErr != OGRERR_NONE )
            break;
        OGRFeature *poNewFeature =
            new OGRFeature(poFlatGeobufTmpLayer->GetLayerDefn());
        poNewFeature->SetFrom(poFeature.get(), aoMapSrcToTargetIdx.data(), true);
        eErr = poFlatGeobufTmpLayer->CreateFeature(poNewFeature);
        delete poNewFeature;
    }
    delete poFlatGeobufTmpLayer;

    // Restore filters.
    poEditableLayer->SetAttributeFilter(pszQueryStringBak);
    CPLFree(pszQueryStringBak);
    poEditableLayer->SetSpatialFilter(iFilterGeomIndexBak, poFilterGeomBak);
    delete poFilterGeomBak;

    if( eErr != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error while creating %s",
                 osTmpFilename.c_str());
        VSIUnlink(osTmpFilename);
        return eErr;
    }

    delete m_poFlatGeobufLayer;
    *ppoDecoratedLayer = nullptr;
    m_poFlatGeobufLayer = nullptr;

    if( osFilename != osTmpFilename )
    {
        const CPLString osTmpOriFilename(osFilename + ".ogr_bak");
        if (VSIRename(osFilename, osTmpOriFilename) != 0 ||
            VSIRename(osTmpFilename, osFilename) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot rename files");
            return OGRERR_FAILURE;
        }
        VSIUnlink(osTmpOriFilename);
    }

    VSILFILE *fp = VSIFOpenL(osFilename, "rb+");
    if( fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen updated %s",
                 osFilename.c_str());
        return OGRERR_FAILURE;
    }

    m_poFlatGeobufLayer = OGRFlatGeobufLayer::Open(osFilename.c_str(), fp, false);
    *ppoDecoratedLayer = m_poFlatGeobufLayer;

    return OGRERR_NONE;
}

OGRFlatGeobufEditableLayer::OGRFlatGeobufEditableLayer(OGRFlatGeobufLayer *poFlatGeobufLayer,
                                         char **papszOpenOptions) :
    OGREditableLayer(poFlatGeobufLayer, true,
                     new OGRFlatGeobufEditableLayerSynchronizer(
                         poFlatGeobufLayer, papszOpenOptions),
                     true)
{
}

GIntBig OGRFlatGeobufEditableLayer::GetFeatureCount( int bForce )
{
    const GIntBig nRet = OGREditableLayer::GetFeatureCount(bForce);
    if( m_poDecoratedLayer != nullptr && m_nNextFID <= 0 )
    {
        const GIntBig nTotalFeatureCount =
            static_cast<OGRFlatGeobufLayer *>(m_poDecoratedLayer)
                ->GetFeatureCount(false);
        if( nTotalFeatureCount >= 0 )
            SetNextFID(nTotalFeatureCount + 1);
    }
    return nRet;
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRFlatGeobufEditableLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap, OLCSequentialWrite) ||
        EQUAL(pszCap, OLCRandomWrite) ||
        EQUAL(pszCap, OLCCreateField) ||
        EQUAL(pszCap, OLCDeleteField) ||
        EQUAL(pszCap, OLCReorderFields) ||
        EQUAL(pszCap, OLCAlterFieldDefn) ||
        EQUAL(pszCap, OLCDeleteFeature) )
    {
        return TRUE;
    }

    return OGREditableLayer::TestCapability(pszCap);
}
