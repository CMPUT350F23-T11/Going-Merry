#include "GoingMerry.h"
#include <iostream>

using namespace sc2;
using namespace std;

void GoingMerry::OnGameStart() { return; }

void GoingMerry::OnStep()
{
    // std::cout << Observation()->GetGameLoop() << std::endl;
    TryBuildSupplyDepot();
    TryBuildBarracks();
    CheckScouts();
   

}


void GoingMerry::OnUnitIdle(const Unit* unit)
{
    switch (unit->unit_type.ToType())
    {
    case UNIT_TYPEID::TERRAN_COMMANDCENTER:
    {
        Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
        break;
    }
    case UNIT_TYPEID::TERRAN_SCV: {
        if (Observation()->GetUnits(Unit::Alliance::Self, sc2::IsUnit(UNIT_TYPEID::TERRAN_SCV)).size() < 16 || scouts.size() > 2)
        {
            const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
            if (!mineral_target) {
                break;
            }
            Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            break;
        }
        else
        {
            scouts.push_back(unit);
            SendScouting(unit);
        }
        
    }
    case UNIT_TYPEID::TERRAN_BARRACKS: {
        Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
        break;
    }
    case UNIT_TYPEID::TERRAN_MARINE: {
        const GameInfo& game_info = Observation()->GetGameInfo();
        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
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

bool GoingMerry::TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type) {
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

    float rx = GetRandomScalar();
    float ry = GetRandomScalar();

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

    return true;

}

bool GoingMerry::TryBuildSupplyDepot() {
    const ObservationInterface* observation = Observation();

    // If we are not supply capped, don't build a supply depot.
    if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
        return false;

    // Try and build a depot. Find a random SCV and give it the order.
    return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
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

bool GoingMerry::TryBuildBarracks() {
    const ObservationInterface* observation = Observation();
    if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
        return false;
    }
    if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) > 0) {
        return false;
    }

    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
}

void GoingMerry::SendScouting(const Unit *unit) {
    
    // goes to a random location on the map to increase observable area
    Point2D target_location = GetRandomMapLocation();
    cout << "Target location: " << target_location.x << ", " << target_location.y << endl;
    Actions()->UnitCommand(unit, ABILITY_ID::SMART, target_location);

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

sc2::Point2D GoingMerry::GetRandomMapLocation() {
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

void GoingMerry::CheckScouts() {
    if (scouts.size() == 0)
    {
        return;
    }

    auto it = scouts.begin();
    while (it != scouts.end())
    {
        if ((*it)->is_alive)
        {
            SendScouting(*it);
            ++it;
        }
        else
        {
            it = scouts.erase(it);
        }
    }
}