#pragma once
#include "../CCustomCommand.h"

class CCommandGetPlayerIntStat : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
