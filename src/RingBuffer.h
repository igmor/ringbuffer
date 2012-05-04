#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>


class RingBufferException : public std::exception
{
    std::string m_message;
public:
    RingBufferException(const std::string& message)
        : m_message(message) {}


    virtual ~RingBufferException() throw() {}

    virtual const char* what() const throw()
    {
        return m_message.c_str();
    }

    
};

class WaitConsumerStrategy
{
public:
    virtual void wait() {};
};

class LockedWaitConsumerStrategy : public WaitConsumerStrategy
{
public:
    virtual void wait() {}
};

class SpinLockWaitConsumerStrategy  : public WaitConsumerStrategy
{
public:
    virtual void wait();
};

class YieldWaitConsumerStrategy  : public WaitConsumerStrategy
{
public:
    virtual void wait() 
    {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 10;

        nanosleep(&ts, NULL); 
    }
};


class RingBufferConsumer;
class RingBufferProducer;

class RingBuffer
{
    friend class RingBufferConsumer;
    friend class RingBufferProducer;

private:
    //memory mapped address
    unsigned char*                   m_address;
    std::vector<unsigned long>       m_offsets;

    //claimed write offset
    unsigned long                    m_write_offset;

    //min read offset among consumers
    unsigned long                    m_read_offset;

    //unclaimed write offset
    unsigned long                    m_unclaimed_write_offset;

    //size of the buffer as 2^(m_order-1) bytes
    unsigned long                    m_order;
    //size of the buffer as number of bytes
    unsigned long                    m_size;

    int                              m_file_descriptor;

    //bit vector for every consumer has 1 set for every consumer
    //clears it up for consumer i when read_offset[consumer_i] goes
    //into mirror memory mapped region
    //ie read_offset[consumer_i] > (1UL << m_order)
    //when this vector is zero it is safe to assume all consumers have
    //read_offset in a mirror memory mapped region
    unsigned long                    m_watermark;

    unsigned long                    m_read_barrier;

    std::vector<RingBufferConsumer*> m_consumers;
    std::vector<RingBufferProducer*> m_producers;

    WaitConsumerStrategy             m_wait_strategy;

private:

    void create_ring_buffer();
    void free_ring_buffer();

    unsigned long claim_write_offset(unsigned long size);

    unsigned long advance_read_offset(unsigned long c_id, unsigned
                                      long offset, unsigned long size);

    unsigned long write(unsigned char* buffer, unsigned long size);

    unsigned long read(unsigned long c_id, unsigned char* buffer,
                       unsigned long offset, unsigned long size);

public:
    RingBuffer(unsigned long order, WaitConsumerStrategy  wait_strategy)
        : m_order(order),
          m_watermark(0),
          m_unclaimed_write_offset(0),
          m_read_offset(0),
          m_write_offset(0),
          m_wait_strategy(wait_strategy)
    {
        create_ring_buffer();
        m_offsets.resize(2*(1UL << m_order) + 9, 0);
    }

    virtual ~RingBuffer()
    {
        free_ring_buffer();
    }

    bool isEmpty() const
    {
        return read_offset() == write_offset();
    }

    //gets last commited write offset in a buffer
    unsigned long write_offset() const
    {
        return __sync_fetch_and_add((unsigned long*)&m_write_offset, 0UL);
    }

    //gets min read_offset amont all consumers
    unsigned long read_offset() const
    {
        return __sync_fetch_and_add((unsigned long*)&m_read_offset, 0);
    }

    RingBufferConsumer* createConsumer();
    RingBufferProducer* createProducer();
};

#endif
