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

#include "ogr_xerces.h"

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"

#include <algorithm>
#include <limits>
#include <map>

CPL_CVSID("$Id$")

#ifdef HAVE_XERCES

class OGRXercesStandardMemoryManager;
class OGRXercesInstrumentedMemoryManager;

/************************************************************************/
/*                        CPLGettimeofday()                             */
/************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__)
#  include <sys/timeb.h>

namespace {
struct CPLTimeVal
{
  time_t  tv_sec;         /* seconds */
  long    tv_usec;        /* and microseconds */
};
} // namespace

static int CPLGettimeofday(struct CPLTimeVal* tp, void* /* timezonep*/ )
{
  struct _timeb theTime;

  _ftime(&theTime);
  tp->tv_sec = static_cast<time_t>(theTime.time);
  tp->tv_usec = theTime.millitm * 1000;
  return 0;
}
#else
#  include <sys/time.h>     /* for gettimeofday() */
#  define  CPLTimeVal timeval
#  define  CPLGettimeofday(t,u) gettimeofday(t,u)
#endif

namespace {
struct LimitationStruct
{
    size_t      maxMemAlloc = 0;
    std::string osMsgMaxMemAlloc{};
    double      timeOut = 0;
    std::string osMsgTimeout{};

    CPLTimeVal initTV{0,0};
    CPLTimeVal lastTV{0,0};
    size_t     totalAllocSize = 0;
    size_t     allocCount = 0;
};
} // namespace

static CPLMutex* hMutex = nullptr;
static int nCounter = 0;
static bool bXercesWasAlreadyInitializedBeforeUs = false;
static OGRXercesStandardMemoryManager* gpExceptionMemoryManager = nullptr;
static OGRXercesInstrumentedMemoryManager* gpMemoryManager = nullptr;
static std::map<GIntBig, LimitationStruct>* gpoMapThreadTimeout = nullptr;

/************************************************************************/
/*                    OGRXercesStandardMemoryManager                    */
/************************************************************************/

class OGRXercesStandardMemoryManager final: public MemoryManager
{
public:
    OGRXercesStandardMemoryManager() = default;

    MemoryManager* getExceptionMemoryManager() override { return this; }

    void* allocate(XMLSize_t size) override;

    void deallocate(void* p) override;
};

void* OGRXercesStandardMemoryManager::allocate(XMLSize_t size)
{
    void* memptr = VSIMalloc(size);
    if(memptr == nullptr && size != 0)
        throw OutOfMemoryException();
    return memptr;
}

void OGRXercesStandardMemoryManager::deallocate(void* p)
{
    if( p )
        VSIFree(p);
}

/************************************************************************/
/*               OGRXercesInstrumentedMemoryManager                     */
/************************************************************************/

class OGRXercesInstrumentedMemoryManager final: public MemoryManager
{
public:
    OGRXercesInstrumentedMemoryManager() = default;

    MemoryManager* getExceptionMemoryManager() override { return gpExceptionMemoryManager; }

    void* allocate(XMLSize_t size) override;

    void deallocate(void* p) override;
};

void* OGRXercesInstrumentedMemoryManager::allocate(XMLSize_t size)
{
    if( size > std::numeric_limits<size_t>::max() - 8U )
        throw OutOfMemoryException();
    void* memptr = VSIMalloc(size + 8);
    if( memptr == nullptr )
        throw OutOfMemoryException();
    memcpy(memptr, &size, sizeof(XMLSize_t));

    LimitationStruct* pLimitation = nullptr;
    {
        CPLMutexHolderD(&hMutex);

        if( gpoMapThreadTimeout )
        {
            auto iter = gpoMapThreadTimeout->find(CPLGetPID());
            if( iter != gpoMapThreadTimeout->end() )
            {
                pLimitation = &(iter->second);
            }
        }
    }

    // Big memory allocation can happen in cases like
    // https://issues.apache.org/jira/browse/XERCESC-1051
    if( pLimitation && pLimitation->maxMemAlloc > 0 )
    {
        pLimitation->totalAllocSize += size;

        if( pLimitation->totalAllocSize > pLimitation->maxMemAlloc )
        {
            pLimitation->maxMemAlloc = 0;
            VSIFree(memptr);
            if( !pLimitation->osMsgMaxMemAlloc.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s", pLimitation->osMsgMaxMemAlloc.c_str());
            }
            throw OutOfMemoryException();
        }
    }

    // Quite a hack, but some pathologic schema can cause excessive
    // processing time. As memory allocations are regularly done, we
    // measure the time of those consecutive allocations and check it
    // does not exceed a threshold set by OGRStartXercesTimeoutForThisThread()
    // Can happen in cases like
    // https://issues.apache.org/jira/browse/XERCESC-1051
    if( pLimitation && pLimitation->timeOut > 0 )
    {
        ++ pLimitation->allocCount;
        if( pLimitation->allocCount == 1000 )
        {
            pLimitation->allocCount = 0;

            CPLTimeVal tv;
            CPLGettimeofday(&tv, nullptr);
            if( pLimitation->initTV.tv_sec == 0 ||
                // Reset the counter if the delay between the last 1000 memory
                // allocations is too large. This enables being tolerant to
                // network requests.
                tv.tv_sec + tv.tv_usec * 1e-6 -
                    (pLimitation->lastTV.tv_sec + pLimitation->lastTV.tv_usec * 1e-6) >
                        std::min(0.1, pLimitation->timeOut / 10))
            {
                pLimitation->initTV = tv;
            }
            else if( tv.tv_sec + tv.tv_usec * 1e-6 -
                    (pLimitation->initTV.tv_sec + pLimitation->initTV.tv_usec * 1e-6) > pLimitation->timeOut )
            {
                pLimitation->timeOut = 0;
                VSIFree(memptr);
                if( !pLimitation->osMsgTimeout.empty() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s", pLimitation->osMsgTimeout.c_str());
                }
                throw OutOfMemoryException();
            }
            pLimitation->lastTV = tv;
        }
    }

    return static_cast<char*>(memptr) + 8;
}

void OGRXercesInstrumentedMemoryManager::deallocate(void* p)
{
    if( p )
    {
        void* rawptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p) - 8);
        XMLSize_t size;
        memcpy(&size, rawptr, sizeof(XMLSize_t));
        VSIFree(rawptr);

        LimitationStruct* pLimitation = nullptr;
        {
            CPLMutexHolderD(&hMutex);

            if( gpoMapThreadTimeout )
            {
                auto iter = gpoMapThreadTimeout->find(CPLGetPID());
                if( iter != gpoMapThreadTimeout->end() )
                {
                    pLimitation = &(iter->second);
                }
            }
        }
        if( pLimitation && pLimitation->maxMemAlloc > 0 )
        {
            pLimitation->totalAllocSize -= size;
        }
    }
}

/************************************************************************/
/*                  OGRStartXercesLimitsForThisThread()                 */
/************************************************************************/

void OGRStartXercesLimitsForThisThread(size_t nMaxMemAlloc,
                                       const char* pszMsgMaxMemAlloc,
                                       double dfTimeoutSecond,
                                       const char* pszMsgTimeout)
{
    CPLMutexHolderD(&hMutex);
    if( gpoMapThreadTimeout == nullptr )
    {
        gpoMapThreadTimeout = new std::map<GIntBig, LimitationStruct>();
    }
    LimitationStruct limitation;
    limitation.maxMemAlloc = nMaxMemAlloc;
    if( pszMsgMaxMemAlloc )
        limitation.osMsgMaxMemAlloc = pszMsgMaxMemAlloc;
    limitation.timeOut = dfTimeoutSecond;
    if( pszMsgTimeout )
        limitation.osMsgTimeout = pszMsgTimeout;
    (*gpoMapThreadTimeout)[CPLGetPID()] = limitation;
}

/************************************************************************/
/*                  OGRStopXercesLimitsForThisThread()                  */
/************************************************************************/

void OGRStopXercesLimitsForThisThread()
{
    CPLMutexHolderD(&hMutex);
    (*gpoMapThreadTimeout).erase(CPLGetPID());
    if( gpoMapThreadTimeout->empty() )
    {
        delete gpoMapThreadTimeout;
        gpoMapThreadTimeout = nullptr;
    }
}

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
        gpExceptionMemoryManager = new OGRXercesStandardMemoryManager();
        gpMemoryManager = new OGRXercesInstrumentedMemoryManager();

        try
        {
            CPLDebug("OGR", "XMLPlatformUtils::Initialize()");
            XMLPlatformUtils::Initialize(XMLUni::fgXercescDefaultLocale,
                                         nullptr, /* nlsHome */
                                         nullptr, /* panicHandler */
                                         gpMemoryManager);

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

            delete gpMemoryManager;
            gpMemoryManager = nullptr;
            delete gpExceptionMemoryManager;
            gpExceptionMemoryManager = nullptr;
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
