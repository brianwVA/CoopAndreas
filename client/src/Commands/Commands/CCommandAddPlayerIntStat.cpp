#include "stdafx.h"
#include "CCommandAddPlayerIntStat.h"

void CCommandAddPlayerIntStat::Process(CRunningScript* script)
{
	script->CollectParameters(2);

	const int statIndex = ScriptParams[0];
	const int delta = ScriptParams[1];

	CStatsSync::AddIntStat(statIndex, delta);
}
