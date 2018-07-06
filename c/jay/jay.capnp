@0x9bf7c057bfbac576;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("jay");

const naInt64 :Int64 = -9223372036854775807;

struct Frame {
  nrows       @0 :Int64;
  ncols       @1 :Int64;
  columns     @2 :List(Column);
}


struct Column {
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
      min     @17 :Int64 = .naInt64;
      max     @18 :Int64 = .naInt64;
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

struct Buffer {
  offset @0 :UInt64;
  length @1 :UInt64;
}
