/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef FILEGDB_FIELDDOMAIN_H
#define FILEGDB_FIELDDOMAIN_H

#include "cpl_minixml.h"
#include "filegdb_gdbtoogrfieldtype.h"

static std::unique_ptr<OGRFieldDomain> ParseXMLFieldDomainDef(const std::string& domainDef)
{
    CPLXMLTreeCloser oTree(CPLParseXMLString(domainDef.c_str()));
    if( !oTree.get() )
    {
        return nullptr;
    }
    const CPLXMLNode* psDomain = CPLGetXMLNode(oTree.get(), "=esri:Domain");
    if( psDomain == nullptr )
    {
        // esri: namespace prefix omitted when called from the FileGDB driver
        psDomain = CPLGetXMLNode(oTree.get(), "=Domain");
    }
    bool bIsCodedValueDomain = false;
    if( psDomain == nullptr )
    {
        // Also sometimes found...
        psDomain = CPLGetXMLNode(oTree.get(), "=esri:CodedValueDomain");
        if( psDomain )
            bIsCodedValueDomain = true;
    }
    if( psDomain == nullptr )
    {
        // Also sometimes found...
        psDomain = CPLGetXMLNode(oTree.get(), "=typens:GPCodedValueDomain2");
        if( psDomain )
            bIsCodedValueDomain = true;
    }
    if( psDomain == nullptr )
    {
        // Also sometimes found...
        psDomain = CPLGetXMLNode(oTree.get(), "=GPCodedValueDomain2");
        if( psDomain )
            bIsCodedValueDomain = true;
    }
    bool bIsRangeDomain = false;
    if( psDomain == nullptr )
    {
        // Also sometimes found...
        psDomain = CPLGetXMLNode(oTree.get(), "=esri:RangeDomain");
        if( psDomain )
            bIsRangeDomain = true;
    }
    if( psDomain == nullptr )
    {
        // Also sometimes found...
        psDomain = CPLGetXMLNode(oTree.get(), "=typens:GPRangeDomain2");
        if( psDomain )
            bIsRangeDomain = true;
    }
    if( psDomain == nullptr )
    {
        // Also sometimes found...
        psDomain = CPLGetXMLNode(oTree.get(), "=GPRangeDomain2");
        if( psDomain )
            bIsRangeDomain = true;
    }
    if( psDomain == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find root 'Domain' node");
        return nullptr;
    }
    const char* pszType = CPLGetXMLValue(psDomain, "xsi:type", "");
    const char* pszName = CPLGetXMLValue(psDomain, "DomainName", "");
    const char* pszDescription = CPLGetXMLValue(psDomain, "Description", "");
    const char* pszFieldType = CPLGetXMLValue(psDomain, "FieldType", "");
    OGRFieldType eFieldType = OFTString;
    OGRFieldSubType eSubType = OFSTNone;
    if( !GDBToOGRFieldType(pszFieldType, &eFieldType, &eSubType) )
    {
        return nullptr;
    }

    std::unique_ptr<OGRFieldDomain> domain;
    if( bIsCodedValueDomain || strcmp(pszType, "esri:CodedValueDomain") == 0 )
    {
        const CPLXMLNode* psCodedValues = CPLGetXMLNode(psDomain, "CodedValues");
        if( psCodedValues == nullptr )
        {
            return nullptr;
        }
        std::vector<OGRCodedValue> asValues;
        for( const CPLXMLNode* psIter = psCodedValues->psChild;
                                        psIter; psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "CodedValue") == 0 )
            {
                OGRCodedValue cv;
                cv.pszCode = CPLStrdup(CPLGetXMLValue(psIter, "Code", ""));
                cv.pszValue = CPLStrdup(CPLGetXMLValue(psIter, "Name", ""));
                asValues.emplace_back(cv);
            }
        }
        domain.reset(
            new OGRCodedFieldDomain(pszName,
                                       pszDescription,
                                       eFieldType,
                                       eSubType,
                                       std::move(asValues)));
    }
    else if( bIsRangeDomain || strcmp(pszType, "esri:RangeDomain") == 0 )
    {
        if( eFieldType != OFTInteger &&
            eFieldType != OFTInteger64 &&
            eFieldType != OFTReal )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported field type for range domain: %s",
                     pszFieldType);
            return nullptr;
        }
        const char* pszMinValue = CPLGetXMLValue(psDomain, "MinValue", "");
        const char* pszMaxValue = CPLGetXMLValue(psDomain, "MaxValue", "");
        OGRField sMin;
        OGRField sMax;
        OGR_RawField_SetUnset(&sMin);
        OGR_RawField_SetUnset(&sMax);
        if( eFieldType == OFTInteger )
        {
            sMin.Integer = atoi(pszMinValue);
            sMax.Integer = atoi(pszMaxValue);
        }
        else if( eFieldType == OFTInteger64 )
        {
            sMin.Integer64 = CPLAtoGIntBig(pszMinValue);
            sMax.Integer64 = CPLAtoGIntBig(pszMaxValue);
        }
        else if( eFieldType == OFTReal )
        {
            sMin.Real = CPLAtof(pszMinValue);
            sMax.Real = CPLAtof(pszMaxValue);
        }
        domain.reset(
            new OGRRangeFieldDomain(pszName,
                                       pszDescription,
                                       eFieldType,
                                       eSubType,
                                       sMin, true,
                                       sMax, true));
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported type of File Geodatabase domain: %s",
                 pszType);
        return nullptr;
    }

    const char* pszMergePolicy = CPLGetXMLValue(psDomain, "MergePolicy",
                                                "esriMPTDefaultValue");
    if( EQUAL(pszMergePolicy, "esriMPTDefaultValue") )
    {
        domain->SetMergePolicy(OFDMP_DEFAULT_VALUE);
    }
    else if( EQUAL(pszMergePolicy, "esriMPTSumValues") )
    {
        domain->SetMergePolicy(OFDMP_SUM);
    }
    else if( EQUAL(pszMergePolicy, "esriMPTAreaWeighted") )
    {
        domain->SetMergePolicy(OFDMP_GEOMETRY_WEIGHTED);
    }

    const char* pszSplitPolicy = CPLGetXMLValue(psDomain, "SplitPolicy",
                                                "esriSPTDefaultValue");
    if( EQUAL(pszSplitPolicy, "esriSPTDefaultValue") )
    {
        domain->SetSplitPolicy(OFDSP_DEFAULT_VALUE);
    }
    else if( EQUAL(pszSplitPolicy, "esriSPTDuplicate") )
    {
        domain->SetSplitPolicy(OFDSP_DUPLICATE);
    }
    else if( EQUAL(pszSplitPolicy, "esriSPTGeometryRatio") )
    {
        domain->SetSplitPolicy(OFDSP_GEOMETRY_RATIO);
    }

    return domain;
}

#endif // FILEGDB_FIELDDOMAIN_H
