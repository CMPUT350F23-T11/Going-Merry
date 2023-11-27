#ifndef GOING_MERRY_H_
#define GOING_MERRY_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include <sc2api/sc2_unit_filters.h>
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include <ctime>

using namespace sc2;

class GoingMerry : public Agent {
public:
	void OnGameStart();
	void OnStep();
	void OnUnitIdle(const Unit* unit);
    std::vector<Point3D> expansions_;

private:

	const ObservationInterface* observation;

	size_t CountUnitType(UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type);
    //Try build structure given a location. This is used most of the time
    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool isExpansion);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type, bool is_expansion = false);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D pylon, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D pylon, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target, UNIT_TYPEID unit_type);
    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);
    bool TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type);
	bool TryExpandBase(ABILITY_ID build_ability, UnitTypeID unit_type);

    
    void MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type);
    void ManageWorkers(UNIT_TYPEID worker_type, AbilityID worker_gather_command, UNIT_TYPEID vespene_building_type);
    
	const Unit* FindNearestMineralPatch(const Point2D& start);
	const Unit* FindNearestVespenes(const Point2D& start);
	
	bool StillNeedingWorkers();
	bool AlreadyBuilt(const Unit* ref, const Units units);
	
	bool TryBuildForge();
	bool TryBuildCyberneticsCore();
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
	bool TryBuildExpansionNexus();
    
    bool TryWarpInUnit(ABILITY_ID ability_type_for_unit);

	void Mine(const Unit* unit,const Unit* nexus);
	void CollectVespeneGas(const Unit* unit, const Unit* assimilator);
	void WorkerHub(const Unit* unit);

	std::vector<const Unit *> enemy_units;
	std::vector<const Unit *> enemy_bases;
	std::vector<const Unit *> scouts;

	GameInfo game_info;
	Point3D start_location;
	Point3D staging_location;
    std::array<Point2D, 1> second_base;
	std::vector<Point3D> expansions;
	int target_worker_count = 15;

	void BuildOrder(float ingame_time, uint32_t current_supply,uint32_t current_minerals, uint32_t current_gas);

	void TrySendScouts();
	void SendScouting();
	sc2::Point2D GetRandomMapLocation();

};

#endif
