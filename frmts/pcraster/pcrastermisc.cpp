// Library headers.
#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

#ifndef INCLUDED_STRING
#include <string>
#define INCLUDED_STRING
#endif

// PCRaster library headers.

// Module headers.
#include "gdal_pam.h"

#ifndef INCLUDED_PCRASTERDATASET
#include "pcrasterdataset.h"
#define INCLUDED_PCRASTERDATASET
#endif



CPL_C_START
void GDALRegister_PCRaster(void);
CPL_C_END



void GDALRegister_PCRaster()
{
  if(!GDALGetDriverByName("PCRaster")) {

    GDALDriver* driver = new GDALDriver();

    driver->SetDescription("PCRaster");
    driver->SetMetadataItem(GDAL_DMD_LONGNAME, "PCRaster Raster File");
    driver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int32 Float32");
    driver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_various.html#PCRaster");
    driver->SetMetadataItem( GDAL_DMD_EXTENSION, "map" );

    driver->pfnOpen = PCRasterDataset::open;
    driver->pfnCreateCopy = PCRasterDataset::createCopy;

    GetGDALDriverManager()->RegisterDriver(driver);
  }
}
