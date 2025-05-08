# Makefile.am – Automake build rules for wd-broker
#
#     Copyright 2025 Julian Friedrich
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is provided on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Source repository: https://github.com/fjulian79/wd-broker

SUBDIRS = src

dist_testdir = wd-broker-test

test-package: all
	@echo "Preparing test package..."
	python3 test/gen_constants.py 
	mkdir -p $(dist_testdir)
	rm -rf $(dist_testdir)/*
	cp -r src/.libs $(dist_testdir)/
	cp src/wd-broker $(dist_testdir)/
	cp src/wd-ctrl $(dist_testdir)/
	cp src/wd-client-test $(dist_testdir)/
	cp test/constants.py $(dist_testdir)/
	cp test/common.py $(dist_testdir)/
	cp test/test-*.py $(dist_testdir)/
	cp test/run-tests.sh $(dist_testdir)/
	tar czf wd-broker-test.tar.gz $(dist_testdir)
	@echo "Test package created: wd-broker-test.tar.gz"
