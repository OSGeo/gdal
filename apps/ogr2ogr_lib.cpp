/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2015, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2015, Faza Mahamood
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
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_featurestyle.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrlayerdecorator.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

typedef enum
{
    GEOMOP_NONE,
    GEOMOP_SEGMENTIZE,
    GEOMOP_SIMPLIFY_PRESERVE_TOPOLOGY,
} GeomOperation;

typedef enum
{
    GTC_DEFAULT,
    GTC_PROMOTE_TO_MULTI,
    GTC_CONVERT_TO_LINEAR,
    GTC_CONVERT_TO_CURVE,
    GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR,
} GeomTypeConversion;

#define GEOMTYPE_UNCHANGED  -2

#define COORD_DIM_UNCHANGED -1
#define COORD_DIM_LAYER_DIM -2
#define COORD_DIM_XYM -3

/************************************************************************/
/*                        GDALVectorTranslateOptions                    */
/************************************************************************/

/** Options for use with GDALVectorTranslate(). GDALVectorTranslateOptions* must be allocated and
 * freed with GDALVectorTranslateOptionsNew() and GDALVectorTranslateOptionsFree() respectively.
 */
struct GDALVectorTranslateOptions
{
    /*! continue after a failure, skipping the failed feature */
    bool bSkipFailures;

    /*! use layer level transaction. If set to FALSE, then it is interpreted as dataset level transaction. */
    int nLayerTransaction;

    /*! force the use of particular transaction type based on GDALVectorTranslate::nLayerTransaction */
    bool bForceTransaction;

    /*! group nGroupTransactions features per transaction (default 20000). Increase the value for better
        performance when writing into DBMS drivers that have transaction support. nGroupTransactions can
        be set to -1 to load the data into a single transaction */
    int nGroupTransactions;

    /*! If provided, only the feature with this feature id will be reported. Operates exclusive of
        the spatial or attribute queries. Note: if you want to select several features based on their
        feature id, you can also use the fact the 'fid' is a special field recognized by OGR SQL.
        So GDALVectorTranslateOptions::pszWHERE = "fid in (1,3,5)" would select features 1, 3 and 5. */
    GIntBig nFIDToFetch;

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet;

    /*! output file format name */
    char *pszFormat;

    /*! list of layers of the source dataset which needs to be selected */
    char **papszLayers;

    /*! dataset creation option (format specific) */
    char **papszDSCO;

    /*! layer creation option (format specific) */
    char **papszLCO;

    /*! access modes */
    GDALVectorTranslateAccessMode eAccessMode;

    /*! It has the effect of adding, to existing target layers, the new fields found in source layers.
        This option is useful when merging files that have non-strictly identical structures. This might
        not work for output formats that don't support adding fields to existing non-empty layers. */
    bool bAddMissingFields;

    /*! It must be set to true to trigger reprojection, otherwise only SRS assignment is done. */
    bool bTransform;

    /*! output SRS. GDALVectorTranslateOptions::bTransform must be set to true to trigger reprojection,
        otherwise only SRS assignment is done. */
    char *pszOutputSRSDef;

    /*! Coordinate epoch of source SRS */
    double dfSourceCoordinateEpoch;

    /*! Coordinate epoch of output SRS */
    double dfOutputCoordinateEpoch;

    /*! override source SRS */
    char *pszSourceSRSDef;

    /*! PROJ pipeline */
    char *pszCTPipeline;

    bool bNullifyOutputSRS;

    /*! If set to false, then field name matching between source and existing target layer is done
        in a more relaxed way if the target driver has an implementation for it. */
    bool bExactFieldNameMatch;

    /*! an alternate name to the new layer */
    char *pszNewLayerName;

    /*! attribute query (like SQL WHERE) */
    char *pszWHERE;

    /*! name of the geometry field on which the spatial filter operates on. */
    char *pszGeomField;

    /*! list of fields from input layer to copy to the new layer. A field is skipped if
        mentioned previously in the list even if the input layer has duplicate field names.
        (Defaults to all; any field is skipped if a subsequent field with same name is
        found.) Geometry fields can also be specified in the list. */
    char **papszSelFields;

    /*! SQL statement to execute. The resulting table/layer will be saved to the output. */
    char *pszSQLStatement;

    /*! SQL dialect. In some cases can be used to use (unoptimized) OGR SQL instead of the
        native SQL of an RDBMS by using "OGRSQL". The "SQLITE" dialect can also be used with
        any datasource. */
    char *pszDialect;

    /*! the geometry type for the created layer */
    int eGType;

    GeomTypeConversion eGeomTypeConversion;

    /*! Geometric operation to perform */
    GeomOperation eGeomOp;

    /*! the parameter to geometric operation */
    double dfGeomOpParam;

    /*! Whether to run MakeValid */
    bool bMakeValid;

    /*! list of field types to convert to a field of type string in the destination layer.
        Valid types are: Integer, Integer64, Real, String, Date, Time, DateTime, Binary,
        IntegerList, Integer64List, RealList, StringList. Special value "All" can be
        used to convert all fields to strings. This is an alternate way to using the CAST
        operator of OGR SQL, that may avoid typing a long SQL query. Note that this does
        not influence the field types used by the source driver, and is only an afterwards
        conversion. */
    char **papszFieldTypesToString;

    /*! list of field types and the field type after conversion in the destination layer.
        ("srctype1=dsttype1","srctype2=dsttype2",...).
        Valid types are : Integer, Integer64, Real, String, Date, Time, DateTime, Binary,
        IntegerList, Integer64List, RealList, StringList. Types can also include subtype
        between parenthesis, such as Integer(Boolean), Real(Float32), ... Special value
        "All" can be used to convert all fields to another type. This is an alternate way to
        using the CAST operator of OGR SQL, that may avoid typing a long SQL query.
        This is a generalization of GDALVectorTranslateOptions::papszFieldTypeToString. Note that this does not influence
        the field types used by the source driver, and is only an afterwards conversion. */
    char **papszMapFieldType;

    /*! set field width and precision to 0 */
    bool bUnsetFieldWidth;

    /*! display progress on terminal. Only works if input layers have the "fast feature count"
    capability */
    bool bDisplayProgress;

    /*! split geometries crossing the dateline meridian */
    bool bWrapDateline;

    /*! offset from dateline in degrees (default long. = +/- 10deg, geometries
    within 170deg to -170deg will be split) */
    double dfDateLineOffset;

    /*! clip geometries when it is set to true */
    bool bClipSrc;

    OGRGeometryH hClipSrc;

    /*! clip datasource */
    char *pszClipSrcDS;

    /*! select desired geometries using an SQL query */
    char *pszClipSrcSQL;

    /*! selected named layer from the source clip datasource */
    char *pszClipSrcLayer;

    /*! restrict desired geometries based on attribute query */
    char *pszClipSrcWhere;

    OGRGeometryH hClipDst;

    /*! destination clip datasource */
    char *pszClipDstDS;

    /*! select desired geometries using an SQL query */
    char *pszClipDstSQL;

    /*! selected named layer from the destination clip datasource */
    char *pszClipDstLayer;

    /*! restrict desired geometries based on attribute query */
    char *pszClipDstWhere;

    /*! split fields of type StringList, RealList or IntegerList into as many fields
        of type String, Real or Integer as necessary. */
    bool bSplitListFields;

    /*! limit the number of subfields created for each split field. */
    int nMaxSplitListSubFields;

    /*! produce one feature for each geometry in any kind of geometry collection in the
        source file */
    bool bExplodeCollections;

    /*! uses the specified field to fill the Z coordinates of geometries */
    char *pszZField;

    /*! the list of field indexes to be copied from the source to the destination. The (n)th value
        specified in the list is the index of the field in the target layer definition in which the
        n(th) field of the source layer must be copied. Index count starts at zero. There must be
        exactly as many values in the list as the count of the fields in the source layer.
        We can use the "identity" option to specify that the fields should be transferred by using
        the same order. This option should be used along with the
        GDALVectorTranslateOptions::eAccessMode = ACCESS_APPEND option. */
    char **papszFieldMap;

    /*! force the coordinate dimension to nCoordDim (valid values are 2 or 3). This affects both
        the layer geometry type, and feature geometries. */
    int nCoordDim;

    /*! destination dataset open option (format specific), only valid in update mode */
    char **papszDestOpenOptions;

    /*! If set to true, does not propagate not-nullable constraints to target layer if they exist
        in source layer */
    bool bForceNullable;

    /*! If set to true, for each field with a coded field domains, create a field that contains
        the description of the coded value. */
    bool bResolveDomains;

    /*! If set to true, empty string values will be treated as null */
    bool bEmptyStrAsNull;

    /*! If set to true, does not propagate default field values to target layer if they exist in
        source layer */
    bool bUnsetDefault;

    /*! to prevent the new default behavior that consists in, if the output driver has a FID layer
        creation option and we are not in append mode, to preserve the name of the source FID column
        and source feature IDs */
    bool bUnsetFid;

    /*! use the FID of the source features instead of letting the output driver to automatically
        assign a new one. If not in append mode, this behavior becomes the default if the output
        driver has a FID layer creation option. In which case the name of the source FID column will
        be used and source feature IDs will be attempted to be preserved. This behavior can be
        disabled by option GDALVectorTranslateOptions::bUnsetFid */
    bool bPreserveFID;

    /*! set it to false to disable copying of metadata from source dataset and layers into target dataset and
        layers, when supported by output driver. */
    bool bCopyMD;

    /*! list of metadata key and value to set on the output dataset, when supported by output driver.
        ("META-TAG1=VALUE1","META-TAG2=VALUE2") */
    char **papszMetadataOptions;

    /*! override spatial filter SRS */
    char *pszSpatSRSDef;

    /*! size of the list GDALVectorTranslateOptions::pasGCPs */
    int nGCPCount;

    /*! list of ground control points to be added */
    GDAL_GCP *pasGCPs;

    /*! order of polynomial used for warping (1 to 3). The default is to select a polynomial
        order based on the number of GCPs */
    int nTransformOrder;

    /*! spatial query extents, in the SRS of the source layer(s) (or the one specified with
        GDALVectorTranslateOptions::pszSpatSRSDef). Only features whose geometry intersects the extents
        will be selected. The geometries will not be clipped unless GDALVectorTranslateOptions::bClipSrc
        is true. */
    OGRGeometryH hSpatialFilter;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;

    /*! Whether layer and feature native data must be transferred. */
    bool bNativeData;

    /*! Maximum number of features, or -1 if no limit. */
    GIntBig nLimit;
};

struct TargetLayerInfo
{
    OGRLayer *   m_poSrcLayer = nullptr;
    GIntBig      m_nFeaturesRead = 0;
    bool         m_bPerFeatureCT = 0;
    OGRLayer    *m_poDstLayer = nullptr;
    std::vector<std::unique_ptr<OGRCoordinateTransformation>> m_apoCT{};
    std::vector<CPLStringList> m_aosTransformOptions{};
    std::vector<int> m_anMap{};
    struct ResolvedInfo
    {
        int nSrcField;
        const OGRFieldDomain* poDomain;
    };
    std::map<int, ResolvedInfo> m_oMapResolved{};
    std::map<const OGRFieldDomain*, std::map<std::string, std::string>> m_oMapDomainToKV{};
    int          m_iSrcZField = -1;
    int          m_iSrcFIDField = -1;
    int          m_iRequestedSrcGeomField = -1;
    bool         m_bPreserveFID = false;
    const char  *m_pszCTPipeline = nullptr;
};

struct AssociatedLayers
{
    OGRLayer         *poSrcLayer = nullptr;
    std::unique_ptr<TargetLayerInfo> psInfo{};
};

class SetupTargetLayer
{
public:
    GDALDataset          *m_poSrcDS;
    GDALDataset          *m_poDstDS;
    char                **m_papszLCO;
    OGRSpatialReference  *m_poOutputSRS;
    bool                  m_bNullifyOutputSRS;
    char                **m_papszSelFields;
    bool                  m_bAppend;
    bool                  m_bAddMissingFields;
    int                   m_eGType;
    GeomTypeConversion    m_eGeomTypeConversion;
    int                   m_nCoordDim;
    bool                  m_bOverwrite;
    char                **m_papszFieldTypesToString;
    char                **m_papszMapFieldType;
    bool                  m_bUnsetFieldWidth;
    bool                  m_bExplodeCollections;
    const char           *m_pszZField;
    char                **m_papszFieldMap;
    const char           *m_pszWHERE;
    bool                  m_bExactFieldNameMatch;
    bool                  m_bQuiet;
    bool                  m_bForceNullable;
    bool                  m_bResolveDomains;
    bool                  m_bUnsetDefault;
    bool                  m_bUnsetFid;
    bool                  m_bPreserveFID;
    bool                  m_bCopyMD;
    bool                  m_bNativeData;
    bool                  m_bNewDataSource;
    const char           *m_pszCTPipeline;

    std::unique_ptr<TargetLayerInfo> Setup(OGRLayer * poSrcLayer,
                                      const char *pszNewLayerName,
                                      GDALVectorTranslateOptions *psOptions,
                                      GIntBig& nTotalEventsDone);
};

class LayerTranslator
{
public:
    GDALDataset                  *m_poSrcDS;
    GDALDataset                  *m_poODS;
    bool                          m_bTransform;
    bool                          m_bWrapDateline;
    CPLString                     m_osDateLineOffset;
    OGRSpatialReference          *m_poOutputSRS;
    bool                          m_bNullifyOutputSRS;
    OGRSpatialReference          *m_poUserSourceSRS;
    OGRCoordinateTransformation  *m_poGCPCoordTrans;
    int                           m_eGType;
    GeomTypeConversion            m_eGeomTypeConversion;
    bool                          m_bMakeValid;
    int                           m_nCoordDim;
    GeomOperation                 m_eGeomOp;
    double                        m_dfGeomOpParam;
    OGRGeometry                  *m_poClipSrc;
    OGRGeometry                  *m_poClipDst;
    bool                          m_bExplodeCollections;
    bool                          m_bNativeData;
    GIntBig                       m_nLimit;
    OGRGeometryFactory::TransformWithOptionsCache m_transformWithOptionsCache;

    int                 Translate(OGRFeature* poFeatureIn,
                                  TargetLayerInfo* psInfo,
                                  GIntBig nCountLayerFeatures,
                                  GIntBig* pnReadFeatureCount,
                                  GIntBig& nTotalEventsDone,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressArg,
                                  GDALVectorTranslateOptions *psOptions);
};

static OGRLayer* GetLayerAndOverwriteIfNecessary(GDALDataset *poDstDS,
                                                 const char* pszNewLayerName,
                                                 bool bOverwrite,
                                                 bool* pbErrorOccurred,
                                                 bool* pbOverwriteActuallyDone,
                                                 bool* pbAddOverwriteLCO);

/************************************************************************/
/*                           LoadGeometry()                             */
/************************************************************************/

static OGRGeometry* LoadGeometry( const char* pszDS,
                                  const char* pszSQL,
                                  const char* pszLyr,
                                  const char* pszWhere)
{
    GDALDataset *poDS =
        reinterpret_cast<GDALDataset *>(OGROpen(pszDS, FALSE, nullptr));
    if (poDS == nullptr)
        return nullptr;

    OGRLayer *poLyr = nullptr;
    if (pszSQL != nullptr)
        poLyr = poDS->ExecuteSQL( pszSQL, nullptr, nullptr );
    else if (pszLyr != nullptr)
        poLyr = poDS->GetLayerByName(pszLyr);
    else
        poLyr = poDS->GetLayer(0);

    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from datasource." );
        GDALClose(poDS);
        return nullptr;
    }

    if (pszWhere)
        poLyr->SetAttributeFilter(pszWhere);

    OGRMultiPolygon *poMP = nullptr;
    for( auto& poFeat: poLyr )
    {
        OGRGeometry* poSrcGeom = poFeat->GetGeometryRef();
        if (poSrcGeom)
        {
            const OGRwkbGeometryType eType =
                wkbFlatten(poSrcGeom->getGeometryType());

            if (poMP == nullptr)
                poMP = new OGRMultiPolygon();

            if( eType == wkbPolygon )
                poMP->addGeometry( poSrcGeom );
            else if( eType == wkbMultiPolygon )
            {
                OGRMultiPolygon* poSrcMP = poSrcGeom->toMultiPolygon();
                const int nGeomCount = poSrcMP->getNumGeometries();

                for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
                {
                    poMP->addGeometry(
                        poSrcMP->getGeometryRef(iGeom) );
                }
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Geometry not of polygon type." );
                OGRGeometryFactory::destroyGeometry(poMP);
                if( pszSQL != nullptr )
                    poDS->ReleaseResultSet( poLyr );
                GDALClose(poDS);
                return nullptr;
            }
        }
    }

    if( pszSQL != nullptr )
        poDS->ReleaseResultSet( poLyr );
    GDALClose(poDS);

    return poMP;
}

/************************************************************************/
/*                     OGRSplitListFieldLayer                           */
/************************************************************************/

typedef struct
{
    int          iSrcIndex;
    OGRFieldType eType;
    int          nMaxOccurrences;
    int          nWidth;
} ListFieldDesc;

class OGRSplitListFieldLayer : public OGRLayer
{
    OGRLayer                    *poSrcLayer;
    OGRFeatureDefn              *poFeatureDefn;
    ListFieldDesc               *pasListFields;
    int                          nListFieldCount;
    int                          nMaxSplitListSubFields;

    OGRFeature                  *TranslateFeature(OGRFeature* poSrcFeature);

  public:
                                 OGRSplitListFieldLayer(OGRLayer* poSrcLayer,
                                                        int nMaxSplitListSubFields);
                        virtual ~OGRSplitListFieldLayer();

    bool                        BuildLayerDefn(GDALProgressFunc pfnProgress,
                                                void *pProgressArg);

    virtual OGRFeature          *GetNextFeature() override;
    virtual OGRFeature          *GetFeature(GIntBig nFID) override;
    virtual OGRFeatureDefn      *GetLayerDefn() override;

    virtual void                 ResetReading() override { poSrcLayer->ResetReading(); }
    virtual int                  TestCapability(const char*) override { return FALSE; }

    virtual GIntBig              GetFeatureCount( int bForce = TRUE ) override
    {
        return poSrcLayer->GetFeatureCount(bForce);
    }

    virtual OGRSpatialReference *GetSpatialRef() override
    {
        return poSrcLayer->GetSpatialRef();
    }

    virtual OGRGeometry         *GetSpatialFilter() override
    {
        return poSrcLayer->GetSpatialFilter();
    }

    virtual OGRStyleTable       *GetStyleTable() override
    {
        return poSrcLayer->GetStyleTable();
    }

    virtual void                 SetSpatialFilter( OGRGeometry *poGeom ) override
    {
        poSrcLayer->SetSpatialFilter(poGeom);
    }

    virtual void                 SetSpatialFilter( int iGeom, OGRGeometry *poGeom ) override
    {
        poSrcLayer->SetSpatialFilter(iGeom, poGeom);
    }

    virtual void                 SetSpatialFilterRect( double dfMinX, double dfMinY,
                                                       double dfMaxX, double dfMaxY ) override
    {
        poSrcLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
    }

    virtual void                 SetSpatialFilterRect( int iGeom,
                                                       double dfMinX, double dfMinY,
                                                       double dfMaxX, double dfMaxY ) override
    {
        poSrcLayer->SetSpatialFilterRect(iGeom, dfMinX, dfMinY, dfMaxX, dfMaxY);
    }

    virtual OGRErr               SetAttributeFilter( const char *pszFilter ) override
    {
        return poSrcLayer->SetAttributeFilter(pszFilter);
    }
};

/************************************************************************/
/*                    OGRSplitListFieldLayer()                          */
/************************************************************************/

OGRSplitListFieldLayer::OGRSplitListFieldLayer(OGRLayer* poSrcLayerIn,
                                               int nMaxSplitListSubFieldsIn) :
    poSrcLayer(poSrcLayerIn),
    poFeatureDefn(nullptr),
    pasListFields(nullptr),
    nListFieldCount(0),
    nMaxSplitListSubFields(
        nMaxSplitListSubFieldsIn < 0 ? INT_MAX : nMaxSplitListSubFieldsIn)
{}

/************************************************************************/
/*                   ~OGRSplitListFieldLayer()                          */
/************************************************************************/

OGRSplitListFieldLayer::~OGRSplitListFieldLayer()
{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    CPLFree(pasListFields);
}

/************************************************************************/
/*                       BuildLayerDefn()                               */
/************************************************************************/

bool OGRSplitListFieldLayer::BuildLayerDefn(GDALProgressFunc pfnProgress,
                                            void *pProgressArg)
{
    CPLAssert(poFeatureDefn == nullptr);

    OGRFeatureDefn* poSrcFieldDefn = poSrcLayer->GetLayerDefn();

    int nSrcFields = poSrcFieldDefn->GetFieldCount();
    pasListFields = static_cast<ListFieldDesc *>(
        CPLCalloc(sizeof(ListFieldDesc), nSrcFields));
    nListFieldCount = 0;

    /* Establish the list of fields of list type */
    for( int i=0; i<nSrcFields; ++i )
    {
        OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(i)->GetType();
        if (eType == OFTIntegerList ||
            eType == OFTInteger64List ||
            eType == OFTRealList ||
            eType == OFTStringList)
        {
            pasListFields[nListFieldCount].iSrcIndex = i;
            pasListFields[nListFieldCount].eType = eType;
            if (nMaxSplitListSubFields == 1)
                pasListFields[nListFieldCount].nMaxOccurrences = 1;
            nListFieldCount++;
        }
    }

    if (nListFieldCount == 0)
        return false;

    /* No need for full scan if the limit is 1. We just to have to create */
    /* one and a single one field */
    if (nMaxSplitListSubFields != 1)
    {
        poSrcLayer->ResetReading();

        const GIntBig nFeatureCount =
            poSrcLayer->TestCapability(OLCFastFeatureCount)
            ? poSrcLayer->GetFeatureCount()
            : 0;
        GIntBig nFeatureIndex = 0;

        /* Scan the whole layer to compute the maximum number of */
        /* items for each field of list type */
        for( auto& poSrcFeature: poSrcLayer )
        {
            for( int i=0; i<nListFieldCount; ++i )
            {
                int nCount = 0;
                OGRField* psField =
                    poSrcFeature->GetRawFieldRef(pasListFields[i].iSrcIndex);
                switch(pasListFields[i].eType)
                {
                    case OFTIntegerList:
                        nCount = psField->IntegerList.nCount;
                        break;
                    case OFTRealList:
                        nCount = psField->RealList.nCount;
                        break;
                    case OFTStringList:
                    {
                        nCount = psField->StringList.nCount;
                        char** paList = psField->StringList.paList;
                        for(int j=0;j<nCount;j++)
                        {
                            int nWidth = static_cast<int>(strlen(paList[j]));
                            if (nWidth > pasListFields[i].nWidth)
                                pasListFields[i].nWidth = nWidth;
                        }
                        break;
                    }
                    default:
                        // cppcheck-suppress knownConditionTrueFalse
                        CPLAssert(false);
                        break;
                }
                if (nCount > pasListFields[i].nMaxOccurrences)
                {
                    if (nCount > nMaxSplitListSubFields)
                        nCount = nMaxSplitListSubFields;
                    pasListFields[i].nMaxOccurrences = nCount;
                }
            }

            nFeatureIndex ++;
            if (pfnProgress != nullptr && nFeatureCount != 0)
                pfnProgress(nFeatureIndex * 1.0 / nFeatureCount, "", pProgressArg);
        }
    }

    /* Now let's build the target feature definition */

    poFeatureDefn =
            OGRFeatureDefn::CreateFeatureDefn( poSrcFieldDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    for( int i=0; i < poSrcFieldDefn->GetGeomFieldCount(); ++i )
    {
        poFeatureDefn->AddGeomFieldDefn(poSrcFieldDefn->GetGeomFieldDefn(i));
    }

    int iListField = 0;
    for( int i=0;i<nSrcFields; ++i)
    {
        const OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(i)->GetType();
        if (eType == OFTIntegerList ||
            eType == OFTInteger64List ||
            eType == OFTRealList ||
            eType == OFTStringList)
        {
            const int nMaxOccurrences =
                pasListFields[iListField].nMaxOccurrences;
            const int nWidth = pasListFields[iListField].nWidth;
            iListField ++;
            if (nMaxOccurrences == 1)
            {
                OGRFieldDefn oFieldDefn(poSrcFieldDefn->GetFieldDefn(i)->GetNameRef(),
                                            (eType == OFTIntegerList) ? OFTInteger :
                                            (eType == OFTInteger64List) ? OFTInteger64 :
                                            (eType == OFTRealList) ?    OFTReal :
                                                                        OFTString);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }
            else
            {
                for(int j=0;j<nMaxOccurrences;j++)
                {
                    CPLString osFieldName;
                    osFieldName.Printf("%s%d",
                        poSrcFieldDefn->GetFieldDefn(i)->GetNameRef(), j+1);
                    OGRFieldDefn oFieldDefn(osFieldName.c_str(),
                                            (eType == OFTIntegerList) ? OFTInteger :
                                            (eType == OFTInteger64List) ? OFTInteger64 :
                                            (eType == OFTRealList) ?    OFTReal :
                                                                        OFTString);
                    oFieldDefn.SetWidth(nWidth);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
            }
        }
        else
        {
            poFeatureDefn->AddFieldDefn(poSrcFieldDefn->GetFieldDefn(i));
        }
    }

    return true;
}

/************************************************************************/
/*                       TranslateFeature()                             */
/************************************************************************/

OGRFeature *OGRSplitListFieldLayer::TranslateFeature(OGRFeature* poSrcFeature)
{
    if (poSrcFeature == nullptr)
        return nullptr;
    if (poFeatureDefn == nullptr)
        return poSrcFeature;

    OGRFeature* poFeature = OGRFeature::CreateFeature(poFeatureDefn);
    poFeature->SetFID(poSrcFeature->GetFID());
    for(int i=0;i<poFeature->GetGeomFieldCount();i++)
    {
        poFeature->SetGeomFieldDirectly(i, poSrcFeature->StealGeometry(i));
    }
    poFeature->SetStyleString(poFeature->GetStyleString());

    OGRFeatureDefn* poSrcFieldDefn = poSrcLayer->GetLayerDefn();
    int nSrcFields = poSrcFeature->GetFieldCount();
    int iDstField = 0;
    int iListField = 0;

    for( int iSrcField=0; iSrcField < nSrcFields; ++iSrcField)
    {
        const OGRFieldType eType =
            poSrcFieldDefn->GetFieldDefn(iSrcField)->GetType();
        OGRField* psField = poSrcFeature->GetRawFieldRef(iSrcField);
        switch(eType)
        {
            case OFTIntegerList:
            {
                const int nCount = std::min(nMaxSplitListSubFields,
                                            psField->IntegerList.nCount);
                int* paList = psField->IntegerList.paList;
                for( int j=0;j<nCount; ++j)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTInteger64List:
            {
                const int nCount = std::min(nMaxSplitListSubFields,
                                            psField->Integer64List.nCount);
                GIntBig* paList = psField->Integer64List.paList;
                for( int j=0; j < nCount; ++j )
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTRealList:
            {
                const int nCount = std::min(nMaxSplitListSubFields,
                                            psField->RealList.nCount);
                double* paList = psField->RealList.paList;
                for( int j=0; j < nCount; ++j )
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTStringList:
            {
                const int nCount = std::min(nMaxSplitListSubFields,
                                            psField->StringList.nCount);
                char** paList = psField->StringList.paList;
                for( int j=0; j < nCount; ++j )
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            default:
            {
                poFeature->SetField(iDstField, psField);
                iDstField ++;
                break;
            }
        }
    }

    OGRFeature::DestroyFeature(poSrcFeature);

    return poFeature;
}

/************************************************************************/
/*                       GetNextFeature()                               */
/************************************************************************/

OGRFeature *OGRSplitListFieldLayer::GetNextFeature()
{
    return TranslateFeature(poSrcLayer->GetNextFeature());
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature *OGRSplitListFieldLayer::GetFeature(GIntBig nFID)
{
    return TranslateFeature(poSrcLayer->GetFeature(nFID));
}

/************************************************************************/
/*                        GetLayerDefn()                                */
/************************************************************************/

OGRFeatureDefn* OGRSplitListFieldLayer::GetLayerDefn()
{
    if (poFeatureDefn == nullptr)
        return poSrcLayer->GetLayerDefn();
    return poFeatureDefn;
}

/************************************************************************/
/*                            GCPCoordTransformation()                  */
/*                                                                      */
/*      Apply GCP Transform to points                                   */
/************************************************************************/

class GCPCoordTransformation : public OGRCoordinateTransformation
{
    GCPCoordTransformation(const GCPCoordTransformation& other):
        hTransformArg(GDALCloneTransformer(other.hTransformArg)),
        bUseTPS(other.bUseTPS),
        poSRS(other.poSRS)
    {
        if( poSRS)
            poSRS->Reference();
    }

    GCPCoordTransformation& operator= (const GCPCoordTransformation&) = delete;

public:

    void               *hTransformArg;
    bool                 bUseTPS;
    OGRSpatialReference* poSRS;

    GCPCoordTransformation( int nGCPCount,
                            const GDAL_GCP *pasGCPList,
                            int  nReqOrder,
                            OGRSpatialReference* poSRSIn) :
        hTransformArg(nullptr),
        bUseTPS(nReqOrder < 0),
        poSRS(poSRSIn)
    {
        if( nReqOrder < 0 )
        {
            hTransformArg =
                GDALCreateTPSTransformer( nGCPCount, pasGCPList, FALSE );
        }
        else
        {
            hTransformArg =
                GDALCreateGCPTransformer( nGCPCount, pasGCPList, nReqOrder, FALSE );
        }
        if( poSRS)
            poSRS->Reference();
    }

    OGRCoordinateTransformation* Clone() const override {
        return new GCPCoordTransformation(*this);
    }

    bool IsValid() const { return hTransformArg != nullptr; }

    virtual ~GCPCoordTransformation()
    {
        if( hTransformArg != nullptr )
        {
            GDALDestroyTransformer(hTransformArg);
        }
        if( poSRS)
            poSRS->Dereference();
    }

    virtual OGRSpatialReference *GetSourceCS() override { return poSRS; }
    virtual OGRSpatialReference *GetTargetCS() override { return poSRS; }

    virtual int Transform( int nCount,
                           double *x, double *y, double *z,
                           double * /* t */,
                           int *pabSuccess ) override
    {
        if( bUseTPS )
            return GDALTPSTransform( hTransformArg, FALSE,
                                 nCount, x, y, z, pabSuccess );
        else
            return GDALGCPTransform( hTransformArg, FALSE,
                                 nCount, x, y, z, pabSuccess );
    }

    virtual OGRCoordinateTransformation* GetInverse() const override { return nullptr; }
};

/************************************************************************/
/*                            CompositeCT                               */
/************************************************************************/

class CompositeCT : public OGRCoordinateTransformation
{
    CompositeCT(const CompositeCT& other):
        poCT1(other.poCT1 ? other.poCT1->Clone(): nullptr),
        bOwnCT1(true),
        poCT2(other.poCT2 ? other.poCT2->Clone(): nullptr),
        bOwnCT2(true) {}

    CompositeCT& operator= (const CompositeCT&) = delete;

public:

    OGRCoordinateTransformation* poCT1;
    bool bOwnCT1;
    OGRCoordinateTransformation* poCT2;
    bool bOwnCT2;

    CompositeCT( OGRCoordinateTransformation* poCT1In, bool bOwnCT1In,
                 OGRCoordinateTransformation* poCT2In, bool bOwnCT2In ) :
        poCT1(poCT1In),
        bOwnCT1(bOwnCT1In),
        poCT2(poCT2In),
        bOwnCT2(bOwnCT2In)
    {}

    virtual ~CompositeCT()
    {
        if( bOwnCT1 )
            delete poCT1;
        if( bOwnCT2 )
            delete poCT2;
    }

    OGRCoordinateTransformation* Clone() const override {
        return new CompositeCT(*this);
    }

    virtual OGRSpatialReference *GetSourceCS() override
    {
        return poCT1 ? poCT1->GetSourceCS() :
               poCT2 ? poCT2->GetSourceCS() : nullptr;
    }

    virtual OGRSpatialReference *GetTargetCS() override
    {
        return poCT2 ? poCT2->GetTargetCS() :
               poCT1 ? poCT1->GetTargetCS() : nullptr;
    }

    virtual int Transform( int nCount,
                           double *x, double *y, double *z,
                           double *t,
                           int *pabSuccess ) override
    {
        int nResult = TRUE;
        if( poCT1 )
            nResult = poCT1->Transform(nCount, x, y, z, t, pabSuccess);
        if( nResult && poCT2 )
            nResult = poCT2->Transform(nCount, x, y, z, t, pabSuccess);
        return nResult;
    }

    virtual OGRCoordinateTransformation* GetInverse() const override { return nullptr; }
};

/************************************************************************/
/*                    AxisMappingCoordinateTransformation               */
/************************************************************************/

class AxisMappingCoordinateTransformation : public OGRCoordinateTransformation
{
public:

    bool bSwapXY = false;

    AxisMappingCoordinateTransformation( const std::vector<int>& mappingIn,
                                         const std::vector<int>& mappingOut )
    {
        if( mappingIn.size() >= 2 && mappingIn[0] == 1 && mappingIn[1] == 2 &&
            mappingOut.size() >= 2 && mappingOut[0] == 2 && mappingOut[1] == 1 )
        {
            bSwapXY = true;
        }
        else if( mappingIn.size() >= 2 && mappingIn[0] == 2 && mappingIn[1] == 1 &&
                 mappingOut.size() >= 2 && mappingOut[0] == 1 && mappingOut[1] == 2 )
        {
            bSwapXY = true;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                      "Unsupported axis transformation");
        }
    }

    ~AxisMappingCoordinateTransformation() override
    {
    }

    virtual OGRCoordinateTransformation *Clone() const override
    {
        return new AxisMappingCoordinateTransformation(*this);
    }

    virtual OGRSpatialReference *GetSourceCS() override
    {
        return nullptr;
    }

    virtual OGRSpatialReference *GetTargetCS() override
    {
        return nullptr;
    }

    virtual int Transform( int nCount,
                           double *x, double *y,
                           double * /*z*/,
                           double * /*t*/,
                           int *pabSuccess ) override
    {
        for(int i = 0; i < nCount; i++ )
        {
            if( pabSuccess )
                pabSuccess[i] = true;
            if( bSwapXY )
                std::swap(x[i], y[i]);
        }
        return true;
    }

    virtual OGRCoordinateTransformation* GetInverse() const override { return nullptr; }
};

/************************************************************************/
/*                        ApplySpatialFilter()                          */
/************************************************************************/

static
void ApplySpatialFilter(OGRLayer* poLayer, OGRGeometry* poSpatialFilter,
                        OGRSpatialReference* poSpatSRS,
                        const char* pszGeomField,
                        OGRSpatialReference* poSourceSRS)
{
    if( poSpatialFilter == nullptr )
        return;

   OGRGeometry* poSpatialFilterReprojected = nullptr;
   if( poSpatSRS )
   {
       poSpatialFilterReprojected = poSpatialFilter->clone();
       poSpatialFilterReprojected->assignSpatialReference(poSpatSRS);
       OGRSpatialReference* poSpatialFilterTargetSRS  = poSourceSRS ? poSourceSRS : poLayer->GetSpatialRef();
       if( poSpatialFilterTargetSRS )
           poSpatialFilterReprojected->transformTo(poSpatialFilterTargetSRS);
       else
           CPLError(CE_Warning, CPLE_AppDefined, "cannot determine layer SRS for %s.", poLayer->GetDescription());
   }

   if( pszGeomField != nullptr )
   {
       const int iGeomField =
           poLayer->GetLayerDefn()->GetGeomFieldIndex(pszGeomField);
       if( iGeomField >= 0 )
           poLayer->SetSpatialFilter( iGeomField,
               poSpatialFilterReprojected ? poSpatialFilterReprojected : poSpatialFilter );
       else
           CPLError(CE_Warning, CPLE_AppDefined,
                    "Cannot find geometry field %s.",
                    pszGeomField);
   }
   else
   {
       poLayer->SetSpatialFilter( poSpatialFilterReprojected ? poSpatialFilterReprojected : poSpatialFilter );
   }

   delete poSpatialFilterReprojected;
}

/************************************************************************/
/*                            GetFieldType()                            */
/************************************************************************/

static int GetFieldType(const char* pszArg, int* pnSubFieldType)
{
    *pnSubFieldType = OFSTNone;
    const char* pszOpenParenthesis = strchr(pszArg, '(');
    const int nLengthBeforeParenthesis =
        pszOpenParenthesis
        ? static_cast<int>(pszOpenParenthesis - pszArg)
        : static_cast<int>(strlen(pszArg));
    for( int iType = 0; iType <= static_cast<int>(OFTMaxType); iType++ )
    {
         const char* pszFieldTypeName = OGRFieldDefn::GetFieldTypeName(
             static_cast<OGRFieldType>(iType));
         if( EQUALN(pszArg,pszFieldTypeName,nLengthBeforeParenthesis) &&
             pszFieldTypeName[nLengthBeforeParenthesis] == '\0' )
         {
             if( pszOpenParenthesis != nullptr )
             {
                 *pnSubFieldType = -1;
                 CPLString osArgSubType = pszOpenParenthesis + 1;
                 if( !osArgSubType.empty() && osArgSubType.back() == ')' )
                     osArgSubType.resize(osArgSubType.size()-1);
                 for( int iSubType = 0;
                      iSubType <= static_cast<int>(OFSTMaxSubType); iSubType++ )
                 {
                     const char* pszFieldSubTypeName = OGRFieldDefn::GetFieldSubTypeName(
                         static_cast<OGRFieldSubType>(iSubType));
                     if( EQUAL( pszFieldSubTypeName, osArgSubType ) )
                     {
                         *pnSubFieldType = iSubType;
                         break;
                     }
                 }
             }
             return iType;
         }
     }
     return -1;
}

/************************************************************************/
/*                            IsNumber()                               */
/************************************************************************/

static bool IsNumber(const char* pszStr)
{
    if (*pszStr == '-' || *pszStr == '+')
        pszStr ++;
    if (*pszStr == '.')
        pszStr ++;
    return (*pszStr >= '0' && *pszStr <= '9');
}

/************************************************************************/
/*                           IsFieldType()                              */
/************************************************************************/

static bool IsFieldType(const char* pszArg)
{
    int iSubType;
    return GetFieldType(pszArg, &iSubType) >= 0 && iSubType >= 0;
}

/************************************************************************/
/*                      GDALVectorTranslateOptionsClone()               */
/************************************************************************/

static
GDALVectorTranslateOptions* GDALVectorTranslateOptionsClone(const GDALVectorTranslateOptions *psOptionsIn)
{
    GDALVectorTranslateOptions* psOptions =
        static_cast<GDALVectorTranslateOptions *>(
            CPLMalloc(sizeof(GDALVectorTranslateOptions)));
    memcpy(psOptions, psOptionsIn, sizeof(GDALVectorTranslateOptions));

    if( psOptionsIn->pszFormat) psOptions->pszFormat = CPLStrdup(psOptionsIn->pszFormat);
    if( psOptionsIn->pszOutputSRSDef ) psOptions->pszOutputSRSDef = CPLStrdup(psOptionsIn->pszOutputSRSDef);
    if( psOptionsIn->pszCTPipeline ) psOptions->pszCTPipeline = CPLStrdup(psOptionsIn->pszCTPipeline);
    if( psOptionsIn->pszSourceSRSDef ) psOptions->pszSourceSRSDef = CPLStrdup(psOptionsIn->pszSourceSRSDef);
    if( psOptionsIn->pszNewLayerName ) psOptions->pszNewLayerName = CPLStrdup(psOptionsIn->pszNewLayerName);
    if( psOptionsIn->pszWHERE ) psOptions->pszWHERE = CPLStrdup(psOptionsIn->pszWHERE);
    if( psOptionsIn->pszGeomField ) psOptions->pszGeomField = CPLStrdup(psOptionsIn->pszGeomField);
    if( psOptionsIn->pszSQLStatement ) psOptions->pszSQLStatement = CPLStrdup(psOptionsIn->pszSQLStatement);
    if( psOptionsIn->pszDialect ) psOptions->pszDialect = CPLStrdup(psOptionsIn->pszDialect);
    if( psOptionsIn->pszClipSrcDS ) psOptions->pszClipSrcDS = CPLStrdup(psOptionsIn->pszClipSrcDS);
    if( psOptionsIn->pszClipSrcSQL ) psOptions->pszClipSrcSQL = CPLStrdup(psOptionsIn->pszClipSrcSQL);
    if( psOptionsIn->pszClipSrcLayer ) psOptions->pszClipSrcLayer = CPLStrdup(psOptionsIn->pszClipSrcLayer);
    if( psOptionsIn->pszClipSrcWhere ) psOptions->pszClipSrcWhere = CPLStrdup(psOptionsIn->pszClipSrcWhere);
    if( psOptionsIn->pszClipDstDS ) psOptions->pszClipDstDS = CPLStrdup(psOptionsIn->pszClipDstDS);
    if( psOptionsIn->pszClipDstSQL ) psOptions->pszClipDstSQL = CPLStrdup(psOptionsIn->pszClipDstSQL);
    if( psOptionsIn->pszClipDstLayer ) psOptions->pszClipDstLayer = CPLStrdup(psOptionsIn->pszClipDstLayer);
    if( psOptionsIn->pszClipDstWhere ) psOptions->pszClipDstWhere = CPLStrdup(psOptionsIn->pszClipDstWhere);
    if( psOptionsIn->pszZField ) psOptions->pszZField = CPLStrdup(psOptionsIn->pszZField);
    if( psOptionsIn->pszSpatSRSDef ) psOptions->pszSpatSRSDef = CPLStrdup(psOptionsIn->pszSpatSRSDef);
    psOptions->papszSelFields = CSLDuplicate(psOptionsIn->papszSelFields);
    psOptions->papszFieldMap = CSLDuplicate(psOptionsIn->papszFieldMap);
    psOptions->papszMapFieldType = CSLDuplicate(psOptionsIn->papszMapFieldType);
    psOptions->papszLayers = CSLDuplicate(psOptionsIn->papszLayers);
    psOptions->papszDSCO = CSLDuplicate(psOptionsIn->papszDSCO);
    psOptions->papszLCO = CSLDuplicate(psOptionsIn->papszLCO);
    psOptions->papszDestOpenOptions = CSLDuplicate(psOptionsIn->papszDestOpenOptions);
    psOptions->papszFieldTypesToString = CSLDuplicate(psOptionsIn->papszFieldTypesToString);
    psOptions->papszMetadataOptions = CSLDuplicate(psOptionsIn->papszMetadataOptions);
    if( psOptionsIn->nGCPCount )
        psOptions->pasGCPs = GDALDuplicateGCPs( psOptionsIn->nGCPCount, psOptionsIn->pasGCPs );
    psOptions->hClipSrc = ( psOptionsIn->hClipSrc != nullptr ) ? OGR_G_Clone(psOptionsIn->hClipSrc) : nullptr;
    psOptions->hClipDst = ( psOptionsIn->hClipDst != nullptr ) ? OGR_G_Clone(psOptionsIn->hClipDst) : nullptr;
    psOptions->hSpatialFilter = ( psOptionsIn->hSpatialFilter != nullptr ) ? OGR_G_Clone(psOptionsIn->hSpatialFilter) : nullptr;

    return psOptions;
}

class GDALVectorTranslateWrappedDataset: public GDALDataset
{
                GDALDataset* m_poBase;
                OGRSpatialReference* m_poOutputSRS;
                bool m_bTransform;

                std::vector<OGRLayer*> m_apoLayers;
                std::vector<OGRLayer*> m_apoHiddenLayers;

                GDALVectorTranslateWrappedDataset(
                                    GDALDataset* poBase,
                                    OGRSpatialReference* poOutputSRS,
                                    bool bTransform);
public:

       virtual ~GDALVectorTranslateWrappedDataset();

       virtual int GetLayerCount() override
                        { return static_cast<int>(m_apoLayers.size()); }
       virtual OGRLayer* GetLayer(int nIdx) override;
       virtual OGRLayer* GetLayerByName(const char* pszName) override;

       virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect ) override;
       virtual void        ReleaseResultSet( OGRLayer * poResultsSet ) override;

       static GDALVectorTranslateWrappedDataset* New(
                                          GDALDataset* poBase,
                                          OGRSpatialReference* poOutputSRS,
                                          bool bTransform );
};

class GDALVectorTranslateWrappedLayer: public OGRLayerDecorator
{
    std::vector<OGRCoordinateTransformation*> m_apoCT;
    OGRFeatureDefn* m_poFDefn;

            GDALVectorTranslateWrappedLayer(OGRLayer* poBaseLayer,
                                            bool bOwnBaseLayer);
            OGRFeature* TranslateFeature(OGRFeature* poSrcFeat);
public:

        virtual ~GDALVectorTranslateWrappedLayer();
        virtual OGRFeatureDefn* GetLayerDefn() override { return m_poFDefn; }
        virtual OGRFeature* GetNextFeature() override;
        virtual OGRFeature* GetFeature(GIntBig nFID) override;

        static GDALVectorTranslateWrappedLayer* New(
                                        OGRLayer* poBaseLayer,
                                        bool bOwnBaseLayer,
                                        OGRSpatialReference* poOutputSRS,
                                        bool bTransform);
};

GDALVectorTranslateWrappedLayer::GDALVectorTranslateWrappedLayer(
                    OGRLayer* poBaseLayer, bool bOwnBaseLayer) :
        OGRLayerDecorator(poBaseLayer, bOwnBaseLayer),
        m_apoCT( poBaseLayer->GetLayerDefn()->GetGeomFieldCount(),
                 static_cast<OGRCoordinateTransformation*>(nullptr) ),
        m_poFDefn( nullptr )
{
}

GDALVectorTranslateWrappedLayer* GDALVectorTranslateWrappedLayer::New(
                    OGRLayer* poBaseLayer,
                    bool bOwnBaseLayer,
                    OGRSpatialReference* poOutputSRS,
                    bool bTransform )
{
    GDALVectorTranslateWrappedLayer* poNew =
                new GDALVectorTranslateWrappedLayer(poBaseLayer, bOwnBaseLayer);
    poNew->m_poFDefn = poBaseLayer->GetLayerDefn()->Clone();
    poNew->m_poFDefn->Reference();
    if( !poOutputSRS )
        return poNew;

    for( int i=0; i < poNew->m_poFDefn->GetGeomFieldCount(); i++ )
    {
        if( bTransform )
        {
            OGRSpatialReference* poSourceSRS =
                poBaseLayer->GetLayerDefn()->
                                    GetGeomFieldDefn(i)->GetSpatialRef();
            if( poSourceSRS == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer %s has no source SRS for geometry field %s",
                         poBaseLayer->GetName(),
                         poBaseLayer->GetLayerDefn()->
                                    GetGeomFieldDefn(i)->GetNameRef());
                delete poNew;
                return nullptr;
            }
            else
            {
                poNew->m_apoCT[i] = OGRCreateCoordinateTransformation(
                                                               poSourceSRS,
                                                               poOutputSRS);
                if( poNew->m_apoCT[i] == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                        "Failed to create coordinate transformation between the\n"
                        "following coordinate systems.  This may be because they\n"
                        "are not transformable." );

                    char *pszWKT = nullptr;
                    poSourceSRS->exportToPrettyWkt( &pszWKT, FALSE );
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Source:\n%s", pszWKT );
                    CPLFree(pszWKT);

                    poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Target:\n%s", pszWKT );
                    CPLFree(pszWKT);

                    delete poNew;
                    return nullptr;
                }
            }
        }
        poNew->m_poFDefn->GetGeomFieldDefn(i)->SetSpatialRef( poOutputSRS );
    }

    return poNew;
}

GDALVectorTranslateWrappedLayer::~GDALVectorTranslateWrappedLayer()
{
    if( m_poFDefn )
        m_poFDefn->Release();
    for( size_t i = 0; i < m_apoCT.size(); ++i )
        delete m_apoCT[i];
}

OGRFeature* GDALVectorTranslateWrappedLayer::GetNextFeature()
{
    return TranslateFeature(OGRLayerDecorator::GetNextFeature());
}

OGRFeature* GDALVectorTranslateWrappedLayer::GetFeature(GIntBig nFID)
{
    return TranslateFeature(OGRLayerDecorator::GetFeature(nFID));
}

OGRFeature* GDALVectorTranslateWrappedLayer::TranslateFeature(
                                                    OGRFeature* poSrcFeat )
{
    if( poSrcFeat == nullptr )
        return nullptr;
    OGRFeature* poNewFeat = new OGRFeature(m_poFDefn);
    poNewFeat->SetFrom(poSrcFeat);
    poNewFeat->SetFID(poSrcFeat->GetFID());
    for( int i=0; i < poNewFeat->GetGeomFieldCount(); i++ )
    {
        OGRGeometry* poGeom = poNewFeat->GetGeomFieldRef(i);
        if( poGeom )
        {
            if( m_apoCT[i] )
                poGeom->transform( m_apoCT[i] );
            poGeom->assignSpatialReference(
                    m_poFDefn->GetGeomFieldDefn(i)->GetSpatialRef() );
        }
    }
    delete poSrcFeat;
    return poNewFeat;
}


GDALVectorTranslateWrappedDataset::GDALVectorTranslateWrappedDataset(
                                    GDALDataset* poBase,
                                    OGRSpatialReference* poOutputSRS,
                                    bool bTransform):
                                                m_poBase(poBase),
                                                m_poOutputSRS(poOutputSRS),
                                                m_bTransform(bTransform)
{
    SetDescription( poBase->GetDescription() );
    if( poBase->GetDriver() )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription( poBase->GetDriver()->GetDescription() );
    }
}

GDALVectorTranslateWrappedDataset* GDALVectorTranslateWrappedDataset::New(
                        GDALDataset* poBase,
                        OGRSpatialReference* poOutputSRS,
                        bool bTransform )
{
    GDALVectorTranslateWrappedDataset* poNew =
                                new GDALVectorTranslateWrappedDataset(
                                                                poBase,
                                                                poOutputSRS,
                                                                bTransform);
    for(int i = 0; i < poBase->GetLayerCount(); i++ )
    {
        OGRLayer* poLayer = GDALVectorTranslateWrappedLayer::New(
                            poBase->GetLayer(i), false, poOutputSRS, bTransform);
        if(poLayer == nullptr )
        {
            delete poNew;
            return nullptr;
        }
        poNew->m_apoLayers.push_back(poLayer);
    }
    return poNew;
}

GDALVectorTranslateWrappedDataset::~GDALVectorTranslateWrappedDataset()
{
    delete poDriver;
    for(size_t i = 0; i < m_apoLayers.size(); i++ )
    {
        delete m_apoLayers[i];
    }
    for(size_t i = 0; i < m_apoHiddenLayers.size(); i++ )
    {
        delete m_apoHiddenLayers[i];
    }
}

OGRLayer* GDALVectorTranslateWrappedDataset::GetLayer(int i)
{
    if( i < 0 || i >= static_cast<int>(m_apoLayers.size()) )
        return nullptr;
    return m_apoLayers[i];
}

OGRLayer* GDALVectorTranslateWrappedDataset::GetLayerByName(const char* pszName)
{
    for(size_t i = 0; i < m_apoLayers.size(); i++ )
    {
        if( strcmp(m_apoLayers[i]->GetName(), pszName) == 0 )
            return m_apoLayers[i];
    }
    for(size_t i = 0; i < m_apoHiddenLayers.size(); i++ )
    {
        if( strcmp(m_apoHiddenLayers[i]->GetName(), pszName) == 0 )
            return m_apoHiddenLayers[i];
    }
    for(size_t i = 0; i < m_apoLayers.size(); i++ )
    {
        if( EQUAL(m_apoLayers[i]->GetName(), pszName) )
            return m_apoLayers[i];
    }
    for(size_t i = 0; i < m_apoHiddenLayers.size(); i++ )
    {
        if( EQUAL(m_apoHiddenLayers[i]->GetName(), pszName) )
            return m_apoHiddenLayers[i];
    }

    OGRLayer* poLayer = m_poBase->GetLayerByName(pszName);
    if( poLayer == nullptr )
        return nullptr;
    poLayer = GDALVectorTranslateWrappedLayer::New(
                                poLayer, false, m_poOutputSRS, m_bTransform);
    if( poLayer == nullptr )
        return nullptr;

    // Replicate source dataset behavior: if the fact of calling
    // GetLayerByName() on a initially hidden layer makes it visible through
    // GetLayerCount()/GetLayer(), do the same. Otherwise we are going to
    // maintain it hidden as well.
    for( int i = 0; i < m_poBase->GetLayerCount(); i++ )
    {
        if( m_poBase->GetLayer(i) == poLayer )
        {
            m_apoLayers.push_back(poLayer);
            return poLayer;
        }
    }
    m_apoHiddenLayers.push_back(poLayer);
    return poLayer;
}


OGRLayer *  GDALVectorTranslateWrappedDataset::ExecuteSQL(
                                        const char *pszStatement,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )
{
    OGRLayer* poLayer = m_poBase->ExecuteSQL(pszStatement,
                                                poSpatialFilter, pszDialect);
    if( poLayer == nullptr )
        return nullptr;
    return GDALVectorTranslateWrappedLayer::New(
                                poLayer, true, m_poOutputSRS, m_bTransform);
}

void GDALVectorTranslateWrappedDataset:: ReleaseResultSet(
                                                    OGRLayer * poResultsSet )
{
    delete poResultsSet;
}

/************************************************************************/
/*                     OGR2OGRSpatialReferenceHolder                    */
/************************************************************************/

class OGR2OGRSpatialReferenceHolder
{
        OGRSpatialReference* m_poSRS;

    public:
        OGR2OGRSpatialReferenceHolder() : m_poSRS(nullptr) {}
       ~OGR2OGRSpatialReferenceHolder() { if( m_poSRS) m_poSRS->Release(); }

       void assignNoRefIncrease(OGRSpatialReference* poSRS) {
           CPLAssert(m_poSRS == nullptr);
           m_poSRS = poSRS;
       }
       OGRSpatialReference* get() { return m_poSRS; }
};

/************************************************************************/
/*                     GDALVectorTranslateCreateCopy()                  */
/************************************************************************/

static GDALDataset* GDALVectorTranslateCreateCopy(
                                    GDALDriver* poDriver,
                                    const char* pszDest,
                                    GDALDataset* poDS,
                                    const GDALVectorTranslateOptions* psOptions)
{
    const char* const szErrorMsg = "%s not supported by this output driver";

    if( psOptions->bSkipFailures )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-skipfailures");
        return nullptr;
    }
    if( psOptions->nLayerTransaction >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-lyr_transaction or -ds_transaction");
        return nullptr;
    }
    if( psOptions->nFIDToFetch >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fid");
        return nullptr;
    }
    if( psOptions->papszLCO )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-lco");
        return nullptr;
    }
    if( psOptions->bAddMissingFields )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-addfields");
        return nullptr;
    }
    if( psOptions->pszSourceSRSDef )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-s_srs");
        return nullptr;
    }
    if( !psOptions->bExactFieldNameMatch )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-relaxedFieldNameMatch");
        return nullptr;
    }
    if( psOptions->pszNewLayerName )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nln");
        return nullptr;
    }
    if( psOptions->papszSelFields )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-select");
        return nullptr;
    }
    if( psOptions->pszSQLStatement )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-sql");
        return nullptr;
    }
    if( psOptions->pszDialect )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-dialect");
        return nullptr;
    }
    if( psOptions->eGType != GEOMTYPE_UNCHANGED ||
        psOptions->eGeomTypeConversion != GTC_DEFAULT )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nlt");
        return nullptr;
    }
    if( psOptions->papszFieldTypesToString )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fieldTypeToString");
        return nullptr;
    }
    if( psOptions->papszMapFieldType )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-mapFieldType");
        return nullptr;
    }
    if( psOptions->bUnsetFieldWidth )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetFieldWidth");
        return nullptr;
    }
    if( psOptions->bWrapDateline )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-wrapdateline");
        return nullptr;
    }
    if( psOptions->bClipSrc )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrc");
        return nullptr;
    }
    if( psOptions->pszClipSrcSQL )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrcsql");
        return nullptr;
    }
    if( psOptions->pszClipSrcLayer )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrclayer");
        return nullptr;
    }
    if( psOptions->pszClipSrcWhere )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrcwhere");
        return nullptr;
    }
    if( psOptions->pszClipDstDS || psOptions->hClipDst )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdst");
        return nullptr;
    }
    if( psOptions->pszClipDstSQL )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstsql");
        return nullptr;
    }
    if( psOptions->pszClipDstLayer )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstlayer");
        return nullptr;
    }
    if( psOptions->pszClipDstWhere )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstwhere");
        return nullptr;
    }
    if( psOptions->bSplitListFields )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-splitlistfields");
        return nullptr;
    }
    if( psOptions->nMaxSplitListSubFields >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-maxsubfields");
        return nullptr;
    }
    if( psOptions->bExplodeCollections )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-explodecollections");
        return nullptr;
    }
    if( psOptions->pszZField )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-zfield");
        return nullptr;
    }
    if( psOptions->nGCPCount )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-gcp");
        return nullptr;
    }
    if( psOptions->papszFieldMap )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fieldmap");
        return nullptr;
    }
    if( psOptions->bForceNullable )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-forceNullable");
        return nullptr;
    }
    if( psOptions->bResolveDomains )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-forceNullable");
        return nullptr;
    }
    if( psOptions->bEmptyStrAsNull )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-emptyStrAsNull");
        return nullptr;
    }
    if( psOptions->bUnsetDefault )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetDefault");
        return nullptr;
    }
    if( psOptions->bUnsetFid )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetFid");
        return nullptr;
    }
    if( !psOptions->bCopyMD )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nomd");
        return nullptr;
    }
    if( !psOptions->bNativeData )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-noNativeData");
        return nullptr;
    }
    if( psOptions->nLimit >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-limit");
        return nullptr;
    }
    if( psOptions->papszMetadataOptions )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-mo");
        return nullptr;
    }

    GDALDataset* poWrkSrcDS = poDS;
    OGR2OGRSpatialReferenceHolder oOutputSRSHolder;

    if( psOptions->pszOutputSRSDef )
    {
        oOutputSRSHolder.assignNoRefIncrease(new OGRSpatialReference());
        oOutputSRSHolder.get()->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( oOutputSRSHolder.get()->
                SetFromUserInput( psOptions->pszOutputSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to process SRS definition: %s",
                      psOptions->pszOutputSRSDef );
            return nullptr;
        }
        oOutputSRSHolder.get()->SetCoordinateEpoch(psOptions->dfOutputCoordinateEpoch);

        poWrkSrcDS = GDALVectorTranslateWrappedDataset::New(
            poDS, oOutputSRSHolder.get(), psOptions->bTransform);
        if( poWrkSrcDS == nullptr )
            return nullptr;
    }

    if( psOptions->pszWHERE )
    {
        // Hack for GMLAS driver
        if( EQUAL(poDriver->GetDescription(), "GMLAS") )
        {
            if( psOptions->papszLayers == nullptr )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-where not supported by this output driver "
                         "without explicit layer name(s)");
                if( poWrkSrcDS != poDS )
                    delete poWrkSrcDS;
                return nullptr;
            }
            else
            {
                char** papszIter = psOptions->papszLayers;
                for( ; *papszIter != nullptr; ++papszIter )
                {
                    OGRLayer* poSrcLayer = poDS->GetLayerByName(*papszIter);
                    if( poSrcLayer != nullptr )
                    {
                        poSrcLayer->SetAttributeFilter( psOptions->pszWHERE );
                    }
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-where");
            if( poWrkSrcDS != poDS )
                delete poWrkSrcDS;
            return nullptr;
        }
    }

    if( psOptions->hSpatialFilter )
    {
        for( int i=0; i<poWrkSrcDS->GetLayerCount();++i)
        {
            OGRLayer* poSrcLayer = poWrkSrcDS->GetLayer(i);
            if( poSrcLayer &&
                poSrcLayer->GetLayerDefn()->GetGeomFieldCount() > 0 &&
                (psOptions->papszLayers == nullptr ||
                 CSLFindString(psOptions->papszLayers,
                               poSrcLayer->GetName())>=0) )
            {
                if( psOptions->pszGeomField != nullptr )
                {
                    const int iGeomField = poSrcLayer->GetLayerDefn()->
                            GetGeomFieldIndex(psOptions->pszGeomField);
                    if( iGeomField >= 0 )
                        poSrcLayer->SetSpatialFilter( iGeomField,
                            reinterpret_cast<OGRGeometry*>(
                                                psOptions->hSpatialFilter) );
                    else
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "Cannot find geometry field %s in layer %s. "
                                  "Applying to first geometry field",
                                  psOptions->pszGeomField,
                                  poSrcLayer->GetName() );
                }
                else
                {
                    poSrcLayer->SetSpatialFilter(
                        reinterpret_cast<OGRGeometry*>(
                                            psOptions->hSpatialFilter) );
                }
            }
        }
    }

    char** papszDSCO = CSLDuplicate(psOptions->papszDSCO);
    if( psOptions->papszLayers )
    {
        // Hack for GMLAS driver
        if( EQUAL(poDriver->GetDescription(), "GMLAS") )
        {
            CPLString osLayers;
            char** papszIter = psOptions->papszLayers;
            for( ; *papszIter != nullptr; ++papszIter )
            {
                if( !osLayers.empty() )
                    osLayers += ",";
                osLayers += *papszIter;
            }
            papszDSCO = CSLSetNameValue(papszDSCO, "LAYERS", osLayers);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg,
                     "Specifying layers");
            CSLDestroy(papszDSCO);
            if( poWrkSrcDS != poDS )
                delete poWrkSrcDS;
            return nullptr;
        }
    }

    // Hack for GMLAS driver (this speed up deletion by avoiding the GML
    // driver to try parsing a pre-existing file). Could be potentially
    // removed if the GML driver implemented fast dataset opening (ie
    // without parsing) and GetFileList()
    if( EQUAL(poDriver->GetDescription(), "GMLAS") )
    {
        GDALDriverH hIdentifyingDriver = GDALIdentifyDriver(pszDest, nullptr);
        if( hIdentifyingDriver != nullptr &&
            EQUAL( GDALGetDescription(hIdentifyingDriver), "GML" ) )
        {
            VSIUnlink( pszDest );
            VSIUnlink( CPLResetExtension(pszDest, "gfs") );
        }
    }

    GDALDataset* poOut = poDriver->CreateCopy(pszDest, poWrkSrcDS, FALSE,
                                              papszDSCO,
                                              psOptions->pfnProgress,
                                              psOptions->pProgressData);
    CSLDestroy(papszDSCO);

    if( poWrkSrcDS != poDS )
        delete poWrkSrcDS;

    return poOut;
}

/************************************************************************/
/*                           GDALVectorTranslate()                      */
/************************************************************************/
/**
 * Converts vector data between file formats.
 *
 * This is the equivalent of the <a href="/programs/ogr2ogr.html">ogr2ogr</a> utility.
 *
 * GDALVectorTranslateOptions* must be allocated and freed with GDALVectorTranslateOptionsNew()
 * and GDALVectorTranslateOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param nSrcCount the number of input datasets (only 1 supported currently)
 * @param pahSrcDS the list of input datasets.
 * @param psOptionsIn the options struct returned by GDALVectorTranslateOptionsNew() or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any usage error has occurred, or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALVectorTranslate( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                                  GDALDatasetH *pahSrcDS,
                                  const GDALVectorTranslateOptions *psOptionsIn, int *pbUsageError )

{
    if( pszDest == nullptr && hDstDS == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszDest == NULL && hDstDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if( nSrcCount != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "nSrcCount != 1");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALDatasetH hSrcDS = pahSrcDS[0];
    if( hSrcDS == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "hSrcDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALVectorTranslateOptions* psOptions =
        psOptionsIn
        ? GDALVectorTranslateOptionsClone(psOptionsIn)
        : GDALVectorTranslateOptionsNew(nullptr, nullptr);

    bool bAppend = false;
    bool bUpdate = false;
    bool bOverwrite = false;

    if( psOptions->eAccessMode == ACCESS_UPDATE )
    {
        bUpdate = true;
    }
    else if ( psOptions->eAccessMode == ACCESS_APPEND )
    {
        bAppend = true;
        bUpdate = true;
    }
    else if ( psOptions->eAccessMode == ACCESS_OVERWRITE )
    {
        bOverwrite = true;
        bUpdate = true;
    }
    else if( hDstDS != nullptr )
    {
        bUpdate = true;
    }

   const CPLString osDateLineOffset =
       CPLOPrintf("%g", psOptions->dfDateLineOffset);

    if( psOptions->bPreserveFID && psOptions->bExplodeCollections )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "cannot use -preserve_fid and -explodecollections at the same time.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if (psOptions->papszFieldMap && !bAppend)
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "if -fieldmap is specified, -append must also be specified");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if (psOptions->papszFieldMap && psOptions->bAddMissingFields)
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "if -addfields is specified, -fieldmap cannot be used.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if (psOptions->papszSelFields && bAppend && !psOptions->bAddMissingFields)
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "if -append is specified, -select cannot be used "
                  "(use -fieldmap or -sql instead)." );
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if( psOptions->papszFieldTypesToString && psOptions->papszMapFieldType )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "-fieldTypeToString and -mapFieldType are exclusive.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if( psOptions->pszSourceSRSDef != nullptr && psOptions->pszOutputSRSDef == nullptr && psOptions->pszSpatSRSDef == nullptr )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "if -s_srs is specified, -t_srs and/or -spat_srs must also be specified.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if( psOptions->bClipSrc && psOptions->pszClipSrcDS != nullptr)
    {
        psOptions->hClipSrc = reinterpret_cast<OGRGeometryH>(LoadGeometry(psOptions->pszClipSrcDS, psOptions->pszClipSrcSQL, psOptions->pszClipSrcLayer, psOptions->pszClipSrcWhere));
        if (psOptions->hClipSrc == nullptr)
        {
            CPLError( CE_Failure,CPLE_IllegalArg, "cannot load source clip geometry");
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }
    else if( psOptions->bClipSrc && psOptions->hClipSrc == nullptr )
    {
        if (psOptions->hSpatialFilter)
            psOptions->hClipSrc = OGR_G_Clone(psOptions->hSpatialFilter);
        if (psOptions->hClipSrc == nullptr)
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "-clipsrc must be used with -spat option or a\n"
                             "bounding box, WKT string or datasource must be specified");
            if(pbUsageError)
                *pbUsageError = TRUE;
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }

    if( psOptions->pszClipDstDS != nullptr)
    {
        psOptions->hClipDst = reinterpret_cast<OGRGeometryH>(LoadGeometry(psOptions->pszClipDstDS, psOptions->pszClipDstSQL, psOptions->pszClipDstLayer, psOptions->pszClipDstWhere));
        if (psOptions->hClipDst == nullptr)
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "cannot load dest clip geometry");
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }

    GDALDataset *poDS = static_cast<GDALDataset *>(hSrcDS);
    GDALDataset *poODS = nullptr;
    GDALDriver *poDriver = nullptr;
    CPLString osDestFilename;

    if(hDstDS)
    {
        poODS = static_cast<GDALDataset *>(hDstDS);
        osDestFilename = poODS->GetDescription();
    }
    else
    {
        osDestFilename = pszDest;
    }

    /* Various tests to avoid overwriting the source layer(s) */
    /* or to avoid appending a layer to itself */
    if( bUpdate && strcmp(osDestFilename, poDS->GetDescription()) == 0 &&
        !EQUAL(poDS->GetDriverName(), "Memory") &&
        (bOverwrite || bAppend) )
    {
        bool bError = false;
        if (psOptions->pszNewLayerName == nullptr)
            bError = true;
        else if (CSLCount(psOptions->papszLayers) == 1)
            bError = strcmp(psOptions->pszNewLayerName, psOptions->papszLayers[0]) == 0;
        else if (psOptions->pszSQLStatement == nullptr)
            bError = true;
        if (bError)
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                        "-nln name must be specified combined with "
                        "a single source layer name,\nor a -sql statement, and "
                        "name must be different from an existing layer.");
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }
    else if( !bUpdate && strcmp(osDestFilename, poDS->GetDescription()) == 0 &&
             (psOptions->pszFormat == nullptr || !EQUAL(psOptions->pszFormat, "Memory")) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Source and destination datasets must be different "
                  "in non-update mode." );
        GDALVectorTranslateOptionsFree(psOptions);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the output datasource as an existing, writable      */
/* -------------------------------------------------------------------- */
    std::vector<CPLString> aoDrivers;
    if( poODS == nullptr && psOptions->pszFormat == nullptr )
    {
        aoDrivers = GetOutputDriversFor(pszDest, GDAL_OF_VECTOR);
        if( !bUpdate && aoDrivers.size() == 1 )
        {
            GDALDriverH hDriver = GDALGetDriverByName(aoDrivers[0]);
            const char* pszPrefix = GDALGetMetadataItem(hDriver,
                    GDAL_DMD_CONNECTION_PREFIX, nullptr);
            if( pszPrefix && STARTS_WITH_CI(pszDest, pszPrefix) )
            {
                bUpdate = true;
            }
        }
    }

    if( bUpdate && poODS == nullptr )
    {
        poODS = static_cast<GDALDataset*>(GDALOpenEx( osDestFilename,
                GDAL_OF_UPDATE | GDAL_OF_VECTOR, nullptr, psOptions->papszDestOpenOptions, nullptr ));

        if( poODS == nullptr )
        {
            if (bOverwrite || bAppend)
            {
                poODS = static_cast<GDALDataset*>(GDALOpenEx( osDestFilename,
                            GDAL_OF_VECTOR, nullptr, psOptions->papszDestOpenOptions, nullptr ));
                if (poODS == nullptr)
                {
                    /* OK the datasource doesn't exist at all */
                    bUpdate = false;
                }
                else
                {
                    poDriver = poODS->GetDriver();
                    GDALClose(poODS);
                    poODS = nullptr;
                }
            }

            if (bUpdate)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Unable to open existing output datasource `%s'.",
                        osDestFilename.c_str() );
                GDALVectorTranslateOptionsFree(psOptions);
                return nullptr;
            }
        }
        else if( CSLCount(psOptions->papszDSCO) > 0 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, "Datasource creation options ignored since an existing datasource\n"
                    "         being updated." );
        }
    }

    if( poODS )
        poDriver = poODS->GetDriver();

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    bool bNewDataSource = false;
    if( !bUpdate )
    {
        GDALDriverManager *poDM = GetGDALDriverManager();

        if( psOptions->pszFormat == nullptr )
        {
            if( aoDrivers.empty() )
            {
                if( EQUAL(CPLGetExtension(pszDest), "") )
                {
                    psOptions->pszFormat = CPLStrdup("ESRI Shapefile");
                }
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "Cannot guess driver for %s", pszDest);
                    GDALVectorTranslateOptionsFree(psOptions);
                    return nullptr;
                }
            }
            else
            {
                if( aoDrivers.size() > 1 )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Several drivers matching %s extension. Using %s",
                            CPLGetExtension(pszDest), aoDrivers[0].c_str() );
                }
                psOptions->pszFormat = CPLStrdup(aoDrivers[0]);
            }
            CPLDebug("GDAL", "Using %s driver",
                     psOptions->pszFormat);
        }

        CPLString osOGRCompatFormat(psOptions->pszFormat);
        // Special processing for non-unified drivers that have the same name
        // as GDAL and OGR drivers. GMT should become OGR_GMT.
        // Other candidates could be VRT, SDTS and PDS, but they don't
        // have write capabilities. But do the substitution to get a sensible
        // error message
        if( EQUAL(osOGRCompatFormat, "GMT") ||
            EQUAL(osOGRCompatFormat, "VRT") ||
            EQUAL(osOGRCompatFormat, "SDTS") ||
            EQUAL(osOGRCompatFormat, "PDS") )
        {
            osOGRCompatFormat = "OGR_" + osOGRCompatFormat;
        }
        poDriver = poDM->GetDriverByName(osOGRCompatFormat);
        if( poDriver == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to find driver `%s'.", psOptions->pszFormat );
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }

        char** papszDriverMD = poDriver->GetMetadata();
        if( !CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                               GDAL_DCAP_VECTOR, "FALSE") ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s driver has no vector capabilities.",
                      psOptions->pszFormat );
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }

        if( !CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                               GDAL_DCAP_CREATE, "FALSE") ) )
        {
            if( CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                            GDAL_DCAP_CREATECOPY, "FALSE") ) )
            {
                poODS = GDALVectorTranslateCreateCopy(poDriver, pszDest,
                                                      poDS, psOptions);
                GDALVectorTranslateOptionsFree(psOptions);
                return poODS;
            }

            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s driver does not support data source creation.",
                    psOptions->pszFormat );
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }

        if( psOptions->papszDestOpenOptions != nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined, "-doo ignored when creating the output datasource.");
        }

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating        */
/*      a datasource with multiple layers into a shapefile. If the      */
/*      user gives a target datasource with .shp and it does not exist, */
/*      the shapefile driver will try to create a file, but this is not */
/*      appropriate because here we have several layers, so create      */
/*      a directory instead.                                            */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            psOptions->pszSQLStatement == nullptr &&
            (CSLCount(psOptions->papszLayers) > 1 ||
             (CSLCount(psOptions->papszLayers) == 0 && poDS->GetLayerCount() > 1)) &&
            psOptions->pszNewLayerName == nullptr &&
            EQUAL(CPLGetExtension(osDestFilename), "SHP") &&
            VSIStatL(osDestFilename, &sStat) != 0)
        {
            if (VSIMkdir(osDestFilename, 0755) != 0)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to create directory %s\n"
                      "for shapefile datastore.",
                      osDestFilename.c_str() );
                GDALVectorTranslateOptionsFree(psOptions);
                return nullptr;
            }
        }

/* -------------------------------------------------------------------- */
/*      Create the output data source.                                  */
/* -------------------------------------------------------------------- */
        poODS = poDriver->Create( osDestFilename, 0, 0, 0, GDT_Unknown, psOptions->papszDSCO );
        if( poODS == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "%s driver failed to create %s",
                    psOptions->pszFormat, osDestFilename.c_str() );
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }
        bNewDataSource = true;

        if( psOptions->bCopyMD )
        {
            char** papszDomains = poDS->GetMetadataDomainList();
            for(char** papszIter = papszDomains; papszIter && *papszIter; ++papszIter )
            {
                char** papszMD = poDS->GetMetadata(*papszIter);
                if( papszMD )
                    poODS->SetMetadata(papszMD, *papszIter);
            }
            CSLDestroy(papszDomains);
        }
        for(char** papszIter = psOptions->papszMetadataOptions; papszIter && *papszIter; ++papszIter )
        {
            char    *pszKey = nullptr;
            const char *pszValue = CPLParseNameValue( *papszIter, &pszKey );
            if( pszKey )
            {
                poODS->SetMetadataItem(pszKey,pszValue);
                CPLFree( pszKey );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      For random reading                                              */
/* -------------------------------------------------------------------- */
    const bool bRandomLayerReading = CPL_TO_BOOL(
                                    poDS->TestCapability(ODsCRandomLayerRead));
    if( bRandomLayerReading &&
        !poODS->TestCapability(ODsCRandomLayerWrite) &&
        CSLCount(psOptions->papszLayers) != 1 &&
        psOptions->pszSQLStatement == nullptr &&
        !psOptions->bQuiet )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                    "Input datasource uses random layer reading, but "
                    "output datasource does not support random layer writing");
    }

    if( psOptions->nLayerTransaction < 0 )
    {
        if( bRandomLayerReading )
            psOptions->nLayerTransaction = FALSE;
        else
            psOptions->nLayerTransaction = !poODS->TestCapability(ODsCTransactions);
    }
    else if( psOptions->nLayerTransaction &&
             bRandomLayerReading )
    {
        psOptions->nLayerTransaction = false;
    }

/* -------------------------------------------------------------------- */
/*      Parse the output SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    OGR2OGRSpatialReferenceHolder oOutputSRSHolder;
    if( psOptions->pszOutputSRSDef != nullptr )
    {
        oOutputSRSHolder.assignNoRefIncrease(new OGRSpatialReference());
        oOutputSRSHolder.get()->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( oOutputSRSHolder.get()->
                SetFromUserInput( psOptions->pszOutputSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                    psOptions->pszOutputSRSDef );
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == nullptr ) GDALClose( poODS );
            return nullptr;
        }
        oOutputSRSHolder.get()->SetCoordinateEpoch(psOptions->dfOutputCoordinateEpoch);
    }

/* -------------------------------------------------------------------- */
/*      Parse the source SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSourceSRS;
    OGRSpatialReference *poSourceSRS = nullptr;
    if( psOptions->pszSourceSRSDef != nullptr )
    {
        oSourceSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( oSourceSRS.SetFromUserInput( psOptions->pszSourceSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                    psOptions->pszSourceSRSDef );
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == nullptr ) GDALClose( poODS );
            return nullptr;
        }
        oSourceSRS.SetCoordinateEpoch(psOptions->dfSourceCoordinateEpoch);
        poSourceSRS = &oSourceSRS;
    }

/* -------------------------------------------------------------------- */
/*      Parse spatial filter SRS if needed.                             */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSpatSRS;
    OGRSpatialReference* poSpatSRS = nullptr;
    if( psOptions->hSpatialFilter != nullptr && psOptions->pszSpatSRSDef != nullptr )
    {
        if( psOptions->pszSQLStatement )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "-spat_srs not compatible with -sql.");
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == nullptr ) GDALClose( poODS );
            return nullptr;
        }
        OGREnvelope sEnvelope;
        OGR_G_GetEnvelope(psOptions->hSpatialFilter, &sEnvelope);
        oSpatSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( oSpatSRS.SetFromUserInput( psOptions->pszSpatSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                    psOptions->pszSpatSRSDef );
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == nullptr ) GDALClose( poODS );
            return nullptr;
        }
        poSpatSRS = &oSpatSRS;
    }

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    GCPCoordTransformation *poGCPCoordTrans = nullptr;
    if( psOptions->nGCPCount > 0 )
    {
        poGCPCoordTrans = new GCPCoordTransformation( psOptions->nGCPCount, psOptions->pasGCPs,
                                                      psOptions->nTransformOrder,
                                                      poSourceSRS ? poSourceSRS : oOutputSRSHolder.get() );
        if( !(poGCPCoordTrans->IsValid()) )
        {
            delete poGCPCoordTrans;
            poGCPCoordTrans = nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create layer setup and transformer objects.                     */
/* -------------------------------------------------------------------- */
    SetupTargetLayer oSetup;
    oSetup.m_poSrcDS = poDS;
    oSetup.m_poDstDS = poODS;
    oSetup.m_papszLCO = psOptions->papszLCO;
    oSetup.m_poOutputSRS = oOutputSRSHolder.get();
    oSetup.m_bNullifyOutputSRS = psOptions->bNullifyOutputSRS;
    oSetup.m_papszSelFields = psOptions->papszSelFields;
    oSetup.m_bAppend = bAppend;
    oSetup.m_bAddMissingFields = psOptions->bAddMissingFields;
    oSetup.m_eGType = psOptions->eGType;
    oSetup.m_eGeomTypeConversion = psOptions->eGeomTypeConversion;
    oSetup.m_nCoordDim = psOptions->nCoordDim;
    oSetup.m_bOverwrite = bOverwrite;
    oSetup.m_papszFieldTypesToString = psOptions->papszFieldTypesToString;
    oSetup.m_papszMapFieldType = psOptions->papszMapFieldType;
    oSetup.m_bUnsetFieldWidth = psOptions->bUnsetFieldWidth;
    oSetup.m_bExplodeCollections = psOptions->bExplodeCollections;
    oSetup.m_pszZField = psOptions->pszZField;
    oSetup.m_papszFieldMap = psOptions->papszFieldMap;
    oSetup.m_pszWHERE = psOptions->pszWHERE;
    oSetup.m_bExactFieldNameMatch = psOptions->bExactFieldNameMatch;
    oSetup.m_bQuiet = psOptions->bQuiet;
    oSetup.m_bForceNullable = psOptions->bForceNullable;
    oSetup.m_bResolveDomains = psOptions->bResolveDomains;
    oSetup.m_bUnsetDefault = psOptions->bUnsetDefault;
    oSetup.m_bUnsetFid = psOptions->bUnsetFid;
    oSetup.m_bPreserveFID = psOptions->bPreserveFID;
    oSetup.m_bCopyMD = psOptions->bCopyMD;
    oSetup.m_bNativeData = psOptions->bNativeData;
    oSetup.m_bNewDataSource = bNewDataSource;
    oSetup.m_pszCTPipeline = psOptions->pszCTPipeline;

    LayerTranslator oTranslator;
    oTranslator.m_poSrcDS = poDS;
    oTranslator.m_poODS = poODS;
    oTranslator.m_bTransform = psOptions->bTransform;
    oTranslator.m_bWrapDateline = psOptions->bWrapDateline;
    oTranslator.m_osDateLineOffset = osDateLineOffset;
    oTranslator.m_poOutputSRS = oOutputSRSHolder.get();
    oTranslator.m_bNullifyOutputSRS = psOptions->bNullifyOutputSRS;
    oTranslator.m_poUserSourceSRS = poSourceSRS;
    oTranslator.m_poGCPCoordTrans = poGCPCoordTrans;
    oTranslator.m_eGType = psOptions->eGType;
    oTranslator.m_eGeomTypeConversion = psOptions->eGeomTypeConversion;
    oTranslator.m_bMakeValid = psOptions->bMakeValid;
    oTranslator.m_nCoordDim = psOptions->nCoordDim;
    oTranslator.m_eGeomOp = psOptions->eGeomOp;
    oTranslator.m_dfGeomOpParam = psOptions->dfGeomOpParam;
    oTranslator.m_poClipSrc = reinterpret_cast<OGRGeometry*>(psOptions->hClipSrc);
    oTranslator.m_poClipDst = reinterpret_cast<OGRGeometry*>(psOptions->hClipDst);
    oTranslator.m_bExplodeCollections = psOptions->bExplodeCollections;
    oTranslator.m_bNativeData = psOptions->bNativeData;
    oTranslator.m_nLimit = psOptions->nLimit;

    if( psOptions->nGroupTransactions )
    {
        if( !psOptions->nLayerTransaction )
            poODS->StartTransaction(psOptions->bForceTransaction);
    }

    GIntBig nTotalEventsDone = 0;

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
    int nRetCode = 0;

    if( psOptions->pszSQLStatement != nullptr )
    {
        /* Special case: if output=input, then we must likely destroy the */
        /* old table before to avoid transaction issues. */
        if( poDS == poODS && psOptions->pszNewLayerName != nullptr && bOverwrite )
            GetLayerAndOverwriteIfNecessary(poODS, psOptions->pszNewLayerName, bOverwrite, nullptr, nullptr, nullptr);

        if( psOptions->pszWHERE != nullptr )
            CPLError( CE_Warning, CPLE_AppDefined, "-where clause ignored in combination with -sql." );
        if( CSLCount(psOptions->papszLayers) > 0 )
            CPLError( CE_Warning, CPLE_AppDefined, "layer names ignored in combination with -sql." );

        OGRLayer *poResultSet =
            poDS->ExecuteSQL(
                psOptions->pszSQLStatement,
                (psOptions->pszGeomField == nullptr) ? reinterpret_cast<OGRGeometry*>(psOptions->hSpatialFilter) : nullptr,
                psOptions->pszDialect);

        if( poResultSet != nullptr )
        {
            if( psOptions->hSpatialFilter != nullptr && psOptions->pszGeomField != nullptr )
            {
                int iGeomField = poResultSet->GetLayerDefn()->GetGeomFieldIndex(psOptions->pszGeomField);
                if( iGeomField >= 0 )
                    poResultSet->SetSpatialFilter( iGeomField, reinterpret_cast<OGRGeometry*>(psOptions->hSpatialFilter) );
                else
                    CPLError( CE_Warning, CPLE_AppDefined, "Cannot find geometry field %s.",
                           psOptions->pszGeomField);
            }

            GIntBig nCountLayerFeatures = 0;
            GDALProgressFunc pfnProgress = nullptr;
            void *pProgressArg = nullptr;
            if (psOptions->bDisplayProgress)
            {
                if (bRandomLayerReading)
                {
                    pfnProgress = psOptions->pfnProgress;
                    pProgressArg = psOptions->pProgressData;
                }
                else if (!poResultSet->TestCapability(OLCFastFeatureCount))
                {
                    CPLError( CE_Warning, CPLE_AppDefined, "Progress turned off as fast feature count is not available.");
                    psOptions->bDisplayProgress = false;
                }
                else
                {
                    nCountLayerFeatures = poResultSet->GetFeatureCount();
                    pfnProgress = psOptions->pfnProgress;
                    pProgressArg = psOptions->pProgressData;
                }
            }

            OGRLayer* poPassedLayer = poResultSet;
            if (psOptions->bSplitListFields)
            {
                auto poLayer = new OGRSplitListFieldLayer(poPassedLayer, psOptions->nMaxSplitListSubFields);
                poPassedLayer = poLayer;
                int nRet = poLayer->BuildLayerDefn(nullptr, nullptr);
                if (!nRet)
                {
                    delete poPassedLayer;
                    poPassedLayer = poResultSet;
                }
            }

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
            VSIStatBufL sStat;
            if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
                psOptions->pszNewLayerName == nullptr &&
                VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode) &&
                (EQUAL(CPLGetExtension(osDestFilename), "shp") ||
                 EQUAL(CPLGetExtension(osDestFilename), "shz") ||
                 EQUAL(CPLGetExtension(osDestFilename), "dbf")) )
            {
                psOptions->pszNewLayerName = CPLStrdup(CPLGetBasename(osDestFilename));
            }

            auto psInfo = oSetup.Setup(poPassedLayer,
                                                   psOptions->pszNewLayerName,
                                                   psOptions,
                                                   nTotalEventsDone);

            poPassedLayer->ResetReading();

            if( psInfo == nullptr ||
                !oTranslator.Translate( nullptr, psInfo.get(),
                                        nCountLayerFeatures, nullptr,
                                        nTotalEventsDone,
                                        pfnProgress, pProgressArg, psOptions ))
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Terminating translation prematurely after failed\n"
                          "translation from sql statement." );

                nRetCode = 1;
            }

            if (poPassedLayer != poResultSet)
                delete poPassedLayer;

            poDS->ReleaseResultSet( poResultSet );
        }
        else
        {
            if( CPLGetLastErrorNo() != 0 )
                nRetCode = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for layer interleaving mode.                       */
/* -------------------------------------------------------------------- */
    else if( bRandomLayerReading )
    {
        if (psOptions->bSplitListFields)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "-splitlistfields not supported in this mode" );
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == nullptr ) GDALClose( poODS );
            delete poGCPCoordTrans;
            return nullptr;
        }

        // Make sure to probe all layers in case some are by default invisible
        for( char** papszIter = psOptions->papszLayers;
                    papszIter && *papszIter; ++papszIter )
        {
            OGRLayer *poLayer = poDS->GetLayerByName(*papszIter);

            if( poLayer == nullptr )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch requested layer %s!",
                          *papszIter );
                GDALVectorTranslateOptionsFree(psOptions);
                if( hDstDS == nullptr ) GDALClose( poODS );
                delete poGCPCoordTrans;
                return nullptr;
            }
        }

        const int nSrcLayerCount = poDS->GetLayerCount();
        std::vector<AssociatedLayers> pasAssocLayers(nSrcLayerCount);

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            (CSLCount(psOptions->papszLayers) == 1 || nSrcLayerCount == 1) &&
            psOptions->pszNewLayerName == nullptr &&
            VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode) &&
            (EQUAL(CPLGetExtension(osDestFilename), "shp") ||
             EQUAL(CPLGetExtension(osDestFilename), "shz") ||
             EQUAL(CPLGetExtension(osDestFilename), "dbf")) )
        {
            psOptions->pszNewLayerName = CPLStrdup(CPLGetBasename(osDestFilename));
        }

        GDALProgressFunc pfnProgress = nullptr;
        void        *pProgressArg = nullptr;
        if ( !psOptions->bQuiet )
        {
            pfnProgress = psOptions->pfnProgress;
            pProgressArg = psOptions->pProgressData;
        }

/* -------------------------------------------------------------------- */
/*      If no target layer specified, use all source layers.            */
/* -------------------------------------------------------------------- */
        if ( CSLCount(psOptions->papszLayers) == 0)
        {
            psOptions->papszLayers = static_cast<char **>(
                CPLCalloc(sizeof(char*), nSrcLayerCount + 1));
            for( int iLayer = 0; iLayer < nSrcLayerCount; iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch advertised layer %d!",
                            iLayer );
                    GDALVectorTranslateOptionsFree(psOptions);
                    if( hDstDS == nullptr ) GDALClose( poODS );
                    delete poGCPCoordTrans;
                    return nullptr;
                }

                psOptions->papszLayers[iLayer] = CPLStrdup(poLayer->GetName());
            }
        }
        else
        {
            const bool bSrcIsOSM = (strcmp(poDS->GetDriverName(), "OSM") == 0);
            if ( bSrcIsOSM )
            {
                CPLString osInterestLayers = "SET interest_layers =";
                for( int iLayer = 0; psOptions->papszLayers[iLayer] != nullptr; iLayer++ )
                {
                    if( iLayer != 0 ) osInterestLayers += ",";
                    osInterestLayers += psOptions->papszLayers[iLayer];
                }

                poDS->ExecuteSQL(osInterestLayers.c_str(), nullptr, nullptr);
            }
        }

/* -------------------------------------------------------------------- */
/*      First pass to set filters.                                      */
/* -------------------------------------------------------------------- */
        std::map<OGRLayer*, int> oMapLayerToIdx;

        for( int iLayer = 0; iLayer < nSrcLayerCount; iLayer++ )
        {
            OGRLayer        *poLayer = poDS->GetLayer(iLayer);
            if( poLayer == nullptr )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch advertised layer %d!",
                        iLayer );
                GDALVectorTranslateOptionsFree(psOptions);
                if( hDstDS == nullptr ) GDALClose( poODS );
                delete poGCPCoordTrans;
                return nullptr;
            }

            pasAssocLayers[iLayer].poSrcLayer = poLayer;

            if( CSLFindString(psOptions->papszLayers, poLayer->GetName()) >= 0 )
            {
                if( psOptions->pszWHERE != nullptr )
                {
                    if( poLayer->SetAttributeFilter( psOptions->pszWHERE ) != OGRERR_NONE )
                    {
                        CPLError( CE_Failure,
                                  CPLE_AppDefined,
                                  "SetAttributeFilter(%s) on layer '%s' failed.",
                                  psOptions->pszWHERE, poLayer->GetName() );
                        if (!psOptions->bSkipFailures)
                        {
                            GDALVectorTranslateOptionsFree(psOptions);
                            if( hDstDS == nullptr ) GDALClose( poODS );
                            delete poGCPCoordTrans;
                            return nullptr;
                        }
                    }
                }

                ApplySpatialFilter(poLayer,
                                   reinterpret_cast<OGRGeometry*>(psOptions->hSpatialFilter),
                                   poSpatSRS, psOptions->pszGeomField,
                                   poSourceSRS );

                oMapLayerToIdx[ poLayer ] = iLayer;
            }
        }

/* -------------------------------------------------------------------- */
/*      Second pass to process features in a interleaved layer mode.    */
/* -------------------------------------------------------------------- */
        bool bTargetLayersHaveBeenCreated = false;
        while( true )
        {
            OGRLayer* poFeatureLayer = nullptr;
            OGRFeature* poFeature = poDS->GetNextFeature(&poFeatureLayer,
                                                         nullptr,
                                                         pfnProgress,
                                                         pProgressArg);
            if( poFeature == nullptr )
                break;
            std::map<OGRLayer*, int>::const_iterator oIter =
                oMapLayerToIdx.find(poFeatureLayer);
            if( oIter == oMapLayerToIdx.end() )
            {
                // Feature in a layer that is not a layer of interest.
                OGRFeature::DestroyFeature(poFeature);
            }
            else
            {
                if( !bTargetLayersHaveBeenCreated )
                {
                    // We defer target layer creation at the first feature
                    // retrieved since getting the layer definition can be
                    // costly (case of the GMLAS driver) and thus we'd better
                    // taking advantage from the progress callback of
                    // GetNextFeature.
                    bTargetLayersHaveBeenCreated = true;
                    for( int iLayer = 0; iLayer < nSrcLayerCount; iLayer++ )
                    {
                        OGRLayer        *poLayer = poDS->GetLayer(iLayer);
                        if( CSLFindString(psOptions->papszLayers, poLayer->GetName()) < 0 )
                            continue;

                        auto psInfo = oSetup.Setup(poLayer,
                                                               psOptions->pszNewLayerName,
                                                               psOptions,
                                                               nTotalEventsDone);

                        if( psInfo == nullptr && !psOptions->bSkipFailures )
                        {
                            GDALVectorTranslateOptionsFree(psOptions);
                            if( hDstDS == nullptr ) GDALClose( poODS );
                            delete poGCPCoordTrans;
                            OGRFeature::DestroyFeature(poFeature);
                            return nullptr;
                        }

                        pasAssocLayers[iLayer].psInfo = std::move(psInfo);
                    }
                    if( nRetCode )
                        break;
                }

                int iLayer = oIter->second;
                TargetLayerInfo *psInfo = pasAssocLayers[iLayer].psInfo.get();
                if( (psInfo == nullptr ||
                     !oTranslator.Translate( poFeature, psInfo,
                                            0, nullptr,
                                            nTotalEventsDone,
                                            nullptr, nullptr, psOptions ))
                    && !psOptions->bSkipFailures )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "Terminating translation prematurely after failed\n"
                            "translation of layer %s (use -skipfailures to skip errors)",
                            poFeatureLayer->GetName() );

                    nRetCode = 1;
                    break;
                }
                if( psInfo == nullptr )
                    OGRFeature::DestroyFeature(poFeature);
            }
        }  // while true

        if (pfnProgress)
        {
            pfnProgress(1.0, "", pProgressArg);
        }

        if( !bTargetLayersHaveBeenCreated )
        {
            // bTargetLayersHaveBeenCreated not used after here.
            // bTargetLayersHaveBeenCreated = true;
            for( int iLayer = 0; iLayer < nSrcLayerCount; iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);
                if( CSLFindString(psOptions->papszLayers, poLayer->GetName()) < 0 )
                    continue;

                auto psInfo = oSetup.Setup(poLayer,
                                                       psOptions->pszNewLayerName,
                                                       psOptions,
                                                       nTotalEventsDone);

                if( psInfo == nullptr && !psOptions->bSkipFailures )
                {
                    GDALVectorTranslateOptionsFree(psOptions);
                    if( hDstDS == nullptr ) GDALClose( poODS );
                    delete poGCPCoordTrans;
                    return nullptr;
                }

                pasAssocLayers[iLayer].psInfo = std::move(psInfo);
            }
        }
    }

    else
    {
        int nLayerCount = 0;
        std::vector<OGRLayer*> apoLayers;

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
        if ( CSLCount(psOptions->papszLayers) == 0)
        {
            nLayerCount = poDS->GetLayerCount();
            apoLayers.resize(nLayerCount);

            for( int iLayer = 0;
                 iLayer < nLayerCount;
                 iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch advertised layer %d!",
                            iLayer );
                    GDALVectorTranslateOptionsFree(psOptions);
                    if( hDstDS == nullptr ) GDALClose( poODS );
                    delete poGCPCoordTrans;
                    return nullptr;
                }

                apoLayers[iLayer] = poLayer;
            }
        }
/* -------------------------------------------------------------------- */
/*      Process specified data source layers.                           */
/* -------------------------------------------------------------------- */
        else
        {
            nLayerCount = CSLCount(psOptions->papszLayers);
            apoLayers.resize(nLayerCount);

            for( int iLayer = 0;
                psOptions->papszLayers[iLayer] != nullptr;
                iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayerByName(psOptions->papszLayers[iLayer]);

                if( poLayer == nullptr )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch requested layer '%s'!",
                             psOptions->papszLayers[iLayer] );
                    if (!psOptions->bSkipFailures)
                    {
                        GDALVectorTranslateOptionsFree(psOptions);
                        if( hDstDS == nullptr ) GDALClose( poODS );
                        delete poGCPCoordTrans;
                        return nullptr;
                    }
                }

                apoLayers[iLayer] = poLayer;
            }
        }

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            nLayerCount == 1 && psOptions->pszNewLayerName == nullptr &&
            VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode) &&
            (EQUAL(CPLGetExtension(osDestFilename), "shp") ||
             EQUAL(CPLGetExtension(osDestFilename), "shz") ||
             EQUAL(CPLGetExtension(osDestFilename), "dbf")) )
        {
            psOptions->pszNewLayerName = CPLStrdup(CPLGetBasename(osDestFilename));
        }

        std::vector<GIntBig> anLayerCountFeatures;
        anLayerCountFeatures.resize(nLayerCount);
        GIntBig nCountLayersFeatures = 0;
        GIntBig nAccCountFeatures = 0;

        /* First pass to apply filters and count all features if necessary */
        for( int iLayer = 0; iLayer < nLayerCount; iLayer++ )
        {
            OGRLayer        *poLayer = apoLayers[iLayer];
            if (poLayer == nullptr)
                continue;

            if( psOptions->pszWHERE != nullptr )
            {
                if( poLayer->SetAttributeFilter( psOptions->pszWHERE ) != OGRERR_NONE )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "SetAttributeFilter(%s) on layer '%s' failed.",
                             psOptions->pszWHERE, poLayer->GetName() );
                    if (!psOptions->bSkipFailures)
                    {
                        GDALVectorTranslateOptionsFree(psOptions);
                        if( hDstDS == nullptr ) GDALClose( poODS );
                        delete poGCPCoordTrans;
                        return nullptr;
                    }
                }
            }

            ApplySpatialFilter(poLayer, reinterpret_cast<OGRGeometry*>(psOptions->hSpatialFilter), poSpatSRS, psOptions->pszGeomField, poSourceSRS);

            if (psOptions->bDisplayProgress)
            {
                if (!poLayer->TestCapability(OLCFastFeatureCount))
                {
                    CPLError(CE_Warning, CPLE_NotSupported, "Progress turned off as fast feature count is not available.");
                    psOptions->bDisplayProgress = false;
                }
                else
                {
                    anLayerCountFeatures[iLayer] = poLayer->GetFeatureCount();
                    nCountLayersFeatures += anLayerCountFeatures[iLayer];
                }
            }
        }

        /* Second pass to do the real job */
        for( int iLayer = 0; iLayer < nLayerCount && nRetCode == 0; iLayer++ )
        {
            OGRLayer        *poLayer = apoLayers[iLayer];
            if (poLayer == nullptr)
                continue;

            GDALProgressFunc pfnProgress = nullptr;
            void        *pProgressArg = nullptr;

            OGRLayer* poPassedLayer = poLayer;
            if (psOptions->bSplitListFields)
            {
                auto poSLFLayer = new OGRSplitListFieldLayer(poPassedLayer, psOptions->nMaxSplitListSubFields);
                poPassedLayer = poSLFLayer;

                if (psOptions->bDisplayProgress && psOptions->nMaxSplitListSubFields != 1 &&
                    nCountLayersFeatures != 0)
                {
                    pfnProgress = GDALScaledProgress;
                    pProgressArg =
                        GDALCreateScaledProgress(nAccCountFeatures * 1.0 / nCountLayersFeatures,
                                                (nAccCountFeatures + anLayerCountFeatures[iLayer] / 2) * 1.0 / nCountLayersFeatures,
                                                psOptions->pfnProgress,
                                                psOptions->pProgressData);
                }
                else
                {
                    pfnProgress = nullptr;
                    pProgressArg = nullptr;
                }

                int nRet = poSLFLayer->BuildLayerDefn(pfnProgress, pProgressArg);
                if (!nRet)
                {
                    delete poPassedLayer;
                    poPassedLayer = poLayer;
                }

                if (psOptions->bDisplayProgress)
                    GDALDestroyScaledProgress(pProgressArg);
                pfnProgress = nullptr;
                pProgressArg = nullptr;
            }

            if (psOptions->bDisplayProgress)
            {
                if( nCountLayersFeatures != 0 )
                {
                    pfnProgress = GDALScaledProgress;
                    GIntBig nStart = 0;
                    if (poPassedLayer != poLayer && psOptions->nMaxSplitListSubFields != 1)
                        nStart = anLayerCountFeatures[iLayer] / 2;
                    pProgressArg =
                        GDALCreateScaledProgress((nAccCountFeatures + nStart) * 1.0 / nCountLayersFeatures,
                                                (nAccCountFeatures + anLayerCountFeatures[iLayer]) * 1.0 / nCountLayersFeatures,
                                                psOptions->pfnProgress,
                                                psOptions->pProgressData);
                }
            }

            nAccCountFeatures += anLayerCountFeatures[iLayer];

            auto psInfo = oSetup.Setup(poPassedLayer,
                                                   psOptions->pszNewLayerName,
                                                   psOptions,
                                                   nTotalEventsDone);

            poPassedLayer->ResetReading();

            if( (psInfo == nullptr ||
                !oTranslator.Translate( nullptr, psInfo.get(),
                                        anLayerCountFeatures[iLayer], nullptr,
                                        nTotalEventsDone,
                                        pfnProgress, pProgressArg, psOptions ))
                && !psOptions->bSkipFailures )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Terminating translation prematurely after failed\n"
                        "translation of layer %s (use -skipfailures to skip errors)",
                        poLayer->GetName() );

                nRetCode = 1;
            }

            if (poPassedLayer != poLayer)
                delete poPassedLayer;

            if (psOptions->bDisplayProgress)
                GDALDestroyScaledProgress(pProgressArg);
        }
    }
/* -------------------------------------------------------------------- */
/*      Process DS style table                                          */
/* -------------------------------------------------------------------- */

    poODS->SetStyleTable( poDS->GetStyleTable () );

    if( psOptions->nGroupTransactions )
    {
        if( !psOptions->nLayerTransaction )
        {
            if( nRetCode != 0 && !psOptions->bSkipFailures )
                poODS->RollbackTransaction();
            else
                poODS->CommitTransaction();
        }
    }

    delete poGCPCoordTrans;

    GDALVectorTranslateOptionsFree(psOptions);
    if(nRetCode == 0)
        return static_cast<GDALDatasetH>(poODS);

    if( hDstDS == nullptr ) GDALClose( poODS );
    return nullptr;
}

/************************************************************************/
/*                               SetZ()                                 */
/************************************************************************/

namespace {
class SetZVisitor: public OGRDefaultGeometryVisitor
{
        double m_dfZ;

    public:
        explicit SetZVisitor(double dfZ): m_dfZ(dfZ) {}

        using OGRDefaultGeometryVisitor::visit;

        void visit(OGRPoint* poPoint) override
        {
            poPoint->setZ(m_dfZ);
        }
};
}

static void SetZ (OGRGeometry* poGeom, double dfZ )
{
    if (poGeom == nullptr)
        return;
    SetZVisitor visitor(dfZ);
    poGeom->set3D(true);
    poGeom->accept(&visitor);
}

/************************************************************************/
/*                       ForceCoordDimension()                          */
/************************************************************************/

static int ForceCoordDimension(int eGType, int nCoordDim)
{
    if( nCoordDim == 2 && eGType != wkbNone )
        return wkbFlatten(eGType);
    else if( nCoordDim == 3 && eGType != wkbNone )
        return wkbSetZ(wkbFlatten(eGType));
    else if( nCoordDim == COORD_DIM_XYM && eGType != wkbNone )
        return wkbSetM(wkbFlatten(eGType));
    else if( nCoordDim == 4 && eGType != wkbNone )
        return OGR_GT_SetModifier(static_cast<OGRwkbGeometryType>(eGType), TRUE, TRUE);
    else
        return eGType;
}

/************************************************************************/
/*                   GetLayerAndOverwriteIfNecessary()                  */
/************************************************************************/

static OGRLayer* GetLayerAndOverwriteIfNecessary(GDALDataset *poDstDS,
                                                 const char* pszNewLayerName,
                                                 bool bOverwrite,
                                                 bool* pbErrorOccurred,
                                                 bool* pbOverwriteActuallyDone,
                                                 bool* pbAddOverwriteLCO)
{
    if( pbErrorOccurred )
        *pbErrorOccurred = false;
    if( pbOverwriteActuallyDone )
        *pbOverwriteActuallyDone = false;
    if( pbAddOverwriteLCO )
        *pbAddOverwriteLCO = false;

    /* GetLayerByName() can instantiate layers that would have been */
    /* 'hidden' otherwise, for example, non-spatial tables in a */
    /* PostGIS-enabled database, so this apparently useless command is */
    /* not useless. (#4012) */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRLayer* poDstLayer = poDstDS->GetLayerByName(pszNewLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    int iLayer = -1;
    if (poDstLayer != nullptr)
    {
        const int nLayerCount = poDstDS->GetLayerCount();
        for( iLayer = 0; iLayer < nLayerCount; iLayer++ )
        {
            OGRLayer        *poLayer = poDstDS->GetLayer(iLayer);
            if (poLayer == poDstLayer)
                break;
        }

        if (iLayer == nLayerCount)
            /* should not happen with an ideal driver */
            poDstLayer = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If the user requested overwrite, and we have the layer in       */
/*      question we need to delete it now so it will get recreated      */
/*      (overwritten).                                                  */
/* -------------------------------------------------------------------- */
    if( poDstLayer != nullptr && bOverwrite )
    {
        /* When using the CARTO driver we don't want to delete the layer if */
        /* it's going to be recreated. Instead we mark it to be overwritten */
        /* when the new creation is requested */
        if ( poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST) != nullptr &&
             strstr(poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST), "CARTODBFY") != nullptr )
        {
            if ( pbAddOverwriteLCO )
                *pbAddOverwriteLCO = true;
            if( pbOverwriteActuallyDone )
                *pbOverwriteActuallyDone = true;

        } else if( poDstDS->DeleteLayer( iLayer ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                     "DeleteLayer() failed when overwrite requested." );
            if( pbErrorOccurred )
                *pbErrorOccurred = true;
        }
        else
        {
            if( pbOverwriteActuallyDone )
                *pbOverwriteActuallyDone = true;
        }
        poDstLayer = nullptr;
    }

    return poDstLayer;
}

/************************************************************************/
/*                          ConvertType()                               */
/************************************************************************/

static OGRwkbGeometryType ConvertType(GeomTypeConversion eGeomTypeConversion,
                                      OGRwkbGeometryType eGType)
{
    OGRwkbGeometryType eRetType = eGType;

    if ( eGeomTypeConversion == GTC_CONVERT_TO_LINEAR ||
         eGeomTypeConversion == GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR )
    {
        eRetType = OGR_GT_GetLinear(eRetType);
    }

    if ( eGeomTypeConversion == GTC_PROMOTE_TO_MULTI ||
         eGeomTypeConversion == GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR )
    {
        if( eRetType == wkbTriangle || eRetType == wkbTIN ||
            eRetType == wkbPolyhedralSurface )
        {
            eRetType = wkbMultiPolygon;
        }
        else if( !OGR_GT_IsSubClassOf(eRetType, wkbGeometryCollection) )
        {
            eRetType = OGR_GT_GetCollection(eRetType);
        }
    }

    if ( eGeomTypeConversion == GTC_CONVERT_TO_CURVE )
        eRetType = OGR_GT_GetCurve(eRetType);

    return eRetType;
}

/************************************************************************/
/*                        DoFieldTypeConversion()                       */
/************************************************************************/

static
void DoFieldTypeConversion(GDALDataset* poDstDS, OGRFieldDefn& oFieldDefn,
                           char** papszFieldTypesToString,
                           char** papszMapFieldType,
                           bool bUnsetFieldWidth,
                           bool bQuiet,
                           bool bForceNullable,
                           bool bUnsetDefault)
{
    if (papszFieldTypesToString != nullptr )
    {
        CPLString osLookupString;
        osLookupString.Printf("%s(%s)",
                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                        OGRFieldDefn::GetFieldSubTypeName(oFieldDefn.GetSubType()));

        int iIdx = CSLFindString(papszFieldTypesToString, osLookupString);
        if( iIdx < 0 )
            iIdx = CSLFindString(papszFieldTypesToString,
                                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()));
        if( iIdx < 0 )
            iIdx = CSLFindString(papszFieldTypesToString, "All");
        if( iIdx >= 0 )
        {
            oFieldDefn.SetSubType(OFSTNone);
            oFieldDefn.SetType(OFTString);
        }
    }
    else if (papszMapFieldType != nullptr)
    {
        CPLString osLookupString;
        osLookupString.Printf("%s(%s)",
                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                        OGRFieldDefn::GetFieldSubTypeName(oFieldDefn.GetSubType()));

        const char* pszType = CSLFetchNameValue(papszMapFieldType, osLookupString);
        if( pszType == nullptr )
            pszType = CSLFetchNameValue(papszMapFieldType,
                                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()));
        if( pszType == nullptr )
            pszType = CSLFetchNameValue(papszMapFieldType, "All");
        if( pszType != nullptr )
        {
            int iSubType;
            int iType = GetFieldType(pszType, &iSubType);
            if( iType >= 0 && iSubType >= 0 )
            {
                oFieldDefn.SetSubType(OFSTNone);
                oFieldDefn.SetType(static_cast<OGRFieldType>(iType));
                oFieldDefn.SetSubType(static_cast<OGRFieldSubType>(iSubType));
                if( iType == OFTInteger )
                    oFieldDefn.SetWidth(0);
            }
        }
    }
    if( bUnsetFieldWidth )
    {
        oFieldDefn.SetWidth(0);
        oFieldDefn.SetPrecision(0);
    }
    if( bForceNullable )
        oFieldDefn.SetNullable(TRUE);
    if( bUnsetDefault )
        oFieldDefn.SetDefault(nullptr);

    if( poDstDS->GetDriver() != nullptr &&
        poDstDS->GetDriver()->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES) != nullptr &&
        strstr(poDstDS->GetDriver()->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES),
                OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType())) == nullptr )
    {
        if( oFieldDefn.GetType() == OFTInteger64 )
        {
            if( !bQuiet )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "The output driver does not seem to natively support %s "
                        "type for field %s. Converting it to Real instead. "
                        "-mapFieldType can be used to control field type conversion.",
                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                        oFieldDefn.GetNameRef());
            }
            oFieldDefn.SetType(OFTReal);
        }
        else if( !bQuiet )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "The output driver does not natively support %s type for "
                        "field %s. Misconversion can happen. "
                        "-mapFieldType can be used to control field type conversion.",
                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                        oFieldDefn.GetNameRef());
        }
    }
    else if( poDstDS->GetDriver() != nullptr &&
             poDstDS->GetDriver()->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES) == nullptr )
    {
        // All drivers supporting OFTInteger64 should advertise it theoretically
        if( oFieldDefn.GetType() == OFTInteger64 )
        {
            if( !bQuiet )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "The output driver does not seem to natively support %s type "
                        "for field %s. Converting it to Real instead. "
                        "-mapFieldType can be used to control field type conversion.",
                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                        oFieldDefn.GetNameRef());
            }
            oFieldDefn.SetType(OFTReal);
        }
    }
}

/************************************************************************/
/*                   SetupTargetLayer::Setup()                          */
/************************************************************************/

std::unique_ptr<TargetLayerInfo> SetupTargetLayer::Setup(OGRLayer* poSrcLayer,
                                         const char* pszNewLayerName,
                                         GDALVectorTranslateOptions *psOptions,
                                         GIntBig& nTotalEventsDone)
{
    int eGType = m_eGType;
    bool bPreserveFID = m_bPreserveFID;
    bool bAppend = m_bAppend;

    if( pszNewLayerName == nullptr )
        pszNewLayerName = poSrcLayer->GetName();

/* -------------------------------------------------------------------- */
/*      Get other info.                                                 */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poSrcFDefn = poSrcLayer->GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Find requested geometry fields.                                 */
/* -------------------------------------------------------------------- */
    std::vector<int> anRequestedGeomFields;
    const int nSrcGeomFieldCount = poSrcFDefn->GetGeomFieldCount();
    if (m_papszSelFields && !bAppend )
    {
        for( int iField=0; m_papszSelFields[iField] != nullptr; iField++)
        {
            int iSrcField = poSrcFDefn->GetFieldIndex(m_papszSelFields[iField]);
            if (iSrcField >= 0)
            {
                /* do nothing */
            }
            else
            {
                iSrcField = poSrcFDefn->GetGeomFieldIndex(m_papszSelFields[iField]);
                if( iSrcField >= 0)
                {
                    anRequestedGeomFields.push_back(iSrcField);
                }
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Field '%s' not found in source layer.",
                              m_papszSelFields[iField] );
                    if( !psOptions->bSkipFailures )
                        return nullptr;
                }
            }
        }

        if( anRequestedGeomFields.size() > 1 &&
            !m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Several geometry fields requested, but output "
                                "datasource does not support multiple geometry "
                                "fields." );
            if( !psOptions->bSkipFailures )
                return nullptr;
            else
                anRequestedGeomFields.resize(0);
        }
    }

    OGRSpatialReference* poOutputSRS = m_poOutputSRS;
    if( poOutputSRS == nullptr && !m_bNullifyOutputSRS )
    {
        if( nSrcGeomFieldCount == 1 || anRequestedGeomFields.empty() )
            poOutputSRS = poSrcLayer->GetSpatialRef();
        else if( anRequestedGeomFields.size() == 1 )
        {
            int iSrcGeomField = anRequestedGeomFields[0];
            poOutputSRS = poSrcFDefn->GetGeomFieldDefn(iSrcGeomField)->
                GetSpatialRef();
        }
    }

    int iSrcZField = -1;
    if (m_pszZField != nullptr)
    {
        iSrcZField = poSrcFDefn->GetFieldIndex(m_pszZField);
        if( iSrcZField < 0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "zfield '%s' does not exist in layer %s",
                     m_pszZField, poSrcLayer->GetName());
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the layer.                                                 */
/* -------------------------------------------------------------------- */

    bool bErrorOccurred;
    bool bOverwriteActuallyDone;
    bool bAddOverwriteLCO;
    OGRLayer *poDstLayer =
        GetLayerAndOverwriteIfNecessary(m_poDstDS,
                                        pszNewLayerName,
                                        m_bOverwrite,
                                        &bErrorOccurred,
                                        &bOverwriteActuallyDone,
                                        &bAddOverwriteLCO);
    if( bErrorOccurred )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If the layer does not exist, then create it.                    */
/* -------------------------------------------------------------------- */
    if( poDstLayer == nullptr )
    {
        if( !m_poDstDS->TestCapability( ODsCCreateLayer ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Layer '%s' does not already exist in the output dataset, and "
                      "cannot be created by the output driver.",
                      pszNewLayerName );
            return nullptr;
        }

        bool bForceGType = ( eGType != GEOMTYPE_UNCHANGED );
        if( !bForceGType )
        {
            if( anRequestedGeomFields.empty() )
            {
                eGType = poSrcFDefn->GetGeomType();
            }
            else if( anRequestedGeomFields.size() == 1  )
            {
                int iSrcGeomField = anRequestedGeomFields[0];
                eGType = poSrcFDefn->GetGeomFieldDefn(iSrcGeomField)->GetType();
            }
            else
            {
                eGType = wkbNone;
            }

            bool bHasZ = CPL_TO_BOOL(wkbHasZ(static_cast<OGRwkbGeometryType>(eGType)));
            eGType = ConvertType(m_eGeomTypeConversion, static_cast<OGRwkbGeometryType>(eGType));

            if ( m_bExplodeCollections )
            {
                const OGRwkbGeometryType eFGType = wkbFlatten(eGType);
                if (eFGType == wkbMultiPoint)
                {
                    eGType = wkbPoint;
                }
                else if (eFGType == wkbMultiLineString)
                {
                    eGType = wkbLineString;
                }
                else if (eFGType == wkbMultiPolygon)
                {
                    eGType = wkbPolygon;
                }
                else if (eFGType == wkbGeometryCollection ||
                         eFGType == wkbMultiCurve ||
                         eFGType == wkbMultiSurface)
                {
                    eGType = wkbUnknown;
                }
            }

            if ( bHasZ || (iSrcZField >= 0 && eGType != wkbNone) )
                eGType = wkbSetZ(static_cast<OGRwkbGeometryType>(eGType));
        }

        eGType = ForceCoordDimension(eGType, m_nCoordDim);

        CPLErrorReset();

        char** papszLCOTemp = CSLDuplicate(m_papszLCO);

        int eGCreateLayerType = eGType;
        if( anRequestedGeomFields.empty() &&
            nSrcGeomFieldCount > 1 &&
            m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
        {
            eGCreateLayerType = wkbNone;
        }
        // If the source layer has a single geometry column that is not nullable
        // and that ODsCCreateGeomFieldAfterCreateLayer is available, use it
        // so as to be able to set the not null constraint (if the driver supports it)
        // Same if the source geometry column has a non empty name that is not
        // overridden
        else if( eGType != wkbNone &&
                 anRequestedGeomFields.empty() &&
                 nSrcGeomFieldCount == 1 &&
                 m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) &&
                 ((!poSrcFDefn->GetGeomFieldDefn(0)->IsNullable() &&
                   CSLFetchNameValue(m_papszLCO, "GEOMETRY_NULLABLE") == nullptr &&
                   !m_bForceNullable) ||
                  (poSrcLayer->GetGeometryColumn() != nullptr &&
                   CSLFetchNameValue(m_papszLCO, "GEOMETRY_NAME") == nullptr &&
                   !EQUAL(poSrcLayer->GetGeometryColumn(), "") &&
                   poSrcFDefn->GetFieldIndex(poSrcLayer->GetGeometryColumn()) < 0)) )
        {
            anRequestedGeomFields.push_back(0);
            eGCreateLayerType = wkbNone;
        }
        else if( anRequestedGeomFields.size() == 1 &&
                 m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
        {
            eGCreateLayerType = wkbNone;
        }

        // If the source feature first geometry column is not nullable
        // and that GEOMETRY_NULLABLE creation option is available, use it
        // so as to be able to set the not null constraint (if the driver supports it)
        if( eGType != wkbNone &&
            anRequestedGeomFields.empty() &&
            nSrcGeomFieldCount >= 1 &&
            !poSrcFDefn->GetGeomFieldDefn(0)->IsNullable() &&
            m_poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST) != nullptr &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST), "GEOMETRY_NULLABLE") != nullptr &&
            CSLFetchNameValue(m_papszLCO, "GEOMETRY_NULLABLE") == nullptr &&
            !m_bForceNullable )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "GEOMETRY_NULLABLE", "NO");
            CPLDebug("GDALVectorTranslate", "Using GEOMETRY_NULLABLE=NO");
        }

        // Use source geometry field name as much as possible
        if( eGType != wkbNone &&
            m_poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST) != nullptr &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST), "GEOMETRY_NAME") != nullptr &&
            CSLFetchNameValue(m_papszLCO, "GEOMETRY_NAME") == nullptr )
        {
            int iSrcGeomField = -1;
            if( anRequestedGeomFields.empty() &&
                (nSrcGeomFieldCount == 1 ||
                 (!m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) &&
                  nSrcGeomFieldCount > 1) ) )
            {
                iSrcGeomField = 0;
            }
            else if( anRequestedGeomFields.size() == 1 )
            {
                iSrcGeomField = anRequestedGeomFields[0];
            }

            if( iSrcGeomField >= 0 )
            {
                const char* pszGFldName = poSrcFDefn->GetGeomFieldDefn(
                                                iSrcGeomField)->GetNameRef();
                if( pszGFldName != nullptr && !EQUAL(pszGFldName, "") &&
                    poSrcFDefn->GetFieldIndex(pszGFldName) < 0 )
                {
                    papszLCOTemp = CSLSetNameValue(papszLCOTemp,
                                                "GEOMETRY_NAME", pszGFldName);
                }
            }
        }

        // Force FID column as 64 bit if the source feature has a 64 bit FID,
        // the target driver supports 64 bit FID and the user didn't set it
        // manually.
        if( poSrcLayer->GetMetadataItem(OLMD_FID64) != nullptr &&
            EQUAL(poSrcLayer->GetMetadataItem(OLMD_FID64), "YES") &&
            m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST) != nullptr &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "FID64") != nullptr &&
            CSLFetchNameValue(m_papszLCO, "FID64") == nullptr )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "FID64", "YES");
            CPLDebug("GDALVectorTranslate", "Using FID64=YES");
        }

        // If output driver supports FID layer creation option, set it with
        // the FID column name of the source layer
        if( !m_bUnsetFid && !bAppend &&
            poSrcLayer->GetFIDColumn() != nullptr &&
            !EQUAL(poSrcLayer->GetFIDColumn(), "") &&
            m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST) != nullptr &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "='FID'") != nullptr &&
            CSLFetchNameValue(m_papszLCO, "FID") == nullptr )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "FID", poSrcLayer->GetFIDColumn());
            CPLDebug("GDALVectorTranslate", "Using FID=%s and -preserve_fid", poSrcLayer->GetFIDColumn());
            bPreserveFID = true;
        }

        // If bAddOverwriteLCO is ON (set up when overwriting a CARTO layer),
        // set OVERWRITE to YES so the new layer overwrites the old one
        if (bAddOverwriteLCO)
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "OVERWRITE", "ON");
            CPLDebug("GDALVectorTranslate", "Using OVERWRITE=ON");
        }

        if( m_bNativeData &&
            poSrcLayer->GetMetadataItem("NATIVE_DATA", "NATIVE_DATA") != nullptr &&
            poSrcLayer->GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA") != nullptr &&
            m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST) != nullptr &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "NATIVE_DATA") != nullptr &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "NATIVE_MEDIA_TYPE") != nullptr )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "NATIVE_DATA",
                    poSrcLayer->GetMetadataItem("NATIVE_DATA", "NATIVE_DATA"));
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "NATIVE_MEDIA_TYPE",
                    poSrcLayer->GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA"));
            CPLDebug("GDALVectorTranslate", "Transferring layer NATIVE_DATA");
        }

        OGRSpatialReference* poOutputSRSClone = nullptr;
        if( poOutputSRS != nullptr )
        {
            poOutputSRSClone = poOutputSRS->Clone();
        }
        poDstLayer = m_poDstDS->CreateLayer( pszNewLayerName, poOutputSRSClone,
                                           static_cast<OGRwkbGeometryType>(eGCreateLayerType),
                                           papszLCOTemp );
        CSLDestroy(papszLCOTemp);

        if( poOutputSRSClone != nullptr )
        {
            poOutputSRSClone->Release();
        }

        if( poDstLayer == nullptr )
        {
            return nullptr;
        }

        if( m_bCopyMD )
        {
            char** papszDomains = poSrcLayer->GetMetadataDomainList();
            for(char** papszIter = papszDomains; papszIter && *papszIter; ++papszIter )
            {
                if( !EQUAL(*papszIter, "IMAGE_STRUCTURE") &&
                    !EQUAL(*papszIter, "SUBDATASETS") )
                {
                    char** papszMD = poSrcLayer->GetMetadata(*papszIter);
                    if( papszMD )
                        poDstLayer->SetMetadata(papszMD, *papszIter);
                }
            }
            CSLDestroy(papszDomains);
        }

        if( anRequestedGeomFields.empty() &&
            nSrcGeomFieldCount > 1 &&
            m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
        {
            for(int i = 0; i < nSrcGeomFieldCount; i ++)
            {
                anRequestedGeomFields.push_back(i);
            }
        }

        if( anRequestedGeomFields.size() > 1 ||
            (anRequestedGeomFields.size() == 1 &&
                 m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer)) )
        {
          for( int i = 0; i < static_cast<int>(anRequestedGeomFields.size());
               i++ )
            {
                const int iSrcGeomField = anRequestedGeomFields[i];
                OGRGeomFieldDefn oGFldDefn
                    (poSrcFDefn->GetGeomFieldDefn(iSrcGeomField));
                if( m_poOutputSRS != nullptr )
                {
                    poOutputSRSClone = m_poOutputSRS->Clone();
                    oGFldDefn.SetSpatialRef(poOutputSRSClone);
                    poOutputSRSClone->Release();
                }
                if( bForceGType )
                {
                    oGFldDefn.SetType(static_cast<OGRwkbGeometryType>(eGType));
                }
                else
                {
                    eGType = oGFldDefn.GetType();
                    eGType = ConvertType(
                        m_eGeomTypeConversion,
                        static_cast<OGRwkbGeometryType>(eGType));
                    eGType = ForceCoordDimension(eGType, m_nCoordDim);
                    oGFldDefn.SetType(static_cast<OGRwkbGeometryType>(eGType));
                }
                if( m_bForceNullable )
                    oGFldDefn.SetNullable(TRUE);
                poDstLayer->CreateGeomField(&oGFldDefn);
            }
        }

        bAppend = false;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we will append to it, if append was requested.        */
/* -------------------------------------------------------------------- */
    else if( !bAppend && !m_bNewDataSource )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Layer %s already exists, and -append not specified.\n"
                         "        Consider using -append, or -overwrite.",
                pszNewLayerName );
        return nullptr;
    }
    else
    {
        if( CSLCount(m_papszLCO) > 0 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, "Layer creation options ignored since an existing layer is\n"
                                                  "         being appended to." );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process Layer style table                                       */
/* -------------------------------------------------------------------- */

    poDstLayer->SetStyleTable( poSrcLayer->GetStyleTable () );
/* -------------------------------------------------------------------- */
/*      Add fields.  Default to copy all field.                         */
/*      If only a subset of all fields requested, then output only      */
/*      the selected fields, and in the order that they were            */
/*      selected.                                                       */
/* -------------------------------------------------------------------- */
    const int nSrcFieldCount = poSrcFDefn->GetFieldCount();
    int         iSrcFIDField = -1;

    // Initialize the index-to-index map to -1's
    std::vector<int> anMap(nSrcFieldCount, -1);

    std::map<int, TargetLayerInfo::ResolvedInfo> oMapResolved;

    /* Caution : at the time of writing, the MapInfo driver */
    /* returns NULL until a field has been added */
    OGRFeatureDefn *poDstFDefn = poDstLayer->GetLayerDefn();

    if (m_papszFieldMap && bAppend)
    {
        bool bIdentity = false;

        if (EQUAL(m_papszFieldMap[0], "identity"))
            bIdentity = true;
        else if (CSLCount(m_papszFieldMap) != nSrcFieldCount)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Field map should contain the value 'identity' or "
                    "the same number of integer values as the source field count.");
            return nullptr;
        }

        for( int iField=0; iField < nSrcFieldCount; iField++)
        {
            anMap[iField] = bIdentity? iField : atoi(m_papszFieldMap[iField]);
            if (anMap[iField] >= poDstFDefn->GetFieldCount())
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Invalid destination field index %d.", anMap[iField]);
                return nullptr;
            }
        }
    }
    else if (m_papszSelFields && !bAppend )
    {
        int nDstFieldCount = poDstFDefn ? poDstFDefn->GetFieldCount() : 0;
        for( int iField=0; m_papszSelFields[iField] != nullptr; iField++)
        {
            const int iSrcField =
                poSrcFDefn->GetFieldIndex(m_papszSelFields[iField]);
            if (iSrcField >= 0)
            {
                OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iSrcField);
                OGRFieldDefn oFieldDefn( poSrcFieldDefn );

                DoFieldTypeConversion(m_poDstDS, oFieldDefn,
                                      m_papszFieldTypesToString,
                                      m_papszMapFieldType,
                                      m_bUnsetFieldWidth,
                                      psOptions->bQuiet,
                                      m_bForceNullable,
                                      m_bUnsetDefault);

                /* The field may have been already created at layer creation */
                const int iDstField =
                    poDstFDefn
                    ? poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef())
                    : -1;
                if (iDstField >= 0)
                {
                    anMap[iSrcField] = iDstField;
                }
                else if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
                {
                    /* now that we've created a field, GetLayerDefn() won't return NULL */
                    if (poDstFDefn == nullptr)
                        poDstFDefn = poDstLayer->GetLayerDefn();

                    /* Sanity check : if it fails, the driver is buggy */
                    if (poDstFDefn != nullptr &&
                        poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "The output driver has claimed to have added the %s field, but it did not!",
                                 oFieldDefn.GetNameRef() );
                    }
                    else
                    {
                        anMap[iSrcField] = nDstFieldCount;
                        nDstFieldCount ++;
                    }
                }
            }
        }

        /* -------------------------------------------------------------------- */
        /* Use SetIgnoredFields() on source layer if available                  */
        /* -------------------------------------------------------------------- */
        if (poSrcLayer->TestCapability(OLCIgnoreFields))
        {
            bool bUseIgnoredFields = true;
            char** papszWHEREUsedFields = nullptr;

            if (m_pszWHERE)
            {
                /* We must not ignore fields used in the -where expression (#4015) */
                OGRFeatureQuery oFeatureQuery;
                if ( oFeatureQuery.Compile( poSrcLayer->GetLayerDefn(), m_pszWHERE, FALSE, nullptr ) == OGRERR_NONE )
                {
                    papszWHEREUsedFields = oFeatureQuery.GetUsedFields();
                }
                else
                {
                    bUseIgnoredFields = false;
                }
            }

            char** papszIgnoredFields = nullptr;

            for(int iSrcField=0;
                bUseIgnoredFields && iSrcField<poSrcFDefn->GetFieldCount();
                iSrcField++)
            {
                const char* pszFieldName =
                    poSrcFDefn->GetFieldDefn(iSrcField)->GetNameRef();
                bool bFieldRequested = false;
                for( int iField=0; m_papszSelFields[iField] != nullptr; iField++)
                {
                    if (EQUAL(pszFieldName, m_papszSelFields[iField]))
                    {
                        bFieldRequested = true;
                        break;
                    }
                }
                bFieldRequested |= CSLFindString(papszWHEREUsedFields, pszFieldName) >= 0;
                bFieldRequested |= (m_pszZField != nullptr && EQUAL(pszFieldName, m_pszZField));

                /* If source field not requested, add it to ignored files list */
                if (!bFieldRequested)
                    papszIgnoredFields = CSLAddString(papszIgnoredFields, pszFieldName);
            }
            if (bUseIgnoredFields)
                poSrcLayer->SetIgnoredFields(const_cast<const char**>(papszIgnoredFields));
            CSLDestroy(papszIgnoredFields);
            CSLDestroy(papszWHEREUsedFields);
        }
    }
    else if( !bAppend || m_bAddMissingFields )
    {
        int nDstFieldCount = poDstFDefn ? poDstFDefn->GetFieldCount() : 0;

        const bool caseInsensitive =
            !EQUAL(m_poDstDS->GetDriver()->GetDescription(), "GeoJSON");
        const auto formatName = [caseInsensitive](const char* name) {
            if( caseInsensitive ) {
                return CPLString(name).toupper();
            } else {
                return CPLString(name);
            }
        };

        /* Save the map of existing fields, before creating new ones */
        /* This helps when converting a source layer that has duplicated field names */
        /* which is a bad idea */
        std::map<CPLString, int> oMapPreExistingFields;
        std::unordered_set<std::string> oSetDstFieldNames;
        for( int iField = 0; iField < nDstFieldCount; iField++ )
        {
            const char* pszFieldName = poDstFDefn->GetFieldDefn(iField)->GetNameRef();
            CPLString osUpperFieldName(formatName(pszFieldName));
            oSetDstFieldNames.insert(osUpperFieldName);
            if( oMapPreExistingFields.find(osUpperFieldName) ==
                                            oMapPreExistingFields.end() )
                oMapPreExistingFields[osUpperFieldName] = iField;
            /*else
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The target layer has already a duplicated field name '%s' before "
                         "adding the fields of the source layer", pszFieldName); */
        }

        const char* pszFIDColumn = poDstLayer->GetFIDColumn();

        std::vector<int> anSrcFieldIndices;
        if( m_papszSelFields )
        {
            for( int iField=0; m_papszSelFields[iField] != nullptr; iField++)
            {
                const int iSrcField =
                    poSrcFDefn->GetFieldIndex(m_papszSelFields[iField]);
                if (iSrcField >= 0)
                {
                    anSrcFieldIndices.push_back(iSrcField);
                }
            }
        }
        else
        {
            for( int iField = 0; iField < nSrcFieldCount; iField++ )
            {
                anSrcFieldIndices.push_back(iField);
            }
        }

        std::unordered_set<std::string> oSetSrcFieldNames;
        for( int i = 0; i < poSrcFDefn->GetFieldCount(); i++ )
        {
            oSetSrcFieldNames.insert(
                formatName(poSrcFDefn->GetFieldDefn(i)->GetNameRef()));
        }

        // For each source field name, memorize the last number suffix to have unique
        // field names in the target.
        // Let's imagine we have a source layer with the field name foo repeated twice
        // After dealing the first field, oMapFieldNameToLastSuffix["foo"] will be
        // 1, so when starting a unique name for the second field, we'll be able to
        // start at 2.
        // This avoids quadratic complexity if a big number of source field names
        // are identical.
        // Like in https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=37768
        std::map<std::string, int> oMapFieldNameToLastSuffix;

        for( size_t i = 0; i < anSrcFieldIndices.size(); i++ )
        {
            const int iField = anSrcFieldIndices[i];
            const OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iField);
            OGRFieldDefn oFieldDefn( poSrcFieldDefn );

            // Avoid creating a field with the same name as the FID column
            if( pszFIDColumn != nullptr && EQUAL(pszFIDColumn, oFieldDefn.GetNameRef()) &&
                (oFieldDefn.GetType() == OFTInteger || oFieldDefn.GetType() == OFTInteger64) )
            {
                iSrcFIDField = iField;
                continue;
            }

            DoFieldTypeConversion(m_poDstDS, oFieldDefn,
                                  m_papszFieldTypesToString,
                                  m_papszMapFieldType,
                                  m_bUnsetFieldWidth,
                                  psOptions->bQuiet,
                                  m_bForceNullable,
                                  m_bUnsetDefault);

            /* The field may have been already created at layer creation */
            {
                const auto oIter =
                    oMapPreExistingFields.find(formatName(oFieldDefn.GetNameRef()));
                if( oIter != oMapPreExistingFields.end() )
                {
                    anMap[iField] = oIter->second;
                    continue;
                }
            }

            bool bHasRenamed = false;
            /* In case the field name already exists in the target layer, */
            /* build a unique field name */
            if( oSetDstFieldNames.find(
                    formatName(oFieldDefn.GetNameRef())) !=
                                                    oSetDstFieldNames.end() )
            {
                const CPLString osTmpNameRaddixUC(formatName(oFieldDefn.GetNameRef()));
                int nTry = 1;
                const auto oIter = oMapFieldNameToLastSuffix.find(osTmpNameRaddixUC);
                if( oIter != oMapFieldNameToLastSuffix.end() )
                    nTry = oIter->second;
                CPLString osTmpNameUC = osTmpNameRaddixUC;
                osTmpNameUC.reserve(osTmpNameUC.size() + 10);
                while( true )
                {
                    ++nTry;
                    char szTry[32];
                    snprintf(szTry, sizeof(szTry), "%d", nTry);
                    osTmpNameUC.replace(osTmpNameRaddixUC.size(), std::string::npos, szTry);

                    /* Check that the proposed name doesn't exist either in the already */
                    /* created fields or in the source fields */
                    if( oSetDstFieldNames.find(osTmpNameUC) ==
                                                    oSetDstFieldNames.end() &&
                        oSetSrcFieldNames.find(osTmpNameUC) ==
                                                    oSetSrcFieldNames.end() )
                    {
                        bHasRenamed = true;
                        oFieldDefn.SetName((CPLString(oFieldDefn.GetNameRef()) + szTry).c_str());
                        oMapFieldNameToLastSuffix[osTmpNameRaddixUC] = nTry;
                        break;
                    }
                }
            }

            // Create field domain in output dataset if not already existing.
            const auto osDomainName = oFieldDefn.GetDomainName();
            if( !osDomainName.empty() )
            {
                if( m_poDstDS->TestCapability(ODsCAddFieldDomain) &&
                    m_poDstDS->GetFieldDomain(osDomainName) == nullptr )
                {
                    const auto poSrcDomain =
                        m_poSrcDS->GetFieldDomain(osDomainName);
                    if( poSrcDomain )
                    {
                        std::string failureReason;
                        if( !m_poDstDS->AddFieldDomain(
                                std::unique_ptr<OGRFieldDomain>(poSrcDomain->Clone()),
                                failureReason) )
                        {
                            oFieldDefn.SetDomainName(std::string());
                            CPLDebug("OGR2OGR", "Cannot create domain %s: %s",
                                     osDomainName.c_str(), failureReason.c_str());
                        }
                    }
                    else
                    {
                        CPLDebug("OGR2OGR",
                                 "Cannot find domain %s in source dataset",
                                 osDomainName.c_str());
                    }
                }
                if( m_poDstDS->GetFieldDomain(osDomainName) == nullptr )
                {
                    oFieldDefn.SetDomainName(std::string());
                }
            }

            if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
            {
                /* now that we've created a field, GetLayerDefn() won't return NULL */
                if (poDstFDefn == nullptr)
                    poDstFDefn = poDstLayer->GetLayerDefn();

                /* Sanity check : if it fails, the driver is buggy */
                if (poDstFDefn != nullptr &&
                    poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The output driver has claimed to have added the %s field, but it did not!",
                             oFieldDefn.GetNameRef() );
                }
                else
                {
                    if( poDstFDefn != nullptr )
                    {
                        const char* pszNewFieldName =
                            poDstFDefn->GetFieldDefn(nDstFieldCount)->GetNameRef();
                        if( bHasRenamed )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "Field '%s' already exists. Renaming it as '%s'",
                                    poSrcFieldDefn->GetNameRef(), pszNewFieldName);
                        }
                        oSetDstFieldNames.insert(formatName(pszNewFieldName));
                    }

                    anMap[iField] = nDstFieldCount;
                    nDstFieldCount ++;
                }
            }

            if( m_bResolveDomains && !osDomainName.empty() )
            {
                const auto poSrcDomain =
                        m_poSrcDS->GetFieldDomain(osDomainName);
                if( poSrcDomain && poSrcDomain->GetDomainType() == OFDT_CODED )
                {
                    OGRFieldDefn oResolvedField(
                        CPLSPrintf("%s_resolved", oFieldDefn.GetNameRef()),
                        OFTString );
                    if (poDstLayer->CreateField( &oResolvedField ) == OGRERR_NONE)
                    {
                        TargetLayerInfo::ResolvedInfo resolvedInfo;
                        resolvedInfo.nSrcField = iField;
                        resolvedInfo.poDomain = poSrcDomain;
                        oMapResolved[nDstFieldCount] = resolvedInfo;
                        nDstFieldCount ++;
                    }
                }
            }
        }
    }
    else
    {
        /* For an existing layer, build the map by fetching the index in the destination */
        /* layer for each source field */
        if (poDstFDefn == nullptr)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "poDstFDefn == NULL." );
            return nullptr;
        }

        for( int iField = 0; iField < nSrcFieldCount; iField++ )
        {
            OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iField);
            const int iDstField =
                poDstLayer->FindFieldIndex(poSrcFieldDefn->GetNameRef(),
                                           m_bExactFieldNameMatch);
            if (iDstField >= 0)
                anMap[iField] = iDstField;
            else
                CPLDebug("GDALVectorTranslate", "Skipping field '%s' not found in destination layer '%s'.",
                         poSrcFieldDefn->GetNameRef(), poDstLayer->GetName() );
        }
    }

    if( bOverwriteActuallyDone && !bAddOverwriteLCO &&
        EQUAL(m_poDstDS->GetDriver()->GetDescription(), "PostgreSQL") &&
        !psOptions->nLayerTransaction &&
        psOptions->nGroupTransactions >= 0 &&
        CPLTestBool(CPLGetConfigOption("PG_COMMIT_WHEN_OVERWRITING", "YES")) )
    {
        CPLDebug("GDALVectorTranslate",
                 "Forcing transaction commit as table overwriting occurred");
        // Commit when overwriting as this consumes a lot of PG resources
        // and could result in """out of shared memory.
        // You might need to increase max_locks_per_transaction."""" errors
        if( m_poDstDS->CommitTransaction() == OGRERR_FAILURE ||
            m_poDstDS->StartTransaction(psOptions->bForceTransaction) == OGRERR_FAILURE )
        {
            return nullptr;
        }
        nTotalEventsDone = 0;
    }

    std::unique_ptr<TargetLayerInfo> psInfo(new TargetLayerInfo);
    psInfo->m_nFeaturesRead = 0;
    psInfo->m_bPerFeatureCT = false;
    psInfo->m_poSrcLayer = poSrcLayer;
    psInfo->m_poDstLayer = poDstLayer;
    psInfo->m_apoCT.resize(poDstLayer->GetLayerDefn()->GetGeomFieldCount());
    psInfo->m_aosTransformOptions.resize(poDstLayer->GetLayerDefn()->GetGeomFieldCount());
    psInfo->m_anMap = std::move(anMap);
    psInfo->m_iSrcZField = iSrcZField;
    psInfo->m_iSrcFIDField = iSrcFIDField;
    if( anRequestedGeomFields.size() == 1 )
        psInfo->m_iRequestedSrcGeomField = anRequestedGeomFields[0];
    else
        psInfo->m_iRequestedSrcGeomField = -1;
    psInfo->m_bPreserveFID = bPreserveFID;
    psInfo->m_pszCTPipeline = m_pszCTPipeline;
    psInfo->m_oMapResolved = std::move(oMapResolved);
    for( const auto& kv: psInfo->m_oMapResolved )
    {
        const auto poDomain = kv.second.poDomain;
        const auto poCodedDomain =
            cpl::down_cast<const OGRCodedFieldDomain*>(poDomain);
        const auto enumeration = poCodedDomain->GetEnumeration();
        std::map<std::string, std::string> oMapCodeValue;
        for( int i = 0; enumeration[i].pszCode != nullptr; ++i )
        {
            oMapCodeValue[enumeration[i].pszCode] =
                enumeration[i].pszValue ? enumeration[i].pszValue : "";
        }
        psInfo->m_oMapDomainToKV[poDomain] = std::move(oMapCodeValue);
    }

    return psInfo;
}

/************************************************************************/
/*                               SetupCT()                              */
/************************************************************************/

static bool SetupCT( TargetLayerInfo* psInfo,
                    OGRLayer* poSrcLayer,
                    bool bTransform,
                    bool bWrapDateline,
                    const CPLString& osDateLineOffset,
                    OGRSpatialReference* poUserSourceSRS,
                    OGRFeature* poFeature,
                    OGRSpatialReference* poOutputSRS,
                    OGRCoordinateTransformation* poGCPCoordTrans)
{
    OGRLayer    *poDstLayer = psInfo->m_poDstLayer;
    const int nDstGeomFieldCount =
        poDstLayer->GetLayerDefn()->GetGeomFieldCount();
    for( int iGeom = 0; iGeom < nDstGeomFieldCount; iGeom ++ )
    {
/* -------------------------------------------------------------------- */
/*      Setup coordinate transformation if we need it.                  */
/* -------------------------------------------------------------------- */
        OGRSpatialReference* poSourceSRS = nullptr;
        OGRCoordinateTransformation* poCT = nullptr;
        char** papszTransformOptions = nullptr;

        int iSrcGeomField;
        auto poDstGeomFieldDefn = poDstLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
        if( psInfo->m_iRequestedSrcGeomField >= 0 )
        {
            iSrcGeomField = psInfo->m_iRequestedSrcGeomField;
        }
        else
        {
            iSrcGeomField = poSrcLayer->GetLayerDefn()->GetGeomFieldIndex(
                poDstGeomFieldDefn->GetNameRef());
            if( iSrcGeomField < 0 )
            {
                if( nDstGeomFieldCount == 1 &&
                    poSrcLayer->GetLayerDefn()->GetGeomFieldCount() > 0 )
                {
                    iSrcGeomField = 0;
                }
                else
                {
                    continue;
                }
            }
        }

        if( psInfo->m_nFeaturesRead == 0 )
        {
            poSourceSRS = poUserSourceSRS;
            if( poSourceSRS == nullptr )
            {
                if( iSrcGeomField > 0 )
                    poSourceSRS = poSrcLayer->GetLayerDefn()->
                        GetGeomFieldDefn(iSrcGeomField)->GetSpatialRef();
                else
                    poSourceSRS = poSrcLayer->GetSpatialRef();
            }
        }
        if( poSourceSRS == nullptr )
        {
            OGRGeometry* poSrcGeometry =
                poFeature->GetGeomFieldRef(iSrcGeomField);
            if( poSrcGeometry )
                poSourceSRS = poSrcGeometry->getSpatialReference();
            psInfo->m_bPerFeatureCT = (bTransform || bWrapDateline);
        }

        if( bTransform )
        {
            if( poSourceSRS == nullptr && psInfo->m_pszCTPipeline == nullptr )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Can't transform coordinates, source layer has no\n"
                        "coordinate system.  Use -s_srs to set one." );

                return false;
            }

            if( psInfo->m_pszCTPipeline == nullptr )
            {
                CPLAssert( nullptr != poSourceSRS );
                CPLAssert( nullptr != poOutputSRS );
            }

            if( psInfo->m_apoCT[iGeom] != nullptr &&
                psInfo->m_apoCT[iGeom]->GetSourceCS() == poSourceSRS )
            {
                poCT = psInfo->m_apoCT[iGeom].get();
            }
            else
            {
                OGRCoordinateTransformationOptions options;
                if( psInfo->m_pszCTPipeline )
                {
                    options.SetCoordinateOperation( psInfo->m_pszCTPipeline, false );
                }
                poCT = OGRCreateCoordinateTransformation( poSourceSRS, poOutputSRS, options );
                if( poCT == nullptr )
                {
                    char        *pszWKT = nullptr;

                    CPLError( CE_Failure, CPLE_AppDefined,
                        "Failed to create coordinate transformation between the\n"
                        "following coordinate systems.  This may be because they\n"
                        "are not transformable." );

                    if( poSourceSRS )
                    {
                        poSourceSRS->exportToPrettyWkt( &pszWKT, FALSE );
                        CPLError( CE_Failure, CPLE_AppDefined,  "Source:\n%s", pszWKT );
                        CPLFree(pszWKT);
                    }

                    if( poOutputSRS )
                    {
                        poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
                        CPLError( CE_Failure, CPLE_AppDefined,  "Target:\n%s", pszWKT );
                        CPLFree(pszWKT);
                    }

                    return false;
                }
                poCT = new CompositeCT( poGCPCoordTrans, false, poCT, true );
                psInfo->m_apoCT[iGeom].reset(poCT);
            }
        }
        else
        {
            const char* const apszOptions[] = {
                "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES",
                "CRITERION=EQUIVALENT", nullptr };
            auto poDstGeomFieldDefnSpatialRef = poDstGeomFieldDefn->GetSpatialRef();
            if( poSourceSRS && poDstGeomFieldDefnSpatialRef &&
                poSourceSRS->GetDataAxisToSRSAxisMapping() !=
                    poDstGeomFieldDefnSpatialRef->GetDataAxisToSRSAxisMapping() &&
                poSourceSRS->IsSame(poDstGeomFieldDefnSpatialRef, apszOptions) )
            {
                psInfo->m_apoCT[iGeom].reset(new CompositeCT(
                    new AxisMappingCoordinateTransformation(
                        poSourceSRS->GetDataAxisToSRSAxisMapping(),
                        poDstGeomFieldDefnSpatialRef->GetDataAxisToSRSAxisMapping()),
                    true,
                    poGCPCoordTrans,
                    false));
                poCT = psInfo->m_apoCT[iGeom].get();
            }
            else if( poGCPCoordTrans )
            {
                psInfo->m_apoCT[iGeom].reset(new CompositeCT(
                    poGCPCoordTrans, false, nullptr, false));
                poCT = psInfo->m_apoCT[iGeom].get();
            }
        }

        if (bWrapDateline)
        {
            if (bTransform && poCT != nullptr && poOutputSRS != nullptr && poOutputSRS->IsGeographic())
            {
                papszTransformOptions =
                    CSLAddString(papszTransformOptions, "WRAPDATELINE=YES");
                if( !osDateLineOffset.empty() )
                {
                    CPLString soOffset("DATELINEOFFSET=");
                    soOffset += osDateLineOffset;
                    papszTransformOptions =
                        CSLAddString(papszTransformOptions, soOffset);
                }
            }
            else if (poSourceSRS != nullptr && poSourceSRS->IsGeographic())
            {
                papszTransformOptions =
                    CSLAddString(papszTransformOptions, "WRAPDATELINE=YES");
                if( !osDateLineOffset.empty() )
                {
                    CPLString soOffset("DATELINEOFFSET=");
                    soOffset += osDateLineOffset;
                    papszTransformOptions =
                        CSLAddString(papszTransformOptions, soOffset);
                }
            }
            else
            {
                static bool bHasWarned = false;
                if( !bHasWarned )
                    CPLError( CE_Failure, CPLE_IllegalArg, "-wrapdateline option only works when reprojecting to a geographic SRS");
                bHasWarned = true;
            }

            psInfo->m_aosTransformOptions[iGeom].Assign(papszTransformOptions);
        }
    }
    return true;
}

/************************************************************************/
/*                     LayerTranslator::Translate()                     */
/************************************************************************/

int LayerTranslator::Translate( OGRFeature* poFeatureIn,
                                TargetLayerInfo* psInfo,
                                GIntBig nCountLayerFeatures,
                                GIntBig* pnReadFeatureCount,
                                GIntBig& nTotalEventsDone,
                                GDALProgressFunc pfnProgress,
                                void *pProgressArg,
                                GDALVectorTranslateOptions *psOptions )
{
    const int eGType = m_eGType;
    OGRSpatialReference* poOutputSRS = m_poOutputSRS;

    OGRLayer *poSrcLayer = psInfo->m_poSrcLayer;
    OGRLayer *poDstLayer = psInfo->m_poDstLayer;
    const int* const panMap = psInfo->m_anMap.data();
    const int iSrcZField = psInfo->m_iSrcZField;
    const bool bPreserveFID = psInfo->m_bPreserveFID;
    const int nSrcGeomFieldCount = poSrcLayer->GetLayerDefn()->GetGeomFieldCount();
    const int nDstGeomFieldCount = poDstLayer->GetLayerDefn()->GetGeomFieldCount();
    const bool bExplodeCollections = m_bExplodeCollections && nDstGeomFieldCount <= 1;
    const int iRequestedSrcGeomField = psInfo->m_iRequestedSrcGeomField;

    if( poOutputSRS == nullptr && !m_bNullifyOutputSRS )
    {
        if( nSrcGeomFieldCount == 1 )
        {
            poOutputSRS = poSrcLayer->GetSpatialRef();
        }
        else if( iRequestedSrcGeomField > 0 )
        {
            poOutputSRS = poSrcLayer->GetLayerDefn()->GetGeomFieldDefn(
                iRequestedSrcGeomField)->GetSpatialRef();
        }
    }

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    if( psOptions->nGroupTransactions )
    {
        if( psOptions->nLayerTransaction )
        {
            if( poDstLayer->StartTransaction() == OGRERR_FAILURE )
                return false;
        }
    }

    OGRFeature *poFeature = nullptr;
    int         nFeaturesInTransaction = 0;
    GIntBig      nCount = 0; /* written + failed */
    GIntBig      nFeaturesWritten = 0;

    bool bRet = true;
    CPLErrorReset();
    while( true )
    {
        if( m_nLimit >= 0 && psInfo->m_nFeaturesRead >= m_nLimit )
        {
            break;
        }

        if( poFeatureIn != nullptr )
            poFeature = poFeatureIn;
        else if( psOptions->nFIDToFetch != OGRNullFID )
            poFeature = poSrcLayer->GetFeature(psOptions->nFIDToFetch);
        else
            poFeature = poSrcLayer->GetNextFeature();

        if( poFeature == nullptr )
        {
            if( CPLGetLastErrorType() == CE_Failure )
            {
                bRet = false;
            }
            break;
        }

        if( psInfo->m_nFeaturesRead == 0 || psInfo->m_bPerFeatureCT )
        {
            if( !SetupCT( psInfo, poSrcLayer, m_bTransform, m_bWrapDateline,
                          m_osDateLineOffset, m_poUserSourceSRS,
                          poFeature, poOutputSRS, m_poGCPCoordTrans) )
            {
                OGRFeature::DestroyFeature( poFeature );
                return false;
            }
        }

        psInfo->m_nFeaturesRead ++;

        int nIters = 1;
        std::unique_ptr<OGRGeometryCollection> poCollToExplode;
        int iGeomCollToExplode = -1;
        if (bExplodeCollections)
        {
            OGRGeometry* poSrcGeometry;
            if( iRequestedSrcGeomField >= 0 )
                poSrcGeometry = poFeature->GetGeomFieldRef(
                                        iRequestedSrcGeomField);
            else
                poSrcGeometry = poFeature->GetGeometryRef();
            if (poSrcGeometry &&
                OGR_GT_IsSubClassOf(poSrcGeometry->getGeometryType(), wkbGeometryCollection) )
            {
                const int nParts = poSrcGeometry->toGeometryCollection()->getNumGeometries();
                if( nParts > 0 )
                {
                    iGeomCollToExplode = iRequestedSrcGeomField >= 0 ?
                        iRequestedSrcGeomField : 0;
                    poCollToExplode.reset(
                        poFeature->StealGeometry(iGeomCollToExplode)->toGeometryCollection());
                    nIters = nParts;
                }
            }
        }

        OGRFeature *poDstFeature = nullptr;
        for(int iPart = 0; iPart < nIters; iPart++)
        {
            if( psOptions->nLayerTransaction &&
                ++nFeaturesInTransaction == psOptions->nGroupTransactions )
            {
                if( poDstLayer->CommitTransaction() == OGRERR_FAILURE ||
                    poDstLayer->StartTransaction() == OGRERR_FAILURE )
                {
                    OGRFeature::DestroyFeature( poFeature );
                    return false;
                }
                nFeaturesInTransaction = 0;
            }
            else if( !psOptions->nLayerTransaction &&
                     psOptions->nGroupTransactions >= 0 &&
                     ++nTotalEventsDone >= psOptions->nGroupTransactions )
            {
                if( m_poODS->CommitTransaction() == OGRERR_FAILURE ||
                        m_poODS->StartTransaction(psOptions->bForceTransaction) == OGRERR_FAILURE )
                {
                    OGRFeature::DestroyFeature( poFeature );
                    return false;
                }
                nTotalEventsDone = 0;
            }

            CPLErrorReset();
            poDstFeature = OGRFeature::CreateFeature( poDstLayer->GetLayerDefn() );

            /* Optimization to avoid duplicating the source geometry in the */
            /* target feature : we steal it from the source feature for now... */
            OGRGeometry* poStolenGeometry = nullptr;
            if( !bExplodeCollections && nSrcGeomFieldCount == 1 &&
                (nDstGeomFieldCount == 1 ||
                 (nDstGeomFieldCount == 0 && m_poClipSrc)) )
            {
                poStolenGeometry = poFeature->StealGeometry();
            }
            else if( !bExplodeCollections &&
                     iRequestedSrcGeomField >= 0 )
            {
                poStolenGeometry = poFeature->StealGeometry(
                    iRequestedSrcGeomField);
            }

            if( nDstGeomFieldCount == 0 && poStolenGeometry && m_poClipSrc )
            {
                OGRGeometry* poClipped = poStolenGeometry->Intersection(m_poClipSrc);
                delete poStolenGeometry;
                poStolenGeometry = nullptr;
                if (poClipped == nullptr || poClipped->IsEmpty())
                {
                    delete poClipped;
                    goto end_loop;
                }
                delete poClipped;
            }

            if( poDstFeature->SetFrom( poFeature, panMap, TRUE ) != OGRERR_NONE )
            {
                if( psOptions->nGroupTransactions )
                {
                    if( psOptions->nLayerTransaction )
                    {
                        if( poDstLayer->CommitTransaction() != OGRERR_NONE )
                        {
                            OGRFeature::DestroyFeature( poFeature );
                            OGRFeature::DestroyFeature( poDstFeature );
                            OGRGeometryFactory::destroyGeometry( poStolenGeometry );
                            return false;
                        }
                    }
                }

                CPLError( CE_Failure, CPLE_AppDefined,
                        "Unable to translate feature " CPL_FRMT_GIB " from layer %s.",
                        poFeature->GetFID(), poSrcLayer->GetName() );

                OGRFeature::DestroyFeature( poFeature );
                OGRFeature::DestroyFeature( poDstFeature );
                OGRGeometryFactory::destroyGeometry( poStolenGeometry );
                return false;
            }

            if (psOptions->bEmptyStrAsNull) {
                for( int i=0; i < poDstFeature->GetFieldCount(); i++ )
                {
                    if (!poDstFeature->IsFieldSetAndNotNull(i))
                        continue;
                    auto fieldDef = poDstFeature->GetFieldDefnRef(i);
                    if (fieldDef->GetType() != OGRFieldType::OFTString)
                        continue;
                    auto str = poDstFeature->GetFieldAsString(i);
                    if (strcmp(str, "") == 0)
                        poDstFeature->SetFieldNull(i);
                }
            }

            /* ... and now we can attach the stolen geometry */
            if( poStolenGeometry )
            {
                poDstFeature->SetGeometryDirectly(poStolenGeometry);
            }

            if( bPreserveFID )
                poDstFeature->SetFID( poFeature->GetFID() );
            else if( psInfo->m_iSrcFIDField >= 0 &&
                     poFeature->IsFieldSetAndNotNull(psInfo->m_iSrcFIDField))
                poDstFeature->SetFID( poFeature->GetFieldAsInteger64(psInfo->m_iSrcFIDField) );

            /* Erase native data if asked explicitly */
            if( !m_bNativeData )
            {
                poDstFeature->SetNativeData(nullptr);
                poDstFeature->SetNativeMediaType(nullptr);
            }

            for( int iGeom = 0; iGeom < nDstGeomFieldCount; iGeom ++ )
            {
                OGRGeometry* poDstGeometry;

                if( poCollToExplode && iGeom == iGeomCollToExplode )
                {
                    OGRGeometry* poPart = poCollToExplode->getGeometryRef(0);
                    poCollToExplode->removeGeometry(0, FALSE);
                    poDstGeometry = poPart;
                    assert(poDstGeometry);
                }
                else
                {
                    poDstGeometry = poDstFeature->StealGeometry(iGeom);
                    if (poDstGeometry == nullptr)
                        continue;
                }

                if (iSrcZField != -1)
                {
                    SetZ(poDstGeometry, poFeature->GetFieldAsDouble(iSrcZField));
                    /* This will correct the coordinate dimension to 3 */
                    OGRGeometry* poDupGeometry = poDstGeometry->clone();
                    delete poDstGeometry;
                    poDstGeometry = poDupGeometry;
                }

                if (m_nCoordDim == 2 || m_nCoordDim == 3)
                {
                    poDstGeometry->setCoordinateDimension( m_nCoordDim );
                }
                else if (m_nCoordDim == 4)
                {
                    poDstGeometry->set3D( TRUE );
                    poDstGeometry->setMeasured( TRUE );
                }
                else if (m_nCoordDim == COORD_DIM_XYM)
                {
                    poDstGeometry->set3D( FALSE );
                    poDstGeometry->setMeasured( TRUE );
                }
                else if ( m_nCoordDim == COORD_DIM_LAYER_DIM )
                {
                    const OGRwkbGeometryType eDstLayerGeomType =
                      poDstLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom)->GetType();
                    poDstGeometry->set3D( wkbHasZ(eDstLayerGeomType) );
                    poDstGeometry->setMeasured( wkbHasM(eDstLayerGeomType) );
                }

                if (m_eGeomOp == GEOMOP_SEGMENTIZE)
                {
                    if (m_dfGeomOpParam > 0)
                        poDstGeometry->segmentize(m_dfGeomOpParam);
                }
                else if (m_eGeomOp == GEOMOP_SIMPLIFY_PRESERVE_TOPOLOGY)
                {
                    if (m_dfGeomOpParam > 0)
                    {
                        OGRGeometry* poNewGeom = poDstGeometry->SimplifyPreserveTopology(m_dfGeomOpParam);
                        if (poNewGeom)
                        {
                            delete poDstGeometry;
                            poDstGeometry = poNewGeom;
                        }
                    }
                }

                if (m_poClipSrc)
                {
                    OGRGeometry* poClipped = poDstGeometry->Intersection(m_poClipSrc);
                    delete poDstGeometry;
                    if (poClipped == nullptr || poClipped->IsEmpty())
                    {
                        delete poClipped;
                        goto end_loop;
                    }
                    poDstGeometry = poClipped;
                }

                OGRCoordinateTransformation* const poCT = psInfo->m_apoCT[iGeom].get();
                char** const papszTransformOptions = psInfo->m_aosTransformOptions[iGeom].List();

                if( poCT != nullptr || papszTransformOptions != nullptr)
                {
                    OGRGeometry* poReprojectedGeom =
                        OGRGeometryFactory::transformWithOptions(
                            poDstGeometry, poCT, papszTransformOptions, m_transformWithOptionsCache);
                    if( poReprojectedGeom == nullptr )
                    {
                        if( psOptions->nGroupTransactions )
                        {
                            if( psOptions->nLayerTransaction )
                            {
                                if( poDstLayer->CommitTransaction() != OGRERR_NONE &&
                                    !psOptions->bSkipFailures )
                                {
                                    OGRFeature::DestroyFeature( poFeature );
                                    OGRFeature::DestroyFeature( poDstFeature );
                                    delete poDstGeometry;
                                    return false;
                                }
                            }
                        }

                        CPLError( CE_Failure, CPLE_AppDefined, "Failed to reproject feature " CPL_FRMT_GIB " (geometry probably out of source or destination SRS).",
                                  poFeature->GetFID() );
                        if( !psOptions->bSkipFailures )
                        {
                            OGRFeature::DestroyFeature( poFeature );
                            OGRFeature::DestroyFeature( poDstFeature );
                            delete poDstGeometry;
                            return false;
                        }
                    }

                    delete poDstGeometry;
                    poDstGeometry = poReprojectedGeom;
                }
                else if (poOutputSRS != nullptr)
                {
                    poDstGeometry->assignSpatialReference(poOutputSRS);
                }

                if( poDstGeometry != nullptr )
                {
                    if (m_poClipDst)
                    {
                        OGRGeometry* poClipped = poDstGeometry->Intersection(m_poClipDst);
                        delete poDstGeometry;
                        if (poClipped == nullptr || poClipped->IsEmpty())
                        {
                            delete poClipped;
                            goto end_loop;
                        }

                        poDstGeometry = poClipped;
                    }

                    if( m_bMakeValid )
                    {
                        OGRGeometry* poValidGeom = poDstGeometry->MakeValid();
                        delete poDstGeometry;
                        poDstGeometry = poValidGeom;
                        if( poDstGeometry == nullptr )
                            goto end_loop;
                        OGRGeometry* poCleanedGeom =
                            OGRGeometryFactory::removeLowerDimensionSubGeoms(poDstGeometry);
                        delete poDstGeometry;
                        poDstGeometry = poCleanedGeom;
                    }

                    if( eGType != GEOMTYPE_UNCHANGED )
                    {
                        poDstGeometry = OGRGeometryFactory::forceTo(
                                poDstGeometry, static_cast<OGRwkbGeometryType>(eGType));
                    }
                    else if( m_eGeomTypeConversion == GTC_PROMOTE_TO_MULTI ||
                            m_eGeomTypeConversion == GTC_CONVERT_TO_LINEAR ||
                            m_eGeomTypeConversion == GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR ||
                            m_eGeomTypeConversion == GTC_CONVERT_TO_CURVE )
                    {
                        OGRwkbGeometryType eTargetType = poDstGeometry->getGeometryType();
                        eTargetType = ConvertType(m_eGeomTypeConversion, eTargetType);
                        poDstGeometry = OGRGeometryFactory::forceTo(poDstGeometry, eTargetType);
                    }
                }

                poDstFeature->SetGeomFieldDirectly(iGeom, poDstGeometry);
            }

            if( !psInfo->m_oMapResolved.empty() )
            {
                for( const auto& kv: psInfo->m_oMapResolved )
                {
                    const int nDstField = kv.first;
                    const int nSrcField = kv.second.nSrcField;
                    if( poFeature->IsFieldSetAndNotNull(nSrcField) )
                    {
                        const auto poDomain = kv.second.poDomain;
                        const auto& oMapKV = psInfo->m_oMapDomainToKV[poDomain];
                        const auto iter = oMapKV.find(
                            poFeature->GetFieldAsString(nSrcField));
                        if( iter != oMapKV.end() )
                        {
                            poDstFeature->SetField(nDstField, iter->second.c_str());
                        }
                    }
                }
            }

            CPLErrorReset();
            if( poDstLayer->CreateFeature( poDstFeature ) == OGRERR_NONE )
            {
                nFeaturesWritten ++;
                if( (bPreserveFID && poDstFeature->GetFID() != poFeature->GetFID()) ||
                    (!bPreserveFID && psInfo->m_iSrcFIDField >= 0 && poFeature->IsFieldSetAndNotNull(psInfo->m_iSrcFIDField) &&
                     poDstFeature->GetFID() != poFeature->GetFieldAsInteger64(psInfo->m_iSrcFIDField)) )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Feature id not preserved");
                }
            }
            else if( !psOptions->bSkipFailures )
            {
                if( psOptions->nGroupTransactions )
                {
                    if( psOptions->nLayerTransaction )
                        poDstLayer->RollbackTransaction();
                }

                CPLError( CE_Failure, CPLE_AppDefined,
                        "Unable to write feature " CPL_FRMT_GIB " from layer %s.",
                        poFeature->GetFID(), poSrcLayer->GetName() );

                OGRFeature::DestroyFeature( poFeature );
                OGRFeature::DestroyFeature( poDstFeature );
                return false;
            }
            else
            {
                CPLDebug( "GDALVectorTranslate", "Unable to write feature " CPL_FRMT_GIB " into layer %s.",
                           poFeature->GetFID(), poSrcLayer->GetName() );
                if( psOptions->nGroupTransactions )
                {
                    if( psOptions->nLayerTransaction )
                    {
                        poDstLayer->RollbackTransaction();
                        CPL_IGNORE_RET_VAL(poDstLayer->StartTransaction());
                    }
                    else
                    {
                        m_poODS->RollbackTransaction();
                        m_poODS->StartTransaction(psOptions->bForceTransaction);
                    }
                }
            }

end_loop:
            OGRFeature::DestroyFeature( poDstFeature );
        }

        OGRFeature::DestroyFeature( poFeature );

        /* Report progress */
        nCount ++;
        bool bGoOn = true;
        if (pfnProgress)
        {
            bGoOn = pfnProgress(nCountLayerFeatures ? nCount * 1.0 / nCountLayerFeatures: 1.0, "", pProgressArg) != FALSE;
        }
        if( !bGoOn )
        {
            bRet = false;
            break;
        }

        if (pnReadFeatureCount)
            *pnReadFeatureCount = nCount;

        if( psOptions->nFIDToFetch != OGRNullFID )
            break;
        if( poFeatureIn != nullptr )
            break;
    }

    if( psOptions->nGroupTransactions )
    {
        if( psOptions->nLayerTransaction )
        {
            if( poDstLayer->CommitTransaction() != OGRERR_NONE )
                bRet = false;
        }
    }

    if( poFeatureIn == nullptr )
    {
        CPLDebug("GDALVectorTranslate", CPL_FRMT_GIB " features written in layer '%s'",
                nFeaturesWritten, poDstLayer->GetName());
    }

    return bRet;
}

/************************************************************************/
/*                             RemoveBOM()                              */
/************************************************************************/

/* Remove potential UTF-8 BOM from data (must be NUL terminated) */
static void RemoveBOM(GByte* pabyData)
{
    if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
    {
        memmove(pabyData, pabyData + 3, strlen(reinterpret_cast<char*>(pabyData) + 3) + 1);
    }
}

static void RemoveSQLComments(char*& pszSQL)
{
    char** papszLines = CSLTokenizeStringComplex(pszSQL, "\r\n", FALSE, FALSE);
    CPLString osSQL;
    for( char** papszIter = papszLines; papszIter && *papszIter; ++papszIter )
    {
        const char* pszLine = *papszIter;
        char chQuote = 0;
        int i = 0;
        for(; pszLine[i] != '\0'; ++i )
        {
            if( chQuote )
            {
                if( pszLine[i] == chQuote )
                {
                    if( pszLine[i+1] == chQuote )
                    {
                        i++;
                    }
                    else
                    {
                        chQuote = 0;
                    }
                }
            }
            else if( pszLine[i] == '\'' || pszLine[i] == '"' )
            {
                chQuote = pszLine[i];
            }
            else if( pszLine[i] == '-' && pszLine[i+1] == '-' )
            {
                break;
            }
        }
        if( i > 0 )
        {
            osSQL.append(pszLine, i);
        }
        osSQL += ' ';
    }
    CSLDestroy(papszLines);
    CPLFree(pszSQL);
    pszSQL = CPLStrdup(osSQL);
}

/************************************************************************/
/*                       GDALVectorTranslateOptionsNew()                */
/************************************************************************/

/**
 * allocates a GDALVectorTranslateOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="/programs/ogr2ogr.html">ogr2ogr</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALVectorTranslateOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALVectorTranslateOptions struct. Must be freed with GDALVectorTranslateOptionsFree().
 *
 * @since GDAL 2.1
 */
GDALVectorTranslateOptions *GDALVectorTranslateOptionsNew(char** papszArgv,
                                                      GDALVectorTranslateOptionsForBinary* psOptionsForBinary)
{
    GDALVectorTranslateOptions *psOptions =
        static_cast<GDALVectorTranslateOptions *>(
            CPLCalloc( 1, sizeof(GDALVectorTranslateOptions)));

    psOptions->eAccessMode = ACCESS_CREATION;
    psOptions->bSkipFailures = false;
    psOptions->nLayerTransaction = -1;
    psOptions->bForceTransaction = false;
    psOptions->nGroupTransactions = 100 * 1000;
    psOptions->nFIDToFetch = OGRNullFID;
    psOptions->bQuiet = false;
    psOptions->pszFormat = nullptr;
    psOptions->papszLayers = nullptr;
    psOptions->papszDSCO = nullptr;
    psOptions->papszLCO = nullptr;
    psOptions->bTransform = false;
    psOptions->bAddMissingFields = false;
    psOptions->pszOutputSRSDef = nullptr;
    psOptions->pszSourceSRSDef = nullptr;
    psOptions->pszCTPipeline = nullptr;
    psOptions->bNullifyOutputSRS = false;
    psOptions->bExactFieldNameMatch = true;
    psOptions->pszNewLayerName = nullptr;
    psOptions->pszWHERE = nullptr;
    psOptions->pszGeomField = nullptr;
    psOptions->papszSelFields = nullptr;
    psOptions->pszSQLStatement = nullptr;
    psOptions->pszDialect = nullptr;
    psOptions->eGType = GEOMTYPE_UNCHANGED;
    psOptions->eGeomTypeConversion = GTC_DEFAULT;
    psOptions->eGeomOp = GEOMOP_NONE;
    psOptions->dfGeomOpParam = 0;
    psOptions->bMakeValid = false;
    psOptions->papszFieldTypesToString = nullptr;
    psOptions->papszMapFieldType = nullptr;
    psOptions->bUnsetFieldWidth = false;
    psOptions->bDisplayProgress = false;
    psOptions->bWrapDateline = false;
    psOptions->dfDateLineOffset = 10.0;
    psOptions->bClipSrc = false;
    psOptions->hClipSrc = nullptr;
    psOptions->pszClipSrcDS = nullptr;
    psOptions->pszClipSrcSQL = nullptr;
    psOptions->pszClipSrcLayer = nullptr;
    psOptions->pszClipSrcWhere = nullptr;
    psOptions->hClipDst = nullptr;
    psOptions->pszClipDstDS = nullptr;
    psOptions->pszClipDstSQL = nullptr;
    psOptions->pszClipDstLayer = nullptr;
    psOptions->pszClipDstWhere = nullptr;
    psOptions->bSplitListFields = false;
    psOptions->nMaxSplitListSubFields = -1;
    psOptions->bExplodeCollections = false;
    psOptions->pszZField = nullptr;
    psOptions->papszFieldMap = nullptr;
    psOptions->nCoordDim = COORD_DIM_UNCHANGED;
    psOptions->papszDestOpenOptions = nullptr;
    psOptions->bForceNullable = false;
    psOptions->bResolveDomains = false;
    psOptions->bUnsetDefault = false;
    psOptions->bUnsetFid = false;
    psOptions->bPreserveFID = false;
    psOptions->bCopyMD = true;
    psOptions->papszMetadataOptions = nullptr;
    psOptions->pszSpatSRSDef = nullptr;
    psOptions->nGCPCount = 0;
    psOptions->pasGCPs = nullptr;
    psOptions->nTransformOrder = 0;  /* Default to 0 for now... let the lib decide */
    psOptions->hSpatialFilter = nullptr;
    psOptions->bNativeData = true;
    psOptions->nLimit = -1;

    int nArgc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != nullptr && i < nArgc; i++ )
    {
        if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = TRUE;
        }
        else if( i+1 < nArgc && (EQUAL(papszArgv[i],"-f") || EQUAL(papszArgv[i],"-of")) )
        {
            CPLFree(psOptions->pszFormat);
            const char* pszFormatArg = papszArgv[++i];
            psOptions->pszFormat = CPLStrdup(pszFormatArg);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-dsco") )
        {
            psOptions->papszDSCO = CSLAddString(psOptions->papszDSCO, papszArgv[++i] );
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-lco") )
        {
            psOptions->papszLCO = CSLAddString(psOptions->papszLCO, papszArgv[++i] );
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-oo") )
        {
            ++i;
            if( psOptionsForBinary )
            {
                psOptionsForBinary->papszOpenOptions = CSLAddString(psOptionsForBinary->papszOpenOptions, papszArgv[i] );
            }
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-doo") )
        {
            ++i;
            psOptions->papszDestOpenOptions = CSLAddString(psOptions->papszDestOpenOptions, papszArgv[i] );
        }
        else if( EQUAL(papszArgv[i],"-preserve_fid") )
        {
            psOptions->bPreserveFID = true;
        }
        else if( STARTS_WITH_CI(papszArgv[i], "-skip") )
        {
            psOptions->bSkipFailures = true;
            psOptions->nGroupTransactions = 1; /* #2409 */
        }
        else if( EQUAL(papszArgv[i],"-append") )
        {
            psOptions->eAccessMode = ACCESS_APPEND;
        }
        else if( EQUAL(papszArgv[i],"-overwrite") )
        {
            psOptions->eAccessMode = ACCESS_OVERWRITE;
        }
        else if( EQUAL(papszArgv[i],"-addfields") )
        {
            psOptions->bAddMissingFields = true;
            psOptions->eAccessMode = ACCESS_APPEND;
        }
        else if( EQUAL(papszArgv[i],"-update") )
        {
            /* Don't reset -append or -overwrite */
            if( psOptions->eAccessMode != ACCESS_APPEND && psOptions->eAccessMode != ACCESS_OVERWRITE )
                psOptions->eAccessMode = ACCESS_UPDATE;
        }
        else if( EQUAL(papszArgv[i],"-relaxedFieldNameMatch") )
        {
            psOptions->bExactFieldNameMatch = false;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-fid") )
        {
            psOptions->nFIDToFetch = CPLAtoGIntBig(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-sql") )
        {
            i++;
            CPLFree(psOptions->pszSQLStatement);
            GByte* pabyRet = nullptr;
            if( papszArgv[i][0] == '@' &&
                VSIIngestFile( nullptr, papszArgv[i] + 1, &pabyRet, nullptr, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                psOptions->pszSQLStatement = reinterpret_cast<char*>(pabyRet);
                RemoveSQLComments(psOptions->pszSQLStatement);
            }
            else
            {
                psOptions->pszSQLStatement = CPLStrdup(papszArgv[i]);
            }
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-dialect") )
        {
            CPLFree(psOptions->pszDialect);
            psOptions->pszDialect = CPLStrdup(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-nln") )
        {
            CPLFree(psOptions->pszNewLayerName);
            psOptions->pszNewLayerName = CPLStrdup(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-nlt") )
        {
            bool bIs3D = false;
            CPLString osGeomName = papszArgv[i+1];
            if (strlen(papszArgv[i+1]) > 3 &&
                STARTS_WITH_CI(papszArgv[i+1] + strlen(papszArgv[i+1]) - 3, "25D"))
            {
                bIs3D = true;
                osGeomName.resize(osGeomName.size() - 3);
            }
            else if (strlen(papszArgv[i+1]) > 1 &&
                STARTS_WITH_CI(papszArgv[i+1] + strlen(papszArgv[i+1]) - 1, "Z"))
            {
                bIs3D = true;
                osGeomName.resize(osGeomName.size() - 1);
            }
            if( EQUAL(osGeomName,"NONE") )
                psOptions->eGType = wkbNone;
            else if( EQUAL(osGeomName,"GEOMETRY") )
                psOptions->eGType = wkbUnknown;
            else if( EQUAL(osGeomName,"PROMOTE_TO_MULTI") )
            {
                if( psOptions->eGeomTypeConversion == GTC_CONVERT_TO_LINEAR )
                    psOptions->eGeomTypeConversion = GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR;
                else
                    psOptions->eGeomTypeConversion = GTC_PROMOTE_TO_MULTI;
            }
            else if( EQUAL(osGeomName,"CONVERT_TO_LINEAR") )
            {
                if( psOptions->eGeomTypeConversion == GTC_PROMOTE_TO_MULTI )
                    psOptions->eGeomTypeConversion = GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR;
                else
                    psOptions->eGeomTypeConversion = GTC_CONVERT_TO_LINEAR;
            }
            else if( EQUAL(osGeomName,"CONVERT_TO_CURVE") )
                psOptions->eGeomTypeConversion = GTC_CONVERT_TO_CURVE;
            else
            {
                psOptions->eGType = OGRFromOGCGeomType(osGeomName);
                if (psOptions->eGType == wkbUnknown)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "-nlt %s: type not recognised.",
                            papszArgv[i+1] );
                    GDALVectorTranslateOptionsFree(psOptions);
                    return nullptr;
                }
            }
            if (psOptions->eGType != GEOMTYPE_UNCHANGED && psOptions->eGType != wkbNone && bIs3D)
                psOptions->eGType = wkbSetZ(static_cast<OGRwkbGeometryType>(psOptions->eGType));

            i++;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-dim") )
        {
            if( EQUAL(papszArgv[i+1], "layer_dim") )
                psOptions->nCoordDim = COORD_DIM_LAYER_DIM;
            else if( EQUAL(papszArgv[i+1], "XY") || EQUAL(papszArgv[i+1], "2") )
                psOptions->nCoordDim = 2;
            else if( EQUAL(papszArgv[i+1], "XYZ") || EQUAL(papszArgv[i+1], "3") )
                psOptions->nCoordDim = 3;
            else if( EQUAL(papszArgv[i+1], "XYM") )
                psOptions->nCoordDim = COORD_DIM_XYM;
            else if( EQUAL(papszArgv[i+1], "XYZM") )
                psOptions->nCoordDim = 4;
            else
            {
                CPLError(CE_Failure, CPLE_IllegalArg,"-dim %s: value not handled.",
                         papszArgv[i+1] );
                GDALVectorTranslateOptionsFree(psOptions);
                return nullptr;
            }
            i++;
        }
        else if( i+1 < nArgc && (EQUAL(papszArgv[i],"-tg") ||
                                 EQUAL(papszArgv[i],"-gt")) )
        {
            ++i;
            /* If skipfailures is already set we should not
               modify nGroupTransactions = 1  #2409 */
            if ( !psOptions->bSkipFailures )
            {
                if( EQUAL(papszArgv[i], "unlimited") )
                    psOptions->nGroupTransactions = -1;
                else
                    psOptions->nGroupTransactions = atoi(papszArgv[i]);
            }
        }
        else if ( EQUAL(papszArgv[i],"-ds_transaction") )
        {
            psOptions->nLayerTransaction = FALSE;
            psOptions->bForceTransaction = true;
        }
        /* Undocumented. Just a provision. Default behavior should be OK */
        else if ( EQUAL(papszArgv[i],"-lyr_transaction") )
        {
            psOptions->nLayerTransaction = TRUE;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-s_srs") )
        {
            CPLFree(psOptions->pszSourceSRSDef);
            psOptions->pszSourceSRSDef = CPLStrdup(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-s_coord_epoch") )
        {
            psOptions->dfSourceCoordinateEpoch = CPLAtof(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-a_srs") )
        {
            CPLFree(psOptions->pszOutputSRSDef);
            psOptions->pszOutputSRSDef = CPLStrdup(papszArgv[++i]);
            if (EQUAL(psOptions->pszOutputSRSDef, "NULL") ||
                EQUAL(psOptions->pszOutputSRSDef, "NONE"))
            {
                psOptions->pszOutputSRSDef = nullptr;
                psOptions->bNullifyOutputSRS = true;
            }
        }
        else if( i+1 < nArgc && (EQUAL(papszArgv[i],"-a_coord_epoch") ||
                                 EQUAL(papszArgv[i],"-t_coord_epoch")) )
        {
            psOptions->dfOutputCoordinateEpoch = CPLAtof(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-t_srs") )
        {
            CPLFree(psOptions->pszOutputSRSDef);
            psOptions->pszOutputSRSDef = CPLStrdup(papszArgv[++i]);
            psOptions->bTransform = true;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-ct") )
        {
            CPLFree(psOptions->pszCTPipeline);
            psOptions->pszCTPipeline = CPLStrdup(papszArgv[++i]);
            psOptions->bTransform = true;
        }
        else if( i+4 < nArgc && EQUAL(papszArgv[i],"-spat") )
        {
            OGRLinearRing  oRing;

            oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
            oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
            oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
            oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
            oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

            OGRPolygon* poSpatialFilter = new OGRPolygon();
            poSpatialFilter->addRing( &oRing );
            OGR_G_DestroyGeometry(psOptions->hSpatialFilter);
            psOptions->hSpatialFilter = reinterpret_cast<OGRGeometryH>(poSpatialFilter);
            i += 4;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-spat_srs") )
        {
            CPLFree(psOptions->pszSpatSRSDef);
            psOptions->pszSpatSRSDef = CPLStrdup(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-geomfield") )
        {
            CPLFree(psOptions->pszGeomField);
            psOptions->pszGeomField = CPLStrdup(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-where") )
        {
            i++;
            CPLFree(psOptions->pszWHERE);
            GByte* pabyRet = nullptr;
            if( papszArgv[i][0] == '@' &&
                VSIIngestFile( nullptr, papszArgv[i] + 1, &pabyRet, nullptr, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                psOptions->pszWHERE = reinterpret_cast<char*>(pabyRet);
            }
            else
            {
                psOptions->pszWHERE = CPLStrdup(papszArgv[i]);
            }
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-select") )
        {
            const char* pszSelect = papszArgv[++i];
            CSLDestroy(psOptions->papszSelFields);
            psOptions->papszSelFields = CSLTokenizeStringComplex(pszSelect, " ,",
                                                      FALSE, FALSE );
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-segmentize") )
        {
            psOptions->eGeomOp = GEOMOP_SEGMENTIZE;
            psOptions->dfGeomOpParam = CPLAtof(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-simplify") )
        {
            psOptions->eGeomOp = GEOMOP_SIMPLIFY_PRESERVE_TOPOLOGY;
            psOptions->dfGeomOpParam = CPLAtof(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-makevalid") )
        {
            // Check that OGRGeometry::MakeValid() is available
            OGRGeometry* poInputGeom = nullptr;
            OGRGeometryFactory::createFromWkt(
                "POLYGON((0 0,1 1,1 0,0 1,0 0))", nullptr, &poInputGeom );
            CPLAssert(poInputGeom);
            OGRGeometry* poValidGeom = poInputGeom->MakeValid();
            delete poInputGeom;
            if( poValidGeom == nullptr )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "-makevalid only supported for builds against GEOS 3.8 or later");
                GDALVectorTranslateOptionsFree(psOptions);
                return nullptr;
            }
            delete poValidGeom;
            psOptions->bMakeValid = true;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-fieldTypeToString") )
        {
            CSLDestroy(psOptions->papszFieldTypesToString);
            psOptions->papszFieldTypesToString =
                    CSLTokenizeStringComplex(papszArgv[++i], " ,",
                                             FALSE, FALSE );
            char** iter = psOptions->papszFieldTypesToString;
            while(*iter)
            {
                if (IsFieldType(*iter))
                {
                    /* Do nothing */
                }
                else if (EQUAL(*iter, "All"))
                {
                    CSLDestroy(psOptions->papszFieldTypesToString);
                    psOptions->papszFieldTypesToString = nullptr;
                    psOptions->papszFieldTypesToString = CSLAddString(psOptions->papszFieldTypesToString, "All");
                    break;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Unhandled type for fieldTypeToString option : %s",
                            *iter);
                    GDALVectorTranslateOptionsFree(psOptions);
                    return nullptr;
                }
                iter ++;
            }
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-mapFieldType") )
        {
            CSLDestroy(psOptions->papszMapFieldType);
            psOptions->papszMapFieldType =
                    CSLTokenizeStringComplex(papszArgv[++i], " ,",
                                             FALSE, FALSE );
            char** iter = psOptions->papszMapFieldType;
            while(*iter)
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(*iter, &pszKey);
                if( pszKey && pszValue)
                {
                    if( !((IsFieldType(pszKey) || EQUAL(pszKey, "All")) && IsFieldType(pszValue)) )
                    {
                        CPLError(CE_Failure, CPLE_IllegalArg,
                                "Invalid value for -mapFieldType : %s",
                                *iter);
                        CPLFree(pszKey);
                        GDALVectorTranslateOptionsFree(psOptions);
                        return nullptr;
                    }
                }
                CPLFree(pszKey);
                iter ++;
            }
        }
        else if( EQUAL(papszArgv[i],"-unsetFieldWidth") )
        {
            psOptions->bUnsetFieldWidth = true;
        }
        else if( EQUAL(papszArgv[i],"-progress") )
        {
            psOptions->bDisplayProgress = true;
        }
        else if( EQUAL(papszArgv[i],"-wrapdateline") )
        {
            psOptions->bWrapDateline = true;
        }
        else if( i < nArgc-1 && EQUAL(papszArgv[i],"-datelineoffset") )
        {
            psOptions->dfDateLineOffset = CPLAtof(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-clipsrc") )
        {
            if (i + 1 >= nArgc)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "%s option requires 1 or 4 arguments", papszArgv[i]);
                GDALVectorTranslateOptionsFree(psOptions);
                return nullptr;
            }

            OGR_G_DestroyGeometry(psOptions->hClipSrc);
            psOptions->hClipSrc = nullptr;
            CPLFree(psOptions->pszClipSrcDS);
            psOptions->pszClipSrcDS = nullptr;

            VSIStatBufL  sStat;
            psOptions->bClipSrc = true;
            if ( IsNumber(papszArgv[i+1])
                 && papszArgv[i+2] != nullptr
                 && papszArgv[i+3] != nullptr
                 && papszArgv[i+4] != nullptr)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

                OGRPolygon* poPoly = new OGRPolygon();
                psOptions->hClipSrc = reinterpret_cast<OGRGeometryH>(poPoly);
                poPoly->addRing( &oRing );
                i += 4;
            }
            else if ((STARTS_WITH_CI(papszArgv[i+1], "POLYGON") ||
                      STARTS_WITH_CI(papszArgv[i+1], "MULTIPOLYGON")) &&
                      VSIStatL(papszArgv[i+1], &sStat) != 0)
            {
                OGRGeometryFactory::createFromWkt(papszArgv[i+1], nullptr,
                        reinterpret_cast<OGRGeometry **>(&psOptions->hClipSrc));
                if (psOptions->hClipSrc == nullptr)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                    GDALVectorTranslateOptionsFree(psOptions);
                    return nullptr;
                }
                i ++;
            }
            else if (EQUAL(papszArgv[i+1], "spat_extent") )
            {
                i ++;
            }
            else
            {
                psOptions->pszClipSrcDS = CPLStrdup(papszArgv[i+1]);
                i ++;
            }
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-clipsrcsql") )
        {
            CPLFree(psOptions->pszClipSrcSQL);
            psOptions->pszClipSrcSQL = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-clipsrclayer") )
        {
            CPLFree(psOptions->pszClipSrcLayer);
            psOptions->pszClipSrcLayer = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-clipsrcwhere") )
        {
            CPLFree(psOptions->pszClipSrcWhere);
            psOptions->pszClipSrcWhere = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( EQUAL(papszArgv[i],"-clipdst") )
        {
            if (i + 1 >= nArgc)
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "%s option requires 1 or 4 arguments", papszArgv[i]);
                GDALVectorTranslateOptionsFree(psOptions);
                return nullptr;
            }

            OGR_G_DestroyGeometry(psOptions->hClipDst);
            psOptions->hClipDst = nullptr;
            CPLFree(psOptions->pszClipDstDS);
            psOptions->pszClipDstDS = nullptr;

            VSIStatBufL  sStat;
            if ( IsNumber(papszArgv[i+1])
                 && papszArgv[i+2] != nullptr
                 && papszArgv[i+3] != nullptr
                 && papszArgv[i+4] != nullptr)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

                OGRPolygon* poPoly = new OGRPolygon();
                psOptions->hClipDst = reinterpret_cast<OGRGeometryH>(poPoly);
                poPoly->addRing( &oRing );
                i += 4;
            }
            else if ((STARTS_WITH_CI(papszArgv[i+1], "POLYGON") ||
                      STARTS_WITH_CI(papszArgv[i+1], "MULTIPOLYGON")) &&
                      VSIStatL(papszArgv[i+1], &sStat) != 0)
            {
                OGRGeometryFactory::createFromWkt(papszArgv[i+1],
                    nullptr, reinterpret_cast<OGRGeometry **>(&psOptions->hClipDst));
                if (psOptions->hClipDst == nullptr)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                    GDALVectorTranslateOptionsFree(psOptions);
                    return nullptr;
                }
                i ++;
            }
            else
            {
                psOptions->pszClipDstDS = CPLStrdup(papszArgv[i+1]);
                i ++;
            }
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-clipdstsql") )
        {
            CPLFree(psOptions->pszClipDstSQL);
            psOptions->pszClipDstSQL = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-clipdstlayer") )
        {
            CPLFree(psOptions->pszClipDstLayer);
            psOptions->pszClipDstLayer = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-clipdstwhere") )
        {
            CPLFree(psOptions->pszClipDstWhere);
            psOptions->pszClipDstWhere = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( EQUAL(papszArgv[i],"-splitlistfields") )
        {
            psOptions->bSplitListFields = true;
        }
        else if ( i+1 < nArgc && EQUAL(papszArgv[i],"-maxsubfields") )
        {
            if (IsNumber(papszArgv[i+1]))
            {
                int nTemp = atoi(papszArgv[i+1]);
                if (nTemp > 0)
                {
                    psOptions->nMaxSplitListSubFields = nTemp;
                    i ++;
                }
            }
        }
        else if( EQUAL(papszArgv[i],"-explodecollections") )
        {
            psOptions->bExplodeCollections = true;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-zfield") )
        {
            CPLFree(psOptions->pszZField);
            psOptions->pszZField = CPLStrdup(papszArgv[i+1]);
            i ++;
        }
        else if( i+4 < nArgc && EQUAL(papszArgv[i],"-gcp") )
        {
            char* endptr = nullptr;
            /* -gcp pixel line easting northing [elev] */

            psOptions->nGCPCount++;
            psOptions->pasGCPs = static_cast<GDAL_GCP *>(
                CPLRealloc(psOptions->pasGCPs,
                           sizeof(GDAL_GCP) * psOptions->nGCPCount));
            GDALInitGCPs( 1, psOptions->pasGCPs + psOptions->nGCPCount - 1 );

            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPPixel = CPLAtof(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPLine = CPLAtof(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPX = CPLAtof(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPY = CPLAtof(papszArgv[++i]);
            if( papszArgv[i+1] != nullptr
                && (CPLStrtod(papszArgv[i+1], &endptr) != 0.0 || papszArgv[i+1][0] == '0') )
            {
                /* Check that last argument is really a number and not a filename */
                /* looking like a number (see ticket #863) */
                if (endptr && *endptr == 0)
                    psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPZ = CPLAtof(papszArgv[++i]);
            }

            /* should set id and info? */
        }
        else if( EQUAL(papszArgv[i],"-tps") )
        {
            psOptions->nTransformOrder = -1;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-order") )
        {
            psOptions->nTransformOrder = atoi( papszArgv[++i] );
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-fieldmap") )
        {
            CSLDestroy(psOptions->papszFieldMap);
            psOptions->papszFieldMap = CSLTokenizeStringComplex(papszArgv[++i], ",",
                                                      FALSE, FALSE );
        }
        else if( EQUAL(papszArgv[i],"-emptyStrAsNull") )
        {
            psOptions->bEmptyStrAsNull = true;
        }
        else if( EQUAL(papszArgv[i],"-forceNullable") )
        {
            psOptions->bForceNullable = true;
        }
        else if( EQUAL(papszArgv[i],"-resolveDomains") )
        {
            psOptions->bResolveDomains = true;
        }
        else if( EQUAL(papszArgv[i],"-unsetDefault") )
        {
            psOptions->bUnsetDefault = true;
        }
        else if( EQUAL(papszArgv[i],"-unsetFid") )
        {
            psOptions->bUnsetFid = true;
        }
        else if( EQUAL(papszArgv[i],"-nomd") )
        {
            psOptions->bCopyMD = false;
        }
        else if( EQUAL(papszArgv[i],"-noNativeData") )
        {
            psOptions->bNativeData = false;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-mo") )
        {
            psOptions->papszMetadataOptions = CSLAddString( psOptions->papszMetadataOptions,
                                                 papszArgv[++i] );
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-limit") )
        {
            psOptions->nLimit = CPLAtoGIntBig( papszArgv[++i] );
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALVectorTranslateOptionsFree(psOptions);
            return nullptr;
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszDestDataSource == nullptr )
            psOptionsForBinary->pszDestDataSource = CPLStrdup(papszArgv[i]);
        else if(  psOptionsForBinary && psOptionsForBinary->pszDataSource == nullptr )
            psOptionsForBinary->pszDataSource = CPLStrdup(papszArgv[i]);
        else
            psOptions->papszLayers = CSLAddString( psOptions->papszLayers, papszArgv[i] );
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->eAccessMode = psOptions->eAccessMode;
        if( psOptions->pszFormat )
            psOptionsForBinary->pszFormat = CPLStrdup( psOptions->pszFormat );

        if( !(CPLTestBool(CSLFetchNameValueDef(
                psOptionsForBinary->papszOpenOptions, "NATIVE_DATA",
                CSLFetchNameValueDef(
                    psOptionsForBinary->papszOpenOptions, "@NATIVE_DATA", "TRUE")))) )
        {
            psOptions->bNativeData = false;
        }

        if( psOptions->bNativeData &&
            CSLFetchNameValue(psOptionsForBinary->papszOpenOptions,
                              "NATIVE_DATA") == nullptr &&
            CSLFetchNameValue(psOptionsForBinary->papszOpenOptions,
                              "@NATIVE_DATA") == nullptr )
        {
            psOptionsForBinary->papszOpenOptions = CSLAddString(
                psOptionsForBinary->papszOpenOptions, "@NATIVE_DATA=YES" );
        }
    }

    return psOptions;
}

/************************************************************************/
/*                      GDALVectorTranslateOptionsFree()                */
/************************************************************************/

/**
 * Frees the GDALVectorTranslateOptions struct.
 *
 * @param psOptions the options struct for GDALVectorTranslate().
 * @since GDAL 2.1
 */

void GDALVectorTranslateOptionsFree( GDALVectorTranslateOptions *psOptions )
{
    if( psOptions == nullptr )
        return;

    CPLFree( psOptions->pszFormat );
    CPLFree( psOptions->pszOutputSRSDef);
    CPLFree( psOptions->pszSourceSRSDef);
    CPLFree( psOptions->pszCTPipeline );
    CPLFree( psOptions->pszNewLayerName);
    CPLFree( psOptions->pszWHERE );
    CPLFree( psOptions->pszGeomField );
    CPLFree( psOptions->pszSQLStatement );
    CPLFree( psOptions->pszDialect );
    CPLFree( psOptions->pszClipSrcDS );
    CPLFree( psOptions->pszClipSrcSQL );
    CPLFree( psOptions->pszClipSrcLayer );
    CPLFree( psOptions->pszClipSrcWhere );
    CPLFree( psOptions->pszClipDstDS );
    CPLFree( psOptions->pszClipDstSQL );
    CPLFree( psOptions->pszClipDstLayer );
    CPLFree( psOptions->pszClipDstWhere );
    CPLFree( psOptions->pszZField );
    CPLFree( psOptions->pszSpatSRSDef );
    CSLDestroy(psOptions->papszSelFields);
    CSLDestroy( psOptions->papszFieldMap );
    CSLDestroy( psOptions->papszMapFieldType );
    CSLDestroy( psOptions->papszLayers );
    CSLDestroy( psOptions->papszDSCO );
    CSLDestroy( psOptions->papszLCO );
    CSLDestroy( psOptions->papszDestOpenOptions );
    CSLDestroy( psOptions->papszFieldTypesToString );
    CSLDestroy( psOptions->papszMetadataOptions );

    if( psOptions->pasGCPs != nullptr )
    {
        GDALDeinitGCPs( psOptions->nGCPCount, psOptions->pasGCPs );
        CPLFree( psOptions->pasGCPs );
    }

    if( psOptions->hClipSrc != nullptr )
        OGR_G_DestroyGeometry( psOptions->hClipSrc );
    if( psOptions->hClipDst != nullptr )
        OGR_G_DestroyGeometry( psOptions->hClipDst );
    if( psOptions->hSpatialFilter != nullptr )
        OGR_G_DestroyGeometry( psOptions->hSpatialFilter );

    CPLFree(psOptions);
}

/************************************************************************/
/*                 GDALVectorTranslateOptionsSetProgress()              */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALVectorTranslate().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALVectorTranslateOptionsSetProgress( GDALVectorTranslateOptions *psOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
    if( pfnProgress == GDALTermProgress )
        psOptions->bQuiet = false;
}
