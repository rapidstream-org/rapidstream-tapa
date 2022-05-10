#ifndef SEEPENS_H
#define SEEPENS_H

#include <ap_int.h>
#include <tapa.h>

constexpr int NUM_CH_SPARSE = 16;
//constexpr int NUM_CH_SPARSE = 24;

constexpr int WINDOW_SIZE = 8192;
constexpr int DEP_DIST_LOAD_STORE = 10;
constexpr int X_PARTITION_FACTOR = 8;
constexpr int URAM_DEPTH = (48 / NUM_CH_SPARSE) * 4096; // 16 -> 12,288, 24 -> 8,192

using float_v16 = tapa::vec_t<float, 16>;

void Serpens(tapa::mmap<int> edge_list_ptr,
             tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,
             tapa::mmap<float_v16> vec_X,
             tapa::mmap<float_v16> vec_Y,
             tapa::mmap<float_v16> vec_Y_out,
             const int NUM_ITE, const int NUM_A_LEN, const int M, const int K,
             const int P_N, const int alpha_u, const int beta_u);

#endif  // SEEPENS_H
