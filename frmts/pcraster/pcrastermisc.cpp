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
#ifndef INCLUDED_CPL_STRING
#include "cpl_string.h"
#define INCLUDED_CPL_STRING
#endif

#ifndef INCLUDED_GDAL_PRIV
#include "gdal_priv.h"
#define INCLUDED_GDAL_PRIV
#endif

#ifndef INCLUDED_PCRASTERDATASET
#include "pcrasterdataset.h"
#define INCLUDED_PCRASTERDATASET
#endif

#ifndef INCLUDED_PCRASTERUTIL
#include "pcrasterutil.h"
#define INCLUDED_PCRASTERUTIL
#endif



/*
static GDALDataset* PCRCreateCopy(const char* fileName, GDALDataset* source,
         int strict, char** options, GDALProgressFunc progress,
         void* progressData)
{
  // Checks.
  int nrBands = source->GetRasterCount();
  if(nrBands != 1) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Too many bands ('%d'): must be 1 band.", nrBands);
    return 0;
  }

  GDALRasterBand* raster = source->GetRasterBand(1);

  // Create PCRaster raster.
  size_t nrRows = raster->GetYSize();
  size_t nrCols = raster->GetXSize();
  std::string string;

  CSF_CR mapCellRepresentation = CR_UNDEFINED;
  if(CSLFetchNameValue(options, "PCR_CELLREPRESENTATION")) {
    string = CSLFetchNameValue(options, "PCR_CELLREPRESENTATION");
  }
  if(!string.empty()) {
    mapCellRepresentation = string2PCRCellRepresentation(string);
  }
  else {
    mapCellRepresentation = GDALType2PCRCellRepresentation(
         raster->GetRasterDataType(), false);
  }

  if(mapCellRepresentation == CR_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid cell representation.");
    return 0;
  }

  CSF_VS valueScale = VS_UNDEFINED;
  if(CSLFetchNameValue(options, "PCR_VALUESCALE")) {
    string = CSLFetchNameValue(options, "PCR_VALUESCALE");
  }
  if(!string.empty()) {
    valueScale = string2PCRValueScale(string);
  }
  else {
    valueScale = GDALType2PCRValueScale(raster->GetRasterDataType());
  }

  if(valueScale == VS_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid value scale.");
    return 0;
  }

  CSF_PT projection = PT_UNDEFINED;
  REAL8 left = 0.0;
  REAL8 top = 0.0;
  REAL8 angle = 0.0;
  REAL8 cellSize = 1.0;

  double transform[6];
  if(source->GetGeoTransform(transform) == CE_None) {
    if(transform[2] == 0.0 and transform[4] == 0.0) {
      projection = (transform[5] > 0.0) ? PT_YINCT2B : PT_YDECT2B;
      left = static_cast<REAL8>(transform[0]);
      top = static_cast<REAL8>(transform[3]);
      cellSize = static_cast<REAL8>(transform[1]);
    }
  }

  MAP* map = Rcreate(fileName , nrRows, nrCols, mapCellRepresentation,
                 valueScale, projection, left, top, angle, cellSize);

  if(!map) {
    CPLError(CE_Failure, CPLE_OpenFailed,
         "Unable to create PCRaster raster %s.\n", fileName);
    return 0;
  }

  // Determine in app cell representation.
  CSF_CR appCellRepresentation = CR_UNDEFINED;
  appCellRepresentation = GDALType2PCRCellRepresentation(
         raster->GetRasterDataType(), true);

  if(appCellRepresentation == CR_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid cell representation.");
    return 0;
  }

  if(RuseAs(map, appCellRepresentation)) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot convert cell representations.");
    return 0;
  }

  // Create buffer for one row of values.
  // TODO: maak buffer die zo groot is als max(app, mapCellRepresenation)
  // zie RmapAlloc oid
  // TODO: conversie van INT2 naar INT4 ondersteunen. zie ruseas.c regel 503.
  // conversie op r 159.
  void* buffer = createBuffer(nrCols, appCellRepresentation);

  // Copy values from source to target.
  CPLErr errorCode = CE_None;
  for(size_t row = 0; errorCode == CE_None && row < nrRows; ++row) {

    // Get row from source.
    errorCode = raster->RasterIO(GF_Read, 0, row, nrCols, 1, buffer, nrCols, 1,
         raster->GetRasterDataType(), 0, 0);

    for(size_t col = 0; col < nrCols; ++col) {
      std::cout << ((INT2*)buffer)[col] << " " << std::ends;
    }

    if(errorCode != CE_None) {
      CPLError(CE_Failure, CPLE_FileIO,
         "PCRaster driver: Error reading from source raster.");
    }

    // int success;
    // convertMissingValues(buffer, nrRows, appCellRepresentation,
    //      raster->GetNoDataValue(&success));

    // Write row in target.
    RputRow(map, row, buffer);

    if(errorCode == CE_None &&
         !progress(
           (row + 1) / (static_cast<double>(nrRows)), 0, progressData)) {
      CPLError( CE_Failure, CPLE_UserInterrupt,
         "PCRaster driver: User terminated CreateCopy().");
    }
  }

  Mclose(map); map = 0;
  deleteBuffer(buffer, appCellRepresentation); buffer = 0;

  if(errorCode != CE_None) {
    return 0;
  }
  else {
    return static_cast<GDALDataset*>(GDALOpen(fileName, GA_ReadOnly));
  }
}
*/



CPL_C_START
void               GDALRegister_PCRaster(void);
CPL_C_END



void GDALRegister_PCRaster()
{
  if(!GDALGetDriverByName("PCRaster")) {

    GDALDriver* driver = new GDALDriver();

    driver->SetDescription("PCRaster");
    driver->SetMetadataItem(GDAL_DMD_LONGNAME, "PCRaster Raster File");
    // driver->SetMetadataItem(GDAL_DMD_EXTENSION, ".map");
    driver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int32 Float32");

    driver->pfnOpen = PCRasterDataset::Open;
    // driver->pfnCreateCopy = PCRCreateCopy;

    GetGDALDriverManager()->RegisterDriver(driver);
  }
}
