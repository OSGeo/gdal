/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Low Level Envisat file access (read/write) API.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Atlantis Scientific, Inc.
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

#ifndef APP_BUILD
#  define GDAL_BUILD
#  include "cpl_conv.h"
#  include "EnvisatFile.h"

CPL_CVSID("$Id$");

#else
#  include "APP/app.h"
#  include "util/Files/EnvisatFile.h"
#endif

typedef struct 
{
    char	*ds_name;
    char	*ds_type;
    char	*filename;
    int		ds_offset;
    int		ds_size;
    int		num_dsr;
    int		dsr_size;
} EnvisatDatasetInfo;

typedef struct
{
    char	*key;
    char	*value;
    char	*units;
    char	*literal_line;
    int         value_offset;
} EnvisatNameValue;

struct EnvisatFile_tag
{
    VSILFILE	*fp;
    char        *filename;
    int		updatable;
    int         header_dirty;
    int		dsd_offset;

    int		mph_count;
    EnvisatNameValue **mph_entries;

    int		sph_count;
    EnvisatNameValue **sph_entries;

    int		ds_count;
    EnvisatDatasetInfo **ds_info;
    
};

#ifdef GDAL_BUILD
#  define SUCCESS 0
#  define FAILURE 1
#  define SendError( text )   CPLError( CE_Failure, CPLE_AppDefined, "%s", text )
#endif

#define MPH_SIZE 1247

/*
 * API For handling name/value lists.
 */
int S_NameValueList_Parse( const char *text, int text_offset, 
                           int *entry_count, 
                           EnvisatNameValue ***entries );
void S_NameValueList_Destroy( int *entry_count, 
                             EnvisatNameValue ***entries );
int S_NameValueList_FindKey( const char *key,
                             int entry_count, 
                             EnvisatNameValue **entries );
const char *S_NameValueList_FindValue( const char *key,
                                       int entry_count, 
                                       EnvisatNameValue **entries,
                                       const char * default_value );

int S_NameValueList_Rewrite( VSILFILE *fp, int entry_count, 
                             EnvisatNameValue **entries );

EnvisatNameValue *
    S_EnivsatFile_FindNameValue( EnvisatFile *self,
                                 EnvisatFile_HeaderFlag mph_or_sph,
                                 const char * key );




/*-----------------------------------------------------------------------------

Name:
    Envisat_SetupLevel0

Purpose:
    Patch up missing information about SPH, and datasets for incomplete 
    level 0 signal datasets.

Description:

Inputs:
    self -- Envisat file handle.

Outputs:

Returns:
    SUCCESS or FAILURE

-----------------------------------------------------------------------------*/

static int EnvisatFile_SetupLevel0( EnvisatFile *self )

{
    int	file_length;
    unsigned char header[68];
    EnvisatDatasetInfo *ds_info;

    self->dsd_offset = 0;
    self->ds_count = 1;
    self->ds_info = (EnvisatDatasetInfo **) 
        calloc(sizeof(EnvisatDatasetInfo*),self->ds_count);

    if( self->ds_info == NULL )
        return FAILURE;

    /*
     * Figure out how long the file is. 
     */

    VSIFSeekL( self->fp, 0, SEEK_END );
    file_length = (int) VSIFTellL( self->fp );
    
    /* 
     * Read the first record header, and verify the well known values.
     */
    VSIFSeekL( self->fp, 3203, SEEK_SET );
    VSIFReadL( header, 68, 1, self->fp );

    if( header[38] != 0 || header[39] != 0x1d
        || header[40] != 0 || header[41] != 0x54 )
    {
        SendError( "Didn't get expected Data Field Header Length, or Mode ID\n"
                   "values for the first data record." );
        return FAILURE;
    }

    /* 
     * Then build the dataset into structure from that. 
     */
    ds_info = (EnvisatDatasetInfo *) calloc(sizeof(EnvisatDatasetInfo),1);
    
    ds_info->ds_name = strdup( "ASAR SOURCE PACKETS         " );
    ds_info->ds_type = strdup( "M" );
    ds_info->filename = strdup( "                                                              " );
    ds_info->ds_offset = 3203;
    ds_info->dsr_size = -1;
    ds_info->num_dsr = 0;
    ds_info->ds_size = file_length - ds_info->ds_offset;
    
    self->ds_info[0] = ds_info;

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    Envisat_Open

Purpose:
    Open an ENVISAT formatted file, and read all headers.

Description:

Inputs:
    filename -- name of Envisat file.
    mode -- either "r" for read access, or "r+" for read/write access.

Outputs:
    self -- file handle, NULL on FAILURE.

Returns:
    SUCCESS or FAILURE

-----------------------------------------------------------------------------*/

int EnvisatFile_Open( EnvisatFile **self_ptr, 
                      const char *filename, 
                      const char *mode )

{
    VSILFILE	*fp;
    EnvisatFile	*self;
    char	mph_data[1248];
    char	*sph_data, *ds_data;
    int		sph_size, num_dsd, dsd_size, i;

    *self_ptr = NULL;

    /*
     * Check for legal mode argument.  Force to be binary for correct
     * operation on DOS file systems.
     */
    if( strcmp(mode,"r") == 0 )
        mode = "rb";
    else if( strcmp(mode,"r+") == 0 )
        mode = "rb+";
    else
    {
        SendError( "Illegal mode value used in EnvisatFile_Open(), only "
                   "\"r\" and \"r+\" are supported." );
        return FAILURE;
    }

    /*
     * Try to open the file, and report failure. 
     */

    fp = VSIFOpenL( filename, mode );

    if( fp == NULL )
    {
        char	error_buf[2048];

        sprintf( error_buf, 
                 "Unable to open file \"%s\" in EnvisatFile_Open().", 
                 filename );

        SendError( error_buf );
        return FAILURE;
    }

    /*
     * Create, and initialize the EnvisatFile structure. 
     */
    self = (EnvisatFile *) calloc(sizeof(EnvisatFile),1);
    if( self == NULL )
        return FAILURE;

    self->fp = fp;
    self->filename = strdup( filename );
    self->header_dirty = 0;
    self->updatable = (strcmp(mode,"rb+") == 0);

    /*
     * Read the MPH, and process it as a group of name/value pairs. 
     */

    if( VSIFReadL( mph_data, 1, MPH_SIZE, fp ) != MPH_SIZE )
    {
        free( self );
        SendError( "VSIFReadL() for mph failed." );
        return FAILURE;
    }

    mph_data[MPH_SIZE] = '\0';
    if( S_NameValueList_Parse( mph_data, 0, 
                               &(self->mph_count), 
                               &(self->mph_entries) ) == FAILURE )
        return FAILURE;

    /*
     * Is this an incomplete level 0 file?
     */
    if( EnvisatFile_GetKeyValueAsInt( self, MPH, "SPH_SIZE", -1 ) == 0 
        && strncmp(EnvisatFile_GetKeyValueAsString( self, MPH, "PRODUCT", ""),
                   "ASA_IM__0P", 10) == 0 )
    {

        if( EnvisatFile_SetupLevel0( self ) == FAILURE )
        {
            EnvisatFile_Close( self );
            return FAILURE;
        }
        else
        {
            *self_ptr = self;
            return SUCCESS;
        }
    }

    /*
     * Read the SPH, and process it as a group of name/value pairs.  
     */
    sph_size = EnvisatFile_GetKeyValueAsInt( self, MPH, "SPH_SIZE", 0 );

    if( sph_size == 0 )							
    {
        SendError( "File does not appear to have SPH,"
                   " SPH_SIZE not set, or zero." );
        return FAILURE;
    }

    sph_data = (char *) malloc(sph_size + 1 );
    if( sph_data == NULL )
        return FAILURE;

    if( (int) VSIFReadL( sph_data, 1, sph_size, fp ) != sph_size )
    {
        free( self );
        SendError( "VSIFReadL() for sph failed." );
        return FAILURE;
    }

    sph_data[sph_size] = '\0';
    ds_data = strstr(sph_data,"DS_NAME");
    if( ds_data != NULL )
    {
        self->dsd_offset = (int) (ds_data - sph_data) + MPH_SIZE;
        *(ds_data-1) = '\0';
    }

    if( S_NameValueList_Parse( sph_data, MPH_SIZE,
                               &(self->sph_count), 
                               &(self->sph_entries) ) == FAILURE )
        return FAILURE;

    /*
     * Parse the Dataset Definitions.
     */
    num_dsd = EnvisatFile_GetKeyValueAsInt( self, MPH, "NUM_DSD", 0 );
    dsd_size = EnvisatFile_GetKeyValueAsInt( self, MPH, "DSD_SIZE", 0 );
    
    if( num_dsd > 0 && ds_data == NULL )
    {
        SendError( "DSDs indicated in MPH, but not found in SPH." );
        return FAILURE;
    }

    self->ds_info = (EnvisatDatasetInfo **) 
        calloc(sizeof(EnvisatDatasetInfo*),num_dsd);
    if( self->ds_info == NULL )
        return FAILURE;

    for( i = 0; i < num_dsd; i++ )
    {
        int	dsdh_count = 0;
        EnvisatNameValue **dsdh_entries = NULL;
        char	*dsd_data;
        EnvisatDatasetInfo *ds_info;

        /*
         * We parse each DSD grouping into a name/value list. 
         */
        dsd_data = ds_data + i * dsd_size;
        dsd_data[dsd_size-1] = '\0';
        
        if( S_NameValueList_Parse( dsd_data, 0, 
                                   &dsdh_count, &dsdh_entries ) == FAILURE )
            return FAILURE;

        /* 
         * Then build the dataset into structure from that. 
         */
        ds_info = (EnvisatDatasetInfo *) calloc(sizeof(EnvisatDatasetInfo),1);

        ds_info->ds_name = strdup( 
            S_NameValueList_FindValue( "DS_NAME", 
                                       dsdh_count, dsdh_entries, "" ));
        ds_info->ds_type = strdup( 
            S_NameValueList_FindValue( "DS_TYPE", 
                                       dsdh_count, dsdh_entries, "" ));
        ds_info->filename = strdup( 
            S_NameValueList_FindValue( "FILENAME", 
                                       dsdh_count, dsdh_entries, "" ));
        ds_info->ds_offset = atoi(
            S_NameValueList_FindValue( "DS_OFFSET", 
                                       dsdh_count, dsdh_entries, "0" ));
        ds_info->ds_size = atoi(
            S_NameValueList_FindValue( "DS_SIZE", 
                                       dsdh_count, dsdh_entries, "0" ));
        ds_info->num_dsr = atoi(
            S_NameValueList_FindValue( "NUM_DSR", 
                                       dsdh_count, dsdh_entries, "0" ));
        ds_info->dsr_size = atoi(
            S_NameValueList_FindValue( "DSR_SIZE", 
                                       dsdh_count, dsdh_entries, "0" ));

        S_NameValueList_Destroy( &dsdh_count, &dsdh_entries );

        self->ds_info[i] = ds_info;
        self->ds_count++;
    }
    
    free( sph_data );

    /*
     * Return successfully.
     */
    *self_ptr = self;

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_Create

Purpose:
    Create a new ENVISAT formatted file based on a template file.

Description:

Inputs:
    filename -- name of Envisat file.
    template_file -- name of envisat file header to utilize as template. 

Outputs:
    self -- file handle, NULL on FAILURE.

Returns:
    SUCCESS or FAILURE

-----------------------------------------------------------------------------*/

int EnvisatFile_Create( EnvisatFile **self_ptr, 
                        const char *filename, 
                        const char *template_file )

{
    int		template_size;
    char	*template_data;
    VSILFILE	*fp;

    /*
     * Try to open the template file, and read it into memory.
     */

    fp = VSIFOpenL( template_file, "rb" );

    if( fp == NULL )
    {
        char	error_buf[2048];

        sprintf( error_buf, 
                 "Unable to open file \"%s\" in EnvisatFile_Create().", 
                 template_file );

        SendError( error_buf );
        return FAILURE;
    }

    VSIFSeekL( fp, 0, SEEK_END );
    template_size = (int) VSIFTellL( fp );

    template_data = (char *) malloc(template_size);
    
    VSIFSeekL( fp, 0, SEEK_SET );
    VSIFReadL( template_data, template_size, 1, fp );
    VSIFCloseL( fp );

    /*
     * Try to write the template out to the new filename. 
     */
    
    fp = VSIFOpenL( filename, "wb" );
    if( fp == NULL )
    {
        char	error_buf[2048];

        sprintf( error_buf, 
                 "Unable to open file \"%s\" in EnvisatFile_Create().", 
                 filename );

        SendError( error_buf );
        return FAILURE;
    }

    VSIFWriteL( template_data, template_size, 1, fp );
    VSIFCloseL( fp );

    free( template_data );

    /*
     * Now just open the file normally. 
     */
    
    return EnvisatFile_Open( self_ptr, filename, "r+" );
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetCurrentLength

Purpose:
    Fetch the current file length.

Description:
    The length is computed by scanning the dataset definitions, not the
    physical file length.  

Inputs:
    self -- the file to operate on. 

Outputs:

Returns:
    Returns length or -1 on failure.

-----------------------------------------------------------------------------*/

int EnvisatFile_GetCurrentLength( EnvisatFile *self )

{
    int		length;
    int		ds;
    int		ds_offset;
    int         ds_size;

    length = MPH_SIZE 
        + EnvisatFile_GetKeyValueAsInt( self, MPH, "SPH_SIZE", 0 );

    for( ds = 0; 
         EnvisatFile_GetDatasetInfo( self, ds, NULL, NULL, NULL, 
                                     &ds_offset, &ds_size, NULL, NULL )
             != FAILURE; 
         ds++ )
    {
        if( ds_offset != 0 && (ds_offset+ds_size) > length )
            length = ds_offset + ds_size;
    }

    return length;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_RewriteHeader

Purpose:
    Update the envisat file header on disk to match the in-memory image.

Description:

Inputs:
    self -- handle for file to close.

Outputs:

Returns:
    SUCCESS or FAILURE.

-----------------------------------------------------------------------------*/

static int EnvisatFile_RewriteHeader( EnvisatFile *self )

{
    int		dsd, dsd_size;

    /* 
     * Rewrite MPH and SPH headers.
     */
    if( S_NameValueList_Rewrite( self->fp, 
                        self->mph_count, self->mph_entries ) == FAILURE )
        return FAILURE;

    if( S_NameValueList_Rewrite( self->fp, 
                        self->sph_count, self->sph_entries ) == FAILURE )
        return FAILURE;

    /*
     * Rewrite DSDs.  We actually have to read each, and reparse to set
     * the individual parameters properly.
     */
    dsd_size = EnvisatFile_GetKeyValueAsInt( self, MPH, "DSD_SIZE", 0 );
    if( dsd_size == 0 )
        return FAILURE;

    for( dsd = 0; dsd < self->ds_count; dsd++ )
    {
        char	*dsd_text;
        int	dsdh_count = 0, key_index;
        EnvisatNameValue **dsdh_entries = NULL;

        dsd_text = (char *) calloc(1,dsd_size+1);
        if( VSIFSeekL( self->fp, self->dsd_offset + dsd * dsd_size, 
                   SEEK_SET ) != 0 )
        {
            SendError( "VSIFSeekL() failed in EnvisatFile_RewriteHeader()" );
            return FAILURE;
        }
        
        if( (int) VSIFReadL( dsd_text, 1, dsd_size, self->fp ) != dsd_size )
        {
            SendError( "VSIFReadL() failed in EnvisatFile_RewriteHeader()" );
            return FAILURE;
        }

        if( S_NameValueList_Parse( dsd_text, self->dsd_offset + dsd*dsd_size, 
                                   &dsdh_count, &dsdh_entries ) == FAILURE )
            return FAILURE;

        free( dsd_text );

        key_index = S_NameValueList_FindKey( "DS_OFFSET", 
                                             dsdh_count, dsdh_entries );
        if( key_index == -1 )
            continue;

        sprintf( dsdh_entries[key_index]->value, "%+021d", 
                 self->ds_info[dsd]->ds_offset );

        key_index = S_NameValueList_FindKey( "DS_SIZE", 
                                             dsdh_count, dsdh_entries );
        sprintf( dsdh_entries[key_index]->value, "%+021d", 
                 self->ds_info[dsd]->ds_size );

        key_index = S_NameValueList_FindKey( "NUM_DSR", 
                                             dsdh_count, dsdh_entries );
        sprintf( dsdh_entries[key_index]->value, "%+011d", 
                 self->ds_info[dsd]->num_dsr );

        key_index = S_NameValueList_FindKey( "DSR_SIZE", 
                                             dsdh_count, dsdh_entries );
        sprintf( dsdh_entries[key_index]->value, "%+011d", 
                 self->ds_info[dsd]->dsr_size );

        if( S_NameValueList_Rewrite( self->fp, dsdh_count, dsdh_entries )
            == FAILURE )
            return FAILURE;

        S_NameValueList_Destroy( &dsdh_count, &dsdh_entries );
    }

    self->header_dirty = 0;

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_Close

Purpose:
    Close an ENVISAT formatted file, releasing all associated resources.

Description:

Inputs:
    self -- handle for file to close.

Outputs:

Returns:


-----------------------------------------------------------------------------*/

void EnvisatFile_Close( EnvisatFile *self )

{
    int		i;

    /*
     * Do we need to write out the header information?
     */
    if( self->header_dirty )
        EnvisatFile_RewriteHeader( self );

    /*
     * Close file. 
     */
    if( self->fp != NULL )
        VSIFCloseL( self->fp );

    /*
     * Clean up data structures. 
     */
    S_NameValueList_Destroy( &(self->mph_count), &(self->mph_entries) );
    S_NameValueList_Destroy( &(self->sph_count), &(self->sph_entries) );

    for( i = 0; i < self->ds_count; i++ )
    {
        if( self->ds_info != NULL && self->ds_info[i] != NULL )
        {
            free( self->ds_info[i]->ds_name );
            free( self->ds_info[i]->ds_type );
            free( self->ds_info[i]->filename );
            free( self->ds_info[i] );
        }
    }
    if( self->ds_info != NULL )
        free( self->ds_info );
    if( self->filename != NULL )
        free( self->filename );

    free( self );
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetFilename

Purpose:
    Fetch name of this Envisat file.

Description:

Inputs:
    self -- handle for file to get name of.

Outputs:

Returns:
    const pointer to internal copy of the filename.  Do not alter or free.


-----------------------------------------------------------------------------*/

const char *EnvisatFile_GetFilename( EnvisatFile *self )

{
    return self->filename;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetKeyByIndex()

Purpose:
    Fetch the key with the indicated index.

Description:
    This function can be used to "discover" the set of available keys by
    by scanning with index values starting at zero and ending when a NULL
    is returned.

Inputs:
    self -- the file to be searched.
    mph_or_sph -- Either MPH or SPH depending on the header to be searched.
    key_index -- key index, from zero to number of keys-1.

Outputs:

Returns:
    pointer to key name or NULL on failure.

-----------------------------------------------------------------------------*/

const char *EnvisatFile_GetKeyByIndex( EnvisatFile *self, 
                                       EnvisatFile_HeaderFlag mph_or_sph,
                                       int key_index )

{
    int	entry_count;
    EnvisatNameValue **entries;

    /*
     * Select source list. 
     */
    if( mph_or_sph == MPH )
    {
        entry_count = self->mph_count;
        entries = self->mph_entries;
    }
    else
    {
        entry_count = self->sph_count;
        entries = self->sph_entries;
    }

    if( key_index < 0 || key_index >= entry_count )
        return NULL;
    else
        return entries[key_index]->key;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetKeyValueAsString()

Purpose:
    Fetch the value associated with the indicated key as a string.

Description:

Inputs:
    self -- the file to be searched.
    mph_or_sph -- Either MPH or SPH depending on the header to be searched.
    key -- the key (name) to be searched for.
    default_value -- the value to return if the key is not found.

Outputs:

Returns:
    pointer to value string, or default_value if not found.

-----------------------------------------------------------------------------*/

const char *EnvisatFile_GetKeyValueAsString( EnvisatFile *self, 
                                             EnvisatFile_HeaderFlag mph_or_sph,
                                             const char *key,
                                             const char *default_value )

{
    int	entry_count, key_index;
    EnvisatNameValue **entries;

    /*
     * Select source list. 
     */
    if( mph_or_sph == MPH )
    {
        entry_count = self->mph_count;
        entries = self->mph_entries;
    }
    else
    {
        entry_count = self->sph_count;
        entries = self->sph_entries;
    }

    /*
     * Find and return the value.
     */
    key_index = S_NameValueList_FindKey( key, entry_count, entries );
    if( key_index == -1 )
        return default_value;
    else
        return entries[key_index]->value;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_SetKeyValueAsString()

Purpose:
    Set the value associated with the indicated key as a string.

Description:

Inputs:
    self -- the file to be searched.
    mph_or_sph -- Either MPH or SPH depending on the header to be searched.
    key -- the key (name) to be searched for.
    value -- the value to assign.

Outputs:

Returns:
    SUCCESS or FAILURE.

-----------------------------------------------------------------------------*/

int EnvisatFile_SetKeyValueAsString( EnvisatFile *self, 
                                     EnvisatFile_HeaderFlag mph_or_sph,
                                     const char *key,
                                     const char *value )

{
    int	entry_count, key_index;
    EnvisatNameValue **entries;

    if( !self->updatable )
    {
        SendError( "File not opened for update access." );
        return FAILURE;
    }

    /*
     * Select source list. 
     */
    if( mph_or_sph == MPH )
    {
        entry_count = self->mph_count;
        entries = self->mph_entries;
    }
    else
    {
        entry_count = self->sph_count;
        entries = self->sph_entries;
    }

    /*
     * Find and return the value.
     */
    key_index = S_NameValueList_FindKey( key, entry_count, entries );
    if( key_index == -1 )
    {
        char	error_buf[2048];

        sprintf( error_buf, 
                 "Unable to set header field \"%s\", field not found.", 
                 key );

        SendError( error_buf );
        return FAILURE;
    }

    self->header_dirty = 1;
    if( strlen(value) > strlen(entries[key_index]->value) )
    {
        strncpy( entries[key_index]->value, value, 
                 strlen(entries[key_index]->value) );
    }
    else
    {
        memset( entries[key_index]->value, ' ', 
                strlen(entries[key_index]->value) );
        strncpy( entries[key_index]->value, value, strlen(value) );
    }

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetKeyValueAsInt()

Purpose:
    Fetch the value associated with the indicated key as an integer.

Description:

Inputs:
    self -- the file to be searched.
    mph_or_sph -- Either MPH or SPH depending on the header to be searched.
    key -- the key (name) to be searched for.
    default_value -- the value to return if the key is not found.

Outputs:

Returns:
    key value, or default_value if key not found.

-----------------------------------------------------------------------------*/

int EnvisatFile_GetKeyValueAsInt( EnvisatFile *self, 
                                  EnvisatFile_HeaderFlag mph_or_sph,
                                  const char *key,
                                  int default_value )

{
    int	entry_count, key_index;
    EnvisatNameValue **entries;

    /*
     * Select source list. 
     */
    if( mph_or_sph == MPH )
    {
        entry_count = self->mph_count;
        entries = self->mph_entries;
    }
    else
    {
        entry_count = self->sph_count;
        entries = self->sph_entries;
    }

    /*
     * Find and return the value.
     */
    key_index = S_NameValueList_FindKey( key, entry_count, entries );
    if( key_index == -1 )
        return default_value;
    else
        return atoi(entries[key_index]->value);
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_SetKeyValueAsInt()

Purpose:
    Set the value associated with the indicated key as an integer.

Description:

Inputs:
    self -- the file to be searched.
    mph_or_sph -- Either MPH or SPH depending on the header to be searched.
    key -- the key (name) to be searched for.
    value -- the value to assign.

Outputs:

Returns:
    SUCCESS or FAILURE.

-----------------------------------------------------------------------------*/

int EnvisatFile_SetKeyValueAsInt( EnvisatFile *self, 
                                  EnvisatFile_HeaderFlag mph_or_sph,
                                  const char *key,
                                  int value )

{
    char format[32], string_value[128];
    const char *prototype_value;


    prototype_value = EnvisatFile_GetKeyValueAsString( self, mph_or_sph, key, NULL);
    if( prototype_value == NULL )
    {
        char	error_buf[2048];

        sprintf( error_buf, 
                 "Unable to set header field \"%s\", field not found.", 
                 key );

        SendError( error_buf );
        return FAILURE;
    }

    sprintf( format, "%%+0%dd", (int) strlen(prototype_value) );
    sprintf( string_value, format, value );

    return EnvisatFile_SetKeyValueAsString( self, mph_or_sph, key, string_value );
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetKeyValueAsDouble()

Purpose:
    Fetch the value associated with the indicated key as a double.

Description:

Inputs:
    self -- the file to be searched.
    mph_or_sph -- Either MPH or SPH depending on the header to be searched.
    key -- the key (name) to be searched for.
    default_value -- the value to return if the key is not found.

Outputs:

Returns:
    key value, or default_value if key not found.

-----------------------------------------------------------------------------*/

double EnvisatFile_GetKeyValueAsDouble( EnvisatFile *self, 
                                        EnvisatFile_HeaderFlag mph_or_sph,
                                        const char *key,
                                        double default_value )

{
    int	entry_count, key_index;
    EnvisatNameValue **entries;

    /*
     * Select source list. 
     */
    if( mph_or_sph == MPH )
    {
        entry_count = self->mph_count;
        entries = self->mph_entries;
    }
    else
    {
        entry_count = self->sph_count;
        entries = self->sph_entries;
    }

    /*
     * Find and return the value.
     */
    key_index = S_NameValueList_FindKey( key, entry_count, entries );
    if( key_index == -1 )
        return default_value;
    else
        return atof(entries[key_index]->value);
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_SetKeyValueAsDouble()

Purpose:
    Set the value associated with the indicated key as a double.

Description:
    Note that this function attempts to format the new value similarly to
    the previous value.  In some cases (expecially exponential values) this 
    may not work out well.  In case of problems the caller is encourage to
    format the value themselves, and use the EnvisatFile_SetKeyValueAsString
    function, but taking extreme care about the string length.

Inputs:
    self -- the file to be searched.
    mph_or_sph -- Either MPH or SPH depending on the header to be searched.
    key -- the key (name) to be searched for.
    value -- the value to assign.

Outputs:

Returns:
    SUCCESS or FAILURE.

-----------------------------------------------------------------------------*/

int EnvisatFile_SetKeyValueAsDouble( EnvisatFile *self, 
                                     EnvisatFile_HeaderFlag mph_or_sph,
                                     const char *key,
                                     double value )

{
    char format[32], string_value[128];
    const char *prototype_value;
    int		length;

    prototype_value = EnvisatFile_GetKeyValueAsString( self, mph_or_sph, key, NULL);
    if( prototype_value == NULL )
    {
        char	error_buf[2048];

        sprintf( error_buf, 
                 "Unable to set header field \"%s\", field not found.", 
                 key );

        SendError( error_buf );
        return FAILURE;
    }

    length = strlen(prototype_value);
    if( prototype_value[length-4] == 'E' )
    {
        sprintf( format, "%%+%dE", length-4 );
        sprintf( string_value, format, value );
    }
    else
    {
        int	decimals = 0, i;
        for( i = length-1; i > 0; i-- )
        {
            if( prototype_value[i] == '.' )
                break;
            
            decimals++;
        }

        sprintf( format, "%%+0%d.%df", length, decimals );
        sprintf( string_value, format, value );

        if( (int)strlen(string_value) > length )
            string_value[length] = '\0';
    }

    return EnvisatFile_SetKeyValueAsString( self, mph_or_sph, key, string_value );
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetDatasetIndex()

Purpose:
    Fetch the datasat index give a dataset name.

Description:
    The provided name is extended with spaces, so it isn't necessary for the
    application to pass all the passing spaces.

Inputs:
    self -- the file to be searched.
    ds_name -- the name (DS_NAME) of the dataset to find.

Outputs:

Returns:
    Dataset index that matches, or -1 if none found.

-----------------------------------------------------------------------------*/

int EnvisatFile_GetDatasetIndex( EnvisatFile *self, const char *ds_name )

{
    int		i;
    char	padded_ds_name[100];

    /* 
     * Padd the name.  While the normal product spec says the DS_NAME will
     * be 28 characters, I try to pad more than this incase the specification
     * is changed. 
     */
    strncpy( padded_ds_name, ds_name, sizeof(padded_ds_name) );
    padded_ds_name[sizeof(padded_ds_name)-1] = 0;
    for( i = strlen(padded_ds_name); i < sizeof(padded_ds_name)-1; i++ )
    {
        padded_ds_name[i] = ' ';
    }
    padded_ds_name[i] = '\0';

    /* 
     * Compare only for the full length of DS_NAME we have saved.
     */
    for( i = 0; i < self->ds_count; i++ )
    {
        if( strncmp( padded_ds_name, self->ds_info[i]->ds_name, 
                     strlen(self->ds_info[i]->ds_name) ) == 0 )
        {
            return i;
        }
    }

    return -1;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_GetDatasetInfo()

Purpose:
    Fetch the information associated with a dataset definition.

Description:
    The returned strings are pointers to internal copies, and should not be
    modified, or freed.  Note, any of the "output" parameters can safely be
    NULL if it is not needed.

Inputs:
    self -- the file to be searched.
    ds_index -- the dataset index to fetch

Outputs:
    ds_name -- the dataset symbolic name, ie 'MDS1 SQ ADS              '.
    ds_type -- the dataset type, ie. 'A', not sure of valid values.
    filename -- dataset filename, normally spaces, or 'NOT USED          '.
    ds_offset -- the byte offset in the whole file to the first byte of 
                 dataset data.  This is 0 for unused datasets.
    ds_size -- the size, in bytes, of the whole dataset.
    num_dsr -- the number of records in the dataset.
    dsr_size -- the size of one record in the dataset in bytes, -1 if
                records are variable sized.

Returns:
    SUCCESS if dataset exists, or FAILURE if ds_index is out of range.

-----------------------------------------------------------------------------*/

int EnvisatFile_GetDatasetInfo( EnvisatFile *self, 
                                int ds_index, 
                                char **ds_name, 
                                char **ds_type,
                                char **filename,
                                int  *ds_offset,
                                int  *ds_size,
                                int  *num_dsr,
                                int  *dsr_size )

{
    if( ds_index < 0 || ds_index >= self->ds_count )
        return FAILURE;

    if( ds_name != NULL )
        *ds_name = self->ds_info[ds_index]->ds_name;
    if( ds_type != NULL )
        *ds_type = self->ds_info[ds_index]->ds_type;
    if( filename != NULL )
        *filename = self->ds_info[ds_index]->filename;
    if( ds_offset != NULL )
        *ds_offset = self->ds_info[ds_index]->ds_offset;
    if( ds_size != NULL )
        *ds_size = self->ds_info[ds_index]->ds_size;
    if( num_dsr != NULL )
        *num_dsr = self->ds_info[ds_index]->num_dsr;
    if( dsr_size != NULL )
        *dsr_size = self->ds_info[ds_index]->dsr_size;

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_SetDatasetInfo()

Purpose:
    Update the information associated with a dataset definition.

Description:

Inputs:
    self -- the file to be searched.
    ds_index -- the dataset index to fetch
    ds_offset -- the byte offset in the whole file to the first byte of 
                 dataset data.  This is 0 for unused datasets.
    ds_size -- the size, in bytes, of the whole dataset.
    num_dsr -- the number of records in the dataset.
    dsr_size -- the size of one record in the dataset in bytes, -1 if
                records are variable sized.

Outputs:

Returns:
    SUCCESS or FAILURE.

-----------------------------------------------------------------------------*/

int EnvisatFile_SetDatasetInfo( EnvisatFile *self, 
                                int ds_index, 
                                int ds_offset,
                                int ds_size,
                                int num_dsr,
                                int dsr_size )

{
    if( ds_index < 0 || ds_index >= self->ds_count )
        return FAILURE;

    self->ds_info[ds_index]->ds_offset = ds_offset;
    self->ds_info[ds_index]->ds_size = ds_size;
    self->ds_info[ds_index]->num_dsr = num_dsr;
    self->ds_info[ds_index]->dsr_size = dsr_size;
    self->header_dirty = 1;

    return SUCCESS;
}


/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_ReadDatasetChunk()

Purpose:
    Read an arbitrary chunk of a dataset.

Description:
    Note that no range checking is made on offset and size, and data may be
    read from outside the dataset if they are inappropriate.

Inputs:
    self -- the file to be searched.
    ds_index -- the index of dataset to access.
    offset -- byte offset within database to read.
    size -- size of buffer to fill in bytes.
    buffer -- buffer to load data into

Outputs:
    buffer is updated on SUCCESS.

Returns:
    SUCCESS or FAILURE

-----------------------------------------------------------------------------*/

int EnvisatFile_ReadDatasetChunk( EnvisatFile *self, 
                                  int ds_index,
                                  int offset,
                                  int size,
                                  void * buffer )

{
    if( ds_index < 0 || ds_index >= self->ds_count )
    {
        SendError( "Attempt to read non-existant dataset in "
                   "EnvisatFile_ReadDatasetChunk()" );
        return FAILURE;
    }

    if( offset < 0 
        || offset + size > self->ds_info[ds_index]->ds_size )
    {
        SendError( "Attempt to read beyond end of dataset in "
                   "EnvisatFile_ReadDatasetChunk()" );
        return FAILURE;
    }

    if( VSIFSeekL( self->fp, self->ds_info[ds_index]->ds_offset+offset, SEEK_SET )
        != 0 )
    {
        SendError( "seek failed in EnvisatFile_ReadChunk()" );
        return FAILURE;
    }

    if( (int) VSIFReadL( buffer, 1, size, self->fp ) != size )
    {
        SendError( "read failed in EnvisatFile_ReadChunk()" );
        return FAILURE;
    }

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_WriteDatasetRecord()

Purpose:
    Write an arbitrary dataset record.

Description:
    Note that no range checking is made on offset and size, and data may be
    read from outside the dataset if they are inappropriate.

Inputs:
    self -- the file to be searched.
    ds_index -- the index of dataset to access.
    record_index -- the record to write.
    record_buffer -- buffer to load data into

Outputs:

Returns:
    SUCCESS or FAILURE

-----------------------------------------------------------------------------*/

int EnvisatFile_WriteDatasetRecord( EnvisatFile *self, 
                                    int ds_index,
                                    int record_index,
                                    void *buffer )

{
    int		absolute_offset;
    int         result;

    if( ds_index < 0 || ds_index >= self->ds_count )
    {
        SendError( "Attempt to write non-existant dataset in "
                   "EnvisatFile_WriteDatasetRecord()" );
        return FAILURE;
    }

    if( record_index < 0
        || record_index >=  self->ds_info[ds_index]->num_dsr )
    {
        SendError( "Attempt to write beyond end of dataset in "
                   "EnvisatFile_WriteDatasetRecord()" );
        return FAILURE;
    }

    absolute_offset = self->ds_info[ds_index]->ds_offset
        + record_index * self->ds_info[ds_index]->dsr_size;

    if( VSIFSeekL( self->fp, absolute_offset, SEEK_SET ) != 0 )
    {
        SendError( "seek failed in EnvisatFile_WriteDatasetRecord()" );
        return FAILURE;
    }

    result = VSIFWriteL( buffer, 1, self->ds_info[ds_index]->dsr_size, self->fp );
    if( result != self->ds_info[ds_index]->dsr_size )
    {
        SendError( "write failed in EnvisatFile_WriteDatasetRecord()" );
        return FAILURE;
    }

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    EnvisatFile_ReadDatasetRecord()

Purpose:
    Read an arbitrary dataset record.

Description:
    Note that no range checking is made on offset and size, and data may be
    read from outside the dataset if they are inappropriate.

Inputs:
    self -- the file to be searched.
    ds_index -- the index of dataset to access.
    record_index -- the record to write.
    record_buffer -- buffer to load data into

Outputs:

Returns:
    SUCCESS or FAILURE

-----------------------------------------------------------------------------*/

int EnvisatFile_ReadDatasetRecord( EnvisatFile *self, 
                                    int ds_index,
                                    int record_index,
                                    void *buffer )

{
    int		absolute_offset;
    int         result;

    if( ds_index < 0 || ds_index >= self->ds_count )
    {
        SendError( "Attempt to write non-existant dataset in "
                   "EnvisatFile_WriteDatasetRecord()" );
        return FAILURE;
    }

    if( record_index < 0
        || record_index >=  self->ds_info[ds_index]->num_dsr )
    {
        SendError( "Attempt to write beyond end of dataset in "
                   "EnvisatFile_WriteDatasetRecord()" );
        return FAILURE;
    }

    absolute_offset = self->ds_info[ds_index]->ds_offset
        + record_index * self->ds_info[ds_index]->dsr_size;

    if( VSIFSeekL( self->fp, absolute_offset, SEEK_SET ) != 0 )
    {
        SendError( "seek failed in EnvisatFile_WriteDatasetRecord()" );
        return FAILURE;
    }

    result = VSIFReadL( buffer, 1, self->ds_info[ds_index]->dsr_size, self->fp );
    if( result != self->ds_info[ds_index]->dsr_size )
    {
        SendError( "read failed in EnvisatFile_ReadDatasetRecord()" );
        return FAILURE;
    }

    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    S_NameValueList_FindKey()

Purpose:
    Search for given key in list of name/value pairs. 

Description:
    Scans list looking for index of EnvisatNameValue where the key matches
    (case sensitive) the passed in name. 

Inputs:
    key -- the key, such as "SLICE_POSITION" being searched for. 
    entry_count -- the number of items in the entries array.
    entries -- array of name/value structures to search. 

Outputs:

Returns:
    array index into entries, or -1 on failure. 

-----------------------------------------------------------------------------*/

int S_NameValueList_FindKey( const char *key, 
                             int entry_count, 
                             EnvisatNameValue **entries )

{
    int		i;

    for( i = 0; i < entry_count; i++ )
    {
        if( strcmp(entries[i]->key,key) == 0 )
            return i;
    }
    
    return -1;
}

/*-----------------------------------------------------------------------------

Name:
    S_NameValueList_FindValue()

Purpose:
    Search for given key in list of name/value pairs, and return value.

Description:
    Returns value string or default if key not found.

Inputs:
    key -- the key, such as "SLICE_POSITION" being searched for. 
    entry_count -- the number of items in the entries array.
    entries -- array of name/value structures to search. 
    default_value -- value to use if key not found.

Outputs:

Returns:
    value string, or default if key not found.

-----------------------------------------------------------------------------*/

const char *S_NameValueList_FindValue( const char *key, 
                                       int entry_count, 
                                       EnvisatNameValue **entries,
                                       const char *default_value )

{
    int		i;

    i = S_NameValueList_FindKey( key, entry_count, entries );
    if( i == -1 )
        return default_value;
    else
        return entries[i]->value;
}

/*-----------------------------------------------------------------------------

Name:
    S_NameValueList_Parse()

Purpose:
    Parse a block of envisat style name/value pairs into an
    EnvisatNameValue structure list. 

Description:
    The passed in text block should be zero terminated.  The entry_count, 
    and entries should be pre-initialized (normally to 0 and NULL).

Inputs:
    text -- the block of text, multiple lines, to be processed.

Outputs:
    entry_count -- returns with the updated number of entries in the
                   entries array.
    entries -- returns with updated array info structures. 

Returns:
    SUCCESS or FAILURE

-----------------------------------------------------------------------------*/

int S_NameValueList_Parse( const char *text, int text_offset, 
                           int *entry_count, 
                           EnvisatNameValue ***entries )

{
    const char  *next_text = text;

    /*
     * Loop over each input line in the text block.
     */
    while( *next_text != '\0' )
    {
        char	line[1024];
        int     line_len = 0, equal_index, src_char, line_offset;
        EnvisatNameValue *entry;

        /*
         * Extract one line of text into the "line" buffer, and remove the
         * newline character.  Eat leading spaces.
         */
        while( *next_text == ' ' ) 
        {
            next_text++;
        }
        line_offset = (int) (next_text - text) + text_offset;
        while( *next_text != '\0' && *next_text != '\n' )
        {
            if( line_len > sizeof(line)-1 )
            {
                SendError( "S_NameValueList_Parse(): "
                           "Corrupt line, longer than 1024 characters." );
                return FAILURE;
            }

            line[line_len++] = *(next_text++);
        }

        line[line_len] = '\0';
        if( *next_text == '\n' )
            next_text++;

        /*
         * Blank lines are permitted.  We will skip processing of any line
         * that doesn't have an equal sign, under the assumption it is
         * white space.
         */
        if( strstr( line, "=") == NULL )
            continue;

        /*
         * Create the name/value info structure. 
         */
        entry = (EnvisatNameValue *) calloc(sizeof(EnvisatNameValue),1);
        entry->literal_line = strdup(line);

        /*
         * Capture the key.  We take everything up to the equal sign.  There
         * shouldn't be any white space, but if so, we take it as part of the
         * key.
         */
        equal_index = strstr(line, "=") - line;
        entry->key = (char *) malloc(equal_index+1);
        strncpy( entry->key, line, equal_index );
        entry->key[equal_index] = '\0';
        entry->value_offset = line_offset + equal_index + 1;

        /*
         * If the next character after the equal sign is a double quote, then
         * the value is a string.  Suck out the text between the double quotes.
         */
        if( line[equal_index+1] == '"' )
        {
            for( src_char = equal_index + 2;
                 line[src_char] != '\0' && line[src_char] != '"';
                 src_char++ ) {}

            line[src_char] = '\0';
            entry->value = strdup( line + equal_index + 2 );
            entry->value_offset += 1;
        }

        /*
         * The value is numeric, and may include a units field.
         */
        else
        {
            for( src_char = equal_index + 1; 
                 line[src_char] != '\0' && line[src_char] != '<' 
                     && line[src_char] != ' ';
                 src_char++ ) {}

            /* capture units */
            if( line[src_char] == '<' )
            {
                int dst_char;

                for( dst_char = src_char+1; 
                     line[dst_char] != '>' && line[dst_char] != '\0';
                     dst_char++ ) {}

                line[dst_char] = '\0';
                entry->units = strdup( line + src_char + 1 );
            }

            line[src_char] = '\0';
            entry->value = strdup( line + equal_index + 1 );
        }

        /*
         * Add the entry to the name/value list. 
         */
        (*entry_count)++;
        *entries = (EnvisatNameValue **)
            realloc( *entries, *entry_count * sizeof(EnvisatNameValue*) );

        if( *entries == NULL )
        {
            *entry_count = 0;
            return FAILURE;
        }

        (*entries)[*entry_count-1] = entry;
    }
    
    return SUCCESS;
}

/*-----------------------------------------------------------------------------

Name:
    S_NameValueList_Rewrite()

Purpose:
    Rewrite the values of a name/value list in the file.

Description:

Inputs:
    fp -- the VSILFILE to operate on.
    entry_count -- number of entries to write.
    entries -- array of entry descriptions.

Returns:
    SUCCESS or FAILURE


-----------------------------------------------------------------------------*/

int S_NameValueList_Rewrite( VSILFILE * fp, int entry_count, 
                              EnvisatNameValue **entries )	      

{
    int		i;

    for( i = 0; i < entry_count; i++ )
    {
        EnvisatNameValue	*entry = entries[i];

        if( VSIFSeekL( fp, entry->value_offset, SEEK_SET ) != 0 )
        {
            SendError( "VSIFSeekL() failed writing name/value list." );
            return FAILURE;
        }

        if( VSIFWriteL( entry->value, 1, strlen(entry->value), fp ) != 
            strlen(entry->value) )
        {
            SendError( "VSIFWriteL() failed writing name/value list." );
            return FAILURE;
        }
    }

    return SUCCESS;
}


/*-----------------------------------------------------------------------------

Name:
    S_NameValueList_Destroy()

Purpose:
    Free resources associated with a name/value list.

Description:
    The count, and name/value list pointers are set to 0/NULL on completion.

Inputs:
    entry_count -- returns with the updated number of entries in the
                   entries array.
    entries -- returns with updated array info structures. 

Outputs:
    entry_count -- Set to zero.
    entries -- Sett o NULL.

Returns:


-----------------------------------------------------------------------------*/

void S_NameValueList_Destroy( int *entry_count, 
                              EnvisatNameValue ***entries )	      

{
    int		i;

    for( i = 0; i < *entry_count; i++ )
    {
        free( (*entries)[i]->key );
        free( (*entries)[i]->value );
        free( (*entries)[i]->units );
        free( (*entries)[i]->literal_line );
        free( (*entries)[i] );
    }

    free( *entries );
    
    *entry_count = 0;
    *entries = NULL;
}

/* EOF */

