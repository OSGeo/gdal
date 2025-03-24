.. _rfc-108:

=====================================================================
RFC 108: Driver removals for GDAL 3.11
=====================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2025-02-18
Status:        Adopted, implemented
Target:        GDAL 3.11
============== =============================================

Summary
-------

This RFC communicates about the partial or total removal of obsolete drivers
for GDAL 3.11.

Motivation
----------

GDAL is an ever growing code base, with probably more than 50% of the code
base being used by less of 1% of users. This is isn't very sustainable, and quite
demotivating for current maintainers who know nothing/little about most drivers,
have no evidence that they are used, and regularly stumble upon them when doing
whole codebase enhancements.
The decision of which driver to removal comes from a mix of "intuition" and
community feedback from
https://docs.google.com/spreadsheets/d/12yk0p8rRK4rAYO_VwXboatWkiILAjE2Cepu32lsHhM8/edit?gid=0#gid=0

Technical solution
------------------

Fully removed drivers:

- raster:

  * JP2Lura: JPEG2000-capable driver. Was based on a proprietary SDK.
    Initial proponents of that driver no longer use it.
  * OZI OZF2/OZFX3: OZI now uses a new format v4 that has not been
    reverse engineered.
  * BLX (Magellan BLX Topo File Format). No activity in this driver from
    non-GDAL team. No obvious sign in issue tracker it is used.
  * XPM driver: Not relevant in a geospatial context. Obsolete image format.
    Users can probably use ImageMagick if support for it is needed.
  * SGI driver: same as above
  * FIT driver: No idea what it is about  (not to be confused by FITS, which
    is actively used by astronomic/planetary community)
  * DIPEx: No idea what it is about
  * CTable2 driver: obsolete PROJ.4 grid format. There was just one file ("null")
    known to use that format.
  * R (R Object Data Store (``*.rda``)): not known to be used. Not to be confused with
    the RRaster driver which is used in the R ecosystem.
  * GSAG (Golden Software ASCII) and GSBG (Golden Software 6.0 binary): likely
    obsolete. GS7B (Golden Software 7.0 binary) kept as there are signs it is
    used.
  * Rasterlite: Rasterlite 1.0 format. Obsolete/no longer maintained by the
    Spatialite crew. The SQLite driver has support for Rasterlite 2
  * BT (VTP .bt Binary Terrain Format). The VTP project seems abandoned since
    2015.
  * SDTS: obsolete
  * RDB (RIEGL Laser Measurement Systems GmbH): Not CI tested. Depends on
    proprietary SDK. Removed with agreement from RIEGL.

- vector:

  * Geoconcept export .txt: no sign it is used. Feedback from a French GIS
    forum when asking if someone still uses the driver: "It is not clear who still
    uses Geoconcept (the software) at all"
  * OGDI: Vector Product Format / DGN / VMap. All obsolete formats. The
    problematic part is that it depends on libogdi, which is an obsolete C
    code base nobody sane would want to maintain anymore.
  * SDTS: obsolete
  * SVG: only supported files from Cloudmade Vector Stream Server, which no
    longer exists. The code could probably be re-enabled to be made more generic
    and handle arbitrary SVG, if the needs arises.
  * Tiger: pre-2006 format for US Census. New datasets are in shapefile.

Write-support removal:

- raster:

  * ADRG: defense-related format. Write support was mostly to test
    the read side of the driver
  * ELAS: not sure what it is about and that this is actively used
  * PAux: not sure what it is about and that this is actively used
  * MFF: not sure what it is about and that this is actively used
  * MFF2/HKV: not sure what it is about and that this is actively used
  * LAN: old ERDAS format before HFA (.img). "raw format", does not support compression. Write support doesn't seem useful.
  * BYN: Canadian grid-related format. Write support doesn't seem useful
  * NTv2: read-support definitely useful. Write support obsolete since PROJ 7
    and use of Geodetic TIFF Grid format.
  * USGSDEM: obsolete format. There might be historic datasets to read, but
    write support doesn't seem useful
  * ISIS2: Planetary format. Obsoleted by ISIS3. ISIS2 write support not used
    by USGS.

About/only 50,000 lines of code removed.

Discussion
----------

- Q. Are we sure they are not used?
  A. No, we aren't. We could also have used a more progressive deprecation
  mechanism (like requiring a runtime configuration option to be set), but we
  don't think it is worth the extra burden. Let's see if we get massive complaints
  from users, and use git revert if needed. We add a similar phase around GDAL 3.5
  and as far as I remember, we just got a couple of remarks, but did not end up
  reviving deleted drivers.

- Q. Would a dedicated per-driver maintainer model help?
  A. Maybe, although in most of the above cases, most of those drivers are not
  problematic per-se. They are just sitting there, contributing to the
  overall weight of the library, and occasionally getting into our legs.

- Q: I don't see the logic in the above removals. That seems a bit random at times.
  A. Yes, it is. Like cognitive processes are sometimes. This is also a way
  to scare people contributing drivers to GDAL that we might scrap them when
  that becomes inconvenient.

- Q. GDAL is all about support for odd formats! Don't do this!
  A. Feel free to create a gdal-legacy-drivers project.

- Should we consider in the future adding telemetry in GDAL?
  Have looked at https://opentelemetry.io a bit. Seems a bit impressive...

Documentation
-------------

Removed drivers will be mentioned at bottom of https://gdal.org/en/latest/drivers/raster/index.html
and https://gdal.org/en/latest/drivers/vector/index.html with the mention of
the version where they have been removed. This gives a way for users that would
need them to use the appropriate past version(s).

Related issues and PRs
----------------------

* Candidate implementation: https://github.com/OSGeo/gdal/pull/11758
  (JP2Lura and OZI have already been removed in master)

Funding
-------

Funded by GDAL Sponsorship Program (GSP) (this RFC text)

Voting history
--------------

+1 from PSC members JukkaR, JavierJS, KurtS, HowardB, DanielM, and -0 from DanB.


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    rda
    VMap
    libogdi

