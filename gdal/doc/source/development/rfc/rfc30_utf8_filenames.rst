.. _rfc-30:

================================================================================
RFC 30: Unicode Filenames
================================================================================

Authors: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

This document describes steps to generally handle filenames as UTF-8
strings in GDAL/OGR. In brief it will be assumed that filenames passed
into and returned by GDAL/OGR interfaces are UTF-8. On some operating
systems, notably Windows, this will require use of "wide character"
interfaces in the low level VSI*L API.

Key Interfaces
--------------

VSI*L API
~~~~~~~~~

All filenames in the VSI*L API will be treated as UTF-8, which means the
cpl_vsil_win32.cpp implementation will need substantial updates to use
wide character interfaces.

-  VSIFOpenL()
-  VSIFStatL()
-  VSIReadDir()
-  VSIMkdir()
-  VSIRmdir()
-  VSIUnlink()
-  VSIRename()

Old (small file) VSI API
~~~~~~~~~~~~~~~~~~~~~~~~

The old VSIFOpen() function will be adapted to use \_wfopen() on windows
instead of fopen() so that utf-8 filenames will be supported.

-  VSIFOpen()
-  VSIStat()

Filename Parsing
~~~~~~~~~~~~~~~~

Because the path/extension delimiter characters '.', '', '/' and ':'
will never appear in the non-ascii portion of utf-8 strings we can
safely leave the existing path parsing functions working as they do now.
They do not need to be aware of the real character boundaries for exotic
characters in utf-8 paths. The following will be left unchanged.

-  CPLGetPath()
-  CPLGetDirname()
-  CPLGetFilename()
-  CPLGetBasename()
-  CPLGetExtension()
-  CPLResetExtension()

Other
~~~~~

-  CPLStat()
-  CPLGetCurrentDir()
-  GDALDataset::GetFileList()

These will all also need to treat filenames as utf-8.

Windows
-------

Currently Windows's cpl_vsil_win32.cpp module uses CreateFile() with
ascii filenames. It needs to be converted to use CreateFileW() and other
wide character functions for stat(), rename, mkdir, etc. Prototype
implementation already developed (r20620).

.. _linux--unix--macos-x:

Linux / Unix / MacOS X
----------------------

On modern linux, unix and MacOS operating systems the fopen(), stat(),
readdir() functions already support UTF-8 strings. It is not currently
anticipated that any work will be needed on Linux/Unix/MacOS X though
there is some question about this. It is considered permissible under
the definition of this RFC for old, and substandard operating systems
(WinCE?) to support only ASCII, not UTF-8 filenames.

Metadata
--------

There are a variety of places where general text may contain filenames.
One obvious case is the subdataset filenames returned from the
SUBDATASET domain. Previously these were just exposed as plain text and
interpretation of the character set was undefined. As part of this RFC
we state that such filenames should be considered to be in utf-8 format.

Python Changes
--------------

I observe with Python 2.6 that functions like gdal.Open() do not accept
unicode strings, but they do accept utf-8 string objects. One possible
solution is to update the bindings in selective places to identify
unicode strings passed in, and transform them to utf-8 strings.

eg.

::

   filename =  u'xx\u4E2D\u6587.\u4E2D\u6587'
   if type(filename) == type(u'a'):
       filename = filename.encode('utf-8')

I'm not sure what the easiest way is to accomplish this in the bindings.
The key entries are:

-  gdal.Open()
-  ogr.Open()
-  gdal.ReadDir()
-  gdal.PushFinderLocation()
-  gdal.FindFile()
-  gdal.Unlink()

Similarly all interfaces (ie. gdal.ReadDir()) that return filenames will
hereafter return unicode objects rather than string objects.

Also note that in Python 3.x strings are always unicode.

C# Changes
----------

Tamas notes that in C# we normally convert the unicode C# strings into C
string with the PtrToStringAnsi marshaller. Presumably we will need to
use a utf-8 converter for all interface strings considered to be
filenames. I would note this should also apploy to OGR string attribute
values which are also intended to be treated as utf-8.

(It is unclear who will take care of this aspect since the primary
author (FrankW) is not C#-binding-competent.

Perl Changes
------------

The general rule in Perl is that all strings should be decoded before
giving them to Perl and encoded when they are output. In practice things
usually just work. To be sure, I (Ari) have added an explicit decode
from utf8 to FindFile and ReadDir (#20800).

Java Changes
------------

No changes are needed for Java. Java strings are unicode, and they are
already converted to utf-8 in the java swig bindings. That is, the java
bindings already assumed passing and receiving utf-8 strings to/from
GDAL/OGR.

Commandline Issues
------------------

On windows argv[] as passed into main() will not generally be able to
represent exotic filenames that can't be represented in the locale
charset. It is possible to fetch the commandline and parse it as wide
characters using GetCommandLineW() and CommandLinetoArgvW() to capture
ucs-16 filenames (easily converted to utf-8); however, this interferes
with the use of setargv.obj to expand wildcards on windows.

I have not been able to come up with a good solution, so for now I am
not intending to make any changes to the GDAL/OGR commandline utilities
to allow passing exotic filenames. So this RFC is mainly aimed at
ensuring that other applications using GDAL/OGR can utilize exotic
filenames.

File Formats
------------

The proposed implementation really only addresses file format drivers
that use VSIFOpenL(), VSIFOpen() and related functions. Some drivers
dependent on external libraries (ie. netcdf) do not have a way to hook
the file IO API and may not support utf-8 filenames. It might be nice to
be able to distinguish these.

At the very least any driver marked with GDAL_DCAP_VIRTUALIO as "YES"
will support UTF-8. Perhaps this opportunity ought to be used to more
uniformly apply this driver metadata (done).

Test Suite
----------

We will need to introduce some test suite tests with multibyte utf-8
filenames. In support of that aspects of the VSI*L API - particularly
the rename, mkdir, rmdir, functions and VSIFOpenL itself have been
exposed in python.

Documentation
-------------

Appropriate API entry points will be documented as taking and return
UTF-8 strings.

Implementation
--------------

Implementation is underway and being tracked in ticket #3766.
