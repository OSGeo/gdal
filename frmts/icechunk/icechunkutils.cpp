/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Icechunk driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "icechunkutils.h"
#include "icechunkdrivercore.h"

#include "cpl_compressor.h"

#include <limits>

namespace gdal::icechunk
{

/************************************************************************/
/*                     GetFilenameFromDatasetName()                     */
/************************************************************************/

std::string GetFilenameFromDatasetName(const std::string &osDatasetName,
                                       std::string &osBranchName,
                                       std::string &osTagName,
                                       bool &ignoreTimestampEtag)
{
    ignoreTimestampEtag = false;
    std::string osFilename = osDatasetName;
    if (STARTS_WITH_CI(osFilename.c_str(), ICECHUNK_PREFIX))
    {
        osFilename = osDatasetName.substr(strlen(ICECHUNK_PREFIX));
        const size_t nQuestionMarkPos = osFilename.find('?');
        if (nQuestionMarkPos != std::string::npos)
        {
            const std::string osSuffix =
                osFilename.substr(nQuestionMarkPos + 1);
            osFilename.resize(nQuestionMarkPos);
            const CPLStringList aosTokens(
                CSLTokenizeString2(osSuffix.c_str(), "&", 0));
            for (const char *pszToken : aosTokens)
            {
                if (EQUAL(pszToken, "ignore-timestamp-etag=yes"))
                {
                    ignoreTimestampEtag = true;
                }
                else if (STARTS_WITH(pszToken, "branch="))
                {
                    osBranchName = pszToken + strlen("branch=");
                }
                else if (STARTS_WITH(pszToken, "tag="))
                {
                    osTagName = pszToken + strlen("tag=");
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid Icechunk connection string");
                    return {};
                }
            }
        }
    }
    return osFilename;
}

/************************************************************************/
/*                           DecompressFile()                           */
/************************************************************************/

/** Read and decompress (if needed) the specified file.
 *
 * @param pszFilename Filename. Must NOT be null.
 * @param poFile Already opened file handle. Must NOT be null.
 * @param nExpectedFileType Expected file type.
 * @param[out] pnVersion File specification version. May be null
 *
 * @return tuple (pointer to content, size) or (nullptr, 0) in case of error.
 */
std::pair<std::unique_ptr<unsigned char, VSIFreeReleaser>, size_t>
DecompressFile(const char *pszFilename, VSIVirtualHandle *poFile,
               int nExpectedFileType, int *pnVersion)
{
    std::pair<std::unique_ptr<unsigned char, VSIFreeReleaser>, size_t> ret{
        nullptr, 0};

    const CPLCompressor *psZSTDDecompressor = CPLGetDecompressor("zstd");
    CPLAssert(psZSTDDecompressor);

    poFile->Seek(0, SEEK_END);
    const vsi_l_offset nSize64 = poFile->Tell();
    if (nSize64 < HEADER_SIZE)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "%s: too small file",
                 pszFilename);
        return ret;
    }
    if (nSize64 > std::numeric_limits<size_t>::max() / 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "%s: too large file",
                 pszFilename);
        return ret;
    }

    const size_t nSize = static_cast<size_t>(nSize64);
    ret.first.reset(static_cast<unsigned char *>(VSI_MALLOC_VERBOSE(nSize)));
    if (!ret.first)
        return ret;

    auto *pabyRaw = ret.first.get();
    if (poFile->Seek(0, SEEK_SET) != 0 || poFile->Read(pabyRaw, nSize) != nSize)
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s: cannot ingest file",
                 pszFilename);
        return ret;
    }

    if constexpr (IS_DEBUG_BUILD)
    {
        if (nExpectedFileType == FILE_TYPE_REPO_INFO)
        {
            std::string osImplementationName;
            osImplementationName.assign(
                reinterpret_cast<const char *>(pabyRaw + SIG_SIZE),
                IMPLEMENTATION_NAME_SIZE);
            osImplementationName.resize(strlen(osImplementationName.c_str()));
            CPLDebugOnly("Icechunk", "Implementation name = '%s'",
                         osImplementationName.c_str());
        }
    }

    if (memcmp(pabyRaw, abySIG, SIG_SIZE) != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s: Icechunk signature not found", pszFilename);
        return ret;
    }

    const int nVersion = pabyRaw[SIG_SIZE + IMPLEMENTATION_NAME_SIZE];
    if (nVersion != 1 && nVersion != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s: Icechunk version %d not supported", pszFilename,
                 nVersion);
        return ret;
    }
    if (pnVersion)
        *pnVersion = nVersion;

    const int nFileType =
        pabyRaw[SIG_SIZE + IMPLEMENTATION_NAME_SIZE + SPEC_VERSION_SIZE];
    if (nFileType != nExpectedFileType)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s: Got file type %d, expected %d", pszFilename, nFileType,
                 nExpectedFileType);
        return ret;
    }

    const int nCompressionAlgo = pabyRaw[SIG_SIZE + IMPLEMENTATION_NAME_SIZE +
                                         SPEC_VERSION_SIZE + FILE_TYPE_SIZE];
    if (nCompressionAlgo != COMPRESSION_ALGO_NONE &&
        nCompressionAlgo != COMPRESSION_ALGO_ZSTD)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s: Icechunk compression algorithm %d not supported",
                 pszFilename, nCompressionAlgo);
        return ret;
    }

    const auto *pabyRawPastHeader = pabyRaw + HEADER_SIZE;
    const auto nSizePastHeader = nSize - HEADER_SIZE;

    if (nCompressionAlgo == COMPRESSION_ALGO_ZSTD)
    {
        size_t nUncompressedSize = 0;
        void *pabyUncompressed = nullptr;
        if (!psZSTDDecompressor->pfnFunc(
                pabyRawPastHeader, nSizePastHeader, &pabyUncompressed,
                &nUncompressedSize, nullptr, psZSTDDecompressor->user_data))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: ZSTD decompression failed", pszFilename);
            ret.first.reset();
            return ret;
        }

        ret.first.reset(static_cast<unsigned char *>(pabyUncompressed));
        ret.second = nUncompressedSize;
    }
    else
    {
        memmove(pabyRaw, pabyRawPastHeader, nSizePastHeader);
        ret.second = nSizePastHeader;
    }

    return ret;
}

/************************************************************************/
/*                       CrockfordBase32Encode()                        */
/************************************************************************/

/** Encode the provided binary buffer as a Crockford Base32 string.
 *
 * Cf https://www.crockford.com/base32.html
 */
std::string CrockfordBase32Encode(const uint8_t *data, size_t size)
{
    std::string ret;
    // Omit I, L, O and U
    constexpr char szDict[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    static_assert(sizeof(szDict) - 1 == 32);
    size_t i = 0;
    unsigned currentVal = 0;
    unsigned currentBitsCount = 0;
    constexpr unsigned SYMBOL_BITS = 5;
    while (true)
    {
        if (currentBitsCount < SYMBOL_BITS)
        {
            // Extra iteration when i == size is intentional
            if (i > size)
                break;
            currentVal = (currentVal << 8) | (i < size ? data[i] : 0);
            ++i;
            currentBitsCount += 8;
        }
        const unsigned int rightShift = currentBitsCount - SYMBOL_BITS;
        const unsigned dictIdx = currentVal >> rightShift;
        CPLAssert(dictIdx < 32);
        ret += szDict[dictIdx];
        // Zero out the 5 left-most valid bits (that we just consumed)
        currentVal &= ~(31U << rightShift);
        currentBitsCount -= SYMBOL_BITS;
    }
    return ret;
}

}  // namespace gdal::icechunk
