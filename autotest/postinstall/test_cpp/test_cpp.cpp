#include <iostream>

/* C API */
#include <gdal.h>

/* C++ API */
#include <cpl_odbc.h>
#include <cpl_vsi_virtual.h>
#include <ogr_geometry.h>

int main(int argc, char** argv) {
    OGRGeometry* poGeom = nullptr;
    OGRGeometryFactory::createFromWkt("POINT(1 2)", nullptr, &poGeom);
    OGRGeometryFactory::destroyGeometry(poGeom);
    std::cout << GDALVersionInfo("RELEASE_NAME") << std::endl;
    return(0);
}
