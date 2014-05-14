/*
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "Player.h"
#include "naxxramas.h"

//Stalagg
enum StalaggYells
{
    SAY_STAL_AGGRO          = 0,
    SAY_STAL_SLAY           = 1,
    SAY_STAL_DEATH          = 2
};

enum StalagSpells
{
    SPELL_POWERSURGE        = 28134,
    H_SPELL_POWERSURGE      = 54529,
    SPELL_MAGNETIC_PULL     = 28338,
    SPELL_STALAGG_TESLA     = 28097
};

//Feugen
enum FeugenYells
{
    SAY_FEUG_AGGRO          = 0,
    SAY_FEUG_SLAY           = 1,
    SAY_FEUG_DEATH          = 2
};

enum FeugenSpells
{
    SPELL_STATICFIELD       = 28135,
    H_SPELL_STATICFIELD     = 54528,
    SPELL_FEUGEN_TESLA      = 28109
};

// Thaddius DoAction
enum ThaddiusActions
{
    ACTION_FEUGEN_RESET,
    ACTION_FEUGEN_DIED,
    ACTION_STALAGG_RESET,
    ACTION_STALAGG_DIED
};

//generic
#define C_TESLA_COIL            16218           //the coils (emotes "Tesla Coil overloads!")

//Thaddius
enum ThaddiusYells
{
    SAY_GREET               = 0,
    SAY_AGGRO               = 1,
    SAY_SLAY                = 2,
    SAY_ELECT               = 3,
    SAY_DEATH               = 4,
    SAY_SCREAM              = 5
};

enum ThaddiusSpells
{
    SPELL_POLARITY_SHIFT        = 28089,
    SPELL_BALL_LIGHTNING        = 28299,
    SPELL_CHAIN_LIGHTNING       = 28167,
    H_SPELL_CHAIN_LIGHTNING     = 54531,
    SPELL_BERSERK               = 27680,
    SPELL_POSITIVE_CHARGE       = 28062,
    SPELL_POSITIVE_CHARGE_STACK = 29659,
    SPELL_NEGATIVE_CHARGE       = 28085,
    SPELL_NEGATIVE_CHARGE_STACK = 29660,
    SPELL_POSITIVE_POLARITY     = 28059,
    SPELL_NEGATIVE_POLARITY     = 28084,
};

enum Events
{
    EVENT_NONE,
    EVENT_SHIFT,
    EVENT_CHAIN,
    EVENT_BERSERK,
};

enum Achievement
{
    DATA_POLARITY_SWITCH    = 76047605,
};

class boss_thaddius : public CreatureScript
{
public:
    boss_thaddius() : CreatureScript("boss_thaddius") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetInstanceAI<boss_thaddiusAI>(creature);
    }

    struct boss_thaddiusAI : public BossAI
    {
        boss_thaddiusAI(Creature* creature) : BossAI(creature, BOSS_THADDIUS)
        {
            // init is a bit tricky because thaddius shall track the life of both adds, but not if there was a wipe
            // and, in particular, if there was a crash after both adds were killed (should not respawn)

            // Moreover, the adds may not yet be spawn. So just track down the status if mob is spawn
            // and each mob will send its status at reset (meaning that it is alive)
            checkFeugenAlive = false;
            if (Creature* pFeugen = me->GetCreature(*me, instance->GetData64(DATA_FEUGEN)))
                checkFeugenAlive = pFeugen->IsAlive();

            checkStalaggAlive = false;
            if (Creature* pStalagg = me->GetCreature(*me, instance->GetData64(DATA_STALAGG)))
                checkStalaggAlive = pStalagg->IsAlive();

            if (!checkFeugenAlive && !checkStalaggAlive)
            {
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_STUNNED);
                me->SetReactState(REACT_AGGRESSIVE);
            }
            else
            {
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_STUNNED);
                me->SetReactState(REACT_PASSIVE);
            }
        }

        bool checkStalaggAlive;
        bool checkFeugenAlive;
        bool polaritySwitch;
        uint32 uiAddsTimer;

        void KilledUnit(Unit* /*victim*/) override
        {
            if (!(rand()%5))
                Talk(SAY_SLAY);
        }

        void JustDied(Unit* /*killer*/) override
        {
            _JustDied();
            Talk(SAY_DEATH);
        }

        void DoAction(int32 action) override
        {
            switch (action)
            {
                case ACTION_FEUGEN_RESET:
                    checkFeugenAlive = true;
                    break;
                case ACTION_FEUGEN_DIED:
                    checkFeugenAlive = false;
                    break;
                case ACTION_STALAGG_RESET:
                    checkStalaggAlive = true;
                    break;
                case ACTION_STALAGG_DIED:
                    checkStalaggAlive = false;
                    break;
            }

            if (!checkFeugenAlive && !checkStalaggAlive)
            {
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_STUNNED);
                // REACT_AGGRESSIVE only reset when he takes damage.
                DoZoneInCombat();
            }
            else
            {
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_STUNNED);
                me->SetReactState(REACT_PASSIVE);
            }
        }

        void EnterCombat(Unit* /*who*/) override
        {
            _EnterCombat();
            Talk(SAY_AGGRO);
            events.ScheduleEvent(EVENT_SHIFT, 30000);
            events.ScheduleEvent(EVENT_CHAIN, urand(10000, 20000));
            events.ScheduleEvent(EVENT_BERSERK, 360000);
        }

        void DamageTaken(Unit* /*pDoneBy*/, uint32 & /*uiDamage*/) override
        {
            me->SetReactState(REACT_AGGRESSIVE);
        }

        void SetData(uint32 id, uint32 data) override
        {
            if (id == DATA_POLARITY_SWITCH)
                polaritySwitch = data ? true : false;
        }

        uint32 GetData(uint32 id) const override
        {
            if (id != DATA_POLARITY_SWITCH)
                return 0;

            return uint32(polaritySwitch);
        }

        //Very hacky solution.
        void RemoveDebuffs() 
        {
            Map* map = me->GetMap();
            Map::PlayerList const &PlayerList = map->GetPlayers();
            for (Map::PlayerList::const_iterator itr = PlayerList.begin(); itr != PlayerList.end(); ++itr)
                if (Player* player = itr->GetSource())
                    if (player->IsInRange(me, 0, 80, true))
                    {
                        //Remove both the positive and negative aura
                        player->RemoveAurasDueToSpell(SPELL_POSITIVE_POLARITY);
                        player->RemoveAurasDueToSpell(SPELL_NEGATIVE_POLARITY);
                    }
        }
		
        void UpdateAI(uint32 diff) override
        {
            if (checkFeugenAlive && checkStalaggAlive)
                uiAddsTimer = 0;

            if (checkStalaggAlive != checkFeugenAlive)
            {
                uiAddsTimer += diff;
                if (uiAddsTimer > 5000)
                {
                    if (!checkStalaggAlive)
                    {
                        if (Creature* pStalagg = me->GetCreature(*me, instance->GetData64(DATA_STALAGG)))
                            pStalagg->Respawn();
                    }
                    else
                    {
                        if (Creature* pFeugen = me->GetCreature(*me, instance->GetData64(DATA_FEUGEN)))
                            pFeugen->Respawn();
                    }
                }
            }

            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_SHIFT:
						RemoveDebuffs();
                        DoCastAOE(SPELL_POLARITY_SHIFT);
                        events.ScheduleEvent(EVENT_SHIFT, 30000);
                        return;
                    case EVENT_CHAIN:
                        DoCastVictim(RAID_MODE(SPELL_CHAIN_LIGHTNING, H_SPELL_CHAIN_LIGHTNING));
                        events.ScheduleEvent(EVENT_CHAIN, urand(10000, 20000));
                        return;
                    case EVENT_BERSERK:
                        DoCast(me, SPELL_BERSERK);
                        return;
                }
            }

            if (events.GetTimer() > 15000 && !me->IsWithinMeleeRange(me->GetVictim()))
                DoCastVictim(SPELL_BALL_LIGHTNING);
            else
                DoMeleeAttackIfReady();
        }
    };

};

class npc_stalagg : public CreatureScript
{
public:
    npc_stalagg() : CreatureScript("npc_stalagg") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetInstanceAI<npc_stalaggAI>(creature);
    }

    struct npc_stalaggAI : public ScriptedAI
    {
        npc_stalaggAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 powerSurgeTimer;
        uint32 magneticPullTimer;

        void Reset() override
        {
            if (Creature* pThaddius = me->GetCreature(*me, instance->GetData64(DATA_THADDIUS)))
                if (pThaddius->AI())
                    pThaddius->AI()->DoAction(ACTION_STALAGG_RESET);
            powerSurgeTimer = urand(20000, 25000);
            magneticPullTimer = 20000;
        }

        void KilledUnit(Unit* /*victim*/) override
        {
            if (!(rand()%5))
                Talk(SAY_STAL_SLAY);
        }

        void EnterCombat(Unit* /*who*/) override
        {
            Talk(SAY_STAL_AGGRO);
            DoCast(SPELL_STALAGG_TESLA);
        }

        void JustDied(Unit* /*killer*/) override
        {
            Talk(SAY_STAL_DEATH);
            if (Creature* pThaddius = me->GetCreature(*me, instance->GetData64(DATA_THADDIUS)))
                if (pThaddius->AI())
                    pThaddius->AI()->DoAction(ACTION_STALAGG_DIED);
        }

        void UpdateAI(uint32 uiDiff) override
        {
            if (!UpdateVictim())
                return;

            if (magneticPullTimer <= uiDiff)
            {
                if (Creature* pFeugen = me->GetCreature(*me, instance->GetData64(DATA_FEUGEN)))
                {
                    Unit* pStalaggVictim = me->GetVictim();
                    Unit* pFeugenVictim = pFeugen->GetVictim();

                    if (pFeugenVictim && pStalaggVictim)
                    {
                        //Aggro Swap
						pFeugen->getThreatManager().addThreat(pStalaggVictim, pFeugen->getThreatManager().getThreat(pFeugenVictim));
						me->getThreatManager().addThreat(pFeugenVictim, me->getThreatManager().getThreat(pStalaggVictim));
						pFeugen->getThreatManager().modifyThreatPercent(pFeugenVictim, -100);
						me->getThreatManager().modifyThreatPercent(pStalaggVictim, -100);

						pFeugenVictim->JumpTo(me, 0.3f);
						pStalaggVictim->JumpTo(pFeugen, 0.3f);
                    }
                }

                magneticPullTimer = 20000;
            }
            else magneticPullTimer -= uiDiff;

            if (powerSurgeTimer <= uiDiff)
            {
                DoCast(me, RAID_MODE(SPELL_POWERSURGE, H_SPELL_POWERSURGE));
                powerSurgeTimer = urand(15000, 20000);
            } else powerSurgeTimer -= uiDiff;

            DoMeleeAttackIfReady();
        }
    };

};

class npc_feugen : public CreatureScript
{
public:
    npc_feugen() : CreatureScript("npc_feugen") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetInstanceAI<npc_feugenAI>(creature);
    }

    struct npc_feugenAI : public ScriptedAI
    {
        npc_feugenAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 staticFieldTimer;

        void Reset() override
        {
            if (Creature* pThaddius = me->GetCreature(*me, instance->GetData64(DATA_THADDIUS)))
                if (pThaddius->AI())
                    pThaddius->AI()->DoAction(ACTION_FEUGEN_RESET);
            staticFieldTimer = 5000;
        }

        void KilledUnit(Unit* /*victim*/) override
        {
            if (!(rand()%5))
                Talk(SAY_FEUG_SLAY);
        }

        void EnterCombat(Unit* /*who*/) override
        {
            Talk(SAY_FEUG_AGGRO);
            DoCast(SPELL_FEUGEN_TESLA);
        }

        void JustDied(Unit* /*killer*/) override
        {
            Talk(SAY_FEUG_DEATH);
            if (Creature* pThaddius = me->GetCreature(*me, instance->GetData64(DATA_THADDIUS)))
                if (pThaddius->AI())
                    pThaddius->AI()->DoAction(ACTION_FEUGEN_DIED);
        }

        void UpdateAI(uint32 uiDiff) override
        {
            if (!UpdateVictim())
                return;

            if (staticFieldTimer <= uiDiff)
            {
                DoCast(me, RAID_MODE(SPELL_STATICFIELD, H_SPELL_STATICFIELD));
                staticFieldTimer = 5000;
            } else staticFieldTimer -= uiDiff;

            DoMeleeAttackIfReady();
        }
    };

};


class achievement_polarity_switch : public AchievementCriteriaScript
{
    public:
        achievement_polarity_switch() : AchievementCriteriaScript("achievement_polarity_switch") { }

        bool OnCheck(Player* /*source*/, Unit* target) override
        {
            return target && target->GetAI()->GetData(DATA_POLARITY_SWITCH);
        }
};

void AddSC_boss_thaddius()
{
    new boss_thaddius();
    new npc_stalagg();
    new npc_feugen();
    new achievement_polarity_switch();
}
