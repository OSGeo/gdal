/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Definition of classes for OGR .dwg driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011,  Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_DWG_H_INCLUDED
#define OGR_DWG_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include <vector>
#include <map>
#include <set>
#include <queue>

#include "ogr_autocad_services.h"
#include "dwg_headers.h"

class OGRDWGDataSource;
class OGRDWGServices;

/************************************************************************/
/*                          DWGBlockDefinition                          */
/*                                                                      */
/*      Container for info about a block.                               */
/************************************************************************/

class DWGBlockDefinition
{
  public:
    DWGBlockDefinition() : poGeometry(nullptr)
    {
    }

    ~DWGBlockDefinition();

    OGRGeometry *poGeometry;
    std::vector<OGRFeature *> apoFeatures;

  private:
    CPL_DISALLOW_COPY_ASSIGN(DWGBlockDefinition)
};

/************************************************************************/
/*                         OGRDWGBlocksLayer()                          */
/************************************************************************/

class OGRDWGBlocksLayer final : public OGRLayer
{
    OGRDWGDataSource *poDS;

    OGRFeatureDefn *poFeatureDefn;

    int iNextFID;
    unsigned int iNextSubFeature;

    std::map<CPLString, DWGBlockDefinition>::iterator oIt;

  public:
    explicit OGRDWGBlocksLayer(OGRDWGDataSource *poDS);
    ~OGRDWGBlocksLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) override;

    OGRFeature *GetNextUnfilteredFeature();

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/*                             OGRDWGLayer                              */
/************************************************************************/
class OGRDWGLayer final : public OGRLayer
{
    OGRDWGDataSource *poDS;

    OGRFeatureDefn *poFeatureDefn;
    int iNextFID;

    std::set<CPLString> oIgnoredEntities;

    std::queue<OGRFeature *> apoPendingFeatures;
    void ClearPendingFeatures();

    std::map<CPLString, CPLString> oStyleProperties;

    void TranslateGenericProperties(OGRFeature *poFeature,
                                    OdDbEntityPtr poEntity);
    void PrepareLineStyle(OGRFeature *poFeature);
    //    void                ApplyOCSTransformer( OGRGeometry * );

    OGRFeature *TranslatePOINT(OdDbEntityPtr poEntity);
    OGRFeature *TranslateLINE(OdDbEntityPtr poEntity);
    OGRFeature *TranslateLWPOLYLINE(OdDbEntityPtr poEntity);
    OGRFeature *Translate2DPOLYLINE(OdDbEntityPtr poEntity);
    OGRFeature *Translate3DPOLYLINE(OdDbEntityPtr poEntity);
    OGRFeature *TranslateELLIPSE(OdDbEntityPtr poEntity);
    OGRFeature *TranslateARC(OdDbEntityPtr poEntity);
    OGRFeature *TranslateMTEXT(OdDbEntityPtr poEntity);
    OGRFeature *TranslateDIMENSION(OdDbEntityPtr poEntity);
    OGRFeature *TranslateCIRCLE(OdDbEntityPtr poEntity);
    OGRFeature *TranslateSPLINE(OdDbEntityPtr poEntity);
    OGRFeature *TranslateHATCH(OdDbEntityPtr poEntity);
    OGRFeature *TranslateTEXT(OdDbEntityPtr poEntity);
    OGRFeature *TranslateINSERT(OdDbEntityPtr poEntity);
    OGRFeature *Translate3DFACE(OdDbEntityPtr poEntity);

    void FormatDimension(CPLString &osText, double dfValue);

    CPLString TextUnescape(OdString oString, bool);

    OdDbBlockTableRecordPtr m_poBlock;
    OdDbObjectIteratorPtr poEntIter;

  public:
    explicit OGRDWGLayer(OGRDWGDataSource *poDS);
    ~OGRDWGLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) override;

    OGRFeature *GetNextUnfilteredFeature();

    // internal
    void SetBlockTable(OdDbBlockTableRecordPtr);
    static double AngleCorrect(double dfAngle, double dfRatio);

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/*                           OGRDWGDataSource                           */
/************************************************************************/

class OGRDWGDataSource final : public GDALDataset
{
    std::vector<OGRLayer *> apoLayers;

    std::set<CPLString> attributeFields;

    std::map<CPLString, DWGBlockDefinition> oBlockMap;
    std::map<CPLString, CPLString> oHeaderVariables;

    CPLString osEncoding;

    // indexed by layer name, then by property name.
    std::map<CPLString, std::map<CPLString, CPLString>> oLayerTable;

    std::map<CPLString, CPLString> oLineTypeTable;

    int bInlineBlocks;
    int bAttributes;
    int bAllAttributes;

    bool m_bClosedLineAsPolygon = false;

    OGRDWGServices *poServices;
    OdDbDatabasePtr poDb;

  public:
    OGRDWGDataSource();
    ~OGRDWGDataSource();

    OdDbDatabasePtr GetDB()
    {
        return poDb;
    }

    int Open(OGRDWGServices *poServices, const char *pszFilename,
             int bHeaderOnly = FALSE);

    int GetLayerCount() override
    {
        return static_cast<int>(apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;

    // The following is only used by OGRDWGLayer

    int InlineBlocks()
    {
        return bInlineBlocks;
    }

    int Attributes()
    {
        return bAttributes;
    }

    int AllAttributes()
    {
        return bAllAttributes;
    }

    bool ClosedLineAsPolygon() const
    {
        return m_bClosedLineAsPolygon;
    }

    void AddStandardFields(OGRFeatureDefn *poDef);

    // Implemented in ogrdxf_blockmap.cpp
    void ReadBlocksSection();
    void ReadAttDefinitions();
    static OGRGeometry *SimplifyBlockGeometry(OGRGeometryCollection *);
    DWGBlockDefinition *LookupBlock(const char *pszName);

    std::map<CPLString, DWGBlockDefinition> &GetBlockMap()
    {
        return oBlockMap;
    }

    std::set<CPLString> &GetAttributes()
    {
        return attributeFields;
    }

    // Layer and other Table Handling (ogrdatasource.cpp)
    void ReadLayerDefinitions();
    void ReadLineTypeDefinitions();
    const char *LookupLayerProperty(const char *pszLayer,
                                    const char *pszProperty);
    const char *LookupLineType(const char *pszName);

    // Header variables.
    void ReadHeaderSection();
    const char *GetVariable(const char *pszName,
                            const char *pszDefault = nullptr);

    const char *GetEncoding()
    {
        return osEncoding;
    }
};

/************************************************************************/
/*                            OGRDWGServices                            */
/*                                                                      */
/*      Services implementation for OGR.  Eventually we should          */
/*      override the ExSystemServices IO to use VSI*L.                  */
/************************************************************************/
class OGRDWGServices : public ExSystemServices, public ExHostAppServices
{
  protected:
    ODRX_USING_HEAP_OPERATORS(ExSystemServices);
};

#endif /* ndef OGR_DWG_H_INCLUDED */
