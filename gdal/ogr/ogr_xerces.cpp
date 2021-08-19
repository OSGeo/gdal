/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Convenience functions for parsing with Xerces-C library
 *           Functions for translating back and forth between XMLCh and char.
 *           We assume that XMLCh is a simple numeric type that we can
 *           correspond 1:1 with char values, but that it likely is larger
 *           than a char.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
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
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABOGRXercesTY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABOGRXercesTY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

// Must be first for DEBUG_BOOL case
#include "ogr_xerces.h"

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

#ifdef HAVE_XERCES

static CPLMutex* hMutex = nullptr;
static int nCounter = 0;
static bool bXercesWasAlreadyInitializedBeforeUs = false;


/************************************************************************/
/*                      OGRXercesBinInputStream                         */
/************************************************************************/

class OGRXercesBinInputStream final: public BinInputStream
{
    CPL_DISALLOW_COPY_ASSIGN(OGRXercesBinInputStream)

    VSILFILE* fp = nullptr;
    bool bOwnFP = false;
    XMLCh emptyString = 0;

  public:
    explicit OGRXercesBinInputStream( VSILFILE* fpIn, bool bOwnFPIn );
    ~OGRXercesBinInputStream() override;

    XMLFilePos curPos() const override;
    XMLSize_t readBytes(XMLByte* const toFill,
                                const XMLSize_t maxToRead) override;
    const XMLCh* getContentType() const override
        { return &emptyString; }
};

/************************************************************************/
/*                      OGRXercesNetAccessor                            */
/************************************************************************/

class OGRXercesNetAccessor final: public XMLNetAccessor
{
public :
    OGRXercesNetAccessor() = default;

    BinInputStream* makeNew(const XMLURL&  urlSource, const XMLNetHTTPInfo* httpInfo) override;
    const XMLCh* getId() const override { return fgMyName; }

private :
    static const XMLCh fgMyName[];

    OGRXercesNetAccessor(const OGRXercesNetAccessor&);
    OGRXercesNetAccessor& operator=(const OGRXercesNetAccessor&);
};


const XMLCh OGRXercesNetAccessor::fgMyName[] = {
    chLatin_O, chLatin_G, chLatin_R,
    chLatin_X, chLatin_e, chLatin_r, chLatin_c, chLatin_e, chLatin_s,
    chLatin_N, chLatin_e, chLatin_t,
    chLatin_A, chLatin_c, chLatin_c, chLatin_e, chLatin_s,
    chLatin_s, chLatin_o, chLatin_r,
    chNull
};

BinInputStream* OGRXercesNetAccessor::makeNew(const XMLURL& urlSource,
                                              const XMLNetHTTPInfo* /*httpInfo*/)
{
    const std::string osURL = "/vsicurl_streaming/" + transcode(urlSource.getURLText());
    VSILFILE* fp = VSIFOpenL(osURL.c_str(), "rb");
    if( !fp )
        return nullptr;
    return new OGRXercesBinInputStream(fp, true);
}

/************************************************************************/
/*                        OGRInitializeXerces()                         */
/************************************************************************/

bool OGRInitializeXerces()
{
    CPLMutexHolderD(&hMutex);

    if( nCounter > 0 )
    {
        nCounter++;
        return true;
    }

    if( XMLPlatformUtils::fgMemoryManager != nullptr )
    {
        CPLDebug("OGR", "Xerces-C already initialized before GDAL");
        bXercesWasAlreadyInitializedBeforeUs = true;
        nCounter = 1;
        return true;
    }
    else
    {
        try
        {
            CPLDebug("OGR", "XMLPlatformUtils::Initialize()");
            XMLPlatformUtils::Initialize();

            // Install our own network accessor instead of the default Xerces-C one
            // This enables us in particular to honour GDAL_HTTP_TIMEOUT
            if( CPLTestBool(CPLGetConfigOption("OGR_XERCES_USE_OGR_NET_ACCESSOR", "YES")) )
            {
                auto oldNetAccessor = XMLPlatformUtils::fgNetAccessor;
                XMLPlatformUtils::fgNetAccessor = new OGRXercesNetAccessor();
                delete oldNetAccessor;
            }

            nCounter = 1;
            return true;
        }
        catch (const XMLException& toCatch)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Exception initializing Xerces: %s",
                      transcode(toCatch.getMessage()).c_str() );
            return false;
        }
    }
}

/************************************************************************/
/*                       OGRDeinitializeXerces()                        */
/************************************************************************/

void OGRDeinitializeXerces()
{
    CPLMutexHolderD(&hMutex);
    if( nCounter == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unpaired OGRInitializeXerces / OGRDeinitializeXerces calls");
        return;
    }
    nCounter--;
    if( nCounter == 0 )
    {
        if( !bXercesWasAlreadyInitializedBeforeUs &&
            CPLTestBool(CPLGetConfigOption("OGR_XERCES_TERMINATE", "YES")) )
        {
            CPLDebug("OGR", "XMLPlatformUtils::Terminate()");
            XMLPlatformUtils::Terminate();
        }
    }
}

/************************************************************************/
/*                       OGRCleanupXercesMutex()                        */
/************************************************************************/

void OGRCleanupXercesMutex()
{
    if( hMutex != nullptr )
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
}

namespace OGR
{

/************************************************************************/
/*                            transcode()                               */
/************************************************************************/

CPLString transcode( const XMLCh *panXMLString, int nLimitingChars )
{
    CPLString osRet;
    transcode( panXMLString, osRet, nLimitingChars );
    return osRet;
}

CPLString& transcode( const XMLCh *panXMLString, CPLString& osRet,
                      int nLimitingChars )
{
    if( panXMLString == nullptr )
    {
        osRet = "(null)";
        return osRet;
    }

    osRet.clear();
    if( nLimitingChars > 0 )
        osRet.reserve(nLimitingChars);

    bool bSimpleASCII = true;
    int nChars = 0;
    for( int i = 0; panXMLString[i] != 0 &&
           (nLimitingChars < 0 || i < nLimitingChars); i++ )
    {
        if( panXMLString[i] > 127 )
        {
            bSimpleASCII = false;
        }
        osRet += static_cast<char>(panXMLString[i]);
        nChars++;
    }

    if( bSimpleASCII )
        return osRet;

/* -------------------------------------------------------------------- */
/*      The simple translation was wrong, because the source is not     */
/*      all simple ASCII characters.  Redo using the more expensive     */
/*      recoding API.                                                   */
/* -------------------------------------------------------------------- */
    wchar_t *pwszSource =
        static_cast<wchar_t *>(CPLMalloc(sizeof(wchar_t) * (nChars+1) ));
    for( int i = 0 ; i < nChars; i++ )
        pwszSource[i] = panXMLString[i];
    pwszSource[nChars] = 0;

    char *pszResult = CPLRecodeFromWChar( pwszSource,
                                          "WCHAR_T", CPL_ENC_UTF8 );

    osRet = pszResult;

    CPLFree( pwszSource );
    CPLFree( pszResult );

    return osRet;
}

}


/************************************************************************/
/*                       OGRXercesInputSource                           */
/************************************************************************/

class OGRXercesInputSource : public InputSource
{
    OGRXercesBinInputStream* pBinInputStream;

    CPL_DISALLOW_COPY_ASSIGN(OGRXercesInputSource)

  public:
    explicit OGRXercesInputSource(VSILFILE* fp,
                         MemoryManager* const manager =
                         XMLPlatformUtils::fgMemoryManager);
    ~OGRXercesInputSource() override;

    BinInputStream* makeStream() const override
        { return pBinInputStream; }
};

/************************************************************************/
/*                      OGRXercesBinInputStream()                       */
/************************************************************************/

OGRXercesBinInputStream::OGRXercesBinInputStream(VSILFILE *fpIn, bool bOwnFPIn) :
    fp(fpIn),
    bOwnFP(bOwnFPIn)
{}

/************************************************************************/
/*                     ~OGRXercesBinInputStream()                       */
/************************************************************************/

OGRXercesBinInputStream::~OGRXercesBinInputStream()
{
    if( bOwnFP )
        VSIFCloseL(fp);
}

/************************************************************************/
/*                              curPos()                                */
/************************************************************************/

XMLFilePos OGRXercesBinInputStream::curPos() const
{
    return static_cast<XMLFilePos>(VSIFTellL(fp));
}

/************************************************************************/
/*                            readBytes()                               */
/************************************************************************/

XMLSize_t OGRXercesBinInputStream::readBytes(XMLByte* const toFill,
                                             const XMLSize_t maxToRead)
{
    return static_cast<XMLSize_t>(VSIFReadL(toFill, 1, maxToRead, fp));
}

/************************************************************************/
/*                       OGRXercesInputSource()                         */
/************************************************************************/

OGRXercesInputSource::OGRXercesInputSource(VSILFILE *fp,
                                           MemoryManager *const manager) :
    InputSource(manager),
    pBinInputStream(new OGRXercesBinInputStream(fp, false))
{}

/************************************************************************/
/*                      ~OGRXercesInputSource()                         */
/************************************************************************/

OGRXercesInputSource::~OGRXercesInputSource() = default;

/************************************************************************/
/*                     OGRCreateXercesInputSource()                     */
/************************************************************************/

InputSource* OGRCreateXercesInputSource(VSILFILE* fp)
{
    return new OGRXercesInputSource(fp);
}

/************************************************************************/
/*                     OGRDestroyXercesInputSource()                    */
/************************************************************************/

void OGRDestroyXercesInputSource(InputSource* is)
{
    delete is;
}

#endif // HAVE_XERCES
