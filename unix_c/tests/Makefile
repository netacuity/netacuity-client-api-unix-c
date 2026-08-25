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
## Makefile for the NetAcuity Client API unit tests.
##
## These tests cover all logic that does not require a
## running NetAcuity server (parsing, validation, get_xml_field).
##
## Usage:
##   make          - compile the test binary
##   make test     - compile and run the tests
##   make clean    - remove compiled output
##
## Requirements: gcc, Linux/Unix (POSIX headers)
##
#############################################################

CC     = gcc
CFLAGS = -Wall -Wextra -Wno-discarded-qualifiers -g -I..

TARGET = test_netacuity_api

all: $(TARGET)

$(TARGET): test_netacuity_api.c
	$(CC) $(CFLAGS) -o $@ $<

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all test clean
