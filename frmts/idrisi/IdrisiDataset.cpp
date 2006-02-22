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

typedef	short int16;
typedef float float32;

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_C_START
#include "IdrisiRasterDoc.h"
#include "IdrisiRasterProj.h"
CPL_C_END

CPL_C_START
void	GDALRegister_IDRISI(void);
CPL_C_END

class IdrisiDataset;
class IdririRasterBand;

class IdrisiDataset : public RawDataset
{
	friend class IdrisiDatasetBand;

	FILE *fpImage;
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
	virtual char **GetCategoryNames();
	virtual CPLErr SetCategoryNames(char **);
};

class IdrisiDatasetBand : public RawRasterBand
{
	friend class IdrisiDataset;

public:
		IdrisiDatasetBand(GDALDataset *poDS, 
			int nBand,
			FILE * fpRaw, 
			vsi_l_offset nImgOffset,
			int nPixelOffset, 
			int nLineOffset,
			GDALDataType eDataType);
		~IdrisiDatasetBand();

	virtual GDALColorTable *GetColorTable();
	virtual GDALColorInterp GetColorInterpretation();
	virtual CPLErr SetColorTable(GDALColorTable *SetColorTable); 
	virtual CPLErr SetColorInterpretation(GDALColorInterp);
};

IdrisiDataset::IdrisiDataset()
{
	pszFilename = NULL;
	fpImage = NULL;
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
printf("\n*** IdrisiDataset::Open 1\n");
	if (poOpenInfo->fp == NULL)
		return NULL;

	// -------------------------------------------------------------------- 
	//      Check the documentation file .rdc
	// -------------------------------------------------------------------- 

	rst_Doc *imgDoc;

	imgDoc = ReadImgDoc(poOpenInfo->pszFilename);

	if (imgDoc == NULL)
		return NULL;

	if (EQUAL(imgDoc->file_format, "IDRISI Raster A.1") == FALSE)
	{
		CPLFree(imgDoc);
		return NULL;
	}

printf("\n*** IdrisiDataset::Open 2\n");

	// -------------------------------------------------------------------- 
	//      Create a corresponding GDALDataset
	// -------------------------------------------------------------------- 

	IdrisiDataset *poDS;

	poDS = new IdrisiDataset();

	poDS->fpImage = poOpenInfo->fp;
	poOpenInfo->fp = NULL;
	
printf("\n*** IdrisiDataset::Open 3\n");

	// -------------------------------------------------------------------- 
	//      Load information from documentation
	// -------------------------------------------------------------------- 

	poDS->imgDoc = imgDoc;
	poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);

	poDS->nRasterXSize = poDS->imgDoc->columns;
	poDS->nRasterYSize = poDS->imgDoc->rows;
	poDS->eAccess = poOpenInfo->eAccess;

	// -------------------------------------------------------------------- 
	//      Create band information
	// -------------------------------------------------------------------- 

printf("\n*** IdrisiDataset::Open 4\n");

	switch (poDS->imgDoc->data_type)
	{
	case RST_DT_BYTE:
		poDS->nBands = 1;
		poDS->SetBand(1, new IdrisiDatasetBand(poDS, 1, poDS->fpImage, 0, 1, poDS->nRasterXSize, GDT_Byte));
		break;
	case RST_DT_INTEGER:			
		poDS->nBands = 1;
		poDS->SetBand(1, new IdrisiDatasetBand(poDS, 1, poDS->fpImage, 0, 1, poDS->nRasterXSize * sizeof(int16), GDT_Int16));
		break;
	case RST_DT_REAL:				
		poDS->nBands = 1;
		poDS->SetBand(1, new IdrisiDatasetBand(poDS, 1, poDS->fpImage, 0, 1, poDS->nRasterXSize * sizeof(float32), GDT_Float32));
		break;
	case RST_DT_RGB24:			
		poDS->nBands = 3;
		poDS->SetBand(1, new IdrisiDatasetBand(poDS, 1, poDS->fpImage, 0, 3, poDS->nRasterXSize * 3, GDT_Byte));
		poDS->SetBand(2, new IdrisiDatasetBand(poDS, 2, poDS->fpImage, 1, 3, poDS->nRasterXSize * 3, GDT_Byte));
		poDS->SetBand(3, new IdrisiDatasetBand(poDS, 3, poDS->fpImage, 3, 3, poDS->nRasterXSize * 3, GDT_Byte));
	};

	// -------------------------------------------------------------------- 
	//      Load the transformation matrix
	// -------------------------------------------------------------------- 

printf("\n*** IdrisiDataset::Open 5\n");

	poDS->adfGeoTransform[0] = (double) imgDoc->min_X;
	poDS->adfGeoTransform[1] = (imgDoc->max_X - imgDoc->min_X) / (imgDoc->unit_dist * imgDoc->columns);
	poDS->adfGeoTransform[2] = 0.0;
	poDS->adfGeoTransform[3] = (double) imgDoc->max_Y;
	poDS->adfGeoTransform[4] = 0.0;
	poDS->adfGeoTransform[5] = (imgDoc->min_Y - imgDoc->max_Y) / (imgDoc->unit_dist * imgDoc->rows);

	// -------------------------------------------------------------------- 
	//      Load Geographic Reference
	// -------------------------------------------------------------------- 
printf("\n*** IdrisiDataset::Open 6\n");

	poDS->pszGeoRef = CPLStrdup(ReadProjSystem(poDS->pszFilename));

printf("\n*** IdrisiDataset::Open End \n");

	return (poDS);
}

GDALDataset *IdrisiDataset::Create(const char *pszFilename,
								   int nXSize, 
								   int nYSize, 
								   int nBands, 
								   GDALDataType eType,
								   char** papszOptions)
{
printf("\n*** IdrisiDataset::Create \n");

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

printf("\n*** IdrisiDataset::Create End \n");

	return (GDALDataset *) GDALOpen(pszFilename, GA_Update);
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

CPLErr IdrisiDatasetBand::SetColorInterpretation(GDALColorInterp)
{
	return CE_None;
}

char **IdrisiDataset::GetCategoryNames()
{
	printf("\n*** GetCategoryNames Begin ***\n");

	return (char **) NULL;
}

CPLErr IdrisiDataset::SetCategoryNames(char **)
{
	printf("\n*** SetCategoryNames Begin ***\n");

	return CE_None;
}

IdrisiDatasetBand::IdrisiDatasetBand(GDALDataset *poDS, 
									 int nBand,
									 FILE * fpRaw, 
									 vsi_l_offset nImgOffset,                   
									 int nPixelOffset, 
									 int nLineOffset,
									 GDALDataType eDataType) : 
RawRasterBand(poDS, 
			  nBand, 
			  fpRaw, 
			  nImgOffset, 
			  nPixelOffset,
			  nLineOffset,
			  eDataType, 
			  TRUE)
{
}

IdrisiDatasetBand::~IdrisiDatasetBand()
{
}

GDALColorTable *IdrisiDatasetBand::GetColorTable()
{
	IdrisiDataset *poPDS = (IdrisiDataset *) poDS;

	return (GDALColorTable *) NULL;
	/*
	double colorTableRed[256];
	double colorTableGreen[256];
	double colorTableBlue[256];

	ReadPalette(poDS->pszFilename, 0, colorTableRed, 255, FALSE);
	ReadPalette(poDS->pszFilename, 1, colorTableGreen, 255, FALSE);
	ReadPalette(poDS->pszFilename, 2, colorTableBlue, 255, FALSE);

	GDALColorEntry oEntry;

	for (int i = 0; i < 255; i++)
	{
	oEntry.c1 = (short) colorTableRed[i];
	oEntry.c2 = (short) colorTableGreen[i];
	oEntry.c3 = (short) colorTableBlue[i];
	oEntry.c4 = 255;                      
	poDS->poColorTable->SetColorEntry(i, &oEntry);
	}
	*/
}

GDALColorInterp IdrisiDatasetBand::GetColorInterpretation()
{
	IdrisiDataset *poPDS = (IdrisiDataset *) poDS;

	switch (poPDS->imgDoc->data_type)
	{
	case RST_DT_BYTE:
		if (poPDS->imgDoc->legend_cats == 0)
			return GCI_GrayIndex;
		else
			return GCI_PaletteIndex;
	case RST_DT_INTEGER:			
		return GCI_GrayIndex;
	case RST_DT_REAL:				
		return GCI_GrayIndex;
	case RST_DT_RGB24:
		if (nBand == 1)
			return GCI_RedBand;
		else if (nBand == 2)
			return GCI_GreenBand;
		else if (nBand == 3)
			return GCI_BlueBand;
		else 
			return GCI_AlphaBand;
	};
	return GCI_Undefined;
}

CPLErr IdrisiDatasetBand::SetColorTable(GDALColorTable *aColorTable)
{
	IdrisiDataset *poPDS = (IdrisiDataset *) poDS;

	return CE_None;
}

void GDALRegister_IDRISI()
{
	GDALDriver  *poDriver;

	if( GDALGetDriverByName("IDRISI") == NULL )
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
