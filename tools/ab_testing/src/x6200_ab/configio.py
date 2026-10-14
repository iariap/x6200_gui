"""Small shared helpers for reading the framework's YAML configuration."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import yaml

from .errors import ConfigError


def read_yaml_mapping(path: Path) -> dict[str, Any]:
    """Read a YAML file that must hold a mapping at the top level."""
    if not path.is_file():
        raise ConfigError(f"missing configuration file: {path}")
    loaded = yaml.safe_load(path.read_text())
    if not isinstance(loaded, dict):
        raise ConfigError(f"{path}: top level must be a mapping")
    return loaded


def as_int(mapping: dict[str, Any], key: str, where: Path | str) -> int:
    try:
        return int(mapping[key])
    except KeyError as exc:
        raise ConfigError(f"{where}: missing `{key}`") from exc
    except (TypeError, ValueError) as exc:
        raise ConfigError(f"{where}: `{key}` must be an integer") from exc


def as_float(mapping: dict[str, Any], key: str, where: Path | str) -> float:
    try:
        return float(mapping[key])
    except KeyError as exc:
        raise ConfigError(f"{where}: missing `{key}`") from exc
    except (TypeError, ValueError) as exc:
        raise ConfigError(f"{where}: `{key}` must be a number") from exc
