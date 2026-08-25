/*
 * Copyright 2026 Digital Envoy, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef NETACUITY_FIELDS_H_
#define NETACUITY_FIELDS_H_


typedef enum netacuity_field_id_t {
    NA_FLD_NULL = 0,

    // common fields
    NA_FLD_FULL_XML_RESPONSE = 100,
    NA_FLD_IP_ADDRESS,
    NA_FLD_TRANS_ID,
    NA_FLD_ERROR,

    // GEO DB = FC 3
    NA_FLD_COUNTRY = 300,
    NA_FLD_REGION,
    NA_FLD_CITY,
    NA_FLD_CONN_SPEED,
    NA_FLD_COUNTRY_CONF,
    NA_FLD_REGION_CONF,
    NA_FLD_CITY_CONF,
    NA_FLD_METRO_CODE,
    NA_FLD_LATITUDE,
    NA_FLD_LONGITUDE,
    NA_FLD_COUNTRY_CODE,
    NA_FLD_REGION_CODE,
    NA_FLD_CITY_CODE,
    NA_FLD_CONTINENT_CODE,
    NA_FLD_TWO_LETTER_COUNTRY,

    // EDGE DB = FC 4
    NA_FLD_EDGE_COUNTRY = 400,
    NA_FLD_EDGE_REGION,
    NA_FLD_EDGE_CITY,
    NA_FLD_EDGE_CONN_SPEED,
    NA_FLD_EDGE_METRO_CODE,
    NA_FLD_EDGE_LATITUDE,
    NA_FLD_EDGE_LONGITUDE,
    NA_FLD_EDGE_POSTAL_CODE,
    NA_FLD_EDGE_COUNTRY_CODE,
    NA_FLD_EDGE_REGION_CODE,
    NA_FLD_EDGE_CITY_CODE,
    NA_FLD_EDGE_CONTINENT_CODE,
    NA_FLD_EDGE_TWO_LETTER_COUNTRY,
    NA_FLD_EDGE_INTERNAL_CODE,
    NA_FLD_EDGE_AREA_CODES,
    NA_FLD_EDGE_COUNTRY_CONF,
    NA_FLD_EDGE_REGION_CONF,
    NA_FLD_EDGE_CITY_CONF,
    NA_FLD_EDGE_POSTAL_CONF,
    NA_FLD_EDGE_GMT_OFFSET,
    NA_FLD_EDGE_IN_DST,
    NA_FLD_EDGE_TIMEZONE_NAME,

    // ZIP DB = FC 7
    NA_FLD_AREA_CODE = 700,
    NA_FLD_GMT_OFFSET,
    NA_FLD_IN_DST,
    NA_FLD_ZIP_CODE_TEXT,
    NA_FLD_ZIP_COUNTRY,
    NA_FLD_TIMEZONE_NAME,

    // ISP DB = FC 8
    NA_FLD_ISP_NAME = 800,

    // HOME_BIZ DB = FC 9
    NA_FLD_HOMEBIZ_TYPE = 900,

    // ASN DB = FC 10
    NA_FLD_ASN = 1000,
    NA_FLD_ASN_NAME,

    // LANGUAGE DB = FC 11
    NA_FLD_PRIMARY_LANG = 1100,
    NA_FLD_SECONDARY_LANG,

    // PROXY DB = FC 12
    NA_FLD_PROXY_TYPE = 1200,
    NA_FLD_PROXY_DESCRIPTION,

    // IS_AN_ISP DB = FC 14
    NA_FLD_IS_AN_ISP = 1400,

    // COMPANY DB = FC 15
    NA_FLD_COMPANY_NAME = 1500,

    // DOMAIN_16 DB = FC 16
    NA_FLD_16_DOMAIN_NAME = 1600,
    NA_FLD_16_DOMAIN_KEY,

    // DEMOGRAPHICS DB = FC 17
    NA_FLD_RANK = 1700,
    NA_FLD_HOUSEHOLDS,
    NA_FLD_WOMEN,
    NA_FLD_W18_34,
    NA_FLD_W35_49,
    NA_FLD_MEN,
    NA_FLD_M18_34,
    NA_FLD_M35_49,
    NA_FLD_TEENS,
    NA_FLD_KIDS,

    // NAICS DB = FC 18
    NA_FLD_NAICS_CODE = 1800,

    // CBSA DB = FC 19
    NA_FLD_CBSA_CODE = 1900,
    NA_FLD_CBSA_TITLE,
    NA_FLD_CBSA_TYPE,
    NA_FLD_CSA_CODE,
    NA_FLD_CSA_TITLE,
    NA_FLD_MD_CODE,
    NA_FLD_MD_TITLE,

    // PULSE_MAX DB = FC 21
    NA_FLD_PULSEMAX_COUNTRY = 2100,
    NA_FLD_PULSEMAX_REGION,
    NA_FLD_PULSEMAX_CITY,
    NA_FLD_PULSEMAX_CONN_SPEED,
    NA_FLD_PULSEMAX_CONN_TYPE,
    NA_FLD_PULSEMAX_METRO_CODE,
    NA_FLD_PULSEMAX_LATITUDE,
    NA_FLD_PULSEMAX_LONGITUDE,
    NA_FLD_PULSEMAX_POSTAL_CODE,
    NA_FLD_PULSEMAX_COUNTRY_CODE,
    NA_FLD_PULSEMAX_REGION_CODE,
    NA_FLD_PULSEMAX_CITY_CODE,
    NA_FLD_PULSEMAX_CONTINENT_CODE,
    NA_FLD_PULSEMAX_TWO_LETTER_COUNTRY,
    NA_FLD_PULSEMAX_INTERNAL_CODE,
    NA_FLD_PULSEMAX_AREA_CODES,
    NA_FLD_PULSEMAX_COUNTRY_CONF,
    NA_FLD_PULSEMAX_REGION_CONF,
    NA_FLD_PULSEMAX_CITY_CONF,
    NA_FLD_PULSEMAX_POSTAL_CONF,
    NA_FLD_PULSEMAX_GMT_OFFSET,
    NA_FLD_PULSEMAX_IN_DST,
    NA_FLD_PULSEMAX_TIMEZONE_NAME,

    // MOBILE DB = FC 24
    NA_FLD_MOBILE_CARRIER = 2400,
    NA_FLD_MCC,
    NA_FLD_MNC,
    NA_FLD_MOBILE_CARRIER_CODE,

    // ORGANIZATION DB = FC 25
    NA_FLD_ORGANIZATION_NAME = 2500,

    // PULSE DB = FC 26
    NA_FLD_PULSE_COUNTRY = 2600,
    NA_FLD_PULSE_REGION,
    NA_FLD_PULSE_CITY,
    NA_FLD_PULSE_CONN_SPEED,
    NA_FLD_PULSE_CONN_TYPE,
    NA_FLD_PULSE_METRO_CODE,
    NA_FLD_PULSE_LATITUDE,
    NA_FLD_PULSE_LONGITUDE,
    NA_FLD_PULSE_POSTAL_CODE,
    NA_FLD_PULSE_COUNTRY_CODE,
    NA_FLD_PULSE_REGION_CODE,
    NA_FLD_PULSE_CITY_CODE,
    NA_FLD_PULSE_CONTINENT_CODE,
    NA_FLD_PULSE_TWO_LETTER_COUNTRY,
    NA_FLD_PULSE_INTERNAL_CODE,
    NA_FLD_PULSE_AREA_CODES,
    NA_FLD_PULSE_COUNTRY_CONF,
    NA_FLD_PULSE_REGION_CONF,
    NA_FLD_PULSE_CITY_CONF,
    NA_FLD_PULSE_POSTAL_CONF,
    NA_FLD_PULSE_GMT_OFFSET,
    NA_FLD_PULSE_IN_DST,
    NA_FLD_PULSE_TIMEZONE_NAME,

    // PULSE_PLUS DB = FC 30
    NA_FLD_PULSEPLUS_COUNTRY = 3000,
    NA_FLD_PULSEPLUS_REGION,
    NA_FLD_PULSEPLUS_CITY,
    NA_FLD_PULSEPLUS_CONN_SPEED,
    NA_FLD_PULSEPLUS_CONN_TYPE,
    NA_FLD_PULSEPLUS_METRO_CODE,
    NA_FLD_PULSEPLUS_LATITUDE,
    NA_FLD_PULSEPLUS_LONGITUDE,
    NA_FLD_PULSEPLUS_POSTAL_CODE,
    NA_FLD_PULSEPLUS_POSTAL_EXT,
    NA_FLD_PULSEPLUS_COUNTRY_CODE,
    NA_FLD_PULSEPLUS_REGION_CODE,
    NA_FLD_PULSEPLUS_CITY_CODE,
    NA_FLD_PULSEPLUS_CONTINENT_CODE,
    NA_FLD_PULSEPLUS_TWO_LETTER_COUNTRY,
    NA_FLD_PULSEPLUS_INTERNAL_CODE,
    NA_FLD_PULSEPLUS_AREA_CODES,
    NA_FLD_PULSEPLUS_COUNTRY_CONF,
    NA_FLD_PULSEPLUS_REGION_CONF,
    NA_FLD_PULSEPLUS_CITY_CONF,
    NA_FLD_PULSEPLUS_POSTAL_CONF,
    NA_FLD_PULSEPLUS_GMT_OFFSET,
    NA_FLD_PULSEPLUS_IN_DST,
    NA_FLD_PULSEPLUS_TIMEZONE_NAME,

    // VPN_PROXY DB = FC 33
    NA_FLD_VPN_PROXY_TYPE = 3300,
    NA_FLD_VPN_PROXY_DESCRIPTION,
    NA_FLD_VPN_PROXY_NODE,
    NA_FLD_VPN_PROXY_NAMES,

    // IPC DB = FC 35
    NA_FLD_IPC_OBS_COUNT = 3500,
    NA_FLD_IPC_DISTINCT_DEVICE_IDS,
    NA_FLD_IPC_DISTINCT_COUNTRIES,
    NA_FLD_IPC_DISTINCT_REGIONS,
    NA_FLD_IPC_DISTINCT_CITIES,
    NA_FLD_IPC_DISTINCT_POSTALS,
    NA_FLD_IPC_AVG_DIST,
    NA_FLD_IPC_MAX_DIST,
    NA_FLD_IPC_DIST_STD_DEV,
    NA_FLD_IPC_LAST_SEEN,
    NA_FLD_IPC_RADIUS_WEEKS_STABLE,
    NA_FLD_IPC_SCORE,
    NA_FLD_IPC_COUNTRY_WEEKS_STABLE,
    NA_FLD_IPC_REGION_WEEKS_STABLE,
    NA_FLD_IPC_CITY_WEEKS_STABLE,
    NA_FLD_IPC_POSTAL_WEEKS_STABLE,

    // NODIFY DB = FC 36
    NA_FLD_HOST_INFO = 3600,

    // OBSERVED_COUNTRIES DB = FC 37
    NA_FLD_OBSERVED_COUNTRIES_ISO3_1WK = 3700,
    NA_FLD_OBSERVED_COUNTRIES_ISO2_1WK,
    NA_FLD_OBSERVED_COUNTRIES_ISO3_5WK,
    NA_FLD_OBSERVED_COUNTRIES_ISO2_5WK,

    // ALTERNATE_AREA SA1 DB = FC 40
    NA_FLD_AA_SA1_COUNTRY = 4000,
    NA_FLD_AA_SA1_VALUE,

    // ALTERNATE_AREA_MAX SA1 DB = FC 41
    NA_FLD_AAMAX_SA1_COUNTRY = 4100,
    NA_FLD_AAMAX_SA1_VALUE,

    // ALTERNATE_AREA IRIS DB = FC 42
    NA_FLD_AA_IRIS_COUNTRY = 4200,
    NA_FLD_AA_IRIS_VALUE,

    // ALTERNATE_AREA PLZ8 DB = FC 43
    NA_FLD_AA_PLZ8_COUNTRY = 4300,
    NA_FLD_AA_PLZ8_VALUE,

    // ALTERNATE_AREA ZIP4 DB = FC 44
    NA_FLD_AA_ZIP4_COUNTRY = 4400,
    NA_FLD_AA_ZIP4_VALUE,

    // ALTERNATE_AREA_MAX ZIP4 DB = FC 45
    NA_FLD_AAMAX_ZIP4_COUNTRY = 4500,
    NA_FLD_AAMAX_ZIP4_VALUE,

    // ALTERNATE_AREA_MAX IRIS DB = FC 46
    NA_FLD_AAMAX_IRIS_COUNTRY = 4600,
    NA_FLD_AAMAX_IRIS_VALUE,

    // ALTERNATE_AREA_MAX PLZ8 DB = FC 47
    NA_FLD_AAMAX_PLZ8_COUNTRY = 4700,
    NA_FLD_AAMAX_PLZ8_VALUE,

    NA_FLD_MAXIMUM_ID   //this should always be the last item in this enum
} netacuity_field_id;


typedef enum netacuity_field_type_t {
  NA_FLD_TYPE_NULL = 0,
  NA_FLD_TYPE_INT,
  NA_FLD_TYPE_FLOAT,
  NA_FLD_TYPE_STRING
} netacuity_field_type;


typedef struct {
    unsigned int feature_code;
    netacuity_field_type field_type;
    netacuity_field_id field_id;
    char *field_name;
} netacuity_field;



static netacuity_field netacuity_field_dictionary[] = {
    // common fields
    {  1, NA_FLD_TYPE_STRING,   NA_FLD_IP_ADDRESS,    "ip" },
    {  1, NA_FLD_TYPE_STRING,   NA_FLD_TRANS_ID,      "trans-id" },
    {  1, NA_FLD_TYPE_STRING,   NA_FLD_ERROR,         "error" },

    // GEO DB
    {  3, NA_FLD_TYPE_STRING,   NA_FLD_COUNTRY,              "country" },
    {  3, NA_FLD_TYPE_STRING,   NA_FLD_REGION,               "region" },
    {  3, NA_FLD_TYPE_STRING,   NA_FLD_CITY,                 "city" },
    {  3, NA_FLD_TYPE_STRING,   NA_FLD_CONN_SPEED,           "conn-speed" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_COUNTRY_CONF,         "country-conf" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_REGION_CONF,          "region-conf" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_CITY_CONF,            "city-conf" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_METRO_CODE,           "metro-code" },
    {  3, NA_FLD_TYPE_FLOAT,    NA_FLD_LATITUDE,             "latitude" },
    {  3, NA_FLD_TYPE_FLOAT,    NA_FLD_LONGITUDE,            "longitude" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_COUNTRY_CODE,         "country-code" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_REGION_CODE,          "region-code" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_CITY_CODE,            "city-code" },
    {  3, NA_FLD_TYPE_INT,      NA_FLD_CONTINENT_CODE,       "continent-code" },
    {  3, NA_FLD_TYPE_STRING,   NA_FLD_TWO_LETTER_COUNTRY,   "two-letter-country" },

    // EDGE DB
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_COUNTRY,             "edge-country" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_REGION,              "edge-region" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_CITY,                "edge-city" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_CONN_SPEED,          "edge-conn-speed" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_METRO_CODE,          "edge-metro-code" },
    {  4, NA_FLD_TYPE_FLOAT,    NA_FLD_EDGE_LATITUDE,            "edge-latitude" },
    {  4, NA_FLD_TYPE_FLOAT,    NA_FLD_EDGE_LONGITUDE,           "edge-longitude" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_POSTAL_CODE,         "edge-postal-code" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_COUNTRY_CODE,        "edge-country-code" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_REGION_CODE,         "edge-region-code" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_CITY_CODE,           "edge-city-code" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_CONTINENT_CODE,      "edge-continent-code" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_TWO_LETTER_COUNTRY,  "edge-two-letter-country" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_INTERNAL_CODE,       "edge-internal-code" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_AREA_CODES,          "edge-area-codes" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_COUNTRY_CONF,        "edge-country-conf" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_REGION_CONF,         "edge-region-conf" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_CITY_CONF,           "edge-city-conf" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_POSTAL_CONF,         "edge-postal-conf" },
    {  4, NA_FLD_TYPE_INT,      NA_FLD_EDGE_GMT_OFFSET,          "edge-gmt-offset" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_IN_DST,              "edge-in-dst" },
    {  4, NA_FLD_TYPE_STRING,   NA_FLD_EDGE_TIMEZONE_NAME,       "edge-timezone-name" },

    // ZIP DB
    {  7, NA_FLD_TYPE_INT,      NA_FLD_AREA_CODE,            "area-code" },
    {  7, NA_FLD_TYPE_INT,      NA_FLD_GMT_OFFSET,           "gmt-offset" },
    {  7, NA_FLD_TYPE_STRING,   NA_FLD_IN_DST,               "in-dst" },
    {  7, NA_FLD_TYPE_STRING,   NA_FLD_ZIP_CODE_TEXT,        "zip-code-text" },
    {  7, NA_FLD_TYPE_STRING,   NA_FLD_ZIP_COUNTRY,          "zip-country" },
    {  7, NA_FLD_TYPE_STRING,   NA_FLD_TIMEZONE_NAME,        "timezone-name" },

    // ISP DB
    {  8, NA_FLD_TYPE_STRING,   NA_FLD_ISP_NAME,             "isp-name" },

    // HOME_BIZ DB
    {  9, NA_FLD_TYPE_STRING,   NA_FLD_HOMEBIZ_TYPE,         "homebiz-type" },

    // ASN DB
    { 10, NA_FLD_TYPE_INT,      NA_FLD_ASN,                  "asn" },
    { 10, NA_FLD_TYPE_STRING,   NA_FLD_ASN_NAME,             "asn-name" },

    // LANGUAGE DB
    { 11, NA_FLD_TYPE_STRING,   NA_FLD_PRIMARY_LANG,         "primary-lang" },
    { 11, NA_FLD_TYPE_STRING,   NA_FLD_SECONDARY_LANG,       "secondary-lang" },

    // PROXY DB
    { 12, NA_FLD_TYPE_STRING,   NA_FLD_PROXY_TYPE,           "proxy-type" },
    { 12, NA_FLD_TYPE_STRING,   NA_FLD_PROXY_DESCRIPTION,    "proxy-description" },

    // IS_AN_ISP DB
    { 14, NA_FLD_TYPE_STRING,   NA_FLD_IS_AN_ISP,            "is-an-isp" },

    // COMPANY DB
    { 15, NA_FLD_TYPE_STRING,   NA_FLD_COMPANY_NAME,         "company-name" },

    // DOMAIN_16 DB
    { 16, NA_FLD_TYPE_STRING,   NA_FLD_16_DOMAIN_NAME,       "domain-name" },
    { 16, NA_FLD_TYPE_INT,      NA_FLD_16_DOMAIN_KEY,        "domain-key" },

    // DEMOGRAPHICS DB
    { 17, NA_FLD_TYPE_INT,      NA_FLD_RANK,                 "rank" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_HOUSEHOLDS,           "households" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_WOMEN,                "women" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_W18_34,               "w18-34" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_W35_49,               "w35-49" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_MEN,                  "men" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_M18_34,               "m18-34" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_M35_49,               "m35-49" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_TEENS,                "teens" },
    { 17, NA_FLD_TYPE_INT,      NA_FLD_KIDS,                 "kids" },

    // NAICS DB
    { 18, NA_FLD_TYPE_INT,      NA_FLD_NAICS_CODE,           "naics-code" },

    // CBSA DB
    { 19, NA_FLD_TYPE_INT,      NA_FLD_CBSA_CODE,            "cbsa-code" },
    { 19, NA_FLD_TYPE_STRING,   NA_FLD_CBSA_TITLE,           "cbsa-title" },
    { 19, NA_FLD_TYPE_STRING,   NA_FLD_CBSA_TYPE,            "cbsa-type" },
    { 19, NA_FLD_TYPE_INT,      NA_FLD_CSA_CODE,             "csa-code" },
    { 19, NA_FLD_TYPE_STRING,   NA_FLD_CSA_TITLE,            "csa-title" },
    { 19, NA_FLD_TYPE_INT,      NA_FLD_MD_CODE,              "md-code" },
    { 19, NA_FLD_TYPE_STRING,   NA_FLD_MD_TITLE,             "md-title" },

    // PULSE_MAX DB
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_COUNTRY,            "pulsemax-country" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_REGION,             "pulsemax-region" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_CITY,               "pulsemax-city" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_CONN_SPEED,         "pulsemax-conn-speed" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_CONN_TYPE,          "pulsemax-conn-type" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_METRO_CODE,         "pulsemax-metro-code" },
    { 21, NA_FLD_TYPE_FLOAT,    NA_FLD_PULSEMAX_LATITUDE,           "pulsemax-latitude" },
    { 21, NA_FLD_TYPE_FLOAT,    NA_FLD_PULSEMAX_LONGITUDE,          "pulsemax-longitude" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_POSTAL_CODE,        "pulsemax-postal-code" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_COUNTRY_CODE,       "pulsemax-country-code" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_REGION_CODE,        "pulsemax-region-code" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_CITY_CODE,          "pulsemax-city-code" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_CONTINENT_CODE,     "pulsemax-continent-code" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_TWO_LETTER_COUNTRY, "pulsemax-two-letter-country" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_INTERNAL_CODE,      "pulsemax-internal-code" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_AREA_CODES,         "pulsemax-area-codes" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_COUNTRY_CONF,       "pulsemax-country-conf" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_REGION_CONF,        "pulsemax-region-conf" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_CITY_CONF,          "pulsemax-city-conf" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_POSTAL_CONF,        "pulsemax-postal-conf" },
    { 21, NA_FLD_TYPE_INT,      NA_FLD_PULSEMAX_GMT_OFFSET,         "pulsemax-gmt-offset" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_IN_DST,             "pulsemax-in-dst" },
    { 21, NA_FLD_TYPE_STRING,   NA_FLD_PULSEMAX_TIMEZONE_NAME,      "pulsemax-timezone-name" },

    // MOBILE DB
    { 24, NA_FLD_TYPE_STRING,   NA_FLD_MOBILE_CARRIER,       "mobile-carrier" },
    { 24, NA_FLD_TYPE_STRING,   NA_FLD_MCC,                  "mcc" },
    { 24, NA_FLD_TYPE_STRING,   NA_FLD_MNC,                  "mnc" },
    { 24, NA_FLD_TYPE_STRING,   NA_FLD_MOBILE_CARRIER_CODE,  "mobile-carrier-code" },

    // ORGANIZATION DB
    { 25, NA_FLD_TYPE_STRING,   NA_FLD_ORGANIZATION_NAME,    "organization-name" },

    // PULSE DB
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_COUNTRY,            "pulse-country" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_REGION,             "pulse-region" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_CITY,               "pulse-city" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_CONN_SPEED,         "pulse-conn-speed" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_CONN_TYPE,          "pulse-conn-type" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_METRO_CODE,         "pulse-metro-code" },
    { 26, NA_FLD_TYPE_FLOAT,    NA_FLD_PULSE_LATITUDE,           "pulse-latitude" },
    { 26, NA_FLD_TYPE_FLOAT,    NA_FLD_PULSE_LONGITUDE,          "pulse-longitude" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_POSTAL_CODE,        "pulse-postal-code" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_COUNTRY_CODE,       "pulse-country-code" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_REGION_CODE,        "pulse-region-code" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_CITY_CODE,          "pulse-city-code" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_CONTINENT_CODE,     "pulse-continent-code" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_TWO_LETTER_COUNTRY, "pulse-two-letter-country" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_INTERNAL_CODE,      "pulse-internal-code" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_AREA_CODES,         "pulse-area-codes" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_COUNTRY_CONF,       "pulse-country-conf" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_REGION_CONF,        "pulse-region-conf" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_CITY_CONF,          "pulse-city-conf" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_POSTAL_CONF,        "pulse-postal-conf" },
    { 26, NA_FLD_TYPE_INT,      NA_FLD_PULSE_GMT_OFFSET,         "pulse-gmt-offset" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_IN_DST,             "pulse-in-dst" },
    { 26, NA_FLD_TYPE_STRING,   NA_FLD_PULSE_TIMEZONE_NAME,      "pulse-timezone-name" },

    // PULSE_PLUS DB
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_COUNTRY,             "pulseplus-country" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_REGION,              "pulseplus-region" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_CITY,                "pulseplus-city" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_CONN_SPEED,          "pulseplus-conn-speed" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_CONN_TYPE,           "pulseplus-conn-type" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_METRO_CODE,          "pulseplus-metro-code" },
    { 30, NA_FLD_TYPE_FLOAT,    NA_FLD_PULSEPLUS_LATITUDE,            "pulseplus-latitude" },
    { 30, NA_FLD_TYPE_FLOAT,    NA_FLD_PULSEPLUS_LONGITUDE,           "pulseplus-longitude" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_POSTAL_CODE,         "pulseplus-postal-code" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_POSTAL_EXT,          "pulseplus-postal-ext" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_COUNTRY_CODE,        "pulseplus-country-code" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_REGION_CODE,         "pulseplus-region-code" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_CITY_CODE,           "pulseplus-city-code" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_CONTINENT_CODE,      "pulseplus-continent-code" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_TWO_LETTER_COUNTRY,  "pulseplus-two-letter-country" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_INTERNAL_CODE,       "pulseplus-internal-code" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_AREA_CODES,          "pulseplus-area-codes" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_COUNTRY_CONF,        "pulseplus-country-conf" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_REGION_CONF,         "pulseplus-region-conf" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_CITY_CONF,           "pulseplus-city-conf" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_POSTAL_CONF,         "pulseplus-postal-conf" },
    { 30, NA_FLD_TYPE_INT,      NA_FLD_PULSEPLUS_GMT_OFFSET,          "pulseplus-gmt-offset" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_IN_DST,              "pulseplus-in-dst" },
    { 30, NA_FLD_TYPE_STRING,   NA_FLD_PULSEPLUS_TIMEZONE_NAME,       "pulseplus-timezone-name" },

    // VPN_PROXY DB
    { 33, NA_FLD_TYPE_STRING,   NA_FLD_VPN_PROXY_TYPE,         "vpn-proxy-type" },
    { 33, NA_FLD_TYPE_STRING,   NA_FLD_VPN_PROXY_DESCRIPTION,  "vpn-proxy-description" },
    { 33, NA_FLD_TYPE_STRING,   NA_FLD_VPN_PROXY_NODE,         "vpn-proxy-node" },
    { 33, NA_FLD_TYPE_STRING,   NA_FLD_VPN_PROXY_NAMES,        "vpn-proxy-names" },

    // IPC DB
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_OBS_COUNT,            "ipc-obs-count" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_DISTINCT_DEVICE_IDS,  "ipc-distinct-device-ids" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_DISTINCT_COUNTRIES,   "ipc-distinct-countries" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_DISTINCT_REGIONS,     "ipc-distinct-regions" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_DISTINCT_CITIES,      "ipc-distinct-cities" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_DISTINCT_POSTALS,     "ipc-distinct-postals" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_AVG_DIST,             "ipc-avg-dist" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_MAX_DIST,             "ipc-max-dist" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_DIST_STD_DEV,         "ipc-dist-std-dev" },
    { 35, NA_FLD_TYPE_STRING,   NA_FLD_IPC_LAST_SEEN,            "ipc-last-seen" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_RADIUS_WEEKS_STABLE,  "ipc-radius-weeks-stable" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_SCORE,                "ipc-score" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_COUNTRY_WEEKS_STABLE, "ipc-country-weeks-stable" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_REGION_WEEKS_STABLE,  "ipc-region-weeks-stable" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_CITY_WEEKS_STABLE,    "ipc-city-weeks-stable" },
    { 35, NA_FLD_TYPE_INT,      NA_FLD_IPC_POSTAL_WEEKS_STABLE,  "ipc-postal-weeks-stable" },

    // NODIFY DB
    { 36, NA_FLD_TYPE_STRING,   NA_FLD_HOST_INFO,               "host-info" },

    // OBSERVED_COUNTRIES DB
    { 37, NA_FLD_TYPE_STRING,   NA_FLD_OBSERVED_COUNTRIES_ISO3_1WK,  "oc-iso3-1wk" },
    { 37, NA_FLD_TYPE_STRING,   NA_FLD_OBSERVED_COUNTRIES_ISO2_1WK,  "oc-iso2-1wk" },
    { 37, NA_FLD_TYPE_STRING,   NA_FLD_OBSERVED_COUNTRIES_ISO3_5WK,  "oc-iso3-5wk" },
    { 37, NA_FLD_TYPE_STRING,   NA_FLD_OBSERVED_COUNTRIES_ISO2_5WK,  "oc-iso2-5wk" },

    // ALTERNATE_AREA SA1 DB
    { 40, NA_FLD_TYPE_STRING,   NA_FLD_AA_SA1_COUNTRY,      "aa-sa1-country" },
    { 40, NA_FLD_TYPE_STRING,   NA_FLD_AA_SA1_VALUE,        "aa-sa1-value" },

    // ALTERNATE_AREA_MAX SA1 DB
    { 41, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_SA1_COUNTRY,   "aamax-sa1-country" },
    { 41, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_SA1_VALUE,     "aamax-sa1-value" },

    // ALTERNATE_AREA IRIS DB
    { 42, NA_FLD_TYPE_STRING,   NA_FLD_AA_IRIS_COUNTRY,     "aa-iris-country" },
    { 42, NA_FLD_TYPE_STRING,   NA_FLD_AA_IRIS_VALUE,       "aa-iris-value" },

    // ALTERNATE_AREA PLZ8 DB
    { 43, NA_FLD_TYPE_STRING,   NA_FLD_AA_PLZ8_COUNTRY,     "aa-plz8-country" },
    { 43, NA_FLD_TYPE_STRING,   NA_FLD_AA_PLZ8_VALUE,       "aa-plz8-value" },

    // ALTERNATE_AREA ZIP4 DB
    { 44, NA_FLD_TYPE_STRING,   NA_FLD_AA_ZIP4_COUNTRY,     "aa-zip4-country" },
    { 44, NA_FLD_TYPE_STRING,   NA_FLD_AA_ZIP4_VALUE,       "aa-zip4-value" },

    // ALTERNATE_AREA_MAX ZIP4 DB
    { 45, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_ZIP4_COUNTRY,  "aamax-zip4-country" },
    { 45, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_ZIP4_VALUE,    "aamax-zip4-value" },

    // ALTERNATE_AREA_MAX IRIS DB
    { 46, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_IRIS_COUNTRY,  "aamax-iris-country" },
    { 46, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_IRIS_VALUE,    "aamax-iris-value" },

    // ALTERNATE_AREA_MAX PLZ8 DB
    { 47, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_PLZ8_COUNTRY,  "aamax-plz8-country" },
    { 47, NA_FLD_TYPE_STRING,   NA_FLD_AAMAX_PLZ8_VALUE,    "aamax-plz8-value" },

    {  0, NA_FLD_TYPE_NULL,     NA_FLD_NULL,    "" }  //this should always be the last item in this array
};


#endif // NETACUITY_FIELDS_H_

