#include <tapa.h>

using tapa::detach;
using tapa::join;

#include "params.h"

void data_feederI_head(
	tapa::mmap<const int> I,
	tapa::ostream<int> & fifo_Iout,
	tapa::ostream<int> & fifo_Ilocal,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn
){
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short chi = 0; chi < nn; chi+=Tn) {
					for(short tr = 0; tr < Tr+K-1; tr++) {
						for(short tc = 0; tc < Tc+K-1; tc++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								int r = row + tr;
								int c = col + tc;
								int n = chi + tn;
								int I_temp = I[(r*(cc+K-1) + c)*nn + n];
								fifo_Iout.write(I_temp);
								fifo_Ilocal.write(I_temp);
							}
						}
					}
				}
			}
		}
	}
}

void data_feederI(
	tapa::istream<int> & fifo_Iin,
	tapa::ostream<int> & fifo_Iout,
	tapa::ostream<int> & fifo_Ilocal,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn
){
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short chi = 0; chi < nn; chi+=Tn) {
					for(short tr = 0; tr < Tr+K-1; tr++) {
						for(short tc = 0; tc < Tc+K-1; tc++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								int I_temp = fifo_Iin.read();
								fifo_Iout.write(I_temp);
								fifo_Ilocal.write(I_temp);
							}
						}
					}
				}
			}
		}
	}
}

void data_feederI_tail(
	tapa::istream<int> & fifo_Iin,
	tapa::ostream<int> & fifo_Ilocal,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn
){
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short chi = 0; chi < nn; chi+=Tn) {
					for(short tr = 0; tr < Tr+K-1; tr++) {
						for(short tc = 0; tc < Tc+K-1; tc++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								int I_temp = fifo_Iin.read();
								fifo_Ilocal.write(I_temp);
							}
						}
					}
				}
			}
		}
	}
}

void data_feederK_head(
	tapa::mmap<tapa::vec_t<int, Tm>> KER,
	tapa::ostream<tapa::vec_t<int, Tm>> & fifo_Kout,
	tapa::ostream<int> & fifo_Klocal,
	const unsigned int pe_id,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn
){
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short chi = 0; chi < nn; chi+=Tn) {
					for(short ki = 0; ki < K; ki++) {
						for(short kj = 0; kj < K; kj++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								int m = cho/Tm;
								int n = chi + tn;
								tapa::vec_t<int, Tm> K_temp = KER[(m*(K*K) + ki*K + kj)*nn+n];
								fifo_Kout.write(K_temp);
								fifo_Klocal.write(K_temp[pe_id]);
							}
						}
					}
				}
			}
		}
	}
}

void data_feederK(
	tapa::istream<tapa::vec_t<int, Tm>> & fifo_Kin,
	tapa::ostream<tapa::vec_t<int, Tm>> & fifo_Kout,
	tapa::ostream<int> & fifo_Klocal,
	const unsigned int pe_id,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn
){
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short chi = 0; chi < nn; chi+=Tn) {
					for(short ki = 0; ki < K; ki++) {
						for(short kj = 0; kj < K; kj++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								tapa::vec_t<int, Tm> K_temp = fifo_Kin.read();
								fifo_Kout.write(K_temp);
								fifo_Klocal.write(K_temp[pe_id]);

							}
						}
					}
				}
			}
		}
	}
}

void data_feederK_tail(
	tapa::istream<tapa::vec_t<int, Tm>> & fifo_Kin,
	tapa::ostream<int> & fifo_Klocal,
	const unsigned int pe_id,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn
){
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short chi = 0; chi < nn; chi+=Tn) {
					for(short ki = 0; ki < K; ki++) {
						for(short kj = 0; kj < K; kj++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								tapa::vec_t<int, Tm> K_temp = fifo_Kin.read();
								fifo_Klocal.write(K_temp[pe_id]);
							}
						}
					}
				}
			}
		}
	}
}

void PE(
	tapa::istream<int> &fifo_Ilocal,
	tapa::istream<int> &fifo_Klocal,
	tapa::ostream<int> &fifo_Olocal,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn

){
	DTYPE Itile[Tr+K-1][Tc+K-1][Tn];
	DTYPE Ktile[K][K][Tn];
	DTYPE Otile[Tr][Tc] ;

#pragma HLS ARRAY_PARTITION dim=3 type=complete variable=Itile
#pragma HLS ARRAY_PARTITION dim=3 type=complete variable=Ktile

	for(short tr = 0; tr < Tr; tr++) {
		[[tapa::pipeline(1)]]
		for(short tc = 0; tc < Tc; tc++) {
			Otile[tr][tc] = 0;
		}
	}

	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short chi = 0; chi < nn; chi+=Tn) {

					for(short tr = 0; tr < Tr+K-1; tr++) {
						for(short tc = 0; tc < Tc+K-1; tc++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								Itile[tr][tc][tn] = fifo_Ilocal.read();
							}
						}
					}

					for(short ki = 0; ki < K; ki++) {
						for(short kj = 0; kj < K; kj++) {
							[[tapa::pipeline(1)]]
							for(short tn = 0; tn < Tn; tn++) {
								Ktile[ki][kj][tn]= fifo_Klocal.read();
							}
						}
					}

					for(short ki = 0; ki < K; ki++) {
						for(short kj = 0; kj < K; kj++) {
							for(short tr = 0; tr < Tr; tr++) {
								for(short tc = 0; tc < Tc; tc++) {
									[[tapa::pipeline(1)]]
									for(short tn = 0; tn < Tn; tn++) {
										Otile[tr][tc] += Ktile[ki][kj][tn] * Itile[tr+ki][tc+kj][tn];
									}
								}
							}
						}
					}
				}
				for(short tr = 0; tr < Tr; tr++) {
					[[tapa::pipeline(1)]]
					for(short tc = 0; tc < Tc; tc++) {
							fifo_Olocal.write(Otile[tr][tc]);
							Otile[tr][tc] = 0;
					}
				}
			}
		}
	}
}

void data_collectorO_head(
	tapa::istream<int> &fifo_Olocal,
	tapa::ostream<int> &fifo_Oout,
	const unsigned int pe_id,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm
){
#pragma HLS INLINE off
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short tr = 0; tr < Tr; tr++) {
					[[tapa::pipeline(1)]]
					for(short tc = 0; tc < Tc; tc++) {
						int O_temp = fifo_Olocal.read();
						fifo_Oout.write(O_temp);
					}
				}
			}
		}
	}
}

void data_collectorO(
	tapa::istream<int> &fifo_Olocal,
	tapa::istream<int> &fifo_Oin,
	tapa::ostream<int> &fifo_Oout,
	const unsigned int pe_id,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm
){
#pragma HLS INLINE off
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short tr = 0; tr < Tr; tr++) {
					for(short tc = 0; tc < Tc; tc++) {
						[[tapa::pipeline(1)]]
						for(short tm = 0; tm < pe_id; tm++) {
							int O_temp = fifo_Oin.read();
							fifo_Oout.write(O_temp);
						}
						int O_temp = fifo_Olocal.read();
						fifo_Oout.write(O_temp);
					}
				}
			}
		}
	}
}

void data_collectorO_tail(
	tapa::mmap<int> O,
	tapa::istream<int> &fifo_Olocal,
	tapa::istream<int> &fifo_Oin,
	const unsigned int pe_id,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm
){
#pragma HLS INLINE off
	for(short row = 0; row < rr; row+=Tr) {
		for(short col = 0; col < cc; col+=Tc) {
			for(short cho = 0; cho < mm; cho+=Tm) {
				for(short tr = 0; tr < Tr; tr++) {
					for(short tc = 0; tc < Tc; tc++) {
						[[tapa::pipeline(1)]]
						for(short tm = 0; tm < Tm; tm++) {
							int r = row + tr;
							int c = col + tc;
							int m = cho + tm;
							int O_temp;
							if( tm == pe_id ){
								O_temp = fifo_Olocal.read();
							}
							else{
								O_temp = fifo_Oin.read();
							}
							O[(r*cc + c)*mm + m] = O_temp;
						}
					}
				}
			}
		}
	}
}

void cnn(
	tapa::mmap<const int> I,
	tapa::mmap<tapa::vec_t<int, Tm>> KER,
	tapa::mmap<int> O,
	const unsigned int rr,
	const unsigned int cc,
	const unsigned int mm,
	const unsigned int nn

){

	tapa::stream<int,2> fifo_I0 ;
	tapa::stream<int,2> fifo_I1 ;
	tapa::stream<int,2> fifo_I2 ;
	tapa::stream<int,2> fifo_I3 ;
	tapa::stream<int,2> fifo_I4 ;
	tapa::stream<int,2> fifo_I5 ;
	tapa::stream<int,2> fifo_I6 ;
	tapa::stream<int,2> fifo_I7 ;
	tapa::stream<int,2> fifo_I8 ;
	tapa::stream<int,2> fifo_I9 ;
	tapa::stream<int,2> fifo_I10;
	tapa::stream<int,2> fifo_I11;
	tapa::stream<int,2> fifo_I12;
	tapa::stream<int,2> fifo_I13;
	tapa::stream<int,2> fifo_I14;

	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K0 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K1 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K2 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K3 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K4 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K5 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K6 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K7 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K8 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K9 ;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K10;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K11;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K12;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K13;
	tapa::stream<tapa::vec_t<int, Tm>, 2> fifo_K14;

	tapa::stream<int,2> fifo_O0 ;
	tapa::stream<int,2> fifo_O1 ;
	tapa::stream<int,2> fifo_O2 ;
	tapa::stream<int,2> fifo_O3 ;
	tapa::stream<int,2> fifo_O4 ;
	tapa::stream<int,2> fifo_O5 ;
	tapa::stream<int,2> fifo_O6 ;
	tapa::stream<int,2> fifo_O7 ;
	tapa::stream<int,2> fifo_O8 ;
	tapa::stream<int,2> fifo_O9 ;
	tapa::stream<int,2> fifo_O10;
	tapa::stream<int,2> fifo_O11;
	tapa::stream<int,2> fifo_O12;
	tapa::stream<int,2> fifo_O13;
	tapa::stream<int,2> fifo_O14;

	tapa::stream<int,2> fifo_Ilocal0 ;
	tapa::stream<int,2> fifo_Ilocal1 ;
	tapa::stream<int,2> fifo_Ilocal2 ;
	tapa::stream<int,2> fifo_Ilocal3 ;
	tapa::stream<int,2> fifo_Ilocal4 ;
	tapa::stream<int,2> fifo_Ilocal5 ;
	tapa::stream<int,2> fifo_Ilocal6 ;
	tapa::stream<int,2> fifo_Ilocal7 ;
	tapa::stream<int,2> fifo_Ilocal8 ;
	tapa::stream<int,2> fifo_Ilocal9 ;
	tapa::stream<int,2> fifo_Ilocal10;
	tapa::stream<int,2> fifo_Ilocal11;
	tapa::stream<int,2> fifo_Ilocal12;
	tapa::stream<int,2> fifo_Ilocal13;
	tapa::stream<int,2> fifo_Ilocal14;
	tapa::stream<int,2> fifo_Ilocal15;

	tapa::stream<int,2> fifo_Klocal0 ;
	tapa::stream<int,2> fifo_Klocal1 ;
	tapa::stream<int,2> fifo_Klocal2 ;
	tapa::stream<int,2> fifo_Klocal3 ;
	tapa::stream<int,2> fifo_Klocal4 ;
	tapa::stream<int,2> fifo_Klocal5 ;
	tapa::stream<int,2> fifo_Klocal6 ;
	tapa::stream<int,2> fifo_Klocal7 ;
	tapa::stream<int,2> fifo_Klocal8 ;
	tapa::stream<int,2> fifo_Klocal9 ;
	tapa::stream<int,2> fifo_Klocal10;
	tapa::stream<int,2> fifo_Klocal11;
	tapa::stream<int,2> fifo_Klocal12;
	tapa::stream<int,2> fifo_Klocal13;
	tapa::stream<int,2> fifo_Klocal14;
	tapa::stream<int,2> fifo_Klocal15;

	tapa::stream<int,2> fifo_Olocal0 ;
	tapa::stream<int,2> fifo_Olocal1 ;
	tapa::stream<int,2> fifo_Olocal2 ;
	tapa::stream<int,2> fifo_Olocal3 ;
	tapa::stream<int,2> fifo_Olocal4 ;
	tapa::stream<int,2> fifo_Olocal5 ;
	tapa::stream<int,2> fifo_Olocal6 ;
	tapa::stream<int,2> fifo_Olocal7 ;
	tapa::stream<int,2> fifo_Olocal8 ;
	tapa::stream<int,2> fifo_Olocal9 ;
	tapa::stream<int,2> fifo_Olocal10;
	tapa::stream<int,2> fifo_Olocal11;
	tapa::stream<int,2> fifo_Olocal12;
	tapa::stream<int,2> fifo_Olocal13;
	tapa::stream<int,2> fifo_Olocal14;
	tapa::stream<int,2> fifo_Olocal15;

tapa::task()
  .invoke(data_feederI_head,      I  , fifo_I0 , fifo_Ilocal0 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I0 , fifo_I1 , fifo_Ilocal1 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I1 , fifo_I2 , fifo_Ilocal2 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I2 , fifo_I3 , fifo_Ilocal3 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I3 , fifo_I4 , fifo_Ilocal4 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I4 , fifo_I5 , fifo_Ilocal5 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I5 , fifo_I6 , fifo_Ilocal6 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I6 , fifo_I7 , fifo_Ilocal7 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I7 , fifo_I8 , fifo_Ilocal8 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I8 , fifo_I9 , fifo_Ilocal9 , rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I9 , fifo_I10, fifo_Ilocal10, rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I10, fifo_I11, fifo_Ilocal11, rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I11, fifo_I12, fifo_Ilocal12, rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I12, fifo_I13, fifo_Ilocal13, rr, cc, mm, nn)
  .invoke(data_feederI,      fifo_I13, fifo_I14, fifo_Ilocal14, rr, cc, mm, nn)
  .invoke(data_feederI_tail, fifo_I14,           fifo_Ilocal15, rr, cc, mm, nn)

  .invoke(data_feederK_head,     KER , fifo_K0 , fifo_Klocal0 , 0 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K0 , fifo_K1 , fifo_Klocal1 , 1 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K1 , fifo_K2 , fifo_Klocal2 , 2 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K2 , fifo_K3 , fifo_Klocal3 , 3 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K3 , fifo_K4 , fifo_Klocal4 , 4 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K4 , fifo_K5 , fifo_Klocal5 , 5 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K5 , fifo_K6 , fifo_Klocal6 , 6 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K6 , fifo_K7 , fifo_Klocal7 , 7 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K7 , fifo_K8 , fifo_Klocal8 , 8 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K8 , fifo_K9 , fifo_Klocal9 , 9 , rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K9 , fifo_K10, fifo_Klocal10, 10, rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K10, fifo_K11, fifo_Klocal11, 11, rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K11, fifo_K12, fifo_Klocal12, 12, rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K12, fifo_K13, fifo_Klocal13, 13, rr, cc, mm, nn)
  .invoke(data_feederK,      fifo_K13, fifo_K14, fifo_Klocal14, 14, rr, cc, mm, nn)
  .invoke(data_feederK_tail, fifo_K14,           fifo_Klocal15, 15, rr, cc, mm, nn)

  .invoke(PE, fifo_Ilocal0 , fifo_Klocal0 , fifo_Olocal0 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal1 , fifo_Klocal1 , fifo_Olocal1 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal2 , fifo_Klocal2 , fifo_Olocal2 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal3 , fifo_Klocal3 , fifo_Olocal3 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal4 , fifo_Klocal4 , fifo_Olocal4 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal5 , fifo_Klocal5 , fifo_Olocal5 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal6 , fifo_Klocal6 , fifo_Olocal6 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal7 , fifo_Klocal7 , fifo_Olocal7 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal8 , fifo_Klocal8 , fifo_Olocal8 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal9 , fifo_Klocal9 , fifo_Olocal9 , rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal10, fifo_Klocal10, fifo_Olocal10, rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal11, fifo_Klocal11, fifo_Olocal11, rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal12, fifo_Klocal12, fifo_Olocal12, rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal13, fifo_Klocal13, fifo_Olocal13, rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal14, fifo_Klocal14, fifo_Olocal14, rr, cc, mm, nn)
  .invoke(PE, fifo_Ilocal15, fifo_Klocal15, fifo_Olocal15, rr, cc, mm, nn)

  .invoke(data_collectorO_head,    fifo_Olocal0 ,           fifo_O0 , 0 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal1 , fifo_O0 , fifo_O1 , 1 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal2 , fifo_O1 , fifo_O2 , 2 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal3 , fifo_O2 , fifo_O3 , 3 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal4 , fifo_O3 , fifo_O4 , 4 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal5 , fifo_O4 , fifo_O5 , 5 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal6 , fifo_O5 , fifo_O6 , 6 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal7 , fifo_O6 , fifo_O7 , 7 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal8 , fifo_O7 , fifo_O8 , 8 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal9 , fifo_O8 , fifo_O9 , 9 , rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal10, fifo_O9 , fifo_O10, 10, rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal11, fifo_O10, fifo_O11, 11, rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal12, fifo_O11, fifo_O12, 12, rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal13, fifo_O12, fifo_O13, 13, rr, cc, mm)
  .invoke(data_collectorO,         fifo_Olocal14, fifo_O13, fifo_O14, 14, rr, cc, mm)
  .invoke(data_collectorO_tail, O, fifo_Olocal15, fifo_O14,           15, rr, cc, mm)
  ;

}

