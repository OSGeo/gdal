/*
 * $Id$
 */

/*
 * $Log$
 * Revision 1.2  2006/09/07 10:26:31  tamas
 * Added default exception support
 *
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */
 
%include cpl_exceptions.i

// When we switch to swig 1.3.26 these definitions can be removed

%{
GDALDataType GDALRasterBandShadow_get_DataType( GDALRasterBandShadow *h ) {
  return GDALGetRasterDataType( h );
}
int GDALRasterBandShadow_get_XSize( GDALRasterBandShadow *h ) {
  return GDALGetRasterBandXSize( h );
}
int GDALRasterBandShadow_get_YSize( GDALRasterBandShadow *h ) {
  return GDALGetRasterBandYSize( h );
}
int GDALDatasetShadow_get_RasterXSize( GDALDatasetShadow *h ) {
  return GDALGetRasterXSize( h );
}
int GDALDatasetShadow_get_RasterYSize( GDALDatasetShadow *h ) {
  return GDALGetRasterYSize( h );
}
int GDALDatasetShadow_get_RasterCount( GDALDatasetShadow *h ) {
  return GDALGetRasterCount( h );
}
char const *GDALDriverShadow_get_ShortName( GDALDriverShadow *h ) {
  return GDALGetDriverShortName( h );
}
char const *GDALDriverShadow_get_LongName( GDALDriverShadow *h ) {
  return GDALGetDriverLongName( h );
}
char const *GDALDriverShadow_get_HelpTopic( GDALDriverShadow *h ) {
  return GDALGetDriverHelpTopic( h );
}
%}


%include typemaps_csharp.i
