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

    //Throttle some behavior that can wait to avoid duplicate orders.
    int frames_to_skip = 4;
    if (observation->GetFoodUsed() >= observation->GetFoodCap()) {
        frames_to_skip = 6;
    }

    if (observation->GetGameLoop() % frames_to_skip != 0) {
        return;
    }

    TrySendScouts();

    BuildOrder();

    if (TryBuildPylon())
    {
        return;
    }

    if (TryBuildAssimilator())
    {
        return;
    }

    if (TryBuildExpansionNexus()) 
    {
        return;
    }
}


void GoingMerry::OnUnitIdle(const Unit* unit)
{
    switch (unit->unit_type.ToType())
    {
        case UNIT_TYPEID::PROTOSS_NEXUS:
        {            
            if (StillNeedingWorkers())
            {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);
            }
            break;
        }
        case UNIT_TYPEID::PROTOSS_PROBE: {

            WorkerHub(unit);
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
        default:
        {
            break;
        }
    }
}

#pragma region worker command

void GoingMerry::WorkerHub(const Unit* unit)
{
    const Units allNexus = observation->GetUnits(Unit::Alliance::Self,IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
    const Units allAssimilator = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));

    for (const auto& nexus : allNexus)
    {
        if (nexus->assigned_harvesters < nexus->ideal_harvesters)
        {
            Mine(unit, nexus);
            return;
        }
    }

    for (const auto& assimilator : allAssimilator)
    {
        if (assimilator->assigned_harvesters < assimilator->ideal_harvesters)
        {
            CollectVespeneGas(unit, assimilator);
            return;
        }
    }
}

void GoingMerry::Mine(const Unit* unit,const Unit* nexus)
{
    const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
    if (!mineral_target) 
    {
        return;
    }
    Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
}

void GoingMerry::CollectVespeneGas(const Unit* unit, const Unit* assimilator)
{
    if (!assimilator)
    {
        return;
    }
    Actions()->UnitCommand(unit, ABILITY_ID::SMART, assimilator);
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE, bool is_expansion) {
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

    if (Query()->Placement(ability_type_for_structure, target->pos))
    {
        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            target);

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

bool GoingMerry::AlreadyBuilt(const Unit* ref, const Units units)
{
    for (const auto& unit : units)
    {
        if (ref->pos == unit->pos)
            return true;
    }
    return false;
}

const Unit* GoingMerry::FindNearestVespenes(const Point2D& start)
{
    const Units allGas = observation->GetUnits(Unit::Alliance::Neutral, IsVisibleGeyser());
    const Units built = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));
    const Unit* target = nullptr;
    float minDis = 0;

    for (const auto& gas : allGas)
    {
        if (AlreadyBuilt(gas, built))
        {
            continue;
        }
        float temp = DistanceSquared2D(gas->pos, start);

        if (temp < minDis || !target)
        {
            minDis = temp;
            target = gas;
        }
    }
    //cout << target->pos.x << " " << target->pos.y << endl;
    return target;
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

    return TryBuildStructure(ABILITY_ID::BUILD_FORGE);
}

bool GoingMerry::TryBuildAssimilator()
{
    const ObservationInterface* observation = Observation();
    const Units bases = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));

    if (CountUnitType(UNIT_TYPEID::PROTOSS_ASSIMILATOR) >= bases.size() * 2)
    {
        return false;
    }
    const Unit* target = nullptr;

    int random_index = rand() % bases.size();

    const Unit *base = bases[random_index];

    target = FindNearestVespenes(base->pos);
 
    if (!target)
    {
        return false;
    }

    return TryBuildStructure(ABILITY_ID::BUILD_ASSIMILATOR, target);
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

    const ObservationInterface* observation = Observation();
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