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

#define VFK_DB_HEADER   "vfk_header"
#define VFK_DB_TABLE    "vfk_tables"


enum RecordType { RecordValid, RecordSkipped, RecordDuplicated };

/************************************************************************/
/*                              VFKProperty                             */
/************************************************************************/
class CPL_DLL VFKProperty
{
private:
    bool                    m_bIsNull;
    
    int                     m_nValue;
    double                  m_dValue;
    CPLString               m_strValue;

public:
    VFKProperty();
    explicit VFKProperty(int);
    explicit VFKProperty(double);
    explicit VFKProperty(const char*);
    explicit VFKProperty(CPLString const&);
    virtual ~VFKProperty();
    
    VFKProperty(VFKProperty const& other);
    VFKProperty& operator=(VFKProperty const& other);

    bool                    IsNull()    const { return m_bIsNull; }
    int                     GetValueI() const { return m_nValue; }
    double                  GetValueD() const { return m_dValue; }
    const char             *GetValueS() const { return m_strValue.c_str(); }
};

/************************************************************************/
/*                              IVFKFeature                              */
/************************************************************************/
class CPL_DLL IVFKFeature
{
protected:
    IVFKDataBlock            *m_poDataBlock;
    long                      m_nFID;
    OGRwkbGeometryType        m_nGeometryType;
    bool                      m_bGeometry;
    bool                      m_bValid;
    OGRGeometry              *m_paGeom;

    virtual bool         LoadGeometryPoint() = 0;
    virtual bool         LoadGeometryLineStringSBP() = 0;
    virtual bool         LoadGeometryLineStringHP() = 0;
    virtual bool         LoadGeometryPolygon() = 0;

public:
    IVFKFeature(IVFKDataBlock *);
    virtual ~IVFKFeature();

    long                 GetFID() const { return m_nFID; }
    void                 SetFID(long);
    void                 SetGeometryType(OGRwkbGeometryType);

    bool                 IsValid() const { return m_bValid; }
    
    IVFKDataBlock       *GetDataBlock() const { return m_poDataBlock; }
    OGRwkbGeometryType   GetGeometryType() const { return m_nGeometryType; }
    bool                 SetGeometry(OGRGeometry *);
    OGRGeometry         *GetGeometry();

    bool                 LoadGeometry();
    virtual OGRErr       LoadProperties(OGRFeature *) = 0;
};

/************************************************************************/
/*                              VFKFeature                              */
/************************************************************************/
class CPL_DLL VFKFeature : public IVFKFeature
{
private:
    typedef std::vector<VFKProperty> VFKPropertyList;
    
    VFKPropertyList      m_propertyList;

    bool                 SetProperty(int, const char *);

    friend class         VFKFeatureSQLite;

    bool                 LoadGeometryPoint();
    bool                 LoadGeometryLineStringSBP();
    bool                 LoadGeometryLineStringHP();
    bool                 LoadGeometryPolygon();

public:
    VFKFeature(IVFKDataBlock *, long);
    
    bool                 SetProperties(const char *);
    const VFKProperty   *GetProperty(int) const;
    const VFKProperty   *GetProperty(const char *) const;

    OGRErr               LoadProperties(OGRFeature *);

    bool                 AppendLineToRing(int, const OGRLineString *);
};

/************************************************************************/
/*                              VFKFeatureSQLite                        */
/************************************************************************/
class CPL_DLL VFKFeatureSQLite : public IVFKFeature
{
private:
    int                  m_iRowId;           /* rowid in DB */
    sqlite3_stmt        *m_hStmt;

    bool                 LoadGeometryPoint();
    bool                 LoadGeometryLineStringSBP();
    bool                 LoadGeometryLineStringHP();
    bool                 LoadGeometryPolygon();

    OGRErr               SetFIDFromDB();
    OGRErr               ExecuteSQL(const char *);
    void                 FinalizeSQL();

public:
    VFKFeatureSQLite(IVFKDataBlock *);
    VFKFeatureSQLite(IVFKDataBlock *, int, long);
    VFKFeatureSQLite(const VFKFeature *);

    OGRErr               LoadProperties(OGRFeature *);
};

/************************************************************************/
/*                              VFKPropertyDefn                         */
/************************************************************************/
class CPL_DLL VFKPropertyDefn
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
    GBool             IsIntBig() const { return m_pszType[0] == 'N'; }
    const char       *GetEncoding() const { return  m_pszEncoding; }
};

/************************************************************************/
/*                              IVFKDataBlock                           */
/************************************************************************/
class CPL_DLL IVFKDataBlock
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

    IVFKReader        *m_poReader;

    long               m_nRecordCount[3];
    
    bool               AppendLineToRing(PointListArray *, const OGRLineString *, bool, bool = FALSE);
    int                LoadData();
    
    virtual int        LoadGeometryPoint() = 0;
    virtual int        LoadGeometryLineStringSBP() = 0;
    virtual int        LoadGeometryLineStringHP() = 0;
    virtual int        LoadGeometryPolygon() = 0;

public:
    IVFKDataBlock(const char *, const IVFKReader *);
    virtual ~IVFKDataBlock();

    const char        *GetName() const { return m_pszName; }

    int                GetPropertyCount() const { return m_nPropertyCount; }
    VFKPropertyDefn   *GetProperty(int) const;
    void               SetProperties(const char *);
    int                GetPropertyIndex(const char *) const;

    int                GetFeatureCount();
    void               SetFeatureCount(int, bool = FALSE);
    IVFKFeature       *GetFeatureByIndex(int) const;
    IVFKFeature       *GetFeature(long);
    void               AddFeature(IVFKFeature *);

    void               ResetReading(int iIdx = -1);
    IVFKFeature       *GetNextFeature();
    IVFKFeature       *GetPreviousFeature();
    IVFKFeature       *GetFirstFeature();
    IVFKFeature       *GetLastFeature();
    int                SetNextFeature(const IVFKFeature *);
    
    OGRwkbGeometryType SetGeometryType();
    OGRwkbGeometryType GetGeometryType() const;

    int                LoadGeometry();

    IVFKReader        *GetReader() const { return m_poReader; }
    int                GetRecordCount(RecordType = RecordValid)  const;
    void               SetIncRecordCount(RecordType);
};

/************************************************************************/
/*                              VFKDataBlock                            */
/************************************************************************/
class CPL_DLL VFKDataBlock : public IVFKDataBlock
{
private:
    int                LoadGeometryPoint();
    int                LoadGeometryLineStringSBP();
    int                LoadGeometryLineStringHP();
    int                LoadGeometryPolygon();

public:
    VFKDataBlock(const char *pszName, const IVFKReader *poReader) : IVFKDataBlock(pszName, poReader) {}

    VFKFeature        *GetFeature(int, GUIntBig, VFKFeatureList* = NULL);
    VFKFeatureList     GetFeatures(int, GUIntBig);
    VFKFeatureList     GetFeatures(int, int, GUIntBig);

    int                GetFeatureCount(const char *, const char *);
};

/************************************************************************/
/*                              VFKDataBlockSQLite                      */
/************************************************************************/
class CPL_DLL VFKDataBlockSQLite : public IVFKDataBlock
{
private:
    bool                 SetGeometryLineString(VFKFeatureSQLite *, OGRLineString *,
                                               bool&, const char *,
                                               std::vector<int>&, int&);

    int                  LoadGeometryPoint();
    int                  LoadGeometryLineStringSBP();
    int                  LoadGeometryLineStringHP();
    int                  LoadGeometryPolygon();

    bool                 LoadGeometryFromDB();
    OGRErr               SaveGeometryToDB(const OGRGeometry *, int);

    bool                 IsRingClosed(const OGRLinearRing *);
    void                 UpdateVfkBlocks(int);
    void                 UpdateFID(long int, std::vector<int>);

public:
    VFKDataBlockSQLite(const char *pszName, const IVFKReader *poReader) : IVFKDataBlock(pszName, poReader) {}

    const char          *GetKey() const;
    IVFKFeature         *GetFeature(long);
    VFKFeatureSQLite    *GetFeature(const char *, GUIntBig, bool = FALSE);
    VFKFeatureSQLite    *GetFeature(const char **, GUIntBig *, int, bool = FALSE);
    VFKFeatureSQLiteList GetFeatures(const char **, GUIntBig *, int);
};

/************************************************************************/
/*                              IVFKReader                              */
/************************************************************************/
class CPL_DLL IVFKReader
{
private:
    virtual void AddInfo(const char *) = 0;

protected:
    virtual IVFKDataBlock *CreateDataBlock(const char *) = 0;
    virtual void           AddDataBlock(IVFKDataBlock * = NULL, const char * = NULL) = 0;
    virtual OGRErr         AddFeature(IVFKDataBlock * = NULL, VFKFeature * = NULL) = 0;

public:
    virtual ~IVFKReader();
    
    virtual bool           IsLatin2() const = 0;
    virtual bool           IsSpatial() const = 0;
    virtual bool           IsPreProcessed() const = 0;
    virtual int            ReadDataBlocks() = 0;
    virtual int            ReadDataRecords(IVFKDataBlock * = NULL) = 0;
    virtual int            LoadGeometry() = 0;

    virtual int            GetDataBlockCount() const = 0;
    virtual IVFKDataBlock *GetDataBlock(int) const = 0;
    virtual IVFKDataBlock *GetDataBlock(const char *) const = 0;

    virtual const char    *GetInfo(const char *) = 0;
};

IVFKReader *CreateVFKReader(const char *);

#endif // GDAL_OGR_VFK_VFKREADER_H_INCLUDED
