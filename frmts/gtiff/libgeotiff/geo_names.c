/*
 * geo_names.c
 *
 *  This encapsulates all of the value-naming mechanism of 
 *  libgeotiff. 
 *
 *  Written By: Niles Ritter
 */

#include "geotiffio.h"
#include "geonames.h"
#include "geo_tiffp.h" /* for tag names */

static KeyInfo _formatInfo[] =  {
   {TYPE_BYTE,    "Byte"},
   {TYPE_SHORT,   "Short"},
   {TYPE_LONG,    "Long"},
   {TYPE_RATIONAL,"Rational"},
   {TYPE_ASCII,   "Ascii"},
   {TYPE_FLOAT,   "Float"},
   {TYPE_DOUBLE,  "Double"},
   {TYPE_SBYTE,   "SignedByte"},
   {TYPE_SSHORT,  "SignedShort"},
   {TYPE_SLONG,  "SignedLong"},
   {TYPE_UNKNOWN, "Unknown"},
    END_LIST
};

static KeyInfo _tagInfo[] =  {
    {GTIFF_PIXELSCALE,  "ModelPixelScaleTag"},
    {GTIFF_TRANSMATRIX, "ModelTransformationTag"},
    {GTIFF_TIEPOINTS,   "ModelTiepointTag"},
     /* This alias maps the Intergraph symbol to the current tag */
    {GTIFF_TRANSMATRIX, "IntergraphMatrixTag"},
    END_LIST
};

static char *FindName(KeyInfo *info,int key)
{
   static char errmsg[80];
   
   while (info->ki_key>=0 && info->ki_key != key) info++;

   if (info->ki_key<0)
   {
	   sprintf(errmsg,"Unknown-%d", key );
	   return errmsg;
   }
   return info->ki_name;
}

char *GTIFKeyName(geokey_t key)
{
   return FindName( &_keyInfo[0],key);
}

char *GTIFTypeName(tagtype_t type)
{
   return FindName( &_formatInfo[0],type);
}

char *GTIFTagName(int tag)
{
   return FindName( &_tagInfo[0],tag);
}

char *GTIFValueName(geokey_t key, int value)
{
   KeyInfo *info;
   
   switch (key)
   {
	/* All codes using linear/angular/whatever units */
	case GeogLinearUnitsGeoKey: 
	case ProjLinearUnitsGeoKey: 
	case GeogAngularUnitsGeoKey: 
	case GeogAzimuthUnitsGeoKey: 
		                      info=_geounitsValue; break;

   	/* put other key-dependent lists here */
	case GTModelTypeGeoKey:       info=_modeltypeValue; break;
	case GTRasterTypeGeoKey:      info=_rastertypeValue; break;
	case GeographicTypeGeoKey:    info=_geographicValue; break;
	case GeogGeodeticDatumGeoKey: info=_geodeticdatumValue; break;
	case GeogEllipsoidGeoKey:     info=_ellipsoidValue; break;
	case GeogPrimeMeridianGeoKey: info=_primemeridianValue; break;
	case ProjectedCSTypeGeoKey:   info=_pcstypeValue; break;
	case ProjectionGeoKey:        info=_projectionValue; break;
	case ProjCoordTransGeoKey:    info=_coordtransValue; break;
	case VerticalCSTypeGeoKey:    info=_vertcstypeValue; break;
	case VerticalDatumGeoKey:     info=_vdatumValue; break;

	/* And if all else fails... */
   	default:                      info = _csdefaultValue;break;
   }
   
   return FindName( info,value);
}

/* 
 * Inverse Utilities (name->code) 
 */


static int FindCode(KeyInfo *info,char *key)
{
   while (info->ki_key>=0 && strcmp(info->ki_name,key) ) info++;

   if (info->ki_key<0)
   {
	/* not a registered key; might be generic code */
	if (!strncmp(key,"Unknown-",8))
	{
		int code=-1;
		sscanf(key,"Unknown-%d",&code);
		return code;
	}
	else return -1;
   }
   return info->ki_key;
}

int GTIFKeyCode(char *key)
{
   return FindCode( &_keyInfo[0],key);
}

int GTIFTypeCode(char *type)
{
   return FindCode( &_formatInfo[0],type);
}

int GTIFTagCode(char *tag)
{
   return FindCode( &_tagInfo[0],tag);
}


/*
 *  The key must be determined with GTIFKeyCode() before
 *  the name can be encoded.
 */
int GTIFValueCode(geokey_t key, char *name)
{
   KeyInfo *info;
   
   switch (key)
   {
	/* All codes using linear/angular/whatever units */
	case GeogLinearUnitsGeoKey: 
	case ProjLinearUnitsGeoKey: 
	case GeogAngularUnitsGeoKey: 
	case GeogAzimuthUnitsGeoKey: 
		                      info=_geounitsValue; break;

   	/* put other key-dependent lists here */
	case GTModelTypeGeoKey:       info=_modeltypeValue; break;
	case GTRasterTypeGeoKey:      info=_rastertypeValue; break;
	case GeographicTypeGeoKey:    info=_geographicValue; break;
	case GeogGeodeticDatumGeoKey: info=_geodeticdatumValue; break;
	case GeogEllipsoidGeoKey:     info=_ellipsoidValue; break;
	case GeogPrimeMeridianGeoKey: info=_primemeridianValue; break;
	case ProjectedCSTypeGeoKey:   info=_pcstypeValue; break;
	case ProjectionGeoKey:        info=_projectionValue; break;
	case ProjCoordTransGeoKey:    info=_coordtransValue; break;
	case VerticalCSTypeGeoKey:    info=_vertcstypeValue; break;
	case VerticalDatumGeoKey:     info=_vdatumValue; break;

	/* And if all else fails... */
   	default:                      info = _csdefaultValue;break;
   }
   
   return FindCode( info,name);
}

