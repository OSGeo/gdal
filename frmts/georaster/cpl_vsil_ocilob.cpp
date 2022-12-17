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

// *****************************************************************************
//                                                             WSIOCILobFSHandle
// *****************************************************************************

class WSIOCILobFSHandle : public VSIFilesystemHandler
{
  public:
    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError,
                           CSLConstList /* papszOptions */) override;
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;

  private:
    char **ParseIdentificator(const char *pszFilename);
    OWConnection *GetConnection(char **papszParam);
    OWStatement *GetStatement(const char *tableName, const char *rasterid,
                              boolean bUpdate, OWConnection *pConnection);
};

// *****************************************************************************
//                                                               VSIOCILobHandle
// *****************************************************************************

class VSIOCILobHandle : public VSIVirtualHandle
{
  private:
    OWConnection *poConnection;
    OWStatement *poStatement;
    OCILobLocator *phLocator;
    GUIntBig nFileSize;
    GUIntBig nCurOff;
    boolean bUpdate;

  public:
    VSIOCILobHandle(OWConnection *poConnectionIn, OWStatement *poStatementIn,
                    OCILobLocator *phLocatorIn, boolean bUpdateIn);
    ~VSIOCILobHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Close() override;
};

// ****************************************************************************
// Implementation                                             WSIOCILobFSHandle
// ****************************************************************************

// -----------------------------------------------------------------------------
//                                                          ParseIdentificator()
// -----------------------------------------------------------------------------

char **WSIOCILobFSHandle::ParseIdentificator(const char *pszFilename)
{
    if (strncmp(pszFilename, "/vsiocilob/", strlen("/vsiocilob/")) != 0)
    {
        return nullptr;
    }

    char **papszParam =
        CSLTokenizeString2(&pszFilename[strlen("/vsiocilob/")], ",",
                           CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS |
                               CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);

    if (CSLCount(papszParam) < 6)
    {
        CSLDestroy(papszParam);
        return nullptr;
    }

    return papszParam;
}

// -----------------------------------------------------------------------------
//                                                                        GetConnection()
// -----------------------------------------------------------------------------

OWConnection *WSIOCILobFSHandle::GetConnection(char **papszParam)
{
    OWConnection *poConnection = nullptr;

    if (!papszParam)
    {
        return nullptr;
    }

    if (strlen(papszParam[0]) == 0 && strlen(papszParam[1]) == 0 &&
        strlen(papszParam[2]) == 0)
    {
        /* In an external procedure environment, before opening any
         * dataset, the caller must pass the with_context as an
         * string metadata item OCI_CONTEXT_PTR to the driver. */

        OCIExtProcContext *with_context = nullptr;

        const char *pszContext = GDALGetMetadataItem(
            GDALGetDriverByName("GEORASTER"), "OCI_CONTEXT_PTR", nullptr);

        if (pszContext)
        {
            sscanf(pszContext, "%p", &with_context);

            poConnection = new OWConnection(with_context);
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        poConnection =
            new OWConnection(papszParam[0], papszParam[1], papszParam[2]);
    }

    if (poConnection && !poConnection->Succeeded())
    {
        delete poConnection;

        return nullptr;
    }

    return poConnection;
}

// -----------------------------------------------------------------------------
//                                                                        GetStatement()
// -----------------------------------------------------------------------------
OWStatement *WSIOCILobFSHandle::GetStatement(const char *tableName,
                                             const char *rasterid,
                                             boolean bUpdate,
                                             OWConnection *pConnection)
{
    OWStatement *pStatement = nullptr;
    const char *pszUpdate = bUpdate ? "for update" : "";

    pStatement = pConnection->CreateStatement(CPLSPrintf(
        "select rasterblock from %s where rasterid = %s and rownum = 1 %s",
        tableName, rasterid, pszUpdate));

    return pStatement;
}

// -----------------------------------------------------------------------------
//                                                                        Open()
// -----------------------------------------------------------------------------

VSIVirtualHandle *WSIOCILobFSHandle::Open(const char *pszFilename,
                                          const char *pszAccess,
                                          bool /* bSetError*/,
                                          CSLConstList /* papszOptions */)
{
    char **papszParam = ParseIdentificator(pszFilename);

    if (!papszParam)
    {
        return nullptr;
    }

    if (!EQUAL(papszParam[5], "noext"))
    {
        CSLDestroy(papszParam);
        return nullptr;
    }

    // Get the connection
    OWConnection *poConnection = GetConnection(papszParam);
    if (!poConnection)
    {
        CSLDestroy(papszParam);
        return nullptr;
    }

    boolean bUpdate = false;

    if (strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, '+') != nullptr)
    {
        bUpdate = true;
    }

    // Get the statement
    OWStatement *poStatement =
        GetStatement(papszParam[3], papszParam[4], bUpdate, poConnection);
    if (!poStatement)
    {
        CSLDestroy(papszParam);
        delete poConnection;
        return nullptr;
    }

    // Get the lob locator
    OCILobLocator *phLocator = nullptr;
    poStatement->Define(&phLocator);
    if (!poStatement->Execute() || !phLocator)
    {
        CSLDestroy(papszParam);
        delete poConnection;
        delete poStatement;
        return nullptr;
    }

    CPLDebug("GEOR", "VSIOCILOB open successfully");
    CSLDestroy(papszParam);

    return new VSIOCILobHandle(poConnection, poStatement, phLocator, bUpdate);
}

// -----------------------------------------------------------------------------
//                                                                        Unlink()
// -----------------------------------------------------------------------------
int WSIOCILobFSHandle::Unlink(const char *pszFilename)
{

    char **papszParam = ParseIdentificator(pszFilename);

    if (!papszParam)
    {
        return -1;
    }

    if (!EQUAL(papszParam[5], "noext"))
    {
        CSLDestroy(papszParam);
        return -1;
    }

    CPLDebug("GEOR", "Unlink VSIOCILOB file");

    // Get the connection
    OWConnection *poConnection = GetConnection(papszParam);
    if (!poConnection)
    {
        CSLDestroy(papszParam);
        return -1;
    }

    // Get the statement
    OWStatement *poStatement =
        GetStatement(papszParam[3], papszParam[4], true, poConnection);
    if (!poStatement)
    {
        CSLDestroy(papszParam);
        delete poConnection;
        return -1;
    }

    // Get the lob locator
    OCILobLocator *phLocator = nullptr;
    poStatement->Define(&phLocator);
    if (!poStatement->Execute() || !phLocator)
    {
        CSLDestroy(papszParam);
        delete poConnection;
        delete poStatement;
        return -1;
    }

    // Trim the lob
    if (poStatement->GetBlobLength(phLocator) > 0)
    {
        CPLDebug("GEOR", "Trim the LOB");
        poStatement->TrimLob(phLocator, 0);
        poConnection->Commit();
        CPLDebug("GEOR", "LOB trimmed");
    }

    // Destroy the objects
    OWStatement::Free(&phLocator, 1);
    delete poStatement;
    delete poConnection;
    CSLDestroy(papszParam);

    return 0;
}

// -----------------------------------------------------------------------------
//                                                                        Stat()
// -----------------------------------------------------------------------------

int WSIOCILobFSHandle::Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
                            int nFlags)
{
    (void)nFlags;

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    char **papszParam = ParseIdentificator(pszFilename);

    if (!papszParam)
    {
        return -1;
    }

    if (strcmp(papszParam[5], "noext") != 0)
    {
        CSLDestroy(papszParam);
        return -1;
    }

    // Get the connection
    OWConnection *poConnection = GetConnection(papszParam);
    if (!poConnection)
    {
        CSLDestroy(papszParam);
        return -1;
    }

    // Get the statement
    OWStatement *poStatement =
        GetStatement(papszParam[3], papszParam[4], false, poConnection);
    if (!poStatement)
    {
        CSLDestroy(papszParam);
        delete poConnection;
        return -1;
    }

    // Get the lob locator
    OCILobLocator *phLocator = nullptr;
    poStatement->Define(&phLocator);
    if (!poStatement->Execute() || !phLocator)
    {
        CSLDestroy(papszParam);
        delete poConnection;
        delete poStatement;
        return -1;
    }

    // Get the lob length
    pStatBuf->st_size = poStatement->GetBlobLength(phLocator);
    pStatBuf->st_mode = S_IFREG;

    // Destroy the objects
    OWStatement::Free(&phLocator, 1);
    delete poStatement;
    delete poConnection;
    CSLDestroy(papszParam);

    return 0;
}

// ****************************************************************************
// Implementation                                               VSIOCILobHandle
// ****************************************************************************

// ----------------------------------------------------------------------------
//                                                            VSIOCILobHandle()
// ----------------------------------------------------------------------------

VSIOCILobHandle::VSIOCILobHandle(OWConnection *poConnectionIn,
                                 OWStatement *poStatementIn,
                                 OCILobLocator *phLocatorIn, boolean bUpdateIn)
    : poConnection(poConnectionIn), poStatement(poStatementIn),
      phLocator(phLocatorIn), bUpdate(bUpdateIn)
{
    nCurOff = 0;

    nFileSize = poStatement->GetBlobLength(phLocator);
}

// ----------------------------------------------------------------------------
//                                                           ~VSIOCILobHandle()
// ----------------------------------------------------------------------------

VSIOCILobHandle::~VSIOCILobHandle()
{
    CPLDebug("GEOR", "Destroy the vsiocilob handle");
    if (phLocator)
    {
        OWStatement::Free(&phLocator, 1);
    }

    if (poStatement)
    {
        delete poStatement;
    }

    if (poConnection)
    {
        delete poConnection;
    }
}

// ----------------------------------------------------------------------------
//                                                                       Seek()
// ----------------------------------------------------------------------------

int VSIOCILobHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
    if (nWhence == SEEK_END)
    {
        nOffset = poStatement->GetBlobLength(phLocator);
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

size_t VSIOCILobHandle::Read(void *pBuffer, size_t nSize, size_t nCount)
{
    GUIntBig nBytes = (nSize * nCount);

    if (nBytes == 0)
    {
        return 0;
    }

    GUIntBig nRead = poStatement->ReadBlob(
        phLocator, pBuffer, static_cast<unsigned long>(nCurOff + 1),
        static_cast<unsigned long>(nBytes));

    nCurOff += (GUIntBig)nRead;

    return (size_t)(nRead / nSize);
}

// ----------------------------------------------------------------------------
//                                                                      Write()
// ----------------------------------------------------------------------------

size_t VSIOCILobHandle::Write(const void *pBuffer, size_t nSize, size_t nCount)
{
    GUIntBig nBytes = (nSize * nCount);

    if (nBytes == 0)
    {
        return 0;
    }

    GUIntBig nWrite = poStatement->WriteBlob(
        phLocator, (void *)pBuffer, static_cast<unsigned long>(nCurOff + 1),
        static_cast<unsigned long>(nBytes));

    nCurOff += (GUIntBig)nWrite;

    return (size_t)(nWrite / nSize);
}

// ----------------------------------------------------------------------------
//                                                                        Eof()
// ----------------------------------------------------------------------------

int VSIOCILobHandle::Eof()
{
    return (int)(nCurOff >= nFileSize);
}

// ----------------------------------------------------------------------------
//                                                                      Close()
// ----------------------------------------------------------------------------

int VSIOCILobHandle::Close()
{
    if (bUpdate)
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
    VSIFileManager::InstallHandler("/vsiocilob/", new WSIOCILobFSHandle);
}
