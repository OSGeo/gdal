/******************************************************************************
* $Id$
*
* Project:  OpenGIS Simple Features Reference Implementation
* Purpose:  Implements Open FileGDB OGR driver.
* Author:   Even Rouault, <even dot rouault at spatialys.com>
*
******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
****************************************************************************/

#ifndef OGR_OPENFILEGDB_H_INCLUDED
#define OGR_OPENFILEGDB_H_INCLUDED

#include "ogrsf_frmts.h"
#include "filegdbtable.h"
#include "ogr_swq.h"
#include "cpl_quad_tree.h"

#include <vector>
#include <map>

using namespace OpenFileGDB;

/************************************************************************/
/*                      OGROpenFileGDBLayer                             */
/************************************************************************/

class OGROpenFileGDBDataSource;
class OGROpenFileGDBGeomFieldDefn;
class OGROpenFileGDBFeatureDefn;

typedef enum
{
    SPI_IN_BUILDING,
    SPI_COMPLETED,
    SPI_INVALID,
} SPIState;

class OGROpenFileGDBLayer final: public OGRLayer
{
    friend class OGROpenFileGDBGeomFieldDefn;
    friend class OGROpenFileGDBFeatureDefn;

    CPLString         m_osGDBFilename;
    CPLString         m_osName;
    FileGDBTable     *m_poLyrTable;
    OGROpenFileGDBFeatureDefn   *m_poFeatureDefn;
    int               m_iGeomFieldIdx;
    int               m_iCurFeat;
    std::string       m_osDefinition;
    std::string       m_osDocumentation;
    OGRwkbGeometryType m_eGeomType;
    int               m_bValidLayerDefn;
    int               m_bEOF;
    bool              m_bTimeInUTC = false;

    int               BuildLayerDefinition();
    int               BuildGeometryColumnGDBv10();
    OGRFeature       *GetCurrentFeature();

    FileGDBOGRGeometryConverter* m_poGeomConverter;

    int               m_iFieldToReadAsBinary;

    FileGDBIterator      *m_poAttributeIterator;
    int                   m_bIteratorSufficientToEvaluateFilter;
    FileGDBIterator*      BuildIteratorFromExprNode(swq_expr_node* poNode);

    FileGDBIterator*      m_poIterMinMax;

    FileGDBSpatialIndexIterator* m_poSpatialIndexIterator = nullptr;
    FileGDBIterator      *m_poCombinedIterator = nullptr;

    // Legacy behavior prior to handling of .spx file
    // To remove ultimately.
    SPIState            m_eSpatialIndexState;
    CPLQuadTree        *m_pQuadTree;
    void              **m_pahFilteredFeatures;
    int                 m_nFilteredFeatureCount;
    static void         GetBoundsFuncEx(const void* hFeature,
                                        CPLRectObj* pBounds,
                                        void* pQTUserData);

    void                TryToDetectMultiPatchKind();
    void                BuildCombinedIterator();

public:

                        OGROpenFileGDBLayer(const char* pszGDBFilename,
                                            const char* pszName,
                                            const std::string& osDefinition,
                                            const std::string& osDocumentation,
                                            const char* pszGeomName = nullptr,
                                            OGRwkbGeometryType eGeomType = wkbUnknown);
  virtual              ~OGROpenFileGDBLayer();

  const std::string&    GetXMLDefinition() { return m_osDefinition; }
  const std::string&    GetXMLDocumentation() { return m_osDocumentation; }
  int                   GetAttrIndexUse() { return (m_poAttributeIterator == nullptr) ? 0 : (m_bIteratorSufficientToEvaluateFilter) ? 2 : 1; }
  const OGRField*       GetMinMaxValue(OGRFieldDefn* poFieldDefn, int bIsMin,
                                       int& eOutType);
  int                   GetMinMaxSumCount(OGRFieldDefn* poFieldDefn,
                                          double& dfMin, double& dfMax,
                                          double& dfSum, int& nCount);
  int                   HasIndexForField(const char* pszFieldName);
  FileGDBIterator*      BuildIndex(const char* pszFieldName,
                                   int bAscending,
                                   int op,
                                   swq_expr_node* poValue);
  SPIState              GetSpatialIndexState() const { return m_eSpatialIndexState; }
  int                   IsValidLayerDefn() { return BuildLayerDefinition(); }

  virtual const char* GetName() override { return m_osName.c_str(); }
  virtual OGRwkbGeometryType GetGeomType() override;

  virtual const char* GetFIDColumn() override;

  virtual void        ResetReading() override;
  virtual OGRFeature* GetNextFeature() override;
  virtual OGRFeature* GetFeature( GIntBig nFeatureId ) override;
  virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override;

  virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
  virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
  virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

  virtual OGRFeatureDefn* GetLayerDefn() override;

  virtual void        SetSpatialFilter( OGRGeometry * ) override;
  virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }
  virtual OGRErr      SetAttributeFilter( const char* pszFilter ) override;

  virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                       OGROpenFileGDBDataSource                       */
/************************************************************************/

class OGROpenFileGDBDataSource final: public OGRDataSource
{
  char                          *m_pszName;
  CPLString                      m_osDirName;
  std::vector <OGRLayer*>        m_apoLayers;
  std::vector <OGRLayer*>        m_apoHiddenLayers;
  char                         **m_papszFiles;
  std::map<std::string, int>     m_osMapNameToIdx;
  std::shared_ptr<GDALGroup>     m_poRootGroup{};

  /* For debugging/testing */
  bool                           bLastSQLUsedOptimizedImplementation;

  int                 OpenFileGDBv10(int iGDBItems,
                                     int nInterestTable);
  int                 OpenFileGDBv9 (int iGDBFeatureClasses,
                                     int iGDBObjectClasses,
                                     int nInterestTable);

  int                 FileExists(const char* pszFilename);
  OGRLayer*           AddLayer( const CPLString& osName,
                                int nInterestTable,
                                int& nCandidateLayers,
                                int& nLayersSDCOrCDF,
                                const CPLString& osDefinition,
                                const CPLString& osDocumentation,
                                const char* pszGeomName,
                                OGRwkbGeometryType eGeomType );

public:
           OGROpenFileGDBDataSource();
  virtual ~OGROpenFileGDBDataSource();

  int                 Open(const char * );

  virtual const char* GetName() override { return m_pszName; }
  virtual int         GetLayerCount() override { return static_cast<int>(m_apoLayers.size()); }

  virtual OGRLayer*   GetLayer( int ) override;
  virtual OGRLayer*   GetLayerByName( const char* pszName ) override;

  virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                  OGRGeometry *poSpatialFilter,
                                  const char *pszDialect ) override;
  virtual void        ReleaseResultSet( OGRLayer * poResultsSet ) override;

  virtual int         TestCapability( const char * ) override;

  virtual char      **GetFileList() override;

  std::shared_ptr<GDALGroup> GetRootGroup() const override { return m_poRootGroup; }
};

int OGROpenFileGDBIsComparisonOp(int op);

#endif /* ndef OGR_OPENFILEGDB_H_INCLUDED */
