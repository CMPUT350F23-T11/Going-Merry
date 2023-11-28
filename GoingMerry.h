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

using namespace sc2;
using namespace std;

class GoingMerry : public Agent {
public:
	void OnGameStart();
	void OnStep();
	void OnUnitIdle(const Unit* unit);

private:

#pragma region private data

	const ObservationInterface* observation;

	std::vector<Point3D> expansions;
	std::vector<const Unit*> enemy_units;
	std::vector<const Unit*> enemy_bases;
	std::vector<const Unit*> scouts;

	vector<int> directionX{ 1,-1,0,0,1,1,-1,-1 };
	vector<int> directionY{ 0,0,1,-1,1,-1,1,-1 };

	GameInfo game_info;

	Point3D start_location;
	Point3D staging_location;
	
	int target_worker_count = 15;

#pragma endregion

#pragma region basic building function

	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D position, UNIT_TYPEID unit_type, bool is_expansion);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D position, UNIT_TYPEID unit_type, bool is_expansion);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point2D pylon, float radius, UNIT_TYPEID unit_type);
	//bool TryBuildStructure(ABILITY_ID ability_type_for_structure, Point3D pylon, float radius, UNIT_TYPEID unit_type);
	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, const Unit* target, UNIT_TYPEID unit_type);
    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);
	bool TryExpandBase(ABILITY_ID build_ability, UnitTypeID unit_type);

#pragma endregion

#pragma region  assistant functions

    
    void MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type);
    void ManageWorkers(UNIT_TYPEID worker_type, AbilityID worker_gather_command, UNIT_TYPEID vespene_building_type);
    
	size_t CountUnitType(UNIT_TYPEID unit_type);
	const Unit* FindNearestMineralPatch(const Point2D& start);
	//const Unit* FindNearestVespenes(const Point2D& start);
	bool StillNeedingWorkers();
	//bool AlreadyBuilt(const Unit* ref, const Units units);
	//bool help(const Point2D& point);
	bool IsNextToRamp(Point2D point);
	bool HavePylonNearby(Point2D& point);
	Point2D FindClostest(Point2D nux, vector<Point2D> position);
	vector<Point2D> GetOffSetPoints(Point2D point, UNIT_TYPEID unit_type);

#pragma endregion

#pragma region Buil structure functions

	//bool TryExpandBase(ABILITY_ID build_ability, UnitTypeID unit_type);
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

#pragma endregion

#pragma region  worker command

	//void Mine(const Unit* unit, const Unit* nexus);
	//void CollectVespeneGas(const Unit* unit, const Unit* assimilator);
	//void WorkerHub(const Unit* unit);

#pragma endregion

#pragma region strategy

	void BuildOrder();
	void TrySendScouts();
	void SendScouting();

#pragma endregion

#pragma region map analysis

	Point2D GetRandomMapLocation();
	vector<Point2D> CalculateGrid(Point2D centre, int range);
	vector<Point2D> FindRamp(Point3D centre, int range);
	Point2D FindNearestRampPoint(const Unit* centre);
	vector<Point2D> CalculatePlacableRamp(const Unit* centre);
	bool IsNextToClif(const Point2D location);

#pragma endregion


};

#endif
