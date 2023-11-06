#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include "GoingMerry.h"
#include "LadderInterface.h"

using namespace sc2;
using namespace std;

// LadderInterface allows the bot to be tested against the built-in AI or
// played against other bots
<<<<<<< Updated upstream
int main(int argc, char* argv[]) 
{
    RunBot(argc, argv, new GoingMerry(), Race::Terran);

    return 0;
=======
int main(int argc, char* argv[]) {
	RunBot(argc, argv, new GoingMerry(), sc2::Race::Protoss);
	return 0;
>>>>>>> Stashed changes
}