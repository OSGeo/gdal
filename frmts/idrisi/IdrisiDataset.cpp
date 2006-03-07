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

typedef	unsigned char	uint8;	
typedef	signed short	int16;		
typedef	unsigned short	uint16;
typedef float			float32;			
typedef long			int32;
typedef unsigned long	uint32;

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
    rst_Doc *poImgDoc;
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
    virtual CPLErr SetColorInterpretation(GDALColorInterp);
    virtual CPLErr SetScale(double);
    virtual CPLErr SetUnitType(const char *);
    virtual CPLErr SetStatistics(double dfMin, double dfMax, 
        double dfMean, double dfStdDev);

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
    poImgDoc = NULL;

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

    if (poImgDoc != NULL)
    {
        WriteImgDoc(poImgDoc, pszFilename);
        FreeImgDoc(poImgDoc);
    }

    CPLFree(pszGeoRef);
    CPLFree(pszFilename);
}

GDALDataset *IdrisiDataset::Open(GDALOpenInfo * poOpenInfo)
{
    if (poOpenInfo->fp == NULL)
        return NULL;

    // -------------------------------------------------------------------- 
    //      Check the documentation file .rdc
    // -------------------------------------------------------------------- 

    rst_Doc *poImgDoc;

    poImgDoc = ReadImgDoc(poOpenInfo->pszFilename);

    if (poImgDoc == NULL)
        return NULL;

    if (! EQUAL(poImgDoc->file_format, "IDRISI Raster A.1"))
    {
        CPLFree(poImgDoc);
        return NULL;
    }

    // -------------------------------------------------------------------- 
    //      Create a corresponding GDALDataset
    // -------------------------------------------------------------------- 

    IdrisiDataset *poDS;

    poDS = new IdrisiDataset();
    poDS->eAccess = poOpenInfo->eAccess;

    if (poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
        poOpenInfo->fp = NULL;
    } 
    else 
    {
        poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "r+b");
        poOpenInfo->fp = NULL;
    }

    if (! poDS->fp)
        return NULL;

    // -------------------------------------------------------------------- 
    //      Load information from documentation
    // -------------------------------------------------------------------- 

    poDS->poImgDoc = poImgDoc;
    poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);

    poDS->nRasterXSize = poDS->poImgDoc->columns;
    poDS->nRasterYSize = poDS->poImgDoc->rows;
    poDS->eAccess = poOpenInfo->eAccess;

    // -------------------------------------------------------------------- 
    //      Create band information
    // -------------------------------------------------------------------- 

    switch (poDS->poImgDoc->data_type)
    {
    case RST_DT_BYTE:
        poDS->nBands = 1;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Byte));
        break;
    case RST_DT_INTEGER:			
        poDS->nBands = 1;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Int16));
        break;
    case RST_DT_REAL:				
        poDS->nBands = 1;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Float32));
        break;
    case RST_DT_RGB24:			
        poDS->nBands = 3;
        poDS->SetBand(1, new IdrisiRasterBand(poDS, 1, GDT_Byte));
        poDS->SetBand(2, new IdrisiRasterBand(poDS, 2, GDT_Byte));
        poDS->SetBand(3, new IdrisiRasterBand(poDS, 3, GDT_Byte));
    };

    // -------------------------------------------------------------------- 
    //      Load the transformation matrix
    // -------------------------------------------------------------------- 

    poDS->adfGeoTransform[0] = (double) poImgDoc->min_X;
    poDS->adfGeoTransform[1] = (poImgDoc->max_X - poImgDoc->min_X) / (poImgDoc->unit_dist * poImgDoc->columns);
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = (double) poImgDoc->max_Y;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = (poImgDoc->min_Y - poImgDoc->max_Y) / (poImgDoc->unit_dist * poImgDoc->rows);

    // -------------------------------------------------------------------- 
    //      Load Geographic Reference
    // -------------------------------------------------------------------- 

    poDS->pszGeoRef = CPLStrdup(ReadProjSystem(poDS->pszFilename));

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

    rst_Doc *poImgDoc;

    poImgDoc = CreateImgDoc();
    poImgDoc->columns = nXSize;
    poImgDoc->rows = nYSize;

    switch (eType)
    {	
    case GDT_Byte:
        poImgDoc->data_type = (nBands == 3 ? RST_DT_RGB24 : RST_DT_BYTE);
        break;
    case GDT_Int16:			
        poImgDoc->data_type = RST_DT_INTEGER;
        break;
    case GDT_Float32:				
        poImgDoc->data_type = RST_DT_REAL;
        break;
    }

    WriteImgDoc(poImgDoc, (char *) pszFilename);
    FreeImgDoc(poImgDoc);

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
        CPLDebug("RST", "Keep the same data type %s", GDALGetDataTypeName(eType));
    }
    else
    {
        if (eType == GDT_Float64)
        {
            eType = GDT_Float32;
            CPLDebug("RST", "Change (1) from %s to %s", 
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
                CPLDebug("RST", "Change (2) from %s to %s", 
                    GDALGetDataTypeName(poBand->GetRasterDataType()),
                    GDALGetDataTypeName(eType)
                    );
            }
            else
            {
                eType = GDT_Int16;
                CPLDebug("RST", "Change (3) from %s to %s", 
                    GDALGetDataTypeName(poBand->GetRasterDataType()),
                    GDALGetDataTypeName(eType)
                    );
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

    for (int i = 1; i <= poDS->nBands; i++)
    {
        poSrcBand = poSrcDS->GetRasterBand(i);
        poBand = poDS->GetRasterBand(i);

        CPLDebug("RST", "CreateCopy:Creating Band %d", i);

        poBand->SetCategoryNames(poSrcBand->GetCategoryNames());
        poBand->SetColorInterpretation(poSrcBand->GetColorInterpretation());
        poBand->SetColorTable(poSrcBand->GetColorTable());
        poBand->SetStatistics(poSrcBand->GetMinimum(), poSrcBand->GetMaximum(), 0.0, 0.0);
        poBand->SetNoDataValue(poSrcBand->GetNoDataValue(NULL));
    }

    // --------------------------------------------------------------------
    //      Add comments
    // --------------------------------------------------------------------

    strcpy(poDS->poImgDoc->file_title, "Generated by GDAL");

    poDS->poImgDoc->comments = (char **) CPLMalloc(CSLCount(papszOptions));
    poDS->poImgDoc->comments_count = CSLCount(papszOptions);

    for (int i = 0; i < CSLCount(papszOptions); i++)
    {
        poDS->poImgDoc->comments[i] = CPLStrdup(papszOptions[i]);
        CPLDebug("RST", "C: %s", poDS->poImgDoc->comments[i]);
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

    poImgDoc->min_X = padfTransform[0];
    poImgDoc->max_X = (padfTransform[1] * poImgDoc->columns) + padfTransform[0];

    poImgDoc->min_Y = padfTransform[3] - (-padfTransform[5] * poImgDoc->rows);
    poImgDoc->max_Y = padfTransform[3];

    poImgDoc->resolution = -padfTransform[5];
    poImgDoc->unit_dist = 1.0;

    return CE_None;
}

const char *IdrisiDataset::GetProjectionRef(void)
{
    return pszGeoRef;
}

CPLErr IdrisiDataset::SetProjection(const char * pszProjString)
{
    //#define SRS_WGS84_SEMIMAJOR     6378137.0                                
    //#define SRS_WGS84_INVFLATTENING 298.257223563


    char peString[MAXPESTRING];

    strcpy(peString, pszProjString);

    //WriteProjSystem(peString, (char *) pszFilename);
    /*
    PROJCS["unnamed",GEOGCS["unnamed",DATUM["unknown",SPHEROID["unretrievable - using WGS84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT[,0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude
    _of_origin",0],PARAMETER["central_meridian",-45],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000],UNIT["unknown",1],AUTHORITY["EPSG","29183"]]
    */
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

    if ((poDS->poImgDoc->data_type == RST_DT_BYTE) && (poDS->poImgDoc->legend_cats != 0))
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

    papszCategoryNames = NULL;
}

double IdrisiRasterBand::GetMinimum(int *pbSuccess)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    return poGDS->poImgDoc->min_value[nBand - 1];
}

double IdrisiRasterBand::GetMaximum(int *pbSuccess)
{
    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    return poGDS->poImgDoc->max_value[nBand - 1];
}

IdrisiRasterBand::~IdrisiRasterBand()
{
    if( poColorTable != NULL )
        delete poColorTable;

    CPLFree(pabyScan);
    CSLDestroy(papszCategoryNames);
}

CPLErr IdrisiRasterBand::IReadBlock(int nBlockXOff, 
                                    int nBlockYOff,
                                    void *pImage)
{
    CPLDebug("RST", "Begin: IdrisiRasterBand::IReadBlock");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    CPLDebug("RST", "nBlockXOff=%d, nBlockYOff=%d, nRecordSize=%d, nBlockXSize=%d, nBlockYSize=%d",
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
        for (int i = 0, int j = (3 - nBand); i < nBlockXSize; i++, j += 3)
        {
            ((uint8 *) pImage)[i] = pabyScan[j];
        }
    }
    else
    {
        memcpy(pImage, pabyScan, nRecordSize);
    }

    CPLDebug("RST", "End: IdrisiRasterBand::IReadBlock");

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
        for (int i = 0, int j = (3 - nBand); i < nBlockXSize; i++, j += 3)
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
GDALColorTable *IdrisiRasterBand::GetColorTable()
{
    CPLDebug("RST", "IdrisiRasterBand::GetColorTable");
    return poColorTable;
}

GDALColorInterp IdrisiRasterBand::GetColorInterpretation()
{
    CPLDebug("RST", "IdrisiRasterBand::GetColorInterpretation");

    IdrisiDataset *poPDS = (IdrisiDataset *) poDS;

    switch (poPDS->poImgDoc->data_type)
    {
    case RST_DT_BYTE:
        return (poPDS->poImgDoc->legend_cats == 0 ? GCI_GrayIndex : GCI_PaletteIndex);
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

char **IdrisiRasterBand::GetCategoryNames()
{
    CPLDebug("RST", "Begin: IdrisiRasterBand::GetCategoryNames");

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    if (poGDS->poImgDoc->legend_cats == 0) 
        return (char **) NULL;

    unsigned int i, j;

    CSLDestroy( papszCategoryNames );
    papszCategoryNames = NULL;

    for (i = 0, j = 0; i < poGDS->poImgDoc->legend_cats; i++)
    {
        CPLDebug("RST", "i=%d,j=%d,code[j]=%d", i,j,poGDS->poImgDoc->codes[j]);
        if (i == poGDS->poImgDoc->codes[j]) 
        {
            papszCategoryNames = CSLAddString(papszCategoryNames, poGDS->poImgDoc->categories[j]);
            j++;
        }
        else
            papszCategoryNames = CSLAddString(papszCategoryNames, "");
    }

    CPLDebug("RST", "End: IdrisiRasterBand::GetCategoryNames");

    return (char **) papszCategoryNames;
}

CPLErr IdrisiRasterBand::SetCategoryNames(char ** papszCategoryNames)
{
    CPLDebug("RST", "Begin: SetCategoryNames");

    return CE_None;
}
CPLErr IdrisiRasterBand::SetNoDataValue(double dfNoDataValue)
{
    CPLDebug("RST", "Begin: SetNoDataValue");

    return CE_None;
}

CPLErr IdrisiRasterBand::SetColorTable(GDALColorTable *aColorTable)
{
    CPLDebug("RST", "Begin: SetColorTable");

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

CPLErr IdrisiRasterBand::SetColorInterpretation(GDALColorInterp eColorInterp)
{
    CPLDebug("RST", "Begin: SetColorInterpretation");

    return CE_None;
}

CPLErr IdrisiRasterBand::SetScale(double dfScale)
{
    CPLDebug("RST", "Begin: SetScale");

    return CE_None;
}

CPLErr IdrisiRasterBand::SetUnitType(const char *pszUnitType)
{
    CPLDebug("RST", "Begin: SetUnitType");

    return CE_None;
}

CPLErr IdrisiRasterBand::SetStatistics(double dfMin, double dfMax, double dfMean, double dfStdDev)
{
    CPLDebug("RST", "Begin: SetStatistics dfMin=%f dfMax=%f", dfMin, dfMax);

    IdrisiDataset *poGDS = (IdrisiDataset *) poDS;

    poGDS->poImgDoc->display_max[nBand - 1] = dfMax;
    poGDS->poImgDoc->display_min[nBand - 1] = dfMin;
    poGDS->poImgDoc->max_value[nBand - 1] = dfMax;
    poGDS->poImgDoc->min_value[nBand - 1] = dfMin;

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
        poDriver->pfnCreateCopy = IdrisiDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}
