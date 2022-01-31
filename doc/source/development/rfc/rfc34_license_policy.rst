.. _rfc-34:

================================================================================
RFC 34: License Policy Enforcement
================================================================================

Authors: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Development

Summary
-------

This document proposes the addition of a new mechanisms so that
applications and end users can define a license policy, and so that GDAL
can help avoid license conflicts between proprietary and reciprocally
licensed applications and format drivers.

Definitions
-----------

Reciprocal FOSS License: A open source software license, such as the
GPL, that requires all other software components linked into the same
executable and distributed beyond the creator to also be offered under
open source terms.

Non-Reciprocal FOSS License: A open source software license, such as
MIT, BSD or LGPL, that does not place any requirements on other linked
components in the same executable at distribution time.

Proprietary License: Software provided under terms that do not adhere to
the requirements of the open source definition, such as libraries from
Oracle (OCI), Lizardtech (MrSID) and Erdas (ECW). While often offered
for zero cost, these components are incompatible with reciprocal FOSS
licenses and may place a variety of other restrictions on the
distributor or end user.

Rationale
---------

GDAL/OGR is distributed under the Non-Reciprocal MIT open source
license which facilitates it's use by proprietary and open source
applications, and facilitates the inclusion of proprietary format
drivers along side the open source format drivers. However, it is still
a license violation to distribute reciprocally licensed applications
(like QGIS and GRASS) which use GDAL with proprietary licensed drivers
(such as the MrSID, ECW or Oracle drivers). Likewise, it is a license
violation to distribute proprietary applications with reciprocally
licensed drivers such as the GDAL GRASS driver, or the PDF driver.

This RFC, and the improvements it promotes are intended to facilitate
users, applications and drivers setting and following license policies
to avoid unintentional license violations. One area this can be
particularly helpful is broad software distributions like
`OSGeo4W <http://osgeo4w.osgeo.org>`__.

Approach
--------

The general approach proposed is that drivers will declare their license
category, and applications or end users will declare a policy for what
sorts of drivers may be used in combination with them. The
GDALDriverManager and OGRDriverRegistrar classes will apply this
information to avoid unintentional license violations.

Drivers
-------

Drivers will declare one of these three driver specific licensing
policies via the "LICENSE_POLICY" (DMD_LICENSE_POLICY) metadata item on
the driver:

-  "RECIPROCAL": the driver is available under a reciprocal FOSS license
   such as the GPL, and should not be mixed with proprietary drivers or
   applications.
-  "NONRECIPROCAL": the driver is available under a non-reciprocal FOSS
   license such as MIT, or LGPL. This is the default if no licensing
   policy is declared and is the natural policy of drivers provided as
   part of GDAL without outside dependencies.
-  "PROPRIETARY": the driver, usually due to use of proprietary
   libraries, has some licensing restrictions which make it ineligible
   for distribution with reciprocally licensed software. This would
   include MrSID, ECW, and Oracle related drivers.

Application License Policy
--------------------------

Applications are encouraged to set one of the following licensing
policies reflective of the applications nature. The policy should be set
as the value of the GDAL_APPLICATION_LICENSE_POLICY configuration
variable, typically via a call to GDALSetConfigOption() *before* the
call to GDALAllRegister() or OGRRegisterAll().

-  "RECIPROCAL": the application is licensed under a reciprocal license
   such as the GPL, and no proprietary drivers should be loaded.
-  "PROPRIETARY": the application has some licensing restrictions which
   make it ineligible for distribution with reciprocally licensed
   software. Care will be taken to avoid loading reciprocally licensed
   drivers, such as the GRASS and PDF drivers.
-  "DEFAULT": the application does not apply any licensing restrictions.
   This is typical of non-GPL open source applications such as
   MapServer, and will be the default policy if nothing is declared.

User License Policy
-------------------

The restrictions on mixing proprietary and reciprocally licensed
software generally applies at the point of distribution. In particular,
it is not intended to prevent the end user from assembling a variety of
components for their own use as they see fit, for their own use. To that
end it is important to provide a mechanism for the end user to
deliberately override the restrictions on mixing reciprocally licensed,
and proprietary components. This is accomplished via the
GDAL_LICENSE_POLICY configuration variable which might typically be set
via the environment or via the --config commandline switch to most GDAL
applications. It may have the following values:

-  "USE_ALL": do not discard any drivers based on licensing
   restrictions.
-  "PREFER_PROPRIETARY": If there is a conflict between proprietary and
   reciprocally licensed drivers, use the proprietary ones.
-  "PREFER_RECIPROCAL": If there is a conflict between proprietary and
   reciprocally licensed drivers, use the reciprocally licensed ones.

In addition to setting this via config variables, there will also be a
configure / nmake.opt declaration to alter the default
GDAL_LICENSE_POLICY. Thus a local build could be configured to USE_ALL
at build time instead of having to set environment variables or
commandline switches. This would not be suitable for software that will
be redistributed.

Policy Logic
------------

1. If the user selected a GDAL_LICENSE_POLICY of "USE_ALL" then no
   drivers are unloaded on the basis of licensing.
2. If the user selected a GDAL_LICENSE_POLICY of "PREFER_PROPRIETARY" or
   "PREFER_RECIPROCAL" then ignore the GDAL_APPLICATION_LICENSE_POLICY.
3. if the application select a GDAL_APPLICATION_LICENSE_POLICY of
   "PROPRIETARY" or "RECIPROCAL" then use that.
4. In the absence of a user or application level policy, default to a
   policy of "PREFER_PROPRIETARY".

The policy will be applied in the GDALDriverManager::AutoSkipDrivers()
method and in the newly introduced OGRSFDriverManager::AutoSkipDrivers()
method. The AutoSkipDrivers() method is already used to unload drivers
based on GDAL_SKIP (and soon OGR_SKIP) and is generally called after the
preliminary registration of drivers.

Strict Link Level Compliance
----------------------------

The GPL, the leading reciprocal license, talks about distribution of GPL
applications with proprietary code linked in. In a literal sense we may
still have running processes with mixed code linked in. Instead of
addressing the problem at the point of linking we are disabling use of
incompatible components at runtime. There is some small risk that this
may be considered not to be compliant with the requirements of the GPL
license in a literal sense, though it is clear we are making every
reasonable effort to enforce it in a practical sense.

In the situation of standalone software packages being distributed with
GDAL, it may still be best for those preparing the package to completely
omit any components incompatible with the license of the applications.
This RFC is primarily intended to support complex mixed-component
distributions such as OSGeo4W.

Drivers Affected
----------------

I believe the following drivers should be marked as "PROPRIETARY":

-  ECW
-  JP2ECW
-  MRSID
-  JP2MRSID
-  MG4Lidar
-  GEORASTER
-  JP2KAK
-  JPIPKAK
-  ArcObjects
-  OCI
-  FileGDB
-  FME
-  ArcSDE (raster and vector)

I believe the following drivers should be marked as "RECIPROCAL":

-  grass (raster and vector)
-  EPSILON
-  MySQL (depending on active license terms!)
-  PDF

Unresolved:

-  The OGR SOSI driver should probably be marked as proprietary
   currently as it relies on linking with binary objects with unknown
   licencing terms, even if apparently the ultimate goal seems to open
   source them.
-  I'm a bit confused by :ref:`raster.msg`.
   Seems that it relies on third party stuff with both proprietary and
   GPL code.
-  I am unsure about the ODBC based drivers. I suppose PGEO and
   MSSQLSPATIAL drivers ought to be marked proprietary too? Might it
   depend on the actual license terms of the odbc library?

Please let me know of other drivers needing marking.

SWIG Bindings
-------------

Some (all?) swig bindings automatically call GDALAllRegister() and/or
OGRRegisterAll() at the point the bindings are loaded making it hard to
set the application level GDAL_LICENSE_POLICY in a script before the
registration takes place. To address that I believe we should expose the
AutoSkipDrivers() methods via SWIG so that scripts can set the policy
and then "clean" the drivers based on the policy in force.

Test Suite
----------

How to test?

Documentation
-------------

How to document?

Implementation
--------------

Frank Warmerdam will do the core implementation in trunk. Driver
maintainers may need to update the metadata for particular drivers.
