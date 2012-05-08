#ifndef __RING_BUFFER_PRODUCER_H
#define __RING_BUFFER_PRODUCER_H

#include "RingBuffer.h"

class RingBuffer;

class RingBufferProducer
{
    friend class RingBuffer;
private:
    RingBuffer*   m_ring_buffer;
    unsigned long m_write_offset;

    RingBufferProducer(RingBuffer* ring_buffer)
        : m_ring_buffer(ring_buffer),
        m_write_offset(0)
    {
    }

public:

    unsigned long write(void* buffer, unsigned long size)
    {
        //claim 
        //m_write_offset = m_ring_buffer->claim_write_offset(size);
        //unsigned long prev_write_offset = m_write_offset;
        //write and claim write offset
        return  m_ring_buffer->write((unsigned char*)buffer, m_write_offset, size);
        /*        if (m_write_offset < prev_write_offset)
        {
            prev_write_offset -= m_ring_buffer->m_size;
        }
        else if (m_write_offset == prev_write_offset)
            return 0;
        else
            return size;
        */
    }

    //alerts producers buffer is full to let them come up
    //with a resonable strategy to react
    virtual void alertBufferIsFull()  { }
};

#endif
