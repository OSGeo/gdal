/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSCoordinateTransformation class.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.22  2003/11/10 17:08:31  warmerda
 * dont delete OGRSpatialReferences if they are still referenced
 *
 * Revision 1.21  2003/06/27 19:02:50  warmerda
 * changed to use pj_init_plus instead of CSLTokenizeString
 *
 * Revision 1.20  2003/01/21 22:04:34  warmerda
 * don't report errors for pj_get_def or pj_dalloc missing
 *
 * Revision 1.19  2002/12/09 17:24:33  warmerda
 * fixed PROJ_STATIC settings for pj_strerrno
 *
 * Revision 1.18  2002/12/09 16:49:55  warmerda
 * implemented support for alternate GEOGCS units
 *
 * Revision 1.17  2002/11/27 14:48:22  warmerda
 * added PROJSO environment variable
 *
 * Revision 1.16  2002/11/19 20:47:04  warmerda
 * fixed to call pj_free, not pj_dalloc for projPJ
 *
 * Revision 1.15  2002/06/11 18:02:03  warmerda
 * add PROJ.4 normalization and EPSG support
 *
 * Revision 1.14  2002/03/05 14:25:14  warmerda
 * expand tabs
 *
 * Revision 1.13  2002/01/18 04:49:17  warmerda
 * report CPL errors if transform() fails
 *
 * Revision 1.12  2001/12/11 17:37:24  warmerda
 * improved PROJ.4 error reporting.
 *
 * Revision 1.11  2001/11/18 00:57:53  warmerda
 * change printf to CPLDebug
 *
 * Revision 1.10  2001/11/07 22:14:17  warmerda
 * add a way of statically linking in PROJ.4
 *
 * Revision 1.9  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.8  2001/05/24 21:02:42  warmerda
 * moved OGRCoordinateTransform destructor defn
 *
 * Revision 1.7  2001/01/19 21:10:47  warmerda
 * replaced tabs
 *
 * Revision 1.6  2000/07/12 18:19:09  warmerda
 * Removed debug statements.
 *
 * Revision 1.5  2000/07/09 20:48:28  warmerda
 * rewrote to use PROJ.4 datum shifting
 *
 * Revision 1.4  2000/03/24 14:49:31  warmerda
 * fetch TOWGS84 coefficients
 *
 * Revision 1.3  2000/03/20 23:08:18  warmerda
 * Added docs.
 *
 * Revision 1.2  2000/03/20 22:40:23  warmerda
 * Added C API.
 *
 * Revision 1.1  2000/03/20 15:00:11  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_string.h"

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

#ifdef WIN32
#  define LIBNAME      "proj.dll"
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
    

    OGRSpatialReference *poSRSTarget;
    void        *psPJTarget;
    int         bTargetLatLong;
    double      dfTargetToRadians;
    double      dfTargetFromRadians;

    int         nErrorCount;

public:
                OGRProj4CT();
    virtual     ~OGRProj4CT();

    int         Initialize( OGRSpatialReference *poSource, 
                            OGRSpatialReference *poTarget );

    virtual OGRSpatialReference *GetSourceCS();
    virtual OGRSpatialReference *GetTargetCS();
    virtual int Transform( int nCount, 
                           double *x, double *y, double *z = NULL );

};

/************************************************************************/
/*                          LoadProjLibrary()                           */
/************************************************************************/

static int LoadProjLibrary()

{
    static int  bTriedToLoad = FALSE;
    const char *pszLibName = LIBNAME;
    
    if( bTriedToLoad )
        return( pfn_pj_init != NULL );

    bTriedToLoad = TRUE;

    if( getenv("PROJSO") != NULL )
        pszLibName = getenv("PROJSO");

#ifdef PROJ_STATIC
    pfn_pj_init = pj_init;
    pfn_pj_init_plus = pj_init_plus;
    pfn_pj_fwd = pj_fwd;
    pfn_pj_inv = pj_inv;
    pfn_pj_free = pj_free;
    pfn_pj_transform = pj_transform;
    pfn_pj_get_errno_ref = pj_get_errno_ref;
    pfn_pj_strerrno = pj_strerrno;
    pfn_pj_dalloc = pj_dalloc;
#ifdef PJ_VERSION >= 446
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

void OCTDestroyCoordinateTransformation( OGRCoordinateTransformationH hCT )

{
    delete (OGRCoordinateTransformation *) hCT;
}

/************************************************************************/
/*                 OGRCreateCoordinateTransformation()                  */
/************************************************************************/

/**
 * Create transformation object.
 *
 * This is the same as the C function OCTNewCoordinateTransformation().
 *
 * The delete operator, or OCTDestroyCoordinateTransformation() should
 * be used to destroy transformation objects. 
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
                  LIBNAME );
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

OGRCoordinateTransformationH OCTNewCoordinateTransformation(
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
/*      Establish PROJ.4 handle for source if projection.               */
/* -------------------------------------------------------------------- */
    char        *pszProj4Defn;

    if( poSRSSource->exportToProj4( &pszProj4Defn ) != OGRERR_NONE )
        return FALSE;

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
    
    CPLFree( pszProj4Defn );
    
    if( psPJSource == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for target if projection.               */
/* -------------------------------------------------------------------- */
    if( poSRSTarget->exportToProj4( &pszProj4Defn ) != OGRERR_NONE )
        return FALSE;

    psPJTarget = pfn_pj_init_plus( pszProj4Defn );
    
    if( psPJTarget == NULL )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Failed to initialize PROJ.4 with `%s'.", 
                  pszProj4Defn );
    
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
/************************************************************************/

int OGRProj4CT::Transform( int nCount, double *x, double *y, double *z )

{
    int   err, i;

/* -------------------------------------------------------------------- */
/*      Potentially transform to radians.                               */
/* -------------------------------------------------------------------- */
    if( bSourceLatLong )
    {
        for( i = 0; i < nCount; i++ )
        {
            x[i] *= dfSourceToRadians;
            y[i] *= dfSourceToRadians;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do the transformation using PROJ.4.                             */
/* -------------------------------------------------------------------- */
    err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );

/* -------------------------------------------------------------------- */
/*      Try to report an error through CPL.  Get proj.4 error string    */
/*      if possible.  Try to avoid reporting thousands of error         */
/*      ... supress further error reporting on this OGRProj4CT if we    */
/*      have already reported 20 errors.                                */
/* -------------------------------------------------------------------- */
    if( err != 0 )
    {
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
            x[i] *= dfTargetFromRadians;
            y[i] *= dfTargetFromRadians;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                            OCTTransform()                            */
/************************************************************************/

int OCTTransform( OGRCoordinateTransformationH hTransform,
                  int nCount, double *x, double *y, double *z )

{
    return ((OGRCoordinateTransformation*) hTransform)->
        Transform( nCount, x, y,z );
}
