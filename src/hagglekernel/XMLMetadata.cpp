#include "XMLMetadata.h"
#include "MetadataParser.h"

#include <string.h>

char *XMLMetadata::initDoc(const char *raw, size_t len)
{
        doc = xmlParseMemory(raw, len);
        
        if (!doc) {
                fprintf(stderr, "initDoc failed\n");
                return NULL;
        }
        return (char *)xmlDocGetRootElement(doc)->name;
}

XMLMetadata::XMLMetadata(const string name, const string content, XMLMetadata *parent) :
                Metadata(name, content, parent)
{
}

XMLMetadata::XMLMetadata(const XMLMetadata& m) :
                Metadata(m)
{
}
#if defined(OS_WINDOWS)
// Make sure MSVSC does not complain about passing this pointer in 
// base member initialization list
#pragma warning(disable : 4355)
#endif
XMLMetadata::XMLMetadata(const char *raw, size_t len) :
                Metadata(initDoc(raw, len), "", this)
{
        if (!doc) {
                return;
        }

        if (!parseXML(xmlDocGetRootElement(doc))) {
                fprintf(stderr, "Parse XML failed\n");
                xmlFreeDoc(doc);
                return;
        }
        
        xmlChar *content = xmlNodeGetContent(xmlDocGetRootElement(doc));
        
        if (content)
                setContent((char *)content);
        
        xmlFree(content);
        xmlFreeDoc(doc);
}

XMLMetadata::~XMLMetadata()
{
}

XMLMetadata *XMLMetadata::copy() const
{
        return new XMLMetadata(*this);
}

bool XMLMetadata::parseXML(xmlNodePtr xn)
{
        for (xmlAttrPtr xmlAttr = xn->properties; xmlAttr; xmlAttr = xmlAttr->next) {
                setParameter((char *)xmlAttr->name, (char *)xmlAttr->children->content);
        }
        for (xmlNodePtr xnc = xn->children; xnc; xnc = xnc->next) {
                if (xnc->type == XML_ELEMENT_NODE) {
                        XMLMetadata *m;
                        
                        if (xnc->children && 
                            xnc->children->type == XML_TEXT_NODE && 
                            !xmlIsBlankNode(xnc->children)) {
                                xmlChar *content = xmlNodeGetContent(xnc);
                                m = static_cast<XMLMetadata *>(addMetadata((char *)xnc->name, (char  *)content));
                                xmlFree(content);
                        } else {
                                m = static_cast<XMLMetadata *>(addMetadata((char *)xnc->name));
                        }

                        // Parse any children
                        if (!m || !m->parseXML(xnc))
                                return false;
                        
                } 
        }
#if defined(ENABLE_METADATAPARSER)
        MetadataParser *mp = MetadataParser::getParser(name);

        if (mp)
                return mp->onParseMetadata(this);
#endif
        return true;
}
bool XMLMetadata::addMetadata(Metadata *m)
{
        return _addMetadata(m);
}

Metadata *XMLMetadata::addMetadata(const string name, const string content)
{
        XMLMetadata *m = new XMLMetadata(name, content, this);

        if (!m)
                return NULL;

        //printf("XMLMetadata::addMetadata() adding metadata %s=%s\n", name.c_str(), content.c_str());
        if (!addMetadata(m)) {
                delete m;
                return NULL;
        }

        return m;
}

ssize_t XMLMetadata::getRaw(unsigned char *buf, size_t len)
{
	int xmlLen;
	xmlChar *xml;

	if (!buf)
		return -1;

	memset(buf, 0, len);

        doc = createXMLDoc();

        if (!doc)
                return false;

	xmlDocDumpFormatMemory(doc, &xml, &xmlLen, 1);

        if (xmlLen < 0)
                return -2;
        
	if ((unsigned int) xmlLen > len) {
		xmlFree(xml);
		return -3;
	}
        
	memcpy(buf, xml, xmlLen);

	xmlFree(xml);

	return xmlLen;
}

bool XMLMetadata::getRawAlloc(unsigned char **buf, size_t *len)
{
        int count;

	if (!buf || !len)
		return false;

	*len = 0;

        doc = createXMLDoc();

        if (!doc)
                return false;

	xmlDocDumpFormatMemory(doc, (xmlChar **) buf, &count, 1);
	
        xmlFreeDoc(doc);
        
	if (count <= 0)
                return false;
        
        *len = count;

        return true;
}

bool XMLMetadata::createXML(xmlNodePtr xn)
{
        if (!xn)
                return false;
        
        // Add parameters
        for (parameter_registry_t::iterator it = param_registry.begin(); it != param_registry.end(); it++) {
                if (!xmlNewProp(xn, (xmlChar *)(*it).first.c_str(), (xmlChar *)(*it).second.c_str()))
                        return false;
        }

        // Add recursively
        for (registry_t::iterator it = registry.begin(); it != registry.end(); it++) {
                XMLMetadata *m = static_cast<XMLMetadata *>((*it).second);
                
                if (!m->createXML(xmlNewChild(xn, NULL, (const xmlChar *) m->name.c_str(), m->content.length() != 0 ? (const xmlChar *)m->content.c_str() : NULL)))
                        return false;
        }
        return true;
}

xmlDocPtr XMLMetadata::createXMLDoc()
{
        doc = xmlNewDoc((xmlChar *)"1.0");

        if (!doc)
                return NULL;

        xmlNodePtr xn = xmlNewNode(NULL, (xmlChar *)name.c_str());
        
        if (!createXML(xn))
                goto out_err;

        xmlDocSetRootElement(doc, xn);

        return doc;
out_err:
        xmlFreeDoc(doc);
        return NULL;
}
