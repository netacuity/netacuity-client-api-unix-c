# Copyright 2026 Digital Envoy, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#############################################################
##
## This is the Makefile for the mod_netacuity Apache module.
## APR_DIR/HTTPD_DIR are auto-detected below by checking which of the
## RHEL or Debian/Ubuntu include paths actually exists on this machine.
## To override (e.g. a custom Apache install), pass them explicitly:
##   make APR_DIR=/path/to/apr HTTPD_DIR=/path/to/httpd
##
#############################################################

API_DIR=..
CC=gcc

LIBXML2_DIR=/usr/include/libxml2/
LIBS=-lxml2

## Must point to a 2.x version of the Apache/APR includes directory.
## RHEL-style paths (/usr/include/apr-1, /usr/include/httpd) are tried
## first, falling back to the Debian/Ubuntu paths (apr-1.0, apache2).
APR_DIR ?= $(firstword $(wildcard /usr/include/apr-1 /usr/include/apr-1.0))
HTTPD_DIR ?= $(firstword $(wildcard /usr/include/httpd /usr/include/apache2))


INCLUDES=-I. -I$(API_DIR) -I$(APR_DIR) -I$(HTTPD_DIR) -I$(LIBXML2_DIR)
DEFS=-D_REENTRANT -D_GNU_SOURCE -D_LARGEFILE64_SOURCE -DLINUX=2
OPTIMIZATION=-O3
#OPTIMIZATION=-g
CFLAGS=$(OPTIMIZATION) $(DEFS) $(INCLUDES) -c -Wall -fPIC

ALL=linux
TARGET=mod_netacuity.so

LINUX_FILES=$(API_DIR)/netacuity_api.o mod_netacuity.o

all: linux

## Make the linux executable.
linux: $(LINUX_FILES)
	@echo -----------------------------------------------
	@echo ---  Making the LINUX $(TARGET) executable
	@echo -----------------------------------------------
	$(CC) $(CFLAGS) $(LIBS)
	$(CC) $(OPTIMIZATION) -shared -o $(TARGET) $(LINUX_FILES) $(LIBS)
	@echo -----------------------------------------------
	@echo ---  LINUX executable successfully made
	@echo -----------------------------------------------

clean:
	rm  -f *.o *.so
	rm  -f $(API_DIR)/*.o

$(API_DIR)/netacuity_api.o: $(API_DIR)/netacuity_api.c $(API_DIR)/netacuity_api.h

mod_netacuity.so: mod_netacuity.c $(API_DIR)/netacuity_api.h
