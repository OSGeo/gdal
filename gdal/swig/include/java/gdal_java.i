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

%extend GDALDatasetShadow {
%apply (int nList, int* pList) { (int band_list, int *pband_list) };
%apply (void* nioBuffer, long nioBufferSize) { (void* nioBuffer, long nioBufferSize) };
  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int band_list, int *pband_list)
{
  int nBands;
  if (buf_type < GDT_Byte || buf_type >= GDT_TypeCount)
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Invalid buffer type");
      return CE_Failure;
  }
  if (band_list)
          nBands = band_list;
  else
          nBands = GDALGetRasterCount(self);
  if (nioBufferSize < buf_xsize * buf_ysize * (GDALGetDataTypeSize( buf_type ) / 8) * nBands)
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Inconsitant buffer size");
      return CE_Failure;
  }
  return GDALDatasetRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                              nioBuffer, buf_xsize, buf_ysize,
                              buf_type, band_list, pband_list, 0, 0, 0 );

}

  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize,
                            int band_list, int *pband_list)
{
  int nBands;
  if (buf_type < GDT_Byte || buf_type >= GDT_TypeCount)
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Invalid buffer type");
      return CE_Failure;
  }
  if (band_list)
          nBands = band_list;
  else
          nBands = GDALGetRasterCount(self);
  if (nioBufferSize < buf_xsize * buf_ysize * (GDALGetDataTypeSize( buf_type ) / 8) * nBands)
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Inconsitant buffer size");
      return CE_Failure;
  }
  return GDALDatasetRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                              nioBuffer, buf_xsize, buf_ysize,
                              buf_type, band_list, pband_list, 0, 0, 0 );

}
//%clear (void *nioBuffer, long nioBufferSize);
%clear (int band_list, int *pband_list);

} /* extend */

%extend GDALRasterBandShadow {
//%apply (void* nioBuffer, long nioBufferSize) { (void* nioBuffer, long nioBufferSize) };
  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize )
{
  if (buf_type < GDT_Byte || buf_type >= GDT_TypeCount)
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Invalid buffer type");
      return CE_Failure;
  }
  if (nioBufferSize < buf_xsize * buf_ysize * (GDALGetDataTypeSize( buf_type ) / 8))
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Inconsitant buffer size");
      return CE_Failure;
  }
  return GDALRasterIO( self, GF_Read, xoff, yoff, xsize, ysize,
                                 nioBuffer, buf_xsize, buf_ysize,
                                 buf_type, 0, 0 );

}

  CPLErr WriteRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize )
{
  if (buf_type < GDT_Byte || buf_type >= GDT_TypeCount)
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Invalid buffer type");
      return CE_Failure;
  }
  if (nioBufferSize < buf_xsize * buf_ysize * (GDALGetDataTypeSize( buf_type ) / 8))
  {
      CPLError(CE_Failure, CPLE_AppDefined, "Inconsitant buffer size");
      return CE_Failure;
  }
  return GDALRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                 nioBuffer, buf_xsize, buf_ysize,
                                 buf_type, 0, 0 );

}
%clear (void *nioBuffer, long nioBufferSize);

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
  public int Checksum() {
    return Checksum(0, 0, getXSize(), getYSize());
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
%typemap(javaout) GDALRasterBandShadow* GetRasterBand {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) GDALRasterBandShadow* GetOverview {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) GDALRasterBandShadow* GetMaskBand {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) GDALColorTableShadow* GetColorTable {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) GDALColorTableShadow* GetRasterColorTable {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

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
