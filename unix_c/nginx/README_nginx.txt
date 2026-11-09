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

README for Nginx NetAcuity Server Module

==============================================================================

The Nginx NetAcuity Server Module (ngx_http_netacuity_module) provides 
real-time lookup of a web client's IP address. It hooks into the Nginx 
web server and queries a NetAcuity Server for a configured set of 
NetAcuity databases on every client request. The response fields from each 
query are placed into Nginx variables that can be examined by other modules.

The Nginx NetAcuity Server Module is a static module which must be compiled 
along with the Nginx source code.

==============================================================================

Instructions for compilation, installation, and configuration:


=== Step 1 ===

The "config" file for the NetAcuity Server Nginx module is located in the same 
directory as the NetAcuity Nginx module source code. Edit this "config" file 
and update the NA_SRC_BASE_DIR variable to point at the location of the 
"unix_c" directory, which was extracted from the NetAcuity API archive.

For example:
  NA_SRC_BASE_DIR="/home/netacuity/netacuity-client-api-unix-c/unix_c"


=== Step 2 ===

Install the following nginx dependencies:
  $ sudo yum install gcc
  $ sudo yum install pcre pcre-devel
  $ sudo yum install zlib zlib-devel

Also install the following NetAcuity Server Module dependency:
  $ sudo yum install libxml2 libxml2-devel


=== Step 3 ===

Download and extract the appropriate version of the Nginx Open-Source 
source code, available at the below link:
  https://nginx.org/en/download.html  

The NetAcuity Server Module is known to work with the nginx-1.28.0 version.


=== Step 4 ===

Navigate to the extracted Nginx souce code directory, and run the "configure" 
script while including the "--add-module" option that points to the directory 
location of the NetAcuity Server Module "config" file.

Make sure that the "-std=gnu99" gcc option is included using the 
"--with-cc-opt" configure option.

Also, the libxml2 library needs to be included and linked during compilation.
This can be specified with the following configure options:
  --with-cc-opt="-I/usr/include/libxml2/" --with-ld-opt="-lxml2"

For example:
  $ sudo ./configure \
    --with-cc-opt="-std=gnu99 -I/usr/include/libxml2/" \
    --with-ld-opt="-lxml2" \
    --with-debug \
    --prefix=/usr/share/nginx \
    --sbin-path=/usr/sbin/nginx \
    --conf-path=/etc/nginx/nginx.conf \
    --error-log-path=/var/log/nginx/error.log \
    --http-log-path=/var/log/nginx/access.log \
    --with-http_realip_module \
    --with-http_sub_module \
    --add-module=/home/netacuity/netacuity-client-api-unix-c/unix_c/nginx

You may refer to the below link for more information on building Nginx:
  http://nginx.org/en/docs/configure.html


=== Step 5 ===

Run "make" to compile the Nginx server along with the NetAcuity Server module. 
  $ sudo make


=== Step 6 ===

Run "make install" to install the Nginx server.
  $ sudo make install


=== Step 7 ===

Edit the "/etc/nginx/nginx.conf" file to add the directive for the NetAcuity
Server module. In the "http" context of the file, add the following:

  netacuity_server_module '<features>' '<server_ip>';

...where <features> is a comma-separated list of feature-codes that will be 
queried on the NetAcuity Server, and <server_ip> is the IP address where the 
NetAcuity Server deployment can be accessed.

For example:
  netacuity_server_module '26,33,35,93' '192.0.2.1';


=== Step 8 ===

Restart the Nginx server.

  $ sudo nginx -s stop
  $ sudo nginx -c /etc/nginx/nginx.conf
  $ sudo nginx -s reload


The Nginx NetAcuity Server Module should now work. 


==============================================================================

Links for Additional Information on Nginx Modules:
  https://nginx.org/en/docs/dev/development_guide.html#Modules

==============================================================================

-------------------
NetAcuity Variables
-------------------

The NetAcuity Server Module will provide a set of NetAcuity-specific 
Apache Environment Variables that can be accessed by other modules.

The variable names begin with the prefix "na_" and will correspond to 
specific response fields within the various NetAcuity databases.
Any hyphens within a NetAcuity field name will be replaced by an underscore.

For example,
    pulse-country        ->  $na_pulse_country
    pulseplus-postal-ext ->  $na_pulseplus_postal_ext
    vpn-proxy-type       ->  $na_vpn_proxy_type
    w35-49               ->  $na_w35_49

The details on each of the NetAcuity Database feature-codes,
including a list of their respective fields, is available
in the NetAcuity documentation at:

    https://docs.digitalelement.com/

Additionally, the following common NetAcuity variables are also available:
  $na_ip - the IP address that was queried on the NetAcuity Server
  $na_trans_id - the Transaction ID that identifies the NetAcuity query
  $na_error - an error message if an error occurred


Note:
It might be necessary to increase the size of the variables_hash in order 
to accommodate the number of variables used by the NetAcuity Server Module.

For example, within the "http" block of nginx.conf:
    variables_hash_max_size 4096;


==============================================================================

Additional Notes:

----

The "real_ip" module is necessary for extracting the correct IP address 
from any potential X-Forwarded-For headers found in HTTP requests. 
The module must be compiled by including the "--with-http_realip_module" 
option when configuring the source code for compilation.

The "real_ip" module must be configured in the "nginx.conf" file. 
Please refer to the below link for information on configuring this module:
  https://nginx.org/en/docs/http/ngx_http_realip_module.html

For example, within the "http" block of nginx.conf:
  set_real_ip_from 192.0.2.0/24;
  set_real_ip_from 192.0.2.1;
  real_ip_header X-Forwarded-For;
  real_ip_recursive on;

The "set_real_ip_from" directive indicates which hosts are trusted 
to provide legitimate X-Forwarded-For headers.

----

The "sub" module is not strictly necessary; however, it is needed to 
correctly handle the examples included in this NetAcuity Client API archive.
The module must be compiled by including the "--with-http_sub_module" 
option when configuring the source code for compilation.


==============================================================================

-------------
Example Usage
-------------

The NetAcuity variables could be used to redirect certain clients 
based on their IP address.
For example:

    if ($na_pulse_country = 'can') {
      return 302 $scheme://www.digitalenvoy.com;
    }

----

The NetAcuity variables could also be used to create replacement strings
that could be contained in HTML files.

For example, with the following conf directives enabled:

    sub_filter_once off;
    sub_filter_types "html";
    sub_filter "#{na_ip}" "$na_ip";
    sub_filter "#{na_pulse_country}" "$na_pulse_country";
    sub_filter "#{na_pulse_region}" "$na_pulse_region";
    sub_filter "#{na_pulse_city}" "$na_pulse_city";

...then the following html content could be used:

    <html>
      <head>
        <title>NetAcuity Test Page for #{na_ip}</title>
      </head>
      <body>
        Your Location is:
        #{na_pulse_city},
        #{na_pulse_region},
        #{na_pulse_country}
      </body>
    </html>

...assuming that NetAcuity Pulse DB (feature 26) is licensed and loaded.

The "netacuity-client-api-unix-c/unix_c/nginx/examples/" 
subdirectory contains an example "netacuity_subfilter.conf" file 
which may be copied into the "/etc/nginx/" directory and referenced via 
an "include" directive in the "/etc/nginx/nginx.conf" file's server block.
Also, for testing purposes, the "ngx-na-mod-test.html" file may be copied 
into the "/usr/share/nginx/html/" directory and then visited by a browser.


----

The NetAcuity variables could also be recorded in the access log.
For example:

    log_format na_log 'Geolocation: $na_pulse_country, '
                      '$na_pulse_region, '
                      '$na_pulse_city, '
                      '$na_pulse_postal_code';
    access_log logs/access.log na_log;

The "netacuity-client-api-unix-c/unix_c/nginx/examples/" 
subdirectory contains an example "netacuity_logging.conf" file 
which may be copied into the "/etc/nginx/" directory and referenced via 
an "include" directive in the "/etc/nginx/nginx.conf" file's http block.
The log entries should appear in "/usr/share/nginx/logs/access.log".


==============================================================================

------------------
Misc Release Notes
------------------

The libxml2 library is required for NetAcuity Server Module 6.5.0 and above.

Several design changes were made to the NetAcuity Server Module in 
version 6.3.0 that are not backward-compatible with previous 
versions of the NetAcuity Server Module 6.2.x and below.


==============================================================================
