/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101Reader
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"
#include "ogrs101readerconstants.h"
#include "ogrs101readerconstants.h"

#include <memory>

/************************************************************************/
/*                  CreateInformationTypeFeatureDefn()                  */
/************************************************************************/

/** Create the feature definition for the informationType layer.
 */
bool OGRS101Reader::CreateInformationTypeFeatureDefn()
{
    if (m_oInformationTypeRecordIndex.GetCount() != 0)
    {
        m_poFeatureDefnInformationType =
            OGRFeatureDefnRefCountedPtr::makeInstance("informationType");
        m_poFeatureDefnInformationType->SetGeomType(wkbNone);
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_ID, OFTInteger);
            m_poFeatureDefnInformationType->AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_VERSION, OFTInteger);
            m_poFeatureDefnInformationType->AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_INFORMATION_TYPE, OFTString);
            m_poFeatureDefnInformationType->AddFieldDefn(&oFieldDefn);
        }

        // Scan the properties of the information type record
        if (!InferFeatureDefn(
                m_oInformationTypeRecordIndex, IRID_FIELD, ATTR_FIELD,
                /* anRecordIdx = */ {}, *m_poFeatureDefnInformationType,
                m_oMapFieldDomains))
        {
            return false;
        }

        // Scan the properties of the association of one information record to
        // another related information type record.
        if (!InferFeatureDefn(
                m_oInformationTypeRecordIndex, IRID_FIELD, INAS_FIELD,
                /* anRecordIdx = */ {}, *m_poFeatureDefnInformationType,
                m_oMapFieldDomains))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                     FillFeatureInformationType()                     */
/************************************************************************/

/** Fill the content of the provided feature from the identified record
 * of m_oInformationTypeRecordIndex.
 */
bool OGRS101Reader::FillFeatureInformationType(const DDFRecordIndex &oIndex,
                                               int iRecord,
                                               OGRFeature &oFeature) const
{
    const auto poRecord = oIndex.GetByIndex(iRecord);
    CPLAssert(poRecord);

    const InfoTypeCode nNITC(
        poRecord->GetIntSubfield(IRID_FIELD, 0, NITC_SUBFIELD, 0));
    const auto oIter = m_informationTypeCodes.find(nNITC);
    if (oIter != m_informationTypeCodes.end())
    {
        oFeature.SetField(OGR_FIELD_NAME_INFORMATION_TYPE,
                          oIter->second.c_str());
    }
    else if (!EMIT_ERROR_OR_WARNING(
                 CPLSPrintf("Unknown value %d for NITC subfield of IRID field.",
                            static_cast<int>(nNITC))))
    {
        return false;
    }
    else
    {
        oFeature.SetField(
            OGR_FIELD_NAME_INFORMATION_TYPE,
            CPLSPrintf("informationTypeCode%d", static_cast<int>(nNITC)));
    }

    // An IRID record might have a ATTR field, a INAS field (thus pointing
    // to another IRID record), both or none...
    return FillFeatureAttributes(oIndex, iRecord, ATTR_FIELD, oFeature) &&
           FillFeatureAttributes(oIndex, iRecord, INAS_FIELD, oFeature) &&
           FillFeatureWithNonAttrAssocSubfields(poRecord, iRecord, INAS_FIELD,
                                                oFeature);
}
