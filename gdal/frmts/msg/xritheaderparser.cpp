// xritheaderparser.cpp: implementation of the XRITHeaderParser class.
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

#include "xritheaderparser.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

XRITHeaderParser::XRITHeaderParser()
: m_iHeaderLength(0)
{

}

XRITHeaderParser::~XRITHeaderParser()
{

}

/*************************************
 * translate channel name into number 
 * 
 *************************************/
/* Note: Only info in file-name known here! */
void channame2nr(XRIT_HDR *xrit_hdr)
{
  if      (xrit_hdr->special=='p')           xrit_hdr->chan_nr=0;
  else if (xrit_hdr->special=='P')           xrit_hdr->chan_nr=0;
  else if (xrit_hdr->special=='e')           xrit_hdr->chan_nr=0;
  else if (xrit_hdr->special=='E')           xrit_hdr->chan_nr=0;
  else if (!strcmp(xrit_hdr->chan,"VIS006")) xrit_hdr->chan_nr=1;
  else if (!strcmp(xrit_hdr->chan,"VIS008")) xrit_hdr->chan_nr=2;
  else if (!strcmp(xrit_hdr->chan,"IR_016")) xrit_hdr->chan_nr=3;
  else if (!strcmp(xrit_hdr->chan,"IR_039")) xrit_hdr->chan_nr=4;
  else if (!strcmp(xrit_hdr->chan,"WV_062")) xrit_hdr->chan_nr=5;
  else if (!strcmp(xrit_hdr->chan,"WV_073")) xrit_hdr->chan_nr=6;
  else if (!strcmp(xrit_hdr->chan,"IR_087")) xrit_hdr->chan_nr=7;
  else if (!strcmp(xrit_hdr->chan,"IR_097")) xrit_hdr->chan_nr=8;
  else if (!strcmp(xrit_hdr->chan,"IR_108")) xrit_hdr->chan_nr=9;
  else if (!strcmp(xrit_hdr->chan,"IR_120")) xrit_hdr->chan_nr=10;
  else if (!strcmp(xrit_hdr->chan,"IR_134")) xrit_hdr->chan_nr=11;
  else if (!strcmp(xrit_hdr->chan,"HRV"))    xrit_hdr->chan_nr=12;
  else                                       xrit_hdr->chan_nr=-1; /* not defined here */
}

/* Remove trailing underscores */
void remove_tr_usc(char *s)
{
  char *p;
  p=s+strlen(s)-1;
  while ((p>=s) && (*p=='_'))
  {
    *p=0;
    p--;
  } 
}

/*************************************
 * Extract annotation info MSG
 * Examples:
 * 
 * L-000-MSG1__-GOES7_______-IR_107___-00004____-200202020202-CE
 * L-000-MSG1__-MSG1________-IR_016___-00001____-200202020202-CE
 * H-000-MSG1__-MSG1________-_________-EPI______-200305040944-__
 * H-000-MSG1__-MSG1________-_________-PRO______-200305040914-__
 *************************************/
int extract_anno_msg(XRIT_HDR *xh)
{
  char *p;
  char tmp[20];
  char anno[1000];
  strcpy(anno,xh->anno);

  if (!strchr(anno,'-')) return 0;
  if (!(p=strtok(anno,"-"))) return 0;      /* L or H */

  xh->hl=*p; 

  if (!(p=strtok(NULL,"-"))) return 0;      /* version */
  strncpy(xh->vers,p,4);


  if (!(p=strtok(NULL,"-"))) return 0;      /* satellite name */
  strncpy(xh->sat,p,7);
  remove_tr_usc(xh->sat);

  if (!(p=strtok(NULL,"-"))) return 0;      /* ID#1: data source (12 chars) */
  strncpy(xh->src,p,13);    
  remove_tr_usc(xh->src);

  if (!strncmp(xh->src,"MSG",3))
    strcpy(xh->satsrc,xh->src);
  else if (!strncmp(xh->src,"SERVICE",7))
    strcpy(xh->satsrc,"Srvc");
  else if (!strncmp(xh->src,"MPEF",7))
    strcpy(xh->satsrc,"Mpef");
  else if (!strncmp(xh->src,"MET",3))
    strcpy(xh->satsrc,"MET");
  else
    strcpy(xh->satsrc,"Frgn");

  if (!(p=strtok(NULL,"-"))) return 0;      /* ID#2: channel (9 chars) */
  strncpy(xh->chan,p,10);
  remove_tr_usc(xh->chan);

  if ((p=strtok(NULL,"-")))                 /* ID#3: nr. or PRO/EPI (9 chars) */
  {
    xh->special=0;
    if (!strncmp(p,"PRO_",4))
    {
      if (!*xh->chan)
        xh->special='P';                    /* group PRO */
      else
        xh->special='p';                    /* channel PRO */
    }
    else if (!strncmp(p,"EPI_",4))
    {
      if (!*xh->chan)
        xh->special='E';                    /* group EPI */
      else
        xh->special='e';                    /* channel EPI */
    }
    else
    {
      xh->segment=atoi(p); 
    }
  }
/*
  if (!*xh->chan)
  {
    if (xh->special=='p') strcpy(xh->chan,"PRO");
    if (xh->special=='e') strcpy(xh->chan,"EPI");
  }
*/
  if ((p=strtok(NULL,"-")))                  /* prod. ID#4: time (12 chars)  */
  {
    strcpy(xh->itime,p);
    memset(&xh->time,0,sizeof(xh->time));
    strncpy(tmp,p,4); tmp[4]=0; p+=4;
    xh->time.tm_year=atoi(tmp)-1900;

    strncpy(tmp,p,2); tmp[2]=0; p+=2;
    xh->time.tm_mon=atoi(tmp)-1;

    strncpy(tmp,p,2); tmp[2]=0; p+=2;
    xh->time.tm_mday=atoi(tmp);

    strncpy(tmp,p,2); tmp[2]=0; p+=2;
    xh->time.tm_hour=atoi(tmp);

    strncpy(tmp,p,2); tmp[2]=0; p+=2;
    xh->time.tm_min=atoi(tmp);
/*
Don't use this; time gets confused because of daylight saving!
    mktime(&xh->time);
*/

  }

  if ((p=strtok(NULL,"-")))                  /* flags */
  {
    xh->compr=xh->encry='_';
    if (strchr(p,'C')) xh->compr='C';
    if (strchr(p,'E')) xh->encry='E';
  }

/* Determine sort-order number: [yyyymmddhhmm][t][c] MUST be always equal length! */
  channame2nr(xh);
  if (xh->chan_nr>=0)
    sprintf(xh->sortn,"%s%c%x%02x",xh->itime,xh->hl,xh->chan_nr,xh->segment);
  else
    sprintf(xh->sortn,"%s%c%s%02x",xh->itime,xh->hl,xh->chan,xh->segment);
  sprintf(xh->id,"%-10s %c   ",xh->chan,xh->hl);
  strftime(xh->id+14,20,"%d-%m-%y %H:%M  ",&xh->time);             /* time */
  return 1;
}

/*************************************
 * Extract annotation info 
 *************************************/
int extract_anno(XRIT_HDR *xh)
{
  if (!strncmp(xh->anno+6,"MSG",3))
    return extract_anno_msg(xh);
  else 
    return 0;
}

void catch_primhdr(unsigned char *l, XRIT_HDR *xrit_hdr)
{
  xrit_hdr->file_type=l[3]; /* 0=image,1=GTS mess.,2=text,3=encr. mess.*/
/* Total header length of this file */
  xrit_hdr->hdr_len=(l[4]<<24)+(l[5]<<16)+(l[6]<<8)+l[7];


/* Total content length of this file */
  xrit_hdr->datalen_msb=(l[8]<<24)+(l[9]<<16)+(l[10]<<8)+l[11];
  xrit_hdr->datalen_lsb=(l[12]<<24)+(l[13]<<16)+(l[14]<<8)+l[15];
  xrit_hdr->data_len=(xrit_hdr->datalen_lsb >> 3) +
                     (xrit_hdr->datalen_msb << 5);
}


/*************************************
 * Extract xrit header
 * Return: Position after headers
 *************************************/
unsigned char *catch_xrit_hdr(unsigned char *l,int ln,XRIT_HDR *xrit_hdr)
{
  unsigned char *p;
  int hdr_len=0;
  int pos=0;
  memset(xrit_hdr,0,sizeof(*xrit_hdr));

  do
  {
/* Get header type and length */
    xrit_hdr->hdr_type=l[0];
    xrit_hdr->hdr_rec_len=(l[1]<<8)+l[2];

/* Test: header length < length l */
    pos+=xrit_hdr->hdr_rec_len;
    if (pos>ln) break;
/* Extract headertype dependent info */
    switch(xrit_hdr->hdr_type)
    {
/* ------------------ Primary header ------------------ */
      case 0:
        catch_primhdr(l,xrit_hdr);
      break;
/* ------------------ Image header ------------------ */
      case 1:
        xrit_hdr->nb=l[3];           /* # bitplanes */
        xrit_hdr->nc=(l[4]<<8)+l[5]; /* # columns (=width) */
        xrit_hdr->nl=(l[6]<<8)+l[7]; /* # lines */
        xrit_hdr->cf=l[8];           /* compr. flag: 0, 1=lossless, 2=lossy */
      break;
/* ------------------ Image navigation ------------------ */
      case 2:
/* Projection name */
        strncpy(xrit_hdr->proj_name,(const char*)(l+3),32); xrit_hdr->proj_name[32]=0;
/* Remove leading spaces */
        for (p=(unsigned char*)(xrit_hdr->proj_name+31); 
             ((*p==' ') && ((char *)p > xrit_hdr->proj_name));
             p--);
        p++; *p=0;
        xrit_hdr->cfac=(l[35]<<24)+(l[36]<<16)+(l[37]<<8)+l[38];
        xrit_hdr->lfac=(l[39]<<24)+(l[40]<<16)+(l[41]<<8)+l[42];
        xrit_hdr->coff=(l[43]<<24)+(l[44]<<16)+(l[45]<<8)+l[46];
        xrit_hdr->loff=(l[47]<<24)+(l[48]<<16)+(l[49]<<8)+l[50];
        if (xrit_hdr->lfac > 0) xrit_hdr->scan_dir='n';
        if (xrit_hdr->lfac < 0) xrit_hdr->scan_dir='s';
      break;
/* ------------------ Image data functions ------------------ */
      case 3:
      break;
/* ------------------ Annotation ------------------ */
      case 4:
/* Extract annotation */
        strncpy(xrit_hdr->anno,(const char*)(l+3),64);
        xrit_hdr->anno[61]=0;
        extract_anno(xrit_hdr);
      break;
/* ------------------ Time stamp ------------------ */
      case 5:
      {
        int i;
        for (i=0; i<7; i++) xrit_hdr->ccdds[i]=l[3+i];
      }
      break;
/* ------------------ ancillary text ------------------ */
      case 6:
      break;
/* ------------------ key header ------------------ */
      case 7:
      break;
/* ------------------ segment identification ------------------ */
      case 128:
        if (!strncmp(xrit_hdr->sat,"MSG",3))
        {
/* Eumetsat */
          xrit_hdr->gp_sc_id   =(l[3]<<8)+l[4];   /* unique for each sat. source? (MSG, GOES...) */
          xrit_hdr->spec_ch_id =l[5];
          xrit_hdr->seq_no     =(l[6]<<8)+l[7];   /* segment no. */
          xrit_hdr->seq_start  =(l[8]<<8)+l[9];   /* "planned" start segment */
          xrit_hdr->seq_end    =(l[10]<<8)+l[11]; /* "planned" end segment */
          xrit_hdr->dt_f_rep   =l[12];
        }
        else
        {
/* NOAA */
          xrit_hdr->pic_id     =(l[3]<<8)+l[4];   /* unique for each picture?? */
          xrit_hdr->seq_no     =(l[5]<<8)+l[6];   /* segment seq. no. */
/* start column=(l[7]<<8)+l[8] */
/* start row   =(l[9]<<8)+l[10] */
          xrit_hdr->seq_start  =1;                /* start segment */
          xrit_hdr->seq_end    =(l[11]<<8)+l[12]; /* max segment */
/* max column=(l[13]<<8)+l[14] */
/* max row   =(l[15]<<8)+l[16] */
        }
      break;
/* ------------------ image segment line quality ------------------ */
      case 129:
      break;
/* ------------------ ??? In GOES LRIT ??? ------------------ */
      case 130:

      break;
/* ------------------ ??? In GOES LRIT ??? ------------------ */
      case 131:
      break;
      default:
        printf("Unexpected hdrtype=%d\n",xrit_hdr->hdr_type);
      break;
    }

    if (xrit_hdr->hdr_type==0)
    {
      hdr_len=xrit_hdr->hdr_len;
    }
    l+=xrit_hdr->hdr_rec_len;

    hdr_len-=xrit_hdr->hdr_rec_len;


  } while (hdr_len>0);
  return l;
}



/*************************************************************************
 * Read from a extracted file (channel with certain order number)
 * the XRIT header.
 * Remaining file is JPEG.
 *************************************************************************/
int XRITHeaderParser::read_xrithdr(std::ifstream & ifile)
{
  unsigned char l1[20],*l;
  int hdr_len;

/* Read in primary header, just to determine length of all headers  */
  ifile.read((char*)l1, 16);
  
#if _MSG_VER > 1000 && _MSC_VER < 1300  
  ifile.seekg(-16, std::ios_base::seekdir::cur);
#else
  ifile.seekg(-16, std::ios_base::cur);
#endif  

/* Test header; expected primary */
  if (l1[0]!=0) return 0;

  if (((l1[1]<<8) + l1[2]) !=16) return 0;

/* Determine total header length */
  hdr_len=(l1[4]<<24)+(l1[5]<<16)+(l1[6]<<8)+l1[7];
  if ((hdr_len>10000) || (hdr_len<10)) return 0;

/* Allocate and read all headers */
  l=(unsigned char*)malloc(hdr_len);
  ifile.read((char*)l, hdr_len);
  
/* Extract header info */
  catch_xrit_hdr(l,hdr_len,&m_xrit_hdr);

/* Determine image type */
  if (m_xrit_hdr.file_type==0)
  {
    unsigned char c[2];
    ifile.read((char*)c, 2);
    if ((c[0]==0xff) && (c[1]==0x01))      m_xrit_hdr.image_iformat='w';
    else if ((c[0]==0xff) && (c[1]==0xd8)) m_xrit_hdr.image_iformat='j';
    else                                   m_xrit_hdr.image_iformat='?';
#if _MSC_VER > 1000 && _MSC_VER < 1300  
    ifile.seekg(-2, std::ios_base::seekdir::cur);
#else
    ifile.seekg(-2, std::ios_base::cur);
#endif    
  }

  channame2nr(&m_xrit_hdr);

  free(l);

  m_iHeaderLength = hdr_len;

  return hdr_len;
}
