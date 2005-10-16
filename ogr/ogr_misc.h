/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Free standing functions for OGR.
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2005/10/16 01:40:31  cfis
 * Implementation of  --config, --debug, and --formats command line parameters similar to what GDAL utilities have.
 *
 */

#ifndef OGR_MISC_CPP_INCLUDED
#define OGR_MISC_CPP_INCLUDED

#include "cpl_string.h"
#include "cpl_minixml.h"
#include "ogrsf_frmts.h"


/************************************************************************/
/*                    OGRGeneralCmdLineProcessor()                     */
/************************************************************************/

/**
 * General utility option processing.
 *
 * This function is intended to provide a variety of generic commandline 
 * options for all OGR commandline utilities.  It takes care of the following
 * commandline options:
 *  
 *  --formats: report all format drivers configured.
 *  --format [format]: report details of one format driver. 
 *  --optfile filename: expand an option file into the argument list. 
 *  --config key value: set system configuration option. 
 *  --debug [on/off/value]: set debug level.
 *  --help-general: report detailed help on general options. 
 *
 * The argument array is replaced "in place" and should be freed with 
 * CSLDestroy() when no longer needed.  The typical usage looks something
 * like the following.  Note that the formats should be registered so that
 * the --formats and --format options will work properly.
 *
 *  int main( int argc, char ** argv )
 *  { 
 *    OGRAllRegister();
 *
 *    argc = OGRGeneralCmdLineProcessor( argc, &argv, 0 );
 *    if( argc < 1 )
 *        exit( -argc );
 *
 * @param nArgc number of values in the argument list.
 * @param Pointer to the argument list array (will be updated in place). 
 *
 * @return updated nArgc argument count.  Return of 0 requests terminate 
 * without error, return of -1 requests exit with error code.
 */

int CPL_STDCALL OGRGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv, int nOptions );

#endif /* ndef OGR_MISC_CPP_INCLUDED */
