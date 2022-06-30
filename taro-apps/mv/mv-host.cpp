#include <tapa.h>
#include <gflags/gflags.h>
#include <chrono>
#include <iostream>
#include <vector>

#include "params.h"
#include "sys/time.h"

using std::vector;
DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

void mv(
  tapa::mmap<tapa::vec_t<int, I>> A,
  tapa::mmap<const int> X,
  tapa::mmap<int> Y,
  int num,
  int jj
  );

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  vector<int> A(NUM*I*J);
  vector<int> X(NUM*J);
  vector<int> Y(NUM*I);
  vector<int> A_hw(NUM*I*J);
  vector<int> X_hw(NUM*J);
  vector<int> Y_hw(NUM*I);

	srand(0);

	printf("Initializing... ");
	for( int n = 0 ; n < NUM; n++ ){
		for( int i = 0 ; i < I; i++ ){
			for( int j = 0 ; j < J; j++ ){
				A[(n*I+i)*J+j] = (int)rand()%8;
				A_hw[(n*J+j)*I+i] = A[(n*I+i)*J+j];
			}
		}
	}

	for( int n = 0 ; n < NUM; n++ ){
		for( int j = 0 ; j < J; j++ ){
			X[n*J+j] = (int)rand()%8;
			X_hw[n*J+j] = X[n*J+j];
		}
	}
	printf("done\n");

	printf("Computing mv in SW... ");
	for( int n = 0 ; n < NUM; n++ ){
		for (int i = 0; i < I; i++){
			Y_hw[n*I+i] = 0;
			Y[n*I+i] = 0;
			for (int j = 0; j < J; j++){
				Y[n*I+i] += A[(n*I+i)*J+j] * X[n*J+j];
			}
		}
	}
	printf("done\n");

	struct  timeval ts, te, td;
	float tser;
	gettimeofday(&ts, NULL);

	tapa::invoke(mv, FLAGS_bitstream, 
	       tapa::read_only_mmap<int>(A_hw).vectorized<I>(),
               tapa::read_only_mmap<const int>(X_hw),
               tapa::write_only_mmap<int>(Y_hw), NUM, J);


	gettimeofday(&te, NULL);
	timersub(&ts, &te, &td);
	tser = fabs(td.tv_sec+(int)td.tv_usec/1000000.0);
	printf("sim time:%f sec\n", tser);

	int thres = 0;
	int err_cnt = 0;
	for( int n = 0 ; n < NUM; n++ ){
		for( int i = 0 ; i < I; i++ ){
			int diff = (Y[n*I+i] - Y_hw[n*I+i]);
			if (diff > thres){
				err_cnt++;
				printf("(n:%d,i:%d) sw - %d hw - %d\n", n, i, Y[n*I+i], Y_hw[n*I+i]);
			}
		}
	}

	if (err_cnt){
		printf("TEST FAILED! %d ERRORS!\n", err_cnt);
	} else {
		printf("TEST PASSED!\n");
	}
}
