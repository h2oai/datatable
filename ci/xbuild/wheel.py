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
import glob
import hashlib
import os
import pathlib
import re
import shutil
import sysconfig
import tempfile
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
    """

    def __init__(self, **meta):
        self._check(meta)
        self._meta = meta
        # Wheel creation time, will be used to timestamp individual files
        # inside the archive.
        self._wheel_time = time.gmtime()[:6]
        self._record_file = io.StringIO()


    def _check(self, meta):
        self._check_name(meta)
        self._check_version(meta)
        self._check_summary(meta)
        self._check_description(meta)
        self._check_description_content_type(meta)
        self._check_keywords(meta)


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
            meta["description"] = description.strip()

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

    # def _check_homepag

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




    def build_wheel(self):
        files = self._meta["sources"]
        temp_dir = pathlib.Path(tempfile.mkdtemp())
        # for bdir in {os.path.dirname(f) for f in files}:
        #     tdir = os.path.join(tempdir, bdir)
        #     os.makedirs(tdir, exist_ok=True)
        #     print("Creating dir `%s`" % tdir)

        info_dir = temp_dir / ("%s-%s.dist-info" % (self.name, self.version))
        os.makedirs(info_dir)
        files.append( self._create_WHEEL_file(info_dir) )
        files.append( self._create_METADATA_file(info_dir) )
        files.append( self._create_LICENSE_file(info_dir) )
        # files.append( self._create_top_level_file(info_dir) )
        files.append( self._create_RECORD_file(info_dir, files) )

        wheel_name = "%s-%s-%s.whl" % (self.name, self.version, self.tag)
        with zipfile.ZipFile(wheel_name, "w", allowZip64=True,
                             compression=zipfile.ZIP_DEFLATED) as zipf:
            for filename in files:
                st = os.stat(filename)
                zinfo = zipfile.ZipInfo(filename, self._wheel_time)
                zinfo.external_attr = st.st_mode << 16
                with open(filename, "rb") as inp:
                    content = inp.read()
                    zipf.writestr(zinfo, content)
        print("Wheel file `%s` created" % wheel_name)


    def add_file_to_archive(self, zipf, src):
        if isinstance(src, str):
            target_file = src
        else:
            assert isinstance(src, tuple) and len(src) == 2
            src, target_file = src

        zinfo = zipfile.ZipInfo(target_file, self._wheel_time)
        if os.path.exists(src):
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

        # Make



    def _get_abi_tag(self):
        soabi = sysconfig.get_config_var("SOABI")
        parts = soabi.split("-")
        assert parts[0] == "cpython"
        return "cp" + parts[1]

    @property
    def tag(self):
        abi_tag = self._get_abi_tag()
        impl_tag = abi_tag[:4]
        return "%s-%s-%s" % (impl_tag, abi_tag, "macosx_10_9_x86_64")


    def _create_WHEEL_file(self, path):
        filename = str(path / "WHEEL")
        with open(filename, "wt", encoding="utf-8") as out:
            out.write("Wheel-Version: 1.0\n")
            out.write("Generator: xbuild (0.0.1)\n")
            out.write("Root-Is-Purelib: false\n")
            out.write("Tag: %s\n" % self.tag)
        return filename


    def _create_METADATA_file(self, path):
        filename = str(path / "METADATA")
        with open(filename, "wt", encoding="utf-8") as out:
            out.write("Metadata-Version: 2.1\n")
            out.write("Name: %s\n" % self.name)
            out.write("Version: %s\n" % self.version)
            if self.summary:
                out.write("Summary: %s\n" % self.summary)
            if self.description_content_type:
                out.write("Description-Content-Type: %s\n"
                          % self.description_content_type)
            if self.keywords:
                out.write("Keywords: %s\n" % ",".join(self.keywords))
            if self.home_page:
                out.write("Home-page: %s\n" % self.home_page)
            out.write("Author: %s\n" % self._meta["author"])
            out.write("Author-email: %s\n" % self._meta["author_email"])
            out.write("License: %s\n" % self._meta["license"])
            for classifier in self._meta["classifiers"]:
                out.write("Classifier: %s\n" % classifier)
            out.write("Requires-Python: %s\n" % self._meta["python_requires"])
            for requirement in self._meta["install_requires"]:
                out.write("Requires-Dist: %s\n" % requirement)
            if self.description:
                out.write("\n")
                out.write(self.description)
                out.write("\n")
        return filename


    def _create_LICENSE_file(self, path):
        filename = str(path / "LICENSE.txt")
        shutil.copy("LICENSE", filename)
        return filename


    def _create_top_level_file(self, path):
        filename = str(path / "top_level.txt")
        with open(filename, "wt", encoding="utf-8") as out:
            out.write(self.name + "\n")
        return filename


    def _create_RECORD_file(self, path, files):
        filename = str(path / "RECORD")
        with open(filename, "wt", encoding="utf-8") as out:
            for file in files:
                with open(file, "rb") as inp:
                    content = inp.read()
                digest = hashlib.sha256(content).digest()
                hashstr = base64.urlsafe_b64encode(digest).rstrip(b'=')
                out.write("%s,sha256=%s,%d\n" % (file, hashstr, len(content)))
            out.write("%s,,\n" % (filename,))
        return filename
