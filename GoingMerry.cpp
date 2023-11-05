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
    //std::cout << Observation()->GetGameLoop() << std::endl;
    TryBuildSupplyDepot();
    TryBuildForge();
    TryBuildAssimilator();
}


void GoingMerry::OnUnitIdle(const Unit* unit)
{
    switch (unit->unit_type.ToType())
    {
        case UNIT_TYPEID::PROTOSS_NEXUS:
        {
            
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);
            break;
        }
        case UNIT_TYPEID::PROTOSS_PROBE: {
            const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
            if (!mineral_target) {
                break;
            }
            Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            break;
        }

        default:
        {
            break;
        }
    }
}

size_t GoingMerry::CountUnitType(UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
}


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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure,Point2D position, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, float radius, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, float radius, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target,  UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
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

bool GoingMerry::AlreadyBuilt(const Unit* ref, const Units units)
{
    for (const auto& unit : units)
    {
        if (ref->pos == unit->pos)
            return true;
    }
    return false;
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

bool GoingMerry::TryBuildCyberneticscore()
{
    return false;
}

bool GoingMerry::TryBuildSupplyDepot() {
    //const ObservationInterface* observation = Observation();
    // If we are not supply capped, don't build a supply depot.
    if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
        return false;

    // Try and build a depot. Find a random SCV and give it the order.
    return TryBuildStructure(ABILITY_ID::BUILD_PYLON);
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

bool GoingMerry::TryBuildForge() {
    //const ObservationInterface* observation = Observation();
    //if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) < 1) {
    //    return false;
    //}
    if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) > CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS)) {
        return false;
    }

    return TryBuildStructure(ABILITY_ID::BUILD_FORGE);
}