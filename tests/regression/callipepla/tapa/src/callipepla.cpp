// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <ap_int.h>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <tapa.h>
#include "callipepla.h"

constexpr int FIFO_DEPTH = 2;
constexpr int FIFO_DEPTH_M6 = 50;

const int NUM_CH_SPARSE_div_8 = NUM_CH_SPARSE / 8;
const int NUM_CH_SPARSE_mult_8 = NUM_CH_SPARSE * 8;
const int WINDOW_SIZE_div_8 = WINDOW_SIZE / 8;

struct MultXVec {
  tapa::vec_t<ap_uint<18>, 8> row;
  double_v8 axv;
};

struct InstRdWr {
  bool rd;
  bool wr;
  // bool require_response;
  int base_addr;
  int len;
};

struct InstVCtrl {
  bool rd;
  bool wr;
  int base_addr;
  int len;
  ap_uint<3> q_rd_idx;
  // ap_uint<3> q_wr_idx;
};

struct InstCmp {
  int len;
  double alpha;
  ap_uint<3> q_idx;
};

struct ResTerm {
  double res;
  bool term;
};

template <typename T, typename R>
inline void async_read(tapa::async_mmap<T>& A, tapa::ostream<T>& fifo_A,
                       const R i_end_addr, R& i_req, R& i_resp) {
#pragma HLS inline
  if ((i_req < i_end_addr) & !A.read_addr.full()) {
    A.read_addr.try_write(i_req);
    ++i_req;
  }
  if (!fifo_A.full() & !A.read_data.empty()) {
    T tmp;
    A.read_data.try_read(tmp);
    fifo_A.try_write(tmp);
    ++i_resp;
  }
}

template <typename T, typename R>
inline void async_write(tapa::async_mmap<T>& Y_out, tapa::istream<T>& fifo_Y,
                        const R num_ite_Y, R& i_req, R& i_resp) {
#pragma HLS inline
  if ((i_req < num_ite_Y) & !fifo_Y.empty() & !Y_out.write_addr.full() &
      !Y_out.write_data.full()) {
    Y_out.write_addr.try_write(i_req);
    T tmpv;
    fifo_Y.try_read(tmpv);
    Y_out.write_data.try_write(tmpv);
    ++i_req;
  }
  uint8_t n_resp;
  if (Y_out.write_resp.try_read(n_resp)) {
    i_resp += R(n_resp) + 1;
  }
}

void rdwr_vec(tapa::async_mmap<double_v8>& vec_p,
              tapa::istream<InstRdWr>& q_inst, tapa::istream<double_v8>& q_din,
              tapa::ostream<double_v8>& q_dout,
              tapa::ostream<bool>& q_response) {
  for (;;) {
    auto inst = q_inst.read();

    const int rd_end_addr = inst.rd ? (inst.base_addr + inst.len) : 0;
    const int wr_end_addr = inst.wr ? (inst.base_addr + inst.len) : 0;

    const int rd_total = inst.rd ? inst.len : 0;
    const int wr_total = inst.wr ? inst.len : 0;

  rdwr:
    for (int rd_req = inst.base_addr, rd_resp = 0, wr_req = inst.base_addr,
             wr_resp = 0;
         (rd_resp < rd_total) | (wr_resp < wr_total);) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      // rd
      if ((rd_req < rd_end_addr) & !vec_p.read_addr.full()) {
        vec_p.read_addr.try_write(rd_req);
        ++rd_req;
      }
      if (!q_dout.full() & !vec_p.read_data.empty()) {
        double_v8 tmp;
        vec_p.read_data.try_read(tmp);
        q_dout.try_write(tmp);
        ++rd_resp;
      }

      // wr
      if ((wr_req < wr_end_addr) & !q_din.empty() & !vec_p.write_addr.full() &
          !vec_p.write_data.full()) {
        vec_p.write_addr.try_write(wr_req);
        double_v8 tmpv;
        q_din.try_read(tmpv);
        vec_p.write_data.try_write(tmpv);
        ++wr_req;
      }
      uint8_t n_resp;
      if (vec_p.write_resp.try_read(n_resp)) {
        wr_resp += int(n_resp) + 1;
      }
    }

    ap_wait();

    if (inst.wr) {
      q_response.write(true);
    }
  }
}

template <typename data_t>
inline void q2q(tapa::istream<data_t>& qin, tapa::ostream<data_t>& qout,
                const int num_ite) {
#pragma HLS inline
q:
  for (int i = 0; i < num_ite;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
    if (!qin.empty() & !qout.full()) {
      data_t tmp;
      qin.try_read(tmp);
      qout.try_write(tmp);
      ++i;
    }
  }
}

template <typename data_t>
inline void clearq(tapa::istream<data_t>& qin, const int num_ite) {
#pragma HLS inline
q:
  for (int i = 0; i < num_ite;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
    if (!qin.empty()) {
      data_t tmp;
      qin.try_read(tmp);
      ++i;
    }
  }
}

template <typename data_t>
inline void q2q(tapa::istreams<data_t, 2>& qin, tapa::ostream<data_t>& qout,
                const int num_ite, const int idx) {
#pragma HLS inline
q:
  for (int i = 0; i < num_ite;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
    if (!qin[idx].empty() & !qout.full()) {
      data_t tmp;
      qin[idx].try_read(tmp);
      qout.try_write(tmp);
      ++i;
    }
  }
}

template <typename data_t>
inline void qq2qq(tapa::istream<data_t>& qin_pe,
                  tapa::ostreams<data_t, 2>& qout_mem,
                  tapa::istreams<data_t, 2>& qin_mem,
                  tapa::ostream<data_t>& qout_qe, const int num_ite,
                  const int idx) {
#pragma HLS inline
qq:
  for (int i = 0; i < num_ite;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
    if (!qin_pe.empty() & !qout_mem[idx].full()) {
      data_t tmp;
      qin_pe.try_read(tmp);
      qout_mem[idx].try_write(tmp);
      ++i;
    }
    if (!qin_mem[1 - idx].empty() & !qout_qe.full()) {
      data_t tmp;
      qin_mem[1 - idx].try_read(tmp);
      qout_qe.try_write(tmp);
    }
  }
}

void term_signal_router(tapa::istream<bool>& q_gbc,
                        tapa::ostream<bool>& q_to_rdA,
                        tapa::ostream<bool>& q_to_edgepointer,
                        tapa::ostream<bool>& q_to_abiter,
                        tapa::ostream<bool>& q_to_ctrlmem,
                        tapa::ostream<bool>& q_to_mux) {
spin:
  for (;;) {
#pragma HLS pipeline II = 1
    if (!q_gbc.empty() & !q_to_rdA.full() & !q_to_edgepointer.full() &
        !q_to_abiter.full() & !q_to_ctrlmem.full() & !q_to_mux.full()) {
      bool tmp;
      q_gbc.try_read(tmp);
      q_to_rdA.try_write(tmp);
      q_to_edgepointer.try_write(tmp);
      q_to_abiter.try_write(tmp);
      q_to_ctrlmem.try_write(tmp);
      q_to_mux.try_write(tmp);
    }
  }
}

void read_edge_list_ptr(const int num_ite, const int M,
                        const int rp_time,  // P_N,
                        tapa::async_mmap<int>& edge_list_ptr,
                        tapa::ostream<int>& PE_inst, tapa::istream<bool>& q_gbc,
                        tapa::ostream<bool>& q_gbc_out) {
  // const int rp_time = (P_N == 0)? 1 : P_N;

  PE_inst.write(num_ite);
  PE_inst.write(M);
  PE_inst.write(rp_time);
  // PE_inst.write(K);

  const int num_ite_plus1 = num_ite + 1;
  bool term_flag = false;

l_rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_ptr:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_plus1;) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      async_read(edge_list_ptr, PE_inst, num_ite_plus1, i_req, i_resp);
    }

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "### exit read_edge_list_ptr" << endl;
}

void read_vec(const int rp_time,  // P_N
              const int M,        // K,
              tapa::async_mmap<double_v8>& vec_X,
              tapa::ostream<double_v8>& fifo_X) {
  // const int rp_time = (P_N == 0)? 1 : P_N;
  const int num_ite_X = (M + 7) >> 3;

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_X:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_X;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      async_read(vec_X, fifo_X, num_ite_X, i_req, i_resp);
    }
  }
}

void read_A(const int rp_time,  // P_N,
            const int A_len, tapa::async_mmap<ap_uint<512>>& A,
            tapa::ostream<ap_uint<512>>& fifo_A, tapa::istream<bool>& q_gbc,
            tapa::ostream<bool>& q_gbc_out) {
  // const int rp_time = (P_N == 0)? 1 : P_N;
  bool term_flag = false;
l_rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_A:
    for (int i_req = 0, i_resp = 0; i_resp < A_len;) {
#pragma HLS loop_tripcount min = 1 max = 10000
#pragma HLS pipeline II = 1
      async_read(A, fifo_A, A_len, i_req, i_resp);
    }

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "##### exit read_A\n";
}

void PEG_Xvec(tapa::istream<int>& fifo_inst_in,
              tapa::istream<ap_uint<512>>& fifo_A,
              tapa::istream<double_v8>& fifo_X_in,
              tapa::ostream<int>& fifo_inst_out,
              tapa::ostream<double_v8>& fifo_X_out,
              // to PEG_Yvec
              tapa::ostream<int>& fifo_inst_out_to_Yvec,
              tapa::ostream<MultXVec>& fifo_aXvec, tapa::istream<bool>& q_gbc,
              tapa::ostream<bool>& q_gbc_out,
              tapa::ostream<bool>& q_gbc_out_Y) {
  const int NUM_ITE = fifo_inst_in.read();
  const int M = fifo_inst_in.read();
  const int rp_time = fifo_inst_in.read();
  // const int K = fifo_inst_in.read();

  fifo_inst_out.write(NUM_ITE);
  fifo_inst_out.write(M);
  fifo_inst_out.write(rp_time);
  // fifo_inst_out.write(K);

  fifo_inst_out_to_Yvec.write(NUM_ITE);
  fifo_inst_out_to_Yvec.write(M);
  fifo_inst_out_to_Yvec.write(rp_time);

  bool term_flag = false;

l_rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    double local_X[4][WINDOW_SIZE];
#pragma HLS bind_storage variable = local_X latency = 1
#pragma HLS array_partition variable = local_X complete dim = 1
#pragma HLS array_partition variable = local_X cyclic factor = \
    X_PARTITION_FACTOR dim = 2

    auto start_32 = fifo_inst_in.read();
    fifo_inst_out.write(start_32);
    fifo_inst_out_to_Yvec.write(start_32);

  main:
    for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min = 1 max = 49

      // fill onchip X
    read_X:
      for (int j = 0; (j < WINDOW_SIZE_div_8) &
                      (j < ((M + 7) >> 3) - i * WINDOW_SIZE_div_8);) {
#pragma HLS loop_tripcount min = 1 max = 512
#pragma HLS pipeline II = 1
        if (!fifo_X_in.empty() & !fifo_X_out.full()) {
          double_v8 x;
          fifo_X_in.try_read(x);
          fifo_X_out.try_write(x);
          for (int kk = 0; kk < 8; ++kk) {  // 512 / 64 = 8
            for (int l = 0; l < 4; ++l) {
              local_X[l][(j << 3) + kk] = x[kk];  // 8 -> << 3
            }
          }
          ++j;
        }
      }

      // computation
      const auto end_32 = fifo_inst_in.read();
      fifo_inst_out.write(end_32);
      fifo_inst_out_to_Yvec.write(end_32);

    computation:
      for (int j = start_32; j < end_32;) {
#pragma HLS loop_tripcount min = 1 max = 200
#pragma HLS pipeline II = 1
        if (!fifo_A.empty()) {
          ap_uint<512> a_pes;
          fifo_A.try_read(a_pes);
          MultXVec raxv;

          for (int p = 0; p < 8; ++p) {
            ap_uint<64> a = a_pes(63 + p * 64, p * 64);
            ap_uint<14> a_col = a(63, 50);
            ap_uint<18> a_row = a(49, 32);
            ap_uint<32> a_val = a(31, 0);

            raxv.row[p] = a_row;
            if (a_row[17] == 0) {
              float a_val_f32 = tapa::bit_cast<float>(a_val);
              double a_val_f64 = (double)a_val_f32;
              raxv.axv[p] = a_val_f64 * local_X[p / 2][a_col];
            }
          }
          fifo_aXvec.write(raxv);
          ++j;
        }
      }
      start_32 = end_32;
    }

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
    q_gbc_out_Y.write(term_flag);
  }
  // cout << "##### exit PEG_Xvec\n";
}

void PEG_Yvec(tapa::istream<int>& fifo_inst_in,
              tapa::istream<MultXVec>& fifo_aXvec,
              tapa::ostream<double>& fifo_Y_out, tapa::istream<bool>& q_gbc) {
  const int NUM_ITE = fifo_inst_in.read();
  const int M = fifo_inst_in.read();
  const int rp_time = fifo_inst_in.read();

  const int num_v_init = (M + NUM_CH_SPARSE_mult_8 - 1) / NUM_CH_SPARSE_mult_8;
  const int num_v_out = (M + NUM_CH_SPARSE - 1) / NUM_CH_SPARSE;

  bool term_flag = false;

  double local_C[8][URAM_DEPTH];
#pragma HLS bind_storage variable = local_C type = RAM_2P impl = \
    URAM latency = 1
#pragma HLS array_partition complete variable = local_C dim = 1

l_rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    // init local C
  init_C:
    for (int i = 0; i < num_v_init; ++i) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      for (int p = 0; p < 8; ++p) {
        local_C[p][i] = 0.0;
      }
    }

    auto start_32 = fifo_inst_in.read();

  main:
    for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min = 1 max = 49

      // computation
      const auto end_32 = fifo_inst_in.read();

    computation:
      for (int j = start_32; j < end_32;) {
#pragma HLS loop_tripcount min = 1 max = 200
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = local_C distance = DEP_DIST_LOAD_STORE
        if (!fifo_aXvec.empty()) {
          MultXVec raxv;
          fifo_aXvec.try_read(raxv);
          for (int p = 0; p < 8; ++p) {
            auto a_row = raxv.row[p];
            if (a_row[17] == 0) {
              local_C[p][a_row] += raxv.axv[p];
            }
          }
          ++j;
        }
      }
      start_32 = end_32;
    }

    // cout << "PE = " << pe_idx << endl;
  write_C_outer:
    for (int i = 0, c_idx = 0; i < num_v_out; ++i) {
#pragma HLS loop_tripcount min = 1 max = 1800
#pragma HLS pipeline II = 1
      double out_v = local_C[c_idx][i >> 3];
      fifo_Y_out.write(out_v);
      ++c_idx;
      if (c_idx == 8) {
        c_idx = 0;
      }
    }

    ap_wait();
    term_flag = q_gbc.read();
  }
  // cout << "#### exit PEG_Yvec\n";
}

void Arbiter_Y(
    const int rp_time,  // P_N,
    const int M,
    tapa::istreams<double, NUM_CH_SPARSE_div_8>& fifo_in,  // 2 = 16 / 8
    tapa::ostream<double>& fifo_out, tapa::istream<bool>& q_gbc,
    tapa::ostream<bool>& q_gbc_out) {
  // const int rp_time = (P_N == 0)? 1 : P_N;
  const int num_pe_output =
      ((M + NUM_CH_SPARSE - 1) / NUM_CH_SPARSE) * NUM_CH_SPARSE_div_8;
  const int num_out = (M + 7) >> 3;
  const int num_ite_Y = num_pe_output * (rp_time + 1);

  bool term_flag = false;

l_rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
  aby:
    for (int i = 0, c_idx = 0; i < num_pe_output;) {
#pragma HLS loop_tripcount min = 1 max = 1800
#pragma HLS pipeline II = 1
      if (!fifo_in[c_idx].empty() & !fifo_out.full()) {
        double tmp;
        fifo_in[c_idx].try_read(tmp);
        if (i < num_out) {
          fifo_out.try_write(tmp);
        }
        ++i;
        c_idx++;
        if (c_idx == NUM_CH_SPARSE_div_8) {
          c_idx = 0;
        }
      }
    }

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }
  // cout << "### exit Arbiter_Y\n";
}

void Merger_Y(tapa::istreams<double, 8>& fifo_in,
              tapa::ostreams<double_v8, 2>& fifo_out) {
  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_out[0].full() | fifo_out[1].full();
    for (int i = 0; i < 8; ++i) {
      flag_nop |= fifo_in[i].empty();
    }
    if (!flag_nop) {
      double_v8 tmpv;
#pragma HLS aggregate variable = tmpv
      for (int i = 0; i < 8; ++i) {
        double tmp;
        fifo_in[i].try_read(tmp);
        tmpv[i] = tmp;
      }
      fifo_out[0].try_write(tmpv);
      fifo_out[1].try_write(tmpv);
    }
  }
}

template <typename data_t>
inline void bh(tapa::istream<data_t>& q) {
#pragma HLS inline
  for (;;) {
#pragma HLS pipeline II = 1
    data_t tmp;
    q.try_read(tmp);
  }
}

void black_hole_int(tapa::istream<int>& fifo_in) { bh(fifo_in); }

void black_hole_double_v8(tapa::istream<double_v8>& fifo_in) { bh(fifo_in); }

void black_hole_bool(tapa::istream<bool>& fifo_in) { bh(fifo_in); }

void ctrl_P(tapa::istreams<double_v8, 2>& qm_din,
            tapa::ostreams<double_v8, 2>& qm_dout,
            const int rp_time,  // P_N
            const int M,        // K,
            tapa::ostreams<InstRdWr, 2>& q_inst,
            tapa::ostream<double_v8>& q_spmv, tapa::ostream<double_v8>& q_dotp,
            // tapa::ostream<double_v8> & q_updtx,
            tapa::ostream<double_v8>& q_updtp,
            tapa::istream<double_v8>& q_updated, tapa::istreams<bool, 2>& q_res,
            tapa::istream<bool>& q_gbc, tapa::ostream<bool>& q_gbc_out) {
  // InstVCtrl vinst1 = {true, false, 0, (M + 7) >> 3, q_spmv};
  // InstVCtrl vinst2 = {true, false, 0, (M + 7) >> 3, q_dotp};
  // InstVCtrl vinst3 = {true, true, 0, (M + 7) >> 3, q_dotp};
  const int num_ite_M = (M + 7) >> 3;
  bool term_flag = false;

l_rp:
  for (int rp = -1, ch = 0; !term_flag & (rp < rp_time); rp++, ch = 1 - ch) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
    InstRdWr ist;

    // 0 -- rd: q_mem -> q_spmv
    ist.rd = true;
    ist.wr = false;
    // ist.require_response = false;//0;
    ist.base_addr = 0;
    ist.len = num_ite_M;

    if (rp == -1) {
      q_inst[ch].write(ist);
      ap_wait();
      q2q(qm_din, q_spmv, ist.len, ch);
      ap_wait();
    }

    // 1 -- rd: q_mem -> q_dotp
    // ist.rdwr = false;//0;
    // ist.base_addr = 0;
    // ist.len = num_ite;
    q_inst[ch].write(ist);
    ap_wait();
    q2q(qm_din, q_dotp, ist.len, ch);
    ap_wait();

    // 3 -- (1)rd: q_mem -> q_updtp, (2)wr: q_updated -> q_dout

    // ist.rdwr = false;//0;
    // ist.base_addr = 0;
    // ist.len = num_ite;
    q_inst[ch].write(ist);
    ap_wait();

    ist.rd = false;
    ist.wr = true;
    // ist.base_addr = 0;
    // ist.len = num_ite;
    q_inst[1 - ch].write(ist);
    ap_wait();

    qq2qq(q_updated, qm_dout, qm_din, q_updtp, ist.len, 1 - ch);

    ap_wait();
    q_res[1 - ch].read();

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "## exit ctrl_P\n";
}

void ctrl_AP(tapa::istream<double_v8>& qm_din,
             tapa::ostream<double_v8>& qm_dout,

             const int rp_time, const int M, tapa::ostream<InstRdWr>& q_inst,

             tapa::ostream<double_v8>& q_updr, tapa::istream<double_v8>& q_pe,
             tapa::istream<bool>& q_res, tapa::istream<bool>& q_gbc,
             tapa::ostream<bool>& q_gbc_out) {
  // InstVCtrl vinst1 = {false, true, 0, (M + 7) >> 3, q_dout};
  // InstVCtrl vinst2 x 2 = {true, false, 0, (M + 7) >> 3, q_updr};

  const int num_ite_M = (M + 7) >> 3;
  bool term_flag = false;

l_rp:
  for (int rp = -1, ch = 0; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
    InstRdWr ist;

    // 0 -- wr: q_pe -> q_dout
    ist.rd = false;
    ist.wr = true;
    ist.base_addr = 0;
    ist.len = num_ite_M;

    q_inst.write(ist);
    ap_wait();
    q2q(q_pe, qm_dout, ist.len);
    ap_wait();
    q_res.read();
    ap_wait();

    // 1 -- rd: q_din -> q_updr
    ist.rd = true;
    ist.wr = false;
    // ist.base_addr = 0;
    // ist.len = num_ite;

    for (int l = 0; l < 2; ++l) {
#pragma HLS loop_flatten off
      q_inst.write(ist);
      ap_wait();
      q2q(qm_din, q_updr, ist.len);
    }

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "## exit ctrl_AP\n";
}

void ctrl_X(tapa::istreams<double_v8, 2>& qm_din,
            tapa::ostreams<double_v8, 2>& qm_dout,

            const int rp_time, const int M, tapa::ostreams<InstRdWr, 2>& q_inst,

            tapa::ostream<double_v8>& q_oldx, tapa::istream<double_v8>& q_newx,
            tapa::istreams<bool, 2>& q_res, tapa::istream<bool>& q_gbc
            // tapa::ostream<bool> & q_gbc_out
) {
  // InstVCtrl vinst = {true, true, 0, (M + 7) >> 3, 0};
  const int num_ite_M = (M + 7) >> 3;
  bool term_flag = false;

l_rp:
  for (int rp = -1, ch = 0; !term_flag & (rp < rp_time); rp++, ch = 1 - ch) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
    InstRdWr ist;
    ist.rd = true;
    ist.wr = false;
    ist.base_addr = 0;
    ist.len = num_ite_M;

    q_inst[ch].write(ist);
    ap_wait();

    ist.rd = false;
    ist.wr = true;
    q_inst[1 - ch].write(ist);
    ap_wait();

    qq2qq(q_newx, qm_dout, qm_din, q_oldx, ist.len, 1 - ch);

    ap_wait();
    q_res[1 - ch].read();

    ap_wait();
    term_flag = q_gbc.read();
    // q_gbc_out.write(term_flag);
  }

  // cout << "## exit ctrl_X\n";
}

void ctrl_R(tapa::istreams<double_v8, 2>& qm_din,
            tapa::ostreams<double_v8, 2>& qm_dout,

            const int rp_time, const int M, tapa::ostreams<InstRdWr, 2>& q_inst,

            tapa::ostream<double_v8>& qr_to_pe,
            tapa::istream<double_v8>& qr_from_pe,

            tapa::istreams<bool, 2>& q_res, tapa::istream<bool>& q_gbc,
            tapa::ostream<bool>& q_gbc_out) {
  // InstVCtrl vinst1 = {true, false, 0, (M + 7) >> 3, 0};
  // InstVCtrl vinst2 = {true, true, 0, (M + 7) >> 3, 0};
  const int num_ite_M = (M + 7) >> 3;
  bool term_flag = false;

l_rp:
  for (int rp = -1, ch = 0; !term_flag & (rp < rp_time); rp++, ch = 1 - ch) {
#pragma HLS loop_flatten off
    InstRdWr ist;

    // 0 -- rd: q_din -> qr_to_pe
    ist.rd = true;
    ist.wr = false;
    ist.base_addr = 0;
    ist.len = num_ite_M;

    q_inst[ch].write(ist);
    ap_wait();
    q2q(qm_din, qr_to_pe, ist.len, ch);
    ap_wait();

    q_inst[ch].write(ist);
    ap_wait();

    ist.rd = false;
    ist.wr = true;
    q_inst[1 - ch].write(ist);
    ap_wait();

    qq2qq(qr_from_pe, qm_dout, qm_din, qr_to_pe, ist.len, 1 - ch);

    ap_wait();
    q_res[1 - ch].read();

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "## exit ctrl_R\n";
}

void read_digA(tapa::async_mmap<double_v8>& vec_mem,
               const int rp_time,  // P_N
               const int M,        // K,
               tapa::ostream<double_v8>& q_dout, tapa::istream<bool>& q_gbc,
               tapa::ostream<bool>& q_gbc_out) {
  // InstVCtrl vinst = {true, false, 0, (M + 7) >> 3, 0};
  const int num_ite_M = (M + 7) >> 3;
  bool term_flag = false;

l_rp:
  for (int rp = -2; !term_flag & (rp < rp_time * 2); rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd:
    for (int addr_req = 0, i_resp = 0; i_resp < num_ite_M;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      async_read(vec_mem, q_dout, num_ite_M, addr_req, i_resp);
    }

    ap_wait();
    if (rp & 0x1) {
      term_flag = q_gbc.read();
      q_gbc_out.write(term_flag);
    }
  }

  // cout << "## exit ctrl_diagA\n";
}

/*  computation modules  */

// M2: alpha = rzold / (p' * Ap)
void dot_alpha(const int rp_time, const int M,
               // const unsigned long rz0,
               tapa::istream<double>& qrz, tapa::istream<double_v8>& q1,
               tapa::istream<double_v8>& q2, tapa::ostreams<double, 2>& q3,
               tapa::istream<bool>& q_gbc, tapa::ostream<bool>& q_gbc_out) {
  // const InstCmp inst = {(M + 7) >> 3, 0.0, 0};
  const int num_ite = (M + 7) >> 3;
  double rzold = 0.0;

  bool term_flag = false;

rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
    double psum[8][DEP_DIST_LOAD_STORE];
#pragma HLS array_partition complete variable = psum dim = 1

  init:
    for (int i = 0; i < DEP_DIST_LOAD_STORE; ++i) {
#pragma HLS pipeline II = 1
      for (int p = 0; p < 8; ++p) {
        psum[p][i] = 0.0;
      }
    }

  comp1:
    for (int i = 0, idx = 0; i < num_ite;) {
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = psum distance = DEP_DIST_LOAD_STORE
      // DEBUG
      bool tmp1 = q1.empty();
      bool tmp2 = q2.empty();
      if (!q1.empty() & !q2.empty()) {
        double_v8 v1;
        q1.try_read(v1);
        double_v8 v2;
        q2.try_read(v2);
        for (int p = 0; p < 8; ++p) {
          psum[p][idx] += ((i * 8 + p < M) ? v1[p] * v2[p] : 0.0);
        }
        ++i;
        ++idx;
        if (idx == DEP_DIST_LOAD_STORE) {
          idx = 0;
        }
      }
    }

  comp2:
    for (int i = DEP_DIST_LOAD_STORE; i < DEP_DIST_LOAD_STORE * 8; ++i) {
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = psum distance = DEP_DIST_LOAD_STORE
      psum[0][i % DEP_DIST_LOAD_STORE] +=
          psum[i / DEP_DIST_LOAD_STORE][i % DEP_DIST_LOAD_STORE];
    }

  comp3:
    for (int i = 1; i < DEP_DIST_LOAD_STORE; ++i) {
#pragma HLS pipeline II = 1
      psum[0][0] += psum[0][i];
    }

    double pAp = psum[0][0];

    double alpha = rzold / pAp;
    double alpha_out[2];

    alpha_out[0] = alpha;
    alpha_out[1] = alpha;
    if (rp < 0) {
      alpha_out[0] = 0.0;
      alpha_out[1] = 1.0;
    }

    q3[0].write(alpha_out[0]);
    q3[1].write(alpha_out[1]);
    ap_wait();
    rzold = qrz.read();

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "@@@ exit dot_alpha\n";
}

// M end: res = r' * r
void dot_res(const int rp_time, const int M, const double th_termination,
             tapa::istream<double_v8>& q1, tapa::ostream<ResTerm>& q2,
             tapa::ostream<bool>& q_termination) {
  // InstCmp inst = {(M + 7) >> 3, 0.0, 0};
  const int num_ite = (M + 7) >> 3;
  bool term_flag = false;

rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
    double psum[8][DEP_DIST_LOAD_STORE];
#pragma HLS array_partition complete variable = psum dim = 1

  init:
    for (int i = 0; i < DEP_DIST_LOAD_STORE; ++i) {
#pragma HLS pipeline II = 1
      for (int p = 0; p < 8; ++p) {
        psum[p][i] = 0.0;
      }
    }

  comp1:
    for (int i = 0, idx = 0; i < num_ite;) {
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = psum distance = DEP_DIST_LOAD_STORE
      if (!q1.empty()) {
        double_v8 v1;
        q1.try_read(v1);
        for (int p = 0; p < 8; ++p) {
          psum[p][idx] += ((i * 8 + p < M) ? v1[p] * v1[p] : 0.0);
        }
        ++i;
        ++idx;
        if (idx == DEP_DIST_LOAD_STORE) {
          idx = 0;
        }
      }
    }

  comp2:
    for (int i = DEP_DIST_LOAD_STORE; i < DEP_DIST_LOAD_STORE * 8; ++i) {
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = psum distance = DEP_DIST_LOAD_STORE
      psum[0][i % DEP_DIST_LOAD_STORE] +=
          psum[i / DEP_DIST_LOAD_STORE][i % DEP_DIST_LOAD_STORE];
    }

  comp3:
    for (int i = 1; i < DEP_DIST_LOAD_STORE; ++i) {
#pragma HLS pipeline II = 1
      psum[0][0] += psum[0][i];
    }

    ResTerm out_p;
    out_p.res = psum[0][0];

    // cout << "ite = " << rp << ", res = " << res << endl;
    term_flag = out_p.res < th_termination;
    out_p.term = term_flag;
    q2.write(out_p);

    ap_wait();
    q_termination.write(term_flag);
  }

  // cout << "$$$ exit dot_res\n";
}

// M6: rznew = r' * z
void dot_rznew(const int rp_time, const int M, tapa::istream<double_v8>& qr,
               tapa::istream<double_v8>& qz, tapa::ostream<double_v8>& qr_out,
               tapa::ostreams<double, 2>& qrz, tapa::istream<bool>& q_gbc,
               tapa::ostream<bool>& q_gbc_out) {
  // InstCmp inst = {(M + 7) >> 3, 0.0, 0};
  const int num_ite = (M + 7) >> 3;
  bool term_flag = false;

rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
    double psum[8][DEP_DIST_LOAD_STORE];
#pragma HLS array_partition complete variable = psum dim = 1

  init:
    for (int i = 0; i < DEP_DIST_LOAD_STORE; ++i) {
#pragma HLS pipeline II = 1
      for (int p = 0; p < 8; ++p) {
        psum[p][i] = 0.0;
      }
    }

  comp1:
    for (int i = 0, idx = 0; i < num_ite;) {
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = psum distance = DEP_DIST_LOAD_STORE
      // DEBUG
      bool tmp1 = qr.empty();
      bool tmp2 = qz.empty();
      bool tmp3 = qr_out.full();
      if (!qr.empty() & !qz.empty() & !qr_out.full()) {
        double_v8 v1;
        qr.try_read(v1);
        double_v8 v2;
        qz.try_read(v2);
        qr_out.try_write(v1);
        for (int p = 0; p < 8; ++p) {
          psum[p][idx] += ((i * 8 + p < M) ? v1[p] * v2[p] : 0.0);
        }
        ++i;
        ++idx;
        if (idx == DEP_DIST_LOAD_STORE) {
          idx = 0;
        }
      }
    }

  comp2:
    for (int i = DEP_DIST_LOAD_STORE; i < DEP_DIST_LOAD_STORE * 8; ++i) {
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = psum distance = DEP_DIST_LOAD_STORE
      psum[0][i % DEP_DIST_LOAD_STORE] +=
          psum[i / DEP_DIST_LOAD_STORE][i % DEP_DIST_LOAD_STORE];
    }

  comp3:
    for (int i = 1; i < DEP_DIST_LOAD_STORE; ++i) {
#pragma HLS pipeline II = 1
      psum[0][0] += psum[0][i];
    }

    double rz = psum[0][0];

    qrz[0].write(rz);
    qrz[1].write(rz);

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "@@@ exit dot_rznew\n";
}

// M3: x = x + alpha * p
void updt_x(const int rp_time, const int M, tapa::istream<double>& qalpha,
            tapa::istream<double_v8>& qx, tapa::istream<double_v8>& qp,
            tapa::ostream<double_v8>& qout, tapa::istream<bool>& q_gbc
            // tapa::ostream<bool> & q_gbc_out
) {
  // InstCmp inst = {(M + 7) >> 3, qalpha.read(), 0};
  const int num_ite = (M + 7) >> 3;
  bool term_flag = false;

l_rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
    const double alpha = qalpha.read();
    // qout = x + alpha .* p;
  cc:
    for (int i = 0; i < num_ite;) {
#pragma HLS pipeline II = 1
      if (!qx.empty() & !qp.empty()) {
        double_v8 tmpx;
        qx.try_read(tmpx);
        double_v8 tmpp;
        qp.try_read(tmpp);
        qout.write(tmpx + alpha * tmpp);
        ++i;
      }
    }

    ap_wait();
    term_flag = q_gbc.read();
    // q_gbc_out.write(term_flag);
  }
  // cout << "@@@ exit updt_x\n";
}

// M7: p = z + (rznew/rzold) * p
void updt_p(const int rp_time, const int M,
            // const unsigned long rz0,
            tapa::istream<double>& qrznew, tapa::istream<double_v8>& qz,
            tapa::istream<double_v8>& qp, tapa::ostream<double_v8>& qp2m3,
            tapa::ostream<double_v8>& qout, tapa::istream<bool>& q_gbc,
            tapa::ostream<bool>& q_gbc_out) {
  // InstCmp inst = {(M + 7) >> 3, rznew/rzold, 0};
  const int num_ite = (M + 7) >> 3;
  double rzold = 1.0;  // tapa::bit_cast<double>(rz0);
  bool term_flag = false;

l_rp:
  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
    const double rznew = qrznew.read();
    double rzndo = rznew / rzold;
    if (rp < 0) {
      rzndo = 0.0;
    }

  cc:
    for (int i = 0; i < num_ite;) {
#pragma HLS pipeline II = 1
      if (!qz.empty() & !qp.empty() & !qp2m3.full()) {
        double_v8 tmpz;
        qz.try_read(tmpz);
        double_v8 tmpp;
        qp.try_read(tmpp);
        qp2m3.try_write(tmpp);
        qout.write(tmpz + rzndo * tmpp);
        ++i;
      }
    }

    rzold = rznew;

    ap_wait();
    term_flag = q_gbc.read();
    q_gbc_out.write(term_flag);
  }

  // cout << "@@@@ exit updt_p\n";
}

// M4: r = r - alpha * Ap
void updt_r(const int rp_time, const int M, tapa::istream<double>& qalpha,
            tapa::istream<double_v8>& qr, tapa::istream<double_v8>& qap,
            tapa::ostream<double_v8>& qout, tapa::istream<bool>& q_gbc,
            tapa::ostream<bool>& q_gbc_out) {
  // InstCmp inst = {(M + 7) >> 3, alpha, 0};
  const int num_ite = (M + 7) >> 3;
  double alpha = 0.0;

  bool term_flag = false;

l_rp:
  for (int rp = -2; !term_flag & (rp < rp_time * 2); rp++) {
#pragma HLS loop_flatten off
    if ((rp & 0x1) == 0) {
      alpha = qalpha.read();
    }

    // qout = x + alpha .* p;
  cc:
    for (int i = 0; i < num_ite;) {
#pragma HLS pipeline II = 1
      if (!qr.empty() & !qap.empty()) {
        double_v8 tmpr;
        qr.try_read(tmpr);
        double_v8 tmpap;
        qap.try_read(tmpap);
        qout.write(tmpr - alpha * tmpap);
        ++i;
      }
    }

    ap_wait();
    if ((rp & 0x1) == 1) {
      term_flag = q_gbc.read();
      q_gbc_out.write(term_flag);
    }
  }

  // cout << "@@@@ exit updt_r\n";
}

// M5: z = diagA \ r
void left_div(const int rp_time, const int M, tapa::istream<double_v8>& qr,
              tapa::istream<double_v8>& qdiagA,

              tapa::ostreams<double_v8, 2>& qz,

              tapa::ostream<double_v8>& qr_m6, tapa::ostream<double_v8>& qrmem,
              tapa::istream<bool>& q_gbc, tapa::ostream<bool>& q_gbc_out) {
  // InstCmp inst1 = {(M + 7) >> 3, 0.0, 0};
  // InstCmp inst2 = {(M + 7) >> 3, 0.0, 1};
  InstCmp inst = {(M + 7) >> 3, 0.0, 0};
  bool term_flag = false;

rp:
  for (int rp = -2; !term_flag & (rp < rp_time * 2); rp++) {
#pragma HLS loop_flatten off
    inst.q_idx = rp & 0x1;
  cc:
    for (int i = 0; i < inst.len;) {
#pragma HLS pipeline II = 1
      // DEBUG
      bool tmp1 = qr.empty();
      bool tmp2 = qdiagA.empty();
      bool nop_flag = qr.empty() | qdiagA.empty();
      if (inst.q_idx == 0) {
        nop_flag |= qr_m6.full();
      } else {
        nop_flag |= qrmem.full();
      }
      if (!nop_flag) {
        double_v8 v1;
        qr.try_read(v1);
        if (inst.q_idx == 0) {
          qr_m6.try_write(v1);
        } else {
          qrmem.try_write(v1);
        }
        double_v8 v2;
        qdiagA.try_read(v2);
        double_v8 result;
        for (int p = 0; p < 8; ++p) {
          result[p] = ((i * 8 + p < M) ? v1[p] / v2[p] : 0.0);
        }
        ++i;
        qz[inst.q_idx].write(result);
      }
    }

    ap_wait();
    if (inst.q_idx == 1) {
      term_flag = q_gbc.read();
      q_gbc_out.write(term_flag);
    }
  }

  // cout << "@@@ exit left_div\n";
}

void wr_r(const int rp_time, tapa::async_mmap<double>& vec_r,
          tapa::istream<ResTerm>& q_din) {
  int wr_count = rp_time + 1;
wr:
  for (int addr_req = 0, i_resp = 0; i_resp < wr_count;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
    if ((addr_req < wr_count) & !q_din.empty() & !vec_r.write_addr.full() &
        !vec_r.write_data.full()) {
      vec_r.write_addr.try_write(addr_req);
      ResTerm tmpv;
      q_din.try_read(tmpv);
      vec_r.write_data.try_write(tmpv.res);
      ++addr_req;
      if (tmpv.term) {
        wr_count = addr_req;
      }
    }
    uint8_t n_resp;
    if (vec_r.write_resp.try_read(n_resp)) {
      i_resp += int(n_resp) + 1;
    }
  }
  // cout << "$$$$ exit wr_r\n";
}

void duplicator(tapa::istream<double_v8>& q_in,
                tapa::ostream<double_v8>& q_out1,
                tapa::ostream<double_v8>& q_out2) {
cc:
  for (;;) {
#pragma HLS pipeline II = 1
    if (!q_in.empty() & !q_out1.full() & !q_out2.full()) {
      double_v8 tmp;
      q_in.try_read(tmp);
      q_out1.try_write(tmp);
      q_out2.try_write(tmp);
    }
  }
}

void vecp_mux(const int rp_time, const int M, tapa::istream<bool>& q_gbc,

              tapa::istream<double_v8>& q_in1, tapa::istream<double_v8>& q_in2,

              tapa::ostream<double_v8>& q_out) {
  const int num_ite = (M + 7) >> 3;
  bool term_flag = false;

  // deliver p form memory at ite 0
  q2q(q_in1, q_out, num_ite);

  for (int rp = -1; !term_flag & (rp < rp_time); rp++) {
#pragma HLS loop_flatten off
    term_flag = q_gbc.read();
    if (term_flag | (rp == rp_time - 1)) {
      clearq(q_in2, num_ite);
    } else {
      q2q(q_in2, q_out, num_ite);
    }
  }
}

void Callipepla(tapa::mmap<int> edge_list_ptr,

                tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,

                tapa::mmaps<double_v8, 2> vec_x,

                tapa::mmaps<double_v8, 2> vec_p,

                tapa::mmap<double_v8> vec_Ap,

                tapa::mmaps<double_v8, 2> vec_r,

                tapa::mmap<double_v8> vec_digA,

                tapa::mmap<double> vec_res,

                const int NUM_ITE, const int NUM_A_LEN, const int M,
                const int rp_time, const double th_termination) {
  // fifos for spmv

  tapa::streams<int, NUM_CH_SPARSE + 1, FIFO_DEPTH> PE_inst("PE_inst");

  tapa::streams<double_v8, NUM_CH_SPARSE + 1, FIFO_DEPTH> fifo_P_pe(
      "fifo_P_pe");

  tapa::streams<ap_uint<512>, NUM_CH_SPARSE, FIFO_DEPTH> fifo_A("fifo_A");

  tapa::streams<int, NUM_CH_SPARSE, FIFO_DEPTH> Yvec_inst("Yvec_inst");

  tapa::streams<MultXVec, NUM_CH_SPARSE, FIFO_DEPTH> fifo_aXvec("fifo_aXvec");

  tapa::streams<double, NUM_CH_SPARSE, FIFO_DEPTH> fifo_Y_pe("fifo_Y_pe");

  tapa::streams<double, 8, FIFO_DEPTH> fifo_Y_pe_abd("fifo_Y_pe_abd");

  // fifos for vector modules

  // P
  tapa::streams<InstRdWr, 2, FIFO_DEPTH> fifo_mi_P("fifo_mi_P");

  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_din_P("fifo_din_P");

  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_dout_P("fifo_dout_P");

  tapa::streams<bool, 2, FIFO_DEPTH> fifo_resp_P("fifo_resp_P");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_P_dot("fifo_P_dot");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_P_updtx("fifo_P_updtx");

  tapa::stream<double_v8, 2> fifo_P_updtp("fifo_P_updtp");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_P_updated("fifo_P_updated");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_P_to_dup("fifo_P_to_dup");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_P_to_mux("fifo_P_to_mux");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_P_from_mem("fifo_P_from_mem");

  // R
  tapa::streams<InstRdWr, 2, FIFO_DEPTH> fifo_mi_R("fifo_mi_R");

  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_din_R("fifo_din_R");

  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_dout_R("fifo_dout_R");

  tapa::streams<bool, 2, FIFO_DEPTH> fifo_resp_R("fifo_resp_R");

  tapa::stream<double_v8, 2> fifo_R("fifo_R");
  // to m4

  tapa::stream<double_v8, FIFO_DEPTH> fifo_R_updtd_m5("fifo_R_updtd_m5");
  // to m5

  tapa::stream<double_v8, FIFO_DEPTH_M6> fifo_R_updtd_m6("fifo_R_updtd_m6");
  // to m6

  tapa::stream<double_v8, FIFO_DEPTH> fifo_R_updtd_rr("fifo_R_updtd_rr");
  // to rr

  tapa::stream<double_v8, FIFO_DEPTH> fifo_R_tomem("fifo_R_tomem");

  tapa::stream<ResTerm, FIFO_DEPTH> fifo_RR("fifo_RR");

  // diagA
  tapa::stream<double_v8, FIFO_DEPTH> fifo_dA("fifo_dA");

  // X
  tapa::streams<InstRdWr, 2, FIFO_DEPTH> fifo_mi_X("fifo_mi_X");

  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_din_X("fifo_din_X");

  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_dout_X("fifo_dout_X");

  tapa::streams<bool, 2, FIFO_DEPTH> fifo_resp_X("fifo_resp_X");

  tapa::stream<double_v8, 2> fifo_X("fifo_X");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_X_updated("fifo_X_updated");

  // AP
  tapa::stream<InstRdWr, FIFO_DEPTH> fifo_mi_AP("fifo_mi_AP");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_din_AP("fifo_din_AP");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_dout_AP("fifo_dout_AP");

  tapa::stream<bool, FIFO_DEPTH> fifo_resp_AP("fifo_resp_AP");

  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_AP_M1("fifo_AP_M1");

  tapa::stream<double_v8, FIFO_DEPTH> fifo_AP("fifo_AP");

  // Z
  tapa::streams<double_v8, 2, FIFO_DEPTH> fifo_Z("fifo_Z");

  // scalars
  tapa::streams<double, 2, FIFO_DEPTH> fifo_alpha("fifo_alpha");

  tapa::streams<double, 2, FIFO_DEPTH> fifo_rz("fifo_rz");

  // termination signal
  tapa::stream<bool, FIFO_DEPTH> tsignal_res("tsignal_res");

  tapa::streams<bool, NUM_CH_SPARSE + 1, FIFO_DEPTH> tsignal_rdA("tsignal_rdA");

  tapa::streams<bool, NUM_CH_SPARSE + 1 + 1, FIFO_DEPTH> tsignal_edgepointer(
      "tsignal_edgepointer");

  tapa::streams<bool, NUM_CH_SPARSE, FIFO_DEPTH> tsignal_Y("tsignal_Y");

  tapa::streams<bool, 8 + 1, FIFO_DEPTH> tsignal_aby("tsignal_aby");

  tapa::stream<bool, FIFO_DEPTH> tsignal_toM3("tsignal_toM3");

  tapa::stream<bool, FIFO_DEPTH> tsignal_toM4("tsignal_toM4");

  tapa::stream<bool, FIFO_DEPTH> tsignal_toM5("tsignal_toM5");

  tapa::stream<bool, FIFO_DEPTH> tsignal_toM6("tsignal_toM6");

  tapa::stream<bool, FIFO_DEPTH> tsignal_toM7("tsignal_toM7");

  tapa::stream<bool, FIFO_DEPTH> tsignal_ctrlP("tsignal_ctrlP");

  tapa::stream<bool, FIFO_DEPTH> tsignal_ctrlAP("tsignal_ctrlAP");

  tapa::stream<bool, FIFO_DEPTH> tsignal_ctrldigA("tsignal_ctrldigA");

  tapa::stream<bool, FIFO_DEPTH> tsignal_ctrlR("tsignal_ctrlR");

  tapa::stream<bool, FIFO_DEPTH> tsignal_ctrlX("tsignal_ctrlX");

  tapa::stream<bool, FIFO_DEPTH> tsignal_mux("tsignal_mux");

  /* =========deploy modules======= */

  tapa::task()
      .invoke<tapa::detach>(term_signal_router, tsignal_res, tsignal_rdA,
                            tsignal_edgepointer, tsignal_aby, tsignal_ctrlP,
                            tsignal_mux)

      .invoke<tapa::join>(read_edge_list_ptr, NUM_ITE, M, rp_time,
                          edge_list_ptr, PE_inst, tsignal_edgepointer,
                          tsignal_edgepointer)

      // P
      .invoke<tapa::detach, 2>(rdwr_vec, vec_p, fifo_mi_P, fifo_dout_P,
                               fifo_din_P, fifo_resp_P)

      .invoke<tapa::join>(ctrl_P, fifo_din_P, fifo_dout_P, rp_time, M,
                          fifo_mi_P, fifo_P_from_mem, fifo_P_dot,
                          // fifo_P_updtx,
                          fifo_P_updtp, fifo_P_updated, fifo_resp_P,
                          tsignal_ctrlP, tsignal_ctrlAP)
      // p mux
      .invoke<tapa::join>(vecp_mux, rp_time, M, tsignal_mux, fifo_P_from_mem,
                          fifo_P_to_mux, fifo_P_pe)

      .invoke<tapa::join, NUM_CH_SPARSE>(read_A, rp_time, NUM_A_LEN,
                                         edge_list_ch, fifo_A, tsignal_rdA,
                                         tsignal_rdA)
      .invoke<tapa::detach>(black_hole_bool, tsignal_rdA)

      .invoke<tapa::join, NUM_CH_SPARSE>(
          PEG_Xvec, PE_inst, fifo_A, fifo_P_pe, PE_inst, fifo_P_pe, Yvec_inst,
          fifo_aXvec, tsignal_edgepointer, tsignal_edgepointer, tsignal_Y)
      .invoke<tapa::detach>(black_hole_int, PE_inst)
      .invoke<tapa::detach>(black_hole_double_v8, fifo_P_pe)
      .invoke<tapa::detach>(black_hole_bool, tsignal_edgepointer)

      .invoke<tapa::join, NUM_CH_SPARSE>(PEG_Yvec, Yvec_inst, fifo_aXvec,
                                         fifo_Y_pe, tsignal_Y)

      .invoke<tapa::join, 8>(Arbiter_Y, rp_time, M, fifo_Y_pe, fifo_Y_pe_abd,
                             tsignal_aby, tsignal_aby)

      .invoke<tapa::detach>(Merger_Y, fifo_Y_pe_abd, fifo_AP_M1)

      // vector memory modules

      // R
      .invoke<tapa::detach, 2>(rdwr_vec, vec_r, fifo_mi_R, fifo_dout_R,
                               fifo_din_R, fifo_resp_R)

      .invoke<tapa::join>(ctrl_R, fifo_din_R, fifo_dout_R, rp_time, M,
                          fifo_mi_R, fifo_R, fifo_R_tomem, fifo_resp_R,
                          tsignal_ctrlR, tsignal_ctrlX)

      // digA
      .invoke<tapa::join>(read_digA, vec_digA, rp_time, M, fifo_dA,
                          tsignal_ctrldigA, tsignal_ctrlR)

      // X
      .invoke<tapa::detach, 2>(rdwr_vec, vec_x, fifo_mi_X, fifo_dout_X,
                               fifo_din_X, fifo_resp_X)

      .invoke<tapa::join>(ctrl_X, fifo_din_X, fifo_dout_X, rp_time, M,
                          fifo_mi_X, fifo_X, fifo_X_updated, fifo_resp_X,
                          tsignal_ctrlX)

      // AP
      .invoke<tapa::detach>(rdwr_vec, vec_Ap, fifo_mi_AP, fifo_dout_AP,
                            fifo_din_AP, fifo_resp_AP)

      .invoke<tapa::join>(ctrl_AP, fifo_din_AP, fifo_dout_AP, rp_time, M,
                          fifo_mi_AP, fifo_AP, fifo_AP_M1, fifo_resp_AP,
                          tsignal_ctrlAP, tsignal_ctrldigA)

      // vector processing modules

      // M2: alpha = rzold / (p' * Ap)
      .invoke<tapa::join>(dot_alpha, rp_time, M,
                          // rz0,
                          fifo_rz, fifo_P_dot, fifo_AP_M1, fifo_alpha,
                          tsignal_aby, tsignal_toM4)

      // M3: x = x + alpha * p
      .invoke<tapa::join>(updt_x, rp_time, M, fifo_alpha, fifo_X, fifo_P_updtx,
                          fifo_X_updated, tsignal_toM3)

      // M4: r = r - alpha * Ap
      .invoke<tapa::join>(updt_r, rp_time, M, fifo_alpha, fifo_R, fifo_AP,
                          fifo_R_updtd_m5, tsignal_toM4, tsignal_toM5)

      // M5: z = diagA \ r
      .invoke<tapa::join>(left_div, rp_time, M, fifo_R_updtd_m5, fifo_dA,
                          fifo_Z, fifo_R_updtd_m6, fifo_R_tomem, tsignal_toM5,
                          tsignal_toM6)

      // M6: rznew = r' * z
      .invoke<tapa::join>(dot_rznew, rp_time, M, fifo_R_updtd_m6, fifo_Z,
                          fifo_R_updtd_rr, fifo_rz, tsignal_toM6, tsignal_toM7)

      // M7: p = z + (rznew/rzold) * p
      .invoke<tapa::join>(updt_p, rp_time, M,
                          // rz0,
                          fifo_rz, fifo_Z, fifo_P_updtp, fifo_P_updtx,
                          fifo_P_to_dup, tsignal_toM7, tsignal_toM3)
      .invoke<tapa::detach>(duplicator, fifo_P_to_dup, fifo_P_updated,
                            fifo_P_to_mux)

      // M residual
      .invoke<tapa::join>(dot_res, rp_time, M, th_termination, fifo_R_updtd_rr,
                          fifo_RR, tsignal_res)

      .invoke<tapa::join>(wr_r, rp_time, vec_res, fifo_RR);
}
