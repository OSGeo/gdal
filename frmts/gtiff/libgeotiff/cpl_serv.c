/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * cpl_serv.c: Various Common Portability Library derived convenience functions
 *
 * $Log$
 * Revision 1.3  1999/09/08 18:17:55  warmerda
 * stripped down again
 *
 * Revision 1.2  1999/09/08 18:12:55  warmerda
 * reimported
 *
 * Revision 1.4  1999/06/25 04:35:26  warmerda
 * Fixed to actually support long lines.
 *
 * Revision 1.3  1999/03/17 20:43:03  geotiff
 * Avoid use of size_t keyword
 *
 * Revision 1.2  1999/03/10 18:22:39  geotiff
 * Added string.h, fixed backslash escaping
 *
 * Revision 1.1  1999/03/09 15:57:04  geotiff
 * New
 *
 */

#include "cpl_serv.h"
#include "geo_tiffp.h"

