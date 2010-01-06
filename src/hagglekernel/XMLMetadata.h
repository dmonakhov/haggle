#ifndef _XMLMETADATA_H
#define _XMLMETADATA_H

#include <libcpphaggle/Map.h>
#include <libcpphaggle/Pair.h>
#include <libxml/tree.h>

#include "Metadata.h"

using namespace haggle;

class XMLMetadata : public Metadata {
        xmlDocPtr doc; // Temporary pointer to doc that we are parsing
        char *initDoc(const char *raw, const size_t len);
        bool createXML(xmlNodePtr xn);
        xmlDocPtr createXMLDoc();
        bool parseXML(xmlNodePtr xn);
    public:
        XMLMetadata(const string name, const string content = "", XMLMetadata *parent = NULL);
        XMLMetadata(const XMLMetadata& m);
        XMLMetadata(const char *raw, const size_t len);
        ~XMLMetadata();
        XMLMetadata *copy() const;
        ssize_t getRaw(unsigned char *buf, size_t len);
        bool getRawAlloc(unsigned char **buf, size_t *len);
        bool addMetadata(Metadata *m);
        Metadata *addMetadata(const string name, const string content = "");
};

#endif /* _XMLMETADATA_H */
