#include "GoingMerry.h"
#include <iostream>

using namespace sc2;
using namespace std;

void GoingMerry::OnGameStart() { 
    observation = Observation();
    game_info = observation->GetGameInfo();
    expansions = search::CalculateExpansionLocations(Observation(), Query());
    start_location = Observation()->GetStartLocation();
    staging_location = start_location;
    cout << start_location.x << " " << start_location.y << endl;
    cout << game_info.width << " " << game_info.height << endl << endl;
    srand(time(0)); // use current time as seed for random generator

    return; 
}


void GoingMerry::OnStep() 
{ 
    const ObservationInterface* observation = Observation();

    //Throttle some behavior that can wait to avoid duplicate orders.
    int frames_to_skip = 4;
    if (observation->GetFoodUsed() >= observation->GetFoodCap()) {
        frames_to_skip = 6;
    }

    if (observation->GetGameLoop() % frames_to_skip != 0) {
        return;
    }
    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    TryBuildPhotonCannon();
    TrySendScouts();

    BuildOrder();

    TryBuildPylon();

    TryBuildAssimilator();
    TryBuildExpansionNexus();

}

void GoingMerry::OnUnitIdle(const Unit* unit)
{
    switch (unit->unit_type.ToType())
    {
        case UNIT_TYPEID::PROTOSS_NEXUS:
        {            
            // Sometimes creates 1 or 2 extra workers when another worker is busy building something
            Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
            for (const auto& base : bases){
//                if (StillNeedingWorkers()){
                if (base->assigned_harvesters <= base->ideal_harvesters && base->build_progress == 1.0){
                    Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);
                }
            }
            break;
        }
        case UNIT_TYPEID::PROTOSS_PROBE: {

            MineIdleWorkers(unit, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID:: PROTOSS_ASSIMILATOR);
            break;
        }
        case UNIT_TYPEID::PROTOSS_GATEWAY:
        {
            if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) > 0)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_ADEPT);
            }
            else
            {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_ZEALOT);
            }
            break;
        }
        case UNIT_TYPEID::PROTOSS_ZEALOT:{
            const GameInfo& game_info = Observation()->GetGameInfo();
            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
        }
            
        default:
        {
            break;
        }
    }
}

#pragma region worker command

//void GoingMerry::WorkerHub(const Unit* unit)
//{
//    const Units allNexus = observation->GetUnits(Unit::Alliance::Self,IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
//    const Units allAssimilator = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));
//
//    for (const auto& nexus : allNexus)
//    {
//        if (nexus->assigned_harvesters < nexus->ideal_harvesters)
//        {
//            Mine(unit, nexus);
//            return;
//        }
//    }
//
//    for (const auto& assimilator : allAssimilator)
//    {
//        if (assimilator->assigned_harvesters < assimilator->ideal_harvesters)
//        {
//            CollectVespeneGas(unit, assimilator);
//            return;
//        }
//    }
//}
//
//void GoingMerry::Mine(const Unit* unit,const Unit* nexus)
//{
//    const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
//    if (!mineral_target) 
//    {
//        return;
//    }
//    Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
//}
//
//void GoingMerry::CollectVespeneGas(const Unit* unit, const Unit* assimilator)
//{
//    if (!assimilator)
//    {
//        return;
//    }
//    Actions()->UnitCommand(unit, ABILITY_ID::SMART, assimilator);
//}

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

    //If all workers are spots are filled just go to any base.
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

#pragma endregion

#pragma region TryBuildStructures

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE, bool is_expansion = false) {
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE, bool is_expansion = false) {
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

#pragma endregion

#pragma region Assistant Functions

size_t GoingMerry::CountUnitType(UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
}

bool GoingMerry::StillNeedingWorkers()
{
    int res = 0;
    auto probeNumbers = CountUnitType(UNIT_TYPEID::PROTOSS_PROBE);
    const Units allNexus = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
    const Units allAssimilator = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));
    for (const auto& nexus : allNexus)
    {
        res += nexus->ideal_harvesters;
    }
    for (const auto& assimilator : allAssimilator)
    {
        res += assimilator->ideal_harvesters;
    }
    if (probeNumbers < res)
        return true;
    return false;
}

//bool GoingMerry::AlreadyBuilt(const Unit* ref, const Units units)
//{
//    for (const auto& unit : units)
//    {
//        if (ref->pos == unit->pos)
//            return true;
//    }
//    return false;
//}
//
//const Unit* GoingMerry::FindNearestVespenes(const Point2D& start)
//{
//    const Units allGas = observation->GetUnits(Unit::Alliance::Neutral, IsVisibleGeyser());
//    const Units built = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));
//    const Unit* target = nullptr;
//    float minDis = 0;
//
//    for (const auto& gas : allGas)
//    {
//        if (AlreadyBuilt(gas, built))
//        {
//            continue;
//        }
//        float temp = DistanceSquared2D(gas->pos, start);
//    }
//}

//bool GoingMerry::AlreadyBuilt(const Unit* ref, const Units units)
//{
//    for (const auto& unit : units)
//    {
//        if (ref->pos == unit->pos)
//            return true;
//    }
//    return false;
//}
//
//const Unit* GoingMerry::FindNearestVespenes(const Point2D& start)
//{
//    const Units allGas = observation->GetUnits(Unit::Alliance::Neutral, IsVisibleGeyser());
//    const Units built = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));
//    const Unit* target = nullptr;
//    float minDis = 0;
//
//    for (const auto& gas : allGas)
//    {
//        if (AlreadyBuilt(gas, built))
//        {
//            continue;
//        }
//        float temp = DistanceSquared2D(gas->pos, start);
//
//        if (temp < minDis || !target)
//        {
//            minDis = temp;
//            target = gas;
//        }
//    }
//    //cout << target->pos.x << " " << target->pos.y << endl;
//    return target;
//}

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
        //If we never found one return false;
        if (distance == std::numeric_limits<float>::max()) {
            return target;
        }
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

    for (auto pylon : pylons)
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
    vector<Point2D> Directions;

    for (int i = -2; i < 3; i++) {
        for (int j = -2; j < 3; j++) {
            if (i == 0 && j == 0) {
                continue;
            }
            Directions.push_back(Point2D(i, j));
        }
    }

    for (auto direction : Directions) {
        double x = point.x + direction.x;
        double y = point.y + direction.y;

        if (observation->IsPlacable(Point2D(x, y)) && startLocation.z - observation->TerrainHeight(Point2D(x, y)) < 0.5) {
            offSetPoints.push_back(Point2D(x, y));

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

#pragma region Try Build Basic Structures

bool GoingMerry::TryBuildForge() {
    //const ObservationInterface* observation = Observation();
    if (CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) < 1)
    {
        return false;
    }

    //if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) > 0) {
    //    return false;
    //}

    return TryBuildStructure(ABILITY_ID::BUILD_FORGE);
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
        if (base->assigned_harvesters >= base->ideal_harvesters) {
            if (base->build_progress == 1) {
                if (TryBuildGas(ABILITY_ID::BUILD_ASSIMILATOR, UNIT_TYPEID::PROTOSS_PROBE, base->pos)) {
                    return true;
                }
            }
        }
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

    // If we are not supply capped, don't build a supply depot.
    if (observation->GetFoodUsed() < observation->GetFoodCap() - 6) {
        return false;
    }

    if (observation->GetMinerals() < 100) {
        return false;
    }

    //check to see if there is already on building
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

    return TryBuildStructure(ABILITY_ID::BUILD_PYLON, build_location, UNIT_TYPEID::PROTOSS_PROBE);
}

bool GoingMerry::TryBuildCyberneticsCore()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) < 1)
    {
        return false;
    }    
    //if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) > 0)
    //{
    //    return false;
    //}
    return TryBuildStructure(ABILITY_ID::BUILD_CYBERNETICSCORE);
}

bool GoingMerry::TryBuildDarkShrine()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL) < 1)
    {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::PROTOSS_DARKSHRINE) > 0)
    {
        return false;
    }
    return TryBuildStructure(ABILITY_ID::BUILD_DARKSHRINE);
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
    return TryBuildStructure(ABILITY_ID::BUILD_FLEETBEACON);
}

bool GoingMerry::TryBuildGateway()
{
    //if (CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) > 0)
    //{
    //    return false;
    //}
    return TryBuildStructure(ABILITY_ID::BUILD_GATEWAY);
}

bool GoingMerry::TryBuildRoboticsFacility()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) < 1)
    {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY) > 0)
    {
        return false;
    }
    return TryBuildStructure(ABILITY_ID::BUILD_ROBOTICSFACILITY);
}

bool GoingMerry::TryBuildRoboticsBay()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) < 1)
    {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY) > 0)
    {
        return false;
    }
    return TryBuildStructure(ABILITY_ID::BUILD_ROBOTICSBAY);
}

bool GoingMerry::TryBuildStargate()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) < 1)
    {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE) > 0)
    {
        return false;
    }
    return TryBuildStructure(ABILITY_ID::BUILD_STARGATE);
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
    return TryBuildStructure(ABILITY_ID::BUILD_TEMPLARARCHIVE);
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
    return TryBuildStructure(ABILITY_ID::BUILD_TWILIGHTCOUNCIL);
}

bool GoingMerry::TryBuildExpansionNexus()
{
    const ObservationInterface* observation = Observation();

    const Units bases = observation->GetUnits(Unit::Alliance::Self,IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
    const Units assimilators = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));

    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_NEXUS) {
                return false;
            }
        }
    }

    for (const auto& assimilator : assimilators)
    {
        if (assimilator->assigned_harvesters < assimilator->ideal_harvesters)
        {
            return false;
        }
    }

    for (const auto& base : bases)
    {
        if (base->assigned_harvesters < base->ideal_harvesters)
        {
            return false;
        }
    }

    return TryExpandBase(ABILITY_ID::BUILD_NEXUS, UNIT_TYPEID::PROTOSS_PROBE);
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

#pragma endregion

#pragma region Try Update Tech/Structures

bool GoingMerry::TryBuildWarpGate()
{
    return false;
}

#pragma endregion

#pragma region Try Build Defense Structure

bool GoingMerry::TryBuildPhotonCannon()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) < 1)
       return false;
    const Units nux = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
    if (nux.size() == 0)
        return false;
   /* auto position = CalculatePlacableRamp(*nux.begin());
    if (position.size() == 0)
        return false;*/
    //for (auto pos : position)
    //{
    //    if (HavePylonNearby(pos))
    //    {
    //        auto temp = TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, pos);
    //        if (temp)
    //        {
    //            return true;
    //        }
    //        else
    //        {
    //            continue;
    //        }
    //    }
    //    else
    //    {
    //        auto closet = FindClostest(*nux[0], position);
    //        auto temp = TryBuildStructure(ABILITY_ID::BUILD_PYLON, closet);
    //        return temp;
    //    }
    //}


    vector<Point2D> grids = CalculateGrid(start_location, 20);
    for (auto grid : grids)
    {
        bool flag = false;
        for (auto temp : game_info.enemy_start_locations)
        {
            if (Distance2D(grid, temp) >= Distance2D(start_location, temp))
            {
                flag = true;
                break;
            }
        }
        if (flag)
            continue;
        if (HaveCannonNearby(grid))
            continue;
        if (observation->IsPlacable(grid) && IsNextToClif(grid))
        {
            vector<Point2D> points = GetOffSetPoints(grid, UNIT_TYPEID::PROTOSS_PHOTONCANNON);

            for (auto point : points)
            {
                if (HavePylonNearby(point))
                {
                    if (HaveCannonNearby(point))
                    {
                        continue;
                    }

                    if (Query()->Placement(ABILITY_ID::BUILD_PHOTONCANNON, point))
                    {
                        return TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, point);
                    }

                }
                else
                {
                    auto closet = FindClostest(start_location, points);
                    auto temp = TryBuildStructure(ABILITY_ID::BUILD_PYLON, closet);
                    return temp;
                }
            }
        }
    }
    return false;
}

bool GoingMerry::TryBuildShieldBattery()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_PHOTONCANNON))
        return false;
    return true;
}

bool GoingMerry::TryBuildStasisWard()
{
    return false;
}

#pragma endregion

#pragma region Strategy

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

void GoingMerry::SendScouting()
{
    Point2D target_location = GetRandomMapLocation();
    float dist = Distance2D(scouts[0]->pos, target_location);
    while (dist < 50)
    {
        target_location = GetRandomMapLocation();
        dist = Distance2D(scouts[0]->pos, target_location);
    }

    if (scouts[0]->orders.empty()) {
        Actions()->UnitCommand(scouts[0], ABILITY_ID::GENERAL_MOVE, target_location);
        Actions()->UnitCommand(scouts[1], ABILITY_ID::GENERAL_MOVE, target_location);
    }
    else if (!scouts[0]->orders.empty()) {
        if (scouts[0]->orders.front().ability_id != ABILITY_ID::GENERAL_MOVE) {
            Actions()->UnitCommand(scouts[0], ABILITY_ID::GENERAL_MOVE, target_location);
            Actions()->UnitCommand(scouts[1], ABILITY_ID::GENERAL_MOVE, target_location);
        }
    }
    const sc2::Units& cur_enemy_units = Observation()->GetUnits(sc2::Unit::Alliance::Enemy);
    bool found = false;
    for (auto cur : cur_enemy_units)
    {
        found = false;
        if (cur->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
            cur->unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
            cur->unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY)
        {
            for (auto seen : enemy_bases)
            {
                if (seen == cur)
                {
                    found = true;
                }
            }
            if (!found)
            {
                cout << "Found base - new count: " << enemy_bases.size() + 1 << endl;
                enemy_bases.push_back(cur);
            }
        }
        else
        {
            for (auto seen : enemy_units)
            {
                if (seen == cur)
                {
                    found = true;
                }
            }
            if (!found)
            {
                cout << "Found enemy - new count: " << enemy_units.size() + 1 << endl;;
                enemy_units.push_back(cur);
            }
        }
    }
}

void GoingMerry::TrySendScouts()
{
    if (scouts.size() != 2)
    {
        if (CountUnitType(UNIT_TYPEID::PROTOSS_ADEPT) > 2)
        {
            const sc2::Units& adepts = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ADEPT));
            scouts.push_back(adepts[0]);
            scouts.push_back(adepts[1]);
            SendScouting();
        }
        return;
    }
    if (scouts.size() == 2)
    {
        SendScouting();
        return;
    }
}

void GoingMerry::BuildOrder()
{
    //TryBuildPylon();
    //TryBuildAssimilator();
    //TryBuildForge();
    //TryBuildCyberneticsCore();
    //TryBuildGateway();
    //TryBuildTwilightCouncil();
    //TryBuildStargate();
    //TryBuildRoboticsFacility();
    //TryBuildFleetBeacon();
    //TryBuildDarkShrine();
    //TryBuildTemplarArchives();
    //TryBuildRoboticsBay();
    //TryBuildExpansionNexus();

    //const ObservationInterface* observation = Observation();
    size_t n_gateway = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) + CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t n_cybernetics = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t n_forge = CountUnitType(UNIT_TYPEID::PROTOSS_FORGE);
    size_t n_base = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);

    if (n_gateway < min<size_t>(2 * n_base, 7))
    {
        if (n_cybernetics < 1 && n_gateway > 0)
        {
            TryBuildCyberneticsCore();
            return;
        }
        else
        {
            if (n_base < 2 && n_gateway > 0)
            {
                TryBuildExpansionNexus();
                return;
            }

            if (observation->GetFoodWorkers() >= target_worker_count && observation->GetMinerals() > 150 + (100 * n_gateway))
            {
                TryBuildGateway();
            }
        }
    }

    if (n_cybernetics > 0 && n_forge < 2)
    {
        TryBuildForge();
    }
}

#pragma endregion

#pragma region  map analysis

vector<Point2D> GoingMerry::CalculateGrid(Point2D centre, int range)
{
    vector<Point2D> res;
    //cout << "1-1" << endl;
    float minX = centre.x - range < 0 ? 0 : centre.x - range;
    float minY = centre.y - range < 0 ? 0 : centre.y - range;
    float maxX = centre.x + range > game_info.width ? game_info.width : centre.x + range;
    float maxY = centre.y + range > game_info.height ? game_info.height : centre.y + range;
    //cout << minX << " " << maxX << " " << minY << " " << maxY << endl;

    for (double i = minX; i <= maxX; i++)
    {
        for (double j = minY; j <= maxY; j++)
        {
            res.push_back(Point2D(i, j));
        }
    }
    //cout << "1-3" << endl;
    //cout << res.size() << endl;
    cout << res[0].x << " " << res[0].y << endl;
    cout << res[res.size() - 1].x << " " << res[res.size() - 1].y << endl;
    return res;
}

vector<Point2D> GoingMerry::FindRamp(Point3D centre, int range)
{
    vector<Point2D> grid = CalculateGrid(Point2D(centre.x, centre.y), range);
    //cout << "2-1" << endl;
    vector<Point2D> ramp;

    cout << "grid " << grid.size() << endl;
    for (const auto point : grid)
    {
        //cout << point.x << " " << point.y << endl;
        if (!observation->IsPlacable(point) && observation->IsPathable(point))
        {
            ramp.push_back(point);
        }
    }

    cout << "ramp " << ramp.size() << endl;

    if (ramp.size() == 0)
    {
        cout << "2-0" << endl;
        return ramp;
    }
    
    double averageX = 0;
    double averageY = 0;

    for (const auto point : ramp)
    {
        averageX += point.x;
        averageY += point.y;
    }

    averageX /= ramp.size();
    averageY /= ramp.size();

    //cout << "2-3" << endl;

    for (vector<Point2D>::iterator point = ramp.begin(); point < ramp.end();)
    {
        //bool temp = help(*point);
           
        if (Distance2D(*point, Point2D(averageX, averageY)) > 8)
        {
            point = ramp.erase(point);
        }
        else
        {
            point++; 
        }
    }
    //cout << "2-4 " << ramp.size() << endl;
    return ramp;
}

Point2D GoingMerry::FindNearestRampPoint(const Unit* centre)
{
    vector<Point2D> ramp = FindRamp(start_location,20);

    if (ramp.size() == 0)
    {
        return Point2D(-1, -1);
    }

    Point2D closet = GetRandomEntry(ramp);
    for (const auto& point : ramp)
    {
        if (DistanceSquared2D(point, start_location) < DistanceSquared2D(closet, start_location))
        {
            closet = point;
        }
    }
    //cout << "3" << endl;
    cout << closet.x << " " << closet.y << endl << endl;

    return closet;
}

vector<Point2D> GoingMerry::CalculatePlacableRamp(const Unit* centre)
{
    vector<Point2D> wall;
    auto clostest = FindNearestRampPoint(centre);
    if (clostest.x == -1 && clostest.y == -1)
    {
        cout << 'NONE' << endl;
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

bool GoingMerry::IsNextToClif(const Point2D point) {

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
