#pragma once
#include "../CCustomCommand.h"

class CCommandGetPlayerSchoolProgress : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
