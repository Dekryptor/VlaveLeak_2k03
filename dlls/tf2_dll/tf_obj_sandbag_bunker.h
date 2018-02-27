//========= Copyright � 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A stationary sandbag bunker that players can take cover in.
//
//=============================================================================

#ifndef TF_OBJ_SANDBAG_BUNKER_H
#define TF_OBJ_SANDBAG_BUNKER_H
#ifdef _WIN32
#pragma once
#endif

// ------------------------------------------------------------------------ //
// Purpose: A stationary sandbag bunker that players can take cover in.
// ------------------------------------------------------------------------ //
class CObjectSandbagBunker : public CBaseObject
{
public:
	DECLARE_CLASS( CObjectSandbagBunker, CBaseObject );

	DECLARE_SERVERCLASS();

	CObjectSandbagBunker( void );

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	SetObjectCollisionBox( void );
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );

private:
};


#endif // TF_OBJ_TOWER_H
