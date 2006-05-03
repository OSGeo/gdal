/*****************************************************************************
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
#include "cpl_csv.h"
#include "ogr_spatialref.h"
#include "gdal_pam.h"

CPL_C_START
void GDALRegister_IDRISI(void);
CPL_C_END

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
#define rstDEGREE       "degree"
#define rstMETER        "meter"
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

//----- USA State's reference table to EPSG PCS Code
static ReferenceTab aoUSStateTable[] = {
	{101,     "AL"},
	{201,     "AZ"},
	{301,     "AR"},
	{401,     "CA"},
	{501,     "CO"},
	{600,     "CT"},
	{700,     "DE"},
	{901,     "FL"},
	{1001,    "GA"},
	{1101,    "ID"},
	{1201,    "IL"},
	{1301,    "IN"},
	{1401,    "IA"},
	{1501,    "KS"},
	{1601,    "KY"},
	{1701,    "LA"},
	{1801,    "ME"},
	{1900,    "MD"},
	{2001,    "MA"},
	{2111,    "MI"},
	{2201,    "MN"},
	{2301,    "MS"},
	{2401,    "MO"},
	{2500,    "MT"},
	{2600,    "NE"},
	{2701,    "NV"},
	{2800,    "NH"},
	{2900,    "NJ"},
	{3001,    "NM"},
	{3101,    "NY"},
	{3200,    "NC"},
	{3301,    "ND"},
	{3401,    "OH"},
	{3501,    "OK"},
	{3601,    "OR"},
	{3701,    "PA"},
	{3800,    "RI"},
	{3900,    "SC"},
	{4001,    "SD"},
	{4100,    "TN"},
	{4201,    "TX"},
	{4301,    "UT"},
	{4400,    "VT"},
	{4501,    "VA"},
	{4601,    "WA"},
	{4701,    "WV"},
	{4801,    "WV"},
	{4901,    "WY"},
	{5001,    "AK"},
	{5101,    "HI"},
	{5200,    "PR"}
};
#define US_STATE_COUNT (sizeof(aoUSStateTable) / sizeof(ReferenceTab))

//----- Import geo reference from Idrisi
OGRErr CPL_DLL OSRImportFromIdrisi(OGRSpatialReferenceH hSRS, 
								   const char *pszRefSystem, 
								   const char *pszRefUnits, 
								   char **papszRefFile = NULL);

//----- Export geo reference to Idrisi
OGRErr CPL_DLL OSRExportToIdrisi(OGRSpatialReferenceH hSRS, 
								 char **pszRefSystem, 
								 char **pszRefUnit, 
								 char ***papszRefFile);

//----- Specialized version of SetAuthority that accept non-numeric codes
OGRErr OSRSetAuthorityLabel( OGRSpatialReferenceH hSRS, 
                        const char *pszTargetKey,
                        const char *pszAuthority,
                        const char *pszLabel );

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

	if (EQUAL(pszVersion, rstVERSION) == FALSE)
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

		poDS->adfGeoTransform[0] = dfMinX - (dfXPixSz / 2);
		poDS->adfGeoTransform[1] = dfXPixSz;
		poDS->adfGeoTransform[2] = 0.0;
		poDS->adfGeoTransform[3] = dfMaxY + (dfYPixSz / 2);
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
			"Attempt to create IDRISI dataset with an illegal\n"
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

	for (int i = 1; i <= poDS->nBands; i++)
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
	//      Save Lineage information
	// --------------------------------------------------------------------

	poDS->papszRDC = CSLSetNameValue(poDS->papszRDC, rdcLINEAGES, 
		CPLSPrintf("Generated by GDAL, source = (%s)", poSrcDS->GetDescription()));

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
	dfMinX   = padfGeoTransform[0] + (dfXPixSz / 2);
	dfMaxX   = (dfXPixSz * nRasterXSize) + dfMinX;
	dfMaxY   = padfGeoTransform[3] - (dfYPixSz / 2);
	dfMinY   = (dfYPixSz * nRasterYSize) + dfMaxY;

	CSLSetNameValue(papszRDC, rdcMIN_X,      CPLSPrintf("%.8g", dfMinX));
	CSLSetNameValue(papszRDC, rdcMAX_X,      CPLSPrintf("%.8g", dfMaxX));
	CSLSetNameValue(papszRDC, rdcMIN_Y,      CPLSPrintf("%.8g", dfMinY));
	CSLSetNameValue(papszRDC, rdcMAX_Y,      CPLSPrintf("%.8g", dfMaxY));
	CSLSetNameValue(papszRDC, rdcRESOLUTION, CPLSPrintf("%.8g", -dfYPixSz));

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
		const char *pszFName = CPLSPrintf("%s/%s.ref", CPLGetDirname(pszFilename), pszRefSystem);
		char **papszRefFile;

		// ------------------------------------------------------------------
		//  Check if the Projection name refers to the name of a ".ref" file.
		// ------------------------------------------------------------------

		if (FileExists(pszFName) == FALSE)
		{
			pszFName = NULL;
			const char *pszIdrisiDir = CPLGetConfigOption("IDRISIDIR", NULL);
			if ((pszIdrisiDir) != NULL)
			{
				pszFName = CPLSPrintf("%s/georef/%s.ref", pszIdrisiDir, pszRefSystem);
				if (FileExists(pszFName) == FALSE)
					pszFName = NULL;
			}
		}

		// ------------------------------------------------------------------
		//  Reads the reference file
		// ------------------------------------------------------------------

		if (pszFName != NULL)
		{
			papszRefFile = CSLLoad(pszFName);
			CSLSetNameValueSeparator(papszRefFile, ":");
		}
		else
			papszRefFile = NULL;

		OGRSpatialReference oSRS;
		OSRImportFromIdrisi(&oSRS, pszRefSystem, pszRefUnit, papszRefFile);
		oSRS.exportToWkt(&pszProjection);

		CSLDestroy(papszRefFile);
	}

	return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr IdrisiDataset::SetProjection(const char *pszProjString)
{   
	OGRSpatialReference oSRS;

	oSRS.importFromWkt((char **) &pszProjString);

	if (oSRS.IsProjected() && oSRS.GetAttrValue("PROJCS") != NULL)
	{
		// ------------------------------------------------------------------
		//	Check if the input projection comes from another RST file.
		// ------------------------------------------------------------------

		const char *pszRefSystem = NULL;
		const char *pszFName = NULL;

		if (oSRS.GetAuthorityName("PROJCS") != NULL
			&& EQUAL(oSRS.GetAuthorityName("PROJCS"), "IDRISI"))
		{
			pszRefSystem = oSRS.GetAuthorityCode("PROJCS");
			pszFName = CPLSPrintf("%s/%s.ref", CPLGetDirname(pszFilename), pszRefSystem);

			if (FileExists(pszFName) == FALSE)
			{
				pszFName = NULL;
				const char *pszIdrisiDir = CPLGetConfigOption("IDRISIDIR", NULL);
				if ((pszIdrisiDir) != NULL)
				{
					pszFName = CPLSPrintf("%s/georef/%s.ref", pszIdrisiDir, pszRefSystem);
					if (FileExists(pszFName) == FALSE)
						pszFName = NULL;
				}
			}
		}

		if (pszFName != NULL)
		{
			char *pszRefUnit = CPLStrdup(oSRS.GetAttrValue("UNIT", 0));
			if (EQUALN(pszRefUnit, "metre", 5))
			{
				(pszRefUnit)[3] = 'e';
				(pszRefUnit)[4] = 'r';
			}
			CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf("%s", pszRefSystem));
			CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf("%s", pszRefUnit));
			CPLFree(pszRefUnit);
			return CE_None;
		}
	}

	char *pszRefSystem = NULL;
	char *pszRefUnit = NULL;
	char **papszRefFile = NULL;

	OSRExportToIdrisi(&oSRS, &pszRefSystem, &pszRefUnit, &papszRefFile);

	if (papszRefFile != NULL)
	{
		// ------------------------------------------------------------------
		// Save the suggested .ref file in to the documentation sections
		// ------------------------------------------------------------------

		int nLine = -1;
		for (int i = 0; i < CSLCount(papszRDC) && nLine == -1; i++)
			if (EQUALN(papszRDC[i], rdcCOMMENTS, 12))
				nLine = i;
		if (nLine > 0)
		{
			papszRDC = CSLSetNameValue(papszRDC, rdcCOMMENTS, 
				"Suggested reference file. (Delete this line and save as <name>.ref)");
			for (int i = 0; i < CSLCount(papszRefFile); i++)
				papszRDC = CSLInsertString(papszRDC, ++nLine, 
				CPLSPrintf("%-.12s: %s", rdcCOMMENTS, papszRefFile[i]));
		}
	}

	CSLSetNameValue(papszRDC, rdcREF_SYSTEM, CPLSPrintf("%s", pszRefSystem));
	CSLSetNameValue(papszRDC, rdcREF_UNITS,  CPLSPrintf("%s", pszRefUnit));

	CSLDestroy(papszRefFile);
	CPLFree(pszRefSystem);
	CPLFree(pszRefUnit);

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
		CSLSetNameValue(poGDS->papszRDC, rdcFLAG_VALUE, CPLSPrintf("%.8g", dfNoDataValue));
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

	int nLine = -1;
	for (int i = 0; (i < CSLCount(poGDS->papszRDC)) && (nLine == -1); i++)
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

	for (int i = 0; i < nCatCount; i++)
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
			aucRGB[0] = (short) oEntry.c1;
			aucRGB[1] = (short) oEntry.c2;
			aucRGB[2] = (short) oEntry.c3;
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
/*                        OSRImportFromIdrisi()                         */
/************************************************************************/

/**
* \brief Import Idrisi Geo Reference
*
* This functions tries to interpret the "ref. system" information and
* process the well known codifications, like UTM-30N and SPC83MA1, 
* otherwise it tries to process the RefFile file content.
*
* User defined or unsupported codifications points to a ".ref" file
* generally in the software installation folder "/Georef" or in the
* same folder as the ".rst" file. 
*
* @param poSRS the Spatial Reference Object.
* @param pszRefSystem the Idrisi Referense system name
* @param pszRefUnits the Reference system unit
* @param papszRefFile the StringList with the content of a reference file
*/

OGRErr CPL_DLL OSRImportFromIdrisi(OGRSpatialReferenceH hSRS, 
								   const char *pszRefSystem, 
								   const char *pszRefUnits, 
								   char **papszRefFile)
{
	OGRSpatialReference *poSRS = ((OGRSpatialReference *) hSRS);

	char pszRefCode[81];
	int i;

	// ---------------------------------------------------------
	// Lowercase the reference in order make the sscanf to works
 	// ---------------------------------------------------------

	for (i = 0; pszRefSystem[i] != '\0' && i < 80; i++)
		pszRefCode[i] = tolower(pszRefSystem[i]);

	pszRefCode[i] = '\0';

	// ---------------------------------------------------------
	// Check the well known codified reference names
 	// ---------------------------------------------------------

	if (EQUAL(pszRefSystem, rstPLANE))
	{
		return OGRERR_NONE;
	}

	if (EQUAL(pszRefSystem, rstLATLONG))
	{
		poSRS->SetWellKnownGeogCS("WGS84");
		return OGRERR_NONE;
	}

	if (EQUALN(pszRefSystem, rstUTM, 3))
	{
		int	nZone;
		char cNorth;

		sscanf(pszRefCode, rstUTM, &nZone, &cNorth);

		poSRS->SetWellKnownGeogCS("WGS84");
		poSRS->SetUTM(nZone, (cNorth == 'n'));
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
		return OGRERR_NONE;
	}

	if (EQUALN(pszRefSystem, rstSPC, 3))
	{
		int nNAD;
		int nZone;
		char szState[3];

		sscanf(pszRefCode, rstSPC, &nNAD, szState, &nZone);

		int nSPCCode = -1;
		for (unsigned int i = 0; 
                     (i < US_STATE_COUNT) && (nSPCCode == -1); 
                     i++)
		{
			if (EQUAL(szState, aoUSStateTable[i].pszName))
				nSPCCode = aoUSStateTable[i].nCode;
		}
		if (nSPCCode == -1)
			poSRS->SetWellKnownGeogCS(CPLSPrintf("NAD%d", nNAD));
		else
		{
			if (nZone == 1)
				nZone = nSPCCode;
			else
				nZone = nSPCCode + nZone - 1;
			poSRS->SetStatePlane(nZone, (nNAD == 83));
		    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
		}
		return OGRERR_NONE;
	}

	if (papszRefFile == NULL)
	{
		// ------------------------------------------
		// Can't identify just by the reference name
		// Assumming a default WGS84 projection (deg)
		// ------------------------------------------

		if (EQUALN(pszRefUnits, rstDEGREE, 3))
		  poSRS->SetWellKnownGeogCS("WGS84");

		return OGRERR_NONE;
	}

	// ---------------------------------------------------------
	//		Read the ".ref" content
	// ---------------------------------------------------------

	const char *pszProj;
	const char *pszRefSys;
	const char *pszDatum;
	const char *pszEllipsoid;
	double dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing,
		dfSemiMajor, dfSemiMinor;
	double dfStdP1, dfStdP2;

	pszRefSys     	= CSLFetchNameValue(papszRefFile, refREF_SYSTEM);
	pszProj			= CSLFetchNameValue(papszRefFile, refPROJECTION);
	pszDatum		= CSLFetchNameValue(papszRefFile, refDATUM);
	pszEllipsoid	= CSLFetchNameValue(papszRefFile, refELLIPSOID);
	dfCenterLat		= atof_nz(CSLFetchNameValue(papszRefFile, refORIGIN_LAT));
	dfCenterLong	= atof_nz(CSLFetchNameValue(papszRefFile, refORIGIN_LONG));
	dfSemiMajor		= atof_nz(CSLFetchNameValue(papszRefFile, refMAJOR_SAX));
	dfSemiMinor		= atof_nz(CSLFetchNameValue(papszRefFile, refMINOR_SAX));
	dfFalseEasting	= atof_nz(CSLFetchNameValue(papszRefFile, refORIGIN_X));
	dfFalseNorthing	= atof_nz(CSLFetchNameValue(papszRefFile, refORIGIN_Y));
	dfStdP1			= atof_nz(CSLFetchNameValue(papszRefFile, refSTANDL_1));
	dfStdP2			= atof_nz(CSLFetchNameValue(papszRefFile, refSTANDL_2));

	if (EQUAL(CSLFetchNameValue(papszRefFile, refSCALE_FAC), "na"))
		dfScale		= 1.0;
	else
		dfScale		= atof_nz(CSLFetchNameValue(papszRefFile, refSCALE_FAC));

	if (EQUAL(pszProj,"Transverse Mercator"))
	{
		poSRS->SetTM(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "Lambert Conformal Conic"))
	{
		poSRS->SetLCC(dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "Lambert North Polar Azimuthal Equal Area"))
	{
		poSRS->SetLAEA(dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "Lambert South Polar Azimuthal Equal Area"))
	{
		poSRS->SetLAEA(dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "North Polar Stereographic"))
	{
		poSRS->SetPS(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "South Polar Stereographic"))
	{
		poSRS->SetPS(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
		poSRS->SetProjCS(pszProj);	
	}
	else if (EQUAL(pszProj, "Transverse Stereographic"))
	{
		poSRS->SetStereographic(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "Oblique Stereographic"))
	{
		poSRS->SetOS(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "SetSinusoidal"))
	{
		poSRS->SetSinusoidal(dfCenterLong, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else if (EQUAL(pszProj, "Alber''s Equal Area Conic"))
	{
		poSRS->SetACEA(dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}

	//------------------------------------------------------------
	//		Try to match Datum using Approximated String option
	//------------------------------------------------------------

	int nEPSG = atoi_nz(CSVGetField(CSVFilename("gcs.csv"), 
		"COORD_REF_SYS_NAME", 
		pszDatum, 
		CC_ApproxString,
		"COORD_REF_SYS_CODE"));

	if (nEPSG != 0)
	{
		OGRSpatialReference oGCS;
		oGCS.importFromEPSG(nEPSG);
		poSRS->CopyGeogCSFrom(&oGCS);
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}
	else
	{
		//------------------------------------------------------------
		//		No match
		//------------------------------------------------------------

		poSRS->SetGeogCS(
			CPLSPrintf(
			"Unknown datum based upon the ellipsoid %s", pszEllipsoid),
			CPLSPrintf(
			"Not specified (based on the spheroid %s)", pszDatum),
			pszEllipsoid, 
			dfSemiMajor, 
			-1.0 / (dfSemiMinor / dfSemiMajor - 1.0));
	    OSRSetAuthorityLabel(poSRS,"PROJCS", "IDRISI", pszRefCode);
	}

	return OGRERR_NONE;
}

/************************************************************************/
/*                        OSRExportToIdrisi()                           */
/************************************************************************/

/**
* \brief Export Idrisi Geo Reference
* 
* Idrisi store in the RDC documentation file, field "ref. system",
* the name of the reference file (.ref) that contains the geo reference
* paramters. Those file are usually located at the software installation
* folders or in the same folder as the rst file.
*
* @param poSRS the Spatial Reference Object.
* @param pszRefSystem the Idrisi Referense system name
* @param pszRefUnits the Reference system unit
* @param papszRefFile the StringList with the content of a reference file
*/

OGRErr CPL_DLL OSRExportToIdrisi(OGRSpatialReferenceH hSRS, 
								 char **pszRefSystem, 
								 char **pszRefUnit, 
								 char ***papszRefFile)
{
	OGRSpatialReference *poSRS = ((OGRSpatialReference *) hSRS);

	const char  *pszProjection = poSRS->GetAttrValue("PROJECTION");

	CSLDestroy(*papszRefFile);

	if (poSRS->IsLocal())
	{
		*pszRefSystem = CPLStrdup(rstPLANE);
		*pszRefUnit   = CPLStrdup(rstMETER);

		return OGRERR_NONE;
	}

	if (pszProjection == NULL)
	{
#ifdef DEBUG
		CPLDebug("RST", "Empty projection definition, considered as LatLong" );
#endif
		*pszRefSystem = CPLStrdup(rstLATLONG);
		*pszRefUnit   = CPLStrdup(poSRS->GetAttrValue("UNIT", 0));

		return OGRERR_NONE;
	}

	// -----------------------------------------------------
	//      Check for UTM
	// -----------------------------------------------------

	if (EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR))
	{
		int nZone = poSRS->GetUTMZone();

		if ((nZone != 0) && (EQUAL(poSRS->GetAttrValue( "DATUM" ), SRS_DN_WGS84)))
		{
			double dfNorth = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING);
			*pszRefSystem  = CPLStrdup(CPLSPrintf(rstUTM, nZone, (dfNorth == 0.0 ? 'n' : 's')));
			*pszRefUnit    = CPLStrdup(rstMETER);

			return OGRERR_NONE;
		}
	}

	// -----------------------------------------------------
	//      Check for State Plane
	// -----------------------------------------------------

	if ((EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP)) ||
		(EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP)) ||
		(EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR)))
	{
		// -----------------------------------------------------------------------------
		// This technique uses the reverse process of SetStatePlane (ogr_fromEPSG.cpp):
		// Get the ID from [PROJCS[...AUTHORITY["EPSG","26986"]] => 26986
		// Get the PSCode from satateplane.cvs => "2001,Massachusetts,Mainland,,,26986"
		// -----------------------------------------------------------------------------

		char *pszID;
		char *pszPCSCode;

		pszID = CPLStrdup(poSRS->GetAuthorityCode("PROJCS"));
		if (strlen(pszID) > 0)
		{

			pszPCSCode = CPLStrdup(CSVGetField(CSVFilename("stateplane.csv"),
				"EPSG_PCS_CODE", pszID, CC_Integer, "ID"));
			if (strlen(pszPCSCode) > 0)
			{
				int nNADYear	= 83;
				char *pszState	= NULL;
				int nZone		= pszPCSCode[strlen(pszPCSCode) - 1] - '0';
				int nPCSCode	= atoi_nz(pszPCSCode);
				if (nZone == 0)
					nZone = 1;
				if (nPCSCode > 10000)
				{
					nNADYear = 27;
					nPCSCode -= 10000;
				}
				for (unsigned int i = 0; (i < US_STATE_COUNT) && (pszState == NULL); i++)
				{
					if (nPCSCode == aoUSStateTable[i].nCode)
						pszState = aoUSStateTable[i].pszName;
				}
				CPLFree(*pszRefSystem);
				CPLFree(*pszRefUnit);
				*pszRefSystem	= CPLStrdup(CPLSPrintf(rstSPC, nNADYear, pszState, nZone));
				*pszRefUnit		= CPLStrdup(poSRS->GetAttrValue("UNIT", 0));
				if (EQUALN(*pszRefUnit, "metre", 5))
				{
					(*pszRefUnit)[3] = 'e';
					(*pszRefUnit)[4] = 'r';
				}
				CPLFree(pszPCSCode);
				CPLFree(pszID);
				return OGRERR_NONE;
			}
			CPLFree(pszPCSCode);
		}
		CPLFree(pszID);
	}

	// ---------------------------------------------------------
	//		Generate the ".ref" content
	// ---------------------------------------------------------

	double padfCoef[3];
	poSRS->GetTOWGS84(padfCoef, 3);

	if (poSRS->GetAttrValue("PROJCS", 0) == NULL)
		*papszRefFile = CSLAddNameValue(*papszRefFile, refPROJECTION, "none");
	else
		*papszRefFile = CSLAddNameValue(*papszRefFile, refPROJECTION, 
			poSRS->GetAttrValue("PROJCS", 0));

	*papszRefFile = CSLAddNameValue(*papszRefFile, refREF_SYSTEM, 
		poSRS->GetAttrValue("GEOGCS", 0));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refDATUM,      
		poSRS->GetAttrValue("DATUM",  0));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refDELTA_WGS84, 
		CPLSPrintf("%.2g %.2g %.2g", padfCoef[0], padfCoef[1], padfCoef[2]));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refELLIPSOID,  
		poSRS->GetAttrValue("SPHEROID", 0));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refMAJOR_SAX,  
		CPLSPrintf("%.8g", poSRS->GetSemiMajor(NULL)));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refMINOR_SAX,  
		CPLSPrintf("%.8g", poSRS->GetSemiMinor(NULL)));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refORIGIN_LAT, 
		CPLSPrintf("%.8g", poSRS->GetProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0, NULL)));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refORIGIN_LONG,
		CPLSPrintf("%.8g", poSRS->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0, NULL)));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refORIGIN_X, 
		CPLSPrintf("%.8g", poSRS->GetProjParm(SRS_PP_FALSE_EASTING, 0.0, NULL)));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refORIGIN_Y, 
		CPLSPrintf("%.8g", poSRS->GetProjParm(SRS_PP_FALSE_NORTHING, 0.0, NULL)));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refSCALE_FAC, 
		CPLSPrintf("%.8g", poSRS->GetProjParm(SRS_PP_SCALE_FACTOR, 0.0, NULL)));
	*papszRefFile = CSLAddNameValue(*papszRefFile, refUNITS, 
		poSRS->GetAttrValue("UNIT", 0));

	double dfStdP1 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0, NULL);
	double dfStdP2 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0, NULL);

	if ((dfStdP1 == 0.0) && (dfStdP2 == 0.0))
		*papszRefFile = CSLAddNameValue(*papszRefFile, refPARAMETERS, "0");
	else
	{
		*papszRefFile = CSLAddNameValue(*papszRefFile, refPARAMETERS, "2");
		*papszRefFile = CSLAddNameValue(*papszRefFile, refSTANDL_1,   CPLSPrintf("%.8g", dfStdP1));
		*papszRefFile = CSLAddNameValue(*papszRefFile, refSTANDL_2,   CPLSPrintf("%.8g", dfStdP1));
	}
	CSLSetNameValueSeparator(*papszRefFile, ": ");

	// ------------------------------------------------------------------
	// Assumes LATLONG only when UNIT is degree, and all else are Unknown
	// ------------------------------------------------------------------

	const char* szUnit = poSRS->GetAttrValue("UNIT", 0);

	if (szUnit && EQUAL(szUnit, rstDEGREE))
	{
		*pszRefSystem = CPLStrdup(rstLATLONG);
		*pszRefUnit   = CPLStrdup(szUnit);
	}
	else
	{
		*pszRefSystem = CPLStrdup(rstPLANE);
		*pszRefUnit   = CPLStrdup(rstMETER);
	}

	return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetAuthorityLabel()                      */
/************************************************************************/

/**
 * Set the authority string label for a node.
 *
 * @param pszTargetKey the partial or complete path to the node to 
 * set an authority on.  ie. "PROJCS", "GEOGCS" or "GEOGCS|UNIT".
 *
 * @param pszAuthority authority name, such as "IDRISI".
 * @param pszLabel string for value with this authority, such as "utm-30n".
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OSRSetAuthorityLabel(OGRSpatialReferenceH hSRS, 
							const char *pszTargetKey,
							const char *pszAuthority, 
							const char *pszLabel)
{
	OGRSpatialReference *poSRS = ((OGRSpatialReference *) hSRS);

/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    OGR_SRSNode  *poNode = poSRS->GetAttrNode( pszTargetKey );

    if( poNode == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      If there is an existing AUTHORITY child blow it away before     */
/*      trying to set a new one.                                        */
/* -------------------------------------------------------------------- */
    int iOldChild = poNode->FindChild( "AUTHORITY" );
    if( iOldChild != -1 )
        poNode->DestroyChild( iOldChild );

/* -------------------------------------------------------------------- */
/*      Create a new authority label.                                   */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poAuthNode;

    poAuthNode = new OGR_SRSNode( "AUTHORITY" );
    poAuthNode->AddChild( new OGR_SRSNode( pszAuthority ) );
    poAuthNode->AddChild( new OGR_SRSNode( pszLabel ) );
    
    poNode->AddChild( poAuthNode );

    return OGRERR_NONE;
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
