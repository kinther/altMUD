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

/* Table attributes we want, such as name, id num, last logon, email, rpp */
#define AT_ANAME(i) (account_table[(i)].name)
#define AT_IDNUM(i) (account_table[(i)].id)
#define AT_LLAST(i) (account_table[(i)].last)
/* Not sure if we need these just yet?
#define AT_EMAIL(i) (account_table[(i)].email)
#define AT_RPP(i)   (account_table[(i)].rpp)
*/

/* New version to build account index for ASCII Account Files. Generate index
 * table for the account file. */
void build_account_index(void)
{
  int rec_count = 0, i;
  FILE *acct_index;
  char index_name[40], line[256];
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
    sscanf(line, "%ld %s %ld", &account_table[i].id, arg2,
      (long *)&account_table[i].last);
    CREATE(account_table[i].name, char, strlen(arg2) + 1);
    strcpy(account_table[i].name, arg2);
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
  for (i = 0; (account_table[pos].name[i] = LOWER(name[i])); i++);

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
  free(AT_ANAME(pos));

  /* Move every other item in the list down the index */
  for (i = pos+1; i <= top_of_a_table; i++) {
    AT_ANAME(i-1) = AT_ANAME(i);
    AT_IDNUM(i-1) = AT_IDNUM(i);
    AT_LLAST(i-1) = AT_LLAST(i);
  }
  AT_ANAME(top_of_a_table) = NULL;

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
  char index_name[50];
  FILE *index_file;

  sprintf(index_name, "%s%s", LIB_ACCTFILES, INDEX_FILE);
  if (!(index_file = fopen(index_name, "w"))) {
    log("SYSERR: Could not write account index file");
    return;
  }

  for (i = 0; i <= top_of_a_table; i++)
    if (*account_table[i].name) {
      fprintf(index_file, "%ld %s %ld\n", account_table[i].id,
	account_table[i].name, (long)account_table[i].last);
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

long get_acct_id_by_name(const char *name)
{
  int i;

  for (i = 0; i <= top_of_a_table; i++)
    if (!str_cmp(account_table[i].name, name))
      return (account_table[i].id);

  return (-1);
}

char *get_acct_name_by_id(long id)
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
int load_acct(const char *name, struct account_data *acct)
{
  int id;
  FILE *fl;
  char filename[40];
  char buf[128], line[MAX_INPUT_LENGTH + 1], tag[6];

  if ((id = get_atable_by_name(name)) < 0)
    return (-1);
  else {
    if (!get_filename(filename, sizeof(filename), ACCT_FILE, account_table[id].name))
      return (-1);
    if (!(fl = fopen(filename, "r"))) {
      mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Couldn't open account file %s", filename);
      return (-1);
    }

    while (get_line(fl, line)) {
      tag_argument(line, tag);

      switch (*tag) {
      case 'A':
	     break;

      case 'B':
	     if (!strcmp(tag, "Badp"))	GET_ACCOUNT_BAD_PWS(acct)		= atoi(line);
	     break;

      case 'C':
	     break;

      case 'D':
	     break;

      case 'E':
	     break;

      case 'F':
	     break;

      case 'G':
	     break;

      case 'H':
	       break;

      case 'I':
	      break;

      case 'L':
	      break;

      case 'M':
	     break;

      case 'N':
	      break;

      case 'O':
        break;

      case 'P':
       break;

      case 'Q':
        break;

      case 'R':
	     break;

      case 'S':
	      break;

      case 'T':
	     break;

      case 'V':
       break;

      case 'W':
	     break;

      default:
	     sprintf(buf, "SYSERR: Unknown tag %s in afile %s", tag, name);
      }
    }
  }

  fclose(fl);
  return(id);
}

/* Write the vital data of a account to the account file. */
/* This is the ASCII Account Files save routine. */
void save_acct(struct account_data * acct)
{
  FILE *fl;
  char filename[40];
  int i, id, save_index = FALSE;

  if (!get_filename(filename, sizeof(filename), ACCT_FILE, GET_ACCOUNT_NAME(acct)))
    return;
  if (!(fl = fopen(filename, "w"))) {
    mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Couldn't open account file %s for write", filename);
    return;
  }

  if (GET_ACCOUNT_NAME(acct))				fprintf(fl, "Name: %s\n", GET_ACCOUNT_NAME(acct));
  if (GET_ACCOUNT_PW(acct))				fprintf(fl, "Pass: %s\n", GET_ACCOUNT_PW(acct));
  if (GET_ACCOUNT_EMAIL(acct))      fprintf(fl, "Email: %s\n", GET_ACCOUNT_EMAIL(acct));
  if (GET_ACCOUNT_HOST(acct)				fprintf(fl, "Host: %s\n", GET_ACCOUNT_HOST(acct);

  fclose(fl);

  if ((id = get_atable_by_name(GET_ACCOUNT_NAME(acct))) < 0)
    return;

  /* update the account in the account index */
  if (account_table[id].last != acct->account_specials->time.logon) {
    save_index = TRUE;
    account_table[id].last = acct->account_specials->time.logon;
  }
  i = account_table[id].flags;

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
  log("ACLEAN: %s Last: %s",
	account_table[afilepos].name,	timestr);
  account_table[afilepos].name[0] = '\0';

  /* Update index table. */
  remove_account_from_index(afilepos);

  save_account_index();
}

void clean_afiles(void)
{
  int i;

  for (i = 0; i <= top_of_a_table; i++) {
    /* We only want to go further if the account isn't protected from deletion
     * and hasn't already been deleted. */
    if (!IS_SET(account_table[i].flags, PINDEX_NODELETE) &&
        *account_table[i].name) {
      /* If the account is already flagged for deletion, then go ahead and get
       * rid of him. */
      if (IS_SET(account_table[i].flags, PINDEX_DELETED)) {
	remove_account(i);
      }
        /* If we got this far and the accounts hasn't been kicked out, then he
	 * can stay a little while longer. */
      }
    }
}
