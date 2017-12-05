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

static CPLMutex* hMutex = NULL;
static int nCounter = 0;

/************************************************************************/
/*                        OGRInitializeXerces()                         */
/************************************************************************/

bool OGRInitializeXerces(void)
{
    CPLMutexHolderD(&hMutex);
    if( nCounter > 0 )
    {
        nCounter++;
        return true;
    }

    try
    {
        CPLDebug("OGR", "XMLPlatformUtils::Initialize()");
        XMLPlatformUtils::Initialize();
        nCounter ++;
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

/************************************************************************/
/*                       OGRDeinitializeXerces()                        */
/************************************************************************/

void OGRDeinitializeXerces(void)
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
        if( CPLTestBool(CPLGetConfigOption("OGR_XERCES_TERMINATE", "YES")) )
        {
            CPLDebug("OGR", "XMLPlatformUtils::Terminate()");
            XMLPlatformUtils::Terminate();
        }
    }
}

/************************************************************************/
/*                       OGRCleanupXercesMutex()                        */
/************************************************************************/

void OGRCleanupXercesMutex(void)
{
    if( hMutex != NULL )
        CPLDestroyMutex(hMutex);
    hMutex = NULL;
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
    if( panXMLString == NULL )
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



#define WORKAROUND_XERCESC_2094

/************************************************************************/
/*                      OGRXercesBinInputStream                         */
/************************************************************************/
class OGRXercesBinInputStream : public BinInputStream
{
    VSILFILE* fp;
    XMLCh emptyString;
#ifdef WORKAROUND_XERCESC_2094
    bool bFirstCallToReadBytes;
#endif

public :

    explicit OGRXercesBinInputStream(VSILFILE* fp);
    virtual ~OGRXercesBinInputStream();

    virtual XMLFilePos curPos() const override;
    virtual XMLSize_t readBytes(XMLByte* const toFill,
                                const XMLSize_t maxToRead) override;
    virtual const XMLCh* getContentType() const override
        { return &emptyString; }
};

/************************************************************************/
/*                       OGRXercesInputSource                           */
/************************************************************************/

class OGRXercesInputSource : public InputSource
{
    OGRXercesBinInputStream* pBinInputStream;

public:
             OGRXercesInputSource(VSILFILE* fp,
                            MemoryManager* const manager =
                                XMLPlatformUtils::fgMemoryManager);
    virtual ~OGRXercesInputSource();

    virtual BinInputStream* makeStream() const override
        { return pBinInputStream; }
};

/************************************************************************/
/*                      OGRXercesBinInputStream()                       */
/************************************************************************/

OGRXercesBinInputStream::OGRXercesBinInputStream(VSILFILE *fpIn) :
    fp(fpIn),
    emptyString(0)
#ifdef WORKAROUND_XERCESC_2094
    ,bFirstCallToReadBytes(true)
#endif
{}

/************************************************************************/
/*                     ~OGRXercesBinInputStream()                       */
/************************************************************************/

OGRXercesBinInputStream::~OGRXercesBinInputStream() {}

/************************************************************************/
/*                              curPos()                                */
/************************************************************************/

XMLFilePos OGRXercesBinInputStream::curPos() const
{
    return (XMLFilePos)VSIFTellL(fp);
}

/************************************************************************/
/*                            readBytes()                               */
/************************************************************************/

XMLSize_t OGRXercesBinInputStream::readBytes(XMLByte* const toFill,
                                             const XMLSize_t maxToRead)
{
    XMLSize_t nRead = (XMLSize_t)VSIFReadL(toFill, 1, maxToRead, fp);
#ifdef WORKAROUND_XERCESC_2094
    if( bFirstCallToReadBytes && nRead > 10 )
    {
        // Workaround leak in Xerces-C when parsing an invalid encoding
        // attribute and there are newline or tab characters between <?xml and
        // version="1.0". So replace those newlines by equivalent spaces....
        // See https://issues.apache.org/jira/browse/XERCESC-2094
        XMLSize_t nToSkip = 0;
        if( memcmp(toFill, "<?xml", 5) == 0 )
            nToSkip = 5;
        else if( memcmp(toFill, "\xEF\xBB\xBF<?xml", 8) == 0 )
            nToSkip = 8;
        if( nToSkip > 0 )
        {
            for( XMLSize_t i = nToSkip; i < nRead; i++ )
            {
                if( toFill[i] == 0xD || toFill[i] == 0xA || toFill[i] == 0x9 )
                    toFill[i] = ' ';
                else
                    break;
            }
        }
        bFirstCallToReadBytes = false;
    }
#endif
    return nRead;
}

/************************************************************************/
/*                       OGRXercesInputSource()                         */
/************************************************************************/

OGRXercesInputSource::OGRXercesInputSource(VSILFILE *fp,
                                           MemoryManager *const manager) :
    InputSource(manager),
    pBinInputStream(new OGRXercesBinInputStream(fp))
{}

/************************************************************************/
/*                      ~OGRXercesInputSource()                         */
/************************************************************************/

OGRXercesInputSource::~OGRXercesInputSource() {}

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
