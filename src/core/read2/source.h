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
#ifndef dt_READ2_SOURCE_h
#define dt_READ2_SOURCE_h
#include <memory>            // std::unique_ptr
#include <string>            // std::string
#include "python/_all.h"     // py::oobj
namespace dt {
namespace read2 {

class GenericReader;
class Source;


class SourceIterable {
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
    // The rules for this pointer are following: whenever an iteration
    // step occurs, the insertion pointer moves to the currentNode_;
    // otherwise whenever add() is called, the new node(s) will be
    // added right after the insertion point, and the pointer is moved
    // to the last node added. When iteration ends (next() returns
    // NULL), the insertion point will be at the tail of the linked
    // list.
    Node* insertionPoint_;

  public:
    SourceIterable() noexcept;

    void add(UniqueSource&&);
    void add(SourceIterable&&);
    Source* next();
};


class Source {
  public:
    virtual ~Source() = default;
    virtual bool keepReading() const { return false; }
};



#if 0
/**
  * Single input source for ?read functions. This is a base abstract
  * class, with different implementations.
  *
  * The objects of this class are used by the MultiSource class only.
  */
class Source
{
  protected:
    std::string name_;

  public:
    Source(const std::string& name);
    virtual ~Source();

    // Each source has a name (the names need not be unique) which
    // attempts to identify the origin of the object. This name will
    // be carried in the `.source` attribute of the frame produced.
    const std::string& name() const { return name_; }

    // Main Source function, it will read the data from the
    // referenced input source, and return it as a python Frame
    // object.
    // virtual py::oobj read(GenericReader& reader) = 0;

    // If the source must return more than one Frame object, the
    // first one shall be returned by the `read()` method above,
    // whereas retrieving all subsequent Frames will require
    // calling this function.
    // virtual std::unique_ptr<Source> continuation();
};



//------------------------------------------------------------------------------
// Implementations
//------------------------------------------------------------------------------

// temporary
class Source_Python : public Source
{
  private:
    py::oobj src_;

  public:
    Source_Python(const std::string& name, py::oobj src);
    // py::oobj read(GenericReader&) override;
};


// temporary
class Source_Result : public Source
{
  private:
    py::oobj result_;

  public:
    Source_Result(const std::string& name, py::oobj res);
    // py::oobj read(GenericReader&) override;
};



class Source_Text : public Source
{
  private:
    const py::oobj src_;

  public:
    Source_Text(const py::robj textsrc)
      : Source("<text>"), src_(textsrc) {}
    // py::oobj read(GenericReader&) override;
};



class Source_Url : public Source
{
  private:
    std::string url_;

  public:
    Source_Url(const std::string& url);
    // py::oobj read(GenericReader&) override;
};
#endif



}}  // namespace dt::read
#endif
