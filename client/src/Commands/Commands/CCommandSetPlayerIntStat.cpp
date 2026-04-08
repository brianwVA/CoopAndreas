#include "stdafx.h"
#include "CCommandSetPlayerIntStat.h"

void CCommandSetPlayerIntStat::Process(CRunningScript* script)
{
	script->CollectParameters(2);

	const int statIndex = ScriptParams[0];
	const int value = ScriptParams[1];

	CStatsSync::SetIntStat(statIndex, value);
}
