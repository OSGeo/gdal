/*
 * geonames.h
 *
 *  This encapsulates all of the value-naming mechanism of 
 *  libgeotiff. 
 *
 *  Written By: Niles Ritter
 *
 *  Revision History:
 *
 *      Author     Date     Key Changes/Additions
 *      ------  ----------  -------------------------------------
 *      ndr    10 Jun 95     Inital Beta Release
 *      ndr    28 Jul 95     Added ModelType aliases, Kv aliases.
 */

#ifndef __geonames_h
#define __geonames_h

struct _KeyInfo {
	int ki_key;
	char *ki_name;
};
typedef struct _KeyInfo KeyInfo;

/* If memory is a premium, then omitting the 
 * long name lists may save some space; simply 
 * #define OMIT_GEOTIFF_NAMES in the compile statement
 * to remove all key->string translation.
 */
#ifdef ValuePair
#  undef ValuePair
#endif

#ifndef OMIT_GEOTIFF_NAMES
#define ValuePair(token,value)  {token,#token},
#else
#define ValuePair(token,value)
#endif

#define END_LIST { -1, (char *)0}

/************************************************************
 *         6.2.x GeoTIFF Keys
 ************************************************************/

static KeyInfo _keyInfo[] =  {
#   include "geokeys.inc"   /* geokey database */
    END_LIST
};

#define COMMON_VALUES \
   {KvUndefined, "Undefined"}, \
   {KvUserDefined,"User-Defined"}, \
   ValuePair(KvUndefined,KvUndefined) \
   ValuePair(KvUserDefined,KvUserDefined) 

static KeyInfo _csdefaultValue[] = {
   COMMON_VALUES
   END_LIST  
};

/************************************************************
 *         6.3.x GeoTIFF Key Values
 ************************************************************/

static KeyInfo _modeltypeValue[] = {
   COMMON_VALUES
    ValuePair(ModelTypeProjected,1)
    ValuePair(ModelTypeGeographic,2)
    ValuePair(ModelTypeGeocentric,3)
    ValuePair(ModelProjected,1)     /* aliases */
    ValuePair(ModelGeographic,2)    /* aliases */
    ValuePair(ModelGeocentric,3)    /* aliases */
   END_LIST  
};

static KeyInfo _rastertypeValue[] = {
   COMMON_VALUES
    ValuePair(RasterPixelIsArea,1)
    ValuePair(RasterPixelIsPoint,2)
   END_LIST  
};

static KeyInfo _geounitsValue[] = {
   COMMON_VALUES
#  include "epsg_units.inc"
   END_LIST  
};

static KeyInfo _geographicValue[] = {
   COMMON_VALUES
#  include "epsg_gcs.inc"
   END_LIST  
};

static KeyInfo _geodeticdatumValue[] = {
   COMMON_VALUES
#  include "epsg_datum.inc"
   END_LIST  
};

static KeyInfo _ellipsoidValue[] = {
   COMMON_VALUES
#  include "epsg_ellipse.inc"
   END_LIST  
};

static KeyInfo _primemeridianValue[] = {
   COMMON_VALUES
#  include "epsg_pm.inc"
   END_LIST  
};

static KeyInfo _pcstypeValue[] = {
   COMMON_VALUES
#  include "epsg_pcs.inc"
   END_LIST  
};

static KeyInfo _projectionValue[] = {
   COMMON_VALUES
#  include "epsg_proj.inc"
   END_LIST  
};

static KeyInfo _coordtransValue[] = {
   COMMON_VALUES
#  include "geo_ctrans.inc"
   END_LIST  
};

static KeyInfo _vertcstypeValue[] = {
   COMMON_VALUES
#  include "epsg_vertcs.inc"
   END_LIST  
};

static KeyInfo _vdatumValue[] = {
   COMMON_VALUES
    ValuePair(VDatumBase,1)
   END_LIST  
};

#endif /* __geonames_h */

