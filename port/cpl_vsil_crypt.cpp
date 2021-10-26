/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for encrypted files.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#ifdef DEBUG_BOOL
#define DO_NOT_USE_DEBUG_BOOL
#endif

#include "cpl_port.h"
#include "cpl_vsi_virtual.h"

#include <cstddef>
#include <algorithm>

#include "cpl_error.h"
#include "cpl_vsi.h"

CPL_C_START
void CPL_DLL VSIInstallCryptFileHandler();
void CPL_DLL VSISetCryptKey( const GByte* pabyKey, int nKeySize );
CPL_C_END

CPL_CVSID("$Id$")

constexpr char VSICRYPT_PREFIX[] = "/vsicrypt/";

#if defined(HAVE_CRYPTOPP) || defined(DOXYGEN_SKIP)

//! @cond Doxygen_Suppress

/* Increase Major in case of backward incompatible changes */
constexpr int VSICRYPT_CURRENT_MAJOR = 1;
constexpr int VSICRYPT_CURRENT_MINOR = 0;
constexpr char VSICRYPT_SIGNATURE[] = "VSICRYPT";  // Must be 8 chars.

constexpr char VSICRYPT_PREFIX_WITHOUT_SLASH[] = "/vsicrypt";

constexpr unsigned int VSICRYPT_READ = 0x1;
constexpr unsigned int VSICRYPT_WRITE = 0x2;

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4505 )
#endif

/* Begin of crypto++ headers */
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4189 )
#pragma warning( disable : 4512 )
#pragma warning( disable : 4244 )
#endif

#ifdef USE_ONLY_CRYPTODLL_ALG
#include "cryptopp/dll.h"
#else
#include "cryptopp/aes.h"
#include "cryptopp/blowfish.h"
#include "cryptopp/camellia.h"
#include "cryptopp/cast.h"
#include "cryptopp/des.h"
#include "cryptopp/mars.h"
#include "cryptopp/idea.h"
#include "cryptopp/rc5.h"
#include "cryptopp/rc6.h"
#include "cryptopp/serpent.h"
#include "cryptopp/shacal2.h"
#include "cryptopp/skipjack.h"
#include "cryptopp/tea.h"
#include "cryptopp/twofish.h"
#endif

#include "cryptopp/filters.h"
#include "cryptopp/modes.h"
#include "cryptopp/osrng.h"

#ifdef _MSC_VER
#pragma warning( pop )
#endif

// Fix compatibility with Crypto++
#if CRYPTOPP_VERSION >= 600
typedef CryptoPP::byte cryptopp_byte;
#else
typedef byte cryptopp_byte;
#endif

/* End of crypto++ headers */

// I don't really understand why this is necessary, especially
// when cryptopp.dll and GDAL have been compiled with the same
// VC version and /MD. But otherwise you'll get crashes
// Borrowed from dlltest.cpp of crypto++
#if defined(WIN32) && defined(USE_ONLY_CRYPTODLL_ALG)

static CryptoPP::PNew s_pNew = nullptr;
static CryptoPP::PDelete s_pDelete = nullptr;

extern "C" __declspec(dllexport)
void __cdecl SetNewAndDeleteFromCryptoPP(
    CryptoPP::PNew pNew,
    CryptoPP::PDelete pDelete,
    CryptoPP::PSetNewHandler pSetNewHandler )
{
    s_pNew = pNew;
    s_pDelete = pDelete;
}

void * __cdecl operator new( vsize_t size )
{
    return s_pNew(size);
}

void __cdecl operator delete( void * p )
{
    s_pDelete(p);
}

#endif  // defined(WIN32) && defined(USE_ONLY_CRYPTODLL_ALG)

static GByte* pabyGlobalKey = nullptr;
static int nGlobalKeySize = 0;

typedef enum
{
    ALG_AES,
    ALG_Blowfish,
    ALG_Camellia,
    // ALG_CAST128, (obsolete)
    ALG_CAST256,
    // ALG_DES, (obsolete)
    ALG_DES_EDE2,
    ALG_DES_EDE3,
    // ALG_DES_XEX3, (obsolete)
    // ALG_Gost, (obsolete)
    ALG_MARS,
    ALG_IDEA,
    // ALG_RC2, (obsolete)
    ALG_RC5,
    ALG_RC6,
    // ALG_SAFER_K, (obsolete)
    // ALG_SAFER_SK, (obsolete)
    ALG_Serpent,
    ALG_SHACAL2,
    // ALG_SHARK, (obsolete)
    ALG_SKIPJACK,
    ALG_Twofish,
    // ALG_ThreeWay, (obsolete)
    ALG_XTEA,
    ALG_MAX = ALG_XTEA
} VSICryptAlg;

typedef enum
{
    MODE_CBC,
    MODE_CFB,
    MODE_OFB,
    MODE_CTR,
    MODE_CBC_CTS,
    MODE_MAX = MODE_CBC_CTS
} VSICryptMode;

//! @endcond

/************************************************************************/
/*                          VSISetCryptKey()                            */
/************************************************************************/

/** Installs the encryption/decryption key.
 *
 * By passing a NULL key, the previously installed key will be cleared.
 * Note, however, that it is not guaranteed that there won't be trace of it
 * in other places in memory or in on-disk temporary file.
 *
 * @param pabyKey key. Might be NULL to clear previously set key.
 * @param nKeySize length of the key in bytes. Might be 0 to clear
 * previously set key.
 *
 * @see VSIInstallCryptFileHandler() for documentation on /vsicrypt/
 */
void VSISetCryptKey( const GByte* pabyKey, int nKeySize )
{
    CPLAssert( (pabyKey != nullptr && nKeySize != 0) ||
               (pabyKey == nullptr && nKeySize == 0) );
    if( pabyGlobalKey )
    {
        // Make some effort to clear the memory, although it could have leaked
        // elsewhere...
        memset( pabyGlobalKey, 0, nGlobalKeySize );
        CPLFree(pabyGlobalKey);
        pabyGlobalKey = nullptr;
        nGlobalKeySize = 0;
    }
    if( pabyKey )
    {
        pabyGlobalKey = static_cast<GByte *>(CPLMalloc(nKeySize));
        memcpy(pabyGlobalKey, pabyKey, nKeySize);
        nGlobalKeySize = nKeySize;
    }
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                             GetAlg()                                 */
/************************************************************************/

#undef CASE_ALG
#define CASE_ALG(alg)   if( EQUAL(pszName, #alg) ) return ALG_##alg;

static VSICryptAlg GetAlg( const char* pszName )
{
    CASE_ALG(AES)
    CASE_ALG(Blowfish)
    CASE_ALG(Camellia)
    // CASE_ALG(CAST128) (obsolete)
    CASE_ALG(CAST256)
    // CASE_ALG(DES) (obsolete)
    CASE_ALG(DES_EDE2)
    CASE_ALG(DES_EDE3)
    // CASE_ALG(DES_XEX3) (obsolete)
    // CASE_ALG(Ghost) (obsolete)
    CASE_ALG(MARS)
    CASE_ALG(IDEA)
    // CASE_ALG(RC2) (obsolete)
    CASE_ALG(RC5)
    CASE_ALG(RC6)
    // CASE_ALG(SAFER_K) (obsolete)
    // CASE_ALG(SAFER_SK) (obsolete)
    CASE_ALG(Serpent)
    CASE_ALG(SHACAL2)
    // CASE_ALG(SHARK) (obsolete)
    CASE_ALG(SKIPJACK)
    // CASE_ALG(ThreeWay) (obsolete)
    CASE_ALG(Twofish)
    CASE_ALG(XTEA)

    CPLError(CE_Warning, CPLE_NotSupported,
             "Unsupported cipher algorithm: %s. Using AES instead", pszName);
    return ALG_AES;
}

/************************************************************************/
/*                          GetEncBlockCipher()                         */
/************************************************************************/

#undef CASE_ALG
#define CASE_ALG(alg)   case ALG_##alg: return new CryptoPP::alg::Encryption();

static CryptoPP::BlockCipher* GetEncBlockCipher( VSICryptAlg eAlg )
{
    switch( eAlg )
    {
        CASE_ALG(AES)
#ifndef USE_ONLY_CRYPTODLL_ALG
        CASE_ALG(Blowfish)
        CASE_ALG(Camellia)
        // CASE_ALG(CAST128) (obsolete)
        CASE_ALG(CAST256)
#endif
        // CASE_ALG(DES) (obsolete)
        CASE_ALG(DES_EDE2)
        CASE_ALG(DES_EDE3)
        // CASE_ALG(DES_XEX3) (obsolete)
#ifndef USE_ONLY_CRYPTODLL_ALG
        // CASE_ALG(Gost) (obsolete)
        CASE_ALG(MARS)
        CASE_ALG(IDEA)
        // CASE_ALG(RC2) (obsolete)
        CASE_ALG(RC5)
        CASE_ALG(RC6)
        // CASE_ALG(SAFER_K) (obsolete)
        // CASE_ALG(SAFER_SK) (obsolete)
        CASE_ALG(Serpent)
        CASE_ALG(SHACAL2)
        // CASE_ALG(SHARK) (obsolete)
#endif
        CASE_ALG(SKIPJACK)
#ifndef USE_ONLY_CRYPTODLL_ALG
        // CASE_ALG(ThreeWay) (obsolete)
        CASE_ALG(Twofish)
        CASE_ALG(XTEA)
#endif
        default: return nullptr;
    }
}

/************************************************************************/
/*                          GetDecBlockCipher()                         */
/************************************************************************/

#undef CASE_ALG
#define CASE_ALG(alg)   case ALG_##alg: return new CryptoPP::alg::Decryption();

static CryptoPP::BlockCipher* GetDecBlockCipher( VSICryptAlg eAlg )
{
    switch( eAlg )
    {
        CASE_ALG(AES)
#ifndef USE_ONLY_CRYPTODLL_ALG
        CASE_ALG(Blowfish)
        CASE_ALG(Camellia)
        // CASE_ALG(CAST128) (obsolete)
        CASE_ALG(CAST256)
#endif
        // CASE_ALG(DES) (obsolete)
        CASE_ALG(DES_EDE2)
        CASE_ALG(DES_EDE3)
        // CASE_ALG(DES_XEX3) (obsolete)
#ifndef USE_ONLY_CRYPTODLL_ALG
        // CASE_ALG(Gost) (obsolete)
        CASE_ALG(MARS)
        CASE_ALG(IDEA)
        // CASE_ALG(RC2) (obsolete)
        CASE_ALG(RC5)
        CASE_ALG(RC6)
        // CASE_ALG(SAFER_K) (obsolete)
        // CASE_ALG(SAFER_SK) (obsolete)
        CASE_ALG(Serpent)
        CASE_ALG(SHACAL2)
        // CASE_ALG(SHARK) (obsolete)
#endif
        CASE_ALG(SKIPJACK)
#ifndef USE_ONLY_CRYPTODLL_ALG
        // CASE_ALG(ThreeWay) (obsolete)
        CASE_ALG(Twofish)
        CASE_ALG(XTEA)
#endif
        default: return nullptr;
    }
}

/************************************************************************/
/*                             GetMode()                                */
/************************************************************************/

static VSICryptMode GetMode( const char* pszName )
{
    if( EQUAL(pszName, "CBC") )
        return MODE_CBC;
    if( EQUAL(pszName, "CFB") )
        return MODE_CFB;
    if( EQUAL(pszName, "OFB") )
        return MODE_OFB;
    if( EQUAL(pszName, "CTR") )
        return MODE_CTR;
    if( EQUAL(pszName, "CBC_CTS") )
        return MODE_CBC_CTS;

    CPLError(CE_Warning, CPLE_NotSupported,
             "Unsupported cipher block mode: %s. Using CBC instead", pszName);
    return MODE_CBC;
}

/************************************************************************/
/*                          VSICryptFileHeader                          */
/************************************************************************/

class VSICryptFileHeader
{
        CPL_DISALLOW_COPY_ASSIGN(VSICryptFileHeader)

        std::string CryptKeyCheck( CryptoPP::BlockCipher* poEncCipher );

    public:
        VSICryptFileHeader() = default;

        int ReadFromFile( VSIVirtualHandle* fp, const CPLString& osKey );
        int WriteToFile( VSIVirtualHandle* fp,
                         CryptoPP::BlockCipher* poEncCipher );

        GUInt16 nHeaderSize = 0;
        GByte nMajorVersion = 0;
        GByte nMinorVersion = 0;
        GUInt16 nSectorSize = 512;
        VSICryptAlg eAlg = ALG_AES;
        VSICryptMode eMode = MODE_CBC;
        CPLString osIV{};
        bool bAddKeyCheck = false;
        GUIntBig nPayloadFileSize = 0;
        CPLString osFreeText{};
        CPLString osExtraContent{};
};

/************************************************************************/
/*                         VSICryptReadError()                          */
/************************************************************************/

static bool VSICryptReadError()
{
    CPLError(CE_Failure, CPLE_FileIO, "Cannot read header");
    return false;
}

/************************************************************************/
/*                       VSICryptGenerateSectorIV()                     */
/************************************************************************/

// TODO(rouault): This function really needs a comment saying what it does.
static std::string VSICryptGenerateSectorIV( const std::string& osIV,
                                             vsi_l_offset nOffset )
{
    std::string osSectorIV(osIV);
    const size_t nLength = std::min(sizeof(vsi_l_offset), osSectorIV.size());
    for( size_t i = 0; i < nLength; i++ )
    {
        // TODO(rouault): Explain what this block is trying to do?
        osSectorIV[i] = static_cast<char>((osSectorIV[i] ^ nOffset) & 0xff);
        nOffset >>= 8;
    }
    return osSectorIV;
}

/************************************************************************/
/*                          CryptKeyCheck()                             */
/************************************************************************/

std::string
VSICryptFileHeader::CryptKeyCheck( CryptoPP::BlockCipher* poEncCipher )
{
    std::string osKeyCheckRes;

    CPLAssert( osIV.size() == poEncCipher->BlockSize() );
    // Generate a unique IV with a sector offset of 0xFFFFFFFFFFFFFFFF.
    std::string osCheckIV(VSICryptGenerateSectorIV(osIV, ~(static_cast<vsi_l_offset>(0))));
    CryptoPP::StreamTransformation* poMode;
    try
    {
        poMode = new CryptoPP::CBC_Mode_ExternalCipher::Encryption(
            *poEncCipher, reinterpret_cast<const cryptopp_byte*>(osCheckIV.c_str()));
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CryptoPP exception: %s", e.what());
        return std::string();
    }
    CryptoPP::StringSink* poSink = new CryptoPP::StringSink(osKeyCheckRes);
    CryptoPP::StreamTransformationFilter* poEnc =
        new CryptoPP::StreamTransformationFilter(
            *poMode, poSink,
            CryptoPP::StreamTransformationFilter::NO_PADDING);
    // Not sure if it is add extra security, but pick up something that is
    // unlikely to be a plain text (random number).
    poEnc->Put(
        reinterpret_cast<const cryptopp_byte*>(
            "\xDB\x31\xB9\x1B\xD3\x1C\xFA\x3E\x84\x06\xC1\x42\xC3\xEC\xCD\x9A"
            "\x02\x36\x22\x15\x58\x88\x74\x65\x00\x2F\x98\xBC\x69\x22\xE1\x63"),
        std::min(32U, poEncCipher->BlockSize()));
    poEnc->MessageEnd();
    delete poEnc;
    delete poMode;

    return osKeyCheckRes;
}

/************************************************************************/
/*                          ReadFromFile()                              */
/************************************************************************/

int VSICryptFileHeader::ReadFromFile( VSIVirtualHandle* fp,
                                      const CPLString& osKey )
{
    GByte abySignature[8] = {};
    fp->Seek(0, SEEK_SET);
    CPL_STATIC_ASSERT(sizeof(VSICRYPT_SIGNATURE) == 8+1);
    if( fp->Read(abySignature, 8, 1) == 0 ||
        memcmp(abySignature, VSICRYPT_SIGNATURE, 8) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid signature");
        return FALSE;
    }

    if( fp->Read(&nHeaderSize, 2, 1) == 0 )
        return VSICryptReadError();
    nHeaderSize = CPL_LSBWORD16(nHeaderSize);
    if( nHeaderSize < 8 + 2 + 1 + 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid header size : %d", nHeaderSize);
        return FALSE;
    }

    if( fp->Read(&nMajorVersion, 1, 1) == 0 )
        return VSICryptReadError();
    if( fp->Read(&nMinorVersion, 1, 1) == 0 )
        return VSICryptReadError();

    if( nMajorVersion != VSICRYPT_CURRENT_MAJOR )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled major version : %d", nMajorVersion);
        return FALSE;
    }
    if( nMinorVersion != VSICRYPT_CURRENT_MINOR )
    {
        CPLDebug("VSICRYPT", "Minor version in file is %d", nMinorVersion);
    }

    if( fp->Read(&nSectorSize, 2, 1) == 0 )
        return VSICryptReadError();
    nSectorSize = CPL_LSBWORD16(nSectorSize);

    GByte nAlg, nMode;
    if( fp->Read(&nAlg, 1, 1) == 0 ||
        fp->Read(&nMode, 1, 1) == 0 )
        return VSICryptReadError();
    if( nAlg > ALG_MAX )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported cipher algorithm %d",
                 nAlg);
        return FALSE;
    }
    if( nMode > MODE_MAX )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported cipher block mode %d",
                 nMode);
        return FALSE;
    }
    eAlg = static_cast<VSICryptAlg>(nAlg);
    eMode = static_cast<VSICryptMode>(nMode);

    GByte nIVSize;
    if( fp->Read(&nIVSize, 1, 1) == 0 )
        return VSICryptReadError();

    osIV.resize(nIVSize);
    // TODO(schwehr): Using the const buffer of a string is a bad idea.
    if( fp->Read(reinterpret_cast<void*>(const_cast<char*>(osIV.c_str())), 1, nIVSize) != nIVSize )
        return VSICryptReadError();

    GUInt16 nFreeTextSize;
    if( fp->Read(&nFreeTextSize, 2, 1) == 0 )
        return VSICryptReadError();

    osFreeText.resize(nFreeTextSize);
    if( fp->Read(reinterpret_cast<void*>(const_cast<char*>(osFreeText.c_str())), 1, nFreeTextSize) != nFreeTextSize )
        return VSICryptReadError();

    GByte nKeyCheckSize;
    if( fp->Read(&nKeyCheckSize, 1, 1) == 0 )
        return VSICryptReadError();
    bAddKeyCheck = nKeyCheckSize != 0;
    if( nKeyCheckSize )
    {
        CPLString osKeyCheck;
        osKeyCheck.resize(nKeyCheckSize);
        if( fp->Read(reinterpret_cast<void*>(const_cast<char*>(osKeyCheck.c_str())), 1,
                     nKeyCheckSize) != nKeyCheckSize )
            return VSICryptReadError();

        if( osKey.empty() && pabyGlobalKey == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Encryption key not defined as key/key_b64 parameter, "
                     "VSICRYPT_KEY/VSICRYPT_KEY_B64 configuration option or "
                     "VSISetCryptKey() API");
            return FALSE;
        }

        CryptoPP::BlockCipher* poEncCipher = GetEncBlockCipher(eAlg);
        if( poEncCipher == nullptr )
            return FALSE;

        if( osIV.size() != poEncCipher->BlockSize() )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Inconsistent initial vector" );
            delete poEncCipher;
            return FALSE;
        }

        int nMaxKeySize = static_cast<int>(poEncCipher->MaxKeyLength());

        try
        {
            if( !osKey.empty() )
            {
                const int nKeySize =
                    std::min(nMaxKeySize, static_cast<int>(osKey.size()));
                poEncCipher->SetKey(reinterpret_cast<const cryptopp_byte*>(osKey.c_str()), nKeySize);
            }
            else if( pabyGlobalKey )
            {
                const int nKeySize = std::min(nMaxKeySize, nGlobalKeySize);
                poEncCipher->SetKey(pabyGlobalKey, nKeySize);
            }
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "CryptoPP exception: %s", e.what());
            delete poEncCipher;
            return FALSE;
        }

        std::string osKeyCheckRes = CryptKeyCheck(poEncCipher);

        delete poEncCipher;

        if( osKeyCheck.size() != osKeyCheckRes.size() ||
            memcmp(osKeyCheck.c_str(), osKeyCheckRes.c_str(), osKeyCheck.size())
            != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Bad key");
            return FALSE;
        }
    }

    if( fp->Read(&nPayloadFileSize, 8, 1) == 0 )
        return VSICryptReadError();
    CPL_LSBPTR64(&nPayloadFileSize);
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "nPayloadFileSize read = " CPL_FRMT_GUIB,
             nPayloadFileSize);
#endif

    GUInt16 nExtraContentSize = 0;
    if( fp->Read(&nExtraContentSize, 2, 1) == 0 )
        return VSICryptReadError();
    nExtraContentSize = CPL_LSBWORD16(nExtraContentSize);

    osExtraContent.resize(nExtraContentSize);
    if( fp->Read(const_cast<char*>(osExtraContent.c_str()), 1, nExtraContentSize)
        != nExtraContentSize )
        return VSICryptReadError();

    return TRUE;
}

/************************************************************************/
/*                          WriteToFile()                               */
/************************************************************************/

int VSICryptFileHeader::WriteToFile( VSIVirtualHandle* fp,
                                     CryptoPP::BlockCipher* poEncCipher )
{
    fp->Seek(0, SEEK_SET);

    bool bRet = fp->Write(VSICRYPT_SIGNATURE, 8, 1) == 1;

    std::string osKeyCheckRes;
    if( bAddKeyCheck )
    {
        osKeyCheckRes = CryptKeyCheck(poEncCipher);
    }

    GUInt16 nHeaderSizeNew = static_cast<GUInt16>(8 + /* signature */
                            2 + /* header size */
                            1 + /* major version */
                            1 + /* minor version */
                            2 + /* sector size */
                            1 + /* alg */
                            1 + /* mode */
                            1 + osIV.size() + /* IV */
                            2 + osFreeText.size() + /* free text */
                            1 + osKeyCheckRes.size() + /* key check */
                            8 + /* payload size */
                            2 + osExtraContent.size()); /* extra content */
    if( nHeaderSize != 0 )
        CPLAssert( nHeaderSizeNew == nHeaderSize );
    else
        nHeaderSize = nHeaderSizeNew;

    GUInt16 nHeaderSizeToWrite = CPL_LSBWORD16(nHeaderSizeNew);
    bRet &= (fp->Write(&nHeaderSizeToWrite, 2, 1) == 1);

    GByte nMajorVersionToWrite = VSICRYPT_CURRENT_MAJOR;
    bRet &= (fp->Write(&nMajorVersionToWrite, 1, 1) == 1);

    GByte nMinorVersionToWrite = VSICRYPT_CURRENT_MINOR;
    bRet &= (fp->Write(&nMinorVersionToWrite, 1, 1) == 1);

    GUInt16 nSectorSizeToWrite = CPL_LSBWORD16(nSectorSize);
    bRet &= (fp->Write(&nSectorSizeToWrite, 2, 1) == 1);

    GByte nAlg = static_cast<GByte>(eAlg);
    bRet &= (fp->Write(&nAlg, 1, 1) == 1);

    GByte nMode = static_cast<GByte>(eMode);
    bRet &= (fp->Write(&nMode, 1, 1) == 1);

    GByte nIVSizeToWrite = static_cast<GByte>(osIV.size());
    CPLAssert(nIVSizeToWrite == osIV.size());
    bRet &= (fp->Write(&nIVSizeToWrite, 1, 1) == 1);
    bRet &= (fp->Write(osIV.c_str(), 1, osIV.size()) == osIV.size());

    GUInt16 nFreeTextSizeToWrite = CPL_LSBWORD16(static_cast<GUInt16>(osFreeText.size()));
    bRet &= (fp->Write(&nFreeTextSizeToWrite, 2, 1) == 1);
    bRet &= (fp->Write(osFreeText.c_str(), 1,
                       osFreeText.size()) == osFreeText.size());

    GByte nSize = static_cast<GByte>(osKeyCheckRes.size());
    bRet &= (fp->Write(&nSize, 1, 1) == 1);
    bRet &= (fp->Write(osKeyCheckRes.c_str(), 1,
                       osKeyCheckRes.size()) == osKeyCheckRes.size());

    GUIntBig nPayloadFileSizeToWrite = nPayloadFileSize;
    CPL_LSBPTR64(&nPayloadFileSizeToWrite);
    bRet &= (fp->Write(&nPayloadFileSizeToWrite, 8, 1) == 1);

    GUInt16 nExtraContentSizeToWrite =
        CPL_LSBWORD16(static_cast<GUInt16>(osExtraContent.size()));
    bRet &= (fp->Write(&nExtraContentSizeToWrite, 2, 1) == 1);
    bRet &= (fp->Write(osExtraContent.c_str(), 1, osExtraContent.size()) ==
             osExtraContent.size());

    CPLAssert(fp->Tell() == nHeaderSize);

    return bRet;
}

/************************************************************************/
/*                          VSICryptFileHandle                          */
/************************************************************************/

class VSICryptFileHandle final : public VSIVirtualHandle
{
        CPL_DISALLOW_COPY_ASSIGN(VSICryptFileHandle)

  private:
        CPLString           osBaseFilename{};
        int                 nPerms = 0;
        VSIVirtualHandle   *poBaseHandle = nullptr;
        VSICryptFileHeader *poHeader = nullptr;
        bool                bUpdateHeader = false;
        vsi_l_offset        nCurPos = 0;
        bool                bEOF = false;

        CryptoPP::BlockCipher* poEncCipher = nullptr;
        CryptoPP::BlockCipher* poDecCipher = nullptr;
        int                 nBlockSize = 0;

        vsi_l_offset        nWBOffset = 0;
        GByte*              pabyWB = nullptr;
        int                 nWBSize = 0;
        bool                bWBDirty = false;

        bool                bLastSectorWasModified = false;

        void             EncryptBlock( GByte* pabyData, vsi_l_offset nOffset );
        bool             DecryptBlock( GByte* pabyData, vsi_l_offset nOffset );
        bool             FlushDirty();

  public:
    VSICryptFileHandle( const CPLString& osBaseFilename,
                        VSIVirtualHandle* poBaseHandle,
                        VSICryptFileHeader* poHeader,
                        int nPerms );
    ~VSICryptFileHandle() override;

    int                  Init( const CPLString& osKey,
                               bool bWriteHeader = false );

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
    int Truncate( vsi_l_offset nNewSize ) override;
};

/************************************************************************/
/*                          VSICryptFileHandle()                        */
/************************************************************************/

VSICryptFileHandle::VSICryptFileHandle( const CPLString& osBaseFilenameIn,
                                        VSIVirtualHandle* poBaseHandleIn,
                                        VSICryptFileHeader* poHeaderIn,
                                        int nPermsIn ) :
    osBaseFilename(osBaseFilenameIn),
    nPerms(nPermsIn),
    poBaseHandle(poBaseHandleIn),
    poHeader(poHeaderIn)
{}

/************************************************************************/
/*                         ~VSICryptFileHandle()                        */
/************************************************************************/

VSICryptFileHandle::~VSICryptFileHandle()
{
    Close();
    delete poHeader;
    delete poEncCipher;
    delete poDecCipher;
    CPLFree(pabyWB);
}

/************************************************************************/
/*                               Init()                                 */
/************************************************************************/

int VSICryptFileHandle::Init( const CPLString& osKey, bool bWriteHeader )
{
    poEncCipher = GetEncBlockCipher(poHeader->eAlg);
    if( poEncCipher == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cipher algorithm not supported in this build: %d",
                 static_cast<int>(poHeader->eAlg));
        return FALSE;
    }

    if( poHeader->osIV.size() != poEncCipher->BlockSize() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Inconsistent initial vector" );
        return FALSE;
    }

    poDecCipher = GetDecBlockCipher(poHeader->eAlg);
    nBlockSize = poEncCipher->BlockSize();
    int nMaxKeySize = static_cast<int>(poEncCipher->MaxKeyLength());

    try
    {
        if( !osKey.empty() )
        {
            const int nKeySize =
                std::min(nMaxKeySize, static_cast<int>(osKey.size()));
            poEncCipher->SetKey(reinterpret_cast<const cryptopp_byte*>(osKey.c_str()), nKeySize);
            poDecCipher->SetKey(reinterpret_cast<const cryptopp_byte*>(osKey.c_str()), nKeySize);
        }
        else if( pabyGlobalKey )
        {
            const int nKeySize = std::min(nMaxKeySize, nGlobalKeySize);
            poEncCipher->SetKey(pabyGlobalKey, nKeySize);
            poDecCipher->SetKey(pabyGlobalKey, nKeySize);
        }
        else
            return FALSE;
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "CryptoPP exception: %s", e.what());
        return FALSE;
    }

    pabyWB = static_cast<GByte *>(CPLCalloc(1, poHeader->nSectorSize));

    if( (poHeader->nSectorSize % nBlockSize) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Sector size (%d) is not a multiple of block size (%d)",
                 poHeader->nSectorSize, nBlockSize);
        return FALSE;
    }
    if( poHeader->eMode == MODE_CBC_CTS &&
        poHeader->nSectorSize < 2 * nBlockSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Sector size (%d) should be at least twice larger than "
                 "the block size (%d) in CBC_CTS.",
                 poHeader->nSectorSize, nBlockSize);
        return FALSE;
    }

    if( bWriteHeader && !poHeader->WriteToFile(poBaseHandle, poEncCipher) )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          EncryptBlock()                              */
/************************************************************************/

void VSICryptFileHandle::EncryptBlock( GByte* pabyData, vsi_l_offset nOffset )
{
    std::string osRes;
    std::string osIV(VSICryptGenerateSectorIV(poHeader->osIV, nOffset));
    CPLAssert( static_cast<int>(osIV.size()) == nBlockSize );

    CryptoPP::StreamTransformation* poMode;
    try
    {
        if( poHeader->eMode == MODE_CBC )
            poMode = new CryptoPP::CBC_Mode_ExternalCipher::Encryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte *>(osIV.c_str()) );
        else if( poHeader->eMode == MODE_CFB )
            poMode = new CryptoPP::CFB_Mode_ExternalCipher::Encryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte *>(osIV.c_str()) );
        else if( poHeader->eMode == MODE_OFB )
            poMode = new CryptoPP::OFB_Mode_ExternalCipher::Encryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte *>(osIV.c_str()) );
        else if( poHeader->eMode == MODE_CTR )
            poMode = new CryptoPP::CTR_Mode_ExternalCipher::Encryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte *>(osIV.c_str()) );
        else
            poMode = new CryptoPP::CBC_CTS_Mode_ExternalCipher::Encryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte *>(osIV.c_str()) );
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "cryptopp exception: %s", e.what());
        return;
    }
    CryptoPP::StringSink* poSink = new CryptoPP::StringSink(osRes);
    CryptoPP::StreamTransformationFilter* poEnc =
        new CryptoPP::StreamTransformationFilter(
            *poMode, poSink, CryptoPP::StreamTransformationFilter::NO_PADDING);
    poEnc->Put(pabyData, poHeader->nSectorSize);
    poEnc->MessageEnd();
    delete poEnc;

    delete poMode;

    CPLAssert( static_cast<int>(osRes.length()) == poHeader->nSectorSize );
    memcpy( pabyData, osRes.c_str(), osRes.length() );
}

/************************************************************************/
/*                          DecryptBlock()                              */
/************************************************************************/

bool VSICryptFileHandle::DecryptBlock( GByte* pabyData, vsi_l_offset nOffset )
{
    std::string osRes;
    std::string osIV(VSICryptGenerateSectorIV(poHeader->osIV, nOffset));
    CPLAssert( static_cast<int>(osIV.size()) == nBlockSize );
    CryptoPP::StringSink* poSink = new CryptoPP::StringSink(osRes);
    CryptoPP::StreamTransformation* poMode = nullptr;
    CryptoPP::StreamTransformationFilter* poDec = nullptr;

    try
    {
        // Yes, some modes need the encryption cipher.
        if( poHeader->eMode == MODE_CBC )
            poMode = new CryptoPP::CBC_Mode_ExternalCipher::Decryption(
                *poDecCipher, reinterpret_cast<const cryptopp_byte*>(osIV.c_str()) );
        else if( poHeader->eMode == MODE_CFB )
            poMode = new CryptoPP::CFB_Mode_ExternalCipher::Decryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte*>(osIV.c_str()) );
        else if( poHeader->eMode == MODE_OFB )
            poMode = new CryptoPP::OFB_Mode_ExternalCipher::Decryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte*>(osIV.c_str()) );
        else if( poHeader->eMode == MODE_CTR )
            poMode = new CryptoPP::CTR_Mode_ExternalCipher::Decryption(
                *poEncCipher, reinterpret_cast<const cryptopp_byte*>(osIV.c_str()) );
        else
            poMode = new CryptoPP::CBC_CTS_Mode_ExternalCipher::Decryption(
                *poDecCipher, reinterpret_cast<const cryptopp_byte*>(osIV.c_str()) );
        poDec = new CryptoPP::StreamTransformationFilter(
            *poMode, poSink, CryptoPP::StreamTransformationFilter::NO_PADDING);
        poDec->Put(reinterpret_cast<const cryptopp_byte*>(pabyData), poHeader->nSectorSize);
        poDec->MessageEnd();
        delete poDec;
        delete poMode;
    }
    catch( const std::exception& e )
    {
        delete poDec;
        delete poMode;

        CPLError(CE_Failure, CPLE_AppDefined,
                 "CryptoPP exception: %s", e.what());
        return false;
    }

    CPLAssert( static_cast<int>(osRes.length()) == poHeader->nSectorSize );
    memcpy( pabyData, osRes.c_str(), osRes.length() );

    return true;
}

/************************************************************************/
/*                             FlushDirty()                             */
/************************************************************************/

bool VSICryptFileHandle::FlushDirty()
{
    if( !bWBDirty )
        return true;
    bWBDirty = false;

    EncryptBlock(pabyWB, nWBOffset);
    poBaseHandle->Seek( poHeader->nHeaderSize + nWBOffset, SEEK_SET );

    nWBOffset = 0;
    nWBSize = 0;

    if( poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 ) != 1 )
        return false;

    return true;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICryptFileHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Seek(nOffset=" CPL_FRMT_GUIB ", nWhence=%d)",
             nOffset, nWhence);
#endif

    bEOF = false;

    if( nWhence == SEEK_SET )
        nCurPos = nOffset;
    else if( nWhence == SEEK_CUR )
        nCurPos += nOffset;
    else
        nCurPos = poHeader->nPayloadFileSize;
    return 0;
}

/************************************************************************/
/*                                  Tell()                              */
/************************************************************************/

vsi_l_offset VSICryptFileHandle::Tell()
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Tell()=" CPL_FRMT_GUIB, nCurPos);
#endif
    return nCurPos;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSICryptFileHandle::Read( void *pBuffer, size_t nSize, size_t nMemb )
{
    size_t nToRead = nSize * nMemb;
    GByte* pabyBuffer = static_cast<GByte *>(pBuffer);

#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Read(nCurPos=" CPL_FRMT_GUIB ", nToRead=%d)",
             nCurPos, static_cast<int>(nToRead));
#endif

    if( (nPerms & VSICRYPT_READ) == 0 )
        return 0;

    if( nCurPos >= poHeader->nPayloadFileSize )
    {
        bEOF = true;
        return 0;
    }

    if( !FlushDirty() )
        return 0;

    while( nToRead > 0 )
    {
        if( nCurPos >= nWBOffset && nCurPos < nWBOffset + nWBSize )
        {
            // TODO(schwehr): Can nToCopy be a size_t to simplify casting?
            int nToCopy = std::min(
                static_cast<int>(nToRead),
                static_cast<int>(nWBSize - (nCurPos - nWBOffset)));
            if( nCurPos + nToCopy > poHeader->nPayloadFileSize )
            {
                bEOF = true;
                nToCopy =
                    static_cast<int>(poHeader->nPayloadFileSize - nCurPos);
            }
            memcpy(pabyBuffer, pabyWB + nCurPos - nWBOffset, nToCopy);
            pabyBuffer += nToCopy;
            nToRead -= nToCopy;
            nCurPos += nToCopy;
            if( bEOF || nToRead == 0 )
                break;
            CPLAssert( (nCurPos % poHeader->nSectorSize) == 0 );
        }

        vsi_l_offset nSectorOffset =
            (nCurPos / poHeader->nSectorSize) * poHeader->nSectorSize;
        poBaseHandle->Seek( poHeader->nHeaderSize + nSectorOffset, SEEK_SET );
        if( poBaseHandle->Read( pabyWB, poHeader->nSectorSize, 1 ) != 1 )
        {
            bEOF = true;
            break;
        }
        if( !DecryptBlock( pabyWB, nSectorOffset) )
        {
            break;
        }
        if( (nPerms & VSICRYPT_WRITE) &&
            nSectorOffset + poHeader->nSectorSize > poHeader->nPayloadFileSize )
        {
            // If the last sector was padded with random values, decrypt it to 0
            // in case of update scenarios.
            CPLAssert( nSectorOffset < poHeader->nPayloadFileSize );
            memset( pabyWB + poHeader->nPayloadFileSize - nSectorOffset, 0,
                    nSectorOffset + poHeader->nSectorSize -
                    poHeader->nPayloadFileSize );
        }
        nWBOffset = nSectorOffset;
        nWBSize = poHeader->nSectorSize;
    }

    int nRet = static_cast<int>( (nSize * nMemb - nToRead) / nSize );
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Read ret = %d (nMemb = %d)",
             nRet, static_cast<int>(nMemb));
#endif
    return nRet;
}

/************************************************************************/
/*                                Write()                               */
/************************************************************************/

size_t
VSICryptFileHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    size_t nToWrite = nSize * nMemb;
    const GByte* pabyBuffer = static_cast<const GByte *>(pBuffer);

#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Write(nCurPos=" CPL_FRMT_GUIB ", nToWrite=%d,"
             "nPayloadFileSize=" CPL_FRMT_GUIB
             ",bWBDirty=%d,nWBOffset=" CPL_FRMT_GUIB ",nWBSize=%d)",
             nCurPos, static_cast<int>(nToWrite), poHeader->nPayloadFileSize,
             static_cast<int>(bWBDirty), nWBOffset, nWBSize);
#endif

    if( (nPerms & VSICRYPT_WRITE) == 0 )
        return 0;

    if( nCurPos >=
        (poHeader->nPayloadFileSize / poHeader->nSectorSize) *
        poHeader->nSectorSize )
    {
        bLastSectorWasModified = true;
    }

    // If seeking past end of file, we need to explicitly encrypt the
    // padding zeroes.
    if( nCurPos > poHeader->nPayloadFileSize && nCurPos > nWBOffset + nWBSize )
    {
        if( !FlushDirty() )
            return 0;
        vsi_l_offset nOffset =
            (poHeader->nPayloadFileSize + poHeader->nSectorSize - 1) /
            poHeader->nSectorSize * poHeader->nSectorSize;
        const vsi_l_offset nEndOffset =
            nCurPos / poHeader->nSectorSize * poHeader->nSectorSize;
        for( ; nOffset < nEndOffset; nOffset += poHeader->nSectorSize )
        {
            memset( pabyWB, 0, poHeader->nSectorSize );
            EncryptBlock( pabyWB, nOffset );
            poBaseHandle->Seek( poHeader->nHeaderSize + nOffset, SEEK_SET );
            if( poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 ) != 1 )
                return 0;
            poHeader->nPayloadFileSize = nOffset + poHeader->nSectorSize;
            bUpdateHeader = true;
        }
    }

    while( nToWrite > 0 )
    {
        if( nCurPos >= nWBOffset && nCurPos < nWBOffset + nWBSize )
        {
            bWBDirty = true;
            const int nToCopy =
                std::min(static_cast<int>(nToWrite),
                         static_cast<int>(nWBSize - (nCurPos - nWBOffset)));
            memcpy(pabyWB + nCurPos - nWBOffset, pabyBuffer, nToCopy);
            pabyBuffer += nToCopy;
            nToWrite -= nToCopy;
            nCurPos += nToCopy;
            if( nCurPos > poHeader->nPayloadFileSize )
            {
                bUpdateHeader = true;
                poHeader->nPayloadFileSize = nCurPos;
            }
            if( nToWrite == 0 )
                break;
            CPLAssert( (nCurPos % poHeader->nSectorSize) == 0 );
        }
        else if( (nCurPos % poHeader->nSectorSize) == 0 &&
                 nToWrite >= static_cast<size_t>(poHeader->nSectorSize) )
        {
            if( !FlushDirty() )
                break;

            bWBDirty = true;
            nWBOffset = nCurPos;
            nWBSize = poHeader->nSectorSize;
            memcpy( pabyWB, pabyBuffer, poHeader->nSectorSize );
            pabyBuffer += poHeader->nSectorSize;
            nToWrite -= poHeader->nSectorSize;
            nCurPos += poHeader->nSectorSize;
            if( nCurPos > poHeader->nPayloadFileSize )
            {
                bUpdateHeader = true;
                poHeader->nPayloadFileSize = nCurPos;
            }
        }
        else
        {
            if( !FlushDirty() )
                break;

            const vsi_l_offset nSectorOffset =
                (nCurPos / poHeader->nSectorSize) * poHeader->nSectorSize;
            const vsi_l_offset nLastSectorOffset =
                (poHeader->nPayloadFileSize / poHeader->nSectorSize) *
                poHeader->nSectorSize;
            if( nSectorOffset > nLastSectorOffset &&
                (poHeader->nPayloadFileSize % poHeader->nSectorSize) != 0 )
            {
                if( poBaseHandle->Seek(
                        poHeader->nHeaderSize + nLastSectorOffset, 0) == 0 &&
                    poBaseHandle->Read(
                       pabyWB, poHeader->nSectorSize, 1 ) == 1 &&
                    DecryptBlock( pabyWB, nLastSectorOffset) )
                {
#ifdef VERBOSE_VSICRYPT
                    CPLDebug("VSICRYPT", "Filling %d trailing bytes with 0",
                             static_cast<int>(poHeader->nSectorSize -
                                              (poHeader->nPayloadFileSize -
                                               nLastSectorOffset )));
#endif
                    // Fill with 0.
                    memset(
                        pabyWB + poHeader->nPayloadFileSize - nLastSectorOffset,
                        0,
                        static_cast<int>(
                            poHeader->nSectorSize -
                            (poHeader->nPayloadFileSize - nLastSectorOffset)));

                    if( poBaseHandle->Seek(
                            poHeader->nHeaderSize + nLastSectorOffset, 0) == 0 )
                    {
                        EncryptBlock( pabyWB, nLastSectorOffset);
                        poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 );
                    }
                }
            }
            poBaseHandle->Seek(poHeader->nHeaderSize + nSectorOffset, SEEK_SET);
            if( poBaseHandle->Read( pabyWB, poHeader->nSectorSize, 1 ) == 0 ||
                !DecryptBlock( pabyWB, nSectorOffset) )
            {
                memset( pabyWB, 0, poHeader->nSectorSize );
            }
            else if( nSectorOffset + poHeader->nSectorSize >
                     poHeader->nPayloadFileSize )
            {
                // If the last sector was padded with random values,
                // decrypt it to 0 in case of update scenarios.
                CPLAssert( nSectorOffset < poHeader->nPayloadFileSize );
                memset(pabyWB + poHeader->nPayloadFileSize - nSectorOffset,
                       0,
                       nSectorOffset + poHeader->nSectorSize -
                       poHeader->nPayloadFileSize );
            }
            nWBOffset = nSectorOffset;
            nWBSize = poHeader->nSectorSize;
        }
    }

    int nRet = static_cast<int>( (nSize * nMemb - nToWrite) / nSize );
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Write ret = %d (nMemb = %d)",
             nRet, static_cast<int>(nMemb));
#endif
    return nRet;
}

/************************************************************************/
/*                             Truncate()                               */
/************************************************************************/

// Returns 0 on success.  Returns -1 on error.
int VSICryptFileHandle::Truncate( vsi_l_offset nNewSize )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Truncate(" CPL_FRMT_GUIB ")", nNewSize);
#endif
    if( (nPerms & VSICRYPT_WRITE) == 0 )
        return -1;

    if( !FlushDirty() )
        return -1;
    if( poBaseHandle->Truncate(
            poHeader->nHeaderSize +
            ((nNewSize + poHeader->nSectorSize - 1) / poHeader->nSectorSize) *
            poHeader->nSectorSize ) != 0 )
        return -1;
    bUpdateHeader = true;
    poHeader->nPayloadFileSize = nNewSize;
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSICryptFileHandle::Eof()
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Eof() = %d", static_cast<int>(bEOF));
#endif
    return bEOF;
}

/************************************************************************/
/*                                 Flush()                              */
/************************************************************************/

int VSICryptFileHandle::Flush()
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Flush()");
#endif
    if( !FlushDirty() )
    {
        return -1;
    }
    if( (nPerms & VSICRYPT_WRITE) )
    {
        if( bLastSectorWasModified &&
            (poHeader->nPayloadFileSize % poHeader->nSectorSize) != 0 )
        {
            const vsi_l_offset nLastSectorOffset =
                (poHeader->nPayloadFileSize / poHeader->nSectorSize) *
                poHeader->nSectorSize;
            if( poBaseHandle->Seek(
                    poHeader->nHeaderSize + nLastSectorOffset, 0) == 0 &&
                poBaseHandle->Read(
                    pabyWB, poHeader->nSectorSize, 1 ) == 1 &&
                DecryptBlock( pabyWB, nLastSectorOffset) )
            {
                // Fill with random
#ifdef VERBOSE_VSICRYPT
                CPLDebug(
                    "VSICRYPT", "Filling %d trailing bytes with random",
                    static_cast<int>(
                        poHeader->nSectorSize -
                        (poHeader->nPayloadFileSize - nLastSectorOffset)));
#endif
                CryptoPP::OS_GenerateRandomBlock(
                    false, // Do not need cryptographic randomness.
                    reinterpret_cast<cryptopp_byte*>(pabyWB +
                            poHeader->nPayloadFileSize - nLastSectorOffset),
                    static_cast<int>(
                        poHeader->nSectorSize -
                        (poHeader->nPayloadFileSize - nLastSectorOffset)));

                if( poBaseHandle->Seek(
                         poHeader->nHeaderSize + nLastSectorOffset, 0) == 0 )
                {
                    EncryptBlock( pabyWB, nLastSectorOffset);
                    poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 );
                }
            }
        }
        bLastSectorWasModified = false;
        if( poBaseHandle->Flush() != 0 )
            return -1;
    }
    if( bUpdateHeader )
    {
#ifdef VERBOSE_VSICRYPT
        CPLDebug("VSICRYPT", "nPayloadFileSize = " CPL_FRMT_GUIB,
                 poHeader->nPayloadFileSize);
#endif
        if( !poHeader->WriteToFile(poBaseHandle, poEncCipher) )
            return -1;
    }

    return 0;
}

/************************************************************************/
/*                                  Close()                             */
/************************************************************************/

int VSICryptFileHandle::Close()
{
    int nRet = 0;
    if( poBaseHandle != nullptr && poHeader != nullptr )
    {
        if( Flush() != 0 )
            return -1;
        nRet = poBaseHandle->Close();
        delete poBaseHandle;
        poBaseHandle = nullptr;
    }
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Close(%s)", osBaseFilename.c_str());
#endif
    return nRet;
}

/************************************************************************/
/*                   VSICryptFilesystemHandler                          */
/************************************************************************/

class VSICryptFilesystemHandler final : public VSIFilesystemHandler
{
public:
    VSICryptFilesystemHandler();
    ~VSICryptFilesystemHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
    int Unlink( const char *pszFilename ) override;
    int Rename( const char *oldpath, const char *newpath ) override;
    char** ReadDirEx( const char *pszDirname, int nMaxFiles ) override;
};

/************************************************************************/
/*                   VSICryptFilesystemHandler()                        */
/************************************************************************/

VSICryptFilesystemHandler::VSICryptFilesystemHandler()
{
}

/************************************************************************/
/*                    ~VSICryptFilesystemHandler()                      */
/************************************************************************/

VSICryptFilesystemHandler::~VSICryptFilesystemHandler()
{
}

/************************************************************************/
/*                             GetFilename()                            */
/************************************************************************/

static CPLString GetFilename( const char* pszFilename )
{
    if( strcmp(pszFilename, VSICRYPT_PREFIX_WITHOUT_SLASH) == 0 )
        pszFilename = VSICRYPT_PREFIX;

    CPLAssert( strncmp(pszFilename, VSICRYPT_PREFIX,
                       strlen(VSICRYPT_PREFIX)) == 0 );
    pszFilename += strlen(VSICRYPT_PREFIX);
    const char* pszFileArg = strstr(pszFilename, "file=");
    if( pszFileArg == nullptr )
        return pszFilename;
    CPLString osRet(pszFileArg + strlen("file="));
    return osRet;
}

/************************************************************************/
/*                             GetArgument()                            */
/************************************************************************/

static CPLString GetArgument( const char* pszFilename, const char* pszParamName,
                              const char* pszDefault = "" )
{
    CPLString osParamName(pszParamName);
    osParamName += "=";

    const char* pszNeedle = strstr(pszFilename, osParamName);
    if( pszNeedle == nullptr )
        return pszDefault;

    CPLString osRet(pszNeedle + osParamName.size());
    size_t nCommaPos = osRet.find(",");
    if( nCommaPos != std::string::npos )
        osRet.resize(nCommaPos);
    return osRet;
}

/************************************************************************/
/*                               GetKey()                               */
/************************************************************************/

static CPLString GetKey( const char* pszFilename )
{
    CPLString osKey = GetArgument(pszFilename, "key");
    // TODO(schwehr): Make 10U and 1024U into symbolic constants.
    if( osKey.empty() )
    {
        const char* pszKey = CPLGetConfigOption("VSICRYPT_KEY", "");
        // Do some form of validation to please Coverity
        CPLAssert( strlen(pszKey) < 10U * 1024U );
        // coverity [tainted_data_transitive]
        osKey = pszKey;
    }
    if( osKey.empty() || EQUAL(osKey, "GENERATE_IT") )
    {
        CPLString osKeyB64(GetArgument(pszFilename, "key_b64"));
        if( osKeyB64.empty() )
        {
            const char* pszKey = CPLGetConfigOption("VSICRYPT_KEY_B64", "");
            // Do some form of validation to please Coverity
            CPLAssert( strlen(pszKey) < 10U * 1024U );
            // coverity [tainted_data_transitive]
            osKeyB64 = pszKey;
        }
        if( !osKeyB64.empty() )
        {
            GByte* key = reinterpret_cast<GByte*>(CPLStrdup(osKeyB64));
            int nLength = CPLBase64DecodeInPlace(key);
            osKey.assign(reinterpret_cast<const char*>(key), nLength);
            memset(key, 0, osKeyB64.size());
            CPLFree(key);
        }
        // coverity[tainted_data]
        memset(const_cast<char*>(osKeyB64.c_str()), 0, osKeyB64.size());
    }
    return osKey;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *VSICryptFilesystemHandler::Open( const char *pszFilename,
                                                   const char *pszAccess,
                                                   bool /* bSetError */,
                                                   CSLConstList /* papszOptions */ )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Open(%s, %s)", pszFilename, pszAccess);
#endif
    CPLString osFilename(GetFilename(pszFilename));

    CPLString osKey(GetKey(pszFilename));
    if( osKey.empty() && pabyGlobalKey == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Encryption key not defined as key/key_b64 parameter, "
                "VSICRYPT_KEY/VSICRYPT_KEY_B64 configuration option or "
                 "VSISetCryptKey() API");
        return nullptr;
    }

    if( strchr(pszAccess, 'r') )
    {
        CPLString osAccess(pszAccess);
        if( strchr(pszAccess, 'b') == nullptr )
            osAccess += "b";
        VSIVirtualHandle* fpBase =
            reinterpret_cast<VSIVirtualHandle*>(VSIFOpenL(osFilename, osAccess));
        if( fpBase == nullptr )
            return nullptr;
        VSICryptFileHeader* poHeader = new VSICryptFileHeader();
        if( !poHeader->ReadFromFile(fpBase, osKey) )
        {
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            fpBase->Close();
            delete fpBase;
            delete poHeader;
            return nullptr;
        }

        VSICryptFileHandle* poHandle =
            new VSICryptFileHandle(
                osFilename, fpBase, poHeader,
                strchr(pszAccess, '+')
                ? VSICRYPT_READ | VSICRYPT_WRITE
                : VSICRYPT_READ);
        if( !poHandle->Init(osKey, false) )
        {
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            delete poHandle;
            poHandle = nullptr;
        }
        memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
        return poHandle;
    }
    else if( strchr(pszAccess, 'w' ) )
    {
        CPLString osAlg(GetArgument(pszFilename, "alg",
                                    CPLGetConfigOption("VSICRYPT_ALG", "AES")));
        VSICryptAlg eAlg = GetAlg(osAlg);

        VSICryptMode eMode =
            GetMode(GetArgument(pszFilename, "mode",
                                CPLGetConfigOption("VSICRYPT_MODE", "CBC")));

        CPLString osFreeText =
            GetArgument(pszFilename, "freetext",
                        CPLGetConfigOption("VSICRYPT_FREETEXT", ""));

        CPLString osIV = GetArgument(pszFilename, "iv",
                                           CPLGetConfigOption("VSICRYPT_IV",
                                                              ""));

        int nSectorSize =
            atoi(GetArgument(pszFilename, "sector_size",
                             CPLGetConfigOption("VSICRYPT_SECTOR_SIZE",
                                                "512")));
        if( nSectorSize <= 0 || nSectorSize >= 65535 )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Invalid value for sector_size. Defaulting to 512.");
            nSectorSize = 512;
        }

        const bool bAddKeyCheck =
            CPLTestBool(
                GetArgument(pszFilename, "add_key_check",
                            CPLGetConfigOption("VSICRYPT_ADD_KEY_CHECK",
                                               "NO")));

        /* Generate random initial vector */
        CryptoPP::BlockCipher* poBlock = GetEncBlockCipher(eAlg);
        if( poBlock == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cipher algorithm not supported in this build: %s",
                     osAlg.c_str());
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            return nullptr;
        }
        int nMinKeySize = static_cast<int>(poBlock->MinKeyLength());
        int nMaxKeySize = static_cast<int>(poBlock->MaxKeyLength());
        int nBlockSize = static_cast<int>(poBlock->BlockSize());
        delete poBlock;

        if( !osIV.empty() )
        {
            if( static_cast<int>(osIV.size()) != nBlockSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "IV should be %d byte large",
                         nBlockSize);
                memset(const_cast<char *>(osKey.c_str()), 0, osKey.size());
                return nullptr;
            }
        }
        else
        {
            osIV.resize(nBlockSize);
            CryptoPP::OS_GenerateRandomBlock(
                false,  // Do not need cryptographic randomness.
                reinterpret_cast<cryptopp_byte*>(const_cast<char*>(osIV.c_str())), osIV.size());
        }

        if( EQUAL(osKey, "GENERATE_IT") )
        {
            osKey.resize(nMaxKeySize);
            CPLDebug("VSICRYPT",
                     "Generating key. This might take some time...");
            CryptoPP::OS_GenerateRandomBlock(
                // Need cryptographic randomness.
                // Config option for speeding tests.
                CPLTestBool(CPLGetConfigOption("VSICRYPT_CRYPTO_RANDOM",
                                               "TRUE")),
                reinterpret_cast<cryptopp_byte*>(const_cast<char*>(osKey.c_str())), osKey.size());

            char* pszB64 = CPLBase64Encode(static_cast<int>(osKey.size()),
                                           reinterpret_cast<const GByte*>(osKey.c_str()));
            if( CPLTestBool(CPLGetConfigOption("VSICRYPT_DISPLAY_GENERATED_KEY",
                                               "TRUE")) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "BASE64 key '%s' has been generated, and installed in "
                        "the VSICRYPT_KEY_B64 configuration option.", pszB64);
            }
            CPLSetConfigOption("VSICRYPT_KEY_B64", pszB64);
            CPLFree(pszB64);
        }

        const int nKeyLength =
            !osKey.empty() ? static_cast<int>(osKey.size()) : nGlobalKeySize;
        if( nKeyLength < nMinKeySize )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Key is too short: %d bytes. Should be at least %d bytes",
                     nKeyLength, nMinKeySize);
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            return nullptr;
        }

        VSIVirtualHandle* fpBase =
            reinterpret_cast<VSIVirtualHandle *>(VSIFOpenL(osFilename, "wb+"));
        if( fpBase == nullptr )
        {
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            return nullptr;
        }

        VSICryptFileHeader* poHeader = new VSICryptFileHeader();
        poHeader->osIV = osIV;
        poHeader->eAlg = eAlg;
        poHeader->eMode = eMode;
        poHeader->nSectorSize = static_cast<GUInt16>(nSectorSize);
        poHeader->osFreeText = osFreeText;
        poHeader->bAddKeyCheck = bAddKeyCheck;

        VSICryptFileHandle* poHandle =
            new VSICryptFileHandle(
                osFilename, fpBase, poHeader,
                strchr(pszAccess, '+')
                ? VSICRYPT_READ | VSICRYPT_WRITE
                : VSICRYPT_WRITE);
        if( !poHandle->Init(osKey, true) )
        {
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            delete poHandle;
            poHandle = nullptr;
        }
        memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
        return poHandle;
    }
    else if( strchr(pszAccess, 'a') )
    {
        VSIVirtualHandle* fpBase =
            reinterpret_cast<VSIVirtualHandle *>(VSIFOpenL(osFilename, "rb+"));
        if( fpBase == nullptr )
        {
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            return VSIFilesystemHandler::Open(pszFilename, "wb+");
        }
        VSICryptFileHeader* poHeader = new VSICryptFileHeader();
        if( !poHeader->ReadFromFile(fpBase, osKey) )
        {
            memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
            fpBase->Close();
            delete fpBase;
            delete poHeader;
            return nullptr;
        }

        VSICryptFileHandle* poHandle =
            new VSICryptFileHandle( osFilename, fpBase, poHeader,
                                    VSICRYPT_READ | VSICRYPT_WRITE );
        if( !poHandle->Init(osKey) )
        {
            delete poHandle;
            poHandle = nullptr;
        }
        memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
        if( poHandle != nullptr )
            poHandle->Seek(0, SEEK_END);
        return poHandle;
    }

    return nullptr;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSICryptFilesystemHandler::Stat( const char *pszFilename,
                                     VSIStatBufL *pStatBuf, int nFlags )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Stat(%s)", pszFilename);
#endif
    CPLString osFilename(GetFilename(pszFilename));
    if( VSIStatExL( osFilename, pStatBuf, nFlags ) != 0 )
        return -1;
    VSIVirtualHandle* fp = reinterpret_cast<VSIVirtualHandle*>(VSIFOpenL(osFilename, "rb"));
    if( fp == nullptr )
        return -1;
    VSICryptFileHeader* poHeader = new VSICryptFileHeader();
    CPLString osKey(GetKey(pszFilename));
    if( !poHeader->ReadFromFile(fp, osKey) )
    {
        memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
        fp->Close();
        delete fp;
        delete poHeader;
        return -1;
    }
    memset(const_cast<char*>(osKey.c_str()), 0, osKey.size());
    fp->Close();
    delete fp;
    if( poHeader )
    {
        pStatBuf->st_size = poHeader->nPayloadFileSize;
        delete poHeader;
        return 0;
    }
    else
        return -1;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSICryptFilesystemHandler::Unlink( const char *pszFilename )
{
    return VSIUnlink(GetFilename(pszFilename));
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSICryptFilesystemHandler::Rename( const char *oldpath,
                                       const char* newpath )
{
    CPLString osNewPath;
    if( strncmp(newpath, VSICRYPT_PREFIX, strlen(VSICRYPT_PREFIX)) == 0 )
        osNewPath = GetFilename(newpath);
    else
        osNewPath = newpath;

    return VSIRename(GetFilename(oldpath), osNewPath);
}

/************************************************************************/
/*                               ReadDirEx()                            */
/************************************************************************/

char** VSICryptFilesystemHandler::ReadDirEx( const char *pszDirname,
                                             int nMaxFiles )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "ReadDir(%s)", pszDirname);
#endif
    return VSIReadDirEx(GetFilename(pszDirname), nMaxFiles);
}

#ifdef VSICRYPT_DRIVER

#include "gdal_priv.h"

/**
 * \brief Evaluate if this is a crypt file.
 *
 * The function signature must match GDALDataset::Identify.
 *
 * @param poOpenInfo The header bytes used for file identification.
 *
 * @return 1 if this is a crypt file or 0 otherwise.
 */

static int VSICryptIdentify(GDALOpenInfo* poOpenInfo)
{
    return poOpenInfo->nHeaderBytes > 8 &&
           memcmp(poOpenInfo->pabyHeader, VSICRYPT_SIGNATURE, 8) == 0;
}

static GDALDataset* VSICryptOpen(GDALOpenInfo* poOpenInfo)
{
    if( !VSICryptIdentify(poOpenInfo) )
        return nullptr;
    return GDALOpen(
        (CPLString(VSICRYPT_PREFIX) + poOpenInfo->pszFilename).c_str(),
        poOpenInfo->eAccess );
}

#endif

//! @endcond

/************************************************************************/
/*                   VSIInstallCryptFileHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsicrypt/ encrypted file system handler
 * (requires <a href="http://www.cryptopp.com/">libcrypto++</a>)
 *
 * A special file handler is installed that allows reading/creating/update
 * encrypted files on the fly, with random access capabilities.
 *
 * The cryptographic algorithms used are
 * <a href="https://en.wikipedia.org/wiki/Block_cipher">block ciphers</a>,
 * with symmetric key.
 *
 * In their simplest form, recognized filenames are of the form
 * /vsicrypt//absolute_path/to/file, /vsicrypt/c:/absolute_path/to/file or
 * /vsicrypt/relative/path/to/file.
 *
 * Options can also be used with the following format :
 * /vsicrypt/option1=val1,option2=val2,...,file=/path/to/file
 *
 * They can also be passed as configuration option/environment variable, because
 * in some use cases, the syntax with option in the filename might not properly
 * work with some drivers.
 *
 * In all modes, the encryption key must be provided. There are several ways
 * of doing so :
 * <ul>
 * <li>By adding a key= parameter to the filename, like
 *     /vsicrypt/key=my_secret_key,file=/path/to/file.  Note that this restricts
 *     the key to be in text format, whereas at its full power, it can be binary
 *     content.</li>
 * <li>By adding a key_b64= parameter to the filename, to specify a binary key
 *     expressed in Base64 encoding, like
 *     /vsicrypt/key_b64=th1sl00kslikebase64=,file=/path/to/file.</li>
 * <li>By setting the VSICRYPT_KEY configuration option. The key should be in
 * text format.</li>
 * <li>By setting the VSICRYPT_KEY_B64 configuration option. The key should be
 * encoded in Base64.</li>
 * <li>By using the VSISetCryptKey() C function.</li>
 * </ul>
 *
 * When creating a file, if key=GENERATE_IT or VSICRYPT_KEY=GENERATE_IT is
 * passed, the encryption key will be generated from the pseudo-random number
 * generator of the operating system. The key will be displayed on the standard
 * error stream in a Base64 form (unless the VSICRYPT_DISPLAY_GENERATED_KEY
 * configuration option is set to OFF), and the VSICRYPT_KEY_B64 configuration
 * option will also be set with the Base64 form of the key (so that
 * CPLGetConfigOption("VSICRYPT_KEY_B64", NULL) can be used to get it back).
 *
 * The available options are :
 * <ul>

 * <li>alg=AES/Blowfish/Camellia/CAST256/DES_EDE2/DES_EDE3/MARS/IDEA/RC5/RC6/Serpent/SHACAL2/SKIPJACK/Twofish/XTEA:
 *     to specify the <a href="https://en.wikipedia.org/wiki/Block_cipher">block
 *     cipher</a> algorithm.  The default is AES.  Only used on
 *     creation. Ignored otherwise.  Note: depending on how GDAL is build, if
 *     linked against the DLL version of libcrypto++, only a subset of those
 *     algorithms will be available, namely AES, DES_EDE2, DES_EDE3 and
 *     SKIPJACK.  Also available as VSICRYPT_ALG configuration option.</li>
 * <li>mode=CBC/CFB/OFB/CTR/CBC_CTS: to specify the
 *     <a href="https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation">
 *       block cipher mode of operation</a>.
 *     The default is CBC.
 *     Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_MODE configuration option.</li>
 * <li>key=text_key: see above.</li>
 * <li>key_b64=base64_encoded_key: see above.</li>
 * <li>freetext=some_text: to specify a text content that will be written
 *     *unencrypted* in the file header, for informational purposes. Default to
 *     empty.  Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_FREETEXT configuration option.</li>
 * <li>sector_size=int_value: to specify the size of the "sector", which is the
 *     unit chunk of information that is encrypted/decrypted. Default to 512
 *     bytes.  The valid values depend on the algorithm and block cipher mode of
 *     operation.  Only used on creation. Ignored otherwise.  Also available as
 *     VSICRYPT_SECTOR_SIZE configuration option.</li>
 * <li>iv=initial_vector_as_text: to specify the Initial Vector. This is an
 *     advanced option that should generally *NOT* be used. It is only useful to
 *     get completely deterministic output given the plaintext, key and other
 *     parameters, which in general *NOT* what you want to do. By default, a
 *     random initial vector of the appropriate size will be generated for each
 *     new file created.  Only used on creation. Ignored otherwise.  Also
 *     available as VSICRYPT_IV configuration option.</li>

 * <li>add_key_check=YES/NO: whether a special value should be encrypted in the
 *     header, so as to be quickly able to determine if the decryption key is
 *     correct.  Defaults to NO.  Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_ADD_KEY_CHECK configuration option.</li>
 * <li>file=filename. To specify the filename. This must be the last option put
 *     in the option list (so as to make it possible to use filenames with comma
 *     in them. )
 * </ul>
 *
 * This special file handler can be combined with other virtual filesystems
 * handlers, such as /vsizip. For example,
 * /vsicrypt//vsicurl/path/to/remote/encrypted/file.tif
 *
 * Implementation details:
 *
 * The structure of encrypted files is the following: a header, immediately
 * followed by the encrypted payload (by sectors, i.e. chunks of sector_size
 * bytes).
 *
 * The header structure is the following :
 * <ol>
 * <li>8 bytes. Signature. Fixed value: VSICRYPT.</li>
 * <li>UINT16_LE. Header size (including previous signature bytes).</li>
 * <li>UINT8. Format major version. Current value: 1.</li>
 * <li>UINT8. Format minor version. Current value: 0.</li>
 * <li>UINT16. Sector size.</li>
 * <li>UINT8. Cipher algorithm. Valid values are: 0 = AES (Rijndael), 1 =
 *     Blowfish, 2 = Camellia, 3 = CAST256, 4 = DES_EDE2, 5 = DES_EDE3, 6 =
 *     MARS, 7 = IDEA, 8 = RC5, 9 = RC6, 10 = Serpent, 11 = SHACAL2, 12 =
 *     SKIPJACK, 13 = Twofish, 14 = XTEA.</li>
 * <li>UINT8. Block cipher mode of operation. Valid values are: 0 = CBC, 1 =
 *     CFB, 2 = OFB, 3 = CTR, 4 = CBC_CTS.</li>
 * <li>UINT8. Size in bytes of the Initial Vector.</li>
 * <li>N bytes with the content of the Initial Vector, where N is the value of
 *     the previous field.</li>
 * <li>UINT16_LE. Size in bytes of the free text.</li>
 * <li>N bytes with the content of the free text, where N is the value of the
 *     previous field.</li>
 * <li>UINT8. Size in bytes of encrypted content (key check), or 0 if key check
 *     is absent.</li>
 * <li>N bytes with encrypted content (key check), where N is the value of the
 *     previous field.</li>
 * <li>UINT64_LE. Size of the unencrypted file, in bytes.</li>
 * <li>UINT16_LE. Size in bytes of extra content (of unspecified semantics). For
 *     v1.0, fixed value of 0</li>
 * <li>N bytes with extra content (of unspecified semantics), where N is the
 *     value of the previous field.</li>
 * </ol>
 *
 * This design does not provide any means of authentication or integrity check.
 *
 * Each sector is encrypted/decrypted independently of other sectors.  For that,
 * the Initial Vector contained in the header is XOR'ed with the file offset
 * (relative to plain text file) of the start of the sector being processed, as
 * a 8-byte integer.  More precisely, the first byte of the main IV is XOR'ed
 * with the 8 least-significant bits of the sector offset, the second byte of
 * the main IV is XOR'ed with the following 8 bits of the sector offset,
 * etc... until the 8th byte.
 *
 * This design could potentially be prone to chosen-plaintext attack, for
 * example if the attacker managed to get (part of) an existing encrypted file
 * to be encrypted from plaintext he might have selected.
 *
 * Note: if "hostile" code can explore process content, or attach to it with a
 * debugger, it might be relatively easy to retrieve the encryption key.  A GDAL
 * plugin could for example get the content of configuration options, or list
 * opened datasets and see the key/key_b64 values, so disabling plugin loading
 * might be a first step, as well as linking statically GDAL to application
 * code.  If plugin loading is enabled or GDAL dynamically linked, using
 * VSISetCryptKey() to set the key might make it a bit more complicated to spy
 * the key.  But, as said initially, this is in no way a perfect protection.
 *
 * @since GDAL 2.1.0
 */
void VSIInstallCryptFileHandler(void)

{
    VSIFileManager::InstallHandler( VSICRYPT_PREFIX,
                                    new VSICryptFilesystemHandler );

#ifdef VSICRYPT_DRIVER
    if( GDALGetDriverByName( "VSICRYPT" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "VSICRYPT" );
#ifdef GDAL_DCAP_RASTER
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
#endif
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               CPLSPrintf("Wrapper for %s files",
                                          VSICRYPT_PREFIX) );

    poDriver->pfnOpen = VSICryptOpen;
    poDriver->pfnIdentify = VSICryptIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
#endif
}

#else /* HAVE_CRYPTOPP */

class VSIDummyCryptFilesystemHandler : public VSIFilesystemHandler
{
public:
    VSIDummyCryptFilesystemHandler() {}

    VSIVirtualHandle *Open( const char * /* pszFilename */,
                            const char * /* pszAccess */,
                            bool /* bSetError */,
                            CSLConstList /* papszOptions */ ) override
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s support not available in this build", VSICRYPT_PREFIX);
        return nullptr;
    }

    int Stat( const char * /* pszFilename */,
              VSIStatBufL * /*pStatBuf */, int /* nFlags */ ) override
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s support not available in this build", VSICRYPT_PREFIX);
        return -1;
    }
};

void VSIInstallCryptFileHandler(void)
{
    VSIFileManager::InstallHandler( VSICRYPT_PREFIX,
                                    new VSIDummyCryptFilesystemHandler );
}

void VSISetCryptKey( const GByte* /* pabyKey */, int /* nKeySize */ )
{
    // Not supported.
}

#endif  // HAVE_CRYPTOPP

// Below is only useful if using as a plugin over GDAL >= 2.0.
#ifdef VSICRYPT_AUTOLOAD

CPL_C_START
void CPL_DLL GDALRegisterMe();
CPL_C_END

void GDALRegisterMe()
{
    VSIFilesystemHandler* poExistingHandler =
                    VSIFileManager::GetHandler(VSICRYPT_PREFIX);
    if( poExistingHandler == VSIFileManager::GetHandler(".") )
    {
        // In the case where VSICRYPT_PREFIX is just handled by the regular
        // handler, install the vsicrypt handler (shouldn't happen)
        VSIInstallCryptFileHandler();
    }
    else
    {
        // If there's already an installed handler, then check if it is a
        // dummy one (should normally be the case) or a real one
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        VSIStatBufL sStat;
        CPL_IGNORE_RET_VAL(
            VSIStatL((CPLString(VSICRYPT_PREFIX) + "i_do_not_exist").c_str(), &sStat));
        CPLPopErrorHandler();
        if( strstr(CPLGetLastErrorMsg(), "support not available in this build") )
        {
            // Dummy handler. Register the new one, and delete the old one
            VSIInstallCryptFileHandler();
            delete poExistingHandler;
        }
        else
        {
            CPLDebug("VSICRYPT", "GDAL has already a working %s implementation",
                     VSICRYPT_PREFIX);
        }
        CPLErrorReset();
    }
}

#endif /* VSICRYPT_AUTOLOAD */
