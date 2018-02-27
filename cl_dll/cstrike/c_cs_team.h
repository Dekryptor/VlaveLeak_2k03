//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Client side CTFTeam class
//
// $NoKeywords: $
//=============================================================================

#ifndef C_CS_TEAM_H
#define C_CS_TEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "c_team.h"
#include "shareddefs.h"
#include "imessagechars.h"

class C_BaseEntity;
class C_BaseObject;
class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class C_CSTeam : public C_Team
{
public:
	DECLARE_CLASS( C_CSTeam, C_Team );

	DECLARE_CLIENTCLASS();

					C_CSTeam();
	virtual			~C_CSTeam();
};


#endif // C_CS_TEAM_H
