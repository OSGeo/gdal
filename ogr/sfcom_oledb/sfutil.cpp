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
 * Revision 1.3  1999/06/22 16:59:55  kshih
 * Modified GetFilenames to return previous filename if NULL is provided.
 *
 * Revision 1.2  1999/06/22 16:17:11  warmerda
 * added ogrcomdebug
 *
 * Revision 1.1  1999/06/22 15:53:54  kshih
 * Utility functions.
 *
 *
 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "cpl_conv.h"
#include "shapefil.h"


static char szSFile[MAX_PATH] ="";
static char szDFile[MAX_PATH] ="";

/************************************************************************/
/*                           SFGetFilenames()                           */
/*                                                                      */
/*      Get the two shape and dbf filenames.                            */
/************************************************************************/
void	SFGetFilenames(const char *pszFileIn, char **ppszSHPFile, 
					   char **ppszDBFFile)
{

	char szDrive[_MAX_DRIVE];   
	char szDir[_MAX_DIR];
  	char szFname[_MAX_FNAME];   
	char szExt[_MAX_EXT];


	if (pszFileIn == NULL)
	{
		if (ppszSHPFile)
			*ppszSHPFile = szSFile;

		if (ppszDBFFile)
			*ppszDBFFile = szDFile;
		return;
	}
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
/*      Get a shape file handle from any related name.			*/
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
/*      Get a DBFFile handle from any related name.			*/
/************************************************************************/
DBFHandle	SFGetDBFHandle(const char *pszName)
{
    char *pszDBFFile;

        SFGetFilenames(pszName,NULL,&pszDBFFile);

	return DBFOpen(pszDBFFile,"r");
}

#ifdef NOTDEF
/************************************************************************/
/*                         GetInitDataSource()                          */
/*  Returns char * to allocated string.  Caller responsible for freeing */
/************************************************************************/
static char *GetInitDataSource(CSFRowset *pRowset)
{
	IRowsetInfo *pRInfo;
	HRESULT		hr;
	char		*pszDataSource = NULL;

// Temporary hack to allow Frank to continue.
	return GetHackedInitDataSource();

   OGRComDebug( "Info", "In GetInitDataSource\n" );

	hr = pRowset->QueryInterface(IID_IRowsetInfo,(void **) &pRInfo);
	if (SUCCEEDED(hr))
	{
        OGRComDebug( "Info", "Got IRowsetInfo\n" );
		IGetDataSource	*pIGetDataSource;
		hr = pRInfo->GetSpecification(IID_IGetDataSource, (IUnknown **) &pIGetDataSource);
		pRInfo->Release();

		if (SUCCEEDED(hr))
		{
			IDBProperties *pIDBProp;

            OGRComDebug( "Info", "Got IGetDataSource\n" );
			hr = pIGetDataSource->GetDataSource(IID_IDBProperties, (IUnknown **) &pIDBProp);
			pIGetDataSource->Release();

			if (SUCCEEDED(hr))
			{
				DBPROPIDSET sPropIdSets[1];
				DBPROPID	rgPropIds[1];

				ULONG		nPropSets;
				DBPROPSET	*rgPropSets;

                                OGRComDebug( "Info", "Got Properties\n" );
				rgPropIds[0] = DBPROP_INIT_DATASOURCE;

				sPropIdSets[0].cPropertyIDs = 1;
				sPropIdSets[0].guidPropertySet = DBPROPSET_DBINIT;
				sPropIdSets[0].rgPropertyIDs = rgPropIds;

				pIDBProp->GetProperties(1,sPropIdSets,&nPropSets,&rgPropSets);

				if (rgPropSets)
				{
					USES_CONVERSION;
					char *pszSource = (char *)  OLE2A(rgPropSets[0].rgProperties[0].vValue.bstrVal);
					pszDataSource = (char *) malloc(1+strlen(pszSource));
					strcpy(pszDataSource,pszSource);
                                        OGRComDebug( "Info", 
                                                     "Got rgPropSets\n" );
				}

				if (rgPropSets)
				{
					int i;
					for (i=0; i < nPropSets; i++)
					{
						CoTaskMemFree(rgPropSets[i].rgProperties);
					}
					CoTaskMemFree(rgPropSets);
				}

				pIDBProp->Release();
			}
		}
	}

	return pszDataSource;
}
#endif
/************************************************************************/
/*                            OGRComDebug()                             */
/************************************************************************/

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... )

{
    va_list args;
    static FILE      *fpDebug = NULL;

/* -------------------------------------------------------------------- */
/*      Do we have a debug file?                                        */
/* -------------------------------------------------------------------- */
    if( fpDebug == NULL )
    {
        fpDebug = fopen( "f:\\gdal\\ogr\\sfcom_oledb\\Debug", "w" );
    }

/* -------------------------------------------------------------------- */
/*      Write message to stdout.                                        */
/* -------------------------------------------------------------------- */
    fprintf( stdout, "%s:", pszDebugClass );

    va_start(args, pszFormat);
    vfprintf( stdout, pszFormat, args );
    va_end(args);

    fflush( stdout );

/* -------------------------------------------------------------------- */
/*      Write message to debug file.                                    */
/* -------------------------------------------------------------------- */
    if( fpDebug != NULL )
    {
        fprintf( fpDebug, "%s:", pszDebugClass );

        va_start(args, pszFormat);
        vfprintf( fpDebug, pszFormat, args );
        va_end(args);

        fflush( fpDebug );
    }
}

