/*
 *  This file is a part of TiledArray.
 *  Copyright (C) 2018  Virginia Tech
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Chong Peng
 *  Department of Chemistry, Virginia Tech
 *  July 24, 2018
 *
 */

#ifndef TILEDARRAY_BTAS_TENSOR_CUDA_CUBLAS_H__INCLUDED
#define TILEDARRAY_BTAS_TENSOR_CUDA_CUBLAS_H__INCLUDED

#include <TiledArray/math/cublas.h>

#ifdef TILEDARRAY_HAS_CUDA

#include <TiledArray/external/btas.h>
#include <TiledArray/external/cuda.h>
//#include <btas/tensor.h>

#include <TiledArray/math/gemm_helper.h>
#include <TiledArray/tensor/cuda/platform.h>
#include <TiledArray/tensor/cuda/um_storage.h>

namespace TiledArray {

namespace detail {

template <typename Range>
const cudaStream_t &get_stream_based_on_range(const Range &range) {
  // TODO better way to get stream based on the id of tensor
  auto stream_id = range.offset() % cudaEnv::instance()->num_cuda_streams();
  auto &stream = cudaEnv::instance()->cuda_stream(stream_id);
  return stream;
}

}  // namespace detail

template <typename T, typename Range, typename Storage>
btas::Tensor<T, Range, Storage> btas_tensor_gemm_cuda_impl(
    const btas::Tensor<T, Range, Storage> &left,
    const btas::Tensor<T, Range, Storage> &right, T factor,
    const TiledArray::math::GemmHelper &gemm_helper) {
  // either both arguments are on host or both on device ... mixed case TBI
  //  TA_ASSERT(left.storage().on_host() == right.storage().on_host() &&
  //            left.storage().on_device() == right.storage().on_device());

  TA_ASSERT((in_memory_space<MemorySpace::CPU>(left.storage()) &&
             in_memory_space<MemorySpace::CPU>(right.storage())) ||
            (in_memory_space<MemorySpace::CUDA>(left.storage()) &&
             in_memory_space<MemorySpace::CUDA>(right.storage())));

  // Check that the arguments are not empty and have the correct ranks
  TA_ASSERT(!left.empty());
  TA_ASSERT(left.range().rank() == gemm_helper.left_rank());
  TA_ASSERT(!right.empty());
  TA_ASSERT(right.range().rank() == gemm_helper.right_rank());

  cudaSetDevice(cudaEnv::instance()->current_cuda_device_id());
  // Construct the result Tensor
  typedef btas::Tensor<T, Range, Storage> Tensor;
  //  typedef typename Tensor::storage_type storage_type;
  auto result_range =
      gemm_helper.make_result_range<Range>(left.range(), right.range());

  auto &cuda_stream = detail::get_stream_based_on_range(result_range);

  Storage result_storage;
  make_device_storage(result_storage, result_range.area(), cuda_stream);
  Tensor result(std::move(result_range), std::move(result_storage));

  // auto stream_left = left.range().ordinal().offset() % num_cuda_streams;
  // auto stream_right = right.range().ordinal().offset() % num_cuda_streams;

  TiledArray::to_execution_space<TiledArray::ExecutionSpace::CUDA>(
      left.storage(), cuda_stream);
  TiledArray::to_execution_space<TiledArray::ExecutionSpace::CUDA>(
      right.storage(), cuda_stream);

  // Check that the inner dimensions of left and right match
  TA_ASSERT(
      gemm_helper.left_right_congruent(std::cbegin(left.range().lobound()),
                                       std::cbegin(right.range().lobound())));
  TA_ASSERT(
      gemm_helper.left_right_congruent(std::cbegin(left.range().upbound()),
                                       std::cbegin(right.range().upbound())));
  TA_ASSERT(gemm_helper.left_right_congruent(
      std::cbegin(left.range().extent()), std::cbegin(right.range().extent())));

  // Compute gemm dimensions
  integer m = 1, n = 1, k = 1;
  gemm_helper.compute_matrix_sizes(m, n, k, left.range(), right.range());

  // Get the leading dimension for left and right matrices.
  const integer lda =
      (gemm_helper.left_op() == madness::cblas::NoTrans ? k : m);
  const integer ldb =
      (gemm_helper.right_op() == madness::cblas::NoTrans ? n : k);

  if (in_memory_space<MemorySpace::CUDA>(result.storage())) {
    const auto &handle = cuBLASHandlePool::handle();
    auto zero = T(0);
    auto status = cublasSetStream(handle, cuda_stream);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
    status = cublasGemm(handle, to_cublas_op(gemm_helper.right_op()),
                        to_cublas_op(gemm_helper.left_op()), n, m, k, &factor,
                        device_data(right.storage()), ldb,
                        device_data(left.storage()), lda, &zero,
                        device_data(result.storage()), n);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
  } else {
    TA_ASSERT(false);
    //    TiledArray::math::gemm(gemm_helper.left_op(), gemm_helper.right_op(),
    //    m, n,
    //                           k, factor, left.data(), lda, right.data(), ldb,
    //                           T(0), result.data(), n);
  }

  // TiledArray::to_execution_space<TiledArray::ExecutionSpace::CPU>(left.storage(),
  // cuda_streams[stream_left]);
  // TiledArray::to_execution_space<TiledArray::ExecutionSpace::CPU>(right.storage(),
  // cuda_streams[stream_right]);

  return result;
}

template <typename T, typename Range, typename Storage>
void btas_tensor_gemm_cuda_impl(
    btas::Tensor<T, Range, Storage> &result,
    const btas::Tensor<T, Range, Storage> &left,
    const btas::Tensor<T, Range, Storage> &right, T factor,
    const TiledArray::math::GemmHelper &gemm_helper) {
  // the result determines were to do gemm
  if (in_memory_space<MemorySpace::CUDA>(result.storage())) {
    TA_ASSERT(in_memory_space<MemorySpace::CUDA>(left.storage()) &&
              in_memory_space<MemorySpace::CUDA>(right.storage()));
  } else {
    TA_ASSERT(in_memory_space<MemorySpace::CPU>(result.storage()) &&
              in_memory_space<MemorySpace::CPU>(left.storage()) &&
              in_memory_space<MemorySpace::CPU>(right.storage()));
  }

  // Check that the result is not empty and has the correct rank
  TA_ASSERT(!result.empty());
  TA_ASSERT(result.range().rank() == gemm_helper.result_rank());

  // Check that the arguments are not empty and have the correct ranks
  TA_ASSERT(!left.empty());
  TA_ASSERT(left.range().rank() == gemm_helper.left_rank());
  TA_ASSERT(!right.empty());
  TA_ASSERT(right.range().rank() == gemm_helper.right_rank());

  // Check that the outer dimensions of left match the the corresponding
  // dimensions in result
  TA_ASSERT(
      gemm_helper.left_result_congruent(std::cbegin(left.range().lobound()),
                                        std::cbegin(result.range().lobound())));
  TA_ASSERT(
      gemm_helper.left_result_congruent(std::cbegin(left.range().upbound()),
                                        std::cbegin(result.range().upbound())));
  TA_ASSERT(
      gemm_helper.left_result_congruent(std::cbegin(left.range().extent()),
                                        std::cbegin(result.range().extent())));

  // Check that the outer dimensions of right match the the corresponding
  // dimensions in result
  TA_ASSERT(gemm_helper.right_result_congruent(
      std::cbegin(right.range().lobound()),
      std::cbegin(result.range().lobound())));
  TA_ASSERT(gemm_helper.right_result_congruent(
      std::cbegin(right.range().upbound()),
      std::cbegin(result.range().upbound())));
  TA_ASSERT(
      gemm_helper.right_result_congruent(std::cbegin(right.range().extent()),
                                         std::cbegin(result.range().extent())));

  // Check that the inner dimensions of left and right match
  TA_ASSERT(
      gemm_helper.left_right_congruent(std::cbegin(left.range().lobound()),
                                       std::cbegin(right.range().lobound())));
  TA_ASSERT(
      gemm_helper.left_right_congruent(std::cbegin(left.range().upbound()),
                                       std::cbegin(right.range().upbound())));
  TA_ASSERT(gemm_helper.left_right_congruent(
      std::cbegin(left.range().extent()), std::cbegin(right.range().extent())));

  // auto stream_left = left.range().ordinal().offset() % num_cuda_streams;
  // auto stream_right = right.range().ordinal().offset() % num_cuda_streams;

  cudaSetDevice(cudaEnv::instance()->current_cuda_device_id());

  auto &stream = detail::get_stream_based_on_range(result.range());

  TiledArray::to_execution_space<TiledArray::ExecutionSpace::CUDA>(
      left.storage(), stream);
  TiledArray::to_execution_space<TiledArray::ExecutionSpace::CUDA>(
      right.storage(), stream);

  // Compute gemm dimensions
  integer m, n, k;
  gemm_helper.compute_matrix_sizes(m, n, k, left.range(), right.range());

  // Get the leading dimension for left and right matrices.
  const integer lda =
      (gemm_helper.left_op() == madness::cblas::NoTrans ? k : m);
  const integer ldb =
      (gemm_helper.right_op() == madness::cblas::NoTrans ? n : k);

  if (in_memory_space<MemorySpace::CUDA>(result.storage())) {
    const auto &handle = cuBLASHandlePool::handle();
    auto one = T(1);
    auto status = cublasSetStream(handle, stream);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
    status = cublasGemm(handle, to_cublas_op(gemm_helper.right_op()),
                        to_cublas_op(gemm_helper.left_op()), n, m, k, &factor,
                        device_data(right.storage()), ldb,
                        device_data(left.storage()), lda, &one,
                        device_data(result.storage()), n);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
  } else {
    TA_ASSERT(false);
    //    TiledArray::math::gemm(gemm_helper.left_op(), gemm_helper.right_op(),
    //    m, n,
    //                           k, factor, left.data(), lda, right.data(), ldb,
    //                           T(1), result.data(), n);
  }
}

/// result[i] = arg[i]
template <typename T, typename Range, typename Storage>
btas::Tensor<T, Range, Storage> btas_tensor_clone_cuda_impl(
    const btas::Tensor<T, Range, Storage> &arg) {
  auto &cuda_stream = detail::get_stream_based_on_range(arg.range());

  Storage result_storage;
  make_device_storage(result_storage, arg.size(), cuda_stream);
  btas::Tensor<T, Range, Storage> result(arg.range(),
                                         std::move(result_storage));

  // call cublasCopy
  const auto &handle = cuBLASHandlePool::handle();
  auto status = cublasSetStream(handle, cuda_stream);
  TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);

  status = cublasCopy(handle, result.size(), device_data(arg.storage()), 1,
                      device_data(result.storage()), 1);

  TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);

  return result;
}

/// result[i] = a * arg[i]
template <typename T, typename Range, typename Storage, typename Scalar>
btas::Tensor<T, Range, Storage> btas_tensor_scale_cuda_impl(
    const btas::Tensor<T, Range, Storage> &arg, const Scalar a) {
  auto &cuda_stream = detail::get_stream_based_on_range(arg.range());

  auto result = btas_tensor_clone_cuda_impl(arg);

  // call cublasScale
  const auto &handle = cuBLASHandlePool::handle();
  auto status = cublasSetStream(handle, cuda_stream);
  status =
      cublasScal(handle, result.size(), &a, device_data(result.storage()), 1);

  TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);

  return result;
}

/// result[i] *= a
template <typename T, typename Range, typename Storage, typename Scalar>
void btas_tensor_scale_to_cuda_impl(
    btas::Tensor<T, Range, Storage> &result, const Scalar a) {
  auto &cuda_stream = detail::get_stream_based_on_range(result.range());
  // call cublasScale
  const auto &handle = cuBLASHandlePool::handle();
  auto status = cublasSetStream(handle, cuda_stream);
  status =
      cublasScal(handle, result.size(), &a, device_data(result.storage()), 1);

  TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
}


/// result[i] = a * arg1[i] - arg2[i]
template <typename T, typename Range, typename Storage>
btas::Tensor<T, Range, Storage> btas_tensor_subt_cuda_impl(
    const btas::Tensor<T, Range, Storage> &arg1,
    const btas::Tensor<T, Range, Storage> &arg2, const T a) {
  auto result = btas_tensor_scale_cuda_impl(arg1, T(-1.0));

  cudaSetDevice(cudaEnv::instance()->current_cuda_device_id());
  auto &stream = detail::get_stream_based_on_range(result.range());

  if (in_memory_space<MemorySpace::CUDA>(result.storage())) {
    const auto &handle = cuBLASHandlePool::handle();
    auto status = cublasSetStream(handle, stream);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
    status = cublasAxpy(handle, result.size(), &a, device_data(arg1.storage()),
                        1, device_data(result.storage()), 1);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
  } else {
    TA_ASSERT(false);
    //    btas::axpy(1.0, arg, result);
  }

  return result;
}

/// result[i] -= a * arg1[i]
template <typename T, typename Range, typename Storage>
void btas_tensor_subt_to_cuda_impl(btas::Tensor<T, Range, Storage> &result,
                                   const btas::Tensor<T, Range, Storage> &arg1,
                                   const T a) {
  // revert the sign of arg1
  auto arg = btas_tensor_scale_cuda_impl(arg1, T(-1.0));

  cudaSetDevice(cudaEnv::instance()->current_cuda_device_id());
  auto &stream = detail::get_stream_based_on_range(result.range());

  const auto &handle = cuBLASHandlePool::handle();
  auto status = cublasSetStream(handle, stream);
  TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
  status = cublasAxpy(handle, result.size(), &a, device_data(arg.storage()), 1,
                      device_data(result.storage()), 1);
  TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
}

/// result[i] = a * arg1[i] + arg2[i]
template <typename T, typename Range, typename Storage>
btas::Tensor<T, Range, Storage> btas_tensor_add_cuda_impl(
    const btas::Tensor<T, Range, Storage> &arg1,
    const btas::Tensor<T, Range, Storage> &arg2, const T a) {
  
  auto result = btas_tensor_scale_cuda_impl(arg2, T(1.0));

  cudaSetDevice(cudaEnv::instance()->current_cuda_device_id());
  auto &stream = detail::get_stream_based_on_range(result.range());

  if (in_memory_space<MemorySpace::CUDA>(result.storage())) {
    const auto &handle = cuBLASHandlePool::handle();
    auto status = cublasSetStream(handle, stream);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
    status = cublasAxpy(handle, result.size(), &a, device_data(arg1.storage()),
                        1, device_data(result.storage()), 1);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
  } else {
    TA_ASSERT(false);
    //    btas::axpy(1.0, arg, result);
  }

  return result;
}

/// result[i] += a * arg[i]
template <typename T, typename Range, typename Storage>
void btas_tensor_add_to_cuda_impl(btas::Tensor<T, Range, Storage> &result,
                                  const btas::Tensor<T, Range, Storage> &arg,
                                  const T a) {
  // the result determines were to do gemm
  if (in_memory_space<MemorySpace::CUDA>(result.storage())) {
    TA_ASSERT(in_memory_space<MemorySpace::CUDA>(arg.storage()));
  } else {
    TA_ASSERT(in_memory_space<MemorySpace::CPU>(result.storage()) &&
              in_memory_space<MemorySpace::CPU>(arg.storage()));
  }
  cudaSetDevice(cudaEnv::instance()->current_cuda_device_id());
  auto &stream = detail::get_stream_based_on_range(result.range());

  // TiledArray::to_execution_space<TiledArray::ExecutionSpace::CUDA>(result.storage(),
  // cuda_streams[stream]);
  // TiledArray::to_execution_space<TiledArray::ExecutionSpace::CUDA>(arg.storage(),
  // cuda_streams[stream]);

  if (in_memory_space<MemorySpace::CUDA>(result.storage())) {
    const auto &handle = cuBLASHandlePool::handle();
    auto status = cublasSetStream(handle, stream);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
    status = cublasAxpy(handle, result.size(), &a, device_data(arg.storage()),
                        1, device_data(result.storage()), 1);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
  } else {
    TA_ASSERT(false);
    //    btas::axpy(1.0, arg, result);
  }
}

// foreach(i) result += arg[i] * arg[i]
template <typename T, typename Range, typename Storage>
typename btas::Tensor<T, Range, Storage>::value_type
btas_tensor_squared_norm_cuda_impl(const btas::Tensor<T, Range, Storage> &arg) {
  cudaSetDevice(cudaEnv::instance()->current_cuda_device_id());

  auto &stream = detail::get_stream_based_on_range(arg.range());

  auto &storage = arg.storage();
  integer size = storage.size();
  T result = 0;
  if (in_memory_space<MemorySpace::CUDA>(storage)) {
    const auto &handle = cuBLASHandlePool::handle();
    auto status = cublasSetStream(handle, stream);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
    status = cublasDot(handle, size, device_data(storage), 1,
                       device_data(storage), 1, &result);
    TA_ASSERT(status == CUBLAS_STATUS_SUCCESS);
  } else {
    TA_ASSERT(false);
    //    result = TiledArray::math::dot(size, storage.data(), storage.data());
  }
  return result;
}

}  // namespace TiledArray

#endif  // TILEDARRAY_HAS_CUDA

#endif  // TILEDARRAY_BTAS_TENSOR_CUDA_CUBLAS_H__INCLUDED