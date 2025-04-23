/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB OGR result layer.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_fgdb.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "FGdbUtils.h"

using std::string;
using std::wstring;

/************************************************************************/
/*                         FGdbResultLayer()                            */
/************************************************************************/
FGdbResultLayer::FGdbResultLayer(FGdbDataSource *pParentDataSource,
                                 const char *pszSQL, EnumRows *pEnumRows)
{
    m_pFeatureDefn = new OGRFeatureDefn("result");
    SetDescription(m_pFeatureDefn->GetName());
    m_pFeatureDefn->Reference();
    m_pEnumRows = pEnumRows;
    m_pDS = pParentDataSource;
    osSQL = pszSQL;

    m_suppressColumnMappingError = false;

    FieldInfo fieldInfo;
    m_pEnumRows->GetFieldInformation(fieldInfo);

    int fieldCount;
    fieldInfo.GetFieldCount(fieldCount);
    for (int i = 0; i < fieldCount; i++)
    {
        FieldType fieldType;
        string strFieldType;
        wstring fieldName;
        fieldInfo.GetFieldType(i, fieldType);
        fieldInfo.GetFieldName(i, fieldName);

        OGRFieldType eType = OFTString;
        int bSkip = FALSE;

        switch (fieldType)
        {
            case fieldTypeSmallInteger:
            case fieldTypeInteger:
                eType = OFTInteger;
                break;

            case fieldTypeSingle:
                eType = OFTReal;
                strFieldType = "esriFieldTypeSingle";
                break;

            case fieldTypeDouble:
                eType = OFTReal;
                break;

            case fieldTypeString:
                eType = OFTString;
                break;

            case fieldTypeDate:
                eType = OFTDateTime;
                break;

            case fieldTypeOID:
                bSkip = TRUE;
                break;

            case fieldTypeGeometry:
                bSkip = TRUE;
                break;

            case fieldTypeBlob:
                eType = OFTBinary;
                break;

            case fieldTypeRaster:
                bSkip = TRUE;
                break;

            case fieldTypeGUID:
                break;

            case fieldTypeGlobalID:
                break;

            case fieldTypeXML:
                break;

            default:
                CPLAssert(false);
                break;
        }

        if (!bSkip)
        {
            OGRFieldDefn oFieldDefn(WStringToString(fieldName).c_str(), eType);
            // cppcheck-suppress danglingTemporaryLifetime
            m_pFeatureDefn->AddFieldDefn(&oFieldDefn);

            m_vOGRFieldToESRIField.push_back(fieldName);
            m_vOGRFieldToESRIFieldType.push_back(strFieldType);
        }
    }
}

/************************************************************************/
/*                         ~FGdbResultLayer()                           */
/************************************************************************/

FGdbResultLayer::~FGdbResultLayer()
{
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void FGdbResultLayer::ResetReading()
{
    m_pEnumRows->Close();
    long hr;
    if (FAILED(hr = m_pDS->GetGDB()->ExecuteSQL(StringToWString(osSQL), true,
                                                *m_pEnumRows)))
    {
        GDBErr(hr, CPLSPrintf("Failed at executing '%s'", osSQL.c_str()));
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int FGdbResultLayer::TestCapability(const char *)
{
    return FALSE;
}
