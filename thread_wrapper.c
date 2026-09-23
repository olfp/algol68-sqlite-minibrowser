#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>

/* aus sock_wrapper.c */
extern int algol68_socket_accept(int listen_fd);

/*
 * Der Algol 68 runtime (libga68) bringt einen eingebetteten Boehm-GC mit.
 * Boehm-GC kann nur in Threads sammeln, die beim GC registriert sind.
 * pthread_create-Threads (unten) sind das nicht - der GC bricht sonst mit
 * "Collecting from unknown thread" ab. Daher registrieren wir jeden
 * Worker-Thread nach dem Start und melden ihn beim Ende wieder ab.
 *
 * Die GC-Funktionen werden per dlsym(RTLD_DEFAULT) aufgeloest, damit die
 * Kopplung an die vom Algol-Runtime tatsaechlich benutzte GC-Instanz
 * (libga68, wird als erstes nachgeladen) garantiert ist und nicht an eine
 * der Dateien gelinkte, separate Kopie (libgc, bdw-gc-alt) geraten kann.
 */

struct gc_stack_base {
  void * mem_base;
};

typedef void (*a68_gc_allow_register_threads_fn)(void);
typedef int  (*a68_gc_get_stack_base_fn)(struct gc_stack_base *);
typedef int  (*a68_gc_register_my_thread_fn)(struct gc_stack_base *);
typedef int  (*a68_gc_unregister_my_thread_fn)(void);

static a68_gc_allow_register_threads_fn gc_allow_register_threads;
static a68_gc_get_stack_base_fn         gc_get_stack_base;
static a68_gc_register_my_thread_fn     gc_register_my_thread;
static a68_gc_unregister_my_thread_fn   gc_unregister_my_thread;

static void gc_functions_init(void)
{
  gc_allow_register_threads =
    (a68_gc_allow_register_threads_fn) dlsym(RTLD_DEFAULT, "GC_allow_register_threads");
  gc_get_stack_base =
    (a68_gc_get_stack_base_fn) dlsym(RTLD_DEFAULT, "GC_get_stack_base");
  gc_register_my_thread =
    (a68_gc_register_my_thread_fn) dlsym(RTLD_DEFAULT, "GC_register_my_thread");
  gc_unregister_my_thread =
    (a68_gc_unregister_my_thread_fn) dlsym(RTLD_DEFAULT, "GC_unregister_my_thread");
}

static void gc_register_thread(void)
{
  struct gc_stack_base base;
  if (gc_get_stack_base && gc_register_my_thread &&
      gc_get_stack_base(&base) == 0)
    gc_register_my_thread(&base);
}

/* The Algol 68 callback: given a client file descriptor, handle one request.
   Signature in Algol 68: proc(int) void */
typedef void (*a68_handler_t)(int client_fd);

struct worker_args {
  int listen_fd;
  a68_handler_t handler;
  int id;
};

static void *worker_thread(void *arg)
{
  struct worker_args *a = (struct worker_args *) arg;
  gc_register_thread();
  while (1) {
    int client_fd = algol68_socket_accept(a->listen_fd);
    if (client_fd >= 0)
      a->handler(client_fd);
  }
  if (gc_unregister_my_thread)
    gc_unregister_my_thread();
  return NULL;
}

/* Start nr_workers threads, each looping accept+handle on listen_fd. */
void algol68_server_serve(int listen_fd, int nr_workers,
                          a68_handler_t handler)
{
  pthread_t threads[nr_workers];
  struct worker_args args[nr_workers];

  gc_functions_init();
  if (gc_allow_register_threads)
    gc_allow_register_threads();

  for (int i = 0; i < nr_workers; i++) {
    args[i].listen_fd = listen_fd;
    args[i].handler   = handler;
    args[i].id        = i;
    pthread_create(&threads[i], NULL, worker_thread, &args[i]);
  }

  /* Wait forever: the server runs until killed. */
  for (int i = 0; i < nr_workers; i++)
    pthread_join(threads[i], NULL);
}