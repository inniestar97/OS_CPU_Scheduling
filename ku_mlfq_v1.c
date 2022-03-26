#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

/*
    각자의 process를 구조체 내부에 정의하여 관리.
    Linked-List로 구성되어 Process의 timesharing을 가능게 함.

    next : 다음 Process
    pid : 관리되고 있는 Process id
    use_cpu_time : 현재 priority에서 cpu사용 시간
    priority : 현재 프로세스의 priority
*/
typedef struct process {
    struct process *next;
    pid_t pid;
    int use_cpu_time;
    int priority;
} Process;

/*
    각 Process들을 관리하고있는 Linked list
    Priority에 따라 3가지가 존재.
*/
typedef struct list {
    Process* head;
    Process* tail;
} List;

/*
    10s 이후 모든 Process Boost Priority
    use_cpu_time 2s이후 priority step down
*/
const int BOOST_TIME = 10;
const int STEP_DOWN_TIME = 2;

List* queue[3]; // priority에 따른 3개 Linked-List : priority 2, 1 ,0 순
Process* running; // current running process
int timePast = 0; // all running time

/*
    create Process 이후 time scheduling을 위해
    가장 높은 priority(3)에 process 삽입을 위한 함수
*/
void listInit();
void initInsert_process(pid_t pid);

void what_OS_do();
void what_process_do();

void os_timer_handler(); // os에서 time Slice (1s) 호출 handler

/*
    현재 실행되는 Process (running) 삽입
    MAINTAIN => 기존 Priority 유지
    STEPDOWN => Priority step down

    void insert_process(Process*, int)와 함께 사용
    args :
        0 : Process* => current running Process
        1 : int => flag : MAINTAIN, STEPDOWN
*/
const int MAINTAIN = 0;
const int STEPDOWN = 1;
void insert_process(Process* running, int flag);
Process* extract_process();

int main(int argc, char* argv[]) {
/*
----------------- Check the all Argument set ------------------------
*/
    if (argc != 3) {
        fprintf(stderr, "It's not fint in number of Arguments.\n");
        exit(1);
    }

    if (atoi(argv[1]) < 1 || atoi(argv[1]) > 26) {
        fprintf(stderr, "Argument is not in Correct Range.\n");
        exit(1);
    }
/*
----------------------------------------------------------------------
*/

    int process_count = atoi(argv[1]);
    pid_t pid;
    int i;

    listInit();
    for (i = 0; i < process_count; i++) {
        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork Error: errno.%d\n", errno);
            exit(1);
        } else if (pid > 0) { // parrent
            initInsert_process(pid);
        } else { // child
            if (kill(getpid(), SIGSTOP) == -1) {
                fprintf(stderr, "init SIGSTOP Error: errno.%d\n", errno);
                exit(1);
            }
            break;
        }
    }

    if (pid != 0) {     // ----- Parent Process Do (OS)
        sleep(5);
        what_OS_do();
    } else {            // ----- Child Process Do
        what_process_do();
    }

    while (1);
    return 0;
}

void listInit() {
    int i = 0;
    for (i = 0; i < 3; i++) {
        queue[i] = (List*) malloc(sizeof(List));
        queue[i]->head = queue[i]->tail = NULL;
    }
}

void what_OS_do() {

    struct sigaction ps_check_sig;
    sigemptyset(&ps_check_sig.sa_mask);
    ps_check_sig.sa_flags = 0;
    ps_check_sig.sa_handler = os_timer_handler;
    sigaction(SIGALRM, &ps_check_sig, NULL);

    // 1s time slice timer
    struct itimerval ch_ps_timer;
    ch_ps_timer.it_interval.tv_sec = 1;
    ch_ps_timer.it_interval.tv_usec = 0;
    ch_ps_timer.it_value.tv_sec = 1;
    ch_ps_timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &ch_ps_timer, NULL);

    // getProcess 해야한다. 처음에 os가 해줘야하는 작업 필요

}

void what_process_do() {
    // TODO: child Process -> lib
}

void os_timer_handler() {
    // TODO: os_handler
}

void insert_process(Process* ps, int flag) {
    // TODO: insert_process 프로세스 삽입 위치 결정
}

Process* extract_process() {
    // TODO: 가장 우선선위가 높은 Process 가져오기
}