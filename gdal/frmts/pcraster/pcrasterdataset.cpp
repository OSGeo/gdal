/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster CSF 2.0 raster file driver
 * Author:   Kor de Jong, k.dejong at geog.uu.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, Kor de Jong
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

#include "gdal_pam.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

#ifndef INCLUDED_PCRASTERDATASET
#include "pcrasterdataset.h"
#define INCLUDED_PCRASTERDATASET
#endif

#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

// PCRaster library headers.

// Module headers.
#ifndef INCLUDED_PCRASTERRASTERBAND
#include "pcrasterrasterband.h"
#define INCLUDED_PCRASTERRASTERBAND
#endif

#ifndef INCLUDED_PCRASTERUTIL
#include "pcrasterutil.h"
#define INCLUDED_PCRASTERUTIL
#endif



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

  Returns 0 if the file could not be opened.
*/
GDALDataset* PCRasterDataset::open(
         GDALOpenInfo* info)
{
  PCRasterDataset* dataset = 0;

  if(info->fp && info->nHeaderBytes >= static_cast<int>(CSF_SIZE_SIG) &&
         strncmp((char*)info->pabyHeader, CSF_SIG, CSF_SIZE_SIG) == 0) {
    MOPEN_PERM mode = info->eAccess == GA_Update
         ? M_READ_WRITE
         : M_READ;

    MAP* map = mapOpen(info->pszFilename, mode);

    if(map) {
      dataset = new PCRasterDataset(map);
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
  int nrBands = source->GetRasterCount();
  if(nrBands != 1) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Too many bands ('%d'): must be 1 band", nrBands);
    return 0;
  }

  GDALRasterBand* raster = source->GetRasterBand(1);

  // Create PCRaster raster. Determine properties of raster to create.
  size_t nrRows = raster->GetYSize();
  size_t nrCols = raster->GetXSize();
  std::string string;

  // The in-file type of the cells.
  CSF_CR fileCellRepresentation = GDALType2CellRepresentation(
         raster->GetRasterDataType(), false);

  if(fileCellRepresentation == CR_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid cell representation");
    return 0;
  }

  // The value scale of the values.
  CSF_VS valueScale = VS_UNDEFINED;
  if(source->GetMetadataItem("PCRASTER_VALUESCALE")) {
    string = source->GetMetadataItem("PCRASTER_VALUESCALE");
  }

  valueScale = !string.empty()
         ? string2ValueScale(string)
         : GDALType2ValueScale(raster->GetRasterDataType());

  if(valueScale == VS_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid value scale");
    return 0;
  }

  CSF_PT const projection = PT_YDECT2B;
  REAL8  const angle = 0.0;
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
  CSF_CR appCellRepresentation = CR_UNDEFINED;
  appCellRepresentation = GDALType2CellRepresentation(
         raster->GetRasterDataType(), true);

  if(appCellRepresentation == CR_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid cell representation");
    return 0;
  }

  // Check whether value scale fits the cell representation. Adjust when
  // needed.
  valueScale = fitValueScale(valueScale, appCellRepresentation);

  // Create a raster with the in file cell representation.
  MAP* map = Rcreate(filename, nrRows, nrCols, fileCellRepresentation,
         valueScale, projection, west, north, angle, cellSize);

  if(!map) {
    CPLError(CE_Failure, CPLE_OpenFailed,
         "PCRaster driver: Unable to create raster %s", filename);
    return 0;
  }

  // Try to convert in app cell representation to the cell representation
  // of the file.
  if(RuseAs(map, appCellRepresentation)) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot convert cells: %s", MstrError());
    Mclose(map);
    return 0;
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

  // TODO conversie van INT2 naar INT4 ondersteunen. zie ruseas.c regel 503.
  // conversie op r 159.

  // Create buffer for one row of values.
  void* buffer = Rmalloc(map, nrCols);

  // Copy values from source to target.
  CPLErr errorCode = CE_None;
  for(size_t row = 0; errorCode == CE_None && row < nrRows; ++row) {

    // Get row from source.
    if(raster->RasterIO(GF_Read, 0, row, nrCols, 1, buffer, nrCols, 1,
         raster->GetRasterDataType(), 0, 0) != CE_None) {
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

    if(!progress((row + 1) / (static_cast<double>(nrRows)), 0, progressData)) {
      CPLError(CE_Failure, CPLE_UserInterrupt,
         "PCRaster driver: User terminated CreateCopy()");
      errorCode = CE_Failure;
      break;
    }
  }

  Mclose(map);
  map = 0;

  free(buffer);
  buffer = 0;

  if( errorCode != CE_None )
      return NULL;

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
  GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( filename, GA_Update );

  if( poDS )
      poDS->CloneInfo( source, GCIF_PAM_DEFAULT );
  
  return poDS;
}



//------------------------------------------------------------------------------
// DEFINITION OF PCRDATASET MEMBERS
//------------------------------------------------------------------------------

//! Constructor.
/*!
  \param     map PCRaster map handle. It is ours to close.
*/
PCRasterDataset::PCRasterDataset(
         MAP* map)

  : GDALPamDataset(),
    d_map(map), d_west(0.0), d_north(0.0), d_cellSize(0.0)

{
  // Read header info.
  nRasterXSize = RgetNrCols(d_map);
  nRasterYSize = RgetNrRows(d_map);
  d_west = static_cast<double>(RgetXUL(d_map));
  d_north = static_cast<double>(RgetYUL(d_map));
  d_cellSize = static_cast<double>(RgetCellSize(d_map));
  d_cellRepresentation = RgetUseCellRepr(d_map);
  CPLAssert(d_cellRepresentation != CR_UNDEFINED);
  d_valueScale = RgetValueScale(d_map);
  CPLAssert(d_valueScale != VS_UNDEFINED);
  d_missingValue = ::missingValue(d_cellRepresentation);

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
    FlushCache();
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
double PCRasterDataset::missingValue() const
{
  return d_missingValue;
}



//------------------------------------------------------------------------------
// DEFINITION OF FREE OPERATORS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// DEFINITION OF FREE FUNCTIONS
//------------------------------------------------------------------------------



