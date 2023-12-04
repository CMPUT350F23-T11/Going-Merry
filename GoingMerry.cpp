#include "GoingMerry.h"
#include <iostream>


using namespace sc2;
using namespace std;


#pragma region Debug Tools

void printLog(string message, bool step = false)
{
    cout << "DEBUG LOG: " << message << endl;

    if (step)
    {
        int temp;
        cin >> temp;
    }
}

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
    
    // Fix for newly created idle workers
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
    for(const auto& worker : workers){
        if(worker->orders.empty()){
            MineIdleWorkers(worker, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID::PROTOSS_ASSIMILATOR);
        }
    }

    ManageArmy();
    
    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    
    TrySendScouts();

    ManageUpgrades();
    
    TryBuildWarpGate();

    BuildOrder(ingame_time, current_supply, current_minerals, current_gas);

    if (TryBuildPylon())
    {
        return;
    }
    if (TryBuildAssimilator())
    {
        return;
    }

    if (TryBuildProbe()) {
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
        case UNIT_TYPEID::PROTOSS_NEXUS:
        {            
            // Sometimes creates 1 or 2 extra workers when another worker is busy building something
            Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
            for (const auto& base : bases){
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


#pragma region Worker Commands

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
bool GoingMerry::TryBuildStructureForPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool isExpansion = false) {

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
    return TryBuildStructureForPylon(ability_type_for_structure, UNIT_TYPEID::PROTOSS_PROBE, build_location);
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
                    //std::cout<<"BUILD GAS TRUE"<<std::endl;
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
    // 23 pylons max
    Units pylons = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON));
    if(pylons.size() > 25){
        return false;
    }

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
    
    if(bases.size() == 3 && fleet_count < 1){
        return false;
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
                
                // GROUND
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
                TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
                if(ground_wep_2_researched){
                    TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSSHIELDS, UNIT_TYPEID::PROTOSS_FORGE);
                }
                
                
                // AIR
                if(stargate_count > 0){
                    TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSAIRWEAPONS, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
                    TryBuildUnit(ABILITY_ID::RESEARCH_PROTOSSAIRARMOR, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
                    TryBuildUnit(ABILITY_ID::RESEARCH_PHOENIXANIONPULSECRYSTALS, UNIT_TYPEID::PROTOSS_FLEETBEACON);
                    TryBuildUnit(ABILITY_ID::RESEARCH_VOIDRAYSPEEDUPGRADE, UNIT_TYPEID::PROTOSS_FLEETBEACON);
                }
            }
        }
        
        // chronoboost upgrades
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


#pragma region Try Build Defense Structure

bool GoingMerry::TryBuildPhotonCannon()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) < 1)
       return false;

    Units nux = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));

    auto position = CalculatePlacableRamp(nux.front());
    if (position.size() == 0)
        return false;

    for (auto pos : position)
    {
        if (HavePylonNearby(pos))
        {
            auto temp = TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, pos);
            if (temp)
            {
                return true;
            }
            else
            {
                continue;
            }
        }
        else
        {
            auto closet = FindClostest(nux.front()->pos, position);
            auto temp = TryBuildStructure(ABILITY_ID::BUILD_PYLON, closet);
            return temp;
        }
    }

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
        {
            continue;
        }

        if (observation->IsPlacable(grid) && IsNextToCliff(grid))
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

                    if (TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, point))
                    {
                        return true;
                        
                    }
                }
                else
                {
                    auto closest = FindClostest(start_location, points);
                    TryBuildStructure(ABILITY_ID::BUILD_PYLON, closest);

                    if (HaveCannonNearby(point))
                    {
                        continue;
                    }

                    if (TryBuildStructure(ABILITY_ID::BUILD_PHOTONCANNON, point))
                    {
                        return true;
                    }
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


#pragma region Try Send Scouting and Harassers

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
    if ((visitedLocations.size() > 100) || (visitedLocations.size() >= minerals.size() && minerals.size() > 0))
    {
        visitedLocations.clear();
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
    
    if (harassers.size() == num_harassers && CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT) >= num_harassers)
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
        }
        else if (!harassers[i]->orders.empty()) 
        {
            if (harassers[i]->orders.front().ability_id != ABILITY_ID::ATTACK_ATTACK) 
            {
                Actions()->UnitCommand(harassers[i], ABILITY_ID::ATTACK_ATTACK, base->pos);
            }
        }
    }

    Actions()->UnitCommand(harassers, ABILITY_ID::ATTACK_ATTACK, base->pos);
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
    
    sc2::Units enemy_bases = Observation()->GetUnits(Unit::Alliance::Enemy, IsTownHall());
    if (enemy_bases.size() > 0) // only send harass if an enemy base is found
    {
        const sc2::Unit *&base = sc2::GetRandomEntry(enemy_bases);;
        TrySendHarassing(base);
    }

    if (scouts.size() == num_scouts) // if a pair of scouts available send to harass or scout
    {
        SendScouting();
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
    if (pylon_count == 0 &&
        assimilator_count < 1) {
        if (TryBuildPylon()) {
            //std::cout<<"PYLON 1 0:20"<<std::endl; // 23 cap
        }
    }

    //      15      0:40      Gateway
    if (gateway_count == 0 &&
        warpgate_count == 0) {
        if (TryBuildGateway()) {
            //std::cout<<"GATEWAY 1 0:40"<<std::endl;
        }
    }

    //      16      0:48      Assimilator
    //      17      0:58      Assimilator
    if (gateway_count == 1 &&
        assimilator_count < 1) {
        if (TryBuildAssimilator()) {
            //std::cout<<"GAS 1 0:48"<<std::endl;
        }
    }

    if (gateway_count == 1 &&
        assimilator_count < 2) {
        if (TryBuildAssimilator()) {
            //            std::cout<<"GAS x2 0:48"<<std::endl;
        }
    }

    //      19      1:13      Gateway
    if (warpgate_count == 0 &&
        gateway_count < 2 &&
        assimilator_count == 2) {
        if (TryBuildGateway()) {
            //std::cout<<"GATEWAY 2 1:13"<<std::endl;
        }
    }

    //      20      1:28      Cybernetics Core
    if (gateway_count == 2 &&
        assimilator_count == 2 &&
        cybernetics_count == 0) {
        if (TryBuildCyberneticsCore()) {
            //std::cout<<"CYBERNETICS 1 1:28"<<std::endl;
        }
    }

    //      27      2:08      Warp Gate Research
    if (gateway_count == 2 &&
        cybernetics_count > 0 &&
        warpgate_reasearched == false) {
        for (const auto& gateway : gateways) {
            if (!gateway->orders.empty()) {
                Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOSTENERGYCOST, gateway);
            }
        }
    }

//    if (gateway_count == 2 &&
//        cybernetics_count > 0 &&
//        warpgate_researched == true &&
//        gateway_count > warpgate_count) {
//
//        if (TryBuildWarpGate())
//        {
//            //std::cout<< "CONVERTING TO WARPGATE" << std::endl;
//        }
//    }
    
    
    //      31      2:56      Nexus
    if(cybernetics_count > 0 &&
       (gateway_count <= 2 || warpgate_count <= 2) &&
       base_count == 1){
        if(current_minerals >= 400){
            if(TryBuildExpansionNexus()){
            }
        }
    }

    //       32      3:10      Robotics Facility
    if(cybernetics_count > 0 &&
       (gateway_count >= 2 || warpgate_count >= 2) &&
       base_count == 2 &&
       robotics_facility_count < 1){
        
        if(TryBuildRoboticsFacility()){

            //std::cout<<"ROBOTICS FAC 1 3:10 "<<std::endl;
        }
    }

    //      32      3:48      Gateway
    if (warpgate_count == 2 &&
        gateway_count == 0 &&
        base_count == 2 &&
        robotics_facility_count >= 1) {
        if (TryBuildGateway()) {
            //std::cout<<"GATEWAY 3 3:48"<<std::endl;
        }
    }

    //      34      4:00      Robotics Bay
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       robotics_facility_count >= 1 &&
       robotics_bay_count == 0 &&
       assimilator_count >= 2){
        if(TryBuildRoboticsBay()){

            //std::cout<<"ROB BAY 4:00"<<std::endl;
        }
    }

    //      49      4:45      Assimilator x2
    if (base_count == 2 &&
        cybernetics_count > 0 &&
        warpgate_count == 3 &&
        robotics_facility_count >= 1 &&
        robotics_bay_count == 1 &&
        assimilator_count < 4) {

        if (TryBuildAssimilator()) {
            //            std::cout<<"ASSIMILATOR x2 4:45"<<std::endl;
        }
    }

    // random shield batteries instead
    if (base_count == 2 &&
        cybernetics_count > 0 &&
        warpgate_count == 3 &&
        robotics_facility_count >= 1 &&
        robotics_bay_count == 1 &&
        assimilator_count == 4 &&
        battery_count < 3) {

        if (TryBuildStructureNearPylon(ABILITY_ID::BUILD_SHIELDBATTERY, UNIT_TYPEID::PROTOSS_PROBE)) {
            //            std::cout<<"BATTERY x3 4:55"<<std::endl;
        }
    }

    //      62      5:35      Nexus
    if(base_count == 2 &&
       cybernetics_count > 0 &&
       warpgate_count >= 3 &&
       robotics_facility_count >= 1 &&
       robotics_bay_count == 1 &&
       battery_count >= 3){
        
        //      62      5:35      Nexus
        if(TryBuildExpansionNexus()){
            std::cout<<"BASE 3 5:35"<<std::endl;
        }
    }

    //      72      6:02      Forge
    if(base_count == 3 &&
       cybernetics_count > 0 &&
       warpgate_count >= 3 &&
       robotics_facility_count >= 1 &&
       robotics_bay_count == 1 &&
       assimilator_count >= 4 &&
       forge_count == 0){
        if(TryBuildForge()){
//            std::cout<<"FORGE 6:02"<<std::endl;

        }

    }

    //      78      6:22      Gateway
    if (base_count == 3 &&
        warpgate_count == 3 &&
        gateway_count == 0 &&
        assimilator_count == 4 &&
        robotics_bay_count == 1 &&
        forge_count == 1) {
        if (TryBuildGateway()) {
            //            std::cout<<"GATEWAY 4 6:22"<<std::endl;
        }
    }

    //      86      6:52      Twilight Council
    if(base_count == 3 &&
       assimilator_count >= 4 &&
       robotics_bay_count == 1 &&
       forge_count == 1 &&
       twilight_count == 0){
//        if (TryBuildUnit(ABILITY_ID::TRAIN_COLOSSUS, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)) {
//            Actions()->UnitCommand(bases.front(), ABILITY_ID::EFFECT_CHRONOBOOST, rfacs.front());
//            //std::cout<<"COLOSSUS 1 6:36"<<std::endl;
//        }
        if(TryBuildTwilightCouncil()){
//            std::cout<<"TWILIGHT 6:52"<<std::endl;

        }
    }

    //      108      7:31      Charge
    if (base_count == 3 &&
        warpgate_count == 4 &&
        assimilator_count <= 6 &&
        robotics_bay_count == 1 &&
        forge_count == 1 &&
        twilight_count == 1) {
        if (TryBuildAssimilator()) {
            //            std::cout<<"GAS 3rd BASE 7:14"<<std::endl;

        }
        if (cannon_count < max_cannon_count) {
            if(TryBuildPhotonCannon()){
                /*std::cout<<"CANNON x5 7:58"<<std::endl;*/
            }
            else
            {
                TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE);
            }
        }
    }

    //      109      7:48      Gateway x2
    //      109      7:58      Gateway
    if (base_count == 3 &&
        warpgate_count < 7 &&
        assimilator_count == 6 &&
        robotics_bay_count == 1 &&
        forge_count == 1 &&
        twilight_count == 1) {
        if (TryBuildGateway()) {
            //            std::cout<<"GATEWAY x3 7:48"<<std::endl;
        }
        if (cannon_count < max_cannon_count) {
            if(TryBuildPhotonCannon()){
                //std::cout<<"CANNON x5 7:58"<<std::endl;
            }
            else
            {
                TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE);
            }
        }
    }

    if (base_count == 3 &&
        warpgate_count >= 7 &&
        assimilator_count == 6 &&
        robotics_bay_count == 1 &&
        forge_count == 1 &&
        twilight_count == 1 &&
        stargate_count == 0) {
        if (TryBuildStargate()) {
            //            std::cout<<"STARGATE"<<std::endl;
        }

        if (cannon_count < max_cannon_count) {
            if (TryBuildPhotonCannon()) {
                std::cout << "CANNON x5 7:58" << std::endl;
            }
            else
            {
                TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE);
            }
        }
    }

    if (base_count == 3 &&
        warpgate_count >= 7 &&
        assimilator_count == 6 &&
        robotics_bay_count == 1 &&
        forge_count == 1 &&
        twilight_count == 1 &&
        stargate_count == 1 &&
        fleet_count == 0) {
        if (TryBuildFleetBeacon()) {
            //            std::cout<<"FLEET"<<std::endl;
        }

        if (cannon_count < max_cannon_count) {
            if (TryBuildPhotonCannon()) {
                std::cout << "CANNON x5 7:58" << std::endl;
            }
            else
            {
                TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE);
            }
        }
    }

    //      149      9:27      Gateway x3, Templar Archives
    if (base_count == 3 &&
        warpgate_count >= 7 &&
        assimilator_count == 6 &&
        robotics_bay_count == 1 &&
        forge_count == 1 &&
        twilight_count == 1 &&
        stargate_count == 1 &&
        fleet_count == 1 &&
        archive_count == 0) {
        TryBuildTemplarArchives();
    }

    //      133      8:38      Nexus
    if (base_count == 3 &&
        warpgate_count >= 7 &&
        assimilator_count == 6 &&
        robotics_bay_count == 1 &&
        forge_count == 1 &&
        twilight_count == 1 &&
        stargate_count == 1 &&
        fleet_count == 1) {
        if (TryBuildExpansionNexus()) {
            std::cout << "BASE 4 8:38" << std::endl;
        }
    }

    // general build order after
    if (base_count >= 4 &&
        warpgate_count <= (base_count * 3) &&
        robotics_facility_count <= (base_count) &&
        gateway_count < 3 &&
        assimilator_count <= (base_count * 2) &&
        twilight_count == 1 &&
        cannon_count < (base_count * 5) &&
        battery_count < (base_count * 3)) {
        TryBuildAssimilator();
        if (warpgate_count < (base_count * 3)) {
            TryBuildGateway();
        }
        if (robotics_facility_count < (base_count)) {
            TryBuildRoboticsFacility();
        }
    }

    if (base_count >= 4 &&
        warpgate_count == (base_count * 3) &&
        robotics_facility_count == (base_count) &&
        assimilator_count == (base_count * 2) &&
        twilight_count == 1 &&
        cannon_count <= (base_count * 5) &&
        battery_count <= (base_count * 3)) {
        if (cannon_count < (base_count * 6)) {
            if (TryBuildPhotonCannon()) {
                std::cout << "CANNON x5 7:58" << std::endl;
            }
            else
            {
                TryBuildStructureNearPylon(ABILITY_ID::BUILD_PHOTONCANNON, UNIT_TYPEID::PROTOSS_PROBE);
            }
        }
        if (battery_count < (base_count * 3)) {
            TryBuildStructureNearPylon(ABILITY_ID::BUILD_SHIELDBATTERY, UNIT_TYPEID::PROTOSS_PROBE);
        }

        if (cannon_count == (base_count * 5) && battery_count == (base_count * 3)) {
            if (TryBuildExpansionNexus()) {
                std::cout << "EXPAND" << std::endl;
            }
        }

    }

    // END BUILD ORDER
}

#pragma endregion


#pragma region Manage Army

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

bool GoingMerry::TryBuildArmy()
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
    if (num_bases < 2 && observation->GetFoodArmy() > 10)
    {
        return false;
    }

    //If we have a decent army already, try hold until we expand again
    if (num_bases < 3 && observation->GetFoodArmy() > 40) {
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
    
    // Ensure we have enough units for scouting
    if (num_zealot < num_scouts && num_stalker < num_scouts)
    {
        if (num_warpgate > 0)
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
        }
        else
        {
            if (num_gateway > 0 && num_cyberneticscore > 0)
            {
                return TryBuildUnit(ABILITY_ID::TRAIN_STALKER, UNIT_TYPEID::PROTOSS_GATEWAY);
            }
            else if (num_gateway > 0)
            {
                return TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
            }
        }
    }

    if (num_zealot < num_harassers)
    {
        if (num_warpgate > 0)
        {
            return TryWarpInUnit(ABILITY_ID::TRAINWARP_ZEALOT);
        }
        else
        {
            if (num_gateway > 0)
            {
                return TryBuildUnit(ABILITY_ID::TRAIN_ZEALOT, UNIT_TYPEID::PROTOSS_GATEWAY);
            }
        }
    }

    if (observation->GetMinerals() > 250 && observation->GetVespene() > 150 && num_stargate > 0 && num_voidray < max_voidray_count) 
    {
        TryBuildUnit(ABILITY_ID::TRAIN_VOIDRAY, UNIT_TYPEID::PROTOSS_STARGATE);
    }
    if (num_phoenix < max_phoenix_count && num_stargate > 0)
    {
        TryBuildUnit(ABILITY_ID::TRAIN_PHOENIX, UNIT_TYPEID::PROTOSS_STARGATE);
    }

    // Train observer units
    if (num_roboticsfacility > 0)
    {
        if (num_observer < 1)
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
        if (observation->GetMinerals() > 250 && observation->GetVespene() > 100)
        {
            if (TryBuildUnit(ABILITY_ID::TRAIN_IMMORTAL, UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY))
            {
                return true;
            }
        }
    }

    // After warpgate is researched
    if (warpgate_reasearched && num_warpgate > 0)
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
        if (num_hightemplar < 2 && num_archon < 2)
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
        if (observation->GetMinerals() > 120 && observation->GetVespene() > 25 && !charge_researched)
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

    //If the unit is doing something besides attacking, make it attack.
    if (unit->orders.front().ability_id != ABILITY_ID::ATTACK) {
        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, position);
    }

    return;
}

void GoingMerry::ManageArmy()
{
    const ObservationInterface* observation = Observation();
    Units visible_enemies = observation->GetUnits(Unit::Alliance::Enemy);
    Units army = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));
    size_t num_immortals = CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL);
    size_t num_colossus = CountUnitType(UNIT_TYPEID::PROTOSS_COLOSSUS);


    //There are no enemies yet, and we don't have a big army
    if (visible_enemies.empty() && army.size() < 10) {

        for (const auto& unit : army)
        {
            if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
                continue;
            }
            if (find(harassers.begin(), harassers.end(), unit) != harassers.end()) {
                continue;
            }

            Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_PATROL, staging_location);
        }
    }

    else if (!visible_enemies.empty()){

        const Unit* target_enemy = visible_enemies.front();

        for (const auto& unit : army) {

            if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
                continue;
            }
            if (find(harassers.begin(), harassers.end(), unit) != harassers.end()) {
                continue;
            }

            if (army.size() > 10 && num_colossus > 1 && num_immortals > 2)
            {
                //cout << "Attacking enemy at (" << target_enemy->pos.x << "," << target_enemy->pos.y << ")" << endl;
                AttackWithUnit(unit, observation, target_enemy->pos);
            }
            else
            {
                DefendWithUnit(unit, observation);
            }

            switch (unit->unit_type.ToType()) {

                //Stalker blink micro, blinks back towards your base
                case(UNIT_TYPEID::PROTOSS_OBSERVER): {
                    
                    if(!enemy_units.empty()){
                        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, enemy_units.front()->pos);
                        break;
                    }
                    break;
                }
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
    else {
        for (const auto& unit : army) {
            if (find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
                //cout << "Skipping scouting unit" << endl;
                continue;
            }
            if (find(harassers.begin(), harassers.end(), unit) != harassers.end()) {
                continue;
            }

            Actions()->UnitCommand(unit, ABILITY_ID::GENERAL_PATROL, staging_location);
        }
    }
}

void GoingMerry::DefendWithUnit(const Unit* unit, const ObservationInterface* observation)
{

    // Safety check: assert unit is not scout or harasser
    if (std::find(scouts.begin(), scouts.end(), unit) != scouts.end()) {
        return;
    }
    if (find(harassers.begin(), harassers.end(), unit) != harassers.end()) {
        return;
    }

    Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
    Units visible_enemies = observation->GetUnits(Unit::Alliance::Enemy);

    for (const auto& base : bases)
    {
        for (const auto& enemy : visible_enemies)
        {
            float d = Distance2D(enemy->pos, base->pos);
            if (d < 10) {
                //cout << "Defending base at (" << base->pos.x << "," << base->pos.y << ")" << endl;
                AttackWithUnit(unit, observation, enemy->pos);
            }
        }
    }
}

bool GoingMerry::BuildAdaptiveUnit(const Unit* reference_unit)
{
    // TODO: Based on reference_unit and enemy army composition, issue build order
    //       for counter unit

    return false;
}

#pragma endregion


#pragma region  Map Analysis

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
    //cout << res[0].x << " " << res[0].y << endl;
    //cout << res[res.size() - 1].x << " " << res[res.size() - 1].y << endl;
    return res;
}

vector<Point2D> GoingMerry::FindRamp(Point3D centre, int range)
{
    vector<Point2D> grid = CalculateGrid(Point2D(centre.x, centre.y), range);
    //cout << "2-1" << endl;
    vector<Point2D> ramp;

    //cout << "grid " << grid.size() << endl;
    for (const auto point : grid)
    {
        //cout << point.x << " " << point.y << endl;
        if (!observation->IsPlacable(point) && observation->IsPathable(point))
        {
            ramp.push_back(point);
        }
    }

    //cout << "ramp " << ramp.size() << endl;

    if (ramp.size() == 0)
    {
        //cout << "2-0" << endl;
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
    //cout << closet.x << " " << closet.y << endl << endl;

    return closet;
}

vector<Point2D> GoingMerry::CalculatePlacableRamp(const Unit* centre)
{
    vector<Point2D> wall;
    auto clostest = FindNearestRampPoint(centre);
    if (clostest.x == -1 && clostest.y == -1)
    {
        //cout << 'NONE' << endl;
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
