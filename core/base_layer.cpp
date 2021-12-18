#include"base_layer.h"


BaseLayer::BaseLayer(){}
BaseLayer::BaseLayer(bool trainable, bool transpose_input)
    : m_trainable(trainable), m_transpose_input(transpose_input){}
BaseLayer::~BaseLayer(){}

// input_tensor=(feat_dim, batch)
Matrix BaseLayer::operator()(Matrix &input_tensor)
{
    this->m_input = input_tensor;
    return this->forward(input_tensor);
}

Matrix BaseLayer::forward(Matrix &input_tenosr)
{
    return input_tenosr;
}

Matrix BaseLayer::backward(Matrix &gradient)
{
    return gradient;
}