/******************************************************************************
 * $Id$
 *
 * Project:  XLSX Translator
 * Purpose:  Implements OGRXLSXDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
                            const char * pszName,
                            int bUpdatedIn) :
                                OGRMemLayer(pszName, NULL, wkbNone)
{
    bInit = FALSE;
    nSheetId = nSheetIdIn;
    poDS = poDSIn;
    bUpdated = bUpdatedIn;
    bHasHeaderLine = FALSE;
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
/*                             Updated()                                */
/************************************************************************/

void OGRXLSXLayer::SetUpdated(int bUpdatedIn)
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

OGRErr OGRXLSXLayer::SyncToDisk()
{
    poDS->FlushCache();
    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature* OGRXLSXLayer::GetNextFeature()
{
    Init();
    OGRFeature* poFeature = OGRMemLayer::GetNextFeature();
    if (poFeature)
        poFeature->SetFID(poFeature->GetFID() + 1 + bHasHeaderLine);
    return poFeature;
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature* OGRXLSXLayer::GetFeature( GIntBig nFeatureId )
{
    Init();
    OGRFeature* poFeature = OGRMemLayer::GetFeature(nFeatureId - (1 + bHasHeaderLine));
    if (poFeature)
        poFeature->SetFID(nFeatureId);
    return poFeature;
}

/************************************************************************/
/*                           ISetFeature()                               */
/************************************************************************/

OGRErr OGRXLSXLayer::ISetFeature( OGRFeature *poFeature )
{
    Init();
    if (poFeature == NULL)
        return OGRMemLayer::ISetFeature(poFeature);

    long nFID = poFeature->GetFID();
    if (nFID != OGRNullFID)
        poFeature->SetFID(nFID - (1 + bHasHeaderLine));
    SetUpdated();
    OGRErr eErr = OGRMemLayer::ISetFeature(poFeature);
    poFeature->SetFID(nFID);
    return eErr;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRXLSXLayer::DeleteFeature( GIntBig nFID )
{
    Init();
    SetUpdated();
    return OGRMemLayer::DeleteFeature(nFID - (1 + bHasHeaderLine));
}

/************************************************************************/
/*                          OGRXLSXDataSource()                         */
/************************************************************************/

OGRXLSXDataSource::OGRXLSXDataSource()

{
    pszName = NULL;
    bUpdatable = FALSE;
    bUpdated = FALSE;

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
    FlushCache();

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
    bUpdatable = bUpdateIn;

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
/*                             Create()                                 */
/************************************************************************/

int OGRXLSXDataSource::Create( const char * pszFilename,
                               CPL_UNUSED char **papszOptions )
{
    bUpdated = TRUE;
    bUpdatable = TRUE;

    pszName = CPLStrdup( pszFilename );

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
        {
            GIntBig nVal = CPLAtoGIntBig(pszValue);
            if( (GIntBig)(int)nVal != nVal )
                return OFTInteger64;
            else
                return OFTInteger;
        }
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
        double dfNumberOfDaysSince1900 = CPLAtof(pszValue);
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
                                            CPL_UNUSED const char **ppszAttr)
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

void OGRXLSXDataSource::endElementTable(CPL_UNUSED const char *pszName)
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
            ((OGRMemLayer*)poCurLayer)->SetUpdatable(bUpdatable);
            ((OGRMemLayer*)poCurLayer)->SetAdvertizeUTF8(TRUE);
            ((OGRXLSXLayer*)poCurLayer)->SetUpdated(FALSE);
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

        const char* pszT = GetAttributeValue(ppszAttr, "t", "");
        if ( EQUAL(pszT,"s"))
            osValueType = "stringLookup";
        else if( EQUAL(pszT,"inlineStr") )
            osValueType = "string";

        osValue = "";
    }
}

/************************************************************************/
/*                            endElementRow()                           */
/************************************************************************/

void OGRXLSXDataSource::endElementRow(CPL_UNUSED const char *pszName)
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

            poCurLayer->SetHasHeaderLine(bFirstLineIsHeaders);

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
                        else if (eFieldType == OFTReal && (eValType == OFTInteger || eValType == OFTInteger64))
                        {
                           /* ok */;
                        }
                        else if (eFieldType == OFTInteger64 && eValType == OFTInteger )
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
                            else if ((eFieldType == OFTInteger || eFieldType == OFTInteger64) &&
                                     eValType == OFTReal)
                                oNewFieldDefn.SetType(OFTReal);
                            else if( eFieldType == OFTInteger && eValType == OFTInteger64 )
                                oNewFieldDefn.SetType(OFTInteger64);
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
                                         CPL_UNUSED const char **ppszAttr)
{
    if (osValue.size() == 0 && strcmp(pszName, "v") == 0)
    {
        PushState(STATE_TEXTV);
    }
    else if (osValue.size() == 0 && strcmp(pszName, "t") == 0)
    {
        PushState(STATE_TEXTV);
    }
}

/************************************************************************/
/*                            endElementCell()                          */
/************************************************************************/

void OGRXLSXDataSource::endElementCell(CPL_UNUSED const char *pszName)
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszName, "c") == 0);

        if (osValueType == "stringLookup")
        {
            int nIndex = atoi(osValue);
            if (nIndex >= 0 && nIndex < (int)(apoSharedStrings.size()))
                osValue = apoSharedStrings[nIndex];
            else
                CPLDebug("XLSX", "Cannot find string %d", nIndex);
            osValueType = "string";
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

void OGRXLSXDataSource::BuildLayer(OGRXLSXLayer* poLayer, int nSheetId)
{
    poCurLayer = poLayer;

    CPLString osSheetFilename(
        CPLSPrintf("/vsizip/%s/xl/worksheets/sheet%d.xml", pszName, nSheetId));
    const char* pszSheetFilename = osSheetFilename.c_str();
    VSILFILE* fp = VSIFOpenL(pszSheetFilename, "rb");
    if (fp == NULL)
        return;

    int bUpdatedBackup = bUpdated;

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

    bUpdated = bUpdatedBackup;
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
                                          CPL_UNUSED const char **ppszAttr)
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

void OGRXLSXDataSource::endElementSSCbk(CPL_UNUSED const char *pszName)
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
        /*const char* pszSheetId = GetAttributeValue(ppszAttr, "sheetId", NULL);*/
        if (pszSheetName /*&& pszSheetId*/)
        {
            /*int nSheetId = atoi(pszSheetId);*/
            int nSheetId = nLayers + 1;
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

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRXLSXDataSource::ICreateLayer( const char * pszLayerName,
                                 CPL_UNUSED OGRSpatialReference *poSRS,
                                 CPL_UNUSED OGRwkbGeometryType eType,
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
    OGRLayer* poLayer = new OGRXLSXLayer(this, nLayers + 1, pszLayerName, TRUE);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    bUpdated = TRUE;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRXLSXDataSource::DeleteLayer( const char *pszLayerName )

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

OGRErr OGRXLSXDataSource::DeleteLayer(int iLayer)
{
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
/*                            WriteOverride()                           */
/************************************************************************/

static void WriteOverride(VSILFILE* fp, const char* pszPartName, const char* pszContentType)
{
    VSIFPrintfL(fp, "<Override PartName=\"%s\" ContentType=\"%s\"/>\n",
                pszPartName, pszContentType);
}

#define XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
#define MAIN_NS "xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\""
#define SCHEMA_OD "http://schemas.openxmlformats.org/officeDocument/2006"
#define SCHEMA_OD_RS "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
#define SCHEMA_PACKAGE "http://schemas.openxmlformats.org/package/2006"
#define SCHEMA_PACKAGE_RS "http://schemas.openxmlformats.org/package/2006/relationships"

/************************************************************************/
/*                           WriteContentTypes()                        */
/************************************************************************/

static void WriteContentTypes(const char* pszName, int nLayers)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/[Content_Types].xml", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<Types xmlns=\"%s/content-types\">\n", SCHEMA_PACKAGE);
    WriteOverride(fp, "/_rels/.rels", "application/vnd.openxmlformats-package.relationships+xml");
    WriteOverride(fp, "/docProps/core.xml", "application/vnd.openxmlformats-package.core-properties+xml");
    WriteOverride(fp, "/docProps/app.xml", "application/vnd.openxmlformats-officedocument.extended-properties+xml");
    WriteOverride(fp, "/xl/_rels/workbook.xml.rels", "application/vnd.openxmlformats-package.relationships+xml");
    for(int i=0;i<nLayers;i++)
    {
        WriteOverride(fp, CPLSPrintf("/xl/worksheets/sheet%d.xml", i+1),
                      "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
    }
    WriteOverride(fp, "/xl/styles.xml","application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml");
    WriteOverride(fp, "/xl/workbook.xml","application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
    WriteOverride(fp, "/xl/sharedStrings.xml","application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml");
    VSIFPrintfL(fp, "</Types>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                             WriteApp()                               */
/************************************************************************/

static void WriteApp(const char* pszName)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/docProps/app.xml", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<Properties xmlns=\"%s/extended-properties\" "
                    "xmlns:vt=\"%s/docPropsVTypes\">\n", SCHEMA_OD, SCHEMA_OD);
    VSIFPrintfL(fp, "<TotalTime>0</TotalTime>\n");
    VSIFPrintfL(fp, "</Properties>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                             WriteCore()                              */
/************************************************************************/

static void WriteCore(const char* pszName)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/docProps/core.xml", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<cp:coreProperties xmlns:cp=\"%s/metadata/core-properties\" "
                    "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                    "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
                    "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n", SCHEMA_PACKAGE);
    VSIFPrintfL(fp, "<cp:revision>0</cp:revision>\n");
    VSIFPrintfL(fp, "</cp:coreProperties>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                            WriteWorkbook()                           */
/************************************************************************/

static void WriteWorkbook(const char* pszName, OGRDataSource* poDS)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/workbook.xml", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<workbook %s xmlns:r=\"%s\">\n", MAIN_NS, SCHEMA_OD_RS);
    VSIFPrintfL(fp, "<fileVersion appName=\"Calc\"/>\n");
    /*
    VSIFPrintfL(fp, "<workbookPr backupFile=\"false\" showObjects=\"all\" date1904=\"false\"/>\n");
    VSIFPrintfL(fp, "<workbookProtection/>\n");
    VSIFPrintfL(fp, "<bookViews>\n");
    VSIFPrintfL(fp, "<workbookView activeTab=\"0\" firstSheet=\"0\" showHorizontalScroll=\"true\" "
                    "showSheetTabs=\"true\" showVerticalScroll=\"true\" tabRatio=\"600\" windowHeight=\"8192\" "
                    "windowWidth=\"16384\" xWindow=\"0\" yWindow=\"0\"/>\n");
    VSIFPrintfL(fp, "</bookViews>\n");
    */
    VSIFPrintfL(fp, "<sheets>\n");
    for(int i=0;i<poDS->GetLayerCount();i++)
    {
        OGRXLSXLayer* poLayer = (OGRXLSXLayer*) poDS->GetLayer(i);
        const char* pszLayerName = poLayer->GetName();
        char* pszXML = OGRGetXML_UTF8_EscapedString(pszLayerName);
        VSIFPrintfL(fp, "<sheet name=\"%s\" sheetId=\"%d\" state=\"visible\" r:id=\"rId%d\"/>\n", pszXML, i+1, i+2);
        CPLFree(pszXML);
    }
    VSIFPrintfL(fp, "</sheets>\n");
    VSIFPrintfL(fp, "<calcPr iterateCount=\"100\" refMode=\"A1\" iterate=\"false\" iterateDelta=\"0.001\"/>\n");
    VSIFPrintfL(fp, "</workbook>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                            BuildColString()                          */
/************************************************************************/

static void BuildColString(char szCol[5], int nCol)
{
    /*
    A Z   AA AZ   BA BZ   ZA   ZZ   AAA    ZZZ      AAAA
    0 25  26 51   52 77   676  701  702    18277    18278
    */
    int k = 0;
    szCol[k++] = (nCol % 26) + 'A';
    while(nCol >= 26)
    {
        nCol /= 26;
        nCol --; /* We wouldn't need that if this was a proper base 26 numeration scheme ! */
        szCol[k++] = (nCol % 26) + 'A';
    }
    szCol[k] = 0;
    for(int l=0;l<k/2;l++)
    {
        char chTmp = szCol[k-1-l];
        szCol[k-1-l] = szCol[l];
        szCol[l] = chTmp;
    }
}

/************************************************************************/
/*                             WriteLayer()                             */
/************************************************************************/

static void WriteLayer(const char* pszName, OGRLayer* poLayer, int iLayer,
                       std::map<std::string,int>& oStringMap,
                       std::vector<std::string>& oStringList)
{
    VSILFILE* fp;
    int j;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/worksheets/sheet%d.xml", pszName, iLayer + 1), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<worksheet %s xmlns:r=\"%s\">\n", MAIN_NS, SCHEMA_OD_RS);
    /*
    VSIFPrintfL(fp, "<sheetViews>\n");
    VSIFPrintfL(fp, "<sheetView colorId=\"64\" defaultGridColor=\"true\" rightToLeft=\"false\" showFormulas=\"false\" showGridLines=\"true\" showOutlineSymbols=\"true\" showRowColHeaders=\"true\" showZeros=\"true\" tabSelected=\"%s\" topLeftCell=\"A1\" view=\"normal\" windowProtection=\"false\" workbookViewId=\"0\" zoomScale=\"100\" zoomScaleNormal=\"100\" zoomScalePageLayoutView=\"60\">\n",
                (i == 0) ? "true" : "false");
    VSIFPrintfL(fp, "<selection activeCell=\"A1\" activeCellId=\"0\" pane=\"topLeft\" sqref=\"A1\"/>\n");
    VSIFPrintfL(fp, "</sheetView>\n");
    VSIFPrintfL(fp, "</sheetViews>\n");*/

    poLayer->ResetReading();

    OGRFeature* poFeature = poLayer->GetNextFeature();

    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    int bHasHeaders = FALSE;
    int iRow = 1;

    VSIFPrintfL(fp, "<cols>\n");
    for(j=0;j<poFDefn->GetFieldCount();j++)
    {
        int nWidth = 15;
        if (poFDefn->GetFieldDefn(j)->GetType() == OFTDateTime)
            nWidth = 25;
        VSIFPrintfL(fp, "<col min=\"%d\" max=\"%d\" width=\"%d\"/>\n",
                    j+1, 1024, nWidth);

        if (strcmp(poFDefn->GetFieldDefn(j)->GetNameRef(),
                    CPLSPrintf("Field%d", j+1)) != 0)
            bHasHeaders = TRUE;
    }
    VSIFPrintfL(fp, "</cols>\n");

    VSIFPrintfL(fp, "<sheetData>\n");

    if (bHasHeaders && poFeature != NULL)
    {
        VSIFPrintfL(fp, "<row r=\"%d\">\n", iRow);
        for(j=0;j<poFDefn->GetFieldCount();j++)
        {
            const char* pszVal = poFDefn->GetFieldDefn(j)->GetNameRef();
            std::map<std::string,int>::iterator oIter = oStringMap.find(pszVal);
            int nStringIndex;
            if (oIter != oStringMap.end())
                nStringIndex = oIter->second;
            else
            {
                nStringIndex = (int)oStringList.size();
                oStringMap[pszVal] = nStringIndex;
                oStringList.push_back(pszVal);
            }

            char szCol[5];
            BuildColString(szCol, j);

            VSIFPrintfL(fp, "<c r=\"%s%d\" t=\"s\">\n", szCol, iRow);
            VSIFPrintfL(fp, "<v>%d</v>\n", nStringIndex);
            VSIFPrintfL(fp, "</c>\n");
        }
        VSIFPrintfL(fp, "</row>\n");

        iRow ++;
    }

    while(poFeature != NULL)
    {
        VSIFPrintfL(fp, "<row r=\"%d\">\n", iRow);
        for(j=0;j<poFeature->GetFieldCount();j++)
        {
            if (poFeature->IsFieldSet(j))
            {
                char szCol[5];
                BuildColString(szCol, j);

                OGRFieldType eType = poFDefn->GetFieldDefn(j)->GetType();

                if (eType == OFTReal)
                {
                    VSIFPrintfL(fp, "<c r=\"%s%d\">\n", szCol, iRow);
                    VSIFPrintfL(fp, "<v>%.16f</v>\n", poFeature->GetFieldAsDouble(j));
                    VSIFPrintfL(fp, "</c>\n");
                }
                else if (eType == OFTInteger)
                {
                    VSIFPrintfL(fp, "<c r=\"%s%d\">\n", szCol, iRow);
                    VSIFPrintfL(fp, "<v>%d</v>\n", poFeature->GetFieldAsInteger(j));
                    VSIFPrintfL(fp, "</c>\n");
                }
                else if (eType == OFTInteger64)
                {
                    VSIFPrintfL(fp, "<c r=\"%s%d\">\n", szCol, iRow);
                    VSIFPrintfL(fp, "<v>" CPL_FRMT_GIB "</v>\n", poFeature->GetFieldAsInteger64(j));
                    VSIFPrintfL(fp, "</c>\n");
                }
                else if (eType == OFTDate || eType == OFTDateTime || eType == OFTTime)
                {
                    VSIFPrintfL(fp, "<c r=\"%s%d\" s=\"%d\">\n", szCol, iRow,
                                (eType == OFTDate) ? 1 : (eType == OFTDateTime) ? 2 : 3);
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    poFeature->GetFieldAsDateTime(j, &nYear, &nMonth, &nDay,
                                                    &nHour, &nMinute, &nSecond, &nTZFlag);
                    struct tm brokendowntime;
                    memset(&brokendowntime, 0, sizeof(brokendowntime));
                    brokendowntime.tm_year = (eType == OFTTime) ? 70 : nYear - 1900;
                    brokendowntime.tm_mon = (eType == OFTTime) ? 0 : nMonth - 1;
                    brokendowntime.tm_mday = (eType == OFTTime) ? 1 : nDay;
                    brokendowntime.tm_hour = nHour;
                    brokendowntime.tm_min = nMinute;
                    brokendowntime.tm_sec = nSecond;
                    GIntBig nUnixTime = CPLYMDHMSToUnixTime(&brokendowntime);
                    double dfNumberOfDaysSince1900 = (1.0 * nUnixTime / NUMBER_OF_SECONDS_PER_DAY);
                    if (eType != OFTTime)
                        dfNumberOfDaysSince1900 += NUMBER_OF_DAYS_BETWEEN_1900_AND_1970;
                    if (eType == OFTDate)
                        VSIFPrintfL(fp, "<v>%d</v>\n", (int)(dfNumberOfDaysSince1900 + 0.1));
                    else
                        VSIFPrintfL(fp, "<v>%.16f</v>\n", dfNumberOfDaysSince1900);
                    VSIFPrintfL(fp, "</c>\n");
                }
                else
                {
                    const char* pszVal = poFeature->GetFieldAsString(j);
                    std::map<std::string,int>::iterator oIter = oStringMap.find(pszVal);
                    int nStringIndex;
                    if (oIter != oStringMap.end())
                        nStringIndex = oIter->second;
                    else
                    {
                        nStringIndex = (int)oStringList.size();
                        oStringMap[pszVal] = nStringIndex;
                        oStringList.push_back(pszVal);
                    }
                    VSIFPrintfL(fp, "<c r=\"%s%d\" t=\"s\">\n", szCol, iRow);
                    VSIFPrintfL(fp, "<v>%d</v>\n", nStringIndex);
                    VSIFPrintfL(fp, "</c>\n");
                }
            }
        }
        VSIFPrintfL(fp, "</row>\n");

        iRow ++;
        delete poFeature;
        poFeature = poLayer->GetNextFeature();
    }
    VSIFPrintfL(fp, "</sheetData>\n");
    VSIFPrintfL(fp, "</worksheet>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                        WriteSharedStrings()                          */
/************************************************************************/

static void WriteSharedStrings(const char* pszName,
                               CPL_UNUSED std::map<std::string,int>& oStringMap,
                               std::vector<std::string>& oStringList)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/sharedStrings.xml", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<sst %s uniqueCount=\"%d\">\n",
                MAIN_NS,
                (int)oStringList.size());
    for(int i = 0; i < (int)oStringList.size(); i++)
    {
        VSIFPrintfL(fp, "<si>\n");
        char* pszXML = OGRGetXML_UTF8_EscapedString(oStringList[i].c_str());
        VSIFPrintfL(fp, "<t>%s</t>\n", pszXML);
        CPLFree(pszXML);
        VSIFPrintfL(fp, "</si>\n");
    }
    VSIFPrintfL(fp, "</sst>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                           WriteStyles()                              */
/************************************************************************/

static void WriteStyles(const char* pszName)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/styles.xml", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<styleSheet %s>\n", MAIN_NS);
    VSIFPrintfL(fp, "<numFmts count=\"4\">\n");
    VSIFPrintfL(fp, "<numFmt formatCode=\"GENERAL\" numFmtId=\"164\"/>\n");
    VSIFPrintfL(fp, "<numFmt formatCode=\"DD/MM/YY\" numFmtId=\"165\"/>\n");
    VSIFPrintfL(fp, "<numFmt formatCode=\"DD/MM/YYYY\\ HH:MM:SS\" numFmtId=\"166\"/>\n");
    VSIFPrintfL(fp, "<numFmt formatCode=\"HH:MM:SS\" numFmtId=\"167\"/>\n");
    VSIFPrintfL(fp, "</numFmts>\n");
    VSIFPrintfL(fp, "<fonts count=\"1\">\n");
    VSIFPrintfL(fp, "<font>\n");
    VSIFPrintfL(fp, "<name val=\"Arial\"/>\n");
    VSIFPrintfL(fp, "<family val=\"2\"/>\n");
    VSIFPrintfL(fp, "<sz val=\"10\"/>\n");
    VSIFPrintfL(fp, "</font>\n");
    VSIFPrintfL(fp, "</fonts>\n");
    VSIFPrintfL(fp, "<fills count=\"1\">\n");
    VSIFPrintfL(fp, "<fill>\n");
    VSIFPrintfL(fp, "<patternFill patternType=\"none\"/>\n");
    VSIFPrintfL(fp, "</fill>\n");
    VSIFPrintfL(fp, "</fills>\n");
    VSIFPrintfL(fp, "<borders count=\"1\">\n");
    VSIFPrintfL(fp, "<border diagonalDown=\"false\" diagonalUp=\"false\">\n");
    VSIFPrintfL(fp, "<left/>\n");
    VSIFPrintfL(fp, "<right/>\n");
    VSIFPrintfL(fp, "<top/>\n");
    VSIFPrintfL(fp, "<bottom/>\n");
    VSIFPrintfL(fp, "<diagonal/>\n");
    VSIFPrintfL(fp, "</border>\n");
    VSIFPrintfL(fp, "</borders>\n");
    VSIFPrintfL(fp, "<cellStyleXfs count=\"1\">\n");
    VSIFPrintfL(fp, "<xf numFmtId=\"164\">\n");
    VSIFPrintfL(fp, "</xf>\n");
    VSIFPrintfL(fp, "</cellStyleXfs>\n");
    VSIFPrintfL(fp, "<cellXfs count=\"4\">\n");
    VSIFPrintfL(fp, "<xf numFmtId=\"164\" xfId=\"0\"/>\n");
    VSIFPrintfL(fp, "<xf numFmtId=\"165\" xfId=\"0\"/>\n");
    VSIFPrintfL(fp, "<xf numFmtId=\"166\" xfId=\"0\"/>\n");
    VSIFPrintfL(fp, "<xf numFmtId=\"167\" xfId=\"0\"/>\n");
    VSIFPrintfL(fp, "</cellXfs>\n");
    VSIFPrintfL(fp, "<cellStyles count=\"1\">\n");
    VSIFPrintfL(fp, "<cellStyle builtinId=\"0\" customBuiltin=\"false\" name=\"Normal\" xfId=\"0\"/>\n");
    VSIFPrintfL(fp, "</cellStyles>\n");
    VSIFPrintfL(fp, "</styleSheet>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                           WriteWorkbookRels()                        */
/************************************************************************/

static void WriteWorkbookRels(const char* pszName, int nLayers)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/xl/_rels/workbook.xml.rels", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<Relationships xmlns=\"%s\">\n", SCHEMA_PACKAGE_RS);
    VSIFPrintfL(fp, "<Relationship Id=\"rId1\" Type=\"%s/styles\" Target=\"styles.xml\"/>\n", SCHEMA_OD_RS);
    for(int i=0;i<nLayers;i++)
    {
        VSIFPrintfL(fp, "<Relationship Id=\"rId%d\" Type=\"%s/worksheet\" Target=\"worksheets/sheet%d.xml\"/>\n",
                    2 + i, SCHEMA_OD_RS, 1 + i);
    }
    VSIFPrintfL(fp, "<Relationship Id=\"rId%d\" Type=\"%s/sharedStrings\" Target=\"sharedStrings.xml\"/>\n",
                2 + nLayers, SCHEMA_OD_RS);
    VSIFPrintfL(fp, "</Relationships>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                             WriteDotRels()                           */
/************************************************************************/

static void WriteDotRels(const char* pszName)
{
    VSILFILE* fp;

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/_rels/.rels", pszName), "wb");
    VSIFPrintfL(fp, XML_HEADER);
    VSIFPrintfL(fp, "<Relationships xmlns=\"%s\">\n", SCHEMA_PACKAGE_RS);
    VSIFPrintfL(fp, "<Relationship Id=\"rId1\" Type=\"%s/officeDocument\" Target=\"xl/workbook.xml\"/>\n", SCHEMA_OD_RS);
    VSIFPrintfL(fp, "<Relationship Id=\"rId2\" Type=\"%s/metadata/core-properties\" Target=\"docProps/core.xml\"/>\n", SCHEMA_PACKAGE_RS);
    VSIFPrintfL(fp, "<Relationship Id=\"rId3\" Type=\"%s/extended-properties\" Target=\"docProps/app.xml\"/>\n", SCHEMA_OD_RS);
    VSIFPrintfL(fp, "</Relationships>\n");
    VSIFCloseL(fp);
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

void OGRXLSXDataSource::FlushCache()
{
    int i;

    if (!bUpdated)
        return;

    VSIStatBufL sStat;
    if (VSIStatL(pszName, &sStat) == 0)
    {
        if (VSIUnlink( pszName ) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                    "Cannot delete %s", pszName);
            return;
        }
    }

    /* Cause all layers to be initialized */
    for(int i = 0; i<nLayers; i++)
    {
        ((OGRXLSXLayer*)papoLayers[i])->GetLayerDefn();
    }

    /* Maintain new ZIP files opened */
    VSILFILE* fpZIP = VSIFOpenL(CPLSPrintf("/vsizip/%s", pszName), "wb");
    if (fpZIP == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot create %s", pszName);
        return;
    }

    WriteContentTypes(pszName, nLayers);

    //VSIMkdir(CPLSPrintf("/vsizip/%s/docProps", pszName),0755);
    WriteApp(pszName);
    WriteCore(pszName);

    //VSIMkdir(CPLSPrintf("/vsizip/%s/xl", pszName),0755);
    WriteWorkbook(pszName, this);

    std::map<std::string,int> oStringMap;
    std::vector<std::string> oStringList;

    //VSIMkdir(CPLSPrintf("/vsizip/%s/xl/worksheets", pszName),0755);
    for(i=0;i<nLayers;i++)
    {
        WriteLayer(pszName, GetLayer(i), i, oStringMap, oStringList);
    }

    WriteSharedStrings(pszName, oStringMap, oStringList);
    WriteStyles(pszName);

    //VSIMkdir(CPLSPrintf("/vsizip/%s/xl/_rels", pszName),0755);
    WriteWorkbookRels(pszName, nLayers);

    //VSIMkdir(CPLSPrintf("/vsizip/%s/_rels", pszName),0755);
    WriteDotRels(pszName);

    /* Now close ZIP file */
    VSIFCloseL(fpZIP);

    /* Reset updated flag at datasource and layer level */
    bUpdated = FALSE;
    for(int i = 0; i<nLayers; i++)
    {
        ((OGRXLSXLayer*)papoLayers[i])->SetUpdated(FALSE);
    }

    return;
}
