/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Mainline test program for converting SDTS TVP (or at least USGS
 *           dlg) datasets to an ASCII dump.
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
 * Revision 1.8  1999/08/16 21:00:02  warmerda
 * added szOBRP support for SDTSModId
 *
 * Revision 1.7  1999/08/16 15:45:46  warmerda
 * added IsSecondary()
 *
 * Revision 1.6  1999/08/16 13:58:30  warmerda
 * added support for secondary attribute modules
 *
 * Revision 1.5  1999/07/30 19:15:24  warmerda
 * fiddled
 *
 * Revision 1.4  1999/05/11 14:05:15  warmerda
 * catd method update
 *
 * Revision 1.3  1999/05/07 13:45:01  warmerda
 * major upgrade to use iso8211lib
 *
 * Revision 1.2  1999/03/23 15:58:29  warmerda
 * added attribute support, dump catalog
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"
#include "cpl_string.h"

int main( int nArgc, char ** papszArgv )

{
{
    SDTS_IREF	oIREF;
    SDTS_CATD	oCATD;
    int		i;
    const char	*pszCATDFilename;

    if( nArgc < 2 )
        pszCATDFilename = "dlg/TR01CATD.DDF";
    else
        pszCATDFilename = papszArgv[1];

/* -------------------------------------------------------------------- */
/*      Read the catalog.                                               */
/* -------------------------------------------------------------------- */
    if( !oCATD.Read( pszCATDFilename ) )
    {
        printf( "Failed to read CATD file\n" );
        exit( 100 );
    }

    printf( "Catalog:\n" );
    for( i = 0; i < oCATD.GetEntryCount(); i++ )
    {
        printf( "  %s: `%s'\n",
                oCATD.GetEntryModule(i),
                oCATD.GetEntryTypeDesc(i) );
    }
    printf( "\n" );
    
/* -------------------------------------------------------------------- */
/*      Dump the internal reference information.                        */
/* -------------------------------------------------------------------- */
#ifdef notdef    
    printf( "IREF filename = %s\n", oCATD.getModuleFilePath( "IREF" ) );
    if( oIREF.Read( oCATD.getModuleFilePath( "IREF" ) ) )
    {
        printf( "X Label = %s\n", oIREF.pszXAxisName );
        printf( "X Scale = %f\n", oIREF.dfXScale );
    }
#endif    
/* -------------------------------------------------------------------- */
/*      Dump the first line file.                                       */
/* -------------------------------------------------------------------- */
#ifdef notdef    
    SDTSLineReader oLineReader( &oIREF );

    if( oLineReader.Open( oCATD.GetModuleFilePath( "LE01" ) ) )
    {
        SDTSRawLine	*poRawLine;
        
        printf( "ATID referenced modules:\n" );
        CSLPrint( oLineReader.ScanModuleReferences(), stdout );

        while( (poRawLine = oLineReader.GetNextLine()) != NULL )
        {
            poRawLine->Dump( stdout );
            delete poRawLine;
        }
        
        oLineReader.Close();
    }
#endif
    
/* -------------------------------------------------------------------- */
/*      Dump the first polygon                                          */
/* -------------------------------------------------------------------- */
#ifndef notdef    
    SDTSPolygonReader oPolyReader;

    if( oPolyReader.Open( oCATD.GetModuleFilePath( "PC01" ) ) )
    {
        SDTSRawPolygon	*poRawPoly;
        
        printf( "ATID referenced modules:\n" );
        CSLPrint( oPolyReader.ScanModuleReferences(), stdout );

        while( (poRawPoly = oPolyReader.GetNextPolygon()) != NULL )
        {
            printf( "PolyId:%s/%s nAttributes=%d  AreaId=%s\n",
                    poRawPoly->oPolyId.GetName(),
                    poRawPoly->oPolyId.szOBRP,
                    poRawPoly->nAttributes,
                    poRawPoly->oAreaId.GetName() );
            delete poRawPoly;
        }
        
        oPolyReader.Close();
    }
#endif
    
/* -------------------------------------------------------------------- */
/*	Dump all modules of type "Primary Attribute" in the catalog.	*/
/* -------------------------------------------------------------------- */
#ifdef notdef    
    SDTSAttrReader oAttrReader( &oIREF );

    for( i = 0; i < oCATD.GetEntryCount(); i++ )
    {
        if( oCATD.GetEntryType(i) == SLTAttr )
        {
            if( oAttrReader.Open(
                	oCATD.GetModuleFilePath( oCATD.GetEntryModule(i)) )
                && oAttrReader.IsSecondary() )
            {
                DDFField	*poATTP;
                SDTSModId	oModId;
                
                while( (poATTP = oAttrReader.GetNextRecord(&oModId)) != NULL )
                {
                    fprintf( stdout,
                             "\nRecord %s:%ld\n",
                             oModId.szModule,
                             oModId.nRecord );

                    poATTP->Dump( stdout );
                }
            }

            oAttrReader.Close();
        }
    }
#endif    
}

#ifdef DBMALLOC
malloc_dump(1);
#endif

}
