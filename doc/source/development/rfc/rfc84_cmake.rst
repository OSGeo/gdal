.. _rfc-84:

===========================================================
RFC 84: Migrating build systems to CMake
===========================================================

======== ==================================================
Authors: Even Rouault, Nyall Dawson, Howard Butler
======== ==================================================
Contact: even.rouault at spatialys.com,
         nyall.dawson at gmail.com,
         howard at hobu.co
Started: 2021-09-22
Status:  Adopted (implementation in progress)
======== ==================================================

Summary
-------

The document proposes :

- to develop a CMake build system, officially integrated in the source tree.

- and remove the current GNU makefiles and nmake build systems, when the CMake
  build system has matured enough and reached feature parity.
  We don't want to end up with a https://xkcd.com/927/ situation.

Motivation
----------

- A dual build system means editing twice, which increases the amount of work and
  the chance for bugs.

- For Unix, we have a rather custom and non-idiomatic build system using autoconf,
  but not automake.

- The makefiles in both build systems are hand-written. One of their main deficiency
  from the point of view of a GDAL developer is the lack of tracking of header
  dependencies. It is this very easy to produce a corrupted GDAL build, if forgetting to
  manually clean a previous build. This is a serious obstacle to embed regular or
  occasional GDAL developers that hit that issue, generally not found in other
  projects.

- Neither Unix or Windows builds support out-of-tree builds.

- Windows builds have poor support for parallel build: it is limited to the files
  in one directory.

- There is generally no consistency in the naming of build options in our
  Unix and Windows build systems, which makes life harder for users having to
  build GDAL on multiple platforms.

- The two existing build systems do not have the same features. For example,
  configure now offers the option to selectively disable drivers, including ones
  that do not depend on external dependencies, using a opt-out or opt-in
  strategies, whereas the NMake build does not.

- CMake has become the defacto solution with the widest adoption for C/C++ software that
  want to address multiple platforms. Most of GDAL dependencies have at least a
  CMake build system, and some have now CMake as the only option (GEOS 3.10 will
  only have a CMake build system, and the same is proposed for PROJ 9 in
  https://github.com/OSGeo/PROJ/pull/2880), making soon CMake a de-facto requirement
  for a GDAL build.
  Looking a bit in the FOSS4G field for C/C++ project, CMake is ubiquitous, so
  there is a widespread knowledge of it among existing or potential contributors
  to GDAL.

- A CMake build system has been asked repeatedly by many developers or users of
  GDAL over the past years. We are aware of a least two public out-of-tree CMake
  efforts: https://github.com/miurahr/cmake4gdal and https://github.com/nextgis-borsch/lib_gdal

- CMake has the widest industry tooling support. A number of Modern C++ IDEs offer good support for CMake:
  Visual Studio, qtcreator, etc.
  This should help reduce the barrier for contribution.

- CMake development is active with regular feature releases, whereas the technologies
  behind our existing build systems are more in a maintenance mode.

Why not CMake?
--------------

Other modern cross-platform build systems exist, including Meson and Bazel,
which have many advantages over CMake. However, they are currently not widely
familiar or used by active GDAL contributors. CMake represent the current
"least worse" solution for multiple platform builds when comparing its capabilities
and its maturity.

CMake does not address simultaneously building shared and static libraries. This
can however be worked around by doing 2 builds and merging build artifacts.

Phases / Schedule
-----------------

The following is a rather conservative tentative schedule:

- Add CMake as an experimental build system. Add it in master to ease the
  contribution. Its experimental/development status will be mentioned in general
  GDAL documentation and in the report output when running cmake.

  ==> Target: GDAL 3.5 / May 2022

- Formally deprecate GNUmakefile and NMake base file systems.
  Users and packagers are encouraged to switch to CMake and actively report
  (and help fixing) issues the find in the process.

  ==> Target: GDAL 3.6 / November 2022

- Completely remove GNUmakefile and NMake base file systems, and make CMake the
  only build system in GDAL source tree.

  ==> Target: GDAL 3.7 / May 2023

Details
-------

The above mentioned cmake4gdal repository seems to us the best starting point.
It respects the current tree organization of the GDAL repository and targets a
not too old CMake version (3.10).

We will start by running the scripts that deploy the CMakeLists.txt and other
cmake support files, and commit them into the GDAL repository.

A checklist of all features of our current build systems that will need to be
ported and checked has been initiated in:
https://docs.google.com/spreadsheets/d/1SsUXiZxKim6jhLjlJFCRs1zwMvNpbJbBMB6yl0ms01c/edit

SWIG bindings
-------------

The Python, Java and CSharp bindings will be available as options of the CMake
build options.

The Perl bindings will not be available, as being planned for removal in GDAL 3.5

Compatibility
-------------

The addition of the CMake build system, being mostly an addition during the transient
phase where it will be available alongside the existing build systems, should
moderately impact existent files. However, it is likely that there will be some
improvements that affect C++ files (for example, to use consistently ``#include <project/header.h>``
style of inclusion instead of the``#include <header.h>`` with ``-I${include_prefix}/project``
pattern sometimes used) and the GNUmakefile/makefile.vc files.

We may use of PRIVATE linking of vendored and intermediate libraries to hide
non-public symbols. This might change a bit from existing builds that can leak them.

Minimum CMake version
---------------------

cmake4gdal uses CMake 3.10 as the minimum version. This is a reasonable choice,
as it would be compatible with the cmake version provided by Ubuntu 18.04 for example,
the current old-stable Ubuntu LTS.

Given the mentioned schedule, CMake will become a requirement in May 2023,
at a time where the new old-stable LTS will be Ubuntu 20.04. So we can't exclude
we will bump this minimum version if it is found to be more practical.
For example, CMake 3.12 adds an easier way for handling "object libraries", that
can help solving issues regarding static builds and vendored dependencies
(cf https://github.com/libgeos/geos/issues/463)

Supported platforms
-------------------

Our continuous integration "only" tests Linux (Intel/AMD, ARM64 and s390x architectures),
Android (build only), MacOSX and Windows. We will welcome involvement at some point
from users/developers of other environments to test and help address any outstanding issues.

General requirements
--------------------

The following lists a few requirements to consider the new build system be ready,
and the existing ones can be removed:

- The build system works on most environments where the build systems are known to work.
  For CI-tested environments, this will involve porting to them and checking that
  the builds are functional. For other build systems, we will depend on manual testing
  from users.

- objdir / out-of-source builds are supported.

- cross builds are supported.

- Explicit testing of OSes through ``if(THIS_OS)`` should be limited, and replaced
  by testing of feature wherever doable.

- There has been a formal release (presumably 3.6) with existing build systems
  and cmake where cmake meets the above requirements, as verified by packager feedback.

Funding
-------

Even Rouault and Nyall Dawson will use project sponsorship funding to complete
that work. An estimate of 2 man-months of effort has been made recently to
provide an initial build out of CMake support for GDAL.

Voting history
--------------

+1 from PSC members: HowardB, MateuszL, KurtS, DanielM and JukkaR
