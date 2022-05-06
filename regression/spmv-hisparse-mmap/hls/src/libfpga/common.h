#ifndef SPMV_COMMON_H_
#define SPMV_COMMON_H_

#include <ap_fixed.h>
#include <ap_int.h>
#include <ap_axi_sdata.h>

#define IDX_MARKER 0xffffffff

#ifndef __SYNTHESIS__
#include <iostream>
#include <iomanip>
#endif

//-------------------------------------------------------------------------
// overlay configurations
//-------------------------------------------------------------------------
const unsigned PACK_SIZE = 8;

//-------------------------------------------------------------------------
// basic data types
//-------------------------------------------------------------------------
const unsigned IBITS = 8;
const unsigned FBITS = 32 - IBITS;
typedef unsigned IDX_T;
typedef ap_ufixed<32, IBITS, AP_RND, AP_SAT> VAL_T;
#define VAL_T_BITCAST(v) (v(31,0))

//-------------------------------------------------------------------------
// kernel-memory interface packet types
//-------------------------------------------------------------------------
typedef struct {IDX_T data[PACK_SIZE];} PACKED_IDX_T;
typedef struct {VAL_T data[PACK_SIZE];} PACKED_VAL_T;

typedef struct {
   PACKED_IDX_T indices;
   PACKED_VAL_T vals;
} SPMV_MAT_PKT_T;

typedef SPMV_MAT_PKT_T SPMSPV_MAT_PKT_T;

typedef struct {IDX_T index; VAL_T val;} IDX_VAL_T;

//-------------------------------------------------------------------------
// intra-kernel dataflow payload types
//-------------------------------------------------------------------------
// 2-bit instruction
typedef ap_uint<2> INST_T;
#define SOD 0x1 // start-of-data
#define EOD 0x2 // end-of-data
#define EOS 0x3 // end-of-stream

// edge payload (COO), used between SpMV matrix loader <-> SpMV shuffle 1
struct EDGE_PLD_T {
    VAL_T mat_val;
    IDX_T row_idx;
    IDX_T col_idx;
    INST_T inst;
};
#define EDGE_PLD_SOD ((EDGE_PLD_T){0,0,0,SOD})
#define EDGE_PLD_EOD ((EDGE_PLD_T){0,0,0,EOD})
#define EDGE_PLD_EOS ((EDGE_PLD_T){0,0,0,EOS})

// update payload, used by all PEs
struct UPDATE_PLD_T {
    VAL_T mat_val;
    VAL_T vec_val;
    IDX_T row_idx;
    INST_T inst;
};
#define UPDATE_PLD_SOD ((UPDATE_PLD_T){0,0,0,SOD})
#define UPDATE_PLD_EOD ((UPDATE_PLD_T){0,0,0,EOD})
#define UPDATE_PLD_EOS ((UPDATE_PLD_T){0,0,0,EOS})

// vector payload, used between SpMV vector unpacker <-> SpMV vector reader
// and all PE outputs
struct VEC_PLD_T{
    VAL_T val;
    IDX_T idx;
    INST_T inst;
};
#define VEC_PLD_SOD ((VEC_PLD_T){0,0,SOD})
#define VEC_PLD_EOD ((VEC_PLD_T){0,0,EOD})
#define VEC_PLD_EOS ((VEC_PLD_T){0,0,EOS})

#ifndef __SYNTHESIS__
std::string inst2str(INST_T inst) {
    switch (inst) {
        case SOD: return std::string("SOD");
        case EOD: return std::string("EOD");
        case EOS: return std::string("EOS");
        default:  return std::string(std::to_string((int)inst));
    }
}

std::ostream& operator<<(std::ostream& os, const EDGE_PLD_T &p) {
    os << '{'
        << "mat val: " << p.mat_val << '|'
        << "row idx: " << p.row_idx << '|'
        << "col idx: " << p.col_idx << '|'
        << "inst: "  << inst2str(p.inst) << '}';
    return os;
}

std::ostream& operator<<(std::ostream& os, const UPDATE_PLD_T &p) {
    os << '{'
        << "mat val: " << p.mat_val << '|'
        << "vec val: " << p.vec_val << '|'
        << "row idx: " << p.row_idx << '|'
        << "inst: "  << inst2str(p.inst) << '}';
    return os;
}

std::ostream& operator<<(std::ostream& os, const VEC_PLD_T &p) {
    os << '{'
        << "val: " << p.val << '|'
        << "idx: " << p.idx << '|'
        << "inst: "  << inst2str(p.inst) << '}';
    return os;
}
#endif

//-------------------------------------------------------------------------
// kernel-to-kernel streaming payload types
//-------------------------------------------------------------------------

// only works on Vitis 2020.2
typedef struct {
    ap_uint<32 * (PACK_SIZE + 1)> data;
    ap_uint<2> user; // same as INST_T
} VEC_AXIS_T;

#define VEC_AXIS_PKT_IDX(p) (p.data(31,0))
#define VEC_AXIS_VAL(p, i) (p.data(63 + 32 * i,32 + 32 * i))

#ifndef __SYNTHESIS__
std::ostream& operator<<(std::ostream& os, const VEC_AXIS_T &p) {
    os << '{' << "pktidx: " << VEC_AXIS_PKT_IDX(p) << '|';
    for (unsigned i = 0; i < PACK_SIZE; i++) {
        os << "val: " << float(VEC_AXIS_VAL(p, i)) / (1 << FBITS) << '|';
    }
    os << "user: "  << inst2str(p.user) << '}';
    return os;
}
#endif

//-------------------------------------------------------------------------
// Kernel configurations
//-------------------------------------------------------------------------
const unsigned FIFO_DEPTH = 64;

const unsigned OB_BANK_SIZE = 1024 * 8;
const unsigned VB_BANK_SIZE = 1024 * 4;

// const unsigned OB_BANK_SIZE = 128;
// const unsigned VB_BANK_SIZE = 512;
const unsigned INTERLEAVE_FACTOR = 1;

const unsigned OB_PER_CLUSTER = OB_BANK_SIZE * PACK_SIZE;
const unsigned VB_PER_CLUSTER = VB_BANK_SIZE * PACK_SIZE;
const unsigned SK0_CLUSTER = 4;
const unsigned SK1_CLUSTER = 6;
const unsigned SK2_CLUSTER = 6;
const unsigned NUM_HBM_CHANNELS = SK0_CLUSTER + SK1_CLUSTER + SK2_CLUSTER;

const unsigned LOGICAL_OB_SIZE = (SK0_CLUSTER + SK1_CLUSTER + SK2_CLUSTER) * OB_PER_CLUSTER;
const unsigned LOGICAL_VB_SIZE = VB_PER_CLUSTER;

// Below kernel configurations will be overwritten by the compiler
#endif  // SPMV_COMMON_H_
