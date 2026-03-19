"""
test_components.py — Component Lifecycle & Dependency Validation Tests.

Validates the contracts between Lua scripts, templates, and engine APIs:
  • Component dependencies (scripts that call GetComponent on sibling components)
  • Template composition (each template has all the components its scripts need)
  • Event publish/subscribe wiring (publisher and subscriber use matching names)
  • Actor.Find name resolution (Find("X") must match an actor spawned with name "X")
  • Lua property initialization (default values declared in script tables)
  • Lifecycle hook ordering (OnStart runs before OnUpdate accesses state)
  • Engine API usage (scripts only call APIs that exist in the engine bindings)

These tests catch wiring bugs between components that would cause nil-reference
errors, missing events, or broken gameplay logic at runtime.
"""

import json
import re
import pytest
from pathlib import Path


# ==========================================================================
#  Helpers — parse Lua scripts for dependency information
# ==========================================================================

def extract_get_component_calls(lua_source: str) -> list[dict]:
    """
    Find all GetComponent("TypeName") calls and their context.
    Returns list of {type, hook, line, is_self} dicts.

    is_self=True means self.actor:GetComponent (same actor)
    is_self=False means some_other:GetComponent (different actor)
    """
    results = []
    # Track which hook we're currently inside
    hook_pattern = re.compile(r"(On\w+)\s*=\s*function\s*\(")
    getcomp_pattern = re.compile(
        r"(self\.actor|[\w_]+):GetComponent\(\s*\"([^\"]+)\"\s*\)"
    )

    current_hook = None
    for i, line in enumerate(lua_source.splitlines(), 1):
        hook_match = hook_pattern.search(line)
        if hook_match:
            current_hook = hook_match.group(1)

        for match in getcomp_pattern.finditer(line):
            caller = match.group(1)
            comp_type = match.group(2)
            results.append({
                "type": comp_type,
                "hook": current_hook,
                "line": i,
                "is_self": caller == "self.actor",
            })
    return results


def extract_actor_find_calls(lua_source: str) -> list[str]:
    """Find all Actor.Find("name") calls, return list of actor names."""
    pattern = re.compile(r'Actor\.Find\(\s*"([^"]+)"\s*\)')
    return pattern.findall(lua_source)


def extract_actor_instantiate_calls(lua_source: str) -> list[str]:
    """Find all Actor.Instantiate("template") calls."""
    pattern = re.compile(r'Actor\.Instantiate\(\s*"([^"]+)"\s*\)')
    return pattern.findall(lua_source)


def extract_event_publishes(lua_source: str) -> list[str]:
    """Find all Event.Publish("event_name") calls."""
    pattern = re.compile(r'Event\.Publish\(\s*"([^"]+)"\s*\)')
    return pattern.findall(lua_source)


def extract_event_subscribes(lua_source: str) -> list[str]:
    """Find all Event.Subscribe("event_name", ...) calls."""
    pattern = re.compile(r'Event\.Subscribe\(\s*"([^"]+)"\s*,')
    return pattern.findall(lua_source)


def extract_api_calls(lua_source: str) -> set[str]:
    """Find all Engine API namespace calls (Input.X, Image.X, etc.)."""
    pattern = re.compile(
        r"(Input|Image|Text|Audio|Camera|Physics|Application|Actor|Event)"
        r"\.(\w+)"
    )
    return {f"{m.group(1)}.{m.group(2)}" for m in pattern.finditer(lua_source)}


def extract_lua_table_defaults(lua_source: str, table_name: str) -> dict[str, str]:
    """
    Extract default property assignments from a Lua table definition.
    e.g. 'speed = 5' inside 'KeyboardControls = { ... }'
    Returns {property_name: raw_value_string}.
    """
    defaults = {}
    # Match simple key = value lines (not function definitions)
    prop_pattern = re.compile(
        rf"^\s+(\w+)\s*=\s*(?!function)(.+?)\s*,?\s*$"
    )
    in_table = False
    brace_depth = 0

    for line in lua_source.splitlines():
        if re.match(rf"^{re.escape(table_name)}\s*=\s*\{{", line):
            in_table = True
            brace_depth = line.count("{") - line.count("}")
            continue
        if in_table:
            brace_depth += line.count("{") - line.count("}")
            if brace_depth <= 0:
                break
            # Only capture top-level properties (depth 1)
            if brace_depth == 1:
                match = prop_pattern.match(line)
                if match:
                    defaults[match.group(1)] = match.group(2).strip()
    return defaults


# ==========================================================================
#  Engine API registry — all functions exposed to Lua via LuaBridge
# ==========================================================================

# Extracted from engine.cpp LuaBridge bindings
ENGINE_API = {
    "Application.Quit", "Application.Sleep",
    "Application.GetFrame", "Application.OpenURL",
    "Input.GetKey", "Input.GetKeyDown", "Input.GetKeyUp",
    "Input.GetMouseButton", "Input.GetMouseButtonDown",
    "Input.GetMouseButtonUp", "Input.GetMouseScrollDelta",
    "Input.HideCursor", "Input.ShowCursor", "Input.GetMousePosition",
    "Audio.Play", "Audio.Halt", "Audio.SetVolume",
    "Image.Draw", "Image.DrawEx", "Image.DrawUI",
    "Image.DrawUIEx", "Image.DrawPixel",
    "Text.Draw",
    "Camera.SetPosition", "Camera.GetPositionX",
    "Camera.GetPositionY", "Camera.SetZoom", "Camera.GetZoom",
    "Actor.Find", "Actor.FindAll",
    "Actor.Instantiate", "Actor.Destroy",
    "Event.Publish", "Event.Subscribe", "Event.Unsubscribe",
    "Physics.Raycast", "Physics.RaycastAll",
}


# ==========================================================================
#  SECTION 1 — Component dependency validation
# ==========================================================================

class TestComponentDependencies:
    """
    When a Lua script calls self.actor:GetComponent("X"), it assumes
    component X exists on the same actor. If X is missing from the
    template, the call returns nil and causes a runtime crash.

    These tests verify that every self-referencing GetComponent call
    is satisfied by the actor's template.
    """

    def _build_dependency_map(self, all_lua_scripts) -> dict[str, list[str]]:
        """
        Map: script_name → [required component types on same actor].
        Only includes self.actor:GetComponent calls (not cross-actor).
        """
        deps = {}
        for name, source in all_lua_scripts.items():
            calls = extract_get_component_calls(source)
            self_deps = [c["type"] for c in calls if c["is_self"]]
            if self_deps:
                deps[name] = list(set(self_deps))
        return deps

    def _get_template_component_types(self, template_data: dict) -> set[str]:
        """Get all component type names from a template."""
        types = set()
        for comp in template_data.get("components", {}).values():
            if "type" in comp:
                types.add(comp["type"])
        return types

    def test_self_dependencies_satisfied_in_templates(
        self, all_lua_scripts, all_templates, builtin_types
    ):
        """
        For each template, verify that every component's GetComponent("X")
        call on self.actor can be resolved by another component in the
        same template.
        """
        dep_map = self._build_dependency_map(all_lua_scripts)
        errors = []

        for tname, tdata in all_templates.items():
            comp_types = self._get_template_component_types(tdata)

            for comp in tdata.get("components", {}).values():
                script_type = comp.get("type", "")
                if script_type in builtin_types:
                    continue
                if script_type not in dep_map:
                    continue

                for required in dep_map[script_type]:
                    if required not in comp_types:
                        errors.append(
                            f"{tname}.template: {script_type} calls "
                            f"GetComponent(\"{required}\") but template "
                            f"has no {required} component"
                        )

        assert not errors, (
            "Component dependency violations found:\n"
            + "\n".join(f"  • {e}" for e in errors)
        )

    def test_keyboard_controls_requires_rigidbody(self, all_templates):
        """
        KeyboardControls.OnStart: self.rb = self.actor:GetComponent("Rigidbody")
        Any template using KeyboardControls MUST also have a Rigidbody.
        """
        for tname, tdata in all_templates.items():
            comp_types = {
                c.get("type") for c in tdata.get("components", {}).values()
            }
            if "KeyboardControls" in comp_types:
                assert "Rigidbody" in comp_types, (
                    f"{tname}.template has KeyboardControls but no Rigidbody. "
                    f"KeyboardControls.OnStart will crash with nil reference."
                )

    def test_sprite_renderer_handles_missing_rigidbody(self, all_lua_scripts):
        """
        SpriteRenderer checks 'if self.rb ~= nil' before using Rigidbody.
        Verify this nil-guard exists — it allows SpriteRenderer to work
        on actors without physics.
        """
        source = all_lua_scripts.get("SpriteRenderer", "")
        assert "self.rb ~= nil" in source or "self.rb~=nil" in source, (
            "SpriteRenderer should nil-check Rigidbody before using it. "
            "Without this guard, actors without Rigidbody will crash."
        )


# ==========================================================================
#  SECTION 2 — Template composition completeness
# ==========================================================================

class TestTemplateComposition:
    """
    Verify that each template's component set forms a coherent unit.
    Templates should have all required components and no contradictions.
    """

    def test_all_instantiated_templates_have_rigidbody(self, all_lua_scripts, all_templates):
        """
        GameManager.lua instantiates templates and immediately calls
        GetComponent("Rigidbody") on each. Every instantiated template
        must include a Rigidbody.
        """
        gm_source = all_lua_scripts.get("GameManager", "")
        instantiated = extract_actor_instantiate_calls(gm_source)

        for tname in instantiated:
            if tname not in all_templates:
                continue  # covered by test_config referential integrity
            comp_types = {
                c.get("type")
                for c in all_templates[tname].get("components", {}).values()
            }
            assert "Rigidbody" in comp_types, (
                f"GameManager instantiates '{tname}' and immediately calls "
                f"GetComponent(\"Rigidbody\"), but {tname}.template has no "
                f"Rigidbody component."
            )

    def test_player_has_required_components(self, all_templates):
        """
        Player needs: Rigidbody (physics), KeyboardControls (input),
        SpriteRenderer (rendering). Missing any breaks core gameplay.
        """
        player = all_templates.get("Player")
        if player is None:
            pytest.skip("No Player template")

        comp_types = {
            c.get("type") for c in player.get("components", {}).values()
        }
        required = {"Rigidbody", "KeyboardControls", "SpriteRenderer"}
        missing = required - comp_types
        assert not missing, (
            f"Player template missing required components: {missing}"
        )

    def test_bouncy_box_has_collision_handler(self, all_templates):
        """
        BouncyBox template needs both a Rigidbody (for collision detection)
        and the BouncyBox script (for OnCollisionEnter handler).
        """
        bb = all_templates.get("BouncyBox")
        if bb is None:
            pytest.skip("No BouncyBox template")

        comp_types = {
            c.get("type") for c in bb.get("components", {}).values()
        }
        assert "Rigidbody" in comp_types, (
            "BouncyBox needs Rigidbody for collision detection"
        )
        assert "BouncyBox" in comp_types, (
            "BouncyBox template needs BouncyBox script for OnCollisionEnter"
        )

    def test_victory_box_has_trigger_handler(self, all_templates):
        """
        VictoryBox uses OnTriggerEnter, so it needs a Rigidbody and
        the VictoryBox script component.
        """
        vb = all_templates.get("VictoryBox")
        if vb is None:
            pytest.skip("No VictoryBox template")

        comp_types = {
            c.get("type") for c in vb.get("components", {}).values()
        }
        assert "Rigidbody" in comp_types, (
            "VictoryBox needs Rigidbody for trigger detection"
        )
        assert "VictoryBox" in comp_types, (
            "VictoryBox template needs VictoryBox script for OnTriggerEnter"
        )

    def test_component_keys_are_sequential(self, all_templates):
        """
        Template component keys should be sequential integers ("1", "2", "3").
        Gaps or non-numeric keys may cause loading issues.
        """
        for tname, tdata in all_templates.items():
            keys = list(tdata.get("components", {}).keys())
            if not keys:
                continue
            # Check all keys are numeric strings
            for k in keys:
                assert k.isdigit(), (
                    f"{tname}.template: component key '{k}' is not numeric"
                )
            # Check sequential starting from 1
            nums = sorted(int(k) for k in keys)
            expected = list(range(1, len(nums) + 1))
            assert nums == expected, (
                f"{tname}.template: component keys {nums} should be "
                f"sequential {expected}"
            )


# ==========================================================================
#  SECTION 3 — Event publish/subscribe wiring
# ==========================================================================

class TestEventWiring:
    """
    The engine's Event system connects components across actors.
    If a publisher uses event name "X" but subscribers listen for "Y",
    the event silently never fires. These tests catch name mismatches.
    """

    def _collect_all_events(self, all_lua_scripts):
        """Collect all published and subscribed event names."""
        published = {}   # event_name → [script_names]
        subscribed = {}  # event_name → [script_names]

        for name, source in all_lua_scripts.items():
            for event in extract_event_publishes(source):
                published.setdefault(event, []).append(name)
            for event in extract_event_subscribes(source):
                subscribed.setdefault(event, []).append(name)

        return published, subscribed

    def test_every_published_event_has_subscriber(self, all_lua_scripts):
        """An event that's published but never subscribed to is dead code."""
        published, subscribed = self._collect_all_events(all_lua_scripts)

        for event, publishers in published.items():
            assert event in subscribed, (
                f"Event '{event}' is published by {publishers} "
                f"but no script subscribes to it."
            )

    def test_every_subscribed_event_has_publisher(self, all_lua_scripts):
        """A subscription to a never-published event will never trigger."""
        published, subscribed = self._collect_all_events(all_lua_scripts)

        for event, subscribers in subscribed.items():
            assert event in published, (
                f"Event '{event}' is subscribed by {subscribers} "
                f"but no script publishes it."
            )

    def test_victory_event_wiring(self, all_lua_scripts):
        """
        Specific test: VictoryBox publishes 'event_victory',
        Hud subscribes to 'event_victory'. Names must match exactly.
        """
        vb_source = all_lua_scripts.get("VictoryBox", "")
        hud_source = all_lua_scripts.get("Hud", "")

        vb_publishes = extract_event_publishes(vb_source)
        hud_subscribes = extract_event_subscribes(hud_source)

        assert "event_victory" in vb_publishes, (
            "VictoryBox should publish 'event_victory'"
        )
        assert "event_victory" in hud_subscribes, (
            "Hud should subscribe to 'event_victory'"
        )

    def test_subscriber_has_callback_method(self, all_lua_scripts):
        """
        When a script calls Event.Subscribe("X", self, self.OnCallback),
        verify the callback method exists as a function in the same script.
        """
        callback_pattern = re.compile(
            r'Event\.Subscribe\(\s*"[^"]+"\s*,\s*self\s*,\s*self\.(\w+)\s*\)'
        )

        for name, source in all_lua_scripts.items():
            for match in callback_pattern.finditer(source):
                callback_name = match.group(1)
                # Verify the callback is defined as a function
                func_pattern = rf"{callback_name}\s*=\s*function\s*\("
                assert re.search(func_pattern, source), (
                    f"{name}.lua subscribes with callback 'self.{callback_name}' "
                    f"but {callback_name} is not defined as a function."
                )


# ==========================================================================
#  SECTION 4 — Actor.Find name resolution
# ==========================================================================

class TestActorFindResolution:
    """
    Actor.Find("name") searches for a live actor by name.
    The name must match what's set in a template or scene file.
    """

    def test_find_calls_match_spawned_actors(
        self, all_lua_scripts, all_templates, all_scenes
    ):
        """
        Every Actor.Find("X") should have a matching actor name
        either in a scene file or as a template's 'name' field.
        """
        # Collect all names from scenes
        scene_names = set()
        for scene_data in all_scenes.values():
            for actor in scene_data.get("actors", []):
                scene_names.add(actor.get("name", ""))

        # Collect all names from templates (template 'name' field)
        template_names = set()
        for tdata in all_templates.values():
            if "name" in tdata:
                template_names.add(tdata["name"])

        all_known_names = scene_names | template_names

        # Collect all Actor.Find calls
        for script_name, source in all_lua_scripts.items():
            find_calls = extract_actor_find_calls(source)
            for actor_name in find_calls:
                assert actor_name in all_known_names, (
                    f"{script_name}.lua calls Actor.Find(\"{actor_name}\") "
                    f"but no scene or template defines an actor named "
                    f"'{actor_name}'. Known names: {sorted(all_known_names)}"
                )

    def test_camera_manager_finds_player(self, all_lua_scripts, all_templates):
        """
        CameraManager calls Actor.Find("player").
        The Player template must define name="player" for this to work.
        """
        player_template = all_templates.get("Player")
        if player_template is None:
            pytest.skip("No Player template")

        assert player_template.get("name") == "player", (
            f"Player template name is '{player_template.get('name')}', "
            f"but CameraManager.lua calls Actor.Find(\"player\"). "
            f"Name must be exactly 'player' (case-sensitive)."
        )


# ==========================================================================
#  SECTION 5 — Property initialization validation
# ==========================================================================

class TestPropertyInitialization:
    """
    Lua component scripts declare default property values in their tables.
    These defaults are the initial state before OnStart runs. Validate
    that defaults are reasonable and consistent with usage.
    """

    def test_keyboard_controls_defaults(self, all_lua_scripts):
        """KeyboardControls should have positive speed and jump_power."""
        source = all_lua_scripts.get("KeyboardControls", "")
        defaults = extract_lua_table_defaults(source, "KeyboardControls")

        assert "speed" in defaults, "KeyboardControls missing 'speed' default"
        speed = float(defaults["speed"])
        assert speed > 0, f"speed must be positive, got {speed}"

        assert "jump_power" in defaults, "KeyboardControls missing 'jump_power'"
        jump = float(defaults["jump_power"])
        assert jump > 0, f"jump_power must be positive, got {jump}"

    def test_camera_manager_defaults(self, all_lua_scripts):
        """CameraManager should have valid ease_factor between 0 and 1."""
        source = all_lua_scripts.get("CameraManager", "")
        defaults = extract_lua_table_defaults(source, "CameraManager")

        assert "ease_factor" in defaults, "CameraManager missing 'ease_factor'"
        ease = float(defaults["ease_factor"])
        assert 0 < ease <= 1, (
            f"ease_factor should be in (0, 1], got {ease}. "
            f"Values outside this range cause erratic camera movement."
        )

    def test_hud_initial_state(self, all_lua_scripts):
        """Hud should start with seconds_elapsed=0 and finish=false."""
        source = all_lua_scripts.get("Hud", "")
        defaults = extract_lua_table_defaults(source, "Hud")

        assert defaults.get("seconds_elapsed") == "0", (
            f"Hud.seconds_elapsed should start at 0, got {defaults.get('seconds_elapsed')}"
        )
        assert defaults.get("finish") == "false", (
            f"Hud.finish should start as false, got {defaults.get('finish')}"
        )

    def test_sprite_renderer_defaults(self, all_lua_scripts):
        """SpriteRenderer color channels should default to 255 (full white)."""
        source = all_lua_scripts.get("SpriteRenderer", "")
        defaults = extract_lua_table_defaults(source, "SpriteRenderer")

        for channel in ["r", "g", "b", "a"]:
            assert channel in defaults, (
                f"SpriteRenderer missing color default '{channel}'"
            )
            val = int(defaults[channel])
            assert 0 <= val <= 255, (
                f"SpriteRenderer.{channel} = {val}, expected 0–255"
            )

    def test_sprite_renderer_has_sprite_default(self, all_lua_scripts):
        """SpriteRenderer must declare a sprite property (even if placeholder)."""
        source = all_lua_scripts.get("SpriteRenderer", "")
        defaults = extract_lua_table_defaults(source, "SpriteRenderer")
        assert "sprite" in defaults, (
            "SpriteRenderer missing 'sprite' default property"
        )


# ==========================================================================
#  SECTION 6 — Engine API usage validation
# ==========================================================================

class TestEngineAPIUsage:
    """
    Verify that Lua scripts only call Engine API functions that are
    actually registered in the LuaBridge bindings. Calling a function
    that doesn't exist causes a Lua runtime error.
    """

    def test_all_api_calls_are_registered(self, all_lua_scripts):
        """Every Namespace.Function call in Lua must exist in engine bindings."""
        errors = []

        for name, source in all_lua_scripts.items():
            api_calls = extract_api_calls(source)
            for call in api_calls:
                if call not in ENGINE_API:
                    errors.append(f"{name}.lua: {call}")

        assert not errors, (
            "Scripts call unregistered engine APIs:\n"
            + "\n".join(f"  • {e}" for e in errors)
        )

    def test_scripts_use_vector2_constructor(self, all_lua_scripts):
        """
        Scripts using Vector2(x, y) depend on it being registered
        in the Lua bindings. Verify Vector2 appears as a constructor
        pattern, not a namespace call.
        """
        vec2_pattern = re.compile(r"Vector2\s*\(")
        scripts_using_vec2 = []

        for name, source in all_lua_scripts.items():
            if vec2_pattern.search(source):
                scripts_using_vec2.append(name)

        # These scripts are known to need Vector2
        expected_users = {"KeyboardControls", "SpriteRenderer", "GameManager"}
        for script in expected_users:
            if script in all_lua_scripts:
                assert script in scripts_using_vec2, (
                    f"{script}.lua should use Vector2 constructor"
                )


# ==========================================================================
#  SECTION 7 — Lifecycle hook ordering
# ==========================================================================

class TestLifecycleOrdering:
    """
    The engine runs lifecycle hooks in order: OnStart → OnUpdate → OnLateUpdate.
    Scripts that initialize state in OnStart and use it in OnUpdate are
    correct. Scripts that use state in OnStart that's only set in OnUpdate
    have a bug.
    """

    def test_onstart_initializes_before_onupdate_uses(self, all_lua_scripts):
        """
        KeyboardControls sets self.rb in OnStart, then uses self.rb in OnUpdate.
        Verify the assignment happens in OnStart, not OnUpdate.
        """
        source = all_lua_scripts.get("KeyboardControls", "")
        if not source:
            pytest.skip("KeyboardControls not found")

        # Find where self.rb is assigned vs used
        assign_pattern = re.compile(r"self\.rb\s*=")
        use_pattern = re.compile(r"self\.rb[:\.]")

        # Check assignment is in OnStart
        in_onstart = False
        assignment_in_onstart = False
        for line in source.splitlines():
            if "OnStart" in line and "function" in line:
                in_onstart = True
            elif "OnUpdate" in line and "function" in line:
                in_onstart = False
            if in_onstart and assign_pattern.search(line):
                assignment_in_onstart = True
                break

        assert assignment_in_onstart, (
            "KeyboardControls should assign self.rb in OnStart, not later. "
            "OnUpdate runs every frame and would re-fetch unnecessarily."
        )

    def test_collision_hooks_have_collision_parameter(self, all_lua_scripts):
        """
        OnCollisionEnter/Exit and OnTriggerEnter/Exit receive a collision
        parameter. Verify the function signature includes it.
        """
        collision_hooks = [
            "OnCollisionEnter", "OnCollisionExit",
            "OnTriggerEnter", "OnTriggerExit",
        ]
        hook_sig_pattern = re.compile(
            r"(On(?:Collision|Trigger)(?:Enter|Exit))\s*=\s*function\s*\(([^)]*)\)"
        )

        for name, source in all_lua_scripts.items():
            for match in hook_sig_pattern.finditer(source):
                hook_name = match.group(1)
                params = [p.strip() for p in match.group(2).split(",")]
                assert len(params) >= 2, (
                    f"{name}.lua: {hook_name} needs (self, collision) parameters, "
                    f"got ({', '.join(params)})"
                )

    def test_scene_actors_load_before_find_calls(self, all_scenes, all_lua_scripts):
        """
        Actors defined in the scene file load in array order.
        CameraManager calls Actor.Find("player"), but "player" is spawned
        by GameManager during OnStart. Verify GameManager is in the scene
        so it can spawn the player before CameraManager looks for it.
        """
        for scene_name, scene_data in all_scenes.items():
            actor_names = [
                a.get("name", "") for a in scene_data.get("actors", [])
            ]
            # Check scripts that use Actor.Find
            for actor in scene_data.get("actors", []):
                for comp in actor.get("components", {}).values():
                    comp_type = comp.get("type", "")
                    if comp_type not in all_lua_scripts:
                        continue
                    find_calls = extract_actor_find_calls(
                        all_lua_scripts[comp_type]
                    )
                    if not find_calls:
                        continue
                    # The spawning script (GameManager) should exist in scene
                    if "GameManager" in [
                        c.get("type")
                        for a in scene_data.get("actors", [])
                        for c in a.get("components", {}).values()
                    ]:
                        pass  # GameManager will spawn the needed actors
                    else:
                        # No GameManager — actor must exist directly in scene
                        for needed_name in find_calls:
                            assert needed_name in actor_names, (
                                f"Scene '{scene_name}': {comp_type} calls "
                                f"Actor.Find(\"{needed_name}\") but no "
                                f"GameManager or direct actor with that name."
                            )
