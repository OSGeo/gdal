.. _multithreading:

===============
Multi-threading
===============

GDAL API: re-entrant, but not thread-safe
-----------------------------------------

The exact meaning of the terms ``thread-safe`` or ``re-entrant`` is not fully
standardized. We will use here the `QT definitions <https://doc.qt.io/qt-5/threads-reentrancy.html>`__.
In particular, a C function or C++ method is said to be re-entrant if it can
be called simultaneously from multiple threads, *but* only if each invocation
uses its own data.

All GDAL public C functions and C++ methods are re-entrant, except:

- the general initialization functions, like :cpp:func:`GDALAllRegister`.
- the general cleanup functions like :cpp:func:`GDALDestroy` or :cpp:func:`OSRCleanup`.

Those functions should not be called concurrently from several threads, and it
is general best practice to call them from the main thread of the program at
program initialization and termination.

Unless otherwise stated, no GDAL public C functions and C++ methods should be
assumed to be thread-safe. That is you should not call simultaneously GDAL
functions from multiple threads on the same data instance, or even instances
that are closely related through ownership relationships. For example, for a
multi-band raster dataset, it is not safe to call concurrently GDAL functions
on different :cpp:class:`GDALRasterBand` instances owned by the same
:cpp:class:`GDALDataset` instance (each thread should instead manipulate a
distinct GDALDataset). Similarly for a GDALDataset owning several :cpp:class:`OGRLayer`.

The reason is that most implementations of GDALDataset or GDALRasterBand
are stateful. A GDALDataset typically owns a file handle,
and performs seek/read operations on it, thus not allowing concurrent access.
Block cache related structures for a given GDALDataset are not thread-safe.
Drivers also often implement lazy initialization strategies to access various
metadata which are resolved only the first time the method to access them is
invoked. Drivers may also rely on third-party libraries that expose objects
that are not thread-safe.

Those restrictions apply to the C and C++ ABI, and all languages bindings (unless
they would take special precautions to serialize calls)

GDAL block cache and multi-threading
------------------------------------

The current design of the GDAL raster block cache allows concurrent reads of several datasets. However performance issues may
arise when writing several datasets from several threads, due to lock contention
in the global structures of the block cache mechanism.

RAM fragmentation and multi-threading
-------------------------------------

It has been observed that scenarios that involve multi-threading reading or
writing of raster datasets are prone to cause a high RAM usage, in particular
when using the default dynamic memory allocator of Linux. Using the alternate
`tcmalloc <https://github.com/google/tcmalloc>`__ memory allocator helps
reducing the amount of virtual and resident memory used.

For example, with Debian/Ubuntu distributions, this can be done by
installing the ``libtcmalloc-minimal4`` package and running the binary that
executes GDAL with:

::

    LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc_minimal.so.4 ./binary

GDAL and multi-processing
-------------------------

POSIX fork() API should not be called during the middle of a GDAL operation,
otherwise some structures like mutexes might appear to be locked forever in the
forked process. If multi-processing is done, we recommend that processes are
forked before any GDAL operation is done. Operating on the same GDALDataset
instance in several sub-processes will generally lead to wrong results due to
the underlying file descriptors being shared.
