/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Function to register all known OGR drivers.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.14  2003/10/06 19:16:38  warmerda
 * added ODBC support
 *
 * Revision 1.13  2003/04/08 19:32:06  warmerda
 * added memory driver
 *
 * Revision 1.12  2003/02/03 21:17:03  warmerda
 * added .rec driver
 *
 * Revision 1.11  2002/12/28 04:10:11  warmerda
 * added Oracle(OCI) support
 *
 * Revision 1.10  2002/05/29 20:33:11  warmerda
 * added FME support
 *
 * Revision 1.9  2002/02/18 20:56:55  warmerda
 * register AVCBin
 *
 * Revision 1.8  2002/01/25 20:40:51  warmerda
 * register gml
 *
 * Revision 1.7  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.6  2000/11/23 06:03:09  warmerda
 * fixed PG support
 *
 * Revision 1.5  2000/10/17 17:53:04  warmerda
 * added postgresql support
 *
 * Revision 1.4  2000/08/24 04:44:05  danmo
 * Added optional OGDI driver in OGR
 *
 * Revision 1.3  1999/12/22 15:36:45  warmerda
 * RegisterOGRMIF no longer exists
 *
 * Revision 1.2  1999/11/14 18:10:44  svillene
 * add RegisterOGRMIF RegisterOGRTAB
 *
 * Revision 1.1  1999/11/04 21:10:51  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRRegisterAll()                           */
/************************************************************************/

void OGRRegisterAll()

{
    RegisterOGRShape();
    RegisterOGRNTF();
    RegisterOGRSDTS();
    RegisterOGRTiger();
    RegisterOGRS57();
    RegisterOGRTAB();
    RegisterOGRDGN();
    RegisterOGRGML();
    RegisterOGRAVCBin();
    RegisterOGRREC();
    RegisterOGRMEM();
    RegisterOGRODBC();
//    RegisterOGRE00();
#ifdef OGDI_ENABLED
    RegisterOGROGDI();
#endif
#ifdef PG_ENABLED
    RegisterOGRPG();
#endif
#ifdef OCI_ENABLED
    RegisterOGROCI();
#endif
#ifdef FME_ENABLED
    RegisterOGRFME();
#endif
}

