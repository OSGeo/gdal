.. _rfc-5:

=======================================================================================
RFC 5: Unicode support in GDAL
=======================================================================================

Author: Andrey Kiselev

Contact: dron@ak4719.spb.edu

Status: Development

Summary
-------

This document contains proposal on how to make GDAL core locale
independent preserving support for native character sets.

Main concepts
-------------

GDAL should be modified in a way to support three following main ideas:

1. Users work in localized environment using their native languages.
   That means we can not assume ASCII character set when working with
   string data passed to GDAL.
2. GDAL uses UTF-8 encoding internally when working with strings.
3. GDAL uses Unicode version of third-party APIs when it is possible.

So all strings, used in GDAL, are in UTF-8, not in plain ASCII. That
means we should convert user's input from the local encoding to UTF-8
during interactive sessions. The opposite should be done for GDAL
output. For example, when user passes a filename as a command-line
parameter to GDAL utilities, that filename should be immediately
converted to UTF-8 and only afetrwards passed to functions like
GDALOpen() or OGROpen(). All functions, which take character strings as
parameters, assume UTF-8 (with except of several ones, which will do the
conversion between different encodings, see Implementation). The same is
valid for output functions. Output functions (CPLError/CPLDebug),
embedded in GDAL, should convert all strings from UTF-8 to local
encoding just before printing them. Custom error handlers should be
aware of UTF-8 issue and do the proper transformation of strings passed
to them.

The string encoding pops up again when GDAL needs to call the
third-party API. UTF-8 should be converted to encoding suitable for that
API. In particular, that means we should convert UTF-8 to UTF-16 before
calling CreateFile() function in Windows implementation of VSIFOpenL().
Another example is a PostgreSQL API. PostgreSQL stores strings in UTF-8
encoding internally, so we should notify server that passed string is
already in UTF-8 and it will be stored as is without any conversions and
losses.

For file format drivers the string representation should be worked out
on per-driver basis. Not all file formats support non-ASCII characters.
For example, various .HDR labeled rasters are just 7-bit ASCII text
files and it is not a good idea to write 8-bit strings in such a files.
When we need to pass strings, extracted from such file outside the
driver (e.g., in SetMetadata() call), we should convert them to UTF-8.
If you just want to use extracted strings internally in driver, there is
no need in any conversions.

In some cases the file encoding can differ from the local system
encoding and we do not have a way to know the file encoding other than
ask a user (for example, imagine a case when someone added a 8-bit
non-ASCII string field to mentioned above plain text .HDR file). That
means we can't use conversion form the local encoding to UTF-8, but from
the file encoding to UTF-8. So we need a way to get file encoding in
some way on per datasource basis. The natural solution of the problem is
to introduce optional open parameter "ENCODING" to GDALOpen/OGROpen
functions. Unfortunately, those functions do not accept options. That
should be introduced in another RFC. Fortunately, tehre is no need to
add encoding parameter immediately, because it is independent from the
general i18n process. We can add UTF-8 support as it is defined in this
RFC and add support for forcing per-datasource encoding later, when the
open options will be introduced.

Implementation
--------------

-  New character conversion functions will be introduced in CPLString
   class. Objects of that class always contain UTF-8 string internally.

::


   // Get string in local encoding from the internal UTF-8 encoded string.
   // Out-of-range characters replaced with '?' in output string.
   // nEncoding A codename of encoding. If 0 the local system
   // encoding will be used.
   char* CPLString::recode( int nEncoding = 0 );

   // Construct UTF-8 string object from string in other encoding
   // nEncoding A codename of encoding. If 0 the local system
   // encoding will be used.
   CPLString::CPLString( const char*, int nEncoding );

   // Construct UTF-8 string object from array of wchar_t elements.
   // Source encoding is system specific.
   CPLString::CPLString( wchar_t* );

   // Get string from UTF-8 encoding into array of wchar_t elements.
   // Destination encoding is system specific.
   operator wchar_t* (void) const;

-  In order to use non-ASCII characters in user input every application
   should call setlocale(LC_ALL, "") function right after the entry
   point.

-  Code example. Let's look how the gdal utilities and core code should
   be changed in regard to Unicode.

For input instead of

::

   pszFilename = argv[i];
   if( pszFilename )
       hDataset = GDALOpen( pszFilename, GA_ReadOnly );

we should do

::


   CPLString oFilename(argv[i], 0); // <-- Conversion from local encoding to UTF-8
   hDataset = GDALOpen( oFilename.c_str(), GA_ReadOnly );

For output instead of

::


   printf( "Description = %s\n", GDALGetDescription(hBand) );

we should do

::


   CPLString oDescription( GDALGetDescription(hBand) );
   printf( "Description = %s\n", oDescription.recode( 0 ) ); // <-- Conversion
                               // from UTF-8 to local

The filename passed to GDALOpen() in UTF-8 encoding in the code snippet
above will be further processed in the GDAL core. On Windows instead of

::


   hFile = CreateFile( pszFilename, dwDesiredAccess,
       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, dwCreationDisposition,
       dwFlagsAndAttributes, NULL );

we do

::


   CPLString oFilename( pszFilename );
   // I am prefer call the wide character version explicitly
   // rather than specify _UNICODE switch.
   hFile = CreateFileW( (wchar_t *)oFilename, dwDesiredAccess,
           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
           dwCreationDisposition,  dwFlagsAndAttributes, NULL );

-  The actual implementation of the character conversion functions does
   not specified in this document yet. It needs additional discussion.
   The main problem is that we need not only local<->UTF-8 encoding
   conversions, but *arbitrary*\ <->UTF-8 ones. That requires
   significant support on software part.

Backward Compatibility
----------------------

The GDAL/OGR backward compatibility will be broken by this new
functionality in the way how 8-bit characters handled. Before users may
rely on that all 8-bit character strings will be passed through the
GDAL/OGR without change and will contain exact the same data all the
way. Now it is only true for 7-bit ASCII and 8-bit UTF-8 encoded
strings. Note, that if you used only ASCII subset with GDAL, you are not
affected by these changes.

From The Unicode Standard, chapter 5:

*The width of wchar_t is compiler-specific and can be as small as 8
bits. Consequently, programs that need to be portable across any C or
C++ compiler should not use wchar_t for storing Unicode text.*

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
-  Filenames also covered in [[wiki:rfc30_utf8_filenames]]
