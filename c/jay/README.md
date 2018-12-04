<!---
  Copyright 2018 H2O.ai

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
-->

# Jay file format

## General layout

Jay files have `.jay` extension and the following internal structure:
```text
    "JAY1" '\0'*4        -- 8 bytes
    [data section]       -- ~
    [meta section]       -- <meta size>
    <meta size>          -- 8 bytes
    '\0'*4 "1JAY"        -- 8 bytes
```

* The size of the file in bytes must be a multiple of 8, and all sections
  are aligned at 8-byte boundaries. (i.e., their offsets within the file are
  multiples of 8.)

* The file begins with a 4-byte string `"JAY1"` followed by 4 NUL `\0`
  bytes. The file ends with 4 NUL bytes followed by a 4-byte string `"1JAY"`.
  Future versions of Jay format may use different signatures; however the
  first and the last 3 bytes in the file will always be `"JAY"`.

* Eight bytes immediately before the final signature of the file contain
  the size of the meta section, as an int64 written in little-endian format.
  The value of `meta_size` must be a multiple of 8.

  * If when saving a Frame, the size of the serialized meta section is not
    a multiple of 8, then `meta_size` must be increased to the next multiple
    of 8, and the meta section padded with NULs.

  * If when reading a Jay file, the `meta_size` is either not a multiple
    of 8 or is larger than the size of the file &minus; 24, such file is
    considered invalid.


## Meta section

The meta section is located at the offset `16 + meta_size` from the end
of the file, and it contains all meta information about the Frame (including
all column descriptors) serialized in [Flatbuffers][] format according to
the schema described in [jay.fbs][] file.

The root structure is `jay::Frame` containing the shape of the Frame and the
array of column descriptors:
```text
table Frame {
  nrows:   uint64;
  ncols:   uint64;
  nkeys:   int;
  columns: [Column];
}
```
The `nkeys` variable here tells us that the Frame is sorted by the first
`nkeys` columns, and that those columns, when viewed as tuples, have unique
values.

Each column within the Frame has the following structure:
```text
table Column {
  type:      Type;
  data:      Buffer;
  strdata:   Buffer;
  name:      string;
  nullcount: uint64;
  stats:     Stats;
}
```

* `type` describes the column's "stype". It is an enum with values `Bool8`,
  `Int8`, `Int16`, `Int32`, `Int64`, `Float32`, `Float64`, `Str32`, `Str64`.

* `data` contains the `Buffer` structure, which describes the location
  of this column's main data array within the "data section". The
  interpretation of a column's data depends on its `type`. See the next
  section for more details.

* `strdata` is used only for columns with `type=Str32` or `type=Str64`. It
  describes the location within the data section of this column's character
  data array.

* `name` is the column's name. The name must be non-empty and cannot contain
  characters from the ASCII C0 control set (`0x00` - `0x1F`). In addition,
  names of all columns in the Frame must be unique.

* `nullcount` is the number of NA values in this column.

* `stats` is an optional field containing additional per-column stats, such as
  min and max. The actual type of this field depends on the column's `type`.



## Data section

The portion of the file starting at offset 8 and ending at the start of
the meta section is considered the "data section". This section contains data
buffers for all columns. The location of each column's data buffer(s) is
stored in `Buffer` structures in the meta section. (See above):
```text
struct Buffer {
  offset: uint64;
  length: uint64;
}
```
Here `offset` is the buffer's offset from the start of the data section, and
`length` is the size of the buffer in bytes. The `offset` must be a multiple
of 8 (thus, the start of each buffer is aligned at 8-byte boundary), whereas
the `length` shouldn't be.

Different buffers should not overlap; however, they are not necessarily
adjacent to each other, nor it is required that they are stored in any
particular order.

Depending on the column's `type`, the interpretation of its data buffer is the
following:

* **Bool8**: the buffer is an array of `int8`s, and its size is equal to
  `nrows` bytes. Individual entries in this array are either `0` (false),
  `1` (true), or `-128` (NA). All other values are considered invalid.

* **Int8**: the buffer is an array of `int8`s, its size is `nrows` bytes.
  The value `-128` encodes NAs. All other values are integer values.

* **Int16**: the buffer is an array of `int16`s, and its size is `2 * nrows`
  bytes. NAs are stored as value `-32768`.

* **Int32**: the buffer is an array of `int32`s, its size is `4 * nrows`
  bytes. NA values are stored as `-2**31 == -2147483648`.

* **Int64**: the buffer is an array of `int64`s of size `8 * nrows` bytes.
  NA values are stored as `-2**63`.

* **Float32**: the buffer is an array of C `float`s of size `4 * nrows`
  bytes. Any float NaN value is considered an NA.

* **Float64**: the buffer is an array of C `double`s of size `8 * nrows`
  bytes. Any double NaN value is considered an NA.

* **Str32**: Columns of this type have two data buffers: `data` and
  `strdata`.

  * `strdata` is the "character data array". It contains string values of
    all column elements stored back-to-back. The strings are UTF-8 encoded.
    NA and empty strings are not present in this array.

  * `data` buffer is an array of `uint32`s of size `4 * (nrows + 1)` bytes.
    Thus, the array has `nrows + 1` elements. The first element is always 0,
    and all the other elements contain offsets within `strdata` where each
    column's string ends. Only the lower 31 bits are used to store the
    offset, however. The highest bit, if present, indicates an NA string.

  For example, suppose the column contains 5 string values `["a", "bcd", "",
  None, "z"]`. Such column will be encoded as a 5-byte `strdata` buffer
  containing bytes `abcdz`; and a 6-element `data` buffer containing
  `[0, 1, 4, 4, (1<<31)|4, 5]`. Note that the last element in `data` is
  equal to the size of `strdata` (possibly after turning off the topmost bit).
  Thus, columns of `str32` type cannot store more than `2**31 = 2.1GB` of
  character data.

* **Str64**: similar in structure to **str32** columns, except the
  `data` buffer is an array of `uint64`s, and therefore they can store up to
  `2**63 = 9.2EB`. NA values for this type are stored as the bit mask with
  the topmost bit (`1 << 63`) turned on.


## Disclaimers

This document describes file format **Jay**, which is an *open* file format.
No restrictions are placed on other people or organizations who would want
to implement their own Jay readers / writers.




[flatbuffers]: https://google.github.io/flatbuffers/
[jay.fbs]:     https://github.com/h2oai/datatable/blob/master/c/jay/jay.fbs
