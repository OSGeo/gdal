/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Public Declarations for OGR free VFK Reader code.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2014, Martin Landa <landa.martin gmail.com>
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

#ifndef GDAL_OGR_VFK_VFKREADER_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADER_H_INCLUDED

#include <vector>
#include <string>

#include "ogrsf_frmts.h"

#include "cpl_port.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

#include "sqlite3.h"

class IVFKReader;
class IVFKDataBlock;
class VFKFeature;
class VFKFeatureSQLite;

typedef std::vector<VFKFeature *>       VFKFeatureList;
typedef std::vector<VFKFeatureSQLite *> VFKFeatureSQLiteList;

#define FID_COLUMN   "ogr_fid"
#define GEOM_COLUMN  "geometry"
#define FILE_COLUMN  "VFK_FILENAME"

#define VFK_DB_HEADER_TABLE      "vfk_header"
#define VFK_DB_TABLE             "vfk_tables"
#define VFK_DB_GEOMETRY_TABLE    "geometry_columns"
#define VFK_DB_SPATIAL_REF_TABLE "spatial_ref_sys"

enum RecordType { RecordValid, RecordSkipped, RecordDuplicated };

/************************************************************************/
/*                              VFKProperty                             */
/************************************************************************/
class VFKProperty
{
private:
    bool                    m_bIsNull;

    GIntBig                 m_iValue;
    double                  m_dValue;
    CPLString               m_strValue;

public:
    VFKProperty();
    explicit VFKProperty(int);
    explicit VFKProperty(GIntBig);
    explicit VFKProperty(double);
    explicit VFKProperty(const char*);
    explicit VFKProperty(CPLString const&);
    virtual ~VFKProperty();

    VFKProperty(VFKProperty const& other) = default;
    VFKProperty& operator=(VFKProperty const&) = default;
    VFKProperty& operator=(VFKProperty&&) = default;

    bool                    IsNull()      const { return m_bIsNull; }
    int                     GetValueI()   const { return static_cast<int> (m_iValue); }
    GIntBig                 GetValueI64() const { return m_iValue; }
    double                  GetValueD()   const { return m_dValue; }
    const char             *GetValueS( bool = false ) const;
};

/************************************************************************/
/*                              IVFKFeature                              */
/************************************************************************/
class IVFKFeature
{
protected:
    IVFKDataBlock            *m_poDataBlock;
    GIntBig                   m_nFID;
    OGRwkbGeometryType        m_nGeometryType;
    bool                      m_bGeometry;
    bool                      m_bValid;
    OGRGeometry              *m_paGeom;

    virtual bool         LoadGeometryPoint() = 0;
    virtual bool         LoadGeometryLineStringSBP() = 0;
    virtual bool         LoadGeometryLineStringHP() = 0;
    virtual bool         LoadGeometryPolygon() = 0;

public:
    explicit IVFKFeature(IVFKDataBlock *);
    virtual ~IVFKFeature();

    GIntBig              GetFID() const { return m_nFID; }
    void                 SetFID(GIntBig);
    void                 SetGeometryType(OGRwkbGeometryType);

    bool                 IsValid() const { return m_bValid; }

    IVFKDataBlock       *GetDataBlock() const { return m_poDataBlock; }
    OGRwkbGeometryType   GetGeometryType() const { return m_nGeometryType; }
    bool                 SetGeometry(OGRGeometry *, const char * = nullptr);
    OGRGeometry         *GetGeometry();

    bool                 LoadGeometry();
    virtual OGRErr       LoadProperties(OGRFeature *) = 0;
};

/************************************************************************/
/*                              VFKFeature                              */
/************************************************************************/
class VFKFeature : public IVFKFeature
{
private:
    typedef std::vector<VFKProperty> VFKPropertyList;

    VFKPropertyList      m_propertyList;

    bool                 SetProperty(int, const char *);

    friend class         VFKFeatureSQLite;

    bool                 LoadGeometryPoint() override;
    bool                 LoadGeometryLineStringSBP() override;
    bool                 LoadGeometryLineStringHP() override;
    bool                 LoadGeometryPolygon() override;

public:
    VFKFeature(IVFKDataBlock *, GIntBig);

    bool                 SetProperties(const char *);
    const VFKProperty   *GetProperty(int) const;
    const VFKProperty   *GetProperty(const char *) const;

    OGRErr               LoadProperties(OGRFeature *) override;

    bool                 AppendLineToRing(int, const OGRLineString *);
};

/************************************************************************/
/*                              VFKFeatureSQLite                        */
/************************************************************************/
class VFKFeatureSQLite : public IVFKFeature
{
private:
    int                  m_iRowId;           /* rowid in DB */
    sqlite3_stmt        *m_hStmt;

    bool                 LoadGeometryPoint() override;
    bool                 LoadGeometryLineStringSBP() override;
    bool                 LoadGeometryLineStringHP() override;
    bool                 LoadGeometryPolygon() override;

    OGRErr               SetFIDFromDB();
    OGRErr               ExecuteSQL(const char *);
    void                 FinalizeSQL();

public:
    explicit VFKFeatureSQLite(IVFKDataBlock *);
    VFKFeatureSQLite(IVFKDataBlock *, int, GIntBig);
    explicit VFKFeatureSQLite(const VFKFeature *);

    OGRErr               LoadProperties(OGRFeature *) override;
    void                 SetRowId(int);
};

/************************************************************************/
/*                              VFKPropertyDefn                         */
/************************************************************************/
class VFKPropertyDefn
{
private:
    char             *m_pszName;

    char             *m_pszType;
    char             *m_pszEncoding;
    OGRFieldType      m_eFType;

    int               m_nWidth;
    int               m_nPrecision;

public:
    VFKPropertyDefn(const char*, const char *, bool);
    virtual ~VFKPropertyDefn();

    const char       *GetName() const  { return m_pszName; }
    int               GetWidth() const { return m_nWidth;  }
    int               GetPrecision() const { return m_nPrecision;  }
    OGRFieldType      GetType() const  { return m_eFType;  }
    CPLString         GetTypeSQL() const;
    const char       *GetEncoding() const { return  m_pszEncoding; }
};

/************************************************************************/
/*                              IVFKDataBlock                           */
/************************************************************************/
class IVFKDataBlock
{
private:
    IVFKFeature      **m_papoFeature;

    int                m_nPropertyCount;
    VFKPropertyDefn  **m_papoProperty;

    int                AddProperty(const char *, const char *);

protected:
    typedef std::vector<OGRPoint>    PointList;
    typedef std::vector<PointList *> PointListArray;

    char              *m_pszName;
    bool               m_bGeometry;

    OGRwkbGeometryType m_nGeometryType;
    bool               m_bGeometryPerBlock;

    int                m_nFeatureCount;
    int                m_iNextFeature;

    // TODO: Make m_poReader const.
    IVFKReader        *m_poReader;

    GIntBig            m_nRecordCount[3];

    bool               AppendLineToRing( PointListArray *,
                                         const OGRLineString *,
                                         bool, bool = false );
    int                LoadData();

    virtual int        LoadGeometryPoint() = 0;
    virtual int        LoadGeometryLineStringSBP() = 0;
    virtual int        LoadGeometryLineStringHP() = 0;
    virtual int        LoadGeometryPolygon() = 0;

    static void        FillPointList(PointList* poList, const OGRLineString *poLine);

public:
    IVFKDataBlock(const char *, const IVFKReader *);
    virtual ~IVFKDataBlock();

    const char        *GetName() const { return m_pszName; }

    int                GetPropertyCount() const { return m_nPropertyCount; }
    VFKPropertyDefn   *GetProperty(int) const;
    void               SetProperties(const char *);
    int                GetPropertyIndex(const char *) const;

    GIntBig            GetFeatureCount( bool = true);
    void               SetFeatureCount( int, bool = false );
    IVFKFeature       *GetFeatureByIndex(int) const;
    IVFKFeature       *GetFeature(GIntBig);
    void               AddFeature(IVFKFeature *);

    void               ResetReading(int iIdx = -1);
    IVFKFeature       *GetNextFeature();
    IVFKFeature       *GetPreviousFeature();
    IVFKFeature       *GetFirstFeature();
    IVFKFeature       *GetLastFeature();
    int                SetNextFeature(const IVFKFeature *);

    OGRwkbGeometryType SetGeometryType(bool = false);
    OGRwkbGeometryType GetGeometryType() const;

    int                LoadGeometry();

    virtual OGRErr     LoadProperties() = 0;
    virtual OGRErr     CleanProperties() = 0;

    IVFKReader        *GetReader() const { return m_poReader; }
    int                GetRecordCount(RecordType = RecordValid)  const;
    void               SetIncRecordCount(RecordType);
};

/************************************************************************/
/*                              VFKDataBlock                            */
/************************************************************************/
class VFKDataBlock : public IVFKDataBlock
{
private:
    int                LoadGeometryPoint() override;
    int                LoadGeometryLineStringSBP() override;
    int                LoadGeometryLineStringHP() override;
    int                LoadGeometryPolygon() override;

public:
    VFKDataBlock(const char *pszName, const IVFKReader *poReader) : IVFKDataBlock(pszName, poReader) {}

    VFKFeature        *GetFeature(int, GUIntBig, VFKFeatureList* poList = nullptr);
    VFKFeatureList     GetFeatures(int, GUIntBig);
    VFKFeatureList     GetFeatures(int, int, GUIntBig);

    GIntBig            GetFeatureCount(const char *, const char *);

    OGRErr             LoadProperties() override { return OGRERR_UNSUPPORTED_OPERATION; }
    OGRErr             CleanProperties() override { return OGRERR_UNSUPPORTED_OPERATION; }
};

/************************************************************************/
/*                              VFKDataBlockSQLite                      */
/************************************************************************/
class VFKDataBlockSQLite : public IVFKDataBlock
{
private:
    sqlite3_stmt        *m_hStmt;

    bool                 SetGeometryLineString(VFKFeatureSQLite *, OGRLineString *,
                                               bool&, const char *,
                                               std::vector<int>&, int&);

    int                  LoadGeometryPoint() override;
    int                  LoadGeometryLineStringSBP() override;
    int                  LoadGeometryLineStringHP() override;
    int                  LoadGeometryPolygon() override;

    bool                 LoadGeometryFromDB();
    OGRErr               SaveGeometryToDB(const OGRGeometry *, int);

    OGRErr               LoadProperties() override;
    OGRErr               CleanProperties() override;

    static bool          IsRingClosed(const OGRLinearRing *);
    void                 UpdateVfkBlocks(int);
    void                 UpdateFID(GIntBig, std::vector<int>);

    friend class         VFKFeatureSQLite;
public:
    VFKDataBlockSQLite(const char *, const IVFKReader *);

    const char          *GetKey() const;
    IVFKFeature         *GetFeature(GIntBig);
    VFKFeatureSQLite    *GetFeature( const char *, GUIntBig, bool = false );
    VFKFeatureSQLite    *GetFeature( const char **, GUIntBig *, int,
                                     bool = false);
    VFKFeatureSQLiteList GetFeatures(const char **, GUIntBig *, int);

    int                  GetGeometrySQLType() const;

    OGRErr               AddGeometryColumn() const;
};

/************************************************************************/
/*                              IVFKReader                              */
/************************************************************************/
class IVFKReader
{
private:
    virtual void AddInfo(const char *) = 0;

protected:
    virtual IVFKDataBlock *CreateDataBlock(const char *) = 0;
    virtual void           AddDataBlock(IVFKDataBlock * = nullptr, const char * = nullptr) = 0;
    virtual OGRErr         AddFeature(IVFKDataBlock * = nullptr, VFKFeature * = nullptr) = 0;

public:
    virtual ~IVFKReader();

    virtual const char    *GetFilename() const = 0;

    virtual bool           IsLatin2() const = 0;
    virtual bool           IsSpatial() const = 0;
    virtual bool           IsPreProcessed() const = 0;
    virtual bool           IsValid() const = 0;
    virtual bool           HasFileField() const = 0;
    virtual int            ReadDataBlocks(bool = false) = 0;
    virtual int            ReadDataRecords(IVFKDataBlock * = nullptr) = 0;
    virtual int            LoadGeometry() = 0;

    virtual int            GetDataBlockCount() const = 0;
    virtual IVFKDataBlock *GetDataBlock(int) const = 0;
    virtual IVFKDataBlock *GetDataBlock(const char *) const = 0;

    virtual const char    *GetInfo(const char *) = 0;
};

IVFKReader *CreateVFKReader( const GDALOpenInfo * );

#endif // GDAL_OGR_VFK_VFKREADER_H_INCLUDED
