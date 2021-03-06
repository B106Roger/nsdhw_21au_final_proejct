import numpy as np

class Layer:
    def __init__(self, trainable, has_var, transpose_input):
        self.trainable = trainable
        self.has_var = has_var
        self._transpose_input_ = transpose_input
        
    def __call__(self, input_tensor):
        # if self._transpose_input_:
        #     # turn (batch, neurals) to (neruals, batch)
        #     input_tensor = input_tensor.T
        # operation, 
        # input_tesnor=(batch, in_feat)
        # output_tesnor=(batch, out_feat)
        print('input_tensor bef forward', input_tensor.shape)
        output_tensor = self.forward(input_tensor)
        # store input_tensor and output_tensor so when calling backward, we can calaulate grad easily.
        self.record_grads(input_tensor)
        return output_tensor

    def record_grads(self, input_tensor):
        # store input_tensor=(batch, in_feat)
        self._input_tensor_ = input_tensor
