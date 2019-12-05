// ---------------------------------------------------------------------
// Copyright (c) 2016-2018, Lawrence Livermore National Security, LLC. All
// rights reserved.
//
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of CHAI.
//
// LLNL-CODE-705877
//
// For details, see https:://github.com/LLNL/CHAI
// Please also see the NOTICE and LICENSE files.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the
//   distribution.
//
// - Neither the name of the LLNS/LLNL nor the names of its contributors
//   may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// ---------------------------------------------------------------------
#include <climits>

#include "benchmark/benchmark.h"

#include "chai/config.hpp"
#include "chai/managed_ptr.hpp"

#include "../src/util/forall.hpp"

class Base {
   public:
      CHAI_HOST_DEVICE virtual void scale(size_t numValues, int* values) = 0;
};

class Derived : public Base {
   public:
      CHAI_HOST_DEVICE Derived(int value) : Base(), m_value(value) {}

      CHAI_HOST_DEVICE virtual void scale(size_t numValues, int* values) override {
         for (size_t i = 0; i < numValues; ++i) {
            values[i] *= m_value;
         }
      }

   private:
      int m_value = -1;
};

template <typename T>
class BaseCRTP {
   public:
      CHAI_HOST_DEVICE void scale(size_t numValues, int* values) {
         return static_cast<T*>(this)->scale(numValues, values);
      }
};

class DerivedCRTP : public BaseCRTP<DerivedCRTP> {
   public:
      CHAI_HOST_DEVICE DerivedCRTP(int value) : BaseCRTP<DerivedCRTP>(), m_value(value) {}

      CHAI_HOST_DEVICE void scale(size_t numValues, int* values) {
         for (size_t i = 0; i < numValues; ++i) {
            values[i] *= m_value;
         }
      }

   private:
      int m_value = -1;
};

class NoInheritance {
   public:
      CHAI_HOST_DEVICE NoInheritance(int value) : m_value(value) {}

      CHAI_HOST_DEVICE void scale(size_t numValues, int* values) {
         for (size_t i = 0; i < numValues; ++i) {
            values[i] *= m_value;
         }
      }

   private:
      int m_value = -1;
};

template <size_t N>
class ClassWithSize {
   private:
      char m_values[N];
};

static void benchmark_managed_ptr_construction_and_destruction(benchmark::State& state)
{
  while (state.KeepRunning()) {
    chai::managed_ptr<Base> temp = chai::make_managed<Derived>(1);
    temp.free();
  }
}

BENCHMARK(benchmark_managed_ptr_construction_and_destruction);

// managed_ptr
static void benchmark_use_managed_ptr_cpu(benchmark::State& state)
{
  chai::managed_ptr<Base> object = chai::make_managed<Derived>(2);

  size_t numValues = 100;
  int* values = (int*) malloc(100 * sizeof(int));

  for (size_t i = 0; i < numValues; ++i) {
     values[i] = i * i;
  }

#ifdef __CUDACC__
  cudaDeviceSynchronize();
#endif

  while (state.KeepRunning()) {
    object->scale(numValues, values);
  }

  object.free();
  cudaDeviceSynchronize();
}

BENCHMARK(benchmark_use_managed_ptr_cpu);

// Curiously recurring template pattern
static void benchmark_curiously_recurring_template_pattern_cpu(benchmark::State& state)
{
  BaseCRTP<DerivedCRTP>* object = new DerivedCRTP(2);

  size_t numValues = 100;
  int* values = (int*) malloc(100 * sizeof(int));

  for (size_t i = 0; i < numValues; ++i) {
     values[i] = i * i;
  }

  while (state.KeepRunning()) {
    object->scale(numValues, values);
  }

  free(values);
  delete object;
}

BENCHMARK(benchmark_curiously_recurring_template_pattern_cpu);

// Class without inheritance
static void benchmark_no_inheritance_cpu(benchmark::State& state)
{
  NoInheritance* object = new NoInheritance(2);

  size_t numValues = 100;
  int* values = (int*) malloc(100 * sizeof(int));

  for (size_t i = 0; i < numValues; ++i) {
     values[i] = i * i;
  }

  while (state.KeepRunning()) {
    object->scale(numValues, values);
  }

  free(values);
  delete object;
}

BENCHMARK(benchmark_no_inheritance_cpu);

#if defined(CHAI_ENABLE_CUDA) || defined(CHAI_ENABLE_HIP)

template <size_t N>
__global__ void copy_kernel(ClassWithSize<N>) {}

// Benchmark how long it takes to copy a class to the GPU
template <size_t N>
static void benchmark_pass_copy_to_gpu(benchmark::State& state)
{
  ClassWithSize<N> helper;

  while (state.KeepRunning()) {
    copy_kernel<<<1, 1>>>(helper);
    cudaDeviceSynchronize();
  }
}

BENCHMARK_TEMPLATE(benchmark_pass_copy_to_gpu, 8);
BENCHMARK_TEMPLATE(benchmark_pass_copy_to_gpu, 64);
BENCHMARK_TEMPLATE(benchmark_pass_copy_to_gpu, 512);
BENCHMARK_TEMPLATE(benchmark_pass_copy_to_gpu, 4096);

template <size_t N>
static void benchmark_copy_to_gpu(benchmark::State& state)
{
  ClassWithSize<N>* cpuPointer = new ClassWithSize<N>();

  while (state.KeepRunning()) {
    ClassWithSize<N>* gpuPointer;
    cudaMalloc(&gpuPointer, sizeof(ClassWithSize<N>));
    cudaMemcpy(gpuPointer, cpuPointer, sizeof(ClassWithSize<N>), cudaMemcpyHostToDevice);
    cudaFree(gpuPointer);
    cudaDeviceSynchronize();
  }

  delete cpuPointer;
}

BENCHMARK_TEMPLATE(benchmark_copy_to_gpu, 8);
BENCHMARK_TEMPLATE(benchmark_copy_to_gpu, 64);
BENCHMARK_TEMPLATE(benchmark_copy_to_gpu, 512);
BENCHMARK_TEMPLATE(benchmark_copy_to_gpu, 4096);
BENCHMARK_TEMPLATE(benchmark_copy_to_gpu, 32768);
BENCHMARK_TEMPLATE(benchmark_copy_to_gpu, 262144);
BENCHMARK_TEMPLATE(benchmark_copy_to_gpu, 2097152);

// Benchmark how long it takes to call placement new on the GPU
template <size_t N>
__global__ void placement_new_kernel(ClassWithSize<N>* address) {
   (void) new(address) ClassWithSize<N>();
}

template <size_t N>
__global__ void placement_delete_kernel(ClassWithSize<N>* address) {
   address->~ClassWithSize<N>();
}

template <size_t N>
static void benchmark_placement_new_on_gpu(benchmark::State& state)
{
  while (state.KeepRunning()) {
    ClassWithSize<N>* address;
    cudaMalloc(&address, sizeof(ClassWithSize<N>));
    placement_new_kernel<<<1, 1>>>(address);
    placement_delete_kernel<<<1, 1>>>(address);
    cudaFree(address);
    cudaDeviceSynchronize();
  }
}

BENCHMARK_TEMPLATE(benchmark_placement_new_on_gpu, 8);
BENCHMARK_TEMPLATE(benchmark_placement_new_on_gpu, 64);
BENCHMARK_TEMPLATE(benchmark_placement_new_on_gpu, 512);
BENCHMARK_TEMPLATE(benchmark_placement_new_on_gpu, 4096);
BENCHMARK_TEMPLATE(benchmark_placement_new_on_gpu, 32768);
BENCHMARK_TEMPLATE(benchmark_placement_new_on_gpu, 262144);
BENCHMARK_TEMPLATE(benchmark_placement_new_on_gpu, 2097152);

// Benchmark how long it takes to call new on the GPU
template <size_t N>
__global__ void create_kernel(ClassWithSize<N>** address) {
   *address = new ClassWithSize<N>();
}

template <size_t N>
__global__ void delete_kernel(ClassWithSize<N>** address) {
   delete *address;
}

template <size_t N>
static void benchmark_new_on_gpu(benchmark::State& state)
{
  while (state.KeepRunning()) {
    ClassWithSize<N>** buffer;
    cudaMalloc(&buffer, sizeof(ClassWithSize<N>*));
    create_kernel<<<1, 1>>>(buffer);
    delete_kernel<<<1, 1>>>(buffer);
    cudaFree(buffer);
    cudaDeviceSynchronize();
  }
}

BENCHMARK_TEMPLATE(benchmark_new_on_gpu, 8);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu, 64);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu, 512);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu, 4096);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu, 32768);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu, 262144);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu, 2097152);

// Benchmark current approach
template <size_t N>
__global__ void delete_kernel_2(ClassWithSize<N>* address) {
   delete address;
}

template <size_t N>
static void benchmark_new_on_gpu_and_copy_to_host(benchmark::State& state)
{
  while (state.KeepRunning()) {
    ClassWithSize<N>** gpuBuffer;
    cudaMalloc(&gpuBuffer, sizeof(ClassWithSize<N>*));
    create_kernel<<<1, 1>>>(gpuBuffer);
    ClassWithSize<N>** cpuBuffer = (ClassWithSize<N>**) malloc(sizeof(ClassWithSize<N>*));
    cudaMemcpy(cpuBuffer, gpuBuffer, sizeof(ClassWithSize<N>*), cudaMemcpyDeviceToHost);
    cudaFree(gpuBuffer);
    ClassWithSize<N>* gpuPointer = cpuBuffer[0];
    free(cpuBuffer);
    delete_kernel_2<<<1, 1>>>(gpuPointer);
    cudaDeviceSynchronize();
  }
}

BENCHMARK_TEMPLATE(benchmark_new_on_gpu_and_copy_to_host, 8);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu_and_copy_to_host, 64);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu_and_copy_to_host, 512);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu_and_copy_to_host, 4096);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu_and_copy_to_host, 32768);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu_and_copy_to_host, 262144);
BENCHMARK_TEMPLATE(benchmark_new_on_gpu_and_copy_to_host, 2097152);

// Benchmark how long it takes to create a stack object on the GPU
template <size_t N>
__global__ void create_on_stack_kernel() {
   (void) ClassWithSize<N>();
}

template <size_t N>
static void benchmark_create_on_stack_on_gpu(benchmark::State& state)
{
  while (state.KeepRunning()) {
    create_on_stack_kernel<N><<<1, 1>>>();
    cudaDeviceSynchronize();
  }
}

BENCHMARK_TEMPLATE(benchmark_create_on_stack_on_gpu, 8);
BENCHMARK_TEMPLATE(benchmark_create_on_stack_on_gpu, 64);
BENCHMARK_TEMPLATE(benchmark_create_on_stack_on_gpu, 512);
BENCHMARK_TEMPLATE(benchmark_create_on_stack_on_gpu, 4096);
BENCHMARK_TEMPLATE(benchmark_create_on_stack_on_gpu, 32768);
BENCHMARK_TEMPLATE(benchmark_create_on_stack_on_gpu, 262144);
BENCHMARK_TEMPLATE(benchmark_create_on_stack_on_gpu, 2097152);

// Use managed_ptr
__global__ void fill(size_t numValues, int* values) {
   size_t i = blockIdx.x * blockDim.x + threadIdx.x;

   if (i < numValues) {
      values[i] = i * i;
   }
}

__global__ void square(chai::managed_ptr<Base> object, size_t numValues, int* values) {
   object->scale(numValues, values);
}

void benchmark_use_managed_ptr_gpu(benchmark::State& state)
{
  chai::managed_ptr<Base> object = chai::make_managed<Derived>(2);

  size_t numValues = 100;
  int* values;
  cudaMalloc(&values, numValues * sizeof(int));
  fill<<<1, 100>>>(numValues, values);

  cudaDeviceSynchronize();

  while (state.KeepRunning()) {
    square<<<1, 1>>>(object, numValues, values);
    cudaDeviceSynchronize();
  }

  cudaFree(values);
  object.free();
  cudaDeviceSynchronize();
}

BENCHMARK(benchmark_use_managed_ptr_gpu);

// Curiously recurring template pattern
__global__ void square(BaseCRTP<DerivedCRTP> object, size_t numValues, int* values) {
   object.scale(numValues, values);
}

void benchmark_curiously_recurring_template_pattern_gpu(benchmark::State& state)
{
  BaseCRTP<DerivedCRTP>* derivedCRTP = new DerivedCRTP(2);
  auto object = *derivedCRTP;

  size_t numValues = 100;
  int* values;
  cudaMalloc(&values, numValues * sizeof(int));
  fill<<<1, 100>>>(numValues, values);

  cudaDeviceSynchronize();

  while (state.KeepRunning()) {
    square<<<1, 1>>>(object, numValues, values);
    cudaDeviceSynchronize();
  }

  cudaFree(values);
  delete derivedCRTP;
  cudaDeviceSynchronize();
}

BENCHMARK(benchmark_curiously_recurring_template_pattern_gpu);

// Class without inheritance
__global__ void square(NoInheritance object, size_t numValues, int* values) {
   object.scale(numValues, values);
}

void benchmark_no_inheritance_gpu(benchmark::State& state)
{
  NoInheritance* noInheritance = new NoInheritance(2);
  auto object = *noInheritance;

  size_t numValues = 100;
  int* values;
  cudaMalloc(&values, numValues * sizeof(int));
  fill<<<1, 100>>>(numValues, values);

  cudaDeviceSynchronize();

  while (state.KeepRunning()) {
    square<<<1, 1>>>(object, numValues, values);
    cudaDeviceSynchronize();
  }

  cudaFree(values);
  delete noInheritance;
  cudaDeviceSynchronize();
}

BENCHMARK(benchmark_no_inheritance_gpu);

#endif

BENCHMARK_MAIN();