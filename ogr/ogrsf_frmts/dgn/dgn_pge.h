/******************************************************************************
 * $Id$
 *
 * Project:  DGN Tag Read/Write Bindings for Pacific Gas and Electric
 * Purpose:  Declarations for PGE DGN Tag functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Pacific Gas and Electric Co, San Franciso, CA, USA.
 *
 * All rights reserved.  Not to be used, reproduced or disclosed without
 * permission.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2002/03/15 15:06:49  warmerda
 * New
 *
 */

#ifndef _DGN_PGE_H_INCLUDED
#define _DGN_PGE_H_INCLUDED

#include "cpl_port.h"

CPL_C_START
char *pgeDGNWriteTags( const char *pszFilename, int nTagScheme, 
                       const char *pszTagList );
char *pgeDGNReadTags( const char *pszFilename, int nTagScheme );
void pgeDGNFreeResult( char *pszResult );
CPL_C_END

#endif /* ndef _DGN_PGE_H_INCLUDED */
