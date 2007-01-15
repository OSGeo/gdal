// xritheaderparser.h: interface for the XRITHeaderParser class.
//
//////////////////////////////////////////////////////////////////////

/******************************************************************************
 *
 * Purpose:  Parse the header of the combined XRIT header/data files.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 * Parts of code Copyright (c) 2003 R. Alblas 
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 ******************************************************************************/

#if !defined(AFX_XRIT_HEADER_H__6BA5C029_3F7A_43B0_9C69_D002B83A2A63__INCLUDED_)
#define AFX_XRIT_HEADER_H__6BA5C029_3F7A_43B0_9C69_D002B83A2A63__INCLUDED_

#include "cpl_port.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <time.h>
#include <fstream>


typedef struct xrit_hdr_tag
{
  int hdr_type;
  int hdr_rec_len;
  int file_type;
  long hdr_len;
  long data_len;
  long datalen_msb;
  long datalen_lsb;

  int nb,nc,nl,cf;
  char image_iformat;  /* 'j' or 'w' */
  char image_oformat;  /* 'j' or 'w' */

  char proj_name[35];
  GInt32 cfac,lfac,coff,loff;

/*Anno and extracted contents */
  char anno[100];
  char hl;            /* hrit/lrit */
  char vers[10];      /* 000 */
  char sat[10];       /* MSG1 */
  char src[20];       /* MSG*, SERVICE, GOES,  ... */
  char satsrc[20];    /* MSG*, Srvc, Frgn */
  int scan_dir;       /* 'n', 's' */
  char chan[20];      /* VIS006, ADMIN, .... */
  int chan_nr;        /* coding chan into number (1, 2, ...) */
  char special;       /* p(ro), e(pi) */
  int segment;        /* segment number */
  char itime[20];     /* time: year/date/hourmin */
  char compr;         /* flag compressed */
  char encry;         /* flag encrypted */

  char sortn[20];
  char id[40];
  struct tm time;

  char ccdds[7];
  int gp_sc_id;
  int spec_ch_id;
  int seq_no,seq_start,seq_end;
  int dt_f_rep;

  GUInt32 pic_id;

} XRIT_HDR;

class XRITHeaderParser  
{
public:
  XRITHeaderParser();
  virtual ~XRITHeaderParser();
  /*************************************************************************
   * Read from a extracted file (channel with certain order number)
   * the XRIT header.
   * Remaining file is JPEG/Wavelet.
   *************************************************************************/
  int read_xrithdr(std::ifstream & ifile);

  const XRIT_HDR & xrit_hdr()
  {
    return m_xrit_hdr;
  };

private:

  // private var for holding the header info
  XRIT_HDR m_xrit_hdr;
  int m_iHeaderLength;
};

#endif // !defined(AFX_XRIT_HEADER_H__6BA5C029_3F7A_43B0_9C69_D002B83A2A63__INCLUDED_)
