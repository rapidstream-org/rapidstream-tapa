#include <ap_int.h>
#include <hls_stream.h>

#define min(x,y) ((x < y) ? x : y)
#define max(x,y) ((x > y) ? x : y)

/* Data Type */
typedef float prev_V_t1;
typedef float V_t1;
typedef float U_tmp_t1;
typedef float L_tmp_t1;
typedef float A_t1;
typedef float L_t1;
typedef float U_t1;
typedef ap_uint<512> U_tmp_t16;
typedef ap_uint<512> U_t16;
typedef ap_uint<128> U_t4;
/* Data Type */

void kernel0(A_t1 *A, L_t1 *L, U_t16 *U);
template<int p0, int p1>
void A_IO_L1_in_intra_trans(float local_A[32][1], hls::stream<float> &fifo_A_local_out);
template<int p0, int p1>
void A_IO_L1_in_inter_trans(float local_A[32][1], hls::stream<float> &fifo_A_in, hls::stream<float> &fifo_A_out);
template<int p0, int p1>
void A_IO_L1_in_inter_trans_boundary(float local_A[32][1], hls::stream<float> &fifo_A_in);
template<int p0, int p1>
void A_IO_L1_in_wrapper(hls::stream<float> &fifo_A_in, hls::stream<float> &fifo_A_out, hls::stream<float> &fifo_A_local_out);
template<int p0, int p1>
void A_IO_L1_in_boundary_wrapper(hls::stream<float> &fifo_A_in, hls::stream<float> &fifo_A_local_out);
template<int p0, int p1>
void PE_wrapper(hls::stream<float> &fifo_V_in, hls::stream<float> &fifo_V_out, hls::stream<float> &fifo_U_tmp_1_in, hls::stream<float> &fifo_U_tmp_1_out, hls::stream<float> &fifo_A_in, hls::stream<float> &fifo_L_drain_out, hls::stream<float> &fifo_U_drain_out);
template<int p0, int p1>
void L_drain_IO_L1_out_intra_trans(float local_L[1][1], hls::stream<float> &fifo_L_drain_local_in);
template<int p0, int p1>
void L_drain_IO_L1_out_inter_trans(float local_L[1][1], hls::stream<float> &fifo_L_drain_in, hls::stream<float> &fifo_L_drain_out);
template<int p0, int p1>
void L_drain_IO_L1_out_inter_trans_boundary(float local_L[1][1], hls::stream<float> &fifo_L_drain_out);
template<int p0, int p1>
void L_drain_IO_L1_out_wrapper(hls::stream<float> &fifo_L_drain_in, hls::stream<float> &fifo_L_drain_out, hls::stream<float> &fifo_L_drain_local_in);
template<int p0, int p1>
void L_drain_IO_L1_out_boundary_wrapper(hls::stream<float> &fifo_L_drain_out, hls::stream<float> &fifo_L_drain_local_in);
template<int p0, int p1>
void U_drain_IO_L1_out_intra_trans(U_t4 local_U[1][8], hls::stream<float> &fifo_U_drain_local_in);
template<int p0, int p1>
void U_drain_IO_L1_out_inter_trans(U_t4 local_U[1][8], hls::stream<U_t4> &fifo_U_drain_in, hls::stream<U_t4> &fifo_U_drain_out);
template<int p0, int p1>
void U_drain_IO_L1_out_inter_trans_boundary(U_t4 local_U[1][8], hls::stream<U_t4> &fifo_U_drain_out);
template<int p0, int p1>
void U_drain_IO_L1_out_wrapper(hls::stream<U_t4> &fifo_U_drain_in, hls::stream<U_t4> &fifo_U_drain_out, hls::stream<float> &fifo_U_drain_local_in);
template<int p0, int p1>
void U_drain_IO_L1_out_boundary_wrapper(hls::stream<U_t4> &fifo_U_drain_out, hls::stream<float> &fifo_U_drain_local_in);
