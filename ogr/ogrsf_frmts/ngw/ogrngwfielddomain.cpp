/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2025, NextGIS <info@nextgis.com>
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/

#include "ogr_ngw.h"

/*
 * OGRNGWCodedFieldDomain()
 */
OGRNGWCodedFieldDomain::OGRNGWCodedFieldDomain(
    const CPLJSONObject &oResourceJsonObject)
{
    nResourceID = oResourceJsonObject.GetLong("resource/id", 0);
    nResourceParentID = oResourceJsonObject.GetLong("resource/parent/id", 0);
    osCreationDate = oResourceJsonObject.GetString("resource/creation_date");
    osDisplayName = oResourceJsonObject.GetString("resource/display_name");
    osKeyName = oResourceJsonObject.GetString("resource/keyname");
    osDescription = oResourceJsonObject.GetString("resource/description");

    std::set<GIntBig> keys;

    bool bOnlyDigitsKeys = true;
    std::vector<OGRCodedValue> aoDom1, aoDom2, aoDom3;
    auto oItems = oResourceJsonObject.GetObj("lookup_table/items");
    for (const auto &oItem : oItems.GetChildren())
    {
        if (bOnlyDigitsKeys)
        {
            auto nNum = CPLAtoGIntBig(oItem.GetName().c_str());
            if (keys.find(nNum) != keys.end())
            {
                bOnlyDigitsKeys = false;
            }
            else
            {
                keys.insert(nNum);
            }
        }

        OGRCodedValue cv1;
        cv1.pszCode = CPLStrdup(oItem.GetName().c_str());
        cv1.pszValue = CPLStrdup(oItem.ToString().c_str());
        aoDom1.emplace_back(cv1);

        OGRCodedValue cv2;
        cv2.pszCode = CPLStrdup(oItem.GetName().c_str());
        cv2.pszValue = CPLStrdup(oItem.ToString().c_str());
        aoDom2.emplace_back(cv2);

        OGRCodedValue cv3;
        cv3.pszCode = CPLStrdup(oItem.GetName().c_str());
        cv3.pszValue = CPLStrdup(oItem.ToString().c_str());
        aoDom3.emplace_back(cv3);
    }

    auto osName = osDisplayName;
    auto oDom = std::make_shared<OGRCodedFieldDomain>(
        osName, osDescription, OFTString, OFSTNone, std::move(aoDom1));
    apDomains[0] = std::move(oDom);

    if (bOnlyDigitsKeys)
    {
        osName = osDisplayName + " (number)";
        oDom = std::make_shared<OGRCodedFieldDomain>(
            osName, osDescription, OFTInteger, OFSTNone, std::move(aoDom2));
        apDomains[1] = std::move(oDom);

        osName = osDisplayName + " (bigint)";
        oDom = std::make_shared<OGRCodedFieldDomain>(
            osName, osDescription, OFTInteger64, OFSTNone, std::move(aoDom3));
        apDomains[2] = std::move(oDom);
    }
}

/*
 * ToFieldDomain()
 */
const OGRFieldDomain *
OGRNGWCodedFieldDomain::ToFieldDomain(OGRFieldType eFieldType) const
{
    for (size_t i = 0; i < apDomains.size(); ++i)
    {
        if (apDomains[i] && apDomains[i]->GetFieldType() == eFieldType)
        {
            return apDomains[i].get();
        }
    }
    return nullptr;
}

/*
 * GetID()
 */
GIntBig OGRNGWCodedFieldDomain::GetID() const
{
    return nResourceID;
}

/*
 * GetDomainsNames()
 */
std::string OGRNGWCodedFieldDomain::GetDomainsNames() const
{
    std::string osOut;
    for (size_t i = 0; i < apDomains.size(); ++i)
    {
        if (apDomains[i])
        {
            if (osOut.empty())
            {
                osOut = apDomains[i]->GetName();
            }
            else
            {
                osOut += ", " + apDomains[i]->GetName();
            }
        }
    }
    return osOut;
}

/**
 * HasDomainName()
 */
bool OGRNGWCodedFieldDomain::HasDomainName(const std::string &osName) const
{
    if (osName.empty())
    {
        return false;
    }
    for (size_t i = 0; i < apDomains.size(); ++i)
    {
        if (apDomains[i] && apDomains[i]->GetName() == osName)
        {
            return true;
        }
    }
    return false;
}