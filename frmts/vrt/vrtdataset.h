/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Declaration of virtual gdal dataset classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.1  2001/11/16 21:14:31  warmerda
 * New
 *
 */

#ifndef VIRTUALDATASET_H_INCLUDED
#define VIRTUALDATASET_H_INCLUDED

#include "gdal_priv.h"
#include "cpl_minixml.h"

CPL_C_START
void	GDALRegister_VRT(void);
CPL_C_END

/************************************************************************/
/*				MEMDataset				*/
/************************************************************************/

class VRTRasterBand;

class CPL_DLL VRTDataset : public GDALDataset
{
  public:
                 VRTDataset(int nXSize, int nYSize);
                ~VRTDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            VRTRasterBand                             */
/************************************************************************/

class VRTSimpleSource;

class CPL_DLL VRTRasterBand : public GDALRasterBand
{
    int		   nSources;
    VRTSimpleSource **papoSources;
    void           Initialize( int nXSize, int nYSize );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

  public:

    		   VRTRasterBand( GDALDataset *poDS, int nBand );
                   VRTRasterBand( GDALDataType eType, 
                                  int nXSize, int nYSize );
    virtual        ~VRTRasterBand();

    CPLErr         XMLInit( CPLXMLNode * );

    CPLErr         AddSimpleSource( GDALRasterBand *poSrcBand, 
                                    int nSrcXOff=-1, int nSrcYOff=-1, 
                                    int nSrcXSize=-1, int nSrcYSize=-1, 
                                    int nDstXOff=-1, int nDstYOff=-1, 
                                    int nDstXSize=-1, int nDstYSize=-1 );

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
};

#endif /* ndef VIRTUALDATASET_H_INCLUDED */
