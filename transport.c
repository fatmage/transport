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

        // wysyłamy zapytania o te chunki których jeszcze nie mamy
        uint32_t queries = 0;
        for (unsigned int i = 0; i < current_chunks && queries < PACK_SIZE; i++) {
            if (chunks[i].received == PENDING) {
                char msg[20] = {};
                sprintf(msg, "GET %d %d\n", START(received_size,i), chunks[i].size);
                uint16_t msgsize = strlen(msg);

                for (unsigned int i = 0; i < REPEATS; i++)
                    sendto(sockfd, msg, msgsize, 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

                queries++;
            }
        }

        // odbieramy
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        fd_set dsc;
        FD_ZERO(&dsc);
        FD_SET(sockfd, &dsc);

        while (select(sockfd+1, &dsc, NULL, NULL, &tv)) {

            struct sockaddr_in 	sender;	
            socklen_t 			sender_len = sizeof(sender);
            byte 			    data[IP_MAXPACKET] = {};

            ssize_t p_len = recvfrom(
                sockfd,
                data,
                IP_MAXPACKET,
                MSG_DONTWAIT,
                (struct sockaddr*)&sender,
                &sender_len);

            if (p_len <= 0)
                continue;

            // sprawdzamy czy ip serwera i port serwera pokrywa się z ip i portem nadawcy
            if (sender.sin_addr.s_addr != serveraddr.sin_addr.s_addr || sender.sin_port != serveraddr.sin_port)
                continue;

            uint32_t offset;
            uint32_t data_size;
            uint16_t datastart = strcspn(data,"\n");

            // początek wiadomości nie pasuje do wzorca
            if (strncmp("DATA ", data, 5) != 0)
                continue;

            offset = atoi((char*)data+5);
            data_size = atoi((char*)data+5+(uint32_t)floor(log10((double)offset))+1);

            if (offset < received_size)
                continue;
            if (chunks[INDEX(offset, received_size, CHUNK_SIZE)].received == RECEIVED)
                continue;

            memcpy(file_data+offset-received_size,data+datastart+1,data_size);

            // oznaczamy, że dostaliśmy chunk pliku
            chunks[INDEX(offset, received_size, CHUNK_SIZE)].received = RECEIVED;
        }

        // zapisujemy co możemy
        uint32_t ready_chunks = 0;
        uint32_t ready_size = 0;

        for (unsigned int i = 0; i < current_chunks; i++) {
            if (chunks[i].received != RECEIVED)
                break;
            ready_chunks++;
            ready_size += chunks[i].size;
        }

        if (ready_chunks > 0) {
            // zapisujemy
            if (fwrite(file_data,ready_size,1,file_to_get) != 1) {
                fprintf(stderr, "fwrite error: [%s]\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            
            // przesuwamy okno
            uint32_t data_to_move = 0;
            for (unsigned int i = 0; i < current_chunks - ready_chunks; i++) {
                chunks[i] = chunks[i+ready_chunks];
                data_to_move += chunks[i].size;
            }

            memmove(file_data, file_data+ready_size, data_to_move);

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
                    for (unsigned int i = old_chunks; i < current_chunks; i++) {
                        if (i == current_chunks-1)
                            chunks[i].size = (file_size - received_size) % CHUNK_SIZE;
                        else
                            chunks[i].size = CHUNK_SIZE;
                        chunks[i].received = PENDING;
                    }
                }
            }

        }

        // printujemy postęp
        printf("%f%% done\n", 100.f*((float)received_size/file_size));
    }


    close(sockfd);
    fclose(file_to_get);

    return EXIT_SUCCESS;
}