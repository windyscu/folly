/*
 * Copyright 2014-present Facebook, Inc.
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

#include <folly/Utility.h>

#include <stdexcept>
#include <tuple>

namespace folly {

template <class T>
Try<T>::Try(Try<T>&& t) noexcept : contains_(t.contains_) {
  if (contains_ == Contains::VALUE) {
    new (&value_)T(std::move(t.value_));
  } else if (contains_ == Contains::EXCEPTION) {
    new (&e_) exception_wrapper(std::move(t.e_));
  }
}

template <class T>
template <class T2>
Try<T>::Try(typename std::enable_if<std::is_same<Unit, T2>::value,
                                    Try<void> const&>::type t)
    : contains_(Contains::NOTHING) {
  if (t.hasValue()) {
    contains_ = Contains::VALUE;
    new (&value_) T();
  } else if (t.hasException()) {
    contains_ = Contains::EXCEPTION;
    new (&e_) exception_wrapper(t.exception());
  }
}

template <class T>
Try<T>& Try<T>::operator=(Try<T>&& t) noexcept {
  if (this == &t) {
    return *this;
  }

  this->~Try();
  contains_ = t.contains_;
  if (contains_ == Contains::VALUE) {
    new (&value_)T(std::move(t.value_));
  } else if (contains_ == Contains::EXCEPTION) {
    new (&e_) exception_wrapper(std::move(t.e_));
  }
  return *this;
}

template <class T>
Try<T>::Try(const Try<T>& t) {
  static_assert(
      std::is_copy_constructible<T>::value,
      "T must be copyable for Try<T> to be copyable");
  contains_ = t.contains_;
  if (contains_ == Contains::VALUE) {
    new (&value_)T(t.value_);
  } else if (contains_ == Contains::EXCEPTION) {
    new (&e_) exception_wrapper(t.e_);
  }
}

template <class T>
Try<T>& Try<T>::operator=(const Try<T>& t) {
  static_assert(
      std::is_copy_constructible<T>::value,
      "T must be copyable for Try<T> to be copyable");
  this->~Try();
  contains_ = t.contains_;
  if (contains_ == Contains::VALUE) {
    new (&value_)T(t.value_);
  } else if (contains_ == Contains::EXCEPTION) {
    new (&e_) exception_wrapper(t.e_);
  }
  return *this;
}

template <class T>
Try<T>::~Try() {
  if (LIKELY(contains_ == Contains::VALUE)) {
    value_.~T();
  } else if (UNLIKELY(contains_ == Contains::EXCEPTION)) {
    e_.~exception_wrapper();
  }
}

template <class T>
T& Try<T>::value() & {
  throwIfFailed();
  return value_;
}

template <class T>
T&& Try<T>::value() && {
  throwIfFailed();
  return std::move(value_);
}

template <class T>
const T& Try<T>::value() const & {
  throwIfFailed();
  return value_;
}

template <class T>
const T&& Try<T>::value() const && {
  throwIfFailed();
  return std::move(value_);
}

template <class T>
void Try<T>::throwIfFailed() const {
  switch (contains_) {
    case Contains::VALUE:
      return;
    case Contains::EXCEPTION:
      e_.throw_exception();
    default:
      throw_exception<UsingUninitializedTry>();
  }
}

void Try<void>::throwIfFailed() const {
  if (!hasValue_) {
    e_.throw_exception();
  }
}

template <typename F>
typename std::enable_if<
    !std::is_same<invoke_result_t<F>, void>::value,
    Try<invoke_result_t<F>>>::type
makeTryWith(F&& f) {
  using ResultType = invoke_result_t<F>;
  try {
    return Try<ResultType>(f());
  } catch (std::exception& e) {
    return Try<ResultType>(exception_wrapper(std::current_exception(), e));
  } catch (...) {
    return Try<ResultType>(exception_wrapper(std::current_exception()));
  }
}

template <typename F>
typename std::
    enable_if<std::is_same<invoke_result_t<F>, void>::value, Try<void>>::type
    makeTryWith(F&& f) {
  try {
    f();
    return Try<void>();
  } catch (std::exception& e) {
    return Try<void>(exception_wrapper(std::current_exception(), e));
  } catch (...) {
    return Try<void>(exception_wrapper(std::current_exception()));
  }
}

namespace try_detail {

/**
 * Trait that removes the layer of Try abstractions from the passed in type
 */
template <typename Type>
struct RemoveTry;
template <template <typename...> class TupleType, typename... Types>
struct RemoveTry<TupleType<folly::Try<Types>...>> {
  using type = TupleType<Types...>;
};

template <std::size_t... Indices, typename Tuple>
auto unwrapTryTupleImpl(folly::index_sequence<Indices...>, Tuple&& instance) {
  using std::get;
  using ReturnType = typename RemoveTry<typename std::decay<Tuple>::type>::type;
  return ReturnType{(get<Indices>(std::forward<Tuple>(instance)).value())...};
}
} // namespace try_detail

template <typename Tuple>
auto unwrapTryTuple(Tuple&& instance) {
  using TupleDecayed = typename std::decay<Tuple>::type;
  using Seq = folly::make_index_sequence<std::tuple_size<TupleDecayed>::value>;
  return try_detail::unwrapTryTupleImpl(Seq{}, std::forward<Tuple>(instance));
}

} // namespace folly
