#include "stdafx.h"

#include "Context.h"

void MainContext::EnableAutoFix()
{
	std::string exe_name = ModuleNameA(NULL);
	std::transform(exe_name.begin(), exe_name.end(), exe_name.begin(), std::tolower);

	if (exe_name == "game.exe" || exe_name == "game.dat")
	{
		autofix = RESIDENT_EVIL_4;
		PrintLog("AutoFix for \"Resident Evil 4\" enabled");
	}

	if (exe_name == "kb.exe")
	{
		autofix = KINGS_BOUNTY_LEGEND;
		PrintLog("AutoFix for \"Kings Bounty: Legend\" enabled");
	}

	if (exe_name == "ffxiiiimg.exe")
	{
		autofix = FINAL_FANTASY_XIII;
		PrintLog("AutoFix for \"Final Fantasy XIII\" enabled");
	}

	if (exe_name == "a17.exe")
	{
		autofix = ATELIER_SOPHIE;
		PrintLog("AutoFix for \"Atelier Sophie\" enabled");
	}
}

/*const std::map<const MainContext::AutoFixes, const uint32_t> MainContext::behaviorflags_fixes =
{
	{ RESIDENT_EVIL_4, D3DCREATE_SOFTWARE_VERTEXPROCESSING },
	{ KINGS_BOUNTY_LEGEND, D3DCREATE_MIXED_VERTEXPROCESSING },
};*/

void MainContext::FixBehaviorFlagConflict(const DWORD flags_in, DWORD* flags_out)
{
	if (flags_in & D3DCREATE_SOFTWARE_VERTEXPROCESSING)
	{
		*flags_out &= ~(D3DCREATE_PUREDEVICE | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MIXED_VERTEXPROCESSING);
		*flags_out |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}
	else if (flags_in & D3DCREATE_MIXED_VERTEXPROCESSING)
	{
		*flags_out &= ~(D3DCREATE_PUREDEVICE | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_SOFTWARE_VERTEXPROCESSING);
		*flags_out |= D3DCREATE_MIXED_VERTEXPROCESSING;
	}
	else if (flags_in & D3DCREATE_HARDWARE_VERTEXPROCESSING)
	{
		*flags_out &= ~(D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_SOFTWARE_VERTEXPROCESSING);
		*flags_out |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	}
}
/*
bool MainContext::ApplyBehaviorFlagsFix(DWORD* flags)
{
	if (autofix == 0) return false;

	auto && fix = behaviorflags_fixes.find(autofix);
	if (fix != behaviorflags_fixes.end())
	{
		FixBehaviorFlagConflict(fix->second, flags);
		return true;
	}

	return false;
}
*/
bool MainContext::ApplyVertexBufferFix(UINT& Length, DWORD& Usage, DWORD& FVF, D3DPOOL& Pool, IDirect3DVertexBuffer9** ppVertexBuffer)
{
	if (autofix == 0) return false;

	// Final Fantasy XIII
	if (autofix == FINAL_FANTASY_XIII)
	{
		if (Length == 358400 && FVF == 0 && Pool == D3DPOOL_MANAGED) { Usage = D3DUSAGE_DYNAMIC; Pool = D3DPOOL_DEFAULT; }
		return true;
	}
	else if (autofix == ATELIER_SOPHIE)
	{
		if (Length == 2800000 && FVF == 0 && Pool == D3DPOOL_MANAGED)
		{
			if (theVertexBuffer == 0)
			{
				theVertexBuffer = ppVertexBuffer;
			}
			if (fixFlags == 0)
			{
				PrintLog("Vertex Buffer fix for \"Atelier Sophie\" applied");
				fixFlags = 1;
			}
			Usage = D3DUSAGE_DYNAMIC; //always use this mode - only because we can save/restore the vertex buffer lol.
			Pool = D3DPOOL_DEFAULT;
		}
		return true;
	}
	return false;
}
void MainContext::A17_ResetCallback()
{
	if (autofix == ATELIER_SOPHIE)
	{
		if (theVertexBuffer != 0)
			(*theVertexBuffer)->Release();
	}
}
IDirect3DVertexBuffer9 ** MainContext::get_Atelier_Sophie_VB()
{
	if (autofix == ATELIER_SOPHIE)
		return theVertexBuffer;
	return 0;
}