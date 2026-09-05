/******************************************************************************
 *
 * Name:     gdal_csharp_extend.i
 * Project:  GDAL CSharp Interface
 * Purpose:  C# specific GDAL extensions
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/


/******************************************************************************
 * GDAL raster R/W support                                                    *
 *****************************************************************************/

%extend GDALRasterBandShadow
{
	%apply (void *buffer_ptr) {void *buffer};
    CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type,
                          GIntBig  pixelSpace, GIntBig lineSpace, GDALRasterIOExtraArg* extraArg = NULL) {
       return GDALRasterIOEx( self, GF_Read, xOff, yOff, xSize, ySize,
		        buffer, buf_xSize, buf_ySize, buf_type, pixelSpace, lineSpace, extraArg );
    }
    CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type,
                          GIntBig pixelSpace, GIntBig lineSpace, GDALRasterIOExtraArg* extraArg = NULL) {
       return GDALRasterIOEx( self, GF_Write, xOff, yOff, xSize, ySize,
		        buffer, buf_xSize, buf_ySize, buf_type, pixelSpace, lineSpace, extraArg );
    }
    %clear void *buffer;
}

%extend GDALDatasetShadow
{
	%apply (void *buffer_ptr) {void *buffer};
	%apply (int INPUT[]) {int *bandMap};
    CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type,
                          int bandCount, int* bandMap, GIntBig pixelSpace, GIntBig lineSpace, GIntBig bandSpace,
						  GDALRasterIOExtraArg* extraArg = NULL) {
       return GDALDatasetRasterIOEx( self, GF_Read, xOff, yOff, xSize, ySize,
		        buffer, buf_xSize, buf_ySize, buf_type, bandCount,
		        bandMap, pixelSpace, lineSpace, bandSpace, extraArg);
    }
    CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, void* buffer,
                          int buf_xSize, int buf_ySize, GDALDataType buf_type,
                          int bandCount, int* bandMap, GIntBig pixelSpace, GIntBig lineSpace, GIntBig bandSpace,
						  GDALRasterIOExtraArg* extraArg = NULL) {
       return GDALDatasetRasterIOEx( self, GF_Write, xOff, yOff, xSize, ySize,
		        buffer, buf_xSize, buf_ySize, buf_type, bandCount,
		        bandMap, pixelSpace, lineSpace, bandSpace, extraArg);
    }
    %clear void *buffer;
    %clear int* bandMap;
}

%inline
{	
  static GDALDatasetShadow* GetDatasetFromLayer(OGRLayerShadow *layer) {
    return OGR_L_GetDataset(layer);
  }
}
