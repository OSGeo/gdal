.. _vector.oda:

Open Design Alliance - ODA Platform
===================================

ODA Platform (previously named Teigha) is required to enable GDAL support for reading AutoCAD DWG and Microstation DGN v8 files. 
GDAL/OGR must be built with ODA support in order to enable these drivers.

ODA required products
---------------------

ODA Platform includes several SDK. Drawings SDK provides access to all data in .dwg and .dgn through an object-oriented API. It is required to compile GDAL. 
Since Kernel SDK is required by all products, these two products must be downloaded:

-  Kernel
-  Drawings

These libraries are not publicly available. You have to became a member to get access to the libraries. 
Upon authentication the libraries are available from: 
`ODA Member Downloads <https://account.opendesign.com/downloads>`__

Get the libraries
-----------------

To selected the appropriate files to download, consider the following ODA name conventions (for Linux):

-  lnx - Linux
-  X86, X64 - indicates X86 or X64 platform
-  8.3, 10.2, 11, 12, 13 - GCC versions
-  dll - indicates a shared library version
-  pic - compiled with Position Independent Code option

ODA archives also contains a release suffix in order to distinguish between releases, like 2025.8 or 2025.12

To download the required files for Linux, the following files could be downloaded:

-  `Kernel_lnxX64_13dll_25.12.tar.gz`
-  `Drawings_lnxX64_13dll_25.12.tar.gz`

In this example, the files names are:

-  `lnx` for Linux
-  `X64` for X64 architecture
-  `13` for gcc 7.2
-  `dll` for shared library version
-  `2025.12` ODA 2025 release, build 12

Compiling the libraries
-----------------------

The libraries must be merged before compiling.

.. code:: bash

   cd ~/dev/cpp/ODA25.12
   mkdir Oda_25_12
   tar xvzf Kernel_lnxX64_13dll_25.12.tar.gz -C Oda_25_12
   tar xvzf Drawings_lnxX64_13dll_25.12.tar.gz -C Oda_25_12

To compile, an activation key is required. It can be requested from ODA Products Activation. 
The activation key must be copied to `ThirdParty/activation/`.

::

   cp OdActivationInfo Oda_25_12/ThirdParty/activation/

Compile the ODA libraries with:

::

   cd Oda_25_12
   ./configure
   make -j8

Make sure your gcc major version matches ODA libs gcc version. On Ubuntu, for example, you can install different gcc/g++ versions, like 8, 10, 11, 12, 13. Switch between them with:

::

   sudo update-alternatives --config gcc
   sudo update-alternatives --config g++

ODA libraries path
------------------

After compiling ODA, the resulting libs are in a non standard search path. 
There is no `make install` included to copy the libraries to a standard location.
This might be an issue.

You have different alternative options to compile and run GDAL/OGR with ODA:

-  copy the ODA libraries to a standard location
-  set LD_LIBRARY_PATH (like `LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/jgr/dev/cpp/ODA21.6/Oda_25_12/bin/lnxX64_25.12dll`).
-  Adding ODA folder to the system library path (`echo "/home/jgr/dev/cpp/ODA21.6/Oda_25_12/bin/lnxX64_25.12dll" | sudo tee -a /etc/ld.so.conf.d/z_gdal-ODA.conf`)
-  Setting a run time path (`rpath`) when compiling GDAL (like `LDFLAGS="-Wl,-rpath=/home/jgr/dev/cpp/ODA21.6/Oda_25_12/bin/lnxX64_25.12dll"`).


ODA library names

Some ODA library names do not conform the usual Linux standard `lib*.so`. If you don't use `rpath`, for the other alternatives listed above, you might have to create symbolic links from the actual names. Example:

::

   cd ~/dev/cpp/ODA25.12/Oda_25_12/bin/lnxX64_25.12dll
   for f in *.tx
   do
      echo "Processing $f"
      ln -s $f lib$f.so
   done
   sudo ldconfig

Check with `ldconfig -v` if all ODA libraries are now visible.


Compiling GDAL
--------------

After compiling ODA libraries, GDAL can be build using three additional options:

-  `-DTEIGHA_ROOT="/home/jgr/dev/cpp/ODA21.6/Oda_25_12" `
-  `-DTEIGHA_PLATFORM="lnxX64_13dll" `
-  `-DTEIGHA_ACTIVATION_FILE_DIRECTORY="/home/jgr/dev/cpp/ODA21.6/Oda_25_12/ThirdParty/Activation"`

The value of `-DTEIGHA_ROOT` is the full path of the folder where the libraries were merged and compiled.
The value of `-DTEIGHA_PLATFORM` must match the platform name used by ODA. The platform name is the folder name under `Platforms`:

:: ls -l Platforms/
   lnxX64_7.2dll

The value of `-DTEIGHA_ACTIVATION_FILE_DIRECTORY` is the full path of the folder where the OdActivationInfo file was copied.

GDAL compilation
----------------

Use your own GDAL build configuration and add the previous mentioned options. After running `configure`, make sure that `Teigha (DWG and DGNv8)` support is configured.

As an example, compiling GDAL can be:

::

   cd gdal
   ./autogen.sh
   ./configure --without-libtool LDFLAGS="-L/usr/lib/x86_64-linux-gnu" --with-python=python3 --with-proj=/usr/local --with-pg=yes --with-poppler -DTEIGHA_ROOT="/home/jgr/dev/cpp/ODA21.6/Oda_25_12" -DTEIGHA_PLATFORM="lnxX64_13dll" -DTEIGHA_ACTIVATION_FILE_DIRECTORY="/home/jgr/dev/cpp/ODA21.6/Oda_25_12/ThirdParty/Activation"
   make -j8
   sudo make install
   sudo ldconfig
   # Python support
   cd swig/python
   python3 setup.py build
   sudo python3 setup.py install   

We added `LDFLAGS="-L/usr/lib/x86_64-linux-gnu"` to use system libs over ODA's `libpcre`, `libcurl`, etc.

Adding ODA Geodata capabilities
-------------------------------

Important infos from ODA documentation (https://docs.opendesign.com/td/db_geo_functionality.html) :

- The csmap library implements coordinate system support and calculations. It's an alternative to proj; see here : https://trac.osgeo.org/csmap
- The csmap library should have access to a folder with the required dictionaries. The dictionaries are available for downloading from your personal account.
- Set the MENTOR_DICTIONARY_PATH or CS_MAP_DIR environment variables pointing to the CSDicts folder.

So step 1 is to get and unzip those csmap dictionaries found at <https://account.opendesign.com/downloads>
Step 2 is to set an env variable (CS_MAP_DIR for example) pointing at the unzipped folder.

Testing
-------

After compiling GDAL, you can check if the new drivers `DGNV8` and `DWG` are supported with:

::

   ./apps/ogrinfo --formats | grep 'AutoCAD\|Microstation'
   DGN -vector- (rw+v): Microstation DGN
   DWG -vector- (ro): AutoCAD DWG
   DGNV8 -vector- (rw+): Microstation DGNv8
   DXF -vector- (rw+v): AutoCAD DXF
   CAD -raster,vector- (rovs): AutoCAD Driver

If a file is DGNv8, you will see that driver in action when opening the file:

::

   ogrinfo ~/dev/cpp/gdal/autotest/ogr/data/dgnv8/test_dgnv8.dgn
   INFO: Open of `/home/jgr/dev/cpp/gdal/autotest/ogr/data/dgnv8/test_dgnv8.dgn'
         using driver `DGNV8' successful.
   1: my_model

Troubleshooting
---------------

If you find linking errors, you can set `LD_LIBRARY_PATH` or `LDFLAGS` environment variables to make sure you are able to get the ODA libraries from their location.

Use `ldconfig -v` to check if ODA's library folder is listed.

For example, you can try:

::

   export LD_LIBRARY_PATH=/home/jgr/dev/cpp/ODA21.6/base_7.2/bin/lnxX64_7.2dll
   ./configure --without-libtool LDFLAGS="-L/usr/lib/x86_64-linux-gnu" --with-python=python3 --with-proj=/usr/local --with-pg=yes --with-poppler --with-teigha=/home/jgr/dev/cpp/ODA21.6/base_7.2 --with-teigha-plt=lnxX64_7.2dll   

You can force a run time location (with `rpath`) with:

::

   ./configure --without-libtool LDFLAGS="-L/usr/lib/x86_64-linux-gnu -Wl,-rpath=/home/jgr/dev/cpp/ODA21.6/base_7.2/bin/lnxX64_7.2dll" --with-python=python3 --with-proj=/usr/local --with-pg=yes --with-poppler --with-teigha=/home/jgr/dev/cpp/ODA21.6/base_7.2 --with-teigha-plt=lnxX64_7.2dll   


Adjust these settings, according to your build environment. 

See Also
--------

-  `Introducing the ODA Platform <https://www.opendesign.com/products>`__
-  :ref:`AutoCAD DWG <vector.dwg>`
-  :ref:`Microstation DGN v8 <vector.dgnv8>`
