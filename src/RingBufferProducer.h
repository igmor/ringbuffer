#ifndef __RING_BUFFER_PRODUCER_H
#define __RING_BUFFER_PRODUCER_H

#include "RingBuffer.h"

class RingBuffer;

class RingBufferProducer
{
    friend class RingBuffer;
private:
    RingBuffer*   m_ring_buffer;

    RingBufferProducer(RingBuffer* ring_buffer)
         : m_ring_buffer(ring_buffer)
    {
    }

public:

    unsigned long write(void* buffer, unsigned long size)
    {
        //advancing unclaimed write offset
        //unsigned long write_offset = m_ring_buffer->claim_write_offset(size);

        //write and claim write offset
        return m_ring_buffer->write((unsigned char*)buffer, size);
    }

    //alerts producers buffer is full to let them come up
    //with a resonable strategy to react
    virtual void alertBufferIsFull()  { }
};

#endif
