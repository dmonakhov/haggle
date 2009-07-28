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
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#define HAGGLE_CTRL_SOCKET_PATH "/tmp/haggle.sock"
#define HAGGLE_SERVICE_DEFAULT_PORT 8787

int main (int argc, char **argv)
{
	int sock;
        ssize_t ret;
        char data[4096];
#if defined(USE_LOCAL_SOCKET)
	struct sockaddr_un saddr;
#else
        struct sockaddr_in saddr;
#endif

	memset(&saddr, 0, sizeof(saddr));

#if defined(USE_LOCAL_SOCKET)
	saddr.sun_family = PF_LOCAL;
	strcpy(saddr.sun_path, HAGGLE_CTRL_SOCKET_PATH);
        
	sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
#else
	saddr.sin_family = AF_INET;
	if(argc > 1)
		saddr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	saddr.sin_port = htons(HAGGLE_SERVICE_DEFAULT_PORT);
        
	sock = socket(PF_INET, SOCK_DGRAM, 0);
#endif


	if (sock < 0) {
		fprintf(stderr, "Could not create Haggle control socket\n");
		return -1;
	}
	ret = read(0, data, 4096);

        printf("Read %zd bytes data\n", ret);

        if (ret > 0) {
                ret = sendto(sock, data, ret, 0, (struct sockaddr *)&saddr, sizeof(saddr));

                
                if (ret < 0) {
                        perror("Sendto failed");
                }

	} else {
                fprintf(stderr, "could not read data : %s\n", strerror(errno));
        }

	close(sock);

	return ret;
}
