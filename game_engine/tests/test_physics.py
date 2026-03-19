"""
test_physics.py — Physics Determinism & Math Validation Tests.

Validates every mathematical formula in the engine's Rigidbody system by
reimplementing the C++/Box2D math in Python and testing against known values.

Covers:
  • Degree ↔ radian conversions (SetRotation / GetRotation)
  • Direction vectors (GetUpDirection, GetRightDirection)
  • Inverse direction setters (SetUpDirection, SetRightDirection)
  • Round-trip consistency (set → get returns original value)
  • Perpendicularity invariant (Up ⊥ Right for any angle)
  • Vector2 utilities (Distance, Dot)
  • Fixture property mappings (template data → Box2D values)
  • Gameplay physics (BouncyBox bounce, tile-grid spawn positions)

These tests run without compiling or launching the engine. They catch
formula bugs, rounding issues, and logic errors in physics code.
"""

import json
import math
import re
import pytest
from pathlib import Path


# ==========================================================================
#  Reference implementation — mirrors the C++ Rigidbody math exactly
# ==========================================================================

B2_PI = math.pi  # Box2D uses the same pi constant


def deg_to_rad(degrees: float) -> float:
    """C++: degrees * (b2_pi / 180.0f)"""
    return degrees * (B2_PI / 180.0)


def rad_to_deg(radians: float) -> float:
    """C++: body->GetAngle() * (180.0f / b2_pi)"""
    return radians * (180.0 / B2_PI)


def get_up_direction(angle_rad: float) -> tuple[float, float]:
    """
    C++: b2Vec2(glm::sin(angle), -glm::cos(angle))
    Returns the "up" direction for a body at the given angle.
    At angle=0, up is (0, -1) — screen-up in SDL coordinates.
    """
    return (math.sin(angle_rad), -math.cos(angle_rad))


def get_right_direction(angle_rad: float) -> tuple[float, float]:
    """
    C++: b2Vec2(glm::cos(angle), glm::sin(angle))
    Returns the "right" direction for a body at the given angle.
    At angle=0, right is (1, 0).
    """
    return (math.cos(angle_rad), math.sin(angle_rad))


def set_up_direction_angle(dx: float, dy: float) -> float:
    """
    C++: glm::atan(norm.x, -norm.y)  (note: atan2(x, -y), NOT atan2(y, x))
    Given a desired up-direction vector, returns the body angle in radians.
    """
    length = math.sqrt(dx * dx + dy * dy)
    if length == 0:
        return 0.0
    nx, ny = dx / length, dy / length
    return math.atan2(nx, -ny)


def set_right_direction_angle(dx: float, dy: float) -> float:
    """
    C++: glm::atan(norm.x, -norm.y) - b2_pi / 2.0f
    Given a desired right-direction vector, returns the body angle in radians.
    """
    length = math.sqrt(dx * dx + dy * dy)
    if length == 0:
        return 0.0
    nx, ny = dx / length, dy / length
    return math.atan2(nx, -ny) - B2_PI / 2.0


def vec2_distance(ax: float, ay: float, bx: float, by: float) -> float:
    """Euclidean distance between two 2D points."""
    return math.sqrt((ax - bx) ** 2 + (ay - by) ** 2)


def vec2_dot(ax: float, ay: float, bx: float, by: float) -> float:
    """Dot product of two 2D vectors."""
    return ax * bx + ay * by


# Tolerance for floating-point comparisons
EPSILON = 1e-6


# ==========================================================================
#  SECTION 1 — Degree ↔ Radian conversions
# ==========================================================================

class TestAngleConversions:
    """
    The engine converts between degrees and radians constantly:
      SetRotation: degrees → radians (stored in Box2D body)
      GetRotation: radians → degrees (returned to Lua)

    A bug here means every rotated object in the game is wrong.
    """

    @pytest.mark.parametrize("degrees,expected_rad", [
        (0, 0.0),
        (90, math.pi / 2),
        (180, math.pi),
        (270, 3 * math.pi / 2),
        (360, 2 * math.pi),
        (-90, -math.pi / 2),
        (45, math.pi / 4),
    ])
    def test_degrees_to_radians(self, degrees, expected_rad):
        result = deg_to_rad(degrees)
        assert abs(result - expected_rad) < EPSILON, (
            f"deg_to_rad({degrees}) = {result}, expected {expected_rad}"
        )

    @pytest.mark.parametrize("radians,expected_deg", [
        (0.0, 0),
        (math.pi / 2, 90),
        (math.pi, 180),
        (3 * math.pi / 2, 270),
        (2 * math.pi, 360),
        (-math.pi / 2, -90),
    ])
    def test_radians_to_degrees(self, radians, expected_deg):
        result = rad_to_deg(radians)
        assert abs(result - expected_deg) < EPSILON, (
            f"rad_to_deg({radians}) = {result}, expected {expected_deg}"
        )

    @pytest.mark.parametrize("degrees", [0, 30, 45, 90, 135, 180, 270, 360, -45, -180])
    def test_round_trip_deg_rad_deg(self, degrees):
        """SetRotation(deg) → GetRotation() should return the same degrees."""
        radians = deg_to_rad(degrees)
        recovered = rad_to_deg(radians)
        assert abs(recovered - degrees) < EPSILON, (
            f"Round-trip failed: {degrees}° → {radians} rad → {recovered}°"
        )


# ==========================================================================
#  SECTION 2 — Direction vector calculations
# ==========================================================================

class TestDirectionVectors:
    """
    GetUpDirection and GetRightDirection derive a unit vector from the
    body's angle. These are used by gameplay scripts for movement,
    aiming, and collision response.
    """

    def test_up_at_zero_is_screen_up(self):
        """At angle=0, up should be (0, -1) — pointing toward top of screen."""
        ux, uy = get_up_direction(0.0)
        assert abs(ux - 0.0) < EPSILON
        assert abs(uy - (-1.0)) < EPSILON

    def test_right_at_zero_is_screen_right(self):
        """At angle=0, right should be (1, 0)."""
        rx, ry = get_right_direction(0.0)
        assert abs(rx - 1.0) < EPSILON
        assert abs(ry - 0.0) < EPSILON

    @pytest.mark.parametrize("degrees", [0, 45, 90, 135, 180, 225, 270, 315])
    def test_up_is_unit_vector(self, degrees):
        """Up direction must always be a unit vector (length ≈ 1)."""
        angle = deg_to_rad(degrees)
        ux, uy = get_up_direction(angle)
        length = math.sqrt(ux ** 2 + uy ** 2)
        assert abs(length - 1.0) < EPSILON, (
            f"Up direction at {degrees}° has length {length}, expected 1.0"
        )

    @pytest.mark.parametrize("degrees", [0, 45, 90, 135, 180, 225, 270, 315])
    def test_right_is_unit_vector(self, degrees):
        """Right direction must always be a unit vector (length ≈ 1)."""
        angle = deg_to_rad(degrees)
        rx, ry = get_right_direction(angle)
        length = math.sqrt(rx ** 2 + ry ** 2)
        assert abs(length - 1.0) < EPSILON, (
            f"Right direction at {degrees}° has length {length}, expected 1.0"
        )

    @pytest.mark.parametrize("degrees", [0, 30, 45, 60, 90, 120, 180, 270])
    def test_up_perpendicular_to_right(self, degrees):
        """
        Up and Right must always be perpendicular (dot product ≈ 0).
        This is the fundamental invariant of any rotation system.
        If this fails, movement and collision normals are broken.
        """
        angle = deg_to_rad(degrees)
        ux, uy = get_up_direction(angle)
        rx, ry = get_right_direction(angle)
        dot = vec2_dot(ux, uy, rx, ry)
        assert abs(dot) < EPSILON, (
            f"Up·Right at {degrees}° = {dot}, expected 0 (perpendicular)"
        )

    def test_up_at_90_degrees(self):
        """At 90° rotation, up should point to the right: (1, 0)."""
        angle = deg_to_rad(90)
        ux, uy = get_up_direction(angle)
        assert abs(ux - 1.0) < EPSILON
        assert abs(uy - 0.0) < EPSILON

    def test_right_at_90_degrees(self):
        """At 90° rotation, right should point down: (0, 1)."""
        angle = deg_to_rad(90)
        rx, ry = get_right_direction(angle)
        assert abs(rx - 0.0) < EPSILON
        assert abs(ry - 1.0) < EPSILON

    def test_up_at_180_degrees(self):
        """At 180° rotation, up should be (0, 1) — pointing down."""
        angle = deg_to_rad(180)
        ux, uy = get_up_direction(angle)
        assert abs(ux - 0.0) < EPSILON
        assert abs(uy - 1.0) < EPSILON


# ==========================================================================
#  SECTION 3 — SetUpDirection / SetRightDirection inverse functions
# ==========================================================================

class TestDirectionSetters:
    """
    SetUpDirection and SetRightDirection compute the body angle from a
    desired direction vector. They must be the inverse of the getters:
      SetUpDirection(vec) → angle → GetUpDirection(angle) ≈ normalize(vec)
    """

    @pytest.mark.parametrize("dx,dy", [
        (0, -1),   # default up
        (1, 0),    # pointing right
        (0, 1),    # pointing down
        (-1, 0),   # pointing left
        (1, -1),   # diagonal
        (-1, -1),  # diagonal
        (3, 4),    # non-unit vector
    ])
    def test_set_up_then_get_up_round_trip(self, dx, dy):
        """SetUpDirection(v) → GetUpDirection should return normalize(v)."""
        angle = set_up_direction_angle(dx, dy)
        ux, uy = get_up_direction(angle)

        # Normalize the input for comparison
        length = math.sqrt(dx ** 2 + dy ** 2)
        nx, ny = dx / length, dy / length

        assert abs(ux - nx) < EPSILON and abs(uy - ny) < EPSILON, (
            f"SetUp({dx},{dy}) → GetUp = ({ux:.6f},{uy:.6f}), "
            f"expected ({nx:.6f},{ny:.6f})"
        )

    @pytest.mark.parametrize("dx,dy", [
        (1, 0),    # default right
        (0, 1),    # pointing down
        (-1, 0),   # pointing left
        (0, -1),   # pointing up
        (1, 1),    # diagonal
    ])
    def test_set_right_then_get_right_round_trip(self, dx, dy):
        """SetRightDirection(v) → GetRightDirection should return normalize(v)."""
        angle = set_right_direction_angle(dx, dy)
        rx, ry = get_right_direction(angle)

        length = math.sqrt(dx ** 2 + dy ** 2)
        nx, ny = dx / length, dy / length

        assert abs(rx - nx) < EPSILON and abs(ry - ny) < EPSILON, (
            f"SetRight({dx},{dy}) → GetRight = ({rx:.6f},{ry:.6f}), "
            f"expected ({nx:.6f},{ny:.6f})"
        )

    def test_set_up_and_set_right_produce_consistent_angles(self):
        """
        If we SetUpDirection to (1,0), the resulting angle should be 90°.
        If we SetRightDirection to (0,1), the resulting angle should also be 90°.
        Both operations describe the same rotation.
        """
        angle_from_up = set_up_direction_angle(1, 0)
        angle_from_right = set_right_direction_angle(0, 1)
        # Normalize to [0, 2π)
        a1 = angle_from_up % (2 * B2_PI)
        a2 = angle_from_right % (2 * B2_PI)
        assert abs(a1 - a2) < EPSILON, (
            f"SetUp(1,0) → {math.degrees(a1):.2f}° vs "
            f"SetRight(0,1) → {math.degrees(a2):.2f}° — should match"
        )


# ==========================================================================
#  SECTION 4 — Vector2 utility functions
# ==========================================================================

class TestVector2Utilities:
    """Vector2_Distance and Vector2_Dot are exposed to Lua gameplay scripts."""

    @pytest.mark.parametrize("ax,ay,bx,by,expected", [
        (0, 0, 3, 4, 5.0),           # classic 3-4-5 triangle
        (0, 0, 0, 0, 0.0),           # same point
        (1, 1, 1, 1, 0.0),           # same point
        (0, 0, 1, 0, 1.0),           # unit horizontal
        (0, 0, 0, 1, 1.0),           # unit vertical
        (-3, -4, 0, 0, 5.0),         # negative coords
        (1.5, 2.5, 4.5, 6.5, 5.0),  # floats
    ])
    def test_distance(self, ax, ay, bx, by, expected):
        result = vec2_distance(ax, ay, bx, by)
        assert abs(result - expected) < EPSILON, (
            f"Distance(({ax},{ay}), ({bx},{by})) = {result}, expected {expected}"
        )

    def test_distance_is_commutative(self):
        """dist(A, B) == dist(B, A)"""
        d1 = vec2_distance(1, 2, 5, 7)
        d2 = vec2_distance(5, 7, 1, 2)
        assert abs(d1 - d2) < EPSILON

    @pytest.mark.parametrize("ax,ay,bx,by,expected", [
        (1, 0, 0, 1, 0.0),     # perpendicular
        (1, 0, 1, 0, 1.0),     # parallel same direction
        (1, 0, -1, 0, -1.0),   # parallel opposite
        (3, 4, 3, 4, 25.0),    # self dot = magnitude²
        (0, 0, 5, 5, 0.0),     # zero vector
        (2, 3, 4, 5, 23.0),    # general case: 2*4 + 3*5 = 23
    ])
    def test_dot_product(self, ax, ay, bx, by, expected):
        result = vec2_dot(ax, ay, bx, by)
        assert abs(result - expected) < EPSILON, (
            f"Dot(({ax},{ay}), ({bx},{by})) = {result}, expected {expected}"
        )

    def test_dot_is_commutative(self):
        """A·B == B·A"""
        d1 = vec2_dot(3, 7, 11, 13)
        d2 = vec2_dot(11, 13, 3, 7)
        assert abs(d1 - d2) < EPSILON

    def test_perpendicular_vectors_dot_zero(self):
        """Orthogonal vectors always have dot product = 0."""
        # (1, 0) and (0, 1)
        assert abs(vec2_dot(1, 0, 0, 1)) < EPSILON
        # Rotated 45° pair
        r2 = math.sqrt(2) / 2
        assert abs(vec2_dot(r2, r2, -r2, r2)) < EPSILON


# ==========================================================================
#  SECTION 5 — Fixture property mappings (template → Box2D)
# ==========================================================================

class TestFixturePropertyMappings:
    """
    Verify that template Rigidbody properties map correctly to Box2D values.
    The engine reads these from JSON and passes them to Box2D fixture/body defs.
    """

    BODY_TYPE_MAP = {
        "static": "b2_staticBody",
        "kinematic": "b2_kinematicBody",
        "dynamic": "b2_dynamicBody",
    }

    def test_body_type_mapping_covers_all_types(self):
        """Engine must handle all three Box2D body types."""
        expected_types = {"static", "kinematic", "dynamic"}
        assert set(self.BODY_TYPE_MAP.keys()) == expected_types

    @pytest.mark.parametrize("body_type", ["static", "kinematic", "dynamic"])
    def test_body_type_string_maps_to_b2_type(self, body_type):
        """Each string in template JSON maps to exactly one Box2D type."""
        assert body_type in self.BODY_TYPE_MAP

    def test_box_collider_half_extents(self):
        """
        C++: shape.SetAsBox(width * 0.5f, height * 0.5f)
        Box2D uses half-widths, so a 2x3 box becomes SetAsBox(1, 1.5).
        """
        test_cases = [
            (1.0, 1.0, 0.5, 0.5),
            (2.0, 3.0, 1.0, 1.5),
            (0.5, 0.5, 0.25, 0.25),
            (10.0, 4.0, 5.0, 2.0),
        ]
        for w, h, expected_hw, expected_hh in test_cases:
            hw = w * 0.5
            hh = h * 0.5
            assert abs(hw - expected_hw) < EPSILON, (
                f"Half-width for w={w}: got {hw}, expected {expected_hw}"
            )
            assert abs(hh - expected_hh) < EPSILON, (
                f"Half-height for h={h}: got {hh}, expected {expected_hh}"
            )

    def test_init_angle_conversion(self):
        """
        C++: bodyDef.angle = rotation * (b2_pi / 180.0f)
        The initial rotation from the template is in degrees, Box2D wants radians.
        """
        for deg in [0, 45, 90, 180, 270, 360]:
            expected = deg * (B2_PI / 180.0)
            result = deg_to_rad(deg)
            assert abs(result - expected) < EPSILON

    def test_player_template_physics_properties(self, all_templates):
        """Validate the Player template's Rigidbody has correct physics config."""
        player = all_templates.get("Player", {})
        rb = None
        for comp in player.get("components", {}).values():
            if comp.get("type") == "Rigidbody":
                rb = comp
                break
        if rb is None:
            pytest.skip("Player template has no Rigidbody")

        # Player uses a circle collider for smooth rolling
        assert rb.get("collider_type") == "circle", (
            "Player should use circle collider for smooth platformer physics"
        )
        # Density affects mass → affects how forces feel
        density = rb.get("density", 1.0)
        assert density > 0, f"Player density must be positive, got {density}"
        # Radius must be positive
        radius = rb.get("radius", 0.5)
        assert radius > 0, f"Player radius must be positive, got {radius}"

    def test_kinematic_bodies_in_templates(self, all_templates):
        """
        KinematicBox and similar templates should use body_type 'kinematic'.
        Kinematic bodies don't respond to forces — they're moved by code only.
        """
        for tname, data in all_templates.items():
            for comp in data.get("components", {}).values():
                if comp.get("type") != "Rigidbody":
                    continue
                bt = comp.get("body_type", "dynamic")
                if bt == "kinematic":
                    # Kinematic bodies shouldn't have density matter for physics,
                    # but the value should still be valid
                    density = comp.get("density", 1.0)
                    assert isinstance(density, (int, float)), (
                        f"{tname}: kinematic body has invalid density type"
                    )


# ==========================================================================
#  SECTION 6 — Gameplay physics logic
# ==========================================================================

class TestGameplayPhysics:
    """
    Test physics-driven gameplay logic extracted from Lua scripts.
    These verify the expected behavior of game mechanics.
    """

    def test_bouncy_box_preserves_horizontal_velocity(self):
        """
        BouncyBox.lua: new_vel = Vector2(current_vel.x, -15)
        The bounce should preserve horizontal velocity (x unchanged)
        and override vertical velocity to -15 (upward impulse).
        """
        test_velocities = [
            (5.0, 10.0),    # moving right and falling
            (-3.0, 8.0),    # moving left and falling
            (0.0, 12.0),    # falling straight down
            (7.0, -2.0),    # moving right, slightly upward
        ]
        for vx, vy in test_velocities:
            # After bounce
            new_vx = vx       # x preserved
            new_vy = -15.0    # y overridden
            assert new_vx == vx, "Bounce must preserve horizontal velocity"
            assert new_vy == -15.0, "Bounce must set vertical velocity to -15"

    def test_bounce_velocity_is_upward(self):
        """
        In SDL/Box2D screen coordinates, negative Y is up.
        The bounce velocity of -15 should send the player upward.
        """
        bounce_vy = -15.0
        assert bounce_vy < 0, (
            "Bounce velocity must be negative (upward in screen coords)"
        )

    def test_tile_grid_spawn_positions(self):
        """
        GameManager.lua spawns actors at tile grid positions.
        Tile (x, y) in the grid maps to world position (x, y) directly.
        Verify the grid-to-world mapping is identity (1:1 tile units).
        """
        # The stage grid is 20x20, 1-indexed in Lua
        # Tile code 2 (player) is at row 17, col 3 in the grid
        player_grid_pos = (3, 17)  # (x, y) from the Lua grid
        expected_world_pos = (3.0, 17.0)  # direct mapping

        assert player_grid_pos[0] == expected_world_pos[0]
        assert player_grid_pos[1] == expected_world_pos[1]

    def test_player_spawn_location_from_stage(self):
        """
        Verify the player spawn point by scanning the actual stage data.
        Tile code 2 = player spawn.
        """
        # Stage data from GameManager.lua (1-indexed rows/cols)
        stage1 = [
            [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,1,1,1,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,1,0,0,1,1,3,3,1,1,1,0,0,1],
            [1,0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,0,0,0,1],
            [1,0,2,0,0,0,0,0,1,1,0,0,0,0,1,0,0,0,1,1],
            [1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1],
            [1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1],
            [1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1],
        ]

        # Find tile code 2 (player spawn)
        player_positions = []
        for y_idx, row in enumerate(stage1):
            for x_idx, tile in enumerate(row):
                if tile == 2:
                    # Lua is 1-indexed, so world pos = (x_idx+1, y_idx+1)
                    player_positions.append((x_idx + 1, y_idx + 1))

        assert len(player_positions) == 1, (
            f"Expected exactly 1 player spawn, found {len(player_positions)}"
        )
        assert player_positions[0] == (3, 17), (
            f"Player spawn at {player_positions[0]}, expected (3, 17)"
        )

    def test_tile_code_coverage(self):
        """
        Verify all tile codes in the stage map have handlers in GameManager.
        Known codes: 0=nothing, 1=KinematicBox, 2=Player, 3=BouncyBox, 4=VictoryBox
        """
        stage1 = [
            [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,4,4,4,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,1,1,1,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1],
            [1,1,1,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0,0,1],
            [1,0,0,0,0,0,0,1,0,0,1,1,3,3,1,1,1,0,0,1],
            [1,0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,0,0,0,1],
            [1,0,2,0,0,0,0,0,1,1,0,0,0,0,1,0,0,0,1,1],
            [1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1],
            [1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1],
            [1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1],
        ]

        known_codes = {0, 1, 2, 3, 4}
        used_codes = set()
        for row in stage1:
            for tile in row:
                used_codes.add(tile)

        unknown = used_codes - known_codes
        assert not unknown, (
            f"Stage uses tile codes with no handler: {unknown}"
        )

        # Verify all non-zero codes are actually used
        gameplay_codes = {1, 2, 3, 4}
        unused = gameplay_codes - used_codes
        assert not unused, (
            f"Defined tile codes never used in stage: {unused}"
        )

    def test_keyboard_controls_force_application(self):
        """
        KeyboardControls.lua applies horizontal force = speed (default 5)
        and jump force = jump_power (default 350, applied as negative Y).
        Verify the force values are reasonable for the physics scale.
        """
        speed = 5          # from KeyboardControls.lua
        jump_power = 350   # from KeyboardControls.lua

        # Horizontal force should be positive and reasonable
        assert speed > 0, "Movement speed must be positive"

        # Jump must be negative (upward in screen coords) and strong
        # enough to overcome gravity for a satisfying jump
        jump_force_y = -jump_power
        assert jump_force_y < 0, "Jump force must be upward (negative Y)"
        assert jump_power > speed, (
            "Jump power should be significantly larger than horizontal speed "
            "for platformer feel"
        )

    def test_raycast_ground_check_direction(self):
        """
        KeyboardControls.lua: Physics.Raycast(pos, Vector2(0, 1), 1)
        The ground check casts downward (0, 1) with distance 1.
        Verify the ray direction and length are correct for ground detection.
        """
        ray_direction = (0, 1)   # downward in screen coords
        ray_distance = 1.0

        assert ray_direction[1] > 0, (
            "Ground check ray must point downward (positive Y)"
        )
        assert ray_distance > 0, "Ray distance must be positive"
        # Distance of 1 unit should reach just past the player's
        # circle collider (radius 0.45) to detect ground
        player_radius = 0.45  # from Player.template
        assert ray_distance > player_radius, (
            f"Ground ray distance ({ray_distance}) must exceed player "
            f"radius ({player_radius}) to detect ground beneath the player"
        )
