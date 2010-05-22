/*
 * Perl typemaps for GDAL SWIG bindings
 * Copyright Ari Jolma 2005.  Based on typemaps_python.i
 * You may distribute this file under the same terms as GDAL itself.
 */

/*
 * Include the typemaps from swig library for returning of
 * standard types through arguments.
 */
%include "typemaps.i"

%apply (long *OUTPUT) { long *argout };
%apply (double *OUTPUT) { double *argout };
%apply (double *OUTPUT) { double *defaultval };

/*
 * double *val, int*hasval, is a special contrived typemap used for
 * the RasterBand GetNoDataValue, GetMinimum, GetMaximum, GetOffset, GetScale methods.
 * the variable hasval is tested.  If it is false (is, the value
 * is not set in the raster band) then undef is returned.  If is is != 0, then
 * the value is coerced into a long and returned.
 */
%typemap(in,numinputs=0) (double *val, int *hasval) ( double tmpval, int tmphasval ) {
  /* %typemap(in,numinputs=0) (double *val, int *hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
}
%typemap(argout) (double *val, int *hasval) {
  /* %typemap(argout) (double *val, int *hasval) */
  $result = sv_newmortal();
  if ( *$2 )
    sv_setnv($result, *$1);
  argvi++;
}

%typemap(out) GIntBig
{
  /* %typemap(out) GIntBig */
  $result = sv_newmortal();
  sv_setiv($result, (IV) $1);
  argvi++;
}
%typemap(out) (char **CSL)
{
    /* %typemap(out) char **CSL */
    AV *av = (AV*)sv_2mortal((SV*)newAV());
    if ($1) {
	int i;
	for (i = 0; $1[i]; i++) {
	    SV *s = newSVpv($1[i], 0);
	    if (!av_store(av, i, s))
		SvREFCNT_dec(s);
	}
	CSLDestroy($1);
    }
    $result = newRV_noinc((SV*)av);
    argvi++;
}
%typemap(out) (char **free)
{
  /* %typemap(out) char **free */
  AV *av = (AV*)sv_2mortal((SV*)newAV());
  if ($1) {
    int i;
    for (i = 0; $1[i]; i++) {
      av_store(av, i, newSVpv($1[0], 0));
    }
    CPLFree($1);
  }
  $result = newRV_noinc((SV*)av);
  argvi++;
}
/* if the call to the fct failed, return an undef */
%typemap(out) IF_FALSE_RETURN_NONE
{
  /* %typemap(out) IF_FALSE_RETURN_NONE */
}
%typemap(ret) IF_FALSE_RETURN_NONE
{
 /* %typemap(ret) IF_FALSE_RETURN_NONE */
  if ($1 == 0 ) {
    /* this is currently used only in GDALGCPsToGeoTransform
       this is probably a memory leak
       ST(argvi-1) is at this point an array which needs to be destr
     */
    ST(argvi-1) = sv_newmortal();
  }
}
%typemap(out) IF_ERROR_RETURN_NONE
{
  /* %typemap(out) IF_ERROR_RETURN_NONE (do not return the error code) */
}

/*
 * SWIG macro to define fixed length array typemaps
 * defines three different typemaps.
 *
 * 1) For argument in.  The wrapped function's prototype is:
 *
 *    FunctionOfDouble3( double *vector );
 *
 *    The function assumes that vector points to three consecutive doubles.
 *    This can be wrapped using:
 * 
 *    %apply (double_3 argin) { (double *vector) };
 *    FunctionOfDouble3( double *vector );
 *    %clear (double *vector);
 *
 *    Example:  Dataset.SetGeoTransform().
 *
 * 2) Functions which modify a fixed length array passed as
 *    an argument or return data in an array allocated by the
 *    caller.
 *
 *    %apply (double_6 argout ) { (double *vector) };
 *    GetVector6( double *vector );
 *    %clear ( double *vector );
 *
 *    Example:  Dataset.GetGeoTransform().
 *
 * 3) Functions which take a double **.  Through this argument it
 *    returns a pointer to a fixed size array allocated with CPLMalloc.
 *
 *    %apply (double_17 *argoug) { (double **vector) };
 *    ReturnVector17( double **vector );
 *    %clear ( double **vector );
 *   
 *    Example:  SpatialReference.ExportToPCI().
 *
 */

%fragment("CreateArrayFromIntArray","header") %{
static SV *
CreateArrayFromIntArray( int *first, unsigned int size ) {
  AV *av = (AV*)sv_2mortal((SV*)newAV());
  for( unsigned int i=0; i<size; i++ ) {
    av_store(av,i,newSViv(*first));
    ++first;
  }
  return newRV_noinc((SV*)av);
}
%}

%fragment("CreateArrayFromDoubleArray","header") %{
static SV *
CreateArrayFromDoubleArray( double *first, unsigned int size ) {
  AV *av = (AV*)sv_2mortal((SV*)newAV());
  for( unsigned int i=0; i<size; i++ ) {
    av_store(av,i,newSVnv(*first));
    ++first;
  }
  return newRV_noinc((SV*)av);
}
%}

%fragment("CreateArrayFromStringArray","header") %{
static SV *
CreateArrayFromStringArray( char **first ) {
  AV *av = (AV*)sv_2mortal((SV*)newAV());
  for( unsigned int i = 0; *first != NULL; i++ ) {
    av_store(av,i,newSVpv(*first, strlen(*first)));
    ++first;
  }
  return newRV_noinc((SV*)av);
}
%}

%typemap(in,numinputs=0) (int *nLen, const int **pList) (int nLen, int *pList)
{
  /* %typemap(in,numinputs=0) (int *nLen, const int **pList) */
  $1 = &nLen;
  $2 = &pList;
}
%typemap(argout,fragment="CreateArrayFromIntArray") (int *nLen, const int **pList)
{
  /* %typemap(argout) (int *nLen, const int **pList) */
  $result = CreateArrayFromIntArray( *($2), *($1) );
  argvi++;
}

%typemap(in,numinputs=1) (int len, int *output)
{
  /* %typemap(in,numinputs=1) (int len, int *output) */
  $1 = SvIV($input);
}
%typemap(check) (int len, int *output)
{
  /* %typemap(check) (int len, int *output) */
  if ($1 < 1) $1 = 1; /* stop idiocy */
  $2 = (int *)CPLMalloc( $1 * sizeof(int) );
    
}
%typemap(argout,fragment="CreateArrayFromIntArray") (int len, int *output)
{
  /* %typemap(argout) (int len, int *output) */
  $result = CreateArrayFromIntArray( $2, $1 );
  argvi++;
}
%typemap(freearg) (int len, int *output)
{
  /* %typemap(freearg) (int len, int *output) */
  CPLFree($2);
}

%typemap(in,numinputs=0) (int *nLen, const double **pList) (int nLen, double *pList)
{
  /* %typemap(in,numinputs=0) (int *nLen, const double **pList) */
  $1 = &nLen;
  $2 = &pList;
}
%typemap(argout,fragment="CreateArrayFromDoubleArray") (int *nLen, const double **pList)
{
  /* %typemap(argout) (int *nLen, const double **pList) */
  $result = CreateArrayFromDoubleArray( *($2), *($1) );
  argvi++;
}

%typemap(in,numinputs=0) (char ***pList) (char **pList)
{
  /* %typemap(in,numinputs=0) (char ***pList) */
  $1 = &pList;
}
%typemap(argout,fragment="CreateArrayFromStringArray") (char ***pList)
{
  /* %typemap(argout) (char ***pList) */
  $result = CreateArrayFromStringArray( *($1) );
  argvi++;
}

%typemap(in,numinputs=0) ( double argout[ANY]) (double argout[$dim0])
{
  /* %typemap(in,numinputs=0) (double argout[ANY]) */
  $1 = argout;
}
%typemap(argout,fragment="CreateArrayFromDoubleArray") ( double argout[ANY])
{
  /* %typemap(argout) (double argout[ANY]) */
  $result = CreateArrayFromDoubleArray( $1, $dim0 );
  argvi++;
}

%typemap(in,numinputs=0) ( double *argout[ANY]) (double *argout)
{
  /* %typemap(in,numinputs=0) (double *argout[ANY]) */
  $1 = &argout;
}
%typemap(argout,fragment="CreateArrayFromDoubleArray") ( double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */
  $result = CreateArrayFromDoubleArray( *$1, $dim0 );
  argvi++;
}
%typemap(freearg) (double *argout[ANY])
{
  /* %typemap(freearg) (double *argout[ANY]) */
  CPLFree(*$1);
}
%typemap(in) (double argin[ANY]) (double argin[$dim0])
{
    /* %typemap(in) (double argin[ANY]) */
    if (!(SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    $1 = argin;
    AV *av = (AV*)(SvRV($input));
    for (unsigned int i=0; i<$dim0; i++) {
	SV **sv = av_fetch(av, i, 0);
	$1[i] =  SvNV(*sv);
    }
}

/*
 *  Typemap for counted arrays of ints <- PySequence
 */
%typemap(in,numinputs=1) (int nList, int* pList)
{
    /* %typemap(in,numinputs=1) (int nList, int* pList) */
    if (!(SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    AV *av = (AV*)(SvRV($input));
    $1 = av_len(av)+1;
    $2 = (int*) malloc($1*sizeof(int));
    for( int i = 0; i<$1; i++ ) {
	SV **sv = av_fetch(av, i, 0);
	$2[i] =  SvIV(*sv);
    }
}
%typemap(freearg) (int nList, int* pList)
{
    /* %typemap(freearg) (int nList, int* pList) */
    if ($2)
	free((void*) $2);
}

%typemap(in,numinputs=1) (int nList, double* pList)
{
    /* %typemap(in,numinputs=1) (int nList, double* pList) */
    if (!(SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    AV *av = (AV*)(SvRV($input));
    $1 = av_len(av)+1;
    $2 = (double*) malloc($1*sizeof(double));
    for( int i = 0; i<$1; i++ ) {
	SV **sv = av_fetch(av, i, 0);
	$2[i] =  SvNV(*sv);
    }
}
%typemap(freearg) (int nList, double* pList)
{
    /* %typemap(freearg) (int nList, double* pList) */
    if ($2)
	free((void*) $2);
}

%typemap(in) (char **pList)
{
    /* %typemap(in) (char **pList) */
    if (!(SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    AV *av = (AV*)(SvRV($input));
    for (int i = 0; i < av_len(av)+1; i++) {
	char *pszItem = SvPV_nolen(*(av_fetch(av, i, 0)));
	$1 = CSLAddString( $1, pszItem );
    }
}
%typemap(freearg) (char **pList)
{
    /* %typemap(freearg) (char **pList) */
    if ($1)
	CSLDestroy( $1 );
}

%typemap(in,numinputs=1) (int defined, double value)
{
    /* %typemap(in,numinputs=1) (int defined, double value) */
    $1 = SvOK($input);
    $2 = SvNV($input);
}

/*
 * Typemap for buffers with length <-> AV
 * Used in Band::ReadRaster() and Band::WriteRaster()
 *
 * This typemap has a typecheck also since the WriteRaster()
 * methods are overloaded.
 */
%typemap(in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen = 0, char *pBuf = 0 )
{
  /* %typemap(in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}
%typemap(argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */
  $result = sv_2mortal(newSVpv( *$2, *$1 ));
  argvi++;
}
%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( *$1 ) {
    free( *$2 );
  }
}
%typemap(in,numinputs=1) (int nLen, char *pBuf )
{
    /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
    if (SvOK($input)) {
	if (!SvPOK($input))
	    SWIG_croak("expected binary data as input");
	STRLEN len = SvCUR($input);
	$2 = SvPV_nolen($input);
	$1 = len;
    } else {
	$2 = NULL;
	$1 = 0;
    }
}

/***************************************************
 * Typemaps for  (retStringAndCPLFree*)
 ***************************************************/

%typemap(out) (retStringAndCPLFree*)
%{ 
    /* %typemap(out) (retStringAndCPLFree*) */
    if($1)
    {
        $result = SWIG_FromCharPtr((const char *)result);
        CPLFree($1);
    }
    else
    {
        $result = NULL;
    }
    argvi++ ;
%}

/* slightly different version(?) for GDALAsyncReader */
%typemap(in,numinputs=0) (int *nLength, char **pBuffer ) ( int nLength = 0, char *pBuffer = 0 )
{
  /* %typemap(in,numinputs=0) (int *nLength, char **pBuffer ) */
  $1 = &nLength;
  $2 = &pBuffer;
}
%typemap(freearg) (int *nLength, char **pBuffer )
{
  /* %typemap(freearg) (int *nLength, char **pBuffer ) */
  if( *$1 ) {
    free( *$2 );
  }
}


/*
 * Typemap argout of GDAL_GCP* used in Dataset::GetGCPs( )
 */
%typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs=0, GDAL_GCP *pGCPs=0 )
{
  /* %typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  $1 = &nGCPs;
  $2 = &pGCPs;
}
%typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs )
{
  /* %typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  AV *dict = (AV*)sv_2mortal((SV*)newAV());
  for( int i = 0; i < *$1; i++ ) {
    GDAL_GCP *o = new_GDAL_GCP( (*$2)[i].dfGCPX,
                                (*$2)[i].dfGCPY,
                                (*$2)[i].dfGCPZ,
                                (*$2)[i].dfGCPPixel,
                                (*$2)[i].dfGCPLine,
                                (*$2)[i].pszInfo,
                                (*$2)[i].pszId );
    SV *sv = newSV(0);
    SWIG_MakePtr( sv, (void*)o, $*2_descriptor, SWIG_SHADOW|SWIG_OWNER);
    av_store(dict, i, sv);
  }
  $result = newRV_noinc((SV*)dict);
  argvi++;
}
%typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) ( GDAL_GCP *tmpGCPList )
{
    /* %typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) */
    if (!(SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    AV *av = (AV*)(SvRV($input));
    $1 = av_len(av)+1;
    tmpGCPList = (GDAL_GCP*) malloc($1*sizeof(GDAL_GCP));
    $2 = tmpGCPList;
    for( int i = 0; i<$1; i++ ) {
	SV **sv = av_fetch(av, i, 0);
	GDAL_GCP *item = 0;
	SWIG_ConvertPtr( *sv, (void**)&item, SWIGTYPE_p_GDAL_GCP, 0 );
	if (!item )
	    SWIG_fail;
	memcpy( (void*) tmpGCPList, (void*) item, sizeof( GDAL_GCP ) );
	++tmpGCPList;
    }
}
%typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
    /* %typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
    if ($2)
	free($2);
}

/*
 * Typemap for GDALColorEntry* <-> AV
 * GDALColorEntry* may be a return value and both input and output param
 */
%typemap(out) GDALColorEntry*
{
    /* %typemap(out) GDALColorEntry* */
    if (!result)
	SWIG_croak("GetColorEntry failed");
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) result->c1);
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) result->c2);
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) result->c3);
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) result->c4);
}
%typemap(in,numinputs=0) GDALColorEntry*(GDALColorEntry e)
{
  /* %typemap(in,numinputs=0) GDALColorEntry*(GDALColorEntry e) */
  $1 = &e;
}
%typemap(argout) GDALColorEntry*
{
    /* %typemap(argout) GDALColorEntry* */
    if (!result)
	SWIG_croak("GetColorEntryAsRGB failed");
    argvi--;
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) e3.c1);
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) e3.c2);
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) e3.c3);
    $result = sv_newmortal();
    sv_setiv(ST(argvi++), (IV) e3.c4);
}
%typemap(argout) const GDALColorEntry*
{
  /* %typemap(argout) const GDALColorEntry* */
}
%typemap(in,numinputs=1) const GDALColorEntry*(GDALColorEntry e)
{
    /* %typemap(in,numinputs=1) const GDALColorEntry*(GDALColorEntry e) */
    $1 = &e3;
    if (!(SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    AV *av = (AV*)(SvRV($input));
    SV **sv = av_fetch(av, 0, 0);
    $1->c1 =  SvIV(*sv);
    sv = av_fetch(av, 1, 0);
    $1->c2 =  SvIV(*sv);
    sv = av_fetch(av, 2, 0);
    $1->c3 =  SvIV(*sv);
    sv = av_fetch(av, 3, 0);
    $1->c4 =  SvIV(*sv);
}

/*
 * Typemap char ** <-> HV *
 */
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (char **dict)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (char **dict) */
  $1 = (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVHV)) ? 1 : 0;
}
%typemap(in) char **dict
{
  /* %typemap(in) char **dict */
  HV *hv = (HV*)SvRV($input);
  SV *sv;
  char *key;
  I32 klen;
  $1 = NULL;
  hv_iterinit(hv);
  while(sv = hv_iternextsv(hv,&key,&klen)) {
    $1 = CSLAddNameValue( $1, key, SvPV_nolen(sv) );
  }
}
%typemap(out) char **dict
{
  /* %typemap(out) char **dict */
  char **stringarray = $1;
  HV *hv = (HV*)sv_2mortal((SV*)newHV());
  if ( stringarray != NULL ) {
    while (*stringarray != NULL ) {
      char const *valptr;
      char *keyptr;
      valptr = CPLParseNameValue( *stringarray, &keyptr );
      if ( valptr != 0 ) {
        hv_store(hv, keyptr, strlen(keyptr), newSVpv(valptr, strlen(valptr)), 0);
        CPLFree( keyptr );
      }
      stringarray++;
    }
  }
  $result = newRV_noinc((SV*)hv);
  argvi++;
}
%typemap(freearg) char **dict
{
  /* %typemap(freearg) char **dict */
  CSLDestroy( $1 );
}

/*
 * Typemap char **options <-> AV
 */
%typemap(in) char **options
{
    /* %typemap(in) char **options */
    if (SvOK($input)) {
        if (SvROK($input)) {
	    if (SvTYPE(SvRV($input))==SVt_PVAV) {
		AV *av = (AV*)(SvRV($input));
		for (int i = 0; i < av_len(av)+1; i++) {
		    char *pszItem = SvPV_nolen(*(av_fetch(av, i, 0)));
		    $1 = CSLAddString( $1, pszItem );
		}
	    } else if (SvTYPE(SvRV($input))==SVt_PVHV) {
		HV *hv = (HV*)SvRV($input);
		SV *sv;
		char *key;
		I32 klen;
		$1 = NULL;
		hv_iterinit(hv);
		while(sv = hv_iternextsv(hv,&key,&klen)) {
                    $1 = CSLAddNameValue( $1, key, SvPV_nolen(sv) );
		}
	    } else
		SWIG_croak("'options' is not a reference to an array or hash");
	} else
	    SWIG_croak("'options' is not a reference");   
    }
}
%typemap(freearg) char **options
{
  /* %typemap(freearg) char **options */
  if ($1) CSLDestroy( $1 );
}
%typemap(out) char **options
{
    /* %typemap(out) char **options -> ( string ) */
    AV* av = (AV*)sv_2mortal((SV*)newAV());
    char **stringarray = $1;
    if ( stringarray != NULL ) {
	int n = CSLCount( stringarray );
	for ( int i = 0; i < n; i++ ) {
	    SV *s = newSVpv(stringarray[i], strlen(*stringarray));
	    if (!av_store(av, i, s))
		SvREFCNT_dec(s);
	}
    }
    $result = newRV_noinc((SV*)av);
    argvi++;
}

/*
 * Typemaps map mutable char ** arguments from AV.  Does not
 * return the modified argument
 */
%typemap(in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
  val = SvPV_nolen($input);
  $1 = &val;
}

/*
 * Typemap for char **argout.
 */
%typemap(in,numinputs=0) (char **argout) (char *argout=0), (char **username) (char *argout=0), (char **usrname) (char *argout=0), (char **type) (char *argout=0)
{
  /* %typemap(in,numinputs=0) (char **argout) */
  $1 = &argout;
}
%typemap(argout) (char **argout), (char **username), (char **usrname), (char **type)
{
  /* %typemap(argout) (char **argout) */
  $result = sv_newmortal();
  if ( $1 )
    sv_setpv($result, *$1);
  argvi++;
}
%typemap(freearg) (char **argout)
{
  /* %typemap(freearg) (char **argout) */
  if ( *$1 )
    CPLFree( *$1 );
}

/*
 * Typemap for an optional POD argument.
 * Declare function to take POD *.  If the parameter
 * is NULL then the function needs to define a default
 * value.
 */
%typemap(in) (int *optional_int) ( int val )
{
  /* %typemap(in) (int *optional_int) */
  if ( !SvOK($input) ) {
    $1 = 0;
  }
  else {
    val = SvIV($input);
    $1 = ($1_type)&val;
  }
}

/*
 * Typedef const char * <- Any object.
 *
 * Formats the object using str and returns the string representation
 */

%typemap(in) (tostring argin)
{
  /* %typemap(in) (tostring argin) */
  $1 = SvPV_nolen( $input ); 
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin)
{
  /* %typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin) */
  $1 = 1;
}

/*
 * Typemap for CPLErr.
 * The assumption is that all errors have been reported through CPLError
 * and are thus caught by call to CPLGetLastErrorType() in %exception
 */
%typemap(out) CPLErr
{
  /* %typemap(out) CPLErr */
}

/*
 * Typemap for OGRErr.
 * _Some_ errors in OGR are not reported through CPLError and the return
 * value of the function must be examined and the message obtained from
 * OGRErrMessages, which is a function within these wrappers.
 */
%import "ogr_error_map.i"
%typemap(out,fragment="OGRErrMessages") OGRErr
{
    /* %typemap(out) OGRErr */
    if ( result != 0 ) {
	const char *err = CPLGetLastErrorMsg();
	if (err and *err) SWIG_croak(err); /* this is usually better */
	SWIG_croak( OGRErrMessages(result) );
    }
}

/*
 * Typemaps for minixml:  CPLXMLNode* input, CPLXMLNode *ret
 */

%fragment("AVToXMLTree","header") %{
/************************************************************************/
/*                          AVToXMLTree()                               */
/************************************************************************/
static CPLXMLNode *AVToXMLTree( AV *av )
{
    int      nChildCount = 0, iChild, nType;
    CPLXMLNode *psThisNode;
    char       *pszText = NULL;
    
    nChildCount = av_len(av) - 1; /* there are two non-childs in the array */
    if (nChildCount < 0)
        /* the input XML is empty */
	return NULL;

    nType = SvIV(*(av_fetch(av,0,0)));
    pszText = SvPV_nolen(*(av_fetch(av,1,0)));
    psThisNode = CPLCreateXMLNode( NULL, (CPLXMLNodeType) nType, pszText );
    
    for( iChild = 0; iChild < nChildCount; iChild++ )
    {
	SV **s = av_fetch(av, iChild+2, 0);
	CPLXMLNode *psChild;
	if (!(SvROK(*s) && (SvTYPE(SvRV(*s))==SVt_PVAV)))
	    /* expected a reference to an array */
	    psChild = NULL;
	else
	    psChild = AVToXMLTree((AV*)SvRV(*s));
	if (psChild)
	    CPLAddXMLChild( psThisNode, psChild );
	else {
	    CPLDestroyXMLNode(psThisNode);
	    return NULL;
	}
    }

    return psThisNode;
}
%}

%typemap(in,fragment="AVToXMLTree") (CPLXMLNode* xmlnode )
{
    /* %typemap(in) (CPLXMLNode* xmlnode ) */
    if (!(SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    AV *av = (AV*)(SvRV($input));
    $1 = AVToXMLTree( av );
    if ( !$1 ) SWIG_croak("Conversion Perl array to XMLTree failed");
}
%typemap(freearg) (CPLXMLNode *xmlnode)
{
    /* %typemap(freearg) (CPLXMLNode *xmlnode) */
    if ( $1 ) CPLDestroyXMLNode( $1 );
}

%fragment("XMLTreeToAV","header") %{
/************************************************************************/
/*                          XMLTreeToAV()                               */
/************************************************************************/
static AV *XMLTreeToAV( CPLXMLNode *psTree )
{
    AV *av;
    int      nChildCount = 0, iChild;
    CPLXMLNode *psChild;

    for( psChild = psTree->psChild; 
         psChild != NULL; 
         psChild = psChild->psNext )
        nChildCount++;

    av = (AV*)sv_2mortal((SV*)newAV());

    av_store(av,0,newSViv((int) psTree->eType));
    av_store(av,1,newSVpv(psTree->pszValue, strlen(psTree->pszValue)));

    for( psChild = psTree->psChild, iChild = 2; 
         psChild != NULL; 
         psChild = psChild->psNext, iChild++ )
    {
	SV *s = newRV_inc((SV*)XMLTreeToAV(psChild));
	if (!av_store(av, iChild, s))
	    SvREFCNT_dec(s);
    }

    return av;
}
%}

%typemap(out,fragment="XMLTreeToAV") (CPLXMLNode*)
{
  /* %typemap(out) (CPLXMLNode*) */
  $result = newRV_noinc((SV*)XMLTreeToAV( $1 ));
  argvi++;
}
%typemap(ret) (CPLXMLNode*)
{
  /* %typemap(ret) (CPLXMLNode*) */
  if ( $1 ) CPLDestroyXMLNode( $1 );
}

/* non NULL input pointer checks */

%define CHECK_NOT_UNDEF(type, param, msg)
%typemap(check) (type *param)
{
    /* %typemap(check) (type *param) */
    if (!$1)
	SWIG_croak("The msg must not be undefined");
}
%enddef

CHECK_NOT_UNDEF(char, method, method)
CHECK_NOT_UNDEF(const char, name, name)
CHECK_NOT_UNDEF(const char, request, request)
CHECK_NOT_UNDEF(const char, cap, capability)
CHECK_NOT_UNDEF(const char, statement, statement)
CHECK_NOT_UNDEF(const char, pszNewDesc, description)
CHECK_NOT_UNDEF(OSRCoordinateTransformationShadow, , coordinate transformation)
CHECK_NOT_UNDEF(OGRGeometryShadow, other, other geometry)
CHECK_NOT_UNDEF(OGRGeometryShadow, other_disown, other geometry)
CHECK_NOT_UNDEF(OGRGeometryShadow, geom, geometry)
CHECK_NOT_UNDEF(OGRFieldDefnShadow, defn, field definition)
CHECK_NOT_UNDEF(OGRFieldDefnShadow, field_defn, field definition)
CHECK_NOT_UNDEF(OGRFeatureShadow, feature, feature)

%typemap(in, numinputs=1) (int nCount, double *x, double *y, double *z)
{
    /* %typemap(in) (int nCount, double *x, double *y, double *z) */
    /* $input is a ref to a list of refs to point lists */
    if (! (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)))
	SWIG_croak("expected a reference to an array");
    AV *av = (AV*)(SvRV($input));
    $1 = av_len(av)+1;
    $2 = (double*) malloc($1*sizeof(double));
    $3 = (double*) malloc($1*sizeof(double));
    $4 = (double*) malloc($1*sizeof(double));
    if (!$2 or !$3 or !$4)
	SWIG_croak("out of memory");
    for (int i = 0; i < $1; i++) {
	SV **sv = av_fetch(av, i, 0); /* ref to one point list */
	if (!(SvROK(*sv) && (SvTYPE(SvRV(*sv))==SVt_PVAV)))
	    SWIG_croak("expected a reference to a list of coordinates");
	AV *ac = (AV*)(SvRV(*sv));
	int n = av_len(ac)+1;
	SV **c = av_fetch(ac, 0, 0);
	$2[i] = SvNV(*c);
	c = av_fetch(ac, 1, 0);
	$3[i] = SvNV(*c);
	if (n < 3) {
	    $4[i] = 0;
	} else {
	    c = av_fetch(ac, 2, 0);
	    $4[i] = SvNV(*c);
	}
    }
}

%typemap(argout) (int nCount, double *x, double *y, double *z)
{
    /* %typemap(argout) (int nCount, double *x, double *y, double *z) */
    AV *av = (AV*)(SvRV($input));
    for (int i = 0; i < $1; i++) {
	SV **sv = av_fetch(av, i, 0);
	AV *ac = (AV*)(SvRV(*sv));
	int n = av_len(ac)+1;
	SV *c = newSVnv($2[i]);
	if (!av_store(ac, 0, c))
	    SvREFCNT_dec(c);
	c = newSVnv($3[i]);
	if (!av_store(ac, 1, c))
	    SvREFCNT_dec(c);
	c = newSVnv($4[i]);
	if (!av_store(ac, 2, c))
	    SvREFCNT_dec(c);
    }
}

%typemap(freearg) (int nCount, double *x, double *y, double *z)
{
    /* %typemap(freearg) (int nCount, double *x, double *y, double *z) */
    if ($2) free($2);
    if ($3) free($3);
    if ($4) free($4);
}

%typemap(arginit, noblock=1) ( void* callback_data = NULL)
{
    SavedEnv saved_env;
    saved_env.fct = NULL;
    saved_env.data = NULL;
}

%typemap(in) (GDALProgressFunc callback = NULL)
{
    /* %typemap(in) (GDALProgressFunc callback = NULL) */
    if (SvOK($input)) {
        if (SvROK($input)) {
	    if (SvTYPE(SvRV($input)) != SVt_PVCV) {
	       SWIG_croak("the callback arg must be a reference to a subroutine\n");
	    } else {
	       saved_env.fct = (SV *)$input;
	       $1 = &callback_d_cp_vp;
           }
        } else {
            SWIG_croak("the callback arg must be a reference to a subroutine\n");
	}
    }
}

%typemap(in) (void* callback_data = NULL)
{
    /* %typemap(in) (void* callback_data=NULL) */
    if (SvOK($input))
	saved_env.data = (SV *)$input;
    if (saved_env.fct)
	$1 = (void *)(&saved_env); /* the Perl layer must make sure that this parameter is always given */
}
