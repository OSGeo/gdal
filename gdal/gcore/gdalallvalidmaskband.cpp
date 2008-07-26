/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALAllValidMaskBand, a class implementing all
 *           a default 'all valid' band mask.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
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

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GDALAllValidMaskBand()                        */
/************************************************************************/

GDALAllValidMaskBand::GDALAllValidMaskBand( GDALRasterBand *poParent )

{
    poDS = NULL;
    nBand = 0;

    nRasterXSize = poParent->GetXSize();
    nRasterYSize = poParent->GetYSize();

    eDataType = GDT_Byte;
    poParent->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                       ~GDALAllValidMaskBand()                        */
/************************************************************************/

GDALAllValidMaskBand::~GDALAllValidMaskBand()

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALAllValidMaskBand::IReadBlock( int nXBlockOff, int nYBlockOff,
                                         void * pImage )

{
    memset( pImage, 255, nBlockXSize * nBlockYSize );

    return CE_None;
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *GDALAllValidMaskBand::GetMaskBand()

{
    return this;
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int GDALAllValidMaskBand::GetMaskFlags()

{
    return GMF_ALL_VALID;
}
