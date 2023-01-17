.. _rfc-69:

=======================================================================================
RFC 69: C/C++ Code Formatting
=======================================================================================

This document proposes and describes desired code formatting style used
across C and C++ source code in GDAL.

=========== ======================================
Author:     Kurt Schwehr
Contact:    schwehr@google.com / schwehr@gmail.com
Co-Author:  Alessandro Pasotti
Contact:    elpaso@itopen.it
Started:    2017-May-04
Status:     Adpoted, implemented
=========== ======================================

This RFC is based on `GEOS RFC
4 <https://trac.osgeo.org/geos/wiki/RFC4>`__ by Mateusz Łoskot.


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

A new pre-commit hook will be added to the current configuration in
``.pre-commit-config.yaml`` to run [clang-format pre-commit].

To enforce the code formatting, a gatekeeper will be installed in CI,
rejecting commits with code not conforming to the code formatting style
and a brief textual hint to install or update the pre-commit hooks
will be added to failure message.



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

To skip the "big reformat" commit from ``git blame``, git offers a mechanism
to [git_blame_ignore] , a pre-configured ignore file ``.git-blame-ignore-revs``
will be provided in the source tree with the "big reformat" commit 
hash in it.

The ignore file can be specified on the command line with 
``git blame --ignore-revs-file .git-blame-ignore-revs`` or it can be set 
permanently with ``git config blame.ignoreRevsFile .git-blame-ignore-revs``.

The a.m. instructions will be added to the `developers documentation <https://gdal.org/development/dev_practices.html>`__.


Branches
^^^^^^^^

Branches to run the big reformat in are:

-  ``master``
-  current stable version (to make backports easier)

After Big Reformat
------------------

The pre-commit hook will automatically take care of formatting
the code before every commit, the CI test will reject not formatted
code.

*"After all, every moment of time wasted on code formatting or
discussion thereof is eliminated."* ~[MongoDB]

Implementation
--------------

1. add clang-format to pre-commit configuration file
2. Set up GitHub workflow [clang-format-check]

A draft of the implementation is available at `this branch <https://github.com/elpaso/gdal/tree/rfc69_cplusplus_formatting_revival>`__.

The relevant files are:

- `pre-commit <https://github.com/elpaso/gdal/blob/rfc69_cplusplus_formatting_revival/.pre-commit-config.yaml#L30>`__
- `.clang-format style <https://github.com/elpaso/gdal/blob/rfc69_cplusplus_formatting_revival/.clang-format>`__
- `GH workflow <https://github.com/elpaso/gdal/blob/rfc69_cplusplus_formatting_revival/.github/workflows/clang-format-check.yml>`__
- `clang-format script <https://github.com/elpaso/gdal/blob/rfc69_cplusplus_formatting_revival/scripts/clang-format.sh>`__




References
----------

- [clang-format pre-commit] `Clang-Format Pre-Commit <https://github.com/pre-commit/mirrors-clang-format>`__
- [clang-format-check] `Clang-Format Check Workflow <https://github.com/marketplace/actions/clang-format-check>`__
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
- [git_blame_ignore] `Ignore Commits in Blame View` <Git https://docs.github.com/en/repositories/working-with-files/using-files/viewing-a-file#ignore-commits-in-the-blame-view>`__

Voting history
------------------

+1 from PSC members KurtS and EvenR
