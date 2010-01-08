/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Private Declarations for OGR free VFK Reader code.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#ifndef GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADERP_H_INCLUDED

#include <map>
#include <string>

#include "vfkreader.h"
#include "ogr_api.h"

class VFKReader;

/************************************************************************/
/*                              VFKReader                               */
/************************************************************************/
class VFKReader:public IVFKReader 
{
private:
    char          *m_pszFilename;

    /* data buffer */
    char          *m_pszWholeText;

    /* data blocks */
    int            m_nDataBlockCount;
    VFKDataBlock **m_papoDataBlock;

    /* metadata */
    std::map<std::string, std::string> poInfo;

    int  AddDataBlock(VFKDataBlock *);
    void AddInfo(const char *);

public:
    VFKReader();
    virtual ~VFKReader();

    void          SetSourceFile(const char *);

    int           LoadData();
    int           LoadDataBlocks();
    long          LoadGeometry();
    
    int           GetDataBlockCount() const { return m_nDataBlockCount; }
    VFKDataBlock *GetDataBlock(int) const;
    VFKDataBlock *GetDataBlock(const char *) const;

    const char   *GetInfo(const char *);
};

#endif // GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
