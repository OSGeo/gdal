/*
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

#include <map>
#include <vector>

#include <limits.h>

CPL_CVSID("$Id$")

// constructor
KEARasterBand::KEARasterBand( KEADataset *pDataset, int nSrcBand, GDALAccess eAccessIn, kealib::KEAImageIO *pImageIO, LockedRefCount *pRefCount ):
    m_eKEADataType(pImageIO->getImageBandDataType(nSrcBand)) // get the data type as KEA enum
{
    this->m_hMutex = CPLCreateMutex();
    CPLReleaseMutex( this->m_hMutex );

    this->poDS = pDataset; // our pointer onto the dataset
    this->nBand = nSrcBand; // this is the band we are
    this->eDataType = KEA_to_GDAL_Type( m_eKEADataType );       // convert to GDAL enum
    this->nBlockXSize = pImageIO->getImageBlockSize(nSrcBand);  // get the native blocksize
    this->nBlockYSize = pImageIO->getImageBlockSize(nSrcBand);
    this->nRasterXSize = this->poDS->GetRasterXSize();          // ask the dataset for the total image size
    this->nRasterYSize = this->poDS->GetRasterYSize();
    this->eAccess = eAccessIn;

    if( pImageIO->attributeTablePresent(nSrcBand) )
    {
        this->m_nAttributeChunkSize
            = pImageIO->getAttributeTableChunkSize(nSrcBand);
    }
    else
    {
        this->m_nAttributeChunkSize = -1; // don't report
    }

    // grab the imageio class and its refcount
    this->m_pImageIO = pImageIO;
    this->m_pRefCount = pRefCount;
    // increment the refcount as we now have a reference to imageio
    this->m_pRefCount->IncRef();

    // Initialize overview variables
    m_nOverviews = 0;
    m_panOverviewBands = nullptr;

    // mask band
    m_pMaskBand = nullptr;
    m_bMaskBandOwned = false;

    // grab the description here
    this->sDescription = pImageIO->getImageBandDescription(nSrcBand);

    this->m_pAttributeTable = nullptr;  // no RAT yet
    this->m_pColorTable = nullptr;     // no color table yet

    // Initialize the metadata as a CPLStringList.
    m_papszMetadataList = nullptr;
    this->UpdateMetadataList();
    m_pszHistoBinValues = nullptr;
}

// destructor
KEARasterBand::~KEARasterBand()
{
    {
        CPLMutexHolderD( &m_hMutex );
        // destroy RAT if any
        delete this->m_pAttributeTable;
        // destroy color table if any
        delete this->m_pColorTable;
        // destroy the metadata
        CSLDestroy(this->m_papszMetadataList);
        if( this->m_pszHistoBinValues != nullptr )
        {
            // histogram bin values as a string
            CPLFree(this->m_pszHistoBinValues);
        }
        // delete any overview bands
        this->deleteOverviewObjects();

        // if GDAL created the mask it will delete it
        if( m_bMaskBandOwned )
        {
            delete m_pMaskBand;
        }
    }

    // according to the docs, this is required
    this->FlushCache(true);

    // decrement the recount and delete if needed
    if( m_pRefCount->DecRef() )
    {
        try
        {
            m_pImageIO->close();
        }
        catch (const kealib::KEAIOException &)
        {
        }
        delete m_pImageIO;
        delete m_pRefCount;
    }
}

// internal method that updates the metadata into m_papszMetadataList
void KEARasterBand::UpdateMetadataList()
{
    CPLMutexHolderD( &m_hMutex );
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

    // STATISTICS_HISTONUMBINS
    const GDALRasterAttributeTable *pTable = KEARasterBand::GetDefaultRAT();
    if( pTable != nullptr )
    {
        CPLString osWorkingResult;
        osWorkingResult.Printf( "%lu", (unsigned long)pTable->GetRowCount());
        m_papszMetadataList = CSLSetNameValue(m_papszMetadataList, "STATISTICS_HISTONUMBINS", osWorkingResult);

        // attribute table chunksize
        if( this->m_nAttributeChunkSize != -1 )
        {
            osWorkingResult.Printf( "%d", this->m_nAttributeChunkSize );
            m_papszMetadataList = CSLSetNameValue(m_papszMetadataList, "ATTRIBUTETABLE_CHUNKSIZE", osWorkingResult );
        }
    }
}

// internal method to set the histogram column from a string (for metadata)

CPLErr KEARasterBand::SetHistogramFromString(const char *pszString)
{
    // copy it so we can change it (put nulls in etc)
    char *pszBinValues = CPLStrdup(pszString);
    if( pszBinValues == nullptr )
        return CE_Failure;

    // find the number of | chars
    int nRows = 0, i = 0;
    while( pszBinValues[i] != '\0' )
    {
        if( pszBinValues[i] == '|' )
            nRows++;
        i++;
    }

    GDALRasterAttributeTable *pTable = this->GetDefaultRAT();
    if( pTable == nullptr )
    {
        CPLFree(pszBinValues);
        return CE_Failure;
    }

    // find histogram column if it exists
    int nCol = pTable->GetColOfUsage(GFU_PixelCount);
    if( nCol == -1 )
    {
        if( pTable->CreateColumn("Histogram", GFT_Real, GFU_PixelCount) != CE_None )
        {
            CPLFree(pszBinValues);
            return CE_Failure;
        }

        nCol = pTable->GetColumnCount() - 1;
    }

    if( nRows > pTable->GetRowCount() )
        pTable->SetRowCount(nRows);

    char * pszWork = pszBinValues;
    for( int nBin = 0; nBin < nRows; ++nBin )
    {
        char * pszEnd = strchr( pszWork, '|' );
        if ( pszEnd != nullptr )
        {
            *pszEnd = 0;
            double dValue = CPLAtof( pszWork );
            pTable->SetValue(nBin, nCol, dValue);
            pszWork = pszEnd + 1;
        }
    }

    CPLFree(pszBinValues);

    return CE_None;
}

// get histogram as string with values separated by '|'
char *KEARasterBand::GetHistogramAsString()
{
    const GDALRasterAttributeTable *pTable = this->GetDefaultRAT();
    if( pTable == nullptr )
        return nullptr;
    int nRows = pTable->GetRowCount();
    // find histogram column if it exists
    int nCol = pTable->GetColOfUsage(GFU_PixelCount);
    if( nCol == -1 )
        return nullptr;

    unsigned int nBufSize = 1024;
    char * pszBinValues = (char *)CPLMalloc( nBufSize );
    int    nBinValuesLen = 0;
    pszBinValues[0] = 0;

    for ( int nBin = 0; nBin < nRows; ++nBin )
    {
        char szBuf[32];
        // RAT's don't handle GUIntBig - use double instead. Cast back
        snprintf( szBuf, 31, CPL_FRMT_GUIB, (GUIntBig)pTable->GetValueAsDouble(nBin, nCol) );
        if ( ( nBinValuesLen + strlen( szBuf ) + 2 ) > nBufSize )
        {
            nBufSize *= 2;
            char* pszNewBinValues = (char *)VSIRealloc( pszBinValues, nBufSize );
            if (pszNewBinValues == nullptr)
            {
                break;
            }

            pszBinValues = pszNewBinValues;
        }

        strcat( pszBinValues+nBinValuesLen, szBuf );
        strcat( pszBinValues+nBinValuesLen, "|" );
        nBinValuesLen += static_cast<int>(strlen(pszBinValues+nBinValuesLen));
    }

    return pszBinValues;
}

// internal method to create the overviews
void KEARasterBand::CreateOverviews(int nOverviews, int *panOverviewList)
{
    CPLMutexHolderD( &m_hMutex );
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
                                        this->m_pImageIO, this->m_pRefCount, nCount + 1, nXSize, nYSize);
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
    catch (const kealib::KEAIOException &e)
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
    catch (const kealib::KEAIOException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Failed to write file: %s", e.what() );
        return CE_Failure;
    }
}

void KEARasterBand::SetDescription(const char *pszDescription)
{
    CPLMutexHolderD( &m_hMutex );
    try
    {
        this->m_pImageIO->setImageBandDescription(this->nBand, pszDescription);
        GDALPamRasterBand::SetDescription(pszDescription);
    }
    catch (const kealib::KEAIOException &)
    {
        // ignore?
    }
}

// set a metadata item
CPLErr KEARasterBand::SetMetadataItem(const char *pszName, const char *pszValue, const char *pszDomain)
{
    CPLMutexHolderD( &m_hMutex );
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != nullptr ) && ( *pszDomain != '\0' ) )
        return CE_Failure;

    // kealib doesn't currently support removing values
    if( pszValue == nullptr )
        return CE_Failure;

    try
    {
        // if it is LAYER_TYPE handle it separately
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
        else if( EQUAL( pszName, "STATISTICS_HISTOBINVALUES" ) )
        {
            if( this->SetHistogramFromString(pszValue) != CE_None )
                return CE_Failure;
            else
                return CE_None;
        }
        else if( EQUAL( pszName, "STATISTICS_HISTONUMBINS" ) )
        {
            GDALRasterAttributeTable *pTable = this->GetDefaultRAT();
            if( pTable != nullptr )
                pTable->SetRowCount(atoi(pszValue));
            // leave to update m_papszMetadataList below
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
    catch (const kealib::KEAIOException &)
    {
        return CE_Failure;
    }
}

// get a single metadata item
const char *KEARasterBand::GetMetadataItem (const char *pszName, const char *pszDomain)
{
    CPLMutexHolderD( &m_hMutex );
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != nullptr ) && ( *pszDomain != '\0' ) )
        return nullptr;

    if(EQUAL( pszName, "STATISTICS_HISTOBINVALUES" ) )
    {
        if( m_pszHistoBinValues != nullptr )
            CPLFree(m_pszHistoBinValues); // could have changed
        m_pszHistoBinValues = this->GetHistogramAsString();
        return m_pszHistoBinValues;
    }

    // get it out of the CSLStringList so we can be sure it is persistent
    return CSLFetchNameValue(m_papszMetadataList, pszName);
}

// get all the metadata as a CSLStringList
char **KEARasterBand::GetMetadata(const char *pszDomain)
{
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != nullptr ) && ( *pszDomain != '\0' ) )
        return nullptr;
    // Note: ignoring STATISTICS_HISTOBINVALUES as these are likely to be very long
    // not sure user should get those unless they really ask...

    // conveniently we already have it in this format
    return m_papszMetadataList;
}

// set the metadata as a CSLStringList
CPLErr KEARasterBand::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    CPLMutexHolderD( &m_hMutex );
    // only deal with 'default' domain - no geolocation etc
    if( ( pszDomain != nullptr ) && ( *pszDomain != '\0' ) )
        return CE_Failure;
    int nIndex = 0;
    try
    {
        // iterate through each one
        while( papszMetadata[nIndex] != nullptr )
        {
            char *pszName = nullptr;
            const char *pszValue =
                CPLParseNameValue( papszMetadata[nIndex], &pszName );
            if( pszValue == nullptr )
                pszValue = "";
            if( pszName != nullptr )
            {
                // it is LAYER_TYPE? if so handle separately
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
                else if( EQUAL( pszName, "STATISTICS_HISTOBINVALUES" ) )
                {
                    if( this->SetHistogramFromString(pszValue) != CE_None )
                    {
                        CPLFree(pszName);
                        return CE_Failure;
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
    catch (const kealib::KEAIOException &)
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
        if( pbSuccess != nullptr )
            *pbSuccess = 1;

        return dVal;
    }
    catch (const kealib::KEAIOException &)
    {
        if( pbSuccess != nullptr )
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
    catch (const kealib::KEAIOException &)
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
    catch (const kealib::KEAIOException &)
    {
        return CE_Failure;
    }
}

CPLErr KEARasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc fn, void *pProgressData)
{
    if( bForce )
    {
        return GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax, pnBuckets,
                        ppanHistogram, bForce, fn, pProgressData);
    }
    else
    {
        // returned cached if avail
        // I've used the RAT interface here as it deals with data type
        // conversions. Would be nice to have GUIntBig support in RAT though...
        GDALRasterAttributeTable *pTable = this->GetDefaultRAT();
        if( pTable == nullptr )
            return CE_Failure;
        int nRows = pTable->GetRowCount();

        // find histogram column if it exists
        int nCol = pTable->GetColOfUsage(GFU_PixelCount);
        if( nCol == -1 )
            return CE_Warning;

        double dfRow0Min, dfBinSize;
        if( !pTable->GetLinearBinning(&dfRow0Min, &dfBinSize) )
            return CE_Warning;

        *ppanHistogram = (GUIntBig*)VSIMalloc2(nRows, sizeof(GUIntBig));
        if( *ppanHistogram == nullptr )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterBand::GetDefaultHistogram");
            return CE_Failure;
        }

        double *pDoubleHisto = (double*)VSIMalloc2(nRows, sizeof(double));
        if( pDoubleHisto == nullptr )
        {
            CPLFree(*ppanHistogram);
            CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterBand::GetDefaultHistogram");
            return CE_Failure;
        }

        if( pTable->ValuesIO(GF_Read, nCol, 0, nRows, pDoubleHisto) != CE_None )
            return CE_Failure;

        // convert to GUIntBig
        for( int n = 0; n < nRows; n++ )
            (*ppanHistogram)[n] = static_cast<GUIntBig>(pDoubleHisto[n]);

        CPLFree(pDoubleHisto);

        *pnBuckets = nRows;
        *pdfMin = dfRow0Min;
        *pdfMax = dfRow0Min + ((nRows + 1) * dfBinSize);
        return CE_None;
    }
}

CPLErr KEARasterBand::SetDefaultHistogram( double /*dfMin*/, double /*dfMax*/,
                                           int nBuckets, GUIntBig *panHistogram )
{

    GDALRasterAttributeTable *pTable = this->GetDefaultRAT();
    if( pTable == nullptr )
        return CE_Failure;
    int nRows = pTable->GetRowCount();

    // find histogram column if it exists
    int nCol = pTable->GetColOfUsage(GFU_PixelCount);
    if( nCol == -1 )
    {
        if( pTable->CreateColumn("Histogram", GFT_Real, GFU_PixelCount) != CE_None )
            return CE_Failure;

        nCol = pTable->GetColumnCount() - 1;
    }

    if( nBuckets > nRows )
        pTable->SetRowCount(nBuckets);

    // convert to double (RATs don't take GUIntBig yet)
    double *pDoubleHist = (double*)VSIMalloc2(nBuckets, sizeof(double));

    if( pDoubleHist == nullptr )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                "Memory Allocation failed in KEARasterBand::SetDefaultHistogram");
        return CE_Failure;
    }

    for( int n = 0; n < nBuckets; n++ )
        pDoubleHist[n] = static_cast<double>(panHistogram[n]);

    if( pTable->ValuesIO(GF_Write, nCol, 0, nBuckets, pDoubleHist) != CE_None )
    {
        CPLFree(pDoubleHist);
        return CE_Failure;
    }

    CPLFree(pDoubleHist);

    return CE_None;
}

GDALRasterAttributeTable *KEARasterBand::GetDefaultRAT()
{
    CPLMutexHolderD( &m_hMutex );
    if( this->m_pAttributeTable == nullptr )
    {
        try
        {
            // we assume this is never NULL - creates a new one if none exists
            // (or raises exception)
            kealib::KEAAttributeTable *pKEATable = this->m_pImageIO->getAttributeTable(kealib::kea_att_file, this->nBand);
            this->m_pAttributeTable = new KEARasterAttributeTable(pKEATable, this);
        }
        catch(const kealib::KEAException &e)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to read attributes: %s", e.what() );
        }
    }
    return this->m_pAttributeTable;
}

CPLErr KEARasterBand::SetDefaultRAT(const GDALRasterAttributeTable *poRAT)
{
    if( poRAT == nullptr )
        return CE_Failure;

    try
    {
        KEARasterAttributeTable *pKEATable = (KEARasterAttributeTable*)this->GetDefaultRAT();
        if( pKEATable == nullptr )
            return CE_Failure;

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
                int *panIntData = (int*)VSI_MALLOC2_VERBOSE(numRows, sizeof(int));
                if( panIntData == nullptr )
                {
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
                double *padfFloatData = (double*)VSI_MALLOC2_VERBOSE(numRows, sizeof(double));
                if( padfFloatData == nullptr )
                {
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
                char **papszStringData = (char**)VSI_MALLOC2_VERBOSE(numRows, sizeof(char*));
                if( papszStringData == nullptr )
                {
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
    catch(const kealib::KEAException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to write attributes: %s", e.what() );
        return CE_Failure;
    }
    return CE_None;
}

GDALColorTable *KEARasterBand::GetColorTable()
{
    CPLMutexHolderD( &m_hMutex );
    if( this->m_pColorTable == nullptr )
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
                    colorEntry.c1 = static_cast<short>(pKEATable->GetValueAsInt(nRowIndex, nRedIdx));
                    colorEntry.c2 = static_cast<short>(pKEATable->GetValueAsInt(nRowIndex, nGreenIdx));
                    colorEntry.c3 = static_cast<short>(pKEATable->GetValueAsInt(nRowIndex, nBlueIdx));
                    colorEntry.c4 = static_cast<short>(pKEATable->GetValueAsInt(nRowIndex, nAlphaIdx));
                    this->m_pColorTable->SetColorEntry(nRowIndex, &colorEntry);
                }
            }
        }
        catch(const kealib::KEAException &e)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to read color table: %s", e.what() );
            delete this->m_pColorTable;
            this->m_pColorTable = nullptr;
        }
    }
    return this->m_pColorTable;
}

CPLErr KEARasterBand::SetColorTable(GDALColorTable *poCT)
{
    if( poCT == nullptr )
        return CE_Failure;

    CPLMutexHolderD( &m_hMutex );
    try
    {
        GDALRasterAttributeTable *pKEATable = this->GetDefaultRAT();
        if( pKEATable == nullptr )
            return CE_Failure;
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
        this->m_pColorTable = nullptr;
    }
    catch(const kealib::KEAException &e)
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
    catch(const kealib::KEAException &)
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
    catch(const kealib::KEAException &)
    {
        // Do nothing? The docs say CE_Failure only if unsupported by format.
    }
    return CE_None;
}

// clean up our overview objects
// assumes mutex being held by caller
void KEARasterBand::deleteOverviewObjects()
{
    // deletes the objects - not the overviews themselves
    int nCount;
    for( nCount = 0; nCount < m_nOverviews; nCount++ )
    {
        delete m_panOverviewBands[nCount];
    }
    CPLFree(m_panOverviewBands);
    m_panOverviewBands = nullptr;
    m_nOverviews = 0;
}

// read in any overviews in the file into our array of objects
void KEARasterBand::readExistingOverviews()
{
    CPLMutexHolderD( &m_hMutex );
    // delete any existing overview bands
    this->deleteOverviewObjects();

    m_nOverviews = this->m_pImageIO->getNumOfOverviews(this->nBand);
    m_panOverviewBands = (KEAOverview**)CPLMalloc(sizeof(KEAOverview*) * m_nOverviews);

    uint64_t nXSize, nYSize;
    for( int nCount = 0; nCount < m_nOverviews; nCount++ )
    {
        this->m_pImageIO->getOverviewSize(this->nBand, nCount + 1, &nXSize, &nYSize);
        m_panOverviewBands[nCount] = new KEAOverview((KEADataset*)this->poDS, this->nBand, GA_ReadOnly,
                                        this->m_pImageIO, this->m_pRefCount, nCount + 1, nXSize, nYSize);
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
        return nullptr;
    }
    else
    {
        return m_panOverviewBands[nOverview];
    }
}

CPLErr KEARasterBand::CreateMaskBand(int)
{
    CPLMutexHolderD( &m_hMutex );
    if( m_bMaskBandOwned )
        delete m_pMaskBand;
    m_pMaskBand = nullptr;
    try
    {
        this->m_pImageIO->createMask(this->nBand);
    }
    catch(const kealib::KEAException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to create mask band: %s", e.what());
        return CE_Failure;
    }
    return CE_None;
}

GDALRasterBand* KEARasterBand::GetMaskBand()
{
    CPLMutexHolderD( &m_hMutex );
    if( m_pMaskBand == nullptr )
    {
        try
        {
            if( this->m_pImageIO->maskCreated(this->nBand) )
            {
                m_pMaskBand = new KEAMaskBand(this, this->m_pImageIO, this->m_pRefCount);
                m_bMaskBandOwned = true;
            }
            else
            {
                // use the base class implementation - GDAL will delete
                //fprintf( stderr, "returning base GetMaskBand()\n" );
                m_pMaskBand = GDALPamRasterBand::GetMaskBand();
            }
        }
        catch(const kealib::KEAException &)
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
    catch(const kealib::KEAException &)
    {
        // do nothing?
    }

    // none of the other flags seem to make sense...
    return 0;
}

kealib::KEALayerType KEARasterBand::getLayerType() const
{
    return m_pImageIO->getImageBandLayerType(nBand);
}
void KEARasterBand::setLayerType(kealib::KEALayerType eLayerType)
{
    m_pImageIO->setImageBandLayerType(nBand, eLayerType);
}

