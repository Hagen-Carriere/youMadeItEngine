#ifndef ACTORDB_H
#define ACTORDB_H

#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <iostream>
#include <optional>

#include <glm/glm.hpp>
#include "rapidjson/document.h"
#include "TemplateDB.h"
#include "SDL2/SDL.h"
#include "SDL2_image/SDL_image.h"
#include "Helper.h"
#include "Input.h"
#include "SceneDB.h"
#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include "ImageDB.h"
#include "Tiled.h"

#include "box2d/box2d.h"


class ContactListener : public b2ContactListener {
public:
    void BeginContact(b2Contact* contact) override;
    void EndContact(b2Contact* contact) override;
};


class ComponentDB {
public:
    inline static int runtime_component_counter = 0;

    struct ComponentInfo {
        std::string type;
        std::shared_ptr<luabridge::LuaRef> luaRef;
        bool enabled = true;
        bool hasStarted = false;
    };


    void init(lua_State* state);
    std::shared_ptr<luabridge::LuaRef> loadComponent(std::string type);
    std::shared_ptr<luabridge::LuaRef> createInstance(const std::string& type, const std::string& key, const rapidjson::Value& properties);
    std::shared_ptr<luabridge::LuaRef> createInstance(const std::string& type, const std::string& key, const std::vector<std::string> props);
    void updateInstance(std::shared_ptr<luabridge::LuaRef> instance, const rapidjson::Value& properties, std::string typeIn);
    void callFunction(ComponentInfo& compInfo, const std::string& functionName, const std::string& actorName);
    void establishInheritance(luabridge::LuaRef& instanceTable, luabridge::LuaRef& parentTable);


    // Add debugLog function for Lua access
    static void debugLog(luabridge::LuaRef message);
    void ReportError(const std::string& actorName, const luabridge::LuaException& e);

    b2World* world = nullptr;
	ContactListener* contactListener = nullptr;

    void parseTypedKeyValue(const std::string& line, std::string& type, std::string& name, std::string& value) {
        size_t firstSpace = line.find(' ');
        size_t equals = line.find('=');

        if (firstSpace == std::string::npos || equals == std::string::npos) return;

        type = line.substr(0, firstSpace);
        name = line.substr(firstSpace + 1, equals - firstSpace - 1);
        value = line.substr(equals + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
            };

        trim(type);
        trim(name);
        trim(value);
    }


private:
    lua_State* lua_state = nullptr;
    luabridge::LuaRef new_metatable = nullptr;
    std::unordered_map<std::string, std::shared_ptr<luabridge::LuaRef>> componentScripts = {};
};




class actorDB {
public:
    void initActors(rapidjson::Document* in, bool tiledIn, SDL_Renderer* rendererIn, SceneDB::camera* camIn, lua_State* lIn);
    void updateAllActors();



    struct Actor {
        int id = -1;
        std::string name = "";
        std::string templateName = "";
		int gid = -1;
        std::vector<std::string> keys = {};
        std::vector<std::pair<std::string, ComponentDB::ComponentInfo>> pendingComponents = {};
        std::unordered_map<std::string, std::vector<std::string>> typeToKeys;
        std::unordered_map<std::string, ComponentDB::ComponentInfo> components = {};
        std::vector<std::string> componentsToRemove = {};
        inline static lua_State* lua_state = nullptr;

		std::vector<std::string> onStartComponents;
        std::vector<std::string> onUpdateComponents;
        std::vector<std::string> onLateUpdateComponents;

        bool dontDestroyOnLoad = false;

        Actor() = default;
        Actor(int id) : id(id) {}
        std::string GetName() const;
        int GetID() const;
		int GetGID() const { return gid; }
        luabridge::LuaRef GetComponentByKey(const std::string& key);
        luabridge::LuaRef GetComponent(const std::string& typeName);
        luabridge::LuaRef GetComponents(const std::string& typeName);
        luabridge::LuaRef AddComponent(const std::string& typeName, const std::string& key, const rapidjson::Value& props, bool immediate);
        luabridge::LuaRef AddComponent(const std::string& typeName, const std::string& key, const std::vector<std::string> props, bool immediate);
        luabridge::LuaRef AddComponentLua(const std::string& type);
        void RemoveComponent(const luabridge::LuaRef& componentRef);


    };

    static actorDB::Actor* Instantiate(const std::string& templateName);
    static void Destroy(actorDB::Actor* actor);



    static std::vector<actorDB::Actor*> FindAll(const std::string& name);
    static actorDB::Actor* Find(const std::string& name);

    static actorDB::Actor* Lua_Find(const std::string& name);
    static luabridge::LuaRef Lua_FindAll(const std::string& name);

    void destroyAllActors();




    std::vector<std::unique_ptr<Actor>> actorsList;
    std::vector<std::unique_ptr<Actor>> actorsSaved = {};
    static actorDB* instance;

    TemplateDB templateDB;
    ComponentDB componentManager;
	Tiled tiledManager;



private:
    SDL_Renderer* renderer = nullptr;
    SceneDB::camera* camPointer = nullptr;


    void initActor(Actor* actor, const rapidjson::Value& actorData, bool tiled);
    rapidjson::Document* getTemplate(const std::string& templateName);


    static bool isPendingDestruction(int actorID);

    std::unordered_map<std::string, rapidjson::Document> templates = {};
    std::vector<std::unique_ptr<Actor>> actorsToAdd;
    std::vector<int> actorsToDestroy = {};


    lua_State* lua_state = nullptr;

    inline static int actorIDCount = 0;

    std::shared_ptr<luabridge::LuaRef> createInstance(const std::string& type, const std::string& key, const rapidjson::Value& properties, actorDB::Actor* aIn, b2World* world);
    std::shared_ptr<luabridge::LuaRef> createInstance(const std::string& type, const std::string& key, const std::vector<std::string> props, actorDB::Actor* aIn, b2World* world);


};

struct Collision {
    actorDB::Actor* other;
    b2Vec2 point;
    b2Vec2 normal;
    b2Vec2 relative_velocity;
};


class Rigidbody {

public:
    b2Body* body = nullptr;
    actorDB::Actor* actor = nullptr;
    std::string collider_type = "box";
    std::string body_type = "dynamic";
    std::string key = "";
    float x = 0.0f;
    float y = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
    float radius = 0.5f;
    float friction = 0.3f;
    float bounciness = 0.3f;
    float gravity_scale = 1.0f;
    float density = 1.0f;
    float angular_friction = 0.3f;
    float rotation = 0.0f;
    bool enabled = true;
    bool precise = true;
    bool has_collider = true;
    bool has_trigger = true;
	std::string trigger_type = "box";
	float trigger_width = 1.0f;
	float trigger_height = 1.0f;
	float trigger_radius = 0.5f;

    Rigidbody() = default;
    void Init(b2World* world) {
        b2BodyDef bodyDef;
        // Set position and rotation (convert degrees to radians)
        bodyDef.position.Set(x, y);
        bodyDef.angle = rotation * (b2_pi / 180.0f); // convert to radians

        // Set body type
        if (body_type == "static") {
            bodyDef.type = b2_staticBody;
        }
        else if (body_type == "kinematic") {
            bodyDef.type = b2_kinematicBody;
        }
        else {
            bodyDef.type = b2_dynamicBody;
        }

        // Set other body properties
        bodyDef.bullet = precise;
        bodyDef.gravityScale = gravity_scale;
        bodyDef.angularDamping = angular_friction;

        // Create the body
        body = world->CreateBody(&bodyDef);

		if (!has_collider && !has_trigger) {
            // Define a basic box shape for the body
            b2PolygonShape boxShape;
            boxShape.SetAsBox(width * 0.5f, height * 0.5f);

            // Define fixture with user-specified density
            b2FixtureDef fixtureDef;
            fixtureDef.shape = &boxShape;
            fixtureDef.density = density;

			fixtureDef.isSensor = true;

            body->CreateFixture(&fixtureDef);
			return;
		}

        if(has_collider) {
            b2FixtureDef fixtureDef;
            fixtureDef.userData.pointer = reinterpret_cast<uintptr_t>(this->actor);

            fixtureDef.density = density;
            fixtureDef.friction = friction;
            fixtureDef.restitution = bounciness;
            fixtureDef.isSensor = false;

            if (collider_type == "circle") {
                b2CircleShape shape;
                shape.m_radius = radius;
                fixtureDef.shape = &shape;
                body->CreateFixture(&fixtureDef);
            }
            else { // default to box
                b2PolygonShape shape;
                shape.SetAsBox(width * 0.5f, height * 0.5f);
                fixtureDef.shape = &shape;
                body->CreateFixture(&fixtureDef);
            }
        }

		if (has_trigger) {
			b2FixtureDef triggerDef;
			triggerDef.userData.pointer = reinterpret_cast<uintptr_t>(this->actor);
			triggerDef.density = density;
			triggerDef.isSensor = true;
			if (trigger_type == "circle") {
				b2CircleShape shape;
				shape.m_radius = trigger_radius;
				triggerDef.shape = &shape;
				body->CreateFixture(&triggerDef);
			}
			else { // default to box
				b2PolygonShape shape;
				shape.SetAsBox(trigger_width * 0.5f, trigger_height * 0.5f);
				triggerDef.shape = &shape;
				body->CreateFixture(&triggerDef);
			}
		}
    }

    void OnStart() {
        if (actorDB::instance->componentManager.world) {
            Init(actorDB::instance->componentManager.world);
        }
    }


    b2Vec2 GetPosition() const {
        return body ? body->GetPosition() : b2Vec2(x, y);
    }

    float GetRotation() const {
        return body ? (body->GetAngle() * (180.0f / b2_pi)) : rotation;
    }

    b2Vec2 GetVelocity() const {
        return body ? body->GetLinearVelocity() : b2Vec2(0, 0);
    }

    float GetAngularVelocity() const {
        return body ? (body->GetAngularVelocity() * (180.0f / b2_pi)) : 0.0f;
    }

    float GetGravityScale() const {
        return body ? body->GetGravityScale() : gravity_scale;
    }

    b2Vec2 GetUpDirection() const {
        if (!body) return b2Vec2(0, -1);
        float angle = body->GetAngle();
        return b2Vec2(glm::sin(angle), -glm::cos(angle));
    }

    b2Vec2 GetRightDirection() const {
        if (!body) return b2Vec2(1, 0);
        float angle = body->GetAngle();
        return b2Vec2(glm::cos(angle), glm::sin(angle));
    }



    static float Vector2_Distance(const b2Vec2& a, const b2Vec2& b) {
        return b2Distance(a, b);
    }

    static float Vector2_Dot(const b2Vec2& a, const b2Vec2& b) {
        return b2Dot(a, b);
    }

    void AddForce(const b2Vec2& force) {
        if (body) body->ApplyForceToCenter(force, true);
    }

    void SetVelocity(const b2Vec2& velocity) {
        if (body) body->SetLinearVelocity(velocity);
    }

    void SetPosition(const b2Vec2& position) {
		x = position.x;
		y = position.y;
        if (body) body->SetTransform(position, body->GetAngle());
    }

    void SetRotation(float degrees) {
		rotation = degrees;
        if (body) {
            float radians = degrees * (b2_pi / 180.0f);
            body->SetTransform(body->GetPosition(), radians);
        }
    }

    void SetAngularVelocity(float degrees) {
        if (body) body->SetAngularVelocity(degrees * (b2_pi / 180.0f));
    }

    void SetGravityScale(float scale) {
        if (body) body->SetGravityScale(scale);
    }

    void SetUpDirection(const b2Vec2& direction) {
        if (!body) return;
        b2Vec2 norm = direction;
        norm.Normalize();
        float angle = glm::atan(norm.x, -norm.y); // screen-up is (0, -1)
        body->SetTransform(body->GetPosition(), angle);
    }

    void SetRightDirection(const b2Vec2& direction) {
        if (!body) return;
        b2Vec2 norm = direction;
        norm.Normalize();
        float angle = glm::atan(norm.x, -norm.y) - b2_pi / 2.0f;
        body->SetTransform(body->GetPosition(), angle);
    }

    void OnDestroy() {
        if (body && actorDB::instance && actorDB::instance->componentManager.world) {
            actorDB::instance->componentManager.world->DestroyBody(body);
            body = nullptr;
        }
    }


};

// HitResult structure
struct HitResult {
    actorDB::Actor* actor = nullptr;
    b2Vec2 point;
    b2Vec2 normal;
    bool is_trigger = false;
};

// Single raycast callback to find the first hit
class SingleRayCastCallback : public b2RayCastCallback {
public:
    HitResult result;
    float closestFraction = 1.0f;

    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        actorDB::Actor* actor = reinterpret_cast<actorDB::Actor*>(fixture->GetUserData().pointer);
        if (!actor) return -1;

        if (fraction < closestFraction && fraction > 0.0f) {
            closestFraction = fraction;
            result.actor = actor;
            result.point = point;
            result.normal = normal;
            result.is_trigger = fixture->IsSensor();
        }
        return fraction;
    }
};

// Multiple raycast callback to find all hits
struct HitResultEntry {
    HitResult hit;
    float fraction;
};

class MultiRayCastCallback : public b2RayCastCallback {
public:
    std::vector<HitResultEntry> hits;

    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        actorDB::Actor* actor = reinterpret_cast<actorDB::Actor*>(fixture->GetUserData().pointer);
        if (!actor) return -1;

        HitResult result;
        result.actor = actor;
        result.point = point;
        result.normal = normal;
        result.is_trigger = fixture->IsSensor();

        hits.push_back({ result, fraction });
        return 1.0f;
    }
};

// Function exposed to Lua for single raycast
static luabridge::LuaRef Physics_Raycast(const b2Vec2& origin, const b2Vec2& dir, float dist) {
    auto* world = actorDB::instance->componentManager.world;
    if (!world || dist <= 0.0f) return luabridge::LuaRef(actorDB::Actor::lua_state);

    b2Vec2 end = origin + dist * dir;
    SingleRayCastCallback callback;
    world->RayCast(&callback, origin, end);

    if (!callback.result.actor) return luabridge::LuaRef(actorDB::Actor::lua_state);

    lua_State* L = actorDB::Actor::lua_state;
    luabridge::LuaRef result = luabridge::newTable(L);
    result["actor"] = callback.result.actor;
    result["point"] = callback.result.point;
    result["normal"] = callback.result.normal;
    result["is_trigger"] = callback.result.is_trigger;
    return result;
}

// Function exposed to Lua for multiple raycast
static luabridge::LuaRef Physics_RaycastAll(const b2Vec2& origin, const b2Vec2& dir, float dist) {
    auto* world = actorDB::instance->componentManager.world;
    if (!world || dist <= 0.0f) return luabridge::LuaRef(actorDB::Actor::lua_state);

    b2Vec2 end = origin + dist * dir;
    MultiRayCastCallback callback;
    world->RayCast(&callback, origin, end);

    std::sort(callback.hits.begin(), callback.hits.end(), [](const HitResultEntry& a, const HitResultEntry& b) {
        return a.fraction < b.fraction;
        });

    lua_State* L = actorDB::Actor::lua_state;
    luabridge::LuaRef results = luabridge::newTable(L);
    int i = 1;
    for (const auto& entry : callback.hits) {
        luabridge::LuaRef hit = luabridge::newTable(L);
        hit["actor"] = entry.hit.actor;
        hit["point"] = entry.hit.point;
        hit["normal"] = entry.hit.normal;
        hit["is_trigger"] = entry.hit.is_trigger;
        results[i++] = hit;
    }

    return results;
}










class ParticleSystem {
public:
    actorDB::Actor* actor = nullptr;
    std::string key = "";
    bool enabled = true;
	bool paused = false;
    ParticleSystem() {};

    void Play() { paused = false; }
    void Stop() { paused = true; }
    void Burst();
    void OnStart();
    void OnUpdate();
    void OnDestroy();

    // Configurable properties
    float x = 0.0f;
    float y = 0.0f;

    int frames_between_bursts = 1;
    int burst_quantity = 1;

    float start_scale_min = 1.0f;
    float start_scale_max = 1.0f;

    float rotation_min = 0.0f;
    float rotation_max = 0.0f;

    int start_color_r = 255;
    int start_color_g = 255;
    int start_color_b = 255;
    int start_color_a = 255;
	std::optional<int> end_color_r;
	std::optional<int> end_color_g;
	std::optional<int> end_color_b;
	std::optional<int> end_color_a;

    float emit_radius_min = 0.0f;
    float emit_radius_max = 0.5f;

    float emit_angle_min = 0.0f;
    float emit_angle_max = 360.0f;

    float start_speed_min = 0.0f;
    float start_speed_max = 0.0f;

    float rotation_speed_min = 0.0f;
    float rotation_speed_max = 0.0f;

	float gravity_scale_x = 0.0f;
	float gravity_scale_y = 0.0f;

	float drag_factor = 1.0f;
    float angular_drag_factor = 1.0f;

	std::optional<float> end_scale;

    std::string image = "";
    int sorting_order = 9999;

    int local_frame = 0;
    int duration_frames = 300;

    // Random generators
    RandomEngine angleGen;
    RandomEngine radiusGen;
    RandomEngine scaleGen;
    RandomEngine rotationGen;
	RandomEngine speedGen;
	RandomEngine rotationSpeedGen;




private:
    std::vector<glm::vec2> positions;
    std::vector<glm::vec2> velocities;
    std::vector<float> scales, initial_scales;
    std::vector<float> rotations, angular_velocities;
    std::vector<int> birth_frames;
    std::vector<bool> actives;
    std::queue<int> freeList;
};

#endif  // ACTORDB_H