/*****************************************************************************
*
* Project:  Idrisi Raster Image File Driver
* Purpose:  Read/write Idrisi Raster Image Format RST
* Author:   Ivan Lucena, ilucena@clarku.edu
*
******************************************************************************
* Copyright (c) 2005, Ivan Lucena
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
*/

typedef	unsigned char	uint8;	
typedef	signed short	int16;		
typedef	unsigned short	uint16;
typedef float			float32;			
typedef long			int32;
typedef unsigned long	uint32;

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_C_START
void GDALRegister_IDRISI(void);
CPL_C_END

// file extensions
#define extRST          "rst"
#define extRDC          "rdc"
#define extREF          "ref"
#define extSMP          "smp"

// file ".rdc" field names:
#define rdcFILE_FORMAT	"file format "
#define rdcFILE_TITLE	"file title  "
#define rdcDATA_TYPE	"data type   "
#define rdcFILE_TYPE	"file type   "
#define rdcCOLUMNS		"columns     "
#define rdcROWS			"rows        "
#define rdcREF_SYSTEM	"ref. system "
#define rdcREF_UNITS	"ref. units  "
#define rdcUNIT_DIST	"unit dist.  "
#define rdcMIN_X		"min. X      "
#define rdcMAX_X		"max. X      "
#define rdcMIN_Y		"min. Y      "
#define rdcMAX_Y		"max. Y      "
#define rdcPOSN_ERROR	"pos'n error "
#define rdcRESOLUTION	"resolution  "
#define rdcMIN_VALUE	"min. value  "
#define rdcMAX_VALUE	"max. value  "
#define rdcDISPLAY_MIN	"display min "
#define rdcDISPLAY_MAX	"display max "
#define rdcVALUE_UNITS	"value units "
#define rdcVALUE_ERROR	"value error "
#define rdcFLAG_VALUE	"flag value  "
#define rdcFLAG_DEFN	"flag def'n  "
#define rdcLEGEND_CATS	"legend cats "
#define rdcLINEAGES		"lineage     "
#define rdcCOMMENTS		"comment     "
#define rdcCODE_N		"code %6d "

//
#define rstVERSION      "Idrisi Raster A.1"
#define rstBYTE         "byte"
#define rstINTEGER      "integer"
#define rstREAL         "real"
#define rstRGB24        "rgb24"
#define rstLATLONG      "latlong"
#define rstPLANE        "plane"
#define rstUTM          "utm-%d%c"

// file ".ref" field names:
#define refREF_SYSTEM   "ref. system "
#define refPROJECTION   "projection  "
#define refDATUM        "datum       " 
#define refDELTA_WGS84  "delta WGS84 " 
#define refELLIPSOID    "ellipsoid   "
#define refMAJOR_SAX    "major s-ax  " 
#define refMINOR_SAX    "minor s-ax  "
#define refORIGIN_LONG  "origin long "
#define refORIGIN_LAT   "origin lat  "
#define refORIGIN_X     "origin X    "
#define refORIGIN_Y     "origin Y    "
#define refSCALE_FAC    "scale fac   "
#define refUNITS        "units       "
#define refSTANDL_1     "stand ln 1  "
#define refSTANDL_2     "stand ln 2  "

// file ".smp" header size
#define smpHEADERSIZE	18

// import idrisi ".ref"
OGRErr OSRImportFromIdrisi(OGRSpatialReferenceH hSRS, char **papszPrj);

// remove left (' ')`s
char *trimL(char *pszText);

class IdrisiDataset;
class IdrisiRasterBand;

class IdrisiDataset : public GDALDataset
{
    friend class IdrisiRasterBand;

    FILE *fp;

    const char *pszDocFilename;
    char **papszRDC;
    char *pszFilename;
    char *pszRefFilename;
    double adfGeoTransform[6];
    char *pszIdrisiPath;

public:
    IdrisiDataset();
    ~IdrisiDataset();

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Create(const char *pszFilename,
        int nXSize,
        int nYSize,
        int nBands, 
        GDALDataType eType,
        char **papszParmList);
    static GDALDataset *CreateCopy(const char *pszFilename, 
        GDALDataset *poSrcDS,
        int bStrict,
        char **papszOptions,
        GDALProgressFunc pfnProgress, 
        void * pProgressData);

    virtual CPLErr GetGeoTransform(double * padfTransform);
    virtual CPLErr SetGeoTransform(double * padfTransform);
    virtual const char *GetProjectionRef(void);
    virtual CPLErr  SetProjection(const char * pszProjString);
};

class IdrisiRasterBand : public GDALRasterBand
{
    friend class IdrisiDataset;

    int nRecordSize;
    GByte *pabyScan;
    GDALColorTable *poColorTable;
    char **papszCategoryNames;

public:
    IdrisiRasterBand(IdrisiDataset *poDS, 
        int nBand, 
        GDALDataType eDataType);
    ~IdrisiRasterBand();

    virtual double GetMinimum(int *pbSuccess = NULL);
    virtual double GetMaximum(int *pbSuccess = NULL);
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage);
    virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void *pImage);
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual char **GetCategoryNames();

    virtual CPLErr SetCategoryNames(char **);
    virtual CPLErr SetNoDataValue(double);
    virtual CPLErr SetColorTable(GDALColorTable *); 
    virtual CPLErr SetScale(double);
    virtual CPLErr SetUnitType(const char *);
    virtual CPLErr SetStatistics(double dfMin, double dfMax, 
        double dfMean, double dfStdDev);

};

//  ----------------------------------------------------------------------------
//	    Implementation of IdrisiDataset
//  ----------------------------------------------------------------------------

IdrisiDataset::IdrisiDataset()
{
    pszFilename = NULL;
    fp = NULL;
    pszRefFilename = NULL;
    papszRDC = NULL;
    pszIdrisiPath = NULL;
    pszDocFilename = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

IdrisiDataset::~IdrisiDataset()
{
    FlushCache();

    if (eAccess == GA_Update)
    {
        CSLSave(papszRDC, pszDocFilename);
    }

    if (papszRDC != NULL)
    {
        CSLDestroy(papszRDC);
    }

    CPLFree(pszIdrisiPath);
    CPLFree(pszRefFilename);
    CPLFree(pszFilename);
}

GDALDataset *IdrisiDataset::Open(GDALOpenInfo * poOpenInfo)
{
    if (poOpenInfo->fp == NULL)
        return NULL;

    // -------------------------------------------------------------------- 
    //      Check the documentation file .rdc
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Check the documentation file .rdc");

    const char *pszDocFilename;
    char **papszRDC;

    pszDocFilename = CPLResetExtension(poOpenInfo->pszFilename, extRDC);
    papszRDC = CSLLoad(pszDocFilename);

    char *pszVersion;
    pszVersion = trimL(CPLStrdup(CSLFetchNameValue(papszRDC, rdcFILE_FORMAT)));

    if (EQUAL(pszVersion, rstVERSION) == FALSE)
    {
        CSLDestroy(papszRDC);
        CPLFree(pszVersion);
        return NULL;
    }

    CPLFree(pszVersion);

    // -------------------------------------------------------------------- 
    //      Create a corresponding GDALDataset                   
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Create a corresponding GDALDataset");

    IdrisiDataset *poDS;

    poDS = new IdrisiDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);
    poDS->papszRDC = papszRDC;

    if (poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fp = VSIFOpenL(poDS->pszFilename, "rb");
        poOpenInfo->fp = NULL;
    } 
    else 
    {
        poDS->fp = VSIFOpenL(poDS->pszFilename, "r+b");
        poOpenInfo->fp = NULL;
    }

    if (poDS->fp == NULL)
        return NULL;

    // -------------------------------------------------------------------- 
    //      Check for the Idrisi Path Installation
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Check for the Idrisi Path Installation");

    if (getenv("IDRISIDIR") == NULL)
    {
        poDS->pszIdrisiPath = NULL;
    }
    else
    {
        poDS->pszIdrisiPath = CPLStrdup(getenv("IDRISIDIR"));
    }

    CPLDebug(extRST, "IDRISIDIR %s", poDS->pszIdrisiPath);

    // -------------------------------------------------------------------- 
    //      Load information from rdc
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Load information from rdc");

    sscanf(CSLFetchNameValue(papszRDC, rdcCOLUMNS), "%d", &poDS->nRasterXSize);
    sscanf(CSLFetchNameValue(papszRDC, rdcROWS),    "%d", &poDS->nRasterYSize);

    // -------------------------------------------------------------------- 
    //      Load the transformation matrix
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Load the transformation matrix");

    double dfMinX, dfMaxX, dfMinY, dfMaxY, dfUnit;

    sscanf(CSLFetchNameValue(papszRDC, rdcMIN_X),    "%lf", &dfMinX);
    sscanf(CSLFetchNameValue(papszRDC, rdcMAX_X),    "%lf", &dfMaxX);
    sscanf(CSLFetchNameValue(papszRDC, rdcMIN_Y),    "%lf", &dfMinY);
    sscanf(CSLFetchNameValue(papszRDC, rdcMAX_Y),    "%lf", &dfMaxY);
    sscanf(CSLFetchNameValue(papszRDC, rdcUNIT_DIST),"%lf", &dfUnit);

    poDS->adfGeoTransform[0] = dfMinX;
    poDS->adfGeoTransform[1] = (dfMaxX - dfMinX) / (dfUnit * poDS->nRasterXSize);
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = dfMaxY;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = (dfMinY - dfMaxY) / (dfUnit * poDS->nRasterYSize);

    // -------------------------------------------------------------------- 
    //      Create band information
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Create band information");

    char *pszDataType;

    pszDataType = trimL(CPLStrdup(CSLFetchNameValue(papszRDC, rdcDATA_TYPE)));

    CPLDebug(extRST, "DataType=%s", pszDataType);

    if (EQUAL(pszDataType, rstBYTE))
    {
        poDS->nBands = 1;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Byte));
    }
    if (EQUAL(pszDataType, rstINTEGER))
    {
        poDS->nBands = 1;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Int16));
    }
    if (EQUAL(pszDataType, rstREAL))
    {
        poDS->nBands = 1;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Float32));
    }
    if (EQUAL(pszDataType, rstRGB24))
    {
        poDS->nBands = 3;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Byte));
        poDS->SetBand(2, new IdrisiRasterBand(poDS, 2, GDT_Byte));
        poDS->SetBand(3, new IdrisiRasterBand(poDS, 3, GDT_Byte));
    }

    CPLFree(pszDataType);

    // -------------------------------------------------------------------- 
    //      Load Geographic Reference
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Load Geographic Reference");

    char *pszRefSystem;

    pszRefSystem = trimL(CPLStrdup(CSLFetchNameValue(papszRDC, rdcREF_SYSTEM)));

    if (EQUAL(pszRefSystem, rstLATLONG))
    {
        OGRSpatialReference oSRS;
        oSRS.SetWellKnownGeogCS("WGS84");
        oSRS.exportToWkt(&poDS->pszRefFilename);
    }
    else if (EQUAL(pszRefSystem, rstPLANE))
    {
        poDS->pszRefFilename = CPLStrdup("");
    }
    else if (EQUALN(pszRefSystem, rstUTM, 3))
    {
        char nZone;
        char cNorth;
        sscanf(pszRefSystem, rstUTM, &nZone, &cNorth);
        OGRSpatialReference oSRS;
        oSRS.SetProjCS(&pszRefSystem[1]);
        oSRS.SetWellKnownGeogCS("WGS84");
        oSRS.SetUTM(nZone, (cNorth == 'n'));
        oSRS.exportToWkt(&poDS->pszRefFilename);
    }
    else
    {
        if (poDS->pszIdrisiPath != NULL)
        {
            char *pszRefFile;
            pszRefFile = CPLStrdup(CPLSPrintf("%sGeoref\\%s.ref", poDS->pszIdrisiPath, &pszRefSystem[1]));
            char **papszIdrisiRef;
            papszIdrisiRef = CSLLoad(pszRefFile);
            OGRSpatialReference oSRS;
            OSRImportFromIdrisi(&oSRS, papszIdrisiRef);
            oSRS.exportToWkt(&poDS->pszRefFilename);
            CSLDestroy(papszIdrisiRef);
        }
        else
        {
            poDS->pszRefFilename = CPLStrdup("unidentified");
        }

    }

    return (poDS);
}

GDALDataset *IdrisiDataset::Create(const char *pszFilename,
                                   int nXSize, 
                                   int nYSize, 
                                   int nBands, 
                                   GDALDataType eType,
                                   char** papszOptions)
{
    // -------------------------------------------------------------------- 
    //      Check input options
    // -------------------------------------------------------------------- 

    if (eType != GDT_Byte && 
        eType != GDT_Int16 && 
        eType != GDT_UInt16 && 
        eType != GDT_UInt32 && 
        eType != GDT_Int32 && 
        eType != GDT_Float32 &&
        eType != GDT_Float64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Attempt to create IDRISI dataset with an illegal\n"
            "data type (%s).\n",
            GDALGetDataTypeName(eType));
        return NULL;
    }

    if (nBands != 1) 
    {
        if (! (nBands == 3 && eType == GDT_Byte))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Attempt to create IDRISI dataset with an illegal\n"
                "number of bands (%d) to data type (%s).\n",
                nBands, GDALGetDataTypeName(eType));
            return NULL;
        }
    }

    // ---------------------------------------------------------------- 
    //  Create the header file with minimun information
    // ---------------------------------------------------------------- 

    char *pszDataType;

    switch (eType)
    {	
    case GDT_Byte:
        if (nBands == 1)
        {
            pszDataType = CPLStrdup(rstBYTE);
        }
        else
        {
            pszDataType = CPLStrdup(rstRGB24);
        }
        break;
    case GDT_Int16:
        pszDataType = CPLStrdup(rstINTEGER);
        break;
    case GDT_Float32:				
        pszDataType = CPLStrdup(rstREAL);
        break;
    };

    const char *pszDocFilename;
    char **papszRDC;

    pszDocFilename = CPLResetExtension(pszFilename, extRDC);
    papszRDC = CSLLoad(pszDocFilename);

    CSLAddNameValue(papszRDC, rdcFILE_FORMAT,   rstVERSION);
    CSLAddNameValue(papszRDC, rdcFILE_TITLE,    "");
    CSLAddNameValue(papszRDC, rdcDATA_TYPE,     pszDataType);
    CSLAddNameValue(papszRDC, rdcFILE_TYPE,     "binary");
    CSLAddNameValue(papszRDC, rdcCOLUMNS,       CPLSPrintf("%d", nXSize));
    CSLAddNameValue(papszRDC, rdcROWS,          CPLSPrintf("%d", nYSize));
    CSLAddNameValue(papszRDC, rdcREF_SYSTEM,    "plane");
    CSLAddNameValue(papszRDC, rdcREF_UNITS,     "meters");
    CSLAddNameValue(papszRDC, rdcUNIT_DIST,     "1.0");
    CSLAddNameValue(papszRDC, rdcMIN_X,         "0.0");
    CSLAddNameValue(papszRDC, rdcMAX_X,         "0.0");
    CSLAddNameValue(papszRDC, rdcMIN_Y,         "0.0");
    CSLAddNameValue(papszRDC, rdcMAX_Y,         "0.0");
    CSLAddNameValue(papszRDC, rdcPOSN_ERROR,    "unspecified");
    CSLAddNameValue(papszRDC, rdcRESOLUTION,    "1.0");
    CSLAddNameValue(papszRDC, rdcMIN_VALUE,     "0.0");
    CSLAddNameValue(papszRDC, rdcMAX_VALUE,     "255.0");
    CSLAddNameValue(papszRDC, rdcDISPLAY_MIN,   "0.0");
    CSLAddNameValue(papszRDC, rdcDISPLAY_MAX,   "255.0");
    CSLAddNameValue(papszRDC, rdcVALUE_UNITS,   "unspecified");
    CSLAddNameValue(papszRDC, rdcVALUE_ERROR,   "unspecified");
    CSLAddNameValue(papszRDC, rdcFLAG_VALUE,    "none");
    CSLAddNameValue(papszRDC, rdcFLAG_DEFN,     "none");
    CSLAddNameValue(papszRDC, rdcLEGEND_CATS,   "0");
    CSLAddNameValue(papszRDC, rdcLINEAGES,      "");
    CSLAddNameValue(papszRDC, rdcCOMMENTS,      "Generated by GDAL");

    for (int i = 0; i < CSLCount(papszOptions); i++)
    {
        CSLAddNameValue(papszRDC, rdcCOMMENTS, papszOptions[i]);
    }

    CSLSave(papszRDC, pszDocFilename);
    CSLDestroy(papszRDC);

    // ---------------------------------------------------------------- 
    //  Create an empty data file
    // ---------------------------------------------------------------- 

    FILE *fp;

    fp = VSIFOpenL(pszFilename, "wb+");

    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
            "Attempt to create file %s' failed.\n", pszFilename);
        return NULL;
    }
    VSIFCloseL(fp);

    return (IdrisiDataset *) GDALOpen(pszFilename, GA_Update);
}

GDALDataset *IdrisiDataset::CreateCopy(const char *pszFilename, 
                                       GDALDataset *poSrcDS,
                                       int bStrict,
                                       char **papszOptions,
                                       GDALProgressFunc pfnProgress, 
                                       void * pProgressData)
{
    // -------------------------------------------------------------------------
    //      Check number of bands
    // -------------------------------------------------------------------------

    if  (((poSrcDS->GetRasterCount() == 1) ||
        ((poSrcDS->GetRasterCount() == 3) &&
        ((poSrcDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte) &&
        (poSrcDS->GetRasterBand(2)->GetRasterDataType() == GDT_Byte) &&
        (poSrcDS->GetRasterBand(3)->GetRasterDataType() == GDT_Byte)))) == FALSE)
    {

        CPLError(CE_Failure, CPLE_AppDefined,
            "Attempt IDRISI dataset with an illegal\n"
            "number of bands (%s).\n",
            poSrcDS->GetRasterCount());
        return NULL;
    }

    // -------------------------------------------------------------------------
    //      Check Data types
    // -------------------------------------------------------------------------

    for (int i = 1; i <= poSrcDS->GetRasterCount(); i++)
    {
        GDALDataType eType = poSrcDS->GetRasterBand(i)->GetRasterDataType();

        if (eType != GDT_Byte && 
            eType != GDT_Int16 && 
            eType != GDT_UInt16 && 
            eType != GDT_UInt32 && 
            eType != GDT_Int32 && 
            eType != GDT_Float32 &&
            eType != GDT_Float64)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Attempt to create IDRISI dataset with an illegal\n"
                "data type (%s).\n",
                GDALGetDataTypeName(eType));
            return NULL;
        }
    }

    // --------------------------------------------------------------------
    //      Define data type 
    // --------------------------------------------------------------------

    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);
    GDALDataType eType = poBand->GetRasterDataType();

    if ((eType == GDT_Byte) || 
        (eType == GDT_Int16) || 
        (eType == GDT_Float32))
    {
        CPLDebug(extRST, "Keep the same data type %s", GDALGetDataTypeName(eType));
    }
    else
    {
        if (eType == GDT_Float64)
        {
            eType = GDT_Float32;
            CPLDebug(extRST, "Change (1) from %s to %s", 
                GDALGetDataTypeName(poBand->GetRasterDataType()),
                GDALGetDataTypeName(eType)
                );
        }
        else
        {
            if ((poBand->GetMinimum() < (double) SHRT_MIN) && 
                (poBand->GetMaximum() > (double) SHRT_MAX))
            {
                eType = GDT_Float32; 
                CPLDebug(extRST, "Change (2) from %s to %s", 
                    GDALGetDataTypeName(poBand->GetRasterDataType()),
                    GDALGetDataTypeName(eType)
                    );
            }
            else
            {
                eType = GDT_Int16;
                CPLDebug(extRST, "Change (3) from %s to %s", 
                    GDALGetDataTypeName(poBand->GetRasterDataType()),
                    GDALGetDataTypeName(eType)
                    );
            }
        }
    }

    // --------------------------------------------------------------------
    //      Create the dataset
    // --------------------------------------------------------------------
    CPLDebug(extRST, "Create the dataset");

    IdrisiDataset *poDS;

    poDS = (IdrisiDataset *) IdrisiDataset::Create(pszFilename, 
        poSrcDS->GetRasterXSize(), 
        poSrcDS->GetRasterYSize(), 
        poSrcDS->GetRasterCount(), 
        eType, 
        papszOptions);

    if (poDS == NULL)
        return NULL;

    // --------------------------------------------------------------------
    //      Copy information to the dataset
    // --------------------------------------------------------------------
    CPLDebug(extRST, "Copy information to the dataset");

    double adfGeoTransform[6];

    poDS->SetProjection(poSrcDS->GetProjectionRef());
    poSrcDS->GetGeoTransform(adfGeoTransform);
    poDS->SetGeoTransform(adfGeoTransform);

    // --------------------------------------------------------------------
    //      Copy information to the raster band
    // --------------------------------------------------------------------
    CPLDebug(extRST, "Copy information to the raster band");

    GDALRasterBand *poSrcBand;

    for (int i = 1; i <= poDS->nBands; i++)
    {
        poSrcBand = poSrcDS->GetRasterBand(i);
        poBand = poDS->GetRasterBand(i);

        CPLDebug(extRST, "CreateCopy:Creating Band %d", i);

        poBand->SetCategoryNames(poSrcBand->GetCategoryNames());
        poBand->SetColorInterpretation(poSrcBand->GetColorInterpretation());
        poBand->SetColorTable(poSrcBand->GetColorTable());
        poBand->SetStatistics(poSrcBand->GetMinimum(), poSrcBand->GetMaximum(), 0.0, 0.0);
        poBand->SetNoDataValue(poSrcBand->GetNoDataValue(NULL));
    }

    // --------------------------------------------------------------------
    //      Copy image data
    // --------------------------------------------------------------------
    CPLDebug(extRST, "Copy image data"); 

    int nXSize = poDS->GetRasterXSize();
    int nYSize = poDS->GetRasterYSize();
    int nBlockXSize, nBlockYSize;

    poDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    for (int iBand = 1; iBand <= poSrcDS->GetRasterCount(); iBand++)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand);
        GDALRasterBand *poDstBand = poDS->GetRasterBand(iBand);

        int	iYOffset, iXOffset;
        void *pData;

        pData = CPLMalloc(nBlockXSize * nBlockYSize * GDALGetDataTypeSize(eType) / 8);

        for (iYOffset = 0; iYOffset < nYSize; iYOffset += nBlockYSize)
        {
            for (iXOffset = 0; iXOffset < nXSize; iXOffset += nBlockXSize)
            {
                if (poSrcBand->RasterIO(GF_Read, 
                    iXOffset, iYOffset, 
                    nBlockXSize, nBlockYSize,
                    pData, nBlockXSize, nBlockYSize,
                    eType, 0, 0))
                {
                    return NULL;
                }

                if (poDstBand->RasterIO(GF_Write, 
                    iXOffset, iYOffset, 
                    nBlockXSize, nBlockYSize,
                    pData, nBlockXSize, nBlockYSize,
                    eType, 0, 0))
                {
                    return NULL;
                }
            }
        }
        CPLFree(pData);
    }

    // --------------------------------------------------------------------
    //      Finilize
    // --------------------------------------------------------------------

    poDS->FlushCache();

    return poDS;
}

CPLErr  IdrisiDataset::GetGeoTransform(double * padfTransform)
{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return CE_None;
}

CPLErr  IdrisiDataset::SetGeoTransform(double * padfTransform)
{
    if (padfTransform[2] != 0.0 || padfTransform[4] != 0.0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Attempt to set rotated geotransform on Idrisi Raster file.\n"
            "Idrisi Raster does not support rotation.\n");
        return CE_Failure;
    }

    double dfMin_X = padfTransform[0];
    double dfMax_X = (padfTransform[1] * nRasterXSize) + padfTransform[0];
    double dfMin_Y = padfTransform[3] - (-padfTransform[5] * nRasterYSize);
    double dfMax_Y = padfTransform[3];
    double dfResolution = -padfTransform[5];
    double dfUnitDist = 1.0;

    CSLSetNameValue(papszRDC, rdcMIN_X,       CPLSPrintf("%g.8", dfMin_X));
    CSLSetNameValue(papszRDC, rdcMAX_X,       CPLSPrintf("%g.8", dfMax_X));
    CSLSetNameValue(papszRDC, rdcMIN_Y,       CPLSPrintf("%g.8", dfMin_Y));
    CSLSetNameValue(papszRDC, rdcMAX_Y,       CPLSPrintf("%g.8", dfMax_Y));
    CSLSetNameValue(papszRDC, rdcRESOLUTION,  CPLSPrintf("%g.8", dfResolution));
    CSLSetNameValue(papszRDC, rdcUNIT_DIST,   CPLSPrintf("%g.8", dfUnitDist));

    return CE_None;
}

const char *IdrisiDataset::GetProjectionRef(void)
{
    return pszRefFilename;
}

CPLErr IdrisiDataset::SetProjection(const char * pszProjString)
{
    //?? TODO

    CPLDebug(extRST, "SetProjection");

    return CE_None;
}

//  ----------------------------------------------------------------------------
//  //  //
//	//			Implementation of IdrisiRasterBand
//
//  ----------------------------------------------------------------------------

IdrisiRasterBand::IdrisiRasterBand(IdrisiDataset *poDS, 
                                   int nBand, 
                                   GDALDataType eDataType)
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;

    poColorTable = NULL;

    // -------------------------------------------------------------------- 
    //      Set Dimension
    // -------------------------------------------------------------------- 

    nBlockYSize = 1;
    nBlockXSize = poDS->GetRasterXSize();
    nRecordSize = poDS->GetRasterXSize() * GDALGetDataTypeSize(eDataType) / 8 * poDS->nBands;

    pabyScan = (GByte *) CPLMalloc(nRecordSize);

    // -------------------------------------------------------------------- 
    //      Set Color Table only form thematic images
    // -------------------------------------------------------------------- 

    if (GetColorInterpretation() == GCI_PaletteIndex)
    {
        const char *pszSMPFilename;
        pszSMPFilename = CPLResetExtension(poDS->pszFilename, extSMP);
        FILE *fpSMP;
        if ((fpSMP = VSIFOpenL(pszSMPFilename, "rb")) != NULL )
        {
            VSIFSeekL(fpSMP, smpHEADERSIZE, SEEK_SET);
            poColorTable = new GDALColorTable();
            GDALColorEntry oEntry;
            unsigned char aucRGB[3];
            int i = 0;
            while ((VSIFReadL(&aucRGB, sizeof(aucRGB), 1, fpSMP)) && (i <= GetMaximum()))
            {
                oEntry.c1 = (short) aucRGB[0];
                oEntry.c2 = (short) aucRGB[1];
                oEntry.c3 = (short) aucRGB[2];
                oEntry.c4 = (short) 255;                      
                poColorTable->SetColorEntry(i, &oEntry);
                i++;
            }
            VSIFCloseL(fpSMP);
        }
    }

    // -------------------------------------------------------------------- 
    //      Set Category Names only form thematic images
    // -------------------------------------------------------------------- 

    papszCategoryNames = NULL;

    if (GetColorInterpretation() == GCI_PaletteIndex)
    {
        for (int i = GetMinimum(); i <= GetMaximum(); i++)
        {
            char *pszLabel = CPLStrdup(CSLFetchNameValue(poDS->papszRDC, CPLSPrintf(rdcCODE_N, i)));
            if (pszLabel == NULL)
            {
                papszCategoryNames = CSLAddString(papszCategoryNames, "");
            }
            else
            {
                papszCategoryNames = CSLAddString(papszCategoryNames, pszLabel);
            }
            CPLFree(pszLabel);
        }
    }
}

IdrisiRasterBand::~IdrisiRasterBand()
{
    if( poColorTable != NULL )
        delete poColorTable;

    CPLFree(pabyScan);
    CSLDestroy(papszCategoryNames);
    CPLFree(papszCategoryNames);
}

double IdrisiRasterBand::GetMinimum(int *pbSuccess)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;
    double adfMinValue[3];
    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMIN_VALUE), "%lf %lf %lf", &adfMinValue[0], &adfMinValue[1], &adfMinValue[2]);
    return adfMinValue[this->nBand - 1];
}

double IdrisiRasterBand::GetMaximum(int *pbSuccess)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;
    double adfMaxValue[3];
    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMAX_VALUE), "%lf,%lf %lf", &adfMaxValue[0], &adfMaxValue[1], &adfMaxValue[2]);
    return adfMaxValue[this->nBand - 1];
}

CPLErr IdrisiRasterBand::IReadBlock(int nBlockXOff, 
                                    int nBlockYOff,
                                    void *pImage)
{
    CPLDebug(extRST, "Begin: IdrisiRasterBand::IReadBlock");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CPLDebug(extRST, "nBlockXOff=%d, nBlockYOff=%d, nRecordSize=%d, nBlockXSize=%d, nBlockYSize=%d",
        nBlockXOff, nBlockYOff, nRecordSize, nBlockXSize, nBlockYSize);

    if (VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET) < 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, 
            "Can't seek (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror(errno));
        return CE_Failure;
    }

    if (VSIFReadL(pabyScan, 1, nRecordSize, poGDS->fp) < nRecordSize)
    {
        CPLError(CE_Failure, CPLE_FileIO, 
            "Can't read (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror(errno));
        return CE_Failure;
    }

    if (poGDS->nBands == 3) 
    {
        int i, j;
        for (i = 0, j = (3 - nBand); i < nBlockXSize; i++, j += 3)
        {
            ((uint8 *) pImage)[i] = pabyScan[j];
        }
    }
    else
    {
        memcpy(pImage, pabyScan, nRecordSize);
    }

    CPLDebug(extRST, "End: IdrisiRasterBand::IReadBlock");

    return CE_None;
}

CPLErr IdrisiRasterBand::IWriteBlock(int nBlockXOff, 
                                     int nBlockYOff,
                                     void *pImage)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (poGDS->nBands == 1)
    {
        memcpy(pabyScan, pImage, nRecordSize);
    }
    else
    {
        if (nBand > 1) 
        {
            VSIFReadL(pabyScan, 1, nRecordSize, poGDS->fp);
            VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);
        }
        int i, j;
        for (i = 0, j = (3 - nBand); i < nBlockXSize; i++, j += 3)
        {
            pabyScan[j] = ((uint8 *) pImage)[i];
        }
    }

    VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);

    if (VSIFWriteL(pabyScan, 1, nRecordSize, poGDS->fp) < nRecordSize)
    {
        CPLError(CE_Failure, CPLE_FileIO, 
            "Can't write (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror(errno));
        return CE_Failure;
    }

    return CE_None;
}

GDALColorInterp IdrisiRasterBand::GetColorInterpretation()
{
    CPLDebug(extRST, "IdrisiRasterBand::GetColorInterpretation");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    char *pszDataType = CPLStrdup(CSLFetchNameValue(poGDS->papszRDC, rdcDATA_TYPE));

    trimL(pszDataType);

    if (EQUAL(pszDataType, rstBYTE))
    {
        int nCount;
        sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcLEGEND_CATS), "%d", &nCount);
        return (nCount == 0 ? GCI_GrayIndex : GCI_PaletteIndex);
    }
    if (EQUAL(pszDataType, rstINTEGER))
    {
        return GCI_GrayIndex;
    }
    if (EQUAL(pszDataType, rstREAL))
    {
        return GCI_GrayIndex;
    }
    if (EQUAL(pszDataType, rstRGB24))
    {
        switch (nBand)
        {
        case 1: return GCI_BlueBand;
        case 2: return GCI_GreenBand;
        case 3: return GCI_RedBand;
        };
    }

    return GCI_Undefined;
}

char **IdrisiRasterBand::GetCategoryNames()
{
    return papszCategoryNames;
}

GDALColorTable *IdrisiRasterBand::GetColorTable()
{
    CPLDebug(extRST, "IdrisiRasterBand::GetColorTable");
    
    return poColorTable;
}

CPLErr IdrisiRasterBand::SetCategoryNames(char **papszCategoryNames)
{
    //?? TODO - Test it

    CPLDebug(extRST, "SetCategoryNames");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    int nCount = CSLCount(papszCategoryNames);

    CSLAddNameValue(poGDS->papszRDC, rdcLEGEND_CATS, CPLSPrintf("%d", nCount));

    int index = CSLFindString(poGDS->papszRDC,         
        CPLSPrintf("%s: %d", rdcLEGEND_CATS,nCount));

    char *pszName;
    char *pszNameValue;

    for (int i = 0; i < nCount; i++)
    {
        pszName = CPLStrdup(CPLSPrintf("%s: %d", rdcCODE_N, i));
        pszNameValue = CPLStrdup(CPLSPrintf("%s: %s", pszName, papszCategoryNames[i]));
        CSLInsertString(poGDS->papszRDC, (index + i + 1), pszNameValue);
        CPLFree(pszName);
        CPLFree(pszNameValue);
    }

    return CE_None;
}
CPLErr IdrisiRasterBand::SetNoDataValue(double dfNoDataValue)
{
    //?? TODO - Test it

    CPLDebug(extRST, "SetNoDataValue");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLAddNameValue(poGDS->papszRDC, rdcFLAG_VALUE,    CPLSPrintf("%g", dfNoDataValue));
    CSLAddNameValue(poGDS->papszRDC, rdcFLAG_DEFN,     "Missing Data");

    return CE_None;
}

CPLErr IdrisiRasterBand::SetColorTable(GDALColorTable *aColorTable)
{
    //?? TODO

    CPLDebug(extRST, "SetColorTable");

    return CE_None;
}

CPLErr IdrisiRasterBand::SetScale(double dfScale)
{
    //?? TODO - Test it

    CPLDebug(extRST, "Begin: SetScale");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLAddNameValue(poGDS->papszRDC, rdcRESOLUTION,    CPLSPrintf("%g", dfScale));

    return CE_None;
}

CPLErr IdrisiRasterBand::SetUnitType(const char *pszUnitType)
{
    //?? TODO

    CPLDebug(extRST, "SetUnitType");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLAddNameValue(poGDS->papszRDC, rdcREF_UNITS, pszUnitType);

    return CE_None;
}

CPLErr IdrisiRasterBand::SetStatistics(double dfMin, double dfMax, double dfMean, double dfStdDev)
{
    CPLDebug(extRST, "Begin: SetStatistics dfMin=%f dfMax=%f", dfMin, dfMax);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLSetNameValue(poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf("%g.8", dfMax));
    CSLSetNameValue(poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf("%g.8", dfMax));
    CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf("%g.8", dfMax));
    CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf("%g.8", dfMax));

    return CE_None;
}

char *trimL(char *pszText)
{
    int i = 0;
    int j = 0;
    while ((pszText[i] == ' ') && (pszText[i] != '\0')) 
        i++;
    while (pszText[i] != '\0') 
        pszText[j++] = pszText[i++];
    pszText[j] = '\0';
    return pszText;
}

OGRErr OSRImportFromIdrisi(OGRSpatialReferenceH hSRS, char **papszPrj)
{
    char *pszProj;
    char *pszRefSystem;
    double dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing;
    double dfStdP1, dfStdP2;

    pszRefSystem = trimL(CPLStrdup(CSLFetchNameValue(papszPrj, refREF_SYSTEM)));
    pszProj = trimL(CPLStrdup(CSLFetchNameValue(papszPrj, refPROJECTION)));

    CPLDebug(extRST, "\npszRefSystem=%s,\n pszProj=%s", pszRefSystem, pszProj);

    sscanf(CSLFetchNameValue(papszPrj, refORIGIN_LAT),   "%lf", &dfCenterLat);
    sscanf(CSLFetchNameValue(papszPrj, refORIGIN_LONG),  "%lf", &dfCenterLong);
    sscanf(CSLFetchNameValue(papszPrj, refORIGIN_X),     "%lf", &dfFalseEasting);
    sscanf(CSLFetchNameValue(papszPrj, refORIGIN_Y),     "%lf", &dfFalseNorthing);
    sscanf(CSLFetchNameValue(papszPrj, refSTANDL_1),     "%lf", &dfStdP1);
    sscanf(CSLFetchNameValue(papszPrj, refSTANDL_2),     "%lf", &dfStdP2);

    if (EQUAL(CSLFetchNameValue(papszPrj, refSCALE_FAC), " na"))
    {
        dfScale = 1.0;
    }
    else
    {
        sscanf(CSLFetchNameValue(papszPrj, refSCALE_FAC),   "%lf", &dfScale);
    }

    if (EQUAL(pszProj,"Mercator"))
    {
        OSRSetMercator(hSRS, dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"Transverse Mercator"))
    {
        OSRSetTM(hSRS, dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"Lambert Conformal Conic"))
    {
        OSRSetLCC(hSRS, dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"Lambert North Polar Azimuthal Equal Area"))
    {
        OSRSetLAEA(hSRS, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"Lambert South Polar Azimuthal Equal Area"))
    {
        OSRSetLAEA(hSRS, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"North Polar Stereographic"))
    {
        OSRSetPS(hSRS, dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"South Polar Stereographic"))
    {
        OSRSetPS(hSRS, dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"Transverse Stereographic"))
    {
        OSRSetPS(hSRS, dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"Oblique Stereographic"))
    {
        OSRSetStereographic(hSRS, dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
    }
    else if (EQUAL(pszProj,"Alber''s Equal Area Conic"))
    {
        OSRSetACEA(hSRS, dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }
    else
    {
        //?? TODO
    
        // Create your own...
    }
    CPLFree(pszProj);
    CPLFree(pszRefSystem);
    return OGRERR_NONE;
}

void GDALRegister_IDRISI()
{
    GDALDriver  *poDriver;

    if (GDALGetDriverByName("IDRISI") == NULL)
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription("IDRISI");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, rstVERSION);
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_various.html#IDRISI");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, extRST);
        poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int16 Float32");
        poDriver->pfnOpen = IdrisiDataset::Open;
        poDriver->pfnCreate	= IdrisiDataset::Create;
        poDriver->pfnCreateCopy = IdrisiDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}
