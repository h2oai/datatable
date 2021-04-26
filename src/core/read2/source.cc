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
#include "read2/source.h"
#include "utils/assert.h"
namespace dt {
namespace read2 {


//------------------------------------------------------------------------------
// SourceIterable
//------------------------------------------------------------------------------

SourceIterable::SourceIterable() noexcept
  : head_(nullptr),
    currentNode_(nullptr),
    insertionPoint_(nullptr) {}


void SourceIterable::add(UniqueSource&& source) {
  UniqueNode newNode(new Node(std::move(source)));
  if (insertionPoint_) {
    newNode->next = std::move(insertionPoint_->next);
    insertionPoint_->next = std::move(newNode);
    insertionPoint_ = insertionPoint_->next.get();
  } else {
    xassert(!head_);
    head_ = std::move(newNode);
    insertionPoint_ = head_.get();
  }
}


void SourceIterable::add(SourceIterable&& sources) {
  UniqueNode first = std::move(sources.head_);
  sources.currentNode_ = nullptr;
  sources.insertionPoint_ = nullptr;
  if (!first) return;
  Node* last = first.get();
  while (last->next) {
    last = last->next.get();
  }
  xassert(!last->next);
  if (insertionPoint_) {
    last->next = std::move(insertionPoint_->next);
    insertionPoint_->next = std::move(first);
  } else {
    xassert(!head_);
    head_ = std::move(first);
  }
  insertionPoint_ = last;
}


Source* SourceIterable::next() {
  if (currentNode_) {
    if (!currentNode_->source->keepReading()) {
      currentNode_ = currentNode_->next.get();
    }
  } else {
    currentNode_ = head_->get();
  }
  if (currentNode_) {
    insertionPoint_ = currentNode_;
    return currentNode_->source.get();
  } else {
    return nullptr;
  }
}




//------------------------------------------------------------------------------
// SourceImpl_Multi
//------------------------------------------------------------------------------

void SourceImpl_Multi::add(Source&& source) {
  sources_.emplace_back(std::move(source));
}




}}
