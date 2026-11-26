"""Resolve project and resource paths independently of the working directory."""

from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
RESOURCE_ROOT = PROJECT_ROOT / "resources"
MAPS_DIR = RESOURCE_ROOT / "maps"
TILESETS_DIR = RESOURCE_ROOT / "tilesets"
UNITS_DIR = RESOURCE_ROOT / "units"
DATASET_DIR = PROJECT_ROOT / "dataset"
MODELS_DIR = PROJECT_ROOT / "models"
SERVER_CONFIG_FILE = PROJECT_ROOT / "config" / "server.ini"
TRAIN_CONFIG_FILE = PROJECT_ROOT / "config" / "train.toml"


def project_path(path: str | Path) -> Path:
    """Resolve a relative path against the project root."""
    return PROJECT_ROOT / Path(path).expanduser()


def resource_path(path: str | Path) -> Path:
    """Resolve a resource name or a project-relative resources path."""
    path = Path(path).expanduser()
    if path.is_absolute() or path.parts[:1] == ("resources",):
        return project_path(path)
    return RESOURCE_ROOT / path


def resolve_map_path(path: str | Path) -> Path:
    """Resolve project-relative or native game map names to resource files."""
    path = Path(path).expanduser()
    if path.is_absolute() or path.parts[:1] == ("resources",):
        return project_path(path)
    if path.parts[:1] == ("maps",):
        return RESOURCE_ROOT / path
    return MAPS_DIR / path
