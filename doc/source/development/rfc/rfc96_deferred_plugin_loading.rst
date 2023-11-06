.. _rfc-96:

==================================================================
RFC 96: Deferred in-tree C++ plugin loading
==================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2023-Nov-01
Status:        In development
Target:        GDAL 3.9
============== =============================================

Summary
-------

This RFC adds a mechanism to defer the loading of in-tree C++ plugin drivers to
the point where their executable code is actually needed, and converts a number
of relevant drivers to use that mechanism. The aim is to allow for more modular
GDAL builds, while improving the performance of plugin loading.

Context and motivation
----------------------

There are currently two ways of loading a GDAL C++ driver:

- embedded in the core libgdal library. This is the default behavior
  for drivers in the official GDAL source repository.

- available in a shared library (.so, .dll, .dylib) in a directory where it
  is dynamically loaded when GDALAllRegister() is called. This is what
  out-of-tree drivers use, or in-tree drivers if enabling the
  [GDAL|OGR]_ENABLE_DRIVER_FOO_PLUGIN=ON or GDAL_ENABLE_PLUGINS=ON
  CMake options (cf https://gdal.org/development/building_from_source.html#build-drivers-as-plugins)
  to build them as plugins.

For packagers/distributors, the second option is convenient for in-tree drivers
that depend on external libraries that are big and/or have a big number of
dependencies (libparquet, etc) and that would substantially increase the size of
the core libgdal library, or which have licenses more restrictive than the MIT
license used by libgdal.

However, there is a penalty at GDALAllRegister() time. For example, on Linux,
it takes ~ 300 ms to complete for a build with 126 plugins, which is a substantial
time for short lived GDAL-based processes (for example a script which would run
gdalinfo or ogrinfo on many files). This time is entirely spent in the dlopen()
method of the operating system and there is, to the best of our knowledge,
nothing we can do reduce it... besides limiting the amount of dynamic loading
(attempts have been made to load plugins in parallel in multiple threads, but
this does not improve total loading time)
For developers, that plugin loading phase is actually considerably slower, of
the order of ten seconds or more, when debugging GDAL under GDB with many plugin
drivers.

Furthermore, loading drivers that are not needed also involves some
startup/teardown code of external libraries to be run, as well as more virtual
memory to be consumed. Hence this proposal of deferring the actual loading of
the shared library of the plugins until it is really needed.

Design constraints
------------------

We want the new mechanism to be opt-in and fully backwards compatible:

- to still allow out-of-tree drivers.

- to still allow in-tree drivers, that are compatible of being built as plugins,
  to be built in libgdal core, or as plugins depending on the CMake variables.

- to progressively convert existing in-tree drivers to use it.

Details
-------

The main idea if that drivers using the new capability will register a proxy
driver (of type GDALPluginDriverProxy, or extending it) with a new
GDALDriverManager::DeclareDeferredPluginDriver() method.

.. code-block:: cpp

    /** Proxy for a plugin driver.
     *
     * Such proxy must be registered with
     * GDALDriverManager::DeclareDeferredPluginDriver().
     *
     * If the real driver defines any of the following metadata items, the
     * proxy driver should also define them with the same value:
     * <ul>
     * <li>GDAL_DMD_LONGNAME</li>
     * <li>GDAL_DMD_EXTENSIONS</li>
     * <li>GDAL_DMD_EXTENSION</li>
     * <li>GDAL_DCAP_RASTER</li>
     * <li>GDAL_DCAP_MULTIDIM_RASTER</li>
     * <li>GDAL_DCAP_VECTOR</li>
     * <li>GDAL_DCAP_GNM</li>
     * <li>GDAL_DMD_OPENOPTIONLIST</li>
     * <li>GDAL_DMD_SUBDATASETS</li>
     * <li>GDAL_DCAP_MULTIPLE_VECTOR_LAYERS</li>
     * <li>GDAL_DCAP_NONSPATIAL</li>
     * </ul>
     *
     * The pfnIdentify and pfnGetSubdatasetInfoFunc callbacks, if they are
     * defined in the real driver, should also be set on the proxy driver.
     *
     * Furthermore, the following metadata items must be defined if the real
     * driver sets the corresponding callback:
     * <ul>
     * <li>GDAL_DCAP_OPEN: must be set to YES if the real driver defines pfnOpen</li>
     * <li>GDAL_DCAP_CREATE: must be set to YES if the real driver defines pfnCreate</li>
     * <li>GDAL_DCAP_CREATE_MULTIDIMENSIONAL: must be set to YES if the real driver defines pfnCreateMultiDimensional</li>
     * <li>GDAL_DCAP_CREATECOPY: must be set to YES if the real driver defines pfnCreateCopy</li>
     * </ul>
     *
     * @since 3.9
     */
    class GDALPluginDriverProxy : public GDALDriver
    {
      public:
        GDALPluginDriverProxy(const std::string &osPluginFileName);
    }


The proxy driver uses the metadata items that have been set on it
to declare a minimum set of capabilities (GDAL_DCAP_RASTER, GDAL_DCAP_MULTIDIM_RASTER,
GDAL_DCAP_VECTOR, GDAL_DCAP_OPEN, etc.) to which it can answer directly, and
which are the ones used by GDALOpen() to open a dataset. For other metadata items,
it will fallback to loading the actual driver and forward the requests to it.


.. code-block:: cpp

    /** Declare a driver that will be loaded as a plugin, when actually needed.
     *
     * @param poProxyDriver Plugin driver proxy
     *
     * @since 3.9
     */
     void GDALDriverManager::DeclareDeferredPluginDriver(GDALPluginDriverProxy *poProxyDriver);


DeclareDeferredPluginDriver() method will also keep track of the plugin filename to avoid automatically
loading it in the GDALDriverManager::AutoLoadDrivers() method (that method
will only load out-of-tree drivers or in-tree drivers that have not been
converted to use DeclareDeferredPluginDriver()).

The main point is that drivers set the Identify() method on the proxy driver.
That Identify() method must be compiled in libgdal itself, and thus be
defined in a C++ file that does not depend on any external library.
Similarly for the GetSubdatasetInfoFunc() optional method.

When loading the actual driver, the GDALPluginDriverProxy::GetRealDriver()
method will check that all information set in its metadata is
consistent with the actual metadata of the underlying driver, and will warn
when there are differences.

GDALDataset::Open(), Create(), CreateCopy() methods are modified to not use
directly the pfnOpen, pfnCreate, pfnCreateCopy callbacks (that would be the ones
of the proxy driver, and thus nullptr), but to call new GetOpenCallback()/
GetCreateCallback()/GetCreateCopyCallback() methods that the GDALProxyDriver
class overloads to return the function pointers of the real driver, once it
has loaded it.

The DeclareDeferredPluginDriver() method checks if the file of the plugin
exists before registering it. If it is not available, a CPLDebug() message is
emitted. This allows to build a "universal" core libgdal, with plugins that can
be optionally available at runtime.

Cherry-on-the-cake: GDALOpen() will given an explicit error message if it
identifies a dataset to a plugin that is not available at runtime. Example::

    $ gdalinfo test.h5
    ERROR 4: `test.h5' not recognized as a supported file format. It could have
    been recognized by driver HDF5, but plugin gdal_HDF5.so is not available
    in your installation.


For each driver supporting deferred plugin loading, GDALAllRegister() must be
modified to call a driver-specific function that calls
GDALDriverManager::DeclareDeferredPluginDriver() (see example in below
paragraph). This code path is enabled only when the driver is built as plugin.


Example of changes to do on a simplified driver
-----------------------------------------------

In the :file:`CMakeLists.txt` file of a driver, the new option CORE_SOURCES can be
passed to ``add_gdal_driver()`` to define source file(s) that must be built in
libgdal, even when the driver is built as a plugin.

::

    add_gdal_driver(TARGET gdal_FOO
                    SOURCES foo.cpp
                    CORE_SOURCES foo_core.cpp
                    PLUGIN_CAPABLE
                    STRONG_CXX_WFLAGS)
    if (NOT TARGET gdal_FOO)
        return()
    endif()
    gdal_standard_includes(gdal_FOO)

A typical :file:`mydrivercore.h`` header will declare the identify method:

.. code-block:: cpp

    #include "gdal_priv.h"

    // Used by both DeclareDeferredFOOPlugin() and GDALRegisterFoo()
    constexpr const char* FOO_DRIVER_NAME = "FOO";

    int CPL_DLL FOODatasetIdentify(GDALOpenInfo* poOpenInfo);

    void CPL_DLL FOODriverSetCommonMetadata(GDALDriver *poDriver);

And :file:`mydrivercore.cpp` will contain the implementation of the identify method,
a ``FOODriverSetCommonMetadata()`` method (with most of the content of the normal
driver registration method, except for function pointers such as pfnOpen, pfnCreate,
pfnCreateCopy or pfnCreateMultiDimensional), as well as a ``DeclareDeferredXXXPlugin()``
method that will be called by GDALAllRegister() when the driver is built as a plugin
(the PLUGIN_FILENAME macro is automatically set by the CMake scripts with the filename of the
plugin, e.g. "gdal_FOO.so"):

.. code-block:: cpp

    int FOODatasetIdentify(GDALOpenInfo* poOpenInfo)
    {
        return poOpenInfo->nHeaderBytes >= 3 &&
               memcmp(poOpenInfo->pabyHeader, "FOO", 3) == 0;
    }

    // Called both by DeclareDeferredFOOPlugin() and GDALRegisterFoo()
    void FOODriverSetCommonMetadata(GDALDriver* poDriver)
    {
        poDriver->SetDescription(FOO_DRIVER_NAME);
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "The FOO format");
        poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "foo");
        poDriver->pfnIdentify = FOODatasetIdentify;
        poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES"); // since the actual driver defines pfnOpen
    }

    #ifdef PLUGIN_FILENAME
    void DeclareDeferredFOOPlugin()
    {
        if (GDALGetDriverByName(FOO_DRIVER_NAME) != nullptr)
        {
            return;
        }
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
        FOODriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    #endif


The GDALRegisterFoo() method itself, which is defined in the plugin code,
calls ``FOODriverSetCommonMetadata``,
and defines the pfnOpen, pfnCreate, pfnCreateCopy, pfnCreateMultiDimensional
callbacks when they exist:

.. code-block:: cpp

    void GDALRegisterFoo()
    {
        if (!GDAL_CHECK_VERSION(DRIVER_NAME))
            return;

        if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
            return;

        GDALDriver *poDriver = new GDALDriver();
        FOODriverSetCommonMetadata(poDriver);
        poDriver->pfnOpen = FOODataset::Open;
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }


The modified :file:`gdalallregister.cpp` file will look like:

.. code-block:: cpp

    void GDALAllRegister()
    {
        auto poDriverManager = GetGDALDriverManager();

        // Deferred driver declarations must be done *BEFORE* AutoLoadDrivers()
        #if defined(DEFERRED_FOO_DRIVER)
        DeclareDeferredFOOPlugin();
        #endif

        // This will not load gdal_FOO if above DeclareDeferredFOOPlugin()
        // has been called
        poDriverManager->AutoLoadDrivers();

        // Standard driver declarations below for drivers built inside libgdal
        // ...
        #if FRMT_foo
        GDALRegisterFoo();
        #endif
    }


Limitations
-----------

That mechanism only applies to in-tree plugins, since it requires a fraction
of the driver code to be embedded in libgdal. Out-of-tree plugins will
still be fully loaded at :cpp:func:`GDALAllRegister` time (or at
:cpp:func:`GDALDriverManager::LoadPlugin` time)

One could imagine a further enhancement for out-of-tree plugins where they
would be accompanied by a sidecar text file that would for example declare the
driver capabilities, as well as a limited implementation
of the identify method as a regular expression. But that is out-of-scope of
this RFC.

Changes in the loading of OGR Python drivers (see :ref:`rfc-76`) are also
out-of-scope of this RFC (they will continue to be loaded at
:cpp:func:`GDALAllRegister` time).

Candidate implementation
------------------------

A candidate implementation has been started to implement all the core mechanism,
and convert the Parquet, netCDF and HDF5 drivers. The HDF5 plugin is actually
a good stress test for the deferred loading mechanism, since it incorporates 4
drivers (HDF5, HDF5Image, BAG and S102) in the same shared object. The plan
is to update progressively all in-tree drivers that depend on third-party
libraries (that is the one that are built as plugins when setting the
GDAL_ENABLE_PLUGINS=YES CMake options).

Tests have also been done with QGIS (with the changes at
https://github.com/qgis/QGIS/pull/55115) to check that the declared set of
metadata items in GDALPluginDriverFeatures is sufficient to avoid loading of the
actual drivers at QGIS startup (they are only loaded when a dataset of the format
handled by the driver is identified)

Backward compatibility
----------------------

Expected to be backward compatible for most practical purposes.

Pedantically, if external code would directly use the pfnOpen, pfnCreate,
pfnCreateCopy function pointers of a GDALDriver instance, it would see them
null before the actual driver is loading, but direct access to
those function pointers has never been documented (instead users should use
GDALOpen(), GDALCreate(), GDALCreateCopy() etc), and is not expected to be
done by code external to libgdal core.

However, the candidate implementation hits an issue with the way the GDAL
CondaForge builds work currently. At time of writing, the GDAL CondaForge
build recipee does:

- a regular GDAL build without Arrow/Parquet dependency (and thus without the
  driver), whose libgdal.so goes in to the libgdal package.
- installs libarrow and libparquet
- does an incremental GDAL build with -DOGR_ENABLE_DRIVER_FOO_PLUGIN=ON to
  generate ogr_Arrow.so and ogr_Parquet.so. However with the above new mechanism,
  this will result in libgdal to be modified to have a DeclareDeferredOGRParquetPlugin
  function, as well as including the identification method of the Parquet plugin.
  But that modified libgdal.so is discarded currently, and the ogr_Parquet.so
  plugin then depends on a identify method that is not implemented.

The initial idea was that the build recipee would have to be modified to produce
all artifacts (libgdal.so and libparquet.so) at a single time, and dispatch
them appropriately in libgdal and libgdal-arrow-parquet packages, rather than
doing two builds. However, CondaForge builds support several libarrow versions,
and produce thus different Arrow/Parquet plugins, so this approach would not be
practial.

To solve this, the following idea was implemented. Extract from the updated
:ref:`building_from_source` document::

    Starting with GDAL 3.9, a number of in-tree drivers, that can be built as
    plugins, are loaded in a deferred way. This involves that some part of their
    code, which does not depend on external libraries, is included in core libgdal,
    whereas most of the driver code is in a separated dynamically loaded library.
    For builds where libgdal and its plugins are built in a single operation, this
    is fully transparent to the user.

    For more specific builds where libgdal would be first built, and then plugin
    drivers built in later incremental builds, this approach would not work, given
    that the core libgdal built initially would lack code needed to declare the
    plugin(s).

    In that situation, the user building GDAL will need to explicitly declare at
    initial libgdal build time that one or several plugin(s) will be later built.
    Note that it is safe to distribute such a libgdal library, even if the plugins
    are not always available at runtime.

    This can be done with the following option:

    .. option:: GDAL_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN:BOOL=ON

    .. option:: OGR_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN:BOOL=ON

        Declares that a driver will be later built as a plugin.

    Setting this option to drivers not ready for it will lead to an explicit
    CMake error.


    For some drivers, like netCDF (only case at time of writing), the dataset
    identification code embedded in libgdal, will depend on optional capabilities
    of the dependent library (libnetcdf)
    In that situation, it is desirable that the dependent library is available at
    CMake configuration time for the core libgdal built, but disabled with
    GDAL_USE_NETCDF=OFF. It must of course be re-enabled later when the plugin is
    built.

    For example::

        cmake .. -DGDAL_REGISTER_DRIVER_NETCDF_FOR_LATER_PLUGIN=ON -DGDAL_USE_NETCDF=OFF
        cmake --build .

        cmake .. -DGDAL_USE_NETCDF=ON -DGDAL_ENABLE_DRIVER_NETCDF=ON -DGDAL_ENABLE_DRIVER_NETCDF_PLUGIN=ON
        cmake --build . --target gdal_netCDF


    For other drivers, GDAL_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN /
    OGR_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN can be declared at
    libgdal build time without requiring the dependent libraries needed to build
    the pluging later to be available.


Documentation
-------------

:ref:`raster_driver_tut` and :ref:`vector_driver_tut` will be updated to point
to this RFC.
:ref:`building_from_source` will receive the new paragraph mentionned above.

Testing
-------

A C++ test will be added testing that for one of the updated drivers, the
plugin is loaded in a deferred way in situations where this is expected, and
is not loaded in other situations.

Related issues and PRs
----------------------

- https://github.com/OSGeo/gdal/compare/master...rouault:gdal:deferred_plugin?expand=1: candidate implementation

Voting history
--------------

TBD
