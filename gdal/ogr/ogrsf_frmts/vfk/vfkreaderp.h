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

#include "sqlite3.h"

class VFKReader;

/************************************************************************/
/*                              VFKReader                               */
/************************************************************************/
class VFKReader : public IVFKReader 
{
private:
    bool           m_bLatin2;

    FILE          *m_poFD;
    char          *ReadLine(bool = FALSE);
    
    /* metadata */
    std::map<CPLString, CPLString> poInfo;
    
    void          AddInfo(const char *);

protected:
    char           *m_pszFilename;
    int             m_nDataBlockCount;
    IVFKDataBlock **m_papoDataBlock;

    IVFKDataBlock  *CreateDataBlock(const char *);
    void            AddDataBlock(IVFKDataBlock *, const char *);
    OGRErr          AddFeature(IVFKDataBlock *, VFKFeature *);

public:
    VFKReader(const char *);
    virtual ~VFKReader();

    bool           IsLatin2() const { return m_bLatin2; }
    bool           IsSpatial() const { return FALSE; }
    int            ReadDataBlocks();
    int            ReadDataRecords(IVFKDataBlock *);
    int            LoadGeometry();
    
    int            GetDataBlockCount() const { return m_nDataBlockCount; }
    IVFKDataBlock *GetDataBlock(int) const;
    IVFKDataBlock *GetDataBlock(const char *) const;

    const char    *GetInfo(const char *);
};

/************************************************************************/
/*                              VFKReaderSQLite                         */
/************************************************************************/

class VFKReaderSQLite : public VFKReader 
{
private:
    sqlite3       *m_poDB;
    bool           m_bSpatial;

    IVFKDataBlock *CreateDataBlock(const char *);
    void           AddDataBlock(IVFKDataBlock *, const char *);
    OGRErr         AddFeature(IVFKDataBlock *, VFKFeature *);

    void           CreateIndex(const char *, const char *, const char *, bool = TRUE);
    
    friend class   VFKFeatureSQLite;
public:
    VFKReaderSQLite(const char *);
    virtual ~VFKReaderSQLite();

    bool          IsSpatial() const { return m_bSpatial; }
    int           ReadDataBlocks();
    int           ReadDataRecords(IVFKDataBlock *);

    sqlite3_stmt *PrepareStatement(const char *);
    OGRErr        ExecuteSQL(const char *, bool = FALSE);
    OGRErr        ExecuteSQL(sqlite3_stmt *);
};

#endif // GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
