//------------------------------------------------------------------------------
// zlib.h -- interface of the 'zlib' general purpose compression library
// version 1.2.11, January 15th, 2017
//
// Copyright (C) 1995-2017 Jean-loup Gailly and Mark Adler
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
// Jean-loup Gailly        Mark Adler
// jloup@gzip.org          madler@alumni.caltech.edu
//
//
// The data format used by the zlib library is described by RFCs (Request for
// Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
// (zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).
//
//------------------------------------------------------------------------------
// This is a modified version of the original zlib.h:
//   - fix many warnings
//   - leave only code needed for the deflate functionality
//
//------------------------------------------------------------------------------
#ifndef ZLIB_H
#define ZLIB_H
#include "lib/zlib/zconf.h"
namespace zlib {


#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpadded"
#endif


#define ZLIB_VERSION "1.2.11"
#define ZLIB_VERNUM 0x12b0
#define ZLIB_VER_MAJOR 1
#define ZLIB_VER_MINOR 2
#define ZLIB_VER_REVISION 11
#define ZLIB_VER_SUBREVISION 0

/*
    The 'zlib' compression library provides in-memory compression and
  decompression functions, including integrity checks of the uncompressed data.
  This version of the library supports only one compression method (deflation)
  but other algorithms will be added later and will have the same stream
  interface.

    Compression can be done in a single step if the buffers are large enough,
  or can be done by repeated calls of the compression function.  In the latter
  case, the application must provide more input and/or consume the output
  (providing more output space) before each call.

    The compressed data format used by default by the in-memory functions is
  the zlib format, which is a zlib wrapper documented in RFC 1950, wrapped
  around a deflate stream, which is itself documented in RFC 1951.

    The library also supports reading and writing files in gzip (.gz) format
  with an interface similar to that of stdio using the functions that start
  with "gz".  The gzip format is different from the zlib format.  gzip is a
  gzip wrapper, documented in RFC 1952, wrapped around a deflate stream.

    This library can optionally read and write gzip and raw deflate streams in
  memory as well.

    The zlib format was designed to be compact and fast for use in memory
  and on communications channels.  The gzip format was designed for single-
  file compression on file systems, has a larger header than zlib to maintain
  directory information, and uses a different, slower check method than zlib.

    The library does not install any signal handler.  The decoder checks
  the consistency of the compressed data, so the library should never crash
  even in the case of corrupted input.
*/

typedef voidpf (*alloc_func) OF((voidpf opaque, uInt items, uInt size));
typedef void   (*free_func)  OF((voidpf opaque, voidpf address));

struct deflate_state;

struct z_stream {
    z_const Bytef *next_in;     /* next input byte */
    uInt     avail_in;  /* number of bytes available at next_in */
    uLong    total_in;  /* total number of input bytes read so far */

    Bytef    *next_out; /* next output byte will go here */
    uInt     avail_out; /* remaining free space at next_out */
    uLong    total_out; /* total number of bytes output so far */

    z_const char *msg;  /* last error message, NULL if no error */
    deflate_state* state; /* not visible by applications */

    alloc_func zalloc;  /* used to allocate the internal state */
    free_func  zfree;   /* used to free the internal state */
    voidpf     opaque;  /* private data object passed to zalloc and zfree */

    int     data_type;  /* best guess about the data type: binary or text
                           for deflate, or the decoding state for inflate */
    uLong   adler;      /* Adler-32 or CRC-32 value of the uncompressed data */
    uLong   reserved;   /* reserved for future use */
};

typedef z_stream* z_streamp;
#define z_stream_size  static_cast<int>(sizeof(z_stream))

/*
     gzip header information passed to and from zlib routines.  See RFC 1952
  for more details on the meanings of these fields.
*/
typedef struct gz_header_s {
    int     text;       /* true if compressed data believed to be text */
    uLong   time;       /* modification time */
    int     xflags;     /* extra flags (not used when writing a gzip file) */
    int     os;         /* operating system */
    Bytef   *extra;     /* pointer to extra field or Z_NULL if none */
    uInt    extra_len;  /* extra field length (valid if extra != Z_NULL) */
    uInt    extra_max;  /* space at extra (only when reading header) */
    Bytef   *name;      /* pointer to zero-terminated file name or Z_NULL */
    uInt    name_max;   /* space at name (only when reading header) */
    Bytef   *comment;   /* pointer to zero-terminated comment or Z_NULL */
    uInt    comm_max;   /* space at comment (only when reading header) */
    int     hcrc;       /* true if there was or will be a header crc */
    int     done;       /* true when done reading gzip header (not used
                           when writing a gzip file) */
} gz_header;


                        /* constants */

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
#define Z_BLOCK         5
#define Z_TREES         6
/* Allowed flush values; see deflate() and inflate() below for details */

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)
/* Return codes for the compression/decompression functions. Negative values
 * are errors, positive values are used for special but normal events.
 */

#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
/* compression levels */

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_RLE                 3
#define Z_FIXED               4
#define Z_DEFAULT_STRATEGY    0
/* compression strategy; see deflateInit2() below for details */

#define Z_BINARY   0
#define Z_TEXT     1
#define Z_ASCII    Z_TEXT   /* for compatibility with 1.2.2 and earlier */
#define Z_UNKNOWN  2
/* Possible values of the data_type field for deflate() */

#define Z_DEFLATED   8
/* The deflate compression method (the only one supported in this version) */

#define Z_NULL  nullptr  /* for initializing zalloc, zfree, opaque */



//------------------------------------------------------------------------------
//                            basic functions
//------------------------------------------------------------------------------

/**
  *   deflate compresses as much data as possible, and stops when the input
  * buffer becomes empty or the output buffer becomes full.  It may introduce
  * some output latency (reading input without producing any output) except when
  * forced to flush.
  *
  *   The detailed semantics are as follows.  deflate performs one or both of the
  * following actions:
  *
  * - Compress more input starting at next_in and update next_in and avail_in
  *   accordingly.  If not all input can be processed (because there is not
  *   enough room in the output buffer), next_in and avail_in are updated and
  *   processing will resume at this point for the next call of deflate().
  *
  * - Generate more output starting at next_out and update next_out and avail_out
  *   accordingly.  This action is forced if the parameter flush is non zero.
  *   Forcing flush frequently degrades the compression ratio, so this parameter
  *   should be set only when necessary.  Some output may be provided even if
  *   flush is zero.
  *
  *   Before the call of deflate(), the application should ensure that at least
  * one of the actions is possible, by providing more input and/or consuming more
  * output, and updating avail_in or avail_out accordingly; avail_out should
  * never be zero before the call.  The application can consume the compressed
  * output when it wants, for example when the output buffer is full (avail_out
  * == 0), or after each call of deflate().  If deflate returns Z_OK and with
  * zero avail_out, it must be called again after making room in the output
  * buffer because there might be more output pending. See deflatePending(),
  * which can be used if desired to determine whether or not there is more ouput
  * in that case.
  *
  *   Normally the parameter flush is set to Z_NO_FLUSH, which allows deflate to
  * decide how much data to accumulate before producing output, in order to
  * maximize compression.
  *
  *   If the parameter flush is set to Z_SYNC_FLUSH, all pending output is
  * flushed to the output buffer and the output is aligned on a byte boundary, so
  * that the decompressor can get all input data available so far.  (In
  * particular avail_in is zero after the call if enough output space has been
  * provided before the call.) Flushing may degrade compression for some
  * compression algorithms and so it should be used only when necessary.  This
  * completes the current deflate block and follows it with an empty stored block
  * that is three bits plus filler bits to the next byte, followed by four bytes
  * (00 00 ff ff).
  *
  *   If flush is set to Z_PARTIAL_FLUSH, all pending output is flushed to the
  * output buffer, but the output is not aligned to a byte boundary.  All of the
  * input data so far will be available to the decompressor, as for Z_SYNC_FLUSH.
  * This completes the current deflate block and follows it with an empty fixed
  * codes block that is 10 bits long.  This assures that enough bytes are output
  * in order for the decompressor to finish the block before the empty fixed
  * codes block.
  *
  *   If flush is set to Z_BLOCK, a deflate block is completed and emitted, as
  * for Z_SYNC_FLUSH, but the output is not aligned on a byte boundary, and up to
  * seven bits of the current block are held to be written as the next byte after
  * the next deflate block is completed.  In this case, the decompressor may not
  * be provided enough bits at this point in order to complete decompression of
  * the data provided so far to the compressor.  It may need to wait for the next
  * block to be emitted.  This is for advanced applications that need to control
  * the emission of deflate blocks.
  *
  *   If flush is set to Z_FULL_FLUSH, all output is flushed as with
  * Z_SYNC_FLUSH, and the compression state is reset so that decompression can
  * restart from this point if previous compressed data has been damaged or if
  * random access is desired.  Using Z_FULL_FLUSH too often can seriously degrade
  * compression.
  *
  *   If deflate returns with avail_out == 0, this function must be called again
  * with the same value of the flush parameter and more output space (updated
  * avail_out), until the flush is complete (deflate returns with non-zero
  * avail_out).  In the case of a Z_FULL_FLUSH or Z_SYNC_FLUSH, make sure that
  * avail_out is greater than six to avoid repeated flush markers due to
  * avail_out == 0 on return.
  *
  *   If the parameter flush is set to Z_FINISH, pending input is processed,
  * pending output is flushed and deflate returns with Z_STREAM_END if there was
  * enough output space.  If deflate returns with Z_OK or Z_BUF_ERROR, this
  * function must be called again with Z_FINISH and more output space (updated
  * avail_out) but no more input data, until it returns with Z_STREAM_END or an
  * error.  After deflate has returned Z_STREAM_END, the only possible operations
  * on the stream are deflateReset or deflateEnd.
  *
  *   Z_FINISH can be used in the first deflate call after deflateInit if all the
  * compression is to be done in a single step.  In order to complete in one
  * call, avail_out must be at least the value returned by deflateBound (see
  * below).  Then deflate is guaranteed to return Z_STREAM_END.  If not enough
  * output space is provided, deflate will not return Z_STREAM_END, and it must
  * be called again as described above.
  *
  *   deflate() sets strm->adler to the Adler-32 checksum of all input read
  * so far (that is, total_in bytes).  If a gzip stream is being generated, then
  * strm->adler will be the CRC-32 checksum of the input read so far.  (See
  * deflateInit2 below.)
  *
  *   deflate() may update strm->data_type if it can make a good guess about
  * the input data type (Z_BINARY or Z_TEXT).  If in doubt, the data is
  * considered binary.  This field is only for information purposes and does not
  * affect the compression algorithm in any manner.
  *
  *   deflate() returns Z_OK if some progress has been made (more input
  * processed or more output produced), Z_STREAM_END if all input has been
  * consumed and all output has been produced (only when flush is set to
  * Z_FINISH), Z_STREAM_ERROR if the stream state was inconsistent (for example
  * if next_in or next_out was Z_NULL or the state was inadvertently written over
  * by the application), or Z_BUF_ERROR if no progress is possible (for example
  * avail_in or avail_out was zero).  Note that Z_BUF_ERROR is not fatal, and
  * deflate() can be called again with more input and more output space to
  * continue compressing.
  */
int deflate(z_stream* strm, int flush);


/**
  *   All dynamically allocated data structures for this stream are freed.
  * This function discards any unprocessed input and does not flush any pending
  * output.
  *
  *   deflateEnd returns Z_OK if success, Z_STREAM_ERROR if the
  * stream state was inconsistent, Z_DATA_ERROR if the stream was freed
  * prematurely (some input or output was discarded).  In the error case, msg
  * may be set but then points to a static string (which must not be
  * deallocated).
  */
int deflateEnd(z_stream* strm);


/**
  *   This function is equivalent to deflateEnd followed by deflateInit, but
  * does not free and reallocate the internal compression state.  The stream
  * will leave the compression level and any other attributes that may have been
  * set unchanged.
  *
  *   deflateReset returns Z_OK if success, or Z_STREAM_ERROR if the source
  * stream state was inconsistent (such as zalloc or state being Z_NULL).
  */
int deflateReset(z_stream* strm);


/**
  *   deflateBound() returns an upper bound on the compressed size after
  * deflation of sourceLen bytes.  It must be called after deflateInit() or
  * deflateInit2(), and after deflateSetHeader(), if used.  This would be used
  * to allocate an output buffer for deflation in a single pass, and so would be
  * called before deflate().  If that first deflate() call is provided the
  * sourceLen input bytes, an output buffer allocated to the size returned by
  * deflateBound(), and the flush value Z_FINISH, then deflate() is guaranteed
  * to return Z_STREAM_END.  Note that it is possible for the compressed size to
  * be larger than the value returned by deflateBound() if flush options other
  * than Z_FINISH or Z_NO_FLUSH are used.
  */
uLong deflateBound(z_stream* strm, uLong sourceLen);




//------------------------------------------------------------------------------
//                         checksum functions
//------------------------------------------------------------------------------

/**
  *   Update a running Adler-32 checksum with the bytes buf[0..len-1] and
  * return the updated checksum.  If buf is Z_NULL, this function returns the
  * required initial value for the checksum.
  *
  *   An Adler-32 checksum is almost as reliable as a CRC-32 but can be computed
  * much faster.
  *
  * Usage example:
  *
  *   uLong adler = adler32(0L, Z_NULL, 0);
  *
  *   while (read_buffer(buffer, length) != EOF) {
  *     adler = adler32(adler, buffer, length);
  *   }
  *   if (adler != original_adler) error();
  */
uLong adler32(uLong adler, const Bytef* buf, uInt len);


/**
  *   Update a running CRC-32 with the bytes buf[0..len-1] and return the
  * updated CRC-32.  If buf is Z_NULL, this function returns the required
  * initial value for the crc.  Pre- and post-conditioning (one's complement) is
  * performed within this function so it shouldn't be done by the application.
  *
  * Usage example:
  *
  *   uLong crc = crc32(0L, Z_NULL, 0);
  *
  *   while (read_buffer(buffer, length) != EOF) {
  *     crc = crc32(crc, buffer, length);
  *   }
  *   if (crc != original_crc) error();
  */
uLong crc32(uLong crc, const Bytef* buf, uInt len);




//------------------------------------------------------------------------------
//                      various hacks, don't look :)
//------------------------------------------------------------------------------

/* deflateInit and inflateInit are macros to allow checking the zlib version
 * and the compiler's view of z_stream:
 */
int deflateInit_(z_stream* strm, int level, const char *version,
                 int stream_size);
int deflateInit2_(z_stream* strm, int level, int method, int windowBits,
                  int memLevel, int strategy, const char *version,
                  int stream_size);

#define deflateInit(strm, level) \
          deflateInit_((strm), (level), ZLIB_VERSION, z_stream_size)
#define deflateInit2(strm, level, method, windowBits, memLevel, strategy) \
          deflateInit2_((strm),(level),(method),(windowBits),(memLevel),\
                        (strategy), ZLIB_VERSION, z_stream_size)


const char* zError(int);


#if defined(__clang__)
  #pragma clang diagnostic pop
#endif


} // namespace zlib
#endif /* ZLIB_H */
