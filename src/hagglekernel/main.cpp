/* Copyright 2008-2009 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/Thread.h>
#include <libcpphaggle/Exception.h>
#include <haggleutils.h>

#include "DebugManager.h"
#include "HaggleKernel.h"
#include "SQLDataStore.h"
#include "DataManager.h"
#include "NodeManager.h"
#include "ProtocolManager.h"
#include "ProtocolSocket.h"
#include "Utility.h"
#if defined(OS_UNIX)
#include "ProtocolLOCAL.h"
#endif
#include "ProtocolUDP.h"
#include "ProtocolTCP.h"
#include "ConnectivityManager.h"
#include "ForwardingManager.h"
#include "ApplicationManager.h"
#ifdef BENCHMARK
#include "BenchmarkManager.h"
#endif
#include "ResourceManager.h"
#ifdef DEBUG_LEAKS
#include "XMLMetadata.h"
#endif
#include "SecurityManager.h"

#ifdef OS_WINDOWS_MOBILE
#include <TrayNotifier.h>
HINSTANCE g_hInstance;
#endif

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WINDOWS_DESKTOP)
#define HAGGLE_LOCAL_SOCKET "haggle.sock"
#include <signal.h>
#endif

HaggleKernel *kernel;
static bool shouldCleanupPidFile = true;
static bool setCreateTimeOnBloomfilterUpdate = false;
static bool recreateDataStore = false;
static bool runAsInteractive = true;
static SecurityLevel_t securityLevel = SECURITY_LEVEL_MEDIUM;
/* Command line options variables. */
// Benchmark specific variables
#ifdef BENCHMARK
static bool isBenchmarking = false;
static unsigned int Benchmark_DataObjects_Attr = 10;
static unsigned int Benchmark_Nodes_Attr = 100;
static unsigned int Benchmark_Attr_Num = 1000;
static unsigned int Benchmark_DataObjects_Num = 10000;
static unsigned int Benchmark_Test_Num = 100;
#endif /* BENCHMARK */

#if defined(OS_UNIX)

#include <termios.h>

static struct termios org_opts, new_opts;

static int setrawtty()
{	
	// Set non-buffered operation on stdin so that we notice one character keypresses
	int res;
	
	res = tcgetattr(STDIN_FILENO, &org_opts);
	
	if (res != 0) {
		fprintf(stderr, "Could not get tc opts: %s\n", strerror(errno));
		return res;
	}
	
	memcpy(&new_opts, &org_opts, sizeof(struct termios));
	
	new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
	
	res = tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
        
	return res;
}

static void resettty()
{
	// Open a stdin file descriptor in case someone closed 0
	int fd = open("/dev/stdin", O_RDONLY);
	
	if (!fd) {
		fprintf(stderr, "Could not open stdin: %s\n", strerror(errno));
                return;
	}
	int res = tcsetattr(fd, TCSAFLUSH, &org_opts);
	
	if (res != 0) {
		//fprintf(stderr, "Could not set tc opts: %s\n", strerror(errno));
		// We failed to reset the terminal to its original setting, so
		// we instead pass the terminal the command 'reset' to hard-reset
		// it. Its ugly, but better than a non-usable terminal.
                res = system("reset");

                if (res == -1) {
                        fprintf(stderr, "call to system() failed : %s\n", strerror(errno));
                }
	}
	close(fd);
}

static void daemonize()
{
        int i, fd;

        /* check if already a daemon */
	if (getppid()==1) 
                return; 

	i=fork();

	if (i < 0) 
                exit(1); /* fork error */
	if (i > 0) 
                exit(0); /* parent exits */

	/* new child (daemon) continues here */

	setsid();  /* obtain a new process group */
         
        /* close all open descriptors */
	for (i = getdtablesize();i>=0;--i) 
                close(i);

        /* Redirect stdin, stdout and stderr to /dev/null */
	fd = open("/dev/null", O_RDWR); /* open stdin */
        i = dup(fd); /* stdout */
        i = dup(fd); /* stderr */
}


#endif /* OS_UNIX */

#if defined(OS_WINDOWS)
typedef DWORD pid_t;

static pid_t getpid()
{
        return GetCurrentProcessId();
}
static void daemonize()
{
        
}
#endif

/*
	NOTE: if this path changes for any reason whatsoever, the path used in
	libhaggle to find the pid file also needs to change. Remember to change it!
*/
#define PID_FILE string(DEFAULT_DATASTORE_PATH).append("/haggle.pid")

enum {
        HAGGLE_PROCESS_BAD_PID = -3,
        HAGGLE_PROCESS_CANNOT_WRITE_PID = -2,
        HAGGLE_PROCESS_NO_ERROR = 0,
        HAGGLE_PROCESS_ALREADY_RUNNING = 1
};

static int write_pid_file(const pid_t pid)
{
#define BUFLEN 50
        char buf[BUFLEN];
        string pidfile = PID_FILE;

	if (!create_path(DEFAULT_DATASTORE_PATH)) {
		HAGGLE_ERR("Could not create directory path \'%s\'\n", DEFAULT_DATASTORE_PATH);
		return false;
	}
        FILE *fp = fopen(pidfile.c_str(), "r");

	if (fp) {
		pid_t old_pid;
		bool old_instance_is_running = false;
		HAGGLE_DBG("Detected old PID file, checking if a daemon is already running...\n");
		// The Pid file already exists. Check if the PID in that file is a 
		// running process.
		int ret = fscanf(fp, "%u", &old_pid);
		fclose(fp);
		
                if (ret == 0 || old_pid == 0) {
                        return HAGGLE_PROCESS_BAD_PID;
                } 
               
#if defined(OS_LINUX)
                snprintf(buf, BUFLEN, "/proc/%d/cmdline", old_pid);
                
                fp = fopen(buf, "r");
                
                if (fp != NULL) {
                        size_t nitems = fread(buf, 1, BUFLEN, fp);
                        if (nitems && strstr(buf, "haggle") != NULL) 
                                old_instance_is_running = true;
                        fclose(fp);
                }
#elif defined(OS_UNIX)
		int res = kill(old_pid, 0);
		old_instance_is_running = (res != -1);
#elif defined(OS_WINDOWS)
                HANDLE p;
		
                p = OpenProcess(0, FALSE, old_pid);
                old_instance_is_running = (p != NULL);
                if (p != NULL)
                        CloseHandle(p);
#endif
		if (old_instance_is_running)
			return HAGGLE_PROCESS_ALREADY_RUNNING;
	}

	HAGGLE_DBG("A Haggle daemon is not running, creating new PID file\n");
	
        snprintf(buf, 20, "%u\n", pid);

        // Ok, open and create the file
        fp = fopen(pidfile.c_str(), "w+");
        
        if (!fp) 
                return HAGGLE_PROCESS_CANNOT_WRITE_PID;

        // Write the pid number to the file
        size_t ret = fwrite(buf, strlen(buf), 1, fp);
        
        fclose(fp);

        if (ret != 1)
                return HAGGLE_PROCESS_CANNOT_WRITE_PID;

        return HAGGLE_PROCESS_NO_ERROR;
}

static void cleanup_pid_file()
{
        string pidfile = PID_FILE;
#if defined(OS_WINDOWS_MOBILE) || defined(OS_WINDOWS_VISTA)
	wchar_t *wpidfile = strtowstr(pidfile.c_str());
        DeleteFile(wpidfile);
	free(wpidfile);
#elif defined(OS_WINDOWS)
	DeleteFileA(pidfile.c_str());
#else
	unlink(pidfile.c_str());
#endif
}

#ifdef OS_WINDOWS_MOBILE

TrayNotifier *trayNotifier;

static void tray_notification_add(HINSTANCE hInstance, HaggleKernel *kernel)
{
	trayNotifier = new TrayNotifier(hInstance, kernel);

	trayNotifier->start();
}

static void tray_notification_remove()
{
	if (trayNotifier) {
		delete trayNotifier;
		trayNotifier = NULL;
	}
}

#endif /* WINDOWS_MOBILE */

static void cleanup()
{
	HAGGLE_DBG("Cleaning up\n");

	if (kernel)
		delete kernel;

#ifdef DEBUG_LEAKS
	/*
		There seems to be some artificial limit in printf statements
		on Windows mobile. If the leak report is printed here using
		the HAGGLE_DBG macro nothing will be printed.
		Using the cout stream is slow but seems to work ok.

                However, on some platforms we do not have STL, so we use printf
                here anyway.
	*/
	printf("===== Leak report ===\n");
	LeakMonitor::reportLeaks();
	printf("=====================\n");
#endif
#if defined(OS_UNIX) && !defined(OS_ANDROID)
	resettty();
#endif
#ifdef OS_WINDOWS_MOBILE
	tray_notification_remove();
#endif
        if (shouldCleanupPidFile)
                cleanup_pid_file();
}

#if !defined(OS_WINDOWS_MOBILE)
static int shutdown_counter = 0;

static void signal_handler(int signal)
{
	switch (signal) {
#if defined(OS_UNIX)
	case SIGKILL:
	case SIGHUP:
#endif
	case SIGINT:		// Not supported by OS_WINDOWS?
	case SIGTERM:
		if (shutdown_counter == 0) {
			HAGGLE_DBG("Interrupt: shutting down. Please wait or interrupt twice more to quit hard.\n");
			kernel->shutdown();
		} else if (shutdown_counter == 1) {
			HAGGLE_DBG("Already shutting down. Please wait or interrupt one more time to quit hard.\n");
		} else {
			HAGGLE_DBG("Forced hard shutdown!\n");
			exit(1);
		}
		shutdown_counter++;
		break;
	default:
		break;
	}
}
#endif // !OS_WINDOWS_MOBILE

#if defined(OS_UNIX)

static struct {
	const char *cmd_short;
	const char *cmd_long;
	const char *cmd_desc;
} cmd[] = {
	{ "-b", "--benchmark", "run benchmark." },
	{ "-I", "--non-interactive", "turn off interactive information." },
	{ "-dd", "--delete-datastore", "delete database file before starting." },
	{ "-h", "--help", "print this help." },
	{ "-d", "--daemonize", "run in the background as a daemon." },
	{ "-f", "--filelog", "write debug output to a file (haggle.log)." },
	{ "-c", "--create-time-bloomfilter", "set create time in node description on bloomfilter update." },
	{ "-s", "--security-level", "set security level 0-2 (low, medium, high)" }
};

static void print_help()
{	
	unsigned int i;

	printf("Usage: ./haggle -[hbdfIcs{dd}]\n");
	
	for (i = 0; i < sizeof(cmd) / (3*sizeof(char *)); i++) {
		printf("\t%-4s %-20s %s\n", cmd[i].cmd_short, cmd[i].cmd_long, cmd[i].cmd_desc);
	}
}

static int check_cmd(char *input, unsigned int index)
{
	if (input == NULL || index >  (sizeof(cmd) / (3*sizeof(char *))))
		return 0;

	if (strcmp(input, cmd[index].cmd_short) == 0 || strcmp(input, cmd[index].cmd_long) == 0)
		return 1;

	return 0;	
}

#endif // OS_UNIX

#if HAVE_EXCEPTION
#include <libcpphaggle/Exception.h>
static void unexpected_exception_handler()
{
	HAGGLE_ERR("Uncaught exception in main thread!\n");
	HAGGLE_ERR("Please do exception handling...\n");
	kernel->shutdown();
}
#endif

int run_haggle()
{
#ifdef ENABLE_DEBUG_MANAGER
	DebugManager *db = NULL;
#endif
	ApplicationManager *am = NULL;
	DataManager *dm = NULL;
	NodeManager *nm = NULL;
	ProtocolManager *pm = NULL;
	ForwardingManager *fm = NULL;
	SecurityManager *sm = NULL;
	ConnectivityManager *cm = NULL;
#ifdef BENCHMARK
	BenchmarkManager *bm = NULL;
	//recreateDataStore = true;
#endif
	ResourceManager *rm = NULL;
#ifdef OS_WINDOWS_MOBILE

	// For testing we force the deletion of the data store
	//recreateDataStore = true;
#endif
	int retval = -1;
	  
	if (!create_path(HAGGLE_DEFAULT_STORAGE_PATH)) {
                HAGGLE_ERR("Could not create Haggle storage path : %s\n", HAGGLE_DEFAULT_STORAGE_PATH);
                return -1;
        }
	
        retval = write_pid_file(getpid());

        if (retval != HAGGLE_PROCESS_NO_ERROR) {
                switch (retval) {
                        case HAGGLE_PROCESS_BAD_PID:
                                HAGGLE_ERR("Cannot read PID file %s.\n", PID_FILE.c_str());
                                break;
                        case HAGGLE_PROCESS_CANNOT_WRITE_PID:
                                HAGGLE_ERR("Cannot write PID file %s\n", PID_FILE.c_str());
                                break;
                        case HAGGLE_PROCESS_ALREADY_RUNNING: 
                                HAGGLE_ERR("PID file %s indicates that Haggle is already running.\n", PID_FILE.c_str());
                                break;
                        default:
                                HAGGLE_ERR("Unknown PID file error\n");

                }
                shouldCleanupPidFile = false;
                return -1;
        }
#if defined(OS_UNIX) && !defined(OS_ANDROID)
	setrawtty();
#endif
      
        /* Seed the random number generator */
	prng_init();

        kernel = new HaggleKernel(new SQLDataStore(recreateDataStore));

	if (!kernel) {
		fprintf(stderr, "Haggle startup error!\n");
		return -1;
	}

	// Build a Haggle configuration
#if HAVE_EXCEPTION
	try 
#endif
        {
		ProtocolSocket *p = NULL;

		am = new ApplicationManager(kernel);

		dm = new DataManager(kernel, setCreateTimeOnBloomfilterUpdate);

		nm = new NodeManager(kernel);

		pm = new ProtocolManager(kernel);

		fm = new ForwardingManager(kernel);
		
		sm = new SecurityManager(kernel, securityLevel);
		
#ifdef USE_UNIX_APPLICATION_SOCKET
		p = new ProtocolLOCAL(kernel->getStoragePath() + "/" + HAGGLE_LOCAL_SOCKET, pm);
		p->setFlag(PROT_FLAG_APPLICATION);
		p->registerWithManager();
#endif
		p = new ProtocolUDP("127.0.0.1", HAGGLE_SERVICE_DEFAULT_PORT, pm);
		p->setFlag(PROT_FLAG_APPLICATION);
		p->registerWithManager();
		/* Add ConnectivityManager last since it will start to
		 * discover interfaces and generate events. At that
		 * point the other managers should already be
		 * running. */

		rm = new ResourceManager(kernel);

#ifdef BENCHMARK
		if (!isBenchmarking) {
#endif
			cm = new ConnectivityManager(kernel);
#ifdef BENCHMARK
		} else {
			if (isBenchmarking) {
				bm = new BenchmarkManager(kernel, Benchmark_DataObjects_Attr, Benchmark_Nodes_Attr, Benchmark_Attr_Num, Benchmark_DataObjects_Num, Benchmark_Test_Num);
			}
		}
#endif
#if defined(ENABLE_DEBUG_MANAGER)
		// It seems as if there can be only one accept() per
		// thread... we need to make the DebugManager register
		// protocol or something with the ProtocolTCPServer
		// somehow
		db = new DebugManager(kernel, runAsInteractive);
#endif
	}
#if HAVE_EXCEPTION
	catch(ProtocolSocket::SocketException & e) {
		HAGGLE_ERR("SocketException caught : %s\n", e.getErrorMsg());
		goto fail_exception;
	}
	catch(ProtocolSocket::BindException & e) {
		HAGGLE_ERR("BindException caught : %s\n", e.getErrorMsg());
		goto fail_exception;
	}
	catch(Manager::ManagerException & e) {
		HAGGLE_ERR("Manager Exception caught : %s\n", e.getErrorMsg());
		goto fail_exception;
	}
	catch(Exception & e) {
		HAGGLE_ERR("Basic Exception : %s\n", e.getErrorMsg());
		goto fail_exception;
	}
#endif

#if HAVE_EXCEPTION
	try 
#endif
        {
		HAGGLE_DBG("Starting Haggle...\n");
		
#ifdef OS_WINDOWS_MOBILE
		if (platform_type(current_platform()) == platform_windows_mobile_professional)
			tray_notification_add(g_hInstance, kernel);
#endif

		kernel->run();
		retval = EXIT_SUCCESS;

		HAGGLE_DBG("Haggle finished...\n");
	}
#if HAVE_EXCEPTION
	catch(Exception & e) {
		HAGGLE_ERR("Exception caught : %s\n", e.getErrorMsg());
	}
	catch(...) {
		HAGGLE_ERR("Unknown exception...\n");
	}
      fail_exception:
#endif
#ifdef BENCHMARK
	if (!isBenchmarking) {
#endif
		delete cm;
#ifdef BENCHMARK
	} else {
		if (isBenchmarking) {
			delete bm;
		}
	}
#endif
	delete sm;
	delete fm;
	delete pm;
	delete nm;
	delete dm;
	delete am;
#if defined(ENABLE_DEBUG_MANAGER)
	delete db;
#endif
	delete rm;
#ifdef OS_WINDOWS_MOBILE
	tray_notification_remove();
#endif
	delete kernel;
	kernel = NULL;

	return retval;
}

#if defined(OS_WINDOWS)
static void set_path(void)
{
	TCHAR	tstr[512];
	char	str[512];
	DWORD	size;
	DWORD	i;
	
	size = GetModuleFileName(NULL, tstr, 512);
	if(size != 512)
	{
		for(i = 0; i <= size; i++)
			str[i] = (char) tstr[i];
		
		fill_in_haggle_path(str);
	}else{
		fill_in_haggle_path(".\\");
	}
}
#endif

#if defined(WINCE)
int WINAPI WinMain(
    HINSTANCE hInstance, 
    HINSTANCE hPrevInstance, 
    LPTSTR lpCmdLine,    
    int nCmdShow)
{
	g_hInstance = hInstance;
	// Turn the following line on if you want to debug on windows mobile
#if defined(BENCHMARK)
	isBenchmarking = true;
#endif
	atexit(cleanup);
	set_path();
	
	return run_haggle();
}
#else
#if defined(OS_UNIX)
int main(int argc, char **argv)
#elif defined(OS_WINDOWS)
int main(void)
#endif
{
#if !defined(OS_WINDOWS_MOBILE)
	shutdown_counter = 0;
#endif
#if defined(OS_UNIX)
	struct sigaction sigact;

	memset(&sigact, 0, sizeof(struct sigaction));
#endif
	
	// Set the handler function for unexpected exceptions
#if HAVE_EXCEPTION
        std::set_terminate(unexpected_exception_handler);
#endif

#if defined(BENCHMARK)
	isBenchmarking = true;
#endif
#if defined(OS_WINDOWS)
	set_path();
#endif
#if defined(OS_UNIX)
	fill_in_haggle_path(argv[0]);
	
	// Do some command line parsing. Could use "getopt", but it
	// probably doesn't port well to Windows
	argv++;
	argc--;

	while (argc) {
		if (check_cmd(argv[0], 3)) {
			print_help();
			return 0;
		} else if (check_cmd(argv[0], 1)) {
			runAsInteractive = false;
		} else if (check_cmd(argv[0], 0)) {
#ifdef BENCHMARK
			if (!(argv[1] && argv[2] && argv[3] && argv[4]
			      && argv[5])) {
				cerr << "Bad number of arguments for benchmark option..." << endl;
				cerr << "usage: -b doAttr nodeAttr numAttr numDataObject numNode" << endl;
				return -1;
			}
			printf("Haggle benchmarking...\n");
			isBenchmarking = true;
			Benchmark_DataObjects_Attr = atoi(argv[1]);
			Benchmark_Nodes_Attr = atoi(argv[2]);
			Benchmark_Attr_Num = atoi(argv[3]);
			Benchmark_DataObjects_Num = atoi(argv[4]);
			Benchmark_Test_Num = atoi(argv[5]);
#else
			fprintf(stderr, "-b: Unsupported: no benchmarking compiled in!\n");
			return -1;
#endif
			break;
		} else if (check_cmd(argv[0], 2)) {
			recreateDataStore = true;
		} else if (check_cmd(argv[0], 4)) {
			runAsInteractive = false;
			daemonize();
		} else if (check_cmd(argv[0], 5)) {
                        Trace::trace.enableFileTrace();
		} else if (check_cmd(argv[0], 6)) {
                        setCreateTimeOnBloomfilterUpdate = true;
		} else if (check_cmd(argv[0], 7)) {
			if (!argv[1] || atoi(argv[1]) < 0 || atoi(argv[1]) > 2) {
				fprintf(stderr, "Bad security level, must be between 0-2\n");
				return -1;
			}
                        securityLevel = static_cast<SecurityLevel_t>(atoi(argv[1]));
			argv++;
			argc--;
		} else {
			fprintf(stderr, "Unknown command line option: %s\n", argv[0]);
			print_help();
			exit(-1);
		}
		argv++;
		argc--;
	}
#endif

#if defined(OS_WINDOWS)
	signal(SIGTERM, &signal_handler);
	signal(SIGINT, &signal_handler);
#elif defined(OS_UNIX)
	sigact.sa_handler = &signal_handler;
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
#endif

	atexit(cleanup);

	return run_haggle();
}

#endif
