#pragma once
#include "../CCustomCommand.h"

class CCommandSetPlayerIntStat : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
