/******************************************************************************
 * $Id$
 *
 * Name:     typemaps.i
 * Project:  GDAL Typemap library
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.1  2005/02/24 17:44:54  kruland
 * Generic typemap.i file which delegates to the appropriate language
 * specific file.
 *
 *
*/

#ifdef SWIGCSHARP
%import typemaps_csharp.i
#endif

#ifdef SWIGPYTHON
%import typemaps_python.i
#endif
