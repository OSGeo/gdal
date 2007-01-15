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


    Copyright (c) 1992, Michigan State University
   
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

 ***************************************************************************
 *
 * $Log$
 * Revision 1.10  2006/12/18 21:35:01  hobu
 * Apply Schuyler's patch for bug 1392
 *
 * Revision 1.9  2006/05/10 19:24:46  fwarmerdam
 * avoid processing HUGE_VAL coordinates
 *
 * Revision 1.8  2004/12/26 16:12:21  fwarmerdam
 * thin plate spline support now implemented
 *
 * Revision 1.7  2004/08/11 19:01:56  warmerda
 * default to 2nd order if we have enough points for 3rd order but 0 requested
 *
 * Revision 1.6  2004/08/09 14:38:27  warmerda
 * added serialize/deserialize support for warpoptions and transformers
 *
 * Revision 1.5  2004/03/19 15:22:02  warmerda
 * Fixed double free of padfRasterX. Submitted by Scott Reynolds.
 *
 * Revision 1.4  2002/12/07 17:09:50  warmerda
 * re-enable 3rd order even though unstable
 *
 * Revision 1.3  2002/12/06 17:58:00  warmerda
 * fix a few bugs
 *
 * Revision 1.2  2002/12/05 05:46:08  warmerda
 * fixed linkage problem
 *
 * Revision 1.1  2002/12/05 05:43:23  warmerda
 * New
 * 
 */

#include "gdal_alg.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

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

CPL_C_START
CPLXMLNode *GDALSerializeGCPTransformer( void *pTransformArg );
void *GDALDeserializeGCPTransformer( CPLXMLNode *psTree );
CPL_C_END

/* crs.c */
static int CRS_georef(double, double, double *, double *, 
                              double [], double [], int);
static int CRS_compute_georef_equations(struct Control_Points *,
    double [], double [], double [], double [], int);


static char *CRS_error_message[] = {
    "Failed to compute GCP transform: Not enough points available",
    "Failed to compute GCP transform: Transform is not solvable",
    "Failed to compute GCP transform: Not enough memory"
    "Failed to compute GCP transform: Parameter error",
    "Failed to compute GCP transform: Internal error"
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
    
} GCPTransformInfo;


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

void *GDALCreateGCPTransformer( int nGCPCount, const GDAL_GCP *pasGCPList, 
                                int nReqOrder, int bReversed )

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

    psInfo->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );
    psInfo->nGCPCount = nGCPCount;

    strcpy( psInfo->sTI.szSignature, "GTI" );
    psInfo->sTI.pszClassName = "GDALGCPTransformer";
    psInfo->sTI.pfnTransform = GDALGCPTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyGCPTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeGCPTransformer;

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
    
/* -------------------------------------------------------------------- */
/*      Compute the forward and reverse polynomials.                    */
/* -------------------------------------------------------------------- */
    nCRSresult = CRS_compute_georef_equations( &sPoints,
                                      psInfo->adfToGeoX, psInfo->adfToGeoY,
                                      psInfo->adfFromGeoX, psInfo->adfFromGeoY,
                                      nReqOrder );
    if (nCRSresult != 1)
    {
        CPLError( CE_Failure, CPLE_AppDefined, CRS_error_message[-nCRSresult]);
        goto CleanupAfterError;
    }
    
    return psInfo;

  CleanupAfterError:
    CPLFree( padfGeoX );
    CPLFree( padfGeoY );
    CPLFree( padfRasterX );
    CPLFree( padfRasterY );
    CPLFree( panStatus );
    
    CPLFree( psInfo );
    return NULL;
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
                      double *x, double *y, double *z, 
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
                                 
/* -------------------------------------------------------------------- */
/*	Attach GCP List. 						*/
/* -------------------------------------------------------------------- */
    if( psInfo->nGCPCount > 0 )
    {
        int iGCP;
        CPLXMLNode *psGCPList = CPLCreateXMLNode( psTree, CXT_Element, 
                                                  "GCPList" );

        for( iGCP = 0; iGCP < psInfo->nGCPCount; iGCP++ )
        {
            CPLXMLNode *psXMLGCP;
            GDAL_GCP *psGCP = psInfo->pasGCPList + iGCP;

            psXMLGCP = CPLCreateXMLNode( psGCPList, CXT_Element, "GCP" );

            CPLSetXMLValue( psXMLGCP, "#Id", psGCP->pszId );

            if( psGCP->pszInfo != NULL && strlen(psGCP->pszInfo) > 0 )
                CPLSetXMLValue( psXMLGCP, "Info", psGCP->pszInfo );

            CPLSetXMLValue( psXMLGCP, "#Pixel", 
                            CPLSPrintf( "%.4f", psGCP->dfGCPPixel ) );

            CPLSetXMLValue( psXMLGCP, "#Line", 
                            CPLSPrintf( "%.4f", psGCP->dfGCPLine ) );

            CPLSetXMLValue( psXMLGCP, "#X", 
                            CPLSPrintf( "%.12E", psGCP->dfGCPX ) );

            CPLSetXMLValue( psXMLGCP, "#Y", 
                            CPLSPrintf( "%.12E", psGCP->dfGCPY ) );

            if( psGCP->dfGCPZ != 0.0 )
                CPLSetXMLValue( psXMLGCP, "#GCPZ", 
                                CPLSPrintf( "%.12E", psGCP->dfGCPZ ) );
        }
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

    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != NULL )
    {
        int  nGCPMax = 0;
        CPLXMLNode *psXMLGCP;
         
        // Count GCPs.
        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
            nGCPMax++;
         
        pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPMax);

        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
        {
            GDAL_GCP *psGCP = pasGCPList + nGCPCount;

            if( !EQUAL(psXMLGCP->pszValue,"GCP") || 
                psXMLGCP->eType != CXT_Element )
                continue;
             
            GDALInitGCPs( 1, psGCP );
             
            CPLFree( psGCP->pszId );
            psGCP->pszId = CPLStrdup(CPLGetXMLValue(psXMLGCP,"Id",""));
             
            CPLFree( psGCP->pszInfo );
            psGCP->pszInfo = CPLStrdup(CPLGetXMLValue(psXMLGCP,"Info",""));
             
            psGCP->dfGCPPixel = atof(CPLGetXMLValue(psXMLGCP,"Pixel","0.0"));
            psGCP->dfGCPLine = atof(CPLGetXMLValue(psXMLGCP,"Line","0.0"));
             
            psGCP->dfGCPX = atof(CPLGetXMLValue(psXMLGCP,"X","0.0"));
            psGCP->dfGCPY = atof(CPLGetXMLValue(psXMLGCP,"Y","0.0"));
            psGCP->dfGCPZ = atof(CPLGetXMLValue(psXMLGCP,"Z","0.0"));
            nGCPCount++;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    nReqOrder = atoi(CPLGetXMLValue(psTree,"Order","3"));
    bReversed = atoi(CPLGetXMLValue(psTree,"Reversed","0"));

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    pResult = GDALCreateGCPTransformer( nGCPCount, pasGCPList, nReqOrder, 
                                        bReversed );
    
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
      break;
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
