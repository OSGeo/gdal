/*****************************************************************************
 * $Id$
 *
 * Project:  Idrisi Raster Image File Driver
 * Purpose:  Read/write Idrisi Raster Image Format RST
 * Author:   Ivan Lucena, ivan@ilucena.net
 *
 ******************************************************************************
 * Copyright (c) 2006, Ivan Lucena
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.31  2007/01/10 23:30:21  fwarmerdam
 * Avoid crashing if the version string is not found in the .rdc file.
 * For instance, if the .rdc file isn't found.
 *
 * Revision 1.30  2006/10/11 08:32:00  dron
 * Use local CPLStrlwr() instead of unportable strlwr(); avoid warnings.
 *
 * Revision 1.29  2006/10/10 16:59:22  ilucena
 * Coordinate system support improvements
 *
 * Revision 1.28  2006/06/06 18:34:01  ilucena
 * Projection system and image extent error fixed
 *
 *
 */

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"
#include "gdal_pam.h"
#include "gdal_alg.h"

CPL_CVSID("$Id$");

CPL_C_START
void GDALRegister_IDRISI(void);
CPL_C_END

#ifdef WIN32        
#define PATHDELIM       '\\'
#else
#define PATHDELIM       '/'
#endif 

//----- Safe numeric conversion, NULL as zero
#define atoi_nz(s) (s == NULL ? (int)      0 : atoi(s))
#define atof_nz(s) (s == NULL ? (double) 0.0 : atof(s))

//----- file extensions:
#define extRST          "rst"
#define extRDC          "rdc"
#define extSMP          "smp"
#define extREF          "ref"

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

//----- ".ref" file field names:
#define refREF_SYSTEM   "ref. system "
#define refREF_SYSTEM2  "ref.system  "
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

//----- standard values:
#define rstVERSION      "Idrisi Raster A.1"
#define rstBYTE         "byte"
#define rstINTEGER      "integer"
#define rstREAL         "real"
#define rstRGB24        "rgb24"
#define rstDEGREE       "degrees"
#define rstMETER        "meters"
#define rstLATLONG      "latlong"
#define rstPLANE        "plane"
#define rstUTM          "utm-%d%c"
#define rstSPC			"spc%2d%2s%d"

//----- palette file (.smp) header size:
#define smpHEADERSIZE	18

//----- check if file exists:
bool FileExists(const char *pszFilename);

//----- Reference Table
struct ReferenceTab {
	int nCode;
	char *pszName;
};

//----- USA State's reference table to USGS PCS Code
static ReferenceTab aoUSStateTable[] = {
	{101,     "al"},
	{201,     "az"},
	{301,     "ar"},
	{401,     "ca"},
	{501,     "co"},
	{600,     "ct"},
	{700,     "de"},
	{901,     "fl"},
	{1001,    "ga"},
	{1101,    "id"},
	{1201,    "il"},
	{1301,    "in"},
	{1401,    "ia"},
	{1501,    "ks"},
	{1601,    "ky"},
	{1701,    "la"},
	{1801,    "me"},
	{1900,    "md"},
	{2001,    "ma"},
	{2111,    "mi"},
	{2201,    "mn"},
	{2301,    "ms"},
	{2401,    "mo"},
	{2500,    "mt"},
	{2600,    "ne"},
	{2701,    "nv"},
	{2800,    "nh"},
	{2900,    "nj"},
	{3001,    "nm"},
	{3101,    "ny"},
	{3200,    "nc"},
	{3301,    "nd"},
	{3401,    "oh"},
	{3501,    "ok"},
	{3601,    "or"},
	{3701,    "pa"},
	{3800,    "ri"},
	{3900,    "sc"},
	{4001,    "sd"},
	{4100,    "tn"},
	{4201,    "tx"},
	{4301,    "ut"},
	{4400,    "vt"},
	{4501,    "va"},
	{4601,    "wa"},
	{4701,    "wv"},
	{4801,    "wv"},
	{4901,    "wy"},
	{5001,    "ak"},
	{5101,    "hi"},
	{5200,    "pr"}
};
#define US_STATE_COUNT (sizeof(aoUSStateTable) / sizeof(ReferenceTab))

//----- Get the Code of a US State
int GetStateCode(const char *pszState);

//----- Get the state name of a Code
const char *GetStateName(int nCode);

//----- Conversion Table definition
struct ConvertionTab {
	char *pszName;
    int nDefault;
    double dfConv;
};

//----- Linear Unit Conversion Table
static ConvertionTab aoLinearUnitsConv[] = {
    {"Meters",      0,  1.0},
    {"Meter",       0,  1.0},
    {"Metre",       0,  1.0},
    {"M",           0,  1.0},
    {"Feet",        4,  0.3048},
    {"Foot",        4,  0.3048},
    {"Ft",          4,  0.3048},
    {"Miles",       7,  1612.9},
    {"Mi",          7,  1612.9},
    {"Kilometers",  9,  1000.0},
    {"Kilometer",   9,  1000.0}, 
    {"Kilometre",   9,  1000.0},
    {"Km",          9,  1000.0}
};
#define LINEAR_UNITS_COUNT (sizeof(aoLinearUnitsConv) / sizeof(ConvertionTab))

//----- Get the index of a given linear units
int GetUnitIndex(const char *pszUnitName);

//----- Get the defaut name
const char *GetUnitDefault(const char *pszUnitName);

//----- Classes pre-definition:
class IdrisiDataset;
class IdrisiRasterBand;

//  ----------------------------------------------------------------------------
//	    Idrisi GDALDataset
//  ----------------------------------------------------------------------------

class IdrisiDataset : public GDALPamDataset
{
	friend class IdrisiRasterBand;

private:
	FILE *fp;

	char *pszFilename;
	char *pszDocFilename;
	char **papszRDC;
	double adfGeoTransform[6];

	char *pszProjection;
	char **papszCategories;
	char *pszUnitType;

    CPLErr GeoReference2Wkt(const char *pszRefSystem,
                            const char *pszRefUnits,
                            char **pszProjString);

    CPLErr Wkt2GeoReference(const char *pszProjString,
	                        const char **pszRefSystem, 
	                        const char **pszRefUnit);

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
		char **papszOptions);
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
//	    Idrisi GDALPamRasterBand
//  ----------------------------------------------------------------------------

class IdrisiRasterBand : public GDALPamRasterBand
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
	virtual const char *GetUnitType();

	virtual CPLErr SetCategoryNames(char **papszCategoryNames);
	virtual CPLErr SetNoDataValue(double dfNoDataValue);
	virtual CPLErr SetColorTable(GDALColorTable *poColorTable); 
	virtual CPLErr SetUnitType(const char *pszUnitType);
	virtual CPLErr SetStatistics(double dfMin, double dfMax, 
		double dfMean, double dfStdDev);
};

//  ------------------------------------------------------------------------  //
//	                    Implementation of IdrisiDataset						  //
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
	papszCategories = NULL;
	pszUnitType = NULL;

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
		{            
			CSLSetNameValueSeparator(papszRDC, ": ");
			CSLSave(papszRDC, pszDocFilename);
		}
		CSLDestroy(papszRDC);
	}

	if (poColorTable)
		delete poColorTable;

	CPLFree(pszFilename);
	CPLFree(pszDocFilename);
	CPLFree(pszProjection);
	CSLDestroy(papszCategories);
	CPLFree(pszUnitType);

	if (fp != NULL)
		VSIFCloseL(fp);
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

	const char *pszLDocFilename = CPLResetExtension(poOpenInfo->pszFilename, extRDC);

	char **papszLRDC = CSLLoad(pszLDocFilename);

	CSLSetNameValueSeparator(papszLRDC, ":");

	const char *pszVersion = CSLFetchNameValue(papszLRDC, rdcFILE_FORMAT);

	if( pszVersion == NULL || !EQUAL(pszVersion, rstVERSION) )
	{
		CSLDestroy(papszLRDC);
		return NULL;
	}

	// -------------------------------------------------------------------- 
	//      Create a corresponding GDALDataset                   
	// -------------------------------------------------------------------- 

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
	{
		CSLDestroy(papszLRDC);
		return NULL;
	}

	poDS->pszDocFilename = CPLStrdup(pszLDocFilename);
	poDS->papszRDC = CSLDuplicate(papszLRDC);
	CSLDestroy(papszLRDC);

	// -------------------------------------------------------------------- 
	//      Load information from rdc
	// -------------------------------------------------------------------- 

	poDS->nRasterXSize = atoi_nz(CSLFetchNameValue(poDS->papszRDC, rdcCOLUMNS));
	poDS->nRasterYSize = atoi_nz(CSLFetchNameValue(poDS->papszRDC, rdcROWS));

	// -------------------------------------------------------------------- 
	//      Create band information
	// -------------------------------------------------------------------- 

	const char *pszDataType = CSLFetchNameValue(poDS->papszRDC, rdcDATA_TYPE);

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

	// -------------------------------------------------------------------- 
	//      Load the transformation matrix
	// -------------------------------------------------------------------- 

	const char *pszMinX = CSLFetchNameValue(poDS->papszRDC, rdcMIN_X);

	if (strlen(pszMinX) > 0)
	{
		double dfMinX, dfMaxX, dfMinY, dfMaxY, dfUnit, dfXPixSz, dfYPixSz;

		dfMinX = atof_nz(CSLFetchNameValue(poDS->papszRDC, rdcMIN_X));
		dfMaxX = atof_nz(CSLFetchNameValue(poDS->papszRDC, rdcMAX_X));
		dfMinY = atof_nz(CSLFetchNameValue(poDS->papszRDC, rdcMIN_Y));
		dfMaxY = atof_nz(CSLFetchNameValue(poDS->papszRDC, rdcMAX_Y));
		dfUnit = atof_nz(CSLFetchNameValue(poDS->papszRDC, rdcUNIT_DIST));

		dfMinX = dfMinX * dfUnit; 
		dfMaxX = dfMaxX * dfUnit; 
		dfMinY = dfMinY * dfUnit; 
		dfMaxY = dfMaxY * dfUnit;

		dfYPixSz = (dfMinY - dfMaxY) / poDS->nRasterYSize;
		dfXPixSz = (dfMaxX - dfMinX) / poDS->nRasterXSize;

		poDS->adfGeoTransform[0] = dfMinX;
		poDS->adfGeoTransform[1] = dfXPixSz;
		poDS->adfGeoTransform[2] = 0.0;
		poDS->adfGeoTransform[3] = dfMaxY;
		poDS->adfGeoTransform[4] = 0.0;
		poDS->adfGeoTransform[5] = dfYPixSz;
	}

	// -------------------------------------------------------------------- 
	//      Set Color Table in the presence of a smp file
	// -------------------------------------------------------------------- 

	if (poDS->nBands != 3)
	{
		const char *pszSMPFilename = CPLResetExtension(poDS->pszFilename, extSMP);
		FILE *fpSMP;
		if ((fpSMP = VSIFOpenL(pszSMPFilename, "rb")) != NULL )
		{
			double dfMaxValue = atof_nz(CSLFetchNameValue(poDS->papszRDC, rdcMAX_VALUE));
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
	//      Check for Unit Type
	// -------------------------------------------------------------------- 

	const char *pszValueUnit = CSLFetchNameValue(poDS->papszRDC, rdcVALUE_UNITS);

	if (pszValueUnit == NULL)
		poDS->pszUnitType = CPLStrdup("unspecified");
	else
	{
		if (EQUALN(pszValueUnit, "meter", 4))
		{
			poDS->pszUnitType = CPLStrdup("m");
		}
		else if (EQUALN(pszValueUnit, "feet", 4))
		{
			poDS->pszUnitType = CPLStrdup("ft");
		}
		else
			poDS->pszUnitType = CPLStrdup(pszValueUnit);
	}

	// -------------------------------------------------------------------- 
	//      Check for category names.
	// -------------------------------------------------------------------- 

	int nCatCount = atoi_nz(CSLFetchNameValue(poDS->papszRDC, rdcLEGEND_CATS));

	if (nCatCount > 0)
	{
		// ----------------------------------------------------------------
		//      Sequentialize categories names, from 0 to the last "code n"
		// ----------------------------------------------------------------

		int nCode = 0;
		int nCount = 0;
		int nLine = -1;
		for (int i = 0; (i < CSLCount(poDS->papszRDC)) && (nLine == -1); i++)
			if (EQUALN(poDS->papszRDC[i], rdcLEGEND_CATS, 11))
				nLine = i;
		if (nLine > 0)
		{
			sscanf(poDS->papszRDC[++nLine], rdcCODE_N, &nCode);
			for (int i = 0; (i < 255) && (nCount < nCatCount); i++)
			{
				if (i == nCode)
				{
					poDS->papszCategories = CSLAddString(poDS->papszCategories, 
						CPLParseNameValue(poDS->papszRDC[nLine], NULL));
					nCount++;
					if (nCount < nCatCount)
						sscanf(poDS->papszRDC[++nLine], rdcCODE_N, &nCode);
				}
				else
					poDS->papszCategories = CSLAddString(poDS->papszCategories, "");
			}
		}
	}

	/* -------------------------------------------------------------------- */
	/*      Check for external overviews.                                   */
	/* -------------------------------------------------------------------- */

	poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

	/* -------------------------------------------------------------------- */
	/*      Initialize any PAM information.                                 */
	/* -------------------------------------------------------------------- */

	poDS->SetDescription(poOpenInfo->pszFilename);
	poDS->TryLoadXML();

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
								   char **papszOptions)
{
	// -------------------------------------------------------------------- 
	//      Check input options
	// -------------------------------------------------------------------- 

	if (nBands != 1) 
	{
		if (! (nBands == 3 && eType == GDT_Byte))
		{
			CPLError(CE_Failure, CPLE_AppDefined,
				"Attempt to create IDRISI dataset with an illegal "
				"number of bands (%d) or data type (%s).\n",
				nBands, GDALGetDataTypeName(eType));
			return NULL;
		}
	}

	// ---------------------------------------------------------------- 
	//  Create the header file with minimun information
	// ---------------------------------------------------------------- 

	const char *pszLDataType;

	switch (eType)
	{	
	case GDT_Byte:
		if (nBands == 1)
			pszLDataType = rstBYTE;
		else
			pszLDataType = rstRGB24;
		break;
	case GDT_Int16:
		pszLDataType = rstINTEGER;
		break;
	case GDT_Float32:				
		pszLDataType = rstREAL;
		break;
	default:
		CPLError(CE_Failure, CPLE_AppDefined,
			"Attempt to create IDRISI dataset with an illegal "
			"data type (%s).\n",
			GDALGetDataTypeName(eType));
		return NULL;

	};

	char **papszLRDC;

	papszLRDC = NULL;
	papszLRDC = CSLAddNameValue(papszLRDC, rdcFILE_FORMAT,   rstVERSION);
	papszLRDC = CSLAddNameValue(papszLRDC, rdcFILE_TITLE,    "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcDATA_TYPE,     pszLDataType);
	papszLRDC = CSLAddNameValue(papszLRDC, rdcFILE_TYPE,     "binary");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcCOLUMNS,       CPLSPrintf("%d", nXSize));
	papszLRDC = CSLAddNameValue(papszLRDC, rdcROWS,          CPLSPrintf("%d", nYSize));
	papszLRDC = CSLAddNameValue(papszLRDC, rdcREF_SYSTEM,    "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcREF_UNITS,     "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcUNIT_DIST,     "1");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcMIN_X,         "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcMAX_X,         "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcMIN_Y,         "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcMAX_Y,         "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcPOSN_ERROR,    "unspecified");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcRESOLUTION,    "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcMIN_VALUE,     "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcMAX_VALUE,     "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcDISPLAY_MIN,   "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcDISPLAY_MAX,   "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcVALUE_UNITS,   "unspecified");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcVALUE_ERROR,   "unspecified");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcFLAG_VALUE,    "none");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcFLAG_DEFN,     "none");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcLEGEND_CATS,   "0");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcLINEAGES,      "");
	papszLRDC = CSLAddNameValue(papszLRDC, rdcCOMMENTS,      "");

	const char *pszLDocFilename;
	pszLDocFilename = CPLResetExtension(pszFilename, extRDC);

	CSLSetNameValueSeparator(papszLRDC, ": ");
	CSLSave(papszLRDC, pszLDocFilename);
	CSLDestroy(papszLRDC);

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
	if (!pfnProgress( 0.0, NULL, pProgressData))
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
			"Attempt to create IDRISI dataset with an illegal "
            "number of bands (%d).\n",
			poSrcDS->GetRasterCount());
		return NULL;
	}

	// -------------------------------------------------------------------------
	//      Check Data types
	// -------------------------------------------------------------------------

    int i;

	for ( i = 1; i <= poSrcDS->GetRasterCount(); i++)
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
				"Attempt to create IDRISI dataset with an illegal "
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

	double dfMin;
	double dfMax;
	double dfMean;
	double dfStdDev = -1;

	if (bStrict == TRUE)
	{
		poBand->GetStatistics(FALSE, TRUE, &dfMin, &dfMax, &dfMean, &dfStdDev);
	}
	else
	{
		dfMin = poBand->GetMinimum();
		dfMax = poBand->GetMaximum();
	}

	if ( ! ((eType == GDT_Byte) || 
		(eType == GDT_Int16) || 
		(eType == GDT_Float32)))
	{
		if (eType == GDT_Float64)
		{
			eType = GDT_Float32;
		}
		else
		{
			if ((dfMin < (double) SHRT_MIN) || 
				(dfMax > (double) SHRT_MAX))
			{
				eType = GDT_Float32; 
			}
			else
			{
				eType = GDT_Int16;
			}
		}
	}

	// --------------------------------------------------------------------
	//      Create the dataset
	// --------------------------------------------------------------------

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

	double adfGeoTransform[6];

	poDS->SetProjection(poSrcDS->GetProjectionRef());
	poSrcDS->GetGeoTransform(adfGeoTransform);
	poDS->SetGeoTransform(adfGeoTransform);

	// --------------------------------------------------------------------
	//      Copy information to the raster band
	// --------------------------------------------------------------------

	GDALRasterBand *poSrcBand;
	int bHasNoDataValue;
	double dfNoDataValue;

	for (i = 1; i <= poDS->nBands; i++)
	{
		poSrcBand = poSrcDS->GetRasterBand(i);
		poBand = poDS->GetRasterBand(i);

		if (i == 1)
		{
			poBand->SetCategoryNames(poSrcBand->GetCategoryNames());
			poBand->SetUnitType(poSrcBand->GetUnitType());
			poBand->SetColorTable(poSrcBand->GetColorTable());
		}
		poSrcBand->GetStatistics(false, true, &dfMin, &dfMax, &dfMean, &dfStdDev);
		poBand->SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
		dfNoDataValue = poSrcBand->GetNoDataValue(&bHasNoDataValue);
		if (bHasNoDataValue)
			poBand->SetNoDataValue(dfNoDataValue);
	}

	// --------------------------------------------------------------------
	//      Avoid misinterpretation with a pre-existent smp file
	// --------------------------------------------------------------------

    const char *pszPalletFName = CPLResetExtension(poDS->pszFilename, extSMP);

    if ((poDS->poColorTable == NULL) && 
        (FileExists(pszPalletFName)))
    {
        VSIUnlink(pszPalletFName);
    }

	// --------------------------------------------------------------------
	//      Copy image data
	// --------------------------------------------------------------------

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
				CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated CreateCopy()");
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
	if (GDALPamDataset::GetGeoTransform(padfTransform) != CE_None)
		memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

	return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

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
	dfMinX   = padfGeoTransform[0];
	dfMaxX   = (dfXPixSz * nRasterXSize) + dfMinX;
	dfMaxY   = padfGeoTransform[3];
	dfMinY   = (dfYPixSz * nRasterYSize) + dfMaxY;

	CSLSetNameValue(papszRDC, rdcMIN_X,      CPLSPrintf("%.7f", dfMinX));
	CSLSetNameValue(papszRDC, rdcMAX_X,      CPLSPrintf("%.7f", dfMaxX));
	CSLSetNameValue(papszRDC, rdcMIN_Y,      CPLSPrintf("%.7f", dfMinY));
	CSLSetNameValue(papszRDC, rdcMAX_Y,      CPLSPrintf("%.7f", dfMaxY));
	CSLSetNameValue(papszRDC, rdcRESOLUTION, CPLSPrintf("%.7f", -dfYPixSz));

	return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *IdrisiDataset::GetProjectionRef(void)
{
	const char *pszPamSRS = GDALPamDataset::GetProjectionRef();

	if (pszPamSRS != NULL && strlen(pszPamSRS) > 0)
		return pszPamSRS;

	if (pszProjection == NULL)
    {
	    const char *pszRefSystem = CSLFetchNameValue(papszRDC, rdcREF_SYSTEM);
	    const char *pszRefUnit = CSLFetchNameValue(papszRDC, rdcREF_UNITS);

	    GeoReference2Wkt(pszRefSystem, pszRefUnit, &pszProjection);
    }
    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr IdrisiDataset::SetProjection(const char *pszProjString)
{   
    CPLFree(pszProjection);
    pszProjection = CPLStrdup(pszProjString);
    CPLErr eResult = CE_None;

	const char *pszRefSystem;
	const char *pszRefUnit;

    eResult = Wkt2GeoReference(pszProjString, &pszRefSystem, &pszRefUnit);

    CSLSetNameValue(papszRDC, rdcREF_SYSTEM, pszRefSystem);
    CSLSetNameValue(papszRDC, rdcREF_UNITS,  pszRefUnit);

	return eResult;
}

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
			((GByte *) pImage)[i] = pabyScanLine[j];
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
			pabyScanLine[j] = ((GByte *) pImage)[i];
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
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	double adfMinValue[3];
	sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMIN_VALUE), "%lf %lf %lf", 
		&adfMinValue[0], &adfMinValue[1], &adfMinValue[2]);

	if (pbSuccess)
		*pbSuccess = TRUE;

	return adfMinValue[this->nBand - 1];
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double IdrisiRasterBand::GetMaximum(int *pbSuccess)
{      
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	double adfMaxValue[3];
	sscanf(CSLFetchNameValue(poGDS->papszRDC, rdcMAX_VALUE), "%lf %lf %lf", 
		&adfMaxValue[0], &adfMaxValue[1], &adfMaxValue[2]);

	if (pbSuccess)
		*pbSuccess = TRUE;

	return adfMaxValue[this->nBand - 1];
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double IdrisiRasterBand::GetNoDataValue(int *pbSuccess)
{      
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	double dfNoData;
	const char *pszFlagDefn;

	if (CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN) != NULL)
		pszFlagDefn = CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN);
	else if (CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN2) != NULL)
		pszFlagDefn = CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_DEFN2);

	// --------------------------------------------------------------------------
	// If Flag_Def is not "none", Flag_Value means "background" or "missing data"
	// --------------------------------------------------------------------------

	if (! EQUAL(pszFlagDefn, "none"))
	{
		dfNoData = atof_nz(CSLFetchNameValue(poGDS->papszRDC, rdcFLAG_VALUE));
		*pbSuccess = TRUE;
	}
	else
	{
		dfNoData = -9999.0;    /* this value should be ignored */
		*pbSuccess = FALSE;
	}

	return dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr IdrisiRasterBand::SetNoDataValue(double dfNoDataValue)
{      
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	poGDS->papszRDC = 
		CSLSetNameValue(poGDS->papszRDC, rdcFLAG_VALUE, CPLSPrintf("%.7g", dfNoDataValue));
	poGDS->papszRDC = 
		CSLSetNameValue(poGDS->papszRDC, rdcFLAG_DEFN,  "missing data");

	return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp IdrisiRasterBand::GetColorInterpretation()
{               
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	if (poGDS->nBands == 3)
	{
		switch (nBand)
		{
		case 1: return GCI_BlueBand;
		case 2: return GCI_GreenBand;
		case 3: return GCI_RedBand;
		}
	}
	else if (poGDS->poColorTable->GetColorEntryCount() > 0)
	{
		return GCI_PaletteIndex;
	}
	return GCI_GrayIndex;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **IdrisiRasterBand::GetCategoryNames()
{      
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	return poGDS->papszCategories;
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr IdrisiRasterBand::SetCategoryNames(char **papszCategoryNames)
{      
	int nCatCount = CSLCount(papszCategoryNames);

	if (nCatCount == 0)
		return CE_None;

	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	CSLDestroy(poGDS->papszCategories);
	poGDS->papszCategories = CSLDuplicate(papszCategoryNames);

	// ------------------------------------------------------
	//		Search for the "Legend cats  : N" line
	// ------------------------------------------------------

	int i, nLine = -1;
	for (i = 0; (i < CSLCount(poGDS->papszRDC)) && (nLine == -1); i++)
		if (EQUALN(poGDS->papszRDC[i], rdcLEGEND_CATS, 12))
			nLine = i;

	if (nLine < 0)
		return CE_None;

	int nCount = atoi_nz(CSLFetchNameValue(poGDS->papszRDC, rdcLEGEND_CATS));

	// ------------------------------------------------------
	//		Delte old instance of the categoty names
	// ------------------------------------------------------

	if (nCount > 0)
        poGDS->papszRDC = CSLRemoveStrings(poGDS->papszRDC, nLine + 1, nCount, NULL);

	nCount = 0;

	for (i = 0; i < nCatCount; i++)
	{
		if ((strlen(papszCategoryNames[i]) > 0))
		{
			poGDS->papszRDC = CSLInsertString(poGDS->papszRDC, (nLine + nCount + 1), 
				CPLSPrintf("%s:%s", CPLSPrintf(rdcCODE_N, i), papszCategoryNames[i]));
			nCount++;
		}
	}

	CSLSetNameValue(poGDS->papszRDC, rdcLEGEND_CATS, CPLSPrintf("%d", nCount));

	return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *IdrisiRasterBand::GetColorTable()
{               
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

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
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::SetColorTable(GDALColorTable *poColorTable)
{      
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
		GByte nPlatform = 1;    VSIFWriteL(&nPlatform, 1, 1, fpSMP);
		GByte nVersion = 11;    VSIFWriteL(&nVersion, 1, 1, fpSMP);
		GByte nDepth = 8;       VSIFWriteL(&nDepth, 1, 1, fpSMP);
		GByte nHeadSz = 18;     VSIFWriteL(&nHeadSz, 1, 1, fpSMP);
		GUInt16 nCount = 255;   VSIFWriteL(&nCount, 2, 1, fpSMP);
		GUInt16 nMix = 0;       VSIFWriteL(&nMix, 2, 1, fpSMP);
		GUInt16 nMax = 255;     VSIFWriteL(&nMax, 2, 1, fpSMP);

		GDALColorEntry oEntry;
		GByte aucRGB[3];
                int i;

		for (i = 0; i < poColorTable->GetColorEntryCount(); i++)
		{
			poColorTable->GetColorEntryAsRGB( i, &oEntry );
			aucRGB[0] = (GByte) oEntry.c1;
			aucRGB[1] = (GByte) oEntry.c2;
			aucRGB[2] = (GByte) oEntry.c3;
			VSIFWriteL(&aucRGB, 3, 1, fpSMP);
		}
		/* smp files always have 256 occurences */
		for (i = poColorTable->GetColorEntryCount(); i <= 255; i++)
		{
			poColorTable->GetColorEntryAsRGB( i, &oEntry );
			aucRGB[0] = (GByte) 0;
			aucRGB[1] = (GByte) 0;
			aucRGB[2] = (GByte) 0;
			VSIFWriteL(&aucRGB, 3, 1, fpSMP);
		}
		VSIFCloseL(fpSMP);
	}

	return CE_None;
}

/************************************************************************/
/*                           GetUnitType()                              */
/************************************************************************/

const char *IdrisiRasterBand::GetUnitType()
{
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	return poGDS->pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr IdrisiRasterBand::SetUnitType(const char *pszUnitType)
{      
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	if (strlen(pszUnitType) == 0)
	{
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcVALUE_UNITS, "unspecified");
	}
	else
	{
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcVALUE_UNITS, pszUnitType);
	}

	return CE_None;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

CPLErr IdrisiRasterBand::SetStatistics(double dfMin, double dfMax, double dfMean, double dfStdDev)
{      
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
			CSLSetNameValue(poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf("%.8g %.8g %.8g", adfMin[0], adfMin[1], adfMin[2]));
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf("%.8g %.8g %.8g", adfMax[0], adfMax[1], adfMax[2]));
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf("%.8g %.8g %.8g", adfMin[0], adfMin[1], adfMin[2]));
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf("%.8g %.8g %.8g", adfMax[0], adfMax[1], adfMax[2]));
	}
	else
	{
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcMIN_VALUE,   CPLSPrintf("%.8g", adfMin[0]));
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcMAX_VALUE,   CPLSPrintf("%.8g", adfMax[0]));
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MIN, CPLSPrintf("%.8g", adfMin[0]));
		poGDS->papszRDC = 
			CSLSetNameValue(poGDS->papszRDC, rdcDISPLAY_MAX, CPLSPrintf("%.8g", adfMax[0]));
	}

	return GDALRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
}

/************************************************************************/
/*                       GeoReference2Wkt()                             */
/************************************************************************/

/***
 * Converts Idrisi geographic reference information to OpenGIS WKT.
 * 
 * The Idrisi metadata file contain two fields that describe the  
 * geographic reference, RefSystem and RefUnit. 
 * 
 * RefSystem can contains the world "plane" or the name of a georeference 
 * file <refsystem>.ref that details the geographic reference  
 * system (coordinate system and projection parameters). RefUnits 
 * indicates the unit of the image bounds. 
 * 
 * The georeference files are generally located in the product installation 
 * folder $IDRISIDIR\Georef, but they are first looked for in the same  
 * folder as the data file. 
 *  
 * If a Reference system names can be recognized by a name convention 
 * it will be interpreted without the need to read the georeference file. 
 * That includes "latlong" and all the UTM and State Plane zones.  
 * 
 * RefSystem "latlong" means that the data is not project and the coordinate 
 * system is WGS84. RefSystem "plane" means that the there is no coordinate 
 * system but the it is possible to calculate areas and distance by looking
 * at the RefUnits.
 *  
 * If the environment variable IDRISIDIR is not set and the georeference file  
 * need to be read then the projection string will result as unknown. 
 ***/

CPLErr IdrisiDataset::GeoReference2Wkt(const char *pszRefSystem,
                                       const char *pszRefUnits,
                                       char **pszProjString)
{
    OGRSpatialReference oSRS;

	// ---------------------------------------------------------
	//  Plane 
 	// ---------------------------------------------------------

	if EQUAL(pszRefSystem, rstPLANE)
    {
        oSRS.SetLocalCS("Plane");
        int nUnit = GetUnitIndex(GetUnitDefault(pszRefUnits));
        if (nUnit > -1)
        {
            oSRS.SetLinearUnits(aoLinearUnitsConv[nUnit].pszName,
                                aoLinearUnitsConv[nUnit].dfConv);
        }
        oSRS.exportToWkt(pszProjString);
        return CE_None;
    }

    // ---------------------------------------------------------
    //  Latlong
    // ---------------------------------------------------------

    if EQUAL(pszRefSystem, rstLATLONG)
    {
            oSRS.SetWellKnownGeogCS("WGS84");
    oSRS.exportToWkt(pszProjString);
    return CE_None;
    }

    // ---------------------------------------------------------
    //  Prepare for scanning in lower case
    // ---------------------------------------------------------

    char *pszRefSystemLower;
    pszRefSystemLower = CPLStrdup(pszRefSystem);
    CPLStrlwr(pszRefSystemLower);

    // ---------------------------------------------------------
    //  UTM naming convention (ex.: utm-30n)
    // ---------------------------------------------------------

    if EQUALN(pszRefSystem, rstUTM, 3)
    {
            int	nZone;
            char cNorth;
            sscanf(pszRefSystemLower, rstUTM, &nZone, &cNorth);
            oSRS.SetWellKnownGeogCS("WGS84");
            oSRS.SetUTM(nZone, (cNorth == 'n'));
    oSRS.exportToWkt(pszProjString);
    return CE_None;
    }

    // ---------------------------------------------------------
    //  State Plane naming convention (ex.: spc83ma1)
    // ---------------------------------------------------------

    if EQUALN(pszRefSystem, rstSPC, 3)
    {
        int nNAD;
        int nZone;
        char szState[3];
        sscanf(pszRefSystemLower, rstSPC, &nNAD, szState, &nZone);
        int nSPCode = GetStateCode(szState);
        if (nSPCode != -1)
        {
            nZone = (nZone == 1 ? nSPCode : nSPCode + nZone - 1);

            if (oSRS.SetStatePlane(nZone, (nNAD == 83)) != OGRERR_FAILURE)
            {
                oSRS.exportToWkt(pszProjString);
                return CE_None;
            }

            // ----------------------------------------------------------
            //  If SetStatePlane fails, set GeoCS as NAD Datum and let it
            //  try to read the projection info from georeference file (*)
            // ----------------------------------------------------------

            oSRS.SetWellKnownGeogCS(CPLSPrintf("NAD%d", nNAD));
        }
    }

    // ------------------------------------------------------------------
    //  Search for georeference file <RefSystem>.ref
    // ------------------------------------------------------------------

    const char *pszFName = CPLSPrintf("%s%c%s.ref", 
    CPLGetDirname(pszFilename), PATHDELIM,  pszRefSystem);

    if (FileExists(pszFName) == FALSE)
    {
        // ------------------------------------------------------------------
        //  Look at $IDRISIDIR\Georef\<RefSystem>.ref
        // ------------------------------------------------------------------

        const char *pszIdrisiDir = CPLGetConfigOption("IDRISIDIR", NULL);
        if ((pszIdrisiDir) != NULL)
        {
            pszFName = CPLSPrintf("%s%cgeoref%c%s.ref", 
            pszIdrisiDir, PATHDELIM, PATHDELIM, pszRefSystem);
        }
    }

    // ------------------------------------------------------------------
    //  Cannot find georeference file
    // ------------------------------------------------------------------

    if (FileExists(pszFName) == FALSE)
    {
        CPLDebug("RST", "Cannot find Idrisi georeference file %d.ref",
                 pszRefSystem);

        if (oSRS.IsGeographic() == FALSE) /* see State Plane remarks (*) */
        {
            oSRS.SetLocalCS("Unknown");
            int nUnit = GetUnitIndex(GetUnitDefault(pszRefUnits));
            if (nUnit > -1)
            {
                oSRS.SetLinearUnits(aoLinearUnitsConv[nUnit].pszName,
                                    aoLinearUnitsConv[nUnit].dfConv);
            }
        }
        oSRS.exportToWkt(pszProjString);
        return CE_Failure;
    }

	// ------------------------------------------------------------------
	//  Read values from georeference file 
    // ------------------------------------------------------------------

    char **papszRef = CSLLoad(pszFName);
	CSLSetNameValueSeparator(papszRef, ":");

    const char *pszGeorefName   = CPLStrdup(CSLFetchNameValue(papszRef, refREF_SYSTEM));
    if EQUAL(pszGeorefName, "") 
	    pszGeorefName           = CPLStrdup(CSLFetchNameValue(papszRef, refREF_SYSTEM2));
	const char *pszProjName     = CPLStrdup(CSLFetchNameValue(papszRef, refPROJECTION));
	const char *pszDatum		= CPLStrdup(CSLFetchNameValue(papszRef, refDATUM));
	const char *pszEllipsoid    = CPLStrdup(CSLFetchNameValue(papszRef, refELLIPSOID));
	double dfCenterLat		    = atof_nz(CSLFetchNameValue(papszRef, refORIGIN_LAT));
	double dfCenterLong	        = atof_nz(CSLFetchNameValue(papszRef, refORIGIN_LONG));
	double dfSemiMajor		    = atof_nz(CSLFetchNameValue(papszRef, refMAJOR_SAX));
	double dfSemiMinor		    = atof_nz(CSLFetchNameValue(papszRef, refMINOR_SAX));
	double dfFalseEasting	    = atof_nz(CSLFetchNameValue(papszRef, refORIGIN_X));
	double dfFalseNorthing	    = atof_nz(CSLFetchNameValue(papszRef, refORIGIN_Y));
    double dfStdP1			    = atof_nz(CSLFetchNameValue(papszRef, refSTANDL_1));
	double dfStdP2			    = atof_nz(CSLFetchNameValue(papszRef, refSTANDL_2));
    double dfScale;
	double adfToWGS84[3];

    sscanf(CSLFetchNameValue(papszRef, refDELTA_WGS84), "%lf %lf %lf", 
		&adfToWGS84[0], &adfToWGS84[1], &adfToWGS84[2]);

	if (EQUAL(CSLFetchNameValue(papszRef, refSCALE_FAC), "na"))
		dfScale = 1.0;
	else
		dfScale	= atof_nz(CSLFetchNameValue(papszRef, refSCALE_FAC));

    CSLDestroy(papszRef);

    // ----------------------------------------------------------------------
	//  Set the Geographic Coordinate System
    // ----------------------------------------------------------------------

    if (oSRS.IsGeographic() == FALSE) /* see State Plane remarks (*) */
    {
        int nEPSG = 0;

        // ----------------------------------------------------------------------
	    //  Is it a WGS84 equivalent?
        // ----------------------------------------------------------------------

        if ((EQUALN(pszEllipsoid, "WGS", 3)) && (strstr(pszEllipsoid, "84")) &&
            (EQUALN(pszDatum, "WGS", 3))     && (strstr(pszDatum, "84")) &&
            (adfToWGS84[0] == 0.0) && (adfToWGS84[1] == 0.0) && (adfToWGS84[2] == 0.0))
        {
            nEPSG = 4326;
        }

        // ----------------------------------------------------------------------
	    //  Match GCS's DATUM_NAME by using 'ApproxString' over Datum 
        // ----------------------------------------------------------------------

        if (nEPSG == 0)
        {
            nEPSG = atoi_nz(CSVGetField(CSVFilename("gcs.csv"), 
                "DATUM_NAME", pszDatum, CC_ApproxString, "COORD_REF_SYS_CODE"));
        }

        // ----------------------------------------------------------------------
	    //  Match GCS's COORD_REF_SYS_NAME by using 'ApproxString' over Datum 
        // ----------------------------------------------------------------------

        if (nEPSG == 0)
        {
            nEPSG = atoi_nz(CSVGetField(CSVFilename("gcs.csv"), 
                "COORD_REF_SYS_NAME", pszDatum, CC_ApproxString, "COORD_REF_SYS_CODE"));
        }

        if (nEPSG != 0)
        {
            oSRS.importFromEPSG(nEPSG);
        }
        else
        {
            // --------------------------------------------------
            //  Create GeogCS based on the georeference file info
            // --------------------------------------------------

            oSRS.SetGeogCS(pszRefSystem, 
                pszDatum, 
                pszEllipsoid, 
                dfSemiMajor, 
                (-1.0 / (dfSemiMinor / dfSemiMajor - 1.0)));
        }

        // ----------------------------------------------------------------------
        //  Note: That will override EPSG info:
        // ----------------------------------------------------------------------

        oSRS.SetTOWGS84(adfToWGS84[0], adfToWGS84[1], adfToWGS84[2]);
    }

    // ----------------------------------------------------------------------
    //  If the georeference file tells that it is a non project system:
    // ----------------------------------------------------------------------

    if EQUAL(pszProjName, "none")
	{
        oSRS.exportToWkt(pszProjString);
        return CE_None;
    }

    // ----------------------------------------------------------------------
	//  Create Projection information based on georeference file info
	// ----------------------------------------------------------------------
    
    //  Idrisi user's Manual,   Supported Projection:
    //
    //      Mercator
    //      Transverse Mercator
    //      Gauss-Kruger
    //      Lambert Conformal Conic
    //      Plate Carrée
    //      Hammer Aitoff
    //      Lambert North Polar Azimuthal Equal Area
    //      Lambert South Polar Azimuthal Equal Area
    //      Lambert Transverse Azimuthal Equal Area
    //      Lambert Oblique Polar Azimuthal Equal Area
    //      North Polar Stereographic
    //      South Polar Stereographic
    //      Transverse Stereographic
    //      Oblique Stereographic
    //      Albers Equal Area Conic
    //      Sinusoidal
    //

    if EQUAL(pszProjName, "Mercator")
	{
		oSRS.SetMercator(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUAL(pszProjName, "Transverse Mercator")
	{
		oSRS.SetTM(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUALN(pszProjName, "Gauss-Kruger", 9)
	{
		oSRS.SetTM(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUAL(pszProjName, "Lambert Conformal Conic")
	{
		oSRS.SetLCC(dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUALN(pszProjName, "Plate Carrée", 10)
	{
        oSRS.SetEquirectangular(dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUAL(pszProjName, "Hammer Aitoff")
	{
        oSRS.SetProjection(pszProjName);
		oSRS.SetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,  dfCenterLat);
		oSRS.SetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,    dfCenterLong);
		oSRS.SetNormProjParm(SRS_PP_FALSE_EASTING,       dfFalseEasting);
		oSRS.SetNormProjParm(SRS_PP_FALSE_NORTHING,      dfFalseNorthing);
	}
	else if (EQUALN(pszProjName, "Lambert North Polar Azimuthal Equal Area", 15) ||
             EQUALN(pszProjName, "Lambert South Polar Azimuthal Equal Area", 15) ||
             EQUALN(pszProjName, "Lambert Transverse Azimuthal Equal Area", 15) ||
             EQUALN(pszProjName, "Lambert Oblique Polar Azimuthal Equal Area", 15))
	{
		oSRS.SetLAEA(dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	}
	else if (EQUALN(pszProjName, "North Polar Stereographic", 15) ||
             EQUALN(pszProjName, "South Polar Stereographic", 15))
    {
		oSRS.SetPS(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUALN(pszProjName, "Transverse Stereographic", 15)
	{
		oSRS.SetStereographic(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUALN(pszProjName, "Oblique Stereographic", 15)
	{
		oSRS.SetOS(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	}
	else if (EQUAL(pszProjName, "Alber's Equal Area Conic") ||
             EQUAL(pszProjName, "Albers Equal Area Conic"))
	{
		oSRS.SetACEA(dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	}
	else if EQUAL(pszProjName, "Sinusoidal")
	{
		oSRS.SetSinusoidal(dfCenterLong, dfFalseEasting, dfFalseNorthing);
	}
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported, 
            "Projection not listed on Idrisi User's Manual (v.15.0/2005).\n\t" 
            "[\"%s\" in georeference file \"%s\"]",
            pszProjName, pszFName);
        oSRS.Clear();
        oSRS.exportToWkt(pszProjString);
        return CE_Warning;
    }

    // ----------------------------------------------------------------------
    //  Set the Linear Units
    // ----------------------------------------------------------------------

    int nUnit = GetUnitIndex(GetUnitDefault(pszRefUnits));
    if (nUnit > -1)
    {
        oSRS.SetLinearUnits(aoLinearUnitsConv[nUnit].pszName,
                            aoLinearUnitsConv[nUnit].dfConv);
    }

	// ----------------------------------------------------------------------
    //  Name ProjCS with the name on the georeference file
    // ----------------------------------------------------------------------

    oSRS.SetProjCS(pszGeorefName);

    oSRS.exportToWkt(pszProjString);
    return CE_None;
}

/************************************************************************/
/*                        Wkt2GeoReference()                            */
/************************************************************************/

/***
 * Converts OpenGIS WKT to Idrisi geographic reference information.
 * 
 * That function will fill up the two parameters RefSystem and RefUnit
 * that goes into the Idrisi metadata. But it could also create
 * a companying georeference file to the output if necessary.
 *
 * First it will try to identify the ProjString as Local, WGS84 or
 * one of the Idrisi name convention reference systems
 * otherwise, if the projection system is supported by Idrisi,  
 * it will create a companying georeference files.
 ***/

CPLErr IdrisiDataset::Wkt2GeoReference(const char *pszProjString,
                                       const char **pszRefSystem, 
                                       const char **pszRefUnit)
{
    // -----------------------------------------------------
	//  Plane with default "Meters"
	// -----------------------------------------------------

    if EQUAL(pszProjString, "")
    {
	    *pszRefSystem = CPLStrdup(rstPLANE);
        *pszRefUnit   = CPLStrdup(rstMETER);
	    return CE_None;
    }

    OGRSpatialReference oSRS;
    oSRS.importFromWkt((char **) &pszProjString);

    // -----------------------------------------------------
	//  Local => Plane + Linear Unit
	// -----------------------------------------------------

    if (oSRS.IsLocal())
    {
	    *pszRefSystem = CPLStrdup(rstPLANE);
	    *pszRefUnit   = GetUnitDefault(oSRS.GetAttrValue("UNIT"));
	    return CE_None;
    }

    // -----------------------------------------------------
	//  Test to identify WGS84 => Latlong + Angular Unit
	// -----------------------------------------------------

    if (oSRS.IsGeographic())
    {
        const char *pszSpheroid     = CPLStrdup(oSRS.GetAttrValue("SPHEROID"));
        const char *pszAuthName     = CPLStrdup(oSRS.GetAuthorityName("GEOGCS"));
        const char *pszDatum        = CPLStrdup(oSRS.GetAttrValue("DATUM"));
        int nGCSCode = -1;
        if EQUAL(pszAuthName, "EPSG")
        {
            nGCSCode = atoi(oSRS.GetAuthorityCode("GEOGCS"));
        }
        if ((nGCSCode == 4326) || (
            (EQUALN(pszSpheroid, "WGS", 3)) && (strstr(pszSpheroid, "84")) &&
            (EQUALN(pszDatum, "WGS", 3))    && (strstr(pszDatum, "84"))) )
        {
	        *pszRefSystem = CPLStrdup(rstLATLONG);
	        *pszRefUnit   = CPLStrdup(rstDEGREE);
            return CE_None;
        }
    }

    // -----------------------------------------------------
	//  Prepare to match some projections 
	// -----------------------------------------------------

    const char *pszProjection = CPLStrdup(oSRS.GetAttrValue("PROJECTION"));

    // -----------------------------------------------------
	//  Check for UTM zones
	// -----------------------------------------------------

    if EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR)
	{
		int nZone = oSRS.GetUTMZone();

		if ((nZone != 0) && (EQUAL(oSRS.GetAttrValue("DATUM"), SRS_DN_WGS84)))
		{
			double dfNorth = oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING);
			*pszRefSystem  = CPLStrdup(CPLSPrintf(rstUTM, nZone, (dfNorth == 0.0 ? 'n' : 's')));
			*pszRefUnit    = CPLStrdup(rstMETER);
			return CE_None;
		}
	}

	// -----------------------------------------------------
	//  Check for State Plane
	// -----------------------------------------------------

    if (EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) ||
        EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR))
    {
        const char *pszPCSCode;
        const char *pszID = CPLStrdup(oSRS.GetAuthorityCode("PROJCS"));
        if (strlen(pszID) > 0)
        {
            pszPCSCode = CPLStrdup(CSVGetField(CSVFilename("stateplane.csv"),
                "EPSG_PCS_CODE", pszID, CC_Integer, "ID"));
            if (strlen(pszPCSCode) > 0)
            {
                int nNADYear	= 83;
                int nZone		= pszPCSCode[strlen(pszPCSCode) - 1] - '0';
                int nSPCode    = atoi_nz(pszPCSCode);

                if (nZone == 0)
                    nZone = 1;
                else
                    nSPCode = nSPCode - nZone + 1;

                if (nSPCode > 10000)
                {
                    nNADYear = 27;
                    nSPCode -= 10000;
                }
                char *pszState  = CPLStrdup(GetStateName(nSPCode));
                if (! EQUAL(pszState, ""))
                {
                    *pszRefSystem	= CPLStrdup(CPLSPrintf(rstSPC, nNADYear, pszState, nZone));
                    *pszRefUnit     = GetUnitDefault(oSRS.GetAttrValue("UNIT"));
                    return CE_None;
                }
            }
        }
    }

	const char *pszProjectionOut = NULL;

    if (oSRS.IsProjected())
    {
        // ---------------------------------------------------------
	    //  Check for supported projections
	    // ---------------------------------------------------------

        if EQUAL(pszProjection, SRS_PT_MERCATOR_1SP)
        {
            pszProjectionOut = CPLStrdup("Mercator");
        }
	    if EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR)         
        {   
            pszProjectionOut = CPLStrdup("Transverse Mercator");
        }
        else if EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)
        {
            pszProjectionOut = CPLStrdup("Lambert Conformal Conic");
        }
        else if EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR)
        {
            pszProjectionOut = CPLStrdup("Plate Carrée");
        }
        else if EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA)
        {   
    	    double dfCenterLat = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0, NULL);
            if (dfCenterLat == 0.0)
                pszProjectionOut = CPLStrdup("Lambert Transverse Azimuthal Equal Area"); 
            else if (fabs(dfCenterLat) == 90.0)
                pszProjectionOut = CPLStrdup("Lambert Oblique Polar Azimuthal Equal Area");
            else if (dfCenterLat > 0.0)
                pszProjectionOut = CPLStrdup("Lambert North Oblique Azimuthal Equal Area"); 
            else
                pszProjectionOut = CPLStrdup("Lambert South Oblique Azimuthal Equal Area");
        }
        else if EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC)
        {
            if (oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0, NULL) > 0)
                pszProjectionOut = CPLStrdup("North Polar Stereographic");
            else
                pszProjectionOut = CPLStrdup("South Polar Stereographic");
        }
        else if EQUAL(pszProjection, SRS_PT_STEREOGRAPHIC)
        {
            pszProjectionOut = CPLStrdup("Transverse Stereographic");
        }
        else if EQUAL(pszProjection, SRS_PT_OBLIQUE_STEREOGRAPHIC)
        {
            pszProjectionOut = CPLStrdup("Oblique Stereographic");
        }
        else if EQUAL(pszProjection, SRS_PT_SINUSOIDAL)
        {
            pszProjectionOut = CPLStrdup("Sinusoidal");
        }
        else if EQUAL(pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA)
        {
            pszProjectionOut = CPLStrdup("Alber's Equal Area Conic");
        }

        // ---------------------------------------------------------
        //  Failure, Projection system not suppotted
        // ---------------------------------------------------------

        if (pszProjectionOut == NULL)
        {
            CPLDebug("RST", "Not support by RST driver: PROJECTION[\"%s\"]", pszProjection);

            *pszRefSystem = CPLStrdup(rstPLANE);
            *pszRefUnit   = CPLStrdup(rstMETER);
            return CE_Failure;
        }
    }
    else
    {
        pszProjectionOut = CPLStrdup("none");
    }

    // ---------------------------------------------------------
    //  Prepare to write ref file
    // ---------------------------------------------------------

	const char *pszGeorefName   = CPLStrdup("Unknown");
	const char *pszDatum        = CPLStrdup(oSRS.GetAttrValue("DATUM"));
	const char *pszEllipsoid    = CPLStrdup(oSRS.GetAttrValue("SPHEROID"));
	double dfSemiMajor          = oSRS.GetSemiMajor();		
	double dfSemiMinor          = oSRS.GetSemiMinor();		
	double adfToWGS84[3];
    oSRS.GetTOWGS84(adfToWGS84, 3);

    double dfCenterLat          = 0.0;
	double dfCenterLong         = 0.0;	    
	double dfFalseNorthing      = 0.0;	
	double dfFalseEasting       = 0.0;	
    double dfScale              = 1.0;
    int nParameters             = 0;         
    double dfStdP1              = 0.0;			
    double dfStdP2              = 0.0;			
    const char *pszUnit         = CPLStrdup(oSRS.GetAttrValue("GEOGCS|UNIT"));

    if (oSRS.IsProjected())
    {
        pszGeorefName   = CPLStrdup(oSRS.GetAttrValue("PROJCS"));
        dfCenterLat     = oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0, NULL);
        dfCenterLong    = oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0, NULL);
        dfFalseNorthing = oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0, NULL);
        dfFalseEasting  = oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0, NULL);
        dfScale         = oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 0.0, NULL);
        dfStdP1         = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, -0.1, NULL);
        dfStdP2         = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, -0.1, NULL);  
        if (dfStdP1 != -0.1)
        {
            nParameters = 1;
            if (dfStdP2 != -0.1)
                nParameters = 2;
        }
    }

    // ---------------------------------------------------------
    //  Create a compaining georeference file for this dataset
    // ---------------------------------------------------------

    char **papszRef = NULL;
    papszRef = CSLAddNameValue(papszRef, refREF_SYSTEM,   pszGeorefName);  
    papszRef = CSLAddNameValue(papszRef, refPROJECTION,   pszProjectionOut);  
    papszRef = CSLAddNameValue(papszRef, refDATUM,        pszDatum);     
    papszRef = CSLAddNameValue(papszRef, refDELTA_WGS84,  CPLSPrintf("%.3g %.3g %.3g", 
                                           adfToWGS84[0], adfToWGS84[1], adfToWGS84[2]));
    papszRef = CSLAddNameValue(papszRef, refELLIPSOID,    pszEllipsoid);
    papszRef = CSLAddNameValue(papszRef, refMAJOR_SAX,    CPLSPrintf("%.3f", dfSemiMajor));
    papszRef = CSLAddNameValue(papszRef, refMINOR_SAX,    CPLSPrintf("%.3f", dfSemiMinor));
    papszRef = CSLAddNameValue(papszRef, refORIGIN_LONG,  CPLSPrintf("%.9g", dfCenterLong));
    papszRef = CSLAddNameValue(papszRef, refORIGIN_LAT,   CPLSPrintf("%.9g", dfCenterLat));
    papszRef = CSLAddNameValue(papszRef, refORIGIN_X,     CPLSPrintf("%.9g", dfFalseEasting));
    papszRef = CSLAddNameValue(papszRef, refORIGIN_Y,     CPLSPrintf("%.9g", dfFalseNorthing));
    papszRef = CSLAddNameValue(papszRef, refSCALE_FAC,    CPLSPrintf("%.9g", dfScale));
    papszRef = CSLAddNameValue(papszRef, refUNITS,        pszUnit);
    papszRef = CSLAddNameValue(papszRef, refPARAMETERS,   CPLSPrintf("%1d",  nParameters));
    if (nParameters > 0)
        papszRef = CSLAddNameValue(papszRef, refSTANDL_1, CPLSPrintf("%.9g", dfStdP1));
    if (nParameters > 1)
        papszRef = CSLAddNameValue(papszRef, refSTANDL_2, CPLSPrintf("%.9g", dfStdP2));
    CSLSetNameValueSeparator(papszRef, ": ");
    CSLSave(papszRef, CPLResetExtension(pszFilename, extREF));
    CSLDestroy(papszRef);

    *pszRefSystem = CPLStrdup(CPLGetBasename(pszFilename));
    *pszRefUnit   = CPLStrdup(pszUnit);

    return CE_None;
}

/************************************************************************/
/*                             FileExists()                             */
/************************************************************************/

bool FileExists(const char *pszFilename)
{
    VSIStatBuf  sStat;
    return bool(CPLStat(pszFilename, &sStat) == 0);
}

/************************************************************************/
/*                            GetStateCode()                            */
/************************************************************************/

int GetStateCode(const char *pszState)
{
    unsigned int i;

    for (i = 0; i < US_STATE_COUNT; i++)
    {
		if EQUAL(pszState, aoUSStateTable[i].pszName)
        {
            return aoUSStateTable[i].nCode;
        }
    }
    return -1;
}

/************************************************************************/
/*                            GetStateCode()                            */
/************************************************************************/

const char *GetStateName(int nCode)
{
    unsigned int i;

    for (i = 0; i < US_STATE_COUNT; i++)
    {
		if (nCode == aoUSStateTable[i].nCode)
        {
            return aoUSStateTable[i].pszName;
        }
    }
    return NULL;
}

/************************************************************************/
/*                            GetUnitIndex()                            */
/************************************************************************/

int GetUnitIndex(const char *pszUnitName)
{
    unsigned int i;

    for (i = 0; i < LINEAR_UNITS_COUNT; i++)
    {
        if EQUAL(pszUnitName, aoLinearUnitsConv[i].pszName)
        {
            return i;
        }
    }
    return -1;
}

/************************************************************************/
/*                            GetUnitDefault()                          */
/************************************************************************/

const char *GetUnitDefault(const char *pszUnitName)
{
    int nIndex = GetUnitIndex(pszUnitName);

    if (nIndex == -1)
    {
        return CPLStrdup("Meter");
    }
    else
    {
        return CPLStrdup(aoLinearUnitsConv[aoLinearUnitsConv[nIndex].nDefault].pszName);
    }
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
