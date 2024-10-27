#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <semaphore.h>

#define SIZE 1024 * 1024
#define SHM_NAME "/shm_test"
#define SOCKET_PATH "./socket_test"
#define FILENAME "test_file"

void create_dummy_message(char *buffer, size_t size) {
    memset(buffer, 'A', size - 1);
    buffer[size - 1] = '\0';
}

void measure_mmap(size_t message_size) {
    char *shared_mem = mmap(NULL, message_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (sem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (sem_init(sem, 1, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if (fork() == 0) {
        char read_buffer[message_size];
        sem_wait(sem);
        memcpy(read_buffer, shared_mem, message_size);
        exit(0);
    } else {
        clock_t start = clock();
        
        create_dummy_message(shared_mem, message_size);
        sem_post(sem);
        wait(NULL);
        
        clock_t end = clock();
        double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        double throughput = (double)message_size / elapsed_time / (1024 * 1024) * 1000;
        printf("mmap: Elapsed time = %.5f ms\n", elapsed_time);
        printf("mmap: Throughput = %.2f MB/ms\n", throughput);

        sem_destroy(sem);
        munmap(shared_mem, message_size);
        munmap(sem, sizeof(sem_t));
    }
}

void measure_shared_memory(size_t message_size) {
    key_t key = ftok(SHM_NAME, 65);
    
    int shm_id = shmget(key, message_size, IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    char *shared_mem = shmat(shm_id, NULL, 0);
    if (shared_mem == (char *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (sem_init(sem, 1, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if (fork() == 0) {
        char read_buffer[message_size];

        sem_wait(sem);

        memcpy(read_buffer, shared_mem, message_size);

        shmdt(shared_mem);
        exit(0);
    } else {
        clock_t start = clock();

        create_dummy_message(shared_mem, message_size);
        sem_post(sem);
        wait(NULL);

        clock_t end = clock();
        double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        double throughput = (double)message_size / elapsed_time / (1024 * 1024) * 1000;

        printf("shared memory: Elapsed time = %.5f ms\n", elapsed_time);
        printf("shared memory: Throughput = %.2f MB/s\n", throughput);

        sem_destroy(sem);
        munmap(sem, sizeof(sem_t));
        shmdt(shared_mem);
        shmctl(shm_id, IPC_RMID, NULL);
    }
}

void measure_unix_socket(size_t message_size) {
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (sem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (sem_init(sem, 1, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    
    if (fork() == 0) {
        sem_wait(sem);
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_fd == -1) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
            perror("connect");
            exit(EXIT_FAILURE);
        }

        char *read_buffer = malloc(message_size);
        if (read(client_fd, read_buffer, message_size) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        close(client_fd);
        free(read_buffer);
        exit(0);
    } else {
        unlink(SOCKET_PATH);
        clock_t start = clock();

        if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
            perror("bind");
            exit(EXIT_FAILURE);
        }


        if (listen(sock_fd, 1) == -1) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        sem_post(sem);

        int conn_fd = accept(sock_fd, NULL, NULL);
        if (conn_fd == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        

        char *message = malloc(message_size);
        create_dummy_message(message, message_size);

        if (write(conn_fd, message, message_size) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        close(conn_fd);
        wait(NULL);

        clock_t end = clock();
        double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        double throughput = (double)message_size / elapsed_time / (1024 * 1024) * 1000;

        printf("unix socket: Elapsed time = %.5f ms\n", elapsed_time);
        printf("unix socket: Throughput = %.2f MB/s\n", throughput);

        close(sock_fd);
        unlink(SOCKET_PATH);
        free(message);
    }
}

void measure_file_read_write(size_t message_size) {
    int file_fd = open(FILENAME, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (file_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (sem_init(sem, 1, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if (fork() == 0) {
        char read_buffer[message_size];
        sem_wait(sem);
        read(file_fd, read_buffer, message_size);
        close(file_fd);
        exit(0);
    } else {
        clock_t start = clock();
        char *message = malloc(message_size);
        create_dummy_message(message, message_size);

        write(file_fd, message, message_size);
        lseek(file_fd, 0, SEEK_SET);
        sem_post(sem); 

        wait(NULL);

        clock_t end = clock();
        double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        double throughput = (double)message_size / elapsed_time / (1024 * 1024) * 1000;

        printf("file read-write: Elapsed time = %.5f ms\n", elapsed_time);
        printf("file read-write: Throughput = %.2f MB/s\n", throughput);

        sem_destroy(sem);
        munmap(sem, sizeof(sem_t));
        close(file_fd);
        unlink(FILENAME);
        free(message);
    }
}


int main(int argc, char *argv[]) {
    size_t message_size = SIZE;

    if (argc >= 2) {
        message_size = atoi(argv[1]);
        if (message_size <= 0) {
            fprintf(stderr, "Invalid message size provided. Using default size: %zu bytes.\n", message_size);
            message_size = SIZE;
        }
    }

    printf("Starting IPC comparison with message size: %zu bytes...\n", message_size);

    measure_mmap(message_size);
    measure_shared_memory(message_size);
    measure_file_read_write(message_size);
    measure_unix_socket(message_size);

    return 0;
}
