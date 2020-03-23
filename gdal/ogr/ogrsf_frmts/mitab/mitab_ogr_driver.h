/**********************************************************************
 * $Id$
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
 **********************************************************************/

#include "mitab.h"
#include "ogrsf_frmts.h"

#ifndef MITAB_OGR_DRIVER_H_INCLUDED_
#define MITAB_OGR_DRIVER_H_INCLUDED_

/*=====================================================================
 *            OGRTABDataSource Class
 *
 * These classes handle all the file types supported by the MITAB lib.
 * through the IMapInfoFile interface.
 *====================================================================*/
class OGRTABDataSource : public OGRDataSource
{
    CPL_DISALLOW_COPY_ASSIGN(OGRTABDataSource)

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
    int                 m_nBlockSize;
    
  private:  
    inline bool         GetUpdate() const { return eAccess == GA_Update; }

  public:
                OGRTABDataSource();
    virtual     ~OGRTABDataSource();

    int         Open( GDALOpenInfo* poOpenInfo, int bTestOpen );
    int         Create( const char *pszName, char ** papszOptions );

    const char  *GetName() override { return m_pszName; }
    int          GetLayerCount() override;
    OGRLayer    *GetLayer( int ) override;
    int          TestCapability( const char * ) override;

    OGRLayer    *ICreateLayer(const char *,
                             OGRSpatialReference * = nullptr,
                             OGRwkbGeometryType = wkbUnknown,
                             char ** = nullptr ) override;

    char        **GetFileList() override;

    virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
};

void CPL_DLL RegisterOGRTAB();

#endif /* MITAB_OGR_DRIVER_H_INCLUDED_ */
