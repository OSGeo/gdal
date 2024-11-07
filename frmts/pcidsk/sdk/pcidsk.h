/******************************************************************************
 *
 * Purpose:  Primary public include file for PCIDSK SDK.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/**
 * \file pcidsk.h
 *
 * Public PCIDSK library classes and functions.
 */

#ifndef PCIDSK_H_INCLUDED
#define PCIDSK_H_INCLUDED

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_file.h"
#include "pcidsk_channel.h"
#include "pcidsk_buffer.h"
#include "pcidsk_mutex.h"
#include "pcidsk_exception.h"
#include "pcidsk_interfaces.h"
#include "pcidsk_segment.h"
#include "pcidsk_io.h"
#include "pcidsk_georef.h"
#include "pcidsk_rpc.h"

//! Namespace for all PCIDSK Library classes and functions.

namespace PCIDSK {
/************************************************************************/
/*                      PCIDSK Access Functions                         */
/************************************************************************/
PCIDSKFile PCIDSK_DLL *Open( const std::string& filename, const std::string& access,
                             const PCIDSKInterfaces *interfaces = nullptr,
                             int max_channel_count_allowed = -1 );

PCIDSKFile PCIDSK_DLL *Create( const std::string& filename, int pixels, int lines,
                               int channel_count, eChanType *channel_types,
                               const std::string&  options,
                               const PCIDSKInterfaces *interfaces = nullptr );


} // end of PCIDSK namespace

#endif // PCIDSK_H_INCLUDED
