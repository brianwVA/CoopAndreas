#include "stdafx.h"
#include "CCommandAddPlayerFloatStat.h"

void CCommandAddPlayerFloatStat::Process(CRunningScript* script)
{
	script->CollectParameters(2);

	const int statIndex = ScriptParams[0];
	const float delta = *reinterpret_cast<float*>(&ScriptParams[1]);

	CStatsSync::AddFloatStat(statIndex, delta);
}
