//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include <algorithm>   // std::min, std::swap
#include <iostream>    // std::ostringstream
#include <memory>      // std::unique_ptr
#include "python/string.h"
#include "utils/assert.h"
#include "utils/fuzzy_match.h"
#include "datatablemodule.h"
namespace dt {


//------------------------------------------------------------------------------
// Levenshtein distance calculation
//------------------------------------------------------------------------------

double levenshtein_distance(const std::string& a, const std::string& b,
                            double* v)
{
  const char* aa = a.data();
  const char* bb = b.data();
  int n = static_cast<int>(a.size());
  int m = static_cast<int>(b.size());
  if (n > m) {
    std::swap(aa, bb);
    std::swap(m, n);
  }
  // Remove common prefix from the strings
  while (n && *aa == *bb) {
    n--; m--;
    aa++; bb++;
  }
  // Remove common suffix from the strings
  while (n && aa[n - 1] == bb[m - 1]) {
    n--; m--;
  }
  if (n == 0) return m;
  xassert(0 < n && n <= m);
  // Compute the Levenshtein distance
  aa--;  // Shift pointers, so that we can use 1-based indexing below
  bb--;
  for (int j = 1; j <= n; ++j) v[j] = j;
  for (int i = 1; i <= m; ++i) {
    double w = i - 1;
    v[0] = i;
    for (int j = 1; j <= n; ++j) {
      char ach = aa[j];
      char bch = bb[i];
      double c = 0.0;
      if (ach != bch) {
        // Use non-trivial cost function to compare character substitution:
        //   * the cost is lowest when 2 characters differ by case only,
        //     or if both are "space-like" (i.e. ' ', '_' or '.')
        //   * medium cost for substituting letters with letters, or digits
        //     with digits
        //   * highest cost for all other substitutions.
        //
        bool a_lower = 'a' <= ach && ach <= 'z';
        bool a_upper = 'A' <= ach && ach <= 'Z';
        bool a_digit = '0' <= ach && ach <= '9';
        bool a_space = ach == ' ' || ach == '_' || ach == '.';
        bool b_lower = 'a' <= bch && bch <= 'z';
        bool b_upper = 'A' <= bch && bch <= 'Z';
        bool b_digit = '0' <= bch && bch <= '9';
        bool b_space = bch == ' ' || bch == '_' || bch == '.';
        c = (a_lower && ach == bch + ('a'-'A'))? 0.2 :
            (a_upper && bch == ach + ('a'-'A'))? 0.2 :
            (a_space && b_space)? 0.2 :
            (a_digit && b_digit)? 0.75 :
            ((a_lower|a_upper) && (b_lower|b_upper))? 0.75 : 1.0;
      }
      double del_cost = v[j] + 1;
      double ins_cost = v[j - 1] + 1;
      double sub_cost = w + c;
      w = v[j];
      v[j] = std::min(del_cost, std::min(ins_cost, sub_cost));
    }
  }
  return v[n];
}



std::string suggest_similar_strings(
    const std::vector<std::string>& candidates,
    const std::string& name)
{
  size_t len = name.size();
  auto tmp = std::unique_ptr<double[]>(new double[len + 1]);
  double maxdist = len <= 3? 1 :
                   len <= 6? 2 :
                   len <= 9? 3 :
                   len <= 16? 4 : 5;
  struct scored_column {
    size_t index;
    double score;
  };
  auto col0 = scored_column { 0, 100.0 };
  auto col1 = scored_column { 0, 100.0 };
  auto col2 = scored_column { 0, 100.0 };
  for (size_t i = 0; i < candidates.size(); ++i) {
    double dist = levenshtein_distance(name, candidates[i], tmp.get());
    if (dist <= maxdist) {
      auto curr = scored_column { i, dist };
      if (curr.score < col0.score) {
        col2 = col1; col1 = col0; col0 = curr;
      } else if (curr.score < col1.score) {
        col2 = col1; col1 = curr;
      } else if (curr.score < col2.score) {
        col2 = curr;
      }
    }
  }

  std::ostringstream out;
  if (col0.score < 10) {
    out << '`' << escape_backticks(candidates[col0.index]) << '`';
    if (col1.score < 10) {
      out << (col2.score < 10? ", " : " or ");
      out << '`' << escape_backticks(candidates[col1.index]) << '`';
      if (col2.score < 10) {
        out << " or `" << escape_backticks(candidates[col2.index]) << '`';
      }
    }
  }
  return out.str();
}


}  // namespace dt
namespace py {


static PKArgs args_fuzzy_match(
  2, 0, 0, false, false, {"candidates", "name"}, "fuzzy_match");

static oobj fuzzy_match(const PKArgs& args) {
  std::vector<std::string> candidates;
  for (auto item : args[0].to_oiter()) {
    candidates.push_back(item.to_string());
  }
  std::string name = args[1].to_string();
  return ostring(dt::suggest_similar_strings(candidates, name));
}


void DatatableModule::init_fuzzy() {
  ADD_FN(&fuzzy_match, args_fuzzy_match);
}


}  // namespace py
