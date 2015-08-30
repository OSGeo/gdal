/**********************************************************************
 * $Id$
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

#include "cpl_vsi_virtual.h"

/* Increase Major in case of backward incompatible changes */
#define VSICRYPT_CURRENT_MAJOR          1
#define VSICRYPT_CURRENT_MINOR          0
#define VSICRYPT_SIGNATURE              "VSICRYPT"

#define VSICRYPT_READ  0x1
#define VSICRYPT_WRITE 0x2

CPL_C_START
void CPL_DLL VSIInstallCryptFileHandler(void);
void CPL_DLL VSISetCryptKey(const GByte* pabyKey, int nKeySize);
CPL_C_END

CPL_CVSID("$Id$");

#if defined(HAVE_CRYPTOPP) || defined(DOXYGEN_SKIP)

/* Begin of crypto++ headers */

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
/* End of crypto++ headers */

// I don't really understand why this is necessary, especially
// when cryptopp.dll and GDAL have been compiled with the same
// VC version and /MD. But otherwise you'll get crashes
// Borrowed from dlltest.cpp of crypto++
#if defined(WIN32) && defined(USE_ONLY_CRYPTODLL_ALG)

static CryptoPP::PNew s_pNew = NULL;
static CryptoPP::PDelete s_pDelete = NULL;

extern "C" __declspec(dllexport)
void __cdecl SetNewAndDeleteFromCryptoPP(CryptoPP::PNew pNew,
                                         CryptoPP::PDelete pDelete,
                                         CryptoPP::PSetNewHandler pSetNewHandler)
{
	s_pNew = pNew;
	s_pDelete = pDelete;
}

void * __cdecl operator new (size_t size)
{
	return s_pNew(size);
}

void __cdecl operator delete (void * p)
{
	s_pDelete(p);
}

#endif //  defined(WIN32) && defined(USE_ONLY_CRYPTODLL_ALG)

static GByte* pabyGlobalKey = NULL;
static int nGlobalKeySize = 0;

typedef enum
{
    ALG_AES,
    ALG_Blowfish,
    ALG_Camellia,
    //ALG_CAST128, (obsolete)
    ALG_CAST256,
    //ALG_DES, (obsolete)
    ALG_DES_EDE2,
    ALG_DES_EDE3,
    //ALG_DES_XEX3, (obsolete)
    //ALG_Gost, (obsolete)
    ALG_MARS,
    ALG_IDEA,
    //ALG_RC2, (obsolete)
    ALG_RC5,
    ALG_RC6,
    //ALG_SAFER_K, (obsolete)
    //ALG_SAFER_SK, (obsolete)
    ALG_Serpent,
    ALG_SHACAL2,
    //ALG_SHARK, (obsolete)
    ALG_SKIPJACK,
    ALG_Twofish,
    //ALG_ThreeWay, (obsolete)
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
 * @param nKeySize length of the key in bytes. Might be 0 to clear previously set key.
 *
 * @see VSIInstallCryptFileHandler() for documentation on /vsicrypt/
 */
void VSISetCryptKey(const GByte* pabyKey, int nKeySize)
{
    CPLAssert( (pabyKey != NULL && nKeySize != 0) ||
               (pabyKey == NULL && nKeySize == 0) );
    if( pabyGlobalKey )
    {
        // Make some effort to clear the memory, although it could have leaked
        // elsewhere...
        memset( pabyGlobalKey, 0, nGlobalKeySize );
        CPLFree(pabyGlobalKey);
        pabyGlobalKey = NULL;
        nGlobalKeySize = 0;
    }
    if( pabyKey )
    {
        pabyGlobalKey = (GByte*) CPLMalloc(nKeySize);
        memcpy(pabyGlobalKey, pabyKey, nKeySize);
        nGlobalKeySize = nKeySize;
    }
}

/************************************************************************/
/*                             GetAlg()                                 */
/************************************************************************/

#undef CASE_ALG
#define CASE_ALG(alg)   if( EQUAL(pszName, #alg) ) return ALG_##alg;

static VSICryptAlg GetAlg(const char* pszName)
{
    CASE_ALG(AES)
    CASE_ALG(Blowfish)
    CASE_ALG(Camellia)
    //CASE_ALG(CAST128) (obsolete)
    CASE_ALG(CAST256)
    //CASE_ALG(DES) (obsolete)
    CASE_ALG(DES_EDE2)
    CASE_ALG(DES_EDE3)
    //CASE_ALG(DES_XEX3) (obsolete)
    //CASE_ALG(Gost) (obsolete)
    CASE_ALG(MARS)
    CASE_ALG(IDEA)
    //CASE_ALG(RC2) (obsolete)
    CASE_ALG(RC5)
    CASE_ALG(RC6)
    //CASE_ALG(SAFER_K) (obsolete)
    //CASE_ALG(SAFER_SK) (obsolete)
    CASE_ALG(Serpent)
    CASE_ALG(SHACAL2)
    //CASE_ALG(SHARK) (obsolete)
    CASE_ALG(SKIPJACK)
    //CASE_ALG(ThreeWay) (obsolete)
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

static CryptoPP::BlockCipher* GetEncBlockCipher(VSICryptAlg eAlg)
{
    switch( eAlg )
    {
        CASE_ALG(AES)
#ifndef USE_ONLY_CRYPTODLL_ALG
        CASE_ALG(Blowfish)
        CASE_ALG(Camellia)
        //CASE_ALG(CAST128) (obsolete)
        CASE_ALG(CAST256)
#endif
        //CASE_ALG(DES) (obsolete)
        CASE_ALG(DES_EDE2)
        CASE_ALG(DES_EDE3)
        //CASE_ALG(DES_XEX3) (obsolete)
#ifndef USE_ONLY_CRYPTODLL_ALG
        //CASE_ALG(Gost) (obsolete)
        CASE_ALG(MARS)
        CASE_ALG(IDEA)
        //CASE_ALG(RC2) (obsolete)
        CASE_ALG(RC5)
        CASE_ALG(RC6)
        //CASE_ALG(SAFER_K) (obsolete)
        //CASE_ALG(SAFER_SK) (obsolete)
        CASE_ALG(Serpent)
        CASE_ALG(SHACAL2)
        //CASE_ALG(SHARK) (obsolete)
#endif
        CASE_ALG(SKIPJACK)
#ifndef USE_ONLY_CRYPTODLL_ALG
        //CASE_ALG(ThreeWay) (obsolete)
        CASE_ALG(Twofish)
        CASE_ALG(XTEA)
#endif
        default: return NULL;
    }
}

/************************************************************************/
/*                          GetDecBlockCipher()                         */
/************************************************************************/

#undef CASE_ALG
#define CASE_ALG(alg)   case ALG_##alg: return new CryptoPP::alg::Decryption();

static CryptoPP::BlockCipher* GetDecBlockCipher(VSICryptAlg eAlg)
{
    switch( eAlg )
    {
        CASE_ALG(AES)
#ifndef USE_ONLY_CRYPTODLL_ALG
        CASE_ALG(Blowfish)
        CASE_ALG(Camellia)
        //CASE_ALG(CAST128) (obsolete)
        CASE_ALG(CAST256)
#endif
        //CASE_ALG(DES) (obsolete)
        CASE_ALG(DES_EDE2)
        CASE_ALG(DES_EDE3)
        //CASE_ALG(DES_XEX3) (obsolete)
#ifndef USE_ONLY_CRYPTODLL_ALG
        //CASE_ALG(Gost) (obsolete)
        CASE_ALG(MARS)
        CASE_ALG(IDEA)
        //CASE_ALG(RC2) (obsolete)
        CASE_ALG(RC5)
        CASE_ALG(RC6)
        //CASE_ALG(SAFER_K) (obsolete)
        //CASE_ALG(SAFER_SK) (obsolete)
        CASE_ALG(Serpent)
        CASE_ALG(SHACAL2)
        //CASE_ALG(SHARK) (obsolete)
#endif
        CASE_ALG(SKIPJACK)
#ifndef USE_ONLY_CRYPTODLL_ALG
        //CASE_ALG(ThreeWay) (obsolete)
        CASE_ALG(Twofish)
        CASE_ALG(XTEA)
#endif
        default: return NULL;
    }
}

/************************************************************************/
/*                             GetMode()                                */
/************************************************************************/

static VSICryptMode GetMode(const char* pszName)
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
        std::string             CryptKeyCheck(CryptoPP::BlockCipher* poEncCipher);

    public:
        VSICryptFileHeader() : nHeaderSize(0),
                               nMajorVersion(0),
                               nMinorVersion(0),
                               nSectorSize(512),
                               eAlg(ALG_AES),
                               eMode(MODE_CBC),
                               bAddKeyCheck(FALSE),
                               nPayloadFileSize(0)  {}

        int ReadFromFile(VSIVirtualHandle* fp, const CPLString& osKey);
        int WriteToFile(VSIVirtualHandle* fp, CryptoPP::BlockCipher* poEncCipher);

        GUInt16 nHeaderSize;
        GByte nMajorVersion;
        GByte nMinorVersion;
        GUInt16 nSectorSize;
        VSICryptAlg eAlg;
        VSICryptMode eMode;
        CPLString osIV;
        int bAddKeyCheck;
        GUIntBig nPayloadFileSize;
        CPLString osFreeText;
        CPLString osExtraContent;
};

/************************************************************************/
/*                         VSICryptReadError()                          */
/************************************************************************/

static int VSICryptReadError()
{
    CPLError(CE_Failure, CPLE_FileIO, "Cannot read header");
    return FALSE;
}

/************************************************************************/
/*                       VSICryptGenerateSectorIV()                     */
/************************************************************************/

static std::string VSICryptGenerateSectorIV(const std::string& osIV,
                                            vsi_l_offset nOffset)
{
    std::string osSectorIV(osIV);
    size_t nLength = MIN(sizeof(vsi_l_offset), osSectorIV.size());
    for( size_t i=0; i < nLength; i++ )
    {
        osSectorIV[i] = (char)((osSectorIV[i] ^ nOffset) & 0xff);
        nOffset >>= 8;
    }
    return osSectorIV;
}

/************************************************************************/
/*                          CryptKeyCheck()                             */
/************************************************************************/

std::string VSICryptFileHeader::CryptKeyCheck(CryptoPP::BlockCipher* poEncCipher)
{
    std::string osKeyCheckRes;

    CPLAssert( osIV.size() == poEncCipher->BlockSize() );
    // Generate a unique IV with a sector offset of 0xFFFFFFFFFFFFFFFF
    std::string osCheckIV(VSICryptGenerateSectorIV(osIV, ~((vsi_l_offset)0)));
    CryptoPP::StringSink* poSink = new CryptoPP::StringSink(osKeyCheckRes);
    CryptoPP::StreamTransformation* poMode = new CryptoPP::CBC_Mode_ExternalCipher::Encryption(*poEncCipher, (const byte*)osCheckIV.c_str() );
    CryptoPP::StreamTransformationFilter* poEnc = new CryptoPP::StreamTransformationFilter(*poMode, poSink, CryptoPP::StreamTransformationFilter::NO_PADDING);
    /* Not sure if it is add extra security, but pick up something that is unlikely to be a plain text (random number) */
    poEnc->Put((const byte*)"\xDB\x31\xB9\x1B\xD3\x1C\xFA\x3E\x84\x06\xC1\x42\xC3\xEC\xCD\x9A\x02\x36\x22\x15\x58\x88\x74\x65\x00\x2F\x98\xBC\x69\x22\xE1\x63",
               MIN(32, poEncCipher->BlockSize()));
    poEnc->MessageEnd();
    delete poEnc;
    delete poMode;

    return osKeyCheckRes;
}

/************************************************************************/
/*                          ReadFromFile()                              */
/************************************************************************/

int VSICryptFileHeader::ReadFromFile(VSIVirtualHandle* fp, const CPLString& osKey)
{
    GByte abySignature[8];
    fp->Seek(0, SEEK_SET);
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
    eAlg = (VSICryptAlg)nAlg;
    eMode = (VSICryptMode)nMode;
    
    GByte nIVSize;
    if( fp->Read(&nIVSize, 1, 1) == 0 )
        return VSICryptReadError();

    osIV.resize(nIVSize);
    if( fp->Read((void*)osIV.c_str(), 1, nIVSize) != nIVSize )
        return VSICryptReadError();

    GUInt16 nFreeTextSize;
    if( fp->Read(&nFreeTextSize, 2, 1) == 0 )
        return VSICryptReadError();

    osFreeText.resize(nFreeTextSize);
    if( fp->Read((void*)osFreeText.c_str(), 1, nFreeTextSize) != nFreeTextSize )
        return VSICryptReadError();
    
    GByte nKeyCheckSize;
    if( fp->Read(&nKeyCheckSize, 1, 1) == 0 )
        return VSICryptReadError();
    bAddKeyCheck = (nKeyCheckSize != 0);
    if( nKeyCheckSize )
    {
        CPLString osKeyCheck;
        osKeyCheck.resize(nKeyCheckSize);
        if( fp->Read((void*)osKeyCheck.c_str(), 1, nKeyCheckSize) != nKeyCheckSize )
            return VSICryptReadError();
        
        if( osKey.size() == 0 && pabyGlobalKey == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Encryption key not defined as key/key_b64 parameter, "
                    "VSICRYPT_KEY/VSICRYPT_KEY_B64 configuration option or VSISetCryptKey() API");
            return FALSE;
        }

        CryptoPP::BlockCipher* poEncCipher = GetEncBlockCipher(eAlg);
        if( poEncCipher == NULL )
            return FALSE;

        if( osIV.size() != poEncCipher->BlockSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistant initial vector");
            delete poEncCipher;
            return FALSE;
        }

        int nMaxKeySize = poEncCipher->MaxKeyLength();

        try
        {
            if( osKey.size() )
            {
                int nKeySize = MIN(nMaxKeySize, (int)osKey.size());
                poEncCipher->SetKey((const byte*)osKey.c_str(), nKeySize);
            }
            else if( pabyGlobalKey )
            {
                int nKeySize = MIN(nMaxKeySize, nGlobalKeySize);
                poEncCipher->SetKey(pabyGlobalKey, nKeySize);
            }
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "CryptoPP exception: %s", e.what());
            return FALSE;
        }

        std::string osKeyCheckRes = CryptKeyCheck(poEncCipher);

        delete poEncCipher;

        if( osKeyCheck.size() != osKeyCheckRes.size() ||
            memcmp(osKeyCheck.c_str(), osKeyCheckRes.c_str(), osKeyCheck.size()) != 0 )
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

    GUInt16 nExtraContentSize;
    if( fp->Read(&nExtraContentSize, 2, 1) == 0 )
        return VSICryptReadError();
    nExtraContentSize = CPL_LSBWORD16(nExtraContentSize);

    osExtraContent.resize(nExtraContentSize);
    if( fp->Read((void*)osExtraContent.c_str(), 1, nExtraContentSize) != nExtraContentSize )
        return VSICryptReadError();

    return TRUE;
}

/************************************************************************/
/*                          WriteToFile()                               */
/************************************************************************/

int VSICryptFileHeader::WriteToFile(VSIVirtualHandle* fp, CryptoPP::BlockCipher* poEncCipher)
{
    int bRet = TRUE;

    fp->Seek(0, SEEK_SET);

    bRet &= (fp->Write(VSICRYPT_SIGNATURE, 8, 1) == 1);

    std::string osKeyCheckRes;
    if( bAddKeyCheck )
    {
        osKeyCheckRes = CryptKeyCheck(poEncCipher);
    }

    GUInt16 nHeaderSizeNew = 8 + /* signature */
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
                            2 + osExtraContent.size(); /* extra content */
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
    
    GByte nAlg = (GByte)eAlg;
    bRet &= (fp->Write(&nAlg, 1, 1) == 1);

    GByte nMode = (GByte)eMode;
    bRet &= (fp->Write(&nMode, 1, 1) == 1);

    GByte nIVSizeToWrite = (GByte)osIV.size();
    CPLAssert(nIVSizeToWrite == osIV.size());
    bRet &= (fp->Write(&nIVSizeToWrite, 1, 1) == 1);
    bRet &= (fp->Write(osIV.c_str(), 1, osIV.size()) == osIV.size());

    GUInt16 nFreeTextSizeToWrite = CPL_LSBWORD16((GUInt16)osFreeText.size());
    bRet &= (fp->Write(&nFreeTextSizeToWrite, 2, 1) == 1);
    bRet &= (fp->Write(osFreeText.c_str(), 1, osFreeText.size()) == osFreeText.size());

    GByte nSize = (GByte)osKeyCheckRes.size();
    bRet &= (fp->Write(&nSize, 1, 1) == 1);
    bRet &= (fp->Write(osKeyCheckRes.c_str(), 1, osKeyCheckRes.size()) == osKeyCheckRes.size());

    GUIntBig nPayloadFileSizeToWrite = nPayloadFileSize;
    CPL_LSBPTR64(&nPayloadFileSizeToWrite);
    bRet &= (fp->Write(&nPayloadFileSizeToWrite, 8, 1) == 1);
    
    GUInt16 nExtraContentSizeToWrite = CPL_LSBWORD16((GUInt16)osExtraContent.size());
    bRet &= (fp->Write(&nExtraContentSizeToWrite, 2, 1) == 1);
    bRet &= (fp->Write(osExtraContent.c_str(), 1, osExtraContent.size()) == osExtraContent.size());

    CPLAssert( fp->Tell() == nHeaderSize) ;
    
    return bRet;
}

/************************************************************************/
/*                          VSICryptFileHandle                          */
/************************************************************************/

class VSICryptFileHandle : public VSIVirtualHandle
{
  private:
        CPLString           osBaseFilename;
        int                 nPerms;
        VSIVirtualHandle   *poBaseHandle;
        VSICryptFileHeader *poHeader;
        int                 bUpdateHeader;
        vsi_l_offset        nCurPos;
        int                 bEOF;

        CryptoPP::BlockCipher* poEncCipher;
        CryptoPP::BlockCipher* poDecCipher;
        int                 nBlockSize;

        vsi_l_offset        nWBOffset;
        GByte*              pabyWB;
        int                 nWBSize;
        int                 bWBDirty;
        
        int                 bLastSectorWasModified;
    
        void                 EncryptBlock(GByte* pabyData, vsi_l_offset nOffset);
        int                  DecryptBlock(GByte* pabyData, vsi_l_offset nOffset);
        int                  FlushDirty();

  public:

    VSICryptFileHandle(CPLString osBaseFilename,
                       VSIVirtualHandle* poBaseHandle,
                       VSICryptFileHeader* poHeader,
                       int nPerms);
    ~VSICryptFileHandle();
    
    int                  Init(const CPLString& osKey, int bWriteHeader = FALSE);

    virtual int          Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t       Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t       Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int          Eof();
    virtual int          Flush();
    virtual int          Close();
    virtual int          Truncate( vsi_l_offset nNewSize );
};

/************************************************************************/
/*                          VSICryptFileHandle()                        */
/************************************************************************/

VSICryptFileHandle::VSICryptFileHandle(CPLString osBaseFilename,
                                       VSIVirtualHandle* poBaseHandle,
                                       VSICryptFileHeader* poHeader,
                                       int nPerms) :
        osBaseFilename(osBaseFilename), nPerms(nPerms),
        poBaseHandle(poBaseHandle), poHeader(poHeader), bUpdateHeader(FALSE),
        nCurPos(0), bEOF(FALSE), poEncCipher(NULL), poDecCipher(NULL), nBlockSize(0),
        nWBOffset(0), pabyWB(NULL), nWBSize(0), bWBDirty(FALSE), bLastSectorWasModified(FALSE)
{

}

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

int VSICryptFileHandle::Init(const CPLString& osKey, int bWriteHeader)
{
    poEncCipher = GetEncBlockCipher(poHeader->eAlg);
    if( poEncCipher == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cipher algorithm not supported in this build: %d",
                    (int)poHeader->eAlg);
        return FALSE;
    }
    
    if( poHeader->osIV.size() != poEncCipher->BlockSize() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Inconsistant initial vector");
        return FALSE;
    }

    poDecCipher = GetDecBlockCipher(poHeader->eAlg);
    nBlockSize = poEncCipher->BlockSize();
    int nMaxKeySize = poEncCipher->MaxKeyLength();
    
    try
    {
        if( osKey.size() )
        {
            int nKeySize = MIN(nMaxKeySize, (int)osKey.size());
            poEncCipher->SetKey((const byte*)osKey.c_str(), nKeySize);
            poDecCipher->SetKey((const byte*)osKey.c_str(), nKeySize);
        }
        else if( pabyGlobalKey )
        {
            int nKeySize = MIN(nMaxKeySize, nGlobalKeySize);
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

    pabyWB = (GByte*)CPLCalloc(1, poHeader->nSectorSize);

    if( (poHeader->nSectorSize % nBlockSize) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Sector size (%d) is not a multiple of block size (%d)",
                 poHeader->nSectorSize, nBlockSize);
        return FALSE;
    }
    if( poHeader->eMode == MODE_CBC_CTS && poHeader->nSectorSize < 2 * nBlockSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Sector size (%d) should be at least twice larger than the block size (%d) in CBC_CTS.",
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

void VSICryptFileHandle::EncryptBlock(GByte* pabyData, vsi_l_offset nOffset)
{
    std::string osRes;
    std::string osIV(VSICryptGenerateSectorIV(poHeader->osIV, nOffset));
    CPLAssert( (int)osIV.size() == nBlockSize );

    CryptoPP::StringSink* poSink = new CryptoPP::StringSink(osRes);
    CryptoPP::StreamTransformation* poMode;
    if( poHeader->eMode == MODE_CBC )
        poMode = new CryptoPP::CBC_Mode_ExternalCipher::Encryption(*poEncCipher, (const byte*)osIV.c_str() );
    else if( poHeader->eMode == MODE_CFB )
        poMode = new CryptoPP::CFB_Mode_ExternalCipher::Encryption(*poEncCipher, (const byte*)osIV.c_str() );
    else if( poHeader->eMode == MODE_OFB )
        poMode = new CryptoPP::OFB_Mode_ExternalCipher::Encryption(*poEncCipher, (const byte*)osIV.c_str() );
    else if( poHeader->eMode == MODE_CTR )
        poMode = new CryptoPP::CTR_Mode_ExternalCipher::Encryption(*poEncCipher, (const byte*)osIV.c_str() );
    else
        poMode = new CryptoPP::CBC_CTS_Mode_ExternalCipher::Encryption(*poEncCipher, (const byte*)osIV.c_str() );
    CryptoPP::StreamTransformationFilter* poEnc = new CryptoPP::StreamTransformationFilter(*poMode, poSink, CryptoPP::StreamTransformationFilter::NO_PADDING);
    poEnc->Put((const byte*)pabyData, poHeader->nSectorSize);
    poEnc->MessageEnd();
    delete poEnc;

    delete poMode;

    CPLAssert( (int)osRes.length() == poHeader->nSectorSize );
    memcpy( pabyData, osRes.c_str(), osRes.length() );
}

/************************************************************************/
/*                          DecryptBlock()                              */
/************************************************************************/

int VSICryptFileHandle::DecryptBlock(GByte* pabyData, vsi_l_offset nOffset)
{
    std::string osRes;
    std::string osIV(VSICryptGenerateSectorIV(poHeader->osIV, nOffset));
    CPLAssert( (int)osIV.size() == nBlockSize );
    CryptoPP::StringSink* poSink = new CryptoPP::StringSink(osRes);
    CryptoPP::StreamTransformation* poMode = NULL;
    CryptoPP::StreamTransformationFilter* poDec = NULL;
    
    try
    {
        /* Yes, some modes need the encryption cipher */
        if( poHeader->eMode == MODE_CBC )
            poMode = new CryptoPP::CBC_Mode_ExternalCipher::Decryption(*poDecCipher, (const byte*)osIV.c_str() );
        else if( poHeader->eMode == MODE_CFB )
            poMode = new CryptoPP::CFB_Mode_ExternalCipher::Decryption(*poEncCipher, (const byte*)osIV.c_str() );
        else if( poHeader->eMode == MODE_OFB )
            poMode = new CryptoPP::OFB_Mode_ExternalCipher::Decryption(*poEncCipher, (const byte*)osIV.c_str() );
        else if( poHeader->eMode == MODE_CTR )
            poMode = new CryptoPP::CTR_Mode_ExternalCipher::Decryption(*poEncCipher, (const byte*)osIV.c_str() );
        else
            poMode = new CryptoPP::CBC_CTS_Mode_ExternalCipher::Decryption(*poDecCipher, (const byte*)osIV.c_str() );
        CryptoPP::StreamTransformationFilter* poDec = new CryptoPP::StreamTransformationFilter(*poMode, poSink, CryptoPP::StreamTransformationFilter::NO_PADDING);
        poDec->Put((const byte*)pabyData, poHeader->nSectorSize);
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
        return FALSE;
    }

    CPLAssert( (int)osRes.length() == poHeader->nSectorSize );
    memcpy( pabyData, osRes.c_str(), osRes.length() );
    
    return TRUE;
}

/************************************************************************/
/*                             FlushDirty()                             */
/************************************************************************/

int VSICryptFileHandle::FlushDirty()
{
    if( !bWBDirty )
        return TRUE;
    bWBDirty = FALSE;
    
    EncryptBlock(pabyWB, nWBOffset);
    poBaseHandle->Seek( poHeader->nHeaderSize + nWBOffset, SEEK_SET );
    
    nWBOffset = 0;
    nWBSize = 0;
    
    if( poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 ) != 1 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSICryptFileHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Seek(nOffset=" CPL_FRMT_GUIB ", nWhence=%d)", nOffset, nWhence);
#endif

    bEOF = FALSE;

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
    GByte* pabyBuffer = (GByte*)pBuffer;

#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Read(nCurPos=" CPL_FRMT_GUIB ", nToRead=%d)", nCurPos, (int)nToRead);
#endif

    if( (nPerms & VSICRYPT_READ) == 0 )
        return 0;

    if( nCurPos >= poHeader->nPayloadFileSize )
    {
        bEOF = TRUE;
        return 0;
    }

    if( !FlushDirty() )
        return 0;

    while(nToRead > 0)
    {
        if( nCurPos >= nWBOffset && nCurPos < nWBOffset + nWBSize )
        {
            int nToCopy = MIN(nToRead, nWBSize - (nCurPos - nWBOffset));
            if( nCurPos + nToCopy > poHeader->nPayloadFileSize )
            {
                bEOF = TRUE;
                nToCopy = poHeader->nPayloadFileSize - nCurPos;
            }
            memcpy(pabyBuffer, pabyWB + nCurPos - nWBOffset, nToCopy);
            pabyBuffer += nToCopy;
            nToRead -= nToCopy;
            nCurPos += nToCopy;
            if( bEOF || nToRead == 0 )
                break;
            CPLAssert( (nCurPos % poHeader->nSectorSize) == 0 );
        }

        vsi_l_offset nSectorOffset = (nCurPos / poHeader->nSectorSize) * poHeader->nSectorSize;
        poBaseHandle->Seek( poHeader->nHeaderSize + nSectorOffset, SEEK_SET );
        if( poBaseHandle->Read( pabyWB, poHeader->nSectorSize, 1 ) != 1 )
        {
            bEOF = TRUE;
            break;
        }
        if( !DecryptBlock( pabyWB, nSectorOffset) )
        {
            break;
        }
        if( (nPerms & VSICRYPT_WRITE) &&
            nSectorOffset + poHeader->nSectorSize > poHeader->nPayloadFileSize )
        {
            // If the last sector was padded with random values, decrypt it to 0 in case of update scenarios
            CPLAssert( nSectorOffset < poHeader->nPayloadFileSize );
            memset( pabyWB + poHeader->nPayloadFileSize - nSectorOffset, 0,
                    nSectorOffset + poHeader->nSectorSize - poHeader->nPayloadFileSize );
        }
        nWBOffset = nSectorOffset;
        nWBSize = poHeader->nSectorSize;
    }

    int nRet = ( (nSize * nMemb - nToRead) / nSize );
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Read ret = %d (nMemb = %d)", nRet, (int)nMemb);
#endif
    return nRet;
}

/************************************************************************/
/*                                Write()                               */
/************************************************************************/

size_t VSICryptFileHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    size_t nToWrite = nSize * nMemb;
    const GByte* pabyBuffer = (const GByte*)pBuffer;

#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Write(nCurPos=" CPL_FRMT_GUIB ", nToWrite=%d,"
             "nPayloadFileSize=" CPL_FRMT_GUIB ",bWBDirty=%d,nWBOffset=" CPL_FRMT_GUIB ",nWBSize=%d)",
             nCurPos, (int)nToWrite, poHeader->nPayloadFileSize, bWBDirty, nWBOffset, nWBSize);
#endif
    
    if( (nPerms & VSICRYPT_WRITE) == 0 )
        return 0;

    if( nCurPos >= (poHeader->nPayloadFileSize / poHeader->nSectorSize) * poHeader->nSectorSize )
        bLastSectorWasModified = TRUE;

    /* If seeking past end of file, we need to explicitely encrypt the padding zeroes */
    if( nCurPos > poHeader->nPayloadFileSize && nCurPos > nWBOffset + nWBSize )
    {
        if( !FlushDirty() )
            return 0;
        vsi_l_offset nOffset = (poHeader->nPayloadFileSize + poHeader->nSectorSize - 1) / poHeader->nSectorSize * poHeader->nSectorSize;
        vsi_l_offset nEndOffset = nCurPos / poHeader->nSectorSize * poHeader->nSectorSize;
        for( ; nOffset < nEndOffset; nOffset += poHeader->nSectorSize )
        {
            memset( pabyWB, 0, poHeader->nSectorSize );
            EncryptBlock( pabyWB, nOffset );
            poBaseHandle->Seek( poHeader->nHeaderSize + nOffset, SEEK_SET );
            if( poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 ) != 1 )
                return 0;
            poHeader->nPayloadFileSize = nOffset + poHeader->nSectorSize;
            bUpdateHeader = TRUE;
        }
    }

    while( nToWrite > 0 )
    {
        if( nCurPos >= nWBOffset && nCurPos < nWBOffset + nWBSize )
        {
            bWBDirty = TRUE;
            int nToCopy = MIN(nToWrite, nWBSize - (nCurPos - nWBOffset));
            memcpy(pabyWB + nCurPos - nWBOffset, pabyBuffer, nToCopy);
            pabyBuffer += nToCopy;
            nToWrite -= nToCopy;
            nCurPos += nToCopy;
            if( nCurPos > poHeader->nPayloadFileSize )
            {
                bUpdateHeader = TRUE;
                poHeader->nPayloadFileSize = nCurPos;
            }
            if( nToWrite == 0 )
                break;
            CPLAssert( (nCurPos % poHeader->nSectorSize) == 0 );
        }
        else if( (nCurPos % poHeader->nSectorSize) == 0 && nToWrite >= (size_t)poHeader->nSectorSize )
        {
            if( !FlushDirty() )
                break;

            bWBDirty = TRUE;
            nWBOffset = nCurPos;
            nWBSize = poHeader->nSectorSize;
            memcpy( pabyWB, pabyBuffer, poHeader->nSectorSize );
            pabyBuffer += poHeader->nSectorSize;
            nToWrite -= poHeader->nSectorSize;
            nCurPos += poHeader->nSectorSize;
            if( nCurPos > poHeader->nPayloadFileSize )
            {
                bUpdateHeader = TRUE;
                poHeader->nPayloadFileSize = nCurPos;
            }
        }
        else
        {
            if( !FlushDirty() )
                break;

            vsi_l_offset nSectorOffset = (nCurPos / poHeader->nSectorSize) * poHeader->nSectorSize;
            vsi_l_offset nLastSectorOffset = (poHeader->nPayloadFileSize / poHeader->nSectorSize) * poHeader->nSectorSize;
            if( nSectorOffset > nLastSectorOffset &&
                (poHeader->nPayloadFileSize % poHeader->nSectorSize) != 0 )
            {
                if( poBaseHandle->Seek( poHeader->nHeaderSize + nLastSectorOffset, 0) == 0 &&
                    poBaseHandle->Read( pabyWB, poHeader->nSectorSize, 1 ) == 1 &&
                    DecryptBlock( pabyWB, nLastSectorOffset) )
                {
                    // Fill with 0
#ifdef VERBOSE_VSICRYPT
                    CPLDebug("VSICRYPT", "Filling %d trailing bytes with 0",
                            (int)(poHeader->nSectorSize - (poHeader->nPayloadFileSize - nLastSectorOffset )));
#endif
                    memset(
                        (byte*)(pabyWB + poHeader->nPayloadFileSize - nLastSectorOffset),
                        0,
                        (int)(poHeader->nSectorSize - (poHeader->nPayloadFileSize - nLastSectorOffset )));

                    if( poBaseHandle->Seek( poHeader->nHeaderSize + nLastSectorOffset, 0) == 0)
                    {
                        EncryptBlock( pabyWB, nLastSectorOffset);
                        poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 );
                    }
                }
            }
            poBaseHandle->Seek( poHeader->nHeaderSize + nSectorOffset, SEEK_SET );
            if( poBaseHandle->Read( pabyWB, poHeader->nSectorSize, 1 ) == 0 ||
                !DecryptBlock( pabyWB, nSectorOffset) )
            {
                memset( pabyWB, 0, poHeader->nSectorSize );
            }
            else if( nSectorOffset + poHeader->nSectorSize > poHeader->nPayloadFileSize )
            {
                // If the last sector was padded with random values, decrypt it to 0 in case of update scenarios
                CPLAssert( nSectorOffset < poHeader->nPayloadFileSize );
                memset( pabyWB + poHeader->nPayloadFileSize - nSectorOffset, 0,
                        nSectorOffset + poHeader->nSectorSize - poHeader->nPayloadFileSize );
            }
            nWBOffset = nSectorOffset;
            nWBSize = poHeader->nSectorSize;
        }
    }

    int nRet = ( (nSize * nMemb - nToWrite) / nSize );
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Write ret = %d (nMemb = %d)", nRet, (int)nMemb);
#endif
    return nRet;
}

/************************************************************************/
/*                             Truncate()                               */
/************************************************************************/

int VSICryptFileHandle::Truncate( vsi_l_offset nNewSize )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Truncate(" CPL_FRMT_GUIB ")", nNewSize);
#endif
    if( (nPerms & VSICRYPT_WRITE) == 0 )
        return -1;

    if( !FlushDirty() )
        return -1;
    if( poBaseHandle->Truncate( poHeader->nHeaderSize +
            ((nNewSize + poHeader->nSectorSize - 1) / poHeader->nSectorSize) * poHeader->nSectorSize ) != 0 )
        return -1;
    bUpdateHeader = TRUE;
    poHeader->nPayloadFileSize = nNewSize;
    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSICryptFileHandle::Eof()
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Eof() = %d", bEOF);
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
        if( bLastSectorWasModified && (poHeader->nPayloadFileSize % poHeader->nSectorSize) != 0 )
        {
            vsi_l_offset nLastSectorOffset = (poHeader->nPayloadFileSize / poHeader->nSectorSize) * poHeader->nSectorSize;
            if( poBaseHandle->Seek( poHeader->nHeaderSize + nLastSectorOffset, 0) == 0 &&
                poBaseHandle->Read( pabyWB, poHeader->nSectorSize, 1 ) == 1 &&
                DecryptBlock( pabyWB, nLastSectorOffset) )
            {
                // Fill with random
#ifdef VERBOSE_VSICRYPT
                CPLDebug("VSICRYPT", "Filling %d trailing bytes with random",
                         (int)(poHeader->nSectorSize - (poHeader->nPayloadFileSize - nLastSectorOffset )));
#endif
                CryptoPP::OS_GenerateRandomBlock(false /* we do not need cryptographic randomness */,
                    (byte*)(pabyWB + poHeader->nPayloadFileSize - nLastSectorOffset),
                    (int)(poHeader->nSectorSize - (poHeader->nPayloadFileSize - nLastSectorOffset )));

                if( poBaseHandle->Seek( poHeader->nHeaderSize + nLastSectorOffset, 0) == 0)
                {
                    EncryptBlock( pabyWB, nLastSectorOffset);
                    poBaseHandle->Write( pabyWB, poHeader->nSectorSize, 1 );
                }
            }
        }
        bLastSectorWasModified = FALSE;
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
    if( poBaseHandle != NULL && poHeader != NULL )
    {
        if( Flush() != 0 )
            return -1;
        nRet = poBaseHandle->Close();
        delete poBaseHandle;
        poBaseHandle = NULL;
    }
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Close(%s)", osBaseFilename.c_str());
#endif
    return nRet;
}

/************************************************************************/
/*                   VSICryptFilesystemHandler                          */
/************************************************************************/

class VSICryptFilesystemHandler : public VSIFilesystemHandler 
{
public:
    VSICryptFilesystemHandler();
    ~VSICryptFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Rename( const char *oldpath, const char *newpath );
    virtual char**   ReadDir( const char *pszDirname );
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

static CPLString GetFilename(const char* pszFilename)
{
    if( strcmp(pszFilename, "/vsicrypt") == 0 )
        pszFilename = "/vsicrypt/";

    CPLAssert( strncmp(pszFilename, "/vsicrypt/", strlen("/vsicrypt/")) == 0 );
    pszFilename += strlen("/vsicrypt/");
    const char* pszFileArg = strstr(pszFilename, "file=");
    if( pszFileArg == NULL )
        return pszFilename;
    CPLString osRet(pszFileArg + strlen("file="));
    return osRet;
}

/************************************************************************/
/*                             GetArgument()                            */
/************************************************************************/

static CPLString GetArgument(const char* pszFilename, const char* pszParamName,
                             const char* pszDefault = "")
{
    CPLString osParamName(pszParamName);
    osParamName += "=";
    
    const char* pszNeedle = strstr(pszFilename, osParamName);
    if( pszNeedle == NULL )
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

static CPLString GetKey(const char* pszFilename)
{
    CPLString osKey = GetArgument(pszFilename, "key");
    if( osKey.size() == 0 )
        osKey = CPLGetConfigOption("VSICRYPT_KEY", "");
    if( osKey.size() == 0 || EQUAL(osKey, "GENERATE_IT") )
    {
        CPLString osKeyB64(GetArgument(pszFilename, "key_b64"));
        if( osKeyB64.size() == 0 )
            osKeyB64 = CPLGetConfigOption("VSICRYPT_KEY_B64", "");
        if( osKeyB64.size() )
        {
            GByte* key = (GByte*)CPLStrdup(osKeyB64);
            int nLength = CPLBase64DecodeInPlace(key);
            osKey.assign((const char*)key, nLength);
            memset(key, 0, osKeyB64.size());
            CPLFree(key);
        }
        memset((void*)osKeyB64.c_str(), 0, osKeyB64.size());
    }
    return osKey;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *VSICryptFilesystemHandler::Open( const char *pszFilename, 
                                                   const char *pszAccess)
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Open(%s, %s)", pszFilename, pszAccess);
#endif
    CPLString osFilename(GetFilename(pszFilename));

    CPLString osKey(GetKey(pszFilename));
    if( osKey.size() == 0 && pabyGlobalKey == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Encryption key not defined as key/key_b64 parameter, "
                "VSICRYPT_KEY/VSICRYPT_KEY_B64 configuration option or VSISetCryptKey() API");
        return NULL;
    }

    if( strchr(pszAccess, 'r') )
    {
        CPLString osAccess(pszAccess);
        if( strchr(pszAccess, 'b') == NULL )
            osAccess += "b";
        VSIVirtualHandle* fpBase = (VSIVirtualHandle*)VSIFOpenL(osFilename, osAccess);
        if( fpBase == NULL )
            return NULL;
        VSICryptFileHeader* poHeader = new VSICryptFileHeader();
        if( !poHeader->ReadFromFile(fpBase, osKey) )
        {
            memset((void*)osKey.c_str(), 0, osKey.size());
            fpBase->Close();
            delete fpBase;
            delete poHeader;
            return NULL;
        }

        VSICryptFileHandle* poHandle = new VSICryptFileHandle( osFilename, fpBase, poHeader,
                    strchr(pszAccess, '+') ? VSICRYPT_READ | VSICRYPT_WRITE : VSICRYPT_READ);
        if( !poHandle->Init(osKey, FALSE) )
        {
            memset((void*)osKey.c_str(), 0, osKey.size());
            delete poHandle;
            poHandle = NULL;
        }
        memset((void*)osKey.c_str(), 0, osKey.size());
        return poHandle;
    }
    else if( strchr(pszAccess, 'w' ) )
    {
        CPLString osAlg(GetArgument(pszFilename, "alg",
                                              CPLGetConfigOption("VSICRYPT_ALG", "AES")));
        VSICryptAlg eAlg = GetAlg(osAlg);

        VSICryptMode eMode = GetMode(GetArgument(pszFilename, "mode",
                                              CPLGetConfigOption("VSICRYPT_MODE", "CBC")));

        CPLString osFreeText = GetArgument(pszFilename, "freetext",
                                           CPLGetConfigOption("VSICRYPT_FREETEXT", ""));

        CPLString osIV = GetArgument(pszFilename, "iv",
                                           CPLGetConfigOption("VSICRYPT_IV", ""));

        int nSectorSize = atoi(GetArgument(pszFilename, "sector_size",
                                           CPLGetConfigOption("VSICRYPT_SECTOR_SIZE", "512")));

        int bAddKeyCheck = CSLTestBoolean(GetArgument(pszFilename, "add_key_check",
                                           CPLGetConfigOption("VSICRYPT_ADD_KEY_CHECK", "NO")));

        /* Generate random initial vector */
        CryptoPP::BlockCipher* poBlock = GetEncBlockCipher(eAlg);
        if( poBlock == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cipher algorithm not supported in this build: %s",
                     osAlg.c_str());
            memset((void*)osKey.c_str(), 0, osKey.size());
            return NULL;
        }
        int nMinKeySize = poBlock->MinKeyLength();
        int nMaxKeySize = poBlock->MaxKeyLength();
        int nBlockSize = poBlock->BlockSize();
        delete poBlock;

        if( osIV.size() != 0 )
        {
            if( (int)osIV.size() != nBlockSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "IV should be %d byte large",
                         nBlockSize);
                memset((void*)osKey.c_str(), 0, osKey.size());
                return NULL;
            }
        }
        else
        {
            osIV.resize(nBlockSize);
            CryptoPP::OS_GenerateRandomBlock(false /* we do not need cryptographic randomness */,
                                            (byte*)osIV.c_str(), osIV.size());
        }

        if( EQUAL(osKey, "GENERATE_IT") )
        {
            osKey.resize(nMaxKeySize);
            CPLDebug("VSICRYPT", "Generating key. This might take some time...");
            CryptoPP::OS_GenerateRandomBlock(
                /* we need cryptographic randomness (config option for speeding tests) */
                CSLTestBoolean(CPLGetConfigOption("VSICRYPT_CRYPTO_RANDOM", "TRUE")),
                (byte*)osKey.c_str(), osKey.size());

            char* pszB64 = CPLBase64Encode(osKey.size(), (const GByte*)osKey.c_str());
            if( CSLTestBoolean(CPLGetConfigOption("VSICRYPT_DISPLAY_GENERATED_KEY", "TRUE")) )
            {
                fprintf(stderr, "BASE64 key '%s' has been generated, and installed in "
                        "the VSICRYPT_KEY_B64 configuration option.\n", pszB64);
            }
            CPLSetConfigOption("VSICRYPT_KEY_B64", pszB64);
            CPLFree(pszB64);
        }

        int nKeyLength = ( osKey.size() ) ? (int)osKey.size() : nGlobalKeySize;
        if( nKeyLength < nMinKeySize )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Key is too short: %d bytes. Should be at least %d bytes",
                     nKeyLength, nMinKeySize);
            memset((void*)osKey.c_str(), 0, osKey.size());
            return NULL;
        }

        VSIVirtualHandle* fpBase = (VSIVirtualHandle*)VSIFOpenL(osFilename, "wb+");
        if( fpBase == NULL )
        {
            memset((void*)osKey.c_str(), 0, osKey.size());
            return NULL;
        }

        VSICryptFileHeader* poHeader = new VSICryptFileHeader();
        poHeader->osIV = osIV;
        poHeader->eAlg = eAlg;
        poHeader->eMode = eMode;
        poHeader->nSectorSize = nSectorSize;
        poHeader->osFreeText = osFreeText;
        poHeader->bAddKeyCheck = bAddKeyCheck;

        VSICryptFileHandle* poHandle = new VSICryptFileHandle( osFilename, fpBase, poHeader,
                    strchr(pszAccess, '+') ? VSICRYPT_READ | VSICRYPT_WRITE : VSICRYPT_WRITE);
        if( !poHandle->Init(osKey, TRUE) )
        {
            memset((void*)osKey.c_str(), 0, osKey.size());
            delete poHandle;
            poHandle = NULL;
        }
        memset((void*)osKey.c_str(), 0, osKey.size());
        return poHandle;
    }
    else if( strchr(pszAccess, 'a') )
    {
        VSIVirtualHandle* fpBase = (VSIVirtualHandle*)VSIFOpenL(osFilename, "rb+");
        if( fpBase == NULL )
        {
            memset((void*)osKey.c_str(), 0, osKey.size());
            return Open(pszFilename, "wb+");
        }
        VSICryptFileHeader* poHeader = new VSICryptFileHeader();
        if( !poHeader->ReadFromFile(fpBase, osKey) )
        {
            memset((void*)osKey.c_str(), 0, osKey.size());
            fpBase->Close();
            delete fpBase;
            delete poHeader;
            return NULL;
        }

        VSICryptFileHandle* poHandle = new VSICryptFileHandle( osFilename, fpBase, poHeader,
                                                               VSICRYPT_READ | VSICRYPT_WRITE );
        if( !poHandle->Init(osKey) )
        {
            delete poHandle;
            poHandle = NULL;
        }
        memset((void*)osKey.c_str(), 0, osKey.size());
        if( poHandle != NULL )
            poHandle->Seek(0, SEEK_END);
        return poHandle;
    }

    return NULL;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSICryptFilesystemHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "Stat(%s)", pszFilename);
#endif
    CPLString osFilename(GetFilename(pszFilename));
    if( VSIStatExL( osFilename, pStatBuf, nFlags ) != 0 )
        return -1;
    VSIVirtualHandle* fp = (VSIVirtualHandle*)VSIFOpenL(osFilename, "rb");
    if( fp == NULL )
        return -1;
    VSICryptFileHeader* poHeader = new VSICryptFileHeader();
    CPLString osKey(GetKey(pszFilename));
    if( !poHeader->ReadFromFile(fp, osKey) )
    {
        memset((void*)osKey.c_str(), 0, osKey.size());
        fp->Close();
        delete fp;
        delete poHeader;
        return -1;
    }
    memset((void*)osKey.c_str(), 0, osKey.size());
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

int VSICryptFilesystemHandler::Rename( const char *oldpath, const char* newpath )
{
    CPLString osNewPath;
    if( strncmp(newpath, "/vsicrypt/", strlen("/vsicrypt/")) == 0 )
        osNewPath = GetFilename(newpath);
    else
        osNewPath = newpath;

    return VSIRename(GetFilename(oldpath), osNewPath);
}

/************************************************************************/
/*                               ReadDir()                              */
/************************************************************************/

char** VSICryptFilesystemHandler::ReadDir( const char *pszDirname )
{
#ifdef VERBOSE_VSICRYPT
    CPLDebug("VSICRYPT", "ReadDir(%s)", pszDirname);
#endif
    return VSIReadDir(GetFilename(pszDirname));
}

#ifdef VSICRYPT_DRIVER

#include "gdal_priv.h"

static int VSICryptIdentify(GDALOpenInfo* poOpenInfo)
{
    return poOpenInfo->nHeaderBytes > 8 &&
           memcmp(poOpenInfo->pabyHeader, VSICRYPT_SIGNATURE, 8) == 0;
}

static GDALDataset* VSICryptOpen(GDALOpenInfo* poOpenInfo)
{
    if( !VSICryptIdentify(poOpenInfo) )
        return NULL;
    return (GDALDataset*)GDALOpen( (CPLString("/vsicrypt/") + poOpenInfo->pszFilename).c_str(),
                     poOpenInfo->eAccess );
}

#endif

/************************************************************************/
/*                   VSIInstallCryptFileHandler()                       */
/************************************************************************/

/**
 * \brief Install /vsicrypt/ encrypted file system handler (requires <a href="http://www.cryptopp.com/">libcrypto++</a>)
 *
 * A special file handler is installed that allows reading/creating/update encrypted
 * files on the fly, with random access capabilities.
 *
 * The cryptographic algorithms used are
 * <a href="https://en.wikipedia.org/wiki/Block_cipher">block ciphers</a>, with symmetric key.
 *
 * In their simplest form, recognized filenames are of the form /vsicrypt//absolute_path/to/file,
 * /vsicrypt/c:/absolute_path/to/file or /vsicrypt/relative/path/to/file.
 *
 * Options can also be used with the following format :
 * /vsicrypt/option1=val1,option2=val2,...,file=/path/to/file
 *
 * They can also be passed as configuration option/environment variable, because
 * in some use cases, the syntax with option in the filename might not properly work with some drivers.
 *
 * In all modes, the encryption key must be provided. There are several ways
 * of doing so :
 * <ul>
 * <li>By adding a key= parameter to the filename, like /vsicrypt/key=my_secret_key,file=/path/to/file.
 *     Note that this restricts the key to be in text format, whereas at its full power,
 *     it can be binary content.</li>
 * <li>By adding a key_b64= parameter to the filename, to specify a binary key expressed
 *     in Base64 encoding, like /vsicrypt/key_b64=th1sl00kslikebase64=,file=/path/to/file.</li>
 * <li>By setting the VSICRYPT_KEY configuration option. The key should be in text format.</li>
 * <li>By setting the VSICRYPT_KEY_B64 configuration option. The key should be encoded in Base64.</li>
 * <li>By using the VSISetCryptKey() C function.</li>
 * </ul>
 *
 * When creating a file, if key=GENERATE_IT or VSICRYPT_KEY=GENERATE_IT is passed,
 * the encryption key will be generated from the pseudo-random number generator of the
 * operating system. The key will be displayed on the standard error stream in a Base64 form
 * (unless the VSICRYPT_DISPLAY_GENERATED_KEY configuration option is set to OFF),
 * and the VSICRYPT_KEY_B64 configuration option will also be set with the Base64 form
 * of the key (so that CPLGetConfigOption("VSICRYPT_KEY_B64", NULL) can be used to get it back).
 *
 * The available options are :
 * <ul>
 * <li>alg=AES/Blowfish/Camellia/CAST256/DES_EDE2/DES_EDE3/MARS/IDEA/RC5/RC6/Serpent/SHACAL2/SKIPJACK/Twofish/XTEA:
 *     to specify the <a href="https://en.wikipedia.org/wiki/Block_cipher">block cipher</a> algorithm.
 *     The default is AES.
 *     Only used on creation. Ignored otherwise.
 *     Note: depending on how GDAL is build, if linked against the DLL version of libcrypto++,
 *     only a subset of those algorithms will be available, namely AES, DES_EDE2, DES_EDE3 and SKIPJACK. 
 *     Also available as VSICRYPT_ALG configuration option.</li>
 * <li>mode=CBC/CFB/OFB/CTR/CBC_CTS: to specify the <a href="https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation">block cipher mode of operation</a>.
 *     The default is CBC.
 *     Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_MODE configuration option.</li>
 * <li>key=text_key: see above.</li>
 * <li>key_b64=base64_encoded_key: see above.</li>
 * <li>freetext=some_text: to specify a text content that will be written *unencrypted*
 *     in the file header, for informational purposes. Default to empty.
 *     Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_FREETEXT configuration option.</li>
 * <li>sector_size=int_value: to specify the size of the "sector", which is the
 *     unit chunk of information that is encrypted/decrypted. Default to 512 bytes.
 *     The valid values depend on the algorithm and block cipher mode of operation.
 *     Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_SECTOR_SIZE configuration option.</li>
 * <li>iv=initial_vector_as_text: to specify the Initial Vector. This is an
 *     advanced option that should generally *NOT* be used. It is only useful
 *     to get completely deterministic output
 *     given the plaintext, key and other parameters, which in general *NOT* what
 *     you want to do. By default, a random initial vector of the appropriate size
 *     will be generated for each new file created.
 *     Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_IV configuration option.</li>
 * <li>add_key_check=YES/NO: whether a special value should be encrypted in the header,
 *     so as to be quickly able to determine if the decryption key is correct.
 *     Defaults to NO.
 *     Only used on creation. Ignored otherwise.
 *     Also available as VSICRYPT_ADD_KEY_CHECK configuration option.</li>
 * <li>file=filename. To specify the filename. This must be the last option put in the
 *     option list (so as to make it possible to use filenames with comma in them. )
 * </ul>
 *
 * This special file handler can be combined with other virtual filesystems handlers,
 * such as /vsizip. For example, /vsicrypt//vsicurl/path/to/remote/encrypted/file.tif
 *
 * Implementation details:
 *
 * The structure of encrypted files is the following: a header, immediatly
 * followed by the encrypted payload (by sectors, i.e. chunks of sector_size bytes).
 *
 * The header structure is the following :
 * <ol>
 * <li>8 bytes. Signature. Fixed value: VSICRYPT.</li>
 * <li>UINT16_LE. Header size (including previous signature bytes).</li>
 * <li>UINT8. Format major version. Current value: 1.</li>
 * <li>UINT8. Format minor version. Current value: 0.</li>
 * <li>UINT16. Sector size.</li>
 * <li>UINT8. Cipher algorithm. Valid values are: 0 = AES (Rijndael), 1 = Blowfish, 2 = Camellia, 3 = CAST256,
 *     4 = DES_EDE2, 5 = DES_EDE3, 6 = MARS, 7 = IDEA, 8 = RC5, 9 = RC6, 10 = Serpent, 11 = SHACAL2,
 *     12 = SKIPJACK, 13 = Twofish, 14 = XTEA.</li>
 * <li>UINT8. Block cipher mode of operation. Valid values are: 0 = CBC, 1 = CFB, 2 = OFB, 3 = CTR, 4 = CBC_CTS.</li>
 * <li>UINT8. Size in bytes of the Initial Vector.</li>
 * <li>N bytes with the content of the Initial Vector, where N is the value of the previous field.</li>
 * <li>UINT16_LE. Size in bytes of the free text.</li>
 * <li>N bytes with the content of the free text, where N is the value of the previous field.</li>
 * <li>UINT8. Size in bytes of encrypted content (key check), or 0 if key check is absent.</li>
 * <li>N bytes with encrypted content (key check), where N is the value of the previous field.</li>
 * <li>UINT64_LE. Size of the unencrypted file, in bytes.</li>
 * <li>UINT16_LE. Size in bytes of extra content (of unspecified semantics). For v1.0, fixed value of 0</li>
 * <li>N bytes with extra content (of unspecified semantics), where N is the value of the previous field.</li>
 * </ol>
 *
 * This design does not provide any means of authentication or integrity check.
 *
 * Each sector is encrypted/decrypted independantly of other sectors.
 * For that, the Initial Vector contained in the header is XOR'ed with the file offset
 * (relative to plain text file) of the start of the sector being processed, as a 8-byte integer.
 * More precisely, the first byte of the main IV is XOR'ed with the 8 least-significant
 * bits of the sector offset, the second byte of the main IV is XOR'ed with the following
 * 8 bits of the sector offset, etc... until the 8th byte.
 *
 * This design could potentially be prone to chosen-plaintext attack, for example
 * if the attacker managed to get (part of) an existing encrypted file to be encrypted from
 * plaintext he might have selected.
 *
 * Note: if "hostile" code can explore process content, or attach to it with a
 * debugger, it might be relatively easy to retrieve the encryption key.
 * A GDAL plugin could for example get the content of configuration options, or
 * list opened datasets and see the key/key_b64 values, so disabling plugin loading
 * might be a first step, as well as linking statically GDAL to application code.
 * If plugin loading is enabled or GDAL dynamically linked, using VSISetCryptKey()
 * to set the key might make it a bit more complicated to spy the key.
 * But, as said initially, this is in no way a perfect protection.
 *
 * @since GDAL 2.1.0
 */
void VSIInstallCryptFileHandler(void)

{
    VSIFileManager::InstallHandler( "/vsicrypt/", new VSICryptFilesystemHandler );

#ifdef VSICRYPT_DRIVER
    if( GDALGetDriverByName( "VSICRYPT" ) == NULL )
    {
        GDALDriver      *poDriver = new GDALDriver();

        poDriver->SetDescription( "VSICRYPT" );
#ifdef GDAL_DCAP_RASTER
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
#endif
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Wrapper for /vsicrypt/ files" );

        poDriver->pfnOpen = VSICryptOpen;
        poDriver->pfnIdentify = VSICryptIdentify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
#endif
}

#else /* HAVE_CRYPTOPP */

class VSIDummyCryptFilesystemHandler : public VSIFilesystemHandler 
{
public:
    VSIDummyCryptFilesystemHandler() {}

    virtual VSIVirtualHandle *Open( CPL_UNUSED const char *pszFilename, 
                                    CPL_UNUSED const char *pszAccess)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "/vsicrypt/ support not available in this build");
        return NULL;
    }

    virtual int Stat( CPL_UNUSED const char *pszFilename,
                      CPL_UNUSED VSIStatBufL *pStatBuf, CPL_UNUSED int nFlags )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "/vsicrypt/ support not available in this build");
        return -1;
    }
};

void VSIInstallCryptFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsicrypt/", new VSIDummyCryptFilesystemHandler );
}

void VSISetCryptKey(CPL_UNUSED const GByte* pabyKey, CPL_UNUSED int nKeySize)
{
    /* not supported */
}


#endif /* HAVE_CRYPTOPP */

/* Below is only useful if using as a plugin over GDAL 1.11 or GDAL 2.0 */
#ifdef VSICRYPT_AUTOLOAD

CPL_C_START
void CPL_DLL GDALRegisterMe();
CPL_C_END

void GDALRegisterMe()
{
    if( VSIFileManager::GetHandler("/vsicrypt/") == VSIFileManager::GetHandler(".") )
        VSIInstallCryptFileHandler();
}

#ifndef GDAL_DCAP_RASTER
CPL_C_START
void CPL_DLL RegisterOGRCRYPT();
CPL_C_END

void RegisterOGRCRYPT()
{
    if( VSIFileManager::GetHandler("/vsicrypt/") == VSIFileManager::GetHandler(".") )
        VSIInstallCryptFileHandler();
}
#endif

#endif /* VSICRYPT_AUTOLOAD */
