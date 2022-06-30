#include <tapa.h>

using tapa::detach;
using tapa::join;

#include "params.h"

void data_feederA_head(
	tapa::mmap<const DTYPE> A,
	//int A[I][K],
	tapa::ostream<DTYPE> & fifo_Aout,
	tapa::ostream<DTYPE> & fifo_Alocal,
	const unsigned int num,
	const unsigned int ii,
	const unsigned int kk
){
#pragma HLS INLINE off  
	for(short n = 0; n < num; n++ ){
		for(short k = 0; k < kk; k++){
			[[tapa::pipeline(1)]]
			for(short i = 0; i < ii; i++){
				DTYPE A_temp = A[(n*kk+k)*ii+i];
				fifo_Aout.write(A_temp);
				fifo_Alocal.write(A_temp);
			}
		}
	}
}

void data_feederA(
	tapa::istream<DTYPE> & fifo_Ain,
	tapa::ostream<DTYPE> & fifo_Aout,
	tapa::ostream<DTYPE> & fifo_Alocal,
	const unsigned int num,
	const unsigned int ii,
	const unsigned int kk
){
#pragma HLS INLINE off  
	for(short n = 0; n < num; n++ ){
		for(short k = 0; k < kk; k++){
			[[tapa::pipeline(1)]]
			for(short i = 0; i < ii; i++){
				DTYPE A_temp = fifo_Ain.read();
				fifo_Aout.write(A_temp);
				fifo_Alocal.write(A_temp);
			}
		}
	}
}

void data_feederA_tail(
	tapa::istream<DTYPE> & fifo_Ain,
	tapa::ostream<DTYPE> & fifo_Alocal,
	const unsigned int num,
	const unsigned int ii,
	const unsigned int kk
){
#pragma HLS INLINE off  
	for(short n = 0; n < num; n++ ){
		for(short k = 0; k < kk; k++){
			[[tapa::pipeline(1)]]
			for(short i = 0; i < ii; i++){
				DTYPE A_temp = fifo_Ain.read();
				fifo_Alocal.write(A_temp);
			}
		}
	}
}

void data_feederB_head(
	tapa::mmap<const DTYPE> B,
	//int B[K][J],
	tapa::ostream<DTYPE> &fifo_Bout,
	tapa::ostream<DTYPE> &fifo_Blocal,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int kk
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		for(short k = 0; k < kk; k++){
			[[tapa::pipeline(1)]]
			for(short j = 0; j < J; j++){
				DTYPE B_temp = B[(n*kk+k)*J+j];
				if( j == pe_id ){
					fifo_Blocal.write(B_temp);
				}
				else{
					fifo_Bout.write(B_temp);
				}
			}
		}
	}
}

void data_feederB(
	tapa::istream<DTYPE> &fifo_Bin,
	tapa::ostream<DTYPE> &fifo_Bout,
	tapa::ostream<DTYPE> &fifo_Blocal,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int kk
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		for(short k = 0; k < kk; k++){
			[[tapa::pipeline(1)]]
			for(short j = pe_id; j < J; j++){
				int B_temp = fifo_Bin.read();
				if( j == pe_id ){
					fifo_Blocal.write(B_temp);
				}
				else{
					fifo_Bout.write(B_temp);
				}
			}
		}
	}
}

void data_feederB_tail(
	tapa::istream<DTYPE> &fifo_Bin,
	tapa::ostream<DTYPE> &fifo_Blocal,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int kk
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short k = 0; k < kk; k++){
			DTYPE B_temp = fifo_Bin.read();
			fifo_Blocal.write(B_temp);
		}
	}
}

void PE(
	tapa::istream<DTYPE> &fifo_Alocal,
	tapa::istream<DTYPE> &fifo_Blocal,
	tapa::ostream<DTYPE> &fifo_Clocal,
	const unsigned int num,
	const unsigned int ii,
	const unsigned int kk
){
#pragma HLS INLINE off
	DTYPE2 C_local[MAX_I] = {0};

	// local computation
	for(short n = 0; n < num; n++ ){
		for(short k = 0; k < kk; k++){
			DTYPE2 opB = fifo_Blocal.read();
			[[tapa::pipeline(1)]]
			for(short i = 0; i < ii; i++){
				DTYPE2 opA = fifo_Alocal.read();
				C_local[i] += opA * opB;   
				//printf("pe %d (n:%d,k:%d,i:%d): %d x %d = %d\n", pe_id, n, k, i, opA, opB, C_local[i]);
			}
		}

		[[tapa::pipeline(1)]]
		for(short i = 0; i < ii; i++){
			fifo_Clocal.write(C_local[i]);
			//printf("[Cwrite] pe %d (i:%d): %d\n", pe_id, i, C_local[i]);
			C_local[i] = 0;
		}
	}
}

void data_collectorC_head(
	tapa::istream<DTYPE> &fifo_Clocal,
	tapa::ostream<DTYPE> &fifo_Cout,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int ii
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short i = 0; i < ii; i++){
			DTYPE C_temp = fifo_Clocal.read();
			fifo_Cout.write(C_temp);
		}
	}
}

void data_collectorC(
	tapa::istream<DTYPE> &fifo_Clocal,
	tapa::istream<DTYPE> &fifo_Cin,
	tapa::ostream<DTYPE> &fifo_Cout,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int ii
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		for(short j = 0; j < pe_id; j++){
			[[tapa::pipeline(1)]]
			for(short i = 0; i < ii; i++){
				DTYPE C_temp = fifo_Cin.read();
				fifo_Cout.write(C_temp);
			}
		}
		[[tapa::pipeline(1)]]
		for(short i = 0; i < ii; i++){
			DTYPE C_temp = fifo_Clocal.read();
			fifo_Cout.write(C_temp);
		}
	}
}

void data_collectorC_tail(
	tapa::mmap<DTYPE> C,
	//int C[I][J],
	tapa::istream<DTYPE> &fifo_Clocal,
	tapa::istream<DTYPE> &fifo_Cin,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int ii
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		for(short j = 0; j < J; j++){
			[[tapa::pipeline(1)]]
			for(short i = 0; i < ii; i++){
				DTYPE C_temp;
				if( j == pe_id ){
					C_temp = fifo_Clocal.read();
				}
				else{
					C_temp = fifo_Cin.read();
				}
				C[(n*J+j)*ii+i] = C_temp;
			}
		}
	}
}

void mm(
  tapa::mmap<const DTYPE> A,
  tapa::mmap<const DTYPE> B,
  tapa::mmap<DTYPE> C,
  //int A[I][K],
  //int B[K][J],
  //int C[I][J],
  int num,
  int ii,
  int kk
){
//#pragma HLS dataflow
	tapa::stream<DTYPE,2> fifo_A0 ;
	tapa::stream<DTYPE,2> fifo_A1 ;
	tapa::stream<DTYPE,2> fifo_A2 ;
	tapa::stream<DTYPE,2> fifo_A3 ;
	tapa::stream<DTYPE,2> fifo_A4 ;
	tapa::stream<DTYPE,2> fifo_A5 ;
	tapa::stream<DTYPE,2> fifo_A6 ;
	tapa::stream<DTYPE,2> fifo_A7 ;
	tapa::stream<DTYPE,2> fifo_A8 ;
	tapa::stream<DTYPE,2> fifo_A9 ;
	tapa::stream<DTYPE,2> fifo_A10;
	tapa::stream<DTYPE,2> fifo_A11;
	tapa::stream<DTYPE,2> fifo_A12;
	tapa::stream<DTYPE,2> fifo_A13;
	tapa::stream<DTYPE,2> fifo_A14;

	tapa::stream<DTYPE,2> fifo_B0 ;
	tapa::stream<DTYPE,2> fifo_B1 ;
	tapa::stream<DTYPE,2> fifo_B2 ;
	tapa::stream<DTYPE,2> fifo_B3 ;
	tapa::stream<DTYPE,2> fifo_B4 ;
	tapa::stream<DTYPE,2> fifo_B5 ;
	tapa::stream<DTYPE,2> fifo_B6 ;
	tapa::stream<DTYPE,2> fifo_B7 ;
	tapa::stream<DTYPE,2> fifo_B8 ;
	tapa::stream<DTYPE,2> fifo_B9 ;
	tapa::stream<DTYPE,2> fifo_B10;
	tapa::stream<DTYPE,2> fifo_B11;
	tapa::stream<DTYPE,2> fifo_B12;
	tapa::stream<DTYPE,2> fifo_B13;
	tapa::stream<DTYPE,2> fifo_B14;

	tapa::stream<DTYPE,2> fifo_C0;
	tapa::stream<DTYPE,2> fifo_C1;
	tapa::stream<DTYPE,2> fifo_C2;
	tapa::stream<DTYPE,2> fifo_C3;
	tapa::stream<DTYPE,2> fifo_C4 ;
	tapa::stream<DTYPE,2> fifo_C5 ;
	tapa::stream<DTYPE,2> fifo_C6 ;
	tapa::stream<DTYPE,2> fifo_C7 ;
	tapa::stream<DTYPE,2> fifo_C8 ;
	tapa::stream<DTYPE,2> fifo_C9 ;
	tapa::stream<DTYPE,2> fifo_C10;
	tapa::stream<DTYPE,2> fifo_C11;
	tapa::stream<DTYPE,2> fifo_C12;
	tapa::stream<DTYPE,2> fifo_C13;
	tapa::stream<DTYPE,2> fifo_C14;

	tapa::stream<DTYPE,2> fifo_Alocal0 ;
	tapa::stream<DTYPE,2> fifo_Alocal1 ;
	tapa::stream<DTYPE,2> fifo_Alocal2 ;
	tapa::stream<DTYPE,2> fifo_Alocal3 ;
	tapa::stream<DTYPE,2> fifo_Alocal4 ;
	tapa::stream<DTYPE,2> fifo_Alocal5 ;
	tapa::stream<DTYPE,2> fifo_Alocal6 ;
	tapa::stream<DTYPE,2> fifo_Alocal7 ;
	tapa::stream<DTYPE,2> fifo_Alocal8 ;
	tapa::stream<DTYPE,2> fifo_Alocal9 ;
	tapa::stream<DTYPE,2> fifo_Alocal10;
	tapa::stream<DTYPE,2> fifo_Alocal11;
	tapa::stream<DTYPE,2> fifo_Alocal12;
	tapa::stream<DTYPE,2> fifo_Alocal13;
	tapa::stream<DTYPE,2> fifo_Alocal14;
	tapa::stream<DTYPE,2> fifo_Alocal15;

	tapa::stream<DTYPE,2> fifo_Blocal0 ;
	tapa::stream<DTYPE,2> fifo_Blocal1 ;
	tapa::stream<DTYPE,2> fifo_Blocal2 ;
	tapa::stream<DTYPE,2> fifo_Blocal3 ;
	tapa::stream<DTYPE,2> fifo_Blocal4 ;
	tapa::stream<DTYPE,2> fifo_Blocal5 ;
	tapa::stream<DTYPE,2> fifo_Blocal6 ;
	tapa::stream<DTYPE,2> fifo_Blocal7 ;
	tapa::stream<DTYPE,2> fifo_Blocal8 ;
	tapa::stream<DTYPE,2> fifo_Blocal9 ;
	tapa::stream<DTYPE,2> fifo_Blocal10;
	tapa::stream<DTYPE,2> fifo_Blocal11;
	tapa::stream<DTYPE,2> fifo_Blocal12;
	tapa::stream<DTYPE,2> fifo_Blocal13;
	tapa::stream<DTYPE,2> fifo_Blocal14;
	tapa::stream<DTYPE,2> fifo_Blocal15;

	tapa::stream<DTYPE,512> fifo_Clocal0;
	tapa::stream<DTYPE,512> fifo_Clocal1;
	tapa::stream<DTYPE,512> fifo_Clocal2;
	tapa::stream<DTYPE,512> fifo_Clocal3;
	tapa::stream<DTYPE,512> fifo_Clocal4 ;
	tapa::stream<DTYPE,512> fifo_Clocal5 ;
	tapa::stream<DTYPE,512> fifo_Clocal6 ;
	tapa::stream<DTYPE,512> fifo_Clocal7 ;
	tapa::stream<DTYPE,512> fifo_Clocal8 ;
	tapa::stream<DTYPE,512> fifo_Clocal9 ;
	tapa::stream<DTYPE,512> fifo_Clocal10;
	tapa::stream<DTYPE,512> fifo_Clocal11;
	tapa::stream<DTYPE,512> fifo_Clocal12;
	tapa::stream<DTYPE,512> fifo_Clocal13;
	tapa::stream<DTYPE,512> fifo_Clocal14;
	tapa::stream<DTYPE,512> fifo_Clocal15;

tapa::task()
  .invoke(data_feederA_head, A, fifo_A0, fifo_Alocal0, num, ii, kk)
  .invoke(data_feederA, fifo_A0 , fifo_A1 , fifo_Alocal1 , num, ii, kk)
  .invoke(data_feederA, fifo_A1 , fifo_A2 , fifo_Alocal2 , num, ii, kk)
  .invoke(data_feederA, fifo_A2 , fifo_A3 , fifo_Alocal3 , num, ii, kk)
  .invoke(data_feederA, fifo_A3 , fifo_A4 , fifo_Alocal4 , num, ii, kk)
  .invoke(data_feederA, fifo_A4 , fifo_A5 , fifo_Alocal5 , num, ii, kk)
  .invoke(data_feederA, fifo_A5 , fifo_A6 , fifo_Alocal6 , num, ii, kk)
  .invoke(data_feederA, fifo_A6 , fifo_A7 , fifo_Alocal7 , num, ii, kk)
  .invoke(data_feederA, fifo_A7 , fifo_A8 , fifo_Alocal8 , num, ii, kk)
  .invoke(data_feederA, fifo_A8 , fifo_A9 , fifo_Alocal9 , num, ii, kk)
  .invoke(data_feederA, fifo_A9 , fifo_A10, fifo_Alocal10, num, ii, kk)
  .invoke(data_feederA, fifo_A10, fifo_A11, fifo_Alocal11, num, ii, kk)
  .invoke(data_feederA, fifo_A11, fifo_A12, fifo_Alocal12, num, ii, kk)
  .invoke(data_feederA, fifo_A12, fifo_A13, fifo_Alocal13, num, ii, kk)
  .invoke(data_feederA, fifo_A13, fifo_A14, fifo_Alocal14, num, ii, kk)
  .invoke(data_feederA_tail, fifo_A14, fifo_Alocal15, num, ii, kk)

  .invoke(data_feederB_head, B, fifo_B0, fifo_Blocal0, 0, num, kk)
  .invoke(data_feederB, fifo_B0 , fifo_B1 , fifo_Blocal1 , 1 , num, kk)
  .invoke(data_feederB, fifo_B1 , fifo_B2 , fifo_Blocal2 , 2 , num, kk)
  .invoke(data_feederB, fifo_B2 , fifo_B3 , fifo_Blocal3 , 3 , num, kk)
  .invoke(data_feederB, fifo_B3 , fifo_B4 , fifo_Blocal4 , 4 , num, kk)
  .invoke(data_feederB, fifo_B4 , fifo_B5 , fifo_Blocal5 , 5 , num, kk)
  .invoke(data_feederB, fifo_B5 , fifo_B6 , fifo_Blocal6 , 6 , num, kk)
  .invoke(data_feederB, fifo_B6 , fifo_B7 , fifo_Blocal7 , 7 , num, kk)
  .invoke(data_feederB, fifo_B7 , fifo_B8 , fifo_Blocal8 , 8 , num, kk)
  .invoke(data_feederB, fifo_B8 , fifo_B9 , fifo_Blocal9 , 9 , num, kk)
  .invoke(data_feederB, fifo_B9 , fifo_B10, fifo_Blocal10, 10, num, kk)
  .invoke(data_feederB, fifo_B10, fifo_B11, fifo_Blocal11, 11, num, kk)
  .invoke(data_feederB, fifo_B11, fifo_B12, fifo_Blocal12, 12, num, kk)
  .invoke(data_feederB, fifo_B12, fifo_B13, fifo_Blocal13, 13, num, kk)
  .invoke(data_feederB, fifo_B13, fifo_B14, fifo_Blocal14, 14, num, kk)
  .invoke(data_feederB_tail, fifo_B14, fifo_Blocal15, 15, num, kk)

  .invoke(PE, fifo_Alocal0 , fifo_Blocal0 , fifo_Clocal0 , num, ii, kk)
  .invoke(PE, fifo_Alocal1 , fifo_Blocal1 , fifo_Clocal1 , num, ii, kk)
  .invoke(PE, fifo_Alocal2 , fifo_Blocal2 , fifo_Clocal2 , num, ii, kk)
  .invoke(PE, fifo_Alocal3 , fifo_Blocal3 , fifo_Clocal3 , num, ii, kk)
  .invoke(PE, fifo_Alocal4 , fifo_Blocal4 , fifo_Clocal4 , num, ii, kk)
  .invoke(PE, fifo_Alocal5 , fifo_Blocal5 , fifo_Clocal5 , num, ii, kk)
  .invoke(PE, fifo_Alocal6 , fifo_Blocal6 , fifo_Clocal6 , num, ii, kk)
  .invoke(PE, fifo_Alocal7 , fifo_Blocal7 , fifo_Clocal7 , num, ii, kk)
  .invoke(PE, fifo_Alocal8 , fifo_Blocal8 , fifo_Clocal8 , num, ii, kk)
  .invoke(PE, fifo_Alocal9 , fifo_Blocal9 , fifo_Clocal9 , num, ii, kk)
  .invoke(PE, fifo_Alocal10, fifo_Blocal10, fifo_Clocal10, num, ii, kk)
  .invoke(PE, fifo_Alocal11, fifo_Blocal11, fifo_Clocal11, num, ii, kk)
  .invoke(PE, fifo_Alocal12, fifo_Blocal12, fifo_Clocal12, num, ii, kk)
  .invoke(PE, fifo_Alocal13, fifo_Blocal13, fifo_Clocal13, num, ii, kk)
  .invoke(PE, fifo_Alocal14, fifo_Blocal14, fifo_Clocal14, num, ii, kk)
  .invoke(PE, fifo_Alocal15, fifo_Blocal15, fifo_Clocal15, num, ii, kk)

  .invoke(data_collectorC_head, fifo_Clocal0 , fifo_C0 , 0, num, ii)
  .invoke(data_collectorC, fifo_Clocal1 , fifo_C0 , fifo_C1 , 1 , num, ii)
  .invoke(data_collectorC, fifo_Clocal2 , fifo_C1 , fifo_C2 , 2 , num, ii)
  .invoke(data_collectorC, fifo_Clocal3 , fifo_C2 , fifo_C3 , 3 , num, ii)
  .invoke(data_collectorC, fifo_Clocal4 , fifo_C3 , fifo_C4 , 4 , num, ii)
  .invoke(data_collectorC, fifo_Clocal5 , fifo_C4 , fifo_C5 , 5 , num, ii)
  .invoke(data_collectorC, fifo_Clocal6 , fifo_C5 , fifo_C6 , 6 , num, ii)
  .invoke(data_collectorC, fifo_Clocal7 , fifo_C6 , fifo_C7 , 7 , num, ii)
  .invoke(data_collectorC, fifo_Clocal8 , fifo_C7 , fifo_C8 , 8 , num, ii)
  .invoke(data_collectorC, fifo_Clocal9 , fifo_C8 , fifo_C9 , 9 , num, ii)
  .invoke(data_collectorC, fifo_Clocal10, fifo_C9 , fifo_C10, 10, num, ii)
  .invoke(data_collectorC, fifo_Clocal11, fifo_C10, fifo_C11, 11, num, ii)
  .invoke(data_collectorC, fifo_Clocal12, fifo_C11, fifo_C12, 12, num, ii)
  .invoke(data_collectorC, fifo_Clocal13, fifo_C12, fifo_C13, 13, num, ii)
  .invoke(data_collectorC, fifo_Clocal14, fifo_C13, fifo_C14, 14, num, ii)
  .invoke(data_collectorC_tail, C, fifo_Clocal15, fifo_C14, 15, num, ii)
  ;

}

