//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_FREAD_PARALLEL_READER_h
#define dt_READ_FREAD_PARALLEL_READER_h
#include <memory>
#include "read/parallel_reader.h"
#include "read/thread_context.h"

class FreadReader;
struct FreadTokenizer;
enum PT : uint8_t;

namespace dt {
namespace read {



class FreadParallelReader : public ParallelReader {

  private:
    FreadReader& f;
    PT* types;

  public:
    FreadParallelReader(FreadReader& reader, PT* types_);
    virtual ~FreadParallelReader() override = default;

    virtual void read_all() override;

  protected:
    virtual std::unique_ptr<ThreadContext> init_thread_context() override;

    virtual void adjust_chunk_coordinates(
      ChunkCoordinates& cc, ThreadContextPtr& ctx) const override;
};


}  // namespace read
}  // namespace dt

#endif
