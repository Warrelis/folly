/*
 * Copyright 2017 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#define HAZPTR_H

#include <atomic>
#include <functional>
#include <memory>
#include <type_traits>

/* Stand-in for C++17 std::pmr::memory_resource */
#include <folly/experimental/hazptr/memory_resource.h>

namespace folly {
namespace hazptr {

/** hazptr_rec: Private class that contains hazard pointers. */
class hazptr_rec;

/** hazptr_obj: Private class for objects protected by hazard pointers. */
class hazptr_obj;

/** hazptr_obj_base: Base template for objects protected by hazard pointers. */
template <typename T, typename Deleter>
class hazptr_obj_base;

/** hazptr_domain: Class of hazard pointer domains. Each domain manages a set
 *  of hazard pointers and a set of retired objects. */
class hazptr_domain {
 public:
  constexpr explicit hazptr_domain(
      memory_resource* = get_default_resource()) noexcept;
  ~hazptr_domain();

  hazptr_domain(const hazptr_domain&) = delete;
  hazptr_domain(hazptr_domain&&) = delete;
  hazptr_domain& operator=(const hazptr_domain&) = delete;
  hazptr_domain& operator=(hazptr_domain&&) = delete;

 private:
  template <typename, typename>
  friend class hazptr_obj_base;
  friend class hazptr_holder;

  memory_resource* mr_;
  std::atomic<hazptr_rec*> hazptrs_ = {nullptr};
  std::atomic<hazptr_obj*> retired_ = {nullptr};
  std::atomic<int> hcount_ = {0};
  std::atomic<int> rcount_ = {0};

  void objRetire(hazptr_obj*);
  hazptr_rec* hazptrAcquire();
  void hazptrRelease(hazptr_rec*) noexcept;
  int pushRetired(hazptr_obj* head, hazptr_obj* tail, int count);
  void tryBulkReclaim();
  void bulkReclaim();
};

/** Get the default hazptr_domain */
hazptr_domain& default_hazptr_domain();

/** Definition of hazptr_obj */
class hazptr_obj {
  friend class hazptr_domain;
  template <typename, typename>
  friend class hazptr_obj_base;

  void (*reclaim_)(hazptr_obj*);
  hazptr_obj* next_;
  const void* getObjPtr() const;
};

/** Definition of hazptr_obj_base */
template <typename T, typename Deleter = std::default_delete<T>>
class hazptr_obj_base : public hazptr_obj {
 public:
  /* Retire a removed object and pass the responsibility for
   * reclaiming it to the hazptr library */
  void retire(
      hazptr_domain& domain = default_hazptr_domain(),
      Deleter reclaim = {});

 private:
  Deleter deleter_;
};

/** hazptr_holder: Class for automatic acquisition and release of
 *  hazard pointers, and interface for hazard pointer operations. */
class hazptr_holder {
 public:
  /* Constructor automatically acquires a hazard pointer. */
  explicit hazptr_holder(hazptr_domain& domain = default_hazptr_domain());
  /* Destructor automatically clears and releases the owned hazard pointer. */
  ~hazptr_holder();

  /* Copy and move constructors and assignment operators are
   * disallowed because:
   * - Each hazptr_holder owns exactly one hazard pointer at any time.
   * - Each hazard pointer may have up to one owner at any time. */
  hazptr_holder(const hazptr_holder&) = delete;
  hazptr_holder(hazptr_holder&&) = delete;
  hazptr_holder& operator=(const hazptr_holder&) = delete;
  hazptr_holder& operator=(hazptr_holder&&) = delete;

  /** Hazard pointer operations */
  /* Returns a protected pointer from the source */
  template <typename T>
  T* get_protected(const std::atomic<T*>& src) noexcept;
  /* Return true if successful in protecting ptr if src == ptr after
   * setting the hazard pointer.  Otherwise sets ptr to src. */
  template <typename T>
  bool try_protect(T*& ptr, const std::atomic<T*>& src) noexcept;
  /* Set the hazard pointer to ptr */
  template <typename T>
  void reset(const T* ptr) noexcept;
  /* Set the hazard pointer to nullptr */
  void reset(std::nullptr_t = nullptr) noexcept;

  /* Swap ownership of hazard pointers between hazptr_holder-s. */
  /* Note: The owned hazard pointers remain unmodified during the swap
   * and continue to protect the respective objects that they were
   * protecting before the swap, if any. */
  void swap(hazptr_holder&) noexcept;

 private:
  hazptr_domain* domain_;
  hazptr_rec* hazptr_;
};

void swap(hazptr_holder&, hazptr_holder&) noexcept;

} // namespace hazptr
} // namespace folly

#include "hazptr-impl.h"
