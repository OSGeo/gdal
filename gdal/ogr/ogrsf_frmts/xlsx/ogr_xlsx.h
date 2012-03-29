/******************************************************************************
 * $Id$
 *
 * Project:  XLSX Translator
 * Purpose:  Definition of classes for OGR OpenOfficeSpreadsheet .xlsx driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#ifndef _OGR_XLSX_H_INCLUDED
#define _OGR_XLSX_H_INCLUDED

#include "ogrsf_frmts.h"

#include "ogr_expat.h"
#include "ogr_mem.h"

#include <vector>
#include <string>
#include <map>

/************************************************************************/
/*                             OGRXLSXLayer                             */
/************************************************************************/

class OGRXLSXDataSource;

class OGRXLSXLayer : public OGRMemLayer
{
    int                bInit;
    OGRXLSXDataSource* poDS;
    int                nSheetId;
    void               Init();
    int                bUpdated;
    int                bHasHeaderLine;

    public:
        OGRXLSXLayer( OGRXLSXDataSource* poDSIn,
                      int nSheetIdIn,
                      const char * pszName,
                      int bUpdateIn = FALSE);

    int                 HasBeenUpdated() { return bUpdated; }
    void                SetUpdated(int bUpdatedIn = TRUE);

    int                 GetHasHeaderLine() { return bHasHeaderLine; }
    void                SetHasHeaderLine(int bIn) { bHasHeaderLine = bIn; }

    const char         *GetName() { return OGRMemLayer::GetLayerDefn()->GetName(); };
    OGRwkbGeometryType  GetGeomType() { return wkbNone; }
    virtual OGRSpatialReference *GetSpatialRef() { return NULL; }

    void                ResetReading()
    { Init(); OGRMemLayer::ResetReading(); }

    /* For external usage. Mess with FID */
    virtual OGRFeature *        GetNextFeature();
    virtual OGRFeature         *GetFeature( long nFeatureId );
    virtual OGRErr              SetFeature( OGRFeature *poFeature );
    virtual OGRErr              DeleteFeature( long nFID );

    virtual OGRErr      SetNextByIndex( long nIndex )
    { Init(); return OGRMemLayer::SetNextByIndex(nIndex); }

    OGRErr              CreateFeature( OGRFeature *poFeature )
    { Init(); SetUpdated(); return OGRMemLayer::CreateFeature(poFeature); }

    OGRFeatureDefn *    GetLayerDefn()
    { Init(); return OGRMemLayer::GetLayerDefn(); }

    int                 GetFeatureCount( int bForce )
    { Init(); return OGRMemLayer::GetFeatureCount(bForce); }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE )
    { Init(); SetUpdated(); return OGRMemLayer::CreateField(poField, bApproxOK); }

    virtual OGRErr      DeleteField( int iField )
    { Init(); SetUpdated(); return OGRMemLayer::DeleteField(iField); }

    virtual OGRErr      ReorderFields( int* panMap )
    { Init(); SetUpdated(); return OGRMemLayer::ReorderFields(panMap); }

    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
    { Init(); SetUpdated(); return OGRMemLayer::AlterFieldDefn(iField, poNewFieldDefn, nFlags); }

    int                 TestCapability( const char * pszCap )
    { Init(); return OGRMemLayer::TestCapability(pszCap); }

    virtual OGRErr      SyncToDisk();
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

class OGRXLSXDataSource : public OGRDataSource
{
    char*               pszName;
    int                 bUpdatable;
    int                 bUpdated;

    int                 nLayers;
    OGRLayer          **papoLayers;

    void                AnalyseSharedStrings(VSILFILE* fpSharedStrings);
    void                AnalyseWorkbook(VSILFILE* fpWorkbook);
    void                AnalyseStyles(VSILFILE* fpStyles);

    std::vector<std::string>  apoSharedStrings;
    std::string         osCurrentString;

    int                 bFirstLineIsHeaders;
    int                 bAutodetectTypes;

    XML_Parser          oParser;
    int                 bStopParsing;
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

    int                        bInCellXFS;
    std::map<int,OGRFieldType> apoMapStyleFormats;
    std::vector<OGRFieldType>  apoStyles;

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
                        ~OGRXLSXDataSource();

    int                 Open( const char * pszFilename,
                              VSILFILE* fpWorkbook,
                              VSILFILE* fpSharedStrings,
                              VSILFILE* fpStyles,
                              int bUpdate );
    int                 Create( const char * pszName, char **papszOptions );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount();
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );

    virtual OGRLayer* CreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** papszOptions );
    virtual OGRErr      DeleteLayer(int iLayer);

    virtual OGRErr      SyncToDisk();

    void                startElementCbk(const char *pszName, const char **ppszAttr);
    void                endElementCbk(const char *pszName);
    void                dataHandlerCbk(const char *data, int nLen);

    void                startElementSSCbk(const char *pszName, const char **ppszAttr);
    void                endElementSSCbk(const char *pszName);
    void                dataHandlerSSCbk(const char *data, int nLen);

    void                startElementWBCbk(const char *pszName, const char **ppszAttr);

    void                startElementStylesCbk(const char *pszName, const char **ppszAttr);
    void                endElementStylesCbk(const char *pszName);

    void                BuildLayer(OGRXLSXLayer* poLayer, int nSheetId);

    int                 GetUpdatable() { return bUpdatable; }
    void                SetUpdated() { bUpdated = TRUE; }
};

/************************************************************************/
/*                             OGRXLSXDriver                             */
/************************************************************************/

class OGRXLSXDriver : public OGRSFDriver
{
  public:
                ~OGRXLSXDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    virtual OGRErr      DeleteDataSource( const char *pszName );
    
};


#endif /* ndef _OGR_XLSX_H_INCLUDED */
