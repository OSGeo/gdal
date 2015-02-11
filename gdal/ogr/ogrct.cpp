/******************************************************************************
 * $Id$
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

#include "ogr_spatialref.h"
#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

#ifdef PROJ_STATIC
#include "proj_api.h"
#endif

CPL_CVSID("$Id$");

/* ==================================================================== */
/*      PROJ.4 interface stuff.                                         */
/* ==================================================================== */
#ifndef PROJ_STATIC
typedef struct { double u, v; } projUV;

#define projPJ void *
#define projCtx void *
#define RAD_TO_DEG      57.29577951308232
#define DEG_TO_RAD      .0174532925199432958

#else

#if PJ_VERSION < 480
#define projCtx void *
#endif

#endif

static void *hPROJMutex = NULL;

static projPJ       (*pfn_pj_init_plus)(const char *) = NULL;
static projPJ       (*pfn_pj_init)(int, char**) = NULL;
static void     (*pfn_pj_free)(projPJ) = NULL;
static int      (*pfn_pj_transform)(projPJ, projPJ, long, int, 
                                    double *, double *, double * ) = NULL;
static int         *(*pfn_pj_get_errno_ref)(void) = NULL;
static char        *(*pfn_pj_strerrno)(int) = NULL;
static char        *(*pfn_pj_get_def)(projPJ,int) = NULL;
static void         (*pfn_pj_dalloc)(void *) = NULL;

static projPJ (*pfn_pj_init_plus_ctx)( projCtx, const char * ) = NULL;
static int (*pfn_pj_ctx_get_errno)( projCtx ) = NULL;
static projCtx (*pfn_pj_ctx_alloc)(void) = NULL;
static void    (*pfn_pj_ctx_free)( projCtx ) = NULL;

#if (defined(WIN32) || defined(WIN32CE)) && !defined(__MINGW32__)
#  define LIBNAME      "proj.dll"
#elif defined(__MINGW32__)
// XXX: If PROJ.4 library was properly built using libtool in Cygwin or MinGW
// environments it has the interface version number embedded in the file name
// (it is CURRENT-AGE number). If DLL came somewhere else (e.g. from MSVC
// build) it can be named either way, so use PROJSO environment variable to
// specify the right library name. By default assume that in Cygwin/MinGW all
// components were buit in the same way.
#  define LIBNAME      "libproj-0.dll"
#elif defined(__CYGWIN__)
#  define LIBNAME      "cygproj-0.dll"
#elif defined(__APPLE__)
#  define LIBNAME      "libproj.dylib"
#else
#  define LIBNAME      "libproj.so"
#endif

/************************************************************************/
/*                         OCTCleanupProjMutex()                        */
/************************************************************************/

void OCTCleanupProjMutex()
{
    if( hPROJMutex != NULL )
    {
        CPLDestroyMutex(hPROJMutex);
        hPROJMutex = NULL;
    }
}

/************************************************************************/
/*                              OGRProj4CT                              */
/************************************************************************/

class OGRProj4CT : public OGRCoordinateTransformation
{
    OGRSpatialReference *poSRSSource;
    void        *psPJSource;
    int         bSourceLatLong;
    double      dfSourceToRadians;
    int         bSourceWrap;
    double      dfSourceWrapLong;
    

    OGRSpatialReference *poSRSTarget;
    void        *psPJTarget;
    int         bTargetLatLong;
    double      dfTargetFromRadians;
    int         bTargetWrap;
    double      dfTargetWrapLong;

    int         bIdentityTransform;

    int         nErrorCount;
    
    int         bCheckWithInvertProj;
    double      dfThreshold;
    
    projCtx     pjctx;

    int         InitializeNoLock( OGRSpatialReference *poSource, 
                                  OGRSpatialReference *poTarget );

    int         nMaxCount;
    double     *padfOriX;
    double     *padfOriY;
    double     *padfOriZ;
    double     *padfTargetX;
    double     *padfTargetY;
    double     *padfTargetZ;

public:
                OGRProj4CT();
    virtual     ~OGRProj4CT();

    int         Initialize( OGRSpatialReference *poSource, 
                            OGRSpatialReference *poTarget );

    virtual OGRSpatialReference *GetSourceCS();
    virtual OGRSpatialReference *GetTargetCS();
    virtual int Transform( int nCount, 
                           double *x, double *y, double *z = NULL );
    virtual int TransformEx( int nCount, 
                             double *x, double *y, double *z = NULL,
                             int *panSuccess = NULL );

};

/************************************************************************/
/*                        GetProjLibraryName()                          */
/************************************************************************/

static const char* GetProjLibraryName()
{
    const char *pszLibName = LIBNAME;
#if !defined(WIN32CE)
    if( CPLGetConfigOption("PROJSO",NULL) != NULL )
        pszLibName = CPLGetConfigOption("PROJSO",NULL);
#endif
    return pszLibName;
}

/************************************************************************/
/*                          LoadProjLibrary()                           */
/************************************************************************/

static int LoadProjLibrary_unlocked()

{
    static int  bTriedToLoad = FALSE;
    const char *pszLibName;
    
    if( bTriedToLoad )
        return( pfn_pj_transform != NULL );

    bTriedToLoad = TRUE;

    pszLibName = GetProjLibraryName();

#ifdef PROJ_STATIC
    pfn_pj_init = pj_init;
    pfn_pj_init_plus = pj_init_plus;
    pfn_pj_free = pj_free;
    pfn_pj_transform = pj_transform;
    pfn_pj_get_errno_ref = (int *(*)(void)) pj_get_errno_ref;
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

    pfn_pj_init = (projPJ (*)(int, char**)) CPLGetSymbol( pszLibName,
                                                       "pj_init" );
    CPLPopErrorHandler();
    
    if( pfn_pj_init == NULL )
       return( FALSE );

    pfn_pj_init_plus = (projPJ (*)(const char *)) 
        CPLGetSymbol( pszLibName, "pj_init_plus" );
    pfn_pj_free = (void (*)(projPJ)) 
        CPLGetSymbol( pszLibName, "pj_free" );
    pfn_pj_transform = (int (*)(projPJ,projPJ,long,int,double*,
                                double*,double*))
                        CPLGetSymbol( pszLibName, "pj_transform" );
    pfn_pj_get_errno_ref = (int *(*)(void))
        CPLGetSymbol( pszLibName, "pj_get_errno_ref" );
    pfn_pj_strerrno = (char *(*)(int))
        CPLGetSymbol( pszLibName, "pj_strerrno" );

    CPLPushErrorHandler( CPLQuietErrorHandler );
    pfn_pj_get_def = (char *(*)(projPJ,int))
        CPLGetSymbol( pszLibName, "pj_get_def" );
    pfn_pj_dalloc = (void (*)(void*))
        CPLGetSymbol( pszLibName, "pj_dalloc" );

    /* PROJ 4.8.0 symbols */
    pfn_pj_ctx_alloc = (projCtx (*)( void ))
        CPLGetSymbol( pszLibName, "pj_ctx_alloc" );
    pfn_pj_ctx_free = (void (*)( projCtx ))
        CPLGetSymbol( pszLibName, "pj_ctx_free" );
    pfn_pj_init_plus_ctx = (projPJ (*)( projCtx, const char * ))
        CPLGetSymbol( pszLibName, "pj_init_plus_ctx" );
    pfn_pj_ctx_get_errno = (int (*)( projCtx ))
        CPLGetSymbol( pszLibName, "pj_ctx_get_errno" );

    CPLPopErrorHandler();
    CPLErrorReset();
#endif

    if (pfn_pj_ctx_alloc != NULL &&
        pfn_pj_ctx_free != NULL &&
        pfn_pj_init_plus_ctx != NULL &&
        pfn_pj_ctx_get_errno != NULL &&
        CSLTestBoolean(CPLGetConfigOption("USE_PROJ_480_FEATURES", "YES")))
    {
        CPLDebug("OGRCT", "PROJ >= 4.8.0 features enabled");
    }
    else
    {
        pfn_pj_ctx_alloc = NULL;
        pfn_pj_ctx_free = NULL;
        pfn_pj_init_plus_ctx = NULL;
        pfn_pj_ctx_get_errno = NULL;
    }

    if( pfn_pj_transform == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to load %s, but couldn't find pj_transform.\n"
                  "Please upgrade to PROJ 4.1.2 or later.", 
                  pszLibName );

        return FALSE;
    }

    return( TRUE );
}

static int LoadProjLibrary()

{
    CPLMutexHolderD( &hPROJMutex );
    return LoadProjLibrary_unlocked();
}

/************************************************************************/
/*                         OCTProj4Normalize()                          */
/*                                                                      */
/*      This function is really just here since we already have all     */
/*      the code to load libproj.so.  It is intended to "normalize"     */
/*      a proj.4 definition, expanding +init= definitions and so        */
/*      forth as possible.                                              */
/************************************************************************/

char *OCTProj4Normalize( const char *pszProj4Src )

{
    char        *pszNewProj4Def, *pszCopy;
    projPJ      psPJSource = NULL;

    CPLMutexHolderD( &hPROJMutex );

    if( !LoadProjLibrary_unlocked() || pfn_pj_dalloc == NULL || pfn_pj_get_def == NULL )
        return CPLStrdup( pszProj4Src );

    CPLLocaleC  oLocaleEnforcer;

    psPJSource = pfn_pj_init_plus( pszProj4Src );

    if( psPJSource == NULL )
        return CPLStrdup( pszProj4Src );

    pszNewProj4Def = pfn_pj_get_def( psPJSource, 0 );

    pfn_pj_free( psPJSource );

    if( pszNewProj4Def == NULL )
        return CPLStrdup( pszProj4Src );

    pszCopy = CPLStrdup( pszNewProj4Def );
    pfn_pj_dalloc( pszNewProj4Def );

    return pszCopy;
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
    delete (OGRCoordinateTransformation *) hCT;
}

/************************************************************************/
/*                             DestroyCT()                              */
/************************************************************************/

/**
 * \brief OGRCoordinateTransformation destructor. 
 *
 * This function is the same as OGRCoordinateTransformation::~OGRCoordinateTransformation()
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
 
void OGRCoordinateTransformation::DestroyCT(OGRCoordinateTransformation* poCT)
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
    OGRProj4CT  *poCT;

    if( pfn_pj_init == NULL && !LoadProjLibrary() )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unable to load PROJ.4 library (%s), creation of\n"
                  "OGRCoordinateTransformation failed.",
                  GetProjLibraryName() );
        return NULL;
    }

    poCT = new OGRProj4CT();
    
    if( !poCT->Initialize( poSource, poTarget ) )
    {
        delete poCT;
        return NULL;
    }
    else
    {
        return poCT;
    }
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
    return (OGRCoordinateTransformationH) 
        OGRCreateCoordinateTransformation( 
            (OGRSpatialReference *) hSourceSRS,
            (OGRSpatialReference *) hTargetSRS );
}

/************************************************************************/
/*                             OGRProj4CT()                             */
/************************************************************************/

OGRProj4CT::OGRProj4CT()

{
    poSRSSource = NULL;
    poSRSTarget = NULL;
    psPJSource = NULL;
    psPJTarget = NULL;
    
    bIdentityTransform = FALSE;
    nErrorCount = 0;
    
    bCheckWithInvertProj = FALSE;
    dfThreshold = 0;

    nMaxCount = 0;
    padfOriX = NULL;
    padfOriY = NULL;
    padfOriZ = NULL;
    padfTargetX = NULL;
    padfTargetY = NULL;
    padfTargetZ = NULL;

    if (pfn_pj_ctx_alloc != NULL)
        pjctx = pfn_pj_ctx_alloc();
    else
        pjctx = NULL;
}

/************************************************************************/
/*                            ~OGRProj4CT()                             */
/************************************************************************/

OGRProj4CT::~OGRProj4CT()

{
    if( poSRSSource != NULL )
    {
        if( poSRSSource->Dereference() <= 0 )
            delete poSRSSource;
    }

    if( poSRSTarget != NULL )
    {
        if( poSRSTarget->Dereference() <= 0 )
            delete poSRSTarget;
    }

    if (pjctx != NULL)
    {
        pfn_pj_ctx_free(pjctx);

        if( psPJSource != NULL )
            pfn_pj_free( psPJSource );

        if( psPJTarget != NULL )
            pfn_pj_free( psPJTarget );
    }
    else
    {
        CPLMutexHolderD( &hPROJMutex );

        if( psPJSource != NULL )
            pfn_pj_free( psPJSource );

        if( psPJTarget != NULL )
            pfn_pj_free( psPJTarget );
    }

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
    CPLLocaleC  oLocaleEnforcer;
    if (pjctx != NULL)
    {
        return InitializeNoLock(poSourceIn, poTargetIn);
    }

    CPLMutexHolderD( &hPROJMutex );
    return InitializeNoLock(poSourceIn, poTargetIn);
}

/************************************************************************/
/*                         InitializeNoLock()                           */
/************************************************************************/

int OGRProj4CT::InitializeNoLock( OGRSpatialReference * poSourceIn, 
                                  OGRSpatialReference * poTargetIn )

{
    if( poSourceIn == NULL || poTargetIn == NULL )
        return FALSE;

    poSRSSource = poSourceIn->Clone();
    poSRSTarget = poTargetIn->Clone();

    bSourceLatLong = poSRSSource->IsGeographic();
    bTargetLatLong = poSRSTarget->IsGeographic();

/* -------------------------------------------------------------------- */
/*      Setup source and target translations to radians for lat/long    */
/*      systems.                                                        */
/* -------------------------------------------------------------------- */
    dfSourceToRadians = DEG_TO_RAD;
    bSourceWrap = FALSE;
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
    bTargetWrap = FALSE;
    dfTargetWrapLong = 0.0;

    if( bTargetLatLong )
    {
        OGR_SRSNode *poUNITS = poSRSTarget->GetAttrNode( "GEOGCS|UNIT" );
        if( poUNITS && poUNITS->GetChildCount() >= 2 )
        {
            double dfTargetToRadians = CPLAtof(poUNITS->GetChild(1)->GetValue());
            if( dfTargetToRadians != 0.0 )
                dfTargetFromRadians = 1 / dfTargetToRadians;
        }
    }

/* -------------------------------------------------------------------- */
/*      Preliminary logic to setup wrapping.                            */
/* -------------------------------------------------------------------- */
    const char *pszCENTER_LONG;

    if( CPLGetConfigOption( "CENTER_LONG", NULL ) != NULL )
    {
        bSourceWrap = bTargetWrap = TRUE;
        dfSourceWrapLong = dfTargetWrapLong = 
            CPLAtof(CPLGetConfigOption( "CENTER_LONG", "" ));
        CPLDebug( "OGRCT", "Wrap at %g.", dfSourceWrapLong );
    }

    pszCENTER_LONG = poSRSSource->GetExtension( "GEOGCS", "CENTER_LONG" );
    if( pszCENTER_LONG != NULL )
    {
        dfSourceWrapLong = CPLAtof(pszCENTER_LONG);
        bSourceWrap = TRUE;
        CPLDebug( "OGRCT", "Wrap source at %g.", dfSourceWrapLong );
    }

    pszCENTER_LONG = poSRSTarget->GetExtension( "GEOGCS", "CENTER_LONG" );
    if( pszCENTER_LONG != NULL )
    {
        dfTargetWrapLong = CPLAtof(pszCENTER_LONG);
        bTargetWrap = TRUE;
        CPLDebug( "OGRCT", "Wrap target at %g.", dfTargetWrapLong );
    }
    
    bCheckWithInvertProj = CSLTestBoolean(CPLGetConfigOption( "CHECK_WITH_INVERT_PROJ", "NO" ));
    
    /* The threshold is rather experimental... Works well with the cases of ticket #2305 */
    if (bSourceLatLong)
        dfThreshold = CPLAtof(CPLGetConfigOption( "THRESHOLD", ".1" ));
    else
        /* 1 works well for most projections, except for +proj=aeqd that requires */
        /* a tolerance of 10000 */
        dfThreshold = CPLAtof(CPLGetConfigOption( "THRESHOLD", "10000" ));

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for source if projection.               */
/* -------------------------------------------------------------------- */
    // OGRThreadSafety: The following variable is not a thread safety issue 
    // since the only issue is incrementing while accessing which at worse 
    // means debug output could be one "increment" late. 
    static int   nDebugReportCount = 0;

    char        *pszSrcProj4Defn = NULL;

    if( poSRSSource->exportToProj4( &pszSrcProj4Defn ) != OGRERR_NONE )
    {
        CPLFree( pszSrcProj4Defn );
        return FALSE;
    }

    if( strlen(pszSrcProj4Defn) == 0 )
    {
        CPLFree( pszSrcProj4Defn );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No PROJ.4 translation for source SRS, coordinate\n"
                  "transformation initialization has failed." );
        return FALSE;
    }

    if (pjctx)
        psPJSource = pfn_pj_init_plus_ctx( pjctx, pszSrcProj4Defn );
    else
        psPJSource = pfn_pj_init_plus( pszSrcProj4Defn );
    
    if( psPJSource == NULL )
    {
        if( pjctx != NULL)
        {
            int pj_errno = pfn_pj_ctx_get_errno(pjctx);

            /* pfn_pj_strerrno not yet thread-safe in PROJ 4.8.0 */
            CPLMutexHolderD(&hPROJMutex);
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "Failed to initialize PROJ.4 with `%s'.\n%s", 
                      pszSrcProj4Defn, pfn_pj_strerrno(pj_errno) );
        }
        else if( pfn_pj_get_errno_ref != NULL
            && pfn_pj_strerrno != NULL )
        {
            int *p_pj_errno = pfn_pj_get_errno_ref();

            CPLError( CE_Failure, CPLE_NotSupported, 
                      "Failed to initialize PROJ.4 with `%s'.\n%s", 
                      pszSrcProj4Defn, pfn_pj_strerrno(*p_pj_errno) );
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "Failed to initialize PROJ.4 with `%s'.\n", 
                      pszSrcProj4Defn );
        }
    }
    
    if( nDebugReportCount < 10 )
        CPLDebug( "OGRCT", "Source: %s", pszSrcProj4Defn );

    if( psPJSource == NULL )
    {
        CPLFree( pszSrcProj4Defn );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for target if projection.               */
/* -------------------------------------------------------------------- */

    char        *pszDstProj4Defn = NULL;

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
                  "No PROJ.4 translation for destination SRS, coordinate\n"
                  "transformation initialization has failed." );
        return FALSE;
    }

    if (pjctx)
        psPJTarget = pfn_pj_init_plus_ctx( pjctx, pszDstProj4Defn );
    else
        psPJTarget = pfn_pj_init_plus( pszDstProj4Defn );
    
    if( psPJTarget == NULL )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Failed to initialize PROJ.4 with `%s'.", 
                  pszDstProj4Defn );
    
    if( nDebugReportCount < 10 )
    {
        CPLDebug( "OGRCT", "Target: %s", pszDstProj4Defn );
        nDebugReportCount++;
    }

    if( psPJTarget == NULL )
    {
        CPLFree( pszSrcProj4Defn );
        CPLFree( pszDstProj4Defn );
        return FALSE;
    }

    /* Determine if we really have a transformation to do */
    bIdentityTransform = (strcmp(pszSrcProj4Defn, pszDstProj4Defn) == 0);

    /* In case of identity transform, under the following conditions, */
    /* we can also avoid transforming from deegrees <--> radians. */
    if( bIdentityTransform && bSourceLatLong && !bSourceWrap &&
        bTargetLatLong && !bTargetWrap &&
        abs(dfSourceToRadians * dfTargetFromRadians - 1.0) < 1e-10 )
    {
        /*bSourceLatLong = FALSE;
        bTargetLatLong = FALSE;*/
    }

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
    int *pabSuccess = (int *) CPLMalloc(sizeof(int) * nCount );
    int bOverallSuccess, i;

    bOverallSuccess = TransformEx( nCount, x, y, z, pabSuccess );

    for( i = 0; i < nCount; i++ )
    {
        if( !pabSuccess[i] )
        {
            bOverallSuccess = FALSE;
            break;
        }
    }

    CPLFree( pabSuccess );

    return bOverallSuccess;
}

/************************************************************************/
/*                            OCTTransform()                            */
/************************************************************************/

int CPL_STDCALL OCTTransform( OGRCoordinateTransformationH hTransform,
                              int nCount, double *x, double *y, double *z )

{
    VALIDATE_POINTER1( hTransform, "OCTTransform", FALSE );

    return ((OGRCoordinateTransformation*) hTransform)->
        Transform( nCount, x, y,z );
}

/************************************************************************/
/*                            TransformEx()                             */
/************************************************************************/

int OGRProj4CT::TransformEx( int nCount, double *x, double *y, double *z,
                             int *pabSuccess )

{
    int   err, i;

/* -------------------------------------------------------------------- */
/*      Potentially transform to radians.                               */
/* -------------------------------------------------------------------- */
    if( bSourceLatLong )
    {
        if( bSourceWrap )
        {
            for( i = 0; i < nCount; i++ )
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

        for( i = 0; i < nCount; i++ )
        {
            if( x[i] != HUGE_VAL )
            {
                x[i] *= dfSourceToRadians;
                y[i] *= dfSourceToRadians;
            }
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Do the transformation using PROJ.4.                             */
/* -------------------------------------------------------------------- */
    if( !bIdentityTransform && pjctx == NULL )
    {
        /* The mutex has already been created */
        CPLAssert(hPROJMutex != NULL);
        CPLAcquireMutex(hPROJMutex, 1000.0);
    }

    if( bIdentityTransform )
        err = 0;
    else if (bCheckWithInvertProj)
    {
        /* For some projections, we cannot detect if we are trying to reproject */
        /* coordinates outside the validity area of the projection. So let's do */
        /* the reverse reprojection and compare with the source coordinates */
        if (nCount > nMaxCount)
        {
            nMaxCount = nCount;
            padfOriX = (double*) CPLRealloc(padfOriX, sizeof(double)*nCount);
            padfOriY = (double*) CPLRealloc(padfOriY, sizeof(double)*nCount);
            padfOriZ = (double*) CPLRealloc(padfOriZ, sizeof(double)*nCount);
            padfTargetX = (double*) CPLRealloc(padfTargetX, sizeof(double)*nCount);
            padfTargetY = (double*) CPLRealloc(padfTargetY, sizeof(double)*nCount);
            padfTargetZ = (double*) CPLRealloc(padfTargetZ, sizeof(double)*nCount);
        }
        memcpy(padfOriX, x, sizeof(double)*nCount);
        memcpy(padfOriY, y, sizeof(double)*nCount);
        if (z)
        {
            memcpy(padfOriZ, z, sizeof(double)*nCount);
        }
        err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );
        if (err == 0)
        {
            memcpy(padfTargetX, x, sizeof(double)*nCount);
            memcpy(padfTargetY, y, sizeof(double)*nCount);
            if (z)
            {
                memcpy(padfTargetZ, z, sizeof(double)*nCount);
            }
            
            err = pfn_pj_transform( psPJTarget, psPJSource , nCount, 1,
                                    padfTargetX, padfTargetY, (z) ? padfTargetZ : NULL);
            if (err == 0)
            {
                for( i = 0; i < nCount; i++ )
                {
                    if ( x[i] != HUGE_VAL && y[i] != HUGE_VAL &&
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
        err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );
    }

/* -------------------------------------------------------------------- */
/*      Try to report an error through CPL.  Get proj.4 error string    */
/*      if possible.  Try to avoid reporting thousands of error         */
/*      ... suppress further error reporting on this OGRProj4CT if we   */
/*      have already reported 20 errors.                                */
/* -------------------------------------------------------------------- */
    if( err != 0 )
    {
        if( pabSuccess )
            memset( pabSuccess, 0, sizeof(int) * nCount );

        if( ++nErrorCount < 20 )
        {
            if (pjctx != NULL)
                /* pfn_pj_strerrno not yet thread-safe in PROJ 4.8.0 */
                CPLAcquireMutex(hPROJMutex, 1000.0);

            const char *pszError = NULL;
            if( pfn_pj_strerrno != NULL )
                pszError = pfn_pj_strerrno( err );
            
            if( pszError == NULL )
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Reprojection failed, err = %d", 
                          err );
            else
                CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );

            if (pjctx != NULL)
                /* pfn_pj_strerrno not yet thread-safe in PROJ 4.8.0 */
                CPLReleaseMutex(hPROJMutex);
        }
        else if( nErrorCount == 20 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Reprojection failed, err = %d, further errors will be suppressed on the transform object.", 
                      err );
        }

        if (pjctx == NULL)
            CPLReleaseMutex(hPROJMutex);
        return FALSE;
    }

    if( !bIdentityTransform && pjctx == NULL )
        CPLReleaseMutex(hPROJMutex);

/* -------------------------------------------------------------------- */
/*      Potentially transform back to degrees.                          */
/* -------------------------------------------------------------------- */
    if( bTargetLatLong )
    {
        for( i = 0; i < nCount; i++ )
        {
            if( x[i] != HUGE_VAL && y[i] != HUGE_VAL )
            {
                x[i] *= dfTargetFromRadians;
                y[i] *= dfTargetFromRadians;
            }
        }

        if( bTargetWrap )
        {
            for( i = 0; i < nCount; i++ )
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
        for( i = 0; i < nCount; i++ )
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

int CPL_STDCALL OCTTransformEx( OGRCoordinateTransformationH hTransform,
                                int nCount, double *x, double *y, double *z,
                                int *pabSuccess )

{
    VALIDATE_POINTER1( hTransform, "OCTTransformEx", FALSE );

    return ((OGRCoordinateTransformation*) hTransform)->
        TransformEx( nCount, x, y, z, pabSuccess );
}

