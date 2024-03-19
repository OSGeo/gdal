/******************************************************************************
 *
 * Project:  MSG Native Reader
 * Purpose:  All code for EUMETSAT Archive format reader
 * Author:   Frans van den Bergh, fvdbergh@csir.co.za
 *
 ******************************************************************************
 * Copyright (c) 2005, Frans van den Bergh <fvdbergh@csir.co.za>
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_error.h"

#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include "msg_reader_core.h"

#include <algorithm>

using namespace msg_native_format;

typedef enum
{
    MODE_VISIR,  // Visible and Infrared bands (1 through 11) in 10-bit raw mode
    MODE_HRV,    // Pan band (band 11) only, in 10-bit raw mode
    MODE_RAD     // Black-body temperature (K) for thermal bands only (4-10),
                 // 64-bit float
} open_mode_type;

typedef enum
{
    WHOLE_DISK,
    RSS,       // letterbox of N 1/3 of earth
    SPLIT_HRV  // the half-width HRV, may be sheared into two block to follow
               // the sun W later in the day
} image_shape_type;

class MSGNRasterBand;

/************************************************************************/
/* ==================================================================== */
/*                            MSGNDataset                               */
/* ==================================================================== */
/************************************************************************/

class MSGNDataset final : public GDALDataset
{
    friend class MSGNRasterBand;

    VSILFILE *fp;

    Msg_reader_core *msg_reader_core;
    open_mode_type m_open_mode = MODE_VISIR;
    image_shape_type m_Shape = WHOLE_DISK;
    int m_nHRVSplitLine = 0;
    int m_nHRVLowerShiftX = 0;
    int m_nHRVUpperShiftX = 0;
    double adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};

  public:
    MSGNDataset();
    ~MSGNDataset();

    static GDALDataset *Open(GDALOpenInfo *);

    CPLErr GetGeoTransform(double *padfTransform) override;
    const OGRSpatialReference *GetSpatialRef() const override;
};

/************************************************************************/
/* ==================================================================== */
/*                            MSGNRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class MSGNRasterBand final : public GDALRasterBand
{
    friend class MSGNDataset;

    unsigned int packet_size;
    unsigned int bytes_per_line;
    unsigned int interline_spacing;
    unsigned int orig_band_no;  // The name of the band
    unsigned int band_in_file;  // The effective index of the band in the file
    open_mode_type open_mode;

    double GetNoDataValue(int *pbSuccess = nullptr) override
    {
        if (pbSuccess)
        {
            *pbSuccess = 1;
        }
        return MSGN_NODATA_VALUE;
    }

    double MSGN_NODATA_VALUE;

    char band_description[30];

  public:
    MSGNRasterBand(MSGNDataset *, int, open_mode_type mode, int orig_band_no,
                   int band_in_file);

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual double GetMinimum(int *pbSuccess = nullptr) override;
    virtual double GetMaximum(int *pbSuccess = nullptr) override;

    virtual const char *GetDescription() const override
    {
        return band_description;
    }
};

/************************************************************************/
/*                           MSGNRasterBand()                            */
/************************************************************************/

MSGNRasterBand::MSGNRasterBand(MSGNDataset *poDSIn, int nBandIn,
                               open_mode_type mode, int orig_band_noIn,
                               int band_in_fileIn)
    : packet_size(0), bytes_per_line(0),
      interline_spacing(poDSIn->msg_reader_core->get_interline_spacing()),
      orig_band_no(orig_band_noIn), band_in_file(band_in_fileIn),
      open_mode(mode)
{
    poDS = poDSIn;
    nBand = nBandIn;  // GDAL's band number, i.e. always starts at 1.

    snprintf(band_description, sizeof(band_description), "band %02u",
             orig_band_no);

    if (mode != MODE_RAD)
    {
        eDataType = GDT_UInt16;
        MSGN_NODATA_VALUE = 0;
    }
    else
    {
        eDataType = GDT_Float64;
        MSGN_NODATA_VALUE = -1000;
    }

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if (mode != MODE_HRV)
    {
        packet_size = poDSIn->msg_reader_core->get_visir_packet_size();
        bytes_per_line = poDSIn->msg_reader_core->get_visir_bytes_per_line();
    }
    else
    {
        packet_size = poDSIn->msg_reader_core->get_hrv_packet_size();
        bytes_per_line = poDSIn->msg_reader_core->get_hrv_bytes_per_line();
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MSGNRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                  void *pImage)

{
    MSGNDataset *poGDS = (MSGNDataset *)poDS;

    // invert y position
    const int i_nBlockYOff = poDS->GetRasterYSize() - 1 - nBlockYOff;

    const int nSamples = static_cast<int>((bytes_per_line * 8) / 10);
    if (poGDS->m_Shape == WHOLE_DISK && nRasterXSize != nSamples)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nRasterXSize %d != nSamples %d",
                 nRasterXSize, nSamples);
        return CE_Failure;
    }

    unsigned int data_length =
        bytes_per_line + (unsigned int)sizeof(SUB_VISIRLINE);
    vsi_l_offset data_offset = 0;

    if (open_mode != MODE_HRV)
    {
        data_offset =
            poGDS->msg_reader_core->get_f_data_offset() +
            static_cast<vsi_l_offset>(interline_spacing) * i_nBlockYOff +
            static_cast<vsi_l_offset>(band_in_file - 1) * packet_size +
            (packet_size - data_length);
    }
    else
    {
        data_offset =
            poGDS->msg_reader_core->get_f_data_offset() +
            static_cast<vsi_l_offset>(interline_spacing) *
                (int(i_nBlockYOff / 3) + 1) -
            static_cast<vsi_l_offset>(packet_size) * (3 - (i_nBlockYOff % 3)) +
            (packet_size - data_length);
    }

    if (VSIFSeekL(poGDS->fp, data_offset, SEEK_SET) != 0)
        return CE_Failure;

    char *pszRecord = (char *)CPLMalloc(data_length);
    size_t nread = VSIFReadL(pszRecord, 1, data_length, poGDS->fp);

    SUB_VISIRLINE *p = (SUB_VISIRLINE *)pszRecord;
    to_native(*p);

    if (p->lineValidity != 1 || poGDS->m_Shape != WHOLE_DISK)
    {  // Split lines are not full width, so NODATA all first
        for (int c = 0; c < nBlockXSize; c++)
        {
            if (open_mode != MODE_RAD)
            {
                ((GUInt16 *)pImage)[c] = (GUInt16)MSGN_NODATA_VALUE;
            }
            else
            {
                ((double *)pImage)[c] = MSGN_NODATA_VALUE;
            }
        }
    }

    if (nread != data_length ||
        (p->lineNumberInVisirGrid -
         ((open_mode == MODE_HRV && poGDS->m_Shape == RSS)
              ? (3 * poGDS->msg_reader_core->get_line_start()) - 2
              : poGDS->msg_reader_core->get_line_start())) !=
            (unsigned int)i_nBlockYOff)
    {
        CPLDebug("MSGN", "Shape %s",
                 poGDS->m_Shape == RSS
                     ? "RSS"
                     : (poGDS->m_Shape == WHOLE_DISK ? "whole" : "split HRV"));

        CPLDebug(
            "MSGN", "nread = %lu, data_len %d, linenum %d, start %d, offset %d",
            (long unsigned int)nread,  // Mingw_w64 otherwise wants %llu - MSG
                                       // read will never exceed 32 bits
            data_length, (p->lineNumberInVisirGrid),
            poGDS->msg_reader_core->get_line_start(), i_nBlockYOff);

        CPLFree(pszRecord);

        CPLError(CE_Failure, CPLE_AppDefined, "MSGN Scanline corrupt.");

        return CE_Failure;
    }

    // unpack the 10-bit values into 16-bit unsigned short ints
    unsigned char *cptr =
        (unsigned char *)pszRecord + (data_length - bytes_per_line);
    int bitsLeft = 8;

    if (open_mode != MODE_RAD)
    {
        int shift = 0;
        if (poGDS->m_Shape == SPLIT_HRV)
            shift = i_nBlockYOff < poGDS->m_nHRVSplitLine
                        ? poGDS->m_nHRVLowerShiftX
                        : poGDS->m_nHRVUpperShiftX;
        for (int c = 0; c < nSamples; c++)
        {
            unsigned short value = 0;
            for (int bit = 0; bit < 10; bit++)
            {
                value <<= 1;
                if (*cptr & 128)
                {
                    value |= 1;
                }
                *cptr <<= 1;
                bitsLeft--;
                if (bitsLeft == 0)
                {
                    cptr++;
                    bitsLeft = 8;
                }
            }
            ((GUInt16 *)pImage)[nBlockXSize - 1 - c - shift] = value;
        }
    }
    else
    {
        // radiance mode
        for (int c = 0; c < nSamples; c++)
        {
            unsigned short value = 0;
            for (int bit = 0; bit < 10; bit++)
            {
                value <<= 1;
                if (*cptr & 128)
                {
                    value |= 1;
                }
                *cptr <<= 1;
                bitsLeft--;
                if (bitsLeft == 0)
                {
                    cptr++;
                    bitsLeft = 8;
                }
            }
            double dvalue = double(value);
            double bbvalue =
                dvalue * poGDS->msg_reader_core
                             ->get_calibration_parameters()[orig_band_no - 1]
                             .cal_slope +
                poGDS->msg_reader_core
                    ->get_calibration_parameters()[orig_band_no - 1]
                    .cal_offset;

            ((double *)pImage)[nBlockXSize - 1 - c] = bbvalue;
        }
    }
    CPLFree(pszRecord);
    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/
double MSGNRasterBand::GetMinimum(int *pbSuccess)
{
    if (pbSuccess)
    {
        *pbSuccess = 1;
    }
    return open_mode != MODE_RAD ? 1 : GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/
double MSGNRasterBand::GetMaximum(int *pbSuccess)
{
    if (pbSuccess)
    {
        *pbSuccess = 1;
    }
    return open_mode != MODE_RAD ? 1023 : GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/* ==================================================================== */
/*                             MSGNDataset                             */
/* ==================================================================== */
/************************************************************************/

MSGNDataset::MSGNDataset() : fp(nullptr), msg_reader_core(nullptr)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    std::fill_n(adfGeoTransform, CPL_ARRAYSIZE(adfGeoTransform), 0);
}

/************************************************************************/
/*                            ~MSGNDataset()                             */
/************************************************************************/

MSGNDataset::~MSGNDataset()

{
    if (fp != nullptr)
        VSIFCloseL(fp);

    if (msg_reader_core)
    {
        delete msg_reader_core;
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MSGNDataset::GetGeoTransform(double *padfTransform)

{
    for (int i = 0; i < 6; i++)
    {
        padfTransform[i] = adfGeoTransform[i];
    }

    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *MSGNDataset::GetSpatialRef() const

{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MSGNDataset::Open(GDALOpenInfo *poOpenInfo)

{
    open_mode_type open_mode = MODE_VISIR;
    GDALOpenInfo *open_info = poOpenInfo;
    std::unique_ptr<GDALOpenInfo> poOpenInfoToFree;

    if (!poOpenInfo->bStatOK)
    {
        if (STARTS_WITH_CI(poOpenInfo->pszFilename, "HRV:"))
        {
            poOpenInfoToFree = std::make_unique<GDALOpenInfo>(
                &poOpenInfo->pszFilename[4], poOpenInfo->eAccess);
            open_info = poOpenInfoToFree.get();
            open_mode = MODE_HRV;
        }
        else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "RAD:"))
        {
            poOpenInfoToFree = std::make_unique<GDALOpenInfo>(
                &poOpenInfo->pszFilename[4], poOpenInfo->eAccess);
            open_info = poOpenInfoToFree.get();
            open_mode = MODE_RAD;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Before trying MSGNOpen() we first verify that there is at        */
    /*      least one "\n#keyword" type signature in the first chunk of     */
    /*      the file.                                                       */
    /* -------------------------------------------------------------------- */
    if (open_info->fpL == nullptr || open_info->nHeaderBytes < 50)
    {
        return nullptr;
    }

    /* check if this is a "NATIVE" MSG format image */
    if (!STARTS_WITH_CI((char *)open_info->pabyHeader,
                        "FormatName                  : NATIVE"))
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The MSGN driver does not support update access to existing"
                 " datasets.\n");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL(open_info->pszFilename, "rb");
    if (fp == nullptr)
    {
        return nullptr;
    }

    auto poDS = std::make_unique<MSGNDataset>();

    poDS->m_open_mode = open_mode;
    poDS->fp = fp;

    /* -------------------------------------------------------------------- */
    /*      Read the header.                                                */
    /* -------------------------------------------------------------------- */
    // first reset the file pointer, then hand over to the msg_reader_core
    CPL_IGNORE_RET_VAL(VSIFSeekL(poDS->fp, 0, SEEK_SET));

    poDS->msg_reader_core = new Msg_reader_core(poDS->fp);

    if (!poDS->msg_reader_core->get_open_success())
    {
        return nullptr;
    }

    poDS->nRasterXSize = poDS->msg_reader_core->get_columns();
    poDS->nRasterYSize = poDS->msg_reader_core->get_lines();

    if (open_mode == MODE_HRV)
    {
        const int nRawHRVColumns =
            (poDS->msg_reader_core->get_hrv_bytes_per_line() * 8) / 10;
        poDS->nRasterYSize *= 3;
        const auto &idr = poDS->msg_reader_core->get_image_description_record();
        // Check if the split layout of the HRV channel meets our expectations
        // to re-assemble it in a consistent way
        CPLDebug("MSGN", "HRV raw col %d raster X %d raster Y %d",
                 nRawHRVColumns, poDS->nRasterXSize, poDS->nRasterYSize);

        if (idr.plannedCoverage_hrv.lowerSouthLinePlanned == 1 &&
            idr.plannedCoverage_hrv.lowerNorthLinePlanned > 1 &&
            idr.plannedCoverage_hrv.lowerNorthLinePlanned <
                poDS->nRasterYSize &&
            idr.plannedCoverage_hrv.upperSouthLinePlanned ==
                idr.plannedCoverage_hrv.lowerNorthLinePlanned + 1 &&
            idr.plannedCoverage_hrv.upperNorthLinePlanned ==
                poDS->nRasterYSize &&
            idr.plannedCoverage_hrv.lowerEastColumnPlanned >= 1 &&
            idr.plannedCoverage_hrv.lowerWestColumnPlanned ==
                idr.plannedCoverage_hrv.lowerEastColumnPlanned +
                    nRawHRVColumns - 1 &&
            idr.plannedCoverage_hrv.lowerWestColumnPlanned <=
                poDS->nRasterXSize * 3 &&
            idr.plannedCoverage_hrv.upperEastColumnPlanned >= 1 &&
            idr.plannedCoverage_hrv.upperWestColumnPlanned ==
                idr.plannedCoverage_hrv.upperEastColumnPlanned +
                    nRawHRVColumns - 1 &&
            idr.plannedCoverage_hrv.upperWestColumnPlanned <=
                poDS->nRasterXSize * 3)
        {
            poDS->nRasterXSize *= 3;
            poDS->m_Shape = SPLIT_HRV;
            poDS->m_nHRVSplitLine =
                idr.plannedCoverage_hrv.upperSouthLinePlanned;
            poDS->m_nHRVLowerShiftX =
                idr.plannedCoverage_hrv.lowerEastColumnPlanned - 1;
            poDS->m_nHRVUpperShiftX =
                idr.plannedCoverage_hrv.upperEastColumnPlanned - 1;
        }
        else if (idr.plannedCoverage_hrv.upperNorthLinePlanned == 0 &&
                 idr.plannedCoverage_hrv.upperSouthLinePlanned == 0 &&
                 idr.plannedCoverage_hrv.upperWestColumnPlanned == 0 &&
                 idr.plannedCoverage_hrv.upperEastColumnPlanned ==
                     0 &&  // RSS only uses the lower section
                 idr.plannedCoverage_hrv.lowerNorthLinePlanned ==
                     idr.referencegrid_hrv.numberOfLines &&  // start at max N
                 // full expected width
                 idr.plannedCoverage_hrv.lowerWestColumnPlanned ==
                     idr.plannedCoverage_hrv.lowerEastColumnPlanned +
                         nRawHRVColumns - 1 &&
                 idr.plannedCoverage_hrv.lowerSouthLinePlanned > 1 &&
                 idr.plannedCoverage_hrv.lowerSouthLinePlanned <
                     idr.referencegrid_hrv.numberOfLines &&
                 idr.plannedCoverage_hrv.lowerEastColumnPlanned >= 1 &&
                 idr.plannedCoverage_hrv.lowerWestColumnPlanned <=
                     poDS->nRasterXSize * 3 &&
                 // full height
                 idr.plannedCoverage_hrv.lowerNorthLinePlanned ==
                     idr.plannedCoverage_hrv.lowerSouthLinePlanned +
                         poDS->nRasterYSize - 1)
        {
            poDS->nRasterXSize *= 3;
            poDS->m_Shape = RSS;
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "HRV neither Whole Disk nor RSS - don't know how to handle");
            return nullptr;
        }
    }
    else
    {
        const int nRawVisIRColumns =
            (poDS->msg_reader_core->get_visir_bytes_per_line() * 8) / 10;

        const auto &idr = poDS->msg_reader_core->get_image_description_record();
        // Check if the VisIR channel is RSS or not, and if it meets our
        // expectations to re-assemble it in a consistent way
        CPLDebug("MSGN", "raw col %d raster X %d raster Y %d", nRawVisIRColumns,
                 poDS->nRasterXSize, poDS->nRasterYSize);

        if (idr.plannedCoverage_visir.southernLinePlanned == 1 &&
            idr.plannedCoverage_visir.northernLinePlanned ==
                poDS->nRasterYSize &&
            idr.plannedCoverage_visir.easternColumnPlanned >= 1 &&
            idr.plannedCoverage_visir.westernColumnPlanned ==
                idr.plannedCoverage_visir.easternColumnPlanned +
                    nRawVisIRColumns - 1 &&
            idr.plannedCoverage_visir.westernColumnPlanned <=
                poDS->nRasterXSize)
        {
            poDS->m_Shape = WHOLE_DISK;
        }
        else if (idr.plannedCoverage_visir.northernLinePlanned ==
                     idr.referencegrid_visir.numberOfLines &&  // start at max N
                 // full expected width
                 idr.plannedCoverage_visir.westernColumnPlanned ==
                     idr.plannedCoverage_visir.easternColumnPlanned +
                         nRawVisIRColumns - 1 &&
                 idr.plannedCoverage_visir.southernLinePlanned > 1 &&
                 idr.plannedCoverage_visir.easternColumnPlanned >= 1 &&
                 idr.plannedCoverage_visir.westernColumnPlanned <=
                     poDS->nRasterXSize &&
                 // full height
                 idr.plannedCoverage_visir.northernLinePlanned ==
                     idr.plannedCoverage_visir.southernLinePlanned +
                         poDS->nRasterYSize - 1)
        {
            poDS->m_Shape = RSS;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Neither Whole Disk nor RSS - don't know how to handle");
            return nullptr;
        }
    }

    CPLDebug("MSGN", "Shape %s",
             poDS->m_Shape == RSS
                 ? "RSS"
                 : (poDS->m_Shape == WHOLE_DISK ? "whole" : "split HRV"));

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    unsigned int i;
    unsigned int band_count = 1;
    unsigned int missing_band_count = 0;
    const unsigned char *bands = poDS->msg_reader_core->get_band_map();
    unsigned char band_map[MSG_NUM_CHANNELS + 1] = {
        0};  // map GDAL band numbers to MSG channels
    for (i = 0; i < MSG_NUM_CHANNELS; i++)
    {
        if (bands[i])
        {
            bool ok_to_add = false;
            switch (open_mode)
            {
                case MODE_VISIR:
                    ok_to_add = i < MSG_NUM_CHANNELS - 1;
                    break;
                case MODE_RAD:
                    ok_to_add = (i <= 2) ||
                                (Msg_reader_core::Blackbody_LUT[i + 1].B != 0);
                    break;
                case MODE_HRV:
                    ok_to_add = i == MSG_NUM_CHANNELS - 1;
                    break;
            }
            if (ok_to_add)
            {
                poDS->SetBand(band_count,
                              new MSGNRasterBand(poDS.get(), band_count,
                                                 open_mode, i + 1,
                                                 i + 1 - missing_band_count));
                band_map[band_count] = (unsigned char)(i + 1);
                band_count++;
            }
        }
        else
        {
            missing_band_count++;
        }
    }

    double pixel_gsd_x;
    double pixel_gsd_y;
    double origin_x;
    double origin_y;

    {
        const auto &idr = poDS->msg_reader_core->get_image_description_record();
        /* there are a number of 'magic' constants below
           I trimmed them to get registration for MSG4, MSG3, MSG2 with country
           outlines  from
           http://ec.europa.eu/eurostat/web/gisco/geodata/reference-data/administrative-units-statistical-units

           Adjust in two phases P1, P2.
           I describe direction as outline being NSEW of coast shape when number
           is changed
        */

        if (open_mode != MODE_HRV)
        {
            pixel_gsd_x =
                1000.0 * poDS->msg_reader_core
                             ->get_col_dir_step();  // convert from km to m
            pixel_gsd_y =
                1000.0 * poDS->msg_reader_core
                             ->get_line_dir_step();  // convert from km to m
            origin_x = -pixel_gsd_x * (-(Conversions::nlines / 2.0) +
                                       poDS->msg_reader_core->get_col_start() -
                                       1);  // all vis/NIR E-W -ve E
            origin_y = -pixel_gsd_y * ((Conversions::nlines / 2.0) -
                                       poDS->msg_reader_core->get_line_start() +
                                       1.5);  // set with 4  N-S +ve S
        }
        else
        {
            pixel_gsd_x =
                1000.0 * poDS->msg_reader_core
                             ->get_hrv_col_dir_step();  // convert from km to m
            pixel_gsd_y =
                1000.0 * poDS->msg_reader_core
                             ->get_hrv_line_dir_step();  // convert from km to m
            if (poDS->m_Shape == RSS)
            {
                origin_x = -pixel_gsd_x *
                           (-(3 * Conversions::nlines / 2.0) -
                            idr.plannedCoverage_hrv.lowerEastColumnPlanned -
                            1);  // MSG3 HRV E-W -ve E
                origin_y = -pixel_gsd_y *
                           ((3 * Conversions::nlines / 2.0) -
                            idr.plannedCoverage_hrv.lowerSouthLinePlanned +
                            2);  //          N-S -ve S
            }
            else
            {
                origin_x =
                    -pixel_gsd_x * (-(3 * Conversions::nlines / 2.0) +
                                    1 * poDS->msg_reader_core->get_col_start() -
                                    3);  // MSG4, MSG2 HRV E-W -ve E
                origin_y = -pixel_gsd_y *
                           ((3 * Conversions::nlines / 2.0) -
                            1 * poDS->msg_reader_core->get_line_start() +
                            4);  //                N-S +ve S
            }
        }

        /* the conversion to lat/long is in two parts:
           pixels to m (around imaginary circle r=sta height) in the geo
           projection (affine transformation) geo to lat/long via the GEOS
           projection (in WKT) and the ellipsoid

           CGMS/DOC/12/0017 section 4.4.2
        */

        poDS->adfGeoTransform[0] = -origin_x;
        poDS->adfGeoTransform[1] = pixel_gsd_x;
        poDS->adfGeoTransform[2] = 0.0;

        poDS->adfGeoTransform[3] = -origin_y;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -pixel_gsd_y;

        poDS->m_oSRS.SetProjCS("Geostationary projection (MSG)");

        poDS->m_oSRS.SetGeogCS(
            "MSG Ellipsoid", "MSG_DATUM", "MSG_SPHEROID",
            Conversions::req *
                1000.0,  // SetGeogCS doesn't specify length units, so all must
                         // be the same - here m.
            1.0 / Conversions::oblate  // 1 / ( 1 -
                                       // Conversions::rpol/Conversions::req)
        );

        poDS->m_oSRS.SetGEOS(
            idr.longitudeOfSSP,
            (Conversions::altitude - Conversions::req) *
                1000.0,  // we're using meters as length unit
            0.0,
            // false northing to handle the fact RSS is only 1/3 disk
            pixel_gsd_y *
                ((poDS->m_Shape == RSS)
                     ? ((open_mode != MODE_HRV)
                            ? -(idr.plannedCoverage_visir.southernLinePlanned -
                                1)
                            :  // MSG-3 vis/NIR N-S P2
                            -(idr.plannedCoverage_hrv.lowerSouthLinePlanned +
                              1))
                     :  // MSG-3 HRV N-S P2 -ve N
                     0.0));
    }

    const CALIBRATION *cal =
        poDS->msg_reader_core->get_calibration_parameters();
    char tagname[30];
    char field[300];

    poDS->SetMetadataItem("Radiometric parameters format", "offset slope");
    for (i = 1; i < band_count; i++)
    {
        snprintf(tagname, sizeof(tagname), "ch%02u_cal", band_map[i]);
        CPLsnprintf(field, sizeof(field), "%.12e %.12e",
                    cal[band_map[i] - 1].cal_offset,
                    cal[band_map[i] - 1].cal_slope);
        poDS->SetMetadataItem(tagname, field);
    }

    snprintf(
        field, sizeof(field), "%04u%02u%02u/%02u:%02u",
        poDS->msg_reader_core->get_year(), poDS->msg_reader_core->get_month(),
        poDS->msg_reader_core->get_day(), poDS->msg_reader_core->get_hour(),
        poDS->msg_reader_core->get_minute());
    poDS->SetMetadataItem("Date/Time", field);

    snprintf(field, sizeof(field), "%u %u",
             poDS->msg_reader_core->get_line_start(),
             poDS->msg_reader_core->get_col_start());
    poDS->SetMetadataItem("Origin", field);

    return poDS.release();
}

/************************************************************************/
/*                          GDALRegister_MSGN()                         */
/************************************************************************/

void GDALRegister_MSGN()

{
    if (GDALGetDriverByName("MSGN") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MSGN");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "EUMETSAT Archive native (.nat)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/msgn.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "nat");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = MSGNDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
