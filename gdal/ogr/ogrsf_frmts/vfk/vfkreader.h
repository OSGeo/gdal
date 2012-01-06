/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Public Declarations for OGR free VFK Reader code.
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

#ifndef GDAL_OGR_VFK_VFKREADER_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADER_H_INCLUDED

#include <vector>
#include <string>

#include "ogrsf_frmts.h"

#include "cpl_port.h"
#include "cpl_minixml.h"

class IVFKReader;
class VFKDataBlock;
class VFKFeature;

typedef std::vector<VFKFeature *> VFKFeatureList;

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
/*                              VFKFeature                              */
/************************************************************************/
class CPL_DLL VFKFeature
{
private:
    typedef std::vector<VFKProperty> VFKPropertyList;
    
    VFKDataBlock             *m_poDataBlock;
 
    VFKPropertyList           m_propertyList;
    
    long                      m_nFID;
    
    OGRwkbGeometryType        m_nGeometryType;
    bool                      m_bGeometry;
    OGRGeometry              *m_paGeom;
    
public:
    VFKFeature(VFKDataBlock *);
    ~VFKFeature();
    
    long                 GetFID() const { return m_nFID; }
    void                 SetFID(long);

    VFKDataBlock        *GetDataBlock() const { return m_poDataBlock; }
    
    void                 SetProperty(int, const char *);
    const VFKProperty   *GetProperty(int) const;
    const VFKProperty   *GetProperty(const char *) const;

    bool                 LoadGeometry();
    OGRwkbGeometryType   GetGeometryType() const { return m_nGeometryType; }
    OGRGeometry         *GetGeometry();
    void                 SetGeometry(OGRGeometry *);

    bool                 AppendLineToRing(int, const OGRLineString *);
};

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
};

/************************************************************************/
/*                              VFKDataBlock                            */
/************************************************************************/
class CPL_DLL VFKDataBlock
{
private:
    typedef std::vector<OGRPoint>  PointList;
    typedef std::vector<PointList *> PointListArray;
    
    char              *m_pszName;
    
    int                m_nPropertyCount;
    VFKPropertyDefn  **m_papoProperty;

    int                m_nFeatureCount;
    VFKFeature       **m_papoFeature;
    
    int                AddProperty(const char *, const char *);
    
    int                m_iNextFeature;
    long               m_nFID;
    
    OGRwkbGeometryType m_nGeometryType;
    bool               m_bGeometry;
    bool               m_bGeometryPerBlock;

    IVFKReader        *m_poReader;

    bool               AppendLineToRing(PointListArray *, const OGRLineString *, bool);

public:
    VFKDataBlock(const char *, const IVFKReader *);
    ~VFKDataBlock();

    const char        *GetName() const { return m_pszName; }

    int                GetPropertyCount() const { return m_nPropertyCount; }
    VFKPropertyDefn   *GetProperty(int) const;
    int                SetProperties(char *);
    int                GetPropertyIndex(const char *) const;

    int                GetFeatureCount() const  { return m_nFeatureCount;  }
    int                GetFeatureCount(const char *, const char *);
    void               SetFeatureCount(int, int = FALSE);
    VFKFeature        *GetFeatureByIndex(int) const;
    VFKFeature        *GetFeature(int, GUIntBig, VFKFeatureList* = NULL);
    VFKFeatureList     GetFeatures(int, GUIntBig);
    VFKFeatureList     GetFeatures(int, int, GUIntBig);
    VFKFeature        *GetFeature(long);
    int                AddFeature(const char *);

    void               ResetReading(int iIdx = -1);
    VFKFeature        *GetNextFeature();
    VFKFeature        *GetPreviousFeature();
    VFKFeature        *GetFirstFeature();
    VFKFeature        *GetLastFeature();
    int                SetNextFeature(const VFKFeature *);
    
    OGRwkbGeometryType SetGeometryType();
    OGRwkbGeometryType GetGeometryType() const;

    long               GetMaxFID() const { return m_nFID; }
    long               SetMaxFID(long);

    long               LoadGeometry();

    IVFKReader        *GetReader() const { return m_poReader; }
};

/************************************************************************/
/*                              IVFKReader                              */
/************************************************************************/
class CPL_DLL IVFKReader
{
private:
    virtual int  AddDataBlock(VFKDataBlock * = NULL) = 0;
    virtual void AddInfo(const char *) = 0;

public:
    virtual ~IVFKReader();

    virtual void          SetSourceFile(const char *) = 0;

    virtual int           LoadData() = 0;
    virtual int           LoadDataBlocks() = 0;
    virtual long          LoadGeometry() = 0;
    
    virtual int           GetDataBlockCount() const = 0;
    virtual VFKDataBlock *GetDataBlock(int) const = 0;
    virtual VFKDataBlock *GetDataBlock(const char *) const = 0;

    virtual const char   *GetInfo(const char *) = 0;
};

IVFKReader *CreateVFKReader();

#endif // GDAL_OGR_VFK_VFKREADER_H_INCLUDED
