/******************************************************************************
 * $Id$
 *
 * Project:  Epi .REC Translator
 * Purpose:  Definition of classes for OGR .REC support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003,  Frank Warmerdam
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
 * Revision 1.5  2005/02/22 12:50:53  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.4  2003/07/11 13:28:08  warmerda
 * added low level api
 *
 * Revision 1.3  2003/05/21 04:00:36  warmerda
 * expand tabs
 *
 * Revision 1.2  2003/02/04 20:42:45  warmerda
 * skip zero length fields, improve type handling
 *
 * Revision 1.1  2003/02/03 21:11:35  warmerda
 * New
 *
 */

#ifndef _OGR_REC_H_INCLUDED
#define _OGR_REC_H_INLLUDED

#include "ogrsf_frmts.h"

class OGRRECDataSource;

CPL_C_START
int CPL_DLL RECGetFieldCount( FILE *fp);
int CPL_DLL RECGetFieldDefinition( FILE *fp, char *pszFieldName, int *pnType, 
                                   int *pnWidth, int *pnPrecision );
int CPL_DLL RECReadRecord( FILE *fp, char *pszRecBuf, int nRecordLength  );
const char CPL_DLL *RECGetField( const char *pszSrc, int nStart, int nWidth );
CPL_C_END


/************************************************************************/
/*                             OGRRECLayer                              */
/************************************************************************/

class OGRRECLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    FILE               *fpREC;
    int                 nStartOfData;
    int                 bIsValid;

    int                 nFieldCount;
    int                *panFieldOffset;
    int                *panFieldWidth;
    int                 nRecordLength;

    int                 nNextFID;

    OGRFeature *        GetNextUnfilteredFeature();

  public:
                        OGRRECLayer( const char *pszName, FILE *fp, 
                                     int nFieldCount );
                        ~OGRRECLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    void                SetSpatialFilter( OGRGeometry * ) {}

    int                 TestCapability( const char * );

    int                 IsValid() { return bIsValid; }

};

/************************************************************************/
/*                           OGRRECDataSource                           */
/************************************************************************/

class OGRRECDataSource : public OGRDataSource
{
    char                *pszName;

    OGRRECLayer        *poLayer;

  public:
                        OGRRECDataSource();
                        ~OGRRECDataSource();

    int                 Open( const char * pszFilename );
    
    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return 1; }
    OGRLayer            *GetLayer( int );
    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRRECDriver                             */
/************************************************************************/

class OGRRECDriver : public OGRSFDriver
{
  public:
                ~OGRRECDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int         TestCapability( const char * );
};


#endif /* ndef _OGR_REC_H_INCLUDED */
