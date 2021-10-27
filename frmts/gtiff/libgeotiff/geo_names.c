/*
 * geo_names.c
 *
 *  This encapsulates all of the value-naming mechanism of
 *  libgeotiff.
 *
 *  Written By: Niles Ritter
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 *
 */

#include "geotiffio.h"
#include "geonames.h"
#include "geo_tiffp.h" /* for tag names */
#include "geo_keyp.h"

#include "proj.h"

static const KeyInfo _formatInfo[] =  {
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

static const KeyInfo _tagInfo[] =  {
    {GTIFF_PIXELSCALE,  "ModelPixelScaleTag"},
    {GTIFF_TRANSMATRIX, "ModelTransformationTag"},
    {GTIFF_TIEPOINTS,   "ModelTiepointTag"},
     /* This alias maps the Intergraph symbol to the current tag */
    {GTIFF_TRANSMATRIX, "IntergraphMatrixTag"},
    END_LIST
};

static const char *FindName(const KeyInfo *info,int key)
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
   return (char*) FindName( &_keyInfo[0],key);
}

const char* GTIFKeyNameEx(GTIF* gtif, geokey_t key)
{
    const KeyInfo *info;
    if( gtif->gt_version == GEOTIFF_SPEC_1_0_VERSION &&
        gtif->gt_rev_major == GEOTIFF_SPEC_1_0_KEY_REVISION &&
        gtif->gt_rev_minor == GEOTIFF_SPEC_1_0_MINOR_REVISION )
    {
        info = &_keyInfo[0];
    }
    else
    {
        info = &_keyInfoV11[0];
    }
    while (info->ki_key>=0 && info->ki_key != (int)key) info++;
    if (info->ki_key<0)
    {
        sprintf(gtif->szTmpBufferForGTIFValueNameEx,"Unknown-%d", key );
        return gtif->szTmpBufferForGTIFValueNameEx;
    }
    return info->ki_name;
}

char *GTIFTypeName(tagtype_t type)
{
   return (char*) FindName( &_formatInfo[0],type);
}

char *GTIFTagName(int tag)
{
   return (char*) FindName( &_tagInfo[0],tag);
}

static const KeyInfo* FindTable(geokey_t key)
{
   const KeyInfo *info;

   switch (key)
   {
	/* All codes using linear/angular/whatever units */
	case GeogLinearUnitsGeoKey:
	case ProjLinearUnitsGeoKey:
	case GeogAngularUnitsGeoKey:
	case GeogAzimuthUnitsGeoKey:
	case VerticalUnitsGeoKey:
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

   return info;
}

char *GTIFValueName(geokey_t key, int value)
{

   return (char*) FindName(FindTable(key), value);
}

static void GetNameFromDatabase(GTIF* gtif,
                                const char* pszCode,
                                PJ_CATEGORY category,
                                char* pszOut,
                                size_t nOutSize)
{
    PJ* obj = proj_create_from_database(
        gtif->pj_context, "EPSG", pszCode, category,
        FALSE, NULL);
    if( obj )
    {
        const char* pszName = proj_get_name(obj);
        if( pszName )
        {
            size_t nToCopy = MIN(strlen(pszName), nOutSize - 1);
            memcpy(pszOut, pszName, nToCopy);
            pszOut[nToCopy] = 0;
        }
        proj_destroy(obj);
    }
    else
    {
        pszOut[0] = 0;
    }
}

const char *GTIFValueNameEx(GTIF* gtif, geokey_t key, int value)
{
    const KeyInfo *info = FindTable(key);
    int useHardcodedTables = 0;

    if( value == KvUndefined || value == KvUserDefined )
    {
        useHardcodedTables = 1;
    }
    else if( gtif->gt_version == GEOTIFF_SPEC_1_0_VERSION &&
             gtif->gt_rev_major == GEOTIFF_SPEC_1_0_KEY_REVISION &&
             gtif->gt_rev_minor == GEOTIFF_SPEC_1_0_MINOR_REVISION )
    {
        useHardcodedTables = 1;
    }
    else if( key == GTModelTypeGeoKey ||
             key == GTRasterTypeGeoKey ||
             key == ProjCoordTransGeoKey )
    {
        useHardcodedTables = 1;
    }
    else if( key == VerticalCSTypeGeoKey &&
             value >= 5001 && value <= 5033 )
    {
        useHardcodedTables = 1;
    }
    if( useHardcodedTables )
    {
        while (info->ki_key>=0 && info->ki_key != value) info++;
    }

    if ( !useHardcodedTables || info->ki_key<0 )
    {
        sprintf(gtif->szTmpBufferForGTIFValueNameEx,"Unknown-%d", value );

        if( gtif->pj_context == NULL )
        {
            gtif->pj_context = proj_context_create();
            if( gtif->pj_context )
            {
                gtif->own_pj_context = TRUE;
            }
        }
        if( gtif->pj_context )
        {
            char szCode[12];
            char szName[120];

            szName[0] = 0;
            sprintf(szCode, "%d", value);

            switch (key)
            {
                /* All codes using linear/angular/whatever units */
                case GeogLinearUnitsGeoKey:
                case ProjLinearUnitsGeoKey:
                case GeogAngularUnitsGeoKey:
                case GeogAzimuthUnitsGeoKey:
                case VerticalUnitsGeoKey:
                {
                    const char* pszName = NULL;
                    if( proj_uom_get_info_from_database(gtif->pj_context,
                         "EPSG", szCode, &pszName, NULL, NULL) && pszName )
                    {
                        strncpy(szName, pszName, sizeof(szName));
                        szName[sizeof(szName)-1] = 0;
                    }
                    break;
                }

                case GeogGeodeticDatumGeoKey:
                case VerticalDatumGeoKey:
                    GetNameFromDatabase(gtif, szCode, PJ_CATEGORY_DATUM,
                                        szName, sizeof(szName));
                    break;

                case GeogEllipsoidGeoKey:
                    GetNameFromDatabase(gtif, szCode, PJ_CATEGORY_ELLIPSOID,
                                        szName, sizeof(szName));
                    break;

                case GeogPrimeMeridianGeoKey:
                    GetNameFromDatabase(gtif, szCode,
                                        PJ_CATEGORY_PRIME_MERIDIAN,
                                        szName, sizeof(szName));
                    break;

                case GeographicTypeGeoKey:
                case ProjectedCSTypeGeoKey:
                case VerticalCSTypeGeoKey:
                    GetNameFromDatabase(gtif, szCode,
                                        PJ_CATEGORY_CRS,
                                        szName, sizeof(szName));
                    break;

                case ProjectionGeoKey:
                    GetNameFromDatabase(gtif, szCode,
                                        PJ_CATEGORY_COORDINATE_OPERATION,
                                        szName, sizeof(szName));
                    break;

                default:
                    break;
            }

            if( szName[0] != 0 )
            {
                sprintf(gtif->szTmpBufferForGTIFValueNameEx,
                        "Code-%d (%s)", value, szName );
            }

        }

        return gtif->szTmpBufferForGTIFValueNameEx;
    }
    return info->ki_name;
}

/*
 * Inverse Utilities (name->code)
 */


static int FindCode(const KeyInfo *info,const char *key)
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
	} else if (!strncmp(key,"Code-",5))
	{
		int code=-1;
		sscanf(key,"Code-%d",&code);
		return code;
	}
	else return -1;
   }
   return info->ki_key;
}

int GTIFKeyCode(const char *key)
{
   int ret = FindCode( &_keyInfo[0],key);
   if( ret < 0 )
       ret = FindCode( &_keyInfoV11[0],key);
   return ret;
}

int GTIFTypeCode(const char *type)
{
   return FindCode( &_formatInfo[0],type);
}

int GTIFTagCode(const char *tag)
{
   return FindCode( &_tagInfo[0],tag);
}


/*
 *  The key must be determined with GTIFKeyCode() before
 *  the name can be encoded.
 */
int GTIFValueCode(geokey_t key, const char *name)
{
   return FindCode(FindTable(key),name);
}
