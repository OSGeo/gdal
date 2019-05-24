.. _rfc-42:

=======================================================================================
RFC 42: OGR Layer laundered field lookup
=======================================================================================

Author: Jürgen Fischer

Contact: jef at norbit dot de

Summary
-------

This (mini)RFC proposes a new method in the OGR layer class (and a C
API) to lookup the field index of fields, whose names have been altered
by drivers (eg. by LAUNDER in OCI or Pg).

Implementation
--------------

There is already a pull request on github
(`https://github.com/OSGeo/gdal/pull/23 <https://github.com/OSGeo/gdal/pull/23>`__)
that implements this RFC. It adds the virtual method
OGRLayer::FindFieldIndex(), that implements the usual mapping, which can
be overloaded by drivers. The OCI driver does this to optionally return
the index of the LAUNDERed field in case the original field does not
exists. The pull request also modifies ogr2ogr to make use of that
method and offers a switch -relaxedFieldNameMatch to enable it.

Background
----------

This is a particular problem when using NAS as that usually operates on
a pre-existing schema. This schema had to be adapted for Oracle as
Oracle has a identifier length restrictions that quite a number of
identifiers in NAS exceed. Hence ogr2ogr failed to make the mapping
between the short names and their long counter parts and leaves those
fields empty.

References
----------

-  `https://github.com/OSGeo/gdal/pull/23 <https://github.com/OSGeo/gdal/pull/23>`__
-  PostgreSQL NAS schema:
   `http://trac.wheregroup.com/PostNAS/browser/trunk/import/alkis_PostNAS_schema.sql <http://trac.wheregroup.com/PostNAS/browser/trunk/import/alkis_PostNAS_schema.sql>`__
-  Oracle NAS schema:
   `http://trac.wheregroup.com/PostNAS/browser/trunk/import/alkis_PostNAS_ORACLE_schema.sql <http://trac.wheregroup.com/PostNAS/browser/trunk/import/alkis_PostNAS_ORACLE_schema.sql>`__
-  conversion script from Pg to OCI:
   `http://trac.wheregroup.com/PostNAS/browser/trunk/import/pg-to-oci.pl <http://trac.wheregroup.com/PostNAS/browser/trunk/import/pg-to-oci.pl>`__

Voting history
--------------

+1 from DanielM, EvenR, FrankW, TamasS, JukkaR and jef

Commits
-------

r26572 & r26573
