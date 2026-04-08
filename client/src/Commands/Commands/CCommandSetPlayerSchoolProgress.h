#pragma once
#include "../CCustomCommand.h"

class CCommandSetPlayerSchoolProgress : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
