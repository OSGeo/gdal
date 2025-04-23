/******************************************************************************
 *
 * Purpose:  PCIDSK library utility functions - private
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

    void   ParseTileFormat(const std::string& oOptions, int & nTileSize,
                           std::string & oCompress);

    void   SwapPixels(void* const data,
                      const eChanType type,
                      const std::size_t count);

    std::string         ParseLinkedFilename(std::string oOptions);

    std::vector<double> ProjParamsFromText( std::string geosys,
                                           std::string params );
    std::string         ProjParamsToText( std::vector<double> );

    std::string         DefaultMergeRelativePath(const PCIDSK::IOInterfaces *,
                                                 const std::string& base,
                                                 const std::string& src_filename);
    std::string         ExtractPath( std::string );

    void LibJPEG_DecompressBlock(
        uint8 *src_data, int src_bytes, uint8 *dst_data, int dst_bytes,
        int xsize, int ysize, eChanType pixel_type );
    void LibJPEG_CompressBlock(
        uint8 *src_data, int src_bytes, uint8 *dst_data, int &dst_bytes,
        int xsize, int ysize, eChanType pixel_type, int quality );

    void LibKAKADU_DecompressBlock
        (PCIDSK::eChanType eChanType,
         uint8 * pabySrcBuffer, int nSrcBufferSize,
         uint8 * pabyDstBuffer, int nDstBufXSize,
         int nXSize, int nYSize, int nChanCount);
    void LibKAKADU_CompressBlock
        (PCIDSK::eChanType eChanType,
         uint8 * pabyDstBuffer, int nDstBufferSize,
         uint8 * pabySrcBuffer, int nSrcBufXSize,
         int nXSize, int nYSize, int nChanCount,
         int & nCompressSize, double dfQuality);

    void                DefaultDebug( const char * );
    void                Debug( void (*)(const char *), const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);

} // end namespace PCIDSK

#endif // INCLUDE_CORE_PCIDSK_UTILS_H
