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

#include "ForwarderRank.h"

ForwarderRank::ForwarderRank(ForwardingManager *m, const string name) : Forwarder(m, name)
{
  const char *nodeId = getManager()->getKernel()->getThisNode()->getIdStr();
  myRank = nodeId[0];

	// Create a new data object:
	myMetricDO = new DataObject((const char *) NULL, 0);

	// Add the Forward=<node id>
	myMetricDO->addAttribute("Forward", getManager()->getKernel()->getThisNode()->getIdStr());

	// Get the metadata of the data object:
	Metadata *md = myMetricDO->getMetadata();
	// Find the "Forward" section:
	Metadata *forw = md->getMetadata("Forward");
	// No such section? Create one:
	if (!forw) {
		forw = md->addMetadata("Forward");
	}
	// Put in the rank in the forwarding section:
	String tmp;
	stringprintf(tmp, "%ld", myRank);
	forw->addMetadata("Rank", tmp);

addMetricDO(myMetricDO);
	/*
	// Print the data object:
	char *str;
	size_t len;
	md->getRawAlloc(&str, &len);
	HAGGLE_DBG("Created ForwarderRank metric data object:\n%s", str);
	free(str);
	*/
}

ForwarderRank::~ForwarderRank()
{
}

void ForwarderRank::addMetricDO(DataObjectRef &metricDO)
{
	if(!metricDO)
		return;

	const Attribute *attr = metricDO->getAttribute("Forward");
	if(!attr)
		return;

	Metadata *md = metricDO->getMetadata();
	// Find the "Forward" section:
	Metadata *forw = md->getMetadata("Forward");
	// No such section? This isn't valid:
	if (!forw)
		return;

	Metadata *rank = forw->getMetadata("Rank");
	if (!rank)
		return;
	
	ranks[attr->getValue()] = strtol(rank->getContent().c_str(), NULL, 10);

	/*
	// Output that we got a new metric do:
	String str;
	
	str = "Got metric DO: " + attr->getValue() + " - " + rank->getContent();
	HAGGLE_DBG("%s\n", str.c_str());
	*/
}

void ForwarderRank::newNeighbor(NodeRef &neighbor)
{
}

void ForwarderRank::endNeighbor(NodeRef &neighbor)
{
}

void ForwarderRank::generateTargetsFor(NodeRef &neighbor)
{

}

void ForwarderRank::generateDelegatesFor(DataObjectRef &dObj, NodeRef &target)
{
	NodeRefList delegateList;

	for (Map<String, long >::iterator it = 
			ranks.begin();
		it != ranks.end();
		it++)
	{
		if(it->second > myRank)
		{
			NodeRef new_node = new Node(it->first.c_str(), NODE_TYPE_PEER, "Rank delegate node");
			delegateList.add(new_node);
		}
	}

	if (!delegateList.empty())
		getManager()->getKernel()->addEvent(new Event(EVENT_TYPE_DELEGATE_NODES, dObj, target, delegateList));
}


string ForwarderRank::getEncodedState(void)
{
	return "";
}

void ForwarderRank::setEncodedState(string &state)
{
}
