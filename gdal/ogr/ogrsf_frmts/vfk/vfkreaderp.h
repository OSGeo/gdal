/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Private Declarations for OGR free VFK Reader code.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012-2013, Martin Landa <landa.martin gmail.com>
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

    VSILFILE      *m_poFD;
    char          *ReadLine( bool = false );

    void          AddInfo(const char *) override;

protected:
    char           *m_pszFilename;
    VSIStatBuf     *m_poFStat;
    bool            m_bAmendment;
    int             m_nDataBlockCount;
    IVFKDataBlock **m_papoDataBlock;

    IVFKDataBlock  *CreateDataBlock(const char *) override;
    void            AddDataBlock(IVFKDataBlock *, const char *) override;
    virtual OGRErr          AddFeature(IVFKDataBlock *, VFKFeature *) override;

    // Metadata.
    std::map<CPLString, CPLString> poInfo;

public:
    explicit VFKReader( const char *pszFilename );
    virtual ~VFKReader();

    bool           IsLatin2() const override { return m_bLatin2; }
    bool           IsSpatial() const override { return false; }
    bool           IsPreProcessed() const override { return false; }
    bool           IsValid() const override { return true; }
    int            ReadDataBlocks() override;
    int            ReadDataRecords(IVFKDataBlock * = NULL) override;
    int            LoadGeometry() override;

    int            GetDataBlockCount() const override { return m_nDataBlockCount; }
    IVFKDataBlock *GetDataBlock(int) const override;
    IVFKDataBlock *GetDataBlock(const char *) const override;

    const char    *GetInfo(const char *) override;
};

/************************************************************************/
/*                              VFKReaderSQLite                         */
/************************************************************************/

class VFKReaderSQLite : public VFKReader
{
private:
    char          *m_pszDBname;
    sqlite3       *m_poDB;
    bool           m_bSpatial;
    bool           m_bNewDb;
    bool           m_bDbSource;

    IVFKDataBlock *CreateDataBlock(const char *) override;
    void           AddDataBlock(IVFKDataBlock *, const char *) override;
    OGRErr         AddFeature(IVFKDataBlock *, VFKFeature *) override;

    void           StoreInfo2DB();

    void           CreateIndex(const char *, const char *, const char *, bool = true);

    friend class   VFKFeatureSQLite;
public:
    explicit VFKReaderSQLite(const char *);
    virtual ~VFKReaderSQLite();

    bool          IsSpatial() const override { return m_bSpatial; }
    bool          IsPreProcessed() const override { return !m_bNewDb; }
    bool          IsValid() const override { return m_poDB != NULL; }
    int           ReadDataBlocks() override;
    int           ReadDataRecords(IVFKDataBlock * = NULL) override;

    sqlite3_stmt *PrepareStatement(const char *);
    OGRErr        ExecuteSQL( const char *, bool = false );
    OGRErr        ExecuteSQL(sqlite3_stmt *);
};

#endif // GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
