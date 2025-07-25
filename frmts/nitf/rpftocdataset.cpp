/******************************************************************************
 *
 * Project:  RPF TOC read Translator
 * Purpose:  Implementation of RPFTOCDataset and RPFTOCSubDataset.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "rpftoclib.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "ogr_spatialref.h"
#include "nitflib.h"
#include "vrtdataset.h"
#include "nitfdrivercore.h"

constexpr int GEOTRSFRM_TOPLEFT_X = 0;
constexpr int GEOTRSFRM_WE_RES = 1;
constexpr int GEOTRSFRM_ROTATION_PARAM1 = 2;
constexpr int GEOTRSFRM_TOPLEFT_Y = 3;
constexpr int GEOTRSFRM_ROTATION_PARAM2 = 4;
constexpr int GEOTRSFRM_NS_RES = 5;

/** Overview of used classes :
   - RPFTOCDataset : lists the different subdatasets, listed in the A.TOC,
                     as subdatasets
   - RPFTOCSubDataset : one of these subdatasets, implemented as a VRT, of
                        the relevant NITF tiles
   - RPFTOCProxyRasterDataSet : a "proxy" dataset that maps to a NITF tile
   - RPFTOCProxyRasterBandPalette / RPFTOCProxyRasterBandRGBA : bands of
   RPFTOCProxyRasterDataSet
*/

/************************************************************************/
/* ==================================================================== */
/*                            RPFTOCDataset                             */
/* ==================================================================== */
/************************************************************************/

class RPFTOCDataset final : public GDALPamDataset
{
    char **papszSubDatasets = nullptr;
    OGRSpatialReference m_oSRS{};
    int bGotGeoTransform = false;
    GDALGeoTransform m_gt{};
    char **papszFileList = nullptr;
    CPL_DISALLOW_COPY_ASSIGN(RPFTOCDataset)

  public:
    RPFTOCDataset()
    {
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    virtual ~RPFTOCDataset()
    {
        CSLDestroy(papszSubDatasets);
        CSLDestroy(papszFileList);
    }

    virtual char **GetMetadata(const char *pszDomain = "") override;

    virtual char **GetFileList() override
    {
        return CSLDuplicate(papszFileList);
    }

    void AddSubDataset(const char *pszFilename, RPFTocEntry *tocEntry);

    void SetSize(int rasterXSize, int rasterYSize)
    {
        nRasterXSize = rasterXSize;
        nRasterYSize = rasterYSize;
    }

    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        if (bGotGeoTransform)
        {
            gt = m_gt;
            return CE_None;
        }
        return CE_Failure;
    }

    virtual CPLErr SetGeoTransform(const GDALGeoTransform &gt) override
    {
        bGotGeoTransform = TRUE;
        m_gt = gt;
        return CE_None;
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override
    {
        m_oSRS.Clear();
        if (poSRS)
            m_oSRS = *poSRS;
        return CE_None;
    }

    static int IsNITFFileTOC(NITFFile *psFile);
    static GDALDataset *OpenFileTOC(NITFFile *psFile, const char *pszFilename,
                                    const char *entryName,
                                    const char *openInformationName);

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
};

/************************************************************************/
/* ==================================================================== */
/*                            RPFTOCSubDataset                          */
/* ==================================================================== */
/************************************************************************/

class RPFTOCSubDataset final : public VRTDataset
{
    int cachedTileBlockXOff = -1;
    int cachedTileBlockYOff = -1;
    void *cachedTileData = nullptr;
    int cachedTileDataSize = 0;
    const char *cachedTileFileName = nullptr;
    char **papszFileList = nullptr;
    CPL_DISALLOW_COPY_ASSIGN(RPFTOCSubDataset)

  public:
    RPFTOCSubDataset(int nXSize, int nYSize) : VRTDataset(nXSize, nYSize)
    {
        /* Don't try to write a VRT file */
        SetWritable(FALSE);

        /* The driver is set to VRT in VRTDataset constructor. */
        /* We have to set it to the expected value ! */
        poDriver = GDALDriver::FromHandle(GDALGetDriverByName("RPFTOC"));
    }

    ~RPFTOCSubDataset() override;

    virtual char **GetFileList() override
    {
        return CSLDuplicate(papszFileList);
    }

    void *GetCachedTile(const char *tileFileName, int nBlockXOff,
                        int nBlockYOff)
    {
        if (cachedTileFileName == tileFileName &&
            cachedTileBlockXOff == nBlockXOff &&
            cachedTileBlockYOff == nBlockYOff)
        {
            return cachedTileData;
        }

        return nullptr;
    }

    void SetCachedTile(const char *tileFileName, int nBlockXOff, int nBlockYOff,
                       const void *pData, int dataSize)
    {
        if (cachedTileData == nullptr || dataSize > cachedTileDataSize)
        {
            cachedTileData = CPLRealloc(cachedTileData, dataSize);
            cachedTileDataSize = dataSize;
        }
        memcpy(cachedTileData, pData, dataSize);
        cachedTileFileName = tileFileName;
        cachedTileBlockXOff = nBlockXOff;
        cachedTileBlockYOff = nBlockYOff;
    }

    static GDALDataset *CreateDataSetFromTocEntry(
        const char *openInformationName, const char *pszTOCFileName, int nEntry,
        const RPFTocEntry *entry, int isRGBA, char **papszMetadataRPFTOCFile);
};

RPFTOCSubDataset::~RPFTOCSubDataset()
{
    CSLDestroy(papszFileList);
    CPLFree(cachedTileData);
}

/************************************************************************/
/* ==================================================================== */
/*                        RPFTOCProxyRasterDataSet                       */
/* ==================================================================== */
/************************************************************************/

class RPFTOCProxyRasterDataSet final : public GDALProxyPoolDataset
{
    /* The following parameters are only for sanity checking */
    bool checkDone = false;
    bool checkOK = false;
    const double nwLong;
    const double nwLat;
    GDALColorTable *colorTableRef = nullptr;
    int bHasNoDataValue = false;
    double noDataValue = 0;
    RPFTOCSubDataset *const subdataset;

    CPL_DISALLOW_COPY_ASSIGN(RPFTOCProxyRasterDataSet)

  public:
    RPFTOCProxyRasterDataSet(RPFTOCSubDataset *subdataset, const char *fileName,
                             int nRasterXSize, int nRasterYSize,
                             int nBlockXSize, int nBlockYSize,
                             const char *projectionRef, double nwLong,
                             double nwLat, int nBands);

    void SetNoDataValue(double noDataValueIn)
    {
        this->noDataValue = noDataValueIn;
        bHasNoDataValue = TRUE;
    }

    double GetNoDataValue(int *pbHasNoDataValue)
    {
        if (pbHasNoDataValue)
            *pbHasNoDataValue = this->bHasNoDataValue;
        return noDataValue;
    }

    GDALDataset *RefUnderlyingDataset() const override;

    void UnrefUnderlyingDataset(GDALDataset *poUnderlyingDataset) const override
    {
        GDALProxyPoolDataset::UnrefUnderlyingDataset(poUnderlyingDataset);
    }

    void SetReferenceColorTable(GDALColorTable *colorTableRefIn)
    {
        this->colorTableRef = colorTableRefIn;
    }

    const GDALColorTable *GetReferenceColorTable() const
    {
        return colorTableRef;
    }

    int SanityCheckOK(GDALDataset *sourceDS);

    RPFTOCSubDataset *GetSubDataset()
    {
        return subdataset;
    }
};

GDALDataset *RPFTOCProxyRasterDataSet::RefUnderlyingDataset() const
{
    return GDALProxyPoolDataset::RefUnderlyingDataset();
}

/************************************************************************/
/* ==================================================================== */
/*                     RPFTOCProxyRasterBandRGBA                        */
/* ==================================================================== */
/************************************************************************/

class RPFTOCProxyRasterBandRGBA final : public GDALPamRasterBand
{
    bool initDone = false;
    std::array<unsigned char, 256> colorTable = {0};
    int blockByteSize = 0;

  private:
    void Expand(void *pImage, const void *srcImage);

  public:
    RPFTOCProxyRasterBandRGBA(GDALProxyPoolDataset *poDSIn, int nBandIn,
                              int nBlockXSizeIn, int nBlockYSizeIn)
    {
        this->poDS = poDSIn;
        nRasterXSize = poDSIn->GetRasterXSize();
        nRasterYSize = poDSIn->GetRasterYSize();
        this->nBlockXSize = nBlockXSizeIn;
        this->nBlockYSize = nBlockYSizeIn;
        eDataType = GDT_Byte;
        this->nBand = nBandIn;
        blockByteSize = nBlockXSize * nBlockYSize;
    }

    virtual GDALColorInterp GetColorInterpretation() override
    {
        return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
    }

  protected:
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff,
                              void *pImage) override;
};

/************************************************************************/
/*                    Expand()                                          */
/************************************************************************/

/* Expand the  array or indexed colors to an array of their corresponding R,G,B
 * or A component */
void RPFTOCProxyRasterBandRGBA::Expand(void *pImage, const void *srcImage)
{
    // pImage might be equal to srcImage

    if ((blockByteSize & (~3)) != 0)
    {
        for (int i = 0; i < blockByteSize; i++)
        {
            static_cast<unsigned char *>(pImage)[i] =
                colorTable[static_cast<const unsigned char *>(srcImage)[i]];
        }
    }
    else
    {
        const int nIter = blockByteSize / 4;
        for (int i = 0; i < nIter; i++)
        {
            const unsigned int four_pixels =
                static_cast<const unsigned int *>(srcImage)[i];
            static_cast<unsigned int *>(pImage)[i] =
                (colorTable[four_pixels >> 24] << 24) |
                (colorTable[(four_pixels >> 16) & 0xFF] << 16) |
                (colorTable[(four_pixels >> 8) & 0xFF] << 8) |
                colorTable[four_pixels & 0xFF];
        }
    }
}

/************************************************************************/
/*                    IReadBlock()                                      */
/************************************************************************/

CPLErr RPFTOCProxyRasterBandRGBA::IReadBlock(int nBlockXOff, int nBlockYOff,
                                             void *pImage)
{
    CPLErr ret;
    RPFTOCProxyRasterDataSet *proxyDS =
        reinterpret_cast<RPFTOCProxyRasterDataSet *>(poDS);

    GDALDataset *ds = proxyDS->RefUnderlyingDataset();
    if (ds)
    {
        if (proxyDS->SanityCheckOK(ds) == FALSE)
        {
            proxyDS->UnrefUnderlyingDataset(ds);
            return CE_Failure;
        }

        GDALRasterBand *srcBand = ds->GetRasterBand(1);
        if (initDone == FALSE)
        {
            GDALColorTable *srcColorTable = srcBand->GetColorTable();
            int bHasNoDataValue;
            int noDataValue =
                static_cast<int>(srcBand->GetNoDataValue(&bHasNoDataValue));
            const int nEntries = srcColorTable->GetColorEntryCount();
            for (int i = 0; i < nEntries; i++)
            {
                const GDALColorEntry *entry = srcColorTable->GetColorEntry(i);
                if (nBand == 1)
                    colorTable[i] = static_cast<unsigned char>(entry->c1);
                else if (nBand == 2)
                    colorTable[i] = static_cast<unsigned char>(entry->c2);
                else if (nBand == 3)
                    colorTable[i] = static_cast<unsigned char>(entry->c3);
                else
                {
                    colorTable[i] = (bHasNoDataValue && i == noDataValue)
                                        ? 0
                                        : static_cast<unsigned char>(entry->c4);
                }
            }
            if (bHasNoDataValue && nEntries == noDataValue)
                colorTable[nEntries] = 0;
            initDone = TRUE;
        }

        /* We use a 1-tile cache as the same source tile will be consecutively
         * asked for */
        /* computing the R tile, the G tile, the B tile and the A tile */
        void *cachedImage = proxyDS->GetSubDataset()->GetCachedTile(
            GetDescription(), nBlockXOff, nBlockYOff);
        if (cachedImage == nullptr)
        {
            CPLDebug("RPFTOC", "Read (%d, %d) of band %d, of file %s",
                     nBlockXOff, nBlockYOff, nBand, GetDescription());
            ret = srcBand->ReadBlock(nBlockXOff, nBlockYOff, pImage);
            if (ret == CE_None)
            {
                proxyDS->GetSubDataset()->SetCachedTile(GetDescription(),
                                                        nBlockXOff, nBlockYOff,
                                                        pImage, blockByteSize);
                Expand(pImage, pImage);
            }

            /* -------------------------------------------------------------- */
            /*  Forcibly load the other bands associated with this scanline.  */
            /* -------------------------------------------------------------- */
            if (nBand == 1)
            {
                GDALRasterBlock *poBlock =
                    poDS->GetRasterBand(2)->GetLockedBlockRef(nBlockXOff,
                                                              nBlockYOff);
                if (poBlock)
                    poBlock->DropLock();

                poBlock = poDS->GetRasterBand(3)->GetLockedBlockRef(nBlockXOff,
                                                                    nBlockYOff);
                if (poBlock)
                    poBlock->DropLock();

                poBlock = poDS->GetRasterBand(4)->GetLockedBlockRef(nBlockXOff,
                                                                    nBlockYOff);
                if (poBlock)
                    poBlock->DropLock();
            }
        }
        else
        {
            Expand(pImage, cachedImage);
            ret = CE_None;
        }
    }
    else
    {
        ret = CE_Failure;
    }

    proxyDS->UnrefUnderlyingDataset(ds);

    return ret;
}

/************************************************************************/
/* ==================================================================== */
/*                 RPFTOCProxyRasterBandPalette                         */
/* ==================================================================== */
/************************************************************************/

class RPFTOCProxyRasterBandPalette final : public GDALPamRasterBand
{
    int initDone;
    int blockByteSize;
    int samePalette;
    unsigned char remapLUT[256];

  public:
    RPFTOCProxyRasterBandPalette(GDALProxyPoolDataset *poDSIn, int nBandIn,
                                 int nBlockXSizeIn, int nBlockYSizeIn)
        : initDone(FALSE), blockByteSize(nBlockXSizeIn * nBlockYSizeIn),
          samePalette(0)
    {
        this->poDS = poDSIn;
        nRasterXSize = poDSIn->GetRasterXSize();
        nRasterYSize = poDSIn->GetRasterYSize();
        this->nBlockXSize = nBlockXSizeIn;
        this->nBlockYSize = nBlockYSizeIn;
        eDataType = GDT_Byte;
        this->nBand = nBandIn;
        memset(remapLUT, 0, sizeof(remapLUT));
    }

    virtual GDALColorInterp GetColorInterpretation() override
    {
        return GCI_PaletteIndex;
    }

    virtual double GetNoDataValue(int *bHasNoDataValue) override
    {
        return (reinterpret_cast<RPFTOCProxyRasterDataSet *>(poDS))
            ->GetNoDataValue(bHasNoDataValue);
    }

    virtual GDALColorTable *GetColorTable() override
    {
        // TODO: This casting is a bit scary.
        return const_cast<GDALColorTable *>(
            reinterpret_cast<RPFTOCProxyRasterDataSet *>(poDS)
                ->GetReferenceColorTable());
    }

  protected:
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff,
                              void *pImage) override;
};

/************************************************************************/
/*                    IReadBlock()                                      */
/************************************************************************/

CPLErr RPFTOCProxyRasterBandPalette::IReadBlock(int nBlockXOff, int nBlockYOff,
                                                void *pImage)
{
    CPLErr ret;
    RPFTOCProxyRasterDataSet *proxyDS =
        reinterpret_cast<RPFTOCProxyRasterDataSet *>(poDS);
    GDALDataset *ds = proxyDS->RefUnderlyingDataset();
    if (ds)
    {
        if (proxyDS->SanityCheckOK(ds) == FALSE)
        {
            proxyDS->UnrefUnderlyingDataset(ds);
            return CE_Failure;
        }

        GDALRasterBand *srcBand = ds->GetRasterBand(1);
        ret = srcBand->ReadBlock(nBlockXOff, nBlockYOff, pImage);

        if (initDone == FALSE)
        {
            int approximateMatching;
            if (srcBand->GetIndexColorTranslationTo(this, remapLUT,
                                                    &approximateMatching))
            {
                samePalette = FALSE;
                if (approximateMatching)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Palette for %s is different from reference palette. "
                        "Coudln't remap exactly all colors. Trying to find "
                        "closest matches.\n",
                        GetDescription());
                }
            }
            else
            {
                samePalette = TRUE;
            }
            initDone = TRUE;
        }

        if (samePalette == FALSE)
        {
            unsigned char *data = static_cast<unsigned char *>(pImage);
            for (int i = 0; i < blockByteSize; i++)
            {
                data[i] = remapLUT[data[i]];
            }
        }
    }
    else
    {
        ret = CE_Failure;
    }

    proxyDS->UnrefUnderlyingDataset(ds);

    return ret;
}

/************************************************************************/
/*                    RPFTOCProxyRasterDataSet()                         */
/************************************************************************/

RPFTOCProxyRasterDataSet::RPFTOCProxyRasterDataSet(
    RPFTOCSubDataset *subdatasetIn, const char *fileNameIn, int nRasterXSizeIn,
    int nRasterYSizeIn, int nBlockXSizeIn, int nBlockYSizeIn,
    const char *projectionRefIn, double nwLongIn, double nwLatIn, int nBandsIn)
    :  // Mark as shared since the VRT will take several references if we are in
       // RGBA mode (4 bands for this dataset).
      GDALProxyPoolDataset(fileNameIn, nRasterXSizeIn, nRasterYSizeIn,
                           GA_ReadOnly, TRUE, projectionRefIn),
      nwLong(nwLongIn), nwLat(nwLatIn), subdataset(subdatasetIn)
{
    if (nBandsIn == 4)
    {
        for (int i = 0; i < 4; i++)
        {
            SetBand(i + 1, new RPFTOCProxyRasterBandRGBA(
                               this, i + 1, nBlockXSizeIn, nBlockYSizeIn));
        }
    }
    else
    {
        SetBand(1, new RPFTOCProxyRasterBandPalette(this, 1, nBlockXSizeIn,
                                                    nBlockYSizeIn));
    }
}

/************************************************************************/
/*                    SanityCheckOK()                                   */
/************************************************************************/

#define WARN_ON_FAIL(x)                                                        \
    do                                                                         \
    {                                                                          \
        if (!(x))                                                              \
        {                                                                      \
            CPLError(CE_Warning, CPLE_AppDefined,                              \
                     "For %s, assert '" #x "' failed", GetDescription());      \
        }                                                                      \
    } while (false)
#define ERROR_ON_FAIL(x)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(x))                                                              \
        {                                                                      \
            CPLError(CE_Warning, CPLE_AppDefined,                              \
                     "For %s, assert '" #x "' failed", GetDescription());      \
            checkOK = FALSE;                                                   \
        }                                                                      \
    } while (false)

int RPFTOCProxyRasterDataSet::SanityCheckOK(GDALDataset *sourceDS)
{
    if (checkDone)
        return checkOK;

    int src_nBlockXSize;
    int src_nBlockYSize;
    int nBlockXSize;
    int nBlockYSize;
    GDALGeoTransform l_gt;

    checkOK = TRUE;
    checkDone = TRUE;

    sourceDS->GetGeoTransform(l_gt);
    WARN_ON_FAIL(fabs(l_gt[GEOTRSFRM_TOPLEFT_X] - nwLong) < l_gt[1]);
    WARN_ON_FAIL(fabs(l_gt[GEOTRSFRM_TOPLEFT_Y] - nwLat) < fabs(l_gt[5]));
    WARN_ON_FAIL(l_gt[GEOTRSFRM_ROTATION_PARAM1] == 0 &&
                 l_gt[GEOTRSFRM_ROTATION_PARAM2] == 0); /* No rotation */
    ERROR_ON_FAIL(sourceDS->GetRasterCount() == 1);     /* Just 1 band */
    ERROR_ON_FAIL(sourceDS->GetRasterXSize() == nRasterXSize);
    ERROR_ON_FAIL(sourceDS->GetRasterYSize() == nRasterYSize);
    WARN_ON_FAIL(EQUAL(sourceDS->GetProjectionRef(), GetProjectionRef()));
    sourceDS->GetRasterBand(1)->GetBlockSize(&src_nBlockXSize,
                                             &src_nBlockYSize);
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    ERROR_ON_FAIL(src_nBlockXSize == nBlockXSize);
    ERROR_ON_FAIL(src_nBlockYSize == nBlockYSize);
    WARN_ON_FAIL(sourceDS->GetRasterBand(1)->GetColorInterpretation() ==
                 GCI_PaletteIndex);
    WARN_ON_FAIL(sourceDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte);

    return checkOK;
}

/************************************************************************/
/*                           MakeTOCEntryName()                         */
/************************************************************************/

static const char *MakeTOCEntryName(RPFTocEntry *tocEntry)
{
    char *str = nullptr;
    if (tocEntry->seriesAbbreviation)
        str = const_cast<char *>(CPLSPrintf(
            "%s_%s_%s_%s_%d", tocEntry->type, tocEntry->seriesAbbreviation,
            tocEntry->scale, tocEntry->zone, tocEntry->boundaryId));
    else
        str = const_cast<char *>(CPLSPrintf("%s_%s_%s_%d", tocEntry->type,
                                            tocEntry->scale, tocEntry->zone,
                                            tocEntry->boundaryId));
    char *c = str;
    while (*c)
    {
        if (*c == ':' || *c == ' ')
            *c = '_';
        c++;
    }
    return str;
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void RPFTOCDataset::AddSubDataset(const char *pszFilename,
                                  RPFTocEntry *tocEntry)

{
    char szName[80];
    const int nCount = CSLCount(papszSubDatasets) / 2;

    snprintf(szName, sizeof(szName), "SUBDATASET_%d_NAME", nCount + 1);
    papszSubDatasets =
        CSLSetNameValue(papszSubDatasets, szName,
                        CPLSPrintf("NITF_TOC_ENTRY:%s:%s",
                                   MakeTOCEntryName(tocEntry), pszFilename));

    snprintf(szName, sizeof(szName), "SUBDATASET_%d_DESC", nCount + 1);
    if (tocEntry->seriesName && tocEntry->seriesAbbreviation)
        papszSubDatasets = CSLSetNameValue(
            papszSubDatasets, szName,
            CPLSPrintf("%s:%s:%s:%s:%s:%d", tocEntry->type,
                       tocEntry->seriesAbbreviation, tocEntry->seriesName,
                       tocEntry->scale, tocEntry->zone, tocEntry->boundaryId));
    else
        papszSubDatasets = CSLSetNameValue(
            papszSubDatasets, szName,
            CPLSPrintf("%s:%s:%s:%d", tocEntry->type, tocEntry->scale,
                       tocEntry->zone, tocEntry->boundaryId));
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **RPFTOCDataset::GetMetadata(const char *pszDomain)

{
    if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                  NITFCreateVRTDataSetFromTocEntry()                  */
/************************************************************************/

#define ASSERT_CREATE_VRT(x)                                                   \
    do                                                                         \
    {                                                                          \
        if (!(x))                                                              \
        {                                                                      \
            CPLError(CE_Failure, CPLE_AppDefined,                              \
                     "For %s, assert '" #x "' failed",                         \
                     entry->frameEntries[i].fullFilePath);                     \
            if (poSrcDS)                                                       \
                GDALClose(poSrcDS);                                            \
            CPLFree(projectionRef);                                            \
            return nullptr;                                                    \
        }                                                                      \
    } while (false)

/* Builds a RPFTOCSubDataset from the set of files of the toc entry */
GDALDataset *RPFTOCSubDataset::CreateDataSetFromTocEntry(
    const char *openInformationName, const char *pszTOCFileName, int nEntry,
    const RPFTocEntry *entry, int isRGBA, char **papszMetadataRPFTOCFile)
{
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("VRT");
    if (poDriver == nullptr)
        return nullptr;

    const int N = entry->nVertFrames * entry->nHorizFrames;

    /* This may not be reliable. See below */
    int sizeX =
        static_cast<int>((entry->seLong - entry->nwLong) /
                             (entry->nHorizFrames * entry->horizInterval) +
                         0.5);

    int sizeY =
        static_cast<int>((entry->nwLat - entry->seLat) /
                             (entry->nVertFrames * entry->vertInterval) +
                         0.5);

    if ((EQUAL(entry->type, "CADRG") || (EQUAL(entry->type, "CIB"))))
    {
        // for CADRG and CIB the frame size is defined with 1536x1536
        // CADRG: see MIL-C-89038: 3.5.2 a - Each frame shall comprise a
        // rectangular array of 1536 by 1536 pixels CIB: see MIL-C-89041: 3.5.2
        // a - Each frame shall comprise a rectangular array of 1536 by 1536
        // pixels
        sizeX = 1536;
        sizeY = 1536;
    }

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    GDALGeoTransform gt;
    char *projectionRef = nullptr;
    int index = 0;

    for (int i = 0; i < N; i++)
    {
        if (!entry->frameEntries[i].fileExists)
            continue;

        if (index == 0)
        {
            /* Open the first available file to get its geotransform, projection
             * ref and block size */
            /* Do a few sanity checks too */
            /* Ideally we should make these sanity checks now on ALL files, but
             * it would be too slow */
            /* for large datasets. So these sanity checks will be done at the
             * time we really need */
            /* to access the file (see SanityCheckOK method) */
            GDALDataset *poSrcDS = GDALDataset::FromHandle(GDALOpenShared(
                entry->frameEntries[i].fullFilePath, GA_ReadOnly));
            ASSERT_CREATE_VRT(poSrcDS);
            poSrcDS->GetGeoTransform(gt);
            projectionRef = CPLStrdup(poSrcDS->GetProjectionRef());
            ASSERT_CREATE_VRT(gt[GEOTRSFRM_ROTATION_PARAM1] == 0 &&
                              gt[GEOTRSFRM_ROTATION_PARAM2] ==
                                  0);                          /* No rotation */
            ASSERT_CREATE_VRT(poSrcDS->GetRasterCount() == 1); /* Just 1 band */

            /* Tolerance of 1%... This is necessary for CADRG_L22/RPF/A.TOC for
             * example */
            ASSERT_CREATE_VRT((entry->horizInterval - gt[GEOTRSFRM_WE_RES]) /
                                  entry->horizInterval <
                              0.01); /* X interval same as in TOC */
            ASSERT_CREATE_VRT((entry->vertInterval - (-gt[GEOTRSFRM_NS_RES])) /
                                  entry->vertInterval <
                              0.01); /* Y interval same as in TOC */

            const int ds_sizeX = poSrcDS->GetRasterXSize();
            const int ds_sizeY = poSrcDS->GetRasterYSize();
            /* for polar zone use the sizes from the dataset */
            if ((entry->zone[0] == '9') || (entry->zone[0] == 'J'))
            {
                sizeX = ds_sizeX;
                sizeY = ds_sizeY;
            }

            /* In the case the east longitude is 180, there's a great chance
             * that it is in fact */
            /* truncated in the A.TOC. Thus, the only reliable way to find out
             * the tile width, is to */
            /* read it from the tile dataset itself... */
            /* This is the case for the GNCJNCN dataset that has world coverage
             */
            if (entry->seLong == 180.00)
                sizeX = ds_sizeX;
            else
                ASSERT_CREATE_VRT(sizeX == ds_sizeX);

            ASSERT_CREATE_VRT(sizeY == ds_sizeY);
            poSrcDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
            ASSERT_CREATE_VRT(
                poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
                GCI_PaletteIndex);
            ASSERT_CREATE_VRT(poSrcDS->GetRasterBand(1)->GetRasterDataType() ==
                              GDT_Byte);
            GDALClose(poSrcDS);
        }

        index++;
    }

    if (index == 0)
        return nullptr;

    /* ------------------------------------ */
    /* Create the VRT with the overall size */
    /* ------------------------------------ */
    RPFTOCSubDataset *poVirtualDS = new RPFTOCSubDataset(
        sizeX * entry->nHorizFrames, sizeY * entry->nVertFrames);

    if (papszMetadataRPFTOCFile)
        poVirtualDS->SetMetadata(papszMetadataRPFTOCFile);

    poVirtualDS->SetProjection(projectionRef);

    gt[GEOTRSFRM_TOPLEFT_X] = entry->nwLong;
    gt[GEOTRSFRM_TOPLEFT_Y] = entry->nwLat;
    poVirtualDS->SetGeoTransform(gt);

    int nBands;

    /* In most cases, all the files inside a TOC entry share the same */
    /* palette and we could use it for the VRT. */
    /* In other cases like for CADRG801_France_250K (TOC entry CADRG_250K_2_2),
     */
    /* the file for Corsica and the file for Sardegna do not share the same
     * palette */
    /* however they contain the same RGB triplets and are just ordered
     * differently */
    /* So we can use the same palette */
    /* In the unlikely event where palettes would be incompatible, we can use
     * the RGBA */
    /* option through the config option RPFTOC_FORCE_RGBA */
    if (isRGBA == FALSE)
    {
        poVirtualDS->AddBand(GDT_Byte, nullptr);
        GDALRasterBand *poBand = poVirtualDS->GetRasterBand(1);
        poBand->SetColorInterpretation(GCI_PaletteIndex);
        nBands = 1;

        for (int i = 0; i < N; i++)
        {
            if (!entry->frameEntries[i].fileExists)
                continue;

            bool bAllBlack = true;
            GDALDataset *poSrcDS = GDALDataset::FromHandle(GDALOpenShared(
                entry->frameEntries[i].fullFilePath, GA_ReadOnly));
            if (poSrcDS != nullptr)
            {
                if (poSrcDS->GetRasterCount() == 1)
                {
                    int bHasNoDataValue;
                    const double noDataValue =
                        poSrcDS->GetRasterBand(1)->GetNoDataValue(
                            &bHasNoDataValue);
                    if (bHasNoDataValue)
                        poBand->SetNoDataValue(noDataValue);

                    /* Avoid setting a color table that is all black (which
                     * might be */
                    /* the case of the edge tiles of a RPF subdataset) */
                    GDALColorTable *poCT =
                        poSrcDS->GetRasterBand(1)->GetColorTable();
                    if (poCT != nullptr)
                    {
                        for (int iC = 0; iC < poCT->GetColorEntryCount(); iC++)
                        {
                            if (bHasNoDataValue &&
                                iC == static_cast<int>(noDataValue))
                                continue;

                            const GDALColorEntry *psColorEntry =
                                poCT->GetColorEntry(iC);
                            if (psColorEntry->c1 != 0 ||
                                psColorEntry->c2 != 0 || psColorEntry->c3 != 0)
                            {
                                bAllBlack = false;
                                break;
                            }
                        }

                        /* Assign it temporarily, in the hope of a better match
                         */
                        /* afterwards */
                        poBand->SetColorTable(poCT);
                        if (bAllBlack)
                        {
                            CPLDebug("RPFTOC",
                                     "Skipping %s. Its palette is all black.",
                                     poSrcDS->GetDescription());
                        }
                    }
                }
                GDALClose(poSrcDS);
            }
            if (!bAllBlack)
                break;
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            poVirtualDS->AddBand(GDT_Byte, nullptr);
            GDALRasterBand *poBand = poVirtualDS->GetRasterBand(i + 1);
            poBand->SetColorInterpretation(
                static_cast<GDALColorInterp>(GCI_RedBand + i));
        }
        nBands = 4;
    }

    CPLFree(projectionRef);
    projectionRef = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */

    poVirtualDS->oOvManager.Initialize(
        poVirtualDS, CPLString().Printf("%s.%d", pszTOCFileName, nEntry + 1));

    poVirtualDS->SetDescription(pszTOCFileName);
    poVirtualDS->papszFileList = poVirtualDS->GDALDataset::GetFileList();
    poVirtualDS->SetDescription(openInformationName);

    int iFile = 0;
    for (int i = 0; i < N; i++)
    {
        if (!entry->frameEntries[i].fileExists)
            continue;

        poVirtualDS->SetMetadataItem(CPLSPrintf("FILENAME_%d", iFile),
                                     entry->frameEntries[i].fullFilePath);
        poVirtualDS->papszFileList = CSLAddString(
            poVirtualDS->papszFileList, entry->frameEntries[i].fullFilePath);
        iFile++;

        /* We create proxy datasets and raster bands */
        /* Using real datasets and raster bands is possible in theory */
        /* However for large datasets, a TOC entry can include several hundreds
         * of files */
        /* and we finally reach the limit of maximum file descriptors open at
         * the same time ! */
        /* So the idea is to warp the datasets into a proxy and open the
         * underlying dataset only when it is */
        /* needed (IRasterIO operation). To improve a bit efficiency, we have a
         * cache of opened */
        /* underlying datasets */
        RPFTOCProxyRasterDataSet *ds = new RPFTOCProxyRasterDataSet(
            cpl::down_cast<RPFTOCSubDataset *>(poVirtualDS),
            entry->frameEntries[i].fullFilePath, sizeX, sizeY, nBlockXSize,
            nBlockYSize, poVirtualDS->GetProjectionRef(),
            entry->nwLong +
                entry->frameEntries[i].frameCol * entry->horizInterval * sizeX,
            entry->nwLat -
                entry->frameEntries[i].frameRow * entry->vertInterval * sizeY,
            nBands);

        if (nBands == 1)
        {
            GDALRasterBand *poBand = poVirtualDS->GetRasterBand(1);
            ds->SetReferenceColorTable(poBand->GetColorTable());
            int bHasNoDataValue;
            const double noDataValue = poBand->GetNoDataValue(&bHasNoDataValue);
            if (bHasNoDataValue)
                ds->SetNoDataValue(noDataValue);
        }

        for (int j = 0; j < nBands; j++)
        {
            VRTSourcedRasterBand *poBand =
                reinterpret_cast<VRTSourcedRasterBand *>(
                    poVirtualDS->GetRasterBand(j + 1));
            /* Place the raster band at the right position in the VRT */
            poBand->AddSimpleSource(
                ds->GetRasterBand(j + 1), 0, 0, sizeX, sizeY,
                entry->frameEntries[i].frameCol * sizeX,
                entry->frameEntries[i].frameRow * sizeY, sizeX, sizeY);
        }

        /* The RPFTOCProxyRasterDataSet will be destroyed when its last raster
         * band will be */
        /* destroyed */
        ds->Dereference();
    }

    poVirtualDS->SetMetadataItem("NITF_SCALE", entry->scale);
    poVirtualDS->SetMetadataItem(
        "NITF_SERIES_ABBREVIATION",
        (entry->seriesAbbreviation) ? entry->seriesAbbreviation : "Unknown");
    poVirtualDS->SetMetadataItem("NITF_SERIES_NAME", (entry->seriesName)
                                                         ? entry->seriesName
                                                         : "Unknown");

    return poVirtualDS;
}

/************************************************************************/
/*                             IsNITFFileTOC()                          */
/************************************************************************/

/* Check whether this NITF file is a TOC file */
int RPFTOCDataset::IsNITFFileTOC(NITFFile *psFile)
{
    const char *fileTitle =
        CSLFetchNameValue(psFile->papszMetadata, "NITF_FTITLE");
    while (fileTitle && *fileTitle)
    {
        if (EQUAL(fileTitle, "A.TOC"))
        {
            return TRUE;
        }
        fileTitle++;
    }
    return FALSE;
}

/************************************************************************/
/*                                OpenFileTOC()                         */
/************************************************************************/

/* Create a dataset from a TOC file */
/* If psFile == NULL, the TOC file has no NITF header */
/* If entryName != NULL, the dataset will be made just of the entry of the TOC
 * file */
GDALDataset *RPFTOCDataset::OpenFileTOC(NITFFile *psFile,
                                        const char *pszFilename,
                                        const char *entryName,
                                        const char *openInformationName)
{
    char buffer[48];
    VSILFILE *fp = nullptr;
    if (psFile == nullptr)
    {
        fp = VSIFOpenL(pszFilename, "rb");

        if (fp == nullptr)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "Failed to open file %s.",
                     pszFilename);
            return nullptr;
        }
        if (VSIFReadL(buffer, 1, 48, fp) != 48)
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return nullptr;
        }
    }
    const int isRGBA =
        CPLTestBool(CPLGetConfigOption("RPFTOC_FORCE_RGBA", "NO"));
    RPFToc *toc = (psFile) ? RPFTOCRead(pszFilename, psFile)
                           : RPFTOCReadFromBuffer(pszFilename, fp, buffer);
    if (fp)
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    fp = nullptr;

    if (entryName != nullptr)
    {
        if (toc)
        {
            for (int i = 0; i < toc->nEntries; i++)
            {
                if (EQUAL(entryName, MakeTOCEntryName(&toc->entries[i])))
                {
                    GDALDataset *ds =
                        RPFTOCSubDataset::CreateDataSetFromTocEntry(
                            openInformationName, pszFilename, i,
                            &toc->entries[i], isRGBA,
                            (psFile) ? psFile->papszMetadata : nullptr);

                    RPFTOCFree(toc);
                    return ds;
                }
            }
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The entry %s does not exist in file %s.", entryName,
                     pszFilename);
        }
        RPFTOCFree(toc);
        return nullptr;
    }

    if (toc)
    {
        RPFTOCDataset *ds = new RPFTOCDataset();
        if (psFile)
            ds->SetMetadata(psFile->papszMetadata);

        bool ok = false;
        char *projectionRef = nullptr;
        double nwLong = 0.0;
        double nwLat = 0.0;
        double seLong = 0.0;
        double seLat = 0.0;
        GDALGeoTransform gt;

        ds->papszFileList = CSLAddString(ds->papszFileList, pszFilename);

        for (int i = 0; i < toc->nEntries; i++)
        {
            if (!toc->entries[i].isOverviewOrLegend)
            {
                GDALDataset *tmpDS =
                    RPFTOCSubDataset::CreateDataSetFromTocEntry(
                        openInformationName, pszFilename, i, &toc->entries[i],
                        isRGBA, nullptr);
                if (tmpDS)
                {
                    char **papszSubDatasetFileList = tmpDS->GetFileList();
                    /* Yes, begin at 1, since the first is the a.toc */
                    ds->papszFileList = CSLInsertStrings(
                        ds->papszFileList, -1, papszSubDatasetFileList + 1);
                    CSLDestroy(papszSubDatasetFileList);

                    tmpDS->GetGeoTransform(gt);
                    if (projectionRef == nullptr)
                    {
                        ok = true;
                        projectionRef = CPLStrdup(tmpDS->GetProjectionRef());
                        nwLong = gt[GEOTRSFRM_TOPLEFT_X];
                        nwLat = gt[GEOTRSFRM_TOPLEFT_Y];
                        seLong = nwLong +
                                 gt[GEOTRSFRM_WE_RES] * tmpDS->GetRasterXSize();
                        seLat = nwLat +
                                gt[GEOTRSFRM_NS_RES] * tmpDS->GetRasterYSize();
                    }
                    else if (ok)
                    {
                        double _nwLong = gt[GEOTRSFRM_TOPLEFT_X];
                        double _nwLat = gt[GEOTRSFRM_TOPLEFT_Y];
                        double _seLong = _nwLong + gt[GEOTRSFRM_WE_RES] *
                                                       tmpDS->GetRasterXSize();
                        double _seLat = _nwLat + gt[GEOTRSFRM_NS_RES] *
                                                     tmpDS->GetRasterYSize();
                        if (!EQUAL(projectionRef, tmpDS->GetProjectionRef()))
                            ok = false;
                        if (_nwLong < nwLong)
                            nwLong = _nwLong;
                        if (_nwLat > nwLat)
                            nwLat = _nwLat;
                        if (_seLong > seLong)
                            seLong = _seLong;
                        if (_seLat < seLat)
                            seLat = _seLat;
                    }
                    delete tmpDS;
                    ds->AddSubDataset(pszFilename, &toc->entries[i]);
                }
            }
        }
        if (ok)
        {
            gt[GEOTRSFRM_TOPLEFT_X] = nwLong;
            gt[GEOTRSFRM_TOPLEFT_Y] = nwLat;
            ds->SetSize(
                static_cast<int>(0.5 +
                                 (seLong - nwLong) / gt[GEOTRSFRM_WE_RES]),
                static_cast<int>(0.5 + (seLat - nwLat) / gt[GEOTRSFRM_NS_RES]));

            ds->SetGeoTransform(gt);
            ds->SetProjection(projectionRef);
        }
        CPLFree(projectionRef);
        RPFTOCFree(toc);

        /* --------------------------------------------------------------------
         */
        /*      Initialize any PAM information. */
        /* --------------------------------------------------------------------
         */
        ds->SetDescription(pszFilename);
        ds->TryLoadXML();

        return ds;
    }

    return nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RPFTOCDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (!RPFTOCDriverIdentify(poOpenInfo))
        return nullptr;

    const char *pszFilename = poOpenInfo->pszFilename;
    char *entryName = nullptr;

    if (STARTS_WITH_CI(pszFilename, "NITF_TOC_ENTRY:"))
    {
        pszFilename += strlen("NITF_TOC_ENTRY:");
        entryName = CPLStrdup(pszFilename);
        char *c = entryName;
        while (*c != '\0' && *c != ':')
            c++;
        if (*c != ':')
        {
            CPLFree(entryName);
            return nullptr;
        }
        *c = 0;

        while (*pszFilename != '\0' && *pszFilename != ':')
            pszFilename++;
        pszFilename++;
    }

    if (RPFTOCIsNonNITFFileTOC((entryName != nullptr) ? nullptr : poOpenInfo,
                               pszFilename))
    {
        GDALDataset *poDS = OpenFileTOC(nullptr, pszFilename, entryName,
                                        poOpenInfo->pszFilename);

        CPLFree(entryName);

        if (poDS && poOpenInfo->eAccess == GA_Update)
        {
            ReportUpdateNotSupportedByDriver("RPFTOC");
            delete poDS;
            return nullptr;
        }

        return poDS;
    }

    /* -------------------------------------------------------------------- */
    /*      Open the file with library.                                     */
    /* -------------------------------------------------------------------- */
    NITFFile *psFile = NITFOpen(pszFilename, FALSE);
    if (psFile == nullptr)
    {
        CPLFree(entryName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Check if it is a TOC file .                                     */
    /* -------------------------------------------------------------------- */
    if (IsNITFFileTOC(psFile))
    {
        GDALDataset *poDS = OpenFileTOC(psFile, pszFilename, entryName,
                                        poOpenInfo->pszFilename);
        NITFClose(psFile);
        CPLFree(entryName);

        if (poDS && poOpenInfo->eAccess == GA_Update)
        {
            ReportUpdateNotSupportedByDriver("RPFTOC");
            delete poDS;
            return nullptr;
        }

        return poDS;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "File %s is not a TOC file.",
                 pszFilename);
        NITFClose(psFile);
        CPLFree(entryName);
        return nullptr;
    }
}

/************************************************************************/
/*                          GDALRegister_RPFTOC()                       */
/************************************************************************/

void GDALRegister_RPFTOC()

{
    if (GDALGetDriverByName(RPFTOC_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    RPFTOCDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = RPFTOCDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
