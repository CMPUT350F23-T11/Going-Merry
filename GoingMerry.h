#ifndef GOING_MERRY_H_
#define GOING_MERRY_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include <sc2api/sc2_unit_filters.h>
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

using namespace sc2;

class GoingMerry : public Agent {
public:
	void OnGameStart();
	void OnStep();
	void OnUnitIdle(const Unit* unit);

private:
	int num_scout = 0;
	std::vector<const Unit*> enemy_units;
	std::vector<const Unit*> enemy_bases;
	size_t CountUnitType(UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV);
	bool TryBuildSupplyDepot();
	const Unit* FindNearestMineralPatch(const Point2D& start);
	bool TryBuildBarracks();
	void SendScouting(const Unit *unit);
	Point2D GetRandomMapLocation();
};

#endif