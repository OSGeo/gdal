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


%extend GDALRasterBandShadow {
%apply (void* nioBuffer, long nioBufferSize) { (void* nioBuffer, long nioBufferSize) };
  CPLErr ReadRaster_Direct( int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            void *nioBuffer, long nioBufferSize )
{
  if (nioBufferSize != buf_xsize * buf_ysize * (GDALGetDataTypeSize( buf_type ) / 8))
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
  if (nioBufferSize != buf_xsize * buf_ysize * (GDALGetDataTypeSize( buf_type ) / 8))
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



%typemap(javacode) GDALRasterBandShadow %{
  /* Ensure that the GC doesn't collect any Dataset instance set from Java */
  private Dataset datasetReference;
  protected void addReference(Dataset dataset) {
    datasetReference = dataset;
  }
  /* Ensure that the GC doesn't collect any Band instance set from Java */
  private Band bandReference;
  protected void addReference(Band band) {
    bandReference = band;
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

}


/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "jobject"
%typemap(jtype) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "ProgressCallback"
%typemap(jstype) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "ProgressCallback"
%typemap(javain) (GDALProgressFunc callback = NULL, void* callback_data=NULL)  "$javainput"
%typemap(javaout) (GDALProgressFunc callback = NULL, void* callback_data=NULL) {
    return $jnicall;
  }

%include typemaps_java.i
