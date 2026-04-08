#pragma once
#include "../CCustomCommand.h"

class CCommandGetPlayerFloatStat : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
