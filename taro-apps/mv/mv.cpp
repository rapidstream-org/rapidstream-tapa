#include <tapa.h>

using tapa::detach;
using tapa::join;

#include "params.h"

void data_feederA_head(
	tapa::mmap<tapa::vec_t<int, I>> A,
	tapa::ostream<tapa::vec_t<int, I>> & fifo_Aout,
	tapa::ostream<int> & fifo_Alocal,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int jj
){
#pragma HLS INLINE off  
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			tapa::vec_t<int, I> A_temp = A[n*jj+j];
			fifo_Aout.write(A_temp);
			fifo_Alocal.write(A_temp[pe_id]);
		}
	}
}

void data_feederA(
	tapa::istream<tapa::vec_t<int, I>> & fifo_Ain,
	tapa::ostream<tapa::vec_t<int, I>> & fifo_Aout,
	tapa::ostream<int> & fifo_Alocal,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int jj
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			tapa::vec_t<int, I> A_temp = fifo_Ain.read();
			fifo_Aout.write(A_temp);
			fifo_Alocal.write(A_temp[pe_id]);
		}
	}
}

void data_feederA_tail(
	tapa::istream<tapa::vec_t<int, I>> & fifo_Ain,
	tapa::ostream<int> &fifo_Alocal,
	const unsigned int pe_id,
	const unsigned int num,
	const unsigned int jj
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			tapa::vec_t<int, I> A_temp = fifo_Ain.read();
			fifo_Alocal.write(A_temp[pe_id]);
		}
	}
}

void data_feederX_head(
	tapa::mmap<const int> X,
	tapa::ostream<int> & fifo_Xout,
	tapa::ostream<int> & fifo_Xlocal,
	const unsigned int num,
	const unsigned int jj
){
#pragma HLS INLINE off  
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			int X_temp = X[n*jj+j];
			fifo_Xout.write(X_temp);
			fifo_Xlocal.write(X_temp);
		}
	}
}

void data_feederX(
	tapa::istream<int> & fifo_Xin,
	tapa::ostream<int> & fifo_Xout,
	tapa::ostream<int> & fifo_Xlocal,
	const unsigned int num,
	const unsigned int jj
){
#pragma HLS INLINE off  
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			int X_temp = fifo_Xin.read();
			fifo_Xout.write(X_temp);
			fifo_Xlocal.write(X_temp);
		}
	}
}

void data_feederX_tail(
	tapa::istream<int> & fifo_Xin,
	tapa::ostream<int> & fifo_Xlocal,
	const unsigned int num,
	const unsigned int jj
){
#pragma HLS INLINE off  
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			int X_temp = fifo_Xin.read();
			fifo_Xlocal.write(X_temp);
		}
	}
}

void PE(
	tapa::istream<int> &fifo_Alocal,
	tapa::istream<int> &fifo_Xlocal,
	tapa::ostream<int> &fifo_Ylocal,
	const unsigned int num,
	const unsigned int jj
){
#pragma HLS INLINE off
	short X_local[MAX_J];

	// local computation
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			X_local[j] = fifo_Xlocal.read();
		}
		short Y_local = 0;

		[[tapa::pipeline(1)]]
		for(short j = 0; j < jj; j++){
			short opA = fifo_Alocal.read();
			Y_local += opA * X_local[j];   
		}

		fifo_Ylocal.write(Y_local);
	}
}

void data_collectorY_head(
	tapa::istream<int> &fifo_Ylocal,
	tapa::ostream<int> &fifo_Yout,
	const unsigned int pe_id,
	unsigned int num
){
#pragma HLS INLINE off
	[[tapa::pipeline(1)]]
	for(short n = 0; n < num; n++ ){
		int Y_temp = fifo_Ylocal.read();
		fifo_Yout.write(Y_temp);
	}
}

void data_collectorY(
	tapa::istream<int> &fifo_Ylocal,
	tapa::istream<int> &fifo_Yin,
	tapa::ostream<int> &fifo_Yout,
	const unsigned int pe_id,
	unsigned int num
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short i = 0; i < pe_id; i++){
			int Y_temp = fifo_Yin.read();
			fifo_Yout.write(Y_temp);
		}
		int Y_temp = fifo_Ylocal.read();
		fifo_Yout.write(Y_temp);
	}
}

void data_collectorY_tail(
	tapa::mmap<int> Y,
	//int Y[I],
	tapa::istream<int> &fifo_Ylocal,
	tapa::istream<int> &fifo_Yin,
	const unsigned int pe_id,
	const unsigned int num
){
#pragma HLS INLINE off
	for(short n = 0; n < num; n++ ){
		[[tapa::pipeline(1)]]
		for(short i = 0; i < I; i++){
			int Y_temp;
			if( i == pe_id ){
				Y_temp = fifo_Ylocal.read();
			}
			else{
				Y_temp = fifo_Yin.read();
			}
			Y[n*I+i] = Y_temp;
		}
	}
}

void mv(
  tapa::mmap<tapa::vec_t<int, I>> A,
  tapa::mmap<const int> X,
  tapa::mmap<int> Y,
  int num,
  int jj
){
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A0 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A1 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A2 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A3 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A4 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A5 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A6 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A7 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A8 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A9 ;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A10;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A11;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A12;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A13;
	tapa::stream<tapa::vec_t<int, I>, 2> fifo_A14;

	tapa::stream<int,2> fifo_X0 ;
	tapa::stream<int,2> fifo_X1 ;
	tapa::stream<int,2> fifo_X2 ;
	tapa::stream<int,2> fifo_X3 ;
	tapa::stream<int,2> fifo_X4 ;
	tapa::stream<int,2> fifo_X5 ;
	tapa::stream<int,2> fifo_X6 ;
	tapa::stream<int,2> fifo_X7 ;
	tapa::stream<int,2> fifo_X8 ;
	tapa::stream<int,2> fifo_X9 ;
	tapa::stream<int,2> fifo_X10;
	tapa::stream<int,2> fifo_X11;
	tapa::stream<int,2> fifo_X12;
	tapa::stream<int,2> fifo_X13;
	tapa::stream<int,2> fifo_X14;

	tapa::stream<int,2> fifo_Y0;
	tapa::stream<int,2> fifo_Y1;
	tapa::stream<int,2> fifo_Y2;
	tapa::stream<int,2> fifo_Y3;
	tapa::stream<int,2> fifo_Y4 ;
	tapa::stream<int,2> fifo_Y5 ;
	tapa::stream<int,2> fifo_Y6 ;
	tapa::stream<int,2> fifo_Y7 ;
	tapa::stream<int,2> fifo_Y8 ;
	tapa::stream<int,2> fifo_Y9 ;
	tapa::stream<int,2> fifo_Y10;
	tapa::stream<int,2> fifo_Y11;
	tapa::stream<int,2> fifo_Y12;
	tapa::stream<int,2> fifo_Y13;
	tapa::stream<int,2> fifo_Y14;

	tapa::stream<int,2> fifo_Alocal0 ;
	tapa::stream<int,2> fifo_Alocal1 ;
	tapa::stream<int,2> fifo_Alocal2 ;
	tapa::stream<int,2> fifo_Alocal3 ;
	tapa::stream<int,2> fifo_Alocal4 ;
	tapa::stream<int,2> fifo_Alocal5 ;
	tapa::stream<int,2> fifo_Alocal6 ;
	tapa::stream<int,2> fifo_Alocal7 ;
	tapa::stream<int,2> fifo_Alocal8 ;
	tapa::stream<int,2> fifo_Alocal9 ;
	tapa::stream<int,2> fifo_Alocal10;
	tapa::stream<int,2> fifo_Alocal11;
	tapa::stream<int,2> fifo_Alocal12;
	tapa::stream<int,2> fifo_Alocal13;
	tapa::stream<int,2> fifo_Alocal14;
	tapa::stream<int,2> fifo_Alocal15;

	tapa::stream<int,2> fifo_Xlocal0 ;
	tapa::stream<int,2> fifo_Xlocal1 ;
	tapa::stream<int,2> fifo_Xlocal2 ;
	tapa::stream<int,2> fifo_Xlocal3 ;
	tapa::stream<int,2> fifo_Xlocal4 ;
	tapa::stream<int,2> fifo_Xlocal5 ;
	tapa::stream<int,2> fifo_Xlocal6 ;
	tapa::stream<int,2> fifo_Xlocal7 ;
	tapa::stream<int,2> fifo_Xlocal8 ;
	tapa::stream<int,2> fifo_Xlocal9 ;
	tapa::stream<int,2> fifo_Xlocal10;
	tapa::stream<int,2> fifo_Xlocal11;
	tapa::stream<int,2> fifo_Xlocal12;
	tapa::stream<int,2> fifo_Xlocal13;
	tapa::stream<int,2> fifo_Xlocal14;
	tapa::stream<int,2> fifo_Xlocal15;

	tapa::stream<int,2> fifo_Ylocal0;
	tapa::stream<int,2> fifo_Ylocal1;
	tapa::stream<int,2> fifo_Ylocal2;
	tapa::stream<int,2> fifo_Ylocal3;
	tapa::stream<int,2> fifo_Ylocal4 ;
	tapa::stream<int,2> fifo_Ylocal5 ;
	tapa::stream<int,2> fifo_Ylocal6 ;
	tapa::stream<int,2> fifo_Ylocal7 ;
	tapa::stream<int,2> fifo_Ylocal8 ;
	tapa::stream<int,2> fifo_Ylocal9 ;
	tapa::stream<int,2> fifo_Ylocal10;
	tapa::stream<int,2> fifo_Ylocal11;
	tapa::stream<int,2> fifo_Ylocal12;
	tapa::stream<int,2> fifo_Ylocal13;
	tapa::stream<int,2> fifo_Ylocal14;
	tapa::stream<int,2> fifo_Ylocal15;

tapa::task()
  .invoke(data_feederA_head, A, fifo_A0, fifo_Alocal0, 0, num, jj)
  .invoke(data_feederA, fifo_A0 , fifo_A1 , fifo_Alocal1 , 1 , num, jj)
  .invoke(data_feederA, fifo_A1 , fifo_A2 , fifo_Alocal2 , 2 , num, jj)
  .invoke(data_feederA, fifo_A2 , fifo_A3 , fifo_Alocal3 , 3 , num, jj)
  .invoke(data_feederA, fifo_A3 , fifo_A4 , fifo_Alocal4 , 4 , num, jj)
  .invoke(data_feederA, fifo_A4 , fifo_A5 , fifo_Alocal5 , 5 , num, jj)
  .invoke(data_feederA, fifo_A5 , fifo_A6 , fifo_Alocal6 , 6 , num, jj)
  .invoke(data_feederA, fifo_A6 , fifo_A7 , fifo_Alocal7 , 7 , num, jj)
  .invoke(data_feederA, fifo_A7 , fifo_A8 , fifo_Alocal8 , 8 , num, jj)
  .invoke(data_feederA, fifo_A8 , fifo_A9 , fifo_Alocal9 , 9 , num, jj)
  .invoke(data_feederA, fifo_A9 , fifo_A10, fifo_Alocal10, 10, num, jj)
  .invoke(data_feederA, fifo_A10, fifo_A11, fifo_Alocal11, 11, num, jj)
  .invoke(data_feederA, fifo_A11, fifo_A12, fifo_Alocal12, 12, num, jj)
  .invoke(data_feederA, fifo_A12, fifo_A13, fifo_Alocal13, 13, num, jj)
  .invoke(data_feederA, fifo_A13, fifo_A14, fifo_Alocal14, 14, num, jj)
  .invoke(data_feederA_tail, fifo_A14, fifo_Alocal15, 15, num, jj)

  .invoke(data_feederX_head, X, fifo_X0, fifo_Xlocal0, num, jj)
  .invoke(data_feederX, fifo_X0 , fifo_X1 , fifo_Xlocal1 , num, jj)
  .invoke(data_feederX, fifo_X1 , fifo_X2 , fifo_Xlocal2 , num, jj)
  .invoke(data_feederX, fifo_X2 , fifo_X3 , fifo_Xlocal3 , num, jj)
  .invoke(data_feederX, fifo_X3 , fifo_X4 , fifo_Xlocal4 , num, jj)
  .invoke(data_feederX, fifo_X4 , fifo_X5 , fifo_Xlocal5 , num, jj)
  .invoke(data_feederX, fifo_X5 , fifo_X6 , fifo_Xlocal6 , num, jj)
  .invoke(data_feederX, fifo_X6 , fifo_X7 , fifo_Xlocal7 , num, jj)
  .invoke(data_feederX, fifo_X7 , fifo_X8 , fifo_Xlocal8 , num, jj)
  .invoke(data_feederX, fifo_X8 , fifo_X9 , fifo_Xlocal9 , num, jj)
  .invoke(data_feederX, fifo_X9 , fifo_X10, fifo_Xlocal10, num, jj)
  .invoke(data_feederX, fifo_X10, fifo_X11, fifo_Xlocal11, num, jj)
  .invoke(data_feederX, fifo_X11, fifo_X12, fifo_Xlocal12, num, jj)
  .invoke(data_feederX, fifo_X12, fifo_X13, fifo_Xlocal13, num, jj)
  .invoke(data_feederX, fifo_X13, fifo_X14, fifo_Xlocal14, num, jj)
  .invoke(data_feederX_tail, fifo_X14, fifo_Xlocal15, num, jj)

  .invoke(PE, fifo_Alocal0 , fifo_Xlocal0 , fifo_Ylocal0 , num, jj)
  .invoke(PE, fifo_Alocal1 , fifo_Xlocal1 , fifo_Ylocal1 , num, jj)
  .invoke(PE, fifo_Alocal2 , fifo_Xlocal2 , fifo_Ylocal2 , num, jj)
  .invoke(PE, fifo_Alocal3 , fifo_Xlocal3 , fifo_Ylocal3 , num, jj)
  .invoke(PE, fifo_Alocal4 , fifo_Xlocal4 , fifo_Ylocal4 , num, jj)
  .invoke(PE, fifo_Alocal5 , fifo_Xlocal5 , fifo_Ylocal5 , num, jj)
  .invoke(PE, fifo_Alocal6 , fifo_Xlocal6 , fifo_Ylocal6 , num, jj)
  .invoke(PE, fifo_Alocal7 , fifo_Xlocal7 , fifo_Ylocal7 , num, jj)
  .invoke(PE, fifo_Alocal8 , fifo_Xlocal8 , fifo_Ylocal8 , num, jj)
  .invoke(PE, fifo_Alocal9 , fifo_Xlocal9 , fifo_Ylocal9 , num, jj)
  .invoke(PE, fifo_Alocal10, fifo_Xlocal10, fifo_Ylocal10, num, jj)
  .invoke(PE, fifo_Alocal11, fifo_Xlocal11, fifo_Ylocal11, num, jj)
  .invoke(PE, fifo_Alocal12, fifo_Xlocal12, fifo_Ylocal12, num, jj)
  .invoke(PE, fifo_Alocal13, fifo_Xlocal13, fifo_Ylocal13, num, jj)
  .invoke(PE, fifo_Alocal14, fifo_Xlocal14, fifo_Ylocal14, num, jj)
  .invoke(PE, fifo_Alocal15, fifo_Xlocal15, fifo_Ylocal15, num, jj)

  .invoke(data_collectorY_head, fifo_Ylocal0 , fifo_Y0 , 0, num)
  .invoke(data_collectorY, fifo_Ylocal1 , fifo_Y0 , fifo_Y1 , 1 , num)
  .invoke(data_collectorY, fifo_Ylocal2 , fifo_Y1 , fifo_Y2 , 2 , num)
  .invoke(data_collectorY, fifo_Ylocal3 , fifo_Y2 , fifo_Y3 , 3 , num)
  .invoke(data_collectorY, fifo_Ylocal4 , fifo_Y3 , fifo_Y4 , 4 , num)
  .invoke(data_collectorY, fifo_Ylocal5 , fifo_Y4 , fifo_Y5 , 5 , num)
  .invoke(data_collectorY, fifo_Ylocal6 , fifo_Y5 , fifo_Y6 , 6 , num)
  .invoke(data_collectorY, fifo_Ylocal7 , fifo_Y6 , fifo_Y7 , 7 , num)
  .invoke(data_collectorY, fifo_Ylocal8 , fifo_Y7 , fifo_Y8 , 8 , num)
  .invoke(data_collectorY, fifo_Ylocal9 , fifo_Y8 , fifo_Y9 , 9 , num)
  .invoke(data_collectorY, fifo_Ylocal10, fifo_Y9 , fifo_Y10, 10, num)
  .invoke(data_collectorY, fifo_Ylocal11, fifo_Y10, fifo_Y11, 11, num)
  .invoke(data_collectorY, fifo_Ylocal12, fifo_Y11, fifo_Y12, 12, num)
  .invoke(data_collectorY, fifo_Ylocal13, fifo_Y12, fifo_Y13, 13, num)
  .invoke(data_collectorY, fifo_Ylocal14, fifo_Y13, fifo_Y14, 14, num)
  .invoke(data_collectorY_tail, Y, fifo_Ylocal15, fifo_Y14, 15, num)
  ;

}

