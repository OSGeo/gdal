#ifndef IdrisiRasterDoc_h
#define IdrisiRasterDoc_h

#include "IdrisiRasterUtils.h"

#define MAXSTRLEN				512
#define MINSTRLEN				80
#define PALHEADERSZ				18
#define MAXLEGENDDEFAULT		25

#define RST_DT_BYTE				0
#define RST_DT_INTEGER			1
#define RST_DT_RGB24			2
#define RST_DT_REAL				3

/* attribute labels in a Idrisi Raste Documentation File */
#define LABEL_DOC_FILE_FORMAT	"file format"
#define LABEL_DOC_FILE_TITLE	"file title"
#define LABEL_DOC_DATA_TYPE		"data type"
#define LABEL_DOC_FILE_TYPE		"file type"
#define LABEL_DOC_COLUMNS		"columns"
#define LABEL_DOC_ROWS			"rows"
#define LABEL_DOC_REF_SYSTEM	"ref. system"
#define LABEL_DOC_REF_UNITS		"ref. units"
#define LABEL_DOC_UNIT_DIST		"unit dist."
#define LABEL_DOC_MIN_X			"min. X"
#define LABEL_DOC_MAX_X			"max. X"
#define LABEL_DOC_MIN_Y			"min. Y"
#define LABEL_DOC_MAX_Y			"max. Y"
#define LABEL_DOC_POSN_ERROR	"pos'n error"
#define LABEL_DOC_RESOLUTION	"resolution"
#define LABEL_DOC_MIN_VALUE		"min. value"
#define LABEL_DOC_MAX_VALUE		"max. value"
#define LABEL_DOC_DISPLAY_MIN	"display min"
#define LABEL_DOC_DISPLAY_MAX	"display max"
#define LABEL_DOC_VALUE_UNITS	"value units"
#define LABEL_DOC_VALUE_ERROR	"value error"
#define LABEL_DOC_FLAG_VALUE	"flag value"
#define LABEL_DOC_FLAG_DEFN		"flag def'n"
#define LABEL_DOC_LEGEND_CATS	"legend cats"
#define LABEL_DOC_CODE_N		"code"
#define LABEL_DOC_lineages		"lineage"
#define LABEL_DOC_comments		"comment"

/* attribute values in a Idrisi Raste Documentation File */
#define VALUE_DOC_TITLE			""
#define VALUE_DOC_UNKNOW		"unknown"
#define VALUE_DOC_NONE			"none"
#define VALUE_DOC_UNSPECIFIED	"unspecified"
#define VALUE_DOC_FILE_FORMAT	"IDRISI Raster A.1"
#define VALUE_DOC_BYNARY		"binary"
#define VALUE_DOC_BYTE			"byte"
#define VALUE_DOC_INTEGER		"integer"
#define VALUE_DOC_RGB			"RGB24"
#define VALUE_DOC_REAL			"real"
#define VALUE_DOC_LATLONG		"latlong"
#define VALUE_DOC_DEGREE		"degree"
#define VALUE_DOC_PLANE			"plane"
#define VALUE_DOC_METER			"meters"
#define VALUE_DOC_BACKGROUND	"background"

/* variable printing format */
#define NUMERIC_FORMAT_INT		"%-12s: %.0f\n"
#define NUMERIC_FORMAT_FLOAT	"%-12s: %.7f\n"

/* Idrisi Raster Documentation record */
typedef struct _rst_Doc {
	char			file_format[MAXSTRLEN];
	char			file_title[MAXSTRLEN];
	unsigned long	data_type;
	char			file_type[MINSTRLEN];
	unsigned long	columns;
	unsigned long	rows;
	char			ref_system[MINSTRLEN];
	char			ref_units[MINSTRLEN];
	double 			unit_dist;
	double 			min_X;
	double 			max_X;
	double 			min_Y;
	double 			max_Y;
	char  			posn_error[MINSTRLEN];
	double 			resolution;
	double 			min_value[3];
	double 			max_value[3];
	double 			display_min[3];
	double 			display_max[3];
	char			value_units[MINSTRLEN];
	char 			value_error[MINSTRLEN];
	double 			flag_value;
	char     		flag_defn[MINSTRLEN];
	unsigned long	legend_cats;
	unsigned int	comments_count;
	unsigned int	lineages_count;
	unsigned int	is_thematic;
	unsigned long	*codes;
	char			**categories;
	char			**lineages;
	char			**comments;
} rst_Doc;

static int max_LegendCats = -1;

rst_Doc *CreateImgDoc();
rst_Doc *ReadImgDoc(const char *fileName);
void FreeImgDoc(rst_Doc *imgDoc);
void WriteImgDoc(rst_Doc *imgDoc, char *fileName);

long ReadPalette(const char *fileName, int rgb_index, double *colorTable, int rowCount, int thematic);
void WritePalette(const char *fileName, int rgb_index, double *colorTable, int rowCount);

#endif /* IdrisiRasterDoc_h */
