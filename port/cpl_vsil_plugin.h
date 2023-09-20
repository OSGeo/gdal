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

namespace cpl
{

/************************************************************************/
/*                     VSIPluginFilesystemHandler                         */
/************************************************************************/

class VSIPluginHandle;

class VSIPluginFilesystemHandler : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIPluginFilesystemHandler)

  private:
    const char *m_Prefix;
    const VSIFilesystemPluginCallbacksStruct *m_cb;
    bool m_bWarnedAdviseReadImplemented = false;

  protected:
    friend class VSIPluginHandle;
    VSIPluginHandle *CreatePluginHandle(void *cbData);
    const char *GetCallbackFilename(const char *pszFilename);
    bool IsValidFilename(const char *pszFilename);

    uint64_t Tell(void *pFile);
    int Seek(void *pFile, uint64_t nOffset, int nWhence);
    size_t Read(void *pFile, void *pBuffer, size_t nSize, size_t nCount);
    int ReadMultiRange(void *pFile, int nRanges, void **ppData,
                       const uint64_t *panOffsets, const size_t *panSizes);
    void AdviseRead(void *pFile, int nRanges, const uint64_t *panOffsets,
                    const size_t *panSizes);
    VSIRangeStatus GetRangeStatus(void *pFile, uint64_t nOffset,
                                  uint64_t nLength);
    int Eof(void *pFile);
    size_t Write(void *pFile, const void *pBuffer, size_t nSize, size_t nCount);
    int Flush(void *pFile);
    int Truncate(void *pFile, uint64_t nNewSize);
    int Close(void *pFile);

  public:
    VSIPluginFilesystemHandler(const char *pszPrefix,
                               const VSIFilesystemPluginCallbacksStruct *cb);
    ~VSIPluginFilesystemHandler() override;

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError,
                           CSLConstList /* papszOptions */) override;

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;
    int Rename(const char *oldpath, const char * /*newpath*/) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    char **ReadDir(const char *pszDirname) override
    {
        return ReadDirEx(pszDirname, 0);
    }
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;
    char **SiblingFiles(const char *pszFilename) override;
    int HasOptimizedReadMultiRange(const char *pszPath) override;
};

/************************************************************************/
/*                           VSIPluginHandle                              */
/************************************************************************/

class VSIPluginHandle : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIPluginHandle)

  protected:
    VSIPluginFilesystemHandler *poFS;
    void *cbData;

  public:
    VSIPluginHandle(VSIPluginFilesystemHandler *poFS, void *cbData);
    ~VSIPluginHandle() override;

    uint64_t Tell() override;
    int Seek(uint64_t nOffset, int nWhence) override;
    size_t Read(void *pBuffer, size_t nSize, size_t nCount) override;
    int ReadMultiRange(int nRanges, void **ppData, const uint64_t *panOffsets,
                       const size_t *panSizes) override;
    void AdviseRead(int nRanges, const uint64_t *panOffsets,
                    const size_t *panSizes) override;
    VSIRangeStatus GetRangeStatus(uint64_t nOffset, uint64_t nLength) override;
    int Eof() override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nCount) override;
    int Flush() override;
    int Truncate(uint64_t nNewSize) override;
    int Close() override;
};

}  // namespace cpl

//! @endcond

#endif  // CPL_VSIL_PLUGIN_H_INCLUDED
