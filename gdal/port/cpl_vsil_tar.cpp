/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for tar files (.tar).
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$");


/************************************************************************/
/* ==================================================================== */
/*                       VSITarEntryFileOffset                          */
/* ==================================================================== */
/************************************************************************/

class VSITarEntryFileOffset : public VSIArchiveEntryFileOffset
{
public:
        GUIntBig nOffset;

        VSITarEntryFileOffset(GUIntBig nOffset)
        {
            this->nOffset = nOffset;
        }
};

/************************************************************************/
/* ==================================================================== */
/*                             VSITarReader                             */
/* ==================================================================== */
/************************************************************************/

class VSITarReader : public VSIArchiveReader
{
    private:
        FILE* fp;
        GUIntBig nCurOffset;
        GUIntBig nNextFileSize;
        CPLString osNextFileName;
        GIntBig nModifiedTime;

    public:
        VSITarReader(const char* pszTarFileName);
        virtual ~VSITarReader();

        int IsValid() { return fp != NULL; }

        virtual int GotoFirstFile();
        virtual int GotoNextFile();
        virtual VSIArchiveEntryFileOffset* GetFileOffset() { return new VSITarEntryFileOffset(nCurOffset); }
        virtual GUIntBig GetFileSize() { return nNextFileSize; }
        virtual CPLString GetFileName() { return osNextFileName; }
        virtual GIntBig GetModifiedTime() { return nModifiedTime; }
        virtual int GotoFileOffset(VSIArchiveEntryFileOffset* pOffset);
};


/************************************************************************/
/*                               VSIIsTGZ()                             */
/************************************************************************/

static int VSIIsTGZ(const char* pszFilename)
{
    return (!EQUALN(pszFilename, "/vsigzip/", 9) &&
            ((strlen(pszFilename) > 4 &&
            EQUALN(pszFilename + strlen(pszFilename) - 4, ".tgz", 4)) ||
            (strlen(pszFilename) > 7 &&
            EQUALN(pszFilename + strlen(pszFilename) - 7, ".tar.gz", 7))));
}

/************************************************************************/
/*                           VSITarReader()                             */
/************************************************************************/

VSITarReader::VSITarReader(const char* pszTarFileName)
{
    fp = VSIFOpenL(pszTarFileName, "rb");
    nNextFileSize = 0;
    nCurOffset = 0;
    nModifiedTime = 0;
}

/************************************************************************/
/*                          ~VSITarReader()                             */
/************************************************************************/

VSITarReader::~VSITarReader()
{
    if (fp)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                           GotoNextFile()                             */
/************************************************************************/

int VSITarReader::GotoNextFile()
{
    char abyHeader[512];
    if (VSIFReadL(abyHeader, 512, 1, fp) != 1)
        return FALSE;

    if (abyHeader[99] != '\0' ||
        abyHeader[107] != '\0' ||
        abyHeader[115] != '\0' ||
        abyHeader[123] != '\0' ||
        abyHeader[135] != '\0' ||
        abyHeader[147] != '\0' ||
        abyHeader[154] != '\0' ||
        abyHeader[155] != ' ')
        return FALSE;

    osNextFileName = abyHeader;
    nNextFileSize = 0;
    int i;
    for(i=0;i<11;i++)
        nNextFileSize = nNextFileSize * 8 + (abyHeader[124+i] - '0');

    nModifiedTime = 0;
    for(i=0;i<11;i++)
        nModifiedTime = nModifiedTime * 8 + (abyHeader[136+i] - '0');

    nCurOffset = VSIFTellL(fp);

    GUIntBig nBytesToSkip = ((nNextFileSize + 511) / 512) * 512;
    VSIFSeekL(fp, nBytesToSkip, SEEK_CUR);

    return TRUE;
}

/************************************************************************/
/*                          GotoFirstFile()                             */
/************************************************************************/

int VSITarReader::GotoFirstFile()
{
    VSIFSeekL(fp, 0, SEEK_SET);
    return GotoNextFile();
}

/************************************************************************/
/*                         GotoFileOffset()                             */
/************************************************************************/

int VSITarReader::GotoFileOffset(VSIArchiveEntryFileOffset* pOffset)
{
    VSITarEntryFileOffset* pTarEntryOffset = (VSITarEntryFileOffset*)pOffset;
    VSIFSeekL(fp, pTarEntryOffset->nOffset - 512, SEEK_SET);
    return GotoNextFile();
}

/************************************************************************/
/* ==================================================================== */
/*                        VSITarFilesystemHandler                      */
/* ==================================================================== */
/************************************************************************/

class VSITarFilesystemHandler : public VSIArchiveFilesystemHandler 
{
public:
    virtual const char* GetPrefix() { return "/vsitar"; }
    virtual std::vector<CPLString> GetExtensions();
    virtual VSIArchiveReader* CreateReader(const char* pszTarFileName);

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
};


/************************************************************************/
/*                          GetExtensions()                             */
/************************************************************************/

std::vector<CPLString> VSITarFilesystemHandler::GetExtensions()
{
    std::vector<CPLString> oList;
    oList.push_back(".tar.gz");
    oList.push_back(".tar");
    oList.push_back(".tgz");
    return oList;
}

/************************************************************************/
/*                           CreateReader()                             */
/************************************************************************/

VSIArchiveReader* VSITarFilesystemHandler::CreateReader(const char* pszTarFileName)
{
    CPLString osTarInFileName;

    if (VSIIsTGZ(pszTarFileName))
    {
        osTarInFileName = "/vsigzip/";
        osTarInFileName += pszTarFileName;
    }
    else
        osTarInFileName = pszTarFileName;

    VSITarReader* poReader = new VSITarReader(osTarInFileName);

    if (!poReader->IsValid())
    {
        delete poReader;
        return NULL;
    }

    if (!poReader->GotoFirstFile())
    {
        delete poReader;
        return NULL;
    }

    return poReader;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

VSIVirtualHandle* VSITarFilesystemHandler::Open( const char *pszFilename, 
                                                 const char *pszAccess)
{
    char* tarFilename;
    CPLString osTarInFileName;

    if (strchr(pszAccess, 'w') != NULL ||
        strchr(pszAccess, '+') != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for /vsitar");
        return NULL;
    }

    tarFilename = SplitFilename(pszFilename, osTarInFileName);
    if (tarFilename == NULL)
        return NULL;

    VSIArchiveReader* poReader = OpenArchiveFile(tarFilename, osTarInFileName);
    if (poReader == NULL)
    {
        CPLFree(tarFilename);
        return NULL;
    }

    CPLString osSubFileName("/vsisubfile/");
    VSITarEntryFileOffset* pOffset = (VSITarEntryFileOffset*) poReader->GetFileOffset();
    osSubFileName += CPLString().Printf(CPL_FRMT_GUIB, pOffset->nOffset);
    osSubFileName += "_";
    osSubFileName += CPLString().Printf(CPL_FRMT_GUIB, poReader->GetFileSize());
    osSubFileName += ",";
    delete pOffset;
    
    if (VSIIsTGZ(tarFilename))
    {
        osSubFileName += "/vsigzip/";
        osSubFileName += tarFilename;
    }
    else
        osSubFileName += tarFilename;

    delete(poReader);

    CPLFree(tarFilename);
    tarFilename = NULL;

    return (VSIVirtualHandle* )VSIFOpenL(osSubFileName, "rb");
}

/************************************************************************/
/*                    VSIInstallTarFileHandler()                        */
/************************************************************************/


/**
 * \brief Install TAR file system handler. 
 *
 * A special file handler is installed that allows reading on-the-fly in TAR (.tar, .tar.gz/.tgz) archives.
 * All portions of the file system underneath the base
 * path "/vsitar/" will be handled by this driver.
 *
 */

void VSIInstallTarFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsitar/", new VSITarFilesystemHandler() );
}
