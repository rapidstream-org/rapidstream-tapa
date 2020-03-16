#include <stdlib.h>

#include <algorithm>

#include <frt.h>
#include <tlp.h>

#include "page-rank.h"

void PageRank(
    Pid num_partitions, tlp::mmap<uint64_t> metadata,
    tlp::async_mmap<tlp::vec_t<VertexAttrAligned, kVertexVecLen>> vertices,
    tlp::async_mmaps<tlp::vec_t<Edge, kEdgeVecLen>, kNumPes> edges,
    tlp::async_mmaps<tlp::vec_t<Update, kUpdateVecLen>, kNumPes> updates) {
  auto instance = fpga::Instance(getenv("BITSTREAM"));
  auto metadata_arg = fpga::ReadWrite(metadata.get(), metadata.size());
  auto vertices_arg = fpga::ReadWrite(vertices.get(), vertices.size());
  instance.SetArg(0, num_partitions);
  instance.AllocBuf(1, metadata_arg);
  instance.SetArg(1, metadata_arg);
  instance.AllocBuf(2, vertices_arg);
  instance.SetArg(2, vertices_arg);
  for (int i = 0; i < kNumPes; ++i) {
    auto edges_arg = fpga::WriteOnly(edges[i].get(), edges[i].size());
    auto updates_arg = fpga::ReadOnly(updates[i].get(), updates[i].size());
    instance.AllocBuf(3 + i, edges_arg);
    instance.SetArg(3 + i, edges_arg);
    instance.AllocBuf(3 + kNumPes + i, updates_arg);
    instance.SetArg(3 + kNumPes + i, updates_arg);
  }

  instance.WriteToDevice();
  instance.Exec();
  instance.ReadFromDevice();
  instance.Finish();

  LOG(INFO) << "kernel time: " << instance.ComputeTimeSeconds() * 1e3 << " ms";
}
