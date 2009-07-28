#include "MetadataParser.h"
#include "Trace.h"
#include <libcpphaggle/Pair.h>
#include <libcpphaggle/Exception.h>

unsigned int MetadataParser::num = 0;
MetadataParser::registry_t MetadataParser::registry;

MetadataParser::MetadataParser(const string _parsekey) : parsekey(_parsekey)
{ 
	Pair<registry_t::iterator, bool> ret;
	
	ret = registry.insert(make_pair(parsekey, this));
	
	if (!ret.second) {
#if HAVE_EXCEPTION
	 	throw Exception(-1, "Parser with given name already registered");
#else
                HAGGLE_ERR("Parser with given name '%s' already registered", parsekey.c_str());
#endif
	}

        //HAGGLE_DBG("Registered metadata parser with key \'%s\'\n", parsekey.c_str()); 
}

MetadataParser::~MetadataParser() 
{
        //HAGGLE_DBG("Unregistered metadata parser with key \'%s\'\n", parsekey.c_str()); 
	registry.erase(parsekey); 
}

MetadataParser *MetadataParser::getParser(string& key)
{
	registry_t::iterator it = registry.find(key);
	return (it == registry.end() ? NULL : (*it).second);
}
