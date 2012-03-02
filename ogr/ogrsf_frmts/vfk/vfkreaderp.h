/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Private Declarations for OGR free VFK Reader code.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012, Martin Landa <landa.martin gmail.com>
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

#ifdef HAVE_SQLITE
#include "sqlite3.h"
#endif

class VFKReader;

/************************************************************************/
/*                              VFKReader                               */
/************************************************************************/
class VFKReader : public IVFKReader 
{
private:
    char          *m_pszFilename;

    FILE          *m_poFD;
    
    /* metadata */
    std::map<std::string, std::string> poInfo;
    
    void          AddInfo(const char *);

protected:
    int             m_nDataBlockCount;
    IVFKDataBlock **m_papoDataBlock;

    IVFKDataBlock  *CreateDataBlock(const char *);
    void            AddDataBlock(IVFKDataBlock *);
    void            AddFeature(IVFKDataBlock *, VFKFeature *);

public:
    VFKReader();
    virtual ~VFKReader();

    OGRErr         OpenFile(const char *);
    int            ReadDataBlocks();
    int            ReadDataRecords(IVFKDataBlock *);
    int            LoadGeometry();
    
    int            GetDataBlockCount() const { return m_nDataBlockCount; }
    IVFKDataBlock *GetDataBlock(int) const;
    IVFKDataBlock *GetDataBlock(const char *) const;

    const char    *GetInfo(const char *);
};

#ifdef HAVE_SQLITE
/************************************************************************/
/*                              VFKReaderSQLite                         */
/************************************************************************/

class VFKReaderSQLite : public VFKReader 
{
private:
    sqlite3       *m_poDB;

    IVFKDataBlock *CreateDataBlock(const char *);
    void           AddDataBlock(IVFKDataBlock *);
    void           AddFeature(IVFKDataBlock *, VFKFeature *);

    friend class   VFKFeatureSQLite;
public:
    VFKReaderSQLite();
    virtual ~VFKReaderSQLite();

    int           ReadDataBlocks();
    int           ReadDataRecords(IVFKDataBlock *);

    sqlite3_stmt *PrepareStatement(const char *);
    OGRErr        ExecuteSQL(const char *);
    OGRErr        ExecuteSQL(sqlite3_stmt *);
};
#endif // HAVE_SQLITE

#endif // GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
