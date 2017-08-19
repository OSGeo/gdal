/******************************************************************************
 *
 * Project:  ODS Translator
 * Purpose:  Implements OGRODSDataSource class
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

#include "ogr_ods.h"
#include "ogr_mem.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_vsi_error.h"
#include "ods_formula.h"
#include <set>

CPL_CVSID("$Id$")

namespace OGRODS {

/************************************************************************/
/*                          ODSCellEvaluator                            */
/************************************************************************/

class ODSCellEvaluator : public IODSCellEvaluator
{
private:
        OGRODSLayer* poLayer;
        std::set<std::pair<int,int> > oVisisitedCells;

public:
        explicit ODSCellEvaluator(OGRODSLayer* poLayerIn) : poLayer(poLayerIn) {}

        int EvaluateRange(int nRow1, int nCol1, int nRow2, int nCol2,
                          std::vector<ods_formula_node>& aoOutValues) override;

        int Evaluate(int nRow, int nCol);
};

/************************************************************************/
/*                            OGRODSLayer()                             */
/************************************************************************/

OGRODSLayer::OGRODSLayer( OGRODSDataSource* poDSIn,
                          const char * pszName,
                          bool bUpdatedIn) :
    OGRMemLayer(pszName, NULL, wkbNone),
    poDS(poDSIn),
    bUpdated(CPL_TO_BOOL(bUpdatedIn)),
    bHasHeaderLine(false),
    m_poAttrQueryODS(NULL)
{}

/************************************************************************/
/*                            ~OGRODSLayer()                            */
/************************************************************************/

OGRODSLayer::~OGRODSLayer()
{
    delete m_poAttrQueryODS;
}

/************************************************************************/
/*                             Updated()                                */
/************************************************************************/

void OGRODSLayer::SetUpdated(bool bUpdatedIn)
{
    if (bUpdatedIn && !bUpdated && poDS->GetUpdatable())
    {
        bUpdated = true;
        poDS->SetUpdated();
    }
    else if (bUpdated && !bUpdatedIn)
    {
        bUpdated = false;
    }
}

/************************************************************************/
/*                           SyncToDisk()                               */
/************************************************************************/

OGRErr OGRODSLayer::SyncToDisk()
{
    poDS->FlushCache();
    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature* OGRODSLayer::GetNextFeature()
{
    while(true)
    {
        OGRFeature* poFeature = OGRMemLayer::GetNextFeature();
        if (poFeature == NULL )
            return NULL;
        poFeature->SetFID(poFeature->GetFID() + 1 + (bHasHeaderLine ? 1 : 0));
        if( m_poAttrQueryODS == NULL
               || m_poAttrQueryODS->Evaluate( poFeature ) )
        {
            return poFeature;
        }
        delete poFeature;
    }
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature* OGRODSLayer::GetFeature( GIntBig nFeatureId )
{
    OGRFeature* poFeature =
        OGRMemLayer::GetFeature(nFeatureId - (1 + (bHasHeaderLine ? 1 : 0)));
    if (poFeature)
        poFeature->SetFID(nFeatureId);
    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRODSLayer::GetFeatureCount( int bForce )
{
    if( m_poAttrQueryODS == NULL )
        return OGRMemLayer::GetFeatureCount(bForce);
    return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                           ISetFeature()                               */
/************************************************************************/

OGRErr OGRODSLayer::ISetFeature( OGRFeature *poFeature )
{
    if (poFeature == NULL)
        return OGRMemLayer::ISetFeature(poFeature);

    GIntBig nFID = poFeature->GetFID();
    if (nFID != OGRNullFID)
        poFeature->SetFID(nFID - (1 + (bHasHeaderLine ? 1 : 0)));
    SetUpdated();
    OGRErr eErr = OGRMemLayer::ISetFeature(poFeature);
    poFeature->SetFID(nFID);
    return eErr;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRODSLayer::DeleteFeature( GIntBig nFID )
{
    SetUpdated();
    return OGRMemLayer::DeleteFeature(nFID - (1 + (bHasHeaderLine ? 1 : 0)));
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRODSLayer::SetAttributeFilter( const char *pszQuery )

{
    // Intercept attribute filter since we mess up with FIDs
    OGRErr eErr = OGRLayer::SetAttributeFilter(pszQuery);
    delete m_poAttrQueryODS;
    m_poAttrQueryODS = m_poAttrQuery;
    m_poAttrQuery = NULL;
    return eErr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODSLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL && m_poAttrQueryODS == NULL;
    return OGRMemLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                          OGRODSDataSource()                          */
/************************************************************************/

OGRODSDataSource::OGRODSDataSource() :
    pszName(NULL),
    bUpdatable(false),
    bUpdated(false),
    bAnalysedFile(false),
    nLayers(0),
    papoLayers(NULL),
    fpSettings(NULL),
    nFlags(0),
    fpContent(NULL),
    bFirstLineIsHeaders(false),
    bAutodetectTypes(
        !EQUAL(CPLGetConfigOption("OGR_ODS_FIELD_TYPES", ""), "STRING")),
    oParser(NULL),
    bStopParsing(false),
    nWithoutEventCounter(0),
    nDataHandlerCounter(0),
    nCurLine(0),
    nEmptyRowsAccumulated(0),
    nRowsRepeated(0),
    nCurCol(0),
    nCellsRepeated(0),
    bEndTableParsing(false),
    poCurLayer(NULL),
    nStackDepth(0),
    nDepth(0)
{
    stateStack[0].eVal = STATE_DEFAULT;
    stateStack[0].nBeginDepth = 0;
}

/************************************************************************/
/*                         ~OGRODSDataSource()                          */
/************************************************************************/

OGRODSDataSource::~OGRODSDataSource()

{
    FlushCache();

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
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
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
    SetDescription(pszFilename);
    bUpdatable = CPL_TO_BOOL(bUpdatableIn);

    pszName = CPLStrdup( pszFilename );
    fpContent = fpContentIn;
    fpSettings = fpSettingsIn;

    return TRUE;
}

/************************************************************************/
/*                             Create()                                 */
/************************************************************************/

int OGRODSDataSource::Create( const char * pszFilename,
                              char ** /* papszOptions */ )
{
    bUpdated = true;
    bUpdatable = true;
    bAnalysedFile = true;

    pszName = CPLStrdup( pszFilename );

    return TRUE;
}

/************************************************************************/
/*                           startElementCbk()                          */
/************************************************************************/

static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
  static_cast<OGRODSDataSource *>(pUserData)->
      startElementCbk( pszName, ppszAttr );
}

void OGRODSDataSource::startElementCbk( const char *pszNameIn,
                                        const char **ppszAttr)
{
    if( bStopParsing )
        return;

    nWithoutEventCounter = 0;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: startElementDefault(pszNameIn, ppszAttr); break;
        case STATE_TABLE:   startElementTable(pszNameIn, ppszAttr); break;
        case STATE_ROW:     startElementRow(pszNameIn, ppszAttr); break;
        case STATE_CELL:    startElementCell(pszNameIn, ppszAttr); break;
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
    static_cast<OGRODSDataSource*>(pUserData)->endElementCbk(pszName);
}

void OGRODSDataSource::endElementCbk(const char *pszNameIn)
{
    if (bStopParsing)
        return;

    nWithoutEventCounter = 0;

    nDepth--;
    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: break;
        case STATE_TABLE:   endElementTable(pszNameIn); break;
        case STATE_ROW:     endElementRow(pszNameIn); break;
        case STATE_CELL:    endElementCell(pszNameIn); break;
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
    static_cast<OGRODSDataSource*>(pUserData)->dataHandlerCbk(data, nLen);
}

void OGRODSDataSource::dataHandlerCbk(const char *data, int nLen)
{
    if (bStopParsing)
        return;

    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = true;
        return;
    }

    nWithoutEventCounter = 0;

    switch(stateStack[nStackDepth].eVal)
    {
        case STATE_DEFAULT: break;
        case STATE_TABLE:   break;
        case STATE_ROW:     break;
        case STATE_CELL:    break;
        case STATE_TEXTP:
            dataHandlerTextP(data, nLen);
            break;
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
        bStopParsing = true;
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
        {
            GIntBig nVal = CPLAtoGIntBig(pszValue);
            if( !CPL_INT64_FITS_ON_INT32(nVal) )
                return OFTInteger64;
            else
                return OFTInteger;
        }
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
        char c = '\0';
        if (STARTS_WITH(pszValue, "PT") &&
            sscanf(pszValue + 2, "%02d%c%02d%c%02d%c",
                   &nHour, &c, &nMinute, &c, &nSecond, &c) == 6)
        {
            poFeature->SetField(i, 0, 0, 0, nHour, nMinute, static_cast<float>(nSecond), 0);
        }
        /* bug with kspread 2.1.2 ? */
        /* ex PT121234M56S */
        else if (STARTS_WITH(pszValue, "PT") &&
                 sscanf(pszValue + 2, "%02d%02d%02d%c%02d%c",
                        &nHour, &nHourRepeated, &nMinute, &c, &nSecond, &c) == 6 &&
                 nHour == nHourRepeated)
        {
            poFeature->SetField(i, 0, 0, 0, nHour, nMinute, static_cast<float>(nSecond), 0);
        }
    }
    else if (eType == OFTDate || eType == OFTDateTime)
    {
        OGRField sField;
        if (OGRParseXMLDateTime( pszValue, &sField ))
        {
            poFeature->SetField(i, &sField);
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
    bool bHeaderLineCandidate = true;

    for( size_t i = 0; i < apoFirstLineTypes.size(); i++ )
    {
        if (apoFirstLineTypes[i] != "string")
        {
            /* If the values in the first line are not text, then it is */
            /* not a header line */
            bHeaderLineCandidate = false;
            break;
        }
    }

    size_t nCountTextOnCurLine = 0;
    size_t nCountNonEmptyOnCurLine = 0;
    for( size_t i = 0; bHeaderLineCandidate && i < apoCurLineTypes.size(); i++ )
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
    bFirstLineIsHeaders = false;
    if (EQUAL(pszODSHeaders, "FORCE"))
        bFirstLineIsHeaders = true;
    else if (EQUAL(pszODSHeaders, "DISABLE"))
        bFirstLineIsHeaders = false;
    else if (osSetLayerHasSplitter.find(poCurLayer->GetName()) !=
             osSetLayerHasSplitter.end())
    {
        bFirstLineIsHeaders = true;
    }
    else if( bHeaderLineCandidate &&
             !apoFirstLineTypes.empty() &&
             apoFirstLineTypes.size() == apoCurLineTypes.size() &&
             nCountTextOnCurLine != apoFirstLineTypes.size() &&
             nCountNonEmptyOnCurLine != 0 )
    {
        bFirstLineIsHeaders = true;
    }
    CPLDebug("ODS", "%s %s",
             poCurLayer->GetName(),
             bFirstLineIsHeaders ? "has header line" : "has no header line");
}

/************************************************************************/
/*                          startElementDefault()                       */
/************************************************************************/

void OGRODSDataSource::startElementDefault(const char *pszNameIn,
                                           const char **ppszAttr)
{
    if (strcmp(pszNameIn, "table:table") == 0)
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
        bEndTableParsing = false;
    }
}

/************************************************************************/
/*                          startElementTable()                        */
/************************************************************************/

void OGRODSDataSource::startElementTable(const char *pszNameIn,
                                         const char **ppszAttr)
{
    if( strcmp(pszNameIn, "table:table-row") == 0 && !bEndTableParsing )
    {
        nRowsRepeated = atoi(
            GetAttributeValue(ppszAttr, "table:number-rows-repeated", "1"));
        if (nRowsRepeated > 65536)
        {
            bEndTableParsing = true;
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

void OGRODSDataSource::endElementTable( CPL_UNUSED /* in non-DEBUG*/ const char * pszNameIn )
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        // Only use of pszNameIn.
        CPLAssert(strcmp(pszNameIn, "table:table") == 0);

        if (nCurLine == 0 ||
            (nCurLine == 1 && apoFirstLineValues.empty()))
        {
            /* Remove empty sheet */
            delete poCurLayer;
            nLayers --;
            poCurLayer = NULL;
        }
        else if (nCurLine == 1)
        {
            /* If we have only one single line in the sheet */

            for( size_t i = 0; i < apoFirstLineValues.size(); i++ )
            {
                const char* pszFieldName = CPLSPrintf("Field%d", (int)i + 1);
                OGRFieldType eType = GetOGRFieldType(apoFirstLineValues[i].c_str(),
                                                     apoFirstLineTypes[i].c_str());
                OGRFieldDefn oFieldDefn(pszFieldName, eType);
                poCurLayer->CreateField(&oFieldDefn);
            }

            OGRFeature* poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
            for( size_t i = 0; i < apoFirstLineValues.size(); i++ )
            {
                SetField(poFeature, static_cast<int>(i), apoFirstLineValues[i].c_str());
            }
            CPL_IGNORE_RET_VAL(poCurLayer->CreateFeature(poFeature));
            delete poFeature;
        }

        if (poCurLayer)
        {
            if( CPLTestBool(CPLGetConfigOption("ODS_RESOLVE_FORMULAS", "YES")) )
            {
                poCurLayer->ResetReading();

                int nRow = 0;
                OGRFeature* poFeature = poCurLayer->GetNextFeature();
                while (poFeature)
                {
                    for( int i = 0; i < poFeature->GetFieldCount(); i++ )
                    {
                        if (poFeature->IsFieldSetAndNotNull(i) &&
                            poFeature->GetFieldDefnRef(i)->GetType() == OFTString)
                        {
                            const char* pszVal = poFeature->GetFieldAsString(i);
                            if (STARTS_WITH(pszVal, "of:="))
                            {
                                ODSCellEvaluator oCellEvaluator(poCurLayer);
                                oCellEvaluator.Evaluate(nRow, i);
                            }
                        }
                    }
                    delete poFeature;

                    poFeature = poCurLayer->GetNextFeature();
                    nRow ++;
                }
            }

            poCurLayer->ResetReading();

            reinterpret_cast<OGRMemLayer*>(poCurLayer)->
                SetUpdatable(bUpdatable);
            reinterpret_cast<OGRMemLayer*>(poCurLayer)->SetAdvertizeUTF8(true);
            reinterpret_cast<OGRODSLayer*>(poCurLayer)->SetUpdated(false);
        }

        poCurLayer = NULL;
    }
}

/************************************************************************/
/*                            startElementRow()                         */
/************************************************************************/

void OGRODSDataSource::startElementRow(const char *pszNameIn,
                                       const char **ppszAttr)
{
    if (strcmp(pszNameIn, "table:table-cell") == 0)
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
        if (pszFormula && STARTS_WITH(pszFormula, "of:="))
        {
            osFormula = pszFormula;
            if (osValueType.empty())
                osValueType = "formula";
        }
        else
            osFormula = "";

        nCellsRepeated = atoi(
            GetAttributeValue(ppszAttr, "table:number-columns-repeated", "1"));
    }
    else if (strcmp(pszNameIn, "table:covered-table-cell") == 0)
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

void OGRODSDataSource::endElementRow( CPL_UNUSED /*in non-DEBUG*/ const char * pszNameIn )
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszNameIn, "table:table-row") == 0);

        /* Remove blank columns at the right to defer type evaluation */
        /* until necessary */
        size_t i = apoCurLineTypes.size();
        while(i > 0)
        {
            i --;
            if (apoCurLineTypes[i] == "")
            {
                apoCurLineValues.resize(i);
                apoCurLineTypes.resize(i);
            }
            else
            {
                break;
            }
        }

        /* Do not add immediately empty rows. Wait until there is another non */
        /* empty row */
        OGRFeature* poFeature = NULL;

        if (nCurLine >= 2 && apoCurLineTypes.empty())
        {
            nEmptyRowsAccumulated += nRowsRepeated;
            return;
        }
        else if (nEmptyRowsAccumulated > 0)
        {
            for(i = 0; i < (size_t)nEmptyRowsAccumulated; i++)
            {
                poFeature = new OGRFeature(poCurLayer->GetLayerDefn());
                CPL_IGNORE_RET_VAL(poCurLayer->CreateFeature(poFeature));
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
            if (apoFirstLineTypes.empty())
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
                    SetField(poFeature, static_cast<int>(i), apoFirstLineValues[i].c_str());
                }
                CPL_IGNORE_RET_VAL(poCurLayer->CreateFeature(poFeature));
                delete poFeature;
            }
        }

        if (nCurLine >= 1 || (nCurLine == 0 && nRowsRepeated > 1))
        {
            /* Add new fields found on following lines. */
            if (apoCurLineValues.size() >
                (size_t)poCurLayer->GetLayerDefn()->GetFieldCount())
            {
                for( i = static_cast<size_t>(
                         poCurLayer->GetLayerDefn()->GetFieldCount());
                     i < apoCurLineValues.size();
                     i++ )
                {
                    const char* pszFieldName =
                        CPLSPrintf("Field%d", static_cast<int>(i) + 1);
                    const OGRFieldType eType = GetOGRFieldType(
                        apoCurLineValues[i].c_str(),
                        apoCurLineTypes[i].c_str() );
                    OGRFieldDefn oFieldDefn(pszFieldName, eType);
                    poCurLayer->CreateField(&oFieldDefn);
                }
            }

            /* Update field type if necessary */
            if (bAutodetectTypes)
            {
                for(i = 0; i < apoCurLineValues.size(); i++)
                {
                    if (!apoCurLineValues[i].empty() )
                    {
                        const OGRFieldType eValType = GetOGRFieldType(
                            apoCurLineValues[i].c_str(),
                            apoCurLineTypes[i].c_str());
                        const OGRFieldType eFieldType =
                            poCurLayer->GetLayerDefn()->GetFieldDefn(
                                static_cast<int>(i))->GetType();
                        if (eFieldType == OFTDateTime &&
                            (eValType == OFTDate || eValType == OFTTime) )
                        {
                            /* ok */
                        }
                        else if ( eFieldType == OFTReal &&
                                  (eValType == OFTInteger ||
                                   eValType == OFTInteger64))
                        {
                           /* ok */;
                        }
                        else if ( eFieldType == OFTInteger64 &&
                                  eValType == OFTInteger )
                        {
                            /* ok */;
                        }
                        else if ( eFieldType != OFTString &&
                                  eValType != eFieldType)
                        {
                            OGRFieldDefn oNewFieldDefn(
                                poCurLayer->GetLayerDefn()->GetFieldDefn(
                                    static_cast<int>(i)));
                            if( ( eFieldType == OFTDate ||
                                  eFieldType == OFTTime) &&
                                eValType == OFTDateTime )
                                oNewFieldDefn.SetType(OFTDateTime);
                            else if( (eFieldType == OFTInteger ||
                                      eFieldType == OFTInteger64 ) &&
                                     eValType == OFTReal)
                                oNewFieldDefn.SetType(OFTReal);
                            else if( eFieldType == OFTInteger &&
                                     eValType == OFTInteger64 )
                                oNewFieldDefn.SetType(OFTInteger64);
                            else
                                oNewFieldDefn.SetType(OFTString);
                            poCurLayer->AlterFieldDefn(
                                static_cast<int>(i), &oNewFieldDefn,
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
                    SetField( poFeature, static_cast<int>(i),
                              apoCurLineValues[i].c_str() );
                }
                CPL_IGNORE_RET_VAL(poCurLayer->CreateFeature(poFeature));
                delete poFeature;
            }
        }

        nCurLine += nRowsRepeated;
    }
}

/************************************************************************/
/*                           startElementCell()                         */
/************************************************************************/

void OGRODSDataSource::startElementCell(const char *pszNameIn,
                                        const char ** /*ppszAttr*/)
{
    if (osValue.empty() && strcmp(pszNameIn, "text:p") == 0)
    {
        PushState(STATE_TEXTP);
    }
}

/************************************************************************/
/*                            endElementCell()                          */
/************************************************************************/

void OGRODSDataSource::endElementCell( CPL_UNUSED /*in non-DEBUG*/ const char * pszNameIn )
{
    if (stateStack[nStackDepth].nBeginDepth == nDepth)
    {
        CPLAssert(strcmp(pszNameIn, "table:table-cell") == 0);

        for(int i = 0; i < nCellsRepeated; i++)
        {
            if( !osValue.empty() )
                apoCurLineValues.push_back(osValue);
            else
                apoCurLineValues.push_back(osFormula);
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

    bAnalysedFile = true;

    AnalyseSettings();

    oParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler( oParser, OGRODS::startElementCbk,
                           OGRODS::endElementCbk);
    XML_SetCharacterDataHandler(oParser, OGRODS::dataHandlerCbk);
    XML_SetUserData(oParser, this);

    nDepth = 0;
    nStackDepth = 0;
    stateStack[0].nBeginDepth = 0;
    bStopParsing = false;
    nWithoutEventCounter = 0;

    VSIFSeekL( fpContent, 0, SEEK_SET );

    char aBuf[BUFSIZ];
    int nDone = 0;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen =
            static_cast<unsigned int>(
                VSIFReadL( aBuf, 1, sizeof(aBuf), fpContent ) );
        nDone = VSIFEofL(fpContent);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "XML parsing of ODS file failed : %s at line %d, "
                      "column %d",
                      XML_ErrorString(XML_GetErrorCode(oParser)),
                      static_cast<int>(XML_GetCurrentLineNumber(oParser)),
                      static_cast<int>(XML_GetCurrentColumnNumber(oParser)));
            bStopParsing = true;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 10);

    XML_ParserFree(oParser);
    oParser = NULL;

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = true;
    }

    VSIFCloseL(fpContent);
    fpContent = NULL;

    bUpdated = false;
}

/************************************************************************/
/*                        startElementStylesCbk()                       */
/************************************************************************/

static void XMLCALL startElementStylesCbk( void *pUserData, const char *pszName,
                                           const char **ppszAttr)
{
    static_cast<OGRODSDataSource*>(pUserData)->
        startElementStylesCbk( pszName, ppszAttr );
}

void OGRODSDataSource::startElementStylesCbk( const char *pszNameIn,
                                              const char **ppszAttr)
{
    if (bStopParsing)
        return;

    nWithoutEventCounter = 0;

    if (nStackDepth == 0 &&
        strcmp(pszNameIn, "config:config-item-map-named") == 0 &&
        strcmp(GetAttributeValue(ppszAttr, "config:name", ""), "Tables") == 0)
    {
        stateStack[++nStackDepth].nBeginDepth = nDepth;
    }
    else if( nStackDepth == 1 &&
             strcmp(pszNameIn, "config:config-item-map-entry") == 0 )
    {
        const char* pszTableName =
            GetAttributeValue(ppszAttr, "config:name", NULL);
        if (pszTableName)
        {
            osCurrentConfigTableName = pszTableName;
            nFlags = 0;
            stateStack[++nStackDepth].nBeginDepth = nDepth;
        }
    }
    else if (nStackDepth == 2 && strcmp(pszNameIn, "config:config-item") == 0)
    {
        const char* pszConfigName =
            GetAttributeValue(ppszAttr, "config:name", NULL);
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
    static_cast<OGRODSDataSource*>(pUserData)->endElementStylesCbk(pszName);
}

void OGRODSDataSource::endElementStylesCbk(const char * /*pszName*/)
{
    if (bStopParsing)
        return;

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

static void XMLCALL dataHandlerStylesCbk( void *pUserData, const char *data,
                                          int nLen )
{
  static_cast<OGRODSDataSource *>(pUserData)->dataHandlerStylesCbk(data, nLen);
}

void OGRODSDataSource::dataHandlerStylesCbk(const char *data, int nLen)
{
    if (bStopParsing)
        return;

    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File probably corrupted (million laugh pattern)");
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = true;
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
    XML_SetElementHandler( oParser, OGRODS::startElementStylesCbk,
                           OGRODS::endElementStylesCbk );
    XML_SetCharacterDataHandler(oParser, OGRODS::dataHandlerStylesCbk);
    XML_SetUserData(oParser, this);

    nDepth = 0;
    nStackDepth = 0;
    bStopParsing = false;
    nWithoutEventCounter = 0;

    VSIFSeekL( fpSettings, 0, SEEK_SET );

    char aBuf[BUFSIZ];
    int nDone = 0;
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
            bStopParsing = true;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 10);

    XML_ParserFree(oParser);
    oParser = NULL;

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = true;
    }

    VSIFCloseL(fpSettings);
    fpSettings = NULL;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRODSDataSource::ICreateLayer( const char * pszLayerName,
                                OGRSpatialReference * /* poSRS */,
                                OGRwkbGeometryType /* eType */,
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
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
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

    papoLayers = static_cast<OGRLayer**>(
        CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*)) );
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    bUpdated = true;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRODSDataSource::DeleteLayer( const char *pszLayerName )

{
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
    int iLayer = 0;
    for( ; iLayer < nLayers; iLayer++ )
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

    bUpdated = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           HasHeaderLine()                            */
/************************************************************************/

static bool HasHeaderLine(OGRLayer* poLayer)
{
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    bool bHasHeaders = false;

    for(int j=0;j<poFDefn->GetFieldCount();j++)
    {
        if (strcmp(poFDefn->GetFieldDefn(j)->GetNameRef(),
                    CPLSPrintf("Field%d", j+1)) != 0)
            bHasHeaders = true;
    }

    return bHasHeaders;
}

/************************************************************************/
/*                            WriteLayer()                              */
/************************************************************************/

static void WriteLayer(VSILFILE* fp, OGRLayer* poLayer)
{
    const char* pszLayerName = poLayer->GetName();
    char* pszXML = OGRGetXML_UTF8_EscapedString(pszLayerName);
    VSIFPrintfL(fp, "<table:table table:name=\"%s\">\n", pszXML);
    CPLFree(pszXML);

    poLayer->ResetReading();

    OGRFeature* poFeature = poLayer->GetNextFeature();

    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    const bool bHasHeaders = HasHeaderLine(poLayer);

    for( int j=0; j<poFDefn->GetFieldCount(); j++ )
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
        for( int j=0; j<poFDefn->GetFieldCount(); j++)
        {
            const char* pszVal = poFDefn->GetFieldDefn(j)->GetNameRef();

            VSIFPrintfL(
                 fp, "<table:table-cell office:value-type=\"string\">\n" );
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
        for( int j=0; j<poFeature->GetFieldCount(); j++ )
        {
            if (poFeature->IsFieldSetAndNotNull(j))
            {
                const OGRFieldType eType = poFDefn->GetFieldDefn(j)->GetType();

                if (eType == OFTReal)
                {
                    VSIFPrintfL(
                        fp,
                        "<table:table-cell office:value-type=\"float\" "
                        "office:value=\"%.16f\"/>\n",
                        poFeature->GetFieldAsDouble(j) );
                }
                else if (eType == OFTInteger)
                {
                    VSIFPrintfL(
                         fp, "<table:table-cell office:value-type=\"float\" "
                         "office:value=\"%d\"/>\n",
                         poFeature->GetFieldAsInteger(j));
                }
                else if (eType == OFTInteger64)
                {
                    VSIFPrintfL(
                         fp, "<table:table-cell office:value-type=\"float\" "
                         "office:value=\"" CPL_FRMT_GIB "\"/>\n",
                         poFeature->GetFieldAsInteger64(j));
                }
                else if (eType == OFTDateTime)
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    int nTZFlag = 0;
                    float fSecond = 0.0f;
                    poFeature->GetFieldAsDateTime(
                        j, &nYear, &nMonth, &nDay,
                        &nHour, &nMinute, &fSecond, &nTZFlag );
                    if( OGR_GET_MS(fSecond) )
                    {
                        VSIFPrintfL(
                            fp,
                            "<table:table-cell "
                            "table:style-name=\"stDateTimeMilliseconds\" "
                            "office:value-type=\"date\" "
                            "office:date-value="
                            "\"%04d-%02d-%02dT%02d:%02d:%06.3f\">\n",
                            nYear, nMonth, nDay, nHour, nMinute, fSecond );
                        VSIFPrintfL(
                            fp,
                            "<text:p>%02d/%02d/%04d "
                            "%02d:%02d:%06.3f</text:p>\n",
                            nDay, nMonth, nYear, nHour, nMinute, fSecond );
                    }
                    else
                    {
                        VSIFPrintfL(
                            fp,
                            "<table:table-cell "
                            "table:style-name=\"stDateTime\" "
                            "office:value-type=\"date\" "
                            "office:date-value="
                            "\"%04d-%02d-%02dT%02d:%02d:%02d\">\n",
                            nYear, nMonth, nDay, nHour, nMinute,
                            static_cast<int>(fSecond) );
                        VSIFPrintfL(
                            fp,
                            "<text:p>%02d/%02d/%04d %02d:%02d:%02d</text:p>\n",
                            nDay, nMonth, nYear, nHour, nMinute,
                            static_cast<int>(fSecond) );
                    }
                    VSIFPrintfL(fp, "</table:table-cell>\n");
                }
                else if (eType == OFTDate)
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    int nSecond = 0;
                    int nTZFlag = 0;
                    poFeature->GetFieldAsDateTime(
                        j, &nYear, &nMonth, &nDay,
                        &nHour, &nMinute, &nSecond, &nTZFlag );
                    VSIFPrintfL(
                        fp, "<table:table-cell table:style-name=\"stDate\" "
                        "office:value-type=\"date\" "
                        "office:date-value=\"%04d-%02d-%02d\">\n",
                        nYear, nMonth, nDay);
                    VSIFPrintfL(fp, "<text:p>%02d/%02d/%04d</text:p>\n",
                                nDay, nMonth, nYear);
                    VSIFPrintfL(fp, "</table:table-cell>\n");
                }
                else if (eType == OFTTime)
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    int nSecond = 0;
                    int nTZFlag = 0;
                    poFeature->GetFieldAsDateTime(
                        j, &nYear, &nMonth, &nDay,
                        &nHour, &nMinute, &nSecond, &nTZFlag );
                    VSIFPrintfL(
                        fp, "<table:table-cell table:style-name=\"stTime\" "
                        "office:value-type=\"time\" "
                        "office:time-value=\"PT%02dH%02dM%02dS\">\n",
                        nHour, nMinute, nSecond );
                    VSIFPrintfL(fp, "<text:p>%02d:%02d:%02d</text:p>\n",
                                nHour, nMinute, nSecond);
                    VSIFPrintfL(fp, "</table:table-cell>\n");
                }
                else
                {
                    const char* pszVal = poFeature->GetFieldAsString(j);
                    pszXML = OGRGetXML_UTF8_EscapedString(pszVal);
                    if (STARTS_WITH(pszVal, "of:="))
                    {
                        VSIFPrintfL(
                            fp,
                            "<table:table-cell table:formula=\"%s\"/>\n",
                            pszXML);
                    }
                    else
                    {
                        VSIFPrintfL(
                             fp, "<table:table-cell "
                             "office:value-type=\"string\">\n");
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
/*                            FlushCache()                              */
/************************************************************************/

void OGRODSDataSource::FlushCache()
{
    if (!bUpdated)
        return;

    CPLAssert(fpSettings == NULL);
    CPLAssert(fpContent == NULL);

    VSIStatBufL sStat;
    if (VSIStatL(pszName, &sStat) == 0)
    {
        if (VSIUnlink( pszName ) != 0)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot delete %s", pszName);
            return;
        }
    }

    /* Maintain new ZIP files opened */
    void *hZIP = CPLCreateZip(pszName, NULL);
    if (hZIP == NULL)
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Cannot create %s: %s", pszName, VSIGetLastErrorMsg() );
        return;
    }

    /* Write uncompressed mimetype */
    char** papszOptions = CSLAddString(NULL, "COMPRESSED=NO");
    if( CPLCreateFileInZip(hZIP, "mimetype", papszOptions ) != CE_None )
    {
        CSLDestroy(papszOptions);
        CPLCloseZip(hZIP);
        return;
    }
    CSLDestroy(papszOptions);
    if( CPLWriteFileInZip(
        hZIP, "application/vnd.oasis.opendocument.spreadsheet",
        static_cast<int>(
            strlen("application/vnd.oasis.opendocument.spreadsheet")) )
        != CE_None )
    {
        CPLCloseZip(hZIP);
        return;
    }
    CPLCloseFileInZip(hZIP);

    /* Now close ZIP file */
    CPLCloseZip(hZIP);
    hZIP = NULL;

    /* Re-open with VSILFILE */
    VSILFILE* fpZIP = VSIFOpenL(CPLSPrintf("/vsizip/%s", pszName), "ab");
    if (fpZIP == NULL)
        return;

    VSILFILE* fp = VSIFOpenL(CPLSPrintf(
        "/vsizip/%s/META-INF/manifest.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(
        fp,
        "<manifest:manifest ""xmlns:manifest=\"urn:oasis:names:tc:"
        "opendocument:xmlns:manifest:1.0\">\n");
    VSIFPrintfL(
         fp, "<manifest:file-entry "
         "manifest:media-type=\"application/vnd.oasis."
         "opendocument.spreadsheet\" "
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
    VSIFPrintfL(
        fp, "<office:document-meta "
        "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
        "office:version=\"1.2\">\n" );
    VSIFPrintfL(fp, "</office:document-meta>\n");
    VSIFCloseL(fp);

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/settings.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(
         fp, "<office:document-settings "
         "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
         "xmlns:config=\"urn:oasis:names:tc:opendocument:xmlns:config:1.0\" "
         "xmlns:ooo=\"http://openoffice.org/2004/office\" "
         "office:version=\"1.2\">\n" );
    VSIFPrintfL(fp, "<office:settings>\n");
    VSIFPrintfL(
         fp, "<config:config-item-set config:name=\"ooo:view-settings\">\n");
    VSIFPrintfL(fp, "<config:config-item-map-indexed config:name=\"Views\">\n");
    VSIFPrintfL(fp, "<config:config-item-map-entry>\n");
    VSIFPrintfL(fp, "<config:config-item-map-named config:name=\"Tables\">\n");
    for(int i=0;i<nLayers;i++)
    {
        OGRLayer* poLayer = papoLayers[i];
        if (HasHeaderLine(poLayer))
        {
            /* Add vertical splitter */
            char* pszXML = OGRGetXML_UTF8_EscapedString(poLayer->GetName());
            VSIFPrintfL(
                 fp,
                 "<config:config-item-map-entry config:name=\"%s\">\n", pszXML);
            CPLFree(pszXML);
            VSIFPrintfL(
                 fp,
                 "<config:config-item config:name=\"VerticalSplitMode\" "
                 "config:type=\"short\">2</config:config-item>\n" );
            VSIFPrintfL(
                 fp,
                 "<config:config-item config:name=\"VerticalSplitPosition\" "
                 "config:type=\"int\">1</config:config-item>\n" );
            VSIFPrintfL(
                fp,
                "<config:config-item config:name=\"ActiveSplitRange\" "
                "config:type=\"short\">2</config:config-item>\n" );
            VSIFPrintfL(
                fp,
                "<config:config-item config:name=\"PositionTop\" "
                "config:type=\"int\">0</config:config-item>\n" );
            VSIFPrintfL(
                fp, "<config:config-item config:name=\"PositionBottom\" "
                "config:type=\"int\">1</config:config-item>\n" );
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
    VSIFPrintfL(
         fp, "<office:document-styles "
         "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
         "xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
         "office:version=\"1.2\">\n" );
    VSIFPrintfL(fp, "<office:styles>\n");
    VSIFPrintfL(fp, "<style:style style:name=\"Default\" "
                    "style:family=\"table-cell\">\n");
    VSIFPrintfL(fp, "</style:style>\n");
    VSIFPrintfL(fp, "</office:styles>\n");
    VSIFPrintfL(fp, "</office:document-styles>\n");
    VSIFCloseL(fp);

    fp = VSIFOpenL(CPLSPrintf("/vsizip/%s/content.xml", pszName), "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(
         fp, "<office:document-content "
         "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
         "xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
         "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
         "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
         "xmlns:number=\"urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0\" "
         "xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:"
         "xsl-fo-compatible:1.0\" "
         "xmlns:of=\"urn:oasis:names:tc:opendocument:xmlns:of:1.2\" "
         "office:version=\"1.2\">\n" );
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
    VSIFPrintfL(
         fp, "<number:date-style style:name=\"nDateTimeMilliseconds\">\n");
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
    VSIFPrintfL(
         fp,
         "<number:seconds number:style=\"long\" "
         "number:decimal-places=\"3\"/>\n" );
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
    VSIFPrintfL(fp, "<style:style style:name=\"stDateTimeMilliseconds\" "
                    "style:family=\"table-cell\" "
                    "style:parent-style-name=\"Default\" "
                    "style:data-style-name=\"nDateTimeMilliseconds\"/>\n");
    VSIFPrintfL(fp, "</office:automatic-styles>\n");
    VSIFPrintfL(fp, "<office:body>\n");
    VSIFPrintfL(fp, "<office:spreadsheet>\n");
    for(int i=0;i<nLayers;i++)
    {
        WriteLayer(fp, papoLayers[i]);
    }
    VSIFPrintfL(fp, "</office:spreadsheet>\n");
    VSIFPrintfL(fp, "</office:body>\n");
    VSIFPrintfL(fp, "</office:document-content>\n");
    VSIFCloseL(fp);

    /* Now close ZIP file */
    VSIFCloseL(fpZIP);

    /* Reset updated flag at datasource and layer level */
    bUpdated = false;
    for(int i = 0; i<nLayers; i++)
    {
        reinterpret_cast<OGRODSLayer*>(papoLayers[i])->SetUpdated(false);
    }

    return;
}

/************************************************************************/
/*                          EvaluateRange()                             */
/************************************************************************/

int ODSCellEvaluator::EvaluateRange(int nRow1, int nCol1, int nRow2, int nCol2,
                                    std::vector<ods_formula_node>& aoOutValues)
{
    if (nRow1 < 0 || nRow1 >= poLayer->GetFeatureCount(FALSE) ||
        nCol1 < 0 || nCol1 >= poLayer->GetLayerDefn()->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Invalid cell (row=%d, col=%d)", nRow1 + 1, nCol1 + 1);
        return FALSE;
    }

    if (nRow2 < 0 || nRow2 >= poLayer->GetFeatureCount(FALSE) ||
        nCol2 < 0 || nCol2 >= poLayer->GetLayerDefn()->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Invalid cell (row=%d, col=%d)", nRow2 + 1, nCol2 + 1);
        return FALSE;
    }

    const int nIndexBackup = static_cast<int>(poLayer->GetNextReadFID());

    if (poLayer->SetNextByIndex(nRow1) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot fetch feature for row = %d", nRow1);
        return FALSE;
    }

    for(int nRow = nRow1; nRow <= nRow2; nRow ++)
    {
        OGRFeature* poFeature = poLayer->GetNextFeatureWithoutFIDHack();

        if (poFeature == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot fetch feature for for row = %d", nRow);
            poLayer->SetNextByIndex(nIndexBackup);
            return FALSE;
        }

        for(int nCol = nCol1; nCol <= nCol2; nCol++)
        {
            if (!poFeature->IsFieldSetAndNotNull(nCol))
            {
                aoOutValues.push_back(ods_formula_node());
            }
            else if (poFeature->GetFieldDefnRef(nCol)->GetType() == OFTInteger)
            {
                aoOutValues.push_back(ods_formula_node(
                    poFeature->GetFieldAsInteger(nCol)));
            }
            else if (poFeature->GetFieldDefnRef(nCol)->GetType() == OFTReal)
            {
                aoOutValues.push_back(ods_formula_node(
                    poFeature->GetFieldAsDouble(nCol)));
            }
            else
            {
                std::string osVal(poFeature->GetFieldAsString(nCol));
                if (STARTS_WITH(osVal.c_str(), "of:="))
                {
                    delete poFeature;
                    poFeature = NULL;

                    if (!Evaluate(nRow, nCol))
                    {
#ifdef DEBUG_VERBOSE
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "Formula at cell (%d, %d) "
                                  "has not yet been resolved",
                                  nRow + 1, nCol + 1 );
#endif
                        poLayer->SetNextByIndex(nIndexBackup);
                        return FALSE;
                    }

                    poLayer->SetNextByIndex(nRow);
                    poFeature = poLayer->GetNextFeatureWithoutFIDHack();

                    if (!poFeature->IsFieldSetAndNotNull(nCol))
                    {
                        aoOutValues.push_back(ods_formula_node());
                    }
                    else if (poFeature->GetFieldDefnRef(nCol)->GetType() == OFTInteger)
                    {
                        aoOutValues.push_back(ods_formula_node(poFeature->GetFieldAsInteger(nCol)));
                    }
                    else if (poFeature->GetFieldDefnRef(nCol)->GetType() == OFTReal)
                    {
                        aoOutValues.push_back(ods_formula_node(poFeature->GetFieldAsDouble(nCol)));
                    }
                    else
                    {
                        osVal = poFeature->GetFieldAsString(nCol);
                        if (!STARTS_WITH(osVal.c_str(), "of:="))
                        {
                            CPLValueType eType = CPLGetValueType(osVal.c_str());
                            /* Try to convert into numeric value if possible */
                            if (eType != CPL_VALUE_STRING)
                                aoOutValues.push_back(ods_formula_node(CPLAtofM(osVal.c_str())));
                            else
                                aoOutValues.push_back(ods_formula_node(osVal.c_str()));
                        }
                    }
                }
                else
                {
                    CPLValueType eType = CPLGetValueType(osVal.c_str());
                    /* Try to convert into numeric value if possible */
                    if (eType != CPL_VALUE_STRING)
                        aoOutValues.push_back(ods_formula_node(CPLAtofM(osVal.c_str())));
                    else
                        aoOutValues.push_back(ods_formula_node(osVal.c_str()));
                }
            }
        }

        delete poFeature;
    }

    poLayer->SetNextByIndex(nIndexBackup);

    return TRUE;
}

/************************************************************************/
/*                            Evaluate()                                */
/************************************************************************/

int ODSCellEvaluator::Evaluate(int nRow, int nCol)
{
    if (oVisisitedCells.find(std::pair<int,int>(nRow, nCol)) != oVisisitedCells.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Circular dependency with (row=%d, col=%d)", nRow + 1, nCol + 1);
        return FALSE;
    }

    oVisisitedCells.insert(std::pair<int,int>(nRow, nCol));

    if (poLayer->SetNextByIndex(nRow) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot fetch feature for row = %d", nRow);
        return FALSE;
    }

    OGRFeature* poFeature = poLayer->GetNextFeatureWithoutFIDHack();
    if (poFeature->IsFieldSetAndNotNull(nCol) &&
        poFeature->GetFieldDefnRef(nCol)->GetType() == OFTString)
    {
        const char* pszVal = poFeature->GetFieldAsString(nCol);
        if (STARTS_WITH(pszVal, "of:="))
        {
            ods_formula_node* expr_out = ods_formula_compile( pszVal + 4 );
            if (expr_out &&
                expr_out->Evaluate(this) &&
                expr_out->eNodeType == SNT_CONSTANT)
            {
                /* Refetch feature in case Evaluate() modified another cell in this row */
                delete poFeature;
                poLayer->SetNextByIndex(nRow);
                poFeature = poLayer->GetNextFeatureWithoutFIDHack();

                if (expr_out->field_type == ODS_FIELD_TYPE_EMPTY)
                {
                    poFeature->UnsetField(nCol);
                    poLayer->SetFeatureWithoutFIDHack(poFeature);
                }
                else if (expr_out->field_type == ODS_FIELD_TYPE_INTEGER)
                {
                    poFeature->SetField(nCol, expr_out->int_value);
                    poLayer->SetFeatureWithoutFIDHack(poFeature);
                }
                else if (expr_out->field_type == ODS_FIELD_TYPE_FLOAT)
                {
                    poFeature->SetField(nCol, expr_out->float_value);
                    poLayer->SetFeatureWithoutFIDHack(poFeature);
                }
                else if (expr_out->field_type == ODS_FIELD_TYPE_STRING)
                {
                    poFeature->SetField(nCol, expr_out->string_value);
                    poLayer->SetFeatureWithoutFIDHack(poFeature);
                }
            }
            delete expr_out;
        }
    }

    delete poFeature;

    return TRUE;
}

} /* end of OGRODS namespace */
