#include "gemv.h"

#include <tapa.h>

void GemvCore(int pe_id, tapa::mmap<bits<DataVec>> a,
              tapa::mmap<bits<DataVec>> x, tapa::mmap<bits<DataVec>> y) {
  DataVec local_x[kMaxMatrixSize / kVecLen];
  DataVec local_y[kMaxMatrixSize / kVecLen / kPeCount];

  constexpr int kMatrixVecCount = kMatrixSize / kVecLen;
  constexpr int kMatrixVecCountPerPe = kMatrixVecCount / kPeCount;

  [[tapa::pipeline(1)]] for (int i = 0; i < kMatrixVecCount; ++i) {
    local_x[i] = tapa::bit_cast<DataVec>(x[i]);
  }
  [[tapa::pipeline(1)]] for (int i = 0; i < kMatrixVecCountPerPe; ++i) {
    local_y[i] = 0;
  }

  for (int i = 0; i < kMatrixSize; ++i) {
    [[tapa::pipeline(1)]] for (int j = 0; j < kMatrixVecCountPerPe; ++j) {
#pragma HLS dependence variable = local_y type = inter false
      DataVec local_a = tapa::bit_cast<DataVec>(
          a[kMatrixVecCount * i + kMatrixVecCountPerPe * pe_id + j]);
      local_y[j] += local_a * local_x[i / kVecLen][i % kVecLen];
    }
  }

  [[tapa::pipeline(1)]] for (int i = 0; i < kMatrixVecCountPerPe; ++i) {
    y[pe_id * kMatrixVecCountPerPe + i] =
        tapa::bit_cast<bits<DataVec>>(local_y[i]);
  }
}

void Gemv(tapa::hmap<bits<DataVec>, kPcCount, kPcSize> mat_a,
          tapa::mmap<bits<DataVec>> vec_x, tapa::mmap<bits<DataVec>> vec_y) {
  tapa::task().invoke<tapa::join, kPeCount>(GemvCore, tapa::seq(), mat_a, vec_x,
                                            vec_y);
}
