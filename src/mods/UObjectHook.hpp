#pragma once

#include <filesystem>
#include <shared_mutex>
#include <unordered_set>
#include <memory>
#include <deque>
#include <future>

#include <nlohmann/json.hpp>

#include <safetyhook.hpp>

#include "Mod.hpp"

namespace sdk {
class UObjectBase;
class UObject;
class UClass;
class FFieldClass;
class FStructProperty;
class UScriptStruct;
class USceneComponent;
class UActorComponent;
class AActor;
class FArrayProperty;
}

class UObjectHook : public Mod {
public:
    static std::shared_ptr<UObjectHook>& get();

    std::unordered_set<sdk::UObjectBase*> get_objects_by_class(sdk::UClass* uclass) const {
        std::shared_lock _{m_mutex};
        if (auto it = m_objects_by_class.find(uclass); it != m_objects_by_class.end()) {
            return it->second;
        }

        return {};
    }

    bool exists(sdk::UObjectBase* object) const {
        std::shared_lock _{m_mutex};
        return exists_unsafe(object);
    }

    void activate();

    bool is_disabled() const {
        return m_uobject_hook_disabled;
    }

    void set_disabled(bool disabled) {
        m_uobject_hook_disabled = disabled;
    }

protected:
    std::string_view get_name() const override { return "UObjectHook"; };
    bool is_advanced_mod() const override { return true; }

    std::vector<SidebarEntryInfo> get_sidebar_entries() override { 
        return {
            { "Main", true },
            { "Config", false },
            { "Developer", true }
        };
    }

    void on_config_load(const utility::Config& cfg, bool set_defaults) override;
    void on_config_save(utility::Config& cfg) override;

    void on_pre_engine_tick(sdk::UGameEngine* engine, float delta) override;
    void on_frame() override;
    void on_draw_sidebar_entry(std::string_view in_entry) override;
    void on_draw_ui() override;

    void draw_config();
    void draw_developer();
    void draw_main();

    void on_pre_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                             const float world_to_meters, Vector3f* view_location, bool is_double) override;

    void on_post_calculate_stereo_view_offset(void* stereo_device, const int32_t view_index, Rotator<float>* view_rotation, 
                                                      const float world_to_meters, Vector3f* view_location, bool is_double) override;

public:
    struct MotionControllerStateBase {
        nlohmann::json to_json() const;
        void from_json(const nlohmann::json& data);

        MotionControllerStateBase& operator=(const MotionControllerStateBase& other) = default;

        // State that can be parsed from disk
        glm::quat rotation_offset{glm::identity<glm::quat>()};
        glm::vec3 location_offset{0.0f, 0.0f, 0.0f};
        uint8_t hand{1};
        bool permanent{false};
    };

    // Assert if MotionControllerStateBase is not trivially copyable
    static_assert(std::is_trivially_copyable_v<MotionControllerStateBase>);
    static_assert(std::is_trivially_destructible_v<MotionControllerStateBase>);
    static_assert(std::is_standard_layout_v<MotionControllerStateBase>);

    struct MotionControllerState final : MotionControllerStateBase {
        ~MotionControllerState();

        MotionControllerState& operator=(const MotionControllerStateBase& other) {
            MotionControllerStateBase::operator=(other);
            return *this;
        }

        operator MotionControllerStateBase&() {
            return *this;
        }

        // In-memory state
        sdk::AActor* adjustment_visualizer{nullptr};
        bool adjusting{false};
    };

    std::shared_ptr<MotionControllerState> get_or_add_motion_controller_state(sdk::USceneComponent* component) {
        {
            std::shared_lock _{m_mutex};
            if (auto it = m_motion_controller_attached_components.find(component); it != m_motion_controller_attached_components.end()) {
                return it->second;
            }
        }

        std::unique_lock _{m_mutex};
        auto result = std::make_shared<MotionControllerState>();
        return m_motion_controller_attached_components[component] = result;

        return result;
    }

    std::optional<std::shared_ptr<MotionControllerState>> get_motion_controller_state(sdk::USceneComponent* component) {
        std::shared_lock _{m_mutex};
        if (auto it = m_motion_controller_attached_components.find(component); it != m_motion_controller_attached_components.end()) {
            return it->second;
        }

        return {};
    }

private:
    struct StatePath;
    struct PersistentState;
    struct PersistentCameraState;
    struct PersistentProperties;

    bool exists_unsafe(sdk::UObjectBase* object) const {
        return m_objects.contains(object);
    }

    void hook();
    void add_new_object(sdk::UObjectBase* object);

    void tick_attachments(
        Rotator<float>* view_rotation, const float world_to_meters, Vector3f* view_location, bool is_double
    );

    void ui_handle_object(sdk::UObject* object);
    void ui_handle_properties(void* object, sdk::UStruct* definition);
    void ui_handle_array_property(void* object, sdk::FArrayProperty* definition);
    void ui_handle_functions(void* object, sdk::UStruct* definition);
    void ui_handle_struct(void* addr, sdk::UStruct* definition);

    void ui_handle_scene_component(sdk::USceneComponent* component);
    void ui_handle_material_interface(sdk::UObject* object);
    void ui_handle_actor(sdk::UObject* object);

    void spawn_overlapper(uint32_t hand = 0);
    void destroy_overlapper();

    std::optional<StatePath> try_get_path(sdk::UObject* target) const;

    static inline const std::vector<std::string> s_allowed_bases {
        "Acknowledged Pawn",
        "Player Controller",
        "Camera Manager",
        "World"
    };

    static std::filesystem::path get_persistent_dir();
    nlohmann::json serialize_mc_state(const std::vector<std::string>& path, const std::shared_ptr<MotionControllerState>& state);
    nlohmann::json serialize_camera(const std::vector<std::string>& path);
    void save_camera_state(const std::vector<std::string>& path);
    std::optional<StatePath> deserialize_path(const nlohmann::json& data);
    std::shared_ptr<PersistentState> deserialize_mc_state(nlohmann::json& data);
    std::shared_ptr<PersistentState> deserialize_mc_state(std::filesystem::path json_path);
    std::vector<std::shared_ptr<PersistentState>> deserialize_all_mc_states();
    std::shared_ptr<PersistentCameraState> deserialize_camera(const nlohmann::json& data);
    std::shared_ptr<PersistentCameraState> deserialize_camera_state();
    void update_persistent_states();
    void update_motion_controller_components(const glm::vec3& left_hand_location, const glm::vec3& left_hand_euler,
                                             const glm::vec3& right_hand_location, const glm::vec3& right_hand_euler);

    static void* add_object(void* rcx, void* rdx, void* r8, void* r9);
    static void* destructor(sdk::UObjectBase* object, void* rdx, void* r8, void* r9);

    bool m_hooked{false};
    bool m_fully_hooked{false};
    bool m_wants_activate{false};
    float m_last_delta_time{1000.0f / 60.0f};

    struct DebugInfo {
        uint64_t constructor_calls{0};
        uint64_t destructor_calls{0};
    } m_debug{};

    glm::vec3 m_last_left_grip_location{};
    glm::vec3 m_last_right_grip_location{};
    glm::quat m_last_left_aim_rotation{glm::identity<glm::quat>()};
    glm::quat m_last_right_aim_rotation{glm::identity<glm::quat>()};

    mutable std::shared_mutex m_mutex{};

    struct MetaObject {
        std::wstring full_name{};
        sdk::UClass* uclass{nullptr};
    };

    std::unordered_set<sdk::UObjectBase*> m_objects{};
    std::unordered_map<sdk::UObjectBase*, std::unique_ptr<MetaObject>> m_meta_objects{};
    std::unordered_map<sdk::UClass*, std::unordered_set<sdk::UObjectBase*>> m_objects_by_class{};

    std::deque<std::unique_ptr<MetaObject>> m_reusable_meta_objects{};

    SafetyHookInline m_add_object_hook{};
    SafetyHookInline m_destructor_hook{};

    std::chrono::steady_clock::time_point m_last_sort_time{};
    std::vector<sdk::UClass*> m_sorted_classes{};
    std::future<std::vector<sdk::UClass*>> m_sorting_task{};

    std::unordered_map<sdk::UClass*, std::function<void (sdk::UObject*)>> m_on_creation_add_component_jobs{};

    std::deque<sdk::UObject*> m_most_recent_objects{};
    std::unordered_set<sdk::UObject*> m_motion_controller_attached_objects{};

    std::unordered_map<sdk::USceneComponent*, std::shared_ptr<MotionControllerState>> m_motion_controller_attached_components{};
    sdk::AActor* m_overlap_detection_actor{nullptr};
    sdk::AActor* m_overlap_detection_actor_left{nullptr};

    struct CameraState {
        sdk::UObject* object{nullptr};
        glm::vec3 offset{};
    } m_camera_attach{};

    void remove_motion_controller_state(sdk::USceneComponent* component) {
        std::unique_lock _{m_mutex};
        m_motion_controller_attached_components.erase(component);
    }

    auto get_spawned_spheres() const {
        std::shared_lock _{m_mutex};
        return m_spawned_spheres;
    }
    
    std::unordered_set<sdk::USceneComponent*> m_spawned_spheres{};
    std::unordered_set<sdk::USceneComponent*> m_components_with_spheres{};
    std::unordered_map<sdk::USceneComponent*, sdk::USceneComponent*> m_spawned_spheres_to_components{};

    struct ResolvedObject {
    public:
        ResolvedObject() = default;
        ResolvedObject(void* data, sdk::UStruct* definition) : data{data}, definition{definition} {}
        ResolvedObject(std::nullptr_t) : data{nullptr}, definition{nullptr} {}

        operator sdk::UObject*() const noexcept {
            return object;
        }

        operator void*() const noexcept {
            return data;
        }

        bool operator==(void* other) const noexcept {
            return data == other;
        }

        bool operator==(sdk::UObject* other) const noexcept {
            return object == other;
        }

        bool operator==(std::nullptr_t) const noexcept {
            return data == nullptr;
        }

        bool operator!=(std::nullptr_t) const noexcept {
            return data != nullptr;
        }

        template<typename T>
        T as() const noexcept {
            return (T)data;
        }

        template<typename T>
        T as() noexcept {
            return (T)data;
        }

    public:
        union {
            void* data{nullptr};
            sdk::UObject* object;
        };

        sdk::UStruct* definition{nullptr};
        bool is_object{false};
    };

    class StatePath {
    public:
        struct PathScope {
            PathScope(const PathScope&) = delete;
            PathScope& operator=(const PathScope&) = delete;

            PathScope(PathScope&& other) noexcept : m_path(other.m_path) {
                other.m_moved = true;
            }

            PathScope& operator=(PathScope&& other) noexcept {
                if (this != &other) {
                    m_path = other.m_path;
                    other.m_moved = true;
                }
                return *this;
            }

            PathScope(StatePath& path, const std::string& name) : m_path{path} {
                m_path.push(name);
            }

            ~PathScope() {
                if (!m_moved) {
                    m_path.m_path.pop_back();
                }
            }

        private:
            StatePath& m_path;
            bool m_moved{false};
        };

        StatePath() = default;
        StatePath(const std::vector<std::string>& path) : m_path{path} {}

        StatePath& operator=(const std::vector<std::string>& path) {
            m_path = path;
            return *this;
        }

        const auto& path() const {
            return m_path;
        }

        PathScope enter(const std::string& name) {
            return PathScope{*this, name};
        }

        PathScope enter_clean(const std::string& name) {
            clear();
            return PathScope{*this, name};
        }

        bool has_valid_base() const {
            if (m_path.empty()) {
                return false;
            }

            return std::find(s_allowed_bases.begin(), s_allowed_bases.end(), m_path[0]) != s_allowed_bases.end();
        }

        sdk::UObject* resolve_base_object() const;
        ResolvedObject resolve()  const;

    private:
        void clear() {
            m_path.clear();
        }

        void push(const std::string& name) {
            m_path.push_back(name);
        }

        void pop() {
            m_path.pop_back();
        }

        std::vector<std::string> m_path{};
    } m_path;

    struct JsonAssociation {
        std::optional<std::filesystem::path> path_to_json{};
        void erase_json_file() const {
            if (path_to_json.has_value() && std::filesystem::exists(*path_to_json)) {
                std::filesystem::remove(*path_to_json);
            }
        }
    };

    struct PersistentState : JsonAssociation {
        StatePath path{};
        MotionControllerStateBase state{};
        sdk::USceneComponent* last_object{nullptr};
    };

    struct PersistentCameraState : JsonAssociation {
        StatePath path{};
        glm::vec3 offset{};
    };

    struct PersistentProperties : JsonAssociation {
        void save_to_file(std::optional<std::filesystem::path> path = std::nullopt);
        nlohmann::json to_json() const;
        static std::shared_ptr<PersistentProperties> from_json(std::filesystem::path json_path);
        static std::shared_ptr<PersistentProperties> from_json(const nlohmann::json& j);
        
        StatePath path{};

        struct PropertyState {
            std::wstring name{};
            union {
                uint64_t u64;
                double d;
                float f;
                int32_t i;
                bool b;
            } data;
        };

        std::vector<std::shared_ptr<PropertyState>> properties{};
        bool hide{false};
        bool hide_legacy{false};
    };

    glm::vec3 m_last_camera_location{};

    std::shared_ptr<PersistentCameraState> m_persistent_camera_state{};
    std::vector<std::shared_ptr<PersistentState>> m_persistent_states{};
    std::vector<std::shared_ptr<PersistentProperties>> m_persistent_properties{};

    void reload_persistent_states() {
        m_persistent_states = deserialize_all_mc_states();
        m_persistent_camera_state = deserialize_camera_state();
        m_persistent_properties = deserialize_all_persistent_properties();
    }

    void reset_persistent_states() {
        m_persistent_states.clear();
        m_persistent_properties.clear();
        m_persistent_camera_state.reset();
    }

    std::vector<std::shared_ptr<PersistentProperties>> deserialize_all_persistent_properties() const;

private:
    ModToggle::Ptr m_enabled_at_startup{ModToggle::create(generate_name("EnabledAtStartup"), false)};
    ModToggle::Ptr m_attach_lerp_enabled{ModToggle::create(generate_name("AttachLerpEnabled"), true)};
    ModSlider::Ptr m_attach_lerp_speed{ModSlider::create(generate_name("AttachLerpSpeed"), 0.01f, 30.0f, 15.0f)};

    ModKey::Ptr m_keybind_toggle_uobject_hook{ModKey::create(generate_name("ToggleUObjectHookKey"))};
    bool m_uobject_hook_disabled{false};
    bool m_fixed_visibilities{false};

public:
    UObjectHook() {
        m_options = {
            *m_enabled_at_startup,
            *m_attach_lerp_enabled,
            *m_attach_lerp_speed,
            *m_keybind_toggle_uobject_hook
        };
    }

private:
};