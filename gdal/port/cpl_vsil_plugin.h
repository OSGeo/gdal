/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Declarations for vsi filesystem plugin handlers
 * Author:   Thomas Bonfort <thomas.bonfort@airbus.com>
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

#ifndef CPL_VSIL_PLUGIN_H_INCLUDED
#define CPL_VSIL_PLUGIN_H_INCLUDED

#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"

//! @cond Doxygen_Suppress

namespace cpl {

/************************************************************************/
/*                     VSIPluginFilesystemHandler                         */
/************************************************************************/

class VSIPluginHandle;

class VSIPluginFilesystemHandler : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIPluginFilesystemHandler)

private:
    const char*         m_Prefix;
    VSIFilesystemPluginStatCallback             stat_cb;
    VSIFilesystemPluginUnlinkCallback           unlink_cb;
    VSIFilesystemPluginRenameCallback           rename_cb;
    VSIFilesystemPluginMkdirCallback            mkdir_cb;
    VSIFilesystemPluginRmdirCallback            rmdir_cb;
    VSIFilesystemPluginReadDirCallback          read_dir_cb;
    VSIFilesystemPluginOpenCallback             open_cb;
    VSIFilesystemPluginTellCallback             tell_cb;
    VSIFilesystemPluginSeekCallback             seek_cb;
    VSIFilesystemPluginReadCallback             read_cb;
    VSIFilesystemPluginReadMultiRangeCallback   read_multi_range_cb;
    VSIFilesystemPluginGetRangeStatusCallback   get_range_status_cb;
    VSIFilesystemPluginEofCallback              eof_cb;
    VSIFilesystemPluginWriteCallback            write_cb;
    VSIFilesystemPluginFlushCallback            flush_cb;
    VSIFilesystemPluginTruncateCallback         truncate_cb;
    VSIFilesystemPluginCloseCallback            close_cb;

protected:
    friend class VSIPluginHandle;
    VSIPluginHandle* CreatePluginHandle(void *cbData);
    const char* GetCallbackFilename(const char* pszFilename);
    bool IsValidFilename(const char *pszFilename);

    vsi_l_offset    Tell( const void *psData );
    int             Seek( const void *psData, vsi_l_offset nOffset, int nWhence );
    size_t          Read( const void *psData, void *pBuffer, size_t nSize, size_t nCount );
    int             ReadMultiRange( const void *psData, int nRanges, void ** ppData, const vsi_l_offset* panOffsets, const size_t* panSizes );
    VSIRangeStatus  GetRangeStatus( const void *psData, vsi_l_offset nOffset, vsi_l_offset nLength );
    int             Eof( const void *psData );
    size_t          Write( const void *psData, const void *pBuffer, size_t nSize,size_t nCount);
    int             Flush( const void *psData );
    int             Truncate( const void *psData, vsi_l_offset nNewSize );
    int             Close( const void *psData );

public:
    VSIPluginFilesystemHandler( const char *pszPrefix,
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
                                VSIFilesystemPluginFlushCallback            fush,
                                VSIFilesystemPluginTruncateCallback         truncate,
                                VSIFilesystemPluginCloseCallback            close);
    ~VSIPluginFilesystemHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError ) override;

    int Stat        ( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags ) override;
    int Unlink      ( const char * pszFilename ) override;
    int Rename      ( const char * oldpath, const char * /*newpath*/ ) override;
    int Mkdir       ( const char * pszDirname, long nMode ) override;
    int Rmdir       ( const char * pszDirname ) override;
    char **ReadDir  ( const char *pszDirname ) override
                        { return ReadDirEx(pszDirname, 0); }
    char **ReadDirEx( const char * pszDirname, int nMaxFiles ) override;
    int HasOptimizedReadMultiRange(const char* pszPath ) override;
    
};

/************************************************************************/
/*                           VSIPluginHandle                              */
/************************************************************************/

class VSIPluginHandle : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIPluginHandle)

  protected:
    VSIPluginFilesystemHandler* poFS;
    const void *cbData;

  public:

    VSIPluginHandle( VSIPluginFilesystemHandler* poFS, const void *cbData);
    ~VSIPluginHandle() override;

    vsi_l_offset    Tell() override;
    int             Seek( vsi_l_offset nOffset, int nWhence ) override;
    size_t          Read( void *pBuffer, size_t nSize, size_t nCount ) override;
    int             ReadMultiRange( int nRanges, void ** ppData, const vsi_l_offset* panOffsets, const size_t* panSizes ) override;
    VSIRangeStatus  GetRangeStatus( vsi_l_offset nOffset, vsi_l_offset nLength ) override;
    int             Eof() override;
    size_t          Write( const void *pBuffer, size_t nSize,size_t nCount) override;
    int             Flush() override;
    int             Truncate( vsi_l_offset nNewSize ) override;
    int             Close() override;
};

} // namespace cpl

//! @endcond

#endif // CPL_VSIL_PLUGIN_H_INCLUDED
