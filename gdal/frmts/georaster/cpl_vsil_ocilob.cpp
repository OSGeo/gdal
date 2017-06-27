/**********************************************************************
 * $Id: cpl_vsil_ocilob.cpp $
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Oracle OCI LOB
 * Author:   Ivan Lucena, <ivan dot lucena at oracle dot com>
 *
 **********************************************************************
 * Copyright (c) 2015, Ivan Lucena, <ivan dot lucena at oracle dot com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_vsi_virtual.h"

#include "georaster_priv.h"

CPL_CVSID("$Id: cpl_vsil_ocilob.cpp $")

// *****************************************************************************
//                                                             WSIOCILobFSHandle
// *****************************************************************************

class WSIOCILobFSHandle : public VSIFilesystemHandler
{

public:
                              WSIOCILobFSHandle();
    virtual                  ~WSIOCILobFSHandle();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess,
                                    bool bSetError ) override;
    virtual int               Stat( const char *pszFilename,
                                    VSIStatBufL *pStatBuf, int nFlags ) override;

private:

    OWConnection*     poConnection;
    OWStatement*      poStatement;
    OCILobLocator*    phLocator;

    char**            ParseIdentificator( const char* pszFilename );
};

// *****************************************************************************
//                                                               VSIOCILobHandle
// *****************************************************************************

class VSIOCILobHandle : public VSIVirtualHandle
{
  private:

    OWConnection*     poConnection;
    OWStatement*      poStatement;
    OCILobLocator*    phLocator;
    GUIntBig          nFileSize;
    GUIntBig          nCurOff;
    boolean           bUpdate;

  public:
                      VSIOCILobHandle( OWConnection* poConnectionIn,
                                       OWStatement* poStatementIn,
                                       OCILobLocator* phLocatorIn,
                                       boolean bUpdateIn );
    virtual          ~VSIOCILobHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    virtual int       Eof() override;
    virtual int       Close() override;
};

// ****************************************************************************
// Implementation                                             WSIOCILobFSHandle
// ****************************************************************************

// ----------------------------------------------------------------------------
//                                                          WSIOCILobFSHandle()
// ----------------------------------------------------------------------------

WSIOCILobFSHandle::WSIOCILobFSHandle()
{
    poStatement  = NULL;
    phLocator    = NULL;
    poConnection = NULL;
}

// -----------------------------------------------------------------------------
//                                                          ~WSIOCILobFSHandle()
// -----------------------------------------------------------------------------

WSIOCILobFSHandle::~WSIOCILobFSHandle()
{
    if( phLocator )
    {
        OWStatement::Free( &phLocator, 1 );
    }

    if( poStatement )
    {
        delete poStatement;
    }

    if( poConnection )
    {
        delete poConnection;
    }
}

// -----------------------------------------------------------------------------
//                                                          ParseIdentificator()
// -----------------------------------------------------------------------------

char** WSIOCILobFSHandle::ParseIdentificator( const char* pszFilename )
{
    if( strncmp(pszFilename, "/vsiocilob/", strlen("/vsiocilob/") ) != 0 )
    {
        return NULL;
    }

    char** papszParam = CSLTokenizeString2( 
                            &pszFilename[strlen("/vsiocilob/")], ",",
                            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS |
                            CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

    if( CSLCount( papszParam ) < 6 )
    {
        CSLDestroy( papszParam );
        return NULL;
    }

    return papszParam;
}

// -----------------------------------------------------------------------------
//                                                                        Open()
// -----------------------------------------------------------------------------

VSIVirtualHandle* WSIOCILobFSHandle::Open( const char* pszFilename, 
                                           const char* pszAccess,
                                           bool /* bSetError*/ )
{
    char** papszParam = ParseIdentificator( pszFilename );

    if( ! papszParam )
    {
        return NULL;
    }

    if( ! EQUAL( papszParam[5], "noext" ) )
    {
        CSLDestroy( papszParam );
        return NULL;
    }

    poConnection = new OWConnection( papszParam[0],
                                     papszParam[1],
                                     papszParam[2] );

    if( ! poConnection->Succeeded() )
    {
        CSLDestroy( papszParam );
        return NULL;
    }

    const char *pszUpdate = "";
    boolean bUpdate = false;

    if( strchr(pszAccess, 'w') != NULL ||
        strchr(pszAccess, '+') != NULL )
    {
        pszUpdate = "for update";
        bUpdate = true;
    }

    poStatement = poConnection->CreateStatement( CPLSPrintf( 
                    "select rasterblock from %s where rasterid = %s and rownum = 1 %s", 
                    papszParam[3], papszParam[4], pszUpdate ) );

    poStatement->Define( &phLocator );

    CSLDestroy( papszParam );

    if( ! poStatement->Execute() )
    {
        return NULL;
    }

    return new VSIOCILobHandle( poConnection, poStatement, phLocator, bUpdate );
}

// -----------------------------------------------------------------------------
//                                                                        Stat()
// -----------------------------------------------------------------------------

int WSIOCILobFSHandle::Stat( const char* pszFilename,
                             VSIStatBufL* pStatBuf,
                             int nFlags )
{
    (void) nFlags;

    memset( pStatBuf, 0, sizeof(VSIStatBufL) );

    char** papszParam = ParseIdentificator( pszFilename );

    if( ! papszParam )
    {
        return -1;
    }

    if(  strcmp( papszParam[5], "noext" ) != 0 )
    {
        return -1;
    }

    CSLDestroy( papszParam );

    if( poStatement && phLocator )
    {
        pStatBuf->st_size = poStatement->GetBlobLength( phLocator );
    }

    pStatBuf->st_mode = S_IFREG;

    return 0;
}

// ****************************************************************************
// Implementation                                               VSIOCILobHandle
// ****************************************************************************

// ----------------------------------------------------------------------------
//                                                            VSIOCILobHandle()
// ----------------------------------------------------------------------------

VSIOCILobHandle::VSIOCILobHandle( OWConnection* poConnectionIn,
                                  OWStatement* poStatementIn,
                                  OCILobLocator* phLocatorIn,
                                  boolean bUpdateIn )
{
    this->poConnection = poConnectionIn;
    this->poStatement  = poStatementIn;
    this->phLocator    = phLocatorIn;
    this->bUpdate      = bUpdateIn;

    nCurOff     = 0;

    nFileSize   = poStatement->GetBlobLength( phLocator );
}

// ----------------------------------------------------------------------------
//                                                           ~VSIOCILobHandle()
// ----------------------------------------------------------------------------

VSIOCILobHandle::~VSIOCILobHandle()
{
}

// ----------------------------------------------------------------------------
//                                                                       Seek()
// ----------------------------------------------------------------------------

int VSIOCILobHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if (nWhence == SEEK_END)
    {
        nOffset = poStatement->GetBlobLength( phLocator );
    }

    if (nWhence == SEEK_CUR)
    {
        nOffset += nCurOff;
    }

    nCurOff = nOffset;

    return 0;
}

// ----------------------------------------------------------------------------
//                                                                       Tell()
// ----------------------------------------------------------------------------

vsi_l_offset VSIOCILobHandle::Tell()
{
    return nCurOff;
}

// ----------------------------------------------------------------------------
//                                                                       Read()
// ----------------------------------------------------------------------------

size_t VSIOCILobHandle::Read( void* pBuffer, size_t nSize, size_t nCount )
{
    GUIntBig  nBytes = ( nSize * nCount );

    if( nBytes == 0 )
    {
        return 0;
    }

    GUIntBig nRead = poStatement->ReadBlob( phLocator,
                                            pBuffer,
                                            (nCurOff + 1),
                                            nBytes  );

    nCurOff += (GUIntBig) nRead;

    return (size_t) ( nRead / nSize );
}

// ----------------------------------------------------------------------------
//                                                                      Write()
// ----------------------------------------------------------------------------

size_t VSIOCILobHandle::Write( const void * pBuffer,
                               size_t nSize,
                               size_t nCount )
{
    GUIntBig  nBytes = ( nSize * nCount );

    if( nBytes == 0 )
    {
        return 0;
    }

    GUIntBig nWrite = poStatement->WriteBlob( phLocator,
                                              (void*) pBuffer,
                                              (nCurOff + 1),
                                              nBytes );

    nCurOff += (GUIntBig) nWrite;

    return (size_t) ( nWrite / nSize );
}

// ----------------------------------------------------------------------------
//                                                                        Eof()
// ----------------------------------------------------------------------------

int VSIOCILobHandle::Eof()
{
    return (int) ( nCurOff >= nFileSize );
}

// ----------------------------------------------------------------------------
//                                                                      Close()
// ----------------------------------------------------------------------------

int VSIOCILobHandle::Close()
{
    if( bUpdate )
    {
        poConnection->Commit();
    }

    return 0;
}

// -----------------------------------------------------------------------------
//                                                      VSIInstallStdinHandler()
// -----------------------------------------------------------------------------

/**
 * \brief Install /vsiocilob/ virtual file system handler
 *
 * A special file handler that allows reading from Oracle's LOB objects.
 *
 * @since GDAL 2.0.0
 */
void VSIInstallOCILobHandler()
{
    VSIFileManager::InstallHandler( "/vsiocilob/", new WSIOCILobFSHandle );
}
