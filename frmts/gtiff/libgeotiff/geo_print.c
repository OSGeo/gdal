/**********************************************************************
 *
 *  geo_print.c  -- Key-dumping routines for GEOTIFF files.
 *
 *    Written By: Niles D. Ritter.
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 *
 *  Revision History;
 *
 *    20 June,  1995      Niles D. Ritter      New
 *     7 July,  1995      NDR                  Fix indexing
 *    27 July,  1995      NDR                  Added Import utils
 *    28 July,  1995      NDR                  Made parser more strict.
 *    29  Sep,  1995      NDR                  Fixed matrix printing.
 *
 **********************************************************************/

#include "geotiff.h"   /* public interface        */
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */
#include "geokeys.h"

#include <stdio.h>     /* for sprintf             */

#define FMT_GEOTIFF "Geotiff_Information:"
#define FMT_VERSION "Version: %hd"
#define FMT_REV     "Key_Revision: %1hd.%hd"
#define FMT_TAGS    "Tagged_Information:"
#define FMT_TAGEND  "End_Of_Tags."
#define FMT_KEYS    "Keyed_Information:"
#define FMT_KEYEND  "End_Of_Keys."
#define FMT_GEOEND  "End_Of_Geotiff."
#define FMT_DOUBLE  "%-17.15g"
#define FMT_SHORT   "%-11hd"

static void DefaultPrint(char *string, void *aux);
static void PrintKey(GeoKey *key, GTIFPrintMethod print,void *aux);
static void PrintGeoTags(GTIF *gtif,GTIFReadMethod scan,void *aux);
static void PrintTag(int tag, int nrows, double *data, int ncols, 
					GTIFPrintMethod print,void *aux);
static void DefaultRead(char *string, void *aux);
static int  ReadKey(GTIF *gt, GTIFReadMethod scan, void *aux);
static int  ReadTag(GTIF *gt,GTIFReadMethod scan,void *aux);

/*
 * Print off the directory info, using whatever method is specified
 * (defaults to fprintf if null). The "aux" parameter is provided for user
 * defined method for passing parameters or whatever.
 *
 * The output format is a "GeoTIFF meta-data" file, which may be
 * used to import information with the GTIFFImport() routine.
 */
 
void GTIFPrint(GTIF *gtif, GTIFPrintMethod print,void *aux)
{
    int i;
    int numkeys = gtif->gt_num_keys;
    GeoKey *key = gtif->gt_keys;
    char message[1024];
	
    if (!print) print = (GTIFPrintMethod) &DefaultPrint;
    if (!aux) aux=stdout;	

    sprintf(message,FMT_GEOTIFF "\n"); 
    print(message,aux);
    sprintf(message, "Version: %hd" ,gtif->gt_version);
    sprintf(message, FMT_VERSION,gtif->gt_version);
    print("   ",aux); print(message,aux); print("\n",aux);
    sprintf(message, FMT_REV,gtif->gt_rev_major,
            gtif->gt_rev_minor); 
    print("   ",aux); print(message,aux); print("\n",aux);

    sprintf(message,"   %s\n",FMT_TAGS); print(message,aux);
    PrintGeoTags(gtif,print,aux);
    sprintf(message,"      %s\n",FMT_TAGEND); print(message,aux);

    sprintf(message,"   %s\n",FMT_KEYS); print(message,aux);
    for (i=0; i<numkeys; i++)
        PrintKey(++key,print,aux);
    sprintf(message,"      %s\n",FMT_KEYEND); print(message,aux);

    sprintf(message,"   %s\n",FMT_GEOEND); print(message,aux);
}

static void PrintGeoTags(GTIF *gt, GTIFPrintMethod print,void *aux)
{
	double *data;
	int count;
	tiff_t *tif=gt->gt_tif;

        if( tif == NULL )
            return;

	if ((gt->gt_methods.get)(tif, GTIFF_TIEPOINTS, &count, &data ))
		PrintTag(GTIFF_TIEPOINTS,count/3, data, 3, print, aux);
	if ((gt->gt_methods.get)(tif, GTIFF_PIXELSCALE, &count, &data ))
		PrintTag(GTIFF_PIXELSCALE,count/3, data, 3, print, aux);
	if ((gt->gt_methods.get)(tif, GTIFF_TRANSMATRIX, &count, &data ))
		PrintTag(GTIFF_TRANSMATRIX,count/4, data, 4, print, aux);
}

static void PrintTag(int tag, int nrows, double *dptr, int ncols, 
					GTIFPrintMethod print,void *aux)
{
	int i,j;
	double *data=dptr;
        char message[1024];

	print("      ",aux);
	print(GTIFTagName(tag),aux);
	sprintf(message," (%d,%d):\n",nrows,ncols);
	print(message,aux);
	for (i=0;i<nrows;i++)
	{
		print("         ",aux);
		for (j=0;j<ncols;j++)
		{
			sprintf(message,FMT_DOUBLE,*data++);
			print(message,aux);

                        if( j < ncols-1 )
                            print(" ",aux);
		}
		print("\n",aux);
	}
	_GTIFFree(dptr); /* free up the allocated memory */
}


static void PrintKey(GeoKey *key, GTIFPrintMethod print, void *aux)
{
    char *data;
    geokey_t keyid = (geokey_t) key->gk_key;
    int count = key->gk_count;
    int vals_now,i;
    pinfo_t *sptr;
    double *dptr;
    char message[40];

    print("      ",aux);
    print(GTIFKeyName(keyid),aux);
	
    sprintf(message," (%s,%d): ",GTIFTypeName(key->gk_type),count);
    print(message,aux);
	
    if (key->gk_type==TYPE_SHORT && count==1)
        data = (char *)&key->gk_data;
    else
        data = key->gk_data;
		
    switch (key->gk_type)
    {
      case TYPE_ASCII: 
      {
          int  in_char, out_char;

          print("\"",aux);

          in_char = 0;
          out_char = 0;
          while( in_char < count-1 )
          {
              char ch = ((char *) data)[in_char++];

              if( ch == '\n' )
              {
                  message[out_char++] = '\\';
                  message[out_char++] = 'n';
              }
              else if( ch == '\\' )
              {
                  message[out_char++] = '\\';
                  message[out_char++] = '\\';
              }
              else
                  message[out_char++] = ch;

              /* flush message if buffer full */
              if( (size_t)out_char >= sizeof(message)-3 )
              {
                  message[out_char] = '\0';
                  print(message,aux);
                  out_char = 0;
              }
          }

          message[out_char]='\0';
          print(message,aux);

          print("\"\n",aux);
      }
      break;
        
      case TYPE_DOUBLE: 
        for (dptr = (double *)data; count > 0; count-= vals_now)
        {
            vals_now = count > 3? 3: count;
            for (i=0; i<vals_now; i++,dptr++)
            {
                sprintf(message,FMT_DOUBLE ,*dptr);
                print(message,aux);
            }
            print("\n",aux);
        }
        break;
        
      case TYPE_SHORT: 
        sptr = (pinfo_t *)data;
        if (count==1)
        {
            print( GTIFValueName(keyid,*sptr), aux );
            print( "\n", aux );
        }
        else
            for (; count > 0; count-= vals_now)
            {
                vals_now = count > 3? 3: count;
                for (i=0; i<vals_now; i++,sptr++)
                {
                    sprintf(message,FMT_SHORT,*sptr);
                    print(message,aux);
                }
                print("\n",aux);
            }
        break;
        
      default: 
        sprintf(message, "Unknown Type (%d)\n",key->gk_type);
        print(message,aux);
        break;
    }
}

static void DefaultPrint(char *string, void *aux)
{
    /* Pretty boring */
    fprintf((FILE *)aux,"%s",string);
}


/*
 *  Importing metadata file
 */

/*
 * Import the directory info, using whatever method is specified
 * (defaults to fscanf if null). The "aux" parameter is provided for user
 * defined method for passing file or whatever.
 *
 * The input format is a "GeoTIFF meta-data" file, which may be
 * generated by the GTIFFPrint() routine.
 */
 
int GTIFImport(GTIF *gtif, GTIFReadMethod scan,void *aux)
{
    int status;
    char message[1024];
	
    if (!scan) scan = (GTIFReadMethod) &DefaultRead;
    if (!aux) aux=stdin;	
	
    scan(message,aux);
    if (strncmp(message,FMT_GEOTIFF,8)) return 0; 
    scan(message,aux);
    if (!sscanf(message,FMT_VERSION,(short int*)&gtif->gt_version)) return 0;
    scan(message,aux);
    if (sscanf(message,FMT_REV,(short int*)&gtif->gt_rev_major,
               (short int*)&gtif->gt_rev_minor) !=2) return 0;

    scan(message,aux);
    if (strncmp(message,FMT_TAGS,8)) return 0;
    while ((status=ReadTag(gtif,scan,aux))>0);
    if (status < 0) return 0;

    scan(message,aux);
    if (strncmp(message,FMT_KEYS,8)) return 0;
    while ((status=ReadKey(gtif,scan,aux))>0);
	
    return (status==0); /* success */
}

static int StringError(char *string)
{
    fprintf(stderr,"Parsing Error at \'%s\'\n",string);
    return -1;
}

#define SKIPWHITE(vptr) \
  while (*vptr && (*vptr==' '||*vptr=='\t')) vptr++
#define FINDCHAR(vptr,c) \
  while (*vptr && *vptr!=(c)) vptr++

static int ReadTag(GTIF *gt,GTIFReadMethod scan,void *aux)
{
    int i,j,tag;
    char *vptr;
    char tagname[100];
    double *data,*dptr;
    int count,nrows,ncols,num;
    char message[1024];

    scan(message,aux);
    if (!strncmp(message,FMT_TAGEND,8)) return 0;

    num=sscanf(message,"%[^( ] (%d,%d):\n",tagname,&nrows,&ncols);
    if (num!=3) return StringError(message);
	
    tag = GTIFTagCode(tagname);
    if (tag < 0) return StringError(tagname);

    count = nrows*ncols;

    data = (double *) _GTIFcalloc(count * sizeof(double));
    dptr = data;
	
    for (i=0;i<nrows;i++)
    {
        scan(message,aux);
        vptr = message;
        for (j=0;j<ncols;j++)
        {
            if (!sscanf(vptr,"%lg",dptr++))
                return StringError(vptr);
            FINDCHAR(vptr,' ');
            SKIPWHITE(vptr);
        }
    }	
    (gt->gt_methods.set)(gt->gt_tif, (pinfo_t) tag, count, data );	

    _GTIFFree( data );

    return 1;
}


static int ReadKey(GTIF *gt, GTIFReadMethod scan, void *aux)
{
    tagtype_t ktype;
    int count,outcount;
    int vals_now,i;
    geokey_t key;
    int icode;
    pinfo_t code;
    short  *sptr;
    char name[1000];
    char type[20];
    double data[100];
    double *dptr;
    char *vptr;
    int num;
    char message[2048];

    scan(message,aux); 
    if (!strncmp(message,FMT_KEYEND,8)) return 0;

    num=sscanf(message,"%[^( ] (%[^,],%d):\n",name,type,&count);
    if (num!=3) return StringError(message);

    vptr = message;
    FINDCHAR(vptr,':'); 
    if (!*vptr) return StringError(message);
    vptr+=2;

    if( GTIFKeyCode(name) < 0 )
        return StringError(name);
    else
        key = (geokey_t) GTIFKeyCode(name);

    if( GTIFTypeCode(type) < 0 )
        return StringError(type);
    else
        ktype = (tagtype_t) GTIFTypeCode(type);

    /* skip white space */
    SKIPWHITE(vptr);
    if (!*vptr) return StringError(message);
			
    switch (ktype)
    {
      case TYPE_ASCII: 
      {
          char *cdata;
          int out_char = 0;

          FINDCHAR(vptr,'"');
          if (!*vptr) return StringError(message);

          cdata = (char *) _GTIFcalloc( count+1 );

          vptr++;
          while( out_char < count-1 )
          {
              if( *vptr == '\0' )
                  break;

              else if( vptr[0] == '\\' && vptr[1] == 'n' )
              {
                  cdata[out_char++] = '\n';
                  vptr += 2;
              }
              else if( vptr[0] == '\\' && vptr[1] == '\\' )
              {
                  cdata[out_char++] = '\\';
                  vptr += 2;
              }
              else
                  cdata[out_char++] = *(vptr++);
          }

          if( out_char < count-1 ) return StringError(message);
          if( *vptr != '"' ) return StringError(message);

          cdata[count-1] = '\0';
          GTIFKeySet(gt,key,ktype,count,cdata);

          _GTIFFree( cdata );
      }
      break;
        
      case TYPE_DOUBLE: 
        outcount = count;
        for (dptr = data; count > 0; count-= vals_now)
        {
            vals_now = count > 3? 3: count;
            for (i=0; i<vals_now; i++,dptr++)
            {
                if (!sscanf(vptr,"%lg" ,dptr))
                    StringError(vptr);
                FINDCHAR(vptr,' ');
                SKIPWHITE(vptr);
            }
            if (vals_now<count)
            {
                scan(message,aux);
                vptr = message;
            }
        }
        if (outcount==1)
            GTIFKeySet(gt,key,ktype,outcount,data[0]);
        else
            GTIFKeySet(gt,key,ktype,outcount,data);
        break;
        
      case TYPE_SHORT: 
        if (count==1)
        {
            icode = GTIFValueCode(key,vptr);
            if (icode < 0) return StringError(vptr);
            code = (pinfo_t) icode;
            GTIFKeySet(gt,key,ktype,count,code);
        }
        else  /* multi-valued short - no such thing yet */
        {
            sptr = (short *)data;
            outcount = count;
            for (; count > 0; count-= vals_now)
            {
                vals_now = count > 3? 3: count;
                for (i=0; i<vals_now; i++,sptr++)
                {
                    int		work_int;

                    /* note: FMT_SHORT (%11hd) not supported on IRIX */
                    sscanf(message,"%11d",&work_int);
                    *sptr = (short) work_int;
                    scan(message,aux);
                }
                if (vals_now<count)
                {
                    scan(message,aux);
                    vptr = message;
                }
            }
            GTIFKeySet(gt,key,ktype,outcount,sptr);
        }
        break;

      default:
        return -1;
    }
    return 1;
}


static void DefaultRead(char *string, void *aux)
{
    /* Pretty boring */
    int num_read;
    num_read = fscanf((FILE *)aux, "%[^\n]\n", string);
    if (num_read != 0) {
      fprintf(stderr, "geo_print.c DefaultRead failed to read anything.\n");
    }
}
