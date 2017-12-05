/******************************************************************************
 * $Id$
 *
 * Project:  XLSX Translator
 * Purpose:  Definition of classes for OGR OpenOfficeSpreadsheet .xlsx driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_XLSX_H_INCLUDED
#define OGR_XLSX_H_INCLUDED

#include "ogrsf_frmts.h"

#include "ogr_expat.h"
#include "ogr_mem.h"

#include <vector>
#include <string>
#include <map>

namespace OGRXLSX {

/************************************************************************/
/*                             OGRXLSXLayer                             */
/************************************************************************/

class OGRXLSXDataSource;

class OGRXLSXLayer : public OGRMemLayer
{
    bool               bInit;
    OGRXLSXDataSource* poDS;
    CPLString          osFilename;
    void               Init();
    bool               bUpdated;
    bool               bHasHeaderLine;

  public:
        OGRXLSXLayer( OGRXLSXDataSource* poDSIn,
                      const char * pszFilename,
                      const char * pszName,
                      int bUpdateIn = FALSE);

    bool                HasBeenUpdated() const { return bUpdated; }
    void                SetUpdated( bool bUpdatedIn = true );

    bool                GetHasHeaderLine() const { return bHasHeaderLine; }
    void                SetHasHeaderLine( bool bIn ) { bHasHeaderLine = bIn; }

    const char         *GetName() override { return OGRMemLayer::GetLayerDefn()->GetName(); }
    OGRwkbGeometryType  GetGeomType() override { return wkbNone; }
    virtual OGRSpatialReference *GetSpatialRef() override { return NULL; }

    void                ResetReading() override
    { Init(); OGRMemLayer::ResetReading(); }

    const CPLString&    GetFilename() const { return osFilename; }

    /* For external usage. Mess with FID */
    virtual OGRFeature *        GetNextFeature() override;
    virtual OGRFeature         *GetFeature( GIntBig nFeatureId ) override;
    virtual OGRErr              ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr              DeleteFeature( GIntBig nFID ) override;

    virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override
    { Init(); return OGRMemLayer::SetNextByIndex(nIndex); }

    virtual OGRErr              ICreateFeature( OGRFeature *poFeature ) override;

    OGRFeatureDefn *    GetLayerDefn() override
    { Init(); return OGRMemLayer::GetLayerDefn(); }

    GIntBig                 GetFeatureCount( int bForce ) override
    { Init(); return OGRMemLayer::GetFeatureCount(bForce); }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override
    { Init(); SetUpdated(); return OGRMemLayer::CreateField(poField, bApproxOK); }

    virtual OGRErr      DeleteField( int iField ) override
    { Init(); SetUpdated(); return OGRMemLayer::DeleteField(iField); }

    virtual OGRErr      ReorderFields( int* panMap ) override
    { Init(); SetUpdated(); return OGRMemLayer::ReorderFields(panMap); }

    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn ) override
    { Init(); SetUpdated(); return OGRMemLayer::AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn); }

    int                 TestCapability( const char * pszCap ) override
    { Init(); return OGRMemLayer::TestCapability(pszCap); }

    virtual OGRErr      SyncToDisk() override;
};

/************************************************************************/
/*                           OGRXLSXDataSource                          */
/************************************************************************/
#define STACK_SIZE 5

typedef enum
{
    STATE_DEFAULT,

    /* for sharedString.xml */
    STATE_T,

    /* for sheet?.xml */
    STATE_SHEETDATA,
    STATE_ROW,
    STATE_CELL,
    STATE_TEXTV,
} HandlerStateEnum;

typedef struct
{
    HandlerStateEnum  eVal;
    int               nBeginDepth;
} HandlerState;

class XLSXFieldTypeExtended
{
public:
    OGRFieldType      eType;
    bool              bHasMS;

                    XLSXFieldTypeExtended() :
                        eType(OFTMaxType),
                        bHasMS(false) {}
                    XLSXFieldTypeExtended(OGRFieldType eTypeIn,
                                          bool bHasMSIn = false) :
                                    eType(eTypeIn), bHasMS(bHasMSIn) {}
};

class OGRXLSXDataSource : public GDALDataset
{
    char*               pszName;
    CPLString           osPrefixedFilename;
    bool                bUpdatable;
    bool                bUpdated;

    int                 nLayers;
    OGRLayer          **papoLayers;
    std::map<CPLString, CPLString> oMapRelsIdToTarget;

    void                AnalyseSharedStrings(VSILFILE* fpSharedStrings);
    void                AnalyseWorkbook(VSILFILE* fpWorkbook);
    void                AnalyseWorkbookRels(VSILFILE* fpWorkbookRels);
    void                AnalyseStyles(VSILFILE* fpStyles);

    std::vector<std::string>  apoSharedStrings;
    std::string         osCurrentString;

    bool                bFirstLineIsHeaders;
    int                 bAutodetectTypes;

    XML_Parser          oParser;
    bool                bStopParsing;
    int                 nWithoutEventCounter;
    int                 nDataHandlerCounter;
    int                 nCurLine;
    int                 nCurCol;

    OGRXLSXLayer       *poCurLayer;

    int                 nStackDepth;
    int                 nDepth;
    HandlerState        stateStack[STACK_SIZE];

    CPLString           osValueType;
    CPLString           osValue;

    std::vector<std::string>  apoFirstLineValues;
    std::vector<std::string>  apoFirstLineTypes;
    std::vector<std::string>  apoCurLineValues;
    std::vector<std::string>  apoCurLineTypes;

    bool                bInCellXFS;
    std::map<int,XLSXFieldTypeExtended> apoMapStyleFormats;
    std::vector<XLSXFieldTypeExtended>  apoStyles;

    void                PushState(HandlerStateEnum eVal);
    void                startElementDefault(const char *pszName, const char **ppszAttr);
    void                startElementTable(const char *pszName, const char **ppszAttr);
    void                endElementTable(const char *pszName);
    void                startElementRow(const char *pszName, const char **ppszAttr);
    void                endElementRow(const char *pszName);
    void                startElementCell(const char *pszName, const char **ppszAttr);
    void                endElementCell(const char *pszName);
    void                dataHandlerTextV(const char *data, int nLen);

    void                DetectHeaderLine();

    OGRFieldType        GetOGRFieldType(const char* pszValue,
                                        const char* pszValueType);

    void                DeleteLayer( const char *pszLayerName );

  public:
                        OGRXLSXDataSource();
                        virtual ~OGRXLSXDataSource();

    int                 Open( const char * pszFilename,
                              const char * pszPrefixedFilename,
                              VSILFILE* fpWorkbook,
                              VSILFILE* fpWorkbookRels,
                              VSILFILE* fpSharedStrings,
                              VSILFILE* fpStyles,
                              int bUpdate );
    int                 Create( const char * pszName, char **papszOptions );

    virtual int                 GetLayerCount() override;
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;

    virtual OGRLayer* ICreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** papszOptions ) override;
    virtual OGRErr      DeleteLayer(int iLayer) override;

    virtual void        FlushCache() override;

    void                startElementCbk(const char *pszName, const char **ppszAttr);
    void                endElementCbk(const char *pszName);
    void                dataHandlerCbk(const char *data, int nLen);

    void                startElementSSCbk(const char *pszName, const char **ppszAttr);
    void                endElementSSCbk(const char *pszName);
    void                dataHandlerSSCbk(const char *data, int nLen);

    void                startElementWBRelsCbk(const char *pszName, const char **ppszAttr);

    void                startElementWBCbk(const char *pszName, const char **ppszAttr);

    void                startElementStylesCbk(const char *pszName, const char **ppszAttr);
    void                endElementStylesCbk(const char *pszName);

    void                BuildLayer(OGRXLSXLayer* poLayer);

    bool                GetUpdatable() { return bUpdatable; }
    void                SetUpdated() { bUpdated = true; }
};

} /* end of OGRXLSX namespace */

#endif /* ndef OGR_XLSX_H_INCLUDED */
