/*****************************************************************************
*
* Project:  Idrisi Raster Image File Driver
* Purpose:  Read/write Idrisi Raster Image Format RST
* Author:   Ivan Lucena, ilucena@clarku.edu
*
******************************************************************************
* Copyright (c) 2005, Clark Labs
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

typedef	unsigned char uint8;	
typedef	signed short  int16;		
typedef float         float32;			

typedef uint8   idrisi_byte;
typedef int16   idrisi_integer;
typedef float32 idrisi_real;

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_C_START
#include "IdrisiRasterDoc.h"
#include "IdrisiRasterProj.h"
CPL_C_END

CPL_C_START
void	GDALRegister_IDRISI(void);
CPL_C_END

class IdrisiDataset;
class IdrisiRasterBand;

class IdrisiDataset : public GDALDataset
{
	friend class IdrisiRasterBand;

	FILE *fp;
	rst_Doc *imgDoc;
	char *pszFilename;
	char *pszGeoRef;
	double adfGeoTransform[6];

public:
	IdrisiDataset();
	~IdrisiDataset();

	static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
	static GDALDataset *Create(const char *pszFilename,
		int nXSize, 
		int nYSize, 
		int nBands, 
		GDALDataType eType,
		char** papszParmList); 

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

public:
	IdrisiRasterBand(IdrisiDataset *poDS, int nChannel);
	~IdrisiRasterBand();

	virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage);
	virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void *pImage);
	virtual GDALColorTable *GetColorTable();
	virtual GDALColorInterp GetColorInterpretation();
	virtual CPLErr SetColorTable(GDALColorTable *SetColorTable); 
	virtual CPLErr SetColorInterpretation(GDALColorInterp);
	virtual char **GetCategoryNames();
	virtual CPLErr SetCategoryNames(char **);
};

//  ----------------------------------------------------------------------------
//  //  //
//	//			Implementation of IdrisiDataset
//
//  ----------------------------------------------------------------------------

IdrisiDataset::IdrisiDataset()
{
	pszFilename = NULL;
	fp = NULL;
	pszGeoRef = NULL;
	imgDoc = NULL;

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
	if (imgDoc != NULL)
		FreeImgDoc(imgDoc);
	CPLFree(pszGeoRef);
	CPLFree(pszFilename);
}

GDALDataset *IdrisiDataset::Open(GDALOpenInfo * poOpenInfo)
{
	CPLDebug("RST", "Begin: IdrisiDataset::Open");

	if (poOpenInfo->fp == NULL)
		return NULL;

	// -------------------------------------------------------------------- 
	//      Check the documentation file .rdc
	// -------------------------------------------------------------------- 

	rst_Doc *imgDoc;

	imgDoc = ReadImgDoc(poOpenInfo->pszFilename);

	if (imgDoc == NULL)
		return NULL;

	if (! EQUAL(imgDoc->file_format, "IDRISI Raster A.1"))
	{
		CPLFree(imgDoc);
		return NULL;
	}

	CPLDebug("RST", "OK: Check the documentation file .rdc");

	// -------------------------------------------------------------------- 
	//      Create a corresponding GDALDataset
	// -------------------------------------------------------------------- 

	IdrisiDataset *poDS;

	poDS = new IdrisiDataset();
    poDS->eAccess = poOpenInfo->eAccess;

    if (poOpenInfo->eAccess == GA_ReadOnly )
	{
		poDS->fp = poOpenInfo->fp;
 		poOpenInfo->fp = NULL;
	}
	else
	{
		poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "r+b");
 		poOpenInfo->fp = NULL;
	}

	if (! poDS->fp)
        return NULL;
	
	CPLDebug("RST", "OK: Create a corresponding GDALDataset");

	// -------------------------------------------------------------------- 
	//      Load information from documentation
	// -------------------------------------------------------------------- 

	poDS->imgDoc = imgDoc;
	poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);

	poDS->nRasterXSize = poDS->imgDoc->columns;
	poDS->nRasterYSize = poDS->imgDoc->rows;
	poDS->eAccess = poOpenInfo->eAccess;

	CPLDebug("RST", "OK: Load information from documentation");

	// -------------------------------------------------------------------- 
	//      Create band information
	// -------------------------------------------------------------------- 

	switch (poDS->imgDoc->data_type)
	{
	case RST_DT_BYTE:
		poDS->nBands = 1;
		poDS->SetBand(1, new IdrisiRasterBand(poDS, 1));
		break;
	case RST_DT_INTEGER:			
		poDS->nBands = 1;
		poDS->SetBand(1, new IdrisiRasterBand(poDS, 1));
		break;
	case RST_DT_REAL:				
		poDS->nBands = 1;
		poDS->SetBand(1, new IdrisiRasterBand(poDS, 1));
		break;
	case RST_DT_RGB24:			
		poDS->nBands = 3;
		poDS->SetBand(1, new IdrisiRasterBand(poDS, 1));
		poDS->SetBand(2, new IdrisiRasterBand(poDS, 2));
		poDS->SetBand(3, new IdrisiRasterBand(poDS, 3));
	};

	CPLDebug("RST", "OK: Create band information");

	// -------------------------------------------------------------------- 
	//      Load the transformation matrix
	// -------------------------------------------------------------------- 

	poDS->adfGeoTransform[0] = (double) imgDoc->min_X;
	poDS->adfGeoTransform[1] = (imgDoc->max_X - imgDoc->min_X) / (imgDoc->unit_dist * imgDoc->columns);
	poDS->adfGeoTransform[2] = 0.0;
	poDS->adfGeoTransform[3] = (double) imgDoc->max_Y;
	poDS->adfGeoTransform[4] = 0.0;
	poDS->adfGeoTransform[5] = (imgDoc->min_Y - imgDoc->max_Y) / (imgDoc->unit_dist * imgDoc->rows);

	CPLDebug("RST", "OK: Load the transformation matrix");

	// -------------------------------------------------------------------- 
	//      Load Geographic Reference
	// -------------------------------------------------------------------- 

	poDS->pszGeoRef = CPLStrdup(ReadProjSystem(poDS->pszFilename));

	CPLDebug("RST", "OK: Load Geographic Reference");

	CPLDebug("RST", "End: IdrisiDataset::Open");

	return (poDS);
}

GDALDataset *IdrisiDataset::Create(const char *pszFilename,
								   int nXSize, 
								   int nYSize, 
								   int nBands, 
								   GDALDataType eType,
								   char** papszOptions)
{
	CPLDebug("RST", "Begin: IdrisiDataset.Create");

	// -------------------------------------------------------------------- 
	//      Check input options
	// -------------------------------------------------------------------- 

	if (eType != GDT_Byte && 
		eType != GDT_Int16 && 
		eType != GDT_Float32)
	{
		CPLError( CE_Failure, CPLE_AppDefined,
			"Attempt to create IDRISI dataset with an illegal\n"
			"data type (%s).\n",
			GDALGetDataTypeName(eType) );
		return NULL;
	}

	if (nBands != 1) 
		if (! (nBands == 3 && eType == GDT_Byte))
		{
			CPLError( CE_Failure, CPLE_AppDefined,
				"Attempt to create IDRISI dataset with an illegal\n"
				"number of bands (%d) to data type (%s).\n",
				nBands, GDALGetDataTypeName(eType) );
			return NULL;
		}

	CPLDebug("RST", "OK: Check input options");

	// ---------------------------------------------------------------- 
	//  Create the header file
	// ---------------------------------------------------------------- 

	rst_Doc *imgDoc;

	imgDoc = CreateImgDoc();
	imgDoc->columns = nXSize;
	imgDoc->rows = nYSize;
	switch (eType )
	{	
	case GDT_Byte:
		imgDoc->data_type = (nBands == 3 ? RST_DT_RGB24 : RST_DT_BYTE);
		break;
	case GDT_Int16:			
		imgDoc->data_type = RST_DT_INTEGER;
		break;
	case GDT_Float32:				
		imgDoc->data_type = RST_DT_REAL;
		break;
	}
	WriteImgDoc(imgDoc, (char *) pszFilename);

	CPLFree(imgDoc);

	CPLDebug("RST", "OK: Create the header file");

	// ---------------------------------------------------------------- 
	//  Create an empty data file
	// ---------------------------------------------------------------- 

	FILE        *fp;

    fp = VSIFOpenL(pszFilename, "wb");

    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create file `%s' failed.\n", pszFilename);
        return NULL;
    }
    VSIFCloseL(fp);

	CPLDebug("RST", "OK: Create an empty data file");

	CPLDebug("RST", "End: IdrisiDataset.Create");

	return (GDALDataset *) GDALOpen(pszFilename, GA_Update);
}

CPLErr  IdrisiDataset::GetGeoTransform(double * padfTransform)
{
	memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6 );
	return CE_None;
}

CPLErr  IdrisiDataset::SetGeoTransform(double * padfTransform)
{
	CPLDebug("RST", "Begin: IdrisiDataset::SetGeoTransform");

	if (padfTransform[2] != 0.0 || padfTransform[4] != 0.0)
	{
		CPLError( CE_Failure, CPLE_AppDefined,
			"Attempt to set rotated geotransform on Idrisi Raster file.\n"
			"Idrisi Raster does not support rotation.\n" );

		return CE_Failure;
	}

	imgDoc->min_X = padfTransform[0];
	imgDoc->max_X = (padfTransform[1] * imgDoc->columns) + padfTransform[0];

	imgDoc->min_Y = padfTransform[3] - (-padfTransform[5] * imgDoc->rows);
	imgDoc->max_Y = padfTransform[3];

	imgDoc->resolution = -padfTransform[5];
	imgDoc->unit_dist = 1.0;

	WriteImgDoc(imgDoc, (char *) pszFilename);

	CPLDebug("RST", "End: IdrisiDataset::SetGeoTransform");

	return CE_None;
}

const char *IdrisiDataset::GetProjectionRef(void)
{
	return pszGeoRef;
}

CPLErr IdrisiDataset::SetProjection(const char * pszProjString)
{
	char peString[MAXPESTRING];

	strcpy(peString, pszProjString);

	//	WriteProjSystem(peString, (char *) pszFilename);

	return CE_None;
}

//  ----------------------------------------------------------------------------
//  //  //
//	//			Implementation of IdrisiRasterBand
//
//  ----------------------------------------------------------------------------

IdrisiRasterBand::IdrisiRasterBand(IdrisiDataset *poDS, int nChannel)
{
	this->poDS = poDS;
	this->nBand = nBand;

    poColorTable = NULL;

	// -------------------------------------------------------------------- 
	//      Set Data Type
	// -------------------------------------------------------------------- 

	switch (poDS->imgDoc->data_type)
	{
	case RST_DT_BYTE:
		eDataType = GDT_Byte;
		break;
	case RST_DT_INTEGER:			
		eDataType = GDT_Int16;
		break;
	case RST_DT_REAL:				
		eDataType = GDT_Float32;
		break;
	case RST_DT_RGB24:			
		eDataType = GDT_Byte;
		break;
	};

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

	if ((poDS->imgDoc->data_type == RST_DT_BYTE) && (poDS->imgDoc->legend_cats != 0))
	{
		poColorTable = new GDALColorTable();
		double colorTable[3][256];
		ReadPalette(poDS->pszFilename, 0, colorTable[0], 256, TRUE);
		ReadPalette(poDS->pszFilename, 1, colorTable[1], 256, TRUE);
		ReadPalette(poDS->pszFilename, 2, colorTable[2], 256, TRUE);
		GDALColorEntry oEntry;
		for (int i = 0; i < 256; i++)
		{
			oEntry.c1 = (short) (255 * colorTable[0][i]);
			oEntry.c2 = (short) (255 * colorTable[1][i]);
			oEntry.c3 = (short) (255 * colorTable[2][i]);
			oEntry.c4 = 255;                      
			poColorTable->SetColorEntry(i, &oEntry);
		}
	}

}

IdrisiRasterBand::~IdrisiRasterBand()
{
    if( poColorTable != NULL )
        delete poColorTable;

	CPLFree(pabyScan);
}

CPLErr IdrisiRasterBand::IReadBlock(int nBlockXOff, 
									int nBlockYOff,
									void *pImage)
{
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);

	VSIFReadL(pabyScan, 1, nRecordSize, poGDS->fp);

	if (poGDS->nBands == 3) 
	{
		for (int i = 0; i < nBlockXSize; i++)
		{
			((uint8 *) pImage)[i] = (uint8) pabyScan[(i * 3) + (3 - this->nBand)];
		}
	}
	else
	{
		memcpy(pImage, pabyScan, nRecordSize);
	}

	return CE_None;
}

CPLErr IdrisiRasterBand::IWriteBlock(int nBlockXOff, 
									int nBlockYOff,
									void *pImage)
{
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	VSIFSeekL(poGDS->fp, nRecordSize * nBlockYOff, SEEK_SET);

	if (poGDS->nBands == 3) 
	{
		for (int i = 0; i < nBlockXSize; i++)
		{
			(uint8) pabyScan[(i * 3) + (3 - this->nBand)] = ((uint8 *) pImage)[i];
		}
	}
	else
	{
		memcpy(pabyScan, pImage, nRecordSize);
	}

	if (VSIFWriteL(pabyScan, 1, nRecordSize, poGDS->fp) < nRecordSize)
    {
        CPLError(CE_Failure, CPLE_FileIO,
			"Can't write (%s) block with X offset %d and Y offset %d.\n%s", 
			poGDS->pszFilename, nBlockXOff, nBlockYOff,
            VSIStrerror(errno));
        return CE_Failure;
    }

	return CE_None;
}
CPLErr IdrisiRasterBand::SetColorInterpretation(GDALColorInterp)
{
	return CE_None;
}

GDALColorTable *IdrisiRasterBand::GetColorTable()
{
    return poColorTable;
}

GDALColorInterp IdrisiRasterBand::GetColorInterpretation()
{
	IdrisiDataset *poPDS = (IdrisiDataset *) poDS;

	switch (poPDS->imgDoc->data_type)
	{
	case RST_DT_BYTE:
		return (poPDS->imgDoc->legend_cats == 0 ? GCI_GrayIndex : GCI_PaletteIndex);
	case RST_DT_INTEGER:			
		return GCI_GrayIndex;
	case RST_DT_RGB24:
		switch (nBand)
		{
		case 1: return GCI_BlueBand;
		case 2: return GCI_GreenBand;
		case 3: return GCI_RedBand;
		};
	case RST_DT_REAL:				
		return GCI_GrayIndex;
	};
	return GCI_Undefined;
}

CPLErr IdrisiRasterBand::SetColorTable(GDALColorTable *aColorTable)
{
	/*
	IdrisiDataset *poPDS = (IdrisiDataset *) poDS;
	GDALColorEntry oEntry;
	double colorTable[3][256];

	poColorTable->GetColorEntryAsRGB(i, oEntry);

		ReadPalette(poDS->pszFilename, 0, colorTable[0], 256, TRUE);
		ReadPalette(poDS->pszFilename, 1, colorTable[1], 256, TRUE);
		ReadPalette(poDS->pszFilename, 2, colorTable[2], 256, TRUE);
		GDALColorEntry oEntry;
		for (int i = 0; i < 256; i++)
		{
			oEntry.c1 = (short) (255 * colorTable[0][i]);
			oEntry.c2 = (short) (255 * colorTable[1][i]);
			oEntry.c3 = (short) (255 * colorTable[2][i]);
			oEntry.c4 = 255;                      
			poColorTable->SetColorEntry(i, &oEntry);
		}
	}


			ReadPalette(poDS->pszFilename, 1, colorTable[1], 256, TRUE);

	for (int i = 0; i < 256; i++)
	{
		colorTable[i] = aColorTable[i] / 255;
	}

	WritePalette(poDS->pszFilename, (this->nBand - 1), colorTable, 255);
*/
	return CE_None;
}

char **IdrisiRasterBand::GetCategoryNames()
{
	IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

	if (poGDS->imgDoc->legend_cats == 0) 
		return (char **) NULL;

	CPLDebug("RST", "Begin: GetCategoryNames");

	char **papszCategoryNames;
	unsigned int i, j;

	papszCategoryNames = (char **) CPLMalloc(poGDS->imgDoc->legend_cats + 1);

	for (i = 0, j = 0; i < poGDS->imgDoc->legend_cats; i++)
	{
		if (i == poGDS->imgDoc->codes[j]) 
		{
			papszCategoryNames[i] = CPLStrdup(poGDS->imgDoc->categories[j]);
			j++;
		}
		else
			papszCategoryNames[i] = CPLStrdup("");
	}

	papszCategoryNames[poGDS->imgDoc->legend_cats] = NULL;

	CPLDebug("RST", "End: GetCategoryNames");

	return (char **) papszCategoryNames;
}

CPLErr IdrisiRasterBand::SetCategoryNames(char **)
{
	return CE_None;
}

void GDALRegister_IDRISI()
{
	GDALDriver  *poDriver;

	if (GDALGetDriverByName("IDRISI") == NULL)
	{
		poDriver = new GDALDriver();

		poDriver->SetDescription("IDRISI");
		poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Idrisi Raster (.rst)");
		poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_various.html#IDRISI");
		poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "rst");
		poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int16 Float32");
		poDriver->pfnOpen = IdrisiDataset::Open;
		poDriver->pfnCreate	= IdrisiDataset::Create;

		GetGDALDriverManager()->RegisterDriver(poDriver);
	}
}
