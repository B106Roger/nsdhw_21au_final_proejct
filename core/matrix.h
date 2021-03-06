#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <iostream>
#include <math.h>
#include <mkl.h>

#ifndef __MATRIX__
#define __MATRIX__
namespace py = pybind11;

class Block {
public:
    Block(size_t nrow, size_t ncol, bool colmajor);
    Block(const Block &block);
    ~Block();
    double   operator() (size_t row, size_t col) const;
    void setContent(double *ptr, size_t row_stride) ;

    size_t nrow() const {return m_nrow;}
    size_t ncol() const {return m_ncol;}

private:
    size_t m_nrow;
    size_t m_ncol;
    double *m_buffer;
    size_t m_row_stride;
    bool m_colmajor;

};

class Matrix {
public:
    Matrix();
    Matrix(size_t nrow, size_t ncol);
    static Matrix fillwith(size_t nrow, size_t ncol, double num);
    
    template<typename Type>
    Matrix(Type* ptr, size_t nrow, size_t ncol);
    Matrix(const Matrix &target);
    ~Matrix();
    
    ///////////////////////////////////////////
    // Math Operation Function Start
    ///////////////////////////////////////////
    double   operator() (size_t row, size_t col) const;
    double & operator() (size_t row, size_t col) ;

    Matrix  operator+(const Matrix &mat) const ;
    Matrix& operator+=(const Matrix &mat) ;
    Matrix  operator+(double num) const;
    Matrix& operator+=(double num);
    friend Matrix operator+(double num, const Matrix &mat);

    Matrix  operator-(const Matrix &mat) const ;
    Matrix& operator-=(const Matrix &mat) ;
    Matrix  operator-(double num) const;
    Matrix& operator-=(double num);
    friend Matrix operator-(double num, const Matrix &mat);

    Matrix  operator*(const Matrix &mat) const;
    Matrix& operator*=(const Matrix &mat);
    Matrix  operator*(double num) const;
    Matrix& operator*=(double num);
    friend Matrix operator*(double num, const Matrix &mat);

    Matrix  operator/(const Matrix &mat) const;
    Matrix& operator/=(const Matrix &mat) ;
    Matrix  operator/(double num) const;
    Matrix& operator/=(double num);
    friend Matrix operator/(double num, const Matrix &mat);
    
    bool operator==(const Matrix &target) const;
    void operator=(const Matrix &target) ;

    Matrix power(double p) const;
    Matrix exp() const;
    Matrix log() const;
    Matrix sigmoid() const;
    Matrix relu() const;
    ///////////////////////////////////////////
    // Math Operation Function End
    ///////////////////////////////////////////

    Block get_block(size_t block_size, size_t row_idx, size_t col_idx, bool col2row = false) const;
    void set_block(size_t block_size, size_t row_idx, size_t col_idx, const Matrix &mat) ;

    Matrix T() const;
    double *data() { return m_buffer; }
    size_t nrow() const { return m_nrow; }
    size_t ncol() const { return m_ncol; }
    double* get_buffer() const {return m_buffer;}
    py::array_t<double, py::array::c_style | py::array::forcecast> get_array();

    friend Matrix multiply_mkl(const Matrix &mat1, const Matrix &mat2);
    void print_shape(const char* mat_name="") const {std::cout << mat_name << " m_nrow: " << m_nrow << " m_ncol: " << m_ncol << std::endl; }
private:
    
    size_t m_nrow;
    size_t m_ncol;
    double * m_buffer;

};

void test(py::buffer b);
Matrix multiply_mkl(const Matrix &mat1, const Matrix &mat2);
Matrix multiply_tile(const Matrix &mat1, const Matrix &mat2, size_t block_size);
Matrix multiply_naive(const Matrix &mat1, const Matrix &mat2);
#endif
// end #define __MATRIX__