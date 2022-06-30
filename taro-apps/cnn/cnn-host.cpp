#include <tapa.h>
#include <gflags/gflags.h>
#include <chrono>
#include <iostream>
#include <vector>

#include "params.h"
#include "sys/time.h"

using std::vector;
DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");


void cnn(
	tapa::mmap<const int> I,
	tapa::mmap<tapa::vec_t<int, Tm>> KER,
	tapa::mmap<int> O,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn

);

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

	vector<int> I(N*(R+K-1)*(C+K-1));
	vector<int> KER_sw(M*N*K*K);
	vector<int> KER_hw(M*N*K*K);
	vector<int> O_sw(M*R*C);
	vector<int> O_hw(M*R*C);

	srand(0);

	printf("Initializing... ");
	for(int row = 0; row < R+K-1; row++) {
		for(int col = 0; col < C+K-1; col++) {
			for(int chi = 0; chi < N; chi++) {
				if (row >= (K-1)/2 && row < R+(K-1)/2 && col >= (K-1)/2 && col < C+(K-1)/2) {
					I[row*(C+K-1)*N + col*N + chi] = rand() % 8;
				}
				else {
					I[row*(C+K-1)*N + col*N + chi] = 0;
				}
			}
		}
	}
	// initialize kernel
	for(int cho = 0; cho < M; cho++) {
		for(int chi = 0; chi < N; chi++) {
			for (int ki = 0; ki < K; ki++) {
				for (int kj = 0; kj < K; kj++) {
					int m = cho/Tm;
					int m2 = cho%Tm;
					int K_temp = rand() % 8;
					KER_sw[cho*(N*K*K) + chi*(K*K) + ki*K + kj] = K_temp;
					KER_hw[((m*(K*K) + ki*K + kj)*N+chi)*Tm+m2] = K_temp;
				}
			}
		}
	}
	// initialize output
	for(int row = 0; row < R; row++) {
		for(int col = 0; col < C; col++) {
			for(int cho = 0; cho < M; cho++) {
				O_sw[(row*C + col)*M+cho] = 0;
				O_hw[(row*C + col)*M+cho] = 0;
			}
		}
	}

	printf("done\n");

	printf("Computing CNN in SW... ");
	for(int row = 0; row < R; row++) {
		for(int col = 0; col < C; col++) {
			for(int cho = 0; cho < M; cho++) {
				for(int chi = 0; chi < N; chi++) {
					for (int ki = 0; ki < K; ki++) {
						for (int kj = 0; kj < K; kj++) {
							O_sw[(row*C + col)*M+cho] += KER_sw[cho*(N*K*K) + chi*(K*K) + ki*K + kj] * I[(row+ki)*(C+K-1)*N + (col+kj)*N + chi];
						}
					}
				}
			}
		}
	}
	printf("done\n");

	struct  timeval ts, te, td;
	float tser;
	gettimeofday(&ts, NULL);

	tapa::invoke(cnn, FLAGS_bitstream, 
               tapa::read_only_mmap<const int>(I),
	       tapa::read_only_mmap<int>(KER_hw).vectorized<Tm>(),
               tapa::write_only_mmap<int>(O_hw), R, C, M, N);


	gettimeofday(&te, NULL);
	timersub(&ts, &te, &td);
	tser = fabs(td.tv_sec+(int)td.tv_usec/1000000.0);
	printf("sim time:%f sec\n", tser);

	int err_cnt = 0;
	for(int row = 0; row < R; row++) {
		for(int col = 0; col < C; col++) {
			for(int cho = 0; cho < M; cho++) {
				if(O_sw[(row*C + col)*M+cho] != O_hw[(row*C + col)*M+cho]) {
					err_cnt++;
					if( err_cnt == 1 ){
						printf("cho:%d row:%d col:%d sw:%d hw:%d\n", cho, row, col, O_sw[(row*C + col)*M+cho], O_hw[(row*C + col)*M+cho]);
					}
				}
			}
		}
	}
	if (err_cnt){
		printf("TEST FAILED! %d ERRORS!\n", err_cnt);
	} else {
		printf("TEST PASSED!\n");
	}
}
