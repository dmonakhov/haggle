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
#include <string.h>
#include <math.h>       
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"
#include "base64.h"
#include "bloomfilter.h"
#include <openssl/sha.h>

static int bloomfilter_calculate_length(unsigned int num_keys, double error_rate, unsigned  int *lowest_m, unsigned int *best_k);

struct bloomfilter *bloomfilter_new(float error_rate, unsigned int capacity)
{
	struct bloomfilter *bf;
	unsigned int m, k, i;
	int bflen;
	salt_t *salts;
	struct timeval tv;

	bloomfilter_calculate_length(capacity, error_rate, &m, &k);

	bflen = k*SALT_SIZE + m*BIN_BITS/8;

	bf = (struct bloomfilter *)malloc(sizeof(struct bloomfilter) + bflen);

	if (!bf)
		return NULL;

	memset(bf, 0, sizeof(struct bloomfilter) + bflen);

	bf->m = m;
	bf->k = k;
	bf->n = 0;
	
	salts = BLOOMFILTER_GET_SALTS(bf);

	// Seed the rand() function's state. rand() should probably be replaced
	// by prng_uint8() or prnguint32(), but I don't know if there would be any
	// bad effects of doing that.	
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);
	
	/* Create salts for hash functions */
	for (i = 0; i < k; i++) {
		salts[i] = (salt_t)rand();
	}	
	return bf;
}

struct bloomfilter *bloomfilter_copy(const struct bloomfilter *bf)
{
	struct bloomfilter *bf_copy;

	if (!bf)
		return NULL;

	bf_copy = (struct bloomfilter *)malloc(BLOOMFILTER_TOT_LEN(bf));

	if (!bf_copy)
		return NULL;

	memcpy(bf_copy, bf, BLOOMFILTER_TOT_LEN(bf));

	return bf_copy;
}

void bloomfilter_print(struct bloomfilter *bf)
{
	unsigned int i;
	
	if (!bf)
		return;

	printf("Bloomfilter m=%u k=%u n=%u\n", bf->m, bf->k, bf->n);

#ifdef COUNTING_BLOOMFILTER	
	for (i = 0; i < bf->m; i++) {
		bint_t *bins = BLOOMFILTER_GET_FILTER(bf);
		printf("%u", bins[i]);
	}
#else
	for (i = 0; i < bf->m / 8; i++) {
		bin_t *bins = BLOOMFILTER_GET_FILTER(bf);

		printf("%d", bins[i] & 0x01 ? 1 : 0);
		printf("%d", bins[i] & 0x02 ? 1 : 0);
		printf("%d", bins[i] & 0x04 ? 1 : 0);
		printf("%d", bins[i] & 0x08 ? 1 : 0);
		printf("%d", bins[i] & 0x10 ? 1 : 0);
		printf("%d", bins[i] & 0x20 ? 1 : 0);
		printf("%d", bins[i] & 0x40 ? 1 : 0);
		printf("%d", bins[i] & 0x80 ? 1 : 0);
	}
#endif
	printf("\n");
}

/* This function converts the bloomfilter byte-by-byte into a
 * hexadecimal string. NOTE: It cannot be used as an encoding over the
 * network, since the integer fields are currently not converted to
 * network byte order. */
char *bloomfilter_to_str(struct bloomfilter *bf)
{
	char *str;
	char *ptr;
	unsigned int i;
	int len = 0;

	if (!bf)
		return NULL;

	//printf("bf len is %d K_SIZE=%ld M_SIZE=%ld N_SIZE=%ld SALTS_LEN=%ld FILTER_LEN=%u\n", len, K_SIZE, M_SIZE, N_SIZE, BLOOMFILTER_SALTS_LEN(bf), FILTER_LEN(bf));

	str = (char *)malloc(BLOOMFILTER_TOT_LEN(bf) + 1);

	if (!str)
		return NULL;

	ptr = (char *)bf;
	
	/* First, format the 'k', 'm' and 'n' into the string */
	for (i = 0; i < BLOOMFILTER_TOT_LEN(bf); i++)
		len += sprintf(str+len, "%02x", ptr[i] & 0xff);

	return str;
}

char *bloomfilter_to_base64(const struct bloomfilter *bf)
{
	char *b64str;
	int len = 0;
	unsigned int i = 0;
	struct bloomfilter *bf_net;
	salt_t *salts, *salts_net;
	u_int16_t *bins, *bins_net;
	
	if (!bf)
		return NULL;

	bf_net = (struct bloomfilter *)malloc(BLOOMFILTER_TOT_LEN(bf));

	if (!bf_net)
		return NULL;

	/* First set the values in host byte order so that we can get
	the pointers to the salts and the filter */
	bf_net->k = bf->k;
	bf_net->m = bf->m;
	bf_net->n = bf->n;
	
	/* Get pointers */
	salts = BLOOMFILTER_GET_SALTS(bf);
	salts_net = BLOOMFILTER_GET_SALTS(bf_net);
	bins = BLOOMFILTER_GET_FILTER(bf);
	bins_net = BLOOMFILTER_GET_FILTER(bf_net);

	/* Now convert into network byte order */
	bf_net->k = htonl(bf->k);
	bf_net->m = htonl(bf->m);
	bf_net->n = htonl(bf->n);

	for (i = 0; i < bf->k; i++)
		salts_net[i] = htonl(salts[i]);
	
#ifdef COUNTING_BLOOMFILTER
	for (i = 0; i < bf->m; i++) {
		bins_net[i] = htonl(bins[i]);
	}
#else
	memcpy(bins_net, bins, FILTER_LEN(bf));
#endif

	len = base64_encode_alloc((const char *)bf_net, BLOOMFILTER_TOT_LEN(bf), &b64str);

	bloomfilter_free(bf_net);

	if (b64str == NULL && len == 0) {
		fprintf(stderr, "Bloomfilter ERROR: input too long to base64 encoder\n");
		return NULL;
	}
	if (b64str == NULL) {
		fprintf(stderr, "Bloomfilter ERROR: base64 encoder allocation error\n");
		return NULL;
	}
	if (len < 0)
		return NULL;

	return b64str;
}

struct bloomfilter *base64_to_bloomfilter(const char *b64str, const size_t b64len)
{
	struct base64_decode_context b64_ctx;
	struct bloomfilter *bf_net;
	char *ptr;
	size_t len;
	salt_t *salts;
	unsigned int i;

	base64_decode_ctx_init(&b64_ctx);

	if (!base64_decode_alloc(&b64_ctx, b64str, b64len, &ptr, &len)) {
		return NULL;
	}

	bf_net = (struct bloomfilter *)ptr;

	bf_net->k = ntohl(bf_net->k);
	bf_net->m = ntohl(bf_net->m);
	bf_net->n = ntohl(bf_net->n);

	salts = BLOOMFILTER_GET_SALTS(bf_net);

	for (i = 0; i < bf_net->k; i++)
		salts[i] = ntohl(salts[i]);
	
#ifdef COUNTING_BLOOMFILTER
	for (i = 0; i < bf_net->m; i++) {
		bin_t *bins = BLOOMFILTER_GET_FILTER(bf_net);
		bins[i] = ntohl(bins[i]);
	}
#endif
		
	return bf_net;
}

int bloomfilter_operation(struct bloomfilter *bf, const char *key, 
			  const unsigned int len, unsigned int op)
{
	unsigned char *buf;
	unsigned int i;
#ifdef COUNTING_BLOOMFILTER
	unsigned short removed = 0;
#endif
	int res = 1;
	salt_t *salts;

	if (!bf || !key)
		return -1;

	if (op >= BF_OP_MAX) {
		return -1;
	}

	buf = malloc(len + SALTS_LEN(bf));
	
	if (!buf)
		return -1;
	
	memcpy(buf, key, len);
	
	salts = BLOOMFILTER_GET_SALTS(bf);

	for (i = 0; i < bf->k; i++) {
		SHA_CTX ctxt;
		unsigned int md[SHA_DIGEST_LENGTH];
		unsigned int hash = 0;
		int j, index;
		
		/* Salt the input */
		memcpy(buf + len, salts + i, SALT_SIZE);

		SHA1_Init(&ctxt);
		SHA1_Update(&ctxt, buf, len + SALT_SIZE);
		SHA1_Final((unsigned char *)md, &ctxt);
				
		for (j = 0; j < 5; j++) {
			hash = hash ^ md[j];			
		}
		
		/* Maybe there is a more efficient way to set the
		   correct bits? */
		index = hash % bf->m;

		//printf("index%d=%u\n", i, index);

		switch(op) {
		case BF_OP_CHECK:
#ifdef COUNTING_BLOOMFILTER
			if (BLOOMFILTER_GET_FILTER(bf)[index] == 0) {
				res = 0;
				goto out;
			}
#else
			if (!(BLOOMFILTER_GET_FILTER(bf)[index/8] & (1 << (index % 8)))) {
				res = 0;
				goto out;
			}
#endif
			break;
		case BF_OP_ADD:
#ifdef COUNTING_BLOOMFILTER
			(BLOOMFILTER_GET_FILTER(bf)[index]++;
#else
			BLOOMFILTER_GET_FILTER(bf)[index/8] |= (1 << (index % 8));
#endif
			break;
#ifdef COUNTING_BLOOMFILTER
		case BF_OP_REMOVE:
			if (BLOOMFILTER_GET_FILTER(bf)[index] > 0) {
				BLOOMFILTER_GET_FILTER(bf)[index]--;
				removed++;
			}
			/*
			else 
				fprintf(stderr, "Cannot remove item, because it is not in filter\n");
			 */
			break;
#endif
		default:
			fprintf(stderr, "Unknown Bloomfilter operation\n");
		}
		
	}
	/* Increment or decrement the number of objects in the filter depending on operation */
	if (op == BF_OP_ADD)
		bf->n++;
#ifdef COUNTING_BLOOMFILTER
	else if (op == BF_OP_REMOVE && removed > 0)
		bf->n--;
#endif
	
out:
	free(buf);

	return res;
}

void bloomfilter_free(struct bloomfilter *bf)
{
	if (bf)
		free(bf);	
}

/* Adapted from the perl code found at this URL:
 * http://www.perl.com/lpt/a/831 and at CPAN */
#define MAX_NUM_HASH_FUNCS 100

int bloomfilter_calculate_length(unsigned int num_keys, double error_rate, 
				 unsigned int *lowest_m, unsigned int *best_k)
{
	double m_tmp = -1;
	double k;
	
	*best_k = 1;

	for (k = 1; k <= MAX_NUM_HASH_FUNCS; k++) {
		double m = (-1 * k * num_keys) / log(1 - pow(error_rate, 1/k));
		
		if (m_tmp < 0 || m < m_tmp) {
			m_tmp = m;
			*best_k = (unsigned int)k;
		}
	}
	*lowest_m = (unsigned int)(m_tmp + 1);

	/* m must be evenly divisible by eight */
	if (*lowest_m % 8 != 0)
		*lowest_m += (8-(*lowest_m % 8));

	return 0;
}

#ifdef MAIN_DEFINED

int main(int argc, char **argv)
{
	struct bloomfilter *bf, *bf2;
	char *key = "John McEnroe";
	char *key2= "John";
	int res;
	
	bf = bloomfilter_new(0.01, 1000);
	bf2 = bloomfilter_new(0.01, 1000);
	
	if (!bf || !bf2)
		return -1;

//	printf("bloomfilter m=%u, k=%u\n", bf->m, bf->k);

	bloomfilter_add(bf, key, strlen(key));
	//printf("bf1:\n");
	//bloomfilter_print(bf);
	bloomfilter_add(bf, key, strlen(key));
	//printf("bf1:\n");
	//bloomfilter_print(bf);
#ifdef COUNTING_BLOOMFILTER
	bloomfilter_remove(bf, key, strlen(key));
#endif
	
	printf("bf1:\n");
	bloomfilter_print(bf);

	bloomfilter_add(bf2, key2, strlen(key2));
	
	//printf("bf2:\n");
	//bloomfilter_print(bf2);
	
//	res = bloomfilter_check(bf, key2, strlen(key2));
	res = bloomfilter_check(bf2, key2, strlen(key2));

	if (res)
		printf("\"%s\" is in filter\n", key2);
	else
		printf("\"%s\" is NOT in filter\n", key2);

	char *bfstr = bloomfilter_to_base64(bf);

	bloomfilter_free(bf2);

	bf2 = base64_to_bloomfilter(bfstr, strlen(bfstr));

	printf("Base64: %s\n", bfstr);

	//printf("bf2:\n");
	//bloomfilter_print(bf2);

	free(bfstr);
	bloomfilter_free(bf);
	bloomfilter_free(bf2);

	return 0;
}

#endif
