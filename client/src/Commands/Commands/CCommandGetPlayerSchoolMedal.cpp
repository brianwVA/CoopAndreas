#include "stdafx.h"
#include "CCommandGetPlayerSchoolMedal.h"

void CCommandGetPlayerSchoolMedal::Process(CRunningScript* script)
{
	script->CollectParameters(1);

	const int medalIndex = ScriptParams[0];
	ScriptParams[0] = CStatsSync::GetSchoolMedal(medalIndex);

	script->StoreParameters(1);
}
