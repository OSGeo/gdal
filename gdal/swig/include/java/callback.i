
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
