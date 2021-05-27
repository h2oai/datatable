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
#ifndef dt_READ2_MULTISOURCE_h
#define dt_READ2_MULTISOURCE_h
#include "_dt.h"
#include "read2/_declarations.h"
#include "read2/source.h"
namespace dt {
namespace read2 {


/**
  * This class encapsulates various input sources for *read family
  * of functions.
  *
  * Consider that the input for fread may come in a variety of
  * different forms: a string, a file, a list of files, a glob
  * pattern, a URL, an archive, a multi-sheet XLS file, etc. This
  * class encapsulates all that variety under a single interface.
  *
  * Internally, this class uses a linked list of `Source` objects,
  * where each source is supposed to produce a single output frame.
  * However, it occasionally happens that during parsing of a source
  * there will be more than one frame inside. In that case, the
  * Source will signal to the SourceIterator that either it must be
  * read one more time, or will add additional Sources at the current
  * iteration point. It is for this reason that we use a linked list
  * instead of a simple vector here.
  */
class SourceIterator {
  private:
    struct Node;
    using UniqueSource = std::unique_ptr<Source>;
    using UniqueNode = std::unique_ptr<Node>;
    struct Node {
      UniqueSource source;
      UniqueNode next;
      Node(UniqueSource&& src)
        : source(std::move(src)), next(nullptr) {}
    };

    // First Node in the linked list, owned by the current class. All
    // subsequent nodes are owned by their predecessors. Thus, second
    // node is owned by the first, third by the second, etc.
    UniqueNode head_;

    // Pointer to the last returned Node during iteration, or nullptr
    // if the iteration hasn't started yet or already finished.
    Node* currentNode_;

    // Pointer to the node where the next insertion will occur. This
    // pointer is NULL only when the linked list is empty.
    //
    // The rules for this pointer are as follows: whenever an
    // iteration step occurs, this pointer moves to the currentNode_.
    // Whenever `add()` is called, the new node(s) will be added
    // right after the insertion point, and the pointer is moved to
    // the last node added. When iteration ends (next() returns
    // NULL), the insertion point will be at the tail of the linked
    // list.
    Node* insertionPoint_;

  public:
    SourceIterator() noexcept;
    SourceIterator(SourceIterator&&) noexcept;
    SourceIterator(const SourceIterator&) = delete;
    static SourceIterator fromArgs(
        const char* functionName,  // for error messages
        const py::robj arg0,
        const py::robj argFile,
        const py::robj argText,
        const py::robj argCmd,
        const py::robj argUrl
    );

    void add(UniqueSource&&);
    void add(SourceIterator&&);
    Source* next();

  private:
    void initFromAny(const py::robj src);
    void initFromCmd(const py::robj src);
    void initFromFile(const py::robj src);
    void initFromGlob(const py::robj src);
    void initFromText(const py::robj src);
    void initFromUrl(const py::robj src);
};




}}  // namespace dt::read2
#endif
