/*
 * $Id$
 */
 
%include cpl_exceptions.i

// When we switch to swig 1.3.26 these definitions can be removed

%{
GDALDataType GDALRasterBandShadow_get_DataType( GDALRasterBandShadow *h ) {
  return GDALGetRasterDataType( h );
}
int GDALRasterBandShadow_get_XSize( GDALRasterBandShadow *h ) {
  return GDALGetRasterBandXSize( h );
}
int GDALRasterBandShadow_get_YSize( GDALRasterBandShadow *h ) {
  return GDALGetRasterBandYSize( h );
}
int GDALDatasetShadow_get_RasterXSize( GDALDatasetShadow *h ) {
  return GDALGetRasterXSize( h );
}
int GDALDatasetShadow_get_RasterYSize( GDALDatasetShadow *h ) {
  return GDALGetRasterYSize( h );
}
int GDALDatasetShadow_get_RasterCount( GDALDatasetShadow *h ) {
  return GDALGetRasterCount( h );
}
char const *GDALDriverShadow_get_ShortName( GDALDriverShadow *h ) {
  return GDALGetDriverShortName( h );
}
char const *GDALDriverShadow_get_LongName( GDALDriverShadow *h ) {
  return GDALGetDriverLongName( h );
}
char const *GDALDriverShadow_get_HelpTopic( GDALDriverShadow *h ) {
  return GDALGetDriverHelpTopic( h );
}
%}

%rename (GetMetadata) GetMetadata_List;
%ignore GetMetadata_Dict;

%include typemaps_csharp.i

%define %rasterio_functions(GDALTYPE,CSTYPE)
 public int ReadRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace) {
      int retval;
      IntPtr ptr = Marshal.AllocHGlobal(buf_xSize * buf_ySize * Marshal.SizeOf(buffer[0]));
      try {
          retval = ReadRaster(xOff, yOff, xSize, ySize, ptr, buf_xSize, buf_ySize, GDALTYPE, pixelSpace, lineSpace);
          Marshal.Copy(ptr, buffer, 0, buf_xSize * buf_ySize);
      } finally {
          Marshal.FreeHGlobal(ptr);
      }
      GC.KeepAlive(this);
      return retval;
  }
  public int WriteRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace) {
      int retval;
      IntPtr ptr = Marshal.AllocHGlobal(buf_xSize * buf_ySize * Marshal.SizeOf(buffer[0]));
      try {
          Marshal.Copy(buffer, 0, ptr, buf_xSize * buf_ySize);
          retval = WriteRaster(xOff, yOff, xSize, ySize, ptr, buf_xSize, buf_ySize, GDALTYPE, pixelSpace, lineSpace);
      } finally {
          Marshal.FreeHGlobal(ptr);
      }
      GC.KeepAlive(this);
      return retval;
  }
  
%enddef

%typemap(cscode, noblock="1") GDALRasterBandShadow {
/*! Eight bit unsigned integer */ %rasterio_functions(1,byte)
/*! Sixteen bit signed integer */ %rasterio_functions(3,short)
/*! Thirty two bit signed integer */ %rasterio_functions(5,int)
/*! Thirty two bit floating point */ %rasterio_functions(6,float)
/*! Sixty four bit floating point */ %rasterio_functions(7,double)
}

/*! Sixteen bit unsigned integer */ //%rasterio_functions(2,ushort)
/*! Thirty two bit unsigned integer */ //%rasterio_functions(4,uint)
/*! Complex Int16 */ //%rasterio_functions(8,int)                 
/*! Complex Int32 */ //%rasterio_functions(9,int)                 
/*! Complex Float32 */ //%rasterio_functions(10,int)              
/*! Complex Float64 */ //%rasterio_functions(11,int)               

