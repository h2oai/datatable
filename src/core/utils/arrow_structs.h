//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_UTILS_ARROW_STRUCTS_h
#define dt_UTILS_ARROW_STRUCTS_h
#include <memory>
#include "_dt.h"
#include "utils/assert.h"
namespace dt {


//------------------------------------------------------------------------------
// The following definitions were taken from
// http://arrow.apache.org/docs/format/CDataInterface.html#structure-definitions
// available under Apache License 2.0.
//
// © Copyright 2016-2019 Apache Software Foundation
//------------------------------------------------------------------------------

#define ARROW_FLAG_DICTIONARY_ORDERED 1
#define ARROW_FLAG_NULLABLE 2
#define ARROW_FLAG_MAP_KEYS_SORTED 4


struct ArrowSchema {
  // Array type description
  const char* format;
  const char* name;
  const char* metadata;
  int64_t flags;
  int64_t n_children;
  ArrowSchema** children;
  ArrowSchema* dictionary;

  // Release callback
  void (*release)(ArrowSchema*);
  // Opaque producer-specific data
  void* private_data;
};


struct ArrowArray {
  // Array data description
  int64_t length;
  int64_t null_count;
  int64_t offset;
  int64_t n_buffers;
  int64_t n_children;
  const void** buffers;
  ArrowArray** children;
  ArrowArray* dictionary;

  // Release callback
  void (*release)(ArrowArray*);
  // Opaque producer-specific data
  void* private_data;
};




//------------------------------------------------------------------------------
// Helper objects that enable RAII semantics for Arrow* structs
//------------------------------------------------------------------------------

/**
  * Simple wrapper around the ArrowSchema structure that ensures
  * the .release() callback is automatically called when the object
  * falls out of scope.
  */
class OArrowSchema {
  private:
    ArrowSchema schema_;

  public:
    OArrowSchema() {
      schema_.format = nullptr;
      schema_.name = nullptr;
      schema_.metadata = nullptr;
      schema_.flags = 0;
      schema_.n_children = 0;
      schema_.children = nullptr;
      schema_.dictionary = nullptr;
      schema_.release = nullptr;
      schema_.private_data = nullptr;
    }
    OArrowSchema(const OArrowSchema&) = delete;
    OArrowSchema(OArrowSchema&&) = delete;

    ~OArrowSchema() {
      if (schema_.release) {
        schema_.release(&schema_);
        wassert(schema_.release == nullptr);
      }
    }

    size_t intptr() const noexcept {
      return reinterpret_cast<size_t>(&schema_);
    }

    ArrowSchema* operator->() noexcept {
      return &schema_;
    }

    const ArrowSchema* operator->() const noexcept {
      return &schema_;
    }
};


class OArrowArray;
class ArrowArrayData {
  private:
    Column column_;
    std::unique_ptr<OArrowArray> root_;
    std::vector<const void*> buffers_;

  public:
    ArrowArrayData(Column&& column)
      : column_(std::move(column)) {}

    void store(std::unique_ptr<OArrowArray>&& ptr) {
      root_ = std::move(ptr);
    }

    const Column& column() const { return column_; }
    std::vector<const void*>& buffers() { return buffers_; }
};



/**
  * Simple wrapper around the ArrowArray structure that ensures
  * the .release() callback is automatically called when the object
  * is deleted.
  *
  * This class is used both when we ingest data from an external
  * arrow object, and when when we send our data into arrow.
  *
  * When ingesting data from arrow, a pointern ArrowArray* struct
  */
class OArrowArray {
  private:
    ArrowArray array_;

  private:
    /**
      * Create a new OArrowArray structure from an existing
      * ArrowArray* pointer, which will be marked as "released".
      *
      * According to Arrow's documentation:
      *   > The consumer can move the ArrowArray structure by bitwise
      *   > copying or shallow member-wise copying. Then it MUST mark
      *   > the source structure released but without calling the release
      *   > callback. This ensures that only one live copy of the
      *   > struct is active at any given time and that lifetime is
      *   > correctly communicated to the producer.
      */
    OArrowArray(ArrowArray* arr) noexcept {
      array_.length       = arr->length;
      array_.null_count   = arr->null_count;
      array_.offset       = arr->offset;
      array_.n_buffers    = arr->n_buffers;
      array_.n_children   = arr->n_children;
      array_.buffers      = arr->buffers;
      array_.children     = arr->children;
      array_.dictionary   = arr->dictionary;
      array_.release      = arr->release;
      array_.private_data = arr->private_data;
      arr->release = nullptr;
    }

  public:
    OArrowArray() {
      array_.length = 0;
      array_.null_count = 0;
      array_.offset = 0;
      array_.n_buffers = 0;
      array_.n_children = 0;
      array_.buffers = nullptr;
      array_.children = nullptr;
      array_.dictionary = nullptr;
      array_.release = nullptr;
      array_.private_data = nullptr;
    }
    OArrowArray(const OArrowArray&) = delete;
    OArrowArray(OArrowArray&& other) noexcept
      : OArrowArray(&other.array_) {}

    ~OArrowArray() {
      if (array_.release) {
        array_.release(&array_);
        wassert(array_.release == nullptr);
      }
    }

    size_t intptr() const noexcept {
      return reinterpret_cast<size_t>(&array_);
    }

    ArrowArray* operator->() noexcept {
      return &array_;
    }

    const ArrowArray* operator->() const noexcept {
      return &array_;
    }

    /**
      * Return i-th child of the current array as a shared pointer.
      * The original ArrowArray struct is marked as released, so that
      * the returned shared pointer will be the sole owner of that
      * data.
      */
    std::shared_ptr<OArrowArray> detach_child(size_t i) {
      xassert(i < static_cast<size_t>(array_.n_children));
      auto ptr = array_.children[i];
      return std::shared_ptr<OArrowArray>(new OArrowArray(ptr));
    }

    /**
      * Store the pointer to `this` object inside its ArrowArrayData*
      * structure. Effectively, after this call, `this` will be owned
      * by itself.
      *
      * This method can only be called when there is an established
      * promise that its `->release()` callback will be called at a
      * future time.
      */
    void ouroboros() {
      auto data = static_cast<ArrowArrayData*>(array_.private_data);
      data->store(std::unique_ptr<OArrowArray>(this));
    }
};


static_assert(sizeof(ArrowArray) == sizeof(OArrowArray),
    "Sizes of ArrowArray and OArrowArray do not match");
static_assert(sizeof(ArrowSchema) == sizeof(OArrowSchema),
    "Sizes of ArrowSchema and OArrowSchema do not match");




} // namespace dt
#endif
