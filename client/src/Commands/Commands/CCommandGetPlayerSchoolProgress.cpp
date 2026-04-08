#include "stdafx.h"
#include "CCommandGetPlayerSchoolProgress.h"

void CCommandGetPlayerSchoolProgress::Process(CRunningScript* script)
{
	script->CollectParameters(1);

	const int schoolIndex = ScriptParams[0];
	ScriptParams[0] = CStatsSync::GetSchoolProgress(schoolIndex);

	script->StoreParameters(1);
}
