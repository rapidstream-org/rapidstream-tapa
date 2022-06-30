#include <tapa.h>
#include <gflags/gflags.h>
#include <chrono>
#include <iostream>
#include <vector>

#include "params.h"
#include "sys/time.h"

using std::vector;
DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

void mm(
  tapa::mmap<const DTYPE> A,
  tapa::mmap<const DTYPE> B,
  tapa::mmap<DTYPE> C,
  //int A[I][K],
  //int B[K][J],
  //int C[I][J],
  int pe_num,
  int ii,
  int kk
  );

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  vector<DTYPE> A(NUM*I*K);
  vector<DTYPE> B(NUM*K*J);
  vector<DTYPE> C(NUM*I*J);
  vector<DTYPE> A_hw(NUM*I*K);
  vector<DTYPE> B_hw(NUM*K*J);
  vector<DTYPE> C_hw(NUM*I*J);

  //init_random(A, I*K);
  //init_random(B, K*J);

	srand(0);

	printf("Initializing... ");
	for( int n = 0 ; n < NUM; n++ ){
		for( int i = 0 ; i < I; i++ ){
			for( int k = 0 ; k < K; k++ ){
				//A[i][k] = (int)rand() / (RAND_MAX) * 1.0;
				A[(n*I+i)*K+k] = (DTYPE)(rand()%8);
				A_hw[(n*K+k)*I+i] = A[(n*I+i)*K+k];
				//printf("(n:%d,i:%d,k:%d) A:%d\n", n, i, k, A[(n*I+i)*K+k]);
			}
		}
	}

	for( int n = 0 ; n < NUM; n++ ){
		for( int k = 0 ; k < K; k++ ){
			for( int j = 0 ; j < J; j++ ){
				//B[k][j] = (int)rand() / (RAND_MAX) * 1.0;
				B[(n*K+k)*J+j] = (DTYPE)(rand()%8);
				B_hw[(n*K+k)*J+j] = B[(n*K+k)*J+j];
				//printf("(n:%d,k:%d,j:%d) B:%d\n", n, k, j, B[(n*K+k)*J+j]);
			}
		}
	}
	printf("done\n");

	printf("Computing mm in SW... ");
	for( int n = 0 ; n < NUM; n++ ){
		for (int i = 0; i < I; i++){
			for (int j = 0; j < J; j++){
				C_hw[(n*J+j)*I+i] = 0;
				C[(n*I+i)*J+j] = 0;
				for (int k = 0; k < K; k++){
					C[(n*I+i)*J+j] += A[(n*I+i)*K+k] * B[(n*K+k)*J+j];
					//printf("(n:%d,i:%d,j:%d,k:%d) %d x %d = %d\n", n, i, j, k, A[(n*I+i)*K+k], B[(n*K+k)*J+j], C[(n*I+i)*J+j]);
				}
			}
		}
	}
	printf("done\n");

	struct  timeval ts, te, td;
	float tser;
	gettimeofday(&ts, NULL);

	//kernel(A,B,C_hw,J);

	tapa::invoke(mm, FLAGS_bitstream, tapa::read_only_mmap<const DTYPE>(A_hw),
               tapa::read_only_mmap<const DTYPE>(B_hw),
               tapa::write_only_mmap<DTYPE>(C_hw), NUM, I, K);

  //top_kernel(A, B, C_hw, J);

	gettimeofday(&te, NULL);
	timersub(&ts, &te, &td);
	tser = fabs(td.tv_sec+(int)td.tv_usec/1000000.0);
	printf("sim time:%f sec\n", tser);

	DTYPE thres = 0;
	int err_cnt = 0;
	for( int n = 0 ; n < NUM; n++ ){
		for( int i = 0 ; i < I; i++ ){
			for( int j = 0 ; j < J; j++ ){
				DTYPE diff = (C[(n*I+i)*J+j] - C_hw[(n*J+j)*I+i]);
				if (diff > thres){
					err_cnt++;
				printf("(n:%d,i:%d,j:%d) sw - %d hw - %d\n", n, i, j, C[(n*I+i)*J+j], C_hw[(n*J+j)*I+i]);
				}
				// if( C_sw[i][j] > C_temp_max ){C_temp_max = C_sw[i][j];}
				//if( C_sw[i][j] < C_temp_min ){C_temp_min = C_sw[i][j];}
			}
		}
	}

	  //printf("C_temp_max:%d, C_temp_min:%d\n", C_temp_max, C_temp_min);

  //int err_cnt = buf_compare(C_sw, C_hw, I*J, 0.01);

  if (err_cnt){
    printf("TEST FAILED! %d ERRORS!\n", err_cnt);
  } else {
    printf("TEST PASSED!\n");
  }
}
