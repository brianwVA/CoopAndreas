#pragma once
#include "../CCustomCommand.h"

class CCommandSetPlayerFloatStat : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
