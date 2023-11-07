#include "GoingMerry.h"
#include <iostream>

using namespace sc2;
using namespace std;

void GoingMerry::OnGameStart() { 
    observation = Observation();
    return; 
}

void GoingMerry::OnStep() 
{ 
    std::cout << Observation()->GetGameLoop() << std::endl;
    TryBuildPylon();
    TryBuildAssimilator();
    TryBuildForge();
    TryBuildCyberneticScore();
    TryBuildGateway();
    TryBuildTwilightCouncil();
    TryBuildStargate();
    TryBuildRoboticsFacility();
    TryBuildFleetBeacon();
    TryBuildDarkShrine();
    TryBuildTemplarArchives();
    TryBuildRoboticsBay();
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
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_ZEALOT);
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

    for (const auto& assimilator : allAssimilator)
    {
        if (assimilator->assigned_harvesters < assimilator->ideal_harvesters)
        {
            CollectVespeneGas(unit, assimilator);
            return;
        }
    }
    for (const auto& nexus : allNexus)
    {
        if (nexus->assigned_harvesters < nexus->ideal_harvesters)
        {
            Mine(unit,nexus);
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

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

    return true;

}

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
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

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        position);

    return true;

}

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
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

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        position);

    return true;

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

    float rx = GetRandomScalar();
    float ry = GetRandomScalar();

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        position);

    return true;

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

    float rx = GetRandomScalar();
    float ry = GetRandomScalar();

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        position);

    return true;

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

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        target);

    return true;

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
        return false;
    if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) > 0) {
        return false;
    }

    return TryBuildStructure(ABILITY_ID::BUILD_FORGE);
}

bool GoingMerry::TryBuildAssimilator()
{
    const Unit* target = nullptr;

    target = FindNearestVespenes(observation->GetStartLocation());
    if (!target)
    {
        return false;
    }

    return TryBuildStructure(ABILITY_ID::BUILD_ASSIMILATOR, target);
}

bool GoingMerry::TryBuildPylon() {
    if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
        return false;


    return TryBuildStructure(ABILITY_ID::BUILD_PYLON);
}

bool GoingMerry::TryBuildCyberneticScore()
{
    if (CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) < 1)
    {
        return false;
    }    
    if (CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) > 0)
    {
        return false;
    }
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
    if (CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) > 0)
    {
        return false;
    }
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

bool GoingMerry::TryExpendBase()
{

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

