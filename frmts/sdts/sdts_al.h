/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Include file for entire SDTS Abstraction Layer functions.
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
 * Revision 1.2  1999/03/23 15:58:01  warmerda
 * some const fixes, and attr record fixes
 *
 * Revision 1.1  1999/03/23 13:56:14  warmerda
 * New
 *
 */

#ifndef SDTS_AL_H_INCLUDED
#define STDS_AL_H_INCLUDED

#include "cpl_conv.h"
#include "container/sc_Record.h"
#include <iostream>
#include <fstream>

/* -------------------------------------------------------------------- */
/*      scal_Record                                                     */
/*                                                                      */
/*      This is a slighly extended record with some more convenient     */
/*      subfield fetching methods.                                      */
/* -------------------------------------------------------------------- */
class SDTS_IREF;

class scal_Record : public sc_Record
{
  public:
    sc_Subfield		*getSubfield( const string, int, const string, int );
};

sc_Subfield * SDTSGetSubfieldOfField( const sc_Field *, string, int );

int SDTSGetSADR( SDTS_IREF *, const sc_Field *, double *, double *, double * );

/************************************************************************/
/*                              SDTS_IREF                               */
/*									*/
/*      Class for reading the IREF (internal reference) module.         */
/************************************************************************/
class SDTS_IREF
{
  public:
    		SDTS_IREF();
		~SDTS_IREF();

    int         Read( const string osFilename );

    string	osXAxisName;			/* XLBL */
    string	osYAxisName;			/* YLBL */

    double	dfXScale;			/* SFAX */
    double      dfYScale;			/* SFAY */

    string	osCoordinateFormat;		/* HFMT */
                
};

/************************************************************************/
/*                              SDTS_CATD                               */
/*                                                                      */
/*      Class for reading the directory of other files file.            */
/************************************************************************/
class SDTS_CATDEntry;

class SDTS_CATD
{
    string	osPrefixPath;

    int		nEntries;
    SDTS_CATDEntry **papoEntries;
    
  public:
    		SDTS_CATD();
                ~SDTS_CATD();

    int         Read( const string osFilename );

    string	getModuleFilePath( const string &osModule );

    int		getEntryCount() { return nEntries; }
    const string &getEntryModule(int);
    const string &getEntryType(int);
};

/************************************************************************/
/*                            SDTSLineReader                            */
/*                                                                      */
/*      Class for reading any of the files lines.                       */
/************************************************************************/

class SDTSRawLine;
class sio_8211Reader;
class sio_8211ForwardIterator;

class SDTSLineReader
{
    ifstream	ifs;
    sio_8211Reader	*po8211Reader;
    sio_8211ForwardIterator *poIter;

    SDTS_IREF	*poIREF;
    
  public:
    		SDTSLineReader( SDTS_IREF * );
                ~SDTSLineReader();

    int         Open( const string );
    SDTSRawLine *GetNextLine( void );
    void	Close();
};

/************************************************************************/
/*                              SDTSModId                               */
/*                                                                      */
/*      A simple class to encapsulate a model and record                */
/*      identifier.  Implemented in sdtslib.cpp.                        */
/************************************************************************/

class SDTSModId
{
  public:
    		SDTSModId() { szModule[0] = '\0'; nRecord = -1; }

    int		Set( const sc_Field * );
    
    char	szModule[8];
    long	nRecord;
};

/************************************************************************/
/*                             SDTSRawLine                              */
/*                                                                      */
/*      Simple container for the information from a line read from a    */
/*      line file.                                                      */
/************************************************************************/
class SDTSRawLine
{
  public:
    		SDTSRawLine();
                ~SDTSRawLine();

    int         Read( SDTS_IREF *, scal_Record * );

    SDTSModId	oLine;			/* LINE field */
                
    int		nVertices;

    double	*padfX;
    double	*padfY;
    double	*padfZ;

    SDTSModId	oLeftPoly;		/* PIDL */
    SDTSModId	oRightPoly;		/* PIDR */
    SDTSModId	oStartNode;		/* SNID */
    SDTSModId	oEndNode;		/* ENID */
    SDTSModId	oAttribute;		/* ATID - last used for now */

    void	Dump( FILE * );
};

/************************************************************************/
/*                            SDTSAttrReader                            */
/*                                                                      */
/*      Class for reading any of the primary attribute files.           */
/************************************************************************/

class SDTSAttrRecord;

class SDTSAttrReader
{
    ifstream	ifs;
    sio_8211Reader	*po8211Reader;
    sio_8211ForwardIterator *poIter;

    SDTS_IREF	*poIREF;
    
  public:
    		SDTSAttrReader( SDTS_IREF * );
                ~SDTSAttrReader();

    int         Open( const string );
    SDTSAttrRecord *GetNextRecord( void );
    void	Close();
};

/************************************************************************/
/*                            SDTSAttrRecord                            */
/*                                                                      */
/*      This class represents one record from a primary attribute       */
/*      file.  It encapsulates a sc_Record, and returns a handle to     */
/*      an sc_Field which the caller uses normal iterators on to        */
/*      find all the sc_Subfields.  Normal methods are used to query    */
/*      these.                                                          */
/************************************************************************/

class SDTSAttrRecord
{
    scal_Record		oRecord;
    const sc_Field	*poATTP;

    friend class SDTSAttrReader;
    
  public:
    			SDTSAttrRecord();
                        ~SDTSAttrRecord();

    SDTSModId		oRecordId;
    
    const sc_Field	*GetSubfieldList() { return poATTP; }
};

#endif /* ndef SDTS_AL_H_INCLUDED */
