/**********************************************************************
 *
 *  geo_set.c  -- Public routines for GEOTIFF GeoKey access.
 *
 *    Written By: Niles D. Ritter.
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 *
 **********************************************************************/

#include "geotiff.h"   /* public interface        */
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */

int GTIFKeySet(GTIF *gtif, geokey_t keyID, tagtype_t type, int count,...)
{
	va_list ap;
	int index = gtif->gt_keyindex[ keyID ];
	int newvalues = 0;
	GeoKey *key;
	char *data;
	char *val;
	pinfo_t sval;
	double dval;

	va_start(ap, count);
	/* pass singleton keys by value */
	if (count>1 && type!=TYPE_ASCII) val = va_arg(ap, char*);
	else switch (type)
	{
	    case TYPE_SHORT:  sval=va_arg(ap, int); val=(char *)&sval;     break;
	    case TYPE_DOUBLE: dval=va_arg(ap, dblparam_t); val=(char *)&dval;  break;
	    case TYPE_ASCII: 
			val=va_arg(ap, char*);
			count = strlen(val) + 1; /* force = string length */
			break;
	}
	va_end(ap);
	
	/* We assume here that there are no multi-valued SHORTS ! */
	if (index)
	{
		/* Key already exists */
		key = gtif->gt_keys+index;
		if (type!=key->gk_type || count > key->gk_count)
		{
			/* need to reset data pointer */
			key->gk_type = type;
			key->gk_count = count;
			key->gk_size = _gtiff_size[ type ];
			newvalues = 1;
		}
	}
	else
	{
		/* We need to create the key */
		if (gtif->gt_num_keys == MAX_KEYS) return 0;
		key = gtif->gt_keys + ++gtif->gt_num_keys;
		index = gtif->gt_num_keys;
		gtif->gt_keyindex[ keyID ] = index;
		key->gk_key = keyID;
		key->gk_type = type;
		key->gk_count = count;
		key->gk_size = _gtiff_size[ type ];
		if (gtif->gt_keymin > keyID)  gtif->gt_keymin=keyID;
		if (gtif->gt_keymax < keyID)  gtif->gt_keymax=keyID;
		newvalues = 1;
	}

	if (newvalues)
	{
	   switch (type)
	   {
	    case TYPE_SHORT:  
			if (count > 1) return 0;
			data = (char *)&key->gk_data; /* store value *in* data */
			break;
	    case TYPE_DOUBLE:
			key->gk_data = (char *)(gtif->gt_double + gtif->gt_ndoubles);
			data = key->gk_data;
			gtif->gt_ndoubles += count;
			break;
	    case TYPE_ASCII:
			key->gk_data = (char *)(gtif->gt_ascii + gtif->gt_nascii);
			data = key->gk_data;
			gtif->gt_nascii += count;
			data[--count] = '|'; /* replace NULL with '|' */
			break;
	    default:
			va_end(ap);
	    	return 0;
	    	break;
	  }
	   gtif->gt_nshorts += sizeof(KeyEntry)/sizeof(pinfo_t);
	}

	
	_GTIFmemcpy(data, val, count*key->gk_size);
	
	gtif->gt_flags |= FLAG_FILE_MODIFIED;
	return 1;
}
