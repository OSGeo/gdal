.. _rfc-82:

=============================================================
RFC 82: Update memory allocation handling/strategies
=============================================================

============== =============================================
Author:        Andrew Bell
Contact:       andrew.bell.ia@gmail.com
Started:       2021-May
Status:        Development
============== =============================================

Summary
-------

Move the standard GDAL memory allocation strategy to RAII and allow the language handing of
memory allocation errors to prevail.

Motivation
----------

The current model of GDAL memory handling is varied, subject to error and tedious.
Since GDAL is a C++ library, it is reasonable to take advantage of modern C++ memory
handling practices. Encouraging a more standard model will simplify code, improve consistency,
make it easier for new developers to contribute and reduce the likelihood of bugs.

Details
-------

When GDAL came into being the C++ standard library either didn't exist or wasn't well-supported
by many vendors. In addition, compilers were less sophisticated and tools to deal with
memory issues were costly and difficult to use.

Today, all relevant C++ compilers support the C++ standard through C++11 fully and GDAL
uses C++11 features (see RFC 68) . Most support
more recent standards as well. This allows the standard libraries to be used where
previously C-style memory allocation was the norm. This allows GDAL code to be written with
using a RAII model rather than a malloc/free (new/delete) model in most cases. Because RAII
eliminates the need to free memory, early returns are simple and safe and memory leaks are
harder to create. Exceptions can also be thrown without worrying about creating memory leaks.

The existing CPL/VSI utility functions provide support for catching memory allocation errors
and tracking programmer errors WRT memory, but this functionality has become virtually
obsolete because

    - The C++ standard library throws an exception (std::bad_alloc) when a memory error occurs,
      allowing an application or API user to catch the (unrecoverable) error and do what
      they wish. This eliminates any need to catch such failures locally. This exception could
      be added to current GDAL memory allocation utilities to provide consistency for
      existing code.
    - The GDAL memory utilities that assist in debugging errors has largely been supplanted
      by tools like valgrind and compiler sanitizers.
    - Proper use of RAII eliminates the majority of memory allocation errors.
    - Computers typically have much larger memory/VM available that makes the likelihood
      of allocation failures rare events.  Some systems will auto-allocate VM from
      available disk.

This RFC doesn't institute any particular change.  Instead it provides recommendations for
acceptable memory handling for changes to existing code and new additions.
The point is to specify how memory management
and handling in GDAL should be done if it were to be have been done from scratch in 2021.

Note that some discussion surrounding these issues has taken place in the past. For context
see this mailing list history, some of which is out of date:
https://lists.osgeo.org/pipermail/gdal-dev/2016-May/044298.html

Recommendations
---------------

    - Use the C++ standard library where possible for buffer creation and general
      data handling.
    - Where practical, use self-created RAII types where the C++ standard library is
      insufficient.
    - Allow memory errors to be thrown as std::bad_alloc exceptions.
    - Add std::bad_alloc handling to GDAL applications where practical.

Questions
---------

    - The GDAL public API is large and in many places depends on knowledge of types with
      ownership requirements that are difficult.  Should the API be maintained in
      perpetuity? Should alternative versions of functions be created that help eliminate
      memory errors for users and simplify handling of data structures internally?
    - Does acceptance of this RFC supercede RFC 19?



Voting History
--------------


