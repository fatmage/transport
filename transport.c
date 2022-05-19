#include "transport.h"


int main(int argc, char **argv) {

    if (argc != 5) {
        fprintf(stderr, "Wrong number of arguments! Usage : ./transport [server_ip_address] [port] [file_name] [file_size]\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        fprintf(stderr, "socket error: [%s]\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    uint32_t serverip;
    if (inet_pton(AF_INET,argv[1],&serverip) < 0) {
        fprintf(stderr, "inet_pton error: [%s]\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    uint32_t serverport = htons(atoi(argv[2]));

    struct sockaddr_in serveraddr;
    memset(&serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = serverip;
    serveraddr.sin_port = serverport;


    if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        fprintf(stderr, "bind error: [%s]\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    FILE *file_to_get = fopen(argv[3],"a");
    size_t file_size = atoi(argv[4]);

    chunk_t chunks[CHUNK_LIMIT] = {};
    byte file_data[CHUNK_LIMIT*CHUNK_SIZE] = {};
    memset(chunks,0,sizeof(chunk_t)*CHUNK_LIMIT);

    uint32_t received_size = 0;
    uint32_t current_chunks = 0;
    if (file_size > CHUNK_LIMIT*CHUNK_SIZE) {
        current_chunks = CHUNK_LIMIT;
        for (unsigned int i = 0; i < current_chunks; i++)
            chunks[i].size = CHUNK_SIZE;

    } else {
        if (file_size % CHUNK_SIZE == 0) {
            current_chunks = file_size / CHUNK_SIZE;
            for (unsigned int i = 0; i < current_chunks; i++)
                chunks[i].size = CHUNK_SIZE;
        }
        else {
            current_chunks = (file_size / CHUNK_SIZE) +1;
            for (unsigned int i = 0; i < current_chunks - 1; i++)
                chunks[i].size = CHUNK_SIZE;
            chunks[current_chunks - 1].size = file_size % CHUNK_SIZE;
        }
    }

    while (received_size < file_size) {

        // sendujemy zapytanie o te ktorych jeszcze nie mamy
        for (unsigned int i = 0; i < current_chunks; i++) {
            if (!chunks[i].received) {
                char msg[200] = {};
                sprintf(msg, "GET %d %d\n", START(received_size,i), chunks[i].size);
                uint16_t msgsize = strlen(msg);

                for (unsigned int i = 0; i < REPEATS; i++)
                    sendto(sockfd, msg, msgsize, 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
            }
        }

        // odbieramy
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 500;

        fd_set dsc;
        FD_ZERO(&dsc);
        FD_SET(sockfd, &dsc);

        while (select(sockfd+1, &dsc, NULL, NULL, &tv)) {

            struct sockaddr_in 	sender;	
            socklen_t 			sender_len = sizeof(sender);
            byte 			data[IP_MAXPACKET] = {};

            ssize_t p_len = recvfrom(
                sockfd,
                data,
                IP_MAXPACKET,
                MSG_DONTWAIT,
                (struct sockaddr*)&sender,
                &sender_len);

            if (p_len <= 0)
                continue;

            // check ip i port jesli sie nie zgadza to continue

            if (sender.sin_addr.s_addr != serveraddr.sin_addr.s_addr || sender.sin_port != serveraddr.sin_port)
                continue;

            uint32_t offset;
            uint32_t data_size;
            uint16_t datastart = strcspn(data,"\n");

            byte *msg = (byte*)malloc(datastart);
            memcpy(msg,data,datastart);

            // początek wiadomości nie pasuje do wzorca
            if (strncmp("DATA ", data, 5) != 0)
                continue;

            offset = atoi((char*)data+5);
            data_size = atoi((char*)data+5+(uint32_t)floor(log10((double)offset))+1);
            memcpy(data+offset-received_size,data+datastart,data_size);

            // oznaczyc ze wczytane
            chunks[INDEX(offset, received_size, CHUNK_SIZE)].received = RECEIVED;
        }

        // zapisujemy co mozemy

        uint16_t ready_chunks = 0;
        uint16_t ready_size = 0;

        for (unsigned int i = 0; i < current_chunks; i++) {
            if (chunks[i].received != RECEIVED)
                break;
            ready_chunks++;
            ready_size += chunks[i].size;
        }

        if (ready_chunks > 0) {

            // zapisujemy
            fwrite(file_data,ready_size,1,file_to_get);
            

            // przesuwamy okno
            memmove(file_data, file_data+ready_size, CHUNK_LIMIT*CHUNK_SIZE-ready_size);
            for (unsigned int i = 0; i < current_chunks - ready_chunks; i++) {
                chunks[i] = chunks[i+ready_chunks];
            }
            uint32_t old_chunks = current_chunks - ready_chunks;
            received_size += ready_size;
            
            if (file_size - received_size > CHUNK_LIMIT*CHUNK_SIZE) {
                current_chunks = CHUNK_LIMIT;
                for (unsigned int i = old_chunks; i < current_chunks; i++) {
                    chunks[i].size = CHUNK_SIZE;
                    chunks[i].received = PENDING;
                }
                    

            } else {
                if ((file_size - received_size) % CHUNK_SIZE == 0) {
                    current_chunks = (file_size - received_size) / CHUNK_SIZE;
                    for (unsigned int i = old_chunks; i < current_chunks; i++) {
                        chunks[i].size = CHUNK_SIZE;
                        chunks[i].received = PENDING;
                    }
                        
                }
                else {
                    current_chunks = ((file_size - received_size) / CHUNK_SIZE) +1;
                    for (unsigned int i = old_chunks; i < current_chunks - 1; i++) {
                        chunks[i].size = CHUNK_SIZE;
                        chunks[i].received = PENDING;
                    }
                    chunks[current_chunks - 1].size = file_size % CHUNK_SIZE;
                    chunks[current_chunks - 1].received = PENDING;
                }
            }

        }

        
    }


    fclose(file_to_get);

    return EXIT_SUCCESS;
}