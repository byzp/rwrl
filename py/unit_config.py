"""Host-side loader for Rusted Warfare unit assets.

The game data uses the engine's ``key: value`` INI dialect rather than the
Python ``configparser`` dialect.  This module parses those files in Python and
passes plain ``UnitDef``/``WeaponDef`` objects through pybind.  The C++ core
does not open files or depend on an operating-system path API.
"""

from __future__ import annotations

import ast
import math
import re
import struct
from pathlib import Path
from typing import Any

import rwrl_core
from resource_paths import UNITS_DIR, project_path


DEFAULT_ASSET_ROOT = UNITS_DIR

# The replay protocol keeps this shipped replacement's stock production name.
CANONICAL_ASSET_NAMES = {"extractorT1": "extractor"}


def _read_ini(path: Path) -> dict[str, dict[str, str]]:
    sections: dict[str, dict[str, str]] = {}
    current: dict[str, str] | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = sections.setdefault(line[1:-1].strip(), {})
            continue
        if current is None or ":" not in line:
            continue
        key, value = line.split(":", 1)
        current[key.strip()] = value.strip()
    return sections


def _inherited_core(path: Path, sections: dict[str, dict[str, str]],
                    seen: set[Path] | None = None) -> dict[str, str]:
    """Return the ``[core]`` section with its ``copyFrom`` chain filled in.

    Only the keys this loader reads from the merged result are applied; the
    rest of the asset layer still uses the file's own section, so adding a
    parent file cannot silently change a value that is already verified.
    """

    core = dict(sections.get("core", {}))
    parent = core.get("copyFrom", "").strip()
    if not parent.endswith(".ini"):
        return core
    seen = seen or {path.resolve()}
    candidate = path.parent / parent
    if not candidate.is_file():
        matches = sorted(DEFAULT_ASSET_ROOT.rglob(parent))
        if not matches:
            return core
        candidate = matches[0]
    if candidate.resolve() in seen:
        return core
    seen.add(candidate.resolve())
    inherited = _inherited_core(candidate, _read_ini(candidate), seen)
    for key, value in inherited.items():
        core.setdefault(key, value)
    return core


def _inherited_sections(path: Path, sections: dict[str, dict[str, str]],
                        seen: set[Path] | None = None) -> dict[str, dict[str, str]]:
    """Merge every INI section in the unit's ``core.copyFrom`` chain.

    CustomUnitMetadata applies copyFrom before parsing attack, movement, and
    turret sections.  Keeping that resolution on the host side lets the core
    remain independent of the asset files while preserving inherited turret
    defaults such as ``shouldResetTurret``.
    """
    parent_name = sections.get("core", {}).get("copyFrom", "").strip()
    # utility.ab's LinkedHashMap retains the child's section order, then
    # appends sections inherited from copyFrom. Turret indices use that order.
    merged = {section: dict(values) for section, values in sections.items()}
    if parent_name.endswith(".ini"):
        seen = seen or {path.resolve()}
        candidate = path.parent / parent_name
        if not candidate.is_file():
            matches = sorted(DEFAULT_ASSET_ROOT.rglob(parent_name))
            candidate = matches[0] if matches else None
        if candidate is not None and candidate.resolve() not in seen:
            seen.add(candidate.resolve())
            inherited = _inherited_sections(
                candidate, _read_ini(candidate), seen)
            for section, values in inherited.items():
                destination = merged.setdefault(section, {})
                # utility.ab.a(ab) suppresses inherited keys in this section;
                # values explicitly written by the child still take effect.
                if _boolean(destination.get("@copyFrom_skipThisSection", "false")):
                    continue
                for key, value in values.items():
                    destination.setdefault(key, value)
    return merged


def _apply_footprint_offsets(definition: Any, core: dict[str, str]) -> None:
    # custom.j.cZ()/da() return custom.l.da/db. Both default to the stock
    # am.cZ()/da() half tile, and y.br() subtracts them from the object
    # position before it grows the placement rectangle.
    if "footprint" in core:
        definition.collision_footprint = _tile_rect(
            core["footprint"], definition.collision_footprint)
    if "constructionFootprint" in core:
        definition.placement_footprint = _tile_rect(
            core["constructionFootprint"], definition.placement_footprint)
    if "buildingToFootprintOffsetX" in core:
        definition.building_to_footprint_offset_x = _number(
            core["buildingToFootprintOffsetX"],
            definition.building_to_footprint_offset_x)
    if "buildingToFootprintOffsetY" in core:
        definition.building_to_footprint_offset_y = _number(
            core["buildingToFootprintOffsetY"],
            definition.building_to_footprint_offset_y)


def _apply_factory_exit(definition: Any, core: dict[str, str]) -> None:
    """Apply custom.j's optional factory output metadata."""
    exit_keys = {
        "exit_x", "exit_y", "exit_moveAwayAmount", "exit_dirOffset",
        "exit_heightOffset", "exitHeightIgnoreParent",
    }
    if not any(key in core for key in exit_keys):
        return
    definition.has_factory_exit_config = True
    if "exit_x" in core:
        definition.factory_exit_x = _number(
            core["exit_x"], definition.factory_exit_x)
    if "exit_y" in core:
        definition.factory_exit_y = _number(
            core["exit_y"], definition.factory_exit_y)
    if "exit_moveAwayAmount" in core:
        definition.factory_exit_move_away_amount = _number(
            core["exit_moveAwayAmount"],
            definition.factory_exit_move_away_amount)
    if "exit_dirOffset" in core:
        definition.factory_exit_has_dir_offset = True
        definition.factory_exit_dir_offset = _number(
            core["exit_dirOffset"], definition.factory_exit_dir_offset)
    if "exit_heightOffset" in core:
        definition.factory_exit_height_offset = _number(
            core["exit_heightOffset"], definition.factory_exit_height_offset)
    if "exitHeightIgnoreParent" in core:
        definition.factory_exit_height_ignore_parent = _boolean(
            core["exitHeightIgnoreParent"],
            definition.factory_exit_height_ignore_parent)


def _tile_rect(value: str, default: Any) -> Any:
    parts = [part.strip() for part in value.split(",")]
    if len(parts) != 4:
        return default
    rect = rwrl_core.TileRect()
    rect.left = _integer(parts[0], default.left)
    rect.top = _integer(parts[1], default.top)
    rect.right = _integer(parts[2], default.right)
    rect.bottom = _integer(parts[3], default.bottom)
    return rect


def _number(value: str, default: float = 0.0) -> float:
    value = value.strip()
    if not value:
        return default
    # Asset values such as ``35s`` and ``credits=250`` are common in custom
    # actions.  Unit core values only need the first numeric component here.
    match = re.match(r"^[^0-9+\-.]*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)", value)
    return float(match.group(1)) if match else default


def _integer(value: str, default: int = 0) -> int:
    return int(_number(value, default))


def _float32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def _time_frames(value: str, default: float = 0.0) -> float:
    """Match utility.ab's frame duration, including the seconds suffix."""
    value = value.strip()
    if not value:
        return default
    seconds = value.endswith("s")
    amount = _float32(float(value[:-1] if seconds else value))
    return _float32(amount * 60.0) if seconds else amount


def _build_speed(value: str, default: float = 0.01, *, zero_is_instant: bool = True) -> float:
    """Match utility.ab.d; custom actions additionally turn a bare zero into 50."""
    value = value.strip()
    if not value:
        return default
    seconds = value.endswith("s")
    amount = _float32(float(value[:-1] if seconds else value))
    if seconds:
        frames = _float32(amount * 60.0)
        amount = _float32(1.0 / frames) if frames else float("inf")
    return 50.0 if zero_is_instant and amount == 0.0 else amount


def _numeric_template(value: str, sections: dict[str, dict[str, str]]) -> str:
    """Resolve static INI arithmetic before the native float parser rounds it."""
    template = re.fullmatch(r"\s*\$\{(.*?)\}(s?)\s*", value, re.DOTALL)
    if template is None:
        return value

    def number(node: ast.AST, references: frozenset[tuple[str, str]]) -> float:
        if isinstance(node, ast.Constant) and type(node.value) in (int, float):
            return float(node.value)
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
            operand = number(node.operand, references)
            return -operand if isinstance(node.op, ast.USub) else operand
        if isinstance(node, ast.BinOp):
            left, right = number(node.left, references), number(node.right, references)
            if isinstance(node.op, ast.Add): return left + right
            if isinstance(node.op, ast.Sub): return left - right
            if isinstance(node.op, ast.Mult): return left * right
            if isinstance(node.op, ast.Div) and right != 0: return left / right
        if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name):
            key = node.value.id, node.attr
            if key in references or len(references) >= 10:
                raise ValueError('Recursive numeric INI reference')
            raw = sections.get(key[0], {}).get(key[1])
            if raw is not None:
                nested = re.fullmatch(r"\s*\$\{(.*?)\}\s*", raw, re.DOTALL)
                expression = nested[1] if nested else raw
                return number(ast.parse(expression.strip(), mode='eval').body, references | {key})
        raise ValueError('Unsupported static numeric INI expression')

    try:
        result = number(ast.parse(template[1].strip(), mode='eval').body, frozenset())
    except (SyntaxError, OverflowError) as error:
        raise ValueError('Invalid static numeric INI expression') from error
    return str(result) + template[2]


def _credit_bundle(value: str) -> int:
    """Read a literal credit-only resource bundle, including a bare price."""
    value = value.strip()
    if not value:
        return 0
    if value.startswith("credits="):
        value = value[len("credits="):].strip()
    return int(value)


def _boolean(value: str, default: bool = False) -> bool:
    lowered = value.strip().lower()
    if lowered in {"true", "yes", "on", "1"}:
        return True
    if lowered in {"false", "no", "off", "0"}:
        return False
    return default


def _enum(enum_type: Any, value: str, default: Any) -> Any:
    try:
        return getattr(enum_type, value.strip().upper())
    except AttributeError:
        return default


def _first_name(core: dict[str, str], prefix: str) -> str:
    entries = []
    for key, value in core.items():
        if key.startswith(prefix) and key.endswith("_name"):
            entries.append((key, value.strip()))
    for _, value in sorted(entries):
        if value and value.upper() not in {"IGNORE", "NONE"}:
            return value
    return ""


def _section_number(section: str, prefix: str) -> int:
    suffix = section[len(prefix):]
    match = re.match(r"(\d+)", suffix)
    return int(match.group(1)) if match else 0


def _apply_core(definition: Any, core: dict[str, str]) -> None:
    definition.is_builder = _boolean(core.get("isBuilder", "false"))
    # custom.ag initializes these independently of a replaced stock type.
    # custom.j.H() selects the building/unit gate; resource reclaim has its
    # own fk flag and does not require either repair permission.
    buildings = _boolean(core.get("canRepairBuildings", "false"))
    units = _boolean(core.get("canRepairUnits", "false"))
    resources = _boolean(core.get("canReclaimResources", "false"))
    definition.can_repair = buildings or units
    definition.can_repair_any = units
    definition.auto_repair = _boolean(core.get("autoRepair", "false"))
    definition.nano_repair_speed = _float32(_number(core.get("nanoRepairSpeed", "0.2"), 0.2))
    definition.nano_reclaim_speed = _float32(_number(core.get("nanoReclaimSpeed", ""),
        _float32(definition.nano_repair_speed * _float32(5.1))))
    definition.resource_reclaim_multiplier = _float32(_number(core.get("resourceReclaimMultiplier", "1"), 1))
    definition.nano_unbuild_speed = _float32(_float32(
        _float32(_number(core.get("nanoUnbuildSpeed", "1"), 1)) * _float32(.001)) * _float32(5.1))
    definition.can_repair_buildings = buildings
    definition.can_reclaim = buildings or units or resources
    definition.can_reclaim_resources = resources
    definition.can_be_repaired = not _boolean(core.get("isUnrepairableUnit", "false"))
    definition.nano_range = _integer(core.get("nanoRange", "-1"), -1)
    definition.nano_build_speed = _float32(_number(core.get("nanoBuildSpeed", "1"), 1))
    definition.nano_factory_speed = _float32(_number(core.get("nanoFactorySpeed", "1"), 1))
    if "isBuilding" in core:
        definition.is_building = _boolean(
            core["isBuilding"], definition.is_building)
    if "placeOnlyOnResPool" in core:
        definition.place_only_on_res_pool = _boolean(
            core["placeOnlyOnResPool"], definition.place_only_on_res_pool)
    if "extraBuildRangeWhenBuildingThis" in core:
        definition.extra_build_range_when_building_this = _number(
            core["extraBuildRangeWhenBuildingThis"],
            definition.extra_build_range_when_building_this)
    if "price" in core:
        definition.cost = _integer(core["price"], definition.cost)
    if "maxHp" in core:
        definition.hp = _number(core["maxHp"], definition.hp)
    if "selfRegenRate" in core:
        definition.self_regen_rate = _number(
            core["selfRegenRate"], definition.self_regen_rate)
    if "generation_resources" in core or "generation_credits" in core:
        interval = max(1, _integer(core.get("generation_delay", "40"), 40))
        credits = _credit_bundle(core.get("generation_resources", ""))
        credits += _integer(core.get("generation_credits", "0"))
        definition.income_interval = float(interval)
        definition.income = _float32(credits * _float32(40.0 / interval))
    # custom.ag parses maxShield as an integer defaulting to 0 into as.g and
    # shieldRegen as a float defaulting to 0.25 into as.h. custom.j adds
    # as.h * deltaFrames each update. Creation normally fills the shield;
    # startShieldAtZero retains the constructor's zero points instead.
    if "maxShield" in core:
        definition.shield = float(_integer(core["maxShield"], 0))
    if "startShieldAtZero" in core:
        definition.start_shield_at_zero = _boolean(
            core["startShieldAtZero"], False)
    definition.shield_regen_per_frame = _float32(
        _number(core.get("shieldRegen", "0.25"), 0.25))
    # custom.ag creates an independent energy pool, even when the asset
    # replaces a stock unit. Keep the parsed per-frame float rates intact.
    definition.max_energy = _float32(_number(core.get("energyMax", "0")))
    definition.start_energy_at_zero = _boolean(core.get("startEnergyAtZero", "false"))
    starting_fraction = _float32(_number(core.get("energyStartingPercentage", "1"), 1))
    definition.initial_energy = (0.0 if definition.start_energy_at_zero else
                                 _float32(definition.max_energy * starting_fraction))
    definition.energy_regen_per_frame = _float32(_number(core.get("energyRegen", "0")))
    definition.energy_recharge_regen_per_frame = _float32(_number(
        core.get("energyRegenWhenRecharging", ""), definition.energy_regen_per_frame))
    definition.energy_regen_rate = _float32(
        definition.energy_regen_per_frame * rwrl_core.FRAME_RATE)
    definition.energy_needs_full_recharge = _boolean(
        core.get("energyNeedsToRechargeToFull", "false"))
    definition.die_on_zero_energy = _boolean(core.get("dieOnZeroEnergy", "false"))
    definition.nuke_on_death = _boolean(core.get("nukeOnDeath", "false"))
    definition.nuke_on_death_range = _float32(_number(core.get("nukeOnDeathRange", "250"), 250))
    definition.nuke_on_death_damage = _float32(_number(core.get("nukeOnDeathDamage", "5400"), 5400))
    definition.nuke_on_death_disable_when_no_nuke = _boolean(
        core.get("nukeOnDeathDisableWhenNoNuke", "false"))
    if "mass" in core:
        definition.mass = _number(core["mass"], definition.mass)
    if "buildSpeed" in core:
        definition.build_speed = _build_speed(
            core["buildSpeed"], definition.build_speed, zero_is_instant=False)
    if "radius" in core:
        definition.radius = _number(core["radius"], definition.radius)
    if "softCollisionOnAll" in core:
        definition.soft_collision_on_all = _integer(
            core["softCollisionOnAll"], definition.soft_collision_on_all)
    if "explodeOnDeathGroundCollision" in core:
        definition.explode_on_death_ground_collision = _boolean(
            core["explodeOnDeathGroundCollision"],
            definition.explode_on_death_ground_collision)
    elif "explodeOnDeathGroundCollosion" in core:
        definition.explode_on_death_ground_collision = _boolean(
            core["explodeOnDeathGroundCollosion"],
            definition.explode_on_death_ground_collision)
    if "hideScorchMark" in core:
        definition.hide_scorch_mark = _boolean(
            core["hideScorchMark"], definition.hide_scorch_mark)
    if core.get("effectOnDeath", "").strip().lower() == "resourcepoolsmoke":
        # custom.ay.resourcePoolSmoke creates two gameFramework.d.f objects.
        # Effects are visual-only here, but their shared object IDs are part
        # of lockstep allocation and must be supplied to the core explicitly.
        definition.death_effect_uid_count = 2
    explosion_type = core.get("explodeTypeOnDeath", "").strip().lower()
    definition.default_ground_death_effect = (
        _boolean(core.get("explodeOnDeath", "true"), True)
        and (not _boolean(core.get("isBio", "false"), False) or bool(explosion_type))
        and explosion_type not in {"verysmall", "buildingnoshockwaveorsmoke"}
    )
    if "techLevel" in core:
        definition.tech_level = _integer(core["techLevel"], definition.tech_level)
    replacement = core.get("overrideAndReplace", "").strip()
    definition.override_and_replace = (
        replacement if replacement.upper() != "NONE" else "")
    built_from = _first_name(core, "builtFrom_")
    if built_from:
        definition.built_from = built_from
    if hasattr(definition, "built_from_factories"):
        definition.built_from_factories = list(dict.fromkeys(
            name.strip()
            for key, value in core.items()
            if key.startswith("builtFrom_") and key.endswith("_name")
            for name in value.split(",")
            if name.strip() and name.strip().upper() not in {"NONE", "IGNORE"}
        ))


def _apply_legs(definition: Any, sections: dict[str, dict[str, str]]) -> None:
    """Resolve ba metadata in numbered leg order followed by numbered arms."""
    numeric = {
        "x": "x", "y": "y", "liftingHeightOffset": "lifting_height",
        "targetHeight": "target_height", "endDirOffset": "end_direction",
        "estimatingPositionMultiplier": "prediction_multiplier",
        "moveSpeed": "move_speed", "rotateSpeed": "rotate_speed",
        "heightSpeed": "height_speed", "holdDisMin": "hold_distance_min",
        "holdDisMax": "hold_distance_max", "hardLimit": "hard_limit",
    }
    booleans = {
        "targetHeightRelative": "height_relative",
        "lockMovement": "lock_movement",
        "holdDisMin_checkNeighbours": "check_neighbours",
        "favourOppositeSideNeighbours": "favour_opposite_side",
        "hidden": "hidden", "explodeOnDeath": "explode_on_death",
        "hold_moveOnlyIfFurthest": "move_only_if_furthest",
    }
    copy_fields = [*numeric.values(), *booleans.values(),
                   "move_warmup_frames", "max_moving_legs"]
    legs = []
    for prefix in ("leg_", "arm_"):
        numbered = {}
        for number in range(1, 21):
            values = sections.get(f"{prefix}{number}")
            if values is None:
                continue
            leg = rwrl_core.LegDef()
            if prefix == "arm_":
                leg.lock_movement = True
                leg.explode_on_death = False
            parent = _integer(values.get("copyFrom", "0"))
            if parent:
                inherited = numbered.get(parent)
                if inherited is None:
                    raise ValueError(f"{prefix}{number}: copyFrom {parent} is not loaded")
                for field in copy_fields:
                    setattr(leg, field, getattr(inherited, field))
            for key, field in numeric.items():
                if key in values:
                    setattr(leg, field, _number(values[key], getattr(leg, field)))
            # ba requires x/y in each section and resets endDirOffset/hidden
            # when these keys are absent from a local copyFrom section.
            leg.x = _number(values.get("x", "0"))
            leg.y = _number(values.get("y", "0"))
            leg.end_direction = _number(values.get("endDirOffset", "0"))
            leg.hidden = _boolean(values.get("hidden", "false"))
            for key, field in booleans.items():
                if key in values:
                    setattr(leg, field, _boolean(values[key], getattr(leg, field)))
            if "moveWarmUp" in values:
                leg.move_warmup_frames = _time_frames(values["moveWarmUp"])
            if "holdDisMin_maxMovingLegs" in values:
                leg.max_moving_legs = _integer(values["holdDisMin_maxMovingLegs"])
            numbered[number] = leg
            legs.append(leg)

    def neighbour_distance(first: Any, second: Any, scale: float) -> float:
        dx = _float32(_float32(first.x * scale) - _float32(second.x * scale))
        dy = _float32(first.y - second.y)
        return _float32(_float32(dx * dx) + _float32(dy * dy))

    for index, leg in enumerate(legs):
        scale = _float32(0.1) if leg.favour_opposite_side else 1.0
        distances = {
            other_index: neighbour_distance(leg, other, scale)
            for other_index, other in enumerate(legs)
            if other_index != index and not other.lock_movement}
        if distances:
            limit = _float32(_float32(math.sqrt(min(distances.values()))) + 2.0)
            limit_sq = _float32(limit * limit)
            leg.neighbours = [other_index for other_index, distance in distances.items()
                              if distance <= limit_sq]
    definition.legs = legs
    definition.leg_death_scorch = not _boolean(
        sections.get("core", {}).get("isBio", "false"))
    definition.leg_rotation_slot = -1
    if _boolean(sections.get("graphics", {}).get(
            "lock_leg_rotation_with_main_turret", "false")):
        main = sections.get("attack", {}).get("setMainTurretAs", "1").strip()
        definition.leg_rotation_slot = next(
            (index for index, weapon in enumerate(definition.weapons)
             if weapon.turret_name == main), 0 if definition.weapons else -1)


def _apply_attack(definition: Any, attack: dict[str, str]) -> None:
    definition.bomber_attack_movement = (
        attack.get("attackMovement", "normal").strip().lower() == "bomber")
    definition.disable_passive_targeting = _boolean(
        attack.get("disablePassiveTargeting", "false"), False)
    definition.turret_multi_targeting = _boolean(
        attack.get("turretMultiTargeting", "false"), False)
    # custom.l.ey defaults to false independently of the stock template.
    # y.E() uses it for the stopping radius, not just for turret aiming.
    definition.attack_follow_e_flag = _boolean(attack.get("isFixedFiring", "false"), False)
    if "maxAttackRange" in attack:
        definition.range = _number(attack["maxAttackRange"], definition.range)
    if "shootDelay" in attack:
        definition.cooldown_max = int(_time_frames(
            attack["shootDelay"], definition.cooldown_max))
    if "canAttackFlyingUnits" in attack:
        definition.targets_air = _boolean(
            attack["canAttackFlyingUnits"], definition.targets_air)
    if "canAttackLandUnits" in attack:
        land_condition = attack["canAttackLandUnits"].strip().lower()
        definition.ground_attack_disabled_while_flying = bool(re.fullmatch(
            r"if\s+not\s+self\.(?:flying|isflying)", land_condition))
        definition.targets_ground = (True if definition.ground_attack_disabled_while_flying
            else _boolean(land_condition, definition.targets_ground))
    definition.targets_underwater = _boolean(
        attack.get("canAttackUnderwaterUnits", "false"), False)
    definition.targets_not_touching_water = _boolean(
        attack.get("canAttackNotTouchingWaterUnits", "true"), True)
    if "canAttack" in attack:
        definition.can_attack = _boolean(attack["canAttack"], True)
        if not definition.can_attack:
            definition.weapon_type = rwrl_core.WeaponType.MOVE

def _apply_movement(definition: Any, movement: dict[str, str]) -> None:
    if "joinsGroupFormations" in movement:
        definition.joins_group_formations = _boolean(
            movement["joinsGroupFormations"], True)
    # custom.ag sets dP for every building before reading ignoreMoveOrders;
    # custom.j.aR() uses that flag independently of movementType and speed.
    definition.ignore_move_orders = definition.is_building or _boolean(
        movement.get("ignoreMoveOrders", "false"), False)
    if "movementType" in movement:
        definition.move_type = _enum(
            rwrl_core.MoveType, movement["movementType"], definition.move_type)
        if definition.move_type == rwrl_core.MoveType.AIR:
            # custom.ag's AIR defaults also apply to newly registered assets.
            if "targetHeight" not in movement:
                definition.target_height = 35.0
            if "targetHeightDrift" not in movement:
                definition.target_height_drift = 1.5
    if "moveSpeed" in movement:
        definition.speed = _number(movement["moveSpeed"], definition.speed)
    if "moveAccelerationSpeed" in movement:
        definition.move_acceleration = _number(
            movement["moveAccelerationSpeed"], definition.move_acceleration)
    if "moveDecelerationSpeed" in movement:
        definition.move_deceleration = _number(
            movement["moveDecelerationSpeed"], definition.move_deceleration)
    if "maxTurnSpeed" in movement:
        definition.turn_speed = _number(
            movement["maxTurnSpeed"], definition.turn_speed)
    if "turnAcceleration" in movement:
        definition.turn_acceleration = _number(
            movement["turnAcceleration"], definition.turn_acceleration)
    if "reverseSpeedPercentage" in movement:
        definition.reverse_speed_multiplier = _number(
            movement["reverseSpeedPercentage"], definition.reverse_speed_multiplier)
    if "moveSlidingMode" in movement:
        definition.strafes = _boolean(movement["moveSlidingMode"], definition.strafes)
    if "moveIgnoringBody" in movement:
        definition.move_ignores_body = _boolean(
            movement["moveIgnoringBody"], definition.move_ignores_body)
    if "targetHeight" in movement:
        definition.target_height = _number(
            movement["targetHeight"], definition.target_height)
    if "targetHeightDrift" in movement:
        definition.target_height_drift = _number(
            movement["targetHeightDrift"], definition.target_height_drift)
    if "heightChangeRate" in movement:
        definition.height_change_rate = _number(
            movement["heightChangeRate"], definition.height_change_rate)
    if "fallingAcceleration" in movement:
        definition.falling_acceleration = _number(
            movement["fallingAcceleration"], definition.falling_acceleration)
    if "landOnGround" in movement:
        landing = movement["landOnGround"].strip().lower()
        if landing not in {"false", "true", "onlyidle"}:
            raise ValueError(f"landOnGround expected true, false or onlyIdle: {landing}")
        definition.land_on_ground = landing != "false"
        definition.land_on_ground_only_idle = landing == "onlyidle"
    if "slowDeathFall" in movement:
        definition.slow_death_fall = _boolean(
            movement["slowDeathFall"], definition.slow_death_fall)


def _apply_transport(definition: Any, core: dict[str, str]) -> None:
    definition.cargo_capacity = _integer(core.get("maxTransportingUnits", "0"))
    unload_condition = core.get("transportUnitsCanUnloadUnits",
        "not self.isOverLiquid() and not self.isMoving()").strip().lower()
    if unload_condition.startswith("if "):
        unload_condition = unload_condition[3:].strip()
    unload_condition = " ".join(unload_condition.split())
    if unload_condition not in {
        "true", "false", "self.isoverliquid()", "not self.isoverliquid()",
        "not self.isoverliquid() and not self.ismoving()",
    }:
        raise ValueError(f"Unsupported transportUnitsCanUnloadUnits: {unload_condition}")
    definition.transport_unload_condition = unload_condition
    definition.transport_add_unload_option = _boolean(core.get(
        "transportUnitsAddUnloadOption", "false" if unload_condition == "false" else "true"))
    definition.transport_unload_delay = _number(core.get("transportUnitsUnloadDelayBetweenEachUnit", "30"))
    definition.factory_keep_built_units = _boolean(core.get("transportUnitsKeepBuiltUnits", "false"))
    definition.cargo_weight = _integer(core.get("transportSlotsNeeded", "1"))
    # Stock transports call utility.y.a(unit, true, true): custom units are
    # excluded when they are buildings, carriers, or use AIR/WATER movement.
    # Land, hover and cliff movement retain the ordinary transport permission.
    definition.transportable = (
        not definition.is_building and definition.cargo_capacity == 0
        and definition.move_type not in (rwrl_core.MoveType.AIR, rwrl_core.MoveType.WATER)
    )


def _apply_projectile(weapon: Any, projectile: dict[str, str]) -> None:
    weapon.projectile_tags = list(dict.fromkeys(
        tag.strip().lower() for tag in projectile.get("tags", "").split(",") if tag.strip()))
    weapon.projectile_should_reveal_fog = _boolean(projectile.get("shouldRevealFog", "false"))
    # bh reads these independently of the replaced stock weapon, then j
    # copies them to game.f.ai/ak/al/am before the native damage chain.
    for key, field in (
        ("buildingDamageMultiplier", "building_damage_mult"),
        ("shieldDamageMultiplier", "shield_mult"),
        ("shieldDefectionMultiplier", "shield_absorb"),
        ("hullDamageMultiplier", "hp_mult"),
    ):
        setattr(weapon, field, _float32(_number(projectile.get(key, "1"))))
    if "directDamage" in projectile:
        weapon.damage = _number(projectile["directDamage"], weapon.damage)
    if "areaDamage" in projectile:
        weapon.splash_damage = _number(projectile["areaDamage"], weapon.splash_damage)
    if "areaRadius" in projectile:
        weapon.splash_radius = _number(projectile["areaRadius"], weapon.splash_radius)
    weapon.splash_expand_time = _float32(_number(projectile.get("areaExpandTime", "0")))
    weapon.splash_friendly_fire = _boolean(projectile.get("friendlyFire", "false"))
    # custom.j copies bh.m into f.ad and disables f.ae's height-layer gate.
    weapon.splash_same_layer = not _boolean(
        projectile.get("areaHitAirAndLandAtSameTime", "false"))
    # custom.j copies ballistic/as/at to f.aH/aI/aJ and forces aL=-1:
    # the current projectile speed controls the vertical ballistic step.
    weapon.height_guided = _boolean(projectile.get("ballistic", "false"))
    launch_height = _float32(_number(projectile.get("ballistic_delaymove_height", "-1")))
    cruise_height = _float32(_number(projectile.get("ballistic_height", "-1")))
    weapon.launch_height = 40 if launch_height == -1 else launch_height
    weapon.cruise_height = 60 if cruise_height == -1 else cruise_height
    weapon.vertical_speed = -1
    weapon.nuke_weapon = _boolean(projectile.get("nukeWeapon", "false"))
    # game.g.w defaults to 5 even when the replaced stock weapon has no
    # travelling projectile (for example laserTank).
    weapon.projectile_speed = _float32(
        _float32(_number(projectile.get("speed", "5"))) * 60.0)
    weapon.projectile_speed_spread = _float32(
        _number(projectile.get("speedSpread", "0")))
    weapon.projectile_wobble_amplitude = _float32(
        _number(projectile.get("wobbleAmplitude", "0")))
    weapon.projectile_wobble_frequency_frames = _time_frames(
        projectile.get("wobbleFrequency", "5"))
    if weapon.projectile_wobble_frequency_frames <= 0:
        raise ValueError("wobbleFrequency must be greater than 0")
    weapon.projectile_flame_weapon = _boolean(projectile.get("flameWeapon", "false"))
    if "life" in projectile:
        weapon.projectile_lifetime = _number(
            projectile["life"], weapon.projectile_lifetime)
    # bh.D/E/F become f.w/u/v; g.G/H remain frame-unit gravity settings.
    # Their zero defaults are independent of the replaced stock weapon.
    for key, field in (
        ("initialUnguidedSpeedHeight", "projectile_unguided_speed_height"),
        ("initialUnguidedSpeedX", "projectile_unguided_speed_x"),
        ("initialUnguidedSpeedY", "projectile_unguided_speed_y"),
        ("gravity", "projectile_gravity"),
        ("trueGravity", "projectile_true_gravity"),
    ):
        setattr(weapon, field, _float32(_number(projectile.get(key, "0"))))
    # game.g starts au/av at -1/0.1, independently of the replaced stock
    # weapon. bh reads targetSpeedAcceleration, then flight copies au/av to
    # game.f.r/s. Preserve float parsing before converting frame units.
    target_speed = _float32(_number(projectile.get("targetSpeed", "-1")))
    weapon.projectile_target_speed = (
        _float32(target_speed * 60.0) if target_speed > 0 else target_speed)
    weapon.projectile_acceleration = _float32(
        _float32(_number(projectile.get("targetSpeedAcceleration", "0.1"))) * 60.0)
    # custom.j copies game.g.aw to f.aG; the search reads g.ax/ay directly.
    weapon.projectile_retarget_on_death = _boolean(
        projectile.get("autoTargetingOnDeadTarget", "false"))
    weapon.projectile_retarget_range = _float32(_number(
        projectile.get("autoTargetingOnDeadTargetRange", "120")))
    weapon.projectile_retarget_lead = _float32(_number(
        projectile.get("autoTargetingOnDeadTargetLead", "15")))
    weapon.projectile_deflection_power = _float32(_number(
        projectile.get("deflectionPower", "1")))
    if "targetGround" in projectile:
        weapon.fixed_target = _boolean(projectile["targetGround"], weapon.fixed_target)
    for key, field in (
        ("targetGround_includeTargetHeight", "fixed_target_include_height"),
        ("disableLeadTargeting", "disable_lead_targeting"),
    ):
        if key in projectile:
            setattr(weapon, field, _boolean(projectile[key], getattr(weapon, field)))
    for key, field in (
        ("leadTargetingSpeedCalculation", "lead_targeting_speed"),
        ("targetGroundHeightOffset", "ground_target_height_offset"),
        ("targetGroundSpread", "ground_target_spread"),
    ):
        if key in projectile:
            setattr(weapon, field, _number(projectile[key], getattr(weapon, field)))
    if "targetAir" in projectile:
        weapon.targets_air = _boolean(projectile["targetAir"], weapon.targets_air)
    if "instant" in projectile:
        weapon.hitscan = _boolean(projectile["instant"], weapon.hitscan)
    else:
        # A custom asset projectile without ``instant:true`` is an ordinary
        # projectile even when its stock replacement base used hitscan fire.
        weapon.hitscan = False
    for key, field in (
        ("instantReuseLast", "instant_reuse_last"),
        ("instantReuseLast_alsoChangeTurretAim", "instant_reuse_last_change_turret_aim"),
        ("instantReuseLast_keepAreaDamageList", "instant_reuse_last_keep_area_damage_list"),
        ("moveWithParent", "move_with_parent"),
    ):
        setattr(weapon, field, _boolean(projectile.get(key, "false")))
    for key, field in (
        ("sweepOffset", "sweep_offset"),
        ("sweepOffsetFromTargetRadius", "sweep_offset_from_target_radius"),
    ):
        setattr(weapon, field, _float32(_number(projectile.get(key, "0"))))
    # game.f retains resolved laser, lightning, and image-beam projectiles
    # until h expires (B/M/g.X), so their shared IDs remain in the state.
    beam_image = projectile.get("beamImage", "").strip()
    weapon.persist_after_hit = (
        _boolean(projectile.get("laserEffect", "false"))
        or _boolean(projectile.get("lightingEffect", "false"))
        or beam_image.upper() not in {"", "NONE"})


def _apply_turret(weapon: Any, turret: dict[str, str]) -> None:
    weapon.energy_usage = _float32(_number(turret.get("energyUsage", "0")))
    weapon.laser_defence_enabled = "laserDefenceEnergyUse" in turret
    weapon.laser_defence_energy_usage = _float32(_number(turret.get("laserDefenceEnergyUse", "0")))
    weapon.intercept_projectile_tags = list(dict.fromkeys(
        tag.strip().lower() for tag in turret.get("interceptProjectiles_withTags", "").split(",") if tag.strip()))
    for key, field, default in (
        ("interceptProjectiles_andTargetingGroundUnderDistance", "intercept_ground_target_distance", "-1"),
        ("interceptProjectiles_andUnderDistance", "intercept_projectile_distance", "2000"),
        ("interceptProjectiles_andOverHeight", "intercept_projectile_height", "-999"),
    ):
        setattr(weapon, field, _float32(_number(turret.get(key, default))))
    weapon.intercept_ammo_usage = 0
    weapon.intercept_required_flags = weapon.intercept_missing_flags = 0
    if weapon.intercept_projectile_tags:
        for resource in filter(None, turret.get("resourceUsage", "").split(",")):
            key, amount = (word.strip() for word in resource.split("=", 1))
            if key == "ammo":
                weapon.intercept_ammo_usage = int(amount)
            elif key == "hasFlag":
                weapon.intercept_required_flags |= 1 << int(amount)
            elif key == "hasMissingFlag":
                weapon.intercept_missing_flags |= 1 << int(amount)
            else:
                raise ValueError(f"Unsupported tagged interception resource: {resource}")
    weapon.limiting_range = _float32(_number(turret.get("limitingRange", "99999")))
    weapon.limiting_min_range = _float32(_number(turret.get("limitingMinRange", "0")))
    weapon.limiting_angle = _float32(_number(turret.get("limitingAngle", "-1")))
    if "turnSpeed" in turret:
        weapon.turret_turn_speed = _number(
            turret["turnSpeed"], weapon.turret_turn_speed)
    if "turnSpeedAcceleration" in turret:
        weapon.turret_turn_acceleration = _number(
            turret["turnSpeedAcceleration"], weapon.turret_turn_acceleration)
    if "turnSpeedDeceleration" in turret:
        weapon.turret_turn_deceleration = _number(
            turret["turnSpeedDeceleration"], weapon.turret_turn_deceleration)
    elif "turnSpeedAcceleration" in turret:
        # A turret that declares only an acceleration rate decelerates at that
        # same rate. Without this the replaced unit's own deceleration leaks
        # through: c_tank kept tank's -1 and its turret stopped instantly,
        # which first became visible as a projectile spawned from a turret
        # angle that had over-rotated.
        weapon.turret_turn_deceleration = weapon.turret_turn_acceleration
    if "warmup" in turret:
        weapon.warmup_frames = _time_frames(turret["warmup"], weapon.warmup_frames)
    if "warmupCallDownRate" in turret:
        weapon.warmup_call_down_rate = _number(
            turret["warmupCallDownRate"], weapon.warmup_call_down_rate)
    if "warmupNoReset" in turret:
        weapon.warmup_no_reset = _boolean(
            turret["warmupNoReset"], weapon.warmup_no_reset)
    if "warmupShootDelayTransfer" in turret:
        weapon.warmup_shoot_delay_transfer = _number(
            turret["warmupShootDelayTransfer"],
            weapon.warmup_shoot_delay_transfer)
    if "canShoot" in turret:
        weapon.fire_enabled = _boolean(turret["canShoot"], weapon.fire_enabled)
    if "shouldResetTurret" in turret:
        weapon.turret_resets_to_idle = _boolean(
            turret["shouldResetTurret"], weapon.turret_resets_to_idle)
    if "recoilOffset" in turret:
        weapon.recoil_offset = _number(turret["recoilOffset"], weapon.recoil_offset)
    if "recoilOutTime" in turret:
        weapon.recoil_out_frames = _number(
            turret["recoilOutTime"], weapon.recoil_out_frames)
    if "recoilReturnTime" in turret:
        weapon.recoil_return_frames = _number(
            turret["recoilReturnTime"], weapon.recoil_return_frames)
    if "delay" in turret:
        weapon.fire_delay_frames = _time_frames(
            turret["delay"], weapon.fire_delay_frames)
    # The native y-coordinate is the forward mount distance used by the
    # structured engine.  x is the lateral mount offset.
    if "y" in turret:
        weapon.mount_forward = _number(turret["y"], weapon.mount_forward)
    if "x" in turret:
        weapon.mount_side = _number(turret["x"], weapon.mount_side)
    if "idleDir" in turret:
        weapon.turret_idle_direction = _number(
            turret["idleDir"], weapon.turret_idle_direction)
    if "idleDirReversing" in turret:
        weapon.turret_idle_direction_reversing = _number(
            turret["idleDirReversing"],
            weapon.turret_idle_direction_reversing)
    if "idleSpin" in turret:
        weapon.turret_idle_spin = _number(
            turret["idleSpin"], weapon.turret_idle_spin)
    if "idleSweepAngle" in turret:
        weapon.turret_idle_sweep_angle = _number(
            turret["idleSweepAngle"], weapon.turret_idle_sweep_angle)
    if "idleSweepDelay" in turret:
        weapon.turret_idle_sweep_delay = _number(
            turret["idleSweepDelay"], weapon.turret_idle_sweep_delay)
    if "idleSweepSpeed" in turret:
        weapon.turret_idle_sweep_speed = _number(
            turret["idleSweepSpeed"], weapon.turret_idle_sweep_speed)
    if "idleSweepAddRandomDelay" in turret:
        weapon.turret_idle_sweep_random_delay = _number(
            turret["idleSweepAddRandomDelay"],
            weapon.turret_idle_sweep_random_delay)
    if weapon.turret_idle_sweep_random_delay < 0.0:
        delay = weapon.turret_idle_sweep_delay
        weapon.turret_idle_sweep_random_delay = (
            20.0 if delay > 200.0 else 5.0 if delay > 50.0 else 1.0)
    if "idleSweepAddRandomAngle" in turret:
        weapon.turret_idle_sweep_random_angle = _number(
            turret["idleSweepAddRandomAngle"],
            weapon.turret_idle_sweep_random_angle)
    condition = turret.get("idleSweepCondition", "").replace(" ", "").lower()
    weapon.turret_idle_sweep_requires_stationary = (
        condition == "ifnotself.ismoving()")


def _apply_weapons(definition: Any, sections: dict[str, dict[str, str]]) -> None:
    definition.nano_turret_index = -1
    turret_sections = [section for section in sections
                       if section.startswith("turret_")]
    if not turret_sections:
        return
    # A replacement custom unit starts from its stock prototype, but its
    # explicit turret sections define the complete custom weapon array.  Do
    # not leave inherited stock slots beyond the last declared turret.
    weapons = list(definition.weapons[:len(turret_sections)])
    if len(weapons) < len(turret_sections):
        weapons.extend(
            rwrl_core.WeaponDef()
            for _ in range(len(turret_sections) - len(weapons))
        )
    definition.weapons = weapons
    projectile_sections = {
        section: sections[section]
        for section in sections if section.startswith("projectile_")
    }
    # An INI unit is a custom.j instance, whose g(slot) muzzle distance is the
    # turret's bn.X.  That defaults to the unit-level turretSize scaled by
    # globalScale (both default to 1) and never inherits the replaced stock
    # class's own constant.  A turret section may override it outright with
    # barrelY or size, which the JAR rejects if both are present.
    default_muzzle = (
        _number(sections.get("attack", {}).get("turretSize", "1"), 1.0) *
        _number(sections.get("core", {}).get("globalScale", "1"), 1.0))
    # bn.af likewise defaults to the unit-level [attack] aimOffsetSpread,
    # whose own default is 0.6.  Without it a custom unit keeps the stock
    # replacement's spread and samples a different per-shot aim offset.
    default_spread = _number(
        sections.get("attack", {}).get("aimOffsetSpread", "0.6"), 0.6)
    attack_turn_speed = sections.get("attack", {}).get("turretTurnSpeed")
    if attack_turn_speed is not None:
        for weapon in definition.weapons:
            weapon.turret_turn_speed = _number(
                attack_turn_speed, weapon.turret_turn_speed)
    default_rotate_with_body = _boolean(
        sections.get("attack", {}).get("turretRotateWithBody", "true"),
        True)
    turret_indices = {
        section[len("turret_"):]: index
        for index, section in enumerate(turret_sections)
    }
    death_turret = sections.get("core", {}).get("fireTurretXAtSelfOnDeath", "NONE").strip()
    definition.fire_turret_on_death = -1
    if death_turret.upper() != "NONE":
        if death_turret not in turret_indices:
            raise ValueError(f"Cannot find fireTurretXAtSelfOnDeath turret: {death_turret}")
        definition.fire_turret_on_death = turret_indices[death_turret]

    def merged_turret(section: str, visiting: set[str] | None = None) -> dict[str, str]:
        """Apply a turret's copyFrom chain before its local overrides.

        The JAR resolves turret inheritance while constructing custom.j. A
        shallow section read leaves copied slots with default projectile and
        firing fields, which is observable even when the child only changes
        its mount position.
        """
        visiting = visiting or set()
        if section in visiting:
            return dict(sections[section])
        visiting = set(visiting)
        visiting.add(section)
        current = dict(sections[section])
        parent_name = current.get("copyFrom", "").strip()
        if parent_name:
            parent_section = ""
            if parent_name in turret_indices:
                parent_section = turret_sections[turret_indices[parent_name]]
            elif "turret_" + parent_name in sections:
                parent_section = "turret_" + parent_name
            if parent_section and parent_section not in visiting:
                inherited = merged_turret(parent_section, visiting)
                inherited.update(current)
                current = inherited
        return current

    turret_delays: dict[str, float] = {}

    def turret_delay(section: str, visiting: set[str] | None = None) -> float:
        # bn.a() copies the parent's parsed m, then a local delay link resets
        # m to 9000 before a local delay can override it. Flattening copyFrom
        # makes an inherited parent's delay look like this local override.
        if section in turret_delays:
            return turret_delays[section]
        visiting = visiting or set()
        if section in visiting:
            raise ValueError('Infinite loop in turret delay inheritance')
        visiting = visiting | {section}
        current = sections[section]
        # ag.a() initializes custom.l.dM to 50 independently of the stock
        # prototype's cooldown, including disabled death/nano turrets.
        default = _time_frames(sections.get('attack', {}).get('shootDelay', ''), 50.0)
        delay = default
        parent_name = current.get('copyFrom', '').strip()
        if parent_name:
            parent_section = ('turret_' + parent_name)
            if parent_name in turret_indices:
                parent_section = turret_sections[turret_indices[parent_name]]
            if parent_section in sections:
                delay = turret_delay(parent_section, visiting)
        if current.get('linkDelayWithTurret', '').strip():
            delay = 9000.0
        if 'delay' in current:
            delay = _time_frames(current['delay'], delay)
        if delay == -1.0:
            delay = default
        turret_delays[section] = delay
        return delay

    for index, section in enumerate(turret_sections):
        turret = merged_turret(section)
        weapon = definition.weapons[index]
        weapon.turret_name = section[len("turret_"):]
        # custom.j.E() applies the unit's isFixedFiring to all ordinary
        # weapon aim passes, including non-firing parents. Slaves and the
        # selected nano turret are handled by their own controllers.
        weapon.fixed_turret = definition.attack_follow_e_flag
        # custom.j starts every bn slot with -1 acceleration/deceleration,
        # regardless of the stock unit that overrideAndReplace selected.
        # Only an explicit value in this turret or its copyFrom chain changes
        # that default.
        weapon.turret_turn_acceleration = -1.0
        weapon.turret_turn_deceleration = -1.0
        # custom.bn starts with its own recoil defaults, including a
        # seven-frame return rather than y.J()'s six-frame stock return.
        weapon.recoil_offset = 0.0
        weapon.recoil_out_frames = 4.0
        weapon.recoil_return_frames = 7.0
        weapon.fire_delay_frames = _time_frames(
            sections.get("attack", {}).get("shootDelay", ""),
            float(definition.cooldown_max))
        _apply_turret(weapon, turret)
        weapon.fire_delay_frames = turret_delay(section)
        weapon.turret_rotate_with_body = default_rotate_with_body
        weapon.attached_to_slot = turret_indices.get(
            turret.get("attachedTo", "").strip(), -1)
        if _boolean(sections[section].get("isMainNanoTurret", "false")):
            definition.nano_turret_index = index
        if _boolean(turret.get("slave", "false"), False):
            attached_to = turret.get("attachedTo", "").strip()
            if attached_to in turret_indices:
                weapon.linked_slot = turret_indices[attached_to]
        # custom.bn defaults idleDirReversing to zero for an attached turret
        # and to idleDir + 180 for a body-mounted turret.  This matters while
        # a unit is reversing, even when the INI omits idleDirReversing.
        if "idleDirReversing" not in turret:
            weapon.turret_idle_direction_reversing = (
                0.0 if weapon.attached_to_slot >= 0 else
                weapon.turret_idle_direction + 180.0)
        delay_link = turret.get("linkDelayWithTurret", "").strip()
        if delay_link:
            if delay_link in turret_indices:
                weapon.delay_link_slot = turret_indices[delay_link]
        weapon.muzzle_offset = default_muzzle
        weapon.muzzle_side = _number(turret.get("barrelX", ""), 0.0)
        weapon.muzzle_side_odd = _number(
            turret.get("barrelOffsetX_onOddShots", ""), 0.0)
        weapon.aim_offset_spread = _number(
            turret.get("aimOffsetSpread", ""), default_spread)
        if "barrelY" in turret:
            weapon.muzzle_offset = _number(turret["barrelY"], default_muzzle)
        if "size" in turret:
            weapon.muzzle_offset = _number(turret["size"], default_muzzle)
        # bn.R defaults to zero: every turret without an explicit projectile
        # uses the first declared projectile, including copied/disabled slots.
        projectile_id = turret.get("projectile", "").strip()
        candidates = [
            f"projectile_{projectile_id}",
            projectile_id if projectile_id.startswith("projectile_") else "",
        ]
        if not projectile_id and projectile_sections:
            candidates = [next(iter(projectile_sections))]
        elif (projectile_id == "0" and len(projectile_sections) == 1 and
              "projectile_1" in projectile_sections):
            candidates = ["projectile_1"]
        for candidate in candidates:
            if candidate in projectile_sections:
                _apply_projectile(weapon, projectile_sections[candidate])
                break
        weapon.cooldown_max = definition.cooldown_max
        # custom.bn J/K/L/M are independent conditions. A missing condition
        # returns true; it does not inherit custom.l eq/er/es/et.
        weapon.targets_air = _boolean(turret.get("canAttackFlyingUnits", "true"), True)
        weapon.targets_ground = _boolean(turret.get("canAttackLandUnits", "true"), True)
        weapon.targets_underwater = _boolean(turret.get("canAttackUnderwaterUnits", "true"), True)
        weapon.targets_not_touching_water = _boolean(
            turret.get("canAttackNotTouchingWaterUnits", "true"), True)

    # Aggregate fields are retained for legacy engine callers.  Stock units
    # use the first projectile as their aggregate weapon definition.
    if definition.weapons:
        first = definition.weapons[0]
        # custom.j.e(0) is the first turret's bn.n warmup. The ATTACK
        # helper uses it to leave three extra units inside the stop radius.
        definition.attack_follow_weapon_n = first.warmup_frames
        definition.damage = first.damage
        definition.splash_damage = first.splash_damage
        definition.splash_radius = first.splash_radius
        definition.projectile_speed = first.projectile_speed
        definition.projectile_lifetime = first.projectile_lifetime
        definition.projectile_tracking = first.tracking
        definition.projectile_hitscan = first.hitscan


def _asset_definition(engine: Any, sections: dict[str, dict[str, str]]) -> tuple[str, Any] | None:
    core = sections.get("core")
    if not core or not core.get("name"):
        return None
    name = core["name"].strip()
    if not name or name.upper() == "NULL":
        return None
    name = CANONICAL_ASSET_NAMES.get(name, name)
    registry = engine.registry()
    definition = registry.get(name)
    if definition is None:
        base = core.get("overrideAndReplace", "").strip()
        if base and registry.get(base) is not None:
            definition = registry.register_custom_unit_from(name, base)
        else:
            definition = rwrl_core.UnitDef()
            definition.name = name
            if not registry.register_unit(definition, True):
                return None
            definition = registry.get(name)
    return (name, definition) if definition is not None else None


def _section_values(sections: dict[str, dict[str, str]], name: str) -> dict[str, str]:
    """Apply ag.a's section copies after the file copyFrom chain is merged."""
    values = dict(sections[name])

    def inherit(source: str, depth: int) -> None:
        if depth > 10:
            raise ValueError("@copyFromSection can only be 10 levels deep")
        # Later sources fill missing keys first, including before their own
        # ancestors. Child values and section insertion order stay intact.
        parents = sections[source].get("@copyFromSection", "").split(",")
        for parent in reversed(parents):
            parent = parent.strip()
            if not parent:
                continue
            inherited = sections.get(parent, {})
            if not any(value != "IGNORE" for value in inherited.values()):
                raise ValueError(f"[{name}] @copyFromSection has no keys in {parent}")
            for key, value in inherited.items():
                if value != "IGNORE":
                    values.setdefault(key, value)
            inherit(parent, depth + 1)

    inherit(name, 0)
    return {key: value for key, value in values.items() if value != "IGNORE"}


def _action_has_effect(values: dict[str, str]) -> bool:
    # ag.a drops effectless declarations unless isVisible was specified.
    # Effects that this loader cannot execute still occupy a native index.
    names = {
        "convertTo", "whenBuilding_temporarilyConvertTo", "fireTurretXAtGround",
        "alsoTriggerAction", "alsoQueueAction", "produceUnits", "spawnUnits",
        "addUnitsIntoTransport", "transportTargetNow", "attachments_addNewUnits",
        "setCustomTarget1", "setCustomTarget2", "setUnitMemory", "shrinkArrays",
        "resourceAmount", "addResourcesWithLogic", "setResourcesWithLogic",
        "addWaypoint_type", "playAnimation", "playAnimationIfNotPlaying",
        "temporarilyAddTags", "temporarilyRemoveTags", "addGlobalTeamTags",
        "removeGlobalTeamTags", "convertResource_from",
        "showMessageToPlayer", "showMessageToAllPlayers", "showMessageToAllEnemyPlayers",
        "showQuickWarLogToPlayer", "showQuickWarLogToAllPlayers", "debugMessage",
        "setUnitStats", "switchToTeam", "teleportTo", "offsetSelfAbsolute", "sendMessageTo",
    }
    if any(values.get(key, "").strip().upper() not in {"", "NONE"} for key in names):
        return True
    if any(key in values for key in (
            "addEnergy", "setBodyRotation", "setHeight", "setBuilt", "resetCustomTimer",
            "spawnEffects", "spawnEffectsOnQueue", "playSoundAtUnit", "playSoundGlobally",
            "playSoundToPlayer", "playSoundToPlayerOnQueue")):
        return True
    for key in ("addResources", "addResourcesScaledByAIHandicaps", "takeResources"):
        resource = values.get(key, "").strip()
        if resource and any(
                "flag" in part.lower() or _number(part) != 0 or "${" in part
                for part in resource.split(",")):
            return True
    booleans = {
        "startUnloadingTransport", "forceUnloadTransportNow", "attachments_disconnect",
        "attachments_unload", "disconnectFromParent", "swapCustomTarget1And2",
        "clearAllWaypoints", "clearActiveWaypoint", "finishPlayingLastAnimation",
        "stopLastAnimation", "resetToDefaultTags", "takeResources_searchOnly",
        "resetUnitStats", "deleteSelf", "switchToNeutralTeam", "switchToAggressiveTeam",
        "clearAllActionCooldowns", "removeAllQueuedItemsWithoutRefund", "refundAllQueuedItems",
    }
    if any(_boolean(values.get(key, "false")) for key in booleans):
        return True
    return any(_number(values.get(key, "0")) > 0 for key in (
        "deleteNumUnitsFromTransport", "attachments_deleteNumUnits",
        "addAllActionCooldownsTime", "addAllActionCooldownsFor",
        "addActionCooldownTime", "addActionCooldownFor"))


def _action_sections(sections: dict[str, dict[str, str]]) -> list[dict[str, str]]:
    # ag reads legacy core conversions before action/hiddenAction sections.
    core = sections.get("core", {})
    action_sections = []
    for index in range(51):
        prefix = f"action_{index}_"
        values = {key[len(prefix):]: value for key, value in core.items()
                  if key.startswith(prefix) and value != "IGNORE"}
        # Legacy slots must first pass ag.a's declaration gate. Visibility
        # and resetCustomTimer alone do not create a core action; a literal
        # convertTo:NONE enters the parser and can retain a visibility slot.
        declared = (_action_has_effect({key: value for key, value in values.items()
                                        if key != "resetCustomTimer"}) or
                    any(key in values for key in (
                        "convertTo", "whenBuilding_temporarilyConvertTo", "fireTurretXAtGround")))
        if declared and (_action_has_effect(values) or "isVisible" in values):
            action_sections.append(values)
    # ab.e(prefix) excludes sections containing only @ directives. Such a
    # disabled inherited action must not consume a generated action index.
    for prefix in ("action_", "hiddenAction_"):
        for name in sections:
            if not name.startswith(prefix):
                continue
            values = _section_values(sections, name)
            if not any(not key.startswith("@") for key in values):
                continue
            if prefix == "hiddenAction_" or "isVisible" in values or _action_has_effect(values):
                action_sections.append(values)
    return action_sections


def _load_queue_actions(registry: Any, unit_name: str,
                        sections: dict[str, dict[str, str]]) -> None:
    definition = registry.get(unit_name)
    auto_triggers = []
    core = sections.get("core", {})
    definition.auto_trigger_cooldown_frames = _time_frames(
        core.get("autoTriggerCooldownTime", "60"), 60)
    # ag.b() appends each canBuild declaration to l.gh before ag.a()
    # assigns action IDs from that list's size. Count declarations rather
    # than available or distinct products: setRally, locked/hidden entries,
    # higher tiers and duplicates all consume indices; NONE does not.
    build_count = 0
    for values in _build_sections(sections):
        names = values.get("name")
        if names is None:
            continue
        entries = names.split(",")
        # Java String.split discards trailing empty comma fields.
        if names:
            while entries and entries[-1] == "":
                entries.pop()
        build_count += sum(name.strip().upper() != "NONE" for name in entries)
    for index, values in enumerate(_action_sections(sections), start=build_count):
        target_name = values.get("convertTo", "").strip()
        if target_name.upper() == "NONE":
            target_name = ""
        ammo_resource = re.fullmatch(
            r"ammo\s*=\s*([+-]?\d+)", values.get("addResources", "").strip())
        fire_turret = values.get("fireTurretXAtGround", "").strip()
        if fire_turret.upper() == "NONE":
            fire_turret = ""
        if fire_turret and any(values.get(key, "").strip().upper() not in {"", "NONE"} for key in (
                "fireTurretXAtGround_withOffset", "fireTurretXAtGround_withTarget",
                "fireTurretXAtGround_withProjectile", "fireTurretXAtGround_onlyOverPassableTileOf")):
            continue
        if not target_name and not ammo_resource and not fire_turret:
            continue
        target = CANONICAL_ASSET_NAMES.get(target_name, target_name)
        if target and registry.get(target) is None:
            continue
        ammo_limit = -1.0
        if ammo_resource:
            active = values.get("isActive", "true").strip()
            ammo_guard = re.fullmatch(
                r"if\s+self\.ammoIncludingQueued\(\s*lessThan\s*=\s*(\d+)\s*\)",
                active)
            if ammo_guard:
                ammo_limit = float(ammo_guard[1])
            elif active.lower() != "true":
                # Do not turn an unsupported LogicBoolean into an active button.
                continue
        try:
            ammo_price = re.fullmatch(r"ammo\s*=\s*(\d+)", values.get("price", "").strip())
            price = 0 if ammo_price else _credit_bundle(values.get("price", "0"))
            added_credits = (0 if ammo_resource else
                             _credit_bundle(values.get("addResources", "0")))
            speed = _build_speed(_numeric_template(values.get("buildSpeed", ""), sections))
        except ValueError:
            # Expressions and non-credit prices need a resource parser.
            continue
        action = rwrl_core.CustomActionDef()
        explicit_id = values.get("id", "").strip()
        action.id = ("c" + explicit_id if "id" in values else
                     f"{target_name}_{index}")
        action.convert_to = target
        action.queueable = True
        action.price = price
        action.price_ammo = int(ammo_price[1]) if ammo_price else 0
        action.high_priority_queue = _boolean(values.get("highPriorityQueue", "false"))
        if fire_turret:
            turret_names = [name[7:] for name in sections if name.startswith("turret_")]
            if fire_turret not in turret_names:
                raise ValueError(f"Cannot find fireTurretXAtGround turret: {fire_turret}")
            action.fire_turret_index = turret_names.index(fire_turret)
            action.fire_turret_count = _integer(values.get("fireTurretXAtGround_count", "1"), 1)
        action.build_speed = speed
        action.can_player_cancel = _boolean(values.get("canPlayerCancel", "true"), True)
        action.allow_multiple_in_queue = bool(not target) and _boolean(
            values.get("allowMultipleInQueue", "true"), True)
        action.ammo_including_queued_limit = ammo_limit
        action.ignore_factory_speed_modifiers = _boolean(
            values.get("buildSpeed_ignoreFactorySpeedModifiers", "false"))
        action.when_building_cannot_move = _boolean(
            values.get("whenBuilding_cannotMove", "false"))
        if "whenBuilding_rotateTo" in values:
            action.has_when_building_rotate_to = True
            action.when_building_rotate_to = _number(values["whenBuilding_rotateTo"])
        action.when_building_rotate_to_or_backwards = _boolean(
            values.get("whenBuilding_rotateTo_orBackwards", "false"))
        resources = rwrl_core.CustomResourceDelta()
        resources.credits = added_credits
        if ammo_resource:
            resources.ammo = int(ammo_resource[1])
        action.add_resources = resources
        if not registry.register_custom_action(unit_name, action):
            raise ValueError(f"Could not register custom action {unit_name}:{action.id}")
        condition = re.fullmatch(
            r"if\s+(not\s+)?self\.(?:overWater|isOverWater)\(\s*\)",
            values.get("autoTrigger", "").strip(), re.IGNORECASE)
        if condition:
            rate = values.get("autoTriggerCheckRate", core.get("autoTriggerCheckRate", "everyFrame"))
            interval = {"everyFrame": 1, "every4Frames": 4, "every8Frames": 8}.get(rate)
            if interval is None:
                raise ValueError(f"Unsupported autoTriggerCheckRate: {rate}")
            trigger = rwrl_core.CustomAutoTriggerDef()
            trigger.action_id = action.id
            trigger.when_over_water = condition[1] is None
            trigger.check_interval_frames = interval
            auto_triggers.append(trigger)
    definition.auto_triggers = auto_triggers


def _build_sections(sections: dict[str, dict[str, str]]) -> list[dict[str, str]]:
    # custom.ag reads both core canBuild_0..50 keys and canBuild sections.
    core = sections.get("core", {})
    declarations = [
        {"name": core[f"canBuild_{index}_name"],
         "forceNano": core.get(f"canBuild_{index}_forceNano", "false"),
         "tech": core.get(f"canBuild_{index}_tech", "1")}
        for index in range(51) if f"canBuild_{index}_name" in core
    ]
    declarations += [_section_values(sections, name) for name in sections
                     if name.startswith("canBuild_")]
    return declarations


def _declared_product_names(sections: dict[str, dict[str, str]]):
    # forceNano selects a positional construction action instead of d.k.
    for values in _build_sections(sections):
        force_nano = _boolean(values.get("forceNano", "false"))
        for name in values.get("name", "").split(","):
            if name.strip():
                yield name.strip(), force_nano, _integer(values.get("tech", "1"), 1)


def load_unit_assets(
    engine: Any,
    asset_root: str | Path | None = None,
    names: set[str] | None = None,
) -> list[str]:
    """Load replacement/custom unit INIs from ``resources/units``.

    Load shipped replacements, units declaring ``builtFrom``, the
    canonical extractor replacement, and their construction/conversion
    destinations. Pass ``names`` to select other asset roots and their
    destinations.
    """

    root = project_path(asset_root) if asset_root is not None else DEFAULT_ASSET_ROOT
    if not root.is_dir():
        raise FileNotFoundError(f"unit asset directory does not exist: {root}")
    registry = engine.registry()
    touched: list[str] = []
    override_aliases: list[tuple[str, str]] = []
    built_from: list[tuple[str, str]] = []
    assets: list[tuple[Path, dict, dict]] = []
    selected: set[str] = set()
    for path in sorted(root.rglob("*.ini")):
        sections = _read_ini(path)
        core = sections.get("core", {})
        name = core.get("name", "").strip()
        if not name or name.upper() == "NULL":
            continue
        # Death effects also belong to a replaced stock prototype. Resolve
        # this metadata even for assets outside the definition selection.
        replacement = core.get("overrideAndReplace", "").strip()
        resolved_sections = _inherited_sections(path, sections)
        merged_core = resolved_sections.get("core", {})
        assets.append((path, sections, resolved_sections))
        if replacement and replacement.upper() != "NONE":
            target = registry.get(replacement)
            if (target is not None and
                    merged_core.get("effectOnDeath", "").strip().lower() ==
                    "resourcepoolsmoke"):
                target.death_effect_uid_count = 2
        if names is not None:
            if name in names:
                selected.add(name)
        elif name in CANONICAL_ASSET_NAMES or name == "missing":
            selected.add(name)
        elif ((core.get("overrideAndReplace") and name.startswith("c_")) or
              (replacement and replacement.upper() != "NONE")):
            # Native ag.h() loads replacements independently of their name.
            # fabricatorT1 and both launchers do not have a c_ prefix.
            selected.add(name)
        else:
            # A mod unit that names a stock factory in builtFrom_N_name is
            # added to that factory's build list by the JAR's own asset
            # loader, so a replay can order it by action id even though it
            # replaces nothing.  Load those as well; everything without a
            # resolved builtFrom entry stays out of the verified prototype
            # table.  The entry is often inherited from a shared template.
            if any(
                    key.startswith("builtFrom_") and key.endswith("_name")
                    for key in merged_core):
                selected.add(name)

    # Declared construction and conversion destinations need their own
    # definitions even when they have no builtFrom or stock replacement.
    # Native canBuild entries can name assets such as outpostT1 directly;
    # dropping those assets also drops otherwise legal construction actions.
    by_name = {sections["core"]["name"].strip(): resolved
               for _, sections, resolved in assets}
    pending = list(selected)
    while pending:
        source = pending.pop()
        destinations = [values.get("convertTo", "").strip()
                        for values in _action_sections(by_name[source])]
        destinations.extend(name for name, _, _ in
                            _declared_product_names(by_name[source]))
        for target in destinations:
            if target in by_name and target not in selected:
                selected.add(target)
                pending.append(target)

    actions_to_load: list[tuple[str, dict]] = []
    for _, sections, resolved_sections in assets:
        if sections["core"]["name"].strip() not in selected:
            continue
        merged_core = resolved_sections.get("core", {})
        item = _asset_definition(engine, sections)
        if item is None:
            continue
        name, definition = item
        _apply_core(definition, merged_core)
        _apply_footprint_offsets(definition, merged_core)
        _apply_factory_exit(definition, merged_core)
        _apply_attack(definition, resolved_sections.get("attack", {}))
        _apply_movement(definition, resolved_sections.get("movement", {}))
        _apply_transport(definition, merged_core)
        _apply_weapons(definition, resolved_sections)
        _apply_legs(definition, resolved_sections)
        # custom.j.e() returns a persistent wreck when the asset declares a
        # wreck image. Only the metadata is needed for simulation; image bytes
        # are never loaded by the core.
        wreck_image = resolved_sections.get("graphics", {}).get(
            "image_wreak", "").strip()
        definition.has_wreck_image = bool(
            wreck_image and wreck_image.upper() != "NONE")
        touched.append(name)
        actions_to_load.append((name, resolved_sections))

        for key, value in merged_core.items():
            if not key.startswith("builtFrom_") or not key.endswith("_name"):
                continue
            factory_name = value.strip()
            if factory_name and factory_name != name:
                built_from.append((name, factory_name))

        # A custom unit that declares overrideAndReplace is accepted by the
        # stock builder whenever the replaced unit is buildable. Keep this
        # relationship in the host-side asset layer so the C++ core only sees
        # the resulting plain can_build list.
        replacement = sections.get("core", {}).get(
            "overrideAndReplace", "").strip()
        if replacement and replacement.upper() != "NONE" and replacement != name:
            override_aliases.append((name, replacement))

    # custom.ag.a(as) matches builtFrom references with equalsIgnoreCase.
    # Resolve them after every asset is registered, including custom factories.
    factory_names = {name.lower(): name
                     for name in [*registry.all_names(), *touched]}
    for custom_name in touched:
        definition = registry.get(custom_name)
        definition.built_from = factory_names.get(
            definition.built_from.lower(), definition.built_from)
        definition.built_from_factories = list(dict.fromkeys(
            factory_names.get(name.lower(), name)
            for name in definition.built_from_factories))

    for custom_name, replacement in override_aliases:
        custom_definition = registry.get(custom_name)
        replacement_definition = registry.get(replacement)
        if custom_definition is not None and replacement_definition is not None:
            replacement_definition.death_effect_uid_count = (
                custom_definition.death_effect_uid_count)
        for factory_name in registry.all_names():
            factory = registry.get(factory_name)
            if factory is None or replacement not in factory.can_build:
                continue
            if custom_name not in factory.can_build:
                factory.can_build = [*factory.can_build, custom_name]
    for custom_name, factory_name in built_from:
        factory_name = factory_names.get(factory_name.lower(), factory_name)
        factory = registry.get(factory_name)
        if factory is None or custom_name in factory.can_build:
            continue
        factory.can_build = [*factory.can_build, custom_name]
    replacements = {original.lower(): custom
                    for custom, original in override_aliases}
    for unit_name, sections in actions_to_load:
        factory = registry.get(unit_name)
        levels = {}
        previous_products = factory.factory_products if factory.has_declared_build_menu else factory.can_build
        factory_products = {name for name in previous_products
                            if registry.get(name) is not None and not registry.is_building(name)}
        for declared_name, force_nano, tech_level in _declared_product_names(sections):
            original = factory_names.get(declared_name.lower())
            product_name = replacements.get(declared_name.lower(), original)
            product = registry.get(product_name) if product_name else None
            if product is None:
                continue
            # Keep the original availability slot as well as its native
            # replacement, as for the builtFrom/replacement menus above.
            for name in (original, product_name):
                if name and name not in factory.can_build:
                    factory.can_build = [*factory.can_build, name]
                if name and (product.is_building or force_nano):
                    available = levels.setdefault(name, [])
                    if tech_level not in available:
                        available.append(tech_level)
                elif name:
                    factory_products.add(name)
        factory.build_tech_levels = levels
        factory.has_declared_build_menu = True
        factory.factory_products = sorted(factory_products)
    for unit_name, sections in actions_to_load:
        _load_queue_actions(registry, unit_name, sections)
    return touched


def load_unit_config(
    engine: Any,
    path: str | Path | None = None,
) -> list[str]:
    """Compatibility entry point used by startup/replay code.

    ``path`` points at an asset directory, not a generated copy of the unit
    data. Omitting it reads the project's resources/units directory.
    """

    return load_unit_assets(engine, path)


__all__ = ["DEFAULT_ASSET_ROOT", "load_unit_assets", "load_unit_config"]
