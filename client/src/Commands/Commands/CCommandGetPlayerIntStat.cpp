#include "stdafx.h"
#include "CCommandGetPlayerIntStat.h"

void CCommandGetPlayerIntStat::Process(CRunningScript* script)
{
	script->CollectParameters(1);

	const int statIndex = ScriptParams[0];
	ScriptParams[0] = CStatsSync::GetIntStat(statIndex);

	script->StoreParameters(1);
}
