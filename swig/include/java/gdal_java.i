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
    static
    CPLErr DatasetRasterIO_Validate(GDALDatasetH hDS, int xoff, int yoff, int xsize, int ysize,
                                int buf_xsize, int buf_ysize,
                                GDALDataType buf_type,
                                void *nioBuffer, long nioBufferSize,
                                int band_list, int *pband_list,
                                int &nPixelSpace, int &nLineSpace, int &nBandSpace)
    {
        int nBands;
        if (buf_type < GDT_Byte || buf_type >= GDT_TypeCount)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid value for buffer type");
            return CE_Failure;
        }
        if (buf_xsize <= 0 || buf_ysize <= 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Illegal values for buffer size");
            return CE_Failure;
        }
        
        if (nPixelSpace < 0 || nLineSpace < 0 || nBandSpace < 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Illegal values for space arguments");
            return CE_Failure;
        }
        
        int nPixelSize = GDALGetDataTypeSize( buf_type ) / 8;
        
        if( nPixelSpace == 0 )
            nPixelSpace = nPixelSize;
        
        if( nLineSpace == 0 )
        {
            if (nPixelSpace > INT_MAX / buf_xsize)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
                return CE_Failure;
            }
            nLineSpace = nPixelSpace * buf_xsize;
        }
        
        if( nBandSpace == 0 )
        {
            if (nLineSpace > INT_MAX / buf_ysize)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
                return CE_Failure;
            }
            nBandSpace = nLineSpace * buf_ysize;
        }
        
        nBands = band_list;
        if (nBands == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid band count");
            return CE_Failure;
        }
        
        if ((buf_ysize - 1) > INT_MAX / nLineSpace ||
            (buf_xsize - 1) > INT_MAX / nPixelSpace ||
            (nBands - 1) > INT_MAX / nBandSpace ||
            (buf_ysize - 1) * nLineSpace > INT_MAX - (buf_xsize - 1) * nPixelSpace ||
            (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace > INT_MAX - (nBands - 1) * nBandSpace ||
            (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace + (nBands - 1) * nBandSpace > INT_MAX - nPixelSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
        
        int nMinBufferSize = (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace + (nBands - 1) * nBandSpace + nPixelSize;
        if (nioBufferSize < nMinBufferSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer not big enough");
            return CE_Failure;
        }
        
        return CE_None;
    }

    static
    CPLErr BandRasterIO_Validate(int buf_xsize, int buf_ysize,
                                 GDALDataType buf_type,
                                 void *nioBuffer, long nioBufferSize,
                                 int &nPixelSpace, int &nLineSpace)
    {
        if (buf_type < GDT_Byte || buf_type >= GDT_TypeCount)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid value for buffer type");
            return CE_Failure;
        }

        if (buf_xsize <= 0 || buf_ysize <= 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Illegal values for buffer size");
            return CE_Failure;
        }

        if (nPixelSpace < 0 || nLineSpace < 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Illegal values for space arguments");
            return CE_Failure;
        }

        int nPixelSize = GDALGetDataTypeSize( buf_type ) / 8;
        if( nPixelSpace == 0 )
            nPixelSpace = nPixelSize;

        if( nLineSpace == 0 )
        {
            if (nPixelSpace > INT_MAX / buf_xsize)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
                return CE_Failure;
            }
            nLineSpace = nPixelSpace * buf_xsize;
        }

        if ((buf_ysize - 1) > INT_MAX / nLineSpace ||
            (buf_xsize - 1) > INT_MAX / nPixelSpace ||
            (buf_ysize - 1) * nLineSpace > INT_MAX - (buf_xsize - 1) * nPixelSpace ||
            (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace > INT_MAX - nPixelSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }

        int nMinBufferSize = (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace + nPixelSize;
        if (nioBufferSize < nMinBufferSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer not big enough");
            return CE_Failure;
        }

        return CE_None;
    }
    
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

  CPLErr eErr;

  eErr = DatasetRasterIO_Validate( (GDALDatasetH)self, xoff, yoff, xsize, ysize, buf_xsize,
                            buf_ysize, buf_type, nioBuffer, nioBufferSize,
                            band_list, pband_list,
                            nPixelSpace, nLineSpace, nBandSpace);
  if (eErr == CE_None)
    eErr = GDALDatasetRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                nioBuffer, buf_xsize, buf_ysize,
                                buf_type, band_list, pband_list, nPixelSpace, nLineSpace, nBandSpace );

  return eErr;
}

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

  CPLErr eErr;

  eErr = DatasetRasterIO_Validate( (GDALDatasetH)self, xoff, yoff, xsize, ysize, buf_xsize,
                            buf_ysize, buf_type, nioBuffer, nioBufferSize,
                            band_list, pband_list,
                            nPixelSpace, nLineSpace, nBandSpace);
  if (eErr == CE_None)
    eErr = GDALDatasetRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                nioBuffer, buf_xsize, buf_ysize,
                                buf_type, band_list, pband_list, nPixelSpace, nLineSpace, nBandSpace );

  return eErr;
}
//%clear (void *nioBuffer, long nioBufferSize);
%clear (int band_list, int *pband_list);

} /* extend */

%extend GDALRasterBandShadow {
//%apply (void* nioBuffer, long nioBufferSize) { (void* nioBuffer, long nioBufferSize) };
  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    if (BandRasterIO_Validate(buf_xsize, buf_ysize, buf_type, nioBuffer, nioBufferSize,
                              nPixelSpace, nLineSpace) != CE_None)
      return CE_Failure;

    return GDALRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                   nioBuffer, buf_xsize, buf_ysize,
                                   buf_type, nPixelSpace, nLineSpace );

  }

  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int nPixelSpace = 0, int nLineSpace = 0)
  {
    if (BandRasterIO_Validate(buf_xsize, buf_ysize, buf_type, nioBuffer, nioBufferSize,
                              nPixelSpace, nLineSpace) != CE_None)
      return CE_Failure;

    return GDALRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                    nioBuffer, buf_xsize, buf_ysize,
                                    buf_type, nPixelSpace, nLineSpace );
  }

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
%clear (void *nioBuffer, long nioBufferSize);

%apply (int nList, int* pListOut) {(int buckets, int *panHistogram)};
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
    CPLErr err = GDALGetRasterHistogram( self, -0.5, 225.5, buckets, panHistogram,
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


%typemap(javaimports) GDALColorTableShadow %{
/* imports for getIndexColorModel */
import java.awt.image.IndexColorModel;
import java.awt.Color;
%}

%typemap(javacode) GDALColorTableShadow %{
/* convienance method */
  public IndexColorModel getIndexColorModel(int bits) {
    int size = GetCount();
    byte[] reds = new byte[size];
    byte[] greens = new byte[size];
    byte[] blues = new byte[size];
    byte[] alphas = new byte[size];

    Color entry = null;
    for(int i = 0; i < size; i++) {
      entry = GetColorEntry(i);
      reds[i] = (byte)entry.getRed();
      greens[i] = (byte)entry.getGreen();
      blues[i] = (byte)entry.getBlue();
      byte alpha = (byte)entry.getAlpha();
      alphas[i] = alpha;
    }
    return new IndexColorModel(bits, size, reds, greens, blues, alphas);
  }
%}


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
%}

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

%typemap(javainterfaces) GDALRasterAttributeTableShadow "Cloneable"

%typemap(javacode) GDALRasterAttributeTableShadow %{

  public int hashCode() {
     return (int)swigCPtr;
  }

  public Object clone()
  {
      return Clone();
  }
%}
  
/************************************************************************/
/*                       Stuff for progress callback                    */
/************************************************************************/


%header %{
typedef struct {
    JNIEnv *jenv;
    jobject pJavaCallback;
} JavaProgressData;
%}

%inline
%{
class ProgressCallback
{
public:
        virtual ~ProgressCallback() {  }
        virtual int run(double dfComplete, const char* pszMessage)
        {
            return 1;
        }
};

class TermProgressCallback : public ProgressCallback
{
public:
    TermProgressCallback()
    {
    }

    virtual int run(double dfComplete, const char* pszMessage)
    {
        return GDALTermProgress(dfComplete, pszMessage, NULL);
    }
};
%}

%{
/************************************************************************/
/*                        JavaProgressProxy()                           */
/************************************************************************/

int CPL_STDCALL
JavaProgressProxy( double dfComplete, const char *pszMessage, void *pData )
{
    JavaProgressData* psProgressInfo = (JavaProgressData*)pData;
    JNIEnv *jenv = psProgressInfo->jenv;
    int ret;
    const jclass progressCallbackClass = jenv->FindClass("org/gdal/gdal/ProgressCallback");
    const jmethodID runMethod = jenv->GetMethodID(progressCallbackClass, "run", "(DLjava/lang/String;)I");
    jstring temp_string = jenv->NewStringUTF(pszMessage);
    ret = jenv->CallIntMethod(psProgressInfo->pJavaCallback, runMethod, dfComplete, temp_string);
    jenv->DeleteLocalRef(temp_string);
    return ret;
}
%}

%typemap(arginit, noblock=1) ( GDALProgressFunc callback = NULL, void* callback_data=NULL)
{
    JavaProgressData sProgressInfo;
    sProgressInfo.jenv = jenv;
    sProgressInfo.pJavaCallback = NULL;

}

%typemap(in) ( GDALProgressFunc callback = NULL, void* callback_data=NULL) 
{
    if ( $input != 0 ) {
        sProgressInfo.pJavaCallback = $input;
        $1 = JavaProgressProxy;
        $2 = &sProgressInfo;
    }
    else
    {
        $1 = NULL;
        $2 = NULL;
    }
}


/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "jobject"
%typemap(jtype) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "ProgressCallback"
%typemap(jstype) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "ProgressCallback"
%typemap(javain) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "$javainput"
%typemap(javaout) (GDALProgressFunc callback = NULL, void* callback_data=NULL) {
    return $jnicall;
  }

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


%include typemaps_java.i
