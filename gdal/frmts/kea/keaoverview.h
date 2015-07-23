/*
 * $Id$
 *  keaoverview.h
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 *  Permission is hereby granted, free of charge, to any person 
 *  obtaining a copy of this software and associated documentation 
 *  files (the "Software"), to deal in the Software without restriction, 
 *  including without limitation the rights to use, copy, modify, 
 *  merge, publish, distribute, sublicense, and/or sell copies of the 
 *  Software, and to permit persons to whom the Software is furnished 
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be 
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR 
 *  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
 *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef KEAOVERVIEW_H
#define KEAOVERVIEW_H

#include "cpl_port.h"
#if defined(USE_GCC_VISIBILITY_FLAG) && !defined(DllExport)
#define DllExport CPL_DLL
#endif
#include "keaband.h"

// overview class. Derives from our band class
// and just overrited and read/write block functions
class KEAOverview : public KEARasterBand
{
    int         m_nOverviewIndex; // the index of this overview
public:
    KEAOverview(KEADataset *pDataset, int nSrcBand, GDALAccess eAccess, 
                kealib::KEAImageIO *pImageIO, int *pRefCount,
                int nOverviewIndex, uint64_t nXSize, uint64_t nYSize );
    ~KEAOverview();

    // virtual methods for RATs - not implemented for overviews
    GDALRasterAttributeTable *GetDefaultRAT();

    CPLErr SetDefaultRAT(const GDALRasterAttributeTable *poRAT);

    // note that Color Table stuff implemented in base class
    // so could be some duplication if overview asked for color table

protected:
    // we just override these functions from KEARasterBand
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
};

#endif //KEAOVERVIEW_H
