#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------

BUILDDIR := build/fast
PYTHON   ?= python3
MODULE   ?= .
ifneq ($(CI),)
PYTEST_FLAGS := -vv -s --showlocals
endif

# Platform details
OS       := $(shell uname | tr A-Z a-z)
ARCH     := $(shell uname -m)
PLATFORM := $(ARCH)-$(OS)

# Distribution directory
DIST_DIR := dist/$(PLATFORM)


.PHONY: all
all:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) test


.PHONY: clean
clean::
	rm -rf .asan
	rm -rf .cache
	rm -rf .eggs
	rm -rf build
	rm -rf dist
	rm -rf datatable.egg-info
	rm -f *.so
ifeq (,$(findstring windows,$(OS)))
	rm -f datatable/lib/_datatable*.pyd
else
	rm -f datatable/lib/_datatable*.so
endif
	rm -f ci/fast.mk
	find . -type d -name "__pycache__" -exec rm -rf {} +


.PHONY: mrproper
mrproper: clean
	git clean -f -d -x



.PHONY: extra_install
extra_install:
	$(PYTHON) -m pip install -r requirements_extra.txt


.PHONY: test_install
test_install:
	$(PYTHON) -m pip install -r requirements_tests.txt


.PHONY: docs_install
docs_install:
	$(PYTHON) -m pip install -r requirements_docs.txt


.PHONY: test
test:
	rm -rf build/test-reports 2>/dev/null
	mkdir -p build/test-reports/
	$(PYTHON) -m pytest -ra --maxfail=10 -Werror $(PYTEST_FLAGS) \
		--junit-prefix=$(PLATFORM) \
		--junitxml=build/test-reports/TEST-datatable.xml \
		tests

# In order to run with Address Sanitizer:
#   $ make asan
#   $ DYLD_INSERT_LIBRARIES=/usr/local/opt/llvm/lib/clang/7.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib ASAN_OPTIONS=detect_leaks=1 python -m pytest
#
.PHONY: asan
asan:
	@$(PYTHON) ext.py asan


.PHONY: build
build:
	@$(PYTHON) ext.py build


.PHONY: debug
debug:
	@$(PYTHON) ext.py debug


.PHONY: geninfo
geninfo:
	@$(PYTHON) ext.py geninfo --strict


.PHONY: coverage
coverage:
	$(PYTHON) -m pip install 'typesentry>=0.2.6' blessed
	$(PYTHON) ext.py coverage
	$(MAKE) test_install
	DTCOVERAGE=1 $(PYTHON) -m pytest -x \
		--cov=datatable --cov-report=html:build/coverage-py \
		tests
	DTCOVERAGE=1 $(PYTHON) -m pytest -x \
		--cov=datatable --cov-report=xml:build/coverage.xml \
		tests
	chmod +x ci/llvm-gcov.sh
	# Note: running `lcov` we should pass an absolute path to
	# `ci/llvm-gcov.sh`. The reason is that `lcov` invokes
	# `geninfo` that is trying to detect the base directory in the
	# `find_base_from_graph()` routine. This routine fails
	# for source files that have no function definitions, because
	# these files are excluded from the file graph,
	# where `find_base_from_graph()` does the search. Passing
	# `${CURDIR}/ci/llvm-gcov.sh` to `lcov` fixes the problem.
	lcov --gcov-tool ${CURDIR}/ci/llvm-gcov.sh --capture --directory . --no-external --output-file build/coverage.info
	genhtml --legend --output-directory build/coverage-c --demangle-cpp build/coverage.info
	mv .coverage build/


.PHONY: wheel
wheel:
	@$(PYTHON) ext.py wheel


.PHONY: sdist
sdist:
	@$(PYTHON) ext.py sdist




#-------------------------------------------------------------------------------
#  OBSOLETE...
#-------------------------------------------------------------------------------

DIST_DIR = dist

ifneq (,$(findstring windows,$(OS)))
	ARCH := $(shell arch)
endif
OS_NAME ?= centos7
PLATFORM := $(ARCH)_$(OS_NAME)

DOCKER_REPO_NAME ?= harbor.h2o.ai
CONTAINER_NAME_SUFFIX ?= -$(USER)
CONTAINER_NAME ?= $(DOCKER_REPO_NAME)/opsh2oai/datatable-build-$(PLATFORM)$(CONTAINER_NAME_SUFFIX)

# PROJECT_VERSION := $(shell grep '^version' src/datatable/__version__.py | sed 's/version = //' | sed 's/\"//g')
# BRANCH_NAME ?= $(shell git rev-parse --abbrev-ref HEAD)
# BRANCH_NAME_SUFFIX = +$(BRANCH_NAME)
BUILD_ID ?= local
BUILD_ID_SUFFIX = .$(BUILD_ID)
# VERSION = $(PROJECT_VERSION)$(BRANCH_NAME_SUFFIX)$(BUILD_ID_SUFFIX)
# CONTAINER_TAG := $(shell echo $(VERSION) | sed 's/[+\/]/-/g')
CONTAINER_TAG := unknown

CONTAINER_NAME_TAG = $(CONTAINER_NAME):$(CONTAINER_TAG)


ARCH_SUBST = undefined
FROM_SUBST = undefined
ifeq ($(PLATFORM),x86_64_centos7)
    FROM_SUBST = centos:7
    ARCH_SUBST = $(ARCH)
endif
ifeq ($(PLATFORM),ppc64le_centos7)
    FROM_SUBST = ppc64le\/centos:7
    ARCH_SUBST = $(ARCH)
endif
ifeq ($(PLATFORM),x86_64_ubuntu)
    FROM_SUBST = x86_64_linux
    ARCH_SUBST = $(ARCH)
endif

Dockerfile-centos7.$(PLATFORM): ci/Dockerfile-centos7.in
	cat $< | sed 's/FROM_SUBST/$(FROM_SUBST)/'g | sed 's/ARCH_SUBST/$(ARCH_SUBST)/g' > $@

Dockerfile-centos7.$(PLATFORM).tag: Dockerfile-centos7.$(PLATFORM)
	docker build \
		-t $(CONTAINER_NAME_TAG) \
		-f Dockerfile-centos7.$(PLATFORM) \
		.
	echo $(CONTAINER_NAME_TAG) > $@

centos7_docker_build: Dockerfile-centos7.$(PLATFORM).tag

centos7_docker_publish: Dockerfile-centos7.$(PLATFORM).tag
	docker push $(CONTAINER_NAME_TAG)


#
# Ubuntu image - will be removed
#
Dockerfile-ubuntu.$(PLATFORM): ci/Dockerfile-ubuntu.in
	cat $< | sed 's/FROM_SUBST/$(FROM_SUBST)/'g | sed 's/ARCH_SUBST/$(ARCH_SUBST)/g' > $@

Dockerfile-ubuntu.$(PLATFORM).tag: Dockerfile-ubuntu.$(PLATFORM)
	docker build \
		-t $(CONTAINER_NAME_TAG) \
		-f Dockerfile-ubuntu.$(PLATFORM) \
		.
	echo $(CONTAINER_NAME_TAG) > $@

ubuntu_docker_build: Dockerfile-ubuntu.$(PLATFORM).tag

ubuntu_docker_publish: Dockerfile-ubuntu.$(PLATFORM).tag
	docker push $(CONTAINER_NAME_TAG)

ARCH_NAME ?= $(shell uname -m)
UBUNTU_DOCKER_IMAGE_NAME ?= harbor.h2o.ai/opsh2oai/datatable-build-$(ARCH_NAME)_ubuntu:0.8.0-master.9


ubuntu_coverage_py36_with_pandas_in_docker:
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "DT_LARGE_TESTS_ROOT=$(DT_LARGE_TESTS_ROOT)" \
		$(CUSTOM_ARGS) \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/datatable-py36-with-pandas/bin/activate && \
			python --version && \
			make coverage"
