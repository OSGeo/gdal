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

/* ==================================================================== */
/*      PROJ.4 interface stuff.                                         */
/* ==================================================================== */
typedef struct { double u, v; }	UV;

#define PJ void

static PJ	*(*pfn_pj_init)(int, char**) = NULL;
static UV	(*pfn_pj_fwd)(UV, PJ *) = NULL;
static UV	(*pfn_pj_inv)(UV, PJ *) = NULL;
static void	(*pfn_pj_free)(PJ *) = NULL;
static int      (*pfn_pj_transform)(PJ *, PJ*, long, int, 
                                    double *, double *, double * );

#define RAD_TO_DEG	57.29577951308232
#define DEG_TO_RAD	.0174532925199432958

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
    void	*psPJSource;
    int         bSourceLatLong;

    OGRSpatialReference *poSRSTarget;
    void        *psPJTarget;
    int         bTargetLatLong;

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
    static int	bTriedToLoad = FALSE;
    
    if( bTriedToLoad )
        return( pfn_pj_init != NULL );

    bTriedToLoad = TRUE;

    CPLPushErrorHandler( CPLQuietErrorHandler );
    pfn_pj_init = (PJ *(*)(int, char**)) CPLGetSymbol( LIBNAME,
                                                       "pj_init" );
    CPLPopErrorHandler();
    
    if( pfn_pj_init == NULL )
       return( FALSE );

    pfn_pj_fwd = (UV (*)(UV,PJ*)) CPLGetSymbol( LIBNAME, "pj_fwd" );
    pfn_pj_inv = (UV (*)(UV,PJ*)) CPLGetSymbol( LIBNAME, "pj_inv" );
    pfn_pj_free = (void (*)(PJ*)) CPLGetSymbol( LIBNAME, "pj_free" );
    pfn_pj_transform = (int (*)(PJ*,PJ*,long,int,double*,double*,double*))
                        CPLGetSymbol( LIBNAME, "pj_transform" );

    if( pfn_pj_transform == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to load %s, but couldn't find pj_transform.\n"
                  "Please upgrade to PROJ 4.1.2 or later.", 
                  LIBNAME );

        return FALSE;
    }

    return( TRUE );
}

/************************************************************************/
/*                    ~OGRCoordinateTransformation()                    */
/************************************************************************/

OGRCoordinateTransformation::~OGRCoordinateTransformation()

{
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
    OGRProj4CT	*poCT;

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
}

/************************************************************************/
/*                            ~OGRProj4CT()                             */
/************************************************************************/

OGRProj4CT::~OGRProj4CT()

{
    if( poSRSSource != NULL )
        delete poSRSSource;

    if( poSRSTarget != NULL )
        delete poSRSTarget;

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
/*      Establish PROJ.4 handle for source if projection.               */
/* -------------------------------------------------------------------- */
    char	*pszProj4Defn, **papszArgs;

    if( poSRSSource->exportToProj4( &pszProj4Defn ) != OGRERR_NONE )
        return FALSE;

    printf( "source = %s\n", pszProj4Defn );
    papszArgs = CSLTokenizeStringComplex( pszProj4Defn, " +",TRUE,FALSE );
    
    psPJSource = pfn_pj_init( CSLCount(papszArgs), papszArgs );
    
    if( psPJSource == NULL )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Failed to initialize PROJ.4 with `%s'.", 
                  pszProj4Defn );
    
    CSLDestroy( papszArgs );
    CPLFree( pszProj4Defn );
    
    if( psPJSource == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Establish PROJ.4 handle for target if projection.               */
/* -------------------------------------------------------------------- */
    if( poSRSTarget->exportToProj4( &pszProj4Defn ) != OGRERR_NONE )
        return FALSE;

    printf( "target = %s\n", pszProj4Defn );
    papszArgs = CSLTokenizeStringComplex( pszProj4Defn, " +",TRUE,FALSE );
    
    psPJTarget = pfn_pj_init( CSLCount(papszArgs), papszArgs );
    
    if( psPJTarget == NULL )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Failed to initialize PROJ.4 with `%s'.", 
                  pszProj4Defn );
    
    CSLDestroy( papszArgs );
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

    if( bSourceLatLong )
    {
        for( i = 0; i < nCount; i++ )
        {
            x[i] *= DEG_TO_RAD;
            y[i] *= DEG_TO_RAD;
        }
    }

    err = pfn_pj_transform( psPJSource, psPJTarget, nCount, 1, x, y, z );

    if( err != 0 )
    {
        printf( "pfn_pj_transform failed:%d.\n", err );
        return FALSE;
    }

    if( bTargetLatLong )
    {
        for( i = 0; i < nCount; i++ )
        {
            x[i] *= RAD_TO_DEG;
            y[i] *= RAD_TO_DEG;
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
