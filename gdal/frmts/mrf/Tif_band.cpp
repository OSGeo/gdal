/*
* Copyright (c) 2002-2012, California Institute of Technology.
* All rights reserved.  Based on Government Sponsored Research under contracts NAS7-1407 and/or NAS7-03001.
* Redistribution and use in source and binary forms, with or without modification, are permitted provided
* that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice, this list of conditions and
*      the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*      the following disclaimer in the documentation and/or other materials provided with the distribution.
*   3. Neither the name of the California Institute of Technology (Caltech), its operating division the
*      Jet Propulsion Laboratory (JPL), the National Aeronautics and Space Administration (NASA),
*      nor the names of its contributors may be used to endorse or promote products derived from this software
*      without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE CALIFORNIA INSTITUTE OF TECHNOLOGY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Copyright 2014-2015 Esri
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/*
 * TIFF band
 * TIFF page compression and decompression functions
 *
 * Author:  Lucian Plesea, lplesea@esri.com
 *
 */

#include "marfa.h"

CPL_CVSID("$Id$")

NAMESPACE_MRF_START

// Returns a string in /vsimem/ + prefix + count that doesn't exist when this function gets called
// It is not thread safe, open the result as soon as possible
static CPLString uniq_memfname(const char *prefix)
{

// Define MRF_LOCAL_TMP to use local files instead of RAM
// #define MRF_LOCAL_TMP
#if defined(MRF_LOCAL_TMP)
    return CPLGenerateTempFilename(prefix);
#else
    CPLString fname;
    VSIStatBufL statb;
    static unsigned int cnt=0;
    do fname.Printf("/vsimem/%s_%08x",prefix, cnt++);
    while (!VSIStatL(fname, &statb));
    return fname;
#endif
}

//
// Uses GDAL to create a temporary TIF file, using the band create options
// copies the content to the destination buffer then erases the temp TIF
//
static CPLErr CompressTIF(buf_mgr &dst, buf_mgr &src, const ILImage &img, char **papszOptions)
{
    CPLErr ret;
    GDALDriver *poTiffDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    VSIStatBufL statb;
    CPLString fname = uniq_memfname("mrf_tif_write");

    GDALDataset *poTiff = poTiffDriver->Create(fname, img.pagesize.x, img.pagesize.y,
                                               img.pagesize.c, img.dt, papszOptions );

    // Read directly to avoid double caching in GDAL
    // Unfortunately not possible for multiple bands
    if (img.pagesize.c == 1) {
        ret = poTiff->GetRasterBand(1)->WriteBlock(0,0,src.buffer);
    } else {
        ret = poTiff->RasterIO(GF_Write, 0,0,img.pagesize.x,img.pagesize.y,
            src.buffer, img.pagesize.x, img.pagesize.y, img.dt, img.pagesize.c,
            NULL, 0,0,0
#if GDAL_VERSION_MAJOR >= 2
            ,NULL
#endif
            );
    }
    if (CE_None != ret) return ret;
    GDALClose(poTiff);

    // Check that we can read the file
    if (VSIStatL(fname, &statb))
    {
        CPLError(CE_Failure,CPLE_AppDefined,
            "MRF: TIFF, can't stat %s", fname.c_str());
        return CE_Failure;
    }

    if (size_t(statb.st_size) > dst.size)
    {
        CPLError(CE_Failure,CPLE_AppDefined,
            "MRF: TIFF, Tiff generated is too large");
        return CE_Failure;
    }

    VSILFILE *pf = VSIFOpenL(fname,"rb");
    if (pf == NULL)
    {
        CPLError(CE_Failure,CPLE_AppDefined,
            "MRF: TIFF, can't open %s", fname.c_str());
        return CE_Failure;
    }

    VSIFReadL(dst.buffer, static_cast<size_t>(statb.st_size), 1, pf);
    dst.size = static_cast<size_t>(statb.st_size);
    VSIFCloseL(pf);
    VSIUnlink(fname);

    return CE_None;
}

// Read from a RAM Tiff. This is rather generic
static CPLErr DecompressTIF(buf_mgr &dst, buf_mgr &src, const ILImage &img)
{
    CPLString fname = uniq_memfname("mrf_tif_read");
    VSILFILE *fp = VSIFileFromMemBuffer(fname, (GByte *)(src.buffer), src.size, false);
    // Comes back opened, but we can't use it
    if (fp)
        VSIFCloseL(fp);
    else {
        CPLError(CE_Failure,CPLE_AppDefined,
            "MRF: TIFF, can't open %s as a temp file", fname.c_str());
        return CE_Failure;
    }
#if GDAL_VERSION_MAJOR >= 2
    const char* const apszAllowedDrivers[] = { "GTiff", NULL };
    GDALDataset *poTiff = reinterpret_cast<GDALDataset*>(GDALOpenEx(fname, GDAL_OF_RASTER, apszAllowedDrivers, NULL, NULL));
#else
    GDALDataset *poTiff = reinterpret_cast<GDALDataset*>(GDALOpen(fname, GA_ReadOnly));
#endif
    if (poTiff == NULL) {
        CPLError(CE_Failure,CPLE_AppDefined,
            "MRF: TIFF, can't open page as a Tiff");
        VSIUnlink(fname);
        return CE_Failure;
    }
    int nBlockXSize, nBlockYSize;
    poTiff->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const GDALDataType eGTiffDT = poTiff->GetRasterBand(1)->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eGTiffDT);
    if( poTiff->GetRasterXSize() != img.pagesize.x ||
        poTiff->GetRasterYSize() != img.pagesize.y ||
        poTiff->GetRasterCount() != img.pagesize.c ||
        nBlockXSize != img.pagesize.x ||
        nBlockYSize != img.pagesize.y ||
        img.dt != eGTiffDT ||
        static_cast<vsi_l_offset>(nBlockXSize) * nBlockYSize * nDTSize * img.pagesize.c != dst.size )
    {
        CPLError(CE_Failure,CPLE_AppDefined,
            "MRF: TIFF inconsistant with MRF parameters");
        GDALClose(poTiff);
        VSIUnlink(fname);
        return CE_Failure;
    }

    CPLErr ret;
    // Bypass the GDAL caching
    if (img.pagesize.c == 1) {
        ret = poTiff->GetRasterBand(1)->ReadBlock(0,0,dst.buffer);
    } else {
        ret = poTiff->RasterIO(GF_Read,0,0,img.pagesize.x,img.pagesize.y,
            dst.buffer, img.pagesize.x, img.pagesize.y, img.dt, img.pagesize.c,
            NULL, 0,0,0
#if GDAL_VERSION_MAJOR >= 2
            ,NULL
#endif
            );
    }
    GDALClose(poTiff);
    VSIUnlink(fname);

    return ret;
}

CPLErr TIF_Band::Decompress(buf_mgr &dst, buf_mgr &src)
{
    return DecompressTIF(dst, src, img);
}

CPLErr TIF_Band::Compress(buf_mgr &dst, buf_mgr &src)
{
    return CompressTIF(dst,src,img, papszOptions);
}

TIF_Band::TIF_Band( GDALMRFDataset *pDS, const ILImage &image,
                    int b, int level ):
    GDALMRFRasterBand(pDS, image, b, int(level))
{
    // Increase the page buffer by 1K in case Tiff expands data
    pDS->SetPBufferSize(image.pageSizeBytes + 1024);

    // Static create options for TIFF tiles
    papszOptions = CSLAddNameValue(NULL, "COMPRESS", "DEFLATE");
    papszOptions = CSLAddNameValue(papszOptions, "TILED", "Yes");
    papszOptions = CSLAddNameValue(papszOptions, "BLOCKXSIZE",
                                   CPLString().Printf("%d",img.pagesize.x));
    papszOptions = CSLAddNameValue(papszOptions, "BLOCKYSIZE",
                                   CPLString().Printf("%d",img.pagesize.y));
    int q = img.quality / 10;
    // Move down so the default 85 maps to 6.  This makes the maz
    // ZLEVEL 8, which is OK.
    if (q >2) q-=2;
    papszOptions = CSLAddNameValue(papszOptions, "ZLEVEL",
                                   CPLString().Printf("%d", q));
}

TIF_Band::~TIF_Band()
{
    CSLDestroy(papszOptions);
}

NAMESPACE_MRF_END
