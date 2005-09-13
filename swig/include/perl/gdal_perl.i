/*
 * $Id$
 *
 * perl specific code for gdal bindings.
 */

/*
 * $Log$
 * Revision 1.2  2005/09/13 17:36:28  kruland
 * Whoops!  import typemaps_perl.i.
 *
 * Revision 1.1  2005/09/13 16:08:45  kruland
 * Added perl specific modifications for gdal and ogr.
 *
 *
 */

%init %{
  /* gdal_perl.i %init code */
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%include cpl_exceptions.i

%import typemaps_perl.i
