/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of high level convenience APIs for warper.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.3  2003/02/21 15:41:55  warmerda
 * rename data member
 *
 * Revision 1.2  2003/02/20 21:53:06  warmerda
 * partial implementation
 *
 * Revision 1.1  2003/02/18 17:25:50  warmerda
 * New
 *
 */

#include "gdalwarper.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         GDALReprojectImage()                         */
/************************************************************************/

/**
 * Reproject image.
 *
 * This is a convenience function utilizing the GDALWarpOperation class to
 * reproject an image from a source to a destination.  In particular, this
 * function takes care of establishing the transformation function to
 * implement the reprojection, and will default a variety of other 
 * warp options. 
 *
 * By default all bands are transferred, with no masking or nodata values
 * in effect.  No metadata, projection info, or color tables are transferred 
 * to the output file. 
 *
 * @param hSrcDS the source image file. 
 * @param pszSrcWKT the source projection.  If NULL the source projection
 * is read from from hSrcDS.
 * @param hDstDS the destination image file. 
 * @param pszDstWKT the destination projection.  If NULL the destination
 * projection will be read from hDstDS.
 * @param eResampleAlg the type of resampling to use.  
 * @param dfWarpMemoryLimit the amount of memory (in bytes) that the warp
 * API is allowed to use for caching.  This is in addition to the memory
 * already allocated to the GDAL caching (as per GDALSetCacheMax()).  May be
 * 0.0 to use default memory settings.
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL. 
 * @param pProgressArg argument to be passed to pfnProgress.  May be NULL.
 * @param psOptions warp options, normally NULL.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr GDALReprojectImage( GDALDatasetH hSrcDS, const char *pszSrcWKT, 
                           GDALDatasetH hDstDS, const char *pszDstWKT,
                           GDALResampleAlg eResampleAlg, 
                           double dfWarpMemoryLimit,
                           GDALProgressFunc pfnProgress, void *pProgressArg, 
                           GDALWarpOptions *psOptions )

{
    GDALWarpOptions *psWOptions;

/* -------------------------------------------------------------------- */
/*      Default a few parameters.                                       */
/* -------------------------------------------------------------------- */
    if( pszSrcWKT == NULL )
        pszSrcWKT = GDALGetProjectionRef( hSrcDS );

    if( pszDstWKT == NULL )
        pszDstWKT = pszSrcWKT;

/* -------------------------------------------------------------------- */
/*      Setup a reprojection based transformer.                         */
/* -------------------------------------------------------------------- */
    void *hTransformArg;

    hTransformArg = 
        GDALCreateGenImgProjTransformer( hSrcDS, pszSrcWKT, hDstDS, pszDstWKT, 
                                         TRUE, 1000.0, 2 );

    if( hTransformArg == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Create a copy of the user provided options, or a defaulted      */
/*      options structure.                                              */
/* -------------------------------------------------------------------- */
    if( psOptions == NULL )
        psWOptions = GDALCreateWarpOptions();
    else
        psWOptions = GDALCloneWarpOptions( psOptions );

/* -------------------------------------------------------------------- */
/*      Set transform.                                                  */
/* -------------------------------------------------------------------- */
    psWOptions->pfnTransformer = GDALGenImgProjTransform;
    psWOptions->pTransformerArg = hTransformArg;

/* -------------------------------------------------------------------- */
/*      Set file and band mapping.                                      */
/* -------------------------------------------------------------------- */
    int  iBand;

    psWOptions->hSrcDS = hSrcDS;
    psWOptions->hDstDS = hDstDS;

    if( psWOptions->nBandCount == 0 )
    {
        psWOptions->nBandCount = MIN(GDALGetRasterCount(hSrcDS),
                                     GDALGetRasterCount(hDstDS));
        
        psWOptions->panSrcBands = (int *) 
            CPLMalloc(sizeof(int) * psWOptions->nBandCount);
        psWOptions->panDstBands = (int *) 
            CPLMalloc(sizeof(int) * psWOptions->nBandCount);

        for( iBand = 0; iBand < psWOptions->nBandCount; iBand++ )
        {
            psWOptions->panSrcBands[iBand] = iBand+1;
            psWOptions->panDstBands[iBand] = iBand+1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set source nodata values if the source dataset seems to have    */
/*      any.                                                            */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < psWOptions->nBandCount; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, iBand+1 );
        int             bGotNoData = FALSE;
        double          dfNoDataValue;
        
        dfNoDataValue = GDALGetRasterNoDataValue( hBand, &bGotNoData );
        if( bGotNoData )
        {
            if( psWOptions->padfSrcNoDataReal == NULL )
            {
                int  ii;

                psWOptions->padfSrcNoDataReal = (double *) 
                    CPLMalloc(sizeof(double) * psWOptions->nBandCount);
                psWOptions->padfSrcNoDataImag = (double *) 
                    CPLMalloc(sizeof(double) * psWOptions->nBandCount);

                for( ii = 0; ii < psWOptions->nBandCount; ii++ )
                {
                    psWOptions->padfSrcNoDataReal[ii] = -1.1e20;
                    psWOptions->padfSrcNoDataReal[ii] = 0.0;
                }
            }

            psWOptions->padfSrcNoDataReal[iBand] = dfNoDataValue;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the progress function.                                      */
/* -------------------------------------------------------------------- */
    if( pfnProgress != NULL )
    {
        psWOptions->pfnProgress = pfnProgress;
        psWOptions->pProgressArg = pProgressArg;
    }

/* -------------------------------------------------------------------- */
/*      Create a warp options based on the options.                     */
/* -------------------------------------------------------------------- */
    GDALWarpOperation  oWarper;
    CPLErr eErr;

    eErr = oWarper.Initialize( psWOptions );

    if( eErr == CE_None )
        eErr = oWarper.ChunkAndWarpImage( 0, 0, 
                                          GDALGetRasterXSize(hDstDS),
                                          GDALGetRasterYSize(hDstDS) );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    GDALDestroyWarpOptions( psWOptions );

    GDALDestroyGenImgProjTransformer( hTransformArg );

    return eErr;
}

/************************************************************************/
/*                    GDALCreateAndReprojectImage()                     */
/*                                                                      */
/*      This is a "quicky" reprojection API.                            */
/************************************************************************/

CPLErr GDALCreateAndReprojectImage( 
    GDALDatasetH hSrcDS, const char *pszSrcWKT, 
    const char *pszDstFilename, GDALDriverH hDstDriver, const char *pszDstWKT,
    GDALResampleAlg eResampleAlg, double dfWarpMemoryLimit,
    GDALProgressFunc pfnProgress, void *pProgressArg, 
    GDALWarpOptions *psOptions )
    
{
/* -------------------------------------------------------------------- */
/*      Default a few parameters.                                       */
/* -------------------------------------------------------------------- */
    if( hDstDriver == NULL )
        hDstDriver = GDALGetDriverByName( "GTiff" );

    if( pszSrcWKT == NULL )
        pszSrcWKT = GDALGetProjectionRef( hSrcDS );

    if( pszDstWKT == NULL )
        pszDstWKT = pszSrcWKT;

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    void *hTransformArg;

    hTransformArg = 
        GDALCreateGenImgProjTransformer( hSrcDS, pszSrcWKT, NULL, pszDstWKT, 
                                         TRUE, 1000.0, 2 );

    if( hTransformArg == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Get approximate output definition.                              */
/* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6];
    int    nPixels, nLines;

    if( GDALSuggestedWarpOutput( hSrcDS, 
                                 GDALGenImgProjTransform, hTransformArg, 
                                 adfDstGeoTransform, &nPixels, &nLines )
        != CE_None )
        return CE_Failure;

    GDALDestroyGenImgProjTransformer( hTransformArg );

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS;

    hDstDS = GDALCreate( hDstDriver, pszDstFilename, nPixels, nLines, 
                         GDALGetRasterCount(hSrcDS),
                         GDALGetRasterDataType(GDALGetRasterBand(hSrcDS,1)),
                         NULL );

    if( hDstDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    GDALSetProjection( hDstDS, pszDstWKT );
    GDALSetGeoTransform( hDstDS, adfDstGeoTransform );

/* -------------------------------------------------------------------- */
/*      Perform the reprojection.                                       */
/* -------------------------------------------------------------------- */
    CPLErr eErr ;

    eErr = 
        GDALReprojectImage( hSrcDS, pszSrcWKT, hDstDS, pszDstWKT, 
                            eResampleAlg, dfWarpMemoryLimit, 
                            pfnProgress, pProgressArg, psOptions );

    GDALClose( hDstDS );

    return eErr;
}


/************************************************************************/
/* ==================================================================== */
/*                           GDALWarpOptions                            */
/* ==================================================================== */
/************************************************************************/

/**
 * \var char **GDALWarpOptions::papszWarpOptions;
 *
 * A string list of additional options controlling the warp operation in
 * name=value format.  A suitable string list can be prepared with 
 * CSLSetNameValue().
 *
 * The following values are currently supported:
 * 
 *  - INIT_DEST=[value] or INIT_DEST=NO_DATA: This option forces the 
 * destination image to be initialized to the indicated value (for all bands)
 * or indicates that it should be initialized to the NO_DATA value in
 * padfDstNoDataReal/padfDstNoDataImag.  If this value isn't set the
 * destination image will be read and overlayed.  
 *
 */

/************************************************************************/
/*                       GDALCreateWarpOptions()                        */
/************************************************************************/

GDALWarpOptions *GDALCreateWarpOptions()

{
    GDALWarpOptions *psOptions;

    psOptions = (GDALWarpOptions *) CPLCalloc(sizeof(GDALWarpOptions),1);

    psOptions->eResampleAlg = GRA_NearestNeighbour;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->eWorkingDataType = GDT_Unknown;

    return psOptions;
}

/************************************************************************/
/*                       GDALDestroyWarpOptions()                       */
/************************************************************************/

void GDALDestroyWarpOptions( GDALWarpOptions *psOptions )

{
    CSLDestroy( psOptions->papszWarpOptions );
    CPLFree( psOptions->panSrcBands );
    CPLFree( psOptions->panDstBands );
    CPLFree( psOptions->padfSrcNoDataReal );
    CPLFree( psOptions->padfSrcNoDataImag );
    CPLFree( psOptions->padfDstNoDataReal );
    CPLFree( psOptions->padfDstNoDataImag );
    CPLFree( psOptions->papfnSrcPerBandValidityMaskFunc );
    CPLFree( psOptions->papSrcPerBandValidityMaskFuncArg );

    CPLFree( psOptions );
}


#define COPY_MEM(target,type,count)					\
   if( (psSrcOptions->target) != NULL && (count) != 0 ) 		\
   { 									\
       (psDstOptions->target) = (type *) CPLMalloc(sizeof(type)*count); \
       memcpy( (psDstOptions->target), (psSrcOptions->target),		\
 	       sizeof(type) * count ); 	        			\
   }

/************************************************************************/
/*                        GDALCloneWarpOptions()                        */
/************************************************************************/

GDALWarpOptions *GDALCloneWarpOptions( const GDALWarpOptions *psSrcOptions )

{
    GDALWarpOptions *psDstOptions = GDALCreateWarpOptions();

    memcpy( psDstOptions, psSrcOptions, sizeof(GDALWarpOptions) );

    if( psSrcOptions->papszWarpOptions != NULL )
        psDstOptions->papszWarpOptions = 
            CSLDuplicate( psSrcOptions->papszWarpOptions );

    COPY_MEM( panSrcBands, int, psSrcOptions->nBandCount );
    COPY_MEM( panDstBands, int, psSrcOptions->nBandCount );
    COPY_MEM( padfSrcNoDataReal, double, psSrcOptions->nBandCount );
    COPY_MEM( padfSrcNoDataImag, double, psSrcOptions->nBandCount );
    COPY_MEM( papfnSrcPerBandValidityMaskFunc, GDALMaskFunc, 
              psSrcOptions->nBandCount );

    return psDstOptions;
}
