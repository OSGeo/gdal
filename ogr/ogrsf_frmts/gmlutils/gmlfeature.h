/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Public Declarations for OGR free GML Reader code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GMLFEATURE_H_INCLUDED
#define GMLFEATURE_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "cpl_minixml.h"
#include "ogr_core.h"
#include "ogr_geomcoordinateprecision.h"

#include <map>
#include <vector>

typedef enum
{
    GMLPT_Untyped = 0,
    GMLPT_String = 1,
    GMLPT_Integer = 2,
    GMLPT_Real = 3,
    GMLPT_Complex = 4,
    GMLPT_StringList = 5,
    GMLPT_IntegerList = 6,
    GMLPT_RealList = 7,
    GMLPT_FeatureProperty = 8,
    GMLPT_FeaturePropertyList = 9,
    GMLPT_Boolean = 10,
    GMLPT_BooleanList = 11,
    GMLPT_Short = 12,
    GMLPT_Float = 13,
    GMLPT_Integer64 = 14,
    GMLPT_Integer64List = 15,
    GMLPT_DateTime = 16,
    GMLPT_Date = 17,
    GMLPT_Time = 18,
} GMLPropertyType;

/************************************************************************/
/*                           GMLPropertyDefn                            */
/************************************************************************/

typedef struct
{
    int nSubProperties;
    char **papszSubProperties;
    char *aszSubProperties[2]; /* Optimization in the case of nSubProperties ==
                                  1 */
} GMLProperty;

class CPL_DLL GMLPropertyDefn
{
    char *m_pszName = nullptr;
    GMLPropertyType m_eType = GMLPT_Untyped;
    OGRFieldSubType m_eSubType = OFSTNone;
    int m_nWidth = 0;
    int m_nPrecision = 0;
    char *m_pszSrcElement = nullptr;
    size_t m_nSrcElementLen = 0;
    char *m_pszCondition = nullptr;
    bool m_bNullable = true;
    bool m_bUnique = false;
    std::string m_osDocumentation{};

    CPL_DISALLOW_COPY_ASSIGN(GMLPropertyDefn)

  public:
    explicit GMLPropertyDefn(const char *pszName,
                             const char *pszSrcElement = nullptr);
    ~GMLPropertyDefn();

    const char *GetName() const
    {
        return m_pszName;
    }

    void SetName(const char *pszName)
    {
        CPLFree(m_pszName);
        m_pszName = CPLStrdup(pszName);
    }

    GMLPropertyType GetType() const
    {
        return m_eType;
    }

    void SetType(GMLPropertyType eType)
    {
        m_eType = eType;
    }

    OGRFieldSubType GetSubType() const
    {
        return m_eSubType;
    }

    void SetSubType(OGRFieldSubType eSubType)
    {
        m_eSubType = eSubType;
    }

    void SetWidth(int nWidth)
    {
        m_nWidth = nWidth;
    }

    int GetWidth() const
    {
        return m_nWidth;
    }

    void SetPrecision(int nPrecision)
    {
        m_nPrecision = nPrecision;
    }

    int GetPrecision() const
    {
        return m_nPrecision;
    }

    void SetSrcElement(const char *pszSrcElement);

    const char *GetSrcElement() const
    {
        return m_pszSrcElement;
    }

    size_t GetSrcElementLen() const
    {
        return m_nSrcElementLen;
    }

    void SetCondition(const char *pszCondition);

    const char *GetCondition() const
    {
        return m_pszCondition;
    }

    void SetNullable(bool bNullable)
    {
        m_bNullable = bNullable;
    }

    bool IsNullable() const
    {
        return m_bNullable;
    }

    void SetUnique(bool bUnique)
    {
        m_bUnique = bUnique;
    }

    bool IsUnique() const
    {
        return m_bUnique;
    }

    void SetDocumentation(const std::string &osDocumentation)
    {
        m_osDocumentation = osDocumentation;
    }

    const std::string &GetDocumentation() const
    {
        return m_osDocumentation;
    }

    void AnalysePropertyValue(const GMLProperty *psGMLProperty,
                              bool bSetWidth = true);

    static bool IsSimpleType(GMLPropertyType eType)
    {
        return eType == GMLPT_String || eType == GMLPT_Integer ||
               eType == GMLPT_Real;
    }
};

/************************************************************************/
/*                    GMLGeometryPropertyDefn                           */
/************************************************************************/

class CPL_DLL GMLGeometryPropertyDefn
{
    char *m_pszName = nullptr;
    char *m_pszSrcElement = nullptr;
    OGRwkbGeometryType m_nGeometryType = wkbUnknown;
    const int m_nAttributeIndex = -1;
    const bool m_bNullable = true;
    bool m_bSRSNameConsistent = true;
    std::string m_osSRSName{};
    OGRGeomCoordinatePrecision m_oCoordPrecision{};

    CPL_DISALLOW_COPY_ASSIGN(GMLGeometryPropertyDefn)

  public:
    GMLGeometryPropertyDefn(const char *pszName, const char *pszSrcElement,
                            OGRwkbGeometryType nType, int nAttributeIndex,
                            bool bNullable,
                            const OGRGeomCoordinatePrecision &oCoordPrec =
                                OGRGeomCoordinatePrecision());
    ~GMLGeometryPropertyDefn();

    const char *GetName() const
    {
        return m_pszName;
    }

    OGRwkbGeometryType GetType() const
    {
        return m_nGeometryType;
    }

    void SetType(OGRwkbGeometryType nType)
    {
        m_nGeometryType = nType;
    }

    const char *GetSrcElement() const
    {
        return m_pszSrcElement;
    }

    int GetAttributeIndex() const
    {
        return m_nAttributeIndex;
    }

    bool IsNullable() const
    {
        return m_bNullable;
    }

    const OGRGeomCoordinatePrecision &GetCoordinatePrecision() const
    {
        return m_oCoordPrecision;
    }

    void SetSRSName(const std::string &srsName)
    {
        m_bSRSNameConsistent = true;
        m_osSRSName = srsName;
    }

    void MergeSRSName(const std::string &osSRSName);

    const std::string &GetSRSName() const
    {
        return m_osSRSName;
    }
};

/************************************************************************/
/*                           GMLFeatureClass                            */
/************************************************************************/

class CPL_DLL GMLFeatureClass
{
    char *m_pszName;
    char *m_pszElementName;
    int n_nNameLen;
    int n_nElementNameLen;
    int m_nPropertyCount;
    GMLPropertyDefn **m_papoProperty;
    std::map<CPLString, int> m_oMapPropertyNameToIndex{};
    std::map<CPLString, int> m_oMapPropertySrcElementToIndex{};

    int m_nGeometryPropertyCount;
    GMLGeometryPropertyDefn **m_papoGeometryProperty;

    bool m_bSchemaLocked;

    GIntBig m_nFeatureCount;

    char *m_pszExtraInfo;

    bool m_bHaveExtents;
    double m_dfXMin;
    double m_dfXMax;
    double m_dfYMin;
    double m_dfYMax;

    char *m_pszSRSName;
    bool m_bSRSNameConsistent;

    bool m_bIsConsistentSingleGeomElemPath = true;
    std::string m_osSingleGeomElemPath{};

    CPL_DISALLOW_COPY_ASSIGN(GMLFeatureClass)

  public:
    explicit GMLFeatureClass(const char *pszName = "");
    ~GMLFeatureClass();

    const char *GetElementName() const;
    size_t GetElementNameLen() const;
    void SetElementName(const char *pszElementName);

    const char *GetName() const
    {
        return m_pszName;
    }

    void SetName(const char *pszNewName);

    int GetPropertyCount() const
    {
        return m_nPropertyCount;
    }

    GMLPropertyDefn *GetProperty(int iIndex) const;
    int GetPropertyIndex(const char *pszName) const;

    GMLPropertyDefn *GetProperty(const char *pszName) const
    {
        return GetProperty(GetPropertyIndex(pszName));
    }

    int GetPropertyIndexBySrcElement(const char *pszElement, int nLen) const;
    void StealProperties();

    int GetGeometryPropertyCount() const
    {
        return m_nGeometryPropertyCount;
    }

    GMLGeometryPropertyDefn *GetGeometryProperty(int iIndex) const;
    int GetGeometryPropertyIndexBySrcElement(const char *pszElement) const;
    void StealGeometryProperties();

    bool HasFeatureProperties();

    int AddProperty(GMLPropertyDefn *, int iPos = -1);
    int AddGeometryProperty(GMLGeometryPropertyDefn *);
    void ClearGeometryProperties();

    void SetConsistentSingleGeomElemPath(bool b)
    {
        m_bIsConsistentSingleGeomElemPath = b;
    }

    bool IsConsistentSingleGeomElemPath() const
    {
        return m_bIsConsistentSingleGeomElemPath;
    }

    void SetSingleGeomElemPath(const std::string &s)
    {
        m_osSingleGeomElemPath = s;
    }

    const std::string &GetSingleGeomElemPath() const
    {
        return m_osSingleGeomElemPath;
    }

    bool IsSchemaLocked() const
    {
        return m_bSchemaLocked;
    }

    void SetSchemaLocked(bool bLock)
    {
        m_bSchemaLocked = bLock;
    }

    const char *GetExtraInfo();
    void SetExtraInfo(const char *);

    GIntBig GetFeatureCount();
    void SetFeatureCount(GIntBig);

    bool HasExtents() const
    {
        return m_bHaveExtents;
    }

    void SetExtents(double dfXMin, double dfXMax, double dFYMin, double dfYMax);
    bool GetExtents(double *pdfXMin, double *pdfXMax, double *pdFYMin,
                    double *pdfYMax);

    void SetSRSName(const char *pszSRSName);
    void MergeSRSName(const char *pszSRSName);

    const char *GetSRSName()
    {
        return m_pszSRSName;
    }

    CPLXMLNode *SerializeToXML();
    bool InitializeFromXML(CPLXMLNode *);
};

/************************************************************************/
/*                              GMLFeature                              */
/************************************************************************/

class CPL_DLL GMLFeature
{
    GMLFeatureClass *m_poClass;
    char *m_pszFID;

    int m_nPropertyCount;
    GMLProperty *m_pasProperties;

    int m_nGeometryCount;
    CPLXMLNode **m_papsGeometry;  /* NULL-terminated. Alias to m_apsGeometry if
                                     m_nGeometryCount <= 1 */
    CPLXMLNode *m_apsGeometry[2]; /* NULL-terminated */

    CPLXMLNode *m_psBoundedByGeometry = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GMLFeature)

  public:
    explicit GMLFeature(GMLFeatureClass *);
    ~GMLFeature();

    GMLFeatureClass *GetClass() const
    {
        return m_poClass;
    }

    void SetGeometryDirectly(CPLXMLNode *psGeom);
    void SetGeometryDirectly(int nIdx, CPLXMLNode *psGeom);
    void AddGeometry(CPLXMLNode *psGeom);

    int GetGeometryCount() const
    {
        return m_nGeometryCount;
    }

    const CPLXMLNode *const *GetGeometryList() const
    {
        return m_papsGeometry;
    }

    const CPLXMLNode *GetGeometryRef(int nIdx) const;

    void SetBoundedByGeometry(CPLXMLNode *psGeom);

    const CPLXMLNode *GetBoundedByGeometry() const
    {
        return m_psBoundedByGeometry;
    }

    void SetPropertyDirectly(int i, char *pszValue);

    const GMLProperty *GetProperty(int i) const
    {
        return (i >= 0 && i < m_nPropertyCount) ? &m_pasProperties[i] : nullptr;
    }

    const char *GetFID() const
    {
        return m_pszFID;
    }

    void SetFID(const char *pszFID);

    void Dump(FILE *fp);
};

OGRFieldType CPL_DLL GML_GetOGRFieldType(GMLPropertyType eType,
                                         OGRFieldSubType &eSubType);

//! Map OGRFieldType to GMLPropertyType
GMLPropertyType CPL_DLL GML_FromOGRFieldType(OGRFieldType eType,
                                             OGRFieldSubType eSubType);

#endif
