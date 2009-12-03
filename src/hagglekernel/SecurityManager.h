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
#ifndef _SECURITYMANAGER_H
#define _SECURITYMANAGER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/

#include <libcpphaggle/String.h>
#include <libcpphaggle/List.h>
#include <libcpphaggle/HashMap.h>
#include <libcpphaggle/Mutex.h>
#include <libcpphaggle/GenericQueue.h>

using namespace haggle;

#include <openssl/rsa.h>
#include <stdio.h>
#include <stdlib.h>

class SecurityManager;

#include "Manager.h"
#include "Certificate.h"
#include "ManagerModule.h"
#include "DataObject.h"
#include "Event.h"

#define CA_ISSUER_NAME "Haggle CA"

typedef enum {
	SECURITY_LEVEL_LOW = 0, // No security enabled.
	SECURITY_LEVEL_MEDIUM = 1, // Only require valid signatures for node descriptions, but not for other data objects.
	SECURITY_LEVEL_HIGH = 2, // Require valid signatures for all data objects. 
} SecurityLevel_t;

typedef enum {
        SECURITY_TASK_GENERATE_CERTIFICATE,
	SECURITY_TASK_VERIFY_CERTIFICATE,
	SECURITY_TASK_VERIFY_DATAOBJECT,
	SECURITY_TASK_SIGN_DATAOBJECT,
} SecurityTaskType_t;

class SecurityTask {
public:
	SecurityTaskType_t type;
	bool completed;
	DataObjectRef dObj;
        RSA *privKey;
        CertificateRef cert;
	SecurityTask(const SecurityTaskType_t _type, DataObjectRef _dObj = NULL, CertificateRef _cert = NULL);
        ~SecurityTask();
};

class SecurityHelper : public ManagerModule<SecurityManager> {
	friend class SecurityManager;
	GenericQueue<SecurityTask *> taskQ;
	const EventType etype;
	Certificate *issuerCert;
	bool signDataObject(RSA *key, DataObjectRef& dObj);
	bool verifyDataObject(DataObjectRef& dObj, CertificateRef& cert) const;
	void doTask(SecurityTask *task);
	bool run();
        void cleanup();
public:
	SecurityHelper(SecurityManager *m, const EventType _etype);
	~SecurityHelper();

	bool addTask(SecurityTask *task) { return taskQ.insert(task); }
};

class SecurityManager : public Manager {
        friend class SecurityHelper;
private:
	SecurityLevel_t securityLevel;
	EventType etype;
	SecurityHelper *helper;
	EventCallback<EventHandler> *onRepositoryDataCallback;
	typedef HashMap<string,CertificateRef> CertificateStore_t;
	CertificateStore_t certStore;
	Mutex certStoreMutex;
	CertificateRef myCert;
        static const char ca_private_key[];
        static const char ca_public_key[];
        const string ca_issuer;
	RSA *caPrivKey, *caPubKey, *privKey;
	bool storeCertificate(CertificateRef& cert, bool replace = false);
	CertificateRef retrieveCertificate(const string subject);
	void onRepositoryData(Event *e);
	void onSecurityTaskComplete(Event *e);
	void onIncomingDataObject(Event *e);
	void onReceivedDataObject(Event *e);
	void onSendDataObject(Event *e);
	
	void onPrepareStartup();
	void onPrepareShutdown();
	void onShutdown();
public:
#ifdef DEBUG
	void printCertificates();
	void onDebugCmdEvent(Event *e);
#endif
	void setSecurityLevel(const SecurityLevel_t slevel) { securityLevel = slevel; }
	SecurityLevel_t getSecurityLevel() const { return securityLevel; }
	SecurityManager(HaggleKernel *_haggle = haggleKernel, SecurityLevel_t slevel = SECURITY_LEVEL_MEDIUM);
	~SecurityManager();

	class SMException : public ManagerException
	{
	public:
		SMException(const int err = 0, const char* data = "Security manager Error") : ManagerException(err, data) {}
	};
};

#endif
