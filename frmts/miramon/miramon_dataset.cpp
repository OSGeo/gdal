/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRDataset class: responsible for generating the
 *           main dataset or the subdatasets as needed.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <algorithm>
#include <cassert>

#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

#include "gdal_frmts.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

/************************************************************************/
/*                        GDALRegister_MiraMon()                        */
/************************************************************************/
void GDALRegister_MiraMon()

{
    if (GDALGetDriverByName("MiraMonRaster") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MiraMonRaster");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MiraMon Raster Images");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/miramon.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "rel img");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='COMPRESS' type='boolean' description='Indicates  "
        "whether the file will be compressed in RLE indexed mode'/>"
        "   <Option name='PATTERN' type='int' description='Indicates the "
        "pattern used to create the names of the different bands. In the "
        "case of RGB, the suffixes “_R”, “_G”, and “_B” will be added to "
        "the base name.'/>"
        "   <Option name='CATEGORICAL_BANDS' type='string' "
        "description='Indicates "
        "which bands have to be treat as categorical.'/>"
        "   <Option name='CONTINUOUS_BANDS' type='string' "
        "description='Indicates "
        "which bands have to be treat as continuous.'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>\n"
        "   <Option name='RAT_OR_CT' type='string-select' "
        "description='Controls whether the Raster Attribute Table (RAT) "
        "and/or the Color Table (CT) are exposed.' default='ALL'>\n"
        "       <Value>ALL</Value>\n"
        "       <Value>RAT</Value>\n"
        "       <Value>CT</Value>\n"
        "   </Option>\n"
        "</OpenOptionList>\n");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnCreateCopy = MMRDataset::CreateCopy;
    poDriver->pfnIdentify = MMRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                             MMRDataset()                             */
/************************************************************************/
MMRDataset::MMRDataset(GDALProgressFunc pfnProgress, void *pProgressData,
                       CSLConstList papszOptions, CPLString osRelname,
                       GDALDataset &oSrcDS, const CPLString &osUsrPattern,
                       const CPLString &osPattern)
    : m_bIsValid(false)
{
    nBands = oSrcDS.GetRasterCount();
    if (nBands == 0)
    {
        ReportError(osRelname, CE_Failure, CPLE_AppDefined,
                    "Unable to translate to MiraMon files with zero bands.");
        return;
    }

    UpdateProjection(oSrcDS);

    // Getting bands information and creating MMRBand objects.
    // Also checking if all bands have the same dimensions.
    bool bNeedOfNomFitxer = (nBands > 1 || !osUsrPattern.empty());

    std::vector<MMRBand> oBands{};
    oBands.reserve(nBands);

    bool bAllBandsSameDim = true;
    for (int nIBand = 0; nIBand < nBands; nIBand++)
    {
        GDALRasterBand *pRasterBand = oSrcDS.GetRasterBand(nIBand + 1);
        if (!pRasterBand)
        {
            ReportError(
                osRelname, CE_Failure, CPLE_AppDefined,
                "Unable to translate the band %d to MiraMon. Process canceled.",
                nIBand);
            return;
        }

        // Detection of the index of the band in the RGB composition (if it applies).
        CPLString osIndexBand;
        CPLString osNumberIndexBand;
        if (pRasterBand->GetColorInterpretation() == GCI_RedBand)
        {
            osIndexBand = "R";
            m_nIBandR = nIBand;
        }
        else if (pRasterBand->GetColorInterpretation() == GCI_GreenBand)
        {
            osIndexBand = "G";
            m_nIBandG = nIBand;
        }
        else if (pRasterBand->GetColorInterpretation() == GCI_BlueBand)
        {
            osIndexBand = "B";
            m_nIBandB = nIBand;
        }
        else if (pRasterBand->GetColorInterpretation() == GCI_AlphaBand)
            osIndexBand = "Alpha";
        else
            osIndexBand = CPLSPrintf("%d", nIBand + 1);

        osNumberIndexBand = CPLSPrintf("%d", nIBand + 1);
        bool bCategorical = IsCategoricalBand(oSrcDS, *pRasterBand,
                                              papszOptions, osNumberIndexBand);

        bool bCompressDS =
            EQUAL(CSLFetchNameValueDef(papszOptions, "COMPRESS", "YES"), "YES");

        // Emplace back a MMRBand
        oBands.emplace_back(pfnProgress, pProgressData, oSrcDS, nIBand,
                            CPLGetPathSafe(osRelname), *pRasterBand,
                            bCompressDS, bCategorical, osPattern, osIndexBand,
                            bNeedOfNomFitxer);
        if (!oBands.back().IsValid())
        {
            ReportError(
                osRelname, CE_Failure, CPLE_AppDefined,
                "Unable to translate the band %d to MiraMon. Process canceled.",
                nIBand);
            return;
        }
        if (nIBand == 0)
        {
            m_nWidth = oBands.back().GetWidth();
            m_nHeight = oBands.back().GetHeight();
        }
        else if (m_nWidth != oBands.back().GetWidth() ||
                 m_nHeight != oBands.back().GetHeight())
        {
            bAllBandsSameDim = false;
        }
    }

    // Getting number of columns and rows
    if (!bAllBandsSameDim)
    {
        // It's not an error. MiraMon have Datasets
        // with dimensions for each band
        m_nWidth = 0;
        m_nHeight = 0;
    }
    else
    {
        // Getting geotransform
        GDALGeoTransform gt;
        if (oSrcDS.GetGeoTransform(gt) == CE_None)
        {
            m_dfMinX = gt[0];
            m_dfMaxY = gt[3];
            m_dfMaxX = m_dfMinX + m_nWidth * gt[1];
            m_dfMinY = m_dfMaxY + m_nHeight * gt[5];
        }
    }

    // Creating the MMRRel object with all the information of the dataset.
    m_pMMRRel = std::make_unique<MMRRel>(
        osRelname, bNeedOfNomFitxer, m_osEPSG, m_nWidth, m_nHeight, m_dfMinX,
        m_dfMaxX, m_dfMinY, m_dfMaxY, std::move(oBands));

    if (!m_pMMRRel->IsValid())
        return;

    // Lineage is updated with the source dataset information, if any, and with the creation options.
    m_pMMRRel->UpdateLineage(papszOptions, oSrcDS);

    // Writing all information in files: I.rel, IMG,...
    if (!m_pMMRRel->Write(oSrcDS))
    {
        m_pMMRRel->SetIsValid(false);
        return;
    }

    // If the dataset is RGB, we write the .mmm file with the RGB information of the bands.
    WriteRGBMap();

    m_bIsValid = true;
}

MMRDataset::MMRDataset(GDALOpenInfo *poOpenInfo)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // Creating the class MMRRel.
    auto pMMfRel = std::make_unique<MMRRel>(poOpenInfo->pszFilename, true);
    if (!pMMfRel->IsValid())
    {
        if (pMMfRel->isAMiraMonFile())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to open %s, probably it's not a MiraMon file.",
                     poOpenInfo->pszFilename);
        }
        return;
    }

    if (pMMfRel->GetNBands() == 0)
    {
        if (pMMfRel->isAMiraMonFile())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to open %s, it has zero usable bands.",
                     poOpenInfo->pszFilename);
        }
        return;
    }

    m_pMMRRel = std::move(pMMfRel);

    // General Dataset information available
    nRasterXSize = m_pMMRRel->GetColumnsNumberFromREL();
    nRasterYSize = m_pMMRRel->GetRowsNumberFromREL();
    ReadProjection();
    nBands = 0;

    // Getting the open option that determines how to expose subdatasets.
    // To avoid recusivity subdatasets are exposed as they are.
    const char *pszDataType =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "RAT_OR_CT");
    if (pszDataType != nullptr)
    {
        if (EQUAL(pszDataType, "RAT"))
            nRatOrCT = RAT_OR_CT::RAT;
        else if (EQUAL(pszDataType, "ALL"))
            nRatOrCT = RAT_OR_CT::ALL;
        else if (EQUAL(pszDataType, "CT"))
            nRatOrCT = RAT_OR_CT::CT;
    }

    AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (m_nNSubdataSets)
    {
        CreateSubdatasetsFromBands();
        // Fills adfGeoTransform if documented
        UpdateGeoTransform();
    }
    else
    {
        if (!CreateRasterBands())
            return;

        // GeoTransform of a subdataset is always the same than the first band
        if (m_pMMRRel->GetNBands() >= 1)
        {
            MMRBand *poBand = m_pMMRRel->GetBand(m_pMMRRel->GetNBands() - 1);
            if (poBand)
                m_gt = poBand->m_gt;
        }
    }

    // Make sure we don't try to do any pam stuff with this dataset.
    nPamFlags |= GPF_NOSAVE;

    // We have a valid DataSet.
    m_bIsValid = true;
}

/************************************************************************/
/*                            ~MMRDataset()                             */
/************************************************************************/

MMRDataset::~MMRDataset()

{
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int MMRDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    // Checking for subdataset
    int nIdentifyResult =
        MMRRel::IdentifySubdataSetFile(poOpenInfo->pszFilename);
    if (nIdentifyResult != GDAL_IDENTIFY_FALSE)
        return nIdentifyResult;

    // Checking for MiraMon raster file
    return MMRRel::IdentifyFile(poOpenInfo);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *MMRDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Verify that this is a MMR file.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The MiraMonRaster driver does not support update "
                 "access to existing datasets.");
        return nullptr;
    }

    // Create the Dataset (with bands or Subdatasets).
    auto poDS = std::make_unique<MMRDataset>(poOpenInfo);
    if (!poDS->IsValid())
        return nullptr;

    // Set description
    poDS->SetDescription(poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/
GDALDataset *MMRDataset::CreateCopy(const char *pszFilename,
                                    GDALDataset *poSrcDS, int /*bStrict*/,
                                    CSLConstList papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData)

{
    // pszFilename doesn't have extension or must end in "I.rel"
    const CPLString osFileName(pszFilename);
    CPLString osRelName = CreateAssociatedMetadataFileName(osFileName);
    if (osRelName.empty())
        return nullptr;

    // osPattern is needed to create band names.
    CPLString osUsrPattern = CSLFetchNameValueDef(papszOptions, "PATTERN", "");
    CPLString osPattern = CreatePatternFileName(osRelName, osUsrPattern);

    if (osPattern.empty())
        return nullptr;

    auto poDS = std::make_unique<MMRDataset>(pfnProgress, pProgressData,
                                             papszOptions, osRelName, *poSrcDS,
                                             osUsrPattern, osPattern);

    if (!poDS->IsValid())
        return nullptr;

    poDS->SetDescription(pszFilename);
    poDS->eAccess = GA_Update;

    return poDS.release();
}

bool MMRDataset::CreateRasterBands()
{
    MMRBand *pBand;

    for (int nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
    {
        // Establish raster band info.
        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return false;
        nRasterXSize = pBand->GetWidth();
        nRasterYSize = pBand->GetHeight();
        pBand->UpdateGeoTransform();  // Fills adfGeoTransform for this band

        auto poRasterBand = std::make_unique<MMRRasterBand>(this, nBands + 1);
        if (!poRasterBand->IsValid())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create a RasterBand from '%s'",
                     m_pMMRRel->GetRELNameChar());

            return false;
        }

        SetBand(nBands + 1, std::move(poRasterBand));
    }
    // Not used metadata in the REL must be preserved just in case to be restored
    // if they are preserved through translations.
    m_pMMRRel->RELToGDALMetadata(this);

    return true;
}

void MMRDataset::ReadProjection()

{
    if (!m_pMMRRel)
        return;

    CPLString osSRS;
    if (!m_pMMRRel->GetMetadataValue("SPATIAL_REFERENCE_SYSTEM:HORIZONTAL",
                                     "HorizontalSystemIdentifier", osSRS) ||
        osSRS.empty())
        return;

    char szResult[MM_MAX_ID_SNY + 10];
    int nResult = ReturnEPSGCodeSRSFromMMIDSRS(osSRS.c_str(), szResult);
    if (nResult == 1 || szResult[0] == '\0')
        return;

    int nEPSG;
    if (1 == sscanf(szResult, "%d", &nEPSG))
        m_oSRS.importFromEPSG(nEPSG);

    return;
}

void MMRDataset::UpdateProjection(GDALDataset &oSrcDS)
{
    const OGRSpatialReference *poSRS = oSrcDS.GetSpatialRef();
    if (poSRS)
    {
        const char *pszTargetKey = nullptr;
        const char *pszAuthorityName = nullptr;
        const char *pszAuthorityCode = nullptr;

        // Reading horizontal reference system and horizontal units
        if (poSRS->IsProjected())
            pszTargetKey = "PROJCS";
        else if (poSRS->IsGeographic() || poSRS->IsDerivedGeographic())
            pszTargetKey = "GEOGCS";
        else if (poSRS->IsGeocentric())
            pszTargetKey = "GEOCCS";
        else if (poSRS->IsLocal())
            pszTargetKey = "LOCAL_CS";

        if (!poSRS->IsLocal())
        {
            pszAuthorityName = poSRS->GetAuthorityName(pszTargetKey);
            pszAuthorityCode = poSRS->GetAuthorityCode(pszTargetKey);
        }

        if (pszAuthorityName && pszAuthorityCode &&
            EQUAL(pszAuthorityName, "EPSG"))
        {
            CPLDebugOnly("MiraMon", "Setting EPSG code %s", pszAuthorityCode);
            m_osEPSG = pszAuthorityCode;
        }
    }
}

/************************************************************************/
/*                             SUBDATASETS                              */
/************************************************************************/
// Assigns every band to a subdataset
void MMRDataset::AssignBandsToSubdataSets()
{
    m_nNSubdataSets = 0;
    if (!m_pMMRRel.get())
        return;

    int nIBand = 0;
    int nIBand2 = 0;

    MMRBand *pBand;
    MMRBand *pOtherBand;
    for (; nIBand < m_pMMRRel->GetNBands(); nIBand++)
    {
        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            continue;

        if (pBand->GetAssignedSubDataSet() != 0)
            continue;

        m_nNSubdataSets++;
        pBand->AssignSubDataSet(m_nNSubdataSets);

        // Let's put all suitable bands in the same subdataset
        for (nIBand2 = nIBand + 1; nIBand2 < m_pMMRRel->GetNBands(); nIBand2++)
        {
            pOtherBand = m_pMMRRel->GetBand(nIBand2);
            if (!pOtherBand)
                continue;

            if (pOtherBand->GetAssignedSubDataSet() != 0)
                continue;

            if (BandInTheSameDataset(nIBand, nIBand2))
                pOtherBand->AssignSubDataSet(m_nNSubdataSets);
        }
    }

    // If there is only one subdataset, it means that
    // we don't need subdatasets (all assigned to 0)
    if (m_nNSubdataSets == 1)
    {
        m_nNSubdataSets = 0;
        for (nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                break;
            pBand->AssignSubDataSet(m_nNSubdataSets);
        }
    }
}

void MMRDataset::CreateSubdatasetsFromBands()
{
    CPLStringList oSubdatasetList;
    CPLString osDSName;
    CPLString osDSDesc;
    MMRBand *pBand;

    for (int iSubdataset = 1; iSubdataset <= m_nNSubdataSets; iSubdataset++)
    {
        int nIBand;
        for (nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                return;
            if (pBand->GetAssignedSubDataSet() == iSubdataset)
                break;
        }

        if (nIBand == m_pMMRRel->GetNBands())
            break;

        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return;

        osDSName.Printf("MiraMonRaster:\"%s\",\"%s\"",
                        pBand->GetRELFileName().c_str(),
                        pBand->GetRawBandFileName().c_str());
        osDSDesc.Printf("Subdataset %d: \"%s\"", iSubdataset,
                        pBand->GetBandName().c_str());
        nIBand++;

        for (; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                return;
            if (pBand->GetAssignedSubDataSet() != iSubdataset)
                continue;

            osDSName.append(
                CPLSPrintf(",\"%s\"", pBand->GetRawBandFileName().c_str()));
            osDSDesc.append(
                CPLSPrintf(",\"%s\"", pBand->GetBandName().c_str()));
        }

        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubdataset), osDSName);
        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubdataset), osDSDesc);
    }

    if (oSubdatasetList.Count() > 0)
    {
        // Add metadata to the main dataset
        SetMetadata(oSubdatasetList.List(), "SUBDATASETS");
        oSubdatasetList.Clear();
    }
}

// Checks if two bands should be in the same subdataset
bool MMRDataset::BandInTheSameDataset(int nIBand1, int nIBand2) const
{
    if (nIBand1 < 0 || nIBand2 < 0)
        return true;

    if (nIBand1 >= m_pMMRRel->GetNBands() || nIBand2 >= m_pMMRRel->GetNBands())
        return true;

    MMRBand *pThisBand = m_pMMRRel->GetBand(nIBand1);
    MMRBand *pOtherBand = m_pMMRRel->GetBand(nIBand2);
    if (!pThisBand || !pOtherBand)
        return true;

    // Two images with different numbers of columns are assigned to different subdatasets
    if (pThisBand->GetWidth() != pOtherBand->GetWidth())
        return false;

    // Two images with different numbers of rows are assigned to different subdatasets
    if (pThisBand->GetHeight() != pOtherBand->GetHeight())
        return false;

    // Two images with different data type are assigned to different subdatasets
    if (pThisBand->GeteMMNCDataType() != pOtherBand->GeteMMNCDataType())
        return false;

    // Two images with different bounding box are assigned to different subdatasets
    if (pThisBand->GetBoundingBoxMinX() != pOtherBand->GetBoundingBoxMinX())
        return false;
    if (pThisBand->GetBoundingBoxMaxX() != pOtherBand->GetBoundingBoxMaxX())
        return false;
    if (pThisBand->GetBoundingBoxMinY() != pOtherBand->GetBoundingBoxMinY())
        return false;
    if (pThisBand->GetBoundingBoxMaxY() != pOtherBand->GetBoundingBoxMaxY())
        return false;

    // Two images with different simbolization are assigned to different subdatasets
    if (!EQUAL(pThisBand->GetColor_Const(), pOtherBand->GetColor_Const()))
        return false;
    if (pThisBand->GetConstantColorRGB().c1 !=
        pOtherBand->GetConstantColorRGB().c1)
        return false;
    if (pThisBand->GetConstantColorRGB().c2 !=
        pOtherBand->GetConstantColorRGB().c2)
        return false;
    if (pThisBand->GetConstantColorRGB().c3 !=
        pOtherBand->GetConstantColorRGB().c3)
        return false;
    if (!EQUAL(pThisBand->GetColor_Paleta(), pOtherBand->GetColor_Paleta()))
        return false;
    if (!EQUAL(pThisBand->GetColor_TractamentVariable(),
               pOtherBand->GetColor_TractamentVariable()))
        return false;
    if (!EQUAL(pThisBand->GetTractamentVariable(),
               pOtherBand->GetTractamentVariable()))
        return false;
    if (!EQUAL(pThisBand->GetColor_EscalatColor(),
               pOtherBand->GetColor_EscalatColor()))
        return false;
    if (!EQUAL(pThisBand->GetColor_N_SimbolsALaTaula(),
               pOtherBand->GetColor_N_SimbolsALaTaula()))
        return false;
    if (pThisBand->IsCategorical() != pOtherBand->IsCategorical())
        return false;
    if (pThisBand->IsCategorical())
    {
        if (pThisBand->GetMaxSet() != pOtherBand->GetMaxSet())
            return false;
        if (pThisBand->GetMaxSet())
        {
            if (pThisBand->GetMax() != pOtherBand->GetMax())
                return false;
        }
    }

    // Two images with different RATs are assigned to different subdatasets
    if (!EQUAL(pThisBand->GetShortRATName(), pOtherBand->GetShortRATName()) ||
        !EQUAL(pThisBand->GetAssociateREL(), pOtherBand->GetAssociateREL()))
        return false;

    // One image has NoData values and the other does not;
    // they are assigned to different subdatasets
    if (pThisBand->BandHasNoData() != pOtherBand->BandHasNoData())
        return false;

    // Two images with different NoData values are assigned to different subdatasets
    if (pThisBand->GetNoDataValue() != pOtherBand->GetNoDataValue())
        return false;

    return true;
}

/************************************************************************/
/*                         UpdateGeoTransform()                         */
/************************************************************************/
int MMRDataset::UpdateGeoTransform()
{
    // Bounding box of the band
    // Section [EXTENT] in rel file

    if (!m_pMMRRel)
        return 1;

    CPLString osMinX;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MinX", osMinX) ||
        osMinX.empty())
        return 1;

    if (1 != CPLsscanf(osMinX, "%lf", &(m_gt.xorig)))
        m_gt.xorig = 0.0;

    int nNCols = m_pMMRRel->GetColumnsNumberFromREL();
    if (nNCols <= 0)
        return 1;

    CPLString osMaxX;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MaxX", osMaxX) ||
        osMaxX.empty())
        return 1;

    double dfMaxX;
    if (1 != CPLsscanf(osMaxX, "%lf", &dfMaxX))
        dfMaxX = 1.0;

    m_gt.xscale = (dfMaxX - m_gt.xorig) / nNCols;
    m_gt.xrot = 0.0;  // No rotation in MiraMon rasters

    CPLString osMinY;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MinY", osMinY) ||
        osMinY.empty())
        return 1;

    CPLString osMaxY;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MaxY", osMaxY) ||
        osMaxY.empty())
        return 1;

    int nNRows = m_pMMRRel->GetRowsNumberFromREL();
    if (nNRows <= 0)
        return 1;

    double dfMaxY;
    if (1 != CPLsscanf(osMaxY, "%lf", &dfMaxY))
        dfMaxY = 1.0;

    m_gt.yorig = dfMaxY;
    m_gt.yrot = 0.0;

    double dfMinY;
    if (1 != CPLsscanf(osMinY, "%lf", &dfMinY))
        dfMinY = 0.0;
    m_gt.yscale = (dfMinY - m_gt.yorig) / nNRows;

    return 0;
}

const OGRSpatialReference *MMRDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

CPLErr MMRDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    if (m_gt.xorig != 0.0 || m_gt.xscale != 1.0 || m_gt.xrot != 0.0 ||
        m_gt.yorig != 0.0 || m_gt.yrot != 0.0 || m_gt.yscale != 1.0)
    {
        gt = m_gt;
        return CE_None;
    }

    return GDALDataset::GetGeoTransform(gt);
}

/************************************************************************/
/*                            REL/IMG names                             */
/************************************************************************/

// Finds the metadata filename associated to osFileName (usually an IMG file)
CPLString
MMRDataset::CreateAssociatedMetadataFileName(const CPLString &osFileName)
{
    if (osFileName.empty())
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Expected output file name.");
        return "";
    }

    CPLString osRELName = osFileName;

    // If the string finishes in "I.rel" we consider it can be
    // the associated file to all bands that are documented in this file.
    if (cpl::ends_with(osFileName, pszExtRasterREL))
        return osRELName;

    // If the string finishes in ".img" or ".rel" (and not "I.rel")
    // we consider it can converted to "I.rel"
    if (cpl::ends_with(osFileName, pszExtRaster) ||
        cpl::ends_with(osFileName, pszExtREL))
    {
        // Extract extension
        osRELName = CPLResetExtensionSafe(osRELName, "");

        if (!osRELName.length())
            return "";

        // Extract "."
        osRELName.resize(osRELName.size() - 1);

        if (!osRELName.length())
            return "";

        // Add "I.rel"
        osRELName += pszExtRasterREL;
        return osRELName;
    }

    // If the file is not a REL file, let's assume that "I.rel" can be added
    // to get the REL file.
    osRELName += pszExtRasterREL;
    return osRELName;
}

// Finds the pattern name to the bands
CPLString MMRDataset::CreatePatternFileName(const CPLString &osFileName,
                                            const CPLString &osPattern)
{
    if (!osPattern.empty())
        return osPattern;

    CPLString osRELName = osFileName;

    if (!cpl::ends_with(osFileName, pszExtRasterREL))
        return "";

    // Extract I.rel and path
    osRELName.resize(osRELName.size() - strlen("I.rel"));
    return CPLGetBasenameSafe(osRELName);
}

// Checks if the band is in the list of categorical or continuous bands
// specified by the user in the creation options.
bool MMRDataset::BandInOptionsList(CSLConstList papszOptions,
                                   const CPLString &pszType,
                                   const CPLString &osIndexBand)
{
    if (!papszOptions)
        return false;

    if (const char *pszCategoricalList =
            CSLFetchNameValue(papszOptions, pszType))
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(pszCategoricalList, ",", 0));

        for (int i = 0; i < aosTokens.size(); ++i)
        {
            if (EQUAL(aosTokens[i], osIndexBand))
                return true;
        }
    }
    return false;
}

// MiraMon needs to know if the band is categorical or continuous to apply the right default symbology.
bool MMRDataset::IsCategoricalBand(GDALDataset &oSrcDS,
                                   GDALRasterBand &pRasterBand,
                                   CSLConstList papszOptions,
                                   const CPLString &osIndexBand)
{
    bool bUsrCategorical =
        BandInOptionsList(papszOptions, "CATEGORICAL_BANDS", osIndexBand);
    bool bUsrContinuous =
        BandInOptionsList(papszOptions, "CONTINUOUS_BANDS", osIndexBand);

    if (!bUsrCategorical && !bUsrContinuous)
    {
        // In case user doesn't specify anything, we try to deduce if the band is categorical or continuous
        // First we try to see if there is metadata in the source dataset that can help us. We look for a key like
        // "ATTRIBUTE_DATA$$$TractamentVariable" in the domain "MIRAMON"
        CPLStringList aosMiraMonMetaData(oSrcDS.GetMetadata(MetadataDomain));
        if (!aosMiraMonMetaData.empty())
        {
            CPLString osClue = CPLSPrintf("ATTRIBUTE_DATA%sTractamentVariable",
                                          SecKeySeparator);
            CPLString osTractamentVariable =
                CSLFetchNameValueDef(aosMiraMonMetaData, osClue, "");
            if (!osTractamentVariable.empty())
            {
                if (EQUAL(osTractamentVariable, "Categorical"))
                    return true;
                return false;
            }
        }

        // In case of no metadata, we try to deduce if the band is categorical or continuous with some heuristics:
        if (pRasterBand.GetCategoryNames() != nullptr)
            return true;

        // In case of floating point data, we consider that the band is continuous.
        if (pRasterBand.GetRasterDataType() == GDT_Float32 ||
            pRasterBand.GetRasterDataType() == GDT_Float64)
            return false;

        // In case of 8 bit integer with a color table, we consider that the band is categorical.
        if ((pRasterBand.GetRasterDataType() == GDT_UInt8 ||
             pRasterBand.GetRasterDataType() == GDT_Int8) &&
            pRasterBand.GetColorTable() != nullptr)
            return true;

        // In case of the band has a RAT, we consider that the band is categorical.
        // This is a heuristic that can be wrong in some cases, but in general if
        // a band has a RAT it's because it has a limited number of values and they are categorical.
        if (pRasterBand.GetDefaultRAT() != nullptr)
            return true;
    }
    else if (bUsrCategorical && bUsrContinuous)
    {
        // User cannot impose both categorical and continuous treatment
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 "Unable to interpret band as Categorical and Continuous at "
                 "the same time. Categorical treatment will be used.");

        return true;
    }
    else if (bUsrCategorical)
        return true;

    return false;
}

// In the RGB case, a map (.mmm) is generated with the RGB information of the bands.
// This allows to visualize the RGB composition in MiraMon without having to create
// a map in MiraMon and set the RGB information.
void MMRDataset::WriteRGBMap()
{
    if (m_nIBandR == -1 || m_nIBandG == -1 || m_nIBandB == -1)
        return;

    CPLString osMapNameAux = m_pMMRRel->GetRELName();
    CPLString osMapNameAux2 =
        m_pMMRRel->MMRGetFileNameFromRelName(osMapNameAux, ".mmm");

    auto pMMMap = std::make_unique<MMRRel>(osMapNameAux2);
    if (!pMMMap->OpenRELFile("wb"))
        return;

    pMMMap->AddSectionStart(SECTION_VERSIO);
    pMMMap->AddKeyValue(KEY_Vers, "2");
    pMMMap->AddKeyValue(KEY_SubVers, "0");
    pMMMap->AddKeyValue("variant", "b");
    pMMMap->AddSectionEnd();

    pMMMap->AddSectionStart("DOCUMENT");
    pMMMap->AddKeyValue("Titol", CPLGetBasenameSafe(osMapNameAux2));
    pMMMap->AddSectionEnd();

    pMMMap->AddSectionStart("VISTA");
    pMMMap->AddKeyValue("ordre", "RASTER_RGB_1");
    pMMMap->AddSectionEnd();

    pMMMap->AddSectionStart("RASTER_RGB_1");
    auto poRedBand = m_pMMRRel->GetBand(m_nIBandR);
    auto poGreenBand = m_pMMRRel->GetBand(m_nIBandG);
    auto poBlueBand = m_pMMRRel->GetBand(m_nIBandB);
    assert(poRedBand);
    assert(poGreenBand);
    assert(poBlueBand);
    pMMMap->AddKeyValue("FitxerR",
                        CPLGetFilename(poRedBand->GetRawBandFileName()));
    pMMMap->AddKeyValue("FitxerG",
                        CPLGetFilename(poGreenBand->GetRawBandFileName()));
    pMMMap->AddKeyValue("FitxerB",
                        CPLGetFilename(poBlueBand->GetRawBandFileName()));
    pMMMap->AddKeyValue("UnificVisCons", "1");
    pMMMap->AddKeyValue("visualitzable", "1");
    pMMMap->AddKeyValue("consultable", "1");
    pMMMap->AddKeyValue("EscalaMaxima", "0");
    pMMMap->AddKeyValue("EscalaMinima", "900000000");
    pMMMap->AddKeyValue("LlegSimb_Vers", "4");
    pMMMap->AddKeyValue("LlegSimb_SubVers", "5");
    pMMMap->AddKeyValue("Color_VisibleALleg", "1");
    CPLString osLlegTitle = "RGB:";
    osLlegTitle.append(CPLGetFilename(poRedBand->GetBandName()));
    osLlegTitle.append("+");
    osLlegTitle.append(CPLGetFilename(poGreenBand->GetBandName()));
    osLlegTitle.append("+");
    osLlegTitle.append(CPLGetFilename(poBlueBand->GetBandName()));
    pMMMap->AddKeyValue("Color_TitolLlegenda", osLlegTitle);
    pMMMap->AddSectionEnd();
}
