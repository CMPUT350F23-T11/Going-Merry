#include "GoingMerry.h"
#include <iostream>

using namespace std;

void GoingMerry::OnGameStart()
{
	cout << "Hello, World!" << endl;
}

void GoingMerry::OnStep()
{
	cout << Observation()->GetGameLoop() << endl;
}

