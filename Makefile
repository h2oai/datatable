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
PYTEST_FLAGS := -vv -s
endif

# Platform details
OS       := $(shell uname | tr A-Z a-z)
ARCH     := $(shell uname -m)
PLATFORM := $(ARCH)-$(OS)

# Distribution directory
DIST_DIR := dist/$(PLATFORM)

.PHONY: all clean mrproper build install uninstall test_install test \
		benchmark debug bi coverage dist fast ci/fast.mk

ifeq ($(MAKECMDGOALS), fast)
-include ci/fast.mk
ci/fast.mk:
	@echo • Regenerating ci/fast.mk
	@$(PYTHON) ci/make_fast.py
endif

ifeq ($(MAKECMDGOALS), main-fast)
-include ci/fast.mk
endif


all:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) install
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


build:
	$(PYTHON) setup.py build
	cp build/lib.*/datatable/lib/_datatable.* datatable/lib/


install:
	$(PYTHON) -m pip install . --upgrade --no-cache-dir


uninstall:
	$(PYTHON) -m pip uninstall datatable -y


test_install:
	$(PYTHON) -m pip install ${MODULE}[testing] --no-cache-dir


test:
	rm -rf build/test-reports 2>/dev/null
	mkdir -p build/test-reports/
	$(PYTHON) -m pytest -ra --maxfail=10 $(PYTEST_FLAGS) \
		--junit-prefix=$(PLATFORM) \
		--junitxml=build/test-reports/TEST-datatable.xml \
		tests


benchmark:
	$(PYTHON) -m pytest -ra -x -v benchmarks


debug:
	$(MAKE) clean
	DTDEBUG=1 \
	$(MAKE) build
	$(MAKE) install


# In order to run with Address Sanitizer:
#	$ make clean
#   $ make asan_env
#   $ source .asan/bin/activate
#   $ make asan
#   $ make install
#   $ make test
# (note that `make test_install` doesn't work due to py-cpuinfo crashing
# in Asan).

asan_env:
	$(MAKE) clean
	bash -x ci/asan-env.sh

asan:
	DTASAN=1 $(MAKE) fast

bi:
	$(MAKE) build
	$(MAKE) install


coverage:
	$(eval DTCOVERAGE := 1)
	$(eval export DTCOVERAGE)
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) install
	$(MAKE) test_install
	$(PYTHON) -m pytest -x \
		--benchmark-skip \
		--cov=datatable --cov-report=html:build/coverage-py \
		tests
	$(PYTHON) -m pytest -x \
		--benchmark-skip \
		--cov=datatable --cov-report=xml:build/coverage.xml \
		tests
	chmod +x ci/llvm-gcov.sh
	lcov --gcov-tool ci/llvm-gcov.sh --capture --directory . --output-file build/coverage.info
	genhtml build/coverage.info --output-directory build/coverage-c
	mv .coverage build/

dist: build
	$(PYTHON) setup.py bdist_wheel -d $(DIST_DIR)

sdist:
	$(PYTHON) setup.py sdist

version:
	@$(PYTHON) setup.py --version

nothing:
	@echo Nothing


#-------------------------------------------------------------------------------
# CentOS7 build
#-------------------------------------------------------------------------------

DIST_DIR = dist

ARCH := $(shell arch)
OS_NAME ?= centos7
PLATFORM := $(ARCH)_$(OS_NAME)

DOCKER_REPO_NAME ?= docker.h2o.ai
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

CI_VERSION_SUFFIX ?= $(BRANCH_NAME)

ARCH_SUBST = undefined
FROM_SUBST = undefined
ifeq ($(PLATFORM),x86_64_centos7)
    FROM_SUBST = centos:7
    ARCH_SUBST = $(ARCH)
endif
ifeq ($(PLATFORM),ppc64le_centos7)
    FROM_SUBST = ibmcom\/centos-ppc64le
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
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
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
DOCKER_IMAGE_TAG ?= 0.6.0-PR-1210.1
CENTOS_DOCKER_IMAGE_NAME ?= docker.h2o.ai/opsh2oai/datatable-build-$(ARCH_NAME)_centos7:$(DOCKER_IMAGE_TAG)
UBUNTU_DOCKER_IMAGE_NAME ?= docker.h2o.ai/opsh2oai/datatable-build-$(ARCH_NAME)_ubuntu:$(DOCKER_IMAGE_TAG)

centos7_build_in_docker_impl:
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
		$(CUSTOM_ARGS) \
		$(CENTOS_DOCKER_IMAGE_NAME) \
		-c ". activate $(BUILD_VENV) && \
			make CI=$(CI) dist"

centos7_build_py36_in_docker:
	$(MAKE) BUILD_VENV=datatable-py36-with-pandas centos7_build_in_docker_impl

centos7_build_py35_in_docker:
	$(MAKE) BUILD_VENV=datatable-py35-with-pandas centos7_build_in_docker_impl

centos7_version_in_docker:
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
		$(CUSTOM_ARGS) \
		$(CENTOS_DOCKER_IMAGE_NAME) \
		-c ". activate datatable-py36-with-pandas && \
			python --version && \
			mkdir -p dist && \
			make CI=$(CI) version > dist/VERSION.txt && \
			cat dist/VERSION.txt"

centos7_test_in_docker_impl:
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "DT_LARGE_TESTS_ROOT=$(DT_LARGE_TESTS_ROOT)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
		$(CUSTOM_ARGS) \
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
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		-e "DT_LARGE_TESTS_ROOT=$(DT_LARGE_TESTS_ROOT)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
		$(CUSTOM_ARGS) \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/$(BUILD_VENV)/bin/activate && \
			python --version && \
			make CI=$(CI) dist"

ubuntu_build_sdist_in_docker:
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "CI_VERSION_SUFFIX=$(CI_VERSION_SUFFIX)" \
		-e "DT_LARGE_TESTS_ROOT=$(DT_LARGE_TESTS_ROOT)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
		$(CUSTOM_ARGS) \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/datatable-py36-with-pandas/bin/activate && \
			python --version && \
			make CI=$(CI) sdist"

ubuntu_build_py36_in_docker:
	$(MAKE) BUILD_VENV=datatable-py36-with-pandas ubuntu_build_in_docker_impl

ubuntu_build_py35_in_docker:
	$(MAKE) BUILD_VENV=datatable-py35-with-pandas ubuntu_build_in_docker_impl

ubuntu_coverage_py36_with_pandas_in_docker:
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "DT_LARGE_TESTS_ROOT=$(DT_LARGE_TESTS_ROOT)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
		$(CUSTOM_ARGS) \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/datatable-py36-with-pandas/bin/activate && \
			python --version && \
			make CI=$(CI) coverage"

ubuntu_test_in_docker_impl:
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		-e "DT_LARGE_TESTS_ROOT=$(DT_LARGE_TESTS_ROOT)" \
		-e "DTBL_GIT_HASH=$(DTBL_GIT_HASH)" \
		$(CUSTOM_ARGS) \
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

# Note:  We don't actually need to run mrproper in docker (as root) because
#        the build step runs as the user.  But keep the API for consistency.
mrproper_in_docker: mrproper

printvars:
	@echo PLATFORM=$(PLATFORM)
	@echo PROJECT_VERSION=$(PROJECT_VERSION)
	@echo VERSION=$(VERSION)
	@echo CONTAINER_TAG=$(CONTAINER_TAG)
	@echo CONTAINER_NAME=$(CONTAINER_NAME)

clean::
	rm -f Dockerfile-centos7.$(PLATFORM)
