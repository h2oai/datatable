#-------------------------------------------------------------------------------
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# © H2O.ai 2018
#-------------------------------------------------------------------------------
# In order to compile, run:
#   $ capnp compile -oc++ jay.capnp
#   $ mv jay.capnp.c++ jay.capnp.cc
#
# Then, in file "jay.capnp.cc" replace line
#   #include "jay.capnp.h"
# with
#   #include "jay/jay.capnp.h"
#
# Finally, in file "jay.capnp.h" replace all occurrences of
# `-9223372036854775808ll` with `INT64_MIN`.
#
#-------------------------------------------------------------------------------
@0x9bf7c057bfbac576;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("jay");


struct Frame @0xcad7df991f5731d7 {
  nrows       @0 :Int64;
  ncols       @1 :Int64;
  columns     @2 :List(Column);
}


struct Column @0xbcafaabda033c3e0 {
  name        @0 :Text;
  nullcount   @1 :UInt64;
  description @2 :Text;

  type            :union {
    mu        @3  :Void;
    bool8         :group {
      data    @4  :Buffer;       # [Int8]
      min     @5  :Int8 = -128;
      max     @6  :Int8 = -128;
    }
    int8          :group {
      data    @7  :Buffer;       # [Int8]
      min     @8  :Int8 = -128;
      max     @9  :Int8 = -128;
    }
    int16         :group {
      data    @10 :Buffer;       # [Int16]
      min     @11 :Int16 = -32768;
      max     @12 :Int16 = -32768;
    }
    int32         :group {
      data    @13 :Buffer;       # [Int32]
      min     @14 :Int32 = -2147483648;
      max     @15 :Int32 = -2147483648;
    }
    int64         :group {
      data    @16 :Buffer;       # [Int64]
      min     @17 :Int64 = -9223372036854775808;
      max     @18 :Int64 = -9223372036854775808;
    }
    float32       :group {
      data    @19 :Buffer;       # [Float32]
      min     @20 :Float32 = nan;
      max     @21 :Float32 = nan;
    }
    float64       :group {
      data    @22 :Buffer;       # [Float64]
      min     @23 :Float64 = nan;
      max     @24 :Float64 = nan;
    }
    str32         :group {
      offsets @25 :Buffer;       # [UInt32]
      strdata @26 :Buffer;       # [Char]
    }
    str64         :group {
      offsets @27 :Buffer;       # [Uint64]
      strdata @28 :Buffer;       # [Char]
    }
  }
}

struct Buffer @0xe1bb4a93d544a7c5 {
  offset @0 :UInt64;
  length @1 :UInt64;
}
