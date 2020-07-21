.. _rfc-7:

=======================================================================================
RFC 7: Use VSILFILE for VSI*L Functions
=======================================================================================

Author: Even Rouault (Eric Doenges is original author)

Contact: even dot rouault at spatialys.com, Eric.Doenges@gmx.net

Status: Adopted

Purpose
-------

To change the API for the VSI*L family of functions to use a new
data-type VSILFILE instead of the current FILE.

Background, Rationale
---------------------

Currently, GDAL offers two APIs to abstract file access functions
(referred to as VSI\* and VSI\ *L in this document). Both APIs claim to
operate on FILE pointers; however, the VSI*\ L functions can only
operate on FILE pointers created by the VSIFOpenL function. This is
because VSIFOpenL returns a pointer to an internal C++ class typecast to
a FILE pointer, not an actual FILE pointer. This makes it impossible for
the compiler to warn when the VSI\* and VSI*L functions are
inappropriately mixed.

Proposed Fix
------------

A new opaque data-type VSILFILE shall be declared. All VSI\ *L functions
shall be changed to use this new type instead of FILE. Additionally, any
GDAL code that uses the VSI*\ L functions must be changed to use this
data-type as well.

RawRasterBand changes
---------------------

-  The 2 constructors are changed to accept a void\* fpRaw instead of a
   FILE\*
-  A new member VSILFILE\* fpRawL is added. The existing member FILE\*
   fpRaw is kept. The constructors will set the adequate member
   according to the value of the bIsVSIL parameter.
-  A new method VSILFILE\* GetFPL() is added.
-  The old FILE\* GetFP() is adapted to have same behavior as before
   (can return a standard FILE handle or a VSI*L handle depending on the
   handle that was passed to the constructor)

Those changes are meant to minimize the need for casting when using
RawRasterBand. Backward API compatibility is preserved.

Compatibility Issues, Transition timeline
-----------------------------------------

In order to allow the compiler to detect inappropriate parameters passed
to any of the VSI*L functions, VSILFILE will be declared with the help
of an empty forward declaration, i.e.

::

   typedef struct _VSILFILE VSILFILE

with the struct \_VSILFILE itself left undefined.

However, this would break source compatibility for any existing code
using the VSI*L API. Therefore, for now, VSILFILE is defined to be an
alias of FILE, unless the VSIL_STRICT_ENFORCE macro is defined.

::

   #ifdef VSIL_STRICT_ENFORCE
   typedef struct _VSILFILE VSILFILE;
   #else
   typedef FILE VSILFILE;
   #endif

In a future release (GDAL 2.0 ?), the behavior will be changed to
enforce the new strong typing.

Any future development done since the adoption of this RFC should use
VSILFILE when dealing with the VSIF*L API.

Questions
---------

-  Should we define VSIL_STRICT_ENFORCE by default when DEBUG is defined
   ?

This would make life easier for GDAL developers to use the appropriate
typing, but not affect API/ABI when using release mode.

Implementation
--------------

The whole source tree ( port, gcore, frmts, ogr, swig/include ) will be
altered adequatly so that the compilation works in VSIL_STRICT_ENFORCE
mode. Ticket #3799 contains a patch with the implementation. The
compilation doesn't add any new warning. The autotest suite still works
after this change.

The GeoRaster and JPIPKAK drivers have been modified during the
conversion process, but I'm not in position to compile them. Testing
appreciated. All other drivers that have been altered in the conversion
process have been compiled.

In the conversion process, a misuse of POSIX FILE API with a large file
handler was discovered in the ceos2 driver, but the function happened to
be unusued.

Voting History
--------------

::

   * Frank Warmerdam +1
   * Tamas Szekeres +1
   * Daniel Morissette +1
   * Howard Butler +1
   * Even Rouault +1

