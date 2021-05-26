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


VSIPluginHandle::VSIPluginHandle( VSIPluginFilesystemHandler* poFSIn,
                                  void *cbDataIn) :
    poFS(poFSIn),
    cbData(cbDataIn)
{
}


VSIPluginHandle::~VSIPluginHandle()
{
    if (cbData) {
        VSIPluginHandle::Close();
    }
}



int VSIPluginHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    return poFS->Seek(cbData,nOffset,nWhence);
}



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


VSIPluginFilesystemHandler::VSIPluginFilesystemHandler( const char *pszPrefix,
                                const VSIFilesystemPluginCallbacksStruct *cbIn):
    m_Prefix(pszPrefix),
    m_cb(nullptr)
{
    m_cb = new VSIFilesystemPluginCallbacksStruct(*cbIn);
}

VSIPluginFilesystemHandler::~VSIPluginFilesystemHandler()
{
    delete m_cb;
}



VSIVirtualHandle* VSIPluginFilesystemHandler::Open( const char *pszFilename,
                                                    const char *pszAccess,
                                                    bool bSetError,
                                                    CSLConstList /* papszOptions */ )
{
    if( !IsValidFilename(pszFilename) )
        return nullptr;
    void *cbData = m_cb->open(m_cb->pUserData, GetCallbackFilename(pszFilename), pszAccess);
    if (cbData == nullptr) {
        if (bSetError) {
            VSIError(VSIE_FileError, "%s: %s", pszFilename, strerror(errno));
        }
        return nullptr;
    }
    if ( m_cb->nBufferSize==0 ) {
        return new VSIPluginHandle(this, cbData);
    } else {
        return VSICreateCachedFile(
            new VSIPluginHandle(this, cbData), m_cb->nBufferSize,
            (m_cb->nCacheSize<m_cb->nBufferSize) ? m_cb->nBufferSize : m_cb->nCacheSize);
    }
}

const char* VSIPluginFilesystemHandler::GetCallbackFilename(const char *pszFilename) {
    return pszFilename + strlen(m_Prefix);
}

bool VSIPluginFilesystemHandler::IsValidFilename(const char *pszFilename) {
    if( !STARTS_WITH_CI(pszFilename, m_Prefix) )
        return false;
    return true;
}


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
    if ( m_cb->stat != nullptr ) {
        nRet = m_cb->stat(m_cb->pUserData, GetCallbackFilename(pszFilename), pStatBuf, nFlags);
    } else {
        nRet = -1;
    }
    return nRet;
}

int VSIPluginFilesystemHandler::Seek(void *pFile, vsi_l_offset nOffset, int nWhence) {
    if (m_cb->seek != nullptr) {
        return m_cb->seek(pFile,nOffset,nWhence);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Seek not implemented for %s plugin", m_Prefix);
    return -1;
}

vsi_l_offset VSIPluginFilesystemHandler::Tell(void *pFile) {
    if (m_cb->tell != nullptr) {
        return m_cb->tell(pFile);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Tell not implemented for %s plugin", m_Prefix);
    return -1;
}

size_t VSIPluginFilesystemHandler::Read(void *pFile, void *pBuffer, size_t nSize, size_t nCount) {
    if (m_cb->read != nullptr) {
        return m_cb->read(pFile, pBuffer, nSize, nCount);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Read not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::HasOptimizedReadMultiRange(const char* /*pszPath*/ ) {
    if (m_cb->read_multi_range != nullptr) {
        return TRUE;
    }
    return FALSE;
}

VSIRangeStatus VSIPluginFilesystemHandler::GetRangeStatus( void *pFile, vsi_l_offset nOffset, vsi_l_offset nLength) {
    if (m_cb->get_range_status != nullptr) {
        return m_cb->get_range_status(pFile,nOffset,nLength);
    }
    return VSI_RANGE_STATUS_UNKNOWN;
}

int VSIPluginFilesystemHandler::ReadMultiRange( void *pFile, int nRanges, void ** ppData, const vsi_l_offset* panOffsets, const size_t* panSizes ) {
    if (m_cb->read_multi_range == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined, "Read not implemented for %s plugin", m_Prefix);
        return -1;
    }
    int iRange;
    int nMergedRanges = 1;
    for ( iRange=0; iRange<nRanges-1; iRange++ ) {
        if ( panOffsets[iRange] + panSizes[iRange] != panOffsets[iRange+1] ) {
            nMergedRanges++;
        }
    }
    if ( nMergedRanges == nRanges ) {
        return m_cb->read_multi_range(pFile, nRanges, ppData, panOffsets, panSizes);
    }
    
    vsi_l_offset *mOffsets = new vsi_l_offset[nMergedRanges];
    size_t *mSizes = new size_t[nMergedRanges];
    char **mData = new char*[nMergedRanges];

    int curRange = 0;
    mSizes[curRange] = panSizes[0];
    mOffsets[curRange] = panOffsets[0];
    for ( iRange=0; iRange<nRanges-1; iRange++ ) {
        if ( panOffsets[iRange] + panSizes[iRange] == panOffsets[iRange+1] ) {
            mSizes[curRange] += panSizes[iRange+1];
        } else {
            mData[curRange] = new char[mSizes[curRange]];
            //start a new range
            curRange++;
            mSizes[curRange] = panSizes[iRange+1];
            mOffsets[curRange] = panOffsets[iRange+1];
        }
    }
    mData[curRange] = new char[mSizes[curRange]];

    int ret = m_cb->read_multi_range(pFile, nMergedRanges, reinterpret_cast<void**>(mData), mOffsets, mSizes);
    
    curRange = 0;
    size_t curOffset = panSizes[0];
    memcpy(ppData[0],mData[0],panSizes[0]);
    for ( iRange=0; iRange<nRanges-1; iRange++ ) {
        if ( panOffsets[iRange] + panSizes[iRange] == panOffsets[iRange+1] ) {
            memcpy(ppData[iRange+1], mData[curRange]+curOffset, panSizes[iRange+1]);
            curOffset += panSizes[iRange+1];
        } else {
            curRange++;
            memcpy(ppData[iRange+1], mData[curRange], panSizes[iRange+1]);
            curOffset = panSizes[iRange+1];
        }
    }

    delete[] mOffsets;
    delete[] mSizes;
    for ( int i=0; i<nMergedRanges; i++ ) {
        delete[] mData[i];
    }
    delete[] mData;

    return ret;
}

int VSIPluginFilesystemHandler::Eof(void *pFile) {
    if (m_cb->eof != nullptr) {
        return m_cb->eof(pFile);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Eof not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::Close(void *pFile) {
    if (m_cb->close != nullptr) {
        return m_cb->close(pFile);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Close not implemented for %s plugin", m_Prefix);
    return -1;
}

size_t VSIPluginFilesystemHandler::Write(void *pFile, const void *psBuffer, size_t nSize, size_t nCount) {
    if (m_cb->write != nullptr) {
        return m_cb->write(pFile,psBuffer,nSize,nCount);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Write not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::Flush(void *pFile) {
    if (m_cb->flush != nullptr) {
        return m_cb->flush(pFile);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Flush not implemented for %s plugin", m_Prefix);
    return -1;
}

int VSIPluginFilesystemHandler::Truncate(void *pFile, vsi_l_offset nNewSize) {
    if (m_cb->truncate != nullptr) {
        return m_cb->truncate(pFile, nNewSize);
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Truncate not implemented for %s plugin", m_Prefix);
    return -1;

}

char ** VSIPluginFilesystemHandler::ReadDirEx( const char * pszDirname, int nMaxFiles ) {
    if( !IsValidFilename(pszDirname) )
        return nullptr;
    if (m_cb->read_dir != nullptr) {
        return m_cb->read_dir(m_cb->pUserData, GetCallbackFilename(pszDirname),nMaxFiles);
    }
    return nullptr;
}

char ** VSIPluginFilesystemHandler::SiblingFiles( const char * pszFilename ) {
    if( !IsValidFilename(pszFilename) )
        return nullptr;
    if (m_cb->sibling_files != nullptr) {
        return m_cb->sibling_files(m_cb->pUserData, GetCallbackFilename(pszFilename));
    }
    return nullptr;
}

int VSIPluginFilesystemHandler::Unlink(const char *pszFilename) {
    if( m_cb->unlink == nullptr || !IsValidFilename(pszFilename) )
        return -1;
    return unlink(GetCallbackFilename(pszFilename));
}
int VSIPluginFilesystemHandler::Rename(const char *oldpath, const char *newpath) {
    if( m_cb->rename == nullptr || !IsValidFilename(oldpath) || !IsValidFilename(newpath) )
        return -1;
    return m_cb->rename(m_cb->pUserData, GetCallbackFilename(oldpath), GetCallbackFilename(newpath));
}
int VSIPluginFilesystemHandler::Mkdir(const char *pszDirname, long nMode) {
    if( m_cb->mkdir == nullptr || !IsValidFilename(pszDirname) )
        return -1;
    return m_cb->mkdir(m_cb->pUserData, GetCallbackFilename(pszDirname), nMode);
}
int VSIPluginFilesystemHandler::Rmdir(const char *pszDirname) {
    if( m_cb->rmdir == nullptr || !IsValidFilename(pszDirname) )
        return -1;
    return m_cb->rmdir(m_cb->pUserData, GetCallbackFilename(pszDirname));
}
} // namespace cpl

#endif //DOXYGEN_SKIP
//! @endcond

int VSIInstallPluginHandler( const char* pszPrefix, const VSIFilesystemPluginCallbacksStruct *poCb) {
    VSIFilesystemHandler* poHandler = new cpl::VSIPluginFilesystemHandler(pszPrefix, poCb);
    //TODO: check pszPrefix starts and ends with a /
    VSIFileManager::InstallHandler( pszPrefix, poHandler );
    return 0;
}

VSIFilesystemPluginCallbacksStruct* VSIAllocFilesystemPluginCallbacksStruct( void ) {
    return static_cast<VSIFilesystemPluginCallbacksStruct*>(VSI_CALLOC_VERBOSE(1, sizeof(VSIFilesystemPluginCallbacksStruct)));
}
void VSIFreeFilesystemPluginCallbacksStruct(VSIFilesystemPluginCallbacksStruct* poCb) {
    CPLFree(poCb);
}
