/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for plugins
 * Author:   Thomas Bonfort, thomas.bonfort@airbus.com
 *
 ******************************************************************************
 * Copyright (c) 2019, Thomas Bonfort <thomas.bonfort@airbus.com>
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

#include "cpl_port.h"
#include "cpl_vsil_plugin.h"

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

namespace cpl {

/************************************************************************/
/*                           VSIPluginHandle()                            */
/************************************************************************/

VSIPluginHandle::VSIPluginHandle( VSIPluginFilesystemHandler* poFSIn,
                                  const void *cbDataIn) :
    poFS(poFSIn),
    cbData(cbDataIn)
{
}

/************************************************************************/
/*                          ~VSIPluginHandle()                            */
/************************************************************************/

VSIPluginHandle::~VSIPluginHandle()
{
    if (cbData) {
        Close();
    }
}


/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIPluginHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    return poFS->Seek(cbData,nOffset,nWhence);
}


/************************************************************************/
/*                                  Tell()                              */
/************************************************************************/

vsi_l_offset VSIPluginHandle::Tell()
{
    return poFS->Tell(cbData);
}

size_t VSIPluginHandle::Read( void * const pBuffer, size_t const nSize,
                            size_t const  nMemb )
{
    return poFS->Read(cbData, pBuffer, nSize, nMemb);
}

int VSIPluginHandle::Eof()
{
    return poFS->Eof(cbData);
}

int VSIPluginHandle::Close()
{
    int ret = poFS->Close(cbData);
    cbData = nullptr;
    return ret;
}

int VSIPluginHandle::ReadMultiRange( int nRanges, void ** ppData, const vsi_l_offset* panOffsets, const size_t* panSizes ) {
    return poFS->ReadMultiRange(cbData,nRanges,ppData,panOffsets,panSizes);
}

VSIRangeStatus VSIPluginHandle::GetRangeStatus( vsi_l_offset nOffset, vsi_l_offset nLength ) {
    return poFS->GetRangeStatus(cbData,nOffset,nLength);
}

size_t VSIPluginHandle::Write( const void *pBuffer, size_t nSize,size_t nCount) {
    return poFS->Write(cbData,pBuffer,nSize,nCount);
}

int VSIPluginHandle::Flush() {
    return poFS->Flush(cbData);
}

int VSIPluginHandle::Truncate( vsi_l_offset nNewSize ) {
    return poFS->Truncate(cbData,nNewSize);
}

/************************************************************************/
/*                   VSIPluginFilesystemHandler()                         */
/************************************************************************/

VSIPluginFilesystemHandler::VSIPluginFilesystemHandler( const char *pszPrefix,
                                VSIFilesystemPluginStatCallback             stat,
                                VSIFilesystemPluginUnlinkCallback           unlink,
                                VSIFilesystemPluginRenameCallback           rename,
                                VSIFilesystemPluginMkdirCallback            mkdir,
                                VSIFilesystemPluginRmdirCallback            rmdir,
                                VSIFilesystemPluginReadDirCallback          read_dir,
                                VSIFilesystemPluginOpenCallback             open,
                                VSIFilesystemPluginTellCallback             tell,
                                VSIFilesystemPluginSeekCallback             seek,
                                VSIFilesystemPluginReadCallback             read,
                                VSIFilesystemPluginReadMultiRangeCallback   read_multi_range,
                                VSIFilesystemPluginGetRangeStatusCallback   get_range_status,
                                VSIFilesystemPluginEofCallback              eof,
                                VSIFilesystemPluginWriteCallback            write,
                                VSIFilesystemPluginFlushCallback            flush,
                                VSIFilesystemPluginTruncateCallback         truncate,
                                VSIFilesystemPluginCloseCallback            close) :
    m_Prefix(pszPrefix),
    stat_cb(stat),
    unlink_cb(unlink),
    rename_cb(rename),
    mkdir_cb(mkdir),
    rmdir_cb(rmdir),
    read_dir_cb(read_dir),
    open_cb(open),
    tell_cb(tell),
    seek_cb(seek),
    read_cb(read),
    read_multi_range_cb(read_multi_range),
    get_range_status_cb(get_range_status),
    eof_cb(eof),
    write_cb(write),
    flush_cb(flush),
    truncate_cb(truncate),
    close_cb(close)
{
}

VSIPluginFilesystemHandler::~VSIPluginFilesystemHandler()
{
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIPluginFilesystemHandler::Open( const char *pszFilename,
                                                  const char *pszAccess,
                                                  bool bSetError )
{
    if( !IsValidFilename(pszFilename) )
        return nullptr;
    const void *cbData = open_cb(GetCallbackFilename(pszFilename), pszAccess);
    if (cbData == nullptr) {
        if (bSetError) {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "plugin callback failed to open %s", GetCallbackFilename(pszFilename));
        }
        return nullptr;
    }
    return new VSIPluginHandle(this, cbData);
}

const char* VSIPluginFilesystemHandler::GetCallbackFilename(const char *pszFilename) {
    return pszFilename + strlen(m_Prefix);
}

bool VSIPluginFilesystemHandler::IsValidFilename(const char *pszFilename) {
    if( !STARTS_WITH_CI(pszFilename, m_Prefix) )
        return false;
    return true;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIPluginFilesystemHandler::Stat( const char *pszFilename,
                                      VSIStatBufL *pStatBuf,
                                      int nFlags )
{
    if( !IsValidFilename(pszFilename) ) {
        errno = EBADF;
        return -1;
    }


    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    int nRet = 0;
    if ( stat_cb != nullptr ) {
        nRet = stat_cb(GetCallbackFilename(pszFilename), pStatBuf, nFlags);
    } else {
        nRet = -1;
    }
    return nRet;
}

int VSIPluginFilesystemHandler::Seek(const void *psData, vsi_l_offset nOffset, int nWhence) {
    if (seek_cb != nullptr) {
        return seek_cb(psData,static_cast<size_t>(nOffset),nWhence);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Seek not implemented for %s plugin", m_Prefix);
    return -1;
}

vsi_l_offset VSIPluginFilesystemHandler::Tell(const void *psData) {
    if (tell_cb != nullptr) {
        return tell_cb(psData);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Tell not implemented for %s plugin", m_Prefix);
    return -1;
}

size_t VSIPluginFilesystemHandler::Read(const void *psData, void *pBuffer, size_t nSize, size_t nCount) {
    if (read_cb != nullptr) {
        return read_cb(psData, pBuffer, nSize, nCount);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Read not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::HasOptimizedReadMultiRange(const char* /*pszPath*/ ) {
    if (read_multi_range_cb != nullptr) {
        return TRUE;
    }
    return FALSE;
}

VSIRangeStatus VSIPluginFilesystemHandler::GetRangeStatus( const void *psData, vsi_l_offset nOffset, vsi_l_offset nLength) {
    if (get_range_status_cb != nullptr) {
        return get_range_status_cb(psData,nOffset,nLength);
    }
    return VSI_RANGE_STATUS_UNKNOWN;
}

int VSIPluginFilesystemHandler::ReadMultiRange( const void *psData, int nRanges, void ** ppData, const vsi_l_offset* panOffsets, const size_t* panSizes ) {
    if (read_multi_range_cb != nullptr) {
        return read_multi_range_cb(psData, nRanges, ppData, panOffsets, panSizes);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Read not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::Eof(const void *psData) {
    if (eof_cb != nullptr) {
        return eof_cb(psData);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Eof not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::Close(const void *psData) {
    if (close_cb != nullptr) {
        return close_cb(psData);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Close not implemented for %s plugin", m_Prefix);
    return -1;
}

size_t VSIPluginFilesystemHandler::Write(const void *psData, const void *psBuffer, size_t nSize, size_t nCount) {
    if (write_cb != nullptr) {
        return write_cb(psData,psBuffer,nSize,nCount);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Write not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::Flush(const void *psData) {
    if (flush_cb != nullptr) {
        return flush_cb(psData);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Flush not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::Truncate(const void *psData, vsi_l_offset nNewSize) {
    if (truncate_cb != nullptr) {
        return truncate_cb(psData, nNewSize);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Truncate not implemented for %s plugin", m_Prefix);
    return -1;

}

char ** VSIPluginFilesystemHandler::ReadDirEx( const char * pszDirname, int nMaxFiles ) {
    if( !IsValidFilename(pszDirname) )
        return nullptr;
    if (read_dir_cb != nullptr) {
        return read_dir_cb(GetCallbackFilename(pszDirname),nMaxFiles);
    }
    return nullptr;
}

int VSIPluginFilesystemHandler::Unlink(const char *pszFilename) {
    if( unlink_cb == nullptr || !IsValidFilename(pszFilename) )
        return -1;
    return unlink_cb(GetCallbackFilename(pszFilename));
}
int VSIPluginFilesystemHandler::Rename(const char *oldpath, const char *newpath) {
    if( rename_cb == nullptr || !IsValidFilename(oldpath) || !IsValidFilename(newpath) )
        return -1;
    return rename_cb(GetCallbackFilename(oldpath), GetCallbackFilename(newpath));
}
int VSIPluginFilesystemHandler::Mkdir(const char *pszDirname, long nMode) {
    if( mkdir_cb == nullptr || !IsValidFilename(pszDirname) )
        return -1;
    return mkdir_cb(GetCallbackFilename(pszDirname), nMode);
}
int VSIPluginFilesystemHandler::Rmdir(const char *pszDirname) {
    if( rmdir_cb == nullptr || !IsValidFilename(pszDirname) )
        return -1;
    return rmdir_cb(GetCallbackFilename(pszDirname));
}
}
#endif

int VSIInstallPluginHandler( const char* pszPrefix, const VSIFilesystemPluginCallbacksStruct *poCb) {
    VSIFilesystemHandler* poHandler = new cpl::VSIPluginFilesystemHandler(pszPrefix,
                poCb->stat,
                poCb->unlink,
                poCb->rename,
                poCb->mkdir,
                poCb->rmdir,
                poCb->read_dir,
                poCb->open,
                poCb->tell,
                poCb->seek,
                poCb->read,
                poCb->read_multi_range,
                poCb->get_range_status,
                poCb->eof,
                poCb->write,
                poCb->flush,
                poCb->truncate,
                poCb->close);
    //TODO: check pszPrefix starts and ends with a /
    VSIFileManager::InstallHandler( pszPrefix, poHandler );
    return 0;
}

VSIFilesystemPluginCallbacksStruct* VSIAllocFilesystemPluginCallbacksStruct() {
    return static_cast<VSIFilesystemPluginCallbacksStruct*>(VSI_CALLOC_VERBOSE(1, sizeof(VSIFilesystemPluginCallbacksStruct)));
}
void VSIFreeFilesystemPluginCallbacksStruct(VSIFilesystemPluginCallbacksStruct* poCb) {
    CPLFree(poCb);
}
