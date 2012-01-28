/******************************************************************************
 * $Id$
 *
 * Project:  XLSX Translator
 * Purpose:  Implements OGRXLSXDataSource class
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

#include "ogr_xlsx.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_time.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRXLSXLayer()                            */
/************************************************************************/

OGRXLSXLayer::OGRXLSXLayer( OGRXLSXDataSource* poDSIn,
                            int nSheetIdIn,
                            const char * pszName) :
                                OGRMemLayer(pszName, NULL, wkbNone)
{
    bInit = FALSE;
    nSheetId = nSheetIdIn;
    poDS = poDSIn;
}

/************************************************************************/
/*                              Init()                                  */
/************************************************************************/

void OGRXLSXLayer::Init()
{
    if (!bInit)
    {
        bInit = TRUE;
        CPLDebug("XLSX", "Init(%s)", GetName());
        poDS->BuildLayer(this, nSheetId);
    }
}

/************************************************************************/
/*                          OGRXLSXDataSource()                          */
/************************************************************************/

OGRXLSXDataSource::OGRXLSXDataSource()

{
    pszName = NULL;

    nLayers = 0;
    papoLayers = NULL;

    bFirstLineIsHeaders = FALSE;

    oParser = NULL;
    bStopParsing = FALSE;
    nWithoutEventCounter = 0;
    nDataHandlerCounter = 0;
    nStackDepth = 0;
    nDepth = 0;
    nCurLine = 0;
    nCurCol = 0;
    stateStack[0].eVal = STATE_DEFAULT;
    stateStack[0].nBeginDepth = 0;
    bInCellXFS = FALSE;

    poCurLayer = NULL;

    const char* pszXLSXFieldTypes =
                CPLGetConfigOption("OGR_XLSX_FIELD_TYPES", "");
    bAutodetectTypes = !EQUAL(pszXLSXFieldTypes, "STRING");
}

/************************************************************************/
/*                         ~OGRXLSXDataSource()                          */
/************************************************************************/

OGRXLSXDataSource::~OGRXLSXDataSource()

{
    CPLFree( pszName );

    for(int i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRXLSXDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRXLSXDataSource::GetLayer( int iLayer )

{
    if (iLayer < 0 || iLayer >= nLayers)
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRXLSXDataSource::GetLayerCount()
{
    return nLayers;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRXLSXDataSource::Open( const char * pszFilename,
                             VSILFILE* fpWorkbook,
                             VSILFILE* fpSharedStrings,
                             VSILFILE* fpStyles,
                             int bUpdateIn )

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );

    AnalyseWorkbook(fpWorkbook);
    AnalyseSharedStrings(fpSharedStrings);
    AnalyseStyles(fpStyles);

    /* Remove empty layers at the end, which tend to be there */
    while(nLayers > 1)
    {
        if (papoLayers[nLayers-1]->GetFeatureCount() == 0)
        {
            delete papoLayers[nLayers-1];
            nLayers --;
        }
        else
            break;
    }

    return TRUE;
}

/************************************************************************/
/*                           startElementCbk()                          */
/************************************************************************/

static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRXLSXDataSource*)pUserData)->startElementCbk(pszName, ppszAttr);
}

void OGRXLSXDataSource::startElementCbk(const char *pszName,
                                       const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: startElementDefault(pszName, ppszAttr); break;
        case STATE_SHEETDATA:   startElementTable(pszName, ppszAttr); break;
        case STATE_ROW:     startElementRow(pszName, ppszAttr); break;
        case STATE_CELL:    startElementCell(pszName, ppszAttr); break;
        case STATE_TEXTV:   break;
        default:            break;
    }
    nDepth++;
}

/************************************************************************/
/*                            endElementCbk()                           */
/************************************************************************/

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    ((OGRXLSXDataSource*)pUserData)->endElementCbk(pszName);
}

void OGRXLSXDataSource::endElementCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    nDepth--;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: break;
        case STATE_SHEETDATA:   endElementTable(pszName); break;
        case STATE_ROW:     endElementRow(pszName); break;
        case STATE_CELL:    endElementCell(pszName); break;
        case STATE_TEXTV:   break;
        default:            break;
    }

    if (stateStack[nStackDepth].nBeginDepth == nDepth)
        nStackDepth --;
}

/************************************************************************/
/*                            dataHandlerCbk()                          */
/************************************************************************/

static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRXLSXDataSource*)pUserData)->dataHandlerCbk(data, nLen);
}

void OGRXLSXDataSource::dataHandlerCbk(const char *data, int nLen)
{
    if (bStopParsing) return;

    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = TRUE;
        return;
    }

    nWithoutEventCounter = 0;

    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: break;
        case STATE_SHEETDATA:   break;
        case STATE_ROW:     break;
        case STATE_CELL:    break;
        case STATE_TEXTV:   dataHandlerTextV(data, nLen);
        default:            break;
    }
}

/************************************************************************/
/*                                PushState()                           */
/************************************************************************/

void OGRXLSXDataSource::PushState(HandlerStateEnum eVal)
{
    if (nStackDepth + 1 == STACK_SIZE)
    {
        bStopParsing = TRUE;
        return;
    }
    nStackDepth ++;
    stateStack[nStackDepth].eVal = eVal;
    stateStack[nStackDepth].nBeginDepth = nDepth;
}

/************************************************************************/
/*                          GetAttributeValue()                         */
/************************************************************************/

static const char* GetAttributeValue(const char **ppszAttr,
                                     const char* pszKey,
                                     const char* pszDefaultVal)
{
    while(*ppszAttr)
    {
        if (strcmp(ppszAttr[0], pszKey) == 0)
            return ppszAttr[1];
        ppszAttr += 2;
    }
    return pszDefaultVal;
}

/************************************************************************/
/*                            GetOGRFieldType()                         */
/************************************************************************/

OGRFieldType OGRXLSXDataSource::GetOGRFieldType(const char* pszValue,
                                                const char* pszValueType)
{
    if (!bAutodetectTypes || pszValueType == NULL)
        return OFTString;
    else if (strcmp(pszValueType, "string") == 0)
        return OFTString;
    else if (strcmp(pszValueType, "float") == 0)
    {
        CPLValueType eValueType = CPLGetValueType(pszValue);
        if (eValueType == CPL_VALUE_STRING)
            return OFTString;
        else if (eValueType == CPL_VALUE_INTEGER)
            return OFTInteger;
        else
            return OFTReal;
    }
    else if (strcmp(pszValueType, "datetime") == 0)
    {
        return OFTDateTime;
    }
    else if (strcmp(pszValueType, "date") == 0)
    {
        return OFTDate;
    }
    else if (strcmp(pszValueType, "time") == 0)
    {
        return OFTTime;
    }
    else
        return OFTString;
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

static void SetField(OGRFeature* poFeature,
                     int i,
                     const char* pszValue,
                     const char* pszCellType)
{
    if (pszValue[0] == '\0')
        return;

    OGRFieldType eType = poFeature->GetFieldDefnRef(i)->GetType();

    if (strcmp(pszCellType, "time") == 0 ||
        strcmp(pszCellType, "date") == 0 ||
        strcmp(pszCellType, "datetime") == 0)
    {
        struct tm sTm;
        double dfNumberOfDaysSince1900 = atof(pszValue);
#define NUMBER_OF_DAYS_BETWEEN_1900_AND_1970        25569
#define NUMBER_OF_SECONDS_PER_DAY                   86400
        GIntBig nUnixTime = (GIntBig)((dfNumberOfDaysSince1900 -
                                       NUMBER_OF_DAYS_BETWEEN_1900_AND_1970 )*
                                                NUMBER_OF_SECONDS_PER_DAY);
        CPLUnixTimeToYMDHMS(nUnixTime, &sTm);

        if (eType == OFTTime || eType == OFTDate || eType == OFTDateTime)
        {
            poFeature->SetField(i, sTm.tm_year + 1900, sTm.tm_mon + 1, sTm.tm_mday,
                                sTm.tm_hour, sTm.tm_min, sTm.tm_sec, 0);
        }
        else if (strcmp(pszCellType, "time") == 0)
        {
            poFeature->SetField(i, CPLSPrintf("%02d:%02d:%02d",
                                sTm.tm_hour, sTm.tm_min, sTm.tm_sec));
        }
        else if (strcmp(pszCellType, "date") == 0)
        {
            poFeature->SetField(i, CPLSPrintf("%04d/%02d/%02d",
                                sTm.tm_year + 1900, sTm.tm_mon + 1, sTm.tm_mday));
        }
        else /* if (strcmp(pszCellType, "datetime") == 0) */
        {
            poFeature->SetField(i, CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d",
                                sTm.tm_year + 1900, sTm.tm_mon + 1, sTm.tm_mday,
                                sTm.tm_hour, sTm.tm_min, sTm.tm_sec));
        }
    }
    else
        poFeature->SetField(i, pszValue);
}

/************************************************************************/
/*                          DetectHeaderLine()                          */
/************************************************************************/

void OGRXLSXDataSource::DetectHeaderLine()

{
    int bHeaderLineCandidate = TRUE;
    size_t i;
    for(i = 0; i < apoFirstLineTypes.size(); i++)
    {
        if (apoFirstLineTypes[i] != "string")
        {
            /* If the values in the first line are not text, then it is */
            /* not a header line */
            bHeaderLineCandidate = FALSE;
            break;
        }
    }

    size_t nCountTextOnCurLine = 0;
    size_t nCountNonEmptyOnCurLine = 0;
    for(i = 0; bHeaderLineCandidate && i < apoCurLineTypes.size(); i++)
    {
        if (apoCurLineTypes[i] == "string")
        {
            /* If there are only text values on the second line, then we cannot */
            /* know if it is a header line or just a regular line */
            nCountTextOnCurLine ++;
        }
        else if (apoCurLineTypes[i] != "")
        {
            nCountNonEmptyOnCurLine ++;
        }
    }

    const char* pszXLSXHeaders = CPLGetConfigOption("OGR_XLSX_HEADERS", "");
    bFirstLineIsHeaders = FALSE;
    if (EQUAL(pszXLSXHeaders, "FORCE"))
        bFirstLineIsHeaders = TRUE;
    else if (EQUAL(pszXLSXHeaders, "DISABLE"))
        bFirstLineIsHeaders = FALSE;
    else if (bHeaderLineCandidate &&
             apoFirstLineTypes.size() != 0 &&
             apoFirstLineTypes.size() == apoCurLineTypes.size() &&
             nCountTextOnCurLine != apoFirstLineTypes.size() &&
             nCountNonEmptyOnCurLine != 0)
    {
        bFirstLineIsHeaders = TRUE;
    }
    CPLDebug("XLSX", "%s %s",
             poCurLayer->GetName(),
             bFirstLineIsHeaders ? "has header line" : "has no header line");
}

/************************************************************************/
/*                          startElementDefault()                       */
/************************************************************************/

void OGRXLSXDataSource::startElementDefault(const char *pszName,
                                           const char **ppszAttr)
{
    if (strcmp(pszName, "sheetData") == 0)
    {
        apoFirstLineValues.resize(0);
        apoFirstLineTypes.resize(0);
        nCurLine = 0;
        PushState(STATE_SHEETDATA);
    }
}

/************************************************************************/
/*                          startElementTable()                        */
/************************************************************************/

void OGRXLSXDataSource::startElementTable(const char *pszName,
                                         const char **ppszAttr)
{
    if (strcmp(pszName, "row") == 0)
    {
        PushState(STATE_ROW);

        int nNewCurLine = atoi(
            GetAttributeValue(ppszAttr, "r", "0")) - 1;
        for(;nCurLine<nNewCurLine;)
        {
            nCurCol = 0;
            apoCurLineValues.resize(0);
            apoCurLineTypes.resize(0);
            endElementRow("row");
        }
        nCurCol = 0;
        apoCurLineValues.resize(0);
        apoCurLineTypes.resize(0);
    }
}

/************************************************************************/
/*                           endElementTable()                          */
/************************************************************************/

void OGRXLSXDataSource::endElementTable(const char *pszName)
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszName, "sheetData") == 0);

        if (nCurLine == 0 ||
            (nCurLine == 1 && apoFirstLineValues.size() == 0))
        {
            /* We could remove empty sheet, but too late now */
        }
        else if (nCurLine == 1)
        {
            /* If we have only one single line in the sheet */
            size_t i;
            for(i = 0; i < apoFirstLineValues.size(); i++)
            {
                const char* pszFieldName = CPLSPrintf("Field%d", (int)i + 1);
                OGRFieldType eType = GetOGRFieldType(apoFirstLineValues[i].c_str(),
                                                     apoFirstLineTypes[i].c_str());
                OGRFieldDefn oFieldDefn(pszFieldName, eType);
                poCurLayer->CreateField(&oFieldDefn);
            }

            OGRFeature* poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
            for(i = 0; i < apoFirstLineValues.size(); i++)
            {
                SetField(poFeature, i, apoFirstLineValues[i].c_str(),
                         apoFirstLineTypes[i].c_str());
            }
            poCurLayer->CreateFeature(poFeature);
            delete poFeature;
        }

        if (poCurLayer)
        {
            ((OGRMemLayer*)poCurLayer)->SetUpdatable(FALSE);
            ((OGRMemLayer*)poCurLayer)->SetAdvertizeUTF8(TRUE);
        }

        poCurLayer = NULL;
    }
}

/************************************************************************/
/*                            startElementRow()                         */
/************************************************************************/

void OGRXLSXDataSource::startElementRow(const char *pszName,
                                       const char **ppszAttr)
{
    if (strcmp(pszName, "c") == 0)
    {
        PushState(STATE_CELL);

        const char* pszR = GetAttributeValue(ppszAttr, "r", NULL);
        if (pszR)
        {
            /* Convert col number from base 26 */
            int nNewCurCol = (pszR[0] - 'A');
            int i = 1;
            while(pszR[i] >= 'A' && pszR[i] <= 'Z')
            {
                nNewCurCol = nNewCurCol * 26 + (pszR[i] - 'A');
                i ++;
            }
            for(;nCurCol<nNewCurCol;nCurCol++)
            {
                apoCurLineValues.push_back("");
                apoCurLineTypes.push_back("");
            }
        }

        osValueType = "float";

        const char* pszS = GetAttributeValue(ppszAttr, "s", "-1");
        int nS = atoi(pszS);
        if (nS >= 0 && nS < (int)apoStyles.size())
        {
            OGRFieldType eType = apoStyles[nS];
            if (eType == OFTDateTime)
                osValueType = "datetime";
            else if (eType == OFTDate)
                osValueType = "date";
            else if (eType == OFTTime)
                osValueType = "time";
        }
        else if (nS != -1)
            CPLDebug("XLSX", "Cannot find style %d", nS);

        const char* pszT = GetAttributeValue(ppszAttr, "t", NULL);
        if (pszT && strcmp(pszT, "s") == 0)
            osValueType = "string";

        osValue = "";
    }
}

/************************************************************************/
/*                            endElementRow()                           */
/************************************************************************/

void OGRXLSXDataSource::endElementRow(const char *pszName)
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszName, "row") == 0);

        OGRFeature* poFeature;
        size_t i;

        /* Backup first line values and types in special arrays */
        if (nCurLine == 0)
        {
            apoFirstLineTypes = apoCurLineTypes;
            apoFirstLineValues = apoCurLineValues;

    #if skip_leading_empty_rows
            if (apoFirstLineTypes.size() == 0)
            {
                /* Skip leading empty rows */
                apoFirstLineTypes.resize(0);
                apoFirstLineValues.resize(0);
                return;
            }
    #endif
        }

        if (nCurLine == 1)
        {
            DetectHeaderLine();

            if (bFirstLineIsHeaders)
            {
                for(i = 0; i < apoFirstLineValues.size(); i++)
                {
                    const char* pszFieldName = apoFirstLineValues[i].c_str();
                    if (pszFieldName[0] == '\0')
                        pszFieldName = CPLSPrintf("Field%d", (int)i + 1);
                    OGRFieldType eType = OFTString;
                    if (i < apoCurLineValues.size())
                    {
                        eType = GetOGRFieldType(apoCurLineValues[i].c_str(),
                                                apoCurLineTypes[i].c_str());
                    }
                    OGRFieldDefn oFieldDefn(pszFieldName, eType);
                    poCurLayer->CreateField(&oFieldDefn);
                }
            }
            else
            {
                for(i = 0; i < apoFirstLineValues.size(); i++)
                {
                    const char* pszFieldName =
                        CPLSPrintf("Field%d", (int)i + 1);
                    OGRFieldType eType = GetOGRFieldType(
                                            apoFirstLineValues[i].c_str(),
                                            apoFirstLineTypes[i].c_str());
                    OGRFieldDefn oFieldDefn(pszFieldName, eType);
                    poCurLayer->CreateField(&oFieldDefn);
                }

                poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
                for(i = 0; i < apoFirstLineValues.size(); i++)
                {
                    SetField(poFeature, i, apoFirstLineValues[i].c_str(),
                             apoFirstLineTypes[i].c_str());
                }
                poCurLayer->CreateFeature(poFeature);
                delete poFeature;
            }
        }

        if (nCurLine >= 1)
        {
            /* Add new fields found on following lines. */
            if (apoCurLineValues.size() >
                (size_t)poCurLayer->GetLayerDefn()->GetFieldCount())
            {
                for(i = (size_t)poCurLayer->GetLayerDefn()->GetFieldCount();
                    i < apoCurLineValues.size();
                    i++)
                {
                    const char* pszFieldName =
                        CPLSPrintf("Field%d", (int)i + 1);
                    OGRFieldType eType = GetOGRFieldType(
                                                apoCurLineValues[i].c_str(),
                                                apoCurLineTypes[i].c_str());
                    OGRFieldDefn oFieldDefn(pszFieldName, eType);
                    poCurLayer->CreateField(&oFieldDefn);
                }
            }

            /* Update field type if necessary */
            if (bAutodetectTypes)
            {
                for(i = 0; i < apoCurLineValues.size(); i++)
                {
                    if (apoCurLineValues[i].size())
                    {
                        OGRFieldType eValType = GetOGRFieldType(
                                                apoCurLineValues[i].c_str(),
                                                apoCurLineTypes[i].c_str());
                        OGRFieldType eFieldType =
                            poCurLayer->GetLayerDefn()->GetFieldDefn(i)->GetType();
                        if (eFieldType == OFTDateTime &&
                            (eValType == OFTDate || eValType == OFTTime) )
                        {
                            /* ok */
                        }
                        else if (eFieldType == OFTReal && eValType == OFTInteger)
                        {
                           /* ok */;
                        }
                        else if (eFieldType != OFTString && eValType != eFieldType)
                        {
                            OGRFieldDefn oNewFieldDefn(
                                poCurLayer->GetLayerDefn()->GetFieldDefn(i));
                            if ((eFieldType == OFTDate || eFieldType == OFTTime) &&
                                   eValType == OFTDateTime)
                                oNewFieldDefn.SetType(OFTDateTime);
                            else if (eFieldType == OFTInteger &&
                                     eValType == OFTReal)
                                oNewFieldDefn.SetType(OFTReal);
                            else
                                oNewFieldDefn.SetType(OFTString);
                            poCurLayer->AlterFieldDefn(i, &oNewFieldDefn,
                                                       ALTER_TYPE_FLAG);
                        }
                    }
                }
            }

            /* Add feature for current line */
            poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
            for(i = 0; i < apoCurLineValues.size(); i++)
            {
                SetField(poFeature, i, apoCurLineValues[i].c_str(),
                         apoCurLineTypes[i].c_str());
            }
            poCurLayer->CreateFeature(poFeature);
            delete poFeature;
       }

        nCurLine++;
    }
}

/************************************************************************/
/*                           startElementCell()                         */
/************************************************************************/

void OGRXLSXDataSource::startElementCell(const char *pszName,
                                        const char **ppszAttr)
{
    if (osValue.size() == 0 && strcmp(pszName, "v") == 0)
    {
        PushState(STATE_TEXTV);
    }
}

/************************************************************************/
/*                            endElementCell()                          */
/************************************************************************/

void OGRXLSXDataSource::endElementCell(const char *pszName)
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszName, "c") == 0);

        if (osValueType == "string")
        {
            int nIndex = atoi(osValue);
            if (nIndex >= 0 && nIndex < (int)(apoSharedStrings.size()))
                osValue = apoSharedStrings[nIndex];
            else
                CPLDebug("XLSX", "Cannot find string %d", nIndex);
        }

        apoCurLineValues.push_back(osValue);
        apoCurLineTypes.push_back(osValueType);

        nCurCol += 1;
    }
}

/************************************************************************/
/*                           dataHandlerTextV()                         */
/************************************************************************/

void OGRXLSXDataSource::dataHandlerTextV(const char *data, int nLen)
{
    osValue.append(data, nLen);
}

/************************************************************************/
/*                              BuildLayer()                            */
/************************************************************************/

void OGRXLSXDataSource::BuildLayer(OGRLayer* poLayer, int nSheetId)
{
    poCurLayer = poLayer;

    CPLString osSheetFilename(
        CPLSPrintf("/vsizip/%s/xl/worksheets/sheet%d.xml", pszName, nSheetId));
    const char* pszSheetFilename = osSheetFilename.c_str();
    VSILFILE* fp = VSIFOpenL(pszSheetFilename, "rb");
    if (fp == NULL)
        return;

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
    XML_SetUserData(oParser, this);

    VSIFSeekL( fp, 0, SEEK_SET );

    bStopParsing = FALSE;
    nWithoutEventCounter = 0;
    nDataHandlerCounter = 0;
    nStackDepth = 0;
    nDepth = 0;
    stateStack[0].eVal = STATE_DEFAULT;
    stateStack[0].nBeginDepth = 0;

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fp );
        nDone = VSIFEofL(fp);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of %s file failed : %s at line %d, column %d",
                     pszSheetFilename,
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 10);

    XML_ParserFree(oParser);
    oParser = NULL;

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    VSIFCloseL(fp);
}

/************************************************************************/
/*                          startElementSSCbk()                         */
/************************************************************************/

static void XMLCALL startElementSSCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRXLSXDataSource*)pUserData)->startElementSSCbk(pszName, ppszAttr);
}

void OGRXLSXDataSource::startElementSSCbk(const char *pszName,
                                       const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT:
        {
            if (strcmp(pszName,"t") == 0)
            {
                PushState(STATE_T);
                osCurrentString = "";
            }
            break;
        }
        default:
            break;
    }
    nDepth++;
}

/************************************************************************/
/*                           endElementSSCbk()                          */
/************************************************************************/

static void XMLCALL endElementSSCbk(void *pUserData, const char *pszName)
{
    ((OGRXLSXDataSource*)pUserData)->endElementSSCbk(pszName);
}

void OGRXLSXDataSource::endElementSSCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    nDepth--;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: break;
        case STATE_T:
        {
            if (stateStack[nStackDepth].nBeginDepth == nDepth)
            {
                apoSharedStrings.push_back(osCurrentString);
            }
            break;
        }
        default:            break;
    }

    if (stateStack[nStackDepth].nBeginDepth == nDepth)
        nStackDepth --;
}

/************************************************************************/
/*                           dataHandlerSSCbk()                         */
/************************************************************************/

static void XMLCALL dataHandlerSSCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRXLSXDataSource*)pUserData)->dataHandlerSSCbk(data, nLen);
}

void OGRXLSXDataSource::dataHandlerSSCbk(const char *data, int nLen)
{
    if (bStopParsing) return;

    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = TRUE;
        return;
    }

    nWithoutEventCounter = 0;

    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: break;
        case STATE_T:       osCurrentString.append(data, nLen); break;
        default:            break;
    }
}

/************************************************************************/
/*                          AnalyseSharedStrings()                      */
/************************************************************************/

void OGRXLSXDataSource::AnalyseSharedStrings(VSILFILE* fpSharedStrings)
{
    if (fpSharedStrings == NULL)
        return;

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementSSCbk, ::endElementSSCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerSSCbk);
    XML_SetUserData(oParser, this);

    VSIFSeekL( fpSharedStrings, 0, SEEK_SET );

    bStopParsing = FALSE;
    nWithoutEventCounter = 0;
    nDataHandlerCounter = 0;
    nStackDepth = 0;
    nDepth = 0;
    stateStack[0].eVal = STATE_DEFAULT;
    stateStack[0].nBeginDepth = 0;

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpSharedStrings );
        nDone = VSIFEofL(fpSharedStrings);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of %s file failed : %s at line %d, column %d",
                     "sharedStrings.xml",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 10);

    XML_ParserFree(oParser);
    oParser = NULL;

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    VSIFCloseL(fpSharedStrings);
    fpSharedStrings = NULL;
}


/************************************************************************/
/*                          startElementWBCbk()                         */
/************************************************************************/

static void XMLCALL startElementWBCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRXLSXDataSource*)pUserData)->startElementWBCbk(pszName, ppszAttr);
}

void OGRXLSXDataSource::startElementWBCbk(const char *pszName,
                                       const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    if (strcmp(pszName,"sheet") == 0)
    {
        const char* pszSheetName = GetAttributeValue(ppszAttr, "name", NULL);
        const char* pszSheetId = GetAttributeValue(ppszAttr, "sheetId", NULL);
        if (pszSheetName && pszSheetId)
        {
            int nSheetId = atoi(pszSheetId);
            papoLayers = (OGRLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers++] = new OGRXLSXLayer(this, nSheetId, pszSheetName);
        }
    }
}

/************************************************************************/
/*                             AnalyseWorkbook()                        */
/************************************************************************/

void OGRXLSXDataSource::AnalyseWorkbook(VSILFILE* fpWorkbook)
{
    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementWBCbk, NULL);
    XML_SetUserData(oParser, this);

    VSIFSeekL( fpWorkbook, 0, SEEK_SET );

    bStopParsing = FALSE;
    nWithoutEventCounter = 0;
    nDataHandlerCounter = 0;

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpWorkbook );
        nDone = VSIFEofL(fpWorkbook);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of %s file failed : %s at line %d, column %d",
                     "workbook.xml",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 10);

    XML_ParserFree(oParser);
    oParser = NULL;

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    VSIFCloseL(fpWorkbook);
}


/************************************************************************/
/*                       startElementStylesCbk()                        */
/************************************************************************/

static void XMLCALL startElementStylesCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRXLSXDataSource*)pUserData)->startElementStylesCbk(pszName, ppszAttr);
}

void OGRXLSXDataSource::startElementStylesCbk(const char *pszName,
                                       const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    if (strcmp(pszName,"numFmt") == 0)
    {
        const char* pszFormatCode = GetAttributeValue(ppszAttr, "formatCode", NULL);
        const char* pszNumFmtId = GetAttributeValue(ppszAttr, "numFmtId", "-1");
        int nNumFmtId = atoi(pszNumFmtId);
        if (pszFormatCode && nNumFmtId >= 164)
        {
            int bHasDate = strstr(pszFormatCode, "DD") != NULL ||
                           strstr(pszFormatCode, "YY") != NULL;
            int bHasTime = strstr(pszFormatCode, "HH") != NULL;
            if (bHasDate && bHasTime)
                apoMapStyleFormats[nNumFmtId] = OFTDateTime;
            else if (bHasDate)
                apoMapStyleFormats[nNumFmtId] = OFTDate;
            else if (bHasTime)
                apoMapStyleFormats[nNumFmtId] = OFTTime;
            else
                apoMapStyleFormats[nNumFmtId] = OFTReal;
        }
    }
    else if (strcmp(pszName,"cellXfs") == 0)
    {
        bInCellXFS = TRUE;
    }
    else if (bInCellXFS && strcmp(pszName,"xf") == 0)
    {
        const char* pszNumFmtId = GetAttributeValue(ppszAttr, "numFmtId", "-1");
        int nNumFmtId = atoi(pszNumFmtId);
        OGRFieldType eType = OFTReal;
        if (nNumFmtId >= 0)
        {
            if (nNumFmtId < 164)
            {
                // From http://social.msdn.microsoft.com/Forums/en-US/oxmlsdk/thread/e27aaf16-b900-4654-8210-83c5774a179c/
                if (nNumFmtId >= 14 && nNumFmtId <= 17)
                    eType = OFTDate;
                else if (nNumFmtId >= 18 && nNumFmtId <= 21)
                    eType = OFTTime;
                else if (nNumFmtId == 22)
                    eType = OFTDateTime;
            }
            else
            {
                std::map<int, OGRFieldType>::iterator oIter = apoMapStyleFormats.find(nNumFmtId);
                if (oIter != apoMapStyleFormats.end())
                    eType = oIter->second;
                else
                    CPLDebug("XLSX", "Cannot find entry in <numFmts> with numFmtId=%d", nNumFmtId);
            }
        }
        //printf("style[%d] = %d\n", apoStyles.size(), eType);

        apoStyles.push_back(eType);
    }
}

/************************************************************************/
/*                       endElementStylesCbk()                          */
/************************************************************************/

static void XMLCALL endElementStylesCbk(void *pUserData, const char *pszName)
{
    ((OGRXLSXDataSource*)pUserData)->endElementStylesCbk(pszName);
}

void OGRXLSXDataSource::endElementStylesCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    if (strcmp(pszName,"cellXfs") == 0)
    {
        bInCellXFS = FALSE;
    }
}

/************************************************************************/
/*                             AnalyseStyles()                          */
/************************************************************************/

void OGRXLSXDataSource::AnalyseStyles(VSILFILE* fpStyles)
{
    if (fpStyles == NULL)
        return;

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementStylesCbk, ::endElementStylesCbk);
    XML_SetUserData(oParser, this);

    VSIFSeekL( fpStyles, 0, SEEK_SET );

    bStopParsing = FALSE;
    nWithoutEventCounter = 0;
    nDataHandlerCounter = 0;
    bInCellXFS = FALSE;

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpStyles );
        nDone = VSIFEofL(fpStyles);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of %s file failed : %s at line %d, column %d",
                     "styles.xml",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 10);

    XML_ParserFree(oParser);
    oParser = NULL;

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    VSIFCloseL(fpStyles);
}
