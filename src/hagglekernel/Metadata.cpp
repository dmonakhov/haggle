#include "Metadata.h"

Metadata::Metadata(const string _name, const string _content, Metadata *_parent) :
                parent(_parent), name(_name), content(_content)
{
}

Metadata::Metadata(const Metadata& m) : 
                parent(m.parent), name(m.name), content(m.content), 
                param_registry(m.param_registry), 
                registry()
{
        for (registry_t::const_iterator it = m.registry.begin(); it != m.registry.end(); it++) {
                const Metadata *m = (*it).second;
                Metadata *mcopy = m->copy();
                mcopy->parent = this;
                registry.insert(make_pair(m->name, mcopy));
        }
}

Metadata::~Metadata()
{
        while (!registry.empty()) {
                registry_t::iterator it = registry.begin();
                Metadata *m = (*it).second;
                registry.erase(it);
                delete m;                
        }
}

bool Metadata::_addMetadata(Metadata *m)
{ 
        if (!m)
                return false;

        registry.insert(make_pair(m->name, m));

        return true;
}


bool Metadata::removeMetadata(const string name)
{
        bool ret = false;
        
        r = registry.equal_range(name);

         while (r.first != r.second) {
                 registry_t::iterator it = r.first;
                 Metadata *m = (*it).second;
                 r.first++;
                 registry.erase(it);
                 delete m;
                 ret = true;
         }
         return ret;
}

Metadata *Metadata::getMetadata(const string name, unsigned int n)
{
        r = registry.equal_range(name);

        if (r.first == r.second)
                return NULL;
        
        while (n) { 
                r.first++;
                n--;
                
                if (r.first == r.second)
                        return NULL;
        }
        return (*r.first).second;
}

Metadata *Metadata::getNextMetadata()
{
        if (r.first == registry.end() || r.first == r.second)
                return NULL;
        
        r.first++;

        if (r.first == r.second)
                return NULL;

        return (*r.first).second;
}

string& Metadata::setParameter(const string name, const string value)
{
        Pair<parameter_registry_t::iterator, bool> p;

        p = param_registry.insert(make_pair(name, value));

        if (!p.second) {
                // Update value
                (*p.first).second = value;
        }
        return (*p.first).second;
}

string& Metadata::setParameter(const string name, const unsigned int value)
{
	char tmp[32];
	sprintf(tmp, "%u", value);
	return setParameter(name,tmp);
}

bool Metadata::removeParameter(const string name)
{
        return param_registry.erase(name) == 1;
}

const char *Metadata::getParameter(const string name)
{
        parameter_registry_t::iterator it;

        it = param_registry.find(name);

        if (it == param_registry.end())
                return NULL;
        
        return (*it).second.c_str();
}


string& Metadata::setContent(const string _content)
{
        content = _content;
        return content;
}

const string& Metadata::getContent() const
{
        return content;
}
