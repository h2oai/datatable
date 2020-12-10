//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_FILE_h
#define dt_UTILS_FILE_h
#include <string>          // std::string
#include <sys/stat.h>      // fstat
#include "utils/macros.h"

#if DT_OS_WINDOWS
  #define STAT __stat64
  typedef int mode_t;      // mode_t is not defined on Windows 
#else
  #define STAT stat
#endif


class File
{
  std::string name_;
  mutable struct STAT statbuf_;
  int fd_;
  int flags_;

public:
  static const int APPEND;
  static const int READ;
  static const int READWRITE;
  static const int CREATE;
  static const int OVERWRITE;
  static const int EXTERNALFD;

  File(const std::string& file);
  File(const std::string& file, int flags, int fd = -1, mode_t mode = 0666);
  ~File();

  int descriptor() const;
  size_t size() const;
  static size_t asize(const std::string& filename);
  void resize(size_t newsize);
  void assert_is_not_dir() const;
  const char* cname() const;

  static void remove(const std::string& name, bool except = false);

  static bool exists(const std::string& name) noexcept;
  static bool nonempty(const std::string& name) noexcept;

private:
  void load_stats() const;
};


#endif
