#include <tapa.h>

template <typename mmap_t, typename stream_t, typename addr_t, typename count_t, typename stride_t>
void async_mmap_read_to_fifo(
    tapa::async_mmap<mmap_t>& mem,
    tapa::ostream<stream_t>& fifo,
    addr_t base_addr,
    count_t count,
    stride_t stride){
#pragma HLS inline
  for (int i_req = 0, i_resp = 0; i_resp < count;) {
#pragma HLS pipeline II=1
    if (i_req < count &&
        mem.read_addr.try_write(base_addr + i_req * stride)) {
        ++i_req;
    }
    if (!fifo.full() && !mem.read_data.empty()) {
        fifo.try_write(mem.read_data.read(nullptr));
        ++i_resp;
    }
  }
}

template <typename mmap_t, typename addr_t, typename data_t>
void async_mmap_read_to_array(
    tapa::async_mmap<mmap_t>& mem,
    data_t* array,
    addr_t base_addr,
    unsigned int count,
    unsigned int stride)
{
#pragma HLS inline
  for (int i_req = 0, i_resp = 0; i_resp < count;) {
#pragma HLS pipeline II=1
    if (i_req < count &&
        mem.read_addr.try_write(base_addr + i_req * stride)) {
        ++i_req;
    }
    if (!mem.read_data.empty()) {
        array[i_resp] = mem.read_data.read(nullptr);
        ++i_resp;
    }
  }
}

template <typename mmap_t, typename data_t, typename addr_t>
mmap_t async_mmap_read(
    tapa::async_mmap<mmap_t>& mem,
    addr_t addr)
{
#pragma HLS inline
  mem.read_addr.write(addr);
  return mem.read_data.read();
}

template <typename mmap_t, typename stream_t, typename addr_t, typename count_t, typename stride_t>
void async_mmap_write_from_fifo(
    tapa::async_mmap<mmap_t>& mem,
    tapa::istream<stream_t>& fifo,
    addr_t base_addr,
    count_t count,
    stride_t stride)
{
#pragma HLS inline
  for(int i_req = 0, i_resp = 0; i_resp < count;) {
#pragma HLS pipeline II=1
    if (i_req < count &&
        !fifo.empty() &&
        !mem.write_addr.full() &&
        !mem.write_data.full() 
    ) {
      mem.write_addr.try_write(base_addr + i_req * stride);
      mem.write_data.try_write(fifo.read(nullptr));
      ++i_req;
    }
    if (!mem.write_resp.empty()) {
      i_resp += unsigned(mem.write_resp.read(nullptr)) + 1;
    }
  }
}

template <typename mmap_t, typename addr_t, typename data_t>
void async_mmap_write_from_array(
    tapa::async_mmap<mmap_t>& mem,
    data_t* array,
    addr_t base_addr,
    unsigned int count,
    unsigned int stride)
{
#pragma HLS inline
  for(int i_req = 0, i_resp = 0; i_resp < count;) {
#pragma HLS pipeline II=1
    if (i_req < count &&
        !mem.write_addr.full() &&
        !mem.write_data.full() 
    ) {
      mem.write_addr.try_write(base_addr + i_req * stride);
      mem.write_data.try_write(array[i_req]);
      ++i_req;
    }
    if (!mem.write_resp.empty()) {
      i_resp += unsigned(mem.write_resp.read(nullptr)) + 1;
    }
  }
}

template <typename mmap_t, typename data_t, typename addr_t>
void async_mmap_blocking_write(
    tapa::async_mmap<mmap_t>& mem,
    addr_t addr,
    data_t data)
{
#pragma HLS inline
  async_mmap_blocking_write(mem, addr, data);
  mem.write_resp.read();
}

template <typename mmap_t, typename data_t, typename addr_t>
void async_mmap_non_blocking_write(
    tapa::async_mmap<mmap_t>& mem,
    addr_t addr,
    data_t data)
{
#pragma HLS inline
  mem.write_addr.write(addr);
  mem.write_data.write(data);
}
