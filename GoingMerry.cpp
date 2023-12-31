#include "GoingMerry.h"
#include <iostream>


using namespace sc2;
using namespace std;


#pragma region Structs

//Ignores Overlords, workers, and structures
struct IsArmy {
    IsArmy(const ObservationInterface* obs) : observation_(obs) {}

    bool operator()(const Unit& unit) {
        auto attributes = observation_->GetUnitTypeData().at(unit.unit_type).attributes;
        for (const auto& attribute : attributes) {
            if (attribute == Attribute::Structure) {
                return false;
            }
        }
        switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::ZERG_OVERLORD: return false;
        case UNIT_TYPEID::PROTOSS_PROBE: return false;
        case UNIT_TYPEID::ZERG_DRONE: return false;
        case UNIT_TYPEID::TERRAN_SCV: return false;
        case UNIT_TYPEID::ZERG_QUEEN: return false;
        case UNIT_TYPEID::ZERG_LARVA: return false;
        case UNIT_TYPEID::ZERG_EGG: return false;
        case UNIT_TYPEID::TERRAN_MULE: return false;
        case UNIT_TYPEID::TERRAN_NUKE: return false;
        default: return true;
        }
    }

    const ObservationInterface* observation_;
};

struct IsAttackable {
    bool operator()(const Unit& unit) {
        switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::ZERG_OVERLORD: return false;
        case UNIT_TYPEID::ZERG_OVERSEER: return false;
        case UNIT_TYPEID::PROTOSS_OBSERVER: return false;
        default: return true;
        }
    }
};

#pragma endregion


#pragma region Game Managers

void GoingMerry::OnGameStart() { 

    observation = Observation();
    game_info = observation->GetGameInfo();
    expansions = search::CalculateExpansionLocations(Observation(), Query());
    start_location = Observation()->GetStartLocation();
    staging_location = start_location;
    
    // Save main base location
    Units base = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Point2D base_loc(base.front()->pos.x, base.front()->pos.y);
    base_locations.push_back(base_loc);

    srand(time(0)); // use current time as seed for random generator
    debug = true;  // Set debug mode
    return; 
}

void GoingMerry::OnStep() 
{ 

    const ObservationInterface* observation = Observation();

    // Get game info
    const GameInfo& game_info = observation->GetGameInfo();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

    // In game time
    // Game loop increments by 1 for every 22.4 milliseconds of game time
    uint32_t current_game_loop = observation->GetGameLoop();
    float ingame_time = current_game_loop / 22.4f;

    // Supply info
    uint32_t current_supply = observation->GetFoodUsed();
    uint32_t supply_cap = observation->GetFoodCap();

    // Mineral and Gas info
    uint32_t current_minerals = observation->GetMinerals();
    uint32_t current_gas = observation->GetVespene();
        
    // ____________________________________________________________________________________
    // Throttle some behavior that can wait to avoid duplicate orders.
    int frames_to_skip = 4;
    if (current_supply >= observation->GetFoodCap()) {
        frames_to_skip = 6;
    }

    if (current_game_loop % frames_to_skip != 0) {
        return;
    }
    // ____________________________________________________________________________________

    if (current_supply > (supply_cap - 8))
    {
        TryBuildPylon();
    }

    if (!bases.empty())
    {
        if (bases.size() == 3 && observation->GetFoodWorkers() < (40))
        {
            if (current_minerals < 100)
            {
                TryBuildProbe();
                return;
            }
        }
    }

    ManageArmy();
    
    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    
    TrySendScouts();

    ManageUpgrades();

    TryBuildBaseArmy();

    BuildOrder(ingame_time, current_supply, current_minerals, current_gas);

    //TryBuildArmy();  // Standard army build

    TryBuildAdaptiveArmy();  // Adaptive army build

    if (enemy_race == -1)
    {
        FindEnemyRace();
    }

    if (TryBuildProbe()) {
        return;
    }
}

void GoingMerry::OnUnitIdle(const Unit* unit)
{
    switch (unit->unit_type.ToType())
    {
        case UNIT_TYPEID::PROTOSS_PROBE: {
            MineIdleWorkers(unit, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID:: PROTOSS_ASSIMILATOR);
            break;
        }

        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
        {
            const ObservationInterface* observation = Observation();
            Units bases = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));

            if (!warpgate_researched)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::RESEARCH_WARPGATE);
                OnUpgradeCompleted(UPGRADE_ID::WARPGATERESEARCH);

                if (!bases.empty())
                {
                    Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_TIMEWARP, unit);
                }
            }
            break;
        }
            
        default:
        {
            break;
        }
    }
}

// Change upgrade status
void GoingMerry::OnUpgradeCompleted(UpgradeID upgrade)
{
    switch (upgrade.ToType())
    {
        case UPGRADE_ID::WARPGATERESEARCH:
        {
            warpgate_researched = true;
        }
        case UPGRADE_ID::EXTENDEDTHERMALLANCE:
        {
            thermal_lance_researched = true;
        }
        case UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1:
        {
            ground_wep_1_researched = true;
        }
        case UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2:
        {
            ground_wep_2_researched = true;
        }
        case UPGRADE_ID::BLINKTECH:
        {
            blink_researched = true;
        }
        case UPGRADE_ID::CHARGE:
        {
            charge_researched = true;
        }
        default:
            break;
    }
}

#pragma endregion


#pragma region Debug Tools

void GoingMerry::printLog(string message, bool step)
{
    const ObservationInterface* observation = Observation();
    Units army = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));

    if (debug)
    {
        cout << "DEBUG LOG: " << message << endl;

        if (step)
        {
            int temp;
            cin >> temp;
        }
    }
}

#pragma endregion


#pragma region TryBuildStructure Variants

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }
    if (!unit_to_build)
        return false;

    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D target_position = Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f);

    if (Query()->Placement(ability_type_for_structure, target_position))
    {
        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            target_position);

        return true;
    }

    return false;
}

//Try build structure given a location. This is used most of the time
bool GoingMerry::TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool is_expansion) {

    const ObservationInterface* observation = Observation();
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));

    //if we have no workers Don't build
    if (workers.empty()) {
        return false;
    }

    // Check to see if there is already a worker heading out to build it
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
    }

    // If no worker is already building one, get a random worker to build one
    const Unit* unit = GetRandomEntry(workers);

    // Check to see if unit can make it there
    if (Query()->PathingDistance(unit, location) < 0.1f) {
        return false;
    }
    if (!is_expansion) {
        for (const auto& expansion : expansions) {
            if (Distance2D(location, Point2D(expansion.x, expansion.y)) < 7) {
                return false;
            }
        }
    }
    // Check to see if unit can build there
    if (Query()->Placement(ability_type_for_structure, location)) {
        Actions()->UnitCommand(unit, ability_type_for_structure, location);
        return true;
    }
    return false;

}

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE, bool is_expansion) {
    const ObservationInterface* observation = Observation();

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }
    if (!unit_to_build)
        return false;

    if (Query()->PathingDistance(unit_to_build, position) < 0.1f)
    {
        return false;
    }
    if (!is_expansion)
    {
        for (const auto& expansion : expansions)
        {
            if (Distance2D(position, Point2D(expansion.x, expansion.y)) < 7)
            {
                return false;
            }
        }
    }

    if (Query()->Placement(ability_type_for_structure, position))
    {
        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            position);
        return true;
    }

    return false;

}

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE, bool is_expansion) {
    //const ObservationInterface* observation = Observation();

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }

    if (!unit_to_build)
        return false;
       
    if (Query()->PathingDistance(unit_to_build, position) < 0.1f)
    {
        return false;
    }
    if (!is_expansion)
    {
        for (const auto& expansion : expansions)
        {
            if (Distance2D(position, Point2D(expansion.x, expansion.y)) < 7)
            {
                return false;
            }
        }
    }

    if (Query()->Placement(ability_type_for_structure, position))
    {
        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            position);
        return true;
    }

    return false;

}

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D pylon, float radius, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }

    if (!unit_to_build)
        return false;

    if (Query()->Placement(ability_type_for_structure, pylon))
    {
        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            pylon);

        return true;
    }

    return false;
}

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }

    if (!unit_to_build)
        return false;

    if (Query()->Placement(ability_type_for_structure, target->pos))
    {
        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            target);

        return true;
    }

    return false;
}

//Try to build a structure based on tag, Used mostly for Vespene, since the pathing check will fail even though the geyser is "Pathable"
bool GoingMerry::TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag) {
    const ObservationInterface* observation = Observation();
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
    const Unit* target = observation->GetUnit(location_tag);

    if (workers.empty()) {
        return false;
    }

    // Check to see if there is already a worker heading out to build it
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
    }

    // If no worker is already building one, get a random worker to build one
    const Unit* unit = GetRandomEntry(workers);

    // Check to see if unit can build there
    if (Query()->Placement(ability_type_for_structure, target->pos)) {
        Actions()->UnitCommand(unit, ability_type_for_structure, target);
        return true;
    }
    return false;

}

//Try build structure given a location. This is used most of the time
// Used by TryBuildStructureNearPylon
bool GoingMerry::TryBuildStructureForPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool isExpansion) {

    const ObservationInterface* observation = Observation();
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));

    //if we have no workers Don't build
    if (workers.empty()) {
        return false;
    }

    // Check to see if there is already a worker heading out to build it
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
    }

    // If no worker is already building one, get a random worker to build one
    const Unit* unit = GetRandomEntry(workers);

    // Check to see if unit can make it there
    if (Query()->PathingDistance(unit, location) < 0.1f) {
        return false;
    }
    if (!isExpansion) {
        for (const auto& expansion : expansions) {
            if (Distance2D(location, Point2D(expansion.x, expansion.y)) < 7) {
                return false;
            }
        }
    }
    // Check to see if unit can build there
    if (Query()->Placement(ability_type_for_structure, location)) {
        Actions()->UnitCommand(unit, ability_type_for_structure, location);
        return true;
    }
    return false;

}

bool GoingMerry::TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID) {
    const ObservationInterface* observation = Observation();

    //Need to check to make sure its a pylon instead of a warp prism
    std::vector<PowerSource> power_sources = observation->GetPowerSources();
    if (power_sources.empty()) {
        return false;
    }

    const PowerSource& random_power_source = GetRandomEntry(power_sources);
    if (observation->GetUnit(random_power_source.tag) != nullptr) {
        if (observation->GetUnit(random_power_source.tag)->unit_type == UNIT_TYPEID::PROTOSS_WARPPRISM) {
            return false;
        }
    }
    else {
        return false;
    }
    float radius = random_power_source.radius;
    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D build_location = Point2D(random_power_source.position.x + rx * radius, random_power_source.position.y + ry * radius);
    return TryBuildStructureForPylon(ability_type_for_structure, UNIT_TYPEID::PROTOSS_PROBE, build_location);
}

#pragma endregion


#pragma region Assistant Functions

bool GoingMerry::TryBuildProbe()
{
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

    if (observation->GetFoodWorkers() >= ideal_worker_count)
    {
        return false;
    }

    if (observation->GetFoodUsed() >= observation->GetFoodCap())
    {
        return false;
    }

    for (const auto& base : bases)
    {
        if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1)
        {
            if (observation->GetMinerals() >= 50)
            {
                return TryBuildUnit(ABILITY_ID::TRAIN_PROBE, UNIT_TYPEID::PROTOSS_NEXUS);
            }
        }
    }

    return false;
}

//An estimate of how many workers we should have based on what buildings we have
int GoingMerry::GetExpectedWorkers(UNIT_TYPEID vespene_building_type) {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(vespene_building_type));
    int expected_workers = 0;
    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            continue;
        }
        expected_workers += base->ideal_harvesters;
    }

    for (const auto& geyser : geysers) {
        if (geyser->vespene_contents > 0) {
            if (geyser->build_progress != 1) {
                continue;
            }
            expected_workers += geyser->ideal_harvesters;
        }
    }

    return expected_workers;
}

// Mine the nearest mineral to Town hall.
// If we don't do this, probes may mine from other patches if they stray too far from the base after building.
void GoingMerry::MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type) {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(vespene_building_type));

    const Unit* valid_mineral_patch = nullptr;

    if (bases.empty()) {
        return;
    }

    for (const auto& geyser : geysers) {
        if (geyser->assigned_harvesters < geyser->ideal_harvesters) {
            Actions()->UnitCommand(worker, worker_gather_command, geyser);
            return;
        }
    }

    //Search for a base that is missing workers.
    for (const auto& base : bases) {
        //If we have already mined out here skip the base.
        if (base->ideal_harvesters == 0 || base->build_progress != 1) {
            continue;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            valid_mineral_patch = FindNearestMineralPatch(base->pos);
            Actions()->UnitCommand(worker, worker_gather_command, valid_mineral_patch);
            return;
        }
    }

    if (!worker->orders.empty()) {
        return;
    }

    //If all workers spots are filled just go to any base.
    const Unit* random_base = GetRandomEntry(bases);
    valid_mineral_patch = FindNearestMineralPatch(random_base->pos);
    Actions()->UnitCommand(worker, worker_gather_command, valid_mineral_patch);
}

// To ensure that we do not over or under saturate any base.
void GoingMerry::ManageWorkers(UNIT_TYPEID worker_type, AbilityID worker_gather_command, UNIT_TYPEID vespene_building_type) {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(vespene_building_type));

    if (bases.empty()) {
        return;
    }

    for (const auto& base : bases) {
        //If we have already mined out or still building here skip the base.
        if (base->ideal_harvesters == 0 || base->build_progress != 1) {
            continue;
        }
        //if base is
        if (base->assigned_harvesters > base->ideal_harvesters) {
            Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(worker_type));

            for (const auto& worker : workers) {
                if (!worker->orders.empty()) {
                    if (worker->orders.front().target_unit_tag == base->tag) {
                        //This should allow them to be picked up by mineidleworkers()
                        MineIdleWorkers(worker, worker_gather_command,vespene_building_type);
                        return;
                    }
                }
            }
        }
    }
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(worker_type));
    for (const auto& geyser : geysers) {
        if (geyser->ideal_harvesters == 0 || geyser->build_progress != 1) {
            continue;
        }
        if (geyser->assigned_harvesters > geyser->ideal_harvesters) {
            for (const auto& worker : workers) {
                if (!worker->orders.empty()) {
                    if (worker->orders.front().target_unit_tag == geyser->tag) {
                        //This should allow them to be picked up by mineidleworkers()
                        MineIdleWorkers(worker, worker_gather_command, vespene_building_type);
                        return;
                    }
                }
            }
        }
        else if (geyser->assigned_harvesters < geyser->ideal_harvesters) {
            for (const auto& worker : workers) {
                if (!worker->orders.empty()) {
                    //This should move a worker that isn't mining gas to gas
                    const Unit* target = observation->GetUnit(worker->orders.front().target_unit_tag);
                    if (target == nullptr) {
                        continue;
                    }
                    if (target->unit_type != vespene_building_type) {
                        //This should allow them to be picked up by mineidleworkers()
                        MineIdleWorkers(worker, worker_gather_command, vespene_building_type);
                        return;
                    }
                }
            }
        }
    }
}


size_t GoingMerry::CountUnitType(UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
}

size_t GoingMerry::CountEnemyUnitType(UNIT_TYPEID unit_type)
{
    return Observation()->GetUnits(Unit::Alliance::Enemy, IsUnit(unit_type)).size();
}

const Unit* GoingMerry::FindNearestMineralPatch(const Point2D & start){
    Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
    float distance = std::numeric_limits<float>::max();
    const Unit* target = nullptr;
    for (const auto& u : units) {
        if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
            float d = DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
    }
    //If we never found one return false;
    if (distance == std::numeric_limits<float>::max()) {
        return target;
    }
    return target;
}

Point2D GoingMerry::FindClostest(Point2D pos, vector<Point2D> position)
{
    Point2D clos = GetRandomEntry(position);
    for (auto pos : position)
    {
        if (Distance2D(pos, pos) < Distance2D(pos,clos))
        {
            clos = pos;
        }
    }
    return clos;
}

bool GoingMerry::HavePylonNearby(Point2D& point)
{
    //Units pylons = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON));

    vector<PowerSource> pylons = observation->GetPowerSources();

    if (pylons.size() == 0)
        return false;

    for (const auto pylon : pylons)
    {
        //if (observation->GetUnit(pylon.tag)->unit_type != UNIT_TYPEID::PROTOSS_PYLON)
        //    continue;
        if (Distance2D(pylon.position, point) <= pylon.radius)
        {
            return true;
        }
    }
    return false;
}

vector<Point2D> GoingMerry::GetOffSetPoints(Point2D point, UNIT_TYPEID unit_type) {
    vector<Point2D> offSetPoints;
    Point3D startLocation = observation->GetStartLocation();
    
    for (int i = 0; i < 8; i++)
    {
        Point2D temp = Point2D(point.x + directionX[i], point.y + directionY[i]);

        if (observation->IsPlacable(temp) && startLocation.z - observation->TerrainHeight(temp) < 0.5) {
            offSetPoints.push_back(temp);
        }
    }

    return offSetPoints;
}

bool GoingMerry::HaveCannonNearby(Point2D& point)
{
    Units cannons = observation->GetUnits(Unit::Alliance::Self,IsUnit(UNIT_TYPEID::PROTOSS_PHOTONCANNON));

    if (cannons.size() == 0)
        return false;

    for (auto cannon : cannons)
    {
        //if (observation->GetUnit(pylon.tag)->unit_type != UNIT_TYPEID::PROTOSS_PYLON)
        //    continue;
        if (Distance2D(cannon->pos, point) <= 11)
        {
            return true;
        }
    }
    return false;
}

#pragma endregion


#pragma region Build Structure Functions

bool GoingMerry::TryBuildForge() {
    //const ObservationInterface* observation = Observation();
    if (CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) < 1)
    {
        return false;
    }

    if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) > 0) {
        return false;
    }

    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_FORGE, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildAssimilator()
{
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

    // Max assimilators already
    if (CountUnitType(UNIT_TYPEID::PROTOSS_ASSIMILATOR) >= observation->GetUnits(Unit::Alliance::Self, IsTownHall()).size() * 2) {
        return false;
    }
    
    // Per base, check if oversaturated mineral workers then build gas
    for (const auto& base : bases) {
//        if (base->assigned_harvesters >= base->ideal_harvesters) {
            if (base->build_progress == 1) {
                if (TryBuildGas(ABILITY_ID::BUILD_ASSIMILATOR, UNIT_TYPEID::PROTOSS_PROBE, base->pos)) {
                    return true;
                }
            }
//        }
    }
    return false;
}

//Tries to build a geyser for a base
bool GoingMerry::TryBuildGas(AbilityID build_ability, UnitTypeID worker_type, Point2D base_location) {
    const ObservationInterface* observation = Observation();
    Units geysers = observation->GetUnits(Unit::Alliance::Neutral, IsGeyser());

    //only search within this radius
    float minimum_distance = 15.0f;
    Tag closestGeyser = 0;
    for (const auto& geyser : geysers) {
        float current_distance = Distance2D(base_location, geyser->pos);
        if (current_distance < minimum_distance) {
            if (Query()->Placement(build_ability, geyser->pos)) {
                minimum_distance = current_distance;
                closestGeyser = geyser->tag;
            }
        }
    }

    // In the case where there are no more available geysers nearby
    if (closestGeyser == 0) {
        return false;
    }
    return TryBuildStructure(build_ability, worker_type, closestGeyser);

}

bool GoingMerry::TryBuildPylon() {
    const ObservationInterface* observation = Observation();
    // 25 pylons max
    Units pylons = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON));

    if(pylons.size() > 24){
        return false;
    }

    if (observation->GetMinerals() < 100) {
        return false;
    }

    //check to see if there is already one building
    Units units = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON));
    if (observation->GetFoodUsed() < 40) {
        for (const auto& unit : units) {
            if (unit->build_progress != 1) {
                return false;
            }
        }
    }

    // Try and build a pylon. Find a random Probe and give it the order.
    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D build_location = Point2D(staging_location.x + rx * 15, staging_location.y + ry * 15);

    return TryBuildStructure(ABILITY_ID::BUILD_PYLON, UNIT_TYPEID::PROTOSS_PROBE, build_location);
}

bool GoingMerry::TryBuildCyberneticsCore()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) < 1)
    {
        return false;
    }
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_CYBERNETICSCORE, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildFleetBeacon()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE) < 1)
    {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::PROTOSS_FLEETBEACON) > 0)
    {
        return false;
    }
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_FLEETBEACON, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildGateway()
{
    size_t num_gateway = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t num_warpgate = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t num_base = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);

    if ((num_gateway + num_warpgate) < ((num_base * 3) + 1))
    {
        if (!warpgate_researched)
        {
            return TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, UNIT_TYPEID::PROTOSS_PROBE);
        }
        else
        {
            if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, UNIT_TYPEID::PROTOSS_PROBE))
            {
                TryBuildWarpGate();
                return true;
            }
        }
    }

    return false;
}

bool GoingMerry::TryBuildRoboticsFacility()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) < 1)
    {
        return false;
    }
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_ROBOTICSFACILITY, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildRoboticsBay()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) < 1)
    {
        return false;
    }
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_ROBOTICSBAY, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildStargate()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) < 1)
    {
        return false;
    }

    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_STARGATE, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildTemplarArchives()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL) < 1)
    {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE) > 0)
    {
        return false;
    }
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_TEMPLARARCHIVE, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildTwilightCouncil()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) < 1)
    {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL) > 0)
    {
        return false;
    }
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_TWILIGHTCOUNCIL, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildExpansionNexus()
{
    const ObservationInterface* observation = Observation();

    const Units bases = observation->GetUnits(Unit::Alliance::Self,IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
    const Units assimilators = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));
    size_t cybernetics_count = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t bay_count = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    size_t fleet_count = CountUnitType(UNIT_TYPEID::PROTOSS_FLEETBEACON);

    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_NEXUS) {
                return false;
            }
        }
    }

    if(bases.size() == 1 && cybernetics_count < 1){
        return false;
    }
    
    if(bases.size() == 2 && bay_count < 1){
        return false;
    }

    //Don't have more active bases than we can provide workers for
    if (GetExpectedWorkers(UNIT_TYPEID::PROTOSS_ASSIMILATOR) > max_worker_count_) {
        return false;
    }
    // If we have extra workers around, try and build another nexus.
    if (GetExpectedWorkers(UNIT_TYPEID::PROTOSS_ASSIMILATOR) < observation->GetFoodWorkers() - 16) {
        return TryExpandBase(ABILITY_ID::BUILD_NEXUS, UNIT_TYPEID::PROTOSS_PROBE);
    }
    //Only build another nexus if we are floating extra minerals
    if (observation->GetMinerals() > CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS) * 400) {
        return TryExpandBase(ABILITY_ID::BUILD_NEXUS, UNIT_TYPEID::PROTOSS_PROBE);
    }

    return false;
}

bool GoingMerry::TryExpandBase(ABILITY_ID build_ability, UnitTypeID unit_type)
{
    const ObservationInterface* observation = Observation();

    float min_dist = numeric_limits<float>::max();
    Point3D closest_expansion;

    for (const auto &expansion : expansions)
    {
        float cur_dist = Distance2D(start_location, expansion);
        if (cur_dist < .01f)
        {
            continue;
        }

        if (cur_dist < min_dist)
        {
            if (Query()->Placement(build_ability, expansion))
            {
                closest_expansion = expansion;
                min_dist = cur_dist;
            }
        }
    }

    if (TryBuildStructure(build_ability, closest_expansion, unit_type, true) && observation->GetUnits(Unit::Self, IsTownHall()).size() < 4)
    {
        staging_location = Point3D(((staging_location.x + closest_expansion.x) / 2), ((staging_location.y + closest_expansion.y) / 2), ((staging_location.z + closest_expansion.z) / 2));
        return true;
    }

    return false;
}

bool GoingMerry::TryBuildUnit(AbilityID ability_type_for_unit, UnitTypeID unit_type) {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

    //If we are at supply cap, don't build anymore units, unless its an overlord.
    if (observation->GetFoodUsed() >= observation->GetFoodCap() && ability_type_for_unit != ABILITY_ID::TRAIN_OVERLORD) {
        return false;
    }

    if (unit_type == UNIT_TYPEID::PROTOSS_GATEWAY)
    {
        Units gateways = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));

        for (const auto& gateway : gateways)
        {
            if (!gateway->orders.empty())
            {
                continue;
            }
            else if (gateway->build_progress != 1)
            {
                continue;
            }
            else
            {
                Actions()->UnitCommand(gateway, ability_type_for_unit);
                return true;
            }
        }

        return false;
    }

    else
    {
        const Unit* unit = nullptr;

        if (!GetRandomUnit(unit, observation, unit_type)) {
            return false;
        }
        if (!unit->orders.empty()) {
            return false;
        }
        if (unit->build_progress != 1) {
            return false;
        }

        Actions()->UnitCommand(unit, ability_type_for_unit);

        if ((unit->unit_type == UNIT_TYPEID::PROTOSS_COLOSSUS && CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS) < 2) || (unit->unit_type == UNIT_TYPEID::PROTOSS_IMMORTAL && CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL) < 4))
        {
            Units facilities = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY));
            // chronoboost robotics facility
            for (const auto& facility : facilities) {
                bool chronoActive = false;
                if (!facility->orders.empty()) {
                    for (const auto& order : facility->orders)
                    {
                        if (order.ability_id == ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST)
                        {
                            chronoActive = true;
                        }
                    }
                    if (!chronoActive)
                    {
                        Actions()->UnitCommand(bases.back(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, facility);
                    }
                }
            }
        }

        else if ((unit->unit_type == UNIT_TYPEID::PROTOSS_VOIDRAY && CountUnitType(UNIT_TYPEID::PROTOSS_VOIDRAY) < 2))
        {
            Units stargates = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_STARGATE));
            // chronoboost robotics facility
            for (const auto& stargate : stargates) {
                bool chronoActive = false;
                if (!stargate->orders.empty()) {
                    for (const auto& order : stargate->orders)
                    {
                        if (order.ability_id == ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST)
                        {
                            chronoActive = true;
                        }
                    }
                    if (!chronoActive)
                    {
                        Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, stargate);
                    }
                }
            }
        }

        return true;
    }


    return false;
}

bool GoingMerry::TryWarpInUnit(ABILITY_ID ability_type_for_unit) {
    const ObservationInterface* observation = Observation();
    std::vector<PowerSource> power_sources = observation->GetPowerSources();
    Units warpgates = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));

    if (power_sources.empty()) {
        return false;
    }

    const PowerSource& random_power_source = GetRandomEntry(power_sources);
    float radius = random_power_source.radius;
    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D build_location = Point2D(random_power_source.position.x + rx * radius, random_power_source.position.y + ry * radius);

    // If the warp location is walled off, don't warp there.
    // We check this to see if there is pathing from the build location to the center of the map
    if (Query()->PathingDistance(build_location, Point2D(game_info.playable_max.x / 2, game_info.playable_max.y / 2)) < .01f) {
        return false;
    }

    for (const auto& warpgate : warpgates) {
        if (warpgate->build_progress == 1) {
            AvailableAbilities abilities = Query()->GetAbilitiesForUnit(warpgate);
            for (const auto& ability : abilities.abilities) {
                if (ability.ability_id == ability_type_for_unit) {
                    Actions()->UnitCommand(warpgate, ability_type_for_unit, build_location);
                    return true;
                }
            }
        }
    }
    return false;
}

#pragma endregion


#pragma region Try Update Tech/Structures

bool GoingMerry::TryBuildWarpGate()
{
    const ObservationInterface* observation = Observation();

    bool gateway_morphed = false;
    Units gateways = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));

    if (warpgate_researched)
    {
        for (const auto& gateway : gateways)
        {
            if (gateway->build_progress == 1)
            {
                Actions()->UnitCommand(gateway, ABILITY_ID::MORPH_WARPGATE);
                gateway_morphed = true;
            }
        }
    }

    return gateway_morphed;
}

void GoingMerry::ManageUpgrades()
{
    const ObservationInterface* observation = Observation();
    auto upgrades = observation->GetUpgrades();
    size_t n_base = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);  // prioritize maintaining number of bases over upgrades
    size_t stargate_count = CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE);

    // Beginning of upgrades
    if (upgrades.empty())
    {
        TryBuildUnit(ABILITY_ID::RESEARCH_WARPGATE, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
        OnUpgradeCompleted(UPGRADE_ID::WARPGATERESEARCH);
    }

    else
    {
        for (const auto& upgrade : upgrades)
        {
            if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1 && n_base > 2)                        // lv 1 weapons
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
                OnUpgradeCompleted(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1 && n_base > 2)                    // lv 1 armor
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2 && n_base > 2)                   // lv 2 weapons
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
                OnUpgradeCompleted(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2);

            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2 && n_base > 2)                    // lv 2 armor
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else
            {
                // ROBOTICS UPGRADES
                TryBuildUnit(ABILITY_ID::RESEARCH_EXTENDEDTHERMALLANCE, UNIT_TYPEID::PROTOSS_ROBOTICSBAY);  // For Colossus units
                OnUpgradeCompleted(UPGRADE_ID::EXTENDEDTHERMALLANCE);
                
                // GATEWAY UPGRADES
                TryBuildUnit(ABILITY_ID::RESEARCH_BLINK, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);  // For Stalker units
                OnUpgradeCompleted(UPGRADE_ID::BLINKTECH);

                TryBuildUnit(ABILITY_ID::RESEARCH_CHARGE, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);  // For Zealot units (if using)
                OnUpgradeCompleted(UPGRADE_ID::CHARGE);
                
                // GROUND UPGRADES
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
                if(ground_wep_2_researched){
                    TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSSHIELDS, UNIT_TYPEID::PROTOSS_FORGE);
                }
                
                
                // AIR UPGRADES
                if(stargate_count > 0){
                    TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSAIRWEAPONS, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
                    TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSAIRARMOR, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
                    TryBuildUnit(ABILITY_ID::RESEARCH_PHOENIXANIONPULSECRYSTALS, UNIT_TYPEID::PROTOSS_FLEETBEACON);
                    TryBuildUnit(ABILITY_ID::RESEARCH_VOIDRAYSPEEDUPGRADE, UNIT_TYPEID::PROTOSS_FLEETBEACON);
                }
            }
        }
        
        // Use chronoboost on upgrades
        Units cores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE));
        Units forges = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_FORGE));
        Units bays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSBAY));
        Units twilights = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL));
        Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
        
        for(const auto& core: cores){
            for(const auto& base : bases){
                if(!core->orders.empty() && base->energy >= 25){
                    Actions()->UnitCommand(base, ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, core);
                    break;
                }
            }
        }
        
        for(const auto& forge: forges){
            for(const auto& base : bases){
                if(!forge->orders.empty() && base->energy >= 25){
                    Actions()->UnitCommand(base, ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, forge);
                    break;
                }
            }
        }
        
        for(const auto& bay: bays){
            for(const auto& base : bases){
                if(!bay->orders.empty() && base->energy >= 25){
                    Actions()->UnitCommand(base, ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, bay);
                    break;
                }
            }
        }
        
        for(const auto& twilight: twilights){
            for(const auto& base : bases){
                if(!twilight->orders.empty() && base->energy >= 25){
                    Actions()->UnitCommand(base, ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, twilight);
                    break;
                }
            }
        }
    }

}

#pragma endregion


#pragma region Try Build Defence Structures

bool GoingMerry::TryBuildPhotonCannon()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) < 1)
       return false;

    Units nux = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
    if (nux.size() == 0)
        return false;

    // Get ramp position
    auto position = CalculatePlacableRamp(nux.front());

    if (position.size() != 0)
    {
        for (auto pos : position)
        {
            if (HaveCannonNearby(pos))
                continue;

            if (HavePylonNearby(pos))
            {
                if (TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, pos))
                {
                    return true;
                }
            }
            else
            {
                // Build pylon
                auto closet = FindClostest(nux.front()->pos, position);
                TryBuildStructure(ABILITY_ID::BUILD_PYLON, closet);
            }
        }
    }   

    // Get nearby grids
    vector<Point2D> grids = CalculateGrid(start_location, 18);

    Point2D farestEnemyBase = GetRandomEntry(game_info.enemy_start_locations);
    for (auto temp : game_info.enemy_start_locations)
    {
        if (Distance2D(start_location, temp) >= Distance2D(start_location, farestEnemyBase))
        {
            farestEnemyBase = temp;
        }
    }

    // Check grids for valid position
    for (auto grid : grids)
    {
        if (HaveCannonNearby(grid))
            continue;

        if (Distance2D(grid, farestEnemyBase) > Distance2D(start_location, farestEnemyBase))
            continue;

        if (observation->IsPlacable(grid) && IsNextToCliff(grid))
        {
            vector<Point2D> points = GetOffSetPoints(grid, UNIT_TYPEID::PROTOSS_PHOTONCANNON);

            for (auto point : points)
            {
                if (!HavePylonNearby(point))
                {
                    auto closest = FindClostest(start_location, points);
                    TryBuildStructure(ABILITY_ID::BUILD_PYLON, closest);
                }

                if (TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, point))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

bool GoingMerry::TryBuildShieldBattery()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_PHOTONCANNON) < 1)
        return false;
    return true;
}

#pragma endregion


#pragma region Get Functions

Point2D GoingMerry::GetRandomMapLocation()
{
    const sc2::GameInfo& game_info = Observation()->GetGameInfo();

    // Define boundaries for the random location
    float minX = game_info.playable_min.x;
    float minY = game_info.playable_min.y;
    float maxX = game_info.playable_max.x;
    float maxY = game_info.playable_max.y;

    // Generate random coordinates within the boundaries
    float randomX = sc2::GetRandomInteger(minX, maxX - 1) + sc2::GetRandomFraction();
    float randomY = sc2::GetRandomInteger(minY, maxY - 1) + sc2::GetRandomFraction();

    return sc2::Point2D(randomX, randomY);
}

bool GoingMerry::GetRandomUnit(const Unit*& unit_out, const ObservationInterface* observation, UnitTypeID unit_type) {
    Units my_units = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
    if (!my_units.empty()) {
        unit_out = GetRandomEntry(my_units);
        return true;
    }
    return false;
}

#pragma endregion


#pragma region Try Send Scouting and Harassers

sc2::Point2D GoingMerry::GetScoutMoveLocation() 
{
    const sc2::GameInfo& game_info = Observation()->GetGameInfo();

    Point2D target_location;
    
    // Check if the target_location is not in the list of previously visited locations
    if (!foundBase && !game_info.enemy_start_locations.empty() && possible_starts_visited < game_info.enemy_start_locations.size())
    {
        for (const auto& location : game_info.enemy_start_locations)
        {
            if ((std::find(visitedLocations.begin(), visitedLocations.end(), location) == visitedLocations.end()))
            {
                target_location = location;
                // Update the list of visited locations
                visitedLocations.push_back(target_location);
                possible_starts_visited += 1;
                return target_location;
            }
        }
    }
    else
    {
        // Define boundaries for the random location
        float minX = game_info.playable_min.x;
        float minY = game_info.playable_min.y;
        float maxX = game_info.playable_max.x;
        float maxY = game_info.playable_max.y;

        // Our base
        const sc2::Units& base = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));

        // Get all mineral fields
        sc2::Units minerals = Observation()->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_MINERALFIELD));

        // Set maximum attempts to avoid an infinite loop
        const int maxAttempts = 10;
        int attemptCount = 0;

        // Generate random coordinates near mineral fields
        Point2D exact_location;
        float distFromScout = 0;
        float distFromBase = 0;

        while (distFromScout < 5.0f && distFromBase < 10.0f && attemptCount < maxAttempts)
        {
            if (minerals.empty())
            {
                // If no mineral fields are found, revert to the original random location logic
                float randomX = sc2::GetRandomInteger(minX, maxX - 1) + sc2::GetRandomFraction();
                float randomY = sc2::GetRandomInteger(minY, maxY - 1) + sc2::GetRandomFraction();
                target_location = sc2::Point2D(randomX, randomY);
                exact_location = target_location;
            }
            else
            {
                // Choose a random mineral field
                const sc2::Unit*& randomMineral = sc2::GetRandomEntry(minerals);

                // Generate random coordinates near the chosen mineral field
                float offsetRadius = 5.0f;  // You can adjust this radius as needed
                float randomOffsetX = sc2::GetRandomInteger(-offsetRadius, offsetRadius) + sc2::GetRandomFraction();
                float randomOffsetY = sc2::GetRandomInteger(-offsetRadius, offsetRadius) + sc2::GetRandomFraction();

                target_location = randomMineral->pos + sc2::Point2D(randomOffsetX, randomOffsetY);
                exact_location = randomMineral->pos;
            }

            // Check if the target_location is not in the list of previously visited locations
            if (std::find(visitedLocations.begin(), visitedLocations.end(), exact_location) == visitedLocations.end())
            {
                // Update the list of visited locations
                visitedLocations.push_back(exact_location);

                // Calculate distances
                distFromScout = Distance2D(scouts[0]->pos, target_location);
                distFromBase = Distance2D(base[0]->pos, target_location);
            }

            attemptCount++;
        }

        // Clear the list if it becomes too large to avoid excessive memory usage
        if ((visitedLocations.size() > 100) || (visitedLocations.size() >= minerals.size() && minerals.size() > 0))
        {
            visitedLocations.clear();
        }
    }

    return target_location;
}

void GoingMerry::MoveScouts()
{
    Point2D target_location = GetScoutMoveLocation(); // get location to send scouts to 
    for (int i = 0; i < scouts.size(); i++)
    {
        if (scouts[i]->orders.empty())
        {
            Actions()->UnitCommand(scouts[i], ABILITY_ID::GENERAL_MOVE, target_location);
        }
        else if (!scouts[i]->orders.empty())
        {
            if (scouts[i]->orders.front().ability_id != ABILITY_ID::GENERAL_MOVE) 
            {
                Actions()->UnitCommand(scouts[i], ABILITY_ID::GENERAL_MOVE, target_location);
            }
        }

    }
}

void GoingMerry::SendScouting()
{
    MoveScouts(); // move scouts to new location
    const sc2::Units& cur_enemy_units = Observation()->GetUnits(sc2::Unit::Alliance::Enemy, IsArmy(observation)); // check if any enemies are found
    bool found = false;
    const sc2::Unit *base;

    // add unit to either enemy_bases vector or enemy_units vector based on whether or not its the first time encountering it
    for (const auto &cur : cur_enemy_units)
    {
        found = false;
        if (!(cur->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
            cur->unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
            cur->unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY))
        {
            if (cur->is_flying)
            {
                enemy_air_units = true;
            }
            for (auto seen : enemy_units)
            {
                if (seen->tag == cur->tag)
                {
                    found = true;
                }
            }
            if (!found)
            {
                enemy_units.push_back(cur);
            }
        }
        else
        {
            foundBase = true;
        }
    }
}

void GoingMerry::TrySendHarassing(const sc2::Unit *base)
{    
    // checking if a base is already found -> send harassing if conditions are met
    if (harassers.size() > 0)
    {
        CheckIfAlive(1);
    }
    if (harassers.size() < num_harassers)
    {
        if (CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT) >= (num_harassers - harassers.size()))
        {
            const sc2::Units& zealots = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT));
            for (int i = 0; i < (num_harassers - harassers.size()); i++)
            {
                harassers.push_back(zealots[i]);
            }
        }
        else
        {
            return;
        }
    }

    bool shieldsUp = true;
    
    for (const auto& unit : harassers)
    {
        if (unit->shield < (unit->shield_max - 10))
        {
            shieldsUp = false;
        }
    }

    observation = Observation();

    Units army = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));
    
    if ((harassers.size() == num_harassers) && shieldsUp)
    {
        SendHarassing(base);
    }
}

void GoingMerry::SendHarassing(const sc2::Unit *base)
{
    // send to harass based on the enemy base location
    for (int i = 0; i < harassers.size(); i++)
    {
        if (harassers[i]->orders.empty())
        {
            Actions()->UnitCommand(harassers[i], ABILITY_ID::ATTACK_ATTACK, base->pos);
            if (charge_researched)
            {
                Actions()->UnitCommand(harassers[i], ABILITY_ID::EFFECT_CHARGE, base->pos);
            }
        }
        else if (!harassers[i]->orders.empty())
        {
            if (harassers[i]->orders.front().ability_id != ABILITY_ID::ATTACK_ATTACK && harassers[i]->orders.front().ability_id != ABILITY_ID::GENERAL_MOVE)
            {
                Actions()->UnitCommand(harassers[i], ABILITY_ID::ATTACK_ATTACK, base->pos);
                if (charge_researched)
                {
                    Actions()->UnitCommand(harassers[i], ABILITY_ID::EFFECT_CHARGE, base->pos);
                }
            }
        }
    }
}

void GoingMerry::CheckIfAlive(int idetifier)
{
    if (idetifier == 0)
    {
        for (auto it = scouts.begin(); it != scouts.end();) // check each scout to see if alive
        {
            if (!(*it)->is_alive)
            {
                it = scouts.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    else
    {
        for (auto it = harassers.begin(); it != harassers.end();) // check each scout to see if alive
        {
            if (!(*it)->is_alive)
            {
                it = harassers.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void GoingMerry::TrySendScouts()
{    
    // checking if a base is already found -> send harassing if conditions are met
    if (scouts.size() > 0)
    {
        CheckIfAlive(0);
    }
    if (scouts.size() < num_scouts)
    {
        int type = 0;
        if (CountUnitType(UNIT_TYPEID::PROTOSS_STALKER) >= (num_scouts - scouts.size()))
        {
            const sc2::Units& stalkers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_STALKER));
            for (int i = 0; i < (num_scouts - scouts.size()); i++)
            {
                scouts.push_back(stalkers[i]);
            }
        }
        else
        {
            return;
        }
    }

    if (scouts.size() == num_scouts) // if a pair of scouts available send to harass or scout
    {
        SendScouting();
    }
}

void GoingMerry::FindEnemyRace()
{
    if (!enemy_units.empty())
    {
        for (const auto& unit : enemy_units)
        {
            if (unit->unit_type.to_string().find("ZERG"))
            {
                enemy_race = 2;
                break;
            }
            else if (unit->unit_type.to_string().find("TERRAN"))
            {
                enemy_race = 1;
                break;
            }
            else if (unit->unit_type.to_string().find("PROTOSS"))
            {
                enemy_race = 0;
                break;
            }
        }
    }
}

#pragma endregion


#pragma region Build Orders

void GoingMerry::BuildOrder(float ingame_time, uint32_t current_supply, uint32_t current_minerals, uint32_t current_gas)
{
    
    // STRUCTURE/UNIT COUNTS
    size_t pylon_count = CountUnitType(UNIT_TYPEID::PROTOSS_PYLON);
    size_t gateway_count = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t warpgate_count = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t assimilator_count = CountUnitType(UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    size_t cybernetics_count = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t base_count = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t robotics_facility_count = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    size_t robotics_bay_count = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    size_t forge_count = CountUnitType(UNIT_TYPEID::PROTOSS_FORGE);
    size_t twilight_count = CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    size_t cannon_count = CountUnitType(UNIT_TYPEID::PROTOSS_PHOTONCANNON);
    size_t battery_count = CountUnitType(UNIT_TYPEID::PROTOSS_SHIELDBATTERY);
    size_t archive_count = CountUnitType(UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE);
    size_t stargate_count = CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE);
    size_t fleet_count = CountUnitType(UNIT_TYPEID::PROTOSS_FLEETBEACON);

    // UNITS
    Units cores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE));
    Units gateways = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
    Units warpgates = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units rfacs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY));
    Units rbays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSBAY));
    Units forges = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_FORGE));
    Units twilights = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL));

    //      14      0:20      Pylon
    if (pylon_count < 1 &&
        assimilator_count >= 0) {
        if (TryBuildPylon()) {
            printLog("PYLON 1");
        }
    }

    //      15      0:40      Gateway
    if (pylon_count >= 0 &&
        warpgate_count + gateway_count < 1) {
        if (TryBuildGateway()) {
            printLog("GATEWAY 1");
        }
    }

    //      16      0:48      Assimilator
    if (warpgate_count + gateway_count >= 1 &&
        assimilator_count < 1) {
        if (TryBuildAssimilator()) {
            printLog("ASSIMILATOR 1");
        }
    }

    //      20      1:28      Cybernetics Core
    if (warpgate_count + gateway_count >= 1 &&
        assimilator_count >= 1 &&
        cybernetics_count < 1) {
        if (TryBuildCyberneticsCore()) {
            printLog("CYBERNETICS 1");
        }
    }


    //      19      1:13      Gateway (x2), moving gateway from 2nd base here
    if (warpgate_count + gateway_count < 2 &&
        assimilator_count >= 1 &&
        cybernetics_count >= 1) {
        if (TryBuildGateway()) {
            printLog("GATEWAY 2");
        }
    }

    if (warpgate_count + gateway_count < 3 &&
        assimilator_count >= 1 &&
        cybernetics_count >= 1)
    {
        if (TryBuildGateway()) {
            printLog("GATEWAY 3");
        }
    }

    if (warpgate_count + gateway_count >= 3 &&
        assimilator_count < 2 && 
        cybernetics_count >= 1) {
        if (TryBuildAssimilator()) {
            printLog("ASSIMILATOR 2");
        }
    }

    //      23      2:02      Stalkers x2
    //      27      2:08      Warp Gate Research

    //       32      3:10      Robotics Facility
    if (cybernetics_count >= 1 &&
        (gateway_count + warpgate_count >= 3) &&
        base_count >= 1 &&
        assimilator_count >= 2 &&
        robotics_facility_count < 1) {

        if (TryBuildRoboticsFacility()) {
            printLog("ROBOTICS FACILITY 1");
        }
    }
    
    //      31      2:56      Nexus
    if(cybernetics_count >= 1 &&
       warpgate_count + gateway_count >= 3 &&
       base_count < 2 &&
       robotics_facility_count >= 1){
        if(current_minerals >= 400){
            if(TryBuildExpansionNexus()){
                printLog("EXPAND 1");
            }
        }
    }

//    //      32      3:48      Gateway

    //      34      4:00      Robotics Bay
    if(base_count >= 2 &&
       cybernetics_count >= 1 &&
        warpgate_count + gateway_count >= 3 &&
       robotics_facility_count >= 1 &&
       robotics_bay_count < 1 &&
       assimilator_count >= 2){
        if(TryBuildRoboticsBay()){
            printLog("ROBOTICS BAY 1");
        }
    }

    //      49      4:45      Assimilator
    if (base_count >= 2 &&
        cybernetics_count >= 1 &&
        warpgate_count + gateway_count >= 3 &&
        robotics_facility_count >= 1 &&
        robotics_bay_count >= 1 &&
        assimilator_count < 3) {

        if (TryBuildAssimilator()) {
            printLog("ASSIMILATOR 3");
        }
    }

    //      49      4:45      Gateway
    if (base_count >= 2 &&
        cybernetics_count >= 1 &&
        warpgate_count + gateway_count < 4 &&
        robotics_facility_count >= 1 &&
        robotics_bay_count >= 1 &&
        assimilator_count >= 3) {

        if (TryBuildGateway()) {
            printLog("GATEWAY 4");
        }
    }

    //      72      6:02      Forge
    if (base_count >= 2 &&
        cybernetics_count >= 1 &&
        warpgate_count + gateway_count >= 4 &&
        robotics_facility_count >= 1 &&
        robotics_bay_count >= 1 &&
        assimilator_count >= 3 &&
        forge_count < 1) {
        if (TryBuildForge()) {
            printLog("FORGE 1");
        }
    }

    //      62      5:35      Nexus
    if (base_count < 3 &&
        cybernetics_count >= 1 &&
        warpgate_count + gateway_count >= 4 &&
        robotics_facility_count >= 1 &&
        assimilator_count >= 3 &&
        robotics_bay_count >= 1 &&
        forge_count >= 1) {

        //      62      5:35      Nexus
        if (TryBuildExpansionNexus()) {
            printLog("EXPAND 2");
        }
    }

    if (base_count >= 3 &&
        cybernetics_count >= 1 &&
        warpgate_count + gateway_count >= 4 &&
        robotics_facility_count < 2 &&
        robotics_bay_count >= 1 &&
        assimilator_count >= 3 &&
        forge_count >= 1)
    {
        if (TryBuildRoboticsFacility())
        {
            printLog("ROBOTICS FACILITY 2");
        }
    }

    if (base_count >= 3 &&
        warpgate_count + gateway_count >= 4 &&
        assimilator_count >= 3 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 1 &&
        stargate_count < 1) {
        if (TryBuildStargate()) {
            printLog("STARGATE 1");
        }
    }

    // random shield batteries instead
    if (base_count >= 3 &&
        cybernetics_count >= 1 &&
        warpgate_count + gateway_count >= 4 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        assimilator_count >= 3 &&
        battery_count < 2 &&
        stargate_count >= 1 &&
        forge_count >= 1) {

        if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_SHIELDBATTERY, UNIT_TYPEID::PROTOSS_PROBE)) {
            printLog("BATTERY x2");
        }
    }

    //      49      4:45      Assimilator
    if (base_count >= 3 &&
        cybernetics_count >= 1 &&
        warpgate_count + gateway_count >= 4 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        assimilator_count < 4 &&
        stargate_count >= 1 &&
        forge_count >= 1) {

        if (TryBuildAssimilator()) {
            printLog("ASSIMILATOR 4");
        }
    }

    //      78      6:22      Gateway
    if (base_count >= 3 &&
        warpgate_count + gateway_count < 5 &&
        assimilator_count >= 4 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 1) {
        if (TryBuildGateway()) {
            printLog("GATEWAY 5");
        }
    }

    //      86      6:52      Twilight Council
    if(base_count >= 3 &&
       assimilator_count >= 4 &&
        warpgate_count + gateway_count >= 5 &&
       robotics_facility_count >= 2 &&
       robotics_bay_count >= 1 &&
       forge_count >= 1 &&
       twilight_count < 1){
        if(TryBuildTwilightCouncil()){
            printLog("TWILIGHT");
        }
    }

    if (base_count >= 3 &&
        assimilator_count >= 4 &&
        warpgate_count + gateway_count >= 5 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count < 2 &&
        twilight_count >= 1) {
        if (TryBuildForge()) {
            printLog("FORGE 2");
        }
    }

    //      108      7:31      Charge
    if (base_count >= 3 &&
        warpgate_count + gateway_count < 6 &&
        assimilator_count >= 4 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1) {
        if (TryBuildGateway()) {
            printLog("GATEWAY 6");
        }
        if (cannon_count < max_cannon_count) {
            if(TryBuildPhotonCannon()){
                printLog("CANON");
            }
            else if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE))
            {
                printLog("CANON");
            }
        }
    }

    if (base_count >= 3 &&
        warpgate_count + gateway_count >= 6 &&
        assimilator_count >= 4 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1 &&
        stargate_count < 2) {
        if (TryBuildStargate()) {
            printLog("STARGATE 2");
        }

        if (cannon_count < max_cannon_count) {
            if (TryBuildPhotonCannon()) {
                printLog("CANON");
            }
            else if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE))
            {
                printLog("CANON");
            }
        }
    }

    if (base_count >= 3 &&
        warpgate_count + gateway_count < 7 &&
        assimilator_count >= 4 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1 &&
        stargate_count >= 1) {
        if (TryBuildAssimilator()) {
            printLog("GATEWAY 7");
        }
        if (cannon_count < max_cannon_count) {
            if (TryBuildPhotonCannon()) {
                printLog("CANON");
            }
            else if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE))
            {
                printLog("CANON");
            }
        }
    }

    if (base_count >= 3 &&
        warpgate_count + gateway_count >= 7 &&
        assimilator_count < 5 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1 && 
        stargate_count >= 1) {
        if (TryBuildAssimilator()) {
            printLog("ASSIMILATOR 5");
        }
        if (cannon_count < max_cannon_count) {
            if(TryBuildPhotonCannon()){
                printLog("CANON");
            }
            else if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE))
            {
                printLog("CANON");
            }
        }
    }

    if (base_count >= 3 &&
        warpgate_count + gateway_count >= 7 &&
        assimilator_count >= 5 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1 &&
        stargate_count >= 1 &&
        fleet_count < 1) {
        if (TryBuildFleetBeacon()) {
            printLog("FLEET BEACON");
        }

        if (cannon_count < max_cannon_count) {
            if (TryBuildPhotonCannon()) {
                printLog("CANON");
            }
            else if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE))
            {
                printLog("CANON");
            }
        }
    }

    //      149      9:27      Gateway x3, Templar Archives
    if (base_count >= 3 &&
        warpgate_count + gateway_count >= 7 &&
        assimilator_count >= 5 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1 &&
        stargate_count >= 1 &&
        fleet_count >= 1 &&
        archive_count < 1) {
        if (TryBuildTemplarArchives())
        {
            printLog("TEMPLAR ARCHIVES");
        }
    }

    if (base_count >= 3 &&
        warpgate_count + gateway_count < 8 &&
        assimilator_count >= 5 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1 &&
        stargate_count >= 1 &&
        fleet_count >= 1 &&
        archive_count >= 1) {
        if (TryBuildGateway())
        {
            printLog("GATEWAY 8");
        }
    }

    //      133      8:38      Nexus
    if (base_count < 4 &&
        warpgate_count + gateway_count >= 7 &&
        assimilator_count >= 5 &&
        robotics_facility_count >= 2 &&
        robotics_bay_count >= 1 &&
        forge_count >= 2 &&
        twilight_count >= 1 &&
        stargate_count >= 1 &&
        fleet_count >= 1) {
        if (TryBuildExpansionNexus()) {
            printLog("EXPAND 3");
        }
    }

    // general build order after
    if (base_count >= 4 &&
        warpgate_count + gateway_count < (base_count * 3) &&
        robotics_facility_count < (base_count) &&
        assimilator_count < (base_count * 2) &&
        twilight_count >= 1 &&
        cannon_count < (base_count * 5) &&
        battery_count < (base_count * 2) &&
        stargate_count < (base_count)) {

        if (assimilator_count < (base_count * 2))
        {
            if (TryBuildAssimilator())
            {
                printLog("ASSIMILATOR EXTRA");
            }
        }
        if (warpgate_count + gateway_count < (base_count * 3)) {
            if (TryBuildGateway())
            {
                printLog("GATEWAY EXTRA");
            }
        }
        if (robotics_facility_count < base_count) {
            if (TryBuildRoboticsFacility())
            {
                printLog("ROBOTICS FACILITY EXTRA");
            }
        }
        if (stargate_count < base_count)
        {
            if (TryBuildStargate())
            {
                printLog("STARGATE EXTRA");
            }
        }
    }

    if (base_count >= 4 &&
        warpgate_count >= (base_count * 3) &&
        robotics_facility_count >= (base_count) &&
        assimilator_count >= (base_count * 2) &&
        twilight_count >= 1 &&
        cannon_count < (base_count * 5) &&
        battery_count < (base_count * 2)) {
        if (cannon_count < (base_count * 5)) {
            if (TryBuildPhotonCannon()) {
                //std::cout << "CANNON x5 7:58" << std::endl;
                printLog("CANON EXTRA");
            }
            else
            {
                TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE);
                printLog("CANON EXTRA");
            }
        }
        if (battery_count < (base_count * 2)) {
            TryBuildStructureNearPylon(ABILITY_ID::BUILD_SHIELDBATTERY, UNIT_TYPEID::PROTOSS_PROBE);
            printLog("BATTERY EXTRA");
        }

        if (cannon_count >= (base_count * 5) && battery_count >= (base_count * 2)) {
            if (TryBuildExpansionNexus()) {
                printLog("EXPAND EXTRA");
            }
        }
    }

    // END BUILD ORDER
}

#pragma endregion


#pragma region Manage Army

/// <summary>
/// Build Base army
/// </summary>
void GoingMerry::TryBuildBaseArmy()
{
    size_t zealot_count = CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT);
    size_t stalker_count = CountUnitType(UNIT_TYPEID::PROTOSS_STALKER);
    size_t sentry_count = CountUnitType(UNIT_TYPEID::PROTOSS_SENTRY);
    size_t immortal_count = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t colossus_count = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
    size_t voidray_count = CountUnitType(UNIT_TYPEID::PROTOSS_VOIDRAY);
    size_t rfacs_count = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    size_t rbay_count = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    size_t stargate_count = CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE);
    size_t warpgate_count = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);

    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units gateways = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));

    // Pause building too many units until 2 bases are expanded
    if (bases.size() < 2 && observation->GetFoodArmy() > 20)
    {
        return;
    }

    if (rbay_count > 0 && rfacs_count > 0 && colossus_count < 2)
    {
        TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    }
    if (rfacs_count > 0 && immortal_count < 4)
    {
        TryBuildUnit(ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    }
    if (stargate_count > 0 && voidray_count < 2)
    {  
        TryBuildUnit(ABILITY_ID::TRAIN_VOIDRAY, UNIT_TYPEID::PROTOSS_STARGATE);

    }
    if (zealot_count < num_harassers) {
        if (warpgate_count > 0)
        {
            TryWarpInUnit(ABILITY_ID::TRAINWARP_ZEALOT);
        }
        else
        {
            TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
        }
    }
    if (stalker_count < max_stalker_count) {
        if (warpgate_count > 0)
        {
            TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
        }
        else
        {
            TryBuildUnit(ABILITY_ID::TRAIN_STALKER, UNIT_TYPEID::PROTOSS_GATEWAY);
        }
    }
    if (sentry_count < max_sentry_count) {
        if (warpgate_count > 0)
        {
            TryWarpInUnit(ABILITY_ID::TRAINWARP_SENTRY);
        }
        else
        {
            TryBuildUnit(ABILITY_ID::TRAIN_SENTRY, UNIT_TYPEID::PROTOSS_GATEWAY);
        }
    }

    // chronoboost gateways
    for (const auto& gateway : gateways) {
        bool chronoActive = false;
        if (!gateway->orders.empty()) {
            for (const auto &order : gateway->orders)
            {
                if (order.ability_id == ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST)
                {
                    chronoActive = true;
                }
            }
            if (!chronoActive)
            {
                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, gateway);
            }
        }
    }
}

/// <summary>
/// Build army
/// </summary>
/// <returns></returns>
bool GoingMerry::TryBuildArmy()
{
    // Builds original fixed army composition
    const ObservationInterface* observation = Observation();

    // Get army unit counts
    size_t num_zealot = CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT);
    size_t num_adept = CountUnitType(UNIT_TYPEID::PROTOSS_ADEPT);
    size_t num_stalker = CountUnitType(UNIT_TYPEID::PROTOSS_STALKER);
    size_t num_immortal = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t num_colossus = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
    size_t num_voidray = CountUnitType(UNIT_TYPEID::PROTOSS_VOIDRAY);
    size_t num_phoenix = CountUnitType(UNIT_TYPEID::PROTOSS_PHOENIX);
    size_t num_hightemplar = CountUnitType(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR);
    size_t num_archon = CountUnitType(UNIT_TYPEID::PROTOSS_ARCHON);
    size_t num_observer = CountUnitType(UNIT_TYPEID::PROTOSS_OBSERVER);
    size_t num_sentry = CountUnitType(UNIT_TYPEID::PROTOSS_SENTRY);

    // Get production structure counts
    size_t num_bases = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t num_gateway = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t num_warpgate = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t num_cyberneticscore = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t num_roboticsfacility = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    size_t num_roboticsbay = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    size_t num_stargate = CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE);

    if (observation->GetFoodWorkers() <= target_worker_count)
    {
        return false;
    }

    // Pause building too many units until 2 bases are expanded
    if (num_bases < 2 && observation->GetFoodArmy() > 20)
    {
        return false;
    }

    //If we have a decent army already, try hold until we expand again
    if (num_bases < 3 && observation->GetFoodArmy() > 70) {
        return false;
    }

    // Merge templars to create Archons
    Units templar = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR));

    if (templar.size() > 1)
    {
        Units merge_templars;
        for (int i = 0; i < 2; ++i)
        {
            merge_templars.push_back(templar.at(i));
        }
        Actions()->UnitCommand(merge_templars, ABILITY_ID::MORPH_ARCHON);
    }

    if (observation->GetMinerals() > 250 && observation->GetVespene() > 150 && num_stargate > 0 && num_voidray < max_voidray_count) 
    {
        TryBuildUnit(ABILITY_ID::TRAIN_VOIDRAY, UNIT_TYPEID::PROTOSS_STARGATE);
    }
    if (num_phoenix < max_phoenix_count && num_stargate > 0 && num_voidray > 1)
    {
        TryBuildUnit(ABILITY_ID::TRAIN_PHOENIX, UNIT_TYPEID::PROTOSS_STARGATE);
    }

    // Train observer units
    if (num_roboticsfacility > 0)
    {
        if (num_observer < max_observer_count)
        {
            TryBuildUnit(ABILITY_ID::TRAIN_OBSERVER, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
        }
    }

    // If we already have a robotics bay but didn't reach max colossus count
    if (num_roboticsbay > 0 && num_colossus < max_colossus_count)
    {
        if (observation->GetMinerals() > 300 && observation->GetVespene() > 200)
        {
            if (TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY))
            {
                return true;
            }
        }
    }
    else
    {
        // Build Immortals if we can't build Colossus
        if (observation->GetMinerals() > 250 && observation->GetVespene() > 100  && num_immortal < max_immortal_count + 5)
        {
            if (TryBuildUnit(ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY))
            {
                return true;
            }
        }
    }

    // After warpgate is researched
    if (warpgate_researched && num_warpgate > 0)
    {
        if (observation->GetMinerals() > 1000 && observation->GetVespene() < 200)
        {
            return TryWarpInUnit(ABILITY_ID::TRAIN_ZEALOT);
        }
        if (num_stalker > max_stalker_count)
        {
            return false;
        }
        if (num_adept > max_stalker_count)
        {
            return false;
        }
        if (num_hightemplar < 2 && num_archon < max_archon_count)
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_HIGHTEMPLAR);
        }

        // If we have robotics facility, build Stalkers. Else, build Adepts
        if (num_roboticsfacility > 0)
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
        }
        else if (!charge_researched)
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_ADEPT);
        }
        else
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_ZEALOT);
        }
    }
    else
    {
        if (observation->GetMinerals() > 120 && observation->GetVespene() > 25)
        {
            if (num_cyberneticscore > 0)
            {
                return TryBuildUnit(ABILITY_ID::TRAIN_ADEPT, UNIT_TYPEID::PROTOSS_GATEWAY);
            }
        }
        else if (observation->GetMinerals() > 100)
        {
            return TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
        }

        return false;
    }
}

/// <summary>
/// Build adaptive army
/// </summary>
/// <returns></returns>
bool GoingMerry::TryBuildAdaptiveArmy()
{
    const ObservationInterface* observation = Observation();

    // Get army unit counts
    size_t num_zealot = CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT);
    size_t num_adept = CountUnitType(UNIT_TYPEID::PROTOSS_ADEPT);
    size_t num_stalker = CountUnitType(UNIT_TYPEID::PROTOSS_STALKER);
    size_t num_immortal = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t num_colossus = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
    size_t num_voidray = CountUnitType(UNIT_TYPEID::PROTOSS_VOIDRAY);
    size_t num_phoenix = CountUnitType(UNIT_TYPEID::PROTOSS_PHOENIX);
    size_t num_hightemplar = CountUnitType(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR);
    size_t num_archon = CountUnitType(UNIT_TYPEID::PROTOSS_ARCHON);
    size_t num_observer = CountUnitType(UNIT_TYPEID::PROTOSS_OBSERVER);
    size_t num_sentry = CountUnitType(UNIT_TYPEID::PROTOSS_SENTRY);


    // Get production structure counts
    size_t num_bases = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t num_gateway = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t num_warpgate = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t num_cyberneticscore = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t num_roboticsfacility = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    size_t num_roboticsbay = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    size_t num_stargate = CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE);

    if (observation->GetFoodWorkers() <= target_worker_count)
    {
        return false;
    }

    // Pause building too many units until 2 bases are expanded
    if (num_bases < 2 && observation->GetFoodArmy() > 20)
    {
        return false;
    }

    // Merge templars to create Archons
    Units templar = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR));

    if (templar.size() > 1)
    {
        Units merge_templars;
        for (int i = 0; i < 2; ++i)
        {
            merge_templars.push_back(templar.at(i));
        }
        Actions()->UnitCommand(merge_templars, ABILITY_ID::MORPH_ARCHON);
    }

    if (observation->GetMinerals() > 250 && observation->GetVespene() > 150 && num_stargate > 0 && num_voidray < max_voidray_count)
    {
        BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_VOIDRAY, ABILITY_ID::TRAIN_VOIDRAY, UNIT_TYPEID::PROTOSS_STARGATE);
    }
    if (num_phoenix < max_phoenix_count && num_stargate > 0)
    {
        BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_PHOENIX, ABILITY_ID::TRAIN_PHOENIX, UNIT_TYPEID::PROTOSS_STARGATE);
    }

    // Train observer units
    if (num_roboticsfacility > 0)
    {
        if (num_observer < max_observer_count)
        {
            TryBuildUnit(ABILITY_ID::TRAIN_OBSERVER, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
        }
    }

    // If we already have a robotics bay but didn't reach max colossus count
    if (num_roboticsbay > 0 && num_colossus < max_colossus_count)
    {
        if (observation->GetMinerals() > 300 && observation->GetVespene() > 200)
        {
            if (BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_COLOSSUS, ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY))
            {
                return true;
            }
        }
    }
    else
    {
        // Build Immortals if we can't build Colossus
        if (observation->GetMinerals() > 250 && observation->GetVespene() > 100 && num_immortal < max_immortal_count + 5)
        {
            if (BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_IMMORTAL, ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY))
            {
                return true;
            }
        }
    }

    // After warpgate is researched
    if (warpgate_researched && num_warpgate > 0)
    {
        if (observation->GetMinerals() > 1000 && observation->GetVespene() < 200)
        {
            return BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_ZEALOT, ABILITY_ID::TRAINWARP_ZEALOT, UNIT_TYPEID::PROTOSS_WARPGATE, true);
        }
        if (num_stalker > max_stalker_count)
        {
            return false;
        }
        if (num_adept > max_stalker_count)
        {
            return false;
        }
        if (num_sentry < max_sentry_count) {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_SENTRY);
        }
        if (num_hightemplar < 2 && num_archon < max_archon_count)
        {
            return BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR, ABILITY_ID::TRAINWARP_HIGHTEMPLAR, UNIT_TYPEID::PROTOSS_WARPGATE, true);
        }

        // If we have robotics facility, build Stalkers. Else, build Adepts
        if (num_roboticsfacility > 0)
        {
            return BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_STALKER, ABILITY_ID::TRAINWARP_STALKER, UNIT_TYPEID::PROTOSS_WARPGATE, true);
        }
        else if (!charge_researched)
        {
            return BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_ADEPT, ABILITY_ID::TRAINWARP_ADEPT, UNIT_TYPEID::PROTOSS_WARPGATE, true);
        }
        else
        {
            return BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_ZEALOT, ABILITY_ID::TRAINWARP_ZEALOT, UNIT_TYPEID::PROTOSS_WARPGATE, true);
        }
    }
    else
    {
        if (observation->GetMinerals() > 120 && observation->GetVespene() > 25)
        {
            if (num_cyberneticscore > 0)
            {
                return BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_ADEPT, ABILITY_ID::TRAIN_ADEPT, UNIT_TYPEID::PROTOSS_GATEWAY);
            }
        }
        else if (observation->GetMinerals() > 100)
        {
            return BuildAdaptiveUnit(UNIT_TYPEID::PROTOSS_ZEALOT, ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
        }

        return false;
    }
}

/// <summary>
/// attack with units
/// </summary>
/// <param name="unit">unit list</param>
/// <param name="observation">observation interface</param>
/// <param name="position">target position</param>
void GoingMerry::AttackWithUnit(const Unit* unit, const ObservationInterface* observation, Point2D position) {


    // Safety check: assert unit is not scout or harasser
    if (std::find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
        return;
    }
    if (find(harassers.begin(), harassers.end(), unit) != harassers.end()) {
        return;
    }

    //If unit isn't doing anything make it attack.
    if (unit->orders.empty()) {
        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, position);
        return;
    }

    // Kite with Colossus
    if (unit->unit_type == UNIT_TYPEID::PROTOSS_COLOSSUS)
    {
        if (thermal_lance_researched)
        {
            if (Distance2D(position, unit->pos) < 9)  // Firing range of Colossus w/o upgrade is 9
            {
                //If the unit is doing something besides attacking, make it attack.
                if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE) {
                    Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                    return;
                }
            }
        }
        else
        {
            if (Distance2D(position, unit->pos) < 7)  // Firing range of Colossus w/o upgrade is 7
            {
                //If the unit is doing something besides attacking, make it attack.
                if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE) {
                    Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                    return;
                }
            }
        }
    }

    // Kite with Void Rays
    if (unit->unit_type == UNIT_TYPEID::PROTOSS_VOIDRAY)
    {
        if (Distance2D(position, unit->pos) < 6)  // Firing range of void ray is 6
        {
                //If the unit is doing something besides attacking, make it attack.
            if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE) 
            {
                Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                return;
            }
        }
    }

    // Kite with Stalkers
    if (unit->unit_type == UNIT_TYPEID::PROTOSS_STALKER)
    {
        if (Distance2D(position, unit->pos) < 6) // Firing range of stalkers is 6
        {
            //If the unit is doing something besides attacking, make it attack.
            if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                return;
            }
        }
    }

    // Kite with Immortals
    if (unit->unit_type == UNIT_TYPEID::PROTOSS_IMMORTAL)
    {
        if (Distance2D(position, unit->pos) < 6) // Firing range of immortals is 6
        {
            //If the unit is doing something besides attacking, make it attack.
            if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                return;
            }
        }
    }

    // Kite with Archons
    if (unit->unit_type == UNIT_TYPEID::PROTOSS_ARCHON)
    {
        if (Distance2D(position, unit->pos) < 3) // Firing range of archons is 3
        {
            //If the unit is doing something besides attacking, make it attack.
            if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                return;
            }
        }
    }

    // Kite with Archons
    if (unit->unit_type == UNIT_TYPEID::PROTOSS_HIGHTEMPLAR)
    {
        if (Distance2D(position, unit->pos) < 6) // Firing range of archons is 3
        {
            //If the unit is doing something besides attacking, make it attack.
            if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                return;
            }
        }
    }

    // Kite with Phoenix
    if (unit->unit_type == UNIT_TYPEID::PROTOSS_PHOENIX)
    {
        if (Distance2D(position, unit->pos) < 5) // Firing range of archons is 3
        {
            //If the unit is doing something besides attacking, make it attack.
            if (unit->orders.front().ability_id != ABILITY_ID::MOVE_MOVE)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, base_locations.front());
                return;
            }
        }
    }

    //If the unit is doing something besides attacking, make it attack.
    if (unit->orders.front().ability_id != ABILITY_ID::ATTACK) {
        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, position);
    }

    return;
}

/// <summary>
/// manage army
/// </summary>
void GoingMerry::ManageArmy()
{
    const ObservationInterface* observation = Observation();
    Units visible_enemies = observation->GetUnits(Unit::Alliance::Enemy);
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units army = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));
    size_t num_immortals = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t num_colossus = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
    size_t num_voidray = CountUnitType(UNIT_TYPEID::PROTOSS_VOIDRAY);
    size_t num_zealots = CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT);
    sc2::Units enemy_bases = Observation()->GetUnits(Unit::Alliance::Enemy, IsTownHall());

    if (num_zealots < num_harassers && launchedHarass)
    {
        launchedHarass = false;
    }
    
    if (army.size() < 20 && launchedAttack)
    {
        launchedAttack = false;
    }

    //There are no enemies yet
    if (visible_enemies.empty()) {

        for (const auto& unit : army)
        {
            if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
                continue;
            }

            if (launchedHarass)
            {
                if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
                    continue;
                }
            }

            const Unit* assigned_base = GetRandomEntry(bases);

            //If unit isn't doing anything make it patrol base.
            if (unit->orders.empty()) {
                Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_PATROL, assigned_base->pos);
                return;
            }

            if (unit->orders.front().ability_id == ABILITY_ID::GENERAL_PATROL)
            {
                continue;
            }
            
            Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_PATROL, assigned_base->pos);
        }
    }

    else {

        const Unit* target_enemy = visible_enemies.front();

        for (const auto& unit : army) {

            const Unit* assigned_base = GetRandomEntry(bases);

            //If unit isn't doing anything make it patrol base.
            if (unit->orders.empty()) {
                Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_PATROL, assigned_base->pos);
                return;
            }

            // Retreat units to base to recharge shields
            if (unit->shield < 5)
            {
                const Unit *shield_battery = nullptr;

                if (GetRandomUnit(shield_battery, observation, UNIT_TYPEID::PROTOSS_SHIELDBATTERY))
                {
                    if (unit->orders.front().ability_id == ABILITY_ID::GENERAL_MOVE && unit->orders.front().target_pos == shield_battery->pos)
                    {
                        continue;
                    }
                    Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_MOVE, shield_battery->pos);
                    continue;
                }
                else
                {
                    Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_PATROL, staging_location);
                    continue;
                }
            }

            // Exclude units
            if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
                continue;
            }

            if (launchedHarass)
            {
                if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
                    continue;
                }
            }

            if (unit->unit_type == UNIT_TYPEID::PROTOSS_OBSERVER)
            {
                if (unit->orders.front().ability_id == ABILITY_ID::ATTACK)
                {
                    continue;
                }
                Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, target_enemy->pos);
                continue;
            }

            // If army composition meets requirement, launch attack
            if ((army.size() > (20 + num_harassers + num_scouts)) && (unit->shield > (unit->shield_max - 10)))
            {
                if (enemy_bases.size() > 0 && !launchedHarass) // only send harass if an enemy base is found
                {
                    const sc2::Unit* base = enemy_bases.back();
                    TrySendHarassing(base);
                    launchedHarass = true;
                    return;
                }

                AttackWithUnit(unit, observation, target_enemy->pos);
                launchedAttack = true;
            }
            // If supply cap reached and current supply used is 195
            else if (observation->GetFoodUsed() > 180 && (unit->shield > (unit->shield_max - 10)))
            {
                if (enemy_bases.size() > 0 && !launchedHarass) // only send harass if an enemy base is found
                {
                    const sc2::Unit *base = enemy_bases.back();
                    TrySendHarassing(base);
                    launchedHarass = true;
                    return;
                }

                AttackWithUnit(unit, observation, target_enemy->pos);
                launchedAttack = true;
            }
            else
            {
                if (!launchedAttack)
                {
                    DefendWithUnit(unit, observation);
                }
            }

            switch (unit->unit_type.ToType()) {

                //Stalker blink micro, blinks back towards your base
                case(UNIT_TYPEID::PROTOSS_STALKER): {
                    if (blink_researched) {
                        const Unit* old_unit = observation->GetUnit(unit->tag);
                        const Unit* target_unit = observation->GetUnit(unit->engaged_target_tag);
                        if (old_unit == nullptr) {
                            break;
                        }
                        Point2D blink_location = staging_location;
                        if (old_unit->shield > 0 && unit->shield < 1) {
                            if (!unit->orders.empty()) {
                                if (target_unit != nullptr) {
                                    Vector2D diff = unit->pos - target_unit->pos;
                                    Normalize2D(diff);
                                    blink_location = unit->pos + diff * 7.0f;
                                }
                                else {
                                    Vector2D diff = unit->pos - start_location;
                                    Normalize2D(diff);
                                    blink_location = unit->pos - diff * 7.0f;
                                }
                                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_BLINK, blink_location);
                            }
                        }
                    }
                    break;
                }
                //Turns on guardian shell when close to an enemy
                case (UNIT_TYPEID::PROTOSS_SENTRY): {
                    if (!unit->orders.empty()) {
                        if (unit->orders.front().ability_id == ABILITY_ID::ATTACK) {
                            float distance = std::numeric_limits<float>::max();
                            for (const auto& u : enemy_units) {
                                float d = Distance2D(u->pos, unit->pos);
                                if (d < distance) {
                                    distance = d;
                                }
                            }
                            if (distance < 6 && unit->energy >= 75) {
                                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_GUARDIANSHIELD);
                            }
                        }
                    }
                    break;
                }
                //Turns on Damage boost when close to an enemy
                case (UNIT_TYPEID::PROTOSS_VOIDRAY): {
                    if (!unit->orders.empty()) {
                        if (unit->orders.front().ability_id == ABILITY_ID::ATTACK) {
                            float distance = std::numeric_limits<float>::max();
                            for (const auto& u : enemy_units) {
                                float d = Distance2D(u->pos, unit->pos);
                                if (d < distance) {
                                    distance = d;
                                }
                            }
                            if (distance < 8) {
                                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_VOIDRAYPRISMATICALIGNMENT);
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}

/// <summary>
/// defend with units
/// </summary>
/// <param name="unit">unit list</param>
/// <param name="observation">observation interface</param>
void GoingMerry::DefendWithUnit(const Unit* unit, const ObservationInterface* observation)
{
    // Safety check: assert unit is not scout or harasser
    if (std::find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
        return;
    }
    if (launchedHarass)
    {
        if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
            return;
        }
    }

    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units visible_enemies = observation->GetUnits(Unit::Alliance::Enemy, IsArmy(observation));
    bool enemiesNearby = false;

    for (const auto& base : bases)
    {
        for (const auto& enemy : visible_enemies)
        {
            float d = Distance2D(enemy->pos, base->pos);
            if (d < 10) {
                enemiesNearby = true;
                AttackWithUnit(unit, observation, enemy->pos);
            }
        }
    }

    if (!enemiesNearby)
    {
        for (const auto& order : unit->orders)
        {
            if (order.ability_id == ABILITY_ID::GENERAL_PATROL)
            {
                continue;
            }
        }
        const Unit *assigned_base = GetRandomEntry(bases);
        Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_PATROL, assigned_base->pos);
    }
}

/// <summary>
/// build adaptive units 
/// </summary>
/// <param name="reference_unit">reference unit type id</param>
/// <param name="ability_type">ablitiy id</param>
/// <param name="production_structure_type">production structure type id</param>
/// <param name="warp"></param>
/// <returns></returns>
bool GoingMerry::BuildAdaptiveUnit(const UNIT_TYPEID reference_unit, ABILITY_ID ability_type, UNIT_TYPEID production_structure_type, bool warp)
{
    // TODO: Based on reference_unit and enemy army composition, issue build order
    //       for counter unit

    size_t num_zealot = CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT);
    size_t num_adept = CountUnitType(UNIT_TYPEID::PROTOSS_ADEPT);
    size_t num_stalker = CountUnitType(UNIT_TYPEID::PROTOSS_STALKER);
    size_t num_immortal = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t num_colossus = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
    size_t num_voidray = CountUnitType(UNIT_TYPEID::PROTOSS_VOIDRAY);
    size_t num_phoenix = CountUnitType(UNIT_TYPEID::PROTOSS_PHOENIX);
    size_t num_hightemplar = CountUnitType(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR);
    size_t num_archon = CountUnitType(UNIT_TYPEID::PROTOSS_ARCHON);

    if (enemy_race == 0)
    {
        // Protoss army units to counter
        size_t num_enemy_immortal = CountEnemyUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
        size_t num_enemy_colossus = CountEnemyUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
        size_t num_enemy_archon = CountEnemyUnitType(UNIT_TYPEID::PROTOSS_ARCHON);

        if (reference_unit == UNIT_TYPEID::PROTOSS_ADEPT)
        {
            if (charge_researched)
            {
                if (warp)
                {
                    return TryWarpInUnit(ABILITY_ID::TRAINWARP_ZEALOT);
                }
                else
                {
                    return TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
                }
            }
        }
        else if (reference_unit == UNIT_TYPEID::PROTOSS_VOIDRAY)
        {
            if (num_enemy_archon > num_colossus)
            {
                if (num_archon < (max_archon_count + (max_voidray_count / 2)))
                {
                    if (warp)
                    {
                        return TryWarpInUnit(ABILITY_ID::TRAINWARP_HIGHTEMPLAR);
                    }
                    else
                    {
                        return TryBuildUnit(ABILITY_ID::TRAIN_HIGHTEMPLAR, UNIT_TYPEID::PROTOSS_GATEWAY);
                    }
                }
            }
        }
        else if (reference_unit == UNIT_TYPEID::PROTOSS_PHOENIX)
        {
            if (!enemy_air_units)
            {
                if (num_stalker < (max_stalker_count + (max_phoenix_count / 2)))
                {
                    return TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
                }
            }
            else
            {
                if (num_enemy_colossus > num_enemy_immortal)
                {
                    if (num_voidray < (max_voidray_count + (max_phoenix_count / 2)))
                    {
                        return TryBuildUnit(ABILITY_ID::TRAIN_VOIDRAY, UNIT_TYPEID::PROTOSS_STARGATE);
                    }
                }
            }
        }
    }
     
    else if (enemy_race == 1)
    {
        // Terran army units to counter
        size_t num_siege_tank = CountEnemyUnitType(UNIT_TYPEID::TERRAN_SIEGETANK);
        size_t num_hellion = CountEnemyUnitType(UNIT_TYPEID::TERRAN_HELLION);
        size_t num_viking = CountEnemyUnitType(UNIT_TYPEID::TERRAN_VIKINGASSAULT);
        size_t num_ghost = CountEnemyUnitType(UNIT_TYPEID::TERRAN_GHOST);
        size_t num_thor = CountEnemyUnitType(UNIT_TYPEID::TERRAN_THOR);

        if (reference_unit == UNIT_TYPEID::PROTOSS_ADEPT)
        {
            if (charge_researched)
            {
                if (warp)
                {
                    return TryWarpInUnit(ABILITY_ID::TRAINWARP_ZEALOT);
                }
                else
                {
                    return TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
                }
            }
        }

        else if (reference_unit == UNIT_TYPEID::PROTOSS_COLOSSUS)
        {
            if (num_siege_tank > num_hellion)
            {
                if (num_immortal < (max_immortal_count + (max_colossus_count/2)))
                {
                    return TryBuildUnit(ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
                }
            }
        }

        else if (reference_unit == UNIT_TYPEID::PROTOSS_VOIDRAY)
        {
            if (num_viking > num_ghost)
            {
                if (num_archon < (max_archon_count + (max_voidray_count/2)))
                {
                    if (warp)
                    {
                        return TryBuildUnit(ABILITY_ID::TRAIN_HIGHTEMPLAR, UNIT_TYPEID::PROTOSS_GATEWAY);
                    }
                    else
                    {
                        return TryWarpInUnit(ABILITY_ID::TRAINWARP_HIGHTEMPLAR);
                    }
                }
                else
                {
                    return TryBuildUnit(ABILITY_ID::TRAIN_PHOENIX, UNIT_TYPEID::PROTOSS_STARGATE);
                }
            }
        }

        else if (reference_unit == UNIT_TYPEID::PROTOSS_PHOENIX)
        {
            if (!enemy_air_units)
            {
                if (num_stalker < (max_stalker_count + (max_phoenix_count / 2)))
                {
                    return TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
                }
            }
            else
            {
                if (num_thor > num_viking)
                {
                    if (num_voidray < (max_voidray_count + (max_phoenix_count / 2)))
                    {
                        return TryBuildUnit(ABILITY_ID::TRAIN_VOIDRAY, UNIT_TYPEID::PROTOSS_STARGATE);
                    }
                }
            }
        }
    }
     
    else if (enemy_race == 2)
    {
        // Zerg army units to counter
        size_t num_corrputor = CountEnemyUnitType(UNIT_TYPEID::ZERG_CORRUPTOR);
        size_t num_hydralisk = CountEnemyUnitType(UNIT_TYPEID::ZERG_HYDRALISK);
        size_t num_ultralisk = CountEnemyUnitType(UNIT_TYPEID::ZERG_ULTRALISK);

        if (reference_unit == UNIT_TYPEID::PROTOSS_ADEPT)
        {
            if (charge_researched)
            {
                if (warp)
                {
                    return TryWarpInUnit(ABILITY_ID::TRAINWARP_ZEALOT);
                }
                else
                {
                    return TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
                }
            }
        }

        else if (reference_unit == UNIT_TYPEID::PROTOSS_COLOSSUS)
        {
            if (num_corrputor > num_hydralisk && CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE) > 0)
            {
                if (num_voidray < (max_voidray_count + (max_colossus_count / 2)))
                {
                    return TryBuildUnit(ABILITY_ID::TRAIN_VOIDRAY, UNIT_TYPEID::PROTOSS_STARGATE);
                }
            }
        }

        else if (reference_unit == UNIT_TYPEID::PROTOSS_VOIDRAY)
        {
            if (num_ultralisk < 3)  // Ultralisk has high kill priority
            {
                if ((num_hydralisk > num_corrputor) || (num_hydralisk > (num_ultralisk * 2)))
                {
                    if (num_colossus < (max_colossus_count + (max_voidray_count / 2)))
                    {
                        return TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
                    }
                }
            }
        }

        else if (reference_unit == UNIT_TYPEID::PROTOSS_PHOENIX)
        {
            if (!enemy_air_units)
            {
                if (num_stalker < (max_stalker_count + (max_phoenix_count / 2)))
                {
                    return TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
                }
            }
            else
            {
                if (num_ultralisk > 2)
                {
                    if (num_archon < (max_archon_count + (max_phoenix_count / 2)))
                    {
                        return TryWarpInUnit(ABILITY_ID::TRAINWARP_HIGHTEMPLAR);
                    }
                }
            }
        }
    }

    if (warp)
    {
        return TryWarpInUnit(ability_type);
    }
    else
    {
        return TryBuildUnit(ability_type, production_structure_type);
    }

    return false;
}

#pragma endregion


#pragma region  Map Analysis

/// <summary>
/// generate grids
/// </summary>
/// <param name="centre">centre point</param>
/// <param name="range">range</param>
/// <returns></returns>
vector<Point2D> GoingMerry::CalculateGrid(Point2D centre, int range)
{
    vector<Point2D> res;

    //calculate the ramge
    float minX = centre.x - range < 0 ? 0 : centre.x - range;
    float minY = centre.y - range < 0 ? 0 : centre.y - range;
    float maxX = centre.x + range > game_info.width ? game_info.width : centre.x + range;
    float maxY = centre.y + range > game_info.height ? game_info.height : centre.y + range;

    //calculate every grid
    for (double i = minX; i <= maxX; i++)
    {
        for (double j = minY; j <= maxY; j++)
        {
            res.push_back(Point2D(i, j));
        }
    }
    return res;
}

/// <summary>
/// find ramp
/// </summary>
/// <param name="centre">centre point</param>
/// <param name="range">range</param>
/// <returns></returns>
vector<Point2D> GoingMerry::FindRamp(Point3D centre, int range)
{
    vector<Point2D> grid = CalculateGrid(Point2D(centre.x, centre.y), range);
    vector<Point2D> ramp;
    
    //check grid one by one
    for (const auto point : grid)
    {
        if (!observation->IsPlacable(point) && observation->IsPathable(point))
        {
            ramp.push_back(point);
        }
    }

    //if no return
    if (ramp.size() == 0)
    {
        return ramp;
    }
    
    //use average to remove unexpected points
    double averageX = 0;
    double averageY = 0;

    for (const auto point : ramp)
    {
        averageX += point.x;
        averageY += point.y;
    }

    averageX /= ramp.size();
    averageY /= ramp.size();


    for (vector<Point2D>::iterator point = ramp.begin(); point < ramp.end();)
    {
           
        if (Distance2D(*point, Point2D(averageX, averageY)) > 8)
        {
            point = ramp.erase(point);
        }
        else
        {
            point++; 
        }
    }
    return ramp;
}

/// <summary>
/// find nearest ramp for centre unit
/// </summary>
/// <param name="centre">centre unit</param>
/// <returns></returns>
Point2D GoingMerry::FindNearestRampPoint(const Unit* centre)
{
    vector<Point2D> ramp = FindRamp(start_location,20);

    //if no ramps return
    if (ramp.size() == 0)
    {
        return Point2D(-1, -1);
    }

    //find the closeted ramp point to the centre point
    Point2D closet = GetRandomEntry(ramp);
    for (const auto& point : ramp)
    {
        if (DistanceSquared2D(point, start_location) < DistanceSquared2D(closet, start_location))
        {
            closet = point;
        }
    }

    return closet;
}

/// <summary>
/// find all points of ramp top
/// </summary>
/// <param name="centre">centre point</param>
/// <returns></returns>
vector<Point2D> GoingMerry::CalculatePlacableRamp(const Unit* centre)
{
    vector<Point2D> wall;
    auto clostest = FindNearestRampPoint(centre);
    if (clostest.x == -1 && clostest.y == -1)
    {
        return wall;
    }

    vector<Point2D> grid = CalculateGrid(clostest,18);

    for (auto point : grid)
    {
        if (centre->pos.z - observation->TerrainHeight(point) < 0.5 && observation->IsPlacable(point) && IsNextToRamp(point))
        {
            wall.push_back(point);
        }
    }
    return wall;
}

/// <summary>
/// check if point next to ramp
/// </summary>
/// <param name="point">point</param>
/// <returns></returns>
bool GoingMerry::IsNextToRamp(const Point2D point)
{
    for (int i = 0; i < 8; i++)
    {
        Point2D temp = Point2D(point.x + directionX[i], point.y + directionY[i]);

        if (!observation->IsPlacable(temp) &&
            observation->IsPathable(temp))
        {
            return true;
        }
    }
    return false;
}

/// <summary>
/// check if point next to cliff
/// </summary>
/// <param name="point">point</param>
/// <returns></returns>
bool GoingMerry::IsNextToCliff(const Point2D point) {

    for (int i = 0; i < 8; i++)
    {
        Point2D temp = Point2D(point.x + directionX[i], point.y + directionY[i]);
        if (start_location.z - observation->TerrainHeight(temp) > 4) {

            return true;
        }
    }
    return false;
}

#pragma endregion
