/**********************************************************************
 * $Id$
 *
 * Project:  iom - The INTERLIS Object Model
 * Purpose:  For more information, please see <http://iom.sourceforge.net>
 * Author:   Claude Eisenhut
 *
 **********************************************************************
 * Copyright (c) 2007, Claude Eisenhut
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

/** @file
 * implementation of utility functions
 * @defgroup utilities utility functions
 * @{
 */

#include <cstdio>
#include <iostream>
#include <string>
#include <string.h>

#include <iom/iom_p.h>




#include <sys/types.h>
#include <sys/stat.h>

#if defined(__MSVCRT__) || defined(_MSC_VER)
#define stat _stat
#else
#include <unistd.h>
#endif

#if !defined(__MSVCRT__) && !defined(_MSC_VER)

#include <stdlib.h>
#include <string.h>

#define PATHSEP ':'

/*
  _searchenv is a function provided by the MSVC library that finds
  files which may be anywhere along a path which appears in an
  environment variable.
*/

static void _searchenv(const char *name, const char *envname, char *hitfile)
{

	// Default failure indication
	*hitfile = '\0';

	// If the given name is absolute, then don't search the
	//   path, but use it as is.
	if(
#ifdef WIN32
		strchr(name, ':') != NULL || // Contain a drive spec? 
		name[0] == '\\' ||		// Start with absolute ref? 
#endif
		name[0] == '/')			// Start with absolute ref?
	{
		// Copy to target
		strcpy(hitfile, name);	
		return;
	}

	char *cp;
	cp = getenv(envname);
	if(cp == NULL){
		// Variable not defined, no search.
		return;	
	}

	while(*cp)
	{
		*hitfile = '\0';
		char *concat;
		concat=hitfile;
		// skip PATHSEP (and empty entries)
		while(*cp && *cp==PATHSEP){
			cp++;
		}
		// copy path
		while(*cp && *cp!=PATHSEP){
			*concat=*cp;
			cp++;
			concat++;
		}
		// end of variable value reached?
		if(concat==hitfile){
			// file not found
			*hitfile = '\0';
			return;
		}
		// does no trailing '/' exists?
		if(*(concat-1) != '/' && *(concat-1) != '\\'){
			// append it
			*concat='/';
			concat++;
		}
		// append file name
		strcpy(concat, name);
		// does file exist?
		if(iom_fileexists(hitfile))
		{
			// file found
			return;
		}
	}
	// file not found
	*hitfile = '\0';
}

#endif



static char *tmpdir=0;
static XMLTranscoder *utf8_transcoder=0;

/** Perform iom library initialization.
 */
extern "C" void iom_init()
{
	//__asm { int 3 };

	try {
           XMLPlatformUtils::Initialize();
    }
    catch (const XMLException& toCatch) {
        char* message = XMLString::transcode(toCatch.getMessage());
		iom_issueerr(message);
        XMLString::release(&message);
        return;
    }
    XMLTransService::Codes resCode;
    utf8_transcoder = XMLPlatformUtils::fgTransService->makeNewTranscoderFor
    (
        "UTF-8"
        , resCode
        , 16 * 1024
    );
	tags::clear();

}

/* Compatibility stuff due to change of Xerces-C API signature (#2616) */
#if XERCES_VERSION_MAJOR >= 3
#define LEN_SIZE_TYPE XMLSize_t
#else
#define LEN_SIZE_TYPE unsigned int
#endif

/** transcode a xerces unicode string to an utf8 encoded one.
*/
char *iom_toUTF8(const XMLCh *src)
{
	LEN_SIZE_TYPE srcLen=XMLString::stringLen(src);
	LEN_SIZE_TYPE destLen=srcLen+10;
	XMLByte *dest;
	dest=dbgnew XMLByte[destLen+1];
	LEN_SIZE_TYPE eaten;
	LEN_SIZE_TYPE endDest;
	endDest=utf8_transcoder->transcodeTo(src,srcLen,dest,destLen,eaten,XMLTranscoder::UnRep_RepChar);
	while(eaten<srcLen){
		delete[] dest;
		destLen=destLen+srcLen-eaten+10;
		dest=dbgnew XMLByte[destLen+1];
		endDest=utf8_transcoder->transcodeTo(src,srcLen,dest,destLen,eaten,XMLTranscoder::UnRep_RepChar);
	}
	dest[endDest]=0;
	return (char*)dest; /* should be a unsigned char* == XMLByte* instead */
}

/** transcode an utf8 encoded string to a xerces unicode one.
*/
XMLCh *iom_fromUTF8(const char *src)
{
	LEN_SIZE_TYPE srcLen=XMLString::stringLen(src);
	LEN_SIZE_TYPE destLen=srcLen;
	XMLCh *dest=dbgnew XMLCh[destLen+1];
	unsigned char *charSizes=dbgnew unsigned char[destLen];
	LEN_SIZE_TYPE eaten;
	LEN_SIZE_TYPE endDest=utf8_transcoder->transcodeFrom((const XMLByte *)src,srcLen,dest,destLen,eaten,charSizes);
	dest[endDest]=0;
	delete[] charSizes;
	return dest;
}

/** Perform iom library termination.
 */
extern "C" void iom_end()
{

	ParserHandler::at_iom_end();
	ErrorUtility::at_iom_end();
	tags::clear();
	XMLPlatformUtils::Terminate();
}

/** sets the directory where iom writes temporary files.
 */
extern "C" void iom_settmpdir(const char *dirname)
{
	if(tmpdir)free(tmpdir);
	tmpdir=strdup(dirname);
}

/** creates a temporary filename.
 * The caller should free the returned buffer.
 */
extern "C" char *iom_gettmpnam()
{
#ifdef _MSC_VER
	return _tempnam(tmpdir,"iom");
#else
	return tempnam(tmpdir,"iom");
#endif
}

/** Searches for a file in
 *  a directory specified by an environment variable.
 * Returns 0 if file not found.
 * Returns a pointer to a static buffer.
 */
extern "C" char *iom_searchenv(const char *filename, const char *varname)
{
	static char pathbuffer[IOM_PATH_MAX];
	_searchenv( filename, varname, pathbuffer );
	if( *pathbuffer != '\0' ){
		return pathbuffer;
	}
	// file not found
	return 0;
}

/** Tests if a file exists.
 * Returns !0 if file exists.
 */
extern "C" int iom_fileexists(const char *filename)
{
	struct stat info;
	// does file exist?
	if(!stat(filename, &info))
	{
		// file found
		return true;
	}
	return false;
}

/** Returns the current time in milliseconds.
 */
extern "C" unsigned long iom_currentmilis()
{
	return XMLPlatformUtils::getCurrentMillis();
}

/**
 * @}
 */
