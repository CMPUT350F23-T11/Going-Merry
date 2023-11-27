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
    srand(time(0)); // use current time as seed for random generator

    return; 
}


void GoingMerry::OnStep() 
{ 
    const ObservationInterface* observation = Observation();
    // Get game info
    const GameInfo& game_info = observation->GetGameInfo();
    
    // In game time
    // Game loop increments by 1 for every 22.4 milliseconds of game time
    uint32_t current_game_loop = observation->GetGameLoop();
    float ingame_time = current_game_loop / 22.4f;
    // Supply
    uint32_t current_supply = observation->GetFoodUsed();
    uint32_t supply_cap = observation->GetFoodCap();
    // Mineral and Gas
    uint32_t current_minerals = observation->GetMinerals();
    uint32_t current_gas = observation->GetVespene();
    
    std::cout<<"Time: "<<ingame_time<<"s || Supply: "<<current_supply<<" / "<<supply_cap<<" || Minerals: "<<current_minerals<<" || Gas:"<<current_gas<<endl;;
    
    
    //____________________________________________________________________________________
    //Throttle some behavior that can wait to avoid duplicate orders.
    int frames_to_skip = 4;
    if (current_supply >= observation->GetFoodCap()) {
        frames_to_skip = 6;
    }

    if (current_game_loop % frames_to_skip != 0) {
        return;
    }
    // ____________________________________________________________________________________
    
    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    TrySendScouts();

    BuildOrder(ingame_time, current_supply, current_minerals, current_gas);

    if (TryBuildPylon())
    {
        return;
    }
        
//
//    if (TryBuildAssimilator())
//    {
//        return;
//    }
//
//    if (TryBuildExpansionNexus()) 
//    {
//        return;
//    }
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
                if (base->assigned_harvesters <= base->ideal_harvesters && base->build_progress == 1.0){
                    Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);
                }
            }
            break;
        }
        case UNIT_TYPEID::PROTOSS_PROBE: {

            MineIdleWorkers(unit, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID:: PROTOSS_ASSIMILATOR);
            break;
        }
//        case UNIT_TYPEID::PROTOSS_GATEWAY:
//        {
//            if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) > 0)
//            {
//                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_ADEPT);
//            }
//            else
//            {
//                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_ZEALOT);
//            }
//            break;
//        }
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

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
//                std::cout<<"already building"<<std::endl;
                return false;
            }
        }
        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }

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
bool GoingMerry::TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool isExpansion = false) {

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
        for (const auto& expansion : expansions_) {
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE, bool is_expansion) {

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
    return TryBuildStructure(ability_type_for_structure, UNIT_TYPEID::PROTOSS_PROBE, build_location);
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

const Unit* GoingMerry::FindNearestMineralPatch(const Point2D& start) {
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
                    std::cout<<"BUILD GAS TRUE"<<std::endl;
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

//     If we are not supply capped, don't build a supply depot.
    if (observation->GetFoodUsed() < observation->GetFoodCap() - 1) {
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
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_DARKSHRINE, UNIT_TYPEID::PROTOSS_PROBE);
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
    return TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, UNIT_TYPEID::PROTOSS_PROBE);
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
    if (CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE) > 0)
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

bool GoingMerry::TryBuildUnit(AbilityID ability_type_for_unit, UnitTypeID unit_type) {
    const ObservationInterface* observation = Observation();

    //If we are at supply cap, don't build anymore units, unless its an overlord.
    if (observation->GetFoodUsed() >= observation->GetFoodCap() && ability_type_for_unit != ABILITY_ID::TRAIN_OVERLORD) {
        return false;
    }
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
    return true;
}

bool GoingMerry::GetRandomUnit(const Unit*& unit_out, const ObservationInterface* observation, UnitTypeID unit_type) {
    Units my_units = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
    if (!my_units.empty()) {
        unit_out = GetRandomEntry(my_units);
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
    return false;
}

#pragma endregion

#pragma region Try Build Defense Structure

bool GoingMerry::TryBuildPhotonCannon()
{
    
    return false;
}

bool GoingMerry::TryBuildShieldBattery()
{
    return false;
}

bool GoingMerry::TryBuildStasisWard()
{
    return false;
}

#pragma endregion


sc2::Point2D GoingMerry::GetRandomMapLocation() 
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

void GoingMerry::BuildOrder(float ingame_time, uint32_t current_supply, uint32_t current_minerals, uint32_t current_gas)
{
    std::cout<<"---"<<std::endl;
    
    // STRUCTURE/UNIT COUNTS
    size_t pylon_count = CountUnitType(UNIT_TYPEID::PROTOSS_PYLON);
    size_t gateway_count = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t warpgate_count = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t assimilator_count = CountUnitType(UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    size_t cybernetics_count = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t base_count = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t robotics_facility_count = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    size_t robotics_bay_count = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    
    size_t observer_count = CountUnitType(UNIT_TYPEID::PROTOSS_OBSERVER);
    size_t stalkers_count = CountUnitType(UNIT_TYPEID::PROTOSS_STALKER);
    size_t immortals_count = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t colossus_count = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
    
    
    // UNITS
    Units cores = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE));
    Units gateways = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
    Units warpgates = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units rfacs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY));
    Units rbays = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSBAY));
    
    // UPGRADES
    const std::vector<UpgradeID> upgrades = observation->GetUpgrades();
    
    bool core_complete = false;
    bool warp_upgrade_complete = false;
    for (const auto& upgrade : upgrades){
        if(upgrade == UPGRADE_ID::WARPGATERESEARCH){
            warp_upgrade_complete = true;
            break;
        }
    }

    //      14      0:20      Pylon
    if(pylon_count == 0 &&
       assimilator_count < 1){
        if(TryBuildPylon()){
            std::cout<<"PYLON 1 0:20"<<std::endl; // 23 cap
        }
        // TODO: build at chokepoint (TONY)
    }
        
    //      15      0:40      Gateway
    if (gateway_count == 0 &&
        assimilator_count < 1){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY 1 0:40"<<std::endl;
        }
        // TODO: build at chokepoint (TONY)
        // TODO: Send 1 probe to walk around enemy base to identify their buildings (ABEER)
    }
        
    //      16      0:48      Assimilator
    //      17      0:58      Assimilator
    if(gateway_count == 1 &&
       assimilator_count < 1){
        if(TryBuildAssimilator()){
            std::cout<<"GAS 1 0:48"<<std::endl;
        }
    }
        
    if(gateway_count == 1 &&
       assimilator_count < 2){
        if(TryBuildAssimilator()){
            std::cout<<"GAS 2 0:58"<<std::endl;
        }
    }

    //      19      1:13      Gateway
    if(warpgate_count == 0 &&
       gateway_count < 2 &&
       assimilator_count == 2){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY 2 1:13"<<std::endl;
        }
    }
        
    //      20      1:28      Cybernetics Core
    if(gateway_count == 2 &&
       assimilator_count == 2 &&
       cybernetics_count == 0){
        if(TryBuildCyberneticsCore()){
            std::cout<<"CYBERNETICS 1 1:28"<<std::endl;
        }
    }
    
    
    //      23      2:02      Stalkers x2 (Chrono Boost) (2:24 250 minerals)
    if(stalkers_count < 8 &&
       gateway_count == 2 &&
       cybernetics_count > 0 &&
       warp_upgrade_complete == false){
        
        bool building_stalkers = false;
        if(cores.front()->build_progress == 1.0f &&
           current_supply <= (observation->GetFoodCap() - 3)){
            if(TryBuildUnit(ABILITY_ID::TRAIN_STALKER, UNIT_TYPEID::PROTOSS_GATEWAY)){
                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, gateways.front());
                std::cout<<"STALKERS 2:02"<<std::endl;
            }
            else if(TryBuildUnit(ABILITY_ID::RESEARCH_WARPGATE, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE)){
                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, cores.front());
                std::cout<<"RESEARCH WARPGATE 2:08"<<std::endl;
            }
        }
    }
    
    //      27      2:08      Warp Gate Research
    if(stalkers_count == 8 &&
       gateway_count == 2 &&
       cybernetics_count == 1 &&
       warp_upgrade_complete == false){
        for(const auto& gateway: gateways){
            if(!gateway->orders.empty()){
                Actions()->UnitCommand(gateway, ABILITY_ID::CANCEL_LAST);
            }
        }
    }
        
    //      31      2:56      Nexus
    if(current_minerals >= 400 &&
       stalkers_count <= 8 &&
       cybernetics_count == 1 &&
       (gateway_count <= 2 || warpgate_count <= 2) &&
       base_count == 1){
        if(TryBuildExpansionNexus()){
            std::cout<<"EXPAND 1 2:56"<<std::endl;
        }
    }
    
//              32      3:10      Robotics Facility
    if(stalkers_count <= 8 &&
       cybernetics_count > 0 &&
       (gateway_count == 2 || warpgate_count == 2) &&
       base_count == 2 &&
       robotics_facility_count < 1){
        
        if(TryBuildRoboticsFacility()){
            std::cout<<"ROBOTICS FAC 1 3:10 "<<std::endl;
        }
    }

    //      32      3:48      Gateway
    if(warpgate_count == 2 &&
       gateway_count == 0 &&
       base_count == 2 &&
       robotics_facility_count == 1){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY 3 3:48"<<std::endl;
        }
    }

    if(warpgate_count <= 3 &&
       cybernetics_count > 0 &&
       gateway_count < 1 &&
       base_count == 2 &&
       stalkers_count <= 12 &&
       robotics_facility_count == 1){
        
        if(stalkers_count < 8){
            for(const auto& warp : warpgates){
                if(TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER)){
                    std::cout<<"STALKER WARP 3:48"<<std::endl;
                }
            }
        }
        
        // BUILD 1 OBSERVER
        Units rfacs = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY));
        //      34      3:57      Observer (Chrono Boost)
        if(CountUnitType(UNIT_TYPEID::PROTOSS_OBSERVER) == 0){
            if(rfacs.front()->orders.empty()){
                Actions()->UnitCommand(rfacs.front(), ABILITY_ID::TRAIN_OBSERVER);
            }
        }
        
        // CHRONOBOOST IF BUILDING
        if(CountUnitType(UNIT_TYPEID::PROTOSS_OBSERVER) == 1 && !rfacs.front()->orders.empty()){
            Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, rfacs.front());
        }
        
        
        // MOVE ARMY TO 2ND BASE
        Units stalker_army = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_STALKER));
        Actions()->UnitCommand(stalker_army, ABILITY_ID::SMART);
        
        
        float dist_1 = Distance3D(start_location, bases.at(0)->pos);
        float dist_2 = Distance3D(start_location, (bases.at(1))->pos);
        
        Point2D base_loc;
        if(dist_1 > dist_2){
            base_loc.x = (bases.at(0))->pos.x;
            base_loc.y = (bases.at(0))->pos.y;
        }
        else{
            base_loc.x = bases.at(1)->pos.x;
            base_loc.y = bases.at(1)->pos.y;
        }
        
        if(second_base[0].x == 0 && second_base[0].y == 0){
            second_base[0] = base_loc;
        }
        else{
            base_loc = second_base[0];
        }
        Actions()->UnitCommand(stalker_army, ABILITY_ID::ATTACK_ATTACK, base_loc);
        
    }
    
        
    //      34      4:00      Robotics Bay
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_count == 3 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 0 &&
       stalkers_count <= 12 &&
       observer_count == 1){
        if(TryBuildRoboticsBay()){
            std::cout<<"ROB BAY 4:00"<<std::endl;
        }
    }
    
    //      43      4:27      Assimilator x2 & Immortal (Chrono Boost)
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_count == 3 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 1 &&
       stalkers_count <= 20 &&
       assimilator_count <= 4 &&
       immortals_count < 2){
        Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
        for(const auto& worker : workers){
            if(worker->orders.empty()){
                MineIdleWorkers(worker, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID:: PROTOSS_ASSIMILATOR);
            }
        }
        
        if(current_minerals >= 275 && current_gas >= 100){
            if(TryBuildUnit(ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)){
                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, rfacs.front());
                std::cout<<"IMMORTAL x2 4:27"<<std::endl;
            }
            
        }
        
        if(TryBuildAssimilator()){
            //      49      4:45      Assimilator x2
            std::cout<<"ASSIMILATOR x2 4:45"<<std::endl;

        }
    }
    
    //      51      4:55      Colossus (Chrono Boost)
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_count == 3 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 1 &&
       stalkers_count <= 20 &&
       assimilator_count == 4 &&
       immortals_count == 2 &&
       colossus_count < 1){
        if(current_minerals >= 300 && current_gas >= 200){
            if(TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)){
                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, rfacs.front());
                std::cout<<"COLOSSUS 1 4:27"<<std::endl;
            }
            
        }
    }
    
    
    //      58      5:05      Extended Thermal Lance
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_count == 3 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 1 &&
       stalkers_count <= 20 &&
       assimilator_count == 4 &&
       immortals_count == 2 &&
       colossus_count == 1){
        if(TryBuildUnit(ABILITY_ID::RESEARCH_EXTENDEDTHERMALLANCE, UNIT_TYPEID::PROTOSS_ROBOTICSBAY)){
            std::cout<<"THERMAL LANCE 5:05"<<std::endl;

        }
        
        Units units = observation->GetUnits(Unit::Alliance::Self);
        Units army;
        for(const auto& unit : units){
            if(unit->unit_type.ToType() == UNIT_TYPEID::PROTOSS_STALKER ||
               unit->unit_type.ToType() == UNIT_TYPEID::PROTOSS_IMMORTAL ||
               unit->unit_type.ToType() == UNIT_TYPEID::PROTOSS_COLOSSUS){
                army.push_back(unit);
            }
        }
        if(!army.empty()){
            Actions()->UnitCommand(army, ABILITY_ID::SMART);
            Actions()->UnitCommand(army, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
        }
    }
    
    

        
        
        
        
        // END BUILD ORDER
//    }
        
        
    









//      61      5:22      Pylon
//      62      5:35      Nexus
//      64      5:53      Colossus (Chrono Boost)
//      72      6:02      Forge
//      74      6:11      Stalker x2
//      78      6:22      Gateway
//      78      6:30      Protoss Ground Weapons Level 1 (Chrono Boost)
//      78      6:36      Colossus (Chrono Boost)
//      86      6:52      Twilight Council
//      89      6:57      Stalker x2
//      96      7:14      Photon Cannon x2
//      99      7:24      Colossus (Chrono Boost)
//      108      7:31      Charge
//      109      7:48      Gateway x2
//      109      7:58      Gateway
//      117      8:08      Observer (Chrono Boost)
//      124      8:20      Protoss Ground Weapons Level 2 (Chrono Boost)
//      124      8:23      Colossus (Chrono Boost)
//      133      8:38      Nexus
//      144      9:07      Warp Prism (Chrono Boost)
//      149      9:27      Gateway x3, Templar Archives
//      149      9:36      Colossus (Chrono Boost)
}
