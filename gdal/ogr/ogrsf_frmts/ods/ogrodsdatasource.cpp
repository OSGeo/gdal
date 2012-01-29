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
/*                            OGRODSLayer()                            */
/************************************************************************/

OGRODSLayer::OGRODSLayer( OGRODSDataSource* poDSIn,
                            const char * pszName,
                            int bUpdatedIn) :
                                OGRMemLayer(pszName, NULL, wkbNone)
{
    poDS = poDSIn;
    bUpdated = bUpdatedIn;
}

/************************************************************************/
/*                             Updated()                                */
/************************************************************************/

void OGRODSLayer::SetUpdated(int bUpdatedIn)
{
    if (bUpdatedIn && !bUpdated && poDS->GetUpdatable())
    {
        bUpdated = TRUE;
        poDS->SetUpdated();
    }
    else if (bUpdated && !bUpdatedIn)
    {
        bUpdated = FALSE;
    }
}

/************************************************************************/
/*                           SyncToDisk()                               */
/************************************************************************/

OGRErr OGRODSLayer::SyncToDisk()
{
    return poDS->SyncToDisk();
}

/************************************************************************/
/*                          OGRODSDataSource()                          */
/************************************************************************/

OGRODSDataSource::OGRODSDataSource()

{
    pszName = NULL;
    fpContent = NULL;
    fpSettings = NULL;
    bUpdatable = FALSE;
    bUpdated = FALSE;
    bAnalysedFile = FALSE;

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
    nEmptyRowsAccumulated = 0;
    nCurCol = 0;
    nRowsRepeated = 0;
    nCellsRepeated = 0;
    stateStack[0].eVal = STATE_DEFAULT;
    stateStack[0].nBeginDepth = 0;
    bEndTableParsing = FALSE;

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
    SyncToDisk();

    CPLFree( pszName );

    if (fpContent)
        VSIFCloseL(fpContent);
    if (fpSettings)
        VSIFCloseL(fpSettings);

    for(int i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODSDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bUpdatable;
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return bUpdatable;
    else
        return FALSE;
}


/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRODSDataSource::GetLayer( int iLayer )

{
    AnalyseFile();
    if (iLayer < 0 || iLayer >= nLayers)
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRODSDataSource::GetLayerCount()
{
    AnalyseFile();
    return nLayers;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRODSDataSource::Open( const char * pszFilename,
                            VSILFILE* fpContentIn,
                            VSILFILE* fpSettingsIn,
                            int bUpdatableIn)

{
    bUpdatable = bUpdatableIn;

    pszName = CPLStrdup( pszFilename );
    fpContent = fpContentIn;
    fpSettings = fpSettingsIn;

    return TRUE;
}

/************************************************************************/
/*                             Create()                                 */
/************************************************************************/

int OGRODSDataSource::Create( const char * pszFilename, char **papszOptions )
{
    bUpdated = TRUE;
    bUpdatable = TRUE;
    bAnalysedFile = TRUE;

    pszName = CPLStrdup( pszFilename );

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
                                nHour, nMinute, (int)fCur, nTZ);
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
    else if (osSetLayerHasSplitter.find(poCurLayer->GetName()) !=
             osSetLayerHasSplitter.end())
    {
        bFirstLineIsHeaders = TRUE;
    }
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

        poCurLayer = new OGRODSLayer(this, pszTableName);
        papoLayers = (OGRLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
        papoLayers[nLayers++] = poCurLayer;

        nCurLine = 0;
        nEmptyRowsAccumulated = 0;
        apoFirstLineValues.resize(0);
        apoFirstLineTypes.resize(0);
        PushState(STATE_TABLE);
        bEndTableParsing = FALSE;
    }
}

/************************************************************************/
/*                          startElementTable()                        */
/************************************************************************/

void OGRODSDataSource::startElementTable(const char *pszName,
                                         const char **ppszAttr)
{
    if (strcmp(pszName, "table:table-row") == 0 && !bEndTableParsing)
    {
        nRowsRepeated = atoi(
            GetAttributeValue(ppszAttr, "table:number-rows-repeated", "1"));
        if (nRowsRepeated > 65536)
        {
            bEndTableParsing = TRUE;
            return;
        }

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
            delete poCurLayer;
            nLayers --;
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
            ((OGRMemLayer*)poCurLayer)->SetUpdatable(bUpdatable);
            ((OGRMemLayer*)poCurLayer)->SetAdvertizeUTF8(TRUE);
            ((OGRODSLayer*)poCurLayer)->SetUpdated(FALSE);
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

        const char* pszFormula = GetAttributeValue(ppszAttr, "table:formula", NULL);
        if (pszFormula && strncmp(pszFormula, "of:=", 4) == 0)
        {
            osFormula = pszFormula;
            if (osValueType.size() == 0)
                osValueType = "formula";
        }
        else
            osFormula = "";

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

        /* Do not add immediately empty rows. Wait until there is another non */
        /* empty row */
        if (nCurLine >= 2 && apoCurLineTypes.size() == 0)
        {
            nEmptyRowsAccumulated += nRowsRepeated;
            return;
        }
        else if (nEmptyRowsAccumulated > 0)
        {
            for(i = 0; i < (size_t)nEmptyRowsAccumulated; i++)
            {
                poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
                poCurLayer->CreateFeature(poFeature);
                delete poFeature;
            }
            nCurLine += nEmptyRowsAccumulated;
            nEmptyRowsAccumulated = 0;
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
            apoCurLineValues.push_back(osValue.size() ? osValue : osFormula);
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
    if (bAnalysedFile)
        return;

    bAnalysedFile = TRUE;

    AnalyseSettings();

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
    XML_SetUserData(oParser, this);

    nDepth = 0;
    nStackDepth = 0;
    stateStack[0].nBeginDepth = 0;
    bStopParsing = FALSE;
    nWithoutEventCounter = 0;

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

    VSIFCloseL(fpContent);
    fpContent = NULL;

    bUpdated = FALSE;
}

/************************************************************************/
/*                        startElementStylesCbk()                       */
/************************************************************************/

static void XMLCALL startElementStylesCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRODSDataSource*)pUserData)->startElementStylesCbk(pszName, ppszAttr);
}

void OGRODSDataSource::startElementStylesCbk(const char *pszName,
                                             const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    if (nStackDepth == 0 &&
        strcmp(pszName, "config:config-item-map-named") == 0 &&
        strcmp(GetAttributeValue(ppszAttr, "config:name", ""), "Tables") == 0)
    {
        stateStack[++nStackDepth].nBeginDepth = nDepth;
    }
    else if (nStackDepth == 1 && strcmp(pszName, "config:config-item-map-entry") == 0)
    {
        const char* pszTableName = GetAttributeValue(ppszAttr, "config:name", NULL);
        if (pszTableName)
        {
            osCurrentConfigTableName = pszTableName;
            nFlags = 0;
            stateStack[++nStackDepth].nBeginDepth = nDepth;
        }
    }
    else if (nStackDepth == 2 && strcmp(pszName, "config:config-item") == 0)
    {
        const char* pszConfigName = GetAttributeValue(ppszAttr, "config:name", NULL);
        if (pszConfigName)
        {
            osConfigName = pszConfigName;
            osValue = "";
            stateStack[++nStackDepth].nBeginDepth = nDepth;
        }
    }

    nDepth++;
}

/************************************************************************/
/*                        endElementStylesCbk()                         */
/************************************************************************/

static void XMLCALL endElementStylesCbk(void *pUserData, const char *pszName)
{
    ((OGRODSDataSource*)pUserData)->endElementStylesCbk(pszName);
}

void OGRODSDataSource::endElementStylesCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    nDepth--;

    if (nStackDepth > 0 && stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        if (nStackDepth == 2)
        {
            if (nFlags == (1 | 2))
                osSetLayerHasSplitter.insert(osCurrentConfigTableName);
        }
        if (nStackDepth == 3)
        {
            if (osConfigName == "VerticalSplitMode" && osValue == "2")
                nFlags |= 1;
            else if (osConfigName == "VerticalSplitPosition" && osValue == "1")
                nFlags |= 2;
        }
        nStackDepth --;
    }
}

/************************************************************************/
/*                         dataHandlerStylesCbk()                       */
/************************************************************************/

static void XMLCALL dataHandlerStylesCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRODSDataSource*)pUserData)->dataHandlerStylesCbk(data, nLen);
}

void OGRODSDataSource::dataHandlerStylesCbk(const char *data, int nLen)
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

    if (nStackDepth == 3)
    {
        osValue.append(data, nLen);
    }
}

/************************************************************************/
/*                           AnalyseSettings()                          */
/*                                                                      */
/* We parse settings.xml to see which layers have a vertical splitter   */
/* on the first line, so as to use it as the header line.               */
/************************************************************************/

void OGRODSDataSource::AnalyseSettings()
{
    if (fpSettings == NULL)
        return;

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oParser, ::startElementStylesCbk, ::endElementStylesCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerStylesCbk);
    XML_SetUserData(oParser, this);

    nDepth = 0;
    nStackDepth = 0;
    bStopParsing = FALSE;
    nWithoutEventCounter = 0;

    VSIFSeekL( fpSettings, 0, SEEK_SET );

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpSettings );
        nDone = VSIFEofL(fpSettings);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of styles.xml file failed : %s at line %d, column %d",
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

    VSIFCloseL(fpSettings);
    fpSettings = NULL;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRODSDataSource::CreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdatable )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

    AnalyseFile();

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( pszLayerName );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRLayer* poLayer = new OGRODSLayer(this, pszLayerName, TRUE);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    bUpdated = TRUE;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRODSDataSource::DeleteLayer( const char *pszLayerName )

{
    int iLayer;

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdatable )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "Layer %s cannot be deleted.\n",
                  pszName, pszLayerName );

        return;
    }

/* -------------------------------------------------------------------- */
/*      Try to find layer.                                              */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetName()) )
            break;
    }

    if( iLayer == nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete layer '%s', but this layer is not known to OGR.",
                  pszLayerName );
        return;
    }

    DeleteLayer(iLayer);
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRODSDataSource::DeleteLayer(int iLayer)
{
    AnalyseFile();

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

    bUpdated = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           HasHeaderLine()                            */
/************************************************************************/

static int HasHeaderLine(OGRLayer* poLayer)
{
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    int bHasHeaders = FALSE;

    for(int j=0;j<poFDefn->GetFieldCount();j++)
    {
        if (strcmp(poFDefn->GetFieldDefn(j)->GetNameRef(),
                    CPLSPrintf("Field%d", j+1)) != 0)
            bHasHeaders = TRUE;
    }

    return bHasHeaders;
}

/************************************************************************/
/*                            WriteLayer()                              */
/************************************************************************/

static void WriteLayer(VSILFILE* fp, OGRLayer* poLayer)
{
    int j;
    const char* pszLayerName = poLayer->GetName();
    char* pszXML = OGRGetXML_UTF8_EscapedString(pszLayerName);
    VSIFPrintfL(fp, "<table:table table:name=\"%s\">\n", pszXML);
    CPLFree(pszXML);
    
    poLayer->ResetReading();

    OGRFeature* poFeature = poLayer->GetNextFeature();

    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    int bHasHeaders = HasHeaderLine(poLayer);

    for(j=0;j<poFDefn->GetFieldCount();j++)
    {
        int nStyleNumber = 1;
        if (poFDefn->GetFieldDefn(j)->GetType() == OFTDateTime)
            nStyleNumber = 2;
        VSIFPrintfL(fp, "<table:table-column table:style-name=\"co%d\" "
                        "table:default-cell-style-name=\"Default\"/>\n",
                    nStyleNumber);
    }

    if (bHasHeaders && poFeature != NULL)
    {
        VSIFPrintfL(fp, "<table:table-row>\n");
        for(j=0;j<poFDefn->GetFieldCount();j++)
        {
            const char* pszVal = poFDefn->GetFieldDefn(j)->GetNameRef();

            VSIFPrintfL(fp, "<table:table-cell office:value-type=\"string\">\n");
            pszXML = OGRGetXML_UTF8_EscapedString(pszVal);
            VSIFPrintfL(fp, "<text:p>%s</text:p>\n", pszXML);
            CPLFree(pszXML);
            VSIFPrintfL(fp, "</table:table-cell>\n");
        }
        VSIFPrintfL(fp, "</table:table-row>\n");
    }

    while(poFeature != NULL)
    {
        VSIFPrintfL(fp, "<table:table-row>\n");
        for(j=0;j<poFeature->GetFieldCount();j++)
        {
            if (poFeature->IsFieldSet(j))
            {
                OGRFieldType eType = poFDefn->GetFieldDefn(j)->GetType();

                if (eType == OFTReal)
                {
                    VSIFPrintfL(fp, "<table:table-cell office:value-type=\"float\" "
                                "office:value=\"%.16f\"/>\n",
                                poFeature->GetFieldAsDouble(j));
                }
                else if (eType == OFTInteger)
                {
                    VSIFPrintfL(fp, "<table:table-cell office:value-type=\"float\" "
                                "office:value=\"%d\"/>\n",
                                poFeature->GetFieldAsInteger(j));
                }
                else if (eType == OFTDateTime)
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    poFeature->GetFieldAsDateTime(j, &nYear, &nMonth, &nDay,
                                                    &nHour, &nMinute, &nSecond, &nTZFlag);
                    VSIFPrintfL(fp, "<table:table-cell table:style-name=\"stDateTime\" "
                                "office:value-type=\"date\" "
                                "office:date-value=\"%04d-%02d-%02dT%02d:%02d:%02d\">\n",
                                nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    VSIFPrintfL(fp, "<text:p>%02d/%02d/%04d %02d:%02d:%02d</text:p>\n",
                                nDay, nMonth, nYear, nHour, nMinute, nSecond);
                    VSIFPrintfL(fp, "</table:table-cell>\n");
                }
                else if (eType == OFTDateTime)
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    poFeature->GetFieldAsDateTime(j, &nYear, &nMonth, &nDay,
                                                    &nHour, &nMinute, &nSecond, &nTZFlag);
                    VSIFPrintfL(fp, "<table:table-cell table:style-name=\"stDate\" "
                                "office:value-type=\"date\" "
                                "office:date-value=\"%04d-%02d-%02dT\">\n",
                                nYear, nMonth, nDay);
                    VSIFPrintfL(fp, "<text:p>%02d/%02d/%04d</text:p>\n",
                                nDay, nMonth, nYear);
                    VSIFPrintfL(fp, "</table:table-cell>\n");
                }
                else if (eType == OFTTime)
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    poFeature->GetFieldAsDateTime(j, &nYear, &nMonth, &nDay,
                                                    &nHour, &nMinute, &nSecond, &nTZFlag);
                    VSIFPrintfL(fp, "<table:table-cell table:style-name=\"stTime\" "
                                "office:value-type=\"time\" "
                                "office:time-value=\"PT%02dH%02dM%02dS\">\n",
                                nHour, nMinute, nSecond);
                    VSIFPrintfL(fp, "<text:p>%02d:%02d:%02d</text:p>\n",
                                nHour, nMinute, nSecond);
                    VSIFPrintfL(fp, "</table:table-cell>\n");
                }
                else
                {
                    const char* pszVal = poFeature->GetFieldAsString(j);
                    pszXML = OGRGetXML_UTF8_EscapedString(pszVal);
                    if (strncmp(pszVal, "of:=", 4) == 0)
                    {
                        VSIFPrintfL(fp, "<table:table-cell table:formula=\"%s\"/>\n", pszXML);
                    }
                    else
                    {
                        VSIFPrintfL(fp, "<table:table-cell office:value-type=\"string\">\n");
                        VSIFPrintfL(fp, "<text:p>%s</text:p>\n", pszXML);
                        VSIFPrintfL(fp, "</table:table-cell>\n");
                    }
                    CPLFree(pszXML);
                }
            }
            else
            {
                VSIFPrintfL(fp, "<table:table-cell/>\n");
            }
        }
        VSIFPrintfL(fp, "</table:table-row>\n");

        delete poFeature;
        poFeature = poLayer->GetNextFeature();
    }

    VSIFPrintfL(fp, "</table:table>\n");
}

/************************************************************************/
/*                            SyncToDisk()                              */
/************************************************************************/

OGRErr OGRODSDataSource::SyncToDisk()
{
    if (!bUpdated)
        return OGRERR_NONE;

    CPLAssert(fpSettings == NULL);
    CPLAssert(fpContent == NULL);

    VSIStatBufL sStat;
    if (VSIStatL(pszName, &sStat) == 0)
    {
        if (VSIUnlink( pszName ) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                    "Cannot delete %s", pszName);
            return OGRERR_FAILURE;
        }
    }

    /* Maintain new ZIP files opened */
    void *hZIP = CPLCreateZip(pszName, NULL);
    if (hZIP == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot create %s", pszName);
        return OGRERR_FAILURE;
    }

    /* Write uncopressed mimetype */
    char** papszOptions = CSLAddString(NULL, "COMPRESSED=NO");
    CPLCreateFileInZip(hZIP, "mimetype", papszOptions );
    CPLWriteFileInZip(hZIP, "application/vnd.oasis.opendocument.spreadsheet",
                      strlen("application/vnd.oasis.opendocument.spreadsheet"));
    CPLCloseFileInZip(hZIP);
    CSLDestroy(papszOptions);

    /* Now close ZIP file */
    CPLCloseZip(hZIP);
    hZIP = NULL;

    /* Re-open with VSILFILE */
    VSILFILE* fpZIP = VSIFOpenL(CPLSPrintf("/vsizip/%s", pszName), "ab");
    if (fpZIP == NULL)
        return OGRERR_FAILURE;

    VSILFILE* fp;
    int i;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/META-INF/manifest.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\">\n");
    VSIFPrintfL(fp, "<manifest:file-entry "
                "manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\" "
                "manifest:version=\"1.2\" manifest:full-path=\"/\"/>\n");
    VSIFPrintfL(fp, "<manifest:file-entry manifest:media-type=\"text/xml\" "
                    "manifest:full-path=\"content.xml\"/>\n");
    VSIFPrintfL(fp, "<manifest:file-entry manifest:media-type=\"text/xml\" "
                    "manifest:full-path=\"styles.xml\"/>\n");
    VSIFPrintfL(fp, "<manifest:file-entry manifest:media-type=\"text/xml\" "
                    "manifest:full-path=\"meta.xml\"/>\n");
    VSIFPrintfL(fp, "<manifest:file-entry manifest:media-type=\"text/xml\" "
                    "manifest:full-path=\"settings.xml\"/>\n");
    VSIFPrintfL(fp, "</manifest:manifest>\n");
    VSIFCloseL(fp);

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/meta.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<office:document-meta "
                "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
                "office:version=\"1.2\">\n");
    VSIFPrintfL(fp, "</office:document-meta>\n");
    VSIFCloseL(fp);

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/settings.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<office:document-settings "
                "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
                "xmlns:config=\"urn:oasis:names:tc:opendocument:xmlns:config:1.0\" "
                "xmlns:ooo=\"http://openoffice.org/2004/office\" "
                "office:version=\"1.2\">\n");
    VSIFPrintfL(fp, "<office:settings>\n");
    VSIFPrintfL(fp, "<config:config-item-set config:name=\"ooo:view-settings\">\n");
    VSIFPrintfL(fp, "<config:config-item-map-indexed config:name=\"Views\">\n");
    VSIFPrintfL(fp, "<config:config-item-map-entry>\n");
    VSIFPrintfL(fp, "<config:config-item-map-named config:name=\"Tables\">\n");
    for(i=0;i<nLayers;i++)
    {
        OGRLayer* poLayer = GetLayer(i);
        if (HasHeaderLine(poLayer))
        {
            /* Add vertical splitter */
            char* pszXML = OGRGetXML_UTF8_EscapedString(GetLayer(i)->GetName());
            VSIFPrintfL(fp, "<config:config-item-map-entry config:name=\"%s\">\n", pszXML);
            CPLFree(pszXML);
            VSIFPrintfL(fp, "<config:config-item config:name=\"VerticalSplitMode\" config:type=\"short\">2</config:config-item>\n");
            VSIFPrintfL(fp, "<config:config-item config:name=\"VerticalSplitPosition\" config:type=\"int\">1</config:config-item>\n");
            VSIFPrintfL(fp, "<config:config-item config:name=\"ActiveSplitRange\" config:type=\"short\">2</config:config-item>\n");
            VSIFPrintfL(fp, "<config:config-item config:name=\"PositionTop\" config:type=\"int\">0</config:config-item>\n");
            VSIFPrintfL(fp, "<config:config-item config:name=\"PositionBottom\" config:type=\"int\">1</config:config-item>\n");
            VSIFPrintfL(fp, "</config:config-item-map-entry>\n");
        }
    }
    VSIFPrintfL(fp, "</config:config-item-map-named>\n");
    VSIFPrintfL(fp, "</config:config-item-map-entry>\n");
    VSIFPrintfL(fp, "</config:config-item-map-indexed>\n");
    VSIFPrintfL(fp, "</config:config-item-set>\n");
    VSIFPrintfL(fp, "</office:settings>\n");
    VSIFPrintfL(fp, "</office:document-settings>\n");
    VSIFCloseL(fp);

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/styles.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<office:document-styles "
                "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
                "xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
                "office:version=\"1.2\">\n");
    VSIFPrintfL(fp, "<office:styles>\n");
    VSIFPrintfL(fp, "<style:style style:name=\"Default\" "
                    "style:family=\"table-cell\">\n");
    VSIFPrintfL(fp, "</style:style>\n");
    VSIFPrintfL(fp, "</office:styles>\n");
    VSIFPrintfL(fp, "</office:document-styles>\n");
    VSIFCloseL(fp);

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/content.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<office:document-content "
                "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
                "xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
                "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
                "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
                "xmlns:number=\"urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0\" "
                "xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" "
                "xmlns:of=\"urn:oasis:names:tc:opendocument:xmlns:of:1.2\" "
                "office:version=\"1.2\">\n");
    VSIFPrintfL(fp, "<office:scripts/>\n");
    VSIFPrintfL(fp, "<office:automatic-styles>\n");
    VSIFPrintfL(fp, "<style:style style:name=\"co1\" "
                    "style:family=\"table-column\">\n");
    VSIFPrintfL(fp, "<style:table-column-properties "
                    "fo:break-before=\"auto\" "
                    "style:column-width=\"2.5cm\"/>\n");
    VSIFPrintfL(fp, "</style:style>\n");
    VSIFPrintfL(fp, "<style:style style:name=\"co2\" "
                    "style:family=\"table-column\">\n");
    VSIFPrintfL(fp, "<style:table-column-properties "
                    "fo:break-before=\"auto\" "
                    "style:column-width=\"5cm\"/>\n");
    VSIFPrintfL(fp, "</style:style>\n");
    VSIFPrintfL(fp, "<number:date-style style:name=\"nDate\" "
                    "number:automatic-order=\"true\">\n");
    VSIFPrintfL(fp, "<number:day number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>/</number:text>\n");
    VSIFPrintfL(fp, "<number:month number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>/</number:text>\n");
    VSIFPrintfL(fp, "<number:year/>\n");
    VSIFPrintfL(fp, "</number:date-style>\n");
    VSIFPrintfL(fp, "<number:time-style style:name=\"nTime\">\n");
    VSIFPrintfL(fp, "<number:hours number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>:</number:text>\n");
    VSIFPrintfL(fp, "<number:minutes number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>:</number:text>\n");
    VSIFPrintfL(fp, "<number:seconds number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "</number:time-style>\n");
    VSIFPrintfL(fp, "<number:date-style style:name=\"nDateTime\" "
                    "number:automatic-order=\"true\">\n");
    VSIFPrintfL(fp, "<number:day number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>/</number:text>\n");
    VSIFPrintfL(fp, "<number:month number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>/</number:text>\n");
    VSIFPrintfL(fp, "<number:year number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text> </number:text>\n");
    VSIFPrintfL(fp, "<number:hours number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>:</number:text>\n");
    VSIFPrintfL(fp, "<number:minutes number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "<number:text>:</number:text>\n");
    VSIFPrintfL(fp, "<number:seconds number:style=\"long\"/>\n");
    VSIFPrintfL(fp, "</number:date-style>\n");
    VSIFPrintfL(fp, "<style:style style:name=\"stDate\" "
                    "style:family=\"table-cell\" "
                    "style:parent-style-name=\"Default\" "
                    "style:data-style-name=\"nDate\"/>\n");
    VSIFPrintfL(fp, "<style:style style:name=\"stTime\" "
                    "style:family=\"table-cell\" "
                    "style:parent-style-name=\"Default\" "
                    "style:data-style-name=\"nTime\"/>\n");
    VSIFPrintfL(fp, "<style:style style:name=\"stDateTime\" "
                    "style:family=\"table-cell\" "
                    "style:parent-style-name=\"Default\" "
                    "style:data-style-name=\"nDateTime\"/>\n");
    VSIFPrintfL(fp, "</office:automatic-styles>\n");
    VSIFPrintfL(fp, "<office:body>\n");
    VSIFPrintfL(fp, "<office:spreadsheet>\n");
    for(i=0;i<nLayers;i++)
    {
        WriteLayer(fp, GetLayer(i));
    }
    VSIFPrintfL(fp, "</office:spreadsheet>\n");
    VSIFPrintfL(fp, "</office:body>\n");
    VSIFPrintfL(fp, "</office:document-content>\n");
    VSIFCloseL(fp);

    /* Now close ZIP file */
    VSIFCloseL(fpZIP);

    /* Reset updated flag at datasource and layer level */
    bUpdated = FALSE;
    for(int i = 0; i<nLayers; i++)
    {
        ((OGRODSLayer*)papoLayers[i])->SetUpdated(FALSE);
    }

    return OGRERR_NONE;
}
