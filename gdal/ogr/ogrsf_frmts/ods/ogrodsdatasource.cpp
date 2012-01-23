/******************************************************************************
 * $Id$
 *
 * Project:  ODS Translator
 * Purpose:  Implements OGRODSDataSource class
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

#include "ogr_ods.h"
#include "ogr_mem.h"
#include "ogr_p.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRODSDataSource()                          */
/************************************************************************/

OGRODSDataSource::OGRODSDataSource()

{
    pszName = NULL;
    fpContent = NULL;

    bFirstLineIsHeaders = FALSE;

    oParser = NULL;
    bStopParsing = FALSE;
    nWithoutEventCounter = 0;
    nDataHandlerCounter = 0;
    nStackDepth = 0;
    nDepth = 0;
    nCurLine = 0;
    nCurCol = 0;
    nRowsRepeated = 0;
    nCellsRepeated = 0;
    stateStack[0].eVal = STATE_DEFAULT;
    stateStack[0].nBeginDepth = 0;

    poMemDS = NULL;
    poCurLayer = NULL;

    const char* pszODSFieldTypes =
                CPLGetConfigOption("OGR_ODS_FIELD_TYPES", "");
    bAutodetectTypes = !EQUAL(pszODSFieldTypes, "STRING");
}

/************************************************************************/
/*                         ~OGRODSDataSource()                          */
/************************************************************************/

OGRODSDataSource::~OGRODSDataSource()

{
    CPLFree( pszName );

    if (fpContent)
        VSIFCloseL(fpContent);

    delete poMemDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODSDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRODSDataSource::GetLayer( int iLayer )

{
    if (poMemDS == NULL)
        AnalyseFile();
    return poMemDS->GetLayer(iLayer);
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRODSDataSource::GetLayerCount()
{
    if (poMemDS == NULL)
        AnalyseFile();
    return poMemDS->GetLayerCount();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRODSDataSource::Open( const char * pszFilename,
                            VSILFILE* fpContentIn,
                            int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );
    fpContent = fpContentIn;

    return TRUE;
}

/************************************************************************/
/*                           startElementCbk()                          */
/************************************************************************/

static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRODSDataSource*)pUserData)->startElementCbk(pszName, ppszAttr);
}

void OGRODSDataSource::startElementCbk(const char *pszName,
                                       const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: startElementDefault(pszName, ppszAttr); break;
        case STATE_TABLE:   startElementTable(pszName, ppszAttr); break;
        case STATE_ROW:     startElementRow(pszName, ppszAttr); break;
        case STATE_CELL:    startElementCell(pszName, ppszAttr); break;
        case STATE_TEXTP:   break;
        default:            break;
    }
    nDepth++;
}

/************************************************************************/
/*                            endElementCbk()                           */
/************************************************************************/

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    ((OGRODSDataSource*)pUserData)->endElementCbk(pszName);
}

void OGRODSDataSource::endElementCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    nDepth--;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: break;
        case STATE_TABLE:   endElementTable(pszName); break;
        case STATE_ROW:     endElementRow(pszName); break;
        case STATE_CELL:    endElementCell(pszName); break;
        case STATE_TEXTP:   break;
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
    ((OGRODSDataSource*)pUserData)->dataHandlerCbk(data, nLen);
}

void OGRODSDataSource::dataHandlerCbk(const char *data, int nLen)
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
        case STATE_TABLE:   break;
        case STATE_ROW:     break;
        case STATE_CELL:    break;
        case STATE_TEXTP:   dataHandlerTextP(data, nLen);
        default:            break;
    }
}

/************************************************************************/
/*                                PushState()                           */
/************************************************************************/

void OGRODSDataSource::PushState(HandlerStateEnum eVal)
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

OGRFieldType OGRODSDataSource::GetOGRFieldType(const char* pszValue,
                                               const char* pszValueType)
{
    if (!bAutodetectTypes || pszValueType == NULL)
        return OFTString;
    else if (strcmp(pszValueType, "string") == 0)
        return OFTString;
    else if (strcmp(pszValueType, "float") == 0 ||
             strcmp(pszValueType, "currency") == 0)
    {
        if (CPLGetValueType(pszValue) == CPL_VALUE_INTEGER)
            return OFTInteger;
        else
            return OFTReal;
    }
    else if (strcmp(pszValueType, "percentage") == 0)
        return OFTReal;
    else if (strcmp(pszValueType, "date") == 0)
    {
        if (strlen(pszValue) == 4 + 1 + 2 + 1 + 2)
            return OFTDate;
        else
            return OFTDateTime;
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
                     const char* pszValue)
{
    if (pszValue[0] == '\0')
        return;

    OGRFieldType eType = poFeature->GetFieldDefnRef(i)->GetType();
    if (eType == OFTTime)
    {
        int nHour, nHourRepeated, nMinute, nSecond;
        char c;
        if (strncmp(pszValue, "PT", 2) == 0 &&
            sscanf(pszValue + 2, "%02d%c%02d%c%02d%c",
                   &nHour, &c, &nMinute, &c, &nSecond, &c) == 6)
        {
            poFeature->SetField(i, 0, 0, 0, nHour, nMinute, nSecond, 0);
        }
        /* bug with kspread 2.1.2 ? */
        /* ex PT121234M56S */
        else if (strncmp(pszValue, "PT", 2) == 0 &&
                 sscanf(pszValue + 2, "%02d%02d%02d%c%02d%c",
                        &nHour, &nHourRepeated, &nMinute, &c, &nSecond, &c) == 6 &&
                 nHour == nHourRepeated)
        {
            poFeature->SetField(i, 0, 0, 0, nHour, nMinute, nSecond, 0);
        }
    }
    else if (eType == OFTDate || eType == OFTDateTime)
    {
        int nYear, nMonth, nDay, nHour, nMinute, nTZ;
        float fCur;
        if (OGRParseXMLDateTime( pszValue,
                                 &nYear, &nMonth, &nDay,
                                 &nHour, &nMinute, &fCur, &nTZ) )
        {
            poFeature->SetField(i, nYear, nMonth, nDay,
                                nHour, nMinute, fCur, nTZ);
        }
    }
    else
        poFeature->SetField(i, pszValue);
}

/************************************************************************/
/*                          DetectHeaderLine()                          */
/************************************************************************/

void OGRODSDataSource::DetectHeaderLine()

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

    const char* pszODSHeaders = CPLGetConfigOption("OGR_ODS_HEADERS", "");
    bFirstLineIsHeaders = FALSE;
    if (EQUAL(pszODSHeaders, "FORCE"))
        bFirstLineIsHeaders = TRUE;
    else if (EQUAL(pszODSHeaders, "DISABLE"))
        bFirstLineIsHeaders = FALSE;
    else if (bHeaderLineCandidate &&
             apoFirstLineTypes.size() != 0 &&
             apoFirstLineTypes.size() == apoCurLineTypes.size() &&
             nCountTextOnCurLine != apoFirstLineTypes.size() &&
             nCountNonEmptyOnCurLine != 0)
    {
        bFirstLineIsHeaders = TRUE;
    }
    CPLDebug("ODS", "%s %s",
             poCurLayer->GetName(),
             bFirstLineIsHeaders ? "has header line" : "has no header line");
}

/************************************************************************/
/*                          startElementDefault()                       */
/************************************************************************/

void OGRODSDataSource::startElementDefault(const char *pszName,
                                           const char **ppszAttr)
{
    if (strcmp(pszName, "table:table") == 0)
    {
        const char* pszTableName =
            GetAttributeValue(ppszAttr, "table:name", "unnamed");
        poCurLayer = poMemDS->CreateLayer(pszTableName, NULL, wkbNone);
        nCurLine = 0;
        apoFirstLineValues.resize(0);
        apoFirstLineTypes.resize(0);
        PushState(STATE_TABLE);
    }
}

/************************************************************************/
/*                          startElementTable()                        */
/************************************************************************/

void OGRODSDataSource::startElementTable(const char *pszName,
                                         const char **ppszAttr)
{
    if (strcmp(pszName, "table:table-row") == 0)
    {
        nRowsRepeated = atoi(
            GetAttributeValue(ppszAttr, "table:number-rows-repeated", "1"));
        nCurCol = 0;

        apoCurLineValues.resize(0);
        apoCurLineTypes.resize(0);

        PushState(STATE_ROW);
    }
}

/************************************************************************/
/*                           endElementTable()                          */
/************************************************************************/

void OGRODSDataSource::endElementTable(const char *pszName)
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszName, "table:table") == 0);

        if (nCurLine == 0 ||
            (nCurLine == 1 && apoFirstLineValues.size() == 0))
        {
            /* Remove empty sheet */
            poMemDS->DeleteLayer(poMemDS->GetLayerCount() - 1);
            poCurLayer = NULL;
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
                SetField(poFeature, i, apoFirstLineValues[i].c_str());
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

void OGRODSDataSource::startElementRow(const char *pszName,
                                       const char **ppszAttr)
{
    if (strcmp(pszName, "table:table-cell") == 0)
    {
        PushState(STATE_CELL);

        osValueType = GetAttributeValue(ppszAttr, "office:value-type", "");
        const char* pszValue =
            GetAttributeValue(ppszAttr, "office:value", NULL);
        if (pszValue)
            osValue = pszValue;
        else
        {
            const char* pszDateValue =
                GetAttributeValue(ppszAttr, "office:date-value", NULL);
            if (pszDateValue)
                osValue = pszDateValue;
            else
                osValue = GetAttributeValue(ppszAttr, "office:time-value", "");
        }

        nCellsRepeated = atoi(
            GetAttributeValue(ppszAttr, "table:number-columns-repeated", "1"));
    }
    else if (strcmp(pszName, "table:covered-table-cell") == 0)
    {
        /* Merged cell */
        apoCurLineValues.push_back("");
        apoCurLineTypes.push_back("");

        nCurCol += 1;
    }
}

/************************************************************************/
/*                            endElementRow()                           */
/************************************************************************/

void OGRODSDataSource::endElementRow(const char *pszName)
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszName, "table:table-row") == 0);

        OGRFeature* poFeature;
        size_t i;

        /* Remove blank columns at the right to defer type evaluation */
        /* until necessary */
        i = apoCurLineTypes.size();
        while(i > 0)
        {
            i --;
            if (apoCurLineTypes[i] == "")
            {
                apoCurLineValues.resize(i);
                apoCurLineTypes.resize(i);
            }
            else
                break;
        }

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
                    SetField(poFeature, i, apoFirstLineValues[i].c_str());
                }
                poCurLayer->CreateFeature(poFeature);
                delete poFeature;
            }
        }

        if (nCurLine >= 1 || (nCurLine == 0 && nRowsRepeated > 1))
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
            for(int j=0;j<nRowsRepeated;j++)
            {
                poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
                for(i = 0; i < apoCurLineValues.size(); i++)
                {
                    SetField(poFeature, i, apoCurLineValues[i].c_str());
                }
                poCurLayer->CreateFeature(poFeature);
                delete poFeature;
            }
        }

        nCurLine += nRowsRepeated;
    }
}

/************************************************************************/
/*                           startElementCell()                         */
/************************************************************************/

void OGRODSDataSource::startElementCell(const char *pszName,
                                        const char **ppszAttr)
{
    if (osValue.size() == 0 && strcmp(pszName, "text:p") == 0)
    {
        PushState(STATE_TEXTP);
    }
}

/************************************************************************/
/*                            endElementCell()                          */
/************************************************************************/

void OGRODSDataSource::endElementCell(const char *pszName)
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszName, "table:table-cell") == 0);

        for(int i = 0; i < nCellsRepeated; i++)
        {
            apoCurLineValues.push_back(osValue);
            apoCurLineTypes.push_back(osValueType);
        }

        nCurCol += nCellsRepeated;
    }
}

/************************************************************************/
/*                           dataHandlerTextP()                         */
/************************************************************************/

void OGRODSDataSource::dataHandlerTextP(const char *data, int nLen)
{
    osValue.append(data, nLen);
}

/************************************************************************/
/*                             AnalyseFile()                            */
/************************************************************************/

void OGRODSDataSource::AnalyseFile()
{
    poMemDS = new OGRMemDataSource("", NULL);

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
    XML_SetUserData(oParser, this);

    VSIFSeekL( fpContent, 0, SEEK_SET );

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpContent );
        nDone = VSIFEofL(fpContent);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of ODS file failed : %s at line %d, column %d",
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
}
