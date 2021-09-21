/******************************************************************************
 * Project:  GDAL
 * Author:   Raul Alonso Reyes <raul dot alonsoreyes at satcen dot europa dot eu>
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Purpose:  JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2014-2016, Even Rouault
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

#include "cpl_port.h"

#include "lwf_jp2.h"
#include "jp2luradataset.h"
#include "jp2luracallbacks.h"
#include "jp2lurarasterband.h"
#include "gdaljp2abstractdataset.h"
#include "cpl_string.h"
#include "gdaljp2metadata.h"
#include "vrt/vrtdataset.h"

extern "C" CPL_DLL void GDALRegister_JP2Lura();

static vsi_l_offset JP2LuraFindCodeStream(VSILFILE* fp,
                                              vsi_l_offset* pnLength);

/************************************************************************/
/*                         FloorPowerOfTwo()                            */
/************************************************************************/

static int FloorPowerOfTwo(int nVal)
{
    int nBits = 0;
    while( nVal > 1 )
    {
        nBits ++;
        nVal >>= 1;
    }
    return 1 << nBits;
}

/************************************************************************/
/*                         GetErrorMessage()                            */
/************************************************************************/

#define ERROR_TUPLE(x) { x, #x }

static const struct
{
    int nErrorCode;
    const char* szErrorText;
}
asErrorText[] =
{
    ERROR_TUPLE(cJP2_Error_Failure_Malloc),
    ERROR_TUPLE(cJP2_Error_Failure_Read),
    ERROR_TUPLE(cJP2_Error_Failure_Write),
    ERROR_TUPLE(cJP2_Error_Invalid_Handle),
    ERROR_TUPLE(cJP2_Error_Invalid_Pointer),
    ERROR_TUPLE(cJP2_Error_Invalid_Number_Of_Components),
    ERROR_TUPLE(cJP2_Error_Invalid_Component_Index),
    ERROR_TUPLE(cJP2_Error_Invalid_Property_Value),
    ERROR_TUPLE(cJP2_Error_Invalid_Property_Key),
    ERROR_TUPLE(cJP2_Error_Invalid_Width),
    ERROR_TUPLE(cJP2_Error_Invalid_Height),
    ERROR_TUPLE(cJP2_Error_Invalid_Bits_Per_Sample),
    ERROR_TUPLE(cJP2_Error_Invalid_Tile_Arrangement),
    ERROR_TUPLE(cJP2_Error_Invalid_Colorspace),
    ERROR_TUPLE(cJP2_Error_Invalid_Component_Dimensions),
    ERROR_TUPLE(cJP2_Error_Invalid_Tile_Index),
    ERROR_TUPLE(cJP2_Error_Invalid_Resolution_Level),
    ERROR_TUPLE(cJP2_Error_Invalid_Wavelet_Filter_Combination),
    ERROR_TUPLE(cJP2_Error_Invalid_Stream),
    ERROR_TUPLE(cJP2_Error_Single_Value_For_All_Components),
    ERROR_TUPLE(cJP2_Error_Single_Value_For_All_Tiles),
    ERROR_TUPLE(cJP2_Error_Read_Only_Property),
    ERROR_TUPLE(cJP2_Error_Set_Only_Property),
    ERROR_TUPLE(cJP2_Error_Quality_Compression_Mode),
    ERROR_TUPLE(cJP2_Error_Decompression_Only_Property),
    ERROR_TUPLE(cJP2_Error_Compression_Only_Property),
    ERROR_TUPLE(cJP2_Error_Bits_Per_Sample_Too_High),
    ERROR_TUPLE(cJP2_Error_Input_Callback_Undefined),
    ERROR_TUPLE(cJP2_Error_Write_Callback_Undefined),
    ERROR_TUPLE(cJP2_Error_Read_Callback_Undefined),
    ERROR_TUPLE(cJP2_Error_Cannot_Find_Suitable_Grid),
    ERROR_TUPLE(cJP2_Error_Byte_Compression_Mode),
    ERROR_TUPLE(cJP2_Error_Requested_File_Size_Too_Small),
    ERROR_TUPLE(cJP2_Error_Invalid_Sample_Rate),
    ERROR_TUPLE(cJP2_Error_Not_Yet_Supported),
    ERROR_TUPLE(cJP2_Error_Trial_Time_Expired),
    ERROR_TUPLE(cJP2_Error_Invalid_Quantization_Filter_Pair),
    ERROR_TUPLE(cJP2_Error_Invalid_Precinct_Dimensions),
    ERROR_TUPLE(cJP2_Error_ROI_Shift_Failed),
    ERROR_TUPLE(cJP2_Error_Scale_Factor_Is_Too_Large),
    ERROR_TUPLE(cJP2_Error_Invalid_Resolution),
    ERROR_TUPLE(cJP2_Error_Invalid_Resolution_Unit),
    ERROR_TUPLE(cJP2_Error_Invalid_Resolution_Type),
    ERROR_TUPLE(cJP2_Error_Max_Number_Of_ROIs_Reached),
    ERROR_TUPLE(cJP2_Error_More_Bytes_Required),
    ERROR_TUPLE(cJP2_Error_Decompression_Cancelled),
    ERROR_TUPLE(cJP2_Error_File_Format_Required),
    ERROR_TUPLE(cJP2_Error_JPX_File_Format_Required),
    ERROR_TUPLE(cJP2_Error_Invalid_Meta_Data_Box_Index),
    ERROR_TUPLE(cJP2_Error_Invalid_Color_Spec_Index),
    ERROR_TUPLE(cJP2_Error_Invalid_ICC_Profile),
    ERROR_TUPLE(cJP2_Error_Use_SetICC_Function),
    ERROR_TUPLE(cJP2_Error_Use_SetLAB_Function),
    ERROR_TUPLE(cJP2_Error_Missing_Palette),
    ERROR_TUPLE(cJP2_Error_Invalid_Palette),
    ERROR_TUPLE(cJP2_Error_Missing_Component_Mapping),
    ERROR_TUPLE(cJP2_Error_Invalid_Component_Mapping),
    ERROR_TUPLE(cJP2_Error_Invalid_Channel_Definition),
    ERROR_TUPLE(cJP2_Error_Use_SetPalette),
    ERROR_TUPLE(cJP2_Error_Transcoding_Finished),
    ERROR_TUPLE(cJP2_Error_Transcode_Scale_Palette_Images),
    ERROR_TUPLE(cJP2_Error_Invalid_Region),
    ERROR_TUPLE(cJP2_Error_Lossless_Compression_Mode),
    ERROR_TUPLE(cJP2_Error_Maximum_Box_Size_Exceeded),
    ERROR_TUPLE(cJP2_Error_Invalid_Label),
    ERROR_TUPLE(cJP2_Error_Invalid_Header),
    ERROR_TUPLE(cJP2_Error_Incompatible_Format),
    ERROR_TUPLE(cJP2_Error_Invalid_Marker),
    ERROR_TUPLE(cJP2_Error_Corrupt_Packet),
    ERROR_TUPLE(cJP2_Error_Invalid_Marker_Segment),

    ERROR_TUPLE(cJP2_Error_Invalid_License),
    ERROR_TUPLE(cJP2_Error_License_Level_Too_Low),

    ERROR_TUPLE(cJP2_Error_Fatal),

    ERROR_TUPLE(cJP2_Warning_Unable_To_Read_All_Data),
};

const char* JP2LuraDataset::GetErrorMessage( long nErrorCode )
{
    for( size_t i = 0; i < CPL_ARRAYSIZE(asErrorText); ++i )
    {
        if( asErrorText[i].nErrorCode == nErrorCode )
            return asErrorText[i].szErrorText;
    }
    return CPLSPrintf("unknown error %ld", nErrorCode);
}

/************************************************************************/
/* ==================================================================== */
/*                           JP2LuraDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        JP2LuraDataset()                              */
/************************************************************************/

JP2LuraDataset::JP2LuraDataset()
{
    fp = nullptr;
    iLevel = 0;
    nOverviewCount = 0;
    papoOverviewDS = nullptr;
    memset(&sOutputData, 0, sizeof(sOutputData));
    poCT = nullptr;
    eColorspace = cJP2_Colorspace_Gray;
    nRedIndex = -1;
    nGreenIndex = -1;
    nBlueIndex = -1;
    nAlphaIndex = -1;
}

/************************************************************************/
/*                         ~JP2LuraDataset()                            */
/************************************************************************/

JP2LuraDataset::~JP2LuraDataset()

{
    if (papoOverviewDS)
    {
            for (int i = 0; i < nOverviewCount; i++)
                    delete papoOverviewDS[i];
            CPLFree(papoOverviewDS);
            papoOverviewDS = nullptr;
    }

    if( sOutputData.pDatacache )
    {
        for( int i = 0; i < nBands; ++i )
            VSIFree(sOutputData.pDatacache[i]);
        CPLFree(sOutputData.pDatacache);
    }

    if (iLevel == 0)
    {
        if(sOutputData.handle)
        {
                JP2_Decompress_End(sOutputData.handle);
                sOutputData.handle = nullptr;
        }

        if (fp)
        {
            VSIFCloseL(fp);
            fp = nullptr;
        }

        delete poCT;
    }
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

constexpr unsigned char jpc_header[] = {0xff,0x4f,0xff,0x51}; // SOC + RSIZ markers
constexpr unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */

int JP2LuraDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes >= 16
        && (memcmp( poOpenInfo->pabyHeader, jpc_header,
                    sizeof(jpc_header) ) == 0
            || memcmp( poOpenInfo->pabyHeader + 4, jp2_box_jp,
                    sizeof(jp2_box_jp) ) == 0
           ) )
        return TRUE;

    else
        return FALSE;
}


/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/

GDALDataset * JP2LuraDataset::CreateCopy(const char * pszFilename,
                                         GDALDataset *poSrcDS,
                                         int /* bStrict */,
                                         char ** papszOptions,
                                         GDALProgressFunc pfnProgress,
                                         void * pProgressData)

{
    char                        pcMsg[255];
    JP2_Comp_Handle             handle = nullptr;
    GDALJP2Metadata             oJP2MD;

    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    if (nBands == 0 || nBands > 32767)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Unable to export files with %d bands. "
                "Must be >= 1 and <= 32767", nBands);
        return nullptr;
    }
    GDALColorTable* poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
    if (poCT != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "JP2Lura driver does not support band with color table");
        return nullptr;
    }

    const GDALDataType eDataType =
                            poSrcDS->GetRasterBand(1)->GetRasterDataType();
    if (eDataType != GDT_Byte && eDataType != GDT_Int16 &&
        eDataType != GDT_UInt16 && eDataType != GDT_UInt32 &&
        eDataType != GDT_Int32 && eDataType != GDT_Float32)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "JP2Lura driver only supports creating Byte, Int16, "
                "UInt16, Int32, UInt32 or Float32");
        return nullptr;
    }
    if( eDataType == GDT_Float32 && nBands != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "JP2Lura driver only supports creating one single Float32 band");
        return nullptr;
    }
    if( eDataType == GDT_Float32 &&
        !CPLFetchBool(papszOptions, "SPLIT_IEEE754", false) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "Encoding of GDT_Float32 band is only supported if "
            "SPLIT_IEEE754=YES is specified");
        return nullptr;
    }

    unsigned long ulBps = 0;
    bool bSigned = false;

    switch (eDataType)
    {
        case GDT_Byte:
        {
            ulBps = 8;
            bSigned = false;
            break;
        }
        case GDT_UInt16:
        {
            ulBps = 16;
            bSigned = false;
            break;
        }
        case GDT_Int16:
        {
            ulBps = 16;
            bSigned = true;
            break;
        }
        case GDT_UInt32:
        {
            ulBps = 28;
            bSigned = false;
            break;
        }
        case GDT_Int32:
        {
            ulBps = 28;
            bSigned = true;
            break;
        }

        default:
                break;
    }

    /* -------------------------------------------------------------------- */
    /*      Read creation options.                                          */
    /* -------------------------------------------------------------------- */
    bool bGeoJP2Option = CPLFetchBool(papszOptions, "GeoJP2", false);
    bool bGMLJP2Option = CPLFetchBool(papszOptions, "GMLJP2", true);
    const char* pszGMLJP2V2Def = CSLFetchNameValue(papszOptions,
                                                   "GMLJP2V2_DEF");


    const char* pszCodec = CSLFetchNameValue(papszOptions, "CODEC");
    const char* QUALITY_STYLE = CSLFetchNameValueDef(papszOptions,
                                                     "QUALITY_STYLE", "PSNR");
    const char* SPEED_MODE = CSLFetchNameValueDef(papszOptions,
                                                  "SPEED_MODE", "Fast");
    int RATE = atoi(CSLFetchNameValueDef(papszOptions, "RATE", "0"));
    int QUALITY = atoi(CSLFetchNameValueDef(papszOptions, "QUALITY", "0"));
    int PRECISION = atoi(CSLFetchNameValueDef(papszOptions, "PRECISION", "0"));
    const char* PROGRESSION = CSLFetchNameValueDef(papszOptions,
                                                   "PROGRESSION", "LRCP");
    bool REVERSIBLE = CPLFetchBool(papszOptions, "REVERSIBLE", false);
    int LEVELS = atoi(CSLFetchNameValueDef(papszOptions, "LEVELS", "5"));
    const char* QUANTIZATION_STYLE = CSLFetchNameValueDef(papszOptions,
                                            "QUANTIZATION_STYLE", "EXPOUNDED");
    int TILEXSIZE = atoi(CSLFetchNameValueDef(papszOptions, "TILEXSIZE", "0"));
    int TILEYSIZE = atoi(CSLFetchNameValueDef(papszOptions, "TILEYSIZE", "0"));
    bool TLM = CPLFetchBool(papszOptions, "TLM", false);
    int CODEBLOCK_WIDTH = atoi(CSLFetchNameValueDef(papszOptions,
                                               "CODEBLOCK_WIDTH", "64"));
    int CODEBLOCK_HEIGHT = atoi(CSLFetchNameValueDef(papszOptions,
                                                "CODEBLOCK_HEIGHT", "64"));
    bool ERROR_RESILIENCE = CPLFetchBool(papszOptions,
                                           "ERROR_RESILIENCE", false);
    bool WRITE_METADATA = CPLFetchBool(papszOptions, "WRITE_METADATA", false);
    bool MAIN_MD_DOMAIN_ONLY = CPLFetchBool(papszOptions,
                                                "MAIN_MD_DOMAIN_ONLY", false);
    const bool USE_SRC_CODESTREAM = CPLFetchBool(papszOptions,
                                             "USE_SRC_CODESTREAM", false);

    int NBITS = atoi(CSLFetchNameValueDef(papszOptions, "NBITS", "0"));
    if( NBITS )
    {
        if( eDataType == GDT_Byte && NBITS <= 8 )
            ulBps = NBITS;
        else if( (eDataType == GDT_Int16 || eDataType == GDT_UInt16) &&
                 NBITS > 8 && NBITS <= 16 )
            ulBps = NBITS;
        else if( (eDataType == GDT_Int16 || eDataType == GDT_UInt16) &&
                 NBITS > 16 && NBITS <= 28 )
            ulBps = NBITS;
        else
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Inconsistent value of NBITS for data type");
    }
    else if( poSrcDS->GetRasterBand(1)->GetMetadataItem( "NBITS",
                                                "IMAGE_STRUCTURE" ) != nullptr )
    {
        ulBps = atoi(poSrcDS->GetRasterBand(1)->GetMetadataItem( "NBITS",
                                                        "IMAGE_STRUCTURE" ) );
    }

    /* -------------------------------------------------------------------- */
    /*      Analyze creation options.                                       */
    /* -------------------------------------------------------------------- */

    /* -------------------------------------------------------------------- */
    /*      Deal with codestream PROFILE                                    */
    /* -------------------------------------------------------------------- */
    const char* pszProfile = CSLFetchNameValueDef(papszOptions, "PROFILE",
                                                  "AUTO");
    bool bProfile1 = false;
    if (EQUAL(pszProfile, "UNRESTRICTED"))
    {
        bProfile1 = false;
        /*if (bInspireTG)
        {
        CPLError(CE_Failure, CPLE_NotSupported,
        "INSPIRE_TG=YES mandates PROFILE=PROFILE_1 (TG requirement 21)");
        return NULL;
        }*/
    }
    else if (EQUAL(pszProfile, "UNRESTRICTED_FORCED"))
    {
        bProfile1 = false;
    }
    else if (EQUAL(pszProfile, "PROFILE_1_FORCED"))
            /* For debug only: can produce inconsistent codestream */
    {
        bProfile1 = true;
    }
    else
    {
        if (!(EQUAL(pszProfile, "PROFILE_1") || EQUAL(pszProfile, "AUTO")))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Unsupported value for PROFILE : %s. Defaulting to AUTO",
                    pszProfile);
            //pszProfile = "AUTO";
        }

        bProfile1 = true;
        //const char* pszReq21OrEmpty = (bInspireTG) ?
        //                                      " (TG requirement 21)" : "";
        if (TILEXSIZE != 0 && TILEYSIZE != 0 &&
            (TILEXSIZE != nXSize || TILEYSIZE != nYSize) &&
            (TILEXSIZE != TILEYSIZE || TILEXSIZE > 1024 ||
            TILEYSIZE > 1024))
        {
            bProfile1 = false;
            /*if (bInspireTG || EQUAL(pszProfile, "PROFILE_1"))
            {
            CPLError(CE_Failure, CPLE_NotSupported,
            "Tile dimensions incompatible with PROFILE_1%s. "
            "Should be whole image or square with dimension <= 1024.",
            pszReq21OrEmpty);
            return NULL;
            }*/
        }

        if (CODEBLOCK_WIDTH > 64 || CODEBLOCK_HEIGHT > 64)
        {
            bProfile1 = false;
            /*if (bInspireTG || EQUAL(pszProfile, "PROFILE_1"))
            {
            CPLError(CE_Failure, CPLE_NotSupported,
            "Codeblock width incompatible with PROFILE_1%s. "
            "Codeblock width or height should be <= 64.",
            pszReq21OrEmpty);
            return NULL;
            }*/
        }
    }

    bool bIsJP2OrJPX = true;

    if (pszCodec)
    {
        if (EQUAL(pszCodec, "Codestream") || EQUAL(pszCodec, "J2K"))
            bIsJP2OrJPX = false;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Unsupported value for CODEC : %s. Defaulting to JP2",
                    pszCodec);
        }
    }
    else
    {
        // Codestream                   Part 1      .jpc .j2k
        // JP2 File Format              Part 1      .jp2
        // JPX File Format              Part 2      .jpf
        if (strlen(pszFilename) > 4)
        {
            if (EQUAL(pszFilename + strlen(pszFilename) - 4, ".JPC") ||
                EQUAL(pszFilename + strlen(pszFilename) - 4, ".J2K"))
            {
                bIsJP2OrJPX = false;
            }
        }
    }

    JP2_Property_Value cJP2_Quality = cJP2_Quality_PSNR;

    if (QUALITY_STYLE)
    {
        if (EQUAL(QUALITY_STYLE, "PSNR"))
            cJP2_Quality = cJP2_Quality_PSNR;
        else if (EQUAL(QUALITY_STYLE, "XXSmall"))
            cJP2_Quality = cJP2_Quality_Visual_XXSmall;
        else if (EQUAL(QUALITY_STYLE, "XSmall"))
            cJP2_Quality = cJP2_Quality_Visual_XSmall;
        else if (EQUAL(QUALITY_STYLE, "Small"))
            cJP2_Quality = cJP2_Quality_Visual_Small;
        else if (EQUAL(QUALITY_STYLE, "Medium"))
            cJP2_Quality = cJP2_Quality_Visual_Medium;
        else if (EQUAL(QUALITY_STYLE, "Large"))
            cJP2_Quality = cJP2_Quality_Visual_Large;
        else if (EQUAL(QUALITY_STYLE, "XLarge"))
            cJP2_Quality = cJP2_Quality_Visual_XLarge;
        else if (EQUAL(QUALITY_STYLE, "XXLarge"))
            cJP2_Quality = cJP2_Quality_Visual_XXLarge;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Unsupported value for QUALITY_STYLE : %s. "
                    "Defaulting to PSNR",
                    QUALITY_STYLE);
        }
    }

    JP2_Property_Value cJP2_Speed = cJP2_Speed_Fast;
    if (SPEED_MODE)
    {
        if (EQUAL(SPEED_MODE, "Fast"))
            cJP2_Speed = cJP2_Speed_Fast;
        else if (EQUAL(SPEED_MODE, "Accurate"))
            cJP2_Speed = cJP2_Speed_Accurate;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Unsupported value for SPEED_MODE : %s. Defaulting to Fast",
                    SPEED_MODE);
        }
    }
    if (RATE < 0)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for RATE : %d. "
                "Defaulting to 0 (maximum quality).",
                RATE);
        RATE = 0;
    }
    if (QUALITY<0 || QUALITY>100)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for QUALITY : %d. "
                "Defaulting to 0 (maximum quality).",
                QUALITY);
        QUALITY = 0;
    }
    if (PRECISION != 32 && PRECISION != 16 && PRECISION != 0)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for PRECISION : %d. "
                "Defaulting to 0 (automatically select appropriate precision).",
                PRECISION);
        PRECISION = 0;
    }
    else if (PRECISION == 32)
        PRECISION = 1;
    else if (PRECISION == 16)
        PRECISION = 0;

    JP2_Property_Value cJP2_Progression = cJP2_Progression_LRCP;
    if (PROGRESSION)
    {
        if (EQUAL(PROGRESSION, "LRCP"))
            cJP2_Progression = cJP2_Progression_LRCP;
        else if (EQUAL(PROGRESSION, "RLCP"))
            cJP2_Progression = cJP2_Progression_RLCP;
        else if (EQUAL(PROGRESSION, "RPCL"))
            cJP2_Progression = cJP2_Progression_RPCL;
        else if (EQUAL(PROGRESSION, "PCRL"))
            cJP2_Progression = cJP2_Progression_PCRL;
        else if (EQUAL(PROGRESSION, "CPRL"))
            cJP2_Progression = cJP2_Progression_CPRL;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Unsupported value for PROGRESSION : %s. "
                    "Defaulting to LRCP (quality)",
                    PROGRESSION);
        }
    }
    if (LEVELS<0 || LEVELS>16)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for LEVELS : %d. Defaulting to 5.",
                LEVELS);
        LEVELS = 5;
    }
    JP2_Property_Value cJP2_Quant = cJP2_Quant_Expounded;
    if (QUANTIZATION_STYLE)
    {
        if (EQUAL(QUANTIZATION_STYLE, "DERIVED"))
            cJP2_Quant = cJP2_Quant_Derived;
        else if (EQUAL(QUANTIZATION_STYLE, "EXPOUNDED"))
            cJP2_Quant = cJP2_Quant_Expounded;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Unsupported value for QUANTIZATION_STYLE : %s. "
                    "Defaulting to EXPOUNDED (quality)",
                    QUANTIZATION_STYLE);
        }
    }
    JP2_Property_Value cJP2_Wavelet =
        (REVERSIBLE) ? cJP2_Wavelet_5_3 : cJP2_Wavelet_9_7;
    if (TILEXSIZE<0 || TILEXSIZE>nXSize)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for TILEXSIZE : %d. image_width is %d. "
                "Defaulting to 0 (Image_Width).",
                TILEXSIZE, nXSize);
        TILEXSIZE = 0;
    }
    if (TILEYSIZE<0 || TILEYSIZE>nYSize)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for TILEYSIZE : %d. Image_Height is %d. "
                "Defaulting to 0 (Image_Height).",
                TILEXSIZE, nYSize);
        TILEYSIZE = 0;
    }

    if (CODEBLOCK_WIDTH<4 || CODEBLOCK_WIDTH>1024)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for CODEBLOCK_WIDTH : %d. Defaulting to 64.",
                CODEBLOCK_WIDTH);
        CODEBLOCK_WIDTH = 64;
    }
    if (CODEBLOCK_HEIGHT<4 || CODEBLOCK_HEIGHT>1024)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Unsupported value for CODEBLOCK_HEIGHT : %d. Defaulting to 64.",
                CODEBLOCK_HEIGHT);
        CODEBLOCK_HEIGHT = 64;
    }
    if( CODEBLOCK_WIDTH * CODEBLOCK_HEIGHT > 4096 )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Invalid values for codeblock size. "
                 "CODEBLOCK_WIDTH * CODEBLOCK_HEIGHT should be <= 4096. "
                 "Defaulting to 64x64");
        CODEBLOCK_WIDTH = 64;
        CODEBLOCK_HEIGHT = 64;
    }
    int nCblockW_po2 = FloorPowerOfTwo(CODEBLOCK_WIDTH);
    int nCblockH_po2 = FloorPowerOfTwo(CODEBLOCK_HEIGHT);
    if( nCblockW_po2 != CODEBLOCK_WIDTH || nCblockH_po2 != CODEBLOCK_HEIGHT )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Non power of two values used for codeblock size. "
                 "Using to %dx%d",
                 nCblockW_po2, nCblockH_po2);
    }
    CODEBLOCK_WIDTH = nCblockW_po2;
    CODEBLOCK_HEIGHT = nCblockH_po2;

    JP2_Property_Value ERROR_RESILIENCE_VALUE = 0;
    if (ERROR_RESILIENCE)
    {
        ERROR_RESILIENCE_VALUE = cJP2_Coder_Context_Reset |
                                 cJP2_Coder_All_Pass_Terminate |
                                 cJP2_Coder_Vertical_Causal_Context |
                                 cJP2_Coder_Segmentation_Symbols;
    }
    else
    {
        ERROR_RESILIENCE_VALUE = 0;
    }


    /* -------------------------------------------------------------------- */
    /*      Georeferencing options                                          */
    /* -------------------------------------------------------------------- */

    int nGMLJP2Version = 1;
    if (pszGMLJP2V2Def != nullptr)
    {
        bGMLJP2Option = true;
        nGMLJP2Version = 2;
        /*if (bInspireTG)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
            "INSPIRE_TG=YES is only compatible with GMLJP2 v1");
            return NULL;
        }*/
    }

    bool bGeoreferencingCompatOfGeoJP2 = false;
    bool bGeoreferencingCompatOfGMLJP2 = false;

    if (bIsJP2OrJPX && (bGMLJP2Option || bGeoJP2Option))
    {
        if (poSrcDS->GetGCPCount() > 0)
        {
            if( CSLFetchNameValue(papszOptions, "GeoJP2") == nullptr )
                bGeoJP2Option = true;
            bGeoreferencingCompatOfGeoJP2 = true;
            oJP2MD.SetGCPs(poSrcDS->GetGCPCount(),
                    poSrcDS->GetGCPs());
            oJP2MD.SetSpatialRef(poSrcDS->GetGCPSpatialRef());
        }
        else
        {
            const auto poSRS = poSrcDS->GetSpatialRef();
            if (poSRS != nullptr && !poSRS->IsEmpty())
            {
                    bGeoreferencingCompatOfGeoJP2 = true;
                    oJP2MD.SetSpatialRef(poSRS);
            }
            double adfGeoTransform[6];
            if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None)
            {
                    bGeoreferencingCompatOfGeoJP2 = true;
                    oJP2MD.SetGeoTransform(adfGeoTransform);
            }
            bGeoreferencingCompatOfGMLJP2 =
                    poSRS != nullptr && !poSRS->IsEmpty() &&
                    poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None;
        }
        if (poSrcDS->GetMetadata("RPC") != nullptr)
        {
                oJP2MD.SetRPCMD(poSrcDS->GetMetadata("RPC"));
                bGeoreferencingCompatOfGeoJP2 = true;
        }

        const char* pszAreaOrPoint =
                            poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        oJP2MD.bPixelIsPoint = pszAreaOrPoint != nullptr &&
                                EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);
        if( oJP2MD.bPixelIsPoint &&
            CSLFetchNameValue(papszOptions, "GeoJP2") == nullptr )
            bGeoJP2Option = true;

        if (bGMLJP2Option &&
            CPLGetConfigOption("GMLJP2OVERRIDE", nullptr) != nullptr)
        {
            // Force V1 since this is the branch in which the hack is
            // implemented
            nGMLJP2Version = 1;
            bGeoreferencingCompatOfGMLJP2 = TRUE;
        }
    }

    if (CSLFetchNameValue(papszOptions, "GMLJP2") != nullptr && bGMLJP2Option &&
        !bGeoreferencingCompatOfGMLJP2)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                "GMLJP2 box was explicitly required but cannot be written due "
                "to lack of georeferencing and/or unsupported georeferencing "
                "for GMLJP2");
    }

    if (CSLFetchNameValue(papszOptions, "GeoJP2") != nullptr && bGeoJP2Option &&
        !bGeoreferencingCompatOfGeoJP2)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                "GeoJP2 box was explicitly required but cannot be written due "
                "to lack of georeferencing");
    }
    /*bool bGeoBoxesAfter = CPLFetchBool(papszOptions, "GEOBOXES_AFTER_JP2C",
            bInspireTG);*/
    GDALJP2Box* poGMLJP2Box = nullptr;
    if (bIsJP2OrJPX && bGMLJP2Option && bGeoreferencingCompatOfGMLJP2)
    {
        if (nGMLJP2Version == 1)
            poGMLJP2Box = oJP2MD.CreateGMLJP2(nXSize, nYSize);
        else
            poGMLJP2Box = oJP2MD.CreateGMLJP2V2(nXSize, nYSize,
                                                pszGMLJP2V2Def, poSrcDS);
        if (poGMLJP2Box == nullptr)
            return nullptr;
    }

    /*++++++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Start the compression process                    */
    /*++++++++++++++++++++++++++++++++++++++++++++++++++*/

    VSILFILE* fp = nullptr;
    bool bException = false;
    try
    {
        const bool bSamePrecision = (eDataType != GDT_Float32);

#ifdef ENABLE_MEMORY_REGISTRAR
        JP2_Callback_Param mallocFreeParam =
            reinterpret_cast<JP2_Callback_Param>(&(poDS->oMemoryRegistrar));
#else
        JP2_Callback_Param mallocFreeParam =
            static_cast<JP2_Callback_Param>(NULL);
#endif
        JP2_Error error = JP2_Compress_Start(
                &handle,
                GDALJP2Lura_Callback_Malloc, mallocFreeParam,
                GDALJP2Lura_Callback_Free, mallocFreeParam,
                (eDataType == GDT_Float32) ? 3 : static_cast<short>(nBands) );
        if( error )
        {
            snprintf(pcMsg, sizeof(pcMsg),
                        "Internal library error (%s).",
                        GetErrorMessage(error));

            throw pcMsg;
        }

        /* ***** set license keys *************************** */

        const char* pszNum1 = CPLGetConfigOption("LURA_LICENSE_NUM_1", "");
        const char* pszNum2 = CPLGetConfigOption("LURA_LICENSE_NUM_2", "");
        if ( !EQUAL(pszNum1, "") && !EQUAL(pszNum2, "") )
        {
            unsigned long license_num_1 =
                static_cast<unsigned long>(CPLAtoGIntBig(pszNum1));
            unsigned long license_num_2 =
                static_cast<unsigned long>(CPLAtoGIntBig(pszNum2));

            error = JP2_Compress_SetLicense(handle, license_num_1,
                                                 license_num_2);
            if( error )
            {
                switch (error)
                {
                    case cJP2_Error_Trial_Time_Expired:
                        snprintf(pcMsg, sizeof(pcMsg),
                            "The evaluation period for this software has expired.");
                        break;

                    case cJP2_Error_License_Level_Too_Low:
                        snprintf(pcMsg, sizeof(pcMsg),
                            "License cannot be used with this library version.");
                        break;

                    case cJP2_Error_Invalid_License:
                        snprintf(pcMsg, sizeof(pcMsg),
                            "Invalid license number.");
                        break;

                    default:
                        snprintf(pcMsg, sizeof(pcMsg),
                                 "Internal library error (%s).",
                                 GetErrorMessage(error));
                        break;
                }

                throw pcMsg;
            }
        }
        else
        {
            snprintf(pcMsg, sizeof(pcMsg),
                     "Environment variables LURA_LICENSE_NUM_1 and "
                     "LURA_LICENSE_NUM_2 not configured.");
            throw pcMsg;
        }

#define SetPropGeneral(prop,value) \
        do { JP2_Error l_error = JP2_Compress_SetProp(handle, prop, \
                        static_cast<JP2_Property_Value>(value), -1, -1); \
          if( l_error ) \
          { \
                snprintf(pcMsg, sizeof(pcMsg), \
                        "Internal library error (%s) while setting %s.", \
                         GetErrorMessage(l_error), #prop); \
                throw pcMsg; \
          } \
        } while(0);

        /* Always ask Lurawave to do codestream only. We will take care */
        /* of JP2 boxes */
        SetPropGeneral(cJP2_Prop_File_Format, cJP2_Format_Codestream);

    /* -------------------------------------------------------------------- */
    /*      Create the dataset.                                             */
    /* -------------------------------------------------------------------- */

        const char* pszAccess =
                    EQUALN(pszFilename, "/vsisubfile/", 12) ? "r+b" : "w+b";
        fp = VSIFOpenL(pszFilename, pszAccess);
        if (fp == nullptr)
        {
            throw "Cannot create file";
        }


        int nRedBandIndex = -1;
        int nGreenBandIndex = -1;
        int nBlueBandIndex = -1;
        int nAlphaBandIndex = -1;
        for(int i=0;i<nBands;i++)
        {
            GDALColorInterp eInterp =
                    poSrcDS->GetRasterBand(i+1)->GetColorInterpretation();
            if( eInterp == GCI_RedBand )
                nRedBandIndex = i;
            else if( eInterp == GCI_GreenBand )
                nGreenBandIndex = i;
            else if( eInterp == GCI_BlueBand )
                nBlueBandIndex = i;
            else if( eInterp == GCI_AlphaBand )
                nAlphaBandIndex = i;
        }
        const JP2_Colorspace eColorspace =
            ( (nBands == 3 || nBands == 4) &&
            nRedBandIndex >= 0 && nGreenBandIndex >= 0 && nBlueBandIndex >= 0 ) ?
            cJP2_Colorspace_RGBa : cJP2_Colorspace_Gray;

    /* -------------------------------------------------------------------- */
    /*      Add JP2 boxes.                                                  */
    /* -------------------------------------------------------------------- */
        //vsi_l_offset nStartJP2C = 0;
        bool bUseXLBoxes = false;

        if ( bIsJP2OrJPX)
        {
            GDALJP2Box jPBox(fp);
            jPBox.SetType("jP  ");
            jPBox.AppendWritableData(4, "\x0D\x0A\x87\x0A");
            WriteBox(fp, &jPBox);

            GDALJP2Box ftypBox(fp);
            ftypBox.SetType("ftyp");
            // http://docs.opengeospatial.org/is/08-085r5/08-085r5.html Req 19
            const bool bJPXOption = CPLFetchBool( papszOptions, "JPX", true );
            if( nGMLJP2Version == 2 && bJPXOption )
                ftypBox.AppendWritableData(4, "jpx "); /* Branding */
            else
                ftypBox.AppendWritableData(4, "jp2 "); /* Branding */
            ftypBox.AppendUInt32(0); /* minimum version */
            ftypBox.AppendWritableData(4, "jp2 "); /* Compatibility list: first value */

            /*
            if (bInspireTG && poGMLJP2Box != NULL && !bJPXOption)
            {
            CPLError(CE_Warning, CPLE_AppDefined,
            "INSPIRE_TG=YES implies following GMLJP2 specification which "
            "recommends advertise reader requirement 67 feature, and thus JPX capability");
            }
            else */
            if (poGMLJP2Box != nullptr && bJPXOption)
            {
                /* GMLJP2 uses lbl and asoc boxes, which are JPEG2000 Part II spec */
                /* advertizing jpx is required per 8.1 of 05-047r3 GMLJP2 */
                ftypBox.AppendWritableData(4, "jpx "); /* Compatibility list: second value */
            }
            WriteBox(fp, &ftypBox);

            const bool bIPR = poSrcDS->GetMetadata("xml:IPR") != nullptr &&
                        WRITE_METADATA;

            /* Reader requirement box */
            if (poGMLJP2Box != nullptr && bJPXOption)
            {
                GDALJP2Box rreqBox(fp);
                rreqBox.SetType("rreq");
                rreqBox.AppendUInt8(1); /* ML = 1 byte for mask length */

                rreqBox.AppendUInt8(0x80 | 0x40 | (bIPR ? 0x20 : 0)); /* FUAM */
                rreqBox.AppendUInt8(0x80); /* DCM */

                rreqBox.AppendUInt16(static_cast<GUInt16>(2 + (bIPR ? 1 : 0))); /* NSF: Number of standard features */

                rreqBox.AppendUInt16((bProfile1) ? 4 : 5); /* SF0 : PROFILE 1 or PROFILE 2 */
                rreqBox.AppendUInt8(0x80); /* SM0 */

                rreqBox.AppendUInt16(67); /* SF1 : GMLJP2 box */
                rreqBox.AppendUInt8(0x40); /* SM1 */

                if (bIPR)
                {
                    rreqBox.AppendUInt16(35); /* SF2 : IPR metadata */
                    rreqBox.AppendUInt8(0x20); /* SM2 */
                }
                rreqBox.AppendUInt16(0); /* NVF */
                WriteBox(fp, &rreqBox);
            }

            GDALJP2Box ihdrBox(fp);
            ihdrBox.SetType("ihdr");
            ihdrBox.AppendUInt32(nYSize);
            ihdrBox.AppendUInt32(nXSize);
            if (nBands == 1 && eDataType == GDT_Float32)
                ihdrBox.AppendUInt16(3);
            else
                ihdrBox.AppendUInt16(static_cast<GUInt16>(nBands));
            GByte BPC;
            if (bSamePrecision)
                BPC = static_cast<GByte>((ulBps - 1) | (bSigned ? 0x80 : 0));
            else
                BPC = 255;
            ihdrBox.AppendUInt8(BPC);
            ihdrBox.AppendUInt8(7); /* C=Compression type: fixed value */
            ihdrBox.AppendUInt8(0); /* UnkC: 0= colourspace of the image is known */
            /*and correctly specified in the Colourspace Specification boxes within the file */
            ihdrBox.AppendUInt8(bIPR ? 1 : 0); /* IPR: 0=no intellectual property, 1=IPR box */

            GDALJP2Box bpccBox(fp);
            if (!bSamePrecision)
            {
                bpccBox.SetType("bpcc");
                bpccBox.AppendUInt8(0x80);
                bpccBox.AppendUInt8(8-1);
                bpccBox.AppendUInt8(23-1);
            }

            GDALJP2Box colrBox(fp);
            colrBox.SetType("colr");
            colrBox.AppendUInt8(1); /* METHOD: 1=Enumerated Colourspace */
            colrBox.AppendUInt8(0); /* PREC: Precedence. 0=(field reserved for ISO use) */
            colrBox.AppendUInt8(0); /* APPROX: Colourspace approximation. */
            GUInt32 enumcs = ( eColorspace == cJP2_Colorspace_RGBa ) ? 16 : 17;
            colrBox.AppendUInt32(enumcs); /* EnumCS: Enumerated colourspace */

            GDALJP2Box cdefBox(fp);
            if( ((nBands == 3 || nBands == 4) &&
                (nRedBandIndex != 0 || nGreenBandIndex != 1 ||
                 nBlueBandIndex != 2)) ||
                nAlphaBandIndex >= 0)
            {
                cdefBox.SetType("cdef");
                int nComponents = nBands;
                cdefBox.AppendUInt16(static_cast<GUInt16>(nComponents));
                for(int i=0;i<nComponents;i++)
                {
                    /* Component number */
                    cdefBox.AppendUInt16(static_cast<GUInt16>(i));
                    if( i != nAlphaBandIndex )
                    {
                        /* Signification: This channel is the colour image data
                           for the associated colour */
                        cdefBox.AppendUInt16(0);
                        if( enumcs == 17 && nComponents == 2)
                        {
                            /* Colour of the component: associated with a
                             *particular colour */
                            cdefBox.AppendUInt16(1);
                        }
                        else if ( enumcs == 16 &&
                                        (nComponents == 3 || nComponents == 4) )
                        {
                            if( i == nRedBandIndex )
                                cdefBox.AppendUInt16(1);
                            else if( i == nGreenBandIndex )
                                cdefBox.AppendUInt16(2);
                            else if( i == nBlueBandIndex )
                                cdefBox.AppendUInt16(3);
                            else
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Could not associate band %d "
                                         "with a red/green/blue channel",
                                         i+1);
                                cdefBox.AppendUInt16(65535);
                            }
                        }
                        else
                        {
                            /* Colour of the component: not associated with
                               any particular colour */
                            cdefBox.AppendUInt16(65535);
                        }
                    }
                    else
                    {
                        /* Signification: Non pre-multiplied alpha */
                        cdefBox.AppendUInt16(1);
                        /* Colour of the component: This channel is
                         *  associated as the image as a whole */
                        cdefBox.AppendUInt16(0);
                    }
                }
            }

            // Add res box if needed
            double dfXRes = 0, dfYRes = 0;
            int nResUnit = 0;
            GDALJP2Box* poRes = nullptr;
            if (poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION") != nullptr
                && poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION") != nullptr
                && poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT") != nullptr)
            {
                dfXRes =
                    CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION"));
                dfYRes =
                    CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION"));
                nResUnit =
                    atoi(poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT"));
#define PIXELS_PER_INCH 2
#define PIXELS_PER_CM   3

                if (nResUnit == PIXELS_PER_INCH)
                {
                    // convert pixels per inch to pixels per cm.
                    dfXRes = dfXRes * 39.37 / 100.0;
                    dfYRes = dfYRes * 39.37 / 100.0;
                    nResUnit = PIXELS_PER_CM;
                }

                if (nResUnit == PIXELS_PER_CM &&
                        dfXRes > 0 && dfYRes > 0 &&
                        dfXRes < 65535 && dfYRes < 65535)
                {
                    /* Format a resd box and embed it inside a res box */
                    GDALJP2Box oResd;
                    oResd.SetType("resd");

                    int nYDenom = 1;
                    while (nYDenom < 32767 && dfYRes < 32767)
                    {
                            dfYRes *= 2;
                            nYDenom *= 2;
                    }
                    int nXDenom = 1;
                    while (nXDenom < 32767 && dfXRes < 32767)
                    {
                            dfXRes *= 2;
                            nXDenom *= 2;
                    }

                    oResd.AppendUInt16((GUInt16)dfYRes);
                    oResd.AppendUInt16((GUInt16)nYDenom);
                    oResd.AppendUInt16((GUInt16)dfXRes);
                    oResd.AppendUInt16((GUInt16)nXDenom);
                    oResd.AppendUInt8(2); /* vertical exponent */
                    oResd.AppendUInt8(2); /* horizontal exponent */

                    GDALJP2Box* poResd = &oResd;
                    poRes = GDALJP2Box::CreateAsocBox(1, &poResd);
                    poRes->SetType("res ");
                }
            }

            /* Build and write jp2h super box now */
            GDALJP2Box* apoBoxes[7];
            int nBoxes = 1;
            apoBoxes[0] = &ihdrBox;
            if (bpccBox.GetDataLength())
                apoBoxes[nBoxes++] = &bpccBox;
            apoBoxes[nBoxes++] = &colrBox;
            //if (pclrBox.GetDataLength())
            //  apoBoxes[nBoxes++] = &pclrBox;
            //if (cmapBox.GetDataLength())
            //  apoBoxes[nBoxes++] = &cmapBox;
            if (cdefBox.GetDataLength())
                apoBoxes[nBoxes++] = &cdefBox;
            if (poRes)
                apoBoxes[nBoxes++] = poRes;
            GDALJP2Box* psJP2HBox = GDALJP2Box::CreateSuperBox("jp2h",
                                                               nBoxes,
                                                               apoBoxes);
            WriteBox(fp, psJP2HBox);
            delete psJP2HBox;
            delete poRes;

            if (bGeoJP2Option && bGeoreferencingCompatOfGeoJP2)
            {
                GDALJP2Box* poBox = oJP2MD.CreateJP2GeoTIFF();
                WriteBox(fp, poBox);
                delete poBox;
            }

            if (WRITE_METADATA &&
                    !MAIN_MD_DOMAIN_ONLY)
            {
                WriteXMPBox(fp, poSrcDS, papszOptions);
            }

            if (WRITE_METADATA)
            {
                if (!MAIN_MD_DOMAIN_ONLY)
                    WriteXMLBoxes(fp, poSrcDS, papszOptions);
                WriteGDALMetadataBox(fp, poSrcDS, papszOptions);
            }

            if (poGMLJP2Box != nullptr)
            {
                WriteBox(fp, poGMLJP2Box);
            }
        }

    /* -------------------------------------------------------------------- */
    /*      Try lossless reuse of an existing JPEG2000 codestream           */
    /* -------------------------------------------------------------------- */
        vsi_l_offset nCodeStreamLength = 0;
        vsi_l_offset nCodeStreamStart = 0;
        VSILFILE* fpSrc = nullptr;
        if( USE_SRC_CODESTREAM)
        {
            CPLString osSrcFilename(poSrcDS->GetDescription());
            if (poSrcDS->GetDriver() != nullptr &&
                    poSrcDS->GetDriver() == GDALGetDriverByName("VRT"))
            {
                    VRTDataset* poVRTDS = (VRTDataset*)poSrcDS;
                    GDALDataset* poSimpleSourceDS =
                                            poVRTDS->GetSingleSimpleSource();
                    if (poSimpleSourceDS)
                            osSrcFilename = poSimpleSourceDS->GetDescription();
            }

            fpSrc = VSIFOpenL(osSrcFilename, "rb");
            if (fpSrc)
            {
                nCodeStreamStart = JP2LuraFindCodeStream(fpSrc,
                            &nCodeStreamLength);
            }
            if (nCodeStreamLength == 0)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "USE_SRC_CODESTREAM=YES specified, "
                         "but no codestream found");
            }
        }

        if (bIsJP2OrJPX)
        {
            // Start codestream box
            //nStartJP2C = VSIFTellL(fp);
            if (nCodeStreamLength)
                bUseXLBoxes = ((vsi_l_offset)(GUInt32)nCodeStreamLength !=
                                                            nCodeStreamLength);
            /*else
                bUseXLBoxes =
                    CSLFetchBoolean(papszOptions, "JP2C_XLBOX", FALSE) ||
                    (GIntBig)nXSize * nYSize * nBands * nDataTypeSize /
                                    dfRates.back() > 4e9;*/
            GUInt32 nLBox = (bUseXLBoxes) ? 1 : 0;
            CPL_MSBPTR32(&nLBox);
            VSIFWriteL(&nLBox, 1, 4, fp);
            VSIFWriteL("jp2c", 1, 4, fp);
            if (bUseXLBoxes)
            {
                GUIntBig nXLBox = 0;
                VSIFWriteL(&nXLBox, 1, 8, fp);
            }
        }

    /* -------------------------------------------------------------------- */
    /*      Do lossless reuse of an existing JPEG2000 codestream            */
    /* -------------------------------------------------------------------- */
        if (fpSrc)
        {
            const char* apszIgnoredOptions[] = {
                    "TILEXSIZE", "TILEYSIZE", "QUALITY", "REVERSIBLE",
                    "LAYERS", "PROGRESSION",
                    "CODEBLOCK_WIDTH", "CODEBLOCK_HEIGHT", nullptr };
            for (int i = 0; apszIgnoredOptions[i]; i++)
            {
                if (CSLFetchNameValue(papszOptions, apszIgnoredOptions[i]))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "Option %s ignored when USE_SRC_CODESTREAM=YES",
                            apszIgnoredOptions[i]);
                }
            }
            GByte abyBuffer[4096];
            VSIFSeekL(fpSrc, nCodeStreamStart, SEEK_SET);
            vsi_l_offset nRead = 0;
            while (nRead < nCodeStreamLength)
            {
                int nToRead = (nCodeStreamLength - nRead > 4096) ? 4096 :
                        (int)(nCodeStreamLength - nRead);
                if ((int)VSIFReadL(abyBuffer, 1, nToRead, fpSrc) != nToRead)
                {
                    VSIFCloseL(fpSrc);

                    throw "Cannot read source codestream";
                }

#ifdef disabled
                if( nRead == 0 && (pszProfile /*|| bInspireTG*/) &&
                    abyBuffer[2] == 0xFF && abyBuffer[3] == 0x51 )
                {
                    if( EQUAL(pszProfile, "UNRESTRICTED") )
                    {
                        abyBuffer[6] = 0;
                        abyBuffer[7] = 0;
                    }
                    else if( EQUAL(pszProfile, "PROFILE_1") /*|| bInspireTG*/ )
                    {
                        // TODO: ultimately we should check that we can
                        //really set Profile 1
                        abyBuffer[6] = 0;
                        abyBuffer[7] = 2;
                    }
                }
#endif

                if ((int)VSIFWriteL(abyBuffer, 1, nToRead, fp) != nToRead ||
                        !pfnProgress((nRead + nToRead) * 1.0 / nCodeStreamLength,
                        nullptr, pProgressData))
                {
                    VSIFCloseL(fpSrc);

                    throw "Cannot write source codestream";
                }
                nRead += nToRead;
            }

            VSIFCloseL(fpSrc);
        }

    /* -------------------------------------------------------------------- */
    /*      Assign creation options.                                        */
    /* -------------------------------------------------------------------- */
        SetPropGeneral(cJP2_Prop_Write_TLM_Marker, static_cast<int>(TLM));
        SetPropGeneral(cJP2_Prop_Height, nYSize);
        SetPropGeneral(cJP2_Prop_Width, nXSize);

        if (eDataType == GDT_Float32 && nBands == 1)
        {
            short channel = 0;
            // 1, 8 and 23 bits to code IEEE754 floating value
            JP2_Property_Value pvBps[3] = { 1, 8, 23 };
            // signed, unsigned, unsigned to store values as IEEE754
            JP2_Property_Value pvSpc[3] = { 1, 0, 0 };
            // Lossless encoding of sign bit and exponent
            JP2_Property_Value cJP2_Waveleta[3] =
                { cJP2_Wavelet_5_3, cJP2_Wavelet_5_3, cJP2_Wavelet_9_7 };
            JP2_Property_Value cJP2_Quanta[3] =
                { cJP2_Quant_Expounded, cJP2_Quant_Expounded,
                  cJP2_Quant_Expounded };

            if (REVERSIBLE == false)
            {
                if (RATE == 0 && QUALITY != 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "Using QUALITY option will also affect the REVERSIBLE "
                        "sign and exponent band, as the SDK can only apply "
                        "the QUALITY parameter the whole image. Thus numeric "
                        "Float pixels will be affected");
                    SetPropGeneral(cJP2_Prop_Rate_Quality, QUALITY);
                }
                if (QUALITY == 0 && RATE != 0)
                {

                    GUIntBig ullTotalBytes =
                                        ((GUIntBig)nXSize * nYSize * 32) >> 3;

                    GUIntBig ulMaxBytes = ullTotalBytes / RATE;
                    //This property can only be set for the complete image
                    SetPropGeneral(cJP2_Prop_Rate_Bytes, ulMaxBytes);
                }
            }
            else
            {
                if( RATE != 0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "RATE option is ignored");
                }
                if( QUALITY != 0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "QUALITY option is ignored");
                }
            }

#define SetPropPerChannel(prop,value,channel) \
        do { JP2_Error l_error = JP2_Compress_SetProp(handle, prop, \
                        static_cast<JP2_Property_Value>(value), -1, channel); \
          if( l_error ) \
          { \
                snprintf(pcMsg, sizeof(pcMsg), \
                        "Internal library error (%s) while setting %s.", \
                         GetErrorMessage(l_error), #prop); \
                throw pcMsg; \
          } \
        } while(0);

            for (channel = 2; channel >= 0; channel--)
            {
                SetPropPerChannel(cJP2_Prop_Bits_Per_Sample, pvBps[channel],
                                  channel);

                SetPropPerChannel(cJP2_Prop_Signed_Samples, pvSpc[channel],
                                  channel);

                if (REVERSIBLE == false)
                {
                    if(QUALITY==0 && RATE==0)
                    {
                        SetPropPerChannel(cJP2_Prop_Wavelet_Filter,
                                        cJP2_Waveleta[channel], channel);
                        if (cJP2_Waveleta[channel] == cJP2_Wavelet_9_7 )
                        {
                            SetPropPerChannel(cJP2_Prop_Quantization_Style,
                                            cJP2_Quanta[channel], channel);
                        }
                    }
                    else
                    {
                        SetPropPerChannel(cJP2_Prop_Wavelet_Filter,
                                          cJP2_Wavelet_9_7, channel);
                    }
                }
                else
                {
                    SetPropPerChannel(cJP2_Prop_Wavelet_Filter,
                                      cJP2_Wavelet_5_3, channel);
                }
            }
        }
        else
        {
            SetPropGeneral(cJP2_Prop_Bits_Per_Sample, ulBps);
            SetPropGeneral(cJP2_Prop_Signed_Samples, bSigned ? 1 : 0);
            if (RATE != 0)
            {
                if( REVERSIBLE )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "RATE option is specified. "
                            "Forcing irreversible wavelet");
                }
                cJP2_Wavelet = cJP2_Wavelet_9_7;

                GUIntBig ullTotalBytes = ((GUIntBig)nXSize * nYSize *
                                                nBands * ulBps + 7) >> 3;

                GUIntBig ulMaxBytes = ullTotalBytes / RATE;
                SetPropGeneral(cJP2_Prop_Rate_Bytes, ulMaxBytes);
            }
            SetPropGeneral(cJP2_Prop_Wavelet_Filter, cJP2_Wavelet);
            if (REVERSIBLE == false)
            {
                SetPropGeneral(cJP2_Prop_Quantization_Style, cJP2_Quant);
            }
            if (RATE == 0 && QUALITY != 0)
            {
                if( cJP2_Wavelet == cJP2_Wavelet_9_7 )
                {
                    SetPropGeneral(cJP2_Prop_Rate_Quality, QUALITY);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "QUALITY option is ignored");
                }
            }
        }

        SetPropGeneral(cJP2_Prop_Extern_Colorspace, eColorspace);
        SetPropGeneral(cJP2_Prop_Wavelet_Levels, LEVELS);
        SetPropGeneral(cJP2_Prop_Precise, PRECISION);

        // SetPropGeneral(cJP2_Prop_Layers, XXXX);

        /* set progression order */
        SetPropGeneral(cJP2_Prop_Progression, cJP2_Progression);

        /* set visual weighting */
        SetPropGeneral(cJP2_Prop_Quality_Style, cJP2_Quality);
        SetPropGeneral(cJP2_Prop_Speed_Mode, cJP2_Speed);
        SetPropGeneral(cJP2_Prop_Coder_Options, ERROR_RESILIENCE_VALUE);

        /* Tile dimensions */
        if (TILEXSIZE == 0 && TILEYSIZE == 0)
        {
            if (nXSize > 15000 && nYSize > 15000)
            {
                TILEXSIZE = 1024;
                TILEYSIZE = 1024;
                CPLDebug("JP2Lura", "Using 1024x1024 tiles");
            }
        }
        SetPropGeneral(cJP2_Prop_Tile_Width, TILEXSIZE);
        SetPropGeneral(cJP2_Prop_Tile_Height, TILEYSIZE);

        /* Code block dimensions */
        SetPropGeneral(cJP2_Prop_Codeblock_Width, CODEBLOCK_WIDTH);
        SetPropGeneral(cJP2_Prop_Codeblock_Height, CODEBLOCK_HEIGHT);

        JP2_Gdal_Stream_Data data;
        data.fp = fp;
        data.Position = VSIFTellL(fp);

        SetPropGeneral(cJP2_Prop_Write_Parameter,
                       reinterpret_cast<JP2_Property_Value>(&data));
        SetPropGeneral(cJP2_Prop_Write_Function,
                       reinterpret_cast<JP2_Property_Value>(
                            GDALJP2Lura_Callback_Compress_Write));

        GDALJP2Lura_Input_Data idata;
        idata.poSrcDS = poSrcDS;
        idata.pProgressData = pProgressData;
        idata.pfnProgress = pfnProgress;

        SetPropGeneral(cJP2_Prop_Input_Parameter,
                       reinterpret_cast<JP2_Property_Value>(&idata));
        SetPropGeneral(cJP2_Prop_Input_Function,
                       reinterpret_cast<JP2_Property_Value>(
                                    GDALJP2Lura_Callback_Compress_Read));

        /*++++++++++++++++++++++++++++++++++++++++++++++++++*/
        /* Compress                                         */
        /*++++++++++++++++++++++++++++++++++++++++++++++++++*/
        if ( !USE_SRC_CODESTREAM )
        {
            error = JP2_Compress_Image(handle);
            if( error )
            {
                snprintf(pcMsg, sizeof(pcMsg),
                         "Internal library error (%s) when compressing.",
                         GetErrorMessage(error));

                throw pcMsg;
            }
        }
    }
    catch( const char* msg )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", msg);
        bException = true;
    }

    /*++++++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Tidy up and end process                          */
    /*++++++++++++++++++++++++++++++++++++++++++++++++++*/
    if (poGMLJP2Box != nullptr)
        delete poGMLJP2Box;
    if (handle)
    {
        JP2_Compress_End(handle);
    }
    if (fp)
    {
        VSIFCloseL(fp);
    }

    if( bException )
        return nullptr;

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                        JP2LuraFindCodeStream()                       */
/************************************************************************/

static vsi_l_offset JP2LuraFindCodeStream(VSILFILE* fp,
                                          vsi_l_offset* pnLength)
{
    vsi_l_offset nCodeStreamStart = 0;
    vsi_l_offset nCodeStreamLength = 0;

    VSIFSeekL(fp, 0, SEEK_SET);
    GByte abyHeader[16];
    VSIFReadL(abyHeader, 1, 16, fp);

    if (memcmp(abyHeader, jpc_header, sizeof(jpc_header)) == 0)
    {
        VSIFSeekL(fp, 0, SEEK_END);
        nCodeStreamLength = VSIFTellL(fp);
    }
    else if (memcmp(abyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp)) == 0)
    {
        /* Find offset of first jp2c box */
        GDALJP2Box oBox(fp);
        if (oBox.ReadFirst())
        {
            while (strlen(oBox.GetType()) > 0)
            {
                if (EQUAL(oBox.GetType(), "jp2c"))
                {
                    nCodeStreamStart = VSIFTellL(fp);
                    nCodeStreamLength = oBox.GetDataLength();
                    break;
                }

                if (!oBox.ReadNext())
                    break;
            }
        }
    }
    *pnLength = nCodeStreamLength;
    return nCodeStreamStart;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JP2LuraDataset::Open(GDALOpenInfo * poOpenInfo)

{
    JP2_Error          error;
    JP2_Property_Value value;
    short              sChannel, sIndex;

    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr)
        return nullptr;

    // No thread-safety issue here
    static bool bIsFirstTime = true;
    if( bIsFirstTime )
    {
        bIsFirstTime = false;
        unsigned long nMajor = 0;
        unsigned long nMinor = 0;
        char* pszVersionString = nullptr;
        unsigned char nLicenseLevel = 0;
        JP2_Common_Get_Library_Version(&nMajor, &nMinor, &pszVersionString,
                                       &nLicenseLevel);
        CPLDebug("JP2Lura", "Runtime info: v%lu.%lu (%s), level=%u",
                 nMajor, nMinor, pszVersionString ? pszVersionString : "",
                 nLicenseLevel);
        CPLDebug("JP2Lura", "Compile-time info: v%.02f (%s), level=%u",
                 LWF_JP2_VERSION, LWF_JP2_VERSION_STRING,
                 LWF_JP2_LICENSE_LEVEL);
    }

    JP2LuraDataset *poDS = new JP2LuraDataset();

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Start the decompression process              */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/
#ifdef ENABLE_MEMORY_REGISTRAR
    JP2_Callback_Param mallocFreeParam =
            reinterpret_cast<JP2_Callback_Param>(&(poDS->oMemoryRegistrar));
#else
    JP2_Callback_Param mallocFreeParam =
            static_cast<JP2_Callback_Param>(NULL);
#endif
    error = JP2_Decompress_Start(
                        &(poDS->sOutputData.handle),
                        GDALJP2Lura_Callback_Malloc, mallocFreeParam,
                        GDALJP2Lura_Callback_Free, mallocFreeParam,
                        GDALJP2Lura_Callback_Decompress_Read,
                        reinterpret_cast<JP2_Callback_Param>(poOpenInfo->fpL) );
    if( error )
    {
        if (error == cJP2_Error_Not_Yet_Supported)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "File contains as yet unsupported JPEG 2000 features.");
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Internal library error (%s).", GetErrorMessage(error));
        }

        delete poDS;
        return nullptr;
    }

    const char* pszNum1 = CPLGetConfigOption("LURA_LICENSE_NUM_1", "");
    const char* pszNum2 = CPLGetConfigOption("LURA_LICENSE_NUM_2", "");
    if ( !EQUAL(pszNum1, "") && !EQUAL(pszNum2, "") )
    {
        unsigned long license_num_1 =
            static_cast<unsigned long>(CPLAtoGIntBig(pszNum1));
        unsigned long license_num_2 =
            static_cast<unsigned long>(CPLAtoGIntBig(pszNum2));

        static bool bFirstTimeLicenseInfo = true;
        if( bFirstTimeLicenseInfo )
        {
            bFirstTimeLicenseInfo = false;
            unsigned char nDay = 0;
            unsigned char nMonth = 0;
            unsigned short nYear = 0;
            error = JP2_Common_Get_License_Expiry_Date(
                    license_num_1, license_num_2, &nDay, &nMonth, &nYear);
            if( !error )
            {
                CPLDebug("JP2Lura", "Licence expires on %04u/%02u/%02u",
                         nYear, nMonth, nDay);
            }

            unsigned char nLicenseLevel = 0;
            error = JP2_Common_Get_License_Level(license_num_1, license_num_2,
                                                 &nLicenseLevel);
            if( !error )
            {
                CPLDebug("JP2Lura", "Licence level is %u", nLicenseLevel);
            }
        }

        error = JP2_Decompress_SetLicense(poDS->sOutputData.handle,
                                          license_num_1, license_num_2);
        if( error )
        {
            switch (error)
            {
                case cJP2_Error_Trial_Time_Expired:
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "The evaluation period for this software has expired.");
                    break;

                case cJP2_Error_License_Level_Too_Low:
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "License cannot be used with this library version.");
                    break;

                case cJP2_Error_Invalid_License:
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid license number.");
                    break;

                default:
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Internal library error (%s).",
                             GetErrorMessage(error));
                    break;
            }

            delete poDS;
            return nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Environment variables LURA_LICENSE_NUM_1 and "
                "LURA_LICENSE_NUM_2 not configured.");
        delete poDS;
        return nullptr;
    }

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Get the number of components                 */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/

    error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                        cJP2_Prop_Components, &value, 0, 0);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }

    short sSpp = static_cast<short>(value);

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Get the colourspace                          */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/

    error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                        cJP2_Prop_Extern_Colorspace, &value,
                                        0, 0);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }

    poDS->eColorspace = static_cast<JP2_Colorspace>(value);

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Get the channel definition structure         */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/

    error = JP2_Decompress_GetChannelDefs(poDS->sOutputData.handle,
                                        &(poDS->sOutputData.pChannelDefs),
                                        &(poDS->sOutputData.ulChannelDefs));
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }

    CPLDebug("JP2Lura", "components = %d", sSpp);
    CPLDebug("JP2Lura", "ulChannelDefs = %lu", poDS->sOutputData.ulChannelDefs);
    for( int i = 0;
         i < static_cast<int>(poDS->sOutputData.ulChannelDefs); ++i )
    {
        if( poDS->eColorspace == cJP2_Colorspace_RGBa &&
            poDS->sOutputData.pChannelDefs[i].ulType ==
                                                cJP2_Channel_Type_Color )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("JP2Lura", "associated[%d] = %lu",
                    i, poDS->sOutputData.pChannelDefs[i].ulAssociated );
#endif
            switch( poDS->sOutputData.pChannelDefs[i].ulAssociated )
            {
                case 1:
                    poDS->nRedIndex = i;
                    break;
                case 2:
                    poDS->nGreenIndex = i;
                    break;
                case 3:
                    poDS->nBlueIndex = i;
                    break;
                default:
                    break;
            }
        }
        else if( poDS->sOutputData.pChannelDefs[i].ulType ==
                                                cJP2_Channel_Type_Opacity )
        {
            poDS->nAlphaIndex = i;
        }
    }

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Get the palette, if necessary                */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/

    switch (poDS->eColorspace)
    {
        case cJP2_Colorspace_Palette_ICCa:
        case cJP2_Colorspace_Palette_Gray:
        case cJP2_Colorspace_Palette_RGBa:
        case cJP2_Colorspace_Palette_RGB_YCCa:
        case cJP2_Colorspace_Palette_CMYKa:
        case cJP2_Colorspace_Palette_CIE_LABa:

            if( sSpp != 1 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Only one component handled for paletted datasets");
                delete poDS;
                return nullptr;
            }

            error = JP2_Decompress_GetPalette(poDS->sOutputData.handle,
                                            &(poDS->sOutputData.pPalette));
            if( error )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Internal library error (%s).", GetErrorMessage(error));
                delete poDS;
                return nullptr;
            }

            if (!poDS->sOutputData.pPalette)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Internal library error (%s).", GetErrorMessage(error));
                delete poDS;
                return nullptr;
            }

            if( poDS->sOutputData.pPalette->ulChannels == 3 ||
                poDS->sOutputData.pPalette->ulChannels == 4 )
            {
                poDS->poCT = new GDALColorTable();
                for( unsigned long i=0;
                            i< poDS->sOutputData.pPalette->ulEntries; ++i )
                {
                    GDALColorEntry sEntry;
                    sEntry.c1 = static_cast<GInt16>(
                        poDS->sOutputData.pPalette->ppulPalette[0][i]);
                    sEntry.c2 = static_cast<GInt16>(
                        poDS->sOutputData.pPalette->ppulPalette[1][i]);
                    sEntry.c3 = static_cast<GInt16>(
                        poDS->sOutputData.pPalette->ppulPalette[2][i]);
                    sEntry.c4 = static_cast<GInt16>(
                        (poDS->sOutputData.pPalette->ulChannels == 4) ?
                        poDS->sOutputData.pPalette->ppulPalette[3][i] : 255);
                    poDS->poCT->SetColorEntry(static_cast<int>(i), &sEntry);
                }
            }

            break;

        default:
            poDS->sOutputData.pPalette = nullptr;
            break;
    }

    const short sStartChannel = 0;

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Get height, width, bpc                       */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/

    error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                cJP2_Prop_Height, &value, 0,
                                sStartChannel);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }

    const unsigned long ulHeight = static_cast<unsigned long>(value);

    error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                cJP2_Prop_Width, &value, 0,
                                sStartChannel);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }


    const unsigned long ulWidth = static_cast<unsigned long>(value);

    if (poDS->sOutputData.pPalette)
    {
        /* get the bit depth and sign of the first column of entries in */
        /* the palette table */

        poDS->sOutputData.lBps =
            poDS->sOutputData.pPalette->pucBitsPerSample[0];
        poDS->sOutputData.bSigned =
            CPL_TO_BOOL(poDS->sOutputData.pPalette->pucSignedSample[0]);

#ifdef unused
        /* get the bit depth of the palette index component */

        error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                    cJP2_Prop_Bits_Per_Sample, &value, 0, 0);
        if( error )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Internal library error (%s).", GetErrorMessage(error));
            delete poDS;
            return NULL;
        }

        poDS->sOutputData.lPaletteBps = static_cast<long>(value);
#endif
    }
    else
    {
        error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                cJP2_Prop_Bits_Per_Sample, &value, 0,
                                sStartChannel);
        if( error )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Internal library error (%s).", GetErrorMessage(error));
            delete poDS;
            return nullptr;
        }

        poDS->sOutputData.lBps = static_cast<long>(value);

        error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                    cJP2_Prop_Signed_Samples, &value, 0,
                                    sStartChannel);
        if( error )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Internal library error (%s).", GetErrorMessage(error));
            delete poDS;
            return nullptr;
        }

        poDS->sOutputData.bSigned = value != 0;

        // Detect special case of IEEE754 split Float32
        if (sSpp == 3)
        {
            //int isfloat = 0;
            JP2_Property_Value avalue[3] = { 0, 0, 0 };
            error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                    cJP2_Prop_Bits_Per_Sample, &(avalue[0]), 0, (short)0);
            if( !error )
                error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                    cJP2_Prop_Bits_Per_Sample, &(avalue[1]), 0, (short)1);
            if( !error )
                error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                        cJP2_Prop_Bits_Per_Sample, &(avalue[2]), 0, (short)2);
            if( error )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Internal library error (%s).", GetErrorMessage(error));
                delete poDS;
                return nullptr;
            }

            if (avalue[0] == 1 && avalue[1] == 8 && avalue[2] == 23)
            {
                // flags special values for float prototype
                poDS->sOutputData.lBps = 0;
                poDS->sOutputData.bSigned = false;
            }
        }
    }

    if (poDS->sOutputData.lBps != 0)
    {
        /*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
        /* Check that the width, height, bpps are the same for all components */
        /*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

        for (sIndex = sStartChannel + 1; sIndex <= sSpp - 1; sIndex++)
        {
            unsigned long ulCheckHeight, ulCheckWidth;
            bool bCheckSigned;
            long lCheckBps;

            if (poDS->sOutputData.pPalette)
            {
                if (sIndex < (short)poDS->sOutputData.pPalette->ulChannels)
                {
                    /* checking one of the columns of the palette */

                    lCheckBps =
                        poDS->sOutputData.pPalette->pucBitsPerSample[sIndex];
                    bCheckSigned = CPL_TO_BOOL(
                        poDS->sOutputData.pPalette->pucSignedSample[sIndex]);

                    sChannel = 0;
                }
                else
                {
                    sChannel = sIndex -
                            (short)poDS->sOutputData.pPalette->ulChannels + 1;

                    error = JP2_Decompress_GetProp(
                        poDS->sOutputData.handle, cJP2_Prop_Bits_Per_Sample,
                        &value, 0, sChannel);
                    if( error )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                        "Internal library error (%s).", GetErrorMessage(error));
                        delete poDS;
                        return nullptr;
                    }

                    lCheckBps = (long)value;

                    error = JP2_Decompress_GetProp(
                            poDS->sOutputData.handle, cJP2_Prop_Signed_Samples,
                            &value, 0, sChannel);
                    if( error )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                        "Internal library error (%s).", GetErrorMessage(error));
                        return nullptr;
                    }

                    bCheckSigned = value != 0;
                }
            }
            else
            {
                sChannel = sIndex;

                error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                        cJP2_Prop_Bits_Per_Sample, &value, 0, sChannel);
                if( error )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "Internal library error (%s).", GetErrorMessage(error));
                    delete poDS;
                    return nullptr;
                }

                lCheckBps = static_cast<long>(value);

                error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                        cJP2_Prop_Signed_Samples, &value, 0, sChannel);
                if( error )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "Internal library error (%s).", GetErrorMessage(error));
                    delete poDS;
                    return nullptr;
                }

                bCheckSigned = value != 0;
            }

            error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                    cJP2_Prop_Height, &value, 0, sChannel);
            if( error )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Internal library error (%s).", GetErrorMessage(error));
                delete poDS;
                return nullptr;
            }

            ulCheckHeight = static_cast<unsigned long>(value);

            error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                                    cJP2_Prop_Width, &value, 0, sChannel);
            if( error )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Internal library error (%s).", GetErrorMessage(error));
                delete poDS;
                return nullptr;
            }

            ulCheckWidth = static_cast<unsigned long>(value);

            if( ulCheckWidth == ulWidth / 2 &&
                ulCheckHeight == ulHeight / 2 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot handle 4:2:0 resampling");
                delete poDS;
                return nullptr;
            }

            if ((ulCheckHeight != ulHeight) ||
                (ulCheckWidth != ulWidth) ||
                (bCheckSigned != poDS->sOutputData.bSigned) ||
                (lCheckBps != poDS->sOutputData.lBps))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Warning: Only the first %d channel(s) will be decoded.",
                    sChannel);

                sSpp = sChannel;
                break;
            }
        }
    }

    /*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    /*  Decompress bitonal images as 8 bit grayscale            */
    /*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

    error = JP2_Decompress_SetProp(poDS->sOutputData.handle,
                                        cJP2_Prop_Expand_Bitonal, 1);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* calculate height and width of the image tile buffer */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/

    error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                cJP2_Prop_Tile_Height, &value, (long)-1, (short)-1);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }
    unsigned long ulTileH = static_cast<unsigned long>(value);

    error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                cJP2_Prop_Tile_Width, &value, (long)-1, (short)-1);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }
    unsigned long ulTileW = static_cast<unsigned long>(value);

    error = JP2_Decompress_SetProp(poDS->sOutputData.handle,
                        cJP2_Prop_InternalReadCache, cJP2_UseInternalCache);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */

    GDALDataType eDataType = GDT_Byte;
    if (poDS->sOutputData.lBps > 16)
    {
        if (poDS->sOutputData.bSigned)
            eDataType = GDT_Int32;
        else
            eDataType = GDT_UInt32;
    }
    else if (poDS->sOutputData.lBps > 8)
    {
        if (poDS->sOutputData.bSigned)
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }
    else if (poDS->sOutputData.lBps == 0) // case float
    {
        eDataType = GDT_Float32;
    }

    poDS->nRasterXSize = static_cast<int>(ulWidth);
    poDS->nRasterYSize = static_cast<int>(ulHeight);

    if (eDataType == GDT_Float32)
    {
        poDS->nBands = 1;
    }
    else
    {
        poDS->nBands = sSpp;
    }

    // do not generate tile sizes larger than the dataset
    // dimensions
    if (static_cast<unsigned long>(poDS->nRasterXSize) < ulTileW)
    {
        CPLDebug("JP2Lura", "Adjusting block width from %lu to %d",
                    ulTileW, poDS->nRasterXSize);
        ulTileW = poDS->nRasterXSize;
    }
    if (static_cast<unsigned long>(poDS->nRasterYSize) < ulTileH)
    {
        CPLDebug("JP2Lura", "Adjusting block width from %lu to %d",
                    ulTileH, poDS->nRasterYSize);
        ulTileH = poDS->nRasterYSize;
    }

    int nTileW = static_cast<int>(ulTileW);
    int nTileH = static_cast<int>(ulTileH);
    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (int iBand = 1; iBand <= poDS->nBands; iBand++)
    {
        JP2LuraRasterBand* poBand = new JP2LuraRasterBand(
                poDS, iBand, eDataType,
                static_cast<int>(poDS->sOutputData.lBps), nTileW, nTileH);
        poDS->SetBand(iBand, poBand);
    }

    /* -------------------------------------------------------------------- */
    /*      Create overview datasets.                                       */
    /* -------------------------------------------------------------------- */

    error = JP2_Decompress_GetProp(poDS->sOutputData.handle,
                    cJP2_Prop_Wavelet_Levels, &value, (long)0, (short)0);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).", GetErrorMessage(error));
        delete poDS;
        return nullptr;
    }

    int numResolutions = static_cast<int>(value);

    /* Lower resolutions are not compatible with a color-table */
    if( poDS->poCT != nullptr )
        numResolutions = 0;

    int nW = poDS->nRasterXSize;
    int nH = poDS->nRasterYSize;

    while (poDS->nOverviewCount + 1 < numResolutions &&
            (nW > 128 || nH > 128) )
    {
        nW = static_cast<int>(ceil(nW / 2.0));
        nH = static_cast<int>(ceil(nH / 2.0));

        JP2LuraDataset* poODS = new JP2LuraDataset();
        poODS->bIsInternal = true;
        poODS->SetDescription(poOpenInfo->pszFilename);
        poODS->iLevel = poDS->nOverviewCount + 1;

        if (nW < nTileW || nH < nTileH)
        {
            nTileW = nW;
            nTileH = nH;
        }
        //else
        //{
        //    nTileW /= 2;
        //    nTileH /= 2;
        //}

        poODS->nRasterXSize = nW;
        poODS->nRasterYSize = nH;

        poODS->nBands = poDS->nBands;
        poODS->poCT = poDS->poCT;
        poODS->eColorspace = poDS->eColorspace;
        poODS->nRedIndex = poDS->nRedIndex;
        poODS->nGreenIndex = poDS->nGreenIndex;
        poODS->nBlueIndex = poDS->nBlueIndex;
        poODS->nAlphaIndex = poDS->nAlphaIndex;

        memcpy(&(poODS->sOutputData), &(poDS->sOutputData),
               sizeof(GDALJP2Lura_Output_Data));
        poODS->sOutputData.pDatacache = nullptr;

        for (int iBand = 1; iBand <= poODS->nBands; iBand++)
        {
            poODS->SetBand(iBand, new JP2LuraRasterBand(
                    poODS, iBand, eDataType,
                    static_cast<int>(poDS->sOutputData.lBps),
                    nTileW, nTileH));
        }

        poDS->papoOverviewDS = (JP2LuraDataset**)CPLRealloc(
                poDS->papoOverviewDS,
                (poDS->nOverviewCount + 1) * sizeof(JP2LuraDataset*));

        poDS->papoOverviewDS[poDS->nOverviewCount++] = poODS;

    }

    poDS->LoadJP2Metadata(poOpenInfo);

    // Borrow fpL
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Vector layers                                                   */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR)
    {
        poDS->LoadVectorLayers(
                CSLFetchBoolean(poOpenInfo->papszOpenOptions,
                                "OPEN_REMOTE_GML", FALSE));

        // If file opened in vector-only mode and there's no vector,
        // return
        if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
                poDS->GetLayerCount() == 0)
        {
            delete poDS;
                return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr  JP2LuraDataset::IRasterIO(GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nBandCount, int *panBandMap,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GSpacing nBandSpace,
                                  GDALRasterIOExtraArg* psExtraArg)
{
    if (eRWFlag != GF_Read)
        return CE_Failure;

    if (nBandCount < 1)
        return CE_Failure;

    GDALRasterBand* poBand = GetRasterBand(panBandMap[0]);

    /* ==================================================================== */
    /*      Do we have overviews that would be appropriate to satisfy       */
    /*      this request?                                                   */
    /* ==================================================================== */

    if ((nBufXSize < nXSize || nBufYSize < nYSize)
            && poBand->GetOverviewCount() > 0 )
    {
        int         nOverview;
        GDALRasterIOExtraArg sExtraArg;

        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        nOverview =
                GDALBandGetBestOverviewLevel2(poBand, nXOff, nYOff,
                                              nXSize, nYSize,
                                            nBufXSize, nBufYSize, &sExtraArg);
        if (nOverview >= 0)
        {
            return papoOverviewDS[nOverview]->RasterIO(
                    eRWFlag, nXOff, nYOff, nXSize, nYSize,
                    pData, nBufXSize, nBufYSize, eBufType,
                    nBandCount, panBandMap,
                    nPixelSpace, nLineSpace, nBandSpace,
                    &sExtraArg);
        }
    }

    CPLErr eErr = GDALPamDataset::IRasterIO(eRWFlag,
            nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize,
            eBufType,
            nBandCount, panBandMap,
            nPixelSpace, nLineSpace, nBandSpace,
            psExtraArg);

    return eErr;
}

/************************************************************************/
/*                           WriteBox()                                 */
/************************************************************************/

void JP2LuraDataset::WriteBox(VSILFILE* fp, GDALJP2Box* poBox)
{
    GUInt32   nLBox;
    GUInt32   nTBox;

    if (poBox == nullptr)
        return;

    nLBox = (int)poBox->GetDataLength() + 8;
    nLBox = CPL_MSBWORD32(nLBox);

    memcpy(&nTBox, poBox->GetType(), 4);

    VSIFWriteL(&nLBox, 4, 1, fp);
    VSIFWriteL(&nTBox, 4, 1, fp);
    VSIFWriteL(poBox->GetWritableData(), 1, (int)poBox->GetDataLength(), fp);
}

/************************************************************************/
/*                         WriteGDALMetadataBox()                       */
/************************************************************************/

void JP2LuraDataset::WriteGDALMetadataBox(VSILFILE* fp,
                                          GDALDataset* poSrcDS,
                                          char** papszOptions)
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(
        poSrcDS, CSLFetchBoolean(papszOptions, "MAIN_MD_DOMAIN_ONLY", FALSE));
    if (poBox)
        WriteBox(fp, poBox);
    delete poBox;
}
/************************************************************************/
/*                           WriteXMPBox()                              */
/************************************************************************/

void JP2LuraDataset::WriteXMPBox(VSILFILE* fp, GDALDataset* poSrcDS,
                                 char** /*papszOptions*/)
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateXMPBox(poSrcDS);
    if (poBox)
        WriteBox(fp, poBox);
    delete poBox;
}
/************************************************************************/
/*                         WriteXMLBoxes()                              */
/************************************************************************/

void JP2LuraDataset::WriteXMLBoxes(VSILFILE* fp, GDALDataset* poSrcDS,
                                   char** /*papszOptions*/)
{
    int nBoxes = 0;
    GDALJP2Box** papoBoxes = GDALJP2Metadata::CreateXMLBoxes(poSrcDS, &nBoxes);
    for (int i = 0; i<nBoxes; i++)
    {
        WriteBox(fp, papoBoxes[i]);
        delete papoBoxes[i];
    }
    CPLFree(papoBoxes);
}
/************************************************************************/
/*                       GDALRegister_JP2Lura()                         */
/************************************************************************/

void GDALRegister_JP2Lura()

{
    GDALDriver  *poDriver;

    if (! GDAL_CHECK_VERSION("JP2Lura driver"))
        return;

    if( GDALGetDriverByName( "JP2Lura" ) == nullptr )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "JP2Lura" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "JPEG-2000 driver based on Lurawave library" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drivers/raster/jp2lura.html" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "jp2 j2f j2k" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte Int16 UInt16 Int32 UInt32 Float32");

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='OPEN_REMOTE_GML' type='boolean' description="
        "'Whether to load remote vector layers referenced by a link in a "
        "GMLJP2 v2 box' default='NO'/>"
"   <Option name='GEOREF_SOURCES' type='string' description="
        "'Comma separated list made with values INTERNAL/GMLJP2/GEOJP2/"
        "WORLDFILE/PAM/NONE that describe the priority order for "
        "georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
"</OpenOptionList>" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='CODEC' type='string-select' description="
            "'Codec to use. Default according to file extension. "
            "If unknown, default to JP2'>"
"       <Value>JP2</Value>"
"       <Value alias='J2K'>Codestream</Value>"
"   </Option>"
"   <Option name='JPX' type='boolean' description="
        "'Whether to advertise JPX features when a GMLJP2 box is written "
        "(or use JPX branding if GMLJP2 v2)' default='YES'/>"
"   <Option name='GeoJP2' type='boolean' description="
                        "'Whether to emit a GeoJP2 box' default='NO'/>"
"   <Option name='GMLJP2' type='boolean' description="
                        "'Whether to emit a GMLJP2 v1 box' default='YES'/>"
"   <Option name='GMLJP2V2_DEF' type='string' description="
        "'Definition file to describe how a GMLJP2 v2 box should be generated. "
        "If set to YES, a minimal instance will be created'/>"
"   <Option name='SPLIT_IEEE754' type='boolean' description="
        "'Whether encoding of Float32 bands as 3 bands with IEEE754 sign bit, "
        "exponent, mantissa values (non standard extension)' default='NO'/>"
"   <Option name='QUALITY_STYLE' type='string-select' description="
        "'This property tag is used to set the quality mode to be used during "
        "lossy compression.For normal images and situations (1:1 pixel display,"
        " ~50 cm viewing distance) we recommend Small or PSNR. For quality "
        "measurement only PSNR should be used' default='PSNR'>"
"       <Value>PSNR</Value>"
"       <Value>XXSmall</Value>"
"       <Value>XSmall</Value>"
"       <Value>Small</Value>"
"       <Value>Medium</Value>"
"       <Value>Large</Value>"
"       <Value>XLarge</Value>"
"       <Value>XXLarge</Value>"
"   </Option>"
"   <Option name='SPEED_MODE' type='string-select' description="
        "'This property tag is used to set the speed mode to be used "
        "during lossy compression. The following modes are defined' "
        "default='Fast'>"
"       <Value>Fast</Value>"
"       <Value>Accurate</Value>"
"   </Option>"
"   <Option name='RATE' type='int' description='"
        "When specifying this value, the target compressed file size will be "
        "the uncompressed file size divided by RATE. In general the "
        "achieved rate will be exactly the requested size or a few bytes "
        "lower. Will force use of irreversible wavelet. "
        "Default value: 0 (maximum quality).' default='0'/>"
"   <Option name='QUALITY' type='int' description="
        "'Compression to a particular quality is possible only when using "
        "the 9-7 filter with the standard expounded quantization and no regions"
        "of interest. A compression quality may be specified between 1 (low) "
        "and 100 (high). The size of the resulting JPEG2000 file will depend "
        "of the image content. Only used for irreversible compression. "
        "The compression quality cannot be used together "
        "the property RATE. Default value: 0 (maximum quality).' "
        "min='0' max='100' default='0'/>"
"   <Option name='PRECISION' type='int' description="
        "'For improved efficiency, the library automatically, depending on the "
        "image depth, uses either 16 or 32 bit representation for wavelet "
        "coefficients. The precision property can be set to force the library "
        "to always use 32 bit representations. The use of 32 bit values may "
        "slightly improve image quality and the expense of speed and memory "
        "requirements. Default value: 0 (automatically select appropriate "
        "precision)' default='0'/>"
"   <Option name='PROGRESSION' type='string-select' description="
        "'The organization of the coded data in the file can be set by this "
        "property tag. The following progression orders are defined: "
        "LRCP = Quality progressive, "
        "RLCP = Resolution then quality progressive, "
        "RPCL = Resolution then position progressive, "
        "PCRL = Position progressive, "
        "CPRL = Color/channel progressive. "
        "The setting LRCP (quality) is most useful when used with several "
        "layers. The PCRL (position) should be used with precincts.' "
        "default='LRCP'>"
"       <Value>LRCP</Value>"
"       <Value>RLCP</Value>"
"       <Value>RPCL</Value>"
"       <Value>PCRL</Value>"
"       <Value>CPRL</Value>"
"   </Option>"
"   <Option name='REVERSIBLE' type='boolean' description="
        "'The reversible (Filter 5_3) and irreversible (Filter 9_7), may be "
        "selected using this property.' default='FALSE'/>"
"   <Option name='LEVELS' type='int' description="
        "'The number of wavelet transformation levels can be set using this "
        "property. Valid values are in the range 0 (no wavelet analysis) to "
        "16 (very fine analysis). The memory requirements and compression time "
        "increases with the number of transformation levels. A reasonable "
        "number of transformation levels is in the 4-6 range.' "
        "min='0' max='16' default='5'/>"
"   <Option name='QUANTIZATION_STYLE' type='string-select' description="
        "'This property may only be set when the irreversible filter (9_7) is "
        "used. The quantization steps can either be derived from a bases "
        "quantization step, DERIVED, or calculated for each image sub-band, "
        "EXPOUNDED.The EXPOUNDED style is recommended when using the "
        "irreversible filter.' default='EXPOUNDED'>"
"       <Value>DERIVED</Value>"
"       <Value>EXPOUNDED</Value>"
"   </Option>"
"   <Option name='TILEXSIZE' type='int' description="
        "'Tile Width. An image can  be split into smaller tiles, with each "
        "tile independently compressed. The basic tile size and the offset to "
        "the first tile on the virtual compression reference grid can be set "
        "using these properties. The first tile must contain the first image "
        "pixel. The tiling of an image is recommended only for very large "
        "images. Default value: (0) One Tile containing the complete image."
        "' default='0'/>"
"   <Option name='TILEYSIZE' type='int' description="
        "'Tile Height. An image can be split into smaller tiles, with each "
        "tile independently compressed. The basic tile size and the offset to "
        "the first tile on the virtual compression reference grid can be set "
        "using these properties. The first tile must contain the first image "
        "pixel. The tiling of an image is recommended only for very large "
        "images. Default value: (0) One Tile containing the complete image."
        "' default='0'/>"
"   <Option name='TLM' type='boolean' description="
    "'The efficiency of decoding regions in a tiled image may be improved by "
    "the usage of a tile length marker. Tile length markers contain the "
    "position of each tile in a JPEG2000 codestream, enabling faster access "
    "to tiled data.' default='FALSE'/>"
"   <Option name='CODEBLOCK_WIDTH' type='int' description="
        "'The size of the blocks of data coded with the arithmetic entropy "
        "coder may be set using these parameters. A codeblock may contain no "
        "more than  4096 (result of CODEBLOCK_WIDTH x CODEBLOCK_HEIGHT) "
        "samples. Smaller codeblocks can aid the decoding of regions of an "
        "image and error resilience.' min='4' max='1024' default='64'/>"
"   <Option name='CODEBLOCK_HEIGHT' type='int' description="
        "'The size of the blocks of data coded with the arithmetic entropy "
        "coder may be set using these parameters. A codeblock may contain no "
        "more than  4096 (result of CODEBLOCK_WIDTH x CODEBLOCK_HEIGHT) "
        "samples. Smaller codeblocks can aid the decoding of regions of an "
        "image and error resilience.' min='4' max='1024' default='64'/>"
"   <Option name='ERROR_RESILIENCE' type='boolean' description="
        "'This option improves error resilient in JPEG2000 streams or for "
        "special codecs (e.g. hardware coder) for a faster compression/"
        "decompression. This option will increase the file size slightly when "
        "generating a code stream with the same image quality.' default='NO'/>"
"   <Option name='WRITE_METADATA' type='boolean' description="
        "'Whether metadata should be written, in a dedicated JP2 XML box' "
        "default='NO'/>"
"   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' description="
        "'(Only if WRITE_METADATA=YES) Whether only metadata from the main "
        "domain should be written' default='NO'/>"
"   <Option name='USE_SRC_CODESTREAM' type='boolean' description="
        "'When source dataset is JPEG2000, whether to reuse the codestream of "
        "the source dataset unmodified' default='NO'/>"
"   <Option name='NBITS' type='int' description="
        "'Bits (precision) for sub-byte files (1-7), sub-uint16 (9-15), "
        "sub-uint32 (17-28)'/>"
"</CreationOptionList>"  );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = JP2LuraDataset::Identify;
        poDriver->pfnOpen = JP2LuraDataset::Open;
        poDriver->pfnCreateCopy = JP2LuraDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

