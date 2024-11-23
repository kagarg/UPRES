#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>

#define MAX_PROGRAMS 3
#define NUM_PROCESSES 5
#define MSGSZ 128
#define QUEUE_NAME "/msg_queue"

struct message {
long mtype;
char mtext[MSGSZ];
};

void child_process(mqd_t mqd, int id) {
    struct message msg;
    msg.mtype = id;

    // Генерация имени программы
    snprintf(msg.mtext, MSGSZ, "Lab%d", id);

    // Отправка сообщения в очередь
    if (mq_send(mqd, (char*)&msg, sizeof(msg),NUM_PROCESSES + 1 - id) == -1) { // здесь NUM_PROCESSES+1-id это приоритет сообщения
    perror("mq_send");
    exit(1);
    }

    printf("Child %d sent message: %s with priority %d\n", getpid(), msg.mtext, NUM_PROCESSES + 1 - id);
    exit(0);
}

void parent_process(mqd_t mqd, int shmid) {
    sem_t* semaphore = shmat(shmid, NULL, 0);
    if (semaphore == (sem_t*)-1){
        perror("shmat");
        exit(1);
    }
    
    struct message msg;
    unsigned int prio;
    int curlen = NUM_PROCESSES;
    while (curlen > 0) {
        if (mq_receive(mqd, (char*)&msg, sizeof(msg), &prio) == -1) {
            perror("mq_receive");
            exit(1);
        }
        // Уменьшение счетчика семафора
        sem_wait(semaphore);
        // Симуляция выполнения программы
        printf("executing a program: %s with priority %d\n", msg.mtext, prio);
        //sleep(NUM_PROCESSES + 1 - prio); // Симуляция работы программы в зависимости от приоритета
        sleep(1);
        printf("Finished program: %s with priority %d\n", msg.mtext, prio);
        sem_post(semaphore);// Увеличение счетчика семафора
        curlen --;
        
        // отсоединение разделяемой обл. памяти
        if (shmdt(semaphore)== -1){ 
            perror("shmdt");
            exit(1);
        }
        exit(0);
    }
}

int main() {
    key_t key = 1234;
    // создание разделяемой памяти
    int shmid = shmget(key, sizeof(sem_t), IPC_CREAT | 0666); 
    if (shmid<0){
        perror("shmget");
        exit(1);
    }
    // присоединение разделяемой памяти
    sem_t* semaphore = shmat(shmid, NULL, 0); 
    if (semaphore == (sem_t*)-1){
        perror("shmat");
        exit(1);
    }

    // Инициализация семафора
    if (sem_init(semaphore, 1, MAX_PROGRAMS) == -1) {
        perror("sem_init");
        exit(1);
    }

    // Создание очереди сообщений
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct message);
    attr.mq_curmsgs = 0;

    mqd_t mqd = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mqd == (mqd_t)-1) {
        perror("mq_open");
        exit(1);
    }

    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (fork() == 0) {
        child_process(mqd, i + 1);
        }
    }

    for (int i = 0; i < NUM_PROCESSES; i++){ wait(NULL); }


    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (fork() == 0) {
        parent_process(mqd, shmid);
        }
    }

    for (int i = 0; i < NUM_PROCESSES; i++){ wait(NULL); }

    // Удаление семафора и очереди сообщений
    sem_destroy(semaphore);
    mq_close(mqd);
    mq_unlink(QUEUE_NAME);

    // отсоединение разделяемой обл. памяти
    if (shmdt(semaphore)== -1){ 
        perror("shmdt");
        exit(1);
    }

    //удаление разделяемой памяти
    if(shmctl(shmid,IPC_RMID, NULL)==-1){ 
        perror("shmctl");
        exit(1);
    }

    return 0;
}
