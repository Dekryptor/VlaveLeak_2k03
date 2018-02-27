//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Defines the client-side representation of CBaseCombatCharacter.
//
// $NoKeywords: $
//=============================================================================

#ifndef C_BASECOMBATCHARACTER_H
#define C_BASECOMBATCHARACTER_H

#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "c_baseflex.h"

class C_BaseCombatWeapon;
class C_WeaponCombatShield;

class C_BaseCombatCharacter : public C_BaseFlex
{
public:
	DECLARE_CLASS( C_BaseCombatCharacter, C_BaseFlex );

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_BaseCombatCharacter( void );
	virtual			~C_BaseCombatCharacter( void );

	virtual bool	IsBaseCombatCharacter( void ) { return true; };

	// -----------------------
	// Ammo
	// -----------------------
	void				RemoveAmmo( int iCount, int iAmmoIndex );
	void				RemoveAllAmmo( );
	int					GetAmmoCount( int iAmmoIndex ) const;
//	int					GetAmmoCount( char *szName ) const;

	C_BaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	virtual	bool		Weapon_Switch( C_BaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	bool				Weapon_CanSwitchTo(C_BaseCombatWeapon *pWeapon);

	virtual C_BaseCombatWeapon	*GetActiveWeapon( void ) const;
	int					WeaponCount() const;
	C_BaseCombatWeapon	*GetWeapon( int i );

	// This is a sort of hack back-door only used by physgun!
	void SetAmmoCount( int iCount, int iAmmoIndex );

	float				GetNextAttack() const { return m_flNextAttack; }
	void				SetNextAttack( float flWait ) { m_flNextAttack = flWait; }

public:

	float			m_flNextAttack;

private:
	CNetworkArray( int, m_iAmmo, MAX_AMMO_TYPES );

	CHandle<C_BaseCombatWeapon>		m_hMyWeapons[MAX_WEAPONS];
	CHandle< C_BaseCombatWeapon > m_hActiveWeapon;
private:
	C_BaseCombatCharacter( const C_BaseCombatCharacter & ); // not defined, not accessible


//-----------------------
#ifdef TF2_CLIENT_DLL
public:
	virtual void	Release( void );
	virtual void	SetDormant( bool bDormant );
	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

	// TF2 Powerups
	virtual bool	CanBePoweredUp( void ) { return true; }
	bool			HasPowerup( int iPowerup ) { return ( m_iPowerups & (1 << iPowerup) ) != 0; };
	virtual void	PowerupStart( int iPowerup, bool bInitial );
	virtual void	PowerupEnd( int iPowerup );
	void			RemoveAllPowerups( void );

	// Powerup effects
	void			AddEMPEffect( float flSize );
	void			AddBuffEffect( float flSize );

	C_WeaponCombatShield		*GetShield( void );

public:
	int				m_iPowerups;
	int				m_iPrevPowerups;
#endif

};

inline C_BaseCombatCharacter *ToBaseCombatCharacter( C_BaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsBaseCombatCharacter() )
		return NULL;

#if _DEBUG
	return dynamic_cast<C_BaseCombatCharacter *>( pEntity );
#else
	return static_cast<C_BaseCombatCharacter *>( pEntity );
#endif
}

EXTERN_RECV_TABLE(DT_BaseCombatCharacter);

#endif // C_BASECOMBATCHARACTER_H
