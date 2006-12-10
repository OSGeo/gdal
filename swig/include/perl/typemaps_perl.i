/*
 * $Id$
 */

/*
 * $Log$
 * Revision 1.14  2006/12/10 19:06:27  ajolma
 * IF_ERROR_RETURN_NONE, which skips returning the error code
 *
 * Revision 1.13  2006/06/07 16:06:49  ajolma
 * prefer CPLGetLastErrorMsg() in OGRErr
 *
 * Revision 1.12  2006/04/11 12:47:55  ajolma
 * removed now deprecated "perl5," from typemaps
 *
 * Revision 1.11  2005/10/11 14:11:42  kruland
 * Fix memory bug in typemap(out) char **options.  The returned array of strings
 * is owned by the dataset.
 *
 * Revision 1.10  2005/10/11 01:49:07  kruland
 * Back out previous change.  It introduced a memory leak.
 *
 * Revision 1.9  2005/10/03 20:28:51  kruland
 * Fixed bug in  %typemap(out) char **dict.
 *
 * Revision 1.8  2005/09/30 18:52:28  kruland
 * Fixed typo.
 *
 * Revision 1.7  2005/09/29 14:00:19  kruland
 * Fixed: %typemap(perl5,argout) (int *nGCPs, GDAL_GCP const **pGCPs )
 * Fixed: %typemap(perl5,in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs )
 *
 * Revision 1.6  2005/09/27 14:32:01  kruland
 * Fixed the in,numinputs=1 int nLen, char *pBuf typemap used by
 * ReadRaster & WriteRaster (thanks Ari).
 *
 * Revision 1.5  2005/09/16 20:42:49  kruland
 * Magical adjustments to some list length calls.
 *
 * Revision 1.4  2005/09/14 15:01:33  kruland
 * Removed accidental debug message.
 *
 * Revision 1.3  2005/09/13 03:02:43  kruland
 * Added OGRErr out typemap which uses ogr_error_map.i.
 *
 * Revision 1.2  2005/09/13 02:10:52  kruland
 * Added Colormap typemaps.
 *
 * Revision 1.1  2005/09/06 01:40:26  kruland
 * Perl typemaps.
 *
 */

/*
 * Copyright Ari Jolma 2005.  Based on typemaps_python.i
 * You may distribute this file under the same terms as GDAL itself.
 */

/*
 * Include the typemaps from swig library for returning of
 * standard types through arguments.
 */
%include "typemaps.i"

%apply (double *OUTPUT) { double *argout };

/*
 * double *val, int*hasval, is a special contrived typemap used for
 * the RasterBand GetNoDataValue, GetMinimum, GetMaximum, GetOffset, GetScale methods.
 * In the python bindings, the variable hasval is tested.  If it is 0 (is, the value
 * is not set in the raster band) then Py_None is returned.  If is is != 0, then
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
  /* %typemap(out) IF_ERROR_RETURN_NONE */
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
  if (! (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV))) {
    croak("argument is not an array ref");
    SWIG_fail;
  }
  $1 = argin;
  AV *av = (AV*)(SvRV($input));
  int seq_size = av_len(av)+1;
  if ( seq_size != $dim0 ) {
    croak("argument array must have length %d",$dim0);
    SWIG_fail;
  }
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
  if (! (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV))) {
    croak("argument is not an array ref");
    SWIG_fail;
  }
  AV *av = (AV*)(SvRV($input));
  $1 = av_len(av)-1;
  $2 = (int*) malloc($1*sizeof(int));
  for( int i = 0; i<$1; i++ ) {
    SV **sv = av_fetch(av, i, 0);
    $2[i] =  SvIV(*sv);
  }
}
%typemap(freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}
%fragment("CreateArrayFromIntegerArray","header") %{
static SV *
CreateArrayFromIntegerArray( double *first, unsigned int size ) {
  AV *av = (AV*)sv_2mortal((SV*)newAV());
  for( unsigned int i=0; i<size; i++ ) {
    av_store(av,i,newSViv(*first));
    ++first;
  }
  return newRV_noinc((SV*)av);
}
%}

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
  if (!SvPOK($input)) {
    croak("buf argument has to be binary data");
    SWIG_fail;
  }
  STRLEN len = SvCUR($input);
  $2 = SvPV_nolen($input);
  $1 = len;
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
  if (! (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV))) {
    croak("argument is not an array ref");
    SWIG_fail;
  }
  AV *av = (AV*)(SvRV($input));
  $1 = av_len(av)+1;
  tmpGCPList = (GDAL_GCP*) malloc($1*sizeof(GDAL_GCP));
  $2 = tmpGCPList;
  for( int i = 0; i<$1; i++ ) {
    SV **sv = av_fetch(av, i, 0);
    GDAL_GCP *item = 0;
    SWIG_ConvertPtr( *sv, (void**)&item, SWIGTYPE_p_GDAL_GCP, 0 );
    if ( ! item ) {
      SWIG_fail;
    }
    memcpy( (void*) tmpGCPList, (void*) item, sizeof( GDAL_GCP ) );
    ++tmpGCPList;
  }
}
%typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
  /* %typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
  if ($2) {
    free( (void*) $2 );
  }
}

/*
 * Typemap for GDALColorEntry* <-> AV
 * GDALColorEntry* may be a return value and both input and output param
 */
%typemap(out) GDALColorEntry*
{
  /* %typemap(out) GDALColorEntry* */
  if (result == NULL)
    croak("GetColorEntry failed at index %i",result);
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
  if (result == FALSE)
    croak("GetColorEntryAsRGB failed at index %i",result);
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
  if (! (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV))) {
    croak("argument is not an array ref");
    SWIG_fail;
  }
  AV *av = (AV*)(SvRV($input));
  int seq_size = av_len(av);
  if ( seq_size != 3 ) {
    croak("color entry argument array must have length 4 (it is %i)",seq_size+1);
    SWIG_fail;
  }
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
  if ( ! (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)) ) {
    croak("argument is not an array ref");
    SWIG_fail;
  }
  AV *av = (AV*)(SvRV($input));
  for (int i = 0; i < av_len(av)-1; i++) {
    char *pszItem = SvPV_nolen(*(av_fetch(av, i, 0)));
    $1 = CSLAddString( $1, pszItem );
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
  AV* av = (AV*)sv_2mortal((SV*)newAV());
  char **stringarray = $1;
  if ( stringarray != NULL ) {
    for ( int i = 0; i < CSLCount( stringarray ); ++i, ++stringarray ) {
      av_store(av, i, newSVpv(*stringarray, strlen(*stringarray)));
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
%typemap(in,numinputs=0) (char **argout) ( char *argout=0 )
{
  /* %typemap(in,numinputs=0) (char **argout) */
  $1 = &argout;
}
%typemap(argout) (char **argout)
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
 * This typemap will use the wrapper C-variable
 * int UseExceptions to determine proper behavour for
 * CPLErr return codes.
 * If UseExceptions ==0, then return the rc.
 * If UseExceptions ==1, then if rc >= CE_Failure, raise an exception.
 */
%typemap(out) CPLErr
{
  /* %typemap(out) CPLErr */
  $result = sv_2mortal(newSViv($1));
  argvi++;
}

/*
 * Typemap for OGRErr.
 */
%import "ogr_error_map.i"
%typemap(out,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(out) OGRErr */
  if ( result != 0 ) {
    if (CPLGetLastErrorMsg()) croak( CPLGetLastErrorMsg() ); /* this is usually better */
    croak( OGRErrMessages(result) );
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
    CPLXMLNode *psChild;
    char       *pszText = NULL;

    nChildCount = av_len(av) - 1;
    if( nChildCount < 0 )
    {
        croak("Error in input XMLTree.");
	return NULL;
    }

    nType = SvIV(*(av_fetch(av,0,0)));
    pszText = SvPV_nolen(*(av_fetch(av,1,0)));
    psThisNode = CPLCreateXMLNode( NULL, (CPLXMLNodeType) nType, pszText );

    for( iChild = 0; iChild < nChildCount; iChild++ )
    {
        psChild = AVToXMLTree( (AV *)(*(av_fetch(av,iChild+2,0))) );
        CPLAddXMLChild( psThisNode, psChild );
    }

    return psThisNode;
}
%}

%typemap(in,fragment="AVToXMLTree") (CPLXMLNode* xmlnode )
{
  /* %typemap(in) (CPLXMLNode* xmlnode ) */
  if ( ! (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)) ) {
    croak("argument is not an array ref");
    SWIG_fail;
  }
  AV *av = (AV*)(SvRV($input));
  $1 = AVToXMLTree( av );
  if ( !$1 ) SWIG_fail;
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
        av_store(av, iChild, newRV_noinc((SV*)(XMLTreeToAV( psChild ))) );
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
