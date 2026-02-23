#include <starpu.h>
#include <starpu_scheduler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct rr_sched_data {
  struct starpu_task_list sched_list;
  starpu_pthread_mutex_t policy_mutex;
  unsigned workerids[STARPU_NMAXWORKERS];
  unsigned nworkers;
  unsigned cpu_workerids[STARPU_NMAXWORKERS];
  unsigned ncpu_workers;
  unsigned current_cpu_rr_index;
  unsigned cuda_workerids[STARPU_NMAXWORKERS];
  unsigned ncuda_workers;
  unsigned current_cuda_rr_index;
  unsigned push_count;
  unsigned pop_count;
  unsigned null_pop_count;
  unsigned rejected_turn_count;
  int debug_enabled;
};

static void init_rr_sched(unsigned sched_ctx_id) {
  struct rr_sched_data *data =
      (struct rr_sched_data *)malloc(sizeof(struct rr_sched_data));

  starpu_task_list_init(&data->sched_list);
  starpu_sched_ctx_set_policy_data(sched_ctx_id, (void *)data);
  STARPU_PTHREAD_MUTEX_INIT(&data->policy_mutex, NULL);

  data->current_cpu_rr_index = 0;
  data->current_cuda_rr_index = 0;
  data->push_count = 0;
  data->pop_count = 0;
  data->null_pop_count = 0;
  data->rejected_turn_count = 0;

  const char *debug_env = starpu_getenv("RR_WORKERS_DEBUG");
  data->debug_enabled = debug_env ? atoi(debug_env) : 0;

  struct starpu_worker_collection *workers =
      starpu_sched_ctx_get_worker_collection(sched_ctx_id);
  struct starpu_sched_ctx_iterator it;
  data->nworkers = 0;
  data->ncpu_workers = 0;
  data->ncuda_workers = 0;
  workers->init_iterator(workers, &it);
  while (workers->has_next(workers, &it)) {
    unsigned w = workers->get_next(workers, &it);
    data->workerids[data->nworkers++] = w;
    if (starpu_worker_get_type(w) == STARPU_CPU_WORKER)
      data->cpu_workerids[data->ncpu_workers++] = w;
    else if (starpu_worker_get_type(w) == STARPU_CUDA_WORKER)
      data->cuda_workerids[data->ncuda_workers++] = w;
  }

  if (data->debug_enabled) {
    fprintf(stderr, "[RR_WORKERS] Init with %u workers\n", data->nworkers);
  }
}

static void deinit_rr_sched(unsigned sched_ctx_id) {
  struct rr_sched_data *data =
      (struct rr_sched_data *)starpu_sched_ctx_get_policy_data(sched_ctx_id);

  if (data->debug_enabled) {
    fprintf(stderr,
            "[RR_WORKERS] Deinit stats: push_task=%u, pop_task=%u, "
            "null_pop_task=%u, rejected_turns=%u\n",
            data->push_count, data->pop_count, data->null_pop_count,
            data->rejected_turn_count);
  }

  STARPU_PTHREAD_MUTEX_DESTROY(&data->policy_mutex);
  free(data);
}

static int push_task_rr(struct starpu_task *task) {
  unsigned sched_ctx_id = task->sched_ctx;
  struct rr_sched_data *data =
      (struct rr_sched_data *)starpu_sched_ctx_get_policy_data(sched_ctx_id);

  if (data->debug_enabled)
    fprintf(stderr, "[RR_WORKERS] push_task_rr called for task %p\n", task);

  STARPU_PTHREAD_MUTEX_LOCK(&data->policy_mutex);
  starpu_task_list_push_back(&data->sched_list, task); /* Global FIFO */
  data->push_count++;
  starpu_push_task_end(task);
  STARPU_PTHREAD_MUTEX_UNLOCK(&data->policy_mutex);

  /* Wake all workers because they might need to check their turn */
  struct starpu_worker_collection *workers =
      starpu_sched_ctx_get_worker_collection(sched_ctx_id);
  struct starpu_sched_ctx_iterator it;
  workers->init_iterator(workers, &it);
  while (workers->has_next(workers, &it)) {
    unsigned worker = workers->get_next(workers, &it);
    starpu_wake_worker_relax_light(worker);
  }

  return 0;
}

static struct starpu_task *pop_task_rr(unsigned sched_ctx_id) {
  struct rr_sched_data *data =
      (struct rr_sched_data *)starpu_sched_ctx_get_policy_data(sched_ctx_id);
  int workerid = starpu_worker_get_id();

  /* Main thread or uninitialized worker calling pop_task? safely return NULL */
  if (workerid == -1)
    return NULL;

  STARPU_PTHREAD_MUTEX_LOCK(&data->policy_mutex);

  if (data->nworkers == 0) {
    struct starpu_worker_collection *workers =
        starpu_sched_ctx_get_worker_collection(sched_ctx_id);
    struct starpu_sched_ctx_iterator it;
    workers->init_iterator(workers, &it);
    while (workers->has_next(workers, &it)) {
      unsigned w = workers->get_next(workers, &it);
      data->workerids[data->nworkers++] = w;
      if (starpu_worker_get_type(w) == STARPU_CPU_WORKER)
        data->cpu_workerids[data->ncpu_workers++] = w;
      else if (starpu_worker_get_type(w) == STARPU_CUDA_WORKER)
        data->cuda_workerids[data->ncuda_workers++] = w;
    }
    if (data->debug_enabled && data->nworkers > 0) {
      fprintf(
          stderr,
          "[RR_WORKERS] Lazy init in pop found %u workers (CPU:%u, CUDA:%u)\n",
          data->nworkers, data->ncpu_workers, data->ncuda_workers);
    }
  }

  if (data->nworkers == 0) {
    STARPU_PTHREAD_MUTEX_UNLOCK(&data->policy_mutex);
    return NULL;
  }

  unsigned expected_worker = (unsigned)-1;
  enum starpu_worker_archtype archtype = starpu_worker_get_type(workerid);
  if (archtype == STARPU_CPU_WORKER && data->ncpu_workers > 0) {
    expected_worker = data->cpu_workerids[data->current_cpu_rr_index];
  } else if (archtype == STARPU_CUDA_WORKER && data->ncuda_workers > 0) {
    expected_worker = data->cuda_workerids[data->current_cuda_rr_index];
  } else {
    STARPU_PTHREAD_MUTEX_UNLOCK(&data->policy_mutex);
    return NULL;
  }

  if ((unsigned)workerid != expected_worker) {
    data->rejected_turn_count++;
    STARPU_PTHREAD_MUTEX_UNLOCK(&data->policy_mutex);
    return NULL;
  }

  if (starpu_task_list_empty(&data->sched_list)) {
    data->null_pop_count++;
    STARPU_PTHREAD_MUTEX_UNLOCK(&data->policy_mutex);
    return NULL;
  }

  struct starpu_task *task = NULL;
  struct starpu_task *t;
  for (t = starpu_task_list_begin(&data->sched_list);
       t != starpu_task_list_end(&data->sched_list);
       t = starpu_task_list_next(t)) {
    if (starpu_worker_can_execute_task(workerid, t, 0)) {
      task = t;
      starpu_task_list_erase(&data->sched_list, t);
      break;
    }
  }

  if (task) {
    data->pop_count++;
    if (archtype == STARPU_CPU_WORKER)
      data->current_cpu_rr_index =
          (data->current_cpu_rr_index + 1) % data->ncpu_workers;
    else
      data->current_cuda_rr_index =
          (data->current_cuda_rr_index + 1) % data->ncuda_workers;
    if (data->debug_enabled)
      fprintf(stderr, "[RR_WORKERS] pop_task_rr: worker %d popped task %p\n",
              workerid, task);
  } else {
    data->null_pop_count++;
    /* STRICT RR: Do not yield the turn. The worker retains its turn until it
     * executes a task. */
  }

  unsigned next_worker = (unsigned)-1;
  if (archtype == STARPU_CPU_WORKER)
    next_worker = data->cpu_workerids[data->current_cpu_rr_index];
  else
    next_worker = data->cuda_workerids[data->current_cuda_rr_index];
  STARPU_PTHREAD_MUTEX_UNLOCK(&data->policy_mutex);

  /* Always wake the next worker in the sequence to prevent deadlock */
  if (next_worker != (unsigned)-1)
    starpu_wake_worker_relax_light(next_worker);

  return task;
}

static struct starpu_sched_policy rr_workers_sched_policy = {
    .init_sched = init_rr_sched,
    .deinit_sched = deinit_rr_sched,
    .push_task = push_task_rr,
    .pop_task = pop_task_rr,
    .policy_name = "rr_workers",
    .policy_description = "strict round-robin across workers",
    .worker_type = STARPU_WORKER_LIST,
    .prefetches = 1,
};

struct starpu_sched_policy *starpu_get_sched_lib_policy(const char *name) {
  if (!strcmp(name, "rr_workers"))
    return &rr_workers_sched_policy;
  return NULL;
}

struct starpu_sched_policy *predefined_policies[] = {&rr_workers_sched_policy,
                                                     NULL};

struct starpu_sched_policy **starpu_get_sched_lib_policies(void) {
  return predefined_policies;
}
