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

#ifndef SWIGCSHARP
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


%feature( "kwargs" ) ComputeMedianCutPCT;
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


%feature( "kwargs" ) DitherRGB2PCT;
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

%newobject ReprojectImage;
%inline %{
CPLErr  ReprojectImage ( GDALDatasetShadow *src_ds,
                         GDALDatasetShadow *dst_ds,
                         const char *src_wkt=NULL,
                         const char *dst_wkt=NULL,
                         GDALResampleAlg eResampleAlg=GRA_NearestNeighbour,
                         double WarpMemoryLimit=0.0,
                         double maxerror = 0.0) {

    CPLErrorReset();

    CPLErr err = GDALReprojectImage( src_ds,
                                     src_wkt,
                                     dst_ds,
                                     dst_wkt,
                                     eResampleAlg,
                                     WarpMemoryLimit,
                                     maxerror,
                                     NULL,
                                     NULL,
                                     NULL);
    
    return err;
}
%} 
%newobject AutoCreateWarpedVRT;
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