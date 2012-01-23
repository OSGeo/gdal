/******************************************************************************
 * $Id$
 *
 * Project:  ODS Translator
 * Purpose:  Definition of classes for OGR OpenOfficeSpreadsheet .ods driver.
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

#ifndef _OGR_ODS_H_INCLUDED
#define _OGR_ODS_H_INCLUDED

#include "ogrsf_frmts.h"

#include "ogr_expat.h"

#include <vector>
#include <string>

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

class OGRODSDataSource : public OGRDataSource
{
    char*               pszName;

    VSILFILE*           fpContent;

    void                AnalyseFile();

    int                 bFirstLineIsHeaders;
    int                 bAutodetectTypes;

    XML_Parser          oParser;
    int                 bStopParsing;
    int                 nWithoutEventCounter;
    int                 nDataHandlerCounter;
    int                 nCurLine;
    int                 nCurCol;
    int                 nRowsRepeated;
    int                 nCellsRepeated;

    OGRDataSource      *poMemDS;
    OGRLayer           *poCurLayer;

    int                 nStackDepth;
    int                 nDepth;
    HandlerState        stateStack[STACK_SIZE];

    CPLString           osValueType;
    CPLString           osValue;

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
                                        const char* pszValueType);

  public:
                        OGRODSDataSource();
                        ~OGRODSDataSource();

    int                 Open( const char * pszFilename,
                              VSILFILE* fpContentIn,
                              int bUpdate );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount();
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );

    void startElementCbk(const char *pszName, const char **ppszAttr);
    void endElementCbk(const char *pszName);
    void dataHandlerCbk(const char *data, int nLen);
};

/************************************************************************/
/*                             OGRODSDriver                             */
/************************************************************************/

class OGRODSDriver : public OGRSFDriver
{
  public:
                ~OGRODSDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_ODS_H_INCLUDED */
