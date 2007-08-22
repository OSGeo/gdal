typedef struct {
	/* Input */
	char *pszURL;
	char **papszOptions;

	/* Output */
	int nStatus;		/* 200 = success, 404 = not found, 0 = no response / error */
	char *pszContentType;
	char *pszError;

	GByte *pabyData;
	size_t nDataLen;
	size_t nDataAlloc;

	//    int nMimePartCount;
	//    CPLMimePart *pasMimePart;

	/* Internal stuff */
	CURL *m_curl_handle;
	struct curl_slist *m_headers;
	char *m_curl_error;
} CPLHTTPRequest;

void CPL_DLL CPLHTTPInitializeRequest(CPLHTTPRequest *psRequest, const char *pszURL = 0, const char **papszOptions = 0);
void CPL_DLL CPLHTTPCleanupRequest(CPLHTTPRequest *psRequest);

void CPL_DLL CPLHTTPFetchMulti(CPLHTTPRequest *pasRequest, int nRequestCount = 1);
