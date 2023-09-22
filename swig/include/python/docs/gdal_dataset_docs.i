%feature("docstring") GDALDatasetShadow "
Python proxy of a raster :cpp:class:`GDALDataset`.

Since GDAL 3.8, a Dataset can be used as a context manager.
When exiting the context, the Dataset will be closed and
data will be written to disk.
"

%extend GDALDatasetShadow {

%feature("docstring")  Close "
Closes opened dataset and releases allocated resources.

This method can be used to force the dataset to close
when one more references to the dataset are still
reachable. If Close is never called, the dataset will
be closed automatically during garbage collection.
"

}

