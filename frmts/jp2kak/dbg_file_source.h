#include "kdu_file_io.h"
#include "cpl_error.h"

/*****************************************************************************/
/*                          dbg_simple_file_source                           */
/*****************************************************************************/

class dbg_simple_file_source : public kdu_compressed_source {
  /* [BIND: reference] */
  public: // Member functions
    dbg_simple_file_source() { file = NULL; }
    dbg_simple_file_source(const char *fname, bool allow_seeks=true)
      { file = NULL; open(fname,allow_seeks); }
      /* [SYNOPSIS] Convenience constructor, which also calls `open'. */
    ~dbg_simple_file_source() { close(); }
      /* [SYNOPSIS] Automatically calls `close'. */
    bool exists() { return (file != NULL); }
      /* [SYNOPSIS]
           Returns true if there is an open file associated with the object.
      */
    bool operator!() { return (file == NULL); }
      /* [SYNOPSIS]
           Opposite of `exists', returning false if there is an open file
           associated with the object.
      */
    void open(const char *fname, bool allow_seeks=true)
      {
      /* [SYNOPSIS]
           Closes any currently open file and attempts to open a new one,
           generating an appropriate error (through `kdu_error') if the
           indicated file cannot be opened.
         [ARG: fname]
           Relative path name of file to be opened.
         [ARG: allow_seeks]
           If false, seeking within the code-stream will not be permitted.
           Disabling seeking has no effect unless the code-stream contains
           TLM and/or PLT marker segments, in which case the ability
           to seek within the file can save a lot of memory when working
           with large images, but this may come at the expense of some loss
           in speed if we know ahead of time that we want to decompress
           the entire image.
      */
        close();
        file = fopen(fname,"rb");
        if (file == NULL)
          { kdu_error e;
            e << "Unable to open compressed data file, \"" << fname << "\"!"; }
        capabilities = KDU_SOURCE_CAP_SEQUENTIAL;
        if (allow_seeks)
          capabilities |= KDU_SOURCE_CAP_SEEKABLE;
        seek_origin = 0;
      }
    int get_capabilities() { return capabilities; }
      /* [SYNOPSIS]
           The returned capabilities word always includes the flag,
           `KDU_SOURCE_CAP_SEQUENTIAL', but may also include
           `KDU_SOURCE_CAP_SEEKABLE', depending on the `allow_seeks' argument
           passed to `open'.  See `kdu_compressed_source::get_capabilities'
           for an explanation of capabilities.
      */
    bool seek(kdu_long offset)
      { /* [SYNOPSIS] See `kdu_compressed_source::seek' for an explanation. */
        assert(file != NULL);
        if (!(capabilities & KDU_SOURCE_CAP_SEEKABLE))
          return false;
        kdu_fseek(file,seek_origin+offset);
        CPLDebug( "KDU", "seek(%ld)", (long) offset );
        return true;
      }
    bool set_seek_origin(kdu_long position)
      { /* [SYNOPSIS]
           See `kdu_compressed_source::set_seek_origin' for an explanation. */
        if (!(capabilities & KDU_SOURCE_CAP_SEEKABLE))
          return false;
        seek_origin = position;
        return true;
      }
    kdu_long get_pos(bool absolute)
      { /* [SYNOPSIS]
           See `kdu_compressed_source::get_pos' for an explanation. */
        if (file == NULL) return -1;
        kdu_long result = kdu_ftell(file);
        if (!absolute) result -= seek_origin;
        return result;
      }
    int read(kdu_byte *buf, int num_bytes)
      { /* [SYNOPSIS] See `kdu_compressed_source::read' for an explanation. */
        assert(file != NULL);
        CPLDebug( "KDU", "read(%ld)", (long) num_bytes );
        num_bytes = fread(buf,1,(size_t) num_bytes,file);
        return num_bytes;
      }
    void close()
      { /* [SYNOPSIS]
             It is safe to call this function, even if no file has been opened.
        */
        if (file != NULL)
          fclose(file);
        file = NULL;
      }
  private: // Data
    int capabilities;
    kdu_long seek_origin;
    FILE *file;
  };

