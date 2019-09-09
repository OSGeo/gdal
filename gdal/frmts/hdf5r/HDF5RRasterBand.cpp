/*
 * HDF5RRasterBand.cpp
 *
 *  Created on: May 14, 2018
 *      Author: nielson
 */
#include <iostream>

#include "HDF5RRasterBand.h"

#define HDF5R_DRVR_DEBUG

//******************************************************************************
// Constructor
//******************************************************************************
HDF5RRasterBand::HDF5RRasterBand( HDF5RDataSet* hdf5rDS,
                                  int bandNum,
                                  int frameIndex,
                                  int rows,
                                  int columns,
                                  GDALAccess eAcc )
: frameIndex_( frameIndex )
{
    // As recommended by the GDAL Driver Tutorial, base attributes are loaded
    poDS = hdf5rDS;
    nBand = bandNum;
    eDataType = GDT_Int32;
    nBlockYSize = rows;
    nBlockXSize = columns;
    eAccess = eAcc;
}

//******************************************************************************
// Destructor
//******************************************************************************
HDF5RRasterBand::~HDF5RRasterBand()
{
    // as recommended by https://www.gdal.org/gdal_drivertut.html
    // call the FlushCache() method
    FlushCache();
}

//******************************************************************************
// IReadBlock
// This method reads a full frame of HDF5-R imagery
//******************************************************************************
CPLErr HDF5RRasterBand::IReadBlock( int nBlockXOff,
                                    int nBlockYOff,
                                    void* pImage )
{
    CPLDebug( HDF5R_DEBUG_STR,
              "HDF5RRasterBand::IReadBlock called. row=y=%d col=x=%d",
              nBlockYOff, nBlockXOff );

    // cast the base class pointer to what was loaded in the constructor
    HDF5RDataSet* hdf5rDS = static_cast<HDF5RDataSet*>( poDS );

    if (hdf5rDS->hdf5rReader_ == nullptr)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "HDF5RRasterBand::IReadBlock called with null Hdf5rReader pointer!" );
        return CE_Failure;
    }

    // handle case where IReadBlock is called after Create() in R/W mode
    // in this case we return a null pImage and a good return code
    if ((eAccess == GA_Update) && !hdf5rDS->hdf5rReader_->haveCalRawData())
    {
        pImage = nullptr;
        return CE_None;
    }

    // read the image frame.
    //    At this time the HDF5-R file is open as are the
    //       CalRawData, and LOS Grid
    //    Off-Earth pixels are blanked (nodata) if requested
    return hdf5rDS->hdf5rReader_->readBlock( frameIndex_,
                                             nBlockYOff, nBlockXOff,
                                             HDF5R_NO_DATA_VALUE,
                                             pImage );
}

//******************************************************************************
// IWriteBlock
//******************************************************************************
CPLErr HDF5RRasterBand::IWriteBlock( int nBlockXOff,
                                     int nBlockYOff,
                                     void *pData )
{
    CPLDebug( HDF5R_DEBUG_STR,
              "HDF5RRasterBand::IWriteBlock called. nBlockXOff=%d nBlockYOff=%d",
              nBlockXOff, nBlockYOff );

    // cast the base class pointer to what was loaded in the constructor
    HDF5RDataSet* hdf5rDS = static_cast<HDF5RDataSet*>( poDS );

    hdf5rDS->hdf5rWriter_->writeImage( nRasterYSize,
                                       nRasterXSize,
                                       static_cast<const int32_t*>(pData) );

    return CE_None;
}

//******************************************************************************
// Return the GDAL 'NODATA' value for HDF5-R
//     - The ICD does not specify a value (it probably should)
//     - Used by the driver to blank off-Earth data points
//******************************************************************************
double HDF5RRasterBand::GetNoDataValue( int* pbSuccess )
{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;
    return double( HDF5R_NO_DATA_VALUE );
}
