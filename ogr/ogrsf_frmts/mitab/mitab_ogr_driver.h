/**********************************************************************
 * $Id: mitab_ogr_driver.h,v 1.5 1999/12/15 16:28:17 warmerda Exp $
 *
 * Name:     mitab_ogr_drive.h
 * Project:  Mid/mif tab ogr support
 * Language: C++
 * Purpose:  Header file containing public definitions for the library.
 * Author:   Stephane Villeneuve, s.villeneuve@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, Stephane Villeneuve
 *
 * All rights reserved.  This software may be copied or reproduced, in
 * all or in part, without the prior written consent of its author,
 * Daniel Morissette (danmo@videotron.ca).  However, any material copied
 * or reproduced must bear the original copyright notice (above), this 
 * original paragraph, and the original disclaimer (below).
 * 
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although 
 * considerable efforts have been used in preparing the Software, the 
 * author does not warrant the accuracy or completeness of the Software.
 * In no event will the author be liable for damages, including loss of
 * profits or consequential damages, arising out of the use of the 
 * Software.
 * 
 **********************************************************************
 *
 * $Log: mitab_ogr_driver.h,v $
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
    IMapInfoFile        *m_poLayer;
    char                *m_pszName;
    
  public:
    OGRTABDataSource( const char * pszName,
                      IMapInfoFile * poLayerIn );
    ~OGRTABDataSource();

    const char	*GetName() { return m_pszName; }
    int          GetLayerCount() { return 1; }
    OGRLayer    *GetLayer( int ) { return m_poLayer; }
    int          TestCapability( const char * ){return 0;}
    OGRLayer    *CreateLayer(const char *, 
                             OGRSpatialReference * = NULL,
                             OGRwkbGeometryType = wkbUnknown,
                             char ** = NULL )   {return NULL;}
};
 

class OGRTABDriver : public OGRSFDriver
{
  public:
    ~OGRTABDriver();

    const char  *GetName();
    OGRDataSource *Open ( const char *,int );
    int         TestCapability( const char * ){return 0;}
    virtual OGRDataSource *CreateDataSource( const char * /*pszName*/,
                                             char ** = NULL ){return NULL;}
    
    

};



#endif /* _MITAB_OGR_DRIVER_H_INCLUDED_ */
