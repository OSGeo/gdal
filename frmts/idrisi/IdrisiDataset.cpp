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

// ".rdc" file field names:
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

// ".rdc" file field standard values:
#define rstVERSION      "Idrisi Raster A.1"
#define rstBYTE         "byte"
#define rstINTEGER      "integer"
#define rstREAL         "real"
#define rstRGB24        "rgb24"
#define rstLATLONG      "latlong"
#define rstDEGREE       "degree"
#define rstPLANE        "plane"
#define rstMETER        "meter"
#define rstUTM          "utm-%d%c"

// ".ref" file field names:
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
#define refPARAMETERS   "parameters  "
#define refSTANDL_1     "stand ln 1  "
#define refSTANDL_2     "stand ln 2  "

// ".smp" file header size
#define smpHEADERSIZE	18

// import idrisi ".ref"
OGRErr OSRImportFromIdrisi(OGRSpatialReferenceH hSRS, char **papszGeoRef);

// export idrisi ".ref"
char **OSRExportToIdrisi(const char *pszProjString, char **papszGeoRef);

// remove left (' ')`s
char *TrimL(const char *pszText);

// check if file exists
bool FileExists(const char *pszFilename)

class IdrisiDataset;
class IdrisiRasterBand;

class IdrisiDataset : public GDALDataset
{
    friend class IdrisiRasterBand;

private:
    FILE *fp;

    char *pszFilename;
    char *pszDocFilename;
    char **papszRDC;
    char *pszIdrisiPath;
    double adfGeoTransform[6];

protected:
    GDALColorTable *poColorTable;
    char **papszCategoryNames;
    GDALColorInterp eColorInterp;

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

    virtual CPLErr GetGeoTransform(double *padfTransform);
    virtual CPLErr SetGeoTransform(double *padfTransform);
    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection(const char *pszProjString);
};

class IdrisiRasterBand : public GDALRasterBand
{
    friend class IdrisiDataset;

private:
    int nRecordSize;
    GByte *pabyScanLine;

public:
    IdrisiRasterBand(IdrisiDataset *poDS, 
        int nBand, 
        GDALDataType eDataType);
    ~IdrisiRasterBand();

    virtual double GetNoDataValue(int *pbSuccess = NULL);
    virtual double GetMinimum(int *pbSuccess = NULL);
    virtual double GetMaximum(int *pbSuccess = NULL);
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage);
    virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void *pImage);
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual char **GetCategoryNames();
    virtual CPLErr GetStatistics(int bApproxOK, int bForce,
        double *pdfMin, double *pdfMax, 
        double *pdfMean, double *padfStdDev);

    virtual CPLErr SetCategoryNames(char **);
    virtual CPLErr SetNoDataValue(double);
    virtual CPLErr SetColorTable(GDALColorTable *); 
    virtual CPLErr SetScale(double);
    virtual CPLErr SetUnitType(const char *);
    virtual CPLErr SetStatistics(double dfMin, double dfMax, 
        double dfMean, double dfStdDev);
    virtual CPLErr SetColorInterpretation(GDALColorInterp);
};

//  ----------------------------------------------------------------------------
//	    Implementation of IdrisiDataset
//  ----------------------------------------------------------------------------

IdrisiDataset::IdrisiDataset()
{
    pszFilename = NULL;
    fp = NULL;
    papszRDC = NULL;
    pszIdrisiPath = NULL;
    pszDocFilename = NULL;
    poColorTable = new GDALColorTable();
    papszCategoryNames = NULL;

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


    if (papszRDC != NULL)
    {
        if (eAccess == GA_Update)
        {
            CSLSave(papszRDC, pszDocFilename);
        }
        CSLDestroy(papszRDC);
    }

    CSLDestroy(papszCategoryNames);
    delete poColorTable;
    CPLFree(pszIdrisiPath);
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

    char *pszVersion = TrimL(CSLFetchNameValue(papszRDC, rdcFILE_FORMAT));

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
        poDS->pszIdrisiPath = CPLStrdup("");
    }
    else
    {
        poDS->pszIdrisiPath = CPLStrdup(getenv("IDRISIDIR"));
    }

    CPLDebug(extRST, "set IDRISIDIR=%s", poDS->pszIdrisiPath);

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

    double dfMinX, dfMaxX, dfMinY, dfMaxY, dfUnit, dfXPixSz, dfYPixSz;

    sscanf(CSLFetchNameValue(papszRDC, rdcMIN_X),    "%lf", &dfMinX);
    sscanf(CSLFetchNameValue(papszRDC, rdcMAX_X),    "%lf", &dfMaxX);
    sscanf(CSLFetchNameValue(papszRDC, rdcMIN_Y),    "%lf", &dfMinY);
    sscanf(CSLFetchNameValue(papszRDC, rdcMAX_Y),    "%lf", &dfMaxY);
    sscanf(CSLFetchNameValue(papszRDC, rdcUNIT_DIST),"%lf", &dfUnit);

    dfMinX = dfMinX * dfUnit; 
    dfMaxX = dfMaxX * dfUnit; 
    dfMinY = dfMinY * dfUnit; 
    dfMaxY = dfMaxY * dfUnit;

    dfYPixSz = (dfMinY - dfMaxY) / poDS->nRasterYSize;
    dfXPixSz = (dfMaxX - dfMinX) / poDS->nRasterXSize;

    poDS->adfGeoTransform[0] = dfMinX - (dfXPixSz / 2);
    poDS->adfGeoTransform[1] = dfXPixSz;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = dfMaxY + (dfYPixSz / 2);
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = dfYPixSz;

    // -------------------------------------------------------------------- 
    //      Create band information
    // -------------------------------------------------------------------- 

    CPLDebug(extRST, "Create band information");

    char *pszDataType = TrimL(CSLFetchNameValue(papszRDC, rdcDATA_TYPE));

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
    //      Set Color Table in the presence of a smp file
    // -------------------------------------------------------------------- 

    if (poDS->nBands == 1)
    {
        CPLDebug(extRST, "Check for Palette file .smp");

        double dfMaxValue;
        sscanf(CSLFetchNameValue(papszRDC, rdcMAX_VALUE),    " %lf", &dfMaxValue);

        const char *pszSMPFilename;
        pszSMPFilename = CPLResetExtension(poDS->pszFilename, extSMP);

        FILE *fpSMP;
        if ((fpSMP = VSIFOpenL(pszSMPFilename, "rb")) != NULL )
        {
            VSIFSeekL(fpSMP, smpHEADERSIZE, SEEK_SET);
            GDALColorEntry oEntry;
            unsigned char aucRGB[3];
            int i = 0;
            while ((VSIFReadL(&aucRGB, sizeof(aucRGB), 1, fpSMP)) && (i <= dfMaxValue))
            {
                oEntry.c1 = (short) aucRGB[0];
                oEntry.c2 = (short) aucRGB[1];
                oEntry.c3 = (short) aucRGB[2];
                oEntry.c4 = (short) 255;                      
                poDS->poColorTable->SetColorEntry(i, &oEntry);
                i++;
            }
            VSIFCloseL(fpSMP);
        }
    }

    // -------------------------------------------------------------------- 
    //      Set Category Names only for thematic images
    // -------------------------------------------------------------------- 

    if (poDS->nBands == 1)
    {
        int nCount;
        sscanf(CSLFetchNameValue(papszRDC, rdcLEGEND_CATS),  " %d", &nCount);

        if (nCount > 0)
        {
            CPLDebug(extRST, "Load the Category Names");

            for (int i = 0; i < nCount; i++)
            {
                char *pszLabel = (char*) CSLFetchNameValue(papszRDC, CPLSPrintf(rdcCODE_N, i));
                if (pszLabel == NULL)
                {
                    poDS->papszCategoryNames = CSLAddString(poDS->papszCategoryNames, "");
                }
                else
                {
                    poDS->papszCategoryNames = CSLAddString(poDS->papszCategoryNames, pszLabel);
                }
                CPLFree(pszLabel);
            }
        }
    }

    poDS->pszDocFilename = CPLStrdup(pszDocFilename);
    poDS->papszRDC = papszRDC;

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

    CPLDebug(extRST, "Create the header file");

    char *pszDataType;

    switch (eType)
    {	
    case GDT_Byte:
        if (nBands == 1)
            pszDataType = CPLStrdup(rstBYTE);
        else
            pszDataType = CPLStrdup(rstRGB24);
        break;
    case GDT_Int16:
        pszDataType = CPLStrdup(rstINTEGER);
        break;
    case GDT_Float32:				
        pszDataType = CPLStrdup(rstREAL);
        break;
    };

    char **papszRDC;

    papszRDC = NULL;
    papszRDC = CSLAddNameValue(papszRDC, rdcFILE_FORMAT,   rstVERSION);
    papszRDC = CSLAddNameValue(papszRDC, rdcFILE_TITLE,    "");
    papszRDC = CSLAddNameValue(papszRDC, rdcDATA_TYPE,     pszDataType);
    papszRDC = CSLAddNameValue(papszRDC, rdcFILE_TYPE,     "binary");
    papszRDC = CSLAddNameValue(papszRDC, rdcCOLUMNS,       CPLSPrintf("%d", nXSize));
    papszRDC = CSLAddNameValue(papszRDC, rdcROWS,          CPLSPrintf("%d", nYSize));
    papszRDC = CSLAddNameValue(papszRDC, rdcREF_SYSTEM,    "");
    papszRDC = CSLAddNameValue(papszRDC, rdcREF_UNITS,     "");
    papszRDC = CSLAddNameValue(papszRDC, rdcUNIT_DIST,     "1");
    papszRDC = CSLAddNameValue(papszRDC, rdcMIN_X,         "");
    papszRDC = CSLAddNameValue(papszRDC, rdcMAX_X,         "");
    papszRDC = CSLAddNameValue(papszRDC, rdcMIN_Y,         "");
    papszRDC = CSLAddNameValue(papszRDC, rdcMAX_Y,         "");
    papszRDC = CSLAddNameValue(papszRDC, rdcPOSN_ERROR,    "unspecified");
    papszRDC = CSLAddNameValue(papszRDC, rdcRESOLUTION,    "");
    papszRDC = CSLAddNameValue(papszRDC, rdcMIN_VALUE,     "");
    papszRDC = CSLAddNameValue(papszRDC, rdcMAX_VALUE,     "");
    papszRDC = CSLAddNameValue(papszRDC, rdcDISPLAY_MIN,   "");
    papszRDC = CSLAddNameValue(papszRDC, rdcDISPLAY_MAX,   "");
    papszRDC = CSLAddNameValue(papszRDC, rdcVALUE_UNITS,   "unspecified");
    papszRDC = CSLAddNameValue(papszRDC, rdcVALUE_ERROR,   "unspecified");
    papszRDC = CSLAddNameValue(papszRDC, rdcFLAG_VALUE,    "none");
    papszRDC = CSLAddNameValue(papszRDC, rdcFLAG_DEFN,     "none");
    papszRDC = CSLAddNameValue(papszRDC, rdcLEGEND_CATS,   "");
    papszRDC = CSLAddNameValue(papszRDC, rdcLINEAGES,      "");
    papszRDC = CSLAddNameValue(papszRDC, rdcCOMMENTS,      "");

    const char *pszDocFilename;
    pszDocFilename = CPLResetExtension(pszFilename, extRDC);

    CSLSetNameValueSeparator(papszRDC, ": ");
    CSLSave(papszRDC, pszDocFilename);
    CSLDestroy(papszRDC);

    // ---------------------------------------------------------------- 
    //  Create an empty data file
    // ---------------------------------------------------------------- 

    CPLDebug(extRST, "Create an empty data file");

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
                                       void *pProgressData)
{
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

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

    double dfMin, dfMax;

    if (bStrict == TRUE)
    {
        poBand->GetStatistics(FALSE, TRUE, &dfMin, &dfMax, NULL, NULL);
    }
    else
    {
        dfMin = poBand->GetMinimum();
        dfMax = poBand->GetMaximum();
    }

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
            if ((dfMin < (double) SHRT_MIN) || 
                (dfMax > (double) SHRT_MAX))
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
        poBand->SetStatistics(dfMin, dfMax, NULL, NULL);
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
        CPLErr eErr = CE_None;

        pData = CPLMalloc(nBlockXSize * nBlockYSize * GDALGetDataTypeSize(eType) / 8);

        for (iYOffset = 0; iYOffset < nYSize; iYOffset += nBlockYSize)
        {
            for (iXOffset = 0; iXOffset < nXSize; iXOffset += nBlockXSize)
            {
                eErr = poSrcBand->RasterIO(GF_Read, 
                    iXOffset, iYOffset, 
                    nBlockXSize, nBlockYSize,
                    pData, nBlockXSize, nBlockYSize,
                    eType, 0, 0);
                if (eErr != CE_None)
                {
                    return NULL;
                }
                eErr = poDstBand->RasterIO(GF_Write, 
                    iXOffset, iYOffset, 
                    nBlockXSize, nBlockYSize,
                    pData, nBlockXSize, nBlockYSize,
                    eType, 0, 0);
                if (eErr != CE_None)
                {
                    return NULL;
                }
            }
            if ((eErr == CE_None) && (! pfnProgress(
                (iYOffset + 1) / (double) nYSize, NULL, pProgressData)))
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated CreateCopy()" );
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
    CPLDebug(extRST, "GetGeoTransform");

    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return CE_None;
}

CPLErr  IdrisiDataset::SetGeoTransform(double * padfGeoTransform)
{
    if (padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0.0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Attempt to set rotated geotransform on Idrisi Raster file.\n"
            "Idrisi Raster does not support rotation.\n");
        return CE_Failure;
    }

    double dfMinX, dfMaxX, dfMinY, dfMaxY, dfXPixSz, dfYPixSz;

    dfXPixSz = padfGeoTransform[1];
    dfYPixSz = padfGeoTransform[5];
    dfMinX   = padfGeoTransform[0] + (dfXPixSz / 2);
    dfMaxX   = (dfXPixSz * nRasterXSize) + dfMinX;
    dfMaxY   = padfGeoTransform[3] - (dfYPixSz / 2);
    dfMinY   = (dfYPixSz * nRasterYSize) + dfMaxY;

    CSLSetNameValue(papszRDC, rdcMIN_X,      CPLSPrintf(" %.8g", dfMinX));
    CSLSetNameValue(papszRDC, rdcMAX_X,      CPLSPrintf(" %.8g", dfMaxX));
    CSLSetNameValue(papszRDC, rdcMIN_Y,      CPLSPrintf(" %.8g", dfMinY));
    CSLSetNameValue(papszRDC, rdcMAX_Y,      CPLSPrintf(" %.8g", dfMaxY));
    CSLSetNameValue(papszRDC, rdcRESOLUTION, CPLSPrintf(" %.8g", -dfYPixSz));

    return CE_None;
}

const char *IdrisiDataset::GetProjectionRef(void)
{        CPLDebug(extRST, "GetProjectionRef");

    char *pszProjection;
    char *pszRefSystem;

    pszRefSystem = TrimL(CSLFetchNameValue(papszRDC, rdcREF_SYSTEM));

    CPLDebug(extRST, "Reference System=%s", pszRefSystem);

    OGRSpatialReference oSRS;

    if (EQUAL(pszRefSystem, rstPLANE))
    {
        pszProjection = CPLStrdup("");
    }
    else if (EQUAL(pszRefSystem, rstLATLONG))
    {
        oSRS.SetWellKnownGeogCS("WGS84");
        oSRS.exportToWkt(&pszProjection);
    }
    else if (EQUALN(pszRefSystem, rstUTM, 3))
    {
        uint8 nZone;
        char cNorth;
        sscanf(pszRefSystem, rstUTM, &nZone, &cNorth);
        oSRS.SetProjCS(&pszRefSystem[1]);
        oSRS.SetWellKnownGeogCS("WGS84");
        oSRS.SetUTM(nZone, (cNorth == 'n'));
        oSRS.exportToWkt(&pszProjection);
    }
    else
    {
        const char *pszRefFilename;
        char **papszIdrisiRef;

        papszIdrisiRef = NULL;
        pszRefFilename = CPLFormFilename(CPLGetPath(pszFilename), pszRefSystem, extREF);

        if (FileExists(pszRefFilename))
        {
            papszIdrisiRef = CSLLoad(pszRefFilename);
        }
        else
        {
            CPLDebug(extRST, "file does not exist = %s", pszRefFilename);
            pszRefFilename = CPLSPrintf("%s\\Georef\\%s.%s", pszIdrisiPath, pszRefSystem, extREF); 
            if (FileExists(pszRefFilename))
            {
                papszIdrisiRef = CSLLoad(pszRefFilename);
            }
            else
            {
                CPLDebug(extRST, "file does not exist = %s", pszRefFilename);
            }
        }
        if (CSLCount(papszIdrisiRef) > 0)
        {
            OSRImportFromIdrisi(&oSRS, papszIdrisiRef);
            oSRS.exportToWkt(&pszProjection);
        }
        else
        {
            pszProjection = CPLStrdup("");
        }
        CSLDestroy(papszIdrisiRef); 
        CPLFree(papszIdrisiRef);
    }

    CPLFree(pszRefSystem);

    return pszProjection;
}

CPLErr IdrisiDataset::SetProjection(const char *pszProjString)
{
    CPLDebug(extRST, "SetProjection");

    char **papszIdrisiRef = NULL;

    papszIdrisiRef = OSRExportToIdrisi(pszProjString, papszIdrisiRef);

    if (CSLCount(papszIdrisiRef) == 0)
    {
        CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", rstPLANE));
        CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", rstMETER));
    }
    else if (EQUAL(CSLFetchNameValue(papszIdrisiRef, refREF_SYSTEM), rstLATLONG))
    {
        CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", rstLATLONG));
        CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", rstDEGREE));
    }
    else if (EQUALN(CSLFetchNameValue(papszIdrisiRef, refREF_SYSTEM), rstUTM, 3))
    {
        CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", 
            CSLFetchNameValue(papszIdrisiRef, refREF_SYSTEM))); 
        CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", 
            CSLFetchNameValue(papszIdrisiRef, refUNITS)));
    }
    else
    {
        const char *pszBaseFilename;
        const char *pszRefFilename;

        pszBaseFilename = CPLGetBasename(pszFilename);
        pszRefFilename = CPLResetExtension(pszFilename, extREF);

        CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", pszBaseFilename));
        CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", 
            CSLFetchNameValue(papszIdrisiRef, refUNITS)));

        CPLDebug(extRST, "(%s)", pszRefFilename);
        CSLSetNameValueSeparator(papszIdrisiRef, ": ");
        CSLSave(papszIdrisiRef, pszRefFilename);
    }
    CSLDestroy(papszIdrisiRef);

    return CE_None;
}

//  ----------------------------------------------------------------------------
//
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

    // -------------------------------------------------------------------- 
    //      Set Dimension
    // -------------------------------------------------------------------- 

    nBlockYSize = 1;
    nBlockXSize = poDS->GetRasterXSize();

    // -------------------------------------------------------------------- 
    //      Get ready for reading and writing
    // -------------------------------------------------------------------- 

    nRecordSize = poDS->GetRasterXSize() * GDALGetDataTypeSize(eDataType) / 8 * poDS->nBands;
    pabyScanLine = (GByte *) CPLMalloc(nRecordSize);
}

IdrisiRasterBand::~IdrisiRasterBand()
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CPLFree(pabyScanLine);
}

CPLErr IdrisiRasterBand::IReadBlock(int nBlockXOff, 
                                    int nBlockYOff,
                                    void *pImage)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET) < 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, 
            "Can't seek (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror(errno));
        return CE_Failure;
    }

    if (VSIFReadL(pabyScanLine, 1, nRecordSize, poGDS->fp) < nRecordSize)
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
            ((uint8 *) pImage)[i] = pabyScanLine[j];
        }
    }
    else
    {
        memcpy(pImage, pabyScanLine, nRecordSize);
    }

    return CE_None;
}

CPLErr IdrisiRasterBand::IWriteBlock(int nBlockXOff, 
                                     int nBlockYOff,
                                     void *pImage)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (poGDS->nBands == 1)
    {
        memcpy(pabyScanLine, pImage, nRecordSize);
    }
    else
    {
        if (nBand > 1) 
        {
            VSIFReadL(pabyScanLine, 1, nRecordSize, poGDS->fp);
            VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);
        }
        int i, j;
        for (i = 0, j = (3 - nBand); i < nBlockXSize; i++, j += 3)
        {
            pabyScanLine[j] = ((uint8 *) pImage)[i];
        }
    }

    VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);

    if (VSIFWriteL(pabyScanLine, 1, nRecordSize, poGDS->fp) < nRecordSize)
    {
        CPLError(CE_Failure, CPLE_FileIO, 
            "Can't write (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror(errno));
        return CE_Failure;
    }

    return CE_None;
}

double IdrisiRasterBand::GetMinimum(int *pbSuccess)
{      CPLDebug(extRST, "GetMinimum");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;
    double adfMinValue[3];
    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMIN_VALUE), "%lf %lf %lf", 
        &adfMinValue[0], &adfMinValue[1], &adfMinValue[2]);

    CPLDebug(extRST, "GetMinimum of Band (%d) = %lf", nBand, adfMinValue[nBand - 1]);

    return adfMinValue[this->nBand - 1];
}

double IdrisiRasterBand::GetMaximum(int *pbSuccess)
{      CPLDebug(extRST, "GetMaximum");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;
    double adfMaxValue[3];
    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMAX_VALUE), "%lf %lf %lf", 
        &adfMaxValue[0], &adfMaxValue[1], &adfMaxValue[2]);

    CPLDebug(extRST, "GetMaximum of Band (%d) = %lf", nBand, adfMaxValue[nBand - 1]);

    return adfMaxValue[this->nBand - 1];
}

CPLErr IdrisiRasterBand::GetStatistics(int bApproxOK, int bForce,
                                       double *pdfMin, double *pdfMax, 
                                       double *pdfMean, double *padfStdDev)
{      CPLDebug(extRST, "GetStatistics");

    *pdfMin = GetMinimum();
    *pdfMax = GetMaximum();
    pdfMean = NULL;
    padfStdDev = NULL;

    return CE_Warning;
}

double IdrisiRasterBand::GetNoDataValue(int *pbSuccess)
{      CPLDebug(extRST, "GetNoDataValue");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    double dfNoData;
    char *pszFlagDefn;

    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_VALUE), " %lf", &dfNoData);
    pszFlagDefn = TrimL(CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN));

    if (pbSuccess)
    {
        *pbSuccess = (! EQUAL(pszFlagDefn, "none"));
    }

    CPLFree(pszFlagDefn);

    return dfNoData;
}

GDALColorInterp IdrisiRasterBand::GetColorInterpretation()
{               CPLDebug(extRST, "GetColorInterpretation");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (poGDS->nBands == 3)
    {
        CPLDebug(extRST, "RGB = %d", nBand);
        switch (nBand)
        {
            case 1: return GCI_BlueBand;
            case 2: return GCI_GreenBand;
            case 3: return GCI_RedBand;
        }
    }
    else 
    {
        if (poGDS->poColorTable->GetColorEntryCount() > 0)
        {
            CPLDebug(extRST, "Palette");
            return GCI_PaletteIndex;
        }
        else
        {
            CPLDebug(extRST, "Greeyscale");
            return GCI_GrayIndex;
        }
    }
}

char **IdrisiRasterBand::GetCategoryNames()
{      CPLDebug(extRST, "GetCategoryNames");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CPLDebug(extRST, "Categories Counter = %d", CSLCount(poGDS->papszCategoryNames));
    return poGDS->papszCategoryNames;
}

GDALColorTable *IdrisiRasterBand::GetColorTable()
{               CPLDebug(extRST, "GetColorTable");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    return poGDS->poColorTable;
}

CPLErr IdrisiRasterBand::SetCategoryNames(char **papszCategoryNames)
{      CPLDebug(extRST, "SetCategoryNames Count = %d", CSLCount(papszCategoryNames));

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    int nCount = CSLCount(papszCategoryNames);

    if (nCount == 0)
        return CE_None;

    poGDS->papszRDC = CSLSetNameValue(poGDS->papszRDC, 
        rdcLEGEND_CATS, CPLSPrintf(" %d", nCount));

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
{      CPLDebug(extRST, "SetNoDataValue = %lf", dfNoDataValue);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLSetNameValue(poGDS->papszRDC, rdcFLAG_VALUE,    CPLSPrintf(" %.8g", dfNoDataValue));
    CSLSetNameValue(poGDS->papszRDC, rdcFLAG_DEFN,     " Missing Data");

    return CE_None;
}

CPLErr IdrisiRasterBand::SetColorInterpretation(GDALColorInterp eNewInterp)
{      CPLDebug(extRST, "SetColorInterpretation %d", eNewInterp);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    poGDS->eColorInterp = eNewInterp;

    return CE_None;
}

CPLErr IdrisiRasterBand::SetColorTable(GDALColorTable *poColorTable)
{      CPLDebug(extRST, "SetColorTable Count = %d", poColorTable->GetColorEntryCount());

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if ((poGDS->eColorInterp != GCI_PaletteIndex) || (poColorTable->GetColorEntryCount() == 0))
        return CE_None;

    const char *pszSMPFilename;
    pszSMPFilename = CPLResetExtension(poGDS->pszFilename, extSMP);
    FILE *fpSMP;

    if ((fpSMP = VSIFOpenL(pszSMPFilename, "w")) != NULL )
    {
        VSIFWriteL("[Idrisi]", 8, 1, fpSMP);
        uint8 niBuf1 = 1;
        VSIFWriteL(&niBuf1, 1, 1, fpSMP);
        niBuf1 = 10;
        VSIFWriteL(&niBuf1, 1, 1, fpSMP);
        niBuf1 = 8;
        VSIFWriteL(&niBuf1, 1, 1, fpSMP);
        niBuf1 = 18;
        VSIFWriteL(&niBuf1, 1, 1, fpSMP);
        niBuf1 = 255;
        VSIFWriteL(&niBuf1, 1, 1, fpSMP);
        uint16 niBuf2 = GetMinimum();
        VSIFWriteL(&niBuf2, 2, 1, fpSMP);
        niBuf2 = GetMaximum();
        VSIFWriteL(&niBuf2, 2, 1, fpSMP);

        GDALColorEntry oEntry;
        uint8 aucRGB[3];

        for (int i = 0; i < poColorTable->GetColorEntryCount(); i++)
        {
            poColorTable->GetColorEntryAsRGB( i, &oEntry );
            aucRGB[0]= (short) oEntry.c1;
            aucRGB[1]= (short) oEntry.c2;
            aucRGB[2]= (short) oEntry.c3;
            VSIFWriteL(&aucRGB, 3, 1, fpSMP);
        }
        for (int i = poColorTable->GetColorEntryCount(); i < 255; i++)
        {
            poColorTable->GetColorEntryAsRGB( i, &oEntry );
            aucRGB[0]= (short) 0;
            aucRGB[1]= (short) 0;
            aucRGB[2]= (short) 0;
            VSIFWriteL(&aucRGB, 3, 1, fpSMP);
        }

        VSIFCloseL(fpSMP);
    }

    return CE_None;
}

CPLErr IdrisiRasterBand::SetScale(double dfScale)
{      CPLDebug(extRST, "SetScale = %lf", dfScale);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLAddNameValue(poGDS->papszRDC, rdcRESOLUTION,    CPLSPrintf(" %.8g", dfScale));

    return CE_None;
}

CPLErr IdrisiRasterBand::SetUnitType(const char *pszUnitType)
{      CPLDebug(extRST, "SetUnitType = %s", pszUnitType);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLAddNameValue(poGDS->papszRDC, rdcREF_UNITS, pszUnitType);

    return CE_None;
}

CPLErr IdrisiRasterBand::SetStatistics(double dfMin, double dfMax, double dfMean, double dfStdDev)
{      CPLDebug(extRST, "SetStatistics dfMin=%f dfMax=%f", dfMin, dfMax);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CSLSetNameValue(poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf(" %.8g", dfMin));
    CSLSetNameValue(poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf(" %.8g", dfMax));
    CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf(" %.8g", dfMin));
    CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf(" %.8g", dfMax));

    return CE_None;
}

OGRErr OSRImportFromIdrisi(OGRSpatialReferenceH hSRS, char **papszGeoRef)
{
    char *pszProj;
    char *pszRefSystem;
    double dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing;
    double dfStdP1, dfStdP2;

    pszRefSystem = TrimL(CSLFetchNameValue(papszGeoRef, refREF_SYSTEM));
    pszProj = TrimL(CSLFetchNameValue(papszGeoRef, refPROJECTION));

    CPLDebug(extRST, "\npszRefSystem=%s,\n pszProj=%s", pszRefSystem, pszProj);

    sscanf(CSLFetchNameValue(papszGeoRef, refORIGIN_LAT),   "%lf", &dfCenterLat);
    sscanf(CSLFetchNameValue(papszGeoRef, refORIGIN_LONG),  "%lf", &dfCenterLong);
    sscanf(CSLFetchNameValue(papszGeoRef, refORIGIN_X),     "%lf", &dfFalseEasting);
    sscanf(CSLFetchNameValue(papszGeoRef, refORIGIN_Y),     "%lf", &dfFalseNorthing);
    sscanf(CSLFetchNameValue(papszGeoRef, refSTANDL_1),     "%lf", &dfStdP1);
    sscanf(CSLFetchNameValue(papszGeoRef, refSTANDL_2),     "%lf", &dfStdP2);

    if (EQUAL(CSLFetchNameValue(papszGeoRef, refSCALE_FAC), " na"))
    {
        dfScale = 1.0;
    }
    else
    {
        sscanf(CSLFetchNameValue(papszGeoRef, refSCALE_FAC),   "%lf", &dfScale);
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

char **OSRExportToIdrisi(const char *pszProjString, char **papszGeoRef)
{
    OGRSpatialReference oSRS;

    if (oSRS.importFromWkt((char **) &pszProjString) != OGRERR_NONE )
        return OGRERR_NONE;

    CPLDebug(extRST, "(%s)", pszProjString);

    double padfCoef[3];

    papszGeoRef = CSLAddNameValue(papszGeoRef, refREF_SYSTEM, oSRS.GetAttrValue("GEOGCS", 0));
    papszGeoRef = CSLAddNameValue(papszGeoRef, refPROJECTION, oSRS.GetAttrValue("PROJCS", 0));
    papszGeoRef = CSLAddNameValue(papszGeoRef, refDATUM,      oSRS.GetAttrValue("DATUM",  0));
/*
    papszGeoRef = CSLAddNameValue(papszGeoRef, refDELTA_WGS84, 
        CPLSPrintf("%.2g %.2g %.2g", 
        oSRS.GetAttrValue("TOWGS84", 0),
        oSRS.GetAttrValue("TOWGS84", 1),
        oSRS.GetAttrValue("TOWGS84", 2)));
*/
    oSRS.GetTOWGS84(padfCoef, 3);
    papszGeoRef = CSLAddNameValue(papszGeoRef, refDELTA_WGS84, 
        CPLSPrintf("%.2g %.2g %.2g", padfCoef[0], padfCoef[1], padfCoef[2]));

    papszGeoRef = CSLAddNameValue(papszGeoRef, refELLIPSOID,  oSRS.GetAttrValue("SPHEROID", 0));
    papszGeoRef = CSLAddNameValue(papszGeoRef, refMAJOR_SAX,
        CPLSPrintf("%.8g", oSRS.GetSemiMajor(NULL)));
    papszGeoRef = CSLAddNameValue(papszGeoRef, refMINOR_SAX,
        CPLSPrintf("%.8g", oSRS.GetSemiMinor(NULL)));

    papszGeoRef = CSLAddNameValue(papszGeoRef, refORIGIN_LONG, 
        CPLSPrintf("%.8g", oSRS.GetProjParm("latitude_of_origin", 0.0, NULL)));
    papszGeoRef = CSLAddNameValue(papszGeoRef, refORIGIN_LAT, 
        CPLSPrintf("%.8g", oSRS.GetProjParm("central_meridian", 0.0, NULL)));

    papszGeoRef = CSLAddNameValue(papszGeoRef, refORIGIN_X, 
        CPLSPrintf("%.8g", oSRS.GetProjParm("false_easting", 0.0, NULL)));
    papszGeoRef = CSLAddNameValue(papszGeoRef, refORIGIN_Y, 
        CPLSPrintf("%.8g", oSRS.GetProjParm("false_northing", 0.0, NULL)));

    papszGeoRef = CSLAddNameValue(papszGeoRef, refSCALE_FAC, 
        CPLSPrintf("%.8g", oSRS.GetProjParm("scale_factor", 0.0, NULL)));
    papszGeoRef = CSLAddNameValue(papszGeoRef, refUNITS, 
        CPLSPrintf("%.8g", oSRS.GetAttrValue("UNIT", 1)));

    double dfStdP1 = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0, NULL);
    double dfStdP2 = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0, NULL);

    if ((dfStdP1 == 0.0) && (dfStdP2 == 0.0))
    {
        papszGeoRef = CSLAddNameValue(papszGeoRef, refPARAMETERS, "0.0");
    }
    else
    {
        papszGeoRef = CSLAddNameValue(papszGeoRef, refPARAMETERS, "2");
        papszGeoRef = CSLAddNameValue(papszGeoRef, refSTANDL_1,   CPLSPrintf("%.8g", dfStdP1));
        papszGeoRef = CSLAddNameValue(papszGeoRef, refSTANDL_2,   CPLSPrintf("%.8g", dfStdP1));
    }

    return papszGeoRef;
}

char *TrimL(const char *pszText)
{
    for (int i = 0; (pszText[i] == ' ') && (pszText[i] != '\0'); i++);

    char *pszResult = CPLStrdup(&pszText[i]);

    return pszResult;
}

bool FileExists(const char *pszFilename)
{
    FILE *fp;

    int exist = ((fp = VSIFOpenL(pszFilename, "rb")) != NULL);

    if (exist)
    {
        VSIFCloseL(fp);
    }

    return exist;
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
