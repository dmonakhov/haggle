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

#include "ForwarderProphet.h"

ForwarderProphet::ForwarderProphet(ForwardingManager *m, const string name) :
	ForwarderAsynchronous(Timeval(PROPHET_TIME_BETWEEN_AGING), m, name),
	kernel(getManager()->getKernel()), next_id_number(1)
{
	// Ensure that the local node's forwarding id is 1:
	id_for_string(kernel->getThisNode()->getIdStr());       
}

ForwarderProphet::~ForwarderProphet()
{
}

prophet_forwarding_id ForwarderProphet::id_for_string(string nodeid)
{
	prophet_forwarding_id &retval = nodeid_to_id_number[nodeid];

	if (retval == 0) {
		retval = next_id_number;
		id_number_to_nodeid[retval] = nodeid;
		next_id_number++;
	}
	return retval;
}

void ForwarderProphet::updateMetricDO(void)
{
	prophet_metric_table public_forwarding_table;
	
	// Go through all possible node ids:
	// Skip 0, because it refers to no node, and 1, because it refers to this
	// node.
	for (prophet_forwarding_id i = 2; i < next_id_number; i++) {
		double public_metric;
		
		// Assume a maximum metric of what the local metric is to start with:
		public_metric = my_metrics[i];
		
		for (Map<prophet_forwarding_id, prophet_metric_table>::iterator jt = 
				forwarding_table.begin();
			jt != forwarding_table.end(); jt++) {
			double tmp;
			
			// The forwarding metric via this node is:
			// (my chance of forwarding)*(his chance of forwarding)
			tmp = my_metrics[jt->first]*jt->second[i];
			// Get the maximum metric:
			if (public_metric < tmp)
				public_metric = tmp;
		}
		
		// Put the new value into the table:
		public_forwarding_table[i] = public_metric;
	}
	
	string forwarding_string = getMetricString(public_forwarding_table);
	bool shouldReplaceDO = true;
	
	if (myMetricDO) {
		// Get the data:
		Metadata *md = myMetricDO->getMetadata();
                
		if (md) {
			md = md->getMetadata("Forward");
			if (md) {
				md = md->getMetadata("PRoPHET");
				if (md) {
					if (md->getContent() == forwarding_string)
						shouldReplaceDO = false;
				}
			}
		}
	}
	
	if (shouldReplaceDO) {
		// No need to have a reference in this function because it won't be 
		// visible outside until this function is done with it.
		DataObject *newDO;
		
		// Create a new, empty, data object:
		newDO = new DataObject((const char *) NULL, 0);
		
		// Add the "Forward" attribute:
		newDO->addAttribute("Forward", 
                                kernel->getThisNode()->getIdStr());
		
		// Add the "PRoPHET" attribute to the forwarding section:
		Metadata *md = newDO->getMetadata();
                Metadata *forw = md->getMetadata("Forward");
                
                if (!forw) {
                        forw = md->addMetadata("Forward");
                }
                forw->addMetadata("PRoPHET", forwarding_string);
                
		// Set the create time so that forwarding managers can sort out which is
		// newest.
		newDO->setCreateTime();
		
		myMetricDO = newDO;
	}
}

/*
	For the moment, the format of a metric data object is as follows:
	
	The data object has an attribute Forward=<Node ID> showing that it is the
	forwarding data object for the node with that node id.
	
	The data object also has an attribute PRoPHET=<string> which encodes the 
	metrics. This should really be placed either under a 
	<Forwarding>...</Forwarding> section of the data object, but since that 
	would require changing parts of the metadata and data object code to allow,
	and that code is currently being considered for redesign anyway, I thought 
	I'd not do all that and use this solution for now.
	
	The prophet string is formatted like so:
		<Node ID>:<floating-point value>:<Node ID>...
	Each value is separated by a colon character, which cannot occur in the 
	node id or a floating point value.
*/

/*
	This function checks if a data object is a proper metric data object for 
	this forwarding module.
*/
static bool isProphetMetricDO(DataObjectRef dObj)
{
	if (dObj->getAttribute("Forward") == NULL)
		return false;
	
	if (dObj->getMetadata() == NULL)
		return false;
	
	if (dObj->getMetadata()->getMetadata("Forward") == NULL)
		return false;
	
	if (dObj->getMetadata()->getMetadata("Forward")->getMetadata("PRoPHET") == NULL)
		return false;
	
	return true;
}

string ForwarderProphet::getMetricString(prophet_metric_table &table)
{
	string retval = "";
	prophet_metric_table::iterator it;
	char tmp[32];
	
	for (it = table.begin(); it != table.end(); it++) {
		if (it->second != 0.0) {
			sprintf(tmp, "%f", it->second);
			retval += id_number_to_nodeid[it->first];
			retval += ':';
			retval += tmp;
			retval += ':';
		}
	}
	
	return retval;
}

void ForwarderProphet::parseMetricString(prophet_metric_table &table, string &metric)
{
	// Extract as byte array:
	const char *str = metric.c_str();
	
	// This holds one node id:
	char node_id[MAX_NODE_ID_STR_LEN];
	// Looping variables:
	long i, len;
	
	// Clean out the table: 
	table.clear();
	
	len = metric.length();
	i = 0;
	while (i < len) {
		long node_start = i;
		double metric;
		
		// Find and copy the node id:
		while (i < len && 
                       (i - node_start) < MAX_NODE_ID_STR_LEN && 
                       str[i] != ':') {
			node_id[i - node_start] = str[i];
			i++;
		}
		// Check that we didn't overrun the end of the string:
		if (i >= len)
			goto fail_end_of_string;
		// Check that we didn't overrun the end of the node id buffer:
		if ((i - node_start) > MAX_NODE_ID_STR_LEN)
			goto fail_malformed;

		// NULL-terminate the node id:
		node_id[i - node_start] = '\0';
		
		// Move to the beginning of the metric:
		i++;
		
		// Read the metric:
		sscanf(&(str[i]), "%lf", &metric);
		
		// Check that the metric isn't out of bounds:
		if (metric < 0.0 || metric >= 1.0)
			goto fail_malformed;
		
		// Insert the metric in the map:
		table[id_for_string(node_id)] = metric;
		
		// Move past the metric:
		while (i < len && str[i] != ':')
			i++;
		// Move beyond the last colon:
		// Please note that there is really no point in checking if we've 
		// already moved past the end of the buffer.
		i++;

		// This happens if we encounter the end of the string while parsing:
fail_end_of_string:;
		// This happens if the data is malformed or broken in some way:
fail_malformed:;
	}
}

void ForwarderProphet::_addMetricDO(DataObjectRef &dObj)
{
	// Check the data object:
	if (!isProphetMetricDO(dObj))
		return;
	
	// Figure out which node this metric is for:
	prophet_forwarding_id node = 
		id_for_string(dObj->getAttribute("Forward")->getValue());
	
	// Get the table for that node:
	prophet_metric_table &node_table = forwarding_table[node];
	
	// Get the data (no need to worry about malformed metadata as
	// we checked it above):
	Metadata *md = dObj->getMetadata()->getMetadata("Forward")->getMetadata("PRoPHET");

        string tmp = md->getContent();

        // Parse the metric string:
        parseMetricString(node_table, tmp);
	
        // Update the public metric DO:
        should_recalculate_metric_do = true;
}

void ForwarderProphet::_ageMetric(void)
{
	for (prophet_metric_table::iterator it = my_metrics.begin();
             it != my_metrics.end(); it++) {
                
		// Is this node a neighbor?
		NodeRef node = getKernel()->getNodeStore()->retrieve(id_number_to_nodeid[it->first]);
                
		if (node && !node->isNeighbor()) {
			// Update our private metric regarding this node:
			it->second *= PROPHET_AGING_CONSTANT;
			// Is this metric close to 0?
			if (it->second < 0.000001) {
				// Let's say it's 0:
				it->second = 0.0;
			}
		}
	}
	// Update the public metric DO:
	should_recalculate_metric_do = true;
}

void ForwarderProphet::_newNeighbor(NodeRef &neighbor)
{
	// We don't handle routing to anything but other haggle nodes:
	if (neighbor->getType() != NODE_TYPE_PEER)
		return;
	
	// Update our private metric regarding this node:
	prophet_forwarding_id neighbor_id = id_for_string(neighbor->getIdStr());
	
	// Remember that my_metrics[neighbor_id] is an log(n) operation - do it 
	// sparingly:
	double &value = my_metrics[neighbor_id];
	
	value = value + (1 - value) * PROPHET_INITIALIZATION_CONSTANT;

	// Update the public metric DO:
	should_recalculate_metric_do = true;
}

void ForwarderProphet::_endNeighbor(NodeRef &neighbor)
{
	// We don't handle routing to anything but other haggle nodes:
	if (neighbor->getType() != NODE_TYPE_PEER)
		return;
	
	// Update our private metric regarding this node:
	prophet_forwarding_id neighbor_id = id_for_string(neighbor->getIdStr());
	
	// Remember that my_metrics[neighbor_id] is an log(n) operation - do it 
	// sparingly:
	double &value = my_metrics[neighbor_id];
	
        value = value * PROPHET_AGING_CONSTANT;
        
        // Is this metric close to 0?
        if (value < 0.000001) {
                // Let's say it's 0:
                value = 0.0;
        }
	
	// Update the public metric DO:
	should_recalculate_metric_do = true;
}

void ForwarderProphet::_generateTargetsFor(NodeRef &neighbor)
{
	NodeRefList lst;
	// Figure out which forwarding table to look in:
	prophet_forwarding_id neighbor_id = id_for_string(neighbor->getIdStr());
	prophet_metric_table &the_table = forwarding_table[neighbor_id];
	
	// Go through the neighbor's forwarding table:
	for (prophet_metric_table::iterator it = the_table.begin();
		it != the_table.end();
		it++) {
		// Skip ourselves and that neighbor (if these accidentally ended up in 
		// that table)
		if (it->first != this_node && it->first != neighbor_id) {
			// Does the neighbor node have a better chance of forwarding to this
			// node than we do?
			if (it->second > my_metrics[it->first]) {
				// Yes: insert this node into the list of targets for this 
				// delegate forwarder.
				
				NodeRef new_node = new Node(id_number_to_nodeid[it->first].c_str(), NODE_TYPE_PEER, "PRoPHET target node");
                                
				if (new_node) {
					lst.push_back(new_node);
                                        HAGGLE_DBG("Neighbor '%s' is a good delegate for target '%s' [my_metric=%lf, neighbor_metric=%lf]\n", neighbor->getName().c_str(), new_node->getName().c_str(), my_metrics[it->first], it->second);
                                }
			}
		}
	}
	
	if (!lst.empty()) {
		kernel->addEvent(new Event(EVENT_TYPE_TARGET_NODES, neighbor, lst));
	}
}

void ForwarderProphet::_generateDelegatesFor(DataObjectRef &dObj, NodeRef &target)
{
	NodeRefList lst;
	// Figure out which node to look for:
	prophet_forwarding_id	target_id = id_for_string(target->getIdStr());
	// Store this value, since it is an O(log(n)) operation to do:
	double min_value = my_metrics[target_id];
	
	// Go through the neighbor's forwarding table:
	for (Map<prophet_forwarding_id, prophet_metric_table>::iterator it = 
			forwarding_table.begin();
		it != forwarding_table.end();
		it++) {
		// Exclude ourselves and the target node from the list of good delegate
		// forwarders:
		if (it->first != this_node && it->first != target_id) {
                        NodeRef new_node = new Node(id_number_to_nodeid[it->first].c_str(), NODE_TYPE_PEER, "PRoPHET delegate node");
			// Would this be a good delegate?
			if (min_value < it->second[target_id]) {
				// Yes: insert this node into the list of delegate forwarders 
				// for this target.
				
			
				if (new_node) {
					lst.push_back(new_node);
                                        HAGGLE_DBG("Node '%s' is a good delegate for target '%s' [my_metric=%lf, neighbor_metric=%lf]\n", new_node->getName().c_str(), target->getName().c_str(), min_value, it->second[target_id]);
                                } 
			} else {
                                HAGGLE_DBG("Node '%s' is NOT a good delegate for target '%s' [my_metric=%lf, neighbor_metric=%lf]\n", new_node->getName().c_str(), target->getName().c_str(), min_value, it->second[target_id]);
                        }
		}
	}
	
	if (!lst.empty()) {
		kernel->addEvent(new Event(EVENT_TYPE_DELEGATE_NODES, dObj, target, lst));
	} else {
                HAGGLE_DBG("No delegates found for target %s\n", 
                           target->getName().c_str());
        }
}

void ForwarderProphet::_getEncodedState(string *state)
{
	*state = "PRoPHET=" + getMetricString(my_metrics);
}

void ForwarderProphet::_setEncodedState(string *state)
{
	// Make sure this is a correct PRoPHET state string:
	if (strncmp("PRoPHET=", state->c_str(), 8) == 0) {
		string tmp = &(state->c_str()[8]);
		parseMetricString(my_metrics, tmp);
	}
}

#ifdef DEBUG
void ForwarderProphet::_printRoutingTable(void)
{
	printf("PRoPHET routing table:\n");
	
	for (Map<prophet_forwarding_id, string>::iterator it = 
			id_number_to_nodeid.begin();
		it != id_number_to_nodeid.end();
		it++) {
		printf("%ld: %s\n", it->first, it->second.c_str());
	}
	
	{
		printf("internal: {");
		prophet_metric_table::iterator jt = my_metrics.begin();
		while (jt != my_metrics.end()) {
			printf("%ld: %f", jt->first, jt->second);
			jt++;
			if (jt != my_metrics.end())
				printf(", ");
		}
		printf("}\n");
	}
	
	for (Map<prophet_forwarding_id, prophet_metric_table>::iterator it = 
			forwarding_table.begin();
		it != forwarding_table.end();
		it++) {
		printf("%ld: {", it->first);;
		prophet_metric_table::iterator jt = it->second.begin();
		while (jt != it->second.end()) {
			printf("%ld: %f", jt->first, jt->second);
			jt++;
			if (jt != it->second.end())
				printf(", ");
		}
		printf("}\n");
	}
}
#endif

