/******************************************************************************
* In order to make life a little bit easier when using the GIF file format,   *
* this library was written, and which does all the dirty work...              *
*                                                                             *
*                                        Written by Gershon Elber,  Jun. 1989 *
*                                        Hacks by Eric S. Raymond,  Sep. 1992 *
*******************************************************************************
* History:                                                                    *
* 14 Jun 89 - Version 1.0 by Gershon Elber.                                   *
*  3 Sep 90 - Version 1.1 by Gershon Elber (Support for Gif89, Unique names)  *
* 15 Sep 90 - Version 2.0 by Eric S. Raymond (Changes to suoport GIF slurp)   *
* 26 Jun 96 - Version 3.0 by Eric S. Raymond (Full GIF89 support)             * 
* 17 Dec 98 - Version 4.0 by Toshio Kuratomi (Fix extension writing code)     *
******************************************************************************/

#ifndef GIF_LIB_H
#define GIF_LIB_H

#define GIF_LIB_VERSION	" Version 4.0, "

#define	GIF_ERROR	0
#define GIF_OK		1

#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif

#ifndef NULL
#define NULL		0
#endif /* NULL */

#define GIF_FILE_BUFFER_SIZE 16384  /* Files uses bigger buffers than usual. */

typedef	int		GifBooleanType;
typedef	unsigned char	GifPixelType;
typedef unsigned char *	GifRowType;
typedef unsigned char	GifByteType;

#define GIF_MESSAGE(Msg) fprintf(stderr, "\n%s: %s\n", PROGRAM_NAME, Msg)
#define GIF_EXIT(Msg)	{ GIF_MESSAGE(Msg); exit(-3); }

#ifdef SYSV
#define VoidPtr char *
#else
#define VoidPtr void *
#endif /* SYSV */

typedef struct GifColorType {
    GifByteType Red, Green, Blue;
} GifColorType;

typedef struct ColorMapObject
{
    int	ColorCount;
    int BitsPerPixel;
    GifColorType *Colors;		/* on malloc(3) heap */
}
ColorMapObject;

typedef struct GifImageDesc {
    int Left, Top, Width, Height,	/* Current image dimensions. */
	Interlace;			/* Sequential/Interlaced lines. */
    ColorMapObject *ColorMap;		/* The local color map */
} GifImageDesc;

typedef struct GifFileType {
    int SWidth, SHeight,		/* Screen dimensions. */
	SColorResolution, 		/* How many colors can we generate? */
	SBackGroundColor;		/* I hope you understand this one... */
    ColorMapObject *SColorMap;		/* NULL if not exists. */
    int ImageCount;			/* Number of current image */
    GifImageDesc Image;			/* Block describing current image */
    struct SavedImage *SavedImages;	/* Use this to accumulate file state */
    VoidPtr UserData;           /* hook to attach user data (TVT) */
    VoidPtr Private;	  		/* Don't mess with this! */
} GifFileType;

typedef enum {
    UNDEFINED_RECORD_TYPE,
    SCREEN_DESC_RECORD_TYPE,
    IMAGE_DESC_RECORD_TYPE,		/* Begin with ',' */
    EXTENSION_RECORD_TYPE,		/* Begin with '!' */
    TERMINATE_RECORD_TYPE		/* Begin with ';' */
} GifRecordType;

/* DumpScreen2Gif routine constants identify type of window/screen to dump.  */
/* Note all values below 1000 are reserved for the IBMPC different display   */
/* devices (it has many!) and are compatible with the numbering TC2.0        */
/* (Turbo C 2.0 compiler for IBM PC) gives to these devices.		     */
typedef enum {
    GIF_DUMP_SGI_WINDOW = 1000,
    GIF_DUMP_X_WINDOW = 1001
} GifScreenDumpType;

/* func type to read gif data from arbitrary sources (TVT) */
typedef int (*InputFunc)(GifFileType*,GifByteType*,int);

/* func type to write gif data ro arbitrary targets.
 * Returns count of bytes written. (MRB)
 */
typedef int (*OutputFunc)(GifFileType *, const GifByteType *, int);
/******************************************************************************
*  GIF89 extension function codes                                             *
******************************************************************************/

#define COMMENT_EXT_FUNC_CODE		0xfe	/* comment */
#define GRAPHICS_EXT_FUNC_CODE		0xf9	/* graphics control */
#define PLAINTEXT_EXT_FUNC_CODE		0x01	/* plaintext */
#define APPLICATION_EXT_FUNC_CODE	0xff	/* application block */

/******************************************************************************
* O.K., here are the routines one can access in order to encode GIF file:     *
* (GIF_LIB file EGIF_LIB.C).						      *
******************************************************************************/

GifFileType *EGifOpenFileName(char *GifFileName, int GifTestExistance);
GifFileType *EGifOpenFileHandle(int GifFileHandle);
GifFileType *EgifOpen(void *userPtr, OutputFunc writeFunc);
int EGifSpew(GifFileType *GifFile);
void EGifSetGifVersion(char *Version);
int EGifPutScreenDesc(GifFileType *GifFile,
	int GifWidth, int GifHeight, int GifColorRes, int GifBackGround,
	ColorMapObject *GifColorMap);
int EGifPutImageDesc(GifFileType *GifFile,
	int GifLeft, int GifTop, int Width, int GifHeight, int GifInterlace,
	ColorMapObject *GifColorMap);
int EGifPutLine(GifFileType *GifFile, GifPixelType *GifLine, int GifLineLen);
int EGifPutPixel(GifFileType *GifFile, GifPixelType GifPixel);
int EGifPutComment(GifFileType *GifFile, char *GifComment);
int EGifPutExtension(GifFileType *GifFile, int GifExtCode, int GifExtLen,
							VoidPtr GifExtension);
int EGifPutCode(GifFileType *GifFile, int GifCodeSize,
						   GifByteType *GifCodeBlock);
int EGifPutCodeNext(GifFileType *GifFile, GifByteType *GifCodeBlock);
int EGifCloseFile(GifFileType *GifFile);

#define	E_GIF_ERR_OPEN_FAILED	1		/* And EGif possible errors. */
#define	E_GIF_ERR_WRITE_FAILED	2
#define E_GIF_ERR_HAS_SCRN_DSCR	3
#define E_GIF_ERR_HAS_IMAG_DSCR	4
#define E_GIF_ERR_NO_COLOR_MAP	5
#define E_GIF_ERR_DATA_TOO_BIG	6
#define E_GIF_ERR_NOT_ENOUGH_MEM 7
#define E_GIF_ERR_DISK_IS_FULL	8
#define E_GIF_ERR_CLOSE_FAILED	9
#define E_GIF_ERR_NOT_WRITEABLE	10

/******************************************************************************
* O.K., here are the routines one can access in order to decode GIF file:     *
* (GIF_LIB file DGIF_LIB.C).						      *
******************************************************************************/

GifFileType *DGifOpenFileName(const char *GifFileName);
GifFileType *DGifOpenFileHandle(int GifFileHandle);
GifFileType *DGifOpen( void* userPtr, InputFunc readFunc );  /* new one (TVT) */
int DGifSlurp(GifFileType *GifFile);
int DGifGetScreenDesc(GifFileType *GifFile);
int DGifGetRecordType(GifFileType *GifFile, GifRecordType *GifType);
int DGifGetImageDesc(GifFileType *GifFile);
int DGifGetLine(GifFileType *GifFile, GifPixelType *GifLine, int GifLineLen);
int DGifGetPixel(GifFileType *GifFile, GifPixelType GifPixel);
int DGifGetComment(GifFileType *GifFile, char *GifComment);
int DGifGetExtension(GifFileType *GifFile, int *GifExtCode,
						GifByteType **GifExtension);
int DGifGetExtensionNext(GifFileType *GifFile, GifByteType **GifExtension);
int DGifGetCode(GifFileType *GifFile, int *GifCodeSize,
						GifByteType **GifCodeBlock);
int DGifGetCodeNext(GifFileType *GifFile, GifByteType **GifCodeBlock);
int DGifGetLZCodes(GifFileType *GifFile, int *GifCode);
int DGifCloseFile(GifFileType *GifFile);

#define	D_GIF_ERR_OPEN_FAILED	101		/* And DGif possible errors. */
#define	D_GIF_ERR_READ_FAILED	102
#define	D_GIF_ERR_NOT_GIF_FILE	103
#define D_GIF_ERR_NO_SCRN_DSCR	104
#define D_GIF_ERR_NO_IMAG_DSCR	105
#define D_GIF_ERR_NO_COLOR_MAP	106
#define D_GIF_ERR_WRONG_RECORD	107
#define D_GIF_ERR_DATA_TOO_BIG	108
#define D_GIF_ERR_NOT_ENOUGH_MEM 109
#define D_GIF_ERR_CLOSE_FAILED	110
#define D_GIF_ERR_NOT_READABLE	111
#define D_GIF_ERR_IMAGE_DEFECT	112
#define D_GIF_ERR_EOF_TOO_SOON	113

/******************************************************************************
* O.K., here are the routines from GIF_LIB file QUANTIZE.C.		      *
******************************************************************************/
int QuantizeBuffer(unsigned int Width, unsigned int Height, int *ColorMapSize,
	GifByteType *RedInput, GifByteType *GreenInput, GifByteType *BlueInput,
	GifByteType *OutputBuffer, GifColorType *OutputColorMap);


/******************************************************************************
* O.K., here are the routines from GIF_LIB file QPRINTF.C.		      *
******************************************************************************/
extern int GifQuietPrint;

#ifdef USE_VARARGS
extern void GifQprintf();
#else
extern void GifQprintf(char *Format, ...);
#endif /* USE_VARARGS */

/******************************************************************************
* O.K., here are the routines from GIF_LIB file GIF_ERR.C.		      *
******************************************************************************/
extern void PrintGifError(void);
extern int GifLastError(void);

/******************************************************************************
* O.K., here are the routines from GIF_LIB file DEV2GIF.C.		      *
******************************************************************************/
extern int DumpScreen2Gif(char *FileName,
			  int ReqGraphDriver,
			  int ReqGraphMode1,
			  int ReqGraphMode2,
			  int ReqGraphMode3);

/*****************************************************************************
 *
 * Everything below this point is new after version 1.2, supporting `slurp
 * mode' for doing I/O in two big belts with all the image-bashing in core.
 *
 *****************************************************************************/

/******************************************************************************
* Color Map handling from ALLOCGIF.C					      *
******************************************************************************/

extern ColorMapObject *MakeMapObject(int ColorCount, GifColorType *ColorMap);
extern void FreeMapObject(ColorMapObject *Object);
extern ColorMapObject *UnionColorMap(ColorMapObject *ColorIn1,
			      ColorMapObject *ColorIn2,
			      GifPixelType ColorTransIn2[]);
extern int BitSize(int n);

/******************************************************************************
* Support for the in-core structures allocation (slurp mode).		      *
******************************************************************************/

/* This is the in-core version of an extension record */
typedef struct {
    int		ByteCount;
    char	*Bytes;		/* on malloc(3) heap */
    int Function;       /* Holds the type of the Extension block. */
} ExtensionBlock;

/* This holds an image header, its unpacked raster bits, and extensions */
typedef struct SavedImage {
    GifImageDesc	ImageDesc;

    char		*RasterBits;		/* on malloc(3) heap */

    int			Function; /* DEPRECATED: Use ExtensionBlocks[x].Function
                           * instead */
    int			ExtensionBlockCount;
    ExtensionBlock	*ExtensionBlocks;	/* on malloc(3) heap */
} SavedImage;

extern void ApplyTranslation(SavedImage *Image, GifPixelType Translation[]);

extern void MakeExtension(SavedImage *New, int Function);
extern int AddExtensionBlock(SavedImage *New, int Len, char ExtData[]);
extern void FreeExtension(SavedImage *Image);

extern SavedImage *MakeSavedImage(GifFileType *GifFile, SavedImage *CopyFrom);
extern void FreeSavedImages(GifFileType *GifFile);

/******************************************************************************
* The library's internal utility font					      *
******************************************************************************/

#define GIF_FONT_WIDTH	8
#define GIF_FONT_HEIGHT	8
extern unsigned char AsciiTable[][GIF_FONT_WIDTH];

extern void DrawText(SavedImage *Image,
		     const int x, const int y,
		     const char *legend,
		     const int color);

extern void DrawBox(SavedImage *Image,
		     const int x, const int y,
		     const int w, const int d,
		     const int color);

void DrawRectangle(SavedImage *Image,
		     const int x, const int y,
		     const int w, const int d,
		     const int color);

extern void DrawBoxedText(SavedImage *Image,
		     const int x, const int y,
		     const char *legend,
		     const int border,
		     const int bg,
		     const int fg);

#endif /* GIF_LIB_H */
