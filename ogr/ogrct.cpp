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

#ifdef DEBUG_PERF
static double g_dfTotalTimeCRStoCRS = 0;
static double g_dfTotalTimeReprojection = 0;

/************************************************************************/
/*                        CPLGettimeofday()                             */
/************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <sys/timeb.h>

namespace
{
struct CPLTimeVal
{
    time_t tv_sec; /* seconds */
    long tv_usec;  /* and microseconds */
};
}  // namespace

static void CPLGettimeofday(struct CPLTimeVal *tp, void * /* timezonep*/)
{
    struct _timeb theTime;

    _ftime(&theTime);
    tp->tv_sec = static_cast<time_t>(theTime.time);
    tp->tv_usec = theTime.millitm * 1000;
}
#else
#include <sys/time.h> /* for gettimeofday() */
#define CPLTimeVal timeval
#define CPLGettimeofday(t, u) gettimeofday(t, u)
#endif

#endif  // DEBUG_PERF

// Cache of OGRProjCT objects
static std::mutex g_oCTCacheMutex;
class OGRProjCT;
typedef std::string CTCacheKey;
typedef std::unique_ptr<OGRProjCT> CTCacheValue;
static lru11::Cache<CTCacheKey, CTCacheValue> *g_poCTCache = nullptr;

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
    double dfAccuracy = -1;  // no constraint

    bool bOnlyBest = false;
    bool bOnlyBestOptionSet = false;

    bool bHasSourceCenterLong = false;
    double dfSourceCenterLong = 0.0;

    bool bHasTargetCenterLong = false;
    double dfTargetCenterLong = 0.0;

    bool bCheckWithInvertProj = false;

    Private();
    Private(const Private &) = default;
    Private(Private &&) = default;
    Private &operator=(const Private &) = default;
    Private &operator=(Private &&) = default;

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
    ret += std::to_string(static_cast<int>(bOnlyBestOptionSet));
    ret += std::to_string(static_cast<int>(bOnlyBest));
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
        CPLTestBool(CPLGetConfigOption("CHECK_WITH_INVERT_PROJ", "NO"));
}

/************************************************************************/
/*                       GetAsAProjRecognizableString()                 */
/************************************************************************/

static char *GetAsAProjRecognizableString(const OGRSpatialReference *poSRS)
{
    CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
    // If there's a PROJ4 EXTENSION node in WKT1, then use
    // it. For example when dealing with "+proj=longlat +lon_wrap=180"
    char *pszText = nullptr;
    if (poSRS->GetExtension(nullptr, "PROJ4", nullptr))
    {
        poSRS->exportToProj4(&pszText);
        if (strstr(pszText, " +type=crs") == nullptr)
        {
            auto tmpText = std::string(pszText) + " +type=crs";
            CPLFree(pszText);
            pszText = CPLStrdup(tmpText.c_str());
        }
    }
    else if (poSRS->IsEmpty())
    {
        pszText = CPLStrdup("");
    }
    else
    {
        // We export to PROJJSON rather than WKT2:2019 because PROJJSON
        // is a bit more verbose, which helps in situations like
        // https://github.com/OSGeo/gdal/issues/9732 /
        // https://github.com/OSGeo/PROJ/pull/4124 where we want to export
        // a DerivedProjectedCRS whose base ProjectedCRS has non-metre axis.
        poSRS->exportToPROJJSON(&pszText, nullptr);
    }

    return pszText;
}

/************************************************************************/
/*                        GetTextRepresentation()                       */
/************************************************************************/

static char *GetTextRepresentation(const OGRSpatialReference *poSRS)
{
    const auto CanUseAuthorityDef = [](const OGRSpatialReference *poSRS1,
                                       OGRSpatialReference *poSRSFromAuth,
                                       const char *pszAuth)
    {
        if (EQUAL(pszAuth, "EPSG") &&
            CPLTestBool(
                CPLGetConfigOption("OSR_CT_USE_DEFAULT_EPSG_TOWGS84", "NO")))
        {
            // We don't want by default to honour 'default' TOWGS84 terms that
            // come with the EPSG code because there might be a better
            // transformation from that Typical case if EPSG:31468 "DHDN /
            // 3-degree Gauss-Kruger zone 4" where the DHDN->TOWGS84
            // transformation can use the BETA2007.gsb grid instead of
            // TOWGS84[598.1,73.7,418.2,0.202,0.045,-2.455,6.7] But if the user
            // really wants it, it can set the OSR_CT_USE_DEFAULT_EPSG_TOWGS84
            // configuration option to YES
            double adfTOWGS84_1[7];
            double adfTOWGS84_2[7];

            poSRSFromAuth->AddGuessedTOWGS84();

            if (poSRS1->GetTOWGS84(adfTOWGS84_1) == OGRERR_NONE &&
                poSRSFromAuth->GetTOWGS84(adfTOWGS84_2) == OGRERR_NONE &&
                memcmp(adfTOWGS84_1, adfTOWGS84_2, sizeof(adfTOWGS84_1)) == 0)
            {
                return false;
            }
        }
        return true;
    };

    char *pszText = nullptr;
    // If we have a AUTH:CODE attached, use it to retrieve the full
    // definition in case a trip to WKT1 has lost the area of use.
    // unless OGR_CT_PREFER_OFFICIAL_SRS_DEF=NO (see
    // https://github.com/OSGeo/PROJ/issues/2955)
    const char *pszAuth = poSRS->GetAuthorityName(nullptr);
    const char *pszCode = poSRS->GetAuthorityCode(nullptr);
    if (pszAuth && pszCode &&
        CPLTestBool(
            CPLGetConfigOption("OGR_CT_PREFER_OFFICIAL_SRS_DEF", "YES")))
    {
        CPLString osAuthCode(pszAuth);
        osAuthCode += ':';
        osAuthCode += pszCode;
        OGRSpatialReference oTmpSRS;
        oTmpSRS.SetFromUserInput(osAuthCode);
        oTmpSRS.SetDataAxisToSRSAxisMapping(
            poSRS->GetDataAxisToSRSAxisMapping());
        const char *const apszOptionsIsSame[] = {"CRITERION=EQUIVALENT",
                                                 nullptr};
        if (oTmpSRS.IsSame(poSRS, apszOptionsIsSame))
        {
            if (CanUseAuthorityDef(poSRS, &oTmpSRS, pszAuth))
            {
                pszText = CPLStrdup(osAuthCode);
            }
        }
    }
    if (pszText == nullptr)
    {
        pszText = GetAsAProjRecognizableString(poSRS);
    }
    return pszText;
}

/************************************************************************/
/*                  OGRCoordinateTransformationOptions()                */
/************************************************************************/

/** \brief Constructs a new OGRCoordinateTransformationOptions.
 *
 * @since GDAL 3.0
 */
OGRCoordinateTransformationOptions::OGRCoordinateTransformationOptions()
    : d(new Private())
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
    const OGRCoordinateTransformationOptions &other)
    : d(new Private(*(other.d)))
{
}

/************************************************************************/
/*                          operator =()                                */
/************************************************************************/

/** \brief Assignment operator
 *
 * @since GDAL 3.1
 */
OGRCoordinateTransformationOptions &
OGRCoordinateTransformationOptions::operator=(
    const OGRCoordinateTransformationOptions &other)
{
    if (this != &other)
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
    if (std::fabs(dfWestLongitudeDeg) > 180)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfWestLongitudeDeg");
        return false;
    }
    if (std::fabs(dfSouthLatitudeDeg) > 90)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfSouthLatitudeDeg");
        return false;
    }
    if (std::fabs(dfEastLongitudeDeg) > 180)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfEastLongitudeDeg");
        return false;
    }
    if (std::fabs(dfNorthLatitudeDeg) > 90)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dfNorthLatitudeDeg");
        return false;
    }
    if (dfSouthLatitudeDeg > dfNorthLatitudeDeg)
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
    OGRCoordinateTransformationOptionsH hOptions, double dfWestLongitudeDeg,
    double dfSouthLatitudeDeg, double dfEastLongitudeDeg,
    double dfNorthLatitudeDeg)
{
    return hOptions->SetAreaOfInterest(dfWestLongitudeDeg, dfSouthLatitudeDeg,
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
 * @param bReverseCO Whether the PROJ or WKT string should be evaluated in the
 * reverse path
 * @return true in case of success.
 *
 * @since GDAL 3.0
 */
bool OGRCoordinateTransformationOptions::SetCoordinateOperation(
    const char *pszCO, bool bReverseCO)
{
    d->osCoordOperation = pszCO ? pszCO : "";
    d->bReverseCO = bReverseCO;
    return true;
}

/************************************************************************/
/*                         SetSourceCenterLong()                        */
/************************************************************************/

/*! @cond Doxygen_Suppress */
void OGRCoordinateTransformationOptions::SetSourceCenterLong(
    double dfCenterLong)
{
    d->dfSourceCenterLong = dfCenterLong;
    d->bHasSourceCenterLong = true;
}

/*! @endcond */

/************************************************************************/
/*                         SetTargetCenterLong()                        */
/************************************************************************/

/*! @cond Doxygen_Suppress */
void OGRCoordinateTransformationOptions::SetTargetCenterLong(
    double dfCenterLong)
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
    OGRCoordinateTransformationOptionsH hOptions, const char *pszCO,
    int bReverseCO)
{
    // cppcheck-suppress knownConditionTrueFalse
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
 * An accuracy of 0 is valid and means a coordinate operation made only of one
 * or several conversions (map projections, unit conversion, etc.) Operations
 * involving ballpark transformations have a unknown accuracy, and will be
 * filtered out by any dfAccuracy >= 0 value.
 *
 * If this option is specified with PROJ < 8, the OGR_CT_OP_SELECTION
 * configuration option will default to BEST_ACCURACY.
 *
 * @param dfAccuracy accuracy in meters (or a negative value to disable this
 * filter)
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
    // cppcheck-suppress knownConditionTrueFalse
    return hOptions->SetDesiredAccuracy(dfAccuracy);
}

/************************************************************************/
/*                       SetBallparkAllowed()                           */
/************************************************************************/

/** \brief Sets whether ballpark transformations are allowed.
 *
 * By default, PROJ may generate "ballpark transformations" (see
 * https://proj.org/glossary.html) when precise datum transformations are
 * missing. For high accuracy use cases, such transformations might not be
 * allowed.
 *
 * If this option is specified with PROJ < 8, the OGR_CT_OP_SELECTION
 * configuration option will default to BEST_ACCURACY.
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
    // cppcheck-suppress knownConditionTrueFalse
    return hOptions->SetBallparkAllowed(CPL_TO_BOOL(bAllowBallpark));
}

/************************************************************************/
/*                         SetOnlyBest()                                */
/************************************************************************/

/** \brief Sets whether only the "best" operation should be used.
 *
 * By default (at least in the PROJ 9.x series), PROJ may use coordinate
 * operations that are not the "best" if resources (typically grids) needed
 * to use them are missing. It will then fallback to other coordinate operations
 * that have a lesser accuracy, for example using Helmert transformations,
 * or in the absence of such operations, to ones with potential very rought
 * accuracy, using "ballpark" transformations
 * (see https://proj.org/glossary.html).
 *
 * When calling this method with bOnlyBest = true, PROJ will only consider the
 * "best" operation, and error out (at Transform() time) if they cannot be
 * used.
 * This method may be used together with SetBallparkAllowed(false) to
 * only allow best operations that have a known accuracy.
 *
 * Note that this method has no effect on PROJ versions before 9.2.
 *
 * The default value for this option can be also set with the
 * PROJ_ONLY_BEST_DEFAULT environment variable, or with the "only_best_default"
 * setting of proj.ini. Calling SetOnlyBest() overrides such default value.
 *
 * @param bOnlyBest set to true to ask PROJ to use only the best operation(s)
 *
 * @since GDAL 3.8 and PROJ 9.2
 */
bool OGRCoordinateTransformationOptions::SetOnlyBest(bool bOnlyBest)
{
    d->bOnlyBest = bOnlyBest;
    d->bOnlyBestOptionSet = true;
    return true;
}

/************************************************************************/
/*        OCTCoordinateTransformationOptionsSetOnlyBest()               */
/************************************************************************/

/** \brief Sets whether only the "best" operation(s) should be used.
 *
 * See OGRCoordinateTransformationOptions::SetOnlyBest()
 *
 * @since GDAL 3.8 and PROJ 9.2
 */
int OCTCoordinateTransformationOptionsSetOnlyBest(
    OGRCoordinateTransformationOptionsH hOptions, bool bOnlyBest)
{
    // cppcheck-suppress knownConditionTrueFalse
    return hOptions->SetOnlyBest(bOnlyBest);
}

/************************************************************************/
/*                              OGRProjCT                               */
/************************************************************************/

//! @cond Doxygen_Suppress
class OGRProjCT : public OGRCoordinateTransformation
{
    class PjPtr
    {
        PJ *m_pj = nullptr;

        void reset()
        {
            if (m_pj)
            {
                proj_assign_context(m_pj, OSRGetProjTLSContext());
                proj_destroy(m_pj);
            }
        }

      public:
        PjPtr() : m_pj(nullptr)
        {
        }

        explicit PjPtr(PJ *pjIn) : m_pj(pjIn)
        {
        }

        ~PjPtr()
        {
            reset();
        }

        PjPtr(const PjPtr &other)
            : m_pj((other.m_pj != nullptr)
                       ? (proj_clone(OSRGetProjTLSContext(), other.m_pj))
                       : (nullptr))
        {
        }

        PjPtr(PjPtr &&other) : m_pj(other.m_pj)
        {
            other.m_pj = nullptr;
        }

        PjPtr &operator=(const PjPtr &other)
        {
            if (this != &other)
            {
                reset();
                m_pj = (other.m_pj != nullptr)
                           ? (proj_clone(OSRGetProjTLSContext(), other.m_pj))
                           : (nullptr);
            }
            return *this;
        }

        PjPtr &operator=(PJ *pjIn)
        {
            if (m_pj != pjIn)
            {
                reset();
                m_pj = pjIn;
            }
            return *this;
        }

        operator PJ *()
        {
            return m_pj;
        }

        operator const PJ *() const
        {
            return m_pj;
        }
    };

    OGRSpatialReference *poSRSSource = nullptr;
    OGRAxisOrientation m_eSourceFirstAxisOrient = OAO_Other;
    bool bSourceLatLong = false;
    bool bSourceWrap = false;
    double dfSourceWrapLong = 0.0;
    bool bSourceIsDynamicCRS = false;
    double dfSourceCoordinateEpoch = 0.0;
    std::string m_osSrcSRS{};  // WKT, PROJ4 or AUTH:CODE

    OGRSpatialReference *poSRSTarget = nullptr;
    OGRAxisOrientation m_eTargetFirstAxisOrient = OAO_Other;
    bool bTargetLatLong = false;
    bool bTargetWrap = false;
    double dfTargetWrapLong = 0.0;
    bool bTargetIsDynamicCRS = false;
    double dfTargetCoordinateEpoch = 0.0;
    std::string m_osTargetSRS{};  // WKT, PROJ4 or AUTH:CODE

    bool bWebMercatorToWGS84LongLat = false;

    size_t nErrorCount = 0;

    double dfThreshold = 0.0;

    PjPtr m_pj{};
    bool m_bReversePj = false;

    bool m_bEmitErrors = true;

    bool bNoTransform = false;

    enum class Strategy
    {
        PROJ,
        BEST_ACCURACY,
        FIRST_MATCHING
    };
    Strategy m_eStrategy = Strategy::PROJ;

    bool
    ListCoordinateOperations(const char *pszSrcSRS, const char *pszTargetSRS,
                             const OGRCoordinateTransformationOptions &options);

    struct Transformation
    {
        double minx = 0.0;
        double miny = 0.0;
        double maxx = 0.0;
        double maxy = 0.0;
        PjPtr pj{};
        CPLString osName{};
        CPLString osProjString{};
        double accuracy = 0.0;

        Transformation(double minxIn, double minyIn, double maxxIn,
                       double maxyIn, PJ *pjIn, const CPLString &osNameIn,
                       const CPLString &osProjStringIn, double accuracyIn)
            : minx(minxIn), miny(minyIn), maxx(maxxIn), maxy(maxyIn), pj(pjIn),
              osName(osNameIn), osProjString(osProjStringIn),
              accuracy(accuracyIn)
        {
        }
    };

    std::vector<Transformation> m_oTransformations{};
    int m_iCurTransformation = -1;
    OGRCoordinateTransformationOptions m_options{};

    void ComputeThreshold();
    void DetectWebMercatorToWGS84();

    OGRProjCT(const OGRProjCT &other);
    OGRProjCT &operator=(const OGRProjCT &) = delete;

    static CTCacheKey
    MakeCacheKey(const OGRSpatialReference *poSRS1, const char *pszSrcSRS,
                 const OGRSpatialReference *poSRS2, const char *pszTargetSRS,
                 const OGRCoordinateTransformationOptions &options);
    bool ContainsNorthPole(const double xmin, const double ymin,
                           const double xmax, const double ymax,
                           bool lon_lat_order);
    bool ContainsSouthPole(const double xmin, const double ymin,
                           const double xmax, const double ymax,
                           bool lon_lat_order);

  public:
    OGRProjCT();
    ~OGRProjCT() override;

    int Initialize(const OGRSpatialReference *poSource, const char *pszSrcSRS,
                   const OGRSpatialReference *poTarget,
                   const char *pszTargetSRS,
                   const OGRCoordinateTransformationOptions &options);

    const OGRSpatialReference *GetSourceCS() const override;
    const OGRSpatialReference *GetTargetCS() const override;

    int Transform(size_t nCount, double *x, double *y, double *z, double *t,
                  int *pabSuccess) override;

    int TransformWithErrorCodes(size_t nCount, double *x, double *y, double *z,
                                double *t, int *panErrorCodes) override;

    int TransformBounds(const double xmin, const double ymin, const double xmax,
                        const double ymax, double *out_xmin, double *out_ymin,
                        double *out_xmax, double *out_ymax,
                        const int densify_pts) override;

    bool GetEmitErrors() const override
    {
        return m_bEmitErrors;
    }

    void SetEmitErrors(bool bEmitErrors) override
    {
        m_bEmitErrors = bEmitErrors;
    }

    OGRCoordinateTransformation *Clone() const override;

    OGRCoordinateTransformation *GetInverse() const override;

    static void InsertIntoCache(OGRProjCT *poCT);

    static OGRProjCT *
    FindFromCache(const OGRSpatialReference *poSource, const char *pszSrcSRS,
                  const OGRSpatialReference *poTarget, const char *pszTargetSRS,
                  const OGRCoordinateTransformationOptions &options);
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
OCTDestroyCoordinateTransformation(OGRCoordinateTransformationH hCT)

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

void OGRCoordinateTransformation::DestroyCT(OGRCoordinateTransformation *poCT)
{
    auto poProjCT = dynamic_cast<OGRProjCT *>(poCT);
    if (poProjCT)
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
 * To have a behavior similar to GDAL &lt; 3.0, the
 * OGR_CT_FORCE_TRADITIONAL_GIS_ORDER configuration option can be set to YES.
 *
 * @param poSource source spatial reference system.
 * @param poTarget target spatial reference system.
 * @return NULL on failure or a ready to use transformation object.
 */

OGRCoordinateTransformation *
OGRCreateCoordinateTransformation(const OGRSpatialReference *poSource,
                                  const OGRSpatialReference *poTarget)

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
 * To have a behavior similar to GDAL &lt; 3.0, the
 * OGR_CT_FORCE_TRADITIONAL_GIS_ORDER configuration option can be set to YES.
 *
 * The source SRS and target SRS should generally not be NULL. This is only
 * allowed if a custom coordinate operation is set through the hOptions
 * argument.
 *
 * Starting with GDAL 3.0.3, the OGR_CT_OP_SELECTION configuration option can be
 * set to PROJ (default if PROJ >= 6.3), BEST_ACCURACY or FIRST_MATCHING to
 * decide of the strategy to select the operation to use among candidates, whose
 * area of use is compatible with the points to transform. It is only taken into
 * account if no user defined coordinate transformation pipeline has been
 * specified. <ul> <li>PROJ means the default behavior used by PROJ
 * proj_create_crs_to_crs(). In particular the operation to use among several
 * initial candidates is evaluated for each point to transform.</li>
 * <li>BEST_ACCURACY means the operation whose accuracy is best. It should be
 *     close to PROJ behavior, except that the operation to select is decided
 *     for the average point of the coordinates passed in a single Transform()
 * call. Note: if the OGRCoordinateTransformationOptions::SetDesiredAccuracy()
 * or OGRCoordinateTransformationOptions::SetBallparkAllowed() methods are
 * called with PROJ < 8, this strategy will be selected instead of PROJ.
 * </li>
 * <li>FIRST_MATCHING is the operation ordered first in the list of candidates:
 *     it will not necessarily have the best accuracy, but generally a larger
 * area of use.  It is evaluated for the average point of the coordinates passed
 * in a single Transform() call. This was the default behavior for GDAL 3.0.0 to
 *     3.0.2</li>
 * </ul>
 *
 * By default, if the source or target SRS definition refers to an official
 * CRS through a code, GDAL will use the official definition if the official
 * definition and the source/target SRS definition are equivalent. Note that
 * TOWGS84[] clauses are ignored when checking equivalence. Starting with
 * GDAL 3.4.1, if you set the OGR_CT_PREFER_OFFICIAL_SRS_DEF configuration
 * option to NO, the source or target SRS definition will be always used.
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

OGRCoordinateTransformation *OGRCreateCoordinateTransformation(
    const OGRSpatialReference *poSource, const OGRSpatialReference *poTarget,
    const OGRCoordinateTransformationOptions &options)

{
    char *pszSrcSRS = poSource ? GetTextRepresentation(poSource) : nullptr;
    char *pszTargetSRS = poTarget ? GetTextRepresentation(poTarget) : nullptr;
    // Try to find if we have a match in the case
    OGRProjCT *poCT = OGRProjCT::FindFromCache(poSource, pszSrcSRS, poTarget,
                                               pszTargetSRS, options);
    if (poCT == nullptr)
    {
        poCT = new OGRProjCT();
        if (!poCT->Initialize(poSource, pszSrcSRS, poTarget, pszTargetSRS,
                              options))
        {
            delete poCT;
            poCT = nullptr;
        }
    }
    CPLFree(pszSrcSRS);
    CPLFree(pszTargetSRS);

    return poCT;
}

/************************************************************************/
/*                   OCTNewCoordinateTransformation()                   */
/************************************************************************/

/**
 * Create transformation object.
 *
 * This is the same as the C++ function OGRCreateCoordinateTransformation(const
 * OGRSpatialReference *, const OGRSpatialReference *)
 *
 * Input spatial reference system objects are assigned
 * by copy (calling clone() method) and no ownership transfer occurs.
 *
 * OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects.
 *
 * This will honour the axis order advertized by the source and target SRS,
 * as well as their "data axis to SRS axis mapping".
 * To have a behavior similar to GDAL &lt; 3.0, the
 * OGR_CT_FORCE_TRADITIONAL_GIS_ORDER configuration option can be set to YES.
 *
 * @param hSourceSRS source spatial reference system.
 * @param hTargetSRS target spatial reference system.
 * @return NULL on failure or a ready to use transformation object.
 */

OGRCoordinateTransformationH CPL_STDCALL OCTNewCoordinateTransformation(
    OGRSpatialReferenceH hSourceSRS, OGRSpatialReferenceH hTargetSRS)

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
 * This is the same as the C++ function OGRCreateCoordinateTransformation(const
 * OGRSpatialReference *, const OGRSpatialReference *, const
 * OGRCoordinateTransformationOptions& )
 *
 * Input spatial reference system objects are assigned
 * by copy (calling clone() method) and no ownership transfer occurs.
 *
 * OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects.
 *
 * The source SRS and target SRS should generally not be NULL. This is only
 * allowed if a custom coordinate operation is set through the hOptions
 * argument.
 *
 * This will honour the axis order advertized by the source and target SRS,
 * as well as their "data axis to SRS axis mapping".
 * To have a behavior similar to GDAL &lt; 3.0, the
 * OGR_CT_FORCE_TRADITIONAL_GIS_ORDER configuration option can be set to YES.
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
OCTNewCoordinateTransformationEx(OGRSpatialReferenceH hSourceSRS,
                                 OGRSpatialReferenceH hTargetSRS,
                                 OGRCoordinateTransformationOptionsH hOptions)

{
    return reinterpret_cast<OGRCoordinateTransformationH>(
        OGRCreateCoordinateTransformation(
            reinterpret_cast<OGRSpatialReference *>(hSourceSRS),
            reinterpret_cast<OGRSpatialReference *>(hTargetSRS),
            hOptions ? *hOptions : OGRCoordinateTransformationOptions()));
}

/************************************************************************/
/*                              OCTClone()                              */
/************************************************************************/

/**
 * Clone transformation object.
 *
 * This is the same as the C++ function OGRCreateCoordinateTransformation::Clone
 *
 * @return handle to transformation's clone or NULL on error,
 *         must be freed with OCTDestroyCoordinateTransformation
 *
 * @since GDAL 3.4
 */

OGRCoordinateTransformationH OCTClone(OGRCoordinateTransformationH hTransform)

{
    VALIDATE_POINTER1(hTransform, "OCTClone", nullptr);
    return OGRCoordinateTransformation::ToHandle(
        OGRCoordinateTransformation::FromHandle(hTransform)->Clone());
}

/************************************************************************/
/*                             OCTGetSourceCS()                         */
/************************************************************************/

/**
 * Transformation's source coordinate system reference.
 *
 * This is the same as the C++ function
 * OGRCreateCoordinateTransformation::GetSourceCS
 *
 * @return handle to transformation's source coordinate system or NULL if not
 * present.
 *
 * The ownership of the returned SRS belongs to the transformation object, and
 * the returned SRS should not be modified.
 *
 * @since GDAL 3.4
 */

OGRSpatialReferenceH OCTGetSourceCS(OGRCoordinateTransformationH hTransform)

{
    VALIDATE_POINTER1(hTransform, "OCTGetSourceCS", nullptr);
    return OGRSpatialReference::ToHandle(const_cast<OGRSpatialReference *>(
        OGRCoordinateTransformation::FromHandle(hTransform)->GetSourceCS()));
}

/************************************************************************/
/*                             OCTGetTargetCS()                         */
/************************************************************************/

/**
 * Transformation's target coordinate system reference.
 *
 * This is the same as the C++ function
 * OGRCreateCoordinateTransformation::GetTargetCS
 *
 * @return handle to transformation's target coordinate system or NULL if not
 * present.
 *
 * The ownership of the returned SRS belongs to the transformation object, and
 * the returned SRS should not be modified.
 *
 * @since GDAL 3.4
 */

OGRSpatialReferenceH OCTGetTargetCS(OGRCoordinateTransformationH hTransform)

{
    VALIDATE_POINTER1(hTransform, "OCTGetTargetCS", nullptr);
    return OGRSpatialReference::ToHandle(const_cast<OGRSpatialReference *>(
        OGRCoordinateTransformation::FromHandle(hTransform)->GetTargetCS()));
}

/************************************************************************/
/*                             OCTGetInverse()                          */
/************************************************************************/

/**
 * Inverse transformation object.
 *
 * This is the same as the C++ function
 * OGRCreateCoordinateTransformation::GetInverse
 *
 * @return handle to inverse transformation or NULL on error,
 *         must be freed with OCTDestroyCoordinateTransformation
 *
 * @since GDAL 3.4
 */

OGRCoordinateTransformationH CPL_DLL
OCTGetInverse(OGRCoordinateTransformationH hTransform)

{
    VALIDATE_POINTER1(hTransform, "OCTGetInverse", nullptr);
    return OGRCoordinateTransformation::ToHandle(
        OGRCoordinateTransformation::FromHandle(hTransform)->GetInverse());
}

/************************************************************************/
/*                             OGRProjCT()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRProjCT::OGRProjCT()
{
}

/************************************************************************/
/*                  OGRProjCT(const OGRProjCT& other)                   */
/************************************************************************/

OGRProjCT::OGRProjCT(const OGRProjCT &other)
    : poSRSSource((other.poSRSSource != nullptr) ? (other.poSRSSource->Clone())
                                                 : (nullptr)),
      m_eSourceFirstAxisOrient(other.m_eSourceFirstAxisOrient),
      bSourceLatLong(other.bSourceLatLong), bSourceWrap(other.bSourceWrap),
      dfSourceWrapLong(other.dfSourceWrapLong),
      bSourceIsDynamicCRS(other.bSourceIsDynamicCRS),
      dfSourceCoordinateEpoch(other.dfSourceCoordinateEpoch),
      m_osSrcSRS(other.m_osSrcSRS),
      poSRSTarget((other.poSRSTarget != nullptr) ? (other.poSRSTarget->Clone())
                                                 : (nullptr)),
      m_eTargetFirstAxisOrient(other.m_eTargetFirstAxisOrient),
      bTargetLatLong(other.bTargetLatLong), bTargetWrap(other.bTargetWrap),
      dfTargetWrapLong(other.dfTargetWrapLong),
      bTargetIsDynamicCRS(other.bTargetIsDynamicCRS),
      dfTargetCoordinateEpoch(other.dfTargetCoordinateEpoch),
      m_osTargetSRS(other.m_osTargetSRS),
      bWebMercatorToWGS84LongLat(other.bWebMercatorToWGS84LongLat),
      nErrorCount(other.nErrorCount), dfThreshold(other.dfThreshold),
      m_pj(other.m_pj), m_bReversePj(other.m_bReversePj),
      m_bEmitErrors(other.m_bEmitErrors), bNoTransform(other.bNoTransform),
      m_eStrategy(other.m_eStrategy),
      m_oTransformations(other.m_oTransformations),
      m_iCurTransformation(other.m_iCurTransformation),
      m_options(other.m_options)
{
}

/************************************************************************/
/*                            ~OGRProjCT()                             */
/************************************************************************/

OGRProjCT::~OGRProjCT()

{
    if (poSRSSource != nullptr)
    {
        poSRSSource->Release();
    }

    if (poSRSTarget != nullptr)
    {
        poSRSTarget->Release();
    }
}

/************************************************************************/
/*                          ComputeThreshold()                          */
/************************************************************************/

void OGRProjCT::ComputeThreshold()
{
    // The threshold is experimental. Works well with the cases of ticket #2305.
    if (bSourceLatLong)
    {
        // coverity[tainted_data]
        dfThreshold = CPLAtof(CPLGetConfigOption("THRESHOLD", ".1"));
    }
    else
    {
        // 1 works well for most projections, except for +proj=aeqd that
        // requires a tolerance of 10000.
        // coverity[tainted_data]
        dfThreshold = CPLAtof(CPLGetConfigOption("THRESHOLD", "10000"));
    }
}

/************************************************************************/
/*                        DetectWebMercatorToWGS84()                    */
/************************************************************************/

void OGRProjCT::DetectWebMercatorToWGS84()
{
    // Detect webmercator to WGS84
    if (m_options.d->osCoordOperation.empty() && poSRSSource && poSRSTarget &&
        poSRSSource->IsProjected() && poSRSTarget->IsGeographic() &&
        ((m_eTargetFirstAxisOrient == OAO_North &&
          poSRSTarget->GetDataAxisToSRSAxisMapping() ==
              std::vector<int>{2, 1}) ||
         (m_eTargetFirstAxisOrient == OAO_East &&
          poSRSTarget->GetDataAxisToSRSAxisMapping() ==
              std::vector<int>{1, 2})))
    {
        // Examine SRS ID before going to Proj4 string for faster execution
        // This assumes that the SRS definition is "not lying", that is, it
        // is equivalent to the resolution of the official EPSG code.
        const char *pszSourceAuth = poSRSSource->GetAuthorityName(nullptr);
        const char *pszSourceCode = poSRSSource->GetAuthorityCode(nullptr);
        const char *pszTargetAuth = poSRSTarget->GetAuthorityName(nullptr);
        const char *pszTargetCode = poSRSTarget->GetAuthorityCode(nullptr);
        if (pszSourceAuth && pszSourceCode && pszTargetAuth && pszTargetCode &&
            EQUAL(pszSourceAuth, "EPSG") && EQUAL(pszTargetAuth, "EPSG"))
        {
            bWebMercatorToWGS84LongLat =
                (EQUAL(pszSourceCode, "3857") ||
                 EQUAL(pszSourceCode, "3785") ||     // deprecated
                 EQUAL(pszSourceCode, "900913")) &&  // deprecated
                EQUAL(pszTargetCode, "4326");
        }
        else
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            char *pszSrcProj4Defn = nullptr;
            poSRSSource->exportToProj4(&pszSrcProj4Defn);

            char *pszDstProj4Defn = nullptr;
            poSRSTarget->exportToProj4(&pszDstProj4Defn);
            CPLPopErrorHandler();

            if (pszSrcProj4Defn && pszDstProj4Defn)
            {
                if (pszSrcProj4Defn[0] != '\0' &&
                    pszSrcProj4Defn[strlen(pszSrcProj4Defn) - 1] == ' ')
                    pszSrcProj4Defn[strlen(pszSrcProj4Defn) - 1] = 0;
                if (pszDstProj4Defn[0] != '\0' &&
                    pszDstProj4Defn[strlen(pszDstProj4Defn) - 1] == ' ')
                    pszDstProj4Defn[strlen(pszDstProj4Defn) - 1] = 0;
                char *pszNeedle = strstr(pszSrcProj4Defn, "  ");
                if (pszNeedle)
                    memmove(pszNeedle, pszNeedle + 1,
                            strlen(pszNeedle + 1) + 1);
                pszNeedle = strstr(pszDstProj4Defn, "  ");
                if (pszNeedle)
                    memmove(pszNeedle, pszNeedle + 1,
                            strlen(pszNeedle + 1) + 1);

                if ((strstr(pszDstProj4Defn, "+datum=WGS84") != nullptr ||
                     strstr(pszDstProj4Defn,
                            "+ellps=WGS84 +towgs84=0,0,0,0,0,0,0 ") !=
                         nullptr) &&
                    strstr(pszSrcProj4Defn, "+nadgrids=@null ") != nullptr &&
                    strstr(pszSrcProj4Defn, "+towgs84") == nullptr)
                {
                    char *pszDst =
                        strstr(pszDstProj4Defn, "+towgs84=0,0,0,0,0,0,0 ");
                    if (pszDst != nullptr)
                    {
                        char *pszSrc =
                            pszDst + strlen("+towgs84=0,0,0,0,0,0,0 ");
                        memmove(pszDst, pszSrc, strlen(pszSrc) + 1);
                    }
                    else
                    {
                        memcpy(strstr(pszDstProj4Defn, "+datum=WGS84"),
                               "+ellps", 6);
                    }

                    pszDst = strstr(pszSrcProj4Defn, "+nadgrids=@null ");
                    char *pszSrc = pszDst + strlen("+nadgrids=@null ");
                    memmove(pszDst, pszSrc, strlen(pszSrc) + 1);

                    pszDst = strstr(pszSrcProj4Defn, "+wktext ");
                    if (pszDst)
                    {
                        pszSrc = pszDst + strlen("+wktext ");
                        memmove(pszDst, pszSrc, strlen(pszSrc) + 1);
                    }
                    bWebMercatorToWGS84LongLat =
                        strcmp(pszDstProj4Defn,
                               "+proj=longlat +ellps=WGS84 +no_defs") == 0 &&
                        (strcmp(pszSrcProj4Defn,
                                "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 "
                                "+lon_0=0.0 "
                                "+x_0=0.0 +y_0=0 +k=1.0 +units=m +no_defs") ==
                             0 ||
                         strcmp(pszSrcProj4Defn,
                                "+proj=merc +a=6378137 +b=6378137 +lat_ts=0 "
                                "+lon_0=0 "
                                "+x_0=0 +y_0=0 +k=1 +units=m +no_defs") == 0);
                }
            }

            CPLFree(pszSrcProj4Defn);
            CPLFree(pszDstProj4Defn);
        }

        if (bWebMercatorToWGS84LongLat)
        {
            CPLDebug("OGRCT", "Using WebMercator to WGS84 optimization");
        }
    }
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRProjCT::Initialize(const OGRSpatialReference *poSourceIn,
                          const char *pszSrcSRS,
                          const OGRSpatialReference *poTargetIn,
                          const char *pszTargetSRS,
                          const OGRCoordinateTransformationOptions &options)

{
    m_options = options;

    if (poSourceIn == nullptr || poTargetIn == nullptr)
    {
        if (options.d->osCoordOperation.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "OGRProjCT::Initialize(): if source and/or target CRS "
                     "are null, a coordinate operation must be specified");
            return FALSE;
        }
    }

    if (poSourceIn)
    {
        poSRSSource = poSourceIn->Clone();
        m_osSrcSRS = pszSrcSRS;
    }
    if (poTargetIn)
    {
        poSRSTarget = poTargetIn->Clone();
        m_osTargetSRS = pszTargetSRS;
    }

    // To easy quick&dirty compatibility with GDAL < 3.0
    if (CPLTestBool(
            CPLGetConfigOption("OGR_CT_FORCE_TRADITIONAL_GIS_ORDER", "NO")))
    {
        if (poSRSSource)
            poSRSSource->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRSTarget)
            poSRSTarget->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    if (poSRSSource)
    {
        bSourceLatLong = CPL_TO_BOOL(poSRSSource->IsGeographic());
        bSourceIsDynamicCRS = poSRSSource->IsDynamic();
        dfSourceCoordinateEpoch = poSRSSource->GetCoordinateEpoch();
        if (!bSourceIsDynamicCRS && dfSourceCoordinateEpoch > 0)
        {
            bSourceIsDynamicCRS = poSRSSource->HasPointMotionOperation();
        }
        poSRSSource->GetAxis(nullptr, 0, &m_eSourceFirstAxisOrient);
    }
    if (poSRSTarget)
    {
        bTargetLatLong = CPL_TO_BOOL(poSRSTarget->IsGeographic());
        bTargetIsDynamicCRS = poSRSTarget->IsDynamic();
        dfTargetCoordinateEpoch = poSRSTarget->GetCoordinateEpoch();
        if (!bTargetIsDynamicCRS && dfTargetCoordinateEpoch > 0)
        {
            bTargetIsDynamicCRS = poSRSTarget->HasPointMotionOperation();
        }
        poSRSTarget->GetAxis(nullptr, 0, &m_eTargetFirstAxisOrient);
    }

#if PROJ_VERSION_MAJOR < 9 ||                                                  \
    (PROJ_VERSION_MAJOR == 9 && PROJ_VERSION_MINOR < 4)
    if (bSourceIsDynamicCRS && bTargetIsDynamicCRS &&
        dfSourceCoordinateEpoch > 0 && dfTargetCoordinateEpoch > 0 &&
        dfSourceCoordinateEpoch != dfTargetCoordinateEpoch)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Coordinate transformation between different epochs only"
                 "supported since PROJ 9.4");
    }
#endif

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
    if (CPLGetConfigOption("CENTER_LONG", nullptr) != nullptr)
    {
        bSourceWrap = true;
        bTargetWrap = true;
        // coverity[tainted_data]
        dfSourceWrapLong = dfTargetWrapLong =
            CPLAtof(CPLGetConfigOption("CENTER_LONG", ""));
        CPLDebug("OGRCT", "Wrap at %g.", dfSourceWrapLong);
    }

    const char *pszCENTER_LONG;
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        pszCENTER_LONG =
            poSRSSource ? poSRSSource->GetExtension("GEOGCS", "CENTER_LONG")
                        : nullptr;
    }
    if (pszCENTER_LONG != nullptr)
    {
        dfSourceWrapLong = CPLAtof(pszCENTER_LONG);
        bSourceWrap = true;
        CPLDebug("OGRCT", "Wrap source at %g.", dfSourceWrapLong);
    }
    else if (bSourceLatLong && options.d->bHasSourceCenterLong)
    {
        dfSourceWrapLong = options.d->dfSourceCenterLong;
        bSourceWrap = true;
        CPLDebug("OGRCT", "Wrap source at %g.", dfSourceWrapLong);
    }

    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        pszCENTER_LONG =
            poSRSTarget ? poSRSTarget->GetExtension("GEOGCS", "CENTER_LONG")
                        : nullptr;
    }
    if (pszCENTER_LONG != nullptr)
    {
        dfTargetWrapLong = CPLAtof(pszCENTER_LONG);
        bTargetWrap = true;
        CPLDebug("OGRCT", "Wrap target at %g.", dfTargetWrapLong);
    }
    else if (bTargetLatLong && options.d->bHasTargetCenterLong)
    {
        dfTargetWrapLong = options.d->dfTargetCenterLong;
        bTargetWrap = true;
        CPLDebug("OGRCT", "Wrap target at %g.", dfTargetWrapLong);
    }

    ComputeThreshold();

    DetectWebMercatorToWGS84();

    const char *pszCTOpSelection =
        CPLGetConfigOption("OGR_CT_OP_SELECTION", nullptr);
    if (pszCTOpSelection)
    {
        if (EQUAL(pszCTOpSelection, "PROJ"))
            m_eStrategy = Strategy::PROJ;
        else if (EQUAL(pszCTOpSelection, "BEST_ACCURACY"))
            m_eStrategy = Strategy::BEST_ACCURACY;
        else if (EQUAL(pszCTOpSelection, "FIRST_MATCHING"))
            m_eStrategy = Strategy::FIRST_MATCHING;
        else
            CPLError(CE_Warning, CPLE_NotSupported,
                     "OGR_CT_OP_SELECTION=%s not supported", pszCTOpSelection);
    }
#if PROJ_VERSION_MAJOR < 8
    else
    {
        if (options.d->dfAccuracy >= 0 || !options.d->bAllowBallpark)
        {
            m_eStrategy = Strategy::BEST_ACCURACY;
        }
    }
#endif
    if (m_eStrategy == Strategy::PROJ)
    {
        const char *pszUseApproxTMERC =
            CPLGetConfigOption("OSR_USE_APPROX_TMERC", nullptr);
        if (pszUseApproxTMERC && CPLTestBool(pszUseApproxTMERC))
        {
            CPLDebug("OSRCT", "Using OGR_CT_OP_SELECTION=BEST_ACCURACY as "
                              "OSR_USE_APPROX_TMERC is set");
            m_eStrategy = Strategy::BEST_ACCURACY;
        }
    }

    if (!options.d->osCoordOperation.empty())
    {
        auto ctx = OSRGetProjTLSContext();
        m_pj = proj_create(ctx, options.d->osCoordOperation);
        if (!m_pj)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot instantiate pipeline %s",
                     options.d->osCoordOperation.c_str());
            return FALSE;
        }
        m_bReversePj = options.d->bReverseCO;
#ifdef DEBUG
        auto info = proj_pj_info(m_pj);
        CPLDebug("OGRCT", "%s %s(user set)", info.definition,
                 m_bReversePj ? "(reversed) " : "");
#endif
    }
    else if (!bWebMercatorToWGS84LongLat && poSRSSource && poSRSTarget)
    {
#ifdef DEBUG_PERF
        struct CPLTimeVal tvStart;
        CPLGettimeofday(&tvStart, nullptr);
        CPLDebug("OGR_CT", "Before proj_create_crs_to_crs()");
#endif
#ifdef DEBUG
        CPLDebug("OGR_CT", "Source CRS: '%s'", pszSrcSRS);
        if (dfSourceCoordinateEpoch > 0)
            CPLDebug("OGR_CT", "Source coordinate epoch: %.3f",
                     dfSourceCoordinateEpoch);
        CPLDebug("OGR_CT", "Target CRS: '%s'", pszTargetSRS);
        if (dfTargetCoordinateEpoch > 0)
            CPLDebug("OGR_CT", "Target coordinate epoch: %.3f",
                     dfTargetCoordinateEpoch);
#endif

        if (m_eStrategy == Strategy::PROJ)
        {
            PJ_AREA *area = nullptr;
            if (options.d->bHasAreaOfInterest)
            {
                area = proj_area_create();
                proj_area_set_bbox(area, options.d->dfWestLongitudeDeg,
                                   options.d->dfSouthLatitudeDeg,
                                   options.d->dfEastLongitudeDeg,
                                   options.d->dfNorthLatitudeDeg);
            }
            auto ctx = OSRGetProjTLSContext();
#if PROJ_VERSION_MAJOR >= 8
            auto srcCRS = proj_create(ctx, pszSrcSRS);
            auto targetCRS = proj_create(ctx, pszTargetSRS);
            if (srcCRS == nullptr || targetCRS == nullptr)
            {
                proj_destroy(srcCRS);
                proj_destroy(targetCRS);
                if (area)
                    proj_area_destroy(area);
                return FALSE;
            }
            CPLStringList aosOptions;
            if (options.d->dfAccuracy >= 0)
                aosOptions.SetNameValue(
                    "ACCURACY", CPLSPrintf("%.18g", options.d->dfAccuracy));
            if (!options.d->bAllowBallpark)
                aosOptions.SetNameValue("ALLOW_BALLPARK", "NO");
#if PROJ_VERSION_MAJOR > 9 ||                                                  \
    (PROJ_VERSION_MAJOR == 9 && PROJ_VERSION_MINOR >= 2)
            if (options.d->bOnlyBestOptionSet)
            {
                aosOptions.SetNameValue("ONLY_BEST",
                                        options.d->bOnlyBest ? "YES" : "NO");
            }
#endif

#if PROJ_VERSION_MAJOR > 9 ||                                                  \
    (PROJ_VERSION_MAJOR == 9 && PROJ_VERSION_MINOR >= 4)
            if (bSourceIsDynamicCRS && dfSourceCoordinateEpoch > 0 &&
                bTargetIsDynamicCRS && dfTargetCoordinateEpoch > 0)
            {
                auto srcCM = proj_coordinate_metadata_create(
                    ctx, srcCRS, dfSourceCoordinateEpoch);
                proj_destroy(srcCRS);
                srcCRS = srcCM;

                auto targetCM = proj_coordinate_metadata_create(
                    ctx, targetCRS, dfTargetCoordinateEpoch);
                proj_destroy(targetCRS);
                targetCRS = targetCM;
            }
#endif

            m_pj = proj_create_crs_to_crs_from_pj(ctx, srcCRS, targetCRS, area,
                                                  aosOptions.List());
            proj_destroy(srcCRS);
            proj_destroy(targetCRS);
#else
            m_pj = proj_create_crs_to_crs(ctx, pszSrcSRS, pszTargetSRS, area);
#endif
            if (area)
                proj_area_destroy(area);
            if (m_pj == nullptr)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Cannot find coordinate operations from `%s' to `%s'",
                         pszSrcSRS, pszTargetSRS);
                return FALSE;
            }
        }
        else if (!ListCoordinateOperations(pszSrcSRS, pszTargetSRS, options))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot find coordinate operations from `%s' to `%s'",
                     pszSrcSRS, pszTargetSRS);
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
    }

    if (options.d->osCoordOperation.empty() && poSRSSource && poSRSTarget &&
        (dfSourceCoordinateEpoch == 0 || dfTargetCoordinateEpoch == 0 ||
         dfSourceCoordinateEpoch == dfTargetCoordinateEpoch))
    {
        // Determine if we can skip the transformation completely.
        const char *const apszOptionsIsSame[] = {"CRITERION=EQUIVALENT",
                                                 nullptr};
        bNoTransform =
            !bSourceWrap && !bTargetWrap &&
            CPL_TO_BOOL(poSRSSource->IsSame(poSRSTarget, apszOptionsIsSame));
    }

    return TRUE;
}

/************************************************************************/
/*                               op_to_pj()                             */
/************************************************************************/

static PJ *op_to_pj(PJ_CONTEXT *ctx, PJ *op,
                    CPLString *osOutProjString = nullptr)
{
    // OSR_USE_ETMERC is here just for legacy
    bool bForceApproxTMerc = false;
    const char *pszUseETMERC = CPLGetConfigOption("OSR_USE_ETMERC", nullptr);
    if (pszUseETMERC && pszUseETMERC[0])
    {
        static bool bHasWarned = false;
        if (!bHasWarned)
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
        const char *pszUseApproxTMERC =
            CPLGetConfigOption("OSR_USE_APPROX_TMERC", nullptr);
        if (pszUseApproxTMERC && pszUseApproxTMERC[0])
        {
            bForceApproxTMerc = CPLTestBool(pszUseApproxTMERC);
        }
    }
    const char *options[] = {
        bForceApproxTMerc ? "USE_APPROX_TMERC=YES" : nullptr, nullptr};
    auto proj_string = proj_as_proj_string(ctx, op, PJ_PROJ_5, options);
    if (!proj_string)
    {
        return nullptr;
    }
    if (osOutProjString)
        *osOutProjString = proj_string;

    if (proj_string[0] == '\0')
    {
        /* Null transform ? */
        return proj_create(ctx, "proj=affine");
    }
    else
    {
        return proj_create(ctx, proj_string);
    }
}

/************************************************************************/
/*                       ListCoordinateOperations()                     */
/************************************************************************/

bool OGRProjCT::ListCoordinateOperations(
    const char *pszSrcSRS, const char *pszTargetSRS,
    const OGRCoordinateTransformationOptions &options)
{
    auto ctx = OSRGetProjTLSContext();

    auto src = proj_create(ctx, pszSrcSRS);
    if (!src)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot instantiate source_crs");
        return false;
    }

    auto dst = proj_create(ctx, pszTargetSRS);
    if (!dst)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot instantiate target_crs");
        proj_destroy(src);
        return false;
    }

    auto operation_ctx = proj_create_operation_factory_context(ctx, nullptr);
    if (!operation_ctx)
    {
        proj_destroy(src);
        proj_destroy(dst);
        return false;
    }

    proj_operation_factory_context_set_spatial_criterion(
        ctx, operation_ctx, PROJ_SPATIAL_CRITERION_PARTIAL_INTERSECTION);
    proj_operation_factory_context_set_grid_availability_use(
        ctx, operation_ctx,
#if PROJ_VERSION_MAJOR >= 7
        proj_context_is_network_enabled(ctx)
            ? PROJ_GRID_AVAILABILITY_KNOWN_AVAILABLE
            :
#endif
            PROJ_GRID_AVAILABILITY_DISCARD_OPERATION_IF_MISSING_GRID);

    if (options.d->bHasAreaOfInterest)
    {
        proj_operation_factory_context_set_area_of_interest(
            ctx, operation_ctx, options.d->dfWestLongitudeDeg,
            options.d->dfSouthLatitudeDeg, options.d->dfEastLongitudeDeg,
            options.d->dfNorthLatitudeDeg);
    }

    if (options.d->dfAccuracy >= 0)
        proj_operation_factory_context_set_desired_accuracy(
            ctx, operation_ctx, options.d->dfAccuracy);
    if (!options.d->bAllowBallpark)
    {
#if PROJ_VERSION_MAJOR > 7 ||                                                  \
    (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 1)
        proj_operation_factory_context_set_allow_ballpark_transformations(
            ctx, operation_ctx, FALSE);
#else
        if (options.d->dfAccuracy < 0)
        {
            proj_operation_factory_context_set_desired_accuracy(
                ctx, operation_ctx, HUGE_VAL);
        }
#endif
    }

    auto op_list = proj_create_operations(ctx, src, dst, operation_ctx);

    if (!op_list)
    {
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        return false;
    }

    auto op_count = proj_list_get_count(op_list);
    if (op_count == 0)
    {
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        CPLDebug("OGRCT", "No operation found matching criteria");
        return false;
    }

    if (op_count == 1 || options.d->bHasAreaOfInterest ||
        proj_get_type(src) == PJ_TYPE_GEOCENTRIC_CRS ||
        proj_get_type(dst) == PJ_TYPE_GEOCENTRIC_CRS)
    {
        auto op = proj_list_get(ctx, op_list, 0);
        CPLAssert(op);
        m_pj = op_to_pj(ctx, op);
        CPLString osName;
        auto name = proj_get_name(op);
        if (name)
            osName = name;
        proj_destroy(op);
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        if (!m_pj)
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
    if (!geodetic_crs)
    {
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        CPLDebug("OGRCT", "Cannot find geodetic CRS matching source CRS");
        return false;
    }
    auto geodetic_crs_type = proj_get_type(geodetic_crs);
    if (geodetic_crs_type == PJ_TYPE_GEOCENTRIC_CRS ||
        geodetic_crs_type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
        geodetic_crs_type == PJ_TYPE_GEOGRAPHIC_3D_CRS)
    {
        auto datum = proj_crs_get_datum(ctx, geodetic_crs);
#if PROJ_VERSION_MAJOR > 7 ||                                                  \
    (PROJ_VERSION_MAJOR == 7 && PROJ_VERSION_MINOR >= 2)
        if (datum == nullptr)
        {
            datum = proj_crs_get_datum_forced(ctx, geodetic_crs);
        }
#endif
        if (datum)
        {
            auto ellps = proj_get_ellipsoid(ctx, datum);
            proj_destroy(datum);
            double semi_major_metre = 0;
            double inv_flattening = 0;
            proj_ellipsoid_get_parameters(ctx, ellps, &semi_major_metre,
                                          nullptr, nullptr, &inv_flattening);
            auto cs = proj_create_ellipsoidal_2D_cs(
                ctx, PJ_ELLPS2D_LONGITUDE_LATITUDE, nullptr, 0);
            // It is critical to set the prime meridian to 0
            auto temp = proj_create_geographic_crs(
                ctx, "unnamed crs", "unnamed datum", proj_get_name(ellps),
                semi_major_metre, inv_flattening, "Reference prime meridian", 0,
                nullptr, 0, cs);
            proj_destroy(ellps);
            proj_destroy(cs);
            proj_destroy(geodetic_crs);
            geodetic_crs = temp;
            geodetic_crs_type = proj_get_type(geodetic_crs);
        }
    }
    if (geodetic_crs_type != PJ_TYPE_GEOGRAPHIC_2D_CRS)
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
    auto op_list_to_geodetic =
        proj_create_operations(ctx, geodetic_crs, src, operation_ctx);
    proj_destroy(geodetic_crs);

    if (op_list_to_geodetic == nullptr ||
        proj_list_get_count(op_list_to_geodetic) == 0)
    {
        CPLDebug(
            "OGRCT",
            "Cannot compute transformation from geographic CRS to source CRS");
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
    if (!pjGeogToSrc)
    {
        proj_list_destroy(op_list);
        proj_operation_factory_context_destroy(operation_ctx);
        proj_destroy(src);
        proj_destroy(dst);
        return false;
    }

    const auto addTransformation =
        [this, &pjGeogToSrc, &ctx](PJ *op, double west_lon, double south_lat,
                                   double east_lon, double north_lat)
    {
        double minx = -std::numeric_limits<double>::max();
        double miny = -std::numeric_limits<double>::max();
        double maxx = std::numeric_limits<double>::max();
        double maxy = std::numeric_limits<double>::max();

        if (!(west_lon == -180.0 && east_lon == 180.0 && south_lat == -90.0 &&
              north_lat == 90.0))
        {
            minx = -minx;
            miny = -miny;
            maxx = -maxx;
            maxy = -maxy;

            double x[21 * 4], y[21 * 4];
            for (int j = 0; j <= 20; j++)
            {
                x[j] = west_lon + j * (east_lon - west_lon) / 20;
                y[j] = south_lat;
                x[21 + j] = west_lon + j * (east_lon - west_lon) / 20;
                y[21 + j] = north_lat;
                x[21 * 2 + j] = west_lon;
                y[21 * 2 + j] = south_lat + j * (north_lat - south_lat) / 20;
                x[21 * 3 + j] = east_lon;
                y[21 * 3 + j] = south_lat + j * (north_lat - south_lat) / 20;
            }
            proj_trans_generic(pjGeogToSrc, PJ_FWD, x, sizeof(double), 21 * 4,
                               y, sizeof(double), 21 * 4, nullptr, 0, 0,
                               nullptr, 0, 0);
            for (int j = 0; j < 21 * 4; j++)
            {
                if (x[j] != HUGE_VAL && y[j] != HUGE_VAL)
                {
                    minx = std::min(minx, x[j]);
                    miny = std::min(miny, y[j]);
                    maxx = std::max(maxx, x[j]);
                    maxy = std::max(maxy, y[j]);
                }
            }
        }

        if (minx <= maxx)
        {
            CPLString osProjString;
            const double accuracy = proj_coordoperation_get_accuracy(ctx, op);
            auto pj = op_to_pj(ctx, op, &osProjString);
            CPLString osName;
            auto name = proj_get_name(op);
            if (name)
                osName = name;
            proj_destroy(op);
            op = nullptr;
            if (pj)
            {
                m_oTransformations.emplace_back(minx, miny, maxx, maxy, pj,
                                                osName, osProjString, accuracy);
            }
        }
        return op;
    };

    // Iterate over source->target candidate transformations and reproject
    // their long-lat bounding box into the source CRS.
    bool foundWorldTransformation = false;
    for (int i = 0; i < op_count; i++)
    {
        auto op = proj_list_get(ctx, op_list, i);
        CPLAssert(op);
        double west_lon = 0.0;
        double south_lat = 0.0;
        double east_lon = 0.0;
        double north_lat = 0.0;
        if (proj_get_area_of_use(ctx, op, &west_lon, &south_lat, &east_lon,
                                 &north_lat, nullptr))
        {
            if (west_lon <= east_lon)
            {
                if (west_lon == -180 && east_lon == 180 && south_lat == -90 &&
                    north_lat == 90)
                {
                    foundWorldTransformation = true;
                }
                op = addTransformation(op, west_lon, south_lat, east_lon,
                                       north_lat);
            }
            else
            {
                auto op_clone = proj_clone(ctx, op);

                op = addTransformation(op, west_lon, south_lat, 180, north_lat);
                op_clone = addTransformation(op_clone, -180, south_lat,
                                             east_lon, north_lat);
                proj_destroy(op_clone);
            }
        }

        proj_destroy(op);
    }

    proj_list_destroy(op_list);

    // Sometimes the user will operate even outside the area of use of the
    // source and target CRS, so if no global transformation has been returned
    // previously, trigger the computation of one.
    if (!foundWorldTransformation)
    {
        proj_operation_factory_context_set_area_of_interest(ctx, operation_ctx,
                                                            -180, -90, 180, 90);
        proj_operation_factory_context_set_spatial_criterion(
            ctx, operation_ctx, PROJ_SPATIAL_CRITERION_STRICT_CONTAINMENT);
        op_list = proj_create_operations(ctx, src, dst, operation_ctx);
        if (op_list)
        {
            op_count = proj_list_get_count(op_list);
            for (int i = 0; i < op_count; i++)
            {
                auto op = proj_list_get(ctx, op_list, i);
                CPLAssert(op);
                double west_lon = 0.0;
                double south_lat = 0.0;
                double east_lon = 0.0;
                double north_lat = 0.0;
                if (proj_get_area_of_use(ctx, op, &west_lon, &south_lat,
                                         &east_lon, &north_lat, nullptr) &&
                    west_lon == -180 && east_lon == 180 && south_lat == -90 &&
                    north_lat == 90)
                {
                    op = addTransformation(op, west_lon, south_lat, east_lon,
                                           north_lat);
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

const OGRSpatialReference *OGRProjCT::GetSourceCS() const

{
    return poSRSSource;
}

/************************************************************************/
/*                            GetTargetCS()                             */
/************************************************************************/

const OGRSpatialReference *OGRProjCT::GetTargetCS() const

{
    return poSRSTarget;
}

/************************************************************************/
/*                             Transform()                              */
/************************************************************************/

int OGRCoordinateTransformation::Transform(size_t nCount, double *x, double *y,
                                           double *z, int *pabSuccessIn)

{
    int *pabSuccess =
        pabSuccessIn
            ? pabSuccessIn
            : static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nCount));
    if (!pabSuccess)
        return FALSE;

    bool bOverallSuccess =
        CPL_TO_BOOL(Transform(nCount, x, y, z, nullptr, pabSuccess));

    for (size_t i = 0; i < nCount; i++)
    {
        if (!pabSuccess[i])
        {
            bOverallSuccess = false;
            break;
        }
    }

    if (pabSuccess != pabSuccessIn)
        CPLFree(pabSuccess);

    return bOverallSuccess;
}

/************************************************************************/
/*                      TransformWithErrorCodes()                       */
/************************************************************************/

int OGRCoordinateTransformation::TransformWithErrorCodes(size_t nCount,
                                                         double *x, double *y,
                                                         double *z, double *t,
                                                         int *panErrorCodes)

{
    std::vector<int> abSuccess;
    try
    {
        abSuccess.resize(nCount);
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate abSuccess[] temporary array");
        return FALSE;
    }

    const bool bOverallSuccess =
        CPL_TO_BOOL(Transform(nCount, x, y, z, t, abSuccess.data()));

    if (panErrorCodes)
    {
        for (size_t i = 0; i < nCount; i++)
        {
            panErrorCodes[i] = abSuccess[i] ? 0 : -1;
        }
    }

    return bOverallSuccess;
}

/************************************************************************/
/*                             Transform()                             */
/************************************************************************/

int OGRProjCT::Transform(size_t nCount, double *x, double *y, double *z,
                         double *t, int *pabSuccess)

{
    bool bOverallSuccess =
        CPL_TO_BOOL(TransformWithErrorCodes(nCount, x, y, z, t, pabSuccess));

    if (pabSuccess)
    {
        for (size_t i = 0; i < nCount; i++)
        {
            pabSuccess[i] = (pabSuccess[i] == 0);
        }
    }

    return bOverallSuccess;
}

/************************************************************************/
/*                       TransformWithErrorCodes()                      */
/************************************************************************/

#ifndef PROJ_ERR_COORD_TRANSFM_INVALID_COORD
#define PROJ_ERR_COORD_TRANSFM_INVALID_COORD 2049
#define PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN 2050
#define PROJ_ERR_COORD_TRANSFM_NO_OPERATION 2051
#endif

int OGRProjCT::TransformWithErrorCodes(size_t nCount, double *x, double *y,
                                       double *z, double *t, int *panErrorCodes)

{
    if (nCount == 0)
        return TRUE;

    // Prevent any coordinate modification when possible
    if (bNoTransform)
    {
        if (panErrorCodes)
        {
            for (size_t i = 0; i < nCount; i++)
            {
                panErrorCodes[i] = 0;
            }
        }
        return TRUE;
    }

#ifdef DEBUG_VERBOSE
    bool bDebugCT = CPLTestBool(CPLGetConfigOption("OGR_CT_DEBUG", "NO"));
    if (bDebugCT)
    {
        CPLDebug("OGRCT", "count = %d", nCount);
        for (size_t i = 0; i < nCount; ++i)
        {
            CPLDebug("OGRCT", "  x[%d] = %.16g y[%d] = %.16g", int(i), x[i],
                     int(i), y[i]);
        }
    }
#endif
#ifdef DEBUG_PERF
    // CPLDebug("OGR_CT", "Begin TransformWithErrorCodes()");
    struct CPLTimeVal tvStart;
    CPLGettimeofday(&tvStart, nullptr);
#endif

    /* -------------------------------------------------------------------- */
    /*      Apply data axis to source CRS mapping.                          */
    /* -------------------------------------------------------------------- */

    // Since we may swap the x and y pointers, but cannot tell the caller about this swap,
    // we save the original pointer. The same axis swap code is executed for poSRSTarget.
    // If this nullifies, we save the swap of both axes
    const auto xOriginal = x;

    if (poSRSSource)
    {
        const auto &mapping = poSRSSource->GetDataAxisToSRSAxisMapping();
        if (mapping.size() >= 2)
        {
            if (std::abs(mapping[0]) == 2 && std::abs(mapping[1]) == 1)
            {
                std::swap(x, y);
            }
            const bool bNegateX = mapping[0] < 0;
            if (bNegateX)
            {
                for (size_t i = 0; i < nCount; i++)
                {
                    x[i] = -x[i];
                }
            }
            const bool bNegateY = mapping[1] < 0;
            if (bNegateY)
            {
                for (size_t i = 0; i < nCount; i++)
                {
                    y[i] = -y[i];
                }
            }
            if (z && mapping.size() >= 3 && mapping[2] == -3)
            {
                for (size_t i = 0; i < nCount; i++)
                {
                    z[i] = -z[i];
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Potentially do longitude wrapping.                              */
    /* -------------------------------------------------------------------- */
    if (bSourceLatLong && bSourceWrap)
    {
        if (m_eSourceFirstAxisOrient == OAO_East)
        {
            for (size_t i = 0; i < nCount; i++)
            {
                if (x[i] != HUGE_VAL && y[i] != HUGE_VAL)
                {
                    if (x[i] < dfSourceWrapLong - 180.0)
                        x[i] += 360.0;
                    else if (x[i] > dfSourceWrapLong + 180)
                        x[i] -= 360.0;
                }
            }
        }
        else
        {
            for (size_t i = 0; i < nCount; i++)
            {
                if (x[i] != HUGE_VAL && y[i] != HUGE_VAL)
                {
                    if (y[i] < dfSourceWrapLong - 180.0)
                        y[i] += 360.0;
                    else if (y[i] > dfSourceWrapLong + 180)
                        y[i] -= 360.0;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Optimized transform from WebMercator to WGS84                   */
    /* -------------------------------------------------------------------- */
    bool bTransformDone = false;
    if (bWebMercatorToWGS84LongLat)
    {
        constexpr double REVERSE_SPHERE_RADIUS = 1.0 / 6378137.0;

        if (m_eSourceFirstAxisOrient != OAO_East)
        {
            std::swap(x, y);
        }

        double y0 = y[0];
        for (size_t i = 0; i < nCount; i++)
        {
            if (x[i] != HUGE_VAL)
            {
                x[i] = x[i] * REVERSE_SPHERE_RADIUS;
                if (x[i] > M_PI)
                {
                    if (x[i] < M_PI + 1e-14)
                    {
                        x[i] = M_PI;
                    }
                    else if (m_options.d->bCheckWithInvertProj)
                    {
                        x[i] = HUGE_VAL;
                        y[i] = HUGE_VAL;
                        y0 = HUGE_VAL;
                        continue;
                    }
                    else
                    {
                        do
                        {
                            x[i] -= 2 * M_PI;
                        } while (x[i] > M_PI);
                    }
                }
                else if (x[i] < -M_PI)
                {
                    if (x[i] > -M_PI - 1e-14)
                    {
                        x[i] = -M_PI;
                    }
                    else if (m_options.d->bCheckWithInvertProj)
                    {
                        x[i] = HUGE_VAL;
                        y[i] = HUGE_VAL;
                        y0 = HUGE_VAL;
                        continue;
                    }
                    else
                    {
                        do
                        {
                            x[i] += 2 * M_PI;
                        } while (x[i] < -M_PI);
                    }
                }
                constexpr double RAD_TO_DEG = 180. / M_PI;
                x[i] *= RAD_TO_DEG;

                // Optimization for the case where we are provided a whole line
                // of same northing.
                if (i > 0 && y[i] == y0)
                    y[i] = y[0];
                else
                {
                    y[i] = M_PI / 2.0 -
                           2.0 * atan(exp(-y[i] * REVERSE_SPHERE_RADIUS));
                    y[i] *= RAD_TO_DEG;
                }
            }
        }

        if (panErrorCodes)
        {
            for (size_t i = 0; i < nCount; i++)
            {
                if (x[i] != HUGE_VAL)
                    panErrorCodes[i] = 0;
                else
                    panErrorCodes[i] =
                        PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN;
            }
        }

        if (m_eTargetFirstAxisOrient != OAO_East)
        {
            std::swap(x, y);
        }

        bTransformDone = true;
    }

    // Determine the default coordinate epoch, if not provided in the point to
    // transform.
    // For time-dependent transformations, PROJ can currently only do
    // staticCRS -> dynamicCRS or dynamicCRS -> staticCRS transformations, and
    // in either case, the coordinate epoch of the dynamicCRS must be provided
    // as the input time.
    double dfDefaultTime = HUGE_VAL;
    if (bSourceIsDynamicCRS && dfSourceCoordinateEpoch > 0 &&
        !bTargetIsDynamicCRS &&
        CPLTestBool(
            CPLGetConfigOption("OGR_CT_USE_SRS_COORDINATE_EPOCH", "YES")))
    {
        dfDefaultTime = dfSourceCoordinateEpoch;
        CPLDebug("OGR_CT", "Using coordinate epoch %f from source CRS",
                 dfDefaultTime);
    }
    else if (bTargetIsDynamicCRS && dfTargetCoordinateEpoch > 0 &&
             !bSourceIsDynamicCRS &&
             CPLTestBool(
                 CPLGetConfigOption("OGR_CT_USE_SRS_COORDINATE_EPOCH", "YES")))
    {
        dfDefaultTime = dfTargetCoordinateEpoch;
        CPLDebug("OGR_CT", "Using coordinate epoch %f from target CRS",
                 dfDefaultTime);
    }

    /* -------------------------------------------------------------------- */
    /*      Select dynamically the best transformation for the data, if     */
    /*      needed.                                                         */
    /* -------------------------------------------------------------------- */
    auto ctx = OSRGetProjTLSContext();
    PJ *pj = m_pj;
    if (!bTransformDone && !pj)
    {
        double avgX = 0.0;
        double avgY = 0.0;
        size_t nCountValid = 0;
        for (size_t i = 0; i < nCount; i++)
        {
            if (x[i] != HUGE_VAL && y[i] != HUGE_VAL)
            {
                avgX += x[i];
                avgY += y[i];
                nCountValid++;
            }
        }
        if (nCountValid != 0)
        {
            avgX /= static_cast<double>(nCountValid);
            avgY /= static_cast<double>(nCountValid);
        }

        constexpr int N_MAX_RETRY = 2;
        int iExcluded[N_MAX_RETRY] = {-1, -1};

        const int nOperations = static_cast<int>(m_oTransformations.size());
        PJ_COORD coord;
        coord.xyzt.x = avgX;
        coord.xyzt.y = avgY;
        coord.xyzt.z = z ? z[0] : 0;
        coord.xyzt.t = t ? t[0] : dfDefaultTime;

        // We may need several attempts. For example the point at
        // lon=-111.5 lat=45.26 falls into the bounding box of the Canadian
        // ntv2_0.gsb grid, except that it is not in any of the subgrids, being
        // in the US. We thus need another retry that will select the conus
        // grid.
        for (int iRetry = 0; iRetry <= N_MAX_RETRY; iRetry++)
        {
            int iBestTransf = -1;
            // Select transform whose BBOX match our data and has the best
            // accuracy if m_eStrategy == BEST_ACCURACY. Or just the first BBOX
            // matching one, if
            //  m_eStrategy == FIRST_MATCHING
            double dfBestAccuracy = std::numeric_limits<double>::infinity();
            for (int i = 0; i < nOperations; i++)
            {
                if (i == iExcluded[0] || i == iExcluded[1])
                {
                    continue;
                }
                const auto &transf = m_oTransformations[i];
                if (avgX >= transf.minx && avgX <= transf.maxx &&
                    avgY >= transf.miny && avgY <= transf.maxy &&
                    (iBestTransf < 0 || (transf.accuracy >= 0 &&
                                         transf.accuracy < dfBestAccuracy)))
                {
                    iBestTransf = i;
                    dfBestAccuracy = transf.accuracy;
                    if (m_eStrategy == Strategy::FIRST_MATCHING)
                        break;
                }
            }
            if (iBestTransf < 0)
            {
                break;
            }
            auto &transf = m_oTransformations[iBestTransf];
            pj = transf.pj;
            proj_assign_context(pj, ctx);
            if (iBestTransf != m_iCurTransformation)
            {
                CPLDebug("OGRCT", "Selecting transformation %s (%s)",
                         transf.osProjString.c_str(), transf.osName.c_str());
                m_iCurTransformation = iBestTransf;
            }

            auto res = proj_trans(pj, m_bReversePj ? PJ_INV : PJ_FWD, coord);
            if (res.xyzt.x != HUGE_VAL)
            {
                break;
            }
            pj = nullptr;
            CPLDebug("OGRCT", "Did not result in valid result. "
                              "Attempting a retry with another operation.");
            if (iRetry == N_MAX_RETRY)
            {
                break;
            }
            iExcluded[iRetry] = iBestTransf;
        }

        if (!pj)
        {
            // In case we did not find an operation whose area of use is
            // compatible with the input coordinate, then goes through again the
            // list, and use the first operation that does not require grids.
            for (int i = 0; i < nOperations; i++)
            {
                auto &transf = m_oTransformations[i];
                if (proj_coordoperation_get_grid_used_count(ctx, transf.pj) ==
                    0)
                {
                    pj = transf.pj;
                    proj_assign_context(pj, ctx);
                    if (i != m_iCurTransformation)
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

        if (!pj)
        {
            if (m_bEmitErrors && ++nErrorCount < 20)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find transformation for provided coordinates");
            }
            else if (nErrorCount == 20)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Reprojection failed, further errors will be "
                         "suppressed on the transform object.");
            }

            for (size_t i = 0; i < nCount; i++)
            {
                x[i] = HUGE_VAL;
                y[i] = HUGE_VAL;
                if (panErrorCodes)
                    panErrorCodes[i] = PROJ_ERR_COORD_TRANSFM_NO_OPERATION;
            }
            return FALSE;
        }
    }
    if (pj)
    {
        proj_assign_context(pj, ctx);
    }

    /* -------------------------------------------------------------------- */
    /*      Do the transformation (or not...) using PROJ                    */
    /* -------------------------------------------------------------------- */

    if (!bTransformDone)
    {
        const auto nLastErrorCounter = CPLGetErrorCounter();

        for (size_t i = 0; i < nCount; i++)
        {
            PJ_COORD coord;
            const double xIn = x[i];
            const double yIn = y[i];
            if (!std::isfinite(xIn))
            {
                x[i] = HUGE_VAL;
                y[i] = HUGE_VAL;
                if (panErrorCodes)
                    panErrorCodes[i] = PROJ_ERR_COORD_TRANSFM_INVALID_COORD;
                continue;
            }
            coord.xyzt.x = x[i];
            coord.xyzt.y = y[i];
            coord.xyzt.z = z ? z[i] : 0;
            coord.xyzt.t = t ? t[i] : dfDefaultTime;
            proj_errno_reset(pj);
            coord = proj_trans(pj, m_bReversePj ? PJ_INV : PJ_FWD, coord);
            x[i] = coord.xyzt.x;
            y[i] = coord.xyzt.y;
            if (z)
                z[i] = coord.xyzt.z;
            if (t)
                t[i] = coord.xyzt.t;
            int err = 0;
            if (std::isnan(coord.xyzt.x))
            {
                // This shouldn't normally happen if PROJ projections behave
                // correctly, but e.g inverse laea before PROJ 8.1.1 could
                // do that for points out of domain.
                // See https://github.com/OSGeo/PROJ/pull/2800
                x[i] = HUGE_VAL;
                y[i] = HUGE_VAL;
                err = PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN;
                static bool bHasWarned = false;
                if (!bHasWarned)
                {
#ifdef DEBUG
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "PROJ returned a NaN value. It should be fixed");
#else
                    CPLDebug("OGR_CT",
                             "PROJ returned a NaN value. It should be fixed");
#endif
                    bHasWarned = true;
                }
            }
            else if (coord.xyzt.x == HUGE_VAL)
            {
                err = proj_errno(pj);
                // PROJ should normally emit an error, but in case it does not
                // (e.g PROJ 6.3 with the +ortho projection), synthetize one
                if (err == 0)
                    err = PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN;
            }
            else if (m_options.d->bCheckWithInvertProj)
            {
                // For some projections, we cannot detect if we are trying to
                // reproject coordinates outside the validity area of the
                // projection. So let's do the reverse reprojection and compare
                // with the source coordinates.
                coord = proj_trans(pj, m_bReversePj ? PJ_FWD : PJ_INV, coord);
                if (fabs(coord.xyzt.x - xIn) > dfThreshold ||
                    fabs(coord.xyzt.y - yIn) > dfThreshold)
                {
                    err = PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN;
                    x[i] = HUGE_VAL;
                    y[i] = HUGE_VAL;
                }
            }

            if (panErrorCodes)
                panErrorCodes[i] = err;

            /* --------------------------------------------------------------------
             */
            /*      Try to report an error through CPL.  Get proj error string
             */
            /*      if possible.  Try to avoid reporting thousands of errors. */
            /*      Suppress further error reporting on this OGRProjCT if we */
            /*      have already reported 20 errors. */
            /* --------------------------------------------------------------------
             */
            if (err != 0)
            {
                if (++nErrorCount < 20)
                {
#if PROJ_VERSION_MAJOR >= 8
                    const char *pszError = proj_context_errno_string(ctx, err);
#else
                    const char *pszError = proj_errno_string(err);
#endif
                    if (m_bEmitErrors
#ifdef PROJ_ERR_OTHER_NO_INVERSE_OP
                        || (i == 0 && err == PROJ_ERR_OTHER_NO_INVERSE_OP)
#endif
                    )
                    {
                        if (nLastErrorCounter != CPLGetErrorCounter() &&
                            CPLGetLastErrorType() == CE_Failure &&
                            strstr(CPLGetLastErrorMsg(), "PROJ:"))
                        {
                            // do nothing
                        }
                        else if (pszError == nullptr)
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Reprojection failed, err = %d", err);
                        else
                            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                                     pszError);
                    }
                    else
                    {
                        if (pszError == nullptr)
                            CPLDebug("OGRCT", "Reprojection failed, err = %d",
                                     err);
                        else
                            CPLDebug("OGRCT", "%s", pszError);
                    }
                }
                else if (nErrorCount == 20)
                {
                    if (m_bEmitErrors)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Reprojection failed, err = %d, further "
                                 "errors will be "
                                 "suppressed on the transform object.",
                                 err);
                    }
                    else
                    {
                        CPLDebug("OGRCT",
                                 "Reprojection failed, err = %d, further "
                                 "errors will be "
                                 "suppressed on the transform object.",
                                 err);
                    }
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Potentially do longitude wrapping.                              */
    /* -------------------------------------------------------------------- */
    if (bTargetLatLong && bTargetWrap)
    {
        if (m_eTargetFirstAxisOrient == OAO_East)
        {
            for (size_t i = 0; i < nCount; i++)
            {
                if (x[i] != HUGE_VAL && y[i] != HUGE_VAL)
                {
                    if (x[i] < dfTargetWrapLong - 180.0)
                        x[i] += 360.0;
                    else if (x[i] > dfTargetWrapLong + 180)
                        x[i] -= 360.0;
                }
            }
        }
        else
        {
            for (size_t i = 0; i < nCount; i++)
            {
                if (x[i] != HUGE_VAL && y[i] != HUGE_VAL)
                {
                    if (y[i] < dfTargetWrapLong - 180.0)
                        y[i] += 360.0;
                    else if (y[i] > dfTargetWrapLong + 180)
                        y[i] -= 360.0;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Apply data axis to target CRS mapping.                          */
    /* -------------------------------------------------------------------- */
    if (poSRSTarget)
    {
        const auto &mapping = poSRSTarget->GetDataAxisToSRSAxisMapping();
        if (mapping.size() >= 2)
        {
            const bool bNegateX = mapping[0] < 0;
            if (bNegateX)
            {
                for (size_t i = 0; i < nCount; i++)
                {
                    x[i] = -x[i];
                }
            }
            const bool bNegateY = mapping[1] < 0;
            if (bNegateY)
            {
                for (size_t i = 0; i < nCount; i++)
                {
                    y[i] = -y[i];
                }
            }
            if (z && mapping.size() >= 3 && mapping[2] == -3)
            {
                for (size_t i = 0; i < nCount; i++)
                {
                    z[i] = -z[i];
                }
            }

            if (std::abs(mapping[0]) == 2 && std::abs(mapping[1]) == 1)
            {
                std::swap(x, y);
            }
        }
    }

    // Check whether final "genuine" axis swap is really necessary
    if (x != xOriginal)
    {
        std::swap_ranges(x, x + nCount, y);
    }

#ifdef DEBUG_VERBOSE
    if (bDebugCT)
    {
        CPLDebug("OGRCT", "Out:");
        for (size_t i = 0; i < nCount; ++i)
        {
            CPLDebug("OGRCT", "  x[%d] = %.16g y[%d] = %.16g", int(i), x[i],
                     int(i), y[i]);
        }
    }
#endif
#ifdef DEBUG_PERF
    struct CPLTimeVal tvEnd;
    CPLGettimeofday(&tvEnd, nullptr);
    const double delay = (tvEnd.tv_sec + tvEnd.tv_usec * 1e-6) -
                         (tvStart.tv_sec + tvStart.tv_usec * 1e-6);
    g_dfTotalTimeReprojection += delay;
    // CPLDebug("OGR_CT", "End TransformWithErrorCodes(): %d ms",
    //          static_cast<int>(delay * 1000));
#endif

    return TRUE;
}

/************************************************************************/
/*                      TransformBounds()                       */
/************************************************************************/

// ---------------------------------------------------------------------------
static double simple_min(const double *data, const int arr_len)
{
    double min_value = data[0];
    for (int iii = 1; iii < arr_len; iii++)
    {
        if (data[iii] < min_value)
            min_value = data[iii];
    }
    return min_value;
}

// ---------------------------------------------------------------------------
static double simple_max(const double *data, const int arr_len)
{
    double max_value = data[0];
    for (int iii = 1; iii < arr_len; iii++)
    {
        if ((data[iii] > max_value || max_value == HUGE_VAL) &&
            data[iii] != HUGE_VAL)
            max_value = data[iii];
    }
    return max_value;
}

// ---------------------------------------------------------------------------
static int _find_previous_index(const int iii, const double *data,
                                const int arr_len)
{
    // find index of nearest valid previous value if exists
    int prev_iii = iii - 1;
    if (prev_iii == -1)  // handle wraparound
        prev_iii = arr_len - 1;
    while (data[prev_iii] == HUGE_VAL && prev_iii != iii)
    {
        prev_iii--;
        if (prev_iii == -1)  // handle wraparound
            prev_iii = arr_len - 1;
    }
    return prev_iii;
}

// ---------------------------------------------------------------------------
/******************************************************************************
Handles the case when longitude values cross the antimeridian
when calculating the minimum.
Note: The data array must be in a linear ring.
Note: This requires a densified ring with at least 2 additional
        points per edge to correctly handle global extents.
If only 1 additional point:
    |        |
    |RL--x0--|RL--
    |        |
-180    180|-180
If they are evenly spaced and it crosses the antimeridian:
x0 - L = 180
R - x0 = -180
For example:
Let R = -179.9, x0 = 0.1, L = -179.89
x0 - L = 0.1 - -179.9 = 180
R - x0 = -179.89 - 0.1 ~= -180
This is the same in the case when it didn't cross the antimeridian.
If you have 2 additional points:
    |            |
    |RL--x0--x1--|RL--
    |            |
-180        180|-180
If they are evenly spaced and it crosses the antimeridian:
x0 - L = 120
x1 - x0 = 120
R - x1 = -240
For example:
Let R = -179.9, x0 = -59.9, x1 = 60.1 L = -179.89
x0 - L = 59.9 - -179.9 = 120
x1 - x0 = 60.1 - 59.9 = 120
R - x1 = -179.89 - 60.1 ~= -240
However, if they are evenly spaced and it didn't cross the antimeridian:
x0 - L = 120
x1 - x0 = 120
R - x1 = 120
From this, we have a delta that is guaranteed to be significantly
large enough to tell the difference reguarless of the direction
the antimeridian was crossed.
However, even though the spacing was even in the source projection, it isn't
guaranteed in the target geographic projection. So, instead of 240, 200 is used
as it significantly larger than 120 to be sure that the antimeridian was crossed
but smalller than 240 to account for possible irregularities in distances
when re-projecting. Also, 200 ensures latitudes are ignored for axis order
handling.
******************************************************************************/
static double antimeridian_min(const double *data, const int arr_len)
{
    double positive_min = HUGE_VAL;
    double min_value = HUGE_VAL;
    int crossed_meridian_count = 0;
    bool positive_meridian = false;

    for (int iii = 0; iii < arr_len; iii++)
    {
        if (data[iii] == HUGE_VAL)
            continue;
        int prev_iii = _find_previous_index(iii, data, arr_len);
        // check if crossed meridian
        double delta = data[prev_iii] - data[iii];
        // 180 -> -180
        if (delta >= 200 && delta != HUGE_VAL)
        {
            if (crossed_meridian_count == 0)
                positive_min = min_value;
            crossed_meridian_count++;
            positive_meridian = false;
            // -180 -> 180
        }
        else if (delta <= -200 && delta != HUGE_VAL)
        {
            if (crossed_meridian_count == 0)
                positive_min = data[iii];
            crossed_meridian_count++;
            positive_meridian = true;
        }
        // positive meridian side min
        if (positive_meridian && data[iii] < positive_min)
            positive_min = data[iii];
        // track general min value
        if (data[iii] < min_value)
            min_value = data[iii];
    }

    if (crossed_meridian_count == 2)
        return positive_min;
    else if (crossed_meridian_count == 4)
        // bounds extends beyond -180/180
        return -180;
    return min_value;
}

// ---------------------------------------------------------------------------
// Handles the case when longitude values cross the antimeridian
// when calculating the minimum.
// Note: The data array must be in a linear ring.
// Note: This requires a densified ring with at least 2 additional
//       points per edge to correctly handle global extents.
// See antimeridian_min docstring for reasoning.
static double antimeridian_max(const double *data, const int arr_len)
{
    double negative_max = -HUGE_VAL;
    double max_value = -HUGE_VAL;
    bool negative_meridian = false;
    int crossed_meridian_count = 0;

    for (int iii = 0; iii < arr_len; iii++)
    {
        if (data[iii] == HUGE_VAL)
            continue;
        int prev_iii = _find_previous_index(iii, data, arr_len);
        // check if crossed meridian
        double delta = data[prev_iii] - data[iii];
        // 180 -> -180
        if (delta >= 200 && delta != HUGE_VAL)
        {
            if (crossed_meridian_count == 0)
                negative_max = data[iii];
            crossed_meridian_count++;
            negative_meridian = true;
            // -180 -> 180
        }
        else if (delta <= -200 && delta != HUGE_VAL)
        {
            if (crossed_meridian_count == 0)
                negative_max = max_value;
            negative_meridian = false;
            crossed_meridian_count++;
        }
        // negative meridian side max
        if (negative_meridian &&
            (data[iii] > negative_max || negative_max == HUGE_VAL) &&
            data[iii] != HUGE_VAL)
            negative_max = data[iii];
        // track general max value
        if ((data[iii] > max_value || max_value == HUGE_VAL) &&
            data[iii] != HUGE_VAL)
            max_value = data[iii];
    }
    if (crossed_meridian_count == 2)
        return negative_max;
    else if (crossed_meridian_count == 4)
        // bounds extends beyond -180/180
        return 180;
    return max_value;
}

// ---------------------------------------------------------------------------
// Check if the original projected bounds contains
// the north pole.
// This assumes that the destination CRS is geographic.
bool OGRProjCT::ContainsNorthPole(const double xmin, const double ymin,
                                  const double xmax, const double ymax,
                                  bool lon_lat_order)
{
    double pole_y = 90;
    double pole_x = 0;
    if (!lon_lat_order)
    {
        pole_y = 0;
        pole_x = 90;
    }
    auto inverseCT = GetInverse();
    if (!inverseCT)
        return false;
    bool success = inverseCT->TransformWithErrorCodes(
        1, &pole_x, &pole_y, nullptr, nullptr, nullptr);
    if (success && CPLGetLastErrorType() != CE_None)
        CPLErrorReset();
    delete inverseCT;
    if (xmin < pole_x && pole_x < xmax && ymax > pole_y && pole_y > ymin)
        return true;
    return false;
}

// ---------------------------------------------------------------------------
// Check if the original projected bounds contains
// the south pole.
// This assumes that the destination CRS is geographic.
bool OGRProjCT::ContainsSouthPole(const double xmin, const double ymin,
                                  const double xmax, const double ymax,
                                  bool lon_lat_order)
{
    double pole_y = -90;
    double pole_x = 0;
    if (!lon_lat_order)
    {
        pole_y = 0;
        pole_x = -90;
    }
    auto inverseCT = GetInverse();
    if (!inverseCT)
        return false;
    bool success = inverseCT->TransformWithErrorCodes(
        1, &pole_x, &pole_y, nullptr, nullptr, nullptr);
    if (success && CPLGetLastErrorType() != CE_None)
        CPLErrorReset();
    delete inverseCT;
    if (xmin < pole_x && pole_x < xmax && ymax > pole_y && pole_y > ymin)
        return true;
    return false;
}

int OGRProjCT::TransformBounds(const double xmin, const double ymin,
                               const double xmax, const double ymax,
                               double *out_xmin, double *out_ymin,
                               double *out_xmax, double *out_ymax,
                               const int densify_pts)
{
    CPLErrorReset();

    if (bNoTransform)
    {
        *out_xmin = xmin;
        *out_ymin = ymin;
        *out_xmax = xmax;
        *out_ymax = ymax;
        return true;
    }

    *out_xmin = HUGE_VAL;
    *out_ymin = HUGE_VAL;
    *out_xmax = HUGE_VAL;
    *out_ymax = HUGE_VAL;

    if (densify_pts < 0 || densify_pts > 10000)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "densify_pts must be between 0-10000.");
        return false;
    }
    if (!poSRSSource)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "missing source SRS.");
        return false;
    }
    if (!poSRSTarget)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "missing target SRS.");
        return false;
    }

    bool degree_input = false;
    bool degree_output = false;
    bool input_lon_lat_order = false;
    bool output_lon_lat_order = false;

    if (bSourceLatLong)
    {
        degree_input = fabs(poSRSSource->GetAngularUnits(nullptr) -
                            CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
        const auto &mapping = poSRSSource->GetDataAxisToSRSAxisMapping();
        if ((mapping[0] == 1 && m_eSourceFirstAxisOrient == OAO_East) ||
            (mapping[0] == 2 && m_eSourceFirstAxisOrient != OAO_East))
        {
            input_lon_lat_order = true;
        }
    }
    if (bTargetLatLong)
    {
        degree_output = fabs(poSRSTarget->GetAngularUnits(nullptr) -
                             CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
        const auto &mapping = poSRSTarget->GetDataAxisToSRSAxisMapping();
        if ((mapping[0] == 1 && m_eTargetFirstAxisOrient == OAO_East) ||
            (mapping[0] == 2 && m_eTargetFirstAxisOrient != OAO_East))
        {
            output_lon_lat_order = true;
        }
    }

    if (degree_output && densify_pts < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "densify_pts must be at least 2 if the output is geograpic.");
        return false;
    }

    int side_pts = densify_pts + 1;  // add one because we are densifying
    const int boundary_len = side_pts * 4;
    std::vector<double> x_boundary_array;
    std::vector<double> y_boundary_array;
    try
    {
        x_boundary_array.resize(boundary_len);
        y_boundary_array.resize(boundary_len);
    }
    catch (const std::exception &e)  // memory allocation failure
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return false;
    }
    double delta_x = 0;
    double delta_y = 0;
    bool north_pole_in_bounds = false;
    bool south_pole_in_bounds = false;
    if (degree_output)
    {
        CPLErrorHandlerPusher oErrorHandlerPusher(CPLQuietErrorHandler);

        north_pole_in_bounds =
            ContainsNorthPole(xmin, ymin, xmax, ymax, output_lon_lat_order);
        if (CPLGetLastErrorType() != CE_None)
        {
            return false;
        }
        south_pole_in_bounds =
            ContainsSouthPole(xmin, ymin, xmax, ymax, output_lon_lat_order);
        if (CPLGetLastErrorType() != CE_None)
        {
            return false;
        }
    }

    if (degree_input && xmax < xmin)
    {
        if (!input_lon_lat_order)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "latitude max < latitude min.");
            return false;
        }
        // handle antimeridian
        delta_x = (xmax - xmin + 360.0) / side_pts;
    }
    else
    {
        delta_x = (xmax - xmin) / side_pts;
    }
    if (degree_input && ymax < ymin)
    {
        if (input_lon_lat_order)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "latitude max < latitude min.");
            return false;
        }
        // handle antimeridian
        delta_y = (ymax - ymin + 360.0) / side_pts;
    }
    else
    {
        delta_y = (ymax - ymin) / side_pts;
    }

    // build densified bounding box
    // Note: must be a linear ring for antimeridian logic
    for (int iii = 0; iii < side_pts; iii++)
    {
        // xmin boundary
        y_boundary_array[iii] = ymax - iii * delta_y;
        x_boundary_array[iii] = xmin;
        // ymin boundary
        y_boundary_array[iii + side_pts] = ymin;
        x_boundary_array[iii + side_pts] = xmin + iii * delta_x;
        // xmax boundary
        y_boundary_array[iii + side_pts * 2] = ymin + iii * delta_y;
        x_boundary_array[iii + side_pts * 2] = xmax;
        // ymax boundary
        y_boundary_array[iii + side_pts * 3] = ymax;
        x_boundary_array[iii + side_pts * 3] = xmax - iii * delta_x;
    }

    {
        CPLErrorHandlerPusher oErrorHandlerPusher(CPLQuietErrorHandler);
        bool success = TransformWithErrorCodes(
            boundary_len, &x_boundary_array[0], &y_boundary_array[0], nullptr,
            nullptr, nullptr);
        if (success && CPLGetLastErrorType() != CE_None)
        {
            CPLErrorReset();
        }
        else if (!success)
        {
            return false;
        }
    }

    if (!degree_output)
    {
        *out_xmin = simple_min(&x_boundary_array[0], boundary_len);
        *out_xmax = simple_max(&x_boundary_array[0], boundary_len);
        *out_ymin = simple_min(&y_boundary_array[0], boundary_len);
        *out_ymax = simple_max(&y_boundary_array[0], boundary_len);

        if (poSRSTarget->IsProjected())
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);

            auto poBaseTarget = std::unique_ptr<OGRSpatialReference>(
                poSRSTarget->CloneGeogCS());
            poBaseTarget->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            auto poCTBaseTargetToSrc =
                std::unique_ptr<OGRCoordinateTransformation>(
                    OGRCreateCoordinateTransformation(poBaseTarget.get(),
                                                      poSRSSource));
            if (poCTBaseTargetToSrc)
            {
                const double dfLon0 =
                    poSRSTarget->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);

                double dfSignedPoleLat = 0;
                double dfAbsPoleLat = 90;
                bool bIncludesPole = false;
                const char *pszProjection =
                    poSRSTarget->GetAttrValue("PROJECTION");
                if (pszProjection &&
                    EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) && dfLon0 == 0)
                {
                    // This MAX_LAT_MERCATOR values is equivalent to the
                    // semi_major_axis * PI easting/northing value only
                    // for EPSG:3857, but it is also quite
                    // reasonable for other Mercator projections
                    constexpr double MAX_LAT_MERCATOR = 85.0511287798066;
                    dfAbsPoleLat = MAX_LAT_MERCATOR;
                }

                // Detect if a point at long = central_meridian and
                // lat = +/- 90deg is included in the extent.
                // Helps for example for EPSG:4326 to ESRI:53037
                for (int iSign = -1; iSign <= 1; iSign += 2)
                {
                    double dfX = dfLon0;
                    constexpr double EPS = 1e-8;
                    double dfY = iSign * (dfAbsPoleLat - EPS);

                    if (poCTBaseTargetToSrc->TransformWithErrorCodes(
                            1, &dfX, &dfY, nullptr, nullptr, nullptr) &&
                        dfX >= xmin && dfY >= ymin && dfX <= xmax &&
                        dfY <= ymax &&
                        TransformWithErrorCodes(1, &dfX, &dfY, nullptr, nullptr,
                                                nullptr))
                    {
                        dfSignedPoleLat = iSign * dfAbsPoleLat;
                        bIncludesPole = true;
                        *out_xmin = std::min(*out_xmin, dfX);
                        *out_ymin = std::min(*out_ymin, dfY);
                        *out_xmax = std::max(*out_xmax, dfX);
                        *out_ymax = std::max(*out_ymax, dfY);
                    }
                }

                const auto TryAtPlusMinus180 =
                    [this, dfLon0, xmin, ymin, xmax, ymax, out_xmin, out_ymin,
                     out_xmax, out_ymax, &poCTBaseTargetToSrc](double dfLat)
                {
                    for (int iSign = -1; iSign <= 1; iSign += 2)
                    {
                        constexpr double EPS = 1e-8;
                        double dfX =
                            fmod(dfLon0 + iSign * (180 - EPS) + 180, 360) - 180;
                        double dfY = dfLat;

                        if (poCTBaseTargetToSrc->TransformWithErrorCodes(
                                1, &dfX, &dfY, nullptr, nullptr, nullptr) &&
                            dfX >= xmin && dfY >= ymin && dfX <= xmax &&
                            dfY <= ymax &&
                            TransformWithErrorCodes(1, &dfX, &dfY, nullptr,
                                                    nullptr, nullptr))
                        {
                            *out_xmin = std::min(*out_xmin, dfX);
                            *out_ymin = std::min(*out_ymin, dfY);
                            *out_xmax = std::max(*out_xmax, dfX);
                            *out_ymax = std::max(*out_ymax, dfY);
                        }
                    }
                };

                // For a projected CRS with a central meridian != 0, try to
                // reproject the points with long = +/- 180deg of the central
                // meridian and at lat = latitude_of_origin.
                const double dfLat0 = poSRSTarget->GetNormProjParm(
                    SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                if (dfLon0 != 0)
                {
                    TryAtPlusMinus180(dfLat0);
                }

                if (bIncludesPole && dfLat0 != dfSignedPoleLat)
                {
                    TryAtPlusMinus180(dfSignedPoleLat);
                }
            }
        }
    }
    else if (north_pole_in_bounds && output_lon_lat_order)
    {
        *out_xmin = -180;
        *out_ymin = simple_min(&y_boundary_array[0], boundary_len);
        *out_xmax = 180;
        *out_ymax = 90;
    }
    else if (north_pole_in_bounds)
    {
        *out_xmin = simple_min(&x_boundary_array[0], boundary_len);
        *out_ymin = -180;
        *out_xmax = 90;
        *out_ymax = 180;
    }
    else if (south_pole_in_bounds && output_lon_lat_order)
    {
        *out_xmin = -180;
        *out_ymin = -90;
        *out_xmax = 180;
        *out_ymax = simple_max(&y_boundary_array[0], boundary_len);
    }
    else if (south_pole_in_bounds)
    {
        *out_xmin = -90;
        *out_ymin = -180;
        *out_xmax = simple_max(&x_boundary_array[0], boundary_len);
        *out_ymax = 180;
    }
    else if (output_lon_lat_order)
    {
        *out_xmin = antimeridian_min(&x_boundary_array[0], boundary_len);
        *out_xmax = antimeridian_max(&x_boundary_array[0], boundary_len);
        *out_ymin = simple_min(&y_boundary_array[0], boundary_len);
        *out_ymax = simple_max(&y_boundary_array[0], boundary_len);
    }
    else
    {
        *out_xmin = simple_min(&x_boundary_array[0], boundary_len);
        *out_xmax = simple_max(&x_boundary_array[0], boundary_len);
        *out_ymin = antimeridian_min(&y_boundary_array[0], boundary_len);
        *out_ymax = antimeridian_max(&y_boundary_array[0], boundary_len);
    }

    return *out_xmin != HUGE_VAL && *out_ymin != HUGE_VAL &&
           *out_xmax != HUGE_VAL && *out_ymax != HUGE_VAL;
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

OGRCoordinateTransformation *OGRProjCT::Clone() const
{
    std::unique_ptr<OGRProjCT> poNewCT(new OGRProjCT(*this));
#if (PROJ_VERSION_MAJOR * 10000 + PROJ_VERSION_MINOR * 100 +                   \
     PROJ_VERSION_PATCH) < 80001
    // See https://github.com/OSGeo/PROJ/pull/2582
    // This may fail before PROJ 8.0.1 if the m_pj object is a "meta"
    // operation being a set of real operations
    bool bCloneDone = ((m_pj == nullptr) == (poNewCT->m_pj == nullptr));
    if (!bCloneDone)
    {
        poNewCT.reset(new OGRProjCT());
        auto ret =
            poNewCT->Initialize(poSRSSource, m_osSrcSRS.c_str(), poSRSTarget,
                                m_osTargetSRS.c_str(), m_options);
        if (!ret)
        {
            return nullptr;
        }
    }
#endif  // PROJ_VERSION
    return poNewCT.release();
}

/************************************************************************/
/*                            GetInverse()                              */
/************************************************************************/

OGRCoordinateTransformation *OGRProjCT::GetInverse() const
{
    PJ *new_pj = nullptr;
    // m_pj can be nullptr if using m_eStrategy != PROJ
    if (m_pj && !bWebMercatorToWGS84LongLat && !bNoTransform)
    {
        // See https://github.com/OSGeo/PROJ/pull/2582
        // This may fail before PROJ 8.0.1 if the m_pj object is a "meta"
        // operation being a set of real operations
        new_pj = proj_clone(OSRGetProjTLSContext(), m_pj);
    }

    OGRCoordinateTransformationOptions newOptions(m_options);
    std::swap(newOptions.d->bHasSourceCenterLong,
              newOptions.d->bHasTargetCenterLong);
    std::swap(newOptions.d->dfSourceCenterLong,
              newOptions.d->dfTargetCenterLong);
    newOptions.d->bReverseCO = !newOptions.d->bReverseCO;
    newOptions.d->RefreshCheckWithInvertProj();

    if (new_pj == nullptr && !bNoTransform)
    {
        return OGRCreateCoordinateTransformation(poSRSTarget, poSRSSource,
                                                 newOptions);
    }

    auto poNewCT = new OGRProjCT();

    if (poSRSTarget)
        poNewCT->poSRSSource = poSRSTarget->Clone();
    poNewCT->m_eSourceFirstAxisOrient = m_eTargetFirstAxisOrient;
    poNewCT->bSourceLatLong = bTargetLatLong;
    poNewCT->bSourceWrap = bTargetWrap;
    poNewCT->dfSourceWrapLong = dfTargetWrapLong;
    poNewCT->bSourceIsDynamicCRS = bTargetIsDynamicCRS;
    poNewCT->dfSourceCoordinateEpoch = dfTargetCoordinateEpoch;
    poNewCT->m_osSrcSRS = m_osTargetSRS;

    if (poSRSSource)
        poNewCT->poSRSTarget = poSRSSource->Clone();
    poNewCT->m_eTargetFirstAxisOrient = m_eSourceFirstAxisOrient;
    poNewCT->bTargetLatLong = bSourceLatLong;
    poNewCT->bTargetWrap = bSourceWrap;
    poNewCT->dfTargetWrapLong = dfSourceWrapLong;
    poNewCT->bTargetIsDynamicCRS = bSourceIsDynamicCRS;
    poNewCT->dfTargetCoordinateEpoch = dfSourceCoordinateEpoch;
    poNewCT->m_osTargetSRS = m_osSrcSRS;

    poNewCT->ComputeThreshold();

    poNewCT->m_pj = new_pj;
    poNewCT->m_bReversePj = !m_bReversePj;
    poNewCT->bNoTransform = bNoTransform;
    poNewCT->m_eStrategy = m_eStrategy;
    poNewCT->m_options = newOptions;

    poNewCT->DetectWebMercatorToWGS84();

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

CTCacheKey OGRProjCT::MakeCacheKey(
    const OGRSpatialReference *poSRS1, const char *pszSrcSRS,
    const OGRSpatialReference *poSRS2, const char *pszTargetSRS,
    const OGRCoordinateTransformationOptions &options)
{
    const auto GetKeyForSRS =
        [](const OGRSpatialReference *poSRS, const char *pszText)
    {
        if (poSRS)
        {
            std::string ret(pszText);
            const auto &mapping = poSRS->GetDataAxisToSRSAxisMapping();
            for (const auto &axis : mapping)
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

    std::string ret(GetKeyForSRS(poSRS1, pszSrcSRS));
    ret += GetKeyForSRS(poSRS2, pszTargetSRS);
    ret += options.d->GetKey();
    return ret;
}

/************************************************************************/
/*                           InsertIntoCache()                          */
/************************************************************************/

void OGRProjCT::InsertIntoCache(OGRProjCT *poCT)
{
    {
        std::lock_guard<std::mutex> oGuard(g_oCTCacheMutex);
        if (g_poCTCache == nullptr)
        {
            g_poCTCache = new lru11::Cache<CTCacheKey, CTCacheValue>();
        }
    }
    const auto key = MakeCacheKey(poCT->poSRSSource, poCT->m_osSrcSRS.c_str(),
                                  poCT->poSRSTarget,
                                  poCT->m_osTargetSRS.c_str(), poCT->m_options);

    std::lock_guard<std::mutex> oGuard(g_oCTCacheMutex);
    if (g_poCTCache->contains(key))
    {
        delete poCT;
        return;
    }
    g_poCTCache->insert(key, std::unique_ptr<OGRProjCT>(poCT));
}

/************************************************************************/
/*                            FindFromCache()                           */
/************************************************************************/

OGRProjCT *OGRProjCT::FindFromCache(
    const OGRSpatialReference *poSource, const char *pszSrcSRS,
    const OGRSpatialReference *poTarget, const char *pszTargetSRS,
    const OGRCoordinateTransformationOptions &options)
{
    {
        std::lock_guard<std::mutex> oGuard(g_oCTCacheMutex);
        if (g_poCTCache == nullptr || g_poCTCache->empty())
            return nullptr;
    }

    const auto key =
        MakeCacheKey(poSource, pszSrcSRS, poTarget, pszTargetSRS, options);
    // Get value from cache and remove it
    std::lock_guard<std::mutex> oGuard(g_oCTCacheMutex);
    CTCacheValue *cachedValue = g_poCTCache->getPtr(key);
    if (cachedValue)
    {
        auto poCT = cachedValue->release();
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
 * @return TRUE if a transformation could be found (but not all points may
 * have necessarily succeed to transform), otherwise FALSE.
 */
int CPL_STDCALL OCTTransform(OGRCoordinateTransformationH hTransform,
                             int nCount, double *x, double *y, double *z)

{
    VALIDATE_POINTER1(hTransform, "OCTTransform", FALSE);

    return OGRCoordinateTransformation::FromHandle(hTransform)
        ->Transform(nCount, x, y, z);
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
 * @return TRUE if a transformation could be found (but not all points may
 * have necessarily succeed to transform), otherwise FALSE.
 */
int CPL_STDCALL OCTTransformEx(OGRCoordinateTransformationH hTransform,
                               int nCount, double *x, double *y, double *z,
                               int *pabSuccess)

{
    VALIDATE_POINTER1(hTransform, "OCTTransformEx", FALSE);

    return OGRCoordinateTransformation::FromHandle(hTransform)
        ->Transform(nCount, x, y, z, pabSuccess);
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
 * @param pabSuccess Output array of nCount value that will be set to
 * TRUE/FALSE. Might be NULL.
 * @since GDAL 3.0
 * @return TRUE if a transformation could be found (but not all points may
 * have necessarily succeed to transform), otherwise FALSE.
 */
int OCTTransform4D(OGRCoordinateTransformationH hTransform, int nCount,
                   double *x, double *y, double *z, double *t, int *pabSuccess)

{
    VALIDATE_POINTER1(hTransform, "OCTTransform4D", FALSE);

    return OGRCoordinateTransformation::FromHandle(hTransform)
        ->Transform(nCount, x, y, z, t, pabSuccess);
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
 * @return TRUE if a transformation could be found (but not all points may
 * have necessarily succeed to transform), otherwise FALSE.
 */
int OCTTransform4DWithErrorCodes(OGRCoordinateTransformationH hTransform,
                                 int nCount, double *x, double *y, double *z,
                                 double *t, int *panErrorCodes)

{
    VALIDATE_POINTER1(hTransform, "OCTTransform4DWithErrorCodes", FALSE);

    return OGRCoordinateTransformation::FromHandle(hTransform)
        ->TransformWithErrorCodes(nCount, x, y, z, t, panErrorCodes);
}

/************************************************************************/
/*                           OCTTransformBounds()                       */
/************************************************************************/
/** \brief Transform boundary.
 *
 * Transform boundary densifying the edges to account for nonlinear
 * transformations along these edges and extracting the outermost bounds.
 *
 * If the destination CRS is geographic, the first axis is longitude,
 * and xmax < xmin then the bounds crossed the antimeridian.
 * In this scenario there are two polygons, one on each side of the
 * antimeridian. The first polygon should be constructed with (xmin, ymin, 180,
 * ymax) and the second with (-180, ymin, xmax, ymax).
 *
 * If the destination CRS is geographic, the first axis is latitude,
 * and ymax < ymin then the bounds crossed the antimeridian.
 * In this scenario there are two polygons, one on each side of the
 * antimeridian. The first polygon should be constructed with (ymin, xmin, ymax,
 * 180) and the second with (ymin, -180, ymax, xmax).
 *
 * @param hTransform Transformation object
 * @param xmin Minimum bounding coordinate of the first axis in source CRS.
 * @param ymin Minimum bounding coordinate of the second axis in source CRS.
 * @param xmax Maximum bounding coordinate of the first axis in source CRS.
 * @param ymax Maximum bounding coordinate of the second axis in source CRS.
 * @param out_xmin Minimum bounding coordinate of the first axis in target CRS
 * @param out_ymin Minimum bounding coordinate of the second axis in target CRS.
 * @param out_xmax Maximum bounding coordinate of the first axis in target CRS.
 * @param out_ymax Maximum bounding coordinate of the second axis in target CRS.
 * @param densify_pts Recommended to use 21. This is the number of points
 *     to use to densify the bounding polygon in the transformation.
 * @return TRUE if successful. FALSE if failures encountered.
 * @since 3.4
 */
int CPL_STDCALL OCTTransformBounds(OGRCoordinateTransformationH hTransform,
                                   const double xmin, const double ymin,
                                   const double xmax, const double ymax,
                                   double *out_xmin, double *out_ymin,
                                   double *out_xmax, double *out_ymax,
                                   int densify_pts)

{
    VALIDATE_POINTER1(hTransform, "TransformBounds", FALSE);

    return OGRProjCT::FromHandle(hTransform)
        ->TransformBounds(xmin, ymin, xmax, ymax, out_xmin, out_ymin, out_xmax,
                          out_ymax, densify_pts);
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
