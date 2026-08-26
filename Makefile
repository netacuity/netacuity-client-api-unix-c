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

## Top-level Makefile — delegates to unix_c/tests
## Requires Linux/Unix with gcc (POSIX headers)

.PHONY: test clean

test:
	$(MAKE) -C unix_c/tests test

clean:
	$(MAKE) -C unix_c/tests clean
