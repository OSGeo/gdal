.. _rfc-85:

===========================================================
RFC 85: Policy regarding substantial code additions
===========================================================

======== ==================================================
Authors: Howard Butler, Even Rouault
======== ==================================================
Started: 2022-01-17
Status:  Adopted
======== ==================================================

Summary
-------

This document describes the policies that the GDAL project will apply to assess
substantial code additions, typically new drivers, in particular coming from new
contributors to the project.

Motivation
----------

The GDAL project has historically been quite open to new code additions, including
drivers that rely on proprietary licensed and/or closed-source SDKs (called
"binary SDK" in the rest of this document). This approach is part
of its strengths, but also comes at a price, with the software becoming larger
and larger, and with contributions sometimes being abandoned by their authors without
new maintainers.

Being a free & open-source project, the GDAL project will apply stronger scrutiny to
code that requires a binary SDK and encourage contributors to submit code additions
that do not depend on such binary SDK, or license SDKs under free & open-source
license terms.

Policy
------

- Drivers require a designated responsible contact, tracked in
  https://github.com/OSGeo/gdal/wiki/Maintainers-per-sub-system

- Contributions should follow other RFCs describing development rules, and come
  with test scripts and sufficient documentation, covering usage and build instructions.

- If the driver require a binary SDK not downloadable without cost, or that requires
  a complicated registration process, the GDAL team is unlikely to support
  driver inclusion.
- GDAL supports multiple operating systems, and new drivers should support as many as
  is practical. At a minimum, drivers should work with the latest and OS-vendor supported
  releases of operating systems the driver is documented to work with.

- If the binary SDK is no longer supported, or modernized to work with current
  compilers and GDAL, the driver can be dropped. This rule also applies to open-source
  dependencies that are no longer maintained or are superseded by other alternatives.

- If the driver has unaddressed bugs, is breaking continuous integration (CI),
  has caused CI bits to be disabled, or has not caught up to API modifications
  requiring updates (extremely rare), the driver will be dropped from build
  scripts, and thus will not be built without significant user intervention.

- If critical/blocking issues reported in a driver are not addressed within a
  2 month time-frame, it can be dropped from the tree entirely by a designated
  GDAL maintainer for all releases going forward.

- Contributors of significant code additions are expected to participate in the
  day-to-day life of the GDAL project, and need to monitor closely the communication
  channels of the project: issue tracker, mailing lists, etc.

- Maintainers are expected to support their contributions by triaging bug reports,
  reviewing related pull requests and RFCs, making functional enhancements, testing
  releases, and improving documentation, tests, and infrastructure.

- In addition, maintainers are expected to respond in a timely manner to wider
  project changes (CI, build scripts, upgrade of dependencies, build tools,
  documentation, etc.) as it pertains to their contributions.

- New contributions may require a significant review effort from a GDAL committer (ie
  someone with direct modification rights to the source repository). While the
  project has funded maintainers, it must be understood that they might not be
  immediately available to do reviews of significant code additions. Contributors
  may contract GDAL committers to have such task done within a more predictable timeline.

- The above rules are not exhaustive. The PSC reserves the right to reject a proposed
  code addition depending on its particular nature and other contextual elements.

Voting history
--------------

+1 from PSC members MateuszL, KurtS, TamasS, SeanG, HowardB, JukkaR and EvenR.
