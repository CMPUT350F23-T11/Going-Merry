#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include "GoingMerry.h"
#include "LadderInterface.h"

// LadderInterface allows the bot to be tested against the built-in AI or
// played against other bots
int main(int argc, char* argv[]) {
//	RunBot(argc, argv, new GoingMerry(), sc2::Race::Protoss);
    
    // RunBot doesn't work for me, so I use this instead (sam)
    //------------------------------------------------------------
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
    //------------------------------------------------------------
	return 0;
}
