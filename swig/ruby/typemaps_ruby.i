
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
 * Revision 1.2  2005/08/20 20:50:13  cfis
 * Added GetLayer method that maps to either GetLayerByName or GetLayerByIndex.  Also commented out Open and OpenShared as DataSouce class static methods.
 *
 * Revision 1.1  2005/08/09 17:40:09  kruland
 * Added support for ruby.
 *
 */


%include "typemaps.i"
%include "../ruby/renames.i"

%apply (double *OUTPUT) { double *argout };




/* -----------  Typemaps ported from typemaps_python.i.  For more information
 * refer to README.typemaps for more information about these type maps. */
 

%typemap(ruby,out) IF_FALSE_RETURN_NONE
{
  /* %typemap(ruby,out) IF_FALSE_RETURN_NONE */
  $result = Qnil;
}

%typemap(ruby,ret) IF_FALSE_RETURN_NONE
{
  /* %typemap(ruby,ret) IF_FALSE_RETURN_NONE */
/*
 *  if ($1 == 0 ) {
 *    $result = Qnil;
 *  }
 *  if ($result == 0) {
 *    $result = Qnil;
 *  }
*/
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
%typemap(ruby,in) (int nLen, char *pBuf )
{
  /* %typemap(ruby,in,numinputs=0) (int nLen, char *pBuf ) */
  $1 = ($1_ltype) StringValueLen($input);
  $2 = ($2_ltype) StringValuePtr($input);
}


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


/* -------------   Ruby Array <-> array of char*  ----------------------

/*
 * Typemap maps char** arguments from Ruby Array
 */
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


/* -------------  Ruby String  <- char ** no lengths ----------------------*/

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


/* -------------  Ruby Exception <- CPLErr  ----------------------*/

/*
 * Typemap for CPLErr.
 * This typemap will use the wrapper C-variable
 * int UseExceptions to determine proper behavour for
 * CPLErr return codes.
 * If UseExceptions ==0, then return the rc.
 * If UseExceptions ==1, then if rc >= CE_Failure, raise an exception.
 */
%typemap(ruby,arginit) CPLErr
{
  /* %typemap(ruby,arginit) CPLErr */
  $1 = ($1_type) 0;
}

%typemap(ruby,out) CPLErr
{
  /* %typemap(ruby,out) CPLErr */
  if ( bUseExceptions == 1 && $1 >= CE_Failure ) {
    int errcode = CPLGetLastErrorNo();
    const char *errmsg = CPLGetLastErrorMsg();
    rb_raise(rb_eRuntimeError, "CPLErr %d: %s", errcode, (char*) errmsg );
  }
}

%typemap(ruby,ret) CPLErr
{
  /* %typemap(ruby,ret) CPLErr */
  if ( bUseExceptions == 0 ) {
    /* We're not using exceptions.  The test in the out typemap means that
       we know we have a valid return value.  Test if there are any return
       values set by argout typemaps.
    */
  /*  if ( $result == 0 ) { */
     /* No other return values set so return None */
  /*    $resul = ($1_type)LONG2NUM($1); */
   /* } */
  }
}


%apply char* {tostring argin}
%apply int* {int* optional_int};

%apply double * {double_2 argout,
				 double_3 argout, double_3 argin, 
				 double_4 argout,
				 double_6 argout, double_6 argin,
				 double_7 argout,
	             dozuble_15 *argout, double_15 argin,
	             double_17 *argout, double_17 argin};

 
%typemap(ruby,in) GDALDataType, CPLErr, GDALPaletteInterp, GDALAccess, 
	GDALResampleAlg, GDALColorInterp, OGRwkbGeometryType, OGRFieldType,
	OGRJustification, OGRwkbByteOrder
{
  /* %typemap(ruby,in) CPLErr */
  $1 = ($1_type) NUM2INT($input);
}


//**************   Ruby specific type maps ***************
/*%extend OGRDriverShadow {
	static OGRDriverShadow* GetDriverByName( char const *name ) {
  	return (OGRDriverShadow*) OGRGetDriverByName( name );
	}
  
	static OGRDriverShadow* GetDriver(int driver_number) {
  	return (OGRDriverShadow*) OGRGetDriver(driver_number);
	}  
}
*/

/* Replace GetLayerByIndex and GetLayerByName by GetLayer */
%ignore OGRDataSourceShadow::GetLayerByIndex;
%ignore OGRDataSourceShadow::GetLayerByName;

%extend OGRDataSourceShadow {
	OGRLayerShadow *GetLayer(VALUE object) {
		// get field index
		switch (TYPE(object)) {
			case T_STRING: {
				char* name = StringValuePtr(object);
				return OGR_DS_GetLayerByName(self, name);
				break;
			}
			case T_FIXNUM: {
				int index = NUM2INT(object);
				return OGR_DS_GetLayer(self, index);
				break;
			}
			default:
				SWIG_exception(SWIG_TypeError, "Value must be a string or integer.");
		}
	}

	/*%newobject Open;
	static OGRDataSourceShadow *Open( const char * filename, int update=0 ) {
	    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpen(filename,update, NULL);
	    return ds;
	  }
	
	%newobject OpenShared;
	static OGRDataSourceShadow *OpenShared( const char * filename, int update=0 ) {
	    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpenShared(filename,update, NULL);
	    return ds;
	}*/
}

// Extend the layers class by adding the method each to support
// the ruby enumerator mixin.
%mixin OGRLayerShadow "Enumerable";
%extend OGRLayerShadow {
  void each() {
		OGRFeatureShadow* feature = NULL;
	 	while (feature = (OGRFeatureShadow*) OGR_L_GetNextFeature(self))
	 	{
			rb_yield(SWIG_NewPointerObj((void *) feature, $descriptor(OGRFeatureShadow *), 0));
    }
	}
}

%extend OGRFeatureShadow {
	VALUE GetField(VALUE object) {
		VALUE result;

		int index;

		// get field index
		switch (TYPE(object)) {
			case T_STRING:
				index = OGR_F_GetFieldIndex(self, StringValuePtr(object));
				break;
			case T_FIXNUM:
				index = NUM2INT(object);
				break;
			default:
				SWIG_exception(SWIG_TypeError, "Value must be a string or integer.");
		}
		
		int count = OGR_F_GetFieldCount(self);
		
		if (index < 0 || index > count) {
			SWIG_exception(SWIG_IndexError, "Illegal field requested.");
		}

		// is the field unset?
	  if (!OGR_F_IsFieldSet(self, index)) {
	  	result = Qnil;
	  	return result;
	  }
	  
	  // get field type
    OGRFieldType field_type = (OGRFieldType) OGR_Fld_GetType(OGR_F_GetFieldDefnRef( self, index));

		switch (field_type) {
			case OFTInteger: {
				const int value = OGR_F_GetFieldAsInteger(self, index);
				result = INT2NUM(value);
				break;
			}

			case OFTIntegerList: {
				int len = 0;
				const int* list = OGR_F_GetFieldAsIntegerList(self, index, &len);
				
				result = rb_ary_new2(len);
				
		    for ( int i = 0; i < len; ++i, ++list ) {
					VALUE item = INT2NUM(*list);
		      rb_ary_store(result, item, i);
		    }
		    break;
			}

			case OFTReal: {
				const double value = OGR_F_GetFieldAsDouble(self, index);
				return rb_float_new(value);
				break;
			}

			case OFTRealList: {
				int len = 0;
				const double* list = OGR_F_GetFieldAsDoubleList(self, index, &len);
				
				result = rb_ary_new2(len);
				
		    for ( int i = 0; i < len; ++i, ++list ) {
					VALUE item = rb_float_new(*list);
		      rb_ary_store(result, item, i);
		    }
		    break;
			}

			case OFTString: {
				const char* value = (const char *) OGR_F_GetFieldAsString(self, index);
				return rb_str_new2(value);
				break;
			}

			case OFTStringList:
/*				int len3 = 0;
				const char** string_list = OGR_F_GetFieldAsStringList(self, index, &len);
				
				result = rb_ary_new2(len3);
				
		    for ( int i = 0; i < len; ++i, ++string_list ) {
					VALUE item = rb_str_new2(*string_list);
		      rb_ary_store(result, item, i);
		    }*/
		    result = Qnil;
		    break;
			default:
				SWIG_exception(SWIG_TypeError, "Unsupported field type.");
		}
	
		return result;		
	}        
}
