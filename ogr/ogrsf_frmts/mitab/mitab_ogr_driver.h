/**********************************************************************
 * $Id: mitab.h,v 1.13 1999/11/08 04:34:55 stephane Exp $
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
 * $Log: mitab.h,v $
 *
 **********************************************************************/
#include "mitab.h"
#include "ogrsf_frmts.h"

/*=====================================================================
 *            OGRTABDataSource Class
 *
 *====================================================================*/
class OGRTABDataSource : public OGRDataSource
{
  private:
    TABFile	        *m_poLayer;
    char		*m_pszName;
    
  public:
    			 OGRTABDataSource( const char * pszName,
                                            TABFile * poLayerIn );
    			~OGRTABDataSource();

    const char	        *GetName() { return m_pszName; }
    int			 GetLayerCount() { return 1; }
    OGRLayer		*GetLayer( int ) { return m_poLayer; }
    int                  TestCapability( const char * ){return 0;}
    OGRLayer    *CreateLayer(const char *, 
			     OGRSpatialReference * = NULL,
			     OGRwkbGeometryType = wkbUnknown,
			     char ** = NULL ){return NULL;}
};
 

class OGRMIDDataSource : public OGRDataSource
{
  private:
    MIFFile	        *m_poLayer;
    char		*m_pszName; 
    
  public:
    			 OGRMIDDataSource( const char * pszName,
                                            MIFFile * poLayerIn );
    			~OGRMIDDataSource();

    const char	        *GetName() { return m_pszName; }
    int			 GetLayerCount() { return 1; }
    OGRLayer		*GetLayer( int ) { return m_poLayer; }
    int                 TestCapability( const char * ){return 0;}
    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL ){return NULL;}
};


class OGRTABDriver : public OGRSFDriver
{
public:
              ~OGRTABDriver();

     const char *GetName();
     OGRDataSource *Open ( const char *,int );
     int TestCapability( const char * ){return 0;}
     virtual OGRDataSource *CreateDataSource( const char *pszName,
						 char ** = NULL ){return NULL;}
    
    

};


class OGRMIDDriver : public OGRSFDriver
{
  public: 
                ~OGRMIDDriver();

      const char *GetName();
      OGRDataSource *Open( const char *,int );
      int                 TestCapability( const char * ){return 0;}
      virtual OGRDataSource *CreateDataSource( const char *pszName,
					       char ** = NULL ){return NULL;}
    
    
};










