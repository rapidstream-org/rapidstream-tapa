#include <hls_stream.h>
#include "math.h"
#include "DILATE.h"
#include <tapa.h>

template<class T>
T HLS_REG(T in){
#pragma HLS pipeline
#pragma HLS inline off
#pragma HLS interface port=return register
    return in;
}

static float DILATE_stencil_kernel(float s_1_2, float s_3_2, float s_1_3, 
    float s_3_3, float s_0_2, float s_3_1, float s_2_1, float s_2_0, 
    float s_2_4, float s_2_3, float s_2_2, float s_4_2, float s_1_1)
{
    /*
         ((t10 > t11)?t10:t11)
    */
    const float t1 =  ((s_0_2 > s_1_1)?s_0_2:s_1_1);
    const float t2 =  ((s_1_2 > s_1_3)?s_1_2:s_1_3);
    const float t3 =  ((s_2_0 > s_2_1)?s_2_0:s_2_1);
    const float t4 =  ((s_2_2 > s_2_3)?s_2_2:s_2_3);
    const float t5 =  ((s_2_4 > s_3_1)?s_2_4:s_3_1);
    const float t6 =  ((s_3_2 > s_3_3)?s_3_2:s_3_3);
    const float t7 =  ((t1 > t2)?t1:t2);
    const float t8 =  ((t3 > t4)?t3:t4);
    const float t9 =  ((t5 > t6)?t5:t6);
    const float t10 =  ((t7 > t8)?t7:t8);
    const float t11 =  ((t9 > s_4_2)?t9:s_4_2);
    return  ((t10 > t11)?t10:t11);
} // stencil kernel definition

void DILATE(tapa::istream<INTERFACE_WIDTH>& s, tapa::ostream<INTERFACE_WIDTH>& y,// int useless, 
    int iters)
{
    INTERFACE_WIDTH s_block_0;
    INTERFACE_WIDTH s_block_1;
    hls::stream<INTERFACE_WIDTH, 255> s_stream_2_to_255;
    INTERFACE_WIDTH s_block_256;
    INTERFACE_WIDTH s_block_257;
    hls::stream<INTERFACE_WIDTH, 255> s_stream_258_to_511;
    INTERFACE_WIDTH s_block_512;
    INTERFACE_WIDTH s_block_513;
    hls::stream<INTERFACE_WIDTH, 255> s_stream_514_to_767;
    INTERFACE_WIDTH s_block_768;
    INTERFACE_WIDTH s_block_769;
    hls::stream<INTERFACE_WIDTH, 255> s_stream_770_to_1023;
    INTERFACE_WIDTH s_block_1024;
    INTERFACE_WIDTH s_block_1025;

    s_block_0 = s.read();
    s_block_1 = s.read();
    for (int i = 0 + 2; i < 0 + 256; i++) {
        s_stream_2_to_255 << s.read();
    }
    s_block_256 = s.read();
    s_block_257 = s.read();
    for (int i = 0 + 258; i < 0 + 512; i++) {
        s_stream_258_to_511 << s.read();
    }
    s_block_512 = s.read();
    s_block_513 = s.read();
    for (int i = 0 + 514; i < 0 + 768; i++) {
        s_stream_514_to_767 << s.read();
    }
    s_block_768 = s.read();
    s_block_769 = s.read();
    for (int i = 0 + 770; i < 0 + 1024; i++) {
        s_stream_770_to_1023 << s.read();
    }
    s_block_1024 = s.read();
    s_block_1025 = s.read();

    MAJOR_LOOP:
    for (int i = 0; i < GRID_COLS/WIDTH_FACTOR*PART_ROWS + (TOP_APPEND+BOTTOM_APPEND)*(iters-1); i++) {
        #pragma HLS pipeline II=1
        INTERFACE_WIDTH out_temp;
        COMPUTE_LOOP:
        for (int k = 0; k < PARA_FACTOR; k++) {
            #pragma HLS unroll
            float s_1_2[PARA_FACTOR], s_3_2[PARA_FACTOR], s_1_3[PARA_FACTOR], s_3_3[PARA_FACTOR], s_0_2[PARA_FACTOR], s_3_1[PARA_FACTOR], s_2_1[PARA_FACTOR], s_2_0[PARA_FACTOR], s_2_4[PARA_FACTOR], s_2_3[PARA_FACTOR], s_2_2[PARA_FACTOR], s_4_2[PARA_FACTOR], s_1_1[PARA_FACTOR];
            #pragma HLS array_partition variable=s_1_2 complete dim=0
            #pragma HLS array_partition variable=s_3_2 complete dim=0
            #pragma HLS array_partition variable=s_1_3 complete dim=0
            #pragma HLS array_partition variable=s_3_3 complete dim=0
            #pragma HLS array_partition variable=s_0_2 complete dim=0
            #pragma HLS array_partition variable=s_3_1 complete dim=0
            #pragma HLS array_partition variable=s_2_1 complete dim=0
            #pragma HLS array_partition variable=s_2_0 complete dim=0
            #pragma HLS array_partition variable=s_2_4 complete dim=0
            #pragma HLS array_partition variable=s_2_3 complete dim=0
            #pragma HLS array_partition variable=s_2_2 complete dim=0
            #pragma HLS array_partition variable=s_4_2 complete dim=0
            #pragma HLS array_partition variable=s_1_1 complete dim=0

            unsigned int idx_k = k << 5;

            uint32_t temp_s_1_2 = (k<14)?s_block_256.range(idx_k + 95, idx_k + 64) : s_block_257.range(idx_k + -417, idx_k + -448);
            s_1_2[k] = *((float*)(&temp_s_1_2));
            uint32_t temp_s_3_2 = (k<14)?s_block_768.range(idx_k + 95, idx_k + 64) : s_block_769.range(idx_k + -417, idx_k + -448);
            s_3_2[k] = *((float*)(&temp_s_3_2));
            uint32_t temp_s_1_3 = (k<13)?s_block_256.range(idx_k + 127, idx_k + 96) : s_block_257.range(idx_k + -385, idx_k + -416);
            s_1_3[k] = *((float*)(&temp_s_1_3));
            uint32_t temp_s_3_3 = (k<13)?s_block_768.range(idx_k + 127, idx_k + 96) : s_block_769.range(idx_k + -385, idx_k + -416);
            s_3_3[k] = *((float*)(&temp_s_3_3));
            uint32_t temp_s_0_2 = (k<14)?s_block_0.range(idx_k + 95, idx_k + 64) : s_block_1.range(idx_k + -417, idx_k + -448);
            s_0_2[k] = *((float*)(&temp_s_0_2));
            uint32_t temp_s_3_1 = (k<15)?s_block_768.range(idx_k + 63, idx_k + 32) : s_block_769.range(idx_k + -449, idx_k + -480);
            s_3_1[k] = *((float*)(&temp_s_3_1));
            uint32_t temp_s_2_1 = (k<15)?s_block_512.range(idx_k + 63, idx_k + 32) : s_block_513.range(idx_k + -449, idx_k + -480);
            s_2_1[k] = *((float*)(&temp_s_2_1));
            uint32_t temp_s_2_0 = s_block_512.range(idx_k+31, idx_k);
            s_2_0[k] = *((float*)(&temp_s_2_0));
            uint32_t temp_s_2_4 = (k<12)?s_block_512.range(idx_k + 159, idx_k + 128) : s_block_513.range(idx_k + -353, idx_k + -384);
            s_2_4[k] = *((float*)(&temp_s_2_4));
            uint32_t temp_s_2_3 = (k<13)?s_block_512.range(idx_k + 127, idx_k + 96) : s_block_513.range(idx_k + -385, idx_k + -416);
            s_2_3[k] = *((float*)(&temp_s_2_3));
            uint32_t temp_s_2_2 = (k<14)?s_block_512.range(idx_k + 95, idx_k + 64) : s_block_513.range(idx_k + -417, idx_k + -448);
            s_2_2[k] = *((float*)(&temp_s_2_2));
            uint32_t temp_s_4_2 = (k<14)?s_block_1024.range(idx_k + 95, idx_k + 64) : s_block_1025.range(idx_k + -417, idx_k + -448);
            s_4_2[k] = *((float*)(&temp_s_4_2));
            uint32_t temp_s_1_1 = (k<15)?s_block_256.range(idx_k + 63, idx_k + 32) : s_block_257.range(idx_k + -449, idx_k + -480);
            s_1_1[k] = *((float*)(&temp_s_1_1));

            float result = DILATE_stencil_kernel(s_1_2[k], s_3_2[k], s_1_3[k], s_3_3[k], s_0_2[k], s_3_1[k], s_2_1[k], s_2_0[k], s_2_4[k], s_2_3[k], s_2_2[k], s_4_2[k], s_1_1[k]);
            out_temp.range(idx_k+31, idx_k) = *((uint32_t *)(&result));
        }
        y.write(out_temp);

        s_block_0 = HLS_REG(s_block_1);
        s_block_1 = s_stream_2_to_255.read();
        s_stream_2_to_255 << HLS_REG(s_block_256);
        s_block_256 = HLS_REG(s_block_257);
        s_block_257 = s_stream_258_to_511.read();
        s_stream_258_to_511 << HLS_REG(s_block_512);
        s_block_512 = HLS_REG(s_block_513);
        s_block_513 = s_stream_514_to_767.read();
        s_stream_514_to_767 << HLS_REG(s_block_768);
        s_block_768 = HLS_REG(s_block_769);
        s_block_769 = s_stream_770_to_1023.read();
        s_stream_770_to_1023 << HLS_REG(s_block_1024);
        s_block_1024 = HLS_REG(s_block_1025);

        unsigned int idx_s = 0 + (i + 1026);
        s_block_1025 = HLS_REG(s.read());
    }

    INTERFACE_WIDTH popout_s_stream_2_to_255;
    while (!s_stream_2_to_255.empty()) {
        #pragma HLS pipeline II=1
        s_stream_2_to_255 >> popout_s_stream_2_to_255;
    }
    INTERFACE_WIDTH popout_s_stream_258_to_511;
    while (!s_stream_258_to_511.empty()) {
        #pragma HLS pipeline II=1
        s_stream_258_to_511 >> popout_s_stream_258_to_511;
    }
    INTERFACE_WIDTH popout_s_stream_514_to_767;
    while (!s_stream_514_to_767.empty()) {
        #pragma HLS pipeline II=1
        s_stream_514_to_767 >> popout_s_stream_514_to_767;
    }
    INTERFACE_WIDTH popout_s_stream_770_to_1023;
    while (!s_stream_770_to_1023.empty()) {
        #pragma HLS pipeline II=1
        s_stream_770_to_1023 >> popout_s_stream_770_to_1023;
    }
    return;
} // stencil kernel definition

void load(tapa::async_mmap<INTERFACE_WIDTH>& a, tapa::async_mmap<INTERFACE_WIDTH>& b,
                tapa::ostream<INTERFACE_WIDTH> &stream_out, tapa::istream<INTERFACE_WIDTH> &stream_in,
                uint32_t iters) {
  #pragma HLS inline off
  // unsigned int loop_bound = GRID_COLS/WIDTH_FACTOR*PART_ROWS + (TOP_APPEND+BOTTOM_APPEND)*(iters-1) + 65 + 66;
  unsigned int loop_bound = GRID_COLS/WIDTH_FACTOR*PART_ROWS + (TOP_APPEND+BOTTOM_APPEND)*(iters-1) + 1026;

  for(int k_wr_req = (1026), k_wr_resp = (1026), k_rd_req = 0, k_rd_resp = 0; k_rd_resp < loop_bound || k_wr_resp < loop_bound; ) {
    // read from a
    if (k_rd_req < loop_bound && a.read_addr.try_write(k_rd_req)) {
      k_rd_req++;
    }
    if (k_rd_resp < loop_bound && !a.read_data.empty() && !stream_out.full()) {
      INTERFACE_WIDTH temp = a.read_data.read(nullptr);
      stream_out.write(temp);
      k_rd_resp++;
    }

    // write to b
    if (k_wr_req < loop_bound && !b.write_addr.full() && !b.write_data.full() && !stream_in.empty()) {
      b.write_addr.write(k_wr_req);
      b.write_data.write(stream_in.read());
      k_wr_req++;
    }
    if (!b.write_resp.empty()) {
      k_wr_resp += (unsigned int)(b.write_resp.read()) + 1;
    }
  }
}

//Block reading
void inter_kernel(tapa::async_mmap<INTERFACE_WIDTH>& a, tapa::async_mmap<INTERFACE_WIDTH>& b,
                tapa::ostream<INTERFACE_WIDTH> &stream_out, tapa::istream<INTERFACE_WIDTH> &stream_in,
                uint32_t iters){

  for(int i = 0; i < iters; i+=STAGE_COUNT){
    if(i%(2*STAGE_COUNT)==0){
      load(a, b, stream_out, stream_in, iters);
    }
    else{
      load(b, a, stream_out, stream_in, iters);
    }
  }
}

void unikernel(tapa::mmap<INTERFACE_WIDTH> in_0, tapa::mmap<INTERFACE_WIDTH> out_0, //HBM 0 1
               tapa::mmap<INTERFACE_WIDTH> in_1, tapa::mmap<INTERFACE_WIDTH> out_1,
               tapa::mmap<INTERFACE_WIDTH> in_2, tapa::mmap<INTERFACE_WIDTH> out_2,
               tapa::mmap<INTERFACE_WIDTH> in_3, tapa::mmap<INTERFACE_WIDTH> out_3,
               tapa::mmap<INTERFACE_WIDTH> in_4, tapa::mmap<INTERFACE_WIDTH> out_4,
               tapa::mmap<INTERFACE_WIDTH> in_5, tapa::mmap<INTERFACE_WIDTH> out_5,
               tapa::mmap<INTERFACE_WIDTH> in_6, tapa::mmap<INTERFACE_WIDTH> out_6,
               tapa::mmap<INTERFACE_WIDTH> in_7, tapa::mmap<INTERFACE_WIDTH> out_7,
               tapa::mmap<INTERFACE_WIDTH> in_8, tapa::mmap<INTERFACE_WIDTH> out_8,
               tapa::mmap<INTERFACE_WIDTH> in_9, tapa::mmap<INTERFACE_WIDTH> out_9,
               tapa::mmap<INTERFACE_WIDTH> in_10, tapa::mmap<INTERFACE_WIDTH> out_10,
               tapa::mmap<INTERFACE_WIDTH> in_11, tapa::mmap<INTERFACE_WIDTH> out_11,
               tapa::mmap<INTERFACE_WIDTH> in_12, tapa::mmap<INTERFACE_WIDTH> out_12,
               tapa::mmap<INTERFACE_WIDTH> in_13, tapa::mmap<INTERFACE_WIDTH> out_13,
               tapa::mmap<INTERFACE_WIDTH> in_14, tapa::mmap<INTERFACE_WIDTH> out_14,
                uint32_t iters){
  tapa::streams<INTERFACE_WIDTH, 15, 3> k_wr;
  tapa::streams<INTERFACE_WIDTH, 15, 3> k_rd;

  tapa::task()
    .invoke(inter_kernel, in_0, out_0, k_rd[0], k_wr[0], iters)
    .invoke(DILATE, k_rd[0], k_wr[0], iters)
    .invoke(inter_kernel, in_1, out_1, k_rd[1], k_wr[1], iters)
    .invoke(DILATE, k_rd[1], k_wr[1], iters)
    .invoke(inter_kernel, in_2, out_2, k_rd[2], k_wr[2], iters)
    .invoke(DILATE, k_rd[2], k_wr[2], iters)
    .invoke(inter_kernel, in_3, out_3, k_rd[3], k_wr[3], iters)
    .invoke(DILATE, k_rd[3], k_wr[3], iters)
    .invoke(inter_kernel, in_4, out_4, k_rd[4], k_wr[4], iters)
    .invoke(DILATE, k_rd[4], k_wr[4], iters)
    .invoke(inter_kernel, in_5, out_5, k_rd[5], k_wr[5], iters)
    .invoke(DILATE, k_rd[5], k_wr[5], iters)
    .invoke(inter_kernel, in_6, out_6, k_rd[6], k_wr[6], iters)
    .invoke(DILATE, k_rd[6], k_wr[6], iters)
    .invoke(inter_kernel, in_7, out_7, k_rd[7], k_wr[7], iters)
    .invoke(DILATE, k_rd[7], k_wr[7], iters)
    .invoke(inter_kernel, in_8, out_8, k_rd[8], k_wr[8], iters)
    .invoke(DILATE, k_rd[8], k_wr[8], iters)
    .invoke(inter_kernel, in_9, out_9, k_rd[9], k_wr[9], iters)
    .invoke(DILATE, k_rd[9], k_wr[9], iters)
    .invoke(inter_kernel, in_10, out_10, k_rd[10], k_wr[10], iters)
    .invoke(DILATE, k_rd[10], k_wr[10], iters)
    .invoke(inter_kernel, in_11, out_11, k_rd[11], k_wr[11], iters)
    .invoke(DILATE, k_rd[11], k_wr[11], iters)
    .invoke(inter_kernel, in_12, out_12, k_rd[12], k_wr[12], iters)
    .invoke(DILATE, k_rd[12], k_wr[12], iters)
    .invoke(inter_kernel, in_13, out_13, k_rd[13], k_wr[13], iters)
    .invoke(DILATE, k_rd[13], k_wr[13], iters)
    .invoke(inter_kernel, in_14, out_14, k_rd[14], k_wr[14], iters)
    .invoke(DILATE, k_rd[14], k_wr[14], iters)
    ;
}
