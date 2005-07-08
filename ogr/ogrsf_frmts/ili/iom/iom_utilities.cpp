/** @file
 * implementation of utility functions
 * @defgroup utilities utility functions
 * @{
 */

#include <iostream>
#include <string>
#include <string.h>

#include <iom/iom_p.h>


#ifdef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#define stat _stat
#else
#include <sys/stat.h>
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


/** Perform iom library initialization.
 */
extern "C" void iom_init()
{
	try {
           XMLPlatformUtils::Initialize();
    }
    catch (const XMLException& toCatch) {
        char* message = XMLString::transcode(toCatch.getMessage());
		iom_issueerr(message);
        XMLString::release(&message);
        return;
    }
}

/** Perform iom library termination.
 */
extern "C" void iom_end()
{

	ParserHandler::at_iom_end();
	ErrorUtility::at_iom_end();
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

/**
 * @}
 */
