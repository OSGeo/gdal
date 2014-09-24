/**********************************************************************
 *
 *  geo_write.c  -- Public routines for GEOTIFF GeoKey access.
 *
 *    Written By: Niles D. Ritter.
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any source code derived therefrom.
 *
 **********************************************************************/

#include "geotiffio.h"   /* public interface        */
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */

static int WriteKey(GTIF* gt, TempKeyData* tempData,
                    KeyEntry* entptr, GeoKey* keyptr);
static int SortKeys(GTIF* gt,int *sortkeys);


/**
This function flushes all the GeoTIFF keys that have been set with the 
GTIFKeySet() function into the associated 
TIFF file.

@param gt The GeoTIFF handle returned by GTIFNew.

GTIFWriteKeys() should be called before 
GTIFFree() is used to deallocate a GeoTIFF access handle.
 */

int GTIFWriteKeys(GTIF *gt)
{
    int i;
    GeoKey *keyptr;
    KeyEntry *entptr;
    KeyHeader *header;
    TempKeyData tempData;
    int sortkeys[MAX_KEYS];
	
    if (!(gt->gt_flags & FLAG_FILE_MODIFIED)) return 1;

    if( gt->gt_tif == NULL )
        return 0;
	
    tempData.tk_asciiParams = 0;
    tempData.tk_asciiParamsLength = 0;
    tempData.tk_asciiParamsOffset = 0;

    /*  Sort the Keys into numerical order */
    if (!SortKeys(gt,sortkeys))
    {
        /* XXX error: a key was not recognized */
    }
	
    /* Set up header of ProjectionInfo tag */
    header = (KeyHeader *)gt->gt_short;
    header->hdr_num_keys = (pinfo_t) gt->gt_num_keys;
    header->hdr_version  = GvCurrentVersion;
    header->hdr_rev_major  = GvCurrentRevision;
    header->hdr_rev_minor  = GvCurrentMinorRev;
	
    /* Sum up the ASCII tag lengths */
    for (i = 0; i < gt->gt_num_keys; i++)
    {
        keyptr = gt->gt_keys + sortkeys[i];
        if (keyptr->gk_type == TYPE_ASCII)
        {
            tempData.tk_asciiParamsLength += keyptr->gk_count;
        }
    }
    if (tempData.tk_asciiParamsLength > 0)
    {
        tempData.tk_asciiParams =
            (char *)_GTIFcalloc(tempData.tk_asciiParamsLength + 1);
        tempData.tk_asciiParams[tempData.tk_asciiParamsLength] = '\0';
    }

    /* Set up the rest of SHORT array properly */
    keyptr = gt->gt_keys;
    entptr = (KeyEntry*)(gt->gt_short + 4);
    for (i=0; i< gt->gt_num_keys; i++,entptr++)
    {
        if (!WriteKey(gt,&tempData,entptr,keyptr+sortkeys[i])) return 0;
    }	
	
    /* Write out the Key Directory */
    (gt->gt_methods.set)(gt->gt_tif, GTIFF_GEOKEYDIRECTORY, gt->gt_nshorts, gt->gt_short );	
	
    /* Write out the params directories */
    if (gt->gt_ndoubles)
        (gt->gt_methods.set)(gt->gt_tif, GTIFF_DOUBLEPARAMS, gt->gt_ndoubles, gt->gt_double );
    if (tempData.tk_asciiParamsLength > 0)
    {
        /* just to be safe */
        tempData.tk_asciiParams[tempData.tk_asciiParamsLength] = '\0';
        (gt->gt_methods.set)(gt->gt_tif,
                             GTIFF_ASCIIPARAMS, 0, tempData.tk_asciiParams);
    }
	
    gt->gt_flags &= ~FLAG_FILE_MODIFIED;

    if (tempData.tk_asciiParamsLength > 0)
    {
        _GTIFFree (tempData.tk_asciiParams);
    }
    return 1;
}

/**********************************************************************
 *
 *                        Private Routines
 *
 **********************************************************************/
 
/*
 * Given GeoKey, write out the KeyEntry entries, returning 0 if failure.
 *  This is the exact complement of ReadKey().
 */

static int WriteKey(GTIF* gt, TempKeyData* tempData,
                    KeyEntry* entptr, GeoKey* keyptr)
{
    int count;
	
    entptr->ent_key = (pinfo_t) keyptr->gk_key;
    entptr->ent_count = (pinfo_t) keyptr->gk_count;
    count = entptr->ent_count;
	
    if (count==1 && keyptr->gk_type==TYPE_SHORT)
    {
        entptr->ent_location = GTIFF_LOCAL;
        entptr->ent_val_offset = *(pinfo_t*)&keyptr->gk_data;
        return 1;
    }
		  
    switch (keyptr->gk_type)
    {
      case TYPE_SHORT:
        entptr->ent_location = GTIFF_GEOKEYDIRECTORY;
        entptr->ent_val_offset = (pinfo_t)
            ((pinfo_t*)keyptr->gk_data - gt->gt_short);
        break;
      case TYPE_DOUBLE:
        entptr->ent_location = GTIFF_DOUBLEPARAMS;
        entptr->ent_val_offset = (pinfo_t) 
            ((double*)keyptr->gk_data - gt->gt_double);
        break;
      case TYPE_ASCII:
        entptr->ent_location = GTIFF_ASCIIPARAMS;
        entptr->ent_val_offset = (pinfo_t) tempData->tk_asciiParamsOffset;
        _GTIFmemcpy (tempData->tk_asciiParams + tempData->tk_asciiParamsOffset
                     , keyptr->gk_data, keyptr->gk_count);
        tempData->tk_asciiParams[tempData->tk_asciiParamsOffset+keyptr->gk_count-1] = '|';
        tempData->tk_asciiParamsOffset += keyptr->gk_count;
        break;
      default:
        return 0; /* failure */
    }
	
    return 1; /* success */
}


/* 
 * Numerically sort the GeoKeys.
 * We just do a linear search through
 * the list and pull out the keys that were set.
 */

static int SortKeys(GTIF* gt,int *sortkeys)
{
    int i, did_work;

    for( i = 0; i < gt->gt_num_keys; i++ )
        sortkeys[i] = i+1;

    do {  /* simple bubble sort */
        did_work = 0;
        for( i = 0; i < gt->gt_num_keys-1; i++ )
        {
            if( gt->gt_keys[sortkeys[i]].gk_key 
                > gt->gt_keys[sortkeys[i+1]].gk_key )
            {
                /* swap keys in sort list */
                int j = sortkeys[i];
                sortkeys[i] = sortkeys[i+1];
                sortkeys[i+1] = j;
                
                did_work = 1;
            }
        }
    } while( did_work );

    return 1;
}

