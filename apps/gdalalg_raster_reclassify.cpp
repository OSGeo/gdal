/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reclassify" step of "raster pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_reclassify.h"

#include "cpl_vsi_virtual.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "../frmts/vrt/vrtdataset.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*    GDALRasterReclassifyAlgorithm::GDALRasterReclassifyAlgorithm()    */
/************************************************************************/

GDALRasterReclassifyAlgorithm::GDALRasterReclassifyAlgorithm(
    bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("mapping", 'm',
           _("Reclassification mappings (or specify a @<filename> to point to "
             "a file containing mappings"),
           &m_mapping)
        .SetRequired();
    AddOutputDataTypeArg(&m_type);
}

/************************************************************************/
/*              GDALRasterReclassifyCreateVRTDerived)                   */
/************************************************************************/

static std::unique_ptr<GDALDataset>
GDALReclassifyCreateVRTDerived(GDALDataset &input, const std::string &mappings,
                               GDALDataType eDstType)
{
    CPLXMLTreeCloser root(CPLCreateXMLNode(nullptr, CXT_Element, "VRTDataset"));

    const auto nX = input.GetRasterXSize();
    const auto nY = input.GetRasterYSize();

    for (int iBand = 1; iBand <= input.GetRasterCount(); ++iBand)
    {
        const GDALDataType bandType =
            eDstType == GDT_Unknown
                ? input.GetRasterBand(iBand)->GetRasterDataType()
                : eDstType;

        CPLXMLNode *band =
            CPLCreateXMLNode(root.get(), CXT_Element, "VRTRasterBand");
        CPLAddXMLAttributeAndValue(band, "subClass", "VRTDerivedRasterBand");
        CPLAddXMLAttributeAndValue(band, "dataType",
                                   GDALGetDataTypeName(bandType));

        CPLXMLNode *pixelFunctionType =
            CPLCreateXMLNode(band, CXT_Element, "PixelFunctionType");
        CPLCreateXMLNode(pixelFunctionType, CXT_Text, "reclassify");
        CPLXMLNode *arguments =
            CPLCreateXMLNode(band, CXT_Element, "PixelFunctionArguments");
        CPLAddXMLAttributeAndValue(arguments, "mapping", mappings.c_str());
    }

    auto ds = std::make_unique<VRTDataset>(nX, nY);
    if (ds->XMLInit(root.get(), "") != CE_None)
    {
        return nullptr;
    };

    for (int iBand = 1; iBand <= input.GetRasterCount(); ++iBand)
    {
        auto poSrcBand = input.GetRasterBand(iBand);
        auto poDstBand =
            cpl::down_cast<VRTDerivedRasterBand *>(ds->GetRasterBand(iBand));
        GDALCopyNoDataValue(poDstBand, poSrcBand);
        poDstBand->AddSimpleSource(poSrcBand);
    }

    std::array<double, 6> gt;
    if (input.GetGeoTransform(gt.data()) == CE_None)
        ds->SetGeoTransform(gt.data());
    ds->SetSpatialRef(input.GetSpatialRef());

    return ds;
}

/************************************************************************/
/*           GDALRasterReclassifyAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALRasterReclassifyAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    // Already validated by argument parser
    GDALDataType eDstType =
        m_type.empty() ? GDT_Unknown : GDALGetDataTypeByName(m_type.c_str());

    if (m_mapping.size() > 0 && m_mapping[0] == '@')
    {
        auto f =
            VSIVirtualHandleUniquePtr(VSIFOpenL(m_mapping.c_str() + 1, "r"));
        if (!f)
        {
            ReportError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                        m_mapping.c_str() + 1);
            return false;
        }

        std::string mappings_from_file;
        while (const char *line = CPLReadLineL(f.get()))
        {
            while (isspace(*line))
            {
                line++;
            }

            if (strlen(line) == 0)
            {
                continue;
            }

            if (!mappings_from_file.empty())
            {
                mappings_from_file.append(";");
            }

            char *comment = const_cast<char *>(strchr(line, '#'));
            if (comment != nullptr)
            {
                *comment = '\0';
            }

            mappings_from_file.append(line);
        }

        m_mapping = mappings_from_file;
    }

    auto vrt = GDALReclassifyCreateVRTDerived(*m_inputDataset.GetDatasetRef(),
                                              m_mapping, eDstType);

    if (vrt == nullptr)
    {
        return false;
    }

    m_outputDataset.Set(std::move(vrt));
    return true;
}

//! @endcond
