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
    // Data Members
	const ObservationInterface* observation;

    // Methods
	size_t CountUnitType(UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D pylon, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D pylon, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target, UNIT_TYPEID unit_type);
    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);

	const Unit* FindNearestMineralPatch(const Point2D& start);
	const Unit* FindNearestVespenes(const Point2D& start);
	
	bool StillNeedingWorkers();
	bool AlreadyBuilt(const Unit* ref, const Units units);
	
	bool TryBuildForge();
	bool TryBuildCyberneticScore();
    
	bool TryBuildAssimilator();
    bool TryBuildGas(AbilityID build_ability, UnitTypeID worker_type, Point2D base_location);
    
	bool TryBuildPylon();
	bool TryBuildDarkShrine();
	bool TryBuildFleetBeacon();
	bool TryBuildGateway();
	bool TryBuildPhotonCannon();
	bool TryBuildRoboticsFacility();
	bool TryBuildStargate();
	bool TryBuildTemplarArchives();
	bool TryBuildTwilightCouncil();
	bool TryBuildWarpGate();
	bool TryBuildShieldBattery();
	bool TryBuildStasisWard();
	bool TryBuildRoboticsBay();
	bool TryExpandBase();

	void Mine(const Unit* unit,const Unit* nexus);
	void CollectVespeneGas(const Unit* unit, const Unit* assimilator);
	void WorkerHub(const Unit* unit);
    
    void ManageWorkers(UNIT_TYPEID worker_type, AbilityID worker_gather_command, UNIT_TYPEID vespene_building_type);
    void MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type);
};

#endif
