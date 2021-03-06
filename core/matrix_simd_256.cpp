#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <iostream>
#include <math.h>
#include <mkl.h>
#include "matrix.h"
#include "debug.h"
#include <immintrin.h>

// Matrix = double + Matrix
#define OPERATOR_DOUBLE_MATRIX(FUNCNAME,OPT) \
Matrix FUNCNAME(double num, const Matrix &mat) \
{\
    Matrix result(mat);\
    for(size_t i = 0; i < result.m_nrow*result.m_ncol; i++){\
        result.m_buffer[i] = num OPT result.m_buffer[i];\
    }\
    return result;\
}\

// Matrix = Matrix + double
#define OPERATOR_MATRIX_DOUBLE(FUNCNAME,OPT)\
Matrix Matrix::FUNCNAME(double num) const\
{\
    Matrix result(m_nrow,m_ncol);\
    for(size_t i = 0; i < m_nrow*m_ncol; i++){\
        result.m_buffer[i] = m_buffer[i] OPT num;\
    }\
    return result;\
}\

// Matrix = Matrix + double
#define SIMD_OPERATOR_MATRIX_DOUBLE(FUNCNAME,SIMD)\
Matrix Matrix::FUNCNAME(double num) const\
{\
    Matrix result(m_nrow,m_ncol);\
    const double scalars[4] = {num, num, num, num};\
    size_t i = 0 ;\
    for(i = 0; i < m_nrow*m_ncol; i+=4){\
        __m256d * ma = (__m256d *) (&m_buffer[i]);\
        __m256d * mb = (__m256d *) (&scalars[0]);\
        __m256d * mr = (__m256d *) (&result.m_buffer[i]);\
        *mr = SIMD (*ma, *mb);\
    }\
    return result;\
}\


// Matrix = Matrix + Matrix// _mm256_add_pd, _mm256_sub_pd, _mm256_mul_pd, _mm256_div_pd
#define SIMD_OPERATOR_MATRIX_MATRIX(FUNCNAME,OPT,SIMD)\
Matrix Matrix::FUNCNAME(const Matrix &mat) const \
{\
    size_t row=std::max(mat.m_nrow, m_nrow);\
    size_t col=std::max(mat.m_ncol, m_ncol);\
    if (DEBUG) {\
    if (mat.m_nrow != m_nrow && !(mat.m_nrow == 1 || m_nrow == 1)) \
        throw std::runtime_error("The shape is not broadcastable in row.");\
    else if (mat.m_ncol != m_ncol && !(mat.m_ncol == 1 || m_ncol == 1))\
        throw std::runtime_error("The shape is not broadcastable in column.");\
    }\
    Matrix result(row, col);\
    if (mat.m_nrow == m_nrow && mat.m_ncol == m_ncol) {\
        size_t i = 0u;\
        for(i = 0; i < m_nrow*m_ncol; i+=4){\
            __m256d * ma = (__m256d *) (&m_buffer[i]);\
            __m256d * mb = (__m256d *) (&mat.m_buffer[i]);\
            __m256d * mr = (__m256d *) (&result.m_buffer[i]);\
            *mr = SIMD (*ma, *mb);\
        }\
        return result;\
    } else {\
        for (size_t i = 0; i < row; i+=1) {\
            for (size_t j = 0; j < col; j+=1) {\
                result.m_buffer[i*col+j] = \
                    (*this)(i % m_nrow,     j % m_ncol) OPT \
                        mat(i % mat.m_nrow, j % mat.m_ncol);\
            }\
        }\
        return result;\
    }\
}\



// Matrix += double
#define OPERATOR_ASSIGN_MATRIX_DOUBLE(FUNCNAME,OPT)\
Matrix& Matrix::FUNCNAME(double num) \
{\
    for(size_t i = 0; i < m_nrow*m_ncol; i++){\
        m_buffer[i] OPT num;\
    }\
    return *this;\
}\

// Matrix += Matrix
#define OPERATOR_ASSIGN_MATRIX_MATRIX(FUNCNAME,OPT)\
Matrix& Matrix::FUNCNAME(const Matrix &mat) {\
    for (size_t i=0; i< m_nrow; i+=1) {\
        for (size_t j=0; j<m_ncol; j+=1) {\
            m_buffer[i*m_ncol+j] OPT mat(i,j);\
        }\
    }\
    return *this;\
}\

namespace py = pybind11;
////////////////////////////////////////////////////////////////////////////////
/////////////       Class Member for Block Class                     //////////
////////////////////////////////////////////////////////////////////////////////

Block::Block(size_t nrow, size_t ncol, bool colmajor):
    m_nrow(nrow), m_ncol(ncol), m_buffer(NULL), m_row_stride(0), m_colmajor(colmajor)
{
    if (m_colmajor)
    {
        // m_buffer=new double[m_nrow*m_ncol];
        m_buffer = (double*)aligned_alloc(32, m_nrow*m_ncol * sizeof(double));
    }
}
Block::Block(const Block &block):
    m_nrow(block.m_nrow), m_ncol(block.m_ncol), m_buffer(NULL), m_row_stride(0), m_colmajor(block.m_colmajor)
{
    if (block.m_colmajor)
    {
        // m_buffer=new double[m_nrow*m_ncol];
        m_buffer = (double*)aligned_alloc(32, m_nrow*m_ncol * sizeof(double));
        memcpy(m_buffer, block.m_buffer, sizeof(double) * m_nrow * m_ncol);
    }
}
Block::~Block() { 
    if (m_colmajor)
        free(m_buffer);
    m_buffer = NULL;
}
double   Block::operator() (size_t row, size_t col) const { // for getitem
    if (m_colmajor)
    {
        return m_buffer[col * m_nrow + row];
    }
    else
        return m_buffer[row * m_row_stride + col];
}
void Block::setContent(double *ptr, size_t row_stride) {
    m_row_stride = row_stride;
    if (m_colmajor) {
        for (size_t i = 0; i < m_nrow; i++) {
            for (size_t j = 0; j < m_ncol; j++) {
                m_buffer[j * m_nrow + i]= ptr[i * m_row_stride + j];
            }
        }
    } else {
        
        m_buffer = ptr;
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////       Class Member for Matrix Class                     //////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////
// Constructor Start
////////////////////////////////////////
Matrix::Matrix()
    : m_nrow(0), m_ncol(0), m_buffer(NULL)
{
}

Matrix::Matrix(size_t nrow, size_t ncol)
  : m_nrow(nrow), m_ncol(ncol), m_buffer(NULL)
{
    size_t nelement = nrow * ncol;
    // m_buffer = new double[nelement];
    m_buffer = (double*)aligned_alloc(32, nelement * sizeof(double));
    if (m_buffer!=NULL)
        memset(m_buffer, 0, nelement*sizeof(double));
    else 
        std::cout << "Your buffer is out of memory" << std::endl;
}

template<typename Type>
Matrix::Matrix(Type* ptr, size_t nrow, size_t ncol)
    :m_nrow(nrow), m_ncol(ncol), m_buffer(NULL)
{  
    
    size_t nelement = nrow * ncol;
    // m_buffer = new double[nelement];
    m_buffer = (double*)aligned_alloc(32, nelement * sizeof(double));
    for(size_t i =0; i < nelement; i++) 
    {
        m_buffer[i] = (double)ptr[i];
    }
}

Matrix::Matrix(const Matrix &target) {
    m_nrow=target.nrow();
    m_ncol=target.ncol();
    // m_buffer = new double[m_nrow*m_ncol];
    m_buffer = (double*)aligned_alloc(32, m_nrow*m_ncol * sizeof(double));

    memcpy(m_buffer, target.m_buffer, sizeof(double) * m_nrow * m_ncol);
}

Matrix Matrix::fillwith(size_t nrow, size_t ncol, double num)
{
    Matrix mat(nrow, ncol);
    std::fill(mat.m_buffer, mat.m_buffer+nrow*ncol, num);
    return mat;
}

Matrix::~Matrix() { 
    if (m_buffer == NULL && m_nrow != 0 and m_ncol != 0)
        std::cout << "pointer is null but has m_row, m_col is not 0\n" << std::endl;

    if (m_buffer != NULL)
    {
        free(m_buffer);
        m_nrow=m_ncol = 0;
    }
}

///////////////////////////////////////
// Other Function
///////////////////////////////////////
// No bound check.
double   Matrix::operator() (size_t row, size_t col) const { // for getitem
    if (row > m_nrow)
        throw std::runtime_error("row out of bound");
    if (col > m_ncol)
        throw std::runtime_error("col out of bound");
    return m_buffer[row*m_ncol + col];
}
double & Matrix::operator() (size_t row, size_t col) {       // for setitem
    if (row > m_nrow)
        throw std::runtime_error("row out of bound");
    if (col > m_ncol)
        throw std::runtime_error("col out of bound");
    return m_buffer[row*m_ncol + col];
}
// implement in vectorize mode

OPERATOR_DOUBLE_MATRIX(operator+,+)
OPERATOR_DOUBLE_MATRIX(operator-,-)
OPERATOR_DOUBLE_MATRIX(operator*,*)
OPERATOR_DOUBLE_MATRIX(operator/,/)

// OPERATOR_MATRIX_DOUBLE(operator+,+)
// OPERATOR_MATRIX_DOUBLE(operator-,-)
// OPERATOR_MATRIX_DOUBLE(operator*,*)
// OPERATOR_MATRIX_DOUBLE(operator/,/)

SIMD_OPERATOR_MATRIX_DOUBLE(operator+,_mm256_add_pd)
SIMD_OPERATOR_MATRIX_DOUBLE(operator-,_mm256_sub_pd)
SIMD_OPERATOR_MATRIX_DOUBLE(operator*,_mm256_mul_pd)
SIMD_OPERATOR_MATRIX_DOUBLE(operator/,_mm256_div_pd)

// OPERATOR_MATRIX_MATRIX(operator+,+)
// OPERATOR_MATRIX_MATRIX(operator-,-)
// OPERATOR_MATRIX_MATRIX(operator*,*)
// OPERATOR_MATRIX_MATRIX(operator/,/)

SIMD_OPERATOR_MATRIX_MATRIX(operator+,+,_mm256_add_pd)
SIMD_OPERATOR_MATRIX_MATRIX(operator-,-,_mm256_sub_pd)
SIMD_OPERATOR_MATRIX_MATRIX(operator*,*,_mm256_mul_pd)
SIMD_OPERATOR_MATRIX_MATRIX(operator/,/,_mm256_div_pd)

OPERATOR_ASSIGN_MATRIX_DOUBLE(operator+=,+=)
OPERATOR_ASSIGN_MATRIX_DOUBLE(operator-=,-=)
OPERATOR_ASSIGN_MATRIX_DOUBLE(operator*=,*=)
OPERATOR_ASSIGN_MATRIX_DOUBLE(operator/=,/=)

OPERATOR_ASSIGN_MATRIX_MATRIX(operator+=,+=)
OPERATOR_ASSIGN_MATRIX_MATRIX(operator-=,-=)
OPERATOR_ASSIGN_MATRIX_MATRIX(operator*=,*=)
OPERATOR_ASSIGN_MATRIX_MATRIX(operator/=,/=)

void Matrix::operator=(const Matrix &target) {
    if (m_buffer != NULL)
        free(m_buffer);
    m_nrow=target.nrow();
    m_ncol=target.ncol();
    // m_buffer = new double[m_nrow*m_ncol];
    m_buffer = (double*)aligned_alloc(32, m_nrow*m_ncol * sizeof(double));
    memcpy(m_buffer, target.m_buffer, sizeof(double) * m_nrow * m_ncol);
}
bool Matrix::operator==(const Matrix &target) const{
    if (m_nrow != target.m_nrow || m_ncol != target.m_ncol) {
        return false;
    } else {
        for (size_t i = 0; i < m_nrow; i++) {
            for (size_t j = 0; j < m_ncol; j++) {
                if ((*this)(i,j) != target(i,j)) return false;
            }
        }
        return true;
    }
}

Matrix Matrix::power(double p) const
{
    size_t nelement=m_ncol*m_nrow;
    Matrix result(m_nrow, m_ncol);
    for(size_t i = 0; i < nelement; i++){
        result.m_buffer[i] = std::pow(m_buffer[i], p);
    }
    return result;
}
Matrix Matrix::exp() const
{
    size_t nelement=m_ncol*m_nrow;
    Matrix result(m_nrow, m_ncol);
    for(size_t i = 0; i < nelement; i++){
        result.m_buffer[i] = std::exp(m_buffer[i]);
    }
    return result;
}
Matrix Matrix::log() const
{
    size_t nelement=m_ncol*m_nrow;
    Matrix result(m_nrow, m_ncol);
    for(size_t i = 0; i < nelement; i++){
        result.m_buffer[i] = std::log(m_buffer[i]);
    }
    return result;
}
Matrix Matrix::sigmoid() const {
    Matrix result(*this);
    for (size_t i = 0; i < m_nrow*m_ncol; i++) {
        result.m_buffer[i] = 1.0 / (1.0 + std::exp(-result.m_buffer[i]));
    }
    return result;
}
Matrix Matrix::relu() const {
    Matrix result(*this);
    for(size_t i = 0; i < result.m_nrow * result.m_ncol; i++) {
        if (result.m_buffer[i] < 0)
            result.m_buffer[i] = 0.0;
    }
    return result;
}

Matrix Matrix::T() const
{
    Matrix result(m_ncol, m_nrow);
    for(size_t i = 0; i<m_nrow*m_ncol; i++)
    {
        size_t col_idx = i / m_ncol;
        size_t row_idx = i % m_ncol;
        result(row_idx, col_idx) = m_buffer[i];
    }
    // std::cout << "right before T return: " << result.get_buffer() << std::endl;
    return result;
}

Block Matrix::get_block(size_t block_size, size_t row_idx, size_t col_idx, bool col2row) const{
    // row_idx: row index of the block
    // col_idx: col index of the block
    size_t bk_col = m_ncol - block_size*col_idx < block_size ? m_ncol - block_size*col_idx : block_size;
    size_t bk_row = m_nrow - block_size*row_idx < block_size ? m_nrow - block_size*row_idx : block_size;
    Block block(bk_row, bk_col, col2row);

    size_t target_row=(block_size*row_idx)*m_ncol;
    size_t target_col=(block_size*col_idx);
    block.setContent(m_buffer+target_row+target_col, m_ncol);
    return block;
}

void Matrix::set_block(size_t block_size, size_t row_idx, size_t col_idx, const Matrix &mat) {
    // row_idx: row index of the block
    // col_idx: col index of the block
    size_t bk_col = m_ncol - block_size*col_idx < block_size ? m_ncol - block_size*col_idx : block_size;
    size_t bk_row = m_nrow - block_size*row_idx < block_size ? m_nrow - block_size*row_idx : block_size;
    for (size_t i=0;i<bk_row; i++) {
        size_t target_row=(block_size*row_idx+i)*m_ncol;
        size_t target_col=(block_size*col_idx);
        size_t source_row=i*bk_col;
        memcpy(m_buffer+target_row+target_col, mat.m_buffer+source_row, sizeof(double) * mat.m_ncol);
    }
}

py::array_t<double, py::array::c_style | py::array::forcecast> Matrix::get_array()
{
    py::buffer_info  buffer(
        m_buffer,
        sizeof(double),
        py::format_descriptor<double>::format(),
        2,
        {m_nrow, m_ncol},
        {sizeof(double) * m_ncol, sizeof(double)}
    );
    return py::array_t<double, py::array::c_style | py::array::forcecast>(buffer, py::cast(this));
}

Matrix multiply_naive_bk(const Block &mat1, const Block &mat2) {
    size_t row=mat1.nrow();
    size_t col=mat2.ncol();
    size_t content=mat1.ncol();
    Matrix tmp(row, col);
    for (size_t i=0; i<row; i++) {
        for (size_t j=0; j<col; j++) {
            double sum=0.0;
            for (size_t k=0; k<content; k++) {
                sum+=mat1(i,k)*mat2(k,j);
            }
            tmp(i,j)=sum;
        }
    }
    return tmp;
}

Matrix multiply_naive(const Matrix &mat1, const Matrix &mat2) {
    size_t row=mat1.nrow();
    size_t col=mat2.ncol();
    size_t content=mat1.ncol();
    Matrix tmp(row, col);
    for (size_t i=0; i<row; i++) {
        for (size_t j=0; j<col; j++) {
            double sum=0.0;
            for (size_t k=0; k<content; k++) {
                sum+=mat1(i,k)*mat2(k,j);
            }
            tmp(i,j)=sum;
        }
    }
    return tmp;
}

Matrix multiply_tile(const Matrix &mat1, const Matrix &mat2, size_t block_size) {
    size_t row=mat1.nrow();
    size_t col=mat2.ncol();
    size_t content=mat1.ncol();
    Matrix result(row, col);
    size_t max_bk_row = row % block_size == 0 ? row/block_size : row/block_size+1;
    size_t max_bk_col = col % block_size == 0 ? col/block_size : col/block_size+1;
    size_t max_bk_content = content % block_size == 0 ? content/block_size : content/block_size+1;

    for (size_t i=0; i<max_bk_row; i++) {
        for (size_t j=0; j<max_bk_col; j++) {
            Matrix tmpmat(1,1);
            for (size_t k=0; k<max_bk_content; k++) {
                if (k==0) 
                    tmpmat = multiply_naive_bk(mat1.get_block(block_size, i, k, false), mat2.get_block(block_size, k, j, true));
                else
                    tmpmat +=  multiply_naive_bk(mat1.get_block(block_size, i, k, false), mat2.get_block(block_size, k, j, true));
            }
            result.set_block(block_size, i, j, tmpmat);
        }
    }
    return result;
}

Matrix multiply_mkl(const Matrix &mat1, const Matrix &mat2) {
    if (mat1.ncol() != mat2.nrow())
        throw std::runtime_error("mismatch mat1.ncol and mat2.nrow!");
    mkl_set_num_threads(1);
    Matrix ret(mat1.nrow(), mat2.ncol());
    cblas_dgemm(
        CblasRowMajor /* const CBLAS_LAYOUT Layout */
      , CblasNoTrans /* const CBLAS_TRANSPOSE transa */
      , CblasNoTrans /* const CBLAS_TRANSPOSE transb */
      , mat1.nrow() /* const MKL_INT m */
      , mat2.ncol() /* const MKL_INT n */
      , mat1.ncol() /* const MKL_INT k */
      , 1.0 /* const double alpha */
      , mat1.m_buffer /* const double *a */
      , mat1.ncol() /* const MKL_INT lda */
      , mat2.m_buffer /* const double *b */
      , mat2.ncol() /* const MKL_INT ldb */
      , 0.0 /* const double beta */
      , ret.m_buffer /* double * c */
      , ret.ncol() /* const MKL_INT ldc */
    );

    return ret;
}

void test(py::buffer b) {
    py::buffer_info info = b.request();
    std::cout << info.format << std::endl;
}
