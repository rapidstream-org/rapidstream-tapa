#include <ap_int.h>
#include <hls_stream.h>

/* Data Type */
typedef ap_uint<512> A_t16;
typedef ap_uint<256> A_t8;
typedef ap_uint<512> B_t16;
typedef ap_uint<256> B_t8;
typedef ap_uint<512> C_t16;
typedef ap_uint<64> C_t2;
/* Data Type */

/* Helper Function */
inline void host_serialize_A(float *A_to, float *A_from){
    /* Variable Declaration */
    unsigned int cnt = 0;
    /* Variable Declaration */

    // array
    // io_L3
    for (int c3 = 0; c3 <= 12; c3 += 1) {
      // io_L2
      for (int c4 = 0; c4 <= 15; c4 += 1)
        for (int c5 = 0; c5 <= 255; c5 += 1)
          A_to[cnt++] = A_from[(16 * c3 + c4) * 256 + c5];
    }
}
/* Helper Function */

/* Helper Function */
inline void host_serialize_B(float *B_to, float *B_from){
    /* Variable Declaration */
    unsigned int cnt = 0;
    /* Variable Declaration */

    // array
    // io_L3
    for (int c3 = 0; c3 <= 15; c3 += 1) {
      // io_L2
      for (int c4 = 0; c4 <= 63; c4 += 1)
        for (int c5 = 0; c5 <= 255; c5 += 1)
          B_to[cnt++] = B_from[(64 * c3 + c4) * 256 + c5];
    }
}
/* Helper Function */

/* Helper Function */
inline void host_deserialize_C(float *C_to, float *C_from){
    /* Variable Declaration */
    unsigned int cnt = 0;
    /* Variable Declaration */

    // array
    // io_L3
    for (int c3 = 0; c3 <= 15; c3 += 1) {
      // io_L2
      for (int c4 = 0; c4 <= 12; c4 += 1) {
        // io_L1
        // pe
        for (int c5 = 0; c5 <= 15; c5 += 1)
          for (int c6 = 0; c6 <= 63; c6 += 1)
            C_to[(16 * c4 + c5) * 1024 + (64 * c3 + c6)] = C_from[cnt++];
      }
    }
}
/* Helper Function */

extern "C" { void kernel3(A_t16 *A, B_t16 *B, C_t16 *C); }
void A_IO_L2_in_intra_trans(int idx, A_t8 local_A[16][32], hls::stream<A_t8> &fifo_A_local_out, bool intra_trans_en);
void A_IO_L2_in_inter_trans(int idx, A_t8 local_A[16][32], hls::stream<A_t8> &fifo_A_in, hls::stream<A_t8> &fifo_A_out, bool inter_trans_en);
void A_IO_L2_in_inter_trans_boundary(int idx, A_t8 local_A[16][32], hls::stream<A_t8> &fifo_A_in, bool inter_trans_en);
void B_IO_L2_in_intra_trans(int idx, B_t8 local_B[64][32], hls::stream<B_t8> &fifo_B_local_out, bool intra_trans_en);
void B_IO_L2_in_inter_trans(int idx, B_t8 local_B[64][32], hls::stream<B_t8> &fifo_B_in, hls::stream<B_t8> &fifo_B_out, bool inter_trans_en);
void B_IO_L2_in_inter_trans_boundary(int idx, B_t8 local_B[64][32], hls::stream<B_t8> &fifo_B_in, bool inter_trans_en);
void PE_wrapper(int idx, int idy, hls::stream<A_t8> &fifo_A_in, hls::stream<A_t8> &fifo_A_out, hls::stream<B_t8> &fifo_B_in, hls::stream<B_t8> &fifo_B_out, hls::stream<float> &fifo_C_drain_out);
void C_drain_IO_L1_out_intra_trans(int idx, int idy, C_t2 local_C[16][32], hls::stream<float> &fifo_C_drain_local_in);
void C_drain_IO_L1_out_inter_trans(int idx, int idy, C_t2 local_C[16][32], hls::stream<C_t2> &fifo_C_drain_in, hls::stream<C_t2> &fifo_C_drain_out);
void C_drain_IO_L1_out_inter_trans_boundary(int idx, int idy, C_t2 local_C[16][32], hls::stream<C_t2> &fifo_C_drain_out);
