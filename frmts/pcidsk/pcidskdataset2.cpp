/******************************************************************************
 *
 * Project:  PCIDSK Database File
 * Purpose:  Read/write PCIDSK Database File used by the PCI software, using
 *           the external PCIDSK library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
 ****************************************************************************/

#include "gdal_frmts.h"
#include "pcidskdataset2.h"

#include <algorithm>

CPL_CVSID("$Id$")

const PCIDSK::PCIDSKInterfaces *PCIDSK2GetInterfaces(void);

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSK2Band                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            PCIDSK2Band()                             */
/*                                                                      */
/*      This constructor is used for main file channels.                */
/************************************************************************/

PCIDSK2Band::PCIDSK2Band( PCIDSKFile *poFileIn,
                          PCIDSKChannel *poChannelIn )

{
    Initialize();

    poFile = poFileIn;
    poChannel = poChannelIn;

    nBlockXSize = static_cast<int>( poChannel->GetBlockWidth() );
    nBlockYSize = static_cast<int>( poChannel->GetBlockHeight() );

    eDataType = PCIDSK2Dataset::PCIDSKTypeToGDAL( poChannel->GetType() );

    if( !STARTS_WITH_CI(poChannel->GetDescription().c_str(),
                "Contents Not Specified") )
        GDALMajorObject::SetDescription( poChannel->GetDescription().c_str() );

/* -------------------------------------------------------------------- */
/*      Do we have overviews?                                           */
/* -------------------------------------------------------------------- */
    RefreshOverviewList();
}

/************************************************************************/
/*                            PCIDSK2Band()                             */
/*                                                                      */
/*      This constructor is used for overviews and bitmap segments      */
/*      as bands.                                                       */
/************************************************************************/

PCIDSK2Band::PCIDSK2Band( PCIDSKChannel *poChannelIn )

{
    Initialize();

    this->poChannel = poChannelIn;

    nBand = 1;

    nBlockXSize = static_cast<int>( poChannel->GetBlockWidth() );
    nBlockYSize = static_cast<int>( poChannel->GetBlockHeight() );

    nRasterXSize = static_cast<int>( poChannel->GetWidth() );
    nRasterYSize = static_cast<int>( poChannel->GetHeight() );

    eDataType = PCIDSK2Dataset::PCIDSKTypeToGDAL( poChannel->GetType() );

    if( poChannel->GetType() == CHN_BIT )
    {
        PCIDSK2Band::SetMetadataItem( "NBITS", "1", "IMAGE_STRUCTURE" );

        if( !STARTS_WITH_CI(poChannel->GetDescription().c_str(),
                    "Contents Not Specified") )
            GDALMajorObject::SetDescription( poChannel->GetDescription().c_str() );
    }
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void PCIDSK2Band::Initialize()

{
    papszLastMDListValue = nullptr;

    poChannel = nullptr;
    poFile = nullptr;
    poDS = nullptr;

    bCheckedForColorTable = false;
    poColorTable = nullptr;
    nPCTSegNumber = -1;

    papszCategoryNames = nullptr;
}

/************************************************************************/
/*                            ~PCIDSK2Band()                            */
/************************************************************************/

PCIDSK2Band::~PCIDSK2Band()

{
    while( !apoOverviews.empty() )
    {
        delete apoOverviews.back();
        apoOverviews.pop_back();
    }
    CSLDestroy( papszLastMDListValue );
    CSLDestroy( papszCategoryNames );

    delete poColorTable;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void PCIDSK2Band::SetDescription( const char *pszDescription )

{
    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set description on read-only file." );
        return;
    }

    try
    {
        poChannel->SetDescription( pszDescription );

        if( !STARTS_WITH_CI(poChannel->GetDescription().c_str(),
                    "Contents Not Specified") )
            GDALMajorObject::SetDescription( poChannel->GetDescription().c_str() );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/*                                                                      */
/*      Offer category names from Class_*_ metadata.                    */
/************************************************************************/

char **PCIDSK2Band::GetCategoryNames()

{
    // already scanned?
    if( papszCategoryNames != nullptr )
        return papszCategoryNames;

    try
    {
        std::vector<std::string> aosMDKeys = poChannel->GetMetadataKeys();
        int nClassCount = 0;
        constexpr int nMaxClasses = 10000;
        papszCategoryNames = reinterpret_cast<char **>(
            CPLCalloc(nMaxClasses+1, sizeof(char*) ) );

        for( size_t i = 0; i < aosMDKeys.size(); i++ )
        {
            CPLString osKey = aosMDKeys[i];

            // is this a "Class_n_name" keyword?
            if( !STARTS_WITH_CI(osKey, "Class_") )
                continue;

            if( !EQUAL(osKey.c_str() + osKey.size() - 5, "_name") )
                continue;

            // Ignore unreasonable class values.
            int iClass = atoi(osKey.c_str() + 6);

            if( iClass < 0 || iClass > 10000 )
                continue;

            // Fetch the name.
            CPLString osName  = poChannel->GetMetadataValue(osKey);

            // do we need to put in place dummy class names for missing values?
            if( iClass >= nClassCount )
            {
                while( iClass >= nClassCount )
                {
                    papszCategoryNames[nClassCount++] = CPLStrdup("");
                    papszCategoryNames[nClassCount] = nullptr;
                }
            }

            // Replace target category name.
            CPLFree( papszCategoryNames[iClass] );
            papszCategoryNames[iClass] = nullptr;

            papszCategoryNames[iClass] = CPLStrdup(osName);
        }

        if( nClassCount == 0 )
            return GDALPamRasterBand::GetCategoryNames();

        return papszCategoryNames;
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return nullptr;
    }
}

/************************************************************************/
/*                         CheckForColorTable()                         */
/************************************************************************/

bool PCIDSK2Band::CheckForColorTable()

{
    if( bCheckedForColorTable || poFile == nullptr )
        return true;

    bCheckedForColorTable = true;

    try
    {
/* -------------------------------------------------------------------- */
/*      Try to find an appropriate PCT segment to use.                  */
/* -------------------------------------------------------------------- */
        std::string osDefaultPCT = poChannel->GetMetadataValue("DEFAULT_PCT_REF");
        PCIDSKSegment *poPCTSeg = nullptr;

        // If there is no metadata, assume a single PCT in a file with only
        // one raster band must be intended for it.
        if( osDefaultPCT.empty()
            && poDS != nullptr
            && poDS->GetRasterCount() == 1 )
        {
            poPCTSeg = poFile->GetSegment( SEG_PCT, "" );
            if( poPCTSeg != nullptr
                && poFile->GetSegment( SEG_PCT, "",
                                       poPCTSeg->GetSegmentNumber() ) != nullptr )
                poPCTSeg = nullptr;
        }
        // Parse default PCT ref assuming an in file reference.
        else if( !osDefaultPCT.empty()
                 && strstr(osDefaultPCT.c_str(),"PCT:") != nullptr )
        {
            poPCTSeg = poFile->GetSegment(
                atoi(strstr(osDefaultPCT.c_str(),"PCT:") + 4) );
        }

        if( poPCTSeg != nullptr )
        {
            poColorTable = new GDALColorTable();
            unsigned char abyPCT[768];

            PCIDSK_PCT *poPCT = dynamic_cast<PCIDSK_PCT*>( poPCTSeg );
            if( poPCT )
            {
                nPCTSegNumber = poPCTSeg->GetSegmentNumber();

                poPCT->ReadPCT( abyPCT );

                for( int i = 0; i < 256; i++ )
                {
                    GDALColorEntry sEntry;

                    sEntry.c1 = abyPCT[256 * 0 + i];
                    sEntry.c2 = abyPCT[256 * 1 + i];
                    sEntry.c3 = abyPCT[256 * 2 + i];
                    sEntry.c4 = 255;
                    poColorTable->SetColorEntry( i, &sEntry );
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      If we did not find an appropriate PCT segment, check for        */
/*      Class_n color data from which to construct a color table.       */
/* -------------------------------------------------------------------- */
        std::vector<std::string> aosMDKeys = poChannel->GetMetadataKeys();

        for( size_t i=0; i < aosMDKeys.size(); i++ )
        {
            CPLString osKey = aosMDKeys[i];

            // is this a "Class_n_name" keyword?

            if( !STARTS_WITH_CI(osKey, "Class_") )
                continue;

            if( !EQUAL(osKey.c_str() + osKey.size() - 6, "_Color") )
                continue;

            // Ignore unreasonable class values.
            const int iClass = atoi(osKey.c_str() + 6);

            if( iClass < 0 || iClass > 10000 )
                continue;

            // Fetch and parse the RGB value "(RGB:red green blue)"
            CPLString osRGB  = poChannel->GetMetadataValue(osKey);

            if( !STARTS_WITH_CI(osRGB, "(RGB:") )
                continue;

            int nRed, nGreen, nBlue;
            if( sscanf( osRGB.c_str() + 5, "%d %d %d",
                        &nRed, &nGreen, &nBlue ) != 3 )
                continue;

            // we have an entry - apply to the color table.
            GDALColorEntry sEntry;

            sEntry.c1 = (short) nRed;
            sEntry.c2 = (short) nGreen;
            sEntry.c3 = (short) nBlue;
            sEntry.c4 = 255;

            if( poColorTable == nullptr )
            {
                CPLDebug( "PCIDSK", "Using Class_n_Color metadata for color table." );
                poColorTable = new GDALColorTable();
            }

            poColorTable->SetColorEntry( iClass, &sEntry );
        }
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return false;
    }

    return true;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *PCIDSK2Band::GetColorTable()

{
    CheckForColorTable();

    if( poColorTable )
        return poColorTable;

    return GDALPamRasterBand::GetColorTable();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr PCIDSK2Band::SetColorTable( GDALColorTable *poCT )

{
    if( !CheckForColorTable() )
        return CE_Failure;

    // no color tables on overviews.
    if( poFile == nullptr )
        return CE_Failure;

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set color table on read-only file." );
        return CE_Failure;
    }

    try
    {
/* -------------------------------------------------------------------- */
/*      Are we trying to delete the color table?                        */
/* -------------------------------------------------------------------- */
        if( poCT == nullptr )
        {
            delete poColorTable;
            poColorTable = nullptr;

            if( nPCTSegNumber != -1 )
                poFile->DeleteSegment( nPCTSegNumber );
            poChannel->SetMetadataValue( "DEFAULT_PCT_REF", "" );
            nPCTSegNumber = -1;

            return CE_None;
        }

/* -------------------------------------------------------------------- */
/*      Do we need to create the segment?  If so, also set the          */
/*      default pct metadata.                                           */
/* -------------------------------------------------------------------- */
        if( nPCTSegNumber == -1 )
        {
            nPCTSegNumber = poFile->CreateSegment( "PCTTable",
                                                   "Default Pseudo-Color Table",
                                                   SEG_PCT, 0 );

            CPLString osRef;
            osRef.Printf( "gdb:/{PCT:%d}", nPCTSegNumber );
            poChannel->SetMetadataValue( "DEFAULT_PCT_REF", osRef );
        }

/* -------------------------------------------------------------------- */
/*      Write out the PCT.                                              */
/* -------------------------------------------------------------------- */
        const int nColorCount = std::min(256, poCT->GetColorEntryCount());

        unsigned char abyPCT[768];
        memset( abyPCT, 0, 768 );

        for( int i = 0; i < nColorCount; i++ )
        {
            GDALColorEntry sEntry;

            poCT->GetColorEntryAsRGB( i, &sEntry );
            abyPCT[256 * 0 + i] = (unsigned char) sEntry.c1;
            abyPCT[256 * 1 + i] = (unsigned char) sEntry.c2;
            abyPCT[256 * 2 + i] = (unsigned char) sEntry.c3;
        }

        PCIDSK_PCT *poPCT = dynamic_cast<PCIDSK_PCT*>(
            poFile->GetSegment( nPCTSegNumber ) );
        if( poPCT )
            poPCT->WritePCT( abyPCT );

        delete poColorTable;
        poColorTable = poCT->Clone();
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp PCIDSK2Band::GetColorInterpretation()

{
    CheckForColorTable();

    if( poColorTable != nullptr )
        return GCI_PaletteIndex;

    return GDALPamRasterBand::GetColorInterpretation();
}

/************************************************************************/
/*                        RefreshOverviewList()                         */
/************************************************************************/

void PCIDSK2Band::RefreshOverviewList()

{
/* -------------------------------------------------------------------- */
/*      Clear existing overviews.                                       */
/* -------------------------------------------------------------------- */
    while( !apoOverviews.empty() )
    {
        delete apoOverviews.back();
        apoOverviews.pop_back();
    }

/* -------------------------------------------------------------------- */
/*      Fetch overviews.                                                */
/* -------------------------------------------------------------------- */
    for( int iOver = 0; iOver < poChannel->GetOverviewCount(); iOver++ )
    {
        auto poOvrBand = new PCIDSK2Band( poChannel->GetOverview(iOver) );
        poOvrBand->eAccess = eAccess;
        apoOverviews.push_back( poOvrBand );
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PCIDSK2Band::IReadBlock( int iBlockX, int iBlockY, void *pData )

{
    try
    {
        poChannel->ReadBlock( iBlockX + iBlockY * nBlocksPerRow,
                              pData );

        // Do we need to upsample 1bit to 8bit?
        if( poChannel->GetType() == CHN_BIT )
        {
            GByte *pabyData = reinterpret_cast<GByte *>( pData );

            for( int ii = nBlockXSize * nBlockYSize - 1; ii >= 0; ii-- )
            {
                if( (pabyData[ii>>3] & (0x80 >> (ii & 0x7))) )
                    pabyData[ii] = 1;
                else
                    pabyData[ii] = 0;
            }
        }

        return CE_None;
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr PCIDSK2Band::IWriteBlock( int iBlockX, int iBlockY, void *pData )

{
    try
    {
        poChannel->WriteBlock( iBlockX + iBlockY * nBlocksPerRow,
                               pData );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int PCIDSK2Band::GetOverviewCount()

{
    if( !apoOverviews.empty() )
        return static_cast<int>( apoOverviews.size() );

    return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *PCIDSK2Band::GetOverview(int iOverview)

{
    if( iOverview < 0 || iOverview >= static_cast<int>( apoOverviews.size() ) )
        return GDALPamRasterBand::GetOverview( iOverview );

    return apoOverviews[iOverview];
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr PCIDSK2Band::SetMetadata( char **papszMD,
                                 const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::SetMetadata( papszMD, pszDomain );

/* -------------------------------------------------------------------- */
/*      Set each item individually.                                     */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = nullptr;
    m_oCacheMetadataItem.clear();

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set metadata on read-only file." );
        return CE_Failure;
    }

    try
    {
        for( int iItem = 0; papszMD && papszMD[iItem]; iItem++ )
        {
            char *pszItemName = nullptr;

            const char *pszItemValue
                = CPLParseNameValue( papszMD[iItem], &pszItemName);
            if( pszItemName != nullptr )
            {
                poChannel->SetMetadataValue( pszItemName, pszItemValue );
                CPLFree( pszItemName );
            }
        }
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr PCIDSK2Band::SetMetadataItem( const char *pszName,
                                     const char *pszValue,
                                     const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::SetMetadataItem(pszName,pszValue,pszDomain);

/* -------------------------------------------------------------------- */
/*      Set on the file.                                                */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = nullptr;
    m_oCacheMetadataItem.clear();

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set metadata on read-only file." );
        return CE_Failure;
    }

    try
    {
        if( !pszValue )
          pszValue = "";
        poChannel->SetMetadataValue( pszName, pszValue );
    }
   catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **PCIDSK2Band::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamRasterBand::GetMetadataDomainList(),
                                   TRUE,
                                   "", nullptr);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *PCIDSK2Band::GetMetadataItem( const char *pszName,
                                          const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::GetMetadataItem( pszName, pszDomain );

/* -------------------------------------------------------------------- */
/*      Try and fetch (use cached value if available)                   */
/* -------------------------------------------------------------------- */
    auto oIter = m_oCacheMetadataItem.find(pszName);
    if( oIter != m_oCacheMetadataItem.end() )
    {
        return oIter->second.empty() ? nullptr : oIter->second.c_str();
    }

    CPLString osValue;
    try
    {
        osValue = poChannel->GetMetadataValue( pszName );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "%s", ex.what() );
        return nullptr;
    }

    oIter = m_oCacheMetadataItem.insert(
        std::pair<std::string, std::string>(pszName, osValue)).first;
    return oIter->second.empty() ? nullptr : oIter->second.c_str();
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **PCIDSK2Band::GetMetadata( const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamRasterBand::GetMetadata( pszDomain );

/* -------------------------------------------------------------------- */
/*      If we have a cached result, just use that.                      */
/* -------------------------------------------------------------------- */
    if( papszLastMDListValue != nullptr )
        return papszLastMDListValue;

/* -------------------------------------------------------------------- */
/*      Fetch and build the list.                                       */
/* -------------------------------------------------------------------- */
    try
    {
        std::vector<std::string> aosKeys = poChannel->GetMetadataKeys();

        for( unsigned int i = 0; i < aosKeys.size(); i++ )
        {
            if( aosKeys[i].c_str()[0] == '_' )
                continue;

            papszLastMDListValue =
                CSLSetNameValue( papszLastMDListValue,
                                 aosKeys[i].c_str(),
                                 poChannel->GetMetadataValue(aosKeys[i]).c_str() );
        }
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return nullptr;
    }

    return papszLastMDListValue;
}

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSK2Dataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           PCIDSK2Dataset()                            */
/************************************************************************/

PCIDSK2Dataset::PCIDSK2Dataset() :
    papszLastMDListValue(nullptr),
    poFile(nullptr)
{}

/************************************************************************/
/*                            ~PCIDSK2Dataset()                          */
/************************************************************************/

// FIXME? is an exception can really be thrown in the destructor, then it is very dangerous !
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4702 )  /*  unreachable code */
#endif
PCIDSK2Dataset::~PCIDSK2Dataset()
{
    PCIDSK2Dataset::FlushCache(true);

    while( !apoLayers.empty() )
    {
        delete apoLayers.back();
        apoLayers.pop_back();
    }

    if( m_poSRS )
        m_poSRS->Release();

    try {
        if( poFile != nullptr)
            delete poFile;
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK SDK Failure in Close(), unexpected exception." );
    }

    CSLDestroy( papszLastMDListValue );
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **PCIDSK2Dataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();
    CPLString osBaseDir = CPLGetPath( GetDescription() );

    try
    {
        for( int nChan = 1; nChan <= poFile->GetChannels(); nChan++ )
        {
            PCIDSKChannel *poChannel = poFile->GetChannel( nChan );
            CPLString osChanFilename;
            uint64 image_offset, pixel_offset, line_offset;
            bool little_endian;

            poChannel->GetChanInfo( osChanFilename, image_offset,
                                    pixel_offset, line_offset, little_endian );

            if( osChanFilename != "" )
            {
                papszFileList =
                    CSLAddString( papszFileList,
                                  CPLProjectRelativeFilename( osBaseDir,
                                                              osChanFilename ) );
            }
        }

        return papszFileList;
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return papszFileList;
    }
}

/************************************************************************/
/*                             ProcessRPC()                             */
/************************************************************************/

void PCIDSK2Dataset::ProcessRPC()

{
/* -------------------------------------------------------------------- */
/*      Search all BIN segments looking for an RPC segment.             */
/* -------------------------------------------------------------------- */
    PCIDSKSegment *poSeg = poFile->GetSegment( SEG_BIN, "" );
    PCIDSKRPCSegment *poRPCSeg = nullptr;

    while( poSeg != nullptr
           && (poRPCSeg = dynamic_cast<PCIDSKRPCSegment*>( poSeg )) == nullptr )

    {
        poSeg = poFile->GetSegment( SEG_BIN, "",
                                    poSeg->GetSegmentNumber() );
    }

    if( poRPCSeg == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Turn RPC segment into GDAL RFC 22 style metadata.               */
/* -------------------------------------------------------------------- */
    try
    {
        CPLString osValue;
        double dfLineOffset, dfLineScale, dfSampOffset, dfSampScale;
        double dfLatOffset, dfLatScale,
            dfLongOffset, dfLongScale,
            dfHeightOffset, dfHeightScale;

        poRPCSeg->GetRPCTranslationCoeffs(
            dfLongOffset, dfLongScale,
            dfLatOffset, dfLatScale,
            dfHeightOffset, dfHeightScale,
            dfSampOffset, dfSampScale,
            dfLineOffset, dfLineScale );

        osValue.Printf( "%.16g", dfLineOffset );
        GDALPamDataset::SetMetadataItem( "LINE_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLineScale );
        GDALPamDataset::SetMetadataItem( "LINE_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfSampOffset );
        GDALPamDataset::SetMetadataItem( "SAMP_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfSampScale );
        GDALPamDataset::SetMetadataItem( "SAMP_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLongOffset );
        GDALPamDataset::SetMetadataItem( "LONG_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLongScale );
        GDALPamDataset::SetMetadataItem( "LONG_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLatOffset );
        GDALPamDataset::SetMetadataItem( "LAT_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfLatScale );
        GDALPamDataset::SetMetadataItem( "LAT_SCALE", osValue, "RPC" );

        osValue.Printf( "%.16g", dfHeightOffset );
        GDALPamDataset::SetMetadataItem( "HEIGHT_OFF", osValue, "RPC" );

        osValue.Printf( "%.16g", dfHeightScale );
        GDALPamDataset::SetMetadataItem( "HEIGHT_SCALE", osValue, "RPC" );

        if( poRPCSeg->GetXNumerator().size() != 20
            || poRPCSeg->GetXDenominator().size() != 20
            || poRPCSeg->GetYNumerator().size() != 20
            || poRPCSeg->GetYDenominator().size() != 20 )
        {
            GDALPamDataset::SetMetadata( nullptr, "RPC" );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Did not get 20 values in the RPC coefficients lists." );
            return;
        }

        std::vector<double> adfCoef = poRPCSeg->GetYNumerator();
        CPLString osCoefList = "";
        for( int i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "LINE_NUM_COEFF", osCoefList, "RPC" );

        adfCoef = poRPCSeg->GetYDenominator();
        osCoefList = "";
        for( int i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "LINE_DEN_COEFF", osCoefList, "RPC" );

        adfCoef = poRPCSeg->GetXNumerator();
        osCoefList = "";
        for( int i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "SAMP_NUM_COEFF", osCoefList, "RPC" );

        adfCoef = poRPCSeg->GetXDenominator();
        osCoefList = "";
        for( int i = 0; i < 20; i++ )
        {
            osValue.Printf( "%.16g ", adfCoef[i] );
            osCoefList += osValue;
        }
        GDALPamDataset::SetMetadataItem( "SAMP_DEN_COEFF", osCoefList, "RPC" );
    }
    catch( const PCIDSKException& ex )
    {
        GDALPamDataset::SetMetadata( nullptr, "RPC" );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void PCIDSK2Dataset::FlushCache(bool bAtClosing)

{
    GDALPamDataset::FlushCache(bAtClosing);

    if( poFile )
    {
        try {
            poFile->Synchronize();
        }
        catch( const PCIDSKException& ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
        }
    }
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetMetadata( char **papszMD,
                                    const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamDataset::SetMetadata( papszMD, pszDomain );

/* -------------------------------------------------------------------- */
/*      Set each item individually.                                     */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = nullptr;
    m_oCacheMetadataItem.clear();

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set metadata on read-only file." );
        return CE_Failure;
    }

    try
    {
        for( int iItem = 0; papszMD && papszMD[iItem]; iItem++ )
        {
            char *pszItemName = nullptr;
            const char *pszItemValue
                = CPLParseNameValue( papszMD[iItem], &pszItemName);
            if( pszItemName != nullptr )
            {
                poFile->SetMetadataValue( pszItemName, pszItemValue );
                CPLFree( pszItemName );
            }
        }
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetMetadataItem( const char *pszName,
                                        const char *pszValue,
                                        const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamDataset::SetMetadataItem( pszName, pszValue, pszDomain );

/* -------------------------------------------------------------------- */
/*      Set on the file.                                                */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszLastMDListValue );
    papszLastMDListValue = nullptr;
    m_oCacheMetadataItem.clear();

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set metadata on read-only file." );
        return CE_Failure;
    }

    try
    {
        poFile->SetMetadataValue( pszName, pszValue );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **PCIDSK2Dataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "", nullptr);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *PCIDSK2Dataset::GetMetadataItem( const char *pszName,
                                             const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamDataset::GetMetadataItem( pszName, pszDomain );

/* -------------------------------------------------------------------- */
/*      Try and fetch (use cached value if available)                   */
/* -------------------------------------------------------------------- */
    auto oIter = m_oCacheMetadataItem.find(pszName);
    if( oIter != m_oCacheMetadataItem.end() )
    {
        return oIter->second.empty() ? nullptr : oIter->second.c_str();
    }

    CPLString osValue;
    try
    {
        osValue = poFile->GetMetadataValue( pszName );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "%s", ex.what() );
        return nullptr;
    }

    oIter = m_oCacheMetadataItem.insert(
        std::pair<std::string, std::string>(pszName, osValue)).first;
    return oIter->second.empty() ? nullptr : oIter->second.c_str();
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **PCIDSK2Dataset::GetMetadata( const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      PCIDSK only supports metadata in the default domain.            */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        return GDALPamDataset::GetMetadata( pszDomain );

/* -------------------------------------------------------------------- */
/*      If we have a cached result, just use that.                      */
/* -------------------------------------------------------------------- */
    if( papszLastMDListValue != nullptr )
        return papszLastMDListValue;

/* -------------------------------------------------------------------- */
/*      Fetch and build the list.                                       */
/* -------------------------------------------------------------------- */
    try
    {
        std::vector<std::string> aosKeys = poFile->GetMetadataKeys();

        for( unsigned int i = 0; i < aosKeys.size(); i++ )
        {
            if( aosKeys[i].c_str()[0] == '_' )
                continue;

            papszLastMDListValue =
                CSLSetNameValue( papszLastMDListValue,
                                 aosKeys[i].c_str(),
                                 poFile->GetMetadataValue(aosKeys[i]).c_str() );
        }
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return nullptr;
    }

    return papszLastMDListValue;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetGeoTransform( double * padfTransform )
{
    PCIDSKGeoref *poGeoref = nullptr;
    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( const PCIDSKException& )
    {
        // I should really check whether this is an expected issue.
    }

    if( poGeoref == nullptr )
        return GDALPamDataset::SetGeoTransform( padfTransform );

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set GeoTransform on read-only file." );
        return CE_Failure;
    }

    try
    {
        poGeoref->WriteSimple( poGeoref->GetGeosys(),
                               padfTransform[0],
                               padfTransform[1],
                               padfTransform[2],
                               padfTransform[3],
                               padfTransform[4],
                               padfTransform[5] );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::GetGeoTransform( double * padfTransform )
{
    PCIDSKGeoref *poGeoref = nullptr;
    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( const PCIDSKException& )
    {
        // I should really check whether this is an expected issue.
    }

    if( poGeoref != nullptr )
    {
        try
        {
            poGeoref->GetTransform( padfTransform[0],
                                    padfTransform[1],
                                    padfTransform[2],
                                    padfTransform[3],
                                    padfTransform[4],
                                    padfTransform[5] );
        }
        catch( const PCIDSKException& ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
            return CE_Failure;
        }

        // If we got anything non-default return it.
        if( padfTransform[0] != 0.0
            || padfTransform[1] != 1.0
            || padfTransform[2] != 0.0
            || padfTransform[3] != 0.0
            || padfTransform[4] != 0.0
            || padfTransform[5] != 1.0 )
            return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Check for worldfile if we have no other georeferencing.         */
/* -------------------------------------------------------------------- */
    if( GDALReadWorldFile( GetDescription(), "pxw",
                           padfTransform ) )
        return CE_None;

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr PCIDSK2Dataset::SetSpatialRef( const OGRSpatialReference* poSRS )

{
    PCIDSKGeoref *poGeoref = nullptr;

    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( const PCIDSKException& )
    {
        // I should really check whether this is an expected issue.
    }

    if( poGeoref == nullptr )
    {
        return GDALPamDataset::SetSpatialRef( poSRS );
    }

    char *pszGeosys = nullptr;
    char *pszUnits = nullptr;
    double *padfPrjParams = nullptr;

    if( poSRS == nullptr ||
        poSRS->exportToPCI( &pszGeosys, &pszUnits,
                             &padfPrjParams ) != OGRERR_NONE )
    {
        return GDALPamDataset::SetSpatialRef( poSRS );
    }

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set projection on read-only file." );
        CPLFree( pszGeosys );
        CPLFree( pszUnits );
        CPLFree( padfPrjParams );
        return CE_Failure;
    }

    try
    {
        double adfGT[6];
        poGeoref->GetTransform( adfGT[0], adfGT[1], adfGT[2],
                                adfGT[3], adfGT[4], adfGT[5] );

        poGeoref->WriteSimple( pszGeosys,
                               adfGT[0], adfGT[1], adfGT[2],
                               adfGT[3], adfGT[4], adfGT[5] );

        std::vector<double> adfPCIParameters;
        for( unsigned int i = 0; i < 17; i++ )
            adfPCIParameters.push_back( padfPrjParams[i] );

        if( STARTS_WITH_CI(pszUnits, "FOOT") )
                adfPCIParameters.push_back(
                    static_cast<double>( static_cast<int>(
                        PCIDSK::UNIT_US_FOOT ) ) );
        else if( EQUALN(pszUnits, "INTL FOOT", 9) )
            adfPCIParameters.push_back(
                static_cast<double>( static_cast<int>(
                    PCIDSK::UNIT_INTL_FOOT ) ) );
        else if( EQUALN(pszUnits,"DEGREE",6) )
            adfPCIParameters.push_back(
                static_cast<double>( static_cast<int>(
                    PCIDSK::UNIT_DEGREE ) ) );
        else
            adfPCIParameters.push_back(
                static_cast<double>( static_cast<int>(
                    PCIDSK::UNIT_METER ) ) );

        poGeoref->WriteParameters( adfPCIParameters );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        return CE_Failure;
    }

    CPLFree( pszGeosys );
    CPLFree( pszUnits );
    CPLFree( padfPrjParams );

    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *PCIDSK2Dataset::GetSpatialRef() const
{
    if( m_poSRS )
        return m_poSRS;

    PCIDSKGeoref *poGeoref = nullptr;

    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( const PCIDSKException& )
    {
        // I should really check whether this is an expected issue.
    }

    if( poGeoref == nullptr )
    {
        return GDALPamDataset::GetSpatialRef();
    }

    CPLString osGeosys;
    const char *pszUnits = nullptr;

    std::vector<double> adfParameters;
    adfParameters.resize(18);

    try
    {
        osGeosys = poGeoref->GetGeosys();
        adfParameters = poGeoref->GetParameters();
        const UnitCode code
            = static_cast<UnitCode>( static_cast<int>( adfParameters[16] ) );

        if( code == PCIDSK::UNIT_DEGREE )
            pszUnits = "DEGREE";
        else if( code == PCIDSK::UNIT_METER )
            pszUnits = "METER";
        else if( code == PCIDSK::UNIT_US_FOOT )
            pszUnits = "FOOT";
        else if( code == PCIDSK::UNIT_INTL_FOOT )
            pszUnits = "INTL FOOT";
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }

    OGRSpatialReference oSRS;
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( oSRS.importFromPCI( osGeosys, pszUnits,
                            &(adfParameters[0]) ) == OGRERR_NONE )
    {
        m_poSRS = oSRS.Clone();
        return m_poSRS;
    }
    else
    {
        return GDALPamDataset::GetSpatialRef();
    }
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::IBuildOverviews( const char *pszResampling,
                                        int nOverviews, int *panOverviewList,
                                        int nListBands, int *panBandList,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData )

{
    PCIDSK2Band *poBand = reinterpret_cast<PCIDSK2Band*>(
        GetRasterBand( panBandList[0] ) );

/* -------------------------------------------------------------------- */
/*      If RRD overviews requested, then invoke generic handling.       */
/* -------------------------------------------------------------------- */
    bool bUseGenericHandling = false;

    if( CPLTestBool(CPLGetConfigOption( "USE_RRD", "NO" ) ) )
    {
        bUseGenericHandling = true;
    }

/* -------------------------------------------------------------------- */
/*      If we don't have read access, then create the overviews         */
/*      externally.                                                     */
/* -------------------------------------------------------------------- */
    if( GetAccess() != GA_Update )
    {
        CPLDebug( "PCIDSK",
                  "File open for read-only accessing, "
                  "creating overviews externally." );

        bUseGenericHandling = true;
    }

    if( bUseGenericHandling )
    {
        if( poBand->GetOverviewCount() != 0 )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Cannot add external overviews when there are already "
                "internal overviews" );
            return CE_Failure;
        }

        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList,
            nListBands, panBandList, pfnProgress, pProgressData );
    }

    if( nListBands == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Currently no support for clearing overviews.                    */
/* -------------------------------------------------------------------- */
    if( nOverviews == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK2 driver does not currently support clearing existing overviews. " );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */

    int nNewOverviews = 0;
    int *panNewOverviewList = reinterpret_cast<int *>(
        CPLCalloc( sizeof( int ), nOverviews ) );
    for( int i = 0; i < nOverviews && poBand != nullptr; i++ )
    {
        for( int j = 0; j < poBand->GetOverviewCount(); j++ )
        {
            GDALRasterBand * poOverview = poBand->GetOverview( j );

            int nOvFactor = GDALComputeOvFactor(poOverview->GetXSize(),
                                                poBand->GetXSize(),
                                                poOverview->GetYSize(),
                                                poBand->GetYSize());

            if( nOvFactor == panOverviewList[i]
                || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                    poBand->GetXSize(),
                                                    poBand->GetYSize() ) )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
            panNewOverviewList[nNewOverviews++] = panOverviewList[i];
        else
            panOverviewList[i] *= -1;
    }

/* -------------------------------------------------------------------- */
/*      Create the overviews that are missing.                          */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nNewOverviews; i++ )
    {
        try
        {
            // conveniently our resampling values mostly match PCIDSK.
            poFile->CreateOverviews( nListBands, panBandList,
                                     panNewOverviewList[i], pszResampling );
        }
        catch( const PCIDSKException& ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
            CPLFree( panNewOverviewList );
            return CE_Failure;
        }
    }

    CPLFree( panNewOverviewList );
    panNewOverviewList = nullptr;

    for( int iBand = 0; iBand < nListBands; iBand++ )
    {
        poBand = reinterpret_cast<PCIDSK2Band *>(
            GetRasterBand( panBandList[iBand] ) );
        reinterpret_cast<PCIDSK2Band *>( poBand )->RefreshOverviewList();
    }

/* -------------------------------------------------------------------- */
/*      Actually generate the overview imagery.                         */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    std::vector<int> anRegenLevels;

    GDALRasterBand **papoOverviewBands = reinterpret_cast<GDALRasterBand **>(
        CPLCalloc( sizeof(void*), nOverviews ) );

    for( int iBand = 0; iBand < nListBands && eErr == CE_None; iBand++ )
    {
        nNewOverviews = 0;

        poBand = reinterpret_cast<PCIDSK2Band *>(
            GetRasterBand( panBandList[iBand] ) );

        for( int i = 0; i < nOverviews && poBand != nullptr; i++ )
        {
            for( int j = 0; j < poBand->GetOverviewCount(); j++ )
            {
                GDALRasterBand * poOverview = poBand->GetOverview( j );

                int nOvFactor = GDALComputeOvFactor(poOverview->GetXSize(),
                                                    poBand->GetXSize(),
                                                    poOverview->GetYSize(),
                                                    poBand->GetYSize());

                if( nOvFactor == panOverviewList[i]
                    || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                       poBand->GetXSize(),
                                                       poBand->GetYSize() ) )
                {
                    papoOverviewBands[nNewOverviews++] = poOverview;
                    anRegenLevels.push_back( j );
                    break;
                }
            }
        }

        if( nNewOverviews > 0 )
        {
            eErr = GDALRegenerateOverviews( (GDALRasterBandH) poBand,
                                            nNewOverviews,
                                            reinterpret_cast<GDALRasterBandH*>( papoOverviewBands ),
                                            pszResampling,
                                            pfnProgress, pProgressData );

            // Mark the regenerated overviews as valid.
            for( int i = 0; i < static_cast<int>( anRegenLevels.size() ); i++ )
                poBand->poChannel->SetOverviewValidity( anRegenLevels[i],
                                                        true );
        }
    }

    CPLFree(papoOverviewBands);

    return eErr;
}

/************************************************************************/
/*                         PCIDSKTypeToGDAL()                           */
/************************************************************************/

GDALDataType PCIDSK2Dataset::PCIDSKTypeToGDAL( eChanType eType )
{
    switch( eType )
    {
      case CHN_8U:
        return GDT_Byte;

      case CHN_16U:
        return GDT_UInt16;

      case CHN_16S:
        return GDT_Int16;

      case CHN_32R:
        return GDT_Float32;

      case CHN_BIT:
        return GDT_Byte;

      case CHN_C16U:
        return GDT_CInt16;

      case CHN_C16S:
        return GDT_CInt16;

      case CHN_C32R:
        return GDT_CFloat32;

      default:
        return GDT_Unknown;
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PCIDSK2Dataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 512
        || !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader, "PCIDSK  ") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PCIDSK2Dataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify( poOpenInfo ) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Try opening the file.                                           */
/* -------------------------------------------------------------------- */
    PCIDSKFile *poFile = nullptr;
    const int nMaxBandCount = atoi(CPLGetConfigOption("GDAL_MAX_BAND_COUNT", "65536"));
    try {
        poFile =
            PCIDSK::Open( poOpenInfo->pszFilename,
                          poOpenInfo->eAccess == GA_ReadOnly ? "r" : "r+",
                          PCIDSK2GetInterfaces(),
                          nMaxBandCount );
        if( poFile == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to re-open %s within PCIDSK driver.\n",
                      poOpenInfo->pszFilename );
            return nullptr;
        }

        const bool bValidRasterDimensions = poFile->GetWidth() &&
                                            poFile->GetHeight();
        if( !bValidRasterDimensions &&
            (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 )
        {
            delete poFile;
            return nullptr;
        }

        /* Check if this is a vector-only PCIDSK file and that we are */
        /* opened in raster-only mode */
        if( poOpenInfo->eAccess == GA_ReadOnly &&
            (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 &&
            poFile->GetChannels() == 0 &&
            poFile->GetSegment( PCIDSK::SEG_VEC, "" ) != nullptr )
        {
            CPLDebug("PCIDSK", "This is a vector-only PCIDSK dataset, "
                     "but it has been opened in read-only in raster-only mode");
            delete poFile;
            return nullptr;
        }
        /* Reverse test */
        if( poOpenInfo->eAccess == GA_ReadOnly &&
            (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
            poFile->GetChannels() != 0 &&
            poFile->GetSegment( PCIDSK::SEG_VEC, "" ) == nullptr )
        {
            CPLDebug("PCIDSK", "This is a raster-only PCIDSK dataset, "
                     "but it has been opened in read-only in vector-only mode");
            delete poFile;
            return nullptr;
        }

        return LLOpen( poOpenInfo->pszFilename, poFile, poOpenInfo->eAccess,
                       poOpenInfo->GetSiblingFiles() );
    }
/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
        delete poFile;
        return nullptr;
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK::Create() failed, unexpected exception." );
        delete poFile;
        return nullptr;
    }
}

/************************************************************************/
/*                               LLOpen()                               */
/*                                                                      */
/*      Low level variant of open that takes the preexisting            */
/*      PCIDSKFile.                                                     */
/************************************************************************/

GDALDataset *PCIDSK2Dataset::LLOpen( const char *pszFilename,
                                     PCIDSK::PCIDSKFile *poFile,
                                     GDALAccess eAccessIn,
                                     char** papszSiblingFiles )

{
    PCIDSK2Dataset *poDS = new PCIDSK2Dataset();
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    poDS->poFile = poFile;
    poDS->eAccess = eAccessIn;
    poDS->nRasterXSize = poFile->GetWidth();
    poDS->nRasterYSize = poFile->GetHeight();

    const bool bValidRasterDimensions = poFile->GetWidth() &&
                                        poFile->GetHeight();
    if( !bValidRasterDimensions )
    {
        poDS->nRasterXSize = 512;
        poDS->nRasterYSize = 512;
    }

    try {

/* -------------------------------------------------------------------- */
/*      Are we specifically PIXEL or BAND interleaving?                 */
/*                                                                      */
/*      We don't set anything for FILE since it is harder to know if    */
/*      this is tiled or what the on disk interleaving is.              */
/* -------------------------------------------------------------------- */
        if( EQUAL(poFile->GetInterleaving().c_str(),"PIXEL") )
            poDS->SetMetadataItem( "IMAGE_STRUCTURE", "PIXEL",
                                   "IMAGE_STRUCTURE" );
        else if( EQUAL(poFile->GetInterleaving().c_str(),"BAND") )
            poDS->SetMetadataItem( "IMAGE_STRUCTURE", "BAND",
                                   "IMAGE_STRUCTURE" );

/* -------------------------------------------------------------------- */
/*      Create band objects.                                            */
/* -------------------------------------------------------------------- */
        for( int iBand = 0; bValidRasterDimensions &&
                            iBand < poFile->GetChannels(); iBand++ )
        {
            PCIDSKChannel* poChannel = poFile->GetChannel( iBand + 1 );
            if (poChannel->GetBlockWidth() <= 0 ||
                poChannel->GetBlockHeight() <= 0)
            {
                delete poDS;
                return nullptr;
            }

            if( PCIDSK2Dataset::PCIDSKTypeToGDAL( poChannel->GetType() )
                    == GDT_Unknown )
            {
                continue;
            }

            poDS->SetBand( poDS->GetRasterCount() + 1,
                new PCIDSK2Band( poFile, poChannel ));
        }

/* -------------------------------------------------------------------- */
/*      Create band objects for bitmap segments.                        */
/* -------------------------------------------------------------------- */
        int nLastBitmapSegment = 0;
        PCIDSKSegment *poBitSeg = nullptr;

        while( bValidRasterDimensions &&
               (poBitSeg = poFile->GetSegment( SEG_BIT, "",
                                               nLastBitmapSegment)) != nullptr )
        {
            PCIDSKChannel *poChannel =
                dynamic_cast<PCIDSKChannel*>( poBitSeg );
            if (poChannel == nullptr ||
                poChannel->GetBlockWidth() <= 0 ||
                poChannel->GetBlockHeight() <= 0)
            {
                delete poDS;
                return nullptr;
            }

            if( PCIDSK2Dataset::PCIDSKTypeToGDAL( poChannel->GetType() )
                    == GDT_Unknown )
            {
                continue;
            }

            poDS->SetBand( poDS->GetRasterCount()+1,
                           new PCIDSK2Band( poChannel ) );

            nLastBitmapSegment = poBitSeg->GetSegmentNumber();
        }

/* -------------------------------------------------------------------- */
/*      Create vector layers from vector segments.                      */
/* -------------------------------------------------------------------- */
        PCIDSK::PCIDSKSegment *segobj
            = poFile->GetSegment( PCIDSK::SEG_VEC, "" );
        for( ;
             segobj != nullptr;
             segobj = poFile->GetSegment( PCIDSK::SEG_VEC, "",
                                          segobj->GetSegmentNumber() ) )
        {
            PCIDSK::PCIDSKVectorSegment* poVecSeg = dynamic_cast<PCIDSK::PCIDSKVectorSegment*>( segobj );
            if( poVecSeg )
                poDS->apoLayers.push_back( new OGRPCIDSKLayer( segobj, poVecSeg, eAccessIn == GA_Update ) );
        }

/* -------------------------------------------------------------------- */
/*      Process RPC segment, if there is one.                           */
/* -------------------------------------------------------------------- */
        poDS->ProcessRPC();

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        poDS->SetDescription( pszFilename );
        poDS->TryLoadXML( papszSiblingFiles );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
        poDS->oOvManager.Initialize( poDS, pszFilename, papszSiblingFiles );

        return poDS;
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK SDK Failure in Open(), unexpected exception." );
    }

/* -------------------------------------------------------------------- */
/*      In case of exception, close dataset                             */
/* -------------------------------------------------------------------- */
    delete poDS;

    return nullptr;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PCIDSK2Dataset::Create( const char * pszFilename,
                                     int nXSize, int nYSize, int nBandsIn,
                                     GDALDataType eType,
                                     char **papszParamList )

{
/* -------------------------------------------------------------------- */
/*      Prepare channel type list.                                      */
/* -------------------------------------------------------------------- */
    std::vector<eChanType> aeChanTypes;

    if( eType == GDT_Float32 )
      aeChanTypes.resize( std::max(1, nBandsIn), CHN_32R );
    else if( eType == GDT_Int16 )
        aeChanTypes.resize( std::max(1, nBandsIn), CHN_16S );
    else if( eType == GDT_UInt16 )
        aeChanTypes.resize( std::max(1, nBandsIn), CHN_16U );
    else if( eType == GDT_CInt16 )
        aeChanTypes.resize( std::max(1, nBandsIn), CHN_C16S );
    else if( eType == GDT_CFloat32 )
        aeChanTypes.resize( std::max(1, nBandsIn), CHN_C32R );
    else
        aeChanTypes.resize( std::max(1, nBandsIn), CHN_8U );

/* -------------------------------------------------------------------- */
/*      Reformat options.  Currently no support for jpeg compression    */
/*      quality.                                                        */
/* -------------------------------------------------------------------- */
    CPLString osOptions;
    const char *pszValue = CSLFetchNameValue( papszParamList, "INTERLEAVING" );
    if( pszValue == nullptr )
        pszValue = "BAND";

    osOptions = pszValue;

    if( osOptions == "TILED" )
    {
        pszValue = CSLFetchNameValue( papszParamList, "TILESIZE" );
        if( pszValue != nullptr )
            osOptions += pszValue;

        pszValue = CSLFetchNameValue( papszParamList, "COMPRESSION" );
        if( pszValue != nullptr )
        {
            osOptions += " ";
            osOptions += pszValue;
        }

        pszValue = CSLFetchNameValue( papszParamList, "TILEVERSION" );
        if( pszValue != nullptr )
        {
            osOptions += " TILEV";
            osOptions += pszValue;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try creation.                                                   */
/* -------------------------------------------------------------------- */

    try {
        if( nBandsIn == 0 )
        {
            nXSize = 512;
            nYSize = 512;
        }
        PCIDSKFile *poFile = PCIDSK::Create( pszFilename, nXSize, nYSize, nBandsIn,
                                             &(aeChanTypes[0]), osOptions,
                                             PCIDSK2GetInterfaces() );

/* -------------------------------------------------------------------- */
/*      Apply band descriptions, if provided as creation options.       */
/* -------------------------------------------------------------------- */
        for( size_t i = 0;
             papszParamList != nullptr && papszParamList[i] != nullptr;
             i++ )
        {
            if( STARTS_WITH_CI(papszParamList[i], "BANDDESC") )
            {
                int nBand = atoi(papszParamList[i] + 8 );
                const char *pszDescription = strstr(papszParamList[i],"=");
                if( pszDescription && nBand > 0 && nBand <= nBandsIn )
                {
                    poFile->GetChannel(nBand)->SetDescription( pszDescription+1 );
                }
            }
        }

        return LLOpen( pszFilename, poFile, GA_Update );
    }
/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", ex.what() );
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK::Create() failed, unexpected exception." );
    }

    return nullptr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int PCIDSK2Dataset::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return eAccess == GA_Update;
    if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return eAccess == GA_Update;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *PCIDSK2Dataset::GetLayer( int iLayer )

{
  if( iLayer < 0 || iLayer >= static_cast<int>( apoLayers.size() ) )
        return nullptr;

    return apoLayers[iLayer];
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
PCIDSK2Dataset::ICreateLayer( const char * pszLayerName,
                              OGRSpatialReference *poSRS,
                              OGRwkbGeometryType eType,
                              CPL_UNUSED char ** papszOptions )
{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( eAccess != GA_Update )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  GetDescription(), pszLayerName );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Figure out what type of layer we need.                          */
/* -------------------------------------------------------------------- */
    std::string osLayerType;

    switch( wkbFlatten(eType) )
    {
      case wkbPoint:
        osLayerType = "POINTS";
        break;

      case wkbLineString:
        osLayerType = "ARCS";
        break;

      case wkbPolygon:
        osLayerType = "WHOLE_POLYGONS";
        break;

      case wkbNone:
        osLayerType = "TABLE";
        break;

      default:
        break;
    }

/* -------------------------------------------------------------------- */
/*      Create the segment.                                             */
/* -------------------------------------------------------------------- */
    int nSegNum;
    try
    {
        nSegNum = poFile->CreateSegment( pszLayerName, "",
                                             PCIDSK::SEG_VEC, 0L );
    }
    catch( const PCIDSKException& ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "%s", ex.what() );
        return nullptr;
    }
    PCIDSK::PCIDSKSegment *poSeg = poFile->GetSegment( nSegNum );
    PCIDSK::PCIDSKVectorSegment *poVecSeg =
        dynamic_cast<PCIDSK::PCIDSKVectorSegment*>( poSeg );
    if( poVecSeg == nullptr )
        return nullptr;

    if( osLayerType != "" )
        poSeg->SetMetadataValue( "LAYER_TYPE", osLayerType );

/* -------------------------------------------------------------------- */
/*      Do we need to apply a coordinate system?                        */
/* -------------------------------------------------------------------- */
    char *pszGeosys = nullptr;
    char *pszUnits = nullptr;
    double *padfPrjParams = nullptr;

    if( poSRS != nullptr
        && poSRS->exportToPCI( &pszGeosys, &pszUnits,
                               &padfPrjParams ) == OGRERR_NONE )
    {
        try
        {
            std::vector<double> adfPCIParameters;

            for( int i = 0; i < 17; i++ )
                adfPCIParameters.push_back( padfPrjParams[i] );

            if( STARTS_WITH_CI(pszUnits, "FOOT") )
                adfPCIParameters.push_back(
                    static_cast<double>( static_cast<int>(
                        PCIDSK::UNIT_US_FOOT ) ) );
            else if( STARTS_WITH_CI(pszUnits, "INTL FOOT") )
                adfPCIParameters.push_back(
                    static_cast<double>( static_cast<int>(
                        PCIDSK::UNIT_INTL_FOOT ) ) );
            else if( STARTS_WITH_CI(pszUnits, "DEGREE") )
                adfPCIParameters.push_back(
                    static_cast<double>( static_cast<int>(
                        PCIDSK::UNIT_DEGREE ) ) );
            else
                adfPCIParameters.push_back(
                    static_cast<double>( static_cast<int>(
                        PCIDSK::UNIT_METER ) ) );

            poVecSeg->SetProjection( pszGeosys, adfPCIParameters );
        }
        catch( const PCIDSKException& ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
        }

        CPLFree( pszGeosys );
        CPLFree( pszUnits );
        CPLFree( padfPrjParams );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */

    apoLayers.push_back( new OGRPCIDSKLayer( poSeg, poVecSeg, TRUE ) );

    return apoLayers.back();
}

/************************************************************************/
/*                        GDALRegister_PCIDSK()                         */
/************************************************************************/

void GDALRegister_PCIDSK()

{
    if( GDALGetDriverByName( "PCIDSK" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "PCIDSK" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "PCIDSK Database File" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/pcidsk.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "pix" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 Float32 CInt16 CFloat32" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='INTERLEAVING' type='string-select' default='BAND' description='raster data organization'>"
"       <Value>PIXEL</Value>"
"       <Value>BAND</Value>"
"       <Value>FILE</Value>"
"       <Value>TILED</Value>"
"   </Option>"
"   <Option name='COMPRESSION' type='string-select' default='NONE' description='compression - (INTERLEAVING=TILED only)'>"
"       <Value>NONE</Value>"
"       <Value>RLE</Value>"
"       <Value>JPEG</Value>"
"   </Option>"
"   <Option name='TILESIZE' type='int' default='127' description='Tile Size (INTERLEAVING=TILED only)'/>"
"   <Option name='TILEVERSION' type='int' default='2' description='Tile Version (INTERLEAVING=TILED only)'/>"
"</CreationOptionList>" );
    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
                               "<LayerCreationOptionList/>" );

    poDriver->pfnIdentify = PCIDSK2Dataset::Identify;
    poDriver->pfnOpen = PCIDSK2Dataset::Open;
    poDriver->pfnCreate = PCIDSK2Dataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
