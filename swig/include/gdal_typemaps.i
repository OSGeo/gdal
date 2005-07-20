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
 * Revision 1.2  2005/07/20 16:30:32  kruland
 * Import the php typemaps.
 *
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

#ifdef SWIGPHP
%import typemaps_php.i
#endif