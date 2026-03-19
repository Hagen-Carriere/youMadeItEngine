#!/usr/bin/env python3
"""
verify_config.py — Standalone test runner for config validation.

Runs every check from test_config.py using plain Python.
This is for demonstration only; the real tests use pytest.

Usage:  python3 verify_config.py [path/to/resources]
"""

import json
import re
import sys
import os
from pathlib import Path

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------

BUILTIN_COMPONENT_TYPES = {"Rigidbody", "ParticleSystem"}

class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.skipped = 0
        self.xfail = 0
        self.results = []

    def ok(self, name):
        self.passed += 1
        self.results.append(("PASS", name, ""))
        print(f"  \033[92mPASS\033[0m  {name}")

    def fail(self, name, msg):
        self.failed += 1
        self.results.append(("FAIL", name, msg))
        print(f"  \033[91mFAIL\033[0m  {name}")
        print(f"         {msg}")

    def skip(self, name, reason):
        self.skipped += 1
        self.results.append(("SKIP", name, reason))
        print(f"  \033[93mSKIP\033[0m  {name} — {reason}")

    def xfailed(self, name, msg):
        self.xfail += 1
        self.results.append(("XFAIL", name, msg))
        print(f"  \033[93mXFAIL\033[0m {name} — {msg}")

    def summary(self):
        total = self.passed + self.failed + self.skipped + self.xfail
        print(f"\n{'='*60}")
        print(f" Results: {total} total | "
              f"\033[92m{self.passed} passed\033[0m | "
              f"\033[91m{self.failed} failed\033[0m | "
              f"\033[93m{self.skipped} skipped\033[0m | "
              f"{self.xfail} xfail")
        print(f"{'='*60}")
        return self.failed == 0


def load_json(path):
    with open(path, "r") as f:
        return json.load(f)


# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

def run_all(resources_dir: Path):
    r = TestResult()

    # Resolve paths
    game_config_path = resources_dir / "game.config"
    rendering_config_path = resources_dir / "rendering.config"
    templates_dir = resources_dir / "actor_templates"
    scenes_dir = resources_dir / "scenes"
    components_dir = resources_dir / "component_types"
    images_dir = resources_dir / "images"

    # ==== SECTION 1: game.config ====
    print("\n\033[1m— game.config —\033[0m")

    if not game_config_path.exists():
        r.fail("game_config_exists", f"Not found: {game_config_path}")
        print("\n  Cannot continue without game.config. Aborting.")
        return r

    r.ok("game_config_exists")

    try:
        gc = load_json(game_config_path)
        assert isinstance(gc, dict)
        r.ok("game_config_valid_json")
    except Exception as e:
        r.fail("game_config_valid_json", str(e))
        return r

    if "initial_scene" in gc:
        r.ok("initial_scene_present")
    else:
        r.fail("initial_scene_present", "Missing 'initial_scene' key")

    scene_name = gc.get("initial_scene", "")
    if isinstance(scene_name, str) and scene_name.strip():
        r.ok("initial_scene_not_empty")
    else:
        r.fail("initial_scene_not_empty", f"Got: {scene_name!r}")

    if scene_name:
        scene_path = scenes_dir / f"{scene_name}.scene"
        tiled_path = scenes_dir / f"{scene_name}.json"
        if scene_path.exists() or tiled_path.exists():
            r.ok(f"initial_scene_file_exists [{scene_name}]")
        else:
            r.fail(f"initial_scene_file_exists [{scene_name}]",
                   f"Neither {scene_path} nor {tiled_path} found")

    if "game_title" in gc:
        if isinstance(gc["game_title"], str):
            r.ok("game_title_is_string")
        else:
            r.fail("game_title_is_string", f"Got {type(gc['game_title']).__name__}")

    # ==== SECTION 2: rendering.config ====
    print("\n\033[1m— rendering.config —\033[0m")

    rc = None
    if not rendering_config_path.exists():
        r.skip("rendering_config_valid_json", "File not present (optional)")
    else:
        try:
            rc = load_json(rendering_config_path)
            assert isinstance(rc, dict)
            r.ok("rendering_config_valid_json")
        except Exception as e:
            r.fail("rendering_config_valid_json", str(e))

    if rc is not None:
        for field in ["x_resolution", "y_resolution"]:
            if field not in rc:
                r.skip(f"resolution_{field}", f"{field} not specified")
            elif isinstance(rc[field], int) and rc[field] > 0:
                r.ok(f"resolution_{field}")
            else:
                r.fail(f"resolution_{field}", f"Expected positive int, got {rc[field]!r}")

        for channel in ["clear_color_r", "clear_color_g", "clear_color_b"]:
            if channel not in rc:
                r.skip(f"color_{channel}", f"{channel} not specified")
            elif isinstance(rc[channel], int) and 0 <= rc[channel] <= 255:
                r.ok(f"color_{channel}")
            else:
                r.fail(f"color_{channel}", f"Expected 0–255, got {rc[channel]!r}")

        if "zoom_factor" in rc:
            if isinstance(rc["zoom_factor"], (int, float)) and rc["zoom_factor"] > 0:
                r.ok("zoom_factor_positive")
            else:
                r.fail("zoom_factor_positive", f"Got {rc['zoom_factor']!r}")

        known_keys = {
            "x_resolution", "y_resolution",
            "clear_color_r", "clear_color_g", "clear_color_b",
            "cam_offset_x", "cam_offset_y", "zoom_factor", "cam_ease_factor",
        }
        unknown = set(rc.keys()) - known_keys
        if unknown:
            r.fail("no_unknown_keys", f"Unrecognized: {unknown}")
        else:
            r.ok("no_unknown_keys")

    # ==== SECTION 3: .template files ====
    print("\n\033[1m— .template files —\033[0m")

    all_templates = {}
    if not templates_dir.is_dir():
        r.skip("templates_directory_exists", "No actor_templates/ directory")
    else:
        r.ok("templates_directory_exists")

        parse_ok = True
        for f in sorted(templates_dir.glob("*.template")):
            try:
                data = load_json(f)
                assert isinstance(data, dict)
                all_templates[f.stem] = data
            except Exception as e:
                r.fail(f"template_parse [{f.name}]", str(e))
                parse_ok = False
        if parse_ok:
            r.ok(f"all_templates_valid_json [{len(all_templates)} files]")

        # Name field
        name_ok = True
        for tname, data in all_templates.items():
            if "name" not in data:
                r.fail(f"template_has_name [{tname}]", "Missing 'name'")
                name_ok = False
        if name_ok and all_templates:
            r.ok("all_templates_have_name")

        # Components structure
        comp_ok = True
        for tname, data in all_templates.items():
            if "components" not in data:
                continue
            comps = data["components"]
            if not isinstance(comps, dict):
                r.fail(f"template_components [{tname}]",
                       f"'components' is {type(comps).__name__}, expected dict")
                comp_ok = False
                continue
            for key, comp in comps.items():
                if not isinstance(comp, dict):
                    r.fail(f"template_component [{tname}][{key}]", "Not an object")
                    comp_ok = False
                elif "type" not in comp:
                    r.fail(f"template_component [{tname}][{key}]", "Missing 'type'")
                    comp_ok = False
        if comp_ok and all_templates:
            r.ok("all_template_components_well_formed")

        # Component types resolvable
        resolve_ok = True
        for tname, data in all_templates.items():
            for key, comp in data.get("components", {}).items():
                ct = comp.get("type", "")
                if ct in BUILTIN_COMPONENT_TYPES:
                    continue
                lua_path = components_dir / f"{ct}.lua"
                if not lua_path.exists():
                    r.fail(f"component_resolvable [{tname}→{ct}]",
                           f"{lua_path} not found")
                    resolve_ok = False
        if resolve_ok and all_templates:
            r.ok("all_template_component_types_resolvable")

        # Rigidbody property validation
        RB_SCHEMA = {
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

        rb_ok = True
        for tname, data in all_templates.items():
            for key, comp in data.get("components", {}).items():
                if comp.get("type") != "Rigidbody":
                    continue
                for prop, val in comp.items():
                    if prop == "type" or prop not in RB_SCHEMA:
                        continue
                    schema = RB_SCHEMA[prop]
                    if "allowed" in schema and val not in schema["allowed"]:
                        r.fail(f"rigidbody_prop [{tname}][{key}].{prop}",
                               f"'{val}' not in {schema['allowed']}")
                        rb_ok = False
                    if "type" in schema and not isinstance(val, schema["type"]):
                        r.fail(f"rigidbody_prop [{tname}][{key}].{prop}",
                               f"Expected {schema['type']}, got {type(val).__name__}")
                        rb_ok = False
                    if "min" in schema and isinstance(val, (int, float)) and val < schema["min"]:
                        r.fail(f"rigidbody_prop [{tname}][{key}].{prop}",
                               f"{val} < minimum {schema['min']}")
                        rb_ok = False
        if rb_ok and all_templates:
            r.ok("all_rigidbody_properties_valid")

    # ==== SECTION 4: .scene files ====
    print("\n\033[1m— .scene files —\033[0m")

    all_scenes = {}
    if not scenes_dir.is_dir():
        r.fail("scenes_directory_exists", f"No scenes/ at {scenes_dir}")
    else:
        r.ok("scenes_directory_exists")
        for f in sorted(scenes_dir.glob("*.scene")):
            try:
                all_scenes[f.stem] = load_json(f)
            except Exception as e:
                r.fail(f"scene_parse [{f.name}]", str(e))
        r.ok(f"all_scenes_valid_json [{len(all_scenes)} files]")

        for sname, data in all_scenes.items():
            if "actors" not in data:
                r.fail(f"scene_has_actors [{sname}]", "Missing 'actors'")
            elif not isinstance(data["actors"], list):
                r.fail(f"scene_has_actors [{sname}]", "'actors' is not an array")
            else:
                r.ok(f"scene_has_actors [{sname}]")

            names_ok = True
            for i, actor in enumerate(data.get("actors", [])):
                if "name" not in actor:
                    r.fail(f"scene_actor_name [{sname}][{i}]", "Missing 'name'")
                    names_ok = False
            if names_ok and data.get("actors"):
                r.ok(f"scene_actors_have_names [{sname}]")

            # Component types resolvable in scenes
            scene_resolve_ok = True
            for actor in data.get("actors", []):
                aname = actor.get("name", "?")
                for key, comp in actor.get("components", {}).items():
                    ct = comp.get("type", "")
                    if ct in BUILTIN_COMPONENT_TYPES:
                        continue
                    lua_path = components_dir / f"{ct}.lua"
                    if not lua_path.exists():
                        r.fail(f"scene_component [{sname}→{aname}→{ct}]",
                               f"{lua_path} not found")
                        scene_resolve_ok = False
            if scene_resolve_ok:
                r.ok(f"scene_component_types_resolvable [{sname}]")

    # ==== SECTION 5: Lua scripts ====
    print("\n\033[1m— Lua scripts —\033[0m")

    all_lua = {}
    if not components_dir.is_dir():
        r.fail("scripts_directory_exists", f"No component_types/ at {components_dir}")
    else:
        r.ok("scripts_directory_exists")
        for f in sorted(components_dir.glob("*.lua")):
            all_lua[f.stem] = f.read_text()

        # Table name matches filename
        table_ok = True
        for name, content in all_lua.items():
            pattern = rf"^{re.escape(name)}\s*="
            if not re.search(pattern, content, re.MULTILINE):
                r.fail(f"script_defines_table [{name}]",
                       f"No top-level '{name}' table definition found")
                table_ok = False
        if table_ok and all_lua:
            r.ok(f"all_scripts_define_matching_table [{len(all_lua)} scripts]")

        # Lifecycle hooks look like function definitions
        hooks = ["OnStart", "OnUpdate", "OnLateUpdate", "OnDestroy",
                 "OnCollisionEnter", "OnCollisionExit",
                 "OnTriggerEnter", "OnTriggerExit"]
        hook_ok = True
        for name, content in all_lua.items():
            for hook in hooks:
                if hook not in content:
                    continue
                func_pat = rf"{hook}\s*=\s*function\s*\("
                if not re.search(func_pat, content):
                    r.fail(f"lifecycle_hook [{name}.{hook}]",
                           "Referenced but not a function definition")
                    hook_ok = False
        if hook_ok and all_lua:
            r.ok("all_lifecycle_hooks_are_functions")

        # Self parameter check
        hook_pattern = re.compile(r"(On\w+)\s*=\s*function\s*\(([^)]*)\)")
        self_ok = True
        for name, content in all_lua.items():
            for match in hook_pattern.finditer(content):
                hook_name = match.group(1)
                params = match.group(2).strip()
                first = params.split(",")[0].strip() if params else ""
                if first != "self":
                    r.fail(f"self_param [{name}.{hook_name}]",
                           f"First param is '{first}', expected 'self'")
                    self_ok = False
        if self_ok and all_lua:
            r.ok("all_hooks_have_self_parameter")

    # ==== SECTION 6: Referential integrity ====
    print("\n\033[1m— Referential integrity —\033[0m")

    # Actor.Instantiate references
    instantiate_pattern = re.compile(r'Actor\.Instantiate\(\s*"([^"]+)"\s*\)')
    referenced_templates = set()
    for content in all_lua.values():
        referenced_templates.update(instantiate_pattern.findall(content))

    inst_ok = True
    for tname in sorted(referenced_templates):
        if tname not in all_templates:
            r.fail(f"instantiated_template_exists [{tname}]",
                   f"Actor.Instantiate(\"{tname}\") but {tname}.template missing")
            inst_ok = False
    if inst_ok and referenced_templates:
        r.ok(f"all_instantiated_templates_exist [{len(referenced_templates)} refs]")

    # Sprite images
    sprite_ok = True
    if images_dir.is_dir():
        for tname, data in all_templates.items():
            for key, comp in data.get("components", {}).items():
                if comp.get("type") != "SpriteRenderer":
                    continue
                sprite = comp.get("sprite")
                if sprite is None or sprite == "???":
                    continue
                sprite_path = images_dir / f"{sprite}.png"
                if not sprite_path.exists():
                    r.fail(f"sprite_exists [{tname}→{sprite}]",
                           f"{sprite_path} not found")
                    sprite_ok = False
        if sprite_ok:
            r.ok("all_sprite_images_exist")
    else:
        r.skip("sprite_images", "No images/ directory")

    # Orphan check
    scene_templates = set()
    for scene_data in all_scenes.values():
        for actor in scene_data.get("actors", []):
            if "template" in actor:
                scene_templates.add(actor["template"])
    all_referenced = referenced_templates | scene_templates
    orphans = set(all_templates.keys()) - all_referenced
    if orphans:
        r.xfailed("orphan_templates", f"Never instantiated: {orphans}")
    else:
        r.ok("no_orphan_templates")

    return r


# ---------------------------------------------------------------------------
# Entry
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    resources = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("resources")
    if not resources.is_dir():
        print(f"ERROR: Resources directory not found: {resources}")
        sys.exit(1)

    print(f"\n{'='*60}")
    print(f" youMadeItEngine — Config & Data Validation Tests")
    print(f" Resources: {resources.resolve()}")
    print(f"{'='*60}")

    result = run_all(resources)
    success = result.summary()
    sys.exit(0 if success else 1)
