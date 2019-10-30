/**************************************************************************
*  File: class.c                                           Part of altMUD *
*  Usage: Source file for class-specific code.                            *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

/* This file attempts to concentrate most of the code which must be changed
 * in order for new classes to be added.  If you're adding a new class, you
 * should go through this entire file from beginning to end and add the
 * appropriate new special cases for your new class. */

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "spells.h"
#include "interpreter.h"
#include "constants.h"
#include "act.h"
#include "class.h"

/* Names first */
const char *class_abbrevs[] = {
  "Mu",
  "Cl",
  "Th",
  "Wa",
  "\n"
};

const char *pc_class_types[] = {
  "Magic User",
  "Cleric",
  "Thief",
  "Warrior",
  "\n"
};

/* The menu for choosing a class in interpreter.c: */
const char *class_menu =
"\r\n"
"Select a class:\r\n"
"  [\t(C\t)]leric\r\n"
"  [\t(T\t)]hief\r\n"
"  [\t(W\t)]arrior\r\n"
"  [\t(M\t)]agic-user\r\n";

/* The code to interpret a class letter -- used in interpreter.c when a new
 * character is selecting a class and by 'set class' in act.wizard.c. */
int parse_class(char arg)
{
  arg = LOWER(arg);

  switch (arg) {
  case 'm': return CLASS_MAGIC_USER;
  case 'c': return CLASS_CLERIC;
  case 'w': return CLASS_WARRIOR;
  case 't': return CLASS_THIEF;
  default:  return CLASS_UNDEFINED;
  }
}

/* bitvectors (i.e., powers of two) for each class, mainly for use in do_who
 * and do_users.  Add new classes at the end so that all classes use sequential
 * powers of two (1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, etc.) up to
 * the limit of your bitvector_t, typically 0-31. */
bitvector_t find_class_bitvector(const char *arg)
{
  size_t rpos, ret = 0;

  for (rpos = 0; rpos < strlen(arg); rpos++)
    ret |= (1 << parse_class(arg[rpos]));

  return (ret);
}

/* These are definitions which control the guildmasters for each class.
 * The  first field (top line) controls the highest percentage skill level a
 * character of the class is allowed to attain in any skill.  (After this
 * level, attempts to practice will say "You are already learned in this area."
 *
 * The second line controls the maximum percent gain in learnedness a character
 * is allowed per practice -- in other words, if the random die throw comes out
 * higher than this number, the gain will only be this number instead.
 *
 * The third line controls the minimu percent gain in learnedness a character
 * is allowed per practice -- in other words, if the random die throw comes
 * out below this number, the gain will be set up to this number.
 *
 * The fourth line simply sets whether the character knows 'spells' or 'skills'.
 * This does not affect anything except the message given to the character when
 * trying to practice (i.e. "You know of the following spells" vs. "You know of
 * the following skills" */

#define SPELL	0
#define SKILL	1

/* #define LEARNED_LEVEL	0  % known which is considered "learned" */
/* #define MAX_PER_PRAC		1  max percent gain in skill per practice */
/* #define MIN_PER_PRAC		2  min percent gain in skill per practice */
/* #define PRAC_TYPE		3  should it say 'spell' or 'skill'?	*/

int prac_params[4][NUM_CLASSES] = {
  /* MAG	CLE	THE	WAR */
  { 95,		95,	85,	80	},	/* learned level */
  { 100,	100,	12,	12	},	/* max per practice */
  { 25,		25,	0,	0	},	/* min per practice */
  { SPELL,	SPELL,	SKILL,	SKILL	},	/* prac name */
};

/* The appropriate rooms for each guildmaster/guildguard; controls which types
 * of people the various guildguards let through.  i.e., the first line shows
 * that from room 3017, only MAGIC_USERS are allowed to go south. Don't forget
 * to visit spec_assign.c if you create any new mobiles that should be a guild
 * master or guard so they can act appropriately. If you "recycle" the
 * existing mobs that are used in other guilds for your new guild, then you
 * don't have to change that file, only here. Guildguards are now implemented
 * via triggers. This code remains as an example. */
struct guild_info_type guild_info[] = {

/* Midgaard */
 { CLASS_MAGIC_USER,    3017,    SOUTH   },
 { CLASS_CLERIC,        3004,    NORTH   },
 { CLASS_THIEF,         3027,    EAST   },
 { CLASS_WARRIOR,       3021,    EAST   },

/* Brass Dragon */
  { -999 /* all */ ,	5065,	WEST	},

/* this must go last -- add new guards above! */
  { -1, NOWHERE, -1}
};

/* Saving throws for : MCTW : PARA, ROD, PETRI, BREATH, SPELL. Levels 0-40. Do
 * not forget to change extern declaration in magic.c if you add to this. */
byte saving_throws(int class_num, int type, int level)
{
  switch (class_num) {
  case CLASS_MAGIC_USER:
    switch (type) {
    case SAVING_PARA:	/* Paralyzation */
      switch (level) {
      case  0: return 90;
      case  1: return 70;
      default:
	log("SYSERR: Missing level for mage paralyzation saving throw.");
	break;
      }
    case SAVING_ROD:	/* Rods */
      switch (level) {
      case  0: return 90;
      case  1: return 55;
      default:
	log("SYSERR: Missing level for mage rod saving throw.");
	break;
      }
    case SAVING_PETRI:	/* Petrification */
      switch (level) {
      case  0: return 90;
      case  1: return 65;
      default:
	log("SYSERR: Missing level for mage petrification saving throw.");
	break;
      }
    case SAVING_BREATH:	/* Breath weapons */
      switch (level) {
      case  0: return 90;
      case  1: return 75;
      default:
	log("SYSERR: Missing level for mage breath saving throw.");
	break;
      }
    case SAVING_SPELL:	/* Generic spells */
      switch (level) {
      case  0: return 90;
      case  1: return 60;
      default:
	log("SYSERR: Missing level for mage spell saving throw.");
	break;
      }
    default:
      log("SYSERR: Invalid saving throw type.");
      break;
    }
    break;
  case CLASS_CLERIC:
    switch (type) {
    case SAVING_PARA:	/* Paralyzation */
      switch (level) {
      case  0: return 90;
      case  1: return 60;
      default:
	log("SYSERR: Missing level for cleric paralyzation saving throw.");
	break;
      }
    case SAVING_ROD:	/* Rods */
      switch (level) {
      case  0: return 90;
      case  1: return 70;
      default:
	log("SYSERR: Missing level for cleric rod saving throw.");
	break;
      }
    case SAVING_PETRI:	/* Petrification */
      switch (level) {
      case  0: return 90;
      case  1: return 65;
      default:
	log("SYSERR: Missing level for cleric petrification saving throw.");
	break;
      }
    case SAVING_BREATH:	/* Breath weapons */
      switch (level) {
      case  0: return 90;
      case  1: return 80;
      default:
	log("SYSERR: Missing level for cleric breath saving throw.");
	break;
      }
    case SAVING_SPELL:	/* Generic spells */
      switch (level) {
      case  0: return 90;
      case  1: return 75;
      default:
	log("SYSERR: Missing level for cleric spell saving throw.");
	break;
      }
    default:
      log("SYSERR: Invalid saving throw type.");
      break;
    }
    break;
  case CLASS_THIEF:
    switch (type) {
    case SAVING_PARA:	/* Paralyzation */
      switch (level) {
      case  0: return 90;
      case  1: return 65;
      default:
	log("SYSERR: Missing level for thief paralyzation saving throw.");
	break;
      }
    case SAVING_ROD:	/* Rods */
      switch (level) {
      case  0: return 90;
      case  1: return 70;
      default:
	log("SYSERR: Missing level for thief rod saving throw.");
	break;
      }
    case SAVING_PETRI:	/* Petrification */
      switch (level) {
      case  0: return 90;
      case  1: return 60;
      default:
	log("SYSERR: Missing level for thief petrification saving throw.");
	break;
      }
    case SAVING_BREATH:	/* Breath weapons */
      switch (level) {
      case  0: return 90;
      case  1: return 80;
      default:
	log("SYSERR: Missing level for thief breath saving throw.");
	break;
      }
    case SAVING_SPELL:	/* Generic spells */
      switch (level) {
      case  0: return 90;
      case  1: return 75;
      default:
	log("SYSERR: Missing level for thief spell saving throw.");
	break;
      }
    default:
      log("SYSERR: Invalid saving throw type.");
      break;
    }
    break;
  case CLASS_WARRIOR:
    switch (type) {
    case SAVING_PARA:	/* Paralyzation */
      switch (level) {
      case  0: return 90;
      case  1: return 70;
      default:
	log("SYSERR: Missing level for warrior paralyzation saving throw.");
	break;
      }
    case SAVING_ROD:	/* Rods */
      switch (level) {
      case  0: return 90;
      case  1: return 80;
      default:
	log("SYSERR: Missing level for warrior rod saving throw.");
	break;
      }
    case SAVING_PETRI:	/* Petrification */
      switch (level) {
      case  0: return 90;
      case  1: return 75;
      default:
	log("SYSERR: Missing level for warrior petrification saving throw.");
	break;
      }
    case SAVING_BREATH:	/* Breath weapons */
      switch (level) {
      case  0: return 90;
      case  1: return 85;
      default:
	log("SYSERR: Missing level for warrior breath saving throw.");
	break;
      }
    case SAVING_SPELL:	/* Generic spells */
      switch (level) {
      case  0: return 90;
      case  1: return 85;
      default:
	log("SYSERR: Missing level for warrior spell saving throw.");
	break;
      }
    default:
      log("SYSERR: Invalid saving throw type.");
      break;
    }
  default:
    log("SYSERR: Invalid class saving throw.");
    break;
  }

  /* Should not get here unless something is wrong. */
  return 100;
}

/* THAC0 for classes and levels.  (To Hit Armor Class 0) */
int thaco(int class_num, int level)
{
  switch (class_num) {
  case CLASS_MAGIC_USER:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    default:
      log("SYSERR: Missing level for mage thac0.");
    }
  case CLASS_CLERIC:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    default:
      log("SYSERR: Missing level for cleric thac0.");
    }
  case CLASS_THIEF:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    default:
      log("SYSERR: Missing level for thief thac0.");
    }
  case CLASS_WARRIOR:
    switch (level) {
    case  0: return 100;
    case  1: return  20;
    default:
      log("SYSERR: Missing level for warrior thac0.");
    }
  default:
    log("SYSERR: Unknown class in thac0 chart.");
  }

  /* Will not get there unless something is wrong. */
  return 100;
}


/* Roll the 6 stats for a character... each stat is made of the sum of the best
 * 3 out of 4 rolls of a 6-sided die.  Each class then decides which priority
 * will be given for the best to worst stats. */
void roll_real_abils(struct char_data *ch)
{
  int i, j, k, temp;
  ubyte table[6];
  ubyte rolls[4];

  for (i = 0; i < 6; i++)
    table[i] = 0;

  for (i = 0; i < 6; i++) {

    for (j = 0; j < 4; j++)
      rolls[j] = rand_number(1, 6);

    temp = rolls[0] + rolls[1] + rolls[2] + rolls[3] -
      MIN(rolls[0], MIN(rolls[1], MIN(rolls[2], rolls[3])));

    for (k = 0; k < 6; k++)
      if (table[k] < temp) {
	temp ^= table[k];
	table[k] ^= temp;
	temp ^= table[k];
      }
  }

  ch->real_abils.str_add = 0;

  switch (GET_CLASS(ch)) {
  case CLASS_MAGIC_USER:
    ch->real_abils.intel = table[0];
    ch->real_abils.wis = table[1];
    ch->real_abils.dex = table[2];
    ch->real_abils.str = table[3];
    ch->real_abils.con = table[4];
    ch->real_abils.cha = table[5];
    break;
  case CLASS_CLERIC:
    ch->real_abils.wis = table[0];
    ch->real_abils.intel = table[1];
    ch->real_abils.str = table[2];
    ch->real_abils.dex = table[3];
    ch->real_abils.con = table[4];
    ch->real_abils.cha = table[5];
    break;
  case CLASS_THIEF:
    ch->real_abils.dex = table[0];
    ch->real_abils.str = table[1];
    ch->real_abils.con = table[2];
    ch->real_abils.intel = table[3];
    ch->real_abils.wis = table[4];
    ch->real_abils.cha = table[5];
    break;
  case CLASS_WARRIOR:
    ch->real_abils.str = table[0];
    ch->real_abils.dex = table[1];
    ch->real_abils.con = table[2];
    ch->real_abils.wis = table[3];
    ch->real_abils.intel = table[4];
    ch->real_abils.cha = table[5];
    if (ch->real_abils.str == 18)
      ch->real_abils.str_add = rand_number(0, 100);
    break;
  }
  ch->aff_abils = ch->real_abils;
}

/* Some initializations for characters, including initial skills */
void do_start(struct char_data *ch)
{
  GET_LEVEL(ch) = 1;
  GET_EXP(ch) = 1;

  set_title(ch, NULL);
  roll_real_abils(ch);

  GET_MAX_HIT(ch)  = 100;
  GET_MAX_MANA(ch) = 100;
  GET_MAX_MOVE(ch) = 100;
  GET_MAX_STUN(ch) = 100;

  switch (GET_CLASS(ch)) {

  case CLASS_MAGIC_USER:
    SET_SKILL(ch, SKILL_FORAGE, 10);
    SET_SKILL(ch, SKILL_UNARMED, 10);
    SET_SKILL(ch, SKILL_SHIELD_USE, 10);
    break;

  case CLASS_CLERIC:
    SET_SKILL(ch, SKILL_FORAGE, 10);
    SET_SKILL(ch, SKILL_UNARMED, 10);
    SET_SKILL(ch, SKILL_PIERCING_WEAPONS, 10);
    SET_SKILL(ch, SKILL_SHIELD_USE, 10);
    break;

  case CLASS_THIEF:
    SET_SKILL(ch, SKILL_SNEAK, 10);
    SET_SKILL(ch, SKILL_HIDE, 10);
    SET_SKILL(ch, SKILL_STEAL, 10);
    SET_SKILL(ch, SKILL_BACKSTAB, 10);
    SET_SKILL(ch, SKILL_PICK_LOCK, 10);
    SET_SKILL(ch, SKILL_TRACK, 10);
    SET_SKILL(ch, SKILL_FORAGE, 10);
    SET_SKILL(ch, SKILL_UNARMED, 10);
    SET_SKILL(ch, SKILL_PIERCING_WEAPONS, 10);
    SET_SKILL(ch, SKILL_STABBING_WEAPONS, 10);
    SET_SKILL(ch, SKILL_SHIELD_USE, 10);
    break;

  case CLASS_WARRIOR:
    SET_SKILL(ch, SKILL_FORAGE, 10);
    SET_SKILL(ch, SKILL_KICK, 10);
    SET_SKILL(ch, SKILL_BASH, 10);
    SET_SKILL(ch, SKILL_FORAGE, 10);
    SET_SKILL(ch, SKILL_BANDAGE, 10);
    SET_SKILL(ch, SKILL_RESCUE, 10);
    SET_SKILL(ch, SKILL_TRACK, 10);
    SET_SKILL(ch, SKILL_UNARMED, 10);
    SET_SKILL(ch, SKILL_DISARM, 10);
    SET_SKILL(ch, SKILL_PIERCING_WEAPONS, 10);
    SET_SKILL(ch, SKILL_SLASHING_WEAPONS, 10);
    SET_SKILL(ch, SKILL_BLUDGEONING_WEAPONS, 10);
    SET_SKILL(ch, SKILL_SHIELD_USE, 10);
    break;
  }

  advance_level(ch);

  GET_HIT(ch) = GET_MAX_HIT(ch);
  GET_MANA(ch) = GET_MAX_MANA(ch);
  GET_MOVE(ch) = GET_MAX_MOVE(ch);
  GET_STUN(ch) = GET_MAX_STUN(ch);

  GET_COND(ch, THIRST) = 24;
  GET_COND(ch, HUNGER) = 24;
  GET_COND(ch, DRUNK) = 0;

  if (CONFIG_SITEOK_ALL)
    SET_BIT_AR(PLR_FLAGS(ch), PLR_SITEOK);
}

/* This function controls the change to maxmove, maxmana, and maxhp for each
 * class every time they gain a level. */
void advance_level(struct char_data *ch)
{
  int add_hp, add_mana = 0, add_move = 0, add_stun = 0, i;

  add_hp = con_app[GET_CON(ch)].hitp;

  switch (GET_CLASS(ch)) {

  case CLASS_MAGIC_USER:
    add_hp += rand_number(3, 8);
    add_mana = rand_number(GET_LEVEL(ch), (int)(1.5 * GET_LEVEL(ch)));
    add_mana = MIN(add_mana, 10);
    add_move = rand_number(0, 2);
    add_stun = rand_number(1, 2);
    break;

  case CLASS_CLERIC:
    add_hp += rand_number(5, 10);
    add_mana = rand_number(GET_LEVEL(ch), (int)(1.5 * GET_LEVEL(ch)));
    add_mana = MIN(add_mana, 10);
    add_move = rand_number(0, 2);
    add_stun = rand_number(1, 2);
    break;

  case CLASS_THIEF:
    add_hp += rand_number(7, 13);
    add_mana = 0;
    add_move = rand_number(1, 3);
    add_stun = rand_number(1, 4);
    break;

  case CLASS_WARRIOR:
    add_hp += rand_number(10, 15);
    add_mana = 0;
    add_move = rand_number(1, 3);
    add_stun = rand_number(1, 4);
    break;
  }

  ch->points.max_hit += MAX(1, add_hp);
  ch->points.max_move += MAX(1, add_move);
  ch->points.max_stun += MAX(1, add_stun);

  if (GET_LEVEL(ch) >= 1)
    ch->points.max_mana += add_mana;
    ch->points.max_stun += add_stun;

  if (IS_MAGIC_USER(ch) || IS_CLERIC(ch))
    GET_PRACTICES(ch) += MAX(2, wis_app[GET_WIS(ch)].bonus);
  else
    GET_PRACTICES(ch) += MIN(2, MAX(1, wis_app[GET_WIS(ch)].bonus));

  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    for (i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;
    SET_BIT_AR(PRF_FLAGS(ch), PRF_HOLYLIGHT);
  }

  snoop_check(ch);
  save_char(ch);
}

/* This simply calculates the backstab multiplier based on a character's level.
 * This used to be an array, but was changed to be a function so that it would
 * be easier to add more levels to your MUD.  This doesn't really create a big
 * performance hit because it's not used very often. */
int backstab_mult(int level)
{
  if (level <= 1)
    return 2;	  /* level 1 - 1 */
  else
    return 20;	  /* immortals */
}

/* invalid_class is used by handler.c to determine if a piece of equipment is
 * usable by a particular class, based on the ITEM_ANTI_{class} bitvectors. */
int invalid_class(struct char_data *ch, struct obj_data *obj)
{
  if (OBJ_FLAGGED(obj, ITEM_ANTI_MAGIC_USER) && IS_MAGIC_USER(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_CLERIC) && IS_CLERIC(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_WARRIOR) && IS_WARRIOR(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_THIEF) && IS_THIEF(ch))
    return TRUE;

  return FALSE;
}

/* SPELLS AND SKILLS.  This area defines which spells are assigned to which
 * classes, and the minimum level the character must be to use the spell or
 * skill. */
void init_spell_levels(void)
{
  /* MAGES */
  spell_level(SPELL_MAGIC_MISSILE, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_DETECT_INVIS, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_DETECT_MAGIC, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_CHILL_TOUCH, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_INFRAVISION, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_INVISIBLE, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_ARMOR, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_BURNING_HANDS, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_LOCATE_OBJECT, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_STRENGTH, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_SHOCKING_GRASP, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_SLEEP, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_LIGHTNING_BOLT, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_BLINDNESS, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_DETECT_POISON, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_COLOR_SPRAY, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_ENERGY_DRAIN, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_CURSE, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_POISON, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_FIREBALL, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_CHARM, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_IDENTIFY, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_FLY, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_ENCHANT_WEAPON, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_CLONE, CLASS_MAGIC_USER, 1);
  spell_level(SKILL_UNARMED, CLASS_MAGIC_USER, 1);
  spell_level(SKILL_SHIELD_USE, CLASS_MAGIC_USER, 1);

  /* CLERICS */
  spell_level(SPELL_CURE_LIGHT, CLASS_CLERIC, 1);
  spell_level(SPELL_ARMOR, CLASS_CLERIC, 1);
  spell_level(SPELL_CREATE_FOOD, CLASS_CLERIC, 1);
  spell_level(SPELL_CREATE_WATER, CLASS_CLERIC, 1);
  spell_level(SPELL_DETECT_POISON, CLASS_CLERIC, 1);
  spell_level(SPELL_DETECT_ALIGN, CLASS_CLERIC, 1);
  spell_level(SPELL_CURE_BLIND, CLASS_CLERIC, 1);
  spell_level(SPELL_BLESS, CLASS_CLERIC, 1);
  spell_level(SPELL_DETECT_INVIS, CLASS_CLERIC, 1);
  spell_level(SPELL_BLINDNESS, CLASS_CLERIC, 1);
  spell_level(SPELL_INFRAVISION, CLASS_CLERIC, 1);
  spell_level(SPELL_PROT_FROM_EVIL, CLASS_CLERIC, 1);
  spell_level(SPELL_POISON, CLASS_CLERIC, 1);
  spell_level(SPELL_GROUP_ARMOR, CLASS_CLERIC, 1);
  spell_level(SPELL_CURE_CRITIC, CLASS_CLERIC, 1);
  spell_level(SPELL_SUMMON, CLASS_CLERIC, 1);
  spell_level(SPELL_REMOVE_POISON, CLASS_CLERIC, 1);
  spell_level(SPELL_IDENTIFY, CLASS_CLERIC, 1);
  spell_level(SPELL_WORD_OF_RECALL, CLASS_CLERIC, 1);
  spell_level(SPELL_DARKNESS, CLASS_CLERIC, 1);
  spell_level(SPELL_EARTHQUAKE, CLASS_CLERIC, 1);
  spell_level(SPELL_DISPEL_EVIL, CLASS_CLERIC, 1);
  spell_level(SPELL_DISPEL_GOOD, CLASS_CLERIC, 1);
  spell_level(SPELL_SANCTUARY, CLASS_CLERIC, 1);
  spell_level(SPELL_CALL_LIGHTNING, CLASS_CLERIC, 1);
  spell_level(SPELL_HEAL, CLASS_CLERIC, 1);
  spell_level(SPELL_CONTROL_WEATHER, CLASS_CLERIC, 1);
  spell_level(SPELL_SENSE_LIFE, CLASS_CLERIC, 1);
  spell_level(SPELL_HARM, CLASS_CLERIC, 1);
  spell_level(SPELL_GROUP_HEAL, CLASS_CLERIC, 1);
  spell_level(SPELL_REMOVE_CURSE, CLASS_CLERIC, 1);
  spell_level(SKILL_UNARMED, CLASS_CLERIC, 1);
  spell_level(SKILL_PIERCING_WEAPONS, CLASS_CLERIC, 1);
  spell_level(SKILL_SHIELD_USE, CLASS_CLERIC, 1);

  /* THIEVES */
  spell_level(SKILL_SNEAK, CLASS_THIEF, 1);
  spell_level(SKILL_PICK_LOCK, CLASS_THIEF, 1);
  spell_level(SKILL_BACKSTAB, CLASS_THIEF, 1);
  spell_level(SKILL_STEAL, CLASS_THIEF, 1);
  spell_level(SKILL_HIDE, CLASS_THIEF, 1);
  spell_level(SKILL_TRACK, CLASS_THIEF, 1);
  spell_level(SKILL_UNARMED, CLASS_THIEF, 1);
  spell_level(SKILL_PIERCING_WEAPONS, CLASS_THIEF, 1);
  spell_level(SKILL_STABBING_WEAPONS, CLASS_THIEF, 1);
  spell_level(SKILL_SHIELD_USE, CLASS_THIEF, 1);

  /* WARRIORS */
  spell_level(SKILL_KICK, CLASS_WARRIOR, 1);
  spell_level(SKILL_RESCUE, CLASS_WARRIOR, 1);
  spell_level(SKILL_BANDAGE, CLASS_WARRIOR, 1);
  spell_level(SKILL_TRACK, CLASS_WARRIOR, 1);
  spell_level(SKILL_BASH, CLASS_WARRIOR, 1);
  spell_level(SKILL_WHIRLWIND, CLASS_WARRIOR, 1);
  spell_level(SKILL_FORAGE, CLASS_WARRIOR, 1);
  spell_level(SKILL_UNARMED, CLASS_WARRIOR, 1);
  spell_level(SKILL_DISARM, CLASS_WARRIOR, 1);
  spell_level(SKILL_PIERCING_WEAPONS, CLASS_WARRIOR, 1);
  spell_level(SKILL_SLASHING_WEAPONS, CLASS_WARRIOR, 1);
  spell_level(SKILL_BLUDGEONING_WEAPONS, CLASS_WARRIOR, 1);
  spell_level(SKILL_SHIELD_USE, CLASS_WARRIOR, 1);
}

/* This is the exp given to implementors -- it must always be greater than the
 * exp required for immortality, plus at least 20,000 or so. */
#define EXP_MAX  10000000

/* Function to return the exp required for each class/level */
int level_exp(int chclass, int level)
{
  if (level > LVL_IMPL || level < 0) {
    log("SYSERR: Requesting exp for invalid level %d!", level);
    return 0;
  }

  /* Gods have exp close to EXP_MAX.  This statement should never have to
   * changed, regardless of how many mortal or immortal levels exist. */
   if (level > LVL_IMMORT) {
     return EXP_MAX - ((LVL_IMPL - level) * 1000);
   }

  /* Exp required for normal mortals is below */
  switch (chclass) {

    case CLASS_MAGIC_USER:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      /* add new levels here */
      case LVL_IMMORT: return 8000000;
    }
    break;

    case CLASS_CLERIC:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      /* add new levels here */
      case LVL_IMMORT: return 7000000;
    }
    break;

    case CLASS_THIEF:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      /* add new levels here */
      case LVL_IMMORT: return 7000000;
    }
    break;

    case CLASS_WARRIOR:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      /* add new levels here */
      case LVL_IMMORT: return 8000000;
    }
    break;
  }

  /* This statement should never be reached if the exp tables in this function
   * are set up properly.  If you see exp of 123456 then the tables above are
   * incomplete. */
  log("SYSERR: XP tables not set up correctly in class.c!");
  return 123456;
}

/* Default titles of male characters. */
const char *title_male(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Man";
  if (level == LVL_IMPL)
    return "the Implementor";

  switch (chclass) {

    case CLASS_MAGIC_USER:
    switch (level) {
      case  1: return "the Apprentice of Magic";
      case LVL_IMMORT: return "the Immortal Warlock";
      case LVL_GOD: return "the Avatar of Magic";
      case LVL_GRGOD: return "the God of Magic";
      default: return "the Mage";
    }

    case CLASS_CLERIC:
    switch (level) {
      case  1: return "the Believer";
      case LVL_IMMORT: return "the Immortal Cardinal";
      case LVL_GOD: return "the Inquisitor";
      case LVL_GRGOD: return "the God of Good and Evil";
      default: return "the Cleric";
    }

    case CLASS_THIEF:
    switch (level) {
      case  1: return "the Pilferer";
      case LVL_IMMORT: return "the Immortal Assassin";
      case LVL_GOD: return "the Demi God of Thieves";
      case LVL_GRGOD: return "the God of Thieves and Tradesmen";
      default: return "the Thief";
    }

    case CLASS_WARRIOR:
    switch(level) {
      case  1: return "the Swordpupil";
      case LVL_IMMORT: return "the Immortal Warlord";
      case LVL_GOD: return "the Extirpator";
      case LVL_GRGOD: return "the God of War";
      default: return "the Warrior";
    }
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}

/* Default titles of female characters. */
const char *title_female(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Woman";
  if (level == LVL_IMPL)
    return "the Implementress";

  switch (chclass) {

    case CLASS_MAGIC_USER:
    switch (level) {
      case  1: return "the Apprentice of Magic";
      case LVL_IMMORT: return "the Immortal Enchantress";
      case LVL_GOD: return "the Empress of Magic";
      case LVL_GRGOD: return "the Goddess of Magic";
      default: return "the Witch";
    }

    case CLASS_CLERIC:
    switch (level) {
      case  1: return "the Believer";
      case LVL_IMMORT: return "the Immortal Priestess";
      case LVL_GOD: return "the Inquisitress";
      case LVL_GRGOD: return "the Goddess of Good and Evil";
      default: return "the Cleric";
    }

    case CLASS_THIEF:
    switch (level) {
      case  1: return "the Pilferess";
      case LVL_IMMORT: return "the Immortal Assassin";
      case LVL_GOD: return "the Demi Goddess of Thieves";
      case LVL_GRGOD: return "the Goddess of Thieves and Tradesmen";
      default: return "the Thief";
    }

    case CLASS_WARRIOR:
    switch(level) {
      case  1: return "the Swordpupil";
      case LVL_IMMORT: return "the Immortal Lady of War";
      case LVL_GOD: return "the Queen of Destruction";
      case LVL_GRGOD: return "the Goddess of War";
      default: return "the Warrior";
    }
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}
