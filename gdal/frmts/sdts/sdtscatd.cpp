/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTS_CATD and SDTS_CATDEntry classes for
 *           reading CATD files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "sdts_al.h"

CPL_CVSID("$Id$");


/************************************************************************/
/* ==================================================================== */
/*                            SDTS_CATDEntry                            */
/*                                                                      */
/*      This class is for internal use of the SDTS_CATD class only,     */
/*      and represents one entry in the directory ... a reference       */
/*      to another module file.                                         */
/* ==================================================================== */
/************************************************************************/

class SDTS_CATDEntry

{
  public:
    char *      pszModule;
    char *      pszType;
    char *      pszFile;
    char *      pszExternalFlag;

    char *      pszFullPath;
};

/************************************************************************/
/* ==================================================================== */
/*                             SDTS_CATD                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             SDTS_CATD()                              */
/************************************************************************/

SDTS_CATD::SDTS_CATD()

{
    nEntries = 0;
    papoEntries = NULL;
    pszPrefixPath = NULL;
}

/************************************************************************/
/*                             ~SDTS_CATD()                             */
/************************************************************************/

SDTS_CATD::~SDTS_CATD()
{
    int         i;

    for( i = 0; i < nEntries; i++ )
    { 
        CPLFree( papoEntries[i]->pszModule );
        CPLFree( papoEntries[i]->pszType );
        CPLFree( papoEntries[i]->pszFile );
        CPLFree( papoEntries[i]->pszExternalFlag );
        CPLFree( papoEntries[i]->pszFullPath );
        delete papoEntries[i];
    }

    CPLFree( papoEntries );
    CPLFree( pszPrefixPath );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read the named file to initialize this structure.               */
/************************************************************************/

int SDTS_CATD::Read( const char * pszFilename )

{
    DDFModule   oCATDFile;
    DDFRecord   *poRecord;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( !oCATDFile.Open( pszFilename ) )
        return FALSE;

    CPLErrorReset();  // clear any ADRG "unrecognised data_struct_code" errors

/* -------------------------------------------------------------------- */
/*      Does this file have a CATD field?  If not, it isn't an SDTS     */
/*      record and we won't even try reading the first record for       */
/*      fear it will we a huge honking ADRG data record or something.   */
/* -------------------------------------------------------------------- */
    if( oCATDFile.FindFieldDefn( "CATD" ) == NULL )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Strip off the filename, and keep the path prefix.               */
/* -------------------------------------------------------------------- */
    int         i;

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
    {
        strcpy( pszPrefixPath, "." );
    }
    
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
        SDTS_CATDEntry  *poEntry = new SDTS_CATDEntry;

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
        poEntry->pszFullPath = 
            CPLStrdup(CPLFormCIFilename( pszPrefixPath, poEntry->pszFile,
                                         NULL ));
        
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
/*                         GetModuleFilePath()                          */
/************************************************************************/

const char * SDTS_CATD::GetModuleFilePath( const char * pszModule )

{
    int         i;

    for( i = 0; i < nEntries; i++ )
    {
        if( EQUAL(papoEntries[i]->pszModule,pszModule) )
            return papoEntries[i]->pszFullPath;
    }

    return NULL;
}

/************************************************************************/
/*                           GetEntryModule()                           */
/************************************************************************/

const char * SDTS_CATD::GetEntryModule( int iEntry )

{
    if( iEntry < 0 || iEntry >= nEntries )
        return NULL;
    else
        return papoEntries[iEntry]->pszModule;
}

/************************************************************************/
/*                          GetEntryTypeDesc()                          */
/************************************************************************/

/**
 * Fetch the type description of a module in the catalog.
 *
 * @param iEntry The module index within the CATD catalog.  A number from
 * zero to GetEntryCount()-1.
 *
 * @return A pointer to an internal string with the type description for
 * this module.  This is from the CATD file (subfield TYPE of field CATD),
 * and will be something like "Attribute Primary        ".
 */

const char * SDTS_CATD::GetEntryTypeDesc( int iEntry )

{
    if( iEntry < 0 || iEntry >= nEntries )
        return NULL;
    else
        return papoEntries[iEntry]->pszType;
}

/************************************************************************/
/*                            GetEntryType()                            */
/************************************************************************/

/**
 * Fetch the enumerated type of a module in the catalog.
 *
 * @param iEntry The module index within the CATD catalog.  A number from
 * zero to GetEntryCount()-1.
 *
 * @return A value from the SDTSLayerType enumeration indicating the type of
 * the module, and indicating the corresponding type of reader.<p>
 *
 * <ul>
 * <li> SLTPoint: Read with SDTSPointReader, underlying type of
 * <tt>Point-Node</tt>.
 * <li> SLTLine: Read with SDTSLineReader, underlying type of
 * <tt>Line</tt>.
 * <li> SLTAttr: Read with SDTSAttrReader, underlying type of
 * <tt>Attribute Primary</tt> or <tt>Attribute Secondary</tt>.
 * <li> SLTPolygon: Read with SDTSPolygonReader, underlying type of
 * <tt>Polygon</tt>.
 * </ul> 
 */

SDTSLayerType SDTS_CATD::GetEntryType( int iEntry )

{
    if( iEntry < 0 || iEntry >= nEntries )
        return SLTUnknown;

    else if( EQUALN(papoEntries[iEntry]->pszType,"Attribute Primary",17) )
        return SLTAttr;
    
    else if( EQUALN(papoEntries[iEntry]->pszType,"Attribute Secondary",17) )
        return SLTAttr;
    
    else if( EQUAL(papoEntries[iEntry]->pszType,"Line")
             || EQUALN(papoEntries[iEntry]->pszType,"Line ",5) )
        return SLTLine;
    
    else if( EQUALN(papoEntries[iEntry]->pszType,"Point-Node",10) )
        return SLTPoint;

    else if( EQUALN(papoEntries[iEntry]->pszType,"Polygon",7) )
        return SLTPoly;

    else if( EQUALN(papoEntries[iEntry]->pszType,"Cell",4) )
        return SLTRaster;

    else
        return SLTUnknown;
}

/************************************************************************/
/*                          GetEntryFilePath()                          */
/************************************************************************/

/**
 * Fetch the full filename of the requested module.
 *
 * @param iEntry The module index within the CATD catalog.  A number from
 * zero to GetEntryCount()-1.
 *
 * @return A pointer to an internal string containing the filename.  This
 * string should not be altered, or freed by the application.
 */

const char * SDTS_CATD::GetEntryFilePath( int iEntry )

{
    if( iEntry < 0 || iEntry >= nEntries )
        return NULL;
    else
        return papoEntries[iEntry]->pszFullPath;
}
