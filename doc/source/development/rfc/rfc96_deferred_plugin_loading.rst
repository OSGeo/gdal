.. _rfc-96:

==================================================================
RFC 96: Deferred C++ plugin loading
==================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2023-Nov-01
Status:        Adopted, implemented
Target:        GDAL 3.9
============== =============================================

Summary
-------

This RFC adds a mechanism to defer the loading of C++ plugin drivers to
the point where their executable code is actually needed, and converts a number
of relevant drivers to use that mechanism. The aim is to allow for more modular
GDAL builds, while improving the performance of plugin loading.

It mostly targets in-tree plugin drivers, but it also provides a way for
out-of-tree plugin drivers to benefit from deferred loading capabilities,
provided libgdal is built in a specific way

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
nothing we can do to reduce it... besides limiting the amount of dynamic loading
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

- to provide the capability to out-of-tree drivers to optionally benefit from
  the new capability, provided they build GDAL with the code needed to declared
  a plugin proxy driver.

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
     * <li>GDAL_DMD_OPENOPTIONLIST</li>
     * <li>GDAL_DMD_SUBDATASETS</li>
     * <li>GDAL_DMD_CONNECTION_PREFIX</li>
     * <li>GDAL_DCAP_RASTER</li>
     * <li>GDAL_DCAP_MULTIDIM_RASTER</li>
     * <li>GDAL_DCAP_VECTOR</li>
     * <li>GDAL_DCAP_GNM</li>
     * <li>GDAL_DCAP_MULTIPLE_VECTOR_LAYERS</li>
     * <li>GDAL_DCAP_NONSPATIAL</li>
     * <li>GDAL_DCAP_VECTOR_TRANSLATE_FROM</li>
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

.. _rfc96_example_driver:

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

Out-of-tree deferred loaded plugins
+++++++++++++++++++++++++++++++++++

Out-of-tree drivers can also benefit from the deferred loading capability, provided
libgdal is built with CMake variable(s) pointing to external code containing the
code for registering a proxy driver.

This can be done with the following CMake option:

.. option:: ADD_EXTERNAL_DEFERRED_PLUGIN_<driver_name>:FILEPATH=/path/to/some/file.cpp

The pointed file must declare a ``void DeclareDeferred<driver_name>(void)``
method with C linkage that takes care of creating a GDALPluginDriverProxy
instance and calling GDALDriverManager::DeclareDeferredPluginDriver() on it.

Limitations
-----------

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

Drivers that would request a driver instance with GDALGetDriverByName() may
now get a GDALPluginDriverProxy instance instead of the "real" driver instance.
This is usually not an issue as few drivers subclass GDALDriver, but that issue
was hit on the PostGISRasterDriver that did subclass it. The solution was to
store the real PostGISRasterDriver instance when it is built in a global variable,
and use that global variable instead of the one returned by GDALGetDriverByName().

Another potential issue is that if external code would directly use the pfnOpen, pfnCreate,
pfnCreateCopy, etc. function pointers of a GDALDriver instance, it would see them
null before the actual driver is loading, but direct access to
those function pointers has never been documented (instead users should use
GDALOpen(), GDALCreate(), GDALCreateCopy() etc), and is not expected to be
done by code external to libgdal core.

However, the candidate implementation hits an issue with the way the GDAL
CondaForge builds work currently. At time of writing, the GDAL CondaForge
build recipe does:

- a regular GDAL build without Arrow/Parquet dependency (and thus without the
  driver), whose libgdal.so goes in to the libgdal package.
- installs libarrow and libparquet
- does an incremental GDAL build with -DOGR_ENABLE_DRIVER_FOO_PLUGIN=ON to
  generate ogr_Arrow.so and ogr_Parquet.so. However with the above new mechanism,
  this will result in libgdal to be modified to have a DeclareDeferredOGRParquetPlugin
  function, as well as including the identification method of the Parquet plugin.
  But that modified libgdal.so is discarded currently, and the ogr_Parquet.so
  plugin then depends on a identify method that is not implemented.

The initial idea was that the build recipe would have to be modified to produce
all artifacts (libgdal.so and libparquet.so) at a single time, and dispatch
them appropriately in libgdal and libgdal-arrow-parquet packages, rather than
doing two builds. However, CondaForge builds support several libarrow versions,
and produce thus different Arrow/Parquet plugins, so this approach would not be
practical.

To solve this, the following idea has been implemented. Extract from the updated
:ref:`building_from_source` document::

    Starting with GDAL 3.9, a number of in-tree drivers, that can be built as
    plugins, are loaded in a deferred way. This involves that some part of their
    code, which does not depend on external libraries, is included in core libgdal,
    whereas most of the driver code is in a separated dynamically loaded library.
    For builds where libgdal and its plugins are built in a single operation, this
    is fully transparent to the user.

    When a plugin driver is known of core libgdal, but not available as a plugin at
    runtime, GDAL will inform the user that the plugin is not available, but could
    be installed. It is possible to give more hints on how to install a plugin
    by setting the following option:

    .. option:: GDAL_DRIVER_<driver_name>_PLUGIN_INSTALLATION_MESSAGE:STRING

    .. option:: OGR_DRIVER_<driver_name>_PLUGIN_INSTALLATION_MESSAGE:STRING

        Custom message to give a hint to the user how to install a missing plugin


    For example, if doing a build with::

        cmake .. -DOGR_DRIVER_PARQUET_PLUGIN_INSTALLATION_MESSAGE="You may install it with with 'conda install -c conda-forge libgdal-arrow-parquet'"

    and opening a Parquet file while the plugin is not installed will display the
    following error::

        $ ogrinfo poly.parquet
        ERROR 4: `poly.parquet' not recognized as a supported file format. It could have been recognized by driver Parquet, but plugin ogr_Parquet.so is not available in your installation. You may install it with with 'conda install -c conda-forge libgdal-arrow-parquet'


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


    For some drivers (ECW, HEIF, JP2KAK, JPEG, JPEGXL, KEA, LERC, MrSID,
    MSSQLSpatial, netCDF, OpenJPEG, PDF, TileDB, WEBP), the metadata and/or dataset
    identification code embedded on libgdal, will depend on optional capabilities
    of the dependent library (e.g. libnetcdf for netCDF)
    In that situation, it is desirable that the dependent library is available at
    CMake configuration time for the core libgdal built, but disabled with
    GDAL_USE_<driver_name>=OFF. It must of course be re-enabled later when the plugin is
    built.

    For example for netCDF::

        cmake .. -DGDAL_REGISTER_DRIVER_NETCDF_FOR_LATER_PLUGIN=ON -DGDAL_USE_NETCDF=OFF
        cmake --build .

        cmake .. -DGDAL_USE_NETCDF=ON -DGDAL_ENABLE_DRIVER_NETCDF=ON -DGDAL_ENABLE_DRIVER_NETCDF_PLUGIN=ON
        cmake --build . --target gdal_netCDF


    For other drivers, GDAL_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN /
    OGR_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN can be declared at
    libgdal build time without requiring the dependent libraries needed to build
    the plugin later to be available.


Documentation
-------------

:ref:`raster_driver_tut` and :ref:`vector_driver_tut` will be updated to point
to this RFC.
:ref:`building_from_source` will receive the new paragraph mentioned above.

Testing
-------

A C++ test will be added testing that for one of the updated drivers, the
plugin is loaded in a deferred way in situations where this is expected, and
is not loaded in other situations.

Related issues and PRs
----------------------

- https://github.com/OSGeo/gdal/pull/8695: candidate implementation

Adjustments done post GDAL 3.9.0, for GDAL 3.9.1
------------------------------------------------

After GDAL 3.9.0 release, it has been noticed that the following setup which
used to work in prior releases no longer worked:

- Step 1: building libgdal without support for driver X
- Step 2: building driver X as a plugin, discarding the libgdal share library,
          built at that stage
- Step 3: using driver X built as a plugin against libgdal built at step 1. In that
          scenario, driver X is expected to be loaded as if it was an out-of-tree drivers.

Such scenario is used when delivering a fully open-source libgdal without any
prior knowledge of which drivers could be later built as plugins, or for which
pre-configuring libgdal to support such drivers is not practical because they
rely on a proprietary SDK and the identification method and/or driver metadata
depends on the availability of the SDK include files (e.g. MrSID).

Starting with GDAL 3.9.1, the ``add_gdal_driver()`` function in the CMakeLists.txt
of drivers which use the ``CORE_SOURCES`` keyword must also declare the
``NO_SHARED_SYMBOL_WITH_CORE`` keyword, so that the files pointed by CORE_SOURCES
are built twice: once in libgdal with a ``GDAL_core_`` prefix, and another time
in the plugin itself with a ``GDAL_driver_`` prefix, by using the
PLUGIN_SYMBOL_NAME() macro of :file:`gdal_priv.h`.

Example in ogr/ogrsf_frmsts/oci/CMakeLists.txt:

.. code-block::

    add_gdal_driver(TARGET ogr_OCI
                    SOURCES ${SOURCE}
                    CORE_SOURCES ogrocidrivercore.cpp
                    PLUGIN_CAPABLE
                    NO_SHARED_SYMBOL_WITH_CORE)


Example in ogrocidrivercore.h:

.. code-block:: cpp

    #define OGROCIDriverIdentify \
       PLUGIN_SYMBOL_NAME(OGROCIDriverIdentify)
    #define OGROCIDriverSetCommonMetadata \
       PLUGIN_SYMBOL_NAME(OGROCIDriverSetCommonMetadata)

    int OGROCIDriverIdentify(GDALOpenInfo *poOpenInfo);

    void OGROCIDriverSetCommonMetadata(GDALDriver *poDriver);


A consequence of that change is that drivers built as a plugin against GDAL 3.9.0
will not be loadable by GDAL 3.9.1 (or later patch in the 3.9 series), because
they relied on driver-specific functions that are no longer exported by libgdal >= 3.9.1.

After that, things should work as they used to, and drivers built against libgdal 3.9.1
should work against libgdal 3.9.2 for example.

Also note that the above only affects *in-tree* plugin drivers. Out-of-tree plugin drivers
are not affected.

Voting history
--------------

+1 from PSC members KurtS, HowardB, JukkaR, JavierJS and EvenR
