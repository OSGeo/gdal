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
#include "vrtdataset.h"

#include <algorithm>
#include <optional>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

struct GDALCalcOptions
{
    GDALDataType dstType{GDT_Unknown};
    bool checkCRS{true};
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

static bool PosIsAggregateFunctionArgument(const std::string &expression,
                                           size_t pos)
{
    // If this position is a function argument, we should be able to
    // scan backwards for a ( and find only variable names, literals or commas.
    while (pos != 0)
    {
        const char c = expression[pos];
        if (c == '(')
        {
            pos--;
            break;
        }
        if (!(isspace(c) || isalnum(c) || c == ',' || c == '.' || c == '[' ||
              c == ']' || c == '_'))
        {
            return false;
        }
        pos--;
    }

    // Now what we've found the (, the preceding characters should be an
    // aggregate function name
    if (pos < 2)
    {
        return false;
    }

    if (STARTS_WITH_CI(expression.c_str() + (pos - 2), "avg") ||
        STARTS_WITH_CI(expression.c_str() + (pos - 2), "sum") ||
        STARTS_WITH_CI(expression.c_str() + (pos - 2), "min") ||
        STARTS_WITH_CI(expression.c_str() + (pos - 2), "max"))
    {
        return true;
    }

    return false;
}

/**
 *  Replace X by X[1],X[2],...X[n]
 */
static std::string
SetBandIndicesFlattenedExpression(const std::string &origExpression,
                                  const std::string &variable, int nBands)
{
    std::string expression = origExpression;

    std::string::size_type seekPos = 0;
    auto pos = expression.find(variable, seekPos);
    while (pos != std::string::npos)
    {
        auto end = pos + variable.size();

        if (MatchIsCompleteVariableNameWithNoIndex(expression, pos, end) &&
            PosIsAggregateFunctionArgument(expression, pos))
        {
            std::string newExpr = expression.substr(0, pos);
            for (int i = 1; i <= nBands; ++i)
            {
                if (i > 1)
                    newExpr += ',';
                newExpr += variable;
                newExpr += '[';
                newExpr += std::to_string(i);
                newExpr += ']';
            }
            const size_t oldExprSize = expression.size();
            newExpr += expression.substr(end);
            expression = std::move(newExpr);
            end += expression.size() - oldExprSize;
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
    GDALGeoTransform gt{};
    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> srs{
        nullptr};
    std::vector<std::optional<double>> noData{};
    GDALDataType eDT{GDT_Unknown};
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
        source.noData.resize(source.nBands);

        if (options.checkExtent)
        {
            ds->GetGeoTransform(source.gt);
        }

        if (options.checkCRS && out.srs)
        {
            const OGRSpatialReference *srs = ds->GetSpatialRef();
            srsMismatch = srs && !srs->IsSame(out.srs.get());
        }

        // Store the source data type if it is the same for all bands in the source
        bool bandsHaveSameType = true;
        for (int i = 1; i <= source.nBands; ++i)
        {
            GDALRasterBand *band = ds->GetRasterBand(i);

            if (i == 1)
            {
                source.eDT = band->GetRasterDataType();
            }
            else if (bandsHaveSameType &&
                     source.eDT != band->GetRasterDataType())
            {
                source.eDT = GDT_Unknown;
                bandsHaveSameType = false;
            }

            int success;
            double noData = band->GetNoDataValue(&success);
            if (success)
            {
                source.noData[i - 1] = noData;
            }
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
 * @param bandType the type of the band(s) to create
 * @param nXOut Number of columns in VRT dataset
 * @param nYOut Number of rows in VRT dataset
 * @param expression Expression for which band(s) should be added
 * @param dialect Expression dialect
 * @param flatten Generate a single band output raster per expression, even if
 *                input datasets are multiband.
 * @param noDataText nodata value to use for the created band, or "none", or ""
 * @param pixelFunctionArguments Pixel function arguments.
 * @param sources Mapping of source names to DSNs
 * @param sourceProps Mapping of source names to properties
 * @param fakeSourceFilename If not empty, used instead of real input filenames.
 * @return true if the band(s) were added, false otherwise
 */
static bool
CreateDerivedBandXML(CPLXMLNode *root, int nXOut, int nYOut,
                     GDALDataType bandType, const std::string &expression,
                     const std::string &dialect, bool flatten,
                     const std::string &noDataText,
                     const std::vector<std::string> &pixelFunctionArguments,
                     const std::map<std::string, std::string> &sources,
                     const std::map<std::string, SourceProperties> &sourceProps,
                     const std::string &fakeSourceFilename)
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
        if (bandType == GDT_Unknown)
        {
            bandType = GDT_Float64;
        }
        CPLAddXMLAttributeAndValue(band, "dataType",
                                   GDALGetDataTypeName(bandType));

        std::optional<double> dstNoData;
        bool autoSelectNoDataValue = false;
        if (noDataText.empty())
        {
            autoSelectNoDataValue = true;
        }
        else if (noDataText != "none")
        {
            char *end;
            dstNoData = CPLStrtod(noDataText.c_str(), &end);
            if (end != noDataText.c_str() + noDataText.size())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid NoData value: %s", noDataText.c_str());
                return false;
            }
        }

        for (const auto &[source_name, dsn] : sources)
        {
            auto it = sourceProps.find(source_name);
            CPLAssert(it != sourceProps.end());
            const auto &props = it->second;

            bool expressionAppliedPerBand = false;
            if (dialect == "builtin")
            {
                expressionAppliedPerBand = !flatten;
            }
            else
            {
                const int nDefaultInBand = std::min(props.nBands, nOutBand);

                if (flatten)
                {
                    bandExpression = SetBandIndicesFlattenedExpression(
                        bandExpression, source_name, props.nBands);
                }

                bandExpression =
                    SetBandIndices(bandExpression, source_name, nDefaultInBand,
                                   expressionAppliedPerBand);
            }

            if (expressionAppliedPerBand)
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

            // Create a source for each input band that is used in
            // the expression.
            for (int nInBand = 1; nInBand <= props.nBands; nInBand++)
            {
                CPLString inBandVariable;
                if (dialect == "builtin")
                {
                    if (!flatten && props.nBands >= 2 && nInBand != nOutBand)
                        continue;
                }
                else
                {
                    inBandVariable.Printf("%s[%d]", source_name.c_str(),
                                          nInBand);
                    if (bandExpression.find(inBandVariable) ==
                        std::string::npos)
                    {
                        continue;
                    }
                }

                const std::optional<double> &srcNoData =
                    props.noData[nInBand - 1];

                CPLXMLNode *source = CPLCreateXMLNode(
                    band, CXT_Element,
                    srcNoData.has_value() ? "ComplexSource" : "SimpleSource");
                if (!inBandVariable.empty())
                {
                    CPLAddXMLAttributeAndValue(source, "name",
                                               inBandVariable.c_str());
                }

                CPLXMLNode *sourceFilename =
                    CPLCreateXMLNode(source, CXT_Element, "SourceFilename");
                if (fakeSourceFilename.empty())
                {
                    CPLAddXMLAttributeAndValue(sourceFilename, "relativeToVRT",
                                               "0");
                    CPLCreateXMLNode(sourceFilename, CXT_Text, dsn.c_str());
                }
                else
                {
                    CPLCreateXMLNode(sourceFilename, CXT_Text,
                                     fakeSourceFilename.c_str());
                }

                CPLXMLNode *sourceBand =
                    CPLCreateXMLNode(source, CXT_Element, "SourceBand");
                CPLCreateXMLNode(sourceBand, CXT_Text,
                                 std::to_string(nInBand).c_str());

                if (srcNoData.has_value())
                {
                    CPLXMLNode *srcNoDataNode =
                        CPLCreateXMLNode(source, CXT_Element, "NODATA");
                    std::string srcNoDataText =
                        CPLSPrintf("%.17g", srcNoData.value());
                    CPLCreateXMLNode(srcNoDataNode, CXT_Text,
                                     srcNoDataText.c_str());

                    if (autoSelectNoDataValue && !dstNoData.has_value())
                    {
                        dstNoData = srcNoData;
                    }
                }

                if (fakeSourceFilename.empty())
                {
                    CPLXMLNode *srcRect =
                        CPLCreateXMLNode(source, CXT_Element, "SrcRect");
                    CPLAddXMLAttributeAndValue(srcRect, "xOff", "0");
                    CPLAddXMLAttributeAndValue(srcRect, "yOff", "0");
                    CPLAddXMLAttributeAndValue(
                        srcRect, "xSize", std::to_string(props.nX).c_str());
                    CPLAddXMLAttributeAndValue(
                        srcRect, "ySize", std::to_string(props.nY).c_str());

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

            if (dstNoData.has_value())
            {
                if (!GDALIsValueExactAs(dstNoData.value(), bandType))
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Band output type %s cannot represent NoData value %g",
                        GDALGetDataTypeName(bandType), dstNoData.value());
                    return false;
                }

                CPLXMLNode *noDataNode =
                    CPLCreateXMLNode(band, CXT_Element, "NoDataValue");
                CPLString dstNoDataText =
                    CPLSPrintf("%.17g", dstNoData.value());
                CPLCreateXMLNode(noDataNode, CXT_Text, dstNoDataText.c_str());
            }
        }

        CPLXMLNode *pixelFunctionType =
            CPLCreateXMLNode(band, CXT_Element, "PixelFunctionType");
        CPLXMLNode *arguments =
            CPLCreateXMLNode(band, CXT_Element, "PixelFunctionArguments");

        if (dialect == "builtin")
        {
            CPLCreateXMLNode(pixelFunctionType, CXT_Text, expression.c_str());
        }
        else
        {
            CPLCreateXMLNode(pixelFunctionType, CXT_Text, "expression");
            CPLAddXMLAttributeAndValue(arguments, "dialect", "muparser");
            // Add the expression as a last step, because we may modify the
            // expression as we iterate through the bands.
            CPLAddXMLAttributeAndValue(arguments, "expression",
                                       bandExpression.c_str());
        }

        if (!pixelFunctionArguments.empty())
        {
            const CPLStringList args(pixelFunctionArguments);
            for (const auto &[key, value] : cpl::IterateNameValue(args))
            {
                CPLAddXMLAttributeAndValue(arguments, key, value);
            }
        }
    }

    return true;
}

static bool ParseSourceDescriptors(const std::vector<std::string> &inputs,
                                   std::map<std::string, std::string> &datasets,
                                   std::string &firstSourceName,
                                   bool requireSourceNames)
{
    for (size_t iInput = 0; iInput < inputs.size(); iInput++)
    {
        const std::string &input = inputs[iInput];
        std::string name;

        const auto pos = input.find('=');
        if (pos == std::string::npos)
        {
            if (requireSourceNames && inputs.size() > 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Inputs must be named when more than one input is "
                         "provided.");
                return false;
            }
            name = "X";
            if (iInput > 0)
            {
                name += std::to_string(iInput);
            }
        }
        else
        {
            name = input.substr(0, pos);
        }

        // Check input name is legal
        for (size_t i = 0; i < name.size(); ++i)
        {
            const char c = name[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
            {
                // ok
            }
            else if (c == '_' || (c >= '0' && c <= '9'))
            {
                if (i == 0)
                {
                    // Reserved constants in MuParser start with an underscore
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Name '%s' is illegal because it starts with a '%c'",
                        name.c_str(), c);
                    return false;
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Name '%s' is illegal because character '%c' is not "
                         "allowed",
                         name.c_str(), c);
                return false;
            }
        }

        std::string dsn =
            (pos == std::string::npos) ? input : input.substr(pos + 1);

        if (!dsn.empty() && dsn.front() == '[' && dsn.back() == ']')
        {
            dsn = "{\"type\":\"gdal_streamed_alg\", \"command_line\":\"gdal "
                  "raster pipeline " +
                  CPLString(dsn.substr(1, dsn.size() - 2))
                      .replaceAll('\\', "\\\\")
                      .replaceAll('"', "\\\"") +
                  "\"}";
        }

        if (datasets.find(name) != datasets.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "An input with name '%s' has already been provided",
                     name.c_str());
            return false;
        }
        datasets[name] = std::move(dsn);

        if (iInput == 0)
        {
            firstSourceName = std::move(name);
        }
    }

    return true;
}

static bool ReadFileLists(const std::vector<GDALArgDatasetValue> &inputDS,
                          std::vector<std::string> &inputFilenames)
{
    for (const auto &dsVal : inputDS)
    {
        const auto &input = dsVal.GetName();
        if (!input.empty() && input[0] == '@')
        {
            auto f =
                VSIVirtualHandleUniquePtr(VSIFOpenL(input.c_str() + 1, "r"));
            if (!f)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                         input.c_str() + 1);
                return false;
            }
            while (const char *filename = CPLReadLineL(f.get()))
            {
                inputFilenames.push_back(filename);
            }
        }
        else
        {
            inputFilenames.push_back(input);
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
 * @param dialect Expression dialect
 * @param flatten Generate a single band output raster per expression, even if
 *                input datasets are multiband.
 * @param noData NoData values to use for output bands, or "none", or ""
 * @param pixelFunctionArguments Pixel function arguments.
 * @param options flags controlling which checks should be performed on the inputs
 * @param[out] maxSourceBands Maximum number of bands in source dataset(s)
 * @param fakeSourceFilename If not empty, used instead of real input filenames.
 *
 * @return a newly created VRTDataset, or nullptr on error
 */
static std::unique_ptr<GDALDataset> GDALCalcCreateVRTDerived(
    const std::vector<std::string> &inputs,
    const std::vector<std::string> &expressions, const std::string &dialect,
    bool flatten, const std::string &noData,
    const std::vector<std::vector<std::string>> &pixelFunctionArguments,
    const GDALCalcOptions &options, int &maxSourceBands,
    const std::string &fakeSourceFilename = std::string())
{
    if (inputs.empty())
    {
        return nullptr;
    }

    std::map<std::string, std::string> sources;
    std::string firstSource;
    bool requireSourceNames = dialect != "builtin";
    if (!ParseSourceDescriptors(inputs, sources, firstSource,
                                requireSourceNames))
    {
        return nullptr;
    }

    // Use the first source provided to determine properties of the output
    const char *firstDSN = sources[firstSource].c_str();

    maxSourceBands = 0;

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
        ds->GetGeoTransform(out.gt);
    }

    CPLXMLTreeCloser root(CPLCreateXMLNode(nullptr, CXT_Element, "VRTDataset"));

    maxSourceBands = 0;

    // Collect properties of the different sources, and verity them for
    // consistency.
    std::map<std::string, SourceProperties> sourceProps;
    for (const auto &[source_name, dsn] : sources)
    {
        // TODO avoid opening the first source twice.
        auto props = UpdateSourceProperties(out, dsn, options);
        if (props.has_value())
        {
            maxSourceBands = std::max(maxSourceBands, props->nBands);
            sourceProps[source_name] = std::move(props.value());
        }
        else
        {
            return nullptr;
        }
    }

    size_t iExpr = 0;
    for (const auto &origExpression : expressions)
    {
        GDALDataType bandType = options.dstType;

        // If output band type has not been specified, set it equal to the
        // input band type for certain pixel functions, if the inputs have
        // a consistent band type.
        if (bandType == GDT_Unknown && dialect == "builtin" &&
            (origExpression == "min" || origExpression == "max" ||
             origExpression == "mode"))
        {
            for (const auto &[_, props] : sourceProps)
            {
                if (bandType == GDT_Unknown)
                {
                    bandType = props.eDT;
                }
                else if (props.eDT == GDT_Unknown || props.eDT != bandType)
                {
                    bandType = GDT_Unknown;
                    break;
                }
            }
        }

        if (!CreateDerivedBandXML(root.get(), out.nX, out.nY, bandType,
                                  origExpression, dialect, flatten, noData,
                                  pixelFunctionArguments[iExpr], sources,
                                  sourceProps, fakeSourceFilename))
        {
            return nullptr;
        }
        ++iExpr;
    }

    //CPLDebug("VRT", "%s", CPLSerializeXMLTree(root.get()));

    auto ds = fakeSourceFilename.empty()
                  ? std::make_unique<VRTDataset>(out.nX, out.nY)
                  : std::make_unique<VRTDataset>(1, 1);
    if (ds->XMLInit(root.get(), "") != CE_None)
    {
        return nullptr;
    };
    ds->SetGeoTransform(out.gt);
    if (out.srs)
    {
        ds->SetSpatialRef(out.srs.get());
    }

    return ds;
}

/************************************************************************/
/*          GDALRasterCalcAlgorithm::GDALRasterCalcAlgorithm()          */
/************************************************************************/

GDALRasterCalcAlgorithm::GDALRasterCalcAlgorithm(bool standaloneStep) noexcept
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetAddDefaultArguments(false)
                                          .SetAutoOpenInputDatasets(false)
                                          .SetInputDatasetMetaVar("INPUTS")
                                          .SetInputDatasetMaxCount(INT_MAX))
{
    AddRasterInputArgs(false, false);
    if (standaloneStep)
    {
        AddProgressArg();
        AddRasterOutputArgs(false);
    }

    AddOutputDataTypeArg(&m_type);

    AddArg("no-check-crs", 0,
           _("Do not check consistency of input coordinate reference systems"),
           &m_noCheckCRS)
        .AddHiddenAlias("no-check-srs");
    AddArg("no-check-extent", 0, _("Do not check consistency of input extents"),
           &m_noCheckExtent);

    AddArg("propagate-nodata", 0,
           _("Whether to set pixels to the output NoData value if any of the "
             "input pixels is NoData"),
           &m_propagateNoData);

    AddArg("calc", 0, _("Expression(s) to evaluate"), &m_expr)
        .SetRequired()
        .SetPackedValuesAllowed(false)
        .SetMinCount(1)
        .SetAutoCompleteFunction(
            [this](const std::string &currentValue)
            {
                std::vector<std::string> ret;
                if (m_dialect == "builtin")
                {
                    if (currentValue.find('(') == std::string::npos)
                        return VRTDerivedRasterBand::GetPixelFunctionNames();
                }
                return ret;
            });

    AddArg("dialect", 0, _("Expression dialect"), &m_dialect)
        .SetDefault(m_dialect)
        .SetChoices("muparser", "builtin");

    AddArg("flatten", 0,
           _("Generate a single band output raster per expression, even if "
             "input datasets are multiband"),
           &m_flatten);

    AddNodataArg(&m_nodata, true);

    // This is a hidden option only used by test_gdalalg_raster_calc_expression_rewriting()
    // for now
    AddArg("no-check-expression", 0,
           _("Whether to skip expression validity checks for virtual format "
             "output"),
           &m_noCheckExpression)
        .SetHidden();

    AddValidationAction(
        [this]()
        {
            GDALPipelineStepRunContext ctxt;
            return m_noCheckExpression || !IsGDALGOutput() || RunStep(ctxt);
        });
}

/************************************************************************/
/*                  GDALRasterCalcAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALRasterCalcAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*                GDALRasterCalcAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALRasterCalcAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

    GDALCalcOptions options;
    options.checkExtent = !m_noCheckExtent;
    options.checkCRS = !m_noCheckCRS;
    if (!m_type.empty())
    {
        options.dstType = GDALGetDataTypeByName(m_type.c_str());
    }

    std::vector<std::string> inputFilenames;
    if (!ReadFileLists(m_inputDataset, inputFilenames))
    {
        return false;
    }

    std::vector<std::vector<std::string>> pixelFunctionArgs;
    if (m_dialect == "builtin")
    {
        for (std::string &expr : m_expr)
        {
            const CPLStringList aosTokens(
                CSLTokenizeString2(expr.c_str(), "()",
                                   CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
            const char *pszFunction = aosTokens[0];
            const auto *pair =
                VRTDerivedRasterBand::GetPixelFunction(pszFunction);
            if (!pair)
            {
                ReportError(CE_Failure, CPLE_NotSupported,
                            "'%s' is a unknown builtin function", pszFunction);
                return false;
            }
            if (aosTokens.size() == 2)
            {
                std::vector<std::string> validArguments;
                AddOptionsSuggestions(pair->second.c_str(), 0, std::string(),
                                      validArguments);
                for (std::string &s : validArguments)
                {
                    if (!s.empty() && s.back() == '=')
                        s.pop_back();
                }

                const CPLStringList aosTokensArgs(CSLTokenizeString2(
                    aosTokens[1], ",",
                    CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
                for (const auto &[key, value] :
                     cpl::IterateNameValue(aosTokensArgs))
                {
                    if (std::find(validArguments.begin(), validArguments.end(),
                                  key) == validArguments.end())
                    {
                        if (validArguments.empty())
                        {
                            ReportError(
                                CE_Failure, CPLE_IllegalArg,
                                "'%s' is a unrecognized argument for builtin "
                                "function '%s'. It does not accept any "
                                "argument",
                                key, pszFunction);
                        }
                        else
                        {
                            std::string validArgumentsStr;
                            for (const std::string &s : validArguments)
                            {
                                if (!validArgumentsStr.empty())
                                    validArgumentsStr += ", ";
                                validArgumentsStr += '\'';
                                validArgumentsStr += s;
                                validArgumentsStr += '\'';
                            }
                            ReportError(
                                CE_Failure, CPLE_IllegalArg,
                                "'%s' is a unrecognized argument for builtin "
                                "function '%s'. Only %s %s supported",
                                key, pszFunction,
                                validArguments.size() == 1 ? "is" : "are",
                                validArgumentsStr.c_str());
                        }
                        return false;
                    }
                    CPL_IGNORE_RET_VAL(value);
                }
                pixelFunctionArgs.emplace_back(aosTokensArgs);
            }
            else
            {
                pixelFunctionArgs.push_back(std::vector<std::string>());
            }
            expr = pszFunction;
        }
    }
    else
    {
        pixelFunctionArgs.resize(m_expr.size());
    }

    if (m_propagateNoData)
    {
        if (m_nodata == "none")
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Output NoData value must be specified to use "
                        "--propagate-nodata");
            return false;
        }
        for (auto &args : pixelFunctionArgs)
        {
            args.push_back("propagateNoData=1");
        }
    }

    int maxSourceBands = 0;
    auto vrt = GDALCalcCreateVRTDerived(inputFilenames, m_expr, m_dialect,
                                        m_flatten, m_nodata, pixelFunctionArgs,
                                        options, maxSourceBands);
    if (vrt == nullptr)
    {
        return false;
    }

    if (!m_noCheckExpression)
    {
        const bool bIsVRT =
            m_format == "VRT" ||
            (m_format.empty() &&
             EQUAL(
                 CPLGetExtensionSafe(m_outputDataset.GetName().c_str()).c_str(),
                 "VRT"));
        const bool bIsGDALG =
            m_format == "GDALG" ||
            (m_format.empty() &&
             cpl::ends_with(m_outputDataset.GetName(), ".gdalg.json"));
        if (!m_standaloneStep || m_format == "stream" || bIsVRT || bIsGDALG)
        {
            // Try reading a single pixel to check formulas are valid.
            std::vector<GByte> dummyData(vrt->GetRasterCount());

            auto poGTIFFDrv = GetGDALDriverManager()->GetDriverByName("GTiff");
            std::string osTmpFilename;
            if (poGTIFFDrv)
            {
                std::string osFilename =
                    VSIMemGenerateHiddenFilename("tmp.tif");
                auto poDS = std::unique_ptr<GDALDataset>(
                    poGTIFFDrv->Create(osFilename.c_str(), 1, 1, maxSourceBands,
                                       GDT_UInt8, nullptr));
                if (poDS)
                    osTmpFilename = std::move(osFilename);
            }
            if (!osTmpFilename.empty())
            {
                auto fakeVRT = GDALCalcCreateVRTDerived(
                    inputFilenames, m_expr, m_dialect, m_flatten, m_nodata,
                    pixelFunctionArgs, options, maxSourceBands, osTmpFilename);
                if (fakeVRT &&
                    fakeVRT->RasterIO(GF_Read, 0, 0, 1, 1, dummyData.data(), 1,
                                      1, GDT_UInt8, vrt->GetRasterCount(),
                                      nullptr, 0, 0, 0, nullptr) != CE_None)
                {
                    return false;
                }
            }
            if (bIsGDALG)
            {
                return true;
            }
        }
    }

    if (m_format == "stream" || !m_standaloneStep)
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
    GDALTranslateOptionsSetProgress(translateOptions, ctxt.m_pfnProgress,
                                    ctxt.m_pProgressData);

    auto poOutDS =
        std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALTranslate(
            m_outputDataset.GetName().c_str(), GDALDataset::ToHandle(vrt.get()),
            translateOptions, nullptr)));
    GDALTranslateOptionsFree(translateOptions);

    const bool bOK = poOutDS != nullptr;
    m_outputDataset.Set(std::move(poOutDS));

    return bOK;
}

GDALRasterCalcAlgorithmStandalone::~GDALRasterCalcAlgorithmStandalone() =
    default;

//! @endcond
