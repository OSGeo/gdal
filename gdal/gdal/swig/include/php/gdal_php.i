/*
 * $Id$
 *
 * php specific code for gdal bindings.
 */

/*
 * $Log$
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accommodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */

%init %{
  if (GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%include typemaps_php.i
