#include <cstdint>
#include <tapa.h>

//#define ALEN 8
//#define BLEN 32//16384//256

#define MATCH_SCORE 1
#define MISMATCH_SCORE -1
#define GAP_SCORE -1

#define ALIGN 1
#define SKIPA 2
#define SKIPB 3

#define MAX(A,B) ( ((A)>(B))?(A):(B) )



void SEQA_datafeed_head(
  tapa::mmap<const int> SEQA,
  tapa::ostream<int> & SEQA_trans_out,
  int ALEN
){
#pragma HLS INLINE off
	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < ALEN; counter++){
		int SEQA_in_data = SEQA[counter];
		SEQA_trans_out.write(SEQA_in_data);
		//printf("SEQA index:%d data:%d\n", counter, SEQA_in_data);
	}
}


void SEQA_datafeed(
  tapa::istream<int> & SEQA_trans_in,
  tapa::ostream<int> & SEQA_trans_out,
  tapa::ostream<int> & SEQA_feed,
  int         pe_col_id,
  int pe_num 
){
#pragma HLS INLINE off
	int SEQA_in_data = SEQA_trans_in.read();
	SEQA_feed.write(SEQA_in_data);

	[[tapa::pipeline(1)]]
	for (int counter = pe_col_id+1; counter < pe_num; counter++){
			int SEQA_in_data = SEQA_trans_in.read();
			SEQA_trans_out.write(SEQA_in_data);
	}
}

void SEQA_datafeed_tail(
  tapa::istream<int> & SEQA_trans_in,
  tapa::ostream<int> & SEQA_feed,
  int         pe_col_id 
){
#pragma HLS INLINE off
	int SEQA_in_data = SEQA_trans_in.read();
	SEQA_feed.write(SEQA_in_data);
}

void SEQB_datafeed_head(
  tapa::mmap<const int> SEQB,
  tapa::ostream<int> & SEQB_trans_out,
  int BLEN
){
#pragma HLS INLINE off
	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < BLEN; counter++){
		int SEQB_in_data = SEQB[counter];
		SEQB_trans_out.write(SEQB_in_data);
		//printf("SEQB index:%d data:%d\n", counter, SEQB_in_data);
	}
}

void op_transfer(
  tapa::istream<int> & SEQA_in,
  tapa::istream<int> & SEQB_in,
  tapa::ostream<int> & SEQB_out,
  tapa::ostream<int> & SEQA_local,
  tapa::ostream<int> & SEQB_local,
  int BLEN
  ){
#pragma HLS INLINE off
	int SEQA_in_data = SEQA_in.read();
	SEQA_local.write(SEQA_in_data);

	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < BLEN; counter++){
		int SEQB_in_data = SEQB_in.read();
		SEQB_local.write(SEQB_in_data);
		SEQB_out.write(SEQB_in_data);
	}
}

void op_transfer_tail(
  tapa::istream<int> & SEQA_in,
  tapa::istream<int> & SEQB_in,
  tapa::ostream<int> & SEQA_local,
  tapa::ostream<int> & SEQB_local,
  int BLEN
  ){
#pragma HLS INLINE off
	int SEQA_in_data = SEQA_in.read();
	SEQA_local.write(SEQA_in_data);

	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < BLEN; counter++){
		int SEQB_in_data = SEQB_in.read();
		SEQB_local.write(SEQB_in_data);
	}
}

void res_transfer(
  tapa::istream<int> & ptr_local,
  tapa::ostream<int> & ptr_collect,
  int BLEN
  ){
#pragma HLS INLINE off
	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < BLEN; counter++){
		int ptr_in_data = ptr_local.read();
		ptr_collect.write(ptr_in_data);
	}
}

void compute(
  tapa::istream<int> & SEQA_local,
  tapa::istream<int> & SEQB_local,
  tapa::istream<int> & Mleft_local_in,
  tapa::ostream<int> & Mleft_local_out,
  tapa::ostream<int> & ptr_local,
  int         pe_col_id,
  int BLEN
){
#pragma HLS INLINE off
	int M = (pe_col_id + 1) * GAP_SCORE;
	int Mleft = (pe_col_id) * GAP_SCORE;

	int SEQA_in_data = SEQA_local.read();

	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < BLEN; counter++){
		int SEQB_in_data = SEQB_local.read();

		int M_prev = M;
		int Mleft_prev = Mleft;
		Mleft = Mleft_local_in.read();

		int score;
		if( SEQA_in_data == SEQB_in_data ){
			score = MATCH_SCORE;
		}
		else{
			score = MISMATCH_SCORE;
		}

		int up_left = Mleft_prev + score;
		int up = M + GAP_SCORE;
		int left = Mleft + GAP_SCORE;

		int max = MAX(up_left, MAX(up, left));
		M = max;
		Mleft_local_out.write(M);

		int ptr;
		if(max == left){
			ptr = SKIPB;
		} else{
			if(max == up){
				ptr = SKIPA;
			} else{
				ptr = ALIGN;
			}
		} 
		ptr_local.write(ptr);
	}
}

void compute_head(
  tapa::istream<int> & SEQA_local,
  tapa::istream<int> & SEQB_local,
  tapa::ostream<int> & Mleft_local_out,
  tapa::ostream<int> & ptr_local,
  int         pe_col_id, 
  int BLEN
){
#pragma HLS INLINE off
	int M = (pe_col_id + 1) * GAP_SCORE;
	int Mleft = (pe_col_id) * GAP_SCORE;

	int SEQA_in_data = SEQA_local.read();

	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < BLEN; counter++){
		int SEQB_in_data = SEQB_local.read();

		int M_prev = M;
		int Mleft_prev = Mleft;
		Mleft = (counter+1)*GAP_SCORE;

		int score;
		if( SEQA_in_data == SEQB_in_data ){
			score = MATCH_SCORE;
		}
		else{
			score = MISMATCH_SCORE;
		}

		int up_left = Mleft_prev + score;
		int up = M + GAP_SCORE;
		int left = Mleft + GAP_SCORE;

		int max = MAX(up_left, MAX(up, left));
		M = max;
		Mleft_local_out.write(M);

		int ptr;
		if(max == left){
			ptr = SKIPB;
		} else{
			if(max == up){
				ptr = SKIPA;
			} else{
				ptr = ALIGN;
			}	 
		}
		ptr_local.write(ptr);
	}
}

void compute_tail(
  tapa::istream<int> & SEQA_local,
  tapa::istream<int> & SEQB_local,
  tapa::istream<int> & Mleft_local_in,
  tapa::ostream<int> & ptr_local,
  int         pe_col_id, 
  int BLEN
){
#pragma HLS INLINE off
	int M = (pe_col_id + 1) * GAP_SCORE;
	int Mleft = (pe_col_id) * GAP_SCORE;

	int SEQA_in_data = SEQA_local.read();

	[[tapa::pipeline(1)]]
	for (int counter = 0; counter < BLEN; counter++){
		int SEQB_in_data = SEQB_local.read();

		int M_prev = M;
		int Mleft_prev = Mleft;
		Mleft = Mleft_local_in.read();

		int score;
		if( SEQA_in_data == SEQB_in_data ){
			score = MATCH_SCORE;
		}
		else{
			score = MISMATCH_SCORE;
		}

		int up_left = Mleft_prev + score;
		int up = M + GAP_SCORE;
		int left = Mleft + GAP_SCORE;

		int max = MAX(up_left, MAX(up, left));
		M = max;

		int ptr;
		if(max == left){
			ptr = SKIPB;
		} else{
			if(max == up){
				ptr = SKIPA;
			} else{
				ptr = ALIGN;
			} 
		}
		ptr_local.write(ptr);
	}
}

void ptr_datacollect_tail(
  tapa::istream<int> & ptr_collect,
  tapa::ostream<int> & ptr_trans_out,
  int         pe_col_id, 
  int BLEN
){
#pragma HLS INLINE off
	[[tapa::pipeline(1)]]
	for (int row_counter = 0; row_counter < BLEN; row_counter++){
		int ptr_in_data = ptr_collect.read();
		ptr_trans_out.write(ptr_in_data);
	}
}

void ptr_datacollect(
  tapa::istream<int> & ptr_collect,
  tapa::istream<int> & ptr_trans_in,
  tapa::ostream<int> & ptr_trans_out,
  int         pe_col_id,
  int BLEN
){
#pragma HLS INLINE off
	for (int row_counter = 0; row_counter < BLEN; row_counter++){
		[[tapa::pipeline(1)]]
		for (int col_counter = 0; col_counter < pe_col_id+1; col_counter++){
			int ptr_in_data;
			if( col_counter == pe_col_id ){
				ptr_in_data = ptr_collect.read();
			}
			else{
				ptr_in_data = ptr_trans_in.read();
			}
			ptr_trans_out.write(ptr_in_data);
		}
	}
}

void ptr_datacollect_head(
  tapa::mmap<int> ptr,
  tapa::istream<int> & ptr_trans_in,
  int ALEN,
  int BLEN
){
#pragma HLS INLINE off
	for (int row_counter = 0; row_counter < BLEN; row_counter++){
		[[tapa::pipeline(1)]]
		for (int col_counter = 0; col_counter < ALEN; col_counter++){
			int ptr_in_data = ptr_trans_in.read();
			ptr[(row_counter+1)*(ALEN+1)+col_counter+1] = ptr_in_data;
			//printf("index:%d data:%d\n", (row_counter+1)*(ALEN+1)+col_counter+1, ptr_in_data);
		}
	}
}

void nw(
  tapa::mmap<const int> SEQA,
  tapa::mmap<const int> SEQB,
  tapa::mmap<int> ptr,
  int pe_num,
  int ALEN,
  int BLEN
){
	tapa::stream<int,2> SEQA_fifo0_transfer;
	tapa::stream<int,2> SEQA_fifo1_transfer;
	tapa::stream<int,2> SEQA_fifo2_transfer;
	tapa::stream<int,2> SEQA_fifo3_transfer;
	tapa::stream<int,2> SEQA_fifo4_transfer;
	tapa::stream<int,2> SEQA_fifo5_transfer;
	tapa::stream<int,2> SEQA_fifo6_transfer;
	tapa::stream<int,2> SEQA_fifo7_transfer;

  	tapa::stream<int,5> SEQA_fifo0_feed;
  	tapa::stream<int,5> SEQB_fifo0_feed;
	tapa::stream<int,5> SEQA_fifo1_feed;
	tapa::stream<int,5> SEQB_fifo1_feed;
	tapa::stream<int,5> SEQA_fifo2_feed;
	tapa::stream<int,5> SEQB_fifo2_feed;
	tapa::stream<int,5> SEQA_fifo3_feed;
	tapa::stream<int,5> SEQB_fifo3_feed;
	tapa::stream<int,5> SEQA_fifo4_feed;
	tapa::stream<int,5> SEQB_fifo4_feed;
	tapa::stream<int,5> SEQA_fifo5_feed;
	tapa::stream<int,5> SEQB_fifo5_feed;
	tapa::stream<int,5> SEQA_fifo6_feed;
	tapa::stream<int,5> SEQB_fifo6_feed;
	tapa::stream<int,5> SEQA_fifo7_feed;
	tapa::stream<int,5> SEQB_fifo7_feed;


  	tapa::stream<int,2> SEQA_fifo0_local;
  	tapa::stream<int,2> SEQB_fifo0_local;
	tapa::stream<int,2> SEQA_fifo1_local;
	tapa::stream<int,2> SEQB_fifo1_local;
	tapa::stream<int,2> SEQA_fifo2_local;
	tapa::stream<int,2> SEQB_fifo2_local;
	tapa::stream<int,2> SEQA_fifo3_local;
	tapa::stream<int,2> SEQB_fifo3_local;
	tapa::stream<int,2> SEQA_fifo4_local;
	tapa::stream<int,2> SEQB_fifo4_local;
	tapa::stream<int,2> SEQA_fifo5_local;
	tapa::stream<int,2> SEQB_fifo5_local;
	tapa::stream<int,2> SEQA_fifo6_local;
	tapa::stream<int,2> SEQB_fifo6_local;
	tapa::stream<int,2> SEQA_fifo7_local;
	tapa::stream<int,2> SEQB_fifo7_local;

	tapa::stream<int,2> Mleft_fifo0_local;
	tapa::stream<int,2> Mleft_fifo1_local;
	tapa::stream<int,2> Mleft_fifo2_local;
	tapa::stream<int,2> Mleft_fifo3_local;
	tapa::stream<int,2> Mleft_fifo4_local;
	tapa::stream<int,2> Mleft_fifo5_local;
	tapa::stream<int,2> Mleft_fifo6_local;

  	tapa::stream<int,5> ptr_fifo0_local;
	tapa::stream<int,5> ptr_fifo1_local;
	tapa::stream<int,5> ptr_fifo2_local;
	tapa::stream<int,5> ptr_fifo3_local;
	tapa::stream<int,5> ptr_fifo4_local;
	tapa::stream<int,5> ptr_fifo5_local;
	tapa::stream<int,5> ptr_fifo6_local;
	tapa::stream<int,5> ptr_fifo7_local;

  	tapa::stream<int,5> ptr_fifo0_collect;
	tapa::stream<int,5> ptr_fifo1_collect;
	tapa::stream<int,5> ptr_fifo2_collect;
	tapa::stream<int,5> ptr_fifo3_collect;
	tapa::stream<int,5> ptr_fifo4_collect;
	tapa::stream<int,5> ptr_fifo5_collect;
	tapa::stream<int,5> ptr_fifo6_collect;
	tapa::stream<int,5> ptr_fifo7_collect;

  	tapa::stream<int,5> ptr_fifo0_transfer;
	tapa::stream<int,5> ptr_fifo1_transfer;
	tapa::stream<int,5> ptr_fifo2_transfer;
	tapa::stream<int,5> ptr_fifo3_transfer;
	tapa::stream<int,5> ptr_fifo4_transfer;
	tapa::stream<int,5> ptr_fifo5_transfer;
	tapa::stream<int,5> ptr_fifo6_transfer;
	tapa::stream<int,5> ptr_fifo7_transfer;

	
tapa::task()
      .invoke(
      SEQA_datafeed_head,
        SEQA,
        SEQA_fifo0_transfer,
	ALEN
      )
      .invoke(
      SEQA_datafeed,
        SEQA_fifo0_transfer,  
        SEQA_fifo1_transfer,  
        SEQA_fifo0_feed,  
        0,
        pe_num
      )
      .invoke(
      SEQA_datafeed,
        SEQA_fifo1_transfer,  
        SEQA_fifo2_transfer,  
        SEQA_fifo1_feed,  
        1,
        pe_num
      )
      .invoke(
      SEQA_datafeed,
        SEQA_fifo2_transfer,  
        SEQA_fifo3_transfer,  
        SEQA_fifo2_feed,  
        2,
        pe_num
      )
      .invoke(
      SEQA_datafeed,
        SEQA_fifo3_transfer,  
        SEQA_fifo4_transfer,  
        SEQA_fifo3_feed,  
        3,
        pe_num
      )
      .invoke(
      SEQA_datafeed,
        SEQA_fifo4_transfer,  
        SEQA_fifo5_transfer,  
        SEQA_fifo4_feed,  
        4,
        pe_num
      )
      .invoke(
      SEQA_datafeed,
        SEQA_fifo5_transfer,  
        SEQA_fifo6_transfer,  
        SEQA_fifo5_feed,  
        5,
        pe_num
      )
      .invoke(
      SEQA_datafeed,
        SEQA_fifo6_transfer,  
        SEQA_fifo7_transfer,  
        SEQA_fifo6_feed,  
        6,
        pe_num
      )
      .invoke(
      SEQA_datafeed_tail,
        SEQA_fifo7_transfer,  
        SEQA_fifo7_feed,  
        7
      )

      .invoke(
      SEQB_datafeed_head,
        SEQB,
        SEQB_fifo0_feed,
	BLEN
      )


      .invoke(
      op_transfer,
        SEQA_fifo0_feed,  
        SEQB_fifo0_feed,  
        SEQB_fifo1_feed,  
        SEQA_fifo0_local,  
        SEQB_fifo0_local,
	BLEN
      )
      .invoke(
      op_transfer,
        SEQA_fifo1_feed,  
        SEQB_fifo1_feed,  
        SEQB_fifo2_feed,  
        SEQA_fifo1_local,  
        SEQB_fifo1_local,
	BLEN
      )
      .invoke(
      op_transfer,
        SEQA_fifo2_feed,  
        SEQB_fifo2_feed,  
        SEQB_fifo3_feed,  
        SEQA_fifo2_local,  
        SEQB_fifo2_local,
	BLEN
      )
      .invoke(
      op_transfer,
        SEQA_fifo3_feed,  
        SEQB_fifo3_feed,  
        SEQB_fifo4_feed,  
        SEQA_fifo3_local,  
        SEQB_fifo3_local,
	BLEN
      )
      .invoke(
      op_transfer,
        SEQA_fifo4_feed,  
        SEQB_fifo4_feed,  
        SEQB_fifo5_feed,  
        SEQA_fifo4_local,  
        SEQB_fifo4_local,
	BLEN
      )
      .invoke(
      op_transfer,
        SEQA_fifo5_feed,  
        SEQB_fifo5_feed,  
        SEQB_fifo6_feed,  
        SEQA_fifo5_local,  
        SEQB_fifo5_local,
	BLEN
      )
      .invoke(
      op_transfer,
        SEQA_fifo6_feed,  
        SEQB_fifo6_feed,  
        SEQB_fifo7_feed,  
        SEQA_fifo6_local,  
        SEQB_fifo6_local,
	BLEN
      )
      .invoke(
      op_transfer_tail,
        SEQA_fifo7_feed,  
        SEQB_fifo7_feed,  
        SEQA_fifo7_local,  
        SEQB_fifo7_local,
	BLEN
      )

      .invoke(
      compute_head,
        SEQA_fifo0_local,  
        SEQB_fifo0_local,  
        Mleft_fifo0_local,  
        ptr_fifo0_local,  
        0,
	BLEN
      )
      .invoke(
      compute,
        SEQA_fifo1_local,  
        SEQB_fifo1_local,  
        Mleft_fifo0_local,  
        Mleft_fifo1_local,  
        ptr_fifo1_local,  
        1,
	BLEN
      )
      .invoke(
      compute,
        SEQA_fifo2_local,  
        SEQB_fifo2_local,  
        Mleft_fifo1_local,  
        Mleft_fifo2_local,  
        ptr_fifo2_local,  
        2,
	BLEN
      )
      .invoke(
      compute,
        SEQA_fifo3_local,  
        SEQB_fifo3_local,  
        Mleft_fifo2_local,  
        Mleft_fifo3_local,  
        ptr_fifo3_local,  
        3,
	BLEN
      )
      .invoke(
      compute,
        SEQA_fifo4_local,  
        SEQB_fifo4_local,  
        Mleft_fifo3_local,  
        Mleft_fifo4_local,  
        ptr_fifo4_local,  
        4,
	BLEN
      )
      .invoke(
      compute,
        SEQA_fifo5_local,  
        SEQB_fifo5_local,  
        Mleft_fifo4_local,  
        Mleft_fifo5_local,  
        ptr_fifo5_local,  
        5,
	BLEN
      )
      .invoke(
      compute,
        SEQA_fifo6_local,  
        SEQB_fifo6_local,  
        Mleft_fifo5_local,  
        Mleft_fifo6_local,  
        ptr_fifo6_local,  
        6,
	BLEN
      )
      .invoke(
      compute_tail,
        SEQA_fifo7_local,  
        SEQB_fifo7_local,  
        Mleft_fifo6_local,  
        ptr_fifo7_local,  
        7,
	BLEN
      )

      .invoke(
      res_transfer,
        ptr_fifo0_local,  
        ptr_fifo0_collect,
	BLEN
      )
      .invoke(
      res_transfer,
        ptr_fifo1_local,  
        ptr_fifo1_collect,
	BLEN  
      )
      .invoke(
      res_transfer,
        ptr_fifo2_local,  
        ptr_fifo2_collect,
	BLEN  
      )
      .invoke(
      res_transfer,
        ptr_fifo3_local,  
        ptr_fifo3_collect,
	BLEN  
      )
      .invoke(
      res_transfer,
        ptr_fifo4_local,  
        ptr_fifo4_collect,
	BLEN  
      )
      .invoke(
      res_transfer,
        ptr_fifo5_local,  
        ptr_fifo5_collect,
	BLEN  
      )
      .invoke(
      res_transfer,
        ptr_fifo6_local,  
        ptr_fifo6_collect,
	BLEN  
      )
      .invoke(
      res_transfer,
        ptr_fifo7_local,  
        ptr_fifo7_collect,
	BLEN  
      )

      .invoke(
      ptr_datacollect_tail,
        ptr_fifo0_collect,  
        ptr_fifo0_transfer,  
        0,
	BLEN
      )
      .invoke(
      ptr_datacollect,
        ptr_fifo1_collect,  
        ptr_fifo0_transfer,  
        ptr_fifo1_transfer,  
        1,
	BLEN
      )
      .invoke(
      ptr_datacollect,
        ptr_fifo2_collect,  
        ptr_fifo1_transfer,  
        ptr_fifo2_transfer,  
        2,
	BLEN
      )
      .invoke(
      ptr_datacollect,
        ptr_fifo3_collect,  
        ptr_fifo2_transfer,  
        ptr_fifo3_transfer,  
        3,
	BLEN
      )
      .invoke(
      ptr_datacollect,
        ptr_fifo4_collect,  
        ptr_fifo3_transfer,  
        ptr_fifo4_transfer,  
        4,
	BLEN
      )
      .invoke(
      ptr_datacollect,
        ptr_fifo5_collect,  
        ptr_fifo4_transfer,  
        ptr_fifo5_transfer,  
        5,
	BLEN
      )
      .invoke(
      ptr_datacollect,
        ptr_fifo6_collect,  
        ptr_fifo5_transfer,  
        ptr_fifo6_transfer,  
        6,
	BLEN
      )
      .invoke(
      ptr_datacollect,
        ptr_fifo7_collect,  
        ptr_fifo6_transfer,  
        ptr_fifo7_transfer,  
        7,
	BLEN
      )
      .invoke(
      ptr_datacollect_head,
        ptr,  
        ptr_fifo7_transfer,
	ALEN,
	BLEN
      );
}

