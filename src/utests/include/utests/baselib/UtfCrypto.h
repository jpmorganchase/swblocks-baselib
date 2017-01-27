/*
 * This file is part of the swblocks-baselib library.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __TEST_UTFCRYPTO_H_
#define __TEST_UTFCRYPTO_H_

#include <baselib/crypto/CryptoBase.h>

#include <baselib/core/OS.h>
#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>

namespace test
{
    template
    <
        typename E = void
    >
    class UtfCryptoT
    {
        BL_DECLARE_STATIC( UtfCryptoT )

    public:

        static auto getDevRootCA() -> const char*
        {
            /*
             * This is from following file: "certs/test-root-ca.pem"
             */

            return
                "-----BEGIN CERTIFICATE-----\n"
                "MIIDXTCCAkWgAwIBAgIJAO4ial8yzT6YMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\n"
                "BAYTAlVTMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
                "aWRnaXRzIFB0eSBMdGQwHhcNMTcwMTI0MTE1MDM1WhcNMjYxMjAzMTE1MDM1WjBF\n"
                "MQswCQYDVQQGEwJVUzETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50\n"
                "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n"
                "CgKCAQEAv2YuOggLVxomaxXF106iXp3sC5w3cafKjSFKxQx17PygTthAelyrwvAb\n"
                "w9FOCM/ecO1TOntKyBHzpD/z2PZuwkKuzsV9fYk7r8arpzLLOSpIQnWBHgenChTm\n"
                "rmQjkefbSzaa6KqQD9DjYHI0D81NJOTk2ckP32yNFf9wG7eMO8gBb60g7Y34SWKE\n"
                "matr4X/wKAzLS6olNLkK6dR0ZcpYv92SChsWkVOqxcxlIrlW6CP1kFqbJVgTWWiC\n"
                "ybJ7r/RhxjgDqRri7YCyBPAeyMDkeFbO+aq4BIY8AlUxWqnorjEATxeJtWiXYu9B\n"
                "MESommZ4xuIA3PpA2TIPH/Ym9zA0BQIDAQABo1AwTjAdBgNVHQ4EFgQU2OTAt/hw\n"
                "iBPHkjEjW3fHfGNgzlEwHwYDVR0jBBgwFoAU2OTAt/hwiBPHkjEjW3fHfGNgzlEw\n"
                "DAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAjr4cikuvuoYI5LhrQnYV\n"
                "0ZALOZqO6+fWVYAWZj56WN/nrBeQOPZuCdc0i4VLmSWqAvgtPuv1Qh20OqtaUC/S\n"
                "Gbnfk/QWqjGI6O2yHHNUaLXYuLOPL+RJZATxIMXgA79nhNx4Zj4bqFKbNOkPiJQc\n"
                "lq1Z5sPJl+mP15hWijOt2f4unM+A3E/UC++dJl9RRAc8jjyFPgJnUBgVM6AHy92G\n"
                "8gLAFLJf51jmsdZY3wNxsAnnrOjs1k3jp2Y5F/Dm7TfvzMaQth2NMETln0SQEvXd\n"
                "vYmjweSFiBhfIhp7y9ekZwS+72pg8gr1AWqoN0YCQxmTCik2HKRIiRVjG9BA9s90\n"
                "+A==\n"
                "-----END CERTIFICATE-----\n";
        }

        static auto getDefaultServerKey() -> const char*
        {
            /*
             * This is from following file: "certs/test-server-key.pem"
             */

            return
                "-----BEGIN RSA PRIVATE KEY-----\n"
                "MIIEpQIBAAKCAQEAuAlmpLxaocmk3XqDELmAPKDzj4Dn5L4UTRyhvPzLdKgWUNO0\n"
                "qPR1yjVLG8ZEmo1hzTqdXe9Z6SeX8yrBsDa0mT1wbZ0tqICRFNiXRtUloUW7Gt9F\n"
                "/kPUA9dJ+X7G0Oi9NGsEDNeHP7wtuT/ikcWRxTrC0qGhHMnPEc5jrPFI4ywdHd+l\n"
                "75XJorrAcBSjS+FgQ1JMhFzStxFhFfbXZIoDWPvYCRhvv+J7+hzu690F4J63rFUk\n"
                "rpeJ/CqBpdLSe/tqVxQgCZeFx144QmBiudd+TumwYhVJtaQ6gDX1bADmyhzylIjK\n"
                "GMUvUMcqGuyy01mHx2avy2LnwlDyaMlJTrChCQIDAQABAoIBAQCEICO0A5Ih803M\n"
                "wGcWe8dIP9l83nQd8iVLE1I/B1LeZsrWrpdWcWObj9VjlF2ug/CCUiGmze9EB+v8\n"
                "ZWb3Jt9T4Rs1ZxHflqYSTEmXTfGdLh3ddgyfNn0hYNA4Mj90vpMP7gDTY20zYduF\n"
                "0wrL0wcLjVtPs0CjFXP3ebBv+i51AP/K6Lr/l+LxFFewRKenwxQIH6NLpjIWLVhH\n"
                "6fdNY/SYBKSalDObfNBo8m4/gV11OD9ZvW0/5p1IjBvs5XVKvc0wl+cqOV2BlqPw\n"
                "Yt6vDX9JM08f3m/yKHqnGgnNKBdZsBp03b6o9dxEt8N8J8kXE2xAezaSGgmcpGPL\n"
                "uBNXtxHdAoGBAONKylRqS5B25XR5VKiJJoHnSwimM7yWPnJOFtJg42zZkEVwmbNZ\n"
                "VCitQ8KqJ1QBXlVW80G5whhAhukQshF9gnJIAcFIel6ukBNXQcw5IfJTLWd2ZGQ+\n"
                "jNyYnSHAk8TcDAtr3nyzDrcAGGiSeRY4wxU6SpbQq6qbuIeIh3NuNSP/AoGBAM9I\n"
                "AAHFhTuC0L0PhT7RlGLdvciNuDBpRGqoahedYdCfgQGksdM0g3+g594hjlza0Zqs\n"
                "lb/SmxQkoUq9TpN/AKTMzRCxaJY+czfoNEZkLROBzTRMdbo+ruAR2OenLA5625vH\n"
                "3XGoODUE13mAVnamG9W75iUxRhuSPPxzxvEAPRr3AoGABPdlZGLOM+HlMZ5VEzmr\n"
                "9bqwEQhQqRY/VxANv5sOXRqD5ICJWzngdOMUT/SX12YQQZ0cw5rjetQuHnmW6nrr\n"
                "lsOsBiUnR1pZG5MUhPnanAjlPRWBLA+R1GAhTtN+ZxbHzJgWzHK9J5KA9gf9TVcA\n"
                "LD6R0qnMlnXAmnWJQCRwVNECgYEAuZwSjYOOu2x3eGR2f3ryMdm3wOfOoGMS5aMr\n"
                "ZTwDw+mgfpU4uxSSD+5I0qsfrNwwxx119tAjF9V5LND5lLAsJmZR1nnWWntPdyeR\n"
                "79pyVr5rv0IcRYst0u2IWl9i3xB6qDM/gzAMLMXKIT2frx0tXaWk/3bw/W1k+Pa/\n"
                "lT0Oep8CgYEAlWqJNHxCucQjlHcad1zKUnphXzGvRbC5Zz6DZcZxUSt/cidOycj6\n"
                "fvLWfM1mgfU6PPfnWisbkzBubOKJFMVX/yBrp6CEI2YTLE9vtVHDI9AHmlK+UHhE\n"
                "4Y9+ij6ZQAj2atR+9+Mfkkc7TEg8sdPdThA8e1zpsB2BCgXo6cLlcds=\n"
                "-----END RSA PRIVATE KEY-----\n";
        }

        static auto getDefaultServerCertificate() -> const char*
        {
            /*
             * This is from following file: "certs/test-server-cert.pem"
             */

            return
                "-----BEGIN CERTIFICATE-----\n"
                "MIIDEjCCAfoCAQEwDQYJKoZIhvcNAQELBQAwRTELMAkGA1UEBhMCVVMxEzARBgNV\n"
                "BAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5IEx0\n"
                "ZDAeFw0xNzAxMjQxMTU3MzNaFw0yNjEyMDMxMTU3MzNaMFkxCzAJBgNVBAYTAlVT\n"
                "MRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRz\n"
                "IFB0eSBMdGQxEjAQBgNVBAMMCWxvY2FsaG9zdDCCASIwDQYJKoZIhvcNAQEBBQAD\n"
                "ggEPADCCAQoCggEBALgJZqS8WqHJpN16gxC5gDyg84+A5+S+FE0cobz8y3SoFlDT\n"
                "tKj0dco1SxvGRJqNYc06nV3vWeknl/MqwbA2tJk9cG2dLaiAkRTYl0bVJaFFuxrf\n"
                "Rf5D1APXSfl+xtDovTRrBAzXhz+8Lbk/4pHFkcU6wtKhoRzJzxHOY6zxSOMsHR3f\n"
                "pe+VyaK6wHAUo0vhYENSTIRc0rcRYRX212SKA1j72AkYb7/ie/oc7uvdBeCet6xV\n"
                "JK6XifwqgaXS0nv7alcUIAmXhcdeOEJgYrnXfk7psGIVSbWkOoA19WwA5soc8pSI\n"
                "yhjFL1DHKhrsstNZh8dmr8ti58JQ8mjJSU6woQkCAwEAATANBgkqhkiG9w0BAQsF\n"
                "AAOCAQEAf0hsx90/I/L+DzpqOPaWSLLUgpYTcJSg7uCxApxahqcpOxyhea3q9ROn\n"
                "sx7pXa2OpJa6Tl+zA5+qdyFAv+W/yUfBk7a8MkCKtzNVLC4ply4x56NQwu++k4yu\n"
                "+ql/nJND/1OtcJNJ4RyUsaUIgV2+uKhGHPEvkmfGTAPgivGXFTpVZ/4IGvASdZL5\n"
                "Yt4pfpif2HewAg5MQCh2YlNayHAXn3wqUvJ/n5FiyOacdIeUTUh1Vyuu0rkgr1WP\n"
                "WO6A33x9v2c4CB/fA1hkRH4yOJlPXIHRDFZjTvKsIXp56eZNmtbNiYBOUGsUS9a9\n"
                "fzpCbf88mQnRlXNPRYUMHpy4uHgf8g==\n"
                "-----END CERTIFICATE-----\n";
        }
    };

    typedef UtfCryptoT<> UtfCrypto;

} // test

#endif /* __TEST_UTFCRYPTO_H_ */
