#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include "sc2api/sc2_control_interfaces.h"

#include "GoingMerry.h"
#include "LadderInterface.h"
#include <bits.h>

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

void printStats(int* stats, int n, int num_games)
{
    float win_percent = (float)stats[0] / (float)num_games;
    float loss_percent = (float)stats[1] / (float)num_games;
    float draws_percent = (float)stats[2] / (float)num_games;
    float undecided_percent = (float)stats[3] / (float)num_games;

    cout << "Number of wins: " << stats[0] << ", Win percentage: " << win_percent*100 << "%" << endl;
    cout << "Number of losses: " << stats[1] << ", Loss percentage: " <<  loss_percent*100 << "%" << endl;
    cout << "Number of draws: " << stats[2] << ", Draw percentage: " << draws_percent*100 << "%" << endl;
    cout << "Number of undecided: " << stats[3] << ", Undecided percentage: " << undecided_percent*100 << "%" << endl;
}

#pragma endregion

int main(int argc, char* argv[]) {

    RunBot(argc, argv, new GoingMerry(), sc2::Race::Protoss);
    
	return 0;
}
