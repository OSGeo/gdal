/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonLayer class.
 * Author:   Abel Pau
 ******************************************************************************
 * Copyright (c) 2024, Xavier Pons
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
#include "ogrmiramon.h"

#include "mm_gdal_functions.h"  // For MMCreateExtendedDBFIndex()
#include "mm_rdlayr.h"          // For MMInitLayerToRead()
#include <algorithm>            // For std::clamp()
#include <string>               // For std::string
#include <algorithm>            // For std::max

/****************************************************************************/
/*                            OGRMiraMonLayer()                             */
/****************************************************************************/
OGRMiraMonLayer::OGRMiraMonLayer(GDALDataset *poDS, const char *pszFilename,
                                 VSILFILE *fp, const OGRSpatialReference *poSRS,
                                 int bUpdateIn, CSLConstList papszOpenOptions,
                                 struct MiraMonVectMapInfo *MMMap)
    : m_poDS(poDS), m_poSRS(nullptr), m_poFeatureDefn(nullptr), m_iNextFID(0),
      phMiraMonLayer(nullptr), hMiraMonLayerPNT(), hMiraMonLayerARC(),
      hMiraMonLayerPOL(), hMiraMonLayerReadOrNonGeom(), hMMFeature(),
      m_bUpdate(CPL_TO_BOOL(bUpdateIn)),
      m_fp(fp ? fp : VSIFOpenL(pszFilename, (bUpdateIn ? "r+" : "r"))),
      padfValues(nullptr), pnInt64Values(nullptr), bValidFile(false)
{

    CPLDebugOnly("MiraMon", "Creating/Opening MiraMon layer...");
    /* -------------------------------------------------------------------- */
    /*      Create the feature definition                                   */
    /* -------------------------------------------------------------------- */
    m_poFeatureDefn = new OGRFeatureDefn(CPLGetBasename(pszFilename));
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->Reference();

    if (m_bUpdate)
    {
        /* ---------------------------------------------------------------- */
        /*      Establish the version to use                                */
        /* ---------------------------------------------------------------- */
        const char *pszVersion = CSLFetchNameValue(papszOpenOptions, "Version");
        int nMMVersion;

        if (pszVersion)
        {
            if (EQUAL(pszVersion, "V1.1"))
                nMMVersion = MM_32BITS_VERSION;
            else if (EQUAL(pszVersion, "V2.0") ||
                     EQUAL(pszVersion, "last_version"))
                nMMVersion = MM_64BITS_VERSION;
            else
                nMMVersion = MM_32BITS_VERSION;  // Default
        }
        else
            nMMVersion = MM_32BITS_VERSION;  // Default

        /* ---------------------------------------------------------------- */
        /*      Establish the charset of the .dbf files                     */
        /* ---------------------------------------------------------------- */
        const char *pszdbfEncoding =
            CSLFetchNameValue(papszOpenOptions, "DBFEncoding");
        char nMMRecode;

        if (pszdbfEncoding)
        {
            if (EQUAL(pszdbfEncoding, "UTF8"))
                nMMRecode = MM_RECODE_UTF8;
            else  //if (EQUAL(pszdbfEncoding, "ANSI"))
                nMMRecode = MM_RECODE_ANSI;
        }
        else
            nMMRecode = MM_RECODE_ANSI;  // Default

        /* ----------------------------------------------------------------- */
        /*   Establish the descriptors language when                         */
        /*   creating .rel files                                             */
        /* ----------------------------------------------------------------- */
        const char *pszLanguage =
            CSLFetchNameValue(papszOpenOptions, "CreationLanguage");
        char nMMLanguage;

        if (pszLanguage)
        {
            if (EQUAL(pszLanguage, "CAT"))
                nMMLanguage = MM_CAT_LANGUAGE;
            else if (EQUAL(pszLanguage, "SPA"))
                nMMLanguage = MM_SPA_LANGUAGE;
            else
                nMMLanguage = MM_ENG_LANGUAGE;
        }
        else
            nMMLanguage = MM_DEF_LANGUAGE;  // Default

        /* ---------------------------------------------------------------- */
        /*      Preparing to write the layer                                */
        /* ---------------------------------------------------------------- */
        // Init the feature (memory, num,...)
        if (MMInitFeature(&hMMFeature))
        {
            bValidFile = false;
            return;
        }

        // Init the Layers (not in disk, only in memory until
        // the first element is read)
        CPLDebugOnly("MiraMon", "Initializing MiraMon points layer...");
        if (MMInitLayer(&hMiraMonLayerPNT, pszFilename, nMMVersion, nMMRecode,
                        nMMLanguage, nullptr, MM_WRITING_MODE, MMMap))
        {
            bValidFile = false;
            return;
        }
        hMiraMonLayerPNT.bIsBeenInit = 0;

        CPLDebugOnly("MiraMon", "Initializing MiraMon arcs layer...");
        if (MMInitLayer(&hMiraMonLayerARC, pszFilename, nMMVersion, nMMRecode,
                        nMMLanguage, nullptr, MM_WRITING_MODE, MMMap))
        {
            bValidFile = false;
            return;
        }
        hMiraMonLayerARC.bIsBeenInit = 0;

        CPLDebugOnly("MiraMon", "Initializing MiraMon polygons layer...");
        if (MMInitLayer(&hMiraMonLayerPOL, pszFilename, nMMVersion, nMMRecode,
                        nMMLanguage, nullptr, MM_WRITING_MODE, MMMap))
        {
            bValidFile = false;
            return;
        }
        hMiraMonLayerPOL.bIsBeenInit = 0;

        // Just in case that there is no geometry but some other
        // information to get. A DBF will be generated
        CPLDebugOnly("MiraMon", "Initializing MiraMon only-ext-DBF layer...");
        if (MMInitLayer(&hMiraMonLayerReadOrNonGeom, pszFilename, nMMVersion,
                        nMMRecode, nMMLanguage, nullptr, MM_WRITING_MODE,
                        nullptr))
        {
            bValidFile = false;
            return;
        }
        hMiraMonLayerPOL.bIsBeenInit = 0;

        // This helps the map to be created
        //GetLayerDefn()->SetName(hMiraMonLayerPNT.pszSrcLayerName);
        m_poFeatureDefn->SetName(hMiraMonLayerPNT.pszSrcLayerName);

        // Saving the HRS in the layer structure
        if (poSRS)
        {
            const char *pszAuthorityName = poSRS->GetAuthorityName(nullptr);
            const char *pszAuthorityCode = poSRS->GetAuthorityCode(nullptr);

            if (pszAuthorityName && pszAuthorityCode &&
                EQUAL(pszAuthorityName, "EPSG"))
            {
                CPLDebugOnly("MiraMon", "Setting EPSG code %s",
                             pszAuthorityCode);
                hMiraMonLayerPNT.pSRS = CPLStrdup(pszAuthorityCode);
                hMiraMonLayerARC.pSRS = CPLStrdup(pszAuthorityCode);
                hMiraMonLayerPOL.pSRS = CPLStrdup(pszAuthorityCode);
            }
            // In the DBF, there are some reserved fields that need to
            // know if the layer is geographic or not to write the
            // precision (they are real)
            if (poSRS->IsGeographic())
            {
                hMiraMonLayerPNT.nSRSType = hMiraMonLayerARC.nSRSType =
                    hMiraMonLayerPOL.nSRSType = MM_SRS_LAYER_IS_GEOGRAPHIC_TYPE;
            }
            else
            {
                hMiraMonLayerPNT.nSRSType = hMiraMonLayerARC.nSRSType =
                    hMiraMonLayerPOL.nSRSType = MM_SRS_LAYER_IS_PROJECTED_TYPE;
            }
        }
        else
        {
            hMiraMonLayerPNT.nSRSType = hMiraMonLayerARC.nSRSType =
                hMiraMonLayerPOL.nSRSType = MM_SRS_LAYER_IS_UNKNOWN_TYPE;
        }
    }
    else
    {
        if (m_fp == nullptr)
        {
            bValidFile = false;
            return;
        }

        /* ------------------------------------------------------------------*/
        /*      Read the header.                                             */
        /* ------------------------------------------------------------------*/
        int nMMLayerVersion;

        if (MMInitLayerToRead(&hMiraMonLayerReadOrNonGeom, m_fp, pszFilename))
        {
            phMiraMonLayer = &hMiraMonLayerReadOrNonGeom;
            bValidFile = false;
            return;
        }
        phMiraMonLayer = &hMiraMonLayerReadOrNonGeom;

        nMMLayerVersion = MMGetVectorVersion(&phMiraMonLayer->TopHeader);
        if (nMMLayerVersion == MM_UNKNOWN_VERSION)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "MiraMon version file unknown.");
            bValidFile = false;
            return;
        }
        if (phMiraMonLayer->bIsPoint)
        {
            if (phMiraMonLayer->TopHeader.bIs3d)
                m_poFeatureDefn->SetGeomType(wkbPoint25D);
            else
                m_poFeatureDefn->SetGeomType(wkbPoint);
        }
        else if (phMiraMonLayer->bIsArc && !phMiraMonLayer->bIsPolygon)
        {
            if (phMiraMonLayer->TopHeader.bIs3d)
                m_poFeatureDefn->SetGeomType(wkbLineString25D);
            else
                m_poFeatureDefn->SetGeomType(wkbLineString);
        }
        else if (phMiraMonLayer->bIsPolygon)
        {
            // 3D
            if (phMiraMonLayer->TopHeader.bIs3d)
            {
                if (phMiraMonLayer->TopHeader.bIsMultipolygon)
                    m_poFeatureDefn->SetGeomType(wkbMultiPolygon25D);
                else
                    m_poFeatureDefn->SetGeomType(wkbPolygon25D);
            }
            else
            {
                if (phMiraMonLayer->TopHeader.bIsMultipolygon)
                    m_poFeatureDefn->SetGeomType(wkbMultiPolygon);
                else
                    m_poFeatureDefn->SetGeomType(wkbPolygon);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "MiraMon file type not supported.");
            bValidFile = false;
            return;
        }

        if (phMiraMonLayer->TopHeader.bIs3d)
        {
            const char *szHeight =
                CSLFetchNameValue(papszOpenOptions, "Height");
            if (szHeight)
            {
                if (EQUAL(szHeight, "Highest"))
                    phMiraMonLayer->nSelectCoordz = MM_SELECT_HIGHEST_COORDZ;
                else if (EQUAL(szHeight, "Lowest"))
                    phMiraMonLayer->nSelectCoordz = MM_SELECT_LOWEST_COORDZ;
                else
                    phMiraMonLayer->nSelectCoordz = MM_SELECT_FIRST_COORDZ;
            }
            else
                phMiraMonLayer->nSelectCoordz = MM_SELECT_FIRST_COORDZ;
        }

        /* ------------------------------------------------------------ */
        /*   Establish the descriptors language when                    */
        /*   opening .rel files                                        */
        /* ------------------------------------------------------------ */
        const char *pszLanguage =
            CSLFetchNameValue(papszOpenOptions, "OpenLanguage");

        if (pszLanguage)
        {
            if (EQUAL(pszLanguage, "CAT"))
                phMiraMonLayer->nMMLanguage = MM_CAT_LANGUAGE;
            else if (EQUAL(pszLanguage, "SPA"))
                phMiraMonLayer->nMMLanguage = MM_SPA_LANGUAGE;
            else
                phMiraMonLayer->nMMLanguage = MM_ENG_LANGUAGE;
        }
        else
            phMiraMonLayer->nMMLanguage = MM_DEF_LANGUAGE;  // Default

        if (phMiraMonLayer->nSRS_EPSG != 0)
        {
            m_poSRS = new OGRSpatialReference();
            m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (m_poSRS->importFromEPSG(phMiraMonLayer->nSRS_EPSG) !=
                OGRERR_NONE)
            {
                delete m_poSRS;
                m_poSRS = nullptr;
            }
            else
                m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poSRS);
        }

        // If there is associated information
        if (phMiraMonLayer->pMMBDXP)
        {
            if (!phMiraMonLayer->pMMBDXP->pfDataBase)
            {
                if ((phMiraMonLayer->pMMBDXP->pfDataBase = fopen_function(
                         phMiraMonLayer->pMMBDXP->szFileName, "r")) == nullptr)
                {
                    CPLDebugOnly("MiraMon", "File '%s' cannot be opened.",
                                 phMiraMonLayer->pMMBDXP->szFileName);
                    bValidFile = false;
                    return;
                }

                if (phMiraMonLayer->pMMBDXP->nFields == 0)
                {
                    // TODO: is this correct? At least this prevents a
                    // nullptr dereference of phMiraMonLayer->pMMBDXP->pField
                    // below
                    CPLDebug("MiraMon",
                             "phMiraMonLayer->pMMBDXP->nFields == 0");
                    bValidFile = false;
                    return;
                }

                // First time we open the extended DBF we create an index
                // to fastly find all non geometrical features.
                phMiraMonLayer->pMultRecordIndex = MMCreateExtendedDBFIndex(
                    phMiraMonLayer->pMMBDXP->pfDataBase,
                    phMiraMonLayer->pMMBDXP->nRecords,
                    phMiraMonLayer->pMMBDXP->FirstRecordOffset,
                    phMiraMonLayer->pMMBDXP->BytesPerRecord,
                    phMiraMonLayer->pMMBDXP
                        ->pField[phMiraMonLayer->pMMBDXP->IdGraficField]
                        .AccumulatedBytes,
                    phMiraMonLayer->pMMBDXP
                        ->pField[phMiraMonLayer->pMMBDXP->IdGraficField]
                        .BytesPerField,
                    &phMiraMonLayer->isListField, &phMiraMonLayer->nMaxN);

                // Creation of maximum number needed for processing
                // multiple records
                if (phMiraMonLayer->pMultRecordIndex)
                {
                    padfValues = static_cast<double *>(CPLCalloc(
                        (size_t)phMiraMonLayer->nMaxN, sizeof(*padfValues)));

                    pnInt64Values = static_cast<GInt64 *>(CPLCalloc(
                        (size_t)phMiraMonLayer->nMaxN, sizeof(*pnInt64Values)));
                }

                phMiraMonLayer->iMultiRecord =
                    MM_MULTIRECORD_NO_MULTIRECORD;  // No option iMultiRecord
                const char *szMultiRecord =
                    CSLFetchNameValue(papszOpenOptions, "MultiRecordIndex");
                if (phMiraMonLayer->isListField && szMultiRecord)
                {
                    if (EQUAL(szMultiRecord, "Last"))
                        phMiraMonLayer->iMultiRecord = MM_MULTIRECORD_LAST;
                    else if (EQUAL(szMultiRecord, "JSON"))
                        phMiraMonLayer->iMultiRecord = MM_MULTIRECORD_JSON;
                    else
                        phMiraMonLayer->iMultiRecord = atoi(szMultiRecord);
                }
            }

            for (MM_EXT_DBF_N_FIELDS nIField = 0;
                 nIField < phMiraMonLayer->pMMBDXP->nFields; nIField++)
            {
                OGRFieldDefn oField("", OFTString);
                oField.SetName(
                    phMiraMonLayer->pMMBDXP->pField[nIField].FieldName);

                oField.SetAlternativeName(
                    phMiraMonLayer->pMMBDXP->pField[nIField]
                        .FieldDescription[phMiraMonLayer->nMMLanguage <
                                                  MM_NUM_IDIOMES_MD_MULTIDIOMA
                                              ? phMiraMonLayer->nMMLanguage
                                              : 0]);

                if (phMiraMonLayer->pMMBDXP->pField[nIField].FieldType == 'C' ||
                    phMiraMonLayer->pMMBDXP->pField[nIField].FieldType == 'L')
                {
                    // It's a list?
                    if (phMiraMonLayer->iMultiRecord ==
                        MM_MULTIRECORD_NO_MULTIRECORD)
                    {
                        if (phMiraMonLayer->pMMBDXP->pField[nIField]
                                .FieldType == 'L')
                        {
                            if (phMiraMonLayer->isListField)
                                oField.SetType(OFTIntegerList);
                            else
                                oField.SetType(OFTInteger);

                            oField.SetSubType(OFSTBoolean);
                        }
                        else
                        {
                            if (phMiraMonLayer->isListField)
                                oField.SetType(OFTStringList);
                            else
                                oField.SetType(OFTString);
                        }
                    }
                    // It's a serialized JSON array
                    else if (phMiraMonLayer->iMultiRecord ==
                             MM_MULTIRECORD_JSON)
                    {
                        oField.SetType(OFTString);
                        oField.SetSubType(OFSTJSON);
                    }
                    else  // iMultiRecord decides which Record translate
                        oField.SetType(OFTString);
                }
                else if (phMiraMonLayer->pMMBDXP->pField[nIField].FieldType ==
                         'N')
                {
                    // It's a list?
                    if (phMiraMonLayer->iMultiRecord ==
                        MM_MULTIRECORD_NO_MULTIRECORD)
                    {
                        if (phMiraMonLayer->pMMBDXP->pField[nIField]
                                .DecimalsIfFloat)
                            oField.SetType(phMiraMonLayer->isListField
                                               ? OFTRealList
                                               : OFTReal);
                        else
                        {
                            if (phMiraMonLayer->pMMBDXP->pField[nIField]
                                    .BytesPerField < 10)
                            {
                                oField.SetType(phMiraMonLayer->isListField
                                                   ? OFTIntegerList
                                                   : OFTInteger);
                            }
                            else
                            {
                                oField.SetType(phMiraMonLayer->isListField
                                                   ? OFTInteger64List
                                                   : OFTInteger64);
                            }
                        }
                    }
                    // It's a serialized JSON array
                    else if (phMiraMonLayer->iMultiRecord ==
                             MM_MULTIRECORD_JSON)
                    {
                        oField.SetType(OFTString);
                        oField.SetSubType(OFSTJSON);
                    }
                    else
                    {
                        if (phMiraMonLayer->pMMBDXP->pField[nIField]
                                .DecimalsIfFloat)
                            oField.SetType(OFTReal);
                        else
                            oField.SetType(OFTInteger);
                    }
                }
                else if (phMiraMonLayer->pMMBDXP->pField[nIField].FieldType ==
                         'D')
                {
                    // It's a serialized JSON array
                    oField.SetType(OFTDate);
                    if (phMiraMonLayer->iMultiRecord == MM_MULTIRECORD_JSON)
                    {
                        oField.SetType(OFTString);
                        oField.SetSubType(OFSTJSON);
                    }
                }

                oField.SetWidth(
                    phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                oField.SetPrecision(
                    phMiraMonLayer->pMMBDXP->pField[nIField].DecimalsIfFloat);

                m_poFeatureDefn->AddFieldDefn(&oField);
            }
        }
    }

    bValidFile = true;
}

/****************************************************************************/
/*                           ~OGRMiraMonLayer()                             */
/****************************************************************************/

OGRMiraMonLayer::~OGRMiraMonLayer()

{
    if (m_nFeaturesRead > 0 && m_poFeatureDefn != nullptr)
    {
        CPLDebugOnly("MiraMon", "%d features read on layer '%s'.",
                     static_cast<int>(m_nFeaturesRead),
                     m_poFeatureDefn->GetName());
    }

    if (hMiraMonLayerPOL.bIsPolygon)
    {
        CPLDebugOnly("MiraMon", "Closing MiraMon polygons layer...");
        if (MMCloseLayer(&hMiraMonLayerPOL))
        {
            CPLDebugOnly("MiraMon", "Error closing polygons layer");
        }
        if (hMiraMonLayerPOL.TopHeader.nElemCount)
        {
            CPLDebugOnly("MiraMon",
                         sprintf_UINT64 " polygon(s) written in file %s.pol",
                         hMiraMonLayerPOL.TopHeader.nElemCount,
                         hMiraMonLayerPOL.pszSrcLayerName);
        }
        CPLDebugOnly("MiraMon", "MiraMon polygons layer closed");
    }
    else if (hMiraMonLayerPOL.ReadOrWrite == MM_WRITING_MODE)
    {
        CPLDebugOnly("MiraMon", "No MiraMon polygons layer created.");
    }

    if (hMiraMonLayerARC.bIsArc)
    {
        CPLDebugOnly("MiraMon", "Closing MiraMon arcs layer...");
        if (MMCloseLayer(&hMiraMonLayerARC))
        {
            CPLDebugOnly("MiraMon", "Error closing arcs layer");
        }
        if (hMiraMonLayerARC.TopHeader.nElemCount)
        {
            CPLDebugOnly("MiraMon",
                         sprintf_UINT64 " arc(s) written in file %s.arc",
                         hMiraMonLayerARC.TopHeader.nElemCount,
                         hMiraMonLayerARC.pszSrcLayerName);
        }

        CPLDebugOnly("MiraMon", "MiraMon arcs layer closed");
    }
    else if (hMiraMonLayerARC.ReadOrWrite == MM_WRITING_MODE)
    {
        CPLDebugOnly("MiraMon", "No MiraMon arcs layer created.");
    }

    if (hMiraMonLayerPNT.bIsPoint)
    {
        CPLDebugOnly("MiraMon", "Closing MiraMon points layer...");
        if (MMCloseLayer(&hMiraMonLayerPNT))
        {
            CPLDebugOnly("MiraMon", "Error closing points layer");
        }
        if (hMiraMonLayerPNT.TopHeader.nElemCount)
        {
            CPLDebugOnly("MiraMon",
                         sprintf_UINT64 " point(s) written in file %s.pnt",
                         hMiraMonLayerPNT.TopHeader.nElemCount,
                         hMiraMonLayerPNT.pszSrcLayerName);
        }
        CPLDebugOnly("MiraMon", "MiraMon points layer closed");
    }
    else if (hMiraMonLayerPNT.ReadOrWrite == MM_WRITING_MODE)
    {
        CPLDebugOnly("MiraMon", "No MiraMon points layer created.");
    }

    if (hMiraMonLayerARC.ReadOrWrite == MM_WRITING_MODE)
    {
        if (hMiraMonLayerReadOrNonGeom.bIsDBF)
        {
            if (hMiraMonLayerReadOrNonGeom.ReadOrWrite == MM_WRITING_MODE)
            {
                CPLDebugOnly("MiraMon", "Closing MiraMon DBF table ...");
            }
            MMCloseLayer(&hMiraMonLayerReadOrNonGeom);
            if (hMiraMonLayerReadOrNonGeom.ReadOrWrite == MM_WRITING_MODE)
            {
                CPLDebugOnly("MiraMon", "MiraMon DBF table closed");
            }
        }
        else if (hMiraMonLayerReadOrNonGeom.ReadOrWrite == MM_WRITING_MODE)
        {
            CPLDebugOnly("MiraMon", "No MiraMon DBF table created.");
        }
    }
    else
    {
        if (hMiraMonLayerReadOrNonGeom.ReadOrWrite == MM_WRITING_MODE)
        {
            CPLDebugOnly("MiraMon", "Closing MiraMon layer ...");
        }
        MMCloseLayer(&hMiraMonLayerReadOrNonGeom);
        if (hMiraMonLayerReadOrNonGeom.ReadOrWrite == MM_WRITING_MODE)
        {
            CPLDebugOnly("MiraMon", "MiraMon layer closed");
        }
    }

    if (hMiraMonLayerPOL.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "Destroying MiraMon polygons layer memory");
    }
    MMDestroyLayer(&hMiraMonLayerPOL);
    if (hMiraMonLayerPOL.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "MiraMon polygons layer memory destroyed");
    }

    if (hMiraMonLayerARC.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "Destroying MiraMon arcs layer memory");
    }
    MMDestroyLayer(&hMiraMonLayerARC);
    if (hMiraMonLayerARC.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "MiraMon arcs layer memory destroyed");
    }

    if (hMiraMonLayerPNT.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "Destroying MiraMon points layer memory");
    }
    MMDestroyLayer(&hMiraMonLayerPNT);
    if (hMiraMonLayerPNT.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "MiraMon points layer memory destroyed");
    }

    if (hMiraMonLayerReadOrNonGeom.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "Destroying MiraMon DBF table layer memory");
    }
    else
    {
        MMCPLDebug("MiraMon", "Destroying MiraMon layer memory");
    }

    MMDestroyLayer(&hMiraMonLayerReadOrNonGeom);
    if (hMiraMonLayerReadOrNonGeom.ReadOrWrite == MM_WRITING_MODE)
    {
        MMCPLDebug("MiraMon", "MiraMon DBF table layer memory destroyed");
    }
    else
    {
        MMCPLDebug("MiraMon", "MiraMon layer memory destroyed");
    }

    memset(&hMiraMonLayerReadOrNonGeom, 0, sizeof(hMiraMonLayerReadOrNonGeom));
    memset(&hMiraMonLayerPNT, 0, sizeof(hMiraMonLayerPNT));
    memset(&hMiraMonLayerARC, 0, sizeof(hMiraMonLayerARC));
    memset(&hMiraMonLayerPOL, 0, sizeof(hMiraMonLayerPOL));

    MMCPLDebug("MiraMon", "Destroying MiraMon temporary feature memory");
    MMDestroyFeature(&hMMFeature);
    MMCPLDebug("MiraMon", "MiraMon temporary feature memory");
    memset(&hMMFeature, 0, sizeof(hMMFeature));

    /* -------------------------------------------------------------------- */
    /*      Clean up.                                                       */
    /* -------------------------------------------------------------------- */

    if (m_poFeatureDefn)
        m_poFeatureDefn->Release();

    if (m_poSRS)
        m_poSRS->Release();

    if (m_fp != nullptr)
        VSIFCloseL(m_fp);

    if (padfValues != nullptr)
        CPLFree(padfValues);

    if (pnInt64Values != nullptr)
        CPLFree(pnInt64Values);
}

/****************************************************************************/
/*                            ResetReading()                                */
/****************************************************************************/

void OGRMiraMonLayer::ResetReading()

{
    if (m_iNextFID == 0)
        return;

    m_iNextFID = 0;

    //VSIFSeekL(m_fp, 0, SEEK_SET);
    if (!phMiraMonLayer)
        return;

    if (phMiraMonLayer->bIsPoint && phMiraMonLayer->MMPoint.pF)
    {
        VSIFSeekL(phMiraMonLayer->MMPoint.pF, 0, SEEK_SET);
        return;
    }
    if (phMiraMonLayer->bIsArc && !phMiraMonLayer->bIsPolygon &&
        phMiraMonLayer->MMArc.pF)
    {
        VSIFSeekL(phMiraMonLayer->MMArc.pF, 0, SEEK_SET);
        return;
    }
    if (phMiraMonLayer->bIsPolygon && phMiraMonLayer->MMPolygon.pF)
    {
        VSIFSeekL(phMiraMonLayer->MMPolygon.pF, 0, SEEK_SET);
        return;
    }
}

/****************************************************************************/
/*                         GetNextRawFeature()                              */
/****************************************************************************/

void OGRMiraMonLayer::GoToFieldOfMultipleRecord(MM_INTERNAL_FID iFID,
                                                MM_EXT_DBF_N_RECORDS nIRecord,
                                                MM_EXT_DBF_N_FIELDS nIField)

{
    // Not an error. Simply there are no features, but there are fields
    if (!phMiraMonLayer->pMultRecordIndex)
        return;

    fseek_function(
        phMiraMonLayer->pMMBDXP->pfDataBase,
        phMiraMonLayer->pMultRecordIndex[iFID].offset +
            (MM_FILE_OFFSET)nIRecord * phMiraMonLayer->pMMBDXP->BytesPerRecord +
            phMiraMonLayer->pMMBDXP->pField[nIField].AccumulatedBytes,
        SEEK_SET);
}

/****************************************************************************/
/*                         GetNextRawFeature()                              */
/****************************************************************************/

OGRFeature *OGRMiraMonLayer::GetNextRawFeature()
{
    if (!phMiraMonLayer)
        return nullptr;

    if (m_iNextFID >= (GUInt64)phMiraMonLayer->TopHeader.nElemCount)
        return nullptr;

    OGRFeature *poFeature = GetFeature(m_iNextFID);

    if (!poFeature)
        return nullptr;

    m_iNextFID++;
    return poFeature;
}

/****************************************************************************/
/*                         GetFeature()                                     */
/****************************************************************************/

OGRFeature *OGRMiraMonLayer::GetFeature(GIntBig nFeatureId)

{
    OGRGeometry *poGeom = nullptr;
    OGRPoint *poPoint = nullptr;
    OGRLineString *poLS = nullptr;
    MM_INTERNAL_FID nIElem;
    MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord = 0;

    if (!phMiraMonLayer)
        return nullptr;

    if (nFeatureId < 0)
        return nullptr;

    if (phMiraMonLayer->bIsPolygon)
    {
        if (nFeatureId == GINTBIG_MAX)
            return nullptr;

        nIElem = (MM_INTERNAL_FID)(nFeatureId + 1);
    }
    else
        nIElem = (MM_INTERNAL_FID)nFeatureId;

    if (nIElem >= phMiraMonLayer->TopHeader.nElemCount)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Read nFeatureId feature directly from the file.                 */
    /* -------------------------------------------------------------------- */
    switch (phMiraMonLayer->eLT)
    {
        case MM_LayerType_Point:
        case MM_LayerType_Point3d:
            // Read point
            poGeom = new OGRPoint();
            poPoint = poGeom->toPoint();

            // Get X,Y (z). MiraMon has no multipoints
            if (MMGetGeoFeatureFromVector(phMiraMonLayer, nIElem))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Wrong file format.");
                delete poGeom;
                return nullptr;
            }

            poPoint->setX(phMiraMonLayer->ReadFeature.pCoord[0].dfX);
            poPoint->setY(phMiraMonLayer->ReadFeature.pCoord[0].dfY);
            if (phMiraMonLayer->TopHeader.bIs3d)
                poPoint->setZ(phMiraMonLayer->ReadFeature.pZCoord[0]);
            break;

        case MM_LayerType_Arc:
        case MM_LayerType_Arc3d:
            poGeom = new OGRLineString();
            poLS = poGeom->toLineString();

            // Get X,Y (Z) n times MiraMon has no multilines
            if (MMGetGeoFeatureFromVector(phMiraMonLayer, nIElem))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Wrong file format.");
                delete poGeom;
                return nullptr;
            }

            for (MM_N_VERTICES_TYPE nIVrt = 0;
                 nIVrt < phMiraMonLayer->ReadFeature.pNCoordRing[0]; nIVrt++)
            {
                if (phMiraMonLayer->TopHeader.bIs3d)
                    poLS->addPoint(
                        phMiraMonLayer->ReadFeature.pCoord[nIVrt].dfX,
                        phMiraMonLayer->ReadFeature.pCoord[nIVrt].dfY,
                        phMiraMonLayer->ReadFeature.pZCoord[nIVrt]);
                else
                    poLS->addPoint(
                        phMiraMonLayer->ReadFeature.pCoord[nIVrt].dfX,
                        phMiraMonLayer->ReadFeature.pCoord[nIVrt].dfY);
            }
            break;

        case MM_LayerType_Pol:
        case MM_LayerType_Pol3d:
            // Read polygon
            auto poPoly = std::make_unique<OGRPolygon>();
            MM_POLYGON_RINGS_COUNT nIRing;
            MM_N_VERTICES_TYPE nIVrtAcum;

            if (phMiraMonLayer->TopHeader.bIsMultipolygon)
            {
                OGRMultiPolygon *poMP = nullptr;

                poGeom = new OGRMultiPolygon();
                poMP = poGeom->toMultiPolygon();

                // Get X,Y (Z) n times MiraMon has no multilines
                if (MMGetGeoFeatureFromVector(phMiraMonLayer, nIElem))
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Wrong file format.");
                    delete poGeom;
                    return nullptr;
                }

                nIVrtAcum = 0;
                if (!(phMiraMonLayer->ReadFeature.flag_VFG[0] &
                      MM_EXTERIOR_ARC_SIDE))
                {
                    CPLError(CE_Failure, CPLE_NoWriteAccess,
                             "Wrong polygon format.");
                    delete poGeom;
                    return nullptr;
                }

                for (nIRing = 0; nIRing < phMiraMonLayer->ReadFeature.nNRings;
                     nIRing++)
                {
                    auto poRing = std::make_unique<OGRLinearRing>();

                    for (MM_N_VERTICES_TYPE nIVrt = 0;
                         nIVrt <
                         phMiraMonLayer->ReadFeature.pNCoordRing[nIRing];
                         nIVrt++)
                    {
                        if (phMiraMonLayer->TopHeader.bIs3d)
                        {
                            poRing->addPoint(
                                phMiraMonLayer->ReadFeature.pCoord[nIVrtAcum]
                                    .dfX,
                                phMiraMonLayer->ReadFeature.pCoord[nIVrtAcum]
                                    .dfY,
                                phMiraMonLayer->ReadFeature.pZCoord[nIVrtAcum]);
                        }
                        else
                        {
                            poRing->addPoint(
                                phMiraMonLayer->ReadFeature.pCoord[nIVrtAcum]
                                    .dfX,
                                phMiraMonLayer->ReadFeature.pCoord[nIVrtAcum]
                                    .dfY);
                        }

                        nIVrtAcum++;
                    }

                    // If I'm going to start a new polygon...
                    if ((nIRing + 1 < phMiraMonLayer->ReadFeature.nNRings &&
                         ((phMiraMonLayer->ReadFeature.flag_VFG[nIRing + 1]) &
                          MM_EXTERIOR_ARC_SIDE)) ||
                        nIRing + 1 >= phMiraMonLayer->ReadFeature.nNRings)
                    {
                        poPoly->addRingDirectly(poRing.release());
                        poMP->addGeometryDirectly(poPoly.release());
                        poPoly = std::make_unique<OGRPolygon>();
                    }
                    else
                        poPoly->addRingDirectly(poRing.release());
                }
            }
            else
            {
                OGRPolygon *poP = nullptr;

                poGeom = new OGRPolygon();
                poP = poGeom->toPolygon();

                // Get X,Y (Z) n times because MiraMon has no multilinetrings
                if (MMGetGeoFeatureFromVector(phMiraMonLayer, nIElem))
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Wrong file format.");
                    delete poGeom;
                    return nullptr;
                }

                if (phMiraMonLayer->ReadFeature.nNRings &&
                    phMiraMonLayer->ReadFeature.nNumpCoord)
                {
                    nIVrtAcum = 0;
                    if (!(phMiraMonLayer->ReadFeature.flag_VFG[0] &
                          MM_EXTERIOR_ARC_SIDE))
                    {
                        CPLError(CE_Failure, CPLE_AssertionFailed,
                                 "Wrong polygon format.");
                        delete poGeom;
                        return nullptr;
                    }

                    for (nIRing = 0;
                         nIRing < phMiraMonLayer->ReadFeature.nNRings; nIRing++)
                    {
                        auto poRing = std::make_unique<OGRLinearRing>();

                        for (MM_N_VERTICES_TYPE nIVrt = 0;
                             nIVrt <
                             phMiraMonLayer->ReadFeature.pNCoordRing[nIRing];
                             nIVrt++)
                        {
                            if (phMiraMonLayer->TopHeader.bIs3d)
                            {
                                poRing->addPoint(phMiraMonLayer->ReadFeature
                                                     .pCoord[nIVrtAcum]
                                                     .dfX,
                                                 phMiraMonLayer->ReadFeature
                                                     .pCoord[nIVrtAcum]
                                                     .dfY,
                                                 phMiraMonLayer->ReadFeature
                                                     .pZCoord[nIVrtAcum]);
                            }
                            else
                            {
                                poRing->addPoint(phMiraMonLayer->ReadFeature
                                                     .pCoord[nIVrtAcum]
                                                     .dfX,
                                                 phMiraMonLayer->ReadFeature
                                                     .pCoord[nIVrtAcum]
                                                     .dfY);
                            }

                            nIVrtAcum++;
                        }
                        poP->addRingDirectly(poRing.release());
                    }
                }
            }

            break;
    }

    if (poGeom == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create feature.                                                 */
    /* -------------------------------------------------------------------- */
    auto poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
    poGeom->assignSpatialReference(m_poSRS);
    poFeature->SetGeometryDirectly(poGeom);

    /* -------------------------------------------------------------------- */
    /*      Process field values if its possible.                           */
    /* -------------------------------------------------------------------- */
    if (phMiraMonLayer->pMMBDXP &&
        (MM_EXT_DBF_N_RECORDS)nIElem < phMiraMonLayer->pMMBDXP->nRecords)
    {
        MM_EXT_DBF_N_FIELDS nIField;

        for (nIField = 0; nIField < phMiraMonLayer->pMMBDXP->nFields; nIField++)
        {
            if (MMResizeStringToOperateIfNeeded(
                    phMiraMonLayer,
                    phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField + 1))
            {
                return nullptr;
            }

            if (poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() ==
                    OFTStringList ||
                (poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() ==
                     OFTString &&
                 poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetSubType() ==
                     OFSTJSON))
            {
                if (!phMiraMonLayer->pMultRecordIndex ||
                    phMiraMonLayer->pMultRecordIndex[nIElem].nMR == 0)
                {
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    continue;
                }
                if (poFeature->GetDefnRef()
                        ->GetFieldDefn(nIField)
                        ->GetSubType() == OFSTJSON)
                {
                    if (MMResizeStringToOperateIfNeeded(
                            phMiraMonLayer,
                            phMiraMonLayer->pMMBDXP->BytesPerRecord +
                                2 * phMiraMonLayer->pMultRecordIndex[nIElem]
                                        .nMR +
                                8))
                    {
                        return nullptr;
                    }
                    std::string szStringToOperate = "[";
                    for (nIRecord = 0;
                         nIRecord <
                         phMiraMonLayer->pMultRecordIndex[nIElem].nMR;
                         nIRecord++)
                    {
                        GoToFieldOfMultipleRecord(nIElem, nIRecord, nIField);

                        fread_function(phMiraMonLayer->szStringToOperate,
                                       phMiraMonLayer->pMMBDXP->pField[nIField]
                                           .BytesPerField,
                                       1, phMiraMonLayer->pMMBDXP->pfDataBase);
                        phMiraMonLayer
                            ->szStringToOperate[phMiraMonLayer->pMMBDXP
                                                    ->pField[nIField]
                                                    .BytesPerField] = '\0';
                        MM_RemoveLeadingWhitespaceOfString(
                            phMiraMonLayer->szStringToOperate);
                        MM_RemoveWhitespacesFromEndOfString(
                            phMiraMonLayer->szStringToOperate);

                        if (phMiraMonLayer->pMMBDXP->CharSet ==
                            MM_JOC_CARAC_OEM850_DBASE)
                            MM_oemansi_n(
                                phMiraMonLayer->szStringToOperate,
                                phMiraMonLayer->pMMBDXP->pField[nIField]
                                    .BytesPerField);

                        if (phMiraMonLayer->pMMBDXP->CharSet !=
                            MM_JOC_CARAC_UTF8_DBF)
                        {
                            // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                            char *pszString =
                                CPLRecode(phMiraMonLayer->szStringToOperate,
                                          CPL_ENC_ISO8859_1, CPL_ENC_UTF8);

                            CPLStrlcpy(
                                phMiraMonLayer->szStringToOperate, pszString,
                                (size_t)phMiraMonLayer->pMMBDXP->pField[nIField]
                                        .BytesPerField +
                                    1);

                            CPLFree(pszString);
                        }
                        szStringToOperate.append(
                            phMiraMonLayer->szStringToOperate);

                        if (nIRecord <
                            phMiraMonLayer->pMultRecordIndex[nIElem].nMR - 1)
                        {
                            szStringToOperate.append(",");
                        }
                        else
                        {
                            szStringToOperate.append("]");
                        }
                    }
                    poFeature->SetField(nIField, szStringToOperate.c_str());
                }
                else
                {
                    CPLStringList aosValues;
                    for (nIRecord = 0;
                         nIRecord <
                         phMiraMonLayer->pMultRecordIndex[nIElem].nMR;
                         nIRecord++)
                    {
                        GoToFieldOfMultipleRecord(nIElem, nIRecord, nIField);
                        memset(phMiraMonLayer->szStringToOperate, 0,
                               phMiraMonLayer->pMMBDXP->pField[nIField]
                                   .BytesPerField);
                        fread_function(phMiraMonLayer->szStringToOperate,
                                       phMiraMonLayer->pMMBDXP->pField[nIField]
                                           .BytesPerField,
                                       1, phMiraMonLayer->pMMBDXP->pfDataBase);
                        phMiraMonLayer
                            ->szStringToOperate[phMiraMonLayer->pMMBDXP
                                                    ->pField[nIField]
                                                    .BytesPerField] = '\0';
                        MM_RemoveWhitespacesFromEndOfString(
                            phMiraMonLayer->szStringToOperate);

                        if (phMiraMonLayer->pMMBDXP->CharSet ==
                            MM_JOC_CARAC_OEM850_DBASE)
                            MM_oemansi_n(
                                phMiraMonLayer->szStringToOperate,
                                phMiraMonLayer->pMMBDXP->pField[nIField]
                                    .BytesPerField);

                        if (phMiraMonLayer->pMMBDXP->CharSet !=
                            MM_JOC_CARAC_UTF8_DBF)
                        {
                            // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                            char *pszString =
                                CPLRecode(phMiraMonLayer->szStringToOperate,
                                          CPL_ENC_ISO8859_1, CPL_ENC_UTF8);

                            CPLStrlcpy(
                                phMiraMonLayer->szStringToOperate, pszString,
                                (size_t)phMiraMonLayer->pMMBDXP->pField[nIField]
                                        .BytesPerField +
                                    1);

                            CPLFree(pszString);
                        }
                        aosValues.AddString(phMiraMonLayer->szStringToOperate);
                    }
                    poFeature->SetField(nIField, aosValues.List());
                }
            }
            else if (poFeature->GetDefnRef()
                         ->GetFieldDefn(nIField)
                         ->GetType() == OFTString)
            {
                if (!phMiraMonLayer->pMultRecordIndex ||
                    phMiraMonLayer->pMultRecordIndex[nIElem].nMR == 0)
                {
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    continue;
                }
                if (phMiraMonLayer->iMultiRecord !=
                    MM_MULTIRECORD_NO_MULTIRECORD)
                {
                    if (phMiraMonLayer->iMultiRecord == MM_MULTIRECORD_LAST)
                        GoToFieldOfMultipleRecord(
                            nIElem,
                            phMiraMonLayer->pMultRecordIndex[nIElem].nMR - 1,
                            nIField);
                    else if ((MM_EXT_DBF_N_MULTIPLE_RECORDS)
                                 phMiraMonLayer->iMultiRecord <
                             phMiraMonLayer->pMultRecordIndex[nIElem].nMR)
                        GoToFieldOfMultipleRecord(
                            nIElem,
                            (MM_EXT_DBF_N_MULTIPLE_RECORDS)
                                phMiraMonLayer->iMultiRecord,
                            nIField);
                    else
                    {
                        memset(phMiraMonLayer->szStringToOperate, 0,
                               phMiraMonLayer->pMMBDXP->pField[nIField]
                                   .BytesPerField);
                        continue;
                    }
                }
                else
                    GoToFieldOfMultipleRecord(nIElem, 0, nIField);

                memset(phMiraMonLayer->szStringToOperate, 0,
                       phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                fread_function(
                    phMiraMonLayer->szStringToOperate,
                    phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField, 1,
                    phMiraMonLayer->pMMBDXP->pfDataBase);
                phMiraMonLayer
                    ->szStringToOperate[phMiraMonLayer->pMMBDXP->pField[nIField]
                                            .BytesPerField] = '\0';
                MM_RemoveWhitespacesFromEndOfString(
                    phMiraMonLayer->szStringToOperate);

                if (phMiraMonLayer->pMMBDXP->CharSet ==
                    MM_JOC_CARAC_OEM850_DBASE)
                    MM_oemansi(phMiraMonLayer->szStringToOperate);

                if (phMiraMonLayer->pMMBDXP->CharSet != MM_JOC_CARAC_UTF8_DBF)
                {
                    // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                    char *pszString =
                        CPLRecode(phMiraMonLayer->szStringToOperate,
                                  CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                    CPLStrlcpy(phMiraMonLayer->szStringToOperate, pszString,
                               (size_t)phMiraMonLayer->pMMBDXP->pField[nIField]
                                       .BytesPerField +
                                   1);
                    CPLFree(pszString);
                }
                poFeature->SetField(nIField, phMiraMonLayer->szStringToOperate);
            }
            else if (poFeature->GetDefnRef()
                             ->GetFieldDefn(nIField)
                             ->GetType() == OFTIntegerList ||
                     poFeature->GetDefnRef()
                             ->GetFieldDefn(nIField)
                             ->GetType() == OFTRealList)
            {
                if (!phMiraMonLayer->pMultRecordIndex ||
                    phMiraMonLayer->pMultRecordIndex[nIElem].nMR == 0)
                {
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    continue;
                }
                MM_EXT_DBF_N_MULTIPLE_RECORDS nRealMR = 0;
                for (nIRecord = 0;
                     nIRecord < phMiraMonLayer->pMultRecordIndex[nIElem].nMR;
                     nIRecord++)
                {
                    GoToFieldOfMultipleRecord(nIElem, nIRecord, nIField);
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    fread_function(
                        phMiraMonLayer->szStringToOperate,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField,
                        1, phMiraMonLayer->pMMBDXP->pfDataBase);
                    phMiraMonLayer->szStringToOperate[phMiraMonLayer->pMMBDXP
                                                          ->pField[nIField]
                                                          .BytesPerField] =
                        '\0';

                    if (!MMIsEmptyString(phMiraMonLayer->szStringToOperate))
                    {
                        if (poFeature->GetDefnRef()
                                    ->GetFieldDefn(nIField)
                                    ->GetType() == OFTIntegerList &&
                            poFeature->GetDefnRef()
                                    ->GetFieldDefn(nIField)
                                    ->GetSubType() == OFSTBoolean)
                        {
                            if (*phMiraMonLayer->szStringToOperate == 'T' ||
                                *phMiraMonLayer->szStringToOperate == 'S' ||
                                *phMiraMonLayer->szStringToOperate == 'Y')
                                padfValues[nRealMR] = 1;
                            else
                                padfValues[nRealMR] = 0;
                        }
                        else
                        {
                            padfValues[nRealMR] =
                                atof(phMiraMonLayer->szStringToOperate);
                        }
                        nRealMR++;
                    }
                }

                poFeature->SetField(nIField, nRealMR, padfValues);
            }
            else if (poFeature->GetDefnRef()
                         ->GetFieldDefn(nIField)
                         ->GetType() == OFTInteger64List)
            {
                if (!phMiraMonLayer->pMultRecordIndex ||
                    phMiraMonLayer->pMultRecordIndex[nIElem].nMR == 0)
                {
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    continue;
                }
                MM_EXT_DBF_N_MULTIPLE_RECORDS nRealMR = 0;
                for (nIRecord = 0;
                     nIRecord < phMiraMonLayer->pMultRecordIndex[nIElem].nMR;
                     nIRecord++)
                {
                    GoToFieldOfMultipleRecord(nIElem, nIRecord, nIField);
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    fread_function(
                        phMiraMonLayer->szStringToOperate,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField,
                        1, phMiraMonLayer->pMMBDXP->pfDataBase);
                    phMiraMonLayer->szStringToOperate[phMiraMonLayer->pMMBDXP
                                                          ->pField[nIField]
                                                          .BytesPerField] =
                        '\0';

                    if (!MMIsEmptyString(phMiraMonLayer->szStringToOperate))
                    {
                        pnInt64Values[nRealMR] =
                            CPLAtoGIntBig(phMiraMonLayer->szStringToOperate);
                        nRealMR++;
                    }
                }

                poFeature->SetField(nIField, nRealMR, pnInt64Values);
            }
            else if (poFeature->GetDefnRef()
                             ->GetFieldDefn(nIField)
                             ->GetType() == OFTInteger ||
                     poFeature->GetDefnRef()
                             ->GetFieldDefn(nIField)
                             ->GetType() == OFTInteger64 ||
                     poFeature->GetDefnRef()
                             ->GetFieldDefn(nIField)
                             ->GetType() == OFTReal)
            {
                if (!phMiraMonLayer->pMultRecordIndex ||
                    phMiraMonLayer->pMultRecordIndex[nIElem].nMR == 0)
                {
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    continue;
                }
                if (phMiraMonLayer->iMultiRecord !=
                    MM_MULTIRECORD_NO_MULTIRECORD)
                {
                    if (phMiraMonLayer->iMultiRecord == MM_MULTIRECORD_LAST)
                    {
                        GoToFieldOfMultipleRecord(
                            nIElem,
                            phMiraMonLayer->pMultRecordIndex[nIElem].nMR - 1,
                            nIField);
                    }
                    else if ((MM_EXT_DBF_N_MULTIPLE_RECORDS)
                                 phMiraMonLayer->iMultiRecord <
                             phMiraMonLayer->pMultRecordIndex[nIElem].nMR)
                    {
                        GoToFieldOfMultipleRecord(
                            nIElem,
                            (MM_EXT_DBF_N_MULTIPLE_RECORDS)
                                phMiraMonLayer->iMultiRecord,
                            nIField);
                    }
                    else
                    {
                        memset(phMiraMonLayer->szStringToOperate, 0,
                               phMiraMonLayer->pMMBDXP->pField[nIField]
                                   .BytesPerField);
                        continue;
                    }
                }
                else
                    GoToFieldOfMultipleRecord(nIElem, 0, nIField);

                memset(phMiraMonLayer->szStringToOperate, 0,
                       phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                fread_function(
                    phMiraMonLayer->szStringToOperate,
                    phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField, 1,
                    phMiraMonLayer->pMMBDXP->pfDataBase);
                phMiraMonLayer
                    ->szStringToOperate[phMiraMonLayer->pMMBDXP->pField[nIField]
                                            .BytesPerField] = '\0';
                MM_RemoveWhitespacesFromEndOfString(
                    phMiraMonLayer->szStringToOperate);

                if (poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() ==
                    OFTInteger64)
                {
                    poFeature->SetField(
                        nIField,
                        CPLAtoGIntBig(phMiraMonLayer->szStringToOperate));
                }
                else
                {
                    if (poFeature->GetDefnRef()
                                ->GetFieldDefn(nIField)
                                ->GetType() == OFTInteger &&
                        poFeature->GetDefnRef()
                                ->GetFieldDefn(nIField)
                                ->GetSubType() == OFSTBoolean)
                    {
                        if (*phMiraMonLayer->szStringToOperate == 'T' ||
                            *phMiraMonLayer->szStringToOperate == 'S' ||
                            *phMiraMonLayer->szStringToOperate == 'Y')
                            poFeature->SetField(nIField, 1);
                        else
                            poFeature->SetField(nIField, 0);
                    }
                    else
                    {
                        poFeature->SetField(
                            nIField, atof(phMiraMonLayer->szStringToOperate));
                    }
                }
            }
            else if (poFeature->GetDefnRef()
                         ->GetFieldDefn(nIField)
                         ->GetType() == OFTDate)
            {
                if (!phMiraMonLayer->pMultRecordIndex ||
                    phMiraMonLayer->pMultRecordIndex[nIElem].nMR == 0)
                {
                    memset(
                        phMiraMonLayer->szStringToOperate, 0,
                        phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                    continue;
                }
                if (phMiraMonLayer->iMultiRecord !=
                    MM_MULTIRECORD_NO_MULTIRECORD)
                {
                    if (phMiraMonLayer->iMultiRecord == MM_MULTIRECORD_LAST)
                        GoToFieldOfMultipleRecord(
                            nIElem,
                            phMiraMonLayer->pMultRecordIndex[nIElem].nMR - 1,
                            nIField);
                    else if ((MM_EXT_DBF_N_MULTIPLE_RECORDS)
                                 phMiraMonLayer->iMultiRecord <
                             phMiraMonLayer->pMultRecordIndex[nIElem].nMR)
                        GoToFieldOfMultipleRecord(
                            nIElem,
                            (MM_EXT_DBF_N_MULTIPLE_RECORDS)
                                phMiraMonLayer->iMultiRecord,
                            nIField);
                    else
                    {
                        memset(phMiraMonLayer->szStringToOperate, 0,
                               phMiraMonLayer->pMMBDXP->pField[nIField]
                                   .BytesPerField);
                        continue;
                    }
                }
                else
                    GoToFieldOfMultipleRecord(nIElem, 0, nIField);

                memset(phMiraMonLayer->szStringToOperate, 0,
                       phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField);
                fread_function(
                    phMiraMonLayer->szStringToOperate,
                    phMiraMonLayer->pMMBDXP->pField[nIField].BytesPerField, 1,
                    phMiraMonLayer->pMMBDXP->pfDataBase);
                phMiraMonLayer
                    ->szStringToOperate[phMiraMonLayer->pMMBDXP->pField[nIField]
                                            .BytesPerField] = '\0';

                MM_RemoveWhitespacesFromEndOfString(
                    phMiraMonLayer->szStringToOperate);
                if (!MMIsEmptyString(phMiraMonLayer->szStringToOperate))
                {
                    char pszDate_5[5];
                    char pszDate_3[3];
                    int Year, Month, Day;

                    CPLStrlcpy(pszDate_5, phMiraMonLayer->szStringToOperate, 5);
                    pszDate_5[4] = '\0';
                    Year = atoi(pszDate_5);

                    CPLStrlcpy(pszDate_3, phMiraMonLayer->szStringToOperate + 4,
                               3);
                    (pszDate_3)[2] = '\0';
                    Month = atoi(pszDate_3);

                    CPLStrlcpy(pszDate_3, phMiraMonLayer->szStringToOperate + 6,
                               3);
                    (pszDate_3)[2] = '\0';
                    Day = atoi(pszDate_3);

                    poFeature->SetField(nIField, Year, Month, Day);
                }
                else
                    poFeature->SetField(nIField,
                                        phMiraMonLayer->szStringToOperate);
            }
        }
    }

    // Even in case of polygons, where the first feature is jumped
    // the ID of the first feature has to be 0, the second, 1,...
    poFeature->SetFID(nFeatureId);

    m_nFeaturesRead++;
    return poFeature.release();
}

/****************************************************************************/
/*                         GetFeatureCount()                                */
/****************************************************************************/
GIntBig OGRMiraMonLayer::GetFeatureCount(int bForce)
{
    if (!phMiraMonLayer || m_poFilterGeom != nullptr ||
        m_poAttrQuery != nullptr)
        return OGRLayer::GetFeatureCount(bForce);

    if (phMiraMonLayer->bIsPolygon)
    {
        return std::max((GIntBig)0,
                        (GIntBig)(phMiraMonLayer->TopHeader.nElemCount - 1));
    }
    return (GIntBig)phMiraMonLayer->TopHeader.nElemCount;
}

/****************************************************************************/
/*                      MMProcessMultiGeometry()                            */
/****************************************************************************/
OGRErr OGRMiraMonLayer::MMProcessMultiGeometry(OGRGeometryH hGeom,
                                               OGRFeature *poFeature)

{
    OGRErr eErr = OGRERR_NONE;
    OGRGeometry *poGeom = OGRGeometry::FromHandle(hGeom);

    if (poGeom == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Features without geometry not supported by MiraMon writer.");
        return OGRERR_FAILURE;
    }

    // Multigeometry field processing (just in case of a MG inside a MG)
    if (wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)
    {
        int nGeom = OGR_G_GetGeometryCount(OGRGeometry::ToHandle(poGeom));
        for (int iGeom = 0; iGeom < nGeom; iGeom++)
        {
            OGRGeometryH poSubGeometry =
                OGR_G_GetGeometryRef(OGRGeometry::ToHandle(poGeom), iGeom);
            eErr = MMProcessMultiGeometry(poSubGeometry, poFeature);
            if (eErr != OGRERR_NONE)
                return eErr;
        }
        return eErr;
    }
    // Converting multilines and multi points to simple ones
    if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString ||
        wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        int nGeom = OGR_G_GetGeometryCount(OGRGeometry::ToHandle(poGeom));
        for (int iGeom = 0; iGeom < nGeom; iGeom++)
        {
            OGRGeometryH poSubGeometry =
                OGR_G_GetGeometryRef(OGRGeometry::ToHandle(poGeom), iGeom);
            eErr = MMProcessGeometry(poSubGeometry, poFeature, (iGeom == 0));
            if (eErr != OGRERR_NONE)
                return eErr;
        }
        return eErr;
    }

    // Processing a simple geometry
    return MMProcessGeometry(OGRGeometry::ToHandle(poGeom), poFeature, TRUE);
}

/****************************************************************************/
/*                           MMProcessGeometry()                            */
/****************************************************************************/
OGRErr OGRMiraMonLayer::MMProcessGeometry(OGRGeometryH hGeom,
                                          OGRFeature *poFeature,
                                          MM_BOOLEAN bcalculateRecord)

{
    OGRErr eErr = OGRERR_NONE;
    OGRGeometry *poGeom = nullptr;
    if (hGeom)
    {
        poGeom = OGRGeometry::FromHandle(hGeom);

        // Translating types from GDAL to MiraMon
        int eLT = poGeom->getGeometryType();
        switch (wkbFlatten(eLT))
        {
            case wkbPoint:
                phMiraMonLayer = &hMiraMonLayerPNT;
                if (OGR_G_Is3D(hGeom))
                    phMiraMonLayer->eLT = MM_LayerType_Point3d;
                else
                    phMiraMonLayer->eLT = MM_LayerType_Point;
                break;
            case wkbLineString:
                phMiraMonLayer = &hMiraMonLayerARC;
                if (OGR_G_Is3D(hGeom))
                    phMiraMonLayer->eLT = MM_LayerType_Arc3d;
                else
                    phMiraMonLayer->eLT = MM_LayerType_Arc;
                break;
            case wkbPolygon:
            case wkbMultiPolygon:
            case wkbPolyhedralSurface:
            case wkbTIN:
            case wkbTriangle:
                phMiraMonLayer = &hMiraMonLayerPOL;
                if (OGR_G_Is3D(hGeom))
                    phMiraMonLayer->eLT = MM_LayerType_Pol3d;
                else
                    phMiraMonLayer->eLT = MM_LayerType_Pol;
                break;
            case wkbUnknown:
            default:
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "MiraMon "
                         "does not support geometry type '%d'",
                         eLT);
                return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
            }
        }
    }
    else
    {
        // Processing only the table. A DBF will be generated
        phMiraMonLayer = &hMiraMonLayerReadOrNonGeom;
        phMiraMonLayer->eLT = MM_LayerType_Unknown;
    }

    /* -------------------------------------------------------------------- */
    /*      Field translation from GDAL to MiraMon                          */
    /* -------------------------------------------------------------------- */
    // Reset the object where read coordinates are going to be stored
    MMResetFeatureGeometry(&hMMFeature);
    if (bcalculateRecord)
    {
        MMResetFeatureRecord(&hMMFeature);
        if (!phMiraMonLayer->pLayerDB)
        {
            eErr = TranslateFieldsToMM();
            if (eErr != OGRERR_NONE)
                return eErr;
        }
        // Content field translation from GDAL to MiraMon
        eErr = TranslateFieldsValuesToMM(poFeature);
        if (eErr != OGRERR_NONE)
        {
            CPLDebugOnly("MiraMon", "Error in MMProcessGeometry()");
            return eErr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write Geometry                                                  */
    /* -------------------------------------------------------------------- */

    // Reads objects with coordinates and transform them to MiraMon
    if (poGeom)
    {
        eErr = MMLoadGeometry(OGRGeometry::ToHandle(poGeom));
    }
    else
    {
        if (!phMiraMonLayer->bIsBeenInit)
        {
            phMiraMonLayer->bIsDBF = TRUE;
            if (MMInitLayerByType(phMiraMonLayer))
                eErr = OGRERR_FAILURE;

            phMiraMonLayer->bIsBeenInit = 1;
        }
    }

    // Writes coordinates to the disk
    if (eErr == OGRERR_NONE)
        return MMWriteGeometry();
    CPLDebugOnly("MiraMon", "Error in MMProcessGeometry()");
    return eErr;
}

/****************************************************************************/
/*                           ICreateFeature()                               */
/****************************************************************************/

OGRErr OGRMiraMonLayer::ICreateFeature(OGRFeature *poFeature)

{
    OGRErr eErr = OGRERR_NONE;

    if (!m_bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Cannot create features on a read-only dataset.");
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the feature                                           */
    /* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    // Processing a feature without geometry.
    if (poGeom == nullptr)
    {
        eErr = MMProcessGeometry(nullptr, poFeature, TRUE);
        if (phMiraMonLayer->bIsDBF && phMiraMonLayer->TopHeader.nElemCount > 0)
            poFeature->SetFID((GIntBig)phMiraMonLayer->TopHeader.nElemCount -
                              1);
        return eErr;
    }

    // Converting to simple geometries
    if (wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)
    {
        int nGeom = OGR_G_GetGeometryCount(OGRGeometry::ToHandle(poGeom));
        for (int iGeom = 0; iGeom < nGeom; iGeom++)
        {
            OGRGeometryH poSubGeometry =
                OGR_G_GetGeometryRef(OGRGeometry::ToHandle(poGeom), iGeom);
            eErr = MMProcessMultiGeometry(poSubGeometry, poFeature);
            if (eErr != OGRERR_NONE)
                return eErr;
        }

        return eErr;
    }

    // Processing the geometry
    eErr = MMProcessMultiGeometry(OGRGeometry::ToHandle(poGeom), poFeature);

    // Set the FID from 0 index
    if (phMiraMonLayer)
    {
        if (phMiraMonLayer->bIsPolygon &&
            phMiraMonLayer->TopHeader.nElemCount > 1)
            poFeature->SetFID((GIntBig)phMiraMonLayer->TopHeader.nElemCount -
                              2);
        else if (phMiraMonLayer->TopHeader.nElemCount > 0)
            poFeature->SetFID((GIntBig)phMiraMonLayer->TopHeader.nElemCount -
                              1);
    }
    return eErr;
}

/****************************************************************************/
/*                          MMDumpVertices()                                */
/****************************************************************************/

OGRErr OGRMiraMonLayer::MMDumpVertices(OGRGeometryH hGeom,
                                       MM_BOOLEAN bExternalRing,
                                       MM_BOOLEAN bUseVFG)
{
    // If the MiraMonLayer structure has not been init,
    // here is the moment to do that.
    if (!phMiraMonLayer)
        return OGRERR_FAILURE;

    if (!phMiraMonLayer->bIsBeenInit)
    {
        if (MMInitLayerByType(phMiraMonLayer))
            return OGRERR_FAILURE;
        phMiraMonLayer->bIsBeenInit = 1;
    }
    if (MMResize_MM_N_VERTICES_TYPE_Pointer(
            &hMMFeature.pNCoordRing, &hMMFeature.nMaxpNCoordRing,
            (MM_N_VERTICES_TYPE)hMMFeature.nNRings + 1, MM_MEAN_NUMBER_OF_RINGS,
            0))
        return OGRERR_FAILURE;

    if (bUseVFG)
    {
        if (MMResizeVFGPointer(&hMMFeature.flag_VFG, &hMMFeature.nMaxVFG,
                               (MM_INTERNAL_FID)hMMFeature.nNRings + 1,
                               MM_MEAN_NUMBER_OF_RINGS, 0))
            return OGRERR_FAILURE;

        hMMFeature.flag_VFG[hMMFeature.nIRing] = MM_END_ARC_IN_RING;
        if (bExternalRing)
            hMMFeature.flag_VFG[hMMFeature.nIRing] |= MM_EXTERIOR_ARC_SIDE;
        // In MiraMon the external ring is clockwise and the internals are
        // coounterclockwise.
        OGRGeometry *poGeom = OGRGeometry::FromHandle(hGeom);
        if ((bExternalRing && !poGeom->toLinearRing()->isClockwise()) ||
            (!bExternalRing && poGeom->toLinearRing()->isClockwise()))
            hMMFeature.flag_VFG[hMMFeature.nIRing] |= MM_ROTATE_ARC;
    }

    hMMFeature.pNCoordRing[hMMFeature.nIRing] = OGR_G_GetPointCount(hGeom);

    if (MMResizeMM_POINT2DPointer(&hMMFeature.pCoord, &hMMFeature.nMaxpCoord,
                                  hMMFeature.nICoord +
                                      hMMFeature.pNCoordRing[hMMFeature.nIRing],
                                  MM_MEAN_NUMBER_OF_NCOORDS, 0))
        return OGRERR_FAILURE;
    if (MMResizeDoublePointer(&hMMFeature.pZCoord, &hMMFeature.nMaxpZCoord,
                              hMMFeature.nICoord +
                                  hMMFeature.pNCoordRing[hMMFeature.nIRing],
                              MM_MEAN_NUMBER_OF_NCOORDS, 0))
        return OGRERR_FAILURE;

    hMMFeature.bAllZHaveSameValue = TRUE;
    for (int iPoint = 0;
         (MM_N_VERTICES_TYPE)iPoint < hMMFeature.pNCoordRing[hMMFeature.nIRing];
         iPoint++)
    {
        hMMFeature.pCoord[hMMFeature.nICoord].dfX = OGR_G_GetX(hGeom, iPoint);
        hMMFeature.pCoord[hMMFeature.nICoord].dfY = OGR_G_GetY(hGeom, iPoint);
        if (OGR_G_GetCoordinateDimension(hGeom) == 2)
            hMMFeature.pZCoord[hMMFeature.nICoord] =
                MM_NODATA_COORD_Z;  // Possible rare case
        else
        {
            hMMFeature.pZCoord[hMMFeature.nICoord] = OGR_G_GetZ(hGeom, iPoint);
            phMiraMonLayer->bIsReal3d = 1;
        }

        // Asking if last Z-coordinate is the same than this one.
        // If all Z-coordinates are the same, following MiraMon specification
        // only the hMMFeature.pZCoord[0] value will be used and the number of
        // vertices will be saved as a negative number on disk
        if (iPoint > 0 &&
            !CPLIsEqual(hMMFeature.pZCoord[hMMFeature.nICoord],
                        hMMFeature.pZCoord[hMMFeature.nICoord - 1]))
            hMMFeature.bAllZHaveSameValue = FALSE;

        hMMFeature.nICoord++;
    }
    hMMFeature.nIRing++;
    hMMFeature.nNRings++;
    return OGRERR_NONE;
}

/****************************************************************************/
/*                           MMLoadGeometry()                               */
/*                                                                          */
/*      Loads on a MiraMon object Feature all coordinates from feature      */
/*                                                                          */
/****************************************************************************/
OGRErr OGRMiraMonLayer::MMLoadGeometry(OGRGeometryH hGeom)

{
    OGRErr eErr = OGRERR_NONE;
    MM_BOOLEAN bExternalRing;

    /* -------------------------------------------------------------------- */
    /*      This is a geometry with sub-geometries.                         */
    /* -------------------------------------------------------------------- */
    int nGeom = OGR_G_GetGeometryCount(hGeom);

    int eLT = wkbFlatten(OGR_G_GetGeometryType(hGeom));

    if (eLT == wkbMultiPolygon || eLT == wkbPolyhedralSurface || eLT == wkbTIN)
    {
        for (int iGeom = 0; iGeom < nGeom && eErr == OGRERR_NONE; iGeom++)
        {
            OGRGeometryH poSubGeometry = OGR_G_GetGeometryRef(hGeom, iGeom);

            // Reads all coordinates
            eErr = MMLoadGeometry(poSubGeometry);
            if (eErr != OGRERR_NONE)
                return eErr;
        }
    }
    if (eLT == wkbTriangle)
    {
        for (int iGeom = 0; iGeom < nGeom && eErr == OGRERR_NONE; iGeom++)
        {
            OGRGeometryH poSubGeometry = OGR_G_GetGeometryRef(hGeom, iGeom);

            // Reads all coordinates
            eErr = MMDumpVertices(poSubGeometry, TRUE, TRUE);
            if (eErr != OGRERR_NONE)
                return eErr;
        }
    }
    else if (eLT == wkbPolygon)
    {
        for (int iGeom = 0; iGeom < nGeom && eErr == OGRERR_NONE; iGeom++)
        {
            OGRGeometryH poSubGeometry = OGR_G_GetGeometryRef(hGeom, iGeom);

            if (iGeom == 0)
                bExternalRing = true;
            else
                bExternalRing = false;

            eErr = MMDumpVertices(poSubGeometry, bExternalRing, TRUE);
            if (eErr != OGRERR_NONE)
                return eErr;
        }
    }
    else if (eLT == wkbPoint || eLT == wkbLineString)
    {
        // Reads all coordinates
        eErr = MMDumpVertices(hGeom, true, FALSE);

        if (eErr != OGRERR_NONE)
            return eErr;
    }
    else if (eLT == wkbGeometryCollection)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "MiraMon: wkbGeometryCollection inside a wkbGeometryCollection?");
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

    return OGRERR_NONE;
}

/****************************************************************************/
/*                           WriteGeometry()                                */
/*                                                                          */
/*                    Writes a geometry to the file.                        */
/****************************************************************************/

OGRErr OGRMiraMonLayer::MMWriteGeometry()

{
    OGRErr eErr = MMAddFeature(phMiraMonLayer, &hMMFeature);

    if (eErr == MM_FATAL_ERROR_WRITING_FEATURES)
    {
        CPLDebugOnly("MiraMon", "Error in MMAddFeature() "
                                "MM_FATAL_ERROR_WRITING_FEATURES");
        CPLError(CE_Failure, CPLE_FileIO, "MiraMon write failure: %s",
                 VSIStrerror(errno));
        return OGRERR_FAILURE;
    }
    if (eErr == MM_STOP_WRITING_FEATURES)
    {
        CPLDebugOnly("MiraMon", "Error in MMAddFeature() "
                                "MM_STOP_WRITING_FEATURES");
        CPLError(CE_Failure, CPLE_FileIO,
                 "MiraMon format limitations. Try V2.0 option (-lco "
                 "Version=V2.0). " sprintf_UINT64
                 " elements have been written correctly.",
                 phMiraMonLayer->TopHeader.nElemCount);
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/****************************************************************************/
/*                       TranslateFieldsToMM()                              */
/*                                                                          */
/*      Translase ogr Fields to a structure that MiraMon can understand     */
/****************************************************************************/

OGRErr OGRMiraMonLayer::TranslateFieldsToMM()

{
    if (m_poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    CPLDebugOnly("MiraMon", "Translating fields to MiraMon...");
    // If the structure is filled we do anything
    if (phMiraMonLayer->pLayerDB)
        return OGRERR_NONE;

    phMiraMonLayer->pLayerDB = static_cast<struct MiraMonDataBase *>(
        VSICalloc(sizeof(*phMiraMonLayer->pLayerDB), 1));
    if (!phMiraMonLayer->pLayerDB)
        return OGRERR_NOT_ENOUGH_MEMORY;

    phMiraMonLayer->pLayerDB->pFields =
        static_cast<struct MiraMonDataBaseField *>(
            VSICalloc(m_poFeatureDefn->GetFieldCount(),
                      sizeof(*(phMiraMonLayer->pLayerDB->pFields))));
    if (!phMiraMonLayer->pLayerDB->pFields)
        return OGRERR_NOT_ENOUGH_MEMORY;

    phMiraMonLayer->pLayerDB->nNFields = 0;
    if (phMiraMonLayer->pLayerDB->pFields)
    {
        memset(phMiraMonLayer->pLayerDB->pFields, 0,
               m_poFeatureDefn->GetFieldCount() *
                   sizeof(*phMiraMonLayer->pLayerDB->pFields));
        for (MM_EXT_DBF_N_FIELDS iField = 0;
             iField < (MM_EXT_DBF_N_FIELDS)m_poFeatureDefn->GetFieldCount();
             iField++)
        {
            switch (m_poFeatureDefn->GetFieldDefn(iField)->GetType())
            {
                case OFTInteger:
                case OFTIntegerList:
                    if (m_poFeatureDefn->GetFieldDefn(iField)->GetSubType() ==
                        OFSTBoolean)
                    {
                        phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                            MM_Logic;
                    }
                    else
                    {
                        phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                            MM_Numeric;
                    }

                    phMiraMonLayer->pLayerDB->pFields[iField]
                        .nNumberOfDecimals = 0;
                    break;

                case OFTInteger64:
                case OFTInteger64List:
                    phMiraMonLayer->pLayerDB->pFields[iField].bIs64BitInteger =
                        TRUE;
                    phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                        MM_Numeric;
                    phMiraMonLayer->pLayerDB->pFields[iField]
                        .nNumberOfDecimals = 0;
                    break;

                case OFTReal:
                case OFTRealList:
                    phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                        MM_Numeric;
                    phMiraMonLayer->pLayerDB->pFields[iField]
                        .nNumberOfDecimals =
                        m_poFeatureDefn->GetFieldDefn(iField)->GetPrecision();
                    break;

                case OFTBinary:
                    phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                        MM_Character;
                    break;

                case OFTDate:
                    phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                        MM_Data;
                    break;

                case OFTTime:
                case OFTDateTime:
                    phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                        MM_Character;
                    break;

                case OFTString:
                case OFTStringList:
                default:
                    phMiraMonLayer->pLayerDB->pFields[iField].eFieldType =
                        MM_Character;
                    break;
            }
            if (m_poFeatureDefn->GetFieldDefn(iField)->GetType() == OFTDate)
                phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize = 8;
            else if ((m_poFeatureDefn->GetFieldDefn(iField)->GetType() ==
                          OFTInteger ||
                      m_poFeatureDefn->GetFieldDefn(iField)->GetType() ==
                          OFTIntegerList) &&
                     m_poFeatureDefn->GetFieldDefn(iField)->GetSubType() ==
                         OFSTBoolean)
            {
                phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize = 1;
            }
            else
            {
                // As https://gdal.org/api/ogrfeature_cpp.html indicates that
                // precision (number of digits after decimal point) is optional,
                // and a 0 is probably the default value, in that case we prefer
                // to save all the guaranteed significant figures in a double
                // (needed if a field contains, for instance, coordinates in
                // geodetic degrees and a 1:1000 map precision applies).
                if (m_poFeatureDefn->GetFieldDefn(iField)->GetPrecision() == 0)
                {
                    if (m_poFeatureDefn->GetFieldDefn(iField)->GetType() ==
                            OFTReal ||
                        m_poFeatureDefn->GetFieldDefn(iField)->GetType() ==
                            OFTRealList)
                    {
                        phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize =
                            20;
                        phMiraMonLayer->pLayerDB->pFields[iField]
                            .nNumberOfDecimals = MAX_RELIABLE_SF_DOUBLE;
                    }
                    else
                    {
                        phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize =
                            m_poFeatureDefn->GetFieldDefn(iField)->GetWidth();
                        if (phMiraMonLayer->pLayerDB->pFields[iField]
                                .nFieldSize == 0)
                            phMiraMonLayer->pLayerDB->pFields[iField]
                                .nFieldSize = 3;
                    }

                    // Some exceptions for some fields:
                    if (EQUAL(
                            m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                            "fontsize"))
                    {
                        phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize =
                            11;
                        phMiraMonLayer->pLayerDB->pFields[iField]
                            .nNumberOfDecimals = 3;
                    }
                    else if (EQUAL(m_poFeatureDefn->GetFieldDefn(iField)
                                       ->GetNameRef(),
                                   "leading") ||
                             EQUAL(m_poFeatureDefn->GetFieldDefn(iField)
                                       ->GetNameRef(),
                                   "chrwidth") ||
                             EQUAL(m_poFeatureDefn->GetFieldDefn(iField)
                                       ->GetNameRef(),
                                   "chrspacing"))
                    {
                        phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize =
                            8;
                        phMiraMonLayer->pLayerDB->pFields[iField]
                            .nNumberOfDecimals = 3;
                    }
                    else if (EQUAL(m_poFeatureDefn->GetFieldDefn(iField)
                                       ->GetNameRef(),
                                   "orientacio"))
                    {
                        phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize =
                            7;
                        phMiraMonLayer->pLayerDB->pFields[iField]
                            .nNumberOfDecimals = 2;
                    }
                }
                else
                {
                    // One more space for the "."
                    phMiraMonLayer->pLayerDB->pFields[iField].nFieldSize =
                        (unsigned int)(m_poFeatureDefn->GetFieldDefn(iField)
                                           ->GetWidth() +
                                       1);
                }
            }

            // Recode from UTF-8 if necessary
            if (phMiraMonLayer->nCharSet != MM_JOC_CARAC_UTF8_DBF)
            {
                char *pszString = CPLRecode(
                    m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                    CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
                CPLStrlcpy(
                    phMiraMonLayer->pLayerDB->pFields[iField].pszFieldName,
                    pszString, MM_MAX_LON_FIELD_NAME_DBF);
                CPLFree(pszString);
            }
            else
            {
                CPLStrlcpy(
                    phMiraMonLayer->pLayerDB->pFields[iField].pszFieldName,
                    m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                    MM_MAX_LON_FIELD_NAME_DBF);
            }

            if (m_poFeatureDefn->GetFieldDefn(iField)->GetAlternativeNameRef())
            {
                if (phMiraMonLayer->nCharSet != MM_JOC_CARAC_UTF8_DBF)
                {
                    char *pszString =
                        CPLRecode(m_poFeatureDefn->GetFieldDefn(iField)
                                      ->GetAlternativeNameRef(),
                                  CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
                    CPLStrlcpy(phMiraMonLayer->pLayerDB->pFields[iField]
                                   .pszFieldDescription,
                               pszString, MM_MAX_BYTES_FIELD_DESC);
                    CPLFree(pszString);
                }
                else
                {
                    CPLStrlcpy(phMiraMonLayer->pLayerDB->pFields[iField]
                                   .pszFieldDescription,
                               m_poFeatureDefn->GetFieldDefn(iField)
                                   ->GetAlternativeNameRef(),
                               MM_MAX_BYTES_FIELD_DESC);
                }
            }
            phMiraMonLayer->pLayerDB->nNFields++;
        }
    }

    CPLDebugOnly("MiraMon", "Fields to MiraMon translated.");
    return OGRERR_NONE;
}

/****************************************************************************/
/*                       TranslateFieldsValuesToMM()                        */
/*                                                                          */
/*      Translate ogr Fields to a structure that MiraMon can understand     */
/****************************************************************************/

OGRErr OGRMiraMonLayer::TranslateFieldsValuesToMM(OGRFeature *poFeature)

{
    if (m_poFeatureDefn->GetFieldCount() == 0)
    {
        // MiraMon have private DataBase records
        hMMFeature.nNumMRecords = 1;
        return OGRERR_NONE;
    }

    MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord;
    MM_EXT_DBF_N_FIELDS nNumFields = m_poFeatureDefn->GetFieldCount();
    MM_EXT_DBF_N_MULTIPLE_RECORDS nNumRecords, nRealNumRecords;
    hMMFeature.nNumMRecords = 0;
#define MAX_SIZE_OF_FIELD_NUMBER_WITH_MINUS 22

    for (MM_EXT_DBF_N_FIELDS iField = 0; iField < nNumFields; iField++)
    {
        OGRFieldType eFType = m_poFeatureDefn->GetFieldDefn(iField)->GetType();
        OGRFieldSubType eFSType =
            m_poFeatureDefn->GetFieldDefn(iField)->GetSubType();
        const char *pszRawValue = poFeature->GetFieldAsString(iField);

        if (eFType == OFTStringList)
        {
            char **papszValues = poFeature->GetFieldAsStringList(iField);
            nRealNumRecords = nNumRecords = CSLCount(papszValues);
            if (nNumRecords == 0)
                nNumRecords++;
            hMMFeature.nNumMRecords =
                max_function(hMMFeature.nNumMRecords, nNumRecords);
            if (MMResizeMiraMonRecord(
                    &hMMFeature.pRecords, &hMMFeature.nMaxMRecords,
                    hMMFeature.nNumMRecords, MM_INC_NUMBER_OF_RECORDS,
                    hMMFeature.nNumMRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < nRealNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField =
                    m_poFeatureDefn->GetFieldCount();

                if (MMResizeMiraMonFieldValue(
                        &(hMMFeature.pRecords[nIRecord].pField),
                        &hMMFeature.pRecords[nIRecord].nMaxField,
                        hMMFeature.pRecords[nIRecord].nNumField,
                        MM_INC_NUMBER_OF_FIELDS,
                        hMMFeature.pRecords[nIRecord].nNumField))
                    return OGRERR_NOT_ENOUGH_MEMORY;

                if (phMiraMonLayer->nCharSet != MM_JOC_CARAC_UTF8_DBF)
                {
                    // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode from UTF-8
                    char *pszString = CPLRecode(
                        papszValues[nIRecord], CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
                    if (MM_SecureCopyStringFieldValue(
                            &hMMFeature.pRecords[nIRecord]
                                 .pField[iField]
                                 .pDinValue,
                            pszString,
                            &hMMFeature.pRecords[nIRecord]
                                 .pField[iField]
                                 .nNumDinValue))
                    {
                        CPLFree(pszString);
                        return OGRERR_NOT_ENOUGH_MEMORY;
                    }
                    CPLFree(pszString);
                }
                else
                {
                    if (MM_SecureCopyStringFieldValue(
                            &hMMFeature.pRecords[nIRecord]
                                 .pField[iField]
                                 .pDinValue,
                            papszValues[nIRecord],
                            &hMMFeature.pRecords[nIRecord]
                                 .pField[iField]
                                 .nNumDinValue))
                        return OGRERR_NOT_ENOUGH_MEMORY;
                }
                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTIntegerList)
        {
            int nCount = 0;
            const int *panValues =
                poFeature->GetFieldAsIntegerList(iField, &nCount);

            nRealNumRecords = nNumRecords = nCount;
            if (nNumRecords == 0)
                nNumRecords++;
            hMMFeature.nNumMRecords =
                max_function(hMMFeature.nNumMRecords, nNumRecords);
            if (MMResizeMiraMonRecord(
                    &hMMFeature.pRecords, &hMMFeature.nMaxMRecords,
                    hMMFeature.nNumMRecords, MM_INC_NUMBER_OF_RECORDS,
                    hMMFeature.nNumMRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            // It will contains the i-th element of the list.
            for (nIRecord = 0; nIRecord < nRealNumRecords; nIRecord++)
            {
                if (MMResizeMiraMonFieldValue(
                        &(hMMFeature.pRecords[nIRecord].pField),
                        &hMMFeature.pRecords[nIRecord].nMaxField,
                        hMMFeature.pRecords[nIRecord].nNumField,
                        MM_INC_NUMBER_OF_FIELDS,
                        hMMFeature.pRecords[nIRecord].nNumField))
                    return OGRERR_NOT_ENOUGH_MEMORY;

                if (eFSType == OFSTBoolean)
                {
                    if (panValues[nIRecord] == 1)
                    {
                        if (MM_SecureCopyStringFieldValue(
                                &hMMFeature.pRecords[nIRecord]
                                     .pField[iField]
                                     .pDinValue,
                                "T",
                                &hMMFeature.pRecords[nIRecord]
                                     .pField[iField]
                                     .nNumDinValue))
                            return OGRERR_NOT_ENOUGH_MEMORY;
                    }
                    else
                    {
                        if (MM_SecureCopyStringFieldValue(
                                &hMMFeature.pRecords[nIRecord]
                                     .pField[iField]
                                     .pDinValue,
                                "F",
                                &hMMFeature.pRecords[nIRecord]
                                     .pField[iField]
                                     .nNumDinValue))
                            return OGRERR_NOT_ENOUGH_MEMORY;
                    }
                }
                else
                {
                    if (MM_SecureCopyStringFieldValue(
                            &hMMFeature.pRecords[nIRecord]
                                 .pField[iField]
                                 .pDinValue,
                            CPLSPrintf("%d", panValues[nIRecord]),
                            &hMMFeature.pRecords[nIRecord]
                                 .pField[iField]
                                 .nNumDinValue))
                        return OGRERR_NOT_ENOUGH_MEMORY;
                }

                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTInteger64List)
        {
            int nCount = 0;
            const GIntBig *panValues =
                poFeature->GetFieldAsInteger64List(iField, &nCount);

            nRealNumRecords = nNumRecords = nCount;
            if (nNumRecords == 0)
                nNumRecords++;
            hMMFeature.nNumMRecords =
                max_function(hMMFeature.nNumMRecords, nNumRecords);
            if (MMResizeMiraMonRecord(
                    &hMMFeature.pRecords, &hMMFeature.nMaxMRecords,
                    hMMFeature.nNumMRecords, MM_INC_NUMBER_OF_RECORDS,
                    hMMFeature.nNumMRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            // It will contains the i-th element of the list.
            for (nIRecord = 0; nIRecord < nRealNumRecords; nIRecord++)
            {
                if (MMResizeMiraMonFieldValue(
                        &(hMMFeature.pRecords[nIRecord].pField),
                        &hMMFeature.pRecords[nIRecord].nMaxField,
                        hMMFeature.pRecords[nIRecord].nNumField,
                        MM_INC_NUMBER_OF_FIELDS,
                        hMMFeature.pRecords[nIRecord].nNumField))
                    return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[nIRecord].pField[iField].iValue =
                    panValues[nIRecord];

                if (MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[nIRecord].pField[iField].pDinValue,
                        CPLSPrintf("%" CPL_FRMT_GB_WITHOUT_PREFIX "d",
                                   panValues[nIRecord]),
                        &hMMFeature.pRecords[nIRecord]
                             .pField[iField]
                             .nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTRealList)
        {
            int nCount = 0;
            const double *padfRLValues =
                poFeature->GetFieldAsDoubleList(iField, &nCount);
            //char format[23];

            nRealNumRecords = nNumRecords = nCount;
            if (nNumRecords == 0)
                nNumRecords++;
            hMMFeature.nNumMRecords =
                max_function(hMMFeature.nNumMRecords, nNumRecords);
            if (MMResizeMiraMonRecord(
                    &hMMFeature.pRecords, &hMMFeature.nMaxMRecords,
                    hMMFeature.nNumMRecords, MM_INC_NUMBER_OF_RECORDS,
                    hMMFeature.nNumMRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            // It will contains the i-th element of the list.
            for (nIRecord = 0; nIRecord < nRealNumRecords; nIRecord++)
            {
                if (MMResizeMiraMonFieldValue(
                        &(hMMFeature.pRecords[nIRecord].pField),
                        &hMMFeature.pRecords[nIRecord].nMaxField,
                        hMMFeature.pRecords[nIRecord].nNumField,
                        MM_INC_NUMBER_OF_FIELDS,
                        hMMFeature.pRecords[nIRecord].nNumField))
                    return OGRERR_NOT_ENOUGH_MEMORY;

                char szChain[MAX_SIZE_OF_FIELD_NUMBER_WITH_MINUS];
                MM_SprintfDoubleSignifFigures(
                    szChain, sizeof(szChain),
                    phMiraMonLayer->pLayerDB->pFields[iField].nNumberOfDecimals,
                    padfRLValues[nIRecord]);

                if (MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[nIRecord].pField[iField].pDinValue,
                        szChain,
                        &hMMFeature.pRecords[nIRecord]
                             .pField[iField]
                             .nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTString)
        {
            hMMFeature.nNumMRecords = max_function(hMMFeature.nNumMRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                                          &hMMFeature.pRecords[0].nMaxField,
                                          hMMFeature.pRecords[0].nNumField,
                                          MM_INC_NUMBER_OF_FIELDS,
                                          hMMFeature.pRecords[0].nNumField))
                return OGRERR_NOT_ENOUGH_MEMORY;

            if (MMIsEmptyString(pszRawValue))
                hMMFeature.pRecords[0].pField[iField].bIsValid = 0;
            {
                if (phMiraMonLayer->nCharSet != MM_JOC_CARAC_UTF8_DBF)
                {
                    // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode from UTF-8
                    char *pszString =
                        CPLRecode(pszRawValue, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
                    if (MM_SecureCopyStringFieldValue(
                            &hMMFeature.pRecords[0].pField[iField].pDinValue,
                            pszString,
                            &hMMFeature.pRecords[0]
                                 .pField[iField]
                                 .nNumDinValue))
                    {
                        CPLFree(pszString);
                        return OGRERR_NOT_ENOUGH_MEMORY;
                    }
                    CPLFree(pszString);
                }
                else
                {
                    if (MM_SecureCopyStringFieldValue(
                            &hMMFeature.pRecords[0].pField[iField].pDinValue,
                            pszRawValue,
                            &hMMFeature.pRecords[0]
                                 .pField[iField]
                                 .nNumDinValue))
                    {
                        return OGRERR_NOT_ENOUGH_MEMORY;
                    }
                }
            }
            hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
        }
        else if (eFType == OFTDate)
        {
            char szDate[15];

            hMMFeature.nNumMRecords = max_function(hMMFeature.nNumMRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                                          &hMMFeature.pRecords[0].nMaxField,
                                          hMMFeature.pRecords[0].nNumField,
                                          MM_INC_NUMBER_OF_FIELDS,
                                          hMMFeature.pRecords[0].nNumField))
                return OGRERR_NOT_ENOUGH_MEMORY;

            if (MMIsEmptyString(pszRawValue))
                hMMFeature.pRecords[0].pField[iField].bIsValid = 0;
            else
            {
                const OGRField *poField = poFeature->GetRawFieldRef(iField);
                if (poField->Date.Year >= 0)
                    snprintf(szDate, sizeof(szDate), "%04d%02d%02d",
                             poField->Date.Year, poField->Date.Month,
                             poField->Date.Day);
                else
                    snprintf(szDate, sizeof(szDate), "%04d%02d%02d", 0, 0, 0);

                if (MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[0].pField[iField].pDinValue,
                        szDate,
                        &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTTime || eFType == OFTDateTime)
        {
            hMMFeature.nNumMRecords = max_function(hMMFeature.nNumMRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                                          &hMMFeature.pRecords[0].nMaxField,
                                          hMMFeature.pRecords[0].nNumField,
                                          MM_INC_NUMBER_OF_FIELDS,
                                          hMMFeature.pRecords[0].nNumField))
                return OGRERR_NOT_ENOUGH_MEMORY;

            if (MMIsEmptyString(pszRawValue))
                hMMFeature.pRecords[0].pField[iField].bIsValid = 0;
            else
            {
                // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode from UTF-8
                if (MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[0].pField[iField].pDinValue,
                        pszRawValue,
                        &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTInteger)
        {
            hMMFeature.nNumMRecords = max_function(hMMFeature.nNumMRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                                          &hMMFeature.pRecords[0].nMaxField,
                                          hMMFeature.pRecords[0].nNumField,
                                          MM_INC_NUMBER_OF_FIELDS,
                                          hMMFeature.pRecords[0].nNumField))
                return OGRERR_NOT_ENOUGH_MEMORY;

            if (MMIsEmptyString(pszRawValue))
                hMMFeature.pRecords[0].pField[iField].bIsValid = 0;
            else
            {
                if (eFSType == OFSTBoolean)
                {
                    if (!strcmp(pszRawValue, "1"))
                    {
                        if (MM_SecureCopyStringFieldValue(
                                &hMMFeature.pRecords[0]
                                     .pField[iField]
                                     .pDinValue,
                                "T",
                                &hMMFeature.pRecords[0]
                                     .pField[iField]
                                     .nNumDinValue))
                            return OGRERR_NOT_ENOUGH_MEMORY;
                    }
                    else
                    {
                        if (MM_SecureCopyStringFieldValue(
                                &hMMFeature.pRecords[0]
                                     .pField[iField]
                                     .pDinValue,
                                "F",
                                &hMMFeature.pRecords[0]
                                     .pField[iField]
                                     .nNumDinValue))
                            return OGRERR_NOT_ENOUGH_MEMORY;
                    }
                }
                else
                {
                    if (MM_SecureCopyStringFieldValue(
                            &hMMFeature.pRecords[0].pField[iField].pDinValue,
                            pszRawValue,
                            &hMMFeature.pRecords[0]
                                 .pField[iField]
                                 .nNumDinValue))
                        return OGRERR_NOT_ENOUGH_MEMORY;
                }
                hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTInteger64)
        {
            hMMFeature.nNumMRecords = max_function(hMMFeature.nNumMRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                                          &hMMFeature.pRecords[0].nMaxField,
                                          hMMFeature.pRecords[0].nNumField,
                                          MM_INC_NUMBER_OF_FIELDS,
                                          hMMFeature.pRecords[0].nNumField))
                return OGRERR_NOT_ENOUGH_MEMORY;

            if (MMIsEmptyString(pszRawValue))
                hMMFeature.pRecords[0].pField[iField].bIsValid = 0;
            else
            {
                hMMFeature.pRecords[0].pField[iField].iValue =
                    poFeature->GetFieldAsInteger64(iField);

                if (MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[0].pField[iField].pDinValue,
                        pszRawValue,
                        &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTReal)
        {
            hMMFeature.nNumMRecords = max_function(hMMFeature.nNumMRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                                          &hMMFeature.pRecords[0].nMaxField,
                                          hMMFeature.pRecords[0].nNumField,
                                          MM_INC_NUMBER_OF_FIELDS,
                                          hMMFeature.pRecords[0].nNumField))
                return OGRERR_NOT_ENOUGH_MEMORY;

            if (MMIsEmptyString(pszRawValue))
                hMMFeature.pRecords[0].pField[iField].bIsValid = 0;
            else
            {
                char szChain[MAX_SIZE_OF_FIELD_NUMBER_WITH_MINUS];
                MM_SprintfDoubleSignifFigures(
                    szChain, sizeof(szChain),
                    phMiraMonLayer->pLayerDB->pFields[iField].nNumberOfDecimals,
                    poFeature->GetFieldAsDouble(iField));

                if (MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[0].pField[iField].pDinValue,
                        szChain,
                        &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "MiraMon: Field type %d not processed by MiraMon\n",
                     eFType);
            hMMFeature.pRecords[0].pField[iField].bIsValid = 0;
        }
    }

    return OGRERR_NONE;
}

/****************************************************************************/
/*                             GetLayerDefn()                               */
/*                                                                          */
/****************************************************************************/
OGRFeatureDefn *OGRMiraMonLayer::GetLayerDefn()
{
    return m_poFeatureDefn;
}

/****************************************************************************/
/*                             GetExtent()                                  */
/*                                                                          */
/*      Fetch extent of the data currently stored in the dataset.           */
/*      The bForce flag has no effect on SHO files since that value         */
/*      is always in the header.                                            */
/****************************************************************************/

OGRErr OGRMiraMonLayer::GetExtent(OGREnvelope *psExtent, int bForce)

{
    if (phMiraMonLayer)
    {
        if (phMiraMonLayer->bIsDBF)
            return OGRERR_FAILURE;

        // For polygons we need another polygon apart from the universal one
        // to have a valid extension
        if (phMiraMonLayer->bIsPolygon &&
            phMiraMonLayer->TopHeader.nElemCount < 1)
            return OGRERR_FAILURE;

        if (phMiraMonLayer->TopHeader.nElemCount < 1)
            return OGRERR_FAILURE;

        psExtent->MinX = phMiraMonLayer->TopHeader.hBB.dfMinX;
        psExtent->MaxX = phMiraMonLayer->TopHeader.hBB.dfMaxX;
        psExtent->MinY = phMiraMonLayer->TopHeader.hBB.dfMinY;
        psExtent->MaxY = phMiraMonLayer->TopHeader.hBB.dfMaxY;
    }
    else
    {
        if (!bForce)
            return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/****************************************************************************/
/*                           TestCapability()                               */
/****************************************************************************/

int OGRMiraMonLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;

    if (EQUAL(pszCap, OLCSequentialWrite))
        return m_bUpdate;

    if (EQUAL(pszCap, OLCFastFeatureCount))
        return !m_poFilterGeom && !m_poAttrQuery;

    if (EQUAL(pszCap, OLCFastGetExtent))
        return TRUE;

    if (EQUAL(pszCap, OLCCreateField))
        return m_bUpdate;

    if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;

    return FALSE;
}

/****************************************************************************/
/*                            CreateField()                                 */
/****************************************************************************/

OGRErr OGRMiraMonLayer::CreateField(const OGRFieldDefn *poField, int bApproxOK)

{
    if (!m_bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Cannot create fields on a read-only dataset.");
        return OGRERR_FAILURE;
    }

    if (phMiraMonLayer && phMiraMonLayer->TopHeader.nElemCount > 0)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Cannot create fields to a layer with "
                 "already existing features in it.");
        return OGRERR_FAILURE;
    }

    switch (poField->GetType())
    {
        case OFTInteger:
        case OFTIntegerList:
        case OFTInteger64:
        case OFTInteger64List:
        case OFTReal:
        case OFTRealList:
        case OFTString:
        case OFTStringList:
        case OFTDate:
            m_poFeatureDefn->AddFieldDefn(poField);
            return OGRERR_NONE;
        default:
            if (!bApproxOK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s is of an unsupported type: %s.",
                         poField->GetNameRef(),
                         poField->GetFieldTypeName(poField->GetType()));
                return OGRERR_FAILURE;
            }
            else
            {
                OGRFieldDefn oModDef(poField);
                oModDef.SetType(OFTString);
                m_poFeatureDefn->AddFieldDefn(poField);
                return OGRERR_NONE;
            }
    }
}

/************************************************************************/
/*                            AddToFileList()                           */
/************************************************************************/

void OGRMiraMonLayer::AddToFileList(CPLStringList &oFileList)
{
    if (!phMiraMonLayer)
        return;

    char szAuxFile[MM_CPL_PATH_BUF_SIZE];

    oFileList.AddStringDirectly(
        VSIGetCanonicalFilename(phMiraMonLayer->pszSrcLayerName));
    char *pszMMExt =
        CPLStrdup(CPLGetExtension(phMiraMonLayer->pszSrcLayerName));

    if (phMiraMonLayer->bIsPoint)
    {
        // As it's explicit on documentation a point has also two more files:

        // FILE_NAME_WITHOUT_EXTENSION.pnt --> FILE_NAME_WITHOUT_EXTENSION + T.rel
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "T.rel" : "T.REL",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.pnt --> FILE_NAME_WITHOUT_EXTENSION + T.dbf
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "T.dbf" : "T.DBF",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));
    }
    else if (phMiraMonLayer->bIsArc && !phMiraMonLayer->bIsPolygon)
    {
        // As it's explicit on documentation a point has also five more files:

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + A.rel
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'a') ? "A.rel" : "A.REL",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + A.dbf
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'a') ? "A.dbf" : "A.DBF",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + .nod
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'a') ? ".nod" : ".NOD",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + N.rel
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'a') ? "N.rel" : "N.REL",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + N.dbf
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'a') ? "N.dbf" : "N.DBF",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));
    }
    else if (phMiraMonLayer->bIsPolygon)
    {
        // As it's explicit on documentation a point has also eight more files:
        const char *szCompleteArcFileName;
        char szArcFileName[MM_CPL_PATH_BUF_SIZE];

        // FILE_NAME_WITHOUT_EXTENSION.pol --> FILE_NAME_WITHOUT_EXTENSION + P.rel
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "P.rel" : "P.REL",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));

        // The name of the arc is in THIS metadata file
        char *pszArcLayerName = MMReturnValueFromSectionINIFile(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr),
            SECTION_OVVW_ASPECTES_TECNICS, KEY_ArcSource);
        if (!pszArcLayerName)
        {
            CPLFree(pszMMExt);
            return;  //Some files are missing
        }
        CPLStrlcpy(szArcFileName, pszArcLayerName, MM_CPL_PATH_BUF_SIZE);

        MM_RemoveInitial_and_FinalQuotationMarks(szArcFileName);

        // If extension is not specified ".arc" will be used
        if (MMIsEmptyString(CPLGetExtension(pszArcLayerName)))
            CPLStrlcat(szArcFileName, (pszMMExt[0] == 'p') ? ".arc" : ".ARC",
                       MM_CPL_PATH_BUF_SIZE);

        CPLFree(pszArcLayerName);

        szCompleteArcFileName =
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szArcFileName, nullptr);

        // The arc that has the coordinates of the polygon
        oFileList.AddStringDirectly(
            VSIGetCanonicalFilename(szCompleteArcFileName));

        // FILE_NAME_WITHOUT_EXTENSION.pol --> FILE_NAME_WITHOUT_EXTENSION + P.dbf
        CPLStrlcpy(szAuxFile, CPLGetBasename(phMiraMonLayer->pszSrcLayerName),
                   MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "P.dbf" : "P.DBF",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(
            CPLFormFilename(CPLGetDirname(phMiraMonLayer->pszSrcLayerName),
                            szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + A.rel
        const char *pszBaseArcName = CPLGetBasename(szCompleteArcFileName);
        CPLStrlcpy(szAuxFile, pszBaseArcName, MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "A.rel" : "A.REL",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(CPLFormFilename(
            CPLGetDirname(szCompleteArcFileName), szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + A.dbf
        CPLStrlcpy(szAuxFile, pszBaseArcName, MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "A.dbf" : "A.DBF",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(CPLFormFilename(
            CPLGetDirname(szCompleteArcFileName), szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + .nod
        CPLStrlcpy(szAuxFile, pszBaseArcName, MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? ".nod" : ".NOD",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(CPLFormFilename(
            CPLGetDirname(szCompleteArcFileName), szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + N.rel
        CPLStrlcpy(szAuxFile, pszBaseArcName, MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "N.rel" : "N.REL",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(CPLFormFilename(
            CPLGetDirname(szCompleteArcFileName), szAuxFile, nullptr)));

        // FILE_NAME_WITHOUT_EXTENSION.arc --> FILE_NAME_WITHOUT_EXTENSION + N.dbf
        CPLStrlcpy(szAuxFile, pszBaseArcName, MM_CPL_PATH_BUF_SIZE);
        CPLStrlcat(szAuxFile, (pszMMExt[0] == 'p') ? "N.dbf" : "N.DBF",
                   MM_CPL_PATH_BUF_SIZE);
        oFileList.AddStringDirectly(VSIGetCanonicalFilename(CPLFormFilename(
            CPLGetDirname(szCompleteArcFileName), szAuxFile, nullptr)));
    }
    CPLFree(pszMMExt);
}
