//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include "expr/outputs.h"
namespace dt {
namespace expr {


Outputs::Output::Output(Column&& col, std::string&& nm, size_t g)
  : column(std::move(col)),
    name(std::move(nm)),
    grouping_level(g) {}



//------------------------------------------------------------------------------
// Outputs
//------------------------------------------------------------------------------

size_t Outputs::size() const noexcept {
  return items.size();
}


Outputs& Outputs::add(Column&& col, std::string&& name, size_t group_level) {
  items.emplace_back(std::move(col), std::move(name), group_level);
  return *this;
}

Outputs& Outputs::add(Column&& col, size_t group_level) {
  items.emplace_back(std::move(col), std::string(), group_level);
  return *this;
}

Outputs& Outputs::add(Column&& col) {
  constexpr size_t g = Outputs::GroupToAll;
  items.emplace_back(std::move(col), std::string(), g);
  return *this;
}


void Outputs::append(Outputs&& other) {
  if (items.size() == 0) {
    items = std::move(other.items);
  }
  else {
    items.reserve(size() + other.size());
    for (auto& item : other.items) {
      items.emplace_back(std::move(item));
    }
  }
}


void Outputs::apply_name(const std::string& name) {
  if (items.size() == 1) {
    items[0].name = name;
  }
  else {
    for (auto& item : items) {
      if (item.name.empty()) {
        item.name = name;
      }
      else {
        // Note: name.c_str() returns pointer to a null-terminated character
        // array with the same data as `name`. The length of that array is
        // the length of `name` + 1 (for trailing NUL). We insert the entire
        // array into `item.name`, together with the NUL character, and then
        // overwrite the NUL with a '.'. The end result is that we have a
        // string with content "{name}.{item.name}".
        item.name.insert(0, name.c_str(), name.size() + 1);
        item.name[name.size()] = '.';
      }
    }
  }
}


strvec Outputs::release_names() {
  strvec out;
  out.reserve(items.size());
  for (auto& item : items) {
    out.push_back(std::move(item.name));
  }
  return out;
}


colvec Outputs::release_columns() {
  colvec out;
  out.reserve(items.size());
  for (auto& item : items) {
    out.push_back(std::move(item.column));
  }
  return out;
}




}}  // namespace dt::expr
