"""
test_config.py — Configuration & Data Validation Tests for youMadeItEngine.

Validates the structural integrity of every data file the engine depends on:
  • game.config and rendering.config (required fields, value ranges)
  • .template files (JSON schema, component declarations)
  • .scene files (actor structure, component declarations)
  • .lua scripts (file existence, lifecycle hook signatures)
  • Cross-file referential integrity (templates ↔ scenes ↔ scripts)

These tests run without compiling or launching the engine. They catch
misconfigurations, missing files, and broken references *before* runtime.
"""

import json
import re
import pytest
from pathlib import Path


# ==========================================================================
#  SECTION 1 — game.config validation
# ==========================================================================

class TestGameConfig:
    """game.config is the engine's entry point. If it's wrong, nothing loads."""

    def test_file_exists(self, game_config_path):
        assert game_config_path.exists(), (
            f"game.config not found at {game_config_path}. "
            "The engine will exit immediately without this file."
        )

    def test_valid_json(self, game_config_path):
        """Confirm the file is parseable JSON (not trailing commas, etc.)."""
        with open(game_config_path, "r") as f:
            data = json.load(f)  # will raise on invalid JSON
        assert isinstance(data, dict), "game.config root must be a JSON object"

    def test_initial_scene_present(self, game_config):
        """Engine calls std::exit if initial_scene is unspecified."""
        assert "initial_scene" in game_config, (
            "game.config missing 'initial_scene' — engine will print "
            "'error: initial_scene unspecified' and exit."
        )

    def test_initial_scene_not_empty(self, game_config):
        scene = game_config.get("initial_scene", "")
        assert isinstance(scene, str) and scene.strip() != "", (
            "initial_scene must be a non-empty string."
        )

    def test_initial_scene_file_exists(self, game_config, scenes_dir):
        """The scene referenced by initial_scene must exist on disk."""
        scene_name = game_config.get("initial_scene", "")
        scene_path = scenes_dir / f"{scene_name}.scene"
        tiled_path = scenes_dir / f"{scene_name}.json"
        assert scene_path.exists() or tiled_path.exists(), (
            f"initial_scene '{scene_name}' does not exist as "
            f"{scene_path} or {tiled_path}. Engine will exit with "
            f"'error: scene {scene_name} is missing'."
        )

    def test_game_title_is_string_if_present(self, game_config):
        if "game_title" in game_config:
            assert isinstance(game_config["game_title"], str), (
                "game_title must be a string."
            )


# ==========================================================================
#  SECTION 2 — rendering.config validation
# ==========================================================================

class TestRenderingConfig:
    """rendering.config is optional but must be well-formed if it exists."""

    def test_valid_json_if_exists(self, rendering_config_path):
        if not rendering_config_path.exists():
            pytest.skip("rendering.config not present (optional)")
        with open(rendering_config_path, "r") as f:
            data = json.load(f)
        assert isinstance(data, dict)

    @pytest.mark.parametrize("field", ["x_resolution", "y_resolution"])
    def test_resolution_fields_are_positive_ints(self, rendering_config, field):
        if rendering_config is None:
            pytest.skip("rendering.config not present")
        if field not in rendering_config:
            pytest.skip(f"{field} not specified (engine uses defaults)")
        val = rendering_config[field]
        assert isinstance(val, int) and val > 0, (
            f"{field} must be a positive integer, got {val!r}"
        )

    @pytest.mark.parametrize("channel", [
        "clear_color_r", "clear_color_g", "clear_color_b"
    ])
    def test_clear_color_channels_in_range(self, rendering_config, channel):
        if rendering_config is None:
            pytest.skip("rendering.config not present")
        if channel not in rendering_config:
            pytest.skip(f"{channel} not specified")
        val = rendering_config[channel]
        assert isinstance(val, int) and 0 <= val <= 255, (
            f"{channel} must be an integer 0–255, got {val!r}"
        )

    def test_zoom_factor_positive_if_present(self, rendering_config):
        if rendering_config is None or "zoom_factor" not in rendering_config:
            pytest.skip("zoom_factor not specified")
        val = rendering_config["zoom_factor"]
        assert isinstance(val, (int, float)) and val > 0, (
            f"zoom_factor must be positive, got {val!r}"
        )

    def test_no_unknown_keys(self, rendering_config):
        """Warn about keys the engine silently ignores."""
        if rendering_config is None:
            pytest.skip("rendering.config not present")
        known_keys = {
            "x_resolution", "y_resolution",
            "clear_color_r", "clear_color_g", "clear_color_b",
            "cam_offset_x", "cam_offset_y",
            "zoom_factor", "cam_ease_factor",
        }
        unknown = set(rendering_config.keys()) - known_keys
        assert not unknown, (
            f"Unrecognized keys in rendering.config (engine ignores these): {unknown}"
        )


# ==========================================================================
#  SECTION 3 — .template file validation
# ==========================================================================

class TestTemplates:
    """Every .template file must be valid JSON with the right structure."""

    def test_templates_directory_exists(self, templates_dir):
        # Not strictly required if no templates are used, but good practice
        if not templates_dir.exists():
            pytest.skip("No actor_templates/ directory found")

    def test_all_templates_parse_as_json(self, templates_dir):
        if not templates_dir.is_dir():
            pytest.skip("No templates directory")
        for f in templates_dir.glob("*.template"):
            with open(f, "r") as fp:
                try:
                    data = json.load(fp)
                except json.JSONDecodeError as e:
                    pytest.fail(f"{f.name} is not valid JSON: {e}")
            assert isinstance(data, dict), f"{f.name} root must be an object"

    def test_each_template_has_name(self, all_templates):
        for filename, data in all_templates.items():
            assert "name" in data, (
                f"Template '{filename}.template' missing 'name' field."
            )

    def test_components_are_keyed_objects(self, all_templates):
        """components should be a dict of numbered keys → component objects."""
        for filename, data in all_templates.items():
            if "components" not in data:
                continue
            comps = data["components"]
            assert isinstance(comps, dict), (
                f"{filename}.template: 'components' must be an object, "
                f"got {type(comps).__name__}"
            )
            for key, comp in comps.items():
                assert isinstance(comp, dict), (
                    f"{filename}.template: component '{key}' must be an object"
                )
                assert "type" in comp, (
                    f"{filename}.template: component '{key}' missing 'type'"
                )


class TestTemplateComponentTypes:
    """Each component type in a template must resolve to a Lua script or C++ built-in."""

    def test_component_types_resolvable(
        self, all_templates, components_dir, builtin_types
    ):
        for filename, data in all_templates.items():
            for key, comp in data.get("components", {}).items():
                comp_type = comp.get("type", "")
                if comp_type in builtin_types:
                    continue
                lua_path = components_dir / f"{comp_type}.lua"
                assert lua_path.exists(), (
                    f"{filename}.template → component '{key}' references type "
                    f"'{comp_type}', but {lua_path} does not exist."
                )


class TestTemplateRigidbodyProperties:
    """Validate that Rigidbody property overrides in templates use correct types."""

    RIGIDBODY_SCHEMA = {
        "body_type": {"allowed": ["dynamic", "static", "kinematic"]},
        "collider_type": {"allowed": ["box", "circle"]},
        "trigger_type": {"allowed": ["box", "circle"]},
        "x": {"type": (int, float)},
        "y": {"type": (int, float)},
        "width": {"type": (int, float), "min": 0},
        "height": {"type": (int, float), "min": 0},
        "radius": {"type": (int, float), "min": 0},
        "friction": {"type": (int, float), "min": 0},
        "bounciness": {"type": (int, float)},
        "density": {"type": (int, float), "min": 0},
        "gravity_scale": {"type": (int, float)},
        "rotation": {"type": (int, float)},
        "has_collider": {"type": bool},
        "has_trigger": {"type": bool},
        "precise": {"type": bool},
        "enabled": {"type": bool},
    }

    def _rigidbody_components(self, all_templates):
        """Yield (template_name, comp_key, properties) for every Rigidbody."""
        for tname, data in all_templates.items():
            for key, comp in data.get("components", {}).items():
                if comp.get("type") == "Rigidbody":
                    yield tname, key, comp

    def test_rigidbody_properties_valid(self, all_templates):
        for tname, key, comp in self._rigidbody_components(all_templates):
            for prop, val in comp.items():
                if prop == "type":
                    continue
                if prop not in self.RIGIDBODY_SCHEMA:
                    # Unknown property — engine will ignore, but flag it
                    continue
                schema = self.RIGIDBODY_SCHEMA[prop]
                if "allowed" in schema:
                    assert val in schema["allowed"], (
                        f"{tname}.template Rigidbody[{key}].{prop}: "
                        f"'{val}' not in {schema['allowed']}"
                    )
                if "type" in schema:
                    assert isinstance(val, schema["type"]), (
                        f"{tname}.template Rigidbody[{key}].{prop}: "
                        f"expected {schema['type']}, got {type(val).__name__}"
                    )
                if "min" in schema and isinstance(val, (int, float)):
                    assert val >= schema["min"], (
                        f"{tname}.template Rigidbody[{key}].{prop}: "
                        f"{val} < minimum {schema['min']}"
                    )


# ==========================================================================
#  SECTION 4 — .scene file validation
# ==========================================================================

class TestScenes:
    """Scene files define what actors exist when a level loads."""

    def test_scenes_directory_exists(self, scenes_dir):
        assert scenes_dir.is_dir(), (
            f"No scenes/ directory at {scenes_dir}. "
            "Engine needs at least one scene to run."
        )

    def test_all_scenes_parse_as_json(self, scenes_dir):
        for f in scenes_dir.glob("*.scene"):
            with open(f, "r") as fp:
                try:
                    data = json.load(fp)
                except json.JSONDecodeError as e:
                    pytest.fail(f"{f.name} is not valid JSON: {e}")

    def test_scene_has_actors_array(self, all_scenes):
        for scene_name, data in all_scenes.items():
            assert "actors" in data, (
                f"Scene '{scene_name}' missing 'actors' array."
            )
            assert isinstance(data["actors"], list), (
                f"Scene '{scene_name}': 'actors' must be an array."
            )

    def test_scene_actors_have_names(self, all_scenes):
        for scene_name, data in all_scenes.items():
            for i, actor in enumerate(data.get("actors", [])):
                assert "name" in actor, (
                    f"Scene '{scene_name}', actor index {i}: missing 'name'."
                )

    def test_scene_component_types_resolvable(
        self, all_scenes, components_dir, builtin_types
    ):
        """Every component type referenced in a scene must exist."""
        for scene_name, data in all_scenes.items():
            for actor in data.get("actors", []):
                actor_name = actor.get("name", "?")
                for key, comp in actor.get("components", {}).items():
                    comp_type = comp.get("type", "")
                    if comp_type in builtin_types:
                        continue
                    lua_path = components_dir / f"{comp_type}.lua"
                    assert lua_path.exists(), (
                        f"Scene '{scene_name}', actor '{actor_name}', "
                        f"component '{key}': type '{comp_type}' not found "
                        f"at {lua_path}."
                    )


# ==========================================================================
#  SECTION 5 — Lua script validation
# ==========================================================================

class TestLuaScripts:
    """Validate Lua component scripts without running them."""

    def test_scripts_directory_exists(self, components_dir):
        assert components_dir.is_dir(), (
            f"No component_types/ directory at {components_dir}."
        )

    def test_each_script_defines_matching_table(self, all_lua_scripts):
        """A file named Foo.lua should define a global table named 'Foo'."""
        for name, content in all_lua_scripts.items():
            # Look for  Foo = {  at the start of a line
            pattern = rf"^{re.escape(name)}\s*="
            assert re.search(pattern, content, re.MULTILINE), (
                f"{name}.lua does not define a top-level '{name}' table. "
                f"The engine expects the table name to match the filename."
            )

    def test_lifecycle_hooks_are_functions(self, all_lua_scripts):
        """
        If a script declares OnStart/OnUpdate/OnLateUpdate/OnDestroy,
        ensure they look like function declarations (not accidental strings).
        """
        hooks = [
            "OnStart", "OnUpdate", "OnLateUpdate", "OnDestroy",
            "OnCollisionEnter", "OnCollisionExit",
            "OnTriggerEnter", "OnTriggerExit",
        ]
        for name, content in all_lua_scripts.items():
            for hook in hooks:
                # Check if the hook name appears in the file
                if hook not in content:
                    continue
                # It should appear as:  OnStart = function(self  ...
                func_pattern = rf"{hook}\s*=\s*function\s*\("
                assert re.search(func_pattern, content), (
                    f"{name}.lua references '{hook}' but it doesn't look "
                    f"like a function definition. Expected pattern: "
                    f"'{hook} = function(self, ...)'"
                )

    def test_self_parameter_on_lifecycle_hooks(self, all_lua_scripts):
        """
        Lua lifecycle hooks must take 'self' as first parameter.
        Missing 'self' causes subtle bugs where the component table
        is passed as the first argument instead.
        """
        hook_pattern = re.compile(
            r"(On\w+)\s*=\s*function\s*\(([^)]*)\)"
        )
        for name, content in all_lua_scripts.items():
            for match in hook_pattern.finditer(content):
                hook_name = match.group(1)
                params = match.group(2).strip()
                first_param = params.split(",")[0].strip() if params else ""
                assert first_param == "self", (
                    f"{name}.lua: {hook_name} first parameter should be "
                    f"'self', got '{first_param}'. This will cause runtime "
                    f"errors when the engine calls the hook."
                )


# ==========================================================================
#  SECTION 6 — Cross-file referential integrity
# ==========================================================================

class TestReferentialIntegrity:
    """
    Verify that references between files are consistent:
    scenes → templates, templates → scripts, scripts → templates.
    """

    def test_instantiated_templates_exist(
        self, referenced_template_names, all_templates
    ):
        """Every Actor.Instantiate("X") in Lua must have a matching X.template."""
        for tname in referenced_template_names:
            assert tname in all_templates, (
                f"Lua calls Actor.Instantiate(\"{tname}\") but "
                f"'{tname}.template' does not exist in actor_templates/."
            )

    def test_sprite_images_exist(self, all_templates, resources_dir):
        """
        SpriteRenderer components reference image names.
        Verify the image files exist (engine loads from resources/images/).
        """
        images_dir = resources_dir / "images"
        if not images_dir.is_dir():
            pytest.skip("No images/ directory found")
        for tname, data in all_templates.items():
            for key, comp in data.get("components", {}).items():
                if comp.get("type") != "SpriteRenderer":
                    continue
                sprite = comp.get("sprite")
                if sprite is None or sprite == "???":
                    continue  # default placeholder
                # Engine typically loads sprite.png
                sprite_path = images_dir / f"{sprite}.png"
                assert sprite_path.exists(), (
                    f"{tname}.template SpriteRenderer[{key}] references "
                    f"sprite '{sprite}' but {sprite_path} not found."
                )

    def test_no_orphan_templates(
        self, all_templates, all_scenes, referenced_template_names
    ):
        """
        Flag templates that are never referenced by any scene or Lua script.
        Not a hard failure, but useful for cleanup.
        """
        # Templates referenced directly in scene files
        scene_templates = set()
        for scene_data in all_scenes.values():
            for actor in scene_data.get("actors", []):
                if "template" in actor:
                    scene_templates.add(actor["template"])

        all_referenced = referenced_template_names | scene_templates
        orphans = set(all_templates.keys()) - all_referenced
        # Soft warning — mark as xfail rather than fail
        if orphans:
            pytest.xfail(
                f"Potentially orphaned templates (never Instantiated): {orphans}"
            )
