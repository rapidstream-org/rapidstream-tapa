#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include <inttypes.h>
#include <stdlib.h>


const int IWIDTH = 256;
#define INTERFACE_WIDTH ap_uint<IWIDTH>
#define INPUT_DIM (2)
#define TOP (10)
#define NUM_SP_PTS (1048576)
#define DISTANCE_METRIC (1)
#define NUM_PE (1)
#define NUM_KERNEL (1)

#define DATA_TYPE_TOTAL_SZ 32
#define DATA_TYPE float
#define LOCAL_DIST_SZ   (32)
#define LOCAL_DIST_DTYPE float
#define TRANSFER_TYPE ap_uint<DATA_TYPE_TOTAL_SZ>

/***************************************************************/

#define BUFFER_SIZE_PADDED (1048576)
#define NUM_SP_PTS_PADDED (1048576)

// NOTE: Each of the below calculations are effectively a ceil() operation.
//      Ex: (x-1)/y + 1 is ceil(x/y).
// L2I = Local to Interface
const int L2I_FACTOR_W = ((IWIDTH - 1) / (INPUT_DIM * LOCAL_DIST_SZ)) + 1;
// D2L = Data_Type to Local
const int D2L_FACTOR_W = ((LOCAL_DIST_SZ - 1)/ (DATA_TYPE_TOTAL_SZ)) + 1;
// D2I = Data_Type to Interface
const int D2I_FACTOR_W = ((IWIDTH - 1) / (INPUT_DIM * DATA_TYPE_TOTAL_SZ)) + 1;
// I2D = Interface to Data_type
const int I2D_FACTOR_W = ((INPUT_DIM * DATA_TYPE_TOTAL_SZ - 1) / (IWIDTH)) + 1;
#define NUM_OF_TILES (64)
#define TILE_LEN_IN_I (BUFFER_SIZE_PADDED / IWIDTH)
#define TILE_LEN_IN_D (BUFFER_SIZE_PADDED / (INPUT_DIM * DATA_TYPE_TOTAL_SZ))
#define TILE_LEN_IN_L (BUFFER_SIZE_PADDED / (INPUT_DIM * LOCAL_DIST_SZ))
// // DEBUG NOTE: BW_FACTOR = 0.7698287024216459
#define USING_LTYPES 1
#define PARALLEL_SORT (1)
#define PARALLEL_SORT_FACTOR (L2I_FACTOR_W * 2)
#define USING_CAT_CMP 0

const int SWIDTH = DATA_TYPE_TOTAL_SZ;
typedef ap_axiu<SWIDTH, 0, 0, 0> pkt;
typedef ap_axiu<32, 0, 0, 0>    id_pkt;
#define STREAM_WIDTH ap_uint<SWIDTH>

const int NUM_FEATURES_PER_READ = (IWIDTH/DATA_TYPE_TOTAL_SZ);
const int QUERY_FEATURE_RESERVE = (128);
#define QUERY_DATA_RESERVE (QUERY_FEATURE_RESERVE / NUM_FEATURES_PER_READ)
#define MAX_DATA_TYPE_VAL (3.402823e+38f)
#define FLOOR_SQRT_MAX_DATA_TYPE_VAL (1.8446742e+19f)

// We name each sub-array of the local_distance arrays a "segment".
#define NUM_SEGMENTS PARALLEL_SORT_FACTOR

#define SEGMENT_SIZE_IN_L (2048)
#define SEGMENT_SIZE_IN_D (SEGMENT_SIZE_IN_L*D2L_FACTOR_W)

const int __NUM_PADDED_SEGMENTS = (1 + ((NUM_SEGMENTS * SEGMENT_SIZE_IN_L - TILE_LEN_IN_L) / SEGMENT_SIZE_IN_L));
const int SEGMENT_IDX_START_OF_PADDING = (NUM_SEGMENTS - __NUM_PADDED_SEGMENTS);
const int LVALUE_IDX_START_OF_PADDING = (TILE_LEN_IN_L % SEGMENT_SIZE_IN_L);

const int NUM_ITERATIONS = 1;
