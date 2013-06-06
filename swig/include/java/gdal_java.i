/******************************************************************************
 * $Id$
 *
 * Name:     gdal_java.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for Java bindings
 * Author:   Benjamin Collins, The MITRE Corporation
 *
 *
 * $Log$
 * Revision 1.2  2006/02/16 17:21:12  collinsb
 * Updates to Java bindings to keep the code from halting execution if the native libraries cannot be found.
 *
 * Revision 1.1  2006/02/02 20:56:07  collinsb
 * Added Java specific typemap code
 *
 *
*/

%pragma(java) jniclasscode=%{
  private static boolean available = false;

  static {
    try {
      System.loadLibrary("gdaljni");
      available = true;
      
      if (gdal.HasThreadSupport() == 0)
      {
        System.err.println("WARNING : GDAL should be compiled with thread support for safe execution in Java.");
      }
      
    } catch (UnsatisfiedLinkError e) {
      available = false;
      System.err.println("Native library load failed.");
      System.err.println(e);
    }
  }
  
  public static boolean isAvailable() {
    return available;
  }
%}

/* This hacks turns the gdalJNI class into a package private class */
%pragma(java) jniclassimports=%{
%}

%pragma(java) modulecode=%{

    /* Uninstanciable class */
    private gdal()
    {
    }

    public static String[] GeneralCmdLineProcessor(String[] args, int nOptions)
    {
        java.util.Vector vArgs = new java.util.Vector();
        int i;
        for(i=0;i<args.length;i++)
            vArgs.addElement(args[i]);

        vArgs = GeneralCmdLineProcessor(vArgs, nOptions);
        java.util.Enumeration eArgs = vArgs.elements();
        args = new String[vArgs.size()];
        i = 0;
        while(eArgs.hasMoreElements())
        {
            String arg = (String)eArgs.nextElement();
            args[i++] = arg;
        }

        return args;
    }

    public static String[] GeneralCmdLineProcessor(String[] args)
    {
        return GeneralCmdLineProcessor(args, 0);
    }

    public static double[] InvGeoTransform(double[] gt_in)
    {
      double gt_out[] = new double[6];
      if (InvGeoTransform(gt_in, gt_out) == 1)
        return gt_out;
      else
        return null;
    }
%}

%typemap(javacode) GDAL_GCP %{
  public GCP(double x, double y, double z, double pixel, double line)
  {
      this(x, y, z, pixel, line, "", "");
  }

  public GCP(double x, double y, double pixel, double line, String info, String id)
  {
      this(x, y, 0.0, pixel, line, info, id);
  }

  public GCP(double x, double y, double pixel, double line)
  {
      this(x, y, 0.0, pixel, line, "", "");
  }
%}


%typemap(javaimports) GDALDriverShadow %{
import java.util.Vector;
import org.gdal.gdalconst.gdalconstConstants;
%}

%typemap(javacode) GDALDriverShadow %{

  private static Vector StringArrayToVector(String[] options)
  {
      if (options == null)
        return null;
      Vector v = new Vector();
      for(int i=0;i<options.length;i++)
        v.addElement(options[i]);
      return v;
  }

  public Dataset Create(String name, int xsize, int ysize, int bands, int eType, String[] options) {
    return Create(name, xsize, ysize, bands, eType, StringArrayToVector(options));
  }

  public Dataset Create(String name, int xsize, int ysize, int bands, String[] options) {
    return Create(name, xsize, ysize, bands, gdalconstConstants.GDT_Byte, StringArrayToVector(options));
  }

  public Dataset CreateCopy(String name, Dataset src, int strict, String[] options) {
    return CreateCopy(name, src, strict, StringArrayToVector(options), null);
  }

  public Dataset CreateCopy(String name, Dataset src, Vector options) {
    return CreateCopy(name, src, 1, options, null);
  }

  public Dataset CreateCopy(String name, Dataset src, String[] options) {
    return CreateCopy(name, src, 1, StringArrayToVector(options), null);
  }

%}


%typemap(javacode) GDALDatasetShadow %{

  // Preferred name to match C++ API
  public int GetRasterXSize() { return getRasterXSize(); }

  // Preferred name to match C++ API
  public int GetRasterYSize() { return getRasterYSize(); }

  // Preferred name to match C++ API
  public int GetRasterCount() { return getRasterCount(); }

  public int BuildOverviews(int[] overviewlist, ProgressCallback callback) {
    return BuildOverviews(null, overviewlist, callback);
  }

  public int BuildOverviews(int[] overviewlist) {
    return BuildOverviews(null, overviewlist, null);
  }

  public java.util.Vector GetGCPs() {
      java.util.Vector gcps = new java.util.Vector();
      GetGCPs(gcps);
      return gcps;
  }
  
  public double[] GetGeoTransform() {
      double adfGeoTransform[] = new double[6];
      GetGeoTransform(adfGeoTransform);
      return adfGeoTransform;
  }
%}

%{
    static CPLErr BandBlockReadWrite_Validate(GDALRasterBandH self, void *nioBuffer, long nioBufferSize)
    {
        int nBlockXSize, nBlockYSize;
        GDALDataType eDataType;
        GDALGetBlockSize(self, &nBlockXSize, &nBlockYSize);
        eDataType = GDALGetRasterDataType(self);
        int nDataTypeSize = GDALGetDataTypeSize( eDataType ) / 8;
        if (nBlockXSize > (INT_MAX / nDataTypeSize) / nBlockYSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
        if (nioBufferSize < nBlockXSize * nBlockYSize * nDataTypeSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer not big enough");
            return CE_Failure;
        }
        return CE_None;
    }
%}


%extend GDALDatasetShadow {
%apply (int nList, int* pList) { (int band_list, int *pband_list) };
%apply (void* nioBuffer, long nioBufferSize) { (void* nioBuffer, long nioBufferSize) };

%apply (char *regularArrayOut, long nRegularArraySizeOut) { (char *regularArrayOut, long nRegularArraySizeOut) };
%apply (short *regularArrayOut, long nRegularArraySizeOut) { (short *regularArrayOut, long nRegularArraySizeOut) };
%apply (int *regularArrayOut, long nRegularArraySizeOut) { (int *regularArrayOut, long nRegularArraySizeOut) };
%apply (float *regularArrayOut, long nRegularArraySizeOut) { (float *regularArrayOut, long nRegularArraySizeOut) };
%apply (double *regularArrayOut, long nRegularArraySizeOut) { (double *regularArrayOut, long nRegularArraySizeOut) };

%apply (char *regularArrayIn, long nRegularArraySizeIn) { (char *regularArrayIn, long nRegularArraySizeIn) };
%apply (short *regularArrayIn, long nRegularArraySizeIn) { (short *regularArrayIn, long nRegularArraySizeIn) };
%apply (int *regularArrayIn, long nRegularArraySizeIn) { (int *regularArrayIn, long nRegularArraySizeIn) };
%apply (float *regularArrayIn, long nRegularArraySizeIn) { (float *regularArrayIn, long nRegularArraySizeIn) };
%apply (double *regularArrayIn, long nRegularArraySizeIn) { (double *regularArrayIn, long nRegularArraySizeIn) };

  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
  if (band_list == 0)
  {
      if (pband_list != NULL)
          return CE_Failure;

      band_list = GDALGetRasterCount(self);
  }

  GIntBig nMinBufferSizeInBytes = ComputeDatasetRasterIOSize (
                         buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                         band_list, pband_list, band_list,
                         nPixelSpace, nLineSpace, nBandSpace, FALSE );
  if (nMinBufferSizeInBytes > 0x7fffffff)
  {
     CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
     nMinBufferSizeInBytes = 0;
  }
  if (nMinBufferSizeInBytes == 0)
      return CE_Failure;
  if (nioBufferSize < nMinBufferSizeInBytes)
  {
      CPLError(CE_Failure, CPLE_AppDefined,
              "Buffer is too small");
      return CE_Failure;
  }
  return  GDALDatasetRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                nioBuffer, buf_xsize, buf_ysize,
                                buf_type, band_list, pband_list, nPixelSpace, nLineSpace, nBandSpace );

}


  %define DEFINE_DS_READ_RASTER(ctype, gdal_type)
CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            ctype *regularArrayOut, long nRegularArraySizeOut,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
    if ((gdal_type == GDT_Int16 && buf_type != GDT_Int16 && buf_type != GDT_UInt16 && buf_type != GDT_CInt16) ||
        (gdal_type == GDT_Int32 && buf_type != GDT_Int32 && buf_type != GDT_UInt32 && buf_type != GDT_CInt32) ||
        (gdal_type == GDT_Float32 && buf_type != GDT_Float32 && buf_type != GDT_CFloat32) ||
        (gdal_type == GDT_Float64 && buf_type != GDT_Float64 && buf_type != GDT_CFloat64))
  {
      CPLError(CE_Failure, CPLE_AppDefined,
              "Java array type is not compatible with GDAL data type");
      return CE_Failure;
  }
    
  if (band_list == 0)
  {
      if (pband_list != NULL)
          return CE_Failure;

      band_list = GDALGetRasterCount(self);
  }

  GIntBig nMinBufferSizeInBytes = ComputeDatasetRasterIOSize (
                         buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                         band_list, pband_list, band_list,
                         nPixelSpace, nLineSpace, nBandSpace, sizeof(ctype) > 1 );
  if (nMinBufferSizeInBytes > 0x7fffffff)
  {
     CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
     nMinBufferSizeInBytes = 0;
  }
  if (nMinBufferSizeInBytes == 0)
      return CE_Failure;
  if (nRegularArraySizeOut < nMinBufferSizeInBytes)
  {
      CPLError(CE_Failure, CPLE_AppDefined,
              "Buffer is too small");
      return CE_Failure;
  }
  return  GDALDatasetRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                regularArrayOut, buf_xsize, buf_ysize,
                                buf_type, band_list, pband_list, nPixelSpace, nLineSpace, nBandSpace );

}
  %enddef
  
  DEFINE_DS_READ_RASTER(char, GDT_Byte)
  DEFINE_DS_READ_RASTER(short, GDT_Int16)
  DEFINE_DS_READ_RASTER(int, GDT_Int32)
  DEFINE_DS_READ_RASTER(float, GDT_Float32)
  DEFINE_DS_READ_RASTER(double, GDT_Float64)
  

  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
  if (band_list == 0)
  {
      if (pband_list != NULL)
          return CE_Failure;

      band_list = GDALGetRasterCount(self);
  }

  GIntBig nMinBufferSizeInBytes = ComputeDatasetRasterIOSize (
                         buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                         band_list, pband_list, band_list,
                         nPixelSpace, nLineSpace, nBandSpace, FALSE );
  if (nMinBufferSizeInBytes > 0x7fffffff)
  {
     CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
     nMinBufferSizeInBytes = 0;
  }
  if (nMinBufferSizeInBytes == 0)
      return CE_Failure;
  if (nioBufferSize < nMinBufferSizeInBytes)
  {
      CPLError(CE_Failure, CPLE_AppDefined,
              "Buffer is too small");
      return CE_Failure;
  }
  return  GDALDatasetRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                nioBuffer, buf_xsize, buf_ysize,
                                buf_type, band_list, pband_list, nPixelSpace, nLineSpace, nBandSpace );
}

  %define DEFINE_DS_WRITE_RASTER(ctype, gdal_type)
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            ctype *regularArrayIn, long nRegularArraySizeIn,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
    if ((gdal_type == GDT_Int16 && buf_type != GDT_Int16 && buf_type != GDT_UInt16 && buf_type != GDT_CInt16) ||
        (gdal_type == GDT_Int32 && buf_type != GDT_Int32 && buf_type != GDT_UInt32 && buf_type != GDT_CInt32) ||
        (gdal_type == GDT_Float32 && buf_type != GDT_Float32 && buf_type != GDT_CFloat32) ||
        (gdal_type == GDT_Float64 && buf_type != GDT_Float64 && buf_type != GDT_CFloat64))
  {
      CPLError(CE_Failure, CPLE_AppDefined,
              "Java array type is not compatible with GDAL data type");
      return CE_Failure;
  }

  if (band_list == 0)
  {
      if (pband_list != NULL)
          return CE_Failure;

      band_list = GDALGetRasterCount(self);
  }

  GIntBig nMinBufferSizeInBytes = ComputeDatasetRasterIOSize (
                         buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                         band_list, pband_list, band_list,
                         nPixelSpace, nLineSpace, nBandSpace, sizeof(ctype) > 1 );
  if (nMinBufferSizeInBytes > 0x7fffffff)
  {
     CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
     nMinBufferSizeInBytes = 0;
  }
  if (nMinBufferSizeInBytes == 0)
      return CE_Failure;
  if (nRegularArraySizeIn < nMinBufferSizeInBytes)
  {
      CPLError(CE_Failure, CPLE_AppDefined,
              "Buffer is too small");
      return CE_Failure;
  }
  return  GDALDatasetRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                regularArrayIn, buf_xsize, buf_ysize,
                                buf_type, band_list, pband_list, nPixelSpace, nLineSpace, nBandSpace );
}
 %enddef
    
  DEFINE_DS_WRITE_RASTER(char, GDT_Byte)
  DEFINE_DS_WRITE_RASTER(short, GDT_Int16)
  DEFINE_DS_WRITE_RASTER(int, GDT_Int32)
  DEFINE_DS_WRITE_RASTER(float, GDT_Float32)
  DEFINE_DS_WRITE_RASTER(double, GDT_Float64)

%clear (int band_list, int *pband_list);

} /* extend */

%extend GDALRasterBandShadow {
  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    GIntBig nMinBufferSizeInBytes = ComputeBandRasterIOSize (
                            buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                            nPixelSpace, nLineSpace, FALSE );
    if (nMinBufferSizeInBytes > 0x7fffffff)
    {
       CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
       nMinBufferSizeInBytes = 0;
    }
    if (nMinBufferSizeInBytes == 0)
        return CE_Failure;
    if (nioBufferSize < nMinBufferSizeInBytes)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Buffer is too small");
        return CE_Failure;
    }

    return GDALRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                   nioBuffer, buf_xsize, buf_ysize,
                                   buf_type, nPixelSpace, nLineSpace );

  }

  %define DEFINE_READ_RASTER(ctype, gdal_type)
  CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                     int buf_xsize, int buf_ysize,
                     GDALDataType buf_type,
                     ctype *regularArrayOut, long nRegularArraySizeOut,
                     int nPixelSpace = 0, int nLineSpace = 0)
  {
    if ((gdal_type == GDT_Int16 && buf_type != GDT_Int16 && buf_type != GDT_UInt16 && buf_type != GDT_CInt16) ||
        (gdal_type == GDT_Int32 && buf_type != GDT_Int32 && buf_type != GDT_UInt32 && buf_type != GDT_CInt32) ||
        (gdal_type == GDT_Float32 && buf_type != GDT_Float32 && buf_type != GDT_CFloat32) ||
        (gdal_type == GDT_Float64 && buf_type != GDT_Float64 && buf_type != GDT_CFloat64))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Java array type is not compatible with GDAL data type");
        return CE_Failure;
    }
  
    GIntBig nMinBufferSizeInBytes = ComputeBandRasterIOSize (
                            buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                            nPixelSpace, nLineSpace, sizeof(ctype) > 1 );
    if (nMinBufferSizeInBytes > 0x7fffffff)
    {
       CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
       nMinBufferSizeInBytes = 0;
    }
    if (nMinBufferSizeInBytes == 0)
        return CE_Failure;
    if (nRegularArraySizeOut < nMinBufferSizeInBytes)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Buffer is too small");
        return CE_Failure;
    }

    return GDALRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                   regularArrayOut, buf_xsize, buf_ysize,
                                   buf_type, nPixelSpace, nLineSpace );
  }
  %enddef
  
  DEFINE_READ_RASTER(char, GDT_Byte)
  DEFINE_READ_RASTER(short, GDT_Int16)
  DEFINE_READ_RASTER(int, GDT_Int32)
  DEFINE_READ_RASTER(float, GDT_Float32)
  DEFINE_READ_RASTER(double, GDT_Float64)
  
  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    GIntBig nMinBufferSizeInBytes = ComputeBandRasterIOSize (
                            buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                            nPixelSpace, nLineSpace, FALSE );
    if (nMinBufferSizeInBytes > 0x7fffffff)
    {
       CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
       nMinBufferSizeInBytes = 0;
    }
    if (nMinBufferSizeInBytes == 0)
        return CE_Failure;
    if (nioBufferSize < nMinBufferSizeInBytes)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Buffer is too small");
        return CE_Failure;
    }

    return GDALRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                    nioBuffer, buf_xsize, buf_ysize,
                                    buf_type, nPixelSpace, nLineSpace );
  }
  
  %define DEFINE_WRITE_RASTER(ctype, gdal_type)
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            ctype *regularArrayIn, long nRegularArraySizeIn,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    if ((gdal_type == GDT_Int16 && buf_type != GDT_Int16 && buf_type != GDT_UInt16 && buf_type != GDT_CInt16) ||
        (gdal_type == GDT_Int32 && buf_type != GDT_Int32 && buf_type != GDT_UInt32 && buf_type != GDT_CInt32) ||
        (gdal_type == GDT_Float32 && buf_type != GDT_Float32 && buf_type != GDT_CFloat32) ||
        (gdal_type == GDT_Float64 && buf_type != GDT_Float64 && buf_type != GDT_CFloat64))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Java array type is not compatible with GDAL data type");
        return CE_Failure;
    }
    
    GIntBig nMinBufferSizeInBytes = ComputeBandRasterIOSize (
                            buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                            nPixelSpace, nLineSpace, sizeof(ctype) > 1 );
    if (nMinBufferSizeInBytes > 0x7fffffff)
    {
       CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
       nMinBufferSizeInBytes = 0;
    }
    if (nMinBufferSizeInBytes == 0)
        return CE_Failure;
    if (nRegularArraySizeIn < nMinBufferSizeInBytes)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Buffer is too small");
        return CE_Failure;
    }

    return GDALRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                    regularArrayIn, buf_xsize, buf_ysize,
                                    buf_type, nPixelSpace, nLineSpace );
  }
  %enddef
  
  DEFINE_WRITE_RASTER(char, GDT_Byte)
  DEFINE_WRITE_RASTER(short, GDT_Int16)
  DEFINE_WRITE_RASTER(int, GDT_Int32)
  DEFINE_WRITE_RASTER(float, GDT_Float32)
  DEFINE_WRITE_RASTER(double, GDT_Float64)
  
  CPLErr ReadBlock_Direct( int nXBlockOff, int nYBlockOff, void *nioBuffer, long nioBufferSize )
  {
    if (BandBlockReadWrite_Validate((GDALRasterBandH)self, nioBuffer, nioBufferSize) != CE_None)
      return CE_Failure;

    return GDALReadBlock(self, nXBlockOff, nYBlockOff, nioBuffer);
  }

  CPLErr WriteBlock_Direct( int nXBlockOff, int nYBlockOff, void *nioBuffer, long nioBufferSize )
  {
    if (BandBlockReadWrite_Validate((GDALRasterBandH)self, nioBuffer, nioBufferSize) != CE_None)
      return CE_Failure;

    return GDALWriteBlock(self, nXBlockOff, nYBlockOff, nioBuffer);
  }
/* %clear (void *nioBuffer, long nioBufferSize); */

%clear (char *regularArrayOut, long nRegularArraySizeOut);
%clear (short *regularArrayOut, long nRegularArraySizeOut);
%clear (int *regularArrayOut, long nRegularArraySizeOut);
%clear (float *regularArrayOut, long nRegularArraySizeOut);
%clear (double *regularArrayOut, long nRegularArraySizeOut);

%clear (char *regularArrayIn, long nRegularArraySizeIn);
%clear (short *regularArrayIn, long nRegularArraySizeIn);
%clear (int *regularArrayIn, long nRegularArraySizeIn);
%clear (float *regularArrayIn, long nRegularArraySizeIn);
%clear (double *regularArrayIn, long nRegularArraySizeIn);

%apply (int nList, int* pListOut) {(int buckets, int *panHistogram)};
%apply Pointer NONNULL { int *panHistogram };
  CPLErr GetHistogram(double min,
                     double max,
                     int buckets,
                     int *panHistogram,
                     bool include_out_of_range,
                     bool approx_ok,
                     GDALProgressFunc callback,
                     void* callback_data) {
    CPLErrorReset(); 
    CPLErr err = GDALGetRasterHistogram( self, min, max, buckets, panHistogram,
                                         include_out_of_range, approx_ok,
                                         callback, callback_data );
    return err;
  }

  CPLErr GetHistogram(double min,
                      double max,
                      int buckets,
                      int *panHistogram,
                      bool include_out_of_range,
                      bool approx_ok) {
    CPLErrorReset(); 
    CPLErr err = GDALGetRasterHistogram( self, min, max, buckets, panHistogram,
                                         include_out_of_range, approx_ok,
                                         NULL, NULL);
    return err;
  }

  CPLErr GetHistogram(double min,
                      double max,
                      int buckets,
                      int *panHistogram) {
    CPLErrorReset(); 
    CPLErr err = GDALGetRasterHistogram( self, min, max, buckets, panHistogram,
                                         0, 1,
                                         NULL, NULL);
    return err;
  }

  CPLErr GetHistogram(int buckets,
                        int *panHistogram) {
    CPLErrorReset(); 
    CPLErr err = GDALGetRasterHistogram( self, -0.5, 255.5, buckets, panHistogram,
                                         0, 1,
                                         NULL, NULL);
    return err;
  }
%clear (int buckets, int *panHistogram);

%apply (int* pnList, int** ppListOut) {(int* buckets_ret, int **ppanHistogram)};
%apply (double *OUTPUT){double *min_ret, double *max_ret}
  CPLErr GetDefaultHistogram( double *min_ret, double *max_ret, int* buckets_ret, 
                              int **ppanHistogram, bool force = 1, 
                              GDALProgressFunc callback = NULL,
                              void* callback_data=NULL ) {
      return GDALGetDefaultHistogram( self, min_ret, max_ret, buckets_ret,
                                      ppanHistogram, force, 
                                      callback, callback_data );
  }
%clear (double *min_ret, double *max_ret);
%clear (int* buckets_ret, int **ppanHistogram);

} /* extend */

#ifdef SWIGANDROID

%typemap(javacode) GDALColorTableShadow %{
  private Object parentReference;

  /* Ensure that the GC doesn't collect any parent instance set from Java */
  protected void addReference(Object reference) {
    parentReference = reference;
  }

  public Object clone()
  {
      return Clone();
  }

%}

#else

%typemap(javaimports) GDALColorTableShadow %{
/* imports for getIndexColorModel */
import java.awt.image.IndexColorModel;
import java.awt.Color;
%}

%typemap(javacode) GDALColorTableShadow %{
  private Object parentReference;

  /* Ensure that the GC doesn't collect any parent instance set from Java */
  protected void addReference(Object reference) {
    parentReference = reference;
  }

  public Object clone()
  {
      return Clone();
  }

/* convienance method */
  public IndexColorModel getIndexColorModel(int bits) {
    int size = GetCount();
    byte[] reds = new byte[size];
    byte[] greens = new byte[size];
    byte[] blues = new byte[size];
    byte[] alphas = new byte[size];
    int noAlphas = 0;
    int zeroAlphas = 0;
    int lastAlphaIndex = -1;

    Color entry = null;
    for(int i = 0; i < size; i++) {
      entry = GetColorEntry(i);
      reds[i] = (byte)(entry.getRed()&0xff);
      greens[i] = (byte)(entry.getGreen()&0xff);
      blues[i] = (byte)(entry.getBlue()&0xff);
      byte alpha = (byte)(entry.getAlpha()&0xff);

      // The byte type is -128 to 127 so a normal 255 will be -1.
      if (alpha == -1) 
          noAlphas ++;
      else{
        if (alpha == 0){
           zeroAlphas++;
           lastAlphaIndex = i;
        }
      }
      alphas[i] = alpha;
    }
    if (noAlphas == size)
        return new IndexColorModel(bits, size, reds, greens, blues);
    else if (noAlphas == (size - 1) && zeroAlphas == 1)
        return new IndexColorModel(bits, size, reds, greens, blues, lastAlphaIndex);
    else 
        return new IndexColorModel(bits, size, reds, greens, blues, alphas);
 }
%}

#endif

%typemap(javaimports) GDALRasterBandShadow %{
import org.gdal.gdalconst.gdalconstConstants;
%}


%typemap(javacode) GDALRasterBandShadow %{

  // Preferred name to match C++ API
  public int GetXSize() { return getXSize(); }

  // Preferred name to match C++ API
  public int GetYSize() { return getYSize(); }

  // Preferred name to match C++ API
  public int GetRasterDataType() { return getDataType(); }

  public int GetBlockXSize()
  {
      int[] anBlockXSize = new int[1];
      int[] anBlockYSize = new int[1];
      GetBlockSize(anBlockXSize, anBlockYSize);
      return anBlockXSize[0];
  }

  public int GetBlockYSize()
  {
      int[] anBlockXSize = new int[1];
      int[] anBlockYSize = new int[1];
      GetBlockSize(anBlockXSize, anBlockYSize);
      return anBlockYSize[0];
  }

  public int Checksum() {
    return Checksum(0, 0, getXSize(), getYSize());
  }

  public int GetStatistics(boolean approx_ok, boolean force, double[] min, double[] max, double[] mean, double[] stddev) {
    return GetStatistics((approx_ok) ? 1 : 0, (force) ? 1 : 0, min, max, mean, stddev);
  }

   public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize,
                                int buf_xsize, int buf_ysize, java.nio.ByteBuffer nioBuffer) {
       return ReadRaster_Direct(xoff, yoff, xsize, ysize, buf_xsize, buf_ysize, gdalconstConstants.GDT_Byte, nioBuffer);
   }

   public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize,
                                java.nio.ByteBuffer nioBuffer) {
       return ReadRaster_Direct(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Byte, nioBuffer);
   }

   public java.nio.ByteBuffer ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize,
                                                int buf_xsize, int buf_ysize, int buf_type)
   {
       long buf_size = buf_xsize * buf_ysize * (gdal.GetDataTypeSize(buf_type) / 8);
       if ((int)buf_size != buf_size)
               throw new OutOfMemoryError();
       java.nio.ByteBuffer nioBuffer = java.nio.ByteBuffer.allocateDirect((int)buf_size);
       int ret = ReadRaster_Direct(xoff, yoff, xsize, ysize, buf_xsize, buf_ysize, buf_type, nioBuffer);
       if (ret == gdalconstConstants.CE_None)
               return nioBuffer;
       else
               return null;
   }

   public java.nio.ByteBuffer ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_type)
   {
       return ReadRaster_Direct(xoff, yoff, xsize, ysize, xsize, ysize, buf_type);
   }

   public java.nio.ByteBuffer ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize)
   {
       return ReadRaster_Direct(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Byte);
   }

   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, byte[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }

   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, byte[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Byte, array);
   }
   
   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, short[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }

   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, short[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Int16, array);
   }
   
   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, int[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }
   
   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, int[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Int32, array);
   }
   
   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, float[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }
   
   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, float[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Float32, array);
   }
   
   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, double[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }
   
   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, double[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Float64, array);
   }
   
   public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize,
                                int buf_xsize, int buf_ysize, java.nio.ByteBuffer nioBuffer) {
       return WriteRaster_Direct(xoff, yoff, xsize, ysize, buf_xsize, buf_ysize, gdalconstConstants.GDT_Byte, nioBuffer);
   }

   public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize,
                                 int buf_type, java.nio.ByteBuffer nioBuffer) {
       return WriteRaster_Direct(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, nioBuffer);
   }

   public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize,
                                 java.nio.ByteBuffer nioBuffer) {
       return WriteRaster_Direct(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Byte, nioBuffer);
   }

   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, byte[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }

   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, byte[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Byte, array);
   }
   
   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, short[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }

   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, short[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Int16, array);
   }
   
   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, int[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }
   
   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, int[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Int32, array);
   }
   
   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, float[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }
   
   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, float[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Float32, array);
   }
   
   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, double[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }
   
   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, double[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Float64, array);
   }
%}

/* Could be disabled as do nothing, but we keep them for backwards compatibility */
/*%typemap(javadestruct_derived, methodname="delete", methodmodifiers="public") GDALRasterBandShadow ""
%typemap(javadestruct_derived, methodname="delete", methodmodifiers="public") GDALDriverShadow ""
%typemap(javadestruct, methodname="delete", methodmodifiers="protected") GDALMajorObjectShadow %{
  {
    if(swigCPtr != 0 && swigCMemOwn) {
      swigCMemOwn = false;
      throw new UnsupportedOperationException("C++ destructor does not have public access");
    }
    swigCPtr = 0;
  }
%}*/

// Add a Java reference to prevent premature garbage collection and resulting use
// of dangling C++ pointer. Intended for methods that return pointers or
// references to a member variable.
%typemap(javaout) GDALRasterBandShadow* GetRasterBand,
                  GDALRasterBandShadow* GetOverview,
                  GDALRasterBandShadow* GetMaskBand,
                  GDALColorTableShadow* GetColorTable,
                  GDALColorTableShadow* GetRasterColorTable,
                  CPLXMLNode* getChild,
                  CPLXMLNode* getNext,
                  CPLXMLNode* GetXMLNode,
                  CPLXMLNode* SearchXMLNode {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javainterfaces) GDALColorTableShadow "Cloneable"
%typemap(javainterfaces) GDALRasterAttributeTableShadow "Cloneable"

%typemap(javacode) GDALRasterAttributeTableShadow %{

  public Object clone()
  {
      return Clone();
  }
%}

%typemap(javacode) GDALMajorObjectShadow %{
  private Object parentReference;

  /* Ensure that the GC doesn't collect any parent instance set from Java */
  protected void addReference(Object reference) {
    parentReference = reference;
  }

  /* For backward compatibilty */
  public int SetMetadata(java.util.Hashtable metadata, String domain)
  {
      if (metadata == null)
          return SetMetadata((java.util.Vector)null, domain);
      java.util.Vector v = new java.util.Vector();
      java.util.Enumeration values = metadata.elements();
      java.util.Enumeration keys = metadata.keys();
      while(keys.hasMoreElements())
      {
          v.add((String)keys.nextElement() + "=" + (String)values.nextElement());
      }
      return SetMetadata(v, domain);
  }

  public int SetMetadata(java.util.Hashtable metadata)
  {
      return SetMetadata(metadata, null);
  }
%}

%typemap(in) (OGRLayerShadow*)
{
    if ($input != NULL)
    {
        const jclass klass = jenv->FindClass("org/gdal/ogr/Layer");
        const jmethodID getCPtr = jenv->GetStaticMethodID(klass, "getCPtr", "(Lorg/gdal/ogr/Layer;)J");
        $1 = (OGRLayerShadow*) jenv->CallStaticLongMethod(klass, getCPtr, $input);
    }
}

%typemap(jni) (OGRLayerShadow*)  "jobject"
%typemap(jtype) (OGRLayerShadow*)  "org.gdal.ogr.Layer"
%typemap(jstype) (OGRLayerShadow*)  "org.gdal.ogr.Layer"
%typemap(javain) (OGRLayerShadow*)  "$javainput"

%include callback.i

%include typemaps_java.i
