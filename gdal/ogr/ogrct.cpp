/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSCoordinateTransformation class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"

#ifndef PROJ_VERSION
#define PROJ_VERSION 4
#endif

#ifdef PROJ_STATIC
#if PROJ_VERSION >= 5
#include "proj.h"

#if PROJ_VERSION_MAJOR == 5 && PROJ_VERSION_MINOR == 0
// FIXME: only available in proj 5.1.0, so cheating for now
extern "C" const char* pj_strerrno(int);
static const char* proj_errno_string(int err)
{
    return pj_strerrno(err);
}
#endif

constexpr double RAD_TO_DEG = 57.29577951308232;
constexpr double DEG_TO_RAD = 0.0174532925199432958;

#else
#include "proj_api.h"
#endif
#endif

CPL_CVSID("$Id$")

#if PROJ_VERSION == 4
/* ==================================================================== */
/*      PROJ.4 interface stuff.                                         */
/* ==================================================================== */
#ifndef PROJ_STATIC
#define projPJ void *
#define projCtx void *
constexpr double RAD_TO_DEG = 57.29577951308232;
constexpr double DEG_TO_RAD = 0.0174532925199432958;

#else

#if PJ_VERSION < 480
#define projCtx void *
#endif

#endif

static CPLMutex *hPROJMutex = nullptr;

static projPJ   (*pfn_pj_init_plus)( const char * ) = nullptr;
static projPJ   (*pfn_pj_init)( int, char** ) = nullptr;
static void     (*pfn_pj_free)( projPJ ) = nullptr;
static int      (*pfn_pj_transform)( projPJ, projPJ, long, int,
                                     double *, double *, double * ) = nullptr;
static int      *(*pfn_pj_get_errno_ref)( void ) = nullptr;
static char     *(*pfn_pj_strerrno)( int ) = nullptr;
static char     *(*pfn_pj_get_def)( projPJ, int ) = nullptr;
static void     (*pfn_pj_dalloc)( void * ) = nullptr;

static projPJ (*pfn_pj_init_plus_ctx)( projCtx, const char * ) = nullptr;
static int (*pfn_pj_ctx_get_errno)( projCtx ) = nullptr;
static projCtx (*pfn_pj_ctx_alloc)() = nullptr;
static void    (*pfn_pj_ctx_free)( projCtx ) = nullptr;

// Locale-safe proj starts with 4.10.
#if defined(PJ_LOCALE_SAFE)
static bool      bProjLocaleSafe = PJ_LOCALE_SAFE != 0;
#else
static bool      bProjLocaleSafe = false;
#endif

#if defined(WIN32) && !defined(__MINGW32__)
#  define LIBNAME "proj.dll"
#elif defined(__MINGW32__)
// XXX: If PROJ.4 library was properly built using libtool in Cygwin or MinGW
// environments it has the interface version number embedded in the file name
// (it is CURRENT-AGE number). If DLL came somewhere else (e.g. from MSVC
// build) it can be named either way, so use PROJSO environment variable to
// specify the right library name. By default assume that in Cygwin/MinGW all
// components were built in the same way.
#  define LIBNAME "libproj-9.dll"
#elif defined(__CYGWIN__)
#  define LIBNAME "cygproj-9.dll"
#elif defined(__APPLE__)
#  define LIBNAME "libproj.dylib"
#else
#  define LIBNAME "libproj.so"
#endif

#endif // PROJ_VERSION == 4

/************************************************************************/
/*                         OCTCleanupProjMutex()                        */
/************************************************************************/

void OCTCleanupProjMutex()
{
#if PROJ_VERSION == 4
    if( hPROJMutex != nullptr )
    {
        CPLDestroyMutex(hPROJMutex);
        hPROJMutex = nullptr;
    }
#endif
}

/************************************************************************/
/*                              OGRProj4CT                              */
/************************************************************************/

class OGRProj4CT : public OGRCoordinateTransformation
{
    OGRSpatialReference *poSRSSource = nullptr;
    bool        bSourceLatLong = false;
    double      dfSourceToRadians = 0.0;
    bool        bSourceWrap = false;
    double      dfSourceWrapLong = 0.0;

    OGRSpatialReference *poSRSTarget = nullptr;
    bool        bTargetLatLong = false;
    double      dfTargetFromRadians = 0.0;
    bool        bTargetWrap = false;
    double      dfTargetWrapLong = 0.0;

    bool        bIdentityTransform = false;
    bool        bWebMercatorToWGS84 = false;

    int         nErrorCount = 0;

    bool        bCheckWithInvertProj = false;
    double      dfThreshold = 0.0;

#if PROJ_VERSION == 4
    void        *psPJSource = nullptr;
    void        *psPJTarget = nullptr;
    projCtx     pjctx = nullptr;
#else
    PJ_CONTEXT* m_pjctx = nullptr;
    PJ*         m_pj = nullptr;
#endif

    int         InitializeNoLock( OGRSpatialReference *poSource,
                                  OGRSpatialReference *poTarget );

    int         nMaxCount = 0;
    double     *padfOriX = nullptr;
    double     *padfOriY = nullptr;
    double     *padfOriZ = nullptr;
    double     *padfTargetX = nullptr;
    double     *padfTargetY = nullptr;
    double     *padfTargetZ = nullptr;

    bool        m_bEmitErrors = true;

    bool        bNoTransform = false;

public:
    OGRProj4CT();
    ~OGRProj4CT() override;

    int         Initialize( OGRSpatialReference *poSource,
                            OGRSpatialReference *poTarget );

    virtual OGRSpatialReference *GetSourceCS() override;
    virtual OGRSpatialReference *GetTargetCS() override;
    virtual int Transform( int nCount,
                           double *x, double *y, double *z = nullptr ) override;
    virtual int TransformEx( int nCount,
                             double *x, double *y, double *z = nullptr,
                             int *panSuccess = nullptr ) override;

    // TODO(schwehr): Make GetEmitErrors const.
    virtual bool GetEmitErrors() override { return m_bEmitErrors; }
    virtual void SetEmitErrors( bool bEmitErrors ) override
        { m_bEmitErrors = bEmitErrors; }
};

#if PROJ_VERSION == 4

/************************************************************************/
/*                        GetProjLibraryName()                          */
/************************************************************************/

static const char* GetProjLibraryName()
{
    const char *pszLibName = LIBNAME;
    if( CPLGetConfigOption("PROJSO", nullptr) != nullptr )
        pszLibName = CPLGetConfigOption("PROJSO", nullptr);
    return pszLibName;
}

/************************************************************************/
/*                          LoadProjLibrary()                           */
/************************************************************************/

static bool LoadProjLibrary_unlocked()

{
    static bool bTriedToLoad = false;

    if( bTriedToLoad )
        return pfn_pj_transform != nullptr;

    bTriedToLoad = true;

    const char *pszLibName = GetProjLibraryName();

#ifdef PROJ_STATIC
    pfn_pj_init = pj_init;
    pfn_pj_init_plus = pj_init_plus;
    pfn_pj_free = pj_free;
    pfn_pj_transform = pj_transform;
    pfn_pj_get_errno_ref = reinterpret_cast<int *(*)(void)>(pj_get_errno_ref);
    pfn_pj_strerrno = pj_strerrno;
    pfn_pj_dalloc = pj_dalloc;
#if PJ_VERSION >= 446
    pfn_pj_get_def = pj_get_def;
#endif
#if PJ_VERSION >= 480
    pfn_pj_ctx_alloc = pj_ctx_alloc;
    pfn_pj_ctx_free = pj_ctx_free;
    pfn_pj_init_plus_ctx = pj_init_plus_ctx;
    pfn_pj_ctx_get_errno = pj_ctx_get_errno;
#endif
#else
    CPLPushErrorHandler( CPLQuietErrorHandler );

    // coverity[tainted_string]
    pfn_pj_init = reinterpret_cast<projPJ (*)(int, char**)>(CPLGetSymbol( pszLibName,
                                                          "pj_init" ));
    CPLPopErrorHandler();

    if( pfn_pj_init == nullptr )
       return false;

    pfn_pj_init_plus = reinterpret_cast<projPJ (*)(const char *)>(
        CPLGetSymbol( pszLibName, "pj_init_plus" ));
    pfn_pj_free = reinterpret_cast<void (*)(projPJ)>(
        CPLGetSymbol( pszLibName, "pj_free" ));
    pfn_pj_transform = reinterpret_cast<int (*)(projPJ, projPJ, long, int, double *,
                                double *, double *)>(
        CPLGetSymbol( pszLibName, "pj_transform" ));
    pfn_pj_get_errno_ref = reinterpret_cast<int *(*)(void)>(
        CPLGetSymbol( pszLibName, "pj_get_errno_ref" ));
    pfn_pj_strerrno = reinterpret_cast<char *(*)(int)>(
        CPLGetSymbol( pszLibName, "pj_strerrno" ));

    CPLPushErrorHandler( CPLQuietErrorHandler );
    pfn_pj_get_def = reinterpret_cast<char *(*)(projPJ, int)>(
        CPLGetSymbol( pszLibName, "pj_get_def" ));
    pfn_pj_dalloc = reinterpret_cast<void (*)(void*)>(
        CPLGetSymbol( pszLibName, "pj_dalloc" ));

    // PROJ 4.8.0 symbols.
    pfn_pj_ctx_alloc = reinterpret_cast<projCtx (*)( void )>(
        CPLGetSymbol( pszLibName, "pj_ctx_alloc" ));
    pfn_pj_ctx_free = reinterpret_cast<void (*)( projCtx )>(
        CPLGetSymbol( pszLibName, "pj_ctx_free" ));
    pfn_pj_init_plus_ctx = reinterpret_cast<projPJ (*)( projCtx, const char * )>(
        CPLGetSymbol( pszLibName, "pj_init_plus_ctx" ));
    pfn_pj_ctx_get_errno = reinterpret_cast<int (*)( projCtx )>(
        CPLGetSymbol( pszLibName, "pj_ctx_get_errno" ));

    bProjLocaleSafe = CPLGetSymbol(pszLibName, "pj_atof") != nullptr;

    CPLPopErrorHandler();
    CPLErrorReset();
#endif

    if( pfn_pj_ctx_alloc != nullptr &&
        pfn_pj_ctx_free != nullptr &&
        pfn_pj_init_plus_ctx != nullptr &&
        pfn_pj_ctx_get_errno != nullptr &&
        CPLTestBool(CPLGetConfigOption("USE_PROJ_480_FEATURES", "YES")) )
    {
        CPLDebug("OGRCT", "PROJ >= 4.8.0 features enabled");
    }
    else
    {
        pfn_pj_ctx_alloc = nullptr;
        pfn_pj_ctx_free = nullptr;
        pfn_pj_init_plus_ctx = nullptr;
        pfn_pj_ctx_get_errno = nullptr;
    }

    if( bProjLocaleSafe )
        CPLDebug("OGRCT", "Using locale-safe proj version");

    if( pfn_pj_transform == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to load %s, but couldn't find pj_transform.  "
                  "Please upgrade to PROJ 4.1.2 or later.",
                  pszLibName );

        return false;
    }

    return true;
}

static bool LoadProjLibrary()

{
    CPLMutexHolderD( &hPROJMutex );
    return LoadProjLibrary_unlocked();
}

/************************************************************************/
/*                         OCTProj4Normalize()                          */
/************************************************************************/

/** This function is really just here since we already have all
 * the code to load libproj.so.  It is intended to "normalize"
 * a proj.4 definition, expanding +init= definitions and so
 * forth as possible.
 */
static char *OCTProj4NormalizeInternal( const char *pszProj4Src )
{
    projPJ psPJSource = pfn_pj_init_plus( pszProj4Src );

    if( psPJSource == nullptr )
        return CPLStrdup( pszProj4Src );

    char *pszNewProj4Def = pfn_pj_get_def( psPJSource, 0 );

    pfn_pj_free( psPJSource );

    if( pszNewProj4Def == nullptr )
        return CPLStrdup( pszProj4Src );

    char *pszCopy = CPLStrdup( pszNewProj4Def );
    pfn_pj_dalloc( pszNewProj4Def );

    return pszCopy;

}

#endif // PROJ_VERSION == 4

char *OCTProj4Normalize( const char *pszProj4Src )

{
#if PROJ_VERSION == 4
    CPLMutexHolderD( &hPROJMutex );

    if( !LoadProjLibrary_unlocked() || pfn_pj_dalloc == nullptr ||
        pfn_pj_get_def == nullptr )
        return CPLStrdup( pszProj4Src );

    if( bProjLocaleSafe )
    {
        return OCTProj4NormalizeInternal(pszProj4Src);
    }
    else
    {
        CPLLocaleC oLocaleEnforcer;
        return OCTProj4NormalizeInternal(pszProj4Src);
    }
#else
    PJ_CONTEXT* ctxt = proj_context_create ();
    PJ* pj = proj_create(ctxt, pszProj4Src);
    if( !pj )
    {
        proj_context_destroy (ctxt);
        return CPLStrdup(pszProj4Src);
    }
    CPLString osDef = proj_pj_info(pj).definition;
    proj_destroy(pj);
    proj_context_destroy (ctxt);
    char** papszTokens = CSLTokenizeString2(osDef, " ", 0);
    CPLString osRet;
    for( char** papszIter = papszTokens; papszIter && *papszIter; ++papszIter )
    {
        if( !osRet.empty() )
            osRet += " ";
        osRet += "+";
        osRet += *papszIter;
    }
    CSLDestroy(papszTokens);
    return CPLStrdup(osRet);
#endif
}

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
    delete OGRCoordinateTransformation::FromHandle(hCT);
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
    delete poCT;
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
 * The PROJ.4 library must be available at run-time.
 *
 * @param poSource source spatial reference system.
 * @param poTarget target spatial reference system.
 * @return NULL on failure or a ready to use transformation object.
 */

OGRCoordinateTransformation*
OGRCreateCoordinateTransformation( OGRSpatialReference *poSource,
                                   OGRSpatialReference *poTarget )

{
#if PROJ_VERSION == 4
    if( pfn_pj_init == nullptr && !LoadProjLibrary() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to load PROJ.4 library (%s), creation of "
                  "OGRCoordinateTransformation failed.",
                  GetProjLibraryName() );
        return nullptr;
    }
#endif

    OGRProj4CT *poCT = new OGRProj4CT();

    if( !poCT->Initialize( poSource, poTarget ) )
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
 * This is the same as the C++ function OGRCreateCoordinateTransformation().
 *
 * Input spatial reference system objects are assigned
 * by copy (calling clone() method) and no ownership transfer occurs.
 *
 * OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects.
 *
 * The PROJ.4 library must be available at run-time.
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
/*                             OGRProj4CT()                             */
/************************************************************************/

OGRProj4CT::OGRProj4CT()
{
#if PROJ_VERSION == 4
    if( pfn_pj_ctx_alloc != nullptr )
        pjctx = pfn_pj_ctx_alloc();
#else
    m_pjctx = proj_context_create();
#endif
}

/************************************************************************/
/*                            ~OGRProj4CT()                             */
/************************************************************************/

OGRProj4CT::~OGRProj4CT()

{
    if( poSRSSource != nullptr )
    {
        if( poSRSSource->Dereference() <= 0 )
            delete poSRSSource;
    }

    if( poSRSTarget != nullptr )
    {
        if( poSRSTarget->Dereference() <= 0 )
            delete poSRSTarget;
    }

#if PROJ_VERSION == 4
    if( pjctx != nullptr )
    {
        if( psPJSource != nullptr )
            pfn_pj_free( psPJSource );

        if( psPJTarget != nullptr )
            pfn_pj_free( psPJTarget );

        pfn_pj_ctx_free(pjctx);
    }
    else
    {
        CPLMutexHolderD( &hPROJMutex );

        if( psPJSource != nullptr )
            pfn_pj_free( psPJSource );

        if( psPJTarget != nullptr )
            pfn_pj_free( psPJTarget );
    }
#else
    if( m_pj )
        proj_destroy(m_pj);
    proj_context_destroy(m_pjctx);
#endif

    CPLFree(padfOriX);
    CPLFree(padfOriY);
    CPLFree(padfOriZ);
    CPLFree(padfTargetX);
    CPLFree(padfTargetY);
    CPLFree(padfTargetZ);
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRProj4CT::Initialize( OGRSpatialReference * poSourceIn,
                            OGRSpatialReference * poTargetIn )

{
#if PROJ_VERSION == 4
    if( bProjLocaleSafe )
    {
        return InitializeNoLock(poSourceIn, poTargetIn);
    }

    CPLLocaleC oLocaleEnforcer;
    if( pjctx != nullptr )
    {
        return InitializeNoLock(poSourceIn, poTargetIn);
    }

    CPLMutexHolderD( &hPROJMutex );
#endif
    return InitializeNoLock(poSourceIn, poTargetIn);
}

/************************************************************************/
/*                         InitializeNoLock()                           */
/************************************************************************/

int OGRProj4CT::InitializeNoLock( OGRSpatialReference * poSourceIn,
                                  OGRSpatialReference * poTargetIn )

{
    if( poSourceIn == nullptr || poTargetIn == nullptr )
        return FALSE;

    poSRSSource = poSourceIn->Clone();
    poSRSTarget = poTargetIn->Clone();

    bSourceLatLong = CPL_TO_BOOL(poSRSSource->IsGeographic());
    bTargetLatLong = CPL_TO_BOOL(poSRSTarget->IsGeographic());

/* -------------------------------------------------------------------- */
/*      Setup source and target translations to radians for lat/long    */
/*      systems.                                                        */
/* -------------------------------------------------------------------- */
    dfSourceToRadians = DEG_TO_RAD;
    bSourceWrap = false;
    dfSourceWrapLong = 0.0;

    if( bSourceLatLong )
    {
        OGR_SRSNode *poUNITS = poSRSSource->GetAttrNode( "GEOGCS|UNIT" );
        if( poUNITS && poUNITS->GetChildCount() >= 2 )
        {
            dfSourceToRadians = CPLAtof(poUNITS->GetChild(1)->GetValue());
            if( dfSourceToRadians == 0.0 )
                dfSourceToRadians = DEG_TO_RAD;
        }
    }

    dfTargetFromRadians = RAD_TO_DEG;
    bTargetWrap = false;
    dfTargetWrapLong = 0.0;

    if( bTargetLatLong )
    {
        OGR_SRSNode *poUNITS = poSRSTarget->GetAttrNode( "GEOGCS|UNIT" );
        if( poUNITS && poUNITS->GetChildCount() >= 2 )
        {
            const double dfTargetToRadians =
                CPLAtof(poUNITS->GetChild(1)->GetValue());
            if( dfTargetToRadians != 0.0 )
                dfTargetFromRadians = 1 / dfTargetToRadians;
        }
    }

/* -------------------------------------------------------------------- */
/*      Preliminary logic to setup wrapping.                            */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "CENTER_LONG", nullptr ) != nullptr )
    {
        bSourceWrap = true;
        bTargetWrap = true;
        dfSourceWrapLong = dfTargetWrapLong =
            CPLAtof(CPLGetConfigOption( "CENTER_LONG", "" ));
        CPLDebug( "OGRCT", "Wrap at %g.", dfSourceWrapLong );
    }

    const char *pszCENTER_LONG =
        poSRSSource->GetExtension( "GEOGCS", "CENTER_LONG" );
    if( pszCENTER_LONG != nullptr )
    {
        dfSourceWrapLong = CPLAtof(pszCENTER_LONG);
        bSourceWrap = true;
        CPLDebug( "OGRCT", "Wrap source at %g.", dfSourceWrapLong );
    }

    pszCENTER_LONG = poSRSTarget->GetExtension( "GEOGCS", "CENTER_LONG" );
    if( pszCENTER_LONG != nullptr )
    {
        dfTargetWrapLong = CPLAtof(pszCENTER_LONG);
        bTargetWrap = true;
        CPLDebug( "OGRCT", "Wrap target at %g.", dfTargetWrapLong );
    }

    bCheckWithInvertProj =
        CPLTestBool(CPLGetConfigOption( "CHECK_WITH_INVERT_PROJ", "NO" ));

    // The threshold is experimental. Works well with the cases of ticket #2305.
    if( bSourceLatLong )
        dfThreshold = CPLAtof(CPLGetConfigOption( "THRESHOLD", ".1" ));
    else
        // 1 works well for most projections, except for +proj=aeqd that
        // requires a tolerance of 10000.
        dfThreshold = CPLAtof(CPLGetConfigOption( "THRESHOLD", "10000" ));

    // OGRThreadSafety: The following variable is not a thread safety issue
    // since the only issue is incrementing while accessing which at worse
    // means debug output could be one "increment" late.
    static int nDebugReportCount = 0;

    char *pszSrcProj4Defn = nullptr;

    if( poSRSSource->exportToProj4( &pszSrcProj4Defn ) != OGRERR_NONE )
    {
        CPLFree( pszSrcProj4Defn );
        return FALSE;
    }

    if( strlen(pszSrcProj4Defn) == 0 )
    {
        CPLFree( pszSrcProj4Defn );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No PROJ.4 translation for source SRS, coordinate "
                  "transformation initialization has failed." );
        return FALSE;
    }

    char *pszDstProj4Defn = nullptr;

    if( poSRSTarget->exportToProj4( &pszDstProj4Defn ) != OGRERR_NONE )
    {
        CPLFree( pszSrcProj4Defn );
        CPLFree( pszDstProj4Defn );
        return FALSE;
    }

    if( strlen(pszDstProj4Defn) == 0 )
    {
        CPLFree( pszSrcProj4Defn );
        CPLFree( pszDstProj4Defn );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No PROJ.4 translation for destination SRS, coordinate "
                  "transformation initialization has failed." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Optimization to avoid useless nadgrids evaluation.              */
/*      For example when converting between WGS84 and WebMercator       */
/* -------------------------------------------------------------------- */
    if( pszSrcProj4Defn[strlen(pszSrcProj4Defn)-1] == ' ' )
        pszSrcProj4Defn[strlen(pszSrcProj4Defn)-1] = 0;
    if( pszDstProj4Defn[strlen(pszDstProj4Defn)-1] == ' ' )
        pszDstProj4Defn[strlen(pszDstProj4Defn)-1] = 0;
    char* pszNeedle = strstr(pszSrcProj4Defn, "  ");
    if( pszNeedle )
        memmove(pszNeedle, pszNeedle + 1, strlen(pszNeedle + 1)+1);
    pszNeedle = strstr(pszDstProj4Defn, "  ");
    if( pszNeedle )
        memmove(pszNeedle, pszNeedle + 1, strlen(pszNeedle + 1)+1);

    if( (strstr(pszSrcProj4Defn, "+datum=WGS84") != nullptr ||
         strstr(pszSrcProj4Defn,
                "+ellps=WGS84 +towgs84=0,0,0,0,0,0,0 ") != nullptr) &&
        strstr(pszDstProj4Defn, "+nadgrids=@null ") != nullptr &&
        strstr(pszDstProj4Defn, "+towgs84") == nullptr )
    {
        char* pszDst = strstr(pszSrcProj4Defn, "+towgs84=0,0,0,0,0,0,0 ");
        if( pszDst != nullptr )
        {
            char *pszSrc = pszDst + strlen("+towgs84=0,0,0,0,0,0,0 ");
            memmove(pszDst, pszSrc, strlen(pszSrc)+1);
        }
        else
        {
            memcpy(strstr(pszSrcProj4Defn, "+datum=WGS84"), "+ellps", 6);
        }

        pszDst = strstr(pszDstProj4Defn, "+nadgrids=@null ");
        char *pszSrc = pszDst + strlen("+nadgrids=@null ");
        memmove(pszDst, pszSrc, strlen(pszSrc)+1);

        pszDst = strstr(pszDstProj4Defn, "+wktext ");
        if( pszDst )
        {
            pszSrc = pszDst + strlen("+wktext ");
            memmove(pszDst, pszSrc, strlen(pszSrc)+1);
        }
    }
    else
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
        bWebMercatorToWGS84 =
            strcmp(pszDstProj4Defn,
                   "+proj=longlat +ellps=WGS84 +no_defs") == 0 &&
            strcmp(pszSrcProj4Defn,
                   "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 "
                   "+x_0=0.0 +y_0=0 +k=1.0 +units=m +no_defs") == 0;
    }

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for source if projection.               */
/* -------------------------------------------------------------------- */
#if PROJ_VERSION == 4
    if( !bWebMercatorToWGS84 )
    {
        if( pjctx )
            psPJSource = pfn_pj_init_plus_ctx( pjctx, pszSrcProj4Defn );
        else
            psPJSource = pfn_pj_init_plus( pszSrcProj4Defn );

        if( psPJSource == nullptr )
        {
            if( pjctx != nullptr)
            {
                const int l_pj_errno = pfn_pj_ctx_get_errno(pjctx);

                // pfn_pj_strerrno not yet thread-safe in PROJ 4.8.0.
                CPLMutexHolderD(&hPROJMutex);
                CPLError( CE_Failure, CPLE_NotSupported,
                          "Failed to initialize PROJ.4 with `%s'.\n%s",
                          pszSrcProj4Defn, pfn_pj_strerrno(l_pj_errno) );
            }
            else if( pfn_pj_get_errno_ref != nullptr
                && pfn_pj_strerrno != nullptr )
            {
                const int *p_pj_errno = pfn_pj_get_errno_ref();

                CPLError( CE_Failure, CPLE_NotSupported,
                          "Failed to initialize PROJ.4 with `%s'.\n%s",
                          pszSrcProj4Defn, pfn_pj_strerrno(*p_pj_errno) );
            }
            else
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                          "Failed to initialize PROJ.4 with `%s'.",
                          pszSrcProj4Defn );
            }
        }
    }
#endif

    if( nDebugReportCount < 10 )
        CPLDebug( "OGRCT", "Source: %s", pszSrcProj4Defn );

#if PROJ_VERSION == 4
    if( !bWebMercatorToWGS84 && psPJSource == nullptr )
    {
        CPLFree( pszSrcProj4Defn );
        CPLFree( pszDstProj4Defn );
        return FALSE;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for target if projection.               */
/* -------------------------------------------------------------------- */
#if PROJ_VERSION == 4
    if( !bWebMercatorToWGS84 )
    {
        if( pjctx )
            psPJTarget = pfn_pj_init_plus_ctx( pjctx, pszDstProj4Defn );
        else
            psPJTarget = pfn_pj_init_plus( pszDstProj4Defn );

        if( psPJTarget == nullptr )
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Failed to initialize PROJ.4 with `%s'.",
                      pszDstProj4Defn );
    }
#endif

    if( nDebugReportCount < 10 )
    {
        CPLDebug( "OGRCT", "Target: %s", pszDstProj4Defn );
        nDebugReportCount++;
    }

#if PROJ_VERSION >= 5
    if( !bWebMercatorToWGS84 )
    {
        CPLString osPipeline("+proj=pipeline +step ");
        osPipeline += pszSrcProj4Defn;
        osPipeline += " +inv +step ";
        osPipeline += pszDstProj4Defn;
        m_pj = proj_create(m_pjctx, osPipeline.c_str());
#ifdef DEBUG_VERBOSE
        CPLDebug("OGRCT", "%s", osPipeline.c_str());
#endif
        if( !m_pj )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Failed to initialize PROJ pipeline "
                      "from `%s' to `%s': %s",
                      pszSrcProj4Defn,
                      pszDstProj4Defn,
                      proj_errno_string(proj_context_errno(m_pjctx)) );
            CPLFree( pszSrcProj4Defn );
            CPLFree( pszDstProj4Defn );
            return FALSE;
        }
    }
#else
    if( !bWebMercatorToWGS84 && psPJTarget == nullptr )
    {
        CPLFree( pszSrcProj4Defn );
        CPLFree( pszDstProj4Defn );
        return FALSE;
    }
#endif

    // Determine if we really have a transformation to do at the proj.4 level
    // (but we may have a unit transformation to do)
    bIdentityTransform = strcmp(pszSrcProj4Defn, pszDstProj4Defn) == 0;

    // Determine if we can skip the transformation completely.
    // Assume that source and target units are defined with at least
    // 10 correct significant digits; hence the 1E-9 tolerance used.
    bNoTransform = bIdentityTransform && bSourceLatLong && !bSourceWrap &&
                    bTargetLatLong && !bTargetWrap &&
                    fabs(dfSourceToRadians * dfTargetFromRadians - 1.0) < 1E-9;

    CPLFree( pszSrcProj4Defn );
    CPLFree( pszDstProj4Defn );

    return TRUE;
}

/************************************************************************/
/*                            GetSourceCS()                             */
/************************************************************************/

OGRSpatialReference *OGRProj4CT::GetSourceCS()

{
    return poSRSSource;
}

/************************************************************************/
/*                            GetTargetCS()                             */
/************************************************************************/

OGRSpatialReference *OGRProj4CT::GetTargetCS()

{
    return poSRSTarget;
}

/************************************************************************/
/*                             Transform()                              */
/*                                                                      */
/*      This is a small wrapper for the extended transform version.     */
/************************************************************************/

int OGRProj4CT::Transform( int nCount, double *x, double *y, double *z )

{
    int *pabSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nCount));

    bool bOverallSuccess =
        CPL_TO_BOOL(TransformEx( nCount, x, y, z, pabSuccess ));

    for( int i = 0; i < nCount; i++ )
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
/*                            TransformEx()                             */
/************************************************************************/

/** Transform an array of points
 *
 * @param nCount Number of points
 * @param x Array of nCount x values.
 * @param y Array of nCount y values.
 * @param z Array of nCount z values.
 * @param pabSuccess Output array of nCount value that will be set to TRUE/FALSE
 * @return TRUE or FALSE
 */
int OGRProj4CT::TransformEx( int nCount, double *x, double *y, double *z,
                             int *pabSuccess )

{
    // Prevent any coordinate modification when possible
    if ( bNoTransform )
    {
        if( pabSuccess )
        {
            for( int i = 0; i < nCount; i++ )
            {
                 pabSuccess[i] = TRUE;
            }
        }
        return TRUE;
    }

    // Workaround potential bugs in proj.4 such as
    // the one of https://github.com/OSGeo/proj.4/commit/
    //                              bc7453d1a75aab05bdff2c51ed78c908e3efa3cd
    for( int i = 0; i < nCount; i++ )
    {
        if( CPLIsNan(x[i]) || CPLIsNan(y[i]) )
        {
            x[i] = HUGE_VAL;
            y[i] = HUGE_VAL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Potentially transform to radians.                               */
/* -------------------------------------------------------------------- */
    if( bSourceLatLong )
    {
        if( bSourceWrap )
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

        for( int i = 0; i < nCount; i++ )
        {
            if( x[i] != HUGE_VAL )
            {
                x[i] *= dfSourceToRadians;
                y[i] *= dfSourceToRadians;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Optimized transform from WebMercator to WGS84                   */
/* -------------------------------------------------------------------- */
    bool bTransformDone = false;
    if( bWebMercatorToWGS84 )
    {
        constexpr double REVERSE_SPHERE_RADIUS = 1.0 / 6378137.0;

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
                    else if( bCheckWithInvertProj )
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
                    else if( bCheckWithInvertProj )
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
                 // Optimization for the case where we are provided a whole line
                 // of same northing.
                if( i > 0 && y[i] == y0 )
                    y[i] = y[0];
                else
                    y[i] =
                        M_PI / 2.0 -
                        2.0 * atan(exp(-y[i] * REVERSE_SPHERE_RADIUS));
            }
        }

        bTransformDone = true;
    }
    else if( bIdentityTransform )
    {
        bTransformDone = true;
    }

/* -------------------------------------------------------------------- */
/*      Do the transformation (or not...) using PROJ.4.                 */
/* -------------------------------------------------------------------- */
#if PROJ_VERSION == 4
    if( !bTransformDone && pjctx == nullptr )
    {
        // The mutex has already been created.
        CPLAssert(hPROJMutex != nullptr);
        CPLAcquireMutex(hPROJMutex, 1000.0);
    }
#endif

    int err = 0;
    if( bTransformDone )
    {
        // err = 0;
    }
    else if( bCheckWithInvertProj )
    {
        // For some projections, we cannot detect if we are trying to reproject
        // coordinates outside the validity area of the projection. So let's do
        // the reverse reprojection and compare with the source coordinates.
        if( nCount > nMaxCount )
        {
            nMaxCount = nCount;
            padfOriX = static_cast<double*>(
                CPLRealloc(padfOriX, sizeof(double) * nCount));
            padfOriY = static_cast<double*>(
                CPLRealloc(padfOriY, sizeof(double)*nCount));
            padfOriZ = static_cast<double*>(
                CPLRealloc(padfOriZ, sizeof(double)*nCount));
            padfTargetX = static_cast<double*>(
                CPLRealloc(padfTargetX, sizeof(double)*nCount));
            padfTargetY = static_cast<double*>(
                CPLRealloc(padfTargetY, sizeof(double)*nCount));
            padfTargetZ = static_cast<double*>(
                CPLRealloc(padfTargetZ, sizeof(double)*nCount));
        }
        memcpy(padfOriX, x, sizeof(double) * nCount);
        memcpy(padfOriY, y, sizeof(double) * nCount);
        if( z )
        {
            memcpy(padfOriZ, z, sizeof(double)*nCount);
        }

#if PROJ_VERSION == 5
        size_t nRet = proj_trans_generic (m_pj, PJ_FWD,
                                x, sizeof(double), nCount,
                                y, sizeof(double), nCount,
                                z, z ? sizeof(double) : 0, z ? nCount : 0,
                                nullptr, 0, 0);
        err == ( static_cast<int>(nRet) == nCount ) ?
                    0 : proj_context_errno(m_pjctx);
#else
        err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );
        if( err == 0 )
#endif
        {
            memcpy(padfTargetX, x, sizeof(double) * nCount);
            memcpy(padfTargetY, y, sizeof(double) * nCount);
            if( z )
            {
                memcpy(padfTargetZ, z, sizeof(double) * nCount);
            }

#if PROJ_VERSION == 5
            nRet = proj_trans_generic (m_pj, PJ_INV,
                padfTargetX, sizeof(double), nCount,
                padfTargetY, sizeof(double), nCount,
                z ? padfTargetZ : nullptr, z ? sizeof(double) : 0, z ? nCount : 0,
                nullptr, 0, 0);
            err == ( static_cast<int>(nRet) == nCount ) ?
                    0 : proj_context_errno(m_pjctx);
#else
            err = pfn_pj_transform( psPJTarget, psPJSource , nCount, 1,
                                    padfTargetX, padfTargetY,
                                    z ? padfTargetZ : nullptr);
            if( err == 0 )
#endif
            {
                for( int i = 0; i < nCount; i++ )
                {
                    if( x[i] != HUGE_VAL && y[i] != HUGE_VAL &&
                        (fabs(padfTargetX[i] - padfOriX[i]) > dfThreshold ||
                         fabs(padfTargetY[i] - padfOriY[i]) > dfThreshold) )
                    {
                        x[i] = HUGE_VAL;
                        y[i] = HUGE_VAL;
                    }
                }
            }
        }
    }
    else
    {
#if PROJ_VERSION == 5
        size_t nRet = proj_trans_generic (m_pj, PJ_FWD,
                                x, sizeof(double), nCount,
                                y, sizeof(double), nCount,
                                z, z ? sizeof(double) : 0, z ? nCount : 0,
                                nullptr, 0, 0);
        err == ( static_cast<int>(nRet) == nCount ) ?
                    0 : proj_context_errno(m_pjctx);
#else
        err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );
#endif
    }

/* -------------------------------------------------------------------- */
/*      Try to report an error through CPL.  Get proj.4 error string    */
/*      if possible.  Try to avoid reporting thousands of errors.       */
/*      Suppress further error reporting on this OGRProj4CT if we       */
/*      have already reported 20 errors.                                */
/* -------------------------------------------------------------------- */
    if( err != 0 )
    {
        if( pabSuccess )
            memset( pabSuccess, 0, sizeof(int) * nCount );

        if( m_bEmitErrors && ++nErrorCount < 20 )
        {
#if PROJ_VERSION == 4
            if( pjctx != nullptr )
                // pfn_pj_strerrno not yet thread-safe in PROJ 4.8.0.
                CPLAcquireMutex(hPROJMutex, 1000.0);

            const char *pszError = nullptr;
            if( pfn_pj_strerrno != nullptr )
                pszError = pfn_pj_strerrno( err );

            if( pszError == nullptr )
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Reprojection failed, err = %d",
                          err );
            else
                CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );

            if( pjctx != nullptr )
                // pfn_pj_strerrno not yet thread-safe in PROJ 4.8.0.
                CPLReleaseMutex(hPROJMutex);
#else
            const char *pszError = proj_errno_string(err);
            if( pszError == nullptr )
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Reprojection failed, err = %d",
                          err );
            else
                CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );
#endif
        }
        else if( nErrorCount == 20 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Reprojection failed, err = %d, further errors will be "
                      "suppressed on the transform object.",
                      err );
        }
#if PROJ_VERSION == 4
        if( pjctx == nullptr )
            CPLReleaseMutex(hPROJMutex);
#endif
        return FALSE;
    }

#if PROJ_VERSION == 4
    if( !bTransformDone && pjctx == nullptr )
        CPLReleaseMutex(hPROJMutex);
#endif

/* -------------------------------------------------------------------- */
/*      Potentially transform back to degrees.                          */
/* -------------------------------------------------------------------- */
    if( bTargetLatLong )
    {
        for( int i = 0; i < nCount; i++ )
        {
            if( x[i] != HUGE_VAL && y[i] != HUGE_VAL )
            {
                x[i] *= dfTargetFromRadians;
                y[i] *= dfTargetFromRadians;
            }
        }

        if( bTargetWrap )
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
    }

/* -------------------------------------------------------------------- */
/*      Establish error information if pabSuccess provided.             */
/* -------------------------------------------------------------------- */
    if( pabSuccess )
    {
        for( int i = 0; i < nCount; i++ )
        {
            if( x[i] == HUGE_VAL || y[i] == HUGE_VAL )
                pabSuccess[i] = FALSE;
            else
                pabSuccess[i] = TRUE;
        }
    }

    return TRUE;
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
        TransformEx( nCount, x, y, z, pabSuccess );
}
