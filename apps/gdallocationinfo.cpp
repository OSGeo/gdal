/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Command line raster query tool.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "cpl_minixml.h"
#include "gdal_version.h"
#include "gdal.h"
#include "commonutils.h"
#include "ogr_spatialref.h"
#include "gdalargumentparser.h"

#include <limits>
#include <vector>

#include <cctype>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/************************************************************************/
/*                             GetSRSAsWKT                              */
/************************************************************************/

static std::string GetSRSAsWKT(const char *pszUserInput)

{
    OGRSpatialReference oSRS;
    oSRS.SetFromUserInput(pszUserInput);
    return oSRS.exportToWkt();
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    double dfGeoX = std::numeric_limits<double>::quiet_NaN();
    double dfGeoY = std::numeric_limits<double>::quiet_NaN();
    std::string osSrcFilename;
    std::string osSourceSRS;
    std::vector<int> anBandList;
    bool bAsXML = false, bLIFOnly = false;
    bool bQuiet = false, bValOnly = false;
    int nOverview = 0;
    CPLStringList aosOpenOptions;
    std::string osFieldSep;
    bool bIgnoreExtraInput = false;
    bool bEcho = false;

    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);
    CPLStringList aosArgv;
    aosArgv.Assign(argv, /* bAssign = */ true);

    GDALArgumentParser argParser(aosArgv[0], /* bForBinary=*/true);

    argParser.add_description(_("Raster query tool."));

    const char *pszEpilog =
        _("For more details, consult "
          "https://gdal.org/programs/gdallocationinfo.html");
    argParser.add_epilog(pszEpilog);

    argParser.add_argument("-xml").flag().store_into(bAsXML).help(
        _("Format the output report as XML."));

    argParser.add_argument("-lifonly")
        .flag()
        .store_into(bLIFOnly)
        .help(_("Only outputs filenames from the LocationInfo request against "
                "the database."));

    argParser.add_argument("-valonly")
        .flag()
        .store_into(bValOnly)
        .help(_("Only outputs pixel values of the selected pixel on each of "
                "the selected bands."));

    argParser.add_argument("-E").flag().store_into(bEcho).help(
        _("Enable Echo mode, where input coordinates are prepended to the "
          "output lines in -valonly mode."));

    argParser.add_argument("-field_sep")
        .metavar("<sep>")
        .store_into(osFieldSep)
        .help(_("Defines the field separator, used in -valonly mode, to "
                "separate different values."));

    argParser.add_argument("-ignore_extra_input")
        .flag()
        .store_into(bIgnoreExtraInput)
        .help(_("Set this flag to avoid extra non-numeric content at end of "
                "input lines."));

    argParser.add_argument("-b")
        .append()
        .metavar("<band>")
        .store_into(anBandList)
        .help(_("Select band(s)."));

    argParser.add_argument("-overview")
        .metavar("<overview_level>")
        .store_into(nOverview)
        .help(_("Query the (overview_level)th overview (overview_level=1 is "
                "the 1st overview)."));

    std::string osResampling;
    argParser.add_argument("-r")
        .store_into(osResampling)
        .metavar("nearest|bilinear|cubic|cubicspline")
        .help(_("Select an interpolation algorithm."));

    {
        auto &group = argParser.add_mutually_exclusive_group();

        group.add_argument("-l_srs")
            .metavar("<srs_def>")
            .store_into(osSourceSRS)
            .help(_("Coordinate system of the input x, y location."));

        group.add_argument("-geoloc")
            .flag()
            .action([&osSourceSRS](const std::string &)
                    { osSourceSRS = "-geoloc"; })
            .help(_("Indicates input x,y points are in the georeferencing "
                    "system of the image."));

        group.add_argument("-wgs84")
            .flag()
            .action([&osSourceSRS](const std::string &)
                    { osSourceSRS = GetSRSAsWKT("WGS84"); })
            .help(_("Indicates input x,y points are WGS84 long, lat."));
    }

    argParser.add_open_options_argument(&aosOpenOptions);

    argParser.add_argument("srcfile")
        .metavar("<srcfile>")
        .nargs(1)
        .store_into(osSrcFilename)
        .help(_("The source GDAL raster datasource name."));

    argParser.add_argument("x")
        .metavar("<x>")
        .nargs(argparse::nargs_pattern::optional)
        .store_into(dfGeoX)
        .help(_("X location of target pixel."));

    argParser.add_argument("y")
        .metavar("<y>")
        .nargs(argparse::nargs_pattern::optional)
        .store_into(dfGeoY)
        .help(_("Y location of target pixel."));

    const auto displayUsage = [&argParser]()
    {
        std::stringstream usageStringStream;
        usageStringStream << argParser.usage();
        std::cerr << CPLString(usageStringStream.str())
                         .replaceAll("<x> <y>", "[<x> <y>]")
                  << std::endl
                  << std::endl;
        std::cout << _("Note: ") << "gdallocationinfo"
                  << _(" --long-usage for full help.") << std::endl;
    };

    try
    {
        argParser.parse_args(aosArgv);
    }
    catch (const std::exception &err)
    {
        std::cerr << _("Error: ") << err.what() << std::endl;
        displayUsage();
        std::exit(1);
    }

    if (bLIFOnly || bValOnly)
        bQuiet = true;

    // User specifies with 1-based index, but internally we use 0-based index
    --nOverview;

    // Deal with special characters
    osFieldSep = CPLString(osFieldSep)
                     .replaceAll("\\t", '\t')
                     .replaceAll("\\r", '\r')
                     .replaceAll("\\n", '\n');

    if (!std::isnan(dfGeoX) && std::isnan(dfGeoY))
    {
        fprintf(stderr, "<y> should be specified when <x> is specified\n\n");
        displayUsage();
        exit(1);
    }

    const bool bIsXYSpecifiedAsArgument = !std::isnan(dfGeoX);

    if (bEcho && !bValOnly)
    {
        fprintf(stderr, "-E can only be used with -valonly\n");
        exit(1);
    }
    if (bEcho && osFieldSep.empty())
    {
        fprintf(stderr, "-E can only be used if -field_sep is specified (to a "
                        "non-newline value)\n");
        exit(1);
    }

    if (osFieldSep.empty())
    {
        osFieldSep = "\n";
    }
    else if (!bValOnly)
    {
        fprintf(stderr, "-field_sep can only be used with -valonly\n");
        exit(1);
    }

    GDALRIOResampleAlg eInterpolation{GRIORA_NearestNeighbour};
    if (osResampling.empty() || STARTS_WITH_CI(osResampling.c_str(), "NEAR"))
    {
        eInterpolation = GRIORA_NearestNeighbour;
    }
    else if (EQUAL(osResampling.c_str(), "BILINEAR"))
    {
        eInterpolation = GRIORA_Bilinear;
    }
    else if (EQUAL(osResampling.c_str(), "CUBICSPLINE"))
    {
        eInterpolation = GRIORA_CubicSpline;
    }
    else if (EQUAL(osResampling.c_str(), "CUBIC"))
    {
        eInterpolation = GRIORA_Cubic;
    }
    else
    {
        fprintf(stderr, "-r can only be used with values nearest, bilinear, "
                        "cubic and cubicspline\n");
        exit(1);
    }

    /* -------------------------------------------------------------------- */
    /*      Open source file.                                               */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS = GDALOpenEx(osSrcFilename.c_str(),
                                     GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                                     nullptr, aosOpenOptions.List(), nullptr);
    if (hSrcDS == nullptr)
        exit(1);

    /* -------------------------------------------------------------------- */
    /*      Setup coordinate transformation, if required                    */
    /* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hSrcSRS = nullptr;
    OGRCoordinateTransformationH hCT = nullptr;
    if (!osSourceSRS.empty() && !EQUAL(osSourceSRS.c_str(), "-geoloc"))
    {
        hSrcSRS = OSRNewSpatialReference(nullptr);
        OGRErr err = OSRSetFromUserInput(hSrcSRS, osSourceSRS.c_str());
        if (err != OGRERR_NONE)
            exit(1);
        OSRSetAxisMappingStrategy(hSrcSRS, OAMS_TRADITIONAL_GIS_ORDER);
        auto hTrgSRS = GDALGetSpatialRef(hSrcDS);
        if (!hTrgSRS)
            exit(1);

        hCT = OCTNewCoordinateTransformation(hSrcSRS, hTrgSRS);
        if (hCT == nullptr)
            exit(1);
    }

    /* -------------------------------------------------------------------- */
    /*      If no bands were requested, we will query them all.             */
    /* -------------------------------------------------------------------- */
    if (anBandList.empty())
    {
        for (int i = 0; i < GDALGetRasterCount(hSrcDS); i++)
            anBandList.push_back(i + 1);
    }

    /* -------------------------------------------------------------------- */
    /*      Turn the location into a pixel and line location.               */
    /* -------------------------------------------------------------------- */
    bool inputAvailable = true;
    CPLString osXML;
    char szLine[1024];
    int nLine = 0;
    std::string osExtraContent;

    if (std::isnan(dfGeoX))
    {
        // Is it an interactive terminal ?
        if (isatty(static_cast<int>(fileno(stdin))))
        {
            if (!osSourceSRS.empty())
            {
                fprintf(stderr, "Enter X Y values separated by space, and "
                                "press Return.\n");
            }
            else
            {
                fprintf(stderr, "Enter pixel line values separated by space, "
                                "and press Return.\n");
            }
        }

        if (fgets(szLine, sizeof(szLine) - 1, stdin))
        {
            const CPLStringList aosTokens(CSLTokenizeString(szLine));
            const int nCount = aosTokens.size();

            ++nLine;
            if (nCount < 2)
            {
                fprintf(stderr, "Not enough values at line %d\n", nLine);
                inputAvailable = false;
            }
            else
            {
                dfGeoX = CPLAtof(aosTokens[0]);
                dfGeoY = CPLAtof(aosTokens[1]);
                if (!bIgnoreExtraInput)
                {
                    for (int i = 2; i < nCount; ++i)
                    {
                        if (!osExtraContent.empty())
                            osExtraContent += ' ';
                        osExtraContent += aosTokens[i];
                    }
                    while (!osExtraContent.empty() &&
                           isspace(static_cast<int>(osExtraContent.back())))
                    {
                        osExtraContent.pop_back();
                    }
                }
            }
        }
        else
        {
            inputAvailable = false;
        }
    }

    int nRetCode = 0;
    while (inputAvailable)
    {
        int iPixel, iLine;
        double dfPixel{0}, dfLine{0};

        if (hCT)
        {
            if (!OCTTransform(hCT, 1, &dfGeoX, &dfGeoY, nullptr))
                exit(1);
        }

        if (!osSourceSRS.empty())
        {
            double adfGeoTransform[6] = {};
            if (GDALGetGeoTransform(hSrcDS, adfGeoTransform) != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot get geotransform");
                exit(1);
            }

            double adfInvGeoTransform[6] = {};
            if (!GDALInvGeoTransform(adfGeoTransform, adfInvGeoTransform))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot invert geotransform");
                exit(1);
            }

            dfPixel = adfInvGeoTransform[0] + adfInvGeoTransform[1] * dfGeoX +
                      adfInvGeoTransform[2] * dfGeoY;
            dfLine = adfInvGeoTransform[3] + adfInvGeoTransform[4] * dfGeoX +
                     adfInvGeoTransform[5] * dfGeoY;
        }
        else
        {
            dfPixel = dfGeoX;
            dfLine = dfGeoY;
        }
        iPixel = static_cast<int>(floor(dfPixel));
        iLine = static_cast<int>(floor(dfLine));

        /* --------------------------------------------------------------------
         */
        /*      Prepare report. */
        /* --------------------------------------------------------------------
         */
        CPLString osXmlLine;

        if (bAsXML)
        {
            osXmlLine.Printf("<Report pixel=\"%d\" line=\"%d\">", iPixel,
                             iLine);
            osXML += osXmlLine;
            if (!osExtraContent.empty())
            {
                char *pszEscaped =
                    CPLEscapeString(osExtraContent.c_str(), -1, CPLES_XML);
                osXML += CPLString().Printf("  <ExtraInput>%s</ExtraInput>",
                                            pszEscaped);
                CPLFree(pszEscaped);
            }
        }
        else if (!bQuiet)
        {
            printf("Report:\n");
            CPLString osPixel, osLine;
            if (eInterpolation == GRIORA_NearestNeighbour)
            {
                osPixel.Printf("%d", iPixel);
                osLine.Printf("%d", iLine);
            }
            else
            {
                osPixel.Printf("%.15g", dfPixel);
                osLine.Printf("%.15g", dfLine);
            }
            printf("  Location: (%sP,%sL)\n", osPixel.c_str(), osLine.c_str());
            if (!osExtraContent.empty())
            {
                printf("  Extra input: %s\n", osExtraContent.c_str());
            }
        }
        else if (bEcho)
        {
            printf("%d%s%d%s", iPixel, osFieldSep.c_str(), iLine,
                   osFieldSep.c_str());
        }

        bool bPixelReport = true;

        if (iPixel < 0 || iLine < 0 || iPixel >= GDALGetRasterXSize(hSrcDS) ||
            iLine >= GDALGetRasterYSize(hSrcDS))
        {
            if (bAsXML)
                osXML += "<Alert>Location is off this file! No further details "
                         "to report.</Alert>";
            else if (bValOnly)
            {
                for (int i = 1; i < static_cast<int>(anBandList.size()); i++)
                {
                    printf("%s", osFieldSep.c_str());
                }
            }
            else if (!bQuiet)
                printf("\nLocation is off this file! No further details to "
                       "report.\n");
            bPixelReport = false;
            nRetCode = 1;
        }

        /* --------------------------------------------------------------------
         */
        /*      Process each band. */
        /* --------------------------------------------------------------------
         */
        for (int i = 0; bPixelReport && i < static_cast<int>(anBandList.size());
             i++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hSrcDS, anBandList[i]);

            int iPixelToQuery = iPixel;
            int iLineToQuery = iLine;

            double dfPixelToQuery = dfPixel;
            double dfLineToQuery = dfLine;

            if (nOverview >= 0 && hBand != nullptr)
            {
                GDALRasterBandH hOvrBand = GDALGetOverview(hBand, nOverview);
                if (hOvrBand != nullptr)
                {
                    int nOvrXSize = GDALGetRasterBandXSize(hOvrBand);
                    int nOvrYSize = GDALGetRasterBandYSize(hOvrBand);
                    iPixelToQuery = static_cast<int>(
                        0.5 +
                        1.0 * iPixel / GDALGetRasterXSize(hSrcDS) * nOvrXSize);
                    iLineToQuery = static_cast<int>(
                        0.5 +
                        1.0 * iLine / GDALGetRasterYSize(hSrcDS) * nOvrYSize);
                    if (iPixelToQuery >= nOvrXSize)
                        iPixelToQuery = nOvrXSize - 1;
                    if (iLineToQuery >= nOvrYSize)
                        iLineToQuery = nOvrYSize - 1;
                    dfPixelToQuery =
                        dfPixel / GDALGetRasterXSize(hSrcDS) * nOvrXSize;
                    dfLineToQuery =
                        dfLine / GDALGetRasterYSize(hSrcDS) * nOvrYSize;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot get overview %d of band %d", nOverview + 1,
                             anBandList[i]);
                }
                hBand = hOvrBand;
            }

            if (hBand == nullptr)
                continue;

            if (bAsXML)
            {
                osXmlLine.Printf("<BandReport band=\"%d\">", anBandList[i]);
                osXML += osXmlLine;
            }
            else if (!bQuiet)
            {
                printf("  Band %d:\n", anBandList[i]);
            }

            /* --------------------------------------------------------------------
             */
            /*      Request location info for this location.  It is possible */
            /*      only the VRT driver actually supports this. */
            /* --------------------------------------------------------------------
             */
            CPLString osItem;

            osItem.Printf("Pixel_%d_%d", iPixelToQuery, iLineToQuery);

            const char *pszLI =
                GDALGetMetadataItem(hBand, osItem, "LocationInfo");

            if (pszLI != nullptr)
            {
                if (bAsXML)
                    osXML += pszLI;
                else if (!bQuiet)
                    printf("    %s\n", pszLI);
                else if (bLIFOnly)
                {
                    /* Extract all files, if any. */

                    CPLXMLNode *psRoot = CPLParseXMLString(pszLI);

                    if (psRoot != nullptr && psRoot->psChild != nullptr &&
                        psRoot->eType == CXT_Element &&
                        EQUAL(psRoot->pszValue, "LocationInfo"))
                    {
                        for (CPLXMLNode *psNode = psRoot->psChild;
                             psNode != nullptr; psNode = psNode->psNext)
                        {
                            if (psNode->eType == CXT_Element &&
                                EQUAL(psNode->pszValue, "File") &&
                                psNode->psChild != nullptr)
                            {
                                char *pszUnescaped =
                                    CPLUnescapeString(psNode->psChild->pszValue,
                                                      nullptr, CPLES_XML);
                                printf("%s\n", pszUnescaped);
                                CPLFree(pszUnescaped);
                            }
                        }
                    }
                    CPLDestroyXMLNode(psRoot);
                }
            }

            /* --------------------------------------------------------------------
             */
            /*      Report the pixel value of this band. */
            /* --------------------------------------------------------------------
             */
            double adfPixel[2] = {0, 0};
            const bool bIsComplex = CPL_TO_BOOL(
                GDALDataTypeIsComplex(GDALGetRasterDataType(hBand)));

            CPLErr err;
            err = GDALRasterInterpolateAtPoint(hBand, dfPixelToQuery,
                                               dfLineToQuery, eInterpolation,
                                               &adfPixel[0], &adfPixel[1]);

            if (err == CE_None)
            {
                CPLString osValue;

                if (bIsComplex)
                    osValue.Printf("%.15g+%.15gi", adfPixel[0], adfPixel[1]);
                else
                    osValue.Printf("%.15g", adfPixel[0]);

                if (bAsXML)
                {
                    osXML += "<Value>";
                    osXML += osValue;
                    osXML += "</Value>";
                }
                else if (!bQuiet)
                    printf("    Value: %s\n", osValue.c_str());
                else if (bValOnly)
                {
                    if (i > 0)
                        printf("%s", osFieldSep.c_str());
                    printf("%s", osValue.c_str());
                }

                // Report unscaled if we have scale/offset values.
                int bSuccess;

                double dfOffset = GDALGetRasterOffset(hBand, &bSuccess);
                // TODO: Should we turn on checking of bSuccess?
                // Alternatively, delete these checks and put a comment as to
                // why checking bSuccess does not matter.
#if 0
                if (bSuccess == FALSE)
                {
                    CPLError( CE_Debug, CPLE_AppDefined,
                              "Unable to get raster offset." );
                }
#endif
                double dfScale = GDALGetRasterScale(hBand, &bSuccess);
#if 0
                if (bSuccess == FALSE)
                {
                    CPLError( CE_Debug, CPLE_AppDefined,
                              "Unable to get raster scale." );
                }
#endif
                if (dfOffset != 0.0 || dfScale != 1.0)
                {
                    adfPixel[0] = adfPixel[0] * dfScale + dfOffset;

                    if (bIsComplex)
                    {
                        adfPixel[1] = adfPixel[1] * dfScale + dfOffset;
                        osValue.Printf("%.15g+%.15gi", adfPixel[0],
                                       adfPixel[1]);
                    }
                    else
                        osValue.Printf("%.15g", adfPixel[0]);

                    if (bAsXML)
                    {
                        osXML += "<DescaledValue>";
                        osXML += osValue;
                        osXML += "</DescaledValue>";
                    }
                    else if (!bQuiet)
                        printf("    Descaled Value: %s\n", osValue.c_str());
                }
            }

            if (bAsXML)
                osXML += "</BandReport>";
        }

        osXML += "</Report>";

        if (bValOnly)
        {
            if (!osExtraContent.empty() && osFieldSep != "\n")
                printf("%s%s", osFieldSep.c_str(), osExtraContent.c_str());
            printf("\n");
        }

        if (bIsXYSpecifiedAsArgument)
            break;

        osExtraContent.clear();
        if (fgets(szLine, sizeof(szLine) - 1, stdin))
        {
            const CPLStringList aosTokens(CSLTokenizeString(szLine));
            const int nCount = aosTokens.size();

            ++nLine;
            if (nCount < 2)
            {
                fprintf(stderr, "Not enough values at line %d\n", nLine);
                continue;
            }
            else
            {
                dfGeoX = CPLAtof(aosTokens[0]);
                dfGeoY = CPLAtof(aosTokens[1]);
                if (!bIgnoreExtraInput)
                {
                    for (int i = 2; i < nCount; ++i)
                    {
                        if (!osExtraContent.empty())
                            osExtraContent += ' ';
                        osExtraContent += aosTokens[i];
                    }
                    while (!osExtraContent.empty() &&
                           isspace(static_cast<int>(osExtraContent.back())))
                    {
                        osExtraContent.pop_back();
                    }
                }
            }
        }
        else
        {
            break;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Finalize xml report and print.                                  */
    /* -------------------------------------------------------------------- */
    if (bAsXML)
    {
        CPLXMLNode *psRoot = CPLParseXMLString(osXML);
        char *pszFormattedXML = CPLSerializeXMLTree(psRoot);
        CPLDestroyXMLNode(psRoot);

        printf("%s", pszFormattedXML);
        CPLFree(pszFormattedXML);
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    if (hCT)
    {
        OSRDestroySpatialReference(hSrcSRS);
        OCTDestroyCoordinateTransformation(hCT);
    }

    GDALClose(hSrcDS);

    GDALDumpOpenDatasets(stderr);
    GDALDestroyDriverManager();

    return nRetCode;
}

MAIN_END
