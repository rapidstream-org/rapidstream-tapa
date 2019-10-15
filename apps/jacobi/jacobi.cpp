#include <tlp.h>

template <typename T, uint64_t N>
struct coalesce_t {
  T data[N];
  T& operator[](uint64_t idx) { return data[idx]; }
  const T& operator[](uint64_t idx) const { return data[idx]; }
};

void Mmap2Stream(tlp::mmap<const float> mmap, uint64_t n,
                 tlp::ostream<coalesce_t<float, 2>>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
#pragma HLS pipeline II = 2
    stream.write({mmap[i * 2], mmap[i * 2 + 1]});
  }
  stream.close();
}

void Stream2Mmap(tlp::istream<coalesce_t<float, 2>>& stream,
                 tlp::mmap<float> mmap) {
  for (uint64_t i = 0;;) {
    bool eos;
    if (stream.try_eos(eos)) {
      if (eos) break;
#pragma HLS pipeline II = 2
      bool succeed;
      auto packed = stream.read(succeed);
      mmap[i * 2] = packed[0];
      mmap[i * 2 + 1] = packed[1];
      ++i;
    }
  }
}

void Module0Func(tlp::ostream<float>& fifo_st_0, tlp::ostream<float>& fifo_st_1,
                 tlp::istream<coalesce_t<float, 2>>& dram_t1_bank_0_fifo) {
module_0_epoch:
  for (;;) {
    bool eos;
    if (dram_t1_bank_0_fifo.try_eos(eos)) {
      if (eos) break;
      bool succeed;
      auto dram_t1_bank_0_buf = dram_t1_bank_0_fifo.read(succeed);
      fifo_st_0.write(dram_t1_bank_0_buf[1]);
      fifo_st_1.write(dram_t1_bank_0_buf[0]);
    }
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module1Func(tlp::ostream<float>& fifo_st_0, tlp::ostream<float>& fifo_st_1,
                 tlp::istream<float>& fifo_ld_0) {
module_1_epoch:
  for (;;) {
    bool eos;
    if (fifo_ld_0.try_eos(eos)) {
      if (eos) break;
      bool succeed;
      auto fifo_ref_0 = fifo_ld_0.read(succeed);
      fifo_st_0.write(fifo_ref_0);
      fifo_st_1.write(fifo_ref_0);
    }
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module2Func(tlp::ostream<float>& fifo_st_0, tlp::ostream<float>& fifo_st_1,
                 tlp::istream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_50_buf[50];
  int fifo_ref_0_delayed_50_ptr = 0;
module_2_epoch:
  for (;;) {
    bool eos;
    if (fifo_ld_0.try_eos(eos)) {
      if (eos) break;
#pragma HLS dependence variable = fifo_ref_1_delayed_50_buf inter false
      bool succeed;
      auto fifo_ref_0 = fifo_ld_0.read(succeed);
      float fifo_ref_0_delayed_50 =
          fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr];
      fifo_st_0.write(fifo_ref_0_delayed_50);
      fifo_st_1.write(fifo_ref_0_delayed_50);
      fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr] = fifo_ref_0;
      fifo_ref_0_delayed_50_ptr =
          fifo_ref_0_delayed_50_ptr < 49 ? fifo_ref_0_delayed_50_ptr + 1 : 0;
    }
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module3Func(tlp::ostream<float>& fifo_st_0, tlp::istream<float>& fifo_ld_0,
                 tlp::istream<float>& fifo_ld_1) {
module_3_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1)) {
      if (eos_0 || eos_1) break;
      bool succeed_0;
      bool succeed_1;
      float fifo_ref_0 = fifo_ld_0.read(succeed_0);
      float fifo_ref_1 = fifo_ld_1.read(succeed_1);
      fifo_st_0.write(fifo_ref_0 + fifo_ref_1);
    }
  }
  fifo_st_0.close();
}

void Module4Func(tlp::ostream<float>& fifo_st_0,
                 tlp::istream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_1_buf[1];
  int fifo_ref_0_delayed_1_ptr = 0;
module_4_epoch:
  for (;;) {
    bool eos;
    if (fifo_ld_0.try_eos(eos)) {
      if (eos) break;
#pragma HLS dependence variable = fifo_ref_0_delayed_1_buf inter false
      bool succeed;
      auto fifo_ref_0 = fifo_ld_0.read(succeed);
      const float fifo_ref_0_delayed_1 =
          fifo_ref_0_delayed_1_buf[fifo_ref_0_delayed_1_ptr];
      fifo_st_0.write(fifo_ref_0_delayed_1);
      fifo_ref_0_delayed_1_buf[fifo_ref_0_delayed_1_ptr] = fifo_ref_0;
      fifo_ref_0_delayed_1_ptr =
          fifo_ref_0_delayed_1_ptr < 0 ? fifo_ref_0_delayed_1_ptr + 1 : 0;
    }
  }
  fifo_st_0.close();
}

void Module5Func(tlp::ostream<float>& fifo_st_0,
                 tlp::istream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_50_buf[50];
  int fifo_ref_0_delayed_50_ptr = 0;
module_5_epoch:
  for (;;) {
    bool eos;
    if (fifo_ld_0.try_eos(eos)) {
      if (eos) break;
#pragma HLS dependence variable = fifo_ref_0_delayed_50_buf inter false
      bool succeed;
      auto fifo_ref_0 = fifo_ld_0.read(succeed);
      const float fifo_ref_0_delayed_50 =
          fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr];
      fifo_st_0.write(fifo_ref_0_delayed_50);
      fifo_ref_0_delayed_50_buf[fifo_ref_0_delayed_50_ptr] = fifo_ref_0;
      fifo_ref_0_delayed_50_ptr =
          fifo_ref_0_delayed_50_ptr < 49 ? fifo_ref_0_delayed_50_ptr + 1 : 0;
    }
  }
  fifo_st_0.close();
}

void Module6Func(tlp::ostream<float>& fifo_st_0, tlp::istream<float>& fifo_ld_0,
                 tlp::istream<float>& fifo_ld_1,
                 tlp::istream<float>& fifo_ld_2) {
module_6_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    bool eos_2;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1) &&
        fifo_ld_2.try_eos(eos_2)) {
      if (eos_0 || eos_1 || eos_2) break;
      bool succeed_0;
      bool succeed_1;
      bool succeed_2;
      auto fifo_ref_0 = fifo_ld_0.read(succeed_0);
      auto fifo_ref_1 = fifo_ld_1.read(succeed_1);
      auto fifo_ref_2 = fifo_ld_2.read(succeed_2);
      fifo_st_0.write((fifo_ref_0 + fifo_ref_1 + fifo_ref_2) * 0.2f);
    }
  }
  fifo_st_0.close();
}

void Module7Func(tlp::ostream<float>& fifo_st_0,
                 tlp::istream<float>& fifo_ld_0) {
  float fifo_ref_0_delayed_49_buf[49];
  int fifo_ref_0_delayed_49_ptr = 0;
module_7_epoch:
  for (;;) {
    bool eos;
    if (fifo_ld_0.try_eos(eos)) {
      if (eos) break;
#pragma HLS dependence variable = fifo_ref_0_delayed_49_buf inter false
      bool succeed;
      auto fifo_ref_0 = fifo_ld_0.read(succeed);

      const float fifo_ref_0_delayed_49 =
          fifo_ref_0_delayed_49_buf[fifo_ref_0_delayed_49_ptr];
      fifo_st_0.write(fifo_ref_0_delayed_49);
      fifo_ref_0_delayed_49_buf[fifo_ref_0_delayed_49_ptr] = fifo_ref_0;
      fifo_ref_0_delayed_49_ptr =
          fifo_ref_0_delayed_49_ptr < 48 ? fifo_ref_0_delayed_49_ptr + 1 : 0;
    }
  }
  fifo_st_0.close();
}

void Module8Func(tlp::ostream<coalesce_t<float, 2>>& dram_t0_bank_0_fifo,
                 tlp::istream<float>& fifo_ld_0,
                 tlp::istream<float>& fifo_ld_1) {
module_8_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1)) {
      if (eos_0 || eos_1) break;
      bool succeed_0;
      bool succeed_1;
      dram_t0_bank_0_fifo.write(
          {fifo_ld_0.read(succeed_0), fifo_ld_1.read(succeed_1)});
    }
  }
  dram_t0_bank_0_fifo.close();
}

void Jacobi(tlp::mmap<float> bank_0_t0, tlp::mmap<const float> bank_0_t1,
            uint64_t coalesced_data_num) {
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
      .invoke<0>(Mmap2Stream, bank_0_t1, coalesced_data_num, bank_0_t1_buf)
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
      .invoke<0>(Stream2Mmap, bank_0_t0_buf, bank_0_t0);
}
