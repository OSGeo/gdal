/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB Support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2019, TileDB, Inc
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include "tiff.h"

#include "tiledb/tiledb"

CPL_CVSID("$Id$")


const CPLString TILEDB_VALUES( "TDB_VALUES" );

/************************************************************************/
/* ==================================================================== */
/*                               TileDBDataset                          */
/* ==================================================================== */
/************************************************************************/

class TileDBRasterBand;

class TileDBDataset : public GDALPamDataset
{
    friend class TileDBRasterBand;

    protected:
        uint16        nBitsPerSample = 8;
        GDALDataType  eDataType = GDT_Unknown;
        int           nBlockXSize = -1;
        int           nBlockYSize = -1;
        int           nBlocksX = 0;
        int           nBlocksY = 0;
        bool          bHasSubDatasets = false;
        int           nSubDataCount;
        char          **papszSubDatasets;
        CPLXMLNode*   psSubDatasetsTree = nullptr;
        
        std::unique_ptr<tiledb::Context> m_ctx;
        std::unique_ptr<tiledb::Array> m_array;
        std::unique_ptr<tiledb::ArraySchema> m_schema;
        std::unique_ptr<tiledb::FilterList> m_filterList;

        bool bStats = FALSE;
        CPLErr AddFilter( const char* pszFilterName, const int level );
        CPLErr CreateAttribute( GDALDataType eType, const CPLString& osAttrName,
                                const int nSubRasterCount=1 );
        CPLErr SetBuffer( tiledb::Query& query, GDALDataType eType,
                            const CPLString& osAttrName,
                            void * pImage, int nSize );
    public:
        virtual ~TileDBDataset();

        CPLErr TryLoadXML(char **papszSiblingFiles = nullptr) override;
        CPLErr TrySaveXML() override;
        char** GetMetadata(const char *pszDomain) override;

        static GDALDataset      *Open( GDALOpenInfo * );
        static int              Identify( GDALOpenInfo * );
        static CPLErr           Delete( const char * pszFilename );
        static CPLErr           CopySubDatasets( GDALDataset* poSrcDS,
                                                TileDBDataset* poDstDS,
                                                GDALProgressFunc pfnProgress, 
                                                void *pProgressData );	
        static TileDBDataset    *CreateLL( const char * pszFilename,
                                    int nXSize, int nYSize, int nBands,
                                    char ** papszOptions );
        static GDALDataset      *Create( const char * pszFilename,
                                    int nXSize, int nYSize, int nBands,
                                    GDALDataType eType, char ** papszOptions );
        static GDALDataset      *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData);
        static void             ErrorHandler( const std::string& msg );
        static void             SetBlockSize( GDALRasterBand* poBand,
                                                char ** &papszOptions );

};

/************************************************************************/
/* ==================================================================== */
/*                            TileDBRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class TileDBRasterBand : public GDALPamRasterBand
{
    friend class TileDBDataset;
    protected:
        TileDBDataset  *poGDS;
        CPLString osAttrName;
    public:
        TileDBRasterBand( TileDBDataset *, int, CPLString = TILEDB_VALUES );
        virtual ~TileDBRasterBand();
        virtual CPLErr IReadBlock( int, int, void * ) override;
        virtual CPLErr IWriteBlock( int, int, void * ) override;
        virtual GDALColorInterp GetColorInterpretation() override;
};

/************************************************************************/
/*                          TileDBRasterBand()                          */
/************************************************************************/

TileDBRasterBand::TileDBRasterBand(
        TileDBDataset *poDSIn, int nBandIn, CPLString osAttr ) :
    poGDS(poDSIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = poGDS->eDataType;
    eAccess = poGDS->eAccess;
    nRasterXSize = poGDS->nRasterXSize;
    nRasterYSize = poGDS->nRasterYSize;
    nBlockXSize = poGDS->nBlockXSize;
    nBlockYSize = poGDS->nBlockYSize;
    osAttrName = osAttr;
}

/************************************************************************/
/*                          ~TileDBRasterBand()                         */
/************************************************************************/
TileDBRasterBand::~TileDBRasterBand()

{
    FlushCache();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr TileDBRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )
{
    tiledb::Query query( *poGDS->m_ctx, *poGDS->m_array );

    size_t nStartX = nBlockXOff * nBlockXSize;
    size_t nStartY = nBlockYOff * nBlockYSize;

    query.set_layout( TILEDB_ROW_MAJOR ); 

    if ( EQUAL( TILEDB_VALUES, osAttrName ) )
    {
        query.set_subarray({ nStartX, nStartX + nBlockXSize - 1, 
                                     nStartY, nStartY + nBlockYSize - 1,
                                     size_t( nBand ), size_t( nBand ) });

    }
    else
    {
        query.set_subarray({ nStartX, nStartX + nBlockXSize - 1, 
                                     nStartY, nStartY + nBlockYSize - 1 });
    }
    
    poGDS->SetBuffer(query, eDataType, osAttrName, 
                     pImage, nBlockXSize * nBlockYSize);

    if ( poGDS->bStats )
        tiledb::Stats::enable();

    auto status = query.submit();

    if ( poGDS->bStats )
    {
        tiledb::Stats::dump(stdout);
        tiledb::Stats::disable();
    }

    if (status != tiledb::Query::Status::COMPLETE)
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/
CPLErr TileDBRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{ 
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to write block, dataset is opened read only.\n" );
        return CE_Failure;
    }

    CPLAssert( poGDS != nullptr
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != nullptr );

    tiledb::Query query( *poGDS->m_ctx, *poGDS->m_array );

    size_t nStartX = nBlockXOff * nBlockXSize;
    size_t nStartY = nBlockYOff * nBlockYSize;

    query.set_layout( TILEDB_GLOBAL_ORDER );
    std::vector<size_t> subarray = { nStartX, nStartX + nBlockXSize - 1, 
                                     nStartY, nStartY + nBlockYSize - 1,
                                     size_t( nBand ), size_t( nBand ) };
    query.set_subarray(subarray);

    poGDS->SetBuffer(query, eDataType, osAttrName, 
                     pImage, nBlockXSize * nBlockYSize);

    if ( poGDS->bStats )
        tiledb::Stats::enable();

    auto status = query.submit();

    if ( poGDS->bStats )
    {
        tiledb::Stats::dump(stdout);
        tiledb::Stats::disable();
    }

    query.finalize();

    if (status != tiledb::Query::Status::COMPLETE)
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp TileDBRasterBand::GetColorInterpretation()

{
    if (poGDS->nBands == 1)
        return GCI_GrayIndex;

    if ( nBand == 1 )
        return GCI_RedBand;

    else if( nBand == 2 )
        return GCI_GreenBand;

    else if ( nBand == 3 )
        return GCI_BlueBand;

    return GCI_AlphaBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             TileDBDataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           ~TileDBDataset()                           */
/************************************************************************/

TileDBDataset::~TileDBDataset()

{
    FlushCache();
    m_array->close();
    CPLDestroyXMLNode( psSubDatasetsTree );
    CSLDestroy( papszSubDatasets );
}

/************************************************************************/
/*                           TrySaveXML()                               */
/************************************************************************/

CPLErr TileDBDataset::TrySaveXML()

{
    tiledb::VFS vfs( *m_ctx, m_ctx->config() );

    nPamFlags &= ~GPF_DIRTY;

    if( psPam == nullptr || (nPamFlags & GPF_NOSAVE) )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Make sure we know the filename we want to store in.             */
/* -------------------------------------------------------------------- */
    if( !BuildPamFilename() )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Build the XML representation of the auxiliary metadata.          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = SerializeToXML( nullptr );
 
    if( psTree == nullptr )
    {
        /* If we have unset all metadata, we have to delete the PAM file */
        vfs.remove_file(psPam->pszPamFilename);
        return CE_None;
    }

    if ( psSubDatasetsTree != nullptr )
    {
        CPLAddXMLChild( psTree, psSubDatasetsTree->psChild );
    }

/* -------------------------------------------------------------------- */
/*      If we are working with a subdataset, we need to integrate       */
/*      the subdataset tree within the whole existing pam tree,         */
/*      after removing any old version of the same subdataset.          */
/* -------------------------------------------------------------------- */
    if( !psPam->osSubdatasetName.empty() )
    {
        CPLXMLNode *psOldTree = nullptr, *psSubTree;

        CPLErrorReset();
        CPLPushErrorHandler( CPLQuietErrorHandler );
 
        if ( vfs.is_file( psPam->pszPamFilename ) )
        {
            auto nBytes = vfs.file_size( psPam->pszPamFilename );
            tiledb::VFS::filebuf fbuf( vfs );
            fbuf.open( psPam->pszPamFilename, std::ios::in );
            std::istream is ( &fbuf );

            if ( is.good() )
            {
                CPLString osDoc;
                osDoc.resize(nBytes);
                is.read( ( char* ) osDoc.data(), nBytes );
                psOldTree = CPLParseXMLString( osDoc );
            }
            
            fbuf.close();
        }

        CPLPopErrorHandler();

        if( psOldTree == nullptr )
            psOldTree = CPLCreateXMLNode( nullptr, CXT_Element, "PAMDataset" );

        for( psSubTree = psOldTree->psChild;
             psSubTree != nullptr;
             psSubTree = psSubTree->psNext )
        {
            if( psSubTree->eType != CXT_Element
                || !EQUAL(psSubTree->pszValue, "Subdataset") )
                continue;

            if( !EQUAL(CPLGetXMLValue( psSubTree, "name", "" ),
                       psPam->osSubdatasetName) )
                continue;

            break;
        }

        if( psSubTree == nullptr )
        {
            psSubTree = CPLCreateXMLNode( psOldTree, CXT_Element,
                                          "Subdataset" );
            CPLCreateXMLNode(
                CPLCreateXMLNode( psSubTree, CXT_Attribute, "name" ),
                CXT_Text, psPam->osSubdatasetName );
        }

        CPLXMLNode *psOldPamDataset = CPLGetXMLNode( psSubTree, "PAMDataset");
        if( psOldPamDataset != nullptr )
        {
            CPLRemoveXMLChild( psSubTree, psOldPamDataset );
            CPLDestroyXMLNode( psOldPamDataset );
        }

        CPLAddXMLChild( psSubTree, psTree );
        psTree = psOldTree;
    }    

/* -------------------------------------------------------------------- */
/*      Try saving the auxiliary metadata.                               */
/* -------------------------------------------------------------------- */

    CPLPushErrorHandler( CPLQuietErrorHandler );

    int bSaved = 0;
    vfs.touch( psPam->pszPamFilename );
    tiledb::VFS::filebuf fbuf( vfs );
    fbuf.open( psPam->pszPamFilename, std::ios::out );
    std::ostream os(&fbuf);

    if (os.good())
    {
        char* pszTree = CPLSerializeXMLTree( psTree );
        os.write( pszTree, strlen(pszTree));
        bSaved = 1;
    }

    fbuf.close();
    
    CPLPopErrorHandler();

/* -------------------------------------------------------------------- */
/*      If it fails, check if we have a proxy directory for auxiliary    */
/*      metadata to be stored in, and try to save there.                */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( bSaved )
        eErr = CE_None;
    else
    {
        const char *pszBasename = GetDescription();

        if( psPam->osPhysicalFilename.length() > 0 )
            pszBasename = psPam->osPhysicalFilename;

        const char *pszNewPam = nullptr;
        if( PamGetProxy(pszBasename) == nullptr
            && ((pszNewPam = PamAllocateProxy(pszBasename)) != nullptr))
        {
            CPLErrorReset();
            CPLFree( psPam->pszPamFilename );
            psPam->pszPamFilename = CPLStrdup(pszNewPam);
            eErr = TrySaveXML();
        }
        /* No way we can save into a /vsicurl resource */
        else if( !STARTS_WITH(psPam->pszPamFilename, "/vsicurl") )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unable to save auxiliary information in %s.",
                      psPam->pszPamFilename );
            eErr = CE_Warning;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    return eErr;
}

/************************************************************************/
/*                           TryLoadXML()                               */
/************************************************************************/

CPLErr TileDBDataset::TryLoadXML( CPL_UNUSED char **papszSiblingFiles )

{
    PamInitialize();

    tiledb::VFS vfs( *m_ctx, m_ctx->config() );

/* -------------------------------------------------------------------- */
/*      Clear dirty flag.  Generally when we get to this point is       */
/*      from a call at the end of the Open() method, and some calls     */
/*      may have already marked the PAM info as dirty (for instance     */
/*      setting metadata), but really everything to this point is       */
/*      reproducible, and so the PAM info should not really be          */
/*      thought of as dirty.                                            */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

/* -------------------------------------------------------------------- */
/*      Try reading the file.                                           */
/* -------------------------------------------------------------------- */
    if( !BuildPamFilename() )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      In case the PAM filename is a .aux.xml file next to the         */
/*      physical file and we have a siblings list, then we can skip     */
/*      stat'ing the filesystem.                                        */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = nullptr;

    CPLErr eLastErr = CPLGetLastErrorType();
    int nLastErrNo = CPLGetLastErrorNo();
    CPLString osLastErrorMsg = CPLGetLastErrorMsg();

    CPLErrorReset();
    CPLPushErrorHandler( CPLQuietErrorHandler );

    if ( vfs.is_file( psPam->pszPamFilename ) )
    {
        auto nBytes = vfs.file_size( psPam->pszPamFilename );
        tiledb::VFS::filebuf fbuf( vfs );
        fbuf.open( psPam->pszPamFilename, std::ios::in );
        std::istream is ( &fbuf );
        CPLString osDoc;
        osDoc.resize(nBytes);
        is.read( ( char* ) osDoc.data(), nBytes );
        fbuf.close();
        psTree = CPLParseXMLString( osDoc );
    }

    CPLPopErrorHandler();
    CPLErrorReset();

    if( eLastErr != CE_None )
        CPLErrorSetState( eLastErr, nLastErrNo, osLastErrorMsg.c_str() );

/* -------------------------------------------------------------------- */
/*      If we are looking for a subdataset, search for its subtree not. */
/* -------------------------------------------------------------------- */
    if( psTree && !psPam->osSubdatasetName.empty() )
    {
        CPLXMLNode *psSubTree = psTree->psChild;

        for( ;
             psSubTree != nullptr;
             psSubTree = psSubTree->psNext )
        {
            if( psSubTree->eType != CXT_Element
                || !EQUAL(psSubTree->pszValue,"Subdataset") )
                continue;

            if( !EQUAL(CPLGetXMLValue( psSubTree, "name", "" ),
                       psPam->osSubdatasetName) )
                continue;

            psSubTree = CPLGetXMLNode( psSubTree, "PAMDataset" );
            break;
        }

        if( psSubTree != nullptr )
            psSubTree = CPLCloneXMLTree( psSubTree );

        CPLDestroyXMLNode( psTree );
        psTree = psSubTree;
    }

/* -------------------------------------------------------------------- */
/*      Initialize ourselves from this XML tree.                        */
/* -------------------------------------------------------------------- */

    CPLString osVRTPath(CPLGetPath(psPam->pszPamFilename));
    const CPLErr eErr = XMLInit( psTree, osVRTPath );

    CPLDestroyXMLNode( psTree );

    if( eErr != CE_None )
        PamClear();

    return eErr;
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **TileDBDataset::GetMetadata(const char *pszDomain)

{
    if( pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS") )
    {
        char** papszMeta = GDALPamDataset::GetMetadata( pszDomain );
        int n = CSLCount( papszMeta );
        for ( int i = 0; i < n; i += 2 )
        {
            CPLString osName;
            osName.Printf( "SUBDATASET_%d_NAME", (i / 2) + 1 );
            char* pszAttr = papszMeta[i] + osName.size() + 1;

            if ( !STARTS_WITH( pszAttr, "TILEDB:" ) )
            {
                CPLFree( papszMeta[i] );
    
                papszMeta[i] =  CPLStrdup(
                    CPLString().Printf(
                        "%s=TILEDB:\"%s\":%s",
                        osName.c_str(),
                        GetDescription(),
                        pszAttr
                    )
                );
            }
        }
        return papszMeta;
    }
    else
    {
        return GDALPamDataset::GetMetadata( pszDomain );
    }    
}

/************************************************************************/
/*                             SetBuffer()                              */
/************************************************************************/

CPLErr TileDBDataset::SetBuffer( tiledb::Query& query, GDALDataType eType,
                        const CPLString& osAttrName, void * pImage, int nSize )
{
    switch (eType)
    {
        case GDT_Byte:
            query.set_buffer(
                osAttrName, reinterpret_cast<unsigned char*>( pImage ), nSize );
            break;
        case GDT_UInt16:
            query.set_buffer(
                osAttrName, reinterpret_cast<unsigned short*>( pImage ), nSize );
            break;
        case GDT_UInt32:
            query.set_buffer(
                osAttrName, reinterpret_cast<unsigned int*>( pImage ), nSize );
            break;
        case GDT_Int16:
            query.set_buffer(
                osAttrName, reinterpret_cast<short*>( pImage ), nSize );
            break;
        case GDT_Int32:
            query.set_buffer(
                osAttrName, reinterpret_cast<int*>( pImage ), nSize );
            break;
        case GDT_Float32:
            query.set_buffer(
                osAttrName, reinterpret_cast<float*>( pImage ), nSize );
            break;
        case GDT_Float64:
            query.set_buffer(
                osAttrName, reinterpret_cast<double*>( pImage ), nSize );
            break;
        case GDT_CInt16:
            query.set_buffer(
                osAttrName, reinterpret_cast<short*>( pImage ), nSize );
            break;
        case GDT_CInt32:
            query.set_buffer(
                osAttrName, reinterpret_cast<int*>( pImage ), nSize * 2 );
            break;
        case GDT_CFloat32:
            query.set_buffer(
                osAttrName, reinterpret_cast<float*>( pImage ), nSize * 2 );
            break;
        case GDT_CFloat64:
            query.set_buffer(
                osAttrName, reinterpret_cast<double*>( pImage ), nSize * 2 );
            break;
        default:
            return CE_Failure;
    }
    return CE_None;
}

/************************************************************************/
/*                           AddFilter()                                */
/************************************************************************/

CPLErr TileDBDataset::AddFilter( const char* pszFilterName, const int level )

{
   if (pszFilterName == nullptr)
        m_filterList->add_filter( tiledb::Filter( *m_ctx, TILEDB_FILTER_NONE )
            .set_option( TILEDB_COMPRESSION_LEVEL, level ) );
    else if EQUAL(pszFilterName, "GZIP")
        m_filterList->add_filter( tiledb::Filter( *m_ctx, TILEDB_FILTER_GZIP )
            .set_option( TILEDB_COMPRESSION_LEVEL, level ) );
    else if EQUAL(pszFilterName, "ZSTD")
        m_filterList->add_filter( tiledb::Filter( *m_ctx, TILEDB_FILTER_ZSTD )
            .set_option( TILEDB_COMPRESSION_LEVEL, level ) );
    else if EQUAL(pszFilterName, "LZ4")
        m_filterList->add_filter( tiledb::Filter( *m_ctx, TILEDB_FILTER_LZ4 )
            .set_option( TILEDB_COMPRESSION_LEVEL, level ) );
    else if EQUAL(pszFilterName, "RLE")
        m_filterList->add_filter( tiledb::Filter( *m_ctx, TILEDB_FILTER_RLE )
            .set_option( TILEDB_COMPRESSION_LEVEL, level ) );
    else if EQUAL(pszFilterName, "BZIP2")
        m_filterList->add_filter( tiledb::Filter( *m_ctx, TILEDB_FILTER_BZIP2 )
            .set_option( TILEDB_COMPRESSION_LEVEL, level ) );
    else if EQUAL(pszFilterName, "DOUBLE-DELTA")
        m_filterList->add_filter( tiledb::Filter( *m_ctx,
            TILEDB_FILTER_DOUBLE_DELTA ) );
    else if EQUAL(pszFilterName, "POSITIVE-DELTA")
        m_filterList->add_filter( tiledb::Filter( *m_ctx,
            TILEDB_FILTER_POSITIVE_DELTA ) );
    else
        return CE_Failure;
    
    return CE_None;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

CPLErr TileDBDataset::Delete( const char * pszFilename )

{
    tiledb::Context ctx;
    ctx.set_error_handler( TileDBDataset::ErrorHandler );
    tiledb::VFS vfs( ctx );
    if ( vfs.is_dir( pszFilename ) )
    {
        vfs.remove_dir( pszFilename );
        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int TileDBDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:") )
    {
        return TRUE;
    }

    const char* pszConfig = CSLFetchNameValue( 
        poOpenInfo->papszOpenOptions, "TILEDB_CONFIG" );
    if ( pszConfig != nullptr )
    {
        tiledb::Config cfg( pszConfig );
        tiledb::Context ctx( cfg );
        ctx.set_error_handler( ErrorHandler );
        tiledb::VFS vfs( ctx, cfg );
        if ( ( vfs.is_bucket(poOpenInfo->pszFilename ) ) && 
            ( tiledb::Object::object( ctx, poOpenInfo->pszFilename ).type() == tiledb::Object::Type::Array ) )
            return TRUE;
    }
    else if( poOpenInfo->bIsDirectory )
    {
        const char* pszArrayName = CPLGetBasename( poOpenInfo->pszFilename ); 
        const int nMaxFiles =
            atoi(CPLGetConfigOption( "GDAL_READDIR_LIMIT_ON_OPEN", "1000" ) );
        char** papszSiblingFiles = VSIReadDirEx( 
                                    poOpenInfo->pszFilename,
                                    nMaxFiles );

        CPLString osAux;
        osAux.Printf( "%s.tdb.aux.xml", pszArrayName );        
        if( CSLFindString( papszSiblingFiles, osAux ) != -1 )
        {
            return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TileDBDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return nullptr;

    TileDBDataset *poDS = new TileDBDataset();

    const char* pszConfig = CSLFetchNameValue(
                                poOpenInfo->papszOpenOptions,
                                "TILEDB_CONFIG" );
    if( pszConfig != nullptr )
    {
        tiledb::Config cfg( pszConfig );
        poDS->m_ctx.reset( new tiledb::Context( cfg ) );
    }
    else
    {
        poDS->m_ctx.reset( new tiledb::Context() );
    }

    poDS->m_ctx->set_error_handler(ErrorHandler);

    CPLString osArrayPath;
    CPLString osAux;
    CPLString osSubdataset; 

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:") )
    {
        // form required read attributes and open file
        // Create a corresponding GDALDataset.
        char **papszName =
            CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                            CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES);

        if( !( CSLCount(papszName) == 3 ) )
        {
            CSLDestroy(papszName);
            delete poDS;
            return nullptr;
        }

        osArrayPath = papszName[1];
        osSubdataset = papszName[2];
        poDS->SetSubdatasetName( osSubdataset.c_str() );
    }
    else
    {
        osArrayPath = poOpenInfo->pszFilename; 
    }

    const char* pszArrayName = CPLGetBasename( osArrayPath ); 
    osAux.Printf( "%s.tdb", pszArrayName );        

    // aux file is in array folder
    poDS->SetPhysicalFilename( CPLFormFilename( osArrayPath, osAux, nullptr ) );
    // Initialize any PAM information.
    poDS->SetDescription( osArrayPath );
    // dependent on PAM metadata for information about array
    poDS->TryLoadXML();

    poDS->m_array.reset(
        new tiledb::Array( *poDS->m_ctx, osArrayPath, TILEDB_READ ) );

    tiledb::ArraySchema schema = poDS->m_array->schema();
    std::vector<tiledb::Dimension> dims = schema.domain().dimensions();

    if( ( dims.size() == 2 ) || ( dims.size() == 3) )
    {
        if ( dims.size() == 3 )
        {
            poDS->nBands = dims[2].domain<size_t>().second
                            - dims[2].domain<size_t>().first + 1;
        }
        else
        {
            const char* pszBands = poDS->GetMetadataItem("NUM_BANDS", 
                                                        "IMAGE_STRUCTURE");
            if ( pszBands )
            {
                poDS->nBands = atoi( pszBands );
            }
        }
               
        poDS->nBlockXSize = dims[0].tile_extent<size_t>();
        poDS->nBlockYSize = dims[1].tile_extent<size_t>();
    }
    else
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
            "Wrong number of dimensions %li expected 2 or 3.", dims.size() );
        return nullptr;
    }

    const char* pszXSize = poDS->GetMetadataItem( "X_SIZE", "IMAGE_STRUCTURE" );
    if ( pszXSize )
    {
        poDS->nRasterXSize = atoi( pszXSize );
    }

    const char* pszYSize = poDS->GetMetadataItem( "Y_SIZE", "IMAGE_STRUCTURE" );
    if ( pszYSize )
    {
        poDS->nRasterYSize = atoi( pszYSize );
    }

    const char* pszNBits = poDS->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" );
    if ( pszNBits )
    {
        poDS->nBitsPerSample = atoi( pszNBits );
    }

    const char* pszDataType = poDS->GetMetadataItem( "DATA_TYPE", "IMAGE_STRUCTURE" );
    if ( pszDataType )
    {
        poDS->eDataType = static_cast<GDALDataType>( atoi( pszDataType ) );
    }

    poDS->eAccess = poOpenInfo->eAccess;

    if ( dims.size() == 3 )
    {
        // Create band information objects.
        for ( int i = 1; i <= poDS->nBands; ++i )
        {
            poDS->SetBand( i, new TileDBRasterBand( poDS, i ) );
        }
    }
    else // subdatasets
    {
        if( poOpenInfo->eAccess == GA_Update )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "The TileDB driver does not support update access "
                    "to subdatasets." );
            delete poDS;
            return nullptr;
        }

        if( !osSubdataset.empty() )
        {
            // do we have the attribute in the schema
            if ( schema.attributes().count( osSubdataset ) )
            {
                poDS->SetBand(1,
                    new TileDBRasterBand(
                        poDS, 1, osSubdataset ) );
            }
            else
            {
                if ( schema.attributes().count( osSubdataset + "_1" ) )
                {
                    // Create band information objects.
                    for ( int i = 1; i <= poDS->nBands; ++i )
                    {
                        CPLString osAttr = CPLString().Printf("%s_%d",
                                            osSubdataset.c_str(), i);
                        poDS->SetBand( i, 
                            new TileDBRasterBand(
                                poDS, i,
                                osAttr ) );
                    }
                }
                else
                {
                    CPLError( CE_Failure, CPLE_NotSupported,
                        "%s attribute is not found in TileDB schema.",
                        osSubdataset.c_str() );
                    delete poDS;
                    return nullptr;
                }
            }
        }
        else
        {
            char** papszMeta = poDS->GetMetadata( "SUBDATASETS" );
            if ( papszMeta != nullptr )
            {
                if ( ( CSLCount( papszMeta ) / 2 ) == 1 )
                {
                    CPLString osDSName =
                        CSLFetchNameValue(poDS->papszSubDatasets,
                                            "SUBDATASET_1_NAME");
                    delete poDS;
                    return (GDALDataset *)GDALOpen( osDSName, poOpenInfo->eAccess );
                }
            }
            else
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                    "%s is missing required TileDB subdataset metadata.",
                    poOpenInfo->pszFilename );
                delete poDS;
                return nullptr;
            }
        }
    }
    
    tiledb::VFS vfs( *poDS->m_ctx, poDS->m_ctx->config() );

    if ( vfs.is_dir( osArrayPath ) )
        poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );
    else 
        CPLError( CE_Warning, CPLE_AppDefined,
            "Overviews not supported for network writes." );
    
    return poDS;
}

/************************************************************************/
/*                              ErrorHandler()                          */
/************************************************************************/

void TileDBDataset::ErrorHandler( const std::string& msg )

{
    CPLError( CE_Failure, CPLE_AppDefined, "%s", msg.c_str() );
}

/************************************************************************/
/*                              CreateAttribute()                       */
/************************************************************************/

CPLErr TileDBDataset::CreateAttribute( GDALDataType eType, 
            const CPLString& osAttrName, const int nSubRasterCount )
{
    for ( int i = 0; i < nSubRasterCount; ++i )
    {
        CPLString osName( osAttrName );
        // a few special cases
        // remove any leading slashes or 
        // additional slashes as in the case of hdf5
        if STARTS_WITH( osName, "//" )
        {
            osName = osName.substr(2);
        }

        osName.replaceAll( "/", "_" );
        CPLString osPrettyName = osName;

        if ( ( bHasSubDatasets ) && ( nSubRasterCount > 1 ) )
        {
            osName = CPLString().Printf( "%s_%d", osName.c_str(), i + 1);
        }

        switch (eType)
        {
            case GDT_Byte:
            {
                m_schema->add_attribute( tiledb::Attribute::create<unsigned char>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 8;
                break;
            }
            case GDT_UInt16:
            {
                m_schema->add_attribute( tiledb::Attribute::create<unsigned short>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 16;
                break;
            }
            case GDT_UInt32:
            {
                m_schema->add_attribute( tiledb::Attribute::create<unsigned int>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 32;
                break;
            }
            case GDT_Int16:
            {
                m_schema->add_attribute( tiledb::Attribute::create<short>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 16;
                break;
            }
            case GDT_Int32:
            {
                m_schema->add_attribute( tiledb::Attribute::create<int>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 32;
                break;
            }
            case GDT_Float32:
            {
                m_schema->add_attribute( tiledb::Attribute::create<float>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 32;
                break;
            }
            case GDT_Float64:
            {
                m_schema->add_attribute( tiledb::Attribute::create<double>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 64;
                break;
            }
            case GDT_CInt16:
            {
                m_schema->add_attribute( tiledb::Attribute::create<short[2]>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 16;
                break;
            }
            case GDT_CInt32:
            {
                m_schema->add_attribute( tiledb::Attribute::create<int[2]>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 32;
                break;
            }
            case GDT_CFloat32:
            {
                m_schema->add_attribute( tiledb::Attribute::create<float[2]>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 32;
                break;
            }
            case GDT_CFloat64:
            {
                m_schema->add_attribute( tiledb::Attribute::create<double[2]>( *m_ctx, osName, *m_filterList ) );
                nBitsPerSample = 64;
                break;
            }
            default:
                return CE_Failure;
        }

        if ( ( bHasSubDatasets ) && ( i == 0 ) )
        {
            ++nSubDataCount;
            
            CPLString osDim;
            switch( nSubRasterCount )
            {
            case 2:
                osDim.Printf( "%dx%d", nRasterXSize, nRasterYSize );
                break;
            default:
                osDim.Printf( "%dx%dx%d", nSubRasterCount,
                    nRasterXSize, nRasterYSize );
                break;
            }

            papszSubDatasets = CSLSetNameValue( papszSubDatasets, 
                            CPLString().Printf( "SUBDATASET_%d_NAME", 
                                nSubDataCount ),
                            CPLString().Printf( "%s", osPrettyName.c_str() ) );

            papszSubDatasets = CSLSetNameValue( papszSubDatasets, 
                CPLString().Printf( "SUBDATASET_%d_DESC", nSubDataCount ),
                CPLString().Printf( "[%s] %s (%s)",
                    osDim.c_str(),
                    osPrettyName.c_str(),
                    GDALGetDataTypeName( eType )
                ) );


            // add to PAM metadata
            if ( psSubDatasetsTree == nullptr )
            {
                psSubDatasetsTree = CPLCreateXMLNode( nullptr, 
                                        CXT_Element, "PAMDataset" );
            }
                
            CPLXMLNode* psSubNode = CPLCreateXMLNode(
                    psSubDatasetsTree, CXT_Element, "Subdataset" );
            CPLAddXMLAttributeAndValue( psSubNode, "name", osPrettyName.c_str() );

            CPLXMLNode* psMetaNode = CPLCreateXMLNode(
                        CPLCreateXMLNode(
                        psSubNode, CXT_Element, "PAMDataset" ),
                    CXT_Element, "Metadata");
            CPLAddXMLAttributeAndValue( psMetaNode, "domain", "IMAGE_STRUCTURE" );

            CPLAddXMLAttributeAndValue( 
                CPLCreateXMLElementAndValue( psMetaNode, "MDI", 
                    CPLString().Printf( "%d", nRasterXSize ) ),
                "KEY",
                "X_SIZE"
            );

            CPLAddXMLAttributeAndValue( 
                CPLCreateXMLElementAndValue( psMetaNode, "MDI", 
                    CPLString().Printf( "%d", nRasterYSize ) ),
                "KEY",
                "Y_SIZE"
            );

            CPLAddXMLAttributeAndValue( 
                CPLCreateXMLElementAndValue( psMetaNode, "MDI", 
                    CPLString().Printf( "%d", eType ) ),
                "KEY",
                "DATA_TYPE"
            );

            CPLAddXMLAttributeAndValue( 
                CPLCreateXMLElementAndValue( psMetaNode, "MDI",
                    CPLString().Printf( "%d", nSubRasterCount ) ),
                "KEY",
                "NUM_BANDS"
            );

            CPLAddXMLAttributeAndValue( 
                CPLCreateXMLElementAndValue( psMetaNode, "MDI",
                    CPLString().Printf( "%d", nBitsPerSample ) ),
                "KEY",
                "NBITS"
            );
        }
    }
    return CE_None;
}

/************************************************************************/
/*                              SetBlockSize()                          */
/************************************************************************/

void TileDBDataset::SetBlockSize( GDALRasterBand* poBand, char ** &papszOptions)

{
    int nX = 0;
    int nY = 0;
    poBand->GetBlockSize( &nX, &nY );

    if ( CSLFetchNameValue(papszOptions, "BLOCKXSIZE") == nullptr )
    {
        papszOptions = CSLSetNameValue(
                            papszOptions, "BLOCKXSIZE",
                            CPLString().Printf( "%d", nX ) );
    }

    if ( CSLFetchNameValue(papszOptions, "BLOCKYSIZE") == nullptr )
    {
        papszOptions = CSLSetNameValue(
                            papszOptions, "BLOCKYSIZE",
                            CPLString().Printf( "%d", nY ) );
    }
}

/************************************************************************/
/*                              CreateLL()                              */
/*                                                                      */
/*      Shared functionality between TileDBDataset::Create() and        */
/*      TileDBDataset::CreateCopy() for creating TileDB array based on  */
/*      a set of options and a configuration.                           */
/************************************************************************/

TileDBDataset* TileDBDataset::CreateLL( const char *pszFilename,
                         int nXSize, int nYSize, int nBands,
                         char **papszOptions )
{
    if( ( nXSize <= 0 && nYSize <= 0 ) ) 
    {
        return nullptr;
    }

    TileDBDataset *poDS = new TileDBDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->nBands = nBands;
    poDS->eAccess = GA_Update;

    const char* pszConfig = CSLFetchNameValue( papszOptions, "TILEDB_CONFIG" );
    if( pszConfig != nullptr )
    {
        tiledb::Config cfg( pszConfig );
        poDS->m_ctx.reset( new tiledb::Context( cfg ) );
    }
    else
    {
        poDS->m_ctx.reset( new tiledb::Context() );
    }

    poDS->m_ctx->set_error_handler(ErrorHandler);

    const char* pszCompression = CSLFetchNameValue(
                                    papszOptions, "COMPRESSION" );  
    const char* pszCompressionLevel = CSLFetchNameValue(
                                    papszOptions, "COMPRESSION_LEVEL" );
 
    const char* pszBlockXSize = CSLFetchNameValue( papszOptions, "BLOCKXSIZE" );
    poDS->nBlockXSize = ( pszBlockXSize ) ? atoi( pszBlockXSize ) : 256;
    const char* pszBlockYSize = CSLFetchNameValue( papszOptions, "BLOCKYSIZE" );
    poDS->nBlockYSize = ( pszBlockYSize ) ? atoi( pszBlockYSize ) : 256;
    poDS->bStats = CSLFetchBoolean( papszOptions, "STATS", FALSE );

    // set dimensions and attribute type for schema
    poDS->m_schema.reset( new tiledb::ArraySchema( *poDS->m_ctx, TILEDB_DENSE ) );
    poDS->m_schema->set_tile_order( TILEDB_ROW_MAJOR );
    poDS->m_schema->set_cell_order( TILEDB_ROW_MAJOR );

    poDS->m_filterList.reset(new tiledb::FilterList(*poDS->m_ctx));

    if (pszCompression != nullptr)
    {
        int nLevel = ( pszCompressionLevel ) ? atoi( pszCompressionLevel ) : -1;
        if ( poDS->AddFilter( pszCompression, nLevel ) == CE_None )
        {
            poDS->SetMetadataItem( "COMPRESSION", pszCompression,
                                   "IMAGE_STRUCTURE" );
            poDS->m_schema->set_coords_filter_list( *poDS->m_filterList );
        }
    }

    CPLString osAux;
    const char* pszArrayName = CPLGetBasename( pszFilename ); 
    osAux.Printf( "%s.tdb", pszArrayName );        

    poDS->SetPhysicalFilename( CPLFormFilename(
                                pszFilename, osAux.c_str(), nullptr ) );

    // Initialize PAM information.
    poDS->SetDescription( pszFilename );

    // this driver enforces that all subdatasets are the same size
    tiledb::Domain domain( *poDS->m_ctx );

    // Note the dimension bounds are inclusive and are expanded to the match the block size
    poDS->nBlocksX = DIV_ROUND_UP( nXSize, poDS->nBlockXSize );
    poDS->nBlocksY = DIV_ROUND_UP( nYSize, poDS->nBlockYSize );
    size_t w = poDS->nBlocksX * poDS->nBlockXSize;
    size_t h = poDS->nBlocksY * poDS->nBlockYSize;
    
    auto d1 = tiledb::Dimension::create<size_t>(
                *poDS->m_ctx, "X", {0, w},
                size_t( poDS->nBlockXSize ) );
    auto d2 = tiledb::Dimension::create<size_t>( *poDS->m_ctx, "Y", {0, h}, size_t( poDS->nBlockYSize ) );
    if ( nBands > 0 )
    {
        auto d3 = tiledb::Dimension::create<size_t>( *poDS->m_ctx, "BANDS", {1, size_t( nBands )}, 1);
        domain.add_dimensions( d1, d2, d3 );
    }
    else
    {
        domain.add_dimensions( d1, d2 );
    }
    
    poDS->m_schema->set_domain( domain );

    return poDS;
}

/************************************************************************/
/*                              CopySubDatasets()                       */
/*                                                                      */
/*      Copy SubDatasets from src to a TileDBDataset                    */
/*                                                                      */
/************************************************************************/

CPLErr TileDBDataset::CopySubDatasets( GDALDataset* poSrcDS,
                                    TileDBDataset* poDstDS,
                                    GDALProgressFunc pfnProgress, 
                                    void *pProgressData )

{
    std::vector<GDALDataset*> apoDatasets;
    poDstDS->bHasSubDatasets = true;
    char ** papszSrcSubDatasets = poSrcDS->GetMetadata( "SUBDATASETS" );
    char* pszSource = CPLStrdup( strstr( papszSrcSubDatasets[0], "=" ) + 1 );
    char* pszAttrName = CPLStrdup( strstr ( strstr( papszSrcSubDatasets[0], ":" ) + 1 , ":" ) + 1 );


    GDALDataset* poSubDataset = (GDALDataset *) GDALOpen( pszSource, GA_ReadOnly );
    apoDatasets.push_back( poSubDataset );

    size_t nSubXSize = poSubDataset->GetRasterXSize();
    size_t nSubYSize = poSubDataset->GetRasterYSize();   

    poDstDS->CreateAttribute( poSubDataset->GetRasterBand( 1 )
                                                ->GetRasterDataType(),
                                            pszAttrName,
                                            poSubDataset->GetRasterCount() );

    CPLFree( pszAttrName );
    CPLFree( pszSource );

    for( int i = 2; papszSrcSubDatasets[i] != nullptr; i += 2 )
    {
        pszSource = CPLStrdup(strstr(papszSrcSubDatasets[i],"=")+1);
        pszAttrName = CPLStrdup( strstr ( 
                        strstr( papszSrcSubDatasets[i], ":" ) + 1 ,
                        ":" ) + 1 );

        GDALDataset* poSubDS = (GDALDataset *) GDALOpen( pszSource, GA_ReadOnly );

        if ( poSubDS->GetRasterCount() > 0 )
        {
            if ( (poSubDS == nullptr) ||
                ( poSubDS->GetRasterXSize() != (int) nSubXSize ) ||
                ( poSubDS->GetRasterYSize() != (int) nSubYSize ) )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                    "Sub-datasets must have the same dimension," 
                    "skipping %s\n",
                    pszSource );
                GDALClose( poSubDS );
            }
            else
            {
                apoDatasets.push_back( poSubDS );
                poDstDS->CreateAttribute(
                    poSubDS->GetRasterBand(1)->GetRasterDataType(),
                    pszAttrName, poSubDS->GetRasterCount() );
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                "Sub-datasets must contain data in bands," 
                "skipping %s\n",
                pszSource );
            GDALClose( poSubDS );
        }
        
        CPLFree( pszAttrName );
        CPLFree( pszSource );
    }

    poDstDS->SetMetadata( poDstDS->papszSubDatasets, "SUBDATASETS" );

    tiledb::Array::create( poDstDS->GetDescription(), *poDstDS->m_schema );
    poDstDS->m_array.reset( new tiledb::Array(
        *poDstDS->m_ctx, poDstDS->GetDescription(), TILEDB_WRITE ) );

    /* --------------------------------------------------------  */
    /*      Report preliminary (0) progress.                     */
    /* --------------------------------------------------------- */
    if( !pfnProgress( 0.0, nullptr, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt,
                "User terminated CreateCopy()" );
        for ( auto poDS : apoDatasets )
            GDALClose( (GDALDatasetH) poDS );
        return CE_Failure;
    }

    // copy over subdatasets
    tiledb::Query query( *poDstDS->m_ctx, *poDstDS->m_array );
    query.set_layout( TILEDB_GLOBAL_ORDER );

    int nTotalBlocks = poDstDS->nBlocksX * poDstDS->nBlocksY;

    for ( int i = 0; i < poDstDS->nBlocksX; ++i )
    {
        for ( int j = 0; j < poDstDS->nBlocksY; ++j )
        {
            size_t nStartX = i * poDstDS->nBlockXSize;
            size_t nStartY = j * poDstDS->nBlockYSize;
            std::vector<void*> aBlocks;

            std::vector<size_t> subarray = { nStartX, 
                nStartX + poDstDS->nBlockXSize - 1, 
                nStartY,
                nStartY + poDstDS->nBlockYSize - 1 };

            query.set_subarray(subarray);

            // all bands are the same size, clamp read to image width
            int nEndX = poDstDS->nBlockXSize;
            int nEndY = poDstDS->nBlockYSize;

            if ( ( nStartX + poDstDS->nBlockXSize ) > nSubXSize )
            {
                nEndX = nSubXSize - nStartX;
            }

            if ( ( nStartY + poDstDS->nBlockYSize ) > nSubYSize )
            {
                nEndY = nSubYSize - nStartY;
            }

            // have to write set all tiledb attributes on write
            int iAttr = 0;
            for ( auto poSubDS : apoDatasets )
            {
                GDALDataType eDT = poSubDS->GetRasterBand( 1 )->
                                            GetRasterDataType();
                poSubDS->AdviseRead( nStartX, nStartY, nEndX, nEndY,
                                        poDstDS->nBlockXSize,
                                        poDstDS->nBlockYSize,
                                        eDT,
                                        poSubDS->GetRasterCount(),
                                        nullptr,
                                        nullptr
                                    );

                for ( int b = 1; b <= poSubDS->GetRasterCount(); ++b )
                {
                    GDALRasterBand* poBand = poSubDS->GetRasterBand( b );
                    size_t nVals = poDstDS->nBlockXSize * poDstDS->nBlockYSize;
                    int nBytes = GDALGetDataTypeSizeBytes( eDT );
                    void* pImage = CPLMalloc( nVals * nBytes );
                    aBlocks.push_back(pImage);

                    // it is possible that the src and dest block sizes
                    // are different, so don't read the block directly
                    if ( poBand->RasterIO(
                        GF_Read, nStartX, nStartY, nEndX, nEndY,
                        pImage, poDstDS->nBlockXSize, poDstDS->nBlockYSize,
                        eDT, 0, 0, nullptr ) == CE_None )
                    {
                        poDstDS->SetBuffer(query, eDT,
                            poDstDS->m_schema->attribute( iAttr++ ).name(),
                            pImage, nVals);
                    }
                    else
                    {
                        CPLError( CE_Failure, CPLE_UserInterrupt,
                                "Unable to copy subdatasets" );
                        for ( auto poDS : apoDatasets )
                            GDALClose( (GDALDatasetH) poDS );

                        for (auto pBlk: aBlocks)
                            CPLFree( pBlk );

                        return CE_Failure;
                    }                    
                }
            }

            if ( poDstDS->bStats )
                tiledb::Stats::enable();

            auto status = query.submit();

            if ( poDstDS->bStats )
            {
                tiledb::Stats::dump(stdout);
                tiledb::Stats::disable();
            }

            query.finalize();

            for (auto pBlk: aBlocks)
                CPLFree( pBlk );

            if (status != tiledb::Query::Status::COMPLETE)
            {
                for ( auto poDS : apoDatasets )
                    GDALClose( (GDALDatasetH) poDS );
                return CE_Failure;
            }
            int nBlocks = ( i * poDstDS->nBlocksX ) + j;
            if( !pfnProgress( nBlocks /  static_cast<double>( nTotalBlocks),
                                nullptr, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt,
                        "User terminated CreateCopy()" );
                for ( auto poDS : apoDatasets )
                    GDALClose( (GDALDatasetH) poDS );
                return CE_Failure;
            }
        }
    }

    for ( auto poDS : apoDatasets )
        GDALClose( (GDALDatasetH) poDS );

    return CE_None;
}


/************************************************************************/
/*                              Create()                                */
/************************************************************************/

GDALDataset *
TileDBDataset::Create( const char * pszFilename, int nXSize, int nYSize, int nBands,
        GDALDataType eType, char ** papszOptions )

{
    TileDBDataset* poDS = TileDBDataset::CreateLL( pszFilename, nXSize, nYSize,
                                                    nBands, papszOptions );
    poDS->eDataType = eType;

    poDS->CreateAttribute( eType, TILEDB_VALUES );

    tiledb::Array::create( pszFilename, *poDS->m_schema );
    poDS->m_array.reset( new tiledb::Array( *poDS->m_ctx, pszFilename, TILEDB_WRITE ) );

    for( int i = 0; i < poDS->nBands; i++ )
        poDS->SetBand( i+1, new TileDBRasterBand( poDS, i+1 ) );

    poDS->SetMetadataItem( "NBITS", 
            CPLString().Printf( "%d", poDS->nBitsPerSample ),
            "IMAGE_STRUCTURE" );
    poDS->SetMetadataItem( "DATA_TYPE", 
            CPLString().Printf( "%d", poDS->eDataType ),
            "IMAGE_STRUCTURE" );

    poDS->SetMetadataItem( "X_SIZE", CPLString().Printf( "%d", poDS->nRasterXSize ), "IMAGE_STRUCTURE" );
    poDS->SetMetadataItem( "Y_SIZE", CPLString().Printf( "%d", poDS->nRasterYSize ), "IMAGE_STRUCTURE" );

    return poDS;
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *
TileDBDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                int bStrict, char ** papszOptions,
                GDALProgressFunc pfnProgress, 
                void *pProgressData )

{
    TileDBDataset* poDstDS = nullptr;

    if ( CSLFetchNameValue(papszOptions, "APPEND_SUBDATASET" ) )
    {
        // TileDB schemas are fixed
        CPLError(CE_Failure, CPLE_NotSupported,
            "TileDB driver does not support "
            "appending to an existing schema.");
        return nullptr;                   
    }

    char** papszSrcSubDatasets = poSrcDS->GetMetadata( "SUBDATASETS" );

    if ( papszSrcSubDatasets == nullptr )
    {
        const int nBands = poSrcDS->GetRasterCount();

        if ( nBands > 0 )
        {
            GDALRasterBand* poBand = poSrcDS->GetRasterBand( 1 );
            GDALDataType eType = poBand->GetRasterDataType();
            TileDBDataset::SetBlockSize( poBand, papszOptions );

            for (int i = 2; i <= nBands; ++i)
            {
                if (eType != poSrcDS->GetRasterBand( i )->GetRasterDataType( ) )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                        "TileDB driver does not support "
                        "source dataset with different band data types.");
                    return nullptr;
                }               
            }
            
            poDstDS = ( TileDBDataset* ) TileDBDataset::Create( pszFilename, 
                        poSrcDS->GetRasterXSize(),
                        poSrcDS->GetRasterYSize(), 
                        nBands, eType, papszOptions );
            
            CPLErr eErr = GDALDatasetCopyWholeRaster( poSrcDS, poDstDS,
                                            papszOptions, pfnProgress,
                                            pProgressData );

            if ( eErr != CE_None )
            {
                CPLError(eErr, CPLE_AppDefined, 
                        "Error copying raster to TileDB.");
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                "TileDB driver does not support "
                "source dataset with zero bands.");
            return nullptr;
        }
    }
    else
    {
        if ( bStrict )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                "TileDB driver does not support copying "
                "subdatasets in strict mode.");
            return nullptr;
        }

        const int nSubDatasetCount = CSLCount( papszSrcSubDatasets ) / 2;
        const int nMaxFiles =
            atoi(CPLGetConfigOption( "GDAL_READDIR_LIMIT_ON_OPEN", "1000" ) );

        if ( nSubDatasetCount <= nMaxFiles )
        {
            char* pszSource = CPLStrdup( strstr( papszSrcSubDatasets[0], "=" ) + 1 );
            GDALDataset* poSubDataset = (GDALDataset *) GDALOpen( 
                                                            pszSource,
                                                            GA_ReadOnly );
            if ( poSubDataset->GetRasterCount() > 0 )
            {
                GDALRasterBand* poBand = poSubDataset->GetRasterBand( 1 );
                TileDBDataset::SetBlockSize( poBand, papszOptions );

                poDstDS = TileDBDataset::CreateLL(
                            pszFilename, poBand->GetXSize(),
                            poBand->GetYSize(), 0, papszOptions );

                if ( TileDBDataset::CopySubDatasets( poSrcDS, poDstDS,
                                        pfnProgress, pProgressData ) != CE_None )
                {
                    delete poDstDS;
                    poDstDS = nullptr;
                    CPLError( CE_Fatal, CPLE_AppDefined, 
                        "Unable to copy subdatasets.");                    
                }
            }

            CPLFree( pszSource );
            GDALClose( poSubDataset );
        }
        else
        {
           CPLError( CE_Fatal, CPLE_AppDefined, 
            "Please increase GDAL_READDIR_LIMIT_ON_OPEN variable.");
        }
    }

    // Supporting mask bands is a possible future task
    if ( poDstDS != nullptr )
    {
        int nCloneFlags = GCIF_PAM_DEFAULT & ~GCIF_MASK;
        poDstDS->CloneInfo( poSrcDS, nCloneFlags );
    }
 
    return poDstDS;
}

/************************************************************************/
/*                         GDALRegister_TILEDB()                        */
/************************************************************************/

void GDALRegister_TileDB()

{
    if( GDALGetDriverByName( "TileDB" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "TileDB" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_SUBCREATECOPY, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "TileDB" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_tiledb.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64 CInt16 CInt32 CFloat32 CFloat64" );
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='COMPRESSION' type='string-select' description='image compression to use' default='NONE'>\n"
"       <Value>NONE</Value>\n"
"       <Value>GZIP</Value>\n"
"       <Value>ZSTD</Value>\n"
"       <Value>LZ4</Value>\n"
"       <Value>RLE</Value>\n"
"       <Value>BZIP2</Value>\n"
"       <Value>DOUBLE-DELTA</Value>\n"
"       <Value>POSITIVE-DELTA</Value>\n"
"   </Option>\n"
"   <Option name='COMPRESSION_LEVEL' type='int' description='Compression level'/>\n"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height'/>"
"   <Option name='STATS' type='boolean' description='Dump TileDB stats'/>"
"   <Option name='TILEDB_CONFIG' type='string' description='location of configuration file for TileDB'/>"
"</CreationOptionList>\n" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='STATS' type='boolean' description='Dump TileDB stats'/>"
"   <Option name='TILEDB_CONFIG' type='string' description='location of configuration file for TileDB'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = TileDBDataset::Identify;
    poDriver->pfnOpen = TileDBDataset::Open;
    poDriver->pfnCreate = TileDBDataset::Create;
    poDriver->pfnCreateCopy = TileDBDataset::CreateCopy;
    poDriver->pfnDelete = TileDBDataset::Delete;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
