# GDAL – Geospatial Data Abstraction Library

GDAL is an open-source translator library for reading, writing, and processing
geospatial data. It provides a unified interface to hundreds of raster and vector
formats and powers many well-known tools and applications, including QGIS,
ArcGIS, PostGIS, GRASS GIS, and countless data-processing pipelines.

GDAL is used everywhere geospatial data is used — from environmental modelling,
satellite and aerial imagery analysis, and cartography, to modern cloud-native
data processing workflows.



## Key Features

- Read and write 300+ raster and vector formats  
  (GeoTIFF, NetCDF, JPEG2000, Shapefile, GeoPackage, Cloud-Optimized GeoTIFF, …)
- Consistent APIs in C/C++, Python, and other languages
- High-performance raster operations  
  (reprojection, resampling, mosaicking, clipping)
- Robust vector processing with OGR  
  (geometry operations, filtering, I/O, attribute editing)
- Cloud-native support (HTTP, S3, Azure, Google Cloud)
- Command-line utilities for rapid geospatial processing



## Quick Example (Python)

```python
from osgeo import gdal

ds = gdal.Open("input.tif")
print("Size:", ds.RasterXSize, ds.RasterYSize)

band = ds.GetRasterBand(1)
array = band.ReadAsArray()
print(array.mean())
```

## Quick Example (C++)

```cpp
#include "gdal_priv.h"

int main()
{
    GDALAllRegister();
    GDALDataset* ds =
        (GDALDataset*) GDALOpen("input.tif", GA_ReadOnly);

    int x = ds->GetRasterXSize();
    int y = ds->GetRasterYSize();
}
```


## Documentation & Resources

Documentation: https://gdal.org
Python API: https://gdal.org/api/python/
C++ API: https://gdal.org/api/
Tutorials: https://gdal.org/tutorials/
Issue Tracker: https://github.com/OSGeo/gdal/issues
Discussions: https://github.com/OSGeo/gdal/discussions


## Contributing

GDAL welcomes contributions!
See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.



## License

GDAL is released under the permissive X/MIT-style license.
See [LICENSE.TXT] for full details.
