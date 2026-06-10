/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement credential provider for accessing NASA Earthdata resources
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_NASA_EARTHDATA_H
#define CPL_NASA_EARTHDATA_H

#ifdef HAVE_CURL

#include <memory>
#include <mutex>
#include <string>

/************************************************************************/
/*                  CPLNasaEarthdataCredentialProvider                  */
/************************************************************************/

// Cf https://earthaccess.readthedocs.io/en/latest/user/authenticate/
// and https://developmentseed.org/obstore/latest/api/auth/earthdata/

/** Credential provider for accessing NASA Earthdata resources with /vsis3/
 */
class CPLNasaEarthdataCredentialProvider
{
  public:
    /** Builds a new credential provider instance.
     *
     * Users generally want to use GetNasaEarthdataCredentialProviderFor() instead
     * that calls Build() with values from the environment.
     *
     * @param osGetCredentialsURL URL that returns a JSON document with S3
     *                            temporary credentials. Required
     *                            e.g. "https://data.asdc.earthdata.nasa.gov/s3credentials"
     * @param osEarthdataHost Hostname for NASA Earthdata authentication.
     *                        If empty, defaults to "urs.earthdata.nasa.gov"
     * @param osEarthdataToken Access token to osGetCredentialsURL obtained from osEarthdataHost
     * @param osEarthdataUsername Login to osEarthdataHost
     * @param osEarthdataPassword Password to osEarthdataHost
     * @param osNetrcFilename Full path to ".netrc" / "_netrc" file
     *
     * @return thread-safe new instance, or nullptr.
     */
    static std::unique_ptr<CPLNasaEarthdataCredentialProvider>
    Build(const std::string &osGetCredentialsURL,
          const std::string &osEarthdataHost = std::string(),
          const std::string &osEarthdataToken = std::string(),
          const std::string &osEarthdataUsername = std::string(),
          const std::string &osEarthdataPassword = std::string(),
          const std::string &osNetrcFilename = std::string());

    /** Returns a (cached) instance corresponding to /vsis3/ osFilename.
     *
     * Takes into account the following path-specific / configuration options:
     *
     * - VSIS3_EARTHDATA_CREDENTIALS_URL: Setting this one is required.
     *   e.g. https://data.asdc.earthdata.nasa.gov/s3credentials .
     * - DEFAULT_EARTHDATA_HOST / EARTHDATA_HOST: defaults to "urs.earthdata.nasa.gov"
     * - EARTHDATA_TOKEN: access token to the URL pointed by VSIS3_EARTHDATA_CREDENTIALS_URL
     * - EARTHDATA_USERNAME + EARTHDATA_PASSWORD: alternate way of getting EARTHDATA_TOKEN
     *
     * @return thread-safe cached instance, or nullptr if VSIS3_EARTHDATA_CREDENTIALS_URL
     *         is not set, or nullptr in case of error
     */
    static std::shared_ptr<CPLNasaEarthdataCredentialProvider>
    Get(const std::string &osFilename, bool *pbErrorOccurred = nullptr);

    /** Returns S3 access key id, or empty string if invalid.
     *
     * This method takes care of refreshing credentials if needed.
     */
    const std::string &GetAccessKeyId()
    {
        RefreshIfNeeded();
        return m_osAccessKeyId;
    }

    /** Returns S3 secret access key, or empty string if invalid.
     *
     * This method takes care of refreshing credentials if needed.
     */
    const std::string &GetSecretAccessKey()
    {
        RefreshIfNeeded();
        return m_osSecretAccessKey;
    }

    /** Returns S3 session token, or empty string if invalid.
     *
     * This method takes care of refreshing credentials if needed.
     */
    const std::string &GetSessionToken()
    {
        RefreshIfNeeded();
        return m_osSessionToken;
    }

    /** Clear credentials cache */
    static void ClearCache();

  private:
    CPLNasaEarthdataCredentialProvider();

    bool RefreshIfNeeded();

    std::string m_osGetCredentialsURL{};
    std::string m_osEarthdataToken{};

    // Output of RefreshIfNeeded()
    std::mutex m_oMutex{};
    std::string m_osAccessKeyId{};
    std::string m_osSecretAccessKey{};
    std::string m_osSessionToken{};
    GIntBig m_nTokenExpirationTimestamp = 0;
};

#endif  // HAVE_CURL

#endif
