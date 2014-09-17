/**********************************************************************
 * $Id: mitab_ogr_driver.h,v 1.14 2007-03-22 20:01:36 dmorissette Exp $
 *
 * Name:     mitab_ogr_drive.h
 * Project:  Mid/mif tab ogr support
 * Language: C++
 * Purpose:  Header file containing public definitions for the library.
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: mitab_ogr_driver.h,v $
 * Revision 1.14  2007-03-22 20:01:36  dmorissette
 * Added SPATIAL_INDEX_MODE=QUICK creation option (MITAB bug 1669)
 *
 * Revision 1.13  2004/07/07 16:11:39  fwarmerdam
 * fixed up some single layer creation issues
 *
 * Revision 1.12  2004/02/27 21:06:03  fwarmerdam
 * Better support for "single file" creation ... don't allow other layers to
 * be created.  But *do* single file to satisfy the first layer creation request
 * made.  Also, allow creating a datasource "on" an existing directory.
 *
 * Revision 1.11  2003/03/20 15:57:46  warmerda
 * Added delete datasource support
 *
 * Revision 1.10  2002/02/08 16:52:16  warmerda
 * added support for FORMAT=MIF option for creating layers
 *
 * Revision 1.9  2001/09/14 03:22:58  warmerda
 * added RegisterOGRTAB() prototype
 *
 * Revision 1.8  2001/01/22 16:03:59  warmerda
 * expanded tabs
 *
 * Revision 1.7  2000/01/26 18:17:00  warmerda
 * reimplement OGR driver
 *
 * Revision 1.6  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.5  1999/12/15 16:28:17  warmerda
 * fixed a few type problems
 *
 * Revision 1.4  1999/12/15 16:15:05  warmerda
 * Avoid unused parameter warnings.
 *
 * Revision 1.3  1999/12/14 02:23:05  daniel
 * Merged TAB+MIF DataSource/Driver into one using IMapInfoFile class
 *
 * Revision 1.2  1999/11/12 02:44:36  stephane
 * added comment, change Register name.
 *
 * Revision 1.1  1999/11/08 21:05:51  svillene
 * first revision
 *
 **********************************************************************/

#include "mitab.h"
#include "ogrsf_frmts.h"

#ifndef _MITAB_OGR_DRIVER_H_INCLUDED_
#define _MITAB_OGR_DRIVER_H_INCLUDED_

/*=====================================================================
 *            OGRTABDataSource Class
 * 
 * These classes handle all the file types supported by the MITAB lib.
 * through the IMapInfoFile interface.
 *====================================================================*/
class OGRTABDataSource : public OGRDataSource
{
  private:
    char                *m_pszName;
    char                *m_pszDirectory;

    int                 m_nLayerCount;
    IMapInfoFile        **m_papoLayers;

    char                **m_papszOptions;
    int                 m_bCreateMIF;
    int                 m_bSingleFile;
    int                 m_bSingleLayerAlreadyCreated;
    GBool               m_bQuickSpatialIndexMode;
    int                 m_bUpdate;

  public:
                OGRTABDataSource();
    virtual     ~OGRTABDataSource();

    int         Open( GDALOpenInfo* poOpenInfo, int bTestOpen );
    int         Create( const char *pszName, char ** papszOptions );

    const char  *GetName() { return m_pszName; }
    int          GetLayerCount();
    OGRLayer    *GetLayer( int );
    int          TestCapability( const char * );
    
    OGRLayer    *ICreateLayer(const char *, 
                             OGRSpatialReference * = NULL,
                             OGRwkbGeometryType = wkbUnknown,
                             char ** = NULL );

    char        **GetFileList();
};

void CPL_DLL RegisterOGRTAB();

#endif /* _MITAB_OGR_DRIVER_H_INCLUDED_ */
