/******************************************************************************
 *
 * Purpose:  PCIDSK library utility functions - private
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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
#ifndef INCLUDE_CORE_PCIDSK_UTILS_H
#define INCLUDE_CORE_PCIDSK_UTILS_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include <string>
#include <vector>

namespace PCIDSK
{
    class IOInterfaces;
    
    /************************************************************************/
    /*                          Utility functions.                          */
    /************************************************************************/

    std::string &UCaseStr( std::string & );
    uint64 atouint64( const char *);
    int64  atoint64( const char *);
    int    pci_strcasecmp( const char *, const char * );
    int    pci_strncasecmp( const char *, const char *, size_t );

#ifndef EQUAL
#define EQUAL(x,y) (pci_strcasecmp(x,y) == 0)
#define EQUALN(x,y,n) (pci_strncasecmp(x,y,n) == 0)
#endif
#ifndef STARTS_WITH_CI
#define STARTS_WITH_CI(x,y) EQUALN(x,y,strlen(y))
#define STARTS_WITH(x,y) (std::strncmp(x,y,strlen(y)) == 0)
#endif
  
    void   SwapData( void* const data, const int size, const int wcount );
    bool   BigEndianSystem(void);
    void   GetCurrentDateTime( char *out_datetime );

    void   ParseTileFormat( std::string full_text, int &block_size, 
                            std::string &compression );
    void   SwapPixels(void* const data, 
                      const eChanType type, 
                      const std::size_t count);

    std::vector<double> ProjParmsFromText( std::string geosys, 
                                           std::string parms );
    std::string         ProjParmsToText( std::vector<double> );

    std::string         MergeRelativePath( const PCIDSK::IOInterfaces *,
                                           std::string base,
                                           std::string src_filename );
    std::string         ExtractPath( std::string );
    
    void LibJPEG_DecompressBlock(
        uint8 *src_data, int src_bytes, uint8 *dst_data, int dst_bytes,
        int xsize, int ysize, eChanType pixel_type );
    void LibJPEG_CompressBlock(
        uint8 *src_data, int src_bytes, uint8 *dst_data, int &dst_bytes,
        int xsize, int ysize, eChanType pixel_type, int quality );

    void                DefaultDebug( const char * );
    void                Debug( void (*)(const char *), const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);

} // end namespace PCIDSK

#endif // INCLUDE_CORE_PCIDSK_UTILS_H
