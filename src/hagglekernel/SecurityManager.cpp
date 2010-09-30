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
#include <ctype.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

// The reason for this function being a macro, is so that HAGGLE_DBG can 
// specify which function called writeErrors().
#if defined(DEBUG)
#define writeErrors(prefix) \
{ \
	unsigned long writeErrors_e; \
	char writeErrors_buf[256]; \
	do { \
		writeErrors_e = ERR_get_error(); \
		if (writeErrors_e != 0) \
			HAGGLE_DBG(prefix "%s\n", \
				ERR_error_string(writeErrors_e, writeErrors_buf)); \
	} while(writeErrors_e != 0); \
}
#else
#define writeErrors(prefix)
#endif

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

SecurityTask::SecurityTask(const SecurityTaskType_t _type, DataObjectRef _dObj, CertificateRef _cert) : type(_type), completed(false), dObj(_dObj), privKey(NULL), cert(_cert) 
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
	if (isRunning())
		stop();
	
	while (!taskQ.empty()) {
		SecurityTask *task = NULL;
		taskQ.retrieve(&task);
		delete task;
	}
}

bool SecurityHelper::signDataObject(DataObjectRef& dObj, RSA *key)
{
	unsigned char *signature;
	
	if (!key || !dObj) 
		return false;
	
	unsigned int siglen = RSA_size(key);
	
	signature = (unsigned char *)malloc(siglen);
	
	if (!signature)
		return false;

	printf("signing data object, siglen=%u\n", siglen);

	memset(signature, 0, siglen);
	
	if (RSA_sign(NID_sha1, dObj->getId(), sizeof(DataObjectId_t), signature, &siglen, key) != 1) {
		free(signature);
		return false;
	}
	
	dObj->setSignature(getManager()->getKernel()->getThisNode()->getIdStr(), signature, siglen);
	
	// Assume that our own signature is valid
	dObj->setSignatureStatus(DataObject::SIGNATURE_VALID);
	
	// Do not free the allocated signature as it is now owned by the data object...
	
	return true;
}

bool SecurityHelper::verifyDataObject(DataObjectRef& dObj, CertificateRef& cert) const
{
	RSA *key;
	
	// Cannot verify without signature
	if (!dObj->getSignature()) {
		HAGGLE_ERR("No signature in data object, cannot verify\n");
		return false;
	}	
	writeErrors("(not this): ");
	
	key = cert->getPubKey();

	if (RSA_verify(NID_sha1, dObj->getId(), sizeof(DataObjectId_t), 
		       const_cast<unsigned char *>(dObj->getSignature()), dObj->getSignatureLength(), key) != 1) {
		char *raw;
		size_t len;
		writeErrors("");
		dObj->getRawMetadataAlloc((unsigned char **)&raw, &len);
		if (raw) {
			HAGGLE_DBG("Signature is invalid:\n%s\n", raw);
			free(raw);
		}
		dObj->setSignatureStatus(DataObject::SIGNATURE_INVALID);

		return false;
	}
	
	HAGGLE_DBG("Signature is valid\n");
	dObj->setSignatureStatus(DataObject::SIGNATURE_VALID);

	return true;
}

void SecurityHelper::doTask(SecurityTask *task)
{
	Metadata *m = NULL;
	
        switch (task->type) {
                case SECURITY_TASK_GENERATE_CERTIFICATE:
                        HAGGLE_DBG("Creating certificate for id=%s\n", 
				   getManager()->getKernel()->getThisNode()->getIdStr());

			if (strlen(getManager()->getKernel()->getThisNode()->getIdStr()) == 0) {
				HAGGLE_ERR("Certificate creation failed: This node's id is not valid!\n");
				break;
			}
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
						} else {
							HAGGLE_DBG("Invalid certificate.\n");
                                                        //printf("%s", task->cert->toString().c_str());
						}
					} else {
						HAGGLE_DBG("Unable to create certificate from metadata\n");
					}
				} else {
					HAGGLE_DBG("No certificate in metadata\n");
				}
			} else {
				HAGGLE_DBG("No security metadata\n");
			}
                        break;
                case SECURITY_TASK_VERIFY_DATAOBJECT:
			// Check for a certificate:
			task->cert = getManager()->retrieveCertificate(task->dObj->getSignee());
			
			if (task->cert) {
				HAGGLE_DBG("Verifying data object [%s]\n", task->dObj->getIdStr());
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
	Manager("SecurityManager", _haggle), securityLevel(slevel), etype(EVENT_TYPE_INVALID), helper(NULL), 
	myCert(NULL), ca_issuer(CA_ISSUER_NAME), caPrivKey(NULL), caPubKey(NULL), privKey(NULL)
{

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

        // Unload OpenSSL algorithms
        EVP_cleanup();

#if defined(DEBUG)
	// Release ssl error strings.
	ERR_free_strings();
#endif
}

bool SecurityManager::init_derived()
{
#define __CLASS__ SecurityManager
	int ret;
	
	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_RECEIVED, onReceivedDataObject);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_SEND, onSendDataObject);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}

	ret = setEventHandler(EVENT_TYPE_DATAOBJECT_INCOMING, onIncomingDataObject);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
	
#ifdef DEBUG
	setEventHandler(EVENT_TYPE_DEBUG_CMD, onDebugCmdEvent);

	if (ret < 0) {
		HAGGLE_ERR("Could not register event handler\n");
		return false;
	}
#endif
	
	onRepositoryDataCallback = newEventCallback(onRepositoryData);

        /* This function must be called to load crypto algorithms used
         * for signing and verification of certificates. */
        OpenSSL_add_all_algorithms();

#if defined(DEBUG)
	/* Load ssl error strings. Needed by ERR_error_string() */
	ERR_load_crypto_strings();
#endif	
        // -- retrieve CA key from memory
	caPrivKey = stringToRSAKey(ca_private_key, KEY_TYPE_PRIVATE);
	
	if (!caPrivKey) {
		HAGGLE_ERR("Could not read CA's private key from memory\n");
		return false;
	}
	
	caPubKey = stringToRSAKey(ca_public_key, KEY_TYPE_PUBLIC);

	if (!caPubKey) {
		HAGGLE_ERR("Could not read CA's public key from memory\n");
		return false;
	}

	HAGGLE_DBG("Successfully read CA's public key\n");

	etype = registerEventType("SecurityTaskEvent", onSecurityTaskComplete);

	HAGGLE_DBG("Security level is set to %s\n", security_level_names[securityLevel]);
	
	helper = new SecurityHelper(this, etype);

	if (!helper || !helper->start()) {
		HAGGLE_ERR("Could not create or start security helper\n");
		return false;
	}

	HAGGLE_DBG("Initialized security manager\n");

	return true;
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
			
			privKey = stringToRSAKey(re->getValueStr(), KEY_TYPE_PRIVATE);
			
			HAGGLE_DBG("Read my own private key from repository\n");
		} else {
			CertificateRef c = Certificate::fromPEM(re->getValueStr());
			
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
			/*
			  NOTE:
			  Here is the possibility to generate a EVENT_TYPE_DATAOBJECT_VERIFIED
			  event even if the data object had a bad signature. In that case, the
			  Data manager will remove the data object from the bloomfilter so that
			  it can be received again (hopefully with a valid signature the next time).
			  However, this means that also the data object with the bad signature
			  can be received once more, in worst case in a never ending circle.

			  Perhaps the best solution is to hash both the data object ID and the 
			  signature (if available) into a node's bloomfilter?
			*/
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
	if (!e || !e->hasData())
		return;
	
	DataObjectRef& dObj = e->getDataObject();
	
	if (!dObj)
		return;

	if (dObj->isDuplicate()) {
		HAGGLE_DBG("Data object [%s] is a duplicate, ignoring\n", dObj->getIdStr());		
		return;
	}
	// Check if the data object's signature should be verified. Otherwise, generate the 
	// verified event immediately.
	if (dObj->signatureIsUnverified() && 
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
	
	if (dObj->isDuplicate())
		return;

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
	if (iface && iface->getType() == Interface::TYPE_APPLICATION_PORT && dObj->shouldSign()) {
		HAGGLE_DBG("Data object should be signed\n");

		// FIXME: data objects should really be signed in the SecurityHelper thread since
		// it is a potentially CPU intensive operation. But it is currently not possible
		// to ensure that the signing operation has finished in the helper thread before
		// the data object is added to the data store.
		if (helper->signDataObject(dObj, privKey)) {
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
	
	if (dObj->shouldSign() && !(iface && iface->getType() == Interface::TYPE_APPLICATION_PORT)) {
		// FIXME: data objects should really be signed in the SecurityHelper thread since
		// it is a potentially CPU intensive operation. But it is currently not possible
		// to ensure that the signing operation has finished in the helper thread before
		// the data object is actually sent on the wire by the protocol manager.
		// To handle this situation, we probably need to add a new public event for 
		// security related operations, after which the security manager generates the
		// real send event.
		
		if (helper->signDataObject(dObj, privKey)) {
			HAGGLE_DBG("Successfully signed data object %s\n", dObj->getIdStr());
		} else {
			HAGGLE_DBG("Signing of data object %s failed!\n", dObj->getIdStr());
		}
	}	
}

void SecurityManager::onConfig(Metadata *m)
{
	const char *param = m->getParameter("security_level");
	
	if (param) {
		char *level = new char[strlen(param) + 1];
		size_t i;
		
		// Convert string to uppercase
		for (i = 0; i < strlen(param); i++) {
			level[i] = toupper(param[i]);
		}
		
		level[i] = '\0';
		
		if (strcmp(level, security_level_names[SECURITY_LEVEL_HIGH]) == 0) {
			securityLevel = SECURITY_LEVEL_HIGH;
			HAGGLE_DBG("Security level set to %s\n", security_level_names[SECURITY_LEVEL_HIGH]);
		} else if (strcmp(level, security_level_names[SECURITY_LEVEL_MEDIUM]) == 0) {
			securityLevel = SECURITY_LEVEL_MEDIUM;
			HAGGLE_DBG("Security level set to %s\n", security_level_names[SECURITY_LEVEL_MEDIUM]);
		} else if (strcmp(level, security_level_names[SECURITY_LEVEL_LOW]) == 0) {
			securityLevel = SECURITY_LEVEL_LOW;
			HAGGLE_DBG("Security level set to %s\n", security_level_names[SECURITY_LEVEL_LOW]);
		} else {
			HAGGLE_ERR("Unrecognized security level '%s'\n", level);
		}
		
		delete [] level;
	}
}

