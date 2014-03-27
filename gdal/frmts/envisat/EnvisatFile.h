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

#ifndef __ENVISAT_FILE_H__
#define __ENVISAT_FILE_H__

typedef struct EnvisatFile_tag EnvisatFile;

typedef enum 
{
    MPH = 0,
    SPH = 1
} EnvisatFile_HeaderFlag;

int EnvisatFile_Open( EnvisatFile **self, const char *filename, 
                      const char *mode );
void EnvisatFile_Close( EnvisatFile *self );
const char *EnvisatFile_GetFilename( EnvisatFile *self );
int EnvisatFile_Create( EnvisatFile **self, const char *filename, 
                        const char *template_file );
int EnvisatFile_GetCurrentLength( EnvisatFile *self );

const char* EnvisatFile_GetKeyByIndex(  EnvisatFile *self, 
                                        EnvisatFile_HeaderFlag mph_or_sph,
                                        int key_index );

int EnvisatFile_TestKey(  EnvisatFile *self, 
                          EnvisatFile_HeaderFlag mph_or_sph,
                          const char *key );

const char *EnvisatFile_GetKeyValueAsString( EnvisatFile *self, 
                                             EnvisatFile_HeaderFlag mph_or_sph,
                                             const char *key,
                                             const char *default_value );

int EnvisatFile_SetKeyValueAsString( EnvisatFile *self, 
                                     EnvisatFile_HeaderFlag mph_or_sph,
                                     const char *key,
                                     const char *value );

int EnvisatFile_GetKeyValueAsInt( EnvisatFile *self, 
                                  EnvisatFile_HeaderFlag mph_or_sph,
                                  const char *key, 
                                  int default_value );

int EnvisatFile_SetKeyValueAsInt( EnvisatFile *self, 
                                  EnvisatFile_HeaderFlag mph_or_sph,
                                  const char *key, 
                                  int value );

double EnvisatFile_GetKeyValueAsDouble( EnvisatFile *self, 
                                        EnvisatFile_HeaderFlag mph_or_sph,
                                        const char *key, 
                                        double default_value );
int EnvisatFile_SetKeyValueAsDouble( EnvisatFile *self, 
                                     EnvisatFile_HeaderFlag mph_or_sph,
                                     const char *key, 
                                     double value );

int EnvisatFile_GetDatasetIndex( EnvisatFile *self, const char *ds_name );

int EnvisatFile_GetDatasetInfo( EnvisatFile *self, 
                                int ds_index,
                                char **ds_name, 
                                char **ds_type,
                                char **filename,
                                int  *ds_offset,
                                int  *ds_size,
                                int  *num_dsr,
                                int  *dsr_size );
int EnvisatFile_SetDatasetInfo( EnvisatFile *self, 
                                int ds_index,
                                int ds_offset,
                                int ds_size,
                                int num_dsr,
                                int dsr_size );
                              
int EnvisatFile_ReadDatasetRecordChunk( EnvisatFile *self, 
                                    int ds_index,
                                    int record_index,
                                    void *buffer, 
                                    int offset, int size ) ;
int EnvisatFile_ReadDatasetRecord( EnvisatFile *self, 
                                   int ds_index,
                                   int record_index, 
                                   void *record_buffer );
int EnvisatFile_WriteDatasetRecord( EnvisatFile *self, 
                                    int ds_index,
                                    int record_index, 
                                    void *record_buffer );
int EnvisatFile_ReadDatasetChunk( EnvisatFile *self, 
                                  int ds_index,
                                  int offset, 
                                  int size,
                                  void *buffer );
                                

#ifndef FAILURE
#  define FAILURE 1
#endif
#ifndef SUCCESS 
#  define SUCCESS 0
#endif

#endif /* __ENVISAT_FILE_H__ */

/* EOF */



