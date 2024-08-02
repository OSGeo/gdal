/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#ifndef VSIARROWFILESYSTEM_HPP_INCLUDED
#define VSIARROWFILESYSTEM_HPP_INCLUDED

#include "arrow/util/config.h"

#include "ograrrowrandomaccessfile.h"

/************************************************************************/
/*                         VSIArrowFileSystem                           */
/************************************************************************/

class VSIArrowFileSystem final : public arrow::fs::FileSystem
{
    const std::string m_osEnvVarPrefix;
    const std::string m_osQueryParameters;

  public:
    explicit VSIArrowFileSystem(const std::string &osEnvVarPrefix,
                                const std::string &osQueryParameters)
        : m_osEnvVarPrefix(osEnvVarPrefix),
          m_osQueryParameters(osQueryParameters)
    {
    }

    std::string type_name() const override
    {
        return "vsi" + m_osEnvVarPrefix;
    }

    using arrow::fs::FileSystem::Equals;

    bool Equals(const arrow::fs::FileSystem &other) const override
    {
        const auto poOther = dynamic_cast<const VSIArrowFileSystem *>(&other);
        return poOther != nullptr &&
               poOther->m_osEnvVarPrefix == m_osEnvVarPrefix &&
               poOther->m_osQueryParameters == m_osQueryParameters;
    }

    using arrow::fs::FileSystem::GetFileInfo;

    arrow::Result<arrow::fs::FileInfo>
    GetFileInfo(const std::string &path) override
    {
        auto fileType = arrow::fs::FileType::Unknown;
        VSIStatBufL sStat;
        if (VSIStatL(path.c_str(), &sStat) == 0)
        {
            if (VSI_ISREG(sStat.st_mode))
                fileType = arrow::fs::FileType::File;
            else if (VSI_ISDIR(sStat.st_mode))
                fileType = arrow::fs::FileType::Directory;
        }
        else
        {
            fileType = arrow::fs::FileType::NotFound;
        }
        arrow::fs::FileInfo info(path, fileType);
        if (fileType == arrow::fs::FileType::File)
            info.set_size(sStat.st_size);
        return info;
    }

    arrow::Result<arrow::fs::FileInfoVector>
    GetFileInfo(const arrow::fs::FileSelector &select) override
    {
        arrow::fs::FileInfoVector res;
        VSIDIR *psDir = VSIOpenDir(select.base_dir.c_str(),
                                   select.recursive ? -1 : 0, nullptr);
        if (psDir == nullptr)
            return res;

        bool bParquetFound = false;
        const int nMaxNonParquetFiles = atoi(
            CPLGetConfigOption("OGR_PARQUET_MAX_NON_PARQUET_FILES", "100"));
        const int nMaxListedFiles =
            atoi(CPLGetConfigOption("OGR_PARQUET_MAX_LISTED_FILES", "1000000"));
        while (const auto psEntry = VSIGetNextDirEntry(psDir))
        {
            if (!bParquetFound)
                bParquetFound =
                    EQUAL(CPLGetExtension(psEntry->pszName), "parquet");

            const std::string osFilename =
                select.base_dir + '/' + psEntry->pszName;
            int nMode = psEntry->nMode;
            if (!psEntry->bModeKnown)
            {
                VSIStatBufL sStat;
                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                    nMode = sStat.st_mode;
            }

            auto fileType = arrow::fs::FileType::Unknown;
            if (VSI_ISREG(nMode))
                fileType = arrow::fs::FileType::File;
            else if (VSI_ISDIR(nMode))
                fileType = arrow::fs::FileType::Directory;

            arrow::fs::FileInfo info(osFilename, fileType);
            if (fileType == arrow::fs::FileType::File && psEntry->bSizeKnown)
            {
                info.set_size(psEntry->nSize);
            }
            res.push_back(info);

            if (m_osEnvVarPrefix == "PARQUET")
            {
                // Avoid iterating over too many files if there's no likely parquet
                // files.
                if (static_cast<int>(res.size()) == nMaxNonParquetFiles &&
                    !bParquetFound)
                    break;
                if (static_cast<int>(res.size()) == nMaxListedFiles)
                    break;
            }
        }
        VSICloseDir(psDir);
        return res;
    }

    arrow::Status CreateDir(const std::string & /*path*/,
                            bool /*recursive*/ = true) override
    {
        return arrow::Status::IOError("CreateDir() unimplemented");
    }

    arrow::Status DeleteDir(const std::string & /*path*/) override
    {
        return arrow::Status::IOError("DeleteDir() unimplemented");
    }

    arrow::Status DeleteDirContents(const std::string & /*path*/
#if ARROW_VERSION_MAJOR >= 8
                                    ,
                                    bool /*missing_dir_ok*/ = false
#endif
                                    ) override
    {
        return arrow::Status::IOError("DeleteDirContents() unimplemented");
    }

    arrow::Status DeleteRootDirContents() override
    {
        return arrow::Status::IOError("DeleteRootDirContents() unimplemented");
    }

    arrow::Status DeleteFile(const std::string & /*path*/) override
    {
        return arrow::Status::IOError("DeleteFile() unimplemented");
    }

    arrow::Status Move(const std::string & /*src*/,
                       const std::string & /*dest*/) override
    {
        return arrow::Status::IOError("Move() unimplemented");
    }

    arrow::Status CopyFile(const std::string & /*src*/,
                           const std::string & /*dest*/) override
    {
        return arrow::Status::IOError("CopyFile() unimplemented");
    }

    using arrow::fs::FileSystem::OpenInputStream;

    arrow::Result<std::shared_ptr<arrow::io::InputStream>>
    OpenInputStream(const std::string &path) override
    {
        return OpenInputFile(path);
    }

    using arrow::fs::FileSystem::OpenInputFile;

    arrow::Result<std::shared_ptr<arrow::io::RandomAccessFile>>
    OpenInputFile(const std::string &path) override
    {
        std::string osPath(path);
        osPath += m_osQueryParameters;
        CPLDebugOnly(m_osEnvVarPrefix.c_str(), "Opening %s", osPath.c_str());
        auto fp = VSIVirtualHandleUniquePtr(VSIFOpenL(osPath.c_str(), "rb"));
        if (fp == nullptr)
            return arrow::Status::IOError("OpenInputFile() failed for " +
                                          osPath);
        return std::make_shared<OGRArrowRandomAccessFile>(std::move(fp));
    }

    using arrow::fs::FileSystem::OpenOutputStream;

    arrow::Result<std::shared_ptr<arrow::io::OutputStream>>
    OpenOutputStream(const std::string & /*path*/,
                     const std::shared_ptr<const arrow::KeyValueMetadata>
                         & /* metadata */) override
    {
        return arrow::Status::IOError("OpenOutputStream() unimplemented");
    }

    arrow::Result<std::shared_ptr<arrow::io::OutputStream>>
    OpenAppendStream(const std::string & /*path*/,
                     const std::shared_ptr<const arrow::KeyValueMetadata>
                         & /* metadata */) override
    {
        return arrow::Status::IOError("OpenAppendStream() unimplemented");
    }
};

#endif  // VSIARROWFILESYSTEM_HPP_INCLUDED
