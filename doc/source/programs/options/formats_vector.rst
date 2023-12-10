.. option:: --formats

    List all vector formats supported by this GDAL build (read-only and
    read-write) and exit. The format support is indicated as follows:

    * ``ro`` is read-only driver
    * ``rw`` is read or write (i.e. supports :cpp:func:`GDALDriver::CreateCopy`)
    * ``rw+`` is read, write and update (i.e. supports :cpp:func:`GDALDriver::Create`)
    * A ``v`` is appended for formats supporting virtual IO (``/vsimem``, ``/vsigzip``, ``/vsizip``, etc).
    * A ``s`` is appended for formats supporting subdatasets.

    The order in which drivers are listed is the one in which they are registered,
    which determines the order in which they are successively probed when opening
    a dataset. Most of the time, this order does not matter, but in some situations,
    several drivers may recognize the same file. The ``-if`` option of some utilities
    can be specified to restrict opening the dataset with a subset of drivers (generally one).
    Note that it does not force those drivers to open the dataset. In particular,
    some drivers have requirements on file extensions.
    Alternatively, the :config:`GDAL_SKIP` configuration option can also be used
    to exclude one or several drivers.
