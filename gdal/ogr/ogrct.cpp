/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSCoordinateTransformation class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <list>
#include <mutex>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_mem_cache.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"
#include "ogr_proj_p.h"

#include "proj.h"
#include "proj_experimental.h"

CPL_CVSID("$Id$")

#ifdef DEBUG_PERF
static double g_dfTotalTimeCRStoCRS = 0;
static double g_dfTotalTimeReprojection = 0;

/************************************************************************/
/*                        CPLGettimeofday()                             */
/************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__)
#  include <sys/timeb.h>

namespace {
struct CPLTimeVal
{
  time_t  tv_sec;         /* seconds */
  long    tv_usec;        /* and microseconds */
};
}

static void CPLGettimeofday(struct CPLTimeVal* tp, void* /* timezonep*/ )
{
  struct _timeb theTime;

  _ftime(&theTime);
  tp->tv_sec = static_cast<time_t>(theTime.time);
  tp->tv_usec = theTime.millitm * 1000;
}
#else
#  include <sys/time.h>     /* for gettimeofday() */
#  define  CPLTimeVal timeval
#  define  CPLGettimeofday(t,u) gettimeofday(t,u)
#endif

#endif // DEBUG_PERF

// Cache of OGRProjCT objects
static std::mutex g_oCTCacheMutex;
class OGRProjCT;
// We wrap a OGRProjCT in a shared_ptr<unique_ptr>, because we need a copyable
// type to be inserted in the cache (shared_ptr), and we need to be able to
// alter the content of the value to release() the unique_ptr value when we
// find a value in it.
// In a future improvement (notably to help for the multi-threaded warping),
// we could let the cached value in the cache and clone it, but for now clone,
// just instantiates a new OGRProjCT from scratch. The clone method should be
// improved to do things similar to GetInverse().
typedef std::string CTCacheKey;
typedef std::shared_ptr<std::unique_ptr<OGRProjCT>> CTCacheValue;
static lru11::Cache<CTCacheKey, CTCacheValue>* g_poCTCache = nullptr;

/************************************************************************/
/*             OGRCoordinateTransformationOptions::Private              */
/************************************************************************/

struct OGRCoordinateTransformationOptions::Private
{
    bool bHasAreaOfInterest = false;
    double dfWestLongitudeDeg = 0.0;
    double dfSouthLatitudeDeg = 0.0;
    double dfEastLongitudeDeg = 0.0;
    double dfNorthLatitudeDeg = 0.0;

    CPLString osCoordOperation{};
    bool bReverseCO = false;

    bool bAllowBallpark = true;
    double dfAccuracy = -1; // no constraint

    bool bHasSourceCenterLong = false;
    double dfSourceCenterLong = 0.0;

    bool bHasTargetCenterLong = false;
    double dfTargetCenterLong = 0.0;

    bool bCheckWithInvertProj = false;

    Private();
    Private(const Private&) = default;
    Private(Private&&) = default;
    Private& operator=(const Private&) = default;
    Private& operator=(Private&&) = default;

    std::string GetKey() const;
    void RefreshCheckWithInvertProj();
};

/************************************************************************/
/*                              Private()                               */
/************************************************************************/

OGRCoordinateTransformationOptions::Private::Private()
{
    RefreshCheckWithInvertProj();
}

/************************************************************************/
/*                              GetKey()                                */
/************************************************************************/

std::string OGRCoordinateTransformationOptions::Private::GetKey() const
{
    std::string ret;
    ret += std::to_string(static_cast<int>(bHasAreaOfInterest));
    ret += std::to_string(dfWestLongitudeDeg);
    ret += std::to_string(dfSouthLatitudeDeg);
    ret += std::to_string(dfEastLongitudeDeg);
    ret += std::to_string(dfNorthLatitudeDeg);
    ret += osCoordOperation;
    ret += std::to_string(static_cast<int>(bReverseCO));
    ret += std::to_string(static_cast<int>(bAllowBallpark));
    ret += std::to_string(dfAccuracy);
    ret += std::to_string(static_cast<int>(bHasSourceCenterLong));
    ret += std::to_string(dfSourceCenterLong);
    ret += std::to_string(static_cast<int>(bHasTargetCenterLong));
    ret += std::to_string(dfTargetCenterLong);
    ret += std::to_string(static_cast<int>(bCheckWithInvertProj));
    return ret;
}

/************************************************************************/
/*                       RefreshCheckWithInvertProj()                   */
/************************************************************************/

void OGRCoordinateTransformationOptions::Private::RefreshCheckWithInvertProj()
{
    bCheckWithInvertProj =
        CPLTestBool(CPLGetConfigOption( "CHECK_WITH_INVERT_PROJ", "NO" ));
}

/************************************************************************/
/*                          GetWktOrProjString()                        */
/************************************************************************/

static char* GetWktOrProjString(const OGRSpatialReference* poSRS)
{
    CPLErrorStateBackuper oErrorStateBackuper;
    CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
    const char* const apszOptionsWKT2_2018[] = { "FORMAT=WKT2_2018", nullptr };
    // If there's a PROJ4 EXTENSION node in WKT1, then use
    // it. For example when dealing with "+proj=longlat +lon_wrap=180"
    char* pszText = nullptr;
    if( poSRS->GetExtension(nullptr, "PROJ4", nullptr) )
    {
        poSRS->exportToProj4(&pszText);
        if (strstr(pszText, " +type=crs") == nullptr )
        {
            auto tmpText = std::string(pszText) + " +type=crs";
            CPLFree(pszText);
            pszText = CPLStrdup(tmpText.c_str());
        }
    }
    else
        poSRS->exportToWkt(&pszText, apszOptionsWKT2_2018);
    return pszText;
}

/************************************************************************/
/*                  OGRCoordinateTransformationOptions()                */
/************************************************************************/

/** \brief Constructs a new OGRCoordinateTransformationOptions.
 *
 * @since GDAL 3.0
 */
OGRCoordinateTransformationOptions::OGRCoordinateTransformationOptions():
    d(new Private())
{
}

/************************************************************************/
/*                  OGRCoordinateTransformationOptions()                */
/************************************************************************/

/** \brief Copy constructor
 *
 * @since GDAL 3.1
 */
OGRCoordinateTransformationOptions::OGRCoordinateTransformationOptions(
    const OGRCoordinateTransformationOptions& other):
        d(new Private(*(other.d)))
{}

/************************************************************************/
/*                          operator =()                                */
/************************************************************************/

/** \brief Assignment operator
 *
 * @since GDAL 3.1
 */
OGRCoordinateTransformationOptions&
    OGRCoordinateTransformationOptions::operator= (const OGRCoordinateTransformationOptions& other)
{
    if( this != &other )
    {
        *d = *(other.d);
    }
    return *this;
}

/************************************************************************/
/*                  OGRCoordinateTransformationOptions()                */
/************************************************************************/

/** \brief Destroys a OGRCoordinateTransformationOptions.
 *
 * @since GDAL 3.0
 */
OGRCoordinateTransformationOptions::~OGRCoordinateTransformationOptions()
{
}

/************************************************************************/
/*                   OCTNewCoordinateTransformationOptions()            */
/************************************************************************/

/** \brief Create coordinate transformation options.
 *
 * To be freed with OCTDestroyCoordinateTransformationOptions()
 *
 * @since GDAL 3.0
 */
OGRCoordinateTransformationOptionsH OCTNewCoordinateTransformationOptions(void)
{
    return new OGRCoordinateTransformationOptions();
}

/************************************************************************/
/*                  OCTDestroyCoordinateTransformationOptions()         */
/************************************************************************/

/** \brief Destroy coordinate transformation options.
 *
 * @since GDAL 3.0
 */
void OCTDestroyCoordinateTransformationOptions(
                            OGRCoordinateTransformationOptionsH hOptions)
{
    delete hOptions;
}

/************************************************************************/
/*                        SetAreaOfInterest()                           */
/************************************************************************/

/** \brief Sets an area of interest.
 *
 * The west longitude is generally lower than the east longitude, except for
 * areas of interest that go across the anti-meridian.
 *
 * @param dfWestLongitudeDeg West longitude (in degree). Must be in [-180,180]
 * @param dfSouthLatitudeDeg South latitude (in degree). Must be in [-90,90]
 * @param dfEastLongitudeDeg East longitude (in degree). Must be in [-180,180]
 * @param dfNorthLatitudeDeg North latitude (in degree). Must be in [-90,90]
 * @return true in case of success.
 *
 * @since GDAL 3.0
 */
bool OGRCoordinateTransformationOptions::SetAreaOfInterest(
        double dfWestLongitudeDeg, double dfSouthLatitudeDeg,
        double dfEastLongitudeDeg, double dfNorthLatitudeDeg)
{
    if( std::fabs(dfWestLongitudeDeg) > 180 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfWestLongitudeDeg");
        return false;
    }
    if( std::fabs(dfSouthLatitudeDeg) > 90 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfSouthLatitudeDeg");
        return false;
    }
    if( std::fabs(dfEastLongitudeDeg) > 180 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfEastLongitudeDeg");
        return false;
    }
    if( std::fabs(dfNorthLatitudeDeg) > 90 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfNorthLatitudeDeg");
        return false;
    }
    if( dfSouthLatitudeDeg > dfNorthLatitudeDeg )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfSouthLatitudeDeg should be lower than dfNorthLatitudeDeg");
        return false;
    }
    d->bHasAreaOfInterest = true;
    d->dfWestLongitudeDeg = dfWestLongitudeDeg;
    d->dfSouthLatitudeDeg = dfSouthLatitudeDeg;
    d->dfEastLongitudeDeg = dfEastLongitudeDeg;
    d->dfNorthLatitudeDeg = dfNorthLatitudeDeg;
    return true;
}

/************************************************************************/
/*           OCTCoordinateTransformationOptionsSetAreaOfInterest()      */
/************************************************************************/

/** \brief Sets an area of interest.
 *
 * See OGRCoordinateTransformationOptions::SetAreaOfInterest()
 *
 * @since GDAL 3.0
 */
int OCTCoordinateTransformationOptionsSetAreaOfInterest(
    OGRCoordinateTransformationOptionsH hOptions,
    double dfWestLongitudeDeg,
    double dfSouthLatitudeDeg,
    double dfEastLongitudeDeg,
    double dfNorthLatitudeDeg)
{
    return hOptions->SetAreaOfInterest(
        dfWestLongitudeDeg, dfSouthLatitudeDeg,
        dfEastLongitudeDeg, dfNorthLatitudeDeg);
}

/************************************************************************/
/*                        SetCoordinateOperation()                      */
/************************************************************************/

/** \brief Sets a coordinate operation.
 *
 * This is a user override to be used instead of the normally computed pipeline.
 *
 * The pipeline must take into account the axis order of the source and target
 * SRS.
 *
 * The pipeline may be provided as a PROJ string (single step operation or
 * multiple step string starting with +proj=pipeline), a WKT2 string describing
 * a CoordinateOperation, or a "urn:ogc:def:coordinateOperation:EPSG::XXXX" URN
 *
 * @param pszCO PROJ or WKT string describing a coordinate operation
 * @param bReverseCO Whether the PROJ or WKT string should be evaluated in the reverse path
 * @return true in case of success.
 *
 * @since GDAL 3.0
 */
bool OGRCoordinateTransformationOptions::SetCoordinateOperation(const char* pszCO, bool bReverseCO)
{
    d->osCoordOperation = pszCO ? pszCO : "";
    d->bReverseCO = bReverseCO;
    return true;
}

/************************************************************************/
/*                         SetSourceCenterLong()                        */
/************************************************************************/

/*! @cond Doxygen_Suppress */
void OGRCoordinateTransformationOptions::SetSourceCenterLong(double dfCenterLong)
{
    d->dfSourceCenterLong = dfCenterLong;
    d->bHasSourceCenterLong = true;
}
/*! @endcond */

/************************************************************************/
/*                         SetTargetCenterLong()                        */
/************************************************************************/

/*! @cond Doxygen_Suppress */
void OGRCoordinateTransformationOptions::SetTargetCenterLong(double dfCenterLong)
{
    d->dfTargetCenterLong = dfCenterLong;
    d->bHasTargetCenterLong = true;
}
/*! @endcond */

/************************************************************************/
/*            OCTCoordinateTransformationOptionsSetOperation()          */
/************************************************************************/

/** \brief Sets a coordinate operation.
 *
 * See OGRCoordinateTransformationOptions::SetCoordinateTransformation()
 *
 * @since GDAL 3.0
 */
int OCTCoordinateTransformationOptionsSetOperation(
    OGRCoordinateTransformationOptionsH hOptions,
    const char* pszCO, int bReverseCO)
{
    return hOptions->SetCoordinateOperation(pszCO, CPL_TO_BOOL(bReverseCO));
}

/************************************************************************/
/*                         SetDesiredAccuracy()                         */
/************************************************************************/

/** \brief Sets the desired accuracy for coordinate operations.
 *
 * Only coordinate operations that offer an accuracy of at least the one
 * specified will be considered.
 *
 * An accuracy of 0 is valid and means a coordinate operation made only of one or
 * several conversions (map projections, unit conversion, etc.)
 * Operations involving ballpark transformations have a unknown accuracy, and
 * will be filtered out by any dfAccuracy >= 0 value.
 *
 * If this option is specified with PROJ < 8, the OGR_CT_OP_SELECTION configuration
 * option will default to BEST_ACCURACY.
 *
 * @param dfAccuracy accuracy in meters (or a negative value to disable this filter)
 *
 * @since GDAL 3.3
 */
bool OGRCoordinateTransformationOptions::SetDesiredAccuracy(double dfAccuracy)
{
    d->dfAccuracy = dfAccuracy;
    return true;
}

/************************************************************************/
/*        OCTCoordinateTransformationOptionsSetDesiredAccuracy()        */
/************************************************************************/

/** \brief Sets the desired accuracy for coordinate operations.
 *
 * See OGRCoordinateTransformationOptions::SetDesiredAccuracy()
 *
 * @since GDAL 3.3
 */
int OCTCoordinateTransformationOptionsSetDesiredAccuracy(
    OGRCoordinateTransformationOptionsH hOptions, double dfAccuracy)
{
    return hOptions->SetDesiredAccuracy(dfAccuracy);
}

/************************************************************************/
/*                       SetBallparkAllowed()                           */
/************************************************************************/

/** \brief Sets whether ballpark transformations are allowed.
 *
 * By default, PROJ may generate "ballpark transformations" (see
 * https://proj.org/glossary.html) when precise datum transformations are missing.
 * For high accuracy use cases, such transformations might not be allowed.
 *
 * If this option is specified with PROJ < 8, the OGR_CT_OP_SELECTION configuration
 * option will default to BEST_ACCURACY.
 *
 * @param bAllowBallpark false to disable the user of ballpark transformations
 *
 * @since GDAL 3.3
 */
bool OGRCoordinateTransformationOptions::SetBallparkAllowed(bool bAllowBallpark)
{
    d->bAllowBallpark = bAllowBallpark;
    return true;
}

/************************************************************************/
/*        OCTCoordinateTransformationOptionsSetBallparkAllowed()        */
/************************************************************************/

/** \brief Sets whether ballpark transformations are allowed.
 *
 * See OGRCoordinateTransformationOptions::SetDesiredAccuracy()
 *
 * @since GDAL 3.3 and PROJ 8
 */
int OCTCoordinateTransformationOptionsSetBallparkAllowed(
    OGRCoordinateTransformationOptionsH hOptions, int bAllowBallpark)
{
    return hOptions->SetBallparkAllowed(CPL_TO_BOOL(bAllowBallpark));
}


/************************************************************************/
/*                              OGRProjCT                               */
/************************************************************************/

//! @cond Doxygen_Suppress
class OGRProjCT : public OGRCoordinateTransformation
{
    OGRSpatialReference *poSRSSource = nullptr;
    bool        bSourceLatLong = false;
    bool        bSourceWrap = false;
    double      dfSourceWrapLong = 0.0;

    OGRSpatialReference *poSRSTarget = nullptr;
    bool        bTargetLatLong = false;
    bool        bTargetWrap = false;
    double      dfTargetWrapLong = 0.0;

    bool        bWebMercatorToWGS84LongLat = false;

    int         nErrorCount = 0;

    double      dfThreshold = 0.0;

    PJ*         m_pj = nullptr;
    bool        m_bReversePj = false;

    bool        m_bEmitErrors = true;

    bool        bNoTransform = false;

    enum class Strategy
    {
        PROJ,
        BEST_ACCURACY,
        FIRST_MATCHING
    };
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 3
    Strategy    m_eStrategy = Strategy::PROJ;
#else
    Strategy    m_eStrategy = Strategy::BEST_ACCURACY;
#endif

    bool        ListCoordinateOperations(const char* pszSrcSRS,
                                         const char* pszTargetSRS,
                                         const OGRCoordinateTransformationOptions& options );

    struct Transformation
    {
        double minx = 0.0;
        double miny = 0.0;
        double maxx = 0.0;
        double maxy = 0.0;
        PJ* pj = nullptr;
        CPLString osName{};
        CPLString osProjString{};
        double accuracy = 0.0;

        Transformation(double minxIn, double minyIn, double maxxIn, double maxyIn,
                       PJ* pjIn,
                       const CPLString& osNameIn,
                       const CPLString& osProjStringIn,
                       double accuracyIn):
            minx(minxIn), miny(minyIn), maxx(maxxIn), maxy(maxyIn),
            pj(pjIn), osName(osNameIn), osProjString(osProjStringIn),
            accuracy(accuracyIn) {}

        Transformation(const Transformation&) = delete;
        Transformation(Transformation&& other):
            minx(other.minx), miny(other.miny), maxx(other.maxx), maxy(other.maxy),
            pj(other.pj), osName(std::move(other.osName)),
            osProjString(std::move(other.osProjString)),
            accuracy(other.accuracy)
        {
            other.pj = nullptr;
        }
        Transformation& operator=(const Transformation&) = delete;

        ~Transformation()
        {
            if( pj )
            {
                proj_assign_context(pj, OSRGetProjTLSContext());
                proj_destroy(pj);
            }
        }
    };
    std::vector<Transformation> m_oTransformations{};
    int m_iCurTransformation = -1;
    OGRCoordinateTransformationOptions m_options{};

    void ComputeThreshold();

    OGRProjCT(const OGRProjCT& other)
    {
        Initialize(other.poSRSSource, other.poSRSTarget, other.m_options);
    }
    OGRProjCT& operator= (const OGRProjCT& ) = delete;

    static CTCacheKey MakeCacheKey(const OGRSpatialReference* poSRS1,
                           const OGRSpatialReference* poSRS2,
                           const OGRCoordinateTransformationOptions& options);

public:
    OGRProjCT();
    ~OGRProjCT() override;

    int         Initialize( const OGRSpatialReference *poSource,
                            const OGRSpatialReference *poTarget,
                            const OGRCoordinateTransformationOptions& options );

    OGRSpatialReference *GetSourceCS() override;
    OGRSpatialReference *GetTargetCS() override;

    int Transform( int nCount,
                             double *x, double *y, double *z, double *t,
                             int *pabSuccess ) override;

    int TransformWithErrorCodes( int nCount,
                             double *x, double *y, double *z, double *t,
                             int *panErrorCodes ) override;

    bool GetEmitErrors() const override { return m_bEmitErrors; }
    void SetEmitErrors( bool bEmitErrors ) override
        { m_bEmitErrors = bEmitErrors; }

    OGRCoordinateTransformation* Clone() const override {
        return new OGRProjCT(*this);
    }

    OGRCoordinateTransformation* GetInverse() const override;

    static void InsertIntoCache( OGRProjCT* poCT );

    static OGRProjCT* FindFromCache( const OGRSpatialReference *poSource,
                                     const OGRSpatialReference *poTarget,
                                     const OGRCoordinateTransformationOptions& options );
};
//! @endcond

/************************************************************************/
/*                 OCTDestroyCoordinateTransformation()                 */
/************************************************************************/

/**
 * \brief OGRCoordinateTransformation destructor.
 *
 * This function is the same as OGRCoordinateTransformation::DestroyCT()
 *
 * @param hCT the object to delete
 */

void CPL_STDCALL
OCTDestroyCoordinateTransformation( OGRCoordinateTransformationH hCT )

{
    OGRCoordinateTransformation::DestroyCT(
        OGRCoordinateTransformation::FromHandle(hCT));
}

/************************************************************************/
/*                             DestroyCT()                              */
/************************************************************************/

/**
 * \brief OGRCoordinateTransformation destructor.
 *
 * This function is the same as
 * OGRCoordinateTransformation::~OGRCoordinateTransformation()
 * and OCTDestroyCoordinateTransformation()
 *
 * This static method will destroy a OGRCoordinateTransformation.  It is
 * equivalent to calling delete on the object, but it ensures that the
 * deallocation is properly executed within the OGR libraries heap on
 * platforms where this can matter (win32).
 *
 * @param poCT the object to delete
 *
 * @since GDAL 1.7.0
 */

void OGRCoordinateTransformation::DestroyCT( OGRCoordinateTransformation* poCT )
{
    auto poProjCT = dynamic_cast<OGRProjCT*>(poCT);
    if( poProjCT )
    {
        OGRProjCT::InsertIntoCache(poProjCT);
    }
    else
    {
        delete poCT;
    }
}

/************************************************************************/
/*                 OGRCreateCoordinateTransformation()                  */
/************************************************************************/

/**
 * Create transformation object.
 *
 * This is the same as the C function OCTNewCoordinateTransformation().
 *
 * Input spatial reference system objects are assigned
 * by copy (calling clone() method) and no ownership transfer occurs.
 *
 * The delete operator, or OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects.
 *
 * This will honour the axis order advertized by the source and target SRS,
 * as well as their "data axis to SRS axis mapping".
 * To have a behavior similar to GDAL &lt; 3.0, the OGR_CT_FORCE_TRADITIONAL_GIS_ORDER
 * configuration option can be set to YES.
 *
 * @param poSource source spatial reference system.
 * @param poTarget target spatial reference system.
 * @return NULL on failure or a ready to use transformation object.
 */

OGRCoordinateTransformation*
OGRCreateCoordinateTransformation( const OGRSpatialReference *poSource,
                                   const OGRSpatialReference *poTarget )

{
    return OGRCreateCoordinateTransformation(
        poSource, poTarget, OGRCoordinateTransformationOptions());
}

/**
 * Create transformation object.
 *
 * This is the same as the C function OCTNewCoordinateTransformationEx().
 *
 * Input spatial reference system objects are assigned
 * by copy (calling clone() method) and no ownership transfer occurs.
 *
 * The delete operator, or OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects.
 *
 * This will honour the axis order advertized by the source and target SRS,
 * as well as their "data axis to SRS axis mapping".
 * To have a behavior similar to GDAL &lt; 3.0, the OGR_CT_FORCE_TRADITIONAL_GIS_ORDER
 * configuration option can be set to YES.
 *
 * The source SRS and target SRS should generally not be NULL. This is only
 * allowed if a custom coordinate operation is set through the hOptions argument.
 *
 * Starting with GDAL 3.0.3, the OGR_CT_OP_SELECTION configuration option can be
 * set to PROJ (default if PROJ >= 6.3), BEST_ACCURACY or FIRST_MATCHING to decide
 * of the strategy to select the operation to use among candidates, whose area of
 * use is compatible with the points to transform. It is only taken into account
 * if no user defined coordinate transformation pipeline has been specified.
 * <ul>
 * <li>PROJ means the default behavior used by PROJ proj_create_crs_to_crs().
 *     In particular the operation to use among several initial candidates is
 *     evaluated for each point to transform.</li>
 * <li>BEST_ACCURACY means the operation whose accuracy is best. It should be
 *     close to PROJ behavior, except that the operation to select is decided
 *     for the average point of the coordinates passed in a single Transform() call.
 *     Note: if the OGRCoordinateTransformationOptions::SetDesiredAccuracy() or
 *     OGRCoordinateTransformationOptions::SetBallparkAllowed() methods are called
 *     with PROJ < 8, this strategy will be selected instead of PROJ.
 * </li>
 * <li>FIRST_MATCHING is the operation ordered first in the list of candidates:
 *     it will not necessarily have the best accuracy, but generally a larger area of
 *     use.  It is evaluated for the average point of the coordinates passed in a
 *     single Transform() call. This was the default behavior for GDAL 3.0.0 to
 *     3.0.2</li>
 * </ul>
 *
 * If options contains a user defined coordinate transformation pipeline, it
 * will be unconditionally used.
 * If options has an area of interest defined, it will be used to research the
 * best fitting coordinate transformation (which will be used for all coordinate
 * transformations, even if they don't fall into the declared area of interest)
 * If no options are set, then a list of candidate coordinate operations will be
 * researched, and at each call to Transform(), the best of those candidate
 * regarding the centroid of the coordinate set will be dynamically selected.
 *
 * @param poSource source spatial reference system.
 * @param poTarget target spatial reference system.
 * @param options Coordinate transformation options.
 * @return NULL on failure or a ready to use transformation object.
 * @since GDAL 3.0
 */

OGRCoordinateTransformation*
OGRCreateCoordinateTransformation( const OGRSpatialReference *poSource,
                                   const OGRSpatialReference *poTarget,
                                   const OGRCoordinateTransformationOptions& options )

{
    // Try to find if we have a match in the case
    auto poCTFromCache = OGRProjCT::FindFromCache(poSource, poTarget, options);
    if( poCTFromCache )
        return poCTFromCache;

    OGRProjCT *poCT = new OGRProjCT();

    if( !poCT->Initialize( poSource, poTarget, options ) )
    {
        delete poCT;
        return nullptr;
    }

    return poCT;
}

/************************************************************************/
/*                   OCTNewCoordinateTransformation()                   */
/************************************************************************/

/**
 * Create transformation object.
 *
 * This is the same as the C++ function OGRCreateCoordinateTransformation(const OGRSpatialReference *, const OGRSpatialReference *)
 *
 * Input spatial reference system objects are assigned
 * by copy (calling clone() method) and no ownership transfer occurs.
 *
 * OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects.
 *
 * This will honour the axis order advertized by the source and target SRS,
 * as well as their "data axis to SRS axis mapping".
 * To have a behavior similar to GDAL &lt; 3.0, the OGR_CT_FORCE_TRADITIONAL_GIS_ORDER
 * configuration option can be set to YES.
 *
 * @param hSourceSRS source spatial reference system.
 * @param hTargetSRS target spatial reference system.
 * @return NULL on failure or a ready to use transformation object.
 */

OGRCoordinateTransformationH CPL_STDCALL
OCTNewCoordinateTransformation(
    OGRSpatialReferenceH hSourceSRS, OGRSpatialReferenceH hTargetSRS )

{
    return reinterpret_cast<OGRCoordinateTransformationH>(
        OGRCreateCoordinateTransformation(
            reinterpret_cast<OGRSpatialReference *>(hSourceSRS),
            reinterpret_cast<OGRSpatialReference *>(hTargetSRS)));
}

/************************************************************************/
/*                   OCTNewCoordinateTransformationEx()                 */
/************************************************************************/

/**
 * Create transformation object.
 *
 * This is the same as the C++ function OGRCreateCoordinateTransformation(const OGRSpatialReference *, const OGRSpatialReference *, const OGRCoordinateTransformationOptions& )
 *
 * Input spatial reference system objects are assigned
 * by copy (calling clone() method) and no ownership transfer occurs.
 *
 * OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects.
 *
 * The source SRS and target SRS should generally not be NULL. This is only
 * allowed if a custom coordinate operation is set through the hOptions argument.
 *
 * This will honour the axis order advertized by the source and target SRS,
 * as well as their "data axis to SRS axis mapping".
 * To have a behavior similar to GDAL &lt; 3.0, the OGR_CT_FORCE_TRADITIONAL_GIS_ORDER
 * configuration option can be set to YES.
 *
 * If options contains a user defined coordinate transformation pipeline, it
 * will be unconditionally used.
 * If options has an area of interest defined, it will be used to research the
 * best fitting coordinate transformation (which will be used for all coordinate
 * transformations, even if they don't fall into the declared area of interest)
 * If no options are set, then a list of candidate coordinate operations will be
 * researched, and at each call to Transform(), the best of those candidate
 * regarding the centroid of the coordinate set will be dynamically selected.
 *
 * @param hSourceSRS source spatial reference system.
 * @param hTargetSRS target spatial reference system.
 * @param hOptions Coordinate transformation options.
 * @return NULL on failure or a ready to use transformation object.
 * @since GDAL 3.0
 */

OGRCoordinateTransformationH
OCTNewCoordinateTransformationEx(
    OGRSpatialReferenceH hSourceSRS, OGRSpatialReferenceH hTargetSRS,
    OGRCoordinateTransformationOptionsH hOptions)

{
    OGRCoordinateTransformationOptions defaultOptions;
    return reinterpret_cast<OGRCoordinateTransformationH>(
        OGRCreateCoordinateTransformation(
            reinterpret_cast<OGRSpatialReference *>(hSourceSRS),
            reinterpret_cast<OGRSpatialReference *>(hTargetSRS),
            hOptions ? *hOptions : defaultOptions));
}

/************************************************************************/
/*                             OGRProjCT()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRProjCT::OGRProjCT()
{
}

/************************************************************************/
/*                            ~OGRProjCT()                             */
/************************************************************************/

OGRProjCT::~OGRProjCT()

{
    if( poSRSSource != nullptr )
    {
        poSRSSource->Release();
    }

    if( poSRSTarget != nullptr )
    {
        poSRSTarget->Release();
    }

    if( m_pj )
    {
        proj_assign_context(m_pj, OSRGetProjTLSContext());
        proj_destroy(m_pj);
    }
}

/************************************************************************/
/*                          ComputeThreshold()                          */
/************************************************************************/

void OGRProjCT::ComputeThreshold()
{
    // The threshold is experimental. Works well with the cases of ticket #2305.
    if( bSourceLatLong )
    {
        // coverity[tainted_data]
        dfThreshold = CPLAtof(CPLGetConfigOption( "THRESHOLD", ".1" ));
    }
    else
    {
        // 1 works well for most projections, except for +proj=aeqd that
        // requires a tolerance of 10000.
        // coverity[tainted_data]
        dfThreshold = CPLAtof(CPLGetConfigOption( "THRESHOLD", "10000" ));
    }
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRProjCT::Initialize( const OGRSpatialReference * poSourceIn,
                           const OGRSpatialReference * poTargetIn,
                           const OGRCoordinateTransformationOptions& options )

{
    m_options = options;

    if( poSourceIn == nullptr || poTargetIn == nullptr )
    {
        if( options.d->osCoordOperation.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "OGRProjCT::Initialize(): if source and/or target CRS "
                     "are null, a coordinate operation must be specified");
            return FALSE;
        }
    }

    if( poSourceIn )
        poSRSSource = poSourceIn->Clone();
    if( poTargetIn )
        poSRSTarget = poTargetIn->Clone();

    // To easy quick&dirty compatibility with GDAL < 3.0
    if( CPLTestBool(CPLGetConfigOption("OGR_CT_FORCE_TRADITIONAL_GIS_ORDER", "NO")) )
    {
        if( poSRSSource )
            poSRSSource->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poSRSTarget )
            poSRSTarget->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    if( poSRSSource )
        bSourceLatLong = CPL_TO_BOOL(poSRSSource->IsGeographic());
    if( poSRSTarget )
        bTargetLatLong = CPL_TO_BOOL(poSRSTarget->IsGeographic());

/* -------------------------------------------------------------------- */
/*      Setup source and target translations to radians for lat/long    */
/*      systems.                                                        */
/* -------------------------------------------------------------------- */
    bSourceWrap = false;
    dfSourceWrapLong = 0.0;

    bTargetWrap = false;
    dfTargetWrapLong = 0.0;

/* -------------------------------------------------------------------- */
/*      Preliminary logic to setup wrapping.                            */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "CENTER_LONG", nullptr ) != nullptr )
    {
        bSourceWrap = true;
        bTargetWrap = true;
        // coverity[tainted_data]
        dfSourceWrapLong = dfTargetWrapLong =
            CPLAtof(CPLGetConfigOption( "CENTER_LONG", "" ));
        CPLDebug( "OGRCT", "Wrap at %g.", dfSourceWrapLong );
    }

    const char *pszCENTER_LONG;
    {
        CPLErrorStateBackuper oErrorStateBackuper;
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        pszCENTER_LONG =
            poSRSSource ? poSRSSource->GetExtension( "GEOGCS", "CENTER_LONG" ) : nullptr;
    }
    if( pszCENTER_LONG != nullptr )
    {
        dfSourceWrapLong = CPLAtof(pszCENTER_LONG);
        bSourceWrap = true;
        CPLDebug( "OGRCT", "Wrap source at %g.", dfSourceWrapLong );
    }
    else if( bSourceLatLong && options.d->bHasSourceCenterLong)
    {
        dfSourceWrapLong = options.d->dfSourceCenterLong;
        bSourceWrap = true;
        CPLDebug( "OGRCT", "Wrap source at %g.", dfSourceWrapLong );
    }

    {
        CPLErrorStateBackuper oErrorStateBackuper;
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        pszCENTER_LONG = poSRSTarget ?
            poSRSTarget->GetExtension( "GEOGCS", "CENTER_LONG" ) : nullptr;
    }
    if( pszCENTER_LONG != nullptr )
    {
        dfTargetWrapLong = CPLAtof(pszCENTER_LONG);
        bTargetWrap = true;
        CPLDebug( "OGRCT", "Wrap target at %g.", dfTargetWrapLong );
    }
    else if( bTargetLatLong && options.d->bHasTargetCenterLong)
    {
        dfTargetWrapLong = options.d->dfTargetCenterLong;
        bTargetWrap = true;
        CPLDebug( "OGRCT", "Wrap target at %g.", dfTargetWrapLong );
    }

    ComputeThreshold();

    // Detect webmercator to WGS84
    OGRAxisOrientation orientAxis0, orientAxis1;
    if( options.d->osCoordOperation.empty() &&
        poSRSSource && poSRSTarget &&
        poSRSSource->IsProjected() && poSRSTarget->IsGeographic() &&
        poSRSTarget->GetAxis(nullptr, 0, &orientAxis0) != nullptr &&
        poSRSTarget->GetAxis(nullptr, 1, &orientAxis1) != nullptr &&
        ((orientAxis0 == OAO_North && orientAxis1 == OAO_East &&
          poSRSTarget->GetDataAxisToSRSAxisMapping() == std::vector<int>{2,1}) ||
         (orientAxis0 == OAO_East && orientAxis1 == OAO_North &&
          poSRSTarget->GetDataAxisToSRSAxisMapping() == std::vector<int>{1,2})) )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        char *pszSrcProj4Defn = nullptr;
        poSRSSource->exportToProj4( &pszSrcProj4Defn );

        char *pszDstProj4Defn = nullptr;
        poSRSTarget->exportToProj4( &pszDstProj4Defn );
        CPLPopErrorHandler();

        if( pszSrcProj4Defn && pszDstProj4Defn )
        {
            if( pszSrcProj4Defn[0] != '\0' &&
                pszSrcProj4Defn[strlen(pszSrcProj4Defn)-1] == ' ' )
                pszSrcProj4Defn[strlen(pszSrcProj4Defn)-1] = 0;
            if( pszDstProj4Defn[0] != '\0' &&
                pszDstProj4Defn[strlen(pszDstProj4Defn)-1] == ' ' )
                pszDstProj4Defn[strlen(pszDstProj4Defn)-1] = 0;
            char* pszNeedle = strstr(pszSrcProj4Defn, "  ");
            if( pszNeedle )
                memmove(pszNeedle, pszNeedle + 1, strlen(pszNeedle + 1)+1);
            pszNeedle = strstr(pszDstProj4Defn, "  ");
            if( pszNeedle )
                memmove(pszNeedle, pszNeedle + 1, strlen(pszNeedle + 1)+1);

            if( (strstr(pszDstProj4Defn, "+datum=WGS84") != nullptr ||
                strstr(pszDstProj4Defn,
                        "+ellps=WGS84 +towgs84=0,0,0,0,0,0,0 ") != nullptr) &&
                strstr(pszSrcProj4Defn, "+nadgrids=@null ") != nullptr &&
                strstr(pszSrcProj4Defn, "+towgs84") == nullptr )
            {
                char* pszDst = strstr(pszDstProj4Defn, "+towgs84=0,0,0,0,0,0,0 ");
                if( pszDst != nullptr)
                {
                    char* pszSrc = pszDst + strlen("+towgs84=0,0,0,0,0,0,0 ");
                    memmove(pszDst, pszSrc, strlen(pszSrc)+1);
                }
                else
                {
                    memcpy(strstr(pszDstProj4Defn, "+datum=WGS84"), "+ellps", 6);
                }

                pszDst = strstr(pszSrcProj4Defn, "+nadgrids=@null ");
                char* pszSrc = pszDst + strlen("+nadgrids=@null ");
                memmove(pszDst, pszSrc, strlen(pszSrc)+1);

                pszDst = strstr(pszSrcProj4Defn, "+wktext ");
                if( pszDst )
                {
                    pszSrc = pszDst + strlen("+wktext ");
                    memmove(pszDst, pszSrc, strlen(pszSrc)+1);
                }
                bWebMercatorToWGS84LongLat =
                    strcmp(pszDstProj4Defn,
                        "+proj=longlat +ellps=WGS84 +no_defs") == 0 &&
                    (strcmp(pszSrcProj4Defn,
                        "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 "
                        "+x_0=0.0 +y_0=0 +k=1.0 +units=m +no_defs") == 0 ||
                    strcmp(pszSrcProj4Defn,
                        "+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 "
                        "+x_0=0 +y_0=0 +k=1 +units=m +no_defs") == 0);
            }
        }

        CPLFree(pszSrcProj4Defn);
        CPLFree(pszDstProj4Defn);
    }

    const char* pszCTOpSelection = CPLGetConfigOption("OGR_CT_OP_SELECTION", nullptr);
    if( pszCTOpSelection )
    {
        if( EQUAL(pszCTOpSelection, "PROJ") )
            m_eStrategy = Strategy::PROJ;
        else if( EQUAL(pszCTOpSelection, "BEST_ACCURACY") )
            m_eStrategy = Strategy::BEST_ACCURACY;
        else if( EQUAL(pszCTOpSelection, "FIRST_MATCHING") )
            m_eStrategy = Strategy::FIRST_MATCHING;
        else
            CPLError(CE_Warning, CPLE_NotSupported,
                     "OGR_CT_OP_SELECTION=%s not supported", pszCTOpSelection);
    }
#if PROJ_VERSION_MAJOR < 8
    else
    {
        if( options.d->dfAccuracy >= 0 || !options.d->bAllowBallpark )
        {
            m_eStrategy = Strategy::BEST_ACCURACY;
        }
    }
#endif
    if( m_eStrategy == Strategy::PROJ )
    {
        const char* pszUseApproxTMERC = CPLGetConfigOption("OSR_USE_APPROX_TMERC", nullptr);
        if( pszUseApproxTMERC && CPLTestBool(pszUseApproxTMERC) )
        {
            CPLDebug("OSRCT", "Using OGR_CT_OP_SELECTION=BEST_ACCURACY as OSR_USE_APPROX_TMERC is set");
            m_eStrategy = Strategy::BEST_ACCURACY;
        }
    }

    if( !options.d->osCoordOperation.empty() )
    {
        auto ctx = OSRGetProjTLSContext();
        m_pj = proj_create(ctx, options.d->osCoordOperation);
        if( !m_pj )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Cannot instantiate pipeline %s",
                      options.d->osCoordOperation.c_str() );
            return FALSE;
        }
        m_bReversePj = options.d->bReverseCO;
#ifdef DEBUG
        auto info = proj_pj_info(m_pj);
        CPLDebug("OGRCT", "%s %s(user set)", info.definition,
                 m_bReversePj ? "(reversed) " : "");
#endif
    }
    else if( !bWebMercatorToWGS84LongLat && poSRSSource && poSRSTarget )
    {
        const auto CanUseAuthorityDef = [](const OGRSpatialReference* poSRS1,
                                           OGRSpatialReference* poSRSFromAuth,
                                           const char* pszAuth)
        {
            if( EQUAL(pszAuth, "EPSG") &&
                CPLTestBool(CPLGetConfigOption("OSR_CT_USE_DEFAULT_EPSG_TOWGS84", "NO")) )
            {
                // We don't want by default to honour 'default' TOWGS84 terms that come with the EPSG code
                // because there might be a better transformation from that
                // Typical case if EPSG:31468 "DHDN / 3-degree Gauss-Kruger zone 4"
                // where the DHDN->TOWGS84 transformation can use the BETA2007.gsb grid
                // instead of TOWGS84[598.1,73.7,418.2,0.202,0.045,-2.455,6.7]
                // But if the user really wants it, it can set the
                // OSR_CT_USE_DEFAULT_EPSG_TOWGS84 configuration option to YES
                double adfTOWGS84_1[7];
                double adfTOWGS84_2[7];

                poSRSFromAuth->AddGuessedTOWGS84();

                if( poSRS1->GetTOWGS84(adfTOWGS84_1) == OGRERR_NONE &&
                    poSRSFromAuth->GetTOWGS84(adfTOWGS84_2) == OGRERR_NONE &&
                    memcmp(adfTOWGS84_1, adfTOWGS84_2, sizeof(adfTOWGS84_1)) == 0 )
                {
                    return false;
                }
            }
            return true;
        };

        const auto exportSRSToText = [&CanUseAuthorityDef](const OGRSpatialReference* poSRS)
        {
            char* pszText = nullptr;
            // If we have a AUTH:CODE attached, use it to retrieve the full
            // definition in case a trip to WKT1 has lost the area of use.
            const char* pszAuth = poSRS->GetAuthorityName(nullptr);
            const char* pszCode = poSRS->GetAuthorityCode(nullptr);
            if( pszAuth && pszCode )
            {
                CPLString osAuthCode(pszAuth);
                osAuthCode += ':';
                osAuthCode += pszCode;
                OGRSpatialReference oTmpSRS;
                oTmpSRS.SetFromUserInput(osAuthCode);
                oTmpSRS.SetDataAxisToSRSAxisMapping(poSRS->GetDataAxisToSRSAxisMapping());
                if( oTmpSRS.IsSame(poSRS) )
                {
                    if( CanUseAuthorityDef(poSRS, &oTmpSRS, pszAuth) )
                    {
                        pszText = CPLStrdup(osAuthCode);
                    }
                }
            }
            if( pszText == nullptr )
            {
                pszText = GetWktOrProjString(poSRS);
            }
            return pszText;
        };

        char* pszSrcSRS = exportSRSToText(poSRSSource);
        char* pszTargetSRS = exportSRSToText(poSRSTarget);
#ifdef DEBUG_PERF
        struct CPLTimeVal tvStart;
        CPLGettimeofday(&tvStart, nullptr);
        CPLDebug("OGR_CT", "Before proj_create_crs_to_crs()");
#endif
#ifdef DEBUG
        CPLDebug("OGR_CT", "Source CRS: '%s'", pszSrcSRS);
        CPLDebug("OGR_CT", "Target CRS: '%s'", pszTargetSRS);
#endif

        if( m_eStrategy == Strategy::PROJ )
        {
            PJ_AREA* area = nullptr;
            if( options.d->bHasAreaOfInterest )
            {
                area = proj_area_create();
                proj_area_set_bbox(area,
                    options.d->dfWestLongitudeDeg,
                    options.d->dfSouthLatitudeDeg,
                    options.d->dfEastLongitudeDeg,
                    options.d->dfNorthLatitudeDeg);
            }
            auto ctx = OSRGetProjTLSContext();
#if PROJ_VERSION_MAJOR >= 8
            auto srcCRS = proj_create(ctx, pszSrcSRS);
            auto targetCRS = proj_create(ctx, pszTargetSRS);
            if( srcCRS == nullptr || targetCRS == nullptr )
            {
                CPLFree( pszSrcSRS );
                CPLFree( pszTargetSRS );
                proj_destroy(srcCRS);
                proj_destroy(targetCRS);
                return FALSE;
            }
            CPLStringList aosOptions;
            if( options.d->dfAccuracy >= 0 )
                aosOptions.SetNameValue("ACCURACY", CPLSPrintf("%.18g", options.d->dfAccuracy));
            if( !options.d->bAllowBallpark )
                aosOptions.SetNameValue("ALLOW_BALLPARK", "NO");
            m_pj = proj_create_crs_to_crs_from_pj(ctx, srcCRS, targetCRS, area, aosOptions.List());
            proj_destroy(srcCRS);
            proj_destroy(targetCRS);
#else
            m_pj = proj_create_crs_to_crs(ctx, pszSrcSRS, pszTargetSRS, area);
#endif
            if( area )
                proj_area_destroy(area);
            if( m_pj == nullptr )
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                            "Cannot find coordinate operations from `%s' to `%s'",
                            pszSrcSRS,
                            pszTargetSRS );
                CPLFree( pszSrcSRS );
                CPLFree( pszTargetSRS );
                return FALSE;
            }

        }
        else if( !ListCoordinateOperations(pszSrcSRS, pszTargetSRS, options) )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                        "Cannot find coordinate operations from `%s' to `%s'",
                        pszSrcSRS,
                        pszTargetSRS );
            CPLFree( pszSrcSRS );
            CPLFree( pszTargetSRS );
            return FALSE;
        }
#ifdef DEBUG_PERF
        struct CPLTimeVal tvEnd;
        CPLGettimeofday(&tvEnd, nullptr);
        const double delay = (tvEnd.tv_sec + tvEnd.tv_usec * 1e-6) -
                             (tvStart.tv_sec + tvStart.tv_usec * 1e-6);
        g_dfTotalTimeCRStoCRS += delay;
        CPLDebug("OGR_CT", "After proj_create_crs_to_crs(): %d ms",
                 static_cast<int>(delay * 1000));
#endif

        CPLFree(pszSrcSRS);
        CPLFree(pszTargetSRS);
    }

    if( options.d->osCoordOperation.empty() && poSRSSource && poSRSTarget )
    {
        // Determine if we can skip the transformation completely.
        bNoTransform = !bSourceWrap && !bTargetWrap &&
                       CPL_TO_BOOL(poSRSSource->IsSame(poSRSTarget));
    }

    return TRUE;
}

/************************************************************************/
/*                               op_to_pj()                             */
/************************************************************************/

static PJ* op_to_pj(PJ_CONTEXT* ctx, PJ* op, CPLString* osOutProjString = nullptr )
{
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
    auto proj_string = proj_as_proj_string(ctx, op, PJ_PROJ_5, options);
    if( !proj_string) {
        return nullptr;
    }
    if( osOutProjString )
        *osOutProjString = proj_string;

    if( proj_string[0] == '\0' ) {
        /* Null transform ? */
        return proj_create(ctx, "proj=affine");
    } else {
        return proj_create(ctx, proj_string);
    }
}

/************************************************************************/
/*                       ListCoordinateOperations()                     */
/************************************************************************/

bool OGRProjCT::ListCoordinateOperations(const char* pszSrcSRS,
                                         const char* pszTargetSRS,
                                         const OGRCoordinateTransformationOptions& options )
{
    auto ctx = OSRGetProjTLSContext();

    auto src = proj_create(ctx, pszSrcSRS);
    if( !src ) {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot instantiate source_crs");
        return false;
    }

    auto dst = proj_create(ctx, pszTargetSRS);
    if( !dst ) {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot instantiate target_crs");
        proj_destroy(src);
        return false;
    }

    auto operation_ctx = proj_create_operation_factory_context(ctx, nullptr);
    if( !operation_ctx ) {
        proj_destroy(src);
        proj_destroy(dst);
        return false;
    }

    proj_operation_factory_context_set_spatial_criterion(
        ctx, operation_ctx, PROJ_SPATIAL_CRITERION_PARTIAL_INTERSECTION);
    proj_operation_factory_context_set_grid_availability_use(
        ctx, operation_ctx, PROJ_GRID_AVAILABILITY_DISCARD_OPERATION_IF_MISSING_GRID);

    if( options.d->bHasAreaOfInterest )
    {
        proj_operation_factory_context_set_area_of_interest(
            ctx,
            operation_ctx,
            options.d->dfWestLongitudeDeg,
            options.d->dfSouthLatitudeDeg,
            options.d->dfEastLongitudeDeg,
            options.d->dfNorthLatitudeDeg);
    }

    if( options.d->dfAccuracy >= 0 )
        proj_operation_factory_context_set_desired_accuracy(ctx ,operation_ctx, options.d->dfAccuracy);
    if ( !options.d->bAllowBallpark )
    {
#if PROJ_VERSION_MAJOR > 7 || (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 1)
        proj_operation_factory_context_set_allow_ballpark_transformations(ctx ,operation_ctx, FALSE);
#else
        if( options.d->dfAccuracy < 0 )
        {
            proj_operation_factory_context_set_desired_accuracy(ctx ,operation_ctx, HUGE_VAL);
        }
#endif
    }

    auto op_list = proj_create_operations(ctx, src, dst, operation_ctx);

    if( !op_list ) {
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        return false;
    }

    auto op_count = proj_list_get_count(op_list);
    if( op_count == 0 ) {
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        CPLDebug("OGRCT", "No operation found matching criteria");
        return false;
    }

    if( op_count == 1 || options.d->bHasAreaOfInterest ||
        proj_get_type(src) == PJ_TYPE_GEOCENTRIC_CRS ||
        proj_get_type(dst) == PJ_TYPE_GEOCENTRIC_CRS ) {
        auto op = proj_list_get(ctx, op_list, 0);
        CPLAssert(op);
        m_pj = op_to_pj(ctx, op);
        CPLString osName;
        auto name = proj_get_name(op);
        if( name )
            osName = name;
        proj_destroy(op);
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        if( !m_pj )
            return false;
#ifdef DEBUG
        auto info = proj_pj_info(m_pj);
        CPLDebug("OGRCT", "%s (%s)", info.definition, osName.c_str());
#endif
        return true;
    }

    // Create a geographic 2D long-lat degrees CRS that is related to the
    // source CRS
    auto geodetic_crs = proj_crs_get_geodetic_crs(ctx, src);
    if( !geodetic_crs ) {
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        CPLDebug("OGRCT", "Cannot find geodetic CRS matching source CRS");
        return false;
    }
    auto geodetic_crs_type = proj_get_type(geodetic_crs);
    if( geodetic_crs_type == PJ_TYPE_GEOCENTRIC_CRS ||
        geodetic_crs_type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
        geodetic_crs_type == PJ_TYPE_GEOGRAPHIC_3D_CRS )
    {
        auto datum = proj_crs_get_datum(ctx, geodetic_crs);
#if PROJ_VERSION_MAJOR > 7 || (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 2)
        if( datum == nullptr )
        {
            datum = proj_crs_get_datum_ensemble(ctx, geodetic_crs);
        }
#endif
        if( datum )
        {
            auto cs = proj_create_ellipsoidal_2D_cs(
                ctx, PJ_ELLPS2D_LONGITUDE_LATITUDE, nullptr, 0);
            auto temp = proj_create_geographic_crs_from_datum(
                ctx,"unnamed", datum, cs);
            proj_destroy(datum);
            proj_destroy(cs);
            proj_destroy(geodetic_crs);
            geodetic_crs = temp;
            geodetic_crs_type = proj_get_type(geodetic_crs);
        }
    }
    if( geodetic_crs_type != PJ_TYPE_GEOGRAPHIC_2D_CRS )
    {
        // Shouldn't happen
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        proj_destroy(geodetic_crs);
        CPLDebug("OGRCT", "Cannot find geographic CRS matching source CRS");
        return false;
    }

    // Create the transformation from this geographic 2D CRS to the source CRS
    auto op_list_to_geodetic = proj_create_operations(
        ctx, geodetic_crs, src, operation_ctx);
    proj_destroy(geodetic_crs);

    if( op_list_to_geodetic == nullptr ||
        proj_list_get_count(op_list_to_geodetic) == 0 )
    {
        CPLDebug("OGRCT", "Cannot compute transformation from geographic CRS to source CRS");
        proj_list_destroy(op_list);
        proj_list_destroy(op_list_to_geodetic);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        return false;
    }
    auto opGeogToSrc = proj_list_get(ctx, op_list_to_geodetic, 0);
    CPLAssert(opGeogToSrc);
    proj_list_destroy(op_list_to_geodetic);
    auto pjGeogToSrc = op_to_pj(ctx, opGeogToSrc);
    proj_destroy(opGeogToSrc);
    if( !pjGeogToSrc ) {
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        return false;
    }

    const auto addTransformation = [=](PJ* op,
                                       double west_lon, double south_lat,
                                       double east_lon, double north_lat) {
        double minx = -std::numeric_limits<double>::max();
        double miny = -std::numeric_limits<double>::max();
        double maxx = std::numeric_limits<double>::max();
        double maxy = std::numeric_limits<double>::max();

        if( !(west_lon == -180.0 && east_lon == 180.0 &&
              south_lat == -90.0 && north_lat == 90.0) )
        {
            minx = -minx;
            miny = -miny;
            maxx = -maxx;
            maxy = -maxy;

            double x[21 * 4], y[21 * 4];
            for( int j = 0; j <= 20; j++ )
            {
                x[j] = west_lon + j * (east_lon - west_lon) / 20;
                y[j] = south_lat;
                x[21+j] = west_lon + j * (east_lon - west_lon) / 20;
                y[21+j] = north_lat;
                x[21*2+j] = west_lon;
                y[21*2+j] = south_lat + j * (north_lat - south_lat) / 20;
                x[21*3+j] = east_lon;
                y[21*3+j] = south_lat + j * (north_lat - south_lat) / 20;
            }
            proj_trans_generic (
                pjGeogToSrc, PJ_FWD,
                    x, sizeof(double), 21 * 4,
                    y, sizeof(double), 21 * 4,
                    nullptr, 0, 0,
                    nullptr, 0, 0);
            for( int j = 0; j < 21 * 4; j++ )
            {
                if( x[j] != HUGE_VAL && y[j] != HUGE_VAL )
                {
                    minx = std::min(minx, x[j]);
                    miny = std::min(miny, y[j]);
                    maxx = std::max(maxx, x[j]);
                    maxy = std::max(maxy, y[j]);
                }
            }
        }

        if( minx <= maxx )
        {
            CPLString osProjString;
            const double accuracy = proj_coordoperation_get_accuracy(ctx, op);
            auto pj = op_to_pj(ctx, op, &osProjString);
            CPLString osName;
            auto name = proj_get_name(op);
            if( name )
                osName = name;
            proj_destroy(op);
            op = nullptr;
            if( pj )
            {
                m_oTransformations.emplace_back(
                    minx, miny, maxx, maxy, pj, osName, osProjString, accuracy);
            }
        }
        return op;
    };

    // Iterate over source->target candidate transformations and reproject
    // their long-lat bounding box into the source CRS.
    bool foundWorldTransformation = false;
    for( int i = 0; i < op_count; i++ )
    {
        auto op = proj_list_get(ctx, op_list, i);
        CPLAssert(op);
        double west_lon = 0.0;
        double south_lat = 0.0;
        double east_lon = 0.0;
        double north_lat = 0.0;
        if( proj_get_area_of_use(ctx, op,
                    &west_lon, &south_lat, &east_lon, &north_lat, nullptr) )
        {
            if( west_lon <= east_lon )
            {
                if( west_lon == -180 && east_lon == 180 &&
                    south_lat == -90 && north_lat == 90 )
                {
                    foundWorldTransformation = true;
                }
                op = addTransformation(op,
                                  west_lon, south_lat, east_lon, north_lat);
            }
            else
            {
                auto op_clone = proj_clone(ctx, op);

                op = addTransformation(op,
                                  west_lon, south_lat, 180, north_lat);
                op_clone = addTransformation(op_clone,
                                  -180, south_lat, east_lon, north_lat);
                proj_destroy(op_clone);
            }
        }

        proj_destroy(op);
    }

    proj_list_destroy(op_list);

    // Sometimes the user will operate even outside the area of use of the
    // source and target CRS, so if no global transformation has been returned
    // previously, trigger the computation of one.
    if( !foundWorldTransformation )
    {
        proj_operation_factory_context_set_area_of_interest(
                                            ctx,
                                            operation_ctx,
                                            -180, -90, 180, 90);
        proj_operation_factory_context_set_spatial_criterion(
            ctx, operation_ctx, PROJ_SPATIAL_CRITERION_STRICT_CONTAINMENT);
        op_list = proj_create_operations(ctx, src, dst, operation_ctx);
        if( op_list )
        {
            op_count = proj_list_get_count(op_list);
            for( int i = 0; i < op_count; i++ )
            {
                auto op = proj_list_get(ctx, op_list, i);
                CPLAssert(op);
                double west_lon = 0.0;
                double south_lat = 0.0;
                double east_lon = 0.0;
                double north_lat = 0.0;
                if( proj_get_area_of_use(ctx, op,
                        &west_lon, &south_lat, &east_lon, &north_lat, nullptr) &&
                    west_lon == -180 && east_lon == 180 &&
                    south_lat == -90 && north_lat == 90 )
                {
                    op = addTransformation(op,
                                  west_lon, south_lat, east_lon, north_lat);
                }
                proj_destroy(op);
            }
        }
        proj_list_destroy(op_list);
    }

    proj_operation_factory_context_destroy(operation_ctx);
    proj_destroy(src);
    proj_destroy(dst);
    proj_destroy(pjGeogToSrc);
    return !m_oTransformations.empty();
}

/************************************************************************/
/*                            GetSourceCS()                             */
/************************************************************************/

OGRSpatialReference *OGRProjCT::GetSourceCS()

{
    return poSRSSource;
}

/************************************************************************/
/*                            GetTargetCS()                             */
/************************************************************************/

OGRSpatialReference *OGRProjCT::GetTargetCS()

{
    return poSRSTarget;
}

/************************************************************************/
/*                             Transform()                              */
/************************************************************************/

int OGRCoordinateTransformation::Transform(
            int nCount, double *x, double *y, double *z,
            int *pabSuccessIn )

{
    int *pabSuccess = pabSuccessIn ? pabSuccessIn :
        static_cast<int *>(CPLMalloc(sizeof(int) * nCount));

    bool bOverallSuccess =
        CPL_TO_BOOL(Transform( nCount, x, y, z, nullptr, pabSuccess ));

    for( int i = 0; i < nCount; i++ )
    {
        if( !pabSuccess[i] )
        {
            bOverallSuccess = false;
            break;
        }
    }

    if( pabSuccess != pabSuccessIn )
        CPLFree( pabSuccess );

    return bOverallSuccess;
}

/************************************************************************/
/*                      TransformWithErrorCodes()                       */
/************************************************************************/

int OGRCoordinateTransformation::TransformWithErrorCodes(
            int nCount, double *x, double *y, double *z, double* t,
            int *panErrorCodes )

{
    std::vector<int> abSuccess(nCount+1);

    bool bOverallSuccess =
        CPL_TO_BOOL(Transform( nCount, x, y, z, t, &abSuccess[0] ));

    if( panErrorCodes )
    {
        for( int i = 0; i < nCount; i++ )
        {
            panErrorCodes[i] = abSuccess[i] ? 0 : -1;
        }
    }

    return bOverallSuccess;
}

/************************************************************************/
/*                             Transform()                             */
/************************************************************************/

int OGRProjCT::Transform( int nCount, double *x, double *y, double *z,
                          double *t, int *pabSuccess )

{
    std::vector<int> anErrorCodes(nCount+1);

    bool bOverallSuccess =
        CPL_TO_BOOL(TransformWithErrorCodes( nCount, x, y, z, t, &anErrorCodes[0] ));

    if( pabSuccess )
    {
        for( int i = 0; i < nCount; i++ )
        {
            pabSuccess[i] = ( anErrorCodes[i] == 0 );
        }
    }

    return bOverallSuccess;
}

/************************************************************************/
/*                       TransformWithErrorCodes()                      */
/************************************************************************/

#ifndef PROJ_ERR_COORD_TRANSFM_INVALID_COORD
#define PROJ_ERR_COORD_TRANSFM_INVALID_COORD             2049
#define PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN 2050
#define PROJ_ERR_COORD_TRANSFM_NO_OPERATION              2051
#endif

int OGRProjCT::TransformWithErrorCodes(
            int nCount, double *x, double *y, double *z, double* t,
            int *panErrorCodes )

{
    if( nCount == 0 )
        return TRUE;

    // Prevent any coordinate modification when possible
    if ( bNoTransform )
    {
        if( panErrorCodes )
        {
            for( int i = 0; i < nCount; i++ )
            {
                 panErrorCodes[i] = 0;
            }
        }
        return TRUE;
    }

#ifdef DEBUG_VERBOSE
    bool bDebugCT = CPLTestBool(CPLGetConfigOption("OGR_CT_DEBUG", "NO"));
    if( bDebugCT )
    {
        CPLDebug("OGRCT", "count = %d", nCount);
        for( int i = 0; i < nCount; ++i )
        {
            CPLDebug("OGRCT", "  x[%d] = %.16g y[%d] = %.16g",
                     i, x[i], i, y[i]);
        }
    }
#endif
#ifdef DEBUG_PERF
    //CPLDebug("OGR_CT", "Begin TransformWithErrorCodes()");
    struct CPLTimeVal tvStart;
    CPLGettimeofday(&tvStart, nullptr);
#endif

/* -------------------------------------------------------------------- */
/*      Apply data axis to source CRS mapping.                          */
/* -------------------------------------------------------------------- */
    if( poSRSSource )
    {
        const auto& mapping = poSRSSource->GetDataAxisToSRSAxisMapping();
        if( mapping.size() >= 2 && (mapping[0] != 1 || mapping[1] != 2) )
        {
            for( int i = 0; i < nCount; i++ )
            {
                double newX = (mapping[0] == 1) ? x[i] :
                    (mapping[0] == -1) ? -x[i] : (mapping[0] == 2) ? y[i] : -y[i];
                double newY = (mapping[1] == 2) ? y[i] :
                    (mapping[1] == -2) ? -y[i] : (mapping[1] == 1) ? x[i] : -x[i];
                x[i] = newX;
                y[i] = newY;
                if( z && mapping.size() >= 3 && mapping[2] == -3)
                    z[i] = -z[i];
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Potentially do longitude wrapping.                              */
/* -------------------------------------------------------------------- */
    if( bSourceLatLong && bSourceWrap )
    {
        OGRAxisOrientation orientation;
        assert( poSRSSource );
        poSRSSource->GetAxis(nullptr, 0, &orientation);
        if( orientation == OAO_East )
        {
            for( int i = 0; i < nCount; i++ )
            {
                if( x[i] != HUGE_VAL && y[i] != HUGE_VAL )
                {
                    if( x[i] < dfSourceWrapLong - 180.0 )
                        x[i] += 360.0;
                    else if( x[i] > dfSourceWrapLong + 180 )
                        x[i] -= 360.0;
                }
            }
        }
        else
        {
            for( int i = 0; i < nCount; i++ )
            {
                if( x[i] != HUGE_VAL && y[i] != HUGE_VAL )
                {
                    if( y[i] < dfSourceWrapLong - 180.0 )
                        y[i] += 360.0;
                    else if( y[i] > dfSourceWrapLong + 180 )
                        y[i] -= 360.0;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Optimized transform from WebMercator to WGS84                   */
/* -------------------------------------------------------------------- */
    bool bTransformDone = false;
    if( bWebMercatorToWGS84LongLat )
    {
        constexpr double REVERSE_SPHERE_RADIUS = 1.0 / 6378137.0;

        if( poSRSSource )
        {
            OGRAxisOrientation orientation;
            poSRSSource->GetAxis(nullptr, 0, &orientation);
            if( orientation != OAO_East )
            {
                for( int i = 0; i < nCount; i++ )
                {
                    std::swap(x[i], y[i]);
                }
            }
        }

        double y0 = y[0];
        for( int i = 0; i < nCount; i++ )
        {
            if( x[i] != HUGE_VAL )
            {
                x[i] = x[i] * REVERSE_SPHERE_RADIUS;
                if( x[i] > M_PI )
                {
                    if( x[i] < M_PI+1e-14 )
                    {
                        x[i] = M_PI;
                    }
                    else if( m_options.d->bCheckWithInvertProj )
                    {
                        x[i] = HUGE_VAL;
                        y[i] = HUGE_VAL;
                        y0 = HUGE_VAL;
                        continue;
                    }
                    else
                    {
                        do {
                            x[i] -= 2 * M_PI;
                        } while( x[i] > M_PI );
                    }
                }
                else if( x[i] < -M_PI )
                {
                    if( x[i] > -M_PI-1e-14 )
                    {
                        x[i] = -M_PI;
                    }
                    else if( m_options.d->bCheckWithInvertProj )
                    {
                        x[i] = HUGE_VAL;
                        y[i] = HUGE_VAL;
                        y0 = HUGE_VAL;
                        continue;
                    }
                    else
                    {
                        do {
                            x[i] += 2 * M_PI;
                        } while( x[i] < -M_PI );
                    }
                }
                constexpr double RAD_TO_DEG = 57.29577951308232;
                x[i] *= RAD_TO_DEG;

                 // Optimization for the case where we are provided a whole line
                 // of same northing.
                if( i > 0 && y[i] == y0 )
                    y[i] = y[0];
                else
                {
                    y[i] =
                        M_PI / 2.0 -
                        2.0 * atan(exp(-y[i] * REVERSE_SPHERE_RADIUS));
                    y[i] *= RAD_TO_DEG;
                }
            }
        }

        if( panErrorCodes )
        {
            for( int i = 0; i < nCount; i++ )
            {
                if( x[i] != HUGE_VAL )
                    panErrorCodes[i] = 0;
                else
                    panErrorCodes[i] = PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN;
            }
        }

        if( poSRSTarget )
        {
            OGRAxisOrientation orientation;
            poSRSTarget->GetAxis(nullptr, 0, &orientation);
            if( orientation != OAO_East )
            {
                for( int i = 0; i < nCount; i++ )
                {
                    std::swap(x[i], y[i]);
                }
            }
        }

        bTransformDone = true;
    }

/* -------------------------------------------------------------------- */
/*      Select dynamically the best transformation for the data, if     */
/*      needed.                                                         */
/* -------------------------------------------------------------------- */

    auto ctx = OSRGetProjTLSContext();
    auto pj = m_pj;
    if( !bTransformDone && !pj )
    {
        double avgX = 0.0;
        double avgY = 0.0;
        int nCountValid = 0;
        for( int i = 0; i < nCount; i++ )
        {
            if( x[i] != HUGE_VAL && y[i] != HUGE_VAL )
            {
                avgX += x[i];
                avgY += y[i];
                nCountValid ++;
            }
        }
        if( nCountValid != 0 )
        {
            avgX /= nCountValid;
            avgY /= nCountValid;
        }

        constexpr int N_MAX_RETRY = 2;
        int iExcluded[N_MAX_RETRY] = {-1, -1};

        const int nOperations = static_cast<int>(m_oTransformations.size());
        PJ_COORD coord;
        coord.xyzt.x = avgX;
        coord.xyzt.y = avgY;
        coord.xyzt.z = z ? z[0] : 0;
        coord.xyzt.t = t ? t[0] : HUGE_VAL;

        // We may need several attempts. For example the point at
        // lon=-111.5 lat=45.26 falls into the bounding box of the Canadian
        // ntv2_0.gsb grid, except that it is not in any of the subgrids, being
        // in the US. We thus need another retry that will select the conus
        // grid.
        for( int iRetry = 0; iRetry <= N_MAX_RETRY; iRetry++ )
        {
            int iBestTransf = -1;
            // Select transform whose BBOX match our data and has the best accuracy
            // if m_eStrategy == BEST_ACCURACY. Or just the first BBOX matching one, if
            //  m_eStrategy == FIRST_MATCHING
            double dfBestAccuracy = std::numeric_limits<double>::infinity();
            for( int i = 0; i < nOperations; i++ )
            {
                if( i == iExcluded[0] || i == iExcluded[1] )
                {
                    continue;
                }
                const auto& transf = m_oTransformations[i];
                if( avgX >= transf.minx && avgX <= transf.maxx &&
                    avgY >= transf.miny && avgY <= transf.maxy &&
                    (iBestTransf < 0 || (transf.accuracy >= 0 &&
                                        transf.accuracy < dfBestAccuracy)) )
                {
                    iBestTransf = i;
                    dfBestAccuracy = transf.accuracy;
                    if( m_eStrategy == Strategy::FIRST_MATCHING )
                        break;
                }
            }
            if( iBestTransf < 0 )
            {
                break;
            }
            const auto& transf = m_oTransformations[iBestTransf];
            pj = transf.pj;
            proj_assign_context( pj, ctx );
            if( iBestTransf != m_iCurTransformation )
            {
                CPLDebug("OGRCT", "Selecting transformation %s (%s)",
                        transf.osProjString.c_str(),
                        transf.osName.c_str());
                m_iCurTransformation = iBestTransf;
            }

            auto res = proj_trans(pj, m_bReversePj ? PJ_INV : PJ_FWD, coord);
            if( res.xyzt.x != HUGE_VAL ) {
                break;
            }
            pj = nullptr;
            CPLDebug("OGRCT",
                     "Did not result in valid result. "
                     "Attempting a retry with another operation.");
            if( iRetry == N_MAX_RETRY ) {
                break;
            }
            iExcluded[iRetry] = iBestTransf;
        }

        if( !pj )
        {
            // In case we did not find an operation whose area of use is compatible
            // with the input coordinate, then goes through again the list, and
            // use the first operation that does not require grids.
            for( int i = 0; i < nOperations; i++ )
            {
                const auto& transf = m_oTransformations[i];
                if( proj_coordoperation_get_grid_used_count(ctx, transf.pj) == 0 )
                {
                    pj = transf.pj;
                    proj_assign_context( pj, ctx );
                    if( i != m_iCurTransformation )
                    {
                        CPLDebug("OGRCT", "Selecting transformation %s (%s)",
                                transf.osProjString.c_str(),
                                transf.osName.c_str());
                        m_iCurTransformation = i;
                    }
                    break;
                }
            }
        }

        if( !pj )
        {
            if( m_bEmitErrors && ++nErrorCount < 20 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot find transformation for provided coordinates");
            }
            else if( nErrorCount == 20 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Reprojection failed, further errors will be "
                        "suppressed on the transform object.");
            }

            for( int i = 0; i < nCount; i++ )
            {
                x[i] = HUGE_VAL;
                y[i] = HUGE_VAL;
                if( panErrorCodes )
                    panErrorCodes[i] = PROJ_ERR_COORD_TRANSFM_NO_OPERATION;
            }
            return FALSE;
        }
    }
    if( pj )
    {
        proj_assign_context( pj, ctx );
    }

/* -------------------------------------------------------------------- */
/*      Do the transformation (or not...) using PROJ                    */
/* -------------------------------------------------------------------- */

    if( !bTransformDone )
    {
        for( int i = 0; i < nCount; i++ )
        {
            PJ_COORD coord;
            const double xIn = x[i];
            const double yIn = y[i];
            if( !std::isfinite(xIn) )
            {
                x[i] = HUGE_VAL;
                y[i] = HUGE_VAL;
                if( panErrorCodes )
                    panErrorCodes[i] = PROJ_ERR_COORD_TRANSFM_INVALID_COORD;
                continue;
            }
            coord.xyzt.x = x[i];
            coord.xyzt.y = y[i];
            coord.xyzt.z = z ? z[i] : 0;
            coord.xyzt.t = t ? t[i] : 0;
            proj_errno_reset(pj);
            coord = proj_trans(pj, m_bReversePj ? PJ_INV : PJ_FWD, coord);
            x[i] = coord.xyzt.x;
            y[i] = coord.xyzt.y;
            if( z )
                z[i] = coord.xyzt.z;
            if( t )
                t[i] = coord.xyzt.t;
            int err = 0;
            if( coord.xyzt.x == HUGE_VAL )
            {
                err = proj_errno(pj);
                // PROJ should normally emit an error, but in case it does not
                // (e.g PROJ 6.3 with the +ortho projection), synthetize one
                if( err == 0 )
                    err = PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN;
            }
            else if( m_options.d->bCheckWithInvertProj )
            {
                // For some projections, we cannot detect if we are trying to reproject
                // coordinates outside the validity area of the projection. So let's do
                // the reverse reprojection and compare with the source coordinates.
                coord = proj_trans(pj, m_bReversePj ? PJ_FWD : PJ_INV, coord);
                if (fabs(coord.xyzt.x - xIn) > dfThreshold ||
                    fabs(coord.xyzt.y - yIn) > dfThreshold)
                {
                    err  = PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN;
                    x[i] = HUGE_VAL;
                    y[i] = HUGE_VAL;
                }
            }

            if( panErrorCodes )
                panErrorCodes[i] = err;

/* -------------------------------------------------------------------- */
/*      Try to report an error through CPL.  Get proj error string      */
/*      if possible.  Try to avoid reporting thousands of errors.       */
/*      Suppress further error reporting on this OGRProjCT if we        */
/*      have already reported 20 errors.                                */
/* -------------------------------------------------------------------- */
            if( err != 0 )
            {
                if( ++nErrorCount < 20 )
                {
#if PROJ_VERSION_MAJOR >= 8
                    const char *pszError = proj_context_errno_string(ctx, err);
#else
                    const char *pszError = proj_errno_string(err);
#endif
                    if( m_bEmitErrors )
                    {
                        if( pszError == nullptr )
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "Reprojection failed, err = %d", err );
                        else
                            CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );
                    }
                    else
                    {
                        if( pszError == nullptr )
                            CPLDebug("OGRCT",
                                     "Reprojection failed, err = %d", err );
                        else
                            CPLDebug("OGRCT", "%s", pszError );
                    }
                }
                else if( nErrorCount == 20 )
                {
                    if( m_bEmitErrors )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Reprojection failed, err = %d, further errors will be "
                                 "suppressed on the transform object.",
                                 err );
                    }
                    else
                    {
                        CPLDebug("OGRCT",
                                 "Reprojection failed, err = %d, further errors will be "
                                 "suppressed on the transform object.",
                                 err );
                    }
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Potentially do longitude wrapping.                              */
/* -------------------------------------------------------------------- */
    if( bTargetLatLong && bTargetWrap )
    {
        OGRAxisOrientation orientation;
        assert( poSRSTarget );
        poSRSTarget->GetAxis(nullptr, 0, &orientation);
        if( orientation == OAO_East )
        {
            for( int i = 0; i < nCount; i++ )
            {
                if( x[i] != HUGE_VAL && y[i] != HUGE_VAL )
                {
                    if( x[i] < dfTargetWrapLong - 180.0 )
                        x[i] += 360.0;
                    else if( x[i] > dfTargetWrapLong + 180 )
                        x[i] -= 360.0;
                }
            }
        }
        else
        {
            for( int i = 0; i < nCount; i++ )
            {
                if( x[i] != HUGE_VAL && y[i] != HUGE_VAL )
                {
                    if( y[i] < dfTargetWrapLong - 180.0 )
                        y[i] += 360.0;
                    else if( y[i] > dfTargetWrapLong + 180 )
                        y[i] -= 360.0;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Apply data axis to target CRS mapping.                          */
/* -------------------------------------------------------------------- */
    if( poSRSTarget )
    {
        const auto& mapping = poSRSTarget->GetDataAxisToSRSAxisMapping();
        if( mapping.size() >= 2 && (mapping[0] != 1 || mapping[1] != 2) )
        {
            for( int i = 0; i < nCount; i++ )
            {
                double newX = (mapping[0] == 1) ? x[i] :
                    (mapping[0] == -1) ? -x[i] : (mapping[0] == 2) ? y[i] : -y[i];
                double newY = (mapping[1] == 2) ? y[i] :
                    (mapping[1] == -2) ? -y[i] : (mapping[1] == 1) ? x[i] : -x[i];
                x[i] = newX;
                y[i] = newY;
                if( z && mapping.size() >= 3 && mapping[2] == -3)
                    z[i] = -z[i];
            }
        }
    }

#ifdef DEBUG_VERBOSE
    if( bDebugCT )
    {
        CPLDebug("OGRCT", "Out:");
        for( int i = 0; i < nCount; ++i )
        {
            CPLDebug("OGRCT", "  x[%d] = %.16g y[%d] = %.16g",
                     i, x[i], i, y[i]);
        }
    }
#endif
#ifdef DEBUG_PERF
    struct CPLTimeVal tvEnd;
    CPLGettimeofday(&tvEnd, nullptr);
    const double delay = (tvEnd.tv_sec + tvEnd.tv_usec * 1e-6) -
                         (tvStart.tv_sec + tvStart.tv_usec * 1e-6);
    g_dfTotalTimeReprojection += delay;
    //CPLDebug("OGR_CT", "End TransformWithErrorCodes(): %d ms",
    //         static_cast<int>(delay * 1000));
#endif

    return TRUE;
}

/************************************************************************/
/*                            GetInverse()                              */
/************************************************************************/

OGRCoordinateTransformation* OGRProjCT::GetInverse() const
{
    PJ* new_pj = nullptr;
    // m_pj can be nullptr if using m_eStrategy != PROJ
    if( m_pj && !bWebMercatorToWGS84LongLat && !bNoTransform )
    {
        // See https://github.com/OSGeo/PROJ/pull/2582
        // This may fail before PROJ 8.0.1 if the m_pj object is a "meta"
        // operation being a set of real operations
        new_pj = proj_clone(OSRGetProjTLSContext(), m_pj);
    }

    OGRCoordinateTransformationOptions newOptions(m_options);
    std::swap(newOptions.d->bHasSourceCenterLong, newOptions.d->bHasTargetCenterLong);
    std::swap(newOptions.d->dfSourceCenterLong, newOptions.d->dfTargetCenterLong);
    newOptions.d->bReverseCO = !newOptions.d->bReverseCO;
    newOptions.d->RefreshCheckWithInvertProj();

    if( new_pj == nullptr && !bNoTransform )
    {
        return OGRCreateCoordinateTransformation(poSRSTarget, poSRSSource,
                                                 newOptions);
    }

    auto poNewCT = new OGRProjCT();

    if( poSRSTarget )
        poNewCT->poSRSSource = poSRSTarget->Clone();
    poNewCT->bSourceLatLong = bTargetLatLong;
    poNewCT->bSourceWrap = bTargetWrap;
    poNewCT->dfSourceWrapLong = dfTargetWrapLong;

    if( poSRSSource )
        poNewCT->poSRSTarget = poSRSSource->Clone();
    poNewCT->bTargetLatLong = bSourceLatLong;
    poNewCT->bTargetWrap = bSourceWrap;
    poNewCT->dfTargetWrapLong = dfSourceWrapLong;

    poNewCT->ComputeThreshold();

    poNewCT->m_pj = new_pj;
    poNewCT->m_bReversePj = !m_bReversePj;
    poNewCT->bNoTransform = bNoTransform;
    poNewCT->m_eStrategy = m_eStrategy;
    poNewCT->m_options = newOptions;
    return poNewCT;
}

/************************************************************************/
/*                            OSRCTCleanCache()                         */
/************************************************************************/

void OSRCTCleanCache()
{
    std::lock_guard<std::mutex> oGuard(g_oCTCacheMutex);
    delete g_poCTCache;
    g_poCTCache = nullptr;
}

/************************************************************************/
/*                          MakeCacheKey()                              */
/************************************************************************/

CTCacheKey OGRProjCT::MakeCacheKey(const OGRSpatialReference* poSRS1,
                                    const OGRSpatialReference* poSRS2,
                                    const OGRCoordinateTransformationOptions& options)
{
    const auto GetKeyForSRS = [](const OGRSpatialReference* poSRS)
    {
        if (poSRS)
        {
            char* pszText = GetWktOrProjString(poSRS);
            std::string ret(pszText);
            CPLFree(pszText);
            const auto& mapping = poSRS->GetDataAxisToSRSAxisMapping();
            for(const auto& axis: mapping)
            {
                ret += std::to_string(axis);
            }
            return ret;
        }
        else
        {
            return std::string("null");
        }
    };

    std::string ret( GetKeyForSRS(poSRS1) );
    ret += GetKeyForSRS(poSRS2);
    ret += options.d->GetKey();
    return ret;
}

/************************************************************************/
/*                           InsertIntoCache()                          */
/************************************************************************/

void OGRProjCT::InsertIntoCache( OGRProjCT* poCT )
{
    std::lock_guard<std::mutex> oGuard(g_oCTCacheMutex);
    if( g_poCTCache == nullptr )
    {
        g_poCTCache = new lru11::Cache<CTCacheKey, CTCacheValue>();
    }
    const auto key = MakeCacheKey(poCT->poSRSSource, poCT->poSRSTarget,
                                  poCT->m_options);
    if( g_poCTCache->contains(key) )
    {
        delete poCT;
        return;
    }
    g_poCTCache->insert(key, std::make_shared<std::unique_ptr<OGRProjCT>>(
                                            std::unique_ptr<OGRProjCT>(poCT)));
}

/************************************************************************/
/*                            FindFromCache()                           */
/************************************************************************/

OGRProjCT* OGRProjCT::FindFromCache( const OGRSpatialReference *poSource,
                                     const OGRSpatialReference *poTarget,
                                     const OGRCoordinateTransformationOptions& options )
{
    std::lock_guard<std::mutex> oGuard(g_oCTCacheMutex);
    if( g_poCTCache == nullptr || g_poCTCache->empty() )
        return nullptr;

    const auto key = MakeCacheKey(poSource, poTarget, options);
    // Get value from cache and remove it
    CTCacheValue holder;
    if( g_poCTCache->tryGet(key, holder) )
    {
        auto poCT = holder->release();
        g_poCTCache->remove(key);
        return poCT;
    }
    return nullptr;
}

//! @endcond

/************************************************************************/
/*                            OCTTransform()                            */
/************************************************************************/

/** Transform an array of points
 *
 * @param hTransform Transformation object
 * @param nCount Number of points
 * @param x Array of nCount x values.
 * @param y Array of nCount y values.
 * @param z Array of nCount z values.
 * @return TRUE or FALSE
 */
int CPL_STDCALL OCTTransform( OGRCoordinateTransformationH hTransform,
                              int nCount, double *x, double *y, double *z )

{
    VALIDATE_POINTER1( hTransform, "OCTTransform", FALSE );

    return OGRCoordinateTransformation::FromHandle(hTransform)->
        Transform( nCount, x, y, z );
}

/************************************************************************/
/*                           OCTTransformEx()                           */
/************************************************************************/

/** Transform an array of points
 *
 * @param hTransform Transformation object
 * @param nCount Number of points
 * @param x Array of nCount x values.
 * @param y Array of nCount y values.
 * @param z Array of nCount z values.
 * @param pabSuccess Output array of nCount value that will be set to TRUE/FALSE
 * @return TRUE or FALSE
 */
int CPL_STDCALL OCTTransformEx( OGRCoordinateTransformationH hTransform,
                                int nCount, double *x, double *y, double *z,
                                int *pabSuccess )

{
    VALIDATE_POINTER1( hTransform, "OCTTransformEx", FALSE );

    return OGRCoordinateTransformation::FromHandle(hTransform)->
        Transform( nCount, x, y, z, pabSuccess );
}

/************************************************************************/
/*                           OCTTransform4D()                           */
/************************************************************************/

/** Transform an array of points
 *
 * @param hTransform Transformation object
 * @param nCount Number of points
 * @param x Array of nCount x values. Should not be NULL
 * @param y Array of nCount y values. Should not be NULL
 * @param z Array of nCount z values. Might be NULL
 * @param t Array of nCount time values. Might be NULL
 * @param pabSuccess Output array of nCount value that will be set to TRUE/FALSE. Might be NULL.
 * @since GDAL 3.0
 * @return TRUE or FALSE
 */
int OCTTransform4D( OGRCoordinateTransformationH hTransform,
                    int nCount, double *x, double *y, double *z,
                    double *t,
                    int *pabSuccess )

{
    VALIDATE_POINTER1( hTransform, "OCTTransform4D", FALSE );

    return OGRCoordinateTransformation::FromHandle(hTransform)->
        Transform( nCount, x, y, z, t, pabSuccess );
}

/************************************************************************/
/*                      OCTTransform4DWithErrorCodes()                  */
/************************************************************************/

/** Transform an array of points
 *
 * @param hTransform Transformation object
 * @param nCount Number of points
 * @param x Array of nCount x values. Should not be NULL
 * @param y Array of nCount y values. Should not be NULL
 * @param z Array of nCount z values. Might be NULL
 * @param t Array of nCount time values. Might be NULL
 * @param panErrorCodes Output array of nCount value that will be set to 0 for
 *                      success, or a non-zero value for failure. Refer to
 *                      PROJ 8 public error codes. Might be NULL
 * @since GDAL 3.3, and PROJ 8 to be able to use PROJ public error codes
 * @return TRUE or FALSE
 */
int OCTTransform4DWithErrorCodes( OGRCoordinateTransformationH hTransform,
                    int nCount, double *x, double *y, double *z,
                    double *t,
                    int *panErrorCodes )

{
    VALIDATE_POINTER1( hTransform, "OCTTransform4DWithErrorCodes", FALSE );

    return OGRCoordinateTransformation::FromHandle(hTransform)->
        TransformWithErrorCodes( nCount, x, y, z, t, panErrorCodes );
}

/************************************************************************/
/*                         OGRCTDumpStatistics()                        */
/************************************************************************/

void OGRCTDumpStatistics()
{
#ifdef DEBUG_PERF
    CPLDebug("OGR_CT", "Total time in proj_create_crs_to_crs(): %d ms",
             static_cast<int>(g_dfTotalTimeCRStoCRS * 1000));
    CPLDebug("OGR_CT", "Total time in coordinate transformation: %d ms",
             static_cast<int>(g_dfTotalTimeReprojection * 1000));
#endif
}
