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

//----- file extensions:
#define extRST          "rst"
#define extRDC          "rdc"
#define extSMP          "smp"

//----- ".rst" contains the data file in binary format;
//   one band data types uint8, int16, float32 or
//   one rgb band interlaced by pixel of Blue, Green, Red in 3 x uint8

//----- ".rdc" documentation file, metadata, text format;
//   11 position for field name completed with spaces;
//   " : " separator;
//   field value.

//----- ".smp" palette, color table, binary format;
//    18 bytes of header folowed by 255 occurences of
//    Blue, Green, Red in 3 x uint8.

//----- field names on rdc file:
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
#define rdcFLAG_DEFN2	"flag def`n  "
#define rdcLEGEND_CATS	"legend cats "
#define rdcLINEAGES		"lineage     "
#define rdcCOMMENTS		"comment     "
#define rdcCODE_N		"code %6d "

//----- standard values:
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

//----- palette file (.smp) header size:
#define smpHEADERSIZE	18

//----- remove left (' ')`s:
char *TrimL(const char *pszText);

//----- check if file exists:
bool FileExists(const char *pszFilename);

//----- classes pre-definition:
class IdrisiDataset;
class IdrisiRasterBand;

//  ----------------------------------------------------------------------------
//	    Idrisi GDALDataset
//  ----------------------------------------------------------------------------

class IdrisiDataset : public GDALDataset
{
    friend class IdrisiRasterBand;

private:
    FILE *fp;

    char *pszFilename;
    char *pszDocFilename;
    char **papszRDC;
    double adfGeoTransform[6];

    char *pszProjection;
    char **papszCategoryNames;

protected:
    GDALColorTable *poColorTable;

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

//  ----------------------------------------------------------------------------
//	    Idrisi GDALRasterBand
//  ----------------------------------------------------------------------------

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

    virtual CPLErr SetCategoryNames(char **papszCategoryNames);
    virtual CPLErr SetNoDataValue(double dfNoDataValue);
    virtual CPLErr SetColorTable(GDALColorTable *poColorTable); 
    virtual CPLErr SetUnitType(const char *pszUnitType);
    virtual CPLErr SetStatistics(double dfMin, double dfMax, 
        double dfMean, double dfStdDev);
};

//  ------------------------------------------------------------------------  //
//	                    Implementation of IdrisiDataset                       //
//  ------------------------------------------------------------------------  //

/************************************************************************/
/*                           IdrisiDataset()                            */
/************************************************************************/
IdrisiDataset::IdrisiDataset()
{
    pszFilename = NULL;
    fp = NULL;
    papszRDC = NULL;
    pszDocFilename = NULL;
    pszProjection = NULL;
    poColorTable = new GDALColorTable();
    papszCategoryNames = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~IdrisiDataset()                           */
/************************************************************************/
IdrisiDataset::~IdrisiDataset()
{
    FlushCache();

    if (papszRDC != NULL)
    {
        if (eAccess == GA_Update)
            CSLSave(papszRDC, pszDocFilename);

        CSLDestroy(papszRDC);
    }

    if( poColorTable )
        delete poColorTable;

    CPLFree(pszFilename);
    CPLFree(pszDocFilename);
    CPLFree(pszProjection);
    CSLDestroy( papszCategoryNames );
    
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *IdrisiDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if ((poOpenInfo->fp == NULL) || 
        (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), extRST) == FALSE))
        return NULL;

    // -------------------------------------------------------------------- 
    //      Check the documentation file .rdc
    // -------------------------------------------------------------------- 

    const char *pszDocFilename;
    char **papszRDC;

    pszDocFilename = CPLResetExtension(poOpenInfo->pszFilename, extRDC);
    papszRDC = CSLLoad(pszDocFilename);

    //CPLDebug(extRST, "Open: Check the documentation file %s", pszDocFilename);

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

    //CPLDebug(extRST, "Open: Create a corresponding GDALDataset, Access = %d", poOpenInfo->eAccess);

    IdrisiDataset *poDS;

    poDS = new IdrisiDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);

    if (poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fp = VSIFOpenL(poDS->pszFilename, "rb");
    } 
    else 
    {
        poDS->fp = VSIFOpenL(poDS->pszFilename, "r+b");
    }

    if (poDS->fp == NULL)
        return NULL;

    poDS->pszDocFilename = CPLStrdup(pszDocFilename);
    poDS->papszRDC = papszRDC;

    // -------------------------------------------------------------------- 
    //      Load information from rdc
    // -------------------------------------------------------------------- 

    sscanf(CSLFetchNameValue(papszRDC, rdcCOLUMNS), "%d", &poDS->nRasterXSize);
    sscanf(CSLFetchNameValue(papszRDC, rdcROWS),    "%d", &poDS->nRasterYSize);

    //CPLDebug(extRST, "Open: Load information from rdc (%d,%d)", poDS->nRasterXSize, poDS->nRasterYSize);

    // -------------------------------------------------------------------- 
    //      Create band information
    // -------------------------------------------------------------------- 

    char *pszDataType = TrimL(CSLFetchNameValue(papszRDC, rdcDATA_TYPE));

    //CPLDebug(extRST, "Open: Create band information data type = %s", pszDataType);

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
    //      Load the transformation matrix
    //
    //	Newly "Created" files may not have values. 
    // -------------------------------------------------------------------- 
    char *pszMinX = TrimL(CSLFetchNameValue(papszRDC, rdcMIN_X));
    if( !EQUAL(pszMinX,"") )
    {
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
    }
    CPLFree( pszMinX );

    //CPLDebug(extRST, "Open: Load the transformation matrix %.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g", dfMinX, dfMaxX, dfMinY, dfMaxY, dfUnit, dfXPixSz, dfYPixSz);

    // -------------------------------------------------------------------- 
    //      Set Color Table in the presence of a smp file
    // -------------------------------------------------------------------- 
    if (poDS->nBands != 3)
    {
        const char *pszSMPFilename;
        pszSMPFilename = CPLResetExtension(poDS->pszFilename, extSMP);

        FILE *fpSMP;
        if ((fpSMP = VSIFOpenL(pszSMPFilename, "rb")) != NULL )
        {
            //CPLDebug(extRST, "Open: Load Color Table from = %s", pszSMPFilename);

            double dfMaxValue;
            sscanf(CSLFetchNameValue(papszRDC, rdcMAX_VALUE),    " %lf", &dfMaxValue);

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
    //      Check for category names.
    // -------------------------------------------------------------------- 
    int nCatCount, i;
    sscanf(CSLFetchNameValue(poDS->papszRDC, rdcLEGEND_CATS),  " %d", &nCatCount);

    for (i = 0; i < nCatCount; i++)
    {
        char *pszLabel = (char*) CSLFetchNameValue(poDS->papszRDC, 
                                                   CPLSPrintf(rdcCODE_N, i));

        if (pszLabel == NULL)
            poDS->papszCategoryNames = CSLAddString(poDS->papszCategoryNames, "");
        else
        {
            if( *pszLabel == ' ' )
                pszLabel++;

            poDS->papszCategoryNames = CSLAddString(poDS->papszCategoryNames, 
                                                    pszLabel);
        }
    }

    return (poDS);
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

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

    //CPLDebug(extRST, "create: Create the header file");

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
      default:
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create IDRISI dataset with an illegal\n"
                 "data type (%s).\n",
                 GDALGetDataTypeName(eType));
        return NULL;

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
    papszRDC = CSLAddNameValue(papszRDC, rdcLEGEND_CATS,   "0");
    papszRDC = CSLAddNameValue(papszRDC, rdcLINEAGES,      " ");
    papszRDC = CSLAddNameValue(papszRDC, rdcCOMMENTS,      " ");

    const char *pszDocFilename;
    pszDocFilename = CPLResetExtension(pszFilename, extRDC);

    CSLSetNameValueSeparator(papszRDC, ": ");
    CSLSave(papszRDC, pszDocFilename);
    CSLDestroy(papszRDC);
    CPLFree( pszDataType );

    // ---------------------------------------------------------------- 
    //  Create an empty data file
    // ---------------------------------------------------------------- 

    //CPLDebug(extRST, "create: Create an empty data file");

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

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

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
        //CPLDebug(extRST, "createcopy: Keep the same data type %s", GDALGetDataTypeName(eType));
    }
    else
    {
        if (eType == GDT_Float64)
        {
            eType = GDT_Float32;
            //CPLDebug(extRST, "createcopy: Change (1) from %s to %s", GDALGetDataTypeName(poBand->GetRasterDataType()), GDALGetDataTypeName(eType));
        }
        else
        {
            if ((dfMin < (double) SHRT_MIN) || 
                (dfMax > (double) SHRT_MAX))
            {
                eType = GDT_Float32; 
                //CPLDebug(extRST, "createcopy: Change (2) from %s to %s", GDALGetDataTypeName(poBand->GetRasterDataType()), GDALGetDataTypeName(eType));
            }
            else
            {
                eType = GDT_Int16;
                //CPLDebug(extRST, "createcopy: Change (3) from %s to %s", GDALGetDataTypeName(poBand->GetRasterDataType()), GDALGetDataTypeName(eType));
            }
        }
    }

    // --------------------------------------------------------------------
    //      Create the dataset
    // --------------------------------------------------------------------
    //CPLDebug(extRST, "createcopy: Create the dataset");

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

    //CPLDebug(extRST, "createcopy: Copy information to the dataset");

    double adfGeoTransform[6];

    poDS->SetProjection(poSrcDS->GetProjectionRef());
    poSrcDS->GetGeoTransform(adfGeoTransform);
    poDS->SetGeoTransform(adfGeoTransform);

    // --------------------------------------------------------------------
    //      Copy information to the raster band
    // --------------------------------------------------------------------

    //CPLDebug(extRST, "createcopy: Copy information to (%d) raster band(s)", poDS->nBands);

    GDALRasterBand *poSrcBand;

    for (int i = 1; i <= poDS->nBands; i++)
    {
        poSrcBand = poSrcDS->GetRasterBand(i);
        poBand = poDS->GetRasterBand(i);

        //CPLDebug(extRST, "createcopy: Set values on Band %d", i);

        if (i == 1)
        {
            poBand->SetCategoryNames(poSrcBand->GetCategoryNames());
            poBand->SetUnitType(poSrcBand->GetUnitType());
            poBand->SetColorTable(poSrcBand->GetColorTable());
        }
        poSrcBand->GetStatistics(false, true, &dfMin, &dfMax, NULL, NULL);
        poBand->SetStatistics(dfMin, dfMax, 0.0, 0.0);
        poBand->SetNoDataValue(poSrcBand->GetNoDataValue(NULL));
    }

    // --------------------------------------------------------------------
    //      Copy image data
    // --------------------------------------------------------------------

    //CPLDebug(extRST, "createcopy: Copy image data"); 

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
    //      Finalize
    // --------------------------------------------------------------------

    poDS->FlushCache();

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr  IdrisiDataset::GetGeoTransform(double * padfTransform)
{    
    //CPLDebug(extRST, "GetGeoTransform");

    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr  IdrisiDataset::SetGeoTransform(double * padfGeoTransform)
{    
    //CPLDebug(extRST, "SetGeoTransform");

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

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *IdrisiDataset::GetProjectionRef(void)
{        
    //CPLDebug(extRST, "GetProjectionRef");

    // Clear last value.
    CPLFree( pszProjection );
    pszProjection = NULL;

    char *pszRefSystem;

    pszRefSystem = TrimL(CSLFetchNameValue(papszRDC, rdcREF_SYSTEM));

    OGRSpatialReference oSRS;

    if (EQUAL(pszRefSystem, rstPLANE))
    {
        pszProjection = CPLStrdup("");

        //CPLDebug(extRST, "No Reference System available (%s)", pszRefSystem);
    }
    else if (EQUAL(pszRefSystem, rstLATLONG))
    {
        oSRS.SetWellKnownGeogCS("WGS84");
        oSRS.exportToWkt(&pszProjection);

        //CPLDebug(extRST, "Reference System = %s", pszRefSystem);
    }
    else if (EQUALN(pszRefSystem, rstUTM, 3))
    {
        int	nZone;
        char 	cNorth;

        sscanf(pszRefSystem, rstUTM, &nZone, &cNorth);
        oSRS.SetProjCS(pszRefSystem);
        oSRS.SetWellKnownGeogCS("WGS84");
        oSRS.SetUTM(nZone, (cNorth == 'n'));
        oSRS.exportToWkt(&pszProjection);

        //CPLDebug(extRST, "UTM Reference System = %s, Zone = %d, %c", pszRefSystem, nZone, cNorth);
    }
    else
    {
        char* pszUnits = TrimL(CSLFetchNameValue(papszRDC, rdcREF_UNITS));
        if (EQUAL(pszUnits, "deg"))
        {
            oSRS.SetWellKnownGeogCS("WGS84");
            oSRS.exportToWkt(&pszProjection);
        }

        CPLFree( pszUnits );
    }

    CPLFree(pszRefSystem);

    if( pszProjection == NULL )
        pszProjection = CPLStrdup(pszProjection);

    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr IdrisiDataset::SetProjection(const char *pszProjString)
{   
    //CPLDebug(extRST, "SetProjection");

    OGRSpatialReference oSRS;

    oSRS.importFromWkt((char **) &pszProjString);

    if (strstr("GEOGCS", pszProjString) == NULL)
    {
        CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", rstPLANE));
        CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", rstMETER));

        //CPLDebug(extRST, "No Reference, assuming = plane");
    }
    else if (oSRS.IsProjected() == FALSE)
    {
        CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", rstLATLONG));
        CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", rstDEGREE));

        //CPLDebug(extRST, "No Projection, assuming = latlong");
    }
    else
    {
        const char * pszProjName = NULL;
        pszProjName = oSRS.GetAttrValue("PROJECTION");
		if (oSRS.GetUTMZone() == 0)
        {
            CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", rstLATLONG));
            CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", rstDEGREE));

            //CPLDebug(extRST, "Reference System not supported, assuming latlong");
        }
        else
        {
            const char *pszRefSystem;
            pszRefSystem = CPLSPrintf(rstUTM, oSRS.GetUTMZone(), 
                (oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING) == 0 ? 'n' : 's'));
            CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf(" %s", pszRefSystem));
            CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf(" %s", rstMETER));
            //CPLDebug(extRST, "Reference System = %s", pszRefSystem);
        }
    }

    return CE_None;
}

//  ------------------------------------------------------------------------  //
//	                    Implementation of IdrisiRasterBand                    //
//  ------------------------------------------------------------------------  //

/************************************************************************/
/*                          IdrisiRasterBand()                          */
/************************************************************************/

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

/************************************************************************/
/*                         ~IdrisiRasterBand()                          */
/************************************************************************/

IdrisiRasterBand::~IdrisiRasterBand()
{
    CPLFree(pabyScanLine);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

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

    if( (int) VSIFReadL(pabyScanLine, 1, nRecordSize, poGDS->fp) < nRecordSize)
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

#ifdef CPL_MSB    
    if( eDataType == GDT_Float32 )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
#endif

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr IdrisiRasterBand::IWriteBlock(int nBlockXOff, 
                                     int nBlockYOff,
                                     void *pImage)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

#ifdef CPL_MSB    
    // Swap in input buffer if needed.
    if( eDataType == GDT_Float32 )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
#endif

    if (poGDS->nBands == 1)
    {
        memcpy(pabyScanLine, pImage, nRecordSize);
    }
    else
    {
        if (nBand > 1) 
        {
            VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);
            VSIFReadL(pabyScanLine, 1, nRecordSize, poGDS->fp);
        }
        int i, j;
        for (i = 0, j = (3 - nBand); i < nBlockXSize; i++, j += 3)
        {
            pabyScanLine[j] = ((uint8 *) pImage)[i];
        }
    }

#ifdef CPL_MSB    
    // Swap input buffer back to original form.
    if( eDataType == GDT_Float32 )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
#endif

    VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);

    if( (int) VSIFWriteL(pabyScanLine, 1, nRecordSize, poGDS->fp) < nRecordSize)
    {
        CPLError(CE_Failure, CPLE_FileIO, 
            "Can't write (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror(errno));
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double IdrisiRasterBand::GetMinimum(int *pbSuccess)
{      
    //CPLDebug(extRST, "GetMinimum");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    double adfMinValue[3];
    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMIN_VALUE), "%lf %lf %lf", 
        &adfMinValue[0], &adfMinValue[1], &adfMinValue[2]);

    //CPLDebug(extRST, "GetMinimum of Band (%d) = %lf", nBand, adfMinValue[nBand - 1]);

    if( pbSuccess )
        *pbSuccess = TRUE;

    return adfMinValue[this->nBand - 1];
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double IdrisiRasterBand::GetMaximum(int *pbSuccess)
{      
    //CPLDebug(extRST, "GetMaximum");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    double adfMaxValue[3];
    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMAX_VALUE), "%lf %lf %lf", 
        &adfMaxValue[0], &adfMaxValue[1], &adfMaxValue[2]);

    //CPLDebug(extRST, "GetMaximum of Band (%d) = %lf", nBand, adfMaxValue[nBand - 1]);

    if( pbSuccess )
        *pbSuccess = TRUE;

    return adfMaxValue[this->nBand - 1];
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::GetStatistics(int bApproxOK, int bForce,
                                       double *pdfMin, double *pdfMax, 
                                       double *pdfMean, double *padfStdDev)
{      
    //CPLDebug(extRST, "GetStatistics");

    *pdfMin = GetMinimum();
    *pdfMax = GetMaximum();
    pdfMean = NULL;
    padfStdDev = NULL;

    return CE_Warning;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double IdrisiRasterBand::GetNoDataValue(int *pbSuccess)
{      
    //CPLDebug(extRST, "GetNoDataValue");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    double dfNoData;
    char *pszFlagDefn;

    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_VALUE), " %lf", &dfNoData);
    if( CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN) != NULL )
        pszFlagDefn = TrimL(CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN));
    else if( CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN2) != NULL )
        pszFlagDefn = TrimL(CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN2));
    else
        pszFlagDefn = CPLStrdup("");

    if (pbSuccess)
    {
        *pbSuccess = (! EQUAL(pszFlagDefn, "none"));
    }

    CPLFree(pszFlagDefn);

    return dfNoData;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp IdrisiRasterBand::GetColorInterpretation()
{               
    //CPLDebug(extRST, "GetColorInterpretation");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (poGDS->nBands == 3)
    {
        //CPLDebug(extRST, "RGB = %d", nBand);
        switch (nBand)
        {
        case 1: return GCI_BlueBand;
        case 2: return GCI_GreenBand;
        case 3: return GCI_RedBand;
        }
    }
    else if (poGDS->poColorTable->GetColorEntryCount() > 0)
    {
        //CPLDebug(extRST, "Palette");
        return GCI_PaletteIndex;
    }

    //CPLDebug(extRST, "GrayIndex");
    return GCI_GrayIndex;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **IdrisiRasterBand::GetCategoryNames()
{      
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    return poGDS->papszCategoryNames;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *IdrisiRasterBand::GetColorTable()
{               
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    //CPLDebug(extRST, "GetColorTable, Count = %d", poGDS->poColorTable->GetColorEntryCount());

    if (poGDS->poColorTable->GetColorEntryCount() == 0)
    {
        return NULL;
    }
    else
    {
        return poGDS->poColorTable;
    }
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr IdrisiRasterBand::SetCategoryNames(char **papszCategoryNames)
{      
    //CPLDebug(extRST, "SetCategoryNames Count = %d", CSLCount(papszCategoryNames));

    int nCount = CSLCount(papszCategoryNames);

    if (nCount == 0)
        return CE_None;

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    poGDS->papszRDC = CSLSetNameValue(poGDS->papszRDC, 
        rdcLEGEND_CATS, CPLSPrintf(" %d", nCount));

    int index = CSLFindString(poGDS->papszRDC, 
        CPLSPrintf("%s: %d", rdcLEGEND_CATS,nCount));

    for (int i = 0; i < nCount; i++)
    {
        CPLString osNameValue;

        osNameValue.Printf( rdcCODE_N, i );
        osNameValue += ": ";
        osNameValue += papszCategoryNames[i];

        poGDS->papszRDC = 
            CSLInsertString(poGDS->papszRDC, (index + i + 1), osNameValue );
    }

    CSLDestroy( poGDS->papszCategoryNames );
    poGDS->papszCategoryNames = CSLDuplicate(papszCategoryNames);

    return CE_None;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr IdrisiRasterBand::SetNoDataValue(double dfNoDataValue)
{      
    //CPLDebug(extRST, "SetNoDataValue = %lf", dfNoDataValue);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (dfNoDataValue == -9999.0)
    {
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcFLAG_VALUE, " none");
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcFLAG_DEFN,  " none");
    }
    else
    {
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcFLAG_VALUE, CPLSPrintf(" %.8g", dfNoDataValue));
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcFLAG_DEFN,  " missing data");
    }

    return CE_None;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::SetColorTable(GDALColorTable *poColorTable)
{      
    //CPLDebug(extRST, "SetColorTable");

    if (poColorTable == NULL)
    {
        return CE_None;
    }

    if (poColorTable->GetColorEntryCount() == 0)
    {
        return CE_None;
    }

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    const char *pszSMPFilename;
    pszSMPFilename = CPLResetExtension(poGDS->pszFilename, extSMP);
    FILE *fpSMP;

    if ((fpSMP = VSIFOpenL(pszSMPFilename, "w")) != NULL )
    {
        VSIFWriteL("[Idrisi]", 8, 1, fpSMP);
        uint8 nPlatform = 1;    VSIFWriteL(&nPlatform, 1, 1, fpSMP);
        uint8 nVersion = 11;    VSIFWriteL(&nVersion, 1, 1, fpSMP);
        uint8 nDepth = 8;       VSIFWriteL(&nDepth, 1, 1, fpSMP);
        uint8 nHeadSz = 18;     VSIFWriteL(&nHeadSz, 1, 1, fpSMP);
        uint16 nCount = 255;    VSIFWriteL(&nCount, 2, 1, fpSMP);
        uint16 nMix = 0;        VSIFWriteL(&nMix, 2, 1, fpSMP);
        uint16 nMax = 255;      VSIFWriteL(&nMax, 2, 1, fpSMP);

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
        /* smp files always have 256 occurences */
        for (int i = poColorTable->GetColorEntryCount(); i <= 255; i++)
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

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr IdrisiRasterBand::SetUnitType(const char *pszUnitType)
{      
    //CPLDebug(extRST, "SetUnitType");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (strlen(pszUnitType) == 0)
    {
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcVALUE_UNITS, " unspecified");
    }
    else
    {
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcVALUE_UNITS, 
                            CPLSPrintf(" %s", pszUnitType));
    }

    return CE_None;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::SetStatistics(double dfMin, double dfMax, double dfMean, double dfStdDev)
{      
    //CPLDebug(extRST, "SetStatistics dfMin=%f dfMax=%f", dfMin, dfMax);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    double adfMin[3] = {0.0, 0.0, 0.0};
    double adfMax[3] = {0.0, 0.0, 0.0};

    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMIN_VALUE), "%lf %lf %lf", &adfMin[0], &adfMin[1], &adfMin[2]);
    sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMAX_VALUE), "%lf %lf %lf", &adfMax[0], &adfMax[1], &adfMax[2]);

    adfMin[nBand - 1] = dfMin;
    adfMax[nBand - 1] = dfMax;

    if (poGDS->nBands == 3)
    {
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf(" %.8g %.8g %.8g", adfMin[0], adfMin[1], adfMin[2]));
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf(" %.8g %.8g %.8g", adfMax[0], adfMax[1], adfMax[2]));
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf(" %.8g %.8g %.8g", adfMin[0], adfMin[1], adfMin[2]));
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf(" %.8g %.8g %.8g", adfMax[0], adfMax[1], adfMax[2]));
    }
    else
    {
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf(" %.8g", adfMin[0]));
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf(" %.8g", adfMax[0]));
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf(" %.8g", adfMin[0]));
        poGDS->papszRDC = 
            CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf(" %.8g", adfMax[0]));
    }
    return CE_None;
}

/************************************************************************/
/*                               TrimL()                                */
/************************************************************************/

char *TrimL(const char *pszText)
{
    int i;

    if( pszText == NULL )
        return CPLStrdup("");

    for (i = 0; (pszText[i] == ' ') && (pszText[i] != '\0'); i++);

    char *pszResult = CPLStrdup(&pszText[i]);

    return pszResult;
}

/************************************************************************/
/*                             FileExists()                             */
/************************************************************************/

bool FileExists(const char *pszFilename)
{
    FILE *fp;

    bool exist = ((fp = VSIFOpenL(pszFilename, "rb")) != NULL);

    if (exist)
    {
        VSIFCloseL(fp);
    }

    return exist;
}

/************************************************************************/
/*                        GDALRegister_IDRISI()                         */
/************************************************************************/

void GDALRegister_IDRISI()
{
    GDALDriver  *poDriver;

    if (GDALGetDriverByName("RST") == NULL)
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription("RST");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, rstVERSION);
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_Idrisi.html");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, extRST);
        poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int16 Float32");
        poDriver->pfnOpen = IdrisiDataset::Open;
        poDriver->pfnCreate	= IdrisiDataset::Create;
        poDriver->pfnCreateCopy = IdrisiDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}
