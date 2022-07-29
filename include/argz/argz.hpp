// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//      http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace argz
{
   template <class T>
   using ref = std::reference_wrapper<T>;

   using var = std::variant<ref<bool>,
      ref<int32_t>, ref<uint32_t>, ref<int64_t>, ref<uint64_t>,
   ref<std::string>>;

   struct ids_t final {
      std::string_view id{};
      char alias = '\0';
   };   

   struct arg_t final {
      ids_t ids{};
      var value;
      std::string_view help{};
      bool required{};
   };

   using options = std::vector<arg_t>;

   inline constexpr bool required = true;

   struct about final {
      std::string_view description{}, version{};
      bool help{};
   };

   template <class T>
   inline constexpr bool false_v = false;

   namespace detail
   {
      template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
      template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
      
      inline std::string parse_var(const char* c) {
         auto start = c;
         while (*c != '\0' && *c != ' ') {
            ++c;
         }
         return { start, static_cast<size_t>(c - start) };
      }

      inline void parse(const char* c, var& v)
      {
         const auto str = parse_var(c);
         std::visit(overloaded {
            [&](ref<std::string>& x) { x.get() = str; },
            [&](ref<bool>& x) { x.get() = str == "true" ? true : false; },
            [&](auto& x) { x.get() = static_cast<typename std::decay_t<decltype(x)>::type>(std::stol(str)); },
         }, v);
      }

      inline std::string to_string(const var& v) {
         return std::visit(overloaded {
            [&](const ref<std::string>& x) { return x.get(); },
            [&](const auto& x) { return std::to_string(x.get()); },
         }, v);
      }
   }

   namespace detail
   {
      inline void help(const about& about, const options& opts)
      {
         if (about.description.size()) {
            std::cout << about.description << '\n';
         }
         if (about.version.size()) {
            std::cout << "Version: " << about.version << '\n';
         }
         
         std::cout << '\n' << R"(-h, --help       write help to console)" << '\n';
         std::cout << R"(-v, --version    write the version to console)" << '\n';

         for (auto& [ids, value, help, req] : opts)
         {
            if (ids.alias != '\0') {
               std::cout << '-' << ids.alias;
               std::cout << ", " << "--" << ids.id;
            }
            else {
               std::cout << (ids.id.size() == 1 ? "-" : "--") << ids.id;
            }
            
            std::cout << (req ? " (required)  " : "    ");
            std::cout << help;
            std::cout << ", default: " << detail::to_string(value) << '\n';
         }
         std::cout << '\n';
      }
   }

   // we use a template to handle both "const char*" and "char*"
   template <class int_t, class char_ptr_t, std::enable_if_t<std::is_pointer_v<char_ptr_t>, int> = 0>
   inline void parse(about& about, const options& opts, const int_t argc, char_ptr_t argv)
   {
      std::unordered_map<std::string_view, var> values;

      if (argc == 1) {
         detail::help(about, opts);
         return;
      }

      std::unordered_set<std::string_view> required_inputs, inputs;

      std::unordered_map<char, std::string_view> aliases;

      aliases['h'] = "help";
      values.emplace("help", about.help);

      for (auto& [ids, value, help, req] : opts)
      {
         if (ids.id.empty() && ids.alias == '\0') {
            throw std::runtime_error("Empty identifier given");
         }
         
         if (ids.alias != '\0') {
            aliases.emplace(ids.alias, ids.id);
         }

         if (ids.id.size()) {
            values.emplace(ids.id, value);
         }

         if (req) {
            required_inputs.emplace(ids.id);
         }
      }

      for (int_t i = 1; i < argc; ++i) {
         const char* flag = argv[i];
         if (*flag != '-') {
            throw std::runtime_error("expected '-'");
         }
         ++flag;
         const bool repated_flag = *flag == '-';

         std::string_view str;
         if (repated_flag) {
            ++flag;
            str = detail::parse_var(flag);
         }
         else {
            str = detail::parse_var(flag);
            if (str.size() == 1 && aliases.contains(*flag)) {
               str = aliases.at(*flag);
            }
            else {
               throw std::runtime_error("Invalid alias flag '-' for: " + std::string(str));
            }
         }
         if (str.empty()) {
            break;
         }
         inputs.emplace(str);
              
         auto& v = values.at(str);
         if (std::holds_alternative<ref<bool>>(v)) {
            std::get<ref<bool>>(v).get() = true;
         }
         else {
            detail::parse(argv[++i], v);
         }
      }

      if (inputs.count("help")) {
         detail::help(about, opts);
      }
      if (inputs.count("version")) {
         std::cout << "Version: " << about.version << '\n';
      }

      for (auto& i : required_inputs) {
         if (!inputs.count(i)) {
            std::cerr << "Required '--" + std::string(i) + "' was not provided\n\n";
            detail::help(about, opts);
         }
      }
   }
}
