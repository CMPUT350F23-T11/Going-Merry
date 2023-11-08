#ifndef GOING_MERRY_H_
#define GOING_MERRY_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include <sc2api/sc2_unit_filters.h>
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include <string>
#include <vector>
#include <stdlib.h>

using namespace sc2;
using namespace std;

class GoingMerry : public Agent {
public:
	void OnGameStart();

	void OnStep();

	void OnUnitIdle(const Unit* unit);

private:

	int target_worker_count = 15;
	int max_worker_count = 70;
	GameInfo game_info_;
	vector<Point3D> expansions;
	Point3D startLocation;
	Point3D stagingLocation;
	string last_action_text_;

	bool TryBuildPylon();

	int GoingMerry::GetExpectedWorkers(UNIT_TYPEID vespene_building_type);

	size_t CountUnitType(const ObservationInterface* observation, UnitTypeID unit_type);

	void BuildOrder();

	bool TryExpand(AbilityID build_ability, UnitTypeID worker_type);

	size_t CountUnitType(UNIT_TYPEID unit_type);

	bool TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type);

	bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);

	bool GoingMerry::TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool isExpansion = false);

	bool TryBuildSupplyDepot();

	const Unit* FindNearestMineralPatch(const Point2D& start);

	bool TryBuildExpansionNexus();

	void GoingMerry::OnGameEnd();

	void GoingMerry::MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type);
};

#endif