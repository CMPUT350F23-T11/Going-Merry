#ifndef GOING_MERRY_H_
#define GOING_MERRY_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include <sc2api/sc2_unit_filters.h>
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include <math.h>
#include <ctime>
#include <array>

using namespace sc2;
using namespace std;

class GoingMerry : public Agent {
public:
    int max_worker_count_ = 70;
    
	void OnGameStart();
	void OnStep();
	void OnUnitIdle(const Unit* unit);
	void OnUpgradeCompleted(UpgradeID upgrade);

	int ideal_worker_count = 70;
    std::vector<Point3D> expansions_;

private:

#pragma region Data

	bool debug = false;  // Toggle debug statements

	int enemy_race = -1;  // -1: Not yet determined, 0: Protoss, 1: Terran, 2: Zerg
	bool enemy_air_units = false;
	bool foundBase = false;
	bool launchedHarass = false;
	bool launchedAttack = false;

	const ObservationInterface* observation;
	bool warpgate_researched = false;
    bool thermal_lance_researched = false;
    bool ground_wep_1_researched = false;
    bool ground_wep_2_researched = false;
	bool blink_researched = false;
	bool charge_researched = false;

	std::vector<const Unit*> enemy_units;
	std::vector<const Unit*> scouts;
	std::vector<const Unit*> harassers;
	std::vector<sc2::Point2D> visitedLocations;
	std::vector<Point3D> expansions;
	std::vector<Point2D> base_locations;


	bool startHarass = false;

	const Unit* scouting_probe = nullptr;
	int possible_starts_visited = 0;
	int num_scouts = 2;
	int num_harassers = 10;
	int target_worker_count = 15;
	int max_zealot_count = 15;  // Supply: 2
	int max_stalker_count = 15;  // Supply: 2
	int max_immortal_count = 10;  // Supply: 4
	int max_colossus_count = 5;  // Supply: 6
	int max_voidray_count = 5;  // Supply: 4
	int max_phoenix_count = 5;  // Supply: 2
	int max_archon_count = 10;  // Supply: 4
	int max_cannon_count = 10;
	int max_observer_count = 1;
	int max_sentry_count = 2;  // Supply: 2
		
	vector<int> directionX{ 1,-1,0,0,1,1,-1,-1 };
	vector<int> directionY{ 0,0,1,-1,1,-1,1,-1 };

	GameInfo game_info;

	Point3D start_location;
	Point3D staging_location;
	
#pragma endregion

#pragma region basic building function

	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type, bool is_expansion = false);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type, bool is_expansion = false);
	bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool is_expansion = false);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D pylon, float radius, UNIT_TYPEID unit_type);
	//bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D pylon, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target, UNIT_TYPEID unit_type);
    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);
    
    //Try build structure given a location. This is used most of the time
    bool TryBuildStructureForPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool isExpansion = false);
    bool TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type);
    
    bool TryBuildProbe();
    //An estimate of how many workers we should have based on what buildings we have
    int GetExpectedWorkers(UNIT_TYPEID vespene_building_type);
#pragma endregion

#pragma region  assistant functions

    void MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type);
    void ManageWorkers(UNIT_TYPEID worker_type, AbilityID worker_gather_command, UNIT_TYPEID vespene_building_type);
    
	size_t CountUnitType(UNIT_TYPEID unit_type);
	size_t CountEnemyUnitType(UNIT_TYPEID unit_type);
	const Unit* FindNearestMineralPatch(const Point2D& start);
	//bool AlreadyBuilt(const Unit* ref, const Units units);
	//bool help(const Point2D& point);
	bool IsNextToRamp(Point2D point);
	bool HavePylonNearby(Point2D& point);
	bool HaveCannonNearby(Point2D& point);
	Point2D FindClostest(Point2D nux, vector<Point2D> position);
	vector<Point2D> GetOffSetPoints(Point2D point, UNIT_TYPEID unit_type);

#pragma endregion

#pragma region Build structure functions

	bool TryExpandBase(ABILITY_ID build_ability, UnitTypeID unit_type);
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
    
    bool TryBuildUnit(AbilityID ability_type_for_unit, UnitTypeID unit_type);
    bool GetRandomUnit(const Unit*& unit_out, const ObservationInterface* observation, UnitTypeID unit_type);
    bool TryWarpInUnit(ABILITY_ID ability_type_for_unit);

#pragma endregion

#pragma region  worker command

	//void Mine(const Unit* unit, const Unit* nexus);
	//void CollectVespeneGas(const Unit* unit, const Unit* assimilator);
	//void WorkerHub(const Unit* unit);

#pragma endregion

#pragma region strategy

	void BuildOrder(float ingame_time, uint32_t current_supply,uint32_t current_minerals, uint32_t current_gas);

	void TrySendScouts();
	void SendScouting();
	sc2::Point2D GetScoutMoveLocation();
	void MoveScouts();
	void SendHarassing(const sc2::Unit *base);
	void TrySendHarassing(const sc2::Unit *base);
	void CheckIfAlive(int idetifier);

	void FindEnemyRace();

#pragma endregion

#pragma region map analysis

	Point2D GetRandomMapLocation();
	vector<Point2D> CalculateGrid(Point2D centre, int range);
	vector<Point2D> FindRamp(Point3D centre, int range);
	Point2D FindNearestRampPoint(const Unit* centre);
	vector<Point2D> CalculatePlacableRamp(const Unit* centre);
	bool IsNextToCliff(const Point2D location);

#pragma endregion

# pragma region manage army

	void ManageUpgrades();
	bool TryBuildArmy();
	void TryBuildBaseArmy();
	bool TryBuildAdaptiveArmy();

	void ManageArmy();
	void AttackWithUnit(const Unit* unit, const ObservationInterface* observation, Point2D position);
	void DefendWithUnit(const Unit* unit, const ObservationInterface* observation);  // TODO
	bool BuildAdaptiveUnit(const UNIT_TYPEID reference_unit, ABILITY_ID ability_type, UNIT_TYPEID production_structure_type, bool warp = false);

#pragma endregion

#pragma region Debug Tools

	void printLog(string message, bool step = false);

#pragma endregion

};

#endif
