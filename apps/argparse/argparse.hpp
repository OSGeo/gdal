/*
  __ _ _ __ __ _ _ __   __ _ _ __ ___  ___
 / _` | '__/ _` | '_ \ / _` | '__/ __|/ _ \ Argument Parser for Modern C++
| (_| | | | (_| | |_) | (_| | |  \__ \  __/ http://github.com/p-ranav/argparse
 \__,_|_|  \__, | .__/ \__,_|_|  |___/\___|
           |___/|_|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2019-2022 Pranav Srinivas Kumar <pranav.srinivas.kumar@gmail.com>
and other contributors.

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <cerrno>

#ifndef ARGPARSE_MODULE_USE_STD_MODULE
#include <algorithm>
#include <any>
#include <array>
#include <set>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#endif

#ifndef ARGPARSE_CUSTOM_STRTOF
#define ARGPARSE_CUSTOM_STRTOF strtof
#endif

#ifndef ARGPARSE_CUSTOM_STRTOD
#define ARGPARSE_CUSTOM_STRTOD strtod
#endif

#ifndef ARGPARSE_CUSTOM_STRTOLD
#define ARGPARSE_CUSTOM_STRTOLD strtold
#endif

namespace argparse {

namespace details { // namespace for helper methods

template <typename T, typename = void>
struct HasContainerTraits : std::false_type {};

template <> struct HasContainerTraits<std::string> : std::false_type {};

template <> struct HasContainerTraits<std::string_view> : std::false_type {};

template <typename T>
struct HasContainerTraits<
    T, std::void_t<typename T::value_type, decltype(std::declval<T>().begin()),
                   decltype(std::declval<T>().end()),
                   decltype(std::declval<T>().size())>> : std::true_type {};

template <typename T>
inline constexpr bool IsContainer = HasContainerTraits<T>::value;

template <typename T, typename = void>
struct HasStreamableTraits : std::false_type {};

template <typename T>
struct HasStreamableTraits<
    T,
    std::void_t<decltype(std::declval<std::ostream &>() << std::declval<T>())>>
    : std::true_type {};

template <typename T>
inline constexpr bool IsStreamable = HasStreamableTraits<T>::value;

constexpr std::size_t repr_max_container_size = 5;

template <typename T> std::string repr(T const &val) {
  if constexpr (std::is_same_v<T, bool>) {
    return val ? "true" : "false";
  } else if constexpr (std::is_convertible_v<T, std::string_view>) {
    return '"' + std::string{std::string_view{val}} + '"';
  } else if constexpr (IsContainer<T>) {
    std::stringstream out;
    out << "{";
    const auto size = val.size();
    if (size > 1) {
      out << repr(*val.begin());
      std::for_each(
          std::next(val.begin()),
          std::next(
              val.begin(),
              static_cast<typename T::iterator::difference_type>(
                  std::min<std::size_t>(size, repr_max_container_size) - 1)),
          [&out](const auto &v) { out << " " << repr(v); });
      if (size <= repr_max_container_size) {
        out << " ";
      } else {
        out << "...";
      }
    }
    if (size > 0) {
      out << repr(*std::prev(val.end()));
    }
    out << "}";
    return out.str();
  } else if constexpr (IsStreamable<T>) {
    std::stringstream out;
    out << val;
    return out.str();
  } else {
    return "<not representable>";
  }
}

namespace {

template <typename T> constexpr bool standard_signed_integer = false;
template <> constexpr bool standard_signed_integer<signed char> = true;
template <> constexpr bool standard_signed_integer<short int> = true;
template <> constexpr bool standard_signed_integer<int> = true;
template <> constexpr bool standard_signed_integer<long int> = true;
template <> constexpr bool standard_signed_integer<long long int> = true;

template <typename T> constexpr bool standard_unsigned_integer = false;
template <> constexpr bool standard_unsigned_integer<unsigned char> = true;
template <> constexpr bool standard_unsigned_integer<unsigned short int> = true;
template <> constexpr bool standard_unsigned_integer<unsigned int> = true;
template <> constexpr bool standard_unsigned_integer<unsigned long int> = true;
template <>
constexpr bool standard_unsigned_integer<unsigned long long int> = true;

} // namespace

constexpr int radix_2 = 2;
constexpr int radix_8 = 8;
constexpr int radix_10 = 10;
constexpr int radix_16 = 16;

template <typename T>
constexpr bool standard_integer =
    standard_signed_integer<T> || standard_unsigned_integer<T>;

template <class F, class Tuple, class Extra, std::size_t... I>
constexpr decltype(auto)
apply_plus_one_impl(F &&f, Tuple &&t, Extra &&x,
                    std::index_sequence<I...> /*unused*/) {
  return std::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...,
                     std::forward<Extra>(x));
}

template <class F, class Tuple, class Extra>
constexpr decltype(auto) apply_plus_one(F &&f, Tuple &&t, Extra &&x) {
  return details::apply_plus_one_impl(
      std::forward<F>(f), std::forward<Tuple>(t), std::forward<Extra>(x),
      std::make_index_sequence<
          std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
}

constexpr auto pointer_range(std::string_view s) noexcept {
  return std::tuple(s.data(), s.data() + s.size());
}

template <class CharT, class Traits>
constexpr bool starts_with(std::basic_string_view<CharT, Traits> prefix,
                           std::basic_string_view<CharT, Traits> s) noexcept {
  return s.substr(0, prefix.size()) == prefix;
}

enum class chars_format {
  scientific = 0xf1,
  fixed = 0xf2,
  hex = 0xf4,
  binary = 0xf8,
  general = fixed | scientific
};

struct ConsumeBinaryPrefixResult {
  bool is_binary;
  std::string_view rest;
};

constexpr auto consume_binary_prefix(std::string_view s)
    -> ConsumeBinaryPrefixResult {
  if (starts_with(std::string_view{"0b"}, s) ||
      starts_with(std::string_view{"0B"}, s)) {
    s.remove_prefix(2);
    return {true, s};
  }
  return {false, s};
}

struct ConsumeHexPrefixResult {
  bool is_hexadecimal;
  std::string_view rest;
};

using namespace std::literals;

constexpr auto consume_hex_prefix(std::string_view s)
    -> ConsumeHexPrefixResult {
  if (starts_with("0x"sv, s) || starts_with("0X"sv, s)) {
    s.remove_prefix(2);
    return {true, s};
  }
  return {false, s};
}

template <class T, auto Param>
inline auto do_from_chars(std::string_view s) -> T {
  T x{0};
  auto [first, last] = pointer_range(s);
  auto [ptr, ec] = std::from_chars(first, last, x, Param);
  if (ec == std::errc()) {
    if (ptr == last) {
      return x;
    }
    throw std::invalid_argument{"pattern '" + std::string(s) +
                                "' does not match to the end"};
  }
  if (ec == std::errc::invalid_argument) {
    throw std::invalid_argument{"pattern '" + std::string(s) + "' not found"};
  }
  if (ec == std::errc::result_out_of_range) {
    throw std::range_error{"'" + std::string(s) + "' not representable"};
  }
  return x; // unreachable
}

template <class T, auto Param = 0> struct parse_number {
  auto operator()(std::string_view s) -> T {
    return do_from_chars<T, Param>(s);
  }
};

template <class T> struct parse_number<T, radix_2> {
  auto operator()(std::string_view s) -> T {
    if (auto [ok, rest] = consume_binary_prefix(s); ok) {
      return do_from_chars<T, radix_2>(rest);
    }
    throw std::invalid_argument{"pattern not found"};
  }
};

template <class T> struct parse_number<T, radix_16> {
  auto operator()(std::string_view s) -> T {
    if (starts_with("0x"sv, s) || starts_with("0X"sv, s)) {
      if (auto [ok, rest] = consume_hex_prefix(s); ok) {
        try {
          return do_from_chars<T, radix_16>(rest);
        } catch (const std::invalid_argument &err) {
          throw std::invalid_argument("Failed to parse '" + std::string(s) +
                                      "' as hexadecimal: " + err.what());
        } catch (const std::range_error &err) {
          throw std::range_error("Failed to parse '" + std::string(s) +
                                 "' as hexadecimal: " + err.what());
        }
      }
    } else {
      // Allow passing hex numbers without prefix
      // Shape 'x' already has to be specified
      try {
        return do_from_chars<T, radix_16>(s);
      } catch (const std::invalid_argument &err) {
        throw std::invalid_argument("Failed to parse '" + std::string(s) +
                                    "' as hexadecimal: " + err.what());
      } catch (const std::range_error &err) {
        throw std::range_error("Failed to parse '" + std::string(s) +
                               "' as hexadecimal: " + err.what());
      }
    }

    throw std::invalid_argument{"pattern '" + std::string(s) +
                                "' not identified as hexadecimal"};
  }
};

template <class T> struct parse_number<T> {
  auto operator()(std::string_view s) -> T {
    auto [ok, rest] = consume_hex_prefix(s);
    if (ok) {
      try {
        return do_from_chars<T, radix_16>(rest);
      } catch (const std::invalid_argument &err) {
        throw std::invalid_argument("Failed to parse '" + std::string(s) +
                                    "' as hexadecimal: " + err.what());
      } catch (const std::range_error &err) {
        throw std::range_error("Failed to parse '" + std::string(s) +
                               "' as hexadecimal: " + err.what());
      }
    }

    auto [ok_binary, rest_binary] = consume_binary_prefix(s);
    if (ok_binary) {
      try {
        return do_from_chars<T, radix_2>(rest_binary);
      } catch (const std::invalid_argument &err) {
        throw std::invalid_argument("Failed to parse '" + std::string(s) +
                                    "' as binary: " + err.what());
      } catch (const std::range_error &err) {
        throw std::range_error("Failed to parse '" + std::string(s) +
                               "' as binary: " + err.what());
      }
    }

    if (starts_with("0"sv, s)) {
      try {
        return do_from_chars<T, radix_8>(rest);
      } catch (const std::invalid_argument &err) {
        throw std::invalid_argument("Failed to parse '" + std::string(s) +
                                    "' as octal: " + err.what());
      } catch (const std::range_error &err) {
        throw std::range_error("Failed to parse '" + std::string(s) +
                               "' as octal: " + err.what());
      }
    }

    try {
      return do_from_chars<T, radix_10>(rest);
    } catch (const std::invalid_argument &err) {
      throw std::invalid_argument("Failed to parse '" + std::string(s) +
                                  "' as decimal integer: " + err.what());
    } catch (const std::range_error &err) {
      throw std::range_error("Failed to parse '" + std::string(s) +
                             "' as decimal integer: " + err.what());
    }
  }
};

namespace {

template <class T> inline const auto generic_strtod = nullptr;
template <> inline const auto generic_strtod<float> = ARGPARSE_CUSTOM_STRTOF;
template <> inline const auto generic_strtod<double> = ARGPARSE_CUSTOM_STRTOD;
template <>
inline const auto generic_strtod<long double> = ARGPARSE_CUSTOM_STRTOLD;

} // namespace

template <class T> inline auto do_strtod(std::string const &s) -> T {
  if (isspace(static_cast<unsigned char>(s[0])) || s[0] == '+') {
    throw std::invalid_argument{"pattern '" + s + "' not found"};
  }

  auto [first, last] = pointer_range(s);
  char *ptr;

  errno = 0;
  auto x = generic_strtod<T>(first, &ptr);
  if (errno == 0) {
    if (ptr == last) {
      return x;
    }
    throw std::invalid_argument{"pattern '" + s +
                                "' does not match to the end"};
  }
  if (errno == ERANGE) {
    throw std::range_error{"'" + s + "' not representable"};
  }
  return x; // unreachable
}

template <class T> struct parse_number<T, chars_format::general> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); r.is_hexadecimal) {
      throw std::invalid_argument{
          "chars_format::general does not parse hexfloat"};
    }
    if (auto r = consume_binary_prefix(s); r.is_binary) {
      throw std::invalid_argument{
          "chars_format::general does not parse binfloat"};
    }

    try {
      return do_strtod<T>(s);
    } catch (const std::invalid_argument &err) {
      throw std::invalid_argument("Failed to parse '" + s +
                                  "' as number: " + err.what());
    } catch (const std::range_error &err) {
      throw std::range_error("Failed to parse '" + s +
                             "' as number: " + err.what());
    }
  }
};

template <class T> struct parse_number<T, chars_format::hex> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); !r.is_hexadecimal) {
      throw std::invalid_argument{"chars_format::hex parses hexfloat"};
    }
    if (auto r = consume_binary_prefix(s); r.is_binary) {
      throw std::invalid_argument{"chars_format::hex does not parse binfloat"};
    }

    try {
      return do_strtod<T>(s);
    } catch (const std::invalid_argument &err) {
      throw std::invalid_argument("Failed to parse '" + s +
                                  "' as hexadecimal: " + err.what());
    } catch (const std::range_error &err) {
      throw std::range_error("Failed to parse '" + s +
                             "' as hexadecimal: " + err.what());
    }
  }
};

template <class T> struct parse_number<T, chars_format::binary> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); r.is_hexadecimal) {
      throw std::invalid_argument{
          "chars_format::binary does not parse hexfloat"};
    }
    if (auto r = consume_binary_prefix(s); !r.is_binary) {
      throw std::invalid_argument{"chars_format::binary parses binfloat"};
    }

    return do_strtod<T>(s);
  }
};

template <class T> struct parse_number<T, chars_format::scientific> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); r.is_hexadecimal) {
      throw std::invalid_argument{
          "chars_format::scientific does not parse hexfloat"};
    }
    if (auto r = consume_binary_prefix(s); r.is_binary) {
      throw std::invalid_argument{
          "chars_format::scientific does not parse binfloat"};
    }
    if (s.find_first_of("eE") == std::string::npos) {
      throw std::invalid_argument{
          "chars_format::scientific requires exponent part"};
    }

    try {
      return do_strtod<T>(s);
    } catch (const std::invalid_argument &err) {
      throw std::invalid_argument("Failed to parse '" + s +
                                  "' as scientific notation: " + err.what());
    } catch (const std::range_error &err) {
      throw std::range_error("Failed to parse '" + s +
                             "' as scientific notation: " + err.what());
    }
  }
};

template <class T> struct parse_number<T, chars_format::fixed> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); r.is_hexadecimal) {
      throw std::invalid_argument{
          "chars_format::fixed does not parse hexfloat"};
    }
    if (auto r = consume_binary_prefix(s); r.is_binary) {
      throw std::invalid_argument{
          "chars_format::fixed does not parse binfloat"};
    }
    if (s.find_first_of("eE") != std::string::npos) {
      throw std::invalid_argument{
          "chars_format::fixed does not parse exponent part"};
    }

    try {
      return do_strtod<T>(s);
    } catch (const std::invalid_argument &err) {
      throw std::invalid_argument("Failed to parse '" + s +
                                  "' as fixed notation: " + err.what());
    } catch (const std::range_error &err) {
      throw std::range_error("Failed to parse '" + s +
                             "' as fixed notation: " + err.what());
    }
  }
};

template <typename StrIt>
std::string join(StrIt first, StrIt last, const std::string &separator) {
  if (first == last) {
    return "";
  }
  std::stringstream value;
  value << *first;
  ++first;
  while (first != last) {
    value << separator << *first;
    ++first;
  }
  return value.str();
}

template <typename T> struct can_invoke_to_string {
  template <typename U>
  static auto test(int)
      -> decltype(std::to_string(std::declval<U>()), std::true_type{});

  template <typename U> static auto test(...) -> std::false_type;

  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> struct IsChoiceTypeSupported {
  using CleanType = typename std::decay<T>::type;
  static const bool value = std::is_integral<CleanType>::value ||
                            std::is_same<CleanType, std::string>::value ||
                            std::is_same<CleanType, std::string_view>::value ||
                            std::is_same<CleanType, const char *>::value;
};

template <typename StringType>
std::size_t get_levenshtein_distance(const StringType &s1,
                                     const StringType &s2) {
  std::vector<std::vector<std::size_t>> dp(
      s1.size() + 1, std::vector<std::size_t>(s2.size() + 1, 0));

  for (std::size_t i = 0; i <= s1.size(); ++i) {
    for (std::size_t j = 0; j <= s2.size(); ++j) {
      if (i == 0) {
        dp[i][j] = j;
      } else if (j == 0) {
        dp[i][j] = i;
      } else if (s1[i - 1] == s2[j - 1]) {
        dp[i][j] = dp[i - 1][j - 1];
      } else {
        dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
      }
    }
  }

  return dp[s1.size()][s2.size()];
}

template <typename ValueType>
std::string get_most_similar_string(const std::map<std::string, ValueType> &map,
                                    const std::string &input) {
  std::string most_similar{};
  std::size_t min_distance = std::numeric_limits<std::size_t>::max();

  for (const auto &entry : map) {
    std::size_t distance = get_levenshtein_distance(entry.first, input);
    if (distance < min_distance) {
      min_distance = distance;
      most_similar = entry.first;
    }
  }

  return most_similar;
}

} // namespace details

enum class nargs_pattern { optional, any, at_least_one };

enum class default_arguments : unsigned int {
  none = 0,
  help = 1,
  version = 2,
  all = help | version,
};

inline default_arguments operator&(const default_arguments &a,
                                   const default_arguments &b) {
  return static_cast<default_arguments>(
      static_cast<std::underlying_type<default_arguments>::type>(a) &
      static_cast<std::underlying_type<default_arguments>::type>(b));
}

class ArgumentParser;

class Argument {
  friend class ArgumentParser;
  friend auto operator<<(std::ostream &stream, const ArgumentParser &parser)
      -> std::ostream &;

  template <std::size_t N, std::size_t... I>
  explicit Argument(std::string_view prefix_chars,
                    std::array<std::string_view, N> &&a,
                    std::index_sequence<I...> /*unused*/)
      : m_accepts_optional_like_value(false),
        m_is_optional((is_optional(a[I], prefix_chars) || ...)),
        m_is_required(false), m_is_repeatable(false), m_is_used(false),
        m_is_hidden(false), m_prefix_chars(prefix_chars) {
    ((void)m_names.emplace_back(a[I]), ...);
    std::sort(
        m_names.begin(), m_names.end(), [](const auto &lhs, const auto &rhs) {
          return lhs.size() == rhs.size() ? lhs < rhs : lhs.size() < rhs.size();
        });
  }

public:
  template <std::size_t N>
  explicit Argument(std::string_view prefix_chars,
                    std::array<std::string_view, N> &&a)
      : Argument(prefix_chars, std::move(a), std::make_index_sequence<N>{}) {}

  Argument &help(std::string help_text) {
    m_help = std::move(help_text);
    return *this;
  }

  Argument &metavar(std::string metavar) {
    m_metavar = std::move(metavar);
    return *this;
  }

  template <typename T> Argument &default_value(T &&value) {
    m_num_args_range = NArgsRange{0, m_num_args_range.get_max()};
    m_default_value_repr = details::repr(value);

    if constexpr (std::is_convertible_v<T, std::string_view>) {
      m_default_value_str = std::string{std::string_view{value}};
    } else if constexpr (details::can_invoke_to_string<T>::value) {
      m_default_value_str = std::to_string(value);
    }

    m_default_value = std::forward<T>(value);
    return *this;
  }

  Argument &default_value(const char *value) {
    return default_value(std::string(value));
  }

  Argument &required() {
    m_is_required = true;
    return *this;
  }

  Argument &implicit_value(std::any value) {
    m_implicit_value = std::move(value);
    m_num_args_range = NArgsRange{0, 0};
    return *this;
  }

  // This is shorthand for:
  //   program.add_argument("foo")
  //     .default_value(false)
  //     .implicit_value(true)
  Argument &flag() {
    default_value(false);
    implicit_value(true);
    return *this;
  }

  template <class F, class... Args>
  auto action(F &&callable, Args &&... bound_args)
      -> std::enable_if_t<std::is_invocable_v<F, Args..., std::string const>,
                          Argument &> {
    using action_type = std::conditional_t<
        std::is_void_v<std::invoke_result_t<F, Args..., std::string const>>,
        void_action, valued_action>;
    if constexpr (sizeof...(Args) == 0) {
      m_actions.emplace_back<action_type>(std::forward<F>(callable));
    } else {
      m_actions.emplace_back<action_type>(
          [f = std::forward<F>(callable),
           tup = std::make_tuple(std::forward<Args>(bound_args)...)](
              std::string const &opt) mutable {
            return details::apply_plus_one(f, tup, opt);
          });
    }
    return *this;
  }

  auto &store_into(bool &var) {
    flag();
    if (m_default_value.has_value()) {
      var = std::any_cast<bool>(m_default_value);
    }
    action([&var](const auto & /*unused*/) { var = true; });
    return *this;
  }

  template <typename T, typename std::enable_if<std::is_integral<T>::value>::type * = nullptr>
  auto &store_into(T &var) {
    if (m_default_value.has_value()) {
      try
      {
        var = std::any_cast<T>(m_default_value);
      }
      catch (...)
      {
        var = static_cast<T>(std::any_cast<int>(m_default_value));
      }
    }
    action([&var](const auto &s) {
      var = details::parse_number<T, details::radix_10>()(s);
    });
    return *this;
  }

  auto &store_into(double &var) {
    if (m_default_value.has_value()) {
      try
      {
        var = std::any_cast<double>(m_default_value);
      }
      catch (...)
      {
        var = std::any_cast<int>(m_default_value);
      }
    }
    action([&var](const auto &s) {
      var = details::parse_number<double, details::chars_format::general>()(s);
    });
    return *this;
  }

  auto &store_into(std::string &var) {
    if (m_default_value.has_value()) {
      var = std::any_cast<std::string>(m_default_value);
    }
    action([&var](const std::string &s) { var = s; });
    return *this;
  }

  auto &store_into(std::vector<std::string> &var) {
    if (m_default_value.has_value()) {
      var = std::any_cast<std::vector<std::string>>(m_default_value);
    }
    action([this, &var](const std::string &s) {
      if (!m_is_used) {
        var.clear();
      }
      m_is_used = true;
      var.push_back(s);
    });
    return *this;
  }

  auto &store_into(std::vector<int> &var) {
    if (m_default_value.has_value()) {
      var = std::any_cast<std::vector<int>>(m_default_value);
    }
    action([this, &var](const std::string &s) {
      if (!m_is_used) {
        var.clear();
      }
      m_is_used = true;
      var.push_back(details::parse_number<int, details::radix_10>()(s));
    });
    return *this;
  }

  auto &store_into(std::set<std::string> &var) {
    if (m_default_value.has_value()) {
      var = std::any_cast<std::set<std::string>>(m_default_value);
    }
    action([this, &var](const std::string &s) {
      if (!m_is_used) {
        var.clear();
      }
      m_is_used = true;
      var.insert(s);
    });
    return *this;
  }

  auto &store_into(std::set<int> &var) {
    if (m_default_value.has_value()) {
      var = std::any_cast<std::set<int>>(m_default_value);
    }
    action([this, &var](const std::string &s) {
      if (!m_is_used) {
        var.clear();
      }
      m_is_used = true;
      var.insert(details::parse_number<int, details::radix_10>()(s));
    });
    return *this;
  }

  auto &append() {
    m_is_repeatable = true;
    return *this;
  }

  // Cause the argument to be invisible in usage and help
  auto &hidden() {
    m_is_hidden = true;
    return *this;
  }

  template <char Shape, typename T>
  auto scan() -> std::enable_if_t<std::is_arithmetic_v<T>, Argument &> {
    static_assert(!(std::is_const_v<T> || std::is_volatile_v<T>),
                  "T should not be cv-qualified");
    auto is_one_of = [](char c, auto... x) constexpr {
      return ((c == x) || ...);
    };

    if constexpr (is_one_of(Shape, 'd') && details::standard_integer<T>) {
      action(details::parse_number<T, details::radix_10>());
    } else if constexpr (is_one_of(Shape, 'i') &&
                         details::standard_integer<T>) {
      action(details::parse_number<T>());
    } else if constexpr (is_one_of(Shape, 'u') &&
                         details::standard_unsigned_integer<T>) {
      action(details::parse_number<T, details::radix_10>());
    } else if constexpr (is_one_of(Shape, 'b') &&
                         details::standard_unsigned_integer<T>) {
      action(details::parse_number<T, details::radix_2>());
    } else if constexpr (is_one_of(Shape, 'o') &&
                         details::standard_unsigned_integer<T>) {
      action(details::parse_number<T, details::radix_8>());
    } else if constexpr (is_one_of(Shape, 'x', 'X') &&
                         details::standard_unsigned_integer<T>) {
      action(details::parse_number<T, details::radix_16>());
    } else if constexpr (is_one_of(Shape, 'a', 'A') &&
                         std::is_floating_point_v<T>) {
      action(details::parse_number<T, details::chars_format::hex>());
    } else if constexpr (is_one_of(Shape, 'e', 'E') &&
                         std::is_floating_point_v<T>) {
      action(details::parse_number<T, details::chars_format::scientific>());
    } else if constexpr (is_one_of(Shape, 'f', 'F') &&
                         std::is_floating_point_v<T>) {
      action(details::parse_number<T, details::chars_format::fixed>());
    } else if constexpr (is_one_of(Shape, 'g', 'G') &&
                         std::is_floating_point_v<T>) {
      action(details::parse_number<T, details::chars_format::general>());
    } else {
      static_assert(alignof(T) == 0, "No scan specification for T");
    }

    return *this;
  }

  Argument &nargs(std::size_t num_args) {
    m_num_args_range = NArgsRange{num_args, num_args};
    return *this;
  }

  Argument &nargs(std::size_t num_args_min, std::size_t num_args_max) {
    m_num_args_range = NArgsRange{num_args_min, num_args_max};
    return *this;
  }

  Argument &nargs(nargs_pattern pattern) {
    switch (pattern) {
    case nargs_pattern::optional:
      m_num_args_range = NArgsRange{0, 1};
      break;
    case nargs_pattern::any:
      m_num_args_range =
          NArgsRange{0, (std::numeric_limits<std::size_t>::max)()};
      break;
    case nargs_pattern::at_least_one:
      m_num_args_range =
          NArgsRange{1, (std::numeric_limits<std::size_t>::max)()};
      break;
    }
    return *this;
  }

  Argument &remaining() {
    m_accepts_optional_like_value = true;
    return nargs(nargs_pattern::any);
  }

  template <typename T> void add_choice(T &&choice) {
    static_assert(details::IsChoiceTypeSupported<T>::value,
                  "Only string or integer type supported for choice");
    static_assert(std::is_convertible_v<T, std::string_view> ||
                      details::can_invoke_to_string<T>::value,
                  "Choice is not convertible to string_type");
    if (!m_choices.has_value()) {
      m_choices = std::vector<std::string>{};
    }

    if constexpr (std::is_convertible_v<T, std::string_view>) {
      m_choices.value().push_back(
          std::string{std::string_view{std::forward<T>(choice)}});
    } else if constexpr (details::can_invoke_to_string<T>::value) {
      m_choices.value().push_back(std::to_string(std::forward<T>(choice)));
    }
  }

  Argument &choices() {
    if (!m_choices.has_value()) {
      throw std::runtime_error("Zero choices provided");
    }
    return *this;
  }

  template <typename T, typename... U>
  Argument &choices(T &&first, U &&... rest) {
    add_choice(std::forward<T>(first));
    choices(std::forward<U>(rest)...);
    return *this;
  }

  void find_default_value_in_choices_or_throw() const {

    const auto &choices = m_choices.value();

    if (m_default_value.has_value()) {
      if (std::find(choices.begin(), choices.end(), m_default_value_str) ==
          choices.end()) {
        // provided arg not in list of allowed choices
        // report error

        std::string choices_as_csv =
            std::accumulate(choices.begin(), choices.end(), std::string(),
                            [](const std::string &a, const std::string &b) {
                              return a + (a.empty() ? "" : ", ") + b;
                            });

        throw std::runtime_error(
            std::string{"Invalid default value "} + m_default_value_repr +
            " - allowed options: {" + choices_as_csv + "}");
      }
    }
  }

  template <typename Iterator>
  void find_value_in_choices_or_throw(Iterator it) const {

    const auto &choices = m_choices.value();

    if (std::find(choices.begin(), choices.end(), *it) == choices.end()) {
      // provided arg not in list of allowed choices
      // report error

      std::string choices_as_csv =
          std::accumulate(choices.begin(), choices.end(), std::string(),
                          [](const std::string &a, const std::string &b) {
                            return a + (a.empty() ? "" : ", ") + b;
                          });

      throw std::runtime_error(std::string{"Invalid argument "} +
                               details::repr(*it) + " - allowed options: {" +
                               choices_as_csv + "}");
    }
  }

  /* The dry_run parameter can be set to true to avoid running the actions,
   * and setting m_is_used. This may be used by a pre-processing step to do
   * a first iteration over arguments.
   */
  template <typename Iterator>
  Iterator consume(Iterator start, Iterator end,
                   std::string_view used_name = {}, bool dry_run = false) {
    if (!m_is_repeatable && m_is_used) {
      throw std::runtime_error(
          std::string("Duplicate argument ").append(used_name));
    }
    m_used_name = used_name;

    if (m_choices.has_value()) {
      // Check each value in (start, end) and make sure
      // it is in the list of allowed choices/options
      std::size_t i = 0;
      auto max_number_of_args = m_num_args_range.get_max();
      for (auto it = start; it != end; ++it) {
        if (i == max_number_of_args) {
          break;
        }
        find_value_in_choices_or_throw(it);
        i += 1;
      }
    }

    const auto num_args_max = m_num_args_range.get_max();
    const auto num_args_min = m_num_args_range.get_min();
    std::size_t dist = 0;
    if (num_args_max == 0) {
      if (!dry_run) {
        m_values.emplace_back(m_implicit_value);
        for(auto &action: m_actions) {
          std::visit([&](const auto &f) { f({}); }, action);
        }
        if(m_actions.empty()){
          std::visit([&](const auto &f) { f({}); }, m_default_action);
        }
        m_is_used = true;
      }
      return start;
    }
    if ((dist = static_cast<std::size_t>(std::distance(start, end))) >=
        num_args_min) {
      if (num_args_max < dist) {
        end = std::next(start, static_cast<typename Iterator::difference_type>(
                                   num_args_max));
      }
      if (!m_accepts_optional_like_value) {
        end = std::find_if(
            start, end,
            std::bind(is_optional, std::placeholders::_1, m_prefix_chars));
        dist = static_cast<std::size_t>(std::distance(start, end));
        if (dist < num_args_min) {
          throw std::runtime_error("Too few arguments for '" +
                                   std::string(m_used_name) + "'.");
        }
      }

      struct ActionApply {
        void operator()(valued_action &f) {
          std::transform(first, last, std::back_inserter(self.m_values), f);
        }

        void operator()(void_action &f) {
          std::for_each(first, last, f);
          if (!self.m_default_value.has_value()) {
            if (!self.m_accepts_optional_like_value) {
              self.m_values.resize(
                  static_cast<std::size_t>(std::distance(first, last)));
            }
          }
        }

        Iterator first, last;
        Argument &self;
      };
      if (!dry_run) {
        for(auto &action: m_actions) {
          std::visit(ActionApply{start, end, *this}, action);
        }
        if(m_actions.empty()){
          std::visit(ActionApply{start, end, *this}, m_default_action);
        }
        m_is_used = true;
      }
      return end;
    }
    if (m_default_value.has_value()) {
      if (!dry_run) {
        m_is_used = true;
      }
      return start;
    }
    throw std::runtime_error("Too few arguments for '" +
                             std::string(m_used_name) + "'.");
  }

  /*
   * @throws std::runtime_error if argument values are not valid
   */
  void validate() const {
    if (m_is_optional) {
      // TODO: check if an implicit value was programmed for this argument
      if (!m_is_used && !m_default_value.has_value() && m_is_required) {
        throw_required_arg_not_used_error();
      }
      if (m_is_used && m_is_required && m_values.empty()) {
        throw_required_arg_no_value_provided_error();
      }
    } else {
      if (!m_num_args_range.contains(m_values.size()) &&
          !m_default_value.has_value()) {
        throw_nargs_range_validation_error();
      }
    }

    if (m_choices.has_value()) {
      // Make sure the default value (if provided)
      // is in the list of choices
      find_default_value_in_choices_or_throw();
    }
  }

  std::string get_names_csv(char separator = ',') const {
    return std::accumulate(
        m_names.begin(), m_names.end(), std::string{""},
        [&](const std::string &result, const std::string &name) {
          return result.empty() ? name : result + separator + name;
        });
  }

  std::string get_usage_full() const {
    std::stringstream usage;

    usage << get_names_csv('/');
    const std::string metavar = !m_metavar.empty() ? m_metavar : "VAR";
    if (m_num_args_range.get_max() > 0) {
      usage << " " << metavar;
      if (m_num_args_range.get_max() > 1) {
        usage << "...";
      }
    }
    return usage.str();
  }

  std::string get_inline_usage() const {
    std::stringstream usage;
    // Find the longest variant to show in the usage string
    std::string longest_name = m_names.front();
    for (const auto &s : m_names) {
      if (s.size() > longest_name.size()) {
        longest_name = s;
      }
    }
    if (!m_is_required) {
      usage << "[";
    }
    usage << longest_name;
    const std::string metavar = !m_metavar.empty() ? m_metavar : "VAR";
    if (m_num_args_range.get_max() > 0) {
      usage << " " << metavar;
      if (m_num_args_range.get_max() > 1 &&
          m_metavar.find("> <") == std::string::npos) {
        usage << "...";
      }
    }
    if (!m_is_required) {
      usage << "]";
    }
    if (m_is_repeatable) {
      usage << "...";
    }
    return usage.str();
  }

  std::size_t get_arguments_length() const {

    std::size_t names_size = std::accumulate(
        std::begin(m_names), std::end(m_names), std::size_t(0),
        [](const auto &sum, const auto &s) { return sum + s.size(); });

    if (is_positional(m_names.front(), m_prefix_chars)) {
      // A set metavar means this replaces the names
      if (!m_metavar.empty()) {
        // Indent and metavar
        return 2 + m_metavar.size();
      }

      // Indent and space-separated
      return 2 + names_size + (m_names.size() - 1);
    }
    // Is an option - include both names _and_ metavar
    // size = text + (", " between names)
    std::size_t size = names_size + 2 * (m_names.size() - 1);
    if (!m_metavar.empty() && m_num_args_range == NArgsRange{1, 1}) {
      size += m_metavar.size() + 1;
    }
    return size + 2; // indent
  }

  friend std::ostream &operator<<(std::ostream &stream,
                                  const Argument &argument) {
    std::stringstream name_stream;
    name_stream << "  "; // indent
    if (argument.is_positional(argument.m_names.front(),
                               argument.m_prefix_chars)) {
      if (!argument.m_metavar.empty()) {
        name_stream << argument.m_metavar;
      } else {
        name_stream << details::join(argument.m_names.begin(),
                                     argument.m_names.end(), " ");
      }
    } else {
      name_stream << details::join(argument.m_names.begin(),
                                   argument.m_names.end(), ", ");
      // If we have a metavar, and one narg - print the metavar
      if (!argument.m_metavar.empty() &&
          argument.m_num_args_range == NArgsRange{1, 1}) {
        name_stream << " " << argument.m_metavar;
      }
      else if (!argument.m_metavar.empty() &&
               argument.m_num_args_range.get_min() == argument.m_num_args_range.get_max() &&
               argument.m_metavar.find("> <") != std::string::npos) {
        name_stream << " " << argument.m_metavar;
      }
    }

    // align multiline help message
    auto stream_width = stream.width();
    auto name_padding = std::string(name_stream.str().size(), ' ');
    auto pos = std::string::size_type{};
    auto prev = std::string::size_type{};
    auto first_line = true;
    auto hspace = "  "; // minimal space between name and help message
    stream << name_stream.str();
    std::string_view help_view(argument.m_help);
    while ((pos = argument.m_help.find('\n', prev)) != std::string::npos) {
      auto line = help_view.substr(prev, pos - prev + 1);
      if (first_line) {
        stream << hspace << line;
        first_line = false;
      } else {
        stream.width(stream_width);
        stream << name_padding << hspace << line;
      }
      prev += pos - prev + 1;
    }
    if (first_line) {
      stream << hspace << argument.m_help;
    } else {
      auto leftover = help_view.substr(prev, argument.m_help.size() - prev);
      if (!leftover.empty()) {
        stream.width(stream_width);
        stream << name_padding << hspace << leftover;
      }
    }

    // print nargs spec
    if (!argument.m_help.empty()) {
      stream << " ";
    }
    stream << argument.m_num_args_range;

    bool add_space = false;
    if (argument.m_default_value.has_value() &&
        argument.m_num_args_range != NArgsRange{0, 0}) {
      stream << "[default: " << argument.m_default_value_repr << "]";
      add_space = true;
    } else if (argument.m_is_required) {
      stream << "[required]";
      add_space = true;
    }
    if (argument.m_is_repeatable) {
      if (add_space) {
        stream << " ";
      }
      stream << "[may be repeated]";
    }
    stream << "\n";
    return stream;
  }

  template <typename T> bool operator!=(const T &rhs) const {
    return !(*this == rhs);
  }

  /*
   * Compare to an argument value of known type
   * @throws std::logic_error in case of incompatible types
   */
  template <typename T> bool operator==(const T &rhs) const {
    if constexpr (!details::IsContainer<T>) {
      return get<T>() == rhs;
    } else {
      using ValueType = typename T::value_type;
      auto lhs = get<T>();
      return std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs),
                        std::end(rhs), [](const auto &a, const auto &b) {
                          return std::any_cast<const ValueType &>(a) == b;
                        });
    }
  }

  /*
   * positional:
   *    _empty_
   *    '-'
   *    '-' decimal-literal
   *    !'-' anything
   */
  static bool is_positional(std::string_view name,
                            std::string_view prefix_chars) {
    auto first = lookahead(name);

    if (first == eof) {
      return true;
    }
    if (prefix_chars.find(static_cast<char>(first)) !=
                          std::string_view::npos) {
      name.remove_prefix(1);
      if (name.empty()) {
        return true;
      }
      return is_decimal_literal(name);
    }
    return true;
  }

private:
  class NArgsRange {
    std::size_t m_min;
    std::size_t m_max;

  public:
    NArgsRange(std::size_t minimum, std::size_t maximum)
        : m_min(minimum), m_max(maximum) {
      if (minimum > maximum) {
        throw std::logic_error("Range of number of arguments is invalid");
      }
    }

    bool contains(std::size_t value) const {
      return value >= m_min && value <= m_max;
    }

    bool is_exact() const { return m_min == m_max; }

    bool is_right_bounded() const {
      return m_max < (std::numeric_limits<std::size_t>::max)();
    }

    std::size_t get_min() const { return m_min; }

    std::size_t get_max() const { return m_max; }

    // Print help message
    friend auto operator<<(std::ostream &stream, const NArgsRange &range)
        -> std::ostream & {
      if (range.m_min == range.m_max) {
        if (range.m_min != 0 && range.m_min != 1) {
          stream << "[nargs: " << range.m_min << "] ";
        }
      } else {
        if (range.m_max == (std::numeric_limits<std::size_t>::max)()) {
          stream << "[nargs: " << range.m_min << " or more] ";
        } else {
          stream << "[nargs=" << range.m_min << ".." << range.m_max << "] ";
        }
      }
      return stream;
    }

    bool operator==(const NArgsRange &rhs) const {
      return rhs.m_min == m_min && rhs.m_max == m_max;
    }

    bool operator!=(const NArgsRange &rhs) const { return !(*this == rhs); }
  };

  void throw_nargs_range_validation_error() const {
    std::stringstream stream;
    if (!m_used_name.empty()) {
      stream << m_used_name << ": ";
    } else {
      stream << m_names.front() << ": ";
    }
    if (m_num_args_range.is_exact()) {
      stream << m_num_args_range.get_min();
    } else if (m_num_args_range.is_right_bounded()) {
      stream << m_num_args_range.get_min() << " to "
             << m_num_args_range.get_max();
    } else {
      stream << m_num_args_range.get_min() << " or more";
    }
    stream << " argument(s) expected. " << m_values.size() << " provided.";
    throw std::runtime_error(stream.str());
  }

  void throw_required_arg_not_used_error() const {
    std::stringstream stream;
    stream << m_names.front() << ": required.";
    throw std::runtime_error(stream.str());
  }

  void throw_required_arg_no_value_provided_error() const {
    std::stringstream stream;
    stream << m_used_name << ": no value provided.";
    throw std::runtime_error(stream.str());
  }

  static constexpr int eof = std::char_traits<char>::eof();

  static auto lookahead(std::string_view s) -> int {
    if (s.empty()) {
      return eof;
    }
    return static_cast<int>(static_cast<unsigned char>(s[0]));
  }

  /*
   * decimal-literal:
   *    '0'
   *    nonzero-digit digit-sequence_opt
   *    integer-part fractional-part
   *    fractional-part
   *    integer-part '.' exponent-part_opt
   *    integer-part exponent-part
   *
   * integer-part:
   *    digit-sequence
   *
   * fractional-part:
   *    '.' post-decimal-point
   *
   * post-decimal-point:
   *    digit-sequence exponent-part_opt
   *
   * exponent-part:
   *    'e' post-e
   *    'E' post-e
   *
   * post-e:
   *    sign_opt digit-sequence
   *
   * sign: one of
   *    '+' '-'
   */
  static bool is_decimal_literal(std::string_view s) {
    if (s == "inf") {
      return true;
    }

    auto is_digit = [](auto c) constexpr {
      switch (c) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        return true;
      default:
        return false;
      }
    };

    // precondition: we have consumed or will consume at least one digit
    auto consume_digits = [=](std::string_view sd) {
      // NOLINTNEXTLINE(readability-qualified-auto)
      auto it = std::find_if_not(std::begin(sd), std::end(sd), is_digit);
      return sd.substr(static_cast<std::size_t>(it - std::begin(sd)));
    };

    switch (lookahead(s)) {
    case '0': {
      s.remove_prefix(1);
      if (s.empty()) {
        return true;
      }
      goto integer_part;
    }
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      s = consume_digits(s);
      if (s.empty()) {
        return true;
      }
      goto integer_part_consumed;
    }
    case '.': {
      s.remove_prefix(1);
      goto post_decimal_point;
    }
    default:
      return false;
    }

  integer_part:
    s = consume_digits(s);
  integer_part_consumed:
    switch (lookahead(s)) {
    case '.': {
      s.remove_prefix(1);
      if (is_digit(lookahead(s))) {
        goto post_decimal_point;
      } else {
        goto exponent_part_opt;
      }
    }
    case 'e':
    case 'E': {
      s.remove_prefix(1);
      goto post_e;
    }
    default:
      return false;
    }

  post_decimal_point:
    if (is_digit(lookahead(s))) {
      s = consume_digits(s);
      goto exponent_part_opt;
    }
    return false;

  exponent_part_opt:
    switch (lookahead(s)) {
    case eof:
      return true;
    case 'e':
    case 'E': {
      s.remove_prefix(1);
      goto post_e;
    }
    default:
      return false;
    }

  post_e:
    switch (lookahead(s)) {
    case '-':
    case '+':
      s.remove_prefix(1);
    }
    if (is_digit(lookahead(s))) {
      s = consume_digits(s);
      return s.empty();
    }
    return false;
  }

  static bool is_optional(std::string_view name,
                          std::string_view prefix_chars) {
    return !is_positional(name, prefix_chars);
  }

  /*
   * Get argument value given a type
   * @throws std::logic_error in case of incompatible types
   */
  template <typename T> T get() const {
    if (!m_values.empty()) {
      if constexpr (details::IsContainer<T>) {
        return any_cast_container<T>(m_values);
      } else {
        return std::any_cast<T>(m_values.front());
      }
    }
    if (m_default_value.has_value()) {
      return std::any_cast<T>(m_default_value);
    }
    if constexpr (details::IsContainer<T>) {
      if (!m_accepts_optional_like_value) {
        return any_cast_container<T>(m_values);
      }
    }

    throw std::logic_error("No value provided for '" + m_names.back() + "'.");
  }

  /*
   * Get argument value given a type.
   * @pre The object has no default value.
   * @returns The stored value if any, std::nullopt otherwise.
   */
  template <typename T> auto present() const -> std::optional<T> {
    if (m_default_value.has_value()) {
      throw std::logic_error("Argument with default value always presents");
    }
    if (m_values.empty()) {
      return std::nullopt;
    }
    if constexpr (details::IsContainer<T>) {
      return any_cast_container<T>(m_values);
    }
    return std::any_cast<T>(m_values.front());
  }

  template <typename T>
  static auto any_cast_container(const std::vector<std::any> &operand) -> T {
    using ValueType = typename T::value_type;

    T result;
    std::transform(
        std::begin(operand), std::end(operand), std::back_inserter(result),
        [](const auto &value) { return std::any_cast<ValueType>(value); });
    return result;
  }

  void set_usage_newline_counter(int i) { m_usage_newline_counter = i; }

  void set_group_idx(std::size_t i) { m_group_idx = i; }

  std::vector<std::string> m_names;
  std::string_view m_used_name;
  std::string m_help;
  std::string m_metavar;
  std::any m_default_value;
  std::string m_default_value_repr;
  std::optional<std::string>
      m_default_value_str; // used for checking default_value against choices
  std::any m_implicit_value;
  std::optional<std::vector<std::string>> m_choices{std::nullopt};
  using valued_action = std::function<std::any(const std::string &)>;
  using void_action = std::function<void(const std::string &)>;
  std::vector<std::variant<valued_action, void_action>> m_actions;
  std::variant<valued_action, void_action> m_default_action{
    std::in_place_type<valued_action>,
    [](const std::string &value) { return value; }};
  std::vector<std::any> m_values;
  NArgsRange m_num_args_range{1, 1};
  // Bit field of bool values. Set default value in ctor.
  bool m_accepts_optional_like_value : 1;
  bool m_is_optional : 1;
  bool m_is_required : 1;
  bool m_is_repeatable : 1;
  bool m_is_used : 1;
  bool m_is_hidden : 1;            // if set, does not appear in usage or help
  std::string_view m_prefix_chars; // ArgumentParser has the prefix_chars
  int m_usage_newline_counter = 0;
  std::size_t m_group_idx = 0;
};

class ArgumentParser {
public:
  explicit ArgumentParser(std::string program_name = {},
                          std::string version = "1.0",
                          default_arguments add_args = default_arguments::all,
                          bool exit_on_default_arguments = true,
                          std::ostream &os = std::cout)
      : m_program_name(std::move(program_name)), m_version(std::move(version)),
        m_exit_on_default_arguments(exit_on_default_arguments),
        m_parser_path(m_program_name) {
    if ((add_args & default_arguments::help) == default_arguments::help) {
      add_argument("-h", "--help")
          .action([&](const auto & /*unused*/) {
            os << help().str();
            if (m_exit_on_default_arguments) {
              std::exit(0);
            }
          })
          .default_value(false)
          .help("shows help message and exits")
          .implicit_value(true)
          .nargs(0);
    }
    if ((add_args & default_arguments::version) == default_arguments::version) {
      add_argument("-v", "--version")
          .action([&](const auto & /*unused*/) {
            os << m_version << std::endl;
            if (m_exit_on_default_arguments) {
              std::exit(0);
            }
          })
          .default_value(false)
          .help("prints version information and exits")
          .implicit_value(true)
          .nargs(0);
    }
  }

  ~ArgumentParser() = default;

  // ArgumentParser is meant to be used in a single function.
  // Setup everything and parse arguments in one place.
  //
  // ArgumentParser internally uses std::string_views,
  // references, iterators, etc.
  // Many of these elements become invalidated after a copy or move.
  ArgumentParser(const ArgumentParser &other) = delete;
  ArgumentParser &operator=(const ArgumentParser &other) = delete;
  ArgumentParser(ArgumentParser &&) noexcept = delete;
  ArgumentParser &operator=(ArgumentParser &&) = delete;

  explicit operator bool() const {
    auto arg_used = std::any_of(m_argument_map.cbegin(), m_argument_map.cend(),
                                [](auto &it) { return it.second->m_is_used; });
    auto subparser_used =
        std::any_of(m_subparser_used.cbegin(), m_subparser_used.cend(),
                    [](auto &it) { return it.second; });

    return m_is_parsed && (arg_used || subparser_used);
  }

  // Parameter packing
  // Call add_argument with variadic number of string arguments
  template <typename... Targs> Argument &add_argument(Targs... f_args) {
    using array_of_sv = std::array<std::string_view, sizeof...(Targs)>;
    auto argument =
        m_optional_arguments.emplace(std::cend(m_optional_arguments),
                                     m_prefix_chars, array_of_sv{f_args...});

    if (!argument->m_is_optional) {
      m_positional_arguments.splice(std::cend(m_positional_arguments),
                                    m_optional_arguments, argument);
    }
    argument->set_usage_newline_counter(m_usage_newline_counter);
    argument->set_group_idx(m_group_names.size());

    index_argument(argument);
    return *argument;
  }

  class MutuallyExclusiveGroup {
    friend class ArgumentParser;

  public:
    MutuallyExclusiveGroup() = delete;

    explicit MutuallyExclusiveGroup(ArgumentParser &parent,
                                    bool required = false)
        : m_parent(parent), m_required(required), m_elements({}) {}

    MutuallyExclusiveGroup(const MutuallyExclusiveGroup &other) = delete;
    MutuallyExclusiveGroup &
    operator=(const MutuallyExclusiveGroup &other) = delete;

    MutuallyExclusiveGroup(MutuallyExclusiveGroup &&other) noexcept
        : m_parent(other.m_parent), m_required(other.m_required),
          m_elements(std::move(other.m_elements)) {
      other.m_elements.clear();
    }

    template <typename... Targs> Argument &add_argument(Targs... f_args) {
      auto &argument = m_parent.add_argument(std::forward<Targs>(f_args)...);
      m_elements.push_back(&argument);
      argument.set_usage_newline_counter(m_parent.m_usage_newline_counter);
      argument.set_group_idx(m_parent.m_group_names.size());
      return argument;
    }

  private:
    ArgumentParser &m_parent;
    bool m_required{false};
    std::vector<Argument *> m_elements{};
  };

  MutuallyExclusiveGroup &add_mutually_exclusive_group(bool required = false) {
    m_mutually_exclusive_groups.emplace_back(*this, required);
    return m_mutually_exclusive_groups.back();
  }

  // Parameter packed add_parents method
  // Accepts a variadic number of ArgumentParser objects
  template <typename... Targs>
  ArgumentParser &add_parents(const Targs &... f_args) {
    for (const ArgumentParser &parent_parser : {std::ref(f_args)...}) {
      for (const auto &argument : parent_parser.m_positional_arguments) {
        auto it = m_positional_arguments.insert(
            std::cend(m_positional_arguments), argument);
        index_argument(it);
      }
      for (const auto &argument : parent_parser.m_optional_arguments) {
        auto it = m_optional_arguments.insert(std::cend(m_optional_arguments),
                                              argument);
        index_argument(it);
      }
    }
    return *this;
  }

  // Ask for the next optional arguments to be displayed on a separate
  // line in usage() output. Only effective if set_usage_max_line_width() is
  // also used.
  ArgumentParser &add_usage_newline() {
    ++m_usage_newline_counter;
    return *this;
  }

  // Ask for the next optional arguments to be displayed in a separate section
  // in usage() and help (<< *this) output.
  // For usage(), this is only effective if set_usage_max_line_width() is
  // also used.
  ArgumentParser &add_group(std::string group_name) {
    m_group_names.emplace_back(std::move(group_name));
    return *this;
  }

  ArgumentParser &add_description(std::string description) {
    m_description = std::move(description);
    return *this;
  }

  ArgumentParser &add_epilog(std::string epilog) {
    m_epilog = std::move(epilog);
    return *this;
  }

  // Add a un-documented/hidden alias for an argument.
  // Ideally we'd want this to be a method of Argument, but Argument
  // does not own its owing ArgumentParser.
  ArgumentParser &add_hidden_alias_for(Argument &arg, std::string_view alias) {
    for (auto it = m_optional_arguments.begin();
         it != m_optional_arguments.end(); ++it) {
      if (&(*it) == &arg) {
        m_argument_map.insert_or_assign(std::string(alias), it);
        return *this;
      }
    }
    throw std::logic_error(
        "Argument is not an optional argument of this parser");
  }

  /* Getter for arguments and subparsers.
   * @throws std::logic_error in case of an invalid argument or subparser name
   */
  template <typename T = Argument> T &at(std::string_view name) {
    if constexpr (std::is_same_v<T, Argument>) {
      return (*this)[name];
    } else {
      std::string str_name(name);
      auto subparser_it = m_subparser_map.find(str_name);
      if (subparser_it != m_subparser_map.end()) {
        return subparser_it->second->get();
      }
      throw std::logic_error("No such subparser: " + str_name);
    }
  }

  ArgumentParser &set_prefix_chars(std::string prefix_chars) {
    m_prefix_chars = std::move(prefix_chars);
    return *this;
  }

  ArgumentParser &set_assign_chars(std::string assign_chars) {
    m_assign_chars = std::move(assign_chars);
    return *this;
  }

  /* Call parse_args_internal - which does all the work
   * Then, validate the parsed arguments
   * This variant is used mainly for testing
   * @throws std::runtime_error in case of any invalid argument
   */
  void parse_args(const std::vector<std::string> &arguments) {
    parse_args_internal(arguments);
    // Check if all arguments are parsed
    for ([[maybe_unused]] const auto &[unused, argument] : m_argument_map) {
      argument->validate();
    }

    // Check each mutually exclusive group and make sure
    // there are no constraint violations
    for (const auto &group : m_mutually_exclusive_groups) {
      auto mutex_argument_used{false};
      Argument *mutex_argument_it{nullptr};
      for (Argument *arg : group.m_elements) {
        if (!mutex_argument_used && arg->m_is_used) {
          mutex_argument_used = true;
          mutex_argument_it = arg;
        } else if (mutex_argument_used && arg->m_is_used) {
          // Violation
          throw std::runtime_error("Argument '" + arg->get_usage_full() +
                                   "' not allowed with '" +
                                   mutex_argument_it->get_usage_full() + "'");
        }
      }

      if (!mutex_argument_used && group.m_required) {
        // at least one argument from the group is
        // required
        std::string argument_names{};
        std::size_t i = 0;
        std::size_t size = group.m_elements.size();
        for (Argument *arg : group.m_elements) {
          if (i + 1 == size) {
            // last
            argument_names += "'" + arg->get_usage_full() + "' ";
          } else {
            argument_names += "'" + arg->get_usage_full() + "' or ";
          }
          i += 1;
        }
        throw std::runtime_error("One of the arguments " + argument_names +
                                 "is required");
      }
    }
  }

  /* Call parse_known_args_internal - which does all the work
   * Then, validate the parsed arguments
   * This variant is used mainly for testing
   * @throws std::runtime_error in case of any invalid argument
   */
  std::vector<std::string>
  parse_known_args(const std::vector<std::string> &arguments) {
    auto unknown_arguments = parse_known_args_internal(arguments);
    // Check if all arguments are parsed
    for ([[maybe_unused]] const auto &[unused, argument] : m_argument_map) {
      argument->validate();
    }
    return unknown_arguments;
  }

  /* Main entry point for parsing command-line arguments using this
   * ArgumentParser
   * @throws std::runtime_error in case of any invalid argument
   */
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
  void parse_args(int argc, const char *const argv[]) {
    parse_args({argv, argv + argc});
  }

  /* Main entry point for parsing command-line arguments using this
   * ArgumentParser
   * @throws std::runtime_error in case of any invalid argument
   */
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
  auto parse_known_args(int argc, const char *const argv[]) {
    return parse_known_args({argv, argv + argc});
  }

  /* Getter for options with default values.
   * @throws std::logic_error if parse_args() has not been previously called
   * @throws std::logic_error if there is no such option
   * @throws std::logic_error if the option has no value
   * @throws std::bad_any_cast if the option is not of type T
   */
  template <typename T = std::string> T get(std::string_view arg_name) const {
    if (!m_is_parsed) {
      throw std::logic_error("Nothing parsed, no arguments are available.");
    }
    return (*this)[arg_name].get<T>();
  }

  /* Getter for options without default values.
   * @pre The option has no default value.
   * @throws std::logic_error if there is no such option
   * @throws std::bad_any_cast if the option is not of type T
   */
  template <typename T = std::string>
  auto present(std::string_view arg_name) const -> std::optional<T> {
    return (*this)[arg_name].present<T>();
  }

  /* Getter that returns true for user-supplied options. Returns false if not
   * user-supplied, even with a default value.
   */
  auto is_used(std::string_view arg_name) const {
    return (*this)[arg_name].m_is_used;
  }

  /* Getter that returns true if a subcommand is used.
   */
  auto is_subcommand_used(std::string_view subcommand_name) const {
    return m_subparser_used.at(std::string(subcommand_name));
  }

  /* Getter that returns true if a subcommand is used.
   */
  auto is_subcommand_used(const ArgumentParser &subparser) const {
    return is_subcommand_used(subparser.m_program_name);
  }

  /* Indexing operator. Return a reference to an Argument object
   * Used in conjunction with Argument.operator== e.g., parser["foo"] == true
   * @throws std::logic_error in case of an invalid argument name
   */
  Argument &operator[](std::string_view arg_name) const {
    std::string name(arg_name);
    auto it = m_argument_map.find(name);
    if (it != m_argument_map.end()) {
      return *(it->second);
    }
    if (!is_valid_prefix_char(arg_name.front())) {
      const auto legal_prefix_char = get_any_valid_prefix_char();
      const auto prefix = std::string(1, legal_prefix_char);

      // "-" + arg_name
      name = prefix + name;
      it = m_argument_map.find(name);
      if (it != m_argument_map.end()) {
        return *(it->second);
      }
      // "--" + arg_name
      name = prefix + name;
      it = m_argument_map.find(name);
      if (it != m_argument_map.end()) {
        return *(it->second);
      }
    }
    throw std::logic_error("No such argument: " + std::string(arg_name));
  }

  // Print help message
  friend auto operator<<(std::ostream &stream, const ArgumentParser &parser)
      -> std::ostream & {
    stream.setf(std::ios_base::left);

    auto longest_arg_length = parser.get_length_of_longest_argument();

    stream << parser.usage() << "\n\n";

    if (!parser.m_description.empty()) {
      stream << parser.m_description << "\n\n";
    }

    const bool has_visible_positional_args = std::find_if(
      parser.m_positional_arguments.begin(),
      parser.m_positional_arguments.end(),
      [](const auto &argument) {
      return !argument.m_is_hidden; }) !=
      parser.m_positional_arguments.end();
    if (has_visible_positional_args) {
      stream << "Positional arguments:\n";
    }

    for (const auto &argument : parser.m_positional_arguments) {
      if (!argument.m_is_hidden) {
        stream.width(static_cast<std::streamsize>(longest_arg_length));
        stream << argument;
      }
    }

    if (!parser.m_optional_arguments.empty()) {
      stream << (!has_visible_positional_args ? "" : "\n")
             << "Optional arguments:\n";
    }

    for (const auto &argument : parser.m_optional_arguments) {
      if (argument.m_group_idx == 0 && !argument.m_is_hidden) {
        stream.width(static_cast<std::streamsize>(longest_arg_length));
        stream << argument;
      }
    }

    for (size_t i_group = 0; i_group < parser.m_group_names.size(); ++i_group) {
      stream << "\n" << parser.m_group_names[i_group] << " (detailed usage):\n";
      for (const auto &argument : parser.m_optional_arguments) {
        if (argument.m_group_idx == i_group + 1 && !argument.m_is_hidden) {
          stream.width(static_cast<std::streamsize>(longest_arg_length));
          stream << argument;
        }
      }
    }

    bool has_visible_subcommands = std::any_of(
        parser.m_subparser_map.begin(), parser.m_subparser_map.end(),
        [](auto &p) { return !p.second->get().m_suppress; });

    if (has_visible_subcommands) {
      stream << (parser.m_positional_arguments.empty()
                     ? (parser.m_optional_arguments.empty() ? "" : "\n")
                     : "\n")
             << "Subcommands:\n";
      for (const auto &[command, subparser] : parser.m_subparser_map) {
        if (subparser->get().m_suppress) {
          continue;
        }

        stream << std::setw(2) << " ";
        if (longest_arg_length >= 2) {
          stream << std::setw(static_cast<int>(longest_arg_length - 2))
                 << command;
        }
        stream << " " << subparser->get().m_description << "\n";
      }
    }

    if (!parser.m_epilog.empty()) {
      stream << '\n';
      stream << parser.m_epilog << "\n\n";
    }

    return stream;
  }

  // Format help message
  auto help() const -> std::stringstream {
    std::stringstream out;
    out << *this;
    return out;
  }

  // Sets the maximum width for a line of the Usage message
  ArgumentParser &set_usage_max_line_width(size_t w) {
    this->m_usage_max_line_width = w;
    return *this;
  }

  // Asks to display arguments of mutually exclusive group on separate lines in
  // the Usage message
  ArgumentParser &set_usage_break_on_mutex() {
    this->m_usage_break_on_mutex = true;
    return *this;
  }

  // Format usage part of help only
  auto usage() const -> std::string {
    std::stringstream stream;

    std::string curline("Usage: ");
    curline += this->m_program_name;
    const bool multiline_usage =
        this->m_usage_max_line_width < std::numeric_limits<std::size_t>::max();
    const size_t indent_size = curline.size();

    const auto deal_with_options_of_group = [&](std::size_t group_idx) {
      bool found_options = false;
      // Add any options inline here
      const MutuallyExclusiveGroup *cur_mutex = nullptr;
      int usage_newline_counter = -1;
      for (const auto &argument : this->m_optional_arguments) {
        if (argument.m_is_hidden) {
          continue;
        }
        if (multiline_usage) {
          if (argument.m_group_idx != group_idx) {
            continue;
          }
          if (usage_newline_counter != argument.m_usage_newline_counter) {
            if (usage_newline_counter >= 0) {
              if (curline.size() > indent_size) {
                stream << curline << std::endl;
                curline = std::string(indent_size, ' ');
              }
            }
            usage_newline_counter = argument.m_usage_newline_counter;
          }
        }
        found_options = true;
        const std::string arg_inline_usage = argument.get_inline_usage();
        const MutuallyExclusiveGroup *arg_mutex =
            get_belonging_mutex(&argument);
        if ((cur_mutex != nullptr) && (arg_mutex == nullptr)) {
          curline += ']';
          if (this->m_usage_break_on_mutex) {
            stream << curline << std::endl;
            curline = std::string(indent_size, ' ');
          }
        } else if ((cur_mutex == nullptr) && (arg_mutex != nullptr)) {
          if ((this->m_usage_break_on_mutex && curline.size() > indent_size) ||
              curline.size() + 3 + arg_inline_usage.size() >
                  this->m_usage_max_line_width) {
            stream << curline << std::endl;
            curline = std::string(indent_size, ' ');
          }
          curline += " [";
        } else if ((cur_mutex != nullptr) && (arg_mutex != nullptr)) {
          if (cur_mutex != arg_mutex) {
            curline += ']';
            if (this->m_usage_break_on_mutex ||
                curline.size() + 3 + arg_inline_usage.size() >
                    this->m_usage_max_line_width) {
              stream << curline << std::endl;
              curline = std::string(indent_size, ' ');
            }
            curline += " [";
          } else {
            curline += '|';
          }
        }
        cur_mutex = arg_mutex;
        if (curline.size() + 1 + arg_inline_usage.size() >
            this->m_usage_max_line_width) {
          stream << curline << std::endl;
          curline = std::string(indent_size, ' ');
          curline += " ";
        } else if (cur_mutex == nullptr) {
          curline += " ";
        }
        curline += arg_inline_usage;
      }
      if (cur_mutex != nullptr) {
        curline += ']';
      }
      return found_options;
    };

    const bool found_options = deal_with_options_of_group(0);

    if (found_options && multiline_usage &&
        !this->m_positional_arguments.empty()) {
      stream << curline << std::endl;
      curline = std::string(indent_size, ' ');
    }
    // Put positional arguments after the optionals
    for (const auto &argument : this->m_positional_arguments) {
      if (argument.m_is_hidden) {
        continue;
      }
      const std::string pos_arg = !argument.m_metavar.empty()
                                      ? argument.m_metavar
                                      : argument.m_names.front();
      if (curline.size() + 1 + pos_arg.size() > this->m_usage_max_line_width) {
        stream << curline << std::endl;
        curline = std::string(indent_size, ' ');
      }
      curline += " ";
      if (argument.m_num_args_range.get_min() == 0 &&
          !argument.m_num_args_range.is_right_bounded()) {
        curline += "[";
        curline += pos_arg;
        curline += "]...";
      } else if (argument.m_num_args_range.get_min() == 1 &&
                 !argument.m_num_args_range.is_right_bounded()) {
        curline += pos_arg;
        curline += "...";
      } else {
        curline += pos_arg;
      }
    }

    if (multiline_usage) {
      // Display options of other groups
      for (std::size_t i = 0; i < m_group_names.size(); ++i) {
        stream << curline << std::endl << std::endl;
        stream << m_group_names[i] << ":" << std::endl;
        curline = std::string(indent_size, ' ');
        deal_with_options_of_group(i + 1);
      }
    }

    stream << curline;

    // Put subcommands after positional arguments
    if (!m_subparser_map.empty()) {
      stream << " {";
      std::size_t i{0};
      for (const auto &[command, subparser] : m_subparser_map) {
        if (subparser->get().m_suppress) {
          continue;
        }

        if (i == 0) {
          stream << command;
        } else {
          stream << "," << command;
        }
        ++i;
      }
      stream << "}";
    }

    return stream.str();
  }

  // Printing the one and only help message
  // I've stuck with a simple message format, nothing fancy.
  [[deprecated("Use cout << program; instead.  See also help().")]] std::string
  print_help() const {
    auto out = help();
    std::cout << out.rdbuf();
    return out.str();
  }

  void add_subparser(ArgumentParser &parser) {
    parser.m_parser_path = m_program_name + " " + parser.m_program_name;
    auto it = m_subparsers.emplace(std::cend(m_subparsers), parser);
    m_subparser_map.insert_or_assign(parser.m_program_name, it);
    m_subparser_used.insert_or_assign(parser.m_program_name, false);
  }

  void set_suppress(bool suppress) { m_suppress = suppress; }

protected:
  const MutuallyExclusiveGroup *get_belonging_mutex(const Argument *arg) const {
    for (const auto &mutex : m_mutually_exclusive_groups) {
      if (std::find(mutex.m_elements.begin(), mutex.m_elements.end(), arg) !=
          mutex.m_elements.end()) {
        return &mutex;
      }
    }
    return nullptr;
  }

  bool is_valid_prefix_char(char c) const {
    return m_prefix_chars.find(c) != std::string::npos;
  }

  char get_any_valid_prefix_char() const { return m_prefix_chars[0]; }

  /*
   * Pre-process this argument list. Anything starting with "--", that
   * contains an =, where the prefix before the = has an entry in the
   * options table, should be split.
   */
  std::vector<std::string>
  preprocess_arguments(const std::vector<std::string> &raw_arguments) const {
    std::vector<std::string> arguments{};
    for (const auto &arg : raw_arguments) {

      const auto argument_starts_with_prefix_chars =
          [this](const std::string &a) -> bool {
        if (!a.empty()) {

          const auto legal_prefix = [this](char c) -> bool {
            return m_prefix_chars.find(c) != std::string::npos;
          };

          // Windows-style
          // if '/' is a legal prefix char
          // then allow single '/' followed by argument name, followed by an
          // assign char, e.g., ':' e.g., 'test.exe /A:Foo'
          const auto windows_style = legal_prefix('/');

          if (windows_style) {
            if (legal_prefix(a[0])) {
              return true;
            }
          } else {
            // Slash '/' is not a legal prefix char
            // For all other characters, only support long arguments
            // i.e., the argument must start with 2 prefix chars, e.g,
            // '--foo' e,g, './test --foo=Bar -DARG=yes'
            if (a.size() > 1) {
              return (legal_prefix(a[0]) && legal_prefix(a[1]));
            }
          }
        }
        return false;
      };

      // Check that:
      // - We don't have an argument named exactly this
      // - The argument starts with a prefix char, e.g., "--"
      // - The argument contains an assign char, e.g., "="
      auto assign_char_pos = arg.find_first_of(m_assign_chars);

      if (m_argument_map.find(arg) == m_argument_map.end() &&
          argument_starts_with_prefix_chars(arg) &&
          assign_char_pos != std::string::npos) {
        // Get the name of the potential option, and check it exists
        std::string opt_name = arg.substr(0, assign_char_pos);
        if (m_argument_map.find(opt_name) != m_argument_map.end()) {
          // This is the name of an option! Split it into two parts
          arguments.push_back(std::move(opt_name));
          arguments.push_back(arg.substr(assign_char_pos + 1));
          continue;
        }
      }
      // If we've fallen through to here, then it's a standard argument
      arguments.push_back(arg);
    }
    return arguments;
  }

  /*
   * @throws std::runtime_error in case of any invalid argument
   */
  void parse_args_internal(const std::vector<std::string> &raw_arguments) {
    auto arguments = preprocess_arguments(raw_arguments);
    if (m_program_name.empty() && !arguments.empty()) {
      m_program_name = arguments.front();
    }
    auto end = std::end(arguments);
    auto positional_argument_it = std::begin(m_positional_arguments);
    for (auto it = std::next(std::begin(arguments)); it != end;) {
      const auto &current_argument = *it;
      if (Argument::is_positional(current_argument, m_prefix_chars)) {
        if (positional_argument_it == std::end(m_positional_arguments)) {

          // Check sub-parsers
          auto subparser_it = m_subparser_map.find(current_argument);
          if (subparser_it != m_subparser_map.end()) {

            // build list of remaining args
            const auto unprocessed_arguments =
                std::vector<std::string>(it, end);

            // invoke subparser
            m_is_parsed = true;
            m_subparser_used[current_argument] = true;
            return subparser_it->second->get().parse_args(
                unprocessed_arguments);
          }

          if (m_positional_arguments.empty()) {

            // Ask the user if they argument they provided was a typo
            // for some sub-parser,
            // e.g., user provided `git totes` instead of `git notes`
            if (!m_subparser_map.empty()) {
              throw std::runtime_error(
                  "Failed to parse '" + current_argument + "', did you mean '" +
                  std::string{details::get_most_similar_string(
                      m_subparser_map, current_argument)} +
                  "'");
            }

            // Ask the user if they meant to use a specific optional argument
            if (!m_optional_arguments.empty()) {
              for (const auto &opt : m_optional_arguments) {
                if (!opt.m_implicit_value.has_value()) {
                  // not a flag, requires a value
                  if (!opt.m_is_used) {
                    throw std::runtime_error(
                        "Zero positional arguments expected, did you mean " +
                        opt.get_usage_full());
                  }
                }
              }

              throw std::runtime_error("Zero positional arguments expected");
            } else {
              throw std::runtime_error("Zero positional arguments expected");
            }
          } else {
            throw std::runtime_error("Maximum number of positional arguments "
                                     "exceeded, failed to parse '" +
                                     current_argument + "'");
          }
        }
        auto argument = positional_argument_it++;

        // Deal with the situation of <positional_arg1>... <positional_arg2>
        if (argument->m_num_args_range.get_min() == 1 &&
            argument->m_num_args_range.get_max() == (std::numeric_limits<std::size_t>::max)() &&
            positional_argument_it != std::end(m_positional_arguments) &&
            std::next(positional_argument_it) == std::end(m_positional_arguments) &&
            positional_argument_it->m_num_args_range.get_min() == 1 &&
            positional_argument_it->m_num_args_range.get_max() == 1 ) {
          if (std::next(it) != end) {
            positional_argument_it->consume(std::prev(end), end);
            end = std::prev(end);
          } else {
            throw std::runtime_error("Missing " + positional_argument_it->m_names.front());
          }
        }

        it = argument->consume(it, end);
        continue;
      }

      auto arg_map_it = m_argument_map.find(current_argument);
      if (arg_map_it != m_argument_map.end()) {
        auto argument = arg_map_it->second;
        it = argument->consume(std::next(it), end, arg_map_it->first);
      } else if (const auto &compound_arg = current_argument;
                 compound_arg.size() > 1 &&
                 is_valid_prefix_char(compound_arg[0]) &&
                 !is_valid_prefix_char(compound_arg[1])) {
        ++it;
        for (std::size_t j = 1; j < compound_arg.size(); j++) {
          auto hypothetical_arg = std::string{'-', compound_arg[j]};
          auto arg_map_it2 = m_argument_map.find(hypothetical_arg);
          if (arg_map_it2 != m_argument_map.end()) {
            auto argument = arg_map_it2->second;
            it = argument->consume(it, end, arg_map_it2->first);
          } else {
            throw std::runtime_error("Unknown argument: " + current_argument);
          }
        }
      } else {
        throw std::runtime_error("Unknown argument: " + current_argument);
      }
    }
    m_is_parsed = true;
  }

  /*
   * Like parse_args_internal but collects unused args into a vector<string>
   */
  std::vector<std::string>
  parse_known_args_internal(const std::vector<std::string> &raw_arguments) {
    auto arguments = preprocess_arguments(raw_arguments);

    std::vector<std::string> unknown_arguments{};

    if (m_program_name.empty() && !arguments.empty()) {
      m_program_name = arguments.front();
    }
    auto end = std::end(arguments);
    auto positional_argument_it = std::begin(m_positional_arguments);
    for (auto it = std::next(std::begin(arguments)); it != end;) {
      const auto &current_argument = *it;
      if (Argument::is_positional(current_argument, m_prefix_chars)) {
        if (positional_argument_it == std::end(m_positional_arguments)) {

          // Check sub-parsers
          auto subparser_it = m_subparser_map.find(current_argument);
          if (subparser_it != m_subparser_map.end()) {

            // build list of remaining args
            const auto unprocessed_arguments =
                std::vector<std::string>(it, end);

            // invoke subparser
            m_is_parsed = true;
            m_subparser_used[current_argument] = true;
            return subparser_it->second->get().parse_known_args_internal(
                unprocessed_arguments);
          }

          // save current argument as unknown and go to next argument
          unknown_arguments.push_back(current_argument);
          ++it;
        } else {
          // current argument is the value of a positional argument
          // consume it
          auto argument = positional_argument_it++;
          it = argument->consume(it, end);
        }
        continue;
      }

      auto arg_map_it = m_argument_map.find(current_argument);
      if (arg_map_it != m_argument_map.end()) {
        auto argument = arg_map_it->second;
        it = argument->consume(std::next(it), end, arg_map_it->first);
      } else if (const auto &compound_arg = current_argument;
                 compound_arg.size() > 1 &&
                 is_valid_prefix_char(compound_arg[0]) &&
                 !is_valid_prefix_char(compound_arg[1])) {
        ++it;
        for (std::size_t j = 1; j < compound_arg.size(); j++) {
          auto hypothetical_arg = std::string{'-', compound_arg[j]};
          auto arg_map_it2 = m_argument_map.find(hypothetical_arg);
          if (arg_map_it2 != m_argument_map.end()) {
            auto argument = arg_map_it2->second;
            it = argument->consume(it, end, arg_map_it2->first);
          } else {
            unknown_arguments.push_back(current_argument);
            break;
          }
        }
      } else {
        // current argument is an optional-like argument that is unknown
        // save it and move to next argument
        unknown_arguments.push_back(current_argument);
        ++it;
      }
    }
    m_is_parsed = true;
    return unknown_arguments;
  }

  // Used by print_help.
  std::size_t get_length_of_longest_argument() const {
    if (m_argument_map.empty()) {
      return 0;
    }
    std::size_t max_size = 0;
    for ([[maybe_unused]] const auto &[unused, argument] : m_argument_map) {
      max_size =
          std::max<std::size_t>(max_size, argument->get_arguments_length());
    }
    for ([[maybe_unused]] const auto &[command, unused] : m_subparser_map) {
      max_size = std::max<std::size_t>(max_size, command.size());
    }
    return max_size;
  }

  using argument_it = std::list<Argument>::iterator;
  using mutex_group_it = std::vector<MutuallyExclusiveGroup>::iterator;
  using argument_parser_it =
      std::list<std::reference_wrapper<ArgumentParser>>::iterator;

  void index_argument(argument_it it) {
    for (const auto &name : std::as_const(it->m_names)) {
      m_argument_map.insert_or_assign(name, it);
    }
  }

  std::string m_program_name;
  std::string m_version;
  std::string m_description;
  std::string m_epilog;
  bool m_exit_on_default_arguments = true;
  std::string m_prefix_chars{"-"};
  std::string m_assign_chars{"="};
  bool m_is_parsed = false;
  std::list<Argument> m_positional_arguments;
  std::list<Argument> m_optional_arguments;
  std::map<std::string, argument_it> m_argument_map;
  std::string m_parser_path;
  std::list<std::reference_wrapper<ArgumentParser>> m_subparsers;
  std::map<std::string, argument_parser_it> m_subparser_map;
  std::map<std::string, bool> m_subparser_used;
  std::vector<MutuallyExclusiveGroup> m_mutually_exclusive_groups;
  bool m_suppress = false;
  std::size_t m_usage_max_line_width = std::numeric_limits<std::size_t>::max();
  bool m_usage_break_on_mutex = false;
  int m_usage_newline_counter = 0;
  std::vector<std::string> m_group_names;
};

} // namespace argparse
