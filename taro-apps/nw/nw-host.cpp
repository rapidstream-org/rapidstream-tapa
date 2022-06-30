#include <chrono>
#include <iostream>
#include <vector>

#include <gflags/gflags.h>
#include <tapa.h>

#define ALEN 8
#define BLEN 32//16384//256

#define MATCH_SCORE 1
#define MISMATCH_SCORE -1
#define GAP_SCORE -1

#define ALIGN 1
#define SKIPA 2
#define SKIPB 3

#define MAX(A,B) ( ((A)>(B))?(A):(B) )

using std::clog;
using std::endl;
using std::vector;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");


void nw(
  tapa::mmap<const int> SEQA,
  tapa::mmap<const int> SEQB,
  tapa::mmap<int> ptr,
  int pe_num,
  int ALEN_temp,
  int BLEN_temp

);


void nw_sw(int SEQA[ALEN], int SEQB[BLEN], int M[(ALEN+1)*(BLEN+1)], int ptr[(ALEN+1)*(BLEN+1)])
{
	int score, up_left, up, left, max;
	int row, row_up, r;
	int a_idx, b_idx;
	int a_str_idx, b_str_idx;

	for(a_idx=0; a_idx<(ALEN+1); a_idx++){
		M[a_idx] = a_idx * GAP_SCORE;
	}
	for(b_idx=0; b_idx<(BLEN+1); b_idx++){
		M[b_idx*(ALEN+1)] = b_idx * GAP_SCORE;
	}

	// Matrix filling loop
	for(b_idx=1; b_idx<(BLEN+1); b_idx++){
		for(a_idx=1; a_idx<(ALEN+1); a_idx++){
			if(SEQA[a_idx-1] == SEQB[b_idx-1]){
				score = MATCH_SCORE;
			} else {
				score = MISMATCH_SCORE;
			}

			row_up = (b_idx-1)*(ALEN+1);
			row = (b_idx)*(ALEN+1);

			up_left = M[row_up + (a_idx-1)] + score;
			up      = M[row_up + (a_idx  )] + GAP_SCORE;
			left    = M[row    + (a_idx-1)] + GAP_SCORE;

			max = MAX(up_left, MAX(up, left));

			M[row + a_idx] = max;
			if(max == left){
				ptr[row + a_idx] = SKIPB;
			} else if(max == up){
				ptr[row + a_idx] = SKIPA;
			} else{
				ptr[row + a_idx] = ALIGN;
			}
		}
	}
}

int main(int argc, char* argv[]) {
	gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

	int SEQA_SW[ALEN]; int SEQB_SW[BLEN];
	int M_SW[(ALEN+1)*(BLEN+1)];
	int ptr_SW[(ALEN+1)*(BLEN+1)];

	vector<int> SEQA_HW(ALEN); vector<int> SEQB_HW(BLEN);
	vector<int> ptr_HW((ALEN+1)*(BLEN+1));

	srand(0);

	for( int i = 0 ; i < ALEN ; i++){
		SEQA_SW[i] = rand()%4;
		SEQA_HW[i] = SEQA_SW[i];
		//printf("A[%d]:%d\n",i,SEQA_SW[i]);
	}
	for( int i = 0 ; i < BLEN ; i++){
		SEQB_SW[i] = rand()%4;
		SEQB_HW[i] = SEQB_SW[i];
		//printf("B[%d]:%d\n",i,SEQB_SW[i]);
	}

	for( int i = 0 ; i < (ALEN+1)*(BLEN+1) ; i++){
		M_SW[i] = 0;
		ptr_SW[i] = 0;
		ptr_HW[i] = 0;
	}

	nw_sw(SEQA_SW, SEQB_SW, M_SW, ptr_SW);
	int pe_num = ALEN;
	int ALEN_temp = ALEN;
	int BLEN_temp = BLEN;

	printf("Starting FPGA execution...\n");

	auto start = high_resolution_clock::now();

	tapa::invoke(nw, FLAGS_bitstream, tapa::read_only_mmap<const int>(SEQA_HW),
               tapa::read_only_mmap<const int>(SEQB_HW),
               tapa::write_only_mmap<int>(ptr_HW), pe_num, ALEN_temp, BLEN_temp);

	auto stop = high_resolution_clock::now();
	duration<double> elapsed = stop - start;
	clog << "elapsed time: " << elapsed.count() << " s" << endl;

	int incorrect = 0;
	for( int i = 0 ; i < (ALEN+1)*(BLEN+1) ; i++){
		if( ptr_SW[i] != ptr_HW[i] ){
			incorrect++;
		}
		if( (ALEN+1 <= i && i < ALEN+5) || (ptr_SW[i] != ptr_HW[i]) ){
			printf("(@%d) answer:%d, hw:%d\n", i, ptr_SW[i], ptr_HW[i]);
		}
	}
	printf("incorrect:%d / %d\n",incorrect, (ALEN+1)*(BLEN+1));

}


