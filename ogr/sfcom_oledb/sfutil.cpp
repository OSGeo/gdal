/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility functions.
 * Author:   Ken Shih, kshih@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/06/22 15:53:54  kshih
 * Utility functions.
 *
 *
 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "shapefil.h"



/************************************************************************/
/*                          SFGetFilenames()                            */
/*                                                                      */
/*      Get the two shape and dbf filenames.							*/
/************************************************************************/
void	SFGetFilenames(const char *pszFileIn, char **ppszSHPFile, 
					   char **ppszDBFFile)
{
	static char szSFile[MAX_PATH];
	static char szDFile[MAX_PATH];

	char szDrive[_MAX_DRIVE];   
	char szDir[_MAX_DIR];
  	char szFname[_MAX_FNAME];   
	char szExt[_MAX_EXT];

	_splitpath(pszFileIn,szDrive,szDir,szFname,szExt);
	

	// Shape file

	strcpy(szExt,".SHP");
	if (strlen(szExt))
	{
		if (islower(szExt[0]))
		{
			strcpy(szExt,".shp");
		}

	}
	else if (strlen(szFname))
	{
		if (islower(szFname[strlen(szFname)-1]))
		{
			strcpy(szExt,".shp");
		}
	}

	_makepath(szSFile,szDrive,szDir,szFname,szExt);
	if (ppszSHPFile)
		*ppszSHPFile = szSFile;
	



	// DBFFile
	strcpy(szExt,".DBF");
	if (strlen(szExt))
	{
		if (islower(szExt[1]))
		{
			strcpy(szExt,".dbf");
		}

	}
	else if (strlen(szFname))
	{
		if (islower(szFname[strlen(szFname)-1]))
		{
			strcpy(szExt,".dbf");
		}
	}

	_makepath(szDFile,szDrive,szDir,szFname,szExt);

	if (ppszDBFFile)
		*ppszDBFFile = szDFile;
}

/************************************************************************/
/*                          SFGetSHPHandle()                            */
/*                                                                      */
/*      Get a shape file handle from any related name.					*/
/************************************************************************/
SHPHandle	SFGetSHPHandle(const char *pszName)
{
	char *pszSHPFile;

	SFGetFilenames(pszName,&pszSHPFile, NULL);

	return SHPOpen(pszSHPFile,"r");
}
/************************************************************************/
/*                          SFGetDBFHandle()                            */
/*                                                                      */
/*      Get a DBFFile handle from any related name.						*/
/************************************************************************/
DBFHandle	SFGetDBFHandle(const char *pszName)
{
	char *pszDBFFile;

	SFGetFilenames(pszName,&pszDBFFile, NULL);

	return DBFOpen(pszDBFFile,"r");
}