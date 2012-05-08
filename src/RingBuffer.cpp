#include <sys/mman.h>
#include <sys/time.h>
#include <emmintrin.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <algorithm>
#include "RingBuffer.h"
#include "RingBufferConsumer.h"
#include "RingBufferProducer.h"

#ifdef __APPLE__
#define MAP_ANONYMOUS MAP_ANON
#endif

template <class T> const T& min ( const T& a, const T& b ) {
  return !(b<a)?a:b;     // or: return !comp(b,a)?a:b; for the comp version
}

void RingBuffer::create_ring_buffer()
{
    //TODO: be smart about this file creating
    //security, permissions, etc
#ifdef __APPLE__
    char path[] = "/tmp/ring-buffer-XXXXXX";
#else
    char path[] = "/dev/shm/ring-buffer-XXXXXX";
#endif
    int status;
    unsigned char* address;

    m_file_descriptor = mkstemp (path);
    if (m_file_descriptor < 0)
        throw RingBufferException(std::string("could not create a file at ") + std::string(path));

    status = unlink (path);
    if (status)
        throw RingBufferException(std::string("could not unlink file at ") + std::string(path));

    m_size = 1UL << m_order;

    status = ftruncate (m_file_descriptor, m_size);
    if (status)
        throw RingBufferException(std::string("could not truncate file at ") + std::string(path));

    m_address = (unsigned char*)mmap (NULL, m_size << 1, PROT_NONE,
                                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (address == MAP_FAILED)
        throw RingBufferException(std::string("could not create an anonymous memory map region "));

    address =
        (unsigned char* )mmap (m_address, m_size, PROT_READ | PROT_WRITE,
                               MAP_FIXED | MAP_SHARED, m_file_descriptor, 0);

    if (address != m_address)
        throw RingBufferException(std::string("could not map first region"));

    address = (unsigned char*)mmap (m_address + m_size,m_size, PROT_READ | PROT_WRITE,
                                    MAP_FIXED | MAP_SHARED, m_file_descriptor, 0);

    if (address != m_address + m_size)
        throw RingBufferException(std::string("could not map mirror region"));

    status = close (m_file_descriptor);
    if (status)
        throw RingBufferException(std::string("could not close file descriptor at" ) + std::string(path));
}

void RingBuffer::free_ring_buffer()
{
    int  status = munmap (m_address, m_size << 1);
    if (status)
        throw RingBufferException(std::string("could not unmap memory region" ));
}

unsigned long RingBuffer::claim_write_offset(unsigned long size)
{
    return __sync_fetch_and_add(&m_unclaimed_write_offset, size);
}

unsigned long RingBuffer::advance_read_offset(unsigned long c_id, unsigned
                                              long offset, unsigned long size)
{
    //if (offset == 0)
    //fprintf(stderr, "%s m_write_offset: %ld, offset: %ld, read_offset: %ld, val: %ld, barrier: 0x%04x, watermark: 0x%04x\n", __FUNCTION__,
    //        m_write_offset, offset, m_read_offset, *((unsigned long*)(m_address + offset)), m_read_barrier, m_watermark);

    //__sync_val_compare_and_swap(&m_read_offset, m_read_offset, offset);

    m_read_offset = offset;
    unsigned long wo = __sync_add_and_fetch(&m_write_offset, 0);
    //if watermark is zero all consumers are in mirror
    //and it is safe to adjust read/write offsets
    if (m_read_offset >= m_size && wo  >= m_size)
    {
        //fprintf(stderr, "adjusting\n");
        
        //__sync_val_compare_and_swap(&m_offsets[offset],
        //                            m_offsets[offset], m_offsets[offset - m_size]);

        offset -= m_size;

        __sync_sub_and_fetch(&m_write_offset, m_size);
        //__sync_sub_and_fetch(&m_unclaimed_write_offset, m_size);
        //__sync_sub_and_fetch(&m_read_offset, m_size);
        //__sync_sub_and_fetch(&m_write_offset, m_size);
        m_read_offset -= m_size;
    }

    return m_read_offset + size;
}

/*
unsigned long RingBuffer::advance_read_offset(unsigned long c_id, unsigned
                                              long offset, unsigned long size)
{
    if (offset % 132072 == 0)
    fprintf(stderr, "%s m_write_offset: %lld, m_unclaimed_write_offset: %lld, offset: %lld, read_offset: %lld, barrier: 0x%04x, watermark: 0x%04x\n", __FUNCTION__,
            m_write_offset, m_unclaimed_write_offset, offset, m_read_offset, m_read_barrier, m_watermark);

    //clear bit in watermark bit vector if current offset goes into mirror
    if (offset + size > m_size)
        __sync_val_compare_and_swap(&m_watermark, m_watermark, m_watermark & ~(1UL << c_id));

    //we got here after we moved all offsets from mirror
    //so we need to correct an offset and set a watermark bit again
    if (offset > m_size && m_write_offset < offset && !(m_watermark & (1UL << c_id)))
    {
        offset -= m_size;
        m_watermark |= (1UL << c_id);
    }

    if (m_read_barrier == 0)
    {
        m_read_offset = offset + size;
        //resets all the bits again
        m_read_barrier = (~m_read_barrier) >> (sizeof(m_read_barrier) - m_consumers.size() );
    }
    else
    {
        if (offset + size < m_read_offset)
            m_read_offset = offset + size;
        //clearing up  the bit
        m_read_barrier &= ~(1UL << c_id);
    }

    //if watermark is zero all consumers are in mirror
    //and it is safe to adjust read/write offsets
    if (__sync_add_and_fetch(&m_watermark, 0) == 0)
    {
        fprintf(stderr, "adjusting\n");
        __sync_sub_and_fetch(&m_write_offset, m_size);
        __sync_sub_and_fetch(&m_unclaimed_write_offset, m_size);
        __sync_sub_and_fetch(&m_read_offset, m_size);
    }

    return offset + size;
}*/

unsigned long RingBuffer::write(unsigned char* buffer, unsigned long offset, unsigned long size)
{
    volatile unsigned long wo = m_write_offset;
    volatile unsigned long ro = m_read_offset;

    //    if (wo + size >= m_size * 2 || (wo >= m_size && ro < m_size))
    //    return 0;
    //if (wo + size >= m_size * 2 ||
    //    (wo >= m_size && ro < m_size && wo - m_size + size >= ro ))
    //    return 0;

    if (wo + size >= 2 * m_size || (wo >= m_size && ro < m_size && wo - m_size + size >= ro ))
        return 0;

    memcpy(m_address + wo, buffer, size);
    //__sync_add_and_fetch(&m_offsets[offset], offset + size);
    __sync_add_and_fetch(&m_write_offset, size);

    //       unsigned long v1 = *((unsigned long*)(m_address + wo - 8));
    //    unsigned long v2 = *((unsigned long*)(m_address + wo));

    //        if (v1 + 1 != v2)
    //            fprintf(stderr, "major oops! %ld, %ld\n", v1, v2);

    //while (__sync_add_and_fetch(&m_offsets[m_write_offset], 0) > 0)
    {
        // __sync_val_compare_and_swap(&m_write_offset,
        //                              m_write_offset, m_offsets[offset]);
    //__sync_val_compare_and_swap(&m_offsets[offset],
    //                                m_offsets[offset], 0);
    }
    return size;
}

unsigned long RingBuffer::read(unsigned long c_id, unsigned char* buffer, unsigned long offset, unsigned long size)
{
    //don't need to sync here, we'll miss a cycle or two in a worst case
    //full memory barrier is an expensive thing
    volatile unsigned long wo = __sync_add_and_fetch(&m_write_offset,0);

    if (offset + size > wo || offset >= wo)
       return offset;

    //if watermark is zero all consumers are in mirror
    //and it is safe to adjust read/write offsets
    if (offset >= m_size && wo >= m_size)
    {
        //fprintf(stderr, "%s m_write_offset: %ld, read_offset: %ld, val: %ld\n", "wraping",
        //        m_write_offset, m_read_offset, *((unsigned long*)(m_address + m_read_offset)), m_read_barrier, m_watermark);

        //fprintf(stderr, "adjusting\n");
        
        //__sync_val_compare_and_swap(&m_offsets[offset],
        //                            m_offsets[offset], m_offsets[offset - m_size]);
        offset -= m_size;
        memcpy(buffer, m_address + offset, size);
        //__sync_sub_and_fetch(&m_read_offset, m_size - size);
        m_read_offset -= (m_size - size);
        __sync_sub_and_fetch(&m_write_offset, m_size);

        return offset + size;
       
        //unsigned long v1 = *((unsigned long*)(m_address + nwo - 16));
        //unsigned long v2 = *((unsigned long*)(m_address + nwo - 8));

        //if (v1 + 1 != v2)
        //    fprintf(stderr, "oops! offset: %ld, v1: %ld, v2: %ld\n", nwo, v1, v2);

        //__sync_sub_and_fetch(&m_unclaimed_write_offset, m_size);

        //__sync_sub_and_fetch(&m_write_offset, m_size);
    }

    memcpy(buffer, m_address + offset, size);
    //    return advance_read_offset(c_id, offset, size);
    //__sync_add_and_fetch(&m_read_offset, size);
    m_read_offset += size;

    return offset + size;
}


RingBufferConsumer* RingBuffer::createConsumer()
{
    //protect it with mutex
    RingBufferConsumer* p =  new RingBufferConsumer(this,
                                                    m_consumers.size());
    m_watermark |= (1UL << m_consumers.size());
    m_read_barrier |= (1UL << m_consumers.size()); 

    m_consumers.push_back(p);
    return p;
}

RingBufferProducer* RingBuffer::createProducer()
{
    //protect it with mutex
    RingBufferProducer* p =  new RingBufferProducer(this);
    m_producers.push_back(p);

    return p;
}


void RingBuffer::dump()
{
    fprintf(stderr, "%s m_write_offset: %ld, read_offset: %ld, val: %ld\n", __FUNCTION__,
            m_write_offset, m_read_offset, *((unsigned long*)(m_address + m_read_offset)), m_read_barrier, m_watermark);
}
