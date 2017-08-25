/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2015, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <map>
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

    /*! output file format name (default is ESRI Shapefile) */
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

    /*! override source SRS */
    char *pszSourceSRSDef;

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

    /*! If set to true, does not propagate default field values to target layer if they exist in
        source layer */
    bool bUnsetDefault;

    /*! to prevent the new default behaviour that consists in, if the output driver has a FID layer
        creation option and we are not in append mode, to preserve the name of the source FID column
        and source feature IDs */
    bool bUnsetFid;

    /*! use the FID of the source features instead of letting the output driver to automatically
        assign a new one. If not in append mode, this behaviour becomes the default if the output
        driver has a FID layer creation option. In which case the name of the source FID column will
        be used and source feature IDs will be attempted to be preserved. This behaviour can be
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

typedef struct
{
    OGRLayer *   poSrcLayer;
    GIntBig      nFeaturesRead;
    bool         bPerFeatureCT;
    OGRLayer    *poDstLayer;
    OGRCoordinateTransformation **papoCT; // size: poDstLayer->GetLayerDefn()->GetFieldCount();
    char       ***papapszTransformOptions; // size: poDstLayer->GetLayerDefn()->GetFieldCount();
    int         *panMap;
    int          iSrcZField;
    int          iSrcFIDField;
    int          iRequestedSrcGeomField;
    bool         bPreserveFID;
} TargetLayerInfo;

typedef struct
{
    OGRLayer         *poSrcLayer;
    TargetLayerInfo  *psInfo;
} AssociatedLayers;

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
    bool                  m_bUnsetDefault;
    bool                  m_bUnsetFid;
    bool                  m_bPreserveFID;
    bool                  m_bCopyMD;
    bool                  m_bNativeData;
    bool                  m_bNewDataSource;

    TargetLayerInfo*            Setup(OGRLayer * poSrcLayer,
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
    int                           m_nCoordDim;
    GeomOperation                 m_eGeomOp;
    double                        m_dfGeomOpParam;
    OGRGeometry                  *m_poClipSrc;
    OGRGeometry                  *m_poClipDst;
    bool                          m_bExplodeCollections;
    bool                          m_bNativeData;
    GIntBig                       m_nLimit;

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
                                                 bool* pbOverwriteActuallyDone);

static void FreeTargetLayerInfo(TargetLayerInfo* psInfo);

/************************************************************************/
/*                           LoadGeometry()                             */
/************************************************************************/

static OGRGeometry* LoadGeometry( const char* pszDS,
                                  const char* pszSQL,
                                  const char* pszLyr,
                                  const char* pszWhere)
{
    GDALDataset *poDS =
        reinterpret_cast<GDALDataset *>(OGROpen(pszDS, FALSE, NULL));
    if (poDS == NULL)
        return NULL;

    OGRLayer *poLyr = NULL;
    if (pszSQL != NULL)
        poLyr = poDS->ExecuteSQL( pszSQL, NULL, NULL );
    else if (pszLyr != NULL)
        poLyr = poDS->GetLayerByName(pszLyr);
    else
        poLyr = poDS->GetLayer(0);

    if (poLyr == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from datasource." );
        GDALClose(poDS);
        return NULL;
    }

    if (pszWhere)
        poLyr->SetAttributeFilter(pszWhere);

    OGRGeometry *poGeom = NULL;
    OGRFeature *poFeat = NULL;
    while ((poFeat = poLyr->GetNextFeature()) != NULL)
    {
        OGRGeometry* poSrcGeom = poFeat->GetGeometryRef();
        if (poSrcGeom)
        {
            const OGRwkbGeometryType eType =
                wkbFlatten(poSrcGeom->getGeometryType());

            if (poGeom == NULL)
                poGeom = OGRGeometryFactory::createGeometry( wkbMultiPolygon );

            if( eType == wkbPolygon )
                ((OGRGeometryCollection*)poGeom)->addGeometry( poSrcGeom );
            else if( eType == wkbMultiPolygon )
            {
                int nGeomCount = OGR_G_GetGeometryCount( (OGRGeometryH)poSrcGeom );

                for( int iGeom = 0; iGeom < nGeomCount; iGeom++ )
                {
                    ((OGRGeometryCollection*)poGeom)->addGeometry(
                                ((OGRGeometryCollection*)poSrcGeom)->getGeometryRef(iGeom) );
                }
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Geometry not of polygon type." );
                OGRGeometryFactory::destroyGeometry(poGeom);
                OGRFeature::DestroyFeature(poFeat);
                if( pszSQL != NULL )
                    poDS->ReleaseResultSet( poLyr );
                GDALClose(poDS);
                return NULL;
            }
        }

        OGRFeature::DestroyFeature(poFeat);
    }

    if( pszSQL != NULL )
        poDS->ReleaseResultSet( poLyr );
    GDALClose(poDS);

    return poGeom;
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
    poFeatureDefn(NULL),
    pasListFields(NULL),
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
    CPLAssert(poFeatureDefn == NULL);

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
        OGRFeature* poSrcFeature;

        GIntBig nFeatureCount = 0;
        if (poSrcLayer->TestCapability(OLCFastFeatureCount))
            nFeatureCount = poSrcLayer->GetFeatureCount();
        GIntBig nFeatureIndex = 0;

        /* Scan the whole layer to compute the maximum number of */
        /* items for each field of list type */
        while( (poSrcFeature = poSrcLayer->GetNextFeature()) != NULL )
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
                        int j;
                        for(j=0;j<nCount;j++)
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
            OGRFeature::DestroyFeature(poSrcFeature);

            nFeatureIndex ++;
            if (pfnProgress != NULL && nFeatureCount != 0)
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
        OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(i)->GetType();
        if (eType == OFTIntegerList ||
            eType == OFTInteger64List ||
            eType == OFTRealList ||
            eType == OFTStringList)
        {
            int nMaxOccurrences = pasListFields[iListField].nMaxOccurrences;
            int nWidth = pasListFields[iListField].nWidth;
            iListField ++;
            int j;
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
                for(j=0;j<nMaxOccurrences;j++)
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
    if (poSrcFeature == NULL)
        return NULL;
    if (poFeatureDefn == NULL)
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
        const OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(iSrcField)->GetType();
        OGRField* psField = poSrcFeature->GetRawFieldRef(iSrcField);
        switch(eType)
        {
            case OFTIntegerList:
            {
                int nCount = psField->IntegerList.nCount;
                if (nCount > nMaxSplitListSubFields)
                    nCount = nMaxSplitListSubFields;
                int* paList = psField->IntegerList.paList;
                for( int j=0;j<nCount; ++j)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTInteger64List:
            {
                int nCount = psField->Integer64List.nCount;
                if (nCount > nMaxSplitListSubFields)
                    nCount = nMaxSplitListSubFields;
                GIntBig* paList = psField->Integer64List.paList;
                for( int j=0; j < nCount; ++j )
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTRealList:
            {
                int nCount = psField->RealList.nCount;
                if (nCount > nMaxSplitListSubFields)
                    nCount = nMaxSplitListSubFields;
                double* paList = psField->RealList.paList;
                for( int j=0; j < nCount; ++j )
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTStringList:
            {
                int nCount = psField->StringList.nCount;
                if (nCount > nMaxSplitListSubFields)
                    nCount = nMaxSplitListSubFields;
                char** paList = psField->StringList.paList;
                for( int j=0; j < nCount; ++j )
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            default:
                poFeature->SetField(iDstField, psField);
                iDstField ++;
                break;
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
    if (poFeatureDefn == NULL)
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
public:

    void               *hTransformArg;
    bool                 bUseTPS;
    OGRSpatialReference* poSRS;

    GCPCoordTransformation( int nGCPCount,
                            const GDAL_GCP *pasGCPList,
                            int  nReqOrder,
                            OGRSpatialReference* poSRSIn)
    {
        if( nReqOrder < 0 )
        {
            bUseTPS = true;
            hTransformArg =
                GDALCreateTPSTransformer( nGCPCount, pasGCPList, FALSE );
        }
        else
        {
            bUseTPS = false;
            hTransformArg =
                GDALCreateGCPTransformer( nGCPCount, pasGCPList, nReqOrder, FALSE );
        }
        poSRS = poSRSIn;
        if( poSRS)
            poSRS->Reference();
    }

    bool IsValid() const { return hTransformArg != NULL; }

    virtual ~GCPCoordTransformation()
    {
        if( hTransformArg != NULL )
        {
            if( bUseTPS )
                GDALDestroyTPSTransformer(hTransformArg);
            else
                GDALDestroyGCPTransformer(hTransformArg);
        }
        if( poSRS)
            poSRS->Dereference();
    }

    virtual OGRSpatialReference *GetSourceCS() override { return poSRS; }
    virtual OGRSpatialReference *GetTargetCS() override { return poSRS; }

    virtual int Transform( int nCount,
                           double *x, double *y, double *z = NULL ) override
    {
        int *pabSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nCount));

        bool bOverallSuccess = CPL_TO_BOOL(TransformEx( nCount, x, y, z, pabSuccess ));

        for( int i = 0; i < nCount; ++i )
        {
            if( !pabSuccess[i] )
            {
                bOverallSuccess = false;
                break;
            }
        }

        CPLFree( pabSuccess );

        return bOverallSuccess;
    }

    virtual int TransformEx( int nCount,
                             double *x, double *y, double *z = NULL,
                             int *pabSuccess = NULL ) override
    {
        if( bUseTPS )
            return GDALTPSTransform( hTransformArg, FALSE,
                                 nCount, x, y, z, pabSuccess );
        else
            return GDALGCPTransform( hTransformArg, FALSE,
                                 nCount, x, y, z, pabSuccess );
    }
};

/************************************************************************/
/*                            CompositeCT                               */
/************************************************************************/

class CompositeCT : public OGRCoordinateTransformation
{
public:

    OGRCoordinateTransformation* poCT1;
    OGRCoordinateTransformation* poCT2;

    CompositeCT( OGRCoordinateTransformation* poCT1In, /* will not be deleted */
                 OGRCoordinateTransformation* poCT2In  /* deleted with OGRCoordinateTransformation::DestroyCT() */ )
    {
        poCT1 = poCT1In;
        poCT2 = poCT2In;
    }

    virtual ~CompositeCT()
    {
        OGRCoordinateTransformation::DestroyCT(poCT2);
    }

    virtual OGRSpatialReference *GetSourceCS() override
    {
        return poCT1 ? poCT1->GetSourceCS() :
               poCT2 ? poCT2->GetSourceCS() : NULL;
    }

    virtual OGRSpatialReference *GetTargetCS() override
    {
        return poCT2 ? poCT2->GetTargetCS() :
               poCT1 ? poCT1->GetTargetCS() : NULL;
    }

    virtual int Transform( int nCount,
                           double *x, double *y, double *z = NULL ) override
    {
        int nResult = TRUE;
        if( poCT1 )
            nResult = poCT1->Transform(nCount, x, y, z);
        if( nResult && poCT2 )
            nResult = poCT2->Transform(nCount, x, y, z);
        return nResult;
    }

    virtual int TransformEx( int nCount,
                             double *x, double *y, double *z = NULL,
                             int *pabSuccess = NULL ) override
    {
        int nResult = TRUE;
        if( poCT1 )
            nResult = poCT1->TransformEx(nCount, x, y, z, pabSuccess);
        if( nResult && poCT2 )
            nResult = poCT2->TransformEx(nCount, x, y, z, pabSuccess);
        return nResult;
    }
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
    if( poSpatialFilter != NULL )
    {
        OGRGeometry* poSpatialFilterReprojected = NULL;
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

        if( pszGeomField != NULL )
        {
            int iGeomField = poLayer->GetLayerDefn()->GetGeomFieldIndex(pszGeomField);
            if( iGeomField >= 0 )
                poLayer->SetSpatialFilter( iGeomField,
                    poSpatialFilterReprojected ? poSpatialFilterReprojected : poSpatialFilter );
            else
                CPLError( CE_Warning, CPLE_AppDefined,"Cannot find geometry field %s.",
                    pszGeomField);
        }
        else
            poLayer->SetSpatialFilter( poSpatialFilterReprojected ? poSpatialFilterReprojected : poSpatialFilter );

        delete poSpatialFilterReprojected;
    }
}

/************************************************************************/
/*                            GetFieldType()                            */
/************************************************************************/

static int GetFieldType(const char* pszArg, int* pnSubFieldType)
{
    *pnSubFieldType = OFSTNone;
    int nLengthBeforeParenthesis = static_cast<int>(strlen(pszArg));
    const char* pszOpenParenthesis = strchr(pszArg, '(');
    if( pszOpenParenthesis )
        nLengthBeforeParenthesis = static_cast<int>(pszOpenParenthesis - pszArg);
    for( int iType = 0; iType <= static_cast<int>(OFTMaxType); iType++ )
    {
         const char* pszFieldTypeName = OGRFieldDefn::GetFieldTypeName(
                                                       (OGRFieldType)iType);
         if( EQUALN(pszArg,pszFieldTypeName,nLengthBeforeParenthesis) &&
             pszFieldTypeName[nLengthBeforeParenthesis] == '\0' )
         {
             if( pszOpenParenthesis != NULL )
             {
                 *pnSubFieldType = -1;
                 CPLString osArgSubType = pszOpenParenthesis + 1;
                 if( !osArgSubType.empty() && osArgSubType.back() == ')' )
                     osArgSubType.resize(osArgSubType.size()-1);
                 for( int iSubType = 0;
                      iSubType <= static_cast<int>(OFSTMaxSubType); iSubType++ )
                 {
                     const char* pszFieldSubTypeName = OGRFieldDefn::GetFieldSubTypeName(
                                                       (OGRFieldSubType)iSubType);
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

    psOptions->pszFormat = CPLStrdup(psOptionsIn->pszFormat);
    if( psOptionsIn->pszOutputSRSDef ) psOptions->pszOutputSRSDef = CPLStrdup(psOptionsIn->pszOutputSRSDef);
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
    psOptions->hClipSrc = ( psOptionsIn->hClipSrc != NULL ) ? OGR_G_Clone(psOptionsIn->hClipSrc) : NULL;
    psOptions->hClipDst = ( psOptionsIn->hClipDst != NULL ) ? OGR_G_Clone(psOptionsIn->hClipDst) : NULL;
    psOptions->hSpatialFilter = ( psOptionsIn->hSpatialFilter != NULL ) ? OGR_G_Clone(psOptionsIn->hSpatialFilter) : NULL;

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
                 static_cast<OGRCoordinateTransformation*>(NULL) ),
        m_poFDefn( NULL )
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
    if( poOutputSRS )
    {
        for( int i=0; i < poNew->m_poFDefn->GetGeomFieldCount(); i++ )
        {
            if( bTransform )
            {
                OGRSpatialReference* poSourceSRS =
                    poBaseLayer->GetLayerDefn()->
                                        GetGeomFieldDefn(i)->GetSpatialRef();
                if( poSourceSRS == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Layer %s has no source SRS for geometry field %s",
                             poBaseLayer->GetName(),
                             poBaseLayer->GetLayerDefn()->
                                        GetGeomFieldDefn(i)->GetNameRef());
                    delete poNew;
                    return NULL;
                }
                else
                {
                    poNew->m_apoCT[i] = OGRCreateCoordinateTransformation(
                                                                   poSourceSRS,
                                                                   poOutputSRS);
                    if( poNew->m_apoCT[i] == NULL )
                    {
                        char        *pszWKT = NULL;

                        CPLError( CE_Failure, CPLE_AppDefined,
                            "Failed to create coordinate transformation between the\n"
                            "following coordinate systems.  This may be because they\n"
                            "are not transformable, or because projection services\n"
                            "(PROJ.4 DLL/.so) could not be loaded." );

                        poSourceSRS->exportToPrettyWkt( &pszWKT, FALSE );
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "Source:\n%s", pszWKT );
                        CPLFree(pszWKT);

                        poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "Target:\n%s", pszWKT );
                        CPLFree(pszWKT);

                        delete poNew;
                        return NULL;
                    }
                }
            }
            poNew->m_poFDefn->GetGeomFieldDefn(i)->SetSpatialRef( poOutputSRS );
        }
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
    if( poSrcFeat == NULL )
        return NULL;
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
        if(poLayer == NULL )
        {
            delete poNew;
            return NULL;
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
        return NULL;
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
    if( poLayer == NULL )
        return NULL;
    poLayer = GDALVectorTranslateWrappedLayer::New(
                                poLayer, false, m_poOutputSRS, m_bTransform);
    if( poLayer == NULL )
        return NULL;

    // Replicate source dataset behaviour: if the fact of calling
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
    if( poLayer == NULL )
        return NULL;
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
        OGR2OGRSpatialReferenceHolder() : m_poSRS(NULL) {}
       ~OGR2OGRSpatialReferenceHolder() { if( m_poSRS) m_poSRS->Release(); }

       void assignNoRefIncrease(OGRSpatialReference* poSRS) {
           CPLAssert(m_poSRS == NULL);
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
    GDALDataset* poWrkSrcDS = poDS;
    OGR2OGRSpatialReferenceHolder oOutputSRSHolder;

    if( psOptions->bSkipFailures )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-skipfailures");
        return NULL;
    }
    if( psOptions->nLayerTransaction >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-lyr_transaction or -ds_transaction");
        return NULL;
    }
    if( psOptions->nFIDToFetch >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fid");
        return NULL;
    }
    if( psOptions->papszLCO )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-lco");
        return NULL;
    }
    if( psOptions->bAddMissingFields )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-addfields");
        return NULL;
    }
    if( psOptions->pszSourceSRSDef )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-s_srs");
        return NULL;
    }
    if( !psOptions->bExactFieldNameMatch )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-relaxedFieldNameMatch");
        return NULL;
    }
    if( psOptions->pszNewLayerName )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nln");
        return NULL;
    }
    if( psOptions->papszSelFields )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-select");
        return NULL;
    }
    if( psOptions->pszSQLStatement )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-sql");
        return NULL;
    }
    if( psOptions->pszDialect )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-dialect");
        return NULL;
    }
    if( psOptions->eGType != GEOMTYPE_UNCHANGED ||
        psOptions->eGeomTypeConversion != GTC_DEFAULT )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nlt");
        return NULL;
    }
    if( psOptions->papszFieldTypesToString )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fieldTypeToString");
        return NULL;
    }
    if( psOptions->papszMapFieldType )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-mapFieldType");
        return NULL;
    }
    if( psOptions->bUnsetFieldWidth )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetFieldWidth");
        return NULL;
    }
    if( psOptions->bWrapDateline )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-wrapdateline");
        return NULL;
    }
    if( psOptions->bClipSrc )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrc");
        return NULL;
    }
    if( psOptions->pszClipSrcSQL )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrcsql");
        return NULL;
    }
    if( psOptions->pszClipSrcLayer )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrclayer");
        return NULL;
    }
    if( psOptions->pszClipSrcWhere )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrcwhere");
        return NULL;
    }
    if( psOptions->pszClipDstDS || psOptions->hClipDst )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdst");
        return NULL;
    }
    if( psOptions->pszClipDstSQL )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstsql");
        return NULL;
    }
    if( psOptions->pszClipDstLayer )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstlayer");
        return NULL;
    }
    if( psOptions->pszClipDstWhere )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstwhere");
        return NULL;
    }
    if( psOptions->bSplitListFields )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-splitlistfields");
        return NULL;
    }
    if( psOptions->nMaxSplitListSubFields >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-maxsubfields");
        return NULL;
    }
    if( psOptions->bExplodeCollections )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-explodecollections");
        return NULL;
    }
    if( psOptions->pszZField )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-zfield");
        return NULL;
    }
    if( psOptions->nGCPCount )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-gcp");
        return NULL;
    }
    if( psOptions->papszFieldMap )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fieldmap");
        return NULL;
    }
    if( psOptions->bForceNullable )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-forceNullable");
        return NULL;
    }
    if( psOptions->bUnsetDefault )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetDefault");
        return NULL;
    }
    if( psOptions->bUnsetFid )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetFid");
        return NULL;
    }
    if( !psOptions->bCopyMD )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nomd");
        return NULL;
    }
    if( !psOptions->bNativeData )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-noNativeData");
        return NULL;
    }
    if( psOptions->nLimit >= 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-limit");
        return NULL;
    }
    if( psOptions->papszMetadataOptions )
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-mo");
        return NULL;
    }

    if( psOptions->pszOutputSRSDef )
    {
        oOutputSRSHolder.assignNoRefIncrease(new OGRSpatialReference());
        if( oOutputSRSHolder.get()->
                SetFromUserInput( psOptions->pszOutputSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to process SRS definition: %s",
                      psOptions->pszOutputSRSDef );
            return NULL;
        }
        poWrkSrcDS = GDALVectorTranslateWrappedDataset::New(
            poDS, oOutputSRSHolder.get(), psOptions->bTransform);
        if( poWrkSrcDS == NULL )
            return NULL;
    }

    if( psOptions->pszWHERE )
    {
        // Hack for GMLAS driver
        if( EQUAL(poDriver->GetDescription(), "GMLAS") )
        {
            if( psOptions->papszLayers == NULL )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-where not supported by this output driver "
                         "without explicit layer name(s)");
                if( poWrkSrcDS != poDS )
                    delete poWrkSrcDS;
                return NULL;
            }
            else
            {
                char** papszIter = psOptions->papszLayers;
                for( ; *papszIter != NULL; ++papszIter )
                {
                    OGRLayer* poSrcLayer = poDS->GetLayerByName(*papszIter);
                    if( poSrcLayer != NULL )
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
            return NULL;
        }
    }

    if( psOptions->hSpatialFilter )
    {
        for( int i=0; i<poWrkSrcDS->GetLayerCount();++i)
        {
            OGRLayer* poSrcLayer = poWrkSrcDS->GetLayer(i);
            if( poSrcLayer &&
                poSrcLayer->GetLayerDefn()->GetGeomFieldCount() > 0 &&
                (psOptions->papszLayers == NULL ||
                 CSLFindString(psOptions->papszLayers,
                               poSrcLayer->GetName())>=0) )
            {
                if( psOptions->pszGeomField != NULL )
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
            for( ; *papszIter != NULL; ++papszIter )
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
            return NULL;
        }
    }

    // Hack for GMLAS driver (this speed up deletion by avoiding the GML
    // driver to try parsing a pre-existing file). Could be potentially
    // removed if the GML driver implemented fast dataset opening (ie
    // without parsing) and GetFileList()
    if( EQUAL(poDriver->GetDescription(), "GMLAS") )
    {
        GDALDriverH hIdentifyingDriver = GDALIdentifyDriver(pszDest, NULL);
        if( hIdentifyingDriver != NULL &&
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
 * This is the equivalent of the <a href="ogr2ogr.html">ogr2ogr</a> utility.
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
 * @param pbUsageError the pointer to int variable to determine any usage error has occurred
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALVectorTranslate( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                                  GDALDatasetH *pahSrcDS,
                                  const GDALVectorTranslateOptions *psOptionsIn, int *pbUsageError )

{
    OGR2OGRSpatialReferenceHolder oOutputSRSHolder;
    OGRSpatialReference oSourceSRS;
    OGRSpatialReference oSpatSRS;
    OGRSpatialReference *poSourceSRS = NULL;
    OGRSpatialReference* poSpatSRS = NULL;
    bool bAppend = false;
    bool bUpdate = false;
    bool bOverwrite = false;
    CPLString osDateLineOffset;
    int nRetCode = 0;

    if( pszDest == NULL && hDstDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszDest == NULL && hDstDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( nSrcCount != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "nSrcCount != 1");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALDatasetH hSrcDS = pahSrcDS[0];
    if( hSrcDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "hSrcDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALVectorTranslateOptions* psOptions =
        (psOptionsIn) ? GDALVectorTranslateOptionsClone(psOptionsIn) :
                        GDALVectorTranslateOptionsNew(NULL, NULL);

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
    else if( hDstDS != NULL )
    {
        bUpdate = true;
    }

    osDateLineOffset = CPLOPrintf("%g", psOptions->dfDateLineOffset);

    if( psOptions->bPreserveFID && psOptions->bExplodeCollections )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "cannot use -preserve_fid and -explodecollections at the same time.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return NULL;
    }

    if (psOptions->papszFieldMap && !bAppend)
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "if -fieldmap is specified, -append must also be specified");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return NULL;
    }

    if (psOptions->papszFieldMap && psOptions->bAddMissingFields)
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "if -addfields is specified, -fieldmap cannot be used.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return NULL;
    }

    if( psOptions->papszFieldTypesToString && psOptions->papszMapFieldType )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "-fieldTypeToString and -mapFieldType are exclusive.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return NULL;
    }

    if( psOptions->pszSourceSRSDef != NULL && psOptions->pszOutputSRSDef == NULL && psOptions->pszSpatSRSDef == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "if -s_srs is specified, -t_srs and/or -spat_srs must also be specified.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALVectorTranslateOptionsFree(psOptions);
        return NULL;
    }

    if( psOptions->bClipSrc && psOptions->pszClipSrcDS != NULL)
    {
        psOptions->hClipSrc = (OGRGeometryH) LoadGeometry(psOptions->pszClipSrcDS, psOptions->pszClipSrcSQL, psOptions->pszClipSrcLayer, psOptions->pszClipSrcWhere);
        if (psOptions->hClipSrc == NULL)
        {
            CPLError( CE_Failure,CPLE_IllegalArg, "cannot load source clip geometry");
            GDALVectorTranslateOptionsFree(psOptions);
            return NULL;
        }
    }
    else if( psOptions->bClipSrc && psOptions->hClipSrc == NULL )
    {
        if (psOptions->hSpatialFilter)
            psOptions->hClipSrc = (OGRGeometryH)((OGRGeometry *)(psOptions->hSpatialFilter))->clone();
        if (psOptions->hClipSrc == NULL)
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "-clipsrc must be used with -spat option or a\n"
                             "bounding box, WKT string or datasource must be specified");
            if(pbUsageError)
                *pbUsageError = TRUE;
            GDALVectorTranslateOptionsFree(psOptions);
            return NULL;
        }
    }

    if( psOptions->pszClipDstDS != NULL)
    {
        psOptions->hClipDst = (OGRGeometryH) LoadGeometry(psOptions->pszClipDstDS, psOptions->pszClipDstSQL, psOptions->pszClipDstLayer, psOptions->pszClipDstWhere);
        if (psOptions->hClipDst == NULL)
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "cannot load dest clip geometry");
            GDALVectorTranslateOptionsFree(psOptions);
            return NULL;
        }
    }

    GDALDataset *poDS = (GDALDataset *) hSrcDS;
    GDALDataset *poODS = NULL;
    GDALDriver *poDriver = NULL;
    CPLString osDestFilename;

    if(hDstDS)
    {
        poODS = (GDALDataset *) hDstDS;
        osDestFilename = poODS->GetDescription();
    }
    else
        osDestFilename = pszDest;

    /* Various tests to avoid overwriting the source layer(s) */
    /* or to avoid appending a layer to itself */
    if( bUpdate && strcmp(osDestFilename, poDS->GetDescription()) == 0 &&
        (bOverwrite || bAppend) )
    {
        bool bError = false;
        if (psOptions->pszNewLayerName == NULL)
            bError = true;
        else if (CSLCount(psOptions->papszLayers) == 1)
            bError = strcmp(psOptions->pszNewLayerName, psOptions->papszLayers[0]) == 0;
        else if (psOptions->pszSQLStatement == NULL)
            bError = true;
        if (bError)
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                        "-nln name must be specified combined with "
                        "a single source layer name,\nor a -sql statement, and "
                        "name must be different from an existing layer.");
            GDALVectorTranslateOptionsFree(psOptions);
            return NULL;
        }
    }
    else if( !bUpdate && strcmp(osDestFilename, poDS->GetDescription()) == 0 &&
             !EQUAL(psOptions->pszFormat, "Memory") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Source and destination datasets must be different "
                  "in non-update mode." );
        GDALVectorTranslateOptionsFree(psOptions);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the output datasource as an existing, writable      */
/* -------------------------------------------------------------------- */

    if( bUpdate && poODS == NULL )
    {
        poODS = (GDALDataset*) GDALOpenEx( osDestFilename,
                GDAL_OF_UPDATE | GDAL_OF_VECTOR, NULL, psOptions->papszDestOpenOptions, NULL );

        if( poODS == NULL )
        {
            if (bOverwrite || bAppend)
            {
                poODS = (GDALDataset*) GDALOpenEx( osDestFilename,
                            GDAL_OF_VECTOR, NULL, psOptions->papszDestOpenOptions, NULL );
                if (poODS == NULL)
                {
                    /* OK the datasource doesn't exist at all */
                    bUpdate = false;
                }
                else
                {
                    if( poODS != NULL )
                        poDriver = poODS->GetDriver();
                    GDALClose(poODS);
                    poODS = NULL;
                }
            }

            if (bUpdate)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Unable to open existing output datasource `%s'.",
                        osDestFilename.c_str() );
                GDALVectorTranslateOptionsFree(psOptions);
                return NULL;
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

        const char* pszOGRCompatFormat = psOptions->pszFormat;
        // Special processing for non-unified drivers that have the same name
        // as GDAL and OGR drivers
        // Other candidates could be VRT, SDTS, OGDI and PDS, but they don't
        // have write capabilities.
        if( EQUAL(pszOGRCompatFormat, "GMT") )
            pszOGRCompatFormat = "OGR_GMT";
        poDriver = poDM->GetDriverByName(pszOGRCompatFormat);
        if( poDriver == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to find driver `%s'.", psOptions->pszFormat );
            GDALVectorTranslateOptionsFree(psOptions);
            return NULL;
        }

        char** papszDriverMD = poDriver->GetMetadata();
        if( !CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                               GDAL_DCAP_VECTOR, "FALSE") ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s driver has no vector capabilities.",
                      psOptions->pszFormat );
            GDALVectorTranslateOptionsFree(psOptions);
            return NULL;
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
            return NULL;
        }

        if( psOptions->papszDestOpenOptions != NULL )
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
            psOptions->pszSQLStatement == NULL &&
            (CSLCount(psOptions->papszLayers) > 1 ||
             (CSLCount(psOptions->papszLayers) == 0 && poDS->GetLayerCount() > 1)) &&
            psOptions->pszNewLayerName == NULL &&
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
                return NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      Create the output data source.                                  */
/* -------------------------------------------------------------------- */
        poODS = poDriver->Create( osDestFilename, 0, 0, 0, GDT_Unknown, psOptions->papszDSCO );
        if( poODS == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "%s driver failed to create %s",
                    psOptions->pszFormat, osDestFilename.c_str() );
            GDALVectorTranslateOptionsFree(psOptions);
            return NULL;
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
            char    *pszKey = NULL;
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
    if( psOptions->pszOutputSRSDef != NULL )
    {
        oOutputSRSHolder.assignNoRefIncrease(new OGRSpatialReference());
        if( oOutputSRSHolder.get()->
                SetFromUserInput( psOptions->pszOutputSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                    psOptions->pszOutputSRSDef );
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == NULL ) GDALClose( poODS );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse the source SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    if( psOptions->pszSourceSRSDef != NULL )
    {
        if( oSourceSRS.SetFromUserInput( psOptions->pszSourceSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                    psOptions->pszSourceSRSDef );
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == NULL ) GDALClose( poODS );
            return NULL;
        }
        poSourceSRS = &oSourceSRS;
    }

/* -------------------------------------------------------------------- */
/*      Parse spatial filter SRS if needed.                             */
/* -------------------------------------------------------------------- */
    if( psOptions->hSpatialFilter != NULL && psOptions->pszSpatSRSDef != NULL )
    {
        if( psOptions->pszSQLStatement )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "-spat_srs not compatible with -sql.");
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == NULL ) GDALClose( poODS );
            return NULL;
        }
        OGREnvelope sEnvelope;
        ((OGRGeometry*)psOptions->hSpatialFilter)->getEnvelope(&sEnvelope);
        if( oSpatSRS.SetFromUserInput( psOptions->pszSpatSRSDef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                    psOptions->pszSpatSRSDef );
            GDALVectorTranslateOptionsFree(psOptions);
            if( hDstDS == NULL ) GDALClose( poODS );
            return NULL;
        }
        poSpatSRS = &oSpatSRS;
    }

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    GCPCoordTransformation *poGCPCoordTrans = NULL;
    if( psOptions->nGCPCount > 0 )
    {
        poGCPCoordTrans = new GCPCoordTransformation( psOptions->nGCPCount, psOptions->pasGCPs,
                                                      psOptions->nTransformOrder,
                                                      poSourceSRS ? poSourceSRS : oOutputSRSHolder.get() );
        if( !(poGCPCoordTrans->IsValid()) )
        {
            delete poGCPCoordTrans;
            poGCPCoordTrans = NULL;
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
    oSetup.m_bUnsetDefault = psOptions->bUnsetDefault;
    oSetup.m_bUnsetFid = psOptions->bUnsetFid;
    oSetup.m_bPreserveFID = psOptions->bPreserveFID;
    oSetup.m_bCopyMD = psOptions->bCopyMD;
    oSetup.m_bNativeData = psOptions->bNativeData;
    oSetup.m_bNewDataSource = bNewDataSource;

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
    oTranslator.m_nCoordDim = psOptions->nCoordDim;
    oTranslator.m_eGeomOp = psOptions->eGeomOp;
    oTranslator.m_dfGeomOpParam = psOptions->dfGeomOpParam;
    oTranslator.m_poClipSrc = (OGRGeometry *)psOptions->hClipSrc;
    oTranslator.m_poClipDst = (OGRGeometry *)psOptions->hClipDst;
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
    if( psOptions->pszSQLStatement != NULL )
    {
        /* Special case: if output=input, then we must likely destroy the */
        /* old table before to avoid transaction issues. */
        if( poDS == poODS && psOptions->pszNewLayerName != NULL && bOverwrite )
            GetLayerAndOverwriteIfNecessary(poODS, psOptions->pszNewLayerName, bOverwrite, NULL, NULL);

        if( psOptions->pszWHERE != NULL )
            CPLError( CE_Warning, CPLE_AppDefined, "-where clause ignored in combination with -sql." );
        if( CSLCount(psOptions->papszLayers) > 0 )
            CPLError( CE_Warning, CPLE_AppDefined, "layer names ignored in combination with -sql." );

        OGRLayer *poResultSet
            = poDS->ExecuteSQL( psOptions->pszSQLStatement,
                                (psOptions->pszGeomField == NULL) ? (OGRGeometry*)psOptions->hSpatialFilter : NULL,
                                psOptions->pszDialect );

        if( poResultSet != NULL )
        {
            if( psOptions->hSpatialFilter != NULL && psOptions->pszGeomField != NULL )
            {
                int iGeomField = poResultSet->GetLayerDefn()->GetGeomFieldIndex(psOptions->pszGeomField);
                if( iGeomField >= 0 )
                    poResultSet->SetSpatialFilter( iGeomField, (OGRGeometry*)psOptions->hSpatialFilter );
                else
                    CPLError( CE_Warning, CPLE_AppDefined, "Cannot find geometry field %s.",
                           psOptions->pszGeomField);
            }

            GIntBig nCountLayerFeatures = 0;
            GDALProgressFunc pfnProgress = NULL;
            void        *pProgressArg = NULL;
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
                poPassedLayer = new OGRSplitListFieldLayer(poPassedLayer, psOptions->nMaxSplitListSubFields);
                int nRet = ((OGRSplitListFieldLayer*)poPassedLayer)->BuildLayerDefn(NULL, NULL);
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
            VSIStatBufL  sStat;
            if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
                psOptions->pszNewLayerName == NULL &&
                VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode))
            {
                psOptions->pszNewLayerName = CPLStrdup(CPLGetBasename(osDestFilename));
            }

            TargetLayerInfo* psInfo = oSetup.Setup(poPassedLayer,
                                                   psOptions->pszNewLayerName,
                                                   psOptions,
                                                   nTotalEventsDone);

            poPassedLayer->ResetReading();

            if( psInfo == NULL ||
                !oTranslator.Translate( NULL, psInfo,
                                        nCountLayerFeatures, NULL,
                                        nTotalEventsDone,
                                        pfnProgress, pProgressArg, psOptions ))
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Terminating translation prematurely after failed\n"
                          "translation from sql statement." );

                nRetCode = 1;
            }

            FreeTargetLayerInfo(psInfo);

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
            if( hDstDS == NULL ) GDALClose( poODS );
            delete poGCPCoordTrans;
            return NULL;
        }

        // Make sure to probe all layers in case some are by default invisible
        for( char** papszIter = psOptions->papszLayers;
                    papszIter && *papszIter; ++papszIter )
        {
            OGRLayer        *poLayer = poDS->GetLayerByName(*papszIter);

            if( poLayer == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch requested layer %s!",
                          *papszIter );
                GDALVectorTranslateOptionsFree(psOptions);
                if( hDstDS == NULL ) GDALClose( poODS );
                delete poGCPCoordTrans;
                return NULL;
            }
        }

        int nSrcLayerCount = poDS->GetLayerCount();
        AssociatedLayers* pasAssocLayers = static_cast<AssociatedLayers *>(
            CPLCalloc(nSrcLayerCount, sizeof(AssociatedLayers)));

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            (CSLCount(psOptions->papszLayers) == 1 || nSrcLayerCount == 1) && psOptions->pszNewLayerName == NULL &&
            VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode))
        {
            psOptions->pszNewLayerName = CPLStrdup(CPLGetBasename(osDestFilename));
        }

        GDALProgressFunc pfnProgress = NULL;
        void        *pProgressArg = NULL;
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

                if( poLayer == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch advertised layer %d!",
                            iLayer );
                    GDALVectorTranslateOptionsFree(psOptions);
                    if( hDstDS == NULL ) GDALClose( poODS );
                    delete poGCPCoordTrans;
                    return NULL;
                }

                psOptions->papszLayers[iLayer] = CPLStrdup(poLayer->GetName());
            }
        }
        else
        {
            bool bSrcIsOSM = (strcmp(poDS->GetDriverName(), "OSM") == 0);
            if ( bSrcIsOSM )
            {
                CPLString osInterestLayers = "SET interest_layers =";
                for( int iLayer = 0; psOptions->papszLayers[iLayer] != NULL; iLayer++ )
                {
                    if( iLayer != 0 ) osInterestLayers += ",";
                    osInterestLayers += psOptions->papszLayers[iLayer];
                }

                poDS->ExecuteSQL(osInterestLayers.c_str(), NULL, NULL);
            }
        }

/* -------------------------------------------------------------------- */
/*      First pass to set filters.                                      */
/* -------------------------------------------------------------------- */
        std::map<OGRLayer*, int> oMapLayerToIdx;

        for( int iLayer = 0; iLayer < nSrcLayerCount; iLayer++ )
        {
            OGRLayer        *poLayer = poDS->GetLayer(iLayer);
            if( poLayer == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch advertised layer %d!",
                        iLayer );
                GDALVectorTranslateOptionsFree(psOptions);
                if( hDstDS == NULL ) GDALClose( poODS );
                delete poGCPCoordTrans;
                return NULL;
            }

            pasAssocLayers[iLayer].poSrcLayer = poLayer;

            if( CSLFindString(psOptions->papszLayers, poLayer->GetName()) >= 0 )
            {
                if( psOptions->pszWHERE != NULL )
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
                            if( hDstDS == NULL ) GDALClose( poODS );
                            delete poGCPCoordTrans;
                            return NULL;
                        }
                    }
                }

                ApplySpatialFilter(poLayer,
                                   (OGRGeometry*)psOptions->hSpatialFilter,
                                   poSpatSRS, psOptions->pszGeomField,
                                   poSourceSRS );

                oMapLayerToIdx[ poLayer ] = iLayer;
            }
            pasAssocLayers[iLayer].psInfo = NULL;
        }

/* -------------------------------------------------------------------- */
/*      Second pass to process features in a interleaved layer mode.    */
/* -------------------------------------------------------------------- */
        bool bTargetLayersHaveBeenCreated = false;
        while( true )
        {
            OGRLayer* poFeatureLayer = NULL;
            OGRFeature* poFeature = poDS->GetNextFeature(&poFeatureLayer,
                                                         NULL,
                                                         pfnProgress,
                                                         pProgressArg);
            if( poFeature == NULL )
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

                        TargetLayerInfo* psInfo = oSetup.Setup(poLayer,
                                                               psOptions->pszNewLayerName,
                                                               psOptions,
                                                               nTotalEventsDone);

                        if( psInfo == NULL && !psOptions->bSkipFailures )
                        {
                            GDALVectorTranslateOptionsFree(psOptions);
                            if( hDstDS == NULL ) GDALClose( poODS );
                            delete poGCPCoordTrans;
                            OGRFeature::DestroyFeature(poFeature);
                            return NULL;
                        }

                        pasAssocLayers[iLayer].psInfo = psInfo;
                    }
                    if( nRetCode )
                        break;
                }

                int iLayer = oIter->second;
                TargetLayerInfo *psInfo = pasAssocLayers[iLayer].psInfo;
                if( (psInfo == NULL ||
                     !oTranslator.Translate( poFeature, psInfo,
                                            0, NULL,
                                            nTotalEventsDone,
                                            NULL, NULL, psOptions ))
                    && !psOptions->bSkipFailures )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "Terminating translation prematurely after failed\n"
                            "translation of layer %s (use -skipfailures to skip errors)",
                            poFeatureLayer->GetName() );

                    nRetCode = 1;
                    break;
                }
                if( psInfo == NULL )
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

                TargetLayerInfo* psInfo = oSetup.Setup(poLayer,
                                                       psOptions->pszNewLayerName,
                                                       psOptions,
                                                       nTotalEventsDone);

                if( psInfo == NULL && !psOptions->bSkipFailures )
                {
                    GDALVectorTranslateOptionsFree(psOptions);
                    if( hDstDS == NULL ) GDALClose( poODS );
                    delete poGCPCoordTrans;
                    return NULL;
                }

                pasAssocLayers[iLayer].psInfo = psInfo;
            }
        }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
        for( int iLayer = 0; iLayer < nSrcLayerCount;  iLayer++ )
        {
            if( pasAssocLayers[iLayer].psInfo )
                FreeTargetLayerInfo(pasAssocLayers[iLayer].psInfo);
        }
        CPLFree(pasAssocLayers);
    }

    else
    {
        int nLayerCount = 0;
        OGRLayer** papoLayers = NULL;

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
        if ( CSLCount(psOptions->papszLayers) == 0)
        {
            nLayerCount = poDS->GetLayerCount();
            papoLayers = static_cast<OGRLayer **>(
                CPLMalloc(sizeof(OGRLayer*) * nLayerCount));

            for( int iLayer = 0;
                 iLayer < nLayerCount;
                 iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch advertised layer %d!",
                            iLayer );
                    GDALVectorTranslateOptionsFree(psOptions);
                    if( hDstDS == NULL ) GDALClose( poODS );
                    delete poGCPCoordTrans;
                    return NULL;
                }

                papoLayers[iLayer] = poLayer;
            }
        }
/* -------------------------------------------------------------------- */
/*      Process specified data source layers.                           */
/* -------------------------------------------------------------------- */
        else
        {
            nLayerCount = CSLCount(psOptions->papszLayers);
            papoLayers = static_cast<OGRLayer **>(
              CPLMalloc(sizeof(OGRLayer*) * nLayerCount));

            for( int iLayer = 0;
                psOptions->papszLayers[iLayer] != NULL;
                iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayerByName(psOptions->papszLayers[iLayer]);

                if( poLayer == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Couldn't fetch requested layer '%s'!",
                             psOptions->papszLayers[iLayer] );
                    if (!psOptions->bSkipFailures)
                    {
                        GDALVectorTranslateOptionsFree(psOptions);
                        if( hDstDS == NULL ) GDALClose( poODS );
                        delete poGCPCoordTrans;
                        return NULL;
                    }
                }

                papoLayers[iLayer] = poLayer;
            }
        }

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            nLayerCount == 1 && psOptions->pszNewLayerName == NULL &&
            VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode))
        {
            psOptions->pszNewLayerName = CPLStrdup(CPLGetBasename(osDestFilename));
        }

        GIntBig* panLayerCountFeatures =
            static_cast<GIntBig *>(CPLCalloc(sizeof(GIntBig), nLayerCount));
        GIntBig nCountLayersFeatures = 0;
        GIntBig nAccCountFeatures = 0;
        int iLayer;

        /* First pass to apply filters and count all features if necessary */
        for( iLayer = 0;
            iLayer < nLayerCount;
            iLayer++ )
        {
            OGRLayer        *poLayer = papoLayers[iLayer];
            if (poLayer == NULL)
                continue;

            if( psOptions->pszWHERE != NULL )
            {
                if( poLayer->SetAttributeFilter( psOptions->pszWHERE ) != OGRERR_NONE )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "SetAttributeFilter(%s) on layer '%s' failed.",
                             psOptions->pszWHERE, poLayer->GetName() );
                    if (!psOptions->bSkipFailures)
                    {
                        GDALVectorTranslateOptionsFree(psOptions);
                        if( hDstDS == NULL ) GDALClose( poODS );
                        delete poGCPCoordTrans;
                        return NULL;
                    }
                }
            }

            ApplySpatialFilter(poLayer, (OGRGeometry*)psOptions->hSpatialFilter, poSpatSRS, psOptions->pszGeomField, poSourceSRS);

            if (psOptions->bDisplayProgress)
            {
                if (!poLayer->TestCapability(OLCFastFeatureCount))
                {
                    CPLError(CE_Warning, CPLE_NotSupported, "Progress turned off as fast feature count is not available.");
                    psOptions->bDisplayProgress = false;
                }
                else
                {
                    panLayerCountFeatures[iLayer] = poLayer->GetFeatureCount();
                    nCountLayersFeatures += panLayerCountFeatures[iLayer];
                }
            }
        }

        /* Second pass to do the real job */
        for( iLayer = 0;
            iLayer < nLayerCount && nRetCode == 0;
            iLayer++ )
        {
            OGRLayer        *poLayer = papoLayers[iLayer];
            if (poLayer == NULL)
                continue;

            GDALProgressFunc pfnProgress = NULL;
            void        *pProgressArg = NULL;

            OGRLayer* poPassedLayer = poLayer;
            if (psOptions->bSplitListFields)
            {
                poPassedLayer = new OGRSplitListFieldLayer(poPassedLayer, psOptions->nMaxSplitListSubFields);

                if (psOptions->bDisplayProgress && psOptions->nMaxSplitListSubFields != 1 &&
                    nCountLayersFeatures != 0)
                {
                    pfnProgress = GDALScaledProgress;
                    pProgressArg =
                        GDALCreateScaledProgress(nAccCountFeatures * 1.0 / nCountLayersFeatures,
                                                (nAccCountFeatures + panLayerCountFeatures[iLayer] / 2) * 1.0 / nCountLayersFeatures,
                                                psOptions->pfnProgress,
                                                psOptions->pProgressData);
                }
                else
                {
                    pfnProgress = NULL;
                    pProgressArg = NULL;
                }

                int nRet = ((OGRSplitListFieldLayer*)poPassedLayer)->BuildLayerDefn(pfnProgress, pProgressArg);
                if (!nRet)
                {
                    delete poPassedLayer;
                    poPassedLayer = poLayer;
                }

                if (psOptions->bDisplayProgress)
                    GDALDestroyScaledProgress(pProgressArg);
                pfnProgress = NULL;
                pProgressArg = NULL;
            }

            if (psOptions->bDisplayProgress)
            {
                if( nCountLayersFeatures != 0 )
                {
                    pfnProgress = GDALScaledProgress;
                    GIntBig nStart = 0;
                    if (poPassedLayer != poLayer && psOptions->nMaxSplitListSubFields != 1)
                        nStart = panLayerCountFeatures[iLayer] / 2;
                    pProgressArg =
                        GDALCreateScaledProgress((nAccCountFeatures + nStart) * 1.0 / nCountLayersFeatures,
                                                (nAccCountFeatures + panLayerCountFeatures[iLayer]) * 1.0 / nCountLayersFeatures,
                                                psOptions->pfnProgress,
                                                psOptions->pProgressData);
                }
            }

            nAccCountFeatures += panLayerCountFeatures[iLayer];

            TargetLayerInfo* psInfo = oSetup.Setup(poPassedLayer,
                                                   psOptions->pszNewLayerName,
                                                   psOptions,
                                                   nTotalEventsDone);

            poPassedLayer->ResetReading();

            if( (psInfo == NULL ||
                !oTranslator.Translate( NULL, psInfo,
                                        panLayerCountFeatures[iLayer], NULL,
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

            FreeTargetLayerInfo(psInfo);

            if (poPassedLayer != poLayer)
                delete poPassedLayer;

            if (psOptions->bDisplayProgress)
                GDALDestroyScaledProgress(pProgressArg);
        }

        CPLFree(panLayerCountFeatures);
        CPLFree(papoLayers);
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
        return (GDALDatasetH) poODS;
    else
    {
        if( hDstDS == NULL ) GDALClose( poODS );
        return NULL;
    }
}

/************************************************************************/
/*                               SetZ()                                 */
/************************************************************************/
static void SetZ (OGRGeometry* poGeom, double dfZ )
{
    if (poGeom == NULL)
        return;
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
        case wkbPoint:
            ((OGRPoint*)poGeom)->setZ(dfZ);
            break;

        case wkbLineString:
        case wkbLinearRing:
        {
            int i;
            OGRLineString* poLS = (OGRLineString*) poGeom;
            for(i=0;i<poLS->getNumPoints();i++)
                poLS->setPoint(i, poLS->getX(i), poLS->getY(i), dfZ);
            break;
        }

        case wkbPolygon:
        {
            int i;
            OGRPolygon* poPoly = (OGRPolygon*) poGeom;
            SetZ(poPoly->getExteriorRing(), dfZ);
            for(i=0;i<poPoly->getNumInteriorRings();i++)
                SetZ(poPoly->getInteriorRing(i), dfZ);
            break;
        }

        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            int i;
            OGRGeometryCollection* poGeomColl = (OGRGeometryCollection*) poGeom;
            for(i=0;i<poGeomColl->getNumGeometries();i++)
                SetZ(poGeomColl->getGeometryRef(i), dfZ);
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                       ForceCoordDimension()                          */
/************************************************************************/

static int ForceCoordDimension(int eGType, int nCoordDim)
{
    if( nCoordDim == 2 && eGType != wkbNone )
        return wkbFlatten(eGType);
    else if( nCoordDim == 3 && eGType != wkbNone )
        return wkbSetZ(wkbFlatten((OGRwkbGeometryType)eGType));
    else if( nCoordDim == COORD_DIM_XYM && eGType != wkbNone )
        return wkbSetM(wkbFlatten((OGRwkbGeometryType)eGType));
    else if( nCoordDim == 4 && eGType != wkbNone )
        return OGR_GT_SetModifier((OGRwkbGeometryType)eGType, TRUE, TRUE);
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
                                                 bool* pbOverwriteActuallyDone)
{
    if( pbErrorOccurred )
        *pbErrorOccurred = false;
    if( pbOverwriteActuallyDone )
        *pbOverwriteActuallyDone = false;

    /* GetLayerByName() can instantiate layers that would have been */
    /* 'hidden' otherwise, for example, non-spatial tables in a */
    /* PostGIS-enabled database, so this apparently useless command is */
    /* not useless. (#4012) */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRLayer* poDstLayer = poDstDS->GetLayerByName(pszNewLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    int iLayer = -1;
    if (poDstLayer != NULL)
    {
        int nLayerCount = poDstDS->GetLayerCount();
        for( iLayer = 0; iLayer < nLayerCount; iLayer++ )
        {
            OGRLayer        *poLayer = poDstDS->GetLayer(iLayer);
            if (poLayer == poDstLayer)
                break;
        }

        if (iLayer == nLayerCount)
            /* should not happen with an ideal driver */
            poDstLayer = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the user requested overwrite, and we have the layer in       */
/*      question we need to delete it now so it will get recreated      */
/*      (overwritten).                                                  */
/* -------------------------------------------------------------------- */
    if( poDstLayer != NULL && bOverwrite )
    {
        if( poDstDS->DeleteLayer( iLayer ) != OGRERR_NONE )
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
        poDstLayer = NULL;
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
    if ( eGeomTypeConversion == GTC_PROMOTE_TO_MULTI )
    {
        if( eGType == wkbTriangle || eGType == wkbTIN ||
            eGType == wkbPolyhedralSurface )
        {
            eRetType = wkbMultiPolygon;
        }
        else if( !OGR_GT_IsSubClassOf(eGType, wkbGeometryCollection) )
        {
            eRetType = OGR_GT_GetCollection(eGType);
        }
    }
    else if ( eGeomTypeConversion == GTC_CONVERT_TO_LINEAR )
        eRetType = OGR_GT_GetLinear(eGType);
    if ( eGeomTypeConversion == GTC_CONVERT_TO_CURVE )
        eRetType = OGR_GT_GetCurve(eGType);
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
    if (papszFieldTypesToString != NULL )
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
    else if (papszMapFieldType != NULL)
    {
        CPLString osLookupString;
        osLookupString.Printf("%s(%s)",
                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                        OGRFieldDefn::GetFieldSubTypeName(oFieldDefn.GetSubType()));

        const char* pszType = CSLFetchNameValue(papszMapFieldType, osLookupString);
        if( pszType == NULL )
            pszType = CSLFetchNameValue(papszMapFieldType,
                                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()));
        if( pszType == NULL )
            pszType = CSLFetchNameValue(papszMapFieldType, "All");
        if( pszType != NULL )
        {
            int iSubType;
            int iType = GetFieldType(pszType, &iSubType);
            if( iType >= 0 && iSubType >= 0 )
            {
                oFieldDefn.SetSubType(OFSTNone);
                oFieldDefn.SetType((OGRFieldType)iType);
                oFieldDefn.SetSubType((OGRFieldSubType)iSubType);
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
        oFieldDefn.SetDefault(NULL);

    if( poDstDS->GetDriver() != NULL &&
        poDstDS->GetDriver()->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES) != NULL &&
        strstr(poDstDS->GetDriver()->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES),
                OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType())) == NULL )
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
    else if( poDstDS->GetDriver() != NULL &&
             poDstDS->GetDriver()->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES) == NULL )
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

TargetLayerInfo* SetupTargetLayer::Setup(OGRLayer* poSrcLayer,
                                         const char* pszNewLayerName,
                                         GDALVectorTranslateOptions *psOptions,
                                         GIntBig& nTotalEventsDone)
{
    OGRLayer    *poDstLayer;
    OGRFeatureDefn *poSrcFDefn;
    OGRFeatureDefn *poDstFDefn = NULL;
    int eGType = m_eGType;
    bool bPreserveFID = m_bPreserveFID;
    bool bAppend = m_bAppend;

    if( pszNewLayerName == NULL )
        pszNewLayerName = poSrcLayer->GetName();

/* -------------------------------------------------------------------- */
/*      Get other info.                                                 */
/* -------------------------------------------------------------------- */
    poSrcFDefn = poSrcLayer->GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Find requested geometry fields.                                 */
/* -------------------------------------------------------------------- */
    std::vector<int> anRequestedGeomFields;
    int nSrcGeomFieldCount = poSrcFDefn->GetGeomFieldCount();
    if (m_papszSelFields && !bAppend )
    {
        for( int iField=0; m_papszSelFields[iField] != NULL; iField++)
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
                        return NULL;
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
                return NULL;
            else
                anRequestedGeomFields.resize(0);
        }
    }

    OGRSpatialReference* poOutputSRS = m_poOutputSRS;
    if( poOutputSRS == NULL && !m_bNullifyOutputSRS )
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
    if (m_pszZField != NULL)
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
    poDstLayer = GetLayerAndOverwriteIfNecessary(m_poDstDS,
                                                 pszNewLayerName,
                                                 m_bOverwrite,
                                                 &bErrorOccurred,
                                                 &bOverwriteActuallyDone);
    if( bErrorOccurred )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If the layer does not exist, then create it.                    */
/* -------------------------------------------------------------------- */
    if( poDstLayer == NULL )
    {
        if( !m_poDstDS->TestCapability( ODsCCreateLayer ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Layer '%s' does not already exist in the output dataset, and "
                      "cannot be created by the output driver.",
                      pszNewLayerName );
            return NULL;
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
                eGType = wkbNone;

            bool bHasZ = CPL_TO_BOOL(wkbHasZ((OGRwkbGeometryType)eGType));
            eGType = ConvertType(m_eGeomTypeConversion, (OGRwkbGeometryType)eGType);

            if ( m_bExplodeCollections )
            {
                OGRwkbGeometryType eFGType = wkbFlatten(eGType);
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
                eGType = wkbSetZ((OGRwkbGeometryType)eGType);
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
        else if( anRequestedGeomFields.size() == 1 &&
                 m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
        {
            eGCreateLayerType = wkbNone;
        }
        // If the source feature has a single geometry column that is not nullable
        // and that ODsCCreateGeomFieldAfterCreateLayer is available, use it
        // so as to be able to set the not null constraint (if the driver supports it)
        else if( anRequestedGeomFields.empty() &&
                 nSrcGeomFieldCount == 1 &&
                 m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) &&
                 !poSrcFDefn->GetGeomFieldDefn(0)->IsNullable() &&
                 !m_bForceNullable)
        {
            anRequestedGeomFields.push_back(0);
            eGCreateLayerType = wkbNone;
        }
        // If the source feature first geometry column is not nullable
        // and that GEOMETRY_NULLABLE creation option is available, use it
        // so as to be able to set the not null constraint (if the driver supports it)
        else if( anRequestedGeomFields.empty() &&
                 nSrcGeomFieldCount >= 1 &&
                 !poSrcFDefn->GetGeomFieldDefn(0)->IsNullable() &&
                 m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST) != NULL &&
                 strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "GEOMETRY_NULLABLE") != NULL &&
                 CSLFetchNameValue(m_papszLCO, "GEOMETRY_NULLABLE") == NULL&&
                 !m_bForceNullable )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "GEOMETRY_NULLABLE", "NO");
            CPLDebug("GDALVectorTranslate", "Using GEOMETRY_NULLABLE=NO");
        }
        // Special case for conversion from GMLAS driver to ensure that
        // source geometry field name will be used as much as possible
        // FIXME: why not make this general behaviour ?
        else if( anRequestedGeomFields.empty() &&
                 nSrcGeomFieldCount == 1 &&
                 m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) &&
                 m_poSrcDS != NULL &&
                 m_poSrcDS->GetDriver() != NULL &&
                 EQUAL(m_poSrcDS->GetDriver()->GetDescription(), "GMLAS") )
        {
            anRequestedGeomFields.push_back(0);
            eGCreateLayerType = wkbNone;
        }

        // Force FID column as 64 bit if the source feature has a 64 bit FID,
        // the target driver supports 64 bit FID and the user didn't set it
        // manually.
        if( poSrcLayer->GetMetadataItem(OLMD_FID64) != NULL &&
            EQUAL(poSrcLayer->GetMetadataItem(OLMD_FID64), "YES") &&
            m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST) != NULL &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "FID64") != NULL &&
            CSLFetchNameValue(m_papszLCO, "FID64") == NULL )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "FID64", "YES");
            CPLDebug("GDALVectorTranslate", "Using FID64=YES");
        }

        // If output driver supports FID layer creation option, set it with
        // the FID column name of the source layer
        if( !m_bUnsetFid && !bAppend &&
            poSrcLayer->GetFIDColumn() != NULL &&
            !EQUAL(poSrcLayer->GetFIDColumn(), "") &&
            m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST) != NULL &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "='FID'") != NULL &&
            CSLFetchNameValue(m_papszLCO, "FID") == NULL )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "FID", poSrcLayer->GetFIDColumn());
            CPLDebug("GDALVectorTranslate", "Using FID=%s and -preserve_fid", poSrcLayer->GetFIDColumn());
            bPreserveFID = true;
        }

        if( m_bNativeData &&
            poSrcLayer->GetMetadataItem("NATIVE_DATA", "NATIVE_DATA") != NULL &&
            poSrcLayer->GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA") != NULL &&
            m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST) != NULL &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "NATIVE_DATA") != NULL &&
            strstr(m_poDstDS->GetDriver()->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST), "NATIVE_MEDIA_TYPE") != NULL )
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "NATIVE_DATA",
                    poSrcLayer->GetMetadataItem("NATIVE_DATA", "NATIVE_DATA"));
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "NATIVE_MEDIA_TYPE",
                    poSrcLayer->GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA"));
            CPLDebug("GDALVectorTranslate", "Transferring layer NATIVE_DATA");
        }

        OGRSpatialReference* poOutputSRSClone = NULL;
        if( poOutputSRS != NULL )
        {
            poOutputSRSClone = poOutputSRS->Clone();
        }
        poDstLayer = m_poDstDS->CreateLayer( pszNewLayerName, poOutputSRSClone,
                                           (OGRwkbGeometryType) eGCreateLayerType,
                                           papszLCOTemp );
        CSLDestroy(papszLCOTemp);

        if( poOutputSRSClone != NULL )
        {
            poOutputSRSClone->Release();
        }

        if( poDstLayer == NULL )
        {
            return NULL;
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
                if( m_poOutputSRS != NULL )
                {
                    poOutputSRSClone = m_poOutputSRS->Clone();
                    oGFldDefn.SetSpatialRef(poOutputSRSClone);
                    poOutputSRSClone->Release();
                }
                if( bForceGType )
                    oGFldDefn.SetType((OGRwkbGeometryType) eGType);
                else
                {
                    eGType = oGFldDefn.GetType();
                    eGType = ConvertType(m_eGeomTypeConversion, (OGRwkbGeometryType)eGType);
                    eGType = ForceCoordDimension(eGType, m_nCoordDim);
                    oGFldDefn.SetType((OGRwkbGeometryType) eGType);
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
        return NULL;
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
    int         nSrcFieldCount = poSrcFDefn->GetFieldCount();
    int         iSrcFIDField = -1;

    // Initialize the index-to-index map to -1's
    int *panMap = static_cast<int *>(VSIMalloc(sizeof(int) * nSrcFieldCount));
    for( int iField=0; iField < nSrcFieldCount; iField++)
        panMap[iField] = -1;

    /* Caution : at the time of writing, the MapInfo driver */
    /* returns NULL until a field has been added */
    poDstFDefn = poDstLayer->GetLayerDefn();

    if (m_papszFieldMap && bAppend)
    {
        bool bIdentity = false;

        if (EQUAL(m_papszFieldMap[0], "identity"))
            bIdentity = true;
        else if (CSLCount(m_papszFieldMap) != nSrcFieldCount)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Field map should contain the value 'identity' or "
                    "the same number of integer values as the source field count.");
            VSIFree(panMap);
            return NULL;
        }

        for( int iField=0; iField < nSrcFieldCount; iField++)
        {
            panMap[iField] = bIdentity? iField : atoi(m_papszFieldMap[iField]);
            if (panMap[iField] >= poDstFDefn->GetFieldCount())
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Invalid destination field index %d.", panMap[iField]);
                VSIFree(panMap);
                return NULL;
            }
        }
    }
    else if (m_papszSelFields && !bAppend )
    {
        int  nDstFieldCount = 0;
        if (poDstFDefn)
            nDstFieldCount = poDstFDefn->GetFieldCount();
        for( int iField=0; m_papszSelFields[iField] != NULL; iField++)
        {
            int iSrcField = poSrcFDefn->GetFieldIndex(m_papszSelFields[iField]);
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
                int iDstField = -1;
                if (poDstFDefn)
                    iDstField = poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef());
                if (iDstField >= 0)
                {
                    panMap[iSrcField] = iDstField;
                }
                else if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
                {
                    /* now that we've created a field, GetLayerDefn() won't return NULL */
                    if (poDstFDefn == NULL)
                        poDstFDefn = poDstLayer->GetLayerDefn();

                    /* Sanity check : if it fails, the driver is buggy */
                    if (poDstFDefn != NULL &&
                        poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "The output driver has claimed to have added the %s field, but it did not!",
                                 oFieldDefn.GetNameRef() );
                    }
                    else
                    {
                        panMap[iSrcField] = nDstFieldCount;
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
            int iSrcField;
            char** papszIgnoredFields = NULL;
            bool bUseIgnoredFields = true;
            char** papszWHEREUsedFields = NULL;

            if (m_pszWHERE)
            {
                /* We must not ignore fields used in the -where expression (#4015) */
                OGRFeatureQuery oFeatureQuery;
                if ( oFeatureQuery.Compile( poSrcLayer->GetLayerDefn(), m_pszWHERE, FALSE, NULL ) == OGRERR_NONE )
                {
                    papszWHEREUsedFields = oFeatureQuery.GetUsedFields();
                }
                else
                {
                    bUseIgnoredFields = false;
                }
            }

            for(iSrcField=0;bUseIgnoredFields && iSrcField<poSrcFDefn->GetFieldCount();iSrcField++)
            {
                const char* pszFieldName =
                    poSrcFDefn->GetFieldDefn(iSrcField)->GetNameRef();
                bool bFieldRequested = false;
                for( int iField=0; m_papszSelFields[iField] != NULL; iField++)
                {
                    if (EQUAL(pszFieldName, m_papszSelFields[iField]))
                    {
                        bFieldRequested = true;
                        break;
                    }
                }
                bFieldRequested |= CSLFindString(papszWHEREUsedFields, pszFieldName) >= 0;
                bFieldRequested |= (m_pszZField != NULL && EQUAL(pszFieldName, m_pszZField));

                /* If source field not requested, add it to ignored files list */
                if (!bFieldRequested)
                    papszIgnoredFields = CSLAddString(papszIgnoredFields, pszFieldName);
            }
            if (bUseIgnoredFields)
                poSrcLayer->SetIgnoredFields((const char**)papszIgnoredFields);
            CSLDestroy(papszIgnoredFields);
            CSLDestroy(papszWHEREUsedFields);
        }
    }
    else if( !bAppend || m_bAddMissingFields )
    {
        int nDstFieldCount = 0;
        if (poDstFDefn)
            nDstFieldCount = poDstFDefn->GetFieldCount();

        /* Save the map of existing fields, before creating new ones */
        /* This helps when converting a source layer that has duplicated field names */
        /* which is a bad idea */
        std::map<CPLString, int> oMapExistingFields;
        for( int iField = 0; iField < nDstFieldCount; iField++ )
        {
            const char* pszFieldName = poDstFDefn->GetFieldDefn(iField)->GetNameRef();
            CPLString osUpperFieldName(CPLString(pszFieldName).toupper());
            if( oMapExistingFields.find(osUpperFieldName) == oMapExistingFields.end() )
                oMapExistingFields[osUpperFieldName] = iField;
            /*else
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The target layer has already a duplicated field name '%s' before "
                         "adding the fields of the source layer", pszFieldName); */
        }

        const char* pszFIDColumn = poDstLayer->GetFIDColumn();

        for( int iField = 0; iField < nSrcFieldCount; iField++ )
        {
            OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iField);
            OGRFieldDefn oFieldDefn( poSrcFieldDefn );

            // Avoid creating a field with the same name as the FID column
            if( pszFIDColumn != NULL && EQUAL(pszFIDColumn, oFieldDefn.GetNameRef()) &&
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
            std::map<CPLString, int>::iterator oIter =
                oMapExistingFields.find(CPLString(oFieldDefn.GetNameRef()).toupper());
            if( oIter != oMapExistingFields.end() )
            {
                panMap[iField] = oIter->second;
                continue;
            }

            bool bHasRenamed = false;
            /* In case the field name already exists in the target layer, */
            /* build a unique field name */
            if( poDstFDefn != NULL &&
                poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef()) >= 0 )
            {
                int nTry = 1;
                while( true )
                {
                    ++nTry;
                    CPLString osTmpName;
                    osTmpName.Printf("%s%d", oFieldDefn.GetNameRef(), nTry);
                    /* Check that the proposed name doesn't exist either in the already */
                    /* created fields or in the source fields */
                    if( poDstFDefn->GetFieldIndex(osTmpName) < 0 &&
                        poSrcFDefn->GetFieldIndex(osTmpName) < 0 )
                    {
                        bHasRenamed = true;
                        oFieldDefn.SetName(osTmpName);
                        break;
                    }
                }
            }

            if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
            {
                /* now that we've created a field, GetLayerDefn() won't return NULL */
                if (poDstFDefn == NULL)
                    poDstFDefn = poDstLayer->GetLayerDefn();

                /* Sanity check : if it fails, the driver is buggy */
                if (poDstFDefn != NULL &&
                    poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The output driver has claimed to have added the %s field, but it did not!",
                             oFieldDefn.GetNameRef() );
                }
                else
                {
                    if( bHasRenamed && poDstFDefn != NULL )
                    {
                        const char* pszNewFieldName =
                            poDstFDefn->GetFieldDefn(nDstFieldCount)->GetNameRef();
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Field '%s' already exists. Renaming it as '%s'",
                                 poSrcFieldDefn->GetNameRef(), pszNewFieldName);
                    }

                    panMap[iField] = nDstFieldCount;
                    nDstFieldCount ++;
                }
            }
        }
    }
    else
    {
        /* For an existing layer, build the map by fetching the index in the destination */
        /* layer for each source field */
        if (poDstFDefn == NULL)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "poDstFDefn == NULL." );
            VSIFree(panMap);
            return NULL;
        }

        for( int iField = 0; iField < nSrcFieldCount; iField++ )
        {
            OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iField);
            int iDstField = poDstLayer->FindFieldIndex(poSrcFieldDefn->GetNameRef(), m_bExactFieldNameMatch);
            if (iDstField >= 0)
                panMap[iField] = iDstField;
            else
                CPLDebug("GDALVectorTranslate", "Skipping field '%s' not found in destination layer '%s'.",
                         poSrcFieldDefn->GetNameRef(), poDstLayer->GetName() );
        }
    }

    if( bOverwriteActuallyDone &&
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
            VSIFree(panMap);
            return NULL;
        }
        nTotalEventsDone = 0;
    }

    TargetLayerInfo* psInfo = static_cast<TargetLayerInfo *>(
        CPLMalloc(sizeof(TargetLayerInfo)));
    psInfo->nFeaturesRead = 0;
    psInfo->bPerFeatureCT = false;
    psInfo->poSrcLayer = poSrcLayer;
    psInfo->poDstLayer = poDstLayer;
    psInfo->papoCT = static_cast<OGRCoordinateTransformation **>(
        CPLCalloc(poDstLayer->GetLayerDefn()->GetGeomFieldCount(),
                  sizeof(OGRCoordinateTransformation*)));
    psInfo->papapszTransformOptions = static_cast<char ***>(
        CPLCalloc(poDstLayer->GetLayerDefn()->GetGeomFieldCount(),
                  sizeof(char**)));
    psInfo->panMap = panMap;
    psInfo->iSrcZField = iSrcZField;
    psInfo->iSrcFIDField = iSrcFIDField;
    if( anRequestedGeomFields.size() == 1 )
        psInfo->iRequestedSrcGeomField = anRequestedGeomFields[0];
    else
        psInfo->iRequestedSrcGeomField = -1;
    psInfo->bPreserveFID = bPreserveFID;

    return psInfo;
}

/************************************************************************/
/*                         FreeTargetLayerInfo()                        */
/************************************************************************/

static void FreeTargetLayerInfo(TargetLayerInfo* psInfo)
{
    if( psInfo == NULL )
        return;
    for(int i=0;i<psInfo->poDstLayer->GetLayerDefn()->GetGeomFieldCount();i++)
    {
        delete psInfo->papoCT[i];
        CSLDestroy(psInfo->papapszTransformOptions[i]);
    }
    CPLFree(psInfo->papoCT);
    CPLFree(psInfo->papapszTransformOptions);
    CPLFree(psInfo->panMap);
    CPLFree(psInfo);
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
    OGRLayer    *poDstLayer = psInfo->poDstLayer;
    int nDstGeomFieldCount = poDstLayer->GetLayerDefn()->GetGeomFieldCount();
    for( int iGeom = 0; iGeom < nDstGeomFieldCount; iGeom ++ )
    {
/* -------------------------------------------------------------------- */
/*      Setup coordinate transformation if we need it.                  */
/* -------------------------------------------------------------------- */
        OGRSpatialReference* poSourceSRS = NULL;
        OGRCoordinateTransformation* poCT = NULL;
        char** papszTransformOptions = NULL;

        int iSrcGeomField;
        if( psInfo->iRequestedSrcGeomField >= 0 )
            iSrcGeomField = psInfo->iRequestedSrcGeomField;
        else
        {
            iSrcGeomField = poSrcLayer->GetLayerDefn()->GetGeomFieldIndex(
                poDstLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom)->GetNameRef());
            if( iSrcGeomField < 0 )
            {
                if( nDstGeomFieldCount == 1 &&
                    poSrcLayer->GetLayerDefn()->GetGeomFieldCount() > 0 )
                {
                    iSrcGeomField = 0;
                }
                else
                    continue;
            }
        }

        if( bTransform || bWrapDateline )
        {
            if( psInfo->nFeaturesRead == 0 )
            {
                poSourceSRS = poUserSourceSRS;
                if( poSourceSRS == NULL )
                {
                    if( iSrcGeomField > 0 )
                        poSourceSRS = poSrcLayer->GetLayerDefn()->
                            GetGeomFieldDefn(iSrcGeomField)->GetSpatialRef();
                    else
                        poSourceSRS = poSrcLayer->GetSpatialRef();
                }
            }
            if( poSourceSRS == NULL )
            {
                OGRGeometry* poSrcGeometry =
                    poFeature->GetGeomFieldRef(iSrcGeomField);
                if( poSrcGeometry )
                    poSourceSRS = poSrcGeometry->getSpatialReference();
                psInfo->bPerFeatureCT = true;
            }
        }

        if( bTransform )
        {
            if( poSourceSRS == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Can't transform coordinates, source layer has no\n"
                        "coordinate system.  Use -s_srs to set one." );

                return false;
            }

            CPLAssert( NULL != poSourceSRS );
            CPLAssert( NULL != poOutputSRS );

            if( psInfo->papoCT[iGeom] != NULL &&
                psInfo->papoCT[iGeom]->GetSourceCS() == poSourceSRS )
            {
                poCT = psInfo->papoCT[iGeom];
            }
            else
            {
                poCT = OGRCreateCoordinateTransformation( poSourceSRS, poOutputSRS );
                if( poCT == NULL )
                {
                    char        *pszWKT = NULL;

                    CPLError( CE_Failure, CPLE_AppDefined, "Failed to create coordinate transformation between the\n"
                        "following coordinate systems.  This may be because they\n"
                        "are not transformable, or because projection services\n"
                        "(PROJ.4 DLL/.so) could not be loaded." );

                    poSourceSRS->exportToPrettyWkt( &pszWKT, FALSE );
                    CPLError( CE_Failure, CPLE_AppDefined,  "Source:\n%s", pszWKT );
                    CPLFree(pszWKT);

                    poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
                    CPLError( CE_Failure, CPLE_AppDefined,  "Target:\n%s", pszWKT );
                    CPLFree(pszWKT);

                    return false;
                }
                if( poGCPCoordTrans != NULL )
                    poCT = new CompositeCT( poGCPCoordTrans, poCT );
            }

            if( poCT != psInfo->papoCT[iGeom] )
            {
                delete psInfo->papoCT[iGeom];
                psInfo->papoCT[iGeom] = poCT;
            }
        }
        else
        {
            poCT = poGCPCoordTrans;
        }

        if (bWrapDateline)
        {
            if (bTransform && poCT != NULL && poOutputSRS != NULL && poOutputSRS->IsGeographic())
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
            else if (poSourceSRS != NULL && poSourceSRS->IsGeographic())
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

            CSLDestroy(psInfo->papapszTransformOptions[iGeom]);
            psInfo->papapszTransformOptions[iGeom] = papszTransformOptions;
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
    OGRLayer    *poSrcLayer;
    OGRLayer    *poDstLayer;
    const int         eGType = m_eGType;
    OGRSpatialReference* poOutputSRS = m_poOutputSRS;

    poSrcLayer = psInfo->poSrcLayer;
    poDstLayer = psInfo->poDstLayer;
    int* const panMap = psInfo->panMap;
    const int iSrcZField = psInfo->iSrcZField;
    const bool bPreserveFID = psInfo->bPreserveFID;
    const int nSrcGeomFieldCount = poSrcLayer->GetLayerDefn()->GetGeomFieldCount();
    const int nDstGeomFieldCount = poDstLayer->GetLayerDefn()->GetGeomFieldCount();
    const bool bExplodeCollections = m_bExplodeCollections && nDstGeomFieldCount <= 1;

    if( poOutputSRS == NULL && !m_bNullifyOutputSRS )
    {
        if( nSrcGeomFieldCount == 1 )
        {
            poOutputSRS = poSrcLayer->GetSpatialRef();
        }
        else if( psInfo->iRequestedSrcGeomField > 0 )
        {
            poOutputSRS = poSrcLayer->GetLayerDefn()->GetGeomFieldDefn(
                psInfo->iRequestedSrcGeomField)->GetSpatialRef();
        }
    }

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;
    int         nFeaturesInTransaction = 0;
    GIntBig      nCount = 0; /* written + failed */
    GIntBig      nFeaturesWritten = 0;

    if( psOptions->nGroupTransactions )
    {
        if( psOptions->nLayerTransaction )
        {
            if( poDstLayer->StartTransaction() == OGRERR_FAILURE )
                return false;
        }
    }

    bool bRet = true;
    while( true )
    {
        if( m_nLimit >= 0 && psInfo->nFeaturesRead >= m_nLimit )
        {
            break;
        }

        OGRFeature      *poDstFeature = NULL;

        if( poFeatureIn != NULL )
            poFeature = poFeatureIn;
        else if( psOptions->nFIDToFetch != OGRNullFID )
            poFeature = poSrcLayer->GetFeature(psOptions->nFIDToFetch);
        else
            poFeature = poSrcLayer->GetNextFeature();

        if( poFeature == NULL )
            break;

        if( psInfo->nFeaturesRead == 0 || psInfo->bPerFeatureCT )
        {
            if( !SetupCT( psInfo, poSrcLayer, m_bTransform, m_bWrapDateline,
                          m_osDateLineOffset, m_poUserSourceSRS,
                          poFeature, poOutputSRS, m_poGCPCoordTrans) )
            {
                OGRFeature::DestroyFeature( poFeature );
                return false;
            }
        }

        psInfo->nFeaturesRead ++;

        int nParts = 0;
        int nIters = 1;
        if (bExplodeCollections)
        {
            OGRGeometry* poSrcGeometry;
            if( psInfo->iRequestedSrcGeomField >= 0 )
                poSrcGeometry = poFeature->GetGeomFieldRef(
                                        psInfo->iRequestedSrcGeomField);
            else
                poSrcGeometry = poFeature->GetGeometryRef();
            if (poSrcGeometry &&
                OGR_GT_IsSubClassOf(poSrcGeometry->getGeometryType(), wkbGeometryCollection) )
            {
                nParts = ((OGRGeometryCollection*)poSrcGeometry)->getNumGeometries();
                nIters = nParts;
                if (nIters == 0)
                    nIters = 1;
            }
        }

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
            OGRGeometry* poStolenGeometry = NULL;
            if( !bExplodeCollections && nSrcGeomFieldCount == 1 &&
                nDstGeomFieldCount == 1 )
            {
                poStolenGeometry = poFeature->StealGeometry();
            }
            else if( !bExplodeCollections &&
                     psInfo->iRequestedSrcGeomField >= 0 )
            {
                poStolenGeometry = poFeature->StealGeometry(
                    psInfo->iRequestedSrcGeomField);
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

            /* ... and now we can attach the stolen geometry */
            if( poStolenGeometry )
            {
                poDstFeature->SetGeometryDirectly(poStolenGeometry);
            }

            if( bPreserveFID )
                poDstFeature->SetFID( poFeature->GetFID() );
            else if( psInfo->iSrcFIDField >= 0 &&
                     poFeature->IsFieldSetAndNotNull(psInfo->iSrcFIDField))
                poDstFeature->SetFID( poFeature->GetFieldAsInteger64(psInfo->iSrcFIDField) );

            /* Erase native data if asked explicitly */
            if( !m_bNativeData )
            {
                poDstFeature->SetNativeData(NULL);
                poDstFeature->SetNativeMediaType(NULL);
            }

            for( int iGeom = 0; iGeom < nDstGeomFieldCount; iGeom ++ )
            {
                OGRGeometry* poDstGeometry = poDstFeature->StealGeometry(iGeom);
                if (poDstGeometry == NULL)
                    continue;

                if (nParts > 0)
                {
                    /* For -explodecollections, extract the iPart(th) of the geometry */
                    OGRGeometry* poPart = ((OGRGeometryCollection*)poDstGeometry)->getGeometryRef(iPart);
                    ((OGRGeometryCollection*)poDstGeometry)->removeGeometry(iPart, FALSE);
                    delete poDstGeometry;
                    poDstGeometry = poPart;
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
                    poDstGeometry->setCoordinateDimension( m_nCoordDim );
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
                    if (poClipped == NULL || poClipped->IsEmpty())
                    {
                        delete poClipped;
                        goto end_loop;
                    }
                    poDstGeometry = poClipped;
                }

                OGRCoordinateTransformation* poCT = psInfo->papoCT[iGeom];
                if( !m_bTransform )
                    poCT = m_poGCPCoordTrans;
                char** papszTransformOptions = psInfo->papapszTransformOptions[iGeom];

                if( poCT != NULL || papszTransformOptions != NULL)
                {
                    OGRGeometry* poReprojectedGeom =
                        OGRGeometryFactory::transformWithOptions(poDstGeometry, poCT, papszTransformOptions);
                    if( poReprojectedGeom == NULL )
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
                else if (poOutputSRS != NULL)
                {
                    poDstGeometry->assignSpatialReference(poOutputSRS);
                }

                if (m_poClipDst)
                {
                    if( poDstGeometry == NULL )
                        goto end_loop;

                    OGRGeometry* poClipped = poDstGeometry->Intersection(m_poClipDst);
                    delete poDstGeometry;
                    if (poClipped == NULL || poClipped->IsEmpty())
                    {
                        delete poClipped;
                        goto end_loop;
                    }

                    poDstGeometry = poClipped;
                }

                if( eGType != GEOMTYPE_UNCHANGED )
                {
                    poDstGeometry = OGRGeometryFactory::forceTo(
                            poDstGeometry, (OGRwkbGeometryType)eGType);
                }
                else if( m_eGeomTypeConversion == GTC_PROMOTE_TO_MULTI ||
                         m_eGeomTypeConversion == GTC_CONVERT_TO_LINEAR ||
                         m_eGeomTypeConversion == GTC_CONVERT_TO_CURVE )
                {
                    if( poDstGeometry != NULL )
                    {
                        OGRwkbGeometryType eTargetType = poDstGeometry->getGeometryType();
                        eTargetType = ConvertType(m_eGeomTypeConversion, eTargetType);
                        poDstGeometry = OGRGeometryFactory::forceTo(poDstGeometry, eTargetType);
                    }
                }

                poDstFeature->SetGeomFieldDirectly(iGeom, poDstGeometry);
            }

            CPLErrorReset();
            if( poDstLayer->CreateFeature( poDstFeature ) == OGRERR_NONE )
            {
                nFeaturesWritten ++;
                if( (bPreserveFID && poDstFeature->GetFID() != poFeature->GetFID()) ||
                    (!bPreserveFID && psInfo->iSrcFIDField >= 0 && poFeature->IsFieldSetAndNotNull(psInfo->iSrcFIDField) &&
                     poDstFeature->GetFID() != poFeature->GetFieldAsInteger64(psInfo->iSrcFIDField)) )
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
        if( poFeatureIn != NULL )
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

    if( poFeatureIn == NULL )
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
        memmove(pabyData, pabyData + 3, strlen((const char*)pabyData + 3) + 1);
    }
}

/************************************************************************/
/*                       GDALVectorTranslateOptionsNew()                */
/************************************************************************/

/**
 * allocates a GDALVectorTranslateOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="ogr2ogr.html">ogr2ogr</a> utility.
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
    psOptions->nGroupTransactions = 20000;
    psOptions->nFIDToFetch = OGRNullFID;
    psOptions->bQuiet = false;
    psOptions->pszFormat = CPLStrdup("ESRI Shapefile");
    psOptions->papszLayers = NULL;
    psOptions->papszDSCO = NULL;
    psOptions->papszLCO = NULL;
    psOptions->bTransform = false;
    psOptions->bAddMissingFields = false;
    psOptions->pszOutputSRSDef = NULL;
    psOptions->pszSourceSRSDef = NULL;
    psOptions->bNullifyOutputSRS = false;
    psOptions->bExactFieldNameMatch = true;
    psOptions->pszNewLayerName = NULL;
    psOptions->pszWHERE = NULL;
    psOptions->pszGeomField = NULL;
    psOptions->papszSelFields = NULL;
    psOptions->pszSQLStatement = NULL;
    psOptions->pszDialect = NULL;
    psOptions->eGType = GEOMTYPE_UNCHANGED;
    psOptions->eGeomTypeConversion = GTC_DEFAULT;
    psOptions->eGeomOp = GEOMOP_NONE;
    psOptions->dfGeomOpParam = 0;
    psOptions->papszFieldTypesToString = NULL;
    psOptions->papszMapFieldType = NULL;
    psOptions->bUnsetFieldWidth = false;
    psOptions->bDisplayProgress = false;
    psOptions->bWrapDateline = false;
    psOptions->dfDateLineOffset = 10.0;
    psOptions->bClipSrc = false;
    psOptions->hClipSrc = NULL;
    psOptions->pszClipSrcDS = NULL;
    psOptions->pszClipSrcSQL = NULL;
    psOptions->pszClipSrcLayer = NULL;
    psOptions->pszClipSrcWhere = NULL;
    psOptions->hClipDst = NULL;
    psOptions->pszClipDstDS = NULL;
    psOptions->pszClipDstSQL = NULL;
    psOptions->pszClipDstLayer = NULL;
    psOptions->pszClipDstWhere = NULL;
    psOptions->bSplitListFields = false;
    psOptions->nMaxSplitListSubFields = -1;
    psOptions->bExplodeCollections = false;
    psOptions->pszZField = NULL;
    psOptions->papszFieldMap = NULL;
    psOptions->nCoordDim = COORD_DIM_UNCHANGED;
    psOptions->papszDestOpenOptions = NULL;
    psOptions->bForceNullable = false;
    psOptions->bUnsetDefault = false;
    psOptions->bUnsetFid = false;
    psOptions->bPreserveFID = false;
    psOptions->bCopyMD = true;
    psOptions->papszMetadataOptions = NULL;
    psOptions->pszSpatSRSDef = NULL;
    psOptions->nGCPCount = 0;
    psOptions->pasGCPs = NULL;
    psOptions->nTransformOrder = 0;  /* Default to 0 for now... let the lib decide */
    psOptions->hSpatialFilter = NULL;
    psOptions->bNativeData = true;
    psOptions->nLimit = -1;

    int nArgc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != NULL && i < nArgc; i++ )
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
            if( psOptionsForBinary )
            {
                psOptionsForBinary->bFormatExplicitlySet = TRUE;
            }
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
            GByte* pabyRet = NULL;
            if( papszArgv[i][0] == '@' &&
                VSIIngestFile( NULL, papszArgv[i] + 1, &pabyRet, NULL, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                psOptions->pszSQLStatement = (char*)pabyRet;
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
                psOptions->eGeomTypeConversion = GTC_PROMOTE_TO_MULTI;
            else if( EQUAL(osGeomName,"CONVERT_TO_LINEAR") )
                psOptions->eGeomTypeConversion = GTC_CONVERT_TO_LINEAR;
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
                    return NULL;
                }
            }
            if (psOptions->eGType != GEOMTYPE_UNCHANGED && psOptions->eGType != wkbNone && bIs3D)
                psOptions->eGType = wkbSetZ((OGRwkbGeometryType)psOptions->eGType);

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
                return NULL;
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
        /* Undocumented. Just a provision. Default behaviour should be OK */
        else if ( EQUAL(papszArgv[i],"-lyr_transaction") )
        {
            psOptions->nLayerTransaction = TRUE;
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-s_srs") )
        {
            CPLFree(psOptions->pszSourceSRSDef);
            psOptions->pszSourceSRSDef = CPLStrdup(papszArgv[++i]);
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-a_srs") )
        {
            CPLFree(psOptions->pszOutputSRSDef);
            psOptions->pszOutputSRSDef = CPLStrdup(papszArgv[++i]);
            if (EQUAL(psOptions->pszOutputSRSDef, "NULL") ||
                EQUAL(psOptions->pszOutputSRSDef, "NONE"))
            {
                psOptions->pszOutputSRSDef = NULL;
                psOptions->bNullifyOutputSRS = true;
            }
        }
        else if( i+1 < nArgc && EQUAL(papszArgv[i],"-t_srs") )
        {
            CPLFree(psOptions->pszOutputSRSDef);
            psOptions->pszOutputSRSDef = CPLStrdup(papszArgv[++i]);
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

            OGRPolygon* poSpatialFilter = (OGRPolygon*) OGRGeometryFactory::createGeometry(wkbPolygon);
            poSpatialFilter->addRing( &oRing );
            OGR_G_DestroyGeometry(psOptions->hSpatialFilter);
            psOptions->hSpatialFilter = (OGRGeometryH) poSpatialFilter;
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
            GByte* pabyRet = NULL;
            if( papszArgv[i][0] == '@' &&
                VSIIngestFile( NULL, papszArgv[i] + 1, &pabyRet, NULL, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                psOptions->pszWHERE = (char*)pabyRet;
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
                    psOptions->papszFieldTypesToString = NULL;
                    psOptions->papszFieldTypesToString = CSLAddString(psOptions->papszFieldTypesToString, "All");
                    break;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Unhandled type for fieldTypeToString option : %s",
                            *iter);
                    GDALVectorTranslateOptionsFree(psOptions);
                    return NULL;
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
                char* pszKey = NULL;
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
                        return NULL;
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
                return NULL;
            }

            VSIStatBufL  sStat;
            psOptions->bClipSrc = true;
            if ( IsNumber(papszArgv[i+1])
                 && papszArgv[i+2] != NULL
                 && papszArgv[i+3] != NULL
                 && papszArgv[i+4] != NULL)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

                OGR_G_DestroyGeometry(psOptions->hClipSrc);
                psOptions->hClipSrc = (OGRGeometryH) OGRGeometryFactory::createGeometry(wkbPolygon);
                ((OGRPolygon *) psOptions->hClipSrc)->addRing( &oRing );
                i += 4;
            }
            else if ((STARTS_WITH_CI(papszArgv[i+1], "POLYGON") ||
                      STARTS_WITH_CI(papszArgv[i+1], "MULTIPOLYGON")) &&
                      VSIStatL(papszArgv[i+1], &sStat) != 0)
            {
                char* pszTmp = (char*) papszArgv[i+1];
                OGR_G_DestroyGeometry(psOptions->hClipSrc);
                OGRGeometryFactory::createFromWkt(&pszTmp, NULL, (OGRGeometry **)&psOptions->hClipSrc);
                if (psOptions->hClipSrc == NULL)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                    GDALVectorTranslateOptionsFree(psOptions);
                    return NULL;
                }
                i ++;
            }
            else if (EQUAL(papszArgv[i+1], "spat_extent") )
            {
                i ++;
            }
            else
            {
                CPLFree(psOptions->pszClipSrcDS);
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
                return NULL;
            }

            VSIStatBufL  sStat;
            if ( IsNumber(papszArgv[i+1])
                 && papszArgv[i+2] != NULL
                 && papszArgv[i+3] != NULL
                 && papszArgv[i+4] != NULL)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+4]) );
                oRing.addPoint( CPLAtof(papszArgv[i+3]), CPLAtof(papszArgv[i+2]) );
                oRing.addPoint( CPLAtof(papszArgv[i+1]), CPLAtof(papszArgv[i+2]) );

                OGR_G_DestroyGeometry(psOptions->hClipDst);
                psOptions->hClipDst = (OGRGeometryH) OGRGeometryFactory::createGeometry(wkbPolygon);
                ((OGRPolygon *) psOptions->hClipDst)->addRing( &oRing );
                i += 4;
            }
            else if ((STARTS_WITH_CI(papszArgv[i+1], "POLYGON") ||
                      STARTS_WITH_CI(papszArgv[i+1], "MULTIPOLYGON")) &&
                      VSIStatL(papszArgv[i+1], &sStat) != 0)
            {
                char* pszTmp = (char*) papszArgv[i+1];
                OGR_G_DestroyGeometry(psOptions->hClipDst);
                OGRGeometryFactory::createFromWkt(&pszTmp, NULL, (OGRGeometry **)&psOptions->hClipDst);
                if (psOptions->hClipDst == NULL)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                    GDALVectorTranslateOptionsFree(psOptions);
                    return NULL;
                }
                i ++;
            }
            else
            {
                CPLFree(psOptions->pszClipDstDS);
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
            char* endptr = NULL;
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
            if( papszArgv[i+1] != NULL
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
        else if( EQUAL(papszArgv[i],"-forceNullable") )
        {
            psOptions->bForceNullable = true;
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
            return NULL;
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszDestDataSource == NULL )
            psOptionsForBinary->pszDestDataSource = CPLStrdup(papszArgv[i]);
        else if(  psOptionsForBinary && psOptionsForBinary->pszDataSource == NULL )
            psOptionsForBinary->pszDataSource = CPLStrdup(papszArgv[i]);
        else
            psOptions->papszLayers = CSLAddString( psOptions->papszLayers, papszArgv[i] );
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);
        psOptionsForBinary->eAccessMode = psOptions->eAccessMode;

        if( !(CPLTestBool(CSLFetchNameValueDef(
                psOptionsForBinary->papszOpenOptions, "NATIVE_DATA",
                CSLFetchNameValueDef(
                    psOptionsForBinary->papszOpenOptions, "@NATIVE_DATA", "TRUE")))) )
        {
            psOptions->bNativeData = false;
        }

        if( psOptions->bNativeData &&
            CSLFetchNameValue(psOptionsForBinary->papszOpenOptions,
                              "NATIVE_DATA") == NULL &&
            CSLFetchNameValue(psOptionsForBinary->papszOpenOptions,
                              "@NATIVE_DATA") == NULL )
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
    if( psOptions == NULL )
        return;

    CPLFree( psOptions->pszFormat );
    CPLFree( psOptions->pszOutputSRSDef);
    CPLFree( psOptions->pszSourceSRSDef);
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

    if( psOptions->pasGCPs != NULL )
    {
        GDALDeinitGCPs( psOptions->nGCPCount, psOptions->pasGCPs );
        CPLFree( psOptions->pasGCPs );
    }

    if( psOptions->hClipSrc != NULL )
        OGR_G_DestroyGeometry( psOptions->hClipSrc );
    if( psOptions->hClipDst != NULL )
        OGR_G_DestroyGeometry( psOptions->hClipDst );
    if( psOptions->hSpatialFilter != NULL )
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
