#ifndef IdrisiRasterUtils_h
#define IdrisiRasterUtils_h

/*  System includes */
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <xmath.h>
#include <windows.h>
#include <time.h>
#include <shlwapi.h>
#include <winreg.h>

#define SUCCESS			0
#define FAILURE			-1

#define RDCSEPARATOR	12

#if USE_CALLOC
#define CALLOC( s, n )	calloc( s, n )
#define	FREE( p )		free( p )
#else
#define CALLOC( s, n )	HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, (DWORD)(s)*(n) )
#define FREE( p )		HeapFree( GetProcessHeap(), (DWORD) 0.0, p )
#endif

char  *allocTextString(const char* text);
char  *allocTextString(const char* text);
char  *Backslash2Slash(char * str);
char  *ReadValueAsString(FILE *stream, const char *Field);
int    DataTypeAsInteger(const char * dataType);
float  ReadValueAsFloat(FILE *stream, const char *Field);
int    ReadValueAsInteger(FILE *stream, const char *Field);
void   ReadValueAsArrayFloat(FILE *stream, const char *Field, double *Value);
char  *ReadParameterString(const char *ParameterStr, const char *search_string);
float  ReadParameterFloat(const char *ParameterStr, const char *search_string);
void   ReadValueAsLegendTable(FILE *stream, unsigned long numCats, char *** categories, unsigned long ** codes);
void   ReadcommentsLines(FILE *stream, unsigned int *counter, char *** comments_lines, const char *search_string);
void   FindInIdrisiInstallation(char * pathName, char * fileName);

char  *GetFromUserPreference(char * key);
int  GetMaxLegendsCats();

#endif /* IdrisiRasterUtils_h */
