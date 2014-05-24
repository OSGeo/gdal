/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/GRASS driver.
 * Author:   Radim Blazek, radim.blazek@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Radim Blazek <radim.blazek@gmail.com>
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

#ifndef _OGRGRASS_H_INCLUDED
#define _OGRGRASS_H_INCLUDED

#include "ogrsf_frmts.h"

extern "C" {
    #include <grass/version.h>
    #include <grass/gprojects.h>
    #include <grass/gis.h>
    #include <grass/dbmi.h>
#if GRASS_VERSION_MAJOR  >= 7
    #include <grass/vector.h>
#else
    #include <grass/Vect.h>
#endif
}

/************************************************************************/
/*                            OGRGRASSLayer                             */
/************************************************************************/
class OGRGRASSLayer : public OGRLayer
{
  public:
                        OGRGRASSLayer(	int layer, struct Map_info * map );
                        ~OGRGRASSLayer();

    // Layer info
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
    int                 GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce);
    virtual OGRSpatialReference *GetSpatialRef();
    int                 TestCapability( const char * );

    // Reading
    void                ResetReading();
    virtual OGRErr      SetNextByIndex( long nIndex );
    OGRFeature *        GetNextFeature();
    OGRFeature         *GetFeature( long nFeatureId );

    // Filters
    virtual OGRErr 	SetAttributeFilter( const char *query );
    virtual void 	SetSpatialFilter( OGRGeometry * poGeomIn );

    // Write access, not supported:
    virtual OGRErr      CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    
  private:
    char		*pszName;
    OGRSpatialReference *poSRS;
    OGRFeatureDefn	*poFeatureDefn;
    char		*pszQuery;	// Attribute filter string

    int			iNextId;
    int			nTotalCount;
    int			iLayer;		// Layer number 
    int			iLayerIndex;	// Layer index (in GRASS category index)
    int			iCatField;	// Field where category (key) is stored
    int			nFields;
    int 		*paFeatureIndex; // Array of indexes to category index array

    // Vector map
    struct Map_info 	*poMap;
    struct field_info   *poLink;

    // Database connection
    bool 		bHaveAttributes;

    dbString		*poDbString;
    dbDriver		*poDriver;
    dbCursor		*poCursor;
    
    bool		bCursorOpened;	// Sequential database cursor opened
    int 		iCurrentCat;	// Current category in select cursor

    struct line_pnts	*poPoints; 
    struct line_cats	*poCats;

    bool		StartDbDriver ();
    bool		StopDbDriver ();

    OGRGeometry		*GetFeatureGeometry ( long nFeatureId, int *cat );
    bool		SetAttributes ( OGRFeature *feature, dbTable *table );

    // Features matching spatial filter for ALL features/elements in GRASS
    char 		*paSpatialMatch;
    bool 		SetSpatialMatch();

    // Features matching attribute filter for ALL features/elements in GRASS
    char 		*paQueryMatch;
    bool 		OpenSequentialCursor();
    bool 		ResetSequentialCursor();
    bool 		SetQueryMatch();
};

/************************************************************************/
/*                          OGRGRASSDataSource                          */
/************************************************************************/
class OGRGRASSDataSource : public OGRDataSource
{
  public:
                        OGRGRASSDataSource();
                        ~OGRGRASSDataSource();

    int                 Open( const char *, int bUpdate, int bTestOpen,
                              int bSingleNewFile = FALSE );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    // Not implemented (returns NULL):
    virtual OGRLayer    *ICreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );


  private:
    OGRGRASSLayer     **papoLayers;
    char                *pszName;	// Date source name
    char		*pszGisdbase;	// GISBASE
    char		*pszLocation;	// location name
    char		*pszMapset;	// mapset name
    char		*pszMap;	// name of vector map

    struct Map_info 	map;
    int                 nLayers;
    
    int                 bOpened;

    static bool SplitPath ( char *, char **, char **, char **, char ** );
};

/************************************************************************/
/*                            OGRGRASSDriver                            */
/************************************************************************/
class OGRGRASSDriver : public OGRSFDriver
{
  public:
			~OGRGRASSDriver();
                
    const char 		*GetName();
    OGRDataSource 	*Open( const char *, int );

    int                 TestCapability( const char * );

    // Not implemented (return error/NULL):
    virtual OGRDataSource *CreateDataSource( const char *pszName, 
	    				     char ** = NULL );
    OGRErr              DeleteDataSource( const char *pszDataSource );
};

#endif /* ndef _OGRGRASS_H_INCLUDED */
