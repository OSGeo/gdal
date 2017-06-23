/* libblx - Magellan BLX topo reader/writer library
 *
 * Copyright (c) 2008, Henrik Johansson <henrik@johome.net>
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "blx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cpl_port.h"

/* Constants */
#define MAXLEVELS 5
#define MAXCOMPONENTS 4

static const int table1[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
			,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
			,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
			,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1
			,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
			,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2
			,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
			,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
			,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5
			,5,5,5,5,5,5,6,6,6,6,6,6,6,6,7,7,7,7
			,7,7,7,7,8,8,8,8,9,9,9,9,10,10,10,10
			,11,11,11,11,12,12,12,12,13,13,13,13,14
			,14,15,15,16,16,17,17,18,18,19,19,20,21
			,22,23,24,25,26,27,28,29,30,31,255,255,255
			,255,255,255,255,255,255,255,255,255,255,255
			,255,255,255,255,255,255,255,255,255,255};

/* { byte, n of bits when compressed, bit pattern << (13-n of bits) } */
static const int table2[][3] = {{0,2,0}, {255,3,2048}, {1,3,3072}, {2,4,4096},
			  {3,4,4608}, {254,5,5120}, {4,5,5376}, {5,5,5632},
			  {253,6,5888}, {6,6,6016}, {252,6,6144}, {7,6,6272},
			  {251,6,6400}, {8,6,6528}, {9,7,6656}, {250,7,6720},
			  {10,7,6784}, {249,7,6848}, {11,7,6912}, {248,7,6976},
			  {12,8,7040}, {247,8,7072}, {16,8,7104}, {246,8,7136},
			  {13,8,7168}, {245,8,7200}, {14,8,7232}, {244,8,7264},
			  {15,8,7296}, {243,8,7328}, {242,8,7360}, {241,8,7392},
			  {17,9,7424}, {18,9,7440}, {240,9,7456}, {239,9,7472},
			  {19,9,7488}, {238,9,7504}, {20,9,7520}, {237,9,7536},
			  {21,9,7552}, {236,9,7568}, {22,9,7584}, {235,9,7600},
			  {234,9,7616}, {23,9,7632}, {233,9,7648}, {24,10,7664},
			  {232,10,7672}, {231,10,7680}, {25,10,7688}, {230,10,7696},
			  {229,10,7704}, {26,10,7712}, {228,10,7720}, {27,10,7728},
			  {227,10,7736}, {225,10,7744}, {226,10,7752}, {28,10,7760},
			  {29,10,7768}, {224,10,7776}, {30,10,7784}, {31,10,7792},
			  {223,10,7800}, {32,10,7808}, {222,10,7816}, {33,10,7824},
			  {221,11,7832}, {220,11,7836}, {34,11,7840}, {219,11,7844},
			  {35,11,7848}, {218,11,7852}, {256,11,7856}, {36,11,7860},
			  {217,11,7864}, {216,11,7868}, {37,11,7872}, {215,11,7876},
			  {38,11,7880}, {214,11,7884}, {193,11,7888}, {213,11,7892},
			  {39,11,7896}, {128,11,7900}, {212,11,7904}, {40,11,7908},
			  {194,11,7912}, {211,11,7916}, {210,11,7920}, {41,11,7924},
			  {209,11,7928}, {208,11,7932}, {42,11,7936}, {207,11,7940},
			  {43,11,7944}, {195,11,7948}, {206,11,7952}, {205,11,7956},
			  {204,11,7960}, {44,11,7964}, {203,11,7968}, {192,11,7972},
			  {196,11,7976}, {45,11,7980}, {201,11,7984}, {200,11,7988},
			  {197,11,7992}, {202,11,7996}, {127,11,8000}, {199,11,8004},
			  {198,11,8008}, {46,12,8012}, {47,12,8014}, {48,12,8016},
			  {49,12,8018}, {50,12,8020}, {51,12,8022}, {191,12,8024},
			  {52,12,8026}, {183,12,8028}, {53,12,8030}, {54,12,8032},
			  {55,12,8034}, {190,12,8036}, {56,12,8038}, {57,12,8040},
			  {189,12,8042}, {58,12,8044}, {176,12,8046}, {59,12,8048},
			  {126,12,8050}, {60,12,8052}, {188,12,8054}, {61,12,8056},
			  {63,12,8058}, {62,12,8060}, {64,12,8062}, {129,12,8064},
			  {187,12,8066}, {186,12,8068}, {65,12,8070}, {66,12,8072},
			  {185,12,8074}, {184,12,8076}, {68,12,8078}, {174,12,8080},
			  {67,12,8082}, {182,13,8084}, {69,13,8085}, {180,13,8086},
			  {181,13,8087}, {71,13,8088}, {70,13,8089}, {179,13,8090},
			  {125,13,8091}, {72,13,8092}, {130,13,8093}, {178,13,8094},
			  {177,13,8095}, {73,13,8096}, {74,13,8097}, {124,13,8098},
			  {76,13,8099}, {175,13,8100}, {75,13,8101}, {131,13,8102},
			  {132,13,8103}, {79,13,8104}, {77,13,8105}, {123,13,8106},
			  {80,13,8107}, {172,13,8108}, {171,13,8109}, {78,13,8110},
			  {173,13,8111}, {81,13,8112}, {169,13,8113}, {122,13,8114},
			  {82,13,8115}, {133,13,8116}, {168,13,8117}, {84,13,8118},
			  {164,13,8119}, {167,13,8120}, {85,13,8121}, {170,13,8122},
			  {166,13,8123}, {165,13,8124}, {121,13,8125}, {160,13,8126},
			  {134,13,8127}, {136,13,8128}, {161,13,8129}, {120,13,8130},
			  {88,13,8131}, {83,13,8132}, {119,13,8133}, {163,13,8134},
			  {162,13,8135}, {159,13,8136}, {91,13,8137}, {135,13,8138},
			  {90,13,8139}, {86,13,8140}, {137,13,8141}, {87,13,8142},
			  {89,13,8143}, {158,13,8144}, {152,13,8145}, {138,13,8146},
			  {139,13,8147}, {116,13,8148}, {140,13,8149}, {92,13,8150},
			  {96,13,8151}, {157,13,8152}, {153,13,8153}, {97,13,8154},
			  {94,13,8155}, {93,13,8156}, {117,13,8157}, {156,13,8158},
			  {155,13,8159}, {95,13,8160}, {118,13,8161}, {143,13,8162},
			  {151,13,8163}, {142,13,8164}, {104,13,8165}, {100,13,8166},
			  {148,13,8167}, {144,13,8168}, {154,13,8169}, {115,13,8170},
			  {113,13,8171}, {98,13,8172}, {146,13,8173}, {112,13,8174},
			  {145,13,8175}, {149,13,8176}, {141,13,8177}, {150,13,8178},
			  {103,13,8179}, {147,13,8180}, {99,13,8181}, {108,13,8182},
			  {101,13,8183}, {114,13,8184}, {105,13,8185}, {102,13,8186},
			  {107,13,8187}, {109,13,8188}, {110,13,8189}, {111,13,8190},
			  {106,13,8191}, {0,0,8192}};

static const int table3[] = {0x20, 0x2f, 0x44, 0x71, 0x95, 0x101};

STATIC int compress_chunk(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outbuflen) {
    int next, m=0, j, outlen=0;
    unsigned reg=0;

    next = *inbuf++;
    inlen--;
    while(next>=0) {
	/* Find index of input byte in table2 and put it in j */
	j=0;
	while(next != table2[j][0]) j++;

	if(inlen) {
	    next = *inbuf++;
	    inlen--;
	} else {
	    if(next == 0x100)
		next = -1;
	    else
		next = 0x100;
	}
	reg = (reg << table2[j][1]) | (table2[j][2] >> (13-table2[j][1]));
	m += table2[j][1];

	while(m>=8) {
	    if(outlen>=outbuflen) return -1;
	    *outbuf++ = (unsigned char)((reg>>(m-8)) & 0xff);
	    outlen++;
	    m-=8;
	}
    }
    if(outlen>=outbuflen) return -1;
    *outbuf++ = (unsigned char)((reg << (8-m)) & 0xff);

    return outlen+1;
}


STATIC int uncompress_chunk(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outbuflen) {
    int i,j,k,m=0, outlen=0;
    unsigned reg, newdata;

    if (inlen < 4)
        return -1;

    reg = *(inbuf+3) | (*(inbuf+2)<<8) | (*(inbuf+1)<<16) | ((unsigned)*(inbuf+0)<<24);
    inbuf+=4; inlen-=4;

    newdata = (reg>>19)&0x1fff;

    while(1) {
	j = newdata >> 5;

	if(table1[j] == 0xff) {
	    i=1;
	    while((int)newdata >= table2[table3[i]][2]) i++;

	    j=table3[i-1];

            k = j + ((newdata-table2[j][2]) >> (13-table2[j][1]));

	    if(table2[k][0] == 0x100)
		break;
	    else {
		if(outlen>=outbuflen) return -1;
		*outbuf++ = (unsigned char)table2[k][0];
		outlen++;
	    }
	} else {
	    j=table1[j];
	    if(outlen>=outbuflen) return -1;
	    *outbuf++ = (unsigned char)table2[j][0];
	    outlen++;
	}

	m += table2[j][1];

	if(m>=19) {
	    if(m>=8) {
		for(i=m>>3; i; i--) {
		    if(inlen) {
			reg = (reg << 8) | *inbuf++;
			inlen--;
		    } else
			reg = reg << 8;
		}
	    }
	    m = m&7;
	}
	newdata = (reg >> (19-m)) & 0x1fff;
    }
    return outlen;
}

/*
  Reconstruct a new detail level with double resolution in the horizontal direction
  from data from the previous detail level and plus new differential data.
*/
STATIC blxdata *reconstruct_horiz(blxdata *base, blxdata *diff, unsigned rows, unsigned cols, blxdata *out) {
    unsigned int i,j;
    blxdata tmp;

    /* Last column */
    for(i=0; i<rows; i++)
	out[2*(cols*i+cols-1)] = diff[cols*i+cols-1] + (((short)(base[cols*i+cols-2]-base[cols*i+cols-1]-1))>>2);

    /* Intermediate columns */
    for(i=0; i<rows; i++)
	for(j=cols-2; j>0; j--)
	    out[2*(cols*i+j)] = diff[cols*i+j] + (((short)(base[cols*i+j] + 2*(base[cols*i+j-1]-out[2*(cols*i+j+1)])-3*base[cols*i+j+1]+1))>>3);

    /* First column */
    for(i=0; i<rows; i++)
	out[2*cols*i] = diff[cols*i] + (((short)(base[cols*i]-base[cols*i+1]+1))>>2);

    for(i=0; i<rows; i++)
	for(j=0; j<cols; j++) {
	    tmp=base[cols*i+j]+(((short)(out[2*(cols*i+j)]+1))>>1);
	    out[2*cols*i+2*j+1] = tmp-out[2*(cols*i+j)];
	    out[2*cols*i+2*j] = tmp;
	}

    return out;
}

/*
  Reconstruct a new detail level with double resolution in the vertical direction
  from data from the previous detail level and plus new differential data.
*/
STATIC blxdata *reconstruct_vert(blxdata *base, blxdata *diff, unsigned rows, unsigned cols, blxdata *out) {
    unsigned int i,j;
    blxdata tmp;

    /* Last row */
    for(i=0; i<cols; i++)
	out[2*cols*(rows-1)+i] = diff[cols*(rows-1)+i] + (((short)(base[cols*(rows-2)+i]-base[cols*(rows-1)+i]-1))>>2);

    /* Intermediate rows */
    for(i=0; i<cols; i++)
	for(j=rows-2; j>0; j--)
	    out[2*cols*j+i] = diff[cols*j+i] + ((short)((base[cols*j+i] + 2*(base[cols*(j-1)+i]-out[2*cols*(j+1)+i])-3*base[cols*(j+1)+i]+1))>>3);

    /* First row */
    for(i=0; i<cols; i++)
	out[i] = diff[i] + (((short)(base[i]-base[i+cols]+1))>>2);

    for(i=0; i<cols; i++)
	for(j=0; j<rows; j++) {
	    tmp = base[cols*j+i]+(((short)(out[2*cols*j+i]+1))>>1);
	    out[cols*(2*j+1)+i] = tmp-out[2*cols*j+i];
	    out[cols*2*j+i] = tmp;
	}
    return out;
}

/*
  Inverse of reconstruct_horiz
 */
STATIC void decimate_horiz(blxdata *in, unsigned int rows, unsigned int cols, blxdata *base, blxdata *diff) {
    unsigned int i,j;
    blxdata tmp;

    for(i=0; i<rows; i++) {
	for(j=0; j<cols; j+=2) {
	    tmp = in[i*cols+j]-in[i*cols+j+1];
	    diff[i*cols/2+j/2] = tmp;
	    base[i*cols/2+j/2] = in[i*cols+j]-(((short)(tmp+1))>>1);
	}
    }

    /* First column */
    for(i=0; i<rows; i++) {
	diff[cols/2*i] -= ((short)(base[i*cols/2]-base[i*cols/2+1]+1))>>2;
    }

    /* Intermediate columns */
    for(i=0; i<rows; i++)
	for(j=1; j<cols/2-1; j++)
	    diff[cols/2*i+j] -= ((short)(base[cols/2*i+j] + 2*(base[cols/2*i+j-1]-diff[cols/2*i+j+1])-3*base[cols/2*i+j+1]+1))>>3;

    /* Last column */
    for(i=0; i<rows; i++)
	diff[cols/2*i+cols/2-1] -= ((short)(base[i*cols/2+cols/2-2]-base[i*cols/2+cols/2-1]-1))>>2;
}

/*
  Inverse of reconstruct_vert
 */
STATIC void decimate_vert(blxdata *in, unsigned int rows, unsigned int cols, blxdata *base, blxdata *diff) {
    unsigned int i,j;
    blxdata tmp;

    for(i=0; i<rows; i+=2)
	for(j=0; j<cols; j++) {
	    tmp = in[i*cols+j]-in[(i+1)*cols+j];
	    diff[i/2*cols+j] = tmp;
	    base[i/2*cols+j] = in[i*cols+j]-(((short)(tmp+1))>>1);
	}

    /* First row */
    for(j=0; j<cols; j++)
	diff[j] -= ((short)(base[j]-base[cols+j]+1))>>2;


    /* Intermediate rows */
    for(i=1; i<rows/2-1; i++)
	for(j=0; j<cols; j++)
	    diff[cols*i+j] -= ((short)(base[cols*i+j] + 2*(base[cols*(i-1)+j]-diff[cols*(i+1)+j])-3*base[cols*(i+1)+j]+1))>>3;

    /* Last row */
    for(j=0; j<cols; j++)
	diff[cols*(rows/2-1)+j] -= ((short)(base[cols*(rows/2-2)+j]-base[cols*(rows/2-1)+j]-1))>>2;
}

typedef union
{
  short s;
  unsigned short u;
} unionshort;

static int get_short_le(unsigned char **data) {
    /* We assume two's complement representation for this to work */
    unionshort result = { 0 };
    result.u = (unsigned short)(*(*data) | (*(*data+1)<<8));
    *data+=2;
    return result.s;
}

static int get_short_be(unsigned char **data) {
    /* We assume two's complement representation for this to work */
    unionshort result = { 0 };
    result.u = (unsigned short)(*(*data+1) | (*(*data)<<8));
    *data+=2;
    return result.s;
}

static void put_short_le(short data, unsigned char **bufptr) {
    /* We assume two's complement representation for this to work */
    unionshort us = { 0 };
    us.s = data;
    *(*bufptr)++ = (unsigned char)(us.u & 0xff);
    *(*bufptr)++ = (unsigned char)((us.u>>8) & 0xff);
}

static void put_short_be(short data, unsigned char **bufptr) {
    /* We assume two's complement representation for this to work */
    unionshort us = { 0 };
    us.s = data;
    *(*bufptr)++ = (unsigned char)((us.u>>8) & 0xff);
    *(*bufptr)++ = (unsigned char)(us.u & 0xff);
}


static int get_unsigned_short_le(unsigned char **data) {
    int result;

    result = *(*data) | (*(*data+1)<<8);
    *data+=2;
    return result;
}

static int get_unsigned_short_be(unsigned char **data) {
    int result;

    result = *(*data+1) | (*(*data)<<8);
    *data+=2;
    return result;
}

static void put_unsigned_short_le(unsigned short data, unsigned char **bufptr) {
    *(*bufptr)++ = (unsigned char)(data & 0xff);
    *(*bufptr)++ = (unsigned char)((data>>8) & 0xff);
}

static void put_unsigned_short_be(unsigned short data, unsigned char **bufptr) {
    *(*bufptr)++ = (unsigned char)((data>>8) & 0xff);
    *(*bufptr)++ = (unsigned char)(data & 0xff);
}

static int get_short(blxcontext_t *ctx, unsigned char **data) {

    if(ctx->endian == LITTLEENDIAN)
	return get_short_le(data);
    else
	return get_short_be(data);
}

static int get_unsigned_short(blxcontext_t *ctx, unsigned char **data) {

    if(ctx->endian == LITTLEENDIAN)
	return get_unsigned_short_le(data);
    else
	return get_unsigned_short_be(data);
}

static void put_short(blxcontext_t *ctx, short data, unsigned char **bufptr) {
    if(ctx->endian == LITTLEENDIAN)
	put_short_le(data, bufptr);
    else
	put_short_be(data, bufptr);
}

static void put_unsigned_short(blxcontext_t *ctx, unsigned short data, unsigned char **bufptr) {
    if(ctx->endian == LITTLEENDIAN)
	put_unsigned_short_le(data, bufptr);
    else
	put_unsigned_short_be(data, bufptr);
}

typedef union
{
  int i;
  unsigned int u;
} unionint;

static int get_int32(blxcontext_t *ctx, unsigned char **data) {
    /* We assume two's complement representation for this to work */
    unionint result = { 0 };

    if(ctx->endian == LITTLEENDIAN)
	result.u = *(*data) | (*(*data+1)<<8) | (*(*data+2)<<16) | ((unsigned)*(*data+3)<<24);
    else
	result.u = *(*data+3) | (*(*data+2)<<8) | (*(*data+1)<<16) | ((unsigned)*(*data)<<24);
    *data+=4;
    return result.i;
}

static void put_int32(blxcontext_t *ctx, int data, unsigned char **bufptr) {
    /* We assume two's complement representation for this to work */
    unionint ui = { 0 };
    ui.i = data;
    if(ctx->endian == LITTLEENDIAN) {
	*(*bufptr)++ = (unsigned char)(ui.u & 0xff);
	*(*bufptr)++ = (unsigned char)((ui.u>>8) & 0xff);
	*(*bufptr)++ = (unsigned char)((ui.u>>16) & 0xff);
	*(*bufptr)++ = (unsigned char)((ui.u>>24) & 0xff);
    } else {
	*(*bufptr)++ = (unsigned char)((ui.u>>24) & 0xff);
	*(*bufptr)++ = (unsigned char)((ui.u>>16) & 0xff);
	*(*bufptr)++ = (unsigned char)((ui.u>>8) & 0xff);
	*(*bufptr)++ = (unsigned char)(ui.u & 0xff);
    }
}

static int get_unsigned32(blxcontext_t *ctx, unsigned char **data) {
    int result;

    if(ctx->endian == LITTLEENDIAN)
	result = *(*data) | (*(*data+1)<<8) | (*(*data+2)<<16) | ((unsigned)*(*data+3)<<24);
    else
	result = *(*data+3) | (*(*data+2)<<8) | (*(*data+1)<<16) | ((unsigned)*(*data)<<24);
    *data+=4;
    return result;
}

/* Check native endian order */
static int is_big_endian(void)
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	return (byte[0] ? 0:1);
}
static double doubleSWAP(double df)
{
        CPL_SWAP64PTR(&df);
        return df;
}

static double get_double(blxcontext_t *ctx, unsigned char **data) {
    double result;
    memcpy(&result, *data, sizeof(double));
    if((is_big_endian() && ctx->endian == LITTLEENDIAN) ||
       (!is_big_endian() && ctx->endian == BIGENDIAN))
	result = doubleSWAP(result);

    *data+=sizeof(double);

    return result;
}

static void put_double(blxcontext_t *ctx, double data, unsigned char **bufptr) {
    if((is_big_endian() && ctx->endian == LITTLEENDIAN) ||
       (!is_big_endian() && ctx->endian == BIGENDIAN))
	data = doubleSWAP(data);
    memcpy(*bufptr, &data, sizeof(double));
    *bufptr+=sizeof(double);
}

static void put_cellindex_entry(blxcontext_t *ctx, struct cellindex_s *ci, unsigned char **buffer) {
    put_int32(ctx, (int)ci->offset, buffer);
    put_unsigned_short(ctx, (unsigned short)ci->datasize, buffer);
    put_unsigned_short(ctx, (unsigned short)ci->compdatasize, buffer);
}

/* Transpose matrix in-place */
static void transpose(blxdata *data, int rows, int cols) {
    int i,j;
    blxdata tmp;

    for(i=0; i<rows; i++)
	for(j=i+1; j<cols; j++) {
	    tmp=data[i*cols+j];
	    data[i*cols+j]=data[j*cols+i];
	    data[j*cols+i]=tmp;
	}
}

struct lutentry_s {
    blxdata value;
    int frequency;
};

static int lutcmp(const void *aa, const void *bb) {
    const struct lutentry_s *a=aa, *b=bb;

    return b->frequency - a->frequency;
}


int blx_encode_celldata(blxcontext_t *ctx,
                        blxdata *indata,
                        int side,
                        unsigned char *outbuf,
                        CPL_UNUSED int outbufsize) {
    unsigned char *p=outbuf, *tmpdata, *coutstart, *cout=NULL;
    int level, cn, coutsize, zeros;
    blxdata *vdec=NULL, *vdiff=NULL, *c[4] = { NULL }, *tc1, *clut, *indata_scaled;

    struct lutentry_s lut[256];
    int lutsize=0;

    int i, j;

    memset( &lut, 0, sizeof(lut) );
    lut[0].value = 0;

    *p++ = (unsigned char)(side/32-4); /* Resolution */

    /* Allocated memory for buffers */
    indata_scaled = BLXmalloc(sizeof(blxdata)*side*side);
    vdec = BLXmalloc(sizeof(blxdata)*side*side/2);
    vdiff = BLXmalloc(sizeof(blxdata)*side*side/2);
    for(cn=0; cn<4; cn++)
	c[cn] = BLXmalloc(sizeof(blxdata)*side*side/4);
    tc1 = BLXmalloc(sizeof(blxdata)*side*side/4);
    tmpdata = BLXmalloc(5*4*side*side/4);

    /* Scale indata and process undefined values*/
    for(i=0; i<side*side; i++) {
        if((indata[i] == BLX_UNDEF) && ctx->fillundef)
        indata[i] = (blxdata)ctx->fillundefval;
        /* cppcheck-suppress uninitdata */
        indata_scaled[i] = (blxdata)(indata[i] / ctx->zscale);
    }

    indata = indata_scaled;

    cout = tmpdata;

    for(level=0; level < 5; level++) {
	if(ctx->debug) {
	    BLXdebug1("\nlevel=%d\n", level);
	}
	decimate_vert(indata, side, side, vdec, vdiff);
	decimate_horiz(vdec, side/2, side, c[0], c[1]);
	decimate_horiz(vdiff, side/2, side, c[2], c[3]);

	/* For some reason the matrix is transposed if the lut is used for vdec_hdiff */
	for(i=0; i<side/2; i++)
	    for(j=0; j<side/2; j++) {
		tc1[j*side/2+i] = c[1][i*side/2+j];
		tc1[i*side/2+j] = c[1][j*side/2+i];
	    }

	for(cn=1; cn<4; cn++) {
	    /* Use the possibly transposed version of c when building lut */
	    if(cn==1)
		clut=tc1;
	    else
		clut=c[cn];

	    lutsize=0;
	    coutstart = cout;
	    for(i=0; i<side*side/4; i++) {
		/* Find element in lookup table */
		for(j=0; (j<lutsize) && (lut[j].value != clut[i]); j++);

		if(clut[i] != 0) {
		    if(j==lutsize) {
			lut[lutsize].value=clut[i];
			lut[lutsize].frequency=1;
			lutsize++;
			if(lutsize >= 255)
			    break;
		    } else
			lut[j].frequency++;
		}
	    }
	    if(lutsize < 255) {
                /* Since the Huffman table is arranged to let smaller number occupy
		   less bits after the compression, the lookup table is sorted on frequency */
		qsort(lut, lutsize, sizeof(struct lutentry_s), lutcmp);

		zeros = 0;
		for(i=0; i<side*side/4; i++) {
		    if(clut[i] == 0)
			zeros++;
		    if(((zeros>0) && (clut[i]!=0)) || (zeros >= 0x100-lutsize)) {
			*cout++ = (unsigned char)(0x100-zeros);
			zeros=0;
		    }
		    if(clut[i] != 0) {
			for(j=0; (j<lutsize) && (lut[j].value != clut[i]); j++);
			*cout++ = (unsigned char)j;
		    }
		}
		if(zeros>0)
		    *cout++ = (unsigned char)(0x100-zeros);
	    }
	    /* Use the lookuptable only when it pays off to do do.
	       For some reason there cannot be lookup tables in level 4.
               Otherwise Mapsend crashes. */
	    coutsize = (int)(cout-coutstart);
	    if((lutsize < 255) && (coutsize+2*lutsize+1 < 2*side*side/4) && (level < 4)) {
		*p++ = (unsigned char)(lutsize+1);
		for(j=0; j<lutsize; j++)
		    put_short_le(lut[j].value, &p);
		put_short_le((short)coutsize, &p);

		if(ctx->debug) {
		    BLXdebug2("n=%d dlen=%d\n", lutsize+1, coutsize);
		    BLXdebug0("lut={");
		    for(i=0; i<lutsize; i++)
			BLXdebug1("%d, ",lut[i].value);
		    BLXdebug0("}\n");
		}
	    } else {
		*p++=0;
		cout = coutstart;
		for(i=0; i<side*side/4; i++)
		    put_short(ctx, c[cn][i], &cout);
	    }
	}

	side >>= 1;
	indata = c[0];
    }

    memcpy(p, tmpdata, cout-tmpdata);
    p += cout-tmpdata;

    for(i=0; i<side*side; i++)
	put_short(ctx, indata[i], &p);

    *p++=0;

    BLXfree(indata_scaled);
    BLXfree(vdec); BLXfree(vdiff);
    for(cn=0; cn<4; cn++)
	BLXfree(c[cn]);
    BLXfree(tc1);
    BLXfree(tmpdata);

    return (int)(p-outbuf);
}

STATIC blxdata *decode_celldata(blxcontext_t *ctx, unsigned char *inbuf, int len, int *side, blxdata *outbuf, int outbufsize, int overviewlevel) {
    unsigned char *inptr=inbuf;
    int resolution,l_div,level,c,n,i,j,dpos,v,tmp,a,value,l_index,step,cellsize;
    int baseside[12] = { 0 };
    blxdata *base, *diff;
    struct component_s linfo[MAXLEVELS][MAXCOMPONENTS];

    if (len < 1)
    {
        BLXerror0("Cell corrupt");
        return NULL;
    }
    resolution = *inptr++;
    len --;

    tmp = (resolution+4)*32;
    for(l_div=1; l_div<12; l_div++)
	baseside[l_div-1] = tmp >> l_div;

    if(side != NULL)
	*side = tmp >> overviewlevel;

    cellsize = tmp*tmp;
    if (outbufsize < cellsize * (int)sizeof(blxdata))
    {
        BLXerror0("Cell will not fit in output buffer\n");
        return NULL;
    }

    if(outbuf == NULL) {
	BLXerror0("outbuf is NULL");
	return NULL;
    }

    if(ctx->debug) {
	BLXdebug0("==============================\n");
    }

    base = BLXmalloc(2 * baseside[0] * baseside[0] * sizeof(blxdata));
    diff = BLXmalloc(2 * baseside[0] * baseside[0] * sizeof(blxdata));
    if (base == NULL || diff == NULL)
    {
        BLXerror0("Not enough memory\n");
        outbuf = NULL;
        goto error;
    }

    /* Clear level info structure */
    memset(linfo, 0, sizeof(linfo));

    for(level=0; level < 5; level++) {
	for(c=1; c < 4; c++) {
            if (len < 1)
            {
                BLXerror0("Cell corrupt");
                outbuf = NULL;
                goto error;
            }
	    n = *inptr++;
            len --;
	    linfo[level][c].n = n;
	    if(n>0) {
		linfo[level][c].lut = BLXmalloc(sizeof(blxdata)*(n-1));
                if (len < (int)sizeof(short) * n)
                {
                    BLXerror0("Cell corrupt");
                    outbuf = NULL;
                    goto error;
                }
		for(i=0; i<n-1; i++)
		    linfo[level][c].lut[i] = (blxdata)get_short_le(&inptr);
		linfo[level][c].dlen = get_short_le(&inptr);
                if( linfo[level][c].dlen < 0 )
                {
                    BLXerror0("Cell corrupt");
                    outbuf = NULL;
                    goto error;
                }
                len -= sizeof(short) * n;
	    } else {
		linfo[level][c].dlen = 0;
	    }
	}
    }

    for(level=0; level < 5; level++) {
	if(ctx->debug) {
	    BLXdebug1("\nlevel=%d\n", level);
	}

	linfo[level][0].data = BLXmalloc(baseside[level]*baseside[level]*sizeof(blxdata));
        if (linfo[level][0].data == NULL)
        {
            BLXerror0("Not enough memory\n");
            outbuf = NULL;
            goto error;
        }

	for(c=1; c < 4; c++) {
	    if(ctx->debug) {
		BLXdebug2("n=%d dlen=%d\n", linfo[level][c].n, linfo[level][c].dlen);
		BLXdebug0("lut={");
		for(i=0; i<linfo[level][c].n-1; i++)
		    BLXdebug1("%d, ",linfo[level][c].lut[i]);
		BLXdebug0("}\n");
	    }

	    linfo[level][c].data = BLXmalloc(baseside[level]*baseside[level]*sizeof(blxdata));
            if (linfo[level][c].data == NULL)
            {
                BLXerror0("Not enough memory\n");
                outbuf = NULL;
                goto error;
            }

	    if(linfo[level][c].n == 0) {
                if (len < (int)sizeof(short) * baseside[level]*baseside[level])
                {
                    BLXerror0("Cell corrupt");
                    outbuf = NULL;
                    goto error;
                }
		for(i=0; i<baseside[level]*baseside[level]; i++)
		    linfo[level][c].data[i] = (blxdata)get_short(ctx, &inptr);
                len -= sizeof(short) * baseside[level]*baseside[level];
	    } else {
		dpos = 0;
                if (len < linfo[level][c].dlen)
                {
                    BLXerror0("Cell corrupt");
                    outbuf = NULL;
                    goto error;
                }
		for(i=0; i<linfo[level][c].dlen; i++) {
		    v = *inptr++;
		    if(v >= linfo[level][c].n-1) {
                        if(dpos + 256-v > baseside[level]*baseside[level]) {
                            BLXerror0("Cell corrupt\n");
                            outbuf = NULL;
                            goto error;
                        }
			for(j=0; j<256-v; j++)
			    linfo[level][c].data[dpos++] = 0;
		    }
                    else
                    {
                        if(dpos + 1 > baseside[level]*baseside[level]) {
                            BLXerror0("Cell corrupt\n");
                            outbuf = NULL;
                            goto error;
                        }
			linfo[level][c].data[dpos++]=linfo[level][c].lut[v];
                    }
		}
                len -= linfo[level][c].dlen;
		if(c==1)
		    transpose(linfo[level][c].data, baseside[level], baseside[level]);
	    }
	    if(0 && ctx->debug) {
		BLXdebug1("baseside:%d\n",baseside[level]);
		BLXdebug0("data={");
		for(i=0; i<baseside[level]*baseside[level]; i++)
		    BLXdebug1("%d, ",linfo[level][c].data[i]);
		BLXdebug0("}\n");
	    }
	}
    }

    if (len < (int)sizeof(short) * baseside[4]*baseside[4])
    {
        BLXerror0("Cell corrupt");
        outbuf = NULL;
        goto error;
    }
    for(i=0; i<baseside[4]*baseside[4]; i++)
	linfo[4][0].data[i] = (blxdata)get_short(ctx, &inptr);
    len -=sizeof(short) * baseside[4]*baseside[4];

    for(level=4; level >= overviewlevel; level--) {
	if(ctx->debug) {
	    BLXdebug1("baseside:%d\n",baseside[level]);
	    BLXdebug0("inbase={");
	    for(i=0; i<baseside[level]*baseside[level]; i++)
		BLXdebug1("%d, ",linfo[level][0].data[i]);
	    BLXdebug0("}\n");
	    BLXdebug0("indiff={");
	    for(i=0; i<baseside[level]*baseside[level]; i++)
		BLXdebug1("%d, ",linfo[level][1].data[i]);
	    BLXdebug0("}\n");
	}

	reconstruct_horiz(linfo[level][0].data, linfo[level][1].data, baseside[level], baseside[level], base);
	if(ctx->debug) {
	    BLXdebug0("base={");
	    for(i=0; i<baseside[level]*baseside[level]; i++)
		BLXdebug1("%d, ",base[i]);
	    BLXdebug0("}\n");
	}

	reconstruct_horiz(linfo[level][2].data, linfo[level][3].data, baseside[level], baseside[level], diff);
	if(ctx->debug) {
	    BLXdebug0("diff={");
	    for(i=0; i<baseside[level]*baseside[level]; i++)
		BLXdebug1("%d, ",diff[i]);
	    BLXdebug0("}\n");
	}
	if(level>overviewlevel)
	    reconstruct_vert(base, diff, baseside[level], 2*baseside[level], linfo[level-1][0].data);
	else
	    reconstruct_vert(base, diff, baseside[level], 2*baseside[level], outbuf);
    }

    if(overviewlevel == 0) {
        if (len < 1)
        {
            BLXerror0("Cell corrupt");
            outbuf = NULL;
            goto error;
        }
	a = *((char *)inptr++);
        len --;
	l_index=0;
	while(len >= 3) {
	    step = inptr[0] | (inptr[1]<<8); inptr+=2;
	    value = *((char *)inptr++);
            len -= 3;

	    l_index += step;

	    if(value & 1)
		value = (value-1)/2-a;
	    else
		value = value/2+a;

	    if(l_index>=cellsize) {
		BLXerror0("Cell data corrupt\n");
		outbuf = NULL;
		goto error;
	    }

	    outbuf[l_index] = outbuf[l_index] + (blxdata)value;
	}

        if (len != 0)
            BLXdebug1("remaining len=%d", len);
    }
    else
    {
        if (len != 1)
            BLXdebug1("remaining len=%d", len);
    }

    /* Scale data */
    for(i=0; i<cellsize; i++)
    {
        int val = outbuf[i] * (blxdata)ctx->zscale;
        if( val < SHRT_MIN )
            outbuf[i] = SHRT_MIN;
        else if( val > SHRT_MAX )
            outbuf[i] = SHRT_MAX;
        else
            outbuf[i] = (blxdata)val;
    }

 error:
    if (base != NULL)
         BLXfree(base);
    if (diff != NULL)
         BLXfree(diff);

    /* Free allocated memory */
    for(level=4; level >= 0; level--)
	for(c=0; c<4; c++) {
	    if(linfo[level][c].lut)
		BLXfree(linfo[level][c].lut);
	    if(linfo[level][c].data)
		BLXfree(linfo[level][c].data);
	}

    return outbuf;
}

blxcontext_t *blx_create_context() {
    blxcontext_t *c;

    c = BLXmalloc(sizeof(blxcontext_t));

    memset(c,0,sizeof(blxcontext_t));

    c->cell_ysize = c->cell_xsize = 128;

    c->minval = 32767;
    c->maxval = -32768;

    c->debug = 0;

    c->zscale = 1;

    c->fillundef = 1;
    c->fillundefval = 0;

    return c;
}

void blx_free_context(blxcontext_t *ctx) {
    if(ctx->cellindex)
	BLXfree(ctx->cellindex);

    BLXfree(ctx);
}

void blxprintinfo(blxcontext_t *ctx) {
    BLXnotice2("Lat: %f Lon: %f\n", ctx->lat, ctx->lon);
    BLXnotice2("Pixelsize: Lat: %f Lon: %f\n", 3600*ctx->pixelsize_lat, 3600*ctx->pixelsize_lon);
    BLXnotice2("Size %dx%d\n", ctx->xsize, ctx->ysize);
    BLXnotice2("Cell size %dx%d\n", ctx->cell_xsize, ctx->cell_ysize);
    BLXnotice2("Cell grid %dx%d\n", ctx->cell_cols, ctx->cell_rows);
    BLXnotice1("Ysize scale factor: %d\n", ctx->zscale);
    BLXnotice1("Max Ysize: %d\n", ctx->zscale * ctx->maxval);
    BLXnotice1("Min Ysize: %d\n", ctx->zscale * ctx->minval);
    BLXnotice1("Max chunksize: %d\n", ctx->maxchunksize);
}

int blx_checkheader(const char *header) {
    const short *signature=(const short *)header;

    return ((signature[0]==0x4) && (signature[1]==0x66)) ||
	((signature[0]==0x400) && (signature[1]==0x6600));
}

static void blx_generate_header(blxcontext_t *ctx, unsigned char *header) {
    unsigned char *hptr = header;

    memset(header, 0, 102);

    /* Write signature */
    put_short(ctx, 0x4, &hptr); // 0
    put_short(ctx, 0x66, &hptr); // 2

    put_int32(ctx, ctx->cell_xsize*ctx->cell_cols, &hptr); // 4
    put_int32(ctx, ctx->cell_ysize*ctx->cell_rows, &hptr); // 8

    put_short(ctx, (short)ctx->cell_xsize, &hptr); // 12
    put_short(ctx, (short)ctx->cell_ysize, &hptr); // 14

    put_short(ctx, (short)ctx->cell_cols, &hptr); // 16
    put_short(ctx, (short)ctx->cell_rows, &hptr); // 18

    put_double(ctx, ctx->lon, &hptr); // 20
    put_double(ctx, -ctx->lat, &hptr); // 28

    put_double(ctx, ctx->pixelsize_lon, &hptr);	// 36
    put_double(ctx, -ctx->pixelsize_lat, &hptr); // 44

    put_short(ctx, (short)ctx->minval, &hptr); // 52
    put_short(ctx, (short)ctx->maxval, &hptr); // 54
    put_short(ctx, (short)ctx->zscale, &hptr); // 56
    put_int32(ctx, ctx->maxchunksize, &hptr); // 58
    // 62
}

int blx_writecell(blxcontext_t *ctx, blxdata *cell, int cellrow, int cellcol) {
    unsigned char *uncompbuf=NULL,*outbuf=NULL;
    int bufsize, uncompsize, compsize;
    int status=0;
    int i,allundef;

    /* Calculate statistics and find out if all elements have undefined values */
    allundef=1;
    for(i=0; i < ctx->cell_xsize*ctx->cell_ysize; i++) {
	if(cell[i] > ctx->maxval)
	    ctx->maxval = cell[i];
	if(cell[i] < ctx->minval)
	    ctx->minval = cell[i];
	if(cell[i]!=BLX_UNDEF)
	    allundef=0;
    }

    if(allundef)
	return status;

    if(ctx->debug)
	BLXdebug2("Writing cell (%d,%d)\n",cellrow, cellcol);

    if(!ctx->open) {
	status=-3;
	goto error;
    }

    if((cellrow >= ctx->cell_rows) || (cellcol >= ctx->cell_cols)) {
	status=-2;
	goto error;
    }

    bufsize = sizeof(blxdata)*ctx->cell_xsize*ctx->cell_ysize+1024;
    uncompbuf = BLXmalloc(bufsize);
    outbuf = BLXmalloc(bufsize);

    uncompsize = blx_encode_celldata(ctx, cell, ctx->cell_xsize, uncompbuf, bufsize);
    compsize = compress_chunk(uncompbuf, uncompsize, outbuf, bufsize);
    if (compsize < 0)
    {
        BLXerror0("Couldn't compress chunk");
        status = -1;
        goto error;
    }

    if(uncompsize > ctx->maxchunksize)
	ctx->maxchunksize = uncompsize;

    ctx->cellindex[cellrow*ctx->cell_cols + cellcol].offset = (int)BLXftell(ctx->fh);
    ctx->cellindex[cellrow*ctx->cell_cols + cellcol].datasize = uncompsize;
    ctx->cellindex[cellrow*ctx->cell_cols + cellcol].compdatasize = compsize;

    if((int)BLXfwrite(outbuf, 1, compsize, ctx->fh) != compsize) {
	status=-1;
	goto error;
    }

 error:
    if(uncompbuf)
	BLXfree(uncompbuf);
    if(outbuf)
	BLXfree(outbuf);
    return status;
}

int blxopen(blxcontext_t *ctx, const char *filename, const char *rw) {
    unsigned char header[102],*hptr;
    int signature[2] = { 0 };
    int i,j;
    struct cellindex_s *ci;

    if(!strcmp(rw, "r") || !strcmp(rw, "rb"))
	ctx->write=0;
    else if(!strcmp(rw,"w") || !strcmp(rw, "wb"))
	ctx->write=1;
    else
	goto error;

    ctx->fh = BLXfopen(filename, rw);

    if(ctx->fh == NULL)
	goto error;

    hptr = header;
    if(ctx->write) {
	blx_generate_header(ctx, header);

	if(BLXfwrite(header, 1, 102, ctx->fh) != 102)
	    goto error;

	ctx->cellindex = BLXmalloc(sizeof(struct cellindex_s) * ctx->cell_rows * ctx->cell_cols);
	if(ctx->cellindex == NULL) {
	    goto error;
	}
	memset(ctx->cellindex, 0, sizeof(struct cellindex_s) * ctx->cell_rows * ctx->cell_cols);

	/* Write the empty cell index (this will be overwritten when we have actual data)*/
	for(i=0;i<ctx->cell_rows;i++)
	    for(j=0;j<ctx->cell_cols;j++) {
		hptr = header;
		put_cellindex_entry(ctx, ctx->cellindex+i*ctx->cell_cols+j, &hptr);
		if((int)BLXfwrite(header, 1, hptr-header, ctx->fh) != (int)(hptr-header))
		    goto error;
	    }

    } else {
	/* Read header */
	if(BLXfread(header, 1, 102, ctx->fh) != 102)
	    goto error;

	signature[0] = get_short_le(&hptr);
	signature[1] = get_short_le(&hptr);

	/* Determine if the endianness of the BLX file */
	if((signature[0] == 0x4) && (signature[1] == 0x66))
	    ctx->endian = LITTLEENDIAN;
	else {
	    hptr = header;
	    signature[0] = get_short_be(&hptr);
	    signature[1] = get_short_be(&hptr);
	    if((signature[0] == 0x4) && (signature[1] == 0x66))
		ctx->endian = BIGENDIAN;
	    else
		goto error;
	}

	ctx->xsize = get_int32(ctx, &hptr);
	ctx->ysize = get_int32(ctx, &hptr);
        if (ctx->xsize <= 0 || ctx->ysize <= 0)
        {
            BLXerror0("Invalid raster size");
            goto error;
        }

	ctx->cell_xsize = get_short(ctx, &hptr);
	ctx->cell_ysize = get_short(ctx, &hptr);
        if (ctx->cell_xsize <= 0 ||
            ctx->cell_ysize <= 0)
        {
            BLXerror0("Invalid cell size");
            goto error;
        }

	ctx->cell_cols = get_short(ctx, &hptr);
	ctx->cell_rows = get_short(ctx, &hptr);
        if (ctx->cell_cols <= 0 || ctx->cell_cols > 10000 ||
            ctx->cell_rows <= 0 || ctx->cell_rows > 10000)
        {
            BLXerror0("Invalid cell number");
            goto error;
        }

	ctx->lon = get_double(ctx, &hptr);
	ctx->lat = -get_double(ctx, &hptr);

	ctx->pixelsize_lon = get_double(ctx, &hptr);
	ctx->pixelsize_lat = -get_double(ctx, &hptr);

	ctx->minval = get_short(ctx, &hptr);
	ctx->maxval = get_short(ctx, &hptr);
	ctx->zscale = get_short(ctx, &hptr);
	ctx->maxchunksize = get_int32(ctx, &hptr);

	ctx->cellindex = BLXmalloc(sizeof(struct cellindex_s) * ctx->cell_rows * ctx->cell_cols);
	if(ctx->cellindex == NULL) {
	    goto error;
	}

	for(i=0;i<ctx->cell_rows;i++)
	    for(j=0;j<ctx->cell_cols;j++) {
		/* Read cellindex entry */
		if(BLXfread(header, 1, 8, ctx->fh) != 8)
		    goto error;
		hptr=header;

		ci = &ctx->cellindex[i*ctx->cell_cols + j];
		ci->offset = get_unsigned32(ctx, &hptr);
		ci->datasize = get_unsigned_short(ctx, &hptr);
		ci->compdatasize = get_unsigned_short(ctx, &hptr);
	    }
    }
    ctx->open = 1;

    return 0;

 error:
    return -1;
}

int blxclose(blxcontext_t *ctx) {
    unsigned char header[102],*hptr;
    int i,j,status=0;

    if(ctx->write) {
	/* Write updated header and cellindex */
        if (BLXfseek(ctx->fh, 0, SEEK_SET) != 0) {
            status=-1;
            goto error;
        }

	blx_generate_header(ctx, header);

	if(BLXfwrite(header, 1, 102, ctx->fh) != 102) {
	    status=-1;
	    goto error;
	}
	for(i=0;i<ctx->cell_rows;i++)
	    for(j=0;j<ctx->cell_cols;j++) {
		hptr = header;
		put_cellindex_entry(ctx, ctx->cellindex+i*ctx->cell_cols+j, &hptr);
		if((int)BLXfwrite(header, 1, hptr-header, ctx->fh) != (int)(hptr-header)) {
		    status=-1;
		    break;
		}
	    }
    }
    ctx->open = 1;

 error:
    if(ctx->fh)
	BLXfclose(ctx->fh);

    return status;
}

short *blx_readcell(blxcontext_t *ctx, int row, int col, short *buffer, int bufsize, int overviewlevel) {
    struct cellindex_s *ci;
    unsigned char *chunk=NULL, *cchunk=NULL;
    blxdata *tmpbuf=NULL;
    int tmpbufsize,i;
    short *result=NULL;
    int npoints;

    if((ctx==NULL) || (row >= ctx->cell_rows) || (col >= ctx->cell_cols))
	return NULL;

    ci = &ctx->cellindex[row*ctx->cell_cols + col];

    npoints = (ctx->cell_xsize*ctx->cell_ysize)>>(2*overviewlevel) ;
    if (bufsize < npoints * (int)sizeof(short))
        return NULL;

    if(ci->datasize == 0) {
	for(i=0; i<npoints; i++)
	    buffer[i] = BLX_UNDEF;
    } else {
	if(BLXfseek(ctx->fh, ci->offset, SEEK_SET) != 0)
            goto error;

	chunk = BLXmalloc(ci->datasize);
	cchunk = BLXmalloc(ci->compdatasize);

	if((chunk == NULL) || (cchunk == NULL))
	    goto error;

	if(BLXfread(cchunk, 1, ci->compdatasize, ctx->fh) != ci->compdatasize)
	    goto error;

        if((unsigned int)uncompress_chunk(cchunk, ci->compdatasize, chunk, ci->datasize) != ci->datasize)
	    goto error;

	tmpbufsize = sizeof(blxdata)*ctx->cell_xsize*ctx->cell_ysize;
	tmpbuf = BLXmalloc(tmpbufsize);
        if (tmpbuf == NULL)
            goto error;

	if (decode_celldata(ctx, chunk, ci->datasize, NULL, tmpbuf, tmpbufsize, overviewlevel) == NULL)
            goto error;

	for(i=0; i<npoints; i++)
	    buffer[i] = tmpbuf[i];
    }

    result = buffer;

 error:
    if(chunk)
	BLXfree(chunk);
    if(cchunk)
	BLXfree(cchunk);
    if(tmpbuf)
	BLXfree(tmpbuf);

    return result;
}
