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
 * Revision 1.2  2004/11/10 10:09:19  kdejong
 * Initial versions. Read only driver.
 *
 * Revision 1.1  2004/10/22 14:19:27  fwarmerdam
 * New
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/*
CPL_C_START
void	GDALRegister_PCRaster(void);
CPL_C_END
*/

/************************************************************************/
/*                       GDALRegister_PCRaster()                        */
/************************************************************************/

/*
void GDALRegister_PCRaster()

{
#ifdef notdef
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PCRaster" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PCRaster" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "PCRaster Image File" );

        poDriver->pfnOpen = PCRasterDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
#endif
}
*/



#ifndef INCLUDED_PCRASTERDATASET
#include "pcrasterdataset.h"
#define INCLUDED_PCRASTERDATASET
#endif

// Library headers.
#ifndef INCLUDED_CASSERT
#include <cassert>
#define INCLUDED_CASSERT
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

//!
/*!
  \param     .
  \return    .
  \exception .
  \warning   .
  \sa        .
  \todo      What about read only / read write access?
*/
GDALDataset* PCRasterDataset::Open(GDALOpenInfo* info)
{
  if(!info->fp) {
    return 0;
  }

  MAP* map = open(info->pszFilename, M_READ);

  // Create a PCRasterDataset.
  PCRasterDataset* dataset = new PCRasterDataset(map);

  return dataset;
}



//------------------------------------------------------------------------------
// DEFINITION OF PCRDATASET MEMBERS
//------------------------------------------------------------------------------

//! Constructor.
/*!
  \param     map PCRaster map handle. It is ours to close.
*/
PCRasterDataset::PCRasterDataset(MAP* map)

  : GDALDataset(),
    d_map(map), d_west(0.0), d_north(0.0), d_cellSize(0.0)

{
  // Read header info.
  nRasterXSize = RgetNrCols(d_map);
  nRasterYSize = RgetNrRows(d_map);
  d_west = static_cast<double>(RgetXUL(d_map));
  d_north = static_cast<double>(RgetYUL(d_map));
  d_cellSize = static_cast<double>(RgetCellSize(d_map));
  d_cellRepresentation = RgetUseCellRepr(d_map);
  d_missingValue = ::missingValue(d_cellRepresentation);

  // Create band information objects.
  nBands = 1;
  SetBand(1, new PCRasterRasterBand(this));
}



//! Destructor.
/*!
  \warning   The map given in the constructor is closed.
*/
PCRasterDataset::~PCRasterDataset()
{
  Mclose(d_map);
}



CPLErr PCRasterDataset::GetGeoTransform(double* transform)
{
  // x = left + nrCols * cellsize
  transform[0] = d_west;
  transform[1] = d_cellSize;
  transform[2] = 0.0;

  // y = top + nrRows * -cellsize
  transform[3] = d_north;
  transform[4] = 0.0;
  transform[5] = d_cellSize;

  return CE_None;
}



/*
void PCRasterDataset::determineMissingValue()
{
  CSF_CR type = RgetUseCellRepr(d_map);

  if(type == CR_UNDEFINED) {
    // TODO: error
  }
  else {
    d_missingValue = ::missingValue(type);
  }
}
*/



//! Returns the map handle.
/*!
  \return    Map handle.
*/
MAP* PCRasterDataset::map() const
{
  return d_map;
}



CSF_CR PCRasterDataset::cellRepresentation() const
{
  return d_cellRepresentation;
}



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



