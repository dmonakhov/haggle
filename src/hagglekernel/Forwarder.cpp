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

#define FORWARDING_METADATA_NAME	("Forwarding")
#define METRIC_METADATA_NAME		("Metric")

void Forwarder::createMetricDataObject(string forwardingMetric)
{
	// No need to have a reference in this function because it won't be 
	// visible outside until this function is done with it.
	DataObject *newDO;
	
	// Create a new, empty, data object:
	newDO = new DataObject((const char *) NULL, 0);
	
	// Forwarding data objects are intentionally persistent. See the 
	// declaration of myMetricDO in Forwarder.h.
	
	// Add the attribute that says this is a forwarding metric data object:
	newDO->addAttribute(
		forwardAttributeName, 
		getManager()->getKernel()->getThisNode()->getIdStr());
	
	// Add the metric data to the forwarding section:
	Metadata *md = newDO->getMetadata();
	Metadata *forw = md->getMetadata(FORWARDING_METADATA_NAME);
	
	if (!forw) {
		forw = md->addMetadata(FORWARDING_METADATA_NAME);
	}
	forw->addMetadata(METRIC_METADATA_NAME, forwardingMetric);
	
	// Set the create time so that forwarding managers can sort out which is
	// newest.
	newDO->setCreateTime();
	
	myMetricDO = newDO;
}

bool Forwarder::isMetricDO(const DataObjectRef dObj) const
{
	if (!dObj)
		return false;
	
	if (dObj->getAttribute(forwardAttributeName) == NULL)
		return false;
	
	if (dObj->getMetadata() == NULL)
		return false;
	
	if (dObj->getMetadata()->getMetadata(FORWARDING_METADATA_NAME) == NULL)
		return false;
	
	if (dObj->
			getMetadata()->
				getMetadata(FORWARDING_METADATA_NAME)->
					getMetadata(METRIC_METADATA_NAME) == NULL)
		return false;
	
	return true;
}

const string Forwarder::getNodeIdFromMetricDataObject(DataObjectRef dObj) const
{
	if (!dObj)
		return NULL;
	
	const Attribute *attr;
	attr = dObj->getAttribute(forwardAttributeName);
	if (attr == NULL)
		return NULL;
	return attr->getValue();
}

const string Forwarder::getMetricFromMetricDataObject(DataObjectRef dObj) const
{
	if (!dObj)
		return NULL;
	
	Metadata *md;
	md = dObj->getMetadata();
	if (md == NULL)
		return NULL;
	
	md = md->getMetadata(FORWARDING_METADATA_NAME);
	if (md == NULL)
		return NULL;
	
	md = md->getMetadata(METRIC_METADATA_NAME);
	if (md == NULL)
		return NULL;
	
	return md->getContent();
}
