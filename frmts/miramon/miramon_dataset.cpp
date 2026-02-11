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
    poDriver->pfnIdentify = MMRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                             MMRDataset()                             */
/************************************************************************/

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

        MMRRasterBand *poBand =
            cpl::down_cast<MMRRasterBand *>(GetRasterBand(nIBand + 1));

        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return false;
        if (!pBand->GetFriendlyDescription().empty())
        {
            poBand->SetMetadataItem("DESCRIPTION",
                                    pBand->GetFriendlyDescription());
        }
    }
    // Some metadata items must be preserved just in case to be restored
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
