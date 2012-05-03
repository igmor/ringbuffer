#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "RingBuffer.h"
#include "RingBufferConsumer.h"
#include "RingBufferProducer.h"

#define N_ITERS 100*1000*1000

void* produce_function ( void *ptr )
{
    RingBufferProducer* p_producer = (RingBufferProducer *) ptr;  /* type cast to a
                                                     pointer to thdata */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1;


    for (unsigned long i = 0 ; i < N_ITERS; i++)
    {
        p_producer->write(&i, sizeof(i));

        if (i % 262144 == 0)
            nanosleep(&ts, NULL);

        // if (i % 10000 == 0)
        //    printf("%d\n", i);

    }
}

void* consume_function ( void *ptr )
{
    RingBufferConsumer* p_consumer = (RingBufferConsumer *) ptr;  /* type cast to a
                                                     pointer to thdata */
    unsigned long v = 0;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1;

    while(true)
    {
        p_consumer->read(&v, sizeof(v));
        //printf("read %d\n", v);
        // if (v % 10000 == 0)
        //    printf("read %d\n", v);

        if (v + 1 == N_ITERS)
            break;
    }
}

int main()
{
    pthread_t producer_thread, consumer_thread;

    unsigned long order = 19;
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

    printf("exec time %lld\n", tv2.tv_sec * 1000000 + tv2.tv_usec - tv1.tv_sec * 1000000 - tv1.tv_usec);
}
