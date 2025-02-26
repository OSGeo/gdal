/******************************************************************************
 *
 * Project:  ISIS Version 2 Driver
 * Purpose:  Implementation of ISIS2Dataset
 * Author:   Trent Hare (thare@usgs.gov),
 *           Robert Soricone (rsoricone@usgs.gov)
 *           Ludovic Mercier (ludovic.mercier@gmail.com)
 *           Frank Warmerdam (warmerdam@pobox.com)
 *
 * NOTE: Original code authored by Trent and Robert and placed in the public
 * domain as per US government policy.  I have (within my rights) appropriated
 * it and placed it under the following license.  This is not intended to
 * diminish Trent and Roberts contribution.
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

constexpr int NULL1 = 0;
constexpr int NULL2 = -32768;
constexpr double NULL3 = -3.4028226550889044521e+38;

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "nasakeywordhandler.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"
#include "pdsdrivercore.h"

/************************************************************************/
/* ==================================================================== */
/*                      ISISDataset     version2                        */
/* ==================================================================== */
/************************************************************************/

class ISIS2Dataset final : public RawDataset
{
    VSILFILE *fpImage;  // image data file.
    CPLString osExternalCube;

    NASAKeywordHandler oKeywords;

    int bGotTransform;
    double adfGeoTransform[6];

    OGRSpatialReference m_oSRS{};

    int parse_label(const char *file, char *keyword, char *value);
    int strstrip(char instr[], char outstr[], int position);

    CPLString oTempResult;

    static void CleanString(CPLString &osInput);

    const char *GetKeyword(const char *pszPath, const char *pszDefault = "");
    const char *GetKeywordSub(const char *pszPath, int iSubscript,
                              const char *pszDefault = "");

    CPLErr Close() override;

  public:
    ISIS2Dataset();
    virtual ~ISIS2Dataset();

    virtual CPLErr GetGeoTransform(double *padfTransform) override;
    const OGRSpatialReference *GetSpatialRef() const override;

    virtual char **GetFileList() override;

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/*                            ISIS2Dataset()                            */
/************************************************************************/

ISIS2Dataset::ISIS2Dataset() : fpImage(nullptr), bGotTransform(FALSE)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ISIS2Dataset()                            */
/************************************************************************/

ISIS2Dataset::~ISIS2Dataset()

{
    ISIS2Dataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr ISIS2Dataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (ISIS2Dataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;
        if (fpImage != nullptr)
        {
            if (VSIFCloseL(fpImage) != 0)
                eErr = CE_Failure;
        }
        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ISIS2Dataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if (!osExternalCube.empty())
        papszFileList = CSLAddString(papszFileList, osExternalCube);

    return papszFileList;
}

/************************************************************************/
/*                         GetSpatialRef()                              */
/************************************************************************/

const OGRSpatialReference *ISIS2Dataset::GetSpatialRef() const
{
    if (!m_oSRS.IsEmpty())
        return &m_oSRS;
    return GDALPamDataset::GetSpatialRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ISIS2Dataset::GetGeoTransform(double *padfTransform)

{
    if (bGotTransform)
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ISIS2Dataset::Open(GDALOpenInfo *poOpenInfo)
{
    /* -------------------------------------------------------------------- */
    /*      Does this look like a CUBE or an IMAGE Primary Data Object?     */
    /* -------------------------------------------------------------------- */
    if (!ISIS2DriverIdentify(poOpenInfo) || poOpenInfo->fpL == nullptr)
        return nullptr;

    VSILFILE *fpQube = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    auto poDS = std::make_unique<ISIS2Dataset>();

    if (!poDS->oKeywords.Ingest(fpQube, 0))
    {
        VSIFCloseL(fpQube);
        return nullptr;
    }

    VSIFCloseL(fpQube);

    /* -------------------------------------------------------------------- */
    /*      We assume the user is pointing to the label (i.e. .lab) file.   */
    /* -------------------------------------------------------------------- */
    // QUBE can be inline or detached and point to an image name
    // ^QUBE = 76
    // ^QUBE = ("ui31s015.img",6441<BYTES>) - has another label on the image
    // ^QUBE = "ui31s015.img" - which implies no label or skip value

    const char *pszQube = poDS->GetKeyword("^QUBE");
    int nQube = 0;
    int bByteLocation = FALSE;
    CPLString osTargetFile = poOpenInfo->pszFilename;

    if (pszQube[0] == '"')
    {
        const CPLString osTPath = CPLGetPathSafe(poOpenInfo->pszFilename);
        CPLString osFilename = pszQube;
        poDS->CleanString(osFilename);
        osTargetFile = CPLFormCIFilenameSafe(osTPath, osFilename, nullptr);
        poDS->osExternalCube = osTargetFile;
    }
    else if (pszQube[0] == '(')
    {
        const CPLString osTPath = CPLGetPathSafe(poOpenInfo->pszFilename);
        CPLString osFilename = poDS->GetKeywordSub("^QUBE", 1, "");
        poDS->CleanString(osFilename);
        osTargetFile = CPLFormCIFilenameSafe(osTPath, osFilename, nullptr);
        poDS->osExternalCube = osTargetFile;

        nQube = atoi(poDS->GetKeywordSub("^QUBE", 2, "1"));
        if (strstr(poDS->GetKeywordSub("^QUBE", 2, "1"), "<BYTES>") != nullptr)
            bByteLocation = true;
    }
    else
    {
        nQube = atoi(pszQube);
        if (strstr(pszQube, "<BYTES>") != nullptr)
            bByteLocation = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Check if file an ISIS2 header file?  Read a few lines of text   */
    /*      searching for something starting with nrows or ncols.           */
    /* -------------------------------------------------------------------- */

    /* -------------------------------------------------------------------- */
    /*      Checks to see if this is valid ISIS2 cube                       */
    /*      SUFFIX_ITEM tag in .cub file should be (0,0,0); no side-planes  */
    /* -------------------------------------------------------------------- */
    const int s_ix = atoi(poDS->GetKeywordSub("QUBE.SUFFIX_ITEMS", 1));
    const int s_iy = atoi(poDS->GetKeywordSub("QUBE.SUFFIX_ITEMS", 2));
    const int s_iz = atoi(poDS->GetKeywordSub("QUBE.SUFFIX_ITEMS", 3));

    if (s_ix != 0 || s_iy != 0 || s_iz != 0)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "*** ISIS 2 cube file has invalid SUFFIX_ITEMS parameters:\n"
                 "*** gdal isis2 driver requires (0, 0, 0), thus no sideplanes "
                 "or backplanes\n"
                 "found: (%i, %i, %i)\n\n",
                 s_ix, s_iy, s_iz);
        return nullptr;
    }

    /**************** end SUFFIX_ITEM check ***********************/

    /***********   Grab layout type (BSQ, BIP, BIL) ************/
    //  AXIS_NAME = (SAMPLE,LINE,BAND)
    /***********************************************************/

    char szLayout[10] = "BSQ";  // default to band seq.
    const char *value = poDS->GetKeyword("QUBE.AXIS_NAME", "");
    if (EQUAL(value, "(SAMPLE,LINE,BAND)"))
        strcpy(szLayout, "BSQ");
    else if (EQUAL(value, "(BAND,LINE,SAMPLE)"))
        strcpy(szLayout, "BIP");
    else if (EQUAL(value, "(SAMPLE,BAND,LINE)") || EQUAL(value, ""))
        strcpy(szLayout, "BSQ");
    else
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "%s layout not supported. Abort\n\n", value);
        return nullptr;
    }

    /***********   Grab samples lines band ************/
    const int nCols = atoi(poDS->GetKeywordSub("QUBE.CORE_ITEMS", 1));
    const int nRows = atoi(poDS->GetKeywordSub("QUBE.CORE_ITEMS", 2));
    const int nBands = atoi(poDS->GetKeywordSub("QUBE.CORE_ITEMS", 3));

    /***********   Grab Qube record bytes  **********/
    const int record_bytes = atoi(poDS->GetKeyword("RECORD_BYTES"));
    if (record_bytes < 0)
    {
        return nullptr;
    }

    GUIntBig nSkipBytes = 0;
    if (nQube > 0 && bByteLocation)
        nSkipBytes = (nQube - 1);
    else if (nQube > 0)
        nSkipBytes = static_cast<GUIntBig>(nQube - 1) * record_bytes;
    else
        nSkipBytes = 0;

    /***********   Grab samples lines band ************/
    char chByteOrder = 'M';  // default to MSB
    CPLString osCoreItemType = poDS->GetKeyword("QUBE.CORE_ITEM_TYPE");
    if ((EQUAL(osCoreItemType, "PC_INTEGER")) ||
        (EQUAL(osCoreItemType, "PC_UNSIGNED_INTEGER")) ||
        (EQUAL(osCoreItemType, "PC_REAL")))
    {
        chByteOrder = 'I';
    }

    /********   Grab format type - isis2 only supports 8,16,32 *******/
    GDALDataType eDataType = GDT_Byte;
    bool bNoDataSet = false;
    double dfNoData = 0.0;

    int itype = atoi(poDS->GetKeyword("QUBE.CORE_ITEM_BYTES", ""));
    switch (itype)
    {
        case 1:
            eDataType = GDT_Byte;
            dfNoData = NULL1;
            bNoDataSet = true;
            break;
        case 2:
            if (strstr(osCoreItemType, "UNSIGNED") != nullptr)
            {
                dfNoData = 0;
                eDataType = GDT_UInt16;
            }
            else
            {
                dfNoData = NULL2;
                eDataType = GDT_Int16;
            }
            bNoDataSet = true;
            break;
        case 4:
            eDataType = GDT_Float32;
            dfNoData = NULL3;
            bNoDataSet = true;
            break;
        case 8:
            eDataType = GDT_Float64;
            dfNoData = NULL3;
            bNoDataSet = true;
            break;
        default:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Itype of %d is not supported in ISIS 2.", itype);
            return nullptr;
    }

    /***********   Grab Cellsize ************/
    double dfXDim = 1.0;
    double dfYDim = 1.0;

    value = poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.MAP_SCALE");
    if (strlen(value) > 0)
    {
        // Convert km to m
        dfXDim = static_cast<float>(CPLAtof(value) * 1000.0);
        dfYDim = static_cast<float>(CPLAtof(value) * 1000.0 * -1);
    }

    /***********   Grab LINE_PROJECTION_OFFSET ************/
    double dfULYMap = 0.5;

    value =
        poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.LINE_PROJECTION_OFFSET");
    if (strlen(value) > 0)
    {
        const double yulcenter = static_cast<float>(CPLAtof(value)) * dfYDim;
        dfULYMap = yulcenter - (dfYDim / 2);
    }

    /***********   Grab SAMPLE_PROJECTION_OFFSET ************/
    double dfULXMap = 0.5;

    value =
        poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.SAMPLE_PROJECTION_OFFSET");
    if (strlen(value) > 0)
    {
        const double xulcenter = static_cast<float>(CPLAtof(value)) * dfXDim;
        dfULXMap = xulcenter - (dfXDim / 2);
    }

    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. MARS ***/
    const CPLString target_name = poDS->GetKeyword("QUBE.TARGET_NAME");

    /***********   Grab MAP_PROJECTION_TYPE ************/
    CPLString map_proj_name =
        poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.MAP_PROJECTION_TYPE");
    poDS->CleanString(map_proj_name);

    /***********   Grab SEMI-MAJOR ************/
    const double semi_major =
        CPLAtof(poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.A_AXIS_RADIUS")) *
        1000.0;

    /***********   Grab semi-minor ************/
    const double semi_minor =
        CPLAtof(poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.C_AXIS_RADIUS")) *
        1000.0;

    /***********   Grab CENTER_LAT ************/
    const double center_lat =
        CPLAtof(poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.CENTER_LATITUDE"));

    /***********   Grab CENTER_LON ************/
    const double center_lon =
        CPLAtof(poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.CENTER_LONGITUDE"));

    /***********   Grab 1st std parallel ************/
    const double first_std_parallel = CPLAtof(
        poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.FIRST_STANDARD_PARALLEL"));

    /***********   Grab 2nd std parallel ************/
    const double second_std_parallel = CPLAtof(
        poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.SECOND_STANDARD_PARALLEL"));

    /*** grab  PROJECTION_LATITUDE_TYPE = "PLANETOCENTRIC" ****/
    // Need to further study how ocentric/ographic will effect the gdal library.
    // So far we will use this fact to define a sphere or ellipse for some
    // projections Frank - may need to talk this over
    bool bIsGeographic = true;
    value =
        poDS->GetKeyword("CUBE.IMAGE_MAP_PROJECTION.PROJECTION_LATITUDE_TYPE");
    if (EQUAL(value, "\"PLANETOCENTRIC\""))
        bIsGeographic = false;

    CPLDebug("ISIS2", "using projection %s", map_proj_name.c_str());

    OGRSpatialReference oSRS;
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    bool bProjectionSet = true;

    // Set oSRS projection and parameters
    if ((EQUAL(map_proj_name, "EQUIRECTANGULAR_CYLINDRICAL")) ||
        (EQUAL(map_proj_name, "EQUIRECTANGULAR")) ||
        (EQUAL(map_proj_name, "SIMPLE_CYLINDRICAL")))
    {
        oSRS.OGRSpatialReference::SetEquirectangular2(0.0, center_lon,
                                                      center_lat, 0, 0);
    }
    else if (EQUAL(map_proj_name, "ORTHOGRAPHIC"))
    {
        oSRS.OGRSpatialReference::SetOrthographic(center_lat, center_lon, 0, 0);
    }
    else if ((EQUAL(map_proj_name, "SINUSOIDAL")) ||
             (EQUAL(map_proj_name, "SINUSOIDAL_EQUAL-AREA")))
    {
        oSRS.OGRSpatialReference::SetSinusoidal(center_lon, 0, 0);
    }
    else if (EQUAL(map_proj_name, "MERCATOR"))
    {
        oSRS.OGRSpatialReference::SetMercator(center_lat, center_lon, 1, 0, 0);
    }
    else if (EQUAL(map_proj_name, "POLAR_STEREOGRAPHIC"))
    {
        oSRS.OGRSpatialReference::SetPS(center_lat, center_lon, 1, 0, 0);
    }
    else if (EQUAL(map_proj_name, "TRANSVERSE_MERCATOR"))
    {
        oSRS.OGRSpatialReference::SetTM(center_lat, center_lon, 1, 0, 0);
    }
    else if (EQUAL(map_proj_name, "LAMBERT_CONFORMAL_CONIC"))
    {
        oSRS.OGRSpatialReference::SetLCC(first_std_parallel,
                                         second_std_parallel, center_lat,
                                         center_lon, 0, 0);
    }
    else if (EQUAL(map_proj_name, ""))
    {
        /* no projection */
        bProjectionSet = false;
    }
    else
    {
        CPLDebug("ISIS2",
                 "Dataset projection %s is not supported. Continuing...",
                 map_proj_name.c_str());
        bProjectionSet = false;
    }

    if (bProjectionSet)
    {
        // Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
        const CPLString proj_target_name = map_proj_name + " " + target_name;
        oSRS.SetProjCS(proj_target_name);  // set ProjCS keyword

        // The geographic/geocentric name will be the same basic name as the
        // body name 'GCS' = Geographic/Geocentric Coordinate System
        const CPLString geog_name = "GCS_" + target_name;

        // The datum and sphere names will be the same basic name aas the planet
        const CPLString datum_name = "D_" + target_name;
        // Might not be IAU defined so don't add.
        CPLString sphere_name = target_name;  // + "_IAU_IAG");

        // calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        double iflattening = 0.0;
        if ((semi_major - semi_minor) < 0.0000001)
            iflattening = 0;
        else
            iflattening = semi_major / (semi_major - semi_minor);

        // Set the body size but take into consideration which proj is being
        // used to help w/ proj4 compatibility The use of a Sphere, polar radius
        // or ellipse here is based on how ISIS does it internally
        if (((EQUAL(map_proj_name, "STEREOGRAPHIC") &&
              (fabs(center_lat) == 90))) ||
            (EQUAL(map_proj_name, "POLAR_STEREOGRAPHIC")))
        {
            if (bIsGeographic)
            {
                // Geograpraphic, so set an ellipse
                oSRS.SetGeogCS(geog_name, datum_name, sphere_name, semi_major,
                               iflattening, "Reference_Meridian", 0.0);
            }
            else
            {
                // Geocentric, so force a sphere using the semi-minor axis. I
                // hope...
                sphere_name += "_polarRadius";
                oSRS.SetGeogCS(geog_name, datum_name, sphere_name, semi_minor,
                               0.0, "Reference_Meridian", 0.0);
            }
        }
        else if ((EQUAL(map_proj_name, "SIMPLE_CYLINDRICAL")) ||
                 (EQUAL(map_proj_name, "ORTHOGRAPHIC")) ||
                 (EQUAL(map_proj_name, "STEREOGRAPHIC")) ||
                 (EQUAL(map_proj_name, "SINUSOIDAL_EQUAL-AREA")) ||
                 (EQUAL(map_proj_name, "SINUSOIDAL")))
        {
            // ISIS uses the spherical equation for these projections so force
            // a sphere.
            oSRS.SetGeogCS(geog_name, datum_name, sphere_name, semi_major, 0.0,
                           "Reference_Meridian", 0.0);
        }
        else if ((EQUAL(map_proj_name, "EQUIRECTANGULAR_CYLINDRICAL")) ||
                 (EQUAL(map_proj_name, "EQUIRECTANGULAR")))
        {
            // Calculate localRadius using ISIS3 simple elliptical method
            //   not the more standard Radius of Curvature method
            // PI = 4 * atan(1);
            if (center_lon == 0)
            {  // No need to calculate local radius
                oSRS.SetGeogCS(geog_name, datum_name, sphere_name, semi_major,
                               0.0, "Reference_Meridian", 0.0);
            }
            else
            {
                const double radLat = center_lat * M_PI / 180;  // in radians
                const double localRadius =
                    semi_major * semi_minor /
                    sqrt(pow(semi_minor * cos(radLat), 2) +
                         pow(semi_major * sin(radLat), 2));
                sphere_name += "_localRadius";
                oSRS.SetGeogCS(geog_name, datum_name, sphere_name, localRadius,
                               0.0, "Reference_Meridian", 0.0);
                CPLDebug("ISIS2", "local radius: %f", localRadius);
            }
        }
        else
        {
            // All other projections: Mercator, Transverse Mercator, Lambert
            // Conformal, etc. Geographic, so set an ellipse
            if (bIsGeographic)
            {
                oSRS.SetGeogCS(geog_name, datum_name, sphere_name, semi_major,
                               iflattening, "Reference_Meridian", 0.0);
            }
            else
            {
                // Geocentric, so force a sphere. I hope...
                oSRS.SetGeogCS(geog_name, datum_name, sphere_name, semi_major,
                               0.0, "Reference_Meridian", 0.0);
            }
        }

        // translate back into a projection string.
        poDS->m_oSRS = std::move(oSRS);
    }

    /* END ISIS2 Label Read */
    /*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

    /* -------------------------------------------------------------------- */
    /*      Did we get the required keywords?  If not we return with        */
    /*      this never having been considered to be a match. This isn't     */
    /*      an error!                                                       */
    /* -------------------------------------------------------------------- */
    if (!GDALCheckDatasetDimensions(nCols, nRows) ||
        !GDALCheckBandCount(nBands, false))
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Capture some information from the file that is of interest.     */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

    /* -------------------------------------------------------------------- */
    /*      Open target binary file.                                        */
    /* -------------------------------------------------------------------- */

    if (poOpenInfo->eAccess == GA_ReadOnly)
        poDS->fpImage = VSIFOpenL(osTargetFile, "rb");
    else
        poDS->fpImage = VSIFOpenL(osTargetFile, "r+b");

    if (poDS->fpImage == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open %s with write permission.\n%s",
                 osTargetFile.c_str(), VSIStrerror(errno));
        return nullptr;
    }

    poDS->eAccess = poOpenInfo->eAccess;

    /* -------------------------------------------------------------------- */
    /*      Compute the line offset.                                        */
    /* -------------------------------------------------------------------- */
    int nItemSize = GDALGetDataTypeSizeBytes(eDataType);
    int nLineOffset, nPixelOffset;
    vsi_l_offset nBandOffset;

    if (EQUAL(szLayout, "BIP"))
    {
        nPixelOffset = nItemSize * nBands;
        if (nPixelOffset > INT_MAX / nBands)
        {
            return nullptr;
        }
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = nItemSize;
    }
    else if (EQUAL(szLayout, "BSQ"))
    {
        nPixelOffset = nItemSize;
        if (nPixelOffset > INT_MAX / nCols)
        {
            return nullptr;
        }
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;
    }
    else /* assume BIL */
    {
        nPixelOffset = nItemSize;
        if (nPixelOffset > INT_MAX / nBands ||
            nPixelOffset * nBands > INT_MAX / nCols)
        {
            return nullptr;
        }
        nLineOffset = nItemSize * nBands * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nItemSize) * nCols;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < nBands; i++)
    {
        auto poBand = RawRasterBand::Create(
            poDS.get(), i + 1, poDS->fpImage, nSkipBytes + nBandOffset * i,
            nPixelOffset, nLineOffset, eDataType,
            chByteOrder == 'I' || chByteOrder == 'L'
                ? RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN
                : RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN,
            RawRasterBand::OwnFP::NO);
        if (!poBand)
            return nullptr;

        if (bNoDataSet)
            poBand->SetNoDataValue(dfNoData);

        // Set offset/scale values at the PAM level.
        poBand->SetOffset(CPLAtofM(poDS->GetKeyword("QUBE.CORE_BASE", "0.0")));
        poBand->SetScale(
            CPLAtofM(poDS->GetKeyword("QUBE.CORE_MULTIPLIER", "1.0")));

        poDS->SetBand(i + 1, std::move(poBand));
    }

    /* -------------------------------------------------------------------- */
    /*      Check for a .prj file. For isis2 I would like to keep this in   */
    /* -------------------------------------------------------------------- */
    const CPLString osPath = CPLGetPathSafe(poOpenInfo->pszFilename);
    const CPLString osName = CPLGetBasenameSafe(poOpenInfo->pszFilename);
    const std::string osPrjFile = CPLFormCIFilenameSafe(osPath, osName, "prj");

    VSILFILE *fp = VSIFOpenL(osPrjFile.c_str(), "r");
    if (fp != nullptr)
    {
        VSIFCloseL(fp);

        char **papszLines = CSLLoad(osPrjFile.c_str());

        poDS->m_oSRS.importFromESRI(papszLines);

        CSLDestroy(papszLines);
    }

    if (dfULXMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0)
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = dfULXMap;
        poDS->adfGeoTransform[1] = dfXDim;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfULYMap;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = dfYDim;
    }

    if (!poDS->bGotTransform)
        poDS->bGotTransform = GDALReadWorldFile(poOpenInfo->pszFilename, "cbw",
                                                poDS->adfGeoTransform);

    if (!poDS->bGotTransform)
        poDS->bGotTransform = GDALReadWorldFile(poOpenInfo->pszFilename, "wld",
                                                poDS->adfGeoTransform);

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *ISIS2Dataset::GetKeyword(const char *pszPath,
                                     const char *pszDefault)

{
    return oKeywords.GetKeyword(pszPath, pszDefault);
}

/************************************************************************/
/*                            GetKeywordSub()                           */
/************************************************************************/

const char *ISIS2Dataset::GetKeywordSub(const char *pszPath, int iSubscript,
                                        const char *pszDefault)

{
    const char *pszResult = oKeywords.GetKeyword(pszPath, nullptr);

    if (pszResult == nullptr)
        return pszDefault;

    if (pszResult[0] != '(')
        return pszDefault;

    char **papszTokens =
        CSLTokenizeString2(pszResult, "(,)", CSLT_HONOURSTRINGS);

    if (iSubscript <= CSLCount(papszTokens))
    {
        oTempResult = papszTokens[iSubscript - 1];
        CSLDestroy(papszTokens);
        return oTempResult.c_str();
    }

    CSLDestroy(papszTokens);
    return pszDefault;
}

/************************************************************************/
/*                            CleanString()                             */
/*                                                                      */
/* Removes single or double quotes, and converts spaces to underscores. */
/* The change is made in-place to CPLString.                            */
/************************************************************************/

void ISIS2Dataset::CleanString(CPLString &osInput)

{
    if ((osInput.size() < 2) ||
        ((osInput.at(0) != '"' || osInput.back() != '"') &&
         (osInput.at(0) != '\'' || osInput.back() != '\'')))
        return;

    char *pszWrk = CPLStrdup(osInput.c_str() + 1);

    pszWrk[strlen(pszWrk) - 1] = '\0';

    for (int i = 0; pszWrk[i] != '\0'; i++)
    {
        if (pszWrk[i] == ' ')
            pszWrk[i] = '_';
    }

    osInput = pszWrk;
    CPLFree(pszWrk);
}

/************************************************************************/
/*                         GDALRegister_ISIS2()                         */
/************************************************************************/

void GDALRegister_ISIS2()

{
    if (GDALGetDriverByName(ISIS2_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    ISIS2DriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = ISIS2Dataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
