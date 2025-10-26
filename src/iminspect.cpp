#include <cstring>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <iminspect.hpp>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace ImInspect {
Style& GetStyle()
{
  static auto style = []() {
    Style s{};
    s.TypeHighlighter.Text      = {0.9f, 0.9f, 0.9f, 1.0f};
    s.TypeHighlighter.Bracket   = {0.4f, 0.7f, 1.0f, 1.0f};
    s.TypeHighlighter.Symbol    = {0.7f, 0.7f, 0.7f, 1.0f};
    s.TypeHighlighter.Operator  = {1.0f, 0.6f, 0.3f, 1.0f};
    s.TypeHighlighter.Keyword   = {1.0f, 0.3f, 0.3f, 1.0f};
    s.TypeHighlighter.Namespace = {0.76f, 0.38f, 0.59f, 1.0f};
    s.TypeHighlighter.Name      = {0.4f, 0.7f, 1.0f, 1.0f};
    return s;
  }();
  return style;
}

namespace {
  struct RegexAlias {
    std::regex  pattern;
    std::string replacement;
  };
  auto& get_regex_pattern_set()
  {
    static std::unordered_set<std::string, std::hash<std::string_view>, std::equal_to<>> r;
    return r;
  }
  auto& get_regex_aliases()
  {
    static auto r = []() {
      std::vector<RegexAlias> ret;
      const auto              add = [&ret](const std::string_view a, std::string p) {
        ret.push_back({std::regex(a.data(), a.data() + a.size()), std::move(p)});
      };

      // FROM CHATGPT
      // --- String types ---
      add(R"(std::basic_string<\s*char\s*,\s*std::char_traits<.*?>\s*,\s*std::allocator<\s*char\s*>\s*>)",
          "std::string");
      add(R"(std::basic_string<\s*char8_t\s*,\s*std::char_traits<.*?>\s*,\s*std::allocator<\s*char8_t\s*>\s*>)",
          "std::u8string");
      add(R"(std::basic_string<\s*char16_t\s*,\s*std::char_traits<.*?>\s*,\s*std::allocator<\s*char16_t\s*>\s*>)",
          "std::u16string");
      add(R"(std::basic_string<\s*char32_t\s*,\s*std::char_traits<.*?>\s*,\s*std::allocator<\s*char32_t\s*>\s*>)",
          "std::u32string");
      add(R"(std::basic_string<\s*wchar_t\s*,\s*std::char_traits<.*?>\s*,\s*std::allocator<\s*wchar_t\s*>\s*>)",
          "std::wstring");

      // --- STL containers ---
      add(R"(std::vector<([^,>]+),\s*std::allocator<[^>]+>>)", "std::vector<$1>");
      add(R"(std::deque<([^,>]+),\s*std::allocator<[^>]+>>)", "std::deque<$1>");
      add(R"(std::list<([^,>]+),\s*std::allocator<[^>]+>>)", "std::list<$1>");
      add(R"(std::set<([^,>]+),\s*std::less<[^>]+>,\s*std::allocator<[^>]+>>)", "std::set<$1>");
      add(R"(std::unordered_set<([^,>]+),\s*std::hash<[^>]+>,\s*std::equal_to<[^>]+>,\s*std::allocator<[^>]+>>)",
          "std::unordered_set<$1>");
      add(R"(std::map<([^,]+),([^,>]+),\s*std::less<[^>]+>,\s*std::allocator<[^>]+>>)", "std::map<$1,$2>");
      add(R"(std::unordered_map<([^,]+),([^,>]+),\s*std::hash<[^>]+>,\s*std::equal_to<[^>]+>,\s*std::allocator<[^>]+>>)",
          "std::unordered_map<$1,$2>");
      add(R"(std::pair<\s*([^,]+)\s*,\s*([^>]+)\s*>)", "std::pair<$1,$2>");

      // --- Smart pointers ---
      add(R"(std::unique_ptr<([^>]+)>)", "std::unique_ptr<$1>");
      add(R"(std::shared_ptr<([^>]+)>)", "std::shared_ptr<$1>");
      add(R"(std::weak_ptr<([^>]+)>)", "std::weak_ptr<$1>");

      // --- String views ---
      add(R"(std::basic_string_view<\s*char\s*,\s*std::char_traits<.*?>\s*>)", "std::string_view");
      add(R"(std::basic_string_view<\s*char8_t\s*,\s*std::char_traits<.*?>\s*>)", "std::u8string_view");
      add(R"(std::basic_string_view<\s*char16_t\s*,\s*std::char_traits<.*?>\s*>)", "std::u16string_view");
      add(R"(std::basic_string_view<\s*char32_t\s*,\s*std::char_traits<.*?>\s*>)", "std::u32string_view");
      add(R"(std::basic_string_view<\s*wchar_t\s*,\s*std::char_traits<.*?>\s*>)", "std::wstring_view");

      // --- Common STL helpers ---
      add(R"(std::span<([^>]+)>)", "std::span<$1>");
      add(R"(std::array<([^,]+),\s*(\d+)>)", "std::array<$1,$2>");
      add(R"(std::optional<([^>]+)>)", "std::optional<$1>");
      add(R"(std::variant<([^>]+)>)", "std::variant<$1>");

      // --- Chrono types ---
      add(R"(std::chrono::duration<([^,]+),\s*([^>]+)>)", "std::chrono::duration<$1,$2>");
      add(R"(std::chrono::time_point<([^,]+),\s*([^>]+)>)", "std::chrono::time_point<$1,$2>");

      return ret;
    }();
    return r;
  }


  void display_label_with_type_tooltip(const std::string_view label, const std::string_view type_name)
  {
    details::Text(label);
    if (ImGui::IsItemHovered())
      details::type_tooltip(type_name);
  }
  ImGuiDataType_ map_int_type_to_imgui(const std::size_t size, const bool is_unsigned)
  {
    static_assert(CHAR_BIT == 8, "This library requires 8 bit systems");
    switch (size) {
      case 1:
        return is_unsigned ? ImGuiDataType_U8 : ImGuiDataType_S8;
      case 2:
        return is_unsigned ? ImGuiDataType_U16 : ImGuiDataType_S16;
      case 4:
        return is_unsigned ? ImGuiDataType_U32 : ImGuiDataType_S32;
      case 8:
        return is_unsigned ? ImGuiDataType_U64 : ImGuiDataType_S64;
    }
    return (assert(!"Invalid type passed"), ImGuiDataType_U8);
  }


  template<typename Void>
  void display_readonly_data_voidptr(Void* const p, const std::string& name)
  {
    static_assert(std::is_void_v<Void>);
    details::display_readonly_data(std::string_view(
                                     !p ? "nullptr" : std::format("0x{:016x}", reinterpret_cast<std::uintptr_t>(p))),
                                   name,
                                   type_name<decltype(p)>);
  }

  void replace_all(std::string& s, const std::string_view pattern, const std::string_view with)
  {
    std::size_t pos;
    while ((pos = s.find(pattern)) != std::string::npos) {
      s.erase(pos, pattern.size());
      s.insert(pos, with);
    }
  }

} // namespace

struct string_hasher : std::hash<std::string_view> {
  using is_transparent = void;
};

std::string normalize_type_name(std::string_view type_name)
{
  static std::unordered_map<std::string, std::string, string_hasher, std::equal_to<>> cache;

  // TODO: see thread safety later
  if (const auto it = cache.find(type_name); it != cache.end())
    return it->second;

  std::string s(type_name);

  static constexpr std::string_view keywords[] =
    {"class ", "enum ", "struct ", "union ", "__1::", "__2::", "__3::", "__4::"};

  for (const auto& kw : keywords)
    ImInspect::replace_all(s, kw, "");

  ImInspect::replace_all(s, "volatile const ", "volatile_const@VOLATILE_CONST@");
  ImInspect::replace_all(s, "const ", "const@CONST@");
  ImInspect::replace_all(s, "volatile ", "volatile@VOLATILE@");

  std::erase_if(s, [](const auto c) { return c == ' ' || c == '\t'; });

  ImInspect::replace_all(s, "const@CONST@", "const ");
  ImInspect::replace_all(s, "volatile@VOLATILE@", "volatile ");
  ImInspect::replace_all(s, "volatile_const@VOLATILE_CONST@", "const volatile ");

  for (const auto& entry : get_regex_aliases())
    s = std::regex_replace(s, entry.pattern, entry.replacement);

  cache.emplace(type_name, s);

  return s;
}

void colored_pretty_typename(const std::string& pretty, float indent)
{
  using namespace ImSweet;

  std::vector<std::pair<std::string_view, ImVec4>> colors;

  const auto& style = ImInspect::GetStyle();

  const auto color_text      = style.TypeHighlighter.Text;
  const auto color_bracket   = style.TypeHighlighter.Bracket;
  const auto color_symbol    = style.TypeHighlighter.Symbol;
  const auto color_operator  = style.TypeHighlighter.Operator;
  const auto color_keyword   = style.TypeHighlighter.Keyword;
  const auto color_namespace = style.TypeHighlighter.Namespace;
  const auto color_name      = style.TypeHighlighter.Name;

  // clang-format off
static constexpr std::string_view cpp_keywords[] = {
    // fundamental types
    "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t",
    "short", "int", "long", "float", "double", "void", "auto",

    // type modifiers
    "signed", "unsigned", "const", "volatile", "mutable",

    // storage/class specifiers
    "static", "extern", "register", "thread_local", "inline", "virtual", "explicit", "friend",
    "constexpr", "consteval", "constinit", "noexcept", "final", "override",

    // class/struct/enum
    "class", "struct", "union", "enum", "typename",

    // control flow
    "if", "else", "switch", "case", "default", "for", "while", "do",
    "break", "continue", "goto", "return", "try", "catch", "throw",

    // memory management
    "new", "delete", "alignas", "alignof",

    // literals
    "true", "false", "nullptr",

    // namespace and modules
    "namespace", "using", "export", "import", "module",

    // type casting
    "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast",

    // operators and miscellaneous keywords
    "sizeof", "typeid", "decltype","typeof", "asm", "bitand", "bitor", "and", "and_eq",
    "or", "or_eq", "xor", "xor_eq", "compl", "not", "not_eq",

    // MSVC extensions
    "__declspec", "__forceinline", "__assume", "__noop", "__unaligned", "__super", "__try",
    "__except", "__finally", "__leave", "__fastcall", "__stdcall", "__cdecl", "__thiscall",
    "__vectorcall", "__ptr32", "__ptr64", "__int8", "__int16", "__int32", "__int64", "__int128",
    "__uuidof", "__based", "__based_ptr", "__based_seg", "__eventadd", "__eventremove", "__identifier",
    "__single_inheritance", "__multiple_inheritance", "__virtual_inheritance", "__interface",

    // GCC/Clang extensions
    "__attribute__", "__builtin_expect", "__builtin_popcount", "__builtin_prefetch", "__builtin_offsetof",
    "__builtin_choose_expr", "__builtin_types_compatible_p", "__builtin_unreachable", "__restrict", "__thread",
    "__has_include", "__has_cpp_attribute", "__FUNCTION__", "__PRETTY_FUNCTION__", "__FUNCDNAME__", "__FUNCSIG__",

    // coroutine keywords
    "co_await", "co_yield", "co_return",

    // rarely-used compiler intrinsics
    "__asm", "__inline", "__volatile", "__cdecl", "__stdcall", "__fastcall", "__vectorcall", "__fastcall",
    "__restrict__", "__thread_local", "__alignof__", "__alignas__", "__typeof__", "__label__", "__signed__", "__unsigned__"
};
  // clang-format on

  static constexpr char brackets[]  = {'<', '>', '{', '}', '(', ')', '[', ']'};
  static constexpr char operators[] = {',', '*', '&', '+', '-', '/', '%', '=', '!', '~', '^', '|', '?', '.'};
  auto                  is_keyword  = [&](std::string_view token) {
    for (auto kw : cpp_keywords)
      if (kw == token)
        return true;
    return false;
  };

  const char*       begin = pretty.data();
  const char* const end   = begin + pretty.size();

  const auto contains = [](const auto& c, const auto& value) {
    return std::ranges::find(c, value) != std::ranges::end(c);
  };

  const char* token_start = nullptr;
  std::string token;

  for (const char* it = begin; it != end; ++it) {
    const char c = *it;

    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      if (token.empty())
        token_start = it;
      token += c;
    }
    else {
      if (!token.empty()) {
        ImVec4 col;
        if (is_keyword(token))
          col = color_keyword;
        else
          col = color_name;

        colors.emplace_back(std::string_view(token_start, token.size()), col);
        token.clear();
      }

      // handle namespace ::
      if (c == ':' && (it + 1 != end && it[1] == ':')) {
        if (!colors.empty())
          colors.back().second = color_namespace;
        colors.emplace_back(std::string_view(it, 2), color_symbol);
        ++it;
      }
      else {
        ImVec4 col = color_symbol;

        if (contains(brackets, c))
          col = color_bracket;
        else if (contains(operators, c))
          col = color_operator;
        else if (std::isspace(static_cast<unsigned char>(c)))
          col = color_text;

        colors.emplace_back(std::string_view(it, 1), col);
      }

      token_start = it + 1;
    }
  }

  // push remaining token
  if (!token.empty()) {
    colors.emplace_back(std::string_view(token_start, token.size()), is_keyword(token) ? color_keyword : color_name);
  }

  // render all accumulated tokens
  for (size_t i = 0; i < colors.size(); ++i) {
    const auto& [sub, color] = colors[i];
    auto style               = StyleColor(ImGuiCol_Text, color);

    assert(sub.size() > 0);
    if (sub[0] == '\n') {
      auto i = ImSweet::Indent(indent);
      details::Text("\n");
    }
    else {
      ImGui::SameLine(0, 0);
      details::Text(sub);
    }
  }
}

std::string pretty_typename(const std::string_view type_name)
{
  std::string s(type_name);

  if (s.size() < 130)
    return s;


  {
    int                               indent_level = 0;
    static constexpr std::string_view indent       = "    ";
    for (std::size_t i = 0; i < s.size(); ++i) {
      if (s[i] == '<') {
        ++indent_level;
        const auto amount = std::size_t(indent_level) * indent.size();
        s.insert(i + 1, "\n" + std::string(amount, ' '));
        i += 1 + amount;
      }
      else if (s[i] == '>') {
        --indent_level;
        const auto amount = std::size_t(indent_level) * indent.size();
        s.insert(i, "\n" + std::string(amount, ' '));
        i += 1 + amount;
      }
    }
  }

  return s;
}

std::string_view to_string(const char* s) { return s; }

void do_inspection(bool& b, const std::string& name) { 
    ImGui::Checkbox(name.c_str(), &b); }
void do_inspection(char& c, const std::string& name)
{
  const ImSweet::ID id(name);
  ImGui::InputText("", &c, 1);
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(c)>);
}

void do_inspection(void* p, const std::string& name) { ImInspect::display_readonly_data_voidptr(p, name); }
void do_inspection(const void* p, const std::string& name) { ImInspect::display_readonly_data_voidptr(p, name); }
void do_inspection(volatile void* p, const std::string& name) { ImInspect::display_readonly_data_voidptr(p, name); }

void do_inspection(const volatile void* const p, const std::string& name)
{
  ImInspect::display_readonly_data_voidptr(p, name);
}

void do_inspection(const bool& b, const std::string& name)
{
  bool copy = b;
  ImGui::Checkbox(name.c_str(), &copy);
}
void do_inspection(const char& c, const std::string& name)
{
  const char buf[3] = {'\'', c, '\''};
  details::display_readonly_data(std::string_view(buf, sizeof(buf)), name, type_name<decltype(c)>);
}

void do_inspection(std::string_view s, const std::string& name)
{
  details::display_readonly_data(s, name, type_name<decltype(s)>);
}

void do_inspection(const std::string& s, const std::string& name)
{
  details::display_readonly_data(s, name, type_name<decltype(s)>);
}

void do_inspection(std::string& s, const std::string& name)
{
  const ImSweet::ID id(name);
  ImGui::InputText("", &s);
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(s)>);
}


void do_inspection(ImVec4& v, const std::string& name)
{
  const ImSweet::ID id(name);
  ImGui::InputFloat4("", &v.x);
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(v)>);
}
void do_inspection(const ImVec4& v, const std::string& name)
{
  const ImSweet::ID id(name);
  auto              c = v;
  ImGui::InputFloat4("", &c.x, "%.3f", ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);
  if (ImGui::IsItemHovered())
    details::red_tooltip("Cannot edit this field it is not writable.");
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(v)>);
}
void do_inspection(ImVec2& v, const std::string& name)
{
  const ImSweet::ID id(name);
  ImGui::InputFloat2("", &v.x);
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(v)>);
}
void do_inspection(const ImVec2& v, const std::string& name)
{
  const ImSweet::ID id(name);
  auto              c = v;
  ImGui::InputFloat2("", &c.x, "%.3f", ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);
  if (ImGui::IsItemHovered())
    details::red_tooltip("Cannot edit this field it is not writable.");
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(v)>);
}
void do_inspection(ImColor& c, const std::string& name)
{
  const ImSweet::ID id(name);
  ImGui::ColorEdit4("", &c.Value.x);
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(c)>);
}
void do_inspection(const ImColor& c, const std::string& name)
{
  const ImSweet::ID id(name);
  auto              copy = c;
  ImGui::ColorEdit4("", &copy.Value.x, ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);
  if (ImGui::IsItemHovered())
    details::red_tooltip("Cannot edit this field it is not writable.");
  ImGui::SameLine();
  ImInspect::display_label_with_type_tooltip(name, type_name<decltype(c)>);
}

namespace details {

  void display_function_pointer(void (*f)(), const std::string& name, std::string_view type_name)
  {
    if (f != nullptr) {
      if constexpr (sizeof(f) == sizeof(void*)) {
        details::display_readonly_data(std::format("0x{:016x}", reinterpret_cast<std::uintptr_t>(f)), name, type_name);
      }
      else {
        details::display_readonly_data("<cannot display function pointer on this architecture>", name, type_name);
      }
    }
    else {
      details::display_readonly_data("nullptr", name, type_name);
    }
  }


  void modify_numeric_float(void* const p, const std::string& name, const bool is_double, const std::string_view type_name)
  {
    const ImSweet::ID id(name);

    if (is_double)
      ImGui::InputDouble("", static_cast<double*>(p));
    else
      ImGui::InputFloat("", static_cast<float*>(p));
    ImGui::SameLine();
    ImInspect::display_label_with_type_tooltip(name, type_name);
  }

  void modify_numeric_int(void* const            p,
                          const std::string&     name,
                          const std::size_t      size,
                          const bool             is_unsigned,
                          const std::string_view type_name)
  {
    const ImSweet::ID id(name);
    ImGui::InputScalar("", ImInspect::map_int_type_to_imgui(size, is_unsigned), p);
    ImGui::SameLine();
    ImInspect::display_label_with_type_tooltip(name, type_name);
  }

  void print_more_container_info(const std::size_t count)
  {
    details::Text(std::format("Size: {}", count));
    details::Text(std::format("Is empty: {}", count == 0));
  }


  void inspect_filesystem_path(void* const fs, const std::string& name)
  {
    assert(fs != nullptr);
    auto&       path = *static_cast<std::filesystem::path*>(fs);
    std::string s    = path.string();

    const ImSweet::ID id(name);
    ImGui::InputText("", &s);
    ImGui::SameLine();
    ImInspect::display_label_with_type_tooltip(name, type_name<decltype(path)>);
    path = static_cast<std::string&&>(s);
  }

  bool red_button(const char* const name)
  {
    const auto color = ImSweet::StyleColor({{ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f)},
                                            {ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f)},
                                            {ImGuiCol_ButtonActive, ImVec4(1.0f, 0.1f, 0.1f, 1.0f)}});
    return ImGui::Button(name);
  }

  bool green_button(const char* const name)
  {
    const auto t = ImSweet::StyleColor({{ImGuiCol_Button, ImVec4(0.1f, 0.8f, 0.1f, 1.0f)},
                                        {ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.9f, 0.2f, 1.0f)},
                                        {ImGuiCol_ButtonActive, ImVec4(0.1f, 1.0f, 0.1f, 1.0f)}});
    return ImGui::Button(name);
  }

  void grey_button(const char* const label, std::string_view tooltip)
  {
    const auto disabled = ImSweet::Disabled();
    ImGui::Button(label);
    if (!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      details::red_tooltip(tooltip);
    }
  }

  void type_tooltip(const std::string_view name, const volatile void*)
  {
    const auto tooltip = ImSweet::Tooltip();
    const auto color   = ImSweet::StyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));

    const char* prefix = "Type: ";
    details::Text(prefix);
    float label_width = ImGui::CalcTextSize(prefix).x;

    ImGui::SameLine();
    auto pretty = ImInspect::pretty_typename(ImInspect::normalize_type_name(name));
    ImInspect::colored_pretty_typename(pretty, label_width);
  }

  void display_readonly_data(const std::string_view s, const std::string& name, const std::string_view type_name)
  {

    const ImSweet::ID id(name);
    {
      const ImSweet::StyleColor color{{ImGuiCol_FrameBg, ImVec4(0.25f, 0.25f, 0.28f, 1.0f)},
                                      {ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f)}};
      ImGui::InputText("", const_cast<char*>(s.data()), s.size() + 1, ImGuiInputTextFlags_ReadOnly);
    }
    if (ImGui::IsItemHovered()) {
      details::red_tooltip("Cannot edit this field it is not writable.");
    }

    ImGui::SameLine();

    ImInspect::display_label_with_type_tooltip(name, type_name);
  }


  void display_readonly_data(const std::string_view s, const std::string& name)
  {
    {
      const auto color = ImSweet::StyleColor(
        {{ImGuiCol_FrameBg, ImVec4(0.25f, 0.25f, 0.28f, 1.0f)}, {ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f)}});
      // imgui expects size() + 1 or something like.
      // The const_cast is hacky but I am using input text with readonly
      // because the text box is nice looking
      // and I coud not replicate it myself.
      ImGui::InputText(name.c_str(), const_cast<char*>(s.data()), s.size() + 1, ImGuiInputTextFlags_ReadOnly);
    }
    if (ImGui::IsItemHovered())
      details::red_tooltip("Cannot edit this field it is not writable.");
  }


  void red_tooltip(const std::string_view tooltip)
  {
    const auto t = ImSweet::Tooltip();
    const auto c = ImSweet::StyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    ImGui::TextUnformatted(tooltip.data(), tooltip.data() + tooltip.size());
  }

  void Text(const std::string_view s) { ImGui::TextUnformatted(s.data(), s.data() + s.size()); }


} // namespace details

void add_regex_alias(std::string_view regex, std::string_view replacement)
{
  auto& set = get_regex_pattern_set();
  // insert returns {it, true} if new
  if (set.emplace(regex).second) {
    get_regex_aliases().push_back({std::regex(regex.data(), regex.data() + regex.size()), std::string(replacement)});
  }
}
} // namespace ImInspect