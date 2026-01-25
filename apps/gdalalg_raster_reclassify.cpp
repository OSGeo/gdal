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
#include "../frmts/vrt/vrtreclassifier.h"

#include <array>

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
/*                 GDALRasterReclassifyValidateMappings                 */
/************************************************************************/

static bool GDALReclassifyValidateMappings(GDALDataset &input,
                                           const std::string &mappings,
                                           GDALDataType eDstType)
{
    int hasNoData;
    std::optional<double> noData =
        input.GetRasterBand(1)->GetNoDataValue(&hasNoData);
    if (!hasNoData)
    {
        noData.reset();
    }

    gdal::Reclassifier reclassifier;
    return reclassifier.Init(mappings.c_str(), noData, eDstType) == CE_None;
}

/************************************************************************/
/*                 GDALRasterReclassifyCreateVRTDerived                 */
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
        const GDALDataType srcType =
            input.GetRasterBand(iBand)->GetRasterDataType();
        const GDALDataType bandType =
            eDstType == GDT_Unknown ? srcType : eDstType;
        const GDALDataType xferType = GDALDataTypeUnion(srcType, bandType);

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

        CPLXMLNode *sourceTransferType =
            CPLCreateXMLNode(band, CXT_Element, "SourceTransferType");
        CPLCreateXMLNode(sourceTransferType, CXT_Text,
                         GDALGetDataTypeName(xferType));
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

    GDALGeoTransform gt;
    if (input.GetGeoTransform(gt) == CE_None)
        ds->SetGeoTransform(gt);
    ds->SetSpatialRef(input.GetSpatialRef());

    return ds;
}

/************************************************************************/
/*               GDALRasterReclassifyAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterReclassifyAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    const auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    // Already validated by argument parser
    const GDALDataType eDstType =
        m_type.empty() ? GDT_Unknown : GDALGetDataTypeByName(m_type.c_str());

    const auto nErrorCount = CPLGetErrorCounter();
    if (!m_mapping.empty() && m_mapping[0] == '@')
    {
        auto f =
            VSIVirtualHandleUniquePtr(VSIFOpenL(m_mapping.c_str() + 1, "r"));
        if (!f)
        {
            ReportError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                        m_mapping.c_str() + 1);
            return false;
        }

        m_mapping.clear();
        try
        {
            constexpr int MAX_CHARS_PER_LINE = 1000 * 1000;
            constexpr size_t MAX_MAPPING_SIZE = 10 * 1000 * 1000;
            while (const char *line =
                       CPLReadLine2L(f.get(), MAX_CHARS_PER_LINE, nullptr))
            {
                while (isspace(*line))
                {
                    line++;
                }

                if (line[0])
                {
                    if (!m_mapping.empty())
                    {
                        m_mapping.append(";");
                    }

                    const char *comment = strchr(line, '#');
                    if (!comment)
                    {
                        m_mapping.append(line);
                    }
                    else
                    {
                        m_mapping.append(line,
                                         static_cast<size_t>(comment - line));
                    }
                    if (m_mapping.size() > MAX_MAPPING_SIZE)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Too large mapping size");
                        return false;
                    }
                }
            }
        }
        catch (const std::exception &)
        {
            ReportError(CE_Failure, CPLE_OutOfMemory,
                        "Out of memory while ingesting mapping file");
        }
    }
    if (nErrorCount == CPLGetErrorCounter())
    {
        if (!GDALReclassifyValidateMappings(*poSrcDS, m_mapping, eDstType))
        {
            return false;
        }

        m_outputDataset.Set(
            GDALReclassifyCreateVRTDerived(*poSrcDS, m_mapping, eDstType));
    }
    return m_outputDataset.GetDatasetRef() != nullptr;
}

GDALRasterReclassifyAlgorithmStandalone::
    ~GDALRasterReclassifyAlgorithmStandalone() = default;

//! @endcond
