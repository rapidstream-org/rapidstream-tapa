#ifndef GRAPHLILY_IO_DATA_LOADER_H_
#define GRAPHLILY_IO_DATA_LOADER_H_

#include <cstdint>
#include <vector>

#include "cnpy.h"


namespace spmv {
namespace io {

//--------------------------------------------------
// Compressed Sparse Row (CSR) format support
//--------------------------------------------------

// Data structure for csr matrix.
template<typename data_type>
struct CSRMatrix {
    /*! \brief The number of rows of the sparse matrix */
    uint32_t num_rows;
    /*! \brief The number of columns of the sparse matrix */
    uint32_t num_cols;
    /*! \brief The non-zero data of the sparse matrix */
    std::vector<data_type> adj_data;
    /*! \brief The column indices of the sparse matrix */
    std::vector<uint32_t> adj_indices;
    /*! \brief The index pointers of the sparse matrix */
    std::vector<uint32_t> adj_indptr;
};


// Create a csr matrix from raw input.
template<typename data_type>
CSRMatrix<data_type> create_csr_matrix(uint32_t num_rows,
                                       uint32_t num_cols,
                                       std::vector<data_type> const &adj_data,
                                       std::vector<uint32_t> const &adj_indices,
                                       std::vector<uint32_t> const &adj_indptr) {
    CSRMatrix<data_type> csr_matrix;
    csr_matrix.num_rows = num_rows;
    csr_matrix.num_cols = num_cols;
    csr_matrix.adj_data = adj_data;
    csr_matrix.adj_indices = adj_indices;
    csr_matrix.adj_indptr = adj_indptr;
    return csr_matrix;
}


// Load a csr matrix from a scipy sparse npz file. The sparse matrix should have float data type.
CSRMatrix<float> load_csr_matrix_from_float_npz(std::string csr_float_npz_path) {
    CSRMatrix<float> csr_matrix;
    cnpy::npz_t npz = cnpy::npz_load(csr_float_npz_path);
    cnpy::NpyArray npy_shape = npz["shape"];
    uint32_t num_rows = npy_shape.data<uint32_t>()[0];
    uint32_t num_cols = npy_shape.data<uint32_t>()[2];
    csr_matrix.num_rows = num_rows;
    csr_matrix.num_cols = num_cols;
    cnpy::NpyArray npy_data = npz["data"];
    uint32_t nnz = npy_data.shape[0];
    cnpy::NpyArray npy_indices = npz["indices"];
    cnpy::NpyArray npy_indptr = npz["indptr"];
    csr_matrix.adj_data.insert(csr_matrix.adj_data.begin(), &npy_data.data<float>()[0],
        &npy_data.data<float>()[nnz]);
    csr_matrix.adj_indices.insert(csr_matrix.adj_indices.begin(), &npy_indices.data<uint32_t>()[0],
        &npy_indices.data<uint32_t>()[nnz]);
    csr_matrix.adj_indptr.insert(csr_matrix.adj_indptr.begin(), &npy_indptr.data<uint32_t>()[0],
        &npy_indptr.data<uint32_t>()[num_rows + 1]);
    return csr_matrix;
}


// Convert a float csr matrix to another data type.
// TODO: does ap_int make formatting slower than float?
template<typename data_type>
CSRMatrix<data_type> csr_matrix_convert_from_float(CSRMatrix<float> const &in) {
    CSRMatrix<data_type> out;
    out.num_rows = in.num_rows;
    out.num_cols = in.num_cols;
    std::copy(in.adj_data.begin(), in.adj_data.end(), std::back_inserter(out.adj_data));
    out.adj_indices = in.adj_indices;
    out.adj_indptr = in.adj_indptr;
    return out;
}


//--------------------------------------------------
// Compressed Sparse Colunm (CSC) format support
//--------------------------------------------------

// Data structure for csc matrix.
template<typename data_type>
struct CSCMatrix {
    /*! \brief The number of rows of the sparse matrix */
    uint32_t num_rows;
    /*! \brief The number of columns of the sparse matrix */
    uint32_t num_cols;
    /*! \brief The non-zero data of the sparse matrix */
    std::vector<data_type> adj_data;
    /*! \brief The row indices of the sparse matrix */
    std::vector<uint32_t> adj_indices;
    /*! \brief The index pointers of the sparse matrix */
    std::vector<uint32_t> adj_indptr;
};


// Convert csr to csc.
template<typename data_type>
CSCMatrix<data_type> csr2csc(CSRMatrix<data_type> const &csr_matrix) {
    CSCMatrix<data_type> csc_matrix;
    csc_matrix.num_rows = csr_matrix.num_rows;
    csc_matrix.num_cols = csr_matrix.num_cols;
    csc_matrix.adj_data = std::vector<data_type>(csr_matrix.adj_data.size());
    csc_matrix.adj_indices = std::vector<uint32_t>(csr_matrix.adj_indices.size());
    csc_matrix.adj_indptr = std::vector<uint32_t>(csc_matrix.num_cols + 1);
    // Convert adj_indptr
    uint32_t nnz = csr_matrix.adj_indptr[csr_matrix.num_rows];
    std::vector<uint32_t> nnz_each_col(csc_matrix.num_cols);
    std::fill(nnz_each_col.begin(), nnz_each_col.end(), 0);
    for (size_t n = 0; n < nnz; n++) {
        nnz_each_col[csr_matrix.adj_indices[n]]++;
    }
    csc_matrix.adj_indptr[0] = 0;
    for (size_t col_idx = 0; col_idx < csc_matrix.num_cols; col_idx++) {
        csc_matrix.adj_indptr[col_idx + 1] = csc_matrix.adj_indptr[col_idx] + nnz_each_col[col_idx];
    }
    assert(csc_matrix.adj_indptr[csc_matrix.num_cols] == nnz);
    // Convert adj_data and adj_indices
    std::vector<uint32_t> nnz_consumed_each_col(csc_matrix.num_cols);
    std::fill(nnz_consumed_each_col.begin(), nnz_consumed_each_col.end(), 0);
    for (size_t row_idx = 0; row_idx < csr_matrix.num_rows; row_idx++){
        for (size_t i = csr_matrix.adj_indptr[row_idx]; i < csr_matrix.adj_indptr[row_idx + 1]; i++){
            uint32_t col_idx = csr_matrix.adj_indices[i];
            uint32_t dest = csc_matrix.adj_indptr[col_idx] + nnz_consumed_each_col[col_idx];
            csc_matrix.adj_indices[dest] = row_idx;
            csc_matrix.adj_data[dest] = csr_matrix.adj_data[i];
            nnz_consumed_each_col[col_idx]++;
        }
    }
    for (size_t col_idx = 0; col_idx < csc_matrix.num_cols; col_idx++) {
        assert(nnz_consumed_each_col[col_idx] == nnz_each_col[col_idx]);
    }
    return csc_matrix;
}


// Convert a float csc matrix to another data type.
template<typename data_type>
CSCMatrix<data_type> csc_matrix_convert_from_float(CSCMatrix<float> const &in) {
    CSCMatrix<data_type> out;
    out.num_rows = in.num_rows;
    out.num_cols = in.num_cols;
    std::copy(in.adj_data.begin(), in.adj_data.end(), std::back_inserter(out.adj_data));
    out.adj_indices = in.adj_indices;
    out.adj_indptr = in.adj_indptr;
    return out;
}

}  // namespace io
}  // namespace spmv

#endif  // GRAPHLILY_IO_DATA_LOADER_H_
