/* Copyright 2009 Uppsala University
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

#include "Forwarder.h"

DataObjectRef Forwarder::createRoutingInformationDataObject()
{
	// No need to have a reference in this function because it won't be 
	// visible outside until this function is done with it.
	DataObjectRef dObj = new DataObject();
	
	if (!dObj)
		return NULL;

	dObj->setPersistent(false);
	dObj->addAttribute("Forwarding", getName());
	
	// Add the metric data to the forwarding section:
	Metadata *md = dObj->getMetadata()->addMetadata(getManager()->getName());
	
	md = md->addMetadata(getName());
	md->setParameter("node_id", getKernel()->getThisNode()->getIdStr());
	
	if (!addRoutingInformation(dObj, md)) {
		HAGGLE_ERR("Could not add routing information\n");
		return NULL;
	}
	
	return dObj;
}

bool Forwarder::hasRoutingInformation(const DataObjectRef& dObj) const
{
	if (!dObj)
		return false;
	
	const Metadata *m = dObj->getMetadata()->getMetadata(getManager()->getName());
	
	if (m == NULL)
		return false;
		
	if (!m->getMetadata(getName()))
		return false;
	    
	return true;
}

const string Forwarder::getNodeIdFromRoutingInformation(const DataObjectRef& dObj) const
{
	if (!dObj)
		return (char *)NULL;
	
	const Metadata *m = dObj->getMetadata()->getMetadata(getManager()->getName());
	
	if (!m)
		return (char *)NULL;
	
	m = m->getMetadata(getName());
	
	if (!m)
		return (char *)NULL;
	
	return m->getParameter("node_id");
}

const Metadata *Forwarder::getRoutingInformation(const DataObjectRef& dObj) const
{
	if (!dObj)
		return NULL;
	
	const Metadata *md = dObj->getMetadata();
	
	if (md == NULL)
		return NULL;
	
	md = md->getMetadata(getManager()->getName());
	
	if (md == NULL)
		return NULL;
	
	md = md->getMetadata(getName());
	
	if (md == NULL)
		return NULL;
	
	return md;
}

bool Forwarder::isTarget(const NodeRef &delegate, const NodeRefList *targets) const
{
	if (!targets)
		return false;

	for (NodeRefList::const_iterator it = targets->begin(); it != targets->end(); it++) {
		if (*it == delegate)
			return true;
	}
	return false;
}
