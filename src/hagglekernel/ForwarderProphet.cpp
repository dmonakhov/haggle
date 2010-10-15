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

#include <libcpphaggle/Platform.h>
#include "ForwarderProphet.h"
#include "XMLMetadata.h"

#include <math.h>

ForwarderProphet::ForwarderProphet(ForwardingManager *m, const EventType type, 
				   ForwardingStrategy *_forwarding_strategy) :
	ForwarderAsynchronous(m, type, PROPHET_NAME),
	kernel(getManager()->getKernel()), next_id_number(1),
	rib_timestamp(Timeval::now()), forwarding_strategy(_forwarding_strategy)
{
	// Ensure that the local node's forwarding id is 1:
	id_for_string(kernel->getThisNode()->getIdStr());
	
	HAGGLE_DBG("Forwarding module \'%s\' initialized with forwarding strategy \'%s\'\n", 
		   getName(), forwarding_strategy->getName().c_str()); 
}

ForwarderProphet::~ForwarderProphet()
{
	if (forwarding_strategy)
		delete forwarding_strategy;
}

size_t ForwarderProphet::getSaveState(RepositoryEntryList& rel)
{
	for (prophet_rib_t::iterator it = rib.begin(); it != rib.end(); it++) {
		char value[256];
		snprintf(value, 256, "%lf:%s", (*it).second.first, (*it).second.second.getAsString().c_str());
		//printf("Repository value is %s\n", value);
		rel.push_back(new RepositoryEntry(getName(), id_number_to_nodeid[(*it).first].c_str(), value));
	}
	
	return rel.size();
}

bool ForwarderProphet::setSaveState(RepositoryEntryRef& e)
{
	if (strcmp(e->getAuthority(), getName()) != 0)
		return false;
	
	string value = e->getValueStr();
	
	// Find the separating ':' character in the string
	size_t pos = value.find(':');
	
	prophet_metric_t& metric = rib[id_for_string(e->getKey())];
	
	// The first part of the value is the P_ab metric
	metric.first = strtod(value.substr(0, pos).c_str(), NULL);
	// The second part is the timeval string
	metric.second = Timeval(value.substr(pos + 1));
	
	/*
	 printf("orig string=%s metric2=%lf, timeval2=%s\n", 
		e->getValue(), metric.first, metric.second.getAsString().c_str());
	*/
	rib_timestamp = Timeval::now();
	
	return true;
}

prophet_node_id_t ForwarderProphet::id_for_string(const string& nodeid)
{
	prophet_node_id_t &retval = nodeid_to_id_number[nodeid];

	if (retval == 0) {
		retval = next_id_number;
		id_number_to_nodeid[retval] = nodeid;
		next_id_number++;
	}
	return retval;
}
/*
 This function takes a metric and ages it if the time since the last aging is
 larger than PROPHET_AGING_TIME_UNIT.
 Typically, this function is called every time a metric is accessed.
 */
prophet_metric_t& ForwarderProphet::age_metric(prophet_metric_t& metric, bool force)
{
	Timeval now = Timeval::now();
	
	if (metric.second == 0) {
		/* The metric is new, so we do not age it, but instead
		   we set its age timestamp to the current time.
		 */
		metric.second = now;
		return metric;
	}
		
	int K = (now - metric.second).getSeconds() / PROPHET_AGING_TIME_UNIT;
	
	if (K > 0) {
		double &P_ab = metric.first;
		/*
		 Age according to the Prophet draft:
		 
			P_(A,B) = P_(A,B)_old * gamma^K (2)
		 */
		P_ab = P_ab * pow(PROPHET_GAMMA, K);
		
		if (P_ab < 0.000001) {
			// Let's say it's 0:
			P_ab = 0.0;
		}
		
		metric.second = now;
	}
	
	rib_timestamp = Timeval::now();
	
	return metric;
}
/*
 This function is called every time new routing information is received from
 a neighbor. The input is the part of the metadata which contains the
 routing information. This means that the metadata could originally be part 
 of any data object.
 */
bool ForwarderProphet::newRoutingInformation(const Metadata *m)
{	
	if (!m || m->getName() != getName())
		return false;
	
	prophet_node_id_t node_b_id = id_for_string(m->getParameter("node_id"));
	prophet_rib_t &neighbor_rib = neighbor_ribs[node_b_id];
	
	HAGGLE_DBG("New %s routing information received from node [id=%s]\n",
		   getName(),
		   m->getParameter("node_id"));
	
	const Metadata *mm = m->getMetadata("Metric");
	
	while (mm) {
		prophet_node_id_t node_c_id = id_for_string(mm->getParameter("node_id"));
		double &P_bc = neighbor_rib[node_c_id].first;
		// Read the metric from the neighbor's metadata:
		sscanf(mm->getContent().c_str(), "%lf", &P_bc);
		
		//printf("node_c_str=%s node_c_id=%u P_bc=%lf\n", mm->getParameter("node_id"), node_c_id, P_bc);
		
		if (node_c_id != this_node_id) {
			double &P_ab = age_metric(rib[node_b_id]).first;
			double &P_ac = age_metric(rib[node_c_id]).first;
		
			/* 
			 Compute the transitative increase in metric.
			 
			 From the Prophet draft:
			 
			 P_(A,C) = P_(A,C)_old + ( 1 - P_(A,C)_old ) * P_(A,B) *
			 P_(B,C) * beta                                (3)
			 
			 As a special case, the P-value for a node itself is always defined to
			 be 1 (i.e., P_(A,A)=1).
			*/
			P_ac = P_ac + (1 - P_ac) * P_ab * P_bc * PROPHET_BETA;
		}
		
		mm = m->getNextMetadata();
	}
	
	rib_timestamp = Timeval::now();
	
	return true;
}

/*
 Add routing information to a data object.
 The parameter "parent" is the location in the data object where the routing 
 information should be inserted.
 */ 
bool ForwarderProphet::addRoutingInformation(DataObjectRef& dObj, Metadata *parent)
{
	if (!dObj || !parent)
		return false;
	
	// Add first our own node ID.
	parent->setParameter("node_id", kernel->getThisNode()->getIdStr());
	
	prophet_rib_t::iterator it;
	
	for (it = rib.begin(); it != rib.end(); it++) {
		if (it->second.first != 0.0) {
			char metric[32];
			sprintf(metric, "%lf", age_metric(it->second).first);
			Metadata *mm = parent->addMetadata("Metric", metric);
			// Mark which node this metric was for.
			mm->setParameter("node_id", id_number_to_nodeid[it->first]);
		}
	}
	
	dObj->setCreateTime(rib_timestamp);
		
	return true;
}

void ForwarderProphet::_newNeighbor(const NodeRef &neighbor)
{
	// We don't handle routing to anything but other haggle nodes:
	if (neighbor->getType() != Node::TYPE_PEER)
		return;
	
	// Update our private metric regarding this node:
	prophet_node_id_t neighbor_id = id_for_string(neighbor->getIdStr());
	
	// Remember that rib[neighbor_id] is an log(n) operation - do it 
	// sparingly:
	double &P_ab = rib[neighbor_id].first;
	
	P_ab = P_ab + (1 - P_ab) * PROPHET_P_ENCOUNTER;
	
	rib_timestamp = Timeval::now();
}

void ForwarderProphet::_endNeighbor(const NodeRef &neighbor)
{
	// We don't handle routing to anything but other haggle nodes:
	if (neighbor->getType() != Node::TYPE_PEER)
		return;
	
	// Update our private metric regarding this node:
	prophet_node_id_t neighbor_id = id_for_string(neighbor->getIdStr());
	
	// Remember that rib[neighbor_id] is an log(n) operation - do it 
	// sparingly:
	prophet_metric_t &metric = rib[neighbor_id];
	double &P_ab = metric.first;
	
	/* 
	 Age by one time interval when neigbhors go away 
	 (i.e., PROPHET_GAMMA^K, where K=1)
	 
	 NOTE: This is an out of draft addition to Prophet.
	 */
        P_ab = P_ab * PROPHET_GAMMA;
        
        // Is this metric close to 0?
        if (P_ab < 0.000001) {
                // Let's say it's 0:
                P_ab = 0.0;
        }
	metric.second = Timeval::now();
	rib_timestamp = metric.second;
}

void ForwarderProphet::_generateTargetsFor(const NodeRef &neighbor)
{
	NodeRefList lst;
	// Figure out which forwarding table to look in:
	prophet_node_id_t neighbor_id = id_for_string(neighbor->getIdStr());
	prophet_rib_t &neighbor_rib = neighbor_ribs[neighbor_id];
	
	HAGGLE_DBG("%s: Finding targets for which neighbor '%s' is a good delegate\n", 
		   getName(), neighbor->getName().c_str());
		   
	// Go through the neighbor's forwarding table:
	for (prophet_rib_t::iterator it = neighbor_rib.begin(); it != neighbor_rib.end(); it++) {
		// Skip ourselves and that neighbor (if these accidentally ended up in 
		// that table)
		if (it->first != this_node_id && it->first != neighbor_id) {
			// Does the neighbor node have a better chance of forwarding to this
			// node target than we do?
			// In other words, as the Prophet draft puts it, is P_bd > P_ad?
			double &P_ad = age_metric(rib[it->first]).first;
			double &P_bd = it->second.first;
			
			if ((*forwarding_strategy)(P_ad, P_bd)) {
				// Yes: insert this node into the list of targets for this 
				// delegate forwarder.
				
				NodeRef target = Node::create_with_id(Node::TYPE_PEER, id_number_to_nodeid[it->first].c_str(), "PRoPHET target node");
                                
				if (target) {
					lst.push_back(target);
                                        HAGGLE_DBG("Neighbor '%s' is a good delegate for target '%s' [my_metric=%lf, neighbor_metric=%lf]\n", 
						   neighbor->getName().c_str(), target->getName().c_str(), P_ad, P_bd);
                                }
			}
		}
	}
	
	if (!lst.empty()) {
		kernel->addEvent(new Event(EVENT_TYPE_TARGET_NODES, neighbor, lst));
	}
}

void ForwarderProphet::_generateDelegatesFor(const DataObjectRef &dObj, const NodeRef &target, const NodeRefList *other_targets)
{
	NodeRefList delegates;
	// Figure out which node to look for:
	prophet_node_id_t target_id = id_for_string(target->getIdStr());
	
	// Retreive this value once, since it is an O(log(n)) operation to do:
	// We age the metric first since the target is not a neighbor
	double &P_ad = age_metric(rib[target_id]).first;
	
	// Go through the neighbor's forwarding table:
	for (Map<prophet_node_id_t, prophet_rib_t>::iterator it = neighbor_ribs.begin(); it != neighbor_ribs.end(); it++) {
		// Exclude ourselves and the target node from the list of good delegate
		// forwarders:
		if (it->first != this_node_id && it->first != target_id) {
                        //NodeRef delegate = Node::create_with_id(Node::TYPE_PEER, id_number_to_nodeid[it->first].c_str(), "PRoPHET delegate node");
			
			NodeRef delegate = kernel->getNodeStore()->retrieve(id_number_to_nodeid[it->first], true);
			
			if (delegate && !isTarget(delegate, other_targets)) {
				// Do not age P_bc since the metric is for a current neighbor... or should we?
				// The draft is not really clear on how to age metrics for neighbors 
				double &P_bd = it->second[target_id].first;
				
				// Would this be a good delegate?
				if ((*forwarding_strategy)(P_ad, P_bd)) {
					// Yes: insert this node into the list of delegate forwarders 
					// for this target.
					
					delegates.push_back(delegate);
					HAGGLE_DBG("Node '%s' is a good delegate for target '%s' [my_metric=%lf, neighbor_metric=%lf]\n", 
						   delegate->getName().c_str(), target->getName().c_str(), P_ad, P_bd);
					
				} else {
					HAGGLE_DBG("Node '%s' is NOT a good delegate for target '%s' [my_metric=%lf, neighbor_metric=%lf]\n", 
						   delegate->getName().c_str(), target->getName().c_str(), P_ad, P_bd);
				}
			}
		}
	}
	
	if (!delegates.empty()) {
		kernel->addEvent(new Event(EVENT_TYPE_DELEGATE_NODES, dObj, target, delegates));
	} else {
                HAGGLE_DBG("No delegates found for target %s\n", target->getName().c_str());
        }
}

void ForwarderProphet::_onForwarderConfig(const Metadata& m)
{
	if (strcmp(getName(), m.getName().c_str()) != 0)
		return;
	
	HAGGLE_DBG("Prophet forwarder configuration\n");

	const char *param = m.getParameter("strategy");
	
	if (param) {
		if (strcmp(param, "GRTR") == 0) {
			delete forwarding_strategy;
			forwarding_strategy = new GRTR();
		} else if (strcmp(param, "GTMX") == 0) {
			delete forwarding_strategy;
			forwarding_strategy = new GTMX();
		}
		HAGGLE_DBG("%s: Setting %s forwarding strategy\n", 
			   getName(), forwarding_strategy->getName().c_str());
	}
	
	param = m.getParameter("P_encounter");
	
	if (param) {
		char *ptr = NULL;
		double p = strtod(param, &ptr);
		
		if (ptr && ptr != param && *ptr == '\0') {
			HAGGLE_DBG("%s: Setting P_encounter to %lf\n", getName(), p);
			P_encounter = p;
		}
	}
	
	param = m.getParameter("alpha");
	
	if (param) {
		char *ptr = NULL;
		double p = strtod(param, &ptr);
		
		if (ptr && ptr != param && *ptr == '\0') {
			HAGGLE_DBG("%s: Setting alpha to %lf\n", getName(), p);
			alpha = p;
		}
	}
	
	param = m.getParameter("beta");
	
	if (param) {
		char *ptr = NULL;
		double p = strtod(param, &ptr);
		
		if (ptr && ptr != param && *ptr == '\0') {
			HAGGLE_DBG("%s: Setting beta to %lf\n", getName(), p);
			beta = p;
		}
	}
	
	param = m.getParameter("gamma");
	
	if (param) {
		char *ptr = NULL;
		double p = strtod(param, &ptr);
		
		if (ptr && ptr != param && *ptr == '\0') {
			HAGGLE_DBG("%s: Setting gamma to %lf\n", getName(), p);
			gamma = p;
		}
	}
	
	param = m.getParameter("aging_time_unit");
	
	if (param) {
		char *ptr = NULL;
		double p = strtod(param, &ptr);
		
		if (ptr && ptr != param && *ptr == '\0') {
			HAGGLE_DBG("%s: Setting aging_time_unit to %lf\n", getName(), p);
			aging_time_unit = p;
		}
	}
	
	param = m.getParameter("aging_constant");
	
	if (param) {
		char *ptr = NULL;
		double p = strtod(param, &ptr);
		
		if (ptr && ptr != param && *ptr == '\0') {
			HAGGLE_DBG("%s: Setting aging_constant to %lf\n", getName(), p);
			aging_constant = p;
		}
	}
}

#ifdef DEBUG
void ForwarderProphet::_printRoutingTable(void)
{
	printf("%s routing table:\n", getName());
	
	for (Map<prophet_node_id_t, string>::iterator it = 
	     id_number_to_nodeid.begin(); it != id_number_to_nodeid.end(); it++) {
		printf("%ld: %s\n", it->first, it->second.c_str());
	}
		
	printf("internal: {");
	prophet_rib_t::iterator jt = rib.begin();
	
	while (jt != rib.end()) {
		// Only age metrics if the node is not a neighbor
		bool isNeighbor = getKernel()->getNodeStore()->stored(id_number_to_nodeid[jt->first], true);
		
		printf("%ld: %lf", jt->first, isNeighbor ? jt->second.first : age_metric(jt->second).first);
		jt++;
		if (jt != rib.end())
			printf(", ");
	}
	printf("}\n");
	
	for (Map<prophet_node_id_t, prophet_rib_t>::iterator it = 
	     neighbor_ribs.begin(); it != neighbor_ribs.end(); it++) {
		printf("%ld: {", it->first);;
		prophet_rib_t::iterator jt = it->second.begin();
		while (jt != it->second.end()) {
			printf("%ld: %lf", jt->first, jt->second.first);
			jt++;
			if (jt != it->second.end())
				printf(", ");
		}
		printf("}\n");
	}
}
#endif

