#include <sys/mman.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "RingBuffer.h"
#include "RingBufferConsumer.h"
#include "RingBufferProducer.h"

void RingBuffer::create_ring_buffer()
{
    //TODO: check this file existence
    char path[] = "/dev/shm/ring-buffer-XXXXXX";
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

    address = (unsigned char*)mmap (m_address + m_size,m_size, PROT_READ
                                    | PROT_WRITE,
                                    MAP_FIXED | MAP_SHARED, m_file_descriptor, 0);

    if (address != m_address + m_size)
        throw RingBufferException(std::string("could not map mirror region"));

    status = close (m_file_descriptor);
    if (status)
        throw RingBufferException(std::string("could not close file descriptor at" ) + std::string(path));
}

void RingBuffer::free_ring_buffer()
{
    int status;

    status = munmap (m_address, m_size << 1);
    if (status)
        throw RingBufferException(std::string("could not unmap memory region" ));
}

unsigned long RingBuffer::advance_write_offset(unsigned long size)
{
    return __sync_add_and_fetch(&m_unclaimed_write_offset, size);
}

unsigned long RingBuffer::advance_read_offset(unsigned long c_id, unsigned
                                              long offset, unsigned long size)
{
    fprintf(stderr, "%s m_write_offset: %lld, m_unclaimed_write_offset: %lld, m_read_offset: %lld, size: %lld, watermark: %x\n", __FUNCTION__,
            m_write_offset, m_unclaimed_write_offset, m_read_offset, size, m_watermark);
    //clear bit in watermark bit vector if current offset goes into mirror
    if (offset + size > m_size)
        __sync_val_compare_and_swap(&m_watermark, m_watermark,
                                    m_watermark & ~(1UL << c_id));

    //we got here after we moved all offsets from mirror
    //so we need to correct an offset and set a watermark bit again
    if (offset > m_size && m_write_offset < offset )
    {
        //offset -= m_size;
        m_watermark |= (1UL << c_id);
    }

    //if watermark is zero all consumers are in mirror
    //and it is safe to adjust read/write offsets
    if (__sync_add_and_fetch(&m_watermark, 0) == 0)
    {
        fprintf(stderr, "adjusting\n");
        __sync_sub_and_fetch(&m_write_offset, m_size);
        __sync_sub_and_fetch(&m_unclaimed_write_offset, m_size);
        __sync_add_and_fetch(&m_read_offset, m_size);
    }

    return offset + size;
}

void RingBuffer::write(unsigned char* buffer, unsigned long offset, unsigned long size)
{
    memcpy(m_address + offset, buffer, size);
    __sync_add_and_fetch(&m_offsets[offset], offset+size);

    while (__sync_add_and_fetch(&m_offsets[m_write_offset], 0) > 0)
    {
        __sync_val_compare_and_swap(&m_write_offset,
                                    m_write_offset, m_offsets[offset]);
        __sync_val_compare_and_swap(&m_offsets[offset],
                                    m_offsets[offset], 0);
    }
}

unsigned long RingBuffer::read(unsigned long c_id, unsigned char* buffer,
                               unsigned long offset, unsigned long size)
{
    memcpy(buffer, m_address + offset, size);
    return advance_read_offset(c_id, offset, size);
}


RingBufferConsumer* RingBuffer::createConsumer()
{
    //protect it with mutex
    RingBufferConsumer* p =  new RingBufferConsumer(this,
                                                    m_consumers.size());
    m_watermark |= (1UL << m_consumers.size());
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


