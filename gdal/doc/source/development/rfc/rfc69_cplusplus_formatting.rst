.. _rfc-69:

=======================================================================================
RFC 69: C/C++ Code Formatting
=======================================================================================

This document proposes and describes desired code formatting style used
across C and C++ source code in GDAL.

======== ======================================
Author:  Kurt Schwehr
Contact: schwehr@google.com / schwehr@gmail.com
Started: 2017-May-04
Status:  *Work-In-Progress*
======== ======================================

This RFC is based on `GEOS RFC
4 <https://trac.osgeo.org/geos/wiki/RFC4>`__ by Mateusz Łoskot.

**TODO:** Give examples of formatting.

Summary
-------

The document proposes and describes desired default code formatting
style guidelines for GDAL programming in C and C++ languages.

The goal of this document is to initiate process to reach an agreement
for the default code formatting style.

Motivation
----------

There is a need to decide on format of GDAL source code and apply such
globally consistent format to GDAL C/C++ codebase.

A uniform, codebase-wide formatting style makes reading and
comprehending existing code easier, writing code focused on important
aspects of new developments and more pleasant, removes burden during a
patch or pull request code reviews and prevents `bikeshedding religious
arguments <http://wiki.c2.com/?WhereDoTheBracesGo>`__. Even in small
projects, contributing developers discover the problems of working
without an agreed upon code format.

The utility of such guidelines has been proven by many open source
software projects.

The scope of the proposal is specifically limited to formatting style
guidelines. It is not an intention to develop a general coding guide
covering other aspects of writing software like naming, etc.

Proposal
--------

It is important to make effortless for developers to produce properly
formatted code.

The proposal suggests to use
`clang-format <https://clang.llvm.org/docs/ClangFormat.html>`__ version
3.8 or higher to define C++ code formatting rules for GDAL code.

The ``clang-format`` is a tool to automatically format C/C++ code, so
that developers don't need to worry about style issues. Unlike other
tools which use own parsers, ``clang-format`` uses the Clang tokenizer
and supports the same C++ source code as the Clang compiler. This
guarantees correct output is produced and offers unique features (eg.
wrapping long lines whether of code, strings, arrays - something which
AStyle has no way of doing).

The style settings can be defined in a ``.clang-format`` configuration
file, however to make it as easy as possible, we will use the default
style (LLVM style?).

The ``clang-format`` is straightforward to run and can support
development workflow as standalone tool or as one of many editor
integrations or other bespoke utilities (eg. ``git cl format``
[Chromium]).

No automation of code reformatting is proposed. It would be treating the
symptoms, no cause: developers not following the code formatting
standard.

Although no means to enforce the default formatting style are proposed,
currently used CI services (eg. Travis CI) may be employed as a
post-commit safety valve - a clang-format lint failure as a compile
break (e.g.
`clang_format.py <https://github.com/mongodb/mongo/blob/master/buildscripts/clang_format.py>`__
build script used by MongoDB). Alternatively, a gatekeeper may be
installed in SVN/Git, rejecting commits with code not conforming to the
code formatting style.

Code Formatting Rules
---------------------

What code formatting rules to use?

*"A mature engineers know that a standard is more important than which
standard."* ~[MongoDB]

``clang-format`` offers several defaults (eg. LLVM, Mozilla, Linux,
Google C++ Style).

The proposal recommends to use one of the base styles without any
modification. It is possible to fine-tune the configuration, but this
RFC aims for simplicity.

The reasons are two-fold:

-  make GDAL code unified with the wide spectrum of well-established
   C/C++ projects
-  long arguments and religious wars prevention.

``.clang-format``
~~~~~~~~~~~~~~~~~

The hope is to avoid requiring a .clang-format file in the code base.

``.editorconfig``
~~~~~~~~~~~~~~~~~

`EditorConfig <http://editorconfig.org/>`__ is currently in use and
``.editorconfig`` file is provided to automatically tell popular code
editors about the basic style settings like indentation, whitespaces and
end-of-line markers for distinguished types of plain text files.

The ``.editorconfig`` file will have to be updated to match the chosen
``.clang-format`` settings if they are required.

EOL
~~~

``clang-format`` does not enforce line endings.

The EOL marker is considered to be `a part of a file encoding
decision <http://lists.llvm.org/pipermail/cfe-commits/Week-of-Mon-20130930/090200.html>`__
and not part of any coding style.

The EOL marker can be enforced as project-wide setting controlled with
``.gitattributes`` and ``.editorconfig``.

However, it shall still be left as configurable setting in developer's
environment of choice (eg. ``git config``) independently from the
project-wide setting.

Big Reformat
~~~~~~~~~~~~

What to do about the existing code?

The proposal recommends to just do one big reformat of the codebase.

While it may seem causing clutter in the repository log (eg.
``svn blame``), if it occurs infrequently (eg. yearly) and is applied to
the entire codebase at that time, it should not be very disruptive to
the source code history. One way to cope with skewed history is to use
``git blame -w`` which ignores whitespace when comparing commits.

Partial application of the code formatting rules would create more work
without delivering the full benefit [MongoDB] leading to codebase with
different styles mixed.

Branches
^^^^^^^^

Branches to run the big reformat in are:

-  ``trunk``
-  [STRIKEOUT:``branches/2.2``]
-  [STRIKEOUT:``branches/2.1``]
-  [STRIKEOUT:``branches/2.0``]

After Big Reformat
------------------

How to work against the natural entropy in a codebase:

-  It is highly recommended to use ``clang-format`` integration while
   writing a code.
-  Format changed code before committing or opening pull requests.
-  If you have to commit change in code formatting, do it in separate
   commit. Avoid commits with a mixture of code and formatting changes.

   -  There is downside of history clutter in repository, but this
      proposal states that a codebase with different styles across is
      even worse.

*"After all, every moment of time wasted on code formatting or
discussion thereof is eliminated."* ~[MongoDB]

Implementation
--------------

Set up Travis CI "style safety valve" build dedicated to run
clang-format lint based on the approach used in ``​clang_format.py``
script by MongoDB.

Miscellaneous
-------------

Those who build GDAL with GCC 6+ may appreciate consistent code format
style as it will help to avoid some dozens of the `new compiler
warnings <https://developers.redhat.com/blog/2016/02/26/gcc-6-wmisleading-indentation-vs-goto-fail/>`__:

::

   src/geom/Polygon.cpp: In member function ‘virtual int geos::geom::Polygon::getCoordinateDimension() const’:
   src/geom/Polygon.cpp:154:5: warning: this ‘if’ clause does not guard... [-Wmisleading-indentation]
        if( shell != NULL )
        ^~
   src/geom/Polygon.cpp:157:2: note: ...this statement, but the latter is misleadingly indented as if it is guarded by the ‘if’
     size_t nholes=holes->size();
     ^~~~~~

References
----------

-  [MongoDB] Succeeding With ClangFormat: `Part
   1 <https://engineering.mongodb.com/post/succeeding-with-clangformat-part-1-pitfalls-and-planning/>`__,
   `Part
   2 <https://engineering.mongodb.com/post/succeeding-with-clangformat-part-2-the-big-reformat/>`__,
   `Part
   3 <https://engineering.mongodb.com/post/succeeding-with-clangformat-part-3-persisting-the-change/>`__
-  [Chromium] `Using clang-format on Chromium C++
   Code <https://chromium.googlesource.com/chromium/src/+/master/docs/clang_format.md>`__
-  `https://clangformat.com <https://clangformat.com>`__ -
   ``clang-format`` interactive guide and builder
-  `https://zed0.co.uk/clang-format-configurator/ <https://zed0.co.uk/clang-format-configurator/>`__
-  `https://trac.osgeo.org/geos/wiki/RFC4 <https://trac.osgeo.org/geos/wiki/RFC4>`__
