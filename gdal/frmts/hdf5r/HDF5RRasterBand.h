/*
 * HDF5RRasterBand.h
 *
 *  Created on: May 14, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RRASTERBAND_H_
#define FRMTS_HDF5R_HDF5RRASTERBAND_H_

#include "gdal_pam.h"
#include "cpl_error.h"
#include "HDF5RDataSet.h"

class HDF5RRasterBand: public GDALPamRasterBand
{
public:
    friend class HDF5RDataSet;
    friend class HDF5RSubDataSet;

    static const int32_t HDF5R_NO_DATA_VALUE = -32768;

    HDF5RRasterBand( HDF5RDataSet* hdf5rDS,
                     int bandNum,
                     int frameNum,
                     int rows,
                     int columns,
                     GDALAccess eAcc = GA_ReadOnly );

    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void* pImage ) override;

    virtual CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void * pData ) override;

    virtual ~HDF5RRasterBand();

    virtual double GetNoDataValue( int* pbSuccess = nullptr ) override;

private:
    int frameIndex_;
};

#endif /* FRMTS_HDF5R_HDF5RRASTERBAND_H_ */
