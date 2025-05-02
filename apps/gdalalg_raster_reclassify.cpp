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

#include <optional>

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
/*                      GetNoDataValueAsString                          */
/************************************************************************/

static std::optional<std::string> GetNoDataValueAsString(GDALRasterBand &band)
{
    int hasNoData;
    if (band.GetRasterDataType() == GDT_UInt64)
    {
        std::uint64_t noData = band.GetNoDataValueAsUInt64(&hasNoData);
        if (hasNoData)
        {
            return std::to_string(noData);
        }
    }
    else if (band.GetRasterDataType() == GDT_Int64)
    {
        std::int64_t noData = band.GetNoDataValueAsInt64(&hasNoData);
        if (hasNoData)
        {
            return std::to_string(noData);
        }
    }
    else
    {
        double noData = band.GetNoDataValue(&hasNoData);
        if (hasNoData)
        {
            return std::to_string(noData);
        }
    }

    return std::nullopt;
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

        CPLXMLNode *source =
            CPLCreateXMLNode(band, CXT_Element, "SimpleSource");

        CPLXMLNode *sourceFilename =
            CPLCreateXMLNode(source, CXT_Element, "SourceFilename");
        CPLAddXMLAttributeAndValue(sourceFilename, "relativeToVRT", "0");
        CPLCreateXMLNode(sourceFilename, CXT_Text, input.GetDescription());

        CPLXMLNode *sourceBand =
            CPLCreateXMLNode(source, CXT_Element, "SourceBand");
        CPLCreateXMLNode(sourceBand, CXT_Text, std::to_string(iBand).c_str());

        CPLXMLNode *srcRect = CPLCreateXMLNode(source, CXT_Element, "SrcRect");
        CPLAddXMLAttributeAndValue(srcRect, "xOff", "0");
        CPLAddXMLAttributeAndValue(srcRect, "yOff", "0");
        CPLAddXMLAttributeAndValue(srcRect, "xSize",
                                   std::to_string(nX).c_str());
        CPLAddXMLAttributeAndValue(srcRect, "ySize",
                                   std::to_string(nY).c_str());

        CPLXMLNode *dstRect = CPLCreateXMLNode(source, CXT_Element, "DstRect");
        CPLAddXMLAttributeAndValue(dstRect, "xOff", "0");
        CPLAddXMLAttributeAndValue(dstRect, "yOff", "0");
        CPLAddXMLAttributeAndValue(dstRect, "xSize",
                                   std::to_string(nX).c_str());
        CPLAddXMLAttributeAndValue(dstRect, "ySize",
                                   std::to_string(nY).c_str());

        auto noData = GetNoDataValueAsString(*input.GetRasterBand(iBand));
        if (noData.has_value())
        {
            CPLXMLNode *noDataNode =
                CPLCreateXMLNode(band, CXT_Element, "NoDataValue");
            CPLCreateXMLNode(noDataNode, CXT_Text, noData.value().c_str());
        }
    }

    auto ds = std::make_unique<VRTDataset>(nX, nY);
    if (ds->XMLInit(root.get(), "") != CE_None)
    {
        return nullptr;
    };

    std::array<double, 6> gt;
    input.GetGeoTransform(gt.data());
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
