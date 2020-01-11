#-------------------------------------------------------------------------------
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

BUILDDIR := build/fast
PYTHON   ?= python
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

# Docker settings
DOCKER_BIN             ?= docker
DOCKER_RUN_COMMON_ARGS = --rm \
						--init \
						-u `id -u`:`id -g` \
						-v `pwd`:/dot \
						-w /dot \
						--ulimit core=-1 \
						--entrypoint /bin/bash \
						-e "DT_LARGE_TESTS_ROOT=$(DT_LARGE_TESTS_ROOT)" \
						-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
						$(DOCKER_CUSTOM_ARGS)
DOCKER_RUN             = $(DOCKER_BIN) run $(DOCKER_RUN_COMMON_ARGS)

.PHONY: all clean mrproper build install uninstall test_install test \
		asan benchmark debug bi coverage dist fast xcoverage

ifeq ($(MAKECMDGOALS), fast)
-include ci/fast.mk
ci/fast.mk:
	@echo • Regenerating ci/fast.mk
	@$(PYTHON) ci/make_fast.py
endif

ifeq ($(MAKECMDGOALS), dfast)
-include ci/fast.mk
ci/fast.mk:
	@echo • Regenerating ci/fast.mk [debug mode]
	@$(PYTHON) ci/make_fast.py debug
endif

ifeq ($(MAKECMDGOALS), main-fast)
-include ci/fast.mk
endif


all:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) test


clean::
	rm -rf .asan
	rm -rf .cache
	rm -rf .eggs
	rm -rf build
	rm -rf dist
	rm -rf datatable.egg-info
	rm -f *.so
	rm -f datatable/lib/_datatable*.so
	rm -f ci/fast.mk
	find . -type d -name "__pycache__" -exec rm -rf {} +


mrproper: clean
	git clean -f -d -x



install:
	$(PYTHON) -m pip install . --upgrade --no-cache-dir


uninstall:
	$(PYTHON) -m pip uninstall datatable -y


extra_install:
	$(PYTHON) -m pip install -r requirements_extra.txt

test_install:
	$(PYTHON) -m pip install -r requirements_tests.txt

docs_install:
	$(PYTHON) -m pip install -r requirements_docs.txt


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
asan:
	@$(PYTHON) ext.py asan

build:
	@$(PYTHON) ext.py build

xcoverage:
	@$(PYTHON) ext.py coverage

debug:
	@$(PYTHON) ext.py debug

gitver:
	@$(PYTHON) ext.py gitver



coverage:
	$(PYTHON) -m pip install 'typesentry>=0.2.6' blessed
	$(MAKE) xcoverage
	$(MAKE) test_install
	$(MAKE) gitver
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

dist:
	@$(PYTHON) ext.py wheel -d $(DIST_DIR)

sdist:
	@$(PYTHON) ext.py sdist

version:
	@$(PYTHON) ci/setup_utils.py version



#-------------------------------------------------------------------------------
# CentOS7 build
#-------------------------------------------------------------------------------

DIST_DIR = dist

ARCH := $(shell arch)
OS_NAME ?= centos7
PLATFORM := $(ARCH)_$(OS_NAME)

DOCKER_REPO_NAME ?= harbor.h2o.ai
CONTAINER_NAME_SUFFIX ?= -$(USER)
CONTAINER_NAME ?= $(DOCKER_REPO_NAME)/opsh2oai/datatable-build-$(PLATFORM)$(CONTAINER_NAME_SUFFIX)

PROJECT_VERSION := $(shell grep '^version' datatable/__version__.py | sed 's/version = //' | sed 's/\"//g')
BRANCH_NAME ?= $(shell git rev-parse --abbrev-ref HEAD)
BRANCH_NAME_SUFFIX = +$(BRANCH_NAME)
BUILD_ID ?= local
BUILD_ID_SUFFIX = .$(BUILD_ID)
VERSION = $(PROJECT_VERSION)$(BRANCH_NAME_SUFFIX)$(BUILD_ID_SUFFIX)
CONTAINER_TAG := $(shell echo $(VERSION) | sed 's/[+\/]/-/g')

CONTAINER_NAME_TAG = $(CONTAINER_NAME):$(CONTAINER_TAG)

ifneq ($(CI), 1)
	CI_VERSION_SUFFIX ?= $(BRANCH_NAME)
endif

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

centos7_in_docker: Dockerfile-centos7.$(PLATFORM).tag
	make clean
	$(DOCKER_RUN) \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		$(CONTAINER_NAME_TAG) \
		-c 'make dist'
	mkdir -p $(DIST_DIR)/$(PLATFORM)
	mv $(DIST_DIR)/*.whl $(DIST_DIR)/$(PLATFORM)
	echo $(VERSION) > $(DIST_DIR)/$(PLATFORM)/VERSION.txt

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
DOCKER_IMAGE_TAG ?= 0.8.0-master.9
CENTOS_DOCKER_IMAGE_NAME ?= harbor.h2o.ai/opsh2oai/datatable-build-$(ARCH_NAME)_centos7:$(DOCKER_IMAGE_TAG)
UBUNTU_DOCKER_IMAGE_NAME ?= harbor.h2o.ai/opsh2oai/datatable-build-$(ARCH_NAME)_ubuntu:$(DOCKER_IMAGE_TAG)

docker_image_tag:
	@echo $(DOCKER_IMAGE_TAG)

centos7_build_in_docker_impl:
	$(DOCKER_RUN) \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		$(CENTOS_DOCKER_IMAGE_NAME) \
		-c ". activate $(BUILD_VENV) && \
			make CI=$(CI) dist"

centos7_build_py37_in_docker:
	$(MAKE) BUILD_VENV=datatable-py37-with-pandas centos7_build_in_docker_impl

centos7_build_py36_in_docker:
	$(MAKE) BUILD_VENV=datatable-py36-with-pandas centos7_build_in_docker_impl

centos7_build_py35_in_docker:
	$(MAKE) BUILD_VENV=datatable-py35-with-pandas centos7_build_in_docker_impl

centos7_version_in_docker:
	$(DOCKER_RUN) \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		$(CENTOS_DOCKER_IMAGE_NAME) \
		-c ". activate datatable-py36-with-pandas && \
			python --version && \
			mkdir -p dist && \
			make CI=$(CI) version > dist/VERSION.txt && \
			cat dist/VERSION.txt"

centos7_test_in_docker_impl:
	$(DOCKER_RUN) \
		$(CENTOS_DOCKER_IMAGE_NAME) \
		-c ". activate $(TEST_VENV) && \
			python --version && \
			pip install --no-cache-dir --upgrade dist/*.whl && \
			make CI=$(CI) MODULE=datatable test_install && \
			make test CI=$(CI)"

centos7_test_py37_with_pandas_in_docker:
	$(MAKE) TEST_VENV=datatable-py37-with-pandas centos7_test_in_docker_impl

centos7_test_py36_with_pandas_in_docker:
	$(MAKE) TEST_VENV=datatable-py36-with-pandas centos7_test_in_docker_impl

centos7_test_py35_with_pandas_in_docker:
	$(MAKE) TEST_VENV=datatable-py35-with-pandas centos7_test_in_docker_impl

centos7_test_py36_with_numpy_in_docker:
	$(MAKE) TEST_VENV=datatable-py36-with-numpy centos7_test_in_docker_impl

centos7_test_py36_in_docker:
	$(MAKE) TEST_VENV=datatable-py36 centos7_test_in_docker_impl

ubuntu_build_in_docker_impl:
	$(DOCKER_RUN) \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/$(BUILD_VENV)/bin/activate && \
			python --version && \
			make CI=$(CI) dist"

ubuntu_build_sdist_in_docker:
	$(DOCKER_RUN) \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/datatable-py36-with-pandas/bin/activate && \
			python --version && \
			make CI=$(CI) sdist"

ubuntu_build_py37_in_docker:
	$(MAKE) BUILD_VENV=datatable-py37-with-pandas ubuntu_build_in_docker_impl

ubuntu_build_py36_in_docker:
	$(MAKE) BUILD_VENV=datatable-py36-with-pandas ubuntu_build_in_docker_impl

ubuntu_build_py35_in_docker:
	$(MAKE) BUILD_VENV=datatable-py35-with-pandas ubuntu_build_in_docker_impl

ubuntu_coverage_py36_with_pandas_in_docker:
	$(DOCKER_RUN) \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/datatable-py36-with-pandas/bin/activate && \
			python --version && \
			make CI=$(CI) coverage"

ubuntu_test_in_docker_impl:
	$(DOCKER_RUN) \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/$(TEST_VENV)/bin/activate && \
			python --version && \
			pip freeze && \
			pip install --no-cache-dir dist/*.whl && \
			python --version && \
			make CI=$(CI) MODULE=datatable test_install && \
			make CI=$(CI) test"

ubuntu_test_py37_with_pandas_in_docker:
	$(MAKE) TEST_VENV=datatable-py37-with-pandas ubuntu_test_in_docker_impl

ubuntu_test_py36_with_pandas_in_docker:
	$(MAKE) TEST_VENV=datatable-py36-with-pandas ubuntu_test_in_docker_impl

ubuntu_test_py35_with_pandas_in_docker:
	$(MAKE) TEST_VENV=datatable-py35-with-pandas ubuntu_test_in_docker_impl

ubuntu_test_py36_with_numpy_in_docker:
	$(MAKE) TEST_VENV=datatable-py36-with-numpy ubuntu_test_in_docker_impl

ubuntu_test_py36_in_docker:
	$(MAKE) TEST_VENV=datatable-py36 ubuntu_test_in_docker_impl


printvars:
	@echo PLATFORM=$(PLATFORM)
	@echo PROJECT_VERSION=$(PROJECT_VERSION)
	@echo VERSION=$(VERSION)
	@echo CONTAINER_TAG=$(CONTAINER_TAG)
	@echo CONTAINER_NAME=$(CONTAINER_NAME)
	@echo DOCKER_RUN=$(DOCKER_RUN)

clean::
	rm -f Dockerfile-centos7.$(PLATFORM)
