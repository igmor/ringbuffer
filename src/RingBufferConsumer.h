#ifndef __RING_BUFFER_CONSUMER_H
#define __RING_BUFFER_CONSUMER_H

#include "RingBuffer.h"

class RingBuffer;

class RingBufferConsumer
{
    friend class RingBuffer;
private:
    RingBuffer*   m_ring_buffer;
    unsigned long m_read_offset;
    unsigned long m_consumer_id;

    RingBufferConsumer(RingBuffer* ring_buffer, unsigned long c_id)
        : m_ring_buffer(ring_buffer),
          m_read_offset(0),
          m_consumer_id(c_id)
    {
    }

public:

    unsigned long read(void* buffer, unsigned long size)
    {
        if (m_ring_buffer->isEmpty())
            return 0;

        unsigned long prev_offset = m_read_offset;
        m_read_offset = m_ring_buffer->read(m_consumer_id, (unsigned char*)buffer, m_read_offset, size); 
        if (prev_offset > m_read_offset)
        {
            prev_offset -= m_ring_buffer->m_size;
            //fprintf(stderr, "val = %ld\n", *((unsigned long*)buffer));
        }

        return m_read_offset - prev_offset;
    }
    void dump()
    {
        m_ring_buffer->dump();
    }

};

#endif
