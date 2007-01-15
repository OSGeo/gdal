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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.10  2006/10/03 14:07:44  dron
 * Rename open() function to mapOpen() to avoid clashing with system functions.
 *
 * Revision 1.9  2005/05/05 15:54:49  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.8  2004/11/25 15:00:41  kdejong
 * Replace and by &&, moved free out of std namespace
 *
 * Revision 1.7  2004/11/22 10:40:23  kdejong
 * Added PCRasterRasterBand::Minimum and Maximum. Improved documentation, layout. Removed unused code. Layout.
 *
 * Revision 1.6  2004/11/13 19:00:55  fwarmerdam
 * Don't blow an assertion if we don't have enough header data, just
 * return NULL (in mapOpen()).
 *
 * Revision 1.5  2004/11/13 12:08:44  kdejong
 * Reading files with other cell representations than UINT1, INT4 or REAL4 will keep their original cell representation in memory.
 *
 * Revision 1.4  2004/11/11 15:50:36  kdejong
 * Added write support, docs, improvements.
 *
 * Revision 1.3  2004/11/10 10:21:42  kdejong
 * *** empty log message ***
 *
 * Revision 1.2  2004/11/10 10:09:19  kdejong
 * Initial versions. Read only driver.
 *
 * Revision 1.1  2004/10/22 14:19:27  fwarmerdam
 * New
 *
 */

#include "gdal_pam.h"

CPL_CVSID("$Id$");

#ifndef INCLUDED_PCRASTERDATASET
#include "pcrasterdataset.h"
#define INCLUDED_PCRASTERDATASET
#endif

// Library headers.
#ifndef INCLUDED_CASSERT
#include <cassert>
#define INCLUDED_CASSERT
#endif

#ifndef INCLUDED_CSTDLIB
#include <cstdlib>
#define INCLUDED_CSTDLIB
#endif

#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif

#ifndef INCLUDED_CPL_STRING
#include "cpl_string.h"
#define INCLUDED_CPL_STRING
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
GDALDataset* PCRasterDataset::open(GDALOpenInfo* info)
{
  PCRasterDataset* dataset = 0;

  if(info->fp && info->nHeaderBytes >= static_cast<int>(CSF_SIZE_SIG) &&
         strncmp((char*)info->pabyHeader, CSF_SIG, CSF_SIZE_SIG) == 0) {
    MOPEN_PERM mode = M_READ;
    if(info->eAccess == GA_Update) {
      mode = M_READ_WRITE;
    }

    MAP* map = mapOpen(info->pszFilename, mode);

    if(map) {
      dataset = new PCRasterDataset(map);
    }
  }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
  if( dataset )
  {
      dataset->SetDescription( info->pszFilename );
      dataset->TryLoadXML();
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

  This function alwasy writes raster using CR_UINT1, CR_INT4 or CR_REAL4
  cell representations.
*/
GDALDataset* PCRasterDataset::createCopy(char const* filename,
         GDALDataset* source, int strict, char** options,
         GDALProgressFunc progress, void* progressData)
{
  // Checks.
  int nrBands = source->GetRasterCount();
  if(nrBands != 1) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Too many bands ('%d'): must be 1 band", nrBands);
    return 0;
  }

  GDALRasterBand* raster = source->GetRasterBand(1);

  // Create PCRaster raster.
  size_t nrRows = raster->GetYSize();
  size_t nrCols = raster->GetXSize();
  std::string string;

  CSF_CR fileCellRepresentation = GDALType2CellRepresentation(
         raster->GetRasterDataType(), false);

  if(fileCellRepresentation == CR_UNDEFINED) {
    CPLError(CE_Failure, CPLE_NotSupported,
         "PCRaster driver: Cannot determine a valid cell representation");
    return 0;
  }

  CSF_VS valueScale = VS_UNDEFINED;
  if(source->GetMetadataItem("PCRASTER_VALUESCALE")) {
    string = source->GetMetadataItem("PCRASTER_VALUESCALE");
  }
  if(!string.empty()) {
    valueScale = string2ValueScale(string);
  }
  else {
    valueScale = GDALType2ValueScale(raster->GetRasterDataType());
  }

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

  // Determine in app cell representation.
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
    return 0;
  }

  int hasMissingValue;
  double missingValue = raster->GetNoDataValue(&hasMissingValue);

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
      free(buffer);
      CPLError(CE_Failure, CPLE_FileIO,
         "PCRaster driver: Error reading from source raster");
    }

    if(hasMissingValue) {
      alterToStdMV(buffer, nrCols, appCellRepresentation, missingValue);
    }

    // Write row in target.
    RputRow(map, row, buffer);

    if(!progress((row + 1) / (static_cast<double>(nrRows)), 0, progressData)) {
      free(buffer);
      CPLError(CE_Failure, CPLE_UserInterrupt,
         "PCRaster driver: User terminated CreateCopy()");
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
PCRasterDataset::PCRasterDataset(MAP* map)

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
  assert(d_cellRepresentation != CR_UNDEFINED);
  d_valueScale = RgetValueScale(d_map);
  assert(d_valueScale != VS_UNDEFINED);
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



