.. _rfc-107:

=====================================================================
RFC 107: Add OGRLayer::IGetExtent() and OGRLayer::ISetSpatialFilter()
=====================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2025-02-06
Status:        Adopted, implemented
Target:        GDAL 3.11
============== =============================================

Summary
-------

This RFC changes the prototype of the OGRLayer::GetExtent(), GetExtent3D(),
SetSpatialFilter() and SetSpatialFilterRect() methods.

Motivation
----------

Originally GetExtent(), SetSpatialFilter() and SetSpatialFilterRect() were
designed for a single geometry field. When support for multiple geometry fields
was added per :ref:`rfc-41`, alternate virtual methods were added to accept a
``int iGeomField`` argument, but this causes slightly repeating code patterns
in most drivers, and omissions of the boilerplate can cause bugs.
This RFC proceeds to minor changes to improve the code and reduce slightly its
amount by 900 lines.

Technical solution
------------------

The ``OGRErr GetExtent(OGREnvelope*, int bForce = TRUE)`` and
``OGRErr GetExtent(int iGeomField, OGREnvelope*, int bForce = TRUE)``
methods are changed to no longer be virtual. Furthermore they are modified to
accept a ``bool`` type for the ``bForce`` argument. Those non-virtual methods
validate the value of ``iGeomField`` and delegate the work to a
``OGRErr IGetExtent(int iGeomField, OGREnvelope*, bool bForce)`` virtual method
(note the leading ``I`` for interface, consistently with practice for similar
situations), which has a default implementation in OGRLayer and is
overridden in drivers that support a more efficient implementation. Overridden
implementations must take care to call the base IGetExtent() method, and not
GetExtent() (otherwise infinite recursion would occur).

The same change is done for ``GetExtent3D``, made non-virtual, and with the addition
of a virtual ``IGetExtent3D`` method.

Similarly, the ``void SetSpatialFilter(OGRGeometry*)``, ``void SetSpatialFilter(int iGeomField, OGRGeometry*)``,
``void SetSpatialFilterRect(double, double, double, double)`` and
``void SetSpatialFilterRect(int iGeomField, double, double, double, double)``
methods which used to be virtual, are changed to no longer be virtual, and their
return type is changed to be ``OGRErr``. They
validate the value of ``iGeomField`` and delegate the work to a new
``OGRErr ISetSpatialFilter(int iGeomField, const OGRGeometry*)`` virtual method,
which has a default implementation in OGRLayer and is
overridden in drivers that support a more efficient implementation
Note that this method returns a ``OGRErr`` to allow drivers to report errors
(currently they can only emit a CPLError(CE_Failure, ...) message), the leading ``I``
in the method name, and the ``OGRGeometry*`` argument being made const.
Overridden implementations must take care to call the base ISetSpatialFilter()
method, and not SetSpatialFilter() (otherwise infinite recursion would occur).

Impact on drivers
-----------------

All drivers that implement those methods must be updated.

Backward compatibility
----------------------

- No impact for the C API
- Little impact for the user-facing C++ API, which should cause little to no
  changes in external C++ code. The bForce argument of GetExtent/GetExtent3D
  is now a ``bool`` instead of a ``int``. SetSpatialFilter/SetSpatialFilterRect
  now return ``OGRErr`` instead of ``void``.
- Out-of-tree drivers (in particular the GDAL-GRASS plugin) must adapt for the
  above changes, also mentioned in MIGRATION_GUIDE.TXT.

Testing
-------

No specific new tests. Minor adaptions to existing ones.

Documentation
-------------

- The 2 new virtual methods receive Doxygen documentation.
- New paragraphs are added in MIGRATION_GUIDE.TXT

Related issues and PRs
----------------------

* Candidate implementation: https://github.com/OSGeo/gdal/pull/11813

Funding
-------

Funded by GDAL Sponsorship Program (GSP)

Voting history
--------------

+1 from PSC members HowardB, JavierJS, DanB and EvenR
