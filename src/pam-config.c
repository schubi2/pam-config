/* Copyright (C) 2006, 2007 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@thkukuk.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <getopt.h>

#include <pam-config.h>

#include "supported-modules.h"

#define CONF_ACCOUNT CONFDIR"/pam.d/common-account"
#define CONF_ACCOUNT_PC CONF_ACCOUNT"-pc"
#define CONF_AUTH CONFDIR"/pam.d/common-auth"
#define CONF_AUTH_PC CONF_AUTH"-pc"
#define CONF_PASSWORD CONFDIR"/pam.d/common-password"
#define CONF_PASSWORD_PC CONF_PASSWORD"-pc"
#define CONF_SESSION CONFDIR"/pam.d/common-session"
#define CONF_SESSION_PC CONF_SESSION"-pc"

int debug = 0;

static void
print_usage (FILE *stream, const char *program)
{
  fprintf (stream, _("Usage: %s -a|-c|-d [...]\n"),
           program);
}

/* Print the version information.  */
static void
print_version (const char *program, const char *years)
{
  fprintf (stdout, "%s (%s) %s\n", program, PACKAGE, VERSION);
  fprintf (stdout, _("\
Copyright (C) %s Thorsten Kukuk.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), years);
  /* fprintf (stdout, _("Written by %s.\n"), "Thorsten Kukuk"); */
}

static void
print_error (const char *program)
{
  fprintf (stderr,
           _("Try `%s --help' or `%s --usage' for more information.\n"),
           program, program);
}

static void
print_help (const char *program)
{
  print_usage (stdout, program);
  fprintf (stdout, _("%s - create PAM config files\n\n"), program);

  fputs (_("  -a, --add         Add options/PAM modules\n"), stdout);
  fputs (_("  -c, --create      Create new configuration\n"), stdout);
  fputs (_("  -d, --delete      Remove options/PAM modules\n"), stdout);
  fputs (_("      --initialize  Convert old config and create new one\n"),
	 stdout);
  fputs (_("      --update      Read current config and write them new\n"),
         stdout);
  fputs (_("  -q, --query       Query for installed modules and options\n"),
	 stdout);
  fputs (_("      --help        Give this help list\n"), stdout);
  fputs (_("  -u, --usage       Give a short usage message\n"), stdout);
  fputs (_("  -v, --version     Print program version\n"), stdout);
}

static int
check_symlink (const char *old, const char *new)
{
  if (access (new, F_OK) == -1)
    {
      if (symlink (old, new) != 0)
	{
	  fprintf (stderr,
		   _("Error activating %s (%m)\n"), new);
	      fprintf (stderr,
		       _("New config from %s is not in use!\n"),
		       old);
	      return 1;
	}
      return 0;
    }
  else
    {
      char buf[1024];

      memset (&buf, 0, sizeof (buf));
      if (readlink (new, buf, sizeof (buf)) <= 0 ||
          strcmp (old, buf) != 0)
	{
	  fprintf (stderr,
		   _("File %s is no symlink to %s.\n"), new, old);
	  fprintf (stderr,
		   _("New config from %s is is not in use!\n"),
		   old);
	  return 1;
	}
      return 0;
    }
}

/* make backup from original file and symlink autogenerated one
   to the original name.  */
static int
relink (const char *file, const char *file_pc, const char *file_bak)
{
  if (link (file, file_bak) != 0 ||
      unlink (file) != 0 ||
      symlink (file_pc, file) != 0)
	{
	  fprintf (stderr,
		   _("Error activating %s (%m)\n"), file);
	  fprintf (stderr,
		   _("New config from %s is not in use!\n"), file_pc);
	  return 1;
	}
  return 0;
}

int
main (int argc, char *argv[])
{
  const char *program = "pam-config";
  int m_add = 0, m_create = 0, m_delete = 0, m_init = 0, m_update = 0;
  int m_query = 0;
  int force = 0;
  int opt_val = 1;
  int retval = 0;
  option_set_t *opt_set;
  config_file_t config_account, config_auth,
    config_password, config_session;

  memset (&config_account, 0, sizeof (config_account));
  memset (&config_auth, 0, sizeof (config_auth));
  memset (&config_password, 0, sizeof (config_password));
  memset (&config_session, 0, sizeof (config_session));

  config_account.type = ACCOUNT;
  config_auth.type = AUTH;
  config_password.type = PASSWORD;
  config_session.type = SESSION;

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  openlog (program, LOG_ODELAY | LOG_PID, LOG_AUTHPRIV);

  if (argc < 2)
    {
      print_error (program);
      return 1;
    }
  else if (strcmp (argv[1], "--debug") == 0)
    {
      debug = 1;
      argc--;
      argv++;
    }


  if (argc < 2)
    {
      print_error (program);
      return 1;
    }
  else if (strcmp (argv[1], "-a") == 0 || strcmp (argv[1], "--add") == 0)
    {
      m_add = 1;
      argc--;
      argv++;
    }
  else if (strcmp (argv[1], "-c") == 0 || strcmp (argv[1], "--create") == 0)
    {
      m_create = 1;
      argc--;
      argv++;
    }
  else if (strcmp (argv[1], "-d") == 0 || strcmp (argv[1], "--delete") == 0)
    {
      m_delete = 1;
      argc--;
      argv++;
      opt_val = 0;
    }
  else if (strcmp (argv[1], "--initialize") == 0)
    {
      m_init = 1;

      argc--;
      argv++;

      if (argc > 1)
	{
	  print_error (program);
	  return 1;
	}

      /* Load old /etc/security/{pam_unix2,pam_pwcheck}.conf
	 files and delete them afterwards.  */
      load_obsolete_conf (supported_module_list);

      if (load_config (CONF_ACCOUNT, ACCOUNT, supported_module_list) != 0)
	{
	load_old_config_error:
	  fprintf (stderr, _("\nCouldn't load config file, aborted!\n"));
	  return 1;
	}
      if (load_config (CONF_AUTH, AUTH, supported_module_list) != 0)
	goto load_old_config_error;
      if (load_config (CONF_PASSWORD, PASSWORD, supported_module_list) != 0)
	goto load_old_config_error;
      if (load_config (CONF_SESSION, SESSION, supported_module_list) != 0)
	goto load_old_config_error;
    }
  else if (strcmp (argv[1], "--update") == 0)
    {
      m_update = 1;
      argc--;
      argv++;
    }
  else if (strcmp (argv[1], "-q") == 0 || strcmp (argv[1], "--query") == 0)
    {
      m_query = 1;
      argc--;
      argv++;
    }

  if (m_add || m_delete || m_update || m_query)
    {
      if (argc == 1 && !m_update && !m_query)
	{
	  print_error (program);
	  return 1;
	}

      if (load_config (CONF_ACCOUNT_PC, ACCOUNT, supported_module_list) != 0)
	{
	load_config_error:
	  fprintf (stderr, _("\nCouldn't load config file, aborted!\n"));
	  return 1;
	}
      if (load_config (CONF_AUTH_PC, AUTH, supported_module_list) != 0)
	goto load_config_error;
      if (load_config (CONF_PASSWORD_PC, PASSWORD, supported_module_list) != 0)
	goto load_config_error;
      if (load_config (CONF_SESSION_PC, SESSION, supported_module_list) != 0)
	goto load_config_error;
    }

  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] = {
        {"version",   no_argument, NULL, 'v' },
        {"usage",     no_argument, NULL, 'u' },
        {"force",     no_argument, NULL, 'f' },
	{"nullok",    no_argument, NULL, 900 },
	{"pam-debug", no_argument, NULL, 901 },
	/* pam_pwcheck */
	{"pwcheck",                   no_argument,       NULL, 1000 },
	{"pwcheck-debug",             no_argument,       NULL, 1001 },
	{"pwcheck-nullok",            no_argument,       NULL, 1002 },
	{"pwcheck-cracklib",          no_argument,       NULL, 1003 },
	{"pwcheck-cracklib-path",     required_argument, NULL, 1004 },
	{"pwcheck-maxlen",            required_argument, NULL, 1005 },
	{"pwcheck-minlen",            required_argument, NULL, 1006 },
	{"pwcheck-tries",             required_argument, NULL, 1007 },
	{"pwcheck-remember",          required_argument, NULL, 1008 },
	{"pwcheck-nisdir",            required_argument, NULL, 1009 },
	{"pwcheck-no_obscure_checks", no_argument,       NULL, 1010 },
	{"mkhomedir",        no_argument,       NULL, 1100 },
	{"mkhomedir-debug",  no_argument,       NULL, 1101 },
	{"mkhomedir-silent", no_argument,       NULL, 1102 },
	{"mkhomedir-umask",  required_argument, NULL, 1103 },
	{"mkhomedir-skel",   required_argument, NULL, 1104 },
	{"limits",           no_argument,       NULL, 1200 },
        {"env",              no_argument,       NULL, 1300 },
        {"make",             no_argument,       NULL, 1500 },
        {"make-dir",         no_argument,       NULL, 1501 },
        {"unix2",            no_argument,       NULL, 1600 },
        {"unix2-debug",      no_argument,       NULL, 1601 },
        {"unix2-nullok",     no_argument,       NULL, 1602 },
        {"unix2-trace",      no_argument,       NULL, 1603 },
        {"unix2-call_modules", required_argument, NULL, 1604 },
	{"bioapi",           no_argument,       NULL, 1700 },
	{"bioapi-options",        required_argument, NULL, 1701 },
	{"krb5",                  no_argument,       NULL, 1800 },
	{"krb5-debug",            no_argument,       NULL, 1801 },
	{"krb5-minimum_uid",      required_argument, NULL, 1802 },
	{"krb5-ignore_unknown_principals",
	                          no_argument,       NULL, 1803 },
	{"ldap",                  no_argument,       NULL, 1900 },
	{"ldap-debug",            no_argument,       NULL, 1901 },
	{"ccreds",                no_argument,       NULL, 2000 },
	{"pkcs11",                no_argument,       NULL, 2010 },
	{"apparmor",              no_argument,       NULL, 2020 },
	{"lum",                   no_argument,       NULL, 2030 },
        {"cracklib",              no_argument,       NULL, 2100 },
        {"cracklib-debug",        no_argument,       NULL, 2101 },
	{"cracklib-dictpath",     required_argument, NULL, 2102 },
	{"cracklib-retry",        required_argument, NULL, 2103 },
	{"winbind",               no_argument,       NULL, 2200 },
	{"winbind-debug",         no_argument,       NULL, 2201 },
	{"umask",                 no_argument,       NULL, 2300 },
	{"umask-debug",           no_argument,       NULL, 2301 },
	{"umask-silent",          no_argument,       NULL, 2302 },
	{"umask-usergroups",      no_argument,       NULL, 2303 },
	{"umask-umask",           required_argument, NULL, 2304 },
	{"capability",            no_argument,       NULL, 2400 },
        {"capability-debug",      no_argument,       NULL, 2401 },
        {"capability-conf",       required_argument, NULL, 2402 },
	{"debug",                 no_argument,       NULL,  254 },
        {"help",                  no_argument,       NULL,  255 },
        {NULL,                    0,                 NULL,    0 }
      };

      c = getopt_long (argc, argv, "fvu",
                       long_options, &option_index);

      if (c == (-1))
        break;
      switch (c)
	{
	case 'f':
	  force = 1;
	  break;
	case 900: /* --nullok */
	  {
	    pam_module_t **modptr = supported_module_list;

	    while (*modptr != NULL)
	      {
		opt_set = (*modptr)->get_opt_set (*modptr, AUTH);
		opt_set->enable (opt_set, "nullok", opt_val);
		opt_set = (*modptr)->get_opt_set (*modptr, ACCOUNT);
		opt_set->enable (opt_set, "nullok", opt_val);
		opt_set = (*modptr)->get_opt_set (*modptr, PASSWORD);
		opt_set->enable (opt_set, "nullok", opt_val);
		opt_set = (*modptr)->get_opt_set (*modptr, SESSION);
		opt_set->enable (opt_set, "nullok", opt_val);
		++modptr;
	      }
	  }
	  break;
	case 901: /* --pam-debug */
	  {
	    pam_module_t **modptr = supported_module_list;

	    while (*modptr != NULL)
	      {
		opt_set = (*modptr)->get_opt_set (*modptr, AUTH);
		opt_set->enable (opt_set, "debug", opt_val);
		opt_set = (*modptr)->get_opt_set (*modptr, ACCOUNT);
		opt_set->enable (opt_set, "debug", opt_val);
		opt_set = (*modptr)->get_opt_set (*modptr, PASSWORD);
		opt_set->enable (opt_set, "debug", opt_val);
		opt_set = (*modptr)->get_opt_set (*modptr, SESSION);
		opt_set->enable (opt_set, "debug", opt_val);

		++modptr;
	      }
	  }
	  break;
	/* pam_pwcheck */
	case 1000:
	  if (m_query)
	    print_module_pwcheck (&config_password);
	  else
	    {
	      if (check_for_pam_module ("pam_pwcheck.so", force) != 0)
		return 1;
	      config_password.use_pwcheck = opt_val;
	    }
	  break;
	case 1001:
	  config_password.pwcheck_debug = opt_val;
	  break;
	case 1002:
	  config_password.pwcheck_nullok = opt_val;
	  break;
	case 1003:
	  config_password.pwcheck_cracklib = opt_val;
	  break;
	case 1004:
	  config_password.pwcheck_cracklib = opt_val;
	  config_password.pwcheck_cracklib_path = optarg;
	  break;
	case 1005:
	  config_password.pwcheck_maxlen = atoi (optarg);
	  break;
	case 1006:
	  if (m_delete)
	    config_password.pwcheck_have_minlen = 0;
	  else
	    {
	      config_password.pwcheck_minlen = atoi (optarg);
	      config_password.pwcheck_have_minlen = 1;
	    }
	  break;
	case 1007:
	  config_password.pwcheck_tries = atoi (optarg);
	  break;
	case 1008:
	  config_password.pwcheck_remember = atoi (optarg);
	  break;
	case 1009:
	  config_password.pwcheck_nisdir = optarg;
	  break;
	case 1010:
	  config_password.pwcheck_no_obscure_checks = opt_val;
	  break;
	case 1100:
	  if (m_query)
	    print_module_config (supported_module_list,
				 "pam_mkhomedir.so");
	  else
	    {
	      if (check_for_pam_module ("pam_mkhomedir.so", force) != 0)
		return 1;
	      opt_set = mod_pam_mkhomedir.get_opt_set (&mod_pam_mkhomedir,
						       SESSION);
	      opt_set->enable (opt_set, "is_enabled", opt_val);
	    }
	  break;
	case 1101:
	  opt_set = mod_pam_mkhomedir.get_opt_set (&mod_pam_mkhomedir,
						   SESSION);
	  opt_set->enable (opt_set, "debug", opt_val);
	  break;
	case 1102:
	  opt_set = mod_pam_mkhomedir.get_opt_set (&mod_pam_mkhomedir,
						   SESSION);
	  opt_set->enable (opt_set, "silent", opt_val);
	  break;
	case 1103:
	  opt_set = mod_pam_mkhomedir.get_opt_set (&mod_pam_mkhomedir,
						   SESSION);
	  opt_set->set_opt (opt_set, "umask", optarg);
	  break;
	case 1104:
	  opt_set = mod_pam_mkhomedir.get_opt_set (&mod_pam_mkhomedir,
						   SESSION);
	  opt_set->set_opt (opt_set, "skel", optarg);
	  break;
	case 1200:
	  if (m_query)
	    {
	      if (config_session.use_limits)
		printf ("session:\n");
	    }
	  else
	    {
	      if (check_for_pam_module ("pam_limits.so", force) != 0)
		return 1;
	      config_session.use_limits = opt_val;
	    }
	  break;
	case 1300:
	  if (m_query)
	    {
	      if (config_session.use_env || config_auth.use_env)
		printf ("session:\n");
	    }
	  else
	    {
	      if (check_for_pam_module ("pam_env.so", force) != 0)
		return 1;
	      /* Remove in every case from auth,
		 else we will have it twice.  */
	      config_auth.use_env = 0;
	      config_session.use_env = opt_val;
	    }
	  break;
	case 1500:
	  if (m_query)
	    {
	      if (config_session.use_make)
		{
		  printf ("session:");
		  if (config_session.make_options)
		    printf (" %s", config_session.make_options);
		  printf ("\n");
		}
	    }
	  else
	    {
	      if (check_for_pam_module ("pam_make.so", force) != 0)
		return 1;
	      config_session.use_make = opt_val;
	    }
	  break;
	case 1501:
	  config_session.make_options = optarg;
	  break;
	case 1600:
	  /* use_unix2 */
	  if (m_query)
            print_module_config (supported_module_list, "pam_unix2.so");
	  else
	    {
	      if (check_for_pam_module ("pam_unix2.so", force) != 0)
		return 1;
              opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, ACCOUNT);
              opt_set->enable (opt_set, "is_enabled", opt_val);
              opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, AUTH);
              opt_set->enable (opt_set, "is_enabled", opt_val);
              opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, PASSWORD);
              opt_set->enable (opt_set, "is_enabled", opt_val);
              opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, SESSION);
              opt_set->enable (opt_set, "is_enabled", opt_val);
	    }
	  break;
	case 1601:
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, ACCOUNT);
	  opt_set->enable (opt_set, "debug", opt_val);
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, AUTH);
	  opt_set->enable (opt_set, "debug", opt_val);
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, PASSWORD);
	  opt_set->enable (opt_set, "debug", opt_val);
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, SESSION);
	  opt_set->enable (opt_set, "debug", opt_val);
	  break;
	case 1602:
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, ACCOUNT);
	  opt_set->enable (opt_set, "nullok", opt_val);
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, AUTH);
	  opt_set->enable (opt_set, "nullok", opt_val);
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, PASSWORD);
	  opt_set->enable (opt_set, "nullok", opt_val);
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, SESSION);
	  opt_set->enable (opt_set, "nullok", opt_val);
	  break;
        case 1603:
	  opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, SESSION);
	  opt_set->enable (opt_set, "trace", opt_val);
          break;
        case 1604:
	  if (m_delete)
	    {
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, ACCOUNT);
	      opt_set->set_opt (opt_set, "call_modules", NULL);
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, AUTH);
	      opt_set->set_opt (opt_set, "call_modules", NULL);
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, PASSWORD);
	      opt_set->set_opt (opt_set, "call_modules", NULL);
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, SESSION);
	      opt_set->set_opt (opt_set, "call_modules", NULL);
	    }
	  else
	    {
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, ACCOUNT);
	      opt_set->set_opt (opt_set, "call_modules", optarg);
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, AUTH);
	      opt_set->set_opt (opt_set, "call_modules", optarg);
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, PASSWORD);
	      opt_set->set_opt (opt_set, "call_modules", optarg);
	      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, SESSION);
	      opt_set->set_opt (opt_set, "call_modules", optarg);
	    }
          break;
	case 1700:
	  /* pam_bioapi */
	  if (m_query)
	    {
	      if (config_auth.use_bioapi)
		{
		  printf ("auth:");
		  if (config_auth.bioapi_options)
		    printf (" %s", config_auth.bioapi_options);
		  printf ("\n");
		}
	    }
	  else
	    {
	      if (check_for_pam_module ("pam_bioapi.so", force) != 0)
		return 1;
	      config_auth.use_bioapi = opt_val;
	    }
	  break;
	case 1701:
	  config_auth.bioapi_options = optarg;
	  break;
	case 1800:
	  /* pam_krb5 */
	  if (m_query)
	    print_module_krb5 (&config_account, &config_auth,
			       &config_password, &config_session);
	  else
	    {
	      if (check_for_pam_module ("pam_krb5.so", force) != 0)
		return 1;
	      config_account.use_krb5 = opt_val;
	      config_auth.use_krb5 = opt_val;
	      config_password.use_krb5 = opt_val;
	      config_session.use_krb5 = opt_val;
	    }
	  break;
	case 1801:
	  config_account.krb5_debug = opt_val;
	  config_auth.krb5_debug = opt_val;
	  config_password.krb5_debug = opt_val;
	  config_session.krb5_debug = opt_val;
	  break;
	case 1802:
	  if (m_delete)
	    {
	      config_account.krb5_minuid = 0;
	      config_auth.krb5_minuid = 0;
	      config_password.krb5_minuid = 0;
	      config_session.krb5_minuid = 0;
	    }
	  else
	    {
	      config_account.krb5_minuid = atoi (optarg);
	      config_auth.krb5_minuid = atoi (optarg);
	      config_password.krb5_minuid = atoi (optarg);
	      config_session.krb5_minuid = atoi (optarg);
	    }
	  break;
	case 1803:
	  config_account.krb5_ignore_unknown_principals = opt_val;
	  config_auth.krb5_ignore_unknown_principals = opt_val;
	  config_password.krb5_ignore_unknown_principals = opt_val;
	  config_session.krb5_ignore_unknown_principals = opt_val;
	  break;
	case 1900:
	  /* pam_ldap */
	  if (m_query)
            print_module_config (supported_module_list, "pam_ldap.so");
	  else
	    {
	      if (check_for_pam_module ("pam_ldap.so", force) != 0)
		return 1;
              opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, ACCOUNT);
              opt_set->enable (opt_set, "is_enabled", opt_val);
              opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, AUTH);
              opt_set->enable (opt_set, "is_enabled", opt_val);
              opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, PASSWORD);
              opt_set->enable (opt_set, "is_enabled", opt_val);
              opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, SESSION);
              opt_set->enable (opt_set, "is_enabled", opt_val);
	    }
	  break;
	case 1901:
	  opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, ACCOUNT);
	  opt_set->enable (opt_set, "debug", opt_val);
	  opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, AUTH);
	  opt_set->enable (opt_set, "debug", opt_val);
	  opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, PASSWORD);
	  opt_set->enable (opt_set, "debug", opt_val);
	  opt_set = mod_pam_ldap.get_opt_set (&mod_pam_ldap, SESSION);
	  opt_set->enable (opt_set, "debug", opt_val);
	  break;
	case 2000:
	  /* pam_ccreds */
	  if (m_query)
            print_module_config (supported_module_list, "pam_ccreds.so");
	  else
	    {
	      if (check_for_pam_module ("pam_ccreds.so", force) != 0)
		return 1;
	      opt_set = mod_pam_ccreds.get_opt_set (&mod_pam_ccreds, opt_val);
	      opt_set->enable (opt_set, "is_enabled", TRUE);
	    }
	  break;
	case 2010:
	  /* pam_pkcs11 */
	  if (m_query)
	    {
	      if (config_auth.use_pkcs11)
		printf ("auth:\n");
	    }
	  else
	    {

	      if (check_for_pam_module ("pam_pkcs11.so", force) != 0)
		return 1;
	      config_auth.use_pkcs11 = opt_val;
	    }
	  break;
	case 2020:
	  /* pam_apparmor */
	  if (m_query)
	    {
	      if (config_session.use_apparmor)
		printf ("session:\n");
	    }
	  else
	    {

	      if (check_for_pam_module ("pam_apparmor.so", force) != 0)
		return 1;
	      config_auth.use_apparmor = opt_val;
	    }
	  break;
	case 2030:
	  /* pam_nam.so */
	  if (m_query)
	    {
	      if (config_account.use_lum) printf ("account:\n");
	      if (config_auth.use_lum) printf ("auth:\n");
	      if (config_password.use_lum) printf ("password:\n");
	      if (config_session.use_lum) printf ("session:\n");
	    }
	  else
	    {

	      if (check_for_pam_module ("pam_lum.so", force) != 0)
		return 1;
	      config_auth.use_lum = opt_val;
	    }
	  break;
	case 2100:
	  /* pam_cracklib */
	  if (m_query)
	    print_module_cracklib (&config_password);
	  else
	    {
	      if (check_for_pam_module ("pam_cracklib.so", force) != 0)
		return 1;
	      config_password.use_cracklib = opt_val;
	    }
	  break;
	case 2101:
	  config_password.cracklib_debug = opt_val;
	  break;
	case 2102:
	  config_password.cracklib_dictpath = optarg;
	  break;
	case 2103:
	  config_password.cracklib_retry = atoi (optarg);
	  break;
	case 2200:
	  /* pam_winbind */
	  if (m_query)
	    print_module_winbind (&config_account, &config_auth,
				  &config_password, &config_session);
	  else
	    {
	      if (check_for_pam_module ("pam_winbind.so", force) != 0)
		return 1;
	      config_account.use_winbind = opt_val;
	      config_auth.use_winbind = opt_val;
	      config_password.use_winbind = opt_val;
	      config_session.use_winbind = opt_val;
	    }
	  break;
	case 2201:
	  config_account.winbind_debug = opt_val;
	  config_auth.winbind_debug = opt_val;
	  config_password.winbind_debug = opt_val;
	  config_session.winbind_debug = opt_val;
	  break;
	case 2300:
	  /* pam_umask.so */
	  if (m_query)
	    print_module_config (supported_module_list, "pam_umask.so");
	  else
	    {
	      if (check_for_pam_module ("pam_umask.so", force) != 0)
		return 1;
	      opt_set = mod_pam_umask.get_opt_set (&mod_pam_umask, SESSION);
	      opt_set->enable (opt_set, "is_enabled", opt_val);
	    }
	  break;
	case 2301:
	  opt_set = mod_pam_umask.get_opt_set (&mod_pam_umask, SESSION);
	  opt_set->enable (opt_set, "debug", opt_val);
	  break;
	case 2302:
	  opt_set = mod_pam_umask.get_opt_set (&mod_pam_umask, SESSION);
	  opt_set->enable (opt_set, "silent", opt_val);
	  break;
	case 2303:
	  opt_set = mod_pam_umask.get_opt_set (&mod_pam_umask, SESSION);
	  opt_set->enable (opt_set, "usergroups", opt_val);
	  break;
	case 2304:
	  opt_set = mod_pam_umask.get_opt_set (&mod_pam_umask, SESSION);
	  opt_set->set_opt (opt_set, "umask", optarg);
	  break;
	case 2400:
	  /* pam_capability.so */
	  if (m_query)
	    print_module_capability (&config_session);
	  else
	    {
	      if (check_for_pam_module ("pam_capability.so", force) != 0)
		return 1;
	      config_session.use_capability = opt_val;
	    }
	  break;
	case 2401:
	  config_session.capability_debug = opt_val;
	  break;
	case 2402:
	  config_password.capability_conf = optarg;
	  break;
	case 254:
	  debug = 1;
	  break;
	case 255:
          print_help (program);
          return 0;
        case 'v':
          print_version (program, "2007");
          return 0;
        case 'u':
          print_usage (stdout, program);
	  return 0;
	default:
	  print_error (program);
	  return 1;
	}
    }

  argc -= optind;
  argv += optind;

  if (argc > 0)
    {
      fprintf (stderr, _("%s: Too many arguments.\n"), program);
      print_error (program);
      return 1;
    }

  if (m_add + m_create + m_delete + m_init + m_update + m_query != 1)
    {
      print_error (program);
      return 1;
    }


  if (m_query)
    return 0;

  if (m_create)
    {
      /* Write account section.  */
      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, ACCOUNT);
      opt_set->enable (opt_set, "is_enabled", TRUE);
      if (sanitize_check_account (supported_module_list) != 0)
	return 1;
      if (write_config_account (CONF_ACCOUNT_PC, module_list_account) != 0)
	return 1;

      /* Write auth section.  */
      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, AUTH);
      opt_set->enable (opt_set, "is_enabled", TRUE);
      if (sanitize_check_auth (supported_module_list) != 0)
	return 1;
      if (write_config_auth (CONF_AUTH_PC, module_list_auth) != 0)
	return 1;

      /* Write password section.  */
      if (!config_password.use_cracklib)
	{
	  config_password.use_pwcheck = 1;
	  config_password.pwcheck_nullok = 1;
	}
      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, PASSWORD);
      opt_set->enable (opt_set, "is_enabled", TRUE);
      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, PASSWORD);
      opt_set->enable (opt_set, "nullok", TRUE);
      if (sanitize_check_password (supported_module_list) != 0)
	return 1;
      if (write_config_password (CONF_PASSWORD_PC, module_list_password) != 0)
	return 1;

      /* Write session section.  */
      opt_set = mod_pam_unix2.get_opt_set (&mod_pam_unix2, SESSION);
      opt_set->enable (opt_set, "is_enabled", opt_val);
      config_session.use_limits = 1;
      config_session.use_env = 1;
      opt_set = mod_pam_umask.get_opt_set (&mod_pam_umask, SESSION);
      opt_set->enable (opt_set, "is_enabled", opt_val);
      if (sanitize_check_session (supported_module_list) != 0)
	return 1;
      if (write_config_session (CONF_SESSION_PC, module_list_session) != 0)
	return 1;
    }
  else
    {
      /* Write account section.  */
      if (write_config_account (CONF_ACCOUNT_PC, module_list_account) != 0)
	return 1;

      /* Write auth section.  */
      if (sanitize_check_auth (supported_module_list) != 0)
	return 1;
      if (write_config_auth (CONF_AUTH_PC, module_list_auth) != 0)
	return 1;

      /* Write password section.  */
      if (sanitize_check_password (supported_module_list) != 0)
	return 1;
      if (write_config_password (CONF_PASSWORD_PC, module_list_password) != 0)
	return 1;

      /* Write session section.  */
      if (write_config_session (CONF_SESSION_PC, module_list_session) != 0)
	return 1;
    }

  if (m_init || (m_create && force))
    {
      if (relink (CONF_ACCOUNT, CONF_ACCOUNT_PC,
		  CONF_ACCOUNT".pam-config-backup") != 0)
	retval = 1;

      if (relink (CONF_AUTH, CONF_AUTH_PC, CONF_AUTH".pam-config-backup") != 0)
	retval = 1;

      if (relink (CONF_PASSWORD, CONF_PASSWORD_PC,
		  CONF_PASSWORD".pam-config-backup") != 0)
	retval = 1;

      if (relink (CONF_SESSION, CONF_SESSION_PC,
		  CONF_SESSION".pam-config-backup") != 0)
	retval = 1;

      if (m_init && retval == 0)
	{
	  rename ("/etc/security/pam_pwcheck.conf",
		  "/etc/security/pam_pwcheck.conf.pam-config-backup");
	  rename ("/etc/security/pam_unix2.conf",
		  "/etc/security/pam_unix2.conf.pam-config-backup");
	}
      return retval;
    }
  else if (force)
    {
      if (unlink (CONF_ACCOUNT) != 0 ||
	  symlink (CONF_ACCOUNT_PC, CONF_ACCOUNT) != 0)
	{
	  fprintf (stderr,
		   _("Error activating %s (%m)\n"), CONF_ACCOUNT);
	  fprintf (stderr,
		   _("New config from %s is not in use!\n"), CONF_ACCOUNT_PC);
	  retval = 1;
	}

      if (unlink (CONF_AUTH) != 0 ||
	  symlink (CONF_AUTH_PC, CONF_AUTH) != 0)
	{
	  fprintf (stderr,
		   _("Error activating %s (%m)\n"), CONF_AUTH);
	  fprintf (stderr,
		   _("New config from %s is not in use!\n"), CONF_AUTH_PC);
	  retval = 1;
	}

      if (unlink (CONF_PASSWORD) != 0 ||
	  symlink (CONF_PASSWORD_PC, CONF_PASSWORD) != 0)
	{
	  fprintf (stderr,
		   _("Error activating %s (%m)\n"), CONF_PASSWORD);
	  fprintf (stderr,
		   _("New config from %s is not in use!\n"),
		   CONF_PASSWORD_PC);
	  retval = 1;
	}

      if (unlink (CONF_SESSION) != 0 ||
	  symlink (CONF_SESSION_PC, CONF_SESSION) != 0)
	{
	  fprintf (stderr,
		   _("Error activating %s (%m)\n"), CONF_SESSION);
	  fprintf (stderr,
		   _("New config from %s is not in use!\n"),
		   CONF_SESSION_PC);
	  retval = 1;
	}
    }

  if (check_symlink (CONF_ACCOUNT_PC, CONF_ACCOUNT) != 0)
    retval = 1;
  if (check_symlink (CONF_AUTH_PC, CONF_AUTH) != 0)
    retval = 1;
  if (check_symlink (CONF_PASSWORD_PC, CONF_PASSWORD) != 0)
    retval = 1;
  if (check_symlink (CONF_SESSION_PC, CONF_SESSION) != 0)
    retval = 1;

  return retval;
}
