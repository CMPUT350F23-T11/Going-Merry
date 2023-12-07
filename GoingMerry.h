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

#pragma region Game Managers

	void OnGameStart();
	void OnStep();
	void OnUnitIdle(const Unit* unit);
	void OnUpgradeCompleted(UpgradeID upgrade);

#pragma endregion

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
	int ideal_worker_count = 70;
	int max_worker_count_ = 70;
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
	std::vector<Point3D> expansions_;
	
#pragma endregion

#pragma region basic building function

	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type, bool is_expansion = false);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type, bool is_expansion = false);
	bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool is_expansion = false);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D pylon, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target, UNIT_TYPEID unit_type);
    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);
    
    //Try build structure given a location. This is used most of the time
    bool TryBuildStructureForPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location, bool isExpansion = false);
    bool TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type);
    bool TryBuildProbe();
    //An estimate of how many workers we should have based on what buildings we have
    int GetExpectedWorkers(UNIT_TYPEID vespene_building_type);
#pragma endregion

#pragma region  Assistant Functions

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

#pragma region Build Structure Functions

	bool TryExpandBase(ABILITY_ID build_ability, UnitTypeID unit_type);

	bool TryBuildForge();

	bool TryBuildCyberneticsCore();

	bool TryBuildAssimilator();

    bool TryBuildGas(AbilityID build_ability, UnitTypeID worker_type, Point2D base_location);

	bool TryBuildPylon();

	bool TryBuildFleetBeacon();

	bool TryBuildGateway();

	bool TryBuildPhotonCannon();

	bool TryBuildRoboticsFacility();

	bool TryBuildStargate();

	bool TryBuildTemplarArchives();

	bool TryBuildTwilightCouncil();

	bool TryBuildWarpGate();

	bool TryBuildShieldBattery();

	bool TryBuildRoboticsBay();

	bool TryBuildExpansionNexus();

    bool TryBuildUnit(AbilityID ability_type_for_unit, UnitTypeID unit_type);

    bool TryWarpInUnit(ABILITY_ID ability_type_for_unit);

#pragma endregion

#pragma region Try Send Scouting and Harassers

	void TrySendScouts();  // Check if possible to send scouts

	void SendScouting();  // Send scouts to points of interest

	sc2::Point2D GetScoutMoveLocation();  // Generate a target location for scouts

	void MoveScouts();

	void SendHarassing(const sc2::Unit *base);  // Send harassment units to base

	void TrySendHarassing(const sc2::Unit *base);  // Check if possible to send harassers

	void CheckIfAlive(int idetifier);  // Check if unit is alive by unique identifier

	void FindEnemyRace();

#pragma endregion

#pragma region Map Analysis

	Point2D GetRandomMapLocation();

	vector<Point2D> CalculateGrid(Point2D centre, int range);

	vector<Point2D> FindRamp(Point3D centre, int range);

	Point2D FindNearestRampPoint(const Unit* centre);

	vector<Point2D> CalculatePlacableRamp(const Unit* centre);

	bool IsNextToCliff(const Point2D location);

#pragma endregion

# pragma region Manage Army

	void ManageUpgrades();  // Manage upgrades for bot

	bool TryBuildArmy();  // Build original fixed army composition

	void TryBuildBaseArmy();  // Build basic army of Zealots and Stalkers (changes as facilities are unlocked)

	bool TryBuildAdaptiveArmy();  // Build adaptive army based on information obtained about enemies

	void ManageArmy();  // Manage bot's army units

	void AttackWithUnit(const Unit* unit, const ObservationInterface* observation, Point2D position);  // Manage bot's attack logic

	void DefendWithUnit(const Unit* unit, const ObservationInterface* observation);  // Manage bot's defence logic

	bool BuildAdaptiveUnit(const UNIT_TYPEID reference_unit, ABILITY_ID ability_type, UNIT_TYPEID production_structure_type, bool warp = false);  // Build equivalent unit to reference based on adaptive logic

#pragma endregion

#pragma region Debug Tools

	void printLog(string message, bool step = false);  // Use to print debug statements

#pragma endregion

};

#endif
