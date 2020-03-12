#include <tlp.h>

void Mmap2Stream(tlp::mmap<const float> mmap, uint64_t n,
                 tlp::ostream<tlp::vec_t<float, 2>>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
#pragma HLS pipeline II = 2
    tlp::vec_t<float, 2> tmp;
    tmp.set(0, mmap[i * 2]);
    tmp.set(1, mmap[i * 2 + 1]);
    stream.write(tmp);
  }
  stream.close();
}

void Stream2Mmap(tlp::istream<tlp::vec_t<float, 2>>& stream,
                 tlp::mmap<float> mmap) {
  for (uint64_t i = 0;;) {
    bool eos;
    if (stream.try_eos(eos)) {
      if (eos) break;
#pragma HLS pipeline II = 2
      auto packed = stream.read(nullptr);
      mmap[i * 2] = packed[0];
      mmap[i * 2 + 1] = packed[1];
      ++i;
    }
  }
}

void Module0Func(tlp::ostream<float>& fifo_st_0, tlp::ostream<float>& fifo_st_1,
                 tlp::istream<tlp::vec_t<float, 2>>& dram_t1_bank_0_fifo) {
module_0_epoch:
  for (;;) {
    bool eos;
    if (dram_t1_bank_0_fifo.try_eos(eos)) {
      if (eos) break;
      auto dram_t1_bank_0_buf = dram_t1_bank_0_fifo.read(nullptr);
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
      auto fifo_ref_0 = fifo_ld_0.read(nullptr);
      fifo_st_0.write(fifo_ref_0);
      fifo_st_1.write(fifo_ref_0);
    }
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module3Func1(tlp::ostream<float>& fifo_st_0,
                  tlp::istream<float>& fifo_ld_0,
                  tlp::istream<float>& fifo_ld_1) {
  const int delay_0 = 50;
  int count = 0;
module_3_1_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1)) {
      if (eos_0 || eos_1) break;
      float fifo_ref_0;
      bool do_ld_0 = count >= delay_0;
      if (do_ld_0) {
        fifo_ref_0 = fifo_ld_0.read(nullptr);
      }
      float fifo_ref_1 = fifo_ld_1.read(nullptr);
      fifo_st_0.write(fifo_ref_0 + fifo_ref_1);
      if (!do_ld_0) {
        ++count;
      }
    }
  }
  fifo_st_0.close();
}

void Module3Func2(tlp::ostream<float>& fifo_st_0,
                  tlp::istream<float>& fifo_ld_0,
                  tlp::istream<float>& fifo_ld_1) {
  const int delay_0 = 51;
  int count = 0;
module_3_2_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1)) {
      if (eos_0 || eos_1) break;
      float fifo_ref_0;
      bool do_ld_0 = count >= delay_0;
      if (do_ld_0) {
        fifo_ref_0 = fifo_ld_0.read(nullptr);
      }
      float fifo_ref_1 = fifo_ld_1.read(nullptr);
      fifo_st_0.write(fifo_ref_0 + fifo_ref_1);
      if (!do_ld_0) {
        ++count;
      }
    }
  }
  fifo_st_0.close();
}

void Module6Func1(tlp::ostream<float>& fifo_st_0,
                  tlp::istream<float>& fifo_ld_0,
                  tlp::istream<float>& fifo_ld_1,
                  tlp::istream<float>& fifo_ld_2) {
  const int delay_0 = 50;
  const int delay_2 = 50;
  int count = 0;
module_6_1_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    bool eos_2;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1) &&
        fifo_ld_2.try_eos(eos_2)) {
      if (eos_0 || eos_1 || eos_2) break;
      float fifo_ref_0;
      bool do_ld_0 = count >= delay_0;
      if (do_ld_0) {
        fifo_ref_0 = fifo_ld_0.read(nullptr);
      }
      auto fifo_ref_1 = fifo_ld_1.read(nullptr);
      float fifo_ref_2;
      bool do_ld_2 = count >= delay_2;
      if (do_ld_2) {
        fifo_ref_2 = fifo_ld_2.read(nullptr);
      }
      fifo_st_0.write((fifo_ref_0 + fifo_ref_1 + fifo_ref_2) * 0.2f);
      if (!do_ld_0 || !do_ld_2) {
        ++count;
      }
    }
  }
  fifo_st_0.close();
}
void Module6Func2(tlp::ostream<float>& fifo_st_0,
                  tlp::istream<float>& fifo_ld_0,
                  tlp::istream<float>& fifo_ld_1,
                  tlp::istream<float>& fifo_ld_2) {
  const int delay_0 = 49;
  const int delay_2 = 50;
  int count = 0;
module_6_2_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    bool eos_2;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1) &&
        fifo_ld_2.try_eos(eos_2)) {
      if (eos_0 || eos_1 || eos_2) break;
      float fifo_ref_0;
      bool do_ld_0 = count >= delay_0;
      if (do_ld_0) {
        fifo_ref_0 = fifo_ld_0.read(nullptr);
      }
      auto fifo_ref_1 = fifo_ld_1.read(nullptr);
      float fifo_ref_2;
      bool do_ld_2 = count >= delay_2;
      if (do_ld_2) {
        fifo_ref_2 = fifo_ld_2.read(nullptr);
      }
      fifo_st_0.write((fifo_ref_0 + fifo_ref_1 + fifo_ref_2) * 0.2f);
      if (!do_ld_0 || !do_ld_2) {
        ++count;
      }
    }
  }
  fifo_st_0.close();
}

void Module8Func(tlp::ostream<tlp::vec_t<float, 2>>& dram_t0_bank_0_fifo,
                 tlp::istream<float>& fifo_ld_0,
                 tlp::istream<float>& fifo_ld_1) {
module_8_epoch:
  for (;;) {
    bool eos_0;
    bool eos_1;
    if (fifo_ld_0.try_eos(eos_0) && fifo_ld_1.try_eos(eos_1)) {
      if (eos_0 || eos_1) break;
      tlp::vec_t<float, 2> tmp;
      tmp.set(0, fifo_ld_0.read(nullptr));
      tmp.set(1, fifo_ld_1.read(nullptr));
      dram_t0_bank_0_fifo.write(tmp);
    }
  }
  dram_t0_bank_0_fifo.close();
}

void Jacobi(tlp::mmap<float> bank_0_t0, tlp::mmap<const float> bank_0_t1,
            uint64_t coalesced_data_num) {
  tlp::stream<tlp::vec_t<float, 2>, 32> bank_0_t1_buf("bank_0_t1_buf");
  tlp::stream<tlp::vec_t<float, 2>, 32> bank_0_t0_buf("bank_0_t0_buf");
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
  tlp::stream<float, 58> from_t1_offset_2000_to_t0_pe_1(
      "from_t1_offset_2000_to_t0_pe_1");
  tlp::stream<float, 52> from_t1_offset_2001_to_tcse_var_0_pe_1(
      "from_t1_offset_2001_to_tcse_var_0_pe_1");
  tlp::stream<float, 56> from_t1_offset_2001_to_t0_pe_0(
      "from_t1_offset_2001_to_t0_pe_0");
  tlp::stream<float, 2> from_tcse_var_0_pe_1_to_tcse_var_0_offset_0(
      "from_tcse_var_0_pe_1_to_tcse_var_0_offset_0");
  tlp::stream<float, 53> from_t1_offset_2000_to_tcse_var_0_pe_0(
      "from_t1_offset_2000_to_tcse_var_0_pe_0");
  tlp::stream<float, 2> from_tcse_var_0_pe_0_to_tcse_var_0_offset_1(
      "from_tcse_var_0_pe_0_to_tcse_var_0_offset_1");
  tlp::stream<float, 6> from_tcse_var_0_offset_0_to_t0_pe_1(
      "from_tcse_var_0_offset_0_to_t0_pe_1");
  tlp::stream<float, 2> from_tcse_var_0_offset_1_to_t0_pe_0(
      "from_tcse_var_0_offset_1_to_t0_pe_0");
  tlp::stream<float, 52> from_tcse_var_0_offset_0_to_t0_pe_0(
      "from_tcse_var_0_offset_0_to_t0_pe_0");
  tlp::stream<float, 4> from_t0_pe_0_to_super_sink(
      "from_t0_pe_0_to_super_sink");
  tlp::stream<float, 51> from_tcse_var_0_offset_1_to_t0_pe_1(
      "from_tcse_var_0_offset_1_to_t0_pe_1");
  tlp::stream<float, 2> from_t0_pe_1_to_super_sink(
      "from_t0_pe_1_to_super_sink");

  tlp::task()
      .invoke<0>(Mmap2Stream, "Mmap2Stream", bank_0_t1, coalesced_data_num,
                 bank_0_t1_buf)
      .invoke<0>(Module0Func, "Module0Func",
                 /*output*/ from_super_source_to_t1_offset_0,
                 /*output*/ from_super_source_to_t1_offset_1,
                 /* input*/ bank_0_t1_buf)
      .invoke<0>(Module1Func, "Module1Func#1",
                 /*output*/ from_t1_offset_0_to_t1_offset_2000,
                 /*output*/ from_t1_offset_0_to_tcse_var_0_pe_1,
                 /* input*/ from_super_source_to_t1_offset_0)
      .invoke<0>(Module1Func, "Module1Func#2",
                 /*output*/ from_t1_offset_1_to_t1_offset_2001,
                 /*output*/ from_t1_offset_1_to_tcse_var_0_pe_0,
                 /* input*/ from_super_source_to_t1_offset_1)
      .invoke<0>(Module1Func, "Module2Func#1",
                 /*output*/ from_t1_offset_2000_to_tcse_var_0_pe_0,
                 /*output*/ from_t1_offset_2000_to_t0_pe_1,
                 /* input*/ from_t1_offset_0_to_t1_offset_2000)
      .invoke<0>(Module1Func, "Module2Func#2",
                 /*output*/ from_t1_offset_2001_to_tcse_var_0_pe_1,
                 /*output*/ from_t1_offset_2001_to_t0_pe_0,
                 /* input*/ from_t1_offset_1_to_t1_offset_2001)
      .invoke<0>(Module3Func1, "Module3Func#1",
                 /*output*/ from_tcse_var_0_pe_1_to_tcse_var_0_offset_0,
                 /* input*/ from_t1_offset_2001_to_tcse_var_0_pe_1,
                 /* input*/ from_t1_offset_0_to_tcse_var_0_pe_1)
      .invoke<0>(Module3Func2, "Module3Func#2",
                 /*output*/ from_tcse_var_0_pe_0_to_tcse_var_0_offset_1,
                 /* input*/ from_t1_offset_2000_to_tcse_var_0_pe_0,
                 /* input*/ from_t1_offset_1_to_tcse_var_0_pe_0)
      .invoke<0>(Module1Func, "Module1Func#3",
                 /*output*/ from_tcse_var_0_offset_0_to_t0_pe_0,
                 /*output*/ from_tcse_var_0_offset_0_to_t0_pe_1,
                 /* input*/ from_tcse_var_0_pe_1_to_tcse_var_0_offset_0)
      .invoke<0>(Module1Func, "Module1Func#4",
                 /*output*/ from_tcse_var_0_offset_1_to_t0_pe_1,
                 /*output*/ from_tcse_var_0_offset_1_to_t0_pe_0,
                 /* input*/ from_tcse_var_0_pe_0_to_tcse_var_0_offset_1)
      .invoke<0>(Module6Func1, "Module6Func#1",
                 /*output*/ from_t0_pe_0_to_super_sink,
                 /* input*/ from_tcse_var_0_offset_0_to_t0_pe_0,
                 /* input*/ from_tcse_var_0_offset_1_to_t0_pe_0,
                 /* input*/ from_t1_offset_2001_to_t0_pe_0)
      .invoke<0>(Module6Func2, "Module6Func#2",
                 /*output*/ from_t0_pe_1_to_super_sink,
                 /* input*/ from_tcse_var_0_offset_1_to_t0_pe_1,
                 /* input*/ from_tcse_var_0_offset_0_to_t0_pe_1,
                 /* input*/ from_t1_offset_2000_to_t0_pe_1)
      .invoke<0>(Module8Func, "Module8Func",
                 /*output*/ bank_0_t0_buf,
                 /* input*/ from_t0_pe_0_to_super_sink,
                 /* input*/ from_t0_pe_1_to_super_sink)
      .invoke<0>(Stream2Mmap, "Stream2Mmap", bank_0_t0_buf, bank_0_t0);
}
