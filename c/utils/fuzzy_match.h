//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_UTILS_FUZZY_MATCH_h
#define dt_UTILS_FUZZY_MATCH_h
#include <string>   // std::string
#include <vector>   // std::vector
namespace dt {


/**
 * Compute Levenshtein distance between two strings `a` and `b`, as described in
 * https://en.wikipedia.org/wiki/Levenshtein_distance
 *
 * Use iterative algorithm, single-row version. The temporary storage required
 * for the calculations is passed in array `v`, which must be allocated for at
 * least `min(a.size(), b.size()) + 1` elements.
 */
double levenshtein_distance(const std::string& a, const std::string& b,
                            double* v);


std::string suggest_similar_strings(
    const std::vector<std::string>& candidates,
    const std::string& name);


}  // namespace dt
#endif
