.. _rfc-9:

===============================================
RFC 9: GDAL Paid Maintainer Guidelines
===============================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Approved, but superseded per :ref:`rfc-83`

Purpose
-------

To formalize guidelines for the work of maintainers paid out of GDAL
project sponsorship funds.

Responsibilities
----------------

1. Analyse and where possible fix bugs reported against GDAL.
2. Run, review and extend the test suite (via buildbot, etc).
3. Maintain and extend documentation.
4. Assist integrating new contributed features.
5. Help maintain project infrastructure (mailing lists, buildbot, source
   control, etc)
6. Provide user support on the project mailing lists, and in other
   venues.
7. Develop new capabilities.

Bug fixing and maintenance should be focused on GDAL/OGR, but as needed
will extend into sub-projects such as libtiff, libgeotiff, Shapelib and
MITAB as long it is to serve a need of the GDAL/OGR project.

In order to provide reasonable response times the maintainer is expected
spend some time each week addressing new bugs and user support. If the
maintainer will be unavailable for an extended period of time (vacation,
etc) then the supervisor should be notified.

Direction
---------

The maintainer is generally subject to the project PSC. However, for day
to day decisions one PSC member will be designated as the supervisor for
the maintainer. This supervisor will prioritize work via email, bug
assignments, and IRC discussions.

The supervisor will try to keep the following in mind when prioritizing
tasks.

-  Bug reports, and support needs of Sponsors should be given higher
   priority than other tasks.
-  Areas of focus identified by the PSC (ie. multi-threading, SWIG
   scripting) should be given higher priority than other tasks.
-  Bugs or needs that affect many users should have higher priority.
-  The maintainer should be used to take care of work that no one else
   is willing and able to do (ie. fill the holes, rather than displacing
   volunteers)
-  Try to avoid tying up the maintainer on one big task for many weeks
   unless directed by the PSC.
-  The maintainer should not be directed to do work for which someone
   else is getting paid.

Substantial new development projects will only be taken on by the
maintainer with the direction of a PSC motion (or possibly an RFC
designating the maintainer to work on a change).

Note that the maintainer and the maintainer supervisor are subject to
the normal RFC process for any substantial change to GDAL.

Reporting
---------

The maintainer will produce a brief bi-weekly report to the gdal-dev
list indicating tasks worked on, and a more detailed timesheet for the
supervisor.

This is intended to provide visibility into status, accomplishments, and
time allocation. It also gives an opportunity for the PSC to request a
"course correction" fairly promptly.
