#pragma once

#include <algorithm>
#include <cstddef>
#include <entt/entity/registry.hpp>
#include <format>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <iminspect.hpp>
#include <imsweet/std_format.hpp>
#include <locale>
#include <ranges>
#include <vector>

namespace ImInspect {
template<>
inline constexpr bool is_opaque_enum<entt::entity> = true;
} // namespace ImInspect

namespace ImEnTT {

namespace details {

  template<typename Entity, typename Alloc>
  Entity clone_entity(entt::basic_registry<Entity, Alloc>& registry, const Entity original)
  {
    const auto e = registry.create();
    // create a copy of an entity component by component
    // I am not sure if this is the best way
    for (auto&& curr : registry.storage()) {
      if (auto& storage = curr.second; storage.contains(original)) {
        storage.push(e, storage.value(original));
      }
    }
    return e;
  }

} // namespace details

template<typename Registry>
struct BasicComponentMeta {
  std::string name;
  BasicComponentMeta(std::string name) : name(static_cast<std::string&&>(name)) {}


  virtual ~BasicComponentMeta() = default;
  using entity_type             = typename Registry::entity_type;

  virtual bool has_component(const Registry& registry, entity_type entity) const = 0;
  virtual void remove_component(Registry& registry, entity_type entity) const    = 0;

  // successfully can added
  virtual bool add_component_menu(Registry& registry, entity_type entity) const = 0;
  virtual void draw(Registry& registry, entity_type entity) const               = 0;
};

template<typename Registry, typename Component>
struct ComponentMeta : BasicComponentMeta<Registry> {
  using Base = BasicComponentMeta<Registry>;
  using Base::Base;
  using Base::name;
  using typename Base::entity_type;
  bool has_component(const Registry& registry, entity_type entity) const override
  {
    return registry.template all_of<Component>(entity);
  }

  void remove_component(Registry& registry, entity_type entity) const override
  {
    registry.template remove<Component>(entity);
  }

  bool add_component_menu(Registry& registry, entity_type entity) const override
  {
    if (!registry.template all_of<Component>(entity)) {
      ImSweet::ID id(name);
      const bool  clicked = ImGui::Selectable(" ");
      ImInspect::colored_pretty_typename(ImInspect::normalize_type_name(name), 0.0f);
      if (clicked) {
        registry.template emplace<Component>(entity, ImInspect::default_value<Component>{}());
        ImGui::CloseCurrentPopup();
      }
      return true;
    }
    return false;
  }

  void draw(Registry& registry, entity_type entity) const override
  {
    if (auto* comp = registry.template try_get<Component>(entity)) {
      // square size matching the header height
      const float size = ImGui::GetFrameHeight();

      if (ImGui::Button("-", ImVec2(size, size))) {
        remove_component(registry, entity);
        return;
      }

      ImGui::SameLine();

      ImGui::BeginGroup();
      ImSweet::ID id(name);

      const auto clicked = ImGui::CollapsingHeader("");
      ImGui::SameLine();
      const auto normalized = ImInspect::normalize_type_name(name);
      ImInspect::colored_pretty_typename(normalized, 0.0f);
      if (clicked) {
        //ImGui::Indent(size + ImGui::GetStyle().ItemSpacing.x);
        ImInspect::do_inspection(*comp, normalized);
        //ImGui::Unindent(size + ImGui::GetStyle().ItemSpacing.x);
      }
      ImGui::EndGroup();
    }
  }
};

template<typename Registry = entt::registry>
class Editor {
public:
  using registry    = Registry;
  using entity_type = typename registry::entity_type;

  std::function<std::string(registry& r, entity_type e)> entityTitle{};
public:
  Editor(std::string name = "Entt Editor") : mName(static_cast<std::string&&>(name)) {}

  template<typename Component>
  void register_component(const std::string& name = std::string(ImInspect::type_name<Component>))
  {
    for (const auto& comp : mComponents) {
      assert(comp->name != name && "name already registered");
      (void)comp;
    }
    mComponents.emplace_back(new ComponentMeta<Registry, Component>(name));
  }

  void render(registry& registry)
  {
    ImGui::Begin(mName.c_str());

    ImGui::BeginChild("LeftPanel", ImVec2(200, 0), true);
    ImGui::Text("Component Filters");
    ImGui::Separator();

    {

      const auto totalEntities = registry.view<entity_type>().size();

      static const std::locale locale("en_US.UTF-8");
      ImSweet::Text("{}", std::format(locale, "Entities: {:L}", totalEntities));

      for (std::size_t i = 0; i < mComponents.size(); ++i) {
        auto& comp = *mComponents[i];

        bool              enabled = std::ranges::find(mEnabledComponents, i) != mEnabledComponents.end();
        const ImSweet::ID id(comp.name);
        const auto        clicked = ImGui::Checkbox(" ", &enabled);
        ImInspect::colored_pretty_typename(ImInspect::normalize_type_name(comp.name), 0.0f);
        if (ImGui::IsItemHovered()) {
          std::size_t matchingEntities = 0;
          for (const auto e : registry.view<entity_type>())
            if (comp.has_component(registry, e))
              ++matchingEntities;
          const float percentage = totalEntities > 0 ? (float(matchingEntities) / totalEntities) * 100.0f : 0.0f;

          const std::string label = std::format("{} ({:.1f}%) entities have this component", matchingEntities, percentage);
          ImGui::SetTooltip("%s", label.c_str());
        }
        if (clicked) {
          if (enabled)
            mEnabledComponents.push_back(i);
          else
            std::erase(mEnabledComponents, i);
        }
      }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // RIGHT PANEL: Entity View
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);

    if (ImGui::Button("Create"))
      (void)registry.create();

    for (const auto entity : registry.view<entity_type>()) {
      bool show = true;
      for (const std::size_t idx : mEnabledComponents) {
        if (!mComponents[idx]->has_component(registry, entity)) {
          show = false;
          break;
        }
      }
      if (!show)
        continue;

      auto       id    = entt::to_integral(entity);
      const auto label = entityTitle ? entityTitle(registry, entity) : std::format("Entity {}", id);
      if (const auto tree = ImSweet::TreeNode(label.c_str())) {
        const ImSweet::ID imguiid(static_cast<int>(id));
        ImGui::SameLine();
        if (ImGui::Button("Clone")) {
          details::clone_entity(registry, entity);
          continue;
        }

        ImGui::SameLine();
        if (ImGui::Button("Add Component")) {
          ImGui::OpenPopup("Available Components");
        }

        if (ImGui::BeginPopup("Available Components")) {
          bool can_add = false;
          for (const auto& meta : mComponents)
            can_add = meta->add_component_menu(registry, entity) || can_add;
          if (!can_add) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            const auto tooltip = std::format("All {} components have been already added!", mComponents.size());
            ImGui::TextUnformatted(tooltip.data(), tooltip.data() + tooltip.size());
            ImGui::PopStyleColor();
          }
          ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::Dummy({10, 0});
        ImGui::SameLine();
        if (ImInspect::details::red_button("Delete")) {
          registry.destroy(entity);
          continue;
        }

        for (const auto& meta : mComponents) {
          ImSweet::ID id(meta->name);
          meta->draw(registry, entity);
        }
      }
    }

    ImGui::EndChild();
    ImGui::End();
  }

private:
  std::string                                                mName;
  std::vector<std::unique_ptr<BasicComponentMeta<registry>>> mComponents;
  std::vector<std::size_t>                                   mEnabledComponents;
};

} // namespace ImEnTT
