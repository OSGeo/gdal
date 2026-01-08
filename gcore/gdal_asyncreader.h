/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALAsyncReader base class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALASYNCREADER_H_INCLUDED
#define GDALASYNCREADER_H_INCLUDED

#include "cpl_port.h"

#include "gdal.h"

class GDALDataset;

/* ******************************************************************** */
/*                          GDALAsyncReader                             */
/* ******************************************************************** */

/**
 * Class used as a session object for asynchronous requests.  They are
 * created with GDALDataset::BeginAsyncReader(), and destroyed with
 * GDALDataset::EndAsyncReader().
 */
class CPL_DLL GDALAsyncReader
{

    CPL_DISALLOW_COPY_ASSIGN(GDALAsyncReader)

  protected:
    //! @cond Doxygen_Suppress
    GDALDataset *poDS;
    int nXOff;
    int nYOff;
    int nXSize;
    int nYSize;
    void *pBuf;
    int nBufXSize;
    int nBufYSize;
    GDALDataType eBufType;
    int nBandCount;
    int *panBandMap;
    int nPixelSpace;
    int nLineSpace;
    int nBandSpace;
    //! @endcond

  public:
    GDALAsyncReader();
    virtual ~GDALAsyncReader();

    /** Return dataset.
     * @return dataset
     */
    GDALDataset *GetGDALDataset()
    {
        return poDS;
    }

    /** Return x offset.
     * @return x offset.
     */
    int GetXOffset() const
    {
        return nXOff;
    }

    /** Return y offset.
     * @return y offset.
     */
    int GetYOffset() const
    {
        return nYOff;
    }

    /** Return width.
     * @return width
     */
    int GetXSize() const
    {
        return nXSize;
    }

    /** Return height.
     * @return height
     */
    int GetYSize() const
    {
        return nYSize;
    }

    /** Return buffer.
     * @return buffer
     */
    void *GetBuffer()
    {
        return pBuf;
    }

    /** Return buffer width.
     * @return buffer width.
     */
    int GetBufferXSize() const
    {
        return nBufXSize;
    }

    /** Return buffer height.
     * @return buffer height.
     */
    int GetBufferYSize() const
    {
        return nBufYSize;
    }

    /** Return buffer data type.
     * @return buffer data type.
     */
    GDALDataType GetBufferType() const
    {
        return eBufType;
    }

    /** Return band count.
     * @return band count
     */
    int GetBandCount() const
    {
        return nBandCount;
    }

    /** Return band map.
     * @return band map.
     */
    int *GetBandMap()
    {
        return panBandMap;
    }

    /** Return pixel spacing.
     * @return pixel spacing.
     */
    int GetPixelSpace() const
    {
        return nPixelSpace;
    }

    /** Return line spacing.
     * @return line spacing.
     */
    int GetLineSpace() const
    {
        return nLineSpace;
    }

    /** Return band spacing.
     * @return band spacing.
     */
    int GetBandSpace() const
    {
        return nBandSpace;
    }

    virtual GDALAsyncStatusType
    GetNextUpdatedRegion(double dfTimeout, int *pnBufXOff, int *pnBufYOff,
                         int *pnBufXSize, int *pnBufYSize) = 0;
    virtual int LockBuffer(double dfTimeout = -1.0);
    virtual void UnlockBuffer();
};

#endif
