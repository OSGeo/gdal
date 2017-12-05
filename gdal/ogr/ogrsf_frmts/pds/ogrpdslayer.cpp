/******************************************************************************
 *
 * Project:  PDS Translator
 * Purpose:  Implements OGRPDSLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_pds.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "cpl_safemaths.hpp"

#include <algorithm>

CPL_CVSID("$Id$")

namespace OGRPDS {

/************************************************************************/
/*                           OGRPDSLayer()                              */
/************************************************************************/

OGRPDSLayer::OGRPDSLayer( CPLString osTableIDIn,
                          const char* pszLayerName, VSILFILE* fp,
                          CPLString osLabelFilename,
                          CPLString osStructureFilename,
                          int nRecordsIn,
                          int nStartBytesIn, int nRecordSizeIn,
                          GByte* pabyRecordIn, bool bIsASCII ) :
    poFeatureDefn(new OGRFeatureDefn( pszLayerName )),
    osTableID(osTableIDIn),
    fpPDS(fp),
    nRecords(nRecordsIn),
    nStartBytes(nStartBytesIn),
    nRecordSize(nRecordSizeIn),
    pabyRecord(pabyRecordIn),
    nNextFID(0),
    nLongitudeIndex(-1),
    nLatitudeIndex(-1),
    pasFieldDesc(NULL)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    if (!osStructureFilename.empty())
    {
        ReadStructure(osStructureFilename);
    }
    else
    {
        ReadStructure(osLabelFilename);
    }

    if (bIsASCII &&
        poFeatureDefn->GetFieldCount() == 0)
    {
        VSIFSeekL( fpPDS, nStartBytes, SEEK_SET );
        VSIFReadL( pabyRecord, nRecordSize, 1, fpPDS);

        char **papszTokens = CSLTokenizeString2(
                (const char*)pabyRecord, " ", CSLT_HONOURSTRINGS );
        const int nTokens = CSLCount(papszTokens);
        for( int i = 0; i < nTokens; i++ )
        {
            const char* pszStr = papszTokens[i];
            char ch = '\0';
            OGRFieldType eFieldType = OFTInteger;
            while((ch = *pszStr) != 0)
            {
                if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-')
                {
                }
                else if (ch == '.')
                {
                    eFieldType = OFTReal;
                }
                else
                {
                    eFieldType = OFTString;
                    break;
                }
                pszStr ++;
            }
            char szFieldName[32];
            snprintf(szFieldName, sizeof(szFieldName), "field_%d",
                    poFeatureDefn->GetFieldCount() + 1);
            OGRFieldDefn oFieldDefn(szFieldName, eFieldType);
            poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
        CSLDestroy(papszTokens);
    }

    if (nLongitudeIndex >= 0 && nLatitudeIndex >= 0)
    {
        poFeatureDefn->SetGeomType( wkbPoint );
    }

    ResetReading();
}

/************************************************************************/
/*                             ~OGRPDSLayer()                           */
/************************************************************************/

OGRPDSLayer::~OGRPDSLayer()

{
    CPLFree(pasFieldDesc);
    poFeatureDefn->Release();
    VSIFree(pabyRecord);

    VSIFCloseL( fpPDS );
}

/************************************************************************/
/*                           ReadStructure()                            */
/************************************************************************/

void OGRPDSLayer::ReadStructure(CPLString osStructureFilename)

{
    VSILFILE* fpStructure = VSIFOpenL(osStructureFilename, "rb");
    if (fpStructure == NULL)
        return;

    int nFields = 0;
    bool bInObjectColumn = false;
    int nExpectedColumnNumber = 0;
    CPLString osColumnName;
    CPLString osColumnDataType;
    CPLString osColumnStartByte;
    CPLString osColumnBytes;
    CPLString osColumnFormat;
    CPLString osColumnUnit;
    CPLString osColumnItems;
    CPLString osColumnItemBytes;
    int nRowBytes = nRecordSize;
    while( true )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        const char* pszLine = CPLReadLine2L(fpStructure, 256, NULL);
        CPLPopErrorHandler();
        CPLErrorReset();
        if (pszLine == NULL)
            break;

        char **papszTokens =
                CSLTokenizeString2( pszLine, " =", CSLT_HONOURSTRINGS );
        const int nTokens = CSLCount(papszTokens);

        if (bInObjectColumn && nTokens >= 1 &&
            EQUAL(papszTokens[0], "END_OBJECT"))
        {
            if (!osColumnName.empty() &&
                !osColumnDataType.empty() &&
                !osColumnStartByte.empty() &&
                !osColumnBytes.empty())
            {
                pasFieldDesc =
                    static_cast<FieldDesc*>(
                        CPLRealloc( pasFieldDesc,
                                    (nFields + 1) * sizeof(FieldDesc)) );
                pasFieldDesc[nFields].nStartByte = atoi(osColumnStartByte) - 1;
                pasFieldDesc[nFields].nByteCount = atoi(osColumnBytes);
                if (pasFieldDesc[nFields].nStartByte >= 0 &&
                    pasFieldDesc[nFields].nByteCount > 0 &&
                    pasFieldDesc[nFields].nStartByte +
                    pasFieldDesc[nFields].nByteCount <= nRecordSize)
                {
                    OGRFieldType eFieldType = OFTString;
                    pasFieldDesc[nFields].eFormat = CHARACTER;
                    pasFieldDesc[nFields].nItemBytes = atoi(osColumnItemBytes);
                    pasFieldDesc[nFields].nItems = atoi(osColumnItems);
                    if (pasFieldDesc[nFields].nItems == 0)
                        pasFieldDesc[nFields].nItems = 1;
                    if (pasFieldDesc[nFields].nItemBytes == 0 &&
                        pasFieldDesc[nFields].nItems == 1)
                        pasFieldDesc[nFields].nItemBytes =
                            pasFieldDesc[nFields].nByteCount;

                    if (osColumnDataType.compare("ASCII_REAL") == 0)
                    {
                        eFieldType = OFTReal;
                        pasFieldDesc[nFields].eFormat = ASCII_REAL;
                    }
                    else if (osColumnDataType.compare("ASCII_INTEGER") == 0)
                    {
                        eFieldType = OFTInteger;
                        pasFieldDesc[nFields].eFormat = ASCII_INTEGER;
                    }
                    else if (osColumnDataType.compare("MSB_UNSIGNED_INTEGER") ==
                             0)
                    {
                        if (pasFieldDesc[nFields].nItemBytes == 1 ||
                            pasFieldDesc[nFields].nItemBytes == 2)
                        {
                            if (pasFieldDesc[nFields].nItems > 1)
                                eFieldType = OFTIntegerList;
                            else
                                eFieldType = OFTInteger;
                        }
                        else
                        {
                            pasFieldDesc[nFields].nItemBytes = 4;
                            if (pasFieldDesc[nFields].nItems > 1)
                                eFieldType = OFTRealList;
                            else
                                eFieldType = OFTReal;
                        }
                        pasFieldDesc[nFields].eFormat = MSB_UNSIGNED_INTEGER;
                    }
                    else if (osColumnDataType.compare("MSB_INTEGER") == 0)
                    {
                        if (pasFieldDesc[nFields].nItemBytes != 1 &&
                            pasFieldDesc[nFields].nItemBytes != 2)
                            pasFieldDesc[nFields].nItemBytes = 4;
                        if (pasFieldDesc[nFields].nItems > 1)
                            eFieldType = OFTIntegerList;
                        else
                            eFieldType = OFTInteger;
                        pasFieldDesc[nFields].eFormat = MSB_INTEGER;
                    }
                    else if (osColumnDataType.compare("IEEE_REAL") == 0)
                    {
                        if (pasFieldDesc[nFields].nItemBytes != 4)
                            pasFieldDesc[nFields].nItemBytes = 4;
                        if (pasFieldDesc[nFields].nItems > 1)
                            eFieldType = OFTRealList;
                        else
                            eFieldType = OFTReal;
                        pasFieldDesc[nFields].eFormat = IEEE_REAL;
                    }

                    OGRFieldDefn oFieldDefn(osColumnName, eFieldType);
                    if ((pasFieldDesc[nFields].eFormat == ASCII_REAL &&
                            !osColumnFormat.empty() &&
                            osColumnFormat[0] == 'F') ||
                        (pasFieldDesc[nFields].eFormat == ASCII_INTEGER &&
                            !osColumnFormat.empty() &&
                            osColumnFormat[0] == 'I'))
                    {
                        const char* pszFormat = osColumnFormat.c_str();
                        int nWidth = atoi(pszFormat + 1);
                        oFieldDefn.SetWidth(nWidth);
                        const char* pszPoint = strchr(pszFormat, '.');
                        if (pszPoint)
                        {
                            int nPrecision = atoi(pszPoint + 1);
                            oFieldDefn.SetPrecision(nPrecision);
                        }
                    }
                    else if (oFieldDefn.GetType() == OFTString &&
                                !osColumnFormat.empty() &&
                                osColumnFormat[0] == 'A')
                    {
                        const char* pszFormat = osColumnFormat.c_str();
                        int nWidth = atoi(pszFormat + 1);
                        oFieldDefn.SetWidth(nWidth);
                    }
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);

                    if (oFieldDefn.GetType() == OFTReal &&
                        osColumnUnit.compare("DEGREE") == 0)
                    {
                        if (osColumnName.compare("LONGITUDE") == 0)
                            nLongitudeIndex = nFields;
                        else if (osColumnName.compare("LATITUDE") == 0)
                            nLatitudeIndex = nFields;
                    }

                    nFields ++;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                                "Field %d out of record extents", nFields);
                    CSLDestroy(papszTokens);
                    break;
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Did not get expected records for field %d", nFields);
                CSLDestroy(papszTokens);
                break;
            }
            bInObjectColumn = false;
        }
        else if (nTokens == 2)
        {
            if (EQUAL(papszTokens[0], "PDS_VERSION_ID"))
            {
                CSLDestroy(papszTokens);
                papszTokens = NULL;
                while( true )
                {
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    pszLine = CPLReadLine2L(fpStructure, 256, NULL);
                    CPLPopErrorHandler();
                    CPLErrorReset();
                    if (pszLine == NULL)
                        break;
                    papszTokens =
                        CSLTokenizeString2( pszLine, " =", CSLT_HONOURSTRINGS );
                    int nTokens2 = CSLCount(papszTokens);
                    if (nTokens2 == 2 &&
                        EQUAL(papszTokens[0], "OBJECT") &&
                        EQUAL(papszTokens[1], osTableID.c_str()))
                    {
                        break;
                    }
                    CSLDestroy(papszTokens);
                    papszTokens = NULL;
                }
                CSLDestroy(papszTokens);
                papszTokens = NULL;
                if (pszLine == NULL)
                    break;
            }
            else if (EQUAL(papszTokens[0], "ROW_BYTES"))
            {
                nRowBytes = atoi(papszTokens[1]);
                if( nRowBytes < 0 || nRowBytes > 10*1024*1024)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid value of ROW_BYTES");
                    CSLDestroy(papszTokens);
                    break;
                }
            }
            else if (EQUAL(papszTokens[0], "ROW_SUFFIX_BYTES"))
            {
                try
                {
                    nRowBytes = (CPLSM(nRowBytes) + CPLSM(atoi(papszTokens[1]))).v();
                }
                catch( const CPLSafeIntOverflow& )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid value of ROW_SUFFIX_BYTES");
                    CSLDestroy(papszTokens);
                    break;
                }
                if( nRowBytes < 0 || nRowBytes > 10*1024*1024)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid value of ROW_SUFFIX_BYTES");
                    CSLDestroy(papszTokens);
                    break;
                }
            }
            else if (EQUAL(papszTokens[0], "OBJECT") &&
                     EQUAL(papszTokens[1], "COLUMN"))
            {
                if (nRowBytes > nRecordSize)
                {
                    nRecordSize = nRowBytes;
                    VSIFree(pabyRecord);
                    pabyRecord = (GByte*) CPLMalloc(nRecordSize + 1);
                    pabyRecord[nRecordSize] = 0;
                }
                else
                    nRecordSize = nRowBytes;

                nExpectedColumnNumber ++;
                bInObjectColumn = true;
                osColumnName = "";
                osColumnDataType = "";
                osColumnStartByte = "";
                osColumnBytes = "";
                osColumnItems = "";
                osColumnItemBytes = "";
                osColumnFormat = "";
                osColumnUnit = "";
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "COLUMN_NUMBER"))
            {
                int nColumnNumber = atoi(papszTokens[1]);
                if (nColumnNumber != nExpectedColumnNumber)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                                "Did not get expected column number");
                    CSLDestroy(papszTokens);
                    break;
                }
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "NAME"))
            {
                osColumnName = "\"";
                osColumnName += papszTokens[1];
                osColumnName += "\"";
                OGRPDSDataSource::CleanString(osColumnName);
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "DATA_TYPE"))
            {
                osColumnDataType = papszTokens[1];
                OGRPDSDataSource::CleanString(osColumnDataType);
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "START_BYTE"))
            {
                osColumnStartByte = papszTokens[1];
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "BYTES"))
            {
                osColumnBytes = papszTokens[1];
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "ITEMS"))
            {
                osColumnItems = papszTokens[1];
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "ITEM_BYTES"))
            {
                osColumnItemBytes = papszTokens[1];
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "FORMAT"))
            {
                osColumnFormat = papszTokens[1];
            }
            else if (bInObjectColumn && EQUAL(papszTokens[0], "UNIT"))
            {
                osColumnUnit = papszTokens[1];
            }
        }
        CSLDestroy(papszTokens);
    }
    VSIFCloseL(fpStructure);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPDSLayer::ResetReading()

{
    nNextFID = 0;
    VSIFSeekL( fpPDS, nStartBytes, SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPDSLayer::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRPDSLayer::GetNextRawFeature()
{
    if (nNextFID == nRecords)
        return NULL;
    int nRead = (int)VSIFReadL( pabyRecord, 1, nRecordSize, fpPDS);
    if (nRead != nRecordSize)
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    int nFieldCount = poFeatureDefn->GetFieldCount();
    if (pasFieldDesc != NULL)
    {
        for( int i=0;i<nFieldCount;i++)
        {
            if (pasFieldDesc[i].eFormat == ASCII_REAL ||
                pasFieldDesc[i].eFormat == ASCII_INTEGER ||
                pasFieldDesc[i].eFormat == CHARACTER)
            {
                char* pchEnd = reinterpret_cast<char*>(
                    &pabyRecord[pasFieldDesc[i].nStartByte +
                                pasFieldDesc[i].nByteCount]);
                char chSaved = *pchEnd;
                *pchEnd = 0;
                const char* pszValue = (const char*)(pabyRecord +
                                                    pasFieldDesc[i].nStartByte);
                if( pasFieldDesc[i].eFormat == CHARACTER )
                {
                    poFeature->SetField(i, pszValue);
                }
                else
                {
                    poFeature->SetField(i, CPLString(pszValue).Trim().c_str());
                }
                *pchEnd = chSaved;
            }
            else if (pasFieldDesc[i].eFormat == MSB_UNSIGNED_INTEGER &&
                     pasFieldDesc[i].nStartByte +
                     pasFieldDesc[i].nItemBytes * pasFieldDesc[i].nItems <= nRecordSize)
            {
                if (pasFieldDesc[i].nItemBytes == 1)
                {
                    if (pasFieldDesc[i].nItems > 1)
                    {
                        int* panValues = static_cast<int *>(
                            CPLMalloc(sizeof(int) * pasFieldDesc[i].nItems) );
                        for( int j = 0; j < pasFieldDesc[i].nItems; j++ )
                        {
                            panValues[j] = pabyRecord[pasFieldDesc[i].nStartByte + j];
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        poFeature->SetField(i, pabyRecord[pasFieldDesc[i].nStartByte]);
                    }
                }
                else if (pasFieldDesc[i].nItemBytes == 2)
                {
                    if (pasFieldDesc[i].nItems > 1)
                    {
                        int* panValues = (int*)CPLMalloc(sizeof(int) * pasFieldDesc[i].nItems);
                        for( int j = 0; j < pasFieldDesc[i].nItems; j++ )
                        {
                            unsigned short sVal = 0;
                            memcpy(&sVal, pabyRecord + pasFieldDesc[i].nStartByte + 2 * j, 2);
                            CPL_MSBPTR16(&sVal);
                            panValues[j] = sVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        unsigned short sVal = 0;
                        memcpy(&sVal, pabyRecord + pasFieldDesc[i].nStartByte, 2);
                        CPL_MSBPTR16(&sVal);
                        poFeature->SetField(i, (int)sVal);
                    }
                }
                else if (pasFieldDesc[i].nItemBytes == 4)
                {
                    if (pasFieldDesc[i].nItems > 1)
                    {
                        double* padfValues = (double*)CPLMalloc(sizeof(double) * pasFieldDesc[i].nItems);
                        for( int j = 0; j < pasFieldDesc[i].nItems; j++ )
                        {
                            unsigned int nVal = 0;
                            memcpy(&nVal, pabyRecord + pasFieldDesc[i].nStartByte + 4 * j, 4);
                            CPL_MSBPTR32(&nVal);
                            padfValues[j] = (double)nVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, padfValues);
                        CPLFree(padfValues);
                    }
                    else
                    {
                        unsigned int nVal = 0;
                        memcpy(&nVal, pabyRecord + pasFieldDesc[i].nStartByte, 4);
                        CPL_MSBPTR32(&nVal);
                        poFeature->SetField(i, (double)nVal);
                    }
                }
            }
            else if (pasFieldDesc[i].eFormat == MSB_INTEGER &&
                     pasFieldDesc[i].nStartByte +
                     pasFieldDesc[i].nItemBytes * pasFieldDesc[i].nItems <= nRecordSize)
            {
                if (pasFieldDesc[i].nItemBytes == 1)
                {
                    if (pasFieldDesc[i].nItems > 1)
                    {
                        int* panValues = (int*)CPLMalloc(sizeof(int) * pasFieldDesc[i].nItems);
                        for( int j = 0; j < pasFieldDesc[i].nItems; j++ )
                        {
                            panValues[j] = ((char*)pabyRecord)[pasFieldDesc[i].nStartByte + j];
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        poFeature->SetField(i, ((char*)pabyRecord)[pasFieldDesc[i].nStartByte]);
                    }
                }
                else if (pasFieldDesc[i].nItemBytes == 2)
                {
                    if (pasFieldDesc[i].nItems > 1)
                    {
                        int* panValues = (int*)CPLMalloc(sizeof(int) * pasFieldDesc[i].nItems);
                        for( int j = 0; j < pasFieldDesc[i].nItems; j++ )
                        {
                            short sVal = 0;
                            memcpy(&sVal, pabyRecord + pasFieldDesc[i].nStartByte + 2 * j, 2);
                            CPL_MSBPTR16(&sVal);
                            panValues[j] = sVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        short sVal = 0;
                        memcpy(&sVal, pabyRecord + pasFieldDesc[i].nStartByte, 2);
                        CPL_MSBPTR16(&sVal);
                        poFeature->SetField(i, (int)sVal);
                    }
                }
                else if (pasFieldDesc[i].nItemBytes == 4)
                {
                    if (pasFieldDesc[i].nItems > 1)
                    {
                        int* panValues = (int*)CPLMalloc(sizeof(int) * pasFieldDesc[i].nItems);
                        for( int j = 0; j < pasFieldDesc[i].nItems; j++ )
                        {
                            int nVal = 0;
                            memcpy(&nVal, pabyRecord + pasFieldDesc[i].nStartByte + 4 * j, 4);
                            CPL_MSBPTR32(&nVal);
                            panValues[j] = nVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        int nVal = 0;
                        memcpy(&nVal, pabyRecord + pasFieldDesc[i].nStartByte, 4);
                        CPL_MSBPTR32(&nVal);
                        poFeature->SetField(i, nVal);
                    }
                }
            }
            else if (pasFieldDesc[i].eFormat == IEEE_REAL &&
                     pasFieldDesc[i].nStartByte +
                     pasFieldDesc[i].nItemBytes * pasFieldDesc[i].nItems <= nRecordSize &&
                     pasFieldDesc[i].nItemBytes == 4)
            {
                if (pasFieldDesc[i].nItems > 1)
                {
                    double* padfValues = (double*)CPLMalloc(sizeof(double) * pasFieldDesc[i].nItems);
                    for( int j = 0; j < pasFieldDesc[i].nItems; j++ )
                    {
                        float fVal = 0.0f;
                        memcpy(&fVal, pabyRecord + pasFieldDesc[i].nStartByte + 4 * j, 4);
                        CPL_MSBPTR32(&fVal);
                        padfValues[j] = (double)fVal;
                    }
                    poFeature->SetField(i, pasFieldDesc[i].nItems, padfValues);
                    CPLFree(padfValues);
                }
                else
                {
                    float fVal = 0.0f;
                    memcpy(&fVal, pabyRecord + pasFieldDesc[i].nStartByte, 4);
                    CPL_MSBPTR32(&fVal);
                    poFeature->SetField(i, (double)fVal);
                }
            }
        }
    }
    else
    {
        char **papszTokens = CSLTokenizeString2(
                (const char*)pabyRecord, " ", CSLT_HONOURSTRINGS );
        const int nTokens = std::min(CSLCount(papszTokens), nFieldCount);
        for( int i = 0; i < nTokens; i++ )
        {
            poFeature->SetField(i, papszTokens[i]);
        }
        CSLDestroy(papszTokens);
    }

    if (nLongitudeIndex >= 0 && nLatitudeIndex >= 0)
        poFeature->SetGeometryDirectly(new OGRPoint(
                                poFeature->GetFieldAsDouble(nLongitudeIndex),
                                poFeature->GetFieldAsDouble(nLatitudeIndex)));

    poFeature->SetFID(nNextFID++);

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDSLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap,OLCFastFeatureCount) &&
        m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return TRUE;

    if (EQUAL(pszCap,OLCRandomRead))
        return TRUE;

    if (EQUAL(pszCap,OLCFastSetNextByIndex) &&
        m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRPDSLayer::GetFeatureCount(int bForce )
{
    if (TestCapability(OLCFastFeatureCount))
        return nRecords;

    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPDSLayer::GetFeature( GIntBig nFID )
{
    if (nFID < 0 || nFID >= nRecords)
        return NULL;

    nNextFID = (int)nFID;
    VSIFSeekL( fpPDS, nStartBytes + nNextFID * nRecordSize, SEEK_SET );
    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetNextByIndex()                             */
/************************************************************************/

OGRErr OGRPDSLayer::SetNextByIndex( GIntBig nIndex )
{
    if (!TestCapability(OLCFastSetNextByIndex))
        return OGRLayer::SetNextByIndex( nIndex );

    if (nIndex < 0 || nIndex >= nRecords)
        return OGRERR_FAILURE;

    nNextFID = (int)nIndex;
    VSIFSeekL( fpPDS, nStartBytes + nNextFID * nRecordSize, SEEK_SET );
    return OGRERR_NONE;
}

} /* end of OGRPDS namespace */
