/* This file is part of the iom project.
 * For more information, please see <http://www.interlis.ch>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/** @file
 * implementation of utility functions
 * @defgroup utilities utility functions
 * @{
 */

#include <iostream>
#include <string>
#include <string.h>

#include <iom/iom_p.h>




#include <sys/types.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#define stat _stat
#else
#include <unistd.h>
#endif

#ifndef _MSC_VER

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

/** transcode a xerces unicode string to an utf8 encoded one.
*/
char *iom_toUTF8(const XMLCh *src)
{
	unsigned int srcLen=XMLString::stringLen(src);
	int destLen=srcLen+10;
	char *dest;
	dest=dbgnew char[destLen+1];
	unsigned int eaten;
	unsigned int endDest;
	endDest=utf8_transcoder->transcodeTo(src,srcLen,(unsigned char *)dest,destLen,eaten,XMLTranscoder::UnRep_RepChar);
	while(eaten<srcLen){
		delete[] dest;
		destLen=destLen+srcLen-eaten+10;
		dest=dbgnew char[destLen+1];
		endDest=utf8_transcoder->transcodeTo(src,srcLen,(unsigned char *)dest,destLen,eaten,XMLTranscoder::UnRep_RepChar);
	}
	dest[endDest]=0;
	return dest;
}

/** transcode an utf8 encoded string to a xerces unicode one.
*/
XMLCh *iom_fromUTF8(const char *src)
{
	int srcLen=XMLString::stringLen(src);
	int destLen=srcLen;
	XMLCh *dest=dbgnew XMLCh[destLen+1];
	unsigned char *charSizes=dbgnew unsigned char[destLen];
	unsigned int eaten;
	unsigned int endDest=utf8_transcoder->transcodeFrom((unsigned char *)src,srcLen,dest,destLen,eaten,charSizes);
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
