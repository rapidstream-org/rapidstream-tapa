#include "ap_int.h"
#include "ap_axi_sdata.h"
#include <tapa.h>
#include <inttypes.h>
#include <stdlib.h>


const int IWIDTH = 256;
#define INTERFACE_WIDTH ap_uint<IWIDTH>
#define INPUT_DIM (2)
#define TOP (10)
#define NUM_SP_PTS (1048576)
#define DISTANCE_METRIC (1)
#define NUM_PE (1)
#define NUM_KERNEL_L1_L2 (3)
#define NUM_KERNEL_L3 (2)

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
#define NUM_PART (1)


static inline DATA_TYPE absval(DATA_TYPE input){
    return (input > 0 ? input : static_cast<DATA_TYPE>(-1*input));
}

bool compare_with_register(DATA_TYPE in_1, DATA_TYPE in_2){
#pragma HLS pipeline
#pragma HLS inline off
#pragma HLS LATENCY min=2 max=2
    return in_1 < in_2;
}

/*************************************************/
/******************** LOADS: *********************/
/*************************************************/

void load(int flag, int tile_idx, INTERFACE_WIDTH* local_SP, tapa::async_mmap<INTERFACE_WIDTH>& searchSpace)
{
#pragma HLS INLINE OFF
    if (flag){
        for (int i_req = 0, i_resp = 0; i_resp < TILE_LEN_IN_I; ) {
          #pragma HLS pipeline II=1
          int addr = QUERY_DATA_RESERVE + tile_idx*TILE_LEN_IN_I + i_req;
          if (i_req < TILE_LEN_IN_I && searchSpace.read_addr.try_write(addr)) {
            i_req++;
          }
          if (!searchSpace.read_data.empty()) {
            local_SP[i_resp] = searchSpace.read_data.read(nullptr);
            i_resp++;
          }
        }
    }
}

/*************************************************/
/******************* COMPUTES: *******************/
/*************************************************/

void compute(int flag, DATA_TYPE* local_Query, INTERFACE_WIDTH* local_SP,
        LOCAL_DIST_DTYPE local_distance[NUM_SEGMENTS][SEGMENT_SIZE_IN_L+TOP],
        int debug_i)
{
#pragma HLS INLINE OFF
    if (flag){
        int SP_idx = 0;
        for (int ii = 0 ; ii < NUM_SEGMENTS; ++ii){
            for (int jj = 0; jj < SEGMENT_SIZE_IN_D/D2I_FACTOR_W; ++jj){
#pragma HLS PIPELINE II=1
                LOCAL_DIST_DTYPE aggregated_local_dists = 0;
                SP_idx = ii * SEGMENT_SIZE_IN_D/D2I_FACTOR_W + jj;
                int TEMP_DEBUG_INT = 0;

                for (int kk = 0; kk < D2I_FACTOR_W; ++kk){
                #pragma HLS UNROLL

                    DATA_TYPE delta_squared_sum = 0.0;
                    unsigned int dist_range_idx =  (kk % D2L_FACTOR_W) * DATA_TYPE_TOTAL_SZ;
                    int start_idx = kk * INPUT_DIM;

                    for (int ll = 0; ll < INPUT_DIM; ++ll){
                        unsigned int sp_range_idx = (start_idx + ll) * DATA_TYPE_TOTAL_SZ;

                        DATA_TYPE sp_dim_item_value;
                        TRANSFER_TYPE tmp;

                        tmp.range(DATA_TYPE_TOTAL_SZ-1, 0) =
                            local_SP[SP_idx].range(sp_range_idx + (DATA_TYPE_TOTAL_SZ-1),
                                                   sp_range_idx);

                        sp_dim_item_value = *((DATA_TYPE*) (&tmp));

#if DISTANCE_METRIC == 0 // manhattan
                        DATA_TYPE delta = absval(sp_dim_item_value - local_Query[ll]);
                        delta_squared_sum += delta;
#elif DISTANCE_METRIC == 1 // L2
                        // NOTE(Kenny): I think this absval will help unsigned payloads.
                        DATA_TYPE delta = absval(sp_dim_item_value - local_Query[ll]);
                        delta_squared_sum += delta * delta;
#endif
                    }
                    aggregated_local_dists = delta_squared_sum;

                    if ((kk % D2L_FACTOR_W) == D2L_FACTOR_W - 1){
                        unsigned int inner_idx_location = (jj*D2I_FACTOR_W + kk) / D2L_FACTOR_W;
                        local_distance[ii][inner_idx_location] = aggregated_local_dists;
                        aggregated_local_dists = 0;
                    }
                }
            }
        }
    }
}
void swap(DATA_TYPE* a, DATA_TYPE* b,
               int* x, int* y)
{
#pragma HLS INLINE

    DATA_TYPE tmpdist_a;
    DATA_TYPE tmpdist_b;

    int tmpid_x;
    int tmpid_y;

    tmpdist_a = *a;
    tmpdist_b = *b;
    *b = tmpdist_a;
    *a = tmpdist_b;

    tmpid_x = *x;
    tmpid_y = *y;
    *x = tmpid_y;
    *y = tmpid_x;
}
void para_partial_sort(LOCAL_DIST_DTYPE* local_distance,
                            int start_id,
                            DATA_TYPE local_kNearstDist[D2L_FACTOR_W][TOP+1],
                            int local_kNearstId[D2L_FACTOR_W][TOP+1])
{
#pragma HLS INLINE OFF
    for (int i = 0; i < (SEGMENT_SIZE_IN_L+TOP)*D2L_FACTOR_W; ++i) {
    #pragma HLS PIPELINE II=2
    #pragma HLS UNROLL FACTOR=D2L_FACTOR_W

        LOCAL_DIST_DTYPE cur_Lval = local_distance[i/D2L_FACTOR_W];
        unsigned char D_idx = i%D2L_FACTOR_W;
        unsigned char range_idx = (D_idx)*DATA_TYPE_TOTAL_SZ;
        DATA_TYPE cur_Dval;

        cur_Dval = cur_Lval;

        local_kNearstDist[D_idx][0] = cur_Dval;
        local_kNearstId[D_idx][0] = start_id + i;


        //compare and swap odd
        for(int ii=1; ii<TOP; ii+=2){
        #pragma HLS UNROLL
        #pragma HLS DEPENDENCE variable="local_kNearstDist" inter false
        #pragma HLS DEPENDENCE variable="local_kNearstId" inter false

            bool fcmp = compare_with_register(local_kNearstDist[D_idx][ii], local_kNearstDist[D_idx][ii+1]);
            if (fcmp){
                swap(&local_kNearstDist[D_idx][ii], &local_kNearstDist[D_idx][ii+1],
                          &local_kNearstId[D_idx][ii], &local_kNearstId[D_idx][ii+1]);
            }

        }


        //compare and swap even
        for(int ii=1; ii<TOP+1; ii+=2){
        #pragma HLS UNROLL
        #pragma HLS DEPENDENCE variable="local_kNearstDist" inter false
        #pragma HLS DEPENDENCE variable="local_kNearstId" inter false
            bool fcmp = compare_with_register(local_kNearstDist[D_idx][ii-1], local_kNearstDist[D_idx][ii]);
            if (fcmp){
                swap(&local_kNearstDist[D_idx][ii], &local_kNearstDist[D_idx][ii-1],
                          &local_kNearstId[D_idx][ii], &local_kNearstId[D_idx][ii-1]);
            }

        }
    }
}
void sort(int flag, int start_id,
          LOCAL_DIST_DTYPE local_distance[NUM_SEGMENTS][SEGMENT_SIZE_IN_L+TOP],
		  DATA_TYPE local_kNearstDist_partial [NUM_SEGMENTS][D2L_FACTOR_W][TOP+1],
          int local_kNearstId_partial [NUM_SEGMENTS][D2L_FACTOR_W][TOP+1])
{
#pragma HLS INLINE OFF
	if (flag){
		int starting_id[NUM_SEGMENTS];
		#pragma HLS ARRAY_PARTITION variable=starting_id complete dim=0

		for (int i = 0; i < NUM_SEGMENTS; ++i){
		#pragma HLS UNROLL
			starting_id[i] = start_id+i*SEGMENT_SIZE_IN_D;
		}

        // overwrite invalid sections of the local_distnace array that are only there because
        //  we cant perfectly "segment" our tiles
		for (int i = 0; i < (NUM_SEGMENTS * SEGMENT_SIZE_IN_L - TILE_LEN_IN_L)%SEGMENT_SIZE_IN_L; ++i){
		#pragma HLS PIPELINE II=1
		#pragma HLS UNROLL FACTOR=L2I_FACTOR_W
            DATA_TYPE maxval = MAX_DATA_TYPE_VAL;
            LOCAL_DIST_DTYPE aggregated_local_dists = 0;

            for (int k = 0; k < D2L_FACTOR_W; ++k){
                aggregated_local_dists = maxval;
            }

            local_distance[SEGMENT_IDX_START_OF_PADDING][LVALUE_IDX_START_OF_PADDING+i] =
                aggregated_local_dists;
		}

        #if (NUM_SEGMENTS - 1) != SEGMENT_IDX_START_OF_PADDING
        for (int outer_idx = SEGMENT_IDX_START_OF_PADDING+1; outer_idx < NUM_SEGMENTS; ++outer_idx){
            for (int i = 0; i < SEGMENT_SIZE_IN_L; i++){
		    #pragma HLS PIPELINE II=1
                DATA_TYPE maxval = MAX_DATA_TYPE_VAL;
                LOCAL_DIST_DTYPE aggregated_local_dists = 0;

                for (int k = 0; k < D2L_FACTOR_W; ++k){
                    aggregated_local_dists = maxval;
                }

                local_distance[outer_idx][i] =
                    aggregated_local_dists;
            }
        }
        #endif

		for (unsigned char i = 0; i < NUM_SEGMENTS; ++i){
		#pragma HLS UNROLL
			para_partial_sort(local_distance[i],
				   starting_id[i],
				   local_kNearstDist_partial[i],
				   local_kNearstId_partial[i]);
		}
	}
	/*
	else{
		for (int i = 0; i < NUM_SEGMENTS; ++i){
			for (int j = 0; j < D2L_FACTOR_W; ++j){
                for (int k = 0; k < TOP+1; ++k){
			    #pragma HLS UNROLL
			    	local_kNearstId_partial[i][j][k] = -1;
			        local_kNearstDist_partial[i][j][k] = MAX_DATA_TYPE_VAL;
			    }
            }
		}
	}
	*/
}
void merge_dual(DATA_TYPE local_kNearstDist_partial_a[TOP+1],
                DATA_TYPE local_kNearstDist_partial_b[TOP+1],
                int local_kNearstId_partial_a[TOP+1],
                int local_kNearstId_partial_b[TOP+1],
                DATA_TYPE dist[TOP+1], int id[TOP+1])
{
    // Process only TWO segments TOPs, instead of all of them.
    int idx_a = TOP;
    int idx_b = TOP;

    /*********************/

    for (int i = TOP; i >= 0; --i)
    {
        if (local_kNearstDist_partial_a[idx_a] < local_kNearstDist_partial_b[idx_b])
        {
            dist[i] = local_kNearstDist_partial_a[idx_a];
            id[i] = local_kNearstId_partial_a[idx_a];
            --idx_a;
        }
        else
        {
            dist[i] = local_kNearstDist_partial_b[idx_b];
            id[i] = local_kNearstId_partial_b[idx_b];
            --idx_b;
        }
    }
}

void merge_dual_all_PEs(DATA_TYPE local_kNearstDist_partial_a[NUM_PART][TOP+1],
                        DATA_TYPE local_kNearstDist_partial_b[NUM_PART][TOP+1],
                        int local_kNearstId_partial_a[NUM_PART][TOP+1],
                        int local_kNearstId_partial_b[NUM_PART][TOP+1],
                        DATA_TYPE dist[NUM_PART][TOP+1], int id[NUM_PART][TOP+1])
{
    for (int i = 0; i < NUM_PART; ++i)
    {
    #pragma HLS UNROLL
        merge_dual(local_kNearstDist_partial_a[i],
                   local_kNearstDist_partial_b[i],
                   local_kNearstId_partial_a  [i],
                   local_kNearstId_partial_b  [i],
                   dist[i],
                   id[i]);
    }
}

template <int id>
void krnl_partialKnn(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
    #pragma HLS inline

    DATA_TYPE local_Query_0[INPUT_DIM];
    #pragma HLS ARRAY_PARTITION variable=local_Query_0 complete dim=1
    INTERFACE_WIDTH local_SP_0_A[TILE_LEN_IN_I];
    #pragma HLS RESOURCE variable=local_SP_0_A core=XPM_MEMORY uram
    INTERFACE_WIDTH local_SP_0_B[TILE_LEN_IN_I];
    #pragma HLS RESOURCE variable=local_SP_0_B core=XPM_MEMORY uram

    LOCAL_DIST_DTYPE local_distance_0_A[NUM_SEGMENTS][SEGMENT_SIZE_IN_L+TOP];
    #pragma HLS ARRAY_PARTITION variable=local_distance_0_A complete dim=1
    #pragma HLS ARRAY_PARTITION variable=local_distance_0_A cyclic factor=L2I_FACTOR_W dim=2
    LOCAL_DIST_DTYPE local_distance_0_B[NUM_SEGMENTS][SEGMENT_SIZE_IN_L+TOP];
    #pragma HLS ARRAY_PARTITION variable=local_distance_0_B complete dim=1
    #pragma HLS ARRAY_PARTITION variable=local_distance_0_B cyclic factor=L2I_FACTOR_W dim=2

    // These are the outputs of the sort() function.
    //  Together, they contain the nearest (distance, ID) pairs for each segment of all tiles.
    static DATA_TYPE local_kNearstDist_partial_0[NUM_SEGMENTS][D2L_FACTOR_W][(TOP+1)];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstDist_partial_0 complete dim=0
    static int local_kNearstId_partial_0[NUM_SEGMENTS][D2L_FACTOR_W][(TOP+1)];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstId_partial_0 complete dim=0

    // These store the top K results for each PE.
    DATA_TYPE local_kNearstDist[NUM_PART][TOP+1];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstDist complete dim=0
    int local_kNearstId[NUM_PART][TOP+1];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstId complete dim=0

    // These store the top K results for this KERNEL.
    DATA_TYPE global_kNearstDist[TOP+1];
    #pragma HLS ARRAY_PARTITION variable=global_kNearstDist complete
    int global_kNearstId[TOP+1];
    #pragma HLS ARRAY_PARTITION variable=global_kNearstId complete

    LOAD_QUERY: for (int i_req = 0, i_resp = 0; i_resp < INPUT_DIM;){
        #pragma HLS pipeline II=1
        // issue read address
        int input_rd_idx = i_req / NUM_FEATURES_PER_READ;
        if (i_req < INPUT_DIM && searchSpace_0.read_addr.try_write(input_rd_idx)) {
          ++i_req;
        }

        // get read response
        if (! searchSpace_0.read_data.empty()) {
          INTERFACE_WIDTH resp = searchSpace_0.read_data.read(nullptr);
          int range_idx = i_resp % NUM_FEATURES_PER_READ;
          TRANSFER_TYPE tmp;
          tmp.range(DATA_TYPE_TOTAL_SZ - 1, 0)
              = resp.range(range_idx*DATA_TYPE_TOTAL_SZ + (DATA_TYPE_TOTAL_SZ-1),
                                                          range_idx*DATA_TYPE_TOTAL_SZ);
          local_Query_0[i_resp] = *((DATA_TYPE*)(&tmp));

          i_resp++;
        }
    }

    ITERATION_LOOP: for (int it_idx = 0; it_idx < NUM_ITERATIONS; ++it_idx)
    {
        for (int i = 0; i < NUM_SEGMENTS; ++i){
            for (int j = 0; j < TOP; ++j){
            #pragma HLS PIPELINE II=1

                DATA_TYPE maxval = MAX_DATA_TYPE_VAL;
                LOCAL_DIST_DTYPE aggregated_local_dists = 0;

                // Overwrite local_distance, chunk by chunk.
                for (int k = 0; k < D2L_FACTOR_W; ++k){
                    aggregated_local_dists = maxval;
                }

                local_distance_0_A[i][SEGMENT_SIZE_IN_L+j] = aggregated_local_dists;
                local_distance_0_B[i][SEGMENT_SIZE_IN_L+j] = aggregated_local_dists;
            }
        }

		for (int i = 0; i < NUM_SEGMENTS; ++i){
			for (int j = 0; j < D2L_FACTOR_W; ++j){
                for (int k = 0; k < TOP+1; ++k){
			    #pragma HLS UNROLL
			    	local_kNearstId_partial_0[i][j][k] = -1;
			        local_kNearstDist_partial_0[i][j][k] = MAX_DATA_TYPE_VAL;
			    }
            }
		}

        for(int i = 0; i < NUM_OF_TILES+2; ++i){
            int load_img_flag = i >= 0 && i < NUM_OF_TILES;
            int compute_flag = i >= 1 && i < NUM_OF_TILES + 1;
            int sort_flag = i >= 2 && i < NUM_OF_TILES + 2;
            if (i % 2 == 0) {
                load(load_img_flag, i, local_SP_0_A, searchSpace_0);
                compute(compute_flag, local_Query_0, local_SP_0_B, local_distance_0_B, i);
                sort(sort_flag, start_id_0+(i-2)*TILE_LEN_IN_D, local_distance_0_A, local_kNearstDist_partial_0, local_kNearstId_partial_0);
            }
            else {
                load(load_img_flag, i, local_SP_0_B, searchSpace_0);
                compute(compute_flag, local_Query_0, local_SP_0_A, local_distance_0_A, i);
                sort(sort_flag, start_id_0+(i-2)*TILE_LEN_IN_D, local_distance_0_B, local_kNearstDist_partial_0, local_kNearstId_partial_0);
            }
        }
        /**********************************************************************/
        /**************************  MERGING PARTIAL SORTS ********************/
        /**********************************************************************/
        DATA_TYPE temp_kNearstDist[NUM_SEGMENTS][D2L_FACTOR_W*2][NUM_PART][TOP+1];
        #pragma HLS ARRAY_PARTITION variable=temp_kNearstDist complete dim=1
        #pragma HLS ARRAY_PARTITION variable=temp_kNearstDist complete dim=2
        #pragma HLS ARRAY_PARTITION variable=temp_kNearstDist complete dim=3
        #pragma HLS RESOURCE variable=temp_kNearstDist core=RAM_1P_LUTRAM
        int       temp_kNearstId  [NUM_SEGMENTS][D2L_FACTOR_W*2][NUM_PART][TOP+1];
        #pragma HLS ARRAY_PARTITION variable=temp_kNearstId complete dim=1
        #pragma HLS ARRAY_PARTITION variable=temp_kNearstId complete dim=2
        #pragma HLS ARRAY_PARTITION variable=temp_kNearstId complete dim=3
        #pragma HLS RESOURCE variable=temp_kNearstId core=RAM_1P_LUTRAM

        for (int i = 0; i < NUM_SEGMENTS; ++i)
        {
        #pragma HLS unroll
            for (int j = 0; j < D2L_FACTOR_W; ++j)
            {
            #pragma HLS unroll
                for (int k = 0; k < TOP+1; ++k)
                {
                #pragma HLS unroll
                    temp_kNearstDist[i][j][0][k] = local_kNearstDist_partial_0[i][j][k];
                    temp_kNearstId  [i][j][0][k] = local_kNearstId_partial_0  [i][j][k];

                }
            }
        }

        /*********************************************/
        /* Merge pairwise on the NUM_SEGMENTS-level. */
        /*********************************************/

        for (int i = 0; i < 4; ++i){
        #pragma HLS unroll
            merge_dual_all_PEs(temp_kNearstDist[i*2 + 0][0],
                               temp_kNearstDist[i*2 + 1][0],
                               temp_kNearstId  [i*2 + 0][0],
                               temp_kNearstId  [i*2 + 1][0],
                               temp_kNearstDist[i][D2L_FACTOR_W],
                               temp_kNearstId  [i][D2L_FACTOR_W]);
        }
        for (int i = 0; i < 2; ++i){
        #pragma HLS unroll
            merge_dual_all_PEs(temp_kNearstDist[i*2 + 0][D2L_FACTOR_W],
                               temp_kNearstDist[i*2 + 1][D2L_FACTOR_W],
                               temp_kNearstId  [i*2 + 0][D2L_FACTOR_W],
                               temp_kNearstId  [i*2 + 1][D2L_FACTOR_W],
                               temp_kNearstDist[i][0],
                               temp_kNearstId  [i][0]);
        }
        for (int i = 0; i < NUM_PART; ++i)
        {
        #pragma HLS UNROLL
            merge_dual(temp_kNearstDist[0][0][i],
                       temp_kNearstDist[1][0][i],
                       temp_kNearstId  [0][0][i],
                       temp_kNearstId  [1][0][i],
                       local_kNearstDist[i],
                       local_kNearstId  [i]);
        }

        // Copy the data to the global buffer.
        for (int j = 0; j < TOP+1; ++j){
            global_kNearstDist[j] = local_kNearstDist[0][j];
            global_kNearstId[j] = local_kNearstId[0][j];
        }
    }

    STREAM_WIDTH v_data;
    DATA_TYPE temp_data;
    DIST_OUT: for (int i = 1; i < TOP+1; ++i)
    {
    #pragma HLS PIPELINE II=1
        temp_data = global_kNearstDist[i];

        v_data = *((STREAM_WIDTH*)(&temp_data));

        pkt v;
        v.data = v_data;
        out_dist.write(v);
    }
    ID_OUT: for (int i = 1; i < TOP+1; ++i)
    {
    #pragma HLS PIPELINE II=1
        id_pkt v_id;
        v_id.data = global_kNearstId[i];
        out_id.write(v_id);
    }
    return;
}



void krnl_partialKnn_wrapper_0(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<0>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_1(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<1>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_2(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<2>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_3(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<3>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_4(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<4>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_5(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<5>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_6(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<6>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_7(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<7>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_8(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<8>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_9(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<9>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_10(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<10>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_11(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<11>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_12(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<12>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_13(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<13>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_14(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<14>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_15(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<15>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_16(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<16>(searchSpace_0, start_id_0, out_dist, out_id);
}


void krnl_partialKnn_wrapper_17(
    tapa::async_mmap<INTERFACE_WIDTH>& searchSpace_0,
    int start_id_0,
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id)
{
  krnl_partialKnn<17>(searchSpace_0, start_id_0, out_dist, out_id);
}


// L1 & L2 Global Merge - 3 stream pairs : 1 stream pair
void seq_global_merge_L1_L2(DATA_TYPE local_kNearstDist_partial[NUM_KERNEL_L1_L2][TOP],
                        int local_kNearstId_partial[NUM_KERNEL_L1_L2][TOP],
                        DATA_TYPE* dist, int* id)
{
#pragma HLS INLINE OFF
    int idx[NUM_KERNEL_L1_L2];
    #pragma HLS ARRAY_PARTITION variable=idx complete dim=0
    for (int i = 0; i < NUM_KERNEL_L1_L2; ++i)
    {
    #pragma HLS UNROLL
        idx[i] = TOP-1;
    }
    for (int i = TOP-1; i >= 0; --i)
    {
        DATA_TYPE min_value = MAX_DATA_TYPE_VAL;
        int min_idx = -1;
        for (int j = 0; j < NUM_KERNEL_L1_L2; ++j)
        {
        #pragma HLS PIPELINE II=1
            if (local_kNearstDist_partial[j][idx[j]] < min_value){
                min_value = local_kNearstDist_partial[j][idx[j]];
                min_idx = j;
            }
        }
        dist[i] = min_value;
        id[i] = local_kNearstId_partial[min_idx][idx[min_idx]];
        idx[min_idx] = idx[min_idx] - 1;
    }
}

void krnl_globalSort_L1_L2(
    tapa::istream<pkt> &in_dist0,    // Internal Stream
    tapa::istream<id_pkt> &in_id0,    // Internal Stream
    tapa::istream<pkt> &in_dist1,    // Internal Stream
    tapa::istream<id_pkt> &in_id1,    // Internal Stream
    tapa::istream<pkt> &in_dist2,    // Internal Stream
    tapa::istream<id_pkt> &in_id2,    // Internal Stream
    tapa::ostream<pkt> &out_dist,
    tapa::ostream<id_pkt> &out_id
) {

    DATA_TYPE local_kNearstDist_partial[NUM_KERNEL_L1_L2][TOP];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstDist_partial complete dim=0
    int local_kNearstId_partial[NUM_KERNEL_L1_L2][TOP];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstId_partial complete dim=0

    DATA_TYPE output_dist[TOP];
    #pragma HLS ARRAY_PARTITION variable=output_dist complete
    int output_id[TOP];
    #pragma HLS ARRAY_PARTITION variable=output_id complete

    for (unsigned int i=0; i<TOP; ++i) {
    #pragma HLS PIPELINE II=1
      pkt v0 = in_dist0.read();
      STREAM_WIDTH v0_item = v0.data.range(DATA_TYPE_TOTAL_SZ-1, 0);
      local_kNearstDist_partial[0][i] = *((DATA_TYPE*)(&v0_item));
      pkt v1 = in_dist1.read();
      STREAM_WIDTH v1_item = v1.data.range(DATA_TYPE_TOTAL_SZ-1, 0);
      local_kNearstDist_partial[1][i] = *((DATA_TYPE*)(&v1_item));
      pkt v2 = in_dist2.read();
      STREAM_WIDTH v2_item = v2.data.range(DATA_TYPE_TOTAL_SZ-1, 0);
      local_kNearstDist_partial[2][i] = *((DATA_TYPE*)(&v2_item));
    }

    for (unsigned int i=0; i<TOP; ++i) {
    #pragma HLS PIPELINE II=1
      id_pkt v0_id = in_id0.read();
      local_kNearstId_partial[0][i] = v0_id.data;
      id_pkt v1_id = in_id1.read();
      local_kNearstId_partial[1][i] = v1_id.data;
      id_pkt v2_id = in_id2.read();
      local_kNearstId_partial[2][i] = v2_id.data;
    }

    seq_global_merge_L1_L2(local_kNearstDist_partial, local_kNearstId_partial, output_dist, output_id);

    STREAM_WIDTH v_data;
    DATA_TYPE temp_data;
    DIST_OUT: for (int i = 0; i < TOP; ++i)
    {
    #pragma HLS PIPELINE II=1
        temp_data = output_dist[i];

        v_data = *((STREAM_WIDTH*)(&temp_data));

        pkt v;
        v.data = v_data;
        out_dist.write(v);
    }
    ID_OUT: for (int i = 0; i < TOP; ++i)
    {
    #pragma HLS PIPELINE II=1
        id_pkt v_id;
        v_id.data = output_id[i];
        out_id.write(v_id);
    }

}

// L3 Global Merge - 2 stream pairs : 1 mmap pair
void seq_global_merge_L3(DATA_TYPE local_kNearstDist_partial[NUM_KERNEL_L3][TOP],
                        int local_kNearstId_partial[NUM_KERNEL_L3][TOP],
                        DATA_TYPE* dist, int* id)
{
#pragma HLS INLINE OFF
    int idx[NUM_KERNEL_L3];
    #pragma HLS ARRAY_PARTITION variable=idx complete dim=0
    for (int i = 0; i < NUM_KERNEL_L3; ++i)
    {
    #pragma HLS UNROLL
        idx[i] = TOP-1;
    }
    for (int i = TOP-1; i >= 0; --i)
    {
        DATA_TYPE min_value = MAX_DATA_TYPE_VAL;
        int min_idx = -1;
        for (int j = 0; j < NUM_KERNEL_L3; ++j)
        {
        #pragma HLS PIPELINE II=1
            if (local_kNearstDist_partial[j][idx[j]] < min_value){
                min_value = local_kNearstDist_partial[j][idx[j]];
                min_idx = j;
            }
        }
        dist[i] = min_value;
        id[i] = local_kNearstId_partial[min_idx][idx[min_idx]];
        idx[min_idx] = idx[min_idx] - 1;
    }
}

void krnl_globalSort_L3(
    tapa::istream<pkt> &in_dist0,    // Internal Stream
    tapa::istream<id_pkt> &in_id0,    // Internal Stream
    tapa::istream<pkt> &in_dist1,    // Internal Stream
    tapa::istream<id_pkt> &in_id1,    // Internal Stream
    tapa::async_mmap<DATA_TYPE>& output_knnDist,   // Output Result
    tapa::async_mmap<int>& output_knnId            // Output Result
) {

    DATA_TYPE local_kNearstDist_partial[NUM_KERNEL_L3][TOP];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstDist_partial complete dim=0
    int local_kNearstId_partial[NUM_KERNEL_L3][TOP];
    #pragma HLS ARRAY_PARTITION variable=local_kNearstId_partial complete dim=0

    DATA_TYPE output_dist[TOP];
    #pragma HLS ARRAY_PARTITION variable=output_dist complete
    int output_id[TOP];
    #pragma HLS ARRAY_PARTITION variable=output_id complete

    for (unsigned int i=0; i<TOP; ++i) {
    #pragma HLS PIPELINE II=1
      pkt v0 = in_dist0.read();
      STREAM_WIDTH v0_item = v0.data.range(DATA_TYPE_TOTAL_SZ-1, 0);
      local_kNearstDist_partial[0][i] = *((DATA_TYPE*)(&v0_item));
      pkt v1 = in_dist1.read();
      STREAM_WIDTH v1_item = v1.data.range(DATA_TYPE_TOTAL_SZ-1, 0);
      local_kNearstDist_partial[1][i] = *((DATA_TYPE*)(&v1_item));
    }

    for (unsigned int i=0; i<TOP; ++i) {
    #pragma HLS PIPELINE II=1
      id_pkt v0_id = in_id0.read();
      local_kNearstId_partial[0][i] = v0_id.data;
      id_pkt v1_id = in_id1.read();
      local_kNearstId_partial[1][i] = v1_id.data;
    }

    seq_global_merge_L3(local_kNearstDist_partial, local_kNearstId_partial, output_dist, output_id);

for (unsigned int i_req_dist = 0, i_resp_dist = 0, i_req_id = 0, i_resp_id = 0; i_resp_dist < TOP || i_resp_id < TOP; ) {
      #pragma HLS pipeline II=1

      // write to output_KnnDist
      if (i_req_dist < TOP &&
          !output_knnDist.write_addr.full() &&
          !output_knnDist.write_data.full()
      ) {
        output_knnDist.write_addr.try_write(i_req_dist);
        output_knnDist.write_data.try_write(output_dist[i_req_dist]);

        ++i_req_dist;
      }

      if (!output_knnDist.write_resp.empty()) {
        i_resp_dist += (unsigned int)(output_knnDist.write_resp.read(nullptr)) + 1;
      }

      // write to output_KnnId
      if (i_req_id < TOP &&
          !output_knnId.write_addr.full() &&
          !output_knnId.write_data.full()
      ) {
        output_knnId.write_addr.try_write(i_req_id);
        output_knnId.write_data.try_write(output_id[i_req_id]);

        ++i_req_id;
      }

      if (!output_knnId.write_resp.empty()) {
        i_resp_id += (unsigned int)(output_knnId.write_resp.read(nullptr)) + 1;
      }

    }
}

void Knn(
  tapa::mmap<INTERFACE_WIDTH> in_0,
  tapa::mmap<INTERFACE_WIDTH> in_1,
  tapa::mmap<INTERFACE_WIDTH> in_2,
  tapa::mmap<INTERFACE_WIDTH> in_3,
  tapa::mmap<INTERFACE_WIDTH> in_4,
  tapa::mmap<INTERFACE_WIDTH> in_5,
  tapa::mmap<INTERFACE_WIDTH> in_6,
  tapa::mmap<INTERFACE_WIDTH> in_7,
  tapa::mmap<INTERFACE_WIDTH> in_8,
  tapa::mmap<INTERFACE_WIDTH> in_9,
  tapa::mmap<INTERFACE_WIDTH> in_10,
  tapa::mmap<INTERFACE_WIDTH> in_11,
  tapa::mmap<INTERFACE_WIDTH> in_12,
  tapa::mmap<INTERFACE_WIDTH> in_13,
  tapa::mmap<INTERFACE_WIDTH> in_14,
  tapa::mmap<INTERFACE_WIDTH> in_15,
  tapa::mmap<INTERFACE_WIDTH> in_16,
  tapa::mmap<INTERFACE_WIDTH> in_17,
  tapa::mmap<float> L3_out_dist,
  tapa::mmap<int> L3_out_id
) {

  tapa::streams<pkt, 18, 2> out_dist;
  tapa::streams<id_pkt, 18, 2> out_id;
  tapa::streams<pkt, 6, 2> L1_out_dist;
  tapa::streams<id_pkt, 6, 2> L1_out_id;
  tapa::stream<pkt, 2> L2_out_dist0;
  tapa::stream<pkt, 2> L2_out_dist1;
  tapa::stream<id_pkt, 2> L2_out_id0;
  tapa::stream<id_pkt, 2> L2_out_id1;

  tapa::task()
    .invoke( krnl_partialKnn_wrapper_0   , in_0,  NUM_SP_PTS_PADDED*0,  out_dist[0], out_id[0]  )
    .invoke( krnl_partialKnn_wrapper_1   , in_1,  NUM_SP_PTS_PADDED*1,  out_dist[1], out_id[1]  )
    .invoke( krnl_partialKnn_wrapper_2   , in_2,  NUM_SP_PTS_PADDED*2,  out_dist[2], out_id[2]  )
    .invoke( krnl_partialKnn_wrapper_3   , in_3,  NUM_SP_PTS_PADDED*3,  out_dist[3], out_id[3]  )
    .invoke( krnl_partialKnn_wrapper_4   , in_4,  NUM_SP_PTS_PADDED*4,  out_dist[4], out_id[4]  )
    .invoke( krnl_partialKnn_wrapper_5   , in_5,  NUM_SP_PTS_PADDED*5,  out_dist[5], out_id[5]  )
    .invoke( krnl_partialKnn_wrapper_6   , in_6,  NUM_SP_PTS_PADDED*6,  out_dist[6], out_id[6]  )
    .invoke( krnl_partialKnn_wrapper_7   , in_7,  NUM_SP_PTS_PADDED*7,  out_dist[7], out_id[7]  )
    .invoke( krnl_partialKnn_wrapper_8   , in_8,  NUM_SP_PTS_PADDED*8,  out_dist[8], out_id[8]  )
    .invoke( krnl_partialKnn_wrapper_9   , in_9,  NUM_SP_PTS_PADDED*9,  out_dist[9], out_id[9]  )
    .invoke( krnl_partialKnn_wrapper_10  , in_10, NUM_SP_PTS_PADDED*10, out_dist[10], out_id[10] )
    .invoke( krnl_partialKnn_wrapper_11  , in_11, NUM_SP_PTS_PADDED*11, out_dist[11], out_id[11] )
    .invoke( krnl_partialKnn_wrapper_12  , in_12, NUM_SP_PTS_PADDED*12, out_dist[12], out_id[12] )
    .invoke( krnl_partialKnn_wrapper_13  , in_13, NUM_SP_PTS_PADDED*13, out_dist[13], out_id[13] )
    .invoke( krnl_partialKnn_wrapper_14  , in_14, NUM_SP_PTS_PADDED*14, out_dist[14], out_id[14] )
    .invoke( krnl_partialKnn_wrapper_15  , in_15, NUM_SP_PTS_PADDED*15, out_dist[15], out_id[15] )
    .invoke( krnl_partialKnn_wrapper_16  , in_16, NUM_SP_PTS_PADDED*16, out_dist[16], out_id[16] )
    .invoke( krnl_partialKnn_wrapper_17  , in_17, NUM_SP_PTS_PADDED*17, out_dist[17], out_id[17] )
    .invoke( krnl_globalSort_L1_L2,  out_dist[0],  out_id[0],  out_dist[1],  out_id[1],  out_dist[2],  out_id[2], L1_out_dist[0], L1_out_id[0] )
    .invoke( krnl_globalSort_L1_L2,  out_dist[3],  out_id[3],  out_dist[4],  out_id[4],  out_dist[5],  out_id[5], L1_out_dist[1], L1_out_id[1] )
    .invoke( krnl_globalSort_L1_L2,  out_dist[6],  out_id[6],  out_dist[7],  out_id[7],  out_dist[8],  out_id[8], L1_out_dist[2], L1_out_id[2] )
    .invoke( krnl_globalSort_L1_L2,  out_dist[9],  out_id[9], out_dist[10], out_id[10], out_dist[11], out_id[11], L1_out_dist[3], L1_out_id[3] )
    .invoke( krnl_globalSort_L1_L2, out_dist[12], out_id[12], out_dist[13], out_id[13], out_dist[14], out_id[14], L1_out_dist[4], L1_out_id[4] )
    .invoke( krnl_globalSort_L1_L2, out_dist[15], out_id[15], out_dist[16], out_id[16], out_dist[17], out_id[17], L1_out_dist[5], L1_out_id[5] )
    .invoke( krnl_globalSort_L1_L2, L1_out_dist[0], L1_out_id[0], L1_out_dist[1], L1_out_id[1], L1_out_dist[2], L1_out_id[2], L2_out_dist0, L2_out_id0 )
    .invoke( krnl_globalSort_L1_L2, L1_out_dist[3], L1_out_id[3], L1_out_dist[4], L1_out_id[4], L1_out_dist[5], L1_out_id[5], L2_out_dist1, L2_out_id1 )
    .invoke( krnl_globalSort_L3, L2_out_dist0, L2_out_id0, L2_out_dist1, L2_out_id1, L3_out_dist, L3_out_id);

}
