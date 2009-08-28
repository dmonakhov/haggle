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

#include "SecurityManager.h"
#include "Certificate.h"
#include "Utility.h"

#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

/* 
 Private and public key of certificate authority in PEM format.
 
 Naturally, these keys are only here for demonstration purposes.
 
 The keys are used to generate Certificates and a public/private key pairs for
 every Haggle node, which they then use to sign data objects that they originate.
 The certificates of each node are signed using the private key of the CA, such
 that each node can verify the validity of a certificate received from another
 node.

 */
const char SecurityManager::ca_public_key[] =
"-----BEGIN PUBLIC KEY-----\n\
MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAPR0eI6KaW618dwjFQtNkm9YwUeDOVqb\n\
Nbh3V7EkdVrB5g4SxY82budnqC3nsN3N9CFlPTw/NwR6oWOqcqqYpGUCAwEAAQ==\n\
-----END PUBLIC KEY-----\n";

const char SecurityManager::ca_private_key[] = 
"-----BEGIN RSA PRIVATE KEY-----\n\
MIIBOgIBAAJBAPR0eI6KaW618dwjFQtNkm9YwUeDOVqbNbh3V7EkdVrB5g4SxY82\n\
budnqC3nsN3N9CFlPTw/NwR6oWOqcqqYpGUCAwEAAQJBAO4j2J3jsLotfSQa+RE9\n\
zH20VPW5nFHsCfVeLYtgHQL/Ig3Ff1GkYuGHBXElFaoMbjml2PRieniSIKxF9RD+\n\
4wECIQD9z32obXTpJDzGnWzJQnBRrwE/PXUYQrVqYmzHueXYcQIhAPaQUcsYaAWy\n\
1QMNp7TpKbsfZ24dmlgh8DoK6v8yoqU1AiAGWfLjDBoo22dJ8RaP0sHMyXxWgMs1\n\
WDYB+4SNWvGNgQIgE7LsFgHZLbtf8WKB555JSz3zEYUj866idsCwjbsJ65ECIBK+\n\
S/IbEYjSBXB4R/Xh7A12WJ0xAi8IEAn9rG/hTnTG\n\
-----END RSA PRIVATE KEY-----\n";


typedef enum {
	KEY_TYPE_PRIVATE,
	KEY_TYPE_PUBLIC,
} KeyType_t;

static RSA *stringToRSAKey(const char *keyStr, KeyType_t type = KEY_TYPE_PUBLIC)
{
	RSA *key = NULL;

	//HAGGLE_DBG("trying to convert:\n%s\n", keyStr);

	BIO *bp = BIO_new_mem_buf(const_cast<char *>(keyStr), -1);
        
        if (!bp) {
		HAGGLE_ERR("Could not allocate BIO\n");
		return NULL;
	}

	if (type == KEY_TYPE_PUBLIC) {
		if (!PEM_read_bio_RSA_PUBKEY(bp, &key, NULL, NULL)) {
			HAGGLE_ERR("Could not read public key from PEM string\n");
		}
	} else if (type == KEY_TYPE_PRIVATE) {
		if (!PEM_read_bio_RSAPrivateKey(bp, &key, NULL, NULL)) {
			HAGGLE_ERR("Could not read private key from PEM string\n");
		}
	}

	BIO_free(bp);

	return key;
}

static const char *RSAPrivKeyToString(RSA *key)
{
	static char buffer[2000];
	
	BIO *bp = BIO_new(BIO_s_mem());

	if (!bp)
		return NULL;
	

	if (!PEM_write_bio_RSAPrivateKey(bp, key, NULL, NULL, 0, NULL, NULL)) {
		BIO_free(bp);
		return NULL;
	}

	int len = BIO_read(bp, buffer, sizeof(buffer));
	
	BIO_free(bp);

	if (len <= 0)
		return NULL;

	buffer[len] = '\0';

	//printf("Key string:\n%s\n", buffer);

	return buffer;
}

#if 0
static bool generateKeyPair(int num, unsigned long e, RSA **keyPair)
{
	if (!keyPair)
		return false;

	*keyPair = RSA_generate_key(num, e, NULL, NULL);

	if (*keyPair == NULL)
		return false;
	else 
		return true;
}
#endif

SecurityTask::SecurityTask(const SecurityTaskType_t _type, DataObjectRef _dObj, CertificateRef _cert) : 
                        type(_type), completed(false), dObj(_dObj), 
                        privKey(NULL), cert(_cert) 
{
}

 SecurityTask::~SecurityTask()
 {
 }

SecurityHelper::SecurityHelper(SecurityManager *m, const EventType _etype) : 
	ManagerModule<SecurityManager>(m, "SecurityHelper"), taskQ("SecurityHelper"), etype(_etype)
{
}

SecurityHelper::~SecurityHelper()
{
	while (!taskQ.empty()) {
		SecurityTask *task = NULL;
		taskQ.retrieve(&task);
		delete task;
	}
}

bool SecurityHelper::signDataObject(RSA *key, DataObjectRef& dObj)
{
	unsigned char *signature;
	
	if (!key || !dObj) 
		return false;
	
	unsigned int siglen = RSA_size(key);
	
	signature = (unsigned char *)malloc(siglen);
	
	if (!signature)
		return false;
	
	if (RSA_sign(NID_sha1, dObj->getId(), sizeof(DataObjectId_t), signature, &siglen, key) != 1) {
		free(signature);
		return false;
	}
	
	dObj->setSignature(getManager()->getKernel()->getThisNode()->getIdStr(), signature, siglen);
	
	// Do not free the allocated signature as it is now owned by the data object...
	
	return true;
}

bool SecurityHelper::verifyDataObject(DataObjectRef& dObj, CertificateRef& cert) const
{
	RSA *key = cert->getPubKey();
	
	// Cannot verify without signature
	if (!dObj->getSignature()) {
		HAGGLE_ERR("No signature in data object, cannot verify\n");
		return false;
	}	
	
	if (RSA_verify(NID_sha1, dObj->getId(), sizeof(DataObjectId_t), 
		       const_cast<unsigned char *>(dObj->getSignature()), dObj->getSignatureLength(), key) != 1) {
		char buf[10000];
		dObj->getRawMetadata(buf, sizeof(buf));
		HAGGLE_DBG("Signature is invalid:\n%s\n", buf);
		dObj->setSignatureStatus(DATAOBJECT_SIGNATURE_INVALID);
		return false;
	}
	
	HAGGLE_DBG("Signature is valid\n");
	dObj->setSignatureStatus(DATAOBJECT_SIGNATURE_VALID);
	
	return true;
}

void SecurityHelper::doTask(SecurityTask *task)
{
	Metadata *m = NULL;
	
        switch (task->type) {
                case SECURITY_TASK_GENERATE_CERTIFICATE:
                        HAGGLE_DBG("Creating certificate\n");
                        task->cert = Certificate::create(getManager()->getKernel()->getThisNode()->getIdStr(), getManager()->ca_issuer, "forever", getManager()->getKernel()->getThisNode()->getId(), &task->privKey);
                        
			if (task->cert) {
				if (!task->cert->sign(getManager()->caPrivKey)) {
					HAGGLE_ERR("Signing of certificate failed\n");
				}
			
				getManager()->storeCertificate(task->cert);		
			}
                        break;
                case SECURITY_TASK_VERIFY_CERTIFICATE:
                        HAGGLE_DBG("Verifying embedded certificate\n");
			
			m = task->dObj->getMetadata()->getMetadata("Security");
			
			if (m) {
				Metadata *cm = m->getMetadata("Certificate");
				
				if (cm) {
					task->cert = Certificate::fromMetadata(*cm);
					
					if (task->cert) {
						task->cert->verifySignature(getManager()->caPubKey);
						
						if (task->cert->isVerified()) {
							HAGGLE_DBG("Certificate is valid, adding to store\n");
							getManager()->storeCertificate(task->cert, true);
						}
					}
				}
			}
                        break;
                case SECURITY_TASK_VERIFY_DATAOBJECT:
			// Check for a certificate:
			task->cert = getManager()->retrieveCertificate(task->dObj->getSignee());
			
			if (task->cert) {
				HAGGLE_DBG("Verifying data object\n");
				verifyDataObject(task->dObj, task->cert);
			} else {
				HAGGLE_DBG("Could not verify data object due to lack of certificate\n");
			}
			break;
                case SECURITY_TASK_SIGN_DATAOBJECT:
                        // to be defined
                        break;
        }
	// Return result if the private event is valid
	if (Event::isPrivate(etype))
		addEvent(new Event(etype, task));
	else
		delete task;

	return;
}

bool SecurityHelper::run()
{	
	HAGGLE_DBG("SecurityHelper running...\n");
	
	while (!shouldExit()) {
		QueueEvent_t qe;
		SecurityTask *task = NULL;
		
		qe = taskQ.retrieve(&task);
		
		switch (qe) {
		case QUEUE_ELEMENT:
			doTask(task);

			// Delete task here or return it with result in private event?
			//delete task;
			break;
		case QUEUE_WATCH_ABANDONED:
			HAGGLE_DBG("SecurityHelper instructed to exit...\n");
			return false;
		default:
			HAGGLE_ERR("Unknown security task queue return value\n");
		}
	}
	return false;
}

void SecurityHelper::cleanup()
{
	while (!taskQ.empty()) {
		SecurityTask *task = NULL;
		taskQ.retrieve(&task);
		delete task;
	}
}

const char *security_level_names[] = { "LOW", "MEDIUM", "HIGH" };

SecurityManager::SecurityManager(HaggleKernel *_haggle, const SecurityLevel_t slevel) :
	Manager("Security Manager", _haggle), securityLevel(slevel), etype(EVENT_TYPE_INVALID), helper(NULL), 
	myCert(NULL), ca_issuer(CA_ISSUER_NAME), caPrivKey(NULL), caPubKey(NULL), privKey(NULL)
{
#define __CLASS__ SecurityManager
	int ret;
	
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_RECEIVED, onReceivedDataObject);

#if HAVE_EXCEPTION
	if (ret < 0)
		throw SMException(ret, "Could not register event");
#endif
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND, onSendDataObject);
	
#if HAVE_EXCEPTION
	if (ret < 0)
		throw SMException(ret, "Could not register event");
#endif
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_INCOMING, onIncomingDataObject);
	
#if HAVE_EXCEPTION
	if (ret < 0)
		throw SMException(ret, "Could not register event");
#endif
	
#ifdef DEBUG
	setEventHandler(EVENT_TYPE_DEBUG_CMD, onDebugCmdEvent);
#if HAVE_EXCEPTION
	if (ret < 0)
		throw SMException(ret, "Could not register event");
#endif
#endif
	
	onRepositoryDataCallback = newEventCallback(onRepositoryData);
	
        // -- retrieve CA key from memory
	caPrivKey = stringToRSAKey(ca_private_key, KEY_TYPE_PRIVATE);
	
	if (!caPrivKey) {
		HAGGLE_ERR("Could not read CA's private key from memory\n");
		return;
	}
	
	caPubKey = stringToRSAKey(ca_public_key, KEY_TYPE_PUBLIC);

	if (!caPubKey) {
		HAGGLE_ERR("Could not read CA's public key from memory\n");
		return;
	}

	HAGGLE_DBG("Successfully read CA's public key\n");

	EventType etype = registerEventType("SecurityTaskEvent", onSecurityTaskComplete);

	HAGGLE_DBG("Security level is set to %s\n", security_level_names[securityLevel]);
	
	helper = new SecurityHelper(this, etype);

	if (helper) {
		HAGGLE_DBG("Starting security helper...\n");
		helper->start();
	}
}

SecurityManager::~SecurityManager()
{
	if (helper)
		delete helper;

	Event::unregisterType(etype);

        if (caPubKey)
                RSA_free(caPubKey);

        if (caPrivKey)
                RSA_free(caPrivKey);
	
	if (privKey)
		RSA_free(privKey);
	
	if (onRepositoryDataCallback)
		delete onRepositoryDataCallback;
}

void SecurityManager::onPrepareStartup()
{
	kernel->getDataStore()->readRepository(new RepositoryEntry(getName()), onRepositoryDataCallback);
}

void SecurityManager::onPrepareShutdown()
{
	Mutex::AutoLocker l(certStoreMutex);
	
	// Save our private key
	kernel->getDataStore()->insertRepository(new RepositoryEntry(getName(), "privkey", RSAPrivKeyToString(privKey)));
			
	// Save all certificates
	for (CertificateStore_t::iterator it = certStore.begin(); it != certStore.end(); it++) {
		kernel->getDataStore()->insertRepository(new RepositoryEntry(getName(), (*it).second->getSubject().c_str(), (*it).second->toPEM()));
	}
	
	signalIsReadyForShutdown();
}

void SecurityManager::onShutdown()
{
	// TODO: store our certificates in a data store repository and then retrieve them on startup again. 
	if (helper) {
		HAGGLE_DBG("Stopping security helper...\n");
		helper->stop();
	}
	unregisterWithKernel();
}


void SecurityManager::onRepositoryData(Event *e)
{
	RepositoryEntryRef re;
	
	if (!e->getData()) {
		signalIsReadyForStartup();
		return;
	}
	
	HAGGLE_DBG("Got repository callback\n");
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());
	
	if (qr->countRepositoryEntries() == 0) {
		HAGGLE_DBG("No repository entries, generating new certificate and keypair\n");
		helper->addTask(new SecurityTask(SECURITY_TASK_GENERATE_CERTIFICATE));
		
		// Delay signalling that we are ready for startup until we get the 
		// task result indicating our certificate is ready.
		delete qr;
		return;
	}
	
	while ((re = qr->detachFirstRepositoryEntry())) {
		if (strcmp(re->getKey(), "privkey") == 0) {
			
			// Just to make sure
			if (privKey)
				RSA_free(privKey);
			
			privKey = stringToRSAKey(re->getValue(), KEY_TYPE_PRIVATE);
			
			HAGGLE_DBG("Read my own private key from repository\n");
		} else {
			CertificateRef c = Certificate::fromPEM(re->getValue());
			
			if (c) {
				if (c->getSubject() == kernel->getThisNode()->getIdStr())
					myCert = c;
				
				storeCertificate(c);
				HAGGLE_DBG("Read certificate for subject '%s' from repository\n", c->getSubject().c_str());
			} else {
				HAGGLE_ERR("Could not read certificate from repository\n");
			}
		}
	}
	
	delete qr;
	
	signalIsReadyForStartup();
}

bool SecurityManager::storeCertificate(CertificateRef& cert, bool replace)
{
	Mutex::AutoLocker l(certStoreMutex);
	
	if (cert->isStored())
		return false;
	
	cert->setStored();
	
	CertificateStore_t::iterator it = certStore.find(cert->getSubject());
	
	if (it != certStore.end()) {
		if (replace) {
			certStore.erase(it);
		} else {
			return false;
		}
	}
	
	certStore.insert(make_pair(cert->getSubject(), cert));

	return true;
}

CertificateRef SecurityManager::retrieveCertificate(const string subject)
{
	Mutex::AutoLocker l(certStoreMutex);
	
	CertificateStore_t::iterator it = certStore.find(subject);
	
	if (it == certStore.end())
		return NULL;
	
	return (*it).second;
}

#ifdef DEBUG

void SecurityManager::onDebugCmdEvent(Event *e)
{
	if (e->getDebugCmd()->getType() != DBG_CMD_PRINT_CERTIFICATES)
		return;
	
	printCertificates();
}

void SecurityManager::printCertificates()
{
	Mutex::AutoLocker l(certStoreMutex);
	int n = 0;
	
	printf("[Certificate Store]:\n");
	
	for (CertificateStore_t::iterator it = certStore.begin(); it != certStore.end(); it++) {
		CertificateRef& cert = (*it).second;
		printf("# %d subject=%s issuer=%s\n", n++, cert->getSubject().c_str(), cert->getIssuer().c_str());
		printf("%s\n", cert->toString().c_str());
	}
	
}
#endif
	
/*
 This function is called after the SecurityHelper thread finished a task.
 The Security manager may act on any of the results if it wishes.
 
 */
void SecurityManager::onSecurityTaskComplete(Event *e)
{
	if (!e || !e->hasData())
		return;
        
        SecurityTask *task = static_cast<SecurityTask *>(e->getData());

        switch (task->type) {
                case SECURITY_TASK_GENERATE_CERTIFICATE:
                        HAGGLE_DBG("Certificate generated!\n");
			if (task->cert->getSubject() == kernel->getThisNode()->getIdStr()) {
				HAGGLE_DBG("Certificate is my own\n");
				
				/* Save our private key and our certificate */
				privKey = task->privKey;
				myCert = task->cert;
				signalIsReadyForStartup();
			}
			break;
                case SECURITY_TASK_VERIFY_CERTIFICATE:
			/*
			if (task->cert && task->cert->isVerified()) {
				printCertificates();
			}
			*/ 
			break;
		case SECURITY_TASK_VERIFY_DATAOBJECT:
			if (task->dObj->hasValidSignature()) {
				HAGGLE_DBG("DataObject %s has a valid signature!\n", task->dObj->getIdStr());
				kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_VERIFIED, task->dObj));
			} else {
				HAGGLE_DBG("DataObject %s has an unverifiable signature!\n", task->dObj->getIdStr());
			}
			break;
                case SECURITY_TASK_SIGN_DATAOBJECT:
                        break;
        }
	delete task;
}

void SecurityManager::onReceivedDataObject(Event *e)
{
	DataObjectRef dObj;
	
	if (!e || !e->hasData())
		return;
	
	dObj = e->getDataObject();
	
	// Check if the data object's signature should be verified. Otherwise, generate the 
	// verified event immediately.
	if (dObj->shouldVerifySignature() && 
	    ((dObj->isNodeDescription() && securityLevel > SECURITY_LEVEL_LOW) || 
	     (securityLevel > SECURITY_LEVEL_MEDIUM))) {
		helper->addTask(new SecurityTask(SECURITY_TASK_VERIFY_DATAOBJECT, dObj));
	} else {
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_VERIFIED, dObj));	
	}
}

/*
	Check incoming data objects for two reasons:
	1) whether they have an embedded certificate, in which case we verify 
	it and add it to our store in case it is not already there.
	2) sign any data objects that were generated by local applications.
 */
void SecurityManager::onIncomingDataObject(Event *e)
{
	DataObjectRef dObj;
	
	if (!e || !e->hasData())
		return;
	
	dObj = e->getDataObject();
	
	Metadata *m = dObj->getMetadata()->getMetadata("Security");
	
	// Check if there is a certificate embedded that we do not already have stored
	if (m && m->getMetadata("Certificate")) {
		HAGGLE_DBG("Data object has embedded certificate, trying to verify it!\n");
		helper->addTask(new SecurityTask(SECURITY_TASK_VERIFY_CERTIFICATE, dObj));
	}
			
	InterfaceRef iface = dObj->getRemoteInterface();

	// Check if this data object came from an application, in that case we sign it.
	// In the future, the signing should potentially be handled by the application
	// itself. But this requires some major rethinking of how to manage certificates 
	// and keys, etc.
	if (iface && iface->getType() == IFTYPE_APPLICATION_PORT && dObj->shouldSign()) {
		HAGGLE_DBG("Data object should be signed\n");

		// FIXME: data objects should really be signed in the SecurityHelper thread since
		// it is a potentially CPU intensive operation. But it is currently not possible
		// to ensure that the signing operation has finished in the helper thread before
		// the data object is added to the data store.
		if (helper->signDataObject(privKey, dObj)) {
			HAGGLE_DBG("Successfully signed data object %s, which was added by an application.\n", dObj->getIdStr());
		} else {
			HAGGLE_DBG("Signing of data object %s, which was added by an application, failed!\n", dObj->getIdStr());
		}
	}
}
	
/*
	On send events, the security manager 
 
 */
void SecurityManager::onSendDataObject(Event *e)
{
	if (!e || !e->hasData())
		return;

	DataObjectRef dObj = e->getDataObject();
	
	if (dObj->isThisNodeDescription()) {
		// This is our node description. Piggy-back our certificate.
		if (myCert) {
			Metadata *m = dObj->getMetadata()->addMetadata("Security");
			
			if (m) {
				m->addMetadata(myCert->toMetadata());
			}
		}
	}
	
	// In most cases the data object is already signed here (e.g., if it is generated by a local
	// application, or was received from another node). The only reason to check if we should
	// sign the data object here, is if a data object was generated internally by Haggle -- in
	// which case the data object might not have a signature (e.g., the data object is a node
	// description).
	InterfaceRef iface = dObj->getRemoteInterface();
	
	if (dObj->shouldSign() && !(iface && iface->getType() == IFTYPE_APPLICATION_PORT)) {
		// FIXME: data objects should really be signed in the SecurityHelper thread since
		// it is a potentially CPU intensive operation. But it is currently not possible
		// to ensure that the signing operation has finished in the helper thread before
		// the data object is actually sent on the wire by the protocol manager.
		// To handle this situation, we probably need to add a new public event for 
		// security related operations, after which the security manager generates the
		// real send event.
		
		if (helper->signDataObject(privKey, dObj)) {
			HAGGLE_DBG("Successfully signed data object %s\n", dObj->getIdStr());
		} else {
			HAGGLE_DBG("Signing of data object %s failed!\n", dObj->getIdStr());
		}
	}	
}
