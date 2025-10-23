#include <filesystem>
#include <format>
#include <imgui.h>
#include <iminspect.hpp>

namespace ImInspect {
namespace {


  void colored_pretty_typename(const std::string& pretty) { details::Text(pretty); }


  std::string pretty_typename(const std::string_view type_name)
  {
    std::string s(type_name);

    static constexpr std::string_view keywords[] = {"class ", "enum ", "struct ", "__1::", "__2::", "__3::", "__4::"};

    for (std::size_t pos; const auto& kw : keywords) {
      while ((pos = s.find(kw)) != std::string::npos) {
        s.erase(pos, kw.size());
      }
    }

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
    details::display_readonly_data(std::string_view(!p ? "nullptr" : std::format("{}", const_cast<void*>(p))),
                                   name,
                                   type_name<decltype(p)>);
  }

} // namespace

std::string_view to_string(const char* s) { return s; }

void do_inspection(bool& b, const std::string& name) { ImGui::Checkbox(name.c_str(), &b); }
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

namespace details {

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


  void inspect_filesystem_path(void* fs, const std::string& name)
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
    // blue ish
    details::Text("Type: ");
    ImGui::SameLine();

    auto pretty = ImInspect::pretty_typename(name);

    ImInspect::colored_pretty_typename(pretty);
    //details::Text(pretty);
    //if (addressof) {
    //details::Text("Address: ");
    //ImGui::SameLine();
    //details::Text(std::format("{}", const_cast<const void*>(addressof)));
    //}
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
} // namespace ImInspect