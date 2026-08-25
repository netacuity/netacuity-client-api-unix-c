Copyright 2026 Digital Envoy, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

==============================================================================

README for Apache NetAcuity Server Module

==============================================================================

The Apache NetAcuity Server Module (mod_netacuity) provides 
real-time lookup of a web client's IP address. It hooks into the Apache 
web server and queries a configured set of NetAcuity databases on every 
client request. The response fields from each query are placed into 
Apache environment variables that can be examined by other modules.

The Apache NetAcuity Server Module is a dynamic module which must be compiled 
along with the Apache httpd source code.

==============================================================================

Instructions for compilation, installation, and configuration:


=== Step 1 ===

Install the following library dependencies.

For RedHat distros:
  $ sudo yum install gcc
  $ sudo yum install httpd httpd-devel
  $ sudo yum install libxml2 libxml2-devel

For Debian/Ubuntu distros:
  $ sudo apt-get install gcc
  $ sudo apt-get install apache2 apache2-dev
  $ sudo apt-get install libxml2 libxml2-dev


=== Step 2 ===

Navigate to the NetAcuity Server API unix_c apache_mod source code directory.
  $ cd netacuity-client-api-unix-c/unix_c/apache


=== Step 3 ===

Run "make" to compile the Apache NetAcuity Server module.
  $ sudo make


=== Step 4 ===

Copy the resulting compiled "mod_netacuity.so" shared library
into the apache2 httpd modules directory.

For example:
  $ sudo cp mod_netacuity.so /etc/httpd/modules/


=== Step 5 ===

Make sure the NetAcuity module has the necessary ownership/privileges.

For example:
  $ sudo chown root:root /etc/httpd/modules/mod_netacuity.so
  $ sudo chmod 755 /etc/httpd/modules/mod_netacuity.so


=== Step 6 ===

Edit the "/etc/httpd/conf/httpd.conf" file to add the directive for the 
NetAcuity Server module.

For example:

      ## Load the NetAcuity module as a Dynamic Shared Object (DSO)
      LoadModule netacuity_module modules/mod_netacuity.so
      <IfModule netacuity_module>

          ## The NetAcuity Server IP which will be used for NetAcuity queries
          NetAcuityServer 192.0.2.1

          ## The comma-separated list of NetAcuity database feature-codes to query
          NetAcuityDBS 26,33,35,93

          ## The maximum time to wait for NetAcuity responses in milliseconds
          NetAcuityTimeout 1000

          ## The API ID to use when querying the NetAcuity Server
          NetAcuityID 100

          ## The comma-delimited list of URL extensions that should not trigger 
          ##  queries for performance reasons
          NetAcuityIgnoreSuffixes .gif,.jpg,.png,.css

          ## A mutex lock file exclusive to the NetAcuity module
          NetAcuityLockfile netacuity_module.lock

          ## Set to 'on' if only POST requests should be looked up
          NetAcuityPostOnly off

          ## The HTTP Header to use for obtaining the queryable IP
          ## If set, this takes precedence over NetAcuityUseUserAgentIP
          ## To unset, comment out the line
          NetAcuityUseHttpHeader X-Forwarded-For

          ## Set this to 0 to query the request's connection client_ip
          ## Set this to 1 to query the request's useragent_ip
          NetAcuityUseUserAgentIP 1
      </IfModule>


=== Step 7 ===

Restart the apache2 httpd server.
  $ sudo service httpd restart


The Apache NetAcuity Server Module should now work. 


==============================================================================

-------------------------------
NetAcuity Environment Variables
-------------------------------

The NetAcuity Server Module will provide a set of NetAcuity-specific 
Apache Environment Variables that can be accessed by other modules.

The variable names begin with the prefix "NA_" and will correspond to 
specific response fields within the various NetAcuity databases.
Any hyphens within a NetAcuity field name will be replaced by an underscore.

For example,
    pulse-country        ->  NA_PULSE_COUNTRY
    pulseplus-postal-ext ->  NA_PULSEPLUS_POSTAL_EXT
    vpn-proxy-type       ->  NA_VPN_PROXY_TYPE
    w35-49               ->  NA_W35_49

The details on each of the NetAcuity Database feature-codes,
including a list of their respective fields, is available
in the NetAcuity documentation at:

    https://docs.netacuity.com/

Additionally, the following common NetAcuity variables are also available:
  NA_IP - the IP address that was queried on the NetAcuity Server
  NA_TRANS_ID - the Transaction ID that identifies the NetAcuity query
  NA_ERROR - an error message if an error occurred


==============================================================================

-------------
Example Usage
-------------

Apache Server-Side Includes can be used to display various data 
depending on the value of particular NA Environment Variables. 

For example, with the following conf directives enabled:

    Options +Includes
    AddType text/html .shtml
    AddOutputFilter INCLUDES .shtml

...then the following shtml content could be used:

    <html>
      <head>
        <title>NetAcuity Test Page for <!--#echo var="NA_IP"--></title>
      </head>
      <body>
        Your Location is:
        <!--#echo var="NA_PULSE_CITY"-->,
        <!--#echo var="NA_PULSE_REGION"-->,
        <!--#echo var="NA_PULSE_COUNTRY"-->
      </body>
    </html>

...assuming that NetAcuity Pulse DB (feature 26) is licensed and loaded.


==============================================================================

------------------
Misc Release Notes
------------------

The libxml2 library is required for NetAcuity Server Module 6.5.0 and above.

The names of some older legacy Apache Environment Variables 
have been corrected as of the 6.5.0 version of this module.
The below list indicates those legacy changes:
    NA_EDGE_POSTAL_CODE_CONF         -> NA_EDGE_POSTAL_CONF
    NA_PULSE_PLUS_COUNTRY            -> NA_PULSEPLUS_COUNTRY
    NA_PULSE_PLUS_REGION             -> NA_PULSEPLUS_REGION
    NA_PULSE_PLUS_CITY               -> NA_PULSEPLUS_CITY
    NA_PULSE_PLUS_CONN_SPEED         -> NA_PULSEPLUS_CONN_SPEED
    NA_PULSE_PLUS_CONN_TYPE          -> NA_PULSEPLUS_CONN_TYPE
    NA_PULSE_PLUS_METRO_CODE         -> NA_PULSEPLUS_METRO_CODE
    NA_PULSE_PLUS_LATITUDE           -> NA_PULSEPLUS_LATITUDE
    NA_PULSE_PLUS_LONGITUDE          -> NA_PULSEPLUS_LONGITUDE
    NA_PULSE_PLUS_POSTAL_CODE        -> NA_PULSEPLUS_POSTAL_CODE
    NA_PULSE_PLUS_POSTAL_CODE_EXT    -> NA_PULSEPLUS_POSTAL_EXT
    NA_PULSE_PLUS_COUNTRY_CODE       -> NA_PULSEPLUS_COUNTRY_CODE
    NA_PULSE_PLUS_REGION_CODE        -> NA_PULSEPLUS_REGION_CODE
    NA_PULSE_PLUS_CITY_CODE          -> NA_PULSEPLUS_CITY_CODE
    NA_PULSE_PLUS_CONTINENT_CODE     -> NA_PULSEPLUS_CONTINENT_CODE
    NA_PULSE_PLUS_TWO_LETTER_COUNTRY -> NA_PULSEPLUS_TWO_LETTER_COUNTRY
    NA_PULSE_PLUS_INTERNAL_CODE      -> NA_PULSEPLUS_INTERNAL_CODE
    NA_PULSE_PLUS_AREA_CODES         -> NA_PULSEPLUS_AREA_CODES
    NA_PULSE_PLUS_COUNTRY_CONF       -> NA_PULSEPLUS_COUNTRY_CONF
    NA_PULSE_PLUS_REGION_CONF        -> NA_PULSEPLUS_REGION_CONF
    NA_PULSE_PLUS_CITY_CONF          -> NA_PULSEPLUS_CITY_CONF
    NA_PULSE_PLUS_POSTAL_CONF        -> NA_PULSEPLUS_POSTAL_CONF
    NA_PULSE_PLUS_GMT_OFFSET         -> NA_PULSEPLUS_GMT_OFFSET
    NA_PULSE_PLUS_IN_DST             -> NA_PULSEPLUS_IN_DST
    NA_PULSE_PLUS_TIMEZONE_NAME      -> NA_PULSEPLUS_TIMEZONE_NAME


==============================================================================
