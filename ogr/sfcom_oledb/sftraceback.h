#include "stdafx.h"

IDBProperties *SFGetDataSourceProperties(ICommand *pICommand);
IDBProperties *SFGetDataSourceProperties(IRowsetInfo* pIRInfo);
IDBProperties *SFGetDataSourceProperties(IGetDataSource *pIGetDataSource);
IDBProperties *SFGetDataSourceProperties(IUnknown *pIUnknown);
