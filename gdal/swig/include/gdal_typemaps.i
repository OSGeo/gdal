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
 * Revision 1.5  2006/02/02 20:52:40  collinsb
 * Added SWIG JAVA bindings
 *
 * Revision 1.4  2005/09/06 01:41:06  kruland
 * Added SWIGPERL include.
 *
 * Revision 1.3  2005/08/09 17:40:09  kruland
 * Added support for ruby.
 *
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

#ifdef SWIGRUBY
%import typemaps_ruby.i
#endif

#ifdef SWIGPERL
%import typemaps_perl.i
#endif

#ifdef SWIGJAVA
%import typemaps_java.i
#endif
