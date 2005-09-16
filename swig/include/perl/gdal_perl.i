/*
 * $Id$
 *
 * perl specific code for gdal bindings.
 */

/*
 * $Log$
 * Revision 1.4  2005/09/16 19:17:46  kruland
 * Call UseExceptions on module init.
 *
 * Revision 1.3  2005/09/13 18:35:50  kruland
 * Rename GetMetadata_Dict to GetMetadata and ignore GetMetadata_List.
 *
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
  UseExceptions();
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%include cpl_exceptions.i

%rename (GetMetadata) GetMetadata_Dict;
%ignore GetMetadata_List;

%import typemaps_perl.i
