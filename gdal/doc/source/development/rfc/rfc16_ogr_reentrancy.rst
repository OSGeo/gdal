.. _rfc-16:

================================================================================
RFC 16: OGR Thread Safety
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Development

Summary
-------

In an effort to better support thread safety in OGR some methods are
added as internal infrastructure is updated.

Definitions
-----------

*Reentrant*: A reentrant function can be called simultaneously by
multiple threads provided that each invocation of the function
references unique data.

*Thread-safe*: A thread-safe function can be called simultaneously by
multiple threads when each invocation references shared data. All access
to the shared data is serialized.

Objective
---------

To make all of the OGR core and selected drivers reentrant, and to make
the driver registrar, drivers and datasources at least potentially
thread-safe.

TestCapability()
----------------

The TestCapability() method on the driver, and datasource will be
extended to include ways of testing for reentrancy and thread safety on
particular instances. The following macros will be added:

::

   #define OLCReentrant    "Reentrant"
   #define ODsCLayerClones "LayerClones"
   #define ODsCReentrant   "Reentrant"
   #define ODsCThreadSafe  "Threadsafe"

Meaning:

-  OLCReentrant: The layer class is reentrant. Multiple threads can
   operate on distinct instances of this class - including different
   layers on a single datasource.
-  ODsCReentrant: The datasource class is reentrant. Multiple threads
   can operate on distinct instances of this class.
-  ODsCThreadSafe: The datasource class is thread-safe. Multiple threads
   can operate on a single instance of this class.
-  ODsCLayerClones: The OGRDataSource::GetLayerClone() method is
   supported, and returns a layer instance with distinct state from the
   default layer returned by GetLayer().

Note that a single layer instance cannot be threadsafe as long as layer
feature reading status is implicit in the layer object. The default
return value for all test values is FALSE, as is normal for the
TestCapability() method, but specific drivers can return TRUE after
determining that the driver datasources or layers are in fact reentrant
and/or threadsafe.

OGRSFDriverRegistrar
--------------------

Various changes have already been made to make the driver registrar
thread safe, primarily by protecting operations on it with a mutex.

OGRSFDriver
-----------

No changes are required to the OGRSFDriver base class for thread safety,
primarily because it does almost nothing.

OGRDataSource
-------------

This class has been modified to include an m_hMutex class data member
which is a mutex used to ensure thread safe access to internal
datastructures such as the layer list. Classes derived from
OGRDataSource that wish to implement threadsafe operation should use
this mutex when exclusivity is required.

A new method is added to this class:

::

     OGRLayer *GetLayerClone( int i );

The default implementation of this method returns NULL. If the
ODsCLayerClones capability is true for the datasource, this method must
return duplicates of the requested layer that have distinct feature
reading state. That is they can have their own spatial and attribute
filter settings, and the internal feature iterator (for GetNextFeature()
and ResetReading()) is distinct from other OGRLayer instances
referencing the same underlying datasource layer.

The intention of this method in the multi-threaded context is that
different threads can have clones of a layer with distinct read state. A
sort of poor-mans threadsafety, even though in fact it is just
reentrancy.

Layers return by GetLayerClone() should be released with the
OGRDataSource::ReleaseResultSet() method, much like layers returned by
ExecuteSQL().

ExecuteSQL()
------------

The default OGR implementation of OGRDataSource::ExecuteSQL() internally
uses and modifies the layer state (feature iterators and filters) and as
such is not appropriate to use on a datasource that is attempting to be
threadsafe even though it is understood that individual layers are not
threadsafe.

The proposed solution is that this code will be modified to use
GetLayerClone() if the datasource supports GetLayerClone().

Testing
-------

A multi-threaded C++ test harnass will be implemented for read-only
stress testing of datasources claiming to support reentrancy and
threadsafety.

No testing of reentrancy and threadsafety will be incorporated into the
regression test suite (gdalautotest) as it does not appear to be
practical.

Implementation
--------------

Frank Warmerdam will implement all the core features of this RFC for the
GDAL/OGR 1.5.0 release. As well the Shapefile, Personal Geodatabase,
ODBC and Oracle drivers will implement OLCReentrant, ODsCLayerClones,
ODsCReentrant and ODsThreadSafe.
