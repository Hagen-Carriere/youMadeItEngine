# """
# test_regression.py — Regression & Snapshot Tests for youMadeItEngine.

# Captures known-good baselines for the engine's data and detects unexpected
# changes across code updates. Each test encodes a "golden" snapshot of how
# the engine is configured. When something changes — a template gets modified,
# a Lua script's API surface shifts, the stage layout moves a tile — these
# tests catch it and force an explicit acknowledgment.

# Covers:
#   • Resource file inventory (no missing or unexpected files)
#   • Template property snapshots (exact Rigidbody configs)
#   • Stage layout fingerprint (tile grid matches known-good state)
#   • Lua API surface (lifecycle hooks, GetComponent deps, event wiring)
#   • Scene structure (actor list, component assignments)
#   • Configuration value snapshots (rendering settings)
#   • Component count invariants (templates don't lose components)

# To update a baseline after an intentional change:
#   1. Run the failing test to see what changed
#   2. Update the expected values in this file
#   3. Commit the test change alongside the code change

# This ensures every data change is reviewed and intentional.
# """

# import hashlib
# import json
# import re
# import pytest
# from pathlib import Path


# # ==========================================================================
# #  SECTION 1 — Resource file inventory
# # ==========================================================================

# class TestResourceInventory:
#     """
#     Detect missing or unexpected resource files. If a file is added,
#     removed, or renamed, these tests catch it immediately.
#     """

#     EXPECTED_TEMPLATES = {"BouncyBox", "KinematicBox", "Player", "VictoryBox"}
#     EXPECTED_SCENES = {"basic"}
#     EXPECTED_LUA_SCRIPTS = {
#         "BouncyBox", "CameraManager", "GameManager", "Hud",
#         "KeyboardControls", "SpriteRenderer", "VictoryBox",
#     }
#     EXPECTED_IMAGES = {"box1", "box2", "box3", "box4", "circle"}

#     def test_template_inventory(self, templates_dir):
#         if not templates_dir.is_dir():
#             pytest.skip("No templates directory")
#         actual = {f.stem for f in templates_dir.glob("*.template")}
#         missing = self.EXPECTED_TEMPLATES - actual
#         added = actual - self.EXPECTED_TEMPLATES
#         assert not missing, f"Templates removed (update baseline if intentional): {missing}"
#         assert not added, f"New templates added (update baseline to include): {added}"

#     def test_scene_inventory(self, scenes_dir):
#         actual = {f.stem for f in scenes_dir.glob("*.scene")}
#         missing = self.EXPECTED_SCENES - actual
#         added = actual - self.EXPECTED_SCENES
#         assert not missing, f"Scenes removed: {missing}"
#         assert not added, f"New scenes added (update baseline): {added}"

#     def test_lua_script_inventory(self, components_dir):
#         actual = {f.stem for f in components_dir.glob("*.lua")}
#         missing = self.EXPECTED_LUA_SCRIPTS - actual
#         added = actual - self.EXPECTED_LUA_SCRIPTS
#         assert not missing, f"Lua scripts removed: {missing}"
#         assert not added, f"New Lua scripts added (update baseline): {added}"

#     def test_image_inventory(self, resources_dir):
#         images_dir = resources_dir / "images"
#         if not images_dir.is_dir():
#             pytest.skip("No images directory")
#         actual = {f.stem for f in images_dir.glob("*.png")}
#         missing = self.EXPECTED_IMAGES - actual
#         added = actual - self.EXPECTED_IMAGES
#         assert not missing, f"Images removed: {missing}"
#         assert not added, f"New images added (update baseline): {added}"


# # ==========================================================================
# #  SECTION 2 — Template property snapshots
# # ==========================================================================

# class TestTemplateSnapshots:
#     """
#     Exact property snapshots for each template's Rigidbody configuration.
#     Any change to physics properties affects gameplay feel and must be
#     reviewed explicitly.
#     """

#     PLAYER_RIGIDBODY = {
#         "type": "Rigidbody",
#         "collider_type": "circle",
#         "radius": 0.45,
#         "density": 0.5,
#         "trigger_radius": 0.55,
#     }

#     KINEMATIC_BOX_RIGIDBODY = {
#         "type": "Rigidbody",
#         "body_type": "kinematic",
#         "has_trigger": False,
#     }

#     BOUNCY_BOX_RIGIDBODY = {
#         "type": "Rigidbody",
#         "body_type": "kinematic",
#         "has_trigger": False,
#     }

#     VICTORY_BOX_RIGIDBODY = {
#         "type": "Rigidbody",
#         "body_type": "kinematic",
#         "has_collider": False,
#     }

#     def _get_rigidbody(self, template_data: dict) -> dict | None:
#         for comp in template_data.get("components", {}).values():
#             if comp.get("type") == "Rigidbody":
#                 return comp
#         return None

#     def test_player_rigidbody_snapshot(self, all_templates):
#         player = all_templates.get("Player")
#         assert player is not None, "Player template missing"
#         rb = self._get_rigidbody(player)
#         assert rb is not None, "Player has no Rigidbody"
#         assert rb == self.PLAYER_RIGIDBODY, (
#             f"Player Rigidbody changed!\n"
#             f"  Expected: {self.PLAYER_RIGIDBODY}\n"
#             f"  Actual:   {rb}\n"
#             f"  If intentional, update PLAYER_RIGIDBODY in test_regression.py"
#         )

#     def test_kinematic_box_rigidbody_snapshot(self, all_templates):
#         kb = all_templates.get("KinematicBox")
#         assert kb is not None, "KinematicBox template missing"
#         rb = self._get_rigidbody(kb)
#         assert rb is not None, "KinematicBox has no Rigidbody"
#         assert rb == self.KINEMATIC_BOX_RIGIDBODY, (
#             f"KinematicBox Rigidbody changed!\n"
#             f"  Expected: {self.KINEMATIC_BOX_RIGIDBODY}\n"
#             f"  Actual:   {rb}\n"
#             f"  Update KINEMATIC_BOX_RIGIDBODY if intentional."
#         )

#     def test_bouncy_box_rigidbody_snapshot(self, all_templates):
#         bb = all_templates.get("BouncyBox")
#         assert bb is not None, "BouncyBox template missing"
#         rb = self._get_rigidbody(bb)
#         assert rb is not None, "BouncyBox has no Rigidbody"
#         assert rb == self.BOUNCY_BOX_RIGIDBODY, (
#             f"BouncyBox Rigidbody changed!\n"
#             f"  Expected: {self.BOUNCY_BOX_RIGIDBODY}\n"
#             f"  Actual:   {rb}\n"
#             f"  Update BOUNCY_BOX_RIGIDBODY if intentional."
#         )

#     def test_victory_box_rigidbody_snapshot(self, all_templates):
#         vb = all_templates.get("VictoryBox")
#         assert vb is not None, "VictoryBox template missing"
#         rb = self._get_rigidbody(vb)
#         assert rb is not None, "VictoryBox has no Rigidbody"
#         assert rb == self.VICTORY_BOX_RIGIDBODY, (
#             f"VictoryBox Rigidbody changed!\n"
#             f"  Expected: {self.VICTORY_BOX_RIGIDBODY}\n"
#             f"  Actual:   {rb}\n"
#             f"  Update VICTORY_BOX_RIGIDBODY if intentional."
#         )

#     def test_template_component_counts(self, all_templates):
#         """
#         Track the number of components per template.
#         A dropped component is almost always a bug.
#         """
#         EXPECTED_COUNTS = {
#             "Player": 3,        # Rigidbody + KeyboardControls + SpriteRenderer
#             "KinematicBox": 2,  # SpriteRenderer + Rigidbody
#             "BouncyBox": 3,     # SpriteRenderer + Rigidbody + BouncyBox
#             "VictoryBox": 3,    # SpriteRenderer + Rigidbody + VictoryBox
#         }
#         for tname, expected_count in EXPECTED_COUNTS.items():
#             if tname not in all_templates:
#                 continue
#             actual = len(all_templates[tname].get("components", {}))
#             assert actual == expected_count, (
#                 f"{tname} component count changed: {actual} (was {expected_count}). "
#                 f"If intentional, update EXPECTED_COUNTS."
#             )


# # ==========================================================================
# #  SECTION 3 — Stage layout fingerprint
# # ==========================================================================

# class TestStageLayoutRegression:
#     """
#     The stage tile grid IS the level design. Any change affects
#     gameplay, difficulty, and player experience. These tests
#     snapshot the exact grid and key metrics.
#     """

#     # Golden snapshot of GameManager.stage1
#     EXPECTED_STAGE_HASH = None  # Computed below

#     EXPECTED_GRID = [
#         [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
#         [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,1,1,1,1,0,1,1,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
#         [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
#         [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
#         [1,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1],
#         [1,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1],
#         [1,1,1,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0,0,1],
#         [1,0,0,0,0,0,0,1,0,0,1,1,3,3,1,1,1,0,0,1],
#         [1,0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,0,0,0,1],
#         [1,0,2,0,0,0,0,0,1,1,0,0,0,0,1,0,0,0,1,1],
#         [1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1],
#         [1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1],
#         [1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1],
#     ]

#     # Pre-computed tile counts from the golden grid
#     EXPECTED_TILE_COUNTS = {
#         0: 250,  # empty
#         1: 138,  # kinematic box (walls/platforms)
#         2: 1,    # player spawn
#         3: 2,    # bouncy box
#         4: 9,    # victory box
#     }

#     EXPECTED_PLAYER_SPAWN = (3, 17)  # (x, y) in 1-indexed Lua coords
#     EXPECTED_GRID_SIZE = (20, 20)    # (width, height)

#     def _extract_stage_grid(self, lua_source: str) -> list[list[int]] | None:
#         """Parse the stage1 grid from GameManager.lua source."""
#         # Find stage1 = { ... }
#         match = re.search(r"stage1\s*=\s*\{(.+?)\n\t\}", lua_source, re.DOTALL)
#         if not match:
#             return None

#         grid = []
#         row_pattern = re.compile(r"\{([^}]+)\}")
#         for row_match in row_pattern.finditer(match.group(1)):
#             row_str = row_match.group(1)
#             row = [int(x.strip()) for x in row_str.split(",") if x.strip()]
#             grid.append(row)
#         return grid

#     def _grid_hash(self, grid: list[list[int]]) -> str:
#         """Deterministic hash of a tile grid."""
#         flat = json.dumps(grid, separators=(",", ":"))
#         return hashlib.sha256(flat.encode()).hexdigest()[:16]

#     def test_stage_grid_matches_baseline(self, all_lua_scripts):
#         """The exact tile grid must match the known-good snapshot."""
#         source = all_lua_scripts.get("GameManager", "")
#         grid = self._extract_stage_grid(source)
#         assert grid is not None, "Could not parse stage1 from GameManager.lua"
#         assert grid == self.EXPECTED_GRID, (
#             "Stage layout has changed! Diff the grids to see what moved.\n"
#             "If intentional, update EXPECTED_GRID in test_regression.py."
#         )

#     def test_grid_dimensions(self, all_lua_scripts):
#         """Stage must remain 20x20."""
#         source = all_lua_scripts.get("GameManager", "")
#         grid = self._extract_stage_grid(source)
#         assert grid is not None

#         height = len(grid)
#         widths = {len(row) for row in grid}

#         assert height == self.EXPECTED_GRID_SIZE[1], (
#             f"Grid height changed: {height} (was {self.EXPECTED_GRID_SIZE[1]})"
#         )
#         assert len(widths) == 1, f"Rows have inconsistent widths: {widths}"
#         assert widths.pop() == self.EXPECTED_GRID_SIZE[0], (
#             f"Grid width changed (was {self.EXPECTED_GRID_SIZE[0]})"
#         )

#     def test_tile_counts_match_baseline(self, all_lua_scripts):
#         """
#         Exact count of each tile type. Detects if platforms were added/removed,
#         bouncy pads were moved, or victory zone was resized.
#         """
#         source = all_lua_scripts.get("GameManager", "")
#         grid = self._extract_stage_grid(source)
#         assert grid is not None

#         counts = {}
#         for row in grid:
#             for tile in row:
#                 counts[tile] = counts.get(tile, 0) + 1

#         assert counts == self.EXPECTED_TILE_COUNTS, (
#             f"Tile counts changed!\n"
#             f"  Expected: {self.EXPECTED_TILE_COUNTS}\n"
#             f"  Actual:   {counts}\n"
#             f"  Update EXPECTED_TILE_COUNTS if intentional."
#         )

#     def test_player_spawn_unchanged(self, all_lua_scripts):
#         """Player spawn position must remain at (3, 17)."""
#         source = all_lua_scripts.get("GameManager", "")
#         grid = self._extract_stage_grid(source)
#         assert grid is not None

#         spawns = []
#         for y, row in enumerate(grid):
#             for x, tile in enumerate(row):
#                 if tile == 2:
#                     spawns.append((x + 1, y + 1))  # 1-indexed

#         assert len(spawns) == 1, f"Expected 1 player spawn, found {len(spawns)}"
#         assert spawns[0] == self.EXPECTED_PLAYER_SPAWN, (
#             f"Player spawn moved to {spawns[0]} (was {self.EXPECTED_PLAYER_SPAWN}). "
#             f"Update EXPECTED_PLAYER_SPAWN if intentional."
#         )

#     def test_border_walls_intact(self, all_lua_scripts):
#         """
#         Row 20 (bottom) and columns 1/20 should be mostly walls (tile 1).
#         Holes in the border let the player fall out of the level.
#         """
#         source = all_lua_scripts.get("GameManager", "")
#         grid = self._extract_stage_grid(source)
#         assert grid is not None

#         # Bottom row (index 19) should be predominantly walls
#         bottom = grid[19]
#         wall_count = sum(1 for t in bottom if t == 1)
#         assert wall_count >= 14, (
#             f"Bottom row has only {wall_count} walls (expected ≥14). "
#             f"Player may fall out of the level."
#         )

#         # Left column (index 0) should all be walls
#         left_col = [grid[y][0] for y in range(20)]
#         assert all(t == 1 for t in left_col), (
#             "Left border has gaps — player can escape the level."
#         )

#         # Right column (index 19) should all be walls
#         right_col = [grid[y][19] for y in range(20)]
#         assert all(t == 1 for t in right_col), (
#             "Right border has gaps — player can escape the level."
#         )


# # ==========================================================================
# #  SECTION 4 — Lua API surface snapshot
# # ==========================================================================

# class TestLuaAPISurfaceRegression:
#     """
#     Snapshot each script's public API surface: lifecycle hooks,
#     dependencies, and engine API calls. Changes here affect how
#     the engine interacts with the script.
#     """

#     # Golden snapshot: script → set of lifecycle hooks defined
#     EXPECTED_HOOKS = {
#         "BouncyBox": {"OnCollisionEnter"},
#         "CameraManager": {"OnUpdate"},
#         "GameManager": {"OnStart", "OnUpdate"},
#         "Hud": {"OnStart", "OnUpdate", "OnEventVictory"},
#         "KeyboardControls": {"OnStart", "OnUpdate"},
#         "SpriteRenderer": {"OnStart", "OnUpdate"},
#         "VictoryBox": {"OnTriggerEnter"},
#     }

#     # Golden snapshot: script → set of GetComponent types called on self
#     EXPECTED_SELF_DEPS = {
#         "KeyboardControls": {"Rigidbody"},
#         "SpriteRenderer": {"Rigidbody"},
#     }

#     # Golden snapshot: event wiring
#     EXPECTED_EVENT_PUBLISHERS = {"VictoryBox": ["event_victory"]}
#     EXPECTED_EVENT_SUBSCRIBERS = {"Hud": ["event_victory"]}

#     def _extract_hooks(self, source: str) -> set[str]:
#         """Find all function definitions that look like lifecycle hooks."""
#         return set(re.findall(r"(\w+)\s*=\s*function\s*\(self", source))

#     def _extract_self_deps(self, source: str) -> set[str]:
#         return set(re.findall(r"self\.actor:GetComponent\(\s*\"([^\"]+)\"\s*\)", source))

#     def test_lifecycle_hooks_unchanged(self, all_lua_scripts):
#         """Each script's set of hooks must match the baseline."""
#         for script_name, expected_hooks in self.EXPECTED_HOOKS.items():
#             source = all_lua_scripts.get(script_name, "")
#             actual = self._extract_hooks(source)
#             assert actual == expected_hooks, (
#                 f"{script_name}.lua hooks changed!\n"
#                 f"  Expected: {expected_hooks}\n"
#                 f"  Actual:   {actual}\n"
#                 f"  Update EXPECTED_HOOKS if intentional."
#             )

#     def test_self_dependencies_unchanged(self, all_lua_scripts):
#         """Each script's GetComponent dependencies must match baseline."""
#         for script_name, expected_deps in self.EXPECTED_SELF_DEPS.items():
#             source = all_lua_scripts.get(script_name, "")
#             actual = self._extract_self_deps(source)
#             assert actual == expected_deps, (
#                 f"{script_name}.lua dependencies changed!\n"
#                 f"  Expected: {expected_deps}\n"
#                 f"  Actual:   {actual}\n"
#                 f"  Update EXPECTED_SELF_DEPS if intentional."
#             )

#     def test_event_publishers_unchanged(self, all_lua_scripts):
#         for script_name, expected_events in self.EXPECTED_EVENT_PUBLISHERS.items():
#             source = all_lua_scripts.get(script_name, "")
#             actual = re.findall(r'Event\.Publish\(\s*"([^"]+)"\s*\)', source)
#             assert actual == expected_events, (
#                 f"{script_name}.lua publishes changed!\n"
#                 f"  Expected: {expected_events}\n"
#                 f"  Actual:   {actual}\n"
#                 f"  Update EXPECTED_EVENT_PUBLISHERS if intentional."
#             )

#     def test_event_subscribers_unchanged(self, all_lua_scripts):
#         for script_name, expected_events in self.EXPECTED_EVENT_SUBSCRIBERS.items():
#             source = all_lua_scripts.get(script_name, "")
#             actual = re.findall(r'Event\.Subscribe\(\s*"([^"]+)"\s*,', source)
#             assert actual == expected_events, (
#                 f"{script_name}.lua subscriptions changed!\n"
#                 f"  Expected: {expected_events}\n"
#                 f"  Actual:   {actual}\n"
#                 f"  Update EXPECTED_EVENT_SUBSCRIBERS if intentional."
#             )


# # ==========================================================================
# #  SECTION 5 — Scene structure snapshot
# # ==========================================================================

# class TestSceneStructureRegression:
#     """
#     Snapshot the exact actor/component structure of each scene.
#     Changes here affect what loads when the game starts.
#     """

#     EXPECTED_BASIC_SCENE_ACTORS = [
#         {
#             "name": "camera",
#             "components": {"1": {"type": "CameraManager"}},
#         },
#         {
#             "name": "GameManager",
#             "components": {"1": {"type": "GameManager"}},
#         },
#         {
#             "name": "HUD",
#             "components": {"1": {"type": "Hud"}},
#         },
#     ]

#     def test_basic_scene_actors_match_baseline(self, all_scenes):
#         scene = all_scenes.get("basic")
#         assert scene is not None, "basic.scene missing"
#         actors = scene.get("actors", [])
#         assert actors == self.EXPECTED_BASIC_SCENE_ACTORS, (
#             f"basic.scene structure changed!\n"
#             f"  Expected: {json.dumps(self.EXPECTED_BASIC_SCENE_ACTORS, indent=2)}\n"
#             f"  Actual:   {json.dumps(actors, indent=2)}\n"
#             f"  Update EXPECTED_BASIC_SCENE_ACTORS if intentional."
#         )

#     def test_basic_scene_actor_count(self, all_scenes):
#         scene = all_scenes.get("basic")
#         assert scene is not None
#         assert len(scene.get("actors", [])) == 3, (
#             "basic.scene should have exactly 3 actors: camera, GameManager, HUD"
#         )

#     def test_basic_scene_actor_order(self, all_scenes):
#         """Load order matters — camera and GameManager must load before HUD."""
#         scene = all_scenes.get("basic")
#         assert scene is not None
#         names = [a.get("name") for a in scene.get("actors", [])]
#         assert names == ["camera", "GameManager", "HUD"], (
#             f"Actor load order changed: {names}. "
#             f"HUD subscribes to events from GameManager-spawned actors, "
#             f"so GameManager must load first."
#         )


# # ==========================================================================
# #  SECTION 6 — Configuration value snapshots
# # ==========================================================================

# class TestConfigRegression:
#     """
#     Snapshot exact configuration values. Unintended changes here
#     affect window size, camera behavior, and visual appearance.
#     """

#     def test_game_config_snapshot(self, game_config):
#         expected = {
#             "game_title": "testcase 9-1",
#             "initial_scene": "basic",
#         }
#         assert game_config == expected, (
#             f"game.config changed!\n"
#             f"  Expected: {expected}\n"
#             f"  Actual:   {game_config}\n"
#             f"  Update test if intentional."
#         )

#     def test_rendering_config_snapshot(self, rendering_config):
#         if rendering_config is None:
#             pytest.skip("No rendering.config")

#         expected = {
#             "clear_color_r": 255,
#             "clear_color_g": 255,
#             "clear_color_b": 255,
#             "x_resolution": 960,
#             "y_resolution": 540,
#         }
#         assert rendering_config == expected, (
#             f"rendering.config changed!\n"
#             f"  Expected: {expected}\n"
#             f"  Actual:   {rendering_config}\n"
#             f"  Update test if intentional."
#         )


# # ==========================================================================
# #  SECTION 7 — Gameplay constant snapshots
# # ==========================================================================

# class TestGameplayConstantsRegression:
#     """
#     Snapshot tuning constants that directly affect gameplay feel.
#     Changes here alter how the game plays even if code logic is unchanged.
#     """

#     def _get_defaults(self, source: str, table_name: str) -> dict[str, str]:
#         defaults = {}
#         prop_pattern = re.compile(r"^\s+(\w+)\s*=\s*(?!function)(.+?)\s*,?\s*$")
#         in_table = False
#         depth = 0
#         for line in source.splitlines():
#             if re.match(rf"^{re.escape(table_name)}\s*=\s*\{{", line):
#                 in_table = True
#                 depth = line.count("{") - line.count("}")
#                 continue
#             if in_table:
#                 depth += line.count("{") - line.count("}")
#                 if depth <= 0:
#                     break
#                 if depth == 1:
#                     match = prop_pattern.match(line)
#                     if match:
#                         defaults[match.group(1)] = match.group(2).strip()
#         return defaults

#     def test_player_movement_constants(self, all_lua_scripts):
#         source = all_lua_scripts.get("KeyboardControls", "")
#         d = self._get_defaults(source, "KeyboardControls")
#         assert d.get("speed") == "5", (
#             f"Player speed changed to {d.get('speed')} (was 5). "
#             f"This affects horizontal movement feel."
#         )
#         assert d.get("jump_power") == "350", (
#             f"Jump power changed to {d.get('jump_power')} (was 350). "
#             f"This affects jump height and platforming difficulty."
#         )

#     def test_camera_constants(self, all_lua_scripts):
#         source = all_lua_scripts.get("CameraManager", "")
#         d = self._get_defaults(source, "CameraManager")
#         assert d.get("ease_factor") == "0.1", (
#             f"Camera ease changed to {d.get('ease_factor')} (was 0.1). "
#             f"This affects how smoothly the camera follows the player."
#         )

#     def test_bounce_velocity(self, all_lua_scripts):
#         """BouncyBox sets vertical velocity to -15 on collision."""
#         source = all_lua_scripts.get("BouncyBox", "")
#         assert "Vector2(current_vel.x, -15)" in source, (
#             "BouncyBox bounce velocity changed. Look for the Vector2 "
#             "constructor in OnCollisionEnter."
#         )

#     def test_hud_timer_interval(self, all_lua_scripts):
#         """HUD increments timer every 60 frames (1 second at 60fps)."""
#         source = all_lua_scripts.get("Hud", "")
#         assert "% 60 ==" in source or "%60 ==" in source or "% 60==" in source, (
#             "HUD timer interval changed. Should tick every 60 frames."
#         )
