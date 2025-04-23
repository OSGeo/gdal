#include <iostream>

/* C API */
#include <gdal.h>

/* C++ API */
#ifdef USE_ODBC
#include <cpl_odbc.h>
#endif
#include <cpl_vsi_virtual.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>

int main(int argc, char **argv)
{
    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkt("POINT(1 2)", nullptr, &poGeom);
    OGRGeometryFactory::destroyGeometry(poGeom);
    std::cout << GDALVersionInfo("RELEASE_NAME") << std::endl;

    // Check fix for https://github.com/OSGeo/gdal/issues/11311
    OGRSpatialReference oSRS;
    int nEntries = 0;
    int *panMatchCondidence = nullptr;
    oSRS.FindMatches(nullptr, &nEntries, &panMatchCondidence);
    CPLFree(panMatchCondidence);

    return (0);
}
