/*
 * $Id$
 *  keaband.cpp
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 *  Permission is hereby granted, free of charge, to any person 
 *  obtaining a copy of this software and associated documentation 
 *  files (the "Software"), to deal in the Software without restriction, 
 *  including without limitation the rights to use, copy, modify, 
 *  merge, publish, distribute, sublicense, and/or sell copies of the 
 *  Software, and to permit persons to whom the Software is furnished 
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be 
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR 
 *  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
 *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "keaband.h"
#include "keaoverview.h"
#include "keamaskband.h"
#include "kearat.h"

#include "gdal_rat.h"
#include "libkea/KEAAttributeTable.h"

#include <map>
#include <vector>

#include <limits.h>

// constructor
KEARasterBand::KEARasterBand( KEADataset *pDataset, int nSrcBand, GDALAccess eAccess, kealib::KEAImageIO *pImageIO, int *pRefCount )
{
    this->poDS = pDataset; // our pointer onto the dataset
    this->nBand = nSrcBand; // this is the band we are
    this->m_eKEADataType = pImageIO->getImageBandDataType(nSrcBand); // get the data type as KEA enum
    this->eDataType = KEA_to_GDAL_Type( m_eKEADataType );       // convert to GDAL enum
    this->nBlockXSize = pImageIO->getImageBlockSize(nSrcBand);  // get the native blocksize
    this->nBlockYSize = pImageIO->getImageBlockSize(nSrcBand);
    this->nRasterXSize = this->poDS->GetRasterXSize();          // ask the dataset for the total image size
    this->nRasterYSize = this->poDS->GetRasterYSize();
    this->eAccess = eAccess;

    if( pImageIO->attributeTablePresent(nSrcBand) )
    {
        this->m_nAttributeChunkSize = pImageIO->getAttributeTableChunkSize(nSrcBand);
    }
    else
    {
        this->m_nAttributeChunkSize = -1; // don't report
    }

    // grab the imageio class and its refcount
    this->m_pImageIO = pImageIO;
    this->m_pnRefCount = pRefCount;
    // increment the refcount as we now have a reference to imageio
    (*this->m_pnRefCount)++;

    // initialise overview variables
    m_nOverviews = 0;
    m_panOverviewBands = NULL;

    // mask band
    m_pMaskBand = NULL;
    m_bMaskBandOwned = false;

    // grab the description here
    this->sDescription = pImageIO->getImageBandDescription(nSrcBand);

    this->m_pAttributeTable = NULL;  // no RAT yet
    this->m_pColorTable = NULL;     // no color table yet

    // initialise the metadata as a CPLStringList
    m_papszMetadataList = NULL;
    this->UpdateMetadataList();
}

// destructor
KEARasterBand::~KEARasterBand()
{
    // destroy RAT if any
    delete this->m_pAttributeTable;
    // destroy color table if any
    delete this->m_pColorTable;
    // destroy the metadata
    CSLDestroy(this->m_papszMetadataList);
    // delete any overview bands
    this->deleteOverviewObjects();

    // if GDAL created the mask it will delete it
    if( m_bMaskBandOwned )
    {
        delete m_pMaskBand;
    }

    // according to the docs, this is required
    this->FlushCache();

    // decrement the recount and delete if needed
    (*m_pnRefCount)--;
    if( *m_pnRefCount == 0 )
    {
        try
        {
            m_pImageIO->close();
        }
        catch (kealib::KEAIOException &e)
        {
        }
        delete m_pImageIO;
        delete m_pnRefCount;
    }
}

// internal method that updates the metadata into m_papszMetadataList
void KEARasterBand::UpdateMetadataList()
{
    std::vector< std::pair<std::string, std::string> > data;

    // get all the metadata and iterate through
    data = this->m_pImageIO->getImageBandMetaData(this->nBand);
    for(std::vector< std::pair<std::string, std::string> >::iterator iterMetaData = data.begin(); iterMetaData != data.end(); ++iterMetaData)
    {
        // add to our list
        m_papszMetadataList = CSLSetNameValue(m_papszMetadataList, iterMetaData->first.c_str(), iterMetaData->second.c_str());
    }
    // we have a pseudo metadata item that tells if we are thematic 
    // or continuous like the HFA driver
    if( this->m_pImageIO->getImageBandLayerType(this->nBand) == kealib::kea_continuous )
    {
        m_papszMetadataList = CSLSetNameValue(m_papszMetadataList, "LAYER_TYPE", "athematic" );
    }
    else
    {
        m_papszMetadataList = CSLSetNameValue(m_papszMetadataList, "LAYER_TYPE", "thematic" );
    }
    // attribute table chunksize
    if( this->m_nAttributeChunkSize != -1 )
    {
        char szTemp[100];
        snprintf(szTemp, 100, "%d", this->m_nAttributeChunkSize );
        m_papszMetadataList = CSLSetNameValue(m_papszMetadataList, "ATTRIBUTETABLE_CHUNKSIZE", szTemp );
    }
}

// internal method to create the overviews
void KEARasterBand::CreateOverviews(int nOverviews, int *panOverviewList)
{
    // delete any existing overview bands
    this->deleteOverviewObjects();

    // allocate space
    m_panOverviewBands = (KEAOverview**)CPLMalloc(sizeof(KEAOverview*) * nOverviews);
    m_nOverviews = nOverviews;

    // loop through and create the overviews
    int nFactor, nXSize, nYSize;
    for( int nCount = 0; nCount < m_nOverviews; nCount++ )
    {
        nFactor = panOverviewList[nCount];
        // divide by the factor to get the new size
        nXSize = this->nRasterXSize / nFactor;
        nYSize = this->nRasterYSize / nFactor;

        // tell image io to create a new overview
        this->m_pImageIO->createOverview(this->nBand, nCount + 1, nXSize, nYSize);

        // create one of our objects to represent it
        m_panOverviewBands[nCount] = new KEAOverview((KEADataset*)this->poDS, this->nBand, GA_Update,
                                        this->m_pImageIO, this->m_pnRefCount, nCount + 1, nXSize, nYSize);
    }
}

// virtual method to read a block
CPLErr KEARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage )
{
    try
    {
        // GDAL deals in blocks - if we are at the end of a row
        // we need to adjust the amount read so we don't go over the edge
        int nxsize = this->nBlockXSize;
        int nxtotalsize = this->nBlockXSize * (nBlockXOff + 1);
        if( nxtotalsize > this->nRasterXSize )
        {
            nxsize -= (nxtotalsize - this->nRasterXSize);
        }
        int nysize = this->nBlockYSize;
        int nytotalsize = this->nBlockYSize * (nBlockYOff + 1);
        if( nytotalsize > this->nRasterYSize )
        {
            nysize -= (nytotalsize - this->nRasterYSize);
        }
        this->m_pImageIO->readImageBlock2Band( this->nBand, pImage, this->nBlockXSize * nBlockXOff,
                                            this->nBlockYSize * nBlockYOff,
                                            nxsize, nysize, this->nBlockXSize, this->nBlockYSize, 
                                            this->m_eKEADataType );
        return CE_None;
    }
    catch (kealib::KEAIOException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Failed to read file: %s", e.what() );
        return CE_Failure;
    }
}

// virtual method to write a block
CPLErr KEARasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff, void * pImage )
{
    try
    {
        // GDAL deals in blocks - if we are at the end of a row
        // we need to adjust the amount written so we don't go over the edge
        int nxsize = this->nBlockXSize;
        int nxtotalsize = this->nBlockXSize * (nBlockXOff + 1);
        if( nxtotalsize > this->nRasterXSize )
        {
            nxsize -= (nxtotalsize - this->nRasterXSize);
        }
        int nysize = this->nBlockYSize;
        int nytotalsize = this->nBlockYSize * (nBlockYOff + 1);
        if( nytotalsize > this->nRasterYSize )
        {
            nysize -= (nytotalsize - this->nRasterYSize);
        }

        this->m_pImageIO->writeImageBlock2Band( this->nBand, pImage, this->nBlockXSize * nBlockXOff,
                                            this->nBlockYSize * nBlockYOff,
                                            nxsize, nysize, this->nBlockXSize, this->nBlockYSize,
                                            this->m_eKEADataType );
        return CE_None;
    }
    catch (kealib::KEAIOException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Failed to write file: %s", e.what() );
        return CE_Failure;
    }
}

void KEARasterBand::SetDescription(const char *pszDescription)
{
    try
    {
        this->m_pImageIO->setImageBandDescription(this->nBand, pszDescription);
        GDALPamRasterBand::SetDescription(pszDescription);
    }
    catch (kealib::KEAIOException &e)
    {
        // ignore?
    }
}

// set a metadata item
CPLErr KEARasterBand::SetMetadataItem(const char *pszName, const char *pszValue, const char *pszDomain)
{
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != NULL ) && ( *pszDomain != '\0' ) )
        return CE_Failure;
    try
    {
        // if it is LAYER_TYPE handle it seperately
        if( EQUAL( pszName, "LAYER_TYPE" ) )
        {
            if( EQUAL( pszValue, "athematic" ) )
            {
                this->m_pImageIO->setImageBandLayerType(this->nBand, kealib::kea_continuous );
            }
            else
            {
                this->m_pImageIO->setImageBandLayerType(this->nBand, kealib::kea_thematic );
            }
        }
        else
        {
            // otherwise set it as normal
            this->m_pImageIO->setImageBandMetaData(this->nBand, pszName, pszValue );
        }
        // CSLSetNameValue will update if already there
        m_papszMetadataList = CSLSetNameValue( m_papszMetadataList, pszName, pszValue );
        return CE_None;
    }
    catch (kealib::KEAIOException &e)
    {
        return CE_Failure;
    }
}

// get a single metdata item
const char *KEARasterBand::GetMetadataItem (const char *pszName, const char *pszDomain)
{
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != NULL ) && ( *pszDomain != '\0' ) )
        return NULL;
    // get it out of the CSLStringList so we can be sure it is persistant
    return CSLFetchNameValue(m_papszMetadataList, pszName);
}

// get all the metadata as a CSLStringList
char **KEARasterBand::GetMetadata(const char *pszDomain)
{
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != NULL ) && ( *pszDomain != '\0' ) )
        return NULL;
    // conveniently we already have it in this format
    return m_papszMetadataList; 
}

// set the metdata as a CSLStringList
CPLErr KEARasterBand::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != NULL ) && ( *pszDomain != '\0' ) )
        return CE_Failure;
    int nIndex = 0;
    char *pszName;
    const char *pszValue;
    try
    {
        // iterate through each one
        while( papszMetadata[nIndex] != NULL )
        {
            pszName = NULL;
            pszValue = CPLParseNameValue( papszMetadata[nIndex], &pszName );
            if( pszValue == NULL )
                pszValue = "";
            if( pszName != NULL )
            {
                // it is LAYER_TYPE? if so handle seperately
                if( EQUAL( pszName, "LAYER_TYPE" ) )
                {
                    if( EQUAL( pszValue, "athematic" ) )
                    {
                        this->m_pImageIO->setImageBandLayerType(this->nBand, kealib::kea_continuous );
                    }
                    else
                    {
                        this->m_pImageIO->setImageBandLayerType(this->nBand, kealib::kea_thematic );
                    }
                }
                else
                {
                    // write it into the image
                    this->m_pImageIO->setImageBandMetaData(this->nBand, pszName, pszValue );
                }
                CPLFree(pszName);
            }
            nIndex++;
        }
    }
    catch (kealib::KEAIOException &e)
    {
        return CE_Failure;
    }
    // destroy our list and duplicate the one passed in
    // and use that as our list from now on
    CSLDestroy(m_papszMetadataList);
    m_papszMetadataList = CSLDuplicate(papszMetadata);
    return CE_None;
}

// get the no data value
double KEARasterBand::GetNoDataValue(int *pbSuccess)
{
    try
    {
        double dVal;
        this->m_pImageIO->getNoDataValue(this->nBand, &dVal, kealib::kea_64float);
        if( pbSuccess != NULL )
            *pbSuccess = 1;

        return dVal;
    }
    catch (kealib::KEAIOException &e)
    {
        if( pbSuccess != NULL )
            *pbSuccess = 0;
        return -1;
    }
}

// set the no data value
CPLErr KEARasterBand::SetNoDataValue(double dfNoData)
{
    // need to check for out of range values
    bool bSet = true;
    GDALDataType dtype = this->GetRasterDataType();
    switch( dtype )
    {
        case GDT_Byte:
            bSet = (dfNoData >= 0) && (dfNoData <= UCHAR_MAX);
            break;
        case GDT_UInt16:
            bSet = (dfNoData >= 0) && (dfNoData <= USHRT_MAX);
            break;
        case GDT_Int16:
            bSet = (dfNoData >= SHRT_MIN) && (dfNoData <= SHRT_MAX);
            break;
        case GDT_UInt32:
            bSet = (dfNoData >= 0) && (dfNoData <= UINT_MAX);
            break;
        case GDT_Int32:
            bSet = (dfNoData >= INT_MIN) && (dfNoData <= INT_MAX);
            break;
        default:
            // for other types we can't really tell if outside the range
            break;
    }

    try
    {
        if( bSet )
        {
            this->m_pImageIO->setNoDataValue(this->nBand, &dfNoData, kealib::kea_64float);
        }
        else
        {
            this->m_pImageIO->undefineNoDataValue(this->nBand);
        }
        return CE_None;
    }
    catch (kealib::KEAIOException &e)
    {
        return CE_Failure;
    }
}

CPLErr KEARasterBand::DeleteNoDataValue()
{
    try
    {
        m_pImageIO->undefineNoDataValue(this->nBand);
        return CE_None;
    }
    catch (kealib::KEAIOException &e)
    {
        return CE_Failure;
    }
}

GDALRasterAttributeTable *KEARasterBand::GetDefaultRAT()
{
    if( this->m_pAttributeTable == NULL )
    {
        try
        {
            // we assume this is never NULL - creates a new one if none exists
            kealib::KEAAttributeTable *pKEATable = this->m_pImageIO->getAttributeTable(kealib::kea_att_file, this->nBand);
            this->m_pAttributeTable = new KEARasterAttributeTable(pKEATable);
        }
        catch(kealib::KEAException &e)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to read attributes: %s", e.what() );
        }
    }
    return this->m_pAttributeTable;
}

CPLErr KEARasterBand::SetDefaultRAT(const GDALRasterAttributeTable *poRAT)
{
    if( poRAT == NULL )
        return CE_Failure;

    try
    {
        KEARasterAttributeTable *pKEATable = (KEARasterAttributeTable*)this->GetDefaultRAT();

        int numRows = poRAT->GetRowCount();
        pKEATable->SetRowCount(numRows);

        for( int nGDALColumnIndex = 0; nGDALColumnIndex < poRAT->GetColumnCount(); nGDALColumnIndex++ )
        {
            const char *pszColumnName = poRAT->GetNameOfCol(nGDALColumnIndex);
            GDALRATFieldType eFieldType = poRAT->GetTypeOfCol(nGDALColumnIndex);

            // do we have it?
            bool bExists = false;
            int nKEAColumnIndex;
            for( nKEAColumnIndex = 0; nKEAColumnIndex < pKEATable->GetColumnCount(); nKEAColumnIndex++ )
            {
                if( EQUAL(pszColumnName, pKEATable->GetNameOfCol(nKEAColumnIndex) ))
                {
                    bExists = true;
                    break;
                }
            }

            if( !bExists )
            {
                if( pKEATable->CreateColumn(pszColumnName, eFieldType,
                                            poRAT->GetUsageOfCol(nGDALColumnIndex) ) != CE_None )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Failed to create column");
                    return CE_Failure;
                }
                nKEAColumnIndex = pKEATable->GetColumnCount() - 1;
            }

            if( numRows == 0 )
                continue;

            // ok now copy data
            if( eFieldType == GFT_Integer )
            {
                int *panIntData = (int*)VSIMalloc2(numRows, sizeof(int));
                if( panIntData == NULL )
                {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                        "Memory Allocation failed in KEARasterAttributeTable::SetDefaultRAT");
                    return CE_Failure;
                }

                if( ((GDALRasterAttributeTable*)poRAT)->ValuesIO(GF_Read, nGDALColumnIndex, 0, numRows, panIntData ) == CE_None )
                {
                    pKEATable->ValuesIO(GF_Write, nKEAColumnIndex, 0, numRows, panIntData);
                }
                CPLFree(panIntData);
            }
            else if( eFieldType == GFT_Real )
            {
                double *padfFloatData = (double*)VSIMalloc2(numRows, sizeof(double));
                if( padfFloatData == NULL )
                {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                        "Memory Allocation failed in KEARasterAttributeTable::SetDefaultRAT");
                    return CE_Failure;
                }

                if( ((GDALRasterAttributeTable*)poRAT)->ValuesIO(GF_Read, nGDALColumnIndex, 0, numRows, padfFloatData ) == CE_None )
                {
                    pKEATable->ValuesIO(GF_Write, nKEAColumnIndex, 0, numRows, padfFloatData);
                }
                CPLFree(padfFloatData);
            }
            else
            {
                char **papszStringData = (char**)VSIMalloc2(numRows, sizeof(char*));
                if( papszStringData == NULL )
                {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                        "Memory Allocation failed in KEARasterAttributeTable::SetDefaultRAT");
                    return CE_Failure;
                }

                if( ((GDALRasterAttributeTable*)poRAT)->ValuesIO(GF_Read, nGDALColumnIndex, 0, numRows, papszStringData ) == CE_None )
                {
                    pKEATable->ValuesIO(GF_Write, nKEAColumnIndex, 0, numRows, papszStringData);
                    for( int n = 0; n < numRows; n++ )
                        CPLFree(papszStringData[n]);
                }
                CPLFree(papszStringData);

            }
        }
    }
    catch(kealib::KEAException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to write attributes: %s", e.what() );
        return CE_Failure;
    }
    return CE_None;
}

GDALColorTable *KEARasterBand::GetColorTable()
{
    if( this->m_pColorTable == NULL )
    {
        try
        {
            GDALRasterAttributeTable *pKEATable = this->GetDefaultRAT();
            int nRedIdx = -1;
            int nGreenIdx = -1;
            int nBlueIdx = -1;
            int nAlphaIdx = -1;

            for( int nColIdx = 0; nColIdx < pKEATable->GetColumnCount(); nColIdx++ )
            {
                if( pKEATable->GetTypeOfCol(nColIdx) == GFT_Integer )
                {
                    GDALRATFieldUsage eFieldUsage = pKEATable->GetUsageOfCol(nColIdx);
                    if( eFieldUsage == GFU_Red )
                        nRedIdx = nColIdx;
                    else if( eFieldUsage == GFU_Green )
                        nGreenIdx = nColIdx;
                    else if( eFieldUsage == GFU_Blue )
                        nBlueIdx = nColIdx;
                    else if( eFieldUsage == GFU_Alpha )
                        nAlphaIdx = nColIdx;
                }
            }

            if( ( nRedIdx != -1 ) && ( nGreenIdx != -1 ) && ( nBlueIdx != -1 ) && ( nAlphaIdx != -1 ) )
            {
                // we need to create one - only do RGB palettes
                this->m_pColorTable = new GDALColorTable(GPI_RGB);

                // OK go through each row and fill in the fields
                for( int nRowIndex = 0; nRowIndex < pKEATable->GetRowCount(); nRowIndex++ )
                {
                    // maybe could be more efficient using ValuesIO
                    GDALColorEntry colorEntry;
                    colorEntry.c1 = pKEATable->GetValueAsInt(nRowIndex, nRedIdx);
                    colorEntry.c2 = pKEATable->GetValueAsInt(nRowIndex, nGreenIdx);
                    colorEntry.c3 = pKEATable->GetValueAsInt(nRowIndex, nBlueIdx);
                    colorEntry.c4 = pKEATable->GetValueAsInt(nRowIndex, nAlphaIdx);
                    this->m_pColorTable->SetColorEntry(nRowIndex, &colorEntry);
                }
            }
        }
        catch(kealib::KEAException &e)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to read color table: %s", e.what() );
            delete this->m_pColorTable;
            this->m_pColorTable = NULL;
        }
    }
    return this->m_pColorTable;
}

CPLErr KEARasterBand::SetColorTable(GDALColorTable *poCT)
{
    if( poCT == NULL )
        return CE_Failure;

    try
    {
        GDALRasterAttributeTable *pKEATable = this->GetDefaultRAT();
        int nRedIdx = -1;
        int nGreenIdx = -1;
        int nBlueIdx = -1;
        int nAlphaIdx = -1;

        if( poCT->GetColorEntryCount() > pKEATable->GetRowCount() )
        {
            pKEATable->SetRowCount(poCT->GetColorEntryCount());
        }

        for( int nColIdx = 0; nColIdx < pKEATable->GetColumnCount(); nColIdx++ )
        {
            if( pKEATable->GetTypeOfCol(nColIdx) == GFT_Integer )
            {
                GDALRATFieldUsage eFieldUsage = pKEATable->GetUsageOfCol(nColIdx);
                if( eFieldUsage == GFU_Red )
                    nRedIdx = nColIdx;
                else if( eFieldUsage == GFU_Green )
                    nGreenIdx = nColIdx;
                else if( eFieldUsage == GFU_Blue )
                    nBlueIdx = nColIdx;
                else if( eFieldUsage == GFU_Alpha )
                    nAlphaIdx = nColIdx;
            }
        }

        // create if needed
        if( nRedIdx == -1 )
        {
            if( pKEATable->CreateColumn("Red", GFT_Integer, GFU_Red ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to create column" );
                return CE_Failure;
            }
            nRedIdx = pKEATable->GetColumnCount() - 1;
        }
        if( nGreenIdx == -1 )
        {
            if( pKEATable->CreateColumn("Green", GFT_Integer, GFU_Green ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to create column" );
                return CE_Failure;
            }
            nGreenIdx = pKEATable->GetColumnCount() - 1;
        }
        if( nBlueIdx == -1 )
        {
            if( pKEATable->CreateColumn("Blue", GFT_Integer, GFU_Blue ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to create column" );
                return CE_Failure;
            }
            nBlueIdx = pKEATable->GetColumnCount() - 1;
        }
        if( nAlphaIdx == -1 )
        {
            if( pKEATable->CreateColumn("Alpha", GFT_Integer, GFU_Alpha ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to create column" );
                return CE_Failure;
            }
            nAlphaIdx = pKEATable->GetColumnCount() - 1;
        }

        // OK go through each row and fill in the fields
        for( int nRowIndex = 0; nRowIndex < poCT->GetColorEntryCount(); nRowIndex++ )
        {
            // maybe could be more efficient using ValuesIO
            GDALColorEntry colorEntry;
            poCT->GetColorEntryAsRGB(nRowIndex, &colorEntry);
            pKEATable->SetValue(nRowIndex, nRedIdx, colorEntry.c1);
            pKEATable->SetValue(nRowIndex, nGreenIdx, colorEntry.c2);
            pKEATable->SetValue(nRowIndex, nBlueIdx, colorEntry.c3);
            pKEATable->SetValue(nRowIndex, nAlphaIdx, colorEntry.c4);
        }

        // out of date
        delete this->m_pColorTable;
        this->m_pColorTable = NULL;
    }
    catch(kealib::KEAException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to write color table: %s", e.what() );
        return CE_Failure;
    }
    return CE_None;
}

GDALColorInterp KEARasterBand::GetColorInterpretation()
{
    kealib::KEABandClrInterp ekeainterp;
    try
    {
        ekeainterp = this->m_pImageIO->getImageBandClrInterp(this->nBand);
    }
    catch(kealib::KEAException &e)
    {
        return GCI_GrayIndex;
    }
        
    GDALColorInterp egdalinterp;
    switch(ekeainterp)
    {
        case kealib::kea_generic:
        case kealib::kea_greyindex:
            egdalinterp = GCI_GrayIndex;
            break;
        case kealib::kea_paletteindex:
            egdalinterp = GCI_PaletteIndex;
            break;
        case kealib::kea_redband:
            egdalinterp = GCI_RedBand;
            break;
        case kealib::kea_greenband:
            egdalinterp = GCI_GreenBand;
            break;
        case kealib::kea_blueband:
            egdalinterp = GCI_BlueBand;
            break;
        case kealib::kea_alphaband:
            egdalinterp = GCI_AlphaBand;
            break;
        case kealib::kea_hueband:
            egdalinterp = GCI_HueBand;
            break;
        case kealib::kea_saturationband:
            egdalinterp = GCI_SaturationBand;
            break;
        case kealib::kea_lightnessband:
            egdalinterp = GCI_LightnessBand;
            break;
        case kealib::kea_cyanband:
            egdalinterp = GCI_CyanBand;
            break;
        case kealib::kea_magentaband:
            egdalinterp = GCI_MagentaBand;
            break;
        case kealib::kea_yellowband:
            egdalinterp = GCI_YellowBand;
            break;
        case kealib::kea_blackband:
            egdalinterp = GCI_BlackBand;
            break;
        case kealib::kea_ycbcr_yband:
            egdalinterp = GCI_YCbCr_YBand;
            break;
        case kealib::kea_ycbcr_cbband:
            egdalinterp = GCI_YCbCr_CbBand;
            break;
        case kealib::kea_ycbcr_crband:
            egdalinterp = GCI_YCbCr_CrBand;
            break;
        default:
            egdalinterp = GCI_GrayIndex;
            break;
    }
        
    return egdalinterp;
}

CPLErr KEARasterBand::SetColorInterpretation(GDALColorInterp egdalinterp)
{
    kealib::KEABandClrInterp ekeainterp;
    switch(egdalinterp)
    {
        case GCI_GrayIndex:
            ekeainterp = kealib::kea_greyindex;
            break;
        case GCI_PaletteIndex:
            ekeainterp = kealib::kea_paletteindex;
            break;
        case GCI_RedBand:
            ekeainterp = kealib::kea_redband;
            break;
        case GCI_GreenBand:
            ekeainterp = kealib::kea_greenband;
            break;
        case GCI_BlueBand:
            ekeainterp = kealib::kea_blueband;
            break;
        case GCI_AlphaBand:
            ekeainterp = kealib::kea_alphaband;
            break;
        case GCI_HueBand:
            ekeainterp = kealib::kea_hueband;
            break;
        case GCI_SaturationBand:
            ekeainterp = kealib::kea_saturationband;
            break;
        case GCI_LightnessBand:
            ekeainterp = kealib::kea_lightnessband;
            break;
        case GCI_CyanBand:
            ekeainterp = kealib::kea_cyanband;
            break;
        case GCI_MagentaBand:
            ekeainterp = kealib::kea_magentaband;
            break;
        case GCI_YellowBand:
            ekeainterp = kealib::kea_yellowband;
            break;
        case GCI_BlackBand:
            ekeainterp = kealib::kea_blackband;
            break;
        case GCI_YCbCr_YBand:
            ekeainterp = kealib::kea_ycbcr_yband;
            break;
        case GCI_YCbCr_CbBand:
            ekeainterp = kealib::kea_ycbcr_cbband;
            break;
        case GCI_YCbCr_CrBand:
            ekeainterp = kealib::kea_ycbcr_crband;
            break;
        default:
            ekeainterp = kealib::kea_greyindex;
            break;
    }

    try
    {
        this->m_pImageIO->setImageBandClrInterp(this->nBand, ekeainterp);
    }
    catch(kealib::KEAException &e)
    {
        // do nothing? The docs say CE_Failure only if unsupporte by format
    }
    return CE_None;
}

// clean up our overview objects
void KEARasterBand::deleteOverviewObjects()
{
    // deletes the objects - not the overviews themselves
    int nCount;
    for( nCount = 0; nCount < m_nOverviews; nCount++ )
    {
        delete m_panOverviewBands[nCount];
    }
    CPLFree(m_panOverviewBands);
    m_panOverviewBands = NULL;
    m_nOverviews = 0;
}

// read in any overviews in the file into our array of objects
void KEARasterBand::readExistingOverviews()
{
    // delete any existing overview bands
    this->deleteOverviewObjects();

    m_nOverviews = this->m_pImageIO->getNumOfOverviews(this->nBand);
    m_panOverviewBands = (KEAOverview**)CPLMalloc(sizeof(KEAOverview*) * m_nOverviews);

    uint64_t nXSize, nYSize;    
    for( int nCount = 0; nCount < m_nOverviews; nCount++ )
    {
        this->m_pImageIO->getOverviewSize(this->nBand, nCount + 1, &nXSize, &nYSize);
        m_panOverviewBands[nCount] = new KEAOverview((KEADataset*)this->poDS, this->nBand, GA_ReadOnly,
                                        this->m_pImageIO, this->m_pnRefCount, nCount + 1, nXSize, nYSize);
    }
}

// number of overviews
int KEARasterBand::GetOverviewCount()
{
    return m_nOverviews;
}

// get a given overview
GDALRasterBand* KEARasterBand::GetOverview(int nOverview)
{
    if( nOverview < 0 || nOverview >= m_nOverviews )
    {
        return NULL;
    }
    else
    {
        return m_panOverviewBands[nOverview];
    }
}

CPLErr KEARasterBand::CreateMaskBand(CPL_UNUSED int nFlags)
{
    if( m_bMaskBandOwned )
        delete m_pMaskBand;
    m_pMaskBand = NULL;
    try
    {
        this->m_pImageIO->createMask(this->nBand);
    }
    catch(kealib::KEAException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to create mask band: %s", e.what());
        return CE_Failure;
    }
    return CE_None;
}

GDALRasterBand* KEARasterBand::GetMaskBand()
{
    if( m_pMaskBand == NULL )
    {
        try
        {
            if( this->m_pImageIO->maskCreated(this->nBand) )
            {
                m_pMaskBand = new KEAMaskBand(this, this->m_pImageIO, this->m_pnRefCount);
                m_bMaskBandOwned = true;
            }
            else
            {
                // use the base class implementation - GDAL will delete
                //fprintf( stderr, "returning base GetMaskBand()\n" );
                m_pMaskBand = GDALPamRasterBand::GetMaskBand();
            }
        }
        catch(kealib::KEAException &e)
        {
            // do nothing?
        }
    }
    return m_pMaskBand;
}

int KEARasterBand::GetMaskFlags()
{
    try
    {
        if( ! this->m_pImageIO->maskCreated(this->nBand) )
        {
            // need to return the base class one since we are using
            // the base class implementation of GetMaskBand()
            //fprintf( stderr, "returning base GetMaskFlags()\n" );
            return GDALPamRasterBand::GetMaskFlags();
        }
    }
    catch(kealib::KEAException &e)
    {
        // do nothing?
    }

    // none of the other flags seem to make sense...
    return 0;
}

