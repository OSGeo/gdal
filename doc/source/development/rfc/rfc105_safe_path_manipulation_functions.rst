.. _rfc-105:

===================================================================
RFC 105: Add and use safe path manipulation functions
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2025-01-11
Status:        Adopted, implemented in 3.11
Target:        GDAL 3.11
============== =============================================

Summary
-------

This RFC adds safe versions, for use by C++ code, of all functions of
cpl_path.cpp (such as CPLGetPath(), CPLGetDirname(), CPLGetBasename(), CPLGetExtension(),
CPLFormFilename(), CPLFormCIFilename(), etc.), that returns a result stored
in more or less ephemeral storage, to avoid potential security issues related
to their mis-use. It also covers converting most of the code base to the safer
alternatives.

Motivation
----------

Above mentioned functions return a ``const char *`` string, which is owned by
a thread-local rotating buffer of 10 strings of up to 2048 bytes each, managed
by the `CPLGetStaticResult() <https://github.com/OSGeo/gdal/blob/ea26bd087b3e34b91ef0315ca3889f39445f2e1f/port/cpl_path.cpp#L53>`__
function of port/cpl_path.cpp

The current functions are very easy to use, allowing to do things like the
following, without caring about freeing temporary buffers:

.. code-block:: c

    const char *pszDirname = CPLGetDirname(pszFilename);
    const char *pszBasename = CPLGetBasename(pszFilename);
    const char *pszSidecarFile = CPLFormFilename(pszDirname, pszBasename, "bin");


But this ease of use doesn't come risk-free, given that there is only room for
10 strings for a given thread. So if doing the following:

.. code-block:: c

        const char* pszExt = CPLGetExtension(mainFile);
        for (int i = 0; i < 10; ++i )
        {
            do_something(CPLGetExtension(auxiliaryFiles[i]));
        }
        do_something_else(pszExt);

when reaching the last line, ``pszExt`` is no longer an alias of
``mainFile``, but of `CPLGetExtension(auxiliaryFiles[0])`. This will in the best
cases "just" cause malfunction, in bad cases, reading outside the bounds of
buffers, and perhaps even worse when unlucky.

This risk of such mis-use has been identified for long, and the documentation of
those functions states that the result they return is short-lived, and that
it must be stored in a longer-living storage when not immediately used.

Thus, patterns like the following is the recommended practice:

.. code-block:: c++

        const std::string osExt = CPLGetExtension(mainFile);
        for (int i = 0; i < 10; ++i )
        {
            do_something(std::string(CPLGetExtension(auxiliaryFiles[i])).c_str());
        }
        do_something_else(osExt.c_str());


However, experience has shown that it is extremely easy to forget that, and over
the years, this has caused several heap buffer overflows bugs detected by
oss-fuzz, and probably latent ones still uncovered.
The later in date is https://issues.oss-fuzz.com/issues/388868487 , fixed
by https://github.com/OSGeo/gdal/pull/11638 , and shows how subtle those bugs
can be. In that instance, the issue is due to complex chains of calls where we
end up using more than 10 instances of the thread-local storage.

Implementation
--------------

Rather to react on a case by case when fuzzers found issues, we want a
systematic approach to suppress their root case:

- Start from the implementation of the CPLXXXX() functions, copy them to
  C++ CPLXXXXSafe() functions that return a ``std::string`` instead of a ``const char *``,
  and modify them to no longer use thread-local rotating buffer of strings,
  but rely on std::string natural concatenation routines instead
  of the convoluted C concatenation logic, resulting in clearer code.

- make the now superseded CPLXXXX() functions call the Safe versions, and store
  the result in the thread-local rotating buffer of strings. So those functions
  will mostly behave as their current implementation

  That is a simple as:

  .. code-block:: c++

        const char *CPLGetPath(const char *pszFilename)
        {
            return CPLPathReturnTLSString(CPLGetPathSafe(pszFilename), __FUNCTION__);
        }


- deprecate the use of the legacy CPLXXXX() functions in C++ code and replace
  their use by the use of the new CPLXXXXSafe() functions.

Most of the unsafe calls can be automatically replaced by the safer alternatives
for patterns like the following ones ``some_func(CPLGetBasename(x))``, to be replaced by
``some_func(CPLGetBasenameSafe(x).c_str())``, using ``sed``.

Patterns like ``variable_name = CPLGetBasename(x)`` will however require manual
intervention. If variable_name is a ``std::string`` or ``CPLString``, then
replacing by ``variable_name = CPLGetBasenameSafe(x)`` is appropriate. If
variable_name is a ``const char*``, case-by-case analysis has to be done,
to either change its type to std::string / CPLString, or create a temporary
std::string, and have variable_name be assigned to std_string_temp.c_str().

All in all, during development of the candidate implementation, several hundreds
manual replacements have been made.

A ``#define`` based protection will prevent any GDAL C++ code from accidentally
re-using the unsafe functions.

Although most call sites can benefit from the safe alternatives, we cannot
remove completely the legacy functions,

- because they are used by C code. For such code, immediately
  storing the result with CPLStrdup() is the best alternative (and remembering
  to CPLFree())

- there is at least one known external code base (MapServer) that use some of
  the legacy functions, although it uses them from a C++ source code file, and
  thus could eventually migrate to the CPLXXXXSafe() functions when they are
  released.


Impact on performance
---------------------

There might be a theoretical impact on performance due to dynamic memory
allocation done by temporary ``std::string`` allocations, although normally
those uses occur in the identification and open part of drivers, and not in
performance critical loops. As the identification part is still critical, and
its main use if to get the filename extension, we extend the GDALOpenInfo class,
so it stores the filename extension as a member variable, which will save tens
of drivers from the need of calling CPLGetExtensionSafe(), as well as a
convenience `GDALOpenInfo::IsExtensionEqualToCI(const char* pszExtension)` method.
So, all in all, the performance impact of those changes is thought/hoped to be
in the hardly measurable category.

Out-of-scope (for the candidate implementation related to this RFC)
-------------------------------------------------------------------

We have exactly the same issue with ``const char* CPLSPrintf(const char* pszFmt, ....)``
which uses its own thread-local rotating buffer of 10 strings of size up to
8000 bytes each.

The safer alternative exists as ``CPLString::Printf()`` and it would be worth
generalizing its use. I don't remember (at least recent) security related issues
related to mis-use, but there have been bugs related to the truncation to 8000
bytes.

To keep things manageable in terms of implementation, such replacement will not
be covered by the candidate implementation linked to this RFC, but this could be
a worthwhile goal to pursue besides it.

Risks
-----

Due to the amount of changes, in particular for replacements that could not be
automated, there is a non-zero risk of causing regressions,
in particular in drivers with poor coverage of our regression test suite.

Backward compatibility
----------------------

No impact

Testing
-------

Existing autotest suite will cover changes.

Documentation
-------------

No impact

Related issues and PRs
----------------------

* Bug that triggered this PR: https://github.com/OSGeo/gdal/pull/11638

* Candidate implementation: https://github.com/OSGeo/gdal/pull/11639

Funding
-------

Funded by GDAL Sponsorship Program (GSP).

Voting history
--------------

+1 from PSC members HowardB, JukkaR, KurtS, JavierJS, DanB and EvenR



.. below is an allow-list for spelling checker.

.. spelling:word-list::
    fuzzers
