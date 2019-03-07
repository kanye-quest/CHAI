#ifndef MANAGED_PTR_H_
#define MANAGED_PTR_H_

#include "chai/ChaiMacros.hpp"

// Standard libary headers
#include <cstddef>
#include <tuple>

namespace chai {

#ifdef __CUDACC__

   namespace ExecutionStrategy {
      struct Host {};
      struct Device {};
      struct Managed {};
   }

   template <typename T, typename ExecStrategy=ExecutionStrategy::Managed>
   class managed_ptr {};

   template <typename T,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed(ExecutionStrategy::Managed&&, Args&&... args);

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] args The arguments to T's constructor
   ///
   template <typename T,
             typename... Args>
   CHAI_INLINE CHAI_HOST managed_ptr<T> make_managed(Args&&... args) {
      return make_managed<T, Args...>(std::forward<ExecutionStrategy::Managed>(ExecutionStrategy::Managed{}), std::forward<Args>(args)...);
   }

   template <typename T,
             typename F,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed_from_factory(ExecutionStrategy::Managed&&, F&& f, Args&&... args);

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] f The factory function that will create the object
   /// @param[in] args The arguments to the factory function
   ///
   template <typename T,
             typename F,
             typename... Args>
   CHAI_INLINE CHAI_HOST managed_ptr<T> make_managed_from_factory(F&& f, Args&&... args) {
      return make_managed_from_factory<T, F, Args...>(std::forward<ExecutionStrategy::Managed>(ExecutionStrategy::Managed{}), std::forward<F>(f), std::forward<Args>(args)...);
   }

#else // __CUDACC__

   namespace ExecutionStrategy {
      struct Host {};
   }

   template <typename T, typename ExecStrategy=ExecutionStrategy::Host>
   class managed_ptr {};

   template <typename T,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed(ExecutionStrategy::Host&&, Args&&... args);

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] args The arguments to T's constructor
   ///
   template <typename T,
             typename... Args>
   CHAI_INLINE CHAI_HOST managed_ptr<T> make_managed(Args&&... args) {
      return make_managed<T, Args...>(std::forward<ExecutionStrategy::Host>(ExecutionStrategy::Host{}), std::forward<Args>(args)...);
   }

   template <typename T,
             typename F,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed_from_factory(ExecutionStrategy::Host&&, F&& f, Args&&... args);

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] f The factory function that will create the object
   /// @param[in] args The arguments to the factory function
   ///
   template <typename T,
             typename F,
             typename... Args>
   CHAI_INLINE CHAI_HOST managed_ptr<T> make_managed_from_factory(F&& f, Args&&... args) {
      return make_managed_from_factory<T, F, Args...>(std::forward<ExecutionStrategy::Host>(ExecutionStrategy::Host{}), std::forward<F>(f), std::forward<Args>(args)...);
   }

#endif // __CUDACC__

   ///
   /// @class managed_ptr<T>
   /// @author Alan Dayton
   /// This wrapper calls new on both the GPU and CPU so that polymorphism can
   ///    be used on the GPU. It is modeled after std::shared_ptr, so it does
   ///    reference counting and automatically cleans up when the last reference
   ///    is destroyed. If we ever do multi-threading on the CPU, locking will
   ///    need to be added to the reference counter.
   /// Requirements:
   ///    The underlying type created (U in the first constructor) must be convertible
   ///       to T (e.g. T is a base class of U or there is a user defined conversion).
   ///    This wrapper does NOT automatically sync the GPU copy if the CPU copy is
   ///       updated and vice versa. The one exception to this is nested ManagedArrays
   ///       and managed_ptrs, but only if they are registered via the registerArguments
   ///       method. The factory methods make_managed and make_managed_from_factory
   ///       will register arguments passed to them automatically. Otherwise, if you
   ///       wish to keep the CPU and GPU instances in sync, you must explicitly modify
   ///       the object in both the CPU context and the GPU context.
   ///    Members of T that are raw pointers need to be initialized correctly with a
   ///       host or device pointer. If it is desired that these be kept in sync,
   ///       pass a ManagedArray to the make_managed or make_managed_from_factory
   ///       functions in place of a raw array. Or, if this is after the managed_ptr
   ///       has been constructed, use the same ManagedArray in both the CPU and GPU
   ///       contexts to initialize the raw pointer member and then register the
   ///       ManagedArray with the registerArguments method on the managed_ptr.
   ///       If only a raw array is passed to make_managed, accessing that member
   ///       will be valid only in the correct context. To prevent the accidental
   ///       use of that member in the wrong context, any methods that access raw
   ///       pointers not initialized in both contexts as previously described
   ///       should be __host__ only or __device__ only. Special care should be
   ///       taken when passing raw pointers as arguments to member functions.
   ///    Methods that can be called on the CPU and GPU must be declared with the
   ///       __host__ __device__ specifiers. This includes the constructors being
   ///       used and destructors.
   ///
   template <typename T>
   class managed_ptr<T, ExecutionStrategy::Host> {
      public:
         using element_type = T;

         ///
         /// @author Alan Dayton
         ///
         /// Default constructor.
         /// Initializes the reference count to 0.
         ///
         CHAI_HOST constexpr managed_ptr() noexcept {}

         ///
         /// @author Alan Dayton
         ///
         /// Construct from nullptr.
         /// Initializes the reference count to 0.
         ///
         CHAI_HOST constexpr managed_ptr(std::nullptr_t) noexcept {}

         ///
         /// @author Alan Dayton
         ///
         /// Constructs a managed_ptr from the given host pointer.
         ///
         /// @param[in] hostPtr The host pointer to take ownership of
         ///
         template <typename U>
         explicit CHAI_HOST managed_ptr(U* hostPtr) :
            m_cpu(hostPtr),
            m_numReferences(new std::size_t{1})
         {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");
         }

         ///
         /// @author Alan Dayton
         ///
         /// Copy constructor.
         /// Constructs a copy of the given managed_ptr and increases the reference count.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         CHAI_HOST managed_ptr(const managed_ptr& other) noexcept :
            m_cpu(other.m_cpu),
            m_numReferences(other.m_numReferences)
         {
            incrementReferenceCount();
         }

         ///
         /// @author Alan Dayton
         ///
         /// Converting constructor.
         /// Constructs a copy of the given managed_ptr and increases the reference count.
         ///    U must be convertible to T.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         template <typename U>
         CHAI_HOST managed_ptr(const managed_ptr<U, ExecutionStrategy::Host>& other) noexcept :
            m_cpu(other.m_cpu),
            m_numReferences(other.m_numReferences)
         {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");

            incrementReferenceCount();
         }

         ///
         /// @author Alan Dayton
         ///
         /// Aliasing constructor.
         /// Has the same ownership information as other, but holds a different pointer.
         ///
         /// @param[in] other The managed_ptr to copy ownership information from
         /// @param[in] hostPtr The pointer to maintain a reference to
         ///
         template <typename U>
         CHAI_HOST managed_ptr(const managed_ptr<U, ExecutionStrategy::Host>& other,
                               T* hostPtr) noexcept :
            m_cpu(hostPtr),
            m_numReferences(other.m_numReferences)
         {
            incrementReferenceCount();
         }

         ///
         /// @author Alan Dayton
         ///
         /// Destructor. Decreases the reference count and if this is the last reference,
         ///    clean up.
         ///
         CHAI_HOST ~managed_ptr() {
            decrementReferenceCount();
         }

         ///
         /// @author Alan Dayton
         ///
         /// Copy assignment operator.
         /// Copies the given managed_ptr and increases the reference count.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         CHAI_HOST managed_ptr& operator=(const managed_ptr& other) noexcept {
            if (this != &other) {
               decrementReferenceCount();

               m_cpu = other.m_cpu;
               m_numReferences = other.m_numReferences;

               incrementReferenceCount();
            }

            return *this;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Conversion copy assignment operator.
         /// Copies the given managed_ptr and increases the reference count.
         ///    U must be convertible to T.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         template<class U>
         CHAI_HOST managed_ptr& operator=(const managed_ptr<U, ExecutionStrategy::Host>& other) noexcept {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");

            decrementReferenceCount();

            m_cpu = other.m_cpu;
            m_numReferences = other.m_numReferences;

            incrementReferenceCount();

            return *this;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU pointer depending on the calling context.
         ///
         CHAI_HOST inline T* get() const {
            return m_cpu;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU pointer depending on the calling context.
         ///
         CHAI_HOST inline T* operator->() const {
            return m_cpu;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU reference depending on the calling context.
         ///
         CHAI_HOST inline T& operator*() const {
            return *m_cpu;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the number of managed_ptrs owning these pointers.
         ///
         CHAI_HOST std::size_t use_count() const {
            if (m_numReferences) {
               return *m_numReferences;
            }
            else {
               return 0;
            }
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns true if the contained pointer is not nullptr, false otherwise.
         ///
         CHAI_HOST inline explicit operator bool() const noexcept {
            return m_cpu != nullptr;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Implicit conversion operator to T*. Be careful when using this because
         /// data movement is not triggered by the raw pointer.
         ///
         CHAI_HOST inline operator T*() const {
            return get();
         }

         ///
         /// @author Alan Dayton
         ///
         /// Saves the arguments in order to later call their copy constructor.
         ///
         /// @param[in] args The arguments to save
         ///
         template <typename... Args>
         CHAI_HOST void registerArguments(Args&&...) {}

      private:
         T* m_cpu = nullptr; /// The host pointer
         size_t* m_numReferences = nullptr; /// The reference counter

         template <typename U, typename ExecStrategy>
         friend class managed_ptr; /// Needed for the converting constructor

         ///
         /// @author Alan Dayton
         ///
         /// Increments the reference count and calls the copy constructor to
         ///    trigger data movement.
         ///
         CHAI_HOST void incrementReferenceCount() {
            if (m_numReferences) {
               (*m_numReferences)++;
            }
         }

         ///
         /// @author Alan Dayton
         ///
         /// Decrements the reference counter. If the resulting number of references
         ///    is 0, clean up the object.
         ///
         CHAI_HOST void decrementReferenceCount() {
            if (m_numReferences) {
               (*m_numReferences)--;

               if (*m_numReferences == 0) {
                  delete m_numReferences;
                  delete m_cpu;
               }
            }
         }
   };

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] args The arguments to T's constructor
   ///
   template <typename T,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed(ExecutionStrategy::Host&&, Args&&... args) {
      static_assert(std::is_constructible<T, Args...>::value,
                    "Type T must be constructible with the given arguments.");

      T* hostPtr = new T(std::forward<Args>(args)...);
      return managed_ptr<T>(hostPtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] f The factory function that will create the object
   /// @param[in] args The arguments to the factory function
   ///
   template <typename T,
             typename F,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed_from_factory(ExecutionStrategy::Host&&, F&& f, Args&&... args) {
      static_assert(std::is_pointer<typename std::result_of<F(Args...)>::type>::value,
                    "Factory function must return a pointer");

      using R = typename std::remove_pointer<typename std::result_of<F(Args...)>::type>::type;

      static_assert(std::is_base_of<T, R>::value ||
                    std::is_convertible<R, T>::value,
                    "Factory function must have a return type that is a descendent of or is convertible to type T.");

      R* hostPtr = f(std::forward<Args>(args)...);
      return managed_ptr<T>(hostPtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using static_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using static_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Host> static_pointer_cast(const managed_ptr<U, ExecutionStrategy::Host>& other) noexcept {
      auto hostPtr = static_cast<T*>(other.get());
      return managed_ptr<T, ExecutionStrategy::Host>(other, hostPtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using dynamic_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using dynamic_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Host> dynamic_pointer_cast(const managed_ptr<U, ExecutionStrategy::Host>& other) noexcept {
      if (auto hostPtr = dynamic_cast<T*>(other.get())) {
         return managed_ptr<T, ExecutionStrategy::Host>(other, hostPtr);
      }
      else {
         return managed_ptr<T, ExecutionStrategy::Host>();
      }
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using const_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using const_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Host> const_pointer_cast(const managed_ptr<U, ExecutionStrategy::Host>& other) noexcept {
      auto hostPtr = const_cast<T*>(other.get());
      return managed_ptr<T, ExecutionStrategy::Host>(other, hostPtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using reinterpret_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using reinterpret_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Host> reinterpret_pointer_cast(const managed_ptr<U, ExecutionStrategy::Host>& other) noexcept {
      auto hostPtr = reinterpret_cast<T*>(other.get());
      return managed_ptr<T, ExecutionStrategy::Host>(other, hostPtr);
   }

#ifdef __CUDACC__
   namespace detail {
      ///
      /// @author Alan Dayton
      ///
      /// Creates a new T on the device.
      ///
      /// @param[out] devicePtr Used to return the device pointer to the new T
      /// @param[in]  args The arguments to T's constructor
      ///
      /// @note Cannot capture argument packs in an extended device lambda,
      ///       so explicit kernel is needed.
      ///
      template <typename T,
                typename... Args>
      __global__ void make_on_device(T*& devicePtr, Args... args)
      {
         devicePtr = new T(std::forward<Args>(args)...);
      }

      ///
      /// @author Alan Dayton
      ///
      /// Creates a new object on the device by calling the given factory method.
      ///
      /// @param[out] devicePtr Used to return the device pointer to the new object
      /// @param[in]  f The factory method
      /// @param[in]  args The arguments to the factory method
      ///
      /// @note Cannot capture argument packs in an extended device lambda,
      ///       so explicit kernel is needed.
      ///
      template <typename T,
                typename F,
                typename... Args>
      __global__ void make_on_device_from_factory(T*& devicePtr, F f, Args... args)
      {
         devicePtr = f(std::forward<Args>(args)...);
      }

      ///
      /// @author Alan Dayton
      ///
      /// Destroys the device pointer.
      ///
      /// @param[out] devicePtr The device pointer to call delete on
      ///
      template <typename T>
      __global__ void destroy_on_device(T*& devicePtr)
      {
         if (devicePtr) {
            delete devicePtr;
         }
      }

      ///
      /// @author Alan Dayton
      ///
      /// Gets the device pointer from the managed_ptr.
      ///
      /// @param[out] devicePtr Used to return the device pointer
      /// @param[in]  other The managed_ptr from which to extract the device pointer
      ///
      template <typename T, typename ExecStrategy>
      __global__ void get_on_device(T*& devicePtr,
                                    const managed_ptr<T, ExecStrategy>& other)
      {
         devicePtr = other.get();
      }

      ///
      /// @author Alan Dayton
      ///
      /// Converts the underlying pointer on the device using static_cast.
      ///
      /// @param[out] devicePtr The device pointer that will contain the result of
      ///                           calling static_cast on the pointer contained by
      ///                           the given managed_ptr
      /// @param[in] other The managed_ptr to share ownership with and whose pointer to
      ///                      convert using static_cast
      ///
      template <typename T, typename U, typename ExecStrategy>
      __global__ void static_pointer_cast_on_device(T*& devicePtr, const managed_ptr<U, ExecStrategy>& other)
      {
         devicePtr = static_cast<T*>(other.get());
      }

      ///
      /// @author Alan Dayton
      ///
      /// Converts the underlying pointer on the device using const_cast.
      ///
      /// @param[out] devicePtr The device pointer that will contain the result of
      ///                           calling const_cast on the pointer contained by
      ///                           the given managed_ptr
      /// @param[in] other The managed_ptr to share ownership with and whose pointer to
      ///                      convert using const_cast
      ///
      template <typename T, typename U, typename ExecStrategy>
      __global__ void const_pointer_cast_on_device(T*& devicePtr, const managed_ptr<U, ExecStrategy>& other) {
         devicePtr = const_cast<T*>(other.get());
      }

      ///
      /// @author Alan Dayton
      ///
      /// Converts the underlying pointer on the device using reinterpret_cast.
      ///
      /// @param[out] devicePtr The device pointer that will contain the result of
      ///                           calling reinterpret_cast on the pointer contained by
      ///                           the given managed_ptr
      /// @param[in] other The managed_ptr to share ownership with and whose pointer to
      ///                      convert using reinterpret_cast
      ///
      template <typename T, typename U, typename ExecStrategy>
      __global__ void reinterpret_pointer_cast_on_device(T*& devicePtr, const managed_ptr<U, ExecStrategy>& other) {
         devicePtr = reinterpret_cast<T*>(other.get());
      }

      ///
      /// @author Alan Dayton
      ///
      /// Creates a new T on the device.
      ///
      /// @param[in]  args The arguments to T's constructor
      ///
      /// @return The device pointer to the new T
      ///
      template <typename T,
                typename... Args>
      CHAI_HOST T* make_on_device(Args&&... args) {
         T* devicePtr;
         make_on_device<<<1, 1>>>(devicePtr, args...);
         cudaDeviceSynchronize();
         return devicePtr;
      }

      ///
      /// @author Alan Dayton
      ///
      /// Calls a factory method to create a new object on the device.
      ///
      /// @param[in]  f    The factory method
      /// @param[in]  args The arguments to the factory method
      ///
      /// @return The device pointer to the new object
      ///
      template <typename T,
                typename F,
                typename... Args>
      CHAI_HOST T* make_on_device_from_factory(F f, Args&&... args) {
         T* devicePtr;
         make_on_device_from_factory<T><<<1, 1>>>(devicePtr, f, args...);
         cudaDeviceSynchronize();
         return devicePtr;
      }

      ///
      /// @author Alan Dayton
      ///
      /// Gets the device pointer from the managed_ptr.
      ///
      /// @param[in] other The managed_ptr from which to extract the device pointer
      ///
      template <typename T, typename ExecStrategy>
      T* get_on_device(const managed_ptr<T, ExecStrategy>& other) {
         T* devicePtr;
         get_on_device<<<1, 1>>>(devicePtr, other);
         cudaDeviceSynchronize();
         return devicePtr;
      }

      ///
      /// @author Alan Dayton
      ///
      /// Converts the underlying pointer on the device using static_cast.
      ///
      /// @param[in] other The managed_ptr to share ownership with and whose pointer to
      ///                      convert using static_cast
      ///
      template <typename T, typename U, typename ExecStrategy>
      CHAI_HOST T* static_pointer_cast_on_device(const managed_ptr<U, ExecStrategy>& other) noexcept {
         T* devicePtr;
         static_pointer_cast_on_device<<<1, 1>>>(devicePtr, other);
         cudaDeviceSynchronize();
         return devicePtr;
      }

      /// @author Alan Dayton
      ///
      /// Converts the underlying pointer on the device using const_cast.
      ///
      /// @param[in] other The managed_ptr to share ownership with and whose pointer to
      ///                      convert using const_cast
      ///
      template <typename T, typename U, typename ExecStrategy>
      CHAI_HOST T* const_pointer_cast_on_device(const managed_ptr<U, ExecStrategy>& other) noexcept {
         T* devicePtr;
         const_pointer_cast_on_device<<<1, 1>>>(devicePtr, other);
         cudaDeviceSynchronize();
         return devicePtr;
      }

      ///
      /// @author Alan Dayton
      ///
      /// Converts the underlying pointer on the device using reinterpret_cast.
      ///
      /// @param[in] other The managed_ptr to share ownership with and whose pointer to
      ///                      convert using reinterpret_cast
      ///
      template <typename T, typename U, typename ExecStrategy>
      CHAI_HOST T* reinterpret_pointer_cast_on_device(const managed_ptr<U, ExecStrategy>& other) noexcept {
         T* devicePtr;
         reinterpret_pointer_cast_on_device<<<1, 1>>>(devicePtr, other);
         cudaDeviceSynchronize();
         return devicePtr;
      }
   }

   ///
   /// @class managed_ptr<T>
   /// @author Alan Dayton
   /// This wrapper calls new on both the GPU and CPU so that polymorphism can
   ///    be used on the GPU. It is modeled after std::shared_ptr, so it does
   ///    reference counting and automatically cleans up when the last reference
   ///    is destroyed. If we ever do multi-threading on the CPU, locking will
   ///    need to be added to the reference counter.
   /// Requirements:
   ///    The underlying type created (U in the first constructor) must be convertible
   ///       to T (e.g. T is a base class of U or there is a user defined conversion).
   ///    This wrapper does NOT automatically sync the GPU copy if the CPU copy is
   ///       updated and vice versa. The one exception to this is nested ManagedArrays
   ///       and managed_ptrs, but only if they are registered via the registerArguments
   ///       method. The factory methods make_managed and make_managed_from_factory
   ///       will register arguments passed to them automatically. Otherwise, if you
   ///       wish to keep the CPU and GPU instances in sync, you must explicitly modify
   ///       the object in both the CPU context and the GPU context.
   ///    Members of T that are raw pointers need to be initialized correctly with a
   ///       host or device pointer. If it is desired that these be kept in sync,
   ///       pass a ManagedArray to the make_managed or make_managed_from_factory
   ///       functions in place of a raw array. Or, if this is after the managed_ptr
   ///       has been constructed, use the same ManagedArray in both the CPU and GPU
   ///       contexts to initialize the raw pointer member and then register the
   ///       ManagedArray with the registerArguments method on the managed_ptr.
   ///       If only a raw array is passed to make_managed, accessing that member
   ///       will be valid only in the correct context. To prevent the accidental
   ///       use of that member in the wrong context, any methods that access raw
   ///       pointers not initialized in both contexts as previously described
   ///       should be __host__ only or __device__ only. Special care should be
   ///       taken when passing raw pointers as arguments to member functions.
   ///    Methods that can be called on the CPU and GPU must be declared with the
   ///       __host__ __device__ specifiers. This includes the constructors being
   ///       used and destructors.
   ///
   template <typename T>
   class managed_ptr<T, ExecutionStrategy::Managed> {
      public:
         using element_type = T;

         ///
         /// @author Alan Dayton
         ///
         /// Default constructor.
         /// Initializes the reference count to 0.
         ///
         CHAI_HOST_DEVICE constexpr managed_ptr() noexcept {}

         ///
         /// @author Alan Dayton
         ///
         /// Construct from nullptr.
         /// Initializes the reference count to 0.
         ///
         CHAI_HOST_DEVICE constexpr managed_ptr(std::nullptr_t) noexcept {}

         ///
         /// @author Alan Dayton
         ///
         /// Constructs a managed_ptr from the given host and device pointers.
         ///
         /// @param[in] hostPtr The host pointer to take ownership of
         /// @param[in] devicePtr The device pointer to take ownership of
         ///
         template <typename U, typename V>
         CHAI_HOST managed_ptr(U* hostPtr, V* devicePtr) :
            m_gpu(devicePtr),
            m_cpu(hostPtr),
            m_numReferences(new std::size_t{1})
         {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");
            static_assert(std::is_base_of<T, V>::value ||
                          std::is_convertible<V, T>::value,
                          "Type V must a descendent of or be convertible to type T.");
         }

         ///
         /// @author Alan Dayton
         ///
         /// Copy constructor.
         /// Constructs a copy of the given managed_ptr and increases the reference count.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         CHAI_HOST_DEVICE managed_ptr(const managed_ptr& other) noexcept :
            m_cpu(other.m_cpu),
            m_numReferences(other.m_numReferences),
            m_gpu(other.m_gpu),
            m_copyArguments(other.m_copyArguments),
            m_copier(other.m_copier),
            m_deleter(other.m_deleter)
         {
#ifndef __CUDA_ARCH__
            incrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Converting constructor.
         /// Constructs a copy of the given managed_ptr and increases the reference count.
         ///    U must be convertible to T.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         template <typename U>
         CHAI_HOST_DEVICE managed_ptr(const managed_ptr<U, ExecutionStrategy::Managed>& other) noexcept :
            m_cpu(other.m_cpu),
            m_numReferences(other.m_numReferences),
            m_gpu(other.m_gpu),
            m_copyArguments(other.m_copyArguments),
            m_copier(other.m_copier),
            m_deleter(other.m_deleter)
         {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");

#ifndef __CUDA_ARCH__
            incrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Aliasing constructor.
         /// Has the same ownership information as other, but holds a different pointer.
         ///
         /// @param[in] other The managed_ptr to copy ownership information from
         /// @param[in] hostPtr The host pointer to maintain a reference to
         /// @param[in] devicePtr The device pointer to maintain a reference to
         ///
         template <typename U>
         CHAI_HOST_DEVICE managed_ptr(const managed_ptr<U, ExecutionStrategy::Managed>& other, T* hostPtr, T* devicePtr) noexcept :
            m_cpu(hostPtr),
            m_numReferences(other.m_numReferences),
            m_gpu(devicePtr),
            m_copyArguments(other.m_copyArguments),
            m_copier(other.m_copier),
            m_deleter(other.m_deleter)
         {
#ifndef __CUDA_ARCH__
            incrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Destructor. Decreases the reference count and if this is the last reference,
         ///    clean up.
         ///
         CHAI_HOST_DEVICE ~managed_ptr() {
#ifndef __CUDA_ARCH__
            decrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Copy assignment operator.
         /// Copies the given managed_ptr and increases the reference count.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         CHAI_HOST_DEVICE managed_ptr& operator=(const managed_ptr& other) noexcept {
            if (this != &other) {
#ifndef __CUDA_ARCH__
               decrementReferenceCount();
#endif

               m_cpu = other.m_cpu;
               m_numReferences = other.m_numReferences;
               m_gpu = other.m_gpu;
               m_copyArguments = other.m_copyArguments;
               m_copier = other.m_copier;
               m_deleter = other.m_deleter;

#ifndef __CUDA_ARCH__
               incrementReferenceCount();
#endif
            }

            return *this;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Conversion copy assignment operator.
         /// Copies the given managed_ptr and increases the reference count.
         ///    U must be convertible to T.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         template<class U>
         CHAI_HOST_DEVICE managed_ptr& operator=(const managed_ptr<U, ExecutionStrategy::Managed>& other) noexcept {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");

#ifndef __CUDA_ARCH__
            decrementReferenceCount();
#endif

            m_cpu = other.m_cpu;
            m_numReferences = other.m_numReferences;
            m_gpu = other.m_gpu;
            m_copyArguments = other.m_copyArguments;
            m_copier = other.m_copier;
            m_deleter = other.m_deleter;

#ifndef __CUDA_ARCH__
            incrementReferenceCount();
#endif

            return *this;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU pointer depending on the calling context.
         ///
         CHAI_HOST_DEVICE inline T* get() const {
#ifndef __CUDA_ARCH__
            return m_cpu;
#else
            return m_gpu;
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU pointer depending on the calling context.
         ///
         CHAI_HOST_DEVICE inline T* operator->() const {
#ifndef __CUDA_ARCH__
            return m_cpu;
#else
            return m_gpu;
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU reference depending on the calling context.
         ///
         CHAI_HOST_DEVICE inline T& operator*() const {
#ifndef __CUDA_ARCH__
            return *m_cpu;
#else
            return *m_gpu;
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the number of managed_ptrs owning these pointers.
         ///
         CHAI_HOST std::size_t use_count() const {
            if (m_numReferences) {
               return *m_numReferences;
            }
            else {
               return 0;
            }
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns true if the contained pointer is not nullptr, false otherwise.
         ///
         CHAI_HOST_DEVICE inline explicit operator bool() const noexcept {
#ifndef __CUDA_ARCH__
            return m_cpu != nullptr;
#else
            return m_gpu != nullptr;
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Implicit conversion operator to T*. Be careful when using this because
         /// data movement is not triggered by the raw pointer.
         ///
         CHAI_HOST_DEVICE inline operator T*() const {
#ifndef __CUDA_ARCH__
            m_copier(m_copyArguments);
#endif
            return get();
         }

         ///
         /// @author Alan Dayton
         ///
         /// Saves the arguments in order to later call their copy constructor.
         ///
         /// @param[in] args The arguments to save
         ///
         template <typename... Args>
         CHAI_HOST void registerArguments(Args&&... args) {
            m_copyArguments = (void*) new std::tuple<Args...>(args...);

            m_copier = [] (void* copyArguments) {
               std::tuple<Args...>(*(static_cast<std::tuple<Args...>*>(copyArguments)));
            };

            m_deleter = [] (void* copyArguments) {
               delete static_cast<std::tuple<Args...>*>(copyArguments);
            };
         }

      private:
         T* m_cpu = nullptr; /// The host pointer
         T* m_gpu = nullptr; /// The device pointer
         size_t* m_numReferences = nullptr; /// The reference counter
         void* m_copyArguments = nullptr; /// ManagedArrays or managed_ptrs which need the copy constructor called on them
         void (*m_copier)(void*); /// Casts m_copyArguments to the appropriate type and calls the copy constructor
         void (*m_deleter)(void*); /// Casts m_copyArguments to the appropriate type and calls delete

         template <typename U, typename ExecStrategy>
         friend class managed_ptr; /// Needed for the converting constructor

         ///
         /// @author Alan Dayton
         ///
         /// Increments the reference count and calls the copy constructor to
         ///    trigger data movement.
         ///
         CHAI_HOST void incrementReferenceCount() {
            if (m_numReferences) {
               (*m_numReferences)++;

               m_copier(m_copyArguments);
            }
         }

         ///
         /// @author Alan Dayton
         ///
         /// Decrements the reference counter. If the resulting number of references
         ///    is 0, clean up the object.
         ///
         CHAI_HOST void decrementReferenceCount() {
            if (m_numReferences) {
               (*m_numReferences)--;

               if (*m_numReferences == 0) {
                  delete m_numReferences;

                  if (m_deleter) {
                     m_deleter(m_copyArguments);
                  }

                  delete m_cpu;

                  detail::destroy_on_device<<<1, 1>>>(m_gpu);
                  cudaDeviceSynchronize();
               }
            }
         }
   };

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] args The arguments to T's constructor
   ///
   template <typename T,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed(ExecutionStrategy::Managed&&, Args&&... args) {
      static_assert(std::is_constructible<T, Args...>::value,
                    "Type T must be constructible with the given arguments.");

      T* hostPtr = new T(args...);
      T* devicePtr = detail::make_on_device<T>(args...);

      managed_ptr<T> result(hostPtr, devicePtr);
      result.registerArguments(std::forward<Args>(args)...);
      return result;
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] f The factory function that will create the object
   /// @param[in] args The arguments to the factory function
   ///
   template <typename T,
             typename F,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed_from_factory(ExecutionStrategy::Managed&&, F&& f, Args&&... args) {
      static_assert(std::is_pointer<typename std::result_of<F(Args...)>::type>::value,
                    "Factory function must return a pointer");

      using R = typename std::remove_pointer<typename std::result_of<F(Args...)>::type>::type;

      static_assert(std::is_base_of<T, R>::value ||
                    std::is_convertible<R, T>::value,
                    "Factory function must have a return type that is a descendent of or is convertible to type T.");

      R* hostPtr = f(args...);
      R* devicePtr = detail::make_on_device_from_factory<R>(f, args...);

      managed_ptr<T> result(hostPtr, devicePtr);
      result.registerArguments(std::forward<Args>(args)...);
      return result;
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using static_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using static_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Managed> static_pointer_cast(const managed_ptr<U, ExecutionStrategy::Managed>& other) noexcept {
      auto hostPtr = static_cast<T*>(other.get());
      auto devicePtr = detail::static_pointer_cast_on_device<T>(other);
      return managed_ptr<T, ExecutionStrategy::Managed>(other, hostPtr, devicePtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using dynamic_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using dynamic_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Managed> dynamic_pointer_cast(const managed_ptr<U, ExecutionStrategy::Managed>& other) noexcept {
      static_assert(true, "CUDA does not support dynamic_cast");
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using const_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using const_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Managed> const_pointer_cast(const managed_ptr<U, ExecutionStrategy::Managed>& other) noexcept {
      auto hostPtr = const_cast<T*>(other.get());
      auto devicePtr = detail::const_pointer_cast_on_device<T>(other);
      return managed_ptr<T, ExecutionStrategy::Managed>(other, hostPtr, devicePtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using reinterpret_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using reinterpret_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Managed> reinterpret_pointer_cast(const managed_ptr<U, ExecutionStrategy::Managed>& other) noexcept {
      auto hostPtr = reinterpret_cast<T*>(other.get());
      auto devicePtr = detail::reinterpret_pointer_cast_on_device<T>(other);
      return managed_ptr<T, ExecutionStrategy::Managed>(other, hostPtr, devicePtr);
   }
   
   ///
   /// @class managed_ptr<T>
   /// @author Alan Dayton
   /// This wrapper calls new on both the GPU and CPU so that polymorphism can
   ///    be used on the GPU. It is modeled after std::shared_ptr, so it does
   ///    reference counting and automatically cleans up when the last reference
   ///    is destroyed. If we ever do multi-threading on the CPU, locking will
   ///    need to be added to the reference counter.
   /// Requirements:
   ///    The underlying type created (U in the first constructor) must be convertible
   ///       to T (e.g. T is a base class of U or there is a user defined conversion).
   ///    This wrapper does NOT automatically sync the GPU copy if the CPU copy is
   ///       updated and vice versa. The one exception to this is nested ManagedArrays
   ///       and managed_ptrs, but only if they are registered via the registerArguments
   ///       method. The factory methods make_managed and make_managed_from_factory
   ///       will register arguments passed to them automatically. Otherwise, if you
   ///       wish to keep the CPU and GPU instances in sync, you must explicitly modify
   ///       the object in both the CPU context and the GPU context.
   ///    Members of T that are raw pointers need to be initialized correctly with a
   ///       host or device pointer. If it is desired that these be kept in sync,
   ///       pass a ManagedArray to the make_managed or make_managed_from_factory
   ///       functions in place of a raw array. Or, if this is after the managed_ptr
   ///       has been constructed, use the same ManagedArray in both the CPU and GPU
   ///       contexts to initialize the raw pointer member and then register the
   ///       ManagedArray with the registerArguments method on the managed_ptr.
   ///       If only a raw array is passed to make_managed, accessing that member
   ///       will be valid only in the correct context. To prevent the accidental
   ///       use of that member in the wrong context, any methods that access raw
   ///       pointers not initialized in both contexts as previously described
   ///       should be __host__ only or __device__ only. Special care should be
   ///       taken when passing raw pointers as arguments to member functions.
   ///    Methods that can be called on the CPU and GPU must be declared with the
   ///       __host__ __device__ specifiers. This includes the constructors being
   ///       used and destructors.
   ///
   template <typename T>
   class managed_ptr<T, ExecutionStrategy::Device> {
      public:
         using element_type = T;

         ///
         /// @author Alan Dayton
         ///
         /// Default constructor.
         /// Initializes the reference count to 0.
         ///
         CHAI_HOST_DEVICE constexpr managed_ptr() noexcept {}

         ///
         /// @author Alan Dayton
         ///
         /// Construct from nullptr.
         /// Initializes the reference count to 0.
         ///
         CHAI_HOST_DEVICE constexpr managed_ptr(std::nullptr_t) noexcept {}

         ///
         /// @author Alan Dayton
         ///
         /// Constructs a managed_ptr from the given device pointer.
         ///
         /// @param[in] devicePtr The device pointer to take ownership of
         ///
         template <typename U>
         explicit CHAI_HOST managed_ptr(U* devicePtr) :
            m_gpu(devicePtr),
            m_numReferences(new std::size_t{1})
         {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");
         }

         ///
         /// @author Alan Dayton
         ///
         /// Copy constructor.
         /// Constructs a copy of the given managed_ptr and increases the reference count.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         CHAI_HOST_DEVICE managed_ptr(const managed_ptr& other) noexcept :
            m_gpu(other.m_gpu),
            m_numReferences(other.m_numReferences)
         {
#ifndef __CUDA_ARCH__
               incrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Copy constructor.
         /// Constructs a copy of the given managed_ptr and increases the reference count.
         ///    U must be convertible to T.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         template <typename U>
         CHAI_HOST_DEVICE managed_ptr(const managed_ptr<U, ExecutionStrategy::Device>& other) noexcept :
            m_gpu(other.m_gpu),
            m_numReferences(other.m_numReferences)
         {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");

#ifndef __CUDA_ARCH__
               incrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Aliasing constructor.
         /// Has the same ownership information as other, but holds a different pointer.
         ///
         /// @param[in] other The managed_ptr to copy ownership information from
         /// @param[in] devicePtr The pointer to maintain a reference to
         ///
         template <typename U>
         CHAI_HOST_DEVICE managed_ptr(const managed_ptr<U, ExecutionStrategy::Device>& other, T* devicePtr) noexcept :
            m_gpu(devicePtr),
            m_numReferences(other.m_numReferences)
         {
#ifndef __CUDA_ARCH__
            incrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Destructor. Decreases the reference count and if this is the last reference,
         ///    clean up.
         ///
         CHAI_HOST_DEVICE ~managed_ptr() {
#ifndef __CUDA_ARCH__
            decrementReferenceCount();
#endif
         }

         ///
         /// @author Alan Dayton
         ///
         /// Copy assignment operator.
         /// Copies the given managed_ptr and increases the reference count.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         CHAI_HOST_DEVICE managed_ptr& operator=(const managed_ptr& other) noexcept {
            if (this != &other) {
#ifndef __CUDA_ARCH__
               decrementReferenceCount();
#endif

               m_gpu = other.m_gpu;
               m_numReferences = other.m_numReferences;

#ifndef __CUDA_ARCH__
               incrementReferenceCount();
#endif
            }

            return *this;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Conversion copy assignment operator.
         /// Copies the given managed_ptr and increases the reference count.
         ///    U must be convertible to T.
         ///
         /// @param[in] other The managed_ptr to copy
         ///
         template<class U>
         CHAI_HOST_DEVICE managed_ptr& operator=(const managed_ptr<U, ExecutionStrategy::Device>& other) noexcept {
            static_assert(std::is_base_of<T, U>::value ||
                          std::is_convertible<U, T>::value,
                          "Type U must a descendent of or be convertible to type T.");

#ifndef __CUDA_ARCH__
            decrementReferenceCount();
#endif

            m_gpu = other.m_gpu;
            m_numReferences = other.m_numReferences;

#ifndef __CUDA_ARCH__
            incrementReferenceCount();
#endif

            return *this;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU pointer depending on the calling context.
         ///
         CHAI_DEVICE inline T* get() const {
            return m_gpu;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU pointer depending on the calling context.
         ///
         CHAI_DEVICE inline T* operator->() const {
            return m_gpu;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the CPU or GPU reference depending on the calling context.
         ///
         CHAI_DEVICE inline T& operator*() const {
            return *m_gpu;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns the number of managed_ptrs owning these pointers.
         ///
         CHAI_HOST std::size_t use_count() const {
            if (m_numReferences) {
               return *m_numReferences;
            }
            else {
               return 0;
            }
         }

         ///
         /// @author Alan Dayton
         ///
         /// Returns true if the contained pointer is not nullptr, false otherwise.
         ///
         CHAI_DEVICE inline explicit operator bool() const noexcept {
            return m_gpu != nullptr;
         }

         ///
         /// @author Alan Dayton
         ///
         /// Implicit conversion operator to T*. Be careful when using this because
         /// data movement is not triggered by the raw pointer.
         ///
         CHAI_DEVICE inline operator T*() const {
            return get();
         }

         ///
         /// @author Alan Dayton
         ///
         /// Saves the arguments in order to later call their copy constructor.
         ///
         /// @param[in] args The arguments to save
         ///
         template <typename... Args>
         CHAI_HOST void registerArguments(Args&&...) {}

      private:
         T* m_gpu = nullptr; /// The device pointer
         size_t* m_numReferences = nullptr; /// The reference counter

         template <typename U, typename ExecStrategy>
         friend class managed_ptr; /// Needed for the converting constructor

         ///
         /// @author Alan Dayton
         ///
         /// Increments the reference count and calls the copy constructor to
         ///    trigger data movement.
         ///
         CHAI_HOST void incrementReferenceCount() {
            if (m_numReferences) {
               (*m_numReferences)++;
            }
         }

         ///
         /// @author Alan Dayton
         ///
         /// Decrements the reference counter. If the resulting number of references
         ///    is 0, clean up the object.
         ///
         CHAI_HOST void decrementReferenceCount() {
            if (m_numReferences) {
               (*m_numReferences)--;

               if (*m_numReferences == 0) {
                  delete m_numReferences;

                  detail::destroy_on_device<<<1, 1>>>(m_gpu);
                  cudaDeviceSynchronize();
               }
            }
         }
   };

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] args The arguments to T's constructor
   ///
   template <typename T,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed(ExecutionStrategy::Device&&, Args&&... args) {
      static_assert(std::is_constructible<T, Args...>::value,
                    "Type T must be constructible with the given arguments.");

      T* devicePtr = detail::make_on_device<T>(std::forward<Args>(args)...);
      return managed_ptr<T>(devicePtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a managed_ptr<T>.
   /// Factory function to create managed_ptrs.
   ///
   /// @param[in] f The factory function that will create the object
   /// @param[in] args The arguments to the factory function
   ///
   template <typename T,
             typename F,
             typename... Args>
   CHAI_HOST managed_ptr<T> make_managed_from_factory(ExecutionStrategy::Device&&, F&& f, Args&&... args) {
      static_assert(std::is_pointer<typename std::result_of<F(Args...)>::type>::value,
                    "Factory function must return a pointer");

      using R = typename std::remove_pointer<typename std::result_of<F(Args...)>::type>::type;

      static_assert(std::is_base_of<T, R>::value ||
                    std::is_convertible<R, T>::value,
                    "Factory function must have a return type that is a descendent of or is convertible to type T.");

      R* devicePtr = detail::make_on_device_from_factory<R>(f, std::forward<Args>(args)...);
      return managed_ptr<T>(devicePtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using static_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using static_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Device> static_pointer_cast(const managed_ptr<U, ExecutionStrategy::Device>& other) noexcept {
      auto devicePtr = detail::static_pointer_cast_on_device<T>(other);
      return managed_ptr<T, ExecutionStrategy::Device>(other, devicePtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using dynamic_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using dynamic_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Device> dynamic_pointer_cast(const managed_ptr<U, ExecutionStrategy::Device>& other) noexcept {
      static_assert(true, "CUDA does not support dynamic_cast");
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using const_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using const_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Device> const_pointer_cast(const managed_ptr<U, ExecutionStrategy::Device>& other) noexcept {
      auto devicePtr = detail::const_pointer_cast_on_device<T>(other);
      return managed_ptr<T, ExecutionStrategy::Device>(other, devicePtr);
   }

   ///
   /// @author Alan Dayton
   ///
   /// Makes a new managed_ptr that shares ownership with the given managed_ptr, but
   ///    the underlying pointer is converted using reinterpret_cast.
   ///
   /// @param[in] other The managed_ptr to share ownership with and whose pointer to
   ///                      convert using reinterpret_cast
   ///
   template <typename T, typename U>
   CHAI_HOST managed_ptr<T, ExecutionStrategy::Device> reinterpret_pointer_cast(const managed_ptr<U, ExecutionStrategy::Device>& other) noexcept {
      auto devicePtr = detail::reinterpret_pointer_cast_on_device<T>(other);
      return managed_ptr<T, ExecutionStrategy::Device>(other, devicePtr);
   }

#endif // __CUDACC__

   /// Comparison operators

   ///
   /// @author Alan Dayton
   ///
   /// Equals comparison.
   ///
   /// @param[in] lhs The first managed_ptr to compare
   /// @param[in] rhs The second managed_ptr to compare
   ///
   template <class T, class U>
   CHAI_HOST_DEVICE CHAI_INLINE
   bool operator==(const managed_ptr<T>& lhs, const managed_ptr<U>& rhs) noexcept {
      return lhs.get() == rhs.get();
   }

   ///
   /// @author Alan Dayton
   ///
   /// Not equals comparison.
   ///
   /// @param[in] lhs The first managed_ptr to compare
   /// @param[in] rhs The second managed_ptr to compare
   ///
   template <class T, class U>
   CHAI_HOST_DEVICE CHAI_INLINE
   bool operator!=(const managed_ptr<T>& lhs, const managed_ptr<U>& rhs) noexcept {
      return lhs.get() != rhs.get();
   }

   /// Comparison operators with nullptr

   ///
   /// @author Alan Dayton
   ///
   /// Equals comparison with nullptr.
   ///
   /// @param[in] lhs The managed_ptr to compare to nullptr
   ///
   template<class T>
   CHAI_HOST_DEVICE CHAI_INLINE
   bool operator==(const managed_ptr<T>& lhs, std::nullptr_t) noexcept {
      return lhs.get() == nullptr;
   }

   ///
   /// @author Alan Dayton
   ///
   /// Equals comparison with nullptr.
   ///
   /// @param[in] rhs The managed_ptr to compare to nullptr
   ///
   template<class T>
   CHAI_HOST_DEVICE CHAI_INLINE
   bool operator==(std::nullptr_t, const managed_ptr<T>& rhs) noexcept {
      return nullptr == rhs.get();
   }

   ///
   /// @author Alan Dayton
   ///
   /// Not equals comparison with nullptr.
   ///
   /// @param[in] lhs The managed_ptr to compare to nullptr
   ///
   template<class T>
   CHAI_HOST_DEVICE CHAI_INLINE
   bool operator!=(const managed_ptr<T>& lhs, std::nullptr_t) noexcept {
      return lhs.get() != nullptr;
   }

   ///
   /// @author Alan Dayton
   ///
   /// Not equals comparison with nullptr.
   ///
   /// @param[in] rhs The managed_ptr to compare to nullptr
   ///
   template<class T>
   CHAI_HOST_DEVICE CHAI_INLINE
   bool operator!=(std::nullptr_t, const managed_ptr<T>& rhs) noexcept {
      return nullptr != rhs.get();
   }
} // namespace chai

#endif // MANAGED_PTR

