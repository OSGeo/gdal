/******************************************************************************
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster CSF 2.0 raster file driver
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_pam.h"

#include "pcrasterrasterband.h"
#include "pcrasterdataset.h"
#include "pcrasterutil.h"

CPL_CVSID("$Id$")

/*!
  \file
  This file contains the implementation of the PCRasterDataset class.
*/

//------------------------------------------------------------------------------
// DEFINITION OF STATIC PCRDATASET MEMBERS
//------------------------------------------------------------------------------

//! Tries to open the file described by \a info.
/*!
  \param     info Object with information about the dataset to open.
  \return    Pointer to newly allocated GDALDataset or 0.

  Returns a nullptr if the file could not be opened.
*/
GDALDataset* PCRasterDataset::open(
         GDALOpenInfo* info)
{
  PCRasterDataset* dataset = nullptr;

  if(info->fpL && info->nHeaderBytes >= static_cast<int>(CSF_SIZE_SIG) &&
     strncmp(reinterpret_cast<char*>( info->pabyHeader ), CSF_SIG, CSF_SIZE_SIG) == 0) {
    MOPEN_PERM mode = info->eAccess == GA_Update
         ? M_READ_WRITE
         : M_READ;

    MAP* map = mapOpen(info->pszFilename, mode);

    if(map) {
      CPLErrorReset();
      dataset = new PCRasterDataset(map, info->eAccess);
      if( CPLGetLastErrorType() != CE_None )
      {
          delete dataset;
          return nullptr;
      }
    }
  }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information and overviews.                   */
/* -------------------------------------------------------------------- */
  if( dataset )
  {
      dataset->SetDescription( info->pszFilename );
      dataset->TryLoadXML();

      dataset->oOvManager.Initialize( dataset, info->pszFilename );
  }

  return dataset;
}

//! Writes a raster to \a filename as a PCRaster raster file.
/*!
  \warning   The source raster must have only 1 band. Currently, the values in
             the source raster must be stored in one of the supported cell
             representations (CR_UINT1, CR_INT4, CR_REAL4, CR_REAL8).

  The meta data item PCRASTER_VALUESCALE will be checked to see what value
  scale to use. Otherwise a value scale is determined using
  GDALType2ValueScale(GDALDataType).

  This function always writes rasters using CR_UINT1, CR_INT4 or CR_REAL4
  cell representations.
*/
GDALDataset* PCRasterDataset::createCopy(
    char const* filename,
    GDALDataset* source,
    CPL_UNUSED int strict,
    CPL_UNUSED char** options,
    GDALProgressFunc progress,
    void* progressData)
{
  // Checks.
  const int nrBands = source->GetRasterCount();
  if(nrBands != 1) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Too many bands ('%d'): must be 1 band", nrBands);
    return nullptr;
  }

  GDALRasterBand* raster = source->GetRasterBand(1);

  // Create PCRaster raster. Determine properties of raster to create.

  // The in-file type of the cells.
  CSF_CR fileCellRepresentation = GDALType2CellRepresentation(
         raster->GetRasterDataType(), false);

  if(fileCellRepresentation == CR_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid cell representation");
    return nullptr;
  }

  // The value scale of the values.
  CSF_VS valueScale = VS_UNDEFINED;
  std::string osString;
  if(source->GetMetadataItem("PCRASTER_VALUESCALE")) {
    osString = source->GetMetadataItem("PCRASTER_VALUESCALE");
  }

  valueScale = !osString.empty()
         ? string2ValueScale(osString)
         : GDALType2ValueScale(raster->GetRasterDataType());

  if(valueScale == VS_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid value scale");
    return nullptr;
  }

  CSF_PT const projection = PT_YDECT2B;
  const REAL8 angle = 0.0;
  REAL8 west = 0.0;
  REAL8 north = 0.0;
  REAL8 cellSize = 1.0;

  double transform[6];
  if(source->GetGeoTransform(transform) == CE_None) {
    if(transform[2] == 0.0 && transform[4] == 0.0) {
      west = static_cast<REAL8>(transform[0]);
      north = static_cast<REAL8>(transform[3]);
      cellSize = static_cast<REAL8>(transform[1]);
    }
  }

  // The in-memory type of the cells.
  CSF_CR appCellRepresentation = GDALType2CellRepresentation(
         raster->GetRasterDataType(), true);

  if(appCellRepresentation == CR_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid cell representation");
    return nullptr;
  }

  // Check whether value scale fits the cell representation. Adjust when
  // needed.
  valueScale = fitValueScale(valueScale, appCellRepresentation);

  // Create a raster with the in file cell representation.
  const size_t nrRows = raster->GetYSize();
  const size_t nrCols = raster->GetXSize();
  MAP* map = Rcreate(filename, nrRows, nrCols, fileCellRepresentation,
         valueScale, projection, west, north, angle, cellSize);

  if(!map) {
    CPLError(CE_Failure, CPLE_OpenFailed,
         "PCRaster driver: Unable to create raster %s", filename);
    return nullptr;
  }

  // Try to convert in app cell representation to the cell representation
  // of the file.
  if(RuseAs(map, appCellRepresentation)) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot convert cells: %s", MstrError());
    Mclose(map);
    return nullptr;
  }

  int hasMissingValue;
  double missingValue = raster->GetNoDataValue(&hasMissingValue);

  // This is needed to get my (KDJ) unit tests running.
  // I am still uncertain why this is needed. If the input raster has float32
  // values and the output int32, than the missing value in the dataset object
  // is not updated like the values are.
  if(missingValue == ::missingValue(CR_REAL4) &&
         fileCellRepresentation == CR_INT4) {
    missingValue = ::missingValue(fileCellRepresentation);
  }

  // TODO: Proper translation of TODO.
  // TODO: Support conversion to INT2 (?) INT4. ruseas.c see line 503.
  // Conversion r 159.

  // Create buffer for one row of values.
  void* buffer = Rmalloc(map, nrCols);

  // Copy values from source to target.
  CPLErr errorCode = CE_None;
  for(size_t row = 0; errorCode == CE_None && row < nrRows; ++row) {

    // Get row from source.
    if(raster->RasterIO(GF_Read, 0, static_cast<int>(row),
        static_cast<int>(nrCols), 1, buffer,
        static_cast<int>(nrCols), 1,
         raster->GetRasterDataType(), 0, 0, nullptr) != CE_None) {
      CPLError(CE_Failure, CPLE_FileIO,
         "PCRaster driver: Error reading from source raster");
      errorCode = CE_Failure;
      break;
    }

    // Upon reading values are converted to the
    // right data type. This includes the missing value. If the source
    // value cannot be represented in the target data type it is set to a
    // missing value.

    if(hasMissingValue) {
      alterToStdMV(buffer, nrCols, appCellRepresentation, missingValue);
    }

    if(valueScale == VS_BOOLEAN) {
      castValuesToBooleanRange(buffer, nrCols, appCellRepresentation);
    }

    // Write row in target.
    RputRow(map, row, buffer);

    if(!progress((row + 1) / (static_cast<double>(nrRows)), nullptr, progressData)) {
      CPLError(CE_Failure, CPLE_UserInterrupt,
         "PCRaster driver: User terminated CreateCopy()");
      errorCode = CE_Failure;
      break;
    }
  }

  Mclose(map);
  map = nullptr;

  free(buffer);
  buffer = nullptr;

  if( errorCode != CE_None )
      return nullptr;

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxiliary pam information.        */
/* -------------------------------------------------------------------- */
  GDALPamDataset *poDS = reinterpret_cast<GDALPamDataset *>(
      GDALOpen( filename, GA_Update ) );

  if( poDS )
      poDS->CloneInfo( source, GCIF_PAM_DEFAULT );

  return poDS;
}

//------------------------------------------------------------------------------
// DEFINITION OF PCRDATASET MEMBERS
//------------------------------------------------------------------------------

//! Constructor.
/*!
  \param     mapIn PCRaster map handle. It is ours to close.
*/
PCRasterDataset::PCRasterDataset( MAP* mapIn, GDALAccess eAccessIn ) :
    GDALPamDataset(),
    d_map(mapIn),
    d_west(0.0),
    d_north(0.0),
    d_cellSize(0.0),
    d_cellRepresentation(CR_UNDEFINED),
    d_valueScale(VS_UNDEFINED),
    d_defaultNoDataValue(0.0),
    d_location_changed(false)
{
  // Read header info.
  eAccess = eAccessIn;
  nRasterXSize = static_cast<int>(RgetNrCols(d_map));
  nRasterYSize = static_cast<int>(RgetNrRows(d_map));
  if( !GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize) )
  {
      return;
  }
  d_west = static_cast<double>(RgetXUL(d_map));
  d_north = static_cast<double>(RgetYUL(d_map));
  d_cellSize = static_cast<double>(RgetCellSize(d_map));
  d_cellRepresentation = RgetUseCellRepr(d_map);
  if( d_cellRepresentation == CR_UNDEFINED )
  {
      CPLError(CE_Failure, CPLE_AssertionFailed, "d_cellRepresentation != CR_UNDEFINED");
  }
  d_valueScale = RgetValueScale(d_map);
  if( d_valueScale == VS_UNDEFINED )
  {
      CPLError(CE_Failure, CPLE_AssertionFailed, "d_valueScale != VS_UNDEFINED");
  }
  d_defaultNoDataValue = ::missingValue(d_cellRepresentation);

  // Create band information objects.
  nBands = 1;
  SetBand(1, new PCRasterRasterBand(this));

  SetMetadataItem("PCRASTER_VALUESCALE", valueScale2String(
         d_valueScale).c_str());
}

//! Destructor.
/*!
  \warning   The map given in the constructor is closed.
*/
PCRasterDataset::~PCRasterDataset()
{
    FlushCache(true);
    Mclose(d_map);
}

//! Sets projections info.
/*!
  \param     transform Array to fill.

  CSF 2.0 supports the notion of y coordinates which increase from north to
  south. Support for this has been dropped and applications reading PCRaster
  rasters will treat or already treat y coordinates as increasing from south
  to north only.
*/
CPLErr PCRasterDataset::GetGeoTransform(double* transform)
{
  // x = west + nrCols * cellsize
  transform[0] = d_west;
  transform[1] = d_cellSize;
  transform[2] = 0.0;

  // y = north + nrRows * -cellsize
  transform[3] = d_north;
  transform[4] = 0.0;
  transform[5] = -1.0 * d_cellSize;

  return CE_None;
}

//! Returns the map handle.
/*!
  \return    Map handle.
*/
MAP* PCRasterDataset::map() const
{
  return d_map;
}

//! Returns the in-app cell representation.
/*!
  \return    cell representation
  \warning   This might not be the same representation as use to store the values in the file.
  \sa        valueScale()
*/
CSF_CR PCRasterDataset::cellRepresentation() const
{
  return d_cellRepresentation;
}

//! Returns the value scale of the data.
/*!
  \return    Value scale
  \sa        cellRepresentation()
*/
CSF_VS PCRasterDataset::valueScale() const
{
  return d_valueScale;
}

//! Returns the value of the missing value.
/*!
  \return    Missing value
*/
double PCRasterDataset::defaultNoDataValue() const
{
  return d_defaultNoDataValue;
}

GDALDataset* PCRasterDataset::create(
     const char* filename,
     int nr_cols,
     int nr_rows,
     int nrBands,
     GDALDataType gdalType,
     char** papszParamList)
{
  // Checks
  if(nrBands != 1){
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver : "
         "attempt to create dataset with too many bands (%d); "
         "must be 1 band.\n", nrBands);
    return nullptr;
  }

  const int row_col_max = INT4_MAX - 1;
  if(nr_cols > row_col_max){
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver : "
         "attempt to create dataset with too many columns (%d); "
         "must be smaller than %d.", nr_cols, row_col_max);
    return nullptr;
  }

  if(nr_rows > row_col_max){
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver : "
         "attempt to create dataset with too many rows (%d); "
         "must be smaller than %d.", nr_rows, row_col_max);
    return nullptr;
  }

  if(gdalType != GDT_Byte &&
     gdalType != GDT_Int32 &&
     gdalType != GDT_Float32){
     CPLError( CE_Failure, CPLE_AppDefined,
       "PCRaster driver: "
       "attempt to create dataset with an illegal data type (%s); "
       "use either Byte, Int32 or Float32.",
       GDALGetDataTypeName(gdalType));
    return nullptr;
  }

  // value scale must be specified by the user,
  // determines cell representation
  const char *valueScale = CSLFetchNameValue(
    papszParamList,"PCRASTER_VALUESCALE");

  if(valueScale == nullptr){
    CPLError(CE_Failure, CPLE_AppDefined,
         "PCRaster driver: value scale can not be determined; "
         "specify PCRASTER_VALUESCALE.");
    return nullptr;
  }

  CSF_VS csf_value_scale = string2ValueScale(valueScale);

  if(csf_value_scale == VS_UNDEFINED){
    CPLError( CE_Failure, CPLE_AppDefined,
         "PCRaster driver: value scale can not be determined (%s); "
         "use either VS_BOOLEAN, VS_NOMINAL, VS_ORDINAL, VS_SCALAR, "
         "VS_DIRECTION, VS_LDD",
          valueScale);
    return nullptr;
  }

  CSF_CR csf_cell_representation = GDALType2CellRepresentation(gdalType, false);

  // default values
  REAL8 west = 0.0;
  REAL8 north = 0.0;
  REAL8 length = 1.0;
  REAL8 angle = 0.0;
  CSF_PT projection = PT_YDECT2B;

  // Create a new raster
  MAP* map = Rcreate(filename, nr_rows, nr_cols, csf_cell_representation,
         csf_value_scale, projection, west, north, angle, length);

  if(!map){
    CPLError(CE_Failure, CPLE_OpenFailed,
         "PCRaster driver: Unable to create raster %s", filename);
    return nullptr;
  }

  Mclose(map);
  map = nullptr;

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxiliary pam information.        */
/* -------------------------------------------------------------------- */
  GDALPamDataset *poDS = reinterpret_cast<GDALPamDataset *>(
      GDALOpen(filename, GA_Update) );

  return poDS;
}

CPLErr PCRasterDataset::SetGeoTransform(double* transform)
{
  if((transform[2] != 0.0) || (transform[4] != 0.0)) {
    CPLError(CE_Failure, CPLE_NotSupported,
             "PCRaster driver: "
             "rotated geotransformations are not supported.");
    return CE_Failure;
  }

  if(transform[1] != transform[5] * -1.0 ) {
    CPLError(CE_Failure, CPLE_NotSupported,
             "PCRaster driver: "
             "only the same width and height for cells is supported." );
    return CE_Failure;
  }

  d_west = transform[0];
  d_north = transform[3];
  d_cellSize = transform[1];
  d_location_changed = true;

  return CE_None;
}

bool PCRasterDataset::location_changed() const {
  return d_location_changed;
}
