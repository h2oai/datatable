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
		benchmark debug bi coverage fast dist
.SECONDARY: main-fast

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
DOCKER_IMAGE_TAG ?= 0.6.0-PR-1010.3
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
		$(CUSTOM_ARGS) \
		$(CENTOS_DOCKER_IMAGE_NAME) \
		-c ". activate $(TEST_VENV) && \
			python --version && \
			pip install --no-cache-dir --upgrade dist/*.whl && \
			make CI=$(CI) MODULE=datatable test_install && \
			make test CI=$(CI)"

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
		$(CUSTOM_ARGS) \
		$(UBUNTU_DOCKER_IMAGE_NAME) \
		-c ". /envs/$(TEST_VENV)/bin/activate && \
			python --version && \
			pip freeze && \
			pip install --no-cache-dir dist/*.whl && \
			python --version && \
			make CI=$(CI) MODULE=datatable test_install && \
			make CI=$(CI) test"

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

#-------------------------------------------------------------------------------
# "Fast" (but fragile) datatable build
#-------------------------------------------------------------------------------

fast_objects = $(addprefix $(BUILDDIR)/, \
	capi.o                    \
	column.o                  \
	column_bool.o             \
	column_from_pylist.o      \
	column_fw.o               \
	column_int.o              \
	column_pyobj.o            \
	column_real.o             \
	column_string.o           \
	columnset.o               \
	csv/fread.o               \
	csv/py_csv.o              \
	csv/reader.o              \
	csv/reader_arff.o         \
	csv/reader_fread.o        \
	csv/reader_parsers.o      \
	csv/writer.o              \
	datatable.o               \
	datatable_cbind.o         \
	datatable_load.o          \
	datatable_rbind.o         \
	datatablemodule.o         \
	encodings.o               \
	expr/binaryop.o           \
	expr/py_expr.o            \
	expr/reduceop.o           \
	expr/unaryop.o            \
	groupby.o                 \
	jay/open_jay.o            \
	jay/save_jay.o            \
	memrange.o                \
	mmm.o                     \
	options.o                 \
	py_buffers.o              \
	py_column.o               \
	py_columnset.o            \
	py_datatable.o            \
	py_datatable_fromlist.o   \
	py_datawindow.o           \
	py_encodings.o            \
	py_groupby.o              \
	py_rowindex.o             \
	py_types.o                \
	py_utils.o                \
	python/float.o            \
	python/list.o             \
	python/long.o             \
	python/string.o           \
	read/column.o             \
	read/columns.o            \
	read/parallel_reader.o    \
	read/thread_context.o     \
	rowindex.o                \
	rowindex_array.o          \
	rowindex_slice.o          \
	sort.o                    \
	sort_groups.o             \
	sort_insert.o             \
	stats.o                   \
	types.o                   \
	utils.o                   \
	utils/alloc.o             \
	utils/exceptions.o        \
	utils/file.o              \
	utils/pyobj.o             \
	writebuf.o                \
	)

fast:
	$(eval DTDEBUG := 1)
	$(eval export DTDEBUG)
	$(eval CC := $(shell python setup.py get_CC))
	$(eval CCFLAGS := $(shell python setup.py get_CCFLAGS))
	$(eval LDFLAGS := $(shell python setup.py get_LDFLAGS))
	$(eval EXTEXT := $(shell python setup.py get_EXTEXT))
	$(eval export CC CCFLAGS LDFLAGS EXTEXT)
	@echo • Checking dependencies graph
	@python ci/fastcheck.py
	@$(MAKE) --no-print-directory main-fast

post-fast:
	@echo • Copying _datatable.so into ``datatable/lib/_datatable$(EXTEXT)``
	@cp $(BUILDDIR)/_datatable.so datatable/lib/_datatable$(EXTEXT)

main-fast: $(BUILDDIR)/_datatable.so
	@echo • Done.


# ------------------------------------------------------------

$(BUILDDIR)/_datatable.so: $(fast_objects)
	@echo • Linking object files into _datatable.so
	@$(CC) $(LDFLAGS) -o $@ $+
	@$(MAKE) --no-print-directory post-fast



#-------------------------------------------------------------------------------
# Header files
#-------------------------------------------------------------------------------

$(BUILDDIR)/capi.h: c/capi.h
	@echo • Refreshing c/capi.h
	@cp c/capi.h $@

$(BUILDDIR)/column.h: c/column.h $(BUILDDIR)/groupby.h $(BUILDDIR)/memrange.h $(BUILDDIR)/py_types.h $(BUILDDIR)/python/list.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/stats.h $(BUILDDIR)/types.h
	@echo • Refreshing c/column.h
	@cp c/column.h $@

$(BUILDDIR)/columnset.h: c/columnset.h $(BUILDDIR)/column.h $(BUILDDIR)/datatable.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/types.h
	@echo • Refreshing c/columnset.h
	@cp c/columnset.h $@

$(BUILDDIR)/datatable.h: c/datatable.h $(BUILDDIR)/column.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/types.h
	@echo • Refreshing c/datatable.h
	@cp c/datatable.h $@

$(BUILDDIR)/encodings.h: c/encodings.h
	@echo • Refreshing c/encodings.h
	@cp c/encodings.h $@

$(BUILDDIR)/groupby.h: c/groupby.h $(BUILDDIR)/memrange.h $(BUILDDIR)/rowindex.h
	@echo • Refreshing c/groupby.h
	@cp c/groupby.h $@

$(BUILDDIR)/jay/jay_generated.h: c/jay/jay_generated.h $(BUILDDIR)/lib/flatbuffers/flatbuffers.h
	@echo • Refreshing c/jay/jay_generated.h
	@cp c/jay/jay_generated.h $@

$(BUILDDIR)/lib/flatbuffers/base.h: c/lib/flatbuffers/base.h $(BUILDDIR)/lib/flatbuffers/stl_emulation.h
	@echo • Refreshing c/lib/flatbuffers/base.h
	@cp c/lib/flatbuffers/base.h $@

$(BUILDDIR)/lib/flatbuffers/flatbuffers.h: c/lib/flatbuffers/flatbuffers.h $(BUILDDIR)/lib/flatbuffers/base.h
	@echo • Refreshing c/lib/flatbuffers/flatbuffers.h
	@cp c/lib/flatbuffers/flatbuffers.h $@

$(BUILDDIR)/lib/flatbuffers/stl_emulation.h: c/lib/flatbuffers/stl_emulation.h
	@echo • Refreshing c/lib/flatbuffers/stl_emulation.h
	@cp c/lib/flatbuffers/stl_emulation.h $@


$(BUILDDIR)/memrange.h: c/memrange.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/writebuf.h
	@echo • Refreshing c/memrange.h
	@cp c/memrange.h $@

$(BUILDDIR)/mmm.h: c/mmm.h
	@echo • Refreshing c/mmm.h
	@cp c/mmm.h $@

$(BUILDDIR)/options.h: c/options.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/options.h
	@cp c/options.h $@

$(BUILDDIR)/py_column.h: c/py_column.h $(BUILDDIR)/column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/py_column.h
	@cp c/py_column.h $@

$(BUILDDIR)/py_columnset.h: c/py_columnset.h $(BUILDDIR)/datatable.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h
	@echo • Refreshing c/py_columnset.h
	@cp c/py_columnset.h $@

$(BUILDDIR)/py_datatable.h: c/py_datatable.h $(BUILDDIR)/datatable.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/py_datatable.h
	@cp c/py_datatable.h $@

$(BUILDDIR)/py_datawindow.h: c/py_datawindow.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/py_datawindow.h
	@cp c/py_datawindow.h $@

$(BUILDDIR)/py_encodings.h: c/py_encodings.h $(BUILDDIR)/encodings.h
	@echo • Refreshing c/py_encodings.h
	@cp c/py_encodings.h $@

$(BUILDDIR)/py_groupby.h: c/py_groupby.h $(BUILDDIR)/groupby.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/py_groupby.h
	@cp c/py_groupby.h $@

$(BUILDDIR)/py_rowindex.h: c/py_rowindex.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h
	@echo • Refreshing c/py_rowindex.h
	@cp c/py_rowindex.h $@

$(BUILDDIR)/py_types.h: c/py_types.h $(BUILDDIR)/types.h $(BUILDDIR)/utils/assert.h
	@echo • Refreshing c/py_types.h
	@cp c/py_types.h $@

$(BUILDDIR)/py_utils.h: c/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Refreshing c/py_utils.h
	@cp c/py_utils.h $@

$(BUILDDIR)/rowindex.h: c/rowindex.h $(BUILDDIR)/utils/array.h
	@echo • Refreshing c/rowindex.h
	@cp c/rowindex.h $@

$(BUILDDIR)/sort.h: c/sort.h $(BUILDDIR)/utils/array.h
	@echo • Refreshing c/sort.h
	@cp c/sort.h $@

$(BUILDDIR)/stats.h: c/stats.h $(BUILDDIR)/types.h
	@echo • Refreshing c/stats.h
	@cp c/stats.h $@

$(BUILDDIR)/types.h: c/types.h
	@echo • Refreshing c/types.h
	@cp c/types.h $@

$(BUILDDIR)/utils.h: c/utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Refreshing c/utils.h
	@cp c/utils.h $@

$(BUILDDIR)/writebuf.h: c/writebuf.h $(BUILDDIR)/utils/file.h $(BUILDDIR)/utils/shared_mutex.h
	@echo • Refreshing c/writebuf.h
	@cp c/writebuf.h $@


$(BUILDDIR)/csv/dtoa.h: c/csv/dtoa.h
	@echo • Refreshing c/csv/dtoa.h
	@cp c/csv/dtoa.h $@

$(BUILDDIR)/csv/fread.h: c/csv/fread.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/memrange.h $(BUILDDIR)/read/field64.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h
	@echo • Refreshing c/csv/fread.h
	@cp c/csv/fread.h $@

$(BUILDDIR)/csv/freadLookups.h: c/csv/freadLookups.h
	@echo • Refreshing c/csv/freadLookups.h
	@cp c/csv/freadLookups.h $@

$(BUILDDIR)/csv/itoa.h: c/csv/itoa.h
	@echo • Refreshing c/csv/itoa.h
	@cp c/csv/itoa.h $@

$(BUILDDIR)/csv/toa.h: c/csv/toa.h $(BUILDDIR)/csv/dtoa.h $(BUILDDIR)/csv/itoa.h
	@echo • Refreshing c/csv/toa.h
	@cp c/csv/toa.h $@

$(BUILDDIR)/csv/py_csv.h: c/csv/py_csv.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/csv/py_csv.h
	@cp c/csv/py_csv.h $@

$(BUILDDIR)/csv/reader.h: c/csv/reader.h $(BUILDDIR)/memrange.h $(BUILDDIR)/read/columns.h $(BUILDDIR)/utils/pyobj.h
	@echo • Refreshing c/csv/reader.h
	@cp c/csv/reader.h $@

$(BUILDDIR)/csv/reader_arff.h: c/csv/reader_arff.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/datatable.h
	@echo • Refreshing c/csv/reader_arff.h
	@cp c/csv/reader_arff.h $@

$(BUILDDIR)/csv/reader_fread.h: c/csv/reader_fread.h $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/py_csv.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_parsers.h $(BUILDDIR)/memrange.h $(BUILDDIR)/read/parallel_reader.h $(BUILDDIR)/utils/shared_mutex.h
	@echo • Refreshing c/csv/reader_fread.h
	@cp c/csv/reader_fread.h $@

$(BUILDDIR)/csv/reader_parsers.h: c/csv/reader_parsers.h $(BUILDDIR)/types.h
	@echo • Refreshing c/csv/reader_parsers.h
	@cp c/csv/reader_parsers.h $@

$(BUILDDIR)/csv/writer.h: c/csv/writer.h $(BUILDDIR)/datatable.h $(BUILDDIR)/utils.h $(BUILDDIR)/writebuf.h
	@echo • Refreshing c/csv/writer.h
	@cp c/csv/writer.h $@


$(BUILDDIR)/expr/py_expr.h: c/expr/py_expr.h $(BUILDDIR)/column.h $(BUILDDIR)/groupby.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/expr/py_expr.h
	@cp c/expr/py_expr.h $@


$(BUILDDIR)/python/float.h: c/python/float.h $(BUILDDIR)/utils/pyobj.h
	@echo • Refreshing c/python/float.h
	@cp c/python/float.h $@

$(BUILDDIR)/python/list.h: c/python/list.h $(BUILDDIR)/utils/pyobj.h
	@echo • Refreshing c/python/list.h
	@cp c/python/list.h $@

$(BUILDDIR)/python/long.h: c/python/long.h $(BUILDDIR)/utils/pyobj.h
	@echo • Refreshing c/python/long.h
	@cp c/python/long.h $@

$(BUILDDIR)/python/string.h: c/python/string.h $(BUILDDIR)/utils/pyobj.h
	@echo • Refreshing c/python/string.h
	@cp c/python/string.h $@


$(BUILDDIR)/read/column.h: c/read/column.h $(BUILDDIR)/memrange.h $(BUILDDIR)/utils/pyobj.h $(BUILDDIR)/writebuf.h
	@echo • Refreshing c/read/column.h
	@cp c/read/column.h $@

$(BUILDDIR)/read/columns.h: c/read/columns.h $(BUILDDIR)/read/column.h
	@echo • Refreshing c/read/columns.h
	@cp c/read/columns.h $@

$(BUILDDIR)/read/field64.h: c/read/field64.h
	@echo • Refreshing c/read/field64.h
	@cp c/read/field64.h $@

$(BUILDDIR)/read/parallel_reader.h: c/read/parallel_reader.h $(BUILDDIR)/read/thread_context.h
	@echo • Refreshing c/read/parallel_reader.h
	@cp c/read/parallel_reader.h $@

$(BUILDDIR)/read/thread_context.h: c/read/thread_context.h $(BUILDDIR)/read/field64.h $(BUILDDIR)/utils/array.h
	@echo • Refreshing c/read/thread_context.h
	@cp c/read/thread_context.h $@


$(BUILDDIR)/utils/alloc.h: c/utils/alloc.h
	@echo • Refreshing c/utils/alloc.h
	@cp c/utils/alloc.h $@

$(BUILDDIR)/utils/array.h: c/utils/array.h $(BUILDDIR)/memrange.h $(BUILDDIR)/utils/alloc.h $(BUILDDIR)/utils/exceptions.h
	@echo • Refreshing c/utils/array.h
	@cp c/utils/array.h $@

$(BUILDDIR)/utils/assert.h: c/utils/assert.h $(BUILDDIR)/utils/exceptions.h
	@echo • Refreshing c/utils/assert.h
	@cp c/utils/assert.h $@

$(BUILDDIR)/utils/exceptions.h: c/utils/exceptions.h $(BUILDDIR)/types.h
	@echo • Refreshing c/utils/exceptions.h
	@cp c/utils/exceptions.h $@

$(BUILDDIR)/utils/file.h: c/utils/file.h
	@echo • Refreshing c/utils/file.h
	@cp c/utils/file.h $@

$(BUILDDIR)/utils/omp.h: c/utils/omp.h
	@echo • Refreshing c/utils/omp.h
	@cp c/utils/omp.h $@

$(BUILDDIR)/utils/pyobj.h: c/utils/pyobj.h
	@echo • Refreshing c/utils/pyobj.h
	@cp c/utils/pyobj.h $@

$(BUILDDIR)/utils/shared_mutex.h: c/utils/shared_mutex.h $(BUILDDIR)/utils/omp.h
	@echo • Refreshing c/utils/shared_mutex.h
	@cp c/utils/shared_mutex.h $@



#-------------------------------------------------------------------------------
# Object files
#-------------------------------------------------------------------------------

$(BUILDDIR)/capi.o : c/capi.cc $(BUILDDIR)/capi.h $(BUILDDIR)/datatable.h $(BUILDDIR)/rowindex.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column.o : c/column.cc $(BUILDDIR)/column.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/sort.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/file.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_bool.o : c/column_bool.cc $(BUILDDIR)/column.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_from_pylist.o : c/column_from_pylist.cc $(BUILDDIR)/column.h $(BUILDDIR)/py_types.h $(BUILDDIR)/python/float.h $(BUILDDIR)/python/list.h $(BUILDDIR)/python/long.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_fw.o : c/column_fw.cc $(BUILDDIR)/column.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_int.o : c/column_int.cc $(BUILDDIR)/column.h $(BUILDDIR)/csv/toa.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_pyobj.o : c/column_pyobj.cc $(BUILDDIR)/column.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_real.o : c/column_real.cc $(BUILDDIR)/column.h $(BUILDDIR)/csv/toa.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_string.o : c/column_string.cc $(BUILDDIR)/column.h $(BUILDDIR)/encodings.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/columnset.o : c/columnset.cc $(BUILDDIR)/columnset.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/alloc.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@


$(BUILDDIR)/csv/fread.o : c/csv/fread.cc $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/freadLookups.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_fread.h $(BUILDDIR)/csv/reader_parsers.h $(BUILDDIR)/datatable.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/py_csv.o : c/csv/py_csv.cc $(BUILDDIR)/options.h $(BUILDDIR)/csv/py_csv.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/writer.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader.o : c/csv/reader.cc $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_arff.h $(BUILDDIR)/csv/reader_fread.h $(BUILDDIR)/datatable.h $(BUILDDIR)/encodings.h $(BUILDDIR)/options.h $(BUILDDIR)/python/list.h $(BUILDDIR)/python/long.h $(BUILDDIR)/python/string.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_arff.o : c/csv/reader_arff.cc $(BUILDDIR)/csv/reader_arff.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_fread.o : c/csv/reader_fread.cc $(BUILDDIR)/column.h $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_fread.h $(BUILDDIR)/csv/reader_parsers.h $(BUILDDIR)/datatable.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_encodings.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/file.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_parsers.o : c/csv/reader_parsers.cc $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/reader_parsers.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/writer.o : c/csv/writer.cc $(BUILDDIR)/column.h $(BUILDDIR)/csv/toa.h $(BUILDDIR)/csv/writer.h $(BUILDDIR)/datatable.h $(BUILDDIR)/memrange.h $(BUILDDIR)/types.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@


$(BUILDDIR)/datatable.o : c/datatable.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/types.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_cbind.o : c/datatable_cbind.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_load.o : c/datatable_load.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_rbind.o : c/datatable_rbind.cc $(BUILDDIR)/column.h $(BUILDDIR)/datatable.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatablemodule.o : c/datatablemodule.cc $(BUILDDIR)/capi.h $(BUILDDIR)/csv/py_csv.h $(BUILDDIR)/csv/writer.h $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/options.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_columnset.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_datawindow.h $(BUILDDIR)/py_encodings.h $(BUILDDIR)/py_groupby.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/encodings.o : c/encodings.cc $(BUILDDIR)/encodings.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/binaryop.o : c/expr/binaryop.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/types.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/py_expr.o : c/expr/py_expr.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/py_column.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/reduceop.o : c/expr/reduceop.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/types.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/unaryop.o : c/expr/unaryop.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/types.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/groupby.o : c/groupby.cc $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/groupby.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@


$(BUILDDIR)/jay/open_jay.o : c/jay/open_jay.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/jay/jay_generated.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/jay/save_jay.o : c/jay/save_jay.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/jay/jay_generated.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/writebuf.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@


$(BUILDDIR)/memrange.o : c/memrange.cc $(BUILDDIR)/memrange.h $(BUILDDIR)/mmm.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/alloc.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/mmm.o : c/mmm.cc $(BUILDDIR)/mmm.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/options.o : c/options.cc $(BUILDDIR)/options.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/omp.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_buffers.o : c/py_buffers.cc $(BUILDDIR)/column.h $(BUILDDIR)/encodings.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_column.o : c/py_column.cc $(BUILDDIR)/py_column.h $(BUILDDIR)/py_types.h $(BUILDDIR)/utils/pyobj.h $(BUILDDIR)/writebuf.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_columnset.o : c/py_columnset.cc $(BUILDDIR)/columnset.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_columnset.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable.o : c/py_datatable.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_columnset.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_datawindow.h $(BUILDDIR)/py_groupby.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/python/string.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable_fromlist.o : c/py_datatable_fromlist.cc $(BUILDDIR)/column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/python/list.h $(BUILDDIR)/python/long.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datawindow.o : c/py_datawindow.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_datawindow.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_encodings.o : c/py_encodings.cc $(BUILDDIR)/py_encodings.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_groupby.o : c/py_groupby.cc $(BUILDDIR)/py_groupby.h $(BUILDDIR)/utils/pyobj.h $(BUILDDIR)/python/list.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_rowindex.o : c/py_rowindex.cc $(BUILDDIR)/py_column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_types.o : c/py_types.cc $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/column.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_utils.o : c/py_utils.cc $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/python/float.o : c/python/float.cc $(BUILDDIR)/python/float.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/python/list.o : c/python/list.cc $(BUILDDIR)/python/float.h $(BUILDDIR)/python/list.h $(BUILDDIR)/python/long.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/python/long.o : c/python/long.cc $(BUILDDIR)/python/long.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/python/string.o : c/python/string.cc $(BUILDDIR)/python/string.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@


$(BUILDDIR)/read/column.o : c/read/column.cc $(BUILDDIR)/read/column.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_parsers.h $(BUILDDIR)/python/string.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/read/columns.o : c/read/columns.cc $(BUILDDIR)/read/columns.h $(BUILDDIR)/csv/reader_parsers.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/read/parallel_reader.o : c/read/parallel_reader.cc $(BUILDDIR)/csv/reader.h $(BUILDDIR)/read/parallel_reader.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/read/thread_context.o : c/read/thread_context.cc $(BUILDDIR)/read/thread_context.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@


$(BUILDDIR)/rowindex.o : c/rowindex.cc $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/rowindex_array.o : c/rowindex_array.cc $(BUILDDIR)/column.h $(BUILDDIR)/memrange.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/rowindex_slice.o : c/rowindex_slice.cc $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/sort.o : c/sort.cc $(BUILDDIR)/column.h $(BUILDDIR)/datatable.h $(BUILDDIR)/options.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/sort.h $(BUILDDIR)/types.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/alloc.h $(BUILDDIR)/utils/array.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/sort_groups.o : c/sort_groups.cc $(BUILDDIR)/sort.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/sort_insert.o : c/sort_insert.cc $(BUILDDIR)/sort.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/stats.o : c/stats.cc $(BUILDDIR)/column.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/stats.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/types.o : c/types.cc $(BUILDDIR)/types.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils.o : c/utils.cc $(BUILDDIR)/utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/alloc.o : c/utils/alloc.cc $(BUILDDIR)/utils/alloc.h $(BUILDDIR)/mmm.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/exceptions.o : c/utils/exceptions.cc $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/file.o : c/utils/file.cc $(BUILDDIR)/utils.h $(BUILDDIR)/utils/file.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/pyobj.o : c/utils/pyobj.cc $(BUILDDIR)/py_column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_groupby.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/python/float.h $(BUILDDIR)/python/list.h $(BUILDDIR)/python/long.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/writebuf.o : c/writebuf.cc $(BUILDDIR)/memrange.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/alloc.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/omp.h $(BUILDDIR)/writebuf.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@
