.. _gdal_band_algebra:

================================================================================
GDAL Band Algebra
================================================================================

.. versionadded:: 3.12

Description
-----------

Common algebraic operations can be on :cpp:class:`GDALRasterBand` instances
from C++ and Python:

- addition, with ``+`` operator
- subtraction, with ``*`` operator
- multiplication, with ``-`` operator
- division, with ``/`` operator
- absolute value / module, with :cpp:func:`gdal::abs` in C++ and
  :py:meth:`osgeo.gdal.abs` in Python
- square root, with :cpp:func:`gdal::sqrt` in C++ and
  :py:meth:`osgeo.gdal.sqrt` in Python
- logarithm base 10, with :cpp:func:`gdal::log10` in C++ and
  :py:meth:`osgeo.gdal.log10` in Python
- natural logarithm, with :cpp:func:`gdal::log` in C++ and
  :py:meth:`osgeo.gdal.log` in Python
- raising to the power, with :cpp:func:`gdal::pow` in C++ and
  :py:meth:`osgeo.gdal.pow` in Python
- strictly greater than comparison, with ``>`` operator
- greater or equal to comparison, with ``>=`` operator
- strictly lesser than comparison, with ``<`` operator
- lesser of equal to comparison, with ``<=`` operator
- equal to comparison, with ``==`` operator
- not equal to comparison, with ``!=`` operator
- logical and, with ``&&`` operator in C++ and :py:meth:`osgeo.gdal.logical_and` in Python
- logical or with ``||`` operator in C++ and :py:meth:`osgeo.gdal.logical_or` in Python
- logical not, with ``!`` operator in C++ and :py:meth:`osgeo.gdal.logical_not` in Python

Left and right operands can be a Band object or a numeric value (at least
one of them must be a Band object). If two bands are provided they must have
the same dimension.

The result of those operations is a band of the same dimension as the input
one(s), and whose each pixel is evaluated by applying the operator on the
corresponding pixel in the input band(s). The resulting Band object is lazy
evaluated, that is pixel values are computed only when they are requested.

Other operations are available:

- ternary / if-then-else, with :cpp:func:`gdal::IfThenElse` in C++ and
  :py:meth:`osgeo.gdal.where` in Python (similar to `NumPy where <https://numpy.org/doc/stable/reference/generated/numpy.where.html>`__)
- cast to a data type, with :cpp:func:`GDALRasterBand::AsType` in C++ and
  :py:meth:`osgeo.gdal.Band.astype` in Python
- minimum of several bands (or constants), with :cpp:func:`gdal::min` in C++ and
  :py:meth:`osgeo.gdal.minimum` in Python
- maximum of several bands (or constants), with :cpp:func:`gdal::max` in C++ and
  :py:meth:`osgeo.gdal.maximum` in Python
- arithmetic mean of several bands, with :cpp:func:`gdal::mean` in C++ and
  :py:meth:`osgeo.gdal.mean` in Python

It is possible to serialize the operation to a :ref:`raster.vrt` file by using
:cpp:func:`GDALDriver::CreateCopy` on the dataset owing the result band.

When several bands are combined together, that at least one of them has a nodata
value but they do not share the same nodata value, not-a-number will be used as
the nodata value for the result band.

The capability is similar to the one offered by the :ref:`gdal_raster_calc` program.

.. note:: The comparison operators, including the ternary one, require a GDAL build against the muparser library.

.. note:: The operations are also available in the C API, for potential bindings
          to other languages. Cf :cpp:func:`GDALRasterBandUnaryOp`,
          :cpp:func:`GDALRasterBandBinaryOpBand`,
          :cpp:func:`GDALRasterBandBinaryOpDouble`, :cpp:func:`GDALRasterBandBinaryOpDoubleToBand`,
          :cpp:func:`GDALRasterBandIfThenElse`, :cpp:func:`GDALRasterBandAsDataType`,
          :cpp:func:`GDALMaximumOfNBands`, :cpp:func:`GDALRasterBandMaxConstant`,
          :cpp:func:`GDALMinimumOfNBands`, :cpp:func:`GDALRasterBandMinConstant` and
          :cpp:func:`GDALMeanOfNBands`

Examples
--------

.. example::
    :title: Convert a RGB dataset to a graylevel one.

    .. tabs::

       .. code-tab:: c++

            #include <gdal_priv.h>

            int main()
            {
                GDALAllRegister();

                auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open("rgb.tif"));
                auto& R = *(poDS->GetRasterBand(1));
                auto& G = *(poDS->GetRasterBand(2));
                auto& B = *(poDS->GetRasterBand(3));
                auto graylevel = (0.299 * R + 0.587 * G + 0.114 * B).AsType(GDT_Byte);

                auto poGTiffDrv = GetGDALDriverManager()->GetDriverByName("GTiff");
                std::unique_ptr<GDALDataset>(
                    poGTiffDrv->CreateCopy("graylevel.tif", graylevel.GetDataset(), false, nullptr, nullptr, nullptr)).reset();

                return 0;
            }

       .. code-tab:: python

            from osgeo import gdal
            gdal.UseExceptions()

            with gdal.Open("rgb.tif") as ds:
               R = ds.GetRasterBand(1)
               G = ds.GetRasterBand(2)
               B = ds.GetRasterBand(3)
               graylevel = (0.299 * R + 0.587 * G + 0.114 * B).astype(gdal.GDT_Byte)
               gdal.GetDriverByName("GTiff").CreateCopy("graylevel.tif", graylevel)


.. example::
    :title: Compute normalized difference vegetation index (NDVI)

    .. tabs::

       .. code-tab:: c++

            #include <gdal_priv.h>

            int main()
            {
                GDALAllRegister();

                auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open("rgbnir.tif"));
                auto& R = *(poDS->GetRasterBand(1));
                auto& NIR = *(poDS->GetRasterBand(4));
                auto NDVI = (NIR - R) / (NIR + R);

                auto poGTiffDrv = GetGDALDriverManager()->GetDriverByName("GTiff");
                std::unique_ptr<GDALDataset>(
                    poGTiffDrv->CreateCopy("NDVI.tif", NDVI.GetDataset(), false, nullptr, nullptr, nullptr)).reset();

                return 0;
            }

       .. code-tab:: python

            from osgeo import gdal
            gdal.UseExceptions()

            with gdal.Open("rgbnir.tif") as ds:
               R = ds.GetRasterBand(1)
               NIR = ds.GetRasterBand(4)
               NDVI = (NIR - R) / (NIR + R)
               gdal.GetDriverByName("GTiff").CreateCopy("NDVI.tif", NDVI)


.. example::
    :title: Normalizing the values of a band to the [0, 1] range using the minimum and maximum of all bands

    .. tabs::

       .. code-tab:: c++

            #include <gdal_priv.h>

            int main()
            {
                GDALAllRegister();

                auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open("input.tif"));
                auto& A = *(poDS->GetRasterBand(1));
                auto& B = *(poDS->GetRasterBand(2));
                auto& C = *(poDS->GetRasterBand(3));
                auto max_minus_min = gdal::max(A,B,C) - gdal::min(A,B,C);
                auto A_normalized = gdal::IfThenElse(max_minus_min == 0, 1.0, (A - gdal::min(A,B,C)) / max_minus_min);

                auto poVRTDrv = GetGDALDriverManager()->GetDriverByName("VRT");
                std::unique_ptr<GDALDataset>(
                    poVRTDrv->CreateCopy("A_normalized.vrt", A_normalized.GetDataset(), false, nullptr, nullptr, nullptr)).reset();

                return 0;
            }

       .. code-tab:: python

            from osgeo import gdal
            gdal.UseExceptions()

            with gdal.Open("input.tif") as ds:
               A = ds.GetRasterBand(1)
               B = ds.GetRasterBand(2)
               C = ds.GetRasterBand(3)
               max_minus_min = gdal.maximum(A,B,C) - gdal.minimum(A,B,C)
               A_normalized = gdal.where(max_minus_min == 0, 1.0, (A - gdal.min(A,B,C)) / max_minus_min)
               gdal.GetDriverByName("VRT").CreateCopy("A_normalized.vrt", A_normalized)
