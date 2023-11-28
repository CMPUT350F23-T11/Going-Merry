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

    TryBuildWarpGate();

    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID::PROTOSS_ASSIMILATOR);

    TrySendScouts();

    ManageUpgrades();

    BuildOrder();

    if (TryBuildPylon())
    {
        return;
    }

    if (TryBuildAssimilator())
    {
        return;
    }

    if (TryBuildProbe())
    {
        return;
    }

    if (TryBuildArmy())
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
//        case UNIT_TYPEID::PROTOSS_NEXUS:
//        {            
//            // Sometimes creates 1 or 2 extra workers when another worker is busy building something
//            Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
//            for (const auto& base : bases){
////                if (StillNeedingWorkers()){
//                if (base->assigned_harvesters <= base->ideal_harvesters && base->build_progress == 1){
//                    Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);
//                }
//            }
//            break;
//        }
        case UNIT_TYPEID::PROTOSS_PROBE: {

            MineIdleWorkers(unit, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID:: PROTOSS_ASSIMILATOR);
            break;
        }
        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
        {
            const ObservationInterface* observation = Observation();
            Units bases = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));

            if (!warpgate_reasearched)
            {
                Actions()->UnitCommand(unit, ABILITY_ID::RESEARCH_WARPGATE);
                OnUpgradeCompleted(UPGRADE_ID::WARPGATERESEARCH);

                if (!bases.empty())
                {
                    Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_TIMEWARP, unit);
                }
            }
        }
            
        default:
        {
            break;
        }
    }
}


void GoingMerry::OnUpgradeCompleted(UpgradeID upgrade)
{
    switch (upgrade.ToType())
    {
        case UPGRADE_ID::WARPGATERESEARCH:
        {
            warpgate_reasearched = true;
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


#pragma region Worker Commands


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
// 
//void GoingMerry::CollectVespeneGas(const Unit* unit, const Unit* assimilator)
//{
//    if (!assimilator)
//    {
//        return;
//    }
//    Actions()->UnitCommand(unit, ABILITY_ID::SMART, assimilator);
//}


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


#pragma region TryBuildStructure()


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
    return TryBuildStructure(ABILITY_ID::BUILD_CYBERNETICSCORE);
}


bool GoingMerry::TryBuildGateway()
{
    return TryBuildStructure(ABILITY_ID::BUILD_GATEWAY);
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


void GoingMerry::TryBuildWarpGate()
{
    const ObservationInterface* observation = Observation();

    Units gateways = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));

    if (warpgate_reasearched)
    {
        for (const auto& gateway : gateways)
        {
            if (gateway->build_progress == 1)
            {
                Actions()->UnitCommand(gateway, ABILITY_ID::MORPH_WARPGATE);
            }
        }
    }
}

void GoingMerry::ManageUpgrades()
{
    const ObservationInterface* observation = Observation();
    auto upgrades = observation->GetUpgrades();
    size_t n_base = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);  // prioritize maintaining number of bases over upgrades

    if (upgrades.empty())
    {
        TryBuildUnit(ABILITY_ID::RESEARCH_WARPGATE, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
        OnUpgradeCompleted(UPGRADE_ID::WARPGATERESEARCH);
    }

    else
    {
        for (const auto& upgrade : upgrades)
        {
            if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1 && n_base > 2)
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1 && n_base > 2)
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2 && n_base > 2)
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2 && n_base > 2)
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else
            {
                TryBuildUnit(ABILITY_ID::RESEARCH_EXTENDEDTHERMALLANCE, UNIT_TYPEID::PROTOSS_ROBOTICSBAY);  // For Colossus units

                TryBuildUnit(ABILITY_ID::RESEARCH_BLINK, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);  // For Stalker units
                OnUpgradeCompleted(UPGRADE_ID::BLINKTECH);

                TryBuildUnit(ABILITY_ID::RESEARCH_CHARGE, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);  // For Zealot units (if using)
                OnUpgradeCompleted(UPGRADE_ID::CHARGE);

                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
        }
    }

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

#pragma region Try Send Scouting

sc2::Point2D GoingMerry::GetScoutMoveLocation() 
{
    const sc2::GameInfo& game_info = Observation()->GetGameInfo();

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
    Point2D target_location;
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
            const sc2::Unit *&randomMineral = sc2::GetRandomEntry(minerals);

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
    if (visitedLocations.size() >= minerals.size())
    {
        visitedLocations.clear();
    }

    return target_location;
}



void GoingMerry::MoveScouts()
{
    Point2D target_location = GetScoutMoveLocation(); // get location to send scouts to 

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
}

void GoingMerry::SendScouting()
{
    MoveScouts(); // move scouts to new location
    const sc2::Units& cur_enemy_units = Observation()->GetUnits(sc2::Unit::Alliance::Enemy); // check if any enemies are found
    bool found = false;
    bool foundBase = false;
    const sc2::Unit *base;

    // add unit to either enemy_bases vector or enemy_units vector based on whether or not its the first time encountering it
    for (auto cur : cur_enemy_units)
    {
        found = false;
        if (!(cur->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
            cur->unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
            cur->unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY))
        {
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
            for (auto seen : enemy_bases)
            {
                if (seen->tag == cur->tag)
                {
                    found = true;
                }
            }
            if (!found)
            {
                enemy_bases.push_back(cur);
            }
        }
    }
}

void GoingMerry::SendHarassing(const sc2::Unit *base)
{
    // send to harass based on the enemy base location
    if (scouts[0]->orders.empty()) 
    {
        Actions()->UnitCommand(scouts[0], ABILITY_ID::ATTACK_ATTACKBARRAGE, base->pos);
        Actions()->UnitCommand(scouts[1], ABILITY_ID::ATTACK_ATTACKBARRAGE, base->pos);
    }
    else if (!scouts[0]->orders.empty()) 
    {
        if (scouts[0]->orders.front().ability_id != ABILITY_ID::ATTACK_ATTACKBARRAGE) 
        {
            Actions()->UnitCommand(scouts[0], ABILITY_ID::ATTACK_ATTACKBARRAGE, base->pos);
            Actions()->UnitCommand(scouts[1], ABILITY_ID::ATTACK_ATTACKBARRAGE, base->pos);
        }
    }
}

void GoingMerry::CheckScoutsAlive()
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

void GoingMerry::TrySendScouts()
{    
    // checking if a base is already found -> send harassing if conditions are met
    if (scouts.size() > 0)
    {
        CheckScoutsAlive();
    }
    if (scouts.size() < 2)
    {
        if (scouts.size() == 0) // if need to add 2 scouts
        {
            if (CountUnitType(UNIT_TYPEID::PROTOSS_ADEPT) > 2)
            {
                const sc2::Units& adepts = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ADEPT));
                scouts.push_back(adepts[0]);
                scouts.push_back(adepts[1]);
            }
        }
        else // if only need to add 1 scout
        {
            if (CountUnitType(UNIT_TYPEID::PROTOSS_ADEPT) > 1)
            {
                const sc2::Units& adepts = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ADEPT));
                scouts.push_back(adepts[0]);
            }
        }
        return;
    }
    
    if (scouts.size() == 2) // if a pair of scouts available send to harass or scout
    {
        const sc2::Units& enemy_bases = Observation()->GetUnits(Unit::Alliance::Enemy, IsUnit(UNIT_TYPEID::ZERG_HATCHERY));
        if (enemy_bases.size() > 0) // only send harass if an enemy base is found
        {
            for (auto base : enemy_bases)
            {
                SendHarassing(base);
                break;
            }
        }
        else
        {
            SendScouting();
        }
    }
}


#pragma endregion


bool GoingMerry::GetRandomUnit(const Unit* &unit_out, const ObservationInterface* observation, UnitTypeID unit_type)
{
    Units units = observation->GetUnits(Unit::Alliance::Self);
    random_shuffle(units.begin(), units.end());

    for (const auto unit : units)
    {
        if (unit->unit_type == unit_type)
        {
            unit_out = unit;
            return true;
        }
    }

    return false;
}


bool GoingMerry::TryBuildUnit(AbilityID ability_type, UnitTypeID unit_type)
{
    const ObservationInterface* observation = Observation();

    // Do not build units if supply cap reached
    if (observation->GetFoodUsed() > -observation->GetFoodCap())
    {
        return false;
    }

    const Unit* unit = nullptr;
    if (!GetRandomUnit(unit, observation, unit_type))
    {
        return false;
    }
    if (!unit->orders.empty())
    {
        return false;
    }
    if (unit->build_progress != 1)
    {
        return false;
    }

    Actions()->UnitCommand(unit, ability_type);
    return true;
}


bool GoingMerry::TryBuildArmy()
{
    const ObservationInterface* observation = Observation();

    if (observation->GetFoodWorkers() <= target_worker_count)
    {
        return false;
    }

    // Pause building too many units until 2 bases are expanded
    if (CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS) < 2 && observation->GetFoodArmy() > 10)
    {
        return false;
    }

    // If we have a satisfying army already, pause until next expansion
    if (CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS) < 3 && observation->GetFoodArmy() > 40)
    {
        return false;
    }

    size_t n_mothership = CountUnitType(UNIT_TYPEID::PROTOSS_MOTHERSHIPCORE);
    n_mothership += CountUnitType(UNIT_TYPEID::PROTOSS_MOTHERSHIP);

    if (observation->GetFoodWorkers() > target_worker_count && n_mothership < 1)
    {
        if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) > 0)
        {
            TryBuildUnit(ABILITY_ID::TRAIN_MOTHERSHIPCORE, UNIT_TYPEID::PROTOSS_NEXUS);
        }
    }

    size_t n_colossus = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);
    size_t n_carrier = CountUnitType(UNIT_TYPEID::PROTOSS_CARRIER);

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

    // Train observer units
    if (CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY) > 0)
    {
        if (CountUnitType(UNIT_TYPEID::PROTOSS_OBSERVER) < 1)
        {
            TryBuildUnit(ABILITY_ID::TRAIN_OBSERVER, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
        }
    }

    // If we already have a robotics bay but didn't reach max colossus count
    if (CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY) > 0 && n_colossus < max_colossus_count)
    {
        if (observation->GetMinerals() > 300 && observation->GetVespene() > 200)
        {
            if (TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY))
            {
                return true;
            }
        }
        else if (CountUnitType(UNIT_TYPEID::PROTOSS_DISRUPTOR) < 2)
        {
            TryBuildUnit(ABILITY_ID::TRAIN_DISRUPTOR, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
        }
    }
    else
    {
        // Build Immortals if we can't build Colossus
        if (observation->GetMinerals() > 250 && observation->GetVespene() > 100)
        {
            if (TryBuildUnit(ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY))
            {
                return true;
            }
        }
    }

    // After warpgate is researched
    if (warpgate_reasearched && CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE) > 0)
    {
        if (observation->GetMinerals() > 1000 && observation->GetVespene() < 200)
        {
            return TryWarpInUnit(ABILITY_ID::TRAIN_ZEALOT);
        }
        if (CountUnitType(UNIT_TYPEID::PROTOSS_STALKER) > max_stalker_count)
        {
            return false;
        }
        if (CountUnitType(UNIT_TYPEID::PROTOSS_ADEPT) > max_stalker_count)
        {
            return false;
        }
        if (CountUnitType(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR) < 2 && CountUnitType(UNIT_TYPEID::PROTOSS_ARCHON) < 2)
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_HIGHTEMPLAR);
        }

        // If we have robotics facility, build Stalkers. Else, build Adepts
        if (CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY) > 0)
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
        }
        else
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_ADEPT);
        }
    }
    else
    {
        if (observation->GetMinerals() > 120 && observation->GetVespene() > 25)
        {
            if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) > 0)
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


bool GoingMerry::TryWarpInUnit(ABILITY_ID ability_type)
{
    const ObservationInterface* observation = Observation();
    vector<PowerSource> power_sources = observation->GetPowerSources();
    Units warpgates = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));

    if (power_sources.empty())
    {
        return false;
    }

    const PowerSource& rand_power_source = GetRandomEntry(power_sources); // Select random power source
    float radius = rand_power_source.radius;
    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D build_location = Point2D(rand_power_source.position.x + (rx * radius), rand_power_source.position.y + (ry * radius));

    // Make sure there is pathing from build_location to center of the map, otherwise, do not warp at build_location
    if (Query()->PathingDistance(build_location, Point2D(game_info.playable_max.x / 2, game_info.playable_max.y / 2)) < .01f)
    {
        return false;
    }

    for (const auto& warpgate : warpgates)
    {
        if (warpgate->build_progress == 1)
        {
            AvailableAbilities abilities = Query()->GetAbilitiesForUnit(warpgate);

                for (const auto &ability : abilities.abilities)
                {
                    if (ability.ability_id == ability_type)
                    {
                        Actions()->UnitCommand(warpgate, ability_type, build_location);
                        return true;
                    }
                }
        }
    }

    return false;
}


void GoingMerry::BuildOrder()
{
    const ObservationInterface* observation = Observation();
    size_t n_gateway = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) + CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t n_cybernetics = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t n_forge = CountUnitType(UNIT_TYPEID::PROTOSS_FORGE);
    size_t n_base = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t n_robotics_facility = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);
    size_t n_robotics_bay = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    size_t n_twilight_council = CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    size_t n_templar_archive = CountUnitType(UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE);

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

    if (n_gateway > 2 && n_cybernetics > 0)
    {
        if (n_robotics_facility < min<size_t>(n_base, 4))
        {
            if (observation->GetMinerals() > 200 && observation->GetVespene() > 100)
            {
                TryBuildStructure(ABILITY_ID::BUILD_ROBOTICSFACILITY, UNIT_TYPEID::PROTOSS_PROBE);
            }
        }
        else if (n_robotics_facility > 0 && n_robotics_bay < 1)
        {
            if (observation->GetMinerals() > 200 && observation->GetVespene() > 200)
            {
                TryBuildStructure(ABILITY_ID::BUILD_ROBOTICSBAY, UNIT_TYPEID::PROTOSS_PROBE);
            }
        }
    }

    if (n_forge > 0 && n_twilight_council < 1 && n_base > 1)
    {
        TryBuildStructure(ABILITY_ID::BUILD_TWILIGHTCOUNCIL, UNIT_TYPEID::PROTOSS_PROBE);
    }

    if (n_twilight_council > 0 && n_templar_archive < 1 && n_base > 2)
    {
        TryBuildStructure(ABILITY_ID::BUILD_TEMPLARARCHIVE, UNIT_TYPEID::PROTOSS_PROBE);
    }
}


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


void GoingMerry::AttackWithUnit(const Unit* unit, const ObservationInterface* observation) {
    //If unit isn't doing anything make it attack.
    Units enemy_units = observation->GetUnits(Unit::Alliance::Enemy);
    if (enemy_units.empty()) {
        return;
    }

    if (unit->orders.empty()) {
        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, enemy_units.front()->pos);
        return;
    }

    //If the unit is doing something besides attacking, make it attack.
    if (unit->orders.front().ability_id != ABILITY_ID::ATTACK) {
        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, enemy_units.front()->pos);
    }
}


void GoingMerry::ManageArmy()
{
    const ObservationInterface* observation = Observation();
    Units enemy_units = observation->GetUnits(Unit::Alliance::Enemy);
    Units army = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));
    int wait_til_supply = 100;

    //There are no enemies yet, and we don't have a big army
    if (enemy_units.empty() && observation->GetFoodArmy() < wait_til_supply) {
        return;
    }

    else if (!enemy_units.empty()){

        for (const auto& unit : army) {
            AttackWithUnit(unit, observation);

            switch (unit->unit_type.ToType()) {

                //Stalker blink micro, blinks back towards your base
                case(UNIT_TYPEID::PROTOSS_OBSERVER): {
                    Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, enemy_units.front()->pos);
                    break;
                }
                case(UNIT_TYPEID::PROTOSS_STALKER): {
                    if (blink_researched) {
                        const Unit* old_unit = observation->GetUnit(unit->tag);
                        const Unit* target_unit = observation->GetUnit(unit->engaged_target_tag);
                        if (old_unit == nullptr) {
                            break;
                        }
                        Point2D blink_location = start_location;
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
                //Turns on oracle weapon when close to an enemy
                case (UNIT_TYPEID::PROTOSS_ORACLE): {
                    if (!unit->orders.empty()) {
                        float distance = std::numeric_limits<float>::max();
                        for (const auto& u : enemy_units) {
                            float d = Distance2D(u->pos, unit->pos);
                            if (d < distance) {
                                distance = d;
                            }
                        }
                        if (distance < 6 && unit->energy >= 25) {
                            Actions()->UnitCommand(unit, ABILITY_ID::BEHAVIOR_PULSARBEAMON);
                        }
                    }
                    break;
                }
                //fires a disruptor nova when in range
                case (UNIT_TYPEID::PROTOSS_DISRUPTOR): {
                    float distance = std::numeric_limits<float>::max();
                    Point2D closest_unit;
                    for (const auto& u : enemy_units) {
                        float d = Distance2D(u->pos, unit->pos);
                        if (d < distance) {
                            distance = d;
                            closest_unit = u->pos;
                        }
                    }
                    if (distance < 7) {
                        Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_PURIFICATIONNOVA, closest_unit);
                    }
                    else {
                        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, closest_unit);
                    }
                    break;
                }
                //controls disruptor novas.
                case (UNIT_TYPEID::PROTOSS_DISRUPTORPHASED): {
                    float distance = std::numeric_limits<float>::max();
                    Point2D closest_unit;
                    for (const auto& u : enemy_units) {
                        if (u->is_flying) {
                            continue;
                        }
                        float d = DistanceSquared2D(u->pos, unit->pos);
                        if (d < distance) {
                            distance = d;
                            closest_unit = u->pos;
                        }
                    }
                    Actions()->UnitCommand(unit, ABILITY_ID::MOVE_MOVE, closest_unit);
                    break;
                }
                default:
                    break;
            }
        }
    }
    else {
        for (const auto& unit : army) {
            ScoutWithUnit(unit, observation);
        }
    }
}

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

void GoingMerry::ScoutWithUnit(const Unit* unit, const ObservationInterface* observation) {
    Units enemy_units = observation->GetUnits(Unit::Alliance::Enemy, IsAttackable());

    if (!unit->orders.empty()) {
        return;
    }

    if (!enemy_units.empty())
    {
        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, enemy_units.front());
        return;
    }
}



