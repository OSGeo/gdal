/******************************************************************************
 *
 * Project:  VFK Reader
 * Purpose:  Private Declarations for OGR free VFK Reader code.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2018, Martin Landa <landa.martin gmail.com>
 *
 * SPDX-License-Identifier: MIT
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
    const char *m_pszEncoding;

    VSILFILE *m_poFD;
    char *ReadLine();

    void AddInfo(const char *) override;

    CPL_DISALLOW_COPY_ASSIGN(VFKReader)

  protected:
    char *m_pszFilename;
    VSIStatBufL *m_poFStat;
    bool m_bAmendment;
    bool m_bFileField;
    int m_nDataBlockCount;
    IVFKDataBlock **m_papoDataBlock;

    IVFKDataBlock *CreateDataBlock(const char *) override;
    void AddDataBlock(IVFKDataBlock *, const char *) override;
    virtual OGRErr AddFeature(IVFKDataBlock *, VFKFeature *) override;
    void ReadEncoding();

    // Metadata.
    std::map<CPLString, CPLString> poInfo;

  public:
    explicit VFKReader(const GDALOpenInfo *);
    virtual ~VFKReader();

    const char *GetFilename() const override
    {
        return m_pszFilename;
    }

    const char *GetEncoding() const override
    {
        return m_pszEncoding;
    }

    bool IsSpatial() const override
    {
        return false;
    }

    bool IsPreProcessed() const override
    {
        return false;
    }

    bool IsValid() const override
    {
        return true;
    }

    bool HasFileField() const override
    {
        return m_bFileField;
    }

    int ReadDataBlocks(bool = false) override;
    int64_t ReadDataRecords(IVFKDataBlock * = nullptr) override;
    int LoadGeometry() override;

    int GetDataBlockCount() const override
    {
        return m_nDataBlockCount;
    }

    IVFKDataBlock *GetDataBlock(int) const override;
    IVFKDataBlock *GetDataBlock(const char *) const override;

    const char *GetInfo(const char *) override;
};

/************************************************************************/
/*                              VFKReaderSQLite                         */
/************************************************************************/

class VFKReaderSQLite : public VFKReader
{
  private:
    char *m_pszDBname;
    sqlite3 *m_poDB;
    bool m_bSpatial;
    bool m_bNewDb;
    bool m_bDbSource;

    IVFKDataBlock *CreateDataBlock(const char *) override;
    void AddDataBlock(IVFKDataBlock *, const char *) override;
    OGRErr AddFeature(IVFKDataBlock *, VFKFeature *) override;

    void StoreInfo2DB();

    void CreateIndex(const char *, const char *, const char *, bool = true);
    void CreateIndices();

    friend class VFKFeatureSQLite;

  public:
    explicit VFKReaderSQLite(const GDALOpenInfo *);
    virtual ~VFKReaderSQLite();

    bool IsSpatial() const override
    {
        return m_bSpatial;
    }

    bool IsPreProcessed() const override
    {
        return !m_bNewDb;
    }

    bool IsValid() const override
    {
        return m_poDB != nullptr;
    }

    int ReadDataBlocks(bool = false) override;
    int64_t ReadDataRecords(IVFKDataBlock * = nullptr) override;

    sqlite3_stmt *PrepareStatement(const char *);
    OGRErr ExecuteSQL(const char *, CPLErr = CE_Failure);
    OGRErr ExecuteSQL(sqlite3_stmt *&);
};

#endif  // GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
