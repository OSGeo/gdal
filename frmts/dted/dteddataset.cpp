/******************************************************************************
 *
 * Project:  DTED Translator
 * Purpose:  GDALDataset driver for DTED translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "dted_api.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <cstdlib>
#include <algorithm>

/************************************************************************/
/* ==================================================================== */
/*                              DTEDDataset                             */
/* ==================================================================== */
/************************************************************************/

class DTEDRasterBand;

class DTEDDataset final : public GDALPamDataset
{
    friend class DTEDRasterBand;

    char *pszFilename;
    DTEDInfo *psDTED;
    int bVerifyChecksum;
    mutable OGRSpatialReference m_oSRS{};

  public:
    DTEDDataset();
    ~DTEDDataset() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr GetGeoTransform(double *) override;

    const char *GetFileName() const
    {
        return pszFilename;
    }
    void SetFileName(const char *pszFilename);

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                            DTEDRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class DTEDRasterBand final : public GDALPamRasterBand
{
    friend class DTEDDataset;

    int bNoDataSet;
    double dfNoDataValue;

  public:
    DTEDRasterBand(DTEDDataset *, int);

    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;

    double GetNoDataValue(int *pbSuccess = nullptr) override;

    const char *GetUnitType() override
    {
        return "m";
    }
};

/************************************************************************/
/*                           DTEDRasterBand()                            */
/************************************************************************/

DTEDRasterBand::DTEDRasterBand(DTEDDataset *poDSIn, int nBandIn)
    : bNoDataSet(TRUE), dfNoDataValue(static_cast<double>(DTED_NODATA_VALUE))
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Int16;

    /* For some applications, it may be valuable to consider the whole DTED */
    /* file as single block, as the column orientation doesn't fit very well */
    /* with some scanline oriented algorithms */
    /* Of course you need to have a big enough case size, particularly for DTED
     * 2 */
    /* datasets */
    nBlockXSize =
        CPLTestBool(CPLGetConfigOption("GDAL_DTED_SINGLE_BLOCK", "NO"))
            ? poDS->GetRasterXSize()
            : 1;
    nBlockYSize = poDS->GetRasterYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DTEDRasterBand::IReadBlock(int nBlockXOff, CPL_UNUSED int nBlockYOff,
                                  void *pImage)
{
    DTEDDataset *poDTED_DS = (DTEDDataset *)poDS;
    int nYSize = poDTED_DS->psDTED->nYSize;
    GInt16 *panData;

    (void)nBlockXOff;
    CPLAssert(nBlockYOff == 0);

    if (nBlockXSize != 1)
    {
        const int cbs = 32;  // optimize for 64 byte cache line size
        const int bsy = (nBlockYSize + cbs - 1) / cbs * cbs;
        panData = (GInt16 *)pImage;
        GInt16 *panBuffer = (GInt16 *)CPLMalloc(sizeof(GInt16) * cbs * bsy);
        for (int i = 0; i < nBlockXSize; i += cbs)
        {
            int n = std::min(cbs, nBlockXSize - i);
            for (int j = 0; j < n; ++j)
            {
                if (!DTEDReadProfileEx(poDTED_DS->psDTED, i + j,
                                       panBuffer + j * bsy,
                                       poDTED_DS->bVerifyChecksum))
                {
                    CPLFree(panBuffer);
                    return CE_Failure;
                }
            }
            for (int y = 0; y < nBlockYSize; ++y)
            {
                GInt16 *dst = panData + i + (nYSize - y - 1) * nBlockXSize;
                GInt16 *src = panBuffer + y;
                for (int j = 0; j < n; ++j)
                {
                    dst[j] = src[j * bsy];
                }
            }
        }

        CPLFree(panBuffer);
        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the data.                                                  */
    /* -------------------------------------------------------------------- */
    panData = (GInt16 *)pImage;
    if (!DTEDReadProfileEx(poDTED_DS->psDTED, nBlockXOff, panData,
                           poDTED_DS->bVerifyChecksum))
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Flip line to orient it top to bottom instead of bottom to       */
    /*      top.                                                            */
    /* -------------------------------------------------------------------- */
    for (int i = nYSize / 2; i >= 0; i--)
    {
        std::swap(panData[i], panData[nYSize - i - 1]);
    }

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DTEDRasterBand::IWriteBlock(int nBlockXOff, CPL_UNUSED int nBlockYOff,
                                   void *pImage)
{
    DTEDDataset *poDTED_DS = (DTEDDataset *)poDS;
    GInt16 *panData;

    (void)nBlockXOff;
    CPLAssert(nBlockYOff == 0);

    if (poDTED_DS->eAccess != GA_Update)
        return CE_Failure;

    if (nBlockXSize != 1)
    {
        panData = (GInt16 *)pImage;
        GInt16 *panBuffer = (GInt16 *)CPLMalloc(sizeof(GInt16) * nBlockYSize);
        for (int i = 0; i < nBlockXSize; i++)
        {
            for (int j = 0; j < nBlockYSize; j++)
            {
                panBuffer[j] = panData[j * nBlockXSize + i];
            }
            if (!DTEDWriteProfile(poDTED_DS->psDTED, i, panBuffer))
            {
                CPLFree(panBuffer);
                return CE_Failure;
            }
        }

        CPLFree(panBuffer);
        return CE_None;
    }

    panData = (GInt16 *)pImage;
    if (!DTEDWriteProfile(poDTED_DS->psDTED, nBlockXOff, panData))
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double DTEDRasterBand::GetNoDataValue(int *pbSuccess)

{
    if (pbSuccess)
        *pbSuccess = bNoDataSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                            ~DTEDDataset()                            */
/************************************************************************/

DTEDDataset::DTEDDataset()
    : pszFilename(CPLStrdup("unknown")), psDTED(nullptr),
      bVerifyChecksum(
          CPLTestBool(CPLGetConfigOption("DTED_VERIFY_CHECKSUM", "NO")))
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                            ~DTEDDataset()                            */
/************************************************************************/

DTEDDataset::~DTEDDataset()

{
    FlushCache(true);
    CPLFree(pszFilename);
    if (psDTED != nullptr)
        DTEDClose(psDTED);
}

/************************************************************************/
/*                            SetFileName()                             */
/************************************************************************/

void DTEDDataset::SetFileName(const char *pszFilenameIn)

{
    CPLFree(this->pszFilename);
    this->pszFilename = CPLStrdup(pszFilenameIn);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int DTEDDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      Does the file start with one of the possible DTED header        */
    /*      record types, and do we have a UHL marker?                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 240)
        return FALSE;

    if (!STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "VOL") &&
        !STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "HDR") &&
        !STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "UHL"))
    {
        return FALSE;
    }

    bool bFoundUHL = false;
    for (int i = 0; i < poOpenInfo->nHeaderBytes - 3 && !bFoundUHL;
         i += DTED_UHL_SIZE)
    {
        if (STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader + i, "UHL"))
        {
            bFoundUHL = true;
        }
    }
    if (!bFoundUHL)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DTEDDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Try opening the dataset.                                        */
    /* -------------------------------------------------------------------- */
    VSILFILE *fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    DTEDInfo *psDTED =
        DTEDOpenEx(fp, poOpenInfo->pszFilename,
                   (poOpenInfo->eAccess == GA_Update) ? "rb+" : "rb", TRUE);

    if (psDTED == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    DTEDDataset *poDS = new DTEDDataset();
    poDS->SetFileName(poOpenInfo->pszFilename);

    poDS->eAccess = poOpenInfo->eAccess;
    poDS->psDTED = psDTED;

    /* -------------------------------------------------------------------- */
    /*      Capture some information from the file that is of interest.     */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psDTED->nXSize;
    poDS->nRasterYSize = psDTED->nYSize;

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    for (int i = 0; i < poDS->nBands; i++)
        poDS->SetBand(i + 1, new DTEDRasterBand(poDS, i + 1));

    /* -------------------------------------------------------------------- */
    /*      Collect any metadata available.                                 */
    /* -------------------------------------------------------------------- */
    char *pszValue = DTEDGetMetadata(psDTED, DTEDMD_VERTACCURACY_UHL);
    poDS->SetMetadataItem("DTED_VerticalAccuracy_UHL", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_VERTACCURACY_ACC);
    poDS->SetMetadataItem("DTED_VerticalAccuracy_ACC", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_SECURITYCODE_UHL);
    poDS->SetMetadataItem("DTED_SecurityCode_UHL", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_SECURITYCODE_DSI);
    poDS->SetMetadataItem("DTED_SecurityCode_DSI", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_UNIQUEREF_UHL);
    poDS->SetMetadataItem("DTED_UniqueRef_UHL", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_UNIQUEREF_DSI);
    poDS->SetMetadataItem("DTED_UniqueRef_DSI", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_DATA_EDITION);
    poDS->SetMetadataItem("DTED_DataEdition", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_MATCHMERGE_VERSION);
    poDS->SetMetadataItem("DTED_MatchMergeVersion", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_MAINT_DATE);
    poDS->SetMetadataItem("DTED_MaintenanceDate", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_MATCHMERGE_DATE);
    poDS->SetMetadataItem("DTED_MatchMergeDate", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_MAINT_DESCRIPTION);
    poDS->SetMetadataItem("DTED_MaintenanceDescription", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_PRODUCER);
    poDS->SetMetadataItem("DTED_Producer", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_VERTDATUM);
    poDS->SetMetadataItem("DTED_VerticalDatum", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_HORIZDATUM);
    poDS->SetMetadataItem("DTED_HorizontalDatum", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_DIGITIZING_SYS);
    poDS->SetMetadataItem("DTED_DigitizingSystem", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_COMPILATION_DATE);
    poDS->SetMetadataItem("DTED_CompilationDate", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_HORIZACCURACY);
    poDS->SetMetadataItem("DTED_HorizontalAccuracy", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_REL_HORIZACCURACY);
    poDS->SetMetadataItem("DTED_RelHorizontalAccuracy", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_REL_VERTACCURACY);
    poDS->SetMetadataItem("DTED_RelVerticalAccuracy", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_ORIGINLAT);
    poDS->SetMetadataItem("DTED_OriginLatitude", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_ORIGINLONG);
    poDS->SetMetadataItem("DTED_OriginLongitude", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_NIMA_DESIGNATOR);
    poDS->SetMetadataItem("DTED_NimaDesignator", pszValue);
    CPLFree(pszValue);

    pszValue = DTEDGetMetadata(psDTED, DTEDMD_PARTIALCELL_DSI);
    poDS->SetMetadataItem("DTED_PartialCellIndicator", pszValue);
    CPLFree(pszValue);

    poDS->SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML(poOpenInfo->GetSiblingFiles());

    // if no SR in xml, try aux
    if (poDS->GDALPamDataset::GetSpatialRef() == nullptr)
    {
        int bTryAux = TRUE;
        if (poOpenInfo->GetSiblingFiles() != nullptr &&
            CSLFindString(poOpenInfo->GetSiblingFiles(),
                          CPLResetExtension(
                              CPLGetFilename(poOpenInfo->pszFilename), "aux")) <
                0 &&
            CSLFindString(
                poOpenInfo->GetSiblingFiles(),
                CPLSPrintf("%s.aux", CPLGetFilename(poOpenInfo->pszFilename))) <
                0)
            bTryAux = FALSE;
        if (bTryAux)
        {
            GDALDataset *poAuxDS = GDALFindAssociatedAuxFile(
                poOpenInfo->pszFilename, GA_ReadOnly, poDS);
            if (poAuxDS)
            {
                const auto poSRS = poAuxDS->GetSpatialRef();
                if (poSRS)
                {
                    poDS->m_oSRS = *poSRS;
                }

                GDALClose(poAuxDS);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Support overviews.                                              */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename,
                                poOpenInfo->GetSiblingFiles());
    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DTEDDataset::GetGeoTransform(double *padfTransform)

{

    bool bApplyPixelIsPoint =
        CPLTestBool(CPLGetConfigOption("DTED_APPLY_PIXEL_IS_POINT", "FALSE"));
    if (!bApplyPixelIsPoint)
    {
        padfTransform[0] = psDTED->dfULCornerX;
        padfTransform[1] = psDTED->dfPixelSizeX;
        padfTransform[2] = 0.0;
        padfTransform[3] = psDTED->dfULCornerY;
        padfTransform[4] = 0.0;
        padfTransform[5] = psDTED->dfPixelSizeY * -1;

        return CE_None;
    }
    else
    {
        padfTransform[0] = psDTED->dfULCornerX + (0.5 * psDTED->dfPixelSizeX);
        padfTransform[1] = psDTED->dfPixelSizeX;
        padfTransform[2] = 0.0;
        padfTransform[3] = psDTED->dfULCornerY - (0.5 * psDTED->dfPixelSizeY);
        padfTransform[4] = 0.0;
        padfTransform[5] = psDTED->dfPixelSizeY * -1;

        return CE_None;
    }
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *DTEDDataset::GetSpatialRef() const

{
    if (!m_oSRS.IsEmpty())
        return &m_oSRS;

    // get xml and aux SR first
    const auto poSRS = GDALPamDataset::GetSpatialRef();
    if (poSRS)
    {
        m_oSRS = *poSRS;
        return &m_oSRS;
    }

    const char *pszVertDatum;
    const char *pszPrj = const_cast<DTEDDataset *>(this)->GetMetadataItem(
        "DTED_HorizontalDatum");
    if (EQUAL(pszPrj, "WGS84"))
    {

        pszVertDatum = const_cast<DTEDDataset *>(this)->GetMetadataItem(
            "DTED_VerticalDatum");
        if ((EQUAL(pszVertDatum, "MSL") || EQUAL(pszVertDatum, "E96")) &&
            CPLTestBool(CPLGetConfigOption("REPORT_COMPD_CS", "NO")))
        {
            m_oSRS.importFromWkt(
                "COMPD_CS[\"WGS 84 + EGM96 geoid height\", GEOGCS[\"WGS 84\", "
                "DATUM[\"WGS_1984\", SPHEROID[\"WGS "
                "84\",6378137,298.257223563, AUTHORITY[\"EPSG\",\"7030\"]], "
                "AUTHORITY[\"EPSG\",\"6326\"]], PRIMEM[\"Greenwich\",0, "
                "AUTHORITY[\"EPSG\",\"8901\"]], "
                "UNIT[\"degree\",0.0174532925199433, "
                "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Latitude\",NORTH],AXIS["
                "\"Longitude\",EAST], AUTHORITY[\"EPSG\",\"4326\"]], "
                "VERT_CS[\"EGM96 geoid height\", VERT_DATUM[\"EGM96 "
                "geoid\",2005, AUTHORITY[\"EPSG\",\"5171\"]], "
                "UNIT[\"metre\",1, AUTHORITY[\"EPSG\",\"9001\"]], "
                "AXIS[\"Up\",UP], AUTHORITY[\"EPSG\",\"5773\"]]]");
        }
        // Support DTED with EGM08 vertical datum reference
        else if ((EQUAL(pszVertDatum, "E08")) &&
                 CPLTestBool(CPLGetConfigOption("REPORT_COMPD_CS", "NO")))
        {
            m_oSRS.importFromWkt(
                "COMPD_CS[\"WGS 84 + EGM2008 height\",GEOGCS[\"WGS "
                "84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS "
                "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],"
                "AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,"
                "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
                "0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY["
                "\"EPSG\",\"4326\"]],VERT_CS[\"EGM2008 "
                "height\",VERT_DATUM[\"EGM2008 "
                "geoid\",2005,AUTHORITY[\"EPSG\",\"1027\"]],UNIT[\"metre\",1,"
                "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"Gravity-related "
                "height\",UP],AUTHORITY[\"EPSG\",\"3855\"]]]");
        }
        else
        {
            m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);
        }
    }
    else if (EQUAL(pszPrj, "WGS72"))
    {
        static bool bWarned = false;
        if (!bWarned)
        {
            bWarned = true;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "The DTED file %s indicates WGS72 as horizontal datum. \n"
                     "As this is outdated nowadays, you should contact your "
                     "data producer to get data georeferenced in WGS84.\n"
                     "In some cases, WGS72 is a wrong indication and the "
                     "georeferencing is really WGS84. In that case\n"
                     "you might consider doing 'gdal_translate -of DTED -mo "
                     "\"DTED_HorizontalDatum=WGS84\" src.dtX dst.dtX' to\n"
                     "fix the DTED file.\n"
                     "No more warnings will be issued in this session about "
                     "this operation.",
                     GetFileName());
        }
        m_oSRS.importFromWkt(
            "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS "
            "72\",6378135,298.26]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0."
            "0174532925199433],AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\","
            "EAST],AUTHORITY[\"EPSG\",\"4322\"]]");
    }
    else
    {
        static bool bWarned = false;
        if (!bWarned)
        {
            bWarned = true;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "The DTED file %s indicates %s as horizontal datum, which "
                     "is not recognized by the DTED driver. \n"
                     "The DTED driver is going to consider it as WGS84.\n"
                     "No more warnings will be issued in this session about "
                     "this operation.",
                     GetFileName(), pszPrj);
        }
        m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);
    }
    return &m_oSRS;
}

/************************************************************************/
/*                           DTEDCreateCopy()                           */
/*                                                                      */
/*      For now we will assume the input is exactly one proper          */
/*      cell.                                                           */
/************************************************************************/

static GDALDataset *DTEDCreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char ** /* papszOptions */,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData)

{
    /* -------------------------------------------------------------------- */
    /*      Some some rudimentary checks                                    */
    /* -------------------------------------------------------------------- */
    const int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "DTED driver does not support source dataset with zero band.\n");
        return nullptr;
    }

    if (nBands != 1)
    {
        CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                 "DTED driver only uses the first band of the dataset.\n");
        if (bStrict)
            return nullptr;
    }

    if (pfnProgress && !pfnProgress(0.0, nullptr, pProgressData))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Work out the level.                                             */
    /* -------------------------------------------------------------------- */
    int nLevel;

    if (poSrcDS->GetRasterYSize() == 121)
        nLevel = 0;
    else if (poSrcDS->GetRasterYSize() == 1201)
        nLevel = 1;
    else if (poSrcDS->GetRasterYSize() == 3601)
        nLevel = 2;
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The source does not appear to be a properly formatted cell.");
        nLevel = 1;
    }

    /* -------------------------------------------------------------------- */
    /*      Checks the input SRS                                            */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference ogrsr_input;
    ogrsr_input.importFromWkt(poSrcDS->GetProjectionRef());
    OGRSpatialReference ogrsr_wgs84;
    ogrsr_wgs84.SetWellKnownGeogCS("WGS84");
    if (ogrsr_input.IsSameGeogCS(&ogrsr_wgs84) == FALSE)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The source projection coordinate system is %s. Only WGS 84 "
                 "is supported.\n"
                 "The DTED driver will generate a file as if the source was "
                 "WGS 84 projection coordinate system.",
                 poSrcDS->GetProjectionRef());
    }

    /* -------------------------------------------------------------------- */
    /*      Work out the LL origin.                                         */
    /* -------------------------------------------------------------------- */
    double adfGeoTransform[6];

    poSrcDS->GetGeoTransform(adfGeoTransform);

    int nLLOriginLat =
        (int)floor(adfGeoTransform[3] +
                   poSrcDS->GetRasterYSize() * adfGeoTransform[5] + 0.5);

    int nLLOriginLong = (int)floor(adfGeoTransform[0] + 0.5);

    if (fabs(nLLOriginLat -
             (adfGeoTransform[3] + (poSrcDS->GetRasterYSize() - 0.5) *
                                       adfGeoTransform[5])) > 1e-10 ||
        fabs(nLLOriginLong - (adfGeoTransform[0] + 0.5 * adfGeoTransform[1])) >
            1e-10)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The corner coordinates of the source are not properly "
                 "aligned on plain latitude/longitude boundaries.");
    }

    /* -------------------------------------------------------------------- */
    /*     Check horizontal source size.                                    */
    /* -------------------------------------------------------------------- */
    int expectedXSize;
    int nReferenceLat = nLLOriginLat < 0 ? -(nLLOriginLat + 1) : nLLOriginLat;
    if (nReferenceLat >= 80)
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 6 + 1;
    else if (nReferenceLat >= 75)
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 4 + 1;
    else if (nReferenceLat >= 70)
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 3 + 1;
    else if (nReferenceLat >= 50)
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 2 + 1;
    else
        expectedXSize = poSrcDS->GetRasterYSize();

    if (poSrcDS->GetRasterXSize() != expectedXSize)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The horizontal source size is not conformant with the one "
                 "expected by DTED Level %d at this latitude (%d pixels found "
                 "instead of %d).",
                 nLevel, poSrcDS->GetRasterXSize(), expectedXSize);
    }

    /* -------------------------------------------------------------------- */
    /*      Create the output dted file.                                    */
    /* -------------------------------------------------------------------- */
    const char *pszError =
        DTEDCreate(pszFilename, nLevel, nLLOriginLat, nLLOriginLong);

    if (pszError != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", pszError);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Open the DTED file so we can output the data to it.             */
    /* -------------------------------------------------------------------- */
    DTEDInfo *psDTED = DTEDOpen(pszFilename, "rb+", FALSE);
    if (psDTED == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Read all the data in a single buffer.                           */
    /* -------------------------------------------------------------------- */
    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(1);
    GInt16 *panData = (GInt16 *)VSI_MALLOC_VERBOSE(
        sizeof(GInt16) * psDTED->nXSize * psDTED->nYSize);
    if (panData == nullptr)
    {
        DTEDClose(psDTED);
        return nullptr;
    }

    for (int iY = 0; iY < psDTED->nYSize; iY++)
    {
        if (poSrcBand->RasterIO(GF_Read, 0, iY, psDTED->nXSize, 1,
                                (void *)(panData + iY * psDTED->nXSize),
                                psDTED->nXSize, 1, GDT_Int16, 0, 0,
                                nullptr) != CE_None)
        {
            DTEDClose(psDTED);
            CPLFree(panData);
            return nullptr;
        }

        if (pfnProgress && !pfnProgress(0.5 * (iY + 1) / (double)psDTED->nYSize,
                                        nullptr, pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated CreateCopy()");
            DTEDClose(psDTED);
            CPLFree(panData);
            return nullptr;
        }
    }

    int bSrcBandHasNoData;
    double srcBandNoData = poSrcBand->GetNoDataValue(&bSrcBandHasNoData);

    /* -------------------------------------------------------------------- */
    /*      Write all the profiles.                                         */
    /* -------------------------------------------------------------------- */
    GInt16 anProfData[3601];
    int dfNodataCount = 0;
    GByte iPartialCell;

    for (int iProfile = 0; iProfile < psDTED->nXSize; iProfile++)
    {
        for (int iY = 0; iY < psDTED->nYSize; iY++)
        {
            anProfData[iY] = panData[iProfile + iY * psDTED->nXSize];
            if (bSrcBandHasNoData && anProfData[iY] == srcBandNoData)
            {
                anProfData[iY] = DTED_NODATA_VALUE;
                dfNodataCount++;
            }
            else if (anProfData[iY] == DTED_NODATA_VALUE)
                dfNodataCount++;
        }
        DTEDWriteProfile(psDTED, iProfile, anProfData);

        if (pfnProgress &&
            !pfnProgress(0.5 + 0.5 * (iProfile + 1) / (double)psDTED->nXSize,
                         nullptr, pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated CreateCopy()");
            DTEDClose(psDTED);
            CPLFree(panData);
            return nullptr;
        }
    }
    CPLFree(panData);

    /* -------------------------------------------------------------------- */
    /* Partial cell indicator: 0 for complete coverage; 1-99 for incomplete */
    /* -------------------------------------------------------------------- */
    char szPartialCell[3];

    if (dfNodataCount == 0)
        iPartialCell = 0;
    else
    {
        iPartialCell =
            (GByte) int(floor(100.0 - (dfNodataCount * 100.0 /
                                       (psDTED->nXSize * psDTED->nYSize))));
        if (iPartialCell < 1)
            iPartialCell = 1;
    }

    CPLsnprintf(szPartialCell, sizeof(szPartialCell), "%02d", iPartialCell);
    DTEDSetMetadata(psDTED, DTEDMD_PARTIALCELL_DSI, szPartialCell);

    /* -------------------------------------------------------------------- */
    /*      Try to copy any matching available metadata.                    */
    /* -------------------------------------------------------------------- */
    if (poSrcDS->GetMetadataItem("DTED_VerticalAccuracy_UHL") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_VERTACCURACY_UHL,
                        poSrcDS->GetMetadataItem("DTED_VerticalAccuracy_UHL"));

    if (poSrcDS->GetMetadataItem("DTED_VerticalAccuracy_ACC") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_VERTACCURACY_ACC,
                        poSrcDS->GetMetadataItem("DTED_VerticalAccuracy_ACC"));

    if (poSrcDS->GetMetadataItem("DTED_SecurityCode_UHL") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_SECURITYCODE_UHL,
                        poSrcDS->GetMetadataItem("DTED_SecurityCode_UHL"));

    if (poSrcDS->GetMetadataItem("DTED_SecurityCode_DSI") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_SECURITYCODE_DSI,
                        poSrcDS->GetMetadataItem("DTED_SecurityCode_DSI"));

    if (poSrcDS->GetMetadataItem("DTED_UniqueRef_UHL") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_UNIQUEREF_UHL,
                        poSrcDS->GetMetadataItem("DTED_UniqueRef_UHL"));

    if (poSrcDS->GetMetadataItem("DTED_UniqueRef_DSI") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_UNIQUEREF_DSI,
                        poSrcDS->GetMetadataItem("DTED_UniqueRef_DSI"));

    if (poSrcDS->GetMetadataItem("DTED_DataEdition") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_DATA_EDITION,
                        poSrcDS->GetMetadataItem("DTED_DataEdition"));

    if (poSrcDS->GetMetadataItem("DTED_MatchMergeVersion") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_MATCHMERGE_VERSION,
                        poSrcDS->GetMetadataItem("DTED_MatchMergeVersion"));

    if (poSrcDS->GetMetadataItem("DTED_MaintenanceDate") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_MAINT_DATE,
                        poSrcDS->GetMetadataItem("DTED_MaintenanceDate"));

    if (poSrcDS->GetMetadataItem("DTED_MatchMergeDate") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_MATCHMERGE_DATE,
                        poSrcDS->GetMetadataItem("DTED_MatchMergeDate"));

    if (poSrcDS->GetMetadataItem("DTED_MaintenanceDescription") != nullptr)
        DTEDSetMetadata(
            psDTED, DTEDMD_MAINT_DESCRIPTION,
            poSrcDS->GetMetadataItem("DTED_MaintenanceDescription"));

    if (poSrcDS->GetMetadataItem("DTED_Producer") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_PRODUCER,
                        poSrcDS->GetMetadataItem("DTED_Producer"));

    if (poSrcDS->GetMetadataItem("DTED_VerticalDatum") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_VERTDATUM,
                        poSrcDS->GetMetadataItem("DTED_VerticalDatum"));

    if (poSrcDS->GetMetadataItem("DTED_HorizontalDatum") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_HORIZDATUM,
                        poSrcDS->GetMetadataItem("DTED_HorizontalDatum"));

    if (poSrcDS->GetMetadataItem("DTED_DigitizingSystem") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_DIGITIZING_SYS,
                        poSrcDS->GetMetadataItem("DTED_DigitizingSystem"));

    if (poSrcDS->GetMetadataItem("DTED_CompilationDate") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_COMPILATION_DATE,
                        poSrcDS->GetMetadataItem("DTED_CompilationDate"));

    if (poSrcDS->GetMetadataItem("DTED_HorizontalAccuracy") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_HORIZACCURACY,
                        poSrcDS->GetMetadataItem("DTED_HorizontalAccuracy"));

    if (poSrcDS->GetMetadataItem("DTED_RelHorizontalAccuracy") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_REL_HORIZACCURACY,
                        poSrcDS->GetMetadataItem("DTED_RelHorizontalAccuracy"));

    if (poSrcDS->GetMetadataItem("DTED_RelVerticalAccuracy") != nullptr)
        DTEDSetMetadata(psDTED, DTEDMD_REL_VERTACCURACY,
                        poSrcDS->GetMetadataItem("DTED_RelVerticalAccuracy"));

    /* -------------------------------------------------------------------- */
    /*      Try to open the resulting DTED file.                            */
    /* -------------------------------------------------------------------- */
    DTEDClose(psDTED);

    /* -------------------------------------------------------------------- */
    /*      Reopen and copy missing information into a PAM file.            */
    /* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = (GDALPamDataset *)GDALOpen(pszFilename, GA_ReadOnly);

    if (poDS)
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_DTED()                          */
/************************************************************************/

void GDALRegister_DTED()

{
    if (GDALGetDriverByName("DTED") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("DTED");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "DTED Elevation Raster");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "dt0 dt1 dt2");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/dted.html");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int16 UInt16");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = DTEDDataset::Open;
    poDriver->pfnIdentify = DTEDDataset::Identify;
    poDriver->pfnCreateCopy = DTEDCreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
