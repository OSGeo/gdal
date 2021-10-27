.. _rfc-81:

=============================================================
RFC 81: Support for coordinate epochs in geospatial formats
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2021-May-10
Last Updated:  2021-Jun-18
Status:        Adopted
Target:        GDAL 3.4
============== =============================================

Summary
-------

This RFC describes support for coordinate epochs in a few key geospatial formats
and in GDAL/OGR API and utilities

Motivation
----------

A number of coordinate reference systems (CRS) are called "dynamic CRS", that
is the coordinates of a point on the surface of the Earth in those CRS may
change with time. To be unambiguous the coordinates must always be qualified
with the epoch at which they are valid. The coordinate epoch is not necessarily
the epoch at which the observation was collected.

Examples of dynamic CRS are ``WGS 84 (G1762)``, ``ITRF2014``, ``ATRF2014``.

The generic EPSG:4326 WGS 84 CRS is also considered dynamic, although it is
not recommended to use it due to being based on a datum ensemble whose positional
accuracy is 2 meters, but prefer one of its realizations, such as WGS 84 (G1762)

At time of writing, no formats handled by GDAL/OGR have a standardized way of
encoding a coordinate epoch. We consequently have made choices how to encode it,
admittedly not always elegant, with the aim of being as much as possible backward
compatible with existing readers.
Those encodings might change if corresponding official specifications
evolve to take this concept into account. But, as this is a bit of a chicken-and-egg
problem ("why should we care about storing coordinate epoch if no software can make use
of it ?"), let's start with this initial solution.

PROJ can handle a number of time-dependent transformations between static CRS
and dynamic CRS, e.g a GDA2020 (static CRS/datum for Australia) to ATRF2014
(dynamic CRS/datum for Australia), taking into account plate motion. Having
support to store coordinate epoch will make it easier to have more accurate
coordinate transformation.

Details
-------

See https://github.com/rouault/gdal/blob/coordinate_epoch_v2/gdal/doc/source/user/coordinate_epoch.rst
for impacts on the API, encoding of CRS in a number of formats, and impacts on
existing utilities.

Backward compatibility
----------------------

At the API level, only additions.

Regarding creation of new datasets, no backward incompatibility at all if
datasets are created without a coordinate epoch associated to their CRS, as its
encoding is only added when needed.

And when it is used, it is done in a way that shouldn't affect existing reader.
The only exception would be the FlatGeobuf format if writing a non-EPSG coded
CRS, and with a coordinate epoch (a backport to the 3.3 branch will be done to avoid
an error in that case).

Documentation
-------------

New methods are documented, and the page mentioned in the Details paragraph
will be part of the user documentation.

Testing
-------

New methods are tested. Formats extended with coordinate epoch support have
also received new tests.

Related PRs:
-------------

https://github.com/OSGeo/gdal/pull/4011

Voting history
--------------

+1 from JukkaR and EvenR, -0 from HowardB and +0 from KurtS
