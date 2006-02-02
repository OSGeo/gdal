/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_java.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for Java bindings
 * Author:   Benjamin Collins, The MITRE Corporation
 *
 *
 * $Log$
 * Revision 1.1  2006/02/02 20:56:07  collinsb
 * Added Java specific typemap code
 *
 *
*/

%include "arrays_java.i";
%include "typemaps.i"


/* JAVA TYPEMAPS */


%typemap(in) (double *val, int*hasval) ( double tmpval, int tmphasval ) {
  /* %typemap(in,numinputs=0) (double *val, int*hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
  if(jenv->GetArrayLength($input) < 1) {
    return $null;
  }
}
%typemap(argout) (double *val, int*hasval) {
  /* %typemap(argout) (double *val, int*hasval) */
  const jclass Double = jenv->FindClass("java/lang/Double");
  const jmethodID ctor = jenv->GetMethodID(Double, "<init>",
    "(D)V");
  if(*$2) {
    jobject dbl = jenv->NewObject(Double, ctor, tmpval$argnum);
    jenv->SetObjectArrayElement($input, (jsize)0, dbl);
  } else {
    jenv->SetObjectArrayElement($input, (jsize)0, 0);
  }
}

%typemap(jni) (double *val, int*hasval) "jobjectArray"
%typemap(jtype) (double *val, int*hasval) "Double[]"
%typemap(jstype) (double *val, int*hasval) "Double[]"
%typemap(javain) (double *val, int*hasval) "$javainput"
%typemap(javaout) (double *val, int*hasval) {
    return $jnicall;
  }


/*
%typemap(in) (GDALColorEntry *) (GDALColorEntry tmp) {
  /* %typemap(in) (GDALColorEntry *) (GDALColorEntry tmp) * /
  $1 = NULL;
  int *colorptr = 0;

  colorptr = (int *)jenv->GetIntArrayElements($input, 0);
  tmp.c1 = (short)colorptr[0];
  tmp.c2 = (short)colorptr[1];
  tmp.c3 = (short)colorptr[2];
  tmp.c4 = (short)colorptr[3];
  printf( "  %d, %d, %d, %d\n",
                    tmp.c1, tmp.c2, tmp.c3, tmp.c4 );
  $1 = &tmp;
}

%typemap(out) (GDALColorEntry *) {
  /* %typemap(out) (GDALColorEntry *) * /
  int colorptr[4];
  colorptr[0] = $1->c1;
  colorptr[1] = $1->c2;
  colorptr[2] = $1->c3;
  colorptr[3] = $1->c4;
  $result = jenv->NewIntArray(4);
  jenv->SetIntArrayRegion($result, 0, 4, (const jint*)colorptr);
}

%typemap(jni) (GDALColorEntry *) "jintArray"
%typemap(jtype) (GDALColorEntry *) "int[]"
%typemap(jstype) (GDALColorEntry *) "int[]"
%typemap(javain) (GDALColorEntry *) "$javainput"
%typemap(javaout) (GDALColorEntry *) {
    return $jnicall;
  }
*/


%typemap(in) (GDALColorEntry *) (GDALColorEntry tmp) {
  /* %typemap(in) (GDALColorEntry *) (GDALColorEntry tmp) */
  $1 = NULL;
  float *colorptr = 0;
  const jclass Color = jenv->FindClass("java/awt/Color");
  const jmethodID colors = jenv->GetMethodID(Color, "getRGBComponents",
    "([F)[F");

  jfloatArray colorArr = jenv->NewFloatArray(4);
  colorArr = (jfloatArray)jenv->CallObjectMethod($input, colors, colorArr); 

  colorptr = (float *)jenv->GetFloatArrayElements(colorArr, 0);
  tmp.c1 = (short)(colorptr[0] * 255);
  tmp.c2 = (short)(colorptr[1] * 255);
  tmp.c3 = (short)(colorptr[2] * 255);
  tmp.c4 = (short)(colorptr[3] * 255);
  /*printf( "  %d, %d, %d, %d\n",
                    tmp.c1, tmp.c2, tmp.c3, tmp.c4 );*/
  $1 = &tmp;
}

%typemap(out) (GDALColorEntry *) {
  /* %typemap(out) (GDALColorEntry *) */
  const jclass Color = jenv->FindClass("java/awt/Color");
  const jmethodID ccon = jenv->GetMethodID(Color, "<init>",
    "(IIII)V");
  $result = jenv->NewObject(Color, ccon, $1->c1, $1->c2, $1->c3, $1->c4);
}

%typemap(jni) (GDALColorEntry *) "jobject"
%typemap(jtype) (GDALColorEntry *) "java.awt.Color"
%typemap(jstype) (GDALColorEntry *) "java.awt.Color"
%typemap(javain) (GDALColorEntry *) "$javainput"
%typemap(javaout) (GDALColorEntry *) {
    return $jnicall;
  }




/*
 * Typemap argout of GDAL_GCP* used in Dataset::GetGCPs( )
 */
%typemap(in, numinputs=1) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs=0, GDAL_GCP *pGCPs=0 )
{
  /* %typemap(in,numinputs=1) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  $1 = &nGCPs;
  $2 = &pGCPs;
}
%typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs )
{
  /* %typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  const jclass GCPClass = jenv->FindClass("org/gdal/gdal/GCP");
  const jclass vectorClass = jenv->FindClass("java/util/Vector");
  const jmethodID add = jenv->GetMethodID(vectorClass, "add", "(Ljava/lang/Object;)Z");
  const jmethodID GCPcon = jenv->GetMethodID(GCPClass, "<init>",
    "(DDDDDLjava/lang/String;Ljava/lang/String;)V");

  for( int i = 0; i < *$1; i++ ) {
    jobject GCPobj = jenv->NewObject(GCPClass, GCPcon, 
                                (*$2)[i].dfGCPX,
                                (*$2)[i].dfGCPY,
                                (*$2)[i].dfGCPZ,
                                (*$2)[i].dfGCPPixel,
                                (*$2)[i].dfGCPLine,
                                (*$2)[i].pszInfo,
                                (*$2)[i].pszId );
	
    jenv->CallBooleanMethod($input, add, GCPobj);
  }
  //$result = $input;
}

%typemap(jni) (int *nGCPs, GDAL_GCP const **pGCPs ) "jobject"
%typemap(jtype) (int *nGCPs, GDAL_GCP const **pGCPs ) "java.util.Vector"
%typemap(jstype) (int *nGCPs, GDAL_GCP const **pGCPs ) "java.util.Vector"
%typemap(javain) (int *nGCPs, GDAL_GCP const **pGCPs ) "$javainput"
%typemap(javaout) (int *nGCPs, GDAL_GCP const **pGCPs ) {
    return $jnicall;
  }





%typemap(in) (int *nLen, char **pBuf ) ( int nLen, char *pBuf )
{
  /* %typemap(in) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}

%typemap(argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */
  /* make sure that the passed array is at lease length 1 */
  if(jenv->GetArrayLength($input) >= 1) {
    jcharArray charArray = jenv->NewCharArray(nLen$argnum);
    jenv->SetCharArrayRegion(charArray, (jsize)0, (jsize)nLen$argnum, (jchar*)pBuf$argnum);
    jenv->SetObjectArrayElement($input,0,charArray);
  }
}

%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( $1 ) {
    free( *$2 );
  }
}

%typemap(jni) (int *nLen, char **pBuf ) "jobjectArray"
%typemap(jtype) (int *nLen, char **pBuf ) "char[][]"
%typemap(jstype) (int *nLen, char **pBuf ) "char[][]"
%typemap(javain) (int *nLen, char **pBuf ) "$javainput"
%typemap(javaout) (int *nLen, char **pBuf ) {
    return $jnicall;
  }


%fragment("OGRErrMessages","header") %{
static char const *
OGRErrMessages( int rc ) {
  switch( rc ) {
  case 0:
    return "OGR Error %d: None";
  case 1:
    return "OGR Error %d: Not enough data";
  case 2:
    return "OGR Error %d: Unsupported geometry type";
  case 3:
    return "OGR Error %d: Unsupported operation";
  case 4:
    return "OGR Error %d: Corrupt data";
  case 5:
    return "OGR Error %d: General Error";
  case 6:
    return "OGR Error %d: Unsupported SRS";
  default:
    return "OGR Error %d: Unknown";
  }
}
%}

%typemap(in) (int nLen, char *pBuf ) (jboolean isCopy)
{
  /* %typemap(in) (int nLen, char *pBuf ) */
  $1 = jenv->GetArrayLength($input);
  $2 = (char *)jenv->GetCharArrayElements($input, &isCopy);
}

%typemap(argout) (int nLen, char *pBuf )
{
  /* %typemap(argout) (int nLen, char *pBuf ) */
}

%typemap(freearg) (int nLen, char *pBuf )
{
  /* %typemap(freearg) (int nLen, char *pBuf ) */
  /* This calls JNI_ABORT, so any modifications will not be passed back
      into the Java caller
   */
  if(isCopy$argnum == JNI_TRUE) {
    jenv->ReleaseCharArrayElements($input, (jchar *)$2, 0);
  }
}

%typemap(jni) (int nLen, char *pBuf ) "jcharArray"
%typemap(jtype) (int nLen, char *pBuf ) "char[]"
%typemap(jstype) (int nLen, char *pBuf ) "char[]"
%typemap(javain) (int nLen, char *pBuf ) "$javainput"
%typemap(javaout) (int nLen, char *pBuf ) {
    return $jnicall;
  }


%typemap(in) (tostring argin)
{
  /* %typemap(in) (tostring argin) */
  $1 = (char *)jenv->GetStringUTFChars($input, 0);
}
%typemap(freearg) (tostring argin)
{
  /* %typemap(in) (tostring argin) */
  jenv->ReleaseStringUTFChars($input, (char*)$1);
}

%typemap(jni) (tostring argin) "jstring"
%typemap(jtype) (tostring argin) "String"
%typemap(jstype) (tostring argin) "String"
%typemap(javain) (tostring argin) "$javainput"
%typemap(javaout) (tostring argin) {
    return $jnicall;
  }


%typemap(in) (char **ignorechange) (char *val)
{
  /* %typemap(in) (char **ignorechange) */
  val = (char *)jenv->GetStringUTFChars($input, 0);
  $1 = &val;
}
%typemap(freearg) (char **ignorechange)
{
  /* %typemap(freearg) (char **ignorechange) */
  jenv->ReleaseStringUTFChars($input, val$argnum);
}

%typemap(jni) (char **ignorechange) "jstring"
%typemap(jtype) (char **ignorechange) "String"
%typemap(jstype) (char **ignorechange) "String"
%typemap(javain) (char **ignorechange) "$javainput"
%typemap(javaout) (char **ignorechange) {
    return $jnicall;
  }


%typemap(out,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(out) OGRErr */
  if (result != 0) {
    SWIG_JavaThrowException(jenv, SWIG_JavaRuntimeException,
      OGRErrMessages(result));
    return $null;
  }
  $result = (jint)result;
}
%typemap(ret) OGRErr
{
  /* %typemap(ret) OGRErr */

}


/* GDAL Typemaps */

%typemap(out) IF_ERR_RETURN_NONE
{
  /* %typemap(out) IF_ERR_RETURN_NONE */
  $result = 0;
}
%typemap(ret) IF_ERR_RETURN_NONE
{
 /* %typemap(ret) IF_ERR_RETURN_NONE */
}
%typemap(out) IF_FALSE_RETURN_NONE
{
  /* %typemap(out) IF_FALSE_RETURN_NONE */
  $result = 0;
}
%typemap(ret) IF_FALSE_RETURN_NONE
{
 /* %typemap(ret) IF_FALSE_RETURN_NONE */
}

/***************************************************
 *
 *  Java typemaps for (int nList, int* pList)
 *
 ***************************************************/ 
%typemap(in) (int nList, int* pList)
{
  /* %typemap(in) (int nList, int* pList) */
  /* check if is List */
  $1 = jenv->GetArrayLength($input);
  $2 = (int *)jenv->GetIntArrayElements($input, NULL);
}

%typemap(argout) (int nList, int* pList)
{
  /* %typemap(argout) (int nList, int* pList) */
}

%typemap(freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}

%typemap(jni) (int nList, int* pList) "jintArray"
%typemap(jtype) (int nList, int* pList) "int[]"
%typemap(jstype) (int nList, int* pList) "int[]"
%typemap(javain) (int nLen, int *pList ) "$javainput"
%typemap(javaout) (int nLen, int *pList ) {
    return $jnicall;
  }



%typemap(in) char **dict
{
  /* %typemap(in) char **dict */
  /* Convert the Hashtable to a char array */
  $1 = NULL;
  if($input != 0) {
    const jclass hashtable = jenv->FindClass("java/util/Hashtable");
    const jclass enumeration = jenv->FindClass("java/util/Enumeration");
    const jmethodID get = jenv->GetMethodID(hashtable, "get",
      "(Ljava/lang/Object;)Ljava/lang/Object;");
    const jmethodID keys = jenv->GetMethodID(hashtable, "keys",
      "()Ljava/lang/Enumeration;");
    const jmethodID hasMoreElements = jenv->GetMethodID(enumeration, 
      "hasMoreElements", "()Z");
    const jmethodID getNextElement = jenv->GetMethodID(enumeration,
      "getNextElement", "()Ljava/lang/Object;");
    for (jobject keyset = jenv->CallObjectMethod($input, keys);
          jenv->CallBooleanMethod(keyset, hasMoreElements) == JNI_TRUE;) {
      jstring key = (jstring)jenv->CallObjectMethod(keyset, getNextElement);
      jstring value = (jstring)jenv->CallObjectMethod($input, get, key);
      const char *keyptr = jenv->GetStringUTFChars(key, 0);
      const char *valptr = jenv->GetStringUTFChars(value, 0);
      $1 = CSLAddNameValue($1, keyptr, valptr);
      jenv->ReleaseStringUTFChars(key, keyptr);
      jenv->ReleaseStringUTFChars(value, valptr);
    }
  }
}

%typemap(out) char **dict
{
  /* %typemap(out) char ** -> to hash */
  /* Convert a char array to a Hashtable */
  char **stringarray = $1;
  const jclass hashtable = jenv->FindClass("java/util/Hashtable");
  const jmethodID constructor = jenv->GetMethodID(hashtable, "<init>", "()V");
  const jmethodID put = jenv->GetMethodID(hashtable, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  $result = jenv->NewObject(hashtable, constructor);
  if ( stringarray != NULL ) {
    while (*stringarray != NULL ) {
      char const *valptr;
      char *keyptr;
      /*printf("working on pair: %s\n", *stringarray);*/
      valptr = CPLParseNameValue( *stringarray, &keyptr );
      if ( valptr != 0 ) {
        jstring name = jenv->NewStringUTF(keyptr);
        jstring value = jenv->NewStringUTF(valptr);
        jenv->CallObjectMethod($result, put, name, value);
        CPLFree( keyptr );
      }
      stringarray++;
    }
  }
}

%typemap(freearg) char **dict
{
  /* %typemap(freearg) char **dict */
  CSLDestroy( $1 );
}

%typemap(jni) (char **dict) "jobject"
%typemap(jtype) (char **dict) "java.util.Hashtable"
%typemap(jstype) (char **dict) "java.util.Hashtable"
%typemap(javain) (char **dict) "$javainput"
%typemap(javaout) (char **dict) {
    return $jnicall;
  }



/*
 * Typemap maps char** arguments from a Vector
 */
%typemap(in) char **options
{
  /* %typemap(in) char **options */
  $1 = NULL;
  if($input != 0) {
    const jclass vector = jenv->FindClass("java/util/Vector");
    const jclass enumeration = jenv->FindClass("java/util/Enumeration");
    const jmethodID elements = jenv->GetMethodID(vector, "elements",
      "()Ljava/util/Enumeration;");
    const jmethodID hasMoreElements = jenv->GetMethodID(enumeration, 
      "hasMoreElements", "()Z");
    const jmethodID getNextElement = jenv->GetMethodID(enumeration,
      "nextElement", "()Ljava/lang/Object;");
    if(vector == NULL || enumeration == NULL || elements == NULL ||
        hasMoreElements == NULL || getNextElement == NULL) {
          fprintf(stderr, "Could not load (options **) jni types.\n");
          return $null;
        }
    for (jobject keys = jenv->CallObjectMethod($input, elements);
          jenv->CallBooleanMethod(keys, hasMoreElements) == JNI_TRUE;) {
      jstring value = (jstring)jenv->CallObjectMethod(keys, getNextElement);
      const char *valptr = jenv->GetStringUTFChars(value, 0);
      $1 = CSLAddString($1,  valptr);
      jenv->ReleaseStringUTFChars(value, valptr);
    }
  }
}
%typemap(freearg) char **options
{
  /* %typemap(freearg) char **options */
  CSLDestroy( $1 );
}
%typemap(out) char **options
{
  /* %typemap(out) char ** -> ( string ) */
  char **stringarray = $1;
  const jclass vector = jenv->FindClass("java/util/Vector");
  const jmethodID constructor = jenv->GetMethodID(vector, "<init>", "()V");
  const jmethodID add = jenv->GetMethodID(vector, "add", "(Ljava/lang/Object;)Z");

  $result = jenv->NewObject(vector, constructor);
  if ( stringarray != NULL ) {
    while(*stringarray != NULL) {
      /*printf("working on string %s\n", *stringarray);*/
      jstring value = (jstring)jenv->NewStringUTF(*stringarray);
      jenv->CallBooleanMethod($result, add, value);
      stringarray++;
    }
  }
}

%typemap(jni) (char **options) "jobject"
%typemap(jtype) (char **options) "java.util.Vector"
%typemap(jstype) (char **options) "java.util.Vector"
%typemap(javain) (char **options) "$javainput"
%typemap(javaout) (char **options) {
    return $jnicall;
  }



%define OPTIONAL_POD(type,argstring)
%typemap(in) (type *optional_##type)
{
  /* %typemap(in) (type *optional_##type) */
  $1 = ($1_type)$input;
}
%typemap(argout) (type *optional_##type)
{
  /* %typemap(in) (type *optional_##type) */
}
%typemap(typecheck,precedence=0) (type *optional_##type)
{
  /* %typemap(typecheck,precedence=0) (type *optionalInt) */
}

%typemap(jni) (type *optional_##type) "jintArray"
%typemap(jtype) (type *optional_##type) "int[]"
%typemap(jstype) (type *optional_##type) "int[]"
%typemap(javain) (type *optional_##type) "$javainput"
%typemap(javaout) (type *optional_##type) {
    return $jnicall;
  }
%enddef

OPTIONAL_POD(int,i);



/*
 * Typemap for char **argout. 
 */
%typemap(in) (char **argout) ( char *argout=0 )
{
  /* %typemap(in) (char **argout) */
  $1 = &argout;
}

%typemap(argout) (char **argout)
{
  /* %typemap(argout) (char **argout) */
  jstring temp_string;

  if((int)jenv->GetArrayLength($input) >= 1) {
    temp_string = jenv->NewStringUTF(argout$argnum);
    jenv->SetObjectArrayElement($input, 0, temp_string);
    jenv->DeleteLocalRef(temp_string);
  }
}

%typemap(freearg) (char **argout)
{
  /* %typemap(freearg) (char **argout) */
  if($1) {
    free((void *)argout$argnum);
  }
}

%typemap(jni) (char **argout) "jobjectArray"
%typemap(jtype) (char **argout) "String[]"
%typemap(jstype) (char **argout) "String[]"
%typemap(javain) (char **argout) "$javainput"
%typemap(javaout) (char **argout) {
    return $jnicall;
  }





%typemap(in) (double *argout[ANY]) (double *argout[$dim0])
{
  /* %typemap(in) (double *argout[ANY]) */
  $1 = argout;
}

%typemap(argout) (double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */
  jenv->SetDoubleArrayRegion($input, (jsize)0, (jsize)$dim0, $1[0]);
}

%typemap(freearg) (double *argout[ANY])
{
  /* %typemap(freearg) (double *argout[ANY]) */
  CPLFree($1);
}

%typemap(jni) (double *argout[ANY]) "jdoubleArray"
%typemap(jtype) (double *argout[ANY]) "double[]"
%typemap(jstype) (double *argout[ANY]) "double[]"
%typemap(javain) (double *argout[ANY]) "$javainput"
%typemap(javaout) (double *argout[ANY]) {
    return $jnicall;
  }



%typemap(in) (double argin[ANY]) (jboolean isCopy)
{
  /* %typemap(in) (double argin[ANY]) */
  $1 = (double *)jenv->GetDoubleArrayElements($input, &isCopy);
}

%typemap(argout) (double argin[ANY])
{
  /* %typemap(argout) (double argin[ANY]) */
}

%typemap(freearg) (double argin[ANY])
{
  /* %typemap(in) (double argin[ANY]) */
  if(isCopy$argnum == JNI_TRUE) {
    jenv->ReleaseDoubleArrayElements($input, (jdouble *)$1, 0);
  }
}

%typemap(jni) (double argin[ANY]) "jdoubleArray"
%typemap(jtype) (double argin[ANY]) "double[]"
%typemap(jstype) (double argin[ANY]) "double[]"
%typemap(javain) (double argin[ANY]) "$javainput"
%typemap(javaout) (double argin[ANY]) {
    return $jnicall;
  }


/* This tells SWIG to treat char ** as a special case when used as a parameter
   in a function call */
%typemap(in) char ** (jint size) {
  /* %typemap(in) char ** (jint size) */
    int i = 0;
    size = jenv->GetArrayLength($input);
    $1 = (char **) malloc((size+1)*sizeof(char *));
    /* make a copy of each string */
    for (i = 0; i<size; i++) {
        jstring j_string = (jstring)jenv->GetObjectArrayElement($input, i);
        const char * c_string = jenv->GetStringUTFChars(j_string, 0);
        $1[i] = (char *)malloc(strlen((c_string)+1)*sizeof(const char *));
        strcpy($1[i], c_string);
        jenv->ReleaseStringUTFChars(j_string, c_string);
        jenv->DeleteLocalRef(j_string);
    }
    $1[i] = 0;
}

/* This cleans up the memory we malloc'd before the function call */
%typemap(freearg) char ** {
  /* %typemap(freearg) char ** */
    int i;
    for (i=0; i<size$argnum-1; i++)
      free($1[i]);
    free($1);
}

/* This allows a C function to return a char ** as a Java String array */
%typemap(out) char ** {
  /* %typemap(out) char ** */
    int i;
    int len=0;
    jstring temp_string;
    const jclass clazz = jenv->FindClass("java/lang/String");

    while ($1[len]) len++;    
    jresult = jenv->NewObjectArray(len, clazz, NULL);
    /* exception checking omitted */

    for (i=0; i<len; i++) {
      temp_string = jenv->NewStringUTF(*result++);
      jenv->SetObjectArrayElement(jresult, i, temp_string);
      jenv->DeleteLocalRef(temp_string);
    }
}

/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) char ** "jobjectArray"
%typemap(jtype) char ** "String[]"
%typemap(jstype) char ** "String[]"
%typemap(javain) char ** "$javainput"
%typemap(javaout) char ** {
    return $jnicall;
  }




%typemap(in) void * {
  /* %typemap(in) void * */
  $1 = jenv->GetDirectBufferAddress($input);
}

/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) void * "jobject"
%typemap(jtype) void * "java.nio.ByteBuffer"
%typemap(jstype) void * "java.nio.ByteBuffer"
%typemap(javain) void * "$javainput"
%typemap(javaout) void * {
    return $jnicall;
  }

