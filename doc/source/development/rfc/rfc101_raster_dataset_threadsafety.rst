.. _rfc-101:

===================================================================
RFC 101: Raster dataset read-only thread-safety
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2024-Aug-29
Status:        Adopted, implemented
Target:        GDAL 3.10
============== =============================================

Summary
-------

This RFC enables users to get instances of :cpp:class:`GDALDataset`
(and their related objects such as :cpp:class:`GDALRasterBand`) that are
thread-safe for read-only raster operations, that is such instances can
be safely used from multiple threads without locking.

Terminology
-----------

The exact meaning of the terms ``thread-safe`` or ``re-entrant`` is not fully
standardized. We will use here the `QT definitions <https://doc.qt.io/qt-5/threads-reentrancy.html>`__.
In particular, a C function or C++ method is said to be re-entrant if it can
be called simultaneously from multiple threads, *but* only if each invocation
uses its own data/instance. On the contrary, it is thread-safe is if can be
called on the same data/instance (so thread-safe is stronger than re-entrant)

Motivation
----------

A number of raster algorithms can be designed to read chunks of a raster in
an independent and concurrent way, with resulting speed-ups when using
multi-threading. Currently, given a GDALDataset instance is not thread-safe,
this requires either to deal with I/O in a single thread, or through a mutex
to protect against concurrent use, or one needs to open a separate GDALDataset
for each worker thread. Both approaches can complicate the writing of such
algorithms. The enhancement of this RFC aims at providing a special GDALDataset
instance that can be used safely from multiple threads. Internally, it does use
one GDALDataset per thread, but hides this implementation detail to the user.

C and C++ API extensions
------------------------

A new ``GDAL_OF_THREAD_SAFE`` opening flag is added to be specified to
:cpp:func:`GDALOpenEx` / :cpp:func:`GDALDataset::Open`. This flag is for now
mutually exclusive with ``GDAL_OF_VECTOR``, ``GDAL_OF_MULTIDIM_RASTER`` and
``GDAL_OF_UPDATE``. That is this flag is only meant for read-only raster
operations (``GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE``).

To know if a given dataset can be used in a thread-safe way, the following
C++ method is added to the GDALDataset class:

.. code-block:: c++

    /** Return whether this dataset, and its related objects (typically raster
     * bands), can be called for the intended scope.
     *
     * Note that in the current implementation, nScopeFlags should be set to
     * GDAL_OF_RASTER, as thread-safety is limited to read-only operations and
     * excludes operations on vector layers (OGRLayer) or multidimensional API
     * (GDALGroup, GDALMDArray, etc.)
     *
     * This is the same as the C function GDALDatasetIsThreadSafe().
     *
     * @since 3.10
     */
    bool IsThreadSafe(int nScopeFlags) const;


The corresponding C function is added:

.. code-block:: c

    bool GDALDatasetIsThreadSafe(GDALDatasetH hDS, int nScopeFlags,
                                 CSLConstList papszOptions);


A new C++ function, GDALGetThreadSafeDataset, is added with two forms:

.. code-block:: c++

    std::unique_ptr<GDALDataset> GDALGetThreadSafeDataset(std::unique_ptr<GDALDataset> poDS, int nScopeFlags);

    GDALDataset* GDALGetThreadSafeDataset(GDALDataset* poDS, int nScopeFlags);

This function accepts a (generally non thread-safe) source dataset and return
a new dataset that is a thread-safe wrapper around it, or the source dataset if
it is already thread-safe.
The nScopeFlags argument must be compulsory set to GDAL_OF_RASTER to express that
the intended scope is read-only raster operations (other values will result in
an error and a NULL returned dataset).
This function is used internally by GDALOpenEx() when the GDAL_OF_THREAD_SAFE
flag is passed to wrap the dataset returned by the driver.
The first form takes ownership of the source dataset. The second form does not,
but references it internally, and assumes that its lifetime will be longer than
the lifetime of the returned thread-safe dataset. Note that the second form does
increase the reference count on the passed dataset while it is being used, so
patterns like the following one are valid:

.. code-block:: c++

   auto poDS = GDALDataset::Open(...);
   GDALDataset* poThreadSafeDS = GDALGetThreadSafeDataset(poDS, GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE);
   poDS->ReleaseRef(); // can be done here, or any time later
   if (poThreadSafeDS )
   {
       // ... do something with poThreadSafeDS ...
       poThreadSafeDS->ReleaseRef();
   }


For proper working both when a new dataset is returned or the passed one if it
is already thread-safe, :cpp:func:`GDALDataset::ReleaseRef()` (and not delete or
GDALClose()) must be called on the returned dataset.


The corresponding C function for the second form is added:

.. code-block:: c

    GDALDatasetH GDALGetThreadSafeDataset(GDALDatasetH hDS, int nScopeFlags, CSLConstList papszOptions);


Usage examples
--------------

Example of a function processing a whole dataset passed as a filename:

.. code-block:: c++

    void foo(const char* pszFilename)
    {
        auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            pszFilename, GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE | GDAL_OF_VERBOSE_ERROR));
        if( !poDS )
        {
            return;
        }

        // TODO: spawn threads using poDS
    }


Example of a function processing a whole dataset passed as an object:

.. code-block:: c++

    void foo(GDALDataset* poDS)
    {
        GDALDataset* poThreadSafeDS = GDALGetThreadSafeDataset(poDS, GDAL_OF_RASTER);
        if( poThreadSafeDS )
        {
            // TODO: spawn threads using poThreadSafeDS

            poThreadSafeDS->ReleaseRef();
        }
        else
        {
            // TODO: Serial version of the algorithm. It can happen if
            // poDS is a on-the-fly dataset.
        }
    }


Example of a function processing a single band passed as an object:

.. code-block:: c++

    void foo(GDALRasterBand* poBand)
    {
        GDALDataset* poThreadSafeDS = nullptr;
        GDALRasterBand* poThreadSafeBand = nullptr;
        GDALDataset* poDS = poBand->GetDataset();
        // Check that poBand has a matching owing dataset
        if( poDS && poDS->GetRasterBand(poBand->GetBand()) == poBand )
        {
            poThreadSafeDS = GDALGetThreadSafeDataset(poDS, GDAL_OF_RASTER);
            if( poThreadSafeDS )
                poThreadSafeBand = poThreadSafeDS->GetBand(poBand->GetBand());
        }

        if( poThreadSafeBand )
        {
            // TODO: spawn threads using poThreadSafeBand

            poThreadSafeDS->ReleaseRef();
        }
        else
        {
            // TODO: Serial version of the algorithm. It can happen if
            // poBand is a on-the-fly band, or a "detached" band, such as a
            // mask band, or an overview band as returned by some drivers.
        }
    }


SWIG bindings
-------------

The new C macro and functions are bound to SWIG as:

- ``gdal.OF_THREAD_SAFE``
- :py:func:`Dataset.IsThreadSafe(nScopeFlags)`
- :py:func:`Dataset.GetThreadSafeDataset(nScopeFlags)`. The Python
  implementation of this method takes care of keeping a reference on the source
  dataset in the returned thread-safe dataset, so the user does not have to
  care about their respective lifetimes.

Usage and design limitations
----------------------------

* As implied by the RFC title, the scope of thread-safety is restricted to
  **raster** and **read-only** operations.

* For GDALDataset instances pointing to a file on the regular filesystem, the
  limitation of the maximum number of file descriptor opened by a process
  (1024 on most Linux systems) could be hit if working with a sufficiently large
  number of worker threads and/or instances of GDALThreadSafeDataset.

* The generic implementation of GDALGetThreadSafeDataset assumes that the
  source dataset can be re-opened by its name (GetDescription()), which is the
  case for datasets opened by GDALOpenEx(). A special implementation is also
  made for dataset instances of the MEM driver. But, there is currently no
  support for creating a thread-safe dataset wrapper on on-the-fly datasets
  returned by some algorithms (e.g GDALTranslate() or GDALWarp() with VRT as
  the output driver and with an empty filename, or custom GDALDataset
  implementation by external code).

* Inherent to the selected approach, there is a band block cache per thread, and
  thus no sharing of cached blocks between threads.
  However, this should not be a too severe limitation for algorithms where
  threads process independent regions of the raster, hence reuse of cached blocks
  would be non-existent or low. Optimal algorithms will make sure to work on
  regions of interest aligned on the block size (this advice also applies for
  the current approach of manually opening a dataset for each worker thread).

* Due to implementation limitations, :cpp:func:`GDALRasterBand::GetDefaultRAT`
  on a GDALThreadSafeDataset instance only works if the RAT is an instance of
  :cpp:class:`GDALDefaultRasterAttributeTable`. An error is emitted if
  this is not the case. This could potentially be extended to work with any
  subclass of :cpp:class:`GDALRasterAttributeTable` but with significant
  additional coding to create a thread-safe wrapper. (GDALDefaultRasterAttributeTable
  is intrinsically thread-safe for read-only operations). This is not perceived
  as a limitation for the intended use cases of this RFC (reading pixel values
  in parallel).

* Some drivers, like netCDF, and HDF5 in some builds, use a global lock around
  each call to their APIs, due to the underlying libraries not being re-entrant.
  Obviously scalability of GDALThreadSafeDataset will be limited by such global
  lock.
  But this is no different than the approach of opening as many dataset as
  worker threads.

Implementation details
----------------------

(This section is mostly of interest for developers familiar with GDAL internals
and may be skipped by users of the GDAL API)

The gist of the implementation lies in a new file ``gcore/gdalthreadsafedataset.cpp``
which defines several classes (internal details):

- ``GDALThreadSafeDataset`` extending :cpp:class:`GDALProxyDataset`.
  Instances of that class are returned by GDALGetThreadSafeDataset().
  On instantiation, it creates as many GDALThreadSafeRasterBand instances as
  the number of bands of the source dataset.
  All virtual methods of GDALDataset are redefined by GDALProxyDataset.
  GDALThreadSafeDataset overloads its ReferenceUnderlyingDataset method, so that
  a thread-local dataset is opened the first-time a thread calls a method on
  the GDALThreadSafeDataset instance, cached for later use, and method call is
  generally forwarded to it. There are exceptions for methods like
  :cpp:func:`GDALDataset::GetSpatialRef`, :cpp:func:`GDALDataset::GetGCPSpatialRef`,
  :cpp:func:`GDALDataset::GetGCPs`, :cpp:func:`GDALDataset::GetMetadata`,
  :cpp:func:`GDALDataset::GetMetadataItem` that return non-primitive types
  where the calls are forwarded to the dataset used to construct GDALThreadSafeDataset,
  with a mutex being taken around them. If the call was otherwise forwarded to
  a thread-local instance, there would be a risk of use-after-free situations
  when the returned value is used by different threads.

- ``GDALThreadSafeRasterBand`` extending :cpp:class:`GDALProxyRasterBand`.
  On instantiation, it creates child GDALThreadSafeRasterBand instances for
  band mask and overviews.
  Its ReferenceUnderlyingRasterBand method calls ReferenceUnderlyingDataset
  on the GDALThreadSafeDataset instance to get a thread-local dataset, fetches
  the appropriate thread-local band and generally forwards its the method call.
  There are exceptions for methods like
  :cpp:func:`GDALRasterBand::GetUnitType`, :cpp:func:`GDALRasterBand::GetMetadata`,
  :cpp:func:`GDALRasterBand::GetMetadataItem` that return non-primitive types where
  the calls are forwarded to the band used to construct GDALThreadSafeRasterBand,
  with a mutex being taken around them, and the returned value being .

- ``GDALThreadLocalDatasetCache``. Instances of that class use thread-local
  storage. The main member of such instances is a LRU cache that maps
  GDALThreadSafeDataset* instances to a thread specific GDALDataset smart pointer.
  On GDALThreadSafeDataset destruction, there's code to iterate over all
  alive GDALThreadLocalDatasetCache instances and evict no-longer needed entries
  in them, within a per-GDALThreadLocalDatasetCache instance mutex, to avoid
  issues when dealing with several instances of GDALThreadLocalDatasetCache...
  Note that the existence of this mutex should not cause performance issues, since
  contention on it, should be very low in real-world use cases (it could become
  a bottleneck if GDALThreadSafeDataset were created and destroyed at a very
  high pace)

Two protected virtual methods are added to GDALDataset for GDALThreadSafeDataset
implementation, and may be overloaded by drivers if needed (but it is not
anticipated that drivers but the MEM driver need to do that)

- ``bool CanBeCloned(int nScopeFlags, bool bCanShareState) const``.
  This method determines if a source dataset can be "cloned" (or re-opened).
  It returns true for instances returned by GDALOpenEx, for instances of the MEM
  driver if ``nScopeFlags`` == ``GDAL_OF_RASTER`` (and ``bCanShareState`` is
  true for instances of the MEM driver)

- ``std::unique_ptr<GDALDataset> Clone(int nScopeFlags, bool bCanShareState) const``.
  This method returns a "clone" of the dataset on which it is called, and is used
  by GDALThreadSafeDataset::ReferenceUnderlyingDataset() when a thread-local
  dataset is needed.
  Implementation of that method must be thread-safe.
  The base implementation calls GDALOpenEx() reusing the dataset name, open flags
  and open option. It is overloaded in the MEM driver to return a new instance
  of MEMDataset, but sharing the memory buffers with the source dataset.

No code in drivers, but the MEM driver, is modified by the candidate
implementation.

A few existing non-virtual methods of GDALDataset and GDALRasterBand have been
made virtual (and overloaded by GDALProxyDataset and GDALProxyRasterBand),
to avoid modifying state on the GDALThreadSafeRasterBand instance, which
wouldn't be thread-safe.

- :cpp:func:`GDALDataset::BlockBasedRasterIO`:
  it interacts with the block cache
- :cpp:func:`GDALRasterBand::GetLockedBlockRef`:
  it interacts with the block cache
- :cpp:func:`GDALRasterBand::TryGetLockedBlockRef`:
  it interacts with the block cache
- :cpp:func:`GDALRasterBand::FlushBlock`:
  it interacts with the block cache
- :cpp:func:`GDALRasterBand::InterpolateAtPoint`:
  it uses a per-band cache
- :cpp:func:`GDALRasterBand::EnablePixelTypeSignedByteWarning`: it should
  already have been made virtual for GDALProxyRasterBand needs.

Non-virtual methods :cpp:func:`GDALDataset::GetProjectionRef` and
:cpp:func:`GDALDataset::GetGCPProjection`, which cache the return value, have
been modify to apply a mutex when run on a dataset that IsThreadSafe() to be
effectively thread-safe.

A SetThreadSafe() method has been added to :cpp:class:`OGRSpatialReference`.
When it is called, all methods of that class run under a per-instance (recursive)
mutex. This is used by GDALThreadSafeDataset for its implementation of the
:cpp:func:`GDALDataset::GetSpatialRef` and :cpp:func:`GDALDataset::GetGCPSpatialRef`
methods, such that the returned OGRSpatialReference instances are thread-safe.

Performance
-----------

The existing multireadtest utility that reads a dataset from multiple threads
has been extended with a -thread_safe flag to asks to use GDAL_OF_THREAD_SAFE
when opening the dataset in the main thread and use it in the worker threads,
instead of the default behavior of opening explicitly a dataset in each thread.
The thread-safe mode shows similar scalability as the default mode, sometimes
with a slightly decreased efficiency, but not in a too problematic way.

For example on a 20x20 raster:

.. code-block:: shell

    $ time multireadtest -t 4 -i 1000000 20x20.tif
    real    0m2.084s
    user    0m8.155s
    sys     0m0.020s

    vs

    $ time multireadtest -thread_safe -t 4 -i 1000000 20x20.tif
    real    0m2.387s
    user    0m9.334s
    sys     0m0.029s


But on a 4096x4096 raster with a number of iterations reduced to 100, the
timings between the default and thread_safe modes are very similar.

A Python equivalent of multireadtest has been written. Scalability depends
on how much Python code is executed. If relatively few long-enough calls to
GDAL are done, scalability tends to be good due to the Python Global Interpreter
Lock (GIL) being dropped around them. If many short calls are done, the GIL
itself, or its repeated acquisition and release, becomes the bottleneck. This is
no different than using a GDALDataset per thread.

Documentation
-------------

Documentation for the new constant and functions will be added. The
:ref:`multithreading` page will be updated to reflect the new capability
introduced by this RFC.

Backward compatibility
----------------------

No issue anticipated: the C and C++ API are extended.
The C++ ABI is modified due to additions of new virtual methods.

Testing
-------

Tests will be added for the new functionality, including stress tests to have
sufficiently high confidence in the correctness of the implementation for common
use cases.

Risks
-----

Like all code related to multi-threading, the C++ language and tooling offers
hardly any safety belt against thread-safety programming errors. So it cannot
be excluded that the implementation suffers from  bugs in some edge scenarios,
or in the usage of some methods of GDALDataset, GDALRasterBand and related objects
(particularly existing non-virtual methods of those classes that could happen
to have a non thread-safe implementation)

Design discussion
-----------------

This paragraph discusses a number of thoughts that arose during the writing of
this RFC.

1.  A significantly different alternative could have consisted in adding native
    thread-safety in each driver. But this is not realistic for the following reasons:

    * if that was feasible, it would require considerable development effort to
      rework each drivers. So realistically, only a few select drivers would be updated.

    * Even updating a reduced number of drivers would be extremely difficult, in
      particular the GeoTIFF one, due to the underlying library not being reentrant,
      and deferred loading strategies and many state variables being modified even
      by read-only APIs. And this applies to most typical drivers.

    * Due to the inevitable locks, there would be a (small) cost bore by callers
      even on single-thread uses of thread-safe native drivers.

    * Some core mechanisms, particularly around the per-band block cache structures,
      are not currently thread-safe.

2.  A variant of the proposed implementation that did not use thread-local storage
    has been initially attempted. It stored instead a
    ``std::map<thread_id, std::unique_ptr<GDALDataset>>`` on each GDALThreadSafeDataset
    instance. This implementation was simpler, but unfortunately suffered from high
    lock contention since a mutex had to be taken around each access to this map,
    with the contention increasing with the number of concurrent threads.

3.  For the unusual situations where a dataset cannot be reopened and thus
    GDALGetThreadSafeDataset() fails, should we provide an additional ``bForce``
    argument to force it to still return a dataset, where calls to the wrapped
    dataset are protected by a mutex? This would enable to always write multi-thread
    safe code, even if the access to the dataset is serialized.
    Similarly we could have a
    ``std::unique_ptr<GDALRasterBand> GDALGetThreadSafeRasterBand(GDALRasterBand* poBand, int nOpenFlags, bool bForce)``
    function that would try to use GDALGetThreadSafeDataset() internally if it
    manages to identify the dataset to which the band belongs to, and otherwise would
    fallback to protecting calls to the wrapped band with a mutex.

    Given the absence of evidence that such option is necessary, this has been excluded
    from the scope of this RFC.


Related issues and PRs
----------------------

- Candidate implementation: https://github.com/OSGeo/gdal/pull/10746

- https://github.com/OSGeo/gdal/issues/8448: GTiff: Allow concurrent reading of single blocks

Voting history
--------------

+1 from PSC members KurtS, JukkaR, JavierJS and EvenR
