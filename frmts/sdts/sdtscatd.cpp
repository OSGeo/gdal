/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTS_CATD and SDTS_CATDEntry classes for
 *           reading CATD files.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.3  1999/05/07 13:45:01  warmerda
 * major upgrade to use iso8211lib
 *
 * Revision 1.2  1999/03/23 16:01:16  warmerda
 * added storage of type and fetch methods
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"


/************************************************************************/
/* ==================================================================== */
/*			      SDTS_CATDEntry				*/
/*									*/
/*	This class is for internal use of the SDTS_CATD class only,	*/
/*	and represents one entry in the directory ... a reference	*/
/*	to another module file. 					*/
/* ==================================================================== */
/************************************************************************/

class SDTS_CATDEntry

{
  public:
    char *	pszModule;
    char *	pszType;
    char *	pszFile;
    char *	pszExternalFlag;

    char *	pszFullPath;
};

/************************************************************************/
/* ==================================================================== */
/*			       SDTS_CATD				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             SDTS_CATD()                              */
/************************************************************************/

SDTS_CATD::SDTS_CATD()

{
    nEntries = 0;
    papoEntries = NULL;
}

/************************************************************************/
/*                             ~SDTS_CATD()                             */
/************************************************************************/

SDTS_CATD::~SDTS_CATD()
{
    int		i;

    for( i = 0; i < nEntries; i++ )
        delete papoEntries[i];

    CPLFree( papoEntries );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read the named file to initialize this structure.               */
/************************************************************************/

int SDTS_CATD::Read( const char * pszFilename )

{
    DDFModule	oCATDFile;
    DDFRecord	*poRecord;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( !oCATDFile.Open( pszFilename ) )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*	Strip off the filename, and keep the path prefix.		*/
/* -------------------------------------------------------------------- */
    int		i;

    pszPrefixPath = CPLStrdup( pszFilename );
    for( i = strlen(pszPrefixPath)-1; i > 0; i-- )
    {
        if( pszPrefixPath[i] == '\\' || pszPrefixPath[i] == '/' )
        {
            pszPrefixPath[i] = '\0';
            break;
        }
    }

    if( i <= 0 )
        pszPrefixPath[0] = '\0';
    
/* ==================================================================== */
/*      Loop reading CATD records, and adding to our list of entries    */
/*      for each.                                                       */
/* ==================================================================== */
    while( (poRecord = oCATDFile.ReadRecord()) != NULL )
    {
/* -------------------------------------------------------------------- */
/*      Verify that we have a proper CATD record.                       */
/* -------------------------------------------------------------------- */
        if( poRecord->GetStringSubfield( "CATD", 0, "MODN", 0 ) == NULL )
            continue;
        
/* -------------------------------------------------------------------- */
/*      Create a new entry, and get the module and file name.           */
/* -------------------------------------------------------------------- */
        SDTS_CATDEntry	*poEntry = new SDTS_CATDEntry;

        poEntry->pszModule =
            CPLStrdup(poRecord->GetStringSubfield( "CATD", 0, "NAME", 0 ));
        poEntry->pszFile =
            CPLStrdup(poRecord->GetStringSubfield( "CATD", 0, "FILE", 0 ));
        poEntry->pszExternalFlag =
            CPLStrdup(poRecord->GetStringSubfield( "CATD", 0, "EXTR", 0 ));
        poEntry->pszType =
            CPLStrdup(poRecord->GetStringSubfield( "CATD", 0, "TYPE", 0 ));

/* -------------------------------------------------------------------- */
/*      Create a full path to the file.                                 */
/* -------------------------------------------------------------------- */
        poEntry->pszFullPath = (char *)
            CPLMalloc(strlen(pszPrefixPath)+strlen(poEntry->pszFile)+2);

        sprintf( poEntry->pszFullPath,
                 "%s%c%s",
                 pszPrefixPath,
#ifdef WIN32
                 '\\',
#else
                 '/',
#endif
                 poEntry->pszFile );
        
/* -------------------------------------------------------------------- */
/*      Add the entry to the list.                                      */
/* -------------------------------------------------------------------- */
        papoEntries = (SDTS_CATDEntry **)
            CPLRealloc(papoEntries, sizeof(void*) * ++nEntries );
        papoEntries[nEntries-1] = poEntry;
    }

    return nEntries > 0;
}


/************************************************************************/
/*                         getModuleFilePath()                          */
/************************************************************************/

const char * SDTS_CATD::getModuleFilePath( const char * pszModule )

{
    int		i;

    for( i = 0; i < nEntries; i++ )
    {
        if( EQUAL(papoEntries[i]->pszModule,pszModule) )
            return papoEntries[i]->pszFullPath;
    }

    return NULL;
}

/************************************************************************/
/*                           getEntryModule()                           */
/************************************************************************/

const char * SDTS_CATD::getEntryModule( int iEntry )

{
    if( iEntry < 0 || iEntry >= nEntries )
        return NULL;
    else
        return papoEntries[iEntry]->pszModule;
}

/************************************************************************/
/*                            getEntryType()                            */
/************************************************************************/

const char * SDTS_CATD::getEntryType( int iEntry )

{
    if( iEntry < 0 || iEntry >= nEntries )
        return NULL;
    else
        return papoEntries[iEntry]->pszType;
}


