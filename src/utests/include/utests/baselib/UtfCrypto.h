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
                "MIIDaDCCAlACCQDWlKTpe/DKXjANBgkqhkiG9w0BAQsFADB2MQswCQYDVQQGEwJV\n"
                "UzERMA8GA1UECAwITmV3IFlvcmsxDDAKBgNVBAcMA05ZQzEXMBUGA1UECgwOTXkg\n"
                "Q29tcGFueSBMdGQxLTArBgNVBAMMJE15IENvbXBhbnkgTHRkIFRlc3QgUm9vdCBD\n"
                "ZXJ0aWZpY2F0ZTAeFw0xNzEwMjAwMzUwNDFaFw0yNzA4MjkwMzUwNDFaMHYxCzAJ\n"
                "BgNVBAYTAlVTMREwDwYDVQQIDAhOZXcgWW9yazEMMAoGA1UEBwwDTllDMRcwFQYD\n"
                "VQQKDA5NeSBDb21wYW55IEx0ZDEtMCsGA1UEAwwkTXkgQ29tcGFueSBMdGQgVGVz\n"
                "dCBSb290IENlcnRpZmljYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC\n"
                "AQEAslipXTEu/Tf2YsVx50D1bZRbksk+6jl0LVyJtjuhlcJS+SwWXEH03prTXoYu\n"
                "+ocEzVvhkA+gmwpt0tqwewdbip42JCT7mrALn82VngTLuwX99jKsBvkzM0NlwK6F\n"
                "++d6yb561Aq84jTumL33tCh83TdOPI5x3giAx8fVNh2Mt8dhj9DgLXnwyMQTeluG\n"
                "ABruWna9gUPlYOeZzBrqaio+nSFFTw7shE6lPltTCb/0LRNPhGWhBUqy0JioRseP\n"
                "VAvbOKffW8veV5WnUTFdP8yBh0dWt4rA3bQIjJK4M6SEx2FJ2xc7D2yVED3uzV5G\n"
                "BF3Fm/8syl6lctpGOmSZtAUxbQIDAQABMA0GCSqGSIb3DQEBCwUAA4IBAQBS2x4n\n"
                "xVyLM8gPhBgufheSfcv04XZDSDGnMFXyNYBiRmWjoR11jHhRt6z8Chc+08yuPxM1\n"
                "JnYbczf3MRQVIwLnPaU83C9hNvyx8Haneavb2csHnbVV43Lqxt58hARL1h+loOxs\n"
                "Ef/4eWspyTl6Gne0dwF7nvaASRjGUDJkIsAFPnGD045UH6OLKl4p8zlHblW4B1es\n"
                "j2ebkKwTGJYKIcO7dZHcxtosZsJ8sRHKKlFyIDSKzVDrdRFTnPRR06zTBZ83KfYy\n"
                "p5mWNQQHsQp0wtUTLdYWjMnJBvo1JtAKeYv76ZoBvYcJiJWlnjYCtRq5hOD5Y6ct\n"
                "X7k2o4k7mtEpT5+B\n"
                "-----END CERTIFICATE-----\n";
        }

        static auto getDefaultServerKey() -> const char*
        {
            /*
             * This is from following file: "certs/test-server-key.pem"
             */

            return
                "-----BEGIN PRIVATE KEY-----\n"
                "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDF04YyIhEtTecw\n"
                "51Xy9Z9kyVsdS7Tr19fTsUVCaFJiDL7KfR9loVSQzlvT/M1xZhePkZISZzQ0Ixi+\n"
                "CTLwF+SzSSCD81KjDHqeRcuRetJgz4afnAq9QsgtET7cIeVyBk6sz+CheBJmOyii\n"
                "TDfgZBaPjN1tVqTN4Hz+whavXeXw0kTqrEPuR1ba0kbCjyU3jjFg75iu1nfwEo+K\n"
                "kizgBSbBdWreiZJQf25q5c9yrPqBe/WSx2/QZfYs24Nehm2jCAUZyk0bh4cr4aRJ\n"
                "QoFUzpAo15Vl8kD2msUJnS5Y5a9wc/8u1Q3OJotvJ2AN45g0xGyqSwCjIiXnBFeT\n"
                "/estF/VPAgMBAAECggEAdoXp0+WHRw5yomEnpJ42tmrRVTcDmX3DSIjgBw57tVUP\n"
                "hj/67KgBA5UvfU3sRLG3EgRUcQQ2SbpxW4Ila6XVFvmMKqJA84FJgcQtV+cvXmNX\n"
                "tA8IfCYjyqSXdco1LuDKiE0vt246D9gH210w6RbuUWlDTPvpV5PVL8lXUBBA8Mvr\n"
                "CJus47wq58U0WK2AF6CABcGFnSY9JiJavdZpAlgaO54sIq5YdZORJ8pbk5IjXybR\n"
                "TGTL3IBpuB8RIo4zO9dDvuE1zxBj2FgSTNoo3IsGoIrpInaT2SWGtv04sBrMIbFl\n"
                "B/mkkEzONf2OOmZ0NLPfnZdj4tejyxGq4woYgbLUkQKBgQD45bEHBIHBtKHuIv0d\n"
                "1Y20OVfjQJOTc2bQUNj7f8WeHrINYBYes3jRuf9LDAFDUYZyesOMGjEUcAnpWuha\n"
                "4mHJJqj5r1R9dIpaW8L1ABcV931t1j7aEnq1E/VdRTM+iUR+rUHRzYid+p3CgqLx\n"
                "SMEnDomeDjRGCw8LKcAuVSwaJwKBgQDLeLxglju0Rm21tRE0oSEYwStkkVhmP4gL\n"
                "nDeRMvephgPDTTXWrP4eV2mvynH0/+t+pwbDWHM1q4XrCBnVX/bGpqqOgoPuAj2x\n"
                "wY1BMTDoUlG9VO7koJWM0SN3gyZeOicykpMKszYfWkOoETQ1U49LrOIFJRx192xR\n"
                "WIvpvXmMmQKBgQC6ZYnF76IdJuF+LcXRafTNW4RuNBZQ/sOojmNxNacRW3uMeMEY\n"
                "DOAWcGy4Dy2C9LLzWOzJJ3RKEf3aPLJ2HcONmN5C3wMvUO+r67x9LqwbT1UnxKMd\n"
                "PWmX4nKGfyR5WONq2uXH8Vy2stEisiLE/+9nCIQXUhvjuLRzb7j0+eQlUQKBgDCF\n"
                "6X6rNS/Hv/Aebyz65BawMnX4R3mS2xHRvlqtKezOneUca6N3e96mf/jBMa34viNl\n"
                "F7LMTCVXc0daljaRfRtgsbnsnCPNewMCInqSjZRJ1V5ue84gEaoUUf31U9gSzDg+\n"
                "Rjy+AkE12H6jI6038Std3kTV1dS4HafEkxE581u5AoGAIgoxxG5L1tOq7FOycPgJ\n"
                "EeN2e81PgAVesy0fsemlXxoobXpnftT3uPn5Zpo/yVwUXbmCzRahblgljAMK19TY\n"
                "HNieCuaJCnml/NQz4Atzq+AhOczo3eK1v6wgkWBDDSqu7CahEu0ndarDI6Xfa3Pc\n"
                "RIGnam24+CSu1F4r2xiy2XA=\n"
                "-----END PRIVATE KEY-----\n";
        }

        static auto getDefaultServerCertificate() -> const char*
        {
            /*
             * This is from following file: "certs/test-server-cert.pem"
             */

            return
                "-----BEGIN CERTIFICATE-----\n"
                "MIIDlzCCAn+gAwIBAgIBATANBgkqhkiG9w0BAQsFADB2MQswCQYDVQQGEwJVUzER\n"
                "MA8GA1UECAwITmV3IFlvcmsxDDAKBgNVBAcMA05ZQzEXMBUGA1UECgwOTXkgQ29t\n"
                "cGFueSBMdGQxLTArBgNVBAMMJE15IENvbXBhbnkgTHRkIFRlc3QgUm9vdCBDZXJ0\n"
                "aWZpY2F0ZTAeFw0xNzEwMjAwMzUyMzRaFw0yNzA4MjkwMzUyMzRaMGMxCzAJBgNV\n"
                "BAYTAlVTMREwDwYDVQQIDAhOZXcgWW9yazEMMAoGA1UEBwwDTllDMRcwFQYDVQQK\n"
                "DA5NeSBDb21wYW55IEx0ZDEaMBgGA1UEAwwRKi4qLm15Y29tcGFueS5jb20wggEi\n"
                "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDF04YyIhEtTecw51Xy9Z9kyVsd\n"
                "S7Tr19fTsUVCaFJiDL7KfR9loVSQzlvT/M1xZhePkZISZzQ0Ixi+CTLwF+SzSSCD\n"
                "81KjDHqeRcuRetJgz4afnAq9QsgtET7cIeVyBk6sz+CheBJmOyiiTDfgZBaPjN1t\n"
                "VqTN4Hz+whavXeXw0kTqrEPuR1ba0kbCjyU3jjFg75iu1nfwEo+KkizgBSbBdWre\n"
                "iZJQf25q5c9yrPqBe/WSx2/QZfYs24Nehm2jCAUZyk0bh4cr4aRJQoFUzpAo15Vl\n"
                "8kD2msUJnS5Y5a9wc/8u1Q3OJotvJ2AN45g0xGyqSwCjIiXnBFeT/estF/VPAgMB\n"
                "AAGjQzBBMAkGA1UdEwQCMAAwCwYDVR0PBAQDAgXgMCcGA1UdEQQgMB6CESouKi5t\n"
                "eWNvbXBhbnkuY29tgglsb2NhbGhvc3QwDQYJKoZIhvcNAQELBQADggEBAC2ewdhS\n"
                "ZN7SOABj+X/ISgmOouo9sewyL0X5OGR+mbmTwTDMTqJciDD38TLGv6Y842xK2zz0\n"
                "y6ham7T4gsWyJ+nkvdAj+st92JOj7ToLRiVRQXyq9jKSd4i4k//30zJ1x5jbZ8Gm\n"
                "RbJG38Lh1YC+yMpGekQ+MLoThMR0AqCiFN71bfgobWjRltiAfSX1T6liryAZxTis\n"
                "KtfHPQA83pLPP+IOJK4ntzVZ0QEgoEb9oXKGQyRBMaEYgFEvEuG7yxoBgzDz6hlA\n"
                "zk1qQnfoZVIurDq5N+g2hyeWJY/Uo+IkOoeB+EC4LXFav7bvPI7MBSCseie7IC8E\n"
                "fK6Vl0UXcuz5VIU=\n"
                "-----END CERTIFICATE-----\n";
        }
    };

    typedef UtfCryptoT<> UtfCrypto;

} // test

#endif /* __TEST_UTFCRYPTO_H_ */
