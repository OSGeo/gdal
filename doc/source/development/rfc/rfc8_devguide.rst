.. _rfc-8:

===========================
RFC 8: Developer Guidelines
===========================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: draft

Purpose
-------

This document is intended to document developer practices for the
GDAL/OGR project. It will be an evolving document.

Portability
-----------

GDAL strives to be widely portable to 32bit and 64bit computing
environments. It accomplishes this in a number of ways - avoid compiler
specific directives, avoiding new, but perhaps not widely available
aspects of C++, and most importantly by abstracting platform specific
operations in CPL functions in the gdal/port directory.

Generally speaking, where available CPL functions should be used in
preference to operating system functions for operations like memory
allocation, path parsing, filesystem io, multithreading functions, and
ODBC access.

Variable Naming
---------------

Much of the existing GDAL and OGR code uses an adapted Hungarian naming
convention. Use of this convention is not mandatory, but when
maintaining code using this convention it is desirable to continue
adhering to it with changes. Most importantly, please avoiding using it
improperly as that can be very confusing.

In Hungarian prefixing the prefix tells something about about the type,
and potentially semantics of a variable. The following are some prefixes
used in GDAL/OGR.

-  *a*: array
-  *b*: C++ bool. Also used for ints with only TRUE/FALSE values in C.
-  *by*: byte (GByte / unsigned char).
-  *df*: floating point value (double precision)
-  *e*: enumeration
-  *i*: integer number used as a zero based array or loop index.
-  *f*: floating point value (single precision)
-  *h*: an opaque handle (such as GDALDatasetH).
-  *n*: integer number (size unspecified)
-  *o*: C++ object
-  *os*: CPLString
-  *p*: pointer
-  *psz*: pointer to a zero terminated string. (eg. "char \*pszName;")
-  *sz*: zero terminated string (eg." char szName[100];")
-  TODO: What about constants (either global or global to a file)?
   Propose: *k*

Prefix can be stacked. The following are some examples of meaningful
variables.

-  \*char !\*\ *papszTokens*: Pointer to the an array of strings.
-  \*int *panBands*: Pointer to the first element of an array of
   numbers.
-  \*double *padfScanline*: Pointer to the first element of an array of
   doubles.
-  \*double *pdfMeanRet*: Pointer to a single double.
-  \*GDALRasterBand *poBand*: Pointer to a single object.
-  \*GByte *pabyHeader*: Pointer to an array of bytes.

It may also be noted that the standard convention for variable names is
to capitalize each word in a variable name.

Memory allocation
-----------------

As per `RFC 19: Safer memory allocation in
GDAL <./rfc19_safememalloc>`__, you can use VSIMalloc2(x, y) instead of
doing CPLMalloc(x \* y) or VSIMalloc(x \* y). VSIMalloc2 will detect
potential overflows in the multiplication and return a NULL pointer if
it happens. This can be useful in GDAL raster drivers where x and y are
related to the raster dimensions or raster block sizes. Similarly,
VSIMalloc3(x, y, z) can be used as a replacement for CPLMalloc(x \* y \*
z).

Headers, and Comment Blocks
---------------------------

.. _misc-notes:

Misc. Notes
-----------

-  Use lower case filenames.
-  Use .cpp extension for C++ files (not .cc).
-  Avoid spaces or other special characters in file or directory names.
-  Use 4 character indentation levels.
-  Use spaces instead of hard tab characters in source code.
-  Try to keep lines to 79 characters or less.

See also
--------

-  `http://erouault.blogspot.com/2016/01/software-quality-improvements-in-gdal.html <http://erouault.blogspot.com/2016/01/software-quality-improvements-in-gdal.html>`__
-  `https://travis-ci.org/OSGeo/gdal/builds <https://travis-ci.org/OSGeo/gdal/builds>`__
-  `https://ci.appveyor.com/project/OSGeo/gdal/history <https://ci.appveyor.com/project/OSGeo/gdal/history>`__
-  `https://travis-ci.org/rouault/gdal_coverage/builds <https://travis-ci.org/rouault/gdal_coverage/builds>`__
-  `https://ci.appveyor.com/project/rouault/gdal-coverage/history <https://ci.appveyor.com/project/rouault/gdal-coverage/history>`__
-  `https://gdalautotest-coverage-results.github.io/coverage_html/index.html <https://gdalautotest-coverage-results.github.io/coverage_html/index.html>`__

Python code
-----------

-  All Python code in autotest, swig/python/scripts and
   swig/python/samples should pass OK with the Pyflakes checker (version
   used currently: 0.8.1). This is asserted by Travis-CI jobs
-  Python code should be written to be compatible with both Python 2 and
   Python 3.
