/******************************************************************************
 *
 * Purpose:  ADRG reader
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_pam.h"
#include "gdal_frmts.h"
#include "iso8211.h"
#include "ogr_spatialref.h"

#include <limits>
#include <new>

#define N_ELEMENTS(x) (sizeof(x) / sizeof(x[0]))

#define DIGIT_ZERO '0'

class ADRGDataset final : public GDALPamDataset
{
    friend class ADRGRasterBand;

    CPLString osGENFileName;
    CPLString osIMGFileName;
    OGRSpatialReference m_oSRS{};

    VSILFILE *fdIMG;
    int *TILEINDEX;
    int offsetInIMG;
    int NFC;
    int NFL;
    double LSO;
    double PSO;
    int ARV;
    int BRV;

    char **papszSubDatasets;

    double adfGeoTransform[6];

    static char **GetGENListFromTHF(const char *pszFileName);
    static char **GetIMGListFromGEN(const char *pszFileName,
                                    int *pnRecordIndex = nullptr);
    static ADRGDataset *OpenDataset(const char *pszGENFileName,
                                    const char *pszIMGFileName,
                                    DDFRecord *record = nullptr);
    static DDFRecord *FindRecordInGENForIMG(DDFModule &module,
                                            const char *pszGENFileName,
                                            const char *pszIMGFileName);

  public:
    ADRGDataset();
    ~ADRGDataset() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr GetGeoTransform(double *padfGeoTransform) override;

    char **GetMetadataDomainList() override;
    char **GetMetadata(const char *pszDomain = "") override;

    char **GetFileList() override;

    void AddSubDataset(const char *pszGENFileName, const char *pszIMGFileName);

    static GDALDataset *Open(GDALOpenInfo *);

    static double GetLongitudeFromString(const char *str);
    static double GetLatitudeFromString(const char *str);
};

/************************************************************************/
/* ==================================================================== */
/*                            ADRGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class ADRGRasterBand final : public GDALPamRasterBand
{
    friend class ADRGDataset;

  public:
    ADRGRasterBand(ADRGDataset *, int);

    GDALColorInterp GetColorInterpretation() override;
    CPLErr IReadBlock(int, int, void *) override;

    double GetNoDataValue(int *pbSuccess = nullptr) override;

    // virtual int GetOverviewCount();
    // virtual GDALRasterBand* GetOverview(int i);
};

/************************************************************************/
/*                           ADRGRasterBand()                            */
/************************************************************************/

ADRGRasterBand::ADRGRasterBand(ADRGDataset *poDSIn, int nBandIn)

{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Byte;

    nBlockXSize = 128;
    nBlockYSize = 128;
}

/************************************************************************/
/*                            GetNoDataValue()                          */
/************************************************************************/

double ADRGRasterBand::GetNoDataValue(int *pbSuccess)
{
    if (pbSuccess)
        *pbSuccess = TRUE;

    return 0.0;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp ADRGRasterBand::GetColorInterpretation()

{
    if (nBand == 1)
        return GCI_RedBand;

    else if (nBand == 2)
        return GCI_GreenBand;

    return GCI_BlueBand;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ADRGRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    ADRGDataset *l_poDS = (ADRGDataset *)this->poDS;
    int nBlock = nBlockYOff * l_poDS->NFC + nBlockXOff;
    if (nBlockXOff >= l_poDS->NFC || nBlockYOff >= l_poDS->NFL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "nBlockXOff=%d, NFC=%d, nBlockYOff=%d, NFL=%d", nBlockXOff,
                 l_poDS->NFC, nBlockYOff, l_poDS->NFL);
        return CE_Failure;
    }
    CPLDebug("ADRG", "(%d,%d) -> nBlock = %d", nBlockXOff, nBlockYOff, nBlock);

    vsi_l_offset offset;
    if (l_poDS->TILEINDEX)
    {
        if (l_poDS->TILEINDEX[nBlock] <= 0)
        {
            memset(pImage, 0, 128 * 128);
            return CE_None;
        }
        offset = l_poDS->offsetInIMG +
                 static_cast<vsi_l_offset>(l_poDS->TILEINDEX[nBlock] - 1) *
                     128 * 128 * 3 +
                 (nBand - 1) * 128 * 128;
    }
    else
        offset = l_poDS->offsetInIMG +
                 static_cast<vsi_l_offset>(nBlock) * 128 * 128 * 3 +
                 (nBand - 1) * 128 * 128;

    if (VSIFSeekL(l_poDS->fdIMG, offset, SEEK_SET) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot seek to offset " CPL_FRMT_GUIB, offset);
        return CE_Failure;
    }
    if (VSIFReadL(pImage, 1, 128 * 128, l_poDS->fdIMG) != 128 * 128)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot read data at offset " CPL_FRMT_GUIB, offset);
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          ADRGDataset()                               */
/************************************************************************/

ADRGDataset::ADRGDataset()
    : fdIMG(nullptr), TILEINDEX(nullptr), offsetInIMG(0), NFC(0), NFL(0),
      LSO(0.0), PSO(0.0), ARV(0), BRV(0), papszSubDatasets(nullptr)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    memset(adfGeoTransform, 0, sizeof(adfGeoTransform));
}

/************************************************************************/
/*                          ~ADRGDataset()                              */
/************************************************************************/

ADRGDataset::~ADRGDataset()
{
    CSLDestroy(papszSubDatasets);

    if (fdIMG)
    {
        VSIFCloseL(fdIMG);
    }

    if (TILEINDEX)
    {
        delete[] TILEINDEX;
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ADRGDataset::GetFileList()
{
    char **papszFileList = GDALPamDataset::GetFileList();

    if (!osGENFileName.empty() && !osIMGFileName.empty())
    {
        CPLString osMainFilename = GetDescription();
        VSIStatBufL sStat;

        const bool bMainFileReal = VSIStatL(osMainFilename, &sStat) == 0;
        if (bMainFileReal)
        {
            CPLString osShortMainFilename = CPLGetFilename(osMainFilename);
            CPLString osShortGENFileName = CPLGetFilename(osGENFileName);
            if (!EQUAL(osShortMainFilename.c_str(), osShortGENFileName.c_str()))
                papszFileList =
                    CSLAddString(papszFileList, osGENFileName.c_str());
        }
        else
            papszFileList = CSLAddString(papszFileList, osGENFileName.c_str());

        papszFileList = CSLAddString(papszFileList, osIMGFileName.c_str());
    }

    return papszFileList;
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void ADRGDataset::AddSubDataset(const char *pszGENFileName,
                                const char *pszIMGFileName)
{
    char szName[80];
    int nCount = CSLCount(papszSubDatasets) / 2;

    CPLString osSubDatasetName;
    osSubDatasetName = "ADRG:";
    osSubDatasetName += pszGENFileName;
    osSubDatasetName += ",";
    osSubDatasetName += pszIMGFileName;

    snprintf(szName, sizeof(szName), "SUBDATASET_%d_NAME", nCount + 1);
    papszSubDatasets =
        CSLSetNameValue(papszSubDatasets, szName, osSubDatasetName);

    snprintf(szName, sizeof(szName), "SUBDATASET_%d_DESC", nCount + 1);
    papszSubDatasets =
        CSLSetNameValue(papszSubDatasets, szName, osSubDatasetName);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **ADRGDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE, "SUBDATASETS", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **ADRGDataset::GetMetadata(const char *pszDomain)

{
    if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                        GetSpatialRef()                               */
/************************************************************************/

const OGRSpatialReference *ADRGDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                        GetGeoTransform()                             */
/************************************************************************/

CPLErr ADRGDataset::GetGeoTransform(double *padfGeoTransform)
{
    if (papszSubDatasets != nullptr)
        return CE_Failure;

    memcpy(padfGeoTransform, adfGeoTransform, sizeof(double) * 6);

    return CE_None;
}

/************************************************************************/
/*                     GetLongitudeFromString()                         */
/************************************************************************/

double ADRGDataset::GetLongitudeFromString(const char *str)
{
    char ddd[3 + 1] = {0};
    char mm[2 + 1] = {0};
    char ssdotss[5 + 1] = {0};
    int sign = (str[0] == '+') ? 1 : -1;
    str++;
    strncpy(ddd, str, 3);
    str += 3;
    strncpy(mm, str, 2);
    str += 2;
    strncpy(ssdotss, str, 5);
    return sign * (CPLAtof(ddd) + CPLAtof(mm) / 60 + CPLAtof(ssdotss) / 3600);
}

/************************************************************************/
/*                      GetLatitudeFromString()                         */
/************************************************************************/

double ADRGDataset::GetLatitudeFromString(const char *str)
{
    char ddd[2 + 1] = {0};
    char mm[2 + 1] = {0};
    char ssdotss[5 + 1] = {0};
    int sign = (str[0] == '+') ? 1 : -1;
    str++;
    strncpy(ddd, str, 2);
    str += 2;
    strncpy(mm, str, 2);
    str += 2;
    strncpy(ssdotss, str, 5);
    return sign * (CPLAtof(ddd) + CPLAtof(mm) / 60 + CPLAtof(ssdotss) / 3600);
}

/************************************************************************/
/*                      FindRecordInGENForIMG()                         */
/************************************************************************/

DDFRecord *ADRGDataset::FindRecordInGENForIMG(DDFModule &module,
                                              const char *pszGENFileName,
                                              const char *pszIMGFileName)
{
    /* Finds the GEN file corresponding to the IMG file */
    if (!module.Open(pszGENFileName, TRUE))
        return nullptr;

    CPLString osShortIMGFilename = CPLGetFilename(pszIMGFileName);

    /* Now finds the record */
    while (true)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        DDFRecord *record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == nullptr)
            return nullptr;

        if (record->GetFieldCount() >= 5)
        {
            DDFField *field = record->GetField(0);
            DDFFieldDefn *fieldDefn = field->GetFieldDefn();
            if (!(strcmp(fieldDefn->GetName(), "001") == 0 &&
                  fieldDefn->GetSubfieldCount() == 2))
            {
                continue;
            }

            const char *RTY = record->GetStringSubfield("001", 0, "RTY", 0);
            if (RTY == nullptr)
                continue;
            /* Ignore overviews */
            if (strcmp(RTY, "OVV") == 0)
                continue;

            if (strcmp(RTY, "GIN") != 0)
                continue;

            field = record->GetField(3);
            fieldDefn = field->GetFieldDefn();

            if (!(strcmp(fieldDefn->GetName(), "SPR") == 0 &&
                  fieldDefn->GetSubfieldCount() == 15))
            {
                continue;
            }

            const char *pszBAD = record->GetStringSubfield("SPR", 0, "BAD", 0);
            if (pszBAD == nullptr || strlen(pszBAD) != 12)
                continue;
            CPLString osBAD = pszBAD;
            {
                char *c = (char *)strchr(osBAD.c_str(), ' ');
                if (c)
                    *c = 0;
            }

            if (EQUAL(osShortIMGFilename.c_str(), osBAD.c_str()))
            {
                return record;
            }
        }
    }
}

/************************************************************************/
/*                           OpenDataset()                              */
/************************************************************************/

ADRGDataset *ADRGDataset::OpenDataset(const char *pszGENFileName,
                                      const char *pszIMGFileName,
                                      DDFRecord *record)
{
    DDFModule module;

    int SCA = 0;
    int ZNA = 0;
    double PSP;
    int ARV;
    int BRV;
    double LSO;
    double PSO;
    int NFL;
    int NFC;
    CPLString osBAD;
    int TIF;
    int *TILEINDEX = nullptr;

    if (record == nullptr)
    {
        record = FindRecordInGENForIMG(module, pszGENFileName, pszIMGFileName);
        if (record == nullptr)
            return nullptr;
    }

    DDFField *field = record->GetField(1);
    if (field == nullptr)
        return nullptr;
    DDFFieldDefn *fieldDefn = field->GetFieldDefn();

    if (!(strcmp(fieldDefn->GetName(), "DSI") == 0 &&
          fieldDefn->GetSubfieldCount() == 2))
    {
        return nullptr;
    }

    const char *pszPTR = record->GetStringSubfield("DSI", 0, "PRT", 0);
    if (pszPTR == nullptr || !EQUAL(pszPTR, "ADRG"))
        return nullptr;

    const char *pszNAM = record->GetStringSubfield("DSI", 0, "NAM", 0);
    if (pszNAM == nullptr || strlen(pszNAM) != 8)
        return nullptr;
    CPLString osNAM = pszNAM;

    field = record->GetField(2);
    if (field == nullptr)
        return nullptr;
    fieldDefn = field->GetFieldDefn();

    // TODO: Support on GIN things.  And what is GIN?
    // GIN might mean general information and might be a typo of GEN.
    // if( isGIN )
    {
        if (!(strcmp(fieldDefn->GetName(), "GEN") == 0 &&
              fieldDefn->GetSubfieldCount() == 21))
        {
            return nullptr;
        }

        if (record->GetIntSubfield("GEN", 0, "STR", 0) != 3)
            return nullptr;

        SCA = record->GetIntSubfield("GEN", 0, "SCA", 0);
        CPLDebug("ADRG", "SCA=%d", SCA);

        ZNA = record->GetIntSubfield("GEN", 0, "ZNA", 0);
        CPLDebug("ADRG", "ZNA=%d", ZNA);

        PSP = record->GetFloatSubfield("GEN", 0, "PSP", 0);
        CPLDebug("ADRG", "PSP=%f", PSP);

        ARV = record->GetIntSubfield("GEN", 0, "ARV", 0);
        CPLDebug("ADRG", "ARV=%d", ARV);

        BRV = record->GetIntSubfield("GEN", 0, "BRV", 0);
        CPLDebug("ADRG", "BRV=%d", BRV);
        if (ARV <= 0 || (ZNA != 9 && ZNA != 18 && BRV <= 0))
            return nullptr;

        const char *pszLSO = record->GetStringSubfield("GEN", 0, "LSO", 0);
        if (pszLSO == nullptr || strlen(pszLSO) != 11)
            return nullptr;
        LSO = GetLongitudeFromString(pszLSO);
        CPLDebug("ADRG", "LSO=%f", LSO);

        const char *pszPSO = record->GetStringSubfield("GEN", 0, "PSO", 0);
        if (pszPSO == nullptr || strlen(pszPSO) != 10)
            return nullptr;
        PSO = GetLatitudeFromString(pszPSO);
        CPLDebug("ADRG", "PSO=%f", PSO);
    }
#if 0
    else
    {
        if( !(strcmp(fieldDefn->GetName(), "OVI") == 0 &&
                fieldDefn->GetSubfieldCount() == 5) )
        {
            return NULL;
        }

        if( record->GetIntSubfield("OVI", 0, "STR", 0) != 3 )
            return NULL;

        ARV = record->GetIntSubfield("OVI", 0, "ARV", 0);
        CPLDebug("ADRG", "ARV=%d", ARV);

        BRV = record->GetIntSubfield("OVI", 0, "BRV", 0);
        CPLDebug("ADRG", "BRV=%d", BRV);

        const char* pszLSO = record->GetStringSubfield("OVI", 0, "LSO", 0);
        if( pszLSO == NULL || strlen(pszLSO) != 11 )
            return NULL;
        LSO = GetLongitudeFromString(pszLSO);
        CPLDebug("ADRG", "LSO=%f", LSO);

        const char* pszPSO = record->GetStringSubfield("OVI", 0, "PSO", 0);
        if( pszPSO == NULL || strlen(pszPSO) != 10 )
            return NULL;
        PSO = GetLatitudeFromString(pszPSO);
        CPLDebug("ADRG", "PSO=%f", PSO);
    }
#endif

    field = record->GetField(3);
    if (field == nullptr)
        return nullptr;
    fieldDefn = field->GetFieldDefn();

    if (!(strcmp(fieldDefn->GetName(), "SPR") == 0 &&
          fieldDefn->GetSubfieldCount() == 15))
    {
        return nullptr;
    }

    NFL = record->GetIntSubfield("SPR", 0, "NFL", 0);
    CPLDebug("ADRG", "NFL=%d", NFL);

    NFC = record->GetIntSubfield("SPR", 0, "NFC", 0);
    CPLDebug("ADRG", "NFC=%d", NFC);

    const auto knIntMax = std::numeric_limits<int>::max();
    if (NFL <= 0 || NFC <= 0 || NFL > knIntMax / 128 || NFC > knIntMax / 128 ||
        NFL > (knIntMax - 1) / (NFC * 5))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid NFL / NFC values");
        return nullptr;
    }

    int PNC = record->GetIntSubfield("SPR", 0, "PNC", 0);
    CPLDebug("ADRG", "PNC=%d", PNC);
    if (PNC != 128)
    {
        return nullptr;
    }

    int PNL = record->GetIntSubfield("SPR", 0, "PNL", 0);
    CPLDebug("ADRG", "PNL=%d", PNL);
    if (PNL != 128)
    {
        return nullptr;
    }

    const char *pszBAD = record->GetStringSubfield("SPR", 0, "BAD", 0);
    if (pszBAD == nullptr || strlen(pszBAD) != 12)
        return nullptr;
    osBAD = pszBAD;
    {
        char *c = (char *)strchr(osBAD.c_str(), ' ');
        if (c)
            *c = 0;
    }
    CPLDebug("ADRG", "BAD=%s", osBAD.c_str());

    DDFSubfieldDefn *subfieldDefn = fieldDefn->GetSubfield(14);
    if (!(strcmp(subfieldDefn->GetName(), "TIF") == 0 &&
          (subfieldDefn->GetFormat())[0] == 'A'))
    {
        return nullptr;
    }

    const char *pszTIF = record->GetStringSubfield("SPR", 0, "TIF", 0);
    if (pszTIF == nullptr)
        return nullptr;
    TIF = pszTIF[0] == 'Y';
    CPLDebug("ADRG", "TIF=%d", TIF);

    if (TIF)
    {
        if (record->GetFieldCount() != 6)
        {
            return nullptr;
        }

        field = record->GetField(5);
        if (field == nullptr)
            return nullptr;
        fieldDefn = field->GetFieldDefn();

        if (!(strcmp(fieldDefn->GetName(), "TIM") == 0))
        {
            return nullptr;
        }

        if (field->GetDataSize() != 5 * NFL * NFC + 1)
        {
            return nullptr;
        }

        try
        {
            TILEINDEX = new int[NFL * NFC];
        }
        catch (const std::exception &)
        {
            return nullptr;
        }
        const char *ptr = field->GetData();
        char offset[5 + 1] = {0};
        for (int i = 0; i < NFL * NFC; i++)
        {
            strncpy(offset, ptr, 5);
            ptr += 5;
            TILEINDEX[i] = atoi(offset);
            // CPLDebug("ADRG", "TSI[%d]=%d", i, TILEINDEX[i]);
        }
    }

    VSILFILE *fdIMG = VSIFOpenL(pszIMGFileName, "rb");
    if (fdIMG == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s\n",
                 pszIMGFileName);
        delete[] TILEINDEX;
        return nullptr;
    }

    /* Skip ISO8211 header of IMG file */
    int offsetInIMG = 0;
    char c;
    char recordName[3];
    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
    {
        VSIFCloseL(fdIMG);
        delete[] TILEINDEX;
        return nullptr;
    }
    while (!VSIFEofL(fdIMG))
    {
        if (c == 30)
        {
            if (VSIFReadL(recordName, 1, 3, fdIMG) != 3)
            {
                VSIFCloseL(fdIMG);
                delete[] TILEINDEX;
                return nullptr;
            }
            offsetInIMG += 3;
            if (STARTS_WITH(recordName, "IMG"))
            {
                offsetInIMG += 4;
                if (VSIFSeekL(fdIMG, 3, SEEK_CUR) != 0 ||
                    VSIFReadL(&c, 1, 1, fdIMG) != 1)
                {
                    VSIFCloseL(fdIMG);
                    delete[] TILEINDEX;
                    return nullptr;
                }
                while (c == ' ')
                {
                    offsetInIMG++;
                    if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
                    {
                        VSIFCloseL(fdIMG);
                        delete[] TILEINDEX;
                        return nullptr;
                    }
                }
                offsetInIMG++;
                break;
            }
        }

        offsetInIMG++;
        if (VSIFReadL(&c, 1, 1, fdIMG) != 1)
        {
            VSIFCloseL(fdIMG);
            delete[] TILEINDEX;
            return nullptr;
        }
    }

    if (VSIFEofL(fdIMG))
    {
        VSIFCloseL(fdIMG);
        delete[] TILEINDEX;
        return nullptr;
    }

    CPLDebug("ADRG", "Img offset data = %d", offsetInIMG);

    ADRGDataset *poDS = new ADRGDataset();

    poDS->osGENFileName = pszGENFileName;
    poDS->osIMGFileName = pszIMGFileName;
    poDS->NFC = NFC;
    poDS->NFL = NFL;
    poDS->nRasterXSize = NFC * 128;
    poDS->nRasterYSize = NFL * 128;
    poDS->LSO = LSO;
    poDS->PSO = PSO;
    poDS->ARV = ARV;
    poDS->BRV = BRV;
    poDS->TILEINDEX = TILEINDEX;
    poDS->fdIMG = fdIMG;
    poDS->offsetInIMG = offsetInIMG;

    if (ZNA == 9)
    {
        // North Polar Case
        poDS->adfGeoTransform[0] =
            111319.4907933 * (90.0 - PSO) * sin(LSO * M_PI / 180.0);
        poDS->adfGeoTransform[1] = 40075016.68558 / ARV;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] =
            -111319.4907933 * (90.0 - PSO) * cos(LSO * M_PI / 180.0);
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -40075016.68558 / ARV;
        poDS->m_oSRS.importFromWkt(
            "PROJCS[\"ARC_System_Zone_09\",GEOGCS[\"GCS_Sphere\","
            "DATUM[\"D_Sphere\",SPHEROID[\"Sphere\",6378137.0,0.0]],"
            "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
            "PROJECTION[\"Azimuthal_Equidistant\"],"
            "PARAMETER[\"latitude_of_center\",90],"
            "PARAMETER[\"longitude_of_center\",0],"
            "PARAMETER[\"false_easting\",0],"
            "PARAMETER[\"false_northing\",0],"
            "UNIT[\"metre\",1]]");
    }
    else if (ZNA == 18)
    {
        // South Polar Case
        poDS->adfGeoTransform[0] =
            111319.4907933 * (90.0 + PSO) * sin(LSO * M_PI / 180.0);
        poDS->adfGeoTransform[1] = 40075016.68558 / ARV;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] =
            111319.4907933 * (90.0 + PSO) * cos(LSO * M_PI / 180.0);
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -40075016.68558 / ARV;
        poDS->m_oSRS.importFromWkt(
            "PROJCS[\"ARC_System_Zone_18\",GEOGCS[\"GCS_Sphere\","
            "DATUM[\"D_Sphere\",SPHEROID[\"Sphere\",6378137.0,0.0]],"
            "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
            "PROJECTION[\"Azimuthal_Equidistant\"],"
            "PARAMETER[\"latitude_of_center\",-90],"
            "PARAMETER[\"longitude_of_center\",0],"
            "PARAMETER[\"false_easting\",0],"
            "PARAMETER[\"false_northing\",0],"
            "UNIT[\"metre\",1]]");
    }
    else
    {
        poDS->adfGeoTransform[0] = LSO;
        poDS->adfGeoTransform[1] = 360. / ARV;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = PSO;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -360. / BRV;
        poDS->m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);
    }

    // if( isGIN )
    {
        char szValue[32];
        snprintf(szValue, sizeof(szValue), "%d", SCA);
        poDS->SetMetadataItem("ADRG_SCA", szValue);
        snprintf(szValue, sizeof(szValue), "%d", ZNA);
        poDS->SetMetadataItem("ADRG_ZNA", szValue);
    }

    poDS->SetMetadataItem("ADRG_NAM", osNAM.c_str());

    poDS->nBands = 3;
    for (int i = 0; i < poDS->nBands; i++)
        poDS->SetBand(i + 1, new ADRGRasterBand(poDS, i + 1));

    return poDS;
}

/************************************************************************/
/*                          GetGENListFromTHF()                         */
/************************************************************************/

char **ADRGDataset::GetGENListFromTHF(const char *pszFileName)
{
    DDFModule module;
    DDFRecord *record = nullptr;
    int nFilenames = 0;
    char **papszFileNames = nullptr;

    if (!module.Open(pszFileName, TRUE))
        return papszFileNames;

    while (true)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == nullptr)
            break;

        if (record->GetFieldCount() >= 2)
        {
            DDFField *field = record->GetField(0);
            DDFFieldDefn *fieldDefn = field->GetFieldDefn();
            if (!(strcmp(fieldDefn->GetName(), "001") == 0 &&
                  fieldDefn->GetSubfieldCount() == 2))
            {
                continue;
            }

            const char *RTY = record->GetStringSubfield("001", 0, "RTY", 0);
            if (RTY == nullptr || !(strcmp(RTY, "TFN") == 0))
            {
                continue;
            }

            int iVFFFieldInstance = 0;
            for (int i = 1; i < record->GetFieldCount(); i++)
            {
                field = record->GetField(i);
                fieldDefn = field->GetFieldDefn();

                if (!(strcmp(fieldDefn->GetName(), "VFF") == 0 &&
                      fieldDefn->GetSubfieldCount() == 1))
                {
                    continue;
                }

                const char *pszVFF = record->GetStringSubfield(
                    "VFF", iVFFFieldInstance++, "VFF", 0);
                if (pszVFF == nullptr)
                    continue;
                CPLString osSubFileName(pszVFF);
                char *c = (char *)strchr(osSubFileName.c_str(), ' ');
                if (c)
                    *c = 0;
                if (EQUAL(CPLGetExtensionSafe(osSubFileName.c_str()).c_str(),
                          "GEN"))
                {
                    CPLDebug("ADRG", "Found GEN file in THF : %s",
                             osSubFileName.c_str());
                    CPLString osGENFileName(CPLGetDirnameSafe(pszFileName));
                    char **tokens =
                        CSLTokenizeString2(osSubFileName.c_str(), "/\"", 0);
                    char **ptr = tokens;
                    if (ptr == nullptr)
                        continue;
                    while (*ptr)
                    {
                        char **papszDirContent =
                            VSIReadDir(osGENFileName.c_str());
                        char **ptrDir = papszDirContent;
                        if (ptrDir)
                        {
                            while (*ptrDir)
                            {
                                if (EQUAL(*ptrDir, *ptr))
                                {
                                    osGENFileName = CPLFormFilenameSafe(
                                        osGENFileName.c_str(), *ptrDir,
                                        nullptr);
                                    CPLDebug("ADRG",
                                             "Building GEN full file name : %s",
                                             osGENFileName.c_str());
                                    break;
                                }
                                ptrDir++;
                            }
                        }
                        if (ptrDir == nullptr)
                            break;
                        CSLDestroy(papszDirContent);
                        ptr++;
                    }
                    int isNameValid = *ptr == nullptr;
                    CSLDestroy(tokens);
                    if (isNameValid)
                    {
                        papszFileNames = (char **)CPLRealloc(
                            papszFileNames, sizeof(char *) * (nFilenames + 2));
                        papszFileNames[nFilenames] =
                            CPLStrdup(osGENFileName.c_str());
                        papszFileNames[nFilenames + 1] = nullptr;
                        nFilenames++;
                    }
                }
            }
        }
    }
    return papszFileNames;
}

/************************************************************************/
/*                          GetIMGListFromGEN()                         */
/************************************************************************/

char **ADRGDataset::GetIMGListFromGEN(const char *pszFileName,
                                      int *pnRecordIndex)
{
    DDFRecord *record = nullptr;
    int nFilenames = 0;
    char **papszFileNames = nullptr;
    int nRecordIndex = -1;

    if (pnRecordIndex)
        *pnRecordIndex = -1;

    DDFModule module;
    if (!module.Open(pszFileName, TRUE))
        return nullptr;

    while (true)
    {
        nRecordIndex++;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        record = module.ReadRecord();
        CPLPopErrorHandler();
        CPLErrorReset();
        if (record == nullptr)
            break;

        if (record->GetFieldCount() >= 5)
        {
            DDFField *field = record->GetField(0);
            DDFFieldDefn *fieldDefn = field->GetFieldDefn();
            if (!(strcmp(fieldDefn->GetName(), "001") == 0 &&
                  fieldDefn->GetSubfieldCount() == 2))
            {
                continue;
            }

            const char *RTY = record->GetStringSubfield("001", 0, "RTY", 0);
            if (RTY == nullptr)
                continue;
            /* Ignore overviews */
            if (strcmp(RTY, "OVV") == 0)
                continue;

            // TODO: Fix the non-GIN section or remove it.
            if (strcmp(RTY, "GIN") != 0)
                continue;

            /* make sure that the GEN file is part of an ADRG dataset, not a SRP
             * dataset, by checking that the GEN field contains a NWO subfield
             */
            const char *NWO = record->GetStringSubfield("GEN", 0, "NWO", 0);
            if (NWO == nullptr)
            {
                CSLDestroy(papszFileNames);
                return nullptr;
            }

            field = record->GetField(3);
            if (field == nullptr)
                continue;
            fieldDefn = field->GetFieldDefn();

            if (!(strcmp(fieldDefn->GetName(), "SPR") == 0 &&
                  fieldDefn->GetSubfieldCount() == 15))
            {
                continue;
            }

            const char *pszBAD = record->GetStringSubfield("SPR", 0, "BAD", 0);
            if (pszBAD == nullptr || strlen(pszBAD) != 12)
                continue;
            CPLString osBAD = pszBAD;
            {
                char *c = (char *)strchr(osBAD.c_str(), ' ');
                if (c)
                    *c = 0;
            }
            CPLDebug("ADRG", "BAD=%s", osBAD.c_str());

            /* Build full IMG file name from BAD value */
            CPLString osGENDir(CPLGetDirnameSafe(pszFileName));

            const CPLString osFileName =
                CPLFormFilenameSafe(osGENDir.c_str(), osBAD.c_str(), nullptr);
            VSIStatBufL sStatBuf;
            if (VSIStatL(osFileName, &sStatBuf) == 0)
            {
                osBAD = osFileName;
                CPLDebug("ADRG", "Building IMG full file name : %s",
                         osBAD.c_str());
            }
            else
            {
                char **papszDirContent = nullptr;
                if (strcmp(osGENDir.c_str(), "/vsimem") == 0)
                {
                    CPLString osTmp = osGENDir + "/";
                    papszDirContent = VSIReadDir(osTmp);
                }
                else
                    papszDirContent = VSIReadDir(osGENDir);
                char **ptrDir = papszDirContent;
                while (ptrDir && *ptrDir)
                {
                    if (EQUAL(*ptrDir, osBAD.c_str()))
                    {
                        osBAD = CPLFormFilenameSafe(osGENDir.c_str(), *ptrDir,
                                                    nullptr);
                        CPLDebug("ADRG", "Building IMG full file name : %s",
                                 osBAD.c_str());
                        break;
                    }
                    ptrDir++;
                }
                CSLDestroy(papszDirContent);
            }

            if (nFilenames == 0 && pnRecordIndex)
                *pnRecordIndex = nRecordIndex;

            papszFileNames = (char **)CPLRealloc(
                papszFileNames, sizeof(char *) * (nFilenames + 2));
            papszFileNames[nFilenames] = CPLStrdup(osBAD.c_str());
            papszFileNames[nFilenames + 1] = nullptr;
            nFilenames++;
        }
    }

    return papszFileNames;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ADRGDataset::Open(GDALOpenInfo *poOpenInfo)
{
    int nRecordIndex = -1;
    CPLString osGENFileName;
    CPLString osIMGFileName;
    bool bFromSubdataset = false;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "ADRG:"))
    {
        char **papszTokens =
            CSLTokenizeString2(poOpenInfo->pszFilename + 5, ",", 0);
        if (CSLCount(papszTokens) == 2)
        {
            osGENFileName = papszTokens[0];
            osIMGFileName = papszTokens[1];
            bFromSubdataset = true;
        }
        CSLDestroy(papszTokens);
    }
    else
    {
        if (poOpenInfo->nHeaderBytes < 500)
            return nullptr;

        CPLString osFileName(poOpenInfo->pszFilename);
        if (EQUAL(CPLGetExtensionSafe(osFileName.c_str()).c_str(), "THF"))
        {
            char **papszFileNames = GetGENListFromTHF(osFileName.c_str());
            if (papszFileNames == nullptr)
                return nullptr;
            if (papszFileNames[1] == nullptr)
            {
                osFileName = papszFileNames[0];
                CSLDestroy(papszFileNames);
            }
            else
            {
                char **ptr = papszFileNames;
                ADRGDataset *poDS = new ADRGDataset();
                while (*ptr)
                {
                    char **papszIMGFileNames = GetIMGListFromGEN(*ptr);
                    char **papszIMGIter = papszIMGFileNames;
                    while (papszIMGIter && *papszIMGIter)
                    {
                        poDS->AddSubDataset(*ptr, *papszIMGIter);
                        papszIMGIter++;
                    }
                    CSLDestroy(papszIMGFileNames);

                    ptr++;
                }
                CSLDestroy(papszFileNames);
                return poDS;
            }
        }

        if (EQUAL(CPLGetExtensionSafe(osFileName.c_str()).c_str(), "GEN"))
        {
            osGENFileName = osFileName;

            char **papszFileNames =
                GetIMGListFromGEN(osFileName.c_str(), &nRecordIndex);
            if (papszFileNames == nullptr)
                return nullptr;
            if (papszFileNames[1] == nullptr)
            {
                osIMGFileName = papszFileNames[0];
                CSLDestroy(papszFileNames);
            }
            else
            {
                char **ptr = papszFileNames;
                ADRGDataset *poDS = new ADRGDataset();
                while (*ptr)
                {
                    poDS->AddSubDataset(osFileName.c_str(), *ptr);
                    ptr++;
                }
                CSLDestroy(papszFileNames);
                return poDS;
            }
        }
    }

    if (!osGENFileName.empty() && !osIMGFileName.empty())
    {
        if (poOpenInfo->eAccess == GA_Update)
        {
            ReportUpdateNotSupportedByDriver("ADRG");
            return nullptr;
        }

        DDFModule module;
        DDFRecord *record = nullptr;
        if (nRecordIndex >= 0 && module.Open(osGENFileName.c_str(), TRUE))
        {
            for (int i = 0; i <= nRecordIndex; i++)
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                record = module.ReadRecord();
                CPLPopErrorHandler();
                CPLErrorReset();
                if (record == nullptr)
                    break;
            }
        }

        ADRGDataset *poDS =
            OpenDataset(osGENFileName.c_str(), osIMGFileName.c_str(), record);

        if (poDS)
        {
            /* -------------------------------------------------------------- */
            /*      Initialize any PAM information.                           */
            /* -------------------------------------------------------------- */
            poDS->SetDescription(poOpenInfo->pszFilename);
            poDS->TryLoadXML();

            /* -------------------------------------------------------------- */
            /*      Check for external overviews.                             */
            /* -------------------------------------------------------------- */
            if (bFromSubdataset)
                poDS->oOvManager.Initialize(poDS, osIMGFileName.c_str());
            else
                poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

            return poDS;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                         GDALRegister_ADRG()                          */
/************************************************************************/

void GDALRegister_ADRG()

{
    if (GDALGetDriverByName("ADRG") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("ADRG");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "ARC Digitized Raster Graphics");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/adrg.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gen");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = ADRGDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
