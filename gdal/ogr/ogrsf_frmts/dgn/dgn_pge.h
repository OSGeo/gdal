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
 ****************************************************************************/

#ifndef _DGN_PGE_H_INCLUDED
#define _DGN_PGE_H_INCLUDED

#include "cpl_port.h"

CPL_C_START
int DGNWriteTags( const char *pszFilename, int nTagScheme, 
                  char **papszTagSets, 
                  char **papszTagNames,
                  char **papszTagValues );
int DGNReadTags( const char *pszFilename, int nTagScheme,
                 char ***ppapszTagSets, 
                 char ***papszTagNames,
                 char ***papszTagValues );
CPL_C_END

#endif /* ndef _DGN_PGE_H_INCLUDED */
