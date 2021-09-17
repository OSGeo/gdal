/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSpatialReference class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2018, Even Rouault <even.rouault at spatialys.com>
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
#include "ogr_spatialref.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <mutex>
#include <vector>

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_error_internal.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_proj_p.h"
#include "ogr_srs_api.h"

#include "proj.h"
#include "proj_experimental.h"
#include "proj_constants.h"

// Exists since 8.0.1
#ifndef PROJ_AT_LEAST_VERSION
#define PROJ_COMPUTE_VERSION(maj,min,patch) ((maj)*10000+(min)*100+(patch))
#define PROJ_VERSION_NUMBER                 \
    PROJ_COMPUTE_VERSION(PROJ_VERSION_MAJOR, PROJ_VERSION_MINOR, PROJ_VERSION_PATCH)
#define PROJ_AT_LEAST_VERSION(maj,min,patch) \
    (PROJ_VERSION_NUMBER >= PROJ_COMPUTE_VERSION(maj,min,patch))
#endif

CPL_CVSID("$Id$")

#define STRINGIFY(s) #s
#define XSTRINGIFY(s) STRINGIFY(s)

struct OGRSpatialReference::Private
{
    struct Listener: public OGR_SRSNode::Listener
    {
        OGRSpatialReference::Private* m_poObj = nullptr;

        explicit Listener(OGRSpatialReference::Private* poObj): m_poObj(poObj) {}
        Listener(const Listener&) = delete;
        Listener& operator=(const Listener&) = delete;

        void notifyChange(OGR_SRSNode*) override
        {
            m_poObj->nodesChanged();
        }
    };

    PJ*             m_pj_crs = nullptr;

    // Temporary state used for object construction
    PJ_TYPE         m_pjType = PJ_TYPE_UNKNOWN;
    CPLString           m_osPrimeMeridianName{};
    CPLString           m_osAngularUnits{};
    CPLString           m_osLinearUnits{};
    CPLString           m_osAxisName[3]{};

    std::vector<std::string> m_wktImportWarnings{};
    std::vector<std::string> m_wktImportErrors{};
    CPLString           m_osAreaName{};

    bool                m_bNodesChanged = false;
    bool                m_bNodesWKT2 = false;
    OGR_SRSNode        *m_poRoot = nullptr;

    double              dfFromGreenwich = 0.0;
    double              dfToMeter = 0.0;
    double              dfToDegrees = 0.0;
    double              m_dfAngularUnitToRadian = 0.0;

    int                 nRefCount = 1;
    int                 bNormInfoSet = FALSE;

    PJ             *m_pj_geod_base_crs_temp = nullptr;
    PJ             *m_pj_proj_crs_cs_temp = nullptr;

    bool                m_pj_crs_modified_during_demote = false;
    PJ             *m_pj_bound_crs_target = nullptr;
    PJ             *m_pj_bound_crs_co = nullptr;
    PJ             *m_pj_crs_backup = nullptr;
    OGR_SRSNode        *m_poRootBackup = nullptr;

    bool                m_bMorphToESRI = false;
    bool                m_bHasCenterLong = false;

    std::shared_ptr<Listener> m_poListener{};

    std::mutex          m_mutex{};

    OSRAxisMappingStrategy m_axisMappingStrategy = OAMS_AUTHORITY_COMPLIANT;
    std::vector<int>       m_axisMapping{1,2,3};

    double              m_coordinateEpoch = 0; // as decimal year

    Private();
    ~Private();
    Private(const Private&) = delete;
    Private& operator= (const Private&) = delete;

    void                clear();
    void                setPjCRS(PJ* pj_crsIn, bool doRefreshAxisMapping = true);
    void                setRoot(OGR_SRSNode* poRoot);
    void                refreshProjObj();
    void                nodesChanged();
    void                refreshRootFromProjObj();
    void                invalidateNodes();

    void                setMorphToESRI(bool b);

    PJ             *getGeodBaseCRS();
    PJ             *getProjCRSCoordSys();

    const char         *getProjCRSName();
    OGRErr              replaceConversionAndUnref(PJ* conv);

    void                demoteFromBoundCRS();
    void                undoDemoteFromBoundCRS();

    PJ_CONTEXT         *getPROJContext() { return OSRGetProjTLSContext(); }

    const char         *nullifyTargetKeyIfPossible(const char* pszTargetKey);

    void                refreshAxisMapping();
};

OGRSpatialReference::Private::Private():
    m_poListener(std::shared_ptr<Listener>(new Listener(this)))
{
}

OGRSpatialReference::Private::~Private()
{
    // In case we destroy the object not in the thread that created it,
    // we need to reassign the PROJ context. Having the context bundled inside
    // PJ* deeply sucks...
    auto ctxt = getPROJContext();

    proj_assign_context( m_pj_crs, ctxt );
    proj_destroy(m_pj_crs);

    proj_assign_context( m_pj_geod_base_crs_temp, ctxt );
    proj_destroy(m_pj_geod_base_crs_temp);

    proj_assign_context( m_pj_proj_crs_cs_temp, ctxt );
    proj_destroy(m_pj_proj_crs_cs_temp);

    proj_assign_context( m_pj_bound_crs_target, ctxt );
    proj_destroy(m_pj_bound_crs_target);

    proj_assign_context( m_pj_bound_crs_co, ctxt );
    proj_destroy(m_pj_bound_crs_co);

    proj_assign_context( m_pj_crs_backup, ctxt );
    proj_destroy(m_pj_crs_backup);

    delete m_poRootBackup;
    delete m_poRoot;
}

void OGRSpatialReference::Private::clear()
{
    proj_assign_context( m_pj_crs, getPROJContext() );
    proj_destroy(m_pj_crs);
    m_pj_crs = nullptr;

    delete m_poRoot;
    m_poRoot = nullptr;
    m_bNodesChanged = false;

    m_wktImportWarnings.clear();
    m_wktImportErrors.clear();

    m_pj_crs_modified_during_demote = false;
    m_pjType = m_pj_crs ? proj_get_type(m_pj_crs) : PJ_TYPE_UNKNOWN;
    m_osPrimeMeridianName.clear();
    m_osAngularUnits.clear();
    m_osLinearUnits.clear();

    bNormInfoSet = FALSE;
    dfFromGreenwich = 1.0;
    dfToMeter = 1.0;
    dfToDegrees = 1.0;
    m_dfAngularUnitToRadian = 0.0;

    m_bMorphToESRI = false;
    m_bHasCenterLong = false;

    m_coordinateEpoch = 0.0;
}

void OGRSpatialReference::Private::setRoot(OGR_SRSNode* poRoot)
{
    m_poRoot = poRoot;
    if( m_poRoot )
    {
        m_poRoot->RegisterListener(m_poListener);
    }
    nodesChanged();
}

void OGRSpatialReference::Private::setPjCRS(PJ* pj_crsIn,
                                            bool doRefreshAxisMapping)
{
    proj_assign_context( m_pj_crs, getPROJContext() );
    proj_destroy(m_pj_crs);
    m_pj_crs = pj_crsIn;
    if( m_pj_crs )
    {
        m_pjType = proj_get_type(m_pj_crs);
    }
    if( m_pj_crs_backup )
    {
        m_pj_crs_modified_during_demote = true;
    }
    invalidateNodes();
    if( doRefreshAxisMapping )
    {
        refreshAxisMapping();
    }
}


void OGRSpatialReference::Private::refreshProjObj()
{
    if( m_bNodesChanged && m_poRoot )
    {
        char* pszWKT = nullptr;
        m_poRoot->exportToWkt(&pszWKT);
        auto poRootBackup = m_poRoot;
        m_poRoot = nullptr;
        const double dfCoordinateEpochBackup = m_coordinateEpoch;
        clear();
        m_coordinateEpoch = dfCoordinateEpochBackup;
        m_bHasCenterLong = strstr(pszWKT, "CENTER_LONG") != nullptr;

        const char* const options[] = { "STRICT=NO", nullptr };
        PROJ_STRING_LIST warnings = nullptr;
        PROJ_STRING_LIST errors = nullptr;
        setPjCRS(proj_create_from_wkt(
            getPROJContext(), pszWKT, options, &warnings, &errors));
        for( auto iter = warnings; iter && *iter; ++iter ) {
            m_wktImportWarnings.push_back(*iter);
        }
        for( auto iter = errors; iter && *iter; ++iter ) {
            m_wktImportErrors.push_back(*iter);
        }
        proj_string_list_destroy(warnings);
        proj_string_list_destroy(errors);

        CPLFree(pszWKT);

        m_poRoot = poRootBackup;
        m_bNodesChanged = false;
    }
}

void OGRSpatialReference::Private::refreshRootFromProjObj()
{
    CPLAssert( m_poRoot == nullptr );

    if( m_pj_crs )
    {
        CPLStringList aosOptions;
        if( !m_bMorphToESRI )
        {
            aosOptions.SetNameValue("OUTPUT_AXIS", "YES");
            aosOptions.SetNameValue("MULTILINE", "NO");
        }
        aosOptions.SetNameValue("STRICT", "NO");

        const char* pszWKT;
        {
            CPLErrorStateBackuper oErrorStateBackuper;
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            pszWKT = proj_as_wkt(getPROJContext(),
                m_pj_crs, m_bMorphToESRI ? PJ_WKT1_ESRI : PJ_WKT1_GDAL,
                aosOptions.List());
            m_bNodesWKT2 = false;
        }
        if( !m_bMorphToESRI && pszWKT == nullptr )
        {
             pszWKT = proj_as_wkt(getPROJContext(), m_pj_crs, PJ_WKT2_2018,
                                  aosOptions.List());
             m_bNodesWKT2 = true;
        }
        if( pszWKT )
        {
            auto root = new OGR_SRSNode();
            setRoot(root);
            root->importFromWkt(&pszWKT);
            m_bNodesChanged = false;
        }
    }
}

static bool isNorthEastAxisOrder(PJ_CONTEXT* ctx, PJ* cs)
{
    const char* pszName1 = nullptr;
    const char* pszDirection1 = nullptr;
    proj_cs_get_axis_info(
        ctx, cs, 0, &pszName1, nullptr, &pszDirection1,
        nullptr, nullptr, nullptr, nullptr);
    const char* pszName2 = nullptr;
    const char* pszDirection2 = nullptr;
    proj_cs_get_axis_info(
        ctx, cs, 1, &pszName2, nullptr, &pszDirection2,
        nullptr, nullptr, nullptr, nullptr);
    if( pszDirection1 && EQUAL(pszDirection1, "north") &&
        pszDirection2 && EQUAL(pszDirection2, "east") )
    {
        return true;
    }
    if( pszDirection1 && pszDirection2 &&
        ((EQUAL(pszDirection1, "north") && EQUAL(pszDirection2, "north")) ||
         (EQUAL(pszDirection1, "south") && EQUAL(pszDirection2, "south"))) &&
        pszName1 && STARTS_WITH_CI(pszName1, "northing") &&
        pszName2 && STARTS_WITH_CI(pszName2, "easting") )
    {
        return true;
    }
    return false;
}

void OGRSpatialReference::Private::refreshAxisMapping()
{
    if( !m_pj_crs || m_axisMappingStrategy == OAMS_CUSTOM )
        return;

    bool doUndoDemote = false;
    if( m_pj_crs_backup == nullptr )
    {
        doUndoDemote = true;
        demoteFromBoundCRS();
    }
    const auto ctxt = getPROJContext();
    PJ* horizCRS = nullptr;
    int axisCount = 0;
    if( m_pjType == PJ_TYPE_VERTICAL_CRS )
    {
        axisCount = 1;
    }
    else if( m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        horizCRS = proj_crs_get_sub_crs(ctxt, m_pj_crs, 0);
        if( horizCRS && proj_get_type(horizCRS) == PJ_TYPE_BOUND_CRS )
        {
            auto baseCRS = proj_get_source_crs(ctxt, horizCRS);
            if( baseCRS )
            {
                proj_destroy(horizCRS);
                horizCRS = baseCRS;
            }
        }

        auto vertCRS = proj_crs_get_sub_crs(ctxt, m_pj_crs, 1);
        if( vertCRS )
        {
            if( proj_get_type(vertCRS) == PJ_TYPE_BOUND_CRS )
            {
                auto baseCRS = proj_get_source_crs(ctxt, vertCRS);
                if( baseCRS )
                {
                    proj_destroy(vertCRS);
                    vertCRS = baseCRS;
                }
            }

            auto cs = proj_crs_get_coordinate_system(ctxt, vertCRS);
            if( cs )
            {
                axisCount += proj_cs_get_axis_count(ctxt, cs);
                proj_destroy(cs);
            }
            proj_destroy(vertCRS);
        }
    }
    else
    {
        horizCRS = m_pj_crs;
    }

    bool bSwitchForGisFriendlyOrder = false;
    if( horizCRS )
    {
        auto cs = proj_crs_get_coordinate_system(ctxt, horizCRS);
        if( cs )
        {
            int nHorizCSAxisCount = proj_cs_get_axis_count(ctxt, cs);
            axisCount += nHorizCSAxisCount;
            if( nHorizCSAxisCount >= 2 )
            {
                bSwitchForGisFriendlyOrder = isNorthEastAxisOrder(ctxt, cs);
            }
            proj_destroy(cs);
        }
    }
    if( horizCRS != m_pj_crs )
    {
        proj_destroy(horizCRS);
    }
    if( doUndoDemote )
    {
        undoDemoteFromBoundCRS();
    }

    m_axisMapping.resize(axisCount);
    if( m_axisMappingStrategy == OAMS_AUTHORITY_COMPLIANT ||
        !bSwitchForGisFriendlyOrder )
    {
        for( int i = 0; i < axisCount; i++ )
        {
            m_axisMapping[i] = i + 1;
        }
    }
    else
    {
        m_axisMapping[0] = 2;
        m_axisMapping[1] = 1;
        if( axisCount == 3 )
        {
            m_axisMapping[2] = 3;
        }
    }
}

void OGRSpatialReference::Private::nodesChanged()
{
    m_bNodesChanged = true;
}

void OGRSpatialReference::Private::invalidateNodes()
{
    delete m_poRoot;
    m_poRoot = nullptr;
    m_bNodesChanged = false;
}

void OGRSpatialReference::Private::setMorphToESRI(bool b)
{
    invalidateNodes();
    m_bMorphToESRI = b;
}

void OGRSpatialReference::Private::demoteFromBoundCRS()
{
    CPLAssert(m_pj_bound_crs_target == nullptr);
    CPLAssert(m_pj_bound_crs_co == nullptr);
    CPLAssert(m_poRootBackup == nullptr);
    CPLAssert(m_pj_crs_backup == nullptr);

    m_pj_crs_modified_during_demote = false;

    if( m_pjType == PJ_TYPE_BOUND_CRS ) {
        auto baseCRS = proj_get_source_crs(getPROJContext(), m_pj_crs);
        m_pj_bound_crs_target = proj_get_target_crs(getPROJContext(), m_pj_crs);
        m_pj_bound_crs_co = proj_crs_get_coordoperation(
            getPROJContext(), m_pj_crs);

        m_poRootBackup = m_poRoot;
        m_poRoot = nullptr;
        m_pj_crs_backup = m_pj_crs;
        m_pj_crs = baseCRS;
        m_pjType = proj_get_type(m_pj_crs);
    }
}

void OGRSpatialReference::Private::undoDemoteFromBoundCRS()
{
    if( m_pj_bound_crs_target )
    {
        CPLAssert(m_poRoot == nullptr);
        CPLAssert(m_pj_crs);
        if( !m_pj_crs_modified_during_demote )
        {
            proj_destroy(m_pj_crs);
            m_pj_crs = m_pj_crs_backup;
            m_pjType = proj_get_type(m_pj_crs);
            m_poRoot = m_poRootBackup;
        }
        else
        {
            delete m_poRootBackup;
            m_poRootBackup = nullptr;
            proj_destroy(m_pj_crs_backup);
            m_pj_crs_backup = nullptr;
            setPjCRS(proj_crs_create_bound_crs(getPROJContext(),
                                                   m_pj_crs,
                                                   m_pj_bound_crs_target,
                                                   m_pj_bound_crs_co), false);
        }
    }

    m_poRootBackup = nullptr;
    m_pj_crs_backup = nullptr;
    proj_destroy(m_pj_bound_crs_target);
    m_pj_bound_crs_target = nullptr;
    proj_destroy(m_pj_bound_crs_co);
    m_pj_bound_crs_co = nullptr;
    m_pj_crs_modified_during_demote = false;
}

const char* OGRSpatialReference::Private::nullifyTargetKeyIfPossible(const char* pszTargetKey)
{
    if( pszTargetKey )
    {
        demoteFromBoundCRS();
        if( (m_pjType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
             m_pjType == PJ_TYPE_GEOGRAPHIC_3D_CRS) &&
            EQUAL(pszTargetKey, "GEOGCS") )
        {
            pszTargetKey = nullptr;
        }
        else if( m_pjType == PJ_TYPE_GEOCENTRIC_CRS &&
            EQUAL(pszTargetKey, "GEOCCS") )
        {
            pszTargetKey = nullptr;
        }
        else if( m_pjType == PJ_TYPE_PROJECTED_CRS &&
            EQUAL(pszTargetKey, "PROJCS") )
        {
            pszTargetKey = nullptr;
        }
        else if( m_pjType == PJ_TYPE_VERTICAL_CRS &&
                 EQUAL(pszTargetKey, "VERT_CS") )
        {
            pszTargetKey = nullptr;
        }
        undoDemoteFromBoundCRS();
    }
    return pszTargetKey;
}

PJ *OGRSpatialReference::Private::getGeodBaseCRS()
{
    if( m_pjType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
        m_pjType == PJ_TYPE_GEOGRAPHIC_3D_CRS ) {
        return m_pj_crs;
    }

    auto ctxt = getPROJContext();
    if( m_pjType == PJ_TYPE_PROJECTED_CRS ) {
        proj_assign_context(m_pj_geod_base_crs_temp, ctxt);
        proj_destroy(m_pj_geod_base_crs_temp);
        m_pj_geod_base_crs_temp = proj_crs_get_geodetic_crs(
            ctxt, m_pj_crs);
        return m_pj_geod_base_crs_temp;
    }

    proj_assign_context(m_pj_geod_base_crs_temp, ctxt);
    proj_destroy(m_pj_geod_base_crs_temp);
    auto cs = proj_create_ellipsoidal_2D_cs(
        ctxt, PJ_ELLPS2D_LATITUDE_LONGITUDE, nullptr, 0);
    m_pj_geod_base_crs_temp = proj_create_geographic_crs(
        ctxt,
        "WGS 84", "World Geodetic System 1984", "WGS 84", SRS_WGS84_SEMIMAJOR,
        SRS_WGS84_INVFLATTENING, SRS_PM_GREENWICH, 0.0,
        SRS_UA_DEGREE, CPLAtof(SRS_UA_DEGREE_CONV), cs);
    proj_destroy(cs);

    return m_pj_geod_base_crs_temp;
}

PJ *OGRSpatialReference::Private::getProjCRSCoordSys()
{
    auto ctxt = getPROJContext();
    if( m_pjType == PJ_TYPE_PROJECTED_CRS ) {
        proj_assign_context(m_pj_proj_crs_cs_temp, ctxt);
        proj_destroy(m_pj_proj_crs_cs_temp);
        m_pj_proj_crs_cs_temp = proj_crs_get_coordinate_system(
            getPROJContext(), m_pj_crs);
        return m_pj_proj_crs_cs_temp;
    }

    proj_assign_context(m_pj_proj_crs_cs_temp, ctxt);
    proj_destroy(m_pj_proj_crs_cs_temp);
    m_pj_proj_crs_cs_temp =  proj_create_cartesian_2D_cs(
        ctxt, PJ_CART2D_EASTING_NORTHING, nullptr, 0);
    return m_pj_proj_crs_cs_temp;
}

const char *OGRSpatialReference::Private::getProjCRSName()
{
    if( m_pjType == PJ_TYPE_PROJECTED_CRS ) {
        return proj_get_name(m_pj_crs);
    }

    return "unnamed";
}

OGRErr OGRSpatialReference::Private::replaceConversionAndUnref(PJ* conv)
{
    refreshProjObj();

    demoteFromBoundCRS();

    auto projCRS = proj_create_projected_crs(
        getPROJContext(),
        getProjCRSName(), getGeodBaseCRS(), conv, getProjCRSCoordSys());
    proj_destroy(conv);

    setPjCRS(projCRS);

    undoDemoteFromBoundCRS();
    return OGRERR_NONE;
}

/************************************************************************/
/*                           ToPointer()                                */
/************************************************************************/

static inline OGRSpatialReference* ToPointer(OGRSpatialReferenceH hSRS)
{
    return OGRSpatialReference::FromHandle(hSRS);
}

/************************************************************************/
/*                           ToHandle()                                 */
/************************************************************************/

static inline OGRSpatialReferenceH ToHandle(OGRSpatialReference* poSRS)
{
    return OGRSpatialReference::ToHandle(poSRS);
}

/************************************************************************/
/*                           OGRsnPrintDouble()                         */
/************************************************************************/

void OGRsnPrintDouble( char * pszStrBuf, size_t size, double dfValue );

void OGRsnPrintDouble( char * pszStrBuf, size_t size, double dfValue )

{
    CPLsnprintf( pszStrBuf, size, "%.16g", dfValue );

    const size_t nLen = strlen(pszStrBuf);

    // The following hack is intended to truncate some "precision" in cases
    // that appear to be roundoff error.
    if( nLen > 15
        && (strcmp(pszStrBuf+nLen-6, "999999") == 0
            || strcmp(pszStrBuf+nLen-6, "000001") == 0) )
    {
        CPLsnprintf( pszStrBuf, size, "%.15g", dfValue );
    }

    // Force to user periods regardless of locale.
    if( strchr( pszStrBuf, ',' ) != nullptr )
    {
        char * const pszDelim = strchr( pszStrBuf, ',' );
        *pszDelim = '.';
    }
}

/************************************************************************/
/*                        OGRSpatialReference()                         */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * This constructor takes an optional string argument which if passed
 * should be a WKT representation of an SRS.  Passing this is equivalent
 * to not passing it, and then calling importFromWkt() with the WKT string.
 *
 * Note that newly created objects are given a reference count of one.
 *
 * The C function OSRNewSpatialReference() does the same thing as this
 * constructor.
 *
 * @param pszWKT well known text definition to which the object should
 * be initialized, or NULL (the default).
 */

OGRSpatialReference::OGRSpatialReference( const char * pszWKT ) :
    d(new Private())
{
    if( pszWKT != nullptr )
        importFromWkt( pszWKT );
}

/************************************************************************/
/*                       OSRNewSpatialReference()                       */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * This function is the same as OGRSpatialReference::OGRSpatialReference()
 */
OGRSpatialReferenceH CPL_STDCALL OSRNewSpatialReference( const char *pszWKT )

{
    OGRSpatialReference * poSRS = new OGRSpatialReference();

    if( pszWKT != nullptr && strlen(pszWKT) > 0 )
    {
        if( poSRS->importFromWkt( pszWKT ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = nullptr;
        }
    }

    return ToHandle( poSRS );
}

/************************************************************************/
/*                        OGRSpatialReference()                         */
/************************************************************************/

/** Simple copy constructor. See also Clone().
 * @param oOther other spatial reference
 */
OGRSpatialReference::OGRSpatialReference(const OGRSpatialReference &oOther) :
    d(new Private())
{
    *this = oOther;
}

/************************************************************************/
/*                        ~OGRSpatialReference()                        */
/************************************************************************/

/**
 * \brief OGRSpatialReference destructor.
 *
 * The C function OSRDestroySpatialReference() does the same thing as this
 * method. Preferred C++ method : OGRSpatialReference::DestroySpatialReference()
  *
 * @deprecated
 */

OGRSpatialReference::~OGRSpatialReference()

{
}

/************************************************************************/
/*                      DestroySpatialReference()                       */
/************************************************************************/

/**
 * \brief OGRSpatialReference destructor.
 *
 * This static method will destroy a OGRSpatialReference.  It is
 * equivalent to calling delete on the object, but it ensures that the
 * deallocation is properly executed within the OGR libraries heap on
 * platforms where this can matter (win32).
 *
 * This function is the same as OSRDestroySpatialReference()
 *
 * @param poSRS the object to delete
 *
 * @since GDAL 1.7.0
 */

void OGRSpatialReference::DestroySpatialReference(OGRSpatialReference* poSRS)
{
    delete poSRS;
}

/************************************************************************/
/*                     OSRDestroySpatialReference()                     */
/************************************************************************/

/**
 * \brief OGRSpatialReference destructor.
 *
 * This function is the same as OGRSpatialReference::~OGRSpatialReference()
 * and OGRSpatialReference::DestroySpatialReference()
 *
 * @param hSRS the object to delete
 */
void CPL_STDCALL OSRDestroySpatialReference( OGRSpatialReferenceH hSRS )

{
    delete ToPointer(hSRS);
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

/**
 * \brief Wipe current definition.
 *
 * Returns OGRSpatialReference to a state with no definition, as it
 * exists when first created.  It does not affect reference counts.
 */

void OGRSpatialReference::Clear()

{
    d->clear();
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

/** Assignment operator.
 * @param oSource SRS to assign to *this
 * @return *this
 */
OGRSpatialReference &
OGRSpatialReference::operator=(const OGRSpatialReference &oSource)

{
    if( &oSource != this )
    {
        Clear();
#ifdef CPPCHECK
        // Otherwise cppcheck would protest that nRefCount isn't modified
        d->nRefCount = (d->nRefCount + 1) - 1;
#endif

        oSource.d->refreshProjObj();
        if( oSource.d->m_pj_crs )
            d->setPjCRS(proj_clone(
                d->getPROJContext(), oSource.d->m_pj_crs));
        if( oSource.d->m_axisMappingStrategy == OAMS_TRADITIONAL_GIS_ORDER )
            SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        else if ( oSource.d->m_axisMappingStrategy == OAMS_CUSTOM )
            SetDataAxisToSRSAxisMapping( oSource.d->m_axisMapping );

        d->m_coordinateEpoch = oSource.d->m_coordinateEpoch;
    }

    return *this;
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
 * \brief Increments the reference count by one.
 *
 * The reference count is used keep track of the number of OGRGeometry objects
 * referencing this SRS.
 *
 * The method does the same thing as the C function OSRReference().
 *
 * @return the updated reference count.
 */

int OGRSpatialReference::Reference()

{
    return CPLAtomicInc(&d->nRefCount);
}

/************************************************************************/
/*                            OSRReference()                            */
/************************************************************************/

/**
 * \brief Increments the reference count by one.
 *
 * This function is the same as OGRSpatialReference::Reference()
 */
int OSRReference( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRReference", 0 );

    return ToPointer(hSRS)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

/**
 * \brief Decrements the reference count by one.
 *
 * The method does the same thing as the C function OSRDereference().
 *
 * @return the updated reference count.
 */

int OGRSpatialReference::Dereference()

{
    if( d->nRefCount <= 0 )
        CPLDebug( "OSR",
                  "Dereference() called on an object with refcount %d,"
                  "likely already destroyed!",
                  d->nRefCount );
    return CPLAtomicDec(&d->nRefCount);
}

/************************************************************************/
/*                           OSRDereference()                           */
/************************************************************************/

/**
 * \brief Decrements the reference count by one.
 *
 * This function is the same as OGRSpatialReference::Dereference()
 */
int OSRDereference( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRDereference", 0 );

    return ToPointer(hSRS)->Dereference();
}

/************************************************************************/
/*                         GetReferenceCount()                          */
/************************************************************************/

/**
 * \brief Fetch current reference count.
 *
 * @return the current reference count.
 */
int OGRSpatialReference::GetReferenceCount() const
{
    return d->nRefCount;
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

/**
 * \brief Decrements the reference count by one, and destroy if zero.
 *
 * The method does the same thing as the C function OSRRelease().
 */

void OGRSpatialReference::Release()

{
    if( Dereference() <= 0 )
        delete this;
}

/************************************************************************/
/*                             OSRRelease()                             */
/************************************************************************/

/**
 * \brief Decrements the reference count by one, and destroy if zero.
 *
 * This function is the same as OGRSpatialReference::Release()
 */
void OSRRelease( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER0( hSRS, "OSRRelease" );

    ToPointer(hSRS)->Release();
}

OGR_SRSNode *OGRSpatialReference::GetRoot()
{
    if( !d->m_poRoot )
    {
        d->refreshRootFromProjObj();
    }
    return d->m_poRoot;
}

const OGR_SRSNode *OGRSpatialReference::GetRoot() const
{
    if( !d->m_poRoot )
    {
        d->refreshRootFromProjObj();
    }
    return d->m_poRoot;
}

/************************************************************************/
/*                              SetRoot()                               */
/************************************************************************/

/**
 * \brief Set the root SRS node.
 *
 * If the object has an existing tree of OGR_SRSNodes, they are destroyed
 * as part of assigning the new root.  Ownership of the passed OGR_SRSNode is
 * is assumed by the OGRSpatialReference.
 *
 * @param poNewRoot object to assign as root.
 */

void OGRSpatialReference::SetRoot( OGR_SRSNode * poNewRoot )

{
    if( d->m_poRoot != poNewRoot )
    {
        delete d->m_poRoot;
        d->setRoot(poNewRoot);
    }
}

/************************************************************************/
/*                            GetAttrNode()                             */
/************************************************************************/

/**
 * \brief Find named node in tree.
 *
 * This method does a pre-order traversal of the node tree searching for
 * a node with this exact value (case insensitive), and returns it.  Leaf
 * nodes are not considered, under the assumption that they are just
 * attribute value nodes.
 *
 * If a node appears more than once in the tree (such as UNIT for instance),
 * the first encountered will be returned.  Use GetNode() on a subtree to be
 * more specific.
 *
 * @param pszNodePath the name of the node to search for.  May contain multiple
 * components such as "GEOGCS|UNIT".
 *
 * @return a pointer to the node found, or NULL if none.
 */

OGR_SRSNode *OGRSpatialReference::GetAttrNode( const char * pszNodePath )

{
    if( strchr(pszNodePath, '|') == nullptr )
    {
        // Fast path
        OGR_SRSNode *poNode = GetRoot();
        if( poNode )
            poNode = poNode->GetNode( pszNodePath );
        return poNode;
    }

    char **papszPathTokens =
        CSLTokenizeStringComplex(pszNodePath, "|", TRUE, FALSE);

    if( CSLCount( papszPathTokens ) < 1 )
    {
        CSLDestroy(papszPathTokens);
        return nullptr;
    }

    OGR_SRSNode *poNode = GetRoot();
    for( int i = 0; poNode != nullptr && papszPathTokens[i] != nullptr; i++ )
    {
        poNode = poNode->GetNode( papszPathTokens[i] );
    }

    CSLDestroy( papszPathTokens );

    return poNode;
}

/**
 * \brief Find named node in tree.
 *
 * This method does a pre-order traversal of the node tree searching for
 * a node with this exact value (case insensitive), and returns it.  Leaf
 * nodes are not considered, under the assumption that they are just
 * attribute value nodes.
 *
 * If a node appears more than once in the tree (such as UNIT for instance),
 * the first encountered will be returned.  Use GetNode() on a subtree to be
 * more specific.
 *
 * @param pszNodePath the name of the node to search for.  May contain multiple
 * components such as "GEOGCS|UNIT".
 *
 * @return a pointer to the node found, or NULL if none.
 */

const OGR_SRSNode *
OGRSpatialReference::GetAttrNode( const char * pszNodePath ) const

{
    OGR_SRSNode *poNode =
        const_cast<OGRSpatialReference *>(this)->GetAttrNode(pszNodePath);

    return poNode;
}

/************************************************************************/
/*                            GetAttrValue()                            */
/************************************************************************/

/**
 * \brief Fetch indicated attribute of named node.
 *
 * This method uses GetAttrNode() to find the named node, and then extracts
 * the value of the indicated child.  Thus a call to GetAttrValue("UNIT",1)
 * would return the second child of the UNIT node, which is normally the
 * length of the linear unit in meters.
 *
 * This method does the same thing as the C function OSRGetAttrValue().
 *
 * @param pszNodeName the tree node to look for (case insensitive).
 * @param iAttr the child of the node to fetch (zero based).
 *
 * @return the requested value, or NULL if it fails for any reason.
 */

const char *OGRSpatialReference::GetAttrValue( const char * pszNodeName,
                                               int iAttr ) const

{
    const OGR_SRSNode *poNode = GetAttrNode( pszNodeName );
    if( poNode == nullptr )
    {
        if( d->m_bNodesWKT2 && EQUAL(pszNodeName, "PROJECTION") )
        {
            return GetAttrValue("METHOD", iAttr);
        }
        return nullptr;
    }

    if( iAttr < 0 || iAttr >= poNode->GetChildCount() )
        return nullptr;

    return poNode->GetChild(iAttr)->GetValue();
}

/************************************************************************/
/*                          OSRGetAttrValue()                           */
/************************************************************************/

/**
 * \brief Fetch indicated attribute of named node.
 *
 * This function is the same as OGRSpatialReference::GetAttrValue()
 */
const char * CPL_STDCALL OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                             const char * pszKey, int iChild )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAttrValue", nullptr );

    return ToPointer(hSRS)->
        GetAttrValue( pszKey, iChild );
}


/************************************************************************/
/*                             GetName()                                */
/************************************************************************/

/**
 * \brief Return the CRS name.
 *
 * The returned value is only short lived and should not be used after other
 * calls to methods on this object.
 *
 * @since GDAL 3.0
 */

const char* OGRSpatialReference::GetName() const
{
    d->refreshProjObj();
    if( !d->m_pj_crs )
        return nullptr;
    return proj_get_name(d->m_pj_crs);
}

/************************************************************************/
/*                           OSRGetName()                               */
/************************************************************************/

/**
 * \brief Return the CRS name.
 *
 * The returned value is only short lived and should not be used after other
 * calls to methods on this object.
 *
 * @since GDAL 3.0
 */
const char* OSRGetName( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRGetName", nullptr );

    return ToPointer(hSRS)->GetName();
}


/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \brief Make a duplicate of this OGRSpatialReference.
 *
 * This method is the same as the C function OSRClone().
 *
 * @return a new SRS, which becomes the responsibility of the caller.
 */

OGRSpatialReference *OGRSpatialReference::Clone() const

{
    OGRSpatialReference *poNewRef = new OGRSpatialReference();

    d->refreshProjObj();
    if( d->m_pj_crs != nullptr )
        poNewRef->d->setPjCRS(proj_clone(d->getPROJContext(), d->m_pj_crs));
    if( d->m_bHasCenterLong && d->m_poRoot )
    {
        poNewRef->d->setRoot(d->m_poRoot->Clone());
    }
    poNewRef->d->m_axisMapping = d->m_axisMapping;
    poNewRef->d->m_axisMappingStrategy = d->m_axisMappingStrategy;
    poNewRef->d->m_coordinateEpoch = d->m_coordinateEpoch;
    return poNewRef;
}

/************************************************************************/
/*                              OSRClone()                              */
/************************************************************************/

/**
 * \brief Make a duplicate of this OGRSpatialReference.
 *
 * This function is the same as OGRSpatialReference::Clone()
 */
OGRSpatialReferenceH CPL_STDCALL OSRClone( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRClone", nullptr );

    return ToHandle(
        ToPointer(hSRS)->Clone() );
}

/************************************************************************/
/*                            dumpReadable()                            */
/************************************************************************/

/** Dump pretty wkt to stdout, mostly for debugging.
 */
void OGRSpatialReference::dumpReadable()

{
    char *pszPrettyWkt = nullptr;

    const char* const apszOptions[] =
        { "FORMAT=WKT2", "MULTILINE=YES", nullptr };
    exportToWkt( &pszPrettyWkt, apszOptions );
    printf( "%s\n", pszPrettyWkt );/*ok*/
    CPLFree( pszPrettyWkt );
}

/************************************************************************/
/*                         exportToPrettyWkt()                          */
/************************************************************************/

/**
 * Convert this SRS into a nicely formatted WKT 1 string for display to a person.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT 1 in OGR.
 *
 * Note that the returned WKT string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * The WKT version can be overridden by using the OSR_WKT_FORMAT configuration
 * option. Valid values are the one of the FORMAT option of
 * exportToWkt( char ** ppszResult, const char* const* papszOptions ) const
 *
 * This method is the same as the C function OSRExportToPrettyWkt().
 *
 * @param ppszResult the resulting string is returned in this pointer.
 * @param bSimplify TRUE if the AXIS, AUTHORITY and EXTENSION nodes should be
 *   stripped off.
 *
 * @return OGRERR_NONE if successful.
 */

OGRErr OGRSpatialReference::exportToPrettyWkt( char ** ppszResult,
                                               int bSimplify ) const

{
    CPLStringList aosOptions;
    aosOptions.SetNameValue("MULTILINE", "YES");
    if( bSimplify )
    {
        aosOptions.SetNameValue("FORMAT", "WKT1_SIMPLE");
    }
    return exportToWkt( ppszResult, aosOptions.List() );
}

/************************************************************************/
/*                        OSRExportToPrettyWkt()                        */
/************************************************************************/

/**
 * \brief Convert this SRS into a nicely formatted WKT 1 string for display to a
 * person.
 *
 * The WKT version can be overridden by using the OSR_WKT_FORMAT configuration
 * option. Valid values are the one of the FORMAT option of
 * exportToWkt( char ** ppszResult, const char* const* papszOptions ) const
 *
 * This function is the same as OGRSpatialReference::exportToPrettyWkt().
 */

OGRErr CPL_STDCALL OSRExportToPrettyWkt( OGRSpatialReferenceH hSRS, char ** ppszReturn,
                             int bSimplify)

{
    VALIDATE_POINTER1( hSRS, "OSRExportToPrettyWkt", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return ToPointer(hSRS)->
        exportToPrettyWkt( ppszReturn, bSimplify );
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

/**
 * \brief Convert this SRS into WKT 1 format.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT 1 in OGR.
 *
 * Note that the returned WKT string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * The WKT version can be overridden by using the OSR_WKT_FORMAT configuration
 * option. Valid values are the one of the FORMAT option of
 * exportToWkt( char ** ppszResult, const char* const* papszOptions ) const
 *
 * This method is the same as the C function OSRExportToWkt().
 *
 * @param ppszResult the resulting string is returned in this pointer.
 *
 * @return OGRERR_NONE if successful.
 */

OGRErr OGRSpatialReference::exportToWkt( char ** ppszResult ) const

{
    return exportToWkt( ppszResult, nullptr );
}

/************************************************************************/
/*                GDAL_proj_crs_create_bound_crs_to_WGS84()             */
/************************************************************************/

static PJ* GDAL_proj_crs_create_bound_crs_to_WGS84(PJ_CONTEXT* ctx, PJ* pj,
                                                   bool onlyIfEPSGCode,
                                                   bool canModifyHorizPart)
{
    PJ* ret = nullptr;
    if( proj_get_type(pj) == PJ_TYPE_COMPOUND_CRS )
    {
        auto horizCRS = proj_crs_get_sub_crs(ctx, pj, 0);
        auto vertCRS = proj_crs_get_sub_crs(ctx, pj, 1);
        if( horizCRS && proj_get_type(horizCRS) != PJ_TYPE_BOUND_CRS && vertCRS &&
            (!onlyIfEPSGCode || proj_get_id_auth_name(horizCRS, 0) != nullptr) )
        {
            auto boundHoriz = canModifyHorizPart ?
                proj_crs_create_bound_crs_to_WGS84(ctx, horizCRS, nullptr) :
                proj_clone(ctx, horizCRS);
            auto boundVert = proj_crs_create_bound_crs_to_WGS84(
                ctx, vertCRS, nullptr);
            if( boundHoriz && boundVert )
            {
                ret = proj_create_compound_crs(
                        ctx, proj_get_name(pj),
                        boundHoriz,
                        boundVert);
            }
            proj_destroy(boundHoriz);
            proj_destroy(boundVert);
        }
        proj_destroy(horizCRS);
        proj_destroy(vertCRS);
    }
    else if( proj_get_type(pj) != PJ_TYPE_BOUND_CRS &&
             (!onlyIfEPSGCode || proj_get_id_auth_name(pj, 0) != nullptr) )
    {
        ret = proj_crs_create_bound_crs_to_WGS84(
                ctx, pj, nullptr);
    }
    return ret;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

/**
 * Convert this SRS into a WKT string.
 *
 * Note that the returned WKT string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT 1 in OGR.
 *
 * @param ppszResult the resulting string is returned in this pointer.
 * @param papszOptions NULL terminated list of options, or NULL. Currently
 * supported options are
 * <ul>
 * <li>MULTILINE=YES/NO. Defaults to NO.</li>
 * <li>FORMAT=SFSQL/WKT1_SIMPLE/WKT1/WKT1_GDAL/WKT1_ESRI/WKT2_2015/WKT2_2018/WKT2/DEFAULT.
 *     If SFSQL, a WKT1 string without AXIS, TOWGS84, AUTHORITY or EXTENSION
 *     node is returned.
 *     If WKT1_SIMPLE, a WKT1 string without AXIS, AUTHORITY or EXTENSION
 *     node is returned.
 *     WKT1 is an alias of WKT1_GDAL.
 *     WKT2 will default to the latest revision implemented (currently WKT2_2018)
 *     WKT2_2019 can be used as an alias of WKT2_2018 since GDAL 3.2
 * <li>ALLOW_ELLIPSOIDAL_HEIGHT_AS_VERTICAL_CRS=YES/NO. Default is NO. If set
 * to YES and FORMAT=WKT1_GDAL, a Geographic 3D CRS or a Projected 3D CRS will
 * be exported as a compound CRS whose vertical part represents an ellipsoidal
 * height (for example for use with LAS 1.4 WKT1).
 * Requires PROJ 7.2.1 and GDAL 3.2.1.</li>
 * </li>
 * </ul>
 *
 * Starting with GDAL 3.0.3, if the OSR_ADD_TOWGS84_ON_EXPORT_TO_WKT1 configuration
 * option is set to YES, when exporting to WKT1_GDAL, this method will try
 * to add a TOWGS84[] node, if there's none attached yet to the SRS and if the SRS has a EPSG code.
 * See the AddGuessedTOWGS84() method for how this TOWGS84[] node may be added.
 *
 * @return OGRERR_NONE if successful.
 * @since GDAL 3.0
 */

OGRErr OGRSpatialReference::exportToWkt( char ** ppszResult,
                                         const char* const* papszOptions ) const
{
    // In the past calling this method was thread-safe, even if we never
    // guaranteed it. Now proj_as_wkt() will cache the result internally,
    // so this is no longer thread-safe.
    std::lock_guard<std::mutex> oLock(d->m_mutex);

    d->refreshProjObj();
    if( !d->m_pj_crs )
    {
        *ppszResult = CPLStrdup("");
        return OGRERR_FAILURE;
    }

    if( d->m_bHasCenterLong && d->m_poRoot && !d->m_bMorphToESRI )
    {
        return d->m_poRoot->exportToWkt(ppszResult);
    }

    auto ctxt = d->getPROJContext();
    auto wktFormat = PJ_WKT1_GDAL;
    const char* pszFormat = CSLFetchNameValueDef(papszOptions, "FORMAT",
                                    CPLGetConfigOption("OSR_WKT_FORMAT", "DEFAULT"));
    if( EQUAL(pszFormat, "DEFAULT") )
        pszFormat = "";

    if( EQUAL(pszFormat, "WKT1_ESRI" ) || d->m_bMorphToESRI )
    {
        wktFormat = PJ_WKT1_ESRI;
    }
    else if( EQUAL(pszFormat, "WKT1") ||
             EQUAL(pszFormat, "WKT1_GDAL") ||
             EQUAL(pszFormat, "WKT1_SIMPLE") ||
             EQUAL(pszFormat, "SFSQL") )
    {
        wktFormat = PJ_WKT1_GDAL;
    }
    else if( EQUAL(pszFormat, "WKT2_2015" ) )
    {
        wktFormat = PJ_WKT2_2015;
    }
    else if( EQUAL(pszFormat, "WKT2" ) ||
             EQUAL(pszFormat, "WKT2_2018" ) ||
             EQUAL(pszFormat, "WKT2_2019" ) )
    {
        wktFormat = PJ_WKT2_2018;
    }
    else if( pszFormat[0] == '\0' )
    {
        if( IsDerivedGeographic() )
        {
            wktFormat = PJ_WKT2_2018;
        }
        else if( (IsGeographic() || IsProjected()) &&
            !IsCompound() && GetAxesCount() == 3 )
        {
            wktFormat = PJ_WKT2_2018;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported value for FORMAT");
        *ppszResult = CPLStrdup("");
        return OGRERR_FAILURE;
    }

    CPLStringList aosOptions;
    if( wktFormat != PJ_WKT1_ESRI )
    {
        aosOptions.SetNameValue("OUTPUT_AXIS", "YES" );
    }
    aosOptions.SetNameValue("MULTILINE",
                    CSLFetchNameValueDef(papszOptions, "MULTILINE", "NO"));

    const char* pszAllowEllpsHeightAsVertCS =
        CSLFetchNameValue(papszOptions, "ALLOW_ELLIPSOIDAL_HEIGHT_AS_VERTICAL_CRS");
    if( pszAllowEllpsHeightAsVertCS )
    {
        aosOptions.SetNameValue("ALLOW_ELLIPSOIDAL_HEIGHT_AS_VERTICAL_CRS",
                                pszAllowEllpsHeightAsVertCS);
    }

    PJ* boundCRS = nullptr;
    if( wktFormat == PJ_WKT1_GDAL &&
        CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "ADD_TOWGS84_ON_EXPORT_TO_WKT1",
                                    CPLGetConfigOption("OSR_ADD_TOWGS84_ON_EXPORT_TO_WKT1", "NO"))) )
    {
        boundCRS = GDAL_proj_crs_create_bound_crs_to_WGS84(
            d->getPROJContext(), d->m_pj_crs, true, true);
    }

    std::vector<CPLErrorHandlerAccumulatorStruct> aoErrors;
    CPLInstallErrorHandlerAccumulator(aoErrors);
    const char* pszWKT = proj_as_wkt(
        ctxt, boundCRS ? boundCRS : d->m_pj_crs,
        wktFormat, aosOptions.List());
    CPLUninstallErrorHandlerAccumulator();
    for( const auto& oError: aoErrors )
    {
        if( pszFormat[0] == '\0' &&
            oError.msg.find("Unsupported conversion method") != std::string::npos )
        {
            CPLErrorReset();
            // If we cannot export in the default mode (WKT1), retry with WKT2
            pszWKT = proj_as_wkt(
                ctxt, boundCRS ? boundCRS : d->m_pj_crs,
                PJ_WKT2_2018, aosOptions.List());
            break;
        }
        CPLError( oError.type, oError.no, "%s", oError.msg.c_str() );
    }

    if( !pszWKT )
    {
        *ppszResult = CPLStrdup("");
        proj_destroy(boundCRS);
        return OGRERR_FAILURE;
    }

    if( EQUAL(pszFormat, "SFSQL" ) || EQUAL(pszFormat, "WKT1_SIMPLE") )
    {
        OGR_SRSNode oRoot;
        oRoot.importFromWkt(&pszWKT);
        oRoot.StripNodes( "AXIS" );
        if( EQUAL(pszFormat, "SFSQL" ) )
        {
            oRoot.StripNodes( "TOWGS84" );
        }
        oRoot.StripNodes( "AUTHORITY" );
        oRoot.StripNodes( "EXTENSION" );
        OGRErr eErr;
        if( CPLTestBool(CSLFetchNameValueDef(papszOptions, "MULTILINE", "NO")) )
            eErr = oRoot.exportToPrettyWkt( ppszResult, 1 );
        else
            eErr = oRoot.exportToWkt( ppszResult );
        proj_destroy(boundCRS);
        return eErr;
    }

    *ppszResult = CPLStrdup( pszWKT );
    proj_destroy(boundCRS);
    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRExportToWkt()                           */
/************************************************************************/

/**
 * \brief Convert this SRS into WKT 1 format.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT in OGR.
 *
 * The WKT version can be overridden by using the OSR_WKT_FORMAT configuration
 * option. Valid values are the one of the FORMAT option of
 * exportToWkt( char ** ppszResult, const char* const* papszOptions ) const
 *
 * This function is the same as OGRSpatialReference::exportToWkt().
 */

OGRErr CPL_STDCALL OSRExportToWkt( OGRSpatialReferenceH hSRS,
                                   char ** ppszReturn )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToWkt", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return ToPointer(hSRS)->exportToWkt( ppszReturn );
}

/************************************************************************/
/*                          OSRExportToWktEx()                          */
/************************************************************************/

/**
 * \brief Convert this SRS into WKT format.
 *
 * This function is the same as OGRSpatialReference::exportToWkt(char ** ppszResult,const char* const* papszOptions ) const
 *
 * @since GDAL 3.0
 */

OGRErr OSRExportToWktEx( OGRSpatialReferenceH hSRS,
                         char ** ppszReturn,
                         const char* const* papszOptions )
{
    VALIDATE_POINTER1( hSRS, "OSRExportToWktEx", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return ToPointer(hSRS)->exportToWkt( ppszReturn, papszOptions );
}


/************************************************************************/
/*                       exportToPROJJSON()                             */
/************************************************************************/

/**
 * Convert this SRS into a PROJJSON string.
 *
 * Note that the returned JSON string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * @param ppszResult the resulting string is returned in this pointer.
 * @param papszOptions NULL terminated list of options, or NULL. Currently
 * supported options are
 * <ul>
 * <li>MULTILINE=YES/NO. Defaults to YES</li>
 * <li>INDENTATION_WIDTH=number. Defaults to 2 (when multiline output is
 * on).</li>
 * <li>SCHEMA=string. URL to PROJJSON schema. Can be set to empty string to
 * disable it.</li>
 * </ul>
 *
 * @return OGRERR_NONE if successful.
 * @since GDAL 3.1 and PROJ 6.2
 */

OGRErr OGRSpatialReference::exportToPROJJSON( char ** ppszResult,
                                              CPL_UNUSED const char* const* papszOptions ) const
{
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 2
    d->refreshProjObj();
    if( !d->m_pj_crs )
    {
        *ppszResult = nullptr;
        return OGRERR_FAILURE;
    }

    const char* pszPROJJSON = proj_as_projjson(
        d->getPROJContext(), d->m_pj_crs, papszOptions);

    if( !pszPROJJSON )
    {
        *ppszResult = CPLStrdup("");
        return OGRERR_FAILURE;
    }

    *ppszResult = CPLStrdup(pszPROJJSON);
    return OGRERR_NONE;
#else
    CPLError(CE_Failure, CPLE_NotSupported,
             "exportToPROJJSON() requires PROJ 6.2 or later");
    *ppszResult = nullptr;
    return OGRERR_UNSUPPORTED_OPERATION;
#endif
}

/************************************************************************/
/*                          OSRExportToPROJJSON()                       */
/************************************************************************/

/**
 * \brief Convert this SRS into PROJJSON format.
 *
 * This function is the same as OGRSpatialReference::exportToPROJJSON() const
 *
 * @since GDAL 3.1 and PROJ 6.2
 */

OGRErr OSRExportToPROJJSON( OGRSpatialReferenceH hSRS,
                         char ** ppszReturn,
                         const char* const* papszOptions )
{
    VALIDATE_POINTER1( hSRS, "OSRExportToPROJJSON", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return ToPointer(hSRS)->exportToPROJJSON( ppszReturn, papszOptions );
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

/**
 * \brief Import from WKT string.
 *
 * This method will wipe the existing SRS definition, and
 * reassign it based on the contents of the passed WKT string.  Only as
 * much of the input string as needed to construct this SRS is consumed from
 * the input string, and the input string pointer
 * is then updated to point to the remaining (unused) input.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT in OGR.
 *
 * This method is the same as the C function OSRImportFromWkt().
 *
 * @param ppszInput Pointer to pointer to input.  The pointer is updated to
 * point to remaining unused input text.
 *
 * @return OGRERR_NONE if import succeeds, or OGRERR_CORRUPT_DATA if it
 * fails for any reason.
 * @since GDAL 2.3
 */

OGRErr OGRSpatialReference::importFromWkt( const char ** ppszInput )

{
    if( !ppszInput || !*ppszInput )
        return OGRERR_FAILURE;
    if( strlen(*ppszInput) > 100 * 1000 &&
        CPLTestBool(CPLGetConfigOption("OSR_IMPORT_FROM_WKT_LIMIT", "YES")) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Suspiciously large input for importFromWkt(). Rejecting it. "
                 "You can remove this limitation by definition the "
                 "OSR_IMPORT_FROM_WKT_LIMIT configuration option to NO.");
        return OGRERR_FAILURE;
    }

    Clear();

    bool canCache = false;
    auto tlsCache = OSRGetProjTLSCache();
    std::string osWkt;
    if( **ppszInput )
    {
        osWkt = *ppszInput;
        auto cachedObj = tlsCache->GetPJForWKT(osWkt);
        if( cachedObj )
        {
            d->setPjCRS(cachedObj);
        }
        else
        {
            const char* const options[] = { "STRICT=NO", nullptr };
            PROJ_STRING_LIST warnings = nullptr;
            PROJ_STRING_LIST errors = nullptr;
            d->setPjCRS(proj_create_from_wkt(
                d->getPROJContext(), *ppszInput, options, &warnings, &errors));
            for( auto iter = warnings; iter && *iter; ++iter ) {
                d->m_wktImportWarnings.push_back(*iter);
            }
            for( auto iter = errors; iter && *iter; ++iter ) {
                d->m_wktImportErrors.push_back(*iter);
                if( !d->m_pj_crs )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s", *iter);
                }
            }
            if( warnings == nullptr && errors == nullptr )
            {
                canCache = true;
            }
            proj_string_list_destroy(warnings);
            proj_string_list_destroy(errors);
        }
    }
    if( !d->m_pj_crs )
        return OGRERR_CORRUPT_DATA;

    // Only accept CRS objects
    const auto type = d->m_pjType;
    if( type != PJ_TYPE_GEODETIC_CRS &&
        type != PJ_TYPE_GEOCENTRIC_CRS &&
        type != PJ_TYPE_GEOGRAPHIC_2D_CRS &&
        type != PJ_TYPE_GEOGRAPHIC_3D_CRS &&
        type != PJ_TYPE_VERTICAL_CRS &&
        type != PJ_TYPE_PROJECTED_CRS &&
        type != PJ_TYPE_COMPOUND_CRS &&
        type != PJ_TYPE_TEMPORAL_CRS &&
        type != PJ_TYPE_ENGINEERING_CRS &&
        type != PJ_TYPE_BOUND_CRS &&
        type != PJ_TYPE_OTHER_CRS )
    {
        Clear();
        return OGRERR_CORRUPT_DATA;
    }

    if( canCache )
    {
        tlsCache->CachePJForWKT(osWkt, d->m_pj_crs);
    }

    if( strstr(*ppszInput, "CENTER_LONG") ) {
        auto poRoot = new OGR_SRSNode();
        d->setRoot(poRoot);
        const char* pszTmp = *ppszInput;
        poRoot->importFromWkt(&pszTmp);
        d->m_bHasCenterLong = true;
    }

    // TODO? we don't really update correctly since we assume that the
    // passed string is only WKT.
    *ppszInput += strlen(*ppszInput);
    return OGRERR_NONE;

#if no_longer_implemented_for_now
/* -------------------------------------------------------------------- */
/*      The following seems to try and detect and unconsumed            */
/*      VERTCS[] coordinate system definition (ESRI style) and to       */
/*      import and attach it to the existing root.  Likely we will      */
/*      need to extend this somewhat to bring it into an acceptable     */
/*      OGRSpatialReference organization at some point.                 */
/* -------------------------------------------------------------------- */
    if( strlen(*ppszInput) > 0 && strstr(*ppszInput, "VERTCS") )
    {
        if( ((*ppszInput)[0]) == ',' )
            (*ppszInput)++;
        OGR_SRSNode *poNewChild = new OGR_SRSNode();
        poRoot->AddChild( poNewChild );
        return poNewChild->importFromWkt( ppszInput );
    }
#endif
}

/**
 * \brief Import from WKT string.
 *
 * This method will wipe the existing SRS definition, and
 * reassign it based on the contents of the passed WKT string.  Only as
 * much of the input string as needed to construct this SRS is consumed from
 * the input string, and the input string pointer
 * is then updated to point to the remaining (unused) input.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT in OGR.
 *
 * This method is the same as the C function OSRImportFromWkt().
 *
 * @param ppszInput Pointer to pointer to input.  The pointer is updated to
 * point to remaining unused input text.
 *
 * @return OGRERR_NONE if import succeeds, or OGRERR_CORRUPT_DATA if it
 * fails for any reason.
 * @deprecated GDAL 2.3. Use importFromWkt(const char**) or importFromWkt(const char*)
 */

OGRErr OGRSpatialReference::importFromWkt( char ** ppszInput )

{
    return importFromWkt( const_cast<const char**>(ppszInput) );
}

/**
 * \brief Import from WKT string.
 *
 * This method will wipe the existing SRS definition, and
 * reassign it based on the contents of the passed WKT string.  Only as
 * much of the input string as needed to construct this SRS is consumed from
 * the input string, and the input string pointer
 * is then updated to point to the remaining (unused) input.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT in OGR.
 *
 * @param pszInput Input WKT
 *
 * @return OGRERR_NONE if import succeeds, or OGRERR_CORRUPT_DATA if it
 * fails for any reason.
 * @since GDAL 2.3
 */

OGRErr OGRSpatialReference::importFromWkt( const char* pszInput )
{
    return importFromWkt(&pszInput);
}

/************************************************************************/
/*                              Validate()                              */
/************************************************************************/

/**
 * \brief Validate CRS imported with importFromWkt() or with modified with
 * direct node manipulations. Otherwise the CRS should be always valid.
 *
 * This method attempts to verify that the spatial reference system is
 * well formed, and consists of known tokens.  The validation is not
 * comprehensive.
 *
 * This method is the same as the C function OSRValidate().
 *
 * @return OGRERR_NONE if all is fine, OGRERR_CORRUPT_DATA if the SRS is
 * not well formed, and OGRERR_UNSUPPORTED_SRS if the SRS is well formed,
 * but contains non-standard PROJECTION[] values.
 */

OGRErr OGRSpatialReference::Validate() const

{
    for( const auto& str: d->m_wktImportErrors )
    {
        CPLDebug("OGRSpatialReference::Validate", "%s", str.c_str());
    }
    for( const auto& str: d->m_wktImportWarnings )
    {
        CPLDebug("OGRSpatialReference::Validate", "%s", str.c_str());
    }
    if( !d->m_pj_crs || !d->m_wktImportErrors.empty() )
    {
        return OGRERR_CORRUPT_DATA;
    }
    if( !d->m_wktImportWarnings.empty() )
    {
        return OGRERR_UNSUPPORTED_SRS;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRValidate()                             */
/************************************************************************/
/**
 * \brief Validate SRS tokens.
 *
 * This function is the same as the C++ method OGRSpatialReference::Validate().
 */
OGRErr OSRValidate( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRValidate", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->Validate();
}

/************************************************************************/
/*                          OSRImportFromWkt()                          */
/************************************************************************/

/**
 * \brief Import from WKT string.
 *
 * Consult also the <a href="wktproblems.html">OGC WKT Coordinate System Issues</a> page
 * for implementation details of WKT in OGR.
 *
 * This function is the same as OGRSpatialReference::importFromWkt().
 */

OGRErr OSRImportFromWkt( OGRSpatialReferenceH hSRS, char **ppszInput )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromWkt", OGRERR_FAILURE );

    return ToPointer(hSRS)->importFromWkt(
                const_cast<const char**>(ppszInput) );
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

/**
 * \brief Set attribute value in spatial reference.
 *
 * Missing intermediate nodes in the path will be created if not already
 * in existence.  If the attribute has no children one will be created and
 * assigned the value otherwise the zeroth child will be assigned the value.
 *
 * This method does the same as the C function OSRSetAttrValue().
 *
 * @param pszNodePath full path to attribute to be set.  For instance
 * "PROJCS|GEOGCS|UNIT".
 *
 * @param pszNewNodeValue value to be assigned to node, such as "meter".
 * This may be NULL if you just want to force creation of the intermediate
 * path.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetNode( const char * pszNodePath,
                                     const char * pszNewNodeValue )

{
    char **papszPathTokens =
        CSLTokenizeStringComplex(pszNodePath, "|", TRUE, FALSE);

    if( CSLCount( papszPathTokens ) < 1 )
    {
        CSLDestroy(papszPathTokens);
        return OGRERR_FAILURE;
    }

    if( GetRoot() == nullptr || !EQUAL(papszPathTokens[0], GetRoot()->GetValue()) )
    {
        if( EQUAL(papszPathTokens[0], "PROJCS") && CSLCount( papszPathTokens ) == 1 )
        {
            CSLDestroy(papszPathTokens);
            return SetProjCS(pszNewNodeValue);
        }
        else
        {
            SetRoot( new OGR_SRSNode( papszPathTokens[0] ) );
        }
    }

    OGR_SRSNode *poNode = GetRoot();
    for( int i = 1; papszPathTokens[i] != nullptr; i++ )
    {
        int j = 0;  // Used after for.

        for( ; j < poNode->GetChildCount(); j++ )
        {
            if( EQUAL(poNode->GetChild( j )->GetValue(), papszPathTokens[i]) )
            {
                poNode = poNode->GetChild(j);
                j = -1;
                break;
            }
        }

        if( j != -1 )
        {
            OGR_SRSNode *poNewNode = new OGR_SRSNode( papszPathTokens[i] );
            poNode->AddChild( poNewNode );
            poNode = poNewNode;
        }
    }

    CSLDestroy( papszPathTokens );

    if( pszNewNodeValue != nullptr )
    {
        if( poNode->GetChildCount() > 0 )
            poNode->GetChild(0)->SetValue( pszNewNodeValue );
        else
            poNode->AddChild( new OGR_SRSNode( pszNewNodeValue ) );
    }
;
    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetAttrValue()                           */
/************************************************************************/

/**
 * \brief Set attribute value in spatial reference.
 *
 * This function is the same as OGRSpatialReference::SetNode()
 */
OGRErr CPL_STDCALL OSRSetAttrValue( OGRSpatialReferenceH hSRS,
                        const char * pszPath, const char * pszValue )

{
    VALIDATE_POINTER1( hSRS, "OSRSetAttrValue", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetNode( pszPath, pszValue );
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

/**
 * \brief Set attribute value in spatial reference.
 *
 * Missing intermediate nodes in the path will be created if not already
 * in existence.  If the attribute has no children one will be created and
 * assigned the value otherwise the zeroth child will be assigned the value.
 *
 * This method does the same as the C function OSRSetAttrValue().
 *
 * @param pszNodePath full path to attribute to be set.  For instance
 * "PROJCS|GEOGCS|UNIT".
 *
 * @param dfValue value to be assigned to node.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetNode( const char *pszNodePath,
                                     double dfValue )

{
    char szValue[64] = { '\0' };

    if( std::abs(dfValue - static_cast<int>(dfValue)) == 0.0 )
        snprintf( szValue, sizeof(szValue), "%d", static_cast<int>(dfValue) );
    else
        OGRsnPrintDouble( szValue, sizeof(szValue), dfValue );

    return SetNode( pszNodePath, szValue );
}

/************************************************************************/
/*                          SetAngularUnits()                           */
/************************************************************************/

/**
 * \brief Set the angular units for the geographic coordinate system.
 *
 * This method creates a UNIT subnode with the specified values as a
 * child of the GEOGCS node.
 *
 * This method does the same as the C function OSRSetAngularUnits().
 *
 * @param pszUnitsName the units name to be used.  Some preferred units
 * names can be found in ogr_srs_api.h such as SRS_UA_DEGREE.
 *
 * @param dfInRadians the value to multiple by an angle in the indicated
 * units to transform to radians.  Some standard conversion factors can
 * be found in ogr_srs_api.h.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetAngularUnits( const char * pszUnitsName,
                                             double dfInRadians )

{
    d->bNormInfoSet = FALSE;

    d->refreshProjObj();
    if( !d->m_pj_crs )
        return OGRERR_FAILURE;
    auto geodCRS = proj_crs_get_geodetic_crs(
        d->getPROJContext(), d->m_pj_crs);
    if( !geodCRS )
        return OGRERR_FAILURE;
    proj_destroy(geodCRS);
    d->demoteFromBoundCRS();
    d->setPjCRS(proj_crs_alter_cs_angular_unit(
        d->getPROJContext(), d->m_pj_crs,
        pszUnitsName, dfInRadians, nullptr, nullptr));
    d->undoDemoteFromBoundCRS();

    d->m_osAngularUnits = pszUnitsName;
    d->m_dfAngularUnitToRadian = dfInRadians;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRSetAngularUnits()                         */
/************************************************************************/

/**
 * \brief Set the angular units for the geographic coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetAngularUnits()
 */
OGRErr OSRSetAngularUnits( OGRSpatialReferenceH hSRS,
                           const char * pszUnits, double dfInRadians )

{
    VALIDATE_POINTER1( hSRS, "OSRSetAngularUnits", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetAngularUnits( pszUnits, dfInRadians );
}

/************************************************************************/
/*                          GetAngularUnits()                           */
/************************************************************************/

/**
 * \brief Fetch angular geographic coordinate system units.
 *
 * If no units are available, a value of "degree" and SRS_UA_DEGREE_CONV
 * will be assumed.  This method only checks directly under the GEOGCS node
 * for units.
 *
 * This method does the same thing as the C function OSRGetAngularUnits().
 *
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should
 * not be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call.
 *
 * @return the value to multiply by angular distances to transform them to
 * radians.
 * @deprecated GDAL 2.3.0. Use GetAngularUnits(const char**) const.
 */

double OGRSpatialReference::GetAngularUnits( const char ** ppszName ) const

{
    d->refreshProjObj();

    if( !d->m_osAngularUnits.empty() )
    {
        if( ppszName != nullptr )
            *ppszName = d->m_osAngularUnits.c_str();
        return d->m_dfAngularUnitToRadian;
    }

    do
    {
        if( d->m_pj_crs == nullptr ||
            d->m_pjType == PJ_TYPE_ENGINEERING_CRS )
        {
            break;
        }

        auto geodCRS = proj_crs_get_geodetic_crs(
            d->getPROJContext(), d->m_pj_crs);
        if( !geodCRS )
        {
            break;
        }
        auto coordSys = proj_crs_get_coordinate_system(
            d->getPROJContext(), geodCRS);
        proj_destroy(geodCRS);
        if( !coordSys )
        {
            break;
        }
        if( proj_cs_get_type(
                d->getPROJContext(), coordSys) != PJ_CS_TYPE_ELLIPSOIDAL )
        {
            proj_destroy(coordSys);
            break;
        }

        double dfConvFactor = 0.0;
        const char* pszUnitName = nullptr;
        if( !proj_cs_get_axis_info(
            d->getPROJContext(), coordSys, 0, nullptr, nullptr, nullptr,
            &dfConvFactor, &pszUnitName, nullptr, nullptr) )
        {
            proj_destroy(coordSys);
            break;
        }

        d->m_osAngularUnits = pszUnitName;

        proj_destroy(coordSys);
        d->m_dfAngularUnitToRadian = dfConvFactor;
    }
    while(false);

    if( d->m_osAngularUnits.empty() )
    {
        d->m_osAngularUnits = "degree";
        d->m_dfAngularUnitToRadian = CPLAtof(SRS_UA_DEGREE_CONV);
    }

    if( ppszName != nullptr )
        *ppszName = d->m_osAngularUnits.c_str();
    return d->m_dfAngularUnitToRadian;
}

/**
 * \brief Fetch angular geographic coordinate system units.
 *
 * If no units are available, a value of "degree" and SRS_UA_DEGREE_CONV
 * will be assumed.  This method only checks directly under the GEOGCS node
 * for units.
 *
 * This method does the same thing as the C function OSRGetAngularUnits().
 *
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should
 * not be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call.
 *
 * @return the value to multiply by angular distances to transform them to
 * radians.
 * @since GDAL 2.3.0
 */

double OGRSpatialReference::GetAngularUnits( char ** ppszName ) const

{
    return GetAngularUnits( const_cast<const char**>(ppszName) );
}

/************************************************************************/
/*                         OSRGetAngularUnits()                         */
/************************************************************************/

/**
 * \brief Fetch angular geographic coordinate system units.
 *
 * This function is the same as OGRSpatialReference::GetAngularUnits()
 */
double OSRGetAngularUnits( OGRSpatialReferenceH hSRS, char ** ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAngularUnits", 0 );

    return ToPointer(hSRS)->
        GetAngularUnits( const_cast<const char**>(ppszName) );
}

/************************************************************************/
/*                 SetLinearUnitsAndUpdateParameters()                  */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This method creates a UNIT subnode with the specified values as a
 * child of the PROJCS or LOCAL_CS node.   It works the same as the
 * SetLinearUnits() method, but it also updates all existing linear
 * projection parameter values from the old units to the new units.
 *
 * @param pszName the units name to be used.  Some preferred units
 * names can be found in ogr_srs_api.h such as SRS_UL_METER, SRS_UL_FOOT
 * and SRS_UL_US_FOOT.
 *
 * @param dfInMeters the value to multiple by a length in the indicated
 * units to transform to meters.  Some standard conversion factors can
 * be found in ogr_srs_api.h.
 *
 * @param pszUnitAuthority Unit authority name. Or nullptr
 *
 * @param pszUnitCode Unit code. Or nullptr
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetLinearUnitsAndUpdateParameters(
    const char *pszName, double dfInMeters, const char *pszUnitAuthority,
    const char *pszUnitCode )

{
    if( dfInMeters <= 0.0 )
        return OGRERR_FAILURE;

    d->refreshProjObj();
    if( !d->m_pj_crs )
        return OGRERR_FAILURE;

    d->demoteFromBoundCRS();
    if( d->m_pjType == PJ_TYPE_PROJECTED_CRS ) {
        d->setPjCRS(proj_crs_alter_parameters_linear_unit(
            d->getPROJContext(),
            d->m_pj_crs, pszName, dfInMeters,
            pszUnitAuthority, pszUnitCode, true));
    }
    d->setPjCRS(proj_crs_alter_cs_linear_unit(
        d->getPROJContext(), d->m_pj_crs,
        pszName, dfInMeters, pszUnitAuthority, pszUnitCode));
    d->undoDemoteFromBoundCRS();

    d->m_osLinearUnits = pszName;
    d->dfToMeter = dfInMeters;

    return OGRERR_NONE;
}

/************************************************************************/
/*                OSRSetLinearUnitsAndUpdateParameters()                */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This function is the same as
 *   OGRSpatialReference::SetLinearUnitsAndUpdateParameters()
 */
OGRErr OSRSetLinearUnitsAndUpdateParameters( OGRSpatialReferenceH hSRS,
                                             const char * pszUnits,
                                             double dfInMeters )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLinearUnitsAndUpdateParameters",
                       OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetLinearUnitsAndUpdateParameters( pszUnits, dfInMeters );
}

/************************************************************************/
/*                           SetLinearUnits()                           */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This method creates a UNIT subnode with the specified values as a
 * child of the PROJCS, GEOCCS, GEOGCS or LOCAL_CS node. When called on a
 * Geographic 3D CRS the vertical axis units will be set.
 *
 * This method does the same as the C function OSRSetLinearUnits().
 *
 * @param pszUnitsName the units name to be used.  Some preferred units
 * names can be found in ogr_srs_api.h such as SRS_UL_METER, SRS_UL_FOOT
 * and SRS_UL_US_FOOT.
 *
 * @param dfInMeters the value to multiple by a length in the indicated
 * units to transform to meters.  Some standard conversion factors can
 * be found in ogr_srs_api.h.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetLinearUnits( const char * pszUnitsName,
                                            double dfInMeters )

{
    return SetTargetLinearUnits( nullptr, pszUnitsName, dfInMeters );
}

/************************************************************************/
/*                         OSRSetLinearUnits()                          */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This function is the same as OGRSpatialReference::SetLinearUnits()
 */
OGRErr OSRSetLinearUnits( OGRSpatialReferenceH hSRS,
                          const char * pszUnits, double dfInMeters )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLinearUnits", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetLinearUnits( pszUnits, dfInMeters );
}

/************************************************************************/
/*                        SetTargetLinearUnits()                        */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This method creates a UNIT subnode with the specified values as a
 * child of the target node.
 *
 * This method does the same as the C function OSRSetTargetLinearUnits().
 *
 * @param pszTargetKey the keyword to set the linear units for.
 * i.e. "PROJCS" or "VERT_CS"
 *
 * @param pszUnitsName the units name to be used.  Some preferred units
 * names can be found in ogr_srs_api.h such as SRS_UL_METER, SRS_UL_FOOT
 * and SRS_UL_US_FOOT.
 *
 * @param dfInMeters the value to multiple by a length in the indicated
 * units to transform to meters.  Some standard conversion factors can
 * be found in ogr_srs_api.h.
 *
 * @param pszUnitAuthority Unit authority name. Or nullptr
 *
 * @param pszUnitCode Unit code. Or nullptr
 *
 * @return OGRERR_NONE on success.
 *
 * @since OGR 1.9.0
 */

OGRErr OGRSpatialReference::SetTargetLinearUnits( const char *pszTargetKey,
                                                  const char * pszUnitsName,
                                                  double dfInMeters,
                                                  const char *pszUnitAuthority,
                                                  const char *pszUnitCode )

{
    if( dfInMeters <= 0.0 )
        return OGRERR_FAILURE;

    d->refreshProjObj();
    pszTargetKey = d->nullifyTargetKeyIfPossible(pszTargetKey);
    if( pszTargetKey == nullptr )
    {
        if( !d->m_pj_crs )
            return OGRERR_FAILURE;

        d->demoteFromBoundCRS();
        if( d->m_pjType == PJ_TYPE_PROJECTED_CRS ) {
            d->setPjCRS(proj_crs_alter_parameters_linear_unit(
                d->getPROJContext(),
                d->m_pj_crs, pszUnitsName, dfInMeters,
                pszUnitAuthority, pszUnitCode, false));
        }
        d->setPjCRS(proj_crs_alter_cs_linear_unit(
            d->getPROJContext(), d->m_pj_crs,
            pszUnitsName, dfInMeters, pszUnitAuthority, pszUnitCode));
        d->undoDemoteFromBoundCRS();

        d->m_osLinearUnits = pszUnitsName;
        d->dfToMeter = dfInMeters;

        return OGRERR_NONE;
    }

    OGR_SRSNode *poCS = GetAttrNode( pszTargetKey );

    if( poCS == nullptr )
        return OGRERR_FAILURE;

    char szValue[128] = { '\0' };
    if( dfInMeters < std::numeric_limits<int>::max() &&
        dfInMeters > std::numeric_limits<int>::min() &&
        dfInMeters == static_cast<int>(dfInMeters) )
        snprintf( szValue, sizeof(szValue),
                  "%d", static_cast<int>(dfInMeters) );
    else
        OGRsnPrintDouble( szValue, sizeof(szValue), dfInMeters );

    OGR_SRSNode *poUnits = nullptr;
    if( poCS->FindChild( "UNIT" ) >= 0 )
    {
        poUnits = poCS->GetChild( poCS->FindChild( "UNIT" ) );
        if( poUnits->GetChildCount() < 2 )
            return OGRERR_FAILURE;
        poUnits->GetChild(0)->SetValue( pszUnitsName );
        poUnits->GetChild(1)->SetValue( szValue );
        if( poUnits->FindChild( "AUTHORITY" ) != -1 )
            poUnits->DestroyChild( poUnits->FindChild( "AUTHORITY" ) );
    }
    else
    {
        poUnits = new OGR_SRSNode( "UNIT" );
        poUnits->AddChild( new OGR_SRSNode( pszUnitsName ) );
        poUnits->AddChild( new OGR_SRSNode( szValue ) );

        poCS->AddChild( poUnits );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRSetLinearUnits()                          */
/************************************************************************/

/**
 * \brief Set the linear units for the target node.
 *
 * This function is the same as OGRSpatialReference::SetTargetLinearUnits()
 *
 * @since OGR 1.9.0
 */
OGRErr OSRSetTargetLinearUnits( OGRSpatialReferenceH hSRS,
                                const char *pszTargetKey,
                                const char * pszUnits, double dfInMeters )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTargetLinearUnits", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetTargetLinearUnits( pszTargetKey, pszUnits, dfInMeters );
}

/************************************************************************/
/*                           GetLinearUnits()                           */
/************************************************************************/

/**
 * \brief Fetch linear projection units.
 *
 * If no units are available, a value of "Meters" and 1.0 will be assumed.
 * This method only checks directly under the PROJCS, GEOCCS, GEOGCS or
 * LOCAL_CS node for units. When called on a Geographic 3D CRS the vertical
 * axis units will be returned.
 *
 * This method does the same thing as the C function OSRGetLinearUnits()
 *
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should
 * not be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call.
 *
 * @return the value to multiply by linear distances to transform them to
 * meters.
 * @deprecated GDAL 2.3.0. Use GetLinearUnits(const char**) const.
 */

double OGRSpatialReference::GetLinearUnits( char ** ppszName ) const

{
    return GetTargetLinearUnits( nullptr, const_cast<const char**>(ppszName) );
}

/**
 * \brief Fetch linear projection units.
 *
 * If no units are available, a value of "Meters" and 1.0 will be assumed.
 * This method only checks directly under the PROJCS, GEOCCS or LOCAL_CS node
 * for units.
 *
 * This method does the same thing as the C function OSRGetLinearUnits()
 *
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should
 * not be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call.
 *
 * @return the value to multiply by linear distances to transform them to
 * meters.
 * @since GDAL 2.3.0
 */

double OGRSpatialReference::GetLinearUnits( const char ** ppszName ) const

{
    return GetTargetLinearUnits( nullptr, ppszName );
}

/************************************************************************/
/*                         OSRGetLinearUnits()                          */
/************************************************************************/

/**
 * \brief Fetch linear projection units.
 *
 * This function is the same as OGRSpatialReference::GetLinearUnits()
 */
double OSRGetLinearUnits( OGRSpatialReferenceH hSRS, char ** ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetLinearUnits", 0 );

    return ToPointer(hSRS)->GetLinearUnits( const_cast<const char**>(ppszName) );
}

/************************************************************************/
/*                        GetTargetLinearUnits()                        */
/************************************************************************/

/**
 * \brief Fetch linear units for target.
 *
 * If no units are available, a value of "Meters" and 1.0 will be assumed.
 *
 * This method does the same thing as the C function OSRGetTargetLinearUnits()
 *
 * @param pszTargetKey the key to look on. i.e. "PROJCS" or "VERT_CS". Might be
 * NULL, in which case PROJCS will be implied (and if not found, LOCAL_CS,
 * GEOCCS, GEOGCS and VERT_CS are looked up)
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should not
 * be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call. ppszName can be set to NULL.
 *
 * @return the value to multiply by linear distances to transform them to
 * meters.
 *
 * @since OGR 1.9.0
 * @deprecated GDAL 2.3.0. Use GetTargetLinearUnits(const char*, const char**) const.
 */

double OGRSpatialReference::GetTargetLinearUnits( const char *pszTargetKey,
                                                  const char ** ppszName ) const

{
    d->refreshProjObj();

    pszTargetKey = d->nullifyTargetKeyIfPossible(pszTargetKey);
    if( pszTargetKey == nullptr )
    {
        // Use cached result if available
        if( !d->m_osLinearUnits.empty() )
        {
            if( ppszName )
                *ppszName = d->m_osLinearUnits.c_str();
            return d->dfToMeter;
        }

        while( true )
        {
            if( d->m_pj_crs == nullptr )
            {
                break;
            }

            d->demoteFromBoundCRS();
            PJ* coordSys = nullptr;
            if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
            {
                auto subCRS = proj_crs_get_sub_crs(
                    d->getPROJContext(), d->m_pj_crs, 1);
                if( subCRS && proj_get_type(subCRS) == PJ_TYPE_BOUND_CRS )
                {
                    auto temp = proj_get_source_crs(
                        d->getPROJContext(), subCRS);
                    proj_destroy(subCRS);
                    subCRS = temp;
                }
                if( subCRS && proj_get_type(subCRS) == PJ_TYPE_VERTICAL_CRS )
                {
                    coordSys = proj_crs_get_coordinate_system(
                        d->getPROJContext(), subCRS);
                    proj_destroy(subCRS);
                }
                else
                {
                    proj_destroy(subCRS);
                    d->undoDemoteFromBoundCRS();
                    break;
                }
            }
            else
            {
                coordSys = proj_crs_get_coordinate_system(
                    d->getPROJContext(), d->m_pj_crs);
            }

            d->undoDemoteFromBoundCRS();
            if( !coordSys )
            {
                break;
            }
            auto csType = proj_cs_get_type(d->getPROJContext(), coordSys);

            if( csType != PJ_CS_TYPE_CARTESIAN
                && csType != PJ_CS_TYPE_VERTICAL
                && csType != PJ_CS_TYPE_ELLIPSOIDAL
                && csType != PJ_CS_TYPE_SPHERICAL )
            {
                proj_destroy(coordSys);
                break;
            }

            int axis = 0;

            if ( csType == PJ_CS_TYPE_ELLIPSOIDAL
                 || csType == PJ_CS_TYPE_SPHERICAL )
            {
                const int axisCount = proj_cs_get_axis_count(
                    d->getPROJContext(), coordSys);

                if( axisCount == 3 )
                {
                    axis = 2;
                }
                else
                {
                    proj_destroy(coordSys);
                    break;
                }
            }

            double dfConvFactor = 0.0;
            const char* pszUnitName = nullptr;
            if( !proj_cs_get_axis_info(
                d->getPROJContext(), coordSys, axis, nullptr, nullptr, nullptr,
                &dfConvFactor, &pszUnitName, nullptr, nullptr) )
            {
                proj_destroy(coordSys);
                break;
            }

            d->m_osLinearUnits = pszUnitName;
            d->dfToMeter = dfConvFactor;
            if( ppszName )
                *ppszName = d->m_osLinearUnits.c_str();

            proj_destroy(coordSys);
            return dfConvFactor;
        }

        d->m_osLinearUnits = "unknown";
        d->dfToMeter = 1.0;

        if( ppszName != nullptr )
            *ppszName = d->m_osLinearUnits.c_str();
        return 1.0;
    }


    const OGR_SRSNode *poCS = GetAttrNode( pszTargetKey );

    if( ppszName != nullptr )
        *ppszName = "unknown";

    if( poCS == nullptr )
        return 1.0;

    for( int iChild = 0; iChild < poCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode *poChild = poCS->GetChild(iChild);

        if( EQUAL(poChild->GetValue(), "UNIT")
            && poChild->GetChildCount() >= 2 )
        {
            if( ppszName != nullptr )
              *ppszName = poChild->GetChild(0)->GetValue();

            return CPLAtof( poChild->GetChild(1)->GetValue() );
        }
    }

    return 1.0;
}

/**
 * \brief Fetch linear units for target.
 *
 * If no units are available, a value of "Meters" and 1.0 will be assumed.
 *
 * This method does the same thing as the C function OSRGetTargetLinearUnits()
 *
 * @param pszTargetKey the key to look on. i.e. "PROJCS" or "VERT_CS". Might be
 * NULL, in which case PROJCS will be implied (and if not found, LOCAL_CS,
 * GEOCCS and VERT_CS are looked up)
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should not
 * be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call. ppszName can be set to NULL.
 *
 * @return the value to multiply by linear distances to transform them to
 * meters.
 *
 * @since GDAL 2.3.0
 */

double OGRSpatialReference::GetTargetLinearUnits( const char *pszTargetKey,
                                                  char ** ppszName ) const

{
    return GetTargetLinearUnits( pszTargetKey,
                                 const_cast<const char**>(ppszName) );
}

/************************************************************************/
/*                      OSRGetTargetLinearUnits()                       */
/************************************************************************/

/**
 * \brief Fetch linear projection units.
 *
 * This function is the same as OGRSpatialReference::GetTargetLinearUnits()
 *
 * @since OGR 1.9.0
 */
double OSRGetTargetLinearUnits( OGRSpatialReferenceH hSRS,
                                const char *pszTargetKey,
                                char ** ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetTargetLinearUnits", 0 );

    return ToPointer(hSRS)->
        GetTargetLinearUnits( pszTargetKey, const_cast<const char**>(ppszName) );
}

/************************************************************************/
/*                          GetPrimeMeridian()                          */
/************************************************************************/

/**
 * \brief Fetch prime meridian info.
 *
 * Returns the offset of the prime meridian from greenwich in degrees,
 * and the prime meridian name (if requested).   If no PRIMEM value exists
 * in the coordinate system definition a value of "Greenwich" and an
 * offset of 0.0 is assumed.
 *
 * If the prime meridian name is returned, the pointer is to an internal
 * copy of the name. It should not be freed, altered or depended on after
 * the next OGR call.
 *
 * This method is the same as the C function OSRGetPrimeMeridian().
 *
 * @param ppszName return location for prime meridian name.  If NULL, name
 * is not returned.
 *
 * @return the offset to the GEOGCS prime meridian from greenwich in decimal
 * degrees.
 * @deprecated GDAL 2.3.0. Use GetPrimeMeridian(const char**) const.
 */

double OGRSpatialReference::GetPrimeMeridian( const char **ppszName ) const

{
    d->refreshProjObj();

    if( !d->m_osPrimeMeridianName.empty() )
    {
        if( ppszName != nullptr )
            *ppszName = d->m_osPrimeMeridianName.c_str();
        return d->dfFromGreenwich;
    }

    while(true)
    {
        if( !d->m_pj_crs)
            break;

        auto pm = proj_get_prime_meridian(
            d->getPROJContext(), d->m_pj_crs);
        if( !pm )
            break;

        d->m_osPrimeMeridianName = proj_get_name(pm);
        if( ppszName )
            *ppszName = d->m_osPrimeMeridianName.c_str();
        double dfLongitude = 0.0;
        double dfConvFactor = 0.0;
        proj_prime_meridian_get_parameters(
            d->getPROJContext(), pm, &dfLongitude, &dfConvFactor, nullptr);
        proj_destroy(pm);
        d->dfFromGreenwich = dfLongitude * dfConvFactor / CPLAtof(SRS_UA_DEGREE_CONV);
        return d->dfFromGreenwich;
    }

    d->m_osPrimeMeridianName = SRS_PM_GREENWICH;
    d->dfFromGreenwich = 0.0;
    if( ppszName != nullptr )
        *ppszName = d->m_osPrimeMeridianName.c_str();
    return d->dfFromGreenwich;
}

/**
 * \brief Fetch prime meridian info.
 *
 * Returns the offset of the prime meridian from greenwich in degrees,
 * and the prime meridian name (if requested).   If no PRIMEM value exists
 * in the coordinate system definition a value of "Greenwich" and an
 * offset of 0.0 is assumed.
 *
 * If the prime meridian name is returned, the pointer is to an internal
 * copy of the name. It should not be freed, altered or depended on after
 * the next OGR call.
 *
 * This method is the same as the C function OSRGetPrimeMeridian().
 *
 * @param ppszName return location for prime meridian name.  If NULL, name
 * is not returned.
 *
 * @return the offset to the GEOGCS prime meridian from greenwich in decimal
 * degrees.
 * @since GDAL 2.3.0
 */

double OGRSpatialReference::GetPrimeMeridian( char **ppszName ) const

{
    return GetPrimeMeridian( const_cast<const char**>(ppszName) );
}

/************************************************************************/
/*                        OSRGetPrimeMeridian()                         */
/************************************************************************/

/**
 * \brief Fetch prime meridian info.
 *
 * This function is the same as OGRSpatialReference::GetPrimeMeridian()
 */
double OSRGetPrimeMeridian( OGRSpatialReferenceH hSRS, char **ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetPrimeMeridian", 0 );

    return ToPointer(hSRS)->
        GetPrimeMeridian( const_cast<const char**>(ppszName) );
}

/************************************************************************/
/*                             SetGeogCS()                              */
/************************************************************************/

/**
 * \brief Set geographic coordinate system.
 *
 * This method is used to set the datum, ellipsoid, prime meridian and
 * angular units for a geographic coordinate system.  It can be used on its
 * own to establish a geographic spatial reference, or applied to a
 * projected coordinate system to establish the underlying geographic
 * coordinate system.
 *
 * This method does the same as the C function OSRSetGeogCS().
 *
 * @param pszGeogName user visible name for the geographic coordinate system
 * (not to serve as a key).
 *
 * @param pszDatumName key name for this datum.  The OpenGIS specification
 * lists some known values, and otherwise EPSG datum names with a standard
 * transformation are considered legal keys.
 *
 * @param pszSpheroidName user visible spheroid name (not to serve as a key)
 *
 * @param dfSemiMajor the semi major axis of the spheroid.
 *
 * @param dfInvFlattening the inverse flattening for the spheroid.
 * This can be computed from the semi minor axis as
 * 1/f = 1.0 / (1.0 - semiminor/semimajor).
 *
 * @param pszPMName the name of the prime meridian (not to serve as a key)
 * If this is NULL a default value of "Greenwich" will be used.
 *
 * @param dfPMOffset the longitude of Greenwich relative to this prime
 * meridian. Always in Degrees
 *
 * @param pszAngularUnits the angular units name (see ogr_srs_api.h for some
 * standard names).  If NULL a value of "degrees" will be assumed.
 *
 * @param dfConvertToRadians value to multiply angular units by to transform
 * them to radians.  A value of SRS_UA_DEGREE_CONV will be used if
 * pszAngularUnits is NULL.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr
OGRSpatialReference::SetGeogCS( const char * pszGeogName,
                                const char * pszDatumName,
                                const char * pszSpheroidName,
                                double dfSemiMajor, double dfInvFlattening,
                                const char * pszPMName, double dfPMOffset,
                                const char * pszAngularUnits,
                                double dfConvertToRadians )

{
    d->bNormInfoSet = FALSE;
    d->m_osAngularUnits.clear();
    d->m_dfAngularUnitToRadian = 0.0;
    d->m_osPrimeMeridianName.clear();
    d->dfFromGreenwich = 0.0;

/* -------------------------------------------------------------------- */
/*      For a geocentric coordinate system we want to set the datum     */
/*      and ellipsoid based on the GEOGCS.  Create the GEOGCS in a      */
/*      temporary srs and use the copy method which has special         */
/*      handling for GEOCCS.                                            */
/* -------------------------------------------------------------------- */
    if( IsGeocentric() )
    {
        OGRSpatialReference oGCS;

        oGCS.SetGeogCS( pszGeogName, pszDatumName, pszSpheroidName,
                        dfSemiMajor, dfInvFlattening,
                        pszPMName, dfPMOffset,
                        pszAngularUnits, dfConvertToRadians );
        return CopyGeogCSFrom( &oGCS );
    }

    auto cs = proj_create_ellipsoidal_2D_cs(
        d->getPROJContext(),PJ_ELLPS2D_LATITUDE_LONGITUDE,
        pszAngularUnits, dfConvertToRadians);
    // Prime meridian expressed in Degree
    auto obj = proj_create_geographic_crs(
        d->getPROJContext(),pszGeogName, pszDatumName, pszSpheroidName,
        dfSemiMajor, dfInvFlattening, pszPMName, dfPMOffset,
        nullptr, 0.0, cs);
    proj_destroy(cs);

    if( d->m_pj_crs == nullptr ||
        d->m_pjType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
        d->m_pjType == PJ_TYPE_GEOGRAPHIC_3D_CRS )
    {
        d->setPjCRS(obj);
    }
    else if( d->m_pjType == PJ_TYPE_PROJECTED_CRS )
    {
        d->setPjCRS(
            proj_crs_alter_geodetic_crs(
                d->getPROJContext(), d->m_pj_crs, obj));
        proj_destroy(obj);
    }
    else
    {
        proj_destroy(obj);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetGeogCS()                            */
/************************************************************************/

/**
 * \brief Set geographic coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetGeogCS()
 */
OGRErr OSRSetGeogCS( OGRSpatialReferenceH hSRS,
                     const char * pszGeogName,
                     const char * pszDatumName,
                     const char * pszSpheroidName,
                     double dfSemiMajor, double dfInvFlattening,
                     const char * pszPMName, double dfPMOffset,
                     const char * pszAngularUnits,
                     double dfConvertToRadians )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGeogCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetGeogCS(
        pszGeogName, pszDatumName,
        pszSpheroidName, dfSemiMajor, dfInvFlattening,
        pszPMName, dfPMOffset, pszAngularUnits, dfConvertToRadians );
}

/************************************************************************/
/*                         SetWellKnownGeogCS()                         */
/************************************************************************/

/**
 * \brief Set a GeogCS based on well known name.
 *
 * This may be called on an empty OGRSpatialReference to make a geographic
 * coordinate system, or on something with an existing PROJCS node to
 * set the underlying geographic coordinate system of a projected coordinate
 * system.
 *
 * The following well known text values are currently supported,
 * Except for "EPSG:n", the others are without dependency on EPSG data files:
 * <ul>
 * <li> "EPSG:n": where n is the code a Geographic coordinate reference system.
 * <li> "WGS84": same as "EPSG:4326" (axis order lat/long).
 * <li> "WGS72": same as "EPSG:4322" (axis order lat/long).
 * <li> "NAD83": same as "EPSG:4269" (axis order lat/long).
 * <li> "NAD27": same as "EPSG:4267" (axis order lat/long).
 * <li> "CRS84", "CRS:84": same as "WGS84" but with axis order long/lat.
 * <li> "CRS72", "CRS:72": same as "WGS72" but with axis order long/lat.
 * <li> "CRS27", "CRS:27": same as "NAD27" but with axis order long/lat.
 * </ul>
 *
 * @param pszName name of well known geographic coordinate system.
 * @return OGRERR_NONE on success, or OGRERR_FAILURE if the name isn't
 * recognised, the target object is already initialized, or an EPSG value
 * can't be successfully looked up.
 */

OGRErr OGRSpatialReference::SetWellKnownGeogCS( const char * pszName )

{
/* -------------------------------------------------------------------- */
/*      Check for EPSG authority numbers.                               */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszName, "EPSG:") || STARTS_WITH_CI(pszName, "EPSGA:") )
    {
        OGRSpatialReference oSRS2;
        const OGRErr eErr = oSRS2.importFromEPSG( atoi(pszName+5) );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !oSRS2.IsGeographic() )
            return OGRERR_FAILURE;

        return CopyGeogCSFrom( &oSRS2 );
    }

/* -------------------------------------------------------------------- */
/*      Check for simple names.                                         */
/* -------------------------------------------------------------------- */
    const char *pszWKT = nullptr;

    if( EQUAL(pszName, "WGS84") )
    {
        pszWKT = SRS_WKT_WGS84_LAT_LONG;
    }
    else if( EQUAL(pszName, "CRS84") ||
             EQUAL(pszName, "CRS:84") )
    {
        pszWKT =
            "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\","
            "SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],"
            "AUTHORITY[\"EPSG\",\"6326\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AXIS[\"Longitude\",EAST],AXIS[\"Latitude\",NORTH]]";
    }
    else if( EQUAL(pszName, "WGS72") )
        pszWKT =
            "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\","
            "SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",\"7043\"]],"
            "AUTHORITY[\"EPSG\",\"6322\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],"
            "AUTHORITY[\"EPSG\",\"4322\"]]";

    else if( EQUAL(pszName, "NAD27") )
        pszWKT =
            "GEOGCS[\"NAD27\",DATUM[\"North_American_Datum_1927\","
            "SPHEROID[\"Clarke 1866\",6378206.4,294.9786982138982,"
            "AUTHORITY[\"EPSG\",\"7008\"]],AUTHORITY[\"EPSG\",\"6267\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],"
            "AUTHORITY[\"EPSG\",\"4267\"]]";

    else if( EQUAL(pszName, "CRS27") || EQUAL(pszName, "CRS:27") )
        pszWKT =
            "GEOGCS[\"NAD27\",DATUM[\"North_American_Datum_1927\","
            "SPHEROID[\"Clarke 1866\",6378206.4,294.9786982138982,"
            "AUTHORITY[\"EPSG\",\"7008\"]],AUTHORITY[\"EPSG\",\"6267\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AXIS[\"Longitude\",EAST],AXIS[\"Latitude\",NORTH]]";

    else if( EQUAL(pszName, "NAD83") )
        pszWKT =
            "GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\","
            "SPHEROID[\"GRS 1980\",6378137,298.257222101,"
            "AUTHORITY[\"EPSG\",\"7019\"]],"
            "AUTHORITY[\"EPSG\",\"6269\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],AUTHORITY[\"EPSG\",\"4269\"]]";

    else if(  EQUAL(pszName, "CRS83") ||  EQUAL(pszName, "CRS:83") )
        pszWKT =
            "GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\","
            "SPHEROID[\"GRS 1980\",6378137,298.257222101,"
            "AUTHORITY[\"EPSG\",\"7019\"]],"
            "AUTHORITY[\"EPSG\",\"6269\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AXIS[\"Longitude\",EAST],AXIS[\"Latitude\",NORTH]]";

    else
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Import the WKT                                                  */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS2;
    const OGRErr eErr = oSRS2.importFromWkt( pszWKT );
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Copy over.                                                      */
/* -------------------------------------------------------------------- */
    return CopyGeogCSFrom( &oSRS2 );
}

/************************************************************************/
/*                       OSRSetWellKnownGeogCS()                        */
/************************************************************************/

/**
 * \brief Set a GeogCS based on well known name.
 *
 * This function is the same as OGRSpatialReference::SetWellKnownGeogCS()
 */
OGRErr OSRSetWellKnownGeogCS( OGRSpatialReferenceH hSRS, const char *pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetWellKnownGeogCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetWellKnownGeogCS( pszName );
}

/************************************************************************/
/*                           CopyGeogCSFrom()                           */
/************************************************************************/

/**
 * \brief Copy GEOGCS from another OGRSpatialReference.
 *
 * The GEOGCS information is copied into this OGRSpatialReference from another.
 * If this object has a PROJCS root already, the GEOGCS is installed within
 * it, otherwise it is installed as the root.
 *
 * @param poSrcSRS the spatial reference to copy the GEOGCS information from.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGRSpatialReference::CopyGeogCSFrom(
    const OGRSpatialReference * poSrcSRS )

{
    d->bNormInfoSet = FALSE;
    d->m_osAngularUnits.clear();
    d->m_dfAngularUnitToRadian = 0.0;
    d->m_osPrimeMeridianName.clear();
    d->dfFromGreenwich = 0.0;

    d->refreshProjObj();
    poSrcSRS->d->refreshProjObj();
    if( !poSrcSRS->d->m_pj_crs )
    {
        return OGRERR_FAILURE;
    }
    auto geodCRS = proj_crs_get_geodetic_crs(
        d->getPROJContext(),poSrcSRS->d->m_pj_crs);
    if( !geodCRS )
    {
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Handle geocentric coordinate systems specially.  We just        */
/*      want to copy the DATUM.                                         */
/* -------------------------------------------------------------------- */
    if( d->m_pjType == PJ_TYPE_GEOCENTRIC_CRS )
    {
        auto datum = proj_crs_get_datum(
            d->getPROJContext(), geodCRS);
#if PROJ_VERSION_MAJOR > 7 || (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 2)
        if( datum == nullptr )
        {
            datum = proj_crs_get_datum_ensemble(d->getPROJContext(), geodCRS);
        }
#endif
        if( datum == nullptr )
        {
            proj_destroy(geodCRS);
            return OGRERR_FAILURE;
        }

        const char* pszUnitName = nullptr;
        double unitConvFactor = GetLinearUnits(&pszUnitName);

        auto pj_crs = proj_create_geocentric_crs_from_datum(
            d->getPROJContext(),
            proj_get_name(d->m_pj_crs), datum, pszUnitName, unitConvFactor);
        proj_destroy(datum);

        d->setPjCRS(pj_crs);
    }

    else if( d->m_pjType == PJ_TYPE_PROJECTED_CRS )
    {
        auto pj_crs = proj_crs_alter_geodetic_crs(
            d->getPROJContext(), d->m_pj_crs, geodCRS);
        d->setPjCRS(pj_crs);
    }

    else
    {
        d->setPjCRS(proj_clone(d->getPROJContext(), geodCRS));
    }

    // Apply TOWGS84 of source CRS
    if( poSrcSRS->d->m_pjType == PJ_TYPE_BOUND_CRS )
    {
        auto target = proj_get_target_crs(
            d->getPROJContext(), poSrcSRS->d->m_pj_crs);
        auto co = proj_crs_get_coordoperation(
            d->getPROJContext(), poSrcSRS->d->m_pj_crs);
        d->setPjCRS(proj_crs_create_bound_crs(
            d->getPROJContext(), d->m_pj_crs, target, co));
        proj_destroy(target);
        proj_destroy(co);
    }

    proj_destroy(geodCRS);

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRCopyGeogCSFrom()                          */
/************************************************************************/

/**
 * \brief Copy GEOGCS from another OGRSpatialReference.
 *
 * This function is the same as OGRSpatialReference::CopyGeogCSFrom()
 */
OGRErr OSRCopyGeogCSFrom( OGRSpatialReferenceH hSRS,
                          const OGRSpatialReferenceH hSrcSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRCopyGeogCSFrom", OGRERR_FAILURE );
    VALIDATE_POINTER1( hSrcSRS, "OSRCopyGeogCSFrom", OGRERR_FAILURE );

    return ToPointer(hSRS)->CopyGeogCSFrom(ToPointer(hSrcSRS) );
}

/************************************************************************/
/*                   SET_FROM_USER_INPUT_LIMITATIONS_get()              */
/************************************************************************/

/** Limitations for OGRSpatialReference::SetFromUserInput().
 *
 * Currently ALLOW_NETWORK_ACCESS=NO and ALLOW_FILE_ACCESS=NO.
 */
const char* const OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS[] = {
    "ALLOW_NETWORK_ACCESS=NO", "ALLOW_FILE_ACCESS=NO", nullptr };

/**
 * \brief Return OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS
 */
CSLConstList OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()
{
    return SET_FROM_USER_INPUT_LIMITATIONS;
}

/************************************************************************/
/*                          SetFromUserInput()                          */
/************************************************************************/

/**
 * \brief Set spatial reference from various text formats.
 *
 * This method will examine the provided input, and try to deduce the
 * format, and then use it to initialize the spatial reference system.  It
 * may take the following forms:
 *
 * <ol>
 * <li> Well Known Text definition - passed on to importFromWkt().
 * <li> "EPSG:n" - number passed on to importFromEPSG().
 * <li> "EPSGA:n" - number passed on to importFromEPSGA().
 * <li> "AUTO:proj_id,unit_id,lon0,lat0" - WMS auto projections.
 * <li> "urn:ogc:def:crs:EPSG::n" - ogc urns
 * <li> PROJ.4 definitions - passed on to importFromProj4().
 * <li> filename - file read for WKT, XML or PROJ.4 definition.
 * <li> well known name accepted by SetWellKnownGeogCS(), such as NAD27, NAD83,
 * WGS84 or WGS72.
 * <li> "IGNF:xxxx", "ESRI:xxxx", etc. from definitions from the PROJ database;
 * <li> PROJJSON (PROJ &gt;= 6.2)
 * </ol>
 *
 * It is expected that this method will be extended in the future to support
 * XML and perhaps a simplified "minilanguage" for indicating common UTM and
 * State Plane definitions.
 *
 * This method is intended to be flexible, but by its nature it is
 * imprecise as it must guess information about the format intended.  When
 * possible applications should call the specific method appropriate if the
 * input is known to be in a particular format.
 *
 * This method does the same thing as the OSRSetFromUserInput() function.
 *
 * @param pszDefinition text definition to try to deduce SRS from.
 *
 * @return OGRERR_NONE on success, or an error code if the name isn't
 * recognised, the definition is corrupt, or an EPSG value can't be
 * successfully looked up.
 */

OGRErr OGRSpatialReference::SetFromUserInput( const char * pszDefinition )
{
    return SetFromUserInput(pszDefinition, nullptr);
}

/**
 * \brief Set spatial reference from various text formats.
 *
 * This method will examine the provided input, and try to deduce the
 * format, and then use it to initialize the spatial reference system.  It
 * may take the following forms:
 *
 * <ol>
 * <li> Well Known Text definition - passed on to importFromWkt().
 * <li> "EPSG:n" - number passed on to importFromEPSG().
 * <li> "EPSGA:n" - number passed on to importFromEPSGA().
 * <li> "AUTO:proj_id,unit_id,lon0,lat0" - WMS auto projections.
 * <li> "urn:ogc:def:crs:EPSG::n" - ogc urns
 * <li> PROJ.4 definitions - passed on to importFromProj4().
 * <li> filename - file read for WKT, XML or PROJ.4 definition.
 * <li> well known name accepted by SetWellKnownGeogCS(), such as NAD27, NAD83,
 * WGS84 or WGS72.
 * <li> "IGNF:xxxx", "ESRI:xxxx", etc. from definitions from the PROJ database;
 * <li> PROJJSON (PROJ &gt;= 6.2)
 * </ol>
 *
 * It is expected that this method will be extended in the future to support
 * XML and perhaps a simplified "minilanguage" for indicating common UTM and
 * State Plane definitions.
 *
 * This method is intended to be flexible, but by its nature it is
 * imprecise as it must guess information about the format intended.  When
 * possible applications should call the specific method appropriate if the
 * input is known to be in a particular format.
 *
 * This method does the same thing as the OSRSetFromUserInput() function.
 *
 * @param pszDefinition text definition to try to deduce SRS from.
 *
 * @param papszOptions NULL terminated list of options, or NULL.
 * <ol>
 * <li> ALLOW_NETWORK_ACCESS=YES/NO.
 *      Whether http:// or https:// access is allowed. Defaults to YES.
 * <li> ALLOW_FILE_ACCESS=YES/NO.
 *      Whether reading a file using the Virtual File System layer is allowed (can also involve network access). Defaults to YES.
 * </ol>
 *
 * @return OGRERR_NONE on success, or an error code if the name isn't
 * recognised, the definition is corrupt, or an EPSG value can't be
 * successfully looked up.
 */

OGRErr OGRSpatialReference::SetFromUserInput( const char * pszDefinition,
                                              CSLConstList papszOptions )
{
    if( STARTS_WITH_CI(pszDefinition, "ESRI::") )
    {
        pszDefinition += 6;
    }

/* -------------------------------------------------------------------- */
/*      Is it a recognised syntax?                                      */
/* -------------------------------------------------------------------- */
    const char* const wktKeywords[] = {
        // WKT1
        "GEOGCS", "GEOCCS", "PROJCS", "VERT_CS", "COMPD_CS", "LOCAL_CS",
        // WKT2"
        "GEODCRS", "GEOGCRS", "GEODETICCRS", "GEOGRAPHICCRS", "PROJCRS",
        "PROJECTEDCRS", "VERTCRS", "VERTICALCRS", "COMPOUNDCRS",
        "ENGCRS", "ENGINEERINGCRS", "BOUNDCRS", "DERIVEDPROJCRS"
    };
    for( const char* keyword: wktKeywords )
    {
        if( STARTS_WITH_CI(pszDefinition, keyword) )
        {
            return importFromWkt( pszDefinition );
        }
    }

    if( STARTS_WITH_CI(pszDefinition, "EPSG:")
        || STARTS_WITH_CI(pszDefinition, "EPSGA:") )
    {
        OGRErr eStatus = OGRERR_NONE;

        if( STARTS_WITH_CI(pszDefinition, "EPSG:") )
            eStatus = importFromEPSG( atoi(pszDefinition+5) );

        else // if( STARTS_WITH_CI(pszDefinition, "EPSGA:") )
            eStatus = importFromEPSGA( atoi(pszDefinition+6) );

        // Do we want to turn this into a compound definition
        // with a vertical datum?
        if( eStatus == OGRERR_NONE && strchr( pszDefinition, '+' ) != nullptr )
        {
            OGRSpatialReference oVertSRS;

            eStatus = oVertSRS.importFromEPSG(
                atoi(strchr(pszDefinition, '+') + 1) );
            if( eStatus == OGRERR_NONE )
            {
                OGRSpatialReference oHorizSRS(*this);

                Clear();

                oHorizSRS.d->refreshProjObj();
                oVertSRS.d->refreshProjObj();
                if( !oHorizSRS.d->m_pj_crs || !oVertSRS.d->m_pj_crs )
                    return OGRERR_FAILURE;

                const char* pszHorizName = proj_get_name(oHorizSRS.d->m_pj_crs);
                const char* pszVertName = proj_get_name(oVertSRS.d->m_pj_crs);

                CPLString osName = pszHorizName ? pszHorizName : "";
                osName += " + ";
                osName += pszVertName ? pszVertName : "";

                SetCompoundCS(osName, &oHorizSRS, &oVertSRS);
            }
        }

        return eStatus;
    }

    if( STARTS_WITH_CI(pszDefinition, "urn:ogc:def:crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:ogc:def:crs,crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:x-ogc:def:crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:opengis:crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:opengis:def:crs:"))
        return importFromURN( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "http://opengis.net/def/crs")
        || STARTS_WITH_CI(pszDefinition, "https://opengis.net/def/crs")
        || STARTS_WITH_CI(pszDefinition, "http://www.opengis.net/def/crs")
        || STARTS_WITH_CI(pszDefinition, "https://www.opengis.net/def/crs")
        || STARTS_WITH_CI(pszDefinition, "www.opengis.net/def/crs"))
        return importFromCRSURL( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "AUTO:") )
        return importFromWMSAUTO( pszDefinition );

    // WMS/WCS OGC codes like OGC:CRS84.
    if( STARTS_WITH_CI(pszDefinition, "OGC:") )
        return SetWellKnownGeogCS( pszDefinition+4 );

    if( STARTS_WITH_CI(pszDefinition, "CRS:") )
        return SetWellKnownGeogCS( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "DICT:")
        && strstr(pszDefinition, ",") )
    {
        char *pszFile = CPLStrdup(pszDefinition+5);
        char *pszCode = strstr(pszFile, ",") + 1;

        pszCode[-1] = '\0';

        OGRErr err = importFromDict( pszFile, pszCode );
        CPLFree( pszFile );

        return err;
    }

    if( EQUAL(pszDefinition, "NAD27")
        || EQUAL(pszDefinition,"NAD83")
        || EQUAL(pszDefinition,"WGS84")
        || EQUAL(pszDefinition,"WGS72") )
    {
        Clear();
        return SetWellKnownGeogCS( pszDefinition );
    }

    // PROJJSON
    if( pszDefinition[0] == '{' && strstr(pszDefinition, "\"type\"") &&
        (strstr(pszDefinition, "GeodeticCRS") ||
         strstr(pszDefinition, "GeographicCRS") ||
         strstr(pszDefinition, "ProjectedCRS") ||
         strstr(pszDefinition, "VerticalCRS") ||
         strstr(pszDefinition, "BoundCRS") ||
         strstr(pszDefinition, "CompoundCRS")) )
    {
        auto obj = proj_create(d->getPROJContext(), pszDefinition);
        if( !obj )
        {
            return OGRERR_FAILURE;
        }
        Clear();
        d->setPjCRS(obj);
        return OGRERR_NONE;
    }

    if( strstr(pszDefinition, "+proj") != nullptr
             || strstr(pszDefinition, "+init") != nullptr )
        return importFromProj4( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "http://") || STARTS_WITH_CI(pszDefinition, "https://") )
    {
        if( CPLTestBool(CSLFetchNameValueDef(papszOptions, "ALLOW_NETWORK_ACCESS", "YES")) )
            return importFromUrl (pszDefinition);

        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot import %s due to ALLOW_NETWORK_ACCESS=NO",
                 pszDefinition);
        return OGRERR_FAILURE;
    }

    if( EQUAL(pszDefinition, "osgb:BNG") )
    {
        return importFromEPSG(27700);
    }

    // Deal with IGNF:xxx, ESRI:xxx, etc from the PROJ database
    const char* pszDot = strchr(pszDefinition, ':');
    if( pszDot )
    {
        CPLString osPrefix(pszDefinition, pszDot - pszDefinition);
        auto authorities = proj_get_authorities_from_database(d->getPROJContext());
        if( authorities )
        {
            for( auto iter = authorities; *iter; ++iter )
            {
                if( *iter == osPrefix )
                {
                    proj_string_list_destroy(authorities);

                    auto obj = proj_create_from_database(d->getPROJContext(),
                        osPrefix, pszDot + 1, PJ_CATEGORY_CRS,
                        false, nullptr);
                    if( !obj )
                    {
                        return OGRERR_FAILURE;
                    }
                    Clear();
                    d->setPjCRS(obj);
                    return OGRERR_NONE;
                }
            }
            proj_string_list_destroy(authorities);
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to open it as a file.                                       */
/* -------------------------------------------------------------------- */
    if( !CPLTestBool(CSLFetchNameValueDef(papszOptions, "ALLOW_FILE_ACCESS", "YES")) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot import %s due to ALLOW_FILE_ACCESS=NO",
                 pszDefinition);
        return OGRERR_FAILURE;
    }

    CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
    VSILFILE * const fp = VSIFOpenL( pszDefinition, "rt" );
    if( fp == nullptr )
        return OGRERR_CORRUPT_DATA;

    const size_t nBufMax = 100000;
    char * const pszBuffer = static_cast<char *>( CPLMalloc(nBufMax) );
    const size_t nBytes = VSIFReadL( pszBuffer, 1, nBufMax-1, fp );
    VSIFCloseL( fp );

    if( nBytes == nBufMax-1 )
    {
        CPLDebug( "OGR",
                  "OGRSpatialReference::SetFromUserInput(%s), opened file "
                  "but it is to large for our generous buffer.  Is it really "
                  "just a WKT definition?", pszDefinition );
        CPLFree( pszBuffer );
        return OGRERR_FAILURE;
    }

    pszBuffer[nBytes] = '\0';

    char *pszBufPtr = pszBuffer;
    while( pszBufPtr[0] == ' ' || pszBufPtr[0] == '\n' )
        pszBufPtr++;

    OGRErr err = OGRERR_NONE;
    if( pszBufPtr[0] == '<' )
        err = importFromXML( pszBufPtr );
    else if( (strstr(pszBuffer, "+proj") != nullptr
              || strstr(pszBuffer, "+init") != nullptr)
             && (strstr(pszBuffer, "EXTENSION") == nullptr
                 && strstr(pszBuffer, "extension") == nullptr) )
        err = importFromProj4( pszBufPtr );
    else
    {
        if( STARTS_WITH_CI(pszBufPtr, "ESRI::") )
        {
            pszBufPtr += 6;
        }

        // coverity[tainted_data]
        err = importFromWkt( pszBufPtr );
    }

    CPLFree( pszBuffer );

    return err;
}

/************************************************************************/
/*                        OSRSetFromUserInput()                         */
/************************************************************************/

/**
 * \brief Set spatial reference from various text formats.
 *
 * This function is the same as OGRSpatialReference::SetFromUserInput()
 */
OGRErr CPL_STDCALL OSRSetFromUserInput( OGRSpatialReferenceH hSRS,
                                        const char *pszDef )

{
    VALIDATE_POINTER1( hSRS, "OSRSetFromUserInput", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetFromUserInput( pszDef );
}

/************************************************************************/
/*                          ImportFromUrl()                             */
/************************************************************************/

/**
 * \brief Set spatial reference from a URL.
 *
 * This method will download the spatial reference at a given URL and
 * feed it into SetFromUserInput for you.
 *
 * This method does the same thing as the OSRImportFromUrl() function.
 *
 * @param pszUrl text definition to try to deduce SRS from.
 *
 * @return OGRERR_NONE on success, or an error code with the curl
 * error message if it is unable to download data.
 */

OGRErr OGRSpatialReference::importFromUrl( const char * pszUrl )

{
    if( !STARTS_WITH_CI(pszUrl, "http://") && !STARTS_WITH_CI(pszUrl, "https://") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "The given string is not recognized as a URL"
                  "starting with 'http://' -- %s", pszUrl );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result.                                               */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    const char* pszHeaders = "HEADERS=Accept: application/x-ogcwkt";
    const char* pszTimeout = "TIMEOUT=10";
    char *apszOptions[] = {
        const_cast<char *>(pszHeaders),
        const_cast<char *>(pszTimeout),
        nullptr
    };

    CPLHTTPResult *psResult = CPLHTTPFetch( pszUrl, apszOptions );

/* -------------------------------------------------------------------- */
/*      Try to handle errors.                                           */
/* -------------------------------------------------------------------- */

    if( psResult == nullptr )
        return OGRERR_FAILURE;
    if( psResult->nDataLen == 0
        || CPLGetLastErrorNo() != 0 || psResult->pabyData == nullptr )
    {
        if( CPLGetLastErrorNo() == 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "No data was returned from the given URL" );
        }
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }

    if( psResult->nStatus != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Curl reports error: %d: %s",
                  psResult->nStatus, psResult->pszErrBuf );
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }

    const char* pszData = reinterpret_cast<const char*>(psResult->pabyData);
    if( STARTS_WITH_CI(pszData, "http://") || STARTS_WITH_CI(pszData, "https://") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "The data that was downloaded also starts with 'http://' "
                  "and cannot be passed into SetFromUserInput.  Is this "
                  "really a spatial reference definition? ");
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }
    if( OGRERR_NONE != SetFromUserInput(pszData)) {
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }

    CPLHTTPDestroyResult( psResult );
    return OGRERR_NONE;
}

/************************************************************************/
/*                        OSRimportFromUrl()                            */
/************************************************************************/

/**
 * \brief Set spatial reference from a URL.
 *
 * This function is the same as OGRSpatialReference::importFromUrl()
 */
OGRErr OSRImportFromUrl( OGRSpatialReferenceH hSRS, const char *pszUrl )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromUrl", OGRERR_FAILURE );

    return
        ToPointer(hSRS)->importFromUrl( pszUrl );
}

/************************************************************************/
/*                         importFromURNPart()                          */
/************************************************************************/
OGRErr OGRSpatialReference::importFromURNPart(const char* pszAuthority,
                                              const char* pszCode,
                                              const char* pszURN)
{
#if PROJ_AT_LEAST_VERSION(8,1,0)
    (void)pszAuthority;
    (void)pszCode;
    (void)pszURN;
    return OGRERR_FAILURE;
#else
/* -------------------------------------------------------------------- */
/*      Is this an EPSG code? Note that we import it with EPSG          */
/*      preferred axis ordering for geographic coordinate systems.      */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszAuthority, "EPSG") )
        return importFromEPSGA( atoi(pszCode) );

/* -------------------------------------------------------------------- */
/*      Is this an IAU code?  Lets try for the IAU2000 dictionary.      */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszAuthority, "IAU") )
        return importFromDict( "IAU2000.wkt", pszCode );

/* -------------------------------------------------------------------- */
/*      Is this an OGC code?                                            */
/* -------------------------------------------------------------------- */
    if( !STARTS_WITH_CI(pszAuthority, "OGC") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URN %s has unrecognized authority.",
                  pszURN );
        return OGRERR_FAILURE;
    }

    if( STARTS_WITH_CI(pszCode, "CRS84") )
        return SetWellKnownGeogCS( pszCode );
    else if( STARTS_WITH_CI(pszCode, "CRS83") )
        return SetWellKnownGeogCS( pszCode );
    else if( STARTS_WITH_CI(pszCode, "CRS27") )
        return SetWellKnownGeogCS( pszCode );
    else if( STARTS_WITH_CI(pszCode, "84") )  // urn:ogc:def:crs:OGC:2:84
        return SetWellKnownGeogCS( "CRS84" );

/* -------------------------------------------------------------------- */
/*      Handle auto codes.  We need to convert from format              */
/*      AUTO42001:99:8888 to format AUTO:42001,99,8888.                 */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH_CI(pszCode, "AUTO") )
    {
        char szWMSAuto[100] = { '\0' };

        if( strlen(pszCode) > sizeof(szWMSAuto)-2 )
            return OGRERR_FAILURE;

        snprintf( szWMSAuto, sizeof(szWMSAuto), "AUTO:%s", pszCode + 4 );
        for( int i = 5; szWMSAuto[i] != '\0'; i++ )
        {
            if( szWMSAuto[i] == ':' )
                szWMSAuto[i] = ',';
        }

        return importFromWMSAUTO( szWMSAuto );
    }

/* -------------------------------------------------------------------- */
/*      Not a recognise OGC item.                                       */
/* -------------------------------------------------------------------- */
    CPLError( CE_Failure, CPLE_AppDefined,
              "URN %s value not supported.",
              pszURN );

    return OGRERR_FAILURE;
#endif
}

/************************************************************************/
/*                           importFromURN()                            */
/*                                                                      */
/*      See OGC recommendation paper 06-023r1 or later for details.     */
/************************************************************************/

/**
 * \brief Initialize from OGC URN.
 *
 * Initializes this spatial reference from a coordinate system defined
 * by an OGC URN prefixed with "urn:ogc:def:crs:" per recommendation
 * paper 06-023r1.  Currently EPSG and OGC authority values are supported,
 * including OGC auto codes, but not including CRS1 or CRS88 (NAVD88).
 *
 * This method is also support through SetFromUserInput() which can
 * normally be used for URNs.
 *
 * @param pszURN the urn string.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGRSpatialReference::importFromURN( const char *pszURN )

{
#if PROJ_AT_LEAST_VERSION(8,1,0)
/* -------------------------------------------------------------------- */
/*      Is this an IAU code?  Lets try for the IAU2000 dictionary.      */
/* -------------------------------------------------------------------- */
    const char* pszIAU = strstr(pszURN, "IAU");
    if( pszIAU )
    {
        const char* pszCode = strchr(pszIAU, ':');
        if( pszCode )
        {
            ++pszCode;
            if( *pszCode == ':' )
                ++pszCode;
            return importFromDict( "IAU2000.wkt", pszCode );
        }
    }
    if( strlen(pszURN) >= 1000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too long input string");
        return OGRERR_CORRUPT_DATA;
    }
    auto obj = proj_create(d->getPROJContext(), pszURN);
    if( !obj )
    {
        return OGRERR_FAILURE;
    }
    Clear();
    d->setPjCRS(obj);
    return OGRERR_NONE;
#else
    const char *pszCur = nullptr;

    if( STARTS_WITH_CI(pszURN, "urn:ogc:def:crs:") )
        pszCur = pszURN + 16;
    else if( STARTS_WITH_CI(pszURN, "urn:ogc:def:crs,crs:") )
        pszCur = pszURN + 20;
    else if( STARTS_WITH_CI(pszURN, "urn:x-ogc:def:crs:") )
        pszCur = pszURN + 18;
    else if( STARTS_WITH_CI(pszURN, "urn:opengis:crs:") )
        pszCur = pszURN + 16;
    else if( STARTS_WITH_CI(pszURN, "urn:opengis:def:crs:") )
        pszCur = pszURN + 20;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URN %s not a supported format.", pszURN );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    Clear();

/* -------------------------------------------------------------------- */
/*      Find code (ignoring version) out of string like:                */
/*                                                                      */
/*      authority:[version]:code                                        */
/* -------------------------------------------------------------------- */
    const char *pszAuthority = pszCur;

    // skip authority
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;

    // skip version
    const char* pszBeforeVersion = pszCur;
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;
    else
        // We come here in the case, the content to parse is authority:code
        // (instead of authority::code) which is probably illegal according to
        // http://www.opengeospatial.org/ogcUrnPolicy but such content is found
        // for example in what is returned by GeoServer.
        pszCur = pszBeforeVersion;

    const char *pszCode = pszCur;

    const char* pszComma = strchr(pszCur, ',');
    if( pszComma == nullptr )
        return importFromURNPart(pszAuthority, pszCode, pszURN);

    // There's a second part with the vertical SRS.
    pszCur = pszComma + 1;
    if( !STARTS_WITH(pszCur, "crs:") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URN %s not a supported format.", pszURN );
        return OGRERR_FAILURE;
    }

    pszCur += 4;

    char* pszFirstCode = CPLStrdup(pszCode);
    pszFirstCode[pszComma - pszCode] = '\0';
    OGRErr eStatus = importFromURNPart(pszAuthority, pszFirstCode, pszURN);
    CPLFree(pszFirstCode);

    // Do we want to turn this into a compound definition
    // with a vertical datum?
    if( eStatus != OGRERR_NONE )
        return eStatus;

    /* -------------------------------------------------------------------- */
    /*      Find code (ignoring version) out of string like:                */
    /*                                                                      */
    /*      authority:[version]:code                                        */
    /* -------------------------------------------------------------------- */
    pszAuthority = pszCur;

    // skip authority
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;

    // skip version
    pszBeforeVersion = pszCur;
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;
    else
        pszCur = pszBeforeVersion;

    pszCode = pszCur;

    OGRSpatialReference oVertSRS;
    eStatus = oVertSRS.importFromURNPart(pszAuthority, pszCode, pszURN);
    if( eStatus == OGRERR_NONE )
    {
        OGRSpatialReference oHorizSRS(*this);

        Clear();

        oHorizSRS.d->refreshProjObj();
        oVertSRS.d->refreshProjObj();
        if( !oHorizSRS.d->m_pj_crs || !oVertSRS.d->m_pj_crs )
            return OGRERR_FAILURE;

        const char* pszHorizName = proj_get_name(oHorizSRS.d->m_pj_crs);
        const char* pszVertName = proj_get_name(oVertSRS.d->m_pj_crs);

        CPLString osName = pszHorizName ? pszHorizName : "";
        osName += " + ";
        osName += pszVertName ? pszVertName : "";

        SetCompoundCS(osName, &oHorizSRS, &oVertSRS);
    }

    return eStatus;
#endif
}

/************************************************************************/
/*                           importFromCRSURL()                         */
/*                                                                      */
/*      See OGC Best Practice document 11-135 for details.              */
/************************************************************************/

/**
 * \brief Initialize from OGC URL.
 *
 * Initializes this spatial reference from a coordinate system defined
 * by an OGC URL prefixed with "http://opengis.net/def/crs" per best practice
 * paper 11-135.  Currently EPSG and OGC authority values are supported,
 * including OGC auto codes, but not including CRS1 or CRS88 (NAVD88).
 *
 * This method is also supported through SetFromUserInput() which can
 * normally be used for URLs.
 *
 * @param pszURL the URL string.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGRSpatialReference::importFromCRSURL( const char *pszURL )

{
#if PROJ_AT_LEAST_VERSION(8,1,0)
    if( strlen(pszURL) >= 10000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too long input string");
        return OGRERR_CORRUPT_DATA;
    }

    auto obj = proj_create(d->getPROJContext(), pszURL);
    if( !obj )
    {
        return OGRERR_FAILURE;
    }
    Clear();
    d->setPjCRS(obj);
    return OGRERR_NONE;
#else
    const char *pszCur = nullptr;

    if( STARTS_WITH_CI(pszURL, "http://opengis.net/def/crs") )
        pszCur = pszURL + 26;
    else if( STARTS_WITH_CI(pszURL, "https://opengis.net/def/crs") )
        pszCur = pszURL + 27;
    else if( STARTS_WITH_CI(pszURL, "http://www.opengis.net/def/crs") )
        pszCur = pszURL + 30;
    else if( STARTS_WITH_CI(pszURL, "https://www.opengis.net/def/crs") )
        pszCur = pszURL + 31;
    else if( STARTS_WITH_CI(pszURL, "www.opengis.net/def/crs") )
        pszCur = pszURL + 23;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URL %s not a supported format.", pszURL );
        return OGRERR_FAILURE;
    }

    if( *pszCur == '\0' )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "URL %s malformed.", pszURL);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    Clear();

    if( STARTS_WITH_CI(pszCur, "-compound?1=") )
    {
/* -------------------------------------------------------------------- */
/*      It's a compound CRS, of the form:                               */
/*                                                                      */
/*      http://opengis.net/def/crs-compound?1=URL1&2=URL2&3=URL3&..     */
/* -------------------------------------------------------------------- */
        pszCur += 12;

        // Extract each component CRS URL.
        int iComponentUrl = 2;

        CPLString osName = "";
        Clear();

        while( iComponentUrl != -1 )
        {
            char searchStr[15] = {};
            snprintf(searchStr, sizeof(searchStr), "&%d=", iComponentUrl);

            const char* pszUrlEnd = strstr(pszCur, searchStr);

            // Figure out the next component URL.
            char* pszComponentUrl = nullptr;

            if( pszUrlEnd )
            {
                size_t nLen = pszUrlEnd - pszCur;
                pszComponentUrl = static_cast<char *>(CPLMalloc(nLen + 1));
                strncpy(pszComponentUrl, pszCur, nLen);
                pszComponentUrl[nLen] = '\0';

                ++iComponentUrl;
                pszCur += nLen + strlen(searchStr);
            }
            else
            {
                if( iComponentUrl == 2 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Compound CRS URLs must have at least two component CRSs." );
                    return OGRERR_FAILURE;
                }
                else
                {
                    pszComponentUrl = CPLStrdup(pszCur);
                    // no more components
                    iComponentUrl = -1;
                }
            }

            OGRSpatialReference oComponentSRS;
            OGRErr eStatus = oComponentSRS.importFromCRSURL( pszComponentUrl );

            CPLFree(pszComponentUrl);
            pszComponentUrl = nullptr;

            if( eStatus == OGRERR_NONE )
            {
                if( osName.length() != 0 )
                {
                  osName += " + ";
                }
                osName += oComponentSRS.GetRoot()->GetValue();
                SetNode( "COMPD_CS", osName );
                GetRoot()->AddChild( oComponentSRS.GetRoot()->Clone() );
            }
            else
                return eStatus;
        }

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      It's a normal CRS URL, of the form:                             */
/*                                                                      */
/*      http://opengis.net/def/crs/AUTHORITY/VERSION/CODE               */
/* -------------------------------------------------------------------- */
    ++pszCur;
    const char *pszAuthority = pszCur;

    // skip authority
    while( *pszCur != '/' && *pszCur )
        pszCur++;
    if( *pszCur == '/' )
        pszCur++;

    // skip version
    while( *pszCur != '/' && *pszCur )
        pszCur++;
    if( *pszCur == '/' )
        pszCur++;

    const char *pszCode = pszCur;

    return importFromURNPart( pszAuthority, pszCode, pszURL );
#endif
}

/************************************************************************/
/*                         importFromWMSAUTO()                          */
/************************************************************************/

/**
 * \brief Initialize from WMSAUTO string.
 *
 * Note that the WMS 1.3 specification does not include the
 * units code, while apparently earlier specs do.  We try to
 * guess around this.
 *
 * @param pszDefinition the WMSAUTO string
 *
 * @return OGRERR_NONE on success or an error code.
 */
OGRErr OGRSpatialReference::importFromWMSAUTO( const char * pszDefinition )

{
#if PROJ_AT_LEAST_VERSION(8,1,0)
    if( strlen(pszDefinition) >= 10000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too long input string");
        return OGRERR_CORRUPT_DATA;
    }

    auto obj = proj_create(d->getPROJContext(), pszDefinition);
    if( !obj )
    {
        return OGRERR_FAILURE;
    }
    Clear();
    d->setPjCRS(obj);
    return OGRERR_NONE;
#else
    int nProjId, nUnitsId;
    double dfRefLong, dfRefLat = 0.0;

/* -------------------------------------------------------------------- */
/*      Tokenize                                                        */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszDefinition, "AUTO:") )
        pszDefinition += 5;

    char **papszTokens =
        CSLTokenizeStringComplex( pszDefinition, ",", FALSE, TRUE );

    if( CSLCount(papszTokens) == 4 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = atoi(papszTokens[1]);
        dfRefLong = CPLAtof(papszTokens[2]);
        dfRefLat = CPLAtof(papszTokens[3]);
    }
    else if( CSLCount(papszTokens) == 3 && atoi(papszTokens[0]) == 42005 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = atoi(papszTokens[1]);
        dfRefLong = CPLAtof(papszTokens[2]);
        dfRefLat = 0.0;
    }
    else if( CSLCount(papszTokens) == 3 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = 9001;
        dfRefLong = CPLAtof(papszTokens[1]);
        dfRefLat = CPLAtof(papszTokens[2]);
    }
    else if( CSLCount(papszTokens) == 2 && atoi(papszTokens[0]) == 42005 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = 9001;
        dfRefLong = CPLAtof(papszTokens[1]);
    }
    else
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "AUTO projection has wrong number of arguments, expected\n"
                  "AUTO:proj_id,units_id,ref_long,ref_lat or"
                  "AUTO:proj_id,ref_long,ref_lat" );
        return OGRERR_FAILURE;
    }

    CSLDestroy( papszTokens );
    papszTokens = nullptr;

/* -------------------------------------------------------------------- */
/*      Build coordsys.                                                 */
/* -------------------------------------------------------------------- */
    Clear();

/* -------------------------------------------------------------------- */
/*      Set WGS84.                                                      */
/* -------------------------------------------------------------------- */
    SetWellKnownGeogCS( "WGS84" );

    switch( nProjId )
    {
      case 42001: // Auto UTM
        SetUTM( static_cast<int>(floor( (dfRefLong + 180.0) / 6.0 )) + 1,
                dfRefLat >= 0.0 );
        break;

      case 42002: // Auto TM (strangely very UTM-like).
        SetTM( 0, dfRefLong, 0.9996,
               500000.0, (dfRefLat >= 0.0) ? 0.0 : 10000000.0 );
        break;

      case 42003: // Auto Orthographic.
        SetOrthographic( dfRefLat, dfRefLong, 0.0, 0.0 );
        break;

      case 42004: // Auto Equirectangular
        SetEquirectangular( dfRefLat, dfRefLong, 0.0, 0.0 );
        break;

      case 42005:
        SetMollweide( dfRefLong, 0.0, 0.0 );
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported projection id in importFromWMSAUTO(): %d",
                  nProjId );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Set units.                                                      */
/* -------------------------------------------------------------------- */

    switch( nUnitsId )
    {
      case 9001:
        SetTargetLinearUnits( nullptr, SRS_UL_METER, 1.0, "EPSG", "9001" );
        break;

      case 9002:
        SetTargetLinearUnits( nullptr, "Foot", 0.3048, "EPSG", "9002" );
        break;

      case 9003:
        SetTargetLinearUnits( nullptr,  "US survey foot", CPLAtof(SRS_UL_US_FOOT_CONV), "EPSG", "9003" );
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported units code (%d).",
                  nUnitsId );
        return OGRERR_FAILURE;
        break;
    }

    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                            GetSemiMajor()                            */
/************************************************************************/

/**
 * \brief Get spheroid semi major axis (in metres starting with GDAL 3.0)
 *
 * This method does the same thing as the C function OSRGetSemiMajor().
 *
 * @param pnErr if non-NULL set to OGRERR_FAILURE if semi major axis
 * can be found.
 *
 * @return semi-major axis, or SRS_WGS84_SEMIMAJOR if it can't be found.
 */

double OGRSpatialReference::GetSemiMajor( OGRErr * pnErr ) const

{
    if( pnErr != nullptr )
        *pnErr = OGRERR_FAILURE;

    d->refreshProjObj();
    if( !d->m_pj_crs )
        return SRS_WGS84_SEMIMAJOR;

    auto ellps = proj_get_ellipsoid(d->getPROJContext(), d->m_pj_crs);
    if( !ellps )
        return SRS_WGS84_SEMIMAJOR;

    double dfSemiMajor = 0.0;
    proj_ellipsoid_get_parameters(
        d->getPROJContext(), ellps, &dfSemiMajor, nullptr, nullptr, nullptr);
    proj_destroy(ellps);

    if( dfSemiMajor > 0 )
    {
        if( pnErr != nullptr )
            *pnErr = OGRERR_NONE;
        return dfSemiMajor;
    }

    return SRS_WGS84_SEMIMAJOR;
}

/************************************************************************/
/*                          OSRGetSemiMajor()                           */
/************************************************************************/

/**
 * \brief Get spheroid semi major axis.
 *
 * This function is the same as OGRSpatialReference::GetSemiMajor()
 */
double OSRGetSemiMajor( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetSemiMajor", 0 );

    return ToPointer(hSRS)->GetSemiMajor( pnErr );
}

/************************************************************************/
/*                          GetInvFlattening()                          */
/************************************************************************/

/**
 * \brief Get spheroid inverse flattening.
 *
 * This method does the same thing as the C function OSRGetInvFlattening().
 *
 * @param pnErr if non-NULL set to OGRERR_FAILURE if no inverse flattening
 * can be found.
 *
 * @return inverse flattening, or SRS_WGS84_INVFLATTENING if it can't be found.
 */

double OGRSpatialReference::GetInvFlattening( OGRErr * pnErr ) const

{
    if( pnErr != nullptr )
        *pnErr = OGRERR_FAILURE;

    d->refreshProjObj();
    if( !d->m_pj_crs )
        return SRS_WGS84_INVFLATTENING;

    auto ellps = proj_get_ellipsoid(d->getPROJContext(), d->m_pj_crs);
    if( !ellps )
        return SRS_WGS84_INVFLATTENING;

    double dfInvFlattening = -1.0;
    proj_ellipsoid_get_parameters(
        d->getPROJContext(), ellps, nullptr, nullptr, nullptr, &dfInvFlattening);
    proj_destroy(ellps);

    if( dfInvFlattening >= 0.0 )
    {
        if( pnErr != nullptr )
            *pnErr = OGRERR_NONE;
        return dfInvFlattening;
    }

    return SRS_WGS84_INVFLATTENING;
}

/************************************************************************/
/*                        OSRGetInvFlattening()                         */
/************************************************************************/

/**
 * \brief Get spheroid inverse flattening.
 *
 * This function is the same as OGRSpatialReference::GetInvFlattening()
 */
double OSRGetInvFlattening( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetInvFlattening", 0 );

    return ToPointer(hSRS)->GetInvFlattening( pnErr );
}

/************************************************************************/
/*                           GetEccentricity()                          */
/************************************************************************/

/**
 * \brief Get spheroid eccentricity
 *
 * @return eccentricity (or -1 in case of error)
 * @since GDAL 2.3
 */

double OGRSpatialReference::GetEccentricity() const

{
    OGRErr eErr = OGRERR_NONE;
    const double dfInvFlattening = GetInvFlattening(&eErr);
    if( eErr != OGRERR_NONE )
    {
        return -1.0;
    }
    if( dfInvFlattening == 0.0 )
        return 0.0;
    if( dfInvFlattening < 0.5 )
        return -1.0;
    return sqrt(2.0 / dfInvFlattening -
                    1.0 / (dfInvFlattening * dfInvFlattening));
}

/************************************************************************/
/*                      GetSquaredEccentricity()                        */
/************************************************************************/

/**
 * \brief Get spheroid squared eccentricity
 *
 * @return squared eccentricity (or -1 in case of error)
 * @since GDAL 2.3
 */

double OGRSpatialReference::GetSquaredEccentricity() const

{
    OGRErr eErr = OGRERR_NONE;
    const double dfInvFlattening = GetInvFlattening(&eErr);
    if( eErr != OGRERR_NONE )
    {
        return -1.0;
    }
    if( dfInvFlattening == 0.0 )
        return 0.0;
    if( dfInvFlattening < 0.5 )
        return -1.0;
    return 2.0 / dfInvFlattening -
                    1.0 / (dfInvFlattening * dfInvFlattening);
}

/************************************************************************/
/*                            GetSemiMinor()                            */
/************************************************************************/

/**
 * \brief Get spheroid semi minor axis.
 *
 * This method does the same thing as the C function OSRGetSemiMinor().
 *
 * @param pnErr if non-NULL set to OGRERR_FAILURE if semi minor axis
 * can be found.
 *
 * @return semi-minor axis, or WGS84 semi minor if it can't be found.
 */

double OGRSpatialReference::GetSemiMinor( OGRErr * pnErr ) const

{
    const double dfSemiMajor = GetSemiMajor( pnErr );
    const double dfInvFlattening = GetInvFlattening( pnErr );

    return OSRCalcSemiMinorFromInvFlattening(dfSemiMajor, dfInvFlattening);
}

/************************************************************************/
/*                          OSRGetSemiMinor()                           */
/************************************************************************/

/**
 * \brief Get spheroid semi minor axis.
 *
 * This function is the same as OGRSpatialReference::GetSemiMinor()
 */
double OSRGetSemiMinor( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetSemiMinor", 0 );

    return ToPointer(hSRS)->GetSemiMinor( pnErr );
}

/************************************************************************/
/*                             SetLocalCS()                             */
/************************************************************************/

/**
 * \brief Set the user visible LOCAL_CS name.
 *
 * This method is the same as the C function OSRSetLocalCS().
 *
 * This method will ensure a LOCAL_CS node is created as the root,
 * and set the provided name on it.  It must be used before SetLinearUnits().
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetLocalCS( const char * pszName )

{
    if( d->m_pjType == PJ_TYPE_UNKNOWN ||
        d->m_pjType == PJ_TYPE_ENGINEERING_CRS )
    {
        d->setPjCRS(proj_create_engineering_crs(
            d->getPROJContext(), pszName));
    }
    else
    {
        CPLDebug( "OGR",
                "OGRSpatialReference::SetLocalCS(%s) failed.  "
                "It appears an incompatible object already exists.",
                pszName );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetLocalCS()                            */
/************************************************************************/

/**
 * \brief Set the user visible LOCAL_CS name.
 *
 * This function is the same as OGRSpatialReference::SetLocalCS()
 */
OGRErr OSRSetLocalCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLocalCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetLocalCS( pszName );
}

/************************************************************************/
/*                             SetGeocCS()                              */
/************************************************************************/

/**
 * \brief Set the user visible GEOCCS name.
 *
 * This method is the same as the C function OSRSetGeocCS().

 * This method will ensure a GEOCCS node is created as the root,
 * and set the provided name on it.  If used on a GEOGCS coordinate system,
 * the DATUM and PRIMEM nodes from the GEOGCS will be transferred over to
 * the GEOGCS.
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 *
 * @return OGRERR_NONE on success.
 *
 * @since OGR 1.9.0
 */

OGRErr OGRSpatialReference::SetGeocCS( const char * pszName )

{
    OGRErr eErr = OGRERR_NONE;
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    if( d->m_pjType == PJ_TYPE_UNKNOWN )
    {
        d->setPjCRS(proj_create_geocentric_crs(
            d->getPROJContext(),
            pszName, "World Geodetic System 1984", "WGS 84", SRS_WGS84_SEMIMAJOR,
            SRS_WGS84_INVFLATTENING, SRS_PM_GREENWICH, 0.0, SRS_UA_DEGREE,
            CPLAtof(SRS_UA_DEGREE_CONV),
            "Metre", 1.0));
    }
    else if( d->m_pjType == PJ_TYPE_GEOCENTRIC_CRS ) {
        d->setPjCRS(proj_alter_name(
            d->getPROJContext(),d->m_pj_crs, pszName));
    }
    else if( d->m_pjType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
             d->m_pjType == PJ_TYPE_GEOGRAPHIC_3D_CRS )
    {
        auto datum = proj_crs_get_datum(
            d->getPROJContext(), d->m_pj_crs);
#if PROJ_VERSION_MAJOR > 7 || (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 2)
        if( datum == nullptr )
        {
            datum = proj_crs_get_datum_ensemble(d->getPROJContext(), d->m_pj_crs);
        }
#endif
        if( datum == nullptr )
        {
            d->undoDemoteFromBoundCRS();
            return OGRERR_FAILURE;
        }

        auto pj_crs = proj_create_geocentric_crs_from_datum(
            d->getPROJContext(),
            proj_get_name(d->m_pj_crs), datum, nullptr, 0.0);
        d->setPjCRS(pj_crs);

        proj_destroy(datum);
    }
    else
    {
        CPLDebug( "OGR",
                "OGRSpatialReference::SetGeocCS(%s) failed.  "
                "It appears an incompatible object already exists.",
                pszName );
        eErr = OGRERR_FAILURE;
    }
    d->undoDemoteFromBoundCRS();

    return eErr;
}

/************************************************************************/
/*                            OSRSetGeocCS()                            */
/************************************************************************/

/**
 * \brief Set the user visible PROJCS name.
 *
 * This function is the same as OGRSpatialReference::SetGeocCS()
 *
 * @since OGR 1.9.0
 */
OGRErr OSRSetGeocCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGeocCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetGeocCS( pszName );
}

/************************************************************************/
/*                             SetVertCS()                              */
/************************************************************************/

/**
 * \brief Set the user visible VERT_CS name.
 *
 * This method is the same as the C function OSRSetVertCS().

 * This method will ensure a VERT_CS node is created if needed.  If the
 * existing coordinate system is GEOGCS or PROJCS rooted, then it will be
 * turned into a COMPD_CS.
 *
 * @param pszVertCSName the user visible name of the vertical coordinate
 * system. Not used as a key.
 *
 * @param pszVertDatumName the user visible name of the vertical datum.  It
 * is helpful if this matches the EPSG name.
 *
 * @param nVertDatumType the OGC vertical datum type. Ignored
 *
 * @return OGRERR_NONE on success.
 *
 * @since OGR 1.9.0
 */

OGRErr OGRSpatialReference::SetVertCS( const char * pszVertCSName,
                                       const char * pszVertDatumName,
                                       int nVertDatumType )

{
    CPL_IGNORE_RET_VAL(nVertDatumType);

    d->refreshProjObj();

    auto vertCRS =
        proj_create_vertical_crs(d->getPROJContext(), pszVertCSName,
                                     pszVertDatumName, nullptr, 0.0);

/* -------------------------------------------------------------------- */
/*      Handle the case where we want to make a compound coordinate     */
/*      system.                                                         */
/* -------------------------------------------------------------------- */
    if( IsProjected() || IsGeographic() )
    {
        auto compoundCRS = proj_create_compound_crs(d->getPROJContext(),
                                                        nullptr,
                                                        d->m_pj_crs,
                                                        vertCRS);
        proj_destroy(vertCRS);
        d->setPjCRS(compoundCRS);
    }
    else
    {
        d->setPjCRS(vertCRS);
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetVertCS()                            */
/************************************************************************/

/**
 * \brief Setup the vertical coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetVertCS()
 *
 * @since OGR 1.9.0
 */
OGRErr OSRSetVertCS( OGRSpatialReferenceH hSRS,
                     const char * pszVertCSName,
                     const char * pszVertDatumName,
                     int nVertDatumType )

{
    VALIDATE_POINTER1( hSRS, "OSRSetVertCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetVertCS( pszVertCSName, pszVertDatumName, nVertDatumType );
}

/************************************************************************/
/*                           SetCompoundCS()                            */
/************************************************************************/

/**
 * \brief Setup a compound coordinate system.
 *
 * This method is the same as the C function OSRSetCompoundCS().

 * This method is replace the current SRS with a COMPD_CS coordinate system
 * consisting of the passed in horizontal and vertical coordinate systems.
 *
 * @param pszName the name of the compound coordinate system.
 *
 * @param poHorizSRS the horizontal SRS (PROJCS or GEOGCS).
 *
 * @param poVertSRS the vertical SRS (VERT_CS).
 *
 * @return OGRERR_NONE on success.
 */

OGRErr
OGRSpatialReference::SetCompoundCS( const char *pszName,
                                    const OGRSpatialReference *poHorizSRS,
                                    const OGRSpatialReference *poVertSRS )

{
/* -------------------------------------------------------------------- */
/*      Verify these are legal horizontal and vertical coordinate       */
/*      systems.                                                        */
/* -------------------------------------------------------------------- */
    if( !poVertSRS->IsVertical() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SetCompoundCS() fails, vertical component is not VERT_CS." );
        return OGRERR_FAILURE;
    }
    if( !poHorizSRS->IsProjected()
        && !poHorizSRS->IsGeographic() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SetCompoundCS() fails, horizontal component is not PROJCS or GEOGCS." );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Replace with compound srs.                                      */
/* -------------------------------------------------------------------- */
    Clear();

    auto compoundCRS = proj_create_compound_crs(
        d->getPROJContext(), pszName,
        poHorizSRS->d->m_pj_crs,
        poVertSRS->d->m_pj_crs);
    d->setPjCRS(compoundCRS);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetCompoundCS()                          */
/************************************************************************/

/**
 * \brief Setup a compound coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetCompoundCS()
 */
OGRErr OSRSetCompoundCS( OGRSpatialReferenceH hSRS,
                         const char *pszName,
                         OGRSpatialReferenceH hHorizSRS,
                         OGRSpatialReferenceH hVertSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRSetCompoundCS", OGRERR_FAILURE );
    VALIDATE_POINTER1( hHorizSRS, "OSRSetCompoundCS", OGRERR_FAILURE );
    VALIDATE_POINTER1( hVertSRS, "OSRSetCompoundCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetCompoundCS( pszName,
                       ToPointer(hHorizSRS),
                       ToPointer(hVertSRS) );
}

/************************************************************************/
/*                             SetProjCS()                              */
/************************************************************************/

/**
 * \brief Set the user visible PROJCS name.
 *
 * This method is the same as the C function OSRSetProjCS().
 *
 * This method will ensure a PROJCS node is created as the root,
 * and set the provided name on it.  If used on a GEOGCS coordinate system,
 * the GEOGCS node will be demoted to be a child of the new PROJCS root.
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetProjCS( const char * pszName )

{
    d->refreshProjObj();
    if( d->m_pjType == PJ_TYPE_PROJECTED_CRS ) {
        d->setPjCRS(proj_alter_name(
            d->getPROJContext(), d->m_pj_crs, pszName));
    } else {
        auto dummyConv = proj_create_conversion(d->getPROJContext(),
                                                    nullptr, nullptr, nullptr,
                                                    nullptr, nullptr, nullptr,
                                                    0, nullptr);
        auto cs =  proj_create_cartesian_2D_cs(
            d->getPROJContext(), PJ_CART2D_EASTING_NORTHING, nullptr, 0);

        auto projCRS = proj_create_projected_crs(
           d->getPROJContext(), pszName, d->getGeodBaseCRS(), dummyConv, cs);
        proj_destroy(dummyConv);
        proj_destroy(cs);

        d->setPjCRS(projCRS);
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetProjCS()                            */
/************************************************************************/

/**
 * \brief Set the user visible PROJCS name.
 *
 * This function is the same as OGRSpatialReference::SetProjCS()
 */
OGRErr OSRSetProjCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetProjCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetProjCS( pszName );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

/**
 * \brief Set a projection name.
 *
 * This method is the same as the C function OSRSetProjection().
 *
 * @param pszProjection the projection name, which should be selected from
 * the macros in ogr_srs_api.h, such as SRS_PT_TRANSVERSE_MERCATOR.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetProjection( const char * pszProjection )

{
    OGR_SRSNode *poGeogCS = nullptr;

    if( GetRoot() != nullptr && EQUAL(d->m_poRoot->GetValue(), "GEOGCS") )
    {
        poGeogCS = d->m_poRoot;
        d->m_poRoot = nullptr;
    }

    if( !GetAttrNode( "PROJCS" ) )
    {
        SetNode( "PROJCS", "unnamed" );
    }

    const OGRErr eErr = SetNode( "PROJCS|PROJECTION", pszProjection );
    if( eErr != OGRERR_NONE )
        return eErr;

    if( poGeogCS != nullptr )
        d->m_poRoot->InsertChild( poGeogCS, 1 );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetProjection()                        */
/************************************************************************/

/**
 * \brief Set a projection name.
 *
 * This function is the same as OGRSpatialReference::SetProjection()
 */
OGRErr OSRSetProjection( OGRSpatialReferenceH hSRS,
                         const char * pszProjection )

{
    VALIDATE_POINTER1( hSRS, "OSRSetProjection", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetProjection( pszProjection );
}

/************************************************************************/
/*                            SetProjParm()                             */
/************************************************************************/

/**
 * \brief Set a projection parameter value.
 *
 * Adds a new PARAMETER under the PROJCS with the indicated name and value.
 *
 * This method is the same as the C function OSRSetProjParm().
 *
 * Please check http://www.remotesensing.org/geotiff/proj_list pages for
 * legal parameter names for specific projections.
 *
 *
 * @param pszParamName the parameter name, which should be selected from
 * the macros in ogr_srs_api.h, such as SRS_PP_CENTRAL_MERIDIAN.
 *
 * @param dfValue value to assign.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetProjParm( const char * pszParamName,
                                         double dfValue )

{
    OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );

    if( poPROJCS == nullptr )
        return OGRERR_FAILURE;

    char szValue[64] = { '\0' };
    OGRsnPrintDouble( szValue, sizeof(szValue), dfValue );

/* -------------------------------------------------------------------- */
/*      Try to find existing parameter with this name.                  */
/* -------------------------------------------------------------------- */
    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        OGR_SRSNode* poParam = poPROJCS->GetChild( iChild );

        if( EQUAL(poParam->GetValue(), "PARAMETER")
            && poParam->GetChildCount() == 2
            && EQUAL(poParam->GetChild(0)->GetValue(), pszParamName) )
        {
            poParam->GetChild(1)->SetValue( szValue );
            return OGRERR_NONE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise create a new parameter and append.                    */
/* -------------------------------------------------------------------- */
    OGR_SRSNode* poParam = new OGR_SRSNode( "PARAMETER" );
    poParam->AddChild( new OGR_SRSNode( pszParamName ) );
    poParam->AddChild( new OGR_SRSNode( szValue ) );

    poPROJCS->AddChild( poParam );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetProjParm()                           */
/************************************************************************/

/**
 * \brief Set a projection parameter value.
 *
 * This function is the same as OGRSpatialReference::SetProjParm()
 */
OGRErr OSRSetProjParm( OGRSpatialReferenceH hSRS,
                       const char * pszParamName, double dfValue )

{
    VALIDATE_POINTER1( hSRS, "OSRSetProjParm", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetProjParm( pszParamName, dfValue );
}

/************************************************************************/
/*                            FindProjParm()                            */
/************************************************************************/

/**
  * \brief Return the child index of the named projection parameter on
  * its parent PROJCS node.
  *
  * @param pszParameter projection parameter to look for
  * @param poPROJCS projection CS node to look in. If NULL is passed,
  *        the PROJCS node of the SpatialReference object will be searched.
  *
  * @return the child index of the named projection parameter. -1 on failure
  */
int OGRSpatialReference::FindProjParm( const char *pszParameter,
                                       const OGR_SRSNode *poPROJCS ) const

{
    if( poPROJCS == nullptr )
        poPROJCS = GetAttrNode( "PROJCS" );

    if( poPROJCS == nullptr )
        return -1;

/* -------------------------------------------------------------------- */
/*      Search for requested parameter.                                 */
/* -------------------------------------------------------------------- */
    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode *poParameter = poPROJCS->GetChild(iChild);

        if( EQUAL(poParameter->GetValue(), "PARAMETER")
            && poParameter->GetChildCount() >= 2
            && EQUAL(poPROJCS->GetChild(iChild)->GetChild(0)->GetValue(),
                     pszParameter) )
        {
            return iChild;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try similar names, for selected parameters.                     */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszParameter, SRS_PP_LATITUDE_OF_ORIGIN) )
    {
        return FindProjParm( SRS_PP_LATITUDE_OF_CENTER, poPROJCS );
    }

    if( EQUAL(pszParameter, SRS_PP_CENTRAL_MERIDIAN) )
    {
        int iChild = FindProjParm(SRS_PP_LONGITUDE_OF_CENTER, poPROJCS );
        if( iChild == -1 )
            iChild = FindProjParm(SRS_PP_LONGITUDE_OF_ORIGIN, poPROJCS );
        return iChild;
    }

    return -1;
}

/************************************************************************/
/*                            GetProjParm()                             */
/************************************************************************/

/**
 * \brief Fetch a projection parameter value.
 *
 * NOTE: This code should be modified to translate non degree angles into
 * degrees based on the GEOGCS unit.  This has not yet been done.
 *
 * This method is the same as the C function OSRGetProjParm().
 *
 * @param pszName the name of the parameter to fetch, from the set of
 * SRS_PP codes in ogr_srs_api.h.
 *
 * @param dfDefaultValue the value to return if this parameter doesn't exist.
 *
 * @param pnErr place to put error code on failure.  Ignored if NULL.
 *
 * @return value of parameter.
 */

double OGRSpatialReference::GetProjParm( const char * pszName,
                                         double dfDefaultValue,
                                         OGRErr *pnErr ) const

{
    d->refreshProjObj();
    GetRoot(); // force update of d->m_bNodesWKT2

    if( pnErr != nullptr )
        *pnErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Find the desired parameter.                                     */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poPROJCS = GetAttrNode( d->m_bNodesWKT2 ? "CONVERSION" : "PROJCS" );
    if( poPROJCS == nullptr )
    {
        if( pnErr != nullptr )
            *pnErr = OGRERR_FAILURE;
        return dfDefaultValue;
    }

    const int iChild = FindProjParm( pszName, poPROJCS );
    if( iChild == -1 )
    {
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
        if( IsProjected() && GetAxesCount() == 3 )
        {
            OGRSpatialReference* poSRSTmp = Clone();
            poSRSTmp->DemoteTo2D(nullptr);
            const double dfRet = poSRSTmp->GetProjParm(pszName, dfDefaultValue, pnErr);
            delete poSRSTmp;
            return dfRet;
        }
#endif

        if( pnErr != nullptr )
            *pnErr = OGRERR_FAILURE;
        return dfDefaultValue;
    }

    const OGR_SRSNode *poParameter = poPROJCS->GetChild(iChild);
    return CPLAtof(poParameter->GetChild(1)->GetValue());
}

/************************************************************************/
/*                           OSRGetProjParm()                           */
/************************************************************************/

/**
 * \brief Fetch a projection parameter value.
 *
 * This function is the same as OGRSpatialReference::GetProjParm()
 */
double OSRGetProjParm( OGRSpatialReferenceH hSRS, const char *pszName,
                       double dfDefaultValue, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetProjParm", 0 );

    return ToPointer(hSRS)->
        GetProjParm(pszName, dfDefaultValue, pnErr);
}

/************************************************************************/
/*                          GetNormProjParm()                           */
/************************************************************************/

/**
 * \brief Fetch a normalized projection parameter value.
 *
 * This method is the same as GetProjParm() except that the value of
 * the parameter is "normalized" into degrees or meters depending on
 * whether it is linear or angular.
 *
 * This method is the same as the C function OSRGetNormProjParm().
 *
 * @param pszName the name of the parameter to fetch, from the set of
 * SRS_PP codes in ogr_srs_api.h.
 *
 * @param dfDefaultValue the value to return if this parameter doesn't exist.
 *
 * @param pnErr place to put error code on failure.  Ignored if NULL.
 *
 * @return value of parameter.
 */

double OGRSpatialReference::GetNormProjParm( const char * pszName,
                                             double dfDefaultValue,
                                             OGRErr *pnErr ) const

{
    GetNormInfo();

    OGRErr nError = OGRERR_NONE;
    double dfRawResult = GetProjParm( pszName, dfDefaultValue, &nError );
    if( pnErr != nullptr )
        *pnErr = nError;

    // If we got the default just return it unadjusted.
    if( nError != OGRERR_NONE )
        return dfRawResult;

    if( d->dfToDegrees != 1.0 && IsAngularParameter(pszName) )
        dfRawResult *= d->dfToDegrees;

    if( d->dfToMeter != 1.0 && IsLinearParameter( pszName ) )
        return dfRawResult * d->dfToMeter;

    return dfRawResult;
}

/************************************************************************/
/*                         OSRGetNormProjParm()                         */
/************************************************************************/

/**
 * \brief This function is the same as OGRSpatialReference::
 *
 * This function is the same as OGRSpatialReference::GetNormProjParm()
 */
double OSRGetNormProjParm( OGRSpatialReferenceH hSRS, const char *pszName,
                           double dfDefaultValue, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetNormProjParm", 0 );

    return ToPointer(hSRS)->
        GetNormProjParm(pszName, dfDefaultValue, pnErr);
}

/************************************************************************/
/*                          SetNormProjParm()                           */
/************************************************************************/

/**
 * \brief Set a projection parameter with a normalized value.
 *
 * This method is the same as SetProjParm() except that the value of
 * the parameter passed in is assumed to be in "normalized" form (decimal
 * degrees for angular values, meters for linear values.  The values are
 * converted in a form suitable for the GEOGCS and linear units in effect.
 *
 * This method is the same as the C function OSRSetNormProjParm().
 *
 * @param pszName the parameter name, which should be selected from
 * the macros in ogr_srs_api.h, such as SRS_PP_CENTRAL_MERIDIAN.
 *
 * @param dfValue value to assign.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetNormProjParm( const char * pszName,
                                             double dfValue )

{
    GetNormInfo();

    if( d->dfToDegrees != 0.0 &&
        (d->dfToDegrees != 1.0 || d->dfFromGreenwich != 0.0)
        && IsAngularParameter(pszName) )
    {
        dfValue /= d->dfToDegrees;
    }
    else if( d->dfToMeter != 1.0 && d->dfToMeter != 0.0 &&
             IsLinearParameter( pszName ) )
        dfValue /= d->dfToMeter;

    return SetProjParm( pszName, dfValue );
}

/************************************************************************/
/*                         OSRSetNormProjParm()                         */
/************************************************************************/

/**
 * \brief Set a projection parameter with a normalized value.
 *
 * This function is the same as OGRSpatialReference::SetNormProjParm()
 */
OGRErr OSRSetNormProjParm( OGRSpatialReferenceH hSRS,
                           const char * pszParamName, double dfValue )

{
    VALIDATE_POINTER1( hSRS, "OSRSetNormProjParm", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetNormProjParm( pszParamName, dfValue );
}

/************************************************************************/
/*                               SetTM()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetTM( double dfCenterLat, double dfCenterLong,
                                   double dfScale,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_transverse_mercator(d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetTM()                              */
/************************************************************************/

OGRErr OSRSetTM( OGRSpatialReferenceH hSRS,
                 double dfCenterLat, double dfCenterLong,
                 double dfScale,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTM", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetTM(
        dfCenterLat, dfCenterLong,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetTMVariant()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetTMVariant(
    const char *pszVariantName,
    double dfCenterLat, double dfCenterLong,
    double dfScale,
    double dfFalseEasting,
    double dfFalseNorthing )

{
    SetProjection( pszVariantName );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetTMVariant()                           */
/************************************************************************/

OGRErr OSRSetTMVariant( OGRSpatialReferenceH hSRS,
                        const char *pszVariantName,
                        double dfCenterLat, double dfCenterLong,
                        double dfScale,
                        double dfFalseEasting,
                        double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTMVariant", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetTMVariant(
        pszVariantName,
        dfCenterLat, dfCenterLong,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetTMSO()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetTMSO( double dfCenterLat, double dfCenterLong,
                                     double dfScale,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    auto conv =
        proj_create_conversion_transverse_mercator_south_oriented(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);

    const char* pszName = nullptr;
    double dfConvFactor = GetTargetLinearUnits(nullptr, &pszName);
    CPLString osName = pszName ? pszName : "";

    d->refreshProjObj();

    d->demoteFromBoundCRS();

    auto cs = proj_create_cartesian_2D_cs(
        d->getPROJContext(),
        PJ_CART2D_WESTING_SOUTHING,
        !osName.empty() ? osName.c_str() : nullptr, dfConvFactor);
    auto projCRS = proj_create_projected_crs(
        d->getPROJContext(),
        d->getProjCRSName(), d->getGeodBaseCRS(), conv, cs);
    proj_destroy(conv);
    proj_destroy(cs);

    d->setPjCRS(projCRS);

    d->undoDemoteFromBoundCRS();

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetTMSO()                             */
/************************************************************************/

OGRErr OSRSetTMSO( OGRSpatialReferenceH hSRS,
                 double dfCenterLat, double dfCenterLong,
                 double dfScale,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTMSO", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetTMSO(
        dfCenterLat, dfCenterLong,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetTPED()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetTPED( double dfLat1, double dfLong1,
                                     double dfLat2, double dfLong2,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_two_point_equidistant(d->getPROJContext(),
            dfLat1, dfLong1, dfLat2, dfLong2,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                             OSRSetTPED()                             */
/************************************************************************/

OGRErr OSRSetTPED( OGRSpatialReferenceH hSRS,
                   double dfLat1, double dfLong1,
                   double dfLat2, double dfLong2,
                   double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTPED", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetTPED(
        dfLat1, dfLong1, dfLat2, dfLong2,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetTMG()                               */
/************************************************************************/

OGRErr
OGRSpatialReference::SetTMG( double dfCenterLat, double dfCenterLong,
                             double dfFalseEasting, double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_tunisia_mapping_grid(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                             OSRSetTMG()                              */
/************************************************************************/

OGRErr OSRSetTMG( OGRSpatialReferenceH hSRS,
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTMG", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetTMG(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetACEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetACEA( double dfStdP1, double dfStdP2,
                                     double dfCenterLat, double dfCenterLong,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    // Note different order of parameters. The one in PROJ is conformant with
    // EPSG
    return d->replaceConversionAndUnref(
        proj_create_conversion_albers_equal_area(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfStdP1, dfStdP2,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                             OSRSetACEA()                             */
/************************************************************************/

OGRErr OSRSetACEA( OGRSpatialReferenceH hSRS,
                   double dfStdP1, double dfStdP2,
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting,
                   double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetACEA", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetACEA(
        dfStdP1, dfStdP2,
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetAE()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetAE( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_azimuthal_equidistant(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetAE()                              */
/************************************************************************/

OGRErr OSRSetAE( OGRSpatialReferenceH hSRS,
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetACEA", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetAE(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetBonne()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetBonne(
    double dfStdP1, double dfCentralMeridian,
    double dfFalseEasting, double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_bonne(
            d->getPROJContext(),
            dfStdP1, dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                            OSRSetBonne()                             */
/************************************************************************/

OGRErr OSRSetBonne( OGRSpatialReferenceH hSRS,
                    double dfStdP1, double dfCentralMeridian,
                    double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetBonne", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetBonne(
        dfStdP1, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetCEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetCEA( double dfStdP1, double dfCentralMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_lambert_cylindrical_equal_area(
            d->getPROJContext(),
            dfStdP1, dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                             OSRSetCEA()                              */
/************************************************************************/

OGRErr OSRSetCEA( OGRSpatialReferenceH hSRS,
                  double dfStdP1, double dfCentralMeridian,
                  double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetCEA", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetCEA(
        dfStdP1, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetCS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetCS( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_cassini_soldner(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetCS()                              */
/************************************************************************/

OGRErr OSRSetCS( OGRSpatialReferenceH hSRS,
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetCS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetCS(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetEC()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetEC( double dfStdP1, double dfStdP2,
                                   double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    // Note: different order of arguments
    return d->replaceConversionAndUnref(
        proj_create_conversion_equidistant_conic(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong,
            dfStdP1, dfStdP2,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetEC()                              */
/************************************************************************/

OGRErr OSRSetEC( OGRSpatialReferenceH hSRS,
                 double dfStdP1, double dfStdP2,
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEC", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetEC(
        dfStdP1, dfStdP2,
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetEckert()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckert( int nVariation,  // 1-6.
                                       double dfCentralMeridian,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    PJ* conv;
    if( nVariation == 1 )
    {
        conv = proj_create_conversion_eckert_i(
            d->getPROJContext(),
            dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 2 )
    {
        conv = proj_create_conversion_eckert_ii(
            d->getPROJContext(),
            dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 3 )
    {
        conv = proj_create_conversion_eckert_iii(
            d->getPROJContext(),
            dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 4 )
    {
        conv = proj_create_conversion_eckert_iv(
            d->getPROJContext(),
            dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 5 )
    {
        conv = proj_create_conversion_eckert_v(
            d->getPROJContext(),
            dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 6 )
    {
        conv = proj_create_conversion_eckert_vi(
            d->getPROJContext(),
            dfCentralMeridian, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported Eckert variation (%d).",
                  nVariation );
        return OGRERR_UNSUPPORTED_SRS;
    }

    return d->replaceConversionAndUnref(conv);
}

/************************************************************************/
/*                            OSRSetEckert()                            */
/************************************************************************/

OGRErr OSRSetEckert( OGRSpatialReferenceH hSRS,
                     int nVariation,
                     double dfCentralMeridian,
                     double dfFalseEasting,
                     double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEckert", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetEckert(
        nVariation, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetEckertIV()                             */
/*                                                                      */
/*      Deprecated                                                      */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckertIV( double dfCentralMeridian,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    return SetEckert(4, dfCentralMeridian, dfFalseEasting, dfFalseNorthing);
}

/************************************************************************/
/*                           OSRSetEckertIV()                           */
/************************************************************************/

OGRErr OSRSetEckertIV( OGRSpatialReferenceH hSRS,
                       double dfCentralMeridian,
                       double dfFalseEasting,
                       double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEckertIV", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetEckertIV(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetEckertVI()                             */
/*                                                                      */
/*      Deprecated                                                      */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckertVI( double dfCentralMeridian,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    return SetEckert(6, dfCentralMeridian, dfFalseEasting, dfFalseNorthing);
}

/************************************************************************/
/*                           OSRSetEckertVI()                           */
/************************************************************************/

OGRErr OSRSetEckertVI( OGRSpatialReferenceH hSRS,
                       double dfCentralMeridian,
                       double dfFalseEasting,
                       double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEckertVI", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetEckertVI(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                         SetEquirectangular()                         */
/************************************************************************/

OGRErr OGRSpatialReference::SetEquirectangular(
                                   double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    if( dfCenterLat == 0.0 )
    {
        return d->replaceConversionAndUnref(
            proj_create_conversion_equidistant_cylindrical(
                d->getPROJContext(),
                0.0, dfCenterLong,
                dfFalseEasting, dfFalseNorthing,
                nullptr, 0.0, nullptr, 0.0));
    }

    // Non-standard extension with non-zero latitude of origin
    SetProjection( SRS_PT_EQUIRECTANGULAR );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OSRSetEquirectangular()                        */
/************************************************************************/

OGRErr OSRSetEquirectangular( OGRSpatialReferenceH hSRS,
                              double dfCenterLat, double dfCenterLong,
                              double dfFalseEasting,
                              double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEquirectangular", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetEquirectangular(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                         SetEquirectangular2()                        */
/* Generalized form                                                     */
/************************************************************************/

OGRErr OGRSpatialReference::SetEquirectangular2(
                                   double dfCenterLat, double dfCenterLong,
                                   double dfStdParallel1,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    if( dfCenterLat == 0.0 )
    {
        return d->replaceConversionAndUnref(
            proj_create_conversion_equidistant_cylindrical(
                d->getPROJContext(),
                dfStdParallel1, dfCenterLong,
                dfFalseEasting, dfFalseNorthing,
                nullptr, 0.0, nullptr, 0.0));
    }

    // Non-standard extension with non-zero latitude of origin
    SetProjection( SRS_PT_EQUIRECTANGULAR );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdParallel1 );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OSRSetEquirectangular2()                       */
/************************************************************************/

OGRErr OSRSetEquirectangular2( OGRSpatialReferenceH hSRS,
                               double dfCenterLat, double dfCenterLong,
                               double dfStdParallel1,
                               double dfFalseEasting,
                               double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEquirectangular2", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetEquirectangular2(
        dfCenterLat, dfCenterLong,
        dfStdParallel1,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetGS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetGS( double dfCentralMeridian,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_gall(
            d->getPROJContext(),
            dfCentralMeridian,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetGS()                              */
/************************************************************************/

OGRErr OSRSetGS( OGRSpatialReferenceH hSRS,
                 double dfCentralMeridian,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetGS(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetGH()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetGH( double dfCentralMeridian,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_goode_homolosine(
            d->getPROJContext(),
            dfCentralMeridian,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetGH()                              */
/************************************************************************/

OGRErr OSRSetGH( OGRSpatialReferenceH hSRS,
                 double dfCentralMeridian,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGH", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetGH(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetIGH()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetIGH()

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_interrupted_goode_homolosine(
            d->getPROJContext(),
            0.0, 0.0, 0.0,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetIGH()                             */
/************************************************************************/

OGRErr OSRSetIGH( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRSetIGH", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetIGH();
}

/************************************************************************/
/*                              SetGEOS()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetGEOS( double dfCentralMeridian,
                                     double dfSatelliteHeight,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_geostationary_satellite_sweep_y(
            d->getPROJContext(),
            dfCentralMeridian, dfSatelliteHeight,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                              OSRSetGEOS()                             */
/************************************************************************/

OGRErr OSRSetGEOS( OGRSpatialReferenceH hSRS,
                   double dfCentralMeridian,
                   double dfSatelliteHeight,
                   double dfFalseEasting,
                   double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGEOS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetGEOS(
        dfCentralMeridian, dfSatelliteHeight,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                       SetGaussSchreiberTMercator()                   */
/************************************************************************/

OGRErr OGRSpatialReference::SetGaussSchreiberTMercator(
                                   double dfCenterLat, double dfCenterLong,
                                   double dfScale,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_gauss_schreiber_transverse_mercator(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                     OSRSetGaussSchreiberTMercator()                  */
/************************************************************************/

OGRErr OSRSetGaussSchreiberTMercator( OGRSpatialReferenceH hSRS,
                                      double dfCenterLat, double dfCenterLong,
                                      double dfScale,
                                      double dfFalseEasting,
                                      double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGaussSchreiberTMercator", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetGaussSchreiberTMercator(
        dfCenterLat, dfCenterLong, dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetGnomonic()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetGnomonic(
                                   double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_gnomonic(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                           OSRSetGnomonic()                           */
/************************************************************************/

OGRErr OSRSetGnomonic( OGRSpatialReferenceH hSRS,
                       double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting,
                       double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGnomonic", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetGnomonic(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetHOMAC()                              */
/************************************************************************/

/**
 * \brief Set an Hotine Oblique Mercator Azimuth Center projection using
 * azimuth angle.
 *
 * This projection corresponds to EPSG projection method 9815, also
 * sometimes known as hotine oblique mercator (variant B).
 *
 * This method does the same thing as the C function OSRSetHOMAC().
 *
 * @param dfCenterLat Latitude of the projection origin.
 * @param dfCenterLong Longitude of the projection origin.
 * @param dfAzimuth Azimuth, measured clockwise from North, of the projection
 * centerline.
 * @param dfRectToSkew Angle from Rectified to Skew Grid
 * @param dfScale Scale factor applies to the projection origin.
 * @param dfFalseEasting False easting.
 * @param dfFalseNorthing False northing.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetHOMAC( double dfCenterLat, double dfCenterLong,
                                      double dfAzimuth, double dfRectToSkew,
                                      double dfScale,
                                      double dfFalseEasting,
                                      double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_hotine_oblique_mercator_variant_b(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfAzimuth, dfRectToSkew, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                            OSRSetHOMAC()                             */
/************************************************************************/

/**
 * \brief Set an Oblique Mercator projection using azimuth angle.
 *
 * This is the same as the C++ method OGRSpatialReference::SetHOMAC()
 */
OGRErr OSRSetHOMAC( OGRSpatialReferenceH hSRS,
                    double dfCenterLat, double dfCenterLong,
                    double dfAzimuth, double dfRectToSkew,
                    double dfScale,
                    double dfFalseEasting,
                    double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetHOMAC", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetHOMAC(
        dfCenterLat, dfCenterLong,
        dfAzimuth, dfRectToSkew,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetHOM()                               */
/************************************************************************/

/**
 * \brief Set a Hotine Oblique Mercator projection using azimuth angle.
 *
 * This projection corresponds to EPSG projection method 9812, also
 * sometimes known as hotine oblique mercator (variant A)..
 *
 * This method does the same thing as the C function OSRSetHOM().
 *
 * @param dfCenterLat Latitude of the projection origin.
 * @param dfCenterLong Longitude of the projection origin.
 * @param dfAzimuth Azimuth, measured clockwise from North, of the projection
 * centerline.
 * @param dfRectToSkew Angle from Rectified to Skew Grid
 * @param dfScale Scale factor applies to the projection origin.
 * @param dfFalseEasting False easting.
 * @param dfFalseNorthing False northing.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetHOM( double dfCenterLat, double dfCenterLong,
                                    double dfAzimuth, double dfRectToSkew,
                                    double dfScale,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_hotine_oblique_mercator_variant_a(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfAzimuth, dfRectToSkew, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                             OSRSetHOM()                              */
/************************************************************************/
/**
 * \brief Set a Hotine Oblique Mercator projection using azimuth angle.
 *
 * This is the same as the C++ method OGRSpatialReference::SetHOM()
 */
OGRErr OSRSetHOM( OGRSpatialReferenceH hSRS,
                  double dfCenterLat, double dfCenterLong,
                  double dfAzimuth, double dfRectToSkew,
                  double dfScale,
                  double dfFalseEasting,
                  double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetHOM", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetHOM(
        dfCenterLat, dfCenterLong,
        dfAzimuth, dfRectToSkew,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetHOM2PNO()                             */
/************************************************************************/

/**
 * \brief Set a Hotine Oblique Mercator projection using two points on projection
 * centerline.
 *
 * This method does the same thing as the C function OSRSetHOM2PNO().
 *
 * @param dfCenterLat Latitude of the projection origin.
 * @param dfLat1 Latitude of the first point on center line.
 * @param dfLong1 Longitude of the first point on center line.
 * @param dfLat2 Latitude of the second point on center line.
 * @param dfLong2 Longitude of the second point on center line.
 * @param dfScale Scale factor applies to the projection origin.
 * @param dfFalseEasting False easting.
 * @param dfFalseNorthing False northing.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetHOM2PNO( double dfCenterLat,
                                        double dfLat1, double dfLong1,
                                        double dfLat2, double dfLong2,
                                        double dfScale,
                                        double dfFalseEasting,
                                        double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_hotine_oblique_mercator_two_point_natural_origin(
            d->getPROJContext(),
            dfCenterLat, dfLat1, dfLong1, dfLat2, dfLong2, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                           OSRSetHOM2PNO()                            */
/************************************************************************/
/**
 * \brief Set a Hotine Oblique Mercator projection using two points on
 *  projection centerline.
 *
 * This is the same as the C++ method OGRSpatialReference::SetHOM2PNO()
 */
OGRErr OSRSetHOM2PNO( OGRSpatialReferenceH hSRS,
                      double dfCenterLat,
                      double dfLat1, double dfLong1,
                      double dfLat2, double dfLong2,
                      double dfScale,
                      double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetHOM2PNO", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetHOM2PNO(
        dfCenterLat,
        dfLat1, dfLong1,
        dfLat2, dfLong2,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetLOM()                               */
/************************************************************************/

/**
 * \brief Set a Laborde Oblique Mercator projection.
 *
 * @param dfCenterLat Latitude of the projection origin.
 * @param dfCenterLong Longitude of the projection origin.
 * @param dfAzimuth Azimuth, measured clockwise from North, of the projection
 * centerline.
 * @param dfScale Scale factor on the initiali line
 * @param dfFalseEasting False easting.
 * @param dfFalseNorthing False northing.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetLOM( double dfCenterLat, double dfCenterLong,
                                    double dfAzimuth,
                                    double dfScale,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_laborde_oblique_mercator(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfAzimuth, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                            SetIWMPolyconic()                         */
/************************************************************************/

OGRErr OGRSpatialReference::SetIWMPolyconic(
                                double dfLat1, double dfLat2,
                                double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_international_map_world_polyconic(
            d->getPROJContext(),
            dfCenterLong, dfLat1, dfLat2,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
}

/************************************************************************/
/*                          OSRSetIWMPolyconic()                        */
/************************************************************************/

OGRErr OSRSetIWMPolyconic( OGRSpatialReferenceH hSRS,
                           double dfLat1, double dfLat2,
                           double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetIWMPolyconic", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetIWMPolyconic(
        dfLat1, dfLat2, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetKrovak()                              */
/************************************************************************/

/** Krovak east-north projection.
 *
 * Note that dfAzimuth and dfPseudoStdParallel1 are ignored when exporting
 * to PROJ and should be respectively set to 30.28813972222222 and 78.5
 */
OGRErr OGRSpatialReference::SetKrovak( double dfCenterLat, double dfCenterLong,
                                       double dfAzimuth,
                                       double dfPseudoStdParallel1,
                                       double dfScale,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_krovak_north_oriented(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfAzimuth, dfPseudoStdParallel1, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));

}

/************************************************************************/
/*                            OSRSetKrovak()                            */
/************************************************************************/

OGRErr OSRSetKrovak( OGRSpatialReferenceH hSRS,
                     double dfCenterLat, double dfCenterLong,
                     double dfAzimuth, double dfPseudoStdParallel1,
                     double dfScale,
                     double dfFalseEasting,
                     double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetKrovak", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetKrovak(
        dfCenterLat, dfCenterLong,
        dfAzimuth, dfPseudoStdParallel1,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetLAEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetLAEA( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    auto conv =
        proj_create_conversion_lambert_azimuthal_equal_area(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);

    const char* pszName = nullptr;
    double dfConvFactor = GetTargetLinearUnits(nullptr, &pszName);
    CPLString osName = pszName ? pszName : "";

    d->refreshProjObj();

    d->demoteFromBoundCRS();

    auto cs = proj_create_cartesian_2D_cs(
        d->getPROJContext(),
        std::fabs(dfCenterLat - 90) < 1e-10 && dfCenterLong == 0 ?
            PJ_CART2D_NORTH_POLE_EASTING_SOUTH_NORTHING_SOUTH :
        std::fabs(dfCenterLat - -90) < 1e-10 && dfCenterLong == 0 ?
            PJ_CART2D_SOUTH_POLE_EASTING_NORTH_NORTHING_NORTH :
            PJ_CART2D_EASTING_NORTHING,
        !osName.empty() ? osName.c_str() : nullptr, dfConvFactor);
    auto projCRS = proj_create_projected_crs(
        d->getPROJContext(),
        d->getProjCRSName(), d->getGeodBaseCRS(), conv, cs);
    proj_destroy(conv);
    proj_destroy(cs);

    d->setPjCRS(projCRS);

    d->undoDemoteFromBoundCRS();

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetLAEA()                             */
/************************************************************************/

OGRErr OSRSetLAEA( OGRSpatialReferenceH hSRS,
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLAEA", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetLAEA(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetLCC()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetLCC( double dfStdP1, double dfStdP2,
                                    double dfCenterLat, double dfCenterLong,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_lambert_conic_conformal_2sp(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfStdP1, dfStdP2,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                             OSRSetLCC()                              */
/************************************************************************/

OGRErr OSRSetLCC( OGRSpatialReferenceH hSRS,
                  double dfStdP1, double dfStdP2,
                  double dfCenterLat, double dfCenterLong,
                  double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLCC", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetLCC(
        dfStdP1, dfStdP2,
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetLCC1SP()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetLCC1SP( double dfCenterLat, double dfCenterLong,
                                       double dfScale,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_lambert_conic_conformal_1sp(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                            OSRSetLCC1SP()                            */
/************************************************************************/

OGRErr OSRSetLCC1SP( OGRSpatialReferenceH hSRS,
                     double dfCenterLat, double dfCenterLong,
                     double dfScale,
                     double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLCC1SP", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetLCC1SP(
        dfCenterLat, dfCenterLong,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetLCCB()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetLCCB( double dfStdP1, double dfStdP2,
                                     double dfCenterLat, double dfCenterLong,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_lambert_conic_conformal_2sp_belgium(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfStdP1, dfStdP2,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                             OSRSetLCCB()                             */
/************************************************************************/

OGRErr OSRSetLCCB( OGRSpatialReferenceH hSRS,
                   double dfStdP1, double dfStdP2,
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLCCB", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetLCCB(
        dfStdP1, dfStdP2,
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetMC()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetMC( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    (void)dfCenterLat; // ignored

    return d->replaceConversionAndUnref(
        proj_create_conversion_miller_cylindrical(
            d->getPROJContext(),
            dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                              OSRSetMC()                              */
/************************************************************************/

OGRErr OSRSetMC( OGRSpatialReferenceH hSRS,
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetMC", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetMC(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetMercator()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetMercator( double dfCenterLat, double dfCenterLong,
                                         double dfScale,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    if( dfCenterLat != 0.0 && dfScale == 1.0 )
    {
        // Not sure this is correct, but this is how it has been used
        // historically
        return SetMercator2SP(dfCenterLat, 0.0, dfCenterLong, dfFalseEasting,
                              dfFalseNorthing);
    }
    return d->replaceConversionAndUnref(
        proj_create_conversion_mercator_variant_a(
            d->getPROJContext(),
            dfCenterLat, // should be zero
            dfCenterLong,
            dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                           OSRSetMercator()                           */
/************************************************************************/

OGRErr OSRSetMercator( OGRSpatialReferenceH hSRS,
                       double dfCenterLat, double dfCenterLong,
                       double dfScale,
                       double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetMercator", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetMercator(
        dfCenterLat, dfCenterLong,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                           SetMercator2SP()                           */
/************************************************************************/

OGRErr OGRSpatialReference::SetMercator2SP(
    double dfStdP1,
    double dfCenterLat, double dfCenterLong,
    double dfFalseEasting,
    double dfFalseNorthing )

{
    if( dfCenterLat == 0.0 )
    {
        return d->replaceConversionAndUnref(
            proj_create_conversion_mercator_variant_b(
                d->getPROJContext(),
                dfStdP1,
                dfCenterLong,
                dfFalseEasting, dfFalseNorthing,
                nullptr, 0, nullptr, 0));
    }

    SetProjection( SRS_PT_MERCATOR_2SP );

    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRSetMercator2SP()                          */
/************************************************************************/

OGRErr OSRSetMercator2SP( OGRSpatialReferenceH hSRS,
                          double dfStdP1,
                          double dfCenterLat, double dfCenterLong,
                          double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetMercator2SP", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetMercator2SP(
        dfStdP1,
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetMollweide()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetMollweide( double dfCentralMeridian,
                                          double dfFalseEasting,
                                          double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_mollweide(
            d->getPROJContext(),
            dfCentralMeridian,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                          OSRSetMollweide()                           */
/************************************************************************/

OGRErr OSRSetMollweide( OGRSpatialReferenceH hSRS,
                        double dfCentralMeridian,
                        double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetMollweide", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetMollweide(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetNZMG()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetNZMG( double dfCenterLat, double dfCenterLong,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_new_zealand_mapping_grid(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                             OSRSetNZMG()                             */
/************************************************************************/

OGRErr OSRSetNZMG( OGRSpatialReferenceH hSRS,
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetNZMG", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetNZMG(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetOS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetOS( double dfOriginLat, double dfCMeridian,
                                   double dfScale,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_oblique_stereographic(
            d->getPROJContext(),
            dfOriginLat, dfCMeridian, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                              OSRSetOS()                              */
/************************************************************************/

OGRErr OSRSetOS( OGRSpatialReferenceH hSRS,
                 double dfOriginLat, double dfCMeridian,
                 double dfScale,
                 double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetOS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetOS(
        dfOriginLat, dfCMeridian,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                          SetOrthographic()                           */
/************************************************************************/

OGRErr OGRSpatialReference::SetOrthographic(
                                double dfCenterLat, double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_orthographic(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                         OSRSetOrthographic()                         */
/************************************************************************/

OGRErr OSRSetOrthographic( OGRSpatialReferenceH hSRS,
                           double dfCenterLat, double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetOrthographic", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetOrthographic(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetPolyconic()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetPolyconic(
                                double dfCenterLat, double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing )

{
    // note: it seems that by some definitions this should include a
    //       scale_factor parameter.
    return d->replaceConversionAndUnref(
        proj_create_conversion_american_polyconic(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                          OSRSetPolyconic()                           */
/************************************************************************/

OGRErr OSRSetPolyconic( OGRSpatialReferenceH hSRS,
                        double dfCenterLat, double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetPolyconic", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetPolyconic(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetPS()                                */
/************************************************************************/

/** Sets a Polar Stereographic projection.
 *
 * Two variants are possible:
 * - Polar Stereographic Variant A: dfCenterLat must be +/- 90° and is
 *   interpretated as the latitude of origin, combined with the scale factor
 * - Polar Stereographic Variant B: dfCenterLat is different from +/- 90° and
 *   is interpretated as the latitude of true scale. In that situation, dfScale
 *   must be set to 1 (it is ignored in the projection parameters)
 */
OGRErr OGRSpatialReference::SetPS(
                                double dfCenterLat, double dfCenterLong,
                                double dfScale,
                                double dfFalseEasting, double dfFalseNorthing )

{
    PJ* conv;
    if( dfScale == 1.0 && std::abs(std::abs(dfCenterLat)-90) > 1e-8 )
    {
        conv = proj_create_conversion_polar_stereographic_variant_b(
                d->getPROJContext(),
                dfCenterLat, dfCenterLong,
                dfFalseEasting, dfFalseNorthing,
                nullptr, 0, nullptr, 0);
    }
    else
    {
        conv = proj_create_conversion_polar_stereographic_variant_a(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0);
    }

    const char* pszName = nullptr;
    double dfConvFactor = GetTargetLinearUnits(nullptr, &pszName);
    CPLString osName = pszName ? pszName : "";

    d->refreshProjObj();

    d->demoteFromBoundCRS();

    auto cs = proj_create_cartesian_2D_cs(
        d->getPROJContext(),
        dfCenterLat > 0 ? PJ_CART2D_NORTH_POLE_EASTING_SOUTH_NORTHING_SOUTH :
                          PJ_CART2D_SOUTH_POLE_EASTING_NORTH_NORTHING_NORTH,
        !osName.empty() ? osName.c_str() : nullptr, dfConvFactor);
    auto projCRS = proj_create_projected_crs(
        d->getPROJContext(),
        d->getProjCRSName(), d->getGeodBaseCRS(), conv, cs);
    proj_destroy(conv);
    proj_destroy(cs);

    d->setPjCRS(projCRS);

    d->undoDemoteFromBoundCRS();

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetPS()                              */
/************************************************************************/

OGRErr OSRSetPS( OGRSpatialReferenceH hSRS,
                 double dfCenterLat, double dfCenterLong,
                 double dfScale,
                 double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetPS", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetPS(
        dfCenterLat, dfCenterLong,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetRobinson()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetRobinson( double dfCenterLong,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_robinson(
            d->getPROJContext(),
            dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                           OSRSetRobinson()                           */
/************************************************************************/

OGRErr OSRSetRobinson( OGRSpatialReferenceH hSRS,
                        double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetRobinson", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetRobinson(
        dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                           SetSinusoidal()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetSinusoidal( double dfCenterLong,
                                           double dfFalseEasting,
                                           double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_sinusoidal(
            d->getPROJContext(),
            dfCenterLong,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                          OSRSetSinusoidal()                          */
/************************************************************************/

OGRErr OSRSetSinusoidal( OGRSpatialReferenceH hSRS,
                         double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetSinusoidal", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetSinusoidal(
        dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                          SetStereographic()                          */
/************************************************************************/

OGRErr OGRSpatialReference::SetStereographic(
                            double dfOriginLat, double dfCMeridian,
                            double dfScale,
                            double dfFalseEasting,
                            double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_stereographic(
            d->getPROJContext(),
            dfOriginLat, dfCMeridian, dfScale,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                        OSRSetStereographic()                         */
/************************************************************************/

OGRErr OSRSetStereographic( OGRSpatialReferenceH hSRS,
                            double dfOriginLat, double dfCMeridian,
                            double dfScale,
                            double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetStereographic", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetStereographic(
        dfOriginLat, dfCMeridian,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetSOC()                               */
/*                                                                      */
/*      NOTE: This definition isn't really used in practice any more    */
/*      and should be considered deprecated.  It seems that swiss       */
/*      oblique mercator is now define as Hotine_Oblique_Mercator       */
/*      with an azimuth of 90 and a rectified_grid_angle of 90.  See    */
/*      EPSG:2056 and Bug 423.                                          */
/************************************************************************/

OGRErr OGRSpatialReference::SetSOC( double dfLatitudeOfOrigin,
                                    double dfCentralMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_hotine_oblique_mercator_variant_b(
            d->getPROJContext(),
            dfLatitudeOfOrigin, dfCentralMeridian, 90.0, 90.0, 1.0,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0));
#if 0
    SetProjection( SRS_PT_SWISS_OBLIQUE_CYLINDRICAL );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfLatitudeOfOrigin );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                             OSRSetSOC()                              */
/************************************************************************/

OGRErr OSRSetSOC( OGRSpatialReferenceH hSRS,
                  double dfLatitudeOfOrigin, double dfCentralMeridian,
                  double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetSOC", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetSOC(
        dfLatitudeOfOrigin, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetVDG()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetVDG( double dfCMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_van_der_grinten(
            d->getPROJContext(),
            dfCMeridian,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                             OSRSetVDG()                              */
/************************************************************************/

OGRErr OSRSetVDG( OGRSpatialReferenceH hSRS,
                  double dfCentralMeridian,
                  double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetVDG", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetVDG(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetUTM()                               */
/************************************************************************/

/**
 * \brief Set UTM projection definition.
 *
 * This will generate a projection definition with the full set of
 * transverse mercator projection parameters for the given UTM zone.
 * If no PROJCS[] description is set yet, one will be set to look
 * like "UTM Zone %d, {Northern, Southern} Hemisphere".
 *
 * This method is the same as the C function OSRSetUTM().
 *
 * @param nZone UTM zone.
 *
 * @param bNorth TRUE for northern hemisphere, or FALSE for southern
 * hemisphere.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetUTM( int nZone, int bNorth )

{
    if( nZone < 0 || nZone > 60 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid zone: %d", nZone);
        return OGRERR_FAILURE;
    }

    return d->replaceConversionAndUnref(
        proj_create_conversion_utm(d->getPROJContext(), nZone, bNorth));
}

/************************************************************************/
/*                             OSRSetUTM()                              */
/************************************************************************/

/**
 * \brief Set UTM projection definition.
 *
 * This is the same as the C++ method OGRSpatialReference::SetUTM()
 */
OGRErr OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth )

{
    VALIDATE_POINTER1( hSRS, "OSRSetUTM", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetUTM( nZone, bNorth );
}

/************************************************************************/
/*                             GetUTMZone()                             */
/*                                                                      */
/*      Returns zero if it isn't UTM.                                   */
/************************************************************************/

/**
 * \brief Get utm zone information.
 *
 * This is the same as the C function OSRGetUTMZone().
 *
 * In SWIG bindings (Python, Java, etc) the GetUTMZone() method returns a
 * zone which is negative in the southern hemisphere instead of having the
 * pbNorth flag used in the C and C++ interface.
 *
 * @param pbNorth pointer to in to set to TRUE if northern hemisphere, or
 * FALSE if southern.
 *
 * @return UTM zone number or zero if this isn't a UTM definition.
 */

int OGRSpatialReference::GetUTMZone( int * pbNorth ) const

{
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
     if( IsProjected() && GetAxesCount() == 3 )
     {
         OGRSpatialReference* poSRSTmp = Clone();
         poSRSTmp->DemoteTo2D(nullptr);
         const int nZone = poSRSTmp->GetUTMZone(pbNorth);
         delete poSRSTmp;
         return nZone;
     }
#endif

    const char *pszProjection = GetAttrValue( "PROJECTION" );

    if( pszProjection == nullptr
        || !EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
        return 0;

    if( GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) != 0.0 )
        return 0;

    if( GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) != 0.9996 )
        return 0;

    if( fabs(GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 )-500000.0) > 0.001 )
        return 0;

    const double dfFalseNorthing = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0);

    if( dfFalseNorthing != 0.0
        && fabs(dfFalseNorthing-10000000.0) > 0.001 )
        return 0;

    if( pbNorth != nullptr )
        *pbNorth = (dfFalseNorthing == 0);

    const double dfCentralMeridian =
        GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0);
    const double dfZone = (dfCentralMeridian + 186.0) / 6.0;

    if( dfCentralMeridian < -177.00001 ||
        dfCentralMeridian > 177.000001 ||
        CPLIsNan(dfZone) ||
        std::abs(dfZone - static_cast<int>(dfZone) - 0.5 ) > 0.00001 )
      return 0;

    return static_cast<int>(dfZone);
}

/************************************************************************/
/*                           OSRGetUTMZone()                            */
/************************************************************************/

/**
 * \brief Get utm zone information.
 *
 * This is the same as the C++ method OGRSpatialReference::GetUTMZone()
 */
int OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth )

{
    VALIDATE_POINTER1( hSRS, "OSRGetUTMZone", 0 );

    return ToPointer(hSRS)->GetUTMZone( pbNorth );
}

/************************************************************************/
/*                             SetWagner()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetWagner( int nVariation,  // 1--7.
                                       double dfCenterLat,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    PJ* conv;
    if( nVariation == 1 )
    {
        conv = proj_create_conversion_wagner_i(
            d->getPROJContext(),
            0.0, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 2 )
    {
        conv = proj_create_conversion_wagner_ii(
            d->getPROJContext(),
            0.0, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 3 )
    {
        conv = proj_create_conversion_wagner_iii(
            d->getPROJContext(),
            dfCenterLat, 0.0, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 4 )
    {
        conv = proj_create_conversion_wagner_iv(
            d->getPROJContext(),
            0.0, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 5 )
    {
        conv = proj_create_conversion_wagner_v(
            d->getPROJContext(),
            0.0, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 6 )
    {
        conv = proj_create_conversion_wagner_vi(
            d->getPROJContext(),
            0.0, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else if( nVariation == 7 )
    {
        conv = proj_create_conversion_wagner_vii(
            d->getPROJContext(),
            0.0, dfFalseEasting, dfFalseNorthing,
            nullptr, 0.0, nullptr, 0.0);
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported Wagner variation (%d).", nVariation );
        return OGRERR_UNSUPPORTED_SRS;
    }

    return d->replaceConversionAndUnref(conv);
}

/************************************************************************/
/*                            OSRSetWagner()                            */
/************************************************************************/

OGRErr OSRSetWagner( OGRSpatialReferenceH hSRS,
                     int nVariation, double dfCenterLat,
                     double dfFalseEasting,
                     double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetWagner", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetWagner(
        nVariation, dfCenterLat, dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetQSC()                     */
/************************************************************************/

OGRErr OGRSpatialReference::SetQSC( double dfCenterLat, double dfCenterLong )
{
    return d->replaceConversionAndUnref(
        proj_create_conversion_quadrilateralized_spherical_cube(
            d->getPROJContext(),
            dfCenterLat, dfCenterLong,
            0.0, 0.0,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                           OSRSetQSC()                   */
/************************************************************************/

OGRErr OSRSetQSC( OGRSpatialReferenceH hSRS,
                       double dfCenterLat, double dfCenterLong )

{
    VALIDATE_POINTER1( hSRS, "OSRSetQSC", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetQSC(
        dfCenterLat, dfCenterLong );
}

/************************************************************************/
/*                            SetSCH()                     */
/************************************************************************/

OGRErr OGRSpatialReference::SetSCH( double dfPegLat, double dfPegLong,
                                    double dfPegHeading, double dfPegHgt)

{
    return d->replaceConversionAndUnref(
        proj_create_conversion_spherical_cross_track_height(
            d->getPROJContext(),
            dfPegLat, dfPegLong,
            dfPegHeading, dfPegHgt,
            nullptr, 0, nullptr, 0));
}

/************************************************************************/
/*                           OSRSetSCH()                   */
/************************************************************************/

OGRErr OSRSetSCH( OGRSpatialReferenceH hSRS,
                       double dfPegLat, double dfPegLong,
                       double dfPegHeading, double dfPegHgt)

{
    VALIDATE_POINTER1( hSRS, "OSRSetSCH", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetSCH(
        dfPegLat, dfPegLong, dfPegHeading, dfPegHgt );
}


/************************************************************************/
/*                         SetVerticalPerspective()                     */
/************************************************************************/

OGRErr OGRSpatialReference::SetVerticalPerspective( double dfTopoOriginLat,
                                                    double dfTopoOriginLon,
                                                    double dfTopoOriginHeight,
                                                    double dfViewPointHeight,
                                                    double dfFalseEasting,
                                                    double dfFalseNorthing )
{
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
    return d->replaceConversionAndUnref(
        proj_create_conversion_vertical_perspective(
            d->getPROJContext(),
            dfTopoOriginLat, dfTopoOriginLon,
            dfTopoOriginHeight, dfViewPointHeight,
            dfFalseEasting, dfFalseNorthing,
            nullptr, 0, nullptr, 0));
#else
    CPL_IGNORE_RET_VAL(dfTopoOriginHeight); // ignored by PROJ

    OGRSpatialReference oSRS;
    CPLString oProj4String;
    oProj4String.Printf(
        "+proj=nsper +lat_0=%.18g +lon_0=%.18g +h=%.18g +x_0=%.18g +y_0=%.18g",
         dfTopoOriginLat, dfTopoOriginLon, dfViewPointHeight,
         dfFalseEasting, dfFalseNorthing);
    oSRS.SetFromUserInput(oProj4String);
    return d->replaceConversionAndUnref(
        proj_crs_get_coordoperation(d->getPROJContext(), oSRS.d->m_pj_crs));
#endif
}

/************************************************************************/
/*                       OSRSetVerticalPerspective()                    */
/************************************************************************/

OGRErr OSRSetVerticalPerspective( OGRSpatialReferenceH hSRS,
                                  double dfTopoOriginLat,
                                  double dfTopoOriginLon,
                                  double dfTopoOriginHeight,
                                  double dfViewPointHeight,
                                  double dfFalseEasting,
                                  double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetVerticalPerspective", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetVerticalPerspective(
        dfTopoOriginLat, dfTopoOriginLon, dfTopoOriginHeight,
        dfViewPointHeight, dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*             SetDerivedGeogCRSWithPoleRotationGRIBConvention()        */
/************************************************************************/

OGRErr OGRSpatialReference::SetDerivedGeogCRSWithPoleRotationGRIBConvention(
                                                           const char* pszCRSName,
                                                           double dfSouthPoleLat,
                                                           double dfSouthPoleLon,
                                                           double dfAxisRotation )
{
#if PROJ_VERSION_MAJOR > 6 || (PROJ_VERSION_MAJOR == 6 && PROJ_VERSION_MINOR >= 3)
    d->refreshProjObj();
    if( !d->m_pj_crs )
        return OGRERR_FAILURE;
    if( d->m_pjType != PJ_TYPE_GEOGRAPHIC_2D_CRS )
        return OGRERR_FAILURE;
    auto ctxt = d->getPROJContext();
    auto conv = proj_create_conversion_pole_rotation_grib_convention(
        ctxt,
        dfSouthPoleLat,
        dfSouthPoleLon,
        dfAxisRotation,
        nullptr, 0);
    auto cs = proj_crs_get_coordinate_system(ctxt, d->m_pj_crs);
    d->setPjCRS(
        proj_create_derived_geographic_crs(
            ctxt,
            pszCRSName,
            d->m_pj_crs,
            conv,
            cs));
    proj_destroy(conv);
    proj_destroy(cs);
    return OGRERR_NONE;
#else
    (void)pszCRSName;
    SetProjection( "Rotated_pole" );
    SetExtension(
        "PROJCS", "PROJ4",
        CPLSPrintf("+proj=ob_tran +lon_0=%.18g +o_proj=longlat +o_lon_p=%.18g "
                   "+o_lat_p=%.18g +a=%.18g +b=%.18g +to_meter=0.0174532925199 +wktext",
                   dfSouthPoleLon,
                   dfAxisRotation == 0 ? 0 : -dfAxisRotation,
                   dfSouthPoleLat == 0 ? 0 : -dfSouthPoleLat,
                   GetSemiMajor(nullptr),
                   GetSemiMinor(nullptr)));
    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*         SetDerivedGeogCRSWithPoleRotationNetCDFCFConvention()        */
/************************************************************************/

OGRErr OGRSpatialReference::SetDerivedGeogCRSWithPoleRotationNetCDFCFConvention(
                                                           const char* pszCRSName,
                                                           double dfGridNorthPoleLat,
                                                           double dfGridNorthPoleLon,
                                                           double dfNorthPoleGridLon )
{
#if PROJ_VERSION_MAJOR > 8 || (PROJ_VERSION_MAJOR == 8 && PROJ_VERSION_MINOR >= 2)
    d->refreshProjObj();
    if( !d->m_pj_crs )
        return OGRERR_FAILURE;
    if( d->m_pjType != PJ_TYPE_GEOGRAPHIC_2D_CRS )
        return OGRERR_FAILURE;
    auto ctxt = d->getPROJContext();
    auto conv = proj_create_conversion_pole_rotation_netcdf_cf_convention(
        ctxt,
        dfGridNorthPoleLat,
        dfGridNorthPoleLon,
        dfNorthPoleGridLon,
        nullptr, 0);
    auto cs = proj_crs_get_coordinate_system(ctxt, d->m_pj_crs);
    d->setPjCRS(
        proj_create_derived_geographic_crs(
            ctxt,
            pszCRSName,
            d->m_pj_crs,
            conv,
            cs));
    proj_destroy(conv);
    proj_destroy(cs);
    return OGRERR_NONE;
#else
    (void)pszCRSName;
    SetProjection( "Rotated_pole" );
    SetExtension(
        "PROJCS", "PROJ4",
        CPLSPrintf("+proj=ob_tran +o_proj=longlat +lon_0=%.18g +o_lon_p=%.18g "
                   "+o_lat_p=%.18g +a=%.18g +b=%.18g +to_meter=0.0174532925199 +wktext",
                   180.0 + dfGridNorthPoleLon,
                   dfNorthPoleGridLon,
                   dfGridNorthPoleLat,
                   GetSemiMajor(nullptr),
                   GetSemiMinor(nullptr)));
    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                            SetAuthority()                            */
/************************************************************************/

/**
 * \brief Set the authority for a node.
 *
 * This method is the same as the C function OSRSetAuthority().
 *
 * @param pszTargetKey the partial or complete path to the node to
 * set an authority on.  i.e. "PROJCS", "GEOGCS" or "GEOGCS|UNIT".
 *
 * @param pszAuthority authority name, such as "EPSG".
 *
 * @param nCode code for value with this authority.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetAuthority( const char *pszTargetKey,
                                          const char * pszAuthority,
                                          int nCode )

{
    d->refreshProjObj();
    pszTargetKey = d->nullifyTargetKeyIfPossible(pszTargetKey);

    if( pszTargetKey == nullptr )
    {
        if( !d->m_pj_crs )
            return OGRERR_FAILURE;
        CPLString osCode;
        osCode.Printf("%d", nCode);
        d->demoteFromBoundCRS();
        d->setPjCRS(
            proj_alter_id(d->getPROJContext(), d->m_pj_crs,
                              pszAuthority, osCode.c_str()));
        d->undoDemoteFromBoundCRS();
        return OGRERR_NONE;
    }

    d->demoteFromBoundCRS();
    if( d->m_pjType == PJ_TYPE_PROJECTED_CRS && EQUAL(pszTargetKey, "GEOGCS") )
    {
        CPLString osCode;
        osCode.Printf("%d", nCode);
        auto newGeogCRS = proj_alter_id(d->getPROJContext(),
                                            d->getGeodBaseCRS(),
                                            pszAuthority, osCode.c_str());

        auto conv = proj_crs_get_coordoperation(d->getPROJContext(),
                                                    d->m_pj_crs);

        auto projCRS = proj_create_projected_crs(
            d->getPROJContext(),
            d->getProjCRSName(), newGeogCRS, conv, d->getProjCRSCoordSys());

        // Preserve existing id on the PROJCRS
        const char* pszProjCRSAuthName = proj_get_id_auth_name(d->m_pj_crs, 0);
        const char* pszProjCRSCode = proj_get_id_code(d->m_pj_crs, 0);
        if( pszProjCRSAuthName && pszProjCRSCode )
        {
            auto projCRSWithId = proj_alter_id(d->getPROJContext(),
                projCRS, pszProjCRSAuthName, pszProjCRSCode);
            proj_destroy(projCRS);
            projCRS = projCRSWithId;
        }

        proj_destroy(newGeogCRS);
        proj_destroy(conv);

        d->setPjCRS(projCRS);
        d->undoDemoteFromBoundCRS();
        return OGRERR_NONE;
    }
    d->undoDemoteFromBoundCRS();

/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poNode = GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      If there is an existing AUTHORITY child blow it away before     */
/*      trying to set a new one.                                        */
/* -------------------------------------------------------------------- */
    int iOldChild = poNode->FindChild( "AUTHORITY" );
    if( iOldChild != -1 )
        poNode->DestroyChild( iOldChild );

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    char szCode[32] = {};

    snprintf( szCode, sizeof(szCode), "%d", nCode );

    OGR_SRSNode *poAuthNode = new OGR_SRSNode( "AUTHORITY" );
    poAuthNode->AddChild( new OGR_SRSNode( pszAuthority ) );
    poAuthNode->AddChild( new OGR_SRSNode( szCode ) );

    poNode->AddChild( poAuthNode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetAuthority()                           */
/************************************************************************/

/**
 * \brief Set the authority for a node.
 *
 * This function is the same as OGRSpatialReference::SetAuthority().
 */
OGRErr OSRSetAuthority( OGRSpatialReferenceH hSRS,
                        const char *pszTargetKey,
                        const char * pszAuthority,
                        int nCode )

{
    VALIDATE_POINTER1( hSRS, "OSRSetAuthority", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetAuthority( pszTargetKey,
                                                         pszAuthority,
                                                         nCode );
}

/************************************************************************/
/*                          GetAuthorityCode()                          */
/************************************************************************/

/**
 * \brief Get the authority code for a node.
 *
 * This method is used to query an AUTHORITY[] node from within the
 * WKT tree, and fetch the code value.
 *
 * While in theory values may be non-numeric, for the EPSG authority all
 * code values should be integral.
 *
 * This method is the same as the C function OSRGetAuthorityCode().
 *
 * @param pszTargetKey the partial or complete path to the node to
 * get an authority from.  i.e. "PROJCS", "GEOGCS", "GEOGCS|UNIT" or NULL to
 * search for an authority node on the root element.
 *
 * @return value code from authority node, or NULL on failure.  The value
 * returned is internal and should not be freed or modified.
 */

const char *
OGRSpatialReference::GetAuthorityCode( const char *pszTargetKey ) const

{
    d->refreshProjObj();
    const char* pszInputTargetKey = pszTargetKey;
    pszTargetKey = d->nullifyTargetKeyIfPossible(pszTargetKey);
    if( pszTargetKey == nullptr )
    {
        if( !d->m_pj_crs )
        {
            return nullptr;
        }
        d->demoteFromBoundCRS();
        auto ret = proj_get_id_code(d->m_pj_crs, 0);
        if( ret == nullptr && d->m_pjType == PJ_TYPE_PROJECTED_CRS )
        {
            auto ctxt = d->getPROJContext();
            auto cs = proj_crs_get_coordinate_system(ctxt, d->m_pj_crs);
            if( cs )
            {
                const int axisCount = proj_cs_get_axis_count(ctxt, cs);
                proj_destroy(cs);
                if( axisCount == 3 )
                {
                    // This might come from a COMPD_CS with a VERT_DATUM type = 2002
                    // in which case, using the WKT1 representation will enable
                    // us to recover the EPSG code.
                    pszTargetKey = pszInputTargetKey;
                }
            }
        }
        d->undoDemoteFromBoundCRS();
        if( ret != nullptr || pszTargetKey == nullptr )
        {
            return ret;
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poNode = GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch AUTHORITY child if there is one.                          */
/* -------------------------------------------------------------------- */
    if( poNode->FindChild("AUTHORITY") == -1 )
        return nullptr;

    poNode = poNode->GetChild(poNode->FindChild("AUTHORITY"));

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    if( poNode->GetChildCount() < 2 )
        return nullptr;

    return poNode->GetChild(1)->GetValue();
}

/************************************************************************/
/*                          OSRGetAuthorityCode()                       */
/************************************************************************/

/**
 * \brief Get the authority code for a node.
 *
 * This function is the same as OGRSpatialReference::GetAuthorityCode().
 */
const char *OSRGetAuthorityCode( OGRSpatialReferenceH hSRS,
                                 const char *pszTargetKey )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAuthorityCode", nullptr );

    return ToPointer(hSRS)->
        GetAuthorityCode( pszTargetKey );
}

/************************************************************************/
/*                          GetAuthorityName()                          */
/************************************************************************/

/**
 * \brief Get the authority name for a node.
 *
 * This method is used to query an AUTHORITY[] node from within the
 * WKT tree, and fetch the authority name value.
 *
 * The most common authority is "EPSG".
 *
 * This method is the same as the C function OSRGetAuthorityName().
 *
 * @param pszTargetKey the partial or complete path to the node to
 * get an authority from.  i.e. "PROJCS", "GEOGCS", "GEOGCS|UNIT" or NULL to
 * search for an authority node on the root element.
 *
 * @return value code from authority node, or NULL on failure. The value
 * returned is internal and should not be freed or modified.
 */

const char *
OGRSpatialReference::GetAuthorityName( const char *pszTargetKey ) const

{
    d->refreshProjObj();
    const char* pszInputTargetKey = pszTargetKey;
    pszTargetKey = d->nullifyTargetKeyIfPossible(pszTargetKey);
    if( pszTargetKey == nullptr )
    {
        if( !d->m_pj_crs )
        {
            return nullptr;
        }
        d->demoteFromBoundCRS();
        auto ret = proj_get_id_auth_name(d->m_pj_crs, 0);
        if( ret == nullptr && d->m_pjType == PJ_TYPE_PROJECTED_CRS )
        {
            auto ctxt = d->getPROJContext();
            auto cs = proj_crs_get_coordinate_system(ctxt, d->m_pj_crs);
            if( cs )
            {
                const int axisCount = proj_cs_get_axis_count(ctxt, cs);
                proj_destroy(cs);
                if( axisCount == 3 )
                {
                    // This might come from a COMPD_CS with a VERT_DATUM type = 2002
                    // in which case, using the WKT1 representation will enable
                    // us to recover the EPSG code.
                    pszTargetKey = pszInputTargetKey;
                }
            }
        }
        d->undoDemoteFromBoundCRS();
        if( ret != nullptr || pszTargetKey == nullptr )
        {
            return ret;
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poNode = GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch AUTHORITY child if there is one.                          */
/* -------------------------------------------------------------------- */
    if( poNode->FindChild("AUTHORITY") == -1 )
        return nullptr;

    poNode = poNode->GetChild(poNode->FindChild("AUTHORITY"));

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    if( poNode->GetChildCount() < 2 )
        return nullptr;

    return poNode->GetChild(0)->GetValue();
}

/************************************************************************/
/*                        OSRGetAuthorityName()                         */
/************************************************************************/

/**
 * \brief Get the authority name for a node.
 *
 * This function is the same as OGRSpatialReference::GetAuthorityName().
 */
const char *OSRGetAuthorityName( OGRSpatialReferenceH hSRS,
                                 const char *pszTargetKey )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAuthorityName", nullptr );

    return ToPointer(hSRS)->
        GetAuthorityName( pszTargetKey );
}

/************************************************************************/
/*                           StripVertical()                            */
/************************************************************************/

/**
 * \brief Convert a compound cs into a horizontal CS.
 *
 * If this SRS is of type COMPD_CS[] then the vertical CS and the root COMPD_CS
 * nodes are stripped resulting and only the horizontal coordinate system
 * portion remains (normally PROJCS, GEOGCS or LOCAL_CS).
 *
 * If this is not a compound coordinate system then nothing is changed.
 *
 * @since OGR 1.8.0
 */

OGRErr OGRSpatialReference::StripVertical()

{
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    if( !d->m_pj_crs || d->m_pjType != PJ_TYPE_COMPOUND_CRS )
    {
        d->undoDemoteFromBoundCRS();
        return OGRERR_NONE;
    }
    auto horizCRS =
        proj_crs_get_sub_crs(d->getPROJContext(), d->m_pj_crs, 0);
    if( !horizCRS )
    {
        d->undoDemoteFromBoundCRS();
        return OGRERR_FAILURE;
    }

    bool reuseExistingBoundCRS = false;
    if( d->m_pj_bound_crs_target )
    {
        auto type = proj_get_type(d->m_pj_bound_crs_target);
        reuseExistingBoundCRS =
            type == PJ_TYPE_GEOCENTRIC_CRS ||
            type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
            type == PJ_TYPE_GEOGRAPHIC_3D_CRS;
    }

    if( reuseExistingBoundCRS )
    {
        auto newBoundCRS =
            proj_crs_create_bound_crs(
                d->getPROJContext(), horizCRS,
                d->m_pj_bound_crs_target, d->m_pj_bound_crs_co);
        proj_destroy(horizCRS);
        d->undoDemoteFromBoundCRS();
        d->setPjCRS(newBoundCRS);
    }
    else
    {
        d->undoDemoteFromBoundCRS();
        d->setPjCRS(horizCRS);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                   StripTOWGS84IfKnownDatumAndAllowed()               */
/************************************************************************/

/**
 * \brief Remove TOWGS84 information if the CRS has a known horizontal datum
 *        and this is allowed by the user.
 *
 * The default behavior is to remove TOWGS84 information if the CRS has a
 * known horizontal datum. This can be disabled by setting the
 * OSR_STRIP_TOWGS84 configuration option to NO.
 *
 * @return true if TOWGS84 has been removed.
 * @since OGR 3.1.0
 */

bool OGRSpatialReference::StripTOWGS84IfKnownDatumAndAllowed()
{
    if( CPLTestBool(CPLGetConfigOption("OSR_STRIP_TOWGS84", "YES")) )
    {
        if( StripTOWGS84IfKnownDatum() )
        {
            CPLDebug("OSR", "TOWGS84 information has been removed. "
                     "It can be kept by setting the OSR_STRIP_TOWGS84 "
                     "configuration option to NO");
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                      StripTOWGS84IfKnownDatum()                      */
/************************************************************************/

/**
 * \brief Remove TOWGS84 information if the CRS has a known horizontal datum
 *
 * @return true if TOWGS84 has been removed.
 * @since OGR 3.1.0
 */

bool OGRSpatialReference::StripTOWGS84IfKnownDatum()

{
    d->refreshProjObj();
    if( !d->m_pj_crs || d->m_pjType != PJ_TYPE_BOUND_CRS )
    {
        return false;
    }
    auto ctxt = d->getPROJContext();
    auto baseCRS = proj_get_source_crs(ctxt, d->m_pj_crs);
    if( proj_get_type(baseCRS) == PJ_TYPE_COMPOUND_CRS )
    {
        proj_destroy(baseCRS);
        return false;
    }

    // Known base CRS code ? Return base CRS
    const char* pszCode = proj_get_id_code(baseCRS, 0);
    if( pszCode )
    {
        d->setPjCRS(baseCRS);
        return true;
    }

    auto datum = proj_crs_get_datum(ctxt, baseCRS);
#if PROJ_VERSION_MAJOR > 7 || (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 2)
    if( datum == nullptr )
    {
        datum = proj_crs_get_datum_ensemble(ctxt, baseCRS);
    }
#endif
    if( !datum )
    {
        proj_destroy(baseCRS);
        return false;
    }

    // Known datum code ? Return base CRS
    pszCode = proj_get_id_code(datum, 0);
    if( pszCode )
    {
        proj_destroy(datum);
        d->setPjCRS(baseCRS);
        return true;
    }

    const char* name = proj_get_name(datum);
    if( EQUAL(name, "unknown") )
    {
        proj_destroy(datum);
        proj_destroy(baseCRS);
        return false;
    }
    const PJ_TYPE type = PJ_TYPE_GEODETIC_REFERENCE_FRAME;
    PJ_OBJ_LIST* list = proj_create_from_name(ctxt, nullptr,
                                              name,
                                              &type, 1,
                                              false,
                                              1,
                                              nullptr);

    bool knownDatumName = false;
    if( list )
    {
        if( proj_list_get_count(list) == 1 )
        {
            knownDatumName = true;
        }
        proj_list_destroy(list);
    }

    proj_destroy(datum);
    if( knownDatumName )
    {
        d->setPjCRS(baseCRS);
        return true;
    }
    proj_destroy(baseCRS);
    return false;
}

/************************************************************************/
/*                             IsCompound()                             */
/************************************************************************/

/**
 * \brief Check if coordinate system is compound.
 *
 * This method is the same as the C function OSRIsCompound().
 *
 * @return TRUE if this is rooted with a COMPD_CS node.
 */

int OGRSpatialReference::IsCompound() const

{
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    bool isCompound = d->m_pjType == PJ_TYPE_COMPOUND_CRS;
    d->undoDemoteFromBoundCRS();
    return isCompound;
}

/************************************************************************/
/*                           OSRIsCompound()                            */
/************************************************************************/

/**
 * \brief Check if the coordinate system is compound.
 *
 * This function is the same as OGRSpatialReference::IsCompound().
 */
int OSRIsCompound( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsCompound", 0 );

    return ToPointer(hSRS)->IsCompound();
}

/************************************************************************/
/*                            IsProjected()                             */
/************************************************************************/

/**
 * \brief Check if projected coordinate system.
 *
 * This method is the same as the C function OSRIsProjected().
 *
 * @return TRUE if this contains a PROJCS node indicating a it is a
 * projected coordinate system. Also if it is a CompoundCRS made of a
 * ProjectedCRS
 */

int OGRSpatialReference::IsProjected() const

{
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    bool isProjected = d->m_pjType == PJ_TYPE_PROJECTED_CRS;
    if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        auto horizCRS =
            proj_crs_get_sub_crs(d->getPROJContext(), d->m_pj_crs, 0);
        if( horizCRS )
        {
            auto horizCRSType = proj_get_type(horizCRS);
            isProjected = horizCRSType == PJ_TYPE_PROJECTED_CRS;
            if( horizCRSType == PJ_TYPE_BOUND_CRS )
            {
                auto base = proj_get_source_crs(d->getPROJContext(), horizCRS);
                if( base )
                {
                    isProjected =
                        proj_get_type(base) == PJ_TYPE_PROJECTED_CRS;
                    proj_destroy(base);
                }
            }
            proj_destroy(horizCRS);
        }
    }
    d->undoDemoteFromBoundCRS();
    return isProjected;
}

/************************************************************************/
/*                           OSRIsProjected()                           */
/************************************************************************/
/**
 * \brief Check if projected coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsProjected().
 */
int OSRIsProjected( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsProjected", 0 );

    return ToPointer(hSRS)->IsProjected();
}

/************************************************************************/
/*                            IsGeocentric()                            */
/************************************************************************/

/**
 * \brief Check if geocentric coordinate system.
 *
 * This method is the same as the C function OSRIsGeocentric().
 *
 * @return TRUE if this contains a GEOCCS node indicating a it is a
 * geocentric coordinate system.
 *
 * @since OGR 1.9.0
 */

int OGRSpatialReference::IsGeocentric() const

{
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    bool isGeocentric = d->m_pjType == PJ_TYPE_GEOCENTRIC_CRS;
    d->undoDemoteFromBoundCRS();
    return isGeocentric;
}

/************************************************************************/
/*                           OSRIsGeocentric()                          */
/************************************************************************/
/**
 * \brief Check if geocentric coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsGeocentric().
 *
 * @since OGR 1.9.0
 */
int OSRIsGeocentric( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsGeocentric", 0 );

    return ToPointer(hSRS)->IsGeocentric();
}

/************************************************************************/
/*                            IsEmpty()                                 */
/************************************************************************/

/**
 * \brief Return if the SRS is not set.
 */

bool OGRSpatialReference::IsEmpty() const
{
    d->refreshProjObj();
    return d->m_pj_crs == nullptr;
}

/************************************************************************/
/*                            IsGeographic()                            */
/************************************************************************/

/**
 * \brief Check if geographic coordinate system.
 *
 * This method is the same as the C function OSRIsGeographic().
 *
 * @return TRUE if this spatial reference is geographic ... that is the
 * root is a GEOGCS node. Also if it is a CompoundCRS made of a
 * GeographicCRS
 */

int OGRSpatialReference::IsGeographic() const

{
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    bool isGeog = d->m_pjType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
                       d->m_pjType == PJ_TYPE_GEOGRAPHIC_3D_CRS;
    if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        auto horizCRS =
            proj_crs_get_sub_crs(d->getPROJContext(), d->m_pj_crs, 0);
        if( horizCRS )
        {
            auto horizCRSType = proj_get_type(horizCRS);
            isGeog = horizCRSType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
                     horizCRSType == PJ_TYPE_GEOGRAPHIC_3D_CRS;
            if( horizCRSType == PJ_TYPE_BOUND_CRS )
            {
                auto base = proj_get_source_crs(d->getPROJContext(), horizCRS);
                if( base )
                {
                    horizCRSType = proj_get_type(base);
                    isGeog = horizCRSType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
                             horizCRSType == PJ_TYPE_GEOGRAPHIC_3D_CRS;
                    proj_destroy(base);
                }
            }
            proj_destroy(horizCRS);
        }
    }
    d->undoDemoteFromBoundCRS();
    return isGeog;
}

/************************************************************************/
/*                          OSRIsGeographic()                           */
/************************************************************************/
/**
 * \brief Check if geographic coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsGeographic().
 */
int OSRIsGeographic( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsGeographic", 0 );

    return ToPointer(hSRS)->IsGeographic();
}


/************************************************************************/
/*                      IsDerivedGeographic()                           */
/************************************************************************/

/**
 * \brief Check if the CRS is a derived geographic coordinate system.
 * (for example a rotated long/lat grid)
 *
 * This method is the same as the C function OSRIsDerivedGeographic().
 *
 * @since GDAL 3.1.0 and PROJ 6.3.0
 */

int OGRSpatialReference::IsDerivedGeographic() const

{
    d->refreshProjObj();
    d->demoteFromBoundCRS();
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
    const bool isGeog = d->m_pjType == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
                        d->m_pjType == PJ_TYPE_GEOGRAPHIC_3D_CRS;
    const bool isDerivedGeographic = isGeog &&
                        proj_is_derived_crs(d->getPROJContext(), d->m_pj_crs);
    d->undoDemoteFromBoundCRS();
    return isDerivedGeographic ? TRUE : FALSE;
#else
    d->undoDemoteFromBoundCRS();
    return FALSE;
#endif
}

/************************************************************************/
/*                      OSRIsDerivedGeographic()                        */
/************************************************************************/
/**
 * \brief Check if derived geographic coordinate system.
 * (for example a rotated long/lat grid)
 *
 * This function is the same as OGRSpatialReference::IsDerivedGeographic().
 */
int OSRIsDerivedGeographic( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsDerivedGeographic", 0 );

    return ToPointer(hSRS)->IsDerivedGeographic();
}

/************************************************************************/
/*                              IsLocal()                               */
/************************************************************************/

/**
 * \brief Check if local coordinate system.
 *
 * This method is the same as the C function OSRIsLocal().
 *
 * @return TRUE if this spatial reference is local ... that is the
 * root is a LOCAL_CS node.
 */

int OGRSpatialReference::IsLocal() const

{
    d->refreshProjObj();
    return d->m_pjType == PJ_TYPE_ENGINEERING_CRS;
}

/************************************************************************/
/*                          OSRIsLocal()                                */
/************************************************************************/
/**
 * \brief Check if local coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsLocal().
 */
int OSRIsLocal( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsLocal", 0 );

    return ToPointer(hSRS)->IsLocal();
}

/************************************************************************/
/*                            IsVertical()                              */
/************************************************************************/

/**
 * \brief Check if vertical coordinate system.
 *
 * This method is the same as the C function OSRIsVertical().
 *
 * @return TRUE if this contains a VERT_CS node indicating a it is a
 * vertical coordinate system. Also if it is a CompoundCRS made of a
 * VerticalCRS
 *
 * @since OGR 1.8.0
 */

int OGRSpatialReference::IsVertical() const

{
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    bool isVertical = d->m_pjType == PJ_TYPE_VERTICAL_CRS;
    if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        auto vertCRS =
            proj_crs_get_sub_crs(d->getPROJContext(), d->m_pj_crs, 1);
        if( vertCRS )
        {
            const auto vertCRSType = proj_get_type(vertCRS);
            isVertical = vertCRSType == PJ_TYPE_VERTICAL_CRS;
            if( vertCRSType == PJ_TYPE_BOUND_CRS )
            {
                auto base = proj_get_source_crs(d->getPROJContext(), vertCRS);
                if( base )
                {
                    isVertical =
                        proj_get_type(base) == PJ_TYPE_VERTICAL_CRS;
                    proj_destroy(base);
                }
            }
            proj_destroy(vertCRS);
        }
    }
    d->undoDemoteFromBoundCRS();
    return isVertical;
}

/************************************************************************/
/*                           OSRIsVertical()                            */
/************************************************************************/
/**
 * \brief Check if vertical coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsVertical().
 *
 * @since OGR 1.8.0
 */
int OSRIsVertical( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsVertical", 0 );

    return ToPointer(hSRS)->IsVertical();
}

/************************************************************************/
/*                            IsDynamic()                               */
/************************************************************************/

/**
 * \brief Check if a CRS is a dynamic CRS.
 *
 * A dynamic CRS relies on a dynamic datum, that is a datum that is not
 * plate-fixed.
 *
 * This method is the same as the C function OSRIsDynamic().
 *
 * @return true if the CRS is dynamic
 *
 * @since OGR 3.4.0
 */

bool OGRSpatialReference::IsDynamic() const

{
    bool isDynamic = false;
    d->refreshProjObj();
    d->demoteFromBoundCRS();
    auto ctxt = d->getPROJContext();
    PJ* horiz = nullptr;
    if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        horiz = proj_crs_get_sub_crs(ctxt, d->m_pj_crs, 0);
    }
    else if( d->m_pj_crs )
    {
        horiz = proj_clone(ctxt, d->m_pj_crs);
    }
    if( horiz && proj_get_type(horiz) == PJ_TYPE_BOUND_CRS )
    {
        auto baseCRS = proj_get_source_crs(ctxt, horiz);
        if( baseCRS )
        {
            proj_destroy(horiz);
            horiz = baseCRS;
        }
    }
    auto datum = horiz ? proj_crs_get_datum(ctxt, horiz) : nullptr;
    if( datum )
    {
        const auto type = proj_get_type(datum);
        isDynamic = type == PJ_TYPE_DYNAMIC_GEODETIC_REFERENCE_FRAME ||
                    type == PJ_TYPE_DYNAMIC_VERTICAL_REFERENCE_FRAME;
        if( !isDynamic )
        {
            const char* auth_name = proj_get_id_auth_name(datum, 0);
            const char* code = proj_get_id_code(datum, 0);
            if( auth_name && code && EQUAL(auth_name, "EPSG") && EQUAL(code, "6326") )
            {
                isDynamic = true;
            }
        }
        proj_destroy(datum);
    }
#if PROJ_VERSION_MAJOR > 7 || (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 2)
    else
    {
        auto ensemble = horiz ? proj_crs_get_datum_ensemble(ctxt, horiz) : nullptr;
        if( ensemble )
        {
            auto member = proj_datum_ensemble_get_member(ctxt, ensemble, 0);
            if( member )
            {
                const auto type = proj_get_type(member);
                isDynamic = type == PJ_TYPE_DYNAMIC_GEODETIC_REFERENCE_FRAME ||
                            type == PJ_TYPE_DYNAMIC_VERTICAL_REFERENCE_FRAME;
                proj_destroy(member);
            }
            proj_destroy(ensemble);
        }
    }
#endif
    proj_destroy(horiz);
    d->undoDemoteFromBoundCRS();
    return isDynamic;
}

/************************************************************************/
/*                           OSRIsDynamic()                             */
/************************************************************************/
/**
 * \brief Check if a CRS is a dynamic CRS.
 *
 * A dynamic CRS relies on a dynamic datum, that is a datum that is not
 * plate-fixed.
 *
 * This function is the same as OGRSpatialReference::IsDynamic().
 *
 * @since OGR 3.4.0
 */
int OSRIsDynamic( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsDynamic", 0 );

    return ToPointer(hSRS)->IsDynamic();
}

/************************************************************************/
/*                            CloneGeogCS()                             */
/************************************************************************/

/**
 * \brief Make a duplicate of the GEOGCS node of this OGRSpatialReference object.
 *
 * @return a new SRS, which becomes the responsibility of the caller.
 */
OGRSpatialReference *OGRSpatialReference::CloneGeogCS() const

{
    d->refreshProjObj();
    if( d->m_pj_crs )
    {
        if( d->m_pjType == PJ_TYPE_ENGINEERING_CRS )
            return nullptr;

        auto geodCRS = proj_crs_get_geodetic_crs(
            d->getPROJContext(), d->m_pj_crs);
        if( geodCRS )
        {
            OGRSpatialReference * poNewSRS = new OGRSpatialReference();
            if( d->m_pjType == PJ_TYPE_BOUND_CRS )
            {
                PJ* hub_crs = proj_get_target_crs(
                    d->getPROJContext(), d->m_pj_crs);
                PJ* co = proj_crs_get_coordoperation(
                    d->getPROJContext(), d->m_pj_crs);
                auto temp = proj_crs_create_bound_crs(
                    d->getPROJContext(), geodCRS, hub_crs, co);
                proj_destroy(geodCRS);
                geodCRS = temp;
                proj_destroy(hub_crs);
                proj_destroy(co);
            }

/* -------------------------------------------------------------------- */
/*      We have to reconstruct the GEOGCS node for geocentric           */
/*      coordinate systems.                                             */
/* -------------------------------------------------------------------- */
            if( proj_get_type(geodCRS) == PJ_TYPE_GEOCENTRIC_CRS )
            {
                auto datum = proj_crs_get_datum(
                    d->getPROJContext(), geodCRS);
#if PROJ_VERSION_MAJOR > 7 || (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 2)
                if( datum == nullptr )
                {
                    datum = proj_crs_get_datum_ensemble(d->getPROJContext(), geodCRS);
                }
#endif
                if( datum )
                {
                    auto cs = proj_create_ellipsoidal_2D_cs(
                        d->getPROJContext(),PJ_ELLPS2D_LATITUDE_LONGITUDE, nullptr, 0);
                    auto temp = proj_create_geographic_crs_from_datum(
                         d->getPROJContext(),"unnamed", datum, cs);
                    proj_destroy(datum);
                    proj_destroy(cs);
                    proj_destroy(geodCRS);
                    geodCRS = temp;
                }
            }

            poNewSRS->d->setPjCRS(geodCRS);
            if( d->m_axisMappingStrategy == OAMS_TRADITIONAL_GIS_ORDER )
                poNewSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            return poNewSRS;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                           OSRCloneGeogCS()                           */
/************************************************************************/
/**
 * \brief Make a duplicate of the GEOGCS node of this OGRSpatialReference object.
 *
 * This function is the same as OGRSpatialReference::CloneGeogCS().
 */
OGRSpatialReferenceH CPL_STDCALL OSRCloneGeogCS( OGRSpatialReferenceH hSource )

{
    VALIDATE_POINTER1( hSource, "OSRCloneGeogCS", nullptr );

    return ToHandle(
        ToPointer(hSource)->CloneGeogCS() );
}

/************************************************************************/
/*                            IsSameGeogCS()                            */
/************************************************************************/

/**
 * \brief Do the GeogCS'es match?
 *
 * This method is the same as the C function OSRIsSameGeogCS().
 *
 * @param poOther the SRS being compared against.
 *
 * @return TRUE if they are the same or FALSE otherwise.
 */

int OGRSpatialReference::IsSameGeogCS( const OGRSpatialReference *poOther ) const

{
    return IsSameGeogCS( poOther, nullptr );
}

/**
 * \brief Do the GeogCS'es match?
 *
 * This method is the same as the C function OSRIsSameGeogCS().
 *
 * @param poOther the SRS being compared against.
 * @param papszOptions options. ignored
 *
 * @return TRUE if they are the same or FALSE otherwise.
 */

int OGRSpatialReference::IsSameGeogCS( const OGRSpatialReference *poOther,
                                       const char* const * papszOptions ) const

{
    CPL_IGNORE_RET_VAL(papszOptions);

    d->refreshProjObj();
    poOther->d->refreshProjObj();
    if( !d->m_pj_crs || !poOther->d->m_pj_crs )
        return FALSE;
    if( d->m_pjType == PJ_TYPE_ENGINEERING_CRS ||
        d->m_pjType == PJ_TYPE_VERTICAL_CRS ||
        poOther->d->m_pjType == PJ_TYPE_ENGINEERING_CRS ||
        poOther->d->m_pjType == PJ_TYPE_VERTICAL_CRS )
    {
        return FALSE;
    }

    auto geodCRS = proj_crs_get_geodetic_crs(d->getPROJContext(),
                                                 d->m_pj_crs);
    auto otherGeodCRS = proj_crs_get_geodetic_crs(d->getPROJContext(),
                                                      poOther->d->m_pj_crs);
    if( !geodCRS || !otherGeodCRS )
    {
        proj_destroy(geodCRS);
        proj_destroy(otherGeodCRS);
        return FALSE;
    }

    int ret = proj_is_equivalent_to(geodCRS, otherGeodCRS,
                                PJ_COMP_EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS);

    proj_destroy(geodCRS);
    proj_destroy(otherGeodCRS);
    return ret;
}

/************************************************************************/
/*                          OSRIsSameGeogCS()                           */
/************************************************************************/

/**
 * \brief Do the GeogCS'es match?
 *
 * This function is the same as OGRSpatialReference::IsSameGeogCS().
 */
int OSRIsSameGeogCS( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    VALIDATE_POINTER1( hSRS1, "OSRIsSameGeogCS", 0 );
    VALIDATE_POINTER1( hSRS2, "OSRIsSameGeogCS", 0 );

    return ToPointer(hSRS1)->IsSameGeogCS(
        ToPointer(hSRS2) );
}

/************************************************************************/
/*                            IsSameVertCS()                            */
/************************************************************************/

/**
 * \brief Do the VertCS'es match?
 *
 * This method is the same as the C function OSRIsSameVertCS().
 *
 * @param poOther the SRS being compared against.
 *
 * @return TRUE if they are the same or FALSE otherwise.
 */

int OGRSpatialReference::IsSameVertCS( const OGRSpatialReference *poOther ) const

{
/* -------------------------------------------------------------------- */
/*      Does the datum name match?                                      */
/* -------------------------------------------------------------------- */
    const char *pszThisValue = this->GetAttrValue( "VERT_DATUM" );
    const char *pszOtherValue = poOther->GetAttrValue( "VERT_DATUM" );

    if( pszThisValue == nullptr || pszOtherValue == nullptr
        || !EQUAL(pszThisValue, pszOtherValue) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do the units match?                                             */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "VERT_CS|UNIT", 1 );
    if( pszThisValue == nullptr )
        pszThisValue = "1.0";

    pszOtherValue = poOther->GetAttrValue( "VERT_CS|UNIT", 1 );
    if( pszOtherValue == nullptr )
        pszOtherValue = "1.0";

    if( std::abs(CPLAtof(pszOtherValue) - CPLAtof(pszThisValue)) > 0.00000001 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          OSRIsSameVertCS()                           */
/************************************************************************/

/**
 * \brief Do the VertCS'es match?
 *
 * This function is the same as OGRSpatialReference::IsSameVertCS().
 */
int OSRIsSameVertCS( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    VALIDATE_POINTER1( hSRS1, "OSRIsSameVertCS", 0 );
    VALIDATE_POINTER1( hSRS2, "OSRIsSameVertCS", 0 );

    return ToPointer(hSRS1)->IsSameVertCS(
        ToPointer(hSRS2) );
}

/************************************************************************/
/*                               IsSame()                               */
/************************************************************************/

/**
 * \brief Do these two spatial references describe the same system ?
 *
 * @param poOtherSRS the SRS being compared to.
 *
 * @return TRUE if equivalent or FALSE otherwise.
 */

int OGRSpatialReference::IsSame( const OGRSpatialReference * poOtherSRS ) const

{
    return IsSame(poOtherSRS, nullptr);
}


/**
 * \brief Do these two spatial references describe the same system ?
 *
 * This also takes into account the data axis to CRS axis mapping by default
 *
 * @param poOtherSRS the SRS being compared to.
 * @param papszOptions options. NULL or NULL terminated list of options.
 * Currently supported options are:
 * <ul>
 * <li>IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES/NO. Defaults to NO</li>
 * <li>CRITERION=STRICT/EQUIVALENT/EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS.
 *     Defaults to EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS.</li>
 * <li>IGNORE_COORDINATE_EPOCH=YES/NO. Defaults to NO</li>
 * </ul>
 *
 * @return TRUE if equivalent or FALSE otherwise.
 */

int OGRSpatialReference::IsSame( const OGRSpatialReference * poOtherSRS,
                                 const char* const * papszOptions ) const

{
    d->refreshProjObj();
    poOtherSRS->d->refreshProjObj();
    if( !d->m_pj_crs || !poOtherSRS->d->m_pj_crs )
        return d->m_pj_crs == poOtherSRS->d->m_pj_crs;
    if( !CPLTestBool(CSLFetchNameValueDef(papszOptions,
                     "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING", "NO")) )
    {
        if( d->m_axisMapping != poOtherSRS->d->m_axisMapping )
            return false;
    }

    if( !CPLTestBool(CSLFetchNameValueDef(papszOptions,
                     "IGNORE_COORDINATE_EPOCH", "NO")) )
    {
        if( d->m_coordinateEpoch != poOtherSRS->d->m_coordinateEpoch )
            return false;
    }

    bool reboundSelf = false;
    bool reboundOther = false;
    if( d->m_pjType == PJ_TYPE_BOUND_CRS &&
        poOtherSRS->d->m_pjType != PJ_TYPE_BOUND_CRS )
    {
        d->demoteFromBoundCRS();
        reboundSelf = true;
    }
    else if( d->m_pjType != PJ_TYPE_BOUND_CRS &&
             poOtherSRS->d->m_pjType == PJ_TYPE_BOUND_CRS )
    {
        poOtherSRS->d->demoteFromBoundCRS();
        reboundOther = true;
    }

    PJ_COMPARISON_CRITERION criterion =
        PJ_COMP_EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS;
    const char* pszCriterion = CSLFetchNameValueDef(
        papszOptions, "CRITERION", "EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS");
    if( EQUAL(pszCriterion, "STRICT") )
        criterion = PJ_COMP_STRICT;
    else if( EQUAL(pszCriterion, "EQUIVALENT") )
        criterion = PJ_COMP_EQUIVALENT;
    else if( !EQUAL(pszCriterion, "EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS") )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for CRITERION: %s", pszCriterion);
    }
    int ret = proj_is_equivalent_to(d->m_pj_crs, poOtherSRS->d->m_pj_crs,
                                        criterion);
    if( reboundSelf )
        d->undoDemoteFromBoundCRS();
    if( reboundOther )
        poOtherSRS->d->undoDemoteFromBoundCRS();

    return ret;
}

/************************************************************************/
/*                             OSRIsSame()                              */
/************************************************************************/

/**
 * \brief Do these two spatial references describe the same system ?
 *
 * This function is the same as OGRSpatialReference::IsSame().
 */
int OSRIsSame( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    VALIDATE_POINTER1( hSRS1, "OSRIsSame", 0 );
    VALIDATE_POINTER1( hSRS2, "OSRIsSame", 0 );

    return ToPointer(hSRS1)->IsSame(
        ToPointer(hSRS2) );
}

/************************************************************************/
/*                             OSRIsSameEx()                            */
/************************************************************************/

/**
 * \brief Do these two spatial references describe the same system ?
 *
 * This function is the same as OGRSpatialReference::IsSame().
 */
int OSRIsSameEx( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2,
                 const char* const* papszOptions )
{
    VALIDATE_POINTER1( hSRS1, "OSRIsSame", 0 );
    VALIDATE_POINTER1( hSRS2, "OSRIsSame", 0 );

    return ToPointer(hSRS1)->IsSame(
        ToPointer(hSRS2), papszOptions );
}

/************************************************************************/
/*                    convertToOtherProjection()                        */
/************************************************************************/

/**
 * \brief Convert to another equivalent projection
 *
 * Currently implemented:
 * <ul>
 * <li>SRS_PT_MERCATOR_1SP to SRS_PT_MERCATOR_2SP</li>
 * <li>SRS_PT_MERCATOR_2SP to SRS_PT_MERCATOR_1SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP</li>
 * </ul>
 *
 * @param pszTargetProjection target projection.
 * @param papszOptions lists of options. None supported currently.
 * @return a new SRS, or NULL in case of error.
 *
 * @since GDAL 2.3
 */
OGRSpatialReference* OGRSpatialReference::convertToOtherProjection(
                            const char* pszTargetProjection,
                            CPL_UNUSED const char* const* papszOptions ) const
{
    if( pszTargetProjection == nullptr )
        return nullptr;
    int new_code;
    if( EQUAL(pszTargetProjection, SRS_PT_MERCATOR_1SP) )
    {
        new_code = EPSG_CODE_METHOD_MERCATOR_VARIANT_A;
    }
    else if( EQUAL(pszTargetProjection, SRS_PT_MERCATOR_2SP) )
    {
        new_code = EPSG_CODE_METHOD_MERCATOR_VARIANT_B;
    }
    else if( EQUAL(pszTargetProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
        new_code = EPSG_CODE_METHOD_LAMBERT_CONIC_CONFORMAL_1SP;
    }
    else if( EQUAL(pszTargetProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        new_code = EPSG_CODE_METHOD_LAMBERT_CONIC_CONFORMAL_2SP;
    }
    else
    {
        return nullptr;
    }

    d->refreshProjObj();
    d->demoteFromBoundCRS();
    OGRSpatialReference* poNewSRS = nullptr;
    if( d->m_pjType == PJ_TYPE_PROJECTED_CRS )
    {
        auto conv = proj_crs_get_coordoperation(
            d->getPROJContext(), d->m_pj_crs);
        auto new_conv = proj_convert_conversion_to_other_method(
            d->getPROJContext(), conv, new_code, nullptr);
        proj_destroy(conv);
        if( new_conv )
        {
            auto geodCRS = proj_crs_get_geodetic_crs(
                d->getPROJContext(), d->m_pj_crs);
            auto cs = proj_crs_get_coordinate_system(
                d->getPROJContext(), d->m_pj_crs);
            if( geodCRS && cs )
            {
                auto new_proj_crs = proj_create_projected_crs(
                    d->getPROJContext(),
                    proj_get_name(d->m_pj_crs),
                    geodCRS, new_conv, cs);
                proj_destroy(new_conv);
                if( new_proj_crs )
                {
                    poNewSRS = new OGRSpatialReference();

                    if( d->m_pj_bound_crs_target && d->m_pj_bound_crs_co )
                    {
                        auto boundCRS = proj_crs_create_bound_crs(
                            d->getPROJContext(),
                            new_proj_crs, d->m_pj_bound_crs_target,
                            d->m_pj_bound_crs_co);
                        if( boundCRS )
                        {
                            proj_destroy(new_proj_crs);
                            new_proj_crs = boundCRS;
                        }
                    }

                    poNewSRS->d->setPjCRS(new_proj_crs);
                }
            }
            proj_destroy(geodCRS);
            proj_destroy(cs);
        }
    }
    d->undoDemoteFromBoundCRS();
    return poNewSRS;
}


/************************************************************************/
/*                    OSRConvertToOtherProjection()                     */
/************************************************************************/

/**
 * \brief Convert to another equivalent projection
 *
 * Currently implemented:
 * <ul>
 * <li>SRS_PT_MERCATOR_1SP to SRS_PT_MERCATOR_2SP</li>
 * <li>SRS_PT_MERCATOR_2SP to SRS_PT_MERCATOR_1SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP</li>
 * </ul>
 *
 * @param hSRS source SRS
 * @param pszTargetProjection target projection.
 * @param papszOptions lists of options. None supported currently.
 * @return a new SRS, or NULL in case of error.
 *
 * @since GDAL 2.3
 */
OGRSpatialReferenceH OSRConvertToOtherProjection(
                                    OGRSpatialReferenceH hSRS,
                                    const char* pszTargetProjection,
                                    const char* const* papszOptions )
{
    VALIDATE_POINTER1( hSRS, "OSRConvertToOtherProjection", nullptr );
    return ToHandle(
        ToPointer(hSRS)->
            convertToOtherProjection(pszTargetProjection, papszOptions));
}

/************************************************************************/
/*                           OSRFindMatches()                           */
/************************************************************************/

/**
 * \brief Try to identify a match between the passed SRS and a related SRS
 * in a catalog.
 *
 * Matching may be partial, or may fail.
 * Returned entries will be sorted by decreasing match confidence (first
 * entry has the highest match confidence).
 *
 * The exact way matching is done may change in future versions. Starting with
 * GDAL 3.0, it relies on PROJ' proj_identify() function.
 *
 * This function is the same as OGRSpatialReference::FindMatches().
 *
 * @param hSRS SRS to match
 * @param papszOptions NULL terminated list of options or NULL
 * @param pnEntries Output parameter. Number of values in the returned array.
 * @param ppanMatchConfidence Output parameter (or NULL). *ppanMatchConfidence
 * will be allocated to an array of *pnEntries whose values between 0 and 100
 * indicate the confidence in the match. 100 is the highest confidence level.
 * The array must be freed with CPLFree().
 *
 * @return an array of SRS that match the passed SRS, or NULL. Must be freed with
 * OSRFreeSRSArray()
 *
 * @since GDAL 2.3
 */
OGRSpatialReferenceH* OSRFindMatches( OGRSpatialReferenceH hSRS,
                                      char** papszOptions,
                                      int* pnEntries,
                                      int** ppanMatchConfidence )
{
    if( pnEntries )
        *pnEntries = 0;
    if( ppanMatchConfidence )
        *ppanMatchConfidence = nullptr;
    VALIDATE_POINTER1( hSRS, "OSRFindMatches", nullptr );

    OGRSpatialReference* poSRS = ToPointer(hSRS);
    return poSRS->FindMatches(papszOptions, pnEntries,
                              ppanMatchConfidence);
}

/************************************************************************/
/*                           OSRFreeSRSArray()                          */
/************************************************************************/

/**
 * \brief Free return of OSRIdentifyMatches()
 *
 * @param pahSRS array of SRS (must be NULL terminated)
 * @since GDAL 2.3
 */
void OSRFreeSRSArray(OGRSpatialReferenceH* pahSRS)
{
    if( pahSRS != nullptr )
    {
        for( int i = 0; pahSRS[i] != nullptr; ++i )
        {
            OSRRelease(pahSRS[i]);
        }
        CPLFree(pahSRS);
    }
}

/************************************************************************/
/*                             SetTOWGS84()                             */
/************************************************************************/


/**
 * \brief Set the Bursa-Wolf conversion to WGS84.
 *
 * This will create the TOWGS84 node as a child of the DATUM.  It will fail
 * if there is no existing DATUM node. It will replace
 * an existing TOWGS84 node if there is one.
 *
 * The parameters have the same meaning as EPSG transformation 9606
 * (Position Vector 7-param. transformation).
 *
 * This method is the same as the C function OSRSetTOWGS84().
 *
 * @param dfDX X child in meters.
 * @param dfDY Y child in meters.
 * @param dfDZ Z child in meters.
 * @param dfEX X rotation in arc seconds (optional, defaults to zero).
 * @param dfEY Y rotation in arc seconds (optional, defaults to zero).
 * @param dfEZ Z rotation in arc seconds (optional, defaults to zero).
 * @param dfPPM scaling factor (parts per million).
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetTOWGS84( double dfDX, double dfDY, double dfDZ,
                                        double dfEX, double dfEY, double dfEZ,
                                        double dfPPM )

{
    d->refreshProjObj();
    if( d->m_pj_crs == nullptr )
    {
        return OGRERR_FAILURE;
    }

    // Remove existing BoundCRS
    if( d->m_pjType == PJ_TYPE_BOUND_CRS ) {
        auto baseCRS = proj_get_source_crs(
            d->getPROJContext(), d->m_pj_crs);
        if( !baseCRS )
            return OGRERR_FAILURE;
        d->setPjCRS(baseCRS);
    }

    PJ_PARAM_DESCRIPTION params[7];

    params[0].name = EPSG_NAME_PARAMETER_X_AXIS_TRANSLATION;
    params[0].auth_name = "EPSG";
    params[0].code = XSTRINGIFY(EPSG_CODE_PARAMETER_X_AXIS_TRANSLATION);
    params[0].value = dfDX;
    params[0].unit_name = "metre";
    params[0].unit_conv_factor = 1.0;
    params[0].unit_type = PJ_UT_LINEAR;

    params[1].name = EPSG_NAME_PARAMETER_Y_AXIS_TRANSLATION;
    params[1].auth_name = "EPSG";
    params[1].code = XSTRINGIFY(EPSG_CODE_PARAMETER_Y_AXIS_TRANSLATION);
    params[1].value = dfDY;
    params[1].unit_name = "metre";
    params[1].unit_conv_factor = 1.0;
    params[1].unit_type = PJ_UT_LINEAR;

    params[2].name = EPSG_NAME_PARAMETER_Z_AXIS_TRANSLATION;
    params[2].auth_name = "EPSG";
    params[2].code = XSTRINGIFY(EPSG_CODE_PARAMETER_Z_AXIS_TRANSLATION);
    params[2].value = dfDZ;
    params[2].unit_name = "metre";
    params[2].unit_conv_factor = 1.0;
    params[2].unit_type = PJ_UT_LINEAR;

    params[3].name = EPSG_NAME_PARAMETER_X_AXIS_ROTATION;
    params[3].auth_name = "EPSG";
    params[3].code = XSTRINGIFY(EPSG_CODE_PARAMETER_X_AXIS_ROTATION);
    params[3].value = dfEX;
    params[3].unit_name = "arc-second";
    params[3].unit_conv_factor = 1. / 3600 * M_PI / 180;
    params[3].unit_type = PJ_UT_ANGULAR;

    params[4].name = EPSG_NAME_PARAMETER_Y_AXIS_ROTATION;
    params[4].auth_name = "EPSG";
    params[4].code = XSTRINGIFY(EPSG_CODE_PARAMETER_Y_AXIS_ROTATION);
    params[4].value = dfEY;
    params[4].unit_name = "arc-second";
    params[4].unit_conv_factor = 1. / 3600 * M_PI / 180;
    params[4].unit_type = PJ_UT_ANGULAR;

    params[5].name = EPSG_NAME_PARAMETER_Z_AXIS_ROTATION;
    params[5].auth_name = "EPSG";
    params[5].code = XSTRINGIFY(EPSG_CODE_PARAMETER_Z_AXIS_ROTATION);
    params[5].value = dfEZ;
    params[5].unit_name = "arc-second";
    params[5].unit_conv_factor = 1. / 3600 * M_PI / 180;
    params[5].unit_type = PJ_UT_ANGULAR;

    params[6].name = EPSG_NAME_PARAMETER_SCALE_DIFFERENCE;
    params[6].auth_name = "EPSG";
    params[6].code = XSTRINGIFY(EPSG_CODE_PARAMETER_SCALE_DIFFERENCE);
    params[6].value = dfPPM;
    params[6].unit_name = "parts per million";
    params[6].unit_conv_factor = 1e-6;
    params[6].unit_type = PJ_UT_SCALE;

    auto sourceCRS =
        proj_crs_get_geodetic_crs(d->getPROJContext(), d->m_pj_crs);
    if( !sourceCRS )
    {
        return OGRERR_FAILURE;
    }

    const auto sourceType = proj_get_type(sourceCRS);

    auto targetCRS = proj_create_from_database(
        d->getPROJContext(), "EPSG",
        sourceType == PJ_TYPE_GEOGRAPHIC_2D_CRS ? "4326":
        sourceType == PJ_TYPE_GEOGRAPHIC_3D_CRS ? "4979": "4978",
        PJ_CATEGORY_CRS, false, nullptr);
    if( !targetCRS )
    {
        proj_destroy(sourceCRS);
        return OGRERR_FAILURE;
    }

    CPLString osMethodCode;
    osMethodCode.Printf("%d",
        sourceType == PJ_TYPE_GEOGRAPHIC_2D_CRS ?
            EPSG_CODE_METHOD_POSITION_VECTOR_GEOGRAPHIC_2D:
        sourceType == PJ_TYPE_GEOGRAPHIC_3D_CRS ?
            EPSG_CODE_METHOD_POSITION_VECTOR_GEOGRAPHIC_3D:
            EPSG_CODE_METHOD_POSITION_VECTOR_GEOCENTRIC);

    auto transf = proj_create_transformation(
        d->getPROJContext(), "Transformation to WGS84", nullptr, nullptr,
        sourceCRS, targetCRS, nullptr,
        sourceType == PJ_TYPE_GEOGRAPHIC_2D_CRS ?
            EPSG_NAME_METHOD_POSITION_VECTOR_GEOGRAPHIC_2D:
        sourceType == PJ_TYPE_GEOGRAPHIC_3D_CRS ?
            EPSG_NAME_METHOD_POSITION_VECTOR_GEOGRAPHIC_3D:
            EPSG_NAME_METHOD_POSITION_VECTOR_GEOCENTRIC,
        "EPSG",
        osMethodCode.c_str(),
        7, params, -1);
    proj_destroy(sourceCRS);
    if( !transf )
    {
        proj_destroy(targetCRS);
        return OGRERR_FAILURE;
    }

    auto newBoundCRS =
        proj_crs_create_bound_crs(
            d->getPROJContext(), d->m_pj_crs, targetCRS, transf);
    proj_destroy(transf);
    proj_destroy(targetCRS);
    if( !newBoundCRS )
    {
        return OGRERR_FAILURE;
    }

    d->setPjCRS(newBoundCRS);
    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetTOWGS84()                            */
/************************************************************************/

/**
 * \brief Set the Bursa-Wolf conversion to WGS84.
 *
 * This function is the same as OGRSpatialReference::SetTOWGS84().
 */
OGRErr OSRSetTOWGS84( OGRSpatialReferenceH hSRS,
                      double dfDX, double dfDY, double dfDZ,
                      double dfEX, double dfEY, double dfEZ,
                      double dfPPM )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTOWGS84", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        SetTOWGS84( dfDX, dfDY, dfDZ, dfEX, dfEY, dfEZ, dfPPM );
}

/************************************************************************/
/*                             GetTOWGS84()                             */
/************************************************************************/

/**
 * \brief Fetch TOWGS84 parameters, if available.
 *
 * The parameters have the same meaning as EPSG transformation 9606
 * (Position Vector 7-param. transformation).
 *
 * @param padfCoeff array into which up to 7 coefficients are placed.
 * @param nCoeffCount size of padfCoeff - defaults to 7.
 *
 * @return OGRERR_NONE on success, or OGRERR_FAILURE if there is no
 * TOWGS84 node available.
 */

OGRErr OGRSpatialReference::GetTOWGS84( double * padfCoeff,
                                        int nCoeffCount ) const

{
    d->refreshProjObj();
    if( d->m_pjType != PJ_TYPE_BOUND_CRS )
        return OGRERR_FAILURE;

    memset( padfCoeff, 0, sizeof(double) * nCoeffCount );

    auto transf =
        proj_crs_get_coordoperation(d->getPROJContext(), d->m_pj_crs);
    int success = proj_coordoperation_get_towgs84_values(
        d->getPROJContext(), transf, padfCoeff, nCoeffCount, false);
    proj_destroy(transf);

    return success ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                           OSRGetTOWGS84()                            */
/************************************************************************/

/**
 * \brief Fetch TOWGS84 parameters, if available.
 *
 * This function is the same as OGRSpatialReference::GetTOWGS84().
 */
OGRErr OSRGetTOWGS84( OGRSpatialReferenceH hSRS,
                      double * padfCoeff, int nCoeffCount )

{
    VALIDATE_POINTER1( hSRS, "OSRGetTOWGS84", OGRERR_FAILURE );

    return ToPointer(hSRS)->
        GetTOWGS84( padfCoeff, nCoeffCount);
}

/************************************************************************/
/*                         IsAngularParameter()                         */
/************************************************************************/

/** Is the passed projection parameter an angular one?
 *
 * @return TRUE or FALSE
 */

int OGRSpatialReference::IsAngularParameter( const char *pszParameterName )

{
    if( STARTS_WITH_CI(pszParameterName, "long")
        || STARTS_WITH_CI(pszParameterName, "lati")
        || EQUAL(pszParameterName, SRS_PP_CENTRAL_MERIDIAN)
        || STARTS_WITH_CI(pszParameterName, "standard_parallel")
        || EQUAL(pszParameterName, SRS_PP_AZIMUTH)
        || EQUAL(pszParameterName, SRS_PP_RECTIFIED_GRID_ANGLE) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                        IsLongitudeParameter()                        */
/************************************************************************/

/** Is the passed projection parameter an angular longitude
 * (relative to a prime meridian)?
 *
 * @return TRUE or FALSE
 */

int OGRSpatialReference::IsLongitudeParameter( const char *pszParameterName )

{
    if( STARTS_WITH_CI(pszParameterName, "long")
        || EQUAL(pszParameterName, SRS_PP_CENTRAL_MERIDIAN) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                         IsLinearParameter()                          */
/************************************************************************/

/** Is the passed projection parameter an linear one measured in meters or
 * some similar linear measure.
 *
 * @return TRUE or FALSE
 */
int OGRSpatialReference::IsLinearParameter( const char *pszParameterName )

{
    if( STARTS_WITH_CI(pszParameterName, "false_")
        || EQUAL(pszParameterName, SRS_PP_SATELLITE_HEIGHT) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            GetNormInfo()                             */
/************************************************************************/

/**
 * \brief Set the internal information for normalizing linear, and angular values.
 */
void OGRSpatialReference::GetNormInfo() const

{
    if( d->bNormInfoSet )
        return;

/* -------------------------------------------------------------------- */
/*      Initialize values.                                              */
/* -------------------------------------------------------------------- */
    d->bNormInfoSet = TRUE;

    d->dfFromGreenwich = GetPrimeMeridian(nullptr);
    d->dfToMeter = GetLinearUnits(nullptr);
    d->dfToDegrees = GetAngularUnits(nullptr) / CPLAtof(SRS_UA_DEGREE_CONV);
    if( fabs(d->dfToDegrees-1.0) < 0.000000001 )
        d->dfToDegrees = 1.0;
}

/************************************************************************/
/*                            GetExtension()                            */
/************************************************************************/

/**
 * \brief Fetch extension value.
 *
 * Fetch the value of the named EXTENSION item for the identified
 * target node.
 *
 * @param pszTargetKey the name or path to the parent node of the EXTENSION.
 * @param pszName the name of the extension being fetched.
 * @param pszDefault the value to return if the extension is not found.
 *
 * @return node value if successful or pszDefault on failure.
 */

const char *OGRSpatialReference::GetExtension( const char *pszTargetKey,
                                               const char *pszName,
                                               const char *pszDefault ) const

{
/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poNode = pszTargetKey == nullptr
        ? GetRoot()
        : GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch matching EXTENSION if there is one.                       */
/* -------------------------------------------------------------------- */
    for( int i = poNode->GetChildCount()-1; i >= 0; i-- )
    {
        const OGR_SRSNode *poChild = poNode->GetChild(i);

        if( EQUAL(poChild->GetValue(), "EXTENSION")
            && poChild->GetChildCount() >= 2 )
        {
            if( EQUAL(poChild->GetChild(0)->GetValue(), pszName) )
                return poChild->GetChild(1)->GetValue();
        }
    }

    return pszDefault;
}

/************************************************************************/
/*                            SetExtension()                            */
/************************************************************************/
/**
 * \brief Set extension value.
 *
 * Set the value of the named EXTENSION item for the identified
 * target node.
 *
 * @param pszTargetKey the name or path to the parent node of the EXTENSION.
 * @param pszName the name of the extension being fetched.
 * @param pszValue the value to set
 *
 * @return OGRERR_NONE on success
 */

OGRErr OGRSpatialReference::SetExtension( const char *pszTargetKey,
                                          const char *pszName,
                                          const char *pszValue )

{
/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poNode = nullptr;

    if( pszTargetKey == nullptr )
        poNode = GetRoot();
    else
        poNode = GetAttrNode(pszTargetKey);

    if( poNode == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Fetch matching EXTENSION if there is one.                       */
/* -------------------------------------------------------------------- */
    for( int i = poNode->GetChildCount()-1; i >= 0; i-- )
    {
        OGR_SRSNode *poChild = poNode->GetChild(i);

        if( EQUAL(poChild->GetValue(), "EXTENSION")
            && poChild->GetChildCount() >= 2 )
        {
            if( EQUAL(poChild->GetChild(0)->GetValue(), pszName) )
            {
                poChild->GetChild(1)->SetValue( pszValue );
                return OGRERR_NONE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a new EXTENSION node.                                    */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poAuthNode = new OGR_SRSNode( "EXTENSION" );
    poAuthNode->AddChild( new OGR_SRSNode( pszName ) );
    poAuthNode->AddChild( new OGR_SRSNode( pszValue ) );

    poNode->AddChild( poAuthNode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRCleanup()                             */
/************************************************************************/

static void CleanupSRSWGS84Mutex();

/**
 * \brief Cleanup cached SRS related memory.
 *
 * This function will attempt to cleanup any cache spatial reference
 * related information, such as cached tables of coordinate systems.
 */
void OSRCleanup( void )

{
    OGRCTDumpStatistics();
    CSVDeaccess( nullptr );
    CleanupSRSWGS84Mutex();
    OSRCTCleanCache();
    OSRCleanupTLSContext();
}

/************************************************************************/
/*                              GetAxesCount()                          */
/************************************************************************/

/**
 * \brief Return the number of axis of the coordinate system of the CRS.
 *
 * @since GDAL 3.0
 */
int OGRSpatialReference::GetAxesCount() const
{
    int axisCount = 0;
    d->refreshProjObj();
    if( d->m_pj_crs == nullptr )
    {
        return 0;
    }
    d->demoteFromBoundCRS();
    auto ctxt = d->getPROJContext();
    if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        for( int i = 0; ; i++ )
        {
            auto subCRS = proj_crs_get_sub_crs(ctxt, d->m_pj_crs, i);
            if( !subCRS )
                break;
            if( proj_get_type(subCRS) == PJ_TYPE_BOUND_CRS )
            {
                auto baseCRS = proj_get_source_crs(ctxt, subCRS);
                if( baseCRS )
                {
                    proj_destroy(subCRS);
                    subCRS = baseCRS;
                }
            }
            auto cs = proj_crs_get_coordinate_system(ctxt, subCRS);
            if( cs )
            {
                axisCount += proj_cs_get_axis_count(ctxt, cs);
                proj_destroy(cs);
            }
            proj_destroy(subCRS);
        }
    }
    else
    {
        auto cs = proj_crs_get_coordinate_system(ctxt, d->m_pj_crs);
        if( cs )
        {
            axisCount = proj_cs_get_axis_count(ctxt, cs);
            proj_destroy(cs);
        }
    }
    d->undoDemoteFromBoundCRS();
    return axisCount;
}

/************************************************************************/
/*                           OSRGetAxesCount()                          */
/************************************************************************/

/**
 * \brief Return the number of axis of the coordinate system of the CRS.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::GetAxesCount()
 *
 * @since GDAL 3.1
 */
int OSRGetAxesCount( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAxesCount", 0 );

    return ToPointer(hSRS)->GetAxesCount();
}

/************************************************************************/
/*                              GetAxis()                               */
/************************************************************************/

/**
 * \brief Fetch the orientation of one axis.
 *
 * Fetches the request axis (iAxis - zero based) from the
 * indicated portion of the coordinate system (pszTargetKey) which
 * should be either "GEOGCS" or "PROJCS".
 *
 * No CPLError is issued on routine failures (such as not finding the AXIS).
 *
 * This method is equivalent to the C function OSRGetAxis().
 *
 * @param pszTargetKey the coordinate system part to query ("PROJCS" or "GEOGCS").
 * @param iAxis the axis to query (0 for first, 1 for second, 2 for third).
 * @param peOrientation location into which to place the fetch orientation, may be NULL.
 *
 * @return the name of the axis or NULL on failure.
 */

const char *
OGRSpatialReference::GetAxis( const char *pszTargetKey, int iAxis,
                              OGRAxisOrientation *peOrientation ) const

{
    if( peOrientation != nullptr )
        *peOrientation = OAO_Other;

    d->refreshProjObj();
    if( d->m_pj_crs == nullptr )
    {
        return nullptr;
    }

    pszTargetKey = d->nullifyTargetKeyIfPossible(pszTargetKey);
    if( pszTargetKey == nullptr && iAxis <= 2 )
    {
        auto ctxt = d->getPROJContext();

        int iAxisModified = iAxis;

        d->demoteFromBoundCRS();

        PJ* cs = nullptr;
        if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
        {
            auto horizCRS = proj_crs_get_sub_crs(ctxt, d->m_pj_crs, 0);
            if( horizCRS )
            {
                if( proj_get_type(horizCRS) == PJ_TYPE_BOUND_CRS )
                {
                    auto baseCRS = proj_get_source_crs(ctxt, horizCRS);
                    if( baseCRS )
                    {
                        proj_destroy(horizCRS);
                        horizCRS = baseCRS;
                    }
                }
                cs = proj_crs_get_coordinate_system(ctxt, horizCRS);
                proj_destroy(horizCRS);
                if( cs )
                {
                    if( iAxisModified >= proj_cs_get_axis_count(ctxt, cs) )
                    {
                        iAxisModified -= proj_cs_get_axis_count(ctxt, cs);
                        proj_destroy(cs);
                        cs = nullptr;
                    }
                }
            }

            if( cs == nullptr )
            {
                auto vertCRS = proj_crs_get_sub_crs(ctxt, d->m_pj_crs, 1);
                if( vertCRS )
                {
                    if( proj_get_type(vertCRS) == PJ_TYPE_BOUND_CRS )
                    {
                        auto baseCRS = proj_get_source_crs(ctxt, vertCRS);
                        if( baseCRS )
                        {
                            proj_destroy(vertCRS);
                            vertCRS = baseCRS;
                        }
                    }

                    cs = proj_crs_get_coordinate_system(ctxt, vertCRS);
                    proj_destroy(vertCRS);
                }
            }
        }
        else
        {
            cs = proj_crs_get_coordinate_system(ctxt, d->m_pj_crs);
        }

        if( cs )
        {
            const char* pszName = nullptr;
            const char* pszOrientation = nullptr;
            proj_cs_get_axis_info(
                ctxt, cs, iAxisModified, &pszName, nullptr, &pszOrientation,
                nullptr, nullptr, nullptr, nullptr);
            if( pszName && pszOrientation )
            {
                d->m_osAxisName[iAxis] = pszName;
                if( peOrientation )
                {
                    if( EQUAL(pszOrientation, "NORTH") )
                        *peOrientation = OAO_North;
                    else if( EQUAL(pszOrientation, "EAST") )
                        *peOrientation = OAO_East;
                    else if( EQUAL(pszOrientation, "SOUTH") )
                        *peOrientation = OAO_South;
                    else if( EQUAL(pszOrientation, "WEST") )
                        *peOrientation = OAO_West;
                    else if( EQUAL(pszOrientation, "UP") )
                        *peOrientation = OAO_Up;
                    else if( EQUAL(pszOrientation, "DOWN") )
                        *peOrientation = OAO_Down;
                }
                proj_destroy(cs);
                d->undoDemoteFromBoundCRS();
                return d->m_osAxisName[iAxis].c_str();
            }
            proj_destroy(cs);
        }
        d->undoDemoteFromBoundCRS();
    }

/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poNode = nullptr;

    if( pszTargetKey == nullptr )
        poNode = GetRoot();
    else
        poNode = GetAttrNode(pszTargetKey);

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Find desired child AXIS.                                        */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poAxis = nullptr;
    const int nChildCount = poNode->GetChildCount();

    for( int iChild = 0; iChild < nChildCount; iChild++ )
    {
        const OGR_SRSNode *poChild = poNode->GetChild( iChild );

        if( !EQUAL(poChild->GetValue(), "AXIS") )
            continue;

        if( iAxis == 0 )
        {
            poAxis = poChild;
            break;
        }
        iAxis--;
    }

    if( poAxis == nullptr )
        return nullptr;

    if( poAxis->GetChildCount() < 2 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Extract name and orientation if possible.                       */
/* -------------------------------------------------------------------- */
    if( peOrientation != nullptr )
    {
        const char *pszOrientation = poAxis->GetChild(1)->GetValue();

        if( EQUAL(pszOrientation, "NORTH") )
            *peOrientation = OAO_North;
        else if( EQUAL(pszOrientation, "EAST") )
            *peOrientation = OAO_East;
        else if( EQUAL(pszOrientation, "SOUTH") )
            *peOrientation = OAO_South;
        else if( EQUAL(pszOrientation, "WEST") )
            *peOrientation = OAO_West;
        else if( EQUAL(pszOrientation, "UP") )
            *peOrientation = OAO_Up;
        else if( EQUAL(pszOrientation, "DOWN") )
            *peOrientation = OAO_Down;
        else if( EQUAL(pszOrientation, "OTHER") )
            *peOrientation = OAO_Other;
        else
        {
            CPLDebug( "OSR", "Unrecognized orientation value '%s'.",
                      pszOrientation );
        }
    }

    return poAxis->GetChild(0)->GetValue();
}

/************************************************************************/
/*                             OSRGetAxis()                             */
/************************************************************************/

/**
 * \brief Fetch the orientation of one axis.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::GetAxis
 */
const char *OSRGetAxis( OGRSpatialReferenceH hSRS,
                        const char *pszTargetKey, int iAxis,
                        OGRAxisOrientation *peOrientation )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAxis", nullptr );

    return ToPointer(hSRS)->GetAxis( pszTargetKey, iAxis,
                                                              peOrientation );
}

/************************************************************************/
/*                         OSRAxisEnumToName()                          */
/************************************************************************/

/**
 * \brief Return the string representation for the OGRAxisOrientation enumeration.
 *
 * For example "NORTH" for OAO_North.
 *
 * @return an internal string
 */
const char *OSRAxisEnumToName( OGRAxisOrientation eOrientation )

{
    if( eOrientation == OAO_North )
        return "NORTH";
    if( eOrientation == OAO_East )
        return "EAST";
    if( eOrientation == OAO_South )
        return "SOUTH";
    if( eOrientation == OAO_West )
        return "WEST";
    if( eOrientation == OAO_Up )
        return "UP";
    if( eOrientation == OAO_Down )
        return "DOWN";
    if( eOrientation == OAO_Other )
        return "OTHER";

    return "UNKNOWN";
}

/************************************************************************/
/*                              SetAxes()                               */
/************************************************************************/

/**
 * \brief Set the axes for a coordinate system.
 *
 * Set the names, and orientations of the axes for either a projected
 * (PROJCS) or geographic (GEOGCS) coordinate system.
 *
 * This method is equivalent to the C function OSRSetAxes().
 *
 * @param pszTargetKey either "PROJCS" or "GEOGCS", must already exist in SRS.
 * @param pszXAxisName name of first axis, normally "Long" or "Easting".
 * @param eXAxisOrientation normally OAO_East.
 * @param pszYAxisName name of second axis, normally "Lat" or "Northing".
 * @param eYAxisOrientation normally OAO_North.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr
OGRSpatialReference::SetAxes( const char *pszTargetKey,
                              const char *pszXAxisName,
                              OGRAxisOrientation eXAxisOrientation,
                              const char *pszYAxisName,
                              OGRAxisOrientation eYAxisOrientation )

{
/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poNode = nullptr;

    if( pszTargetKey == nullptr )
        poNode = GetRoot();
    else
        poNode = GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Strip any existing AXIS children.                               */
/* -------------------------------------------------------------------- */
    while( poNode->FindChild( "AXIS" ) >= 0 )
        poNode->DestroyChild( poNode->FindChild( "AXIS" ) );

/* -------------------------------------------------------------------- */
/*      Insert desired axes                                             */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( pszXAxisName ) );
    poAxis->AddChild( new OGR_SRSNode( OSRAxisEnumToName(eXAxisOrientation) ));

    poNode->AddChild( poAxis );

    poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( pszYAxisName ) );
    poAxis->AddChild( new OGR_SRSNode( OSRAxisEnumToName(eYAxisOrientation) ));

    poNode->AddChild( poAxis );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetAxes()                             */
/************************************************************************/
/**
 * \brief Set the axes for a coordinate system.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::SetAxes
 */
OGRErr OSRSetAxes( OGRSpatialReferenceH hSRS,
                   const char *pszTargetKey,
                   const char *pszXAxisName,
                   OGRAxisOrientation eXAxisOrientation,
                   const char *pszYAxisName,
                   OGRAxisOrientation eYAxisOrientation )
{
    VALIDATE_POINTER1( hSRS, "OSRSetAxes", OGRERR_FAILURE );

    return ToPointer(hSRS)->SetAxes( pszTargetKey,
                                                              pszXAxisName,
                                                              eXAxisOrientation,
                                                              pszYAxisName,
                                                    eYAxisOrientation );
}

#ifdef HAVE_MITAB
char CPL_DLL *MITABSpatialRef2CoordSys( const OGRSpatialReference * );
OGRSpatialReference CPL_DLL * MITABCoordSys2SpatialRef( const char * );
#endif

/************************************************************************/
/*                       OSRExportToMICoordSys()                        */
/************************************************************************/
/**
 * \brief Export coordinate system in Mapinfo style CoordSys format.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::exportToMICoordSys
 */
OGRErr OSRExportToMICoordSys( OGRSpatialReferenceH hSRS, char ** ppszReturn )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToMICoordSys", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return ToPointer(hSRS)->exportToMICoordSys( ppszReturn );
}

/************************************************************************/
/*                         exportToMICoordSys()                         */
/************************************************************************/

/**
 * \brief Export coordinate system in Mapinfo style CoordSys format.
 *
 * Note that the returned WKT string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * This method is the same as the C function OSRExportToMICoordSys().
 *
 * @param ppszResult pointer to which dynamically allocated Mapinfo CoordSys
 * definition will be assigned.
 *
 * @return OGRERR_NONE on success, OGRERR_FAILURE on failure,
 * OGRERR_UNSUPPORTED_OPERATION if MITAB library was not linked in.
 */

OGRErr OGRSpatialReference::exportToMICoordSys( char **ppszResult ) const

{
#ifdef HAVE_MITAB
    *ppszResult = MITABSpatialRef2CoordSys( this );
    if( *ppszResult != nullptr && strlen(*ppszResult) > 0 )
        return OGRERR_NONE;

    return OGRERR_FAILURE;
#else
    CPLError( CE_Failure, CPLE_NotSupported,
              "MITAB not available, CoordSys support disabled." );

    return OGRERR_UNSUPPORTED_OPERATION;
#endif
}

/************************************************************************/
/*                       OSRImportFromMICoordSys()                      */
/************************************************************************/
/**
 * \brief Import Mapinfo style CoordSys definition.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::importFromMICoordSys
 */

OGRErr OSRImportFromMICoordSys( OGRSpatialReferenceH hSRS,
                                const char *pszCoordSys )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromMICoordSys", OGRERR_FAILURE );

    return ToPointer(hSRS)->importFromMICoordSys( pszCoordSys );
}

/************************************************************************/
/*                        importFromMICoordSys()                        */
/************************************************************************/

/**
 * \brief Import Mapinfo style CoordSys definition.
 *
 * The OGRSpatialReference is initialized from the passed Mapinfo style CoordSys definition string.
 *
 * This method is the equivalent of the C function OSRImportFromMICoordSys().
 *
 * @param pszCoordSys Mapinfo style CoordSys definition string.
 *
 * @return OGRERR_NONE on success, OGRERR_FAILURE on failure,
 * OGRERR_UNSUPPORTED_OPERATION if MITAB library was not linked in.
 */

OGRErr OGRSpatialReference::importFromMICoordSys( const char *pszCoordSys )

{
#ifdef HAVE_MITAB
    OGRSpatialReference *poResult = MITABCoordSys2SpatialRef( pszCoordSys );

    if( poResult == nullptr )
        return OGRERR_FAILURE;

    *this = *poResult;
    delete poResult;

    return OGRERR_NONE;
#else
    CPLError( CE_Failure, CPLE_NotSupported,
              "MITAB not available, CoordSys support disabled." );

    return OGRERR_UNSUPPORTED_OPERATION;
#endif
}

/************************************************************************/
/*                        OSRCalcInvFlattening()                        */
/************************************************************************/

/**
 * \brief Compute inverse flattening from semi-major and semi-minor axis
 *
 * @param dfSemiMajor Semi-major axis length.
 * @param dfSemiMinor Semi-minor axis length.
 *
 * @return inverse flattening, or 0 if both axis are equal.
 * @since GDAL 2.0
 */

double OSRCalcInvFlattening( double dfSemiMajor, double dfSemiMinor )
{
    if( fabs(dfSemiMajor-dfSemiMinor) < 1e-1 )
        return 0;
    if( dfSemiMajor <= 0 || dfSemiMinor <= 0 || dfSemiMinor > dfSemiMajor )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "OSRCalcInvFlattening(): Wrong input values");
        return 0;
    }

    return dfSemiMajor / (dfSemiMajor - dfSemiMinor);
}

/************************************************************************/
/*                        OSRCalcInvFlattening()                        */
/************************************************************************/

/**
 * \brief Compute semi-minor axis from semi-major axis and inverse flattening.
 *
 * @param dfSemiMajor Semi-major axis length.
 * @param dfInvFlattening Inverse flattening or 0 for sphere.
 *
 * @return semi-minor axis
 * @since GDAL 2.0
 */

double OSRCalcSemiMinorFromInvFlattening( double dfSemiMajor, double dfInvFlattening )
{
    if( fabs(dfInvFlattening) < 0.000000000001 )
        return dfSemiMajor;
    if( dfSemiMajor <= 0.0 || dfInvFlattening <= 1.0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "OSRCalcSemiMinorFromInvFlattening(): Wrong input values");
        return dfSemiMajor;
    }

    return dfSemiMajor * (1.0 - 1.0/dfInvFlattening);
}

/************************************************************************/
/*                        GetWGS84SRS()                                 */
/************************************************************************/

static OGRSpatialReference* poSRSWGS84 = nullptr;
static CPLMutex* hMutex = nullptr;

/**
 * \brief Returns an instance of a SRS object with WGS84 WKT.
 *
 * Note: the instance will have SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER)
 *
 * The reference counter of the returned object is not increased by this operation.
 *
 * @return instance.
 * @since GDAL 2.0
 */

OGRSpatialReference* OGRSpatialReference::GetWGS84SRS()
{
    CPLMutexHolderD(&hMutex);
    if( poSRSWGS84 == nullptr )
    {
        poSRSWGS84 = new OGRSpatialReference(SRS_WKT_WGS84_LAT_LONG);
        poSRSWGS84->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    return poSRSWGS84;
}

/************************************************************************/
/*                        CleanupSRSWGS84Mutex()                       */
/************************************************************************/

static void CleanupSRSWGS84Mutex()
{
    if( hMutex != nullptr )
    {
        poSRSWGS84->Release();
        poSRSWGS84 = nullptr;
        CPLDestroyMutex(hMutex);
        hMutex = nullptr;
    }
}

/************************************************************************/
/*                         OSRImportFromProj4()                         */
/************************************************************************/
/**
 * \brief Import PROJ coordinate string.
 *
 * This function is the same as OGRSpatialReference::importFromProj4().
 */
OGRErr OSRImportFromProj4( OGRSpatialReferenceH hSRS, const char *pszProj4 )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromProj4", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->
        importFromProj4( pszProj4 );
}

/************************************************************************/
/*                          importFromProj4()                           */
/************************************************************************/

/**
 * \brief Import PROJ coordinate string.
 *
 * The OGRSpatialReference is initialized from the passed PROJs style
 * coordinate system string.
 *
 * Example:
 *   pszProj4 = "+proj=utm +zone=11 +datum=WGS84"
 *
 * It is also possible to import "+init=epsg:n" style definitions. Those are
 * a legacy syntax that should be avoided in the future. In particular they will
 * result in CRS objects whose axis order might not correspond to the official
 * EPSG axis order.
 *
 * This method is the equivalent of the C function OSRImportFromProj4().
 *
 * @param pszProj4 the PROJ style string.
 *
 * @return OGRERR_NONE on success or OGRERR_CORRUPT_DATA on failure.
 */

OGRErr OGRSpatialReference::importFromProj4( const char * pszProj4 )

{
    if( strlen(pszProj4) >= 10000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too long PROJ string");
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    Clear();

    CPLString osProj4(pszProj4);
    if( osProj4.find("type=crs") == std::string::npos )
    {
        osProj4 += " +type=crs";
    }

    if( osProj4.find("+init=epsg:") != std::string::npos &&
        getenv("PROJ_USE_PROJ4_INIT_RULES") == nullptr )
    {
        static bool bHasWarned = false;
        if( !bHasWarned )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "+init=epsg:XXXX syntax is deprecated. It might return "
                     "a CRS with a non-EPSG compliant axis order.");
            bHasWarned = true;
        }
    }
    proj_context_use_proj4_init_rules(d->getPROJContext(), true);
    d->setPjCRS(proj_create(d->getPROJContext(), osProj4.c_str()));
    proj_context_use_proj4_init_rules(d->getPROJContext(), false);
    return d->m_pj_crs ? OGRERR_NONE : OGRERR_CORRUPT_DATA;
}

/************************************************************************/
/*                          OSRExportToProj4()                          */
/************************************************************************/
/**
 * \brief Export coordinate system in PROJ.4 legacy format.
 *
 * \warning Use of this function is discouraged. Its behavior in GDAL &gt;= 3 /
 * PROJ &gt;= 6 is significantly different from earlier versions. In particular
 * +datum will only encode WGS84, NAD27 and NAD83, and +towgs84/+nadgrids terms
 * will be missing most of the time. PROJ strings to encode CRS should be
 * considered as a legacy solution. Using a AUTHORITY:CODE or WKT representation is the
 * recommended way.
 *
 * This function is the same as OGRSpatialReference::exportToProj4().
 */
OGRErr CPL_STDCALL OSRExportToProj4( OGRSpatialReferenceH hSRS,
                                     char ** ppszReturn )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToProj4", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return OGRSpatialReference::FromHandle(hSRS)->exportToProj4( ppszReturn );
}

/************************************************************************/
/*                           exportToProj4()                            */
/************************************************************************/

/**
 * \brief Export coordinate system in PROJ.4 legacy format.
 *
 * \warning Use of this function is discouraged. Its behavior in GDAL &gt;= 3 /
 * PROJ &gt;= 6 is significantly different from earlier versions. In particular
 * +datum will only encode WGS84, NAD27 and NAD83, and +towgs84/+nadgrids terms
 * will be missing most of the time. PROJ strings to encode CRS should be
 * considered as a a legacy solution. Using a AUTHORITY:CODE or WKT representation is the
 * recommended way.
 *
 * Converts the loaded coordinate reference system into PROJ format
 * to the extent possible.  The string returned in ppszProj4 should be
 * deallocated by the caller with CPLFree() when no longer needed.
 *
 * LOCAL_CS coordinate systems are not translatable.  An empty string
 * will be returned along with OGRERR_NONE.
 *
 * Special processing for Transverse Mercator:
 * Starting with GDAL 3.0, if the OSR_USE_APPROX_TMERC configuration option is
 * set to YES, the PROJ definition built from the SRS will use the +approx flag
 * for the tmerc and utm projection methods, rather than the more accurate method.
 *
 * Starting with GDAL 3.0.3, this method will try to add a +towgs84 parameter,
 * if there's none attached yet to the SRS and if the SRS has a EPSG code.
 * See the AddGuessedTOWGS84() method for how this +towgs84 parameter may be added.
 * This automatic addition may be disabled by setting the
 * OSR_ADD_TOWGS84_ON_EXPORT_TO_PROJ4 configuration option to NO.
 *
 * This method is the equivalent of the C function OSRExportToProj4().
 *
 * @param ppszProj4 pointer to which dynamically allocated PROJ definition
 * will be assigned.
 *
 * @return OGRERR_NONE on success or an error code on failure.
 */

OGRErr OGRSpatialReference::exportToProj4( char ** ppszProj4 ) const

{
    // In the past calling this method was thread-safe, even if we never
    // guaranteed it. Now proj_as_proj_string() will cache the result internally,
    // so this is no longer thread-safe.
    std::lock_guard<std::mutex> oLock(d->m_mutex);

    d->refreshProjObj();
    if( d->m_pj_crs == nullptr ||
        d->m_pjType == PJ_TYPE_ENGINEERING_CRS )
    {
        *ppszProj4 = CPLStrdup("");
        return OGRERR_FAILURE;
    }

    // OSR_USE_ETMERC is here just for legacy
    bool bForceApproxTMerc = false;
    const char* pszUseETMERC = CPLGetConfigOption("OSR_USE_ETMERC", nullptr);
    if( pszUseETMERC && pszUseETMERC[0] )
    {
        static bool bHasWarned = false;
        if( !bHasWarned )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "OSR_USE_ETMERC is a legacy configuration option, which "
                     "now has only effect when set to NO (YES is the default). "
                     "Use OSR_USE_APPROX_TMERC=YES instead");
            bHasWarned = true;
        }
        bForceApproxTMerc = !CPLTestBool(pszUseETMERC);
    }
    else
    {
        const char* pszUseApproxTMERC = CPLGetConfigOption("OSR_USE_APPROX_TMERC", nullptr);
        if( pszUseApproxTMERC && pszUseApproxTMERC[0] )
        {
            bForceApproxTMerc = CPLTestBool(pszUseApproxTMERC);
        }
    }
    const char* options[] = {
        bForceApproxTMerc ? "USE_APPROX_TMERC=YES" : nullptr,
        nullptr
    };

    const char* projString = proj_as_proj_string(d->getPROJContext(),
                                                 d->m_pj_crs, PJ_PROJ_4, options);

    PJ* boundCRS = nullptr;
    if( projString &&
        (strstr(projString, "+datum=") == nullptr ||
         d->m_pjType == PJ_TYPE_COMPOUND_CRS) &&
        CPLTestBool(
            CPLGetConfigOption("OSR_ADD_TOWGS84_ON_EXPORT_TO_PROJ4", "YES")) )
    {
        boundCRS = GDAL_proj_crs_create_bound_crs_to_WGS84(
            d->getPROJContext(), d->m_pj_crs, true,
            strstr(projString, "+datum=") == nullptr);
        if( boundCRS )
        {
            projString = proj_as_proj_string(d->getPROJContext(),
                                             boundCRS, PJ_PROJ_4, options);
        }
    }

    if( projString == nullptr )
    {
        *ppszProj4 = CPLStrdup("");
        proj_destroy(boundCRS);
        return OGRERR_FAILURE;
    }
    *ppszProj4 = CPLStrdup(projString);
    proj_destroy(boundCRS);
    char* pszTypeCrs = strstr(*ppszProj4, " +type=crs");
    if( pszTypeCrs )
        *pszTypeCrs = '\0';
    return OGRERR_NONE;
}

/************************************************************************/
/*                            morphToESRI()                             */
/************************************************************************/
/**
 * \brief Convert in place to ESRI WKT format.
 *
 * The value nodes of this coordinate system are modified in various manners
 * more closely map onto the ESRI concept of WKT format.  This includes
 * renaming a variety of projections and arguments, and stripping out
 * nodes note recognised by ESRI (like AUTHORITY and AXIS).
 *
 * \note Since GDAL 3.0, this function has only user-visible effects at
 * exportToWkt() time. It is recommended to use instead exportToWkt(char**, const char* const char*) const
 * with options having FORMAT=WKT1_ESRI.
 *
 * This does the same as the C function OSRMorphToESRI().
 *
 * @return OGRERR_NONE unless something goes badly wrong.
 * @deprecated
 */

OGRErr OGRSpatialReference::morphToESRI()

{
    d->refreshProjObj();
    d->setMorphToESRI(true);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRMorphToESRI()                           */
/************************************************************************/

/**
 * \brief Convert in place to ESRI WKT format.
 *
 * This function is the same as the C++ method
 * OGRSpatialReference::morphToESRI().
 */
OGRErr OSRMorphToESRI( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRMorphToESRI", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->morphToESRI();
}

/************************************************************************/
/*                           morphFromESRI()                            */
/************************************************************************/

/**
 * \brief Convert in place from ESRI WKT format.
 *
 * The value notes of this coordinate system are modified in various manners
 * to adhere more closely to the WKT standard.  This mostly involves
 * translating a variety of ESRI names for projections, arguments and
 * datums to "standard" names, as defined by Adam Gawne-Cain's reference
 * translation of EPSG to WKT for the CT specification.
 *
 * \note Since GDAL 3.0, this function is essentially a no-operation, since
 * morphing from ESRI is automatically done by importFromWkt(). Its only
 * effect is to undo the effect of a potential prior call to morphToESRI().
 *
 * This does the same as the C function OSRMorphFromESRI().
 *
 * @return OGRERR_NONE unless something goes badly wrong.
 * @deprecated
 */

OGRErr OGRSpatialReference::morphFromESRI()

{
    d->refreshProjObj();
    d->setMorphToESRI(false);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRMorphFromESRI()                          */
/************************************************************************/

/**
 * \brief Convert in place from ESRI WKT format.
 *
 * This function is the same as the C++ method
 * OGRSpatialReference::morphFromESRI().
 */
OGRErr OSRMorphFromESRI( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRMorphFromESRI", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->morphFromESRI();
}

/************************************************************************/
/*                            FindMatches()                             */
/************************************************************************/

/**
 * \brief Try to identify a match between the passed SRS and a related SRS
 * in a catalog.
 *
 * Matching may be partial, or may fail.
 * Returned entries will be sorted by decreasing match confidence (first
 * entry has the highest match confidence).
 *
 * The exact way matching is done may change in future versions. Starting with
 * GDAL 3.0, it relies on PROJ' proj_identify() function.
 *
 * This method is the same as OSRFindMatches().
 *
 * @param papszOptions NULL terminated list of options or NULL
 * @param pnEntries Output parameter. Number of values in the returned array.
 * @param ppanMatchConfidence Output parameter (or NULL). *ppanMatchConfidence
 * will be allocated to an array of *pnEntries whose values between 0 and 100
 * indicate the confidence in the match. 100 is the highest confidence level.
 * The array must be freed with CPLFree().
 *
 * @return an array of SRS that match the passed SRS, or NULL. Must be freed with
 * OSRFreeSRSArray()
 *
 * @since GDAL 2.3
 */
OGRSpatialReferenceH* OGRSpatialReference::FindMatches(
                                          char** papszOptions,
                                          int* pnEntries,
                                          int** ppanMatchConfidence ) const
{
    CPL_IGNORE_RET_VAL(papszOptions);

    if( pnEntries )
        *pnEntries = 0;
    if( ppanMatchConfidence )
        *ppanMatchConfidence = nullptr;

    d->refreshProjObj();
    if( !d->m_pj_crs )
        return nullptr;

    int* panConfidence = nullptr;
    auto list = proj_identify(d->getPROJContext(),
                                  d->m_pj_crs,
                                  nullptr,
                                  nullptr,
                                  &panConfidence);
    if( !list )
        return nullptr;

    const int nMatches = proj_list_get_count(list);

    if( pnEntries )
        *pnEntries = static_cast<int>(nMatches);
    OGRSpatialReferenceH* pahRet =
                static_cast<OGRSpatialReferenceH*>(
                        CPLCalloc(sizeof(OGRSpatialReferenceH),
                                  nMatches + 1));
    if( ppanMatchConfidence )
    {
        *ppanMatchConfidence = static_cast<int*>(
                            CPLMalloc(sizeof(int) * (nMatches + 1)));
    }
    for(int i=0; i<nMatches; i++)
    {
        PJ* obj = proj_list_get(d->getPROJContext(), list, i);
        CPLAssert(obj);
        OGRSpatialReference* poSRS = new OGRSpatialReference();
        poSRS->d->setPjCRS(obj);
        pahRet[i] = ToHandle(poSRS);
        if( ppanMatchConfidence )
            (*ppanMatchConfidence)[i] = panConfidence[i];
    }
    pahRet[ nMatches ] = nullptr;
    proj_list_destroy(list);
    proj_int_list_destroy(panConfidence);

    return pahRet;
}

/************************************************************************/
/*                          importFromEPSGA()                           */
/************************************************************************/

/**
 * \brief  Initialize SRS based on EPSG geographic, projected or vertical CRS code.
 *
 * This method will initialize the spatial reference based on the
 * passed in EPSG CRS code found in the PROJ database.
 *
 * Since GDAL 3.0, this method is identical to importFromEPSG().
 *
 * Before GDAL 3.0.3, this method try to attach a 3-parameter or 7-parameter
 * Helmert transformation to WGS84 when there is one and only one such method
 * available for the CRS.
 * This behavior might not always be desirable, so starting with GDAL 3.0.3,
 * this is no longer done. However the OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG
 * configuration option can be set to YES to enable past behavior.
 * The AddGuessedTOWGS84() method can also be used for that purpose.
 *
 * This method is the same as the C function OSRImportFromEPSGA().
 *
 * @param nCode a CRS code.
 *
 * @return OGRERR_NONE on success, or an error code on failure.
 */

OGRErr OGRSpatialReference::importFromEPSGA( int nCode )

{
    Clear();

    const bool bUseNonDeprecated = CPLTestBool(
                CPLGetConfigOption("OSR_USE_NON_DEPRECATED", "YES"));
    const bool bAddTOWGS84 = CPLTestBool(
            CPLGetConfigOption("OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG", "NO"));
    auto tlsCache = OSRGetProjTLSCache();
    if( tlsCache )
    {
        auto cachedObj = tlsCache->GetPJForEPSGCode(nCode, bUseNonDeprecated, bAddTOWGS84);
        if( cachedObj )
        {
            d->setPjCRS(cachedObj);
            return OGRERR_NONE;
        }
    }

    CPLString osCode;
    osCode.Printf("%d", nCode);
    auto obj = proj_create_from_database(d->getPROJContext(),
                                             "EPSG",
                                             osCode.c_str(),
                                             PJ_CATEGORY_CRS,
                                             true,
                                             nullptr);
    if( !obj )
    {
        return OGRERR_FAILURE;
    }

    if( proj_is_deprecated(obj) ) {
        auto list = proj_get_non_deprecated(d->getPROJContext(), obj);
        if( list && bUseNonDeprecated ) {
            const auto count = proj_list_get_count(list);
            if( count == 1 ) {
                auto nonDeprecated =
                    proj_list_get(d->getPROJContext(), list, 0);
                if( nonDeprecated ) {
                    proj_destroy(obj);
                    obj = nonDeprecated;
                }
            }
        }
        proj_list_destroy(list);
    }

    if( bAddTOWGS84 )
    {
        auto boundCRS = proj_crs_create_bound_crs_to_WGS84(
            d->getPROJContext(), obj, nullptr);
        if( boundCRS )
        {
            proj_destroy(obj);
            obj = boundCRS;
        }
    }

    d->setPjCRS(obj);

    if( tlsCache )
    {
        tlsCache->CachePJForEPSGCode(nCode, bUseNonDeprecated, bAddTOWGS84, obj);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AddGuessedTOWGS84()                         */
/************************************************************************/

/**
 * \brief  Try to add a a 3-parameter or 7-parameter Helmert transformation
 * to WGS84.
 *
 * This method try to attach a 3-parameter or 7-parameter Helmert transformation
 * to WGS84 when there is one and only one such method available for the CRS.
 * Note: this is more restrictive to how GDAL < 3 worked.
 *
 * This method is the same as the C function OSRAddGuessedTOWGS84().
 *
 * @return OGRERR_NONE on success, or an error code on failure (the CRS has
 * already a transformation to WGS84 or none matching could be found).
 *
 * @since GDAL 3.0.3
 */
OGRErr OGRSpatialReference::AddGuessedTOWGS84()
{
    d->refreshProjObj();
    if( !d->m_pj_crs )
        return OGRERR_FAILURE;
    auto boundCRS = GDAL_proj_crs_create_bound_crs_to_WGS84(
        d->getPROJContext(), d->m_pj_crs, false, true);
    if( !boundCRS )
    {
        return OGRERR_FAILURE;
    }
    d->setPjCRS(boundCRS);
    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRImportFromEPSGA()                         */
/************************************************************************/

/**
 * \brief  Try to add a a 3-parameter or 7-parameter Helmert transformation
 * to WGS84.
 *
 * This function is the same as OGRSpatialReference::AddGuessedTOWGS84().
 *
 * @since GDAL 3.0.3
 */

OGRErr OSRAddGuessedTOWGS84( OGRSpatialReferenceH hSRS)

{
    VALIDATE_POINTER1( hSRS, "OSRAddGuessedTOWGS84", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->AddGuessedTOWGS84();
}

/************************************************************************/
/*                         OSRImportFromEPSGA()                         */
/************************************************************************/

/**
 * \brief  Initialize SRS based on EPSG geographic, projected or vertical CRS code.
 *
 * This function is the same as OGRSpatialReference::importFromEPSGA().
 */

OGRErr CPL_STDCALL OSRImportFromEPSGA( OGRSpatialReferenceH hSRS, int nCode )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromEPSGA", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->
        importFromEPSGA( nCode );
}

/************************************************************************/
/*                           importFromEPSG()                           */
/************************************************************************/

/**
 * \brief  Initialize SRS based on EPSG geographic, projected or vertical CRS code.
 *
 * This method will initialize the spatial reference based on the
 * passed in EPSG CRS code found in the PROJ database.
 *
 * This method is the same as the C function OSRImportFromEPSG().
 *
 * This method try to attach a 3-parameter or 7-parameter Helmert transformation
 * to WGS84 when there is one and only one such method available for the CRS.
 * This behavior might not always be desirable, so starting with GDAL 3.0.3,
 * the OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG configuration option can be set to
 * NO to disable this behavior.
 *
 * @param nCode a GCS or PCS code from the horizontal coordinate system table.
 *
 * @return OGRERR_NONE on success, or an error code on failure.
 */

OGRErr OGRSpatialReference::importFromEPSG( int nCode )

{
    return importFromEPSGA( nCode );
}

/************************************************************************/
/*                         OSRImportFromEPSG()                          */
/************************************************************************/

/**
 * \brief  Initialize SRS based on EPSG geographic, projected or vertical CRS code.
 *
 * This function is the same as OGRSpatialReference::importFromEPSG().
 */

OGRErr CPL_STDCALL OSRImportFromEPSG( OGRSpatialReferenceH hSRS, int nCode )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromEPSG", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->
        importFromEPSG( nCode );
}

/************************************************************************/
/*                        EPSGTreatsAsLatLong()                         */
/************************************************************************/

/**
 * \brief This method returns TRUE if EPSG feels this geographic coordinate
 * system should be treated as having lat/long coordinate ordering.
 *
 * Currently this returns TRUE for all geographic coordinate systems
 * with an EPSG code set, and axes set defining it as lat, long.
 *
 * \note Important change of behavior since GDAL 3.0. In previous versions,
 * geographic CRS imported with importFromEPSG() would cause this method to
 * return FALSE on them, whereas now it returns TRUE, since importFromEPSG()
 * is now equivalent to importFromEPSGA().
 *
 * FALSE will be returned for all coordinate systems that are not geographic,
 * or that do not have an EPSG code set.
 *
 * This method is the same as the C function OSREPSGTreatsAsLatLong().
 *
 * @return TRUE or FALSE.
 */

int OGRSpatialReference::EPSGTreatsAsLatLong() const

{
    if( !IsGeographic() )
        return FALSE;

    d->demoteFromBoundCRS();
    const char* pszAuth = proj_get_id_auth_name(d->m_pj_crs, 0);
    if( pszAuth == nullptr || !EQUAL(pszAuth, "EPSG") )
    {
        d->undoDemoteFromBoundCRS();
        return FALSE;
    }

    bool ret = false;
    if ( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        auto horizCRS = proj_crs_get_sub_crs(d->getPROJContext(),
                                                d->m_pj_crs, 0);
        if ( horizCRS )
        {
            auto cs = proj_crs_get_coordinate_system(d->getPROJContext(),
                                                        horizCRS);
            if ( cs )
            {
                const char* pszDirection = nullptr;
                if( proj_cs_get_axis_info(
                    d->getPROJContext(), cs, 0, nullptr, nullptr,
                    &pszDirection, nullptr, nullptr, nullptr, nullptr) )
                {
                    if( EQUAL(pszDirection, "north") )
                    {
                        ret = true;
                    }
                }

                proj_destroy(cs);
            }

            proj_destroy(horizCRS);
        }
    }
    else
    {
        auto cs = proj_crs_get_coordinate_system(d->getPROJContext(),
                                                    d->m_pj_crs);
        if ( cs )
        {
            const char* pszDirection = nullptr;
            if( proj_cs_get_axis_info(
                d->getPROJContext(), cs, 0, nullptr, nullptr, &pszDirection,
                nullptr, nullptr, nullptr, nullptr) )
            {
                if( EQUAL(pszDirection, "north") )
                {
                    ret = true;
                }
            }

            proj_destroy(cs);
        }
    }
    d->undoDemoteFromBoundCRS();

    return ret;
}

/************************************************************************/
/*                       OSREPSGTreatsAsLatLong()                       */
/************************************************************************/

/**
 * \brief This function returns TRUE if EPSG feels this geographic coordinate
 * system should be treated as having lat/long coordinate ordering.
 *
 * This function is the same as OGRSpatialReference::OSREPSGTreatsAsLatLong().
 */

int OSREPSGTreatsAsLatLong( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSREPSGTreatsAsLatLong", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->EPSGTreatsAsLatLong();
}

/************************************************************************/
/*                     EPSGTreatsAsNorthingEasting()                    */
/************************************************************************/

/**
 * \brief This method returns TRUE if EPSG feels this projected coordinate
 * system should be treated as having northing/easting coordinate ordering.
 *
 * Currently this returns TRUE for all projected coordinate systems
 * with an EPSG code set, and axes set defining it as northing, easting.
 *
 * \note Important change of behavior since GDAL 3.0. In previous versions,
 * projected CRS with northing, easting axis order imported with
 * importFromEPSG() would cause this method to
 * return FALSE on them, whereas now it returns TRUE, since importFromEPSG()
 * is now equivalent to importFromEPSGA().
 *
 * FALSE will be returned for all coordinate systems that are not projected,
 * or that do not have an EPSG code set.
 *
 * This method is the same as the C function EPSGTreatsAsNorthingEasting().
 *
 * @return TRUE or FALSE.
 *
 * @since OGR 1.10.0
 */

int OGRSpatialReference::EPSGTreatsAsNorthingEasting() const

{
    if( !IsProjected() )
        return FALSE;

    d->demoteFromBoundCRS();
    PJ* projCRS;
    const auto ctxt = d->getPROJContext();
    if( d->m_pjType == PJ_TYPE_COMPOUND_CRS )
    {
        projCRS = proj_crs_get_sub_crs(
            ctxt, d->m_pj_crs, 1);
        if( !projCRS || proj_get_type(projCRS) != PJ_TYPE_PROJECTED_CRS )
        {
            d->undoDemoteFromBoundCRS();
            proj_destroy(projCRS);
            return FALSE;
        }
    }
    else
    {
        projCRS = proj_clone(ctxt, d->m_pj_crs);
    }
    const char* pszAuth = proj_get_id_auth_name(projCRS, 0);
    if( pszAuth == nullptr || !EQUAL(pszAuth, "EPSG") )
    {
        d->undoDemoteFromBoundCRS();
        proj_destroy(projCRS);
        return FALSE;
    }

    bool ret = false;
    auto cs = proj_crs_get_coordinate_system(ctxt, projCRS);
    proj_destroy(projCRS);
    d->undoDemoteFromBoundCRS();

    if( cs )
    {
        ret = isNorthEastAxisOrder(ctxt, cs);
        proj_destroy(cs);
    }

    return ret;
}

/************************************************************************/
/*                     OSREPSGTreatsAsNorthingEasting()                 */
/************************************************************************/

/**
 * \brief This function returns TRUE if EPSG feels this projected coordinate
 * system should be treated as having northing/easting coordinate ordering.
 *
 * This function is the same as
 * OGRSpatialReference::EPSGTreatsAsNorthingEasting().
 *
 * @since OGR 1.10.0
 */

int OSREPSGTreatsAsNorthingEasting( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSREPSGTreatsAsNorthingEasting", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->
        EPSGTreatsAsNorthingEasting();
}

/************************************************************************/
/*                     ImportFromESRIWisconsinWKT()                     */
/*                                                                      */
/*      Search a ESRI State Plane WKT and import it.                    */
/************************************************************************/

// This is only used by the HFA driver and somewhat dubious we really need that
// Coming from an old ESRI merge

OGRErr OGRSpatialReference::ImportFromESRIWisconsinWKT(
    const char* prjName, double centralMeridian, double latOfOrigin,
    const char* unitsName, const char* crsName )
{
    if( centralMeridian < -93 || centralMeridian > -87 )
        return OGRERR_FAILURE;
    if( latOfOrigin < 40 || latOfOrigin > 47 )
        return OGRERR_FAILURE;

    // If the CS name is known.
    if( !prjName && !unitsName && crsName )
    {
        const PJ_TYPE type = PJ_TYPE_PROJECTED_CRS;
        PJ_OBJ_LIST* list = proj_create_from_name(
            d->getPROJContext(), "ESRI", crsName, &type, 1, false, 1, nullptr);
        if( list )
        {
            if( proj_list_get_count(list) == 1 )
            {
                auto crs = proj_list_get(d->getPROJContext(), list, 0);
                if( crs )
                {
                    Clear();
                    d->setPjCRS(crs);
                    proj_list_destroy(list);
                    return OGRERR_NONE;
                }
            }
            proj_list_destroy(list);
        }
        return OGRERR_FAILURE;
    }

    if( prjName == nullptr || unitsName == nullptr )
    {
        return OGRERR_FAILURE;
    }

    const PJ_TYPE type = PJ_TYPE_PROJECTED_CRS;
    PJ_OBJ_LIST* list = proj_create_from_name(
        d->getPROJContext(), "ESRI", "NAD_1983_HARN_WISCRS_", &type, 1, true,
        0, nullptr);
    if( list )
    {
        const auto listSize = proj_list_get_count(list);
        for( int i = 0; i < listSize; i++ )
        {
            auto crs = proj_list_get(d->getPROJContext(), list, i);
            if( !crs )
            {
                continue;
            }

            auto conv = proj_crs_get_coordoperation(
                d->getPROJContext(), crs);
            if( !conv )
            {
                proj_destroy(crs);
                continue;
            }
            const char* pszMethodCode = nullptr;
            proj_coordoperation_get_method_info(
                d->getPROJContext(), conv, nullptr, nullptr, &pszMethodCode);
            const int nMethodCode = atoi(pszMethodCode ? pszMethodCode : "0");
            if( !((EQUAL(prjName, SRS_PT_TRANSVERSE_MERCATOR) &&
                   nMethodCode == EPSG_CODE_METHOD_TRANSVERSE_MERCATOR) ||
                  (EQUAL(prjName, "Lambert_Conformal_Conic") &&
                   nMethodCode == EPSG_CODE_METHOD_LAMBERT_CONIC_CONFORMAL_1SP)) )
            {
                proj_destroy(crs);
                proj_destroy(conv);
                continue;
            }

            auto coordSys = proj_crs_get_coordinate_system(
                d->getPROJContext(), crs);
            if( !coordSys )
            {
                proj_destroy(crs);
                proj_destroy(conv);
                continue;
            }

            double dfConvFactor = 0.0;
            proj_cs_get_axis_info(
                d->getPROJContext(), coordSys, 0, nullptr, nullptr, nullptr,
                &dfConvFactor, nullptr, nullptr, nullptr);
            proj_destroy(coordSys);

            if( (EQUAL(unitsName, "meters") && dfConvFactor != 1.0) ||
                (!EQUAL(unitsName, "meters") &&
                 std::fabs(dfConvFactor - CPLAtof(SRS_UL_US_FOOT_CONV)) > 1e-10 ) )
            {
                proj_destroy(crs);
                proj_destroy(conv);
                continue;
            }

            int idx_lat = proj_coordoperation_get_param_index(
                d->getPROJContext(), conv,
                EPSG_NAME_PARAMETER_LATITUDE_OF_NATURAL_ORIGIN);
            double valueLat = -1000;
            proj_coordoperation_get_param(
                d->getPROJContext(), conv, idx_lat,
                nullptr, nullptr, nullptr,
                &valueLat, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            int idx_lon  = proj_coordoperation_get_param_index(
                d->getPROJContext(), conv,
                EPSG_NAME_PARAMETER_LONGITUDE_OF_NATURAL_ORIGIN);
            double valueLong = -1000;
            proj_coordoperation_get_param(
                d->getPROJContext(), conv, idx_lon,
                nullptr, nullptr, nullptr,
                &valueLong, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if( std::fabs(centralMeridian - valueLong) <= 1e-10 &&
                std::fabs(latOfOrigin - valueLat) <= 1e-10 )
            {
                Clear();
                d->setPjCRS(crs);
                proj_list_destroy(list);
                proj_destroy(conv);
                return OGRERR_NONE;
            }

            proj_destroy(crs);
            proj_destroy(conv);
        }
        proj_list_destroy(list);
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                      GetAxisMappingStrategy()                        */
/************************************************************************/

/** \brief Return the data axis to CRS axis mapping strategy.
 *
 * <ul>
 * <li>OAMS_TRADITIONAL_GIS_ORDER means that for geographic CRS with
 *     lat/long order, the data will still be long/lat ordered. Similarly for
 *     a projected CRS with northing/easting order, the data will still be
 *     easting/northing ordered.
 * <li>OAMS_AUTHORITY_COMPLIANT means that the data axis will be identical to
 *     the CRS axis.
 * <li>OAMS_CUSTOM means that the data axis are customly defined with
 *     SetDataAxisToSRSAxisMapping()
 * </ul>
 * @return the the data axis to CRS axis mapping strategy.
 * @since GDAL 3.0
 */
OSRAxisMappingStrategy OGRSpatialReference::GetAxisMappingStrategy() const
{
    return d->m_axisMappingStrategy;
}

/************************************************************************/
/*                      OSRGetAxisMappingStrategy()                     */
/************************************************************************/

/** \brief Return the data axis to CRS axis mapping strategy.
 *
 * See OGRSpatialReference::GetAxisMappingStrategy()
 * @since GDAL 3.0
 */
OSRAxisMappingStrategy OSRGetAxisMappingStrategy( OGRSpatialReferenceH hSRS )
{
    VALIDATE_POINTER1( hSRS, "OSRGetAxisMappingStrategy", OAMS_CUSTOM );

    return OGRSpatialReference::FromHandle(hSRS)->GetAxisMappingStrategy();
}

/************************************************************************/
/*                      SetAxisMappingStrategy()                        */
/************************************************************************/

/** \brief Set the data axis to CRS axis mapping strategy.
 *
 * See OGRSpatialReference::GetAxisMappingStrategy()
 * @since GDAL 3.0
 */
void OGRSpatialReference::SetAxisMappingStrategy(OSRAxisMappingStrategy strategy)
{
    d->m_axisMappingStrategy = strategy;
    d->refreshAxisMapping();
}

/************************************************************************/
/*                      OSRSetAxisMappingStrategy()                     */
/************************************************************************/

/** \brief Set the data axis to CRS axis mapping strategy.
 *
 * See OGRSpatialReference::SetAxisMappingStrategy()
 * @since GDAL 3.0
 */
void OSRSetAxisMappingStrategy( OGRSpatialReferenceH hSRS,
                                OSRAxisMappingStrategy strategy )
{
    VALIDATE_POINTER0( hSRS, "OSRSetAxisMappingStrategy" );

    OGRSpatialReference::FromHandle(hSRS)->SetAxisMappingStrategy(strategy);
}

/************************************************************************/
/*                      GetDataAxisToSRSAxisMapping()                   */
/************************************************************************/

/** \brief Return the data axis to SRS axis mapping.
 *
 * The number of elements of the vector will be the number of axis of the CRS.
 * Values start at 1.
 *
 * If m = GetDataAxisToSRSAxisMapping(), then m[0] is the data axis number
 * for the first axis of the CRS.
 *
 * @since GDAL 3.0
 */
const std::vector<int>& OGRSpatialReference::GetDataAxisToSRSAxisMapping() const
{
    return d->m_axisMapping;
}

/************************************************************************/
/*                     OSRGetDataAxisToSRSAxisMapping()                 */
/************************************************************************/

/** \brief Return the data axis to SRS axis mapping.
 *
 * See OGRSpatialReference::GetDataAxisToSRSAxisMapping()
 *
 * @since GDAL 3.0
 */
const int *OSRGetDataAxisToSRSAxisMapping( OGRSpatialReferenceH hSRS, int* pnCount )
{
    VALIDATE_POINTER1( hSRS, "OSRGetDataAxisToSRSAxisMapping", nullptr );
    VALIDATE_POINTER1( pnCount, "OSRGetDataAxisToSRSAxisMapping", nullptr );

    const auto& v =
        OGRSpatialReference::FromHandle(hSRS)->GetDataAxisToSRSAxisMapping();
    *pnCount = static_cast<int>(v.size());
    return v.data();
}

/************************************************************************/
/*                      SetDataAxisToSRSAxisMapping()                   */
/************************************************************************/

/** \brief Set a custom data axis to CRS axis mapping.
 *
 * Automatically implies SetAxisMappingStrategy(OAMS_CUSTOM)
 *
 * See OGRSpatialReference::GetAxisMappingStrategy()
 * @since GDAL 3.0
 */
OGRErr OGRSpatialReference::SetDataAxisToSRSAxisMapping(const std::vector<int>& mapping)
{
    if( mapping.size() < 2 )
        return OGRERR_FAILURE;
    d->m_axisMappingStrategy = OAMS_CUSTOM;
    d->m_axisMapping = mapping;
    return OGRERR_NONE;
}

/************************************************************************/
/*                     OSRSetDataAxisToSRSAxisMapping()                 */
/************************************************************************/

/** \brief Set a custom data axis to CRS axis mapping.
 *s
 * Automatically implies SetAxisMappingStrategy(OAMS_CUSTOM)
 *
 * See OGRSpatialReference::SetDataAxisToSRSAxisMapping()
 *
 * @since GDAL 3.1
 */
OGRErr OSRSetDataAxisToSRSAxisMapping( OGRSpatialReferenceH hSRS,
                                       int nMappingSize,
                                       const int* panMapping )
{
    VALIDATE_POINTER1( hSRS, "OSRSetDataAxisToSRSAxisMapping", OGRERR_FAILURE );
    VALIDATE_POINTER1( panMapping, "OSRSetDataAxisToSRSAxisMapping", OGRERR_FAILURE );

    if( nMappingSize < 0 )
        return OGRERR_FAILURE;

    std::vector<int> mapping(nMappingSize);
    if( nMappingSize )
        memcpy(&mapping[0], panMapping, nMappingSize * sizeof(int));
    return OGRSpatialReference::FromHandle(hSRS)->SetDataAxisToSRSAxisMapping(mapping);
}

/************************************************************************/
/*                               GetAreaOfUse()                         */
/************************************************************************/

/** \brief Return the area of use of the CRS.
 *
 * This method is the same as the OSRGetAreaOfUse() function.
 *
 * @param pdfWestLongitudeDeg Pointer to a double to receive the western-most
 * longitude, expressed in degree. Might be NULL. If the returned value is -1000,
 * the bounding box is unknown.
 * @param pdfSouthLatitudeDeg Pointer to a double to receive the southern-most
 * latitude, expressed in degree. Might be NULL. If the returned value is -1000,
 * the bounding box is unknown.
 * @param pdfEastLongitudeDeg Pointer to a double to receive the eastern-most
 * longitude, expressed in degree. Might be NULL. If the returned value is -1000,
 * the bounding box is unknown.
 * @param pdfNorthLatitudeDeg Pointer to a double to receive the northern-most
 * latitude, expressed in degree. Might be NULL. If the returned value is -1000,
 * the bounding box is unknown.
 * @param ppszAreaName Pointer to a string to receive the name of the area of
 * use. Might be NULL. Note that *ppszAreaName is short-lived and might be invalidated
 * by further calls.
 * @return true in case of success
 * @since GDAL 3.0
 */
bool OGRSpatialReference::GetAreaOfUse( double* pdfWestLongitudeDeg,
                                        double* pdfSouthLatitudeDeg,
                                        double* pdfEastLongitudeDeg,
                                        double* pdfNorthLatitudeDeg,
                                        const char **ppszAreaName ) const
{
    d->refreshProjObj();
    if( !d->m_pj_crs )
    {
        return false;
    }
    d->demoteFromBoundCRS();
    const char* pszAreaName = nullptr;
    int bSuccess = proj_get_area_of_use(
                          d->getPROJContext(),
                          d->m_pj_crs,
                          pdfWestLongitudeDeg,
                          pdfSouthLatitudeDeg,
                          pdfEastLongitudeDeg,
                          pdfNorthLatitudeDeg,
                          &pszAreaName);
    d->undoDemoteFromBoundCRS();
    d->m_osAreaName = pszAreaName ? pszAreaName : "";
    if( ppszAreaName )
        *ppszAreaName = d->m_osAreaName.c_str();
    return CPL_TO_BOOL(bSuccess);
}

/************************************************************************/
/*                               GetAreaOfUse()                         */
/************************************************************************/

/** \brief Return the area of use of the CRS.
 *
 * This function is the same as the OGRSpatialReference::GetAreaOfUse() method.
 *
 * @since GDAL 3.0
 */
int OSRGetAreaOfUse( OGRSpatialReferenceH hSRS,
                     double* pdfWestLongitudeDeg,
                     double* pdfSouthLatitudeDeg,
                     double* pdfEastLongitudeDeg,
                     double* pdfNorthLatitudeDeg,
                     const char **ppszAreaName )
{
    VALIDATE_POINTER1( hSRS, "OSRGetAreaOfUse", FALSE );

    return OGRSpatialReference::FromHandle(hSRS)->GetAreaOfUse(
        pdfWestLongitudeDeg, pdfSouthLatitudeDeg,
        pdfEastLongitudeDeg, pdfNorthLatitudeDeg,
        ppszAreaName);
}

/************************************************************************/
/*                     OSRGetCRSInfoListFromDatabase()                  */
/************************************************************************/

/** \brief Enumerate CRS objects from the database.
 *
 * The returned object is an array of OSRCRSInfo* pointers, whose last
 * entry is NULL. This array should be freed with OSRDestroyCRSInfoList()
 *
 * @param pszAuthName Authority name, used to restrict the search.
 * Or NULL for all authorities.
 * @param params Additional criteria. Must be set to NULL for now.
 * @param pnOutResultCount Output parameter pointing to an integer to receive
 * the size of the result list. Might be NULL
 * @return an array of OSRCRSInfo* pointers to be freed with
 * OSRDestroyCRSInfoList(), or NULL in case of error.
 *
 * @since GDAL 3.0
 */
OSRCRSInfo **OSRGetCRSInfoListFromDatabase(
                                      const char *pszAuthName,
                                      CPL_UNUSED const OSRCRSListParameters* params,
                                      int *pnOutResultCount)
{
    int nResultCount = 0;
    auto projList = proj_get_crs_info_list_from_database(OSRGetProjTLSContext(),
                                         pszAuthName,
                                         nullptr,
                                         &nResultCount);
    if( pnOutResultCount )
        *pnOutResultCount = nResultCount;
    if( !projList )
    {
        return nullptr;
    }
    auto res = new OSRCRSInfo*[nResultCount + 1];
    for( int i = 0; i < nResultCount; i++ )
    {
        res[i] = new OSRCRSInfo;
        res[i]->pszAuthName = projList[i]->auth_name ?
            CPLStrdup(projList[i]->auth_name) : nullptr;
        res[i]->pszCode = projList[i]->code ?
            CPLStrdup(projList[i]->code) : nullptr;
        res[i]->pszName = projList[i]->name ?
            CPLStrdup(projList[i]->name) : nullptr;
        res[i]->eType = OSR_CRS_TYPE_OTHER;
        switch(  projList[i]->type )
        {
            case PJ_TYPE_GEOGRAPHIC_2D_CRS:
                res[i]->eType = OSR_CRS_TYPE_GEOGRAPHIC_2D; break;
            case PJ_TYPE_GEOGRAPHIC_3D_CRS:
                res[i]->eType = OSR_CRS_TYPE_GEOGRAPHIC_3D; break;
            case PJ_TYPE_GEOCENTRIC_CRS:
                res[i]->eType = OSR_CRS_TYPE_GEOCENTRIC; break;
            case PJ_TYPE_PROJECTED_CRS:
                res[i]->eType = OSR_CRS_TYPE_PROJECTED; break;
            case PJ_TYPE_VERTICAL_CRS:
                res[i]->eType = OSR_CRS_TYPE_VERTICAL; break;
            case PJ_TYPE_COMPOUND_CRS:
                res[i]->eType = OSR_CRS_TYPE_COMPOUND; break;
            default:
                break;
        }
        res[i]->bDeprecated = projList[i]->deprecated;
        res[i]->bBboxValid = projList[i]->bbox_valid;
        res[i]->dfWestLongitudeDeg = projList[i]->west_lon_degree;
        res[i]->dfSouthLatitudeDeg = projList[i]->south_lat_degree;
        res[i]->dfEastLongitudeDeg = projList[i]->east_lon_degree;
        res[i]->dfNorthLatitudeDeg = projList[i]->north_lat_degree;
        res[i]->pszAreaName = projList[i]->area_name ?
        CPLStrdup(projList[i]->area_name) : nullptr;
        res[i]->pszProjectionMethod = projList[i]->projection_method_name ?
            CPLStrdup(projList[i]->projection_method_name) : nullptr;
    }
    res[nResultCount] = nullptr;
    proj_crs_info_list_destroy(projList);
    return res;
}

/************************************************************************/
/*                        OSRDestroyCRSInfoList()                       */
/************************************************************************/


/** \brief Destroy the result returned by
 * OSRGetCRSInfoListFromDatabase().
 *
 * @since GDAL 3.0
 */
void OSRDestroyCRSInfoList(OSRCRSInfo** list)
{
    if (list)
    {
        for (int i = 0; list[i] != nullptr; i++)
        {
            CPLFree(list[i]->pszAuthName);
            CPLFree(list[i]->pszCode);
            CPLFree(list[i]->pszName);
            CPLFree(list[i]->pszAreaName);
            CPLFree(list[i]->pszProjectionMethod);
            delete list[i];
        }
        delete[] list;
    }
}

/************************************************************************/
/*                    UpdateCoordinateSystemFromGeogCRS()               */
/************************************************************************/

/*! @cond Doxygen_Suppress */
/** \brief Used by gt_wkt_srs.cpp to create projected 3D CRS. Internal use only
 *
 * @since GDAL 3.1
 */
void OGRSpatialReference::UpdateCoordinateSystemFromGeogCRS()
{
    d->refreshProjObj();
    if( !d->m_pj_crs )
        return;
    if( d->m_pjType != PJ_TYPE_PROJECTED_CRS )
        return;
    if( GetAxesCount() == 3 )
        return;
    auto ctxt = d->getPROJContext();
    auto baseCRS = proj_crs_get_geodetic_crs(ctxt, d->m_pj_crs);
    if( !baseCRS )
        return;
    auto baseCRSCS = proj_crs_get_coordinate_system(ctxt, baseCRS);
    if( !baseCRSCS )
    {
        proj_destroy(baseCRS);
        return;
    }
    if( proj_cs_get_axis_count(ctxt, baseCRSCS) != 3 )
    {
        proj_destroy(baseCRSCS);
        proj_destroy(baseCRS);
        return;
    }
    auto projCS = proj_crs_get_coordinate_system(ctxt, d->m_pj_crs);
    if( !projCS || proj_cs_get_axis_count(ctxt, projCS) != 2 )
    {
        proj_destroy(baseCRSCS);
        proj_destroy(baseCRS);
        proj_destroy(projCS);
        return;
    }

    PJ_AXIS_DESCRIPTION axis[3];
    for( int i = 0; i < 3; i++ )
    {
        const char* name = nullptr;
        const char* abbreviation = nullptr;
        const char* direction = nullptr;
        double unit_conv_factor = 0;
        const char* unit_name = nullptr;
        proj_cs_get_axis_info(ctxt,
                              i < 2 ? projCS : baseCRSCS,
                              i,
                              &name,
                              &abbreviation,
                              &direction,
                              &unit_conv_factor,
                              &unit_name, nullptr, nullptr);
        axis[i].name = CPLStrdup(name);
        axis[i].abbreviation = CPLStrdup(abbreviation);
        axis[i].direction = CPLStrdup(direction);
        axis[i].unit_name = CPLStrdup(unit_name);
        axis[i].unit_conv_factor = unit_conv_factor;
        axis[i].unit_type = PJ_UT_LINEAR;
    }
    proj_destroy(baseCRSCS);
    proj_destroy(projCS);
    auto cs = proj_create_cs(ctxt, PJ_CS_TYPE_CARTESIAN, 3, axis);
    for( int i = 0; i < 3; i++ )
    {
        CPLFree(axis[i].name);
        CPLFree(axis[i].abbreviation);
        CPLFree(axis[i].direction);
        CPLFree(axis[i].unit_name);
    }
    if( !cs )
    {
        proj_destroy(baseCRS);
        return;
    }
    auto conversion = proj_crs_get_coordoperation(ctxt, d->m_pj_crs);
    auto crs = proj_create_projected_crs(ctxt,
                                         d->getProjCRSName(),
                                         baseCRS,
                                         conversion,
                                         cs);
    proj_destroy(baseCRS);
    proj_destroy(conversion);
    proj_destroy(cs);
    d->setPjCRS(crs);
}

/*! @endcond */

/************************************************************************/
/*                             PromoteTo3D()                            */
/************************************************************************/

/** \brief "Promotes" a 2D CRS to a 3D CRS one.
 *
 * The new axis will be ellipsoidal height, oriented upwards, and with metre
 * units.
 *
 * @param pszName New name for the CRS. If set to NULL, the previous name will be used.
 * @return OGRERR_NONE if no error occurred.
 * @since GDAL 3.1 and PROJ 6.3
 */
OGRErr OGRSpatialReference::PromoteTo3D(const char* pszName)
{
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
    d->refreshProjObj();
    if( !d->m_pj_crs )
        return OGRERR_FAILURE;
    auto newPj = proj_crs_promote_to_3D( d->getPROJContext(), pszName, d->m_pj_crs );
    if( !newPj )
        return OGRERR_FAILURE;
    d->setPjCRS(newPj);
    return OGRERR_NONE;
#else
    CPL_IGNORE_RET_VAL(pszName);
    CPLError(CE_Failure, CPLE_NotSupported, "PROJ 6.3 required");
    return OGRERR_UNSUPPORTED_OPERATION;
#endif
}

/************************************************************************/
/*                             OSRPromoteTo3D()                         */
/************************************************************************/

/** \brief "Promotes" a 2D CRS to a 3D CRS one.
 *
 * See OGRSpatialReference::PromoteTo3D()
 *
 * @since GDAL 3.1 and PROJ 6.3
 */
OGRErr OSRPromoteTo3D( OGRSpatialReferenceH hSRS, const char* pszName  )
{
    VALIDATE_POINTER1( hSRS, "OSRPromoteTo3D", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->PromoteTo3D(pszName);
}

/************************************************************************/
/*                             DemoteTo2D()                             */
/************************************************************************/

/** \brief "Demote" a 3D CRS to a 2D CRS one.
 *
 * @param pszName New name for the CRS. If set to NULL, the previous name will be used.
 * @return OGRERR_NONE if no error occurred.
 * @since GDAL 3.2 and PROJ 6.3
 */
OGRErr OGRSpatialReference::DemoteTo2D(const char* pszName)
{
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
    d->refreshProjObj();
    if( !d->m_pj_crs )
        return OGRERR_FAILURE;
    auto newPj = proj_crs_demote_to_2D( d->getPROJContext(), pszName, d->m_pj_crs );
    if( !newPj )
        return OGRERR_FAILURE;
    d->setPjCRS(newPj);
    return OGRERR_NONE;
#else
    CPL_IGNORE_RET_VAL(pszName);
    CPLError(CE_Failure, CPLE_NotSupported, "PROJ 6.3 required");
    return OGRERR_UNSUPPORTED_OPERATION;
#endif
}

/************************************************************************/
/*                             OSRDemoteTo2D()                          */
/************************************************************************/

/** \brief "Demote" a 3D CRS to a 2D CRS one.
 *
 * See OGRSpatialReference::DemoteTo2D()
 *
 * @since GDAL 3.2 and PROJ 6.3
 */
OGRErr OSRDemoteTo2D( OGRSpatialReferenceH hSRS, const char* pszName  )
{
    VALIDATE_POINTER1( hSRS, "OSRDemoteTo2D", OGRERR_FAILURE );

    return OGRSpatialReference::FromHandle(hSRS)->DemoteTo2D(pszName);
}

/************************************************************************/
/*                           GetEPSGGeogCS()                            */
/************************************************************************/

/** Try to establish what the EPSG code for this coordinate systems
 * GEOGCS might be.  Returns -1 if no reasonable guess can be made.
 *
 * @return EPSG code
 */

int OGRSpatialReference::GetEPSGGeogCS() const

{
/* -------------------------------------------------------------------- */
/*      Check axis order.                                               */
/* -------------------------------------------------------------------- */
    auto poGeogCRS = std::unique_ptr<OGRSpatialReference>(CloneGeogCS());
    if( !poGeogCRS )
        return -1;

    bool ret = false;
    poGeogCRS->d->demoteFromBoundCRS();
    auto cs = proj_crs_get_coordinate_system(d->getPROJContext(),
                                             poGeogCRS->d->m_pj_crs);
    poGeogCRS->d->undoDemoteFromBoundCRS();
    if( cs )
    {
        const char* pszDirection = nullptr;
        if( proj_cs_get_axis_info(
            d->getPROJContext(), cs, 0, nullptr, nullptr, &pszDirection,
            nullptr, nullptr, nullptr, nullptr) )
        {
            if( EQUAL(pszDirection, "north") )
            {
                ret = true;
            }
        }

        proj_destroy(cs);
    }
    if( !ret )
        return -1;

/* -------------------------------------------------------------------- */
/*      Do we already have it?                                          */
/* -------------------------------------------------------------------- */
    const char *pszAuthName = GetAuthorityName( "GEOGCS" );
    if( pszAuthName != nullptr && EQUAL(pszAuthName, "epsg") )
        return atoi(GetAuthorityCode( "GEOGCS" ));

/* -------------------------------------------------------------------- */
/*      Get the datum and geogcs names.                                 */
/* -------------------------------------------------------------------- */

    const char *pszGEOGCS = GetAttrValue( "GEOGCS" );
    const char *pszDatum = GetAttrValue( "DATUM" );

    // We can only operate on coordinate systems with a geogcs.
    OGRSpatialReference oSRSTmp;
    if( pszGEOGCS == nullptr || pszDatum == nullptr )
    {
        // Calling GetAttrValue("GEOGCS") will fail on a CRS that can't be
        // export to WKT1, so try to extract the geographic CRS through PROJ
        // API with CopyGeogCSFrom() and get the nodes' values from it.
        oSRSTmp.CopyGeogCSFrom(this);
        pszGEOGCS = oSRSTmp.GetAttrValue( "GEOGCS" );
        pszDatum = oSRSTmp.GetAttrValue( "DATUM" );
        if( pszGEOGCS == nullptr || pszDatum == nullptr )
        {
            return -1;
        }
    }

    // Lookup geographic CRS name
    const PJ_TYPE type = PJ_TYPE_GEOGRAPHIC_2D_CRS;
    PJ_OBJ_LIST* list = proj_create_from_name(d->getPROJContext(), nullptr,
                                              pszGEOGCS,
                                              &type, 1,
                                              false,
                                              1,
                                              nullptr);
    if( list )
    {
        const auto listSize = proj_list_get_count(list);
        if( listSize == 1 )
        {
            auto crs = proj_list_get(d->getPROJContext(), list, 0);
            if( crs )
            {
                pszAuthName = proj_get_id_auth_name(crs, 0);
                const char* pszCode = proj_get_id_code(crs, 0);
                if( pszAuthName && pszCode && EQUAL(pszAuthName, "EPSG") )
                {
                    const int nCode = atoi(pszCode);
                    proj_destroy(crs);
                    proj_list_destroy(list);
                    return nCode;
                }
                proj_destroy(crs);
            }
        }
        proj_list_destroy(list);
    }

/* -------------------------------------------------------------------- */
/*      Is this a "well known" geographic coordinate system?            */
/* -------------------------------------------------------------------- */
    const bool bWGS = strstr(pszGEOGCS, "WGS") != nullptr
        || strstr(pszDatum, "WGS")
        || strstr(pszGEOGCS, "World Geodetic System")
        || strstr(pszGEOGCS, "World_Geodetic_System")
        || strstr(pszDatum, "World Geodetic System")
        || strstr(pszDatum, "World_Geodetic_System");

    const bool bNAD = strstr(pszGEOGCS, "NAD") != nullptr
        || strstr(pszDatum, "NAD")
        || strstr(pszGEOGCS, "North American")
        || strstr(pszGEOGCS, "North_American")
        || strstr(pszDatum, "North American")
        || strstr(pszDatum, "North_American");

    if( bWGS && (strstr(pszGEOGCS, "84") || strstr(pszDatum, "84")) )
        return 4326;

    if( bWGS && (strstr(pszGEOGCS, "72") || strstr(pszDatum, "72")) )
        return 4322;

    // This is questionable as there are several 'flavors' of NAD83 that
    // are not the same as 4269
    if( bNAD && (strstr(pszGEOGCS, "83") || strstr(pszDatum, "83")) )
        return 4269;

    if( bNAD && (strstr(pszGEOGCS, "27") || strstr(pszDatum, "27")) )
        return 4267;

/* -------------------------------------------------------------------- */
/*      If we know the datum, associate the most likely GCS with        */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    const OGRSpatialReference& oActiveObj = oSRSTmp.IsEmpty() ? *this : oSRSTmp;
    pszAuthName = oActiveObj.GetAuthorityName( "GEOGCS|DATUM" );
    if( pszAuthName != nullptr
        && EQUAL(pszAuthName, "epsg")
        && GetPrimeMeridian() == 0.0 )
    {
        const int nDatum = atoi(oActiveObj.GetAuthorityCode("GEOGCS|DATUM"));

        if( nDatum >= 6000 && nDatum <= 6999 )
            return nDatum - 2000;
    }

    return -1;
}

/************************************************************************/
/*                          SetCoordinateEpoch()                        */
/************************************************************************/

/** Set the coordinate epoch, as decimal year.
 *
 * In a dynamic CRS, coordinates of a point on the surface of the Earth may
 * change with time. To be unambiguous the coordinates must always be qualified
 * with the epoch at which they are valid. The coordinate epoch is not necessarily
 * the epoch at which the observation was collected.
 *
 * Pedantically the coordinate epoch of an observation belongs to the
 * observation, and not to the CRS, however it is often more practical to
 * bind it to the CRS. The coordinate epoch should be specified for dynamic
 * CRS (see IsDynamic())
 *
 * This method is the same as the OSRSetCoordinateEpoch() function.
 *
 * @param dfCoordinateEpoch Coordinate epoch as decimal year (e.g. 2021.3)
 * @since OGR 3.4
 */

void OGRSpatialReference::SetCoordinateEpoch( double dfCoordinateEpoch )
{
    d->m_coordinateEpoch = dfCoordinateEpoch;
}

/************************************************************************/
/*                      OSRSetCoordinateEpoch()                         */
/************************************************************************/

/** \brief Set the coordinate epoch, as decimal year.
 *
 * See OGRSpatialReference::SetCoordinateEpoch()
 *
 * @since OGR 3.4
 */
void OSRSetCoordinateEpoch( OGRSpatialReferenceH hSRS, double dfCoordinateEpoch )
{
    VALIDATE_POINTER0( hSRS, "OSRSetCoordinateEpoch" );

    return OGRSpatialReference::FromHandle(hSRS)->SetCoordinateEpoch(dfCoordinateEpoch);
}

/************************************************************************/
/*                          GetCoordinateEpoch()                        */
/************************************************************************/

/** Return the coordinate epoch, as decimal year.
 *
 * In a dynamic CRS, coordinates of a point on the surface of the Earth may
 * change with time. To be unambiguous the coordinates must always be qualified
 * with the epoch at which they are valid. The coordinate epoch is not necessarily
 * the epoch at which the observation was collected.
 *
 * Pedantically the coordinate epoch of an observation belongs to the
 * observation, and not to the CRS, however it is often more practical to
 * bind it to the CRS. The coordinate epoch should be specified for dynamic
 * CRS (see IsDynamic())
 *
 * This method is the same as the OSRGetCoordinateEpoch() function.
 *
 * @return coordinateEpoch Coordinate epoch as decimal year (e.g. 2021.3), or 0
 *                         if not set, or relevant.
 * @since OGR 3.4
 */

double OGRSpatialReference::GetCoordinateEpoch() const
{
    return d->m_coordinateEpoch;
}

/************************************************************************/
/*                      OSRGetCoordinateEpoch()                        */
/************************************************************************/

/** \brief Get the coordinate epoch, as decimal year.
 *
 * See OGRSpatialReference::GetCoordinateEpoch()
 *
 * @since OGR 3.4
 */
double OSRGetCoordinateEpoch( OGRSpatialReferenceH hSRS )
{
    VALIDATE_POINTER1( hSRS, "OSRGetCoordinateEpoch", 0 );

    return OGRSpatialReference::FromHandle(hSRS)->GetCoordinateEpoch();
}
