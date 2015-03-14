/******************************************************************************
 * $Id$
 *
 * Name:     Operations.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Raster Operations SWIG Interface declarations.
 * Author:   Howard Butler, hobu.inc@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Howard Butler
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
 *****************************************************************************/

%{
#include "gdalgrid.h"

#ifdef DEBUG 
typedef struct OGRLayerHS OGRLayerShadow;
typedef struct OGRGeometryHS OGRGeometryShadow;
#else
typedef void OGRLayerShadow;
typedef void OGRGeometryShadow;
#endif
%}

/************************************************************************/
/*                            TermProgress()                            */
/************************************************************************/

#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
%rename (TermProgress_nocb) GDALTermProgress_nocb;
%feature( "kwargs" ) GDALTermProgress_nocb;
%inline %{
int GDALTermProgress_nocb( double dfProgress, const char * pszMessage=NULL, void *pData=NULL ) {
  return GDALTermProgress( dfProgress, pszMessage, pData);
}
%}

%rename (TermProgress) GDALTermProgress;
%callback("%s");
int GDALTermProgress( double, const char *, void * );
%nocallback;
#endif


/************************************************************************/
/*                        ComputeMedianCutPCT()                         */
/************************************************************************/
#ifndef SWIGJAVA
%feature( "kwargs" ) ComputeMedianCutPCT;
#endif
%apply Pointer NONNULL { GDALRasterBandShadow *red, GDALRasterBandShadow *green, GDALRasterBandShadow *blue, GDALRasterBandShadow *target, GDALColorTableShadow* colors };
%inline %{
int  ComputeMedianCutPCT ( GDALRasterBandShadow *red,
                              GDALRasterBandShadow *green,
                              GDALRasterBandShadow *blue,
                              int num_colors,
                              GDALColorTableShadow* colors,
                              GDALProgressFunc callback = NULL,
                              void* callback_data=NULL) {

    CPLErrorReset();

    int err = GDALComputeMedianCutPCT( red,
                                          green,
                                          blue,
                                          NULL,
                                          num_colors,
                                          colors,
                                          callback,
                                          callback_data);
    
    return err;
}
%} 

/************************************************************************/
/*                           DitherRGB2PCT()                            */
/************************************************************************/
#ifndef SWIGJAVA
%feature( "kwargs" ) DitherRGB2PCT;
#endif
%inline %{
int  DitherRGB2PCT ( GDALRasterBandShadow *red,
                     GDALRasterBandShadow *green,
                     GDALRasterBandShadow *blue,
                     GDALRasterBandShadow *target,
                     GDALColorTableShadow *colors,
                     GDALProgressFunc callback = NULL,
                     void* callback_data=NULL) {

    CPLErrorReset();
    int err;
    err = GDALDitherRGB2PCT(  red,
                                  green,
                                  blue,
                                  target,
                                  colors,
                                  callback,
                                  callback_data);
    
    return err;
}
%}
%clear GDALRasterBandShadow *red, GDALRasterBandShadow *green, GDALRasterBandShadow *blue, GDALRasterBandShadow *target, GDALColorTableShadow* colors;

/************************************************************************/
/*                           ReprojectImage()                           */
/************************************************************************/
%apply Pointer NONNULL {GDALDatasetShadow *src_ds, GDALDatasetShadow *dst_ds};
%inline %{
CPLErr  ReprojectImage ( GDALDatasetShadow *src_ds,
                         GDALDatasetShadow *dst_ds,
                         const char *src_wkt=NULL,
                         const char *dst_wkt=NULL,
                         GDALResampleAlg eResampleAlg=GRA_NearestNeighbour,
                         double WarpMemoryLimit=0.0,
                         double maxerror = 0.0,
			 GDALProgressFunc callback = NULL,
                     	 void* callback_data=NULL) {

    CPLErrorReset();

    CPLErr err = GDALReprojectImage( src_ds,
                                     src_wkt,
                                     dst_ds,
                                     dst_wkt,
                                     eResampleAlg,
                                     WarpMemoryLimit,
                                     maxerror,
                                     callback,
                                     callback_data,
                                     NULL);
    
    return err;
}
%} 
%clear GDALDatasetShadow *src_ds, GDALDatasetShadow *dst_ds;

/************************************************************************/
/*                          ComputeProximity()                          */
/************************************************************************/
#ifndef SWIGJAVA
%feature( "kwargs" ) ComputeProximity;
#endif
%apply Pointer NONNULL {GDALRasterBandShadow *srcBand, GDALRasterBandShadow *proximityBand};
%inline %{
int  ComputeProximity( GDALRasterBandShadow *srcBand,
                       GDALRasterBandShadow *proximityBand,
                       char **options = NULL,
                       GDALProgressFunc callback=NULL,
                       void* callback_data=NULL) {

    CPLErrorReset();

    return GDALComputeProximity( srcBand, proximityBand, options,
                                 callback, callback_data );
}
%} 
%clear GDALRasterBandShadow *srcBand, GDALRasterBandShadow *proximityBand;

/************************************************************************/
/*                        RasterizeLayer()                              */
/************************************************************************/

%apply Pointer NONNULL {GDALDatasetShadow *dataset, OGRLayerShadow *layer};

#ifdef SWIGJAVA
%apply (int nList, int *pList ) { (int bands, int *band_list ) };
%apply (int nList, double *pList ) { (int burn_values, double *burn_values_list ) };
%inline %{
int  RasterizeLayer( GDALDatasetShadow *dataset,
                 int bands, int *band_list,
                 OGRLayerShadow *layer,
		 int burn_values = 0, double *burn_values_list = NULL, 
                 char **options = NULL,
                 GDALProgressFunc callback=NULL,
                 void* callback_data=NULL) {

    CPLErr eErr;

    CPLErrorReset();

    if( burn_values == 0 )
    {
        burn_values_list = (double *) CPLMalloc(sizeof(double)*bands);
        for( int i = 0; i < bands; i++ )
            burn_values_list[i] = 255.0;
    }
    else if( burn_values != bands )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Did not get the expected number of burn values in RasterizeLayer()" );
        return CE_Failure;
    }

    eErr = GDALRasterizeLayers( dataset, bands, band_list,
                                1, &layer, 
                                NULL, NULL,
                                burn_values_list, options, 
                                callback, callback_data );

    if( burn_values == 0 )
        CPLFree( burn_values_list );

    return eErr;
}
%} 
#else
%feature( "kwargs" ) RasterizeLayer;
%apply (int nList, int *pList ) { (int bands, int *band_list ) };
%apply (int nList, double *pList ) { (int burn_values, double *burn_values_list ) };
%inline %{
int  RasterizeLayer( GDALDatasetShadow *dataset,
                 int bands, int *band_list,
                 OGRLayerShadow *layer,
                 void *pfnTransformer = NULL,
                 void *pTransformArg = NULL, 
		 int burn_values = 0, double *burn_values_list = NULL, 
                 char **options = NULL,
                 GDALProgressFunc callback=NULL,
                 void* callback_data=NULL) {

    CPLErr eErr;

    CPLErrorReset();

    if( burn_values == 0 )
    {
        burn_values_list = (double *) CPLMalloc(sizeof(double)*bands);
        for( int i = 0; i < bands; i++ )
            burn_values_list[i] = 255.0;
    }
    else if( burn_values != bands )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Did not get the expected number of burn values in RasterizeLayer()" );
        return CE_Failure;
    }

    eErr = GDALRasterizeLayers( dataset, bands, band_list,
                                1, &layer, 
                                (GDALTransformerFunc) pfnTransformer, 
                                pTransformArg,
                                burn_values_list, options, 
                                callback, callback_data );

    if( burn_values == 0 )
        CPLFree( burn_values_list );

    return eErr;
}
%} 
#endif

/************************************************************************/
/*                             Polygonize()                             */
/************************************************************************/

%apply Pointer NONNULL {GDALRasterBandShadow *srcBand, OGRLayerShadow *outLayer};
#ifndef SWIGJAVA
%feature( "kwargs" ) Polygonize;
#endif
%inline %{
int  Polygonize( GDALRasterBandShadow *srcBand,
     		 GDALRasterBandShadow *maskBand,
  	         OGRLayerShadow *outLayer, 
                 int iPixValField,
                 char **options = NULL,
                 GDALProgressFunc callback=NULL,
                 void* callback_data=NULL) {

    CPLErrorReset();

    return GDALPolygonize( srcBand, maskBand, outLayer, iPixValField,
                           options, callback, callback_data );
}
%} 
%clear GDALRasterBandShadow *srcBand, OGRLayerShadow *outLayer;

/************************************************************************/
/*                             FillNodata()                             */
/************************************************************************/

/* Interface method added for GDAL 1.7.0 */
%apply Pointer NONNULL {GDALRasterBandShadow *targetBand};
#ifndef SWIGJAVA
%feature( "kwargs" ) FillNodata;
#endif
%inline %{
int  FillNodata( GDALRasterBandShadow *targetBand,
     		 GDALRasterBandShadow *maskBand,
                 double maxSearchDist,
                 int smoothingIterations,
                 char **options = NULL,
                 GDALProgressFunc callback=NULL,
                 void* callback_data=NULL) {

    CPLErrorReset();

    return GDALFillNodata( targetBand, maskBand, maxSearchDist, 
    	   		   0, smoothingIterations, options, 
			   callback, callback_data );
}
%} 
%clear GDALRasterBandShadow *targetBand;

/************************************************************************/
/*                            SieveFilter()                             */
/************************************************************************/

%apply Pointer NONNULL {GDALRasterBandShadow *srcBand, GDALRasterBandShadow *dstBand};
#ifndef SWIGJAVA
%feature( "kwargs" ) SieveFilter;
#endif
%inline %{
int  SieveFilter( GDALRasterBandShadow *srcBand,
     		  GDALRasterBandShadow *maskBand,
  	          GDALRasterBandShadow *dstBand,
                  int threshold, int connectedness=4,
                  char **options = NULL,
                  GDALProgressFunc callback=NULL,
                  void* callback_data=NULL) {

    CPLErrorReset();

    return GDALSieveFilter( srcBand, maskBand, dstBand, 
                            threshold, connectedness,
                            options, callback, callback_data );
}
%} 
%clear GDALRasterBandShadow *srcBand, GDALRasterBandShadow *dstBand;

/************************************************************************/
/*                        RegenerateOverviews()                         */
/************************************************************************/

#ifndef SWIGJAVA
%feature( "kwargs" ) RegenerateOverviews;
#endif /* SWIGJAVA */
#ifndef SWIGCSHARP
%apply (int object_list_count, GDALRasterBandShadow **poObjects) {(int overviewBandCount, GDALRasterBandShadow **overviewBands)};
#endif /* SWIGCSHARP */
#ifdef SWIGJAVA
%apply (const char* stringWithDefaultValue) {const char *resampling};
#endif /* SWIGJAVA */
%apply Pointer NONNULL { GDALRasterBandShadow* srcBand };
%inline %{
int  RegenerateOverviews( GDALRasterBandShadow *srcBand,
     			  int overviewBandCount,
                          GDALRasterBandShadow **overviewBands,
                          const char *resampling = "average",
                          GDALProgressFunc callback=NULL,
                          void* callback_data=NULL) {

    CPLErrorReset();

    return GDALRegenerateOverviews( srcBand, overviewBandCount, overviewBands,
    	   			    resampling ? resampling : "average", callback, callback_data );
}
%}
#ifdef SWIGJAVA
%clear (const char* resampling);
#endif /* SWIGJAVA */
%clear GDALRasterBandShadow* srcBand;

/************************************************************************/
/*                         RegenerateOverview()                         */
/************************************************************************/

#ifndef SWIGJAVA
%feature( "kwargs" ) RegenerateOverview;
#endif
%apply Pointer NONNULL { GDALRasterBandShadow* srcBand, GDALRasterBandShadow* overviewBand};
%inline %{
int  RegenerateOverview( GDALRasterBandShadow *srcBand,
                          GDALRasterBandShadow *overviewBand,
                          const char *resampling = "average",
                          GDALProgressFunc callback=NULL,
                          void* callback_data=NULL) {

    CPLErrorReset();

    return GDALRegenerateOverviews( srcBand, 1, &overviewBand,
    	   			    resampling ? resampling : "average", callback, callback_data );
}
%}
%clear GDALRasterBandShadow* srcBand, GDALRasterBandShadow* overviewBand, char* resampling;

/************************************************************************/
/*                             GridCreate()                             */
/************************************************************************/

#ifdef SWIGJAVA
%rename (GridCreate) wrapper_GridCreate;
%apply (int nCount, double *x, double *y, double *z) { (int points, double *x, double *y, double *z) };
%apply (void* nioBuffer, long nioBufferSize) { (void* nioBuffer, long nioBufferSize) };
%inline %{
int wrapper_GridCreate( char* algorithmOptions,
                        int points, double *x, double *y, double *z,
                        double xMin, double xMax, double yMin, double yMax,
                        int xSize, int ySize, GDALDataType dataType,
                        void* nioBuffer, long nioBufferSize,
                        GDALProgressFunc callback = NULL,
                        void* callback_data = NULL)
{
    GDALGridAlgorithm eAlgorithm = GGA_InverseDistanceToAPower;
    void* pOptions = NULL;

    CPLErr eErr = CE_Failure;

    CPLErrorReset();

    if (xSize * ySize * (GDALGetDataTypeSize(dataType) / 8) > nioBufferSize)
    {
        CPLError( eErr, CPLE_AppDefined, "Buffer too small" );
        return eErr;
    }

    if ( algorithmOptions )
    {
        eErr = ParseAlgorithmAndOptions( algorithmOptions, &eAlgorithm, &pOptions );
    }
    else
    {
        eErr = ParseAlgorithmAndOptions( szAlgNameInvDist, &eAlgorithm, &pOptions );
    }
    
    if ( eErr != CE_None )
    {
        CPLError( eErr, CPLE_AppDefined, "Failed to process algorithm name and parameters.\n" );
        return eErr;
    }

    eErr = GDALGridCreate( eAlgorithm, pOptions, points, x, y, z,
                           xMin, xMax, yMin, yMax, xSize, ySize, dataType, nioBuffer,
                           callback, callback_data );

    CPLFree(pOptions);

    return eErr;
}
%}
%clear (void *nioBuffer, long nioBufferSize);
#endif

/************************************************************************/
/*                          ContourGenerate()                           */
/************************************************************************/

#ifndef SWIGJAVA
%feature( "kwargs" ) ContourGenerate;
#endif
%apply Pointer NONNULL {GDALRasterBandShadow *srcBand, OGRLayerShadow* dstLayer};
%apply (int nList, double *pList ) { (int fixedLevelCount, double *fixedLevels ) };
%inline %{
int ContourGenerate( GDALRasterBandShadow *srcBand,
                     double contourInterval,
                     double contourBase,
                     int fixedLevelCount,
                     double *fixedLevels,
                     int useNoData,
                     double noDataValue,
                     OGRLayerShadow* dstLayer, 
                     int idField,
                     int elevField,
                     GDALProgressFunc callback = NULL,
                     void* callback_data = NULL)
{
    CPLErr eErr;

    CPLErrorReset();

    eErr =  GDALContourGenerate( srcBand,
                                 contourInterval,
                                 contourBase,
                                 fixedLevelCount,
                                 fixedLevels,
                                 useNoData,
                                 noDataValue,
                                 dstLayer,
                                 idField,
                                 elevField,
                                 callback,
                                 callback_data);

    return eErr;
}
%}
%clear GDALRasterBandShadow *srcBand;
%clear OGRLayerShadow* dstLayer;
%clear  (int fixedLevelCount, double *fixedLevels );

/************************************************************************/
/*                        AutoCreateWarpedVRT()                         */
/************************************************************************/

%newobject AutoCreateWarpedVRT;
%apply Pointer NONNULL { GDALDatasetShadow *src_ds };
%inline %{
GDALDatasetShadow *AutoCreateWarpedVRT( GDALDatasetShadow *src_ds,
                                        const char *src_wkt = 0,
                                        const char *dst_wkt = 0,
                                        GDALResampleAlg eResampleAlg = GRA_NearestNeighbour,
                                        double maxerror = 0.0 ) {
  GDALDatasetShadow *ds = GDALAutoCreateWarpedVRT( src_ds, src_wkt,
                                                   dst_wkt,
                                                   eResampleAlg,
                                                   maxerror,
                                                   0 );
  if (ds == 0) {
    /*throw CPLGetLastErrorMsg(); causes a SWIG_exception later*/
  }
  return ds;
  
}
%}
%clear GDALDatasetShadow *src_ds;

/************************************************************************/
/*                             Transformer                              */
/************************************************************************/

%rename (Transformer) GDALTransformerInfoShadow;
class GDALTransformerInfoShadow {
private:
  GDALTransformerInfoShadow();
public:
%extend {

  GDALTransformerInfoShadow( GDALDatasetShadow *src, GDALDatasetShadow *dst,
                             char **options ) {
    GDALTransformerInfoShadow *obj = (GDALTransformerInfoShadow*) 
       GDALCreateGenImgProjTransformer2( (GDALDatasetH)src, (GDALDatasetH)dst, 
                                         options );
    return obj;
  }

  ~GDALTransformerInfoShadow() {
    GDALDestroyTransformer( self );
  }

// Need to apply argin typemap second so the numinputs=1 version gets applied
// instead of the numinputs=0 version from argout.
#ifdef SWIGJAVA
%apply (double argout[ANY]) {(double inout[3])};
#else
%apply (double argout[ANY]) {(double inout[3])};
%apply (double argin[ANY]) {(double inout[3])};
#endif
  int TransformPoint( int bDstToSrc, double inout[3] ) {
    int nRet, nSuccess = TRUE;

    nRet = GDALUseTransformer( self, bDstToSrc, 
                               1, &inout[0], &inout[1], &inout[2], 
                               &nSuccess );

    return nRet && nSuccess;
  }
%clear (double inout[3]);

  int TransformPoint( double argout[3], int bDstToSrc, 
                      double x, double y, double z = 0.0 ) {
    int nRet, nSuccess = TRUE;
    
    argout[0] = x;
    argout[1] = y;
    argout[2] = z;
    nRet = GDALUseTransformer( self, bDstToSrc, 
                               1, &argout[0], &argout[1], &argout[2], 
                               &nSuccess );

    return nRet && nSuccess;
  }

#ifdef SWIGCSHARP
  %apply (double *inout) {(double*)};
  %apply (double *inout) {(int*)};
#endif
  int TransformPoints( int bDstToSrc, 
                       int nCount, double *x, double *y, double *z,
                       int *panSuccess ) {
    int nRet;

    nRet = GDALUseTransformer( self, bDstToSrc, nCount, x, y, z, panSuccess );

    return nRet;
  }
#ifdef SWIGCSHARP
  %clear (double*);
  %clear (int*);
#endif

/************************************************************************/
/*                       TransformGeolocations()                        */
/************************************************************************/

%apply Pointer NONNULL {GDALRasterBandShadow *xBand, GDALRasterBandShadow *yBand, GDALRasterBandShadow *zBand};

#ifndef SWIGJAVA
%feature( "kwargs" ) TransformGeolocations;
#endif

  int  TransformGeolocations( GDALRasterBandShadow *xBand,
                              GDALRasterBandShadow *yBand, 
	  		      GDALRasterBandShadow *zBand,
                              GDALProgressFunc callback=NULL,
                              void* callback_data=NULL,
                              char **options = NULL) {

    CPLErrorReset();

    return GDALTransformGeolocations( xBand, yBand, zBand, 
                                      GDALUseTransformer, self,
                            	      callback, callback_data, options );
  }
%clear GDALRasterBandShadow *xBand, GDALRasterBandShadow *yBand, GDALRasterBandShadow *zBand;

} /*extend */
};
