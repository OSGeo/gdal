/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSCoordinateTransformation class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#define RAD_TO_DEG      57.29577951308232
#define DEG_TO_RAD      .0174532925199432958

#endif

static void *hPROJMutex = NULL;

static projPJ       (*pfn_pj_init_plus)(const char *) = NULL;
static projPJ       (*pfn_pj_init)(int, char**) = NULL;
static projUV       (*pfn_pj_fwd)(projUV, projPJ) = NULL;
static projUV       (*pfn_pj_inv)(projUV, projPJ) = NULL;
static void     (*pfn_pj_free)(projPJ) = NULL;
static int      (*pfn_pj_transform)(projPJ, projPJ, long, int, 
                                    double *, double *, double * ) = NULL;
static int         *(*pfn_pj_get_errno_ref)(void) = NULL;
static char        *(*pfn_pj_strerrno)(int) = NULL;
static char        *(*pfn_pj_get_def)(projPJ,int) = NULL;
static void         (*pfn_pj_dalloc)(void *) = NULL;

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
/*                              OGRProj4CT                              */
/************************************************************************/

class OGRProj4CT : public OGRCoordinateTransformation
{
    OGRSpatialReference *poSRSSource;
    void        *psPJSource;
    int         bSourceLatLong;
    double      dfSourceToRadians;
    double      dfSourceFromRadians;
    int         bSourceWrap;
    double      dfSourceWrapLong;
    

    OGRSpatialReference *poSRSTarget;
    void        *psPJTarget;
    int         bTargetLatLong;
    double      dfTargetToRadians;
    double      dfTargetFromRadians;
    int         bTargetWrap;
    double      dfTargetWrapLong;

    int         nErrorCount;
    
    int         bCheckWithInvertProj;
    double      dfThreshold;

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

static int LoadProjLibrary()

{
    CPLMutexHolderD( &hPROJMutex );
    static int  bTriedToLoad = FALSE;
    const char *pszLibName;
    
    if( bTriedToLoad )
        return( pfn_pj_transform != NULL );

    bTriedToLoad = TRUE;

    pszLibName = GetProjLibraryName();

#ifdef PROJ_STATIC
    pfn_pj_init = pj_init;
    pfn_pj_init_plus = pj_init_plus;
    pfn_pj_fwd = pj_fwd;
    pfn_pj_inv = pj_inv;
    pfn_pj_free = pj_free;
    pfn_pj_transform = pj_transform;
    pfn_pj_get_errno_ref = (int *(*)(void)) pj_get_errno_ref;
    pfn_pj_strerrno = pj_strerrno;
    pfn_pj_dalloc = pj_dalloc;
#if PJ_VERSION >= 446
    pfn_pj_get_def = pj_get_def;
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
    pfn_pj_fwd = (projUV (*)(projUV,projPJ)) 
        CPLGetSymbol( pszLibName, "pj_fwd" );
    pfn_pj_inv = (projUV (*)(projUV,projPJ)) 
        CPLGetSymbol( pszLibName, "pj_inv" );
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
    CPLPopErrorHandler();

#endif

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

    if( !LoadProjLibrary() || pfn_pj_dalloc == NULL || pfn_pj_get_def == NULL )
        return CPLStrdup( pszProj4Src );

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

    if( !LoadProjLibrary() )
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
    
    nErrorCount = 0;
    
    bCheckWithInvertProj = FALSE;
    dfThreshold = 0;
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

    CPLMutexHolderD( &hPROJMutex );

    if( psPJSource != NULL )
        pfn_pj_free( psPJSource );

    if( psPJTarget != NULL )
        pfn_pj_free( psPJTarget );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRProj4CT::Initialize( OGRSpatialReference * poSourceIn, 
                            OGRSpatialReference * poTargetIn )

{
    CPLMutexHolderD( &hPROJMutex );

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
    dfSourceFromRadians = RAD_TO_DEG;
    bSourceWrap = FALSE;
    dfSourceWrapLong = 0.0;

    if( bSourceLatLong )
    {
        OGR_SRSNode *poUNITS = poSRSSource->GetAttrNode( "GEOGCS|UNIT" );
        if( poUNITS && poUNITS->GetChildCount() >= 2 )
        {
            dfSourceToRadians = atof(poUNITS->GetChild(1)->GetValue());
            if( dfSourceToRadians == 0.0 )
                dfSourceToRadians = DEG_TO_RAD;
            else
                dfSourceFromRadians = 1 / dfSourceToRadians;
        }
    }

    dfTargetToRadians = DEG_TO_RAD;
    dfTargetFromRadians = RAD_TO_DEG;
    bTargetWrap = FALSE;
    dfTargetWrapLong = 0.0;

    if( bTargetLatLong )
    {
        OGR_SRSNode *poUNITS = poSRSTarget->GetAttrNode( "GEOGCS|UNIT" );
        if( poUNITS && poUNITS->GetChildCount() >= 2 )
        {
            dfTargetToRadians = atof(poUNITS->GetChild(1)->GetValue());
            if( dfTargetToRadians == 0.0 )
                dfTargetToRadians = DEG_TO_RAD;
            else
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
            atof(CPLGetConfigOption( "CENTER_LONG", "" ));
        CPLDebug( "OGRCT", "Wrap at %g.", dfSourceWrapLong );
    }

    pszCENTER_LONG = poSRSSource->GetExtension( "GEOGCS", "CENTER_LONG" );
    if( pszCENTER_LONG != NULL )
    {
        dfSourceWrapLong = atof(pszCENTER_LONG);
        bSourceWrap = TRUE;
        CPLDebug( "OGRCT", "Wrap source at %g.", dfSourceWrapLong );
    }

    pszCENTER_LONG = poSRSTarget->GetExtension( "GEOGCS", "CENTER_LONG" );
    if( pszCENTER_LONG != NULL )
    {
        dfTargetWrapLong = atof(pszCENTER_LONG);
        bTargetWrap = TRUE;
        CPLDebug( "OGRCT", "Wrap target at %g.", dfTargetWrapLong );
    }
    
    bCheckWithInvertProj = CSLTestBoolean(CPLGetConfigOption( "CHECK_WITH_INVERT_PROJ", "NO" ));
    
    /* The threshold is rather experimental... Works well with the cases of ticket #2305 */
    if (bSourceLatLong)
        dfThreshold = atof(CPLGetConfigOption( "THRESHOLD", ".1" ));
    else
        /* 1 works well for most projections, except for +proj=aeqd that requires */
        /* a tolerance of 10000 */
        dfThreshold = atof(CPLGetConfigOption( "THRESHOLD", "10000" ));

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for source if projection.               */
/* -------------------------------------------------------------------- */
    // OGRThreadSafety: The following variable is not a thread safety issue 
    // since the only issue is incrementing while accessing which at worse 
    // means debug output could be one "increment" late. 
    static int   nDebugReportCount = 0;

    char        *pszProj4Defn = NULL;

    if( poSRSSource->exportToProj4( &pszProj4Defn ) != OGRERR_NONE )
    {
        CPLFree( pszProj4Defn );
        return FALSE;
    }

    if( strlen(pszProj4Defn) == 0 )
    {
        CPLFree( pszProj4Defn );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No PROJ.4 translation for source SRS, coordinate\n"
                  "transformation initialization has failed." );
        return FALSE;
    }

    psPJSource = pfn_pj_init_plus( pszProj4Defn );
    
    if( psPJSource == NULL )
    {
        if( pfn_pj_get_errno_ref != NULL
            && pfn_pj_strerrno != NULL )
        {
            int *p_pj_errno = pfn_pj_get_errno_ref();

            CPLError( CE_Failure, CPLE_NotSupported, 
                      "Failed to initialize PROJ.4 with `%s'.\n%s", 
                      pszProj4Defn, pfn_pj_strerrno(*p_pj_errno) );
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "Failed to initialize PROJ.4 with `%s'.\n", 
                      pszProj4Defn );
        }
    }
    
    if( nDebugReportCount < 10 )
        CPLDebug( "OGRCT", "Source: %s", pszProj4Defn );
    
    CPLFree( pszProj4Defn );

    if( psPJSource == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for target if projection.               */
/* -------------------------------------------------------------------- */
    pszProj4Defn = NULL;

    if( poSRSTarget->exportToProj4( &pszProj4Defn ) != OGRERR_NONE )
    {
        CPLFree( pszProj4Defn );
        return FALSE;
    }

    if( strlen(pszProj4Defn) == 0 )
    {
        CPLFree( pszProj4Defn );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No PROJ.4 translation for destination SRS, coordinate\n"
                  "transformation initialization has failed." );
        return FALSE;
    }

    psPJTarget = pfn_pj_init_plus( pszProj4Defn );
    
    if( psPJTarget == NULL )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Failed to initialize PROJ.4 with `%s'.", 
                  pszProj4Defn );
    
    if( nDebugReportCount < 10 )
    {
        CPLDebug( "OGRCT", "Target: %s", pszProj4Defn );
        nDebugReportCount++;
    }

    CPLFree( pszProj4Defn );
    
    if( psPJTarget == NULL )
        return FALSE;

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
    CPLMutexHolderD( &hPROJMutex );
        
    if (bCheckWithInvertProj)
    {
        /* For some projections, we cannot detect if we are trying to reproject */
        /* coordinates outside the validity area of the projection. So let's do */
        /* the reverse reprojection and compare with the source coordinates */
        
        double *ori_x = NULL;
        double *ori_y = NULL;
        double *ori_z = NULL;
        ori_x = (double*)CPLMalloc(sizeof(double)*nCount);
        memcpy(ori_x, x, sizeof(double)*nCount);
        ori_y = (double*)CPLMalloc(sizeof(double)*nCount);
        memcpy(ori_y, y, sizeof(double)*nCount);
        if (z)
        {
            ori_z = (double*)CPLMalloc(sizeof(double)*nCount);
            memcpy(ori_z, z, sizeof(double)*nCount);
        }
        err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );
        if (err == 0)
        {
            double* target_x = (double*)CPLMalloc(sizeof(double)*nCount);
            double* target_y = (double*)CPLMalloc(sizeof(double)*nCount);
            double* target_z = NULL;
            memcpy(target_x, x, sizeof(double)*nCount);
            memcpy(target_y, y, sizeof(double)*nCount);
            if (z)
            {
                target_z = (double*)CPLMalloc(sizeof(double)*nCount);
                memcpy(target_z, z, sizeof(double)*nCount);
            }
            
            err = pfn_pj_transform( psPJTarget, psPJSource , nCount, 1,
                                    target_x, target_y, target_z );
            if (err == 0)
            {
                for( i = 0; i < nCount; i++ )
                {
                    if ( x[i] != HUGE_VAL && y[i] != HUGE_VAL &&
                        (fabs(target_x[i] - ori_x[i]) > dfThreshold ||
                         fabs(target_y[i] - ori_y[i]) > dfThreshold) )
                    {
                        x[i] = HUGE_VAL;
                        y[i] = HUGE_VAL;
                    }
                }
            }
            
            CPLFree(target_x);
            CPLFree(target_y);
            CPLFree(target_z);
        }
        CPLFree(ori_x);
        CPLFree(ori_y);
        CPLFree(ori_z);
    }
    else
    {
        err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );
    }

/* -------------------------------------------------------------------- */
/*      Try to report an error through CPL.  Get proj.4 error string    */
/*      if possible.  Try to avoid reporting thousands of error         */
/*      ... supress further error reporting on this OGRProj4CT if we    */
/*      have already reported 20 errors.                                */
/* -------------------------------------------------------------------- */
    if( err != 0 )
    {
        if( pabSuccess )
            memset( pabSuccess, 0, sizeof(int) * nCount );

        if( ++nErrorCount < 20 )
        {
            const char *pszError = NULL;
            if( pfn_pj_strerrno != NULL )
                pszError = pfn_pj_strerrno( err );
            
            if( pszError == NULL )
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Reprojection failed, err = %d", 
                          err );
            else
                CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );
        }
        else if( nErrorCount == 20 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Reprojection failed, err = %d, further errors will be supressed on the transform object.", 
                      err );
        }

        return FALSE;
    }

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
    return ((OGRCoordinateTransformation*) hTransform)->
        TransformEx( nCount, x, y, z, pabSuccess );
}

