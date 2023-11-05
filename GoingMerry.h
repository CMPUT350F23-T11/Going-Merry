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

	const ObservationInterface* observation;

	size_t CountUnitType(UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target, UNIT_TYPEID unit_type);
	bool TryBuildSupplyDepot();
	const Unit* FindNearestMineralPatch(const Point2D& start);
	bool TryBuildForge();
	bool TryBuildCyberneticscore();
	bool TryBuildAssimilator();
	const Unit* FindNearestVespenes(const Point2D& start);
	bool AlreadyBuilt(const Unit* ref, const Units units);
};

#endif