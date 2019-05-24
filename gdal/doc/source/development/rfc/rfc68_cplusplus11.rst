.. _rfc-68:

==============================
RFC 68: C++11 Compilation Mode
==============================

======== ======================================
Author:  Kurt Schwehr
Contact: schwehr@google.com / schwehr@gmail.com
Started: 2017-Apr-11
Passed:  2017-Sep-11
Status:  Adopted, implemented in GDAL 2.3
======== ======================================

This RFC is based on `GEOS RFC
5 <http://trac.osgeo.org/geos/wiki/RFC5>`__ by Mateusz Łoskot.

Summary
-------

The document proposes to switch to
`C++11 <http://en.wikipedia.org/wiki/C%2B%2B11>`__ compilation mode as
default throughout the whole C++ source code of GDAL.

The goal of the document is to request and achieve agreement on using
C++11 as the minimum required version of the C++ programming language
standard.

Motivation
----------

C++11 is the first major update of the C++ standard since 1998.
(`C++03 <https://en.wikipedia.org/wiki/C%2B%2B03>`__ was a bug fix
release.)

Having fewer versions of C++ to support will reduce the load on
developers and testing systems.

C++11 features aim to promote writing clean, compact, type-safe and fast
code. It also delivers better feature-wise compatibility with the C
language (C99).

The Wikipedia article at
`http://en.wikipedia.org/wiki/C++11 <http://en.wikipedia.org/wiki/C++11>`__
does a great job describing all changes in C++11 extensively.

The ``std::auto_ptr`` smart pointer, together with a bunch of other
features, were deprecated and will be removed from C++17. Features like
``std::unique_ptr`` provide much stronger replacements.

Enabling C++11 compilation mode will improve the programming environment
making it much friendlier than C++98.

A social factor: since (many) C++ programmers no longer enjoy C++98,
allowing C++11 mode may increase potential for new contributions.

Compilers Landscape
-------------------

Summary of compilers supported by GDAL with their minimal versions
required to compile source code based on

`http://en.cppreference.com/w/cpp/compiler_support <http://en.cppreference.com/w/cpp/compiler_support>`__
C++11 features.

C++11
~~~~~

===== ============ ========================================================================= =================================================================================================
GCC   4.8.1+       `C++11 status <https://gcc.gnu.org/projects/cxx-status.html#cxx11>`__     Debian 8 (stable), Ubuntu 15.04+, Ubuntu 14.04 ``ppa:ubuntu-toolchain-r/test``, Fedora 19+, RHEL7
===== ============ ========================================================================= =================================================================================================
Clang 3.3+         `C++11 status <https://clang.llvm.org/cxx_status.html#cxx11>`__           Debian 8 (stable), Ubuntu 14.04+, Fedora 19+, CentOS 6(?)
MSVC  14.0+ (2015) `C++11 status <https://msdn.microsoft.com/en-us/library/hh567368.aspx>`__ n/a
===== ============ ========================================================================= =================================================================================================

C++14
~~~~~

The C++14 compilers are listed for comparison only:

===== ============
GCC   4.9+
===== ============
Clang 3.4+
MSVC  14.0+ (2015)
===== ============

Plan
----

This proposal only requests agreement for the C++11 compilation mode
switch in the current ``trunk`` branch only.

This proposal does not suggest any detailed roadmap of large refactoring
of the GDAL C++ codebase.

The GDAL codebase is > 1.3M LOC and given the available man-power to
LOCs ratio, such one-step refactoring would not be feasible.

Instead, the task will be tackled with the baby step approach gradually
transforming the codebase according to priorities set along the way. Any
disruptive refactoring, changes in interfaces of C++ classes, breaking
changes in C++ API must be announced and discussed on the mailing list
or the bug tracker.

*IMPORTANT*: C++11 refactoring must not change the C API or break C API
compatibility, unless agreed upon based on prior RFC proposed.

However, if the proposal is accepted, any new C++ code written for GDAL
must be C++11-compliant.

Prior acceptance of this proposal is necessary in order to start any
source code refactoring using C++11 features.

Once accepted, first step will be to update the build configurations to
require C++11-compliant compiler.

Issues
------

This section outlines issues potentially caused by upgrade to C++11
language.

-  C++11 destructors, by default, have now the new exception
   specification of ``nothrow(true)``. Destructors of GDAL classes
   should be reviewed and any that are allowed/expected to throw
   exceptions must be marked with ``nothrow(false)``. Otherwise, any
   user of the existing GDAL codebase would find the program terminating
   whenever GDAL destructor throws an exception. Such review would be
   beneficial anyway.

Release
-------

First release of GDAL with C++11 compiler requirement could be 2.3.0.

.. _c14-1:

C++14
~~~~~

This section clarifies status of C++14 support in GDAL.

-  Once C++11 is adopted as default compilation mode, GDAL developers
   and maintainers must ensure it also successfully compiles in C++14
   and C++17 modes.

-  Are contributors allowed to add ``ifdef``'s for C++14 and C++17? No.
   Not at this time.

-  Is there a plan to upgrade to C++14 or C++17 to allow use of the C++
   latest features? No, there is no plan. It is, however, recognized,
   such motion may be put to the vote around 2020.

References
----------

-  `http://en.cppreference.com/w/cpp/compiler_support <http://en.cppreference.com/w/cpp/compiler_support>`__
   C++ compiler support

Self-assigned development constraints
-------------------------------------

The changes should have moderate impact on the existing GDAL/OGR code
base, and particularly on most of its code, that lies in drivers.
Existing users of the GDAL/OGR API should also be moderately impacted by
the changes, if they do not need to use the new offered capabilities.

GDAL has been working well with C++11 builds for a number of years now,
so there should be no externally visible changes for the initial removal
of C++03 support.

Core changes: summary
---------------------

1. Change configure.ac to remove ``with_cpp11`` flag, always use C++11,
   and fail if ``AX_CXX_COMPILE_STDCXX_11`` does not find C++11
2. Remove @CXX11_SUPPORT@ in GDALmake.opt.in and anywhere else it occurs
3. Remove continuous build targets that do not support C++11 from
   Travis-CI and AppVeyor
4. Remove #if HAVE_CXX11 and leave only the C++11 code

   -  ``find . -name \*.h -o -name \*.cpp | xargs egrep 'HAVE_CXX11'``

5. CPL_STATIC_ASSERT -> ``static_assert``
6. ``NULL`` -> ``nullptr`` (Only for C++ code)
7. CPL_OVERRIDE -> override and remove redundant virtual
8. -MAX or -max() -> ``std::numeric_limits<T>::lowest()``

Changes in SWIG bindings
------------------------

The switch does not impact the ``C`` API used by the SWIG bindings.
However, there may be some code that can be removed.

Potential changes that are *NOT* included in this RFC
-----------------------------------------------------

There are many C++11 features not address in this RFC. See
`https://en.wikipedia.org/wiki/C%2B%2B11 <https://en.wikipedia.org/wiki/C%2B%2B11>`__

-  attributes
-  ``auto``
-  ``consexpr``
-  ``cstdint``
-  ``delete`` and ``default`` for member functions
-  enum classes
-  initializer lists and ``std::initializer_list``
-  lambda
-  range based for loops
-  std::regex
-  rvalue references
-  smart pointers ``std::unique_ptr`` and ``std::shared_ptr``
-  New string literals: u8"An UTF-8 string", u"An UTF-16 str", U"An
   UTF-32 str", R"xml(raw content)xml"
-  ``std::thread``, ``thread_local`` and related
-  tuples
-  And more...

Backward compatibility
----------------------

Any code using the C++ API must use C++11 or newer.

The C API should not be impacted.

GDAL 2.2.x and older will continue to have C++03 support.

Testing
-------

The existing autotest suite should continue to pass.

Version numbering
-----------------

Although the above describes changes should have very few impact on
existing applications of the C API, some behavior changes, C++ level
changes and the conceptual changes are thought to deserve a 2.3 version
number.

Implementation
--------------

Implementation will be done by Kurt Schwehr. Others are welcome to pitch
in.

Related RFCs
------------

Related RFCs:

-  `GEOS RFC 5: C++11 Compilation
   Mode <http://trac.osgeo.org/geos/wiki/RFC5>`__: GEOS is switching to
   requiring C++11 to build.
-  `Move OTB to
   C++14 <https://wiki.orfeo-toolbox.org/index.php/Request_for_Comments-36:_Move_OTB_to_C%2B%2B14>`__

Voting history
--------------

`https://lists.osgeo.org/pipermail/gdal-dev/2017-September/047139.html <https://lists.osgeo.org/pipermail/gdal-dev/2017-September/047139.html>`__

-  EvenR +1
-  JukkaR +1
-  DanielM +0
-  HowardB +1
-  KurtS +1
