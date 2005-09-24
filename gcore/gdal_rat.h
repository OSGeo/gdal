#include "cpl_minixml.h"

class GDALRasterAttributeField
{
public:
    CPLString         sName;

    GDALRATFieldType  eType;

    GDALRATFieldUsage eUsage;

    std::vector<GInt32> anValues;
    std::vector<double> adfValues;
    std::vector<CPLString> aosValues;
};

class CPL_DLL GDALRasterAttributeTable 
{
    friend const char * CPL_STDCALL GDALRATGetNameOfCol( GDALRasterAttributeTableH, int );
    friend const char * CPL_STDCALL GDALRATGetValueAsString( GDALRasterAttributeTableH, int, int );

private:
    std::vector<GDALRasterAttributeField> aoFields;

    int bRegularBinning;
    double dfRow0Min;
    double dfBinSize;

    int   nMinCol;
    int   nMaxCol;

    int   nRowCount;

    CPLString     osWorkingResult;

public:
    GDALRasterAttributeTable();
    GDALRasterAttributeTable(const GDALRasterAttributeTable&);
    ~GDALRasterAttributeTable();

    GDALRasterAttributeTable *Clone() const;
    
    int           GetColumnCount() const;

    CPLString     GetNameOfCol( int ) const;
    GDALRATFieldUsage GetUsageOfCol( int ) const;
    GDALRATFieldType GetTypeOfCol( int ) const;
    
    int           GetColOfUsage( GDALRATFieldUsage ) const;

    int           GetRowCount() const;

    CPLString     GetValueAsString( int iRow, int iField ) const;
    int           GetValueAsInt( int iRow, int iField ) const;
    double        GetValueAsDouble( int iRow, int iField ) const;

    void          SetValue( int iRow, int iField, const char *pszValue );
    void          SetValue( int iRow, int iField, double dfValue);
    void          SetValue( int iRow, int iField, int nValue );
    void          SetRowCount( int iCount );

    int           GetRowOfValue( double dfValue ) const;
    int           GetRowOfValue( int nValue ) const;
    int           GetColorOfValue( double dfValue, GDALColorEntry *psEntry ) const;

    double        GetRowMin( int iRow ) const;
    double        GetRowMax( int iRow ) const;

    CPLErr        CreateColumn( CPLString osFieldName, 
                                GDALRATFieldType eFieldType, 
                                GDALRATFieldUsage eFieldUsage );
    CPLErr        SetLinearBinning( double dfRow0Min, double dfBinSize );
    int           GetRegularBinning( double *pdfRow0Min, double *pdfBinSize ) const;

    CPLXMLNode   *Serialize() const;
    CPLErr        XMLInit( CPLXMLNode *, const char * );

    CPLErr        InitializeFromColorTable( const GDALColorTable * );
    
    void          DumpReadable( FILE * = NULL );
};
