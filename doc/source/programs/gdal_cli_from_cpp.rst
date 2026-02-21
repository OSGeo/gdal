.. _gdal_cli_from_cpp:

================================================================================
How to use ``gdal`` CLI algorithms from C++
================================================================================

.. versionadded:: 3.12

``gdal`` CLI algorithms are available as :cpp:class:`GDALAlgorithm` instances,
and the include file to use is :file:`gdalalgorithm.h`.

The first step is to get the instance of the ``GDALGlobalAlgorithmRegistry`` with
:cpp:func:`GDALGlobalAlgorithmRegistry::GetSingleton` and then call
:cpp:func:`GDALAlgorithmRegistry::Instantiate` with the path of the algorithm.

For example:

.. code-block:: cpp

    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    GDALAlgorithm *algPtr = singleton.Instantiate("raster", "reproject");
    if (!algPtr)
    {
        std::cout << "cannot instantiate algorithm" << std::endl;
        return 1;
    }
    GDALAlgorithm &alg = *algPtr;


Arguments can be queried and set through :cpp:func:`GDALAlgorithm::operator[]`.

For example:

.. code-block:: cpp

    alg["input"] = "byte.tif";
    alg["output"] = "out.tif";
    alg["dst-crs"] = "EPSG:26711";
    alg["resampling"] = "cubic";
    alg["resolution"] = std::vector<double>{60, 60};
    alg["overwrite"] = true;


Execution of the algorithm is performed by :cpp:func:`GDALAlgorithm::Run`, with an optional
progress function. Proper closing of the output dataset must be performed with
:cpp:func:`GDALAlgorithm::Finalize`

For example:

.. code-block:: cpp

    if (alg.Run() && alg.Finalize())
    {
        std::cout << "success" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "failure" << std::endl;
        return 1;
    }


When outputting to a :ref:`raster.mem` dataset, you typically want to get
the output dataset with:

.. code-block:: cpp

    GDALDataset *poDS = alg["output"].Get<GDALArgDatasetValue>().GetDatasetRef();


Putting all things together:

.. code-block:: cpp

    #include "gdal.h"
    #include "gdalalgorithm.h"
    #include "gdal_dataset.h"

    #include <iostream>

    int main()
    {
        GDALAllRegister();
        auto& singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
        GDALAlgorithm *algPtr = singleton.Instantiate("raster", "reproject");
        if( !algPtr )
        {
            std::cout << "cannot instantiate algorithm" << std::endl;
            return 1;
        }
        GDALAlgorithm& alg = *algPtr;
        alg["input"] = "byte.tif";
        alg["output-format"] = "MEM";
        alg["dst-crs"] = "EPSG:26711";
        alg["resampling"] = "cubic";
        alg["resolution"] = std::vector<double>{60, 60};
        if( alg.Run() )
        {
            GDALDataset *poDS = alg["output"].Get<GDALArgDatasetValue>().GetDatasetRef();
            std::cout << "width=" << poDS->GetRasterXSize() << std::endl;
            std::cout << "height=" << poDS->GetRasterYSize() << std::endl;
            alg.Finalize();
            return 0;
        }
        else
        {
            std::cout << "failure" << std::endl;
            return 1;
        }
    }
