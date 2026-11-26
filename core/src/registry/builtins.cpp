#include <rwrl/unit_def.h>

#include <cctype>

namespace rwrl {

void UnitRegistry::reg_(const UnitDef& d) {
    defs_[d.name] = d;
}

void UnitRegistry::register_builtin_units() {
    reg_({"commandCenter", 3000, 4000, 0, 280, 70, 18, 0, 30, 1, 20, 60, MoveType::BUILDING, WeaponType::GUARD, true, 0, 280, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"extractorT1", 700, 800, 0, 0, 0, 8, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"extractorT2", 2100, 1200, 0, 0, 0, 12, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"extractorT3", 6100, 1800, 0, 0, 0, 20, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"fabricatorT1", 2200, 600, 0, 0, 0, 2, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"fabricatorT2", 6600, 1000, 0, 0, 0, 7, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"fabricatorT3", 14100, 1400, 0, 0, 0, 14, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    reg_({"mechFactory", 1000, 800, 0, 0, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {"mechGun","mechMissile","mechArtillery","mechFlame","mechLaser","mechLightning","mechMinigun"}, "", 1});
    reg_({"airFactory", 1000, 800, 0, 0, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {"heavyInterceptor","lightGunship","bomber","missileAirship","spyDrone"}, "", 1});
    reg_({"seaFactory", 1000, 800, 0, 0, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {"battleShip","heavyBattleship","lightSub","attackSubmarine","gunBoat","missileShip"}, "", 1});
    reg_({"landFactory", 700, 800, 0, 0, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {"scout","plasmaTank","mammothTank","laserTank","heavyTank","hoverTank","missileTank","combatEngineer"}, "", 1});

    reg_({"builder", 500, 200, 0.8f, 0, 0, 0, 0, 20, 1, 20, 999, MoveType::LAND, WeaponType::BUILD, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"builderShip", 500, 200, 0.8f, 0, 0, 0, 0, 20, 1, 20, 999, MoveType::HOVER, WeaponType::BUILD, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"scout", 700, 350, 1.0f, 110, 17, 0, 0, 15, 1, 20, 40, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});
    reg_({"dropship", 800, 300, 1.2f, 0, 0, 0, 0, 25, 1, 20, 999, MoveType::AIR, WeaponType::MOVE, false, 0, 250, 1, 300, 0, true, true, "", 5, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    reg_({"mechGun", 600, 500, 0.8f, 130, 23, 0, 0, 15, 1, 20, 40, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "mechFactory", 1});
    reg_({"mechMissile", 900, 500, 0.8f, 190, 70, 0, 0, 15, 1, 20, 80, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "mechFactory", 1});
    reg_({"mechArtillery", 1400, 500, 0.6f, 290, 80, 0, 0, 15, 1, 20, 180, MoveType::LAND, WeaponType::ATTACK, false, 40, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "mechFactory", 1});
    reg_({"mechFlame", 12000, 800, 0.7f, 90, 25, 0, 0, 15, 1, 20, 10, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, true, 2.0f, {}, "mechFactory", 1});
    reg_({"mechLaser", 7000, 1200, 0.6f, 310, 90, 0, 0, 15, 1, 20, 60, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, true, 0, 0.0f, false, 0.0f, {}, "mechFactory", 1});
    reg_({"mechLightning", 5200, 700, 0.7f, 180, 55, 0, 0, 15, 1, 20, 50, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 3, 120.0f, false, 0.0f, {}, "mechFactory", 1});
    reg_({"mechMinigun", 5000, 400, 0.9f, 110, 8, 0, 0, 15, 1, 20, 6, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "mechFactory", 1});

    reg_({"plasmaTank", 1000, 220, 0.8f, 165, 100, 0, 0, 20, 1, 20, 70, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});
    reg_({"mammothTank", 3900, 800, 0.5f, 220, 120, 0, 0, 25, 1, 20, 150, MoveType::LAND, WeaponType::ATTACK, false, 30, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});
    reg_({"laserTank", 1600, 600, 0.6f, 240, 80, 0, 0, 20, 1, 20, 90, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, true, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});
    reg_({"heavyTank", 800, 600, 0.8f, 160, 50, 0, 0, 20, 1, 20, 80, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});
    reg_({"hoverTank", 450, 300, 1.2f, 130, 25, 0, 0, 18, 1, 20, 45, MoveType::HOVER, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});
    reg_({"missileTank", 2500, 400, 0.7f, 260, 65, 0, 0, 20, 1, 20, 120, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});
    reg_({"experimentalTank", 14000, 3000, 0.4f, 350, 250, 0, 0, 35, 1, 20, 200, MoveType::LAND, WeaponType::ATTACK, false, 60, 300, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    reg_({"heavyInterceptor", 1200, 400, 1.5f, 150, 25, 0, 0, 15, 1, 20, 35, MoveType::AIR, WeaponType::ATTACK, false, 0, 250, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "airFactory", 1});
    reg_({"lightGunship", 250, 200, 1.3f, 110, 10, 0, 0, 12, 1, 20, 20, MoveType::AIR, WeaponType::ATTACK, false, 0, 250, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "airFactory", 1});
    reg_({"bomber", 4000, 600, 0.8f, 120, 140, 0, 0, 15, 1, 20, 200, MoveType::AIR, WeaponType::ATTACK, false, 40, 250, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "airFactory", 1});
    reg_({"missileAirship", 0, 400, 0.6f, 280, 80, 0, 0, 20, 1, 20, 140, MoveType::AIR, WeaponType::ATTACK, false, 0, 250, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "airFactory", 1});
    reg_({"spyDrone", 1500, 150, 2.0f, 0, 0, 0, 0, 10, 1, 20, 999, MoveType::AIR, WeaponType::MOVE, false, 0, 350, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "airFactory", 1});
    reg_({"experimentalGunship", 140000, 8000, 0.7f, 350, 300, 30, 0, 40, 1, 20, 150, MoveType::AIR, WeaponType::ATTACK, false, 80, 300, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    reg_({"battleShip", 1500, 900, 0.4f, 280, 70, 0, 0, 30, 1, 20, 100, MoveType::HOVER, WeaponType::ATTACK, false, 25, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "seaFactory", 1});
    reg_({"heavyBattleship", 6000, 2000, 0.3f, 380, 180, 0, 0, 40, 1, 20, 180, MoveType::HOVER, WeaponType::ATTACK, false, 50, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "seaFactory", 1});
    reg_({"lightSub", 450, 300, 0.8f, 150, 35, 0, 0, 18, 1, 20, 80, MoveType::HOVER, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "seaFactory", 1});
    reg_({"attackSubmarine", 800, 400, 0.7f, 180, 55, 0, 0, 20, 1, 20, 100, MoveType::HOVER, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "seaFactory", 1});
    reg_({"gunBoat", 300, 200, 1.0f, 100, 15, 0, 0, 15, 1, 20, 30, MoveType::HOVER, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "seaFactory", 1});
    reg_({"missileShip", 900, 350, 0.6f, 250, 60, 0, 0, 20, 1, 20, 120, MoveType::HOVER, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "seaFactory", 1});

    reg_({"turret", 500, 700, 0, 180, 41, 0, 0, 25, 1, 20, 60, MoveType::BUILDING, WeaponType::GUARD, true, 0, 250, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"antiAirTurret", 600, 600, 0, 240, 30, 0, 0, 25, 1, 20, 40, MoveType::BUILDING, WeaponType::GUARD, true, 0, 250, 1, 300, 0, true, false, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"antiAirTurretFlak", 4600, 800, 0, 200, 40, 0, 0, 25, 1, 20, 30, MoveType::BUILDING, WeaponType::GUARD, true, 20, 250, 1, 300, 0, true, false, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"outpost", 1500, 1200, 0, 0, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 300, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"laboratory", 4000, 600, 0, 0, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"repairbay", 1500, 800, 0, 0, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    reg_({"experimentalSpider", 70000, 10000, 0.5f, 290, 350, 18, 0, 50, 1, 20, 300, MoveType::LAND, WeaponType::ATTACK, false, 100, 300, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"experimentalDropship", 30000, 2000, 0.8f, 0, 0, 4, 0, 40, 1, 20, 999, MoveType::AIR, WeaponType::MOVE, false, 0, 300, 1, 300, 0, true, true, "", 10, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"modularSpider", 90000, 8000, 0.5f, 350, 200, 30, 0, 50, 1, 20, 200, MoveType::LAND, WeaponType::ATTACK, false, 70, 300, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    reg_({"nukeLauncher", 8000, 1500, 0, 500, 5000, 0, 0, 30, 1, 20, 600, MoveType::BUILDING, WeaponType::GUARD, true, 200, 300, 1, 300, 0, true, true, "", 0, true, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"antiNukeLauncher", 3500, 1000, 0, 400, 0, 0, 0, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::GUARD, true, 0, 300, 3, 120, 0, true, true, "", 0, false, true, 400, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"laserDefence", 1200, 500, 0, 200, 40, 0, 0, 25, 1, 20, 30, MoveType::BUILDING, WeaponType::GUARD, true, 0, 250, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"wall", 200, 600, 0, 0, 0, 0, 0, 20, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 100, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"combatEngineer", 800, 250, 0.9f, 80, 15, 0, 0, 18, 1, 20, 60, MoveType::LAND, WeaponType::ATTACK, false, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "landFactory", 1});

    reg_({"bug", 100, 80, 1.5f, 40, 5, 0, 0, 8, 1, 20, 999, MoveType::LAND, WeaponType::MOVE, false, 0, 150, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, true, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"bugAssault", 300, 200, 1.2f, 60, 20, 0, 0, 12, 1, 20, 30, MoveType::LAND, WeaponType::ATTACK, false, 0, 150, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, true, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"bugSpitter", 400, 150, 1.0f, 150, 30, 0, 0, 12, 1, 20, 60, MoveType::LAND, WeaponType::ATTACK, false, 0, 150, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"bugNest", 500, 500, 0, 0, 0, 0, 0, 25, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"bugQueen", 5000, 2000, 0.6f, 120, 80, 0, 0, 30, 1, 20, 120, MoveType::LAND, WeaponType::ATTACK, false, 0, 250, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    reg_({"shieldGenerator", 2000, 400, 0, 0, 0, 0, 200, 30, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"powerGenerator", 1500, 300, 0, 0, 0, 0, 0, 25, 1, 20, 999, MoveType::BUILDING, WeaponType::MOVE, true, 0, 200, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"landMine", 100, 50, 0, 0, 500, 0, 0, 10, 1, 20, 999, MoveType::BUILDING, WeaponType::TRIGGER_WHEN_IN_RANGE, true, 40, 50, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});
    reg_({"seaMine", 100, 50, 0, 0, 500, 0, 0, 10, 1, 20, 999, MoveType::BUILDING, WeaponType::TRIGGER_WHEN_IN_RANGE, true, 40, 50, 1, 300, 0, true, true, "", 0, false, false, 0, 1.0f, 0.2f, 1.0f, 1.0f, false, false, 0, 0.0f, false, 0.0f, {}, "", 1});

    // The old table above was assembled from a different unit set. Keep its
    // detailed combat defaults as templates, then publish only the names,
    // costs and D() build speeds that are present in game-lib.jar.
    const Map<Str, UnitDef> legacy = defs_;
    defs_.clear();
    struct JarUnit {
        const char* name;
        const char* template_name;
        i32 cost;
        f32 build_speed;
        f32 income;
        bool building;
    };
    static const JarUnit units[] = {
        {"extractor", "extractorT1", 700, 0.00100000005f, 8, true},
        {"landFactory", "landFactory", 700, 0.00100000005f, 0, true},
        {"airFactory", "airFactory", 1000, 0.00100000005f, 0, true},
        {"seaFactory", "seaFactory", 1000, 0.000699999975f, 0, true},
        {"commandCenter", "commandCenter", 3000, 0.000500000024f, 18, true},
        {"turret", "turret", 500, 0.000600000028f, 0, true},
        {"antiAirTurret", "antiAirTurret", 600, 0.000799999980f, 0, true},
        {"builder", "builder", 500, 0.00200000009f, 0, false},
        {"tank", "scout", 350, 0.00200000009f, 0, false},
        {"hoverTank", "hoverTank", 450, 0.00200000009f, 0, false},
        {"artillery", "mechArtillery", 900, 0.00139999995f, 0, false},
        {"helicopter", "heavyInterceptor", 650, 0.00120000006f, 0, false},
        {"airShip", "dropship", 600, 0.00200000009f, 0, false},
        {"gunShip", "lightGunship", 800, 0.00100000005f, 0, false},
        {"missileShip", "missileShip", 900, 0.00100000005f, 0, false},
        {"gunBoat", "gunBoat", 300, 0.00499999989f, 0, false},
        {"megaTank", "mammothTank", 800, 0.00150000001f, 0, false},
        {"laserTank", "laserTank", 1300, 0.00130000000f, 0, false},
        {"hovercraft", "hoverTank", 600, 0.00300000003f, 0, false},
        {"ladybug", "bug", 400, 0.00400000019f, 0, false},
        {"battleShip", "battleShip", 1500, 0.00100000005f, 0, false},
        {"tankDestroyer", "heavyTank", 800, 0.00300000003f, 0, false},
        {"heavyTank", "heavyTank", 800, 0.00109999999f, 0, false},
        {"heavyHoverTank", "hoverTank", 1000, 0.00100000005f, 0, false},
        {"laserDefence", "laserDefence", 1200, 0.00100000005f, 0, true},
        {"dropship", "dropship", 800, 0.00100000005f, 0, false},
        {"tree", nullptr, 0, 0.00249999994f, 0, false},
        {"repairbay", "repairbay", 1500, 0.00100000005f, 0, true},
        {"NukeLaucher", "nukeLauncher", 45000, 0.0000999999975f, 0, true},
        {"AntiNukeLaucher", "antiNukeLauncher", 15000, 0.000699999975f, 0, true},
        {"mammothTank", "mammothTank", 3900, 0.000899999985f, 0, false},
        {"experimentalTank", "experimentalTank", 14000, 0.000199999995f, 0, false},
        {"experimentalLandFactory", "mechFactory", 11000, 0.000349999988f, 0, true},
        {"crystalResource", nullptr, 5000, 0.00100000005f, 0, true},
        {"wall_v", "wall", 100, 0.00300000003f, 0, true},
        {"fabricator", "fabricatorT1", 1500, 0.000600000028f, 2, true},
        {"attackSubmarine", "attackSubmarine", 800, 0.00100000005f, 0, false},
        {"builderShip", "builderShip", 500, 0.00100000005f, 0, false},
        {"amphibiousJet", "dropship", 2000, 0.00100000005f, 0, false},
        {"supplyDepot", nullptr, 1000, 0.00100000005f, 0, true},
        {"experimentalHoverTank", "experimentalTank", 21000, 0.000199999995f, 0, false},
        {"turret_artillery", "mechArtillery", 2100, 0.000600000028f, 0, true},
        {"turret_flamethrower", "mechFlame", 1200, 0.000600000028f, 0, true},
        {"fogRevealer", nullptr, 1000, 0.000600000028f, 0, false},
        {"spreadingFire", nullptr, 1000, 0.000600000028f, 0, false},
        {"antiAirTurretT2", "antiAirTurret", 1700, 0.000600000028f, 0, true},
        {"turretT2", "turret", 1500, 0.000600000028f, 0, true},
        {"turretT3", "turret", 12500, 0.000300000014f, 0, true},
        {"damagingBorder", nullptr, 1000, 0.000600000028f, 0, false},
        {"zoneMarker", nullptr, 1000, 0.000600000028f, 0, false},
        {"editorOrBuilder", "builder", 500, 0.00200000009f, 0, false},
        {"marker", nullptr, 9999, 1.0f, 0, false},
    };
    for (const auto& spec : units) {
        UnitDef def{};
        if (spec.template_name != nullptr) {
            auto it = legacy.find(spec.template_name);
            if (it != legacy.end()) def = it->second;
        }
        def.name = spec.name;
        def.cost = spec.cost;
        def.build_speed = spec.build_speed;
        def.income = spec.income;
        def.is_building = spec.building;
        defs_[def.name] = std::move(def);
    }

    // The shipped extractor and fabricator overrides use custom-unit
    // generation_resources. Their default generation_delay is the full
    // 40-frame economy window, unlike stock units' 10-frame income pulse.
    defs_["extractor"].income_interval = 40.0f;
    defs_["fabricator"].income_interval = 40.0f;

    // scout is the built-in custom unit shipped in gameData/assets. It is
    // distinct from the stock tank despite using the same legacy template
    // in the table above: the JAR exposes it as u_scout at price 700 and
    // permits production from both commandCenter and landFactory.
    {
        UnitDef scout = legacy.at("scout");
        scout.name = "scout";
        scout.cost = 700;
        scout.hp = 350.0f;
        scout.speed = 1.0f;
        scout.range = 110.0f;
        scout.damage = 17.0f;
        scout.radius = 11.0f;
        scout.mass = 500.0f;
        scout.cooldown_max = 50;
        scout.move_type = MoveType::HOVER;
        scout.weapon_type = WeaponType::ATTACK;
        scout.is_building = false;
        scout.sight_range = 440.0f;
        scout.can_move = true;
        scout.airborne = false;
        scout.targets_air = true;
        scout.targets_ground = true;
        scout.build_speed = 0.0015f;
        scout.built_from = "commandCenter";
        scout.turn_speed = 2.4f;
        scout.turn_acceleration = 0.2f;
        scout.move_acceleration = 0.03f;
        scout.move_deceleration = 0.06f;
        scout.strafes = true;
        scout.move_ignores_body = true;
        scout.target_height = 1.5f;
        scout.target_height_drift = 0.8f;
        scout.weapons = {WeaponDef{}};
        scout.weapons[0].damage = 17.0f;
        scout.weapons[0].projectile_speed = 6.0f * EngineConstants::FRAME_RATE;
        scout.weapons[0].projectile_lifetime = 70.0f;
        scout.weapons[0].cooldown_max = 50;
        scout.weapons[0].tracking = true;
        scout.weapons[0].targets_air = true;
        scout.weapons[0].targets_ground = true;
        scout.weapons[0].turret_turn_speed = 2.4f;
        scout.weapons[0].aim_tolerance = 5.0f;
        scout.weapons[0].muzzle_offset = 7.0f;
        scout.weapons[0].turret_rotate_with_body = false;
        defs_[scout.name] = std::move(scout);
    }

    // game.f initializes all three damage-stage multipliers to 1.0. The
    // legacy templates used 0.2 for shield absorption, which does not match
    // any stock projectile in this JAR.
    for (auto& [_, def] : defs_) {
        def.shield_mult = 1.0f;
        def.shield_absorb = 1.0f;
        def.hp_mult = 1.0f;
    }

    // Canonical factory production lists from the built-in JAR units.
    defs_["landFactory"].construction_tech_costs[2] = 2700;
    defs_["airFactory"].construction_tech_costs[2] = 2500;
    defs_["seaFactory"].construction_tech_costs[2] = 3000;
    defs_["landFactory"].can_build = {
        "builder", "tank", "scout", "hoverTank", "artillery"
    };
    defs_["airFactory"].can_build = {};
    defs_["seaFactory"].can_build = {
        "builderShip", "gunBoat", "missileShip", "hovercraft", "battleShip",
        "attackSubmarine"
    };
    defs_["experimentalLandFactory"].can_build = {
        "experimentalTank", "experimentalHoverTank"
    };
    defs_["experimentalLandFactory"].tech_level = 2;
    defs_["commandCenter"].can_build = {"builder", "scout"};
    defs_["builder"].can_build = {
        "extractor", "turret", "antiAirTurret", "landFactory", "airFactory",
        "seaFactory", "laserDefence", "repairbay", "fabricator",
        "experimentalLandFactory", "NukeLaucher", "AntiNukeLaucher"
    };
    defs_["builderShip"].can_build = {
        "extractor", "turret", "antiAirTurret", "landFactory", "airFactory",
        "seaFactory", "fabricator", "laserDefence", "repairbay"
    };

    // Values read from game-lib.jar prototype constructors. The legacy
    // templates above only provide the remaining weapon metadata.
    struct RuntimeSpec {
        const char* name;
        f32 hp;
        f32 radius;
        f32 range;
        f32 speed;
        i32 cooldown;
    };
    static const RuntimeSpec runtime_specs[] = {
        {"extractor", 800, 18, 0, 0, 0},
        {"landFactory", 1200, 30, 0, 0, 0},
        {"airFactory", 1000, 30, 0, 0, 0},
        {"seaFactory", 1000, 45, 0, 0, 0},
        {"commandCenter", 4000, 30, 280, 0, 70},
        {"turret", 700, 16, 165, 0, 30},
        {"antiAirTurret", 800, 16, 250, 0, 80},
        {"builder", 170, 10, 30, 0.8f, 100},
        {"tank", 210, 11, 130, 1.0f, 75},
        {"hoverTank", 150, 7, 140, 1.0f, 90},
        {"artillery", 140, 18, 290, 0.9f, 240},
        {"helicopter", 150, 13, 130, 0, 60},
        {"airShip", 250, 11, 170, 0, 40},
        {"gunShip", 260, 15, 140, 0, 40},
        {"missileShip", 350, 15, 200, 1.2f, 170},
        {"gunBoat", 170, 12, 120, 1.5f, 60},
        {"megaTank", 550, 12, 140, 0.8f, 70},
        {"laserTank", 300, 14, 190, 0.7f, 450},
        {"hovercraft", 450, 15, 30, 0.9f, 100},
        {"ladybug", 130, 5, 43, 1.7f, 17},
        {"tankDestroyer", 350, 11, 150, 1.0f, 70},
        {"battleShip", 1200, 20, 240, 0.8f, 120},
        {"heavyTank", 600, 15, 160, 0.8f, 70},
        {"heavyHoverTank", 450, 11, 160, 0.7f, 75},
        {"laserDefence", 500, 19, 160, 0, 0},
        {"dropship", 500, 20, 140, 2.3f, 40},
        {"tree", 100, 3, 0, 0, 0},
        {"repairbay", 1000, 30, 230, 0, 0},
        {"NukeLaucher", 1500, 40, 0, 0, 0},
        {"AntiNukeLaucher", 2800, 24, 1000, 0, 0},
        {"mammothTank", 2900, 21, 210, 0.5f, 140},
        {"experimentalTank", 6000, 37, 310, 0.4f, 110},
        {"experimentalHoverTank", 3500, 30, 180, 0.6f, 8},
        {"experimentalLandFactory", 3200, 55, 0, 0, 0},
        {"crystalResource", 600, 11, 0, 0, 0},
        {"wall_v", 700, 15, 0, 0, 0},
        {"fabricator", 800, 25, 0, 0, 0},
        {"attackSubmarine", 260, 15, 250, 0.8f, 170},
        {"builderShip", 500, 13, 240, 0.8f, 120},
        {"amphibiousJet", 530, 12, 170, 1.4f, 110},
        {"supplyDepot", 800, 20, 0, 0, 0},
        {"turret_artillery", 1000, 16, 350, 0, 220},
        {"turret_flamethrower", 1600, 16, 155, 0, 5},
        {"fogRevealer", 100, 1, 30, 0, 100},
        {"spreadingFire", 100, 20, 0, 0, 0},
        {"antiAirTurretT2", 1400, 16, 320, 0, 70},
        {"turretT2", 1100, 16, 185, 0, 20},
        {"turretT3", 3900, 16, 320, 0, 13},
        {"damagingBorder", 100, 1, 30, 0, 100},
        {"zoneMarker", 100, 1, 30, 0, 100},
        {"editorOrBuilder", 170000, 10, 30, 0, 100},
        {"marker", 100, 1, 30, 0, 100},
    };
    for (const auto& spec : runtime_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end()) continue;
        it->second.hp = spec.hp;
        it->second.radius = spec.radius;
        it->second.range = spec.range;
        it->second.speed = spec.speed;
        it->second.cooldown_max = spec.cooldown;
    }

    // Exact inclusive cc()/cd() rectangles read from the stock prototypes.
    // cc blocks ordinary movement; cd populates the BUILDING cost layer and
    // is also the rectangle traversed when validating a build waypoint.
    struct FootprintSpec {
        const char* name;
        TileRect collision;
        TileRect placement;
    };
    static const FootprintSpec footprint_specs[] = {
        // The shipped custom extractor overrides the stock extractor's
        // two-tile-high rectangles: custom.j cc()/cd() are both one tile.
        {"extractor", {0, 0, 0, 0}, {0, 0, 0, 0}},
        {"landFactory", {-1, -1, 1, 1}, {-1, -1, 1, 3}},
        {"airFactory", {-1, -1, 1, 1}, {-1, -1, 1, 2}},
        {"seaFactory", {-1, -1, 1, 2}, {-2, -1, 2, 4}},
        {"commandCenter", {-1, -1, 1, 1}, {-1, -1, 1, 2}},
        {"turret", {0, 0, 1, 1}, {0, 0, 1, 1}},
        {"antiAirTurret", {0, 0, 1, 1}, {0, 0, 1, 1}},
        {"laserDefence", {0, 0, 1, 1}, {0, 0, 1, 1}},
        {"repairbay", {-1, -1, 1, 1}, {-1, -1, 1, 1}},
        {"NukeLaucher", {-2, -1, 2, 1}, {-2, -1, 2, 2}},
        {"AntiNukeLaucher", {-1, -1, 1, 1}, {-1, -1, 1, 1}},
        {"experimentalLandFactory", {-2, -2, 2, 2}, {-2, -2, 2, 4}},
        {"crystalResource", {0, -1, 0, 0}, {0, -1, 0, 0}},
        {"wall_v", {0, 0, 1, 0}, {0, 0, 1, 0}},
        {"fabricator", {-1, -1, 1, 1}, {-1, -1, 1, 1}},
        {"supplyDepot", {-1, -1, 0, 0}, {-1, -1, 0, 0}},
        {"turret_artillery", {0, 0, 1, 1}, {0, 0, 1, 1}},
        {"turret_flamethrower", {0, 0, 1, 1}, {0, 0, 1, 1}},
        {"antiAirTurretT2", {0, 0, 1, 1}, {0, 0, 1, 1}},
        {"turretT2", {0, 0, 1, 1}, {0, 0, 1, 1}},
        {"turretT3", {0, 0, 1, 1}, {0, 0, 1, 1}},
    };
    for (const auto& spec : footprint_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end()) continue;
        it->second.collision_footprint = spec.collision;
        it->second.placement_footprint = spec.placement;
    }

    // d.a.b (stock turrets, including AA) and d.p (laser defence)
    // override am.cZ()/da() with the full tile width/height, rather than
    // am's half-tile default. y.br() subtracts these offsets when clearing
    // wrecks around a newly accepted building.
    for (const char* name : {"turret", "turretT2", "turretT3",
                             "turret_artillery", "turret_flamethrower",
                             "antiAirTurret", "antiAirTurretT2",
                             "laserDefence"}) {
        defs_[name].building_to_footprint_offset_x = 20.0f;
        defs_[name].building_to_footprint_offset_y = 20.0f;
    }

    // am.bN() defaults to 3000 for the stock units. A small set of heavier
    // and lighter units override that value; collision displacement uses the
    // ratio between the two masses.
    for (auto& [_, def] : defs_) def.mass = 3000.0f;
    struct MassSpec {
        const char* name;
        f32 mass;
    };
    static const MassSpec mass_specs[] = {
        {"artillery", 14000.0f},
        {"gunBoat", 1500.0f},
        {"megaTank", 7000.0f},
        {"hovercraft", 12000.0f},
        {"battleShip", 9000.0f},
        {"heavyTank", 7000.0f},
        {"dropship", 16000.0f},
        {"mammothTank", 14000.0f},
        {"experimentalTank", 80000.0f},
        {"builderShip", 6000.0f},
        {"experimentalHoverTank", 80000.0f},
    };
    for (const auto& spec : mass_specs) {
        auto it = defs_.find(spec.name);
        if (it != defs_.end()) it->second.mass = spec.mass;
    }
    defs_["scout"].mass = 500.0f;

    // Upgrade actions exposed by the stock JAR.  The JAR keeps the same
    // runtime object and changes its weapon configuration, while the C++
    // registry represents the resulting tiers as canonical unit names.
    defs_["turret"].upgrade_to = "turretT2";
    defs_["turretT2"].upgrade_to = "turretT3";
    defs_["antiAirTurret"].upgrade_to = "antiAirTurretT2";
    defs_["turret"].upgrade_cost = 1000;
    defs_["turretT2"].upgrade_cost = 11000;
    defs_["antiAirTurret"].upgrade_cost = 1200;

    // Only this canonical built-in has a non-zero prototype shield pool.
    defs_["experimentalHoverTank"].shield = 5000.0f;
    // e.c.a(float) restores 0.25 shield points per JAR game frame and only
    // regenerates while the shield is still above zero.
    defs_["experimentalHoverTank"].shield_regen_rate =
        0.25f * EngineConstants::FRAME_RATE;
    defs_["experimentalHoverTank"].shield_regen_per_frame = 0.25f;
    defs_["experimentalTank"].is_experimental = true;
    defs_["experimentalHoverTank"].is_experimental = true;
    // game.units.e.d.bh() returns 1 only for experimentalTank; every other
    // canonical unit keeps y.bh() == 0. The value becomes k.k.q on path
    // requests: tiles with clearance <= q cost +100 in k.o.e().
    defs_["experimentalTank"].path_min_clearance = 1;

    // Movement type, airborne targeting state (am.i()) and orderability
    // (y.I()) from the JAR runtime prototypes.
    struct MovementSpec {
        const char* name;
        MoveType move_type;
        bool airborne;
        bool can_move;
    };
    static const MovementSpec movement_specs[] = {
        {"extractor", MoveType::NONE, false, false},
        {"landFactory", MoveType::NONE, false, false},
        {"airFactory", MoveType::NONE, false, false},
        {"seaFactory", MoveType::NONE, false, false},
        {"commandCenter", MoveType::NONE, false, false},
        {"turret", MoveType::NONE, false, false},
        {"antiAirTurret", MoveType::NONE, false, false},
        {"builder", MoveType::LAND, false, true},
        {"tank", MoveType::LAND, false, true},
        {"hoverTank", MoveType::HOVER, false, true},
        {"artillery", MoveType::LAND, false, true},
        {"helicopter", MoveType::AIR, true, true},
        {"airShip", MoveType::AIR, true, true},
        {"gunShip", MoveType::AIR, true, true},
        {"missileShip", MoveType::WATER, false, true},
        {"gunBoat", MoveType::WATER, false, true},
        {"megaTank", MoveType::LAND, false, true},
        {"laserTank", MoveType::LAND, false, true},
        {"hovercraft", MoveType::HOVER, false, true},
        {"ladybug", MoveType::LAND, false, true},
        {"battleShip", MoveType::WATER, false, true},
        {"tankDestroyer", MoveType::LAND, false, true},
        {"heavyTank", MoveType::LAND, false, true},
        {"heavyHoverTank", MoveType::HOVER, false, true},
        {"laserDefence", MoveType::NONE, false, false},
        {"dropship", MoveType::AIR, false, true},
        {"tree", MoveType::NONE, false, false},
        {"repairbay", MoveType::NONE, false, false},
        {"NukeLaucher", MoveType::NONE, false, false},
        {"AntiNukeLaucher", MoveType::NONE, false, false},
        {"mammothTank", MoveType::LAND, false, true},
        {"experimentalTank", MoveType::LAND, false, true},
        {"experimentalLandFactory", MoveType::NONE, false, false},
        {"crystalResource", MoveType::NONE, false, false},
        {"wall_v", MoveType::NONE, false, false},
        {"fabricator", MoveType::NONE, false, false},
        {"attackSubmarine", MoveType::WATER, false, true},
        {"builderShip", MoveType::WATER, false, true},
        {"amphibiousJet", MoveType::AIR, true, true},
        {"supplyDepot", MoveType::NONE, false, false},
        {"experimentalHoverTank", MoveType::HOVER, false, true},
        {"turret_artillery", MoveType::NONE, false, false},
        {"turret_flamethrower", MoveType::NONE, false, false},
        {"fogRevealer", MoveType::AIR, true, false},
        {"spreadingFire", MoveType::NONE, false, false},
        {"antiAirTurretT2", MoveType::NONE, false, false},
        {"turretT2", MoveType::NONE, false, false},
        {"turretT3", MoveType::NONE, false, false},
        {"damagingBorder", MoveType::AIR, true, false},
        {"zoneMarker", MoveType::AIR, true, false},
        {"editorOrBuilder", MoveType::LAND, false, false},
        {"marker", MoveType::AIR, true, false},
    };
    for (const auto& spec : movement_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end()) continue;
        it->second.move_type = spec.move_type;
        it->second.airborne = spec.airborne;
        it->second.can_move = spec.can_move;
    }

    // y.A()/B()/C()/D()/bi() from the stock runtime prototypes. A and B
    // control body turn rate and turn acceleration; C and D are the linear
    // acceleration and braking rates consumed by y.a(float).
    struct KinematicsSpec {
        const char* name;
        f32 turn_speed;
        f32 turn_acceleration;
        f32 move_acceleration;
        f32 move_deceleration;
        bool strafes;
    };
    static const KinematicsSpec kinematics_specs[] = {
        {"builder", 3.8f, 0.35f, 0.04f, 0.10f, false},
        {"tank", 4.1f, 0.25f, 0.07f, 0.17f, false},
        {"hoverTank", 180.0f, 0.50f, 0.04f, 0.09f, true},
        {"artillery", 1.7f, 0.05f, 0.05f, 0.12f, false},
        {"helicopter", 6.0f, 0.40f, 0.07f, 0.10f, true},
        {"airShip", 3.7f, 0.40f, 0.10f, 0.16f, true},
        {"gunShip", 4.0f, 0.40f, 0.20f, 0.10f, true},
        {"missileShip", 1.9f, 0.20f, 0.05f, 0.10f, false},
        {"gunBoat", 2.8f, 0.35f, 0.06f, 0.20f, false},
        {"megaTank", 1.2f, -1.0f, 0.05f, 0.10f, false},
        {"laserTank", 1.5f, 0.10f, 0.07f, 0.12f, false},
        {"hovercraft", 1.4f, 0.10f, 0.03f, 0.05f, false},
        {"ladybug", 5.5f, -1.0f, 0.07f, 0.12f, false},
        {"battleShip", 1.8f, 0.08f, 0.03f, 0.10f, false},
        {"tankDestroyer", 1.9f, -1.0f, 0.07f, 0.12f, false},
        {"heavyTank", 1.9f, 0.20f, 0.05f, 0.10f, false},
        {"heavyHoverTank", 20.0f, -1.0f, 0.06f, 0.09f, true},
        {"dropship", 1.4f, -1.0f, 0.03f, 0.05f, false},
        {"mammothTank", 1.0f, 0.50f, 0.04f, 0.08f, false},
        {"experimentalTank", 0.8f, 0.04f, 0.03f, 0.08f, false},
        {"attackSubmarine", 1.2f, 0.06f, 0.018f, 0.10f, false},
        {"builderShip", 1.9f, 0.12f, 0.03f, 0.10f, false},
        {"amphibiousJet", 3.8f, 0.30f, 0.03f, 0.10f, true},
        {"experimentalHoverTank", 1.1f, 0.03f, 0.02f, 0.02f, true},
    };
    for (const auto& spec : kinematics_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end()) continue;
        it->second.turn_speed = spec.turn_speed;
        it->second.turn_acceleration = spec.turn_acceleration;
        it->second.move_acceleration = spec.move_acceleration;
        it->second.move_deceleration = spec.move_deceleration;
        it->second.strafes = spec.strafes;
    }
    // These stock units override y.bj(): their inertial velocity follows
    // ch independently of body cg, with the controller's 181-degree gate.
    for (const Str& name : {
        "hoverTank", "heavyHoverTank", "experimentalHoverTank",
        "helicopter", "airShip", "amphibiousJet",
    }) {
        defs_.at(name).move_ignores_body = true;
    }
    defs_["helicopter"].reverse_speed_multiplier = 0.1f;
    for (const Str& name : {
        "missileShip", "battleShip", "heavyTank", "mammothTank",
        "experimentalTank", "experimentalHoverTank",
    }) {
        defs_[name].reverse_speed_multiplier = 1.0f;
    }

    // am.s() returns sight in map tiles. The stock map grid is 20 world
    // units per tile, so the runtime-facing world distances are 15*20 for
    // ordinary units, 20*20 for commandCenter and 14*20 for fogRevealer.
    for (auto& [_, def] : defs_) def.sight_range = 300.0f;
    defs_["commandCenter"].sight_range = 400.0f;
    defs_["scout"].sight_range = 440.0f;
    defs_["fogRevealer"].sight_range = 280.0f;
    defs_["tree"].sight_range = 0.0f;
    defs_["tree"].can_be_repaired = false;
    defs_["crystalResource"].can_be_repaired = false;
    defs_["damagingBorder"].sight_range = 0.0f;
    defs_["zoneMarker"].sight_range = 0.0f;
    defs_["spreadingFire"].sight_range = 0.0f;

    // y.k(am) is the JAR's target-category gate. These exceptions are the
    // units whose result differs from the default ability to target both
    // grounded and airborne units. A dropship is intentionally grounded for
    // this check because its am.i() result is false despite AIR movement.
    static const char* ground_only[] = {
        "turret", "tank", "artillery", "gunShip", "gunBoat", "ladybug",
        "battleShip", "tankDestroyer", "turret_artillery",
        "turret_flamethrower", "turretT2", "turretT3",
    };
    static const char* air_only[] = {
        "antiAirTurret", "airShip", "antiAirTurretT2",
    };
    for (const char* name : ground_only) {
        auto it = defs_.find(name);
        if (it == defs_.end()) continue;
        it->second.targets_ground = true;
        it->second.targets_air = false;
    }
    for (const char* name : air_only) {
        auto it = defs_.find(name);
        if (it == defs_.end()) continue;
        it->second.targets_ground = false;
        it->second.targets_air = true;
    }

    // Legacy templates are only a source for fields that have been checked
    // against this JAR. Do not let their old weapon values make canonical
    // non-combat units (notably hovercraft and map markers) fire attacks.
    // The executable weapon probe below explicitly restores every stock
    // projectile weapon, and ladybug's direct weapon is restored separately.
    for (auto& [_, def] : defs_) {
        def.damage = 0.0f;
        def.splash_damage = 0.0f;
        def.splash_radius = 0.0f;
        def.weapon_type = WeaponType::MOVE;
        def.weapons.clear();
        def.cargo_capacity = 0;
    }
    // Reapply the custom scout's attack after the canonical reset above.
    defs_["scout"].weapon_type = WeaponType::ATTACK;
    defs_["scout"].damage = 17.0f;
    defs_["scout"].weapons = {WeaponDef{}};
    defs_["scout"].weapons[0].damage = 17.0f;
    defs_["scout"].weapons[0].projectile_speed =
        6.0f * EngineConstants::FRAME_RATE;
    defs_["scout"].weapons[0].projectile_lifetime = 70.0f;
    defs_["scout"].weapons[0].cooldown_max = 50;
    defs_["scout"].weapons[0].tracking = true;
    defs_["scout"].weapons[0].targets_air = true;
    defs_["scout"].weapons[0].targets_ground = true;
    defs_["scout"].weapons[0].turret_turn_speed = 2.4f;
    defs_["scout"].weapons[0].aim_tolerance = 5.0f;
    defs_["scout"].weapons[0].muzzle_offset = 7.0f;
    defs_["scout"].weapons[0].turret_rotate_with_body = false;
    defs_["builder"].weapon_type = WeaponType::BUILD;
    defs_["builderShip"].weapon_type = WeaponType::BUILD;
    defs_["editorOrBuilder"].weapon_type = WeaponType::BUILD;
    for (const Str& name : {"builder", "builderShip", "editorOrBuilder"}) {
        defs_[name].is_builder = true;
        defs_[name].can_repair = true;
        defs_[name].can_reclaim = true;
    }
    defs_["editorOrBuilder"].can_repair_any = true;
    defs_["builderShip"].base_nano_range = 145;
    // h.b has one independent nano turret even though am.l()==false and
    // its weapon hook creates no combat projectile.
    defs_["builderShip"].weapons = {WeaponDef{}};
    defs_["builderShip"].weapons[0].cooldown_max = 120;
    defs_["builderShip"].weapons[0].mount_forward = 8.0f;
    defs_["extractor"].place_only_on_res_pool = true;
    defs_["repairbay"].can_repair = true;
    defs_["repairbay"].can_repair_any = true;
    defs_["repairbay"].can_reclaim = true;
    defs_["repairbay"].base_nano_range = 230;
    // custom.l's loader supplies these defaults from movementType before it
    // reads the writable targetHeight stat.
    for (auto& [_, def] : defs_) {
        if (def.move_type == MoveType::AIR) {
            def.target_height = 35.0f;
            def.target_height_drift = 1.5f;
        }
    }
    // The stock ladybug constructor overrides y.P; every other canonical
    // orderable unit retains y's onlyInRange default.
    defs_["ladybug"].behavior = Behavior::OUT_OF_RANGE;
    // Both stock transports expose four cargo slots. Other aircraft that
    // share legacy transport templates are not cargo carriers in this JAR.
    defs_["hovercraft"].cargo_capacity = 4;
    defs_["dropship"].cargo_capacity = 4;
    defs_["hovercraft"].hovercraft_unload_style = true;
    defs_["experimentalTank"].cargo_weight = 5;
    defs_["experimentalHoverTank"].cargo_weight = 5;
    for (const Str& name : {
        "builder", "tank", "hoverTank", "artillery", "megaTank", "laserTank",
        "ladybug", "tankDestroyer", "heavyTank", "heavyHoverTank", "tree",
        "mammothTank",
    }) {
        defs_[name].transportable = true;
    }

    // Projectile fields captured by invoking y.a(am, int) in game-lib.jar
    // and reading the resulting game.f instance. Per-slot overrides are
    // installed below after the common first-slot values are populated.
    struct WeaponSpec {
        const char* name;
        f32 direct_damage;       // game.f.U
        f32 splash_damage;       // game.f.Y
        f32 splash_radius;       // game.f.Z
        f32 speed_per_frame;     // game.f.t
        f32 lifetime_frames;     // game.f.h
        bool ballistic;          // game.f.m
        bool hitscan;            // game.f.A
    };
    static const WeaponSpec weapon_specs[] = {
        {"commandCenter", 70, 0, 0, 2.0f, 180, false, false},
        {"turret", 41, 0, 0, 5.0f, 60, false, false},
        {"antiAirTurret", 60, 0, 0, 0.3f, 220, false, false},
        {"tank", 30, 0, 0, 3.0f, 60, false, false},
        {"hoverTank", 23, 0, 0, 2.0f, 85, false, false},
        {"artillery", 0, 80, 45, 4.0f, 150, true, false},
        {"helicopter", 17, 0, 0, 8.0f, 30, false, true},
        {"airShip", 30, 0, 0, 6.0f, 75, false, false},
        {"gunShip", 35, 0, 0, 4.0f, 80, false, false},
        {"missileShip", 62, 0, 0, 2.0f, 190, false, false},
        {"gunBoat", 12, 0, 0, 8.0f, 30, false, false},
        {"megaTank", 50, 0, 0, 3.0f, 60, false, false},
        {"laserTank", 450, 0, 0, 0.0f, 8, false, true},
        {"battleShip", 65, 0, 0, 4.0f, 80, false, false},
        {"tankDestroyer", 35, 0, 0, 3.0f, 60, false, false},
        {"heavyTank", 50, 0, 0, 4.0f, 60, false, false},
        {"heavyHoverTank", 40, 0, 0, 1.0f, 95, false, false},
        {"dropship", 35, 0, 0, 4.0f, 80, false, false},
        {"mammothTank", 260, 0, 0, 4.0f, 20, false, true},
        {"experimentalTank", 40, 60, 45, 5.0f, 120, false, false},
        {"attackSubmarine", 42, 0, 0, 2.0f, 190, false, false},
        {"amphibiousJet", 45, 0, 0, 4.0f, 10, false, true},
        {"experimentalHoverTank", 380, 0, 0, 0.0f, 21, false, true},
        {"turret_artillery", 0, 100, 55, 4.0f, 150, true, false},
        {"turret_flamethrower", 0, 4, 65, 3.0f, 60, true, false},
        {"antiAirTurretT2", 60, 0, 0, 0.5f, 250, false, false},
        {"turretT2", 44, 0, 0, 6.0f, 60, false, false},
        {"turretT3", 40, 0, 0, 9.0f, 60, false, false},
    };
    for (const auto& spec : weapon_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end()) continue;
        UnitDef& def = it->second;
        def.damage = spec.direct_damage;
        def.splash_damage = spec.splash_damage;
        def.splash_radius = spec.splash_radius;
        def.projectile_speed = spec.speed_per_frame * EngineConstants::FRAME_RATE;
        def.projectile_lifetime = spec.lifetime_frames;
        def.projectile_tracking = !spec.ballistic && !spec.hitscan;
        def.projectile_hitscan = spec.hitscan;
        def.is_beam = false;
        def.is_flame = false;
    }

    auto weapon = [](f32 damage, f32 splash_damage, f32 splash_radius,
                     f32 speed_per_frame, f32 lifetime_frames, i32 cooldown,
                     bool tracking, bool hitscan,
                     bool targets_air, bool targets_ground) {
        WeaponDef result{};
        result.damage = damage;
        result.splash_damage = splash_damage;
        result.splash_radius = splash_radius;
        result.projectile_speed = speed_per_frame * EngineConstants::FRAME_RATE;
        result.projectile_lifetime = lifetime_frames;
        result.cooldown_max = cooldown;
        result.tracking = tracking;
        result.hitscan = hitscan;
        result.targets_air = targets_air;
        result.targets_ground = targets_ground;
        // y.C(slot) parks stock turrets at cg+180 while ci && bb().
        result.turret_idle_direction_reversing = 180.0f;
        return result;
    };

    // Keep one real slot for every stock projectile weapon.  The JAR uses a
    // slot state machine even for single-turret units, which is where its
    // warm-up and turret-aim gates live.
    for (const auto& spec : weapon_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end() || !it->second.weapons.empty()) continue;
        it->second.weapons = {weapon(
            spec.direct_damage, spec.splash_damage, spec.splash_radius,
            spec.speed_per_frame, spec.lifetime_frames,
            it->second.cooldown_max, !spec.ballistic && !spec.hitscan,
            spec.hitscan, it->second.targets_air, it->second.targets_ground)};
    }

    // bl(), b(slot) and per-slot game.f samples from the JAR. Stock jet's
    // third slot runs the ordinary turret cycle with a no-op projectile hook.
    defs_["antiAirTurret"].weapons = {
        weapon(60, 0, 0, 0.3f, 220, 80, true, false, true, false),
        weapon(60, 0, 0, 0.3f, 220, 80, true, false, true, false),
        weapon(60, 0, 0, 0.3f, 220, 80, true, false, true, false),
    };
    defs_["battleShip"].weapons = {
        weapon(65, 0, 0, 4.0f, 80, 120, true, false, false, true),
        weapon(65, 0, 0, 4.0f, 80, 92, true, false, false, true),
    };
    for (const Str& name : {"battleShip", "heavyTank"}) {
        for (auto& slot : defs_[name].weapons) {
            // h.a/e.f.H/I/J: the turret base recoils during its own reload.
            slot.recoil_offset = -2.0f;
            slot.recoil_out_frames = 4.0f;
            slot.recoil_return_frames = 12.0f;
        }
    }
    // h.a.C(slot) parks the two turrets on opposite sides while the hull
    // remains in reverse, including idle time after its MOVE is finished.
    defs_["battleShip"].weapons[0].turret_idle_direction_reversing = 140.0f;
    defs_["battleShip"].weapons[1].turret_idle_direction_reversing = -140.0f;
    defs_["experimentalTank"].weapons = {
        weapon(40, 60, 45, 5.0f, 120, 110, true, false, false, true),
        weapon(40, 60, 45, 5.0f, 120, 90, true, false, false, true),
        weapon(40, 60, 45, 5.0f, 120, 70, true, false, false, true),
        weapon(40, 60, 45, 5.0f, 120, 50, true, false, false, true),
        weapon(60, 0, 0, 3.0f, 190, 110, true, false, true, false),
        weapon(60, 0, 0, 3.0f, 190, 90, true, false, true, false),
    };
    defs_["amphibiousJet"].weapons = {
        weapon(45, 0, 0, 4.0f, 10, 110, false, true, true, true),
        weapon(45, 0, 0, 4.0f, 10, 110, false, true, true, true),
        weapon(0, 0, 0, 4.0f, 10, 110, false, true, true, true),
    };
    defs_["antiAirTurretT2"].weapons = {
        weapon(60, 0, 0, 0.5f, 250, 70, true, false, true, false),
        weapon(60, 0, 0, 0.5f, 250, 70, true, false, true, false),
        weapon(60, 0, 0, 0.5f, 250, 70, true, false, true, false),
    };

    struct TurretSpec {
        const char* name;
        i32 slot;
        f32 warmup;
        bool enabled;
        i32 linked;
        f32 max_speed;
        f32 acceleration;
        f32 deceleration;
        bool fixed;
    };
    static const TurretSpec turret_specs[] = {
        {"commandCenter", 0, 0, true, -1, 999, -1, -1, false},
        {"turret", 0, 0, true, -1, 5, .5f, .5f, false},
        {"antiAirTurret", 0, 0, true, -1, 6, .5f, .5f, false},
        {"antiAirTurret", 1, 0, true, -1, 6, .5f, .5f, false},
        {"antiAirTurret", 2, 0, false, -1, 6, .5f, .5f, false},
        {"tank", 0, 0, true, -1, 4, -1, -1, false},
        {"hoverTank", 0, 0, true, -1, 4, .2f, .2f, false},
        {"artillery", 0, 0, true, -1, 99, -1, -1, true},
        {"helicopter", 0, 0, true, -1, 16, -1, -1, false},
        {"airShip", 0, 0, true, -1, 3.7f, .4f, .4f, false},
        {"gunShip", 0, 0, true, -1, 99, -1, -1, false},
        {"missileShip", 0, 0, true, -1, 99, -1, -1, false},
        {"gunBoat", 0, 0, true, -1, 99, -1, -1, false},
        {"builderShip", 0, 30, true, -1, 3.5f, .25f, .25f, false},
        {"megaTank", 0, 0, true, -1, 2, -1, -1, false},
        {"laserTank", 0, 80, true, -1, 3, -1, -1, false},
        {"battleShip", 0, 0, true, -1, 2.5f, .08f, .08f, false},
        {"battleShip", 1, 30, true, -1, 2.5f, .08f, .08f, false},
        {"tankDestroyer", 0, 0, true, -1, 3, -1, -1, false},
        {"heavyTank", 0, 0, true, -1, 3, .12f, .12f, false},
        {"heavyHoverTank", 0, 0, true, -1, 2.4f, -1, -1, false},
        {"dropship", 0, 0, true, -1, 99, -1, -1, false},
        {"mammothTank", 0, 60, true, -1, 2.5f, .08f, .08f, false},
        {"experimentalTank", 0, 0, true, -1, 2.5f, .08f, .08f, false},
        {"experimentalTank", 1, 20, true, -1, 2.5f, .08f, .08f, false},
        {"experimentalTank", 2, 40, true, -1, 2.5f, .08f, .08f, false},
        {"experimentalTank", 3, 60, true, -1, 2.5f, .08f, .08f, false},
        {"experimentalTank", 4, 0, true, -1, 4.5f, 1, 1, false},
        {"experimentalTank", 5, 20, true, -1, 4.5f, 1, 1, false},
        {"attackSubmarine", 0, 10, true, -1, 2.5f, .08f, .08f, false},
        {"amphibiousJet", 0, 25, true, -1, 4, .35f, .38f, false},
        {"amphibiousJet", 1, 35, true, -1, 4, .35f, .38f, false},
        {"amphibiousJet", 2, 45, true, -1, 4, .35f, .38f, false},
        {"experimentalHoverTank", 0, 8, true, -1, 1.5f, -1, -1, false},
        {"turret_artillery", 0, 0, true, -1, 2.5f, .2f, .2f, false},
        {"turret_flamethrower", 0, 0, true, -1, 11, 2, 2, false},
        {"antiAirTurretT2", 0, 0, true, -1, 6, .5f, .5f, false},
        {"antiAirTurretT2", 1, 0, true, 0, 6, .5f, .5f, false},
        {"antiAirTurretT2", 2, 25, true, 0, 6, .5f, .5f, false},
        {"turretT2", 0, 0, true, -1, 5, .5f, .5f, false},
        {"turretT3", 0, 0, true, -1, 5, .5f, .5f, false},
    };
    for (const auto& spec : turret_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end() || spec.slot < 0 ||
            spec.slot >= static_cast<i32>(it->second.weapons.size())) continue;
        WeaponDef& slot = it->second.weapons[spec.slot];
        slot.warmup_frames = spec.warmup;
        slot.fire_enabled = spec.enabled;
        slot.linked_slot = spec.linked;
        slot.turret_turn_speed = spec.max_speed;
        slot.turret_turn_acceleration = spec.acceleration;
        slot.turret_turn_deceleration = spec.deceleration;
        slot.fixed_turret = spec.fixed;
        // TurretSpec.fixed is the unit's native E(), shared by every slot.
        it->second.attack_follow_e_flag = spec.fixed;
    }

    struct MuzzleSpec {
        const char* name;
        f32 offset;
    };
    static const MuzzleSpec muzzle_specs[] = {
        {"turret", 21}, {"antiAirTurret", 9}, {"tank", 20},
        {"hoverTank", 2}, {"artillery", 20}, {"helicopter", 7},
        {"airShip", 10}, {"gunShip", 15}, {"megaTank", 12},
        {"laserTank", 19}, {"battleShip", 15}, {"tankDestroyer", 10},
        {"heavyTank", 21}, {"heavyHoverTank", 16}, {"dropship", 15},
        {"mammothTank", 22}, {"experimentalTank", 20},
        {"builderShip", 11}, {"experimentalHoverTank", 8},
        {"turret_artillery", 23}, {"turret_flamethrower", 18},
        {"antiAirTurretT2", 9}, {"turretT2", 21}, {"turretT3", 23},
    };
    for (const auto& spec : muzzle_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end()) continue;
        for (WeaponDef& slot : it->second.weapons) {
            slot.muzzle_offset = spec.offset;
        }
    }

    // game.f updates t toward r by s after each active flight pass,
    // including zero horizontal distance. A non-positive r disables
    // the speed approach even though the stock constructor still leaves
    // s=0.1 on the projectile.
    struct SpeedProfileSpec {
        const char* name;
        i32 slot;
        f32 target_speed_per_frame;
        f32 acceleration_per_frame;
    };
    static const SpeedProfileSpec speed_profile_specs[] = {
        {"commandCenter", 0, 5, .1f},
        {"antiAirTurret", 0, 6, .1f},
        {"antiAirTurret", 1, 6, .1f},
        {"antiAirTurret", 2, 6, .1f},
        {"hoverTank", 0, 6, .2f},
        {"heavyHoverTank", 0, 7, .2f},
        {"experimentalTank", 4, 6, .1f},
        {"experimentalTank", 5, 6, .1f},
        {"antiAirTurretT2", 0, 7, .1f},
        {"antiAirTurretT2", 1, 7, .1f},
        {"antiAirTurretT2", 2, 7, .1f},
    };
    for (const auto& spec : speed_profile_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end() || spec.slot < 0 ||
            spec.slot >= static_cast<i32>(it->second.weapons.size())) continue;
        WeaponDef& slot = it->second.weapons[spec.slot];
        slot.projectile_target_speed =
            spec.target_speed_per_frame * EngineConstants::FRAME_RATE;
        slot.projectile_acceleration =
            spec.acceleration_per_frame * EngineConstants::FRAME_RATE;
    }

    // Subclasses that override y.G(slot) place their turret bases away from
    // the unit origin. Store the equivalent local forward/side vector so it
    // rotates with the hull. experimentalTank's two missile slots are the
    // stock exception that ignore E(slot) and spawn directly at the hull.
    struct MountSpec {
        const char* name;
        i32 slot;
        f32 forward;
        f32 side;
        bool uses_muzzle;
    };
    constexpr f32 diagonal_mount = 12.727922061f; // 18 * sin(45 degrees)
    static const MountSpec mount_specs[] = {
        {"turret", 0, 5, 0, true},
        {"turretT2", 0, 5, 0, true},
        {"turretT3", 0, 5, 0, true},
        {"turret_artillery", 0, 5, 0, true},
        {"turret_flamethrower", 0, 5, 0, true},
        {"antiAirTurret", 0, 5, 0, true},
        {"antiAirTurret", 1, 5, 0, true},
        {"antiAirTurret", 2, 5, 0, true},
        {"antiAirTurretT2", 0, 5, 0, true},
        {"antiAirTurretT2", 1, 5, 0, true},
        {"antiAirTurretT2", 2, 5, 0, true},
        {"battleShip", 0, 22, 0, true},
        {"battleShip", 1, 4, 0, true},
        {"experimentalTank", 0, 5 + diagonal_mount, -diagonal_mount, true},
        {"experimentalTank", 1, 5 + diagonal_mount, diagonal_mount, true},
        {"experimentalTank", 2, -diagonal_mount, diagonal_mount, true},
        {"experimentalTank", 3, -diagonal_mount, -diagonal_mount, true},
        {"experimentalTank", 4, 0, 0, false},
        {"experimentalTank", 5, 0, 0, false},
        {"amphibiousJet", 0, 12, -12, true},
        {"amphibiousJet", 1, 12, 12, true},
    };
    for (const auto& spec : mount_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end() || spec.slot < 0 ||
            spec.slot >= static_cast<i32>(it->second.weapons.size())) continue;
        WeaponDef& slot = it->second.weapons[spec.slot];
        slot.mount_forward = spec.forward;
        slot.mount_side = spec.side;
        slot.projectile_uses_muzzle = spec.uses_muzzle;
    }

    struct FlightSpec {
        const char* name;
        i32 slot;
        f32 launch_height;
        f32 cruise_height;
        f32 vertical_speed;
    };
    static const FlightSpec flight_specs[] = {
        {"commandCenter", 0, 40, 60, 2},
        {"missileShip", 0, 40, 60, 2},
        {"experimentalTank", 4, 10, 15, 2},
        {"experimentalTank", 5, 10, 15, 2},
        {"attackSubmarine", 0, 40, 60, 2},
    };
    for (const auto& spec : flight_specs) {
        auto it = defs_.find(spec.name);
        if (it == defs_.end() || spec.slot < 0 ||
            spec.slot >= static_cast<i32>(it->second.weapons.size())) continue;
        WeaponDef& slot = it->second.weapons[spec.slot];
        slot.height_guided = true;
        slot.launch_height = spec.launch_height;
        slot.cruise_height = spec.cruise_height;
        slot.vertical_speed = spec.vertical_speed;
    }

    // Projectile target/damage modifiers sampled from game.f.ai..am. All
    // unlisted stock weapons retain the constructor defaults of 1.0.
    defs_["mammothTank"].shield_absorb = 0.0f;
    defs_["mammothTank"].building_damage_mult = 0.5f;
    defs_["amphibiousJet"].shield_absorb = 0.1f;
    defs_["amphibiousJet"].building_damage_mult = 0.5f;
    defs_["experimentalHoverTank"].shield_mult = 0.75f;
    for (WeaponDef& slot : defs_["amphibiousJet"].weapons) {
        slot.shield_absorb = 0.1f;
        slot.building_damage_mult = 0.5f;
        slot.persist_after_hit = true;
        slot.warmup_call_down_rate = 0.2f;
    }
    for (WeaponDef& slot : defs_["mammothTank"].weapons) {
        slot.shield_absorb = 0.0f;
        slot.building_damage_mult = 0.5f;
        // e.l.a(am,slot) sets game.f.M for its retained lightning hit.
        slot.persist_after_hit = true;
    }
    for (WeaponDef& slot : defs_["experimentalHoverTank"].weapons) {
        slot.shield_mult = 0.75f;
        // e.c.q(slot) returns zero; its retained beam supplies K/L instead.
        slot.aim_offset_spread = 0.0f;
    }
    // game.f.C excludes this ballistic flame projectile from laser defence.
    // It is independent of the custom-content expanding-area flags.
    for (WeaponDef& slot : defs_["turret_flamethrower"].weapons) {
        slot.projectile_flame_weapon = true;
        slot.laser_defence_excluded = true;
    }

    // laserDefence has no y.a(am, int) weapon. It spends an internal energy
    // pool to intercept eligible projectiles instead of attacking units.
    auto& laser_defence = defs_["laserDefence"];
    laser_defence.damage = 0.0f;
    laser_defence.splash_damage = 0.0f;
    laser_defence.weapon_type = WeaponType::MOVE;
    laser_defence.max_energy = 1.0f;
    laser_defence.initial_energy = 1.0f;
    laser_defence.energy_per_shot = 0.11f;
    laser_defence.energy_regen_rate = 0.024f; // 0.0004 per JAR frame

    // ladybug applies damage directly without creating a game.f projectile.
    defs_["ladybug"].damage = 14.0f;
    defs_["ladybug"].is_melee = true;

    // Stockpiled missiles in the JAR are action-built, not generic weapon
    // ammunition. Keep these buildings out of automatic combat/reload until
    // the action system is implemented, while preserving their direct fields.
    auto& nuke = defs_["NukeLaucher"];
    nuke.hp = 1500.0f;
    nuke.radius = 40.0f;
    nuke.weapon_type = WeaponType::MOVE;
    nuke.ammo_max = 4;
    nuke.initial_ammo = 0;
    auto& anti_nuke = defs_["AntiNukeLaucher"];
    anti_nuke.hp = 2800.0f;
    anti_nuke.radius = 24.0f;
    anti_nuke.weapon_type = WeaponType::MOVE;
    anti_nuke.ammo_max = 12;
    anti_nuke.initial_ammo = 0;
    anti_nuke.nuke_range = 2200.0f;

    // am.l() is an attack capability, independent of the active waypoint.
    // The restored offensive weapons include ladybug's direct damage.
    for (auto& [_, def] : defs_) {
        def.can_attack = def.damage > 0.0f || def.splash_damage > 0.0f;
        // Stock y.e(0) is the first turret's warmup too. The attack
        // approach helper uses it to shrink its stopping radius by three.
        if (!def.weapons.empty()) {
            def.attack_follow_weapon_n = def.weapons.front().warmup_frames;
        }
    }
    // dropship exposes an unused firing method but overrides l() to false.
    defs_["dropship"].can_attack = false;

    // gameData/assets/units/light_gunship/light_gunship.ini is a shipped
    // custom unit with its own u_lightGunship production action. It is not
    // the stock gunShip: notably its price is 250 rather than 800.
    UnitDef light_gunship = legacy.at("lightGunship");
    light_gunship.name = "lightGunship";
    light_gunship.cost = 250;
    light_gunship.hp = 50.0f;
    light_gunship.mass = 250.0f;
    light_gunship.soft_collision_on_all = 3;
    light_gunship.speed = 1.1f;
    light_gunship.range = 120.0f;
    light_gunship.damage = 10.0f;
    light_gunship.radius = 7.0f;
    light_gunship.cooldown_max = 40;
    light_gunship.move_type = MoveType::AIR;
    light_gunship.weapon_type = WeaponType::ATTACK;
    light_gunship.is_building = false;
    light_gunship.sight_range = 200.0f;
    light_gunship.targets_air = false;
    light_gunship.targets_ground = true;
    light_gunship.built_from = "airFactory";
    light_gunship.tech_level = 1;
    light_gunship.build_speed = 0.00300000003f;
    light_gunship.projectile_speed = 6.0f * EngineConstants::FRAME_RATE;
    light_gunship.projectile_lifetime = 70.0f;
    light_gunship.projectile_tracking = true;
    light_gunship.projectile_hitscan = false;
    light_gunship.can_move = true;
    light_gunship.airborne = true;
    light_gunship.turn_speed = 99.0f;
    light_gunship.turn_acceleration = 99.0f;
    light_gunship.move_acceleration = 0.02f;
    light_gunship.move_deceleration = 0.06f;
    light_gunship.strafes = true;
    light_gunship.move_ignores_body = true;
    light_gunship.reverse_speed_multiplier = 0.6f;
    light_gunship.target_height = 17.0f;
    light_gunship.target_height_drift = 1.0f;
    light_gunship.slow_death_fall = true;
    light_gunship.weapons = {WeaponDef{}};
    light_gunship.weapons[0].damage = 10.0f;
    light_gunship.weapons[0].projectile_speed =
        6.0f * EngineConstants::FRAME_RATE;
    light_gunship.weapons[0].projectile_lifetime = 70.0f;
    light_gunship.weapons[0].cooldown_max = 40;
    light_gunship.weapons[0].recoil_offset = -1.0f;
    light_gunship.weapons[0].recoil_out_frames = 10.0f;
    light_gunship.weapons[0].recoil_return_frames = 14.0f;
    light_gunship.weapons[0].tracking = true;
    light_gunship.weapons[0].targets_air = false;
    light_gunship.weapons[0].targets_ground = true;
    light_gunship.weapons[0].turret_turn_speed = 6.4f;
    light_gunship.weapons[0].turret_turn_acceleration = 0.4f;
    light_gunship.weapons[0].turret_turn_deceleration = 0.4f;
    light_gunship.weapons[0].muzzle_offset = 9.0f;
    light_gunship.weapons[0].aim_offset_spread = 0.4f;
    custom_defs_[light_gunship.name] = std::move(light_gunship);

    // g.a(...) recognizes scoutShip as a special legacy fallback even though
    // it is absent from ar.values(). Keep it and shipped custom map units in
    // a separate lookup so the canonical registry surface stays unchanged.
    auto add_map_def = [&](const Str& name, UnitDef def) {
        def.name = name;
        map_defs_[name] = std::move(def);
    };
    UnitDef scout_ship = defs_["missileShip"];
    scout_ship.cost = 900;
    scout_ship.hp = 350.0f;
    scout_ship.radius = 15.0f;
    scout_ship.range = 200.0f;
    scout_ship.damage = 62.0f;
    scout_ship.move_type = MoveType::WATER;
    scout_ship.speed = 1.2f;
    scout_ship.turn_speed = 1.9f;
    scout_ship.move_acceleration = 0.05f;
    scout_ship.move_deceleration = 0.1f;
    add_map_def("scoutShip", scout_ship);

    UnitDef bug_melee = defs_["ladybug"];
    bug_melee.cost = 300;
    bug_melee.hp = 150.0f;
    bug_melee.radius = 8.0f;
    bug_melee.range = 9.0f;
    bug_melee.damage = 23.0f;
    bug_melee.speed = 1.5f;
    bug_melee.cooldown_max = 30;
    add_map_def("bugMelee", bug_melee);

    UnitDef bug_ranged = bug_melee;
    bug_ranged.cost = 650;
    bug_ranged.hp = 380.0f;
    bug_ranged.radius = 9.0f;
    bug_ranged.range = 170.0f;
    bug_ranged.damage = 25.0f;
    bug_ranged.speed = 1.1f;
    bug_ranged.cooldown_max = 60;
    bug_ranged.is_melee = false;
    add_map_def("bugRanged", bug_ranged);

    UnitDef bug_spore = bug_melee;
    bug_spore.cost = 100;
    bug_spore.hp = 160.0f;
    bug_spore.weapon_type = WeaponType::BUILD;
    bug_spore.damage = 0.0f;
    bug_spore.range = 0.0f;
    bug_spore.speed = 1.4f;
    add_map_def("bugSpore", bug_spore);

    UnitDef bug_nest = defs_["supplyDepot"];
    bug_nest.cost = 600;
    bug_nest.hp = 800.0f;
    bug_nest.radius = 18.0f;
    bug_nest.is_building = true;
    bug_nest.can_move = false;
    bug_nest.move_type = MoveType::NONE;
    bug_nest.weapon_type = WeaponType::MOVE;
    add_map_def("bugNest", bug_nest);
}

} // namespace rwrl
