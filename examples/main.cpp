#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <enchantum/bitwise_operators.hpp>
#include <filesystem>
#include "imentt.hpp"
#include <map>
#include <optional>
#include <stdio.h>
#include <tuple>
#include <variant>
#include <vector>

struct VoidPtr {
  void*                a{};
  const void*          b{&a};
  volatile void*       c{&a};
  const volatile void* d{&c};
};

struct NoDefConstructor {
  NoDefConstructor() = delete;
  NoDefConstructor(int) {}
};

struct OptNoDef {
  std::optional<NoDefConstructor> d;
};

template<>
struct ImInspect::default_value<NoDefConstructor> {
  NoDefConstructor operator()() const noexcept { return {0}; }
};
enum class Abilities : uint32_t {
  None         = 0,
  Fly          = 1 << 0,
  Swim         = 1 << 1,
  Climb        = 1 << 2,
  Invisibility = 1 << 3,
  Teleport     = 1 << 4,
};

ENCHANTUM_DEFINE_BITWISE_FOR(Abilities);
static_assert(enchantum::is_bitflag<Abilities>);

enum class Permissions : uint32_t {
  Read    = 1 << 0,
  Write   = 1 << 1,
  Execute = 1 << 2,
  Delete  = 1 << 3,
  Admin   = 1 << 4,
};

ENCHANTUM_DEFINE_BITWISE_FOR(Permissions);
static_assert(enchantum::is_bitflag<Permissions>);

enum class Faction {
  Neutral,
  Friendly,
  Hostile
};


struct Health {
  int  current;
  int  max;
  int* hate{};
};

struct Stats {
  Health  health;
  int     stamina;
  Faction faction;
};

struct Inventory {
  std::vector<std::string> items;
};

struct Target {
  std::optional<entt::entity> target;
};

struct Attributes {
  std::map<std::string, float> attributes;
};

struct State {
  std::variant<std::monostate, int, std::string, NoDefConstructor> value;
};

struct Character {
  std::string      name;
  Stats            stats;
  Inventory        inventory;
  Attributes       attributes;
  bool             alive = false;
  std::string_view type_name{"Character"};
};

struct Transform {
  std::tuple<float, float, float> position;
  std::tuple<float, float, float> rotation;
  std::tuple<float, float, float> scale;

  using T = std::tuple<float, float, float, float>;
  std::tuple<T, T, T, T> as_matrix{};

  using U = std::tuple<T, T, T, T>;
  std::tuple<U, U, U, U> fully_intern{};
};

struct Position {
  float x, y;
};

struct Velocity {
  float dx, dy;
};

struct VVelocity {
  std::array<float, 2> components{};
};

struct VVVelocity {
  Velocity dx, dy;
};

struct WeirdVelocity {
  const int dx{};
};

struct Name {
  std::string name;
};

struct Constructor {
  Constructor(int x) : x(x) {}
  int x;
};

enum class EntityType {
  Zombie,
  Player,
  Skeleton
};

struct A;

struct B {
  A*  a;
  int value = 2;
};

struct A {
  B* b;
};
template<>
struct ImInspect::inspect<Constructor> {
  void operator()(Constructor& c, const std::string& name) const { ImGui::DragInt("X", &c.x); }
};

template<>
struct ImInspect::default_value<Constructor> {
  auto operator()() const { return Constructor{0}; }
};

static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main()
{
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  // Decide GL+GLSL versions
#if defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

  // Create window with graphics context
  GLFWwindow* window = glfwCreateWindow(1280, 800, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
  if (window == nullptr)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Our state
  bool   show_demo_window    = true;
  bool   show_another_window = false;
  ImVec4 clear_color         = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  entt::registry registry;

  auto entity = registry.create();
  registry.emplace<Position>(entity, 100.f, 200.f);
  registry.emplace<Velocity>(entity, 1.0f, -0.5f);
  registry.emplace<VVelocity>(entity, 1.0f, -0.5f);
  registry.emplace<VVVelocity>(entity, VVVelocity{});
  registry.emplace<Constructor>(entity, Constructor{3});

  registry.emplace<Name>(entity, Name{"Hero"});
  registry.emplace<EntityType>(entity, EntityType::Player);

  registry.emplace<Health>(entity, Health{75, 100});
  registry.emplace<Stats>(entity, Stats{Health{50, 100}, 80, Faction::Friendly});
  using SPair = std::pair<std::string, int>;
  auto& inv   = registry.emplace<Inventory>(entity);
  inv.items.emplace_back("Sword");
  inv.items.emplace_back("Shield");
  inv.items.emplace_back("Bow");

  registry.emplace<Target>(entity, std::nullopt);
  registry.emplace<Attributes>(entity, Attributes{{{"Strength", 10.0f}, {"Agility", 7.5f}}});
  registry.emplace<State>(entity, State{std::string{"Idle"}});

  registry.emplace<Character>(entity,
                              Character{"Archer",
                                        Stats{Health{80, 100}, 60, Faction::Neutral},
                                        inv,
                                        Attributes{{{"Focus", 9.0f}}}});

  registry.emplace<Transform>(entity,
                              Transform{
                                {0.f, 1.f, 0.f}, // position
                                {0.f, 0.f, 0.f}, // rotation
                                {1.f, 1.f, 1.f}  // scale
                              });


  static A a;
  static B b{&a};
  a = {&b};
  registry.emplace<A>(entity, a);
  //registry.emplace<B>(entity, B{&a});


  ImEnTT::Editor<entt::registry> editor;
  editor.entityTitle = [](auto& registry, entt::entity e) {
    if (const Name* name = registry.try_get<Name>(e)) {
      return std::format("Entity '{}'", name->name);
    }
    else {
      return std::format("Entity '{}'", entt::to_integral(e));
    }
  };
  editor.register_component<Position>();
  editor.register_component<Velocity>();
  editor.register_component<VVelocity>();
  editor.register_component<VVVelocity>();
  editor.register_component<Constructor>();
  editor.register_component<Name>();
  editor.register_component<EntityType>();

  editor.register_component<std::filesystem::path>();
  editor.register_component<Health>();
  editor.register_component<Stats>();
  editor.register_component<Inventory>();
  editor.register_component<Target>();
  editor.register_component<Attributes>();
  editor.register_component<State>();
  editor.register_component<Character>();
  editor.register_component<Transform>();
  editor.register_component<Abilities>();
  editor.register_component<std::string>();

  editor.register_component<OptNoDef>();
  editor.register_component<Permissions>();
  editor.register_component<int>();

  editor.register_component<WeirdVelocity>();
  editor.register_component<A>();
  editor.register_component<B>();
  editor.register_component<VoidPtr>();


  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    editor.render(registry);
    //editor.draw(registry);


    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w,
                 clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w,
                 clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}