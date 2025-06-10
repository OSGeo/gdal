/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal raster calc" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_calc.h"

#include "../frmts/vrt/gdal_vrt.h"
#include "../frmts/vrt/vrtdataset.h"

#include "cpl_float.h"
#include "cpl_vsi_virtual.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

#include <algorithm>
#include <array>
#include <optional>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

struct GDALCalcOptions
{
    GDALDataType dstType{GDT_Float64};
    bool checkSRS{true};
    bool checkExtent{true};
};

static bool MatchIsCompleteVariableNameWithNoIndex(const std::string &str,
                                                   size_t from, size_t to)
{
    if (to < str.size())
    {
        // If the character after the end of the match is:
        // * alphanumeric or _ : we've matched only part of a variable name
        // * [ : we've matched a variable that already has an index
        // * ( : we've matched a function name
        if (std::isalnum(str[to]) || str[to] == '_' || str[to] == '[' ||
            str[to] == '(')
        {
            return false;
        }
    }
    if (from > 0)
    {
        // If the character before the start of the match is alphanumeric or _,
        // we've matched only part of a variable name.
        if (std::isalnum(str[from - 1]) || str[from - 1] == '_')
        {
            return false;
        }
    }

    return true;
}

/**
 *  Add a band subscript to all instances of a specified variable that
 *  do not already have such a subscript. For example, "X" would be
 *  replaced with "X[3]" but "X[1]" would be left untouched.
 */
static std::string SetBandIndices(const std::string &origExpression,
                                  const std::string &variable, int band,
                                  bool &expressionChanged)
{
    std::string expression = origExpression;
    expressionChanged = false;

    std::string::size_type seekPos = 0;
    auto pos = expression.find(variable, seekPos);
    while (pos != std::string::npos)
    {
        auto end = pos + variable.size();

        if (MatchIsCompleteVariableNameWithNoIndex(expression, pos, end))
        {
            // No index specified for variable
            expression = expression.substr(0, pos + variable.size()) + '[' +
                         std::to_string(band) + ']' + expression.substr(end);
            expressionChanged = true;
        }

        seekPos = end;
        pos = expression.find(variable, seekPos);
    }

    return expression;
}

struct SourceProperties
{
    int nBands{0};
    int nX{0};
    int nY{0};
    std::array<double, 6> gt{};
    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> srs{
        nullptr};
};

static std::optional<SourceProperties>
UpdateSourceProperties(SourceProperties &out, const std::string &dsn,
                       const GDALCalcOptions &options)
{
    SourceProperties source;
    bool srsMismatch = false;
    bool extentMismatch = false;
    bool dimensionMismatch = false;

    {
        std::unique_ptr<GDALDataset> ds(
            GDALDataset::Open(dsn.c_str(), GDAL_OF_RASTER));

        if (!ds)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to open %s",
                     dsn.c_str());
            return std::nullopt;
        }

        source.nBands = ds->GetRasterCount();
        source.nX = ds->GetRasterXSize();
        source.nY = ds->GetRasterYSize();

        if (options.checkExtent)
        {
            ds->GetGeoTransform(source.gt.data());
        }

        if (options.checkSRS && out.srs)
        {
            const OGRSpatialReference *srs = ds->GetSpatialRef();
            srsMismatch = srs && !srs->IsSame(out.srs.get());
        }
    }

    if (source.nX != out.nX || source.nY != out.nY)
    {
        dimensionMismatch = true;
    }

    if (source.gt[0] != out.gt[0] || source.gt[2] != out.gt[2] ||
        source.gt[3] != out.gt[3] || source.gt[4] != out.gt[4])
    {
        extentMismatch = true;
    }
    if (source.gt[1] != out.gt[1] || source.gt[5] != out.gt[5])
    {
        // Resolutions are different. Are the extents the same?
        double xmaxOut = out.gt[0] + out.nX * out.gt[1] + out.nY * out.gt[2];
        double yminOut = out.gt[3] + out.nX * out.gt[4] + out.nY * out.gt[5];

        double xmax =
            source.gt[0] + source.nX * source.gt[1] + source.nY * source.gt[2];
        double ymin =
            source.gt[3] + source.nX * source.gt[4] + source.nY * source.gt[5];

        // Max allowable extent misalignment, expressed as fraction of a pixel
        constexpr double EXTENT_RTOL = 1e-3;

        if (std::abs(xmax - xmaxOut) > EXTENT_RTOL * std::abs(source.gt[1]) ||
            std::abs(ymin - yminOut) > EXTENT_RTOL * std::abs(source.gt[5]))
        {
            extentMismatch = true;
        }
    }

    if (options.checkExtent && extentMismatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Input extents are inconsistent.");
        return std::nullopt;
    }

    if (!options.checkExtent && dimensionMismatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inputs do not have the same dimensions.");
        return std::nullopt;
    }

    // Find a common resolution
    if (source.nX > out.nX)
    {
        auto dx = CPLGreatestCommonDivisor(out.gt[1], source.gt[1]);
        if (dx == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to find common resolution for inputs.");
            return std::nullopt;
        }
        out.nX = static_cast<int>(
            std::round(static_cast<double>(out.nX) * out.gt[1] / dx));
        out.gt[1] = dx;
    }
    if (source.nY > out.nY)
    {
        auto dy = CPLGreatestCommonDivisor(out.gt[5], source.gt[5]);
        if (dy == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to find common resolution for inputs.");
            return std::nullopt;
        }
        out.nY = static_cast<int>(
            std::round(static_cast<double>(out.nY) * out.gt[5] / dy));
        out.gt[5] = dy;
    }

    if (srsMismatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Input spatial reference systems are inconsistent.");
        return std::nullopt;
    }

    return source;
}

/** Create XML nodes for one or more derived bands resulting from the evaluation
 *  of a single expression
 *
 * @param root VRTDataset node to which the band nodes should be added
 * @param nXOut Number of columns in VRT dataset
 * @param nYOut Number of rows in VRT dataset
 * @param expression Expression for which band(s) should be added
 * @param sources Mapping of source names to DSNs
 * @param sourceProps Mapping of source names to properties
 * @return true if the band(s) were added, false otherwise
 */
static bool
CreateDerivedBandXML(CPLXMLNode *root, int nXOut, int nYOut,
                     GDALDataType bandType, const std::string &expression,
                     const std::map<std::string, std::string> &sources,
                     const std::map<std::string, SourceProperties> &sourceProps)
{
    int nOutBands = 1;  // By default, each expression produces a single output
                        // band. When processing the expression below, we may
                        // discover that the expression produces multiple bands,
                        // in which case this will be updated.
    for (int nOutBand = 1; nOutBand <= nOutBands; nOutBand++)
    {
        // Copy the expression for each output band, because we may modify it
        // when adding band indices (e.g., X -> X[1]) to the variables in the
        // expression.
        std::string bandExpression = expression;

        CPLXMLNode *band = CPLCreateXMLNode(root, CXT_Element, "VRTRasterBand");
        CPLAddXMLAttributeAndValue(band, "subClass", "VRTDerivedRasterBand");
        CPLAddXMLAttributeAndValue(band, "dataType",
                                   GDALGetDataTypeName(bandType));

        CPLXMLNode *sourceTransferType =
            CPLCreateXMLNode(band, CXT_Element, "SourceTransferType");
        CPLCreateXMLNode(sourceTransferType, CXT_Text,
                         GDALGetDataTypeName(GDT_Float64));

        CPLXMLNode *pixelFunctionType =
            CPLCreateXMLNode(band, CXT_Element, "PixelFunctionType");
        CPLCreateXMLNode(pixelFunctionType, CXT_Text, "expression");
        CPLXMLNode *arguments =
            CPLCreateXMLNode(band, CXT_Element, "PixelFunctionArguments");

        for (const auto &[source_name, dsn] : sources)
        {
            auto it = sourceProps.find(source_name);
            CPLAssert(it != sourceProps.end());
            const auto &props = it->second;

            {
                const int nDefaultInBand = std::min(props.nBands, nOutBand);

                CPLString expressionBandVariable;
                expressionBandVariable.Printf("%s[%d]", source_name.c_str(),
                                              nDefaultInBand);

                bool expressionUsesAllBands = false;
                bandExpression =
                    SetBandIndices(bandExpression, source_name, nDefaultInBand,
                                   expressionUsesAllBands);

                if (expressionUsesAllBands)
                {
                    if (nOutBands <= 1)
                    {
                        nOutBands = props.nBands;
                    }
                    else if (props.nBands != 1 && props.nBands != nOutBands)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Expression cannot operate on all bands of "
                                 "rasters with incompatible numbers of bands "
                                 "(source %s has %d bands but expected to have "
                                 "1 or %d bands).",
                                 source_name.c_str(), props.nBands, nOutBands);
                        return false;
                    }
                }
            }

            // Create a <SimpleSource> for each input band that is used in
            // the expression.
            for (int nInBand = 1; nInBand <= props.nBands; nInBand++)
            {
                CPLString inBandVariable;
                inBandVariable.Printf("%s[%d]", source_name.c_str(), nInBand);
                if (bandExpression.find(inBandVariable) == std::string::npos)
                {
                    continue;
                }

                CPLXMLNode *source =
                    CPLCreateXMLNode(band, CXT_Element, "SimpleSource");
                CPLAddXMLAttributeAndValue(source, "name",
                                           inBandVariable.c_str());

                CPLXMLNode *sourceFilename =
                    CPLCreateXMLNode(source, CXT_Element, "SourceFilename");
                CPLAddXMLAttributeAndValue(sourceFilename, "relativeToVRT",
                                           "0");
                CPLCreateXMLNode(sourceFilename, CXT_Text, dsn.c_str());

                CPLXMLNode *sourceBand =
                    CPLCreateXMLNode(source, CXT_Element, "SourceBand");
                CPLCreateXMLNode(sourceBand, CXT_Text,
                                 std::to_string(nInBand).c_str());

                // TODO add <SourceProperties> ?

                CPLXMLNode *srcRect =
                    CPLCreateXMLNode(source, CXT_Element, "SrcRect");
                CPLAddXMLAttributeAndValue(srcRect, "xOff", "0");
                CPLAddXMLAttributeAndValue(srcRect, "yOff", "0");
                CPLAddXMLAttributeAndValue(srcRect, "xSize",
                                           std::to_string(props.nX).c_str());
                CPLAddXMLAttributeAndValue(srcRect, "ySize",
                                           std::to_string(props.nY).c_str());

                CPLXMLNode *dstRect =
                    CPLCreateXMLNode(source, CXT_Element, "DstRect");
                CPLAddXMLAttributeAndValue(dstRect, "xOff", "0");
                CPLAddXMLAttributeAndValue(dstRect, "yOff", "0");
                CPLAddXMLAttributeAndValue(dstRect, "xSize",
                                           std::to_string(nXOut).c_str());
                CPLAddXMLAttributeAndValue(dstRect, "ySize",
                                           std::to_string(nYOut).c_str());
            }
        }

        // Add the expression as a last step, because we may modify the
        // expression as we iterate through the bands.
        CPLAddXMLAttributeAndValue(arguments, "expression",
                                   bandExpression.c_str());
        CPLAddXMLAttributeAndValue(arguments, "dialect", "muparser");
    }

    return true;
}

static bool ParseSourceDescriptors(const std::vector<std::string> &inputs,
                                   std::map<std::string, std::string> &datasets,
                                   std::string &firstSourceName)
{
    bool isFirst = true;

    for (const auto &input : inputs)
    {
        std::string name = "";

        auto pos = input.find('=');
        if (pos == std::string::npos)
        {
            if (inputs.size() > 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Inputs must be named when more than one input is "
                         "provided.");
                return false;
            }
            name = "X";
        }
        else
        {
            name = input.substr(0, pos);
        }

        std::string dsn =
            (pos == std::string::npos) ? input : input.substr(pos + 1);
        datasets[name] = std::move(dsn);

        if (isFirst)
        {
            firstSourceName = name;
            isFirst = false;
        }
    }

    return true;
}

static bool ReadFileLists(std::vector<std::string> &inputs)
{
    for (std::size_t i = 0; i < inputs.size(); i++)
    {
        const auto &input = inputs[i];
        if (input[0] == '@')
        {
            auto f =
                VSIVirtualHandleUniquePtr(VSIFOpenL(input.c_str() + 1, "r"));
            if (!f)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                         input.c_str() + 1);
                return false;
            }
            std::vector<std::string> sources;
            while (const char *filename = CPLReadLineL(f.get()))
            {
                sources.push_back(filename);
            }
            inputs.erase(inputs.begin() + static_cast<int>(i));
            inputs.insert(inputs.end(), sources.begin(), sources.end());
        }
    }

    return true;
}

/** Creates a VRT datasource with one or more derived raster bands containing
 *  results of an expression.
 *
 * To make this work with muparser (which does not support vector types), we
 * do a simple parsing of the expression internally, transforming it into
 * multiple expressions with explicit band indices. For example, for a two-band
 * raster "X", the expression "X + 3" will be transformed into "X[1] + 3" and
 * "X[2] + 3". The use of brackets is for readability only; as far as the
 * expression engine is concerned, the variables "X[1]" and "X[2]" have nothing
 * to do with each other.
 *
 * @param inputs A list of sources, expressed as NAME=DSN
 * @param expressions A list of expressions to be evaluated
 * @param options flags controlling which checks should be performed on the inputs
 *
 * @return a newly created VRTDataset, or nullptr on error
 */
static std::unique_ptr<GDALDataset>
GDALCalcCreateVRTDerived(const std::vector<std::string> &inputs,
                         const std::vector<std::string> &expressions,
                         const GDALCalcOptions &options)
{
    if (inputs.empty())
    {
        return nullptr;
    }

    std::map<std::string, std::string> sources;
    std::string firstSource;
    if (!ParseSourceDescriptors(inputs, sources, firstSource))
    {
        return nullptr;
    }

    // Use the first source provided to determine properties of the output
    const char *firstDSN = sources[firstSource].c_str();

    // Read properties from the first source
    SourceProperties out;
    {
        std::unique_ptr<GDALDataset> ds(
            GDALDataset::Open(firstDSN, GDAL_OF_RASTER));

        if (!ds)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to open %s",
                     firstDSN);
            return nullptr;
        }

        out.nX = ds->GetRasterXSize();
        out.nY = ds->GetRasterYSize();
        out.nBands = 1;
        out.srs.reset(ds->GetSpatialRef() ? ds->GetSpatialRef()->Clone()
                                          : nullptr);
        ds->GetGeoTransform(out.gt.data());
    }

    CPLXMLTreeCloser root(CPLCreateXMLNode(nullptr, CXT_Element, "VRTDataset"));

    // Collect properties of the different sources, and verity them for
    // consistency.
    std::map<std::string, SourceProperties> sourceProps;
    for (const auto &[source_name, dsn] : sources)
    {
        // TODO avoid opening the first source twice.
        auto props = UpdateSourceProperties(out, dsn, options);
        if (props.has_value())
        {
            sourceProps[source_name] = std::move(props.value());
        }
        else
        {
            return nullptr;
        }
    }

    for (const auto &origExpression : expressions)
    {
        if (!CreateDerivedBandXML(root.get(), out.nX, out.nY, options.dstType,
                                  origExpression, sources, sourceProps))
        {
            return nullptr;
        }
    }

    //CPLDebug("VRT", "%s", CPLSerializeXMLTree(root.get()));

    auto ds = std::make_unique<VRTDataset>(out.nX, out.nY);
    if (ds->XMLInit(root.get(), "") != CE_None)
    {
        return nullptr;
    };
    ds->SetGeoTransform(out.gt.data());
    if (out.srs)
    {
        ds->SetSpatialRef(out.srs.get());
    }

    return ds;
}

/************************************************************************/
/*          GDALRasterCalcAlgorithm::GDALRasterCalcAlgorithm()          */
/************************************************************************/

GDALRasterCalcAlgorithm::GDALRasterCalcAlgorithm() noexcept
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    m_supportsStreamedOutput = true;

    AddProgressArg();

    AddArg(GDAL_ARG_NAME_INPUT, 'i', _("Input raster datasets"), &m_inputs)
        .SetPositional()
        .SetRequired()
        .SetMinCount(1)
        .SetAutoOpenDataset(false)
        .SetMetaVar("INPUTS");

    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ true,
                       /* bGDALGAllowed = */ true);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);
    AddOutputDataTypeArg(&m_type);

    AddArg("no-check-srs", 0,
           _("Do not check consistency of input spatial reference systems"),
           &m_NoCheckSRS);
    AddArg("no-check-extent", 0, _("Do not check consistency of input extents"),
           &m_NoCheckExtent);

    AddArg("calc", 0, _("Expression(s) to evaluate"), &m_expr)
        .SetRequired()
        .SetPackedValuesAllowed(false)
        .SetMinCount(1);
}

/************************************************************************/
/*                GDALRasterCalcAlgorithm::RunImpl()                    */
/************************************************************************/

bool GDALRasterCalcAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

    GDALCalcOptions options;
    options.checkExtent = !m_NoCheckExtent;
    options.checkSRS = !m_NoCheckSRS;
    if (!m_type.empty())
    {
        options.dstType = GDALGetDataTypeByName(m_type.c_str());
    }

    if (!ReadFileLists(m_inputs))
    {
        return false;
    }

    auto vrt = GDALCalcCreateVRTDerived(m_inputs, m_expr, options);
    if (vrt == nullptr)
    {
        return false;
    }

    if (m_format == "stream")
    {
        m_outputDataset.Set(std::move(vrt));
        return true;
    }

    CPLStringList translateArgs;
    if (!m_format.empty())
    {
        translateArgs.AddString("-of");
        translateArgs.AddString(m_format.c_str());
    }
    for (const auto &co : m_creationOptions)
    {
        translateArgs.AddString("-co");
        translateArgs.AddString(co.c_str());
    }

    GDALTranslateOptions *translateOptions =
        GDALTranslateOptionsNew(translateArgs.List(), nullptr);
    GDALTranslateOptionsSetProgress(translateOptions, pfnProgress,
                                    pProgressData);

    auto poOutDS =
        std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALTranslate(
            m_outputDataset.GetName().c_str(), GDALDataset::ToHandle(vrt.get()),
            translateOptions, nullptr)));
    GDALTranslateOptionsFree(translateOptions);

    const bool bOK = poOutDS != nullptr;
    m_outputDataset.Set(std::move(poOutDS));

    return bOK;
}

//! @endcond
