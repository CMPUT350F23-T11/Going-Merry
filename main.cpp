#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include "sc2api/sc2_control_interfaces.h"

#include "GoingMerry.h"
#include "LadderInterface.h"

using namespace sc2;
using namespace std;

// LadderInterface allows the bot to be tested against the built-in AI or
// played against other bots

#pragma region Helper functions

void getStats(vector<int> results, int* stats)
{
    for (const auto& result : results)
    {
        stats[result] += 1;
    }
}

void printStats(int* stats, int n)
{
    float win_percent = (float)stats[0] / (float)100;
    float loss_percent = (float)stats[1] / (float)100;
    float draws_percent = (float)stats[2] / (float)100;
    float undecided_percent = (float)stats[3] / (float)100;

    cout << "Number of wins: " << stats[0] << ", Win percentage: " << win_percent << "%" << endl;
    cout << "Number of losses: " << stats[1] << ", Loss percentage: " <<  loss_percent << "%" << endl;
    cout << "Number of draws: " << stats[2] << ", Draw percentage: " << draws_percent << "%" << endl;
    cout << "Number of undecided: " << stats[3] << ", Undecided percentage: " << undecided_percent << "%" << endl;
}

#pragma endregion

int main(int argc, char* argv[]) {

//    RunBot(argc, argv, new GoingMerry(), sc2::Race::Protoss);
    
    // RunBot doesn't work for me, I use below instead (sam)
    //  -------------------------------------------------
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);

     GoingMerry bot;
     coordinator.SetParticipants({
         CreateParticipant(Race::Protoss, &bot),
         CreateComputer(Race::Zerg)
     });

     coordinator.LaunchStarcraft();
     coordinator.StartGame(sc2::kMapBelShirVestigeLE);

     while (coordinator.Update()) {
     }
    // -------------------------------------------------
    
    // Code for report metrics
    // To run tests, uncomment lines 147-158 in LadderInterface.h and
    // comment out lines 182-186 in sc2_replay_observer.cc at cpp-sc2/src/sc2api/.

    // vector<int> results;
    // for (int i = 0; i < 100; ++i)
    // {
    //     RunBot(argc, argv, new GoingMerry(), sc2::Race::Protoss);

    //     ReplayObserver replay_observer;
    //     ReplayControlInterface *replay_control =  replay_observer.ReplayControl();

    //     InterfaceSettings settings;
    //     settings.use_feature_layers = false;
    //     settings.use_render = false;

    //     replay_control->LoadReplay("C:/SC2/Replays/001.SC2Replay", settings, 1, false);
    //     ReplayInfo replay_info = replay_control->GetReplayInfo();
    //     ReplayPlayerInfo player_info;
    //     replay_info.GetPlayerInfo(player_info, 1);

    //     cout << "Game " << i+1 << " Result: " << player_info.game_result << endl;  // 0, 1, 2, 3: Win, Loss, Tie, Undecided
    //     results.push_back(player_info.game_result);
    // }

    // int stats[4] = { 0, 0, 0, 0 };
    // getStats(results, stats);

    // printStats(stats, 4);

	return 0;
}
