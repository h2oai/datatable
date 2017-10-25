//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_FILE_H
#define dt_FILE_H
#include "sys/stat.h"  // fstat
#include <string>      // std::string


class File
{
  std::string name;
  mutable struct stat statbuf;
  int fd;
  int : 32;

public:
  File(const std::string& file);
  File(const std::string& file, int flags, mode_t mode = 0666);
  ~File();

  int descriptor() const;
  size_t size() const;
  void resize(size_t newsize);
  void assert_is_not_dir() const;
  const char* cname() const;

  static void remove(const std::string& name, bool except = false);

private:
  void load_stats() const;
};


#endif
