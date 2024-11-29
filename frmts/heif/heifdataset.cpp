/******************************************************************************
 *
 * Project:  HEIF read-only Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "heifdataset.h"

extern "C" void CPL_DLL GDALRegister_HEIF();

/************************************************************************/
/*                       GDALHEIFRasterBand                             */
/************************************************************************/

class GDALHEIFRasterBand final : public GDALPamRasterBand
{
  protected:
    CPLErr IReadBlock(int, int, void *) override;

  public:
    GDALHEIFRasterBand(GDALHEIFDataset *poDSIn, int nBandIn);

    GDALColorInterp GetColorInterpretation() override
    {
        return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
    }

    int GetOverviewCount() override
    {
        GDALHEIFDataset *poGDS = static_cast<GDALHEIFDataset *>(poDS);
        return static_cast<int>(poGDS->m_apoOvrDS.size());
    }

    GDALRasterBand *GetOverview(int idx) override
    {
        if (idx < 0 || idx >= GetOverviewCount())
            return nullptr;
        GDALHEIFDataset *poGDS = static_cast<GDALHEIFDataset *>(poDS);
        return poGDS->m_apoOvrDS[idx]->GetRasterBand(nBand);
    }
};

/************************************************************************/
/*                         GDALHEIFDataset()                            */
/************************************************************************/

GDALHEIFDataset::GDALHEIFDataset() : m_hCtxt(heif_context_alloc())

{
#ifdef HAS_CUSTOM_FILE_READER
    m_oReader.reader_api_version = 1;
    m_oReader.get_position = GetPositionCbk;
    m_oReader.read = ReadCbk;
    m_oReader.seek = SeekCbk;
    m_oReader.wait_for_file_size = WaitForFileSizeCbk;
#endif
}

/************************************************************************/
/*                         ~GDALHEIFDataset()                           */
/************************************************************************/

GDALHEIFDataset::~GDALHEIFDataset()
{
    if (m_hCtxt)
        heif_context_free(m_hCtxt);
#ifdef HAS_CUSTOM_FILE_READER
    if (m_fpL)
        VSIFCloseL(m_fpL);
#endif
#ifndef LIBHEIF_SUPPORTS_TILES
    if (m_hImage)
        heif_image_release(m_hImage);
#endif
    if (m_hImageHandle)
        heif_image_handle_release(m_hImageHandle);
}

#ifdef HAS_CUSTOM_FILE_READER

/************************************************************************/
/*                          GetPositionCbk()                            */
/************************************************************************/

int64_t GDALHEIFDataset::GetPositionCbk(void *userdata)
{
    GDALHEIFDataset *poThis = static_cast<GDALHEIFDataset *>(userdata);
    return static_cast<int64_t>(VSIFTellL(poThis->m_fpL));
}

/************************************************************************/
/*                             ReadCbk()                                */
/************************************************************************/

int GDALHEIFDataset::ReadCbk(void *data, size_t size, void *userdata)
{
    GDALHEIFDataset *poThis = static_cast<GDALHEIFDataset *>(userdata);
    return VSIFReadL(data, 1, size, poThis->m_fpL) == size ? 0 : -1;
}

/************************************************************************/
/*                             SeekCbk()                                */
/************************************************************************/

int GDALHEIFDataset::SeekCbk(int64_t position, void *userdata)
{
    GDALHEIFDataset *poThis = static_cast<GDALHEIFDataset *>(userdata);
    return VSIFSeekL(poThis->m_fpL, static_cast<vsi_l_offset>(position),
                     SEEK_SET);
}

/************************************************************************/
/*                         WaitForFileSizeCbk()                         */
/************************************************************************/

enum heif_reader_grow_status
GDALHEIFDataset::WaitForFileSizeCbk(int64_t target_size, void *userdata)
{
    GDALHEIFDataset *poThis = static_cast<GDALHEIFDataset *>(userdata);
    if (target_size > static_cast<int64_t>(poThis->m_nSize))
        return heif_reader_grow_status_size_beyond_eof;
    return heif_reader_grow_status_size_reached;
}

#endif

/************************************************************************/
/*                              Init()                                  */
/************************************************************************/

bool GDALHEIFDataset::Init(GDALOpenInfo *poOpenInfo)
{
    CPLString osFilename(poOpenInfo->pszFilename);
#ifdef HAS_CUSTOM_FILE_READER
    VSILFILE *fpL;
#endif
    int iPart = 0;
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "HEIF:"))
    {
        const char *pszPartPos = poOpenInfo->pszFilename + strlen("HEIF:");
        const char *pszNextColumn = strchr(pszPartPos, ':');
        if (pszNextColumn == nullptr)
            return false;
        iPart = atoi(pszPartPos);
        if (iPart <= 0)
            return false;
        osFilename = pszNextColumn + 1;
#ifdef HAS_CUSTOM_FILE_READER
        fpL = VSIFOpenL(osFilename, "rb");
        if (fpL == nullptr)
            return false;
#endif
    }
    else
    {
#ifdef HAS_CUSTOM_FILE_READER
        fpL = poOpenInfo->fpL;
        poOpenInfo->fpL = nullptr;
#endif
    }

#ifdef HAS_CUSTOM_FILE_READER
    m_oReader.reader_api_version = 1;
    m_oReader.get_position = GetPositionCbk;
    m_oReader.read = ReadCbk;
    m_oReader.seek = SeekCbk;
    m_oReader.wait_for_file_size = WaitForFileSizeCbk;
    m_fpL = fpL;

    VSIFSeekL(m_fpL, 0, SEEK_END);
    m_nSize = VSIFTellL(m_fpL);
    VSIFSeekL(m_fpL, 0, SEEK_SET);

    auto err =
        heif_context_read_from_reader(m_hCtxt, &m_oReader, this, nullptr);
    if (err.code != heif_error_Ok)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 err.message ? err.message : "Cannot open file");
        return false;
    }
#else
    auto err =
        heif_context_read_from_file(m_hCtxt, osFilename.c_str(), nullptr);
    if (err.code != heif_error_Ok)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 err.message ? err.message : "Cannot open file");
        return false;
    }
#endif

    const int nSubdatasets =
        heif_context_get_number_of_top_level_images(m_hCtxt);
    if (iPart == 0)
    {
        if (nSubdatasets > 1)
        {
            CPLStringList aosSubDS;
            for (int i = 0; i < nSubdatasets; i++)
            {
                aosSubDS.SetNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", i + 1),
                    CPLSPrintf("HEIF:%d:%s", i + 1, poOpenInfo->pszFilename));
                aosSubDS.SetNameValue(CPLSPrintf("SUBDATASET_%d_DESC", i + 1),
                                      CPLSPrintf("Subdataset %d", i + 1));
            }
            GDALDataset::SetMetadata(aosSubDS.List(), "SUBDATASETS");
        }
    }
    else if (iPart > nSubdatasets)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid image part number. Maximum allowed is %d",
                 nSubdatasets);
        return false;
    }
    else
    {
        iPart--;
    }
    std::vector<heif_item_id> idArray(nSubdatasets);
    heif_context_get_list_of_top_level_image_IDs(m_hCtxt, &idArray[0],
                                                 nSubdatasets);
    const auto itemId = idArray[iPart];

    err = heif_context_get_image_handle(m_hCtxt, itemId, &m_hImageHandle);
    if (err.code != heif_error_Ok)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 err.message ? err.message : "Cannot open image");
        return false;
    }

#ifdef LIBHEIF_SUPPORTS_TILES
    err = heif_image_handle_get_image_tiling(m_hImageHandle, true, &m_tiling);
    if (err.code != heif_error_Ok)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 err.message ? err.message : "Cannot get image tiling");
        return false;
    }
#endif

    nRasterXSize = heif_image_handle_get_width(m_hImageHandle);
    nRasterYSize = heif_image_handle_get_height(m_hImageHandle);
    const int l_nBands =
        3 + (heif_image_handle_has_alpha_channel(m_hImageHandle) ? 1 : 0);
    for (int i = 0; i < l_nBands; i++)
    {
        SetBand(i + 1, new GDALHEIFRasterBand(this, i + 1));
    }

    ReadMetadata();

    OpenThumbnails();

    if (poOpenInfo->nHeaderBytes > 12 &&
        memcmp(poOpenInfo->pabyHeader + 4, "ftypavif", 8) == 0)
    {
        poDriver = GetGDALDriverManager()->GetDriverByName("AVIF_HEIF");
    }

    // Initialize any PAM information.
    if (nSubdatasets > 1)
    {
        SetSubdatasetName(CPLSPrintf("%d", iPart + 1));
        SetPhysicalFilename(osFilename.c_str());
    }
    SetDescription(poOpenInfo->pszFilename);
    TryLoadXML(poOpenInfo->GetSiblingFiles());

    return true;
}

/************************************************************************/
/*                         ReadMetadata()                               */
/************************************************************************/

void GDALHEIFDataset::ReadMetadata()
{
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 19, 0)
    processProperties();
    ReadUserDescription();
#endif
    const int nMDBlocks = heif_image_handle_get_number_of_metadata_blocks(
        m_hImageHandle, nullptr);
    if (nMDBlocks <= 0)
        return;

    std::vector<heif_item_id> idsMDBlock(nMDBlocks);
    heif_image_handle_get_list_of_metadata_block_IDs(m_hImageHandle, nullptr,
                                                     &idsMDBlock[0], nMDBlocks);
    for (const auto &id : idsMDBlock)
    {
        const char *pszType =
            heif_image_handle_get_metadata_type(m_hImageHandle, id);
        const size_t nCount =
            heif_image_handle_get_metadata_size(m_hImageHandle, id);
        if (pszType && EQUAL(pszType, "Exif") && nCount > 8 &&
            nCount < 1024 * 1024)
        {
            std::vector<GByte> data(nCount);
            heif_image_handle_get_metadata(m_hImageHandle, id, &data[0]);

            // There are 2 variants
            // - the one from
            // https://github.com/nokiatech/heif_conformance/blob/master/conformance_files/C034.heic
            //   where the TIFF file immediately starts
            // - the one found in iPhone files (among others), where there
            //   is first a 4-byte big-endian offset (after those initial 4
            //   bytes) that points to the TIFF file, with a "Exif\0\0" just
            //   before
            unsigned nTIFFFileOffset = 0;
            if (memcmp(&data[0], "II\x2a\x00", 4) == 0 ||
                memcmp(&data[0], "MM\x00\x2a", 4) == 0)
            {
                // do nothing
            }
            else
            {
                unsigned nOffset;
                memcpy(&nOffset, &data[0], 4);
                CPL_MSBPTR32(&nOffset);
                if (nOffset < nCount - 8 &&
                    (memcmp(&data[nOffset + 4], "II\x2a\x00", 4) == 0 ||
                     memcmp(&data[nOffset + 4], "MM\x00\x2a", 4) == 0))
                {
                    nTIFFFileOffset = nOffset + 4;
                }
                else
                {
                    continue;
                }
            }

            const CPLString osTempFile(
                VSIMemGenerateHiddenFilename("heif_exif.tif"));
            VSILFILE *fpTemp =
                VSIFileFromMemBuffer(osTempFile, &data[nTIFFFileOffset],
                                     nCount - nTIFFFileOffset, FALSE);
            char **papszMD = nullptr;

            const bool bLittleEndianTIFF = data[nTIFFFileOffset] == 'I' &&
                                           data[nTIFFFileOffset + 1] == 'I';
            const bool bLSBPlatform = CPL_IS_LSB != 0;
            const bool bSwabflag = bLittleEndianTIFF != bLSBPlatform;

            int nTIFFDirOff;
            memcpy(&nTIFFDirOff, &data[nTIFFFileOffset + 4], 4);
            if (bSwabflag)
            {
                CPL_SWAP32PTR(&nTIFFDirOff);
            }
            int nExifOffset = 0;
            int nInterOffset = 0;
            int nGPSOffset = 0;
            EXIFExtractMetadata(papszMD, fpTemp, nTIFFDirOff, bSwabflag, 0,
                                nExifOffset, nInterOffset, nGPSOffset);
            if (nExifOffset > 0)
            {
                EXIFExtractMetadata(papszMD, fpTemp, nExifOffset, bSwabflag, 0,
                                    nExifOffset, nInterOffset, nGPSOffset);
            }
            if (nGPSOffset > 0)
            {
                EXIFExtractMetadata(papszMD, fpTemp, nGPSOffset, bSwabflag, 0,
                                    nExifOffset, nInterOffset, nGPSOffset);
            }
            if (nInterOffset > 0)
            {
                EXIFExtractMetadata(papszMD, fpTemp, nInterOffset, bSwabflag, 0,
                                    nExifOffset, nInterOffset, nGPSOffset);
            }

            if (papszMD)
            {
                GDALDataset::SetMetadata(papszMD, "EXIF");
                CSLDestroy(papszMD);
            }

            VSIFCloseL(fpTemp);
            VSIUnlink(osTempFile);
        }
        else if (pszType && EQUAL(pszType, "mime"))
        {
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 2, 0)
            const char *pszContentType =
                heif_image_handle_get_metadata_content_type(m_hImageHandle, id);
            if (pszContentType &&
                EQUAL(pszContentType, "application/rdf+xml") &&
#else
            if (
#endif
                nCount > 0 && nCount < 1024 * 1024)
            {
                std::string osXMP;
                osXMP.resize(nCount);
                heif_image_handle_get_metadata(m_hImageHandle, id, &osXMP[0]);
                if (osXMP.find("<?xpacket") != std::string::npos)
                {
                    char *apszMDList[2] = {&osXMP[0], nullptr};
                    GDALDataset::SetMetadata(apszMDList, "xml:XMP");
                }
            }
        }
    }
}

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 19, 0)
static bool GetPropertyData(heif_context *m_hCtxt, heif_item_id item_id,
                            heif_property_id prop_id,
                            std::shared_ptr<std::vector<GByte>> &data)
{
    size_t size;
    heif_error err =
        heif_item_get_property_raw_size(m_hCtxt, item_id, prop_id, &size);
    if (err.code != 0)
    {
        return false;
    }
    if (size == 0)
    {
        return false;
    }
    data = std::make_shared<std::vector<uint8_t>>(size);
    err = heif_item_get_property_raw_data(m_hCtxt, item_id, prop_id,
                                          data->data());
    if (err.code != 0)
    {
        return false;
    }
    return true;
}

void GDALHEIFDataset::processProperties()
{
    constexpr heif_item_property_type TIEP_4CC =
        (heif_item_property_type)heif_fourcc('t', 'i', 'e', 'p');
    constexpr heif_item_property_type MTXF_4CC =
        (heif_item_property_type)heif_fourcc('m', 't', 'x', 'f');
    constexpr heif_item_property_type MCRS_4CC =
        (heif_item_property_type)heif_fourcc('m', 'c', 'r', 's');
    constexpr int MAX_PROPERTIES_REQUIRED = 50;
    heif_property_id prop_ids[MAX_PROPERTIES_REQUIRED];
    heif_item_id item_id = heif_image_handle_get_item_id(m_hImageHandle);
    int num_props = heif_item_get_properties_of_type(
        m_hCtxt, item_id, heif_item_property_type_invalid, &prop_ids[0],
        MAX_PROPERTIES_REQUIRED);
    for (int i = 0; i < num_props; i++)
    {
        heif_item_property_type prop_type =
            heif_item_get_property_type(m_hCtxt, item_id, prop_ids[i]);
        if (prop_type == TIEP_4CC)
        {
            std::shared_ptr<std::vector<uint8_t>> data;
            if (!GetPropertyData(m_hCtxt, item_id, prop_ids[i], data))
            {
                continue;
            }
            geoHEIF.addGCPs(data->data(), data->size());
        }
        else if (prop_type == MTXF_4CC)
        {
            std::shared_ptr<std::vector<uint8_t>> data;
            if (!GetPropertyData(m_hCtxt, item_id, prop_ids[i], data))
            {
                continue;
            }
            geoHEIF.setModelTransformation(data->data(), data->size());
        }
        else if (prop_type == MCRS_4CC)
        {
            std::shared_ptr<std::vector<uint8_t>> data;
            if (!GetPropertyData(m_hCtxt, item_id, prop_ids[i], data))
            {
                continue;
            }
            geoHEIF.extractSRS(data->data(), data->size());
        }
    }
}

/************************************************************************/
/*                      ReadUserDescription()                           */
/************************************************************************/
void GDALHEIFDataset::ReadUserDescription()
{
    constexpr int MAX_PROPERTIES = 50;
    heif_item_id item_id = heif_image_handle_get_item_id(m_hImageHandle);
    heif_property_id properties[MAX_PROPERTIES];
    int nProps = heif_item_get_properties_of_type(
        m_hCtxt, item_id, heif_item_property_type_user_description, properties,
        MAX_PROPERTIES);

    heif_property_user_description *user_description = nullptr;
    for (int i = 0; i < nProps; i++)
    {
        heif_error err = heif_item_get_property_user_description(
            m_hCtxt, item_id, properties[i], &user_description);
        if (err.code == 0)
        {
            std::string domain = "DESCRIPTION";
            if (strlen(user_description->lang) != 0)
            {
                domain += "_";
                domain += user_description->lang;
            }
            SetMetadataItem("NAME", user_description->name, domain.c_str());
            SetMetadataItem("DESCRIPTION", user_description->description,
                            domain.c_str());
            if (strlen(user_description->tags) != 0)
            {
                SetMetadataItem("TAGS", user_description->tags, domain.c_str());
            }
            heif_property_user_description_release(user_description);
        }
    }
}
#endif

/************************************************************************/
/*                         OpenThumbnails()                             */
/************************************************************************/

void GDALHEIFDataset::OpenThumbnails()
{
    int nThumbnails =
        heif_image_handle_get_number_of_thumbnails(m_hImageHandle);
    if (nThumbnails <= 0)
        return;

    heif_item_id thumbnailId = 0;
    heif_image_handle_get_list_of_thumbnail_IDs(m_hImageHandle, &thumbnailId,
                                                1);
    heif_image_handle *hThumbnailHandle = nullptr;
    heif_image_handle_get_thumbnail(m_hImageHandle, thumbnailId,
                                    &hThumbnailHandle);
    if (hThumbnailHandle == nullptr)
        return;

    const int nThumbnailBands =
        3 + (heif_image_handle_has_alpha_channel(hThumbnailHandle) ? 1 : 0);
    if (nThumbnailBands != nBands)
    {
        heif_image_handle_release(hThumbnailHandle);
        return;
    }
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 4, 0)
    const int nBits =
        heif_image_handle_get_luma_bits_per_pixel(hThumbnailHandle);
    if (nBits != heif_image_handle_get_luma_bits_per_pixel(m_hImageHandle))
    {
        heif_image_handle_release(hThumbnailHandle);
        return;
    }
#endif

    auto poOvrDS = std::make_unique<GDALHEIFDataset>();
    poOvrDS->m_hImageHandle = hThumbnailHandle;
    poOvrDS->m_bIsThumbnail = true;
    poOvrDS->nRasterXSize = heif_image_handle_get_width(hThumbnailHandle);
    poOvrDS->nRasterYSize = heif_image_handle_get_height(hThumbnailHandle);
#ifdef LIBHEIF_SUPPORTS_TILES
    auto err = heif_image_handle_get_image_tiling(hThumbnailHandle, true,
                                                  &poOvrDS->m_tiling);
    if (err.code != heif_error_Ok)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 err.message ? err.message : "Cannot get image tiling");
        heif_image_handle_release(hThumbnailHandle);
        return;
    }
#endif
    for (int i = 0; i < nBands; i++)
    {
        poOvrDS->SetBand(i + 1, new GDALHEIFRasterBand(poOvrDS.get(), i + 1));
    }
    m_apoOvrDS.push_back(std::move(poOvrDS));
}

/************************************************************************/
/*                     HEIFDriverIdentify()                             */
/************************************************************************/

static int HEIFDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "HEIF:"))
        return true;

    if (poOpenInfo->nHeaderBytes < 12 || poOpenInfo->fpL == nullptr)
        return false;

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 4, 0)
    const auto res =
        heif_check_filetype(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes);
    if (res == heif_filetype_yes_supported)
        return TRUE;
    if (res == heif_filetype_maybe)
        return -1;
    if (res == heif_filetype_yes_unsupported)
    {
        CPLDebug("HEIF", "HEIF file, but not supported by libheif");
    }
    return FALSE;
#else
    // Simplistic test...
    const unsigned char abySig1[] = "\x00"
                                    "\x00"
                                    "\x00"
                                    "\x20"
                                    "ftypheic";
    const unsigned char abySig2[] = "\x00"
                                    "\x00"
                                    "\x00"
                                    "\x18"
                                    "ftypheic";
    const unsigned char abySig3[] = "\x00"
                                    "\x00"
                                    "\x00"
                                    "\x18"
                                    "ftypmif1"
                                    "\x00"
                                    "\x00"
                                    "\x00"
                                    "\x00"
                                    "mif1heic";
    return (poOpenInfo->nHeaderBytes >= static_cast<int>(sizeof(abySig1)) &&
            memcmp(poOpenInfo->pabyHeader, abySig1, sizeof(abySig1)) == 0) ||
           (poOpenInfo->nHeaderBytes >= static_cast<int>(sizeof(abySig2)) &&
            memcmp(poOpenInfo->pabyHeader, abySig2, sizeof(abySig2)) == 0) ||
           (poOpenInfo->nHeaderBytes >= static_cast<int>(sizeof(abySig3)) &&
            memcmp(poOpenInfo->pabyHeader, abySig3, sizeof(abySig3)) == 0);
#endif
}

/************************************************************************/
/*                            OpenHEIF()                                */
/************************************************************************/

GDALDataset *GDALHEIFDataset::OpenHEIF(GDALOpenInfo *poOpenInfo)
{
    if (!HEIFDriverIdentify(poOpenInfo))
    {
        return nullptr;
    }
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Update of existing HEIF file not supported");
        return nullptr;
    }

    auto poDS = std::make_unique<GDALHEIFDataset>();
    if (!poDS->Init(poOpenInfo))
        return nullptr;

    return poDS.release();
}

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 12, 0)

/************************************************************************/
/*                     HEIFIdentifyOnlyAVIF()                           */
/************************************************************************/

static int HEIFIdentifyOnlyAVIF(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->nHeaderBytes < 12 || poOpenInfo->fpL == nullptr)
        return false;
    if (memcmp(poOpenInfo->pabyHeader + 4, "ftypavif", 8) == 0)
        return true;
    return false;
}

/************************************************************************/
/*                              OpenAVIF()                              */
/************************************************************************/

GDALDataset *GDALHEIFDataset::OpenAVIF(GDALOpenInfo *poOpenInfo)
{
    if (!HEIFIdentifyOnlyAVIF(poOpenInfo))
        return nullptr;
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Update of existing AVIF file not supported");
        return nullptr;
    }

    auto poDS = std::make_unique<GDALHEIFDataset>();
    if (!poDS->Init(poOpenInfo))
        return nullptr;

    return poDS.release();
}
#endif

/************************************************************************/
/*                          GDALHEIFRasterBand()                        */
/************************************************************************/

GDALHEIFRasterBand::GDALHEIFRasterBand(GDALHEIFDataset *poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Byte;
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 4, 0)
    const int nBits =
        heif_image_handle_get_luma_bits_per_pixel(poDSIn->m_hImageHandle);
    if (nBits > 8)
    {
        eDataType = GDT_UInt16;
    }
    if (nBits != 8 && nBits != 16)
    {
        GDALRasterBand::SetMetadataItem("NBITS", CPLSPrintf("%d", nBits),
                                        "IMAGE_STRUCTURE");
    }
#endif

#ifdef LIBHEIF_SUPPORTS_TILES
    nBlockXSize = poDSIn->m_tiling.tile_width;
    nBlockYSize = poDSIn->m_tiling.tile_height;
#else
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
#endif
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/
#ifdef LIBHEIF_SUPPORTS_TILES
CPLErr GDALHEIFRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                      void *pImage)
{
    GDALHEIFDataset *poGDS = static_cast<GDALHEIFDataset *>(poDS);
    if (poGDS->m_bFailureDecoding)
        return CE_Failure;
    const int nBands = poGDS->GetRasterCount();
    heif_image *hImage = nullptr;
    struct heif_decoding_options *decode_options =
        heif_decoding_options_alloc();

    auto err = heif_image_handle_decode_image_tile(
        poGDS->m_hImageHandle, &hImage, heif_colorspace_RGB,
        nBands == 3
            ? (eDataType == GDT_UInt16 ?
#if CPL_IS_LSB
                                       heif_chroma_interleaved_RRGGBB_LE
#else
                                       heif_chroma_interleaved_RRGGBB_BE
#endif
                                       : heif_chroma_interleaved_RGB)
            : (eDataType == GDT_UInt16 ?
#if CPL_IS_LSB
                                       heif_chroma_interleaved_RRGGBBAA_LE
#else
                                       heif_chroma_interleaved_RRGGBBAA_BE
#endif
                                       : heif_chroma_interleaved_RGBA),
        decode_options, nBlockXOff, nBlockYOff);

    if (err.code != heif_error_Ok)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 err.message ? err.message : "Cannot decode image");
        poGDS->m_bFailureDecoding = true;
        heif_decoding_options_free(decode_options);
        return CE_Failure;
    }
    heif_decoding_options_free(decode_options);
    int nStride = 0;
    const uint8_t *pSrcData = heif_image_get_plane_readonly(
        hImage, heif_channel_interleaved, &nStride);
    if (eDataType == GDT_Byte)
    {
        for (int y = 0; y < nBlockYSize; y++)
        {
            for (int x = 0; x < nBlockXSize; x++)
            {
                const size_t srcIndex = static_cast<size_t>(y) * nStride +
                                        static_cast<size_t>(x) * nBands +
                                        nBand - 1;
                const size_t outIndex =
                    static_cast<size_t>(y) * nBlockXSize + x;
                (static_cast<GByte *>(pImage))[outIndex] = pSrcData[srcIndex];
            }
        }
    }
    else
    {
        for (int y = 0; y < nBlockYSize; y++)
        {
            for (int x = 0; x < nBlockXSize; x++)
            {
                const size_t srcIndex = static_cast<size_t>(y) * (nStride / 2) +
                                        static_cast<size_t>(x) * nBands +
                                        nBand - 1;
                const size_t outIndex =
                    static_cast<size_t>(y) * nBlockXSize + x;
                (static_cast<GUInt16 *>(pImage))[outIndex] =
                    (reinterpret_cast<const GUInt16 *>(pSrcData))[srcIndex];
            }
        }
    }
    heif_image_release(hImage);
    return CE_None;
}
#else
CPLErr GDALHEIFRasterBand::IReadBlock(int, int nBlockYOff, void *pImage)
{
    GDALHEIFDataset *poGDS = static_cast<GDALHEIFDataset *>(poDS);
    if (poGDS->m_bFailureDecoding)
        return CE_Failure;
    const int nBands = poGDS->GetRasterCount();
    if (poGDS->m_hImage == nullptr)
    {
        auto err = heif_decode_image(
            poGDS->m_hImageHandle, &(poGDS->m_hImage), heif_colorspace_RGB,
            nBands == 3
                ? (
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 4, 0)
                      eDataType == GDT_UInt16 ?
#if CPL_IS_LSB
                                              heif_chroma_interleaved_RRGGBB_LE
#else
                                              heif_chroma_interleaved_RRGGBB_BE
#endif
                                              :
#endif
                                              heif_chroma_interleaved_RGB)
                : (
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 4, 0)
                      eDataType == GDT_UInt16
                          ?
#if CPL_IS_LSB
                          heif_chroma_interleaved_RRGGBBAA_LE
#else
                          heif_chroma_interleaved_RRGGBBAA_BE
#endif
                          :
#endif
                          heif_chroma_interleaved_RGBA),
            nullptr);
        if (err.code != heif_error_Ok)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     err.message ? err.message : "Cannot decode image");
            poGDS->m_bFailureDecoding = true;
            return CE_Failure;
        }
        const int nBitsPerPixel = heif_image_get_bits_per_pixel(
            poGDS->m_hImage, heif_channel_interleaved);
        if (nBitsPerPixel != nBands * GDALGetDataTypeSize(eDataType))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected bits_per_pixel = %d value", nBitsPerPixel);
            poGDS->m_bFailureDecoding = true;
            return CE_Failure;
        }
    }

    int nStride = 0;
    const uint8_t *pSrcData = heif_image_get_plane_readonly(
        poGDS->m_hImage, heif_channel_interleaved, &nStride);
    pSrcData += static_cast<size_t>(nBlockYOff) * nStride;
    if (eDataType == GDT_Byte)
    {
        for (int i = 0; i < nBlockXSize; i++)
            (static_cast<GByte *>(pImage))[i] =
                pSrcData[nBand - 1 + static_cast<size_t>(i) * nBands];
    }
    else
    {
        for (int i = 0; i < nBlockXSize; i++)
            (static_cast<GUInt16 *>(pImage))[i] =
                (reinterpret_cast<const GUInt16 *>(
                    pSrcData))[nBand - 1 + static_cast<size_t>(i) * nBands];
    }

    return CE_None;
}
#endif

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 19, 0)
/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALHEIFDataset::GetGeoTransform(double *padfTransform)
{
    return geoHEIF.GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/
const OGRSpatialReference *GDALHEIFDataset::GetSpatialRef() const
{
    return geoHEIF.GetSpatialRef();
}

int GDALHEIFDataset::GetGCPCount()
{
    return geoHEIF.GetGCPCount();
}

const GDAL_GCP *GDALHEIFDataset::GetGCPs()
{
    return geoHEIF.GetGCPs();
}

const OGRSpatialReference *GDALHEIFDataset::GetGCPSpatialRef() const
{
    return this->GetSpatialRef();
}
#endif

/************************************************************************/
/*                       GDALRegister_HEIF()                            */
/************************************************************************/

void GDALRegister_HEIF()

{
    if (!GDAL_CHECK_VERSION("HEIF driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    auto poDM = GetGDALDriverManager();
    {
        GDALDriver *poDriver = new GDALDriver();
        HEIFDriverSetCommonMetadata(poDriver);

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 12, 0)
        if (heif_have_decoder_for_format(heif_compression_AVC))
        {
            poDriver->SetMetadataItem("SUPPORTS_AVC", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_AVC))
        {
            poDriver->SetMetadataItem("SUPPORTS_AVC_WRITE", "YES", "HEIF");
        }
        // If the AVIF dedicated driver is not available, register an AVIF driver,
        // called AVIF_HEIF, based on libheif, if it has AV1 decoding capabilities.
        if (heif_have_decoder_for_format(heif_compression_AV1))
        {
            poDriver->SetMetadataItem("SUPPORTS_AVIF", "YES", "HEIF");
            poDriver->SetMetadataItem("SUPPORTS_AV1", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_AV1))
        {
            poDriver->SetMetadataItem("SUPPORTS_AV1_WRITE", "YES", "HEIF");
        }
        if (heif_have_decoder_for_format(heif_compression_HEVC))
        {
            poDriver->SetMetadataItem("SUPPORTS_HEVC", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_HEVC))
        {
            poDriver->SetMetadataItem("SUPPORTS_HEVC_WRITE", "YES", "HEIF");
        }
        if (heif_have_decoder_for_format(heif_compression_JPEG))
        {
            poDriver->SetMetadataItem("SUPPORTS_JPEG", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_JPEG))
        {
            poDriver->SetMetadataItem("SUPPORTS_JPEG_WRITE", "YES", "HEIF");
        }
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 15, 0)
        if (heif_have_decoder_for_format(heif_compression_JPEG2000))
        {
            poDriver->SetMetadataItem("SUPPORTS_JPEG2000", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_JPEG2000))
        {
            poDriver->SetMetadataItem("SUPPORTS_JPEG2000_WRITE", "YES", "HEIF");
        }
#endif
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 18, 0)
        if (heif_have_decoder_for_format(heif_compression_HTJ2K))
        {
            poDriver->SetMetadataItem("SUPPORTS_HTJ2K", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_HTJ2K))
        {
            poDriver->SetMetadataItem("SUPPORTS_HTJ2K_WRITE", "YES", "HEIF");
        }
#endif
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 16, 0)
        if (heif_have_decoder_for_format(heif_compression_uncompressed))
        {
            poDriver->SetMetadataItem("SUPPORTS_UNCOMPRESSED", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_uncompressed))
        {
            poDriver->SetMetadataItem("SUPPORTS_UNCOMPRESSED_WRITE", "YES",
                                      "HEIF");
        }
#endif
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 15, 0)
        if (heif_have_decoder_for_format(heif_compression_VVC))
        {
            poDriver->SetMetadataItem("SUPPORTS_VVC", "YES", "HEIF");
        }
        if (heif_have_encoder_for_format(heif_compression_VVC))
        {
            poDriver->SetMetadataItem("SUPPORTS_VVC_WRITE", "YES", "HEIF");
        }
#endif
#else
        // Anything that old probably supports only HEVC
        poDriver->SetMetadataItem("SUPPORTS_HEVC", "YES", "HEIF");
#endif
#ifdef LIBHEIF_SUPPORTS_TILES
        poDriver->SetMetadataItem("SUPPORTS_TILES", "YES", "HEIF");
#endif
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 19, 0)
        poDriver->SetMetadataItem("SUPPORTS_GEOHEIF", "YES", "HEIF");
#endif
        poDriver->pfnOpen = GDALHEIFDataset::OpenHEIF;

#ifdef HAS_CUSTOM_FILE_WRITER
        poDriver->pfnCreateCopy = GDALHEIFDataset::CreateCopy;
#endif
        poDM->RegisterDriver(poDriver);
    }

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 12, 0)
    // If the AVIF dedicated driver is not available, register an AVIF driver,
    // called AVIF_HEIF, based on libheif, if it has AV1 decoding capabilities.
    if (heif_have_decoder_for_format(heif_compression_AV1) &&
        !poDM->IsKnownDriver("AVIF") && !poDM->IsKnownDriver("AVIF_HEIF"))
    {
        GDALDriver *poAVIF_HEIFDriver = new GDALDriver();
        poAVIF_HEIFDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
        poAVIF_HEIFDriver->SetDescription("AVIF_HEIF");
        poAVIF_HEIFDriver->SetMetadataItem(
            GDAL_DMD_LONGNAME, "AV1 Image File Format (using libheif)");
        poAVIF_HEIFDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/avif");
        poAVIF_HEIFDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                                           "drivers/raster/heif.html");
        poAVIF_HEIFDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "avif");
        poAVIF_HEIFDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

        poAVIF_HEIFDriver->pfnOpen = GDALHEIFDataset::OpenAVIF;
        poAVIF_HEIFDriver->pfnIdentify = HEIFIdentifyOnlyAVIF;

        poDM->RegisterDriver(poAVIF_HEIFDriver);
    }
#endif
}
