/******************************************************************************
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Implementation of the GDALTransformer wrapper around CRS.C functions
 *           to build a polynomial transformation based on ground control
 *           points.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ***************************************************************************

    CRS.C - Center for Remote Sensing rectification routines

    Written By: Brian J. Buckley

            At: The Center for Remote Sensing
                Michigan State University
                302 Berkey Hall
                East Lansing, MI  48824
                (517)353-7195

    Written: 12/19/91

    Last Update: 12/26/91 Brian J. Buckley
    Last Update:  1/24/92 Brian J. Buckley
      Added printout of trnfile. Triggered by BDEBUG.
    Last Update:  1/27/92 Brian J. Buckley
      Fixed bug so that only the active control points were used.
    Last Update:  6/29/2011 C. F. Stallmann & R. van den Dool (South African National Space Agency)
      GCP refinement added

    Copyright (c) 1992, Michigan State University
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

 ****************************************************************************/

#include "gdal_alg.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_atomic_ops.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

CPL_CVSID("$Id$")

#define MAXORDER 3

namespace {
struct Control_Points
{
    int  count;
    double *e1;
    double *n1;
    double *e2;
    double *n2;
    int *status;
};
}

typedef struct
{
    GDALTransformerInfo sTI;

    double adfToGeoX[20];
    double adfToGeoY[20];

    double adfFromGeoX[20];
    double adfFromGeoY[20];
    double x1_mean;
    double y1_mean;
    double x2_mean;
    double y2_mean;
    int    nOrder;
    int    bReversed;

    int       nGCPCount;
    GDAL_GCP *pasGCPList;
    int    bRefine;
    int    nMinimumGcps;
    double dfTolerance;

    volatile int nRefCount;

} GCPTransformInfo;

CPL_C_START
CPLXMLNode *GDALSerializeGCPTransformer( void *pTransformArg );
void *GDALDeserializeGCPTransformer( CPLXMLNode *psTree );
CPL_C_END

/* crs.c */
static int CRS_georef(double, double, double *, double *,
                              double [], double [], int);
static int CRS_compute_georef_equations(GCPTransformInfo *psInfo, struct Control_Points *,
    double [], double [], double [], double [], int);
static int remove_outliers(GCPTransformInfo *);


#define MSUCCESS     1 /* SUCCESS */
#define MNPTERR      0 /* NOT ENOUGH POINTS */
#define MUNSOLVABLE -1 /* NOT SOLVABLE */
#define MMEMERR     -2 /* NOT ENOUGH MEMORY */
#define MPARMERR    -3 /* PARAMETER ERROR */
#define MINTERR     -4 /* INTERNAL ERROR */

static const char * const CRS_error_message[] = {
    "Failed to compute GCP transform: Not enough points available",
    "Failed to compute GCP transform: Transform is not solvable",
    "Failed to compute GCP transform: Not enough memory",
    "Failed to compute GCP transform: Parameter error",
    "Failed to compute GCP transform: Internal error"
};

/************************************************************************/
/*                   GDALCreateSimilarGCPTransformer()                  */
/************************************************************************/

static
void* GDALCreateSimilarGCPTransformer( void *hTransformArg, double dfRatioX, double dfRatioY )
{
    GDAL_GCP *pasGCPList = nullptr;
    GCPTransformInfo *psInfo = static_cast<GCPTransformInfo *>(hTransformArg);

    VALIDATE_POINTER1( hTransformArg, "GDALCreateSimilarGCPTransformer", nullptr );

    if( dfRatioX == 1.0 && dfRatioY == 1.0 )
    {
        /* We can just use a ref count, since using the source transformation */
        /* is thread-safe */
        CPLAtomicInc(&(psInfo->nRefCount));
    }
    else
    {
        pasGCPList = GDALDuplicateGCPs( psInfo->nGCPCount, psInfo->pasGCPList );
        for(int i=0;i<psInfo->nGCPCount;i++)
        {
            pasGCPList[i].dfGCPPixel /= dfRatioX;
            pasGCPList[i].dfGCPLine /= dfRatioY;
        }
        /* As remove_outliers modifies the provided GCPs we don't need to reapply it */
        psInfo = static_cast<GCPTransformInfo *>(GDALCreateGCPTransformer(
            psInfo->nGCPCount, pasGCPList, psInfo->nOrder, psInfo->bReversed ));
        GDALDeinitGCPs( psInfo->nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    return psInfo;
}

/************************************************************************/
/*                      GDALCreateGCPTransformer()                      */
/************************************************************************/

static
void *GDALCreateGCPTransformerEx( int nGCPCount, const GDAL_GCP *pasGCPList,
                                int nReqOrder, int bReversed, int bRefine, double dfTolerance, int nMinimumGcps)

{
    GCPTransformInfo *psInfo = nullptr;
    double *padfGeoX = nullptr;
    double *padfGeoY = nullptr;
    double *padfRasterX = nullptr;
    double *padfRasterY = nullptr;
    int *panStatus = nullptr;
    int iGCP = 0;
    int nCRSresult = 0;
    struct Control_Points sPoints;

    double x1_sum = 0;
    double y1_sum = 0;
    double x2_sum = 0;
    double y2_sum = 0;

    memset( &sPoints, 0, sizeof(sPoints) );

    if( nReqOrder == 0 )
    {
        if( nGCPCount >= 10 )
            nReqOrder = 2; /*for now we avoid 3rd order since it is unstable*/
        else if( nGCPCount >= 6 )
            nReqOrder = 2;
        else
            nReqOrder = 1;
    }

    psInfo = static_cast<GCPTransformInfo *>(CPLCalloc(sizeof(GCPTransformInfo),1));
    psInfo->bReversed = bReversed;
    psInfo->nOrder = nReqOrder;
    psInfo->bRefine = bRefine;
    psInfo->dfTolerance = dfTolerance;
    psInfo->nMinimumGcps = nMinimumGcps;

    psInfo->nRefCount = 1;

    psInfo->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );
    psInfo->nGCPCount = nGCPCount;

    memcpy( psInfo->sTI.abySignature, GDAL_GTI2_SIGNATURE, strlen(GDAL_GTI2_SIGNATURE) );
    psInfo->sTI.pszClassName = "GDALGCPTransformer";
    psInfo->sTI.pfnTransform = GDALGCPTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyGCPTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeGCPTransformer;
    psInfo->sTI.pfnCreateSimilar = GDALCreateSimilarGCPTransformer;

/* -------------------------------------------------------------------- */
/*      Compute the forward and reverse polynomials.                    */
/* -------------------------------------------------------------------- */

    if( nGCPCount == 0 )
    {
        nCRSresult = MNPTERR;
    }
    else if(bRefine)
    {
        nCRSresult = remove_outliers(psInfo);
    }
    else
    {
        /* -------------------------------------------------------------------- */
        /*      Allocate and initialize the working points list.                */
        /* -------------------------------------------------------------------- */
      try
      {
        padfGeoX = new double[nGCPCount];
        padfGeoY = new double[nGCPCount];
        padfRasterX = new double[nGCPCount];
        padfRasterY = new double[nGCPCount];
        panStatus = new int[nGCPCount];
        for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
        {
            panStatus[iGCP] = 1;
            padfGeoX[iGCP] = pasGCPList[iGCP].dfGCPX;
            padfGeoY[iGCP] = pasGCPList[iGCP].dfGCPY;
            padfRasterX[iGCP] = pasGCPList[iGCP].dfGCPPixel;
            padfRasterY[iGCP] = pasGCPList[iGCP].dfGCPLine;
            x1_sum += pasGCPList[iGCP].dfGCPPixel;
            y1_sum += pasGCPList[iGCP].dfGCPLine;
            x2_sum += pasGCPList[iGCP].dfGCPX;
            y2_sum += pasGCPList[iGCP].dfGCPY;
        }
        psInfo->x1_mean = x1_sum / nGCPCount;
        psInfo->y1_mean = y1_sum / nGCPCount;
        psInfo->x2_mean = x2_sum / nGCPCount;
        psInfo->y2_mean = y2_sum / nGCPCount;

        sPoints.count = nGCPCount;
        sPoints.e1 = padfRasterX;
        sPoints.n1 = padfRasterY;
        sPoints.e2 = padfGeoX;
        sPoints.n2 = padfGeoY;
        sPoints.status = panStatus;
        nCRSresult = CRS_compute_georef_equations( psInfo, &sPoints,
                                                psInfo->adfToGeoX, psInfo->adfToGeoY,
                                                psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                                                nReqOrder );
      }
      catch( const std::exception& e )
      {
          CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
          nCRSresult = MINTERR;
      }
      delete[] padfGeoX;
      delete[] padfGeoY;
      delete[] padfRasterX;
      delete[] padfRasterY;
      delete[] panStatus;
    }

    if (nCRSresult != 1)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "%s", CRS_error_message[-nCRSresult]);
        GDALDestroyGCPTransformer( psInfo );
        return nullptr;
    }
    else
    {
        return psInfo;
    }
}

/**
 * Create GCP based polynomial transformer.
 *
 * Computes least squares fit polynomials from a provided set of GCPs,
 * and stores the coefficients for later transformation of points between
 * pixel/line and georeferenced coordinates.
 *
 * The return value should be used as a TransformArg in combination with
 * the transformation function GDALGCPTransform which fits the
 * GDALTransformerFunc signature.  The returned transform argument should
 * be deallocated with GDALDestroyGCPTransformer when no longer needed.
 *
 * This function may fail (returning nullptr) if the provided set of GCPs
 * are inadequate for the requested order, the determinate is zero or they
 * are otherwise "ill conditioned".
 *
 * Note that 2nd order requires at least 6 GCPs, and 3rd order requires at
 * least 10 gcps.  If nReqOrder is 0 the highest order possible (limited to 2)
 * with the provided gcp count will be used.
 *
 * @param nGCPCount the number of GCPs in pasGCPList.
 * @param pasGCPList an array of GCPs to be used as input.
 * @param nReqOrder the requested polynomial order.  It should be 1, 2 or 3.
 * Using 3 is not recommended due to potential numeric instabilities issues.
 * @param bReversed set it to TRUE to compute the reversed transformation.
 *
 * @return the transform argument or nullptr if creation fails.
 */
void *GDALCreateGCPTransformer( int nGCPCount, const GDAL_GCP *pasGCPList,
                                int nReqOrder, int bReversed )

{
    return GDALCreateGCPTransformerEx(nGCPCount, pasGCPList, nReqOrder, bReversed, FALSE, -1, -1);
}

/** Create GCP based polynomial transformer, with a tolerance threshold to
 * discard GCPs that transform badly.
 */
void *GDALCreateGCPRefineTransformer( int nGCPCount, const GDAL_GCP *pasGCPList,
                                int nReqOrder, int bReversed, double dfTolerance, int nMinimumGcps)

{
    //If no minimumGcp parameter was passed, we  use the default value according to the model
    if(nMinimumGcps == -1)
    {
        nMinimumGcps = ((nReqOrder+1) * (nReqOrder+2)) / 2 + 1;
    }
    return GDALCreateGCPTransformerEx(nGCPCount, pasGCPList, nReqOrder, bReversed, TRUE, dfTolerance, nMinimumGcps);
}


/************************************************************************/
/*                     GDALDestroyGCPTransformer()                      */
/************************************************************************/

/**
 * Destroy GCP transformer.
 *
 * This function is used to destroy information about a GCP based
 * polynomial transformation created with GDALCreateGCPTransformer().
 *
 * @param pTransformArg the transform arg previously returned by
 * GDALCreateGCPTransformer().
 */

void GDALDestroyGCPTransformer( void *pTransformArg )

{
    if( pTransformArg == nullptr )
        return;

    GCPTransformInfo *psInfo = static_cast<GCPTransformInfo *>(pTransformArg);

    if( CPLAtomicDec(&(psInfo->nRefCount)) == 0 )
    {
        GDALDeinitGCPs( psInfo->nGCPCount, psInfo->pasGCPList );
        CPLFree( psInfo->pasGCPList );

        CPLFree( pTransformArg );
    }
}

/************************************************************************/
/*                          GDALGCPTransform()                          */
/************************************************************************/

/**
 * Transforms point based on GCP derived polynomial model.
 *
 * This function matches the GDALTransformerFunc signature, and can be
 * used to transform one or more points from pixel/line coordinates to
 * georeferenced coordinates (SrcToDst) or vice versa (DstToSrc).
 *
 * @param pTransformArg return value from GDALCreateGCPTransformer().
 * @param bDstToSrc TRUE if transformation is from the destination
 * (georeferenced) coordinates to pixel/line or FALSE when transforming
 * from pixel/line to georeferenced coordinates.
 * @param nPointCount the number of values in the x, y and z arrays.
 * @param x array containing the X values to be transformed.
 * @param y array containing the Y values to be transformed.
 * @param z array containing the Z values to be transformed.
 * @param panSuccess array in which a flag indicating success (TRUE) or
 * failure (FALSE) of the transformation are placed.
 *
 * @return TRUE.
 */

int GDALGCPTransform( void *pTransformArg, int bDstToSrc,
                      int nPointCount,
                      double *x, double *y, CPL_UNUSED double *z,
                      int *panSuccess )

{
    int i = 0;
    GCPTransformInfo *psInfo = static_cast<GCPTransformInfo *>(pTransformArg);

    if( psInfo->bReversed )
        bDstToSrc = !bDstToSrc;

    for( i = 0; i < nPointCount; i++ )
    {
        if( x[i] == HUGE_VAL || y[i] == HUGE_VAL )
        {
            panSuccess[i] = FALSE;
            continue;
        }

        if( bDstToSrc )
        {
            CRS_georef( x[i] - psInfo->x2_mean, y[i] - psInfo->y2_mean, x + i, y + i,
                        psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                        psInfo->nOrder );
        }
        else
        {
            CRS_georef( x[i] - psInfo->x1_mean, y[i] - psInfo->y1_mean, x + i, y + i,
                        psInfo->adfToGeoX, psInfo->adfToGeoY,
                        psInfo->nOrder );
        }
        panSuccess[i] = TRUE;
    }

    return TRUE;
}

/************************************************************************/
/*                    GDALSerializeGCPTransformer()                     */
/************************************************************************/

CPLXMLNode *GDALSerializeGCPTransformer( void *pTransformArg )

{
    CPLXMLNode *psTree = nullptr;
    GCPTransformInfo *psInfo = static_cast<GCPTransformInfo *>(pTransformArg);

    VALIDATE_POINTER1( pTransformArg, "GDALSerializeGCPTransformer", nullptr );

    psTree = CPLCreateXMLNode( nullptr, CXT_Element, "GCPTransformer" );

/* -------------------------------------------------------------------- */
/*      Serialize Order and bReversed.                                  */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(
        psTree, "Order",
        CPLSPrintf( "%d", psInfo->nOrder ) );

    CPLCreateXMLElementAndValue(
        psTree, "Reversed",
        CPLSPrintf( "%d", psInfo->bReversed ) );

    if( psInfo->bRefine )
    {
        CPLCreateXMLElementAndValue(
            psTree, "Refine",
            CPLSPrintf( "%d", psInfo->bRefine ) );

        CPLCreateXMLElementAndValue(
            psTree, "MinimumGcps",
            CPLSPrintf( "%d", psInfo->nMinimumGcps ) );

        CPLCreateXMLElementAndValue(
            psTree, "Tolerance",
            CPLSPrintf( "%f", psInfo->dfTolerance ) );
    }

/* -------------------------------------------------------------------- */
/*     Attach GCP List.                                                 */
/* -------------------------------------------------------------------- */
    if( psInfo->nGCPCount > 0 )
    {
        if(psInfo->bRefine)
        {
            remove_outliers(psInfo);
        }

        GDALSerializeGCPListToXML( psTree,
                                   psInfo->pasGCPList,
                                   psInfo->nGCPCount,
                                   nullptr );
    }

    return psTree;
}

/************************************************************************/
/*               GDALDeserializeReprojectionTransformer()               */
/************************************************************************/

void *GDALDeserializeGCPTransformer( CPLXMLNode *psTree )

{
    GDAL_GCP *pasGCPList = nullptr;
    int nGCPCount = 0;
    void *pResult = nullptr;
    int nReqOrder = 0;
    int bReversed = 0;
    int bRefine = 0;
    int nMinimumGcps = 0;
    double dfTolerance = 0.0;

    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != nullptr )
    {
        GDALDeserializeGCPListFromXML( psGCPList,
                                       &pasGCPList,
                                       &nGCPCount,
                                       nullptr );
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    nReqOrder = atoi(CPLGetXMLValue(psTree,"Order","3"));
    bReversed = atoi(CPLGetXMLValue(psTree,"Reversed","0"));
    bRefine = atoi(CPLGetXMLValue(psTree,"Refine","0"));
    nMinimumGcps = atoi(CPLGetXMLValue(psTree,"MinimumGcps","6"));
    dfTolerance = CPLAtof(CPLGetXMLValue(psTree,"Tolerance","1.0"));

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    if(bRefine)
    {
        pResult = GDALCreateGCPRefineTransformer( nGCPCount, pasGCPList, nReqOrder,
                                        bReversed, dfTolerance, nMinimumGcps );
    }
    else
    {
        pResult = GDALCreateGCPTransformer( nGCPCount, pasGCPList, nReqOrder,
                                        bReversed );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup GCP copy.                                               */
/* -------------------------------------------------------------------- */
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

    return pResult;
}

/************************************************************************/
/* ==================================================================== */
/*      Everything below this point derived from the CRS.C from GRASS.  */
/* ==================================================================== */
/************************************************************************/


/* STRUCTURE FOR USE INTERNALLY WITH THESE FUNCTIONS.  THESE FUNCTIONS EXPECT
   SQUARE MATRICES SO ONLY ONE VARIABLE IS GIVEN (N) FOR THE MATRIX SIZE */

struct MATRIX
{
    int     n;     /* SIZE OF THIS MATRIX (N x N) */
    double *v;
};

/* CALCULATE OFFSET INTO ARRAY BASED ON R/C */

#define M(row,col) m->v[(((row)-1)*(m->n))+(col)-1]

/***************************************************************************/
/*
    FUNCTION PROTOTYPES FOR STATIC (INTERNAL) FUNCTIONS
*/
/***************************************************************************/

static int calccoef(struct Control_Points *,double,double,double *,double *,int);
static int calcls(struct Control_Points *,struct MATRIX *,double,double,
                  double *,double *,double *,double *);
static int exactdet(struct Control_Points *,struct MATRIX *,double,double,
                    double *,double *,double *,double *);
static int solvemat(struct MATRIX *,double *,double *,double *,double *);
static double term(int,double,double);

/***************************************************************************/
/*
    TRANSFORM A SINGLE COORDINATE PAIR.
*/
/***************************************************************************/

static int
CRS_georef (
    double e1,  /* EASTINGS TO BE TRANSFORMED */
    double n1,  /* NORTHINGS TO BE TRANSFORMED */
    double *e,  /* EASTINGS TO BE TRANSFORMED */
    double *n,  /* NORTHINGS TO BE TRANSFORMED */
    double E[], /* EASTING COEFFICIENTS */
    double N[], /* NORTHING COEFFICIENTS */
    int order  /* ORDER OF TRANSFORMATION TO BE PERFORMED, MUST MATCH THE
               ORDER USED TO CALCULATE THE COEFFICIENTS */
)
  {
  double e3 = 0.0;
  double e2n = 0.0;
  double en2 = 0.0;
  double n3 = 0.0;
  double e2 = 0.0;
  double en = 0.0;
  double n2 = 0.0;

  switch(order)
    {
    case 1:

      *e = E[0] + E[1] * e1 + E[2] * n1;
      *n = N[0] + N[1] * e1 + N[2] * n1;
      break;

    case 2:

      e2 = e1 * e1;
      n2 = n1 * n1;
      en = e1 * n1;

      *e = E[0]      + E[1] * e1 + E[2] * n1 +
           E[3] * e2 + E[4] * en + E[5] * n2;
      *n = N[0]      + N[1] * e1 + N[2] * n1 +
           N[3] * e2 + N[4] * en + N[5] * n2;
      break;

    case 3:

      e2  = e1 * e1;
      en  = e1 * n1;
      n2  = n1 * n1;
      e3  = e1 * e2;
      e2n = e2 * n1;
      en2 = e1 * n2;
      n3  = n1 * n2;

      *e = E[0]      +
           E[1] * e1 + E[2] * n1  +
           E[3] * e2 + E[4] * en  + E[5] * n2  +
           E[6] * e3 + E[7] * e2n + E[8] * en2 + E[9] * n3;
      *n = N[0]      +
           N[1] * e1 + N[2] * n1  +
           N[3] * e2 + N[4] * en  + N[5] * n2  +
           N[6] * e3 + N[7] * e2n + N[8] * en2 + N[9] * n3;
      break;

    default:

      return(MPARMERR);
    }

  return(MSUCCESS);
  }

/***************************************************************************/
/*
    COMPUTE THE GEOREFFERENCING COEFFICIENTS BASED ON A SET OF CONTROL POINTS
*/
/***************************************************************************/

static int
CRS_compute_georef_equations (GCPTransformInfo *psInfo, struct Control_Points *cp,
                                      double E12[], double N12[],
                                      double E21[], double N21[],
                                      int order)
{
    double *tempptr = nullptr;
    int status = 0;

    if(order < 1 || order > MAXORDER)
        return(MPARMERR);

    /* CALCULATE THE FORWARD TRANSFORMATION COEFFICIENTS */

    status = calccoef(cp,psInfo->x1_mean,psInfo->y1_mean,E12,N12,order);
    if(status != MSUCCESS)
        return(status);

    /* SWITCH THE 1 AND 2 EASTING AND NORTHING ARRAYS */

    tempptr = cp->e1;
    cp->e1 = cp->e2;
    cp->e2 = tempptr;
    tempptr = cp->n1;
    cp->n1 = cp->n2;
    cp->n2 = tempptr;

    /* CALCULATE THE BACKWARD TRANSFORMATION COEFFICIENTS */

    status = calccoef(cp,psInfo->x2_mean,psInfo->y2_mean,E21,N21,order);

    /* SWITCH THE 1 AND 2 EASTING AND NORTHING ARRAYS BACK */

    tempptr = cp->e1;
    cp->e1 = cp->e2;
    cp->e2 = tempptr;
    tempptr = cp->n1;
    cp->n1 = cp->n2;
    cp->n2 = tempptr;

    return(status);
}

/***************************************************************************/
/*
    COMPUTE THE GEOREFFERENCING COEFFICIENTS BASED ON A SET OF CONTROL POINTS
*/
/***************************************************************************/

static int
calccoef (struct Control_Points *cp, double x_mean, double y_mean, double E[], double N[], int order)
{
    struct MATRIX m;
    double *a = nullptr;
    double *b = nullptr;
    int numactive = 0;   /* NUMBER OF ACTIVE CONTROL POINTS */
    int status = 0;
    int i = 0;

    memset( &m, 0, sizeof(m) );

    /* CALCULATE THE NUMBER OF VALID CONTROL POINTS */

    for(i = numactive = 0 ; i < cp->count ; i++)
    {
        if(cp->status[i] > 0)
            numactive++;
    }

    /* CALCULATE THE MINIMUM NUMBER OF CONTROL POINTS NEEDED TO DETERMINE
       A TRANSFORMATION OF THIS ORDER */

    m.n = ((order + 1) * (order + 2)) / 2;

    if(numactive < m.n)
        return(MNPTERR);

    /* INITIALIZE MATRIX */

    m.v = static_cast<double*>(VSICalloc(m.n*m.n,sizeof(double)));
    if(m.v == nullptr)
    {
        return(MMEMERR);
    }
    a = static_cast<double*>(VSICalloc(m.n,sizeof(double)));
    if(a == nullptr)
    {
        CPLFree(m.v);
        return(MMEMERR);
    }
    b = static_cast<double*>(VSICalloc(m.n,sizeof(double)));
    if(b == nullptr)
    {
        CPLFree(m.v);
        CPLFree(a);
        return(MMEMERR);
    }

    if(numactive == m.n)
        status = exactdet(cp,&m, x_mean, y_mean, a,b,E,N);
    else
        status = calcls(cp,&m, x_mean, y_mean,a,b,E,N);

    CPLFree(m.v);
    CPLFree(a);
    CPLFree(b);

    return(status);
}

/***************************************************************************/
/*
    CALCULATE THE TRANSFORMATION COEFFICIENTS WITH EXACTLY THE MINIMUM
    NUMBER OF CONTROL POINTS REQUIRED FOR THIS TRANSFORMATION.
*/
/***************************************************************************/

static int exactdet (
    struct Control_Points *cp,
    struct MATRIX *m,
    double x_mean,
    double y_mean,
    double a[],
    double b[],
    double E[],     /* EASTING COEFFICIENTS */
    double N[]     /* NORTHING COEFFICIENTS */
)
  {
  int currow = 1;

  for(int pntnow = 0 ; pntnow < cp->count ; pntnow++)
    {
    if(cp->status[pntnow] > 0)
      {
      /* POPULATE MATRIX M */

      for(int j = 1 ; j <= m->n ; j++)
        {
        M(currow,j) = term(j,cp->e1[pntnow] - x_mean, cp->n1[pntnow] - y_mean);
        }

      /* POPULATE MATRIX A AND B */

      a[currow-1] = cp->e2[pntnow];
      b[currow-1] = cp->n2[pntnow];

      currow++;
      }
    }

  if(currow - 1 != m->n)
    return(MINTERR);

  return(solvemat(m,a,b,E,N));
  }

/***************************************************************************/
/*
    CALCULATE THE TRANSFORMATION COEFFICIENTS WITH MORE THAN THE MINIMUM
    NUMBER OF CONTROL POINTS REQUIRED FOR THIS TRANSFORMATION.  THIS
    ROUTINE USES THE LEAST SQUARES METHOD TO COMPUTE THE COEFFICIENTS.
*/
/***************************************************************************/

static int calcls (
    struct Control_Points *cp,
    struct MATRIX *m,
    double x_mean,
    double y_mean,
    double a[],
    double b[],
    double E[],     /* EASTING COEFFICIENTS */
    double N[]     /* NORTHING COEFFICIENTS */
)
{
    int numactive = 0;

    /* INITIALIZE THE UPPER HALF OF THE MATRIX AND THE TWO COLUMN VECTORS */

    for(int i = 1 ; i <= m->n ; i++)
    {
        for(int j = i ; j <= m->n ; j++)
            M(i,j) = 0.0;
        a[i-1] = b[i-1] = 0.0;
    }

    /* SUM THE UPPER HALF OF THE MATRIX AND THE COLUMN VECTORS ACCORDING TO
       THE LEAST SQUARES METHOD OF SOLVING OVER DETERMINED SYSTEMS */

    for(int n = 0 ; n < cp->count ; n++)
    {
        if(cp->status[n] > 0)
        {
            numactive++;
            for(int i = 1 ; i <= m->n ; i++)
            {
                for(int j = i ; j <= m->n ; j++)
                    M(i,j) += term(i,cp->e1[n] - x_mean, cp->n1[n] - y_mean) * term(j,cp->e1[n] - x_mean, cp->n1[n] - y_mean);

                a[i-1] += cp->e2[n] * term(i,cp->e1[n] - x_mean, cp->n1[n] - y_mean);
                b[i-1] += cp->n2[n] * term(i,cp->e1[n] - x_mean, cp->n1[n] - y_mean);
            }
        }
    }

    if(numactive <= m->n)
        return(MINTERR);

    /* TRANSPOSE VALUES IN UPPER HALF OF M TO OTHER HALF */

    for(int i = 2 ; i <= m->n ; i++)
    {
        for(int j = 1 ; j < i ; j++)
            M(i,j) = M(j,i);
    }

    return(solvemat(m,a,b,E,N));
}

/***************************************************************************/
/*
    CALCULATE THE X/Y TERM BASED ON THE TERM NUMBER

ORDER\TERM   1    2    3    4    5    6    7    8    9   10
  1        e0n0 e1n0 e0n1
  2        e0n0 e1n0 e0n1 e2n0 e1n1 e0n2
  3        e0n0 e1n0 e0n1 e2n0 e1n1 e0n2 e3n0 e2n1 e1n2 e0n3
*/
/***************************************************************************/

static double term (int nTerm, double e, double n)
{
    switch(nTerm)
    {
      case  1: return(1.0);
      case  2: return(e);
      case  3: return(n);
      case  4: return((e*e));
      case  5: return((e*n));
      case  6: return((n*n));
      case  7: return((e*e*e));
      case  8: return((e*e*n));
      case  9: return((e*n*n));
      case 10: return((n*n*n));
    }
    return 0.0;
}

/***************************************************************************/
/*
    SOLVE FOR THE 'E' AND 'N' COEFFICIENTS BY USING A SOMEWHAT MODIFIED
    GAUSSIAN ELIMINATION METHOD.

    | M11 M12 ... M1n | | E0   |   | a0   |
    | M21 M22 ... M2n | | E1   | = | a1   |
    |  .   .   .   .  | | .    |   | .    |
    | Mn1 Mn2 ... Mnn | | En-1 |   | an-1 |

    and

    | M11 M12 ... M1n | | N0   |   | b0   |
    | M21 M22 ... M2n | | N1   | = | b1   |
    |  .   .   .   .  | | .    |   | .    |
    | Mn1 Mn2 ... Mnn | | Nn-1 |   | bn-1 |
*/
/***************************************************************************/

static int solvemat (struct MATRIX *m,
  double a[], double b[], double E[], double N[])
{
    for(int i = 1 ; i <= m->n ; i++)
    {
        int j = i;

        /* find row with largest magnitude value for pivot value */

        double pivot = M(i,j); /* ACTUAL VALUE OF THE LARGEST PIVOT CANDIDATE */
        int imark = i;
        for(int i2 = i + 1 ; i2 <= m->n ; i2++)
        {
            if(fabs(M(i2,j)) > fabs(pivot))
            {
                pivot = M(i2,j);
                imark = i2;
            }
        }

        /* if the pivot is very small then the points are nearly co-linear */
        /* co-linear points result in an undefined matrix, and nearly */
        /* co-linear points results in a solution with rounding error */

        if(pivot == 0.0)
            return(MUNSOLVABLE);

        /* if row with highest pivot is not the current row, switch them */

        if(imark != i)
        {
            for(int j2 = 1 ; j2 <= m->n ; j2++)
            {
                std::swap(M(imark,j2), M(i,j2));
            }

            std::swap(a[imark-1], a[i-1]);
            std::swap(b[imark-1], b[i-1]);
        }

        /* compute zeros above and below the pivot, and compute
           values for the rest of the row as well */

        for(int i2 = 1 ; i2 <= m->n ; i2++)
        {
            if(i2 != i)
            {
                const double factor = M(i2,j) / pivot;
                for(int j2 = j ; j2 <= m->n ; j2++)
                    M(i2,j2) -= factor * M(i,j2);
                a[i2-1] -= factor * a[i-1];
                b[i2-1] -= factor * b[i-1];
            }
        }
    }

    /* SINCE ALL OTHER VALUES IN THE MATRIX ARE ZERO NOW, CALCULATE THE
       COEFFICIENTS BY DIVIDING THE COLUMN VECTORS BY THE DIAGONAL VALUES. */

    for(int i = 1 ; i <= m->n ; i++)
    {
        E[i-1] = a[i-1] / M(i,i);
        N[i-1] = b[i-1] / M(i,i);
    }

    return(MSUCCESS);
}

/***************************************************************************/
/*
  DETECTS THE WORST OUTLIER IN THE GCP LIST AND RETURNS THE INDEX OF THE
  OUTLIER.

  THE WORST OUTLIER IS CALCULATED BASED ON THE CONTROL POINTS, COEFFICIENTS
  AND THE ALLOWED TOLERANCE:

  sampleAdj = a0 + a1*sample + a2*line + a3*line*sample
  lineAdj = b0 + b1*sample + b2*line + b3*line*sample

  WHERE sampleAdj AND lineAdj ARE CORRELATED GCPS

  [residualSample] = [A1][sampleCoefficients] - [b1]
  [residualLine] = [A2][lineCoefficients] - [b2]

  sampleResidual^2 = sum( [residualSample]^2 )
  lineResidual^2 = sum( [lineSample]^2 )

  residuals(i) = squareRoot( residualSample(i)^2 + residualLine(i)^2 )

  THE GCP WITH THE GREATEST DISTANCE residual(i) GREATER THAN THE TOLERANCE WILL
  CONSIDERED THE WORST OUTLIER.

  IF NO OUTLIER CAN BE FOUND, -1 WILL BE RETURNED.
*/
/***************************************************************************/
static int worst_outlier(struct Control_Points *cp, double x_mean, double y_mean, int nOrder, double E[], double N[], double dfTolerance)
{
    //double dfSampleResidual = 0.0;
    //double dfLineResidual = 0.0;
    double *padfResiduals = static_cast<double*>(CPLCalloc(sizeof(double),cp->count));

    for(int nI = 0; nI < cp->count; nI++)
    {
        double dfSampleRes = 0.0;
        double dfLineRes = 0.0;
        CRS_georef( cp->e1[nI] - x_mean, cp->n1[nI] - y_mean, &dfSampleRes, &dfLineRes,E,N,nOrder );
        dfSampleRes -= cp->e2[nI];
        dfLineRes -= cp->n2[nI];
        //dfSampleResidual += dfSampleRes*dfSampleRes;
        //dfLineResidual += dfLineRes*dfLineRes;

        padfResiduals[nI] = sqrt(dfSampleRes*dfSampleRes + dfLineRes*dfLineRes);
    }

    int nIndex = -1;
    double dfDifference = -1.0;
    for(int nI = 0; nI < cp->count; nI++)
    {
        double dfCurrentDifference = padfResiduals[nI];
        if(fabs(dfCurrentDifference) < 1.19209290E-07F /*FLT_EPSILON*/)
        {
            dfCurrentDifference = 0.0;
        }
        if(dfCurrentDifference > dfDifference && dfCurrentDifference >= dfTolerance)
        {
            dfDifference = dfCurrentDifference;
            nIndex = nI;
        }
    }
    CPLFree( padfResiduals );
    return nIndex;
}

/***************************************************************************/
/*
  REMOVES THE WORST OUTLIERS ITERATIVELY UNTIL THE MINIMUM NUMBER OF GCPS
  ARE REACHED OR NO OUTLIERS CAN BE DETECTED.

  1. WE CALCULATE THE COEFFICIENTS FOR ALL THE GCPS.
  2. THE GCP LIST WILL BE SCANNED TO DETERMINE THE WORST OUTLIER USING
     THE CALCULATED COEFFICIENTS.
  3. THE WORST OUTLIER WILL BE REMOVED FROM THE GCP LIST.
  4. THE COEFFICIENTS WILL BE RECALCULATED WITHOUT THE WORST OUTLIER.
  5. STEP 1 TO 4 ARE EXECUTED UNTIL THE MINIMUM NUMBER OF GCPS WERE REACHED
     OR IF NO GCP IS CONSIDERED AN OUTLIER WITH THE PASSED TOLERANCE.
*/
/***************************************************************************/
static int remove_outliers( GCPTransformInfo *psInfo )
{
    double *padfGeoX = nullptr;
    double *padfGeoY = nullptr;
    double *padfRasterX = nullptr;
    double *padfRasterY = nullptr;
    int *panStatus = nullptr;
    int nCRSresult = 0;
    int nGCPCount = 0;
    int nMinimumGcps = 0;
    int nReqOrder = 0;
    double dfTolerance = 0;
    struct Control_Points sPoints;

    double x1_sum = 0;
    double y1_sum = 0;
    double x2_sum = 0;
    double y2_sum = 0;
    memset( &sPoints, 0, sizeof(sPoints) );

    nGCPCount = psInfo->nGCPCount;
    nMinimumGcps = psInfo->nMinimumGcps;
    nReqOrder = psInfo->nOrder;
    dfTolerance = psInfo->dfTolerance;

    try
    {
        padfGeoX = new double[nGCPCount];
        padfGeoY = new double[nGCPCount];
        padfRasterX = new double[nGCPCount];
        padfRasterY = new double[nGCPCount];
        panStatus = new int[nGCPCount];

        for( int nI = 0; nI < nGCPCount; nI++ )
        {
            panStatus[nI] = 1;
            padfGeoX[nI] = psInfo->pasGCPList[nI].dfGCPX;
            padfGeoY[nI] = psInfo->pasGCPList[nI].dfGCPY;
            padfRasterX[nI] = psInfo->pasGCPList[nI].dfGCPPixel;
            padfRasterY[nI] = psInfo->pasGCPList[nI].dfGCPLine;
            x1_sum += psInfo->pasGCPList[nI].dfGCPPixel;
            y1_sum += psInfo->pasGCPList[nI].dfGCPLine;
            x2_sum += psInfo->pasGCPList[nI].dfGCPX;
            y2_sum += psInfo->pasGCPList[nI].dfGCPY;
        }
        psInfo->x1_mean = x1_sum / nGCPCount;
        psInfo->y1_mean = y1_sum / nGCPCount;
        psInfo->x2_mean = x2_sum / nGCPCount;
        psInfo->y2_mean = y2_sum / nGCPCount;

        sPoints.count = nGCPCount;
        sPoints.e1 = padfRasterX;
        sPoints.n1 = padfRasterY;
        sPoints.e2 = padfGeoX;
        sPoints.n2 = padfGeoY;
        sPoints.status = panStatus;

        nCRSresult = CRS_compute_georef_equations( psInfo, &sPoints,
                                        psInfo->adfToGeoX, psInfo->adfToGeoY,
                                        psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                                        nReqOrder );

        while(sPoints.count > nMinimumGcps)
        {
            int nIndex =
                worst_outlier(&sPoints, psInfo->x1_mean, psInfo->y1_mean, psInfo->nOrder, 
                            psInfo->adfToGeoX, psInfo->adfToGeoY,
                            dfTolerance);

            //If no outliers were detected, stop the GCP elimination
            if(nIndex == -1)
            {
                break;
            }

            CPLFree(psInfo->pasGCPList[nIndex].pszId);
            CPLFree(psInfo->pasGCPList[nIndex].pszInfo);

            for( int nI = nIndex; nI < sPoints.count - 1; nI++ )
            {
                sPoints.e1[nI] = sPoints.e1[nI + 1];
                sPoints.n1[nI] = sPoints.n1[nI + 1];
                sPoints.e2[nI] = sPoints.e2[nI + 1];
                sPoints.n2[nI] = sPoints.n2[nI + 1];
                psInfo->pasGCPList[nI].pszId = psInfo->pasGCPList[nI + 1].pszId;
                psInfo->pasGCPList[nI].pszInfo = psInfo->pasGCPList[nI + 1].pszInfo;
            }

            sPoints.count = sPoints.count - 1;

            nCRSresult = CRS_compute_georef_equations( psInfo, &sPoints,
                                        psInfo->adfToGeoX, psInfo->adfToGeoY,
                                        psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                                        nReqOrder );
        }

        for( int nI = 0; nI < sPoints.count; nI++ )
        {
            psInfo->pasGCPList[nI].dfGCPX = sPoints.e2[nI];
            psInfo->pasGCPList[nI].dfGCPY = sPoints.n2[nI];
            psInfo->pasGCPList[nI].dfGCPPixel = sPoints.e1[nI];
            psInfo->pasGCPList[nI].dfGCPLine = sPoints.n1[nI];
        }
        psInfo->nGCPCount = sPoints.count;
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        nCRSresult = MINTERR;
    }
    delete[] padfGeoX;
    delete[] padfGeoY;
    delete[] padfRasterX;
    delete[] padfRasterY;
    delete[] panStatus;

    return nCRSresult;
}
