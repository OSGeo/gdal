/*
 * $Id$
 *
 * perl specific code for gdal bindings.
 */

/*
 * $Log$
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

%import typemaps_python.i