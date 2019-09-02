#include <tlp.h>

template <typename T, uint64_t N>
struct coalesce_t {
  T data[N];
  T& operator[](uint64_t idx) { return data[idx]; }
  const T& operator[](uint64_t idx) const { return data[idx]; }
};

void Array2Stream(const float* array, uint64_t n,
                  tlp::stream<coalesce_t<float, 2>>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
#pragma HLS pipeline II = 2
    stream.write({array[i * 2], array[i * 2 + 1]});
  }
  stream.close();
}

void Stream2Array(tlp::stream<coalesce_t<float, 2>>& stream, float* array) {
  for (uint64_t i = 0; !stream.eos(); ++i) {
#pragma HLS pipeline II = 2
    auto packed = stream.read();
    array[i * 2] = packed[0];
    array[i * 2 + 1] = packed[1];
  }
}

void Module0Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /*output*/ tlp::stream<float>& fifo_st_1,
    /* input*/ tlp::stream<coalesce_t<float, 2>>& dram_t1_bank_0_fifo) {
module_0_epoch:
  while (!dram_t1_bank_0_fifo.eos()) {
    auto dram_t1_bank_0_buf = dram_t1_bank_0_fifo.read();
    fifo_st_0.write(dram_t1_bank_0_buf[1]);
    fifo_st_1.write(dram_t1_bank_0_buf[0]);
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module1Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /*output*/ tlp::stream<float>& fifo_st_1,
    /* input*/ tlp::stream<float>& fifo_ld_0) {
module_1_epoch:
  while (!fifo_ld_0.eos()) {
    auto fifo_ref_0 = fifo_ld_0.read();
    fifo_st_0.write(fifo_ref_0);
    fifo_st_1.write(fifo_ref_0);
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module2Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /*output*/ tlp::stream<float>& fifo_st_1,
    /* input*/ tlp::stream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_50_buf[50];
  int fifo_ref_0_delayed_50_ptr = 0;
module_2_epoch:
  while (!fifo_ld_0.eos()) {
#pragma HLS dependence variable = fifo_ref_1_delayed_50_buf inter false
    auto fifo_ref_0 = fifo_ld_0.read();
    float fifo_ref_0_delayed_50 =
        fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr];
    fifo_st_0.write(fifo_ref_0_delayed_50);
    fifo_st_1.write(fifo_ref_0_delayed_50);
    fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr] = fifo_ref_0;
    fifo_ref_0_delayed_50_ptr =
        fifo_ref_0_delayed_50_ptr < 49 ? fifo_ref_0_delayed_50_ptr + 1 : 0;
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module3Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /* input*/ tlp::stream<float>& fifo_ld_0,
    /* input*/ tlp::stream<float>& fifo_ld_1) {
module_3_epoch:
  while (!fifo_ld_0.eos() && !fifo_ld_1.eos()) {
    float fifo_ref_0 = fifo_ld_0.read();
    float fifo_ref_1 = fifo_ld_1.read();
    fifo_st_0.write(fifo_ref_0 + fifo_ref_1);
  }
  fifo_st_0.close();
}

void Module4Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /* input*/ tlp::stream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_1_buf[1];
  int fifo_ref_0_delayed_1_ptr = 0;
module_4_epoch:
  while (!fifo_ld_0.eos()) {
#pragma HLS dependence variable = fifo_ref_0_delayed_1_buf inter false
    auto fifo_ref_0 = fifo_ld_0.read();
    const float fifo_ref_0_delayed_1 =
        fifo_ref_0_delayed_1_buf[fifo_ref_0_delayed_1_ptr];
    fifo_st_0.write(fifo_ref_0_delayed_1);
    fifo_ref_0_delayed_1_buf[fifo_ref_0_delayed_1_ptr] = fifo_ref_0;
    fifo_ref_0_delayed_1_ptr =
        fifo_ref_0_delayed_1_ptr < 0 ? fifo_ref_0_delayed_1_ptr + 1 : 0;
  }
  fifo_st_0.close();
}

void Module5Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /* input*/ tlp::stream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_50_buf[50];
  int fifo_ref_0_delayed_50_ptr = 0;
module_5_epoch:
  while (!fifo_ld_0.eos()) {
#pragma HLS dependence variable = fifo_ref_0_delayed_50_buf inter false
    auto fifo_ref_0 = fifo_ld_0.read();
    const float fifo_ref_0_delayed_50 =
        fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr];
    fifo_st_0.write(fifo_ref_0_delayed_50);
    fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr] = fifo_ref_0;
    fifo_ref_0_delayed_50_ptr =
        fifo_ref_0_delayed_50_ptr < 49 ? fifo_ref_0_delayed_50_ptr + 1 : 0;
  }
  fifo_st_0.close();
}

void Module6Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /* input*/ tlp::stream<float>& fifo_ld_0,
    /* input*/ tlp::stream<float>& fifo_ld_1,
    /* input*/ tlp::stream<float>& fifo_ld_2) {
module_6_epoch:
  while (!fifo_ld_0.eos() && !fifo_ld_1.eos() && !fifo_ld_2.eos()) {
    auto fifo_ref_0 = fifo_ld_0.read();
    auto fifo_ref_1 = fifo_ld_1.read();
    auto fifo_ref_2 = fifo_ld_2.read();
    fifo_st_0.write((fifo_ref_0 + fifo_ref_1 + fifo_ref_2) * 0.2f);
  }
  fifo_st_0.close();
}

void Module7Func(
    /*output*/ tlp::stream<float>& fifo_st_0,
    /* input*/ tlp::stream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_49_buf[49];
  int fifo_ref_0_delayed_49_ptr = 0;
module_7_epoch:
  while (!fifo_ld_0.eos()) {
#pragma HLS dependence variable = fifo_ref_0_delayed_49_buf inter false
    auto fifo_ref_0 = fifo_ld_0.read();

    const float fifo_ref_0_delayed_49 =
        fifo_ref_0_delayed_49_buf[fifo_ref_0_delayed_49_ptr];
    fifo_st_0.write(fifo_ref_0_delayed_49);
    fifo_ref_0_delayed_49_buf[fifo_ref_0_delayed_49_ptr] = fifo_ref_0;
    fifo_ref_0_delayed_49_ptr =
        fifo_ref_0_delayed_49_ptr < 48 ? fifo_ref_0_delayed_49_ptr + 1 : 0;
  }
  fifo_st_0.close();
}

void Module8Func(
    /*output*/ tlp::stream<coalesce_t<float, 2>>& dram_t0_bank_0_fifo,
    /* input*/ tlp::stream<float>& fifo_ld_0,
    /* input*/ tlp::stream<float>& fifo_ld_1) {
module_8_epoch:
  while (!fifo_ld_0.eos() && !fifo_ld_1.eos()) {
    dram_t0_bank_0_fifo.write({fifo_ld_0.read(), fifo_ld_1.read()});
  }
  dram_t0_bank_0_fifo.close();
}

void Jacobi(float* bank_0_t0, float* bank_0_t1, uint64_t coalesced_data_num) {
  tlp::stream<coalesce_t<float, 2>, 32> bank_0_t1_buf("bank_0_t1_buf");
  tlp::stream<coalesce_t<float, 2>, 32> bank_0_t0_buf("bank_0_t0_buf");
  tlp::stream<float, 2> from_super_source_to_t1_offset_0(
      "from_super_source_to_t1_offset_0");
  tlp::stream<float, 2> from_super_source_to_t1_offset_1(
      "from_super_source_to_t1_offset_1");
  tlp::stream<float, 2> from_t1_offset_0_to_t1_offset_2000(
      "from_t1_offset_0_to_t1_offset_2000");
  tlp::stream<float, 4> from_t1_offset_0_to_tcse_var_0_pe_1(
      "from_t1_offset_0_to_tcse_var_0_pe_1");
  tlp::stream<float, 2> from_t1_offset_1_to_t1_offset_2001(
      "from_t1_offset_1_to_t1_offset_2001");
  tlp::stream<float, 6> from_t1_offset_1_to_tcse_var_0_pe_0(
      "from_t1_offset_1_to_tcse_var_0_pe_0");
  tlp::stream<float, 2> from_t1_offset_2000_to_t1_offset_2002(
      "from_t1_offset_2000_to_t1_offset_2002");
  tlp::stream<float, 8> from_t1_offset_2000_to_t0_pe_1(
      "from_t1_offset_2000_to_t0_pe_1");
  tlp::stream<float, 2> from_t1_offset_2001_to_tcse_var_0_pe_1(
      "from_t1_offset_2001_to_tcse_var_0_pe_1");
  tlp::stream<float, 6> from_t1_offset_2001_to_t0_pe_0(
      "from_t1_offset_2001_to_t0_pe_0");
  tlp::stream<float, 2> from_tcse_var_0_pe_1_to_tcse_var_0_offset_0(
      "from_tcse_var_0_pe_1_to_tcse_var_0_offset_0");
  tlp::stream<float, 2> from_t1_offset_2002_to_tcse_var_0_pe_0(
      "from_t1_offset_2002_to_tcse_var_0_pe_0");
  tlp::stream<float, 2> from_tcse_var_0_pe_0_to_tcse_var_0_offset_1(
      "from_tcse_var_0_pe_0_to_tcse_var_0_offset_1");
  tlp::stream<float, 2> from_tcse_var_0_offset_0_to_tcse_var_0_offset_2000(
      "from_tcse_var_0_offset_0_to_tcse_var_0_offset_2000");
  tlp::stream<float, 6> from_tcse_var_0_offset_0_to_t0_pe_1(
      "from_tcse_var_0_offset_0_to_t0_pe_1");
  tlp::stream<float, 2> from_tcse_var_0_offset_1_to_tcse_var_0_offset_1999(
      "from_tcse_var_0_offset_1_to_tcse_var_0_offset_1999");
  tlp::stream<float, 2> from_tcse_var_0_offset_1_to_t0_pe_0(
      "from_tcse_var_0_offset_1_to_t0_pe_0");
  tlp::stream<float, 2> from_tcse_var_0_offset_2000_to_t0_pe_0(
      "from_tcse_var_0_offset_2000_to_t0_pe_0");
  tlp::stream<float, 4> from_t0_pe_0_to_super_sink(
      "from_t0_pe_0_to_super_sink");
  tlp::stream<float, 2> from_tcse_var_0_offset_1999_to_t0_pe_1(
      "from_tcse_var_0_offset_1999_to_t0_pe_1");
  tlp::stream<float, 2> from_t0_pe_1_to_super_sink(
      "from_t0_pe_1_to_super_sink");

  tlp::task()
      .invoke<0>(Array2Stream, bank_0_t1, coalesced_data_num, bank_0_t1_buf)
      .invoke<0>(Module0Func,
                 /*output*/ from_super_source_to_t1_offset_0,
                 /*output*/ from_super_source_to_t1_offset_1,
                 /* input*/ bank_0_t1_buf)
      .invoke<0>(Module1Func,
                 /*output*/ from_t1_offset_0_to_t1_offset_2000,
                 /*output*/ from_t1_offset_0_to_tcse_var_0_pe_1,
                 /* input*/ from_super_source_to_t1_offset_0)
      .invoke<0>(Module1Func,
                 /*output*/ from_t1_offset_1_to_t1_offset_2001,
                 /*output*/ from_t1_offset_1_to_tcse_var_0_pe_0,
                 /* input*/ from_super_source_to_t1_offset_1)
      .invoke<0>(Module2Func,
                 /*output*/ from_t1_offset_2000_to_t1_offset_2002,
                 /*output*/ from_t1_offset_2000_to_t0_pe_1,
                 /* input*/ from_t1_offset_0_to_t1_offset_2000)
      .invoke<0>(Module2Func,
                 /*output*/ from_t1_offset_2001_to_tcse_var_0_pe_1,
                 /*output*/ from_t1_offset_2001_to_t0_pe_0,
                 /* input*/ from_t1_offset_1_to_t1_offset_2001)
      .invoke<0>(Module3Func,
                 /*output*/ from_tcse_var_0_pe_1_to_tcse_var_0_offset_0,
                 /* input*/ from_t1_offset_2001_to_tcse_var_0_pe_1,
                 /* input*/ from_t1_offset_0_to_tcse_var_0_pe_1)
      .invoke<0>(Module4Func,
                 /*output*/ from_t1_offset_2002_to_tcse_var_0_pe_0,
                 /* input*/ from_t1_offset_2000_to_t1_offset_2002)
      .invoke<0>(Module3Func,
                 /*output*/ from_tcse_var_0_pe_0_to_tcse_var_0_offset_1,
                 /* input*/ from_t1_offset_2002_to_tcse_var_0_pe_0,
                 /* input*/ from_t1_offset_1_to_tcse_var_0_pe_0)
      .invoke<0>(Module1Func,
                 /*output*/ from_tcse_var_0_offset_0_to_tcse_var_0_offset_2000,
                 /*output*/ from_tcse_var_0_offset_0_to_t0_pe_1,
                 /* input*/ from_tcse_var_0_pe_1_to_tcse_var_0_offset_0)
      .invoke<0>(Module1Func,
                 /*output*/ from_tcse_var_0_offset_1_to_tcse_var_0_offset_1999,
                 /*output*/ from_tcse_var_0_offset_1_to_t0_pe_0,
                 /* input*/ from_tcse_var_0_pe_0_to_tcse_var_0_offset_1)
      .invoke<0>(Module5Func,
                 /*output*/ from_tcse_var_0_offset_2000_to_t0_pe_0,
                 /* input*/ from_tcse_var_0_offset_0_to_tcse_var_0_offset_2000)
      .invoke<0>(Module6Func,
                 /*output*/ from_t0_pe_0_to_super_sink,
                 /* input*/ from_tcse_var_0_offset_2000_to_t0_pe_0,
                 /* input*/ from_tcse_var_0_offset_1_to_t0_pe_0,
                 /* input*/ from_t1_offset_2001_to_t0_pe_0)
      .invoke<0>(Module7Func,
                 /*output*/ from_tcse_var_0_offset_1999_to_t0_pe_1,
                 /* input*/ from_tcse_var_0_offset_1_to_tcse_var_0_offset_1999)
      .invoke<0>(Module6Func,
                 /*output*/ from_t0_pe_1_to_super_sink,
                 /* input*/ from_tcse_var_0_offset_1999_to_t0_pe_1,
                 /* input*/ from_tcse_var_0_offset_0_to_t0_pe_1,
                 /* input*/ from_t1_offset_2000_to_t0_pe_1)
      .invoke<0>(Module8Func,
                 /*output*/ bank_0_t0_buf,
                 /* input*/ from_t0_pe_0_to_super_sink,
                 /* input*/ from_t0_pe_1_to_super_sink)
      .invoke<0>(Stream2Array, bank_0_t0_buf, bank_0_t0);
}
