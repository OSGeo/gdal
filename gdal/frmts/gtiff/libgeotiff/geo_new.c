/**********************************************************************
 *
 *  geo_new.c  -- Public routines for GEOTIFF GeoKey access.
 *
 *    Written By: Niles D. Ritter.
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 *
 *    20 June, 1995      Niles D. Ritter         New
 *    7 July,  1995      Greg Martin             Fix index
 *
 **********************************************************************/

#include "geotiffio.h"   /* public interface        */
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */
#include "geo_simpletags.h" 

/* private local routines */
static int ReadKey(GTIF* gt, TempKeyData* tempData,
                   KeyEntry* entptr, GeoKey* keyptr);


/**********************************************************************
 *
 *                        Public Routines
 *
 **********************************************************************/


/**
 * Given an open TIFF file, look for GTIF keys and 
 *  values and return GTIF structure.

This function creates a GeoTIFF information interpretation handle
(GTIF *) based on a passed in TIFF handle originally from 
XTIFFOpen().  Even though the argument 
(<b>tif</b>) is shown as type <tt>void *</tt>, it is really normally
of type <tt>TIFF *</tt>.<p>

The returned GTIF handle can be used to read or write GeoTIFF tags 
using the various GTIF functions.  The handle should be destroyed using
GTIFFree() before the file is closed with TIFFClose().<p>

If the file accessed has no GeoTIFF keys, an valid (but empty) GTIF is
still returned.  GTIFNew() is used both for existing files being read, and
for new TIFF files that will have GeoTIFF tags written to them.<p>

 */

GTIF* GTIFNew(void *tif)

{
    TIFFMethod default_methods;
    _GTIFSetDefaultTIFF( &default_methods );

    return GTIFNewWithMethods( tif, &default_methods );
}

GTIF *GTIFNewSimpleTags( void *tif )

{
    TIFFMethod default_methods;
    GTIFSetSimpleTagsMethods( &default_methods );

    return GTIFNewWithMethods( tif, &default_methods );
}

/************************************************************************/
/*                         GTIFNewWithMethods()                         */
/*                                                                      */
/*      Create a new geotiff, passing in the methods structure to       */
/*      support not libtiff implementations without replacing the       */
/*      default methods.                                                */
/************************************************************************/

GTIF* GTIFNewWithMethods(void *tif, TIFFMethod* methods)
{
    GTIF* gt=(GTIF*)0;
    int count,bufcount,index;
    GeoKey *keyptr;
    pinfo_t *data;
    KeyEntry *entptr;
    KeyHeader *header;
    TempKeyData tempData;

    memset( &tempData, 0, sizeof(tempData) );
    gt = (GTIF*)_GTIFcalloc( sizeof(GTIF));
    if (!gt) goto failure;	
	
    /* install TIFF file and I/O methods */
    gt->gt_tif = (tiff_t *)tif;
    memcpy( &gt->gt_methods, methods, sizeof(TIFFMethod) );

    /* since this is an array, GTIF will allocate the memory */
    if ( tif == NULL 
         || !(gt->gt_methods.get)(tif, GTIFF_GEOKEYDIRECTORY, &gt->gt_nshorts, &data ))
    {
        /* No ProjectionInfo, create a blank one */
        data=(pinfo_t*)_GTIFcalloc((4+MAX_VALUES)*sizeof(pinfo_t));
        if (!data) goto failure;	
        header = (KeyHeader *)data;
        header->hdr_version = GvCurrentVersion;
        header->hdr_rev_major = GvCurrentRevision;
        header->hdr_rev_minor = GvCurrentMinorRev;
        gt->gt_nshorts=sizeof(KeyHeader)/sizeof(pinfo_t);
    }
    else
    {
        /* resize data array so it can be extended if needed */
        data = (pinfo_t*) _GTIFrealloc(data,(4+MAX_VALUES)*sizeof(pinfo_t));
    }
    gt->gt_short = data;
    header = (KeyHeader *)data;
	
    if (header->hdr_version > GvCurrentVersion) goto failure;
    if (header->hdr_rev_major > GvCurrentRevision)
    {
        /* issue warning */
    }
	
    /* If we got here, then the geokey can be parsed */
    count = header->hdr_num_keys;

    if (count * sizeof(KeyEntry) >= (4 + MAX_VALUES) * sizeof(pinfo_t)) 
        goto failure; 

    gt->gt_num_keys = count;
    gt->gt_version  = header->hdr_version;
    gt->gt_rev_major  = header->hdr_rev_major;
    gt->gt_rev_minor  = header->hdr_rev_minor;

    bufcount = count+MAX_KEYS; /* allow for expansion */

    /* Get the PARAMS Tags, if any */
    if (tif == NULL
        || !(gt->gt_methods.get)(tif, GTIFF_DOUBLEPARAMS,
                                 &gt->gt_ndoubles, &gt->gt_double ))
    {
        gt->gt_double=(double*)_GTIFcalloc(MAX_VALUES*sizeof(double));
        if (!gt->gt_double) goto failure;	
    }
    else
    {
        /* resize data array so it can be extended if needed */
        gt->gt_double = (double*) _GTIFrealloc(gt->gt_double,
                                               (MAX_VALUES)*sizeof(double));
    }
    if ( tif == NULL
         || !(gt->gt_methods.get)(tif, GTIFF_ASCIIPARAMS,
                                  &tempData.tk_asciiParamsLength,
                                  &tempData.tk_asciiParams ))
    {
        tempData.tk_asciiParams         = 0;
        tempData.tk_asciiParamsLength   = 0;
    }
    else
    {
        /* last NULL doesn't count; "|" used for delimiter */
        if( tempData.tk_asciiParamsLength > 0 
            && tempData.tk_asciiParams[tempData.tk_asciiParamsLength-1] == '\0')
        {
            --tempData.tk_asciiParamsLength;
        }
    }

    /* allocate space for GeoKey array and its index */
    gt->gt_keys = (GeoKey *)_GTIFcalloc( sizeof(GeoKey)*bufcount);
    if (!gt->gt_keys) goto failure;
    gt->gt_keyindex = (int *)_GTIFcalloc( sizeof(int)*(MAX_KEYINDEX+1));
    if (!gt->gt_keyindex) goto failure;
	
    /*  Loop to get all GeoKeys */
    entptr = ((KeyEntry *)data) + 1;
    keyptr = gt->gt_keys;
    gt->gt_keymin = MAX_KEYINDEX;
    gt->gt_keymax = 0;
    for (index=1; index<=count; index++,entptr++)
    {
        if (!ReadKey(gt, &tempData, entptr, ++keyptr))
            goto failure;
			
        /* Set up the index (start at 1, since 0=unset) */
        gt->gt_keyindex[entptr->ent_key] = index;		
    }

    if( tempData.tk_asciiParams != NULL )
        _GTIFFree( tempData.tk_asciiParams );
	
    return gt;
	
  failure:
    /* Notify of error */
    if( tempData.tk_asciiParams != NULL )
        _GTIFFree( tempData.tk_asciiParams );    
    GTIFFree (gt);
    return (GTIF *)0;
}

/**********************************************************************
 *
 *                        Private Routines
 *
 **********************************************************************/

/*
 * Given KeyEntry, read in the GeoKey value location and set up
 *  the Key structure, returning 0 if failure.
 */

static int ReadKey(GTIF* gt, TempKeyData* tempData,
                   KeyEntry* entptr, GeoKey* keyptr)
{
    int offset,count;

    keyptr->gk_key = entptr->ent_key;
    keyptr->gk_count = entptr->ent_count;
    count = entptr->ent_count;
    offset = entptr->ent_val_offset;
    if (gt->gt_keymin > keyptr->gk_key)  gt->gt_keymin=keyptr->gk_key;
    if (gt->gt_keymax < keyptr->gk_key)  gt->gt_keymax=keyptr->gk_key;

    if (entptr->ent_location)
        keyptr->gk_type = (gt->gt_methods.type)(gt->gt_tif,entptr->ent_location);
    else
        keyptr->gk_type = (gt->gt_methods.type)(gt->gt_tif,GTIFF_GEOKEYDIRECTORY);

    switch (entptr->ent_location)
    {
        case GTIFF_LOCAL:
            /* store value into data value */
            memcpy(&keyptr->gk_data, &(entptr->ent_val_offset), sizeof(pinfo_t));
            break;
        case GTIFF_GEOKEYDIRECTORY:
            keyptr->gk_data = (char *)(gt->gt_short+offset);
            if (gt->gt_nshorts < offset+count)
                gt->gt_nshorts = offset+count;
            break;
        case GTIFF_DOUBLEPARAMS:
            keyptr->gk_data = (char *)(gt->gt_double+offset);
            if (gt->gt_ndoubles < offset+count)
                gt->gt_ndoubles = offset+count;
            break;
        case GTIFF_ASCIIPARAMS:
            if( offset + count == tempData->tk_asciiParamsLength + 1 
                && count > 0 )
            {
                /* some vendors seem to feel they should not use the 
                   terminating '|' char, but do include a terminating '\0'
                   which we lose in the low level reading code.  
                   If this is the case, drop the extra character */
                count--;
            }
            else if (offset < tempData->tk_asciiParamsLength
                     && offset + count > tempData->tk_asciiParamsLength )
            {
                count = tempData->tk_asciiParamsLength - offset;
                /* issue warning... if we could */
            }
            else if (offset + count > tempData->tk_asciiParamsLength)
                return (0);

            keyptr->gk_count = MAX(1,count+1);
            keyptr->gk_data = (char *) _GTIFcalloc (keyptr->gk_count);

            _GTIFmemcpy (keyptr->gk_data,
                         tempData->tk_asciiParams + offset, count);
            if( keyptr->gk_data[MAX(0,count-1)] == '|' )
            {
                keyptr->gk_data[MAX(0,count-1)] = '\0';
                keyptr->gk_count = count;
            }
            else
                keyptr->gk_data[MAX(0,count)] = '\0';
            break;
        default:
            return 0; /* failure */
    }
    keyptr->gk_size = _gtiff_size[keyptr->gk_type];
	
    return 1; /* success */
}
