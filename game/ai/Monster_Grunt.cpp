
#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

class rvMonsterGrunt : public idAI {
public:

	CLASS_PROTOTYPE( rvMonsterGrunt );

	rvMonsterGrunt ( void );
	
	void				Spawn					( void );
	void				Save					( idSaveGame *savefile ) const;
	void				Restore					( idRestoreGame *savefile );
	
	virtual void		AdjustHealthByDamage	( int damage );

protected:

	rvAIAction			actionMeleeMoveAttack;
	rvAIAction			actionChaingunAttack;

	virtual bool		CheckActions		( void );

	virtual void		OnTacticalChange	( aiTactical_t oldTactical );
	virtual void		OnDeath				( void );

private:

	int					standingMeleeNoAttackTime;
	int					rageThreshold;

	void				RageStart			( void );
	void				RageStop			( void );
	
	// Torso States
	stateResult_t		State_Torso_Enrage		( const stateParms_t& parms );
	stateResult_t		State_Torso_Pain		( const stateParms_t& parms );
	stateResult_t		State_Torso_LeapAttack	( const stateParms_t& parms );

	idVec3				home;

	CLASS_STATES_PROTOTYPE ( rvMonsterGrunt );
};

CLASS_DECLARATION( idAI, rvMonsterGrunt )
END_CLASS

/*
================
rvMonsterGrunt::rvMonsterGrunt
================
*/
rvMonsterGrunt::rvMonsterGrunt ( void ) {
	standingMeleeNoAttackTime = 0;
}

/*
================
rvMonsterGrunt::Spawn
================
*/
void rvMonsterGrunt::Spawn ( void ) {
	animator.SetPlaybackRate(0.5f);
}

/*
================
rvMonsterGrunt::Save
================
*/
void rvMonsterGrunt::Save ( idSaveGame *savefile ) const {
	actionMeleeMoveAttack.Save( savefile );
	actionChaingunAttack.Save( savefile );

	savefile->WriteInt( rageThreshold );
	savefile->WriteInt( standingMeleeNoAttackTime );
}

/*
================
rvMonsterGrunt::Restore
================
*/
void rvMonsterGrunt::Restore ( idRestoreGame *savefile ) {
	actionMeleeMoveAttack.Restore( savefile );
	actionChaingunAttack.Restore( savefile );

	savefile->ReadInt( rageThreshold );
	savefile->ReadInt( standingMeleeNoAttackTime );
}

/*
================
rvMonsterGrunt::RageStart
================
*/
void rvMonsterGrunt::RageStart ( void ) {
	SetShaderParm ( 6, 1 );

	// Disable non-rage actions
	actionEvadeLeft.fl.disabled = true;
	actionEvadeRight.fl.disabled = true;
	
	// Speed up animations
	animator.SetPlaybackRate ( 1.25f );

	// Disable pain
	pain.threshold = 0;

	// Start over with health when enraged
	health = spawnArgs.GetInt ( "health" );
	
	// No more going to rage
	rageThreshold = 0;
}

/*
================
rvMonsterGrunt::RageStop
================
*/
void rvMonsterGrunt::RageStop ( void ) {
	SetShaderParm ( 6, 0 );
}

/*
================
rvMonsterGrunt::CheckActions
================
*/
bool rvMonsterGrunt::CheckActions ( void ) {
	WanderAround();
	return false;
}

/*
================
rvMonsterGrunt::OnDeath
================
*/
void rvMonsterGrunt::OnDeath ( void ) {
	RageStop ( );
	return idAI::OnDeath ( );
}

/*
================
rvMonsterGrunt::OnTacticalChange

Enable/Disable the ranged attack based on whether the grunt needs it
================
*/
void rvMonsterGrunt::OnTacticalChange ( aiTactical_t oldTactical ) {
	switch ( combat.tacticalCurrent ) {
		case AITACTICAL_MELEE:
			actionRangedAttack.fl.disabled = true;
			break;

		default:
			actionRangedAttack.fl.disabled = false;
			break;
	}
}

/*
=====================
rvMonsterGrunt::AdjustHealthByDamage
=====================
*/
void rvMonsterGrunt::AdjustHealthByDamage ( int damage ) {
	// Take less damage during enrage process 
	if ( rageThreshold && health < rageThreshold ) { 
		health -= (damage * 0.25f);
		return;
	}
	return idAI::AdjustHealthByDamage ( damage );
}

/*
===============================================================================

	States 

===============================================================================
*/

CLASS_STATES_DECLARATION ( rvMonsterGrunt )
	STATE ( "Torso_Enrage",		rvMonsterGrunt::State_Torso_Enrage )
	STATE ( "Torso_Pain",		rvMonsterGrunt::State_Torso_Pain )
	STATE ( "Torso_LeapAttack",	rvMonsterGrunt::State_Torso_LeapAttack )
END_CLASS_STATES

/*
================
rvMonsterGrunt::State_Torso_Pain
================
*/
stateResult_t rvMonsterGrunt::State_Torso_Pain ( const stateParms_t& parms ) {
	// Stop streaming pain if its time to get angry
	if ( pain.loopEndTime && health < rageThreshold ) {
		pain.loopEndTime = 0;
	}
	return idAI::State_Torso_Pain ( parms );
}

/*
================
rvMonsterGrunt::State_Torso_Enrage
================
*/
stateResult_t rvMonsterGrunt::State_Torso_Enrage ( const stateParms_t& parms ) {
	enum {
		STAGE_ANIM,
		STAGE_ANIM_WAIT,
	};
	switch ( parms.stage ) {
		case STAGE_ANIM:
			DisableAnimState ( ANIMCHANNEL_LEGS );
			PlayAnim ( ANIMCHANNEL_TORSO, "anger", parms.blendFrames );
			return SRESULT_STAGE ( STAGE_ANIM_WAIT );
		
		case STAGE_ANIM_WAIT:
			if ( AnimDone ( ANIMCHANNEL_TORSO, 4 ) ) {
				RageStart ( );
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}


/*
================
rvMonsterGrunt::State_Torso_LeapAttack
================
*/
stateResult_t rvMonsterGrunt::State_Torso_LeapAttack ( const stateParms_t& parms ) {
	enum {
		STAGE_ANIM,
		STAGE_ANIM_WAIT,
	};
	switch ( parms.stage ) {
		case STAGE_ANIM:
			DisableAnimState ( ANIMCHANNEL_LEGS );
			lastAttackTime = 0;
			// Play the action animation
			PlayAnim ( ANIMCHANNEL_TORSO, animator.GetAnim ( actionAnimNum )->FullName ( ), parms.blendFrames );
			return SRESULT_STAGE ( STAGE_ANIM_WAIT );
		
		case STAGE_ANIM_WAIT:
			if ( AnimDone ( ANIMCHANNEL_TORSO, parms.blendFrames ) ) {
				// If we missed our leap attack get angry
				if ( !lastAttackTime && rageThreshold ) {
					PostAnimState ( ANIMCHANNEL_TORSO, "Torso_Enrage", parms.blendFrames );
				}
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}
