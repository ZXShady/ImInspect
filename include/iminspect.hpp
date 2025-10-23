#pragma once

#include <algorithm>
#include <cstddef>
#include <enchantum/enchantum.hpp>
#include <format>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <imsweet/raii.hpp>
#include <lahzam/lahzam.hpp>
#include <locale>
#include <ranges>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace ImInspect {

template<typename T>
inline constexpr bool is_opaque_enum = false;

template<>
inline constexpr bool is_opaque_enum<std::byte> = true;

std::string_view to_string(const char* s);

template<typename T>
auto to_string(const T& t)
{
  if constexpr (std::is_enum_v<T>) {
    if constexpr (is_opaque_enum<T>)
      return std::to_string(std::underlying_type_t<T>(t));
    else
      return enchantum::to_string(t);
  }
  else if constexpr (requires { t.to_string(); }) {
    return t.to_string();
  }
  else {
    return std::format("{}", t);
  }
}

template<typename T>
constexpr auto type_name = enchantum::raw_type_name<T>;

template<typename T>
constexpr auto type_name<T&> = type_name<T>;

template<typename T>
constexpr auto type_name<const T> = type_name<T>;

template<typename T>
constexpr auto type_name<volatile T> = type_name<T>;

template<typename T>
constexpr auto type_name<const volatile T> = type_name<T>;

template<>
constexpr auto type_name<std::string> = std::string_view("std::string");

template<>
constexpr auto type_name<std::string_view> = std::string_view("std::string_view");

// Forward declared so they can select the correct overload.
// sigh this took me a while to understand this is required.
template<typename T>
void do_inspection(const T& t, const std::string& name);
template<typename T>
void do_inspection(T& t, const std::string& name);
void do_inspection(bool& b, const std::string& name);
void do_inspection(char& c, const std::string& name);
void do_inspection(void* p, const std::string& name);
void do_inspection(const void* p, const std::string& name);
void do_inspection(volatile void* p, const std::string& name);
void do_inspection(const volatile void* p, const std::string& name);

void do_inspection(const bool& b, const std::string& name);
void do_inspection(const char& c, const std::string& name);
void do_inspection(std::string_view s, const std::string& name);
void do_inspection(const std::string& s, const std::string& name);
void do_inspection(std::string& s, const std::string& name);


namespace details {

  template<typename T>
  concept TupleLike = requires { typename std::tuple_size<T>::type; };

  template<typename T>
  concept VariantLike = requires { typename std::variant_size<T>::type; };

  template<typename T>
  concept OptionalLike = requires(T o, const T co) {
    o ? 0 : 0;
    *o;
    requires !std::is_same_v<decltype(*o), decltype(*co)>;
    requires std::is_same_v<std::remove_cvref_t<decltype(*o)>, std::remove_cvref_t<decltype(*co)>>;
    typename T::value_type;
    o.emplace();
    o.reset();
  };

  template<typename T>
  concept PointerLike = requires(T o, const T co) {
    o ? 0 : 0;
    *o;
    requires std::is_same_v<decltype(*o), decltype(*co)>;
  };


  template<typename T>
  concept AssociativeContainer = requires(T& c, const typename T::key_type& key) { c.at(key); };


  void inspect_filesystem_path(void* fs, const std::string& name);

  bool red_button(const char* name);
  bool green_button(const char* name);
  void grey_button(const char* name, std::string_view tooltip = "");
  void type_tooltip(std::string_view name, const volatile void* addressof = nullptr);
  void red_tooltip(std::string_view tooltip);

  void display_readonly_data(std::string_view s, const std::string& name);
  void display_readonly_data(std::string_view s, const std::string& name, const std::string_view type_name);

  void modify_numeric_int(void* p, const std::string& name, std::size_t size, bool is_unsigned, std::string_view type_name);
  void modify_numeric_float(void* p, const std::string& name, bool is_double, std::string_view type_name);

  void Text(std::string_view s);

  template<typename E>
  bool EnumCheckboxFlags(const std::string& group_label, E& enum_flags)
  {
    static_assert(enchantum::BitFlagEnum<E>);

    details::Text(group_label);
    if (ImGui::IsItemHovered())
      details::type_tooltip(type_name<E>);
    ImGui::SameLine();
    if (details::red_button("Clear"))
      enum_flags = E{};

    auto flags   = enchantum::to_underlying(enum_flags);
    bool changed = false;

    constexpr auto count           = enchantum::count<E>;
    constexpr auto count_diff_zero = count - enchantum::has_zero_flag<E>;
    constexpr int  items_per_row   = count_diff_zero <= 3 ? 1 : count_diff_zero <= 9 ? 2 : 3;

    if (const auto table = ImSweet::Table(group_label.c_str(), items_per_row, ImGuiTableFlags_SizingFixedFit)) {
      int column = 0;
      for (std::size_t i = enchantum::has_zero_flag<E>; i < count; ++i) {
        const auto bit_value = enchantum::to_underlying(enchantum::values_generator<E>[i]);
        bool       checked   = (flags & bit_value) != 0;
        const auto label     = enchantum::names_generator<E>[i];

        ImGui::TableNextColumn();
        if (ImGui::Checkbox(label.data(), &checked)) {
          changed = true;
          if (checked)
            flags |= bit_value;
          else
            flags &= ~bit_value;
        }
      }
    }

    enum_flags = static_cast<E>(flags);
    return changed;
  }


  template<typename E>
  bool EnumListBox(const std::string& label, E& current_enum, const std::size_t height_in_items = enchantum::count<E>)
  {
    static_assert(std::is_enum_v<E>);
    constexpr std::size_t item_count    = enchantum::count<E>;
    const auto            current_index = *enchantum::enum_to_index(current_enum);

    bool changed = false;


    // TODO: figure how to render it on the label
    //if (ImGui::IsItemHovered()) {
    //details::type_tooltip(type_name<E>);
    //}
    if (const auto listbox = ImSweet::ListBox(label.c_str(),
                                              ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * height_in_items))) {
      for (std::size_t i = 0; i < item_count; ++i) {
        const bool is_selected = (i == current_index);

        if (ImGui::Selectable(enchantum::names<E>[i].data(), is_selected)) {
          current_enum = enchantum::values<E>[i];
          changed      = true;
        }
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
    }
    return changed;
  }

  ImGuiDataType_ map_int_type_to_imgui(std::size_t size, bool is_unsigned);

  template<typename O>
  void inspect_optional(O& o, const std::string& name)
  {
    if (o) {
      if constexpr (requires { o.reset(); }) {
        if (details::red_button("-")) {
          o.reset();
          return;
        }
      }
      else {
        details::grey_button("-", "This optional-like type is does not have `.reset()` function.");
      }
      ImGui::SameLine();
      ImInspect::do_inspection(*o, name);
    }
    else {
      using ValT = typename O::value_type;
      if constexpr (std::is_default_constructible_v<ValT> && requires { o.emplace(); }) {
        if (details::green_button("+")) {
          o.emplace();
          return;
        }
      }
      else {
        std::string s = "Cannot create default instance the type is not default-constructible.\n type is ";
        s += type_name<ValT>;
        details::grey_button("+", s);
      }
      ImGui::SameLine();
      details::display_readonly_data("none", name, type_name<decltype(o)>);
    }
  }

  template<typename P>
  void inspect_pointer(P& p, const std::string& name)
  {
    if (p)
      ImInspect::do_inspection(*p, name);
    else
      details::display_readonly_data(std::string_view("nullptr"), name, type_name<decltype(p)>);
  }

  template<typename T, std::size_t... Is>
  void print_tuple(T& t, const std::string& name, std::index_sequence<Is...>)
  {
    int        i    = 0;
    const auto tree = ImSweet::TreeNode(name.c_str());
    if (ImGui::IsItemHovered()) {
      details::type_tooltip(type_name<T>);
    }
    if (tree) {
      using ::std::get;
      char c[] = {(ImSweet::ID(i++), ImInspect::do_inspection(get<Is>(t), "(" + std::to_string(Is) + ")"), 0)..., 0};
      (void)c;
    }
  }

  template<std::size_t I, typename V>
  void default_construct_alternative(void* v)
  {
    using T = std::variant_alternative_t<I, V>;
    if constexpr (std::is_default_constructible_v<T>)
      static_cast<V*>(v)->template emplace<T>();
  }
  template<std::size_t I, typename V>
  concept CanCallEmplaceAtleastOnce = requires(V& v) { v.template emplace<std::variant_alternative_t<I, V>>(); };

  template<typename V, std::size_t... Is>
  void print_variant(V& v, const std::string& name, std::index_sequence<Is...>)
  {
    using ::std::visit;

    const std::string_view type_names[] = {type_name<std::variant_alternative_t<Is, V>>...};
    static constexpr bool default_constructible[] = {std::is_default_constructible_v<std::variant_alternative_t<Is, V>>...};

    if constexpr ((CanCallEmplaceAtleastOnce<Is, V> || ...)) {
      const auto non_templated = [&](const std::string_view n) {
        using F                            = void(void*);
        static constexpr F* constructors[] = {default_construct_alternative<Is, V>...};
        if (ImGui::Button("+")) {
          ImGui::OpenPopup("Alternatives");
        }

        if (const auto popup = ImSweet::Popup("Alternatives")) {

          for (std::size_t i = 0; i < sizeof...(Is); ++i) {
            bool is_selected = n == type_names[i];
            if (default_constructible[i]) {
              if (ImGui::Selectable(type_names[i].data(), is_selected))
                constructors[i](static_cast<void*>(&v));
              if (is_selected)
                ImGui::SetItemDefaultFocus();
            }
            else {
              const ImSweet::ID id(static_cast<int>(i));
              ImGui::TextDisabled("%s", type_names[i].data()); 
              if (ImGui::IsItemHovered()) {
                details::red_tooltip("This alternative is not default-constructible!");
              }
            }
          }
        }

        ImGui::SameLine();
        details::Text("Current Type: ");
        ImGui::SameLine();
        details::Text(n);
      };

      visit([&non_templated](auto& e) { non_templated(type_name<std::remove_cvref_t<decltype(e)>>); }, v);
    }
    else {
      const auto non_templated = [&](const std::string_view n) {
        details::grey_button("+", R"(
Cannot change alternative for const variants.
)");
        ImGui::SameLine();
        details::Text("Current Type: ");
        ImGui::SameLine();
        details::Text(n);
      };

      visit([&non_templated](auto& e) { non_templated(type_name<decltype(e)>); }, v);
    }

    visit([&name](auto& e) { ImInspect::do_inspection(e, name); }, v);
  }

  template<typename Iter, typename Sen, std::size_t... Is>
  void print_asscoiative_range(Iter begin, const Sen end, const std::string& name)
  {
    for (int i = 0; begin != end; ++begin) {
      auto&& [k, v] = *begin;
      ImSweet::ID id(i++);
      using ImInspect::to_string;
      auto&& s = to_string(k);
      ImInspect::do_inspection(v, static_cast<const std::string&>(s));
    }
  }

  template<typename Iter, typename Sen, std::size_t... Is>
  void print_range(Iter begin, const Sen end, const std::string& name)
  {
    int i = 0;
    for (; begin != end; ++begin) {
      ImSweet::ID id(i);
      ImInspect::do_inspection(*begin, "[" + std::to_string(i) + "]");
      ++i;
    }
  }

  void print_more_container_info(std::size_t count);

  template<typename C>
  void print_container(C& c, const std::string& name)
  {
    static_assert(std::ranges::range<C>);

    const auto tree = ImSweet::TreeNode(name.c_str());
    if (ImGui::IsItemHovered())
      details::type_tooltip(type_name<C>);
    if (tree) {

      if (ImGui::Button("Show Info")) {
        ImGui::OpenPopup("InfoPopup");
      }

      if (const auto popup = ImSweet::Popup("InfoPopup")) {
        details::print_more_container_info(std::ranges::size(c));
      }

      ImGui::SameLine();
      static constexpr const char* creation_button_name = "Emplace Back";
      if constexpr (requires { c.emplace_back(); } && std::is_default_constructible_v<typename C::value_type>) {
        if (details::green_button(creation_button_name)) {
          c.emplace_back();
        }
      }
      else {
        details::grey_button(creation_button_name, "This container does not emplacing elements at the end.");
      }

      auto       begin = std::ranges::begin(c);
      const auto end   = std::ranges::end(c);

      for (int i = 0; begin != end; ++begin, ++i) {
        ImSweet::ID id(i);

        if constexpr (requires { c.erase(begin); }) {
          if (details::red_button("-")) {
            c.erase(begin);
            break;
          }
        }
        else {
          details::grey_button("-", "This container does not support erasing elements.");
        }

        ImGui::SameLine();
        ImInspect::do_inspection(*begin, "[" + std::to_string(i) + "]");
      }
    }
  }

  template<typename T, std::size_t... Is>
  void inspect_aggregate(T& tuple, std::index_sequence<Is...>)
  {
    auto f = [](auto& e, const std::size_t i) {
      ImSweet::ID id(i);
      const auto      n = lahzam::member_names<T>[i];
      using E           = std::remove_cvref_t<decltype(e)>;
      if constexpr (lahzam::reflectable<E>) {
        if constexpr (lahzam::member_count<E> > 1) {
          const auto tree = ImSweet::TreeNode(n.data());
          if (ImGui::IsItemHovered()) {
            details::type_tooltip(type_name<E>);
          }
          if (tree) {
            ImInspect::do_inspection(e, "");
          }
        }
        else {
          ImInspect::do_inspection(e, std::string(n));
        }
      }
      else {
        ImInspect::do_inspection(e, std::string(n));
      }
      return '\0';
    };
    const char c[] = {f(lahzam::get<Is>(tuple), Is)..., 0};
    (void)c;
  }

} // namespace details


template<typename T>
struct inspect;

template<typename T>
void do_inspection(const T& t, const std::string& name)
{
  static_assert(!std::is_volatile_v<T>);
  if constexpr (requires { inspect<T>{}(t, name); }) {
    inspect<T>{}(t, name);
  }
  else if constexpr (std::is_enum_v<T>) {
    if constexpr (is_opaque_enum<T>) {
      const auto v = static_cast<std::underlying_type_t<T>>(t);
      ImInspect::do_inspection(v, name);
    }
    else {
      auto v = t;
      if constexpr (enchantum::is_bitflag<T>)
        details::EnumCheckboxFlags(name, v);
      else
        details::EnumListBox(name, v);
    }
  }
  else if constexpr (std::ranges::range<T>) {
    if constexpr (details::AssociativeContainer<T>) {
      details::print_asscoiative_range(std::ranges::begin(t), std::ranges::end(t), name);
    }
    else if constexpr (requires {
                         t.string();
                         t.stem();
                         t.extension();
                         requires(enchantum::raw_type_name<T>.find("std::") != std::size_t(-1));
                       }) {
      ImInspect::do_inspection(std::string_view(t.string()), name);
    }
    else {
      details::print_container(t, name);
    }
  }
  else if constexpr (details::TupleLike<T>) {
    details::print_tuple(t, name, std::make_index_sequence<std::tuple_size_v<T>>{});
  }
  else if constexpr (details::VariantLike<T>) {
    details::print_variant(t, name, std::make_index_sequence<std::variant_size_v<T>>{});
  }
  else if constexpr (details::OptionalLike<T>) {
    details::inspect_optional(t, name);
  }
  else if constexpr (details::PointerLike<T>) {
    details::inspect_pointer(t, name);
  }
  else if constexpr (std::is_empty_v<T>) {
    ImGui::Text("{ this is an empty type }");
  }
  else if constexpr (requires { std::formatter<T>{}; }) {
    ImInspect::do_inspection(std::format("{}", t), name);
  }
  else if constexpr (lahzam::reflectable<T>) {
    (void)name;
    details::inspect_aggregate(t, std::make_index_sequence<lahzam::member_count<T>>{});
  }
  else {
    static_assert(sizeof(T) == 0, "cannot format type please specialize inspect<T>.");
  }
}

template<typename T>
void do_inspection(T& t, const std::string& name)
{
  static_assert(!std::is_volatile_v<T>);


  constexpr int           max_depth = 32;
  thread_local static int depth     = 0;

  if (depth >= max_depth) {
    ImGui::Text("%s: <maximum depth count reached>", name.c_str());
    return;
  }

  ++depth;

  if constexpr (requires { inspect<T>{}(t, name); }) {
    inspect<T>{}(t, name);
  }
  else if constexpr (std::is_enum_v<T>) {
    if constexpr (is_opaque_enum<T>) {
      auto v = static_cast<std::underlying_type_t<T>>(t);
      details::modify_numeric_int(static_cast<void*>(&v), name, sizeof(v), std::is_unsigned_v<decltype(v)>, type_name<T>);
      t = T(v);
    }
    else if constexpr (enchantum::is_bitflag<T>) {
      details::EnumCheckboxFlags(name.c_str(), t);
    }
    else {
      details::EnumListBox<T>(name.c_str(), t);
    }
  }
  else if constexpr (!std::is_same_v<T, long double> && std::is_floating_point_v<T>) {
    details::modify_numeric_float(static_cast<void*>(&t), name, std::is_same_v<T, double>, type_name<T>);
  }
  else if constexpr (std::is_integral_v<T>) {
    details::modify_numeric_int(static_cast<void*>(&t), name, sizeof(T), std::is_unsigned_v<T>, type_name<T>);
  }
  else if constexpr (std::ranges::range<T>) {
    if constexpr (details::AssociativeContainer<T>) {
      details::print_asscoiative_range(std::ranges::begin(t), std::ranges::end(t), name);
    }
    else if constexpr (requires {
                         t.string();
                         t.stem();
                         t.extension();
                         requires(enchantum::raw_type_name<T>.find("std::") != std::size_t(-1));
                       }) {
      details::inspect_filesystem_path(static_cast<void*>(&t), name);
    }
    else {
      details::print_container(t, name);
    }
  }
  else if constexpr (details::TupleLike<T>) {
    details::print_tuple(t, name, std::make_index_sequence<std::tuple_size_v<T>>{});
  }
  else if constexpr (details::VariantLike<T>) {
    details::print_variant(t, name, std::make_index_sequence<std::variant_size_v<T>>{});
  }
  else if constexpr (details::OptionalLike<T>) {
    details::inspect_optional(t, name);
  }
  else if constexpr (details::PointerLike<T>) {
    details::inspect_pointer(t, name);
  }
  else if constexpr (std::is_empty_v<T>) {
    ImGui::Text("{ this is an empty type }");
  }
  else if constexpr (lahzam::reflectable<T>) {
    (void)name;
    details::inspect_aggregate(t, std::make_index_sequence<lahzam::member_count<T>>{});
  }
  else {
    static_assert(sizeof(T) == 0, "cannot inspect type please specialize inspect<T>.");
  }

  --depth;
}

template<typename T>
struct default_value {
  T operator()() const { return T{}; }
};

} // namespace ImInspect
