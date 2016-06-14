#include "gdal.h"

int main()
{
    /* see https://trac.osgeo.org/gdal/ticket/6139 */
    GDALDestroy();
    return 0;
}
