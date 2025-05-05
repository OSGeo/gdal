/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Declarations for vsi filesystem plugin handlers
 * Author:   Thomas Bonfort <thomas.bonfort@airbus.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Thomas Bonfort <thomas.bonfort@airbus.com>
 *
 * SPDX-License-Identifier: MIT
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

    vsi_l_offset Tell(void *pFile);
    int Seek(void *pFile, vsi_l_offset nOffset, int nWhence);
    size_t Read(void *pFile, void *pBuffer, size_t nSize, size_t nCount);
    int ReadMultiRange(void *pFile, int nRanges, void **ppData,
                       const vsi_l_offset *panOffsets, const size_t *panSizes);
    void AdviseRead(void *pFile, int nRanges, const vsi_l_offset *panOffsets,
                    const size_t *panSizes);
    VSIRangeStatus GetRangeStatus(void *pFile, vsi_l_offset nOffset,
                                  vsi_l_offset nLength);
    int Eof(void *pFile);
    int Error(void *pFile);
    void ClearErr(void *pFile);
    size_t Write(void *pFile, const void *pBuffer, size_t nSize, size_t nCount);
    int Flush(void *pFile);
    int Truncate(void *pFile, vsi_l_offset nNewSize);
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
    int Rename(const char *oldpath, const char * /*newpath*/, GDALProgressFunc,
               void *) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
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

    vsi_l_offset Tell() override;
    int Seek(vsi_l_offset nOffset, int nWhence) override;
    size_t Read(void *pBuffer, size_t nSize, size_t nCount) override;
    int ReadMultiRange(int nRanges, void **ppData,
                       const vsi_l_offset *panOffsets,
                       const size_t *panSizes) override;
    void AdviseRead(int nRanges, const vsi_l_offset *panOffsets,
                    const size_t *panSizes) override;
    VSIRangeStatus GetRangeStatus(vsi_l_offset nOffset,
                                  vsi_l_offset nLength) override;
    void ClearErr() override;
    int Eof() override;
    int Error() override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nCount) override;
    int Flush() override;
    int Truncate(vsi_l_offset nNewSize) override;
    int Close() override;
};

}  // namespace cpl

//! @endcond

#endif  // CPL_VSIL_PLUGIN_H_INCLUDED
