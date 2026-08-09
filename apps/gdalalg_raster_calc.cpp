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

constexpr const char *DEFAULT_SOURCE_NAME = "X";
constexpr const char *PIPELINE_INPUT_DSN = "";

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
    bool hasGT{false};
    GDALGeoTransform gt{};
    OGRSpatialReferenceRefCountedPtr srs{};
    std::vector<std::optional<double>> noData{};
    GDALDataType eDT{GDT_Unknown};
};

static std::optional<SourceProperties>
UpdateSourceProperties(SourceProperties &out, GDALDataset *ds,
                       const GDALCalcOptions &options)
{
    SourceProperties source;
    bool srsMismatch = false;
    bool extentMismatch = false;
    bool dimensionMismatch = false;

    {
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

    if (source.gt.xorig != out.gt.xorig || source.gt.xrot != out.gt.xrot ||
        source.gt.yorig != out.gt.yorig || source.gt.yrot != out.gt.yrot)
    {
        extentMismatch = true;
    }
    if (source.gt.xscale != out.gt.xscale || source.gt.yscale != out.gt.yscale)
    {
        // Resolutions are different. Are the extents the same?
        double xmaxOut =
            out.gt.xorig + out.nX * out.gt.xscale + out.nY * out.gt.xrot;
        double yminOut =
            out.gt.yorig + out.nX * out.gt.yrot + out.nY * out.gt.yscale;

        double xmax = source.gt.xorig + source.nX * source.gt.xscale +
                      source.nY * source.gt.xrot;
        double ymin = source.gt.yorig + source.nX * source.gt.yrot +
                      source.nY * source.gt.yscale;

        // Max allowable extent misalignment, expressed as fraction of a pixel
        constexpr double EXTENT_RTOL = 1e-3;

        if (std::abs(xmax - xmaxOut) >
                EXTENT_RTOL * std::abs(source.gt.xscale) ||
            std::abs(ymin - yminOut) > EXTENT_RTOL * std::abs(source.gt.yscale))
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
        auto dx = CPLGreatestCommonDivisor(out.gt.xscale, source.gt.xscale);
        if (dx == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to find common resolution for inputs.");
            return std::nullopt;
        }
        out.nX = static_cast<int>(
            std::round(static_cast<double>(out.nX) * out.gt.xscale / dx));
        out.gt.xscale = dx;
    }
    if (source.nY > out.nY)
    {
        auto dy = CPLGreatestCommonDivisor(out.gt.yscale, source.gt.yscale);
        if (dy == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to find common resolution for inputs.");
            return std::nullopt;
        }
        out.nY = static_cast<int>(
            std::round(static_cast<double>(out.nY) * out.gt.yscale / dy));
        out.gt.yscale = dy;
    }

    if (srsMismatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Input spatial reference systems are inconsistent.");
        return std::nullopt;
    }

    return source;
}

/** Add one or more derived bands to a VRTDataset, representing the evaluation
 *  of a single expression
 *
 * @param poDS VRT dataset
 * @param bandType the type of the band(s) to create
 * @param expression Expression for which band(s) should be added
 * @param dialect Expression dialect
 * @param flatten Generate a single band output raster per expression, even if
 *                input datasets are multiband.
 * @param noDataText nodata value to use for the created band, or "none", or ""
 * @param pixelFunctionArguments Pixel function arguments.
 * @param sources Mapping of source names to DSNs
 * @param sourceProps Mapping of source names to properties
 * @param fakeSourceFilename If not empty, used instead of real input filenames.
 * @param pipelineInputSource A pointer to a dataset representing pipeline input.
 * @return true if the band(s) were added, false otherwise
 */
static bool CreateVRTDerivedBand(
    VRTDataset *poDS, GDALDataType bandType, const std::string &expression,
    const std::string &dialect, bool flatten, const std::string &noDataText,
    const std::vector<std::string> &pixelFunctionArguments,
    const std::map<std::string, std::string> &sources,
    const std::map<std::string, SourceProperties> &sourceProps,
    const std::string &fakeSourceFilename, GDALDataset *pipelineInputSource)
{
    const char *pszVRTFilename = poDS->GetDescription();

    const int nPrevBands = poDS->GetRasterCount();
    const int nXOut = poDS->GetRasterXSize();
    const int nYOut = poDS->GetRasterYSize();

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

        CPLStringList papszBandArgs;
        papszBandArgs.SetNameValue("subclass", "VRTDerivedRasterBand");
        if (poDS->AddBand(bandType == GDT_Unknown ? GDT_Float64 : bandType,
                          papszBandArgs) != CE_None)
        {
            return false;
        }
        VRTDerivedRasterBand *poBand = cpl::down_cast<VRTDerivedRasterBand *>(
            poDS->GetRasterBand(nPrevBands + nOutBand));

        std::optional<double> dstNoData;
        bool autoSelectNoDataValue = false;
        if (noDataText.empty())
        {
            autoSelectNoDataValue = true;
        }
        else if (noDataText != "none")
        {
            if (auto parsed = cpl::strict_parse<double>(noDataText);
                parsed.has_value())
            {
                dstNoData = parsed.value();
            }
            else
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

                std::unique_ptr<VRTSimpleSource> poSource;
                if (srcNoData.has_value())
                {
                    poSource = std::make_unique<VRTComplexSource>();
                }
                else
                {
                    poSource = std::make_unique<VRTSimpleSource>();
                }

                if (!inBandVariable.empty())
                {
                    poSource->SetName(inBandVariable);
                }

                if (fakeSourceFilename.empty())
                {
                    if (dsn == PIPELINE_INPUT_DSN)
                    {
                        CPLAssertNotNull(pipelineInputSource);
                        pipelineInputSource->Reference();
                        poSource->SetSrcBand(
                            pipelineInputSource->GetRasterBand(nInBand));
                    }
                    else
                    {
                        std::string osSourceFilename = dsn;
                        bool bRelativeToVRT = false;
                        if (pszVRTFilename[0])
                        {
                            std::tie(osSourceFilename, bRelativeToVRT) =
                                VRTSimpleSource::
                                    ComputeSourceNameAndRelativeFlag(
                                        CPLGetPathSafe(pszVRTFilename).c_str(),
                                        dsn);
                        }
                        poSource->SetSrcBand(osSourceFilename.c_str(), nInBand);
                    }
                }
                else
                {
                    poSource->SetSrcBand(fakeSourceFilename.c_str(), nInBand);
                }

                if (srcNoData.has_value())
                {
                    cpl::down_cast<VRTComplexSource *>(poSource.get())
                        ->SetNoDataValue(srcNoData.value());

                    if (autoSelectNoDataValue && !dstNoData.has_value())
                    {
                        dstNoData = srcNoData;
                    }
                }

                if (fakeSourceFilename.empty())
                {
                    poSource->SetSrcWindow(0, 0, props.nX, props.nY);
                    poSource->SetDstWindow(0, 0, nXOut, nYOut);
                }

                poBand->AddSource(std::move(poSource));
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

                poBand->SetNoDataValue(dstNoData.value());
            }
        }

        if (dialect == "builtin")
        {
            poBand->SetPixelFunctionName(expression.c_str());
        }
        else
        {
            poBand->SetPixelFunctionName("expression");
            poBand->AddPixelFunctionArgument("dialect", "muparser");
            // Add the expression as a last step, because we may modify the
            // expression as we iterate through the bands.
            poBand->AddPixelFunctionArgument("expression",
                                             bandExpression.c_str());
        }

        if (!pixelFunctionArguments.empty())
        {
            const CPLStringList args(pixelFunctionArguments);
            for (const auto &[key, value] : cpl::IterateNameValue(args))
            {
                poBand->AddPixelFunctionArgument(key, value);
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
            name = DEFAULT_SOURCE_NAME;
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

/** Creates a VRT dataset with one or more derived raster bands containing
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
 * @param inputs Either:
 *               - a list of sources, expressed as NAME=DSN
 *               - pointer to a single opened dataset
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
    std::variant<GDALDataset *, const std::vector<std::string> *> inputs,
    const std::vector<std::string> &expressions, const std::string &dialect,
    bool flatten, const std::string &noData,
    const std::vector<std::vector<std::string>> &pixelFunctionArguments,
    const GDALCalcOptions &options, int &maxSourceBands,
    const std::string &fakeSourceFilename = std::string())
{
    std::map<std::string, std::string> sources;
    std::map<std::string, SourceProperties> sourceProps;
    GDALDataset *pipelineInputDS = std::holds_alternative<GDALDataset *>(inputs)
                                       ? std::get<GDALDataset *>(inputs)
                                       : nullptr;

    maxSourceBands = 0;

    // Read properties from the first source
    SourceProperties out;
    {
        std::unique_ptr<GDALDataset> poTmpDS;
        const GDALDataset *poTemplateDS;

        if (pipelineInputDS)
        {
            poTemplateDS = pipelineInputDS;
        }
        else
        {
            const std::vector<std::string> &sourceDescriptors =
                *std::get<const std::vector<std::string> *>(inputs);

            if (sourceDescriptors.empty())
            {
                return nullptr;
            }

            const bool requireSourceNames = dialect != "builtin";

            std::string firstSource;
            if (!ParseSourceDescriptors(sourceDescriptors, sources, firstSource,
                                        requireSourceNames))
            {
                return nullptr;
            }

            // Use the first source provided to determine properties of the output
            const char *firstDSN = sources[firstSource].c_str();

            poTmpDS.reset(GDALDataset::Open(firstDSN, GDAL_OF_RASTER));
            if (!poTmpDS)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Failed to open %s",
                         firstDSN);
                return nullptr;
            }
            poTemplateDS = poTmpDS.get();
        }

        out.nX = poTemplateDS->GetRasterXSize();
        out.nY = poTemplateDS->GetRasterYSize();
        out.nBands = 1;
        out.srs = OGRSpatialReferenceRefCountedPtr::makeClone(
            poTemplateDS->GetSpatialRef());
        out.hasGT = poTemplateDS->GetGeoTransform(out.gt) == CE_None;

        maxSourceBands = 0;

        if (pipelineInputDS)
        {
            sources[DEFAULT_SOURCE_NAME] = PIPELINE_INPUT_DSN;
            if (auto props =
                    UpdateSourceProperties(out, pipelineInputDS, options))
            {
                sourceProps[DEFAULT_SOURCE_NAME] = props.value();
                maxSourceBands = props.value().nBands;
            }
            else
            {
                return nullptr;  // error message emitted from UpdateSourceProperties
            }
        }
        else
        {
            // Collect properties of the different sources, and verify them for
            // consistency.
            for (const auto &[source_name, dsn] : sources)
            {
                // TODO avoid opening the first source twice.
                std::unique_ptr<GDALDataset> ds(
                    GDALDataset::Open(dsn.c_str(), GDAL_OF_RASTER));

                if (!ds)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Failed to open %s",
                             dsn.c_str());
                    return nullptr;
                }

                auto props = UpdateSourceProperties(out, ds.get(), options);
                if (props.has_value())
                {
                    maxSourceBands = std::max(maxSourceBands, props->nBands);
                    sourceProps[source_name] = std::move(props.value());
                }
                else
                {
                    return nullptr;  // error message emitted from UpdateSourceProperties
                }
            }
        }
    }

    size_t iExpr = 0;

    auto poDS = VRTDataset::CreateVRTDataset("", out.nX, out.nY, 0,
                                             options.dstType, nullptr);

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

        if (!CreateVRTDerivedBand(
                poDS.get(), bandType, origExpression, dialect, flatten, noData,
                pixelFunctionArguments[iExpr], sources, sourceProps,
                fakeSourceFilename, pipelineInputDS))
        {
            return nullptr;
        }
        ++iExpr;
    }

    if (out.hasGT)
    {
        poDS->SetGeoTransform(out.gt);
    }
    if (out.srs)
    {
        poDS->SetSpatialRef(out.srs.get());
    }

    return poDS;
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
/*                  GDALRasterCalcAlgorithm::RunStep()                  */
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

    GDALDataset *poPipelineInput = nullptr;
    std::vector<std::string> inputFilenames;
    if (m_inputDataset.size() == 1 && m_inputDataset[0].GetDatasetRef())
    {
        poPipelineInput = m_inputDataset[0].GetDatasetRef();
    }
    else
    {
        if (!ReadFileLists(m_inputDataset, inputFilenames))
        {
            return false;
        }
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
    const bool bIsVRT =
        m_format == "VRT" ||
        (m_format.empty() &&
         EQUAL(CPLGetExtensionSafe(m_outputDataset.GetName().c_str()).c_str(),
               "VRT"));

    std::variant<GDALDataset *, const std::vector<std::string> *> inputs;
    if (poPipelineInput)
    {
        inputs = poPipelineInput;
    }
    else
    {
        inputs = &inputFilenames;
    }

    auto vrt =
        GDALCalcCreateVRTDerived(inputs, m_expr, m_dialect, m_flatten, m_nodata,
                                 pixelFunctionArgs, options, maxSourceBands);
    if (vrt == nullptr)
    {
        return false;
    }

    if (!m_noCheckExpression)
    {
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
                    &inputFilenames, m_expr, m_dialect, m_flatten, m_nodata,
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

    bool bOK = false;
    GDALTranslateOptions *translateOptions =
        GDALTranslateOptionsNew(translateArgs.List(), nullptr);
    if (translateOptions)
    {
        GDALTranslateOptionsSetProgress(translateOptions, ctxt.m_pfnProgress,
                                        ctxt.m_pProgressData);

        auto poOutDS =
            std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALTranslate(
                m_outputDataset.GetName().c_str(),
                GDALDataset::ToHandle(vrt.get()), translateOptions, nullptr)));
        GDALTranslateOptionsFree(translateOptions);

        bOK = poOutDS != nullptr;
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bOK;
}

GDALRasterCalcAlgorithmStandalone::~GDALRasterCalcAlgorithmStandalone() =
    default;

//! @endcond
