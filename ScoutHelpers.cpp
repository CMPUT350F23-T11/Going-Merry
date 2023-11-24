#include "GoingMerry.h"
#include <iostream>

using namespace sc2;
using namespace std;

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
