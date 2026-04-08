#pragma once
#include "../CCustomCommand.h"

class CCommandAddPlayerIntStat : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
