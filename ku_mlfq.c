#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

typedef struct process {
    struct process *next;
    pid_t pid;
    int use_cpu_time;
    int priority;
} Process;

typedef struct list {
    struct process *head;
    struct process *tail;
} List;

const int BOOST_TIME = 10;
const int STEP_DOWN_TIME = 2;

const int MAINTAIN = 0;
const int STEPDOWN = 1;

List* queue[3];
Process* running;
int timeSliceCount = 0;

void linkedListInit();
void initProcesses(pid_t pid);

void os_handler();
void os_signal_init();
void os_timer_init();
void what_OS_do();
Process* getProcess();
void what_Process_do();

void insert_process(Process*, int);
void priorityBoost();

int main(int argc, char* argv[]) {

    /*
        ------ Check the all Argument sets -------
    */
    if (argc < 3) {
        fprintf(stderr, "Insuffient number of Argument.\n");
        exit(1);
    } else if (argc > 3) {
        fprintf(stderr, "To Many Argment.\n");
        exit(1);
    }

    if (atoi(argv[1]) < 1 || atoi(argv[1]) > 26) {
        fprintf(stderr, "Argument is not in Correct Range.\n");
        exit(1);
    }
    //  -------------------------------------------

    int process_count = atoi(argv[1]);
    pid_t pid; // OS 역할의 프로세스 구분을 위한 변수
    int i;

    linkedListInit(); // queue 초기화
    for (i = 0; i < process_count; i++) {
        pid = fork();

        if (pid < 0) {
            fprintf(stderr, "fork Error: Errno.%d\n", errno);
            exit(1);
        } else if (pid > 0) { // 부모 프로세스

            initProcesses(pid); // 모든 프로세스 큐 삽입

        } else { // 자식 프로세스

            if (kill(getpid(), SIGSTOP) == -1) { // 모든 프로세스는 initializeiation 시 멈춰있는다.
                fprintf(stderr, "initializeializing SIGSTOP Error: Errno.%d\n", errno);
            }
            break;

        }
    }

    if (pid != 0) {     // ----- What Parent Process (OS) do
        sleep(5);
        os_timer_init();
        what_OS_do();
    } else {            // ----- What Child Process do
        what_Process_do();
    }

    // while (1);
    return 0;
} 

void linkedListInit() {
    int i = 0;
    for (i = 0; i < 3; i++) {
        queue[i] = (List*) malloc(sizeof(List));
        queue[i]->head = NULL;
        queue[i]->tail = NULL;
    }
}

void initProcesses(pid_t pid) {
    Process* ps = (Process*) malloc(sizeof(Process));
    ps->next = NULL;
    ps->pid = pid;
    ps->priority = 3;
    ps->use_cpu_time = 0;

    if (queue[2]->head == NULL && queue[2]->tail == NULL) {
        queue[2]->head = queue[2]->tail = ps;
    } else {
        queue[2]->tail->next = ps;
        queue[2]->tail = ps;
    }
}

void os_handler() {
    
    /*
        TODO list -> 해결? 확인 한번 더할것
        0. 이거 os에서 1초마다 실행되는거임
        1. 가장 높은 priority에 있는 거 맨 앞에꺼 먼저 꺼내와
        2. 꺼내와서 점유 시간이 2초 됐는지 확인해, 2초 지났으면 한단계 priority 내려
        3. 안지났으면 현재 queue 에서 맨 뒤로 보내 (Insert)
        4. 이때 10초 지났는지 봐야대 10초 됐으면 모든 큐에잇는거 boost해줘야해 -> priorityBoost
    */

    kill(running->pid, SIGSTOP); // 기존 프로세스 Stop
    running->use_cpu_time += 1;
    timeSliceCount += 1;

    if (timeSliceCount == BOOST_TIME) {
        priorityBoost();
    } else if (running->use_cpu_time == STEP_DOWN_TIME) {
        insert_process(running, STEPDOWN);
    } else { // 같은 priority의 tail에 삽입
        insert_process(running, MAINTAIN);
    }

    Process* focus = getProcess();
    running = focus; // 다음 프로세스로 change
    kill(running->pid, SIGCONT); // 새로운 프로세스 Start

}

void os_signal_init() {
    struct sigaction ps_check_sig;
    sigemptyset(&ps_check_sig.sa_mask);
    ps_check_sig.sa_flags = 0;
    ps_check_sig.sa_handler = os_handler;

    sigaction(SIGALRM, &ps_check_sig, NULL);
}

void os_timer_init() {

    // ---------- 1초마다 프로세스 변경 타이머 -----------
    struct itimerval ch_ps_timer;
    ch_ps_timer.it_interval.tv_sec = 1;
    ch_ps_timer.it_interval.tv_usec = 0;
    ch_ps_timer.it_value.tv_sec = 1;
    ch_ps_timer.it_value.tv_usec = 0;

    setitimer(ITIMER_REAL, &ch_ps_timer, NULL);
}

Process* getProcess() { // 가장 우선순위가 높은 Process 가져오기
    Process* ps;
    int i;
    for (i = 2; i >= 0; i--) {
        if (queue[i]->head != NULL) {
            ps = queue[i]->head;

            queue[i]->head = ps->next;
            ps->next = NULL;

            return ps;
        }
    }
}

void what_OS_do() {

    os_signal_init(); // 시그널 초기화
    os_timer_init(); // 타이머 초기화

    running = getProcess();
    kill(running->pid, SIGCONT);

}

void what_Process_do() {
/*
    TODO

    나중에 올라오는 강의자료 파일    
*/
}

void insert_process(Process* ps, int flag) {
    // flag == MAINTAIN : 같은 priority 맨 뒷줄
    // flag == STEPDOWN : priority 한단계 내림
    int q_idx;

    if (flag == STEPDOWN) {
        ps->use_cpu_time = 0;
        if (ps->priority > 1) ps->priority -= 1;
    }

    q_idx = ps->priority - 1;

    if (queue[q_idx]->head == NULL && queue[q_idx]->tail == NULL) {
        queue[q_idx]->head = queue[q_idx]->tail = ps;
    } else {
        queue[q_idx]->tail->next = ps;
        queue[q_idx]->tail = ps;
    }
}

void priorityBoost() {
    timeSliceCount = 0;

}