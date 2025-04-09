#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <csignal>
#include <sys/time.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include "job.h"

#define IO_TIME 40
#define QUANTUM 100

using namespace std;

int sim_time_remaining;
int sim_current_time = 0;
string algorithm;
struct job* current_job = nullptr;

vector<struct job*> finished_jobs;
vector<struct job*> blocked_jobs;

vector<struct job*> srt_queue;
queue<struct job*> feedback_queues[4];

void print_job_result(struct job* j, int completion_time) {
    int turnaround = completion_time - j->w;
    float norm_turnaround = (float)turnaround / j->total_time;

    printf("Job ID: %d\n", j->id);
    printf(" Arrival Time: %d ms\n", j->w);
    printf(" Completion Time: %d ms\n", completion_time);
    printf(" Service Time: %d ms\n", j->total_time);
    printf(" Turnaround Time: %d ms\n", turnaround);
    printf(" Normalized Turnaround Time: %.2f\n\n", norm_turnaround);
}

void block_current_job() {
    current_job->e = sim_current_time;
    blocked_jobs.push_back(current_job);
    current_job = nullptr;
}

void complete_current_job() {
    print_job_result(current_job, sim_current_time);
    finished_jobs.push_back(current_job);
    current_job = nullptr;
}

void update_blocked_jobs() {
    auto it = blocked_jobs.begin();
    while (it != blocked_jobs.end()) {
        struct job* j = *it;
        if ((sim_current_time - j->e) >= IO_TIME) {
            j->next_interrupt++;
            if (algorithm == "srt")
                srt_queue.push_back(j);
            else
                feedback_queues[3].push(j);
            it = blocked_jobs.erase(it);
        } else {
            ++it;
        }
    }
}

void generate_random_job() {
    if (rand() % 100 < 2) {
        struct job* j = generate_next_job();
        j->w = sim_current_time;
        if (algorithm == "srt")
            srt_queue.push_back(j);
        else
            feedback_queues[0].push(j);
    }
}

struct job* select_from_srt() {
    if (srt_queue.empty()) return nullptr;
    auto min_it = min_element(srt_queue.begin(), srt_queue.end(),
                              [](struct job* a, struct job* b) {
                                  return a->time_remaining < b->time_remaining;
                              });
    struct job* selected = *min_it;
    srt_queue.erase(min_it);
    return selected;
}

struct job* select_from_feedback() {
    for (int i = 0; i < 4; ++i) {
        if (!feedback_queues[i].empty()) {
            struct job* j = feedback_queues[i].front();
            feedback_queues[i].pop();
            return j;
        }
    }
    return nullptr;
}

void reset_timer() {
    struct itimerval t;
    memset(&t, 0, sizeof(t));
    t.it_value.tv_usec = 1000; // 1ms
    setitimer(ITIMER_REAL, &t, nullptr);
}

void dispatcher(int signum) {
    sim_current_time++;
    sim_time_remaining--;

    if (current_job) {
        current_job->time_remaining--;

        // Handle I/O block
        if (current_job->next_interrupt < current_job->num_interrupts &&
            current_job->total_time - current_job->time_remaining >=
            current_job->interrupts[current_job->next_interrupt]) {
            block_current_job();
        } else if (current_job->time_remaining <= 0) {
            complete_current_job();
        } else {
            // Re-queue if not finished
            if (algorithm == "srt") {
                srt_queue.push_back(current_job);
            } else {
                feedback_queues[3].push(current_job);
            }
            current_job = nullptr;
        }
    }

    generate_random_job();
    update_blocked_jobs();

    if (!current_job) {
        if (algorithm == "srt")
            current_job = select_from_srt();
        else
            current_job = select_from_feedback();

        if (current_job && current_job->e == 0)
            current_job->e = sim_current_time;
    }

    if (sim_time_remaining <= 0) {
        printf("\n=== Simulation Finished ===\n");
        for (auto& j : finished_jobs) {
            print_job_result(j, sim_current_time);
        }
        exit(0);
    }

    reset_timer();
}

int main(int argc, char** argv) {
    if (argc != 4) {
        cerr << "Usage: ./sim <seed> <srt|feedback> <duration_ms>\n";
        return 1;
    }

    srandom(atoi(argv[1]));
    algorithm = argv[2];
    sim_time_remaining = atoi(argv[3]);

    if (algorithm != "srt" && algorithm != "feedback") {
        cerr << "Error: Algorithm must be 'srt' or 'feedback'\n";
        return 1;
    }

    signal(SIGALRM, dispatcher);

    struct job* initial = generate_next_job();
    initial->w = sim_current_time;

    if (algorithm == "srt")
        srt_queue.push_back(initial);
    else
        feedback_queues[0].push(initial);

    reset_timer();

    while (true)
        pause();

    return 0;
}
