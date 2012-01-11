/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Public Declarations for OGR free VFK Reader code.
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

#ifndef GDAL_OGR_VFK_VFKREADER_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADER_H_INCLUDED

#include <vector>
#include <string>

#include "ogrsf_frmts.h"

#include "cpl_port.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

#ifdef HAVE_SQLITE
#include "sqlite3.h"
#endif

class IVFKReader;
class IVFKDataBlock;
class VFKFeature;
class VFKFeatureSQLite;

typedef std::vector<VFKFeature *>       VFKFeatureList;
typedef std::vector<VFKFeatureSQLite *> VFKFeatureSQLiteList;

/************************************************************************/
/*                              VFKProperty                             */
/************************************************************************/
class CPL_DLL VFKProperty
{
private:
    bool                    m_bIsNull;
    
    int                     m_nValue;
    double                  m_dValue;
    std::string             m_strValue;

public:
    VFKProperty();
    explicit VFKProperty(int);
    explicit VFKProperty(double);
    explicit VFKProperty(const char*);
    explicit VFKProperty(std::string const&);
    ~VFKProperty();
    
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
    OGRGeometry              *m_paGeom;

    virtual bool         LoadGeometryPoint() = 0;
    virtual bool         LoadGeometryLineStringSBP() = 0;
    virtual bool         LoadGeometryLineStringHP() = 0;
    virtual bool         LoadGeometryPolygon() = 0;

public:
    IVFKFeature(IVFKDataBlock *);
    ~IVFKFeature();

    long                 GetFID() const { return m_nFID; }
    void                 SetFID(long);

    IVFKDataBlock       *GetDataBlock() const { return m_poDataBlock; }
    OGRwkbGeometryType   GetGeometryType() const { return m_nGeometryType; }
    void                 SetGeometry(OGRGeometry *);
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

    void                 SetProperty(int, const char *);

    friend class VFKFeatureSQLite;

    bool                 LoadGeometryPoint();
    bool                 LoadGeometryLineStringSBP();
    bool                 LoadGeometryLineStringHP();
    bool                 LoadGeometryPolygon();

public:
    VFKFeature(IVFKDataBlock *);
    
    void                 SetProperties(const char *);
    const VFKProperty   *GetProperty(int) const;
    const VFKProperty   *GetProperty(const char *) const;

    OGRErr               LoadProperties(OGRFeature *);

    bool                 AppendLineToRing(int, const OGRLineString *);
};

#ifdef HAVE_SQLITE
/************************************************************************/
/*                              VFKFeatureSQLite                        */
/************************************************************************/
class CPL_DLL VFKFeatureSQLite : public IVFKFeature
{
private:
    int                  m_nIndex; /* feature index in the array */
    sqlite3_stmt        *m_hStmt;

    bool                 LoadGeometryPoint();
    bool                 LoadGeometryLineStringSBP();
    bool                 LoadGeometryLineStringHP();
    bool                 LoadGeometryPolygon();

public:
    VFKFeatureSQLite(const VFKFeature *);

    OGRErr               LoadProperties(OGRFeature *);
};

#endif

/************************************************************************/
/*                              VFKPropertyDefn                         */
/************************************************************************/
class CPL_DLL VFKPropertyDefn
{
private:
    char             *m_pszName;

    char             *m_pszType;
    OGRFieldType      m_eFType;

    int               m_nWidth;
    int               m_nPrecision;

public:
    VFKPropertyDefn(const char*, const char *);
    ~VFKPropertyDefn();

    const char       *GetName() const  { return m_pszName; }
    int               GetWidth() const { return m_nWidth;  }
    int               GetPrecision() const { return m_nPrecision;  }
    OGRFieldType      GetType() const  { return m_eFType;  }
    CPLString         GetTypeSQL() const;
    GBool             IsIntBig() const { return m_pszType[0] == 'N'; }
};

/************************************************************************/
/*                              IVFKDataBlock                           */
/************************************************************************/
class CPL_DLL IVFKDataBlock
{
private:
    int                m_nPropertyCount;
    VFKPropertyDefn  **m_papoProperty;

    IVFKFeature      **m_papoFeature;
    
    long               m_nFID;
    
    OGRwkbGeometryType m_nGeometryType;
    bool               m_bGeometryPerBlock;

    int                AddProperty(const char *, const char *);

protected:
    typedef std::vector<OGRPoint>    PointList;
    typedef std::vector<PointList *> PointListArray;

    char              *m_pszName;
    bool               m_bGeometry;

    int                m_nFeatureCount;    
    int                m_iNextFeature;

    IVFKReader        *m_poReader;

    bool               AppendLineToRing(PointListArray *, const OGRLineString *, bool);
    virtual long       LoadGeometryPoint() = 0;
    virtual long       LoadGeometryLineStringSBP() = 0;
    virtual long       LoadGeometryLineStringHP() = 0;
    virtual long       LoadGeometryPolygon() = 0;

public:
    IVFKDataBlock(const char *, const IVFKReader *);
    ~IVFKDataBlock();

    const char        *GetName() const { return m_pszName; }

    int                GetPropertyCount() const { return m_nPropertyCount; }
    VFKPropertyDefn   *GetProperty(int) const;
    void               SetProperties(char *);
    int                GetPropertyIndex(const char *) const;

    int                GetFeatureCount() const  { return m_nFeatureCount;  }
    void               SetFeatureCount(int, int = FALSE);
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

    long               GetMaxFID() const { return m_nFID; }
    long               SetMaxFID(long);

    long               LoadGeometry();

    IVFKReader        *GetReader() const { return m_poReader; }
};

/************************************************************************/
/*                              VFKDataBlock                            */
/************************************************************************/
class CPL_DLL VFKDataBlock : public IVFKDataBlock
{
private:
    long               LoadGeometryPoint();
    long               LoadGeometryLineStringSBP();
    long               LoadGeometryLineStringHP();
    long               LoadGeometryPolygon();

public:
    VFKDataBlock(const char *pszName, const IVFKReader *poReader) : IVFKDataBlock(pszName, poReader) {}

    VFKFeature        *GetFeature(int, GUIntBig, VFKFeatureList* = NULL);
    VFKFeatureList     GetFeatures(int, GUIntBig);
    VFKFeatureList     GetFeatures(int, int, GUIntBig);

    int                GetFeatureCount(const char *, const char *);
};

#ifdef HAVE_SQLITE
/************************************************************************/
/*                              VFKDataBlockSQLite                      */
/************************************************************************/
class CPL_DLL VFKDataBlockSQLite : public IVFKDataBlock
{
private:
    long                 LoadGeometryPoint();
    long                 LoadGeometryLineStringSBP();
    long                 LoadGeometryLineStringHP();
    long                 LoadGeometryPolygon();

public:
    VFKDataBlockSQLite(const char *pszName, const IVFKReader *poReader) : IVFKDataBlock(pszName, poReader) {}

    VFKFeatureSQLite    *GetFeature(const char *, GUIntBig);
    VFKFeatureSQLite    *GetFeature(const char **, GUIntBig *, int);
    VFKFeatureSQLiteList GetFeatures(const char **, GUIntBig *, int);
};
#endif

/************************************************************************/
/*                              IVFKReader                              */
/************************************************************************/
class CPL_DLL IVFKReader
{
private:
    virtual void AddInfo(const char *) = 0;

protected:
    virtual IVFKDataBlock *CreateDataBlock(const char *) = 0;
    virtual void AddDataBlock(IVFKDataBlock * = NULL) = 0;
    virtual void AddFeature(IVFKDataBlock * = NULL, VFKFeature * = NULL) = 0;

public:
    virtual ~IVFKReader();

    virtual void           SetSourceFile(const char *) = 0;

    virtual int            LoadData() = 0;
    virtual int            LoadDataBlocks() = 0;
    virtual long           LoadGeometry() = 0;
    
    virtual int            GetDataBlockCount() const = 0;
    virtual IVFKDataBlock *GetDataBlock(int) const = 0;
    virtual IVFKDataBlock *GetDataBlock(const char *) const = 0;

    virtual const char    *GetInfo(const char *) = 0;
};

IVFKReader *CreateVFKReader();

#endif // GDAL_OGR_VFK_VFKREADER_H_INCLUDED
