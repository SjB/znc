/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include <getopt.h>

static struct option g_LongOpts[] = {
	{ "help",			no_argument,	0,	'h' },
	{ "version",			no_argument,	0,	'v' },
	{ "no-color",			no_argument,	0,	'n' },
	{ "allow-root",			no_argument,	0,	'r' },
	{ "makeconf",			no_argument,	0,	'c' },
	{ "makepass",			no_argument,	0,	's' },
#ifdef HAVE_LIBSSL
	{ "makepem",			no_argument,	0,	'p' },
	{ "encrypt-pem",		no_argument,	0, 	'e' },
#endif /* HAVE_LIBSSL */
	{ "datadir",                    required_argument,	0,   'd' },
	{ 0, 0, 0, 0 }
};

static void GenerateHelp(const char *appname) {
	CUtils::PrintMessage("USAGE: " + CString(appname) + " [options] [config]");
	CUtils::PrintMessage("Options are:");
	CUtils::PrintMessage("\t-h, --help         List available command line options (this page)");
	CUtils::PrintMessage("\t-v, --version      Output version information and exit");
	CUtils::PrintMessage("\t-n, --no-color     Don't use escape sequences in the output");
	CUtils::PrintMessage("\t-r, --allow-root   Don't complain if ZNC is run as root");
	CUtils::PrintMessage("\t-c, --makeconf     Interactively create a new config");
	CUtils::PrintMessage("\t-s, --makepass     Generates a password for use in config");
#ifdef HAVE_LIBSSL
	CUtils::PrintMessage("\t-p, --makepem      Generates a pemfile for use with SSL");
	CUtils::PrintMessage("\t-e, --encrypt-pem  when used along with --makepem, encrypts the private key in the pemfile");
#endif /* HAVE_LIBSSL */
	CUtils::PrintMessage("\t-d, --datadir      Set a different znc repository (default is ~/.znc)");
}

static void die(int sig) {
	signal(SIGPIPE, SIG_DFL);

#ifdef _DEBUG
	CUtils::PrintMessage("Exiting on SIG [" + CString(sig) + "]");
	if ((sig == SIGABRT) || (sig == SIGSEGV)) {
		abort();
	}
#endif /* _DEBUG */

	delete &CZNC::Get();
	exit(sig);
}

static void rehash(int sig) {
	CZNC::Get().SetNeedRehash(true);
}

static bool isRoot() {
	uid_t u_real, u_effective, u_saved;

	getresuid(&u_real, &u_effective, &u_saved);

	// User root? If one of these were root, we could switch the others to root, too
	if (u_real == 0 || u_effective == 0 || u_saved == 0)
		return true;

	return false;
}

int main(int argc, char** argv) {
	CString sConfig;
	CString sDataDir = "";

	srand(time(NULL));
	CUtils::SetStdoutIsTTY(isatty(1));

#ifdef HAVE_LIBSSL
	InitSSL();
#endif /* HAVE_LIBSSL */

	int iArg, iOptIndex = -1;
	bool bMakeConf = false;
	bool bMakePass = false;
#ifdef HAVE_LIBSSL
	bool bMakePem = false;
	bool bEncPem = false;
	bool bAllowRoot = false;

	while ((iArg = getopt_long(argc, argv, "hvnrcsped:", g_LongOpts, &iOptIndex)) != -1) {
#else
	while ((iArg = getopt_long(argc, argv, "hvnrcsd:", g_LongOpts, &iOptIndex)) != -1) {
#endif /* HAVE_LIBSSL */
	    switch (iArg) {
		case 'h':
			    GenerateHelp(argv[0]);
			    return 0;
		case 'v':
			    cout << CZNC::GetTag() << endl;
			    return 0;
		case 'n':
			    CUtils::SetStdoutIsTTY(false);
			    break;
		case 'r':
			    bAllowRoot = true;
			    break;
		case 'c':
			    bMakeConf = true;
			    break;
		case 's':
			    bMakePass = true;
			    break;
#ifdef HAVE_LIBSSL
		case 'p':
			    bMakePem = true;
			    break;
		case 'e':
			    bEncPem = true;
			    break;
#endif /* HAVE_LIBSSL */
		case 'd':
			    sDataDir = CString(optarg);
			    break;
		case '?':
		default:
			    GenerateHelp(argv[0]);
			    return 1;
	    }
	}

	if (optind < argc) {
		sConfig = argv[optind];
	} else {
		sConfig = "znc.conf";
	}

	if (bMakeConf) {
		CZNC& ZNC = CZNC::Get();
		ZNC.InitDirs("", sDataDir);
		if (ZNC.WriteNewConfig(sConfig)) {
			char const* args[5];

			if (argc > 2) {
				args[0] = argv[0];
				if (!sDataDir.empty()) {
					args[1] = "--datadir";
					args[2] = strdup(sDataDir.c_str());
					args[3] = argv[optind];
					args[4] = NULL;
				} else {
					args[1] = argv[optind];
					args[2] = NULL;
				}
			} else if (argc > 1) {
				args[0] = argv[0];
				if (!sDataDir.empty()) {
					args[1] = "--datadir";
					args[2] = strdup(sDataDir.c_str());
					args[3] = NULL;
				} else {
					args[1] = NULL;
				}
			} else {
				CUtils::PrintError("Unable to launch znc [Try manually restarting]");
				return 1;
			}

			if ((chdir(ZNC.GetCurPath().c_str()) == -1)
					|| (execv(*argv, (char *const*)args) == -1)) {
				CUtils::PrintError("Unable to launch znc [" + CString(strerror(errno)) + "]");
				return 1;
			}
		}

		return 0;
	}

#ifdef HAVE_LIBSSL
	if (bMakePem) {
		CZNC* pZNC = &CZNC::Get();
		pZNC->InitDirs("", sDataDir);
		pZNC->WritePemFile(bEncPem);

		delete pZNC;
		return 0;
	}
	if (bEncPem && !bMakePem) {
		CUtils::PrintError("--encrypt-pem should be used along with --makepem.");
		return 1;
	}

#endif /* HAVE_LIBSSL */
	if (bMakePass) {
		CString sHash = CUtils::GetHashPass();
		CUtils::PrintMessage("Use this in the <User> section of your config:");
		CUtils::PrintMessage("Pass = " + sHash + " -");

		return 0;
	}

	CZNC* pZNC = &CZNC::Get();
	pZNC->InitDirs(((argc) ? argv[0] : ""), sDataDir);

	if (!pZNC->ParseConfig(sConfig)) {
		CUtils::PrintError("Unrecoverable config error.");
		delete pZNC;
		return 1;
	}

	if (!pZNC->OnBoot()) {
		CUtils::PrintError("Exiting due to module boot errors.");
		delete pZNC;
		return 1;
	}

	if (isRoot()) {
		CUtils::PrintError("You are running ZNC as root! Don't do that! There are not many valid");
		CUtils::PrintError("reasons for this and it can, in theory, cause great damage!");
		if (!bAllowRoot) {
			exit(1);
		}
		CUtils::PrintError("You have been warned.");
		CUtils::PrintError("Hit CTRL+C now if you don't want to run ZNC as root.");
		CUtils::PrintError("ZNC will start in 30 seconds.");
		sleep(30);
	}

#ifdef _DEBUG
	int iPid = getpid();
	CUtils::PrintMessage("Staying open for debugging [pid: " + CString(iPid) + "]");

	pZNC->WritePidFile(iPid);
	CUtils::PrintMessage(CZNC::GetTag());
#else
	CUtils::PrintAction("Forking into the background");

	int iPid = fork();

	if (iPid == -1) {
		CUtils::PrintStatus(false, strerror(errno));
		delete pZNC;
		exit(1);
	}

	if (iPid > 0) {
		// We are the parent. We are done and will go to bed.
		CUtils::PrintStatus(true, "[pid: " + CString(iPid) + "]");

		pZNC->WritePidFile(iPid);
		CUtils::PrintMessage(CZNC::GetTag());
		exit(0);
	}

	// Redirect std in/out/err to /dev/null
	close(0); open("/dev/null", O_RDONLY);
	close(1); open("/dev/null", O_WRONLY);
	close(2); open("/dev/null", O_WRONLY);

	CUtils::SetStdoutIsTTY(false);

	// We are the child. There is no way we can be a process group
	// leader, thus setsid() must succeed.
	setsid();
	// Now we are in our own process group and session (no controlling
	// terminal). We are independent!
#endif

	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, (struct sigaction*) NULL);

	sa.sa_handler = rehash;
	sigaction(SIGHUP,  &sa, (struct sigaction*) NULL);

	// Once this signal is caught, the signal handler is reset
	// to SIG_DFL. This avoids endless loop with signals.
	sa.sa_flags = SA_RESETHAND;
	sa.sa_handler = die;
	sigaction(SIGINT,  &sa, (struct sigaction*) NULL);
	sigaction(SIGILL,  &sa, (struct sigaction*) NULL);
	sigaction(SIGQUIT, &sa, (struct sigaction*) NULL);
	sigaction(SIGBUS,  &sa, (struct sigaction*) NULL);
	sigaction(SIGSEGV, &sa, (struct sigaction*) NULL);
	sigaction(SIGTERM, &sa, (struct sigaction*) NULL);

	int iRet = 0;

	try {
		iRet = pZNC->Loop();
	} catch (CException e) {
		// EX_Shutdown is thrown to exit
		switch (e.GetType()) {
			case CException::EX_Shutdown:
				iRet = 0;
			default:
				iRet = 1;
		}
	}

	delete pZNC;

	return iRet;
}
