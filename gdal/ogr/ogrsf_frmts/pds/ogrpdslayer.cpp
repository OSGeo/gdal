/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRPDSLayer()                              */
/************************************************************************/

OGRPDSLayer::OGRPDSLayer(   CPLString osTableID,
                            const char* pszLayerName, VSILFILE* fp,
                            CPLString osLabelFilename,
                            CPLString osStructureFilename,
                            int nRecords,
                            int nStartBytes, int nRecordSize,
                            GByte* pabyRecordIn, int bIsASCII)

{
    fpPDS = fp;
    this->osTableID = osTableID;
    this->nRecords = nRecords;
    this->nStartBytes = nStartBytes;
    this->nRecordSize = nRecordSize;
    nLongitudeIndex = -1;
    nLatitudeIndex = -1;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    pasFieldDesc = NULL;

    pabyRecord = pabyRecordIn;

    if (osStructureFilename.size() != 0)
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
        int nTokens = CSLCount(papszTokens);
        int i;
        for(i=0;i<nTokens;i++)
        {
            const char* pszStr = papszTokens[i];
            char ch;
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
            sprintf(szFieldName, "field_%d",
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
    int nFields = 0;
    VSILFILE* fpStructure = VSIFOpenL(osStructureFilename, "rb");
    if (fpStructure == NULL)
        return;

    const char* pszLine;
    int bInObjectColumn = FALSE;
    int nExpectedColumnNumber = 0;
    CPLString osColumnName, osColumnDataType, osColumnStartByte,
              osColumnBytes, osColumnFormat, osColumnUnit,
              osColumnItems, osColumnItemBytes;
    int nRowBytes = nRecordSize;
    while(TRUE)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        pszLine = CPLReadLine2L(fpStructure, 256, NULL);
        CPLPopErrorHandler();
        CPLErrorReset();
        if (pszLine == NULL)
            break;

        char **papszTokens =
                CSLTokenizeString2( pszLine, " =", CSLT_HONOURSTRINGS );
        int nTokens = CSLCount(papszTokens);

        if (bInObjectColumn && nTokens >= 1 &&
            EQUAL(papszTokens[0], "END_OBJECT"))
        {
            if (osColumnName.size() != 0 &&
                osColumnDataType.size() != 0 &&
                osColumnStartByte.size() != 0 &&
                osColumnBytes.size() != 0)
            {
                pasFieldDesc =
                        (FieldDesc*) CPLRealloc(pasFieldDesc,
                                (nFields + 1) * sizeof(FieldDesc));
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
                        pasFieldDesc[nFields].nItemBytes = pasFieldDesc[nFields].nByteCount;

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
                    else if (osColumnDataType.compare("MSB_UNSIGNED_INTEGER") == 0)
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
                            osColumnFormat.size() != 0 &&
                            osColumnFormat[0] == 'F') ||
                        (pasFieldDesc[nFields].eFormat == ASCII_INTEGER &&
                            osColumnFormat.size() != 0 &&
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
                                osColumnFormat.size() != 0 &&
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
            bInObjectColumn = FALSE;
        }
        else if (nTokens == 2)
        {
            if (EQUAL(papszTokens[0], "PDS_VERSION_ID"))
            {
                CSLDestroy(papszTokens);
                papszTokens = NULL;
                while(TRUE)
                {
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    pszLine = CPLReadLine2L(fpStructure, 256, NULL);
                    CPLPopErrorHandler();
                    CPLErrorReset();
                    if (pszLine == NULL)
                        break;
                    papszTokens =
                        CSLTokenizeString2( pszLine, " =", CSLT_HONOURSTRINGS );
                    int nTokens = CSLCount(papszTokens);
                    if (nTokens == 2 &&
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
            }
            else if (EQUAL(papszTokens[0], "ROW_SUFFIX_BYTES"))
            {
                nRowBytes += atoi(papszTokens[1]);
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
                bInObjectColumn = TRUE;
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
    OGRFeature  *poFeature;

    while(TRUE)
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
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
        int i, j;
        for(i=0;i<nFieldCount;i++)
        {
            if (pasFieldDesc[i].eFormat == ASCII_REAL ||
                pasFieldDesc[i].eFormat == ASCII_INTEGER ||
                pasFieldDesc[i].eFormat == CHARACTER)
            {
                char* pchEnd = (char*) &pabyRecord[pasFieldDesc[i].nStartByte +
                                                pasFieldDesc[i].nByteCount];
                char chSaved = *pchEnd;
                *pchEnd = 0;
                poFeature->SetField(i, (const char*)(pabyRecord +
                                                    pasFieldDesc[i].nStartByte));
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
                        int* panValues = (int*)CPLMalloc(sizeof(int) * pasFieldDesc[i].nItems);
                        for(j=0;j<pasFieldDesc[i].nItems;j++)
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
                        for(j=0;j<pasFieldDesc[i].nItems;j++)
                        {
                            unsigned short sVal;
                            memcpy(&sVal, pabyRecord + pasFieldDesc[i].nStartByte + 2 * j, 2);
                            CPL_MSBPTR16(&sVal);
                            panValues[j] = sVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        unsigned short sVal;
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
                        for(j=0;j<pasFieldDesc[i].nItems;j++)
                        {
                            unsigned int nVal;
                            memcpy(&nVal, pabyRecord + pasFieldDesc[i].nStartByte + 4 * j, 4);
                            CPL_MSBPTR32(&nVal);
                            padfValues[j] = (double)nVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, padfValues);
                        CPLFree(padfValues);
                    }
                    else
                    {
                        unsigned int nVal;
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
                        for(j=0;j<pasFieldDesc[i].nItems;j++)
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
                        for(j=0;j<pasFieldDesc[i].nItems;j++)
                        {
                            short sVal;
                            memcpy(&sVal, pabyRecord + pasFieldDesc[i].nStartByte + 2 * j, 2);
                            CPL_MSBPTR16(&sVal);
                            panValues[j] = sVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        short sVal;
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
                        for(j=0;j<pasFieldDesc[i].nItems;j++)
                        {
                            int nVal;
                            memcpy(&nVal, pabyRecord + pasFieldDesc[i].nStartByte + 4 * j, 4);
                            CPL_MSBPTR32(&nVal);
                            panValues[j] = nVal;
                        }
                        poFeature->SetField(i, pasFieldDesc[i].nItems, panValues);
                        CPLFree(panValues);
                    }
                    else
                    {
                        int nVal;
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
                    for(j=0;j<pasFieldDesc[i].nItems;j++)
                    {
                        float fVal;
                        memcpy(&fVal, pabyRecord + pasFieldDesc[i].nStartByte + 4 * j, 4);
                        CPL_MSBPTR32(&fVal);
                        padfValues[j] = (double)fVal;
                    }
                    poFeature->SetField(i, pasFieldDesc[i].nItems, padfValues);
                    CPLFree(padfValues);
                }
                else
                {
                    float fVal;
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
        int nTokens = CSLCount(papszTokens);
        nTokens = MIN(nTokens, nFieldCount);
        int i;
        for(i=0;i<nTokens;i++)
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
    else if (EQUAL(pszCap,OLCRandomRead))
        return TRUE;
    else if (EQUAL(pszCap,OLCFastSetNextByIndex) &&
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
