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

#include <memory>

/************************************************************************/
/*                           LaunderCRSName()                           */
/************************************************************************/

/** Launder a CRS name to be layer name friendly. */
/* static */ std::string
OGRS101Reader::LaunderCRSName(const OGRSpatialReference &oSRS)
{
    const char *pszSRSName = oSRS.GetName();
    return CPLString(pszSRSName ? pszSRSName : "(null)")
        .replaceAll("WGS 84 + ", "")
        .replaceAll(" depth", "");
}

/************************************************************************/
/*                              ReadCSID()                              */
/************************************************************************/

/** Read the (mandatory) CRS record.
 *
 * The horizontal CRS is mandatory and WGS 84 geographic.
 * There might be 0..n vertical CRS.
 */
bool OGRS101Reader::ReadCSID(const DDFRecord *poRecord)
{

    const auto poField = poRecord->FindField(CSID_FIELD);
    if (!poField)
        return EMIT_ERROR("CSID field not found");

    // Record name
    const RecordName nRCNM =
        poRecord->GetIntSubfield(poField, RCNM_SUBFIELD, 0);
    if (nRCNM != RECORD_NAME_CRS &&
        !EMIT_ERROR_OR_WARNING("Invalid value for RCNM subfield of CSID."))
        return false;

    // Record identifier
    const int nRCID = poRecord->GetIntSubfield(poField, RCID_SUBFIELD, 0);
    // Only one CRS record expected
    if (nRCID != 1 &&
        !EMIT_ERROR_OR_WARNING("Invalid value for RCID subfield of CSID."))
        return false;

    // Number of CRS Components
    constexpr const char *NCRC_SUBFIELD = "NCRC";
    int nNCRC = poRecord->GetIntSubfield(poField, NCRC_SUBFIELD, 0);
    if (nNCRC < 1 &&
        !EMIT_ERROR_OR_WARNING("Invalid value for NCRC subfield of CSID."))
        return false;

    // Fields in that record have normally the following sequence:
    // - CSID
    // - CRSH: horizontal CRS
    // - CRSH: vertical CRS 1
    //      - CSAX: coordinate system axis
    //      - VDAT: vertical datum
    // ...
    // - CRSH: vertical CRS N
    //      - CSAX: coordinate system axis
    //      - VDAT: vertical datum
    // Establish a mapping from the CRSH instance count to the CSAX/VDAT
    // instance count

    const int nFieldCount = poRecord->GetFieldCount();
    std::vector<int> anCSAXInstanceForCRSH;
    std::vector<int> anVDATInstanceForCRSH;
    int iInstanceCRSH = -1;
    int iInstanceCSAX = -1;
    int iInstanceVDAT = -1;
    for (int i = 1; i < nFieldCount; ++i)
    {
        const char *pszFieldName =
            poRecord->GetField(i)->GetFieldDefn()->GetName();
        if (strcmp(pszFieldName, CRSH_FIELD) == 0)
        {
            ++iInstanceCRSH;
            anCSAXInstanceForCRSH.push_back(-1);
            anVDATInstanceForCRSH.push_back(-1);
        }
        else if (strcmp(pszFieldName, CSAX_FIELD) == 0)
        {
            ++iInstanceCSAX;
            if (iInstanceCRSH >= 0)
            {
                CPLAssert(static_cast<size_t>(iInstanceCRSH) <
                          anCSAXInstanceForCRSH.size());
                if (anCSAXInstanceForCRSH[iInstanceCRSH] >= 0)
                {
                    if (iInstanceCRSH != 0 &&
                        !EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("Several CSAX fields associated to CRSH "
                                       "instance of index %d.",
                                       iInstanceCRSH)))
                    {
                        return false;
                    }
                }
                else
                {
                    anCSAXInstanceForCRSH[iInstanceCRSH] = iInstanceCSAX;
                }
            }
            else
            {
                if (!EMIT_ERROR_OR_WARNING("CSAX field found before CRSH."))
                {
                    return false;
                }
            }
        }
        else if (strcmp(pszFieldName, VDAT_FIELD) == 0)
        {
            ++iInstanceVDAT;
            if (iInstanceCRSH >= 0)
            {
                CPLAssert(static_cast<size_t>(iInstanceCRSH) <
                          anVDATInstanceForCRSH.size());
                if (anVDATInstanceForCRSH[iInstanceCRSH] >= 0)
                {
                    if (!EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("Several VDAT fields associated to CRSH "
                                       "instance of index %d.",
                                       iInstanceCRSH)))
                    {
                        return false;
                    }
                }
                else
                {
                    anVDATInstanceForCRSH[iInstanceCRSH] = iInstanceVDAT;
                }
            }
            else
            {
                if (!EMIT_ERROR_OR_WARNING("VDAT field found before CRSH."))
                {
                    return false;
                }
            }
        }
        else if (!EMIT_ERROR_OR_WARNING(
                     CPLSPrintf("Unexpected field found in CRS record: %s.",
                                pszFieldName)))
        {
            return false;
        }
    }
    if (nNCRC != iInstanceCRSH + 1 &&
        !EMIT_ERROR_OR_WARNING(
            "NCRC field of CSID is not consistent with number of CRSH fields."))
    {
        return false;
    }

    if (iInstanceCRSH < 0)
    {
        return EMIT_ERROR_OR_WARNING("No CRSH field.");
    }

    nNCRC = std::clamp(nNCRC, 1, iInstanceCRSH + 1);

    // Subfields of field CRSH
    constexpr const char *CRIX_SUBFIELD = "CRIX";
    constexpr const char *CRST_SUBFIELD = "CRST";
    constexpr const char *CSTY_SUBFIELD = "CSTY";
    constexpr const char *CRNM_SUBFIELD = "CRNM";
    constexpr const char *CRSI_SUBFIELD = "CRSI";
    constexpr const char *CRSS_SUBFIELD = "CRSS";

    const auto apoCRSHFields = poRecord->GetFields(CRSH_FIELD);

    // Read horizontal CRS definition (required to be WGS 84 by S-101 spec)
    {
        CPLAssert(!apoCRSHFields.empty());
        const auto poCRSHField = apoCRSHFields[0];

        // CRS Index
        const int nCRIX =
            poRecord->GetIntSubfield(poCRSHField, CRIX_SUBFIELD, 0);
        // 1 for the horizontal CRS
        if (nCRIX != 1 && !EMIT_ERROR_OR_WARNING(
                              "Invalid value for CRIX field of CRSH idx 0."))
            return false;

        // CRS Type
        const int nCRST =
            poRecord->GetIntSubfield(poCRSHField, CRST_SUBFIELD, 0);
        constexpr int CRS_TYPE_GEOGRAPHIC_2D = 1;
        if (nCRST != CRS_TYPE_GEOGRAPHIC_2D &&
            !EMIT_ERROR_OR_WARNING(
                "Invalid value for CRST field of CRSH idx 0."))
            return false;

        // Coordinate System Type
        const int nCSTY =
            poRecord->GetIntSubfield(poCRSHField, CSTY_SUBFIELD, 0);
        constexpr int CS_TYPE_ELLIPSOIDAL = 1;
        if (nCSTY != CS_TYPE_ELLIPSOIDAL &&
            !EMIT_ERROR_OR_WARNING(
                "Invalid value for CSTY field of CRSH idx 0."))
            return false;

        // CRS Name
        const char *pszCRNM =
            poRecord->GetStringSubfield(poCRSHField, CRNM_SUBFIELD, 0);
        if (!pszCRNM || strcmp(pszCRNM, "WGS84") != 0)
        {
            if (!EMIT_ERROR_OR_WARNING(
                    "Invalid value for CRNM field of CRSH idx 0."))
                return false;
        }

        // CRS Identifier
        const char *pszCRSI =
            poRecord->GetStringSubfield(poCRSHField, CRSI_SUBFIELD, 0);
        if (!pszCRSI || strcmp(pszCRSI, "4326") != 0)
        {
            if (!EMIT_ERROR_OR_WARNING(
                    "Invalid value for CRSI field of CRSH idx 0."))
                return false;
        }

        // CRS Source
        const int nCRSS =
            poRecord->GetIntSubfield(poCRSHField, CRSS_SUBFIELD, 0);
        constexpr int CRS_SOURCE_EPSG = 2;
        if (nCRSS != CRS_SOURCE_EPSG &&
            !EMIT_ERROR_OR_WARNING(
                "Invalid value for CRSS field of CRSH idx 0."))
            return false;

        CPLAssert(!anCSAXInstanceForCRSH.empty());
        const int nCSAXIdx = anCSAXInstanceForCRSH[0];
        if (nCSAXIdx >= 0 &&
            !EMIT_ERROR_OR_WARNING(
                "Unexpected CSAX field associated to first CRSH field."))
            return false;

        CPLAssert(!anVDATInstanceForCRSH.empty());
        const int nVDATIdx = anVDATInstanceForCRSH[0];
        if (nVDATIdx >= 0 &&
            !EMIT_ERROR_OR_WARNING(
                "Unexpected VDAT field associated to first CRSH field."))
            return false;

        const int nEPSGCode = pszCRSI ? atoi(pszCRSI) : 0;
        if (nEPSGCode > 0)
        {
            OGRSpatialReference oSRSHorizontal;
            oSRSHorizontal.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (oSRSHorizontal.importFromEPSG(nEPSGCode) != OGRERR_NONE &&
                m_bStrict)
                return false;

            m_oMapSRS[nCRIX] = std::move(oSRSHorizontal);
        }
        else
        {
            return EMIT_ERROR(
                CPLSPrintf("Invalid CRS EPSG code: %d.", nEPSGCode));
        }
    }

    const auto &oSRSHorizontal = m_oMapSRS[HORIZONTAL_CRS_ID];

    // Loop through optional vertical CRS definitions
    for (int iCRSHIdx = 1; iCRSHIdx < nNCRC; ++iCRSHIdx)
    {
        const auto poCRSHField = apoCRSHFields[iCRSHIdx];

        // CRS Index
        const int nCRIX =
            poRecord->GetIntSubfield(poCRSHField, CRIX_SUBFIELD, 0);
        if (!(nCRIX >= 2 && nCRIX <= nNCRC) &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "Invalid value for CRIX field of CRSH idx %d.", iCRSHIdx)))
        {
            return false;
        }

        if (cpl::contains(m_oMapSRS, nCRIX) &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "Several CRSH field instances with same CRIX = %d.", iCRSHIdx)))
        {
            return false;
        }

        // CRS Type
        const int nCRST =
            poRecord->GetIntSubfield(poCRSHField, CRST_SUBFIELD, 0);
        constexpr int CRS_TYPE_VERTICAL = 5;
        if (nCRST != CRS_TYPE_VERTICAL &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "Invalid value for CRST field of CRSH idx %d.", iCRSHIdx)))
        {
            return false;
        }

        // Coordinate System Type
        const int nCSTY =
            poRecord->GetIntSubfield(poCRSHField, CSTY_SUBFIELD, 0);
        constexpr int CS_TYPE_VERTICAL = 3;
        if (nCSTY != CS_TYPE_VERTICAL &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "Invalid value for CSTY field of CRSH idx %d.", iCRSHIdx)))
            return false;

        // CRS Name
        const char *pszCRNM =
            poRecord->GetStringSubfield(poCRSHField, CRNM_SUBFIELD, 0);
        if (!pszCRNM &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "No value for CRNM field of CRSH idx %d.", iCRSHIdx)))
            return false;
        if (!pszCRNM)
            pszCRNM = "(null)";

        // CRS Source
        const int nCRSS =
            poRecord->GetIntSubfield(poCRSHField, CRSS_SUBFIELD, 0);
        constexpr int CRS_SOURCE_NOT_APPLICABLE = 255;
        if (nCRSS != CRS_SOURCE_NOT_APPLICABLE &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "Invalid value for CRSS field of CRSH idx %d.", iCRSHIdx)))
            return false;

        // Read associated CSAX field
        CPLAssert(static_cast<size_t>(iCRSHIdx) < anCSAXInstanceForCRSH.size());
        const int nCSAXIdx = anCSAXInstanceForCRSH[iCRSHIdx];
        if (nCSAXIdx < 0 && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "No CSAX field for CRSH idx %d.", iCRSHIdx)))
        {
            return false;
        }

        if (nCSAXIdx >= 0)
        {
            const auto poCSAXField = poRecord->FindField(CSAX_FIELD, nCSAXIdx);

            // Axis type
            const int nAXTY = poRecord->GetIntSubfield(poCSAXField, "AXTY", 0);
            constexpr int AXIS_TYPE_GRAVITY_RELATED_DEPTH = 12;
            if (nAXTY != AXIS_TYPE_GRAVITY_RELATED_DEPTH &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Invalid value for AXTY field of CSAX idx %d.", nCSAXIdx)))
            {
                return false;
            }

            //  Axis Unit of Measure
            const int nAXUM = poRecord->GetIntSubfield(poCSAXField, "AXUM", 0);
            constexpr int UOM_METRE = 4;
            if (nAXUM != UOM_METRE &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Invalid value for AXUM field of CSAX idx %d.", nCSAXIdx)))
            {
                return false;
            }
        }

        // Read associated VDAT field
        CPLAssert(static_cast<size_t>(iCRSHIdx) < anVDATInstanceForCRSH.size());
        const int nVDATIdx = anVDATInstanceForCRSH[iCRSHIdx];
        if (nVDATIdx < 0 && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "No VDAT field for CRSH idx %d.", iCRSHIdx)))
        {
            return false;
        }

        const char *pszDTNM = nullptr;
        if (nVDATIdx >= 0)
        {
            const auto poVDATField = poRecord->FindField(VDAT_FIELD, nVDATIdx);

            // Vertical Datum Name
            pszDTNM = poRecord->GetStringSubfield(poVDATField, "DTNM", 0);
            if (!pszDTNM &&
                !EMIT_ERROR(CPLSPrintf(
                    "No value for DTNM field of VDAT idx %d.", nVDATIdx)))
                return false;

            // Vertical Datum Identifier
            const char *pszDTID =
                poRecord->GetStringSubfield(poVDATField, "DTID", 0);
            if (!pszDTID &&
                !EMIT_ERROR(CPLSPrintf(
                    "No value for DTID field of VDAT idx %d.", nVDATIdx)))
                return false;

            // Datum Source
            const int nDTSR = poRecord->GetIntSubfield(poVDATField, "DTSR", 0);
            constexpr int SOURCE_FEATURE_CATALG = 2;
            if (nDTSR != SOURCE_FEATURE_CATALG &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Invalid value for DTSR field of VDAT idx %d.", nVDATIdx)))
                return false;
        }
        if (!pszDTNM)
            pszDTNM = "(null)";

        // WKT CRS constraints
        if (strchr(pszCRNM, '"'))
        {
            if (!EMIT_ERROR_OR_WARNING(
                    "Double quote not allowed in vertical CRS name."))
                return false;
            pszCRNM = "unknown";
        }
        if (strchr(pszDTNM, '"'))
        {
            if (!EMIT_ERROR_OR_WARNING(
                    "Double quote not allowed in vertical datum name."))
                return false;
            pszDTNM = "unknown";
        }

        OGRSpatialReference oSRSVertical;
        const std::string osWKT = CPLSPrintf(
            "VERTCRS[\"%s depth\",VDATUM[\"%s\"],CS[vertical,1],"
            "AXIS[\"gravity-related depth (D)\",down,LENGTHUNIT[\"metre\",1]]]",
            pszCRNM, pszDTNM);
        if (oSRSVertical.importFromWkt(osWKT.c_str()) != OGRERR_NONE)
        {
            if (m_bStrict)
                return false;
            continue;
        }

        OGRSpatialReference oCompoundCRS;
        oCompoundCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (oCompoundCRS.SetCompoundCS(std::string(oSRSHorizontal.GetName())
                                           .append(" + ")
                                           .append(oSRSVertical.GetName())
                                           .c_str(),
                                       &oSRSHorizontal,
                                       &oSRSVertical) == OGRERR_NONE)
        {
            m_oMapSRS[nCRIX] = std::move(oCompoundCRS);
        }
        else if (m_bStrict)
        {
            return false;
        }
    }

    return true;
}
