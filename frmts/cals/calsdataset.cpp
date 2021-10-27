/******************************************************************************
 *
 * Project:  CALS driver
 * Purpose:  CALS driver
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

#include "tiff.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                            CALSDataset                               */
/* ==================================================================== */
/************************************************************************/

class CALSDataset final: public GDALPamDataset
{
    friend class CALSRasterBand;

    CPLString    osTIFFHeaderFilename;
    CPLString    osSparseFilename;
    GDALDataset* poUnderlyingDS;

    static void WriteLEInt16( VSILFILE* fp, GInt16 nVal );
    static void WriteLEInt32( VSILFILE* fp, GInt32 nVal );
    static void WriteTIFFTAG( VSILFILE* fp, GInt16 nTagName, GInt16 nTagType,
                              GInt32 nTagValue );

  public:
                 CALSDataset() : poUnderlyingDS(nullptr) {}
                ~CALSDataset();

    static int          Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char *pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict,
                                    char **papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                          CALSRasterBand                              */
/* ==================================================================== */
/************************************************************************/

class CALSRasterBand final: public GDALPamRasterBand
{
    GDALRasterBand* poUnderlyingBand;

  public:
    explicit CALSRasterBand( CALSDataset* poDSIn )
    {
        poDS = poDSIn;
        poUnderlyingBand = poDSIn->poUnderlyingDS->GetRasterBand(1);
        poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        nBand = 1;
        eDataType = GDT_Byte;
    }

    CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void * pData ) override
    {
        return poUnderlyingBand->ReadBlock(nBlockXOff, nBlockYOff, pData);
    }

    CPLErr IRasterIO( GDALRWFlag eRWFlag,
                      int nXOff, int nYOff, int nXSize, int nYSize,
                      void * pData, int nBufXSize, int nBufYSize,
                      GDALDataType eBufType,
                      GSpacing nPixelSpace,
                      GSpacing nLineSpace,
                      GDALRasterIOExtraArg* psExtraArg ) override
    {
        return poUnderlyingBand->RasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nPixelSpace, nLineSpace, psExtraArg ) ;
    }

    GDALColorTable* GetColorTable() override
    {
        return poUnderlyingBand->GetColorTable();
    }

    GDALColorInterp GetColorInterpretation() override
    {
        return GCI_PaletteIndex;
    }

    char** GetMetadata(const char* pszDomain) override
    {
        return poUnderlyingBand->GetMetadata(pszDomain);
    }

    const char* GetMetadataItem( const char* pszKey,
                                 const char* pszDomain ) override
    {
        return poUnderlyingBand->GetMetadataItem(pszKey, pszDomain);
    }
};

/************************************************************************/
/* ==================================================================== */
/*                          CALSWrapperSrcBand                          */
/* ==================================================================== */
/************************************************************************/

class CALSWrapperSrcBand final: public GDALPamRasterBand
{
        GDALDataset* poSrcDS;
        bool bInvertValues;

    public:
        explicit CALSWrapperSrcBand( GDALDataset* poSrcDSIn )
        {
            poSrcDS = poSrcDSIn;
            SetMetadataItem("NBITS", "1", "IMAGE_STRUCTURE");
            poSrcDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
            eDataType = GDT_Byte;
            bInvertValues = true;
            GDALColorTable* poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
            if( poCT != nullptr && poCT->GetColorEntryCount() >= 2 )
            {
                const GDALColorEntry* psEntry1 = poCT->GetColorEntry(0);
                const GDALColorEntry* psEntry2 = poCT->GetColorEntry(1);
                if( psEntry1->c1 == 255 &&
                    psEntry1->c2 == 255 &&
                    psEntry1->c3 == 255 &&
                    psEntry2->c1 == 0 &&
                    psEntry2->c2 == 0 &&
                    psEntry2->c3 == 0 )
                {
                    bInvertValues = false;
                }
            }
        }

    CPLErr IReadBlock( int /* nBlockXOff */,
                       int /* nBlockYOff */,
                       void * /* pData */ ) override
    {
        // Should not be called.
        return CE_Failure;
    }

    CPLErr IRasterIO( GDALRWFlag eRWFlag,
                      int nXOff, int nYOff, int nXSize, int nYSize,
                      void * pData, int nBufXSize, int nBufYSize,
                      GDALDataType eBufType,
                      GSpacing nPixelSpace,
                      GSpacing nLineSpace,
                      GDALRasterIOExtraArg* psExtraArg ) override
    {
        const CPLErr eErr =
            poSrcDS->GetRasterBand(1)->RasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType,
                nPixelSpace, nLineSpace, psExtraArg ) ;
        if( bInvertValues )
        {
            for( int j = 0; j < nBufYSize; j++ )
            {
                for( int i = 0; i < nBufXSize; i++ )
                    ((GByte*)pData)[j * nLineSpace + i * nPixelSpace] =
                        1 - ((GByte*)pData)[j * nLineSpace +
                                            i * nPixelSpace];
            }
        }
        return eErr;
    }
};

/************************************************************************/
/* ==================================================================== */
/*                          CALSWrapperSrcDataset                       */
/* ==================================================================== */
/************************************************************************/

class CALSWrapperSrcDataset final: public GDALPamDataset
{
    public:
        CALSWrapperSrcDataset( GDALDataset* poSrcDS, const char* pszPadding )
        {
            nRasterXSize = poSrcDS->GetRasterXSize();
            nRasterYSize = poSrcDS->GetRasterYSize();
            SetBand(1, new CALSWrapperSrcBand(poSrcDS));
            SetMetadataItem("TIFFTAG_DOCUMENTNAME", pszPadding);
        }
};

/************************************************************************/
/* ==================================================================== */
/*                            CALSDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~CALSDataset()                            */
/************************************************************************/

CALSDataset::~CALSDataset()

{
    delete poUnderlyingDS;
    if( !osTIFFHeaderFilename.empty() )
        VSIUnlink(osTIFFHeaderFilename);
    if( !osSparseFilename.empty() )
        VSIUnlink(osSparseFilename);
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int CALSDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    // If in the ingested bytes we found neither srcdocid: or rtype: 1, give up
    if( poOpenInfo->nHeaderBytes == 0 ||
        (strstr( (const char*) poOpenInfo->pabyHeader, "srcdocid:") == nullptr &&
         strstr( (const char*) poOpenInfo->pabyHeader, "rtype: 1") == nullptr) )
        return FALSE;

    // If we found srcdocid: try to ingest up to 2048 bytes
    if( strstr( (const char*) poOpenInfo->pabyHeader, "srcdocid:") &&
        !poOpenInfo->TryToIngest(2048) )
        return FALSE;

    return strstr((const char*) poOpenInfo->pabyHeader, "rtype: 1") != nullptr &&
           strstr((const char*) poOpenInfo->pabyHeader, "rorient:") != nullptr &&
           strstr((const char*) poOpenInfo->pabyHeader, "rpelcnt:") != nullptr;
}

/************************************************************************/
/*                           WriteLEInt16()                             */
/************************************************************************/

void CALSDataset::WriteLEInt16( VSILFILE* fp, GInt16 nVal )
{
    CPL_LSBPTR16(&nVal);
    VSIFWriteL(&nVal, 1, 2, fp);
}

/************************************************************************/
/*                            WriteLEInt32()                            */
/************************************************************************/

void CALSDataset::WriteLEInt32( VSILFILE* fp, GInt32 nVal )
{
    CPL_LSBPTR32(&nVal);
    VSIFWriteL(&nVal, 1, 4, fp);
}

/************************************************************************/
/*                            WriteTIFFTAG()                            */
/************************************************************************/

void CALSDataset::WriteTIFFTAG( VSILFILE* fp, GInt16 nTagName, GInt16 nTagType,
                                GInt32 nTagValue )
{
    WriteLEInt16(fp, nTagName);
    WriteLEInt16(fp, nTagType);
    WriteLEInt32(fp, 1);
    WriteLEInt32(fp, nTagValue);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *CALSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr )
        return nullptr;

    const char* pszRPelCnt =
        strstr((const char*) poOpenInfo->pabyHeader, "rpelcnt:");
    int nXSize = 0;
    int nYSize = 0;
    if( sscanf(pszRPelCnt+strlen("rpelcnt:"),"%d,%d",&nXSize,&nYSize) != 2 ||
        nXSize <= 0 || nYSize <= 0 )
        return nullptr;

    const char* pszOrient =
        strstr((const char*) poOpenInfo->pabyHeader, "rorient:");
    int nAngle1, nAngle2;
    if( sscanf(pszOrient+strlen("rorient:"),"%d,%d",&nAngle1,&nAngle2) != 2 )
        return nullptr;

    const char* pszDensity =
        strstr((const char*) poOpenInfo->pabyHeader, "rdensty:");
    int nDensity = 0;
    if( pszDensity )
        sscanf(pszDensity+strlen("rdensty:"), "%d", &nDensity);

    VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
    int nFAX4BlobSize = static_cast<int>(VSIFTellL(poOpenInfo->fpL)) - 2048;
    if( nFAX4BlobSize < 0 )
        return nullptr;

    CALSDataset* poDS = new CALSDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

    // Create a TIFF header for a single-strip CCITTFAX4 file.
    poDS->osTIFFHeaderFilename = CPLSPrintf("/vsimem/cals/header_%p.tiff", poDS);
    VSILFILE* fp = VSIFOpenL(poDS->osTIFFHeaderFilename, "wb");
    const int nTagCount = 10;
    const int nHeaderSize = 4 + 4 + 2 + nTagCount * 12 + 4;
    WriteLEInt16(fp, TIFF_LITTLEENDIAN);  // TIFF little-endian signature.
    WriteLEInt16(fp, 42);  // TIFF classic.

    WriteLEInt32(fp, 8);  // Offset of IFD0.

    WriteLEInt16(fp, nTagCount);  // Number of entries.

    WriteTIFFTAG(fp, TIFFTAG_IMAGEWIDTH, TIFF_LONG, nXSize);
    WriteTIFFTAG(fp, TIFFTAG_IMAGELENGTH, TIFF_LONG, nYSize);
    WriteTIFFTAG(fp, TIFFTAG_BITSPERSAMPLE, TIFF_SHORT, 1);
    WriteTIFFTAG(fp, TIFFTAG_COMPRESSION, TIFF_SHORT, COMPRESSION_CCITTFAX4);
    WriteTIFFTAG(fp, TIFFTAG_PHOTOMETRIC, TIFF_SHORT, PHOTOMETRIC_MINISWHITE);
    WriteTIFFTAG(fp, TIFFTAG_STRIPOFFSETS, TIFF_LONG, nHeaderSize);
    WriteTIFFTAG(fp, TIFFTAG_SAMPLESPERPIXEL, TIFF_SHORT, 1);
    WriteTIFFTAG(fp, TIFFTAG_ROWSPERSTRIP, TIFF_LONG, nYSize);
    WriteTIFFTAG(fp, TIFFTAG_STRIPBYTECOUNTS, TIFF_LONG, nFAX4BlobSize);
    WriteTIFFTAG(fp, TIFFTAG_PLANARCONFIG, TIFF_SHORT, PLANARCONFIG_CONTIG);

    WriteLEInt32(fp, 0);  // Offset of next IFD.

    VSIFCloseL(fp);

    // Create a /vsisparse/ description file assembling the TIFF header
    // with the FAX4 codestream that starts at offset 2048 of the CALS file.
    poDS->osSparseFilename = CPLSPrintf("/vsimem/cals/sparse_%p.xml", poDS);
    fp = VSIFOpenL(poDS->osSparseFilename, "wb");
    CPLAssert(fp);
    VSIFPrintfL(fp, "<VSISparseFile>"
                    "<Length>%d</Length>"
                    "<SubfileRegion>"
                      "<Filename relative='0'>%s</Filename>"
                      "<DestinationOffset>0</DestinationOffset>"
                      "<SourceOffset>0</SourceOffset>"
                      "<RegionLength>%d</RegionLength>"
                    "</SubfileRegion>"
                    "<SubfileRegion>"
                      "<Filename relative='0'>%s</Filename>"
                      "<DestinationOffset>%d</DestinationOffset>"
                      "<SourceOffset>%d</SourceOffset>"
                      "<RegionLength>%d</RegionLength>"
                    "</SubfileRegion>"
                    "</VSISparseFile>",
                nHeaderSize + nFAX4BlobSize,
                poDS->osTIFFHeaderFilename.c_str(),
                nHeaderSize,
                poOpenInfo->pszFilename,
                nHeaderSize,
                2048,
                nFAX4BlobSize);
    VSIFCloseL(fp);

    poDS->poUnderlyingDS = (GDALDataset*) GDALOpenEx(
        CPLSPrintf("/vsisparse/%s", poDS->osSparseFilename.c_str()),
        GDAL_OF_RASTER | GDAL_OF_INTERNAL, nullptr, nullptr, nullptr);
    if( poDS->poUnderlyingDS == nullptr )
    {
        delete poDS;
        return nullptr;
    }

    if( nAngle1 != 0 || nAngle2 != 270 )
    {
        poDS->SetMetadataItem("PIXEL_PATH", CPLSPrintf("%d", nAngle1));
        poDS->SetMetadataItem("LINE_PROGRESSION", CPLSPrintf("%d", nAngle2));
    }

    if( nDensity != 0 )
    {
        poDS->SetMetadataItem("TIFFTAG_XRESOLUTION", CPLSPrintf("%d", nDensity));
        poDS->SetMetadataItem("TIFFTAG_YRESOLUTION", CPLSPrintf("%d", nDensity));
        poDS->SetMetadataItem("TIFFTAG_RESOLUTIONUNIT", "2 (pixels/inch)");
    }

    poDS->SetBand(1, new CALSRasterBand(poDS));

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML( poOpenInfo->GetSiblingFiles() );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
                                 poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *CALSDataset::CreateCopy( const char *pszFilename,
                                      GDALDataset *poSrcDS,
                                      int bStrict,
                                      char ** /* papszOptionsUnused */,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData )
{
    if( poSrcDS->GetRasterCount() == 0 ||
        (bStrict && poSrcDS->GetRasterCount() != 1) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "CALS driver only supports single band raster.");
        return nullptr;
    }
    if( poSrcDS->GetRasterBand(1)->
            GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == nullptr ||
        !EQUAL(poSrcDS->GetRasterBand(1)->
                   GetMetadataItem("NBITS", "IMAGE_STRUCTURE"), "1") )
    {
        CPLError( bStrict ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "CALS driver only supports 1-bit.");
        if( bStrict )
            return nullptr;
    }

    if( poSrcDS->GetRasterXSize() > 999999 ||
        poSrcDS->GetRasterYSize() > 999999 )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "CALS driver only supports datasets with dimension <= 999999.");
        return nullptr;
    }

    GDALDriver* poGTiffDrv =
        static_cast<GDALDriver *>(GDALGetDriverByName("GTiff"));
    if( poGTiffDrv == nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "CALS driver needs GTiff driver." );
        return nullptr;
    }

    // Write a in-memory TIFF with just the TIFF header to figure out
    // how large it will be.
    CPLString osTmpFilename(CPLSPrintf("/vsimem/cals/tmp_%p", poSrcDS));
    char** papszOptions = nullptr;
    papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "CCITTFAX4");
    papszOptions = CSLSetNameValue(papszOptions, "NBITS", "1");
    papszOptions = CSLSetNameValue(papszOptions, "BLOCKYSIZE",
                                   CPLSPrintf("%d", poSrcDS->GetRasterYSize()));
    papszOptions = CSLSetNameValue(papszOptions, "SPARSE_OK", "YES");
    GDALDataset* poDS = poGTiffDrv->Create(osTmpFilename,
                                           poSrcDS->GetRasterXSize(),
                                           poSrcDS->GetRasterYSize(),
                                           1, GDT_Byte,
                                           papszOptions);
    if( poDS == nullptr )
    {
        // Should not happen normally (except if CCITTFAX4 not available).
        CSLDestroy(papszOptions);
        return nullptr;
    }
    const char INITIAL_PADDING[] = "12345";
     // To adjust padding.
    poDS->SetMetadataItem("TIFFTAG_DOCUMENTNAME", INITIAL_PADDING);
    GDALClose(poDS);
    VSIStatBufL sStat;
    if( VSIStatL(osTmpFilename, &sStat) != 0 )
    {
        // Shouldn't happen really. Just to make Coverity happy.
        CSLDestroy(papszOptions);
        return nullptr;
    }
    int nTIFFHeaderSize = static_cast<int>(sStat.st_size);
    VSIUnlink(osTmpFilename);

    // Redo the same thing, but this time write it to the output file
    // and use a variable TIFF tag (TIFFTAG_DOCUMENTNAME) to enlarge the
    // header + the variable TIFF tag so that they are 2048 bytes large.
    char szBuffer[2048+1] = {};
    memset(szBuffer, 'X', 2048 - nTIFFHeaderSize + strlen(INITIAL_PADDING));
    szBuffer[2048 - nTIFFHeaderSize + strlen(INITIAL_PADDING)] = 0;
    GDALDataset* poTmpDS = new CALSWrapperSrcDataset(poSrcDS, szBuffer);
    poDS = poGTiffDrv->CreateCopy(pszFilename, poTmpDS, FALSE, papszOptions,
                                  pfnProgress, pProgressData );
    delete poTmpDS;
    CSLDestroy(papszOptions);
    if( poDS == nullptr )
        return nullptr;
    delete poDS;

    // Now replace the TIFF header by the CALS header.
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb+");
    if( fp == nullptr )
        return nullptr; // Shouldn't happen normally.
    memset(szBuffer, ' ', 2048);
    CPLString osField;
    osField = "srcdocid: NONE";
    // cppcheck-suppress redundantCopy
    memcpy(szBuffer, osField, osField.size());

    osField = "dstdocid: NONE";
    memcpy(szBuffer + 128, osField, osField.size());

    osField = "txtfilid: NONE";
    memcpy(szBuffer + 128*2, osField, osField.size());

    osField = "figid: NONE";
    memcpy(szBuffer + 128*3, osField, osField.size());

    osField = "srcgph: NONE";
    memcpy(szBuffer + 128*4, osField, osField.size());

    osField = "doccls: NONE";
    memcpy(szBuffer + 128*5, osField, osField.size());

    osField = "rtype: 1";
    memcpy(szBuffer + 128*6, osField, osField.size());

    int nAngle1 = 0;
    int nAngle2 = 270;
    const char* pszPixelPath = poSrcDS->GetMetadataItem("PIXEL_PATH");
    const char* pszLineProgression = poSrcDS->GetMetadataItem("LINE_PROGRESSION");
    if( pszPixelPath && pszLineProgression )
    {
        nAngle1 = atoi(pszPixelPath);
        nAngle2 = atoi(pszLineProgression);
    }
    osField = CPLSPrintf("rorient: %03d,%03d", nAngle1, nAngle2);
    memcpy(szBuffer + 128*7, osField, osField.size());

    osField = CPLSPrintf("rpelcnt: %06d,%06d",
                         poSrcDS->GetRasterXSize(),
                         poSrcDS->GetRasterYSize());
    memcpy(szBuffer + 128*8, osField, osField.size());

    int nDensity = 200;
    const char* pszXRes = poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION");
    const char* pszYRes = poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION");
    const char* pszResUnit = poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT");
    if( pszXRes && pszYRes && pszResUnit && EQUAL(pszXRes, pszYRes) &&
        atoi(pszResUnit) == 2 )
    {
        nDensity = atoi(pszXRes);
        if( nDensity < 1 || nDensity > 9999 )
            nDensity = 200;
    }
    osField = CPLSPrintf("rdensty: %04d", nDensity);
    memcpy(szBuffer + 128*9, osField, osField.size());

    osField = "notes: NONE";
    memcpy(szBuffer + 128*10, osField, osField.size());
    VSIFWriteL(szBuffer, 1, 2048, fp);
    VSIFCloseL(fp);

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly, nullptr);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                        GDALRegister_CALS()                           */
/************************************************************************/

void GDALRegister_CALS()

{
    if( GDALGetDriverByName( "CALS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "CALS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "CALS (Type 1)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/cals.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "cal ct1");

    poDriver->pfnIdentify = CALSDataset::Identify;
    poDriver->pfnOpen = CALSDataset::Open;
    poDriver->pfnCreateCopy = CALSDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
