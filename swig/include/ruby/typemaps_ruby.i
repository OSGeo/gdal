
/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_ruby.i
 * Project:  GDAL Ruby Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Charles F. I. Savage
 *

 *
 * $Log$
 * Revision 1.1  2005/09/26 08:20:19  cfis
 * Significantly updated typemaps for Ruby - resynced with the Python typemaps file.
 *
 * Revision 1.5  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 * Revision 1.4  2005/08/25 21:00:55  cfis
 * Added note saying that SWIG 1.3.26 or higher is required because the bindings need the SWIGTYPE *DISOWN  typemap.
 *
 * Revision 1.3  2005/08/21 23:52:08  cfis
 * The Layer each method was not correctly setting the owernship flag for returned objects.  This has now been fixed and commented.
 *
 * Revision 1.2  2005/08/20 20:50:13  cfis
 * Added GetLayer method that maps to either GetLayerByName or GetLayerByIndex.  Also commented out Open and OpenShared as DataSouce class static methods.
 *
 * Revision 1.1  2005/08/09 17:40:09  kruland
 * Added support for ruby.
 *
 */

/* !NOTE! - The Ruby bindings require SWIG-1.3.26 or above.  Earlier versions
   do not work because they did not include support for the WWIGTYPE *DISOWN
	typemap which is crucial for supporting the AddGeometryDirectly and 
	SetGeometryDirectly methods.

	These typemaps  were ported from typemaps_python.i.  For more information
	please refer to that file and to the README.typemaps file */

%include typemaps.i
%include renames.i
%include ogr_error_map.i



%apply (double *OUTPUT) { double *argout };

/*
 * double *val, int*hasval, is a special contrived typemap used for
 * the RasterBand GetNoDataValue, GetMinimum, GetMaximum, GetOffset, GetScale methods.
 * In the Ruby bindings, the variable hasval is tested.  If it is 0 (is, the value
 * is not set in the raster band) then Py_None is returned.  If is is != 0, then
 * the value is coerced into a long and returned.
 */
%typemap(ruby,in,numinputs=0) (double *val, int*hasval) ( double tmpval, int tmphasval ) {
  /* %typemap(ruby,in,numinputs=0) (double *val, int*hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
}

%typemap(ruby,argout) (double *val, int*hasval) {
  /* %typemap(ruby,argout) (double *val, int*hasval) */

  if ( !*$2 ) {
    $result = Qnil;
  }
  else {
    $result = rb_float_new(*$1);
  }
}

/* Define a simple return code typemap which checks if the return code from
 * the wrapped method is non-zero. If non-zero, return None.  Otherwise,
 * return any argout or None.
 *
 * Applied like this:
 * %apply (IF_ERR_RETURN_NONE) {CPLErr};
 * CPLErr function_to_wrap( );
 * %clear (CPLErr); */

%typemap(ruby,out) IF_ERR_RETURN_NONE
{
  /* %typemap(ruby,out) IF_ERR_RETURN_NONE */
  /* result = Qnil; */
}

%typemap(ruby,out) IF_FALSE_RETURN_NONE
{
  /* %typemap(ruby,out) IF_FALSE_RETURN_NONE */
  if ($1 == 0 ) {
    $result = Qnil;
  }
}

/* --------  OGR Error Handling --------------- */
%typemap(ruby, out) OGRErr
{
  if (result != 0) {
    rb_raise(rb_eRuntimeError, OGRErrMessages(result));
  }
}

/* -------------  Ruby Array  <-> Fixed Length Double Array  ----------------------*/
%typemap(ruby,in,numinputs=0) ( double argout[ANY]) (double argout[$dim0])
{
  /* %typemap(ruby,in,numinputs=0) (double argout[ANY]) */
  $1 = argout;
}

%typemap(ruby,argout) ( double argout[ANY])
{
  /* %typemap(ruby,argout) (double argout[ANY]) */
  $result = rb_ary_new();

  for(int i=0; i<$dim0; i++)
  {
    VALUE value = rb_float_new($1[i]);
    rb_ary_push($result, value);
  }
}

%typemap(ruby,in,numinputs=0) ( double *argout[ANY]) (double *argout)
{
  /* %typemap(ruby,in,numinputs=0) (double *argout[ANY]) */
  $1 = &argout;
}

%typemap(ruby,argout) ( double *argout[ANY])
{
  /* %typemap(ruby,argout) (double argout[ANY]) */
  $result = rb_ary_new();

  for(int i=0; i<$dim0; i++)
  {
    VALUE value = rb_float_new(*$1[i]);
    rb_ary_push($result, value);
  }
}

%typemap(ruby,freearg) (double *argout[ANY])
{
  /* %typemap(ruby, freearg) (double *argout[ANY]) */
  CPLFree(*$1);
}

%typemap(ruby,in) (double argin[ANY]) (double argin[$dim0])
{
  /* %typemap(ruby,in) (double argin[ANY]) (double argin[$dim0]) */
  /* Make sure this is an array. */
  Check_Type($input, T_ARRAY);

  /* Get the length */
  int seq_size = RARRAY($input)->len;
  
  if ( seq_size != $dim0 ) {
    rb_raise(rb_eRangeError, "sequence must have length %i.", seq_size);
  }

  for( int i = 0; i<$dim0; i++ ) {
    /* Get the Ruby Object */
    VALUE item = rb_ary_entry($input,i);
    
    /* Convert to double */
    $1[i] = NUM2DBL(item);
  }
}

/* -------------  Ruby Array  <-> integer Array  ----------------------*/
%typemap(ruby,in,numinputs=1) (int nList, int* pList)
{
  /* %typemap(ruby,in,numinputs=1) (int nList, int* pList) */

  /* Make sure this is an array. */
  Check_Type($input, T_ARRAY);

  /* Get the length */
  $1 = RARRAY($input)->len;
  
  /* Allocate space for the C array. */
  $2 = (int*) malloc($1*sizeof(int));
  
  for( int i = 0; i<$1; i++ ) {
    /* Get the Ruby Object */
    VALUE item = rb_ary_entry($input,i);
    /* Conver to an integer */
    $2[i] = NUM2INT(item);
  }
}

%typemap(ruby,freearg) (int nList, int* pList)
{
  /* %typemap(ruby,freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}

/* -------------  Ruby String  <-> char ** with lengths ----------------------*/
%typemap(ruby,in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen = 0, char *pBuf = 0 )
{
  /* %typemap(ruby,in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen = 0, char *pBuf = 0 ) */
  $1 = &nLen;
  $2 = &pBuf;
}

%typemap(ruby,argout) (int *nLen, char **pBuf )
{
  /* %typemap(ruby,argout) (int *nLen, char **pBuf ) */
  $result = rb_str_new(*$2, *$1);
}

%typemap(ruby,freearg) (int *nLen, char **pBuf )
{
  /* %typemap(ruby,freearg) (int *nLen, char **pBuf ) */
  if( *$2 ) {
    free( *$2 );
  }
}

/* -------------  Ruby String  <-> char *  ----------------------*/
%typemap(ruby,in) (int nLen, char *pBuf ) = (int LENGTH, char *STRING);

%typemap(ruby,typecheck,precedence=SWIG_TYPECHECK_POINTER)
        (int nLen, char *pBuf)
{
  /* %typecheck(ruby,typecheck,precedence=SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf) */
  $1 = (TYPE($input) == T_STRING) ? 1: 0;
}



/*  ---------    GDAL_GCP used in Dataset::GetGCPs( ) ----------- */
%typemap(ruby,in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs=0, GDAL_GCP *pGCPs=0 )
{
  /* %typemap(ruby, in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  $1 = &nGCPs;
  $2 = &pGCPs;
}

%typemap(ruby,argout) (int *nGCPs, GDAL_GCP const **pGCPs )
{
  /* %typemap(ruby, argout) (int *nGCPs, GDAL_GCP const **pGCPs ) */

  $result = rb_ary_new2(*$1);

  for( int i = 0; i < *$1; i++ ) {
    GDAL_GCP *o = new_GDAL_GCP( (*$2)[i].dfGCPX,
                                (*$2)[i].dfGCPY,
                                (*$2)[i].dfGCPZ,
                                (*$2)[i].dfGCPPixel,
                                (*$2)[i].dfGCPLine,
                                (*$2)[i].pszInfo,
                                (*$2)[i].pszId );
	
	 rb_ary_store($result, i, 
					  SWIG_NewPointerObj((void*)o, SWIGTYPE_p_GDAL_GCP,1));
  }
}

%typemap(ruby,in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) ( GDAL_GCP *tmpGCPList )
{
  /* %typemap(ruby, in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) */

  /* Check if is a list */
  Check_Type($input, T_ARRAY);

  $1 = RARRAY($input)->len;
  tmpGCPList = (GDAL_GCP*) malloc($1*sizeof(GDAL_GCP));
  $2 = tmpGCPList;

  for( int i = 0; i<$1; i++ ) {
    VALUE rubyItem = rb_ary_entry($input,i);
    GDAL_GCP *item = 0;

    SWIG_ConvertPtr( rubyItem, (void**)&item, SWIGTYPE_p_GDAL_GCP, SWIG_POINTER_EXCEPTION | 0 );

	 if (!item) {
		 rb_raise(rb_eRuntimeError, "GDAL_GCP item cannot be nil");
	 }

    memcpy( (void*) item, (void*) tmpGCPList, sizeof( GDAL_GCP ) );
    ++tmpGCPList;
  }
}

%typemap(ruby,freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
  /* %typemap(ruby, freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
  if ($2) {
    free( (void*) $2 );
  }
}

/* ----------- Typemap for GDALColorEntry* <-> tuple -------------- */
/*%typemap(ruby,out) GDALColorEntry*
{*/
  /* %typemap(ruby, out) GDALColorEntry* */

 /* $result = Py_BuildValue( "(hhhh)", (*$1).c1,(*$1).c2,(*$1).c3,(*$1).c4);
}

%typemap(ruby,in) GDALColorEntry* (GDALColorEntry ce)
{*/
  /* %typemap(in) GDALColorEntry* */
/*   ce.c4 = 255;

   int size = PySequence_Size($input);

   if ( size > 4 ) {
     PyErr_SetString(PyExc_TypeError, "ColorEntry sequence too long");
     SWIG_fail;
   }

   if ( size < 3 ) {
     PyErr_SetString(PyExc_TypeError, "ColorEntry sequence too short");
     SWIG_fail;
   }

   PyArg_ParseTuple( $input,"hhh|h", &ce.c1, &ce.c2, &ce.c3, &ce.c4 );
   $1 = &ce;
}
*/


/* -------------   Ruby Hash <-> char **  ----------------------
 * Used to convert a native dictionary/hash type into name value pairs. */

/*  Hash -> char** */
%typemap(ruby,in) char **dict
{
  /* %typemap(ruby,in) char **dict */

  $1 = NULL;
  
  /* is the provided object an array or a hash? */
  if ( TYPE($input) == T_ARRAY) {
    /* get the size of the array */
    int size = RARRAY($input)->len;
    
    for (int i = 0; i < size; i++) {
      /* get the ruby object */
      VALUE value = rb_ary_entry($input, i);
      
      /* Convert the value to a string via ruby duck typing 
       * (i.e., the object might not actually be a string)
       */
      char *pszItem = StringValuePtr(value);
      $1 = CSLAddString( $1, pszItem );
    }
  }
  
  else if ( TYPE($input) == T_HASH) {
    /* This is a hash - get the size by calling via the ruby method */
    int size = NUM2INT(rb_funcall($input, rb_intern("size"), 0, NULL));

    if ( size > 0 ) {
      /* Get the keys by caling via ruby */
      VALUE keys_arr = rb_funcall($input, rb_intern("keys"), 0, NULL);

      for( int i=0; i<size; i++ ) {
      	/* Get the key and value as ruby objects */
        VALUE key = rb_ary_entry(keys_arr, i);
        VALUE value = rb_hash_aref($input, key);
		
        /* Convert the key and value to strings via ruby duck typing 
         * (i.e., the objects might not actually be strings)
         */
       char *nm = StringValuePtr(key);
       char *val = StringValuePtr(value);
		
       /* Add the value */
       $1 = CSLAddNameValue( $1, nm, val );
      }
    }
  }
  else {
    rb_raise(rb_eTypeError, "Argument must be dictionary or sequence of strings");
  }
}

/* char** --> Hash */
%typemap(ruby,out) char **dict
{
  /* %typemap(ruby,out) char **dict */

  /* Get a pointer to the c array */
  char **stringarray = $1;

  /* Create a new hash table, this will be returned to Ruby.  */
  $result = rb_hash_new();
  if ( stringarray != NULL ) {
    while (*stringarray != NULL ) {
      /* Get the key and value */
      char const *valptr;
      char *keyptr;
      valptr = CPLParseNameValue( *stringarray, &keyptr );

      if ( valptr != 0 ) {
        /* Convert the key and value to Ruby strings */
        VALUE nm = rb_str_new2( keyptr );
        VALUE val = rb_str_new2( valptr );
        /* Save the key, value pair to the hash table. */
        rb_hash_aset($result, nm, val);
        CPLFree( keyptr );
      }
      stringarray++;
    }
  }
}

/*
 * Typemap char **<- dict.  This typemap actually supports lists as well,
 * Then each entry in the list must be a string and have the form:
 * "name=value" so gdal can handle it.
 */
%typemap(ruby,typecheck,precedence=SWIG_TYPECHECK_POINTER) (char **dict)
{
  /* %typemap(ruby,typecheck,precedence=SWIG_TYPECHECK_POINTER) (char **dict) */
  $1 = ((TYPE($input) == T_HASH) || (TYPE($input) == T_ARRAY)) ? 1 : 0;
}


%typemap(ruby,freearg) char **dict
{
  /* %typemap(ruby,freearg) char **dict */
  CSLDestroy( $1 );
}


/* -------------   Ruby Array <-> array of char*  ------------*/


/* Typemap maps char** arguments from Ruby Array  */
%typemap(ruby,in) char **options
{
  /* %typemap(ruby,in) char **options */

  /* Check if is a list */
  Check_Type($input, T_ARRAY);

  int size = RARRAY($input)->len;
  for (int i = 0; i < size; i++) {
    VALUE item = rb_ary_entry($input, i);
    char *pszItem = StringValuePtr(item);
    $1 = CSLAddString( $1, pszItem );
  }
}

%typemap(ruby,out) char **options
{
  /* %typemap(ruby,out) char **options */

  char **stringarray = $1;
  if ( stringarray == NULL ) {
    $result = Qnil;
  }
  else {
    int len = CSLCount( stringarray );
    $result = rb_ary_new2( len );
    for ( int i = 0; i < len; ++i, ++stringarray ) {
      VALUE nm = rb_str_new2( *stringarray );
      rb_ary_push($result, nm);
    }
    CSLDestroy( $1 );
  }
}

%typemap(ruby,freearg) char **options
{
  /* %typemap(ruby,freearg) char **options */

  CSLDestroy( $1 );
}

/*
 * Typemaps map mutable char ** arguments from Ruby Strings.  Does not
 * return the modified argument
 */
%typemap(ruby,in) char ** ( char *val=0 )
{
  /* %typemap(ruby,in) char ** ( char *val=0 ) */

  val = StringValuePtr($input);
  $1 = &val;
}

%apply char** {char **ignorechange};


/* -------------  Ruby String  <- char ** no lengths ------------------*/

%typemap(ruby,in,numinputs=0) (char **argout) ( char *argout=0 )
{
  /* %typemap(ruby,in,numinputs=0) (char **argout) ( char *argout=0 ) */

  $1 = &argout;
}

%typemap(ruby,argout) (char **argout)
{
  /* %typemap(ruby,argout) (char **argout) */
	
  if ( $1 ) {
		$result = rb_str_new2( *$1 );
  }
  else {
    $result = Qnil;
  }
}

%typemap(ruby,freearg) (char **argout)
{
  /* %typemap(ruby,freearg) (char **argout) */

  if ( *$1 )
    CPLFree( *$1 );
}

/* -------------  POD Typemaps  ----------------------*/

/*
 * Typemap for an optional POD argument.
 * Declare function to take POD *.  If the parameter
 * is NULL then the function needs to define a default
 * value.
 */
/*%define OPTIONAL_POD(type,argstring)
%typemap(ruby,in) (type *optional_##type) ( type val )
{
*/
  /* %typemap(ruby,in) (type *optional_##type) */
/*  if ( $input == Qnil ) {
    $1 = 0;
  }
  else if ( PyArg_Parse( $input, #argstring ,&val ) ) {
    $1 = ($1_type) &val;
  }
  else {
    rb_raise(rb_eRuntimeError, "Invalid Parameter");
  }
}*/


/*%typemap(ruby,typecheck,precedence=0) (type *optional_##type)
{
*/
  /* %typemap(typecheck,precedence=0) (type *optionalInt) */
 /* $1 = (($input==Py_None) || my_PyCheck_##type($input)) ? 1 : 0;
}
%enddef*/


//OPTIONAL_POD(int,i);

/* --------  const char * <- Any object ------------ */

/* Formats the object using str and returns the string representation */

%typemap(ruby,in) (tostring argin) (VALUE rubyString)
{
  /* %typemap(ruby, in) (tostring argin) */

  $1 = StringValuePtr($input);
}


%typemap(ruby,typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin)
{
  /* %typemap(ruby,typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin) */
  $1 = 1;
}


/* -------------  Ruby Exception <- CPLErr  ----------------------*/

/*
 * Typemap for CPLErr.
 * This typemap will use the wrapper C-variable
 * int UseExceptions to determine proper behavour for
 * CPLErr return codes.
 * If UseExceptions ==0, then return the rc.
 * If UseExceptions ==1, then if rc >= CE_Failure, raise an exception.
 */
%typemap(ruby,out) CPLErr
{
  /* %typemap(ruby,out) CPLErr */
  if ( bUseExceptions == 1 && $1 >= CE_Failure ) {
    int errcode = CPLGetLastErrorNo();
    const char *errmsg = CPLGetLastErrorMsg();
    rb_raise(rb_eRuntimeError, "CPLErr %d: %s", errcode, (char*) errmsg );
  }

  $result = ($1_type)LONG2NUM($1);
}

/* -----------  Ruby Arrays <------> XML Trees Helper Methods --------------- */

%typemap(ruby,in) (CPLXMLNode* xmlnode )
{
  /* %typemap(ruby,in) (CPLXMLNode* xmlnode ) */
  $1 = RubyArrayToXMLTree($input);

  if ( !$1 ) {
    rb_raise(rb_eRuntimeError, "Could not convert Ruby Array to XML tree.");
  }
}

%typemap(ruby,freearg) (CPLXMLNode *xmlnode)
{
  /* %typemap(ruby,freearg) (CPLXMLNode *xmlnode) */

  if ( $1 ) {
    CPLDestroyXMLNode( $1 );
  }
}


%typemap(ruby,out,fragment="XMLTreeToPyList") (CPLXMLNode*)
{
  /* %typemap(ruby,out) (CPLXMLNode*) */

  $result = XMLTreeToRubyArray($1);
}

%typemap(ruby,ret) (CPLXMLNode*)
{
  /* %typemap(ruby,ret) (CPLXMLNode*) */
  if ( $1 ) {
    CPLDestroyXMLNode( $1 );
  }
}


%apply char* {tostring argin}
%apply int* {int* optional_int};

%typemap(ruby,in) GDALDataType, CPLErr, GDALPaletteInterp, GDALAccess, 
	GDALResampleAlg, GDALColorInterp, OGRwkbGeometryType, OGRFieldType,
	OGRJustification, OGRwkbByteOrder
{
  /* %typemap(ruby,in) CPLErr */
  $1 = ($1_type) NUM2INT($input);
}

%typemap(out) SWIGTYPE* ParentReference {
	/* %typemap(out) SWIGTYPE* ParentReference */

	/* There parent C++ object (self) owns the returned C++ object (result).
		If the parent goes out of scope it will free the child, invalidating
		the scripting language object that represents the child.  To prevent
		that create a reference from the child to the parent, thereby telling
		the garabage collector not to GC the parent.*/

	$result = SWIG_NewPointerObj((void *) $1, $1_descriptor,$owner);
	rb_iv_set($result, "swig_parent_reference", self);
}


/*%typemap(freearg) SWIGTYPE* ParentReference {
	/* %typemap(freearg) SWIGTYPE* ParentReference */

	/* Subtract 2, 1 for self and 1 since argv is 0-based */
	//rb_iv_set(argv[$argnum-2], "swig_parent_reference", self);
//}*/
