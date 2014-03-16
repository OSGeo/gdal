/******************************************************************************
 * $Id: $
 *
 * Project:  DDS Driver
 * Purpose:  Implement GDAL DDS Support
 * Author:   Alan Boudreault, aboudreault@mapgears.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Alan Boudreault
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 ******************************************************************************
 *
 * THE CURRENT IMPLEMENTATION IS WRITE ONLY.
 * 
 */

#include "gdal_pam.h"
#include "crnlib.h"
#include "dds_defs.h"

CPL_CVSID("$Id: $");

CPL_C_START
void	GDALRegister_DDS(void);
CPL_C_END

using namespace crnlib;

enum { DDS_COLOR_TYPE_RGB,
       DDS_COLOR_TYPE_RGB_ALPHA };


/************************************************************************/
/* ==================================================================== */
/*				DDSDataset				*/
/* ==================================================================== */
/************************************************************************/

class DDSDataset : public GDALPamDataset
{
public:
    static GDALDataset* CreateCopy(const char * pszFilename,
                                   GDALDataset *poSrcDS,
                                   int bStrict, char ** papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData);
};


/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
DDSDataset::CreateCopy(const char * pszFilename, GDALDataset *poSrcDS, 
                       int bStrict, char ** papszOptions, 
                       GDALProgressFunc pfnProgress, void * pProgressData)

{  
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    
    /* -------------------------------------------------------------------- */
    /*      Some rudimentary checks                                         */
    /* -------------------------------------------------------------------- */
    if (nBands != 3 && nBands != 4)
    {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "DDS driver doesn't support %d bands. Must be 3 (rgb) \n"
                 "or 4 (rgba) bands.\n", 
                 nBands);
        
        return NULL;
    }

    if (poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "DDS driver doesn't support data type %s. "
                  "Only eight bit (Byte) bands supported. %s\n", 
                  GDALGetDataTypeName( 
                                      poSrcDS->GetRasterBand(1)->GetRasterDataType()),
                  (bStrict) ? "" : "Defaulting to Byte" );

        if (bStrict)
            return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Setup some parameters.                                          */
    /* -------------------------------------------------------------------- */
    int  nColorType = 0;

    if (nBands == 3)
      nColorType = DDS_COLOR_TYPE_RGB;
    else if (nBands == 4)
      nColorType = DDS_COLOR_TYPE_RGB_ALPHA;

    /* -------------------------------------------------------------------- */
    /*      Create the dataset.                                             */
    /* -------------------------------------------------------------------- */
    VSILFILE	*fpImage;

    fpImage = VSIFOpenL(pszFilename, "wb");
    if (fpImage == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "Unable to create dds file %s.\n", 
                 pszFilename);
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the Crunch compressor                                    */
    /* -------------------------------------------------------------------- */
    
    /* Default values */
    crn_format fmt = cCRNFmtDXT3; 
    const uint cDXTBlockSize = 4;    
    crn_dxt_quality dxt_quality = cCRNDXTQualityNormal;
    bool srgb_colorspace = true;    
    bool dxt1a_transparency = true;
    
    /* Check the texture format */
    const char *pszFormat = CSLFetchNameValue( papszOptions, "FORMAT" );

    if (pszFormat)
    {
        if (EQUAL(pszFormat, "dxt1"))
            fmt = cCRNFmtDXT1;
        else if (EQUAL(pszFormat, "dxt1a"))
            fmt = cCRNFmtDXT1;
        else if (EQUAL(pszFormat, "dxt3"))
            fmt = cCRNFmtDXT3;
        else if (EQUAL(pszFormat, "dxt5"))
            fmt = cCRNFmtDXT5;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal FORMAT value '%s', should be DXT1, DXT1A, DXT3 or DXT5.",
                      pszFormat );
            return NULL;
        }
    }

    /* Check the compression quality */
    const char *pszQuality = CSLFetchNameValue( papszOptions, "QUALITY" );

    if (pszQuality)
    {
        if (EQUAL(pszQuality, "SUPERFAST"))
            dxt_quality = cCRNDXTQualitySuperFast;            
        else if (EQUAL(pszQuality, "FAST"))
            dxt_quality = cCRNDXTQualityFast;
        else if (EQUAL(pszQuality, "NORMAL"))
            dxt_quality = cCRNDXTQualityNormal;
        else if (EQUAL(pszQuality, "BETTER"))
            dxt_quality = cCRNDXTQualityBetter;
        else if (EQUAL(pszQuality, "UBER"))
            dxt_quality = cCRNDXTQualityUber;        
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal QUALITY value '%s', should be SUPERFAST, FAST, NORMAL, BETTER or UBER.",
                      pszQuality );
            return NULL;
        }
    }

    if ((nXSize%4!=0) || (nYSize%4!=0)) {
      CPLError(CE_Warning, CPLE_AppDefined,
               "Raster size is not a multiple of 4: %dx%d. "
               "Extra rows/colums will be ignored during the compression.",
               nXSize, nYSize);
    }
    
    crn_comp_params comp_params;
    comp_params.m_format = fmt;
    comp_params.m_dxt_quality = dxt_quality;
    comp_params.set_flag(cCRNCompFlagPerceptual, srgb_colorspace);
    comp_params.set_flag(cCRNCompFlagDXT1AForTransparency, dxt1a_transparency);
    
    crn_block_compressor_context_t pContext = crn_create_block_compressor(comp_params);
    
    /* -------------------------------------------------------------------- */
    /*      Write the DDS header to the file.                               */
    /* -------------------------------------------------------------------- */

    VSIFWriteL(&crnlib::cDDSFileSignature, 1,
               sizeof(crnlib::cDDSFileSignature), fpImage);
    
    crnlib::DDSURFACEDESC2 ddsDesc;
    memset(&ddsDesc, 0, sizeof(ddsDesc));
    ddsDesc.dwSize = sizeof(ddsDesc);
    ddsDesc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    ddsDesc.dwWidth = nXSize;
    ddsDesc.dwHeight = nYSize;
    
    ddsDesc.ddpfPixelFormat.dwSize = sizeof(crnlib::DDPIXELFORMAT);
    ddsDesc.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    ddsDesc.ddpfPixelFormat.dwFourCC = crn_get_format_fourcc(fmt);
    ddsDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    // Set pitch/linearsize field (some DDS readers require this field to be non-zero).
    uint bits_per_pixel = crn_get_format_bits_per_texel(fmt);
    ddsDesc.lPitch = (((ddsDesc.dwWidth + 3) & ~3) * ((ddsDesc.dwHeight + 3) & ~3) * bits_per_pixel) >> 3;
    ddsDesc.dwFlags |= DDSD_LINEARSIZE;

    /* Endianness problems when serializing structure?? dds on-disk format
       should be verified */
    VSIFWriteL(&ddsDesc, 1, sizeof(ddsDesc), fpImage);
    
    /* -------------------------------------------------------------------- */
    /*      Loop over image, compressing image data.                        */
    /* -------------------------------------------------------------------- */
    const uint bytesPerBlock = crn_get_bytes_per_dxt_block(fmt);
    CPLErr eErr = CE_None;
    const uint nYNumBlocks = (nYSize + cDXTBlockSize - 1) / cDXTBlockSize;  
    const uint num_blocks_x = (nXSize + cDXTBlockSize - 1) / cDXTBlockSize;
    const uint total_compressed_size = num_blocks_x * bytesPerBlock;

    void *pCompressed_data = CPLMalloc(total_compressed_size);
    GByte* pabyScanlines = (GByte *) CPLMalloc(nBands * nXSize * cDXTBlockSize);
    crn_uint32 *pixels = (crn_uint32*) CPLMalloc(sizeof(crn_uint32)*cDXTBlockSize * cDXTBlockSize);
    crn_uint32 *src_image = NULL;
    if (nColorType == DDS_COLOR_TYPE_RGB)
        src_image = (crn_uint32*) CPLMalloc(sizeof(crn_uint32)*nXSize*cDXTBlockSize);

    for (uint iLine = 0; iLine < nYNumBlocks && eErr == CE_None; iLine++)
    {
        const uint size_y = (iLine*cDXTBlockSize+cDXTBlockSize) < (uint)nYSize ?
                           cDXTBlockSize : (cDXTBlockSize-((iLine*cDXTBlockSize+cDXTBlockSize)-(uint)nYSize));
        
        eErr = poSrcDS->RasterIO(GF_Read, 0, iLine*cDXTBlockSize, nXSize, size_y, 
                                 pabyScanlines, nXSize, size_y, GDT_Byte,
                                 nBands, NULL,
                                 nBands, 
                                 nBands * nXSize, 1);

        if (eErr != CE_None)
            break;
        
        crn_uint32 *pSrc_image = NULL;
        if (nColorType == DDS_COLOR_TYPE_RGB_ALPHA)
            pSrc_image = (crn_uint32*)pabyScanlines;
        else if (nColorType == DDS_COLOR_TYPE_RGB)
        { /* crunch needs 32bits integers */
            int nPixels = nXSize*cDXTBlockSize;
            for (int i=0; i<nPixels;++i)
            {
                int y = (i*3);
                src_image[i] = (255<<24) | (pabyScanlines[y+2]<<16) | (pabyScanlines[y+1]<<8) |
                  pabyScanlines[y];            
            }
            
            pSrc_image = &(src_image[0]);
        }

        for (crn_uint32 block_x = 0; block_x < num_blocks_x; block_x++)
        {
            // Exact block from image, clamping at the sides of non-divisible by
            // 4 images to avoid artifacts.
            crn_uint32 *pDst_pixels = pixels;
            for (uint y = 0; y < cDXTBlockSize; y++)
            {
                const uint actual_y = MIN(cDXTBlockSize - 1U, y);
                for (uint x = 0; x < cDXTBlockSize; x++)
                {
                    const uint actual_x = MIN(nXSize - 1U, (block_x * cDXTBlockSize) + x);
                    *pDst_pixels++ = pSrc_image[actual_x + actual_y * nXSize];
                }
            }
            
            // Compress the DXTn block.
            crn_compress_block(pContext, pixels, static_cast<crn_uint8 *>(pCompressed_data) + block_x * bytesPerBlock);
        }

        if (eErr == CE_None)
            VSIFWriteL(pCompressed_data, 1, total_compressed_size, fpImage);

        if (eErr == CE_None
            && !pfnProgress( (iLine+1) / (double) nYNumBlocks,
                             NULL, pProgressData))
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_UserInterrupt,
                      "User terminated CreateCopy()");
        }

    }

    CPLFree(src_image);
    CPLFree(pixels);
    CPLFree(pCompressed_data);
    CPLFree(pabyScanlines);
    crn_free_block_compressor(pContext);
    pContext = NULL;
   
    VSIFCloseL(fpImage);

    if (eErr != CE_None)
        return NULL;

    DDSDataset *poDsDummy = new DDSDataset();
    
    return poDsDummy;
}

/************************************************************************/
/*                          GDALRegister_DDS()                          */
/************************************************************************/

void GDALRegister_DDS()
{
    GDALDriver	*poDriver;

    if (GDALGetDriverByName( "DDS" ) == NULL)
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription("DDS");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, 
                                  "DirectDraw Surface");
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#DDS" );        
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dds");
        poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/dds");

        poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST, 
                                  "<CreationOptionList>\n"
                                  "   <Option name='FORMAT' type='string-select' description='Texture format' default='DXT3'>\n"
                                  "     <Value>DXT1</Value>\n"
                                  "     <Value>DXT1A</Value>\n"
                                  "     <Value>DXT3</Value>\n"
                                  "     <Value>DXT5</Value>\n"                                                                    
                                  "   </Option>\n"
                                  "   <Option name='QUALITY' type='string-select' description='Compression Quality' default='NORMAL'>\n"
                                  "     <Value>SUPERFAST</Value>\n"
                                  "     <Value>FAST</Value>\n"
                                  "     <Value>NORMAL</Value>\n"
                                  "     <Value>BETTER</Value>\n"
                                  "     <Value>UBER</Value>\n"                                                                                                      
                                  "   </Option>\n"                                  
                                  "</CreationOptionList>\n" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        
        poDriver->pfnCreateCopy = DDSDataset::CreateCopy;
        
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}
