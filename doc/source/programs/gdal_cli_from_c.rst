.. _gdal_cli_from_c:

================================================================================
How to use ``gdal`` CLI algorithms from C
================================================================================

.. versionadded:: 3.12

The C API for ``gdal`` CLI algorithms is available in :file:`gdalalgorithm.h`.

The first step is to get the instance of the ``GDALGlobalAlgorithmRegistry`` with
:cpp:func:`GDALGetGlobalAlgorithmRegistry` and then call
:cpp:func:`GDALAlgorithmRegistryInstantiateAlgFromPath` with the path of the algorithm.
Once done with a ``GDALAlgorithmRegistryH`` handle, the memory associated to it
can be released with :cpp:func:`GDALAlgorithmRegistryRelease` (the algorithm handles
got from the registry will still be valid).

For example:

.. code-block:: cpp

    GDALAlgorithmRegistryH hRegistry = GDALGetGlobalAlgorithmRegistry();
    const char *const papszAlgPath[] = { "raster", "reproject", NULL };
    GDALAlgorithmH hAlg = GDALAlgorithmRegistryInstantiateAlgFromPath(hRegistry, papszAlgPath);
    GDALAlgorithmRegistryRelease(hRegistry);

Arguments can be queried with :cpp:func:`GDALAlgorithmGetArg` and set through
different setters depending on the type of the argument, such as
:cpp:func:`GDALAlgorithmArgSetAsString`, :cpp:func:`GDALAlgorithmArgSetAsInteger`,
:cpp:func:`GDALAlgorithmArgSetAsDouble`, etc. Once done with the argument handle,
it must be released with :cpp:func:`GDALAlgorithmArgRelease`.

For example:

.. code-block:: cpp

    {
        GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "input");
        GDALAlgorithmArgSetAsString(hArg, "byte.tif");
        GDALAlgorithmArgRelease(hArg);
    }
    {
        GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "resolution");
        const double res[] = {60, 60};
        GDALAlgorithmArgSetAsDoubleList(hArg, 2, res);
        GDALAlgorithmArgRelease(hArg);
    }


Execution of the algorithm is performed by :cpp:func:`GDALAlgorithmRun`, with an optional
progress function. Proper closing of the output dataset must be performed with
:cpp:func:`GDALAlgorithmFinalize`. Once done with the algorithm handle,
it must be released with :cpp:func:`GDALAlgorithmFinalize`.

.. code-block:: cpp

    int ret = 0;
    if( GDALAlgorithmRun(hAlg, NULL, NULL) )
    {
        // do something
        GDALAlgorithmArgRelease(hArg);
    }
    else
    {
        fprintf(stderr, "failure\n");
        ret = 1;
    }
    GDALAlgorithmFinalize(hAlg);
    GDALAlgorithmRelease(hAlg);


When outputting to a :ref:`raster.mem` dataset, you typically want to get
the output dataset with:

.. code-block:: cpp

    GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "output");
    GDALArgDatasetValueH hVal = GDALAlgorithmArgGetAsDatasetValue(hArg);
    GDALDatasetH hDS = GDALArgDatasetValueGetDatasetRef(hVal);


Putting all things together:

.. code-block:: cpp

    #include "gdal.h"
    #include "gdalalgorithm.h"

    #include <stdio.h>

    int main()
    {
        GDALAllRegister();
        GDALAlgorithmRegistryH hRegistry = GDALGetGlobalAlgorithmRegistry();
        const char *const papszAlgPath[] = { "raster", "reproject", NULL };
        GDALAlgorithmH hAlg = GDALAlgorithmRegistryInstantiateAlgFromPath(hRegistry, papszAlgPath);
        GDALAlgorithmRegistryRelease(hRegistry);
        if( !hAlg )
        {
            fprintf(stderr, "cannot instantiate algorithm\n");
            return 1;
        }

        {
            GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "input");
            GDALAlgorithmArgSetAsString(hArg, "byte.tif");
            GDALAlgorithmArgRelease(hArg);
        }

        {
            GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "output-format");
            GDALAlgorithmArgSetAsString(hArg, "MEM");
            GDALAlgorithmArgRelease(hArg);
        }

        {
            GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "dst-crs");
            GDALAlgorithmArgSetAsString(hArg, "EPSG:26711");
            GDALAlgorithmArgRelease(hArg);
        }

        {
            GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "resolution");
            const double res[] = {60, 60};
            GDALAlgorithmArgSetAsDoubleList(hArg, 2, res);
            GDALAlgorithmArgRelease(hArg);
        }

        int ret = 0;
        if( GDALAlgorithmRun(hAlg, NULL, NULL) )
        {
            GDALAlgorithmArgH hArg = GDALAlgorithmGetArg(hAlg, "output");
            GDALArgDatasetValueH hVal = GDALAlgorithmArgGetAsDatasetValue(hArg);
            GDALDatasetH hDS = GDALArgDatasetValueGetDatasetRef(hVal);
            printf("width=%d, height=%d\n", GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));
            GDALAlgorithmArgRelease(hArg);
        }
        else
        {
            fprintf(stderr, "failure\n");
            ret = 1;
        }
        GDALAlgorithmFinalize(hAlg);
        GDALAlgorithmRelease(hAlg);
        return ret;
    }
