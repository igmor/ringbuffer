#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "RingBuffer.h"
#include "RingBufferConsumer.h"
#include "RingBufferProducer.h"

#define N_ITERS 25*1000*1000

void* produce_function ( void *ptr )
{
    RingBufferProducer* p_producer = (RingBufferProducer *) ptr;  /* type cast to a
                                                     pointer to thdata */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1;


    for (unsigned long i = 1 ; i <= N_ITERS; i++)
    {
        while (p_producer->write(&i, sizeof(i)) != sizeof(i))
            ;
        //nanosleep(&ts, NULL);

        // if (i % 10000 == 0)
        //    printf("%d\n", i);

    }
    fprintf(stderr, "producer is done\n"); 
}

void* consume_function ( void *ptr )
{
    RingBufferConsumer* p_consumer = (RingBufferConsumer *) ptr;  /* type cast to a
                                                     pointer to thdata */
    unsigned long v = 0;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1;
    unsigned long prev_v = v;

    while(true)
    {
        if (p_consumer->read(&v, sizeof(v)) != sizeof(v))
        {
            //nanosleep(&ts, NULL);
            continue;
        }
        //else
        //    if (v % 1000000 == 0) fprintf(stderr, "read %ld\n", v);
        //fprintf(stderr, "val = %ld\n", v);
        if (prev_v + 1 != v)
        {
            fprintf(stderr, "inconsistency when reading consecutive numbers, prev = %ld, next = %ld\n", prev_v, v);
            p_consumer->dump();
        }
        if (v + 1 >= N_ITERS+1)
            break;
        prev_v = v;
    }
}

int main(int argc, char ** argv)
{
    pthread_t producer_thread, consumer_thread;

    unsigned long order = argc > 1 ? atoi(argv[argc-1]) : 26;
    RingBuffer rb(order, YieldWaitConsumerStrategy());
    RingBufferConsumer* p_consumer = rb.createConsumer();
    RingBufferProducer* p_producer = rb.createProducer();

    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);

    pthread_create (&producer_thread, NULL, &produce_function, (void*) p_producer);
    pthread_create (&consumer_thread, NULL, &consume_function, (void*) p_consumer);

    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    gettimeofday(&tv2, NULL);

    printf("exec time %ld\n", tv2.tv_sec * 1000000 + tv2.tv_usec - tv1.tv_sec * 1000000 - tv1.tv_usec);
}
