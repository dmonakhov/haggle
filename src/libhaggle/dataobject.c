/* Copyright 2008 Uppsala University
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "sha1.h"

#define DATAOBJECT_IN_MEMORY_NAME_PREFIX "mem-dObj-"

#define LIBHAGGLE_INTERNAL
#include <libhaggle/haggle.h>
#include <libhaggle/platform.h>
#include "debug.h"
#include "base64.h"
#include "sha1.h"
#include "metadata.h"

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#if HAVE_EXTRACTOR
#include <extractor.h>
#include <iconv.h>
#include <langinfo.h>

static char * iconvHelper(iconv_t cd,
			  const char * in) {
	size_t inSize;
	char * buf;
	char * ibuf;
	const char * i;
	size_t outSize;
	size_t outLeft;

	i = in;
	/* reset iconv */
	iconv(cd, NULL, NULL, NULL, NULL);

	inSize = strlen(in);
	outSize = 4 * strlen(in) + 2;
	outLeft = outSize - 2; /* make sure we have 2 0-terminations! */
	buf = malloc(outSize);
	ibuf = buf;
	memset(buf, 0, outSize);
	if (iconv(cd,
		  (char**) &in,
		  &inSize,
		  &ibuf,
		  &outLeft) == (size_t)-1) {
		/* conversion failed */
		free(buf);
		return strdup(i);
	}
	return buf;
}
#endif

#ifdef DEBUG
static unsigned long num_dobj_alloc = 0;
static unsigned long num_dobj_free = 0;
#endif

#if HAVE_EXTRACTOR

static int replace_spaces(char *str) 
{
	char *ptr = str;
	char c = ' ';
	int n = 0;

	for (;;) {
		ptr = strchr(ptr, c);

		if (!ptr)
			break;

		*ptr = '_';
		ptr++;
		n++;
	}
	return n;
}
#endif /* HAVE_EXTRACTOR */

static int dataobject_set_hash_str(struct dataobject *dobj);

unsigned short haggle_dataobject_set_flags(struct dataobject *dobj, unsigned short flags)
{
        dobj->flags |= flags;

        return dobj->flags;
}
unsigned short haggle_dataobject_unset_flags(struct dataobject *dobj, unsigned short flags)
{
        dobj->flags &= (flags ^ 0xffff);

        return dobj->flags;
}

int haggle_dataobject_set_createtime(struct dataobject *dobj, const struct timeval *createtime)
{
	if(dobj == NULL)
		return HAGGLE_PARAM_ERROR;

	if (createtime == NULL) {
#if defined(OS_LINUX) || defined(OS_MACOSX)
		struct timeval now;
		gettimeofday(&now, NULL);
		createtime = &now;
#else
		return HAGGLE_PARAM_ERROR;
#endif
	}
	
	dobj->createtime.tv_sec = createtime->tv_sec;
	dobj->createtime.tv_usec = createtime->tv_usec;

	return HAGGLE_NO_ERROR;
}

struct dataobject *haggle_dataobject_new()
{
	return haggle_dataobject_new_from_raw(NULL, 0);
}

struct dataobject *haggle_dataobject_new_from_raw(const char *raw, const size_t len)
{
	struct dataobject *dobj;
        const char *persistent;
        const char *createtime;
        metadata_t *md;

	dobj = (struct dataobject *)malloc(sizeof(struct dataobject));

	if (!dobj) 
		return NULL;

	memset(dobj, 0, sizeof(struct dataobject));

	haggle_dataobject_set_flags(dobj, DATAOBJECT_FLAG_PERSISTENT);

	dobj->al = haggle_attributelist_new();

	if (!dobj->al) {
		haggle_dataobject_free(dobj);
		return NULL;
	}
 
	if (raw) {
		metadata_t *ma;
		
		dobj->m = metadata_new_from_raw(raw, len);

		if (!dobj->m) {
			haggle_dataobject_free(dobj);
			return NULL;
		}

		persistent = metadata_get_parameter(dobj->m, DATAOBJECT_PERSISTENT_PARAM);

		if (persistent && strcmp(persistent, "no") == 0) {
			haggle_dataobject_unset_flags(dobj, DATAOBJECT_FLAG_PERSISTENT);
		}
		
		createtime = metadata_get_parameter(dobj->m, DATAOBJECT_CREATE_TIME_PARAM);

		if (createtime) {
			long tv_sec, tv_usec;
			struct timeval ct;
			
			sscanf(createtime, "%ld.%ld", &tv_sec, &tv_usec);
			ct.tv_sec = tv_sec;
			ct.tv_usec = tv_usec;
			haggle_dataobject_set_createtime(dobj, &ct);
		}
		
		md = metadata_get(dobj->m, DATAOBJECT_METADATA_DATA);

		if (md) {
			metadata_t *mc;
			const char *str = metadata_get_parameter(md, DATAOBJECT_METADATA_DATA_DATALEN_PARAM);

			if (str)
				dobj->datalen = atoi(str);

			mc = metadata_get(md, NULL);
			
			while (mc) {
				
				if (metadata_name_is(mc, DATAOBJECT_METADATA_DATA_FILEPATH)) {
					haggle_dataobject_set_filepath(dobj, metadata_get_content(mc));
				} else if (metadata_name_is(mc, DATAOBJECT_METADATA_DATA_FILENAME)) {
					haggle_dataobject_set_filename(dobj, metadata_get_content(mc));
				} else if (metadata_name_is(mc, DATAOBJECT_METADATA_DATA_FILEHASH)) {
					struct base64_decode_context ctx;
					size_t len = HASH_LENGTH;
					base64_decode_ctx_init(&ctx);

					if (base64_decode(&ctx, metadata_get_content(mc), 
						strlen(metadata_get_content(mc)), 
						(char *)dobj->hash, &len)) {
							dataobject_set_hash_str(dobj);
					}
				}

				mc = metadata_get_next(md);
			}
		}

		ma = metadata_get(dobj->m, DATAOBJECT_METADATA_ATTRIBUTE);

		while (ma) {
			const char *name = metadata_get_parameter(ma, DATAOBJECT_METADATA_ATTRIBUTE_NAME_PARAM);
			const char *weightstr = metadata_get_parameter(ma, DATAOBJECT_METADATA_ATTRIBUTE_WEIGHT_PARAM);
			haggle_attr_t *a;

			if (name) {
				if (weightstr)
					a = haggle_attribute_new_weighted(name, metadata_get_content(ma), strtoul(weightstr, NULL, 10));
				else
					a = haggle_attribute_new(name, metadata_get_content(ma));

				if (a)
					haggle_attributelist_add_attribute(dobj->al, a);
			}
			//LIBHAGGLE_DBG("it: %s:%s\n", metadata_get_name(mc), metadata_get_content(mc));
			ma = metadata_get_next(dobj->m);
		}
	}

        
#ifdef DEBUG
	num_dobj_alloc++;
        dobj->num = num_dobj_alloc;
        LIBHAGGLE_DBG("Allocating data object num=%lu\n", dobj->num);
#endif
	return dobj;
}
struct dataobject *haggle_dataobject_new_from_file(const char *filepath)
{
        FILE *fp;
	struct dataobject *dobj;
        size_t datalen;

	if (!filepath || strlen(filepath) == 0)
		return NULL;
	
        fp = fopen(filepath, "r");

        if (!fp) {
                LIBHAGGLE_DBG("Could not stat file %s\n", filepath);
                return NULL;
        }

        fseek(fp, 0L, SEEK_END);
        datalen = ftell(fp);
        fclose(fp);

	dobj = (struct dataobject *)malloc(sizeof(struct dataobject));

	if (!dobj) 
		return NULL;

	memset(dobj, 0, sizeof(struct dataobject));
	
	haggle_dataobject_set_flags(dobj, DATAOBJECT_FLAG_PERSISTENT);
	
	dobj->datalen = datalen;
        
	dobj->al = haggle_attributelist_new();

	if (!dobj->al) {
		haggle_dataobject_free(dobj);
		return NULL;
	}
		
	dobj->filepath = (char *)malloc(strlen(filepath) + 1);

	if (!dobj->filepath) {
		haggle_dataobject_free(dobj);
		return NULL;
	}
	strcpy(dobj->filepath, filepath);

#if HAVE_EXTRACTOR
	do {
		EXTRACTOR_ExtractorList *plugins;
		EXTRACTOR_KeywordList *keywords;
		iconv_t iconv;
		
		plugins = EXTRACTOR_loadDefaultLibraries();
		keywords = EXTRACTOR_getKeywords(plugins, filepath);

		EXTRACTOR_removeEmptyKeywords(keywords);
		/* EXTRACTOR_removeDuplicateKeyword(keywords, 0); */
		iconv = iconv_open(nl_langinfo(CODESET), "UTF-8");

		while (keywords) {
			char *name = iconvHelper(iconv, EXTRACTOR_getKeywordTypeAsString(keywords->keywordType));

			if (!name)
				continue;

			char *value = iconvHelper(iconv, keywords->keyword);

			if (!value) {
				free(name);
				continue;
			}

			replace_spaces(name);

			haggle_dataobject_add_attribute(dobj, name, value);

			free(name);
			free(value);

			keywords = keywords->next;
		}
		
		EXTRACTOR_freeKeywords(keywords);
		EXTRACTOR_removeAll(plugins); /* unload plugins */
	} while(0);
#endif

#ifdef DEBUG
	num_dobj_alloc++;
        dobj->num = num_dobj_alloc;
        LIBHAGGLE_DBG("Allocating data object num=%lu\n", dobj->num);
#endif
	return dobj;
}

struct dataobject *haggle_dataobject_new_from_buffer(const unsigned char *data, const size_t len)
{
#define FILEPATH_LEN 256
	char filepath[FILEPATH_LEN];
	long i;
	FILE *fp;
	struct dataobject *retval;
	
	// Make sure we don't try to write to (null)/...
	if (!haggle_directory)
		return NULL;
	
	// Find a file name that does not exist:
	i = 0;
	do {
		int len = snprintf(filepath, FILEPATH_LEN, "%s%s" DATAOBJECT_IN_MEMORY_NAME_PREFIX "%ld.do", 
				   haggle_directory, PLATFORM_PATH_DELIMITER, i);
		
		if (len < 0) {
			return NULL;
		}
		i++;
		
		fp = fopen(filepath, "r");
		
		// Does it exist?
		if (fp != NULL) {
			// Close it. Do not se the fp to NULL, since the while loop uses 
			// that to check if the file existed.
			fclose(fp);
		}
	} while (fp != NULL);
	
	// Open the file:
	fp = fopen(filepath, "w");
	
	// Write the data to it:
	if (fwrite(data, len, 1, fp) != 1) {
		// Error while writing.
		
		// Close the file:
		fclose(fp);
		
		// FIXME: should really delete the file here!
		
		// Fail:
		return NULL;
	}
	// Close the file:
	fclose(fp);
	
	// Use this function to do the work of creating the dobj.
	retval = haggle_dataobject_new_from_file(filepath);

	if (retval == NULL) {
		// FIXME: delete the file, perhaps?
	}
	return retval;
}

void haggle_dataobject_free(struct dataobject *dobj)
{
	if (!dobj)
		return;
	
#ifdef DEBUG
        LIBHAGGLE_DBG("Freeing data object num=%lu\n", dobj->num);
	num_dobj_free++;
#endif
	if (dobj->fp)
		haggle_dataobject_read_data_stop(dobj);
	
	if (dobj->filename)
		free(dobj->filename);

	if (dobj->filepath)
		free(dobj->filepath);

	if (dobj->al)
		haggle_attributelist_free(dobj->al);

	if (dobj->raw)
		free(dobj->raw);

        if (dobj->hash_str)
                free(dobj->hash_str);

	if (dobj->thumbnail_str)
		free(dobj->thumbnail_str);

        if (dobj->m)
                metadata_free(dobj->m);

	free(dobj);
	
	dobj = NULL;
}

/*
	This function must work in the exact same way as the corresponding calcId()-function
	in the Haggle kernel's code (DataObject.cpp)
*/
int haggle_dataobject_calculate_id(const struct dataobject *dobj, dataobject_id_t *id)
{
	SHA1_CTX ctxt;
        
        SHA1_Init(&ctxt);
	
	if (dobj->al) {
		list_t *pos;

		list_for_each(pos, &dobj->al->attributes) {
			haggle_attr_t *a = (haggle_attr_t *)pos;
			unsigned long w;

			SHA1_Update(&ctxt, (unsigned char *)haggle_attribute_get_name(a), strlen(haggle_attribute_get_name(a)));
			SHA1_Update(&ctxt, (unsigned char *)haggle_attribute_get_value(a), strlen(haggle_attribute_get_value(a)));

			w = htonl(haggle_attribute_get_weight(a));
			SHA1_Update(&ctxt, (unsigned char *)&w, sizeof(w));
		}
	}
	// FIXME: If this data object has a create time, add that to the ID.
	

	/*
	 If the data object has associated data, we add the data's file hash.
	 If the data is a file, but there is no hash, then we instead use
	 the filename and data length.
	*/ 
	if (dobj->hash_str) {
		SHA1_Update(&ctxt, (unsigned char *)dobj->hash, HASH_LENGTH);
	} else if (dobj->filename && dobj->datalen > 0) {
		SHA1_Update(&ctxt, (unsigned char *)dobj->filename, strlen(dobj->filename));
		SHA1_Update(&ctxt, (unsigned char *)&dobj->datalen, sizeof(dobj->datalen));
	}

	// Create the final ID hash value:
        SHA1_Final(*id, &ctxt);

#if DEBUG
	if (1) {
		char idStr[2*sizeof(dataobject_id_t)+1];
		unsigned int i, len = 0;

		// Generate a readable string of the Id
		for (i = 0; i < sizeof(dataobject_id_t); i++) {
			len += sprintf(idStr + len, "%02x", *id[i] & 0xff);
		}

		LIBHAGGLE_DBG("data object id to delete: %s\n", idStr);
	}
#endif
	return 0;
}

metadata_t *haggle_dataobject_to_metadata(struct dataobject *dobj)
{
        char *persistent = NULL;
        metadata_t *md;
        list_t *pos;
        
	if (!dobj)
		return NULL;
        
        if (dobj->flags & DATAOBJECT_FLAG_PERSISTENT)
                persistent = "yes";
        else
                persistent = "no";

        if (dobj->m)
                metadata_free(dobj->m);

        dobj->m = metadata_new(HAGGLE_TAG, NULL, NULL);

        if (!dobj->m) {
                LIBHAGGLE_ERR("failed to create new metadata\n");
                return NULL;
        }

        metadata_set_parameter(dobj->m, DATAOBJECT_PERSISTENT_PARAM, persistent);
	
	if (dobj->createtime.tv_sec != 0) {
		char createtime[32];
		long sec, usec;
		
		sec = dobj->createtime.tv_sec;
		usec = dobj->createtime.tv_usec;
		
		sprintf(createtime, "%ld.%06ld", sec, usec);
		metadata_set_parameter(dobj->m, DATAOBJECT_CREATE_TIME_PARAM, createtime);
	}
	
	
        if (dobj->filepath || dobj->datalen > 0) {
                char datalenstr[20];
                md = metadata_new(DATAOBJECT_METADATA_DATA, NULL, dobj->m);

                if (!md) {
                        LIBHAGGLE_ERR("failed to create new child metadata\n");
                        metadata_free(dobj->m);
                        return NULL;
                }
                metadata_add(dobj->m, md);
#if defined(OS_WINDOWS)
                snprintf(datalenstr, 20, "%u", dobj->datalen);
#else
                snprintf(datalenstr, 20, "%zu", dobj->datalen);
#endif
                metadata_set_parameter(md, DATAOBJECT_METADATA_DATA_DATALEN_PARAM, datalenstr);
                
                if (dobj->filename) {
                        metadata_add(md, metadata_new(DATAOBJECT_METADATA_DATA_FILENAME, dobj->filename, md));
                }

		if (dobj->hash_str) {
			char base64_hash[BASE64_LENGTH(SHA1_DIGEST_LENGTH) + 1];
			memset(base64_hash, '\0', BASE64_LENGTH(SHA1_DIGEST_LENGTH) + 1);

			base64_encode((char *)dobj->hash, SHA1_DIGEST_LENGTH, base64_hash, BASE64_LENGTH(SHA1_DIGEST_LENGTH) + 1);
			metadata_add(md, metadata_new(DATAOBJECT_METADATA_DATA_FILEHASH, base64_hash, md));
			LIBHAGGLE_DBG("Added file hash in base64 %s\n", base64_hash);
		}

		if (dobj->thumbnail_str) {
			metadata_add(md, metadata_new(DATAOBJECT_METADATA_DATA_THUMBNAIL, dobj->thumbnail_str, md));
		}
                if (dobj->filepath)
                        metadata_add(md, metadata_new(DATAOBJECT_METADATA_DATA_FILEPATH, dobj->filepath, md));
        }


        list_for_each(pos, &dobj->al->attributes) {
                haggle_attr_t *a = (haggle_attr_t *)pos;
                metadata_t *am = metadata_new(DATAOBJECT_METADATA_ATTRIBUTE, haggle_attribute_get_value(a), dobj->m);

                if (am) {
					metadata_set_parameter(am, DATAOBJECT_METADATA_ATTRIBUTE_NAME_PARAM, haggle_attribute_get_name(a));
					if (haggle_attribute_get_weight(a)) {
						char str_weight[8];
						sprintf(str_weight, "%ld", haggle_attribute_get_weight(a));
						metadata_set_parameter(am, DATAOBJECT_METADATA_ATTRIBUTE_WEIGHT_PARAM, str_weight);
					}
					metadata_add(dobj->m, am);
                }
        }

        return dobj->m;
}

char *haggle_dataobject_get_raw(struct dataobject *dobj)
{
        metadata_t *m;

        if (!dobj)
                return NULL;

        m = haggle_dataobject_to_metadata(dobj);
        
        if (!m)
                return NULL;
        
	if (dobj->raw)
		free(dobj->raw);

        metadata_get_raw_alloc(m, &dobj->raw, (size_t *)&dobj->raw_len);

	if (dobj->raw_len <= 0)
                return NULL;
        
	return dobj->raw;
}

int haggle_dataobject_get_raw_alloc(struct dataobject *dobj, char **buf, size_t *len)
{
        metadata_t *m;
        int raw_len, ret;

        if (!dobj)
                return -1;

        m = haggle_dataobject_to_metadata(dobj);
        
        if (!m)
                return -1;
        
        ret = metadata_get_raw_alloc(m, buf, (size_t *)&raw_len);

	if (ret < 0)
                return -1;

        *len = raw_len;
        
	return raw_len;
}


size_t haggle_dataobject_get_raw_length(const struct dataobject *dobj)
{
	return (dobj ? dobj->raw_len : 0);
}

const char *haggle_dataobject_get_filename(const struct dataobject *dobj)
{
	return (dobj ? dobj->filename : NULL);
}

const char *haggle_dataobject_get_filepath(const struct dataobject *dobj)
{
	return (dobj ? dobj->filepath : NULL);
}

const char *haggle_dataobject_set_filename(struct dataobject *dobj, const char *filename)
{
        char *tmp;

        if (!dobj)
                return NULL;

        tmp = (char *)malloc(strlen(filename) + 1);
        
        if (!tmp)
                return NULL;
        
        if (dobj->filename)
                free(dobj->filename);

        dobj->filename = tmp;

        strcpy(dobj->filename, filename);

        return dobj->filename;        
}

const char *haggle_dataobject_set_filepath(struct dataobject *dobj, const char *filepath)
{
        char *tmp;

        if (!dobj)
                return NULL;

        tmp = (char *)malloc(strlen(filepath) + 1);
        
        if (!tmp)
                return NULL;
        
        if (dobj->filepath)
                free(dobj->filepath);

        dobj->filepath = tmp;

        strcpy(dobj->filepath, filepath);

        return dobj->filepath;        
}


int haggle_dataobject_get_data_size(const struct dataobject *dobj, size_t *count)
{
	FILE *fp;
	long retval = 0;

	if (!dobj)
		return HAGGLE_DATAOBJECT_ERROR;
	
	if (!dobj->filepath)
		return HAGGLE_FILE_ERROR;

	if (!count)
		return HAGGLE_PARAM_ERROR;

	*count = 0;

	fp = fopen(dobj->filepath, "r");
			
	if (fp == NULL)
		return HAGGLE_FILE_ERROR;
		       
	/* fseek returns -1 on error */
	retval = fseek(fp, 0L, SEEK_END);

	if (retval != 0) {
		fclose(fp);
		return HAGGLE_FILE_ERROR;
	}

	/* ftell returns -1 on error */
	retval = ftell(fp);

	if (retval == -1) {
		fclose(fp);
		return HAGGLE_FILE_ERROR;
	}
	
	*count = retval;

	fclose(fp);

	return 0;
}

int haggle_dataobject_read_data_start(struct dataobject *dobj)
{
	if (!dobj)
		return HAGGLE_DATAOBJECT_ERROR;
					
	if (!dobj->filepath)
		return HAGGLE_FILE_ERROR;
				  	
	dobj->fp = fopen(dobj->filepath, "r");
				  
	if (!dobj->fp)
		return HAGGLE_FILE_ERROR;
				  
	return 0;
}

ssize_t haggle_dataobject_read_data(struct dataobject *dobj, void *buffer, size_t count)
{
	ssize_t nbytes;

	if (!dobj)
		return HAGGLE_DATAOBJECT_ERROR;
	
	if (!dobj->fp)
		return HAGGLE_FILE_ERROR;

	if (!buffer)
		return HAGGLE_PARAM_ERROR;

	nbytes = fread(buffer, 1, count, dobj->fp);
	
	if (ferror(dobj->fp) != 0) {
		return HAGGLE_FILE_ERROR;
	} else if (feof(dobj->fp) != 0) {
		fclose(dobj->fp);
		dobj->fp = NULL;
	}

	return nbytes;
}

int haggle_dataobject_read_data_stop(struct dataobject *dobj)
{
	if (!dobj)
		return HAGGLE_DATAOBJECT_ERROR;

	if (!dobj->fp)
		return 0;
		
	fclose(dobj->fp);
	dobj->fp = NULL;

	return 1;
}

void *haggle_dataobject_get_data_all(struct dataobject *dobj)
{
	FILE *fp;
	void *retval;
        long readlen;
	size_t len;
	
	/* Check that the data object is ok: */
	if (!dobj)
		goto fail_dobj;
	
	/* Check that there is a file path: */
	if (!dobj->filepath)
		goto fail_dobj_filepath;
	
	/* Open the file: */
	fp = fopen(dobj->filepath, "r");
	
	/* Check that that worked: */
	if (fp == NULL)
		goto fail_open;
	
	/* Go to the end of the file: */
	if (fseek(fp, 0L, SEEK_END) != 0)
		goto fail_fseek;
	
	/* Figure out how long the file is: */
	readlen = ftell(fp);
	
	if (readlen == -1)
		goto fail_size;
	
	/* Go to the beginning of the file: */
	if (fseek(fp, 0L, SEEK_SET) != 0)
		goto fail_fseek;
	
	/* Allocate the needed memory: */
	retval = malloc(readlen);

	/* Check that it went well: */
	if (retval == NULL)
		goto fail_retval;
	
        len = readlen;

	/* Read the data & check if it worked: */
	if (len != fread(retval, 1, readlen, fp))
		goto fail_read;
	
	/* Close the file: */
	fclose(fp);
	
	/* Done! */
	return retval;
	
fail_read:
	/* Free the pointer, to avoid leaking memory: */
	free(retval);
fail_retval:
fail_size:
fail_fseek:
	/* Close the file: */
	fclose(fp);
fail_open:
fail_dobj_filepath:
fail_dobj:
	return NULL;
}

unsigned long haggle_dataobject_get_num_attributes(const struct dataobject *dobj)
{
	return haggle_attributelist_size(dobj->al);
}

struct attribute *haggle_dataobject_get_attribute_n(struct dataobject *dobj, const unsigned long n)
{
	return dobj ? haggle_attributelist_get_attribute_n(dobj->al, n) : NULL;
}

struct attribute *haggle_dataobject_get_attribute_by_name(struct dataobject *dobj, const char *name)
{	
	return dobj ? haggle_attributelist_get_attribute_by_name(dobj->al, name) : NULL;
}

struct attribute *haggle_dataobject_get_attribute_by_name_value(struct dataobject *dobj, const char *name, const char *value)
{	
	return dobj ? haggle_attributelist_get_attribute_by_name_value(dobj->al, name, value) : NULL;
}

struct attribute *haggle_dataobject_get_attribute_by_name_n(struct dataobject *dobj, const char *name, const unsigned long n)
{
	return dobj ? haggle_attributelist_get_attribute_by_name_n(dobj->al, name, n) : NULL;
}

struct attributelist *haggle_dataobject_get_attributelist(struct dataobject *dobj)
{
	return dobj ? dobj->al : NULL;
}

int haggle_dataobject_remove_attribute(struct dataobject *dobj, struct attribute *a)
{
	if (haggle_attributelist_detach_attribute(dobj->al, a) == 1) {
		return 1;
	}
	return 0;
}

int haggle_dataobject_remove_attribute_by_name_value(struct dataobject *dobj, const char *name, const char *value)
{
	struct attribute *a;
	
	a = haggle_attributelist_remove_attribute(dobj->al, name, value);

	if (a) {
		haggle_attribute_free(a);
		return 1;
	}

	return 0;
}

int dataobject_set_hash_str(struct dataobject *dobj)
{
	int i;

	if (!dobj)
		return HAGGLE_PARAM_ERROR;

	if (!dobj->hash_str) {
		dobj->hash_str = (char *)malloc(HASH_LENGTH*2 + 1);

		if (!dobj->hash_str) {
			return HAGGLE_ALLOC_ERROR;
		}
	}

	/* Convert to readable format: */
	for (i = 0; i < HASH_LENGTH; i++) {
		int len = sprintf(&(dobj->hash_str[i*2]), "%02hhX", dobj->hash[i]);

		if (len < 0) {
			free(dobj->hash_str);
			dobj->hash_str = NULL;
			return HAGGLE_INTERNAL_ERROR;
		}
	}

	return HAGGLE_NO_ERROR;
}

int haggle_dataobject_add_hash(struct dataobject *dobj)
{
	SHA1_CTX ctx;
	FILE *fp;
#define DATA_BUFFER_LEN 4096
	unsigned char data[DATA_BUFFER_LEN];
	size_t read_bytes;
	
	/* Check that the data object exists and has a filepath */
	if (!dobj || !dobj->filepath)
		return HAGGLE_INTERNAL_ERROR;
	
	LIBHAGGLE_DBG("Adding file hash to file %s\n", dobj->filepath);

	/* Open the file: */
	fp = fopen(dobj->filepath, "rb");
	
	if (fp == NULL)
		return HAGGLE_FILE_ERROR;
	
	/* Initialize the SHA1 hash context */
	SHA1_Init(&ctx);
	
	/* Go through the file until there is no more, or there was
	   an error: */
	while (!(feof(fp) || ferror(fp))) {
		/* Read up to DATA_BUFFER_LEN bytes more: */
		read_bytes = fread(data, 1, DATA_BUFFER_LEN, fp);
		
		/* Insert them into the hash: */
		SHA1_Update(&ctx, data, read_bytes);
	}
	
	/* Was there an error? */
	if (ferror(fp) != 0) {
		fclose(fp);
		return HAGGLE_FILE_ERROR;
	}
	
	fclose(fp);
	
	/* No? Finalize the hash: */
	SHA1_Final(dobj->hash, &ctx);
		
	dataobject_set_hash_str(dobj);
        
	LIBHAGGLE_DBG("File hash is %s\n", dobj->hash_str);

        return HAGGLE_NO_ERROR;
}

int haggle_dataobject_set_thumbnail(struct dataobject *dobj, char *data, size_t len)
{
	char *b64;
	size_t b64_len;
	
        if (!dobj)
                return HAGGLE_PARAM_ERROR;

	b64_len = BASE64_LENGTH(len);
	b64 = malloc(b64_len + 1);
        
	if (b64 == NULL)
		return HAGGLE_INTERNAL_ERROR;
	
	memset(b64, '\0', b64_len + 1);
	
	base64_encode(data, len, b64, b64_len + 1);
	
	if (dobj->thumbnail_str != NULL)
		free(dobj->thumbnail_str);

	dobj->thumbnail_str = b64;
	
	return HAGGLE_NO_ERROR;
}

ssize_t haggle_dataobject_read_thumbnail(const struct dataobject *dobj, char *data, size_t len)
{
        struct base64_decode_context ctx;

        if (!dobj || !dobj->thumbnail_str)
                return HAGGLE_PARAM_ERROR;

        base64_decode_ctx_init(&ctx);

        if (!base64_decode(&ctx, dobj->thumbnail_str, 
                          strlen(dobj->thumbnail_str), 
                          data, &len)) {
                return HAGGLE_INTERNAL_ERROR;
        }

        return len;
}

int haggle_dataobject_get_thumbnail_size(const struct dataobject *dobj, size_t *bytes)
{
        if (!dobj || !dobj->thumbnail_str)
                return HAGGLE_PARAM_ERROR;

        /* This may allocate a bit too much space, but it is too
         * costly to calculate the exact size. See base64_decode_alloc
         * in base64.c for an explanation.
         */
        *bytes = 3 * (strlen(dobj->thumbnail_str) / 4) + 2;

        return HAGGLE_NO_ERROR;
}

int haggle_dataobject_add_attribute(struct dataobject *dobj, const char *name, const char *value)
{
	return haggle_dataobject_add_attribute_weighted(dobj, name, value, 1);
}

int haggle_dataobject_add_attribute_weighted(struct dataobject *dobj, const char *name, const char *value, const unsigned long weight)
{
	struct attribute *attr;

	attr = haggle_attribute_new_weighted(name, value, weight);

	if (!attr) {
		LIBHAGGLE_DBG("Could not add attribute %s:%s\n", name, value);
		return HAGGLE_INTERNAL_ERROR;
	}
	return haggle_attributelist_add_attribute(dobj->al, attr);
}

#ifdef DEBUG
void haggle_dataobject_leak_report_print()
{
	LIBHAGGLE_DBG("\n"
		"=== libhaggle data object leak report begin ===\n"
		"num alloc %lu\n"
		"num free  %lu\n"
		"=== libhaggle data object leak report end ===\n", 
		num_dobj_alloc, 
		num_dobj_free);	
}

void haggle_dataobject_print_attributes(struct dataobject *dobj)
{
	if (!dobj || !dobj->al) {
		LIBHAGGLE_ERR("not a valid data object or attribute list\n");
		return;
	}
	haggle_attributelist_print(dobj->al);
}
#endif
