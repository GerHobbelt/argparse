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
#include <algorithm>
#include <any>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <functional>
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

#ifndef ARGPARSE_NO_SCAN
#include <charconv>
#endif

namespace argparse {

namespace details { // namespace for helper methods

template <typename T, typename = void>
struct HasContainerTraits : std::false_type {};

template <> struct HasContainerTraits<std::string> : std::false_type {};

template <typename T>
struct HasContainerTraits<
    T, std::void_t<typename T::value_type, decltype(std::declval<T>().begin()),
                   decltype(std::declval<T>().end()),
                   decltype(std::declval<T>().size())>> : std::true_type {};

template <typename T>
static constexpr bool IsContainer = HasContainerTraits<T>::value;

template <typename T, typename = void>
struct HasStreamableTraits : std::false_type {};

template <typename T>
struct HasStreamableTraits<
    T,
    std::void_t<decltype(std::declval<std::ostream &>() << std::declval<T>())>>
    : std::true_type {};

template <typename T>
static constexpr bool IsStreamable = HasStreamableTraits<T>::value;

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
          std::next(val.begin(),
                    std::min<std::size_t>(size, repr_max_container_size) - 1),
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

constexpr int radix_8 = 8;
constexpr int radix_10 = 10;
constexpr int radix_16 = 16;

template <typename T>
constexpr bool standard_integer =
    standard_signed_integer<T> || standard_unsigned_integer<T>;

template <class F, class Tuple, class Extra, std::size_t... I>
constexpr decltype(auto) apply_plus_one_impl(F &&f, Tuple &&t, Extra &&x,
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
  scientific = 0x1,
  fixed = 0x2,
  hex = 0x4,
  general = fixed | scientific
};

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

#ifndef ARGPARSE_NO_SCAN

template <class T, auto Param>
inline auto do_from_chars(std::string_view s) -> T {
  T x;
  auto [first, last] = pointer_range(s);
  auto [ptr, ec] = std::from_chars(first, last, x, Param);
  if (ec == std::errc()) {
    if (ptr == last) {
      return x;
    }
    throw std::invalid_argument{"pattern does not match to the end"};
  }
  if (ec == std::errc::invalid_argument) {
    throw std::invalid_argument{"pattern not found"};
  }
  if (ec == std::errc::result_out_of_range) {
    throw std::range_error{"not representable"};
  }
  return x; // unreachable
}

template <class T, auto Param = 0> struct parse_number {
  auto operator()(std::string_view s) -> T {
    return do_from_chars<T, Param>(s);
  }
};

template <class T> struct parse_number<T, radix_16> {
  auto operator()(std::string_view s) -> T {
    if (auto [ok, rest] = consume_hex_prefix(s); ok) {
      return do_from_chars<T, radix_16>(rest);
    }
    throw std::invalid_argument{"pattern not found"};
  }
};

template <class T> struct parse_number<T> {
  auto operator()(std::string_view s) -> T {
    auto [ok, rest] = consume_hex_prefix(s);
    if (ok) {
      return do_from_chars<T, radix_16>(rest);
    }
    if (starts_with("0"sv, s)) {
      return do_from_chars<T, radix_8>(rest);
    }
    return do_from_chars<T, radix_10>(rest);
  }
};
#endif

namespace {

template <class T> inline const auto generic_strtod = nullptr;
template <> inline const auto generic_strtod<float> = strtof;
template <> inline const auto generic_strtod<double> = strtod;
template <> inline const auto generic_strtod<long double> = strtold;

} // namespace

template <class T> inline auto do_strtod(std::string const &s) -> T {
  if (isspace(static_cast<unsigned char>(s[0])) || s[0] == '+') {
    throw std::invalid_argument{"pattern not found"};
  }

  auto [first, last] = pointer_range(s);
  char *ptr;

  errno = 0;
  auto x = generic_strtod<T>(first, &ptr);
  if (errno == 0) {
    if (ptr == last) {
      return x;
    }
    throw std::invalid_argument{"pattern does not match to the end"};
  }
  if (errno == ERANGE) {
    throw std::range_error{"not representable"};
  }
  return x; // unreachable
}

#ifndef ARGPARSE_NO_SCAN

template <class T> struct parse_number<T, chars_format::general> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); r.is_hexadecimal) {
      throw std::invalid_argument{
          "chars_format::general does not parse hexfloat"};
    }

    return do_strtod<T>(s);
  }
};

template <class T> struct parse_number<T, chars_format::hex> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); !r.is_hexadecimal) {
      throw std::invalid_argument{"chars_format::hex parses hexfloat"};
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
    if (s.find_first_of("eE") == std::string::npos) {
      throw std::invalid_argument{
          "chars_format::scientific requires exponent part"};
    }

    return do_strtod<T>(s);
  }
};

template <class T> struct parse_number<T, chars_format::fixed> {
  auto operator()(std::string const &s) -> T {
    if (auto r = consume_hex_prefix(s); r.is_hexadecimal) {
      throw std::invalid_argument{
          "chars_format::fixed does not parse hexfloat"};
    }
    if (s.find_first_of("eE") != std::string::npos) {
      throw std::invalid_argument{
          "chars_format::fixed does not parse exponent part"};
    }

    return do_strtod<T>(s);
  }
};
#endif // #ifndef ARGPARSE_NO_SCAN

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

inline std::size_t get_screen_width() {
  char *c = std::getenv("COLUMNS");
  if (c == nullptr) {
    return 80;
  }
  return std::stoi(c);
}

} // namespace details

enum class nargs_pattern {
  optional,
  any,
  at_least_one
};

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
class ArgumentParserMutuallyExclusiveGroup;

class Argument {
  friend class ArgumentParser;
  friend class ArgumentParserMutuallyExclusiveGroup;

  friend auto operator<<(std::ostream &stream, const ArgumentParser &parser)
      -> std::ostream &;

  template <std::size_t N, std::size_t... I>
  explicit Argument(std::array<std::string_view, N> &&a,
                    std::index_sequence<I...> /*unused*/)
      : m_is_optional((is_optional(a[I]) || ...)), m_is_required(false),
        m_is_repeatable(false), m_is_used(false), m_in_group(false) {
    ((void)m_names.emplace_back(a[I]), ...);
    std::sort(
        m_names.begin(), m_names.end(), [](const auto &lhs, const auto &rhs) {
          return lhs.size() == rhs.size() ? lhs < rhs : lhs.size() < rhs.size();
        });
  }

public:
  template <std::size_t N>
  explicit Argument(std::array<std::string_view, N> &&a)
      : Argument(std::move(a), std::make_index_sequence<N>{}) {}

  Argument &help(std::string help_text) {
    m_help = std::move(help_text);
    return *this;
  }

  Argument &metavar(std::string metavar) {
    m_metavar = std::move(metavar);
    return *this;
  }

  template <typename T> Argument &default_value(T &&value) {
    m_default_value_repr = details::repr(value);
    m_default_value = std::forward<T>(value);
    return *this;
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

  template <class F, class... Args>
  auto action(F &&callable, Args &&...bound_args)
      -> std::enable_if_t<std::is_invocable_v<F, Args..., std::string const>,
                          Argument &> {
    using action_type = std::conditional_t<
        std::is_void_v<std::invoke_result_t<F, Args..., std::string const>>,
        void_action, valued_action>;
    if constexpr (sizeof...(Args) == 0) {
      m_action.emplace<action_type>(std::forward<F>(callable));
    } else {
      m_action.emplace<action_type>(
          [f = std::forward<F>(callable),
           tup = std::make_tuple(std::forward<Args>(bound_args)...)](
              std::string const &opt) mutable {
            return details::apply_plus_one(f, tup, opt);
          });
    }
    return *this;
  }

  auto &append() {
    m_is_repeatable = true;
    return *this;
  }

#ifndef ARGPARSE_NO_SCAN

  template <char Shape, typename T>
  auto scan() -> std::enable_if_t<std::is_arithmetic_v<T>, Argument &> {
    static_assert(!(std::is_const_v<T> || std::is_volatile_v<T>),
                  "T should not be cv-qualified");
    auto is_one_of = [](char c, auto... x) constexpr {
      return ((c == x) || ...);
    };

    if constexpr (is_one_of(Shape, 'd') && details::standard_integer<T>) {
      action(details::parse_number<T, details::radix_10>());
    } else if constexpr (is_one_of(Shape, 'i') && details::standard_integer<T>) {
      action(details::parse_number<T>());
    } else if constexpr (is_one_of(Shape, 'u') &&
                         details::standard_unsigned_integer<T>) {
      action(details::parse_number<T, details::radix_10>());
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
#endif // #ifndef ARGPARSE_NO_SCAN

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
      m_num_args_range = NArgsRange{0, std::numeric_limits<std::size_t>::max()};
      break;
    case nargs_pattern::at_least_one:
      m_num_args_range = NArgsRange{1, std::numeric_limits<std::size_t>::max()};
      break;
    }
    return *this;
  }

  Argument &remaining() {
    m_accepts_optional_like_value = true;
    return nargs(nargs_pattern::any);
  }

  template <typename Iterator>
  Iterator consume(Iterator start, Iterator end,
                   std::string_view used_name = {}) {
    if (!m_is_repeatable && m_is_used) {
      throw std::runtime_error("Duplicate argument");
    }
    m_is_used = true;
    m_used_name = used_name;

    const auto num_args_max = m_num_args_range.get_max();
    const auto num_args_min = m_num_args_range.get_min();
    std::size_t dist = 0;
    if (num_args_max == 0) {
      m_values.emplace_back(m_implicit_value);
      std::visit([](const auto &f) { f({}); }, m_action);
      return start;
    }
    if ((dist = static_cast<std::size_t>(std::distance(start, end))) >= num_args_min) {
      if (num_args_max < dist) {
        end = std::next(start, num_args_max);
      }
      if (!m_accepts_optional_like_value) {
        end = std::find_if(start, end, Argument::is_optional);
        dist = static_cast<std::size_t>(std::distance(start, end));
        if (dist < num_args_min) {
          throw std::runtime_error("Too few arguments");
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
              self.m_values.resize(std::distance(first, last));
            }
          }
        }

        Iterator first, last;
        Argument &self;
      };
      std::visit(ActionApply{start, end, *this}, m_action);
      return end;
    }
    if (m_default_value.has_value()) {
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
      if (!m_in_group && !m_num_args_range.contains(m_values.size()) && !m_default_value.has_value()) {
        throw_nargs_range_validation_error();
      }
    }
  }

  std::string get_inline_usage() const {
    std::stringstream usage;
    if (!m_is_required && !m_in_group) {
      usage << "[";
    }
    if (!m_is_optional) {
      if (!m_metavar.empty()) {
        usage << " " << m_metavar;
      } else {
        usage << " " << m_names[0];
      }
    } else {
      // Use the longest variant. Thankfully, these are pre-sorted
      usage << m_names.back();
      const std::string metavar = m_metavar.size() > 0 ? m_metavar : "VAR";
      if (m_num_args_range.get_max() > 0) {
        usage << " " << metavar;
        if (m_num_args_range.get_max() > 1) {
          usage << "...";
        }
      }
    }
    if (!m_is_required && !m_in_group) {
      usage << "]";
    }
    return usage.str();
  }

  std::size_t get_arguments_length() const {
    std::size_t names_size = std::accumulate(
        std::begin(m_names), std::end(m_names), std::size_t(0),
        [](const auto &sum, const auto &s) { return sum + s.size(); });
    if (is_positional(m_names.front())) {
      // A set metavar means this replaces the names
      if (!m_metavar.empty()) {
        // Indent and metavar
        return 2 + m_metavar.size();
      } else {
        // Indent and space-separated
        return 2 + names_size + (m_names.size() - 1);
      }
    }
    // Is an option - include both names _and_ metavar
    // size = text + (", " between names)
    std::size_t size = names_size + 2 * (m_names.size() - 1);
    if (m_metavar.size() > 0 && m_num_args_range == NArgsRange{1, 1}) {
      size += m_metavar.size() + 1;
    }
    return size + 2; // indent
  }

  friend std::ostream &operator<<(std::ostream &stream,
                                  const Argument &argument) {
    // Geometry for managing wrapping
    std::size_t screen_width = details::get_screen_width();
    // The stream is already set to the maximum argument name values width
    // 3 = indent between names and help (including token starter)
    std::size_t help_indent = stream.width() + 3;
    std::size_t width = help_indent;
    // std::cout << "Screen: " << screen_width << ", stream: " << stream.width() << std::endl;

    std::stringstream name_stream;
    name_stream << "  "; // indent
    if (argument.is_positional(argument.m_names.front())) {
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
      if (argument.m_metavar.size() > 0 &&
          argument.m_num_args_range == NArgsRange{1, 1}) {
        name_stream << " " << argument.m_metavar;
      }
    }
    stream << name_stream.str() << "   ";

    // Now, we can stream the help message. Let's wrap this over the screen width
    std::istringstream help(argument.m_help);
    std::string token;
    while (std::getline(help, token, ' ')) {
      if (token.size() + width + 1 > screen_width) {
        stream << "\n" << std::string(help_indent, ' ' );
        width = help_indent;
      }
      stream << " " << token;
      width += token.size() + 1;
    }

    // stream << argument.m_help;

    if (argument.m_default_value.has_value() &&
        argument.m_num_args_range != NArgsRange{0, 0}) {
      std::string default_msg;
      if (!argument.m_help.empty()) {
        default_msg = " ";
      }
      default_msg += "[default: " + argument.m_default_value_repr + "]";
      if (default_msg.size() + width > screen_width) {
        stream << "\n" << std::string(help_indent, ' ' );
        width = help_indent;
      }
      stream << default_msg;
    } else if (argument.m_is_required) {
      std::string required_msg;
      if (!argument.m_help.empty()) {
        required_msg = " ";
      }
      required_msg += "[required]";
      if (required_msg.size() + width > screen_width) {
        stream << "\n" << std::string(help_indent, ' ');
        width = help_indent;
      }
      stream << required_msg;
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
      auto lhs = get<T>();
      return std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs),
                        std::end(rhs), [](const auto &lhs, const auto &rhs) {
                          return lhs == rhs;
                        });
    }
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

    bool is_exact() const {
      return m_min == m_max;
    }

    bool is_right_bounded() const {
      return m_max < std::numeric_limits<std::size_t>::max();
    }

    std::size_t get_min() const {
      return m_min;
    }

    std::size_t get_max() const {
      return m_max;
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
    }
    if (m_num_args_range.is_exact()) {
      stream << m_num_args_range.get_min();
    } else if (m_num_args_range.is_right_bounded()) {
      stream << m_num_args_range.get_min() << " to " << m_num_args_range.get_max();
    } else {
      stream << m_num_args_range.get_min() << " or more";
    }
    stream << " argument(s) expected. " << m_values.size() << " provided.";
    throw std::runtime_error(stream.str());
  }

  void throw_required_arg_not_used_error() const {
    std::stringstream stream;
    stream << m_names[0] << ": required.";
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
    auto consume_digits = [=](std::string_view s) {
      // NOLINTNEXTLINE(readability-qualified-auto)
      auto it = std::find_if_not(std::begin(s), std::end(s), is_digit);
      return s.substr(it - std::begin(s));
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

  static bool is_optional(std::string_view name) {
    return !is_positional(name);
  }

  /*
   * positional:
   *    _empty_
   *    '-'
   *    '-' decimal-literal
   *    !'-' anything
   */
  static bool is_positional(std::string_view name) {
    switch (lookahead(name)) {
    case eof:
      return true;
    case '-': {
      name.remove_prefix(1);
      if (name.empty()) {
        return true;
      }
      return is_decimal_literal(name);
    }
    default:
      return true;
    }
  }

  /*
   * Get argument value given a type
   * @throws std::logic_error in case of incompatible types
   */
  template <typename T> auto get() const -> std::conditional_t<details::IsContainer<T>, T, const T&> {
    if (!m_values.empty()) {
      if constexpr (details::IsContainer<T>) {
        return any_cast_container<T>(m_values);
      } else {
        return *std::any_cast<T>(&m_values.front());
      }
    }
    if (m_default_value.has_value()) {
      return *std::any_cast<T>(&m_default_value);
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
        [](const auto &value) { return *std::any_cast<ValueType>(&value); });
    return result;
  }

  std::vector<std::string> m_names;
  std::string_view m_used_name;
  std::string m_help;
  std::string m_metavar;
  std::any m_default_value;
  std::string m_default_value_repr;
  std::any m_implicit_value;
  using valued_action = std::function<std::any(const std::string &)>;
  using void_action = std::function<void(const std::string &)>;
  std::variant<valued_action, void_action> m_action{
      std::in_place_type<valued_action>,
      [](const std::string &value) { return value; }};
  std::vector<std::any> m_values;
  NArgsRange m_num_args_range {1, 1};
  bool m_accepts_optional_like_value = false;
  bool m_is_optional : true;
  bool m_is_required : true;
  bool m_is_repeatable : true;
  bool m_is_used : true; // True if the optional argument is used by user
  bool m_in_group : true; // Controls e.g. visibility in usage vs group
};

class ArgumentParserMutuallyExclusiveGroup {
  friend class ArgumentParser;
  friend auto operator<<(std::ostream &stream, const ArgumentParser &parser)
      -> std::ostream &;

    ArgumentParserMutuallyExclusiveGroup(ArgumentParserMutuallyExclusiveGroup&) = delete;
public:
    ArgumentParserMutuallyExclusiveGroup(ArgumentParser &parser, bool required) : m_parser(parser), m_required(required) {}

  std::string get_inline_usage() const {
    std::stringstream usage;
    usage << (m_required ? "( " : "[ ");

    // Make an internal list of all the options, then join in
    std::vector<std::string> options;

    for (const Argument &arg : m_arguments)  {
      options.push_back((arg).get_inline_usage());
    }
    usage << details::join(options.begin(), options.end(), " | ");
    usage << (m_required ? " )" : " ]");

    return usage.str();
  }

  auto validate() const -> bool {
    // Validate that
    // - if required, we have one argument
    // - we don't have more than one argument
    std::optional<std::reference_wrapper<Argument>> used;
    for (Argument &arg : m_arguments) {
      if (arg.m_is_used) {
        if (used) {
          throw std::runtime_error("Got mutually exclusive argument error: Both '" + used.value().get().m_names.back() + "' and '" + (arg.m_metavar.empty() ? arg.m_names.back() : arg.m_metavar) + "' specified.");
        }
        used = arg;
      }
    }
    if (m_required && !used) {
      throw std::runtime_error("Missing option from required mutually exclusive group '" + get_inline_usage() + "'");
    }
    return true;
  }

  template <typename... Targs> Argument &add_argument(Targs... f_args);


private:
  ArgumentParser &m_parser;
  std::list<std::reference_wrapper<Argument>> m_arguments;
  bool m_has_positional = false;
  bool m_required;
};


class ArgumentParser {
public:
  explicit ArgumentParser(std::string program_name = {},
                          std::string version = "1.0",
                          default_arguments add_args = default_arguments::all)
      : m_program_name(std::move(program_name)), m_version(std::move(version)) {
    if ((add_args & default_arguments::help) == default_arguments::help) {
      add_argument("-h", "--help")
          .action([&](const auto &/*unused*/) {
            std::cout << help().str();
            std::exit(0);
          })
          .default_value(false)
          .help("shows help message and exits")
          .implicit_value(true)
          .nargs(0);
    }
    if ((add_args & default_arguments::version) == default_arguments::version) {
      add_argument("-v", "--version")
          .action([&](const auto &/*unused*/) {
            std::cout << m_version << std::endl;
            std::exit(0);
          })
          .default_value(false)
          .help("prints version information and exits")
          .implicit_value(true)
          .nargs(0);
    }
  }

  ArgumentParser(ArgumentParser &&) noexcept = default;
  ArgumentParser &operator=(ArgumentParser &&) = default;

  ArgumentParser(const ArgumentParser &other)
      : m_program_name(other.m_program_name),
        m_version(other.m_version),
        m_description(other.m_description),
        m_epilog(other.m_epilog),
        m_is_parsed(other.m_is_parsed),
        m_positional_arguments(other.m_positional_arguments),
        m_optional_arguments(other.m_optional_arguments) {
    for (auto it = std::begin(m_positional_arguments);
         it != std::end(m_positional_arguments); ++it) {
      index_argument(it);
    }
    for (auto it = std::begin(m_optional_arguments);
         it != std::end(m_optional_arguments); ++it) {
      index_argument(it);
    }
  }

  ~ArgumentParser() = default;

  ArgumentParser &operator=(const ArgumentParser &other) {
    auto tmp = other;
    std::swap(*this, tmp);
    return *this;
  }

  // Parameter packing
  // Call add_argument with variadic number of string arguments
  template <typename... Targs> Argument &add_argument(Targs... f_args) {
    using array_of_sv = std::array<std::string_view, sizeof...(Targs)>;
    auto argument = m_optional_arguments.emplace(
        std::cend(m_optional_arguments), array_of_sv{f_args...});

    if (!argument->m_is_optional) {
      m_positional_arguments.splice(std::cend(m_positional_arguments),
                                    m_optional_arguments, argument);
    }

    index_argument(argument);
    return *argument;
  }

  // Parameter packed add_parents method
  // Accepts a variadic number of ArgumentParser objects
  template <typename... Targs>
  ArgumentParser &add_parents(const Targs &...f_args) {
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

  ArgumentParser &add_description(std::string description) {
    m_description = std::move(description);
    return *this;
  }

  ArgumentParser &add_epilog(std::string epilog) {
    m_epilog = std::move(epilog);
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
    for ([[maybe_unused]] const auto& [unused, argument] : m_argument_map) {
      argument->validate();
    }
    // Check that all the groups are correctly exclusive
    for (const auto &group : m_exclusive_groups) {
      group.validate();
    }
  }

  /* Main entry point for parsing command-line arguments using this
   * ArgumentParser
   * @throws std::runtime_error in case of any invalid argument
   */
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
  void parse_args(int argc, const char *const argv[]) {
    parse_args({argv, argv + argc});
  }

  /* Getter for options with default values.
   * @throws std::logic_error if parse_args() has not been previously called
   * @throws std::logic_error if there is no such option
   * @throws std::logic_error if the option has no value
   * @throws std::bad_any_cast if the option is not of type T
   */
  template <typename T = std::string> auto get(std::string_view arg_name) const
    -> std::conditional_t<details::IsContainer<T>, T, const T&> {
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

  /* Indexing operator. Return a reference to an Argument object
   * Used in conjuction with Argument.operator== e.g., parser["foo"] == true
   * @throws std::logic_error in case of an invalid argument name
   */
  Argument &operator[](std::string_view arg_name) const {
    auto it = m_argument_map.find(arg_name);
    if (it != m_argument_map.end()) {
      return *(it->second);
    }
    if (arg_name.front() != '-') {
      std::string name(arg_name);
      // "-" + arg_name
      name = "-" + name;
      it = m_argument_map.find(name);
      if (it != m_argument_map.end()) {
        return *(it->second);
      }
      // "--" + arg_name
      name = "-" + name;
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

    std::size_t longest_arg_length = parser.get_length_of_longest_argument();

    stream << parser.usage() << "\n\n";

    if (!parser.m_description.empty()) {
      stream << parser.m_description << "\n\n";
    }

    if (!parser.m_positional_arguments.empty()) {
      stream << "Positional arguments:\n";
    }

    for (const auto &argument : parser.m_positional_arguments) {
      stream.width(longest_arg_length);
      stream << argument;
    }

    if (!parser.m_optional_arguments.empty()) {
      stream << (parser.m_positional_arguments.empty() ? "" : "\n")
             << "Optional arguments:\n";
    }

    for (const auto &argument : parser.m_optional_arguments) {
      stream.width(longest_arg_length);
      stream << argument;
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

  // Format usage part of help only
  auto usage() const -> std::string {
    std::stringstream stream;

    stream << "Usage: " << this->m_program_name;

    // Geometry for managing wrapping
    std::size_t screen_width = details::get_screen_width();
    std::size_t program_indent = 7 + this->m_program_name.size();
    std::size_t width = program_indent;

    // Add any options inline here
    for (const auto &argument : this->m_optional_arguments) {
      std::string term;
      if (argument.m_in_group) {
        continue;
      } else if (argument.m_names[0] == "-h") {
        term = " [-h]";
      } else {
        term = " " + argument.get_inline_usage();
      }
      if (width + term.size() > screen_width) {
        stream << "\n" << std::string(program_indent, ' ');
        // stream << ;
        // for (int i = 0; i < program_indent; ++i) stream << " ";
        width = program_indent;
      }
      stream << term;
      width += term.size();
    }
    // Put positional arguments after the optionals
    for (const auto &argument : this->m_positional_arguments) {
      std::string term;
      if (argument.m_in_group) {
        continue;
      } else if (!argument.m_metavar.empty()) {
        term = " " + argument.m_metavar;
      } else {
        term = " " + argument.m_names.front();
      }
      if (width + term.size() > screen_width) {
        stream << "\n" << std::string(program_indent, ' ');
        width = program_indent;
      }
      stream << term;
      width+= term.size();
    }

    // Finally, treat any mutually exclusive groups
    for (const auto &group : this->m_exclusive_groups) {
      std::string term = " " + group.get_inline_usage();
      if (width + term.size() > screen_width) {
        stream << "\n" << std::string(program_indent, ' ');
        width = program_indent;
      }
      stream << term;
      width+= term.size();
    }
    return stream.str();
  }

  auto add_mutually_exclusive_group(bool required = false) -> ArgumentParserMutuallyExclusiveGroup & {
    auto &group = m_exclusive_groups.emplace_back(*this, required);
    return group;
  }

  // Printing the one and only help message
  // I've stuck with a simple message format, nothing fancy.
  [[deprecated("Use cout << program; instead.  See also help().")]] std::string
  print_help() const {
    auto out = help();
    std::cout << out.rdbuf();
    return out.str();
  }

private:
  /*
   * @throws std::runtime_error in case of any invalid argument
   */
  void parse_args_internal(const std::vector<std::string> &raw_arguments) {
    // Pre-process this argument list. Anything starting with "--", that
    // contains an =, where the prefix before the = has an entry in the
    // options table, should be split.
    std::vector<std::string> arguments;
    for (const auto &arg : raw_arguments) {
      // Check that:
      // - We don't have an argument named exactly this
      // - The argument starts with "--"
      // - The argument contains a "="
      std::size_t eqpos = arg.find("=");
      if (m_argument_map.find(arg) == m_argument_map.end() &&
          arg.rfind("--", 0) == 0 && eqpos != std::string::npos) {
        // Get the name of the potential option, and check it exists
        std::string opt_name = arg.substr(0, eqpos);
        if (m_argument_map.find(opt_name) != m_argument_map.end()) {
          // This is the name of an option! Split it into two parts
          arguments.push_back(std::move(opt_name));
          arguments.push_back(arg.substr(eqpos + 1));
          continue;
        }
      }
      // If we've fallen through to here, then it's a standard argument
      arguments.push_back(arg);
    }

    if (m_program_name.empty() && !arguments.empty()) {
      m_program_name = arguments.front();
    }
    auto end = std::end(arguments);
    auto positional_argument_it = std::begin(m_positional_arguments);
    for (auto it = std::next(std::begin(arguments)); it != end;) {
      const auto &current_argument = *it;
      if (Argument::is_positional(current_argument)) {
        if (positional_argument_it == std::end(m_positional_arguments)) {
          throw std::runtime_error(
              "Maximum number of positional arguments exceeded");
        }
        auto argument = positional_argument_it++;
        it = argument->consume(it, end);
        continue;
      }

      auto arg_map_it = m_argument_map.find(current_argument);
      if (arg_map_it != m_argument_map.end()) {
        auto argument = arg_map_it->second;
        it = argument->consume(std::next(it), end, arg_map_it->first);
      } else if (const auto &compound_arg = current_argument;
                 compound_arg.size() > 1 && compound_arg[0] == '-' &&
                 compound_arg[1] != '-') {
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

  // Used by print_help.
  std::size_t get_length_of_longest_argument() const {
    if (m_argument_map.empty()) {
      return 0;
    }
    std::size_t max_size = 0;
    for ([[maybe_unused]] const auto& [unused, argument] : m_argument_map) {
      max_size = std::max(max_size, argument->get_arguments_length());
    }
    return max_size;
  }

  using list_iterator = std::list<Argument>::iterator;

  void index_argument(list_iterator it) {
    for (const auto &name : std::as_const(it->m_names)) {
      m_argument_map.insert_or_assign(name, it);
    }
  }

  std::string m_program_name;
  std::string m_version;
  std::string m_description;
  std::string m_epilog;
  bool m_is_parsed = false;
  std::list<Argument> m_positional_arguments;
  std::list<Argument> m_optional_arguments;
  std::list<ArgumentParserMutuallyExclusiveGroup> m_exclusive_groups;
  std::map<std::string_view, list_iterator, std::less<>> m_argument_map;
};

// Parameter packing
// Call add_argument with variadic number of string arguments
template <typename... Targs> Argument &ArgumentParserMutuallyExclusiveGroup::add_argument(Targs... f_args) {
  using array_of_sv = std::array<std::string_view, sizeof...(Targs)>;
  // auto argument = m_parser.add_argument(f_args...);
  // argument.m_in_group = true;
  auto argument_test = Argument{array_of_sv{f_args...}};
  if (!argument_test.m_is_optional) {
    if (m_has_positional) {
      throw std::logic_error("Only one positional allowed per mutually exclusive group");
    }
    m_has_positional = true;
  }
  auto &argument = m_parser.add_argument<Targs...>(f_args...);
  argument.m_in_group = true;
  m_arguments.push_back(argument);
  return argument;
}

} // namespace argparse
