/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of derived subdatasets
 * Author:   Julien Michel <julien dot michel at cnes dot fr>
 *
 ******************************************************************************
 * Copyright (c) 2016 Julien Michel <julien dot michel at cnes dot fr>
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
 *****************************************************************************/
#include "../vrt/vrtdataset.h"
#include "gdal_pam.h"
#include "derivedlist.h"

CPL_CVSID("$Id$")

class DerivedDataset final: public VRTDataset
{
    public:
        DerivedDataset( int nXSize, int nYSize );
       ~DerivedDataset() {}

        static int Identify( GDALOpenInfo * );
        static GDALDataset *Open( GDALOpenInfo * );
};

DerivedDataset::DerivedDataset(int nXSize, int nYSize) :
    VRTDataset(nXSize, nYSize)
{
    poDriver = nullptr;
    SetWritable(FALSE);
}

int DerivedDataset::Identify(GDALOpenInfo * poOpenInfo)
{
    /* Try to open original dataset */
    CPLString filename(poOpenInfo->pszFilename);

    /* DERIVED_SUBDATASET should be first domain */
    const size_t dsds_pos = filename.find("DERIVED_SUBDATASET:");

    if (dsds_pos != 0)
    {
        /* Unable to Open in this case */
        return FALSE;
    }

    return TRUE;
}

GDALDataset * DerivedDataset::Open( GDALOpenInfo * poOpenInfo )
{
    /* Try to open original dataset */
    CPLString filename(poOpenInfo->pszFilename);

    /* DERIVED_SUBDATASET should be first domain */
    const size_t dsds_pos = filename.find("DERIVED_SUBDATASET:");
    const size_t nPrefixLen = strlen("DERIVED_SUBDATASET:");

    if (dsds_pos != 0)
    {
        /* Unable to Open in this case */
        return nullptr;
    }

    /* Next, we need to now which derived dataset to compute */
    const size_t alg_pos = filename.find(":",nPrefixLen+1);
    if (alg_pos == std::string::npos)
    {
        /* Unable to Open if we do not find the name of the derived dataset */
        return nullptr;
    }

    CPLString odDerivedName = filename.substr(nPrefixLen,alg_pos-nPrefixLen);

    CPLDebug("DerivedDataset::Open","Derived dataset requested: %s",odDerivedName.c_str());

    CPLString pixelFunctionName = "";
    bool datasetFound = false;

    unsigned int nbSupportedDerivedDS(0);
    GDALDataType type  = GDT_Float64;

    const DerivedDatasetDescription * poDDSDesc = GDALGetDerivedDatasetDescriptions(&nbSupportedDerivedDS);

    for(unsigned int derivedId = 0; derivedId<nbSupportedDerivedDS;++derivedId)
    {
        if(odDerivedName == poDDSDesc[derivedId].pszDatasetName)
        {
            datasetFound = true;
            pixelFunctionName = poDDSDesc[derivedId].pszPixelFunction;
            type = GDALGetDataTypeByName(poDDSDesc[derivedId].pszOutputPixelType);
        }
    }

    if(!datasetFound)
    {
        return nullptr;
    }

    CPLString odFilename = filename.substr(alg_pos+1,filename.size() - alg_pos);

    GDALDataset * poTmpDS = (GDALDataset*)GDALOpen(odFilename, GA_ReadOnly);

    if( poTmpDS == nullptr )
        return nullptr;

    int nbBands = poTmpDS->GetRasterCount();

    if(nbBands == 0)
    {
        GDALClose(poTmpDS);
        return nullptr;
    }

    int nRows = poTmpDS->GetRasterYSize();
    int nCols = poTmpDS->GetRasterXSize();

    DerivedDataset * poDS = new DerivedDataset(nCols,nRows);

    // Transfer metadata
    poDS->SetMetadata(poTmpDS->GetMetadata());

    char** papszRPC = poTmpDS->GetMetadata("RPC");
    if( papszRPC )
    {
        poDS->SetMetadata(papszRPC, "RPC");
    }

    // Transfer projection
    poDS->SetProjection(poTmpDS->GetProjectionRef());

    // Transfer geotransform
    double padfTransform[6];
    CPLErr transformOk = poTmpDS->GetGeoTransform(padfTransform);

    if(transformOk == CE_None)
    {
        poDS->SetGeoTransform(padfTransform);
    }

    // Transfer GCPs
    const char * gcpProjection = poTmpDS->GetGCPProjection();
    int nbGcps = poTmpDS->GetGCPCount();
    poDS->SetGCPs(nbGcps,poTmpDS->GetGCPs(),gcpProjection);

    // Map bands
    for(int nBand = 1; nBand <= nbBands; ++nBand)
    {
        VRTDerivedRasterBand *poBand =
            new VRTDerivedRasterBand(poDS,nBand,type,nCols,nRows);
        poDS->SetBand(nBand,poBand);

        poBand->SetPixelFunctionName(pixelFunctionName);
        poBand->SetSourceTransferType(poTmpDS->GetRasterBand(nBand)->GetRasterDataType());

        poBand->AddComplexSource(odFilename,nBand,0,0,nCols,nRows,0,0,nCols,nRows);
    }

    GDALClose(poTmpDS);

    // If dataset is a real file, initialize overview manager
    VSIStatBufL  sStat;
    if( VSIStatL( odFilename, &sStat ) == 0 )
    {
        CPLString path = CPLGetPath(odFilename);
        CPLString ovrFileName = "DERIVED_DATASET_"+odDerivedName+"_"+CPLGetFilename(odFilename);
        CPLString ovrFilePath = CPLFormFilename(path,ovrFileName,nullptr);

        poDS->oOvManager.Initialize( poDS, ovrFilePath );
    }

    return poDS;
}

void GDALRegister_Derived()
{
    if( GDALGetDriverByName( "DERIVED" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "DERIVED" );
#ifdef GDAL_DCAP_RASTER
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
#endif
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Derived datasets using VRT pixel functions" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/derived.html" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "NO" );

    poDriver->pfnOpen = DerivedDataset::Open;
    poDriver->pfnIdentify = DerivedDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
