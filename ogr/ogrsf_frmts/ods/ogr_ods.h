/******************************************************************************
 * $Id$
 *
 * Project:  ODS Translator
 * Purpose:  Definition of classes for OGR OpenOfficeSpreadsheet .ods driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_ODS_H_INCLUDED
#define OGR_ODS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_mem.h"

#include "ogr_expat.h"

#include <vector>
#include <string>
#include <set>

namespace OGRODS {

/************************************************************************/
/*                             OGRODSLayer                              */
/************************************************************************/

class OGRODSDataSource;

class OGRODSLayer final: public OGRMemLayer
{
    OGRODSDataSource* poDS;
    bool              bUpdated;
    bool              bHasHeaderLine;
    OGRFeatureQuery  *m_poAttrQueryODS;

    public:
        OGRODSLayer( OGRODSDataSource* poDSIn,
                      const char * pszName,
                      bool bUpdateIn = FALSE);
       ~OGRODSLayer();

    void                SetUpdated(bool bUpdatedIn = true);

    bool                GetHasHeaderLine() { return bHasHeaderLine; }
    void                SetHasHeaderLine(bool bIn) { bHasHeaderLine = bIn; }

    const char         *GetName() override { return OGRMemLayer::GetLayerDefn()->GetName(); }
    OGRwkbGeometryType  GetGeomType() override { return wkbNone; }
    virtual OGRSpatialReference *GetSpatialRef() override { return nullptr; }

    /* For external usage. Mess with FID */
    virtual OGRFeature *        GetNextFeature() override;
    virtual OGRFeature         *GetFeature( GIntBig nFeatureId ) override;
    virtual OGRErr              ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr              DeleteFeature( GIntBig nFID ) override;

    virtual GIntBig             GetFeatureCount( int ) override;

    virtual OGRErr              SetAttributeFilter( const char *pszQuery ) override;

    virtual int                 TestCapability( const char * pszCap ) override;

    /* For internal usage, for cell resolver */
    OGRFeature *        GetNextFeatureWithoutFIDHack() { return OGRMemLayer::GetNextFeature(); }
    OGRErr              SetFeatureWithoutFIDHack( OGRFeature *poFeature ) { SetUpdated(); return OGRMemLayer::ISetFeature(poFeature); }

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override
    { SetUpdated(); return OGRMemLayer::ICreateFeature(poFeature); }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override
    {  SetUpdated(); return OGRMemLayer::CreateField(poField, bApproxOK); }

    virtual OGRErr      DeleteField( int iField ) override
    { SetUpdated(); return OGRMemLayer::DeleteField(iField); }

    virtual OGRErr      ReorderFields( int* panMap ) override
    { SetUpdated(); return OGRMemLayer::ReorderFields(panMap); }

    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn ) override
    { SetUpdated(); return OGRMemLayer::AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn); }

    virtual OGRErr      SyncToDisk() override;
};

/************************************************************************/
/*                           OGRODSDataSource                           */
/************************************************************************/
#define STACK_SIZE 5

typedef enum
{
    STATE_DEFAULT,
    STATE_TABLE,
    STATE_ROW,
    STATE_CELL,
    STATE_TEXTP,
} HandlerStateEnum;

typedef struct
{
    HandlerStateEnum  eVal;
    int               nBeginDepth;
} HandlerState;

class OGRODSDataSource final: public GDALDataset
{
    char*               pszName;
    bool                bUpdatable;
    bool                bUpdated;
    bool                bAnalysedFile;

    int                 nLayers;
    OGRLayer          **papoLayers;

    VSILFILE*           fpSettings;
    std::string         osCurrentConfigTableName;
    std::string         osConfigName;
    int                 nVerticalSplitFlags;
    std::set<std::string> osSetLayerHasSplitter;
    void                AnalyseSettings();

    VSILFILE*           fpContent;
    void                AnalyseFile();

    bool                bFirstLineIsHeaders;
    int                 bAutodetectTypes;

    XML_Parser          oParser;
    bool                bStopParsing;
    int                 nWithoutEventCounter;
    int                 nDataHandlerCounter;
    int                 nCurLine;
    int                 nEmptyRowsAccumulated;
    int                 nRowsRepeated;
    int                 nCurCol;
    int                 nCellsRepeated;
    // Accumulated memory allocations related to repeated cells.
    size_t              m_nAccRepeatedMemory = 0;
    bool                bEndTableParsing;

    OGRODSLayer        *poCurLayer;

    int                 nStackDepth;
    int                 nDepth;
    HandlerState        stateStack[STACK_SIZE];

    CPLString           osValueType;
    CPLString           osValue;
    bool                m_bValueFromTableCellAttribute = false;
    std::string         osFormula;

    std::vector<std::string>  apoFirstLineValues;
    std::vector<std::string>  apoFirstLineTypes;
    std::vector<std::string>  apoCurLineValues;
    std::vector<std::string>  apoCurLineTypes;

    void                PushState(HandlerStateEnum eVal);
    void                startElementDefault(const char *pszName, const char **ppszAttr);
    void                startElementTable(const char *pszName, const char **ppszAttr);
    void                endElementTable(const char *pszName);
    void                startElementRow(const char *pszName, const char **ppszAttr);
    void                endElementRow(const char *pszName);
    void                startElementCell(const char *pszName, const char **ppszAttr);
    void                endElementCell(const char *pszName);
    void                dataHandlerTextP(const char *data, int nLen);

    void                DetectHeaderLine();

    OGRFieldType        GetOGRFieldType(const char* pszValue,
                                        const char* pszValueType,
                                        OGRFieldSubType& eSubType);

    void                DeleteLayer( const char *pszLayerName );

    void                FillRepeatedCells(bool wasLastCell);

  public:
                        OGRODSDataSource();
                        virtual ~OGRODSDataSource();

    int                 Open( const char * pszFilename,
                              VSILFILE* fpContentIn,
                              VSILFILE* fpSettingsIn,
                              int bUpdatableIn );
    int                 Create( const char * pszName, char **papszOptions );

    virtual int                 GetLayerCount() override;
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;

    virtual OGRLayer* ICreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** papszOptions ) override;
    virtual OGRErr      DeleteLayer(int iLayer) override;

    virtual void        FlushCache(bool bAtClosing) override;

    void startElementCbk(const char *pszName, const char **ppszAttr);
    void endElementCbk(const char *pszName);
    void dataHandlerCbk(const char *data, int nLen);

    void startElementStylesCbk(const char *pszName, const char **ppszAttr);
    void endElementStylesCbk(const char *pszName);
    void dataHandlerStylesCbk(const char *data, int nLen);

    bool                GetUpdatable() { return bUpdatable; }
    void                SetUpdated() { bUpdated = true; }
};

} /* end of OGRODS namespace */

#endif /* ndef OGR_ODS_H_INCLUDED */
