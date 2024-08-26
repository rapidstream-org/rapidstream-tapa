// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>

void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
                 tapa::ostream<tapa::vec_t<float, 2>>& stream) {
  [[tapa::pipeline(2)]] for (uint64_t i = 0; i < n; ++i) {
    tapa::vec_t<float, 2> tmp;
    tmp.set(0, mmap[i * 2]);
    tmp.set(1, mmap[i * 2 + 1]);
    stream.write(tmp);
  }
  stream.close();
}

void Stream2Mmap(tapa::istream<tapa::vec_t<float, 2>>& stream,
                 tapa::mmap<float> mmap) {
  [[tapa::pipeline(2)]] for (uint64_t i = 0;;) {
    bool eot;
    if (stream.try_eot(eot)) {
      if (eot) break;
      auto packed = stream.read(nullptr);
      mmap[i * 2] = packed[0];
      mmap[i * 2 + 1] = packed[1];
      ++i;
    }
  }
}

void Module0Func(tapa::ostream<float>& fifo_st_0,
                 tapa::ostream<float>& fifo_st_1,
                 tapa::istream<tapa::vec_t<float, 2>>& dram_t1_bank_0_fifo) {
module_0_epoch:
  TAPA_WHILE_NOT_EOT(dram_t1_bank_0_fifo) {
    auto dram_t1_bank_0_buf = dram_t1_bank_0_fifo.read(nullptr);
    fifo_st_0.write(dram_t1_bank_0_buf[1]);
    fifo_st_1.write(dram_t1_bank_0_buf[0]);
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module1Func(tapa::ostream<float>& fifo_st_0,
                 tapa::ostream<float>& fifo_st_1,
                 tapa::istream<float>& fifo_ld_0) {
module_1_epoch:
  TAPA_WHILE_NOT_EOT(fifo_ld_0) {
    auto fifo_ref_0 = fifo_ld_0.read(nullptr);
    fifo_st_0.write(fifo_ref_0);
    fifo_st_1.write(fifo_ref_0);
  }
  fifo_st_0.close();
  fifo_st_1.close();
}

void Module3Func1(tapa::ostream<float>& fifo_st_0,
                  tapa::istream<float>& fifo_ld_0,
                  tapa::istream<float>& fifo_ld_1) {
  const int delay_0 = 50;
  int count = 0;
module_3_1_epoch:
  TAPA_WHILE_NEITHER_EOT(fifo_ld_0, fifo_ld_1) {
    float fifo_ref_0 = 0.f;
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
  fifo_st_0.close();
}

void Module3Func2(tapa::ostream<float>& fifo_st_0,
                  tapa::istream<float>& fifo_ld_0,
                  tapa::istream<float>& fifo_ld_1) {
  const int delay_0 = 51;
  int count = 0;
module_3_2_epoch:
  TAPA_WHILE_NEITHER_EOT(fifo_ld_0, fifo_ld_1) {
    float fifo_ref_0 = 0.f;
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
  fifo_st_0.close();
}

void Module6Func1(tapa::ostream<float>& fifo_st_0,
                  tapa::istream<float>& fifo_ld_0,
                  tapa::istream<float>& fifo_ld_1,
                  tapa::istream<float>& fifo_ld_2) {
  const int delay_0 = 50;
  const int delay_2 = 50;
  int count = 0;
module_6_1_epoch:
  TAPA_WHILE_NONE_EOT(fifo_ld_0, fifo_ld_1, fifo_ld_2) {
    float fifo_ref_0 = 0.f;
    bool do_ld_0 = count >= delay_0;
    if (do_ld_0) {
      fifo_ref_0 = fifo_ld_0.read(nullptr);
    }
    auto fifo_ref_1 = fifo_ld_1.read(nullptr);
    float fifo_ref_2 = 0.f;
    bool do_ld_2 = count >= delay_2;
    if (do_ld_2) {
      fifo_ref_2 = fifo_ld_2.read(nullptr);
    }
    fifo_st_0.write((fifo_ref_0 + fifo_ref_1 + fifo_ref_2) * 0.2f);
    if (!do_ld_0 || !do_ld_2) {
      ++count;
    }
  }
  fifo_st_0.close();
}
void Module6Func2(tapa::ostream<float>& fifo_st_0,
                  tapa::istream<float>& fifo_ld_0,
                  tapa::istream<float>& fifo_ld_1,
                  tapa::istream<float>& fifo_ld_2) {
  const int delay_0 = 49;
  const int delay_2 = 50;
  int count = 0;
module_6_2_epoch:
  TAPA_WHILE_NONE_EOT(fifo_ld_0, fifo_ld_1, fifo_ld_2) {
    float fifo_ref_0 = 0.f;
    bool do_ld_0 = count >= delay_0;
    if (do_ld_0) {
      fifo_ref_0 = fifo_ld_0.read(nullptr);
    }
    auto fifo_ref_1 = fifo_ld_1.read(nullptr);
    float fifo_ref_2 = 0.f;
    bool do_ld_2 = count >= delay_2;
    if (do_ld_2) {
      fifo_ref_2 = fifo_ld_2.read(nullptr);
    }
    fifo_st_0.write((fifo_ref_0 + fifo_ref_1 + fifo_ref_2) * 0.2f);
    if (!do_ld_0 || !do_ld_2) {
      ++count;
    }
  }
  fifo_st_0.close();
}

void Module8Func(tapa::ostream<tapa::vec_t<float, 2>>& dram_t0_bank_0_fifo,
                 tapa::istream<float>& fifo_ld_0,
                 tapa::istream<float>& fifo_ld_1) {
module_8_epoch:
  TAPA_WHILE_NEITHER_EOT(fifo_ld_0, fifo_ld_1) {
    tapa::vec_t<float, 2> tmp;
    tmp.set(0, fifo_ld_0.read(nullptr));
    tmp.set(1, fifo_ld_1.read(nullptr));
    dram_t0_bank_0_fifo.write(tmp);
  }
  dram_t0_bank_0_fifo.close();
}

void Jacobi(tapa::mmap<float> bank_0_t0, tapa::mmap<const float> bank_0_t1,
            uint64_t coalesced_data_num) {
  tapa::stream<tapa::vec_t<float, 2>, 32> bank_0_t1_buf("bank_0_t1_buf");
  tapa::stream<tapa::vec_t<float, 2>, 32> bank_0_t0_buf("bank_0_t0_buf");
  tapa::stream<float, 2> from_super_source_to_t1_offset_0(
      "from_super_source_to_t1_offset_0");
  tapa::stream<float, 2> from_super_source_to_t1_offset_1(
      "from_super_source_to_t1_offset_1");
  tapa::stream<float, 2> from_t1_offset_0_to_t1_offset_2000(
      "from_t1_offset_0_to_t1_offset_2000");
  tapa::stream<float, 4> from_t1_offset_0_to_tcse_var_0_pe_1(
      "from_t1_offset_0_to_tcse_var_0_pe_1");
  tapa::stream<float, 2> from_t1_offset_1_to_t1_offset_2001(
      "from_t1_offset_1_to_t1_offset_2001");
  tapa::stream<float, 6> from_t1_offset_1_to_tcse_var_0_pe_0(
      "from_t1_offset_1_to_tcse_var_0_pe_0");
  tapa::stream<float, 58> from_t1_offset_2000_to_t0_pe_1(
      "from_t1_offset_2000_to_t0_pe_1");
  tapa::stream<float, 52> from_t1_offset_2001_to_tcse_var_0_pe_1(
      "from_t1_offset_2001_to_tcse_var_0_pe_1");
  tapa::stream<float, 56> from_t1_offset_2001_to_t0_pe_0(
      "from_t1_offset_2001_to_t0_pe_0");
  tapa::stream<float, 2> from_tcse_var_0_pe_1_to_tcse_var_0_offset_0(
      "from_tcse_var_0_pe_1_to_tcse_var_0_offset_0");
  tapa::stream<float, 53> from_t1_offset_2000_to_tcse_var_0_pe_0(
      "from_t1_offset_2000_to_tcse_var_0_pe_0");
  tapa::stream<float, 2> from_tcse_var_0_pe_0_to_tcse_var_0_offset_1(
      "from_tcse_var_0_pe_0_to_tcse_var_0_offset_1");
  tapa::stream<float, 6> from_tcse_var_0_offset_0_to_t0_pe_1(
      "from_tcse_var_0_offset_0_to_t0_pe_1");
  tapa::stream<float, 2> from_tcse_var_0_offset_1_to_t0_pe_0(
      "from_tcse_var_0_offset_1_to_t0_pe_0");
  tapa::stream<float, 52> from_tcse_var_0_offset_0_to_t0_pe_0(
      "from_tcse_var_0_offset_0_to_t0_pe_0");
  tapa::stream<float, 4> from_t0_pe_0_to_super_sink(
      "from_t0_pe_0_to_super_sink");
  tapa::stream<float, 51> from_tcse_var_0_offset_1_to_t0_pe_1(
      "from_tcse_var_0_offset_1_to_t0_pe_1");
  tapa::stream<float, 2> from_t0_pe_1_to_super_sink(
      "from_t0_pe_1_to_super_sink");

  tapa::task()
      .invoke(Mmap2Stream, "Mmap2Stream", bank_0_t1, coalesced_data_num,
              bank_0_t1_buf)
      .invoke(Module0Func, "Module0Func",
              /*output*/ from_super_source_to_t1_offset_0,
              /*output*/ from_super_source_to_t1_offset_1,
              /* input*/ bank_0_t1_buf)
      .invoke(Module1Func, "Module1Func#1",
              /*output*/ from_t1_offset_0_to_t1_offset_2000,
              /*output*/ from_t1_offset_0_to_tcse_var_0_pe_1,
              /* input*/ from_super_source_to_t1_offset_0)
      .invoke(Module1Func, "Module1Func#2",
              /*output*/ from_t1_offset_1_to_t1_offset_2001,
              /*output*/ from_t1_offset_1_to_tcse_var_0_pe_0,
              /* input*/ from_super_source_to_t1_offset_1)
      .invoke(Module1Func, "Module2Func#1",
              /*output*/ from_t1_offset_2000_to_tcse_var_0_pe_0,
              /*output*/ from_t1_offset_2000_to_t0_pe_1,
              /* input*/ from_t1_offset_0_to_t1_offset_2000)
      .invoke(Module1Func, "Module2Func#2",
              /*output*/ from_t1_offset_2001_to_tcse_var_0_pe_1,
              /*output*/ from_t1_offset_2001_to_t0_pe_0,
              /* input*/ from_t1_offset_1_to_t1_offset_2001)
      .invoke(Module3Func1, "Module3Func#1",
              /*output*/ from_tcse_var_0_pe_1_to_tcse_var_0_offset_0,
              /* input*/ from_t1_offset_2001_to_tcse_var_0_pe_1,
              /* input*/ from_t1_offset_0_to_tcse_var_0_pe_1)
      .invoke(Module3Func2, "Module3Func#2",
              /*output*/ from_tcse_var_0_pe_0_to_tcse_var_0_offset_1,
              /* input*/ from_t1_offset_2000_to_tcse_var_0_pe_0,
              /* input*/ from_t1_offset_1_to_tcse_var_0_pe_0)
      .invoke(Module1Func, "Module1Func#3",
              /*output*/ from_tcse_var_0_offset_0_to_t0_pe_0,
              /*output*/ from_tcse_var_0_offset_0_to_t0_pe_1,
              /* input*/ from_tcse_var_0_pe_1_to_tcse_var_0_offset_0)
      .invoke(Module1Func, "Module1Func#4",
              /*output*/ from_tcse_var_0_offset_1_to_t0_pe_1,
              /*output*/ from_tcse_var_0_offset_1_to_t0_pe_0,
              /* input*/ from_tcse_var_0_pe_0_to_tcse_var_0_offset_1)
      .invoke(Module6Func1, "Module6Func#1",
              /*output*/ from_t0_pe_0_to_super_sink,
              /* input*/ from_tcse_var_0_offset_0_to_t0_pe_0,
              /* input*/ from_tcse_var_0_offset_1_to_t0_pe_0,
              /* input*/ from_t1_offset_2001_to_t0_pe_0)
      .invoke(Module6Func2, "Module6Func#2",
              /*output*/ from_t0_pe_1_to_super_sink,
              /* input*/ from_tcse_var_0_offset_1_to_t0_pe_1,
              /* input*/ from_tcse_var_0_offset_0_to_t0_pe_1,
              /* input*/ from_t1_offset_2000_to_t0_pe_1)
      .invoke(Module8Func, "Module8Func",
              /*output*/ bank_0_t0_buf,
              /* input*/ from_t0_pe_0_to_super_sink,
              /* input*/ from_t0_pe_1_to_super_sink)
      .invoke(Stream2Mmap, "Stream2Mmap", bank_0_t0_buf, bank_0_t0);
}
