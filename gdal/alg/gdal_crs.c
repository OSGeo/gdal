/******************************************************************************
 * $Id$
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Implemention of the GDALTransformer wrapper around CRS.C functions
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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
   
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
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/* Hum, we cannot include gdal_priv.h from a .c file... */
CPL_C_START

void GDALSerializeGCPListToXML( CPLXMLNode* psParentNode,
                                GDAL_GCP* pasGCPList,
                                int nGCPCount,
                                const char* pszGCPProjection );
void GDALDeserializeGCPListFromXML( CPLXMLNode* psGCPList,
                                    GDAL_GCP** ppasGCPList,
                                    int* pnGCPCount,
                                    char** ppszGCPProjection );

CPL_C_END

#define MAXORDER 3

struct Control_Points
{
    int  count;
    double *e1;
    double *n1;
    double *e2;
    double *n2;
    int *status;
};

typedef struct
{
    GDALTransformerInfo sTI;

    double adfToGeoX[20];
    double adfToGeoY[20];
    
    double adfFromGeoX[20];
    double adfFromGeoY[20];

    int    nOrder;
    int    bReversed;

    int       nGCPCount;
    GDAL_GCP *pasGCPList;
    int    bRefine;
    int    nMinimumGcps;
    double dfTolerance;
    
} GCPTransformInfo;

CPL_C_START
CPLXMLNode *GDALSerializeGCPTransformer( void *pTransformArg );
void *GDALDeserializeGCPTransformer( CPLXMLNode *psTree );
CPL_C_END

/* crs.c */
static int CRS_georef(double, double, double *, double *, 
                              double [], double [], int);
static int CRS_compute_georef_equations(struct Control_Points *,
    double [], double [], double [], double [], int);
static int remove_outliers(GCPTransformInfo *);

static char *CRS_error_message[] = {
    "Failed to compute GCP transform: Not enough points available",
    "Failed to compute GCP transform: Transform is not solvable",
    "Failed to compute GCP transform: Not enough memory",
    "Failed to compute GCP transform: Parameter error",
    "Failed to compute GCP transform: Internal error"
};


/************************************************************************/
/*                      GDALCreateGCPTransformer()                      */
/************************************************************************/

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
 * This function may fail (returning NULL) if the provided set of GCPs
 * are inadequate for the requested order, the determinate is zero or they
 * are otherwise "ill conditioned".  
 *
 * Note that 2nd order requires at least 6 GCPs, and 3rd order requires at
 * least 10 gcps.  If nReqOrder is 0 the highest order possible with the
 * provided gcp count will be used.
 *
 * @param nGCPCount the number of GCPs in pasGCPList.
 * @param pasGCPList an array of GCPs to be used as input.
 * @param nReqOrder the requested polynomial order.  It should be 1, 2 or 3.
 * 
 * @return the transform argument or NULL if creation fails. 
 */

void *GDALCreateGCPTransformerEx( int nGCPCount, const GDAL_GCP *pasGCPList, 
                                int nReqOrder, int bReversed, int bRefine, double dfTolerance, int nMinimumGcps)

{
    GCPTransformInfo *psInfo;
    double *padfGeoX, *padfGeoY, *padfRasterX, *padfRasterY;
    int    *panStatus, iGCP;
    int    nCRSresult;
    struct Control_Points sPoints;

    if( nReqOrder == 0 )
    {
        if( nGCPCount >= 10 )
            nReqOrder = 2; /*for now we avoid 3rd order since it is unstable*/
        else if( nGCPCount >= 6 )
            nReqOrder = 2;
        else
            nReqOrder = 1;
    }
    
    psInfo = (GCPTransformInfo *) CPLCalloc(sizeof(GCPTransformInfo),1);
    psInfo->bReversed = bReversed;
    psInfo->nOrder = nReqOrder;
    psInfo->bRefine = bRefine;
    psInfo->dfTolerance = dfTolerance;
    psInfo->nMinimumGcps = nMinimumGcps;

    psInfo->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );
    psInfo->nGCPCount = nGCPCount;

    strcpy( psInfo->sTI.szSignature, "GTI" );
    psInfo->sTI.pszClassName = "GDALGCPTransformer";
    psInfo->sTI.pfnTransform = GDALGCPTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyGCPTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeGCPTransformer;
    
/* -------------------------------------------------------------------- */
/*      Compute the forward and reverse polynomials.                    */
/* -------------------------------------------------------------------- */

    if(bRefine)
    {
        nCRSresult = remove_outliers(psInfo);
    }
    else
    {
        /* -------------------------------------------------------------------- */
        /*      Allocate and initialize the working points list.                */
        /* -------------------------------------------------------------------- */
        padfGeoX = (double *) CPLCalloc(sizeof(double),nGCPCount);
        padfGeoY = (double *) CPLCalloc(sizeof(double),nGCPCount);
        padfRasterX = (double *) CPLCalloc(sizeof(double),nGCPCount);
        padfRasterY = (double *) CPLCalloc(sizeof(double),nGCPCount);
        panStatus = (int *) CPLCalloc(sizeof(int),nGCPCount);
    
        for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
        {
            panStatus[iGCP] = 1;
            padfGeoX[iGCP] = pasGCPList[iGCP].dfGCPX;
            padfGeoY[iGCP] = pasGCPList[iGCP].dfGCPY;
            padfRasterX[iGCP] = pasGCPList[iGCP].dfGCPPixel;
            padfRasterY[iGCP] = pasGCPList[iGCP].dfGCPLine;
        }

        sPoints.count = nGCPCount;
        sPoints.e1 = padfRasterX;
        sPoints.n1 = padfRasterY;
        sPoints.e2 = padfGeoX;
        sPoints.n2 = padfGeoY;
        sPoints.status = panStatus;
        nCRSresult = CRS_compute_georef_equations( &sPoints,
                                                psInfo->adfToGeoX, psInfo->adfToGeoY,
                                                psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                                                nReqOrder );
        CPLFree( padfGeoX );
        CPLFree( padfGeoY );
        CPLFree( padfRasterX );
        CPLFree( padfRasterY );
        CPLFree( panStatus );
    }

    if (nCRSresult != 1)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "%s", CRS_error_message[-nCRSresult]);
        GDALDestroyGCPTransformer( psInfo );
        return NULL;
    }
    else
    {
        return psInfo;
    }
}

void *GDALCreateGCPTransformer( int nGCPCount, const GDAL_GCP *pasGCPList, 
                                int nReqOrder, int bReversed )

{
    return GDALCreateGCPTransformerEx(nGCPCount, pasGCPList, nReqOrder, bReversed, FALSE, -1, -1);
}

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
    GCPTransformInfo *psInfo = (GCPTransformInfo *) pTransformArg;

    VALIDATE_POINTER0( pTransformArg, "GDALDestroyGCPTransformer" );

    GDALDeinitGCPs( psInfo->nGCPCount, psInfo->pasGCPList );
    CPLFree( psInfo->pasGCPList );

    CPLFree( pTransformArg );
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
    int    i;
    GCPTransformInfo *psInfo = (GCPTransformInfo *) pTransformArg;

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
            CRS_georef( x[i], y[i], x + i, y + i, 
                        psInfo->adfFromGeoX, psInfo->adfFromGeoY, 
                        psInfo->nOrder );
        }
        else
        {
            CRS_georef( x[i], y[i], x + i, y + i, 
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
    CPLXMLNode *psTree;
    GCPTransformInfo *psInfo = (GCPTransformInfo *) pTransformArg;

    VALIDATE_POINTER1( pTransformArg, "GDALSerializeGCPTransformer", NULL );

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "GCPTransformer" );

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
/*	Attach GCP List. 						*/
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
                                   NULL );
    }

    return psTree;
}

/************************************************************************/
/*               GDALDeserializeReprojectionTransformer()               */
/************************************************************************/

void *GDALDeserializeGCPTransformer( CPLXMLNode *psTree )

{
    GDAL_GCP *pasGCPList = 0;
    int nGCPCount = 0;
    void *pResult;
    int nReqOrder;
    int bReversed;
    int bRefine;
    int nMinimumGcps;
    double dfTolerance;

    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != NULL )
    {
        GDALDeserializeGCPListFromXML( psGCPList,
                                       &pasGCPList,
                                       &nGCPCount,
                                       NULL );
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    nReqOrder = atoi(CPLGetXMLValue(psTree,"Order","3"));
    bReversed = atoi(CPLGetXMLValue(psTree,"Reversed","0"));
    bRefine = atoi(CPLGetXMLValue(psTree,"Refine","0"));
    nMinimumGcps = atoi(CPLGetXMLValue(psTree,"MinimumGcps","6"));
    dfTolerance = atof(CPLGetXMLValue(psTree,"Tolerance","1.0"));

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


#define MSUCCESS     1 /* SUCCESS */
#define MNPTERR      0 /* NOT ENOUGH POINTS */
#define MUNSOLVABLE -1 /* NOT SOLVABLE */
#define MMEMERR     -2 /* NOT ENOUGH MEMORY */
#define MPARMERR    -3 /* PARAMETER ERROR */
#define MINTERR     -4 /* INTERNAL ERROR */

/***************************************************************************/
/*
    FUNCTION PROTOTYPES FOR STATIC (INTERNAL) FUNCTIONS
*/
/***************************************************************************/

static int calccoef(struct Control_Points *,double *,double *,int);
static int calcls(struct Control_Points *,struct MATRIX *,
                  double *,double *,double *,double *);
static int exactdet(struct Control_Points *,struct MATRIX *,
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
  double e3, e2n, en2, n3, e2, en, n2;

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
CRS_compute_georef_equations (struct Control_Points *cp, 
                                      double E12[], double N12[], 
                                      double E21[], double N21[], 
                                      int order)
{
    double *tempptr;
    int status;

    if(order < 1 || order > MAXORDER)
        return(MPARMERR);

    /* CALCULATE THE FORWARD TRANSFORMATION COEFFICIENTS */

    status = calccoef(cp,E12,N12,order);
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

    status = calccoef(cp,E21,N21,order);

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
calccoef (struct Control_Points *cp, double E[], double N[], int order)
{
    struct MATRIX m;
    double *a;
    double *b;
    int numactive;   /* NUMBER OF ACTIVE CONTROL POINTS */
    int status, i;

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

    m.v = (double *)CPLCalloc(m.n*m.n,sizeof(double));
    if(m.v == NULL)
    {
        return(MMEMERR);
    }
    a = (double *)CPLCalloc(m.n,sizeof(double));
    if(a == NULL)
    {
        CPLFree((char *)m.v);
        return(MMEMERR);
    }
    b = (double *)CPLCalloc(m.n,sizeof(double));
    if(b == NULL)
    {
        CPLFree((char *)m.v);
        CPLFree((char *)a);
        return(MMEMERR);
    }

    if(numactive == m.n)
        status = exactdet(cp,&m,a,b,E,N);
    else
        status = calcls(cp,&m,a,b,E,N);

    CPLFree((char *)m.v);
    CPLFree((char *)a);
    CPLFree((char *)b);

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
    double a[],
    double b[],
    double E[],     /* EASTING COEFFICIENTS */
    double N[]     /* NORTHING COEFFICIENTS */
)
  {
  int pntnow, currow, j;

  currow = 1;
  for(pntnow = 0 ; pntnow < cp->count ; pntnow++)
    {
    if(cp->status[pntnow] > 0)
      {
      /* POPULATE MATRIX M */

      for(j = 1 ; j <= m->n ; j++)
        {
        M(currow,j) = term(j,cp->e1[pntnow],cp->n1[pntnow]);
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
    double a[],
    double b[],
    double E[],     /* EASTING COEFFICIENTS */
    double N[]     /* NORTHING COEFFICIENTS */
)
{
    int i, j, n, numactive = 0;

    /* INITIALIZE THE UPPER HALF OF THE MATRIX AND THE TWO COLUMN VECTORS */

    for(i = 1 ; i <= m->n ; i++)
    {
        for(j = i ; j <= m->n ; j++)
            M(i,j) = 0.0;
        a[i-1] = b[i-1] = 0.0;
    }

    /* SUM THE UPPER HALF OF THE MATRIX AND THE COLUMN VECTORS ACCORDING TO
       THE LEAST SQUARES METHOD OF SOLVING OVER DETERMINED SYSTEMS */

    for(n = 0 ; n < cp->count ; n++)
    {
        if(cp->status[n] > 0)
        {
            numactive++;
            for(i = 1 ; i <= m->n ; i++)
            {
                for(j = i ; j <= m->n ; j++)
                    M(i,j) += term(i,cp->e1[n],cp->n1[n]) * term(j,cp->e1[n],cp->n1[n]);

                a[i-1] += cp->e2[n] * term(i,cp->e1[n],cp->n1[n]);
                b[i-1] += cp->n2[n] * term(i,cp->e1[n],cp->n1[n]);
            }
        }
    }

    if(numactive <= m->n)
        return(MINTERR);

    /* TRANSPOSE VALUES IN UPPER HALF OF M TO OTHER HALF */

    for(i = 2 ; i <= m->n ; i++)
    {
        for(j = 1 ; j < i ; j++)
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

static double term (int term, double e, double n)
{
    switch(term)
    {
      case  1: return((double)1.0);
      case  2: return((double)e);
      case  3: return((double)n);
      case  4: return((double)(e*e));
      case  5: return((double)(e*n));
      case  6: return((double)(n*n));
      case  7: return((double)(e*e*e));
      case  8: return((double)(e*e*n));
      case  9: return((double)(e*n*n));
      case 10: return((double)(n*n*n));
    }
    return((double)0.0);
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
    int i, j, i2, j2, imark;
    double factor, temp;
    double  pivot;  /* ACTUAL VALUE OF THE LARGEST PIVOT CANDIDATE */

    for(i = 1 ; i <= m->n ; i++)
    {
        j = i;

        /* find row with largest magnitude value for pivot value */

        pivot = M(i,j);
        imark = i;
        for(i2 = i + 1 ; i2 <= m->n ; i2++)
        {
            temp = fabs(M(i2,j));
            if(temp > fabs(pivot))
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
            for(j2 = 1 ; j2 <= m->n ; j2++)
            {
                temp = M(imark,j2);
                M(imark,j2) = M(i,j2);
                M(i,j2) = temp;
            }

            temp = a[imark-1];
            a[imark-1] = a[i-1];
            a[i-1] = temp;

            temp = b[imark-1];
            b[imark-1] = b[i-1];
            b[i-1] = temp;
        }

        /* compute zeros above and below the pivot, and compute
           values for the rest of the row as well */

        for(i2 = 1 ; i2 <= m->n ; i2++)
        {
            if(i2 != i)
            {
                factor = M(i2,j) / pivot;
                for(j2 = j ; j2 <= m->n ; j2++)
                    M(i2,j2) -= factor * M(i,j2);
                a[i2-1] -= factor * a[i-1];
                b[i2-1] -= factor * b[i-1];
            }
        }
    }

    /* SINCE ALL OTHER VALUES IN THE MATRIX ARE ZERO NOW, CALCULATE THE
       COEFFICIENTS BY DIVIDING THE COLUMN VECTORS BY THE DIAGONAL VALUES. */

    for(i = 1 ; i <= m->n ; i++)
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
static int worst_outlier(struct Control_Points *cp, double E[], double N[], double dfTolerance)
{
    double *padfResiduals;
    int nI, nIndex;
    double dfDifference, dfSampleResidual, dfLineResidual, dfSampleRes, dfLineRes, dfCurrentDifference;
    double dfE1, dfN1, dfE2, dfN2, dfEn;
  
    padfResiduals = (double *) CPLCalloc(sizeof(double),cp->count);
    dfSampleResidual = 0.0;
    dfLineResidual = 0.0;
  
    for(nI = 0; nI < cp->count; nI++)
    {
        dfE1 = cp->e1[nI];
        dfN1 = cp->n1[nI];
        dfE2 = dfE1 * dfE1;
        dfN2 = dfN1 * dfN1;
        dfEn = dfE1 * dfN1;

        dfSampleRes = E[0] + E[1] * dfE1 + E[2] * dfN1 + E[3] * dfE2 + E[4] * dfEn + E[5] * dfN2 - cp->e2[nI];
        dfLineRes = N[0] + N[1] * dfE1 + N[2] * dfN1 + N[3] * dfE2 + N[4] * dfEn + N[5] * dfN2 - cp->n2[nI];
    
        dfSampleResidual += dfSampleRes*dfSampleRes;
        dfLineResidual += dfLineRes*dfLineRes;
    
        padfResiduals[nI] = sqrt(dfSampleRes*dfSampleRes + dfLineRes*dfLineRes);
    }
  
    nIndex = -1;
    dfDifference = -1.0;
    for(nI = 0; nI < cp->count; nI++)
    {
        dfCurrentDifference = padfResiduals[nI];
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
  2. THE GCP LIST WILL BE SCANED TO DETERMINE THE WORST OUTLIER USING
     THE CALCULATED COEFFICIENTS.
  3. THE WORST OUTLIER WILL BE REMOVED FROM THE GCP LIST.
  4. THE COEFFICIENTS WILL BE RECALCULATED WITHOUT THE WORST OUTLIER.
  5. STEP 1 TO 4 ARE EXECUTED UNTIL THE MINIMUM NUMBER OF GCPS WERE REACHED
     OR IF NO GCP IS CONSIDERED AN OUTLIER WITH THE PASSED TOLERANCE.
*/
/***************************************************************************/
static int remove_outliers( GCPTransformInfo *psInfo )
{
    double *padfGeoX, *padfGeoY, *padfRasterX, *padfRasterY;
    int *panStatus;
    int nI, nCRSresult, nGCPCount, nMinimumGcps, nReqOrder;
    double dfTolerance;
    struct Control_Points sPoints;
    
    nGCPCount = psInfo->nGCPCount;
    nMinimumGcps = psInfo->nMinimumGcps;
    nReqOrder = psInfo->nOrder;
    dfTolerance = psInfo->dfTolerance;
    
    padfGeoX = (double *) CPLCalloc(sizeof(double),nGCPCount);
    padfGeoY = (double *) CPLCalloc(sizeof(double),nGCPCount);
    padfRasterX = (double *) CPLCalloc(sizeof(double),nGCPCount);
    padfRasterY = (double *) CPLCalloc(sizeof(double),nGCPCount);
    panStatus = (int *) CPLCalloc(sizeof(int),nGCPCount);
    
    for( nI = 0; nI < nGCPCount; nI++ )
    {
        panStatus[nI] = 1;
        padfGeoX[nI] = psInfo->pasGCPList[nI].dfGCPX;
        padfGeoY[nI] = psInfo->pasGCPList[nI].dfGCPY;
        padfRasterX[nI] = psInfo->pasGCPList[nI].dfGCPPixel;
        padfRasterY[nI] = psInfo->pasGCPList[nI].dfGCPLine;
    }

    sPoints.count = nGCPCount;
    sPoints.e1 = padfRasterX;
    sPoints.n1 = padfRasterY;
    sPoints.e2 = padfGeoX;
    sPoints.n2 = padfGeoY;
    sPoints.status = panStatus;
  
    nCRSresult = CRS_compute_georef_equations( &sPoints,
                                      psInfo->adfToGeoX, psInfo->adfToGeoY,
                                      psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                                      nReqOrder );

    while(sPoints.count > nMinimumGcps)
    {
        int nIndex;

        nIndex = worst_outlier(&sPoints, psInfo->adfFromGeoX, psInfo->adfFromGeoY, dfTolerance);

        //If no outliers were detected, stop the GCP elimination
        if(nIndex == -1)
        {
            break;
        }

        CPLFree(psInfo->pasGCPList[nIndex].pszId);
        CPLFree(psInfo->pasGCPList[nIndex].pszInfo);

        for( nI = nIndex; nI < sPoints.count - 1; nI++ )
        {
            sPoints.e1[nI] = sPoints.e1[nI + 1];
            sPoints.n1[nI] = sPoints.n1[nI + 1];
            sPoints.e2[nI] = sPoints.e2[nI + 1];
            sPoints.n2[nI] = sPoints.n2[nI + 1];
            psInfo->pasGCPList[nI].pszId = psInfo->pasGCPList[nI + 1].pszId;
            psInfo->pasGCPList[nI].pszInfo = psInfo->pasGCPList[nI + 1].pszInfo;
        }

        sPoints.count = sPoints.count - 1;

        nCRSresult = CRS_compute_georef_equations( &sPoints,
                                      psInfo->adfToGeoX, psInfo->adfToGeoY,
                                      psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                                      nReqOrder );
    }

    for( nI = 0; nI < sPoints.count; nI++ )
    {
        psInfo->pasGCPList[nI].dfGCPX = sPoints.e2[nI];
        psInfo->pasGCPList[nI].dfGCPY = sPoints.n2[nI];
        psInfo->pasGCPList[nI].dfGCPPixel = sPoints.e1[nI];
        psInfo->pasGCPList[nI].dfGCPLine = sPoints.n1[nI];
    }
    psInfo->nGCPCount = sPoints.count;
    
    CPLFree( sPoints.e1 );
    CPLFree( sPoints.n1 );
    CPLFree( sPoints.e2 );
    CPLFree( sPoints.n2 );
    CPLFree( sPoints.status );
    return nCRSresult;
}
