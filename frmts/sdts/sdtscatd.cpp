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
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"

#include <iostream>
#include <fstream>
#include "io/sio_Reader.h"
#include "io/sio_8211Converter.h"

#include "container/sc_Record.h"

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
    string	osModule;
    string	osFile;
    string	osExternalFlag;
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

int SDTS_CATD::Read( string osFilename )

{
    sc_Subfield	*poSubfield;
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    ifstream	ifs;
    
    ifs.open( osFilename.c_str() );
    if( !ifs )
    {
        printf( "Unable to open `%s'\n", osFilename.c_str() );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Establish reader access to the file, and read the first         */
/*      (only) record in the IREF file.                                 */
/* -------------------------------------------------------------------- */
    sio_8211Reader	oReader( ifs, NULL );
    sio_8211ForwardIterator oIter( oReader );
    scal_Record		oRecord;
    
    if( !oIter )
        return FALSE;
    
/* ==================================================================== */
/*      Loop reading CATD records, and adding to our list of entries    */
/*      for each.                                                       */
/* ==================================================================== */
    for( ; oIter; ++oIter )
    {
/* -------------------------------------------------------------------- */
/*      Read the record.                                                */
/* -------------------------------------------------------------------- */
        oIter.get( oRecord );

/* -------------------------------------------------------------------- */
/*      Verify that we have a proper CATD record.                       */
/* -------------------------------------------------------------------- */
        if( oRecord.getSubfield( "CATD", 0, "MODN", 0 ) == NULL )
            continue;
        
/* -------------------------------------------------------------------- */
/*      Create a new entry, and get the module and file name.           */
/* -------------------------------------------------------------------- */
        SDTS_CATDEntry	*poEntry = new SDTS_CATDEntry;

        poSubfield = oRecord.getSubfield( "CATD", 0, "NAME", 0 );
        if( poSubfield != NULL )
            poSubfield->getA( poEntry->osModule );

        poSubfield = oRecord.getSubfield( "CATD", 0, "FILE", 0 );
        if( poSubfield != NULL )
            poSubfield->getA( poEntry->osFile );

        poSubfield = oRecord.getSubfield( "CATD", 0, "EXTR", 0 );
        if( poSubfield != NULL )
            poSubfield->getA( poEntry->osExternalFlag );

/* -------------------------------------------------------------------- */
/*      Add the entry to the list.                                      */
/* -------------------------------------------------------------------- */
        papoEntries = (SDTS_CATDEntry **)
            CPLRealloc(papoEntries, sizeof(void*) * ++nEntries );
        papoEntries[nEntries-1] = poEntry;
    }

/* -------------------------------------------------------------------- */
/*	Strip off the filename, and keep the path prefix.		*/
/* -------------------------------------------------------------------- */
    int		i;
    
    osPrefixPath = osFilename;
    for( i = osPrefixPath.length()-1; i > 0; i-- )
    {
        if( osPrefixPath[i] == '\\' || osPrefixPath[i] == '/' )
        {
            osPrefixPath.resize(i);
            break;
        }
    }

    if( i <= 0 )
        osPrefixPath = "";
    
    return nEntries > 0;
}


/************************************************************************/
/*                         getModuleFilePath()                          */
/************************************************************************/

string SDTS_CATD::getModuleFilePath( string osModule )

{
    int		i;
    string	osFullPath;

/* -------------------------------------------------------------------- */
/*      Find the corresponding entry.                                   */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nEntries; i++ )
    {
        if( papoEntries[i]->osModule == osModule )
            break;
    }

    if( i == nEntries )
        return osFullPath;

/* -------------------------------------------------------------------- */
/*      Build up path.                                                  */
/* -------------------------------------------------------------------- */
    if( osPrefixPath.length() > 0 )
    {
        osFullPath = osPrefixPath;
#ifdef WIN32        
        osFullPath += "\\";
#else
        osFullPath += "/";
#endif
    }

    osFullPath += papoEntries[i]->osFile;

    return osFullPath;
}
