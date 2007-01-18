/******************************************************************************
 * $Id$
 *
 * Project:  JPEG-2000
 * Purpose:  Implements read-only virtual io on a subregion of a file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "kdu_file_io.h"
#include "cpl_error.h"

/************************************************************************/
/*                            subfile_source                            */
/************************************************************************/

class subfile_source : public kdu_compressed_source {

  public: 
    subfile_source() { file = NULL; }
    subfile_source(const char *fname, bool allow_seeks=true)
      { file = NULL; open(fname,allow_seeks); }


    ~subfile_source() { close(); }


    bool exists() { return (file != NULL); }

    bool operator!() { return (file == NULL); }

    void open(const char *fname, bool allow_seeks=true)
      {
          const char *real_filename;
          close();

          if( sscanf( fname, "J2K_SUBFILE:%d,%d", 
                      &subfile_offset, &subfile_size ) != 2 )
          {
              kdu_error e;

              e << "Corrupt subfile definition:" << fname;
              return;
          }

          real_filename = strstr(fname,",");
          if( real_filename != NULL )
              real_filename = strstr(real_filename+1,",");
          if( real_filename != NULL )
              real_filename++;
          else
          {
              kdu_error e;

              e << "Could not find filename in subfile definition." << fname;
              return;
          }

          file = VSIFOpenL( real_filename, "rb" );
          if( file == NULL )
          {
              kdu_error e;
              e << "Unable to open compressed data file, \"" << 
                  real_filename << "\"!";
              return;
          }

          capabilities = KDU_SOURCE_CAP_SEQUENTIAL;
          if (allow_seeks)
              capabilities |= KDU_SOURCE_CAP_SEEKABLE;

          seek_origin = subfile_offset;
          seek( 0 );
      }

    int get_capabilities() { return capabilities; }

    bool seek(kdu_long offset)
      { 
          assert(file != NULL);
          if( file == NULL )
              return false;
          
          if (!(capabilities & KDU_SOURCE_CAP_SEEKABLE))
              return false;
          
          if( VSIFSeekL( file, seek_origin+offset, SEEK_SET ) == 0 )
              return true;
          else
              return false;
      }

    bool set_seek_origin(kdu_long position)
      { 
          if (!(capabilities & KDU_SOURCE_CAP_SEEKABLE))
              return false;
          seek_origin = position + subfile_offset;
          return true;
      }

    kdu_long get_pos(bool absolute)
      { 
        if (file == NULL) return -1;
        kdu_long result = VSIFTell( file );
        if (!absolute) 
            result -= seek_origin;
        else
            result -= subfile_offset;
        return result;
      }

    int read(kdu_byte *buf, int num_bytes)
      { 
        assert(file != NULL);

        num_bytes = VSIFReadL(buf,1,(size_t) num_bytes,file);
        return num_bytes;
      }

    bool close()
      {
        if (file != NULL)
            VSIFCloseL( file );
        file = NULL;
        return true;
      }

  private: // Data
    int capabilities;
    kdu_long seek_origin;

    int subfile_offset;
    int subfile_size;
    
    FILE *file;
  };

