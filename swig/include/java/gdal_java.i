/******************************************************************************
 * $Id$
 *
 * Name:     gdal_java.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for Java bindings
 * Author:   Benjamin Collins, The MITRE Corporation
 *
*/

%include java_exceptions.i

%pragma(java) jniclasscode=%{
  private static boolean available = false;

  static {
    try {
      System.loadLibrary("gdalalljni");
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
import org.gdal.osr.SpatialReference;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.StyleTable;
import org.gdal.ogr.Layer;
import org.gdal.ogr.Feature;
import org.gdal.ogr.FieldDomain;
import org.gdal.ogr.GeomFieldDefn;
%}

%pragma(java) moduleimports=%{
import org.gdal.osr.SpatialReference;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.StyleTable;
import org.gdal.ogr.Layer;
import org.gdal.ogr.Feature;
import org.gdal.ogr.FieldDomain;
%}

%typemap(javaimports) GDALDatasetShadow %{
import org.gdal.osr.SpatialReference;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.StyleTable;
import org.gdal.ogr.Layer;
import org.gdal.ogr.Feature;
import org.gdal.ogr.FieldDomain;
import org.gdal.ogr.GeomFieldDefn;
%}

%typemap(javaimports) GDALDimensionHS %{
import java.util.Vector;
%}

%typemap(javaimports) GDALExtendedDataTypeHS %{
import org.gdal.gdal.ExtendedDataType;
%}

%typemap(javaimports) GDALGroupHS %{
import java.math.BigInteger;
import java.util.Vector;
import org.gdal.gdal.Dimension;
import org.gdal.gdal.ExtendedDataType;
import org.gdal.gdal.MDArray;
import org.gdal.ogr.Layer;
%}

%pragma(java) modulecode=%{

    /* Uninstantiable class */
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

%typemap(javaimports) GDALMDArrayHS %{
import org.gdal.osr.SpatialReference;
import org.gdal.gdalconst.gdalconst;
import org.gdal.gdal.Dimension;
import java.util.Vector;
import java.util.List;
import java.util.ArrayList;
import java.lang.Integer;
%}

%{

  static GDALDimensionH GDALMDArrayGetDim(GDALMDArrayH hMDA, size_t index) {

    size_t dimCount;

    GDALDimensionH* dims = GDALMDArrayGetDimensions(hMDA, &dimCount);

    GDALDimensionH retVal;

    if (index < 0 || index >= dimCount) {

      retVal = NULL;
    }
    else {

      retVal = dims[index];

      dims[index] = NULL;  // make sure we do not free our index
    }

    // free all the other indices

    GDALReleaseDimensions(dims, dimCount);

    return retVal;
  }

  static bool MDArrayRead(GDALMDArrayH hMDA,
                            int numDims,
                            const GInt64 *arrayStartIdxes,
                            const GInt64 *counts,
                            const GInt64 *arraySteps,
                            GInt64 *bufferStrides,
                            void* arrayOut,
                            size_t arrayByteSize,
                            GDALExtendedDataTypeH data_type)
  {
    size_t* localCounts =
      (size_t*) malloc(sizeof(size_t) * numDims);

    GPtrDiff_t* localBufferStrides =
      (GPtrDiff_t*) malloc(sizeof(GPtrDiff_t) * numDims);

    for (int i = 0; i < numDims; i++) {
      localCounts[i] = (size_t) counts[i];
      localBufferStrides[i] = (GPtrDiff_t) bufferStrides[i];
    }

    bool retVal = GDALMDArrayRead(hMDA,
                                   (const GUInt64*) arrayStartIdxes,
                                   localCounts,
                                   arraySteps,
                                   localBufferStrides,
                                   data_type,
                                   arrayOut,
                                   arrayOut,
                                   arrayByteSize);

    free(localBufferStrides);
    free(localCounts);

    return retVal;
  }

  static bool MDArrayWrite(GDALMDArrayH hMDA,
                            int numDims,
                            const GInt64 *arrayStartIdxes,
                            const GInt64 *counts,
                            const GInt64 *arraySteps,
                            GInt64 *bufferStrides,
                            void* arrayIn,
                            size_t arrayByteSize,
                            GDALExtendedDataTypeH data_type)
  {
    size_t* localCounts =
      (size_t*) malloc(sizeof(size_t) * numDims);

    GPtrDiff_t* localBufferStrides =
      (GPtrDiff_t*) malloc(sizeof(GPtrDiff_t) * numDims);

    for (int i = 0; i < numDims; i++) {
      localCounts[i] = (size_t) counts[i];
      localBufferStrides[i] = (GPtrDiff_t) bufferStrides[i];
    }

    bool retVal = GDALMDArrayWrite(hMDA,
                                    (const GUInt64*) arrayStartIdxes,
                                    localCounts,
                                    arraySteps,
                                    localBufferStrides,
                                    data_type,
                                    arrayIn,
                                    arrayIn,
                                    arrayByteSize);

    free(localBufferStrides);
    free(localCounts);

    return retVal;
  }

%}

%extend GDALMDArrayHS {

%newobject GetDimension;
  GDALDimensionHS* GetDimension(size_t index) {
    return GDALMDArrayGetDim(self, index);
  }

%define DEFINE_READ_MDA_DATA(ctype, buffer_type_code)
  %apply(int nList, GInt64 *pList) { (int starts,  GInt64 *startsValues) };
  %apply(int nList, GInt64 *pList) { (int counts,  GInt64 *countsValues) };
  %apply(int nList, GInt64 *pList) { (int steps,   GInt64 *stepsValues) };
  %apply(int nList, GInt64 *pList) { (int strides, GInt64 *stridesValues) };
  %apply (ctype *arrayOut, size_t arraySize) { (ctype *arrayOut, size_t arraySize) };
  bool Read(int starts,  GInt64 *startsValues,
            int counts,  GInt64 *countsValues,
            int steps,   GInt64 *stepsValues,
            int strides, GInt64 *stridesValues,
            ctype *arrayOut,
            size_t arraySize
           )
  {
    size_t numDims = GDALMDArrayGetDimensionCount(self);

    if (starts != numDims ||
        counts != numDims ||
        steps != numDims ||
        strides != numDims)
    {
      return false;
    }

    GDALExtendedDataTypeH buffer_type =
      GDALExtendedDataTypeCreate(buffer_type_code);

    bool result =
        MDArrayRead(self,
                        counts,
                        startsValues,
                        countsValues,
                        stepsValues,
                        stridesValues,
                        arrayOut,
                        arraySize,
                        buffer_type);

    GDALExtendedDataTypeRelease(buffer_type);

    return result;
  }
%enddef

  DEFINE_READ_MDA_DATA(char,    GDT_Byte)
  DEFINE_READ_MDA_DATA(short,   GDT_Int16)
  DEFINE_READ_MDA_DATA(int,     GDT_Int32)
  DEFINE_READ_MDA_DATA(int64_t, GDT_Int64)
  DEFINE_READ_MDA_DATA(float,   GDT_Float32)
  DEFINE_READ_MDA_DATA(double,  GDT_Float64)

%define DEFINE_WRITE_MDA_DATA(ctype, buffer_type_code)
  %apply(int nList, GInt64 *pList) { (int starts,  GInt64 *startsValues) };
  %apply(int nList, GInt64 *pList) { (int counts,  GInt64 *countsValues) };
  %apply(int nList, GInt64 *pList) { (int steps,   GInt64 *stepsValues) };
  %apply(int nList, GInt64 *pList) { (int strides, GInt64 *stridesValues) };
  %apply (ctype *arrayIn, size_t arraySize) { (ctype *arrayIn, size_t arraySize) };
  bool Write(int starts,  GInt64 *startsValues,
             int counts,  GInt64 *countsValues,
             int steps,   GInt64 *stepsValues,
             int strides, GInt64 *stridesValues,
             ctype *arrayIn,
             size_t arraySize
            )
  {
    size_t numDims = GDALMDArrayGetDimensionCount(self);

    if (starts != numDims ||
        counts != numDims ||
        steps != numDims ||
        strides != numDims)
    {
      return false;
    }

    GDALExtendedDataTypeH buffer_type =
      GDALExtendedDataTypeCreate(buffer_type_code);

    bool result =
        MDArrayWrite(self,
                        counts,
                        startsValues,
                        countsValues,
                        stepsValues,
                        stridesValues,
                        arrayIn,
                        arraySize,
                        buffer_type);

    GDALExtendedDataTypeRelease(buffer_type);

    return result;
  }
%enddef

  DEFINE_WRITE_MDA_DATA(char   , GDT_Byte)
  DEFINE_WRITE_MDA_DATA(short  , GDT_Int16)
  DEFINE_WRITE_MDA_DATA(int    , GDT_Int32)
  DEFINE_WRITE_MDA_DATA(int64_t, GDT_Int64)
  DEFINE_WRITE_MDA_DATA(float  , GDT_Float32)
  DEFINE_WRITE_MDA_DATA(double , GDT_Float64)

} /* extend */

%typemap(javacode) GDALMDArrayHS %{

   public Dimension[] GetDimensions() {

       long size = GetDimensionCount();

       if (size > Integer.MAX_VALUE)
           throw new IllegalArgumentException("java array can hold at most "+Integer.MAX_VALUE+" values.");

       Dimension[] arr = new Dimension[(int) size];

       for (int i = 0; i < size; i++) {
           Dimension dim = GetDimension(i);
           arr[i] = dim;
       }

       return arr;
   }

   private long[] defaultSteps(int numDims) {

       long[] retVal = new long[numDims];

       for (int i = 0; i < numDims; i++) {
           retVal[i] = 1;
       }

       return retVal;
   }

   private long[] defaultStrides(long[] counts) {

       int numDims = counts.length;

       long[] retVal = new long[numDims];

       if (numDims>0)
       {
           retVal[numDims-1] = 1;
           for (int i = numDims - 2; i >= 0; i--) {
               retVal[i] = retVal[i+1] * counts[i+1];
           }
       }

       return retVal;
   }

   public boolean Read(long[] starts, long[] counts, long[] steps, byte[] outputBuffer) {
       return Read(starts, counts, steps, defaultStrides(counts), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, long[] steps, short[] outputBuffer) {
       return Read(starts, counts, steps, defaultStrides(counts), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, long[] steps, int[] outputBuffer) {
       return Read(starts, counts, steps, defaultStrides(counts), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, long[] steps, long[] outputBuffer) {
       return Read(starts, counts, steps, defaultStrides(counts), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, long[] steps, float[] outputBuffer) {
       return Read(starts, counts, steps, defaultStrides(counts), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, long[] steps, double[] outputBuffer) {
       return Read(starts, counts, steps, defaultStrides(counts), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, byte[] outputBuffer) {
       return Read(starts, counts, defaultSteps(counts.length), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, short[] outputBuffer) {
       return Read(starts, counts, defaultSteps(counts.length), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, int[] outputBuffer) {
       return Read(starts, counts, defaultSteps(counts.length), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, long[] outputBuffer) {
       return Read(starts, counts, defaultSteps(counts.length), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, float[] outputBuffer) {
       return Read(starts, counts, defaultSteps(counts.length), outputBuffer);
   }

   public boolean Read(long[] starts, long[] counts, double[] outputBuffer) {
       return Read(starts, counts, defaultSteps(counts.length), outputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, long[] steps, byte[] inputBuffer) {
       return Write(starts, counts, steps, defaultStrides(counts), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, long[] steps, short[] inputBuffer) {
       return Write(starts, counts, steps, defaultStrides(counts), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, long[] steps, int[] inputBuffer) {
       return Write(starts, counts, steps, defaultStrides(counts), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, long[] steps, long[] inputBuffer) {
       return Write(starts, counts, steps, defaultStrides(counts), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, long[] steps, float[] inputBuffer) {
       return Write(starts, counts, steps, defaultStrides(counts), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, long[] steps, double[] inputBuffer) {
       return Write(starts, counts, steps, defaultStrides(counts), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, byte[] inputBuffer) {
       return Write(starts, counts, defaultSteps(counts.length), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, short[] inputBuffer) {
       return Write(starts, counts, defaultSteps(counts.length), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, int[] inputBuffer) {
       return Write(starts, counts, defaultSteps(counts.length), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, long[] inputBuffer) {
       return Write(starts, counts, defaultSteps(counts.length), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, float[] inputBuffer) {
       return Write(starts, counts, defaultSteps(counts.length), inputBuffer);
   }

   public boolean Write(long[] starts, long[] counts, double[] inputBuffer) {
       return Write(starts, counts, defaultSteps(counts.length), inputBuffer);
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

%typemap(javabody_derived) GDALDatasetShadow %{
  private long swigCPtr;

  public Dataset(long cPtr, boolean cMemoryOwn) {
    super(gdalJNI.Dataset_SWIGUpcast(cPtr), cMemoryOwn);
    swigCPtr = cPtr;
  }

  public static long getCPtr(Dataset obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
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

  public Layer GetLayer(int index)
  {
      return GetLayerByIndex(index);
  }

  public Layer GetLayer(String layerName)
  {
      return GetLayerByName(layerName);
  }
%}

%{
    static CPLErr BandBlockReadWrite_Validate(GDALRasterBandH self, void *nioBuffer, size_t nioBufferSize)
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
        if (nioBufferSize < (size_t)nBlockXSize * nBlockYSize * nDataTypeSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer not big enough");
            return CE_Failure;
        }
        return CE_None;
    }
%}

%{

static
GIntBig ComputeDatasetRasterIOSize (int buf_xsize, int buf_ysize, int nPixelSize,
                                int nBands, int* bandMap, int nBandMapArrayLength,
                                GIntBig nPixelSpace, GIntBig nLineSpace, GIntBig nBandSpace,
                                int bSpacingShouldBeMultipleOfPixelSize );

static CPLErr DatasetRasterIO( GDALDatasetH hDS, GDALRWFlag eRWFlag,
                            int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *regularArray, size_t nRegularArraySize,
                            int band_list, int *pband_list,
                            int nPixelSpace, int nLineSpace, int nBandSpace,
                            GDALDataType gdal_type, size_t sizeof_ctype)
{
  if ((gdal_type == GDT_Int16 && buf_type != GDT_Int16 && buf_type != GDT_UInt16 && buf_type != GDT_CInt16) ||
      (gdal_type == GDT_Int32 && buf_type != GDT_Int32 && buf_type != GDT_UInt32 && buf_type != GDT_CInt32) ||
      (gdal_type == GDT_Int64 && buf_type != GDT_Int64 && buf_type != GDT_UInt64) ||
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

      band_list = GDALGetRasterCount(hDS);
  }

  GIntBig nMinBufferSizeInBytes = ComputeDatasetRasterIOSize (
                         buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                         band_list, pband_list, band_list,
                         nPixelSpace, nLineSpace, nBandSpace, sizeof_ctype > 1 );

  if (nMinBufferSizeInBytes > 0x7fffffff)
  {
     CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
     nMinBufferSizeInBytes = 0;
  }

  if (nMinBufferSizeInBytes == 0)
      return CE_Failure;

  if (nRegularArraySize < nMinBufferSizeInBytes)
  {
      CPLError(CE_Failure, CPLE_AppDefined,
              "Buffer is too small");
      return CE_Failure;
  }

  return  GDALDatasetRasterIO( hDS, eRWFlag, xoff, yoff, xsize, ysize,
                                regularArray, buf_xsize, buf_ysize,
                                buf_type, band_list, pband_list, nPixelSpace, nLineSpace, nBandSpace );
}

%}


%extend GDALDatasetShadow {

%apply (int nList, int* pList) { (int band_list, int *pband_list) };

%apply (void* nioBuffer, size_t nioBufferSize) { (void* nioBuffer, size_t nioBufferSize) };

%apply (char *regularArrayOut, size_t nRegularArraySizeOut) { (char *regularArrayOut, size_t nRegularArraySizeOut) };
%apply (short *regularArrayOut, size_t nRegularArraySizeOut) { (short *regularArrayOut, size_t nRegularArraySizeOut) };
%apply (int *regularArrayOut, size_t nRegularArraySizeOut) { (int *regularArrayOut, size_t nRegularArraySizeOut) };
%apply (int64_t *regularArrayOut, size_t nRegularArraySizeOut) { (int64_t *regularArrayOut, size_t nRegularArraySizeOut) };
%apply (float *regularArrayOut, size_t nRegularArraySizeOut) { (float *regularArrayOut, size_t nRegularArraySizeOut) };
%apply (double *regularArrayOut, size_t nRegularArraySizeOut) { (double *regularArrayOut, size_t nRegularArraySizeOut) };

%apply (char *regularArrayIn, size_t nRegularArraySizeIn) { (char *regularArrayIn, size_t nRegularArraySizeIn) };
%apply (short *regularArrayIn, size_t nRegularArraySizeIn) { (short *regularArrayIn, size_t nRegularArraySizeIn) };
%apply (int *regularArrayIn, size_t nRegularArraySizeIn) { (int *regularArrayIn, size_t nRegularArraySizeIn) };
%apply (int64_t *regularArrayIn, size_t nRegularArraySizeIn) { (int64_t *regularArrayIn, size_t nRegularArraySizeIn) };
%apply (float *regularArrayIn, size_t nRegularArraySizeIn) { (float *regularArrayIn, size_t nRegularArraySizeIn) };
%apply (double *regularArrayIn, size_t nRegularArraySizeIn) { (double *regularArrayIn, size_t nRegularArraySizeIn) };

  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, size_t nioBufferSize,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
    return DatasetRasterIO( (GDALDatasetH)self, GF_Read,
                              xoff, yoff, xsize, ysize,
                              buf_xsize, buf_ysize,
                              buf_type,
                              nioBuffer, nioBufferSize,
                              band_list, pband_list,
                              nPixelSpace, nLineSpace, nBandSpace,
                              GDT_Unknown, 0);
}


  %define DEFINE_DS_READ_RASTER(ctype, gdal_type)
CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            ctype *regularArrayOut, size_t nRegularArraySizeOut,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
    return DatasetRasterIO( (GDALDatasetH)self, GF_Read,
                              xoff, yoff, xsize, ysize,
                              buf_xsize, buf_ysize,
                              buf_type,
                              regularArrayOut, nRegularArraySizeOut,
                              band_list, pband_list,
                              nPixelSpace, nLineSpace, nBandSpace,
                              gdal_type, sizeof(ctype));
}
  %enddef

  DEFINE_DS_READ_RASTER(char, GDT_Byte)
  DEFINE_DS_READ_RASTER(short, GDT_Int16)
  DEFINE_DS_READ_RASTER(int, GDT_Int32)
  DEFINE_DS_READ_RASTER(int64_t, GDT_Int64)
  DEFINE_DS_READ_RASTER(float, GDT_Float32)
  DEFINE_DS_READ_RASTER(double, GDT_Float64)


  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, size_t nioBufferSize,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
    return DatasetRasterIO( (GDALDatasetH)self, GF_Write,
                              xoff, yoff, xsize, ysize,
                              buf_xsize, buf_ysize,
                              buf_type,
                              nioBuffer, nioBufferSize,
                              band_list, pband_list,
                              nPixelSpace, nLineSpace, nBandSpace,
                              GDT_Unknown, 0);
}

  %define DEFINE_DS_WRITE_RASTER(ctype, gdal_type)
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            ctype *regularArrayIn, size_t nRegularArraySizeIn,
                            int band_list, int *pband_list,
                            int nPixelSpace = 0, int nLineSpace = 0, int nBandSpace = 0)
{
    return DatasetRasterIO( (GDALDatasetH)self, GF_Write,
                              xoff, yoff, xsize, ysize,
                              buf_xsize, buf_ysize,
                              buf_type,
                              regularArrayIn, nRegularArraySizeIn,
                              band_list, pband_list,
                              nPixelSpace, nLineSpace, nBandSpace,
                              gdal_type, sizeof(ctype));
}
 %enddef

  DEFINE_DS_WRITE_RASTER(char, GDT_Byte)
  DEFINE_DS_WRITE_RASTER(short, GDT_Int16)
  DEFINE_DS_WRITE_RASTER(int, GDT_Int32)
  DEFINE_DS_WRITE_RASTER(int64_t, GDT_Int64)
  DEFINE_DS_WRITE_RASTER(float, GDT_Float32)
  DEFINE_DS_WRITE_RASTER(double, GDT_Float64)

%clear (int band_list, int *pband_list);

} /* extend */


%{
static
GIntBig ComputeBandRasterIOSize (int buf_xsize, int buf_ysize, int nPixelSize,
                                 GIntBig nPixelSpace, GIntBig nLineSpace,
                                 int bSpacingShouldBeMultipleOfPixelSize );

static CPLErr BandRasterIO( GDALRasterBandH hBand, GDALRWFlag eRWFlag,
                            int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *regularArrayOut, size_t nRegularArraySizeOut,
                            int nPixelSpace, int nLineSpace,
                            GDALDataType gdal_type, size_t sizeof_ctype)
{
    if ((gdal_type == GDT_Int16 && buf_type != GDT_Int16 && buf_type != GDT_UInt16 && buf_type != GDT_CInt16) ||
        (gdal_type == GDT_Int32 && buf_type != GDT_Int32 && buf_type != GDT_UInt32 && buf_type != GDT_CInt32) ||
        (gdal_type == GDT_Int64 && buf_type != GDT_Int64 && buf_type != GDT_UInt64) ||
        (gdal_type == GDT_Float32 && buf_type != GDT_Float32 && buf_type != GDT_CFloat32) ||
        (gdal_type == GDT_Float64 && buf_type != GDT_Float64 && buf_type != GDT_CFloat64))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Java array type is not compatible with GDAL data type");
        return CE_Failure;
    }

    GIntBig nMinBufferSizeInBytes = ComputeBandRasterIOSize (
                            buf_xsize, buf_ysize, GDALGetDataTypeSize(buf_type) / 8,
                            nPixelSpace, nLineSpace, sizeof_ctype > 1 );
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

    return GDALRasterIO( hBand, eRWFlag, xoff, yoff, xsize, ysize,
                                   regularArrayOut, buf_xsize, buf_ysize,
                                   buf_type, nPixelSpace, nLineSpace );
}

%}

%extend GDALRasterBandShadow {
  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, size_t nioBufferSize,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    return BandRasterIO( self, GF_Read,
                         xoff, yoff, xsize, ysize,
                         buf_xsize, buf_ysize,
                         buf_type,
                         nioBuffer, nioBufferSize,
                         nPixelSpace, nLineSpace,
                         GDT_Unknown, 0 );
  }

  %define DEFINE_READ_RASTER(ctype, gdal_type)
  CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                     int buf_xsize, int buf_ysize,
                     GDALDataType buf_type,
                     ctype *regularArrayOut, size_t nRegularArraySizeOut,
                     int nPixelSpace = 0, int nLineSpace = 0)
  {
    return BandRasterIO( self, GF_Read,
                         xoff, yoff, xsize, ysize,
                         buf_xsize, buf_ysize,
                         buf_type,
                         regularArrayOut, nRegularArraySizeOut,
                         nPixelSpace, nLineSpace,
                         gdal_type, sizeof(ctype) );
  }
  %enddef

  DEFINE_READ_RASTER(char, GDT_Byte)
  DEFINE_READ_RASTER(short, GDT_Int16)
  DEFINE_READ_RASTER(int, GDT_Int32)
  DEFINE_READ_RASTER(int64_t, GDT_Int64)
  DEFINE_READ_RASTER(float, GDT_Float32)
  DEFINE_READ_RASTER(double, GDT_Float64)

  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, size_t nioBufferSize,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    return BandRasterIO( self, GF_Write,
                         xoff, yoff, xsize, ysize,
                         buf_xsize, buf_ysize,
                         buf_type,
                         nioBuffer, nioBufferSize,
                         nPixelSpace, nLineSpace,
                         GDT_Unknown, 0 );
  }

  %define DEFINE_WRITE_RASTER(ctype, gdal_type)
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            ctype *regularArrayIn, size_t nRegularArraySizeIn,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    return BandRasterIO( self, GF_Write,
                         xoff, yoff, xsize, ysize,
                         buf_xsize, buf_ysize,
                         buf_type,
                         regularArrayIn, nRegularArraySizeIn,
                         nPixelSpace, nLineSpace,
                         gdal_type, sizeof(ctype) );
  }
  %enddef

  DEFINE_WRITE_RASTER(char, GDT_Byte)
  DEFINE_WRITE_RASTER(short, GDT_Int16)
  DEFINE_WRITE_RASTER(int, GDT_Int32)
  DEFINE_WRITE_RASTER(int64_t, GDT_Int64)
  DEFINE_WRITE_RASTER(float, GDT_Float32)
  DEFINE_WRITE_RASTER(double, GDT_Float64)

  CPLErr ReadBlock_Direct( int nXBlockOff, int nYBlockOff, void *nioBuffer, size_t nioBufferSize )
  {
    if (BandBlockReadWrite_Validate((GDALRasterBandH)self, nioBuffer, nioBufferSize) != CE_None)
      return CE_Failure;

    return GDALReadBlock(self, nXBlockOff, nYBlockOff, nioBuffer);
  }

  CPLErr WriteBlock_Direct( int nXBlockOff, int nYBlockOff, void *nioBuffer, size_t nioBufferSize )
  {
    if (BandBlockReadWrite_Validate((GDALRasterBandH)self, nioBuffer, nioBufferSize) != CE_None)
      return CE_Failure;

    return GDALWriteBlock(self, nXBlockOff, nYBlockOff, nioBuffer);
  }
/* %clear (void *nioBuffer, size_t nioBufferSize); */

%clear (char *regularArrayOut, size_t nRegularArraySizeOut);
%clear (short *regularArrayOut, size_t nRegularArraySizeOut);
%clear (int *regularArrayOut, size_t nRegularArraySizeOut);
%clear (int64_t *regularArrayOut, size_t nRegularArraySizeOut);
%clear (float *regularArrayOut, size_t nRegularArraySizeOut);
%clear (double *regularArrayOut, size_t nRegularArraySizeOut);

%clear (char *regularArrayIn, size_t nRegularArraySizeIn);
%clear (short *regularArrayIn, size_t nRegularArraySizeIn);
%clear (int *regularArrayIn, size_t nRegularArraySizeIn);
%clear (int64_t *regularArrayIn, size_t nRegularArraySizeIn);
%clear (float *regularArrayIn, size_t nRegularArraySizeIn);
%clear (double *regularArrayIn, size_t nRegularArraySizeIn);

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

  // Convenience method.
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

   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, long[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }

   public int ReadRaster(int xoff, int yoff, int xsize, int ysize, long[] array) {
       return ReadRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Int64, array);
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

   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, int buf_type, long[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, buf_type, array);
   }

   public int WriteRaster(int xoff, int yoff, int xsize, int ysize, long[] array) {
       return WriteRaster(xoff, yoff, xsize, ysize, xsize, ysize, gdalconstConstants.GDT_Int64, array);
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
                  OGRLayerShadow* CreateLayer,
                  OGRLayerShadow* CopyLayer,
                  OGRLayerShadow* GetLayerByIndex,
                  OGRLayerShadow* GetLayerByName,
                  OGRLayerShadow* ExecuteSQL,
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

  /* For backward compatibility */
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

%{
  static size_t GDALAttributeGetDimSize(GDALAttributeH attH, size_t index) {

    size_t size;

    size_t count;

    GUInt64* sizes = GDALAttributeGetDimensionsSize(attH, &count);

    if (index < 0 || index >= count) {

      size = (size_t) 0;
    }
    else {

      size = (size_t) sizes[index];
    }

    CPLFree(sizes);

    return size;
  }

%}

%extend GDALAttributeHS {

    size_t GetDimensionSize(size_t index) {
        return GDALAttributeGetDimSize(self, index);
    }

} /* extend */


%typemap(javacode) GDALAttributeHS %{

   public long[] GetDimensionSizes() {

       long size = GetDimensionCount();

       if (size > Integer.MAX_VALUE)
           throw new IllegalArgumentException("java array can hold at most "+Integer.MAX_VALUE+" values.");

       long[] arr = new long[(int) size];

       for (int i = 0; i < size; i++) {
           arr[i] = GetDimensionSize(i);
       }

       return arr;
   }
%}

%include callback.i

%include typemaps_java.i
