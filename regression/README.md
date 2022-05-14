This directory includes a set of real-world TAPA designs. We are keep adding more to this directory.

- `cnn` and `lu_decomposition` are both systolic arrays (of different shape) originally published in [FPGA'21](https://dl.acm.org/doi/pdf/10.1145/3431920.3439292).

- `hbm-bandwidth` can be used to profile the HBM bandwidth. It reads from and write back to all 32 channels of HBM. It is a good demonstration of the expressiveness and the optimized area of `async_mmap`.

- `hbm-bandwidth-1-ch` only reads from and writes to one HBM channel.

- `serpens` is a sparse matrix-vector multiplication published in [DAC'22](https://arxiv.org/pdf/2111.12555.pdf). We provide different versions that are of the same architecture but different parallelism.

- `spmm` is a sparse matrix-matrix multiplication published in [FPGA'22](https://dl.acm.org/doi/pdf/10.1145/3490422.3502357)

- `spmv-hisparse-mmap` is another sparse matrix-vector multiplication published in [FPGA'22](https://www.csl.cornell.edu/~zhiruz/pdfs/spmv-fpga2022.pdf)

- `knn` is a K-nearest-neighbor accelerator originally published in [FPT'20](http://www.sfu.ca/~zhenman/files/C19-FPT2020-CHIP-KNN.pdf)

- `page_rank` is an accelerator for the Page-Rank algorithm that is included in [FCCM'21](https://about.blaok.me/pub/fccm21-tapa.pdf)
