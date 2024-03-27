/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_port.h"  // Must be first.

#include "gtiff.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "gdal.h"
#include "gdal_mdreader.h"  // RPC_xxx
#include "gtiffdataset.h"
#include "tiffio.h"
#include "tif_jxl.h"
#include "xtiffio.h"
#include <cctype>

// Needed to expose WEBP_LOSSLESS option
#ifdef WEBP_SUPPORT
#include "webp/encode.h"
#endif

#ifdef LERC_SUPPORT
#include "Lerc_c_api.h"
#endif

#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)

static thread_local bool bThreadLocalInExternalOvr = false;

static thread_local int gnThreadLocalLibtiffError = 0;

int &GTIFFGetThreadLocalLibtiffError()
{
    return gnThreadLocalLibtiffError;
}

/************************************************************************/
/*                         GTIFFSupportsPredictor()                     */
/************************************************************************/

bool GTIFFSupportsPredictor(int nCompression)
{
    return nCompression == COMPRESSION_LZW ||
           nCompression == COMPRESSION_ADOBE_DEFLATE ||
           nCompression == COMPRESSION_ZSTD;
}

/************************************************************************/
/*                     GTIFFSetThreadLocalInExternalOvr()               */
/************************************************************************/

void GTIFFSetThreadLocalInExternalOvr(bool b)
{
    bThreadLocalInExternalOvr = b;
}

/************************************************************************/
/*                     GTIFFGetOverviewBlockSize()                      */
/************************************************************************/

void GTIFFGetOverviewBlockSize(GDALRasterBandH hBand, int *pnBlockXSize,
                               int *pnBlockYSize)
{
    const char *pszVal = CPLGetConfigOption("GDAL_TIFF_OVR_BLOCKSIZE", nullptr);
    if (!pszVal)
    {
        GDALRasterBand *const poBand = GDALRasterBand::FromHandle(hBand);
        poBand->GetBlockSize(pnBlockXSize, pnBlockYSize);
        if (*pnBlockXSize != *pnBlockYSize || *pnBlockXSize < 64 ||
            *pnBlockXSize > 4096 || !CPLIsPowerOfTwo(*pnBlockXSize))
        {
            *pnBlockXSize = *pnBlockYSize = 128;
        }
    }
    else
    {
        int nOvrBlockSize = atoi(pszVal);
        if (nOvrBlockSize < 64 || nOvrBlockSize > 4096 ||
            !CPLIsPowerOfTwo(nOvrBlockSize))
        {
            static bool bHasWarned = false;
            if (!bHasWarned)
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Wrong value for GDAL_TIFF_OVR_BLOCKSIZE : %s. "
                         "Should be a power of 2 between 64 and 4096. "
                         "Defaulting to 128",
                         pszVal);
                bHasWarned = true;
            }
            nOvrBlockSize = 128;
        }

        *pnBlockXSize = nOvrBlockSize;
        *pnBlockYSize = nOvrBlockSize;
    }
}

/************************************************************************/
/*                        GTIFFSetJpegQuality()                         */
/* Called by GTIFFBuildOverviews() to set the jpeg quality on the IFD   */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetJpegQuality(GDALDatasetH hGTIFFDS, int nJpegQuality)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nJpegQuality = static_cast<signed char>(nJpegQuality);

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
        poDS->m_papoOverviewDS[i]->m_nJpegQuality = poDS->m_nJpegQuality;
}

/************************************************************************/
/*                        GTIFFSetWebPLevel()                         */
/* Called by GTIFFBuildOverviews() to set the jpeg quality on the IFD   */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetWebPLevel(GDALDatasetH hGTIFFDS, int nWebpLevel)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nWebPLevel = static_cast<signed char>(nWebpLevel);

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
        poDS->m_papoOverviewDS[i]->m_nWebPLevel = poDS->m_nWebPLevel;
}

/************************************************************************/
/*                       GTIFFSetWebPLossless()                         */
/* Called by GTIFFBuildOverviews() to set webp lossless on the IFD      */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetWebPLossless(GDALDatasetH hGTIFFDS, bool bWebpLossless)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_bWebPLossless = bWebpLossless;

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
        poDS->m_papoOverviewDS[i]->m_bWebPLossless = poDS->m_bWebPLossless;
}

/************************************************************************/
/*                     GTIFFSetJpegTablesMode()                         */
/* Called by GTIFFBuildOverviews() to set the jpeg tables mode on the   */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetJpegTablesMode(GDALDatasetH hGTIFFDS, int nJpegTablesMode)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nJpegTablesMode = static_cast<signed char>(nJpegTablesMode);

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
        poDS->m_papoOverviewDS[i]->m_nJpegTablesMode = poDS->m_nJpegTablesMode;
}

/************************************************************************/
/*                        GTIFFSetZLevel()                              */
/* Called by GTIFFBuildOverviews() to set the deflate level on the IFD  */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetZLevel(GDALDatasetH hGTIFFDS, int nZLevel)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nZLevel = static_cast<signed char>(nZLevel);

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
        poDS->m_papoOverviewDS[i]->m_nZLevel = poDS->m_nZLevel;
}

/************************************************************************/
/*                        GTIFFSetZSTDLevel()                           */
/* Called by GTIFFBuildOverviews() to set the ZSTD level on the IFD     */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetZSTDLevel(GDALDatasetH hGTIFFDS, int nZSTDLevel)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nZSTDLevel = static_cast<signed char>(nZSTDLevel);

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
        poDS->m_papoOverviewDS[i]->m_nZSTDLevel = poDS->m_nZSTDLevel;
}

/************************************************************************/
/*                        GTIFFSetMaxZError()                           */
/* Called by GTIFFBuildOverviews() to set the Lerc max error on the IFD */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetMaxZError(GDALDatasetH hGTIFFDS, double dfMaxZError)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_dfMaxZError = dfMaxZError;
    poDS->m_dfMaxZErrorOverview = dfMaxZError;

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
    {
        poDS->m_papoOverviewDS[i]->m_dfMaxZError = poDS->m_dfMaxZError;
        poDS->m_papoOverviewDS[i]->m_dfMaxZErrorOverview =
            poDS->m_dfMaxZErrorOverview;
    }
}

#if HAVE_JXL

/************************************************************************/
/*                       GTIFFSetJXLLossless()                          */
/* Called by GTIFFBuildOverviews() to set the JXL lossyness on the IFD  */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetJXLLossless(GDALDatasetH hGTIFFDS, bool bIsLossless)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_bJXLLossless = bIsLossless;

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
    {
        poDS->m_papoOverviewDS[i]->m_bJXLLossless = poDS->m_bJXLLossless;
    }
}

/************************************************************************/
/*                       GTIFFSetJXLEffort()                            */
/* Called by GTIFFBuildOverviews() to set the JXL effort on the IFD     */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetJXLEffort(GDALDatasetH hGTIFFDS, int nEffort)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nJXLEffort = nEffort;

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
    {
        poDS->m_papoOverviewDS[i]->m_nJXLEffort = poDS->m_nJXLEffort;
    }
}

/************************************************************************/
/*                       GTIFFSetJXLDistance()                          */
/* Called by GTIFFBuildOverviews() to set the JXL distance on the IFD   */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetJXLDistance(GDALDatasetH hGTIFFDS, float fDistance)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_fJXLDistance = fDistance;

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
    {
        poDS->m_papoOverviewDS[i]->m_fJXLDistance = poDS->m_fJXLDistance;
    }
}

/************************************************************************/
/*                     GTIFFSetJXLAlphaDistance()                       */
/* Called by GTIFFBuildOverviews() to set the JXL alpha distance on the */
/* IFD of the .ovr file.                                                */
/************************************************************************/

void GTIFFSetJXLAlphaDistance(GDALDatasetH hGTIFFDS, float fAlphaDistance)
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset *const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_fJXLAlphaDistance = fAlphaDistance;

    poDS->ScanDirectories();

    for (int i = 0; i < poDS->m_nOverviewCount; ++i)
    {
        poDS->m_papoOverviewDS[i]->m_fJXLAlphaDistance =
            poDS->m_fJXLAlphaDistance;
    }
}

#endif  // HAVE_JXL

/************************************************************************/
/*                         GTiffGetAlphaValue()                         */
/************************************************************************/

uint16_t GTiffGetAlphaValue(const char *pszValue, uint16_t nDefault)
{
    if (pszValue == nullptr)
        return nDefault;
    if (EQUAL(pszValue, "YES"))
        return DEFAULT_ALPHA_TYPE;
    if (EQUAL(pszValue, "PREMULTIPLIED"))
        return EXTRASAMPLE_ASSOCALPHA;
    if (EQUAL(pszValue, "NON-PREMULTIPLIED"))
        return EXTRASAMPLE_UNASSALPHA;
    if (EQUAL(pszValue, "NO") || EQUAL(pszValue, "UNSPECIFIED"))
        return EXTRASAMPLE_UNSPECIFIED;

    return nDefault;
}

/************************************************************************/
/*                 GTIFFIsStandardColorInterpretation()                 */
/************************************************************************/

bool GTIFFIsStandardColorInterpretation(GDALDatasetH hSrcDS,
                                        uint16_t nPhotometric,
                                        CSLConstList papszCreationOptions)
{
    GDALDataset *poSrcDS = GDALDataset::FromHandle(hSrcDS);
    bool bStandardColorInterp = true;
    if (nPhotometric == PHOTOMETRIC_MINISBLACK)
    {
        for (int i = 0; i < poSrcDS->GetRasterCount(); ++i)
        {
            const GDALColorInterp eInterp =
                poSrcDS->GetRasterBand(i + 1)->GetColorInterpretation();
            if (!(eInterp == GCI_GrayIndex || eInterp == GCI_Undefined ||
                  (i > 0 && eInterp == GCI_AlphaBand)))
            {
                bStandardColorInterp = false;
                break;
            }
        }
    }
    else if (nPhotometric == PHOTOMETRIC_PALETTE)
    {
        bStandardColorInterp =
            poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
            GCI_PaletteIndex;
    }
    else if (nPhotometric == PHOTOMETRIC_RGB)
    {
        int iStart = 0;
        if (EQUAL(CSLFetchNameValueDef(papszCreationOptions, "PHOTOMETRIC", ""),
                  "RGB"))
        {
            iStart = 3;
            if (poSrcDS->GetRasterCount() == 4 &&
                CSLFetchNameValue(papszCreationOptions, "ALPHA") != nullptr)
            {
                iStart = 4;
            }
        }
        for (int i = iStart; i < poSrcDS->GetRasterCount(); ++i)
        {
            const GDALColorInterp eInterp =
                poSrcDS->GetRasterBand(i + 1)->GetColorInterpretation();
            if (!((i == 0 && eInterp == GCI_RedBand) ||
                  (i == 1 && eInterp == GCI_GreenBand) ||
                  (i == 2 && eInterp == GCI_BlueBand) ||
                  (i >= 3 &&
                   (eInterp == GCI_Undefined || eInterp == GCI_AlphaBand))))
            {
                bStandardColorInterp = false;
                break;
            }
        }
    }
    else if (nPhotometric == PHOTOMETRIC_YCBCR &&
             poSrcDS->GetRasterCount() == 3)
    {
        // do nothing
    }
    else
    {
        bStandardColorInterp = false;
    }
    return bStandardColorInterp;
}

/************************************************************************/
/*                     GTiffDatasetWriteRPCTag()                        */
/*                                                                      */
/*      Format a TAG according to:                                      */
/*                                                                      */
/*      http://geotiff.maptools.org/rpc_prop.html                       */
/************************************************************************/

void GTiffDatasetWriteRPCTag(TIFF *hTIFF, char **papszRPCMD)

{
    GDALRPCInfoV2 sRPC;

    if (!GDALExtractRPCInfoV2(papszRPCMD, &sRPC))
        return;

    double adfRPCTag[92] = {};
    adfRPCTag[0] = sRPC.dfERR_BIAS;  // Error Bias
    adfRPCTag[1] = sRPC.dfERR_RAND;  // Error Random

    adfRPCTag[2] = sRPC.dfLINE_OFF;
    adfRPCTag[3] = sRPC.dfSAMP_OFF;
    adfRPCTag[4] = sRPC.dfLAT_OFF;
    adfRPCTag[5] = sRPC.dfLONG_OFF;
    adfRPCTag[6] = sRPC.dfHEIGHT_OFF;
    adfRPCTag[7] = sRPC.dfLINE_SCALE;
    adfRPCTag[8] = sRPC.dfSAMP_SCALE;
    adfRPCTag[9] = sRPC.dfLAT_SCALE;
    adfRPCTag[10] = sRPC.dfLONG_SCALE;
    adfRPCTag[11] = sRPC.dfHEIGHT_SCALE;

    memcpy(adfRPCTag + 12, sRPC.adfLINE_NUM_COEFF, sizeof(double) * 20);
    memcpy(adfRPCTag + 32, sRPC.adfLINE_DEN_COEFF, sizeof(double) * 20);
    memcpy(adfRPCTag + 52, sRPC.adfSAMP_NUM_COEFF, sizeof(double) * 20);
    memcpy(adfRPCTag + 72, sRPC.adfSAMP_DEN_COEFF, sizeof(double) * 20);

    TIFFSetField(hTIFF, TIFFTAG_RPCCOEFFICIENT, 92, adfRPCTag);
}

/************************************************************************/
/*                             ReadRPCTag()                             */
/*                                                                      */
/*      Format a TAG according to:                                      */
/*                                                                      */
/*      http://geotiff.maptools.org/rpc_prop.html                       */
/************************************************************************/

char **GTiffDatasetReadRPCTag(TIFF *hTIFF)

{
    double *padfRPCTag = nullptr;
    uint16_t nCount;

    if (!TIFFGetField(hTIFF, TIFFTAG_RPCCOEFFICIENT, &nCount, &padfRPCTag) ||
        nCount != 92)
        return nullptr;

    CPLStringList asMD;
    asMD.SetNameValue(RPC_ERR_BIAS, CPLOPrintf("%.15g", padfRPCTag[0]));
    asMD.SetNameValue(RPC_ERR_RAND, CPLOPrintf("%.15g", padfRPCTag[1]));
    asMD.SetNameValue(RPC_LINE_OFF, CPLOPrintf("%.15g", padfRPCTag[2]));
    asMD.SetNameValue(RPC_SAMP_OFF, CPLOPrintf("%.15g", padfRPCTag[3]));
    asMD.SetNameValue(RPC_LAT_OFF, CPLOPrintf("%.15g", padfRPCTag[4]));
    asMD.SetNameValue(RPC_LONG_OFF, CPLOPrintf("%.15g", padfRPCTag[5]));
    asMD.SetNameValue(RPC_HEIGHT_OFF, CPLOPrintf("%.15g", padfRPCTag[6]));
    asMD.SetNameValue(RPC_LINE_SCALE, CPLOPrintf("%.15g", padfRPCTag[7]));
    asMD.SetNameValue(RPC_SAMP_SCALE, CPLOPrintf("%.15g", padfRPCTag[8]));
    asMD.SetNameValue(RPC_LAT_SCALE, CPLOPrintf("%.15g", padfRPCTag[9]));
    asMD.SetNameValue(RPC_LONG_SCALE, CPLOPrintf("%.15g", padfRPCTag[10]));
    asMD.SetNameValue(RPC_HEIGHT_SCALE, CPLOPrintf("%.15g", padfRPCTag[11]));

    CPLString osField;
    CPLString osMultiField;

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", padfRPCTag[12 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_LINE_NUM_COEFF, osMultiField);

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", padfRPCTag[32 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_LINE_DEN_COEFF, osMultiField);

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", padfRPCTag[52 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_SAMP_NUM_COEFF, osMultiField);

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", padfRPCTag[72 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_SAMP_DEN_COEFF, osMultiField);

    return asMD.StealList();
}

/************************************************************************/
/*                  GTiffFormatGDALNoDataTagValue()                     */
/************************************************************************/

CPLString GTiffFormatGDALNoDataTagValue(double dfNoData)
{
    CPLString osVal;
    if (CPLIsNan(dfNoData))
        osVal = "nan";
    else
        osVal.Printf("%.18g", dfNoData);
    return osVal;
}

/************************************************************************/
/*                       GTIFFUpdatePhotometric()                      */
/************************************************************************/

bool GTIFFUpdatePhotometric(const char *pszPhotometric,
                            const char *pszOptionKey, int nCompression,
                            const char *pszInterleave, int nBands,
                            uint16_t &nPhotometric, uint16_t &nPlanarConfig)
{
    if (pszPhotometric != nullptr && pszPhotometric[0] != '\0')
    {
        if (EQUAL(pszPhotometric, "MINISBLACK"))
            nPhotometric = PHOTOMETRIC_MINISBLACK;
        else if (EQUAL(pszPhotometric, "MINISWHITE"))
            nPhotometric = PHOTOMETRIC_MINISWHITE;
        else if (EQUAL(pszPhotometric, "RGB"))
        {
            nPhotometric = PHOTOMETRIC_RGB;
        }
        else if (EQUAL(pszPhotometric, "CMYK"))
        {
            nPhotometric = PHOTOMETRIC_SEPARATED;
        }
        else if (EQUAL(pszPhotometric, "YCBCR"))
        {
            nPhotometric = PHOTOMETRIC_YCBCR;

            // Because of subsampling, setting YCBCR without JPEG compression
            // leads to a crash currently. Would need to make
            // GTiffRasterBand::IWriteBlock() aware of subsampling so that it
            // doesn't overrun buffer size returned by libtiff.
            if (nCompression != COMPRESSION_JPEG)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Currently, %s=YCBCR requires JPEG compression",
                         pszOptionKey);
                return false;
            }

            if (pszInterleave != nullptr && pszInterleave[0] != '\0' &&
                nPlanarConfig == PLANARCONFIG_SEPARATE)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "%s=YCBCR requires PIXEL interleaving", pszOptionKey);
                return false;
            }
            else
            {
                nPlanarConfig = PLANARCONFIG_CONTIG;
            }

            // YCBCR strictly requires 3 bands. Not less, not more
            // Issue an explicit error message as libtiff one is a bit cryptic:
            // JPEGLib:Bogus input colorspace.
            if (nBands != 3)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "%s=YCBCR requires a source raster "
                         "with only 3 bands (RGB)",
                         pszOptionKey);
                return false;
            }
        }
        else if (EQUAL(pszPhotometric, "CIELAB"))
        {
            nPhotometric = PHOTOMETRIC_CIELAB;
        }
        else if (EQUAL(pszPhotometric, "ICCLAB"))
        {
            nPhotometric = PHOTOMETRIC_ICCLAB;
        }
        else if (EQUAL(pszPhotometric, "ITULAB"))
        {
            nPhotometric = PHOTOMETRIC_ITULAB;
        }
        else
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "%s=%s value not recognised, ignoring.", pszOptionKey,
                     pszPhotometric);
        }
    }
    return true;
}

/************************************************************************/
/*                      GTiffWriteJPEGTables()                          */
/*                                                                      */
/*      Sets the TIFFTAG_JPEGTABLES (and TIFFTAG_REFERENCEBLACKWHITE)   */
/*      tags immediately, instead of relying on the TIFF JPEG codec     */
/*      to write them when it starts compressing imagery. This avoids   */
/*      an IFD rewrite at the end of the file.                          */
/*      Must be used after having set TIFFTAG_SAMPLESPERPIXEL,          */
/*      TIFFTAG_BITSPERSAMPLE.                                          */
/************************************************************************/

void GTiffWriteJPEGTables(TIFF *hTIFF, const char *pszPhotometric,
                          const char *pszJPEGQuality,
                          const char *pszJPEGTablesMode)
{
    // This trick
    // creates a temporary in-memory file and fetches its JPEG tables so that
    // we can directly set them, before tif_jpeg.c compute them at the first
    // strip/tile writing, which is too late, since we have already crystalized
    // the directory. This way we avoid a directory rewriting.
    uint16_t nBands = 0;
    if (!TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nBands))
        nBands = 1;

    uint16_t l_nBitsPerSample = 0;
    if (!TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(l_nBitsPerSample)))
        l_nBitsPerSample = 1;

    CPLString osTmpFilenameIn;
    osTmpFilenameIn.Printf("%s%p", szJPEGGTiffDatasetTmpPrefix, hTIFF);
    VSILFILE *fpTmp = nullptr;
    CPLString osTmp;
    char **papszLocalParameters = nullptr;
    const int nInMemImageWidth = 16;
    const int nInMemImageHeight = 16;
    papszLocalParameters =
        CSLSetNameValue(papszLocalParameters, "COMPRESS", "JPEG");
    papszLocalParameters =
        CSLSetNameValue(papszLocalParameters, "JPEG_QUALITY", pszJPEGQuality);
    if (nBands <= 4)
    {
        papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                               "PHOTOMETRIC", pszPhotometric);
    }
    papszLocalParameters = CSLSetNameValue(papszLocalParameters, "BLOCKYSIZE",
                                           CPLSPrintf("%u", nInMemImageHeight));
    papszLocalParameters = CSLSetNameValue(papszLocalParameters, "NBITS",
                                           CPLSPrintf("%u", l_nBitsPerSample));
    papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                           "JPEGTABLESMODE", pszJPEGTablesMode);

    TIFF *hTIFFTmp =
        GTiffDataset::CreateLL(osTmpFilenameIn, nInMemImageWidth,
                               nInMemImageHeight, (nBands <= 4) ? nBands : 1,
                               (l_nBitsPerSample <= 8) ? GDT_Byte : GDT_UInt16,
                               0.0, papszLocalParameters, &fpTmp, osTmp);
    CSLDestroy(papszLocalParameters);
    if (hTIFFTmp)
    {
        uint16_t l_nPhotometric = 0;
        int nJpegTablesModeIn = 0;
        TIFFGetField(hTIFFTmp, TIFFTAG_PHOTOMETRIC, &(l_nPhotometric));
        TIFFGetField(hTIFFTmp, TIFFTAG_JPEGTABLESMODE, &nJpegTablesModeIn);
        TIFFWriteCheck(hTIFFTmp, FALSE, "CreateLL");
        TIFFWriteDirectory(hTIFFTmp);
        TIFFSetDirectory(hTIFFTmp, 0);
        // Now, reset quality and jpegcolormode.
        const int l_nJpegQuality = pszJPEGQuality ? atoi(pszJPEGQuality) : 0;
        if (l_nJpegQuality > 0)
            TIFFSetField(hTIFFTmp, TIFFTAG_JPEGQUALITY, l_nJpegQuality);
        if (l_nPhotometric == PHOTOMETRIC_YCBCR &&
            CPLTestBool(CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES")))
        {
            TIFFSetField(hTIFFTmp, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }
        if (nJpegTablesModeIn >= 0)
            TIFFSetField(hTIFFTmp, TIFFTAG_JPEGTABLESMODE, nJpegTablesModeIn);

        GPtrDiff_t nBlockSize = static_cast<GPtrDiff_t>(nInMemImageWidth) *
                                nInMemImageHeight *
                                ((nBands <= 4) ? nBands : 1);
        if (l_nBitsPerSample == 12)
            nBlockSize = (nBlockSize * 3) / 2;
        std::vector<GByte> abyZeroData(nBlockSize, 0);
        TIFFWriteEncodedStrip(hTIFFTmp, 0, &abyZeroData[0], nBlockSize);

        uint32_t nJPEGTableSize = 0;
        void *pJPEGTable = nullptr;
        if (TIFFGetField(hTIFFTmp, TIFFTAG_JPEGTABLES, &nJPEGTableSize,
                         &pJPEGTable))
            TIFFSetField(hTIFF, TIFFTAG_JPEGTABLES, nJPEGTableSize, pJPEGTable);

        float *ref = nullptr;
        if (TIFFGetField(hTIFFTmp, TIFFTAG_REFERENCEBLACKWHITE, &ref))
            TIFFSetField(hTIFF, TIFFTAG_REFERENCEBLACKWHITE, ref);

        XTIFFClose(hTIFFTmp);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTmp));
    }
    VSIUnlink(osTmpFilenameIn);
}

/************************************************************************/
/*                       PrepareTIFFErrorFormat()                       */
/*                                                                      */
/*      sometimes the "module" has stuff in it that has special         */
/*      meaning in a printf() style format, so we try to escape it.     */
/*      For now we hope the only thing we have to escape is %'s.        */
/************************************************************************/

static char *PrepareTIFFErrorFormat(const char *module, const char *fmt)

{
    const size_t nModuleSize = strlen(module);
    const size_t nModFmtSize = nModuleSize * 2 + strlen(fmt) + 2;
    char *pszModFmt = static_cast<char *>(CPLMalloc(nModFmtSize));

    size_t iOut = 0;  // Used after for.

    for (size_t iIn = 0; iIn < nModuleSize; ++iIn)
    {
        if (module[iIn] == '%')
        {
            CPLAssert(iOut < nModFmtSize - 2);
            pszModFmt[iOut++] = '%';
            pszModFmt[iOut++] = '%';
        }
        else
        {
            CPLAssert(iOut < nModFmtSize - 1);
            pszModFmt[iOut++] = module[iIn];
        }
    }
    CPLAssert(iOut < nModFmtSize);
    pszModFmt[iOut] = '\0';
    strcat(pszModFmt, ":");
    strcat(pszModFmt, fmt);

    return pszModFmt;
}

#if !defined(SUPPORTS_LIBTIFF_OPEN_OPTIONS)

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
static void GTiffWarningHandler(const char *module, const char *fmt, va_list ap)
{
    if (GTIFFGetThreadLocalLibtiffError() > 0)
    {
        GTIFFGetThreadLocalLibtiffError()++;
        if (GTIFFGetThreadLocalLibtiffError() > 10)
            return;
    }

    if (strstr(fmt, "nknown field") != nullptr)
        return;

    char *pszModFmt = PrepareTIFFErrorFormat(module, fmt);
    if (strstr(fmt, "does not end in null byte") != nullptr)
    {
        CPLString osMsg;
        osMsg.vPrintf(pszModFmt, ap);
        CPLDebug("GTiff", "%s", osMsg.c_str());
    }
    else
    {
        CPLErrorV(CE_Warning, CPLE_AppDefined, pszModFmt, ap);
    }
    CPLFree(pszModFmt);
    return;
}

/************************************************************************/
/*                         GTiffErrorHandler()                          */
/************************************************************************/
static void GTiffErrorHandler(const char *module, const char *fmt, va_list ap)
{
    if (GTIFFGetThreadLocalLibtiffError() > 0)
    {
        GTIFFGetThreadLocalLibtiffError()++;
        if (GTIFFGetThreadLocalLibtiffError() > 10)
            return;
    }

    if (strcmp(fmt, "Maximum TIFF file size exceeded") == 0)
    {
        if (bThreadLocalInExternalOvr)
            fmt = "Maximum TIFF file size exceeded. "
                  "Use --config BIGTIFF_OVERVIEW YES configuration option.";
        else
            fmt = "Maximum TIFF file size exceeded. "
                  "Use BIGTIFF=YES creation option.";
    }

    char *pszModFmt = PrepareTIFFErrorFormat(module, fmt);
    CPLErrorV(CE_Failure, CPLE_AppDefined, pszModFmt, ap);
    CPLFree(pszModFmt);
    return;
}
#else

/************************************************************************/
/*                      GTiffWarningHandlerExt()                        */
/************************************************************************/
extern int GTiffWarningHandlerExt(TIFF *tif, void *user_data,
                                  const char *module, const char *fmt,
                                  va_list ap);

int GTiffWarningHandlerExt(TIFF *tif, void *user_data, const char *module,
                           const char *fmt, va_list ap)
{
    (void)tif;
    (void)user_data;
    auto &nLibtiffErrors = GTIFFGetThreadLocalLibtiffError();
    // cppcheck-suppress knownConditionTrueFalse
    if (nLibtiffErrors > 0)
    {
        nLibtiffErrors++;
        // cppcheck-suppress knownConditionTrueFalse
        if (nLibtiffErrors > 10)
            return 1;
    }

    if (strstr(fmt, "nknown field") != nullptr)
        return 1;

    char *pszModFmt = PrepareTIFFErrorFormat(module, fmt);
    if (strstr(fmt, "does not end in null byte") != nullptr)
    {
        CPLString osMsg;
        osMsg.vPrintf(pszModFmt, ap);
        CPLDebug("GTiff", "%s", osMsg.c_str());
    }
    else
    {
        CPLErrorV(CE_Warning, CPLE_AppDefined, pszModFmt, ap);
    }
    CPLFree(pszModFmt);
    return 1;
}

/************************************************************************/
/*                       GTiffErrorHandlerExt()                         */
/************************************************************************/
extern int GTiffErrorHandlerExt(TIFF *tif, void *user_data, const char *module,
                                const char *fmt, va_list ap);

int GTiffErrorHandlerExt(TIFF *tif, void *user_data, const char *module,
                         const char *fmt, va_list ap)
{
    (void)tif;
    (void)user_data;
    auto &nLibtiffErrors = GTIFFGetThreadLocalLibtiffError();
    // cppcheck-suppress knownConditionTrueFalse
    if (nLibtiffErrors > 0)
    {
        nLibtiffErrors++;
        // cppcheck-suppress knownConditionTrueFalse
        if (nLibtiffErrors > 10)
            return 1;
    }

    if (strcmp(fmt, "Maximum TIFF file size exceeded") == 0)
    {
        if (bThreadLocalInExternalOvr)
            fmt = "Maximum TIFF file size exceeded. "
                  "Use --config BIGTIFF_OVERVIEW YES configuration option.";
        else
            fmt = "Maximum TIFF file size exceeded. "
                  "Use BIGTIFF=YES creation option.";
    }

    char *pszModFmt = PrepareTIFFErrorFormat(module, fmt);
    CPLErrorV(CE_Failure, CPLE_AppDefined, pszModFmt, ap);
    CPLFree(pszModFmt);
    return 1;
}

#endif

/************************************************************************/
/*                          GTiffTagExtender()                          */
/*                                                                      */
/*      Install tags specially known to GDAL.                           */
/************************************************************************/

static TIFFExtendProc _ParentExtender = nullptr;

static void GTiffTagExtender(TIFF *tif)

{
    const TIFFFieldInfo xtiffFieldInfo[] = {
        {TIFFTAG_GDAL_METADATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, TRUE, FALSE,
         const_cast<char *>("GDALMetadata")},
        {TIFFTAG_GDAL_NODATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, TRUE, FALSE,
         const_cast<char *>("GDALNoDataValue")},
        {TIFFTAG_RPCCOEFFICIENT, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM, TRUE, TRUE,
         const_cast<char *>("RPCCoefficient")},
        {TIFFTAG_TIFF_RSID, -1, -1, TIFF_ASCII, FIELD_CUSTOM, TRUE, FALSE,
         const_cast<char *>("TIFF_RSID")},
        {TIFFTAG_GEO_METADATA, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_BYTE,
         FIELD_CUSTOM, TRUE, TRUE, const_cast<char *>("GEO_METADATA")}};

    if (_ParentExtender)
        (*_ParentExtender)(tif);

    TIFFMergeFieldInfo(tif, xtiffFieldInfo,
                       sizeof(xtiffFieldInfo) / sizeof(xtiffFieldInfo[0]));
}

/************************************************************************/
/*                          GTiffOneTimeInit()                          */
/*                                                                      */
/*      This is stuff that is initialized for the TIFF library just     */
/*      once.  We deliberately defer the initialization till the        */
/*      first time we are likely to call into libtiff to avoid          */
/*      unnecessary paging in of the library for GDAL apps that         */
/*      don't use it.                                                   */
/************************************************************************/

static std::mutex oDeleteMutex;
#ifdef HAVE_JXL
static TIFFCodec *pJXLCodec = nullptr;
#endif

void GTiffOneTimeInit()

{
    std::lock_guard<std::mutex> oLock(oDeleteMutex);

    static bool bOneTimeInitDone = false;
    if (bOneTimeInitDone)
        return;

    bOneTimeInitDone = true;

#ifdef HAVE_JXL
    if (pJXLCodec == nullptr)
    {
        pJXLCodec = TIFFRegisterCODEC(COMPRESSION_JXL, "JXL", TIFFInitJXL);
    }
#endif

    _ParentExtender = TIFFSetTagExtender(GTiffTagExtender);

#if !defined(SUPPORTS_LIBTIFF_OPEN_OPTIONS)
    TIFFSetWarningHandler(GTiffWarningHandler);
    TIFFSetErrorHandler(GTiffErrorHandler);
#endif

    LibgeotiffOneTimeInit();
}

/************************************************************************/
/*                        GDALDeregister_GTiff()                        */
/************************************************************************/

static void GDALDeregister_GTiff(GDALDriver *)

{
#ifdef HAVE_JXL
    if (pJXLCodec)
        TIFFUnRegisterCODEC(pJXLCodec);
    pJXLCodec = nullptr;
#endif
}

#define COMPRESSION_ENTRY(x, bWriteSupported)                                  \
    {                                                                          \
        COMPRESSION_##x, STRINGIFY(x), bWriteSupported                         \
    }

static const struct
{
    int nCode;
    const char *pszText;
    bool bWriteSupported;
} asCompressionNames[] = {
    // Compression methods in read/write mode
    COMPRESSION_ENTRY(NONE, true),
    COMPRESSION_ENTRY(CCITTRLE, true),
    COMPRESSION_ENTRY(CCITTFAX3, true),
    {COMPRESSION_CCITTFAX3, "FAX3", true},  // alternate name for write side
    COMPRESSION_ENTRY(CCITTFAX4, true),
    {COMPRESSION_CCITTFAX4, "FAX4", true},  // alternate name for write side
    COMPRESSION_ENTRY(LZW, true),
    COMPRESSION_ENTRY(JPEG, true),
    COMPRESSION_ENTRY(PACKBITS, true),
    {COMPRESSION_ADOBE_DEFLATE, "DEFLATE",
     true},  // manual entry since we want the user friendly name to be DEFLATE
    {COMPRESSION_ADOBE_DEFLATE, "ZIP", true},  // alternate name for write side
    COMPRESSION_ENTRY(LZMA, true),
    COMPRESSION_ENTRY(ZSTD, true),
    COMPRESSION_ENTRY(LERC, true),
    {COMPRESSION_LERC, "LERC_DEFLATE", true},
    {COMPRESSION_LERC, "LERC_ZSTD", true},
    COMPRESSION_ENTRY(WEBP, true),
    COMPRESSION_ENTRY(JXL, true),

    // Compression methods in read-only
    COMPRESSION_ENTRY(OJPEG, false),
    COMPRESSION_ENTRY(NEXT, false),
    COMPRESSION_ENTRY(CCITTRLEW, false),
    COMPRESSION_ENTRY(THUNDERSCAN, false),
    COMPRESSION_ENTRY(PIXARFILM, false),
    COMPRESSION_ENTRY(PIXARLOG, false),
    COMPRESSION_ENTRY(DEFLATE, false),  // COMPRESSION_DEFLATE is deprecated
    COMPRESSION_ENTRY(DCS, false),
    COMPRESSION_ENTRY(JBIG, false),
    COMPRESSION_ENTRY(SGILOG, false),
    COMPRESSION_ENTRY(SGILOG24, false),
    COMPRESSION_ENTRY(JP2000, false),
};

/************************************************************************/
/*                    GTIFFGetCompressionMethodName()                   */
/************************************************************************/

const char *GTIFFGetCompressionMethodName(int nCompressionCode)
{
    for (const auto &entry : asCompressionNames)
    {
        if (entry.nCode == nCompressionCode)
        {
            return entry.pszText;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                   GTIFFGetCompressionMethod()                        */
/************************************************************************/

int GTIFFGetCompressionMethod(const char *pszValue, const char *pszVariableName)
{
    int nCompression = COMPRESSION_NONE;
    bool bFoundMatch = false;
    for (const auto &entry : asCompressionNames)
    {
        if (entry.bWriteSupported && EQUAL(entry.pszText, pszValue))
        {
            bFoundMatch = true;
            nCompression = entry.nCode;
            break;
        }
    }

    if (!bFoundMatch)
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "%s=%s value not recognised, ignoring.", pszVariableName,
                 pszValue);
    }

    if (nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured(static_cast<uint16_t>(nCompression)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create TIFF file due to missing codec for %s.",
                 pszValue);
        return -1;
    }

    return nCompression;
}

/************************************************************************/
/*                     GTiffGetCompressValues()                         */
/************************************************************************/

CPLString GTiffGetCompressValues(bool &bHasLZW, bool &bHasDEFLATE,
                                 bool &bHasLZMA, bool &bHasZSTD, bool &bHasJPEG,
                                 bool &bHasWebP, bool &bHasLERC, bool bForCOG)
{
    bHasLZW = false;
    bHasDEFLATE = false;
    bHasLZMA = false;
    bHasZSTD = false;
    bHasJPEG = false;
    bHasWebP = false;
    bHasLERC = false;

    /* -------------------------------------------------------------------- */
    /*      Determine which compression codecs are available that we        */
    /*      want to advertise.  If we are using an old libtiff we won't     */
    /*      be able to find out so we just assume all are available.        */
    /* -------------------------------------------------------------------- */
    CPLString osCompressValues = "       <Value>NONE</Value>";

    TIFFCodec *codecs = TIFFGetConfiguredCODECs();

    for (TIFFCodec *c = codecs; c->name; ++c)
    {
        if (c->scheme == COMPRESSION_PACKBITS && !bForCOG)
        {
            osCompressValues += "       <Value>PACKBITS</Value>";
        }
        else if (c->scheme == COMPRESSION_JPEG)
        {
            bHasJPEG = true;
            osCompressValues += "       <Value>JPEG</Value>";
        }
        else if (c->scheme == COMPRESSION_LZW)
        {
            bHasLZW = true;
            osCompressValues += "       <Value>LZW</Value>";
        }
        else if (c->scheme == COMPRESSION_ADOBE_DEFLATE)
        {
            bHasDEFLATE = true;
            osCompressValues += "       <Value>DEFLATE</Value>";
        }
        else if (c->scheme == COMPRESSION_CCITTRLE && !bForCOG)
        {
            osCompressValues += "       <Value>CCITTRLE</Value>";
        }
        else if (c->scheme == COMPRESSION_CCITTFAX3 && !bForCOG)
        {
            osCompressValues += "       <Value>CCITTFAX3</Value>";
        }
        else if (c->scheme == COMPRESSION_CCITTFAX4 && !bForCOG)
        {
            osCompressValues += "       <Value>CCITTFAX4</Value>";
        }
        else if (c->scheme == COMPRESSION_LZMA)
        {
            bHasLZMA = true;
            osCompressValues += "       <Value>LZMA</Value>";
        }
        else if (c->scheme == COMPRESSION_ZSTD)
        {
            bHasZSTD = true;
            osCompressValues += "       <Value>ZSTD</Value>";
        }
        else if (c->scheme == COMPRESSION_WEBP)
        {
            bHasWebP = true;
            osCompressValues += "       <Value>WEBP</Value>";
        }
        else if (c->scheme == COMPRESSION_LERC)
        {
            bHasLERC = true;
        }
    }
    if (bHasLERC)
    {
        osCompressValues += "       <Value>LERC</Value>"
                            "       <Value>LERC_DEFLATE</Value>";
        if (bHasZSTD)
        {
            osCompressValues += "       <Value>LERC_ZSTD</Value>";
        }
    }
#ifdef HAVE_JXL
    osCompressValues += "       <Value>JXL</Value>";
#endif
    _TIFFfree(codecs);

    return osCompressValues;
}

/************************************************************************/
/*                    OGRGTiffDriverGetSubdatasetInfo()                 */
/************************************************************************/

struct GTiffDriverSubdatasetInfo : public GDALSubdatasetInfo
{
  public:
    explicit GTiffDriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override
    {
        if (!STARTS_WITH_CI(m_fileName.c_str(), "GTIFF_DIR:"))
        {
            return;
        }

        CPLStringList aosParts{CSLTokenizeString2(m_fileName.c_str(), ":", 0)};
        const int iPartsCount{CSLCount(aosParts)};

        if (iPartsCount == 3 || iPartsCount == 4)
        {

            m_driverPrefixComponent = aosParts[0];

            const bool hasDriveLetter{
                strlen(aosParts[2]) == 1 &&
                std::isalpha(static_cast<unsigned char>(aosParts[2][0]))};

            // Check for drive letter
            if (iPartsCount == 4)
            {
                // Invalid
                if (!hasDriveLetter)
                {
                    return;
                }
                m_pathComponent = aosParts[2];
                m_pathComponent.append(":");
                m_pathComponent.append(aosParts[3]);
            }
            else  // count is 3
            {
                if (hasDriveLetter)
                {
                    return;
                }
                m_pathComponent = aosParts[2];
            }

            m_subdatasetComponent = aosParts[1];
        }
    }
};

static GDALSubdatasetInfo *GTiffDriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH_CI(pszFileName, "GTIFF_DIR:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<GTiffDriverSubdatasetInfo>(pszFileName);
        if (!info->GetSubdatasetComponent().empty() &&
            !info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GTiff()

{
    if (GDALGetDriverByName("GTiff") != nullptr)
        return;

    CPLString osOptions;

    bool bHasLZW = false;
    bool bHasDEFLATE = false;
    bool bHasLZMA = false;
    bool bHasZSTD = false;
    bool bHasJPEG = false;
    bool bHasWebP = false;
    bool bHasLERC = false;
    CPLString osCompressValues(GTiffGetCompressValues(
        bHasLZW, bHasDEFLATE, bHasLZMA, bHasZSTD, bHasJPEG, bHasWebP, bHasLERC,
        false /* bForCOG */));

    GDALDriver *poDriver = new GDALDriver();

    /* -------------------------------------------------------------------- */
    /*      Build full creation option list.                                */
    /* -------------------------------------------------------------------- */
    osOptions = "<CreationOptionList>"
                "   <Option name='COMPRESS' type='string-select'>";
    osOptions += osCompressValues;
    osOptions += "   </Option>";
    if (bHasLZW || bHasDEFLATE || bHasZSTD)
        osOptions += ""
                     "   <Option name='PREDICTOR' type='int' "
                     "description='Predictor Type (1=default, 2=horizontal "
                     "differencing, 3=floating point prediction)'/>";
    osOptions +=
        ""
        "   <Option name='DISCARD_LSB' type='string' description='Number of "
        "least-significant bits to set to clear as a single value or "
        "comma-separated list of values for per-band values'/>";
    if (bHasJPEG)
    {
        osOptions +=
            ""
            "   <Option name='JPEG_QUALITY' type='int' description='JPEG "
            "quality 1-100' default='75'/>"
            "   <Option name='JPEGTABLESMODE' type='int' description='Content "
            "of JPEGTABLES tag. 0=no JPEGTABLES tag, 1=Quantization tables "
            "only, 2=Huffman tables only, 3=Both' default='1'/>";
#ifdef JPEG_DIRECT_COPY
        osOptions +=
            ""
            "   <Option name='JPEG_DIRECT_COPY' type='boolean' description='To "
            "copy without any decompression/recompression a JPEG source file' "
            "default='NO'/>";
#endif
    }
    if (bHasDEFLATE)
    {
#ifdef LIBDEFLATE_SUPPORT
        osOptions += ""
                     "   <Option name='ZLEVEL' type='int' description='DEFLATE "
                     "compression level 1-12' default='6'/>";
#else
        osOptions += ""
                     "   <Option name='ZLEVEL' type='int' description='DEFLATE "
                     "compression level 1-9' default='6'/>";
#endif
    }
    if (bHasLZMA)
        osOptions +=
            ""
            "   <Option name='LZMA_PRESET' type='int' description='LZMA "
            "compression level 0(fast)-9(slow)' default='6'/>";
    if (bHasZSTD)
        osOptions +=
            ""
            "   <Option name='ZSTD_LEVEL' type='int' description='ZSTD "
            "compression level 1(fast)-22(slow)' default='9'/>";
    if (bHasLERC)
    {
        osOptions +=
            ""
            "   <Option name='MAX_Z_ERROR' type='float' description='Maximum "
            "error for LERC compression' default='0'/>"
            "   <Option name='MAX_Z_ERROR_OVERVIEW' type='float' "
            "description='Maximum error for LERC compression in overviews' "
            "default='0'/>";
    }
    if (bHasWebP)
    {
#ifndef DEFAULT_WEBP_LEVEL
#error "DEFAULT_WEBP_LEVEL should be defined"
#endif
        osOptions +=
            ""
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
            "   <Option name='WEBP_LOSSLESS' type='boolean' "
            "description='Whether lossless compression should be used' "
            "default='FALSE'/>"
#endif
            "   <Option name='WEBP_LEVEL' type='int' description='WEBP quality "
            "level. Low values result in higher compression ratios' "
            "default='" XSTRINGIFY(DEFAULT_WEBP_LEVEL) "'/>";
    }
#ifdef HAVE_JXL
    osOptions +=
        ""
        "   <Option name='JXL_LOSSLESS' type='boolean' description='Whether "
        "JPEGXL compression should be lossless' default='YES'/>"
        "   <Option name='JXL_EFFORT' type='int' description='Level of effort "
        "1(fast)-9(slow)' default='5'/>"
        "   <Option name='JXL_DISTANCE' type='float' description='Distance "
        "level for lossy compression (0=mathematically lossless, 1.0=visually "
        "lossless, usual range [0.5,3])' default='1.0' min='0.1' max='15.0'/>";
#ifdef HAVE_JxlEncoderSetExtraChannelDistance
    osOptions += "   <Option name='JXL_ALPHA_DISTANCE' type='float' "
                 "description='Distance level for alpha channel "
                 "(-1=same as non-alpha channels, "
                 "0=mathematically lossless, 1.0=visually lossless, "
                 "usual range [0.5,3])' default='-1' min='-1' max='15.0'/>";
#endif
#endif
    osOptions +=
        ""
        "   <Option name='NUM_THREADS' type='string' description='Number of "
        "worker threads for compression. Can be set to ALL_CPUS' default='1'/>"
        "   <Option name='NBITS' type='int' description='BITS for sub-byte "
        "files (1-7), sub-uint16_t (9-15), sub-uint32_t (17-31), or float32 "
        "(16)'/>"
        "   <Option name='INTERLEAVE' type='string-select' default='PIXEL'>"
        "       <Value>BAND</Value>"
        "       <Value>PIXEL</Value>"
        "   </Option>"
        "   <Option name='TILED' type='boolean' description='Switch to tiled "
        "format'/>"
        "   <Option name='TFW' type='boolean' description='Write out world "
        "file'/>"
        "   <Option name='RPB' type='boolean' description='Write out .RPB "
        "(RPC) file'/>"
        "   <Option name='RPCTXT' type='boolean' description='Write out "
        "_RPC.TXT file'/>"
        "   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
        "   <Option name='BLOCKYSIZE' type='int' description='Tile/Strip "
        "Height'/>"
        "   <Option name='PHOTOMETRIC' type='string-select'>"
        "       <Value>MINISBLACK</Value>"
        "       <Value>MINISWHITE</Value>"
        "       <Value>PALETTE</Value>"
        "       <Value>RGB</Value>"
        "       <Value>CMYK</Value>"
        "       <Value>YCBCR</Value>"
        "       <Value>CIELAB</Value>"
        "       <Value>ICCLAB</Value>"
        "       <Value>ITULAB</Value>"
        "   </Option>"
        "   <Option name='SPARSE_OK' type='boolean' description='Should empty "
        "blocks be omitted on disk?' default='FALSE'/>"
        "   <Option name='ALPHA' type='string-select' description='Mark first "
        "extrasample as being alpha'>"
        "       <Value>NON-PREMULTIPLIED</Value>"
        "       <Value>PREMULTIPLIED</Value>"
        "       <Value>UNSPECIFIED</Value>"
        "       <Value aliasOf='NON-PREMULTIPLIED'>YES</Value>"
        "       <Value aliasOf='UNSPECIFIED'>NO</Value>"
        "   </Option>"
        "   <Option name='PROFILE' type='string-select' default='GDALGeoTIFF'>"
        "       <Value>GDALGeoTIFF</Value>"
        "       <Value>GeoTIFF</Value>"
        "       <Value>BASELINE</Value>"
        "   </Option>"
        "   <Option name='PIXELTYPE' type='string-select' "
        "description='(deprecated, use Int8 datatype)'>"
        "       <Value>DEFAULT</Value>"
        "       <Value>SIGNEDBYTE</Value>"
        "   </Option>"
        "   <Option name='BIGTIFF' type='string-select' description='Force "
        "creation of BigTIFF file'>"
        "     <Value>YES</Value>"
        "     <Value>NO</Value>"
        "     <Value>IF_NEEDED</Value>"
        "     <Value>IF_SAFER</Value>"
        "   </Option>"
        "   <Option name='ENDIANNESS' type='string-select' default='NATIVE' "
        "description='Force endianness of created file. For DEBUG purpose "
        "mostly'>"
        "       <Value>NATIVE</Value>"
        "       <Value>INVERTED</Value>"
        "       <Value>LITTLE</Value>"
        "       <Value>BIG</Value>"
        "   </Option>"
        "   <Option name='COPY_SRC_OVERVIEWS' type='boolean' default='NO' "
        "description='Force copy of overviews of source dataset "
        "(CreateCopy())'/>"
        "   <Option name='SOURCE_ICC_PROFILE' type='string' description='ICC "
        "profile'/>"
        "   <Option name='SOURCE_PRIMARIES_RED' type='string' "
        "description='x,y,1.0 (xyY) red chromaticity'/>"
        "   <Option name='SOURCE_PRIMARIES_GREEN' type='string' "
        "description='x,y,1.0 (xyY) green chromaticity'/>"
        "   <Option name='SOURCE_PRIMARIES_BLUE' type='string' "
        "description='x,y,1.0 (xyY) blue chromaticity'/>"
        "   <Option name='SOURCE_WHITEPOINT' type='string' "
        "description='x,y,1.0 (xyY) whitepoint'/>"
        "   <Option name='TIFFTAG_TRANSFERFUNCTION_RED' type='string' "
        "description='Transfer function for red'/>"
        "   <Option name='TIFFTAG_TRANSFERFUNCTION_GREEN' type='string' "
        "description='Transfer function for green'/>"
        "   <Option name='TIFFTAG_TRANSFERFUNCTION_BLUE' type='string' "
        "description='Transfer function for blue'/>"
        "   <Option name='TIFFTAG_TRANSFERRANGE_BLACK' type='string' "
        "description='Transfer range for black'/>"
        "   <Option name='TIFFTAG_TRANSFERRANGE_WHITE' type='string' "
        "description='Transfer range for white'/>"
        "   <Option name='STREAMABLE_OUTPUT' type='boolean' default='NO' "
        "description='Enforce a mode compatible with a streamable file'/>"
        "   <Option name='GEOTIFF_KEYS_FLAVOR' type='string-select' "
        "default='STANDARD' description='Which flavor of GeoTIFF keys must be "
        "used'>"
        "       <Value>STANDARD</Value>"
        "       <Value>ESRI_PE</Value>"
        "   </Option>"
#if LIBGEOTIFF_VERSION >= 1600
        "   <Option name='GEOTIFF_VERSION' type='string-select' default='AUTO' "
        "description='Which version of GeoTIFF must be used'>"
        "       <Value>AUTO</Value>"
        "       <Value>1.0</Value>"
        "       <Value>1.1</Value>"
        "   </Option>"
#endif
        "</CreationOptionList>";

    /* -------------------------------------------------------------------- */
    /*      Set the driver details.                                         */
    /* -------------------------------------------------------------------- */
    poDriver->SetDescription("GTiff");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GeoTIFF");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/gtiff.html");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/tiff");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "tif");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "tif tiff");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int8 UInt16 Int16 UInt32 Int32 Float32 "
                              "Float64 CInt16 CInt32 CFloat32 CFloat64");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST, osOptions);
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='NUM_THREADS' type='string' description='Number of "
        "worker threads for compression. Can be set to ALL_CPUS' default='1'/>"
        "   <Option name='GEOTIFF_KEYS_FLAVOR' type='string-select' "
        "default='STANDARD' description='Which flavor of GeoTIFF keys must be "
        "used (for writing)'>"
        "       <Value>STANDARD</Value>"
        "       <Value>ESRI_PE</Value>"
        "   </Option>"
        "   <Option name='GEOREF_SOURCES' type='string' description='Comma "
        "separated list made with values "
        "INTERNAL/TABFILE/WORLDFILE/PAM/XML/NONE "
        "that describe the priority order for georeferencing' "
        "default='PAM,INTERNAL,TABFILE,WORLDFILE,XML'/>"
        "   <Option name='SPARSE_OK' type='boolean' description='Should empty "
        "blocks be omitted on disk?' default='FALSE'/>"
        "   <Option name='IGNORE_COG_LAYOUT_BREAK' type='boolean' "
        "description='Allow update mode on files with COG structure' "
        "default='FALSE'/>"
        "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

#ifdef INTERNAL_LIBTIFF
    poDriver->SetMetadataItem("LIBTIFF", "INTERNAL");
#else
    poDriver->SetMetadataItem("LIBTIFF", TIFFLIB_VERSION_STR);
#endif

    poDriver->SetMetadataItem("LIBGEOTIFF", XSTRINGIFY(LIBGEOTIFF_VERSION));

#if defined(LERC_SUPPORT) && defined(LERC_VERSION_MAJOR)
    poDriver->SetMetadataItem("LERC_VERSION_MAJOR",
                              XSTRINGIFY(LERC_VERSION_MAJOR), "LERC");
    poDriver->SetMetadataItem("LERC_VERSION_MINOR",
                              XSTRINGIFY(LERC_VERSION_MINOR), "LERC");
    poDriver->SetMetadataItem("LERC_VERSION_PATCH",
                              XSTRINGIFY(LERC_VERSION_PATCH), "LERC");
#endif

    poDriver->SetMetadataItem(GDAL_DCAP_COORDINATE_EPOCH, "YES");

    poDriver->pfnOpen = GTiffDataset::Open;
    poDriver->pfnCreate = GTiffDataset::Create;
    poDriver->pfnCreateCopy = GTiffDataset::CreateCopy;
    poDriver->pfnUnloadDriver = GDALDeregister_GTiff;
    poDriver->pfnIdentify = GTiffDataset::Identify;
    poDriver->pfnGetSubdatasetInfoFunc = GTiffDriverGetSubdatasetInfo;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
