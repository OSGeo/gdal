#ifndef grib2_H
#define grib2_H
#include<stdio.h>

#define G2_VERSION "g2clib-1.0.4"
/*                .      .    .                                       .
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-25
//
//   Each element of structure gribfield is defined as:
//   
//   gribfield gfld;
//
//        gfld->version = GRIB edition number ( currently 2 )
//        gfld->discipline = Message Discipline ( see Code Table 0.0 )
//        gfld->idsect = Contains the entries in the Identification
//                        Section ( Section 1 )
//                        This element is a pointer to an array
//                        that holds the data.
//            gfld->idsect[0]  = Identification of originating Centre
//                                    ( see Common Code Table C-1 )
//                             7 - US National Weather Service
//            gfld->idsect[1]  = Identification of originating Sub-centre
//            gfld->idsect[2]  = GRIB Master Tables Version Number
//                                    ( see Code Table 1.0 )
//                             0 - Experimental
//                             1 - Initial operational version number
//            gfld->idsect[3]  = GRIB Local Tables Version Number
//                                    ( see Code Table 1.1 )
//                             0     - Local tables not used
//                             1-254 - Number of local tables version used
//            gfld->idsect[4]  = Significance of Reference Time (Code Table 1.2)
//                             0 - Analysis
//                             1 - Start of forecast
//                             2 - Verifying time of forecast
//                             3 - Observation time
//            gfld->idsect[5]  = Year ( 4 digits )
//            gfld->idsect[6]  = Month
//            gfld->idsect[7)  = Day
//            gfld->idsect[8]  = Hour
//            gfld->idsect[9]  = Minute
//            gfld->idsect[10]  = Second
//            gfld->idsect[11]  = Production status of processed data
//                                    ( see Code Table 1.3 )
//                              0 - Operational products
//                              1 - Operational test products
//                              2 - Research products
//                              3 - Re-analysis products
//            gfld->idsect[12]  = Type of processed data ( see Code Table 1.4 )
//                              0  - Analysis products
//                              1  - Forecast products
//                              2  - Analysis and forecast products
//                              3  - Control forecast products
//                              4  - Perturbed forecast products
//                              5  - Control and perturbed forecast products
//                              6  - Processed satellite observations
//                              7  - Processed radar observations
//        gfld->idsectlen = Number of elements in gfld->idsect[].
//        gfld->local   = Pointer to character array containing contents
//                       of Local Section 2, if included
//        gfld->locallen = length of array gfld->local[]
//        gfld->ifldnum = field number within GRIB message
//        gfld->griddef = Source of grid definition (see Code Table 3.0)
//                      0 - Specified in Code table 3.1
//                      1 - Predetermined grid Defined by originating centre
//        gfld->ngrdpts = Number of grid points in the defined grid.
//        gfld->numoct_opt = Number of octets needed for each
//                          additional grid points definition.
//                          Used to define number of
//                          points in each row ( or column ) for
//                          non-regular grids.
//                          = 0, if using regular grid.
//        gfld->interp_opt = Interpretation of list for optional points
//                          definition.  (Code Table 3.11)
//        gfld->igdtnum = Grid Definition Template Number (Code Table 3.1)
//        gfld->igdtmpl  = Contains the data values for the specified Grid
//                         Definition Template ( NN=gfld->igdtnum ).  Each
//                         element of this integer array contains an entry (in
//                         the order specified) of Grid Definition Template 3.NN
//                         This element is a pointer to an array
//                         that holds the data.
//        gfld->igdtlen = Number of elements in gfld->igdtmpl[].  i.e. number of
//                       entries in Grid Definition Template 3.NN
//                       ( NN=gfld->igdtnum ).
//        gfld->list_opt  = (Used if gfld->numoct_opt .ne. 0)  This array
//                          contains the number of grid points contained in
//                          each row ( or column ).  (part of Section 3)
//                          This element is a pointer to an array
//                          that holds the data.  This pointer is nullified
//                          if gfld->numoct_opt=0.
//        gfld->num_opt = (Used if gfld->numoct_opt .ne. 0)  The number of entries
//                       in array ideflist.  i.e. number of rows ( or columns )
//                       for which optional grid points are defined.  This value
//                       is set to zero, if gfld->numoct_opt=0.
//        gfdl->ipdtnum = Product Definition Template Number (see Code Table 4.0)
//        gfld->ipdtmpl  = Contains the data values for the specified Product
//                         Definition Template ( N=gfdl->ipdtnum ).  Each element
//                         of this integer array contains an entry (in the
//                         order specified) of Product Definition Template 4.N.
//                         This element is a pointer to an array
//                         that holds the data.
//        gfld->ipdtlen = Number of elements in gfld->ipdtmpl[].  i.e. number of
//                       entries in Product Definition Template 4.N
//                       ( N=gfdl->ipdtnum ).
//        gfld->coord_list  = Real array containing floating point values
//                            intended to document the vertical discretisation
//                            associated to model data on hybrid coordinate
//                            vertical levels.  (part of Section 4)
//                            This element is a pointer to an array
//                            that holds the data.
//        gfld->num_coord = number of values in array gfld->coord_list[].
//        gfld->ndpts = Number of data points unpacked and returned.
//        gfld->idrtnum = Data Representation Template Number
//                       ( see Code Table 5.0)
//        gfld->idrtmpl  = Contains the data values for the specified Data
//                         Representation Template ( N=gfld->idrtnum ).  Each
//                         element of this integer array contains an entry
//                         (in the order specified) of Product Definition
//                         Template 5.N.
//                         This element is a pointer to an array
//                         that holds the data.
//        gfld->idrtlen = Number of elements in gfld->idrtmpl[].  i.e. number
//                       of entries in Data Representation Template 5.N
//                       ( N=gfld->idrtnum ).
//        gfld->unpacked = logical value indicating whether the bitmap and
//                        data values were unpacked.  If false,
//                        gfld->bmap and gfld->fld pointers are nullified.
//        gfld->expanded = Logical value indicating whether the data field 
//                         was expanded to the grid in the case where a 
//                         bit-map is present.  If true, the data points in
//                         gfld->fld match the grid points and zeros were 
//                         inserted at grid points where data was bit-mapped
//                         out.  If false, the data values in gfld->fld were
//                         not expanded to the grid and are just a consecutive
//                         array of data points corresponding to each value of
//                         "1" in gfld->bmap.
//        gfld->ibmap = Bitmap indicator ( see Code Table 6.0 )
//                     0 = bitmap applies and is included in Section 6.
//                     1-253 = Predefined bitmap applies
//                     254 = Previously defined bitmap applies to this field
//                     255 = Bit map does not apply to this product.
//        gfld->bmap  = integer array containing decoded bitmap,
//                      if gfld->ibmap=0 or gfld->ibap=254.  Otherwise nullified.
//                      This element is a pointer to an array
//                      that holds the data.
//        gfld->fld  = Array of gfld->ndpts unpacked data points.
//                     This element is a pointer to an array
//                     that holds the data.
*/

typedef int g2int;
typedef unsigned int g2intu;
typedef float g2float;

typedef struct {
   g2int type;           /* 3=Grid Definition Template.                       */
                         /* 4=Product Definition Template.                    */
                         /* 5=Data Representation Template.                  */
   g2int num;            /* template number.                                 */
   g2int maplen;         /* number of entries in the static part             */
                         /*                    of the template.              */
   g2int *map;           /* num of octets of each entry in the               */
                         /*         static part of the template.             */
   g2int needext;        /* indicates whether or not the template needs      */
                         /*     to be extended.                              */
   g2int extlen;         /* number of entries in the template extension.     */
   g2int *ext;           /* num of octets of each entry in the extension     */
                         /*                      part of the template.       */
} xxtemplate;

//typedef struct template template; // otherwise VC6 gives "error C2447: missing function header (old-style formal list?)"

struct gribfield {
   g2int   version,discipline;
   g2int   *idsect;
   g2int   idsectlen;
   unsigned char *local;
   g2int   locallen;
   g2int   ifldnum;
   g2int   griddef,ngrdpts;
   g2int   numoct_opt,interp_opt,num_opt;
   g2int   *list_opt;
   g2int   igdtnum,igdtlen;
   g2int   *igdtmpl;
   g2int   ipdtnum,ipdtlen;
   g2int   *ipdtmpl;
   g2int   num_coord;
   g2float *coord_list;
   g2int   ndpts,idrtnum,idrtlen;
   g2int   *idrtmpl;
   g2int   unpacked;
   g2int   expanded;
   g2int   ibmap;
   g2int   *bmap;
   g2float *fld;
};

typedef struct gribfield gribfield;

#define RINT(d)   (floor(d+0.5))

/*  Prototypes for unpacking API  */
void seekgb(FILE *,g2int ,g2int ,g2int *,g2int *);
g2int g2_info(unsigned char *,g2int *,g2int *,g2int *,g2int *);
g2int g2_getfld(unsigned char *,g2int ,g2int ,g2int ,gribfield **);
void g2_free(gribfield *);

/*  Prototypes for packing API  */
g2int g2_create(unsigned char *,g2int *,g2int *);
g2int g2_addlocal(unsigned char *,unsigned char *,g2int );
g2int g2_addgrid(unsigned char *,g2int *,g2int *,g2int *,g2int ); 
g2int g2_addfield(unsigned char *,g2int ,g2int *,
                       g2float *,g2int ,g2int ,g2int *,
                       g2float *,g2int ,g2int ,g2int *);
g2int g2_gribend(unsigned char *);

/*  Prototypes for supporting routines  */
extern double int_power(double, g2int );
extern void mkieee(g2float *,g2int *,g2int);
void rdieee(g2int *,g2float *,g2int );
extern xxtemplate *getpdstemplate(g2int);
extern xxtemplate *extpdstemplate(g2int,g2int *);
extern xxtemplate *getdrstemplate(g2int);
extern xxtemplate *extdrstemplate(g2int,g2int *);
extern xxtemplate *getgridtemplate(g2int);
extern xxtemplate *extgridtemplate(g2int,g2int *);
extern void simpack(g2float *,g2int,g2int *,unsigned char *,g2int *);
extern void compack(g2float *,g2int,g2int,g2int *,unsigned char *,g2int *);
void misspack(g2float *,g2int ,g2int ,g2int *, unsigned char *, g2int *);
void gbit(unsigned char *,g2int *,g2int ,g2int );
void sbit(unsigned char *,const g2int *,g2int ,g2int );
void gbits(unsigned char *,g2int *,g2int ,g2int ,g2int ,g2int );
void sbits(unsigned char *,const g2int *,g2int ,g2int ,g2int ,g2int );

int pack_gp(g2int *, g2int *, g2int *,
            g2int *, g2int *, g2int *, g2int *, g2int *,
            g2int *, g2int *, g2int *, g2int *,
            g2int *, g2int *, g2int *, g2int *, g2int *,
            g2int *, g2int *, g2int *);

g2int g2_unpack1(unsigned char *cgrib,g2int *iofst,g2int **ids,g2int *idslen);
g2int g2_unpack2(unsigned char *cgrib,g2int *iofst,g2int *lencsec2,unsigned char **csec2);
g2int g2_unpack3(unsigned char *cgrib,g2int *iofst,g2int **igds,g2int **igdstmpl,
                         g2int *mapgridlen,g2int **ideflist,g2int *idefnum);
g2int g2_unpack4(unsigned char *cgrib,g2int *iofst,g2int *ipdsnum,g2int **ipdstmpl,
               g2int *mappdslen,g2float **coordlist,g2int *numcoord);
g2int g2_unpack5(unsigned char *cgrib,g2int *iofst,g2int *ndpts,g2int *idrsnum,
               g2int **idrstmpl,g2int *mapdrslen);
g2int g2_unpack6(unsigned char *cgrib,g2int *iofst,g2int ngpts,g2int *ibmap,
               g2int **bmap);
g2int g2_unpack7(unsigned char *cgrib,g2int *iofst,g2int igdsnum,g2int *igdstmpl,
               g2int idrsnum,g2int *idrstmpl,g2int ndpts,g2float **fld);
g2int simunpack(unsigned char *,g2int *, g2int,g2float *);
int comunpack(unsigned char *,g2int,g2int,g2int *,g2int,g2float *);
g2int specunpack(unsigned char *,g2int *,g2int,g2int,g2int, g2int, g2float *);
g2int jpcunpack(unsigned char *,g2int,g2int *,g2int, g2float *);
void specpack(g2float *fld,g2int ndpts,g2int JJ,g2int KK,g2int MM,
              g2int *idrstmpl,unsigned char *cpack,g2int *lcpack);

typedef g2int integer;
typedef g2float real;
int reduce(integer *kfildo, integer *jmin, integer *jmax,
        integer *lbit, integer *nov, integer *lx, integer *ndg, integer *ibit,
         integer *jbit, integer *kbit, integer *novref, integer *ibxx2,
        integer *ier);
void cmplxpack(g2float *fld,g2int ndpts, g2int idrsnum,g2int *idrstmpl,
               unsigned char *cpack, g2int *lcpack);
g2int getdim(unsigned char *csec3,g2int *width,g2int *height,g2int *iscan);
void g2_miss( gribfield *gfld, float *rmiss, int *nmiss );
g2int getpoly(unsigned char *csec3,g2int *jj,g2int *kk,g2int *mm);
void jpcpack(g2float *fld,g2int width,g2int height,g2int *idrstmpl,
             unsigned char *cpack,g2int *lcpack);
int enc_jpeg2000(unsigned char *cin,g2int width,g2int height,g2int nbits,
                 g2int ltype, g2int ratio, g2int retry, char *outjpc, 
                 g2int jpclen);

#endif  /*  grib2_H  */

