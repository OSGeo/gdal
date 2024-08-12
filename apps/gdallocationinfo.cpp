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
#include <vector>

#include <cctype>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(bool bIsError)

{
    fprintf(
        bIsError ? stderr : stdout,
        "Usage: gdallocationinfo [--help] [--help-general]\n"
        "                        [-xml] [-lifonly] [-valonly]\n"
        "                        [-E] [-field_sep <sep>] "
        "[-ignore_extra_input]\n"
        "                        [-b <band>]... [-overview <overview_level>]\n"
        "                        [-l_srs <srs_def>] [-geoloc] [-wgs84]\n"
        "                        [-oo <NAME>=<VALUE>]... <srcfile> [<x> <y>]\n"
        "\n");
    exit(1);
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

static char *SanitizeSRS(const char *pszUserInput)

{
    CPLErrorReset();

    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);

    char *pszResult = nullptr;
    if (OSRSetFromUserInput(hSRS, pszUserInput) == OGRERR_NONE)
        OSRExportToWkt(hSRS, &pszResult);
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Translating source or target SRS failed:\n%s", pszUserInput);
        exit(1);
    }

    OSRDestroySpatialReference(hSRS);

    return pszResult;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    const char *pszLocX = nullptr, *pszLocY = nullptr;
    const char *pszSrcFilename = nullptr;
    char *pszSourceSRS = nullptr;
    std::vector<int> anBandList;
    bool bAsXML = false, bLIFOnly = false;
    bool bQuiet = false, bValOnly = false;
    int nOverview = -1;
    char **papszOpenOptions = nullptr;
    std::string osFieldSep;
    bool bIgnoreExtraInput = false;
    bool bEcho = false;

    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    /* -------------------------------------------------------------------- */
    /*      Parse arguments.                                                */
    /* -------------------------------------------------------------------- */
    for (int i = 1; i < argc; i++)
    {
        if (EQUAL(argv[i], "--utility_version"))
        {
            printf("%s was compiled against GDAL %s and is running against "
                   "GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            GDALDestroyDriverManager();
            CSLDestroy(argv);
            return 0;
        }
        else if (EQUAL(argv[i], "--help"))
        {
            Usage(false);
        }
        else if (i < argc - 1 && EQUAL(argv[i], "-b"))
        {
            anBandList.push_back(atoi(argv[++i]));
        }
        else if (i < argc - 1 && EQUAL(argv[i], "-overview"))
        {
            nOverview = atoi(argv[++i]) - 1;
        }
        else if (i < argc - 1 && EQUAL(argv[i], "-l_srs"))
        {
            CPLFree(pszSourceSRS);
            // coverity[tainted_data]
            pszSourceSRS = SanitizeSRS(argv[++i]);
        }
        else if (EQUAL(argv[i], "-geoloc"))
        {
            CPLFree(pszSourceSRS);
            pszSourceSRS = CPLStrdup("-geoloc");
        }
        else if (EQUAL(argv[i], "-wgs84"))
        {
            CPLFree(pszSourceSRS);
            pszSourceSRS = SanitizeSRS("WGS84");
        }
        else if (EQUAL(argv[i], "-xml"))
        {
            bAsXML = true;
        }
        else if (EQUAL(argv[i], "-lifonly"))
        {
            bLIFOnly = true;
            bQuiet = true;
        }
        else if (EQUAL(argv[i], "-valonly"))
        {
            bValOnly = true;
            bQuiet = true;
        }
        else if (i < argc - 1 && EQUAL(argv[i], "-field_sep"))
        {
            osFieldSep = CPLString(argv[++i])
                             .replaceAll("\\t", '\t')
                             .replaceAll("\\r", '\r')
                             .replaceAll("\\n", '\n');
        }
        else if (EQUAL(argv[i], "-ignore_extra_input"))
        {
            bIgnoreExtraInput = true;
        }
        else if (EQUAL(argv[i], "-E"))
        {
            bEcho = true;
        }
        else if (i < argc - 1 && EQUAL(argv[i], "-oo"))
        {
            papszOpenOptions = CSLAddString(papszOpenOptions, argv[++i]);
        }
        else if (argv[i][0] == '-' &&
                 !isdigit(static_cast<unsigned char>(argv[i][1])))
            Usage(true);

        else if (pszSrcFilename == nullptr)
            pszSrcFilename = argv[i];

        else if (pszLocX == nullptr)
            pszLocX = argv[i];

        else if (pszLocY == nullptr)
            pszLocY = argv[i];

        else
            Usage(true);
    }

    if (pszSrcFilename == nullptr || (pszLocX != nullptr && pszLocY == nullptr))
        Usage(true);

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

    /* -------------------------------------------------------------------- */
    /*      Open source file.                                               */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS =
        GDALOpenEx(pszSrcFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                   nullptr, papszOpenOptions, nullptr);
    if (hSrcDS == nullptr)
        exit(1);

    /* -------------------------------------------------------------------- */
    /*      Setup coordinate transformation, if required                    */
    /* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hSrcSRS = nullptr;
    OGRCoordinateTransformationH hCT = nullptr;
    if (pszSourceSRS != nullptr && !EQUAL(pszSourceSRS, "-geoloc"))
    {

        hSrcSRS = OSRNewSpatialReference(pszSourceSRS);
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
    double dfGeoX = 0;
    double dfGeoY = 0;
    CPLString osXML;
    char szLine[1024];
    int nLine = 0;
    std::string osExtraContent;

    if (pszLocX == nullptr && pszLocY == nullptr)
    {
        // Is it an interactive terminal ?
        if (isatty(static_cast<int>(fileno(stdin))))
        {
            if (pszSourceSRS != nullptr)
            {
                fprintf(
                    stderr,
                    "Enter X Y values separated by space, and press Return.\n");
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
    else
    {
        dfGeoX = CPLAtof(pszLocX);
        dfGeoY = CPLAtof(pszLocY);
    }

    int nRetCode = 0;
    while (inputAvailable)
    {
        int iPixel, iLine;
        const double dfXIn = dfGeoX;
        const double dfYIn = dfGeoY;

        if (hCT)
        {
            if (!OCTTransform(hCT, 1, &dfGeoX, &dfGeoY, nullptr))
                exit(1);
        }

        if (pszSourceSRS != nullptr)
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

            iPixel = static_cast<int>(floor(adfInvGeoTransform[0] +
                                            adfInvGeoTransform[1] * dfGeoX +
                                            adfInvGeoTransform[2] * dfGeoY));
            iLine = static_cast<int>(floor(adfInvGeoTransform[3] +
                                           adfInvGeoTransform[4] * dfGeoX +
                                           adfInvGeoTransform[5] * dfGeoY));
        }
        else
        {
            iPixel = static_cast<int>(floor(dfGeoX));
            iLine = static_cast<int>(floor(dfGeoY));
        }

        /* --------------------------------------------------------------------
         */
        /*      Prepare report. */
        /* --------------------------------------------------------------------
         */
        CPLString osLine;

        if (bAsXML)
        {
            osLine.Printf("<Report pixel=\"%d\" line=\"%d\">", iPixel, iLine);
            osXML += osLine;
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
            printf("  Location: (%dP,%dL)\n", iPixel, iLine);
            if (!osExtraContent.empty())
            {
                printf("  Extra input: %s\n", osExtraContent.c_str());
            }
        }
        else if (bEcho)
        {
            printf("%.15g%s%.15g%s", dfXIn, osFieldSep.c_str(), dfYIn,
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
                osLine.Printf("<BandReport band=\"%d\">", anBandList[i]);
                osXML += osLine;
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

            if (GDALRasterIO(hBand, GF_Read, iPixelToQuery, iLineToQuery, 1, 1,
                             adfPixel, 1, 1,
                             bIsComplex ? GDT_CFloat64 : GDT_Float64, 0,
                             0) == CE_None)
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

        if (pszLocX != nullptr && pszLocY != nullptr)
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
    CPLFree(pszSourceSRS);
    CSLDestroy(papszOpenOptions);

    CSLDestroy(argv);

    return nRetCode;
}

MAIN_END
