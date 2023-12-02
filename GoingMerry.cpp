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
    
    // Save main base location
    Units base = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Point2D base_loc(base.front()->pos.x, base.front()->pos.y);
    base_locations.push_back(base_loc);
    
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

    //TryBuildWarpGate();
    
    // Should fix newly created idle workers
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
    for(const auto& worker : workers){
        if(worker->orders.empty()){
            MineIdleWorkers(worker, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID::PROTOSS_ASSIMILATOR);
        }
    }

    ManageArmy();
    
    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID::PROTOSS_ASSIMILATOR);

    TryBuildPhotonCannon();
    
    TrySendScouts();

    ManageUpgrades();

    BuildOrder(ingame_time, current_supply, current_minerals, current_gas);

    if (TryBuildPylon())
    {
        return;
    }
    
    if (TryBuildProbe()) {
        return;
    }
        
     if (TryBuildPylon())
     {
         return;
     }

     if (TryBuildArmy())
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
            // Sometimes creates 1 or 2 extra workers when another worker is busy building something
            Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
            for (const auto& base : bases){
//                if (StillNeedingWorkers()){
                if (base->assigned_harvesters <= base->ideal_harvesters && base->build_progress == 1){
                    Actions()->UnitCommand(base, ABILITY_ID::TRAIN_PROBE);
                }
            }
            break;
        }
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
        case UPGRADE_ID::EXTENDEDTHERMALLANCE:
        {
            thermal_lance_researched = true;
        }
        case UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1:
        {
            ground_wep_1_researched = true;
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


#pragma region Worker commands

bool GoingMerry::TryBuildProbe() {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    if (observation->GetFoodWorkers() >= max_worker_count_) {
        return false;
    }

    if (observation->GetFoodUsed() >= observation->GetFoodCap()) {
        return false;
    }

    if (observation->GetFoodWorkers() > GetExpectedWorkers(UNIT_TYPEID::PROTOSS_ASSIMILATOR)) {
        return false;
    }

    for (const auto& base : bases) {
        //if there is a base with less than ideal workers
        if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1) {
            if (observation->GetMinerals() >= 50) {
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

//bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE, bool is_expansion = false) {
//    //const ObservationInterface* observation = Observation();
//
//    const ObservationInterface* observation = Observation();
//    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
//
//    //if we have no workers Don't build
//    if (workers.empty()) {
//        return false;
//    }
//
//    // Check to see if there is already a worker heading out to build it
//    for (const auto& worker : workers) {
//        for (const auto& order : worker->orders) {
//            if (order.ability_id == ability_type_for_structure) {
//                return false;
//            }
//        }
//    }
//    if (!unit_to_build)
//        return false;
//
//    // If no worker is already building one, get a random worker to build one
//    const Unit* unit = GetRandomEntry(workers);
//    // do not use scouting probe or it will go back to build
//    if(unit == scouting_probe){
//        const Unit* unit = GetRandomEntry(workers);
//    }
//
//    // Check to see if unit can make it there
//    if (Query()->PathingDistance(unit, location) < 0.1f) {
//        return false;
//    }
//    if (!isExpansion) {
//        for (const auto& expansion : expansions_) {
//            if (Distance2D(location, Point2D(expansion.x, expansion.y)) < 7) {
//                return false;
//            }
//        }
//    }
//    // Check to see if unit can build there
//    if (Query()->Placement(ability_type_for_structure, location)) {
//        Actions()->UnitCommand(unit, ability_type_for_structure, location);
//        return true;
//    }
//    return false;
//
//}

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
                OnUpgradeCompleted(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1);
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
                OnUpgradeCompleted(UPGRADE_ID::EXTENDEDTHERMALLANCE);

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
//                        return TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, point);
                        // TODO: (TONY) invalid parameters ^
                    }

                }
                else
                {
                    auto closet = FindClostest(start_location, points);
//                    auto temp = TryBuildStructure(ABILITY_ID::BUILD_PYLON, closet);
                    // TODO: (TONY) invalid parameters ^

//                    return temp;
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
            if (CountUnitType(UNIT_TYPEID::PROTOSS_STALKER) > 2)
            {
                const sc2::Units& stalkers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_STALKER));
                scouts.push_back(stalkers[0]);
                scouts.push_back(stalkers[1]);
            }
        }
        else // if only need to add 1 scout
        {
            if (CountUnitType(UNIT_TYPEID::PROTOSS_STALKER) > 1)
            {
                const sc2::Units& stalkers = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_STALKER));
                scouts.push_back(stalkers[0]);
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
                return TryBuildUnit(ABILITY_ID::TRAIN_STALKER, UNIT_TYPEID::PROTOSS_GATEWAY);
            }
        }
        else if (observation->GetMinerals() > 100)
        {
            return TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
        }

        return false;
    }
}


void GoingMerry::BuildOrder(float ingame_time, uint32_t current_supply, uint32_t current_minerals, uint32_t current_gas)
{
    
    //TODO: build cannons around bases 3 each?
    
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
    size_t forge_count = CountUnitType(UNIT_TYPEID::PROTOSS_FORGE);
    size_t twilight_count = CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    size_t cannon_count = CountUnitType(UNIT_TYPEID::PROTOSS_PHOTONCANNON);
    
    
    
    size_t observer_count = CountUnitType(UNIT_TYPEID::PROTOSS_OBSERVER);
    size_t zealot_count = CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT);
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
    Units forges = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_FORGE));
    Units twilights = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL));
    
    // UPGRADES
    const std::vector<UpgradeID> upgrades = observation->GetUpgrades();
    
    bool core_complete = false;
    bool warp_upgrade_complete = false;
    bool thermal_lance_complete = false;
    bool ground_weapons_1_complete = false;
    bool charge_complete = false;
    
    for (const auto& upgrade : upgrades){
        if(upgrade == UPGRADE_ID::WARPGATERESEARCH){
            warp_upgrade_complete = true;
        }
        if(upgrade == UPGRADE_ID::EXTENDEDTHERMALLANCE){
            thermal_lance_complete = true;
        }
        if(upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1){
            ground_weapons_1_complete = true;
        }
        if(upgrade == UPGRADE_ID::CHARGE){
            charge_complete = true;
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
        warpgate_count == 0){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY 1 0:40"<<std::endl;
        }
        // TODO: build at chokepoint (TONY)
        
    }
        
    //      16      0:48      Assimilator
    //      17      0:58      Assimilator
    if(gateway_count == 1 &&
       assimilator_count < 2){
        
        // send scout after building 1st gateway
        // TODO: Send 1 probe to walk around enemy base to identify their buildings (ABEER)
        if(scouting_probe == nullptr){
            Units probes = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
            scouting_probe = probes.front();
            Actions()->UnitCommand(scouting_probe, ABILITY_ID::MOVE_MOVE, game_info.enemy_start_locations.front());
        }
        
        if(TryBuildAssimilator()){
            std::cout<<"GAS x2 0:48"<<std::endl;
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
    //      27      2:08      Warp Gate Research
    if(gateway_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_reasearched == false){
        for(const auto& gateway : gateways){
            if(!gateway->orders.empty()){
                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, gateway);
            }
        }
    }
    
    //      31      2:56      Nexus
    if(cybernetics_count > 0 &&
       (gateway_count <= 2 || warpgate_count <= 2) &&
       base_count == 1){
        if(current_minerals >= 400){
            if(TryBuildExpansionNexus()){
                std::cout<<"EXPAND 1 2:56"<<std::endl;
            }
        }
    }
    
    //       32      3:10      Robotics Facility
    if(cybernetics_count > 0 &&
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
    
    // TODO: should move army to second base to defend here
    
    //      34      4:00      Robotics Bay
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_count == 3 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 0 &&
       assimilator_count == 2){
        if(TryBuildRoboticsBay()){
            std::cout<<"ROB BAY 4:00"<<std::endl;
        }
    }
 
    //      43      4:27      Immortal (Chrono Boost)
    //      49      4:45      Assimilator x2
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_count == 3 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 1 &&
       assimilator_count < 4){
        
//        if(immortals_count < 2 && current_minerals >= 275 && current_gas >= 100){
//            if(TryBuildUnit(ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)){
//                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, rfacs.front());
//                std::cout<<"IMMORTAL x2 4:27"<<std::endl;
//            }
//        }
        
        if(TryBuildAssimilator()){
            std::cout<<"ASSIMILATOR x2 4:45"<<std::endl;
        }
    }
   
    //      51      4:55      Colossus (Chrono Boost)
    //      58      5:05      Extended Thermal Lance
    
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 1){
        
        //      62      5:35      Nexus
        if(TryBuildExpansionNexus()){
            std::cout<<"EXPAND 2 5:35"<<std::endl;
        }
        
        
//        Units units = observation->GetUnits(Unit::Alliance::Self);
//        Units army;
//        for(const auto& unit : units){
//            if(unit->unit_type.ToType() == UNIT_TYPEID::PROTOSS_STALKER ||
//               unit->unit_type.ToType() == UNIT_TYPEID::PROTOSS_IMMORTAL ||
//               unit->unit_type.ToType() == UNIT_TYPEID::PROTOSS_COLOSSUS ||
//               unit->unit_type.ToType() == UNIT_TYPEID::PROTOSS_ZEALOT){
//                army.push_back(unit);
//            }
//        }
//        if(!army.empty()){
//            Actions()->UnitCommand(army, ABILITY_ID::SMART);
////            Actions()->UnitCommand(army, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
//            Actions()->UnitCommand(army, ABILITY_ID::ATTACK_ATTACK, base_locations[1]);
//        }
    }
    
    //      64      5:53      Colossus (Chrono Boost)
    //      72      6:02      Forge
    if(base_count == 3 &&
       cybernetics_count > 0 &&
       warpgate_count == 3 &&
       robotics_facility_count == 1 &&
       robotics_bay_count == 1 &&
       assimilator_count == 4 &&
       forge_count == 0){
//        if(colossus_count == 1 && current_minerals >= 300 && current_gas >= 200){
//            if(TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)){
//                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, rfacs.front());
//                std::cout<<"COLOSSUS 2 5:53"<<std::endl;
//            }
//        }
        
        if(TryBuildForge()){
            std::cout<<"FORGE 6:02"<<std::endl;
        }
        
//        // Build zealots to defend 3rd base
//        TryWarpInUnit(ABILITY_ID::TRAINWARP_ZEALOT);
//        
//        Units zealots = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT));
//        if(bases.size() == 3 && base_locations.size() == 2){
//            // go through each base
//            for(int i = 0 ; i < 3; ++i){
//                Point2D base(bases.at(i)->pos.x, bases.at(i)->pos.y);
//                bool found = false;
//                
//                // find base in saved base locations
//                for(int j = 0; j < base_locations.size(); ++j){
//                    if(base == base_locations[j]){
//                        found = true;
//                        break;
//                    }
//                }
//                
//                // if not found, push back into base_locations
//                if(!found){
//                    base_locations.push_back(base);
//                }
//            }
//        }
//        
//        // Go to 3rd base (goes to enemy base by OnUnitIdle
//        if(!zealots.empty() && base_locations.size() == 3){
//            Actions()->UnitCommand(zealots, ABILITY_ID::SMART);
//            Actions()->UnitCommand(zealots, ABILITY_ID::ATTACK_ATTACK, base_locations[2]);
//        }
        
    }
        
    //      78      6:22      Gateway
    if(base_count == 3 &&
       warpgate_count == 3 &&
       gateway_count == 0 &&
       assimilator_count == 4 &&
       robotics_bay_count == 1 &&
       forge_count == 1){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY 4 6:22"<<std::endl;
        }
    }
        
    //      78      6:30      Protoss Ground Weapons Level 1 (Chrono Boost)
    //      78      6:36      Colossus (Chrono Boost)
    //      86      6:52      Twilight Council
    if(base_count == 3 &&
       ((warpgate_count < 4 && gateway_count == 1) || (warpgate_count == 4 && gateway_count == 0)) &&
       assimilator_count == 4 &&
       robotics_bay_count == 1 &&
       forge_count == 1 &&
       twilight_count == 0){
        
//        if(forges.front()->build_progress == 1.0){
//            if(TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONSLEVEL1, UNIT_TYPEID::PROTOSS_FORGE)){
//                std::cout<<"RESEARCH GROUND WEAPONS 1 6:30"<<std::endl;
//            }
//        }
//        
//        //      78      6:36      Colossus (Chrono Boost)
//        if(colossus_count < 3){
//            if(TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)){
//                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, rfacs.front());
//                std::cout<<"COLOSSUS 3 6:36"<<std::endl;
//            }
//        }
        
        if(TryBuildTwilightCouncil()){
            std::cout<<"TWILIGHT 6:52"<<std::endl;

        }
    }
    
    //      96      7:14      Photon Cannon x2
    //      99      7:24      Colossus (Chrono Boost)
    //      108      7:31      Charge
    if(base_count == 3 &&
       warpgate_count == 4 &&
       assimilator_count <= 6 &&
       robotics_bay_count == 1 &&
       forge_count == 1 &&
       twilight_count == 1 &&
       cannon_count <= 10 &&
       !charge_researched){
        if(TryBuildAssimilator()){
            std::cout<<"GAS 3rd BASE 7:14"<<std::endl;

        }
        if(cannon_count < 10){
            if(TryBuildPhotonCannon()){
                std::cout<<"CANNON x5 7:14"<<std::endl;
            }
        }
        
//        //      99      7:24      Colossus (Chrono Boost)
//        if(colossus_count < 4){
//            if(TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)){
//                if(!rfacs.front()->orders.empty()){
//                    Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, rfacs.front());
//                }
//                std::cout<<"COLOSSUS 4 7:24"<<std::endl;
//            }
//        }
        
        //      108      7:31      Charge
        if(!twilights.front()->orders.empty()){
            Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, twilights.front());
        }
    }
    
    //      109      7:48      Gateway x2
    //      109      7:58      Gateway
    if(base_count == 3 &&
       warpgate_count == 4 &&
       gateway_count == 0 &&
       assimilator_count == 6 &&
       robotics_bay_count == 1 &&
       forge_count == 1 &&
       twilight_count == 1 &&
       cannon_count == 5){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY x3 7:48"<<std::endl;
        }
    }
    
    if(base_count == 3 &&
       warpgate_count <= 5 &&
       gateway_count == 1 &&
       assimilator_count == 6 &&
       robotics_bay_count == 1 &&
       forge_count == 1 &&
       twilight_count == 1 &&
       cannon_count == 5){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY x3 7:48"<<std::endl;
        }
    }
    
    if(base_count == 3 &&
       warpgate_count <= 6 &&
       gateway_count == 2 &&
       assimilator_count == 6 &&
       robotics_bay_count == 1 &&
       forge_count == 1 &&
       twilight_count == 1 &&
       cannon_count == 5){
        if(TryBuildGateway()){
            std::cout<<"GATEWAY x3 7:48"<<std::endl;
        }
    }
    
    if(base_count == 3 &&
       warpgate_count == 7 &&
       assimilator_count == 6 &&
       robotics_bay_count == 1 &&
       forge_count == 1 &&
       twilight_count == 1 &&
       cannon_count < 10){
        if(TryBuildPhotonCannon()){
            std::cout<<"MORE CANNONS x5 7:48"<<std::endl;
        }
    }
    
    //      117      8:08      Observer (Chrono Boost)
    //      124      8:20      Protoss Ground Weapons Level 2 (Chrono Boost)
    //      124      8:23      Colossus (Chrono Boost)
    //      133      8:38      Nexus
    if(base_count == 3 &&
       assimilator_count == 6 &&
       warpgate_count == 7 &&
       forge_count == 1 &&
       twilight_count == 1 &&
       ground_wep_2_researched){
        if(TryBuildExpansionNexus()){
            std::cout<<"EXPAND 3 8:38"<<std::endl;

        }
    }
    
    //      144      9:07      Warp Prism (Chrono Boost)
    
    //      149      9:27      Gateway x3, Templar Archives
    if(base_count == 3 &&
       assimilator_count < 8){
        TryBuildAssimilator();
    }
    //      149      9:36      Colossus (Chrono Boost)
    
    // general build order after
    if(base_count > 3 &&
       warpgate_count < (base_count * 3) &&
       gateway_count < 3 &&
       assimilator_count <= (base_count * 2)){
        TryBuildAssimilator();
        TryBuildGateway();
    }
     
        
    // END BUILD ORDER
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


void GoingMerry::AttackWithUnit(const Unit* unit, const ObservationInterface* observation, Point2D position) {

    if (std::find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
        return;
    }
    else {
        //If unit isn't doing anything make it attack.
        if (unit->orders.empty()) {
            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, position);
            return;
        }

        //If the unit is doing something besides attacking, make it attack.
        if (unit->orders.front().ability_id != ABILITY_ID::ATTACK) {
            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, position);
        }
    }
}


void GoingMerry::ManageArmy()
{
    const ObservationInterface* observation = Observation();
    Units visible_enemies = observation->GetUnits(Unit::Alliance::Enemy);
    Units army = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));
    size_t num_immortals = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t num_colossus = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);

    int wait_til_supply = 100;

    //There are no enemies yet, and we don't have a big army
    if (visible_enemies.empty() && observation->GetFoodArmy() < wait_til_supply) {
        return;
    }

    else if (!visible_enemies.empty()){

        for (const auto& unit : army) {
            if (army.size() > 10 && num_colossus > 0 && num_immortals > 0)
            {
                cout << "Error at line 1819" << endl;
                AttackWithUnit(unit, observation, visible_enemies.front()->pos);
            }
            else
            {
                Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_HOLDPOSITION, staging_location);
            }

            switch (unit->unit_type.ToType()) {

                //Stalker blink micro, blinks back towards your base
                case(UNIT_TYPEID::PROTOSS_OBSERVER): {
                    if(!enemy_units.empty()){
                        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, enemy_units.front()->pos);
                        break;
                    }}
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
                    Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_MOVE, closest_unit);
                    break;
                }
                default:
                    break;
            }
        }
    }

    else if (!enemy_bases.empty())
    {
        for (const auto& unit : army) {
            if (army.size() > 10 && num_colossus > 0 && num_immortals > 0)
            {
                cout << "Error here at line 1958" << endl;
                AttackWithUnit(unit, observation, enemy_bases.front()->pos);
            }
            else
            {
                Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_HOLDPOSITION, staging_location);
            }
        }
    }
    else {
        for (const auto& unit : army) {
            DefendWithUnit(unit, observation);
        }
    }
}

void GoingMerry::DefendWithUnit(const Unit* unit, const ObservationInterface* observation)
{
    // TODO
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
