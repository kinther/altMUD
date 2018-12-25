/**************************************************************************
*  File: accounts.c                                     Part of CircleRPI *
*  Usage: Account loading/saving and utility routines.                    *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "handler.h"
#include "pfdefaults.h"
#include "dg_scripts.h"
#include "comm.h"
#include "interpreter.h"
#include "genolc.h" /* for strip_cr */
#include "config.h" /* for pclean_criteria[] */
#include "dg_scripts.h" /* To enable saving of account variables to disk */
#include "quest.h"

#define AT_ANAME(i) (account_table[(i)].name)
#define AT_IDNUM(i) (account_table[(i)].id)
#define AT_LLAST(i) (account_table[(i)].last)

/* New version to build account index for ASCII Account Files. Generate index
 * table for the account file. */
void build_account_index(void)
{
  int rec_count = 0, i;
  FILE *acct_index;
  char index_name[40], line[256], bits[64];
  char arg2[80];

  sprintf(index_name, "%s%s", LIB_ACCTFILES, INDEX_FILE);
  if (!(acct_index = fopen(index_name, "r"))) {
    top_of_a_table = -1;
    log("No account index file!  First new account will be Admin!");
    return;
  }

  while (get_line(acct_index, line))
    if (*line != '~')
      rec_count++;
  rewind(acct_index);

  if (rec_count == 0) {
    account_table = NULL;
    top_of_a_table = -1;
    return;
  }

  CREATE(account_table, struct account_index_element, rec_count);
  for (i = 0; i < rec_count; i++) {
    get_line(acct_index, line);
    sscanf(line, "%ld %s %d %s %ld", &account_table[i].id, arg2,
      &account_table[i].level, bits, (long *)&account_table[i].last);
    CREATE(account_table[i].name, char, strlen(arg2) + 1);
    strcpy(account_table[i].name, arg2);
    account_table[i].flags = asciiflag_conv(bits);
    top_a_idnum = MAX(top_a_idnum, account_table[i].id);
  }

  fclose(acct_index);
  top_of_a_file = top_of_a_table = i - 1;
}

/* Create a new entry in the in-memory index table for the account file. If the
 * name already exists, by overwriting a deleted account, then we re-use the
 * old position. */
int create_acct_entry(char *name)
{
  int i, pos;

  if (top_of_a_table == -1) {	/* no table */
    pos = top_of_a_table = 0;
    CREATE(account_table, struct account_index_element, 1);
  } else if ((pos = get_atable_by_name(name)) == -1) {	/* new name */
    i = ++top_of_a_table + 1;

    RECREATE(account_table, struct account_index_element, i);
    pos = top_of_a_table;
  }

  CREATE(account_table[pos].name, char, strlen(name) + 1);

  /* copy lowercase equivalent of name to table field */
  for (i = 0; (account_table[pos].name[i] = LOWER(name[i])); i++)
    /* Nothing */;

  /* clear the bitflag in case we have garbage data */
  account_table[pos].flags = 0;

  return (pos);
}


/* Remove an entry from the in-memory account index table.               *
 * Requires the 'pos' value returned by the get_atable_by_name function */
static void remove_account_from_index(int pos)
{
  int i;

  if (pos < 0 || pos > top_of_a_table)
    return;

  /* We only need to free the name string */
  free(PT_PNAME(pos));

  /* Move every other item in the list down the index */
  for (i = pos+1; i <= top_of_a_table; i++) {
    PT_PNAME(i-1) = PT_PNAME(i);
    PT_IDNUM(i-1) = PT_IDNUM(i);
    PT_LEVEL(i-1) = PT_LEVEL(i);
    PT_FLAGS(i-1) = PT_FLAGS(i);
    PT_LLAST(i-1) = PT_LLAST(i);
  }
  PT_PNAME(top_of_a_table) = NULL;

  /* Reduce the index table counter */
  top_of_a_table--;

  /* And reduce the size of the table */
  if (top_of_a_table >= 0)
    RECREATE(account_table, struct account_index_element, (top_of_a_table+1));
  else {
    free(account_table);
    account_table = NULL;
  }
}

/* This function necessary to save a seperate ASCII account index */
void save_account_index(void)
{
  int i;
  char index_name[50], bits[64];
  FILE *index_file;

  sprintf(index_name, "%s%s", LIB_ACCTFILES, INDEX_FILE);
  if (!(index_file = fopen(index_name, "w"))) {
    log("SYSERR: Could not write account index file");
    return;
  }

  for (i = 0; i <= top_of_a_table; i++)
    if (*account_table[i].name) {
      sprintascii(bits, account_table[i].flags);
      fprintf(index_file, "%ld %s %d %s %ld\n", account_table[i].id,
	account_table[i].name, account_table[i].level, *bits ? bits : "0",
        (long)account_table[i].last);
    }
  fprintf(index_file, "~\n");

  fclose(index_file);
}

void free_account_index(void)
{
  int tp;

  if (!account_table)
    return;

  for (tp = 0; tp <= top_of_a_table; tp++)
    if (account_table[tp].name)
      free(account_table[tp].name);

  free(account_table);
  account_table = NULL;
  top_of_a_table = 0;
}

long get_atable_by_name(const char *name)
{
  int i;

  for (i = 0; i <= top_of_a_table; i++)
    if (!str_cmp(account_table[i].name, name))
      return (i);

  return (-1);
}

long get_id_by_name(const char *name)
{
  int i;

  for (i = 0; i <= top_of_a_table; i++)
    if (!str_cmp(account_table[i].name, name))
      return (account_table[i].id);

  return (-1);
}

char *get_name_by_id(long id)
{
  int i;

  for (i = 0; i <= top_of_a_table; i++)
    if (account_table[i].id == id)
      return (account_table[i].name);

  return (NULL);
}

/* Stuff related to the save/load account system. */
/* New load_char reads ASCII account Files. Load an account, TRUE if loaded, FALSE
 * if not. */
int load_acct(const char *name, struct account_data *ch)
{
  int id, i;
  FILE *fl;
  char filename[40];
  char buf[128], buf2[128], line[MAX_INPUT_LENGTH + 1], tag[6];
  char f1[128], f2[128], f3[128], f4[128];
  trig_data *t = NULL;
  trig_rnum t_rnum = NOTHING;

  if ((id = get_atable_by_name(name)) < 0)
    return (-1);
  else {
    if (!get_filename(filename, sizeof(filename), ACCT_FILE, account_table[id].name))
      return (-1);
    if (!(fl = fopen(filename, "r"))) {
      mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Couldn't open account file %s", filename);
      return (-1);
    }

    /* Character initializations. Necessary to keep some things straight.
    ch->affected = NULL;
    for (i = 1; i <= MAX_SKILLS; i++)
      GET_SKILL(ch, i) = 0;
    GET_SEX(ch) = PFDEF_SEX;
    GET_CLASS(ch) = PFDEF_CLASS;
    GET_LEVEL(ch) = PFDEF_LEVEL;
    GET_HEIGHT(ch) = PFDEF_HEIGHT;
    GET_WEIGHT(ch) = PFDEF_WEIGHT;
    GET_ALIGNMENT(ch) = PFDEF_ALIGNMENT;
    for (i = 0; i < NUM_OF_SAVING_THROWS; i++)
      GET_SAVE(ch, i) = PFDEF_SAVETHROW;
    GET_LOADROOM(ch) = PFDEF_LOADROOM;
    GET_INVIS_LEV(ch) = PFDEF_INVISLEV;
    GET_FREEZE_LEV(ch) = PFDEF_FREEZELEV;
    GET_WIMP_LEV(ch) = PFDEF_WIMPLEV;
    GET_COND(ch, HUNGER) = PFDEF_HUNGER;
    GET_COND(ch, THIRST) = PFDEF_THIRST;
    GET_COND(ch, DRUNK) = PFDEF_DRUNK;
    GET_BAD_PWS(ch) = PFDEF_BADPWS;
    GET_PRACTICES(ch) = PFDEF_PRACTICES;
    GET_GOLD(ch) = PFDEF_GOLD;
    GET_BANK_GOLD(ch) = PFDEF_BANK;
    GET_EXP(ch) = PFDEF_EXP;
    GET_HITROLL(ch) = PFDEF_HITROLL;
    GET_DAMROLL(ch) = PFDEF_DAMROLL;
    GET_AC(ch) = PFDEF_AC;
    ch->real_abils.str = PFDEF_STR;
    ch->real_abils.str_add = PFDEF_STRADD;
    ch->real_abils.dex = PFDEF_DEX;
    ch->real_abils.intel = PFDEF_INT;
    ch->real_abils.wis = PFDEF_WIS;
    ch->real_abils.con = PFDEF_CON;
    ch->real_abils.cha = PFDEF_CHA;
    GET_HIT(ch) = PFDEF_HIT;
    GET_MAX_HIT(ch) = PFDEF_MAXHIT;
    GET_MANA(ch) = PFDEF_MANA;
    GET_MAX_MANA(ch) = PFDEF_MAXMANA;
    GET_MOVE(ch) = PFDEF_MOVE;
    GET_MAX_MOVE(ch) = PFDEF_MAXMOVE;
    GET_STUN(ch) = PFDEF_STUN;
    GET_MAX_STUN(ch) = PFDEF_MAXSTUN;
    GET_OLC_ZONE(ch) = PFDEF_OLC;
    GET_PAGE_LENGTH(ch) = PFDEF_PAGELENGTH;
    GET_SCREEN_WIDTH(ch) = PFDEF_SCREENWIDTH;
    GET_ALIASES(ch) = NULL;
    SITTING(ch) = NULL;
    NEXT_SITTING(ch) = NULL;
    GET_QUESTPOINTS(ch) = PFDEF_QUESTPOINTS;
    GET_QUEST_COUNTER(ch) = PFDEF_QUESTCOUNT;
    GET_QUEST(ch) = PFDEF_CURRQUEST;
    GET_NUM_QUESTS(ch) = PFDEF_COMPQUESTS;
    GET_LAST_MOTD(ch) = PFDEF_LASTMOTD;
    GET_LAST_NEWS(ch) = PFDEF_LASTNEWS;

    for (i = 0; i < AF_ARRAY_MAX; i++)
      AFF_FLAGS(ch)[i] = PFDEF_AFFFLAGS;
    for (i = 0; i < PM_ARRAY_MAX; i++)
      PLR_FLAGS(ch)[i] = PFDEF_PLRFLAGS;
    for (i = 0; i < PR_ARRAY_MAX; i++)
      PRF_FLAGS(ch)[i] = PFDEF_PREFFLAGS;
      */

    while (get_line(fl, line)) {
      tag_argument(line, tag);

      switch (*tag) {
      case 'A':
	     break;

      case 'B':
	     if (!strcmp(tag, "Badp"))	GET_ACCT_BAD_PWS(ch)		= atoi(line);
	     break;

      case 'C':
	     break;

      case 'D':
	     if (!strcmp(tag, "Desc"))	ch->account.description	= fread_string(fl, buf2);
	     break;

      case 'E':
	     break;

      case 'F':
	     break;

      case 'G':
	     break;

      case 'H':
        if (!strcmp(tag, "Host")) {
          if (GET_HOST(ch))
            free(GET_HOST(ch));
            GET_HOST(ch) = strdup(line);
        }
	       break;

      case 'I':
	     if (!strcmp(tag, "Id  "))	GET_IDNUM(ch)		= atol(line);
	      break;

      case 'L':
	     if (!strcmp(tag, "Last"))	ch->account.time.logon	= atol(line);
	      break;

      case 'M':
	     break;

      case 'N':
	     if (!strcmp(tag, "Name"))	GET_PC_NAME(ch)	= strdup(line);
	      break;

      case 'O':
        break;

      case 'P':
       if (!strcmp(tag, "Page"))  GET_PAGE_LENGTH(ch) = atoi(line);
	     else if (!strcmp(tag, "Pass"))	strcpy(GET_PASSWD(ch), line);
	     else if (!strcmp(tag, "Plyd"))	ch->account.time.played	= atoi(line);
       break;

      case 'Q':
        break;

      case 'R':
	     break;

      case 'S':
        if (!strcmp(tag, "ScrW"))  GET_SCREEN_WIDTH(ch) = atoi(line);
	      break;

      case 'T':
	     break;

      case 'V':
	     if (!strcmp(tag, "Vars"))	read_saved_vars_ascii(fl, ch, atoi(line));
      break;

      case 'W':
	     break;

      default:
	     sprintf(buf, "SYSERR: Unknown tag %s in afile %s", tag, name);
      }
    }
  }

  affect_total(ch);

  fclose(fl);
  return(id);
}

/* Write the vital data of a account to the account file. */
/* This is the ASCII Account Files save routine. */
void save_acct(struct account_data * acct)
{
  FILE *fl;
  char filename[40], buf[MAX_STRING_LENGTH], bits[127], bits2[127], bits3[127], bits4[127];
  int i, j, id, save_index = FALSE;
  trig_data *t;

  if (IS_NPC(ch) || GET_AFILEPOS(ch) < 0)
    return;

  if (!get_filename(filename, sizeof(filename), ACCT_FILE, GET_ACCOUNT_NAME(acct)))
    return;
  if (!(fl = fopen(filename, "w"))) {
    mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Couldn't open account file %s for write", filename);
    return;
  }

  if (GET_ACCOUNT_NAME(acct))				fprintf(fl, "Name: %s\n", GET_ACCOUNT_NAME(acct));
  if (GET_ACCOUNT_PW(acct))				fprintf(fl, "Pass: %s\n", GET_ACCOUNT_PW(acct));
  if (GET_ACCOUNT_EMAIL(acct))      fprintf(fl, "Email: %s\n", GET_ACCOUNT_EMAIL(acct));
  if (GET_HOST(acct))				fprintf(fl, "Host: %s\n", GET_HOST(acct));

  fclose(fl);

  if ((id = get_atable_by_name(GET_ACCOUNT_NAME(ch))) < 0)
    return;

  /* update the account in the account index */
  if (account_table[id].last != ch->account.time.logon) {
    save_index = TRUE;
    account_table[id].last = ch->account.time.logon;
  }
  i = account_table[id].flags;
  if (PLR_FLAGGED(ch, PLR_DELETED))
    SET_BIT(account_table[id].flags, PINDEX_DELETED);
  else
    REMOVE_BIT(account_table[id].flags, PINDEX_DELETED);
  if (PLR_FLAGGED(ch, PLR_NODELETE) || PLR_FLAGGED(ch, PLR_CRYO))
    SET_BIT(account_table[id].flags, PINDEX_NODELETE);
  else
    REMOVE_BIT(account_table[id].flags, PINDEX_NODELETE);

  if (PLR_FLAGGED(ch, PLR_FROZEN) || PLR_FLAGGED(ch, PLR_NOWIZLIST))
    SET_BIT(account_table[id].flags, PINDEX_NOWIZLIST);
  else
    REMOVE_BIT(account_table[id].flags, PINDEX_NOWIZLIST);

  if (account_table[id].flags != i || save_index)
    save_account_index();
}

/* Separate a 4-character id tag from the data it precedes */
void tag_argument(char *argument, char *tag)
{
  char *tmp = argument, *ttag = tag, *wrt = argument;
  int i;

  for (i = 0; i < 4; i++)
    *(ttag++) = *(tmp++);
  *ttag = '\0';

  while (*tmp == ':' || *tmp == ' ')
    tmp++;

  while (*tmp)
    *(wrt++) = *(tmp++);
  *wrt = '\0';
}

/* Stuff related to the account file cleanup system. */

/* remove_account() removes all files associated with a account who is self-deleted,
 * deleted by an immortal, or deleted by the auto-wipe system (if enabled). */
void remove_account(int afilepos)
{
  char filename[MAX_STRING_LENGTH], timestr[25];
  int i;

  if (!*account_table[afilepos].name)
    return;

  /* Unlink all account-owned files */
  for (i = 0; i < MAX_FILES; i++) {
    if (get_filename(filename, sizeof(filename), i, account_table[afilepos].name))
      unlink(filename);
  }

  strftime(timestr, sizeof(timestr), "%c", localtime(&(account_table[afilepos].last)));
  log("ACLEAN: %s Lev: %d Last: %s",
	account_table[afilepos].name, account_table[afilepos].level,
	timestr);
  account_table[afilepos].name[0] = '\0';

  /* Update index table. */
  remove_account_from_index(afilepos);

  save_account_index();
}

void clean_afiles(void)
{
  int i, ci;

  for (i = 0; i <= top_of_a_table; i++) {
    /* We only want to go further if the account isn't protected from deletion
     * and hasn't already been deleted. */
    if (!IS_SET(account_table[i].flags, PINDEX_NODELETE) &&
        *account_table[i].name) {
      /* If the account is already flagged for deletion, then go ahead and get
       * rid of him. */
      if (IS_SET(account_table[i].flags, PINDEX_DELETED)) {
	remove_account(i);
      } else {
        /* Check to see if the account has overstayed his welcome based on level. */
	for (ci = 0; pclean_criteria[ci].level > -1; ci++) {
	  if (account_table[i].level <= pclean_criteria[ci].level &&
	      ((time(0) - account_table[i].last) >
	       (pclean_criteria[ci].days * SECS_PER_REAL_DAY))) {
	    remove_account(i);
	    break;
	  }
	}
        /* If we got this far and the accounts hasn't been kicked out, then he
	 * can stay a little while longer. */
      }
    }
  }
  /* After everything is done, we should rebuild account_index and remove the
   * entries of the accounts that were just deleted. */
}
