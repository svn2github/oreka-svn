/*
 * Oreka -- A media capture and retrieval platform
 * 
 * Copyright (C) 2005, orecx LLC
 *
 * http://www.orecx.com
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License.
 * Please refer to http://www.gnu.org/copyleft/gpl.html
 *
 */


#include "stdio.h"

#include "MultiThreadedServer.h"
#include "ace/Thread_Manager.h"
#include "OrkAudio.h"
#include "Utils.h"
#include "messages/TapeMsg.h"
#include "messages/PingMsg.h"
#include "messages/DeleteTapeMsg.h"
#include "messages/CaptureMsg.h"
#include "messages/TestMsg.h"
#include "Config.h"
#include "LogManager.h"
#include "ImmediateProcessing.h"
#include "BatchProcessing.h"
#include "Reporting.h"
#include "ConfigManager.h"
#include "Daemon.h"
#include "ObjectFactory.h"
#include "CapturePluginProxy.h"
#include "ace/OS_NS_arpa_inet.h"


static volatile bool serviceStop = false;

void StopHandler()
{
	serviceStop = true;
}

void MainThread()
{
	LogManagerSingleton::instance()->Initialize();
	LOG4CXX_INFO(LOG.rootLog, CStdString("\n\nOrkAudio service starting\n"));

	// Initialize object factory and register existing objects
	ObjectFactorySingleton::instance()->Initialize();

	ObjectRef objRef;
	objRef.reset(new PingMsg);
	ObjectFactorySingleton::instance()->RegisterObject(objRef);
	objRef.reset(new TapeMsg);
	ObjectFactorySingleton::instance()->RegisterObject(objRef);
	objRef.reset(new SimpleResponseMsg);
	ObjectFactorySingleton::instance()->RegisterObject(objRef);
	objRef.reset(new DeleteTapeMsg);
	ObjectFactorySingleton::instance()->RegisterObject(objRef);
	objRef.reset(new CaptureMsg);
	ObjectFactorySingleton::instance()->RegisterObject(objRef);
	objRef.reset(new TestMsg);
	ObjectFactorySingleton::instance()->RegisterObject(objRef);

	ConfigManagerSingleton::instance()->Initialize();

	if (!ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(ImmediateProcessing::ThreadHandler)))
	{
		LOG4CXX_INFO(LOG.rootLog, CStdString("Failed to create immediate processing thread"));
	}
	if(CONFIG.m_storageAudioFormat != AudioTape::FfNative)
	{
		// storage format is not native, which means we need batch workers to compress to wanted format 
		if (!ACE_Thread_Manager::instance()->spawn_n(CONFIG.m_numBatchThreads, ACE_THR_FUNC(BatchProcessing::ThreadHandler)))
		{
			LOG4CXX_INFO(LOG.rootLog, CStdString("Failed to create batch processing thread"));
		}
	}
	if (!ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(Reporting::ThreadHandler)))
	{
		LOG4CXX_INFO(LOG.rootLog, CStdString("Failed to create reporting thread"));
	}
	// Create command line server on port 10000
	if (!ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(CommandLineServer::run), (void *)10000))
	{
		LOG4CXX_INFO(LOG.rootLog, CStdString("Failed to create command line server"));
	}

	// Create Http server on port 20000
	if (!ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(HttpServer::run), (void *)20000))
	{
		LOG4CXX_INFO(LOG.rootLog, CStdString("Failed to create Http server"));
	}

	if(CapturePluginProxySingleton::instance()->Initialize())
	{
		CapturePluginProxySingleton::instance()->Run();
	}

	//ACE_Thread_Manager::instance ()->wait ();
	while(serviceStop == false)
	{
		ACE_OS::sleep(1);
	}
	LOG4CXX_INFO(LOG.rootLog, CStdString("Stopping service"));
}


int main(int argc, char* argv[])
{
	// figure out service name
	CStdString program(argv[0]);
	CStdString serviceNameWithExtension = BaseName(program);
	CStdString serviceName = StripFileExtension(serviceNameWithExtension);
	if (serviceName.IsEmpty())
	{
		return -1;
	}

	DaemonSingleton::instance()->Initialize(serviceName, MainThread, StopHandler);
	CStdString argument = argv[1];

	if (argc>1)
	{
		if (argument.CompareNoCase("debug") == 0)
		{
			MainThread();
		}
		else if (argument.CompareNoCase("install") == 0)
		{
			DaemonSingleton::instance()->Install();
		}
		else if  (argument.CompareNoCase("uninstall") == 0)
		{
			DaemonSingleton::instance()->Uninstall();
		}
		else
		{
			printf("Argument incorrect. Possibilies are:\n\tdebug\n\tinstall\n\tuninstall\n");
		}
	}
	else
	{
		// No arguments, launch the daemon
		DaemonSingleton::instance()->Start();
	}
	return 0;
}

