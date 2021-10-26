.. _rfc-23:

================================================================================
RFC 23.1: Unicode support in OGR
================================================================================

Authors: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted (implemented)

Summary
-------

This document proposes preliminary steps towards GDAL/OGR handling
strings internally in UTF-8, and supporting conversion between different
encodings.

Main concepts
-------------

GDAL should be modified in a way to support three following main ideas:

1. C Functions will be provided to support a variety of encoding
   conversions, including conversion between representations (ie. UTF-8
   to UCS-16/wchar_t).
2. Character encodings will be identified by iconv() style strings.
3. OFTString/OFTStringList feature attributes in OGR will be treated as
   being in UTF-8.

This RFC specifically does not attempt to address issues of using
non-ascii filenames. It also does not attempt to make definitions about
the encoding of other strings used in GDAL/OGR (such as field names,
metadata, etc). These would presumably be addressed in a later RFC
building on this one.

CPLRecode API
-------------

The following three C callable functions will be introduced for recoding
strings, and for converting between wchar_t (wide character) and char
(multi-byte) formats:

::

   char *CPLRecode( const char *pszSource, 
                    const char *pszSrcEncoding, const char *pszDstEncoding );

   char *CPLRecodeFromWChar( const wchar_t *pwszSource, 
                             const char *pszSrcEncoding, 
                             const char *pszDstEncoding );
   wchar_t *CPLRecodeToWChar( const char *pszSource,
                              const char *pszSrcEncoding, 
                              const char *pszDstEncoding );

In each case the returned string is zero terminated, as is the input
string, and the returned string should be deallocated with CPLFree(). In
case of error the returned string will be NULL, and the function will
issue a CPLError(). The functions will be marked with CPL_DLL and
considered part of the public GDAL/OGR API for use of applications as
well as internal use.

Encoding Names
--------------

It is proposed that the encoding names will be the same sorts of names
used by iconv(). So stuff like "UTF-8", "LATIN5", "CP850" and
"ISO_8859-1". It does not appear that these names for encodings are a
1:1 match with C library locale names (like "en_CA.utf8" for instance)
which may cause some issues.

Some particular names of interest:

-  "": The current locale. Use this when converting from/to the users
   locale.
-  "UTF-8": Unicode in multi-byte encoding. Most of the time this will
   be our internal linga-franca.
-  "POSIX": I think this is roughly ASCII (perhaps with some extended
   characters?).
-  "UCS-2": Two byte unicode. This is a wide character format and only
   suitable for use with the wchar_t methods.

On some systems you can use "iconv --list" to get a list of supported
encodings.

iconv()
-------

It is proposed to implement the CPLRecode() method using the iconv() and
related functions when available.

There is an excellent implementation of this API as GNU libiconv(),
which is used by the C libraries on Linux. Also some operating systems
provide the iconv() API as part of the C library (all unix?); however,
the system iconv() often has a restricted set of conversions supported
so it may be desirable to use libiconv in preference to the system
iconv() even when it is available.

If iconv() is not available, a stub implementation of the recode
services will be provided which:

-  implements UCS-2 / UTF-8 interconversion using either mbtowc/wctomb,
   or an implementation derived from
   `http://www.cl.cam.ac.uk/~mgk25/unicode.html <http://www.cl.cam.ac.uk/~mgk25/unicode.html>`__.
-  Implements recoding from "" to and from "UTF-8" by doing nothing, but
   issuing a warning on the first use if the current locale does not
   appear to be the "C" locale.
-  Implements recoding from "ASCII" to "UTF-8" as a null operation.
-  Implements recoding from "UTF-8" to "ASCII" by turning all non-ASCII
   multi-byte characters to '?'.

This hopefully gives us a weak operational status when built without
iconv(), but full operation when it is available.

The --with-iconv= option will be added to configure. The argument can be
the path to a libiconv installation or the special value 'system'
indicating that the system lib should be used. Alternatively,
--without-iconv can be used to avoid using iconv.

OFTString/OFTStringList Fields
------------------------------

It is declared that OGR string attribute values will be in UTF-8. This
means that OGR drivers are responsible for translating format specific
representations to UTF-8 when reading, and back to the format specific
representation when writing. In many cases (of simple ASCII text) this
requires no transformation.

This implies that the arguments to methods like OGRFeature::SetField(
int i, const char \*) should be UTF-8, and that GetFieldAsString() will
return UTF-8.

The same issues apply to OFTStringList lists of strings. Each string
will be assumed to be UTF-8.

OLCStringsAsUTF8 Capability Flag
--------------------------------

Some drivers (ie. CSV) can effectively not know the encoding of their
inputs. Therefore, it isn't always practical to turn things into UTF-8
in a guaranteed way. So, the new layer level capability called
"StringsAsUTF8" represented with the macro "OLCStringsAsUTF8" will be
testable at the layer level with TestCapability(). Drivers which are
certain to return string attributes as UTF-8 should return TRUE, while
drivers that do not know the encoding they return should return FALSE.
Any driver which knows it's encoding should convert to UTF-8.

OGR Driver Updates
------------------

The following OGR drivers could benefit immediately from recoding to
UTF-8 support in one way or another.

-  ODBC (add support for wchar_t / NVARSHAR fields)
-  Shapefile
-  GML (I'm not sure how the XML encoding values all map to our concept
   of encoding)
-  Postgres

I'm sure a number of the other drivers, particularly the RDBMS drivers,
could benefit from an update.

Implementation
--------------

Frank Warmerdam will implement the core iconv() capabilities, the
CPLRecode() additions and update the ODBC driver. Other OGR drivers
would be updated as time and demand mandates to conform to the
definitions in this RFC by interested developers.

The core work will be completed for GDAL/OGR 1.6.0 release.

References
----------

-  `The Unicode Standard, Version 4.0 - Implementation
   Guidelines <http://unicode.org/versions/Unicode4.0.0/ch05.pdf>`__ -
   Chapter 5 (PDF)
-  FAQ on how to use Unicode in software:
   `http://www.cl.cam.ac.uk/~mgk25/unicode.html <http://www.cl.cam.ac.uk/~mgk25/unicode.html>`__
-  FLTK implementation of string conversion functions:
   `http://svn.easysw.com/public/fltk/fltk/trunk/src/utf.c <http://svn.easysw.com/public/fltk/fltk/trunk/src/utf.c>`__
-  `http://www.easysw.com/~mike/fltk/doc-2.0/html/utf_8h.html <http://www.easysw.com/~mike/fltk/doc-2.0/html/utf_8h.html>`__
-  Ticket #1494 : UTF-8 encoding for GML output.
-  Libiconv:
   `http://www.gnu.org/software/libiconv/ <http://www.gnu.org/software/libiconv/>`__
-  ICU (another i18n library):
   `http://www.icu-project.org/ <http://www.icu-project.org/>`__
