#ifndef _SHAPEFILE_H_INCLUDED
#define _SHAPEFILE_H_INCLUDED

/* NOTE: This file (dbfopen.h) is a copy of shapefil.h (described below)
 *       in which only the DBF-related stuff was kept to include it in
 *       the AVCE00 library.
 */

/******************************************************************************
 * $Id: dbfopen.h,v 1.2 2000/09/26 20:21:05 daniel Exp $
 *
 * Project:  Shapelib
 * Purpose:  Primary include file for Shapelib.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see LICENSE.LGPL).  This
 * option is discussed in more detail in shapelib.html.
 *
 * --
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
 * $Log: dbfopen.h,v $
 * Revision 1.2  2000/09/26 20:21:05  daniel
 * Added AVCCoverPC write
 *
 * Revision 1.1  1999/12/24 07:16:41  daniel
 * Checked into AVCE00 lib + Added DBFGetNativeFieldType()
 *
 * Revision 1.14  1999/11/05 14:12:05  warmerda
 * updated license terms
 *
 * Revision 1.13  1999/06/02 18:24:21  warmerda
 * added trimming code
 *
 * Revision 1.12  1999/06/02 17:56:12  warmerda
 * added quad'' subnode support for trees
 *
 * Revision 1.11  1999/05/18 19:11:11  warmerda
 * Added example searching capability
 *
 * Revision 1.10  1999/05/18 17:49:38  warmerda
 * added initial quadtree support
 *
 * Revision 1.9  1999/05/11 03:19:28  warmerda
 * added new Tuple api, and improved extension handling - add from candrsn
 *
 * Revision 1.8  1999/03/23 17:22:27  warmerda
 * Added extern "C" protection for C++ users of shapefil.h.
 *
 * Revision 1.7  1998/12/31 15:31:07  warmerda
 * Added the TRIM_DBF_WHITESPACE and DISABLE_MULTIPATCH_MEASURE options.
 *
 * Revision 1.6  1998/12/03 15:48:15  warmerda
 * Added SHPCalculateExtents().
 *
 * Revision 1.5  1998/11/09 20:57:16  warmerda
 * Altered SHPGetInfo() call.
 *
 * Revision 1.4  1998/11/09 20:19:33  warmerda
 * Added 3D support, and use of SHPObject.
 *
 * Revision 1.3  1995/08/23 02:24:05  warmerda
 * Added support for reading bounds.
 *
 * Revision 1.2  1995/08/04  03:17:39  warmerda
 * Added header.
 *
 */

#include <stdio.h>

#ifdef USE_DBMALLOC
#include <dbmalloc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
    
/************************************************************************/
/*                        Configuration options.                        */
/************************************************************************/

/* -------------------------------------------------------------------- */
/*      Should the DBFReadStringAttribute() strip leading and           */
/*      trailing white space?                                           */
/* -------------------------------------------------------------------- */

/* Note: For AVCE00 purposes, we do not want the spaces to be stripped.
 *
 * #define TRIM_DBF_WHITESPACE
 */

/************************************************************************/
/*                             DBF Support.                             */
/************************************************************************/
typedef	struct
{
    FILE	*fp;

    int         nRecords;

    int		nRecordLength;
    int		nHeaderLength;
    int		nFields;
    int		*panFieldOffset;
    int		*panFieldSize;
    int		*panFieldDecimals;
    char	*pachFieldType;

    char	*pszHeader;

    int		nCurrentRecord;
    int		bCurrentRecordModified;
    char	*pszCurrentRecord;
    
    int		bNoHeader;
    int		bUpdated;
} DBFInfo;

typedef DBFInfo * DBFHandle;

typedef enum {
  FTString,
  FTInteger,
  FTDouble,
  FTInvalid
} DBFFieldType;

#define XBASE_FLDHDR_SZ       32

DBFHandle DBFOpen( const char * pszDBFFile, const char * pszAccess );
DBFHandle DBFCreate( const char * pszDBFFile );

int	DBFGetFieldCount( DBFHandle psDBF );
int	DBFGetRecordCount( DBFHandle psDBF );
int	DBFAddField( DBFHandle hDBF, const char * pszFieldName,
		     DBFFieldType eType, int nWidth, int nDecimals );

DBFFieldType DBFGetFieldInfo( DBFHandle psDBF, int iField, 
			      char * pszFieldName, 
			      int * pnWidth, int * pnDecimals );

char    DBFGetNativeFieldType( DBFHandle psDBF, int iField );

int 	DBFReadIntegerAttribute( DBFHandle hDBF, int iShape, int iField );
double 	DBFReadDoubleAttribute( DBFHandle hDBF, int iShape, int iField );
const char *DBFReadStringAttribute( DBFHandle hDBF, int iShape, int iField );

int DBFWriteIntegerAttribute( DBFHandle hDBF, int iShape, int iField, 
			      int nFieldValue );
int DBFWriteDoubleAttribute( DBFHandle hDBF, int iShape, int iField,
			     double dFieldValue );
int DBFWriteStringAttribute( DBFHandle hDBF, int iShape, int iField,
			     const char * pszFieldValue );
int DBFWriteAttributeDirectly(DBFHandle psDBF, int hEntity, int iField,
                              void * pValue );

const char *DBFReadTuple(DBFHandle psDBF, int hEntity );
int DBFWriteTuple(DBFHandle psDBF, int hEntity, void * pRawTuple );

DBFHandle DBFCloneEmpty(DBFHandle psDBF, const char * pszFilename );
 
void	DBFClose( DBFHandle hDBF );

#ifdef __cplusplus
}
#endif

#endif /* ndef _SHAPEFILE_H_INCLUDED */
