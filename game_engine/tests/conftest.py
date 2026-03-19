"""
conftest.py — Shared fixtures for youMadeItEngine test suite.

Provides path discovery and parsed data structures used across all test modules.
All paths are resolved relative to the project root (one level above /tests).
"""

import json
import os
import re
import pytest
from pathlib import Path


# ---------------------------------------------------------------------------
# Path discovery
# ---------------------------------------------------------------------------

def _find_project_root() -> Path:
    """
    Locate the project root by searching for resources/game.config
    or game.config. Checks multiple strategies to work on both local
    machines and CI runners (GitHub Actions, Jenkins).
    """
    candidates = [
        # 1. Standard: conftest.py is in tests/, project root is one up
        Path(__file__).resolve().parent.parent,
        # 2. CI: pytest may run from the working directory directly
        Path.cwd(),
    ]
    # 3. Walk upward from conftest.py (handles nested repo structures)
    walker = Path(__file__).resolve().parent
    for _ in range(5):
        walker = walker.parent
        candidates.append(walker)

    for candidate in candidates:
        if (candidate / "resources" / "game.config").exists():
            return candidate
        if (candidate / "game.config").exists():
            return candidate

    raise FileNotFoundError(
        "Could not locate project root. "
        "Ensure resources/game.config exists in the project directory."
    )


@pytest.fixture(scope="session")
def project_root() -> Path:
    return _find_project_root()


@pytest.fixture(scope="session")
def resources_dir(project_root) -> Path:
    """Return the resources/ directory, or project root if flat layout."""
    res = project_root / "resources"
    return res if res.is_dir() else project_root


# ---------------------------------------------------------------------------
# Config file fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def game_config_path(resources_dir) -> Path:
    return resources_dir / "game.config"


@pytest.fixture(scope="session")
def game_config(game_config_path) -> dict:
    """Parsed game.config as a dict. Fails fast if missing or invalid JSON."""
    with open(game_config_path, "r") as f:
        return json.load(f)


@pytest.fixture(scope="session")
def rendering_config_path(resources_dir) -> Path:
    return resources_dir / "rendering.config"


@pytest.fixture(scope="session")
def rendering_config(rendering_config_path) -> dict | None:
    """Parsed rendering.config, or None if file does not exist."""
    if not rendering_config_path.exists():
        return None
    with open(rendering_config_path, "r") as f:
        return json.load(f)


# ---------------------------------------------------------------------------
# Directory fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def templates_dir(resources_dir) -> Path:
    return resources_dir / "actor_templates"


@pytest.fixture(scope="session")
def scenes_dir(resources_dir) -> Path:
    return resources_dir / "scenes"


@pytest.fixture(scope="session")
def components_dir(resources_dir) -> Path:
    return resources_dir / "component_types"


# ---------------------------------------------------------------------------
# Aggregated data fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def all_templates(templates_dir) -> dict[str, dict]:
    """Map of template_name -> parsed JSON for every .template file."""
    templates = {}
    if not templates_dir.is_dir():
        return templates
    for f in templates_dir.glob("*.template"):
        with open(f, "r") as fp:
            templates[f.stem] = json.load(fp)
    return templates


@pytest.fixture(scope="session")
def all_scenes(scenes_dir) -> dict[str, dict]:
    """Map of scene_name -> parsed JSON for every .scene file."""
    scenes = {}
    if not scenes_dir.is_dir():
        return scenes
    for f in scenes_dir.glob("*.scene"):
        with open(f, "r") as fp:
            scenes[f.stem] = json.load(fp)
    return scenes


@pytest.fixture(scope="session")
def all_lua_scripts(components_dir) -> dict[str, str]:
    """Map of script_name -> raw file contents for every .lua file."""
    scripts = {}
    if not components_dir.is_dir():
        return scripts
    for f in components_dir.glob("*.lua"):
        scripts[f.stem] = f.read_text()
    return scripts


# ---------------------------------------------------------------------------
# Engine-known built-in component types (implemented in C++, not Lua)
# ---------------------------------------------------------------------------

BUILTIN_COMPONENT_TYPES = {"Rigidbody", "ParticleSystem"}


@pytest.fixture(scope="session")
def builtin_types() -> set[str]:
    return BUILTIN_COMPONENT_TYPES


# ---------------------------------------------------------------------------
# Helper: extract all component types referenced across templates and scenes
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def referenced_component_types(all_templates, all_scenes) -> set[str]:
    """Every unique component 'type' string found in templates and scenes."""
    types = set()
    for data in all_templates.values():
        for comp in data.get("components", {}).values():
            if "type" in comp:
                types.add(comp["type"])
    for scene_data in all_scenes.values():
        for actor in scene_data.get("actors", []):
            for comp in actor.get("components", {}).values():
                if "type" in comp:
                    types.add(comp["type"])
    return types


@pytest.fixture(scope="session")
def referenced_template_names(all_scenes, all_lua_scripts) -> set[str]:
    """Template names referenced via Actor.Instantiate in Lua scripts."""
    names = set()
    pattern = re.compile(r'Actor\.Instantiate\(\s*"([^"]+)"\s*\)')
    for content in all_lua_scripts.values():
        names.update(pattern.findall(content))
    return names
