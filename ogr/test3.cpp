
#define INITGUID
#define DBINITCONSTANTS

#include <oledb.h>
#include "oledb_sup.h"
#include "oledb_sf.h"

#include "ogr_geometry.h"

#include "geometryidl.h"
#include "spatialreferenceidl.h"

// Get various classid.
#include "msdaguid.h"
//#include "MSjetoledb.h"
#include "sfclsid.h"
#include "sfiiddef.h"
#include "oledbgis.h"
#include <atlbase.h>

char      *_gpszFile;
int        _gnLine;
#define CHECK(a) if (FAILED(a)) {_gpszFile = __FILE__; _gnLine = __LINE__;goto error;}

void    DumpRowset(char *pszDataSource, char *pszCommand)
{
        // Create Data Source
        IDBCreateSession    *pIDBCreateSession = NULL;
    IDBInitialize*      pIDBInit = NULL;
    IDBProperties*      pIDBProperties = NULL;
    DBPROPSET           dbPropSet[1];
    DBPROP              dbProp[1];
        
    HRESULT     hr;
        
        USES_CONVERSION;
    
        VariantInit(&(dbProp[0].vValue));
        
    // Create an instance of the SampProv sample data provider
    CHECK(hr = CoCreateInstance( CLSID_SFProvider, NULL, CLSCTX_INPROC_SERVER, 
                        IID_IDBInitialize, (void **)&pIDBInit )); 
    
    if (strlen(pszDataSource) > 0)
        {
        LPWSTR            pwszDataSource = NULL;
        // Initialize this provider with the path to the customer.csv file
        dbPropSet[0].rgProperties       = &dbProp[0];
        dbPropSet[0].cProperties        = 1;
        dbPropSet[0].guidPropertySet    = DBPROPSET_DBINIT;
                
        dbProp[0].dwPropertyID          = DBPROP_INIT_DATASOURCE;
        dbProp[0].dwOptions             = DBPROPOPTIONS_REQUIRED;
        dbProp[0].colid                 = DB_NULLID;

        pwszDataSource = A2OLE(pszDataSource);
                
        V_VT(&(dbProp[0].vValue))       = VT_BSTR;
        V_BSTR(&(dbProp[0].vValue))     = SysAllocString( pwszDataSource );
        CoTaskMemFree( pwszDataSource );
                
        if ( NULL == V_BSTR(&(dbProp[0].vValue)) )
        {
            goto error;
        }
                
        CHECK(hr = pIDBInit->QueryInterface( IID_IDBProperties, 
                          (void**)&pIDBProperties));
        
        CHECK(hr = pIDBProperties->SetProperties( 1, &dbPropSet[0]));
        
    }
    
        CHECK(hr = pIDBInit->Initialize());
    
    hr = pIDBInit->QueryInterface( IID_IDBCreateSession, 
                                   (void**)&pIDBCreateSession);
    pIDBInit->Release();
    pIDBInit = NULL;
        
        CHECK(hr);
        
        IDBCreateCommand *pIDBCreateCommand;

        CHECK(hr = pIDBCreateSession->CreateSession(NULL,IID_IDBCreateCommand,
                        (IUnknown **) &pIDBCreateCommand));

        ICommand *pICommand;

        CHECK(hr = pIDBCreateCommand->CreateCommand(NULL,IID_ICommand,
                                                                                                (IUnknown **) &pICommand));

        pIDBCreateCommand->Release();
        pIDBCreateCommand = NULL;

        ICommandText *pICommandText;
        CHECK(hr = pICommand->QueryInterface(IID_ICommandText, (void **) &pICommandText));
        


        CHECK(hr = pICommandText->SetCommandText(DBGUID_DEFAULT,A2OLE("Shaspe")));


        IRowset *pIRowset;
        DBPARAMS pParams;

        pParams.cParamSets = 1;
        pParams.hAccessor = NULL;
        pParams.pData = "This is it";


        if (FAILED(hr = pICommand->Execute(NULL, 
                IID_IRowset,
                &pParams,
                NULL,
                (IUnknown **) &pIRowset)))
        {
                IErrorInfo *pIE;


                ISupportErrorInfo *pISEI;

                if (!FAILED(pICommand->QueryInterface(IID_ISupportErrorInfo, (void **) & pISEI)))
                {
        
                        if (S_OK == pISEI->InterfaceSupportsErrorInfo(IID_ICommand))
                        {
                        
                                if (!FAILED(GetErrorInfo(0,&pIE)))
                                {
                                        BSTR pbstr;
                                        
                                        fprintf(stderr,"There is error information.\n");
                                        
                                        pIE->GetDescription(&pbstr);
                                        if (pbstr)
                                                fprintf(stderr,OLE2A(pbstr));
                                        
                                        
                                        IErrorRecords *pIER;
                                        pIE->QueryInterface(IID_IErrorRecords, (void **) &pIER);
                                        
                                        ULONG nErrorCount;
                                        pIER->GetRecordCount(&nErrorCount);
                                        int i;
                                        for (i=0; i < nErrorCount; i++)
                                        {
                                                IErrorInfo *pErrorInfo;
                                                
                                                ERRORINFO ErrorInfo;
                                                
                                                pIER->GetBasicErrorInfo(i,&ErrorInfo);
                                                
                                                
                                                pIER->GetErrorInfo(i,0, &pErrorInfo);
                                                
                                                
                                                pErrorInfo->GetSource(&pbstr);
                                                if(pbstr)
                                                        fprintf(stderr,OLE2A(pbstr));
                                                
                                                pErrorInfo->GetDescription(&pbstr);
                                                if(pbstr)
                                                        fprintf(stderr,OLE2A(pbstr));
                                                
                                        }
                                }
                        }
                }
                
        }


        fprintf(stderr,"Success\n");


        return;

error:
        fprintf(stderr,"Error has occurred %n %s %n.\n",hr,_gpszFile,_gnLine);
        return;
}



int main(int argc, char *argv[])
{
        
        char pszCommand[1024];

        char *pszDataSource;


        int  i;

        if (argc < 3)
        {
                fprintf(stderr,"test3 <datasource> <command>");
                return 1;
        }

        pszDataSource = argv[1];
        pszCommand[0] = 0;
        for (i=2; i < argc; i++)
        {
                strcat(pszCommand,argv[i]);
                strcat(pszCommand," ");
        }

                OleInitialize(0);
        DumpRowset(pszDataSource,pszCommand);

        return 0;
}
