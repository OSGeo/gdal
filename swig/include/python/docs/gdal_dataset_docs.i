%feature("docstring") GDALDatasetShadow "
Python proxy of a raster :cpp:class:`GDALDataset`.

Since GDAL 3.8, a Dataset can be used as a context manager.
When exiting the context, the Dataset will be closed and
data will be written to disk.
"

%extend GDALDatasetShadow {
}

