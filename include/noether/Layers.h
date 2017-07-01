#ifndef NOETHER_LAYERS_H
#define NOETHER_LAYERS_H

#include "noether/Tensor.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// Represents a node in the network compute graph.
template <class ElemTy> class Layer {

  /// \returns a descriptive name for the operation.
  virtual std::string getName() const = 0;

  /// \returns the output of a node in the compute graph.
  virtual Array3D<ElemTy> &getOutput() = 0;

  /// \returns the dimension of the tensor.
  std::tuple<size_t, size_t, size_t> dims() const { return getOutput().dims(); }

  /// \returns the number of elements in the tensor.
  size_t size() const { return getOutput().size(); }

  /// Does the forward propagation.
  virtual void forward() = 0;

  /// Does the backwards propagation.
  virtual void backward() = 0;
};


template <class ElemTy> class ConvLayer final : public Layer<ElemTy> {
  Layer<ElemTy> *input_;
  /// A list of convolution filters.
  std::vector<Array3D<ElemTy>> filters_;
  /// The convolution bias.
  Array3D<ElemTy> bias_;
  /// The convolution output.
  Array3D<ElemTy> output_;

  size_t filterSize_;
  size_t stride_;
  size_t pad_;

  ConvLayer(Layer<ElemTy> *input, size_t outDepth, size_t filterSize,
            size_t stride, size_t pad)
      : input_(input), filterSize_(filterSize), stride_(stride), pad_(pad) {
    assert(pad == 0 && "Unsupported pad size");
    assert(input && "Invalid input layer");
    size_t inx, iny, inz;
    std::tie(inx, iny, inz) = input_->dims();

    size_t outsx = ((inx + pad_ * 2 - filterSize) / stride + 1);
    size_t outsy = ((iny + pad_ * 2 - filterSize) / stride + 1);

    output_.reset(outsx, outsy, outDepth);
    bias_.reset(1, 1, outDepth);

    for (size_t i = 0; i < outDepth; i++) {
      filters_.emplace(filterSize, filterSize, inz);
    }
  }

  void forward() {
    size_t outx, outy, outz;
    std::tie(outx, outy, outz) = output_->dims();
    size_t inx, iny, inz;
    std::tie(inx, iny, inz) = input_->dims();

    auto &inputBuffer = input_->getOutput();

    // For each layer in the output tensor:
    for (size_t d = 0; d < outz; d++) {
      auto &currFilter = filters_[d];

      // For each convolution 'jump' in the input tensor:
      size_t y = 0;
      for (size_t ay = 0; ay < outy; y += stride_, ay++) {
        size_t x = 0;
        for (size_t ax = 0; ax < outy; x += stride_, ax++) {

          // For each element in the convolution-filter:
          ElemTy sum = 0;
          for (size_t fy = 0; fy < filterSize_; fy++) {
            for (size_t fx = 0; fx < filterSize_; fx++) {
              auto ox = x + fx;
              auto oy = y + fy;

              if (output_.isInBounds(ox, oy)) {
                for (size_t fd = 0; fd < inz; fd++) {
                  sum +=
                      currFilter.get(fx, fy, fd) * inputBuffer.get(ox, oy, fd);
                }
              }
            }
          }

          sum += bias_.get(d);
          output_.get(ax, ay, d) = sum;
        }
      }
    }
  }
};

template <class ElemTy> class FullyConnectedLayer final : public Layer<ElemTy> {
  /// A reference to the layer input.
  Layer<ElemTy> *input_;
  /// A list of filters.
  std::vector<Array3D<ElemTy>> filters_;
  /// The biases.
  Array3D<ElemTy> bias_;
  /// The output.
  Array3D<ElemTy> output_;

  FullyConnectedLayer(Layer<ElemTy> *input, size_t outDepth)
      : input_(input) {
    assert(input && "Invalid input layer");
    size_t inx, iny, inz;
    std::tie(inx, iny, inz) = input_->dims();

    output_.reset(1, 1, outDepth);
    bias_.reset(1, 1, outDepth);

    size_t numInputs = input_->size();
    for (size_t i = 0; i < outDepth; i++) {
      filters_.emplace(1, 1, numInputs);
    }
  }

  void forward() {
    size_t inx, iny, inz;
    std::tie(inx, iny, inz) = input_->dims();
    size_t outx, outy, outz;
    std::tie(outx, outy, outz) = output_->dims();
    auto &inputBuffer = input_->getOutput();
    size_t numInputs = input_->size();

    for (size_t i = 0; i < outz; i++) {
      auto &currFilter = filters_[i];
      ElemTy sum = 0;

      size_t idx = 0;
      for (size_t x = 0; x < inx; x++) {
        for (size_t y = 0; y < iny; y++) {
          for (size_t z = 0; z < inz; z++) {
            sum += inputBuffer.get(x,y,z) * currFilter[idx++];
          }
        }
      }
      assert(idx == numInputs && "Invalid index");
      sum += bias_[i];
      output_.get(1,1,i) = sum;
    }
  }
};

#endif // NOETHER_LAYERS_H
