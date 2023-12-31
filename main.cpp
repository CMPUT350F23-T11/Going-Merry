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

#pragma endregion

int main(int argc, char* argv[]) {

	RunBot(argc, argv, new GoingMerry(), sc2::Race::Protoss);
    
	return 0;
}
