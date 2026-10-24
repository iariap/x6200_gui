"""Pytest fixtures shared by the unit tests."""

from __future__ import annotations

from dataclasses import dataclass

import pytest

from helpers import FIXTURE_NAMES, QUALITY_DIR
from x6200_ab.known_issues import KnownIssue, load_known_issues
from x6200_ab.quality_config import (
    Budgets,
    PerformanceExceptions,
    QualityContracts,
    load_budgets,
    load_contracts,
    load_performance_exceptions,
)
from x6200_ab.regions import RegionMap, load_regions


@dataclass(frozen=True)
class RealQuality:
    """The repository's real quality files, loaded once for the whole session."""

    regions: RegionMap
    contracts: QualityContracts
    budgets: Budgets
    known_issues: tuple[KnownIssue, ...]
    performance_exceptions: PerformanceExceptions


@pytest.fixture(scope="session")
def region_map() -> RegionMap:
    return load_regions(QUALITY_DIR / "regions.yaml")


@pytest.fixture(scope="session")
def real_quality(region_map: RegionMap) -> RealQuality:
    names = set(region_map.regions)
    return RealQuality(
        regions=region_map,
        contracts=load_contracts(QUALITY_DIR / "contracts.yaml", names, set(FIXTURE_NAMES)),
        budgets=load_budgets(QUALITY_DIR / "budgets.yaml"),
        known_issues=load_known_issues(QUALITY_DIR / "known_issues.yaml", names),
        performance_exceptions=load_performance_exceptions(
            QUALITY_DIR / "performance_exceptions.yaml"
        ),
    )
