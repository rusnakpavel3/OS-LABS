#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <pthread.h>

#define DECK_SIZE 52

typedef struct {
	unsigned long rounds;
	unsigned int seed;
	unsigned long hits;
} ThreadArgs;



static void print(int fd, const char *s)
{
	if (write(fd, s, strlen(s)) == -1)
		exit(1);
}

static void die(const char *msg)
{
	print(STDERR_FILENO, msg);
	exit(1);
}

static void print_num(unsigned long n, int min_digits)
{
	char buf[32];
	int len = 0;

	do {
		buf[sizeof(buf) - 1 - len++] = '0' + n % 10;
		n /= 10;
	} while (n > 0 || len < min_digits);

	if (write(STDOUT_FILENO, buf + sizeof(buf) - len, len) == -1)
		exit(1);
}

static long long now_us(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
		die("Ошибка clock_gettime\n");
	return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}



static unsigned long count_pairs(unsigned long rounds, unsigned int seed)
{
	int deck[DECK_SIZE];
	unsigned long hits = 0;

	for (int i = 0; i < DECK_SIZE; ++i)
		deck[i] = i;

	for (unsigned long r = 0; r < rounds; ++r) {
		for (int i = 0; i < DECK_SIZE - 1; ++i) {
			int j = i + rand_r(&seed) % (DECK_SIZE - i);
			int tmp = deck[i];
			deck[i] = deck[j];
			deck[j] = tmp;
		}

		if (deck[0] % 13 == deck[1] % 13)
			++hits;
	}
	return hits;
}


static void *work(void *_args)
{
	ThreadArgs *args = (ThreadArgs *)_args;

	args->hits = count_pairs(args->rounds, args->seed);
	return NULL;
}


static unsigned long run_parallel(long n_threads, long rounds)
{
	pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
	ThreadArgs *thread_args = malloc(n_threads * sizeof(ThreadArgs));

	if (threads == NULL || thread_args == NULL)
		die("Ошибка malloc\n");

	for (long i = 0; i < n_threads; ++i) {
		thread_args[i] = (ThreadArgs){
			.rounds = rounds / n_threads + (i < rounds % n_threads),
			.seed = i + 1,
		};

		if (pthread_create(&threads[i], NULL, work, &thread_args[i]) != 0)
			die("Ошибка pthread_create\n");
	}

	unsigned long hits = 0;
	for (long i = 0; i < n_threads; ++i) {
		if (pthread_join(threads[i], NULL) != 0)
			die("Ошибка pthread_join\n");
		hits += thread_args[i].hits;
	}

	free(thread_args);
	free(threads);
	return hits;
}




int main(int argc, char **argv)
{
	long n_threads = 0, rounds = 0;
	int parallel = 0;

	for (int i = 1; i + 1 < argc; i += 2) {
		if (strcmp(argv[i], "-t") == 0) {
			n_threads = atol(argv[i + 1]);
			parallel = 1;
		} else if (strcmp(argv[i], "-r") == 0) {
			rounds = atol(argv[i + 1]);
		}
	}

	if (rounds < 1 || (parallel && n_threads < 1))
		die("Использование: ./lab2 -r <раунды> [-t <потоки>]\n"
		    "без -t работает последовательная версия\n");

	long long start = now_us();
	unsigned long hits = parallel ? run_parallel(n_threads, rounds)
	                              : count_pairs(rounds, 1);
	long long us = now_us() - start;

	unsigned long scaled = hits * 1000000 / rounds;

	if (parallel) {
		print(STDOUT_FILENO, "Версия: параллельная, потоков: ");
		print_num(n_threads, 1);
	} else {
		print(STDOUT_FILENO, "Версия: последовательная");
	}
	print(STDOUT_FILENO, "\nРаундов: ");
	print_num(rounds, 1);
	print(STDOUT_FILENO, ", пар сверху: ");
	print_num(hits, 1);
	print(STDOUT_FILENO, "\nВероятность: ");
	print_num(scaled / 1000000, 1);
	print(STDOUT_FILENO, ".");
	print_num(scaled % 1000000, 6);
	print(STDOUT_FILENO, "\nВремя: ");
	print_num(us / 1000, 1);
	print(STDOUT_FILENO, ".");
	print_num(us % 1000, 3);
	print(STDOUT_FILENO, " мс\n");
	return 0;
}
