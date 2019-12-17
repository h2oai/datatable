#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
# See Also
# --------
#   [PEP-427](https://www.python.org/dev/peps/pep-0427/)
#       The Wheel Binary Package Format 1.0
#       Note: PEP-491 specifies wheel format 1.9, however the status of that
#       PEP is "deferred".
#
#   [PyPA](https://packaging.python.org/specifications/)
#       List of specification maintained by Python Packaging Authority.
#
#-------------------------------------------------------------------------------
import base64
import hashlib
import io
import os
import re
import sysconfig
import textwrap
import time
import zipfile

rx_name = re.compile(r"[a-zA-Z0-9]([\w\.\-]*[a-zA-Z0-9])?")

rx_version = re.compile(r"(?:\d+!)?"           # epoch
                        r"\d+(?:\.\d+)*"       # release segment
                        r"(?:(?:a|b|rc)\d+)?"  # pre-release segment
                        r"(?:\.post\d+)?"      # post-release segment
                        r"(?:\.dev\d+)?")      # development release segment



class Wheel:
    """
    Class for building wheel files:

        wb = xbuild.Wheel(**meta)
        wb.build_wheel()

    The primary payload is the dictionary of "meta" variables, which
    is largely similar to the format of ``setuptools.setup()``. The
    following fields are supported:

    Parameters
    ----------
    name: str
        The name of the module being built (required).

    version: str
        Version string of the module being built. The format of this
        field is specified in PEP 440 (required).

    summary: str
        Short one-sentence summary describing the purpose of the
        module. Note: this field is called `description` by
        setuptools.

    description: str
        More detailed description of the module, can be multiple
        paragraphs long. By default, this field should be formatted
        using reStructuredText markup. If needed, the description can
        also be in plain text or use Markdown. Note: this field is
        called `long_description` in setuptools.

    description_content_type: str
        Content type of the `description` field. This should be one of
        "text/plain", "text/x-rst" or "text/markdown". If omitted,
        then "text/x-rst" is assumed.

    keywords: List[str]
        List of keywords to assist users in finding your package.

    home_page: str
        The URL of the module's home page. Note: this fields is called
        `url` in setuptools.

    author: str
        The module's author name.

    author_email: str
        Contact email of the author.

    maintainer: str
        Name of the person who is maintaining the package (if different
        from the author).

    maintainer_email: str
        Contact email of the maintainer.

    license: str
        Short string describing the license type of the package.

    classifiers: List[str]
        List of OSI-approved classifiers from the list
        https://pypi.org/classifiers/

    requirements: List[str]
        List of prerequisites for this module. These are names of
        other modules, optionally with standard version specifiers.

    requires_python: str
        Python version(s) that the distribution is compatible with.
        This should be in the form of a version specifier, for example
        ">=3", "==3.6", etc.

    sources: List[str | Tuple[str|bytes, str]]
        List of files to be put into the wheel. Each "source" is
        either a filename, or a tuple (source_filename, dest_filename),
        or a tuple (source_text, dest_filename), where `dest_filename`
        is the name of the file as it should be inside the wheel
        archive.
    """

    def __init__(self, **meta):
        self._sources = None
        self._check_meta(meta)
        self._meta = meta
        # Wheel creation time, will be used to timestamp individual files
        # inside the archive.
        self._wheel_time = time.gmtime()[:6]
        self._record_file = io.StringIO()


    def _check_meta(self, meta):
        self._check_sources(meta)
        self._check_name(meta)
        self._check_version(meta)
        self._check_summary(meta)
        self._check_description(meta)
        self._check_description_content_type(meta)
        self._check_keywords(meta)
        self._check_home_page(meta)
        self._check_author(meta)
        self._check_author_email(meta)
        self._check_maintainer(meta)
        self._check_maintainer_email(meta)
        self._check_license(meta)
        self._check_classifiers(meta)
        self._check_requirements(meta)
        self._check_requires_python(meta)
        self._check_unknown_fields(meta)


    def _check_sources(self, meta):
        assert "sources" in meta, "`sources` field is required"
        sources = meta.pop("sources")
        assert isinstance(sources, list)
        for src in sources:
            if isinstance(src, str):
                if not os.path.exists(src):
                    raise FileNotFoundError("Cannot find file `%s`" % src)
                if os.path.isabs(src):
                    raise ValueError("Source file `%s` cannot use absolute "
                                     "path" % src)
            else:
                assert (isinstance(src, tuple) and len(src) == 2 and
                        isinstance(src[0], (str, bytes)) and
                        isinstance(src[1], str))
        self._sources = sources

    def _check_name(self, meta):
        assert "name" in meta, "`name` field is required"
        name = meta["name"]
        assert isinstance(name, str)
        assert re.fullmatch(rx_name, name), \
               "Invalid package name: ``" % name

    def _check_version(self, meta):
        assert "version" in meta, "`version` field is required"
        version = meta["version"]
        assert isinstance(version, str)
        assert re.fullmatch(rx_version, version), \
               "Version is not in canonical format: `%s`" % version

    def _check_summary(self, meta):
        if "summary" in meta:
            summary = meta["summary"]
            assert isinstance(summary, str)
            meta["summary"] = summary.strip()

    def _check_description(self, meta):
        if "long_description" in meta:
            raise KeyError("Deprecated field `long_description` specified. Did "
                           "you mean `description`?")
        if "description" in meta:
            description = meta["description"]
            assert isinstance(description, str)
            meta["description"] = textwrap.dedent(description).strip()

    def _check_description_content_type(self, meta):
        if "description_content_type" in meta:
            dct = meta["description_content_type"]
            assert isinstance(dct, str)
            assert dct in ["text/plain", "text/x-rst", "text/markdown"], \
                "Invalid description_content_type: `%s`" % dct

    def _check_keywords(self, meta):
        if "keywords" in meta:
            keywords = meta["keywords"]
            assert isinstance(keywords, list)
            assert all(isinstance(kw, str) for kw in keywords)

    def _check_home_page(self, meta):
        if "home_page" in meta:
            home_page = meta["home_page"]
            assert isinstance(home_page, str)
            assert re.match("^https?://", home_page)

    def _check_author(self, meta):
        if "author" in meta:
            assert isinstance(meta["author"], str)

    def _check_author_email(self, meta):
        if "author_email" in meta:
            assert isinstance(meta["author_email"], str)

    def _check_maintainer(self, meta):
        if "maintainer" in meta:
            assert isinstance(meta["maintainer"], str)

    def _check_maintainer_email(self, meta):
        if "maintainer_email" in meta:
            assert isinstance(meta["maintainer_email"], str)

    def _check_license(self, meta):
        if "license" in meta:
            assert isinstance(meta["license"], str)

    def _check_classifiers(self, meta):
        if "classifiers" in meta:
            classifiers = meta["classifiers"]
            assert isinstance(classifiers, list)
            assert all(isinstance(cls, str) for cls in classifiers)

    def _check_requirements(self, meta):
        if "requirements" in meta:
            requirements = meta["requirements"]
            assert isinstance(requirements, list)
            assert all(isinstance(req, str) for req in requirements)

    def _check_requires_python(self, meta):
        if "requires_python" in meta:
            req = meta["requires_python"]
            assert isinstance(req, str)

    def _check_unknown_fields(self, meta):
        known_fields = {
            "name", "version", "summary", "keywords", "description",
            "description_content_type", "home_page", "author", "author_email",
            "maintainer", "maintainer_email", "license", "classifiers",
            "requirements", "requires_python"}
        unknown_fields = set(meta.keys()) - known_fields
        if unknown_fields:
            raise KeyError("Unknown meta parameters %r" % unknown_fields)



    #---------------------------------------------------------------------------
    # Properties
    #---------------------------------------------------------------------------

    @property
    def name(self):
        return self._meta["name"]

    @property
    def version(self):
        return self._meta["version"]

    @property
    def summary(self):
        return self._meta.get("summary")

    @property
    def description(self):
        return self._meta.get("description")

    @property
    def description_content_type(self):
        return self._meta.get("description_content_type")

    @property
    def keywords(self):
        return self._meta.get("keywords")

    @property
    def home_page(self):
        return self._meta.get("home_page")

    @property
    def author(self):
        return self._meta.get("author")

    @property
    def author_email(self):
        return self._meta.get("author_email")

    @property
    def maintainer(self):
        return self._meta.get("maintainer")

    @property
    def maintainer_email(self):
        return self._meta.get("maintainer_email")

    @property
    def license(self):
        return self._meta.get("license")

    @property
    def classifiers(self):
        return self._meta.get("classifiers")

    @property
    def requirements(self):
        return self._meta.get("requirements")

    @property
    def requires_python(self):
        return self._meta.get("requires_python")

    @property
    def sources(self):
        return self._sources

    @property
    def info_dir(self):
        return "%s-%s.dist-info" % (self.name, self.version)


    def build_wheel(self):
        wheel_name = "%s-%s-%s.whl" % (self.name, self.version, self.get_tag())
        with zipfile.ZipFile(wheel_name, "w", allowZip64=True,
                             compression=zipfile.ZIP_DEFLATED) as zipf:
            for filename in self.sources:
                self._add_file_to_archive(zipf, filename)
            self._add_LICENSE_file(zipf)
            self._add_METADATA_file(zipf)
            self._add_WHEEL_file(zipf)
            self._add_RECORD_file(zipf)  # must come last
        print("Wheel file `%s` created" % wheel_name)


    def _add_file_to_archive(self, zipf, src):
        if isinstance(src, str):
            target_file = src
        else:
            assert isinstance(src, tuple) and len(src) == 2
            src, target_file = src

        zinfo = zipfile.ZipInfo(target_file, self._wheel_time)
        if os.path.exists(src):
            st = os.stat(src)
            zinfo.external_attr = st.st_mode << 16
            with open(src, "rb") as inp:
                bcontent = inp.read()
        elif isinstance(src, bytes):
            bcontent = src
        elif isinstance(src, str) and "\n" in src:
            bcontent = src.encode("utf-8")
        else:
            raise FileNotFoundError("File `%s` does not exist" % src)

        # Write the content to zip
        zipf.writestr(zinfo, bcontent)

        # Record file's sha256
        digest = hashlib.sha256(bcontent).digest()
        hashstr = base64.urlsafe_b64encode(digest).rstrip(b'=').decode()
        assert isinstance(hashstr, str)
        self._record_file.write("%s,%s,%d\n"
                                % (target_file, hashstr, len(bcontent)))


    def _get_abi_tag(self):
        soabi = sysconfig.get_config_var("SOABI")
        parts = soabi.split("-")
        assert parts[0] == "cpython"
        return "cp" + parts[1]

    def get_tag(self):
        from distutils.util import get_platform
        abi_tag = self._get_abi_tag()
        impl_tag = abi_tag[:4]
        plat_tag = get_platform().replace('.', '_').replace('-', '_')
        return "%s-%s-%s" % (impl_tag, abi_tag, plat_tag)



    def _add_WHEEL_file(self, zipf):
        out = "Wheel-Version: 1.0\n"
        out += "Generator: xbuild (0.0.1)\n"
        out += "Root-Is-Purelib: false\n"
        out += "Tag: %s\n" % self.get_tag()
        src = (out, self.info_dir + "/WHEEL")
        self._add_file_to_archive(zipf, src)


    def _add_LICENSE_file(self, zipf):
        filename = self.info_dir + "/LICENSE"
        if os.path.exists("LICENSE"):
            src = ("LICENSE", filename)
        elif os.path.exists("LICENSE.txt"):
            src = ("LICENSE.txt", filename)
        else:
            print("Warning: LICENSE file not found in the root directory")
            return
        self._add_file_to_archive(zipf, src)


    def _add_METADATA_file(self, zipf):
        out = "Metadata-Version: 2.1\n"
        out += "Name: %s\n" % self.name
        out += "Version: %s\n" % self.version
        if self.summary:
            out += "Summary: %s\n" % self.summary
        if self.description_content_type:
            out += "Description-Content-Type: %s\n" \
                    % self.description_content_type
        if self.keywords:
            out += "Keywords: %s\n" % ",".join(self.keywords)
        if self.home_page:
            out += "Home-page: %s\n" % self.home_page
        if self.author:
            out += "Author: %s\n" % self.author
        if self.author_email:
            out += "Author-email: %s\n" % self.author_email
        if self.maintainer:
            out += "Maintainer: %s\n" % self.maintainer
        if self.maintainer_email:
            out += "Maintainer-email: %s\n" % self.maintainer_email
        if self.license:
            out += "License: %s\n" % self.license
        if self.classifiers:
            for classifier in self.classifiers:
                out += "Classifier: %s\n" % classifier
        if self.requirements:
            rx_extra = re.compile(r"extra\s*==\s*['\"](\w+)['\"]")
            extras = set()
            for req in self.requirements:
                mm = re.search(rx_extra, req)
                if mm:
                    extras.add(mm.group(1))
                out += "Requires-Dist: %s\n" % req
            for extra in sorted(extras):
                out += "Provides-Extra: %s\n" % extra
        if self.requires_python:
            out += "Requires-Python: %s\n" % self.requires_python
        if self.description:
            out += "\n"
            out += self.description
            out += "\n"
        src = (out, self.info_dir + "/METADATA")
        self._add_file_to_archive(zipf, src)


    def _add_RECORD_file(self, zipf):
        filename = self.info_dir + "/RECORD"
        record = self._record_file.getvalue()
        record += "%s,," % filename
        src = (record, filename)
        self._add_file_to_archive(zipf, src)
