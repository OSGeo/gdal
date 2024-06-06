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
#include "gdalargumentparser.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <limits>
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
#include "cpl_time.h"
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
#include "ogr_recordbatch.h"
#include "ogr_spatialref.h"
#include "ogrlayerdecorator.h"
#include "ogrsf_frmts.h"

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

#define GEOMTYPE_UNCHANGED -2

#define COORD_DIM_UNCHANGED -1
#define COORD_DIM_LAYER_DIM -2
#define COORD_DIM_XYM -3

#define TZ_OFFSET_INVALID INT_MIN

/************************************************************************/
/*                              CopyableGCPs                            */
/************************************************************************/

namespace gdal::ogr2ogr_lib
{
struct CopyableGCPs
{
    /*! size of the list pasGCPs */
    int nGCPCount = 0;

    /*! list of ground control points to be added */
    GDAL_GCP *pasGCPs = nullptr;

    CopyableGCPs() = default;

    CopyableGCPs(const CopyableGCPs &other)
    {
        nGCPCount = other.nGCPCount;
        if (other.nGCPCount)
            pasGCPs = GDALDuplicateGCPs(other.nGCPCount, other.pasGCPs);
        else
            pasGCPs = nullptr;
    }

    ~CopyableGCPs()
    {
        if (pasGCPs)
        {
            GDALDeinitGCPs(nGCPCount, pasGCPs);
            CPLFree(pasGCPs);
        }
    }
};
}  // namespace gdal::ogr2ogr_lib

using namespace gdal::ogr2ogr_lib;

/************************************************************************/
/*                        GDALVectorTranslateOptions                    */
/************************************************************************/

/** Options for use with GDALVectorTranslate(). GDALVectorTranslateOptions* must
 * be allocated and freed with GDALVectorTranslateOptionsNew() and
 * GDALVectorTranslateOptionsFree() respectively.
 */
struct GDALVectorTranslateOptions
{
    // All arguments passed to GDALVectorTranslate() except the positional
    // ones (that is dataset names and layer names)
    CPLStringList aosArguments{};

    /*! continue after a failure, skipping the failed feature */
    bool bSkipFailures = false;

    /*! use layer level transaction. If set to FALSE, then it is interpreted as
     * dataset level transaction. */
    int nLayerTransaction = -1;

    /*! force the use of particular transaction type based on
     * GDALVectorTranslate::nLayerTransaction */
    bool bForceTransaction = false;

    /*! group nGroupTransactions features per transaction.
       Increase the value for better performance when writing into DBMS drivers
       that have transaction support. nGroupTransactions can be set to -1 to
       load the data into a single transaction */
    int nGroupTransactions = 100 * 1000;

    /*! If provided, only the feature with this feature id will be reported.
       Operates exclusive of the spatial or attribute queries. Note: if you want
       to select several features based on their feature id, you can also use
       the fact the 'fid' is a special field recognized by OGR SQL. So
       GDALVectorTranslateOptions::pszWHERE = "fid in (1,3,5)" would select
       features 1, 3 and 5. */
    GIntBig nFIDToFetch = OGRNullFID;

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet = false;

    /*! output file format name */
    std::string osFormat{};

    /*! list of layers of the source dataset which needs to be selected */
    CPLStringList aosLayers{};

    /*! dataset creation option (format specific) */
    CPLStringList aosDSCO{};

    /*! layer creation option (format specific) */
    CPLStringList aosLCO{};

    /*! access modes */
    GDALVectorTranslateAccessMode eAccessMode = ACCESS_CREATION;

    /*! whether to use UpsertFeature() instead of CreateFeature() */
    bool bUpsert = false;

    /*! It has the effect of adding, to existing target layers, the new fields
       found in source layers. This option is useful when merging files that
       have non-strictly identical structures. This might not work for output
       formats that don't support adding fields to existing non-empty layers. */
    bool bAddMissingFields = false;

    /*! It must be set to true to trigger reprojection, otherwise only SRS
     * assignment is done. */
    bool bTransform = false;

    /*! output SRS. GDALVectorTranslateOptions::bTransform must be set to true
       to trigger reprojection, otherwise only SRS assignment is done. */
    std::string osOutputSRSDef{};

    /*! Coordinate epoch of source SRS */
    double dfSourceCoordinateEpoch = 0;

    /*! Coordinate epoch of output SRS */
    double dfOutputCoordinateEpoch = 0;

    /*! override source SRS */
    std::string osSourceSRSDef{};

    /*! PROJ pipeline */
    std::string osCTPipeline{};

    bool bNullifyOutputSRS = false;

    /*! If set to false, then field name matching between source and existing
       target layer is done in a more relaxed way if the target driver has an
       implementation for it. */
    bool bExactFieldNameMatch = true;

    /*! an alternate name to the new layer */
    std::string osNewLayerName{};

    /*! attribute query (like SQL WHERE) */
    std::string osWHERE{};

    /*! name of the geometry field on which the spatial filter operates on. */
    std::string osGeomField{};

    /*! whether osGeomField is set (useful for empty strings) */
    bool bGeomFieldSet = false;

    /*! whether -select has been specified. This is of course true when
     * !aosSelFields.empty(), but this can also be set when an empty string
     * has been to disable fields. */
    bool bSelFieldsSet = false;

    /*! list of fields from input layer to copy to the new layer.
     * Geometry fields can also be specified in the list. */
    CPLStringList aosSelFields{};

    /*! SQL statement to execute. The resulting table/layer will be saved to the
     * output. */
    std::string osSQLStatement{};

    /*! SQL dialect. In some cases can be used to use (unoptimized) OGR SQL
       instead of the native SQL of an RDBMS by using "OGRSQL". The "SQLITE"
       dialect can also be used with any datasource. */
    std::string osDialect{};

    /*! the geometry type for the created layer */
    int eGType = GEOMTYPE_UNCHANGED;

    GeomTypeConversion eGeomTypeConversion = GTC_DEFAULT;

    /*! Geometric operation to perform */
    GeomOperation eGeomOp = GEOMOP_NONE;

    /*! the parameter to geometric operation */
    double dfGeomOpParam = 0;

    /*! Whether to run MakeValid */
    bool bMakeValid = false;

    /*! list of field types to convert to a field of type string in the
       destination layer. Valid types are: Integer, Integer64, Real, String,
       Date, Time, DateTime, Binary, IntegerList, Integer64List, RealList,
       StringList. Special value "All" can be used to convert all fields to
       strings. This is an alternate way to using the CAST operator of OGR SQL,
       that may avoid typing a long SQL query. Note that this does not influence
       the field types used by the source driver, and is only an afterwards
        conversion. */
    CPLStringList aosFieldTypesToString{};

    /*! list of field types and the field type after conversion in the
       destination layer.
        ("srctype1=dsttype1","srctype2=dsttype2",...).
        Valid types are : Integer, Integer64, Real, String, Date, Time,
       DateTime, Binary, IntegerList, Integer64List, RealList, StringList. Types
       can also include subtype between parenthesis, such as Integer(Boolean),
       Real(Float32), ... Special value "All" can be used to convert all fields
       to another type. This is an alternate way to using the CAST operator of
       OGR SQL, that may avoid typing a long SQL query. This is a generalization
       of GDALVectorTranslateOptions::papszFieldTypeToString. Note that this
       does not influence the field types used by the source driver, and is only
       an afterwards conversion. */
    CPLStringList aosMapFieldType{};

    /*! set field width and precision to 0 */
    bool bUnsetFieldWidth = false;

    /*! display progress on terminal. Only works if input layers have the "fast
    feature count" capability */
    bool bDisplayProgress = false;

    /*! split geometries crossing the dateline meridian */
    bool bWrapDateline = false;

    /*! offset from dateline in degrees (default long. = +/- 10deg, geometries
    within 170deg to -170deg will be split) */
    double dfDateLineOffset = 10.0;

    /*! clip geometries when it is set to true */
    bool bClipSrc = false;

    std::shared_ptr<OGRGeometry> poClipSrc{};

    /*! clip datasource */
    std::string osClipSrcDS{};

    /*! select desired geometries using an SQL query */
    std::string osClipSrcSQL{};

    /*! selected named layer from the source clip datasource */
    std::string osClipSrcLayer{};

    /*! restrict desired geometries based on attribute query */
    std::string osClipSrcWhere{};

    std::shared_ptr<OGRGeometry> poClipDst{};

    /*! destination clip datasource */
    std::string osClipDstDS{};

    /*! select desired geometries using an SQL query */
    std::string osClipDstSQL{};

    /*! selected named layer from the destination clip datasource */
    std::string osClipDstLayer{};

    /*! restrict desired geometries based on attribute query */
    std::string osClipDstWhere{};

    /*! split fields of type StringList, RealList or IntegerList into as many
       fields of type String, Real or Integer as necessary. */
    bool bSplitListFields = false;

    /*! limit the number of subfields created for each split field. */
    int nMaxSplitListSubFields = -1;

    /*! produce one feature for each geometry in any kind of geometry collection
       in the source file */
    bool bExplodeCollections = false;

    /*! uses the specified field to fill the Z coordinates of geometries */
    std::string osZField{};

    /*! the list of field indexes to be copied from the source to the
       destination. The (n)th value specified in the list is the index of the
       field in the target layer definition in which the n(th) field of the
       source layer must be copied. Index count starts at zero. There must be
        exactly as many values in the list as the count of the fields in the
       source layer. We can use the "identity" option to specify that the fields
       should be transferred by using the same order. This option should be used
       along with the GDALVectorTranslateOptions::eAccessMode = ACCESS_APPEND
       option. */
    CPLStringList aosFieldMap{};

    /*! force the coordinate dimension to nCoordDim (valid values are 2 or 3).
       This affects both the layer geometry type, and feature geometries. */
    int nCoordDim = COORD_DIM_UNCHANGED;

    /*! destination dataset open option (format specific), only valid in update
     * mode */
    CPLStringList aosDestOpenOptions{};

    /*! If set to true, does not propagate not-nullable constraints to target
       layer if they exist in source layer */
    bool bForceNullable = false;

    /*! If set to true, for each field with a coded field domains, create a
       field that contains the description of the coded value. */
    bool bResolveDomains = false;

    /*! If set to true, empty string values will be treated as null */
    bool bEmptyStrAsNull = false;

    /*! If set to true, does not propagate default field values to target layer
       if they exist in source layer */
    bool bUnsetDefault = false;

    /*! to prevent the new default behavior that consists in, if the output
       driver has a FID layer creation option and we are not in append mode, to
       preserve the name of the source FID column and source feature IDs */
    bool bUnsetFid = false;

    /*! use the FID of the source features instead of letting the output driver
       to automatically assign a new one. If not in append mode, this behavior
       becomes the default if the output driver has a FID layer creation option.
       In which case the name of the source FID column will be used and source
       feature IDs will be attempted to be preserved. This behavior can be
        disabled by option GDALVectorTranslateOptions::bUnsetFid */
    bool bPreserveFID = false;

    /*! set it to false to disable copying of metadata from source dataset and
       layers into target dataset and layers, when supported by output driver.
     */
    bool bCopyMD = true;

    /*! list of metadata key and value to set on the output dataset, when
       supported by output driver.
        ("META-TAG1=VALUE1","META-TAG2=VALUE2") */
    CPLStringList aosMetadataOptions{};

    /*! override spatial filter SRS */
    std::string osSpatSRSDef{};

    /*! list of ground control points to be added */
    CopyableGCPs oGCPs{};

    /*! order of polynomial used for warping (1 to 3). The default is to select
       a polynomial order based on the number of GCPs */
    int nTransformOrder = 0;

    /*! spatial query extents, in the SRS of the source layer(s) (or the one
       specified with GDALVectorTranslateOptions::pszSpatSRSDef). Only features
       whose geometry intersects the extents will be selected. The geometries
       will not be clipped unless GDALVectorTranslateOptions::bClipSrc is true.
     */
    std::shared_ptr<OGRGeometry> poSpatialFilter;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress = nullptr;

    /*! pointer to the progress data variable */
    void *pProgressData = nullptr;

    /*! Whether layer and feature native data must be transferred. */
    bool bNativeData = true;

    /*! Maximum number of features, or -1 if no limit. */
    GIntBig nLimit = -1;

    /*! Wished offset w.r.t UTC of dateTime */
    int nTZOffsetInSec = TZ_OFFSET_INVALID;

    /*! Geometry X,Y coordinate resolution */
    double dfXYRes = OGRGeomCoordinatePrecision::UNKNOWN;

    /*! Unit of dXYRes. empty string, "m", "mm" or "deg" */
    std::string osXYResUnit{};

    /*! Geometry Z coordinate resolution */
    double dfZRes = OGRGeomCoordinatePrecision::UNKNOWN;

    /*! Unit of dfZRes. empty string, "m" or "mm" */
    std::string osZResUnit{};

    /*! Geometry M coordinate resolution */
    double dfMRes = OGRGeomCoordinatePrecision::UNKNOWN;

    /*! Whether to unset geometry coordinate precision */
    bool bUnsetCoordPrecision = false;
};

struct TargetLayerInfo
{
    OGRLayer *m_poSrcLayer = nullptr;
    GIntBig m_nFeaturesRead = 0;
    bool m_bPerFeatureCT = 0;
    OGRLayer *m_poDstLayer = nullptr;
    bool m_bUseWriteArrowBatch = false;

    struct ReprojectionInfo
    {
        std::unique_ptr<OGRCoordinateTransformation> m_poCT{};
        CPLStringList m_aosTransformOptions{};
        bool m_bCanInvalidateValidity = true;
    };

    std::vector<ReprojectionInfo> m_aoReprojectionInfo{};

    std::vector<int> m_anMap{};

    struct ResolvedInfo
    {
        int nSrcField;
        const OGRFieldDomain *poDomain;
    };

    std::map<int, ResolvedInfo> m_oMapResolved{};
    std::map<const OGRFieldDomain *, std::map<std::string, std::string>>
        m_oMapDomainToKV{};
    int m_iSrcZField = -1;
    int m_iSrcFIDField = -1;
    int m_iRequestedSrcGeomField = -1;
    bool m_bPreserveFID = false;
    const char *m_pszCTPipeline = nullptr;
    bool m_bCanAvoidSetFrom = false;
    const char *m_pszSpatSRSDef = nullptr;
    OGRGeometryH m_hSpatialFilter = nullptr;
    const char *m_pszGeomField = nullptr;
    std::vector<int> m_anDateTimeFieldIdx{};
    bool m_bSupportCurves = false;
};

struct AssociatedLayers
{
    OGRLayer *poSrcLayer = nullptr;
    std::unique_ptr<TargetLayerInfo> psInfo{};
};

class SetupTargetLayer
{
    bool CanUseWriteArrowBatch(OGRLayer *poSrcLayer, OGRLayer *poDstLayer,
                               bool bJustCreatedLayer,
                               const GDALVectorTranslateOptions *psOptions,
                               bool &bError);

  public:
    GDALDataset *m_poSrcDS;
    GDALDataset *m_poDstDS;
    char **m_papszLCO;
    OGRSpatialReference *m_poOutputSRS;
    bool m_bTransform = false;
    bool m_bNullifyOutputSRS;
    bool m_bSelFieldsSet = false;
    char **m_papszSelFields;
    bool m_bAppend;
    bool m_bAddMissingFields;
    int m_eGType;
    GeomTypeConversion m_eGeomTypeConversion;
    int m_nCoordDim;
    bool m_bOverwrite;
    char **m_papszFieldTypesToString;
    char **m_papszMapFieldType;
    bool m_bUnsetFieldWidth;
    bool m_bExplodeCollections;
    const char *m_pszZField;
    char **m_papszFieldMap;
    const char *m_pszWHERE;
    bool m_bExactFieldNameMatch;
    bool m_bQuiet;
    bool m_bForceNullable;
    bool m_bResolveDomains;
    bool m_bUnsetDefault;
    bool m_bUnsetFid;
    bool m_bPreserveFID;
    bool m_bCopyMD;
    bool m_bNativeData;
    bool m_bNewDataSource;
    const char *m_pszCTPipeline;

    std::unique_ptr<TargetLayerInfo>
    Setup(OGRLayer *poSrcLayer, const char *pszNewLayerName,
          GDALVectorTranslateOptions *psOptions, GIntBig &nTotalEventsDone);
};

class LayerTranslator
{
    static bool TranslateArrow(const TargetLayerInfo *psInfo,
                               GIntBig nCountLayerFeatures,
                               GIntBig *pnReadFeatureCount,
                               GDALProgressFunc pfnProgress, void *pProgressArg,
                               const GDALVectorTranslateOptions *psOptions);

  public:
    GDALDataset *m_poSrcDS = nullptr;
    GDALDataset *m_poODS = nullptr;
    bool m_bTransform = false;
    bool m_bWrapDateline = false;
    CPLString m_osDateLineOffset{};
    OGRSpatialReference *m_poOutputSRS = nullptr;
    bool m_bNullifyOutputSRS = false;
    OGRSpatialReference *m_poUserSourceSRS = nullptr;
    OGRCoordinateTransformation *m_poGCPCoordTrans = nullptr;
    int m_eGType = -1;
    GeomTypeConversion m_eGeomTypeConversion = GTC_DEFAULT;
    bool m_bMakeValid = false;
    int m_nCoordDim = 0;
    GeomOperation m_eGeomOp = GEOMOP_NONE;
    double m_dfGeomOpParam = 0;
    OGRGeometry *m_poClipSrcOri = nullptr;
    bool m_bWarnedClipSrcSRS = false;
    std::unique_ptr<OGRGeometry> m_poClipSrcReprojectedToSrcSRS;
    const OGRSpatialReference *m_poClipSrcReprojectedToSrcSRS_SRS = nullptr;
    OGRGeometry *m_poClipDstOri = nullptr;
    bool m_bWarnedClipDstSRS = false;
    std::unique_ptr<OGRGeometry> m_poClipDstReprojectedToDstSRS;
    const OGRSpatialReference *m_poClipDstReprojectedToDstSRS_SRS = nullptr;
    bool m_bExplodeCollections = false;
    bool m_bNativeData = false;
    GIntBig m_nLimit = -1;
    OGRGeometryFactory::TransformWithOptionsCache m_transformWithOptionsCache;

    bool Translate(OGRFeature *poFeatureIn, TargetLayerInfo *psInfo,
                   GIntBig nCountLayerFeatures, GIntBig *pnReadFeatureCount,
                   GIntBig &nTotalEventsDone, GDALProgressFunc pfnProgress,
                   void *pProgressArg,
                   const GDALVectorTranslateOptions *psOptions);

  private:
    const OGRGeometry *GetDstClipGeom(const OGRSpatialReference *poGeomSRS);
    const OGRGeometry *GetSrcClipGeom(const OGRSpatialReference *poGeomSRS);
};

static OGRLayer *GetLayerAndOverwriteIfNecessary(GDALDataset *poDstDS,
                                                 const char *pszNewLayerName,
                                                 bool bOverwrite,
                                                 bool *pbErrorOccurred,
                                                 bool *pbOverwriteActuallyDone,
                                                 bool *pbAddOverwriteLCO);

/************************************************************************/
/*                           LoadGeometry()                             */
/************************************************************************/

static std::unique_ptr<OGRGeometry> LoadGeometry(const std::string &osDS,
                                                 const std::string &osSQL,
                                                 const std::string &osLyr,
                                                 const std::string &osWhere)
{
    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(osDS.c_str(), GDAL_OF_VECTOR));
    if (poDS == nullptr)
        return nullptr;

    OGRLayer *poLyr = nullptr;
    if (!osSQL.empty())
        poLyr = poDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr);
    else if (!osLyr.empty())
        poLyr = poDS->GetLayerByName(osLyr.c_str());
    else
        poLyr = poDS->GetLayer(0);

    if (poLyr == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to identify source layer from datasource.");
        return nullptr;
    }

    if (!osWhere.empty())
        poLyr->SetAttributeFilter(osWhere.c_str());

    OGRGeometryCollection oGC;

    const auto poSRSSrc = poLyr->GetSpatialRef();
    if (poSRSSrc)
    {
        auto poSRSClone = poSRSSrc->Clone();
        oGC.assignSpatialReference(poSRSClone);
        poSRSClone->Release();
    }

    for (auto &poFeat : poLyr)
    {
        auto poSrcGeom = std::unique_ptr<OGRGeometry>(poFeat->StealGeometry());
        if (poSrcGeom)
        {
            // Only take into account areal geometries.
            if (poSrcGeom->getDimension() == 2)
            {
                if (!poSrcGeom->IsValid())
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Geometry of feature " CPL_FRMT_GIB " of %s "
                             "is invalid. Trying to make it valid",
                             poFeat->GetFID(), osDS.c_str());
                    auto poValid =
                        std::unique_ptr<OGRGeometry>(poSrcGeom->MakeValid());
                    if (poValid)
                    {
                        oGC.addGeometryDirectly(poValid.release());
                    }
                }
                else
                {
                    oGC.addGeometryDirectly(poSrcGeom.release());
                }
            }
        }
    }

    if (!osSQL.empty())
        poDS->ReleaseResultSet(poLyr);

    if (oGC.IsEmpty())
        return nullptr;

    return std::unique_ptr<OGRGeometry>(oGC.UnaryUnion());
}

/************************************************************************/
/*                     OGRSplitListFieldLayer                           */
/************************************************************************/

typedef struct
{
    int iSrcIndex;
    OGRFieldType eType;
    int nMaxOccurrences;
    int nWidth;
} ListFieldDesc;

class OGRSplitListFieldLayer : public OGRLayer
{
    OGRLayer *poSrcLayer;
    OGRFeatureDefn *poFeatureDefn;
    ListFieldDesc *pasListFields;
    int nListFieldCount;
    int nMaxSplitListSubFields;

    OGRFeature *TranslateFeature(OGRFeature *poSrcFeature);

  public:
    OGRSplitListFieldLayer(OGRLayer *poSrcLayer, int nMaxSplitListSubFields);
    virtual ~OGRSplitListFieldLayer();

    bool BuildLayerDefn(GDALProgressFunc pfnProgress, void *pProgressArg);

    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual void ResetReading() override
    {
        poSrcLayer->ResetReading();
    }

    virtual int TestCapability(const char *) override
    {
        return FALSE;
    }

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override
    {
        return poSrcLayer->GetFeatureCount(bForce);
    }

    virtual OGRSpatialReference *GetSpatialRef() override
    {
        return poSrcLayer->GetSpatialRef();
    }

    virtual OGRGeometry *GetSpatialFilter() override
    {
        return poSrcLayer->GetSpatialFilter();
    }

    virtual OGRStyleTable *GetStyleTable() override
    {
        return poSrcLayer->GetStyleTable();
    }

    virtual void SetSpatialFilter(OGRGeometry *poGeom) override
    {
        poSrcLayer->SetSpatialFilter(poGeom);
    }

    virtual void SetSpatialFilter(int iGeom, OGRGeometry *poGeom) override
    {
        poSrcLayer->SetSpatialFilter(iGeom, poGeom);
    }

    virtual void SetSpatialFilterRect(double dfMinX, double dfMinY,
                                      double dfMaxX, double dfMaxY) override
    {
        poSrcLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
    }

    virtual void SetSpatialFilterRect(int iGeom, double dfMinX, double dfMinY,
                                      double dfMaxX, double dfMaxY) override
    {
        poSrcLayer->SetSpatialFilterRect(iGeom, dfMinX, dfMinY, dfMaxX, dfMaxY);
    }

    virtual OGRErr SetAttributeFilter(const char *pszFilter) override
    {
        return poSrcLayer->SetAttributeFilter(pszFilter);
    }
};

/************************************************************************/
/*                    OGRSplitListFieldLayer()                          */
/************************************************************************/

OGRSplitListFieldLayer::OGRSplitListFieldLayer(OGRLayer *poSrcLayerIn,
                                               int nMaxSplitListSubFieldsIn)
    : poSrcLayer(poSrcLayerIn), poFeatureDefn(nullptr), pasListFields(nullptr),
      nListFieldCount(0),
      nMaxSplitListSubFields(
          nMaxSplitListSubFieldsIn < 0 ? INT_MAX : nMaxSplitListSubFieldsIn)
{
}

/************************************************************************/
/*                   ~OGRSplitListFieldLayer()                          */
/************************************************************************/

OGRSplitListFieldLayer::~OGRSplitListFieldLayer()
{
    if (poFeatureDefn)
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

    OGRFeatureDefn *poSrcFieldDefn = poSrcLayer->GetLayerDefn();

    int nSrcFields = poSrcFieldDefn->GetFieldCount();
    pasListFields = static_cast<ListFieldDesc *>(
        CPLCalloc(sizeof(ListFieldDesc), nSrcFields));
    nListFieldCount = 0;

    /* Establish the list of fields of list type */
    for (int i = 0; i < nSrcFields; ++i)
    {
        OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(i)->GetType();
        if (eType == OFTIntegerList || eType == OFTInteger64List ||
            eType == OFTRealList || eType == OFTStringList)
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
        for (auto &poSrcFeature : poSrcLayer)
        {
            for (int i = 0; i < nListFieldCount; ++i)
            {
                int nCount = 0;
                OGRField *psField =
                    poSrcFeature->GetRawFieldRef(pasListFields[i].iSrcIndex);
                switch (pasListFields[i].eType)
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
                        char **paList = psField->StringList.paList;
                        for (int j = 0; j < nCount; j++)
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

            nFeatureIndex++;
            if (pfnProgress != nullptr && nFeatureCount != 0)
                pfnProgress(nFeatureIndex * 1.0 / nFeatureCount, "",
                            pProgressArg);
        }
    }

    /* Now let's build the target feature definition */

    poFeatureDefn =
        OGRFeatureDefn::CreateFeatureDefn(poSrcFieldDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    for (int i = 0; i < poSrcFieldDefn->GetGeomFieldCount(); ++i)
    {
        poFeatureDefn->AddGeomFieldDefn(poSrcFieldDefn->GetGeomFieldDefn(i));
    }

    int iListField = 0;
    for (int i = 0; i < nSrcFields; ++i)
    {
        const OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(i)->GetType();
        if (eType == OFTIntegerList || eType == OFTInteger64List ||
            eType == OFTRealList || eType == OFTStringList)
        {
            const int nMaxOccurrences =
                pasListFields[iListField].nMaxOccurrences;
            const int nWidth = pasListFields[iListField].nWidth;
            iListField++;
            if (nMaxOccurrences == 1)
            {
                OGRFieldDefn oFieldDefn(
                    poSrcFieldDefn->GetFieldDefn(i)->GetNameRef(),
                    (eType == OFTIntegerList)     ? OFTInteger
                    : (eType == OFTInteger64List) ? OFTInteger64
                    : (eType == OFTRealList)      ? OFTReal
                                                  : OFTString);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }
            else
            {
                for (int j = 0; j < nMaxOccurrences; j++)
                {
                    CPLString osFieldName;
                    osFieldName.Printf(
                        "%s%d", poSrcFieldDefn->GetFieldDefn(i)->GetNameRef(),
                        j + 1);
                    OGRFieldDefn oFieldDefn(
                        osFieldName.c_str(),
                        (eType == OFTIntegerList)     ? OFTInteger
                        : (eType == OFTInteger64List) ? OFTInteger64
                        : (eType == OFTRealList)      ? OFTReal
                                                      : OFTString);
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

OGRFeature *OGRSplitListFieldLayer::TranslateFeature(OGRFeature *poSrcFeature)
{
    if (poSrcFeature == nullptr)
        return nullptr;
    if (poFeatureDefn == nullptr)
        return poSrcFeature;

    OGRFeature *poFeature = OGRFeature::CreateFeature(poFeatureDefn);
    poFeature->SetFID(poSrcFeature->GetFID());
    for (int i = 0; i < poFeature->GetGeomFieldCount(); i++)
    {
        poFeature->SetGeomFieldDirectly(i, poSrcFeature->StealGeometry(i));
    }
    poFeature->SetStyleString(poFeature->GetStyleString());

    OGRFeatureDefn *poSrcFieldDefn = poSrcLayer->GetLayerDefn();
    int nSrcFields = poSrcFeature->GetFieldCount();
    int iDstField = 0;
    int iListField = 0;

    for (int iSrcField = 0; iSrcField < nSrcFields; ++iSrcField)
    {
        const OGRFieldType eType =
            poSrcFieldDefn->GetFieldDefn(iSrcField)->GetType();
        OGRField *psField = poSrcFeature->GetRawFieldRef(iSrcField);
        switch (eType)
        {
            case OFTIntegerList:
            {
                const int nCount = std::min(nMaxSplitListSubFields,
                                            psField->IntegerList.nCount);
                int *paList = psField->IntegerList.paList;
                for (int j = 0; j < nCount; ++j)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTInteger64List:
            {
                const int nCount = std::min(nMaxSplitListSubFields,
                                            psField->Integer64List.nCount);
                GIntBig *paList = psField->Integer64List.paList;
                for (int j = 0; j < nCount; ++j)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTRealList:
            {
                const int nCount =
                    std::min(nMaxSplitListSubFields, psField->RealList.nCount);
                double *paList = psField->RealList.paList;
                for (int j = 0; j < nCount; ++j)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            case OFTStringList:
            {
                const int nCount = std::min(nMaxSplitListSubFields,
                                            psField->StringList.nCount);
                char **paList = psField->StringList.paList;
                for (int j = 0; j < nCount; ++j)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurrences;
                iListField++;
                break;
            }
            default:
            {
                poFeature->SetField(iDstField, psField);
                iDstField++;
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

OGRFeatureDefn *OGRSplitListFieldLayer::GetLayerDefn()
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
    GCPCoordTransformation(const GCPCoordTransformation &other)
        : hTransformArg(GDALCloneTransformer(other.hTransformArg)),
          bUseTPS(other.bUseTPS), poSRS(other.poSRS)
    {
        if (poSRS)
            poSRS->Reference();
    }

    GCPCoordTransformation &operator=(const GCPCoordTransformation &) = delete;

  public:
    void *hTransformArg;
    bool bUseTPS;
    OGRSpatialReference *poSRS;

    GCPCoordTransformation(int nGCPCount, const GDAL_GCP *pasGCPList,
                           int nReqOrder, OGRSpatialReference *poSRSIn)
        : hTransformArg(nullptr), bUseTPS(nReqOrder < 0), poSRS(poSRSIn)
    {
        if (nReqOrder < 0)
        {
            hTransformArg =
                GDALCreateTPSTransformer(nGCPCount, pasGCPList, FALSE);
        }
        else
        {
            hTransformArg = GDALCreateGCPTransformer(nGCPCount, pasGCPList,
                                                     nReqOrder, FALSE);
        }
        if (poSRS)
            poSRS->Reference();
    }

    OGRCoordinateTransformation *Clone() const override
    {
        return new GCPCoordTransformation(*this);
    }

    bool IsValid() const
    {
        return hTransformArg != nullptr;
    }

    virtual ~GCPCoordTransformation()
    {
        if (hTransformArg != nullptr)
        {
            GDALDestroyTransformer(hTransformArg);
        }
        if (poSRS)
            poSRS->Dereference();
    }

    virtual const OGRSpatialReference *GetSourceCS() const override
    {
        return poSRS;
    }

    virtual const OGRSpatialReference *GetTargetCS() const override
    {
        return poSRS;
    }

    virtual int Transform(size_t nCount, double *x, double *y, double *z,
                          double * /* t */, int *pabSuccess) override
    {
        CPLAssert(nCount <=
                  static_cast<size_t>(std::numeric_limits<int>::max()));
        if (bUseTPS)
            return GDALTPSTransform(hTransformArg, FALSE,
                                    static_cast<int>(nCount), x, y, z,
                                    pabSuccess);
        else
            return GDALGCPTransform(hTransformArg, FALSE,
                                    static_cast<int>(nCount), x, y, z,
                                    pabSuccess);
    }

    virtual OGRCoordinateTransformation *GetInverse() const override
    {
        return nullptr;
    }
};

/************************************************************************/
/*                            CompositeCT                               */
/************************************************************************/

class CompositeCT : public OGRCoordinateTransformation
{
    CompositeCT(const CompositeCT &other)
        : poCT1(other.poCT1 ? other.poCT1->Clone() : nullptr), bOwnCT1(true),
          poCT2(other.poCT2 ? other.poCT2->Clone() : nullptr), bOwnCT2(true)
    {
    }

    CompositeCT &operator=(const CompositeCT &) = delete;

  public:
    OGRCoordinateTransformation *poCT1;
    bool bOwnCT1;
    OGRCoordinateTransformation *poCT2;
    bool bOwnCT2;

    CompositeCT(OGRCoordinateTransformation *poCT1In, bool bOwnCT1In,
                OGRCoordinateTransformation *poCT2In, bool bOwnCT2In)
        : poCT1(poCT1In), bOwnCT1(bOwnCT1In), poCT2(poCT2In), bOwnCT2(bOwnCT2In)
    {
    }

    virtual ~CompositeCT()
    {
        if (bOwnCT1)
            delete poCT1;
        if (bOwnCT2)
            delete poCT2;
    }

    OGRCoordinateTransformation *Clone() const override
    {
        return new CompositeCT(*this);
    }

    virtual const OGRSpatialReference *GetSourceCS() const override
    {
        return poCT1   ? poCT1->GetSourceCS()
               : poCT2 ? poCT2->GetSourceCS()
                       : nullptr;
    }

    virtual const OGRSpatialReference *GetTargetCS() const override
    {
        return poCT2   ? poCT2->GetTargetCS()
               : poCT1 ? poCT1->GetTargetCS()
                       : nullptr;
    }

    virtual bool GetEmitErrors() const override
    {
        if (poCT1)
            return poCT1->GetEmitErrors();
        if (poCT2)
            return poCT2->GetEmitErrors();
        return true;
    }

    virtual void SetEmitErrors(bool bEmitErrors) override
    {
        if (poCT1)
            poCT1->SetEmitErrors(bEmitErrors);
        if (poCT2)
            poCT2->SetEmitErrors(bEmitErrors);
    }

    virtual int Transform(size_t nCount, double *x, double *y, double *z,
                          double *t, int *pabSuccess) override
    {
        int nResult = TRUE;
        if (poCT1)
            nResult = poCT1->Transform(nCount, x, y, z, t, pabSuccess);
        if (nResult && poCT2)
            nResult = poCT2->Transform(nCount, x, y, z, t, pabSuccess);
        return nResult;
    }

    virtual OGRCoordinateTransformation *GetInverse() const override
    {
        return nullptr;
    }
};

/************************************************************************/
/*                    AxisMappingCoordinateTransformation               */
/************************************************************************/

class AxisMappingCoordinateTransformation : public OGRCoordinateTransformation
{
  public:
    bool bSwapXY = false;

    AxisMappingCoordinateTransformation(const std::vector<int> &mappingIn,
                                        const std::vector<int> &mappingOut)
    {
        if (mappingIn.size() >= 2 && mappingIn[0] == 1 && mappingIn[1] == 2 &&
            mappingOut.size() >= 2 && mappingOut[0] == 2 && mappingOut[1] == 1)
        {
            bSwapXY = true;
        }
        else if (mappingIn.size() >= 2 && mappingIn[0] == 2 &&
                 mappingIn[1] == 1 && mappingOut.size() >= 2 &&
                 mappingOut[0] == 1 && mappingOut[1] == 2)
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

    virtual const OGRSpatialReference *GetSourceCS() const override
    {
        return nullptr;
    }

    virtual const OGRSpatialReference *GetTargetCS() const override
    {
        return nullptr;
    }

    virtual int Transform(size_t nCount, double *x, double *y, double * /*z*/,
                          double * /*t*/, int *pabSuccess) override
    {
        for (size_t i = 0; i < nCount; i++)
        {
            if (pabSuccess)
                pabSuccess[i] = true;
            if (bSwapXY)
                std::swap(x[i], y[i]);
        }
        return true;
    }

    virtual OGRCoordinateTransformation *GetInverse() const override
    {
        return nullptr;
    }
};

/************************************************************************/
/*                        ApplySpatialFilter()                          */
/************************************************************************/

static void ApplySpatialFilter(OGRLayer *poLayer, OGRGeometry *poSpatialFilter,
                               const OGRSpatialReference *poSpatSRS,
                               const char *pszGeomField,
                               const OGRSpatialReference *poSourceSRS)
{
    if (poSpatialFilter == nullptr)
        return;

    OGRGeometry *poSpatialFilterReprojected = nullptr;
    if (poSpatSRS)
    {
        poSpatialFilterReprojected = poSpatialFilter->clone();
        poSpatialFilterReprojected->assignSpatialReference(poSpatSRS);
        const OGRSpatialReference *poSpatialFilterTargetSRS =
            poSourceSRS ? poSourceSRS : poLayer->GetSpatialRef();
        if (poSpatialFilterTargetSRS)
        {
            // When transforming the spatial filter from its spat_srs to the
            // layer SRS, make sure to densify it sufficiently to avoid issues
            constexpr double SEGMENT_DISTANCE_METRE = 10 * 1000;
            if (poSpatSRS->IsGeographic())
            {
                const double LENGTH_OF_ONE_DEGREE =
                    poSpatSRS->GetSemiMajor(nullptr) * M_PI / 180.0;
                poSpatialFilterReprojected->segmentize(SEGMENT_DISTANCE_METRE /
                                                       LENGTH_OF_ONE_DEGREE);
            }
            else if (poSpatSRS->IsProjected())
            {
                poSpatialFilterReprojected->segmentize(
                    SEGMENT_DISTANCE_METRE /
                    poSpatSRS->GetLinearUnits(nullptr));
            }
            poSpatialFilterReprojected->transformTo(poSpatialFilterTargetSRS);
        }
        else
            CPLError(CE_Warning, CPLE_AppDefined,
                     "cannot determine layer SRS for %s.",
                     poLayer->GetDescription());
    }

    if (pszGeomField != nullptr)
    {
        const int iGeomField =
            poLayer->GetLayerDefn()->GetGeomFieldIndex(pszGeomField);
        if (iGeomField >= 0)
            poLayer->SetSpatialFilter(iGeomField,
                                      poSpatialFilterReprojected
                                          ? poSpatialFilterReprojected
                                          : poSpatialFilter);
        else
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot find geometry field %s.", pszGeomField);
    }
    else
    {
        poLayer->SetSpatialFilter(poSpatialFilterReprojected
                                      ? poSpatialFilterReprojected
                                      : poSpatialFilter);
    }

    delete poSpatialFilterReprojected;
}

/************************************************************************/
/*                            GetFieldType()                            */
/************************************************************************/

static int GetFieldType(const char *pszArg, int *pnSubFieldType)
{
    *pnSubFieldType = OFSTNone;
    const char *pszOpenParenthesis = strchr(pszArg, '(');
    const int nLengthBeforeParenthesis =
        pszOpenParenthesis ? static_cast<int>(pszOpenParenthesis - pszArg)
                           : static_cast<int>(strlen(pszArg));
    for (int iType = 0; iType <= static_cast<int>(OFTMaxType); iType++)
    {
        const char *pszFieldTypeName =
            OGRFieldDefn::GetFieldTypeName(static_cast<OGRFieldType>(iType));
        if (EQUALN(pszArg, pszFieldTypeName, nLengthBeforeParenthesis) &&
            pszFieldTypeName[nLengthBeforeParenthesis] == '\0')
        {
            if (pszOpenParenthesis != nullptr)
            {
                *pnSubFieldType = -1;
                CPLString osArgSubType = pszOpenParenthesis + 1;
                if (!osArgSubType.empty() && osArgSubType.back() == ')')
                    osArgSubType.resize(osArgSubType.size() - 1);
                for (int iSubType = 0;
                     iSubType <= static_cast<int>(OFSTMaxSubType); iSubType++)
                {
                    const char *pszFieldSubTypeName =
                        OGRFieldDefn::GetFieldSubTypeName(
                            static_cast<OGRFieldSubType>(iSubType));
                    if (EQUAL(pszFieldSubTypeName, osArgSubType))
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
/*                           IsFieldType()                              */
/************************************************************************/

static bool IsFieldType(const char *pszArg)
{
    int iSubType;
    return GetFieldType(pszArg, &iSubType) >= 0 && iSubType >= 0;
}

class GDALVectorTranslateWrappedDataset : public GDALDataset
{
    GDALDataset *m_poBase;
    OGRSpatialReference *m_poOutputSRS;
    bool m_bTransform;

    std::vector<OGRLayer *> m_apoLayers;
    std::vector<OGRLayer *> m_apoHiddenLayers;

    GDALVectorTranslateWrappedDataset(GDALDataset *poBase,
                                      OGRSpatialReference *poOutputSRS,
                                      bool bTransform);

  public:
    virtual ~GDALVectorTranslateWrappedDataset();

    virtual int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    virtual OGRLayer *GetLayer(int nIdx) override;
    virtual OGRLayer *GetLayerByName(const char *pszName) override;

    virtual OGRLayer *ExecuteSQL(const char *pszStatement,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;
    virtual void ReleaseResultSet(OGRLayer *poResultsSet) override;

    static GDALVectorTranslateWrappedDataset *
    New(GDALDataset *poBase, OGRSpatialReference *poOutputSRS, bool bTransform);
};

class GDALVectorTranslateWrappedLayer : public OGRLayerDecorator
{
    std::vector<OGRCoordinateTransformation *> m_apoCT;
    OGRFeatureDefn *m_poFDefn;

    GDALVectorTranslateWrappedLayer(OGRLayer *poBaseLayer, bool bOwnBaseLayer);
    OGRFeature *TranslateFeature(OGRFeature *poSrcFeat);

  public:
    virtual ~GDALVectorTranslateWrappedLayer();

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFDefn;
    }

    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;

    static GDALVectorTranslateWrappedLayer *
    New(OGRLayer *poBaseLayer, bool bOwnBaseLayer,
        OGRSpatialReference *poOutputSRS, bool bTransform);
};

GDALVectorTranslateWrappedLayer::GDALVectorTranslateWrappedLayer(
    OGRLayer *poBaseLayer, bool bOwnBaseLayer)
    : OGRLayerDecorator(poBaseLayer, bOwnBaseLayer),
      m_apoCT(poBaseLayer->GetLayerDefn()->GetGeomFieldCount(),
              static_cast<OGRCoordinateTransformation *>(nullptr)),
      m_poFDefn(nullptr)
{
}

GDALVectorTranslateWrappedLayer *
GDALVectorTranslateWrappedLayer::New(OGRLayer *poBaseLayer, bool bOwnBaseLayer,
                                     OGRSpatialReference *poOutputSRS,
                                     bool bTransform)
{
    GDALVectorTranslateWrappedLayer *poNew =
        new GDALVectorTranslateWrappedLayer(poBaseLayer, bOwnBaseLayer);
    poNew->m_poFDefn = poBaseLayer->GetLayerDefn()->Clone();
    poNew->m_poFDefn->Reference();
    if (!poOutputSRS)
        return poNew;

    for (int i = 0; i < poNew->m_poFDefn->GetGeomFieldCount(); i++)
    {
        if (bTransform)
        {
            const OGRSpatialReference *poSourceSRS = poBaseLayer->GetLayerDefn()
                                                         ->GetGeomFieldDefn(i)
                                                         ->GetSpatialRef();
            if (poSourceSRS == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer %s has no source SRS for geometry field %s",
                         poBaseLayer->GetName(),
                         poBaseLayer->GetLayerDefn()
                             ->GetGeomFieldDefn(i)
                             ->GetNameRef());
                delete poNew;
                return nullptr;
            }
            else
            {
                poNew->m_apoCT[i] =
                    OGRCreateCoordinateTransformation(poSourceSRS, poOutputSRS);
                if (poNew->m_apoCT[i] == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Failed to create coordinate transformation "
                             "between the\n"
                             "following coordinate systems.  This may be "
                             "because they\n"
                             "are not transformable.");

                    char *pszWKT = nullptr;
                    poSourceSRS->exportToPrettyWkt(&pszWKT, FALSE);
                    CPLError(CE_Failure, CPLE_AppDefined, "Source:\n%s",
                             pszWKT);
                    CPLFree(pszWKT);

                    poOutputSRS->exportToPrettyWkt(&pszWKT, FALSE);
                    CPLError(CE_Failure, CPLE_AppDefined, "Target:\n%s",
                             pszWKT);
                    CPLFree(pszWKT);

                    delete poNew;
                    return nullptr;
                }
            }
        }
        poNew->m_poFDefn->GetGeomFieldDefn(i)->SetSpatialRef(poOutputSRS);
    }

    return poNew;
}

GDALVectorTranslateWrappedLayer::~GDALVectorTranslateWrappedLayer()
{
    if (m_poFDefn)
        m_poFDefn->Release();
    for (size_t i = 0; i < m_apoCT.size(); ++i)
        delete m_apoCT[i];
}

OGRFeature *GDALVectorTranslateWrappedLayer::GetNextFeature()
{
    return TranslateFeature(OGRLayerDecorator::GetNextFeature());
}

OGRFeature *GDALVectorTranslateWrappedLayer::GetFeature(GIntBig nFID)
{
    return TranslateFeature(OGRLayerDecorator::GetFeature(nFID));
}

OGRFeature *
GDALVectorTranslateWrappedLayer::TranslateFeature(OGRFeature *poSrcFeat)
{
    if (poSrcFeat == nullptr)
        return nullptr;
    OGRFeature *poNewFeat = new OGRFeature(m_poFDefn);
    poNewFeat->SetFrom(poSrcFeat);
    poNewFeat->SetFID(poSrcFeat->GetFID());
    for (int i = 0; i < poNewFeat->GetGeomFieldCount(); i++)
    {
        OGRGeometry *poGeom = poNewFeat->GetGeomFieldRef(i);
        if (poGeom)
        {
            if (m_apoCT[i])
                poGeom->transform(m_apoCT[i]);
            poGeom->assignSpatialReference(
                m_poFDefn->GetGeomFieldDefn(i)->GetSpatialRef());
        }
    }
    delete poSrcFeat;
    return poNewFeat;
}

GDALVectorTranslateWrappedDataset::GDALVectorTranslateWrappedDataset(
    GDALDataset *poBase, OGRSpatialReference *poOutputSRS, bool bTransform)
    : m_poBase(poBase), m_poOutputSRS(poOutputSRS), m_bTransform(bTransform)
{
    SetDescription(poBase->GetDescription());
    if (poBase->GetDriver())
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription(poBase->GetDriver()->GetDescription());
    }
}

GDALVectorTranslateWrappedDataset *GDALVectorTranslateWrappedDataset::New(
    GDALDataset *poBase, OGRSpatialReference *poOutputSRS, bool bTransform)
{
    GDALVectorTranslateWrappedDataset *poNew =
        new GDALVectorTranslateWrappedDataset(poBase, poOutputSRS, bTransform);
    for (int i = 0; i < poBase->GetLayerCount(); i++)
    {
        OGRLayer *poLayer = GDALVectorTranslateWrappedLayer::New(
            poBase->GetLayer(i), false, poOutputSRS, bTransform);
        if (poLayer == nullptr)
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
    for (size_t i = 0; i < m_apoLayers.size(); i++)
    {
        delete m_apoLayers[i];
    }
    for (size_t i = 0; i < m_apoHiddenLayers.size(); i++)
    {
        delete m_apoHiddenLayers[i];
    }
}

OGRLayer *GDALVectorTranslateWrappedDataset::GetLayer(int i)
{
    if (i < 0 || i >= static_cast<int>(m_apoLayers.size()))
        return nullptr;
    return m_apoLayers[i];
}

OGRLayer *GDALVectorTranslateWrappedDataset::GetLayerByName(const char *pszName)
{
    for (size_t i = 0; i < m_apoLayers.size(); i++)
    {
        if (strcmp(m_apoLayers[i]->GetName(), pszName) == 0)
            return m_apoLayers[i];
    }
    for (size_t i = 0; i < m_apoHiddenLayers.size(); i++)
    {
        if (strcmp(m_apoHiddenLayers[i]->GetName(), pszName) == 0)
            return m_apoHiddenLayers[i];
    }
    for (size_t i = 0; i < m_apoLayers.size(); i++)
    {
        if (EQUAL(m_apoLayers[i]->GetName(), pszName))
            return m_apoLayers[i];
    }
    for (size_t i = 0; i < m_apoHiddenLayers.size(); i++)
    {
        if (EQUAL(m_apoHiddenLayers[i]->GetName(), pszName))
            return m_apoHiddenLayers[i];
    }

    OGRLayer *poLayer = m_poBase->GetLayerByName(pszName);
    if (poLayer == nullptr)
        return nullptr;
    poLayer = GDALVectorTranslateWrappedLayer::New(poLayer, false,
                                                   m_poOutputSRS, m_bTransform);
    if (poLayer == nullptr)
        return nullptr;

    // Replicate source dataset behavior: if the fact of calling
    // GetLayerByName() on a initially hidden layer makes it visible through
    // GetLayerCount()/GetLayer(), do the same. Otherwise we are going to
    // maintain it hidden as well.
    for (int i = 0; i < m_poBase->GetLayerCount(); i++)
    {
        if (m_poBase->GetLayer(i) == poLayer)
        {
            m_apoLayers.push_back(poLayer);
            return poLayer;
        }
    }
    m_apoHiddenLayers.push_back(poLayer);
    return poLayer;
}

OGRLayer *
GDALVectorTranslateWrappedDataset::ExecuteSQL(const char *pszStatement,
                                              OGRGeometry *poSpatialFilter,
                                              const char *pszDialect)
{
    OGRLayer *poLayer =
        m_poBase->ExecuteSQL(pszStatement, poSpatialFilter, pszDialect);
    if (poLayer == nullptr)
        return nullptr;
    return GDALVectorTranslateWrappedLayer::New(poLayer, true, m_poOutputSRS,
                                                m_bTransform);
}

void GDALVectorTranslateWrappedDataset::ReleaseResultSet(OGRLayer *poResultsSet)
{
    delete poResultsSet;
}

/************************************************************************/
/*                     OGR2OGRSpatialReferenceHolder                    */
/************************************************************************/

class OGR2OGRSpatialReferenceHolder
{
    OGRSpatialReference *m_poSRS;

  public:
    OGR2OGRSpatialReferenceHolder() : m_poSRS(nullptr)
    {
    }

    ~OGR2OGRSpatialReferenceHolder()
    {
        if (m_poSRS)
            m_poSRS->Release();
    }

    void assignNoRefIncrease(OGRSpatialReference *poSRS)
    {
        CPLAssert(m_poSRS == nullptr);
        m_poSRS = poSRS;
    }

    OGRSpatialReference *get()
    {
        return m_poSRS;
    }
};

/************************************************************************/
/*                     GDALVectorTranslateCreateCopy()                  */
/************************************************************************/

static GDALDataset *
GDALVectorTranslateCreateCopy(GDALDriver *poDriver, const char *pszDest,
                              GDALDataset *poDS,
                              const GDALVectorTranslateOptions *psOptions)
{
    const char *const szErrorMsg = "%s not supported by this output driver";

    if (psOptions->bSkipFailures)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-skipfailures");
        return nullptr;
    }
    if (psOptions->nLayerTransaction >= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg,
                 "-lyr_transaction or -ds_transaction");
        return nullptr;
    }
    if (psOptions->nFIDToFetch >= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fid");
        return nullptr;
    }
    if (!psOptions->aosLCO.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-lco");
        return nullptr;
    }
    if (psOptions->bAddMissingFields)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-addfields");
        return nullptr;
    }
    if (!psOptions->osSourceSRSDef.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-s_srs");
        return nullptr;
    }
    if (!psOptions->bExactFieldNameMatch)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg,
                 "-relaxedFieldNameMatch");
        return nullptr;
    }
    if (!psOptions->osNewLayerName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nln");
        return nullptr;
    }
    if (psOptions->bSelFieldsSet)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-select");
        return nullptr;
    }
    if (!psOptions->osSQLStatement.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-sql");
        return nullptr;
    }
    if (!psOptions->osDialect.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-dialect");
        return nullptr;
    }
    if (psOptions->eGType != GEOMTYPE_UNCHANGED ||
        psOptions->eGeomTypeConversion != GTC_DEFAULT)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nlt");
        return nullptr;
    }
    if (!psOptions->aosFieldTypesToString.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg,
                 "-fieldTypeToString");
        return nullptr;
    }
    if (!psOptions->aosMapFieldType.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-mapFieldType");
        return nullptr;
    }
    if (psOptions->bUnsetFieldWidth)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetFieldWidth");
        return nullptr;
    }
    if (psOptions->bWrapDateline)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-wrapdateline");
        return nullptr;
    }
    if (psOptions->bClipSrc)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrc");
        return nullptr;
    }
    if (!psOptions->osClipSrcSQL.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrcsql");
        return nullptr;
    }
    if (!psOptions->osClipSrcLayer.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrclayer");
        return nullptr;
    }
    if (!psOptions->osClipSrcWhere.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipsrcwhere");
        return nullptr;
    }
    if (!psOptions->osClipDstDS.empty() || psOptions->poClipDst)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdst");
        return nullptr;
    }
    if (!psOptions->osClipDstSQL.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstsql");
        return nullptr;
    }
    if (!psOptions->osClipDstLayer.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstlayer");
        return nullptr;
    }
    if (!psOptions->osClipDstWhere.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-clipdstwhere");
        return nullptr;
    }
    if (psOptions->bSplitListFields)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-splitlistfields");
        return nullptr;
    }
    if (psOptions->nMaxSplitListSubFields >= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-maxsubfields");
        return nullptr;
    }
    if (psOptions->bExplodeCollections)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg,
                 "-explodecollections");
        return nullptr;
    }
    if (!psOptions->osZField.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-zfield");
        return nullptr;
    }
    if (psOptions->oGCPs.nGCPCount)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-gcp");
        return nullptr;
    }
    if (!psOptions->aosFieldMap.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-fieldmap");
        return nullptr;
    }
    if (psOptions->bForceNullable)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-forceNullable");
        return nullptr;
    }
    if (psOptions->bResolveDomains)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-forceNullable");
        return nullptr;
    }
    if (psOptions->bEmptyStrAsNull)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-emptyStrAsNull");
        return nullptr;
    }
    if (psOptions->bUnsetDefault)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetDefault");
        return nullptr;
    }
    if (psOptions->bUnsetFid)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-unsetFid");
        return nullptr;
    }
    if (!psOptions->bCopyMD)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-nomd");
        return nullptr;
    }
    if (!psOptions->bNativeData)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-noNativeData");
        return nullptr;
    }
    if (psOptions->nLimit >= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-limit");
        return nullptr;
    }
    if (!psOptions->aosMetadataOptions.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-mo");
        return nullptr;
    }

    GDALDataset *poWrkSrcDS = poDS;
    OGR2OGRSpatialReferenceHolder oOutputSRSHolder;

    if (!psOptions->osOutputSRSDef.empty())
    {
        oOutputSRSHolder.assignNoRefIncrease(new OGRSpatialReference());
        oOutputSRSHolder.get()->SetAxisMappingStrategy(
            OAMS_TRADITIONAL_GIS_ORDER);
        if (oOutputSRSHolder.get()->SetFromUserInput(
                psOptions->osOutputSRSDef.c_str()) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to process SRS definition: %s",
                     psOptions->osOutputSRSDef.c_str());
            return nullptr;
        }
        oOutputSRSHolder.get()->SetCoordinateEpoch(
            psOptions->dfOutputCoordinateEpoch);

        poWrkSrcDS = GDALVectorTranslateWrappedDataset::New(
            poDS, oOutputSRSHolder.get(), psOptions->bTransform);
        if (poWrkSrcDS == nullptr)
            return nullptr;
    }

    if (!psOptions->osWHERE.empty())
    {
        // Hack for GMLAS driver
        if (EQUAL(poDriver->GetDescription(), "GMLAS"))
        {
            if (psOptions->aosLayers.empty())
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "-where not supported by this output driver "
                         "without explicit layer name(s)");
                if (poWrkSrcDS != poDS)
                    delete poWrkSrcDS;
                return nullptr;
            }
            else
            {
                for (const char *pszLayer : psOptions->aosLayers)
                {
                    OGRLayer *poSrcLayer = poDS->GetLayerByName(pszLayer);
                    if (poSrcLayer != nullptr)
                    {
                        poSrcLayer->SetAttributeFilter(
                            psOptions->osWHERE.c_str());
                    }
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg, "-where");
            if (poWrkSrcDS != poDS)
                delete poWrkSrcDS;
            return nullptr;
        }
    }

    if (psOptions->poSpatialFilter)
    {
        for (int i = 0; i < poWrkSrcDS->GetLayerCount(); ++i)
        {
            OGRLayer *poSrcLayer = poWrkSrcDS->GetLayer(i);
            if (poSrcLayer &&
                poSrcLayer->GetLayerDefn()->GetGeomFieldCount() > 0 &&
                (psOptions->aosLayers.empty() ||
                 psOptions->aosLayers.FindString(poSrcLayer->GetName()) >= 0))
            {
                if (psOptions->bGeomFieldSet)
                {
                    const int iGeomField =
                        poSrcLayer->GetLayerDefn()->GetGeomFieldIndex(
                            psOptions->osGeomField.c_str());
                    if (iGeomField >= 0)
                        poSrcLayer->SetSpatialFilter(
                            iGeomField, psOptions->poSpatialFilter.get());
                    else
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot find geometry field %s in layer %s. "
                                 "Applying to first geometry field",
                                 psOptions->osGeomField.c_str(),
                                 poSrcLayer->GetName());
                }
                else
                {
                    poSrcLayer->SetSpatialFilter(
                        psOptions->poSpatialFilter.get());
                }
            }
        }
    }

    CPLStringList aosDSCO(psOptions->aosDSCO);
    if (!psOptions->aosLayers.empty())
    {
        // Hack for GMLAS driver
        if (EQUAL(poDriver->GetDescription(), "GMLAS"))
        {
            CPLString osLayers;
            for (const char *pszLayer : psOptions->aosLayers)
            {
                if (!osLayers.empty())
                    osLayers += ",";
                osLayers += pszLayer;
            }
            aosDSCO.SetNameValue("LAYERS", osLayers);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, szErrorMsg,
                     "Specifying layers");
            if (poWrkSrcDS != poDS)
                delete poWrkSrcDS;
            return nullptr;
        }
    }

    // Hack for GMLAS driver (this speed up deletion by avoiding the GML
    // driver to try parsing a pre-existing file). Could be potentially
    // removed if the GML driver implemented fast dataset opening (ie
    // without parsing) and GetFileList()
    if (EQUAL(poDriver->GetDescription(), "GMLAS"))
    {
        GDALDriverH hIdentifyingDriver = GDALIdentifyDriver(pszDest, nullptr);
        if (hIdentifyingDriver != nullptr &&
            EQUAL(GDALGetDescription(hIdentifyingDriver), "GML"))
        {
            VSIUnlink(pszDest);
            VSIUnlink(CPLResetExtension(pszDest, "gfs"));
        }
    }

    GDALDataset *poOut =
        poDriver->CreateCopy(pszDest, poWrkSrcDS, FALSE, aosDSCO.List(),
                             psOptions->pfnProgress, psOptions->pProgressData);

    if (poWrkSrcDS != poDS)
        delete poWrkSrcDS;

    return poOut;
}

/************************************************************************/
/*                           GDALVectorTranslate()                      */
/************************************************************************/
/**
 * Converts vector data between file formats.
 *
 * This is the equivalent of the <a href="/programs/ogr2ogr.html">ogr2ogr</a>
 * utility.
 *
 * GDALVectorTranslateOptions* must be allocated and freed with
 * GDALVectorTranslateOptionsNew() and GDALVectorTranslateOptionsFree()
 * respectively. pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param nSrcCount the number of input datasets (only 1 supported currently)
 * @param pahSrcDS the list of input datasets.
 * @param psOptionsIn the options struct returned by
 * GDALVectorTranslateOptionsNew() or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred, or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALVectorTranslate(const char *pszDest, GDALDatasetH hDstDS,
                                 int nSrcCount, GDALDatasetH *pahSrcDS,
                                 const GDALVectorTranslateOptions *psOptionsIn,
                                 int *pbUsageError)

{
    if (pszDest == nullptr && hDstDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pszDest == NULL && hDstDS == NULL");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (nSrcCount != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nSrcCount != 1");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALDatasetH hSrcDS = pahSrcDS[0];
    if (hSrcDS == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hSrcDS == NULL");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    auto psOptions =
        psOptionsIn ? std::make_unique<GDALVectorTranslateOptions>(*psOptionsIn)
                    : std::make_unique<GDALVectorTranslateOptions>();

    bool bAppend = false;
    bool bUpdate = false;
    bool bOverwrite = false;

    if (psOptions->eAccessMode == ACCESS_UPDATE)
    {
        bUpdate = true;
    }
    else if (psOptions->eAccessMode == ACCESS_APPEND)
    {
        bAppend = true;
        bUpdate = true;
    }
    else if (psOptions->eAccessMode == ACCESS_OVERWRITE)
    {
        bOverwrite = true;
        bUpdate = true;
    }
    else if (hDstDS != nullptr)
    {
        bUpdate = true;
    }

    const CPLString osDateLineOffset =
        CPLOPrintf("%g", psOptions->dfDateLineOffset);

    if (psOptions->bPreserveFID && psOptions->bExplodeCollections)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "cannot use -preserve_fid and -explodecollections at the same "
                 "time.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (!psOptions->aosFieldMap.empty() && !bAppend)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "if -fieldmap is specified, -append must also be specified");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (!psOptions->aosFieldMap.empty() && psOptions->bAddMissingFields)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "if -addfields is specified, -fieldmap cannot be used.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (psOptions->bSelFieldsSet && bAppend && !psOptions->bAddMissingFields)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "if -append is specified, -select cannot be used "
                 "(use -fieldmap or -sql instead).");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (!psOptions->aosFieldTypesToString.empty() &&
        !psOptions->aosMapFieldType.empty())
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-fieldTypeToString and -mapFieldType are exclusive.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (!psOptions->osSourceSRSDef.empty() &&
        psOptions->osOutputSRSDef.empty() && psOptions->osSpatSRSDef.empty())
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "if -s_srs is specified, -t_srs and/or -spat_srs must also be "
                 "specified.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Parse spatial filter SRS if needed.                             */
    /* -------------------------------------------------------------------- */
    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poSpatSRS;
    if (psOptions->poSpatialFilter && !psOptions->osSpatSRSDef.empty())
    {
        if (!psOptions->osSQLStatement.empty())
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-spat_srs not compatible with -sql.");
            return nullptr;
        }
        OGREnvelope sEnvelope;
        psOptions->poSpatialFilter->getEnvelope(&sEnvelope);
        poSpatSRS.reset(new OGRSpatialReference());
        poSpatSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSpatSRS->SetFromUserInput(psOptions->osSpatSRSDef.c_str()) !=
            OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to process SRS definition: %s",
                     psOptions->osSpatSRSDef.c_str());
            return nullptr;
        }
    }

    if (!psOptions->poClipSrc && !psOptions->osClipSrcDS.empty())
    {
        psOptions->poClipSrc =
            LoadGeometry(psOptions->osClipSrcDS, psOptions->osClipSrcSQL,
                         psOptions->osClipSrcLayer, psOptions->osClipSrcWhere);
        if (psOptions->poClipSrc == nullptr)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "cannot load source clip geometry");
            return nullptr;
        }
    }
    else if (psOptions->bClipSrc && !psOptions->poClipSrc &&
             psOptions->poSpatialFilter)
    {
        psOptions->poClipSrc.reset(psOptions->poSpatialFilter->clone());
        if (poSpatSRS)
        {
            psOptions->poClipSrc->assignSpatialReference(poSpatSRS.get());
        }
    }
    else if (psOptions->bClipSrc && !psOptions->poClipSrc)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-clipsrc must be used with -spat option or a\n"
                 "bounding box, WKT string or datasource must be specified");
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (!psOptions->osClipDstDS.empty())
    {
        psOptions->poClipDst =
            LoadGeometry(psOptions->osClipDstDS, psOptions->osClipDstSQL,
                         psOptions->osClipDstLayer, psOptions->osClipDstWhere);
        if (psOptions->poClipDst == nullptr)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "cannot load dest clip geometry");
            return nullptr;
        }
    }

    GDALDataset *poDS = GDALDataset::FromHandle(hSrcDS);
    GDALDataset *poODS = nullptr;
    GDALDriver *poDriver = nullptr;
    CPLString osDestFilename;

    if (hDstDS)
    {
        poODS = GDALDataset::FromHandle(hDstDS);
        osDestFilename = poODS->GetDescription();
    }
    else
    {
        osDestFilename = pszDest;
    }

    /* Various tests to avoid overwriting the source layer(s) */
    /* or to avoid appending a layer to itself */
    if (bUpdate && strcmp(osDestFilename, poDS->GetDescription()) == 0 &&
        !EQUAL(poDS->GetDriverName(), "Memory") && (bOverwrite || bAppend))
    {
        bool bError = false;
        if (psOptions->osNewLayerName.empty())
            bError = true;
        else if (psOptions->aosLayers.size() == 1)
            bError = strcmp(psOptions->osNewLayerName.c_str(),
                            psOptions->aosLayers[0]) == 0;
        else if (psOptions->osSQLStatement.empty())
            bError = true;
        if (bError)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-nln name must be specified combined with "
                     "a single source layer name,\nor a -sql statement, and "
                     "name must be different from an existing layer.");
            return nullptr;
        }
    }
    else if (!bUpdate && strcmp(osDestFilename, poDS->GetDescription()) == 0 &&
             (psOptions->osFormat.empty() ||
              !EQUAL(psOptions->osFormat.c_str(), "Memory")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Source and destination datasets must be different "
                 "in non-update mode.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Try opening the output datasource as an existing, writable      */
    /* -------------------------------------------------------------------- */
    std::vector<std::string> aoDrivers;
    if (poODS == nullptr && psOptions->osFormat.empty())
    {
        aoDrivers = GetOutputDriversFor(pszDest, GDAL_OF_VECTOR);
        if (!bUpdate && aoDrivers.size() == 1)
        {
            GDALDriverH hDriver = GDALGetDriverByName(aoDrivers[0].c_str());
            const char *pszPrefix = GDALGetMetadataItem(
                hDriver, GDAL_DMD_CONNECTION_PREFIX, nullptr);
            if (pszPrefix && STARTS_WITH_CI(pszDest, pszPrefix))
            {
                bUpdate = true;
            }
        }
    }

    if (bUpdate && poODS == nullptr)
    {
        poODS = GDALDataset::Open(
            osDestFilename, GDAL_OF_UPDATE | GDAL_OF_VECTOR, nullptr,
            psOptions->aosDestOpenOptions.List(), nullptr);

        if (poODS == nullptr)
        {
            if (bOverwrite || bAppend)
            {
                poODS = GDALDataset::Open(
                    osDestFilename, GDAL_OF_VECTOR, nullptr,
                    psOptions->aosDestOpenOptions.List(), nullptr);
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
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to open existing output datasource `%s'.",
                         osDestFilename.c_str());
                return nullptr;
            }
        }
        else if (psOptions->aosDSCO.size() > 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Datasource creation options ignored since an existing "
                     "datasource\n"
                     "         being updated.");
        }
    }

    if (poODS)
        poDriver = poODS->GetDriver();

    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */
    bool bNewDataSource = false;
    if (!bUpdate)
    {
        GDALDriverManager *poDM = GetGDALDriverManager();

        if (psOptions->osFormat.empty())
        {
            if (aoDrivers.empty())
            {
                if (EQUAL(CPLGetExtension(pszDest), ""))
                {
                    psOptions->osFormat = "ESRI Shapefile";
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot guess driver for %s", pszDest);
                    return nullptr;
                }
            }
            else
            {
                if (aoDrivers.size() > 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Several drivers matching %s extension. Using %s",
                             CPLGetExtension(pszDest), aoDrivers[0].c_str());
                }
                psOptions->osFormat = aoDrivers[0];
            }
            CPLDebug("GDAL", "Using %s driver", psOptions->osFormat.c_str());
        }

        CPLString osOGRCompatFormat(psOptions->osFormat);
        // Special processing for non-unified drivers that have the same name
        // as GDAL and OGR drivers. GMT should become OGR_GMT.
        // Other candidates could be VRT, SDTS and PDS, but they don't
        // have write capabilities. But do the substitution to get a sensible
        // error message
        if (EQUAL(osOGRCompatFormat, "GMT") ||
            EQUAL(osOGRCompatFormat, "VRT") ||
            EQUAL(osOGRCompatFormat, "SDTS") || EQUAL(osOGRCompatFormat, "PDS"))
        {
            osOGRCompatFormat = "OGR_" + osOGRCompatFormat;
        }
        poDriver = poDM->GetDriverByName(osOGRCompatFormat);
        if (poDriver == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unable to find driver `%s'.",
                     psOptions->osFormat.c_str());
            return nullptr;
        }

        char **papszDriverMD = poDriver->GetMetadata();
        if (!CPLTestBool(
                CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_VECTOR, "FALSE")))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s driver has no vector capabilities.",
                     psOptions->osFormat.c_str());
            return nullptr;
        }

        if (poDriver->CanVectorTranslateFrom(
                pszDest, poDS, psOptions->aosArguments.List(), nullptr))
        {
            return poDriver->VectorTranslateFrom(
                pszDest, poDS, psOptions->aosArguments.List(),
                psOptions->pfnProgress, psOptions->pProgressData);
        }

        if (!CPLTestBool(
                CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE")))
        {
            if (CPLTestBool(CSLFetchNameValueDef(
                    papszDriverMD, GDAL_DCAP_CREATECOPY, "FALSE")))
            {
                poODS = GDALVectorTranslateCreateCopy(poDriver, pszDest, poDS,
                                                      psOptions.get());
                return poODS;
            }

            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s driver does not support data source creation.",
                     psOptions->osFormat.c_str());
            return nullptr;
        }

        if (!psOptions->aosDestOpenOptions.empty())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-doo ignored when creating the output datasource.");
        }

        /* --------------------------------------------------------------------
         */
        /*      Special case to improve user experience when translating */
        /*      a datasource with multiple layers into a shapefile. If the */
        /*      user gives a target datasource with .shp and it does not exist,
         */
        /*      the shapefile driver will try to create a file, but this is not
         */
        /*      appropriate because here we have several layers, so create */
        /*      a directory instead. */
        /* --------------------------------------------------------------------
         */
        VSIStatBufL sStat;
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            psOptions->osSQLStatement.empty() &&
            (psOptions->aosLayers.size() > 1 ||
             (psOptions->aosLayers.empty() && poDS->GetLayerCount() > 1)) &&
            psOptions->osNewLayerName.empty() &&
            EQUAL(CPLGetExtension(osDestFilename), "SHP") &&
            VSIStatL(osDestFilename, &sStat) != 0)
        {
            if (VSIMkdir(osDestFilename, 0755) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create directory %s\n"
                         "for shapefile datastore.",
                         osDestFilename.c_str());
                return nullptr;
            }
        }

        CPLStringList aosDSCO(psOptions->aosDSCO);

        if (!aosDSCO.FetchNameValue("SINGLE_LAYER"))
        {
            // Informs the target driver (e.g. JSONFG) if a single layer
            // will be created
            const char *pszCOList =
                poDriver->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
            if (pszCOList && strstr(pszCOList, "SINGLE_LAYER") &&
                (!psOptions->osSQLStatement.empty() ||
                 psOptions->aosLayers.size() == 1 ||
                 (psOptions->aosLayers.empty() && poDS->GetLayerCount() == 1)))
            {
                aosDSCO.SetNameValue("SINGLE_LAYER", "YES");
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Create the output data source. */
        /* --------------------------------------------------------------------
         */
        poODS = poDriver->Create(osDestFilename, 0, 0, 0, GDT_Unknown,
                                 aosDSCO.List());
        if (poODS == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s driver failed to create %s",
                     psOptions->osFormat.c_str(), osDestFilename.c_str());
            return nullptr;
        }
        bNewDataSource = true;

        if (psOptions->bCopyMD)
        {
            const CPLStringList aosDomains(poDS->GetMetadataDomainList());
            for (const char *pszMD : aosDomains)
            {
                if (char **papszMD = poDS->GetMetadata(pszMD))
                    poODS->SetMetadata(papszMD, pszMD);
            }
        }
        for (const auto &[pszKey, pszValue] :
             cpl::IterateNameValue(psOptions->aosMetadataOptions))
        {
            poODS->SetMetadataItem(pszKey, pszValue);
        }

        // When writing to GeoJSON and using -nln, set the @NAME layer
        // creation option to avoid the GeoJSON driver to potentially reuse
        // the source feature collection name if the input is also GeoJSON.
        if (!psOptions->osNewLayerName.empty() &&
            EQUAL(psOptions->osFormat.c_str(), "GeoJSON"))
        {
            psOptions->aosLCO.SetNameValue("@NAME",
                                           psOptions->osNewLayerName.c_str());
        }
    }

    // Automatically close poODS on error, if it has been created by this
    // method.
    GDALDatasetUniquePtr poODSUniquePtr(hDstDS == nullptr ? poODS : nullptr);

    // Some syntaxic sugar to make "ogr2ogr [-f PostgreSQL] PG:dbname=....
    // source [srclayer] -lco OVERWRITE=YES" work like "ogr2ogr -overwrite
    // PG:dbname=.... source [srclayer]" The former syntax used to work at
    // GDAL 1.1.8 time when it was documented in the PG driver, but was broken
    // starting with GDAL 1.3.2
    // (https://github.com/OSGeo/gdal/commit/29c108a6c9f651dfebae6d1313ba0e707a77c1aa)
    // This could probably be generalized to other drivers that support the
    // OVERWRITE layer creation option, but we'd need to make sure that they
    // just do a DeleteLayer() call. The CARTO driver is an exception regarding
    // that.
    if (EQUAL(poODS->GetDriver()->GetDescription(), "PostgreSQL") &&
        CPLTestBool(psOptions->aosLCO.FetchNameValueDef("OVERWRITE", "NO")))
    {
        if (bAppend)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "-append and -lco OVERWRITE=YES are mutually exclusive");
            return nullptr;
        }
        bOverwrite = true;
    }

    /* -------------------------------------------------------------------- */
    /*      For random reading                                              */
    /* -------------------------------------------------------------------- */
    const bool bRandomLayerReading =
        CPL_TO_BOOL(poDS->TestCapability(ODsCRandomLayerRead));
    if (bRandomLayerReading && !poODS->TestCapability(ODsCRandomLayerWrite) &&
        psOptions->aosLayers.size() != 1 && psOptions->osSQLStatement.empty() &&
        !psOptions->bQuiet)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Input datasource uses random layer reading, but "
                 "output datasource does not support random layer writing");
    }

    if (psOptions->nLayerTransaction < 0)
    {
        if (bRandomLayerReading)
            psOptions->nLayerTransaction = FALSE;
        else
            psOptions->nLayerTransaction =
                !poODS->TestCapability(ODsCTransactions);
    }
    else if (psOptions->nLayerTransaction && bRandomLayerReading)
    {
        psOptions->nLayerTransaction = false;
    }

    /* -------------------------------------------------------------------- */
    /*      Parse the output SRS definition if possible.                    */
    /* -------------------------------------------------------------------- */
    OGR2OGRSpatialReferenceHolder oOutputSRSHolder;
    if (!psOptions->osOutputSRSDef.empty())
    {
        oOutputSRSHolder.assignNoRefIncrease(new OGRSpatialReference());
        oOutputSRSHolder.get()->SetAxisMappingStrategy(
            OAMS_TRADITIONAL_GIS_ORDER);
        if (oOutputSRSHolder.get()->SetFromUserInput(
                psOptions->osOutputSRSDef.c_str()) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to process SRS definition: %s",
                     psOptions->osOutputSRSDef.c_str());
            return nullptr;
        }
        oOutputSRSHolder.get()->SetCoordinateEpoch(
            psOptions->dfOutputCoordinateEpoch);
    }

    /* -------------------------------------------------------------------- */
    /*      Parse the source SRS definition if possible.                    */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference oSourceSRS;
    OGRSpatialReference *poSourceSRS = nullptr;
    if (!psOptions->osSourceSRSDef.empty())
    {
        oSourceSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (oSourceSRS.SetFromUserInput(psOptions->osSourceSRSDef.c_str()) !=
            OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to process SRS definition: %s",
                     psOptions->osSourceSRSDef.c_str());
            return nullptr;
        }
        oSourceSRS.SetCoordinateEpoch(psOptions->dfSourceCoordinateEpoch);
        poSourceSRS = &oSourceSRS;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a transformation object from the source to               */
    /*      destination coordinate system.                                  */
    /* -------------------------------------------------------------------- */
    std::unique_ptr<GCPCoordTransformation> poGCPCoordTrans;
    if (psOptions->oGCPs.nGCPCount > 0)
    {
        poGCPCoordTrans = std::make_unique<GCPCoordTransformation>(
            psOptions->oGCPs.nGCPCount, psOptions->oGCPs.pasGCPs,
            psOptions->nTransformOrder,
            poSourceSRS ? poSourceSRS : oOutputSRSHolder.get());
        if (!(poGCPCoordTrans->IsValid()))
        {
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create layer setup and transformer objects.                     */
    /* -------------------------------------------------------------------- */
    SetupTargetLayer oSetup;
    oSetup.m_poSrcDS = poDS;
    oSetup.m_poDstDS = poODS;
    oSetup.m_papszLCO = psOptions->aosLCO.List();
    oSetup.m_poOutputSRS = oOutputSRSHolder.get();
    oSetup.m_bTransform = psOptions->bTransform;
    oSetup.m_bNullifyOutputSRS = psOptions->bNullifyOutputSRS;
    oSetup.m_bSelFieldsSet = psOptions->bSelFieldsSet;
    oSetup.m_papszSelFields = psOptions->aosSelFields.List();
    oSetup.m_bAppend = bAppend;
    oSetup.m_bAddMissingFields = psOptions->bAddMissingFields;
    oSetup.m_eGType = psOptions->eGType;
    oSetup.m_eGeomTypeConversion = psOptions->eGeomTypeConversion;
    oSetup.m_nCoordDim = psOptions->nCoordDim;
    oSetup.m_bOverwrite = bOverwrite;
    oSetup.m_papszFieldTypesToString = psOptions->aosFieldTypesToString.List();
    oSetup.m_papszMapFieldType = psOptions->aosMapFieldType.List();
    oSetup.m_bUnsetFieldWidth = psOptions->bUnsetFieldWidth;
    oSetup.m_bExplodeCollections = psOptions->bExplodeCollections;
    oSetup.m_pszZField =
        psOptions->osZField.empty() ? nullptr : psOptions->osZField.c_str();
    oSetup.m_papszFieldMap = psOptions->aosFieldMap.List();
    oSetup.m_pszWHERE =
        psOptions->osWHERE.empty() ? nullptr : psOptions->osWHERE.c_str();
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
    oSetup.m_pszCTPipeline = psOptions->osCTPipeline.empty()
                                 ? nullptr
                                 : psOptions->osCTPipeline.c_str();

    LayerTranslator oTranslator;
    oTranslator.m_poSrcDS = poDS;
    oTranslator.m_poODS = poODS;
    oTranslator.m_bTransform = psOptions->bTransform;
    oTranslator.m_bWrapDateline = psOptions->bWrapDateline;
    oTranslator.m_osDateLineOffset = osDateLineOffset;
    oTranslator.m_poOutputSRS = oOutputSRSHolder.get();
    oTranslator.m_bNullifyOutputSRS = psOptions->bNullifyOutputSRS;
    oTranslator.m_poUserSourceSRS = poSourceSRS;
    oTranslator.m_poGCPCoordTrans = poGCPCoordTrans.get();
    oTranslator.m_eGType = psOptions->eGType;
    oTranslator.m_eGeomTypeConversion = psOptions->eGeomTypeConversion;
    oTranslator.m_bMakeValid = psOptions->bMakeValid;
    oTranslator.m_nCoordDim = psOptions->nCoordDim;
    oTranslator.m_eGeomOp = psOptions->eGeomOp;
    oTranslator.m_dfGeomOpParam = psOptions->dfGeomOpParam;
    // Do not emit warning if the user specified directly the clip source geom
    if (psOptions->osClipSrcDS.empty())
        oTranslator.m_bWarnedClipSrcSRS = true;
    oTranslator.m_poClipSrcOri = psOptions->poClipSrc.get();
    // Do not emit warning if the user specified directly the clip dest geom
    if (psOptions->osClipDstDS.empty())
        oTranslator.m_bWarnedClipDstSRS = true;
    oTranslator.m_poClipDstOri = psOptions->poClipDst.get();
    oTranslator.m_bExplodeCollections = psOptions->bExplodeCollections;
    oTranslator.m_bNativeData = psOptions->bNativeData;
    oTranslator.m_nLimit = psOptions->nLimit;

    if (psOptions->nGroupTransactions)
    {
        if (!psOptions->nLayerTransaction)
            poODS->StartTransaction(psOptions->bForceTransaction);
    }

    GIntBig nTotalEventsDone = 0;

    /* -------------------------------------------------------------------- */
    /*      Special case for -sql clause.  No source layers required.       */
    /* -------------------------------------------------------------------- */
    int nRetCode = 0;

    if (!psOptions->osSQLStatement.empty())
    {
        /* Special case: if output=input, then we must likely destroy the */
        /* old table before to avoid transaction issues. */
        if (poDS == poODS && !psOptions->osNewLayerName.empty() && bOverwrite)
            GetLayerAndOverwriteIfNecessary(
                poODS, psOptions->osNewLayerName.c_str(), bOverwrite, nullptr,
                nullptr, nullptr);

        if (!psOptions->osWHERE.empty())
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-where clause ignored in combination with -sql.");
        if (psOptions->aosLayers.size() > 0)
            CPLError(CE_Warning, CPLE_AppDefined,
                     "layer names ignored in combination with -sql.");

        OGRLayer *poResultSet = poDS->ExecuteSQL(
            psOptions->osSQLStatement.c_str(),
            (!psOptions->bGeomFieldSet) ? psOptions->poSpatialFilter.get()
                                        : nullptr,
            psOptions->osDialect.empty() ? nullptr
                                         : psOptions->osDialect.c_str());

        if (poResultSet != nullptr)
        {
            if (psOptions->poSpatialFilter && psOptions->bGeomFieldSet)
            {
                int iGeomField = poResultSet->GetLayerDefn()->GetGeomFieldIndex(
                    psOptions->osGeomField.c_str());
                if (iGeomField >= 0)
                    poResultSet->SetSpatialFilter(
                        iGeomField, psOptions->poSpatialFilter.get());
                else
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot find geometry field %s.",
                             psOptions->osGeomField.c_str());
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
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Progress turned off as fast feature count is not "
                             "available.");
                    psOptions->bDisplayProgress = false;
                }
                else
                {
                    nCountLayerFeatures = poResultSet->GetFeatureCount();
                    pfnProgress = psOptions->pfnProgress;
                    pProgressArg = psOptions->pProgressData;
                }
            }

            OGRLayer *poPassedLayer = poResultSet;
            if (psOptions->bSplitListFields)
            {
                auto poLayer = new OGRSplitListFieldLayer(
                    poPassedLayer, psOptions->nMaxSplitListSubFields);
                poPassedLayer = poLayer;
                int nRet = poLayer->BuildLayerDefn(nullptr, nullptr);
                if (!nRet)
                {
                    delete poPassedLayer;
                    poPassedLayer = poResultSet;
                }
            }

            /* --------------------------------------------------------------------
             */
            /*      Special case to improve user experience when translating
             * into   */
            /*      single file shapefile and source has only one layer, and
             * that   */
            /*      the layer name isn't specified */
            /* --------------------------------------------------------------------
             */
            VSIStatBufL sStat;
            if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
                psOptions->osNewLayerName.empty() &&
                VSIStatL(osDestFilename, &sStat) == 0 &&
                VSI_ISREG(sStat.st_mode) &&
                (EQUAL(CPLGetExtension(osDestFilename), "shp") ||
                 EQUAL(CPLGetExtension(osDestFilename), "shz") ||
                 EQUAL(CPLGetExtension(osDestFilename), "dbf")))
            {
                psOptions->osNewLayerName = CPLGetBasename(osDestFilename);
            }

            auto psInfo = oSetup.Setup(poPassedLayer,
                                       psOptions->osNewLayerName.empty()
                                           ? nullptr
                                           : psOptions->osNewLayerName.c_str(),
                                       psOptions.get(), nTotalEventsDone);

            poPassedLayer->ResetReading();

            if (psInfo == nullptr ||
                !oTranslator.Translate(nullptr, psInfo.get(),
                                       nCountLayerFeatures, nullptr,
                                       nTotalEventsDone, pfnProgress,
                                       pProgressArg, psOptions.get()))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Terminating translation prematurely after failed\n"
                         "translation from sql statement.");

                nRetCode = 1;
            }

            if (poPassedLayer != poResultSet)
                delete poPassedLayer;

            poDS->ReleaseResultSet(poResultSet);
        }
        else
        {
            if (CPLGetLastErrorNo() != 0)
                nRetCode = 1;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for layer interleaving mode.                       */
    /* -------------------------------------------------------------------- */
    else if (bRandomLayerReading)
    {
        if (psOptions->bSplitListFields)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "-splitlistfields not supported in this mode");
            return nullptr;
        }

        // Make sure to probe all layers in case some are by default invisible
        for (const char *pszLayer : psOptions->aosLayers)
        {
            OGRLayer *poLayer = poDS->GetLayerByName(pszLayer);

            if (poLayer == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Couldn't fetch requested layer %s!", pszLayer);
                return nullptr;
            }
        }

        const int nSrcLayerCount = poDS->GetLayerCount();
        std::vector<AssociatedLayers> pasAssocLayers(nSrcLayerCount);

        /* --------------------------------------------------------------------
         */
        /*      Special case to improve user experience when translating into */
        /*      single file shapefile and source has only one layer, and that */
        /*      the layer name isn't specified */
        /* --------------------------------------------------------------------
         */
        VSIStatBufL sStat;
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            (psOptions->aosLayers.size() == 1 || nSrcLayerCount == 1) &&
            psOptions->osNewLayerName.empty() &&
            VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode) &&
            (EQUAL(CPLGetExtension(osDestFilename), "shp") ||
             EQUAL(CPLGetExtension(osDestFilename), "shz") ||
             EQUAL(CPLGetExtension(osDestFilename), "dbf")))
        {
            psOptions->osNewLayerName = CPLGetBasename(osDestFilename);
        }

        GDALProgressFunc pfnProgress = nullptr;
        void *pProgressArg = nullptr;
        if (!psOptions->bQuiet)
        {
            pfnProgress = psOptions->pfnProgress;
            pProgressArg = psOptions->pProgressData;
        }

        /* --------------------------------------------------------------------
         */
        /*      If no target layer specified, use all source layers. */
        /* --------------------------------------------------------------------
         */
        if (psOptions->aosLayers.empty())
        {
            for (int iLayer = 0; iLayer < nSrcLayerCount; iLayer++)
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if (poLayer == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch advertised layer %d!", iLayer);
                    return nullptr;
                }

                psOptions->aosLayers.AddString(poLayer->GetName());
            }
        }
        else
        {
            const bool bSrcIsOSM = (strcmp(poDS->GetDriverName(), "OSM") == 0);
            if (bSrcIsOSM)
            {
                CPLString osInterestLayers = "SET interest_layers =";
                for (int iLayer = 0; iLayer < psOptions->aosLayers.size();
                     iLayer++)
                {
                    if (iLayer != 0)
                        osInterestLayers += ",";
                    osInterestLayers += psOptions->aosLayers[iLayer];
                }

                poDS->ExecuteSQL(osInterestLayers.c_str(), nullptr, nullptr);
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      First pass to set filters. */
        /* --------------------------------------------------------------------
         */
        std::map<OGRLayer *, int> oMapLayerToIdx;

        for (int iLayer = 0; iLayer < nSrcLayerCount; iLayer++)
        {
            OGRLayer *poLayer = poDS->GetLayer(iLayer);
            if (poLayer == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Couldn't fetch advertised layer %d!", iLayer);
                return nullptr;
            }

            pasAssocLayers[iLayer].poSrcLayer = poLayer;

            if (psOptions->aosLayers.FindString(poLayer->GetName()) >= 0)
            {
                if (!psOptions->osWHERE.empty())
                {
                    if (poLayer->SetAttributeFilter(
                            psOptions->osWHERE.c_str()) != OGRERR_NONE)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "SetAttributeFilter(%s) on layer '%s' failed.",
                                 psOptions->osWHERE.c_str(),
                                 poLayer->GetName());
                        if (!psOptions->bSkipFailures)
                        {
                            return nullptr;
                        }
                    }
                }

                ApplySpatialFilter(
                    poLayer, psOptions->poSpatialFilter.get(), poSpatSRS.get(),
                    psOptions->bGeomFieldSet ? psOptions->osGeomField.c_str()
                                             : nullptr,
                    poSourceSRS);

                oMapLayerToIdx[poLayer] = iLayer;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Second pass to process features in a interleaved layer mode. */
        /* --------------------------------------------------------------------
         */
        bool bTargetLayersHaveBeenCreated = false;
        while (true)
        {
            OGRLayer *poFeatureLayer = nullptr;
            auto poFeature = std::unique_ptr<OGRFeature>(poDS->GetNextFeature(
                &poFeatureLayer, nullptr, pfnProgress, pProgressArg));
            if (poFeature == nullptr)
                break;
            std::map<OGRLayer *, int>::const_iterator oIter =
                oMapLayerToIdx.find(poFeatureLayer);
            if (oIter == oMapLayerToIdx.end())
            {
                // Feature in a layer that is not a layer of interest.
                // nothing to do
            }
            else
            {
                if (!bTargetLayersHaveBeenCreated)
                {
                    // We defer target layer creation at the first feature
                    // retrieved since getting the layer definition can be
                    // costly (case of the GMLAS driver) and thus we'd better
                    // taking advantage from the progress callback of
                    // GetNextFeature.
                    bTargetLayersHaveBeenCreated = true;
                    for (int iLayer = 0; iLayer < nSrcLayerCount; iLayer++)
                    {
                        OGRLayer *poLayer = poDS->GetLayer(iLayer);
                        if (psOptions->aosLayers.FindString(
                                poLayer->GetName()) < 0)
                            continue;

                        auto psInfo = oSetup.Setup(
                            poLayer,
                            psOptions->osNewLayerName.empty()
                                ? nullptr
                                : psOptions->osNewLayerName.c_str(),
                            psOptions.get(), nTotalEventsDone);

                        if (psInfo == nullptr && !psOptions->bSkipFailures)
                        {
                            return nullptr;
                        }

                        pasAssocLayers[iLayer].psInfo = std::move(psInfo);
                    }
                    if (nRetCode)
                        break;
                }

                int iLayer = oIter->second;
                TargetLayerInfo *psInfo = pasAssocLayers[iLayer].psInfo.get();
                if ((psInfo == nullptr ||
                     !oTranslator.Translate(poFeature.release(), psInfo, 0,
                                            nullptr, nTotalEventsDone, nullptr,
                                            nullptr, psOptions.get())) &&
                    !psOptions->bSkipFailures)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Terminating translation prematurely after failed\n"
                        "translation of layer %s (use -skipfailures to skip "
                        "errors)",
                        poFeatureLayer->GetName());

                    nRetCode = 1;
                    break;
                }
            }
        }  // while true

        if (pfnProgress)
        {
            pfnProgress(1.0, "", pProgressArg);
        }

        if (!bTargetLayersHaveBeenCreated)
        {
            // bTargetLayersHaveBeenCreated not used after here.
            // bTargetLayersHaveBeenCreated = true;
            for (int iLayer = 0; iLayer < nSrcLayerCount; iLayer++)
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);
                if (psOptions->aosLayers.FindString(poLayer->GetName()) < 0)
                    continue;

                auto psInfo =
                    oSetup.Setup(poLayer,
                                 psOptions->osNewLayerName.empty()
                                     ? nullptr
                                     : psOptions->osNewLayerName.c_str(),
                                 psOptions.get(), nTotalEventsDone);

                if (psInfo == nullptr && !psOptions->bSkipFailures)
                {
                    return nullptr;
                }

                pasAssocLayers[iLayer].psInfo = std::move(psInfo);
            }
        }
    }

    else
    {
        std::vector<OGRLayer *> apoLayers;

        /* --------------------------------------------------------------------
         */
        /*      Process each data source layer. */
        /* --------------------------------------------------------------------
         */
        if (psOptions->aosLayers.empty())
        {
            const int nLayerCount = poDS->GetLayerCount();

            for (int iLayer = 0; iLayer < nLayerCount; iLayer++)
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if (poLayer == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch advertised layer %d!", iLayer);
                    return nullptr;
                }
                if (!poDS->IsLayerPrivate(iLayer))
                {
                    apoLayers.push_back(poLayer);
                }
            }
        }
        /* --------------------------------------------------------------------
         */
        /*      Process specified data source layers. */
        /* --------------------------------------------------------------------
         */
        else
        {

            for (int iLayer = 0; psOptions->aosLayers[iLayer] != nullptr;
                 iLayer++)
            {
                OGRLayer *poLayer =
                    poDS->GetLayerByName(psOptions->aosLayers[iLayer]);

                if (poLayer == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch requested layer '%s'!",
                             psOptions->aosLayers[iLayer]);
                    if (!psOptions->bSkipFailures)
                    {
                        return nullptr;
                    }
                }

                apoLayers.emplace_back(poLayer);
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Special case to improve user experience when translating into */
        /*      single file shapefile and source has only one layer, and that */
        /*      the layer name isn't specified */
        /* --------------------------------------------------------------------
         */
        VSIStatBufL sStat;
        const int nLayerCount = static_cast<int>(apoLayers.size());
        if (EQUAL(poDriver->GetDescription(), "ESRI Shapefile") &&
            nLayerCount == 1 && psOptions->osNewLayerName.empty() &&
            VSIStatL(osDestFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode) &&
            (EQUAL(CPLGetExtension(osDestFilename), "shp") ||
             EQUAL(CPLGetExtension(osDestFilename), "shz") ||
             EQUAL(CPLGetExtension(osDestFilename), "dbf")))
        {
            psOptions->osNewLayerName = CPLGetBasename(osDestFilename);
        }

        std::vector<GIntBig> anLayerCountFeatures;
        anLayerCountFeatures.resize(nLayerCount);
        GIntBig nCountLayersFeatures = 0;
        GIntBig nAccCountFeatures = 0;

        /* First pass to apply filters and count all features if necessary */
        for (int iLayer = 0; iLayer < nLayerCount; iLayer++)
        {
            OGRLayer *poLayer = apoLayers[iLayer];
            if (poLayer == nullptr)
                continue;

            if (!psOptions->osWHERE.empty())
            {
                if (poLayer->SetAttributeFilter(psOptions->osWHERE.c_str()) !=
                    OGRERR_NONE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "SetAttributeFilter(%s) on layer '%s' failed.",
                             psOptions->osWHERE.c_str(), poLayer->GetName());
                    if (!psOptions->bSkipFailures)
                    {
                        return nullptr;
                    }
                }
            }

            ApplySpatialFilter(
                poLayer, psOptions->poSpatialFilter.get(), poSpatSRS.get(),
                psOptions->bGeomFieldSet ? psOptions->osGeomField.c_str()
                                         : nullptr,
                poSourceSRS);

            if (psOptions->bDisplayProgress)
            {
                if (!poLayer->TestCapability(OLCFastFeatureCount))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Progress turned off as fast feature count is not "
                             "available.");
                    psOptions->bDisplayProgress = false;
                }
                else
                {
                    anLayerCountFeatures[iLayer] = poLayer->GetFeatureCount();
                    if (psOptions->nLimit >= 0)
                        anLayerCountFeatures[iLayer] = std::min(
                            anLayerCountFeatures[iLayer], psOptions->nLimit);
                    nCountLayersFeatures += anLayerCountFeatures[iLayer];
                }
            }
        }

        /* Second pass to do the real job */
        for (int iLayer = 0; iLayer < nLayerCount && nRetCode == 0; iLayer++)
        {
            OGRLayer *poLayer = apoLayers[iLayer];
            if (poLayer == nullptr)
                continue;

            GDALProgressFunc pfnProgress = nullptr;
            void *pProgressArg = nullptr;

            OGRLayer *poPassedLayer = poLayer;
            if (psOptions->bSplitListFields)
            {
                auto poSLFLayer = new OGRSplitListFieldLayer(
                    poPassedLayer, psOptions->nMaxSplitListSubFields);
                poPassedLayer = poSLFLayer;

                if (psOptions->bDisplayProgress &&
                    psOptions->nMaxSplitListSubFields != 1 &&
                    nCountLayersFeatures != 0)
                {
                    pfnProgress = GDALScaledProgress;
                    pProgressArg = GDALCreateScaledProgress(
                        nAccCountFeatures * 1.0 / nCountLayersFeatures,
                        (nAccCountFeatures + anLayerCountFeatures[iLayer] / 2) *
                            1.0 / nCountLayersFeatures,
                        psOptions->pfnProgress, psOptions->pProgressData);
                }
                else
                {
                    pfnProgress = nullptr;
                    pProgressArg = nullptr;
                }

                int nRet =
                    poSLFLayer->BuildLayerDefn(pfnProgress, pProgressArg);
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
                if (nCountLayersFeatures != 0)
                {
                    pfnProgress = GDALScaledProgress;
                    GIntBig nStart = 0;
                    if (poPassedLayer != poLayer &&
                        psOptions->nMaxSplitListSubFields != 1)
                        nStart = anLayerCountFeatures[iLayer] / 2;
                    pProgressArg = GDALCreateScaledProgress(
                        (nAccCountFeatures + nStart) * 1.0 /
                            nCountLayersFeatures,
                        (nAccCountFeatures + anLayerCountFeatures[iLayer]) *
                            1.0 / nCountLayersFeatures,
                        psOptions->pfnProgress, psOptions->pProgressData);
                }
            }

            nAccCountFeatures += anLayerCountFeatures[iLayer];

            auto psInfo = oSetup.Setup(poPassedLayer,
                                       psOptions->osNewLayerName.empty()
                                           ? nullptr
                                           : psOptions->osNewLayerName.c_str(),
                                       psOptions.get(), nTotalEventsDone);

            poPassedLayer->ResetReading();

            if ((psInfo == nullptr ||
                 !oTranslator.Translate(nullptr, psInfo.get(),
                                        anLayerCountFeatures[iLayer], nullptr,
                                        nTotalEventsDone, pfnProgress,
                                        pProgressArg, psOptions.get())) &&
                !psOptions->bSkipFailures)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Terminating translation prematurely after failed\n"
                         "translation of layer %s (use -skipfailures to skip "
                         "errors)",
                         poLayer->GetName());

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

    poODS->SetStyleTable(poDS->GetStyleTable());

    if (psOptions->nGroupTransactions)
    {
        if (!psOptions->nLayerTransaction)
        {
            if (nRetCode != 0 && !psOptions->bSkipFailures)
                poODS->RollbackTransaction();
            else
            {
                OGRErr eRet = poODS->CommitTransaction();
                if (eRet != OGRERR_NONE && eRet != OGRERR_UNSUPPORTED_OPERATION)
                {
                    nRetCode = 1;
                }
            }
        }
    }

    // Note: this guarantees that the file can be opened in a consistent state,
    // without requiring to close poODS, only if the driver declares
    // DCAP_FLUSHCACHE_CONSISTENT_STATE
    if (poODS->FlushCache() != CE_None)
        nRetCode = 1;

    if (nRetCode == 0)
    {
        if (hDstDS)
            return hDstDS;
        else
            return GDALDataset::ToHandle(poODSUniquePtr.release());
    }

    return nullptr;
}

/************************************************************************/
/*                               SetZ()                                 */
/************************************************************************/

namespace
{
class SetZVisitor : public OGRDefaultGeometryVisitor
{
    double m_dfZ;

  public:
    explicit SetZVisitor(double dfZ) : m_dfZ(dfZ)
    {
    }

    using OGRDefaultGeometryVisitor::visit;

    void visit(OGRPoint *poPoint) override
    {
        poPoint->setZ(m_dfZ);
    }
};
}  // namespace

static void SetZ(OGRGeometry *poGeom, double dfZ)
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
    if (nCoordDim == 2 && eGType != wkbNone)
        return wkbFlatten(eGType);
    else if (nCoordDim == 3 && eGType != wkbNone)
        return wkbSetZ(wkbFlatten(eGType));
    else if (nCoordDim == COORD_DIM_XYM && eGType != wkbNone)
        return wkbSetM(wkbFlatten(eGType));
    else if (nCoordDim == 4 && eGType != wkbNone)
        return OGR_GT_SetModifier(static_cast<OGRwkbGeometryType>(eGType), TRUE,
                                  TRUE);
    else
        return eGType;
}

/************************************************************************/
/*                   GetLayerAndOverwriteIfNecessary()                  */
/************************************************************************/

static OGRLayer *GetLayerAndOverwriteIfNecessary(GDALDataset *poDstDS,
                                                 const char *pszNewLayerName,
                                                 bool bOverwrite,
                                                 bool *pbErrorOccurred,
                                                 bool *pbOverwriteActuallyDone,
                                                 bool *pbAddOverwriteLCO)
{
    if (pbErrorOccurred)
        *pbErrorOccurred = false;
    if (pbOverwriteActuallyDone)
        *pbOverwriteActuallyDone = false;
    if (pbAddOverwriteLCO)
        *pbAddOverwriteLCO = false;

    /* GetLayerByName() can instantiate layers that would have been */
    /* 'hidden' otherwise, for example, non-spatial tables in a */
    /* PostGIS-enabled database, so this apparently useless command is */
    /* not useless. (#4012) */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRLayer *poDstLayer = poDstDS->GetLayerByName(pszNewLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    int iLayer = -1;
    if (poDstLayer != nullptr)
    {
        const int nLayerCount = poDstDS->GetLayerCount();
        for (iLayer = 0; iLayer < nLayerCount; iLayer++)
        {
            OGRLayer *poLayer = poDstDS->GetLayer(iLayer);
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
    if (poDstLayer != nullptr && bOverwrite)
    {
        /* When using the CARTO driver we don't want to delete the layer if */
        /* it's going to be recreated. Instead we mark it to be overwritten */
        /* when the new creation is requested */
        if (poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST) != nullptr &&
            strstr(poDstDS->GetDriver()->GetMetadataItem(
                       GDAL_DS_LAYER_CREATIONOPTIONLIST),
                   "CARTODBFY") != nullptr)
        {
            if (pbAddOverwriteLCO)
                *pbAddOverwriteLCO = true;
            if (pbOverwriteActuallyDone)
                *pbOverwriteActuallyDone = true;
        }
        else if (poDstDS->DeleteLayer(iLayer) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteLayer() failed when overwrite requested.");
            if (pbErrorOccurred)
                *pbErrorOccurred = true;
        }
        else
        {
            if (pbOverwriteActuallyDone)
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

    if (eGeomTypeConversion == GTC_CONVERT_TO_LINEAR ||
        eGeomTypeConversion == GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR)
    {
        eRetType = OGR_GT_GetLinear(eRetType);
    }

    if (eGeomTypeConversion == GTC_PROMOTE_TO_MULTI ||
        eGeomTypeConversion == GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR)
    {
        if (eRetType == wkbTriangle || eRetType == wkbTIN ||
            eRetType == wkbPolyhedralSurface)
        {
            eRetType = wkbMultiPolygon;
        }
        else if (!OGR_GT_IsSubClassOf(eRetType, wkbGeometryCollection))
        {
            eRetType = OGR_GT_GetCollection(eRetType);
        }
    }

    if (eGeomTypeConversion == GTC_CONVERT_TO_CURVE)
        eRetType = OGR_GT_GetCurve(eRetType);

    return eRetType;
}

/************************************************************************/
/*                        DoFieldTypeConversion()                       */
/************************************************************************/

static void DoFieldTypeConversion(GDALDataset *poDstDS,
                                  OGRFieldDefn &oFieldDefn,
                                  char **papszFieldTypesToString,
                                  char **papszMapFieldType,
                                  bool bUnsetFieldWidth, bool bQuiet,
                                  bool bForceNullable, bool bUnsetDefault)
{
    if (papszFieldTypesToString != nullptr)
    {
        CPLString osLookupString;
        osLookupString.Printf(
            "%s(%s)", OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
            OGRFieldDefn::GetFieldSubTypeName(oFieldDefn.GetSubType()));

        int iIdx = CSLFindString(papszFieldTypesToString, osLookupString);
        if (iIdx < 0)
            iIdx = CSLFindString(
                papszFieldTypesToString,
                OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()));
        if (iIdx < 0)
            iIdx = CSLFindString(papszFieldTypesToString, "All");
        if (iIdx >= 0)
        {
            oFieldDefn.SetSubType(OFSTNone);
            oFieldDefn.SetType(OFTString);
        }
    }
    else if (papszMapFieldType != nullptr)
    {
        CPLString osLookupString;
        osLookupString.Printf(
            "%s(%s)", OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
            OGRFieldDefn::GetFieldSubTypeName(oFieldDefn.GetSubType()));

        const char *pszType =
            CSLFetchNameValue(papszMapFieldType, osLookupString);
        if (pszType == nullptr)
            pszType = CSLFetchNameValue(
                papszMapFieldType,
                OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()));
        if (pszType == nullptr)
            pszType = CSLFetchNameValue(papszMapFieldType, "All");
        if (pszType != nullptr)
        {
            int iSubType;
            int iType = GetFieldType(pszType, &iSubType);
            if (iType >= 0 && iSubType >= 0)
            {
                oFieldDefn.SetSubType(OFSTNone);
                oFieldDefn.SetType(static_cast<OGRFieldType>(iType));
                oFieldDefn.SetSubType(static_cast<OGRFieldSubType>(iSubType));
                if (iType == OFTInteger)
                    oFieldDefn.SetWidth(0);
            }
        }
    }
    if (bUnsetFieldWidth)
    {
        oFieldDefn.SetWidth(0);
        oFieldDefn.SetPrecision(0);
    }
    if (bForceNullable)
        oFieldDefn.SetNullable(TRUE);
    if (bUnsetDefault)
        oFieldDefn.SetDefault(nullptr);

    const auto poDstDriver = poDstDS->GetDriver();
    const char *pszCreationFieldDataTypes =
        poDstDriver
            ? poDstDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES)
            : nullptr;
    const char *pszCreationFieldDataSubtypes =
        poDstDriver
            ? poDstDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES)
            : nullptr;
    if (pszCreationFieldDataTypes &&
        strstr(pszCreationFieldDataTypes,
               OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType())) == nullptr)
    {
        if (pszCreationFieldDataSubtypes &&
            (oFieldDefn.GetType() == OFTIntegerList ||
             oFieldDefn.GetType() == OFTInteger64List ||
             oFieldDefn.GetType() == OFTRealList ||
             oFieldDefn.GetType() == OFTStringList) &&
            strstr(pszCreationFieldDataSubtypes, "JSON"))
        {
            if (!bQuiet)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "The output driver does not seem to natively support %s "
                    "type for field %s. Converting it to String(JSON) instead. "
                    "-mapFieldType can be used to control field type "
                    "conversion.",
                    OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                    oFieldDefn.GetNameRef());
            }
            oFieldDefn.SetSubType(OFSTNone);
            oFieldDefn.SetType(OFTString);
            oFieldDefn.SetSubType(OFSTJSON);
        }
        else if (oFieldDefn.GetType() == OFTInteger64)
        {
            if (!bQuiet)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "The output driver does not seem to natively support %s "
                    "type for field %s. Converting it to Real instead. "
                    "-mapFieldType can be used to control field type "
                    "conversion.",
                    OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                    oFieldDefn.GetNameRef());
            }
            oFieldDefn.SetType(OFTReal);
        }
        else if (!bQuiet)
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "The output driver does not natively support %s type for "
                "field %s. Misconversion can happen. "
                "-mapFieldType can be used to control field type conversion.",
                OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                oFieldDefn.GetNameRef());
        }
    }
    else if (!pszCreationFieldDataTypes)
    {
        // All drivers supporting OFTInteger64 should advertise it theoretically
        if (oFieldDefn.GetType() == OFTInteger64)
        {
            if (!bQuiet)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The output driver does not seem to natively support "
                         "%s type "
                         "for field %s. Converting it to Real instead. "
                         "-mapFieldType can be used to control field type "
                         "conversion.",
                         OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()),
                         oFieldDefn.GetNameRef());
            }
            oFieldDefn.SetType(OFTReal);
        }
    }
}

/************************************************************************/
/*                 SetupTargetLayer::CanUseWriteArrowBatch()            */
/************************************************************************/

bool SetupTargetLayer::CanUseWriteArrowBatch(
    OGRLayer *poSrcLayer, OGRLayer *poDstLayer, bool bJustCreatedLayer,
    const GDALVectorTranslateOptions *psOptions, bool &bError)
{
    bError = false;

    // Check if we can use the Arrow interface to get and write features
    // as it will be faster if the input driver has a fast
    // implementation of GetArrowStream().
    // We also can only do that only if using ogr2ogr without options that
    // alter features.
    // OGR2OGR_USE_ARROW_API config option is mostly for testing purposes
    // or as a safety belt if things turned bad...
    bool bUseWriteArrowBatch = false;
    if (((poSrcLayer->TestCapability(OLCFastGetArrowStream) &&
          // As we don't control the input array size when the input or output
          // drivers are Arrow/Parquet (as they don't use the generic
          // implementation), we can't guarantee that ROW_GROUP_SIZE/BATCH_SIZE
          // layer creation options will be honored.
          !psOptions->aosLCO.FetchNameValue("ROW_GROUP_SIZE") &&
          !psOptions->aosLCO.FetchNameValue("BATCH_SIZE") &&
          CPLTestBool(CPLGetConfigOption("OGR2OGR_USE_ARROW_API", "YES"))) ||
         CPLTestBool(CPLGetConfigOption("OGR2OGR_USE_ARROW_API", "NO"))) &&
        !psOptions->bSkipFailures && !psOptions->bTransform &&
        !psOptions->poClipSrc && !psOptions->poClipDst &&
        psOptions->oGCPs.nGCPCount == 0 && !psOptions->bWrapDateline &&
        !m_papszSelFields && !m_bAddMissingFields &&
        m_eGType == GEOMTYPE_UNCHANGED && psOptions->eGeomOp == GEOMOP_NONE &&
        m_eGeomTypeConversion == GTC_DEFAULT && m_nCoordDim < 0 &&
        !m_papszFieldTypesToString && !m_papszMapFieldType &&
        !m_bUnsetFieldWidth && !m_bExplodeCollections && !m_pszZField &&
        m_bExactFieldNameMatch && !m_bForceNullable && !m_bResolveDomains &&
        !m_bUnsetDefault && psOptions->nFIDToFetch == OGRNullFID &&
        psOptions->dfXYRes == OGRGeomCoordinatePrecision::UNKNOWN &&
        !psOptions->bMakeValid)
    {
        struct ArrowArrayStream streamSrc;
        const char *const apszOptions[] = {"SILENCE_GET_SCHEMA_ERROR=YES",
                                           nullptr};
        if (poSrcLayer->GetArrowStream(&streamSrc, apszOptions))
        {
            struct ArrowSchema schemaSrc;
            if (streamSrc.get_schema(&streamSrc, &schemaSrc) == 0)
            {
                std::string osErrorMsg;
                if (poDstLayer->IsArrowSchemaSupported(&schemaSrc, nullptr,
                                                       osErrorMsg))
                {
                    const OGRFeatureDefn *poSrcFDefn =
                        poSrcLayer->GetLayerDefn();
                    const OGRFeatureDefn *poDstFDefn =
                        poDstLayer->GetLayerDefn();
                    if (bJustCreatedLayer && poDstFDefn &&
                        poDstFDefn->GetFieldCount() == 0 &&
                        poDstFDefn->GetGeomFieldCount() ==
                            poSrcFDefn->GetGeomFieldCount())
                    {
                        // Create output fields using CreateFieldFromArrowSchema()
                        for (int i = 0; i < schemaSrc.n_children; ++i)
                        {
                            const char *pszFieldName =
                                schemaSrc.children[i]->name;

                            const auto iSrcField =
                                poSrcFDefn->GetFieldIndex(pszFieldName);
                            if (iSrcField >= 0)
                            {
                                const auto poSrcFieldDefn =
                                    poSrcFDefn->GetFieldDefn(iSrcField);
                                // Create field domain in output dataset if not already existing.
                                const std::string osDomainName(
                                    poSrcFieldDefn->GetDomainName());
                                if (!osDomainName.empty())
                                {
                                    if (m_poDstDS->TestCapability(
                                            ODsCAddFieldDomain) &&
                                        m_poDstDS->GetFieldDomain(
                                            osDomainName) == nullptr)
                                    {
                                        const auto poSrcDomain =
                                            m_poSrcDS->GetFieldDomain(
                                                osDomainName);
                                        if (poSrcDomain)
                                        {
                                            std::string failureReason;
                                            if (!m_poDstDS->AddFieldDomain(
                                                    std::unique_ptr<
                                                        OGRFieldDomain>(
                                                        poSrcDomain->Clone()),
                                                    failureReason))
                                            {
                                                CPLDebug("OGR2OGR",
                                                         "Cannot create domain "
                                                         "%s: %s",
                                                         osDomainName.c_str(),
                                                         failureReason.c_str());
                                            }
                                        }
                                        else
                                        {
                                            CPLDebug("OGR2OGR",
                                                     "Cannot find domain %s in "
                                                     "source dataset",
                                                     osDomainName.c_str());
                                        }
                                    }
                                }
                            }

                            if (!EQUAL(pszFieldName, "OGC_FID") &&
                                !EQUAL(pszFieldName, "wkb_geometry") &&
                                !EQUAL(pszFieldName,
                                       poSrcLayer->GetFIDColumn()) &&
                                poSrcFDefn->GetGeomFieldIndex(pszFieldName) <
                                    0 &&
                                !poDstLayer->CreateFieldFromArrowSchema(
                                    schemaSrc.children[i], nullptr))
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Cannot create field %s",
                                         pszFieldName);
                                schemaSrc.release(&schemaSrc);
                                streamSrc.release(&streamSrc);
                                return false;
                            }
                        }
                        bUseWriteArrowBatch = true;
                    }
                    else if (!bJustCreatedLayer)
                    {
                        // If the layer already exist, get its schema, and
                        // check that it looks to be the same as the source
                        // one
                        struct ArrowArrayStream streamDst;
                        if (poDstLayer->GetArrowStream(&streamDst, nullptr))
                        {
                            struct ArrowSchema schemaDst;
                            if (streamDst.get_schema(&streamDst, &schemaDst) ==
                                0)
                            {
                                if (schemaDst.n_children ==
                                    schemaSrc.n_children)
                                {
                                    bUseWriteArrowBatch = true;
                                }
                                schemaDst.release(&schemaDst);
                            }
                            streamDst.release(&streamDst);
                        }
                    }
                    if (bUseWriteArrowBatch)
                    {
                        CPLDebug("OGR2OGR", "Using WriteArrowBatch()");
                    }
                }
                else
                {
                    CPLDebug("OGR2OGR",
                             "Cannot use WriteArrowBatch() because "
                             "input layer schema is not supported by output "
                             "layer: %s",
                             osErrorMsg.c_str());
                }
                schemaSrc.release(&schemaSrc);
            }
            streamSrc.release(&streamSrc);
        }
    }
    return bUseWriteArrowBatch;
}

/************************************************************************/
/*                   SetupTargetLayer::Setup()                          */
/************************************************************************/

std::unique_ptr<TargetLayerInfo>
SetupTargetLayer::Setup(OGRLayer *poSrcLayer, const char *pszNewLayerName,
                        GDALVectorTranslateOptions *psOptions,
                        GIntBig &nTotalEventsDone)
{
    int eGType = m_eGType;
    bool bPreserveFID = m_bPreserveFID;
    bool bAppend = m_bAppend;

    if (pszNewLayerName == nullptr)
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
    if (m_bSelFieldsSet && !bAppend)
    {
        for (int iField = 0; m_papszSelFields && m_papszSelFields[iField];
             iField++)
        {
            int iSrcField = poSrcFDefn->GetFieldIndex(m_papszSelFields[iField]);
            if (iSrcField >= 0)
            {
                /* do nothing */
            }
            else
            {
                iSrcField =
                    poSrcFDefn->GetGeomFieldIndex(m_papszSelFields[iField]);
                if (iSrcField >= 0)
                {
                    anRequestedGeomFields.push_back(iSrcField);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Field '%s' not found in source layer.",
                             m_papszSelFields[iField]);
                    if (!psOptions->bSkipFailures)
                        return nullptr;
                }
            }
        }

        if (anRequestedGeomFields.size() > 1 &&
            !m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Several geometry fields requested, but output "
                     "datasource does not support multiple geometry "
                     "fields.");
            if (!psOptions->bSkipFailures)
                return nullptr;
            else
                anRequestedGeomFields.resize(0);
        }
    }

    const OGRSpatialReference *poOutputSRS = m_poOutputSRS;
    if (poOutputSRS == nullptr && !m_bNullifyOutputSRS)
    {
        if (nSrcGeomFieldCount == 1 || anRequestedGeomFields.empty())
            poOutputSRS = poSrcLayer->GetSpatialRef();
        else if (anRequestedGeomFields.size() == 1)
        {
            int iSrcGeomField = anRequestedGeomFields[0];
            poOutputSRS =
                poSrcFDefn->GetGeomFieldDefn(iSrcGeomField)->GetSpatialRef();
        }
    }

    int iSrcZField = -1;
    if (m_pszZField != nullptr)
    {
        iSrcZField = poSrcFDefn->GetFieldIndex(m_pszZField);
        if (iSrcZField < 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "zfield '%s' does not exist in layer %s", m_pszZField,
                     poSrcLayer->GetName());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Find the layer.                                                 */
    /* -------------------------------------------------------------------- */

    bool bErrorOccurred;
    bool bOverwriteActuallyDone;
    bool bAddOverwriteLCO;
    OGRLayer *poDstLayer = GetLayerAndOverwriteIfNecessary(
        m_poDstDS, pszNewLayerName, m_bOverwrite, &bErrorOccurred,
        &bOverwriteActuallyDone, &bAddOverwriteLCO);
    const bool bJustCreatedLayer = (poDstLayer == nullptr);
    if (bErrorOccurred)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      If the layer does not exist, then create it.                    */
    /* -------------------------------------------------------------------- */
    if (poDstLayer == nullptr)
    {
        if (!m_poDstDS->TestCapability(ODsCCreateLayer))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Layer '%s' does not already exist in the output dataset, and "
                "cannot be created by the output driver.",
                pszNewLayerName);
            return nullptr;
        }

        bool bForceGType = (eGType != GEOMTYPE_UNCHANGED);
        if (!bForceGType)
        {
            if (anRequestedGeomFields.empty())
            {
                eGType = poSrcFDefn->GetGeomType();
            }
            else if (anRequestedGeomFields.size() == 1)
            {
                int iSrcGeomField = anRequestedGeomFields[0];
                eGType = poSrcFDefn->GetGeomFieldDefn(iSrcGeomField)->GetType();
            }
            else
            {
                eGType = wkbNone;
            }

            bool bHasZ =
                CPL_TO_BOOL(wkbHasZ(static_cast<OGRwkbGeometryType>(eGType)));
            eGType = ConvertType(m_eGeomTypeConversion,
                                 static_cast<OGRwkbGeometryType>(eGType));

            if (m_bExplodeCollections)
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
                         eFGType == wkbMultiCurve || eFGType == wkbMultiSurface)
                {
                    eGType = wkbUnknown;
                }
            }

            if (bHasZ || (iSrcZField >= 0 && eGType != wkbNone))
                eGType = wkbSetZ(static_cast<OGRwkbGeometryType>(eGType));
        }

        eGType = ForceCoordDimension(eGType, m_nCoordDim);

        CPLErrorReset();

        char **papszLCOTemp = CSLDuplicate(m_papszLCO);
        const char *pszDestCreationOptions =
            m_poDstDS->GetDriver()->GetMetadataItem(
                GDAL_DS_LAYER_CREATIONOPTIONLIST);

        int eGCreateLayerType = eGType;
        if (anRequestedGeomFields.empty() && nSrcGeomFieldCount > 1 &&
            m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            eGCreateLayerType = wkbNone;
        }
        // If the source layer has a single geometry column that is not nullable
        // and that ODsCCreateGeomFieldAfterCreateLayer is available, use it
        // so as to be able to set the not null constraint (if the driver
        // supports it) and that the output driver has no GEOMETRY_NULLABLE
        // layer creation option. Same if the source geometry column has a non
        // empty name that is not overridden, and that the output driver has no
        // GEOMETRY_NAME layer creation option, but no LAUNDER option (if
        // laundering is available, then we might want to launder the geometry
        // column name as well)
        else if (eGType != wkbNone && anRequestedGeomFields.empty() &&
                 nSrcGeomFieldCount == 1 &&
                 m_poDstDS->TestCapability(
                     ODsCCreateGeomFieldAfterCreateLayer) &&
                 ((!poSrcFDefn->GetGeomFieldDefn(0)->IsNullable() &&
                   CSLFetchNameValue(m_papszLCO, "GEOMETRY_NULLABLE") ==
                       nullptr &&
                   (pszDestCreationOptions == nullptr ||
                    strstr(pszDestCreationOptions, "GEOMETRY_NULLABLE") !=
                        nullptr) &&
                   !m_bForceNullable) ||
                  (poSrcLayer->GetGeometryColumn() != nullptr &&
                   CSLFetchNameValue(m_papszLCO, "GEOMETRY_NAME") == nullptr &&
                   !EQUAL(poSrcLayer->GetGeometryColumn(), "") &&
                   (pszDestCreationOptions == nullptr ||
                    strstr(pszDestCreationOptions, "GEOMETRY_NAME") ==
                        nullptr ||
                    strstr(pszDestCreationOptions, "LAUNDER") != nullptr) &&
                   poSrcFDefn->GetFieldIndex(poSrcLayer->GetGeometryColumn()) <
                       0)))
        {
            anRequestedGeomFields.push_back(0);
            eGCreateLayerType = wkbNone;
        }
        else if (anRequestedGeomFields.size() == 1 &&
                 m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            eGCreateLayerType = wkbNone;
        }

        OGRGeomCoordinatePrecision oCoordPrec;
        std::string osGeomFieldName;
        bool bGeomFieldNullable = true;

        {
            int iSrcGeomField = -1;
            if (anRequestedGeomFields.empty() &&
                (nSrcGeomFieldCount == 1 ||
                 (!m_poDstDS->TestCapability(
                      ODsCCreateGeomFieldAfterCreateLayer) &&
                  nSrcGeomFieldCount > 1)))
            {
                iSrcGeomField = 0;
            }
            else if (anRequestedGeomFields.size() == 1)
            {
                iSrcGeomField = anRequestedGeomFields[0];
            }

            if (iSrcGeomField >= 0)
            {
                const auto poSrcGeomFieldDefn =
                    poSrcFDefn->GetGeomFieldDefn(iSrcGeomField);
                if (!psOptions->bUnsetCoordPrecision)
                {
                    oCoordPrec = poSrcGeomFieldDefn->GetCoordinatePrecision()
                                     .ConvertToOtherSRS(
                                         poSrcGeomFieldDefn->GetSpatialRef(),
                                         poOutputSRS);
                }

                bGeomFieldNullable =
                    CPL_TO_BOOL(poSrcGeomFieldDefn->IsNullable());

                const char *pszGFldName = poSrcGeomFieldDefn->GetNameRef();
                if (pszGFldName != nullptr && !EQUAL(pszGFldName, "") &&
                    poSrcFDefn->GetFieldIndex(pszGFldName) < 0)
                {
                    osGeomFieldName = pszGFldName;

                    // Use source geometry field name as much as possible
                    if (eGType != wkbNone && pszDestCreationOptions &&
                        strstr(pszDestCreationOptions, "GEOMETRY_NAME") !=
                            nullptr &&
                        CSLFetchNameValue(m_papszLCO, "GEOMETRY_NAME") ==
                            nullptr)
                    {
                        papszLCOTemp = CSLSetNameValue(
                            papszLCOTemp, "GEOMETRY_NAME", pszGFldName);
                    }
                }
            }
        }

        // If the source feature first geometry column is not nullable
        // and that GEOMETRY_NULLABLE creation option is available, use it
        // so as to be able to set the not null constraint (if the driver
        // supports it)
        if (eGType != wkbNone && anRequestedGeomFields.empty() &&
            nSrcGeomFieldCount >= 1 &&
            !poSrcFDefn->GetGeomFieldDefn(0)->IsNullable() &&
            pszDestCreationOptions != nullptr &&
            strstr(pszDestCreationOptions, "GEOMETRY_NULLABLE") != nullptr &&
            CSLFetchNameValue(m_papszLCO, "GEOMETRY_NULLABLE") == nullptr &&
            !m_bForceNullable)
        {
            bGeomFieldNullable = false;
            papszLCOTemp =
                CSLSetNameValue(papszLCOTemp, "GEOMETRY_NULLABLE", "NO");
            CPLDebug("GDALVectorTranslate", "Using GEOMETRY_NULLABLE=NO");
        }

        if (psOptions->dfXYRes != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            if (m_poDstDS->GetDriver()->GetMetadataItem(
                    GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION) == nullptr &&
                !OGRGeometryFactory::haveGEOS())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "-xyRes specified, but driver does not expose the "
                         "DCAP_HONOR_GEOM_COORDINATE_PRECISION capability, "
                         "and this build has no GEOS support");
            }

            oCoordPrec.dfXYResolution = psOptions->dfXYRes;
            if (!psOptions->osXYResUnit.empty())
            {
                if (!poOutputSRS)
                {
                    CSLDestroy(papszLCOTemp);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unit suffix for -xyRes cannot be used with an "
                             "unknown destination SRS");
                    return nullptr;
                }

                if (psOptions->osXYResUnit == "mm")
                {
                    oCoordPrec.dfXYResolution *= 1e-3;
                }
                else if (psOptions->osXYResUnit == "deg")
                {
                    double dfFactorDegToMeter =
                        poOutputSRS->GetSemiMajor(nullptr) * M_PI / 180;
                    oCoordPrec.dfXYResolution *= dfFactorDegToMeter;
                }
                else
                {
                    // Checked at argument parsing time
                    CPLAssert(psOptions->osXYResUnit == "m");
                }

                OGRGeomCoordinatePrecision tmp;
                tmp.SetFromMeter(poOutputSRS, oCoordPrec.dfXYResolution, 0, 0);
                oCoordPrec.dfXYResolution = tmp.dfXYResolution;
            }
        }

        if (psOptions->dfZRes != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            if (m_poDstDS->GetDriver()->GetMetadataItem(
                    GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION) == nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "-zRes specified, but driver does not expose the "
                         "DCAP_HONOR_GEOM_COORDINATE_PRECISION capability");
            }

            oCoordPrec.dfZResolution = psOptions->dfZRes;
            if (!psOptions->osZResUnit.empty())
            {
                if (!poOutputSRS)
                {
                    CSLDestroy(papszLCOTemp);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unit suffix for -zRes cannot be used with an "
                             "unknown destination SRS");
                    return nullptr;
                }

                if (psOptions->osZResUnit == "mm")
                {
                    oCoordPrec.dfZResolution *= 1e-3;
                }
                else
                {
                    // Checked at argument parsing time
                    CPLAssert(psOptions->osZResUnit == "m");
                }

                OGRGeomCoordinatePrecision tmp;
                tmp.SetFromMeter(poOutputSRS, 0, oCoordPrec.dfZResolution, 0);
                oCoordPrec.dfZResolution = tmp.dfZResolution;
            }
        }

        if (psOptions->dfMRes != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            if (m_poDstDS->GetDriver()->GetMetadataItem(
                    GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION) == nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "-mRes specified, but driver does not expose the "
                         "DCAP_HONOR_GEOM_COORDINATE_PRECISION capability");
            }

            oCoordPrec.dfMResolution = psOptions->dfMRes;
        }

        // Force FID column as 64 bit if the source feature has a 64 bit FID,
        // the target driver supports 64 bit FID and the user didn't set it
        // manually.
        if (poSrcLayer->GetMetadataItem(OLMD_FID64) != nullptr &&
            EQUAL(poSrcLayer->GetMetadataItem(OLMD_FID64), "YES") &&
            pszDestCreationOptions &&
            strstr(pszDestCreationOptions, "FID64") != nullptr &&
            CSLFetchNameValue(m_papszLCO, "FID64") == nullptr)
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "FID64", "YES");
            CPLDebug("GDALVectorTranslate", "Using FID64=YES");
        }

        // If output driver supports FID layer creation option, set it with
        // the FID column name of the source layer
        if (!m_bUnsetFid && !bAppend && poSrcLayer->GetFIDColumn() != nullptr &&
            !EQUAL(poSrcLayer->GetFIDColumn(), "") &&
            pszDestCreationOptions != nullptr &&
            (strstr(pszDestCreationOptions, "='FID'") != nullptr ||
             strstr(pszDestCreationOptions, "=\"FID\"") != nullptr) &&
            CSLFetchNameValue(m_papszLCO, "FID") == nullptr)
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "FID",
                                           poSrcLayer->GetFIDColumn());
            if (!psOptions->bExplodeCollections)
            {
                CPLDebug("GDALVectorTranslate",
                         "Using FID=%s and -preserve_fid",
                         poSrcLayer->GetFIDColumn());
                bPreserveFID = true;
            }
            else
            {
                CPLDebug("GDALVectorTranslate",
                         "Using FID=%s and disable -preserve_fid because not "
                         "compatible with -explodecollection",
                         poSrcLayer->GetFIDColumn());
                bPreserveFID = false;
            }
        }
        // Detect scenario of converting from GPX to a format like GPKG
        // Cf https://github.com/OSGeo/gdal/issues/9225
        else if (!bPreserveFID && !m_bUnsetFid && !bAppend &&
                 m_poSrcDS->GetDriver() &&
                 EQUAL(m_poSrcDS->GetDriver()->GetDescription(), "GPX") &&
                 pszDestCreationOptions &&
                 (strstr(pszDestCreationOptions, "='FID'") != nullptr ||
                  strstr(pszDestCreationOptions, "=\"FID\"") != nullptr) &&
                 CSLFetchNameValue(m_papszLCO, "FID") == nullptr)
        {
            CPLDebug("GDALVectorTranslate",
                     "Forcing -preserve_fid because source is GPX and layers "
                     "have FID cross references");
            bPreserveFID = true;
        }
        // Detect scenario of converting GML2 with fid attribute to GPKG
        else if (EQUAL(m_poDstDS->GetDriver()->GetDescription(), "GPKG") &&
                 CSLFetchNameValue(m_papszLCO, "FID") == nullptr)
        {
            int nFieldIdx = poSrcLayer->GetLayerDefn()->GetFieldIndex("fid");
            if (nFieldIdx >= 0 && poSrcLayer->GetLayerDefn()
                                          ->GetFieldDefn(nFieldIdx)
                                          ->GetType() == OFTString)
            {
                CPLDebug("GDALVectorTranslate",
                         "Source layer has a non-string 'fid' column. Using "
                         "FID=gpkg_fid for GeoPackage");
                papszLCOTemp = CSLSetNameValue(papszLCOTemp, "FID", "gpkg_fid");
            }
        }

        // If bAddOverwriteLCO is ON (set up when overwriting a CARTO layer),
        // set OVERWRITE to YES so the new layer overwrites the old one
        if (bAddOverwriteLCO)
        {
            papszLCOTemp = CSLSetNameValue(papszLCOTemp, "OVERWRITE", "ON");
            CPLDebug("GDALVectorTranslate", "Using OVERWRITE=ON");
        }

        if (m_bNativeData &&
            poSrcLayer->GetMetadataItem("NATIVE_DATA", "NATIVE_DATA") !=
                nullptr &&
            poSrcLayer->GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA") !=
                nullptr &&
            pszDestCreationOptions != nullptr &&
            strstr(pszDestCreationOptions, "NATIVE_DATA") != nullptr &&
            strstr(pszDestCreationOptions, "NATIVE_MEDIA_TYPE") != nullptr)
        {
            papszLCOTemp = CSLSetNameValue(
                papszLCOTemp, "NATIVE_DATA",
                poSrcLayer->GetMetadataItem("NATIVE_DATA", "NATIVE_DATA"));
            papszLCOTemp =
                CSLSetNameValue(papszLCOTemp, "NATIVE_MEDIA_TYPE",
                                poSrcLayer->GetMetadataItem("NATIVE_MEDIA_TYPE",
                                                            "NATIVE_DATA"));
            CPLDebug("GDALVectorTranslate", "Transferring layer NATIVE_DATA");
        }

        // For FileGeodatabase, automatically set
        // CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES creation option if the source
        // layer has a Shape_Area/Shape_Length field
        if (pszDestCreationOptions &&
            strstr(pszDestCreationOptions,
                   "CREATE_SHAPE_AREA_AND_LENGTH_FIELDS") != nullptr &&
            CSLFetchNameValue(m_papszLCO,
                              "CREATE_SHAPE_AREA_AND_LENGTH_FIELDS") == nullptr)
        {
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            const int nIdxShapeArea =
                poSrcLayerDefn->GetFieldIndex("Shape_Area");
            const int nIdxShapeLength =
                poSrcLayerDefn->GetFieldIndex("Shape_Length");
            if ((nIdxShapeArea >= 0 &&
                 poSrcLayerDefn->GetFieldDefn(nIdxShapeArea)->GetDefault() !=
                     nullptr &&
                 EQUAL(
                     poSrcLayerDefn->GetFieldDefn(nIdxShapeArea)->GetDefault(),
                     "FILEGEODATABASE_SHAPE_AREA") &&
                 (m_papszSelFields == nullptr ||
                  CSLFindString(m_papszSelFields, "Shape_Area") >= 0)) ||
                (nIdxShapeLength >= 0 &&
                 poSrcLayerDefn->GetFieldDefn(nIdxShapeLength)->GetDefault() !=
                     nullptr &&
                 EQUAL(poSrcLayerDefn->GetFieldDefn(nIdxShapeLength)
                           ->GetDefault(),
                       "FILEGEODATABASE_SHAPE_LENGTH") &&
                 (m_papszSelFields == nullptr ||
                  CSLFindString(m_papszSelFields, "Shape_Length") >= 0)))
            {
                papszLCOTemp = CSLSetNameValue(
                    papszLCOTemp, "CREATE_SHAPE_AREA_AND_LENGTH_FIELDS", "YES");
                CPLDebug("GDALVectorTranslate",
                         "Setting CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES");
            }
        }

        OGRGeomFieldDefn oGeomFieldDefn(
            osGeomFieldName.c_str(),
            static_cast<OGRwkbGeometryType>(eGCreateLayerType));
        oGeomFieldDefn.SetSpatialRef(poOutputSRS);
        oGeomFieldDefn.SetCoordinatePrecision(oCoordPrec);
        oGeomFieldDefn.SetNullable(bGeomFieldNullable);
        poDstLayer = m_poDstDS->CreateLayer(
            pszNewLayerName,
            eGCreateLayerType == wkbNone ? nullptr : &oGeomFieldDefn,
            papszLCOTemp);
        CSLDestroy(papszLCOTemp);

        if (poDstLayer == nullptr)
        {
            return nullptr;
        }

        // Cf https://github.com/OSGeo/gdal/issues/6859
        // warn if the user requests -t_srs but the driver uses a different SRS.
        if (m_poOutputSRS != nullptr && m_bTransform && !psOptions->bQuiet &&
            // MapInfo is somewhat lossy regarding SRS, so do not warn
            !EQUAL(m_poDstDS->GetDriver()->GetDescription(), "MapInfo File"))
        {
            auto poCreatedSRS = poDstLayer->GetSpatialRef();
            if (poCreatedSRS != nullptr)
            {
                const char *const apszOptions[] = {
                    "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES",
                    "CRITERION=EQUIVALENT", nullptr};
                if (!poCreatedSRS->IsSame(m_poOutputSRS, apszOptions))
                {
                    const char *pszTargetSRSName = m_poOutputSRS->GetName();
                    const char *pszCreatedSRSName = poCreatedSRS->GetName();
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Target SRS %s not taken into account as target "
                             "driver likely implements on-the-fly reprojection "
                             "to %s",
                             pszTargetSRSName ? pszTargetSRSName : "",
                             pszCreatedSRSName ? pszCreatedSRSName : "");
                }
            }
        }

        if (m_bCopyMD)
        {
            const CPLStringList aosDomains(poSrcLayer->GetMetadataDomainList());
            for (const char *pszMD : aosDomains)
            {
                if (!EQUAL(pszMD, "IMAGE_STRUCTURE") &&
                    !EQUAL(pszMD, "SUBDATASETS"))
                {
                    if (char **papszMD = poSrcLayer->GetMetadata(pszMD))
                        poDstLayer->SetMetadata(papszMD, pszMD);
                }
            }
        }

        if (anRequestedGeomFields.empty() && nSrcGeomFieldCount > 1 &&
            m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            for (int i = 0; i < nSrcGeomFieldCount; i++)
            {
                anRequestedGeomFields.push_back(i);
            }
        }

        if (anRequestedGeomFields.size() > 1 ||
            (anRequestedGeomFields.size() == 1 &&
             m_poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer)))
        {
            for (int i = 0; i < static_cast<int>(anRequestedGeomFields.size());
                 i++)
            {
                const int iSrcGeomField = anRequestedGeomFields[i];
                OGRGeomFieldDefn oGFldDefn(
                    poSrcFDefn->GetGeomFieldDefn(iSrcGeomField));
                if (m_poOutputSRS != nullptr)
                {
                    auto poOutputSRSClone = m_poOutputSRS->Clone();
                    oGFldDefn.SetSpatialRef(poOutputSRSClone);
                    poOutputSRSClone->Release();
                }
                if (bForceGType)
                {
                    oGFldDefn.SetType(static_cast<OGRwkbGeometryType>(eGType));
                }
                else
                {
                    eGType = oGFldDefn.GetType();
                    eGType =
                        ConvertType(m_eGeomTypeConversion,
                                    static_cast<OGRwkbGeometryType>(eGType));
                    eGType = ForceCoordDimension(eGType, m_nCoordDim);
                    oGFldDefn.SetType(static_cast<OGRwkbGeometryType>(eGType));
                }
                if (m_bForceNullable)
                    oGFldDefn.SetNullable(TRUE);
                poDstLayer->CreateGeomField(&oGFldDefn);
            }
        }

        bAppend = false;
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise we will append to it, if append was requested.        */
    /* -------------------------------------------------------------------- */
    else if (!bAppend && !m_bNewDataSource)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer %s already exists, and -append not specified.\n"
                 "        Consider using -append, or -overwrite.",
                 pszNewLayerName);
        return nullptr;
    }
    else
    {
        if (CSLCount(m_papszLCO) > 0)
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Layer creation options ignored since an existing layer is\n"
                "         being appended to.");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Process Layer style table                                       */
    /* -------------------------------------------------------------------- */

    poDstLayer->SetStyleTable(poSrcLayer->GetStyleTable());
    /* -------------------------------------------------------------------- */
    /*      Add fields.  Default to copy all field.                         */
    /*      If only a subset of all fields requested, then output only      */
    /*      the selected fields, and in the order that they were            */
    /*      selected.                                                       */
    /* -------------------------------------------------------------------- */
    const int nSrcFieldCount = poSrcFDefn->GetFieldCount();
    int iSrcFIDField = -1;

    // Initialize the index-to-index map to -1's
    std::vector<int> anMap(nSrcFieldCount, -1);

    std::map<int, TargetLayerInfo::ResolvedInfo> oMapResolved;

    /* Determine if NUMERIC field width narrowing is allowed */
    const char *pszSrcWidthIncludesDecimalSeparator{
        m_poSrcDS->GetDriver()->GetMetadataItem(
            "DMD_NUMERIC_FIELD_WIDTH_INCLUDES_DECIMAL_SEPARATOR")};
    const bool bSrcWidthIncludesDecimalSeparator{
        pszSrcWidthIncludesDecimalSeparator &&
        EQUAL(pszSrcWidthIncludesDecimalSeparator, "YES")};
    const char *pszDstWidthIncludesDecimalSeparator{
        m_poDstDS->GetDriver()->GetMetadataItem(
            "DMD_NUMERIC_FIELD_WIDTH_INCLUDES_DECIMAL_SEPARATOR")};
    const bool bDstWidthIncludesDecimalSeparator{
        pszDstWidthIncludesDecimalSeparator &&
        EQUAL(pszDstWidthIncludesDecimalSeparator, "YES")};
    const char *pszSrcWidthIncludesMinusSign{
        m_poSrcDS->GetDriver()->GetMetadataItem(
            "DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN")};
    const bool bSrcWidthIncludesMinusSign{
        pszSrcWidthIncludesMinusSign &&
        EQUAL(pszSrcWidthIncludesMinusSign, "YES")};
    const char *pszDstWidthIncludesMinusSign{
        m_poDstDS->GetDriver()->GetMetadataItem(
            "DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN")};
    const bool bDstWidthIncludesMinusSign{
        pszDstWidthIncludesMinusSign &&
        EQUAL(pszDstWidthIncludesMinusSign, "YES")};

    // Calculate width delta
    int iChangeWidthBy{0};

    if (bSrcWidthIncludesDecimalSeparator && !bDstWidthIncludesDecimalSeparator)
    {
        iChangeWidthBy--;
    }
    else if (!bSrcWidthIncludesDecimalSeparator &&
             bDstWidthIncludesDecimalSeparator)
    {
        iChangeWidthBy++;
    }

    // We cannot assume there is no minus sign, we can only inflate here
    if (!bSrcWidthIncludesMinusSign && bDstWidthIncludesMinusSign)
    {
        iChangeWidthBy++;
    }

    bool bError = false;
    const bool bUseWriteArrowBatch = CanUseWriteArrowBatch(
        poSrcLayer, poDstLayer, bJustCreatedLayer, psOptions, bError);
    if (bError)
        return nullptr;

    /* Caution : at the time of writing, the MapInfo driver */
    /* returns NULL until a field has been added */
    OGRFeatureDefn *poDstFDefn = poDstLayer->GetLayerDefn();

    if (bUseWriteArrowBatch)
    {
        // Fields created above
    }
    else if (m_papszFieldMap && bAppend)
    {
        bool bIdentity = false;

        if (EQUAL(m_papszFieldMap[0], "identity"))
            bIdentity = true;
        else if (CSLCount(m_papszFieldMap) != nSrcFieldCount)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Field map should contain the value 'identity' or "
                "the same number of integer values as the source field count.");
            return nullptr;
        }

        for (int iField = 0; iField < nSrcFieldCount; iField++)
        {
            anMap[iField] = bIdentity ? iField : atoi(m_papszFieldMap[iField]);
            if (anMap[iField] >= poDstFDefn->GetFieldCount())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid destination field index %d.", anMap[iField]);
                return nullptr;
            }
        }
    }
    else if (m_bSelFieldsSet && !bAppend)
    {
        int nDstFieldCount = poDstFDefn ? poDstFDefn->GetFieldCount() : 0;
        for (int iField = 0; m_papszSelFields && m_papszSelFields[iField];
             iField++)
        {
            const int iSrcField =
                poSrcFDefn->GetFieldIndex(m_papszSelFields[iField]);
            if (iSrcField >= 0)
            {
                OGRFieldDefn *poSrcFieldDefn =
                    poSrcFDefn->GetFieldDefn(iSrcField);
                OGRFieldDefn oFieldDefn(poSrcFieldDefn);

                DoFieldTypeConversion(
                    m_poDstDS, oFieldDefn, m_papszFieldTypesToString,
                    m_papszMapFieldType, m_bUnsetFieldWidth, psOptions->bQuiet,
                    m_bForceNullable, m_bUnsetDefault);

                if (iChangeWidthBy != 0 && oFieldDefn.GetType() == OFTReal &&
                    oFieldDefn.GetWidth() != 0)
                {
                    oFieldDefn.SetWidth(oFieldDefn.GetWidth() + iChangeWidthBy);
                }

                /* The field may have been already created at layer creation */
                const int iDstField =
                    poDstFDefn
                        ? poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef())
                        : -1;
                if (iDstField >= 0)
                {
                    anMap[iSrcField] = iDstField;
                }
                else if (poDstLayer->CreateField(&oFieldDefn) == OGRERR_NONE)
                {
                    /* now that we've created a field, GetLayerDefn() won't
                     * return NULL */
                    if (poDstFDefn == nullptr)
                        poDstFDefn = poDstLayer->GetLayerDefn();

                    /* Sanity check : if it fails, the driver is buggy */
                    if (poDstFDefn != nullptr &&
                        poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "The output driver has claimed to have added "
                                 "the %s field, but it did not!",
                                 oFieldDefn.GetNameRef());
                    }
                    else
                    {
                        anMap[iSrcField] = nDstFieldCount;
                        nDstFieldCount++;
                    }
                }
            }
        }

        /* --------------------------------------------------------------------
         */
        /* Use SetIgnoredFields() on source layer if available */
        /* --------------------------------------------------------------------
         */
        if (poSrcLayer->TestCapability(OLCIgnoreFields))
        {
            bool bUseIgnoredFields = true;
            char **papszWHEREUsedFields = nullptr;

            if (m_pszWHERE)
            {
                /* We must not ignore fields used in the -where expression
                 * (#4015) */
                OGRFeatureQuery oFeatureQuery;
                if (oFeatureQuery.Compile(poSrcLayer->GetLayerDefn(),
                                          m_pszWHERE, FALSE,
                                          nullptr) == OGRERR_NONE)
                {
                    papszWHEREUsedFields = oFeatureQuery.GetUsedFields();
                }
                else
                {
                    bUseIgnoredFields = false;
                }
            }

            char **papszIgnoredFields = nullptr;

            for (int iSrcField = 0;
                 bUseIgnoredFields && iSrcField < poSrcFDefn->GetFieldCount();
                 iSrcField++)
            {
                const char *pszFieldName =
                    poSrcFDefn->GetFieldDefn(iSrcField)->GetNameRef();
                bool bFieldRequested = false;
                for (int iField = 0;
                     m_papszSelFields && m_papszSelFields[iField]; iField++)
                {
                    if (EQUAL(pszFieldName, m_papszSelFields[iField]))
                    {
                        bFieldRequested = true;
                        break;
                    }
                }
                bFieldRequested |=
                    CSLFindString(papszWHEREUsedFields, pszFieldName) >= 0;
                bFieldRequested |= (m_pszZField != nullptr &&
                                    EQUAL(pszFieldName, m_pszZField));

                /* If source field not requested, add it to ignored files list
                 */
                if (!bFieldRequested)
                    papszIgnoredFields =
                        CSLAddString(papszIgnoredFields, pszFieldName);
            }
            if (bUseIgnoredFields)
                poSrcLayer->SetIgnoredFields(
                    const_cast<const char **>(papszIgnoredFields));
            CSLDestroy(papszIgnoredFields);
            CSLDestroy(papszWHEREUsedFields);
        }
    }
    else if (!bAppend || m_bAddMissingFields)
    {
        int nDstFieldCount = poDstFDefn ? poDstFDefn->GetFieldCount() : 0;

        const bool caseInsensitive =
            !EQUAL(m_poDstDS->GetDriver()->GetDescription(), "GeoJSON");
        const auto formatName = [caseInsensitive](const char *name)
        {
            if (caseInsensitive)
            {
                return CPLString(name).toupper();
            }
            else
            {
                return CPLString(name);
            }
        };

        /* Save the map of existing fields, before creating new ones */
        /* This helps when converting a source layer that has duplicated field
         * names */
        /* which is a bad idea */
        std::map<CPLString, int> oMapPreExistingFields;
        std::unordered_set<std::string> oSetDstFieldNames;
        for (int iField = 0; iField < nDstFieldCount; iField++)
        {
            const char *pszFieldName =
                poDstFDefn->GetFieldDefn(iField)->GetNameRef();
            CPLString osUpperFieldName(formatName(pszFieldName));
            oSetDstFieldNames.insert(osUpperFieldName);
            if (oMapPreExistingFields.find(osUpperFieldName) ==
                oMapPreExistingFields.end())
                oMapPreExistingFields[osUpperFieldName] = iField;
            /*else
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The target layer has already a duplicated field name
               '%s' before " "adding the fields of the source layer",
               pszFieldName); */
        }

        const char *pszFIDColumn = poDstLayer->GetFIDColumn();

        std::vector<int> anSrcFieldIndices;
        if (m_bSelFieldsSet)
        {
            for (int iField = 0; m_papszSelFields && m_papszSelFields[iField];
                 iField++)
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
            for (int iField = 0; iField < nSrcFieldCount; iField++)
            {
                anSrcFieldIndices.push_back(iField);
            }
        }

        std::unordered_set<std::string> oSetSrcFieldNames;
        for (int i = 0; i < poSrcFDefn->GetFieldCount(); i++)
        {
            oSetSrcFieldNames.insert(
                formatName(poSrcFDefn->GetFieldDefn(i)->GetNameRef()));
        }

        // For each source field name, memorize the last number suffix to have
        // unique field names in the target. Let's imagine we have a source
        // layer with the field name foo repeated twice After dealing the first
        // field, oMapFieldNameToLastSuffix["foo"] will be 1, so when starting a
        // unique name for the second field, we'll be able to start at 2. This
        // avoids quadratic complexity if a big number of source field names are
        // identical. Like in
        // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=37768
        std::map<std::string, int> oMapFieldNameToLastSuffix;

        for (size_t i = 0; i < anSrcFieldIndices.size(); i++)
        {
            const int iField = anSrcFieldIndices[i];
            const OGRFieldDefn *poSrcFieldDefn =
                poSrcFDefn->GetFieldDefn(iField);
            OGRFieldDefn oFieldDefn(poSrcFieldDefn);

            // Avoid creating a field with the same name as the FID column
            if (pszFIDColumn != nullptr &&
                EQUAL(pszFIDColumn, oFieldDefn.GetNameRef()) &&
                (oFieldDefn.GetType() == OFTInteger ||
                 oFieldDefn.GetType() == OFTInteger64))
            {
                iSrcFIDField = iField;
                continue;
            }

            DoFieldTypeConversion(
                m_poDstDS, oFieldDefn, m_papszFieldTypesToString,
                m_papszMapFieldType, m_bUnsetFieldWidth, psOptions->bQuiet,
                m_bForceNullable, m_bUnsetDefault);

            if (iChangeWidthBy != 0 && oFieldDefn.GetType() == OFTReal &&
                oFieldDefn.GetWidth() != 0)
            {
                oFieldDefn.SetWidth(oFieldDefn.GetWidth() + iChangeWidthBy);
            }

            /* The field may have been already created at layer creation */
            {
                const auto oIter = oMapPreExistingFields.find(
                    formatName(oFieldDefn.GetNameRef()));
                if (oIter != oMapPreExistingFields.end())
                {
                    anMap[iField] = oIter->second;
                    continue;
                }
            }

            bool bHasRenamed = false;
            /* In case the field name already exists in the target layer, */
            /* build a unique field name */
            if (oSetDstFieldNames.find(formatName(oFieldDefn.GetNameRef())) !=
                oSetDstFieldNames.end())
            {
                const CPLString osTmpNameRaddixUC(
                    formatName(oFieldDefn.GetNameRef()));
                int nTry = 1;
                const auto oIter =
                    oMapFieldNameToLastSuffix.find(osTmpNameRaddixUC);
                if (oIter != oMapFieldNameToLastSuffix.end())
                    nTry = oIter->second;
                CPLString osTmpNameUC = osTmpNameRaddixUC;
                osTmpNameUC.reserve(osTmpNameUC.size() + 10);
                while (true)
                {
                    ++nTry;
                    char szTry[32];
                    snprintf(szTry, sizeof(szTry), "%d", nTry);
                    osTmpNameUC.replace(osTmpNameRaddixUC.size(),
                                        std::string::npos, szTry);

                    /* Check that the proposed name doesn't exist either in the
                     * already */
                    /* created fields or in the source fields */
                    if (oSetDstFieldNames.find(osTmpNameUC) ==
                            oSetDstFieldNames.end() &&
                        oSetSrcFieldNames.find(osTmpNameUC) ==
                            oSetSrcFieldNames.end())
                    {
                        bHasRenamed = true;
                        oFieldDefn.SetName(
                            (CPLString(oFieldDefn.GetNameRef()) + szTry)
                                .c_str());
                        oMapFieldNameToLastSuffix[osTmpNameRaddixUC] = nTry;
                        break;
                    }
                }
            }

            // Create field domain in output dataset if not already existing.
            const std::string osDomainName(oFieldDefn.GetDomainName());
            if (!osDomainName.empty())
            {
                if (m_poDstDS->TestCapability(ODsCAddFieldDomain) &&
                    m_poDstDS->GetFieldDomain(osDomainName) == nullptr)
                {
                    const auto poSrcDomain =
                        m_poSrcDS->GetFieldDomain(osDomainName);
                    if (poSrcDomain)
                    {
                        std::string failureReason;
                        if (!m_poDstDS->AddFieldDomain(
                                std::unique_ptr<OGRFieldDomain>(
                                    poSrcDomain->Clone()),
                                failureReason))
                        {
                            oFieldDefn.SetDomainName(std::string());
                            CPLDebug("OGR2OGR", "Cannot create domain %s: %s",
                                     osDomainName.c_str(),
                                     failureReason.c_str());
                        }
                    }
                    else
                    {
                        CPLDebug("OGR2OGR",
                                 "Cannot find domain %s in source dataset",
                                 osDomainName.c_str());
                    }
                }
                if (m_poDstDS->GetFieldDomain(osDomainName) == nullptr)
                {
                    oFieldDefn.SetDomainName(std::string());
                }
            }

            if (poDstLayer->CreateField(&oFieldDefn) == OGRERR_NONE)
            {
                /* now that we've created a field, GetLayerDefn() won't return
                 * NULL */
                if (poDstFDefn == nullptr)
                    poDstFDefn = poDstLayer->GetLayerDefn();

                /* Sanity check : if it fails, the driver is buggy */
                if (poDstFDefn != nullptr &&
                    poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The output driver has claimed to have added the "
                             "%s field, but it did not!",
                             oFieldDefn.GetNameRef());
                }
                else
                {
                    if (poDstFDefn != nullptr)
                    {
                        const char *pszNewFieldName =
                            poDstFDefn->GetFieldDefn(nDstFieldCount)
                                ->GetNameRef();
                        if (bHasRenamed)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Field '%s' already exists. Renaming it "
                                     "as '%s'",
                                     poSrcFieldDefn->GetNameRef(),
                                     pszNewFieldName);
                        }
                        oSetDstFieldNames.insert(formatName(pszNewFieldName));
                    }

                    anMap[iField] = nDstFieldCount;
                    nDstFieldCount++;
                }
            }

            if (m_bResolveDomains && !osDomainName.empty())
            {
                const auto poSrcDomain =
                    m_poSrcDS->GetFieldDomain(osDomainName);
                if (poSrcDomain && poSrcDomain->GetDomainType() == OFDT_CODED)
                {
                    OGRFieldDefn oResolvedField(
                        CPLSPrintf("%s_resolved", oFieldDefn.GetNameRef()),
                        OFTString);
                    if (poDstLayer->CreateField(&oResolvedField) == OGRERR_NONE)
                    {
                        TargetLayerInfo::ResolvedInfo resolvedInfo;
                        resolvedInfo.nSrcField = iField;
                        resolvedInfo.poDomain = poSrcDomain;
                        oMapResolved[nDstFieldCount] = resolvedInfo;
                        nDstFieldCount++;
                    }
                }
            }
        }
    }
    else
    {
        /* For an existing layer, build the map by fetching the index in the
         * destination */
        /* layer for each source field */
        if (poDstFDefn == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "poDstFDefn == NULL.");
            return nullptr;
        }

        for (int iField = 0; iField < nSrcFieldCount; iField++)
        {
            OGRFieldDefn *poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iField);
            const int iDstField = poDstLayer->FindFieldIndex(
                poSrcFieldDefn->GetNameRef(), m_bExactFieldNameMatch);
            if (iDstField >= 0)
                anMap[iField] = iDstField;
            else
                CPLDebug(
                    "GDALVectorTranslate",
                    "Skipping field '%s' not found in destination layer '%s'.",
                    poSrcFieldDefn->GetNameRef(), poDstLayer->GetName());
        }
    }

    if (bOverwriteActuallyDone && !bAddOverwriteLCO &&
        EQUAL(m_poDstDS->GetDriver()->GetDescription(), "PostgreSQL") &&
        !psOptions->nLayerTransaction && psOptions->nGroupTransactions > 0 &&
        CPLTestBool(CPLGetConfigOption("PG_COMMIT_WHEN_OVERWRITING", "YES")))
    {
        CPLDebug("GDALVectorTranslate",
                 "Forcing transaction commit as table overwriting occurred");
        // Commit when overwriting as this consumes a lot of PG resources
        // and could result in """out of shared memory.
        // You might need to increase max_locks_per_transaction."""" errors
        if (m_poDstDS->CommitTransaction() == OGRERR_FAILURE ||
            m_poDstDS->StartTransaction(psOptions->bForceTransaction) ==
                OGRERR_FAILURE)
        {
            return nullptr;
        }
        nTotalEventsDone = 0;
    }

    std::unique_ptr<TargetLayerInfo> psInfo(new TargetLayerInfo);
    psInfo->m_bUseWriteArrowBatch = bUseWriteArrowBatch;
    psInfo->m_nFeaturesRead = 0;
    psInfo->m_bPerFeatureCT = false;
    psInfo->m_poSrcLayer = poSrcLayer;
    psInfo->m_poDstLayer = poDstLayer;
    psInfo->m_aoReprojectionInfo.resize(
        poDstLayer->GetLayerDefn()->GetGeomFieldCount());
    psInfo->m_anMap = std::move(anMap);
    psInfo->m_iSrcZField = iSrcZField;
    psInfo->m_iSrcFIDField = iSrcFIDField;
    if (anRequestedGeomFields.size() == 1)
        psInfo->m_iRequestedSrcGeomField = anRequestedGeomFields[0];
    else
        psInfo->m_iRequestedSrcGeomField = -1;
    psInfo->m_bPreserveFID = bPreserveFID;
    psInfo->m_pszCTPipeline = m_pszCTPipeline;
    psInfo->m_oMapResolved = std::move(oMapResolved);
    for (const auto &kv : psInfo->m_oMapResolved)
    {
        const auto poDomain = kv.second.poDomain;
        const auto poCodedDomain =
            cpl::down_cast<const OGRCodedFieldDomain *>(poDomain);
        const auto enumeration = poCodedDomain->GetEnumeration();
        std::map<std::string, std::string> oMapCodeValue;
        for (int i = 0; enumeration[i].pszCode != nullptr; ++i)
        {
            oMapCodeValue[enumeration[i].pszCode] =
                enumeration[i].pszValue ? enumeration[i].pszValue : "";
        }
        psInfo->m_oMapDomainToKV[poDomain] = std::move(oMapCodeValue);
    }

    // Detect if we can directly pass the source feature to the CreateFeature()
    // method of the target layer, without doing any copying of field content.
    psInfo->m_bCanAvoidSetFrom = false;
    if (!m_bExplodeCollections && iSrcZField == -1 && poDstFDefn != nullptr)
    {
        psInfo->m_bCanAvoidSetFrom = true;
        const int nDstGeomFieldCount = poDstFDefn->GetGeomFieldCount();
        if (nSrcFieldCount != poDstFDefn->GetFieldCount() ||
            nSrcGeomFieldCount != nDstGeomFieldCount)
        {
            psInfo->m_bCanAvoidSetFrom = false;
        }
        else
        {
            for (int i = 0; i < nSrcFieldCount; ++i)
            {
                auto poSrcFieldDefn = poSrcFDefn->GetFieldDefn(i);
                auto poDstFieldDefn = poDstFDefn->GetFieldDefn(i);
                if (poSrcFieldDefn->GetType() != poDstFieldDefn->GetType() ||
                    psInfo->m_anMap[i] != i)
                {
                    psInfo->m_bCanAvoidSetFrom = false;
                    break;
                }
            }
            if (!psInfo->m_bCanAvoidSetFrom && nSrcGeomFieldCount > 1)
            {
                for (int i = 0; i < nSrcGeomFieldCount; ++i)
                {
                    auto poSrcGeomFieldDefn = poSrcFDefn->GetGeomFieldDefn(i);
                    auto poDstGeomFieldDefn = poDstFDefn->GetGeomFieldDefn(i);
                    if (!EQUAL(poSrcGeomFieldDefn->GetNameRef(),
                               poDstGeomFieldDefn->GetNameRef()))
                    {
                        psInfo->m_bCanAvoidSetFrom = false;
                        break;
                    }
                }
            }
        }
    }

    psInfo->m_pszSpatSRSDef = psOptions->osSpatSRSDef.empty()
                                  ? nullptr
                                  : psOptions->osSpatSRSDef.c_str();
    psInfo->m_hSpatialFilter =
        OGRGeometry::ToHandle(psOptions->poSpatialFilter.get());
    psInfo->m_pszGeomField =
        psOptions->bGeomFieldSet ? psOptions->osGeomField.c_str() : nullptr;

    if (psOptions->nTZOffsetInSec != TZ_OFFSET_INVALID && poDstFDefn)
    {
        for (int i = 0; i < poDstFDefn->GetFieldCount(); ++i)
        {
            if (poDstFDefn->GetFieldDefn(i)->GetType() == OFTDateTime)
            {
                psInfo->m_anDateTimeFieldIdx.push_back(i);
            }
        }
    }

    psInfo->m_bSupportCurves =
        CPL_TO_BOOL(poDstLayer->TestCapability(OLCCurveGeometries));

    return psInfo;
}

/************************************************************************/
/*                               SetupCT()                              */
/************************************************************************/

static bool
SetupCT(TargetLayerInfo *psInfo, OGRLayer *poSrcLayer, bool bTransform,
        bool bWrapDateline, const CPLString &osDateLineOffset,
        const OGRSpatialReference *poUserSourceSRS, OGRFeature *poFeature,
        const OGRSpatialReference *poOutputSRS,
        OGRCoordinateTransformation *poGCPCoordTrans, bool bVerboseError)
{
    OGRLayer *poDstLayer = psInfo->m_poDstLayer;
    const int nDstGeomFieldCount =
        poDstLayer->GetLayerDefn()->GetGeomFieldCount();
    for (int iGeom = 0; iGeom < nDstGeomFieldCount; iGeom++)
    {
        /* --------------------------------------------------------------------
         */
        /*      Setup coordinate transformation if we need it. */
        /* --------------------------------------------------------------------
         */
        const OGRSpatialReference *poSourceSRS = nullptr;
        OGRCoordinateTransformation *poCT = nullptr;
        char **papszTransformOptions = nullptr;

        int iSrcGeomField;
        auto poDstGeomFieldDefn =
            poDstLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
        if (psInfo->m_iRequestedSrcGeomField >= 0)
        {
            iSrcGeomField = psInfo->m_iRequestedSrcGeomField;
        }
        else
        {
            iSrcGeomField = poSrcLayer->GetLayerDefn()->GetGeomFieldIndex(
                poDstGeomFieldDefn->GetNameRef());
            if (iSrcGeomField < 0)
            {
                if (nDstGeomFieldCount == 1 &&
                    poSrcLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
                {
                    iSrcGeomField = 0;
                }
                else
                {
                    continue;
                }
            }
        }

        if (psInfo->m_nFeaturesRead == 0)
        {
            poSourceSRS = poUserSourceSRS;
            if (poSourceSRS == nullptr)
            {
                if (iSrcGeomField > 0)
                    poSourceSRS = poSrcLayer->GetLayerDefn()
                                      ->GetGeomFieldDefn(iSrcGeomField)
                                      ->GetSpatialRef();
                else
                    poSourceSRS = poSrcLayer->GetSpatialRef();
            }
        }
        if (poSourceSRS == nullptr)
        {
            if (poFeature == nullptr)
            {
                if (bVerboseError)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Non-null feature expected to set transformation");
                }
                return false;
            }
            OGRGeometry *poSrcGeometry =
                poFeature->GetGeomFieldRef(iSrcGeomField);
            if (poSrcGeometry)
                poSourceSRS = poSrcGeometry->getSpatialReference();
            psInfo->m_bPerFeatureCT = (bTransform || bWrapDateline);
        }

        if (bTransform)
        {
            if (poSourceSRS == nullptr && psInfo->m_pszCTPipeline == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Can't transform coordinates, source layer has no\n"
                         "coordinate system.  Use -s_srs to set one.");

                return false;
            }

            if (psInfo->m_pszCTPipeline == nullptr)
            {
                CPLAssert(nullptr != poSourceSRS);
                CPLAssert(nullptr != poOutputSRS);
            }

            if (psInfo->m_nFeaturesRead == 0 && !psInfo->m_bPerFeatureCT)
            {
                const auto &supportedSRSList =
                    poSrcLayer->GetSupportedSRSList(iGeom);
                if (!supportedSRSList.empty())
                {
                    const char *const apszOptions[] = {
                        "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES", nullptr};
                    for (const auto &poSRS : supportedSRSList)
                    {
                        if (poSRS->IsSame(poOutputSRS, apszOptions))
                        {
                            OGRSpatialReference oSourceSRSBackup;
                            if (poSourceSRS)
                                oSourceSRSBackup = *poSourceSRS;
                            if (poSrcLayer->SetActiveSRS(iGeom, poSRS.get()) ==
                                OGRERR_NONE)
                            {
                                CPLDebug("ogr2ogr",
                                         "Switching layer active SRS to %s",
                                         poSRS->GetName());

                                if (psInfo->m_hSpatialFilter != nullptr &&
                                    ((psInfo->m_iRequestedSrcGeomField < 0 &&
                                      iGeom == 0) ||
                                     (iGeom ==
                                      psInfo->m_iRequestedSrcGeomField)))
                                {
                                    OGRSpatialReference oSpatSRS;
                                    oSpatSRS.SetAxisMappingStrategy(
                                        OAMS_TRADITIONAL_GIS_ORDER);
                                    if (psInfo->m_pszSpatSRSDef)
                                        oSpatSRS.SetFromUserInput(
                                            psInfo->m_pszSpatSRSDef);
                                    ApplySpatialFilter(
                                        poSrcLayer,
                                        OGRGeometry::FromHandle(
                                            psInfo->m_hSpatialFilter),
                                        !oSpatSRS.IsEmpty() ? &oSpatSRS
                                        : !oSourceSRSBackup.IsEmpty()
                                            ? &oSourceSRSBackup
                                            : nullptr,
                                        psInfo->m_pszGeomField, poOutputSRS);
                                }

                                bTransform = false;
                            }
                            break;
                        }
                    }
                }
            }

            if (!bTransform)
            {
                // do nothing
            }
            else if (psInfo->m_aoReprojectionInfo[iGeom].m_poCT != nullptr &&
                     psInfo->m_aoReprojectionInfo[iGeom]
                             .m_poCT->GetSourceCS() == poSourceSRS)
            {
                poCT = psInfo->m_aoReprojectionInfo[iGeom].m_poCT.get();
            }
            else
            {
                OGRCoordinateTransformationOptions options;
                if (psInfo->m_pszCTPipeline)
                {
                    options.SetCoordinateOperation(psInfo->m_pszCTPipeline,
                                                   false);
                }
                poCT = OGRCreateCoordinateTransformation(poSourceSRS,
                                                         poOutputSRS, options);
                if (poCT == nullptr)
                {
                    char *pszWKT = nullptr;

                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Failed to create coordinate transformation "
                             "between the\n"
                             "following coordinate systems.  This may be "
                             "because they\n"
                             "are not transformable.");

                    if (poSourceSRS)
                    {
                        poSourceSRS->exportToPrettyWkt(&pszWKT, FALSE);
                        CPLError(CE_Failure, CPLE_AppDefined, "Source:\n%s",
                                 pszWKT);
                        CPLFree(pszWKT);
                    }

                    if (poOutputSRS)
                    {
                        poOutputSRS->exportToPrettyWkt(&pszWKT, FALSE);
                        CPLError(CE_Failure, CPLE_AppDefined, "Target:\n%s",
                                 pszWKT);
                        CPLFree(pszWKT);
                    }

                    return false;
                }
                poCT = new CompositeCT(poGCPCoordTrans, false, poCT, true);
                psInfo->m_aoReprojectionInfo[iGeom].m_poCT.reset(poCT);
                psInfo->m_aoReprojectionInfo[iGeom].m_bCanInvalidateValidity =
                    !(poGCPCoordTrans == nullptr && poSourceSRS &&
                      poSourceSRS->IsGeographic() && poOutputSRS &&
                      poOutputSRS->IsGeographic());
            }
        }
        else
        {
            const char *const apszOptions[] = {
                "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES",
                "CRITERION=EQUIVALENT", nullptr};
            auto poDstGeomFieldDefnSpatialRef =
                poDstGeomFieldDefn->GetSpatialRef();
            if (poSourceSRS && poDstGeomFieldDefnSpatialRef &&
                poSourceSRS->GetDataAxisToSRSAxisMapping() !=
                    poDstGeomFieldDefnSpatialRef
                        ->GetDataAxisToSRSAxisMapping() &&
                poSourceSRS->IsSame(poDstGeomFieldDefnSpatialRef, apszOptions))
            {
                psInfo->m_aoReprojectionInfo[iGeom].m_poCT.reset(
                    new CompositeCT(
                        new AxisMappingCoordinateTransformation(
                            poSourceSRS->GetDataAxisToSRSAxisMapping(),
                            poDstGeomFieldDefnSpatialRef
                                ->GetDataAxisToSRSAxisMapping()),
                        true, poGCPCoordTrans, false));
                poCT = psInfo->m_aoReprojectionInfo[iGeom].m_poCT.get();
            }
            else if (poGCPCoordTrans)
            {
                psInfo->m_aoReprojectionInfo[iGeom].m_poCT.reset(
                    new CompositeCT(poGCPCoordTrans, false, nullptr, false));
                poCT = psInfo->m_aoReprojectionInfo[iGeom].m_poCT.get();
            }
        }

        if (bWrapDateline)
        {
            if (bTransform && poCT != nullptr && poOutputSRS != nullptr &&
                poOutputSRS->IsGeographic())
            {
                papszTransformOptions =
                    CSLAddString(papszTransformOptions, "WRAPDATELINE=YES");
                if (!osDateLineOffset.empty())
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
                if (!osDateLineOffset.empty())
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
                if (!bHasWarned)
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "-wrapdateline option only works when "
                             "reprojecting to a geographic SRS");
                bHasWarned = true;
            }

            psInfo->m_aoReprojectionInfo[iGeom].m_aosTransformOptions.Assign(
                papszTransformOptions);
        }
    }
    return true;
}

/************************************************************************/
/*                 LayerTranslator::TranslateArrow()                    */
/************************************************************************/

bool LayerTranslator::TranslateArrow(
    const TargetLayerInfo *psInfo, GIntBig nCountLayerFeatures,
    GIntBig *pnReadFeatureCount, GDALProgressFunc pfnProgress,
    void *pProgressArg, const GDALVectorTranslateOptions *psOptions)
{
    struct ArrowArrayStream stream;
    struct ArrowSchema schema;
    CPLStringList aosOptionsGetArrowStream;
    CPLStringList aosOptionsWriteArrowBatch;
    aosOptionsGetArrowStream.SetNameValue("GEOMETRY_ENCODING", "WKB");
    if (!psInfo->m_bPreserveFID)
        aosOptionsGetArrowStream.SetNameValue("INCLUDE_FID", "NO");
    else
    {
        aosOptionsWriteArrowBatch.SetNameValue(
            "FID", psInfo->m_poSrcLayer->GetFIDColumn());
        aosOptionsWriteArrowBatch.SetNameValue("IF_FID_NOT_PRESERVED",
                                               "WARNING");
    }
    if (psOptions->nLimit >= 0)
    {
        aosOptionsGetArrowStream.SetNameValue(
            "MAX_FEATURES_IN_BATCH",
            CPLSPrintf(CPL_FRMT_GIB,
                       std::min<GIntBig>(psOptions->nLimit,
                                         (psOptions->nGroupTransactions > 0
                                              ? psOptions->nGroupTransactions
                                              : 65536))));
    }
    else if (psOptions->nGroupTransactions > 0)
    {
        aosOptionsGetArrowStream.SetNameValue(
            "MAX_FEATURES_IN_BATCH",
            CPLSPrintf("%d", psOptions->nGroupTransactions));
    }
    if (psInfo->m_poSrcLayer->GetArrowStream(&stream,
                                             aosOptionsGetArrowStream.List()))
    {
        if (stream.get_schema(&stream, &schema) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "stream.get_schema() failed");
            stream.release(&stream);
            return false;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetArrowStream() failed");
        return false;
    }

    bool bRet = true;

    GIntBig nCount = 0;
    bool bGoOn = true;
    while (bGoOn)
    {
        struct ArrowArray array;
        // Acquire source batch
        if (stream.get_next(&stream, &array) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "stream.get_next() failed");
            bRet = false;
            break;
        }

        if (array.release == nullptr)
        {
            // End of stream
            break;
        }

        // Limit number of features in batch if needed
        if (psOptions->nLimit >= 0 &&
            nCount + array.length >= psOptions->nLimit)
        {
            const auto nAdjustedLength = psOptions->nLimit - nCount;
            for (int i = 0; i < array.n_children; ++i)
            {
                if (array.children[i]->length == array.length)
                    array.children[i]->length = nAdjustedLength;
            }
            array.length = nAdjustedLength;
            nCount = psOptions->nLimit;
            bGoOn = false;
        }
        else
        {
            nCount += array.length;
        }

        // Write batch to target layer
        if (!psInfo->m_poDstLayer->WriteArrowBatch(
                &schema, &array, aosOptionsWriteArrowBatch.List()))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "WriteArrowBatch() failed");
            if (array.release)
                array.release(&array);
            bRet = false;
            break;
        }

        if (array.release)
            array.release(&array);

        /* Report progress */
        if (pfnProgress)
        {
            if (!pfnProgress(nCountLayerFeatures
                                 ? nCount * 1.0 / nCountLayerFeatures
                                 : 1.0,
                             "", pProgressArg))
            {
                bGoOn = false;
                bRet = false;
            }
        }

        if (pnReadFeatureCount)
            *pnReadFeatureCount = nCount;
    }

    schema.release(&schema);

    // Ugly hack to work around https://github.com/OSGeo/gdal/issues/9497
    // Deleting a RecordBatchReader obtained from arrow::dataset::Scanner.ToRecordBatchReader()
    // is a lengthy operation since all batches are read in its destructors.
    // Here we ask to our custom I/O layer to return in error to short circuit
    // that lengthy operation.
    if (auto poDS = psInfo->m_poSrcLayer->GetDataset())
    {
        if (poDS->GetLayerCount() == 1 && poDS->GetDriver() &&
            EQUAL(poDS->GetDriver()->GetDescription(), "PARQUET"))
        {
            bool bStopIO = false;
            const char *pszArrowStopIO =
                CPLGetConfigOption("OGR_ARROW_STOP_IO", nullptr);
            if (pszArrowStopIO && CPLTestBool(pszArrowStopIO))
            {
                bStopIO = true;
            }
            else if (!pszArrowStopIO)
            {
                std::string osExePath;
                osExePath.resize(1024);
                if (CPLGetExecPath(osExePath.data(),
                                   static_cast<int>(osExePath.size())))
                {
                    osExePath.resize(strlen(osExePath.data()));
                    if (strcmp(CPLGetBasename(osExePath.data()), "ogr2ogr") ==
                        0)
                    {
                        bStopIO = true;
                    }
                }
            }
            if (bStopIO)
            {
                CPLSetConfigOption("OGR_ARROW_STOP_IO", "YES");
                CPLDebug("OGR2OGR", "Forcing interruption of Parquet I/O");
            }
        }
    }

    stream.release(&stream);
    return bRet;
}

/************************************************************************/
/*                     LayerTranslator::Translate()                     */
/************************************************************************/

bool LayerTranslator::Translate(
    OGRFeature *poFeatureIn, TargetLayerInfo *psInfo,
    GIntBig nCountLayerFeatures, GIntBig *pnReadFeatureCount,
    GIntBig &nTotalEventsDone, GDALProgressFunc pfnProgress, void *pProgressArg,
    const GDALVectorTranslateOptions *psOptions)
{
    if (psInfo->m_bUseWriteArrowBatch)
    {
        return TranslateArrow(psInfo, nCountLayerFeatures, pnReadFeatureCount,
                              pfnProgress, pProgressArg, psOptions);
    }

    const int eGType = m_eGType;
    const OGRSpatialReference *poOutputSRS = m_poOutputSRS;

    OGRLayer *poSrcLayer = psInfo->m_poSrcLayer;
    OGRLayer *poDstLayer = psInfo->m_poDstLayer;
    const int *const panMap = psInfo->m_anMap.data();
    const int iSrcZField = psInfo->m_iSrcZField;
    const bool bPreserveFID = psInfo->m_bPreserveFID;
    const auto poSrcFDefn = poSrcLayer->GetLayerDefn();
    const auto poDstFDefn = poDstLayer->GetLayerDefn();
    const int nSrcGeomFieldCount = poSrcFDefn->GetGeomFieldCount();
    const int nDstGeomFieldCount = poDstFDefn->GetGeomFieldCount();
    const bool bExplodeCollections =
        m_bExplodeCollections && nDstGeomFieldCount <= 1;
    const int iRequestedSrcGeomField = psInfo->m_iRequestedSrcGeomField;

    if (poOutputSRS == nullptr && !m_bNullifyOutputSRS)
    {
        if (nSrcGeomFieldCount == 1)
        {
            poOutputSRS = poSrcLayer->GetSpatialRef();
        }
        else if (iRequestedSrcGeomField > 0)
        {
            poOutputSRS = poSrcLayer->GetLayerDefn()
                              ->GetGeomFieldDefn(iRequestedSrcGeomField)
                              ->GetSpatialRef();
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Transfer features.                                              */
    /* -------------------------------------------------------------------- */
    if (psOptions->nGroupTransactions)
    {
        if (psOptions->nLayerTransaction)
        {
            if (poDstLayer->StartTransaction() == OGRERR_FAILURE)
            {
                delete poFeatureIn;
                return false;
            }
        }
    }

    std::unique_ptr<OGRFeature> poFeature;
    std::unique_ptr<OGRFeature> poDstFeature(new OGRFeature(poDstFDefn));
    int nFeaturesInTransaction = 0;
    GIntBig nCount = 0; /* written + failed */
    GIntBig nFeaturesWritten = 0;
    bool bRunSetPrecisionEvaluated = false;
    bool bRunSetPrecision = false;

    bool bRet = true;
    CPLErrorReset();

    bool bSetupCTOK = false;
    if (m_bTransform && psInfo->m_nFeaturesRead == 0 &&
        !psInfo->m_bPerFeatureCT)
    {
        bSetupCTOK = SetupCT(psInfo, poSrcLayer, m_bTransform, m_bWrapDateline,
                             m_osDateLineOffset, m_poUserSourceSRS, nullptr,
                             poOutputSRS, m_poGCPCoordTrans, false);
    }

    while (true)
    {
        if (m_nLimit >= 0 && psInfo->m_nFeaturesRead >= m_nLimit)
        {
            break;
        }

        if (poFeatureIn != nullptr)
            poFeature.reset(poFeatureIn);
        else if (psOptions->nFIDToFetch != OGRNullFID)
            poFeature.reset(poSrcLayer->GetFeature(psOptions->nFIDToFetch));
        else
            poFeature.reset(poSrcLayer->GetNextFeature());

        if (poFeature == nullptr)
        {
            if (CPLGetLastErrorType() == CE_Failure)
            {
                bRet = false;
            }
            break;
        }

        if (!bSetupCTOK &&
            (psInfo->m_nFeaturesRead == 0 || psInfo->m_bPerFeatureCT))
        {
            if (!SetupCT(psInfo, poSrcLayer, m_bTransform, m_bWrapDateline,
                         m_osDateLineOffset, m_poUserSourceSRS, poFeature.get(),
                         poOutputSRS, m_poGCPCoordTrans, true))
            {
                return false;
            }
        }

        psInfo->m_nFeaturesRead++;

        int nIters = 1;
        std::unique_ptr<OGRGeometryCollection> poCollToExplode;
        int iGeomCollToExplode = -1;
        if (bExplodeCollections)
        {
            OGRGeometry *poSrcGeometry;
            if (iRequestedSrcGeomField >= 0)
                poSrcGeometry =
                    poFeature->GetGeomFieldRef(iRequestedSrcGeomField);
            else
                poSrcGeometry = poFeature->GetGeometryRef();
            if (poSrcGeometry &&
                OGR_GT_IsSubClassOf(poSrcGeometry->getGeometryType(),
                                    wkbGeometryCollection))
            {
                const int nParts =
                    poSrcGeometry->toGeometryCollection()->getNumGeometries();
                if (nParts > 0)
                {
                    iGeomCollToExplode = iRequestedSrcGeomField >= 0
                                             ? iRequestedSrcGeomField
                                             : 0;
                    poCollToExplode.reset(
                        poFeature->StealGeometry(iGeomCollToExplode)
                            ->toGeometryCollection());
                    nIters = nParts;
                }
            }
        }

        const GIntBig nSrcFID = poFeature->GetFID();
        GIntBig nDesiredFID = OGRNullFID;
        if (bPreserveFID)
            nDesiredFID = nSrcFID;
        else if (psInfo->m_iSrcFIDField >= 0 &&
                 poFeature->IsFieldSetAndNotNull(psInfo->m_iSrcFIDField))
            nDesiredFID =
                poFeature->GetFieldAsInteger64(psInfo->m_iSrcFIDField);

        for (int iPart = 0; iPart < nIters; iPart++)
        {
            if (psOptions->nLayerTransaction &&
                ++nFeaturesInTransaction == psOptions->nGroupTransactions)
            {
                if (poDstLayer->CommitTransaction() == OGRERR_FAILURE ||
                    poDstLayer->StartTransaction() == OGRERR_FAILURE)
                {
                    return false;
                }
                nFeaturesInTransaction = 0;
            }
            else if (!psOptions->nLayerTransaction &&
                     psOptions->nGroupTransactions > 0 &&
                     ++nTotalEventsDone >= psOptions->nGroupTransactions)
            {
                if (m_poODS->CommitTransaction() == OGRERR_FAILURE ||
                    m_poODS->StartTransaction(psOptions->bForceTransaction) ==
                        OGRERR_FAILURE)
                {
                    return false;
                }
                nTotalEventsDone = 0;
            }

            CPLErrorReset();
            if (psInfo->m_bCanAvoidSetFrom)
            {
                poDstFeature = std::move(poFeature);
                // From now on, poFeature is null !
                poDstFeature->SetFDefnUnsafe(poDstFDefn);
                poDstFeature->SetFID(nDesiredFID);
            }
            else
            {
                /* Optimization to avoid duplicating the source geometry in the
                 */
                /* target feature : we steal it from the source feature for
                 * now... */
                std::unique_ptr<OGRGeometry> poStolenGeometry;
                if (!bExplodeCollections && nSrcGeomFieldCount == 1 &&
                    (nDstGeomFieldCount == 1 ||
                     (nDstGeomFieldCount == 0 && m_poClipSrcOri)))
                {
                    poStolenGeometry.reset(poFeature->StealGeometry());
                }
                else if (!bExplodeCollections && iRequestedSrcGeomField >= 0)
                {
                    poStolenGeometry.reset(
                        poFeature->StealGeometry(iRequestedSrcGeomField));
                }

                if (nDstGeomFieldCount == 0 && poStolenGeometry &&
                    m_poClipSrcOri)
                {
                    const OGRGeometry *poClipGeom =
                        GetSrcClipGeom(poStolenGeometry->getSpatialReference());

                    if (poClipGeom != nullptr &&
                        !poClipGeom->Intersects(poStolenGeometry.get()))
                    {
                        goto end_loop;
                    }
                }

                poDstFeature->Reset();
                if (poDstFeature->SetFrom(poFeature.get(), panMap, TRUE) !=
                    OGRERR_NONE)
                {
                    if (psOptions->nGroupTransactions)
                    {
                        if (psOptions->nLayerTransaction)
                        {
                            if (poDstLayer->CommitTransaction() != OGRERR_NONE)
                            {
                                return false;
                            }
                        }
                    }

                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to translate feature " CPL_FRMT_GIB
                             " from layer %s.",
                             nSrcFID, poSrcLayer->GetName());

                    return false;
                }

                /* ... and now we can attach the stolen geometry */
                if (poStolenGeometry)
                {
                    poDstFeature->SetGeometryDirectly(
                        poStolenGeometry.release());
                }

                if (!psInfo->m_oMapResolved.empty())
                {
                    for (const auto &kv : psInfo->m_oMapResolved)
                    {
                        const int nDstField = kv.first;
                        const int nSrcField = kv.second.nSrcField;
                        if (poFeature->IsFieldSetAndNotNull(nSrcField))
                        {
                            const auto poDomain = kv.second.poDomain;
                            const auto &oMapKV =
                                psInfo->m_oMapDomainToKV[poDomain];
                            const auto iter = oMapKV.find(
                                poFeature->GetFieldAsString(nSrcField));
                            if (iter != oMapKV.end())
                            {
                                poDstFeature->SetField(nDstField,
                                                       iter->second.c_str());
                            }
                        }
                    }
                }

                if (nDesiredFID != OGRNullFID)
                    poDstFeature->SetFID(nDesiredFID);
            }

            if (psOptions->bEmptyStrAsNull)
            {
                for (int i = 0; i < poDstFeature->GetFieldCount(); i++)
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

            if (!psInfo->m_anDateTimeFieldIdx.empty())
            {
                for (int i : psInfo->m_anDateTimeFieldIdx)
                {
                    if (!poDstFeature->IsFieldSetAndNotNull(i))
                        continue;
                    auto psField = poDstFeature->GetRawFieldRef(i);
                    if (psField->Date.TZFlag == 0 || psField->Date.TZFlag == 1)
                        continue;

                    const int nTZOffsetInSec =
                        (psField->Date.TZFlag - 100) * 15 * 60;
                    if (nTZOffsetInSec == psOptions->nTZOffsetInSec)
                        continue;

                    struct tm brokendowntime;
                    memset(&brokendowntime, 0, sizeof(brokendowntime));
                    brokendowntime.tm_year = psField->Date.Year - 1900;
                    brokendowntime.tm_mon = psField->Date.Month - 1;
                    brokendowntime.tm_mday = psField->Date.Day;
                    GIntBig nUnixTime = CPLYMDHMSToUnixTime(&brokendowntime);
                    int nSec = psField->Date.Hour * 3600 +
                               psField->Date.Minute * 60 +
                               static_cast<int>(psField->Date.Second);
                    nSec += psOptions->nTZOffsetInSec - nTZOffsetInSec;
                    nUnixTime += nSec;
                    CPLUnixTimeToYMDHMS(nUnixTime, &brokendowntime);

                    psField->Date.Year =
                        static_cast<GInt16>(brokendowntime.tm_year + 1900);
                    psField->Date.Month =
                        static_cast<GByte>(brokendowntime.tm_mon + 1);
                    psField->Date.Day =
                        static_cast<GByte>(brokendowntime.tm_mday);
                    psField->Date.Hour =
                        static_cast<GByte>(brokendowntime.tm_hour);
                    psField->Date.Minute =
                        static_cast<GByte>(brokendowntime.tm_min);
                    psField->Date.Second = static_cast<float>(
                        brokendowntime.tm_sec + fmod(psField->Date.Second, 1));
                    psField->Date.TZFlag = static_cast<GByte>(
                        100 + psOptions->nTZOffsetInSec / (15 * 60));
                }
            }

            /* Erase native data if asked explicitly */
            if (!m_bNativeData)
            {
                poDstFeature->SetNativeData(nullptr);
                poDstFeature->SetNativeMediaType(nullptr);
            }

            for (int iGeom = 0; iGeom < nDstGeomFieldCount; iGeom++)
            {
                std::unique_ptr<OGRGeometry> poDstGeometry;

                if (poCollToExplode && iGeom == iGeomCollToExplode)
                {
                    OGRGeometry *poPart = poCollToExplode->getGeometryRef(0);
                    poCollToExplode->removeGeometry(0, FALSE);
                    poDstGeometry.reset(poPart);
                }
                else
                {
                    poDstGeometry.reset(poDstFeature->StealGeometry(iGeom));
                }
                if (poDstGeometry == nullptr)
                    continue;

                // poFeature hasn't been moved if iSrcZField != -1
                // cppcheck-suppress accessMoved
                if (iSrcZField != -1 && poFeature != nullptr)
                {
                    SetZ(poDstGeometry.get(),
                         poFeature->GetFieldAsDouble(iSrcZField));
                    /* This will correct the coordinate dimension to 3 */
                    poDstGeometry.reset(poDstGeometry->clone());
                }

                if (m_nCoordDim == 2 || m_nCoordDim == 3)
                {
                    poDstGeometry->setCoordinateDimension(m_nCoordDim);
                }
                else if (m_nCoordDim == 4)
                {
                    poDstGeometry->set3D(TRUE);
                    poDstGeometry->setMeasured(TRUE);
                }
                else if (m_nCoordDim == COORD_DIM_XYM)
                {
                    poDstGeometry->set3D(FALSE);
                    poDstGeometry->setMeasured(TRUE);
                }
                else if (m_nCoordDim == COORD_DIM_LAYER_DIM)
                {
                    const OGRwkbGeometryType eDstLayerGeomType =
                        poDstLayer->GetLayerDefn()
                            ->GetGeomFieldDefn(iGeom)
                            ->GetType();
                    poDstGeometry->set3D(wkbHasZ(eDstLayerGeomType));
                    poDstGeometry->setMeasured(wkbHasM(eDstLayerGeomType));
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
                        auto poNewGeom = std::unique_ptr<OGRGeometry>(
                            poDstGeometry->SimplifyPreserveTopology(
                                m_dfGeomOpParam));
                        if (poNewGeom)
                        {
                            poDstGeometry = std::move(poNewGeom);
                        }
                    }
                }

                if (m_poClipSrcOri)
                {

                    const OGRGeometry *poClipGeom =
                        GetSrcClipGeom(poDstGeometry->getSpatialReference());

                    std::unique_ptr<OGRGeometry> poClipped;
                    if (poClipGeom != nullptr)
                    {
                        OGREnvelope oClipEnv;
                        OGREnvelope oDstEnv;

                        poClipGeom->getEnvelope(&oClipEnv);
                        poDstGeometry->getEnvelope(&oDstEnv);

                        if (oClipEnv.Intersects(oDstEnv))
                        {
                            poClipped.reset(
                                poClipGeom->Intersection(poDstGeometry.get()));
                        }
                    }

                    if (poClipped == nullptr || poClipped->IsEmpty())
                    {
                        goto end_loop;
                    }

                    const int nDim = poDstGeometry->getDimension();
                    if (poClipped->getDimension() < nDim &&
                        wkbFlatten(
                            poDstFDefn->GetGeomFieldDefn(iGeom)->GetType()) !=
                            wkbUnknown)
                    {
                        CPLDebug(
                            "OGR2OGR",
                            "Discarding feature " CPL_FRMT_GIB " of layer %s, "
                            "as its intersection with -clipsrc is a %s "
                            "whereas the input is a %s",
                            nSrcFID, poSrcLayer->GetName(),
                            OGRToOGCGeomType(poClipped->getGeometryType()),
                            OGRToOGCGeomType(poDstGeometry->getGeometryType()));
                        goto end_loop;
                    }

                    poDstGeometry = std::move(poClipped);
                }

                OGRCoordinateTransformation *const poCT =
                    psInfo->m_aoReprojectionInfo[iGeom].m_poCT.get();
                char **const papszTransformOptions =
                    psInfo->m_aoReprojectionInfo[iGeom]
                        .m_aosTransformOptions.List();
                const bool bReprojCanInvalidateValidity =
                    psInfo->m_aoReprojectionInfo[iGeom]
                        .m_bCanInvalidateValidity;

                if (poCT != nullptr || papszTransformOptions != nullptr)
                {
                    // If we need to change the geometry type to linear, and
                    // we have a geometry with curves, then convert it to
                    // linear first, to avoid invalidities due to the fact
                    // that validity of arc portions isn't always kept while
                    // reprojecting and then discretizing.
                    if (bReprojCanInvalidateValidity &&
                        (!psInfo->m_bSupportCurves ||
                         m_eGeomTypeConversion == GTC_CONVERT_TO_LINEAR ||
                         m_eGeomTypeConversion ==
                             GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR))
                    {
                        if (poDstGeometry->hasCurveGeometry(TRUE))
                        {
                            OGRwkbGeometryType eTargetType = OGR_GT_GetLinear(
                                poDstGeometry->getGeometryType());
                            poDstGeometry.reset(OGRGeometryFactory::forceTo(
                                poDstGeometry.release(), eTargetType));
                        }
                    }
                    else if (bReprojCanInvalidateValidity &&
                             eGType != GEOMTYPE_UNCHANGED &&
                             !OGR_GT_IsNonLinear(
                                 static_cast<OGRwkbGeometryType>(eGType)) &&
                             poDstGeometry->hasCurveGeometry(TRUE))
                    {
                        poDstGeometry.reset(OGRGeometryFactory::forceTo(
                            poDstGeometry.release(),
                            static_cast<OGRwkbGeometryType>(eGType)));
                    }

                    for (int iIter = 0; iIter < 2; ++iIter)
                    {
                        auto poReprojectedGeom = std::unique_ptr<OGRGeometry>(
                            OGRGeometryFactory::transformWithOptions(
                                poDstGeometry.get(), poCT,
                                papszTransformOptions,
                                m_transformWithOptionsCache));
                        if (poReprojectedGeom == nullptr)
                        {
                            if (psOptions->nGroupTransactions)
                            {
                                if (psOptions->nLayerTransaction)
                                {
                                    if (poDstLayer->CommitTransaction() !=
                                            OGRERR_NONE &&
                                        !psOptions->bSkipFailures)
                                    {
                                        return false;
                                    }
                                }
                            }

                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Failed to reproject feature " CPL_FRMT_GIB
                                     " (geometry probably out of source or "
                                     "destination SRS).",
                                     nSrcFID);
                            if (!psOptions->bSkipFailures)
                            {
                                return false;
                            }
                        }

                        // Check if a curve geometry is no longer valid after
                        // reprojection
                        const auto eType = poDstGeometry->getGeometryType();
                        const auto eFlatType = wkbFlatten(eType);

                        const auto IsValid = [](const OGRGeometry *poGeom)
                        {
                            CPLErrorHandlerPusher oErrorHandler(
                                CPLQuietErrorHandler);
                            return poGeom->IsValid();
                        };

                        if (iIter == 0 && bReprojCanInvalidateValidity &&
                            OGRGeometryFactory::haveGEOS() &&
                            (eFlatType == wkbCurvePolygon ||
                             eFlatType == wkbCompoundCurve ||
                             eFlatType == wkbMultiCurve ||
                             eFlatType == wkbMultiSurface) &&
                            poDstGeometry->hasCurveGeometry(TRUE) &&
                            IsValid(poDstGeometry.get()))
                        {
                            OGRwkbGeometryType eTargetType = OGR_GT_GetLinear(
                                poDstGeometry->getGeometryType());
                            auto poDstGeometryTmp =
                                std::unique_ptr<OGRGeometry>(
                                    OGRGeometryFactory::forceTo(
                                        poReprojectedGeom->clone(),
                                        eTargetType));
                            if (!IsValid(poDstGeometryTmp.get()))
                            {
                                CPLDebug("OGR2OGR",
                                         "Curve geometry no longer valid after "
                                         "reprojection: transforming it into "
                                         "linear one before reprojecting");
                                poDstGeometry.reset(OGRGeometryFactory::forceTo(
                                    poDstGeometry.release(), eTargetType));
                                poDstGeometry.reset(OGRGeometryFactory::forceTo(
                                    poDstGeometry.release(), eType));
                            }
                            else
                            {
                                poDstGeometry = std::move(poReprojectedGeom);
                                break;
                            }
                        }
                        else
                        {
                            poDstGeometry = std::move(poReprojectedGeom);
                            break;
                        }
                    }
                }
                else if (poOutputSRS != nullptr)
                {
                    poDstGeometry->assignSpatialReference(poOutputSRS);
                }

                if (poDstGeometry != nullptr)
                {
                    if (m_poClipDstOri)
                    {
                        const OGRGeometry *poClipGeom = GetDstClipGeom(
                            poDstGeometry->getSpatialReference());
                        if (poClipGeom == nullptr)
                        {
                            goto end_loop;
                        }

                        std::unique_ptr<OGRGeometry> poClipped;

                        OGREnvelope oClipEnv;
                        OGREnvelope oDstEnv;

                        poClipGeom->getEnvelope(&oClipEnv);
                        poDstGeometry->getEnvelope(&oDstEnv);

                        if (oClipEnv.Intersects(oDstEnv))
                        {
                            poClipped.reset(
                                poClipGeom->Intersection(poDstGeometry.get()));
                        }

                        if (poClipped == nullptr || poClipped->IsEmpty())
                        {
                            goto end_loop;
                        }

                        const int nDim = poDstGeometry->getDimension();
                        if (poClipped->getDimension() < nDim &&
                            wkbFlatten(poDstFDefn->GetGeomFieldDefn(iGeom)
                                           ->GetType()) != wkbUnknown)
                        {
                            CPLDebug(
                                "OGR2OGR",
                                "Discarding feature " CPL_FRMT_GIB
                                " of layer %s, "
                                "as its intersection with -clipdst is a %s "
                                "whereas the input is a %s",
                                nSrcFID, poSrcLayer->GetName(),
                                OGRToOGCGeomType(poClipped->getGeometryType()),
                                OGRToOGCGeomType(
                                    poDstGeometry->getGeometryType()));
                            goto end_loop;
                        }

                        poDstGeometry = std::move(poClipped);
                    }

                    if (psOptions->dfXYRes !=
                            OGRGeomCoordinatePrecision::UNKNOWN &&
                        OGRGeometryFactory::haveGEOS() &&
                        !poDstGeometry->hasCurveGeometry())
                    {
                        // OGR_APPLY_GEOM_SET_PRECISION default value for
                        // OGRLayer::CreateFeature() purposes, but here in the
                        // ogr2ogr -xyRes context, we force calling SetPrecision(),
                        // unless the user explicitly asks not to do it by
                        // setting the config option to NO.
                        if (!bRunSetPrecisionEvaluated)
                        {
                            bRunSetPrecisionEvaluated = true;
                            bRunSetPrecision = CPLTestBool(CPLGetConfigOption(
                                "OGR_APPLY_GEOM_SET_PRECISION", "YES"));
                        }
                        if (bRunSetPrecision)
                        {
                            auto poNewGeom = std::unique_ptr<OGRGeometry>(
                                poDstGeometry->SetPrecision(psOptions->dfXYRes,
                                                            /* nFlags = */ 0));
                            if (!poNewGeom)
                                goto end_loop;
                            poDstGeometry = std::move(poNewGeom);
                        }
                    }

                    if (m_bMakeValid)
                    {
                        const bool bIsGeomCollection =
                            wkbFlatten(poDstGeometry->getGeometryType()) ==
                            wkbGeometryCollection;
                        auto poNewGeom = std::unique_ptr<OGRGeometry>(
                            poDstGeometry->MakeValid());
                        if (!poNewGeom)
                            goto end_loop;
                        poDstGeometry = std::move(poNewGeom);
                        if (!bIsGeomCollection)
                        {
                            poDstGeometry.reset(
                                OGRGeometryFactory::
                                    removeLowerDimensionSubGeoms(
                                        poDstGeometry.get()));
                        }
                    }

                    if (m_eGeomTypeConversion != GTC_DEFAULT)
                    {
                        OGRwkbGeometryType eTargetType =
                            poDstGeometry->getGeometryType();
                        eTargetType =
                            ConvertType(m_eGeomTypeConversion, eTargetType);
                        poDstGeometry.reset(OGRGeometryFactory::forceTo(
                            poDstGeometry.release(), eTargetType));
                    }
                    else if (eGType != GEOMTYPE_UNCHANGED)
                    {
                        poDstGeometry.reset(OGRGeometryFactory::forceTo(
                            poDstGeometry.release(),
                            static_cast<OGRwkbGeometryType>(eGType)));
                    }
                }

                poDstFeature->SetGeomFieldDirectly(iGeom,
                                                   poDstGeometry.release());
            }

            CPLErrorReset();
            if ((psOptions->bUpsert
                     ? poDstLayer->UpsertFeature(poDstFeature.get())
                     : poDstLayer->CreateFeature(poDstFeature.get())) ==
                OGRERR_NONE)
            {
                nFeaturesWritten++;
                if (nDesiredFID != OGRNullFID &&
                    poDstFeature->GetFID() != nDesiredFID)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Feature id " CPL_FRMT_GIB " not preserved",
                             nDesiredFID);
                }
            }
            else if (!psOptions->bSkipFailures)
            {
                if (psOptions->nGroupTransactions)
                {
                    if (psOptions->nLayerTransaction)
                        poDstLayer->RollbackTransaction();
                }

                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to write feature " CPL_FRMT_GIB
                         " from layer %s.",
                         nSrcFID, poSrcLayer->GetName());

                return false;
            }
            else
            {
                CPLDebug("GDALVectorTranslate",
                         "Unable to write feature " CPL_FRMT_GIB
                         " into layer %s.",
                         nSrcFID, poSrcLayer->GetName());
                if (psOptions->nGroupTransactions)
                {
                    if (psOptions->nLayerTransaction)
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

        end_loop:;  // nothing
        }

        /* Report progress */
        nCount++;
        bool bGoOn = true;
        if (pfnProgress)
        {
            bGoOn = pfnProgress(nCountLayerFeatures
                                    ? nCount * 1.0 / nCountLayerFeatures
                                    : 1.0,
                                "", pProgressArg) != FALSE;
        }
        if (!bGoOn)
        {
            bRet = false;
            break;
        }

        if (pnReadFeatureCount)
            *pnReadFeatureCount = nCount;

        if (psOptions->nFIDToFetch != OGRNullFID)
            break;
        if (poFeatureIn != nullptr)
            break;
    }

    if (psOptions->nGroupTransactions)
    {
        if (psOptions->nLayerTransaction)
        {
            if (poDstLayer->CommitTransaction() != OGRERR_NONE)
                bRet = false;
        }
    }

    if (poFeatureIn == nullptr)
    {
        CPLDebug("GDALVectorTranslate",
                 CPL_FRMT_GIB " features written in layer '%s'",
                 nFeaturesWritten, poDstLayer->GetName());
    }

    return bRet;
}

/************************************************************************/
/*                LayerTranslator::GetDstClipGeom()                     */
/************************************************************************/

const OGRGeometry *
LayerTranslator::GetDstClipGeom(const OGRSpatialReference *poGeomSRS)
{
    if (m_poClipDstReprojectedToDstSRS_SRS != poGeomSRS)
    {
        auto poClipDstSRS = m_poClipDstOri->getSpatialReference();
        if (poClipDstSRS && poGeomSRS && !poClipDstSRS->IsSame(poGeomSRS))
        {
            // Transform clip geom to geometry SRS
            m_poClipDstReprojectedToDstSRS.reset(m_poClipDstOri->clone());
            if (m_poClipDstReprojectedToDstSRS->transformTo(poGeomSRS) !=
                OGRERR_NONE)
            {
                return nullptr;
            }
            m_poClipDstReprojectedToDstSRS_SRS = poGeomSRS;
        }
        else if (!poClipDstSRS && poGeomSRS)
        {
            if (!m_bWarnedClipDstSRS)
            {
                m_bWarnedClipDstSRS = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Clip destination geometry has no "
                         "attached SRS, but the feature's "
                         "geometry has one. Assuming clip "
                         "destination geometry SRS is the "
                         "same as the feature's geometry");
            }
        }
    }

    return m_poClipDstReprojectedToDstSRS ? m_poClipDstReprojectedToDstSRS.get()
                                          : m_poClipDstOri;
}

/************************************************************************/
/*                LayerTranslator::GetSrcClipGeom()                     */
/************************************************************************/

const OGRGeometry *
LayerTranslator::GetSrcClipGeom(const OGRSpatialReference *poGeomSRS)
{
    if (m_poClipSrcReprojectedToSrcSRS_SRS != poGeomSRS)
    {
        auto poClipSrcSRS = m_poClipSrcOri->getSpatialReference();
        if (poClipSrcSRS && poGeomSRS && !poClipSrcSRS->IsSame(poGeomSRS))
        {
            // Transform clip geom to geometry SRS
            m_poClipSrcReprojectedToSrcSRS.reset(m_poClipSrcOri->clone());
            if (m_poClipSrcReprojectedToSrcSRS->transformTo(poGeomSRS) !=
                OGRERR_NONE)
            {
                return nullptr;
            }
            m_poClipSrcReprojectedToSrcSRS_SRS = poGeomSRS;
        }
        else if (!poClipSrcSRS && poGeomSRS)
        {
            if (!m_bWarnedClipSrcSRS)
            {
                m_bWarnedClipSrcSRS = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Clip source geometry has no attached SRS, "
                         "but the feature's geometry has one. "
                         "Assuming clip source geometry SRS is the "
                         "same as the feature's geometry");
            }
        }
    }

    return m_poClipSrcReprojectedToSrcSRS ? m_poClipSrcReprojectedToSrcSRS.get()
                                          : m_poClipSrcOri;
}

/************************************************************************/
/*                   GDALVectorTranslateOptionsGetParser()              */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser> GDALVectorTranslateOptionsGetParser(
    GDALVectorTranslateOptions *psOptions,
    GDALVectorTranslateOptionsForBinary *psOptionsForBinary, int nCountClipSrc,
    int nCountClipDst)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "ogr2ogr", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(
        _("Converts simple features data between file formats."));

    argParser->add_epilog(
        _("For more details, consult https://gdal.org/programs/ogr2ogr.html"));

    argParser->add_output_format_argument(psOptions->osFormat);

    argParser->add_argument("-dsco")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosDSCO.AddString(s.c_str()); })
        .help(_("Dataset creation option (format specific)."));

    argParser->add_argument("-lco")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosLCO.AddString(s.c_str()); })
        .help(_("Layer creation option (format specific)."));

    argParser->add_usage_newline();

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-append")
            .flag()
            .action([psOptions](const std::string &)
                    { psOptions->eAccessMode = ACCESS_APPEND; })
            .help(_("Append to existing layer instead of creating new."));

        group.add_argument("-upsert")
            .flag()
            .action(
                [psOptions](const std::string &)
                {
                    psOptions->eAccessMode = ACCESS_APPEND;
                    psOptions->bUpsert = true;
                })
            .help(_("Variant of -append where the UpsertFeature() operation is "
                    "used to insert or update features."));

        group.add_argument("-overwrite")
            .flag()
            .action([psOptions](const std::string &)
                    { psOptions->eAccessMode = ACCESS_OVERWRITE; })
            .help(_("Delete the output layer and recreate it empty."));
    }

    argParser->add_argument("-update")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                /* Don't reset -append or -overwrite */
                if (psOptions->eAccessMode != ACCESS_APPEND &&
                    psOptions->eAccessMode != ACCESS_OVERWRITE)
                    psOptions->eAccessMode = ACCESS_UPDATE;
            })
        .help(_("Open existing output datasource in update mode rather than "
                "trying to create a new one."));

    argParser->add_argument("-sql")
        .metavar("<statement>|@<filename>")
        .action(
            [psOptions](const std::string &s)
            {
                GByte *pabyRet = nullptr;
                if (!s.empty() && s.front() == '@' &&
                    VSIIngestFile(nullptr, s.c_str() + 1, &pabyRet, nullptr,
                                  1024 * 1024))
                {
                    GDALRemoveBOM(pabyRet);
                    char *pszSQLStatement = reinterpret_cast<char *>(pabyRet);
                    psOptions->osSQLStatement =
                        GDALRemoveSQLComments(pszSQLStatement);
                    VSIFree(pszSQLStatement);
                }
                else
                {
                    psOptions->osSQLStatement = s;
                }
            })
        .help(_("SQL statement to execute."));

    argParser->add_argument("-dialect")
        .metavar("<dialect>")
        .store_into(psOptions->osDialect)
        .help(_("SQL dialect."));

    argParser->add_argument("-spat")
        .metavar("<xmin> <ymin> <xmax> <ymax>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("Spatial query extents, in the SRS of the source layer(s) (or "
                "the one specified with -spat_srs."));

    argParser->add_argument("-where")
        .metavar("<restricted_where>|@<filename>")
        .action(
            [psOptions](const std::string &s)
            {
                GByte *pabyRet = nullptr;
                if (!s.empty() && s.front() == '@' &&
                    VSIIngestFile(nullptr, s.c_str() + 1, &pabyRet, nullptr,
                                  1024 * 1024))
                {
                    GDALRemoveBOM(pabyRet);
                    char *pszWHERE = reinterpret_cast<char *>(pabyRet);
                    psOptions->osWHERE = pszWHERE;
                    VSIFree(pszWHERE);
                }
                else
                {
                    psOptions->osWHERE = s;
                }
            })
        .help(_("Attribute query (like SQL WHERE)."));

    argParser->add_argument("-select")
        .metavar("<field_list>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->bSelFieldsSet = true;
                psOptions->aosSelFields =
                    CSLTokenizeStringComplex(s.c_str(), ",", TRUE, FALSE);
            })
        .help(_("Comma-delimited list of fields from input layer to copy to "
                "the new layer."));

    argParser->add_argument("-nln")
        .metavar("<name>")
        .store_into(psOptions->osNewLayerName)
        .help(_("Assign an alternate name to the new layer."));

    argParser->add_argument("-nlt")
        .metavar("<type>")
        .append()
        .action(
            [psOptions](const std::string &osGeomNameIn)
            {
                bool bIs3D = false;
                std::string osGeomName(osGeomNameIn);
                if (osGeomName.size() > 3 &&
                    STARTS_WITH_CI(osGeomName.c_str() + osGeomName.size() - 3,
                                   "25D"))
                {
                    bIs3D = true;
                    osGeomName.resize(osGeomName.size() - 3);
                }
                else if (osGeomName.size() > 1 &&
                         STARTS_WITH_CI(
                             osGeomName.c_str() + osGeomName.size() - 1, "Z"))
                {
                    bIs3D = true;
                    osGeomName.resize(osGeomName.size() - 1);
                }
                if (EQUAL(osGeomName.c_str(), "NONE"))
                {
                    if (psOptions->eGType != GEOMTYPE_UNCHANGED)
                    {
                        throw std::invalid_argument(
                            "Unsupported combination of -nlt arguments.");
                    }
                    psOptions->eGType = wkbNone;
                }
                else if (EQUAL(osGeomName.c_str(), "GEOMETRY"))
                {
                    if (psOptions->eGType != GEOMTYPE_UNCHANGED)
                    {
                        throw std::invalid_argument(
                            "Unsupported combination of -nlt arguments.");
                    }
                    psOptions->eGType = wkbUnknown;
                }
                else if (EQUAL(osGeomName.c_str(), "PROMOTE_TO_MULTI"))
                {
                    if (psOptions->eGeomTypeConversion == GTC_CONVERT_TO_LINEAR)
                        psOptions->eGeomTypeConversion =
                            GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR;
                    else if (psOptions->eGeomTypeConversion == GTC_DEFAULT)
                        psOptions->eGeomTypeConversion = GTC_PROMOTE_TO_MULTI;
                    else
                    {
                        throw std::invalid_argument(
                            "Unsupported combination of -nlt arguments.");
                    }
                }
                else if (EQUAL(osGeomName.c_str(), "CONVERT_TO_LINEAR"))
                {
                    if (psOptions->eGeomTypeConversion == GTC_PROMOTE_TO_MULTI)
                        psOptions->eGeomTypeConversion =
                            GTC_PROMOTE_TO_MULTI_AND_CONVERT_TO_LINEAR;
                    else if (psOptions->eGeomTypeConversion == GTC_DEFAULT)
                        psOptions->eGeomTypeConversion = GTC_CONVERT_TO_LINEAR;
                    else
                    {
                        throw std::invalid_argument(
                            "Unsupported combination of -nlt arguments.");
                    }
                }
                else if (EQUAL(osGeomName.c_str(), "CONVERT_TO_CURVE"))
                {
                    if (psOptions->eGeomTypeConversion == GTC_DEFAULT)
                        psOptions->eGeomTypeConversion = GTC_CONVERT_TO_CURVE;
                    else
                    {
                        throw std::invalid_argument(
                            "Unsupported combination of -nlt arguments.");
                    }
                }
                else
                {
                    if (psOptions->eGType != GEOMTYPE_UNCHANGED)
                    {
                        throw std::invalid_argument(
                            "Unsupported combination of -nlt arguments.");
                    }
                    psOptions->eGType = OGRFromOGCGeomType(osGeomName.c_str());
                    if (psOptions->eGType == wkbUnknown)
                    {
                        throw std::invalid_argument(
                            CPLSPrintf("-nlt %s: type not recognised.",
                                       osGeomName.c_str()));
                    }
                }
                if (psOptions->eGType != GEOMTYPE_UNCHANGED &&
                    psOptions->eGType != wkbNone && bIs3D)
                    psOptions->eGType = wkbSetZ(
                        static_cast<OGRwkbGeometryType>(psOptions->eGType));
            })
        .help(_("Define the geometry type for the created layer."));

    argParser->add_argument("-s_srs")
        .metavar("<srs_def>")
        .store_into(psOptions->osSourceSRSDef)
        .help(_("Set/override source SRS."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-a_srs")
            .metavar("<srs_def>")
            .action(
                [psOptions](const std::string &osOutputSRSDef)
                {
                    psOptions->osOutputSRSDef = osOutputSRSDef;
                    if (EQUAL(psOptions->osOutputSRSDef.c_str(), "NULL") ||
                        EQUAL(psOptions->osOutputSRSDef.c_str(), "NONE"))
                    {
                        psOptions->osOutputSRSDef.clear();
                        psOptions->bNullifyOutputSRS = true;
                    }
                })
            .help(_("Assign an output SRS, but without reprojecting."));

        group.add_argument("-t_srs")
            .metavar("<srs_def>")
            .action(
                [psOptions](const std::string &osOutputSRSDef)
                {
                    psOptions->osOutputSRSDef = osOutputSRSDef;
                    psOptions->bTransform = true;
                })
            .help(_("Reproject/transform to this SRS on output, and assign it "
                    "as output SRS."));
    }

    ///////////////////////////////////////////////////////////////////////
    argParser->add_group("Field related options");

    argParser->add_argument("-addfields")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                psOptions->bAddMissingFields = true;
                psOptions->eAccessMode = ACCESS_APPEND;
            })
        .help(_("Same as append, but add also any new fields."));

    argParser->add_argument("-relaxedFieldNameMatch")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->bExactFieldNameMatch = false; })
        .help(_("Do field name matching between source and existing target "
                "layer in a more relaxed way."));

    argParser->add_argument("-fieldTypeToString")
        .metavar("All|<type1>[,<type2>]...")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->aosFieldTypesToString =
                    CSLTokenizeStringComplex(s.c_str(), " ,", FALSE, FALSE);
                CSLConstList iter = psOptions->aosFieldTypesToString.List();
                while (*iter)
                {
                    if (IsFieldType(*iter))
                    {
                        /* Do nothing */
                    }
                    else if (EQUAL(*iter, "All"))
                    {
                        psOptions->aosFieldTypesToString.Clear();
                        psOptions->aosFieldTypesToString.AddString("All");
                        break;
                    }
                    else
                    {
                        throw std::invalid_argument(CPLSPrintf(
                            "Unhandled type for fieldTypeToString option : %s",
                            *iter));
                    }
                    iter++;
                }
            })
        .help(_("Converts any field of the specified type to a field of type "
                "string in the destination layer."));

    argParser->add_argument("-mapFieldType")
        .metavar("<srctype>|All=<dsttype>[,<srctype2>=<dsttype2>]...")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->aosMapFieldType =
                    CSLTokenizeStringComplex(s.c_str(), " ,", FALSE, FALSE);
                CSLConstList iter = psOptions->aosMapFieldType.List();
                while (*iter)
                {
                    char *pszKey = nullptr;
                    const char *pszValue = CPLParseNameValue(*iter, &pszKey);
                    if (pszKey && pszValue)
                    {
                        if (!((IsFieldType(pszKey) || EQUAL(pszKey, "All")) &&
                              IsFieldType(pszValue)))
                        {
                            CPLFree(pszKey);
                            throw std::invalid_argument(CPLSPrintf(
                                "Invalid value for -mapFieldType : %s", *iter));
                        }
                    }
                    CPLFree(pszKey);
                    iter++;
                }
            })
        .help(_("Converts any field of the specified type to another type."));

    argParser->add_argument("-fieldmap")
        .metavar("<field_1>[,<field_2>]...")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->aosFieldMap =
                    CSLTokenizeStringComplex(s.c_str(), ",", FALSE, FALSE);
            })
        .help(_("Specifies the list of field indexes to be copied from the "
                "source to the destination."));

    argParser->add_argument("-splitlistfields")
        .store_into(psOptions->bSplitListFields)
        .help(_("Split fields of type list type into as many fields of scalar "
                "type as necessary."));

    argParser->add_argument("-maxsubfields")
        .metavar("<n>")
        .scan<'i', int>()
        .action(
            [psOptions](const std::string &s)
            {
                const int nVal = atoi(s.c_str());
                if (nVal > 0)
                {
                    psOptions->nMaxSplitListSubFields = nVal;
                }
            })
        .help(_("To be combined with -splitlistfields to limit the number of "
                "subfields created for each split field."));

    argParser->add_argument("-emptyStrAsNull")
        .store_into(psOptions->bEmptyStrAsNull)
        .help(_("Treat empty string values as null."));

    argParser->add_argument("-forceNullable")
        .store_into(psOptions->bForceNullable)
        .help(_("Do not propagate not-nullable constraints to target layer if "
                "they exist in source layer."));

    argParser->add_argument("-unsetFieldWidth")
        .store_into(psOptions->bUnsetFieldWidth)
        .help(_("Set field width and precision to 0."));

    argParser->add_argument("-unsetDefault")
        .store_into(psOptions->bUnsetDefault)
        .help(_("Do not propagate default field values to target layer if they "
                "exist in source layer."));

    argParser->add_argument("-resolveDomains")
        .store_into(psOptions->bResolveDomains)
        .help(_("Cause any selected field that is linked to a coded field "
                "domain will be accompanied by an additional field."));

    argParser->add_argument("-dateTimeTo")
        .metavar("UTC|UTC(+|-)<HH>|UTC(+|-)<HH>:<MM>")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszFormat = s.c_str();
                if (EQUAL(pszFormat, "UTC"))
                {
                    psOptions->nTZOffsetInSec = 0;
                }
                else if (STARTS_WITH_CI(pszFormat, "UTC") &&
                         (strlen(pszFormat) == strlen("UTC+HH") ||
                          strlen(pszFormat) == strlen("UTC+HH:MM")) &&
                         (pszFormat[3] == '+' || pszFormat[3] == '-'))
                {
                    const int nHour = atoi(pszFormat + strlen("UTC+"));
                    if (nHour < 0 || nHour > 14)
                    {
                        throw std::invalid_argument("Invalid UTC hour offset.");
                    }
                    else if (strlen(pszFormat) == strlen("UTC+HH"))
                    {
                        psOptions->nTZOffsetInSec = nHour * 3600;
                        if (pszFormat[3] == '-')
                            psOptions->nTZOffsetInSec =
                                -psOptions->nTZOffsetInSec;
                    }
                    else  // if( strlen(pszFormat) == strlen("UTC+HH:MM") )
                    {
                        const int nMin = atoi(pszFormat + strlen("UTC+HH:"));
                        if (nMin == 0 || nMin == 15 || nMin == 30 || nMin == 45)
                        {
                            psOptions->nTZOffsetInSec =
                                nHour * 3600 + nMin * 60;
                            if (pszFormat[3] == '-')
                                psOptions->nTZOffsetInSec =
                                    -psOptions->nTZOffsetInSec;
                        }
                    }
                }
                if (psOptions->nTZOffsetInSec == TZ_OFFSET_INVALID)
                {
                    throw std::invalid_argument(
                        "Value of -dateTimeTo should be UTC, UTC(+|-)HH or "
                        "UTC(+|-)HH:MM with HH in [0,14] and MM=00,15,30,45");
                }
            })
        .help(_("Converts date time values from the timezone specified in the "
                "source value to the target timezone."));

    argParser->add_argument("-noNativeData")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->bNativeData = false; })
        .help(_("Disable copying of native data."));

    ///////////////////////////////////////////////////////////////////////
    argParser->add_group("Advanced geometry and SRS related options");

    argParser->add_argument("-dim")
        .metavar("layer_dim|2|XY|3|XYZ|XYM|XYZM")
        .action(
            [psOptions](const std::string &osDim)
            {
                if (EQUAL(osDim.c_str(), "layer_dim"))
                    psOptions->nCoordDim = COORD_DIM_LAYER_DIM;
                else if (EQUAL(osDim.c_str(), "XY") ||
                         EQUAL(osDim.c_str(), "2"))
                    psOptions->nCoordDim = 2;
                else if (EQUAL(osDim.c_str(), "XYZ") ||
                         EQUAL(osDim.c_str(), "3"))
                    psOptions->nCoordDim = 3;
                else if (EQUAL(osDim.c_str(), "XYM"))
                    psOptions->nCoordDim = COORD_DIM_XYM;
                else if (EQUAL(osDim.c_str(), "XYZM"))
                    psOptions->nCoordDim = 4;
                else
                {
                    throw std::invalid_argument(CPLSPrintf(
                        "-dim %s: value not handled.", osDim.c_str()));
                }
            })
        .help(_("Force the coordinate dimension."));

    argParser->add_argument("-s_coord_epoch")
        .metavar("<epoch>")
        .store_into(psOptions->dfSourceCoordinateEpoch)
        .help(_("Assign a coordinate epoch, linked with the source SRS."));

    argParser->add_argument("-a_coord_epoch")
        .metavar("<epoch>")
        .store_into(psOptions->dfOutputCoordinateEpoch)
        .help(_("Assign a coordinate epoch, linked with the output SRS when "
                "-a_srs is used."));

    argParser->add_argument("-t_coord_epoch")
        .metavar("<epoch>")
        .store_into(psOptions->dfOutputCoordinateEpoch)
        .help(_("Assign a coordinate epoch, linked with the output SRS when "
                "-t_srs is used."));

    argParser->add_argument("-ct")
        .metavar("<pipeline_def>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->osCTPipeline = s;
                psOptions->bTransform = true;
            })
        .help(_("Override the default transformation from the source to the "
                "target CRS."));

    argParser->add_argument("-spat_srs")
        .metavar("<srs_def>")
        .store_into(psOptions->osSpatSRSDef)
        .help(_("Override spatial filter SRS."));

    argParser->add_argument("-geomfield")
        .metavar("<name>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->osGeomField = s;
                psOptions->bGeomFieldSet = true;
            })
        .help(_("Name of the geometry field on which the spatial filter "
                "operates on."));

    argParser->add_argument("-segmentize")
        .metavar("<max_dist>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->eGeomOp = GEOMOP_SEGMENTIZE;
                psOptions->dfGeomOpParam = CPLAtofM(s.c_str());
            })
        .help(_("Maximum distance between 2 nodes."));

    argParser->add_argument("-simplify")
        .metavar("<tolerance>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->eGeomOp = GEOMOP_SIMPLIFY_PRESERVE_TOPOLOGY;
                psOptions->dfGeomOpParam = CPLAtofM(s.c_str());
            })
        .help(_("Distance tolerance for simplification."));

    argParser->add_argument("-makevalid")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                if (!OGRGeometryFactory::haveGEOS())
                {
                    throw std::invalid_argument(
                        "-makevalid only supported for builds against GEOS");
                }
                psOptions->bMakeValid = true;
            })
        .help(_("Fix geometries to be valid regarding the rules of the Simple "
                "Features specification."));

    argParser->add_argument("-wrapdateline")
        .store_into(psOptions->bWrapDateline)
        .help(_("Split geometries crossing the dateline meridian."));

    argParser->add_argument("-datelineoffset")
        .metavar("<val_in_degree>")
        .store_into(psOptions->dfDateLineOffset)
        .default_value(psOptions->dfDateLineOffset)
        .help(_("Offset from dateline in degrees."));

    argParser->add_argument("-clipsrc")
        .nargs(nCountClipSrc)
        .metavar("[<xmin> <ymin> <xmax> <ymax>]|<WKT>|<datasource>|spat_extent")
        .help(_("Clip geometries (in source SRS)."));

    argParser->add_argument("-clipsrcsql")
        .metavar("<sql_statement>")
        .store_into(psOptions->osClipSrcSQL)
        .help(_("Select desired geometries from the source clip datasource "
                "using an SQL query."));

    argParser->add_argument("-clipsrclayer")
        .metavar("<layername>")
        .store_into(psOptions->osClipSrcLayer)
        .help(_("Select the named layer from the source clip datasource."));

    argParser->add_argument("-clipsrcwhere")
        .metavar("<expression>")
        .store_into(psOptions->osClipSrcWhere)
        .help(_("Restrict desired geometries from the source clip layer based "
                "on an attribute query."));

    argParser->add_argument("-clipdst")
        .nargs(nCountClipDst)
        .metavar("[<xmin> <ymin> <xmax> <ymax>]|<WKT>|<datasource>")
        .help(_("Clip geometries (in target SRS)."));

    argParser->add_argument("-clipdstsql")
        .metavar("<sql_statement>")
        .store_into(psOptions->osClipDstSQL)
        .help(_("Select desired geometries from the destination clip "
                "datasource using an SQL query."));

    argParser->add_argument("-clipdstlayer")
        .metavar("<layername>")
        .store_into(psOptions->osClipDstLayer)
        .help(
            _("Select the named layer from the destination clip datasource."));

    argParser->add_argument("-clipdstwhere")
        .metavar("<expression>")
        .store_into(psOptions->osClipDstWhere)
        .help(_("Restrict desired geometries from the destination clip layer "
                "based on an attribute query."));

    argParser->add_argument("-explodecollections")
        .store_into(psOptions->bExplodeCollections)
        .help(_("Produce one feature for each geometry in any kind of geometry "
                "collection in the source file."));

    argParser->add_argument("-zfield")
        .metavar("<name>")
        .store_into(psOptions->osZField)
        .help(_("Uses the specified field to fill the Z coordinate of "
                "geometries."));

    argParser->add_argument("-gcp")
        .metavar(
            "<ungeoref_x> <ungeoref_y> <georef_x> <georef_y> [<elevation>]")
        .nargs(4, 5)
        .append()
        .scan<'g', double>()
        .help(_("Add the indicated ground control point."));

    argParser->add_argument("-tps")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->nTransformOrder = -1; })
        .help(_("Force use of thin plate spline transformer based on available "
                "GCPs."));

    argParser->add_argument("-order")
        .metavar("1|2|3")
        .store_into(psOptions->nTransformOrder)
        .help(_("Order of polynomial used for warping."));

    argParser->add_argument("-xyRes")
        .metavar("<val>[ m|mm|deg]")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszVal = s.c_str();

                char *endptr = nullptr;
                psOptions->dfXYRes = CPLStrtodM(pszVal, &endptr);
                if (!endptr)
                {
                    throw std::invalid_argument(
                        "Invalid value for -xyRes. Must be of the form "
                        "{numeric_value}[ ]?[m|mm|deg]?");
                }
                if (*endptr == ' ')
                    ++endptr;
                if (*endptr != 0 && strcmp(endptr, "m") != 0 &&
                    strcmp(endptr, "mm") != 0 && strcmp(endptr, "deg") != 0)
                {
                    throw std::invalid_argument(
                        "Invalid value for -xyRes. Must be of the form "
                        "{numeric_value}[ ]?[m|mm|deg]?");
                }
                psOptions->osXYResUnit = endptr;
            })
        .help(_("Set/override the geometry X/Y coordinate resolution."));

    argParser->add_argument("-zRes")
        .metavar("<val>[ m|mm]")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszVal = s.c_str();

                char *endptr = nullptr;
                psOptions->dfZRes = CPLStrtodM(pszVal, &endptr);
                if (!endptr)
                {
                    throw std::invalid_argument(
                        "Invalid value for -zRes. Must be of the form "
                        "{numeric_value}[ ]?[m|mm]?");
                }
                if (*endptr == ' ')
                    ++endptr;
                if (*endptr != 0 && strcmp(endptr, "m") != 0 &&
                    strcmp(endptr, "mm") != 0 && strcmp(endptr, "deg") != 0)
                {
                    throw std::invalid_argument(
                        "Invalid value for -zRes. Must be of the form "
                        "{numeric_value}[ ]?[m|mm]?");
                }
                psOptions->osZResUnit = endptr;
            })
        .help(_("Set/override the geometry Z coordinate resolution."));

    argParser->add_argument("-mRes")
        .metavar("<val>")
        .store_into(psOptions->dfMRes)
        .help(_("Set/override the geometry M coordinate resolution."));

    argParser->add_argument("-unsetCoordPrecision")
        .store_into(psOptions->bUnsetCoordPrecision)
        .help(_("Prevent the geometry coordinate resolution from being set on "
                "target layer(s)."));

    ///////////////////////////////////////////////////////////////////////
    argParser->add_group("Other options");

    argParser->add_quiet_argument(&psOptions->bQuiet);

    argParser->add_argument("-progress")
        .store_into(psOptions->bDisplayProgress)
        .help(_("Display progress on terminal. Only works if input layers have "
                "the 'fast feature count' capability."));

    argParser->add_input_format_argument(
        psOptionsForBinary ? &psOptionsForBinary->aosAllowInputDrivers
                           : nullptr);

    argParser->add_open_options_argument(
        psOptionsForBinary ? &(psOptionsForBinary->aosOpenOptions) : nullptr);

    argParser->add_argument("-doo")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosDestOpenOptions.AddString(s.c_str()); })
        .help(_("Open option(s) for output dataset."));

    argParser->add_usage_newline();

    argParser->add_argument("-fid")
        .metavar("<FID>")
        .store_into(psOptions->nFIDToFetch)
        .help(_("If provided, only the feature with the specified feature id "
                "will be processed."));

    argParser->add_argument("-preserve_fid")
        .store_into(psOptions->bPreserveFID)
        .help(_("Use the FID of the source features instead of letting the "
                "output driver automatically assign a new one."));

    argParser->add_argument("-unsetFid")
        .store_into(psOptions->bUnsetFid)
        .help(_("Prevent the name of the source FID column and source feature "
                "IDs from being re-used."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-skip", "-skipfailures")
            .flag()
            .action(
                [psOptions](const std::string &)
                {
                    psOptions->bSkipFailures = true;
                    psOptions->nGroupTransactions = 1; /* #2409 */
                })
            .help(_("Continue after a failure, skipping the failed feature."));

        auto &arg = group.add_argument("-gt")
                        .metavar("<n>|unlimited")
                        .action(
                            [psOptions](const std::string &s)
                            {
                                /* If skipfailures is already set we should not
               modify nGroupTransactions = 1  #2409 */
                                if (!psOptions->bSkipFailures)
                                {
                                    if (EQUAL(s.c_str(), "unlimited"))
                                        psOptions->nGroupTransactions = -1;
                                    else
                                        psOptions->nGroupTransactions =
                                            atoi(s.c_str());
                                }
                            })
                        .help(_("Group <n> features per transaction "));

        argParser->add_hidden_alias_for(arg, "tg");
    }

    argParser->add_argument("-limit")
        .metavar("<nb_features>")
        .store_into(psOptions->nLimit)
        .help(_("Limit the number of features per layer."));

    argParser->add_argument("-ds_transaction")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                psOptions->nLayerTransaction = FALSE;
                psOptions->bForceTransaction = true;
            })
        .help(_("Force the use of a dataset level transaction."));

    /* Undocumented. Just a provision. Default behavior should be OK */
    argParser->add_argument("-lyr_transaction")
        .flag()
        .hidden()
        .action([psOptions](const std::string &)
                { psOptions->nLayerTransaction = TRUE; })
        .help(_("Force the use of a layer level transaction."));

    argParser->add_metadata_item_options_argument(
        psOptions->aosMetadataOptions);

    argParser->add_argument("-nomd")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->bCopyMD = false; })
        .help(_("Disable copying of metadata from source dataset and layers "
                "into target dataset and layers."));

    if (psOptionsForBinary)
    {
        argParser->add_argument("dst_dataset_name")
            .metavar("<dst_dataset_name>")
            .store_into(psOptionsForBinary->osDestDataSource)
            .help(_("Output dataset."));

        argParser->add_argument("src_dataset_name")
            .metavar("<src_dataset_name>")
            .store_into(psOptionsForBinary->osDataSource)
            .help(_("Input dataset."));
    }

    argParser->add_argument("layer")
        .remaining()
        .metavar("<layer_name>")
        .help(_("Layer name"));
    return argParser;
}

/************************************************************************/
/*                    GDALVectorTranslateGetParserUsage()               */
/************************************************************************/

std::string GDALVectorTranslateGetParserUsage()
{
    try
    {
        GDALVectorTranslateOptions sOptions;
        GDALVectorTranslateOptionsForBinary sOptionsForBinary;
        auto argParser = GDALVectorTranslateOptionsGetParser(
            &sOptions, &sOptionsForBinary, 1, 1);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                   CHECK_HAS_ENOUGH_ADDITIONAL_ARGS()                 */
/************************************************************************/

#ifndef CheckHasEnoughAdditionalArgs_defined
#define CheckHasEnoughAdditionalArgs_defined

static bool CheckHasEnoughAdditionalArgs(CSLConstList papszArgv, int i,
                                         int nExtraArg, int nArgc)
{
    if (i + nExtraArg >= nArgc)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "%s option requires %d argument%s", papszArgv[i], nExtraArg,
                 nExtraArg == 1 ? "" : "s");
        return false;
    }
    return true;
}
#endif

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg)                            \
    if (!CheckHasEnoughAdditionalArgs(papszArgv, i, nExtraArg, nArgc))         \
    {                                                                          \
        return nullptr;                                                        \
    }

/************************************************************************/
/*                       GDALVectorTranslateOptionsNew()                */
/************************************************************************/

/**
 * allocates a GDALVectorTranslateOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/ogr2ogr.html">ogr2ogr</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALVectorTranslateOptionsForBinaryNew() prior to
 * this function. Will be filled with potentially present filename, open
 * options,...
 * @return pointer to the allocated GDALVectorTranslateOptions struct. Must be
 * freed with GDALVectorTranslateOptionsFree().
 *
 * @since GDAL 2.1
 */
GDALVectorTranslateOptions *GDALVectorTranslateOptionsNew(
    char **papszArgv, GDALVectorTranslateOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALVectorTranslateOptions>();

    /* -------------------------------------------------------------------- */
    /*      Pre-processing for custom syntax that ArgumentParser does not   */
    /*      support.                                                        */
    /* -------------------------------------------------------------------- */

    CPLStringList aosArgv;
    const int nArgc = CSLCount(papszArgv);
    int nCountClipSrc = 0;
    int nCountClipDst = 0;
    for (int i = 0;
         i < nArgc && papszArgv != nullptr && papszArgv[i] != nullptr; i++)
    {
        if (EQUAL(papszArgv[i], "-gcp"))
        {
            // repeated argument of varying size: not handled by argparse.

            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            char *endptr = nullptr;
            /* -gcp pixel line easting northing [elev] */

            psOptions->oGCPs.nGCPCount++;
            psOptions->oGCPs.pasGCPs = static_cast<GDAL_GCP *>(
                CPLRealloc(psOptions->oGCPs.pasGCPs,
                           sizeof(GDAL_GCP) * psOptions->oGCPs.nGCPCount));
            GDALInitGCPs(1, psOptions->oGCPs.pasGCPs +
                                psOptions->oGCPs.nGCPCount - 1);

            psOptions->oGCPs.pasGCPs[psOptions->oGCPs.nGCPCount - 1]
                .dfGCPPixel = CPLAtof(papszArgv[++i]);
            psOptions->oGCPs.pasGCPs[psOptions->oGCPs.nGCPCount - 1].dfGCPLine =
                CPLAtof(papszArgv[++i]);
            psOptions->oGCPs.pasGCPs[psOptions->oGCPs.nGCPCount - 1].dfGCPX =
                CPLAtof(papszArgv[++i]);
            psOptions->oGCPs.pasGCPs[psOptions->oGCPs.nGCPCount - 1].dfGCPY =
                CPLAtof(papszArgv[++i]);
            if (papszArgv[i + 1] != nullptr &&
                (CPLStrtod(papszArgv[i + 1], &endptr) != 0.0 ||
                 papszArgv[i + 1][0] == '0'))
            {
                /* Check that last argument is really a number and not a
                 * filename */
                /* looking like a number (see ticket #863) */
                if (endptr && *endptr == 0)
                    psOptions->oGCPs.pasGCPs[psOptions->oGCPs.nGCPCount - 1]
                        .dfGCPZ = CPLAtof(papszArgv[++i]);
            }

            /* should set id and info? */
        }

        else if (EQUAL(papszArgv[i], "-clipsrc"))
        {
            if (nCountClipSrc)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Duplicate argument %s",
                         papszArgv[i]);
                return nullptr;
            }
            // argparse doesn't handle well variable number of values
            // just before the positional arguments, so we have to detect
            // it manually and set the correct number.
            nCountClipSrc = 1;
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            if (CPLGetValueType(papszArgv[i + 1]) != CPL_VALUE_STRING &&
                i + 4 < nArgc)
            {
                nCountClipSrc = 4;
            }

            for (int j = 0; j < 1 + nCountClipSrc; ++j)
            {
                aosArgv.AddString(papszArgv[i]);
                ++i;
            }
            --i;
        }

        else if (EQUAL(papszArgv[i], "-clipdst"))
        {
            if (nCountClipDst)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Duplicate argument %s",
                         papszArgv[i]);
                return nullptr;
            }
            // argparse doesn't handle well variable number of values
            // just before the positional arguments, so we have to detect
            // it manually and set the correct number.
            nCountClipDst = 1;
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            if (CPLGetValueType(papszArgv[i + 1]) != CPL_VALUE_STRING &&
                i + 4 < nArgc)
            {
                nCountClipDst = 4;
            }

            for (int j = 0; j < 1 + nCountClipDst; ++j)
            {
                aosArgv.AddString(papszArgv[i]);
                ++i;
            }
            --i;
        }

        else
        {
            aosArgv.AddString(papszArgv[i]);
        }
    }

    try
    {
        auto argParser = GDALVectorTranslateOptionsGetParser(
            psOptions.get(), psOptionsForBinary, nCountClipSrc, nCountClipDst);

        // Collect non-positional arguments for VectorTranslateFrom() case
        psOptions->aosArguments =
            argParser->get_non_positional_arguments(aosArgv);

        argParser->parse_args_without_binary_name(aosArgv.List());

        if (psOptionsForBinary)
            psOptionsForBinary->bQuiet = psOptions->bQuiet;

        if (auto oSpat = argParser->present<std::vector<double>>("-spat"))
        {
            OGRLinearRing oRing;
            const double dfMinX = (*oSpat)[0];
            const double dfMinY = (*oSpat)[1];
            const double dfMaxX = (*oSpat)[2];
            const double dfMaxY = (*oSpat)[3];

            oRing.addPoint(dfMinX, dfMinY);
            oRing.addPoint(dfMinX, dfMaxY);
            oRing.addPoint(dfMaxX, dfMaxY);
            oRing.addPoint(dfMaxX, dfMinY);
            oRing.addPoint(dfMinX, dfMinY);

            auto poSpatialFilter = std::make_shared<OGRPolygon>();
            poSpatialFilter->addRing(&oRing);
            psOptions->poSpatialFilter = poSpatialFilter;
        }

        if (auto oClipSrc =
                argParser->present<std::vector<std::string>>("-clipsrc"))
        {
            const std::string &osVal = (*oClipSrc)[0];

            psOptions->poClipSrc.reset();
            psOptions->osClipSrcDS.clear();

            VSIStatBufL sStat;
            psOptions->bClipSrc = true;
            if (oClipSrc->size() == 4)
            {
                const double dfMinX = CPLAtofM((*oClipSrc)[0].c_str());
                const double dfMinY = CPLAtofM((*oClipSrc)[1].c_str());
                const double dfMaxX = CPLAtofM((*oClipSrc)[2].c_str());
                const double dfMaxY = CPLAtofM((*oClipSrc)[3].c_str());

                OGRLinearRing oRing;

                oRing.addPoint(dfMinX, dfMinY);
                oRing.addPoint(dfMinX, dfMaxY);
                oRing.addPoint(dfMaxX, dfMaxY);
                oRing.addPoint(dfMaxX, dfMinY);
                oRing.addPoint(dfMinX, dfMinY);

                auto poPoly = std::make_shared<OGRPolygon>();
                psOptions->poClipSrc = poPoly;
                poPoly->addRing(&oRing);
            }
            else if ((STARTS_WITH_CI(osVal.c_str(), "POLYGON") ||
                      STARTS_WITH_CI(osVal.c_str(), "MULTIPOLYGON")) &&
                     VSIStatL(osVal.c_str(), &sStat) != 0)
            {
                OGRGeometry *poGeom = nullptr;
                OGRGeometryFactory::createFromWkt(osVal.c_str(), nullptr,
                                                  &poGeom);
                psOptions->poClipSrc.reset(poGeom);
                if (psOptions->poClipSrc == nullptr)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or "
                             "MULTIPOLYGON WKT");
                    return nullptr;
                }
            }
            else if (EQUAL(osVal.c_str(), "spat_extent"))
            {
                // Nothing to do
            }
            else
            {
                psOptions->osClipSrcDS = osVal;
            }
        }

        if (auto oClipDst =
                argParser->present<std::vector<std::string>>("-clipdst"))
        {
            const std::string &osVal = (*oClipDst)[0];

            psOptions->poClipDst.reset();
            psOptions->osClipDstDS.clear();

            VSIStatBufL sStat;
            if (oClipDst->size() == 4)
            {
                const double dfMinX = CPLAtofM((*oClipDst)[0].c_str());
                const double dfMinY = CPLAtofM((*oClipDst)[1].c_str());
                const double dfMaxX = CPLAtofM((*oClipDst)[2].c_str());
                const double dfMaxY = CPLAtofM((*oClipDst)[3].c_str());

                OGRLinearRing oRing;

                oRing.addPoint(dfMinX, dfMinY);
                oRing.addPoint(dfMinX, dfMaxY);
                oRing.addPoint(dfMaxX, dfMaxY);
                oRing.addPoint(dfMaxX, dfMinY);
                oRing.addPoint(dfMinX, dfMinY);

                auto poPoly = std::make_shared<OGRPolygon>();
                psOptions->poClipDst = poPoly;
                poPoly->addRing(&oRing);
            }
            else if ((STARTS_WITH_CI(osVal.c_str(), "POLYGON") ||
                      STARTS_WITH_CI(osVal.c_str(), "MULTIPOLYGON")) &&
                     VSIStatL(osVal.c_str(), &sStat) != 0)
            {
                OGRGeometry *poGeom = nullptr;
                OGRGeometryFactory::createFromWkt(osVal.c_str(), nullptr,
                                                  &poGeom);
                psOptions->poClipDst.reset(poGeom);
                if (psOptions->poClipDst == nullptr)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid geometry. Must be a valid POLYGON or "
                             "MULTIPOLYGON WKT");
                    return nullptr;
                }
            }
            else
            {
                psOptions->osClipDstDS = osVal;
            }
        }

        auto layers = argParser->present<std::vector<std::string>>("layer");
        if (layers)
        {
            for (const auto &layer : *layers)
            {
                psOptions->aosLayers.AddString(layer.c_str());
            }
        }
        if (psOptionsForBinary)
        {
            psOptionsForBinary->eAccessMode = psOptions->eAccessMode;
            psOptionsForBinary->osFormat = psOptions->osFormat;

            if (!(CPLTestBool(
                    psOptionsForBinary->aosOpenOptions.FetchNameValueDef(
                        "NATIVE_DATA",
                        psOptionsForBinary->aosOpenOptions.FetchNameValueDef(
                            "@NATIVE_DATA", "TRUE")))))
            {
                psOptions->bNativeData = false;
            }

            if (psOptions->bNativeData &&
                psOptionsForBinary->aosOpenOptions.FetchNameValue(
                    "NATIVE_DATA") == nullptr &&
                psOptionsForBinary->aosOpenOptions.FetchNameValue(
                    "@NATIVE_DATA") == nullptr)
            {
                psOptionsForBinary->aosOpenOptions.AddString(
                    "@NATIVE_DATA=YES");
            }
        }

        return psOptions.release();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", err.what());
        return nullptr;
    }
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

void GDALVectorTranslateOptionsFree(GDALVectorTranslateOptions *psOptions)
{
    delete psOptions;
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

void GDALVectorTranslateOptionsSetProgress(
    GDALVectorTranslateOptions *psOptions, GDALProgressFunc pfnProgress,
    void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
    if (pfnProgress == GDALTermProgress)
        psOptions->bQuiet = false;
}

#undef CHECK_HAS_ENOUGH_ADDITIONAL_ARGS
