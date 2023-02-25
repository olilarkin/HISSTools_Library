
#pragma once

#include "ConvolveErrors.h"
#include "PartitionedConvolve.hpp"
#include "TimeDomainConvolve.hpp"
#include "../MemorySwap.hpp"
#include "../SIMDSupport.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

enum LatencyMode
{
    kLatencyZero,
    kLatencyShort,
    kLatencyMedium,
} ;

class MonoConvolve
{
    using PartPtr = MemorySwap<PartitionedConvolve>::Ptr;
    using PartUniquePtr = std::unique_ptr<PartitionedConvolve>;
    
public:
    
    // Constructors
    
    MonoConvolve(uintptr_t max_length, LatencyMode latency)
    : m_allocator(nullptr)
    , m_part_4(0)
    , m_length(0)
    , m_reset_offset(0)
    , m_reset(false)
    , m_rand_gen(std::random_device()())
    {
        switch (latency)
        {
            case kLatencyZero:      set_partitions(max_length, true, 256, 1024, 4096, 16384);     break;
            case kLatencyShort:     set_partitions(max_length, false, 256, 1024, 4096, 16384);    break;
            case kLatencyMedium:    set_partitions(max_length, false, 1024, 4096, 16384);         break;
        }
    }
    
    MonoConvolve(uintptr_t max_length, bool zero_latency, uint32_t A, uint32_t B = 0, uint32_t C = 0, uint32_t D = 0)
    : m_allocator(nullptr)
    , m_part_4(0)
    , m_length(0)
    , m_reset_offset(0)
    , m_reset(false)
    , m_rand_gen(std::random_device()())
    {
        set_partitions(max_length, zero_latency, A, B, C, D);
    }
    
    // Moveable but not copyable
    
    MonoConvolve(MonoConvolve& obj) = delete;
    MonoConvolve& operator = (MonoConvolve& obj) = delete;
    
    MonoConvolve(MonoConvolve&& obj)
    : m_allocator(obj.m_allocator)
    , m_sizes(std::move(obj.m_sizes))
    , m_time(std::move(obj.m_time))
    , m_part_1(std::move(obj.m_part_1))
    , m_part_2(std::move(obj.m_part_2))
    , m_part_3(std::move(obj.m_part_3))
    , m_part_4(std::move(obj.m_part_4))
    , m_length(obj.m_length)
    , m_reset_offset(obj.m_reset_offset)
    , m_reset(true)
    {}
    
    MonoConvolve& operator = (MonoConvolve&& obj)
    {
        m_allocator = obj.m_allocator;
        m_sizes = std::move(obj.m_sizes);
        m_time = std::move(obj.m_time);
        m_part_1 = std::move(obj.m_part_1);
        m_part_2 = std::move(obj.m_part_2);
        m_part_3 = std::move(obj.m_part_3);
        m_part_4 = std::move(obj.m_part_4);
        m_length = obj.m_length;
        m_reset_offset = obj.m_reset_offset;
        m_reset = true;
        
        return *this;
    }
    
    // Offsets / resize / set/ reset
    
    void set_reset_offset(intptr_t offset = -1)
    {
        PartPtr part_4 = m_part_4.access();
        
        set_reset_offset(part_4, offset);
    }
    
    ConvolveError resize(uintptr_t length)
    {
        m_length = 0;
        PartPtr part_4 = m_part_4.equal(m_allocator, large_free, length);
        
        if (part_4.get())
            part_4.get()->set_reset_offset(m_reset_offset);
        
        return part_4.get_size() == length ? CONVOLVE_ERR_NONE : CONVOLVE_ERR_MEM_UNAVAILABLE;
    }
    
    template <class T>
    ConvolveError set(const T *input, uintptr_t length, bool request_resize)
    {
        TypeConformedInput<float, T> typed_input(input, length);

        // Lock or resize first to ensure that audio finishes processing before we replace
        
        m_length = 0;
        PartPtr part4 = request_resize ? m_part_4.equal(m_allocator, large_free, length) : m_part_4.access();
        
        if (part4.get())
        {
            set_part(m_time.get(), typed_input.get(), length);
            set_part(m_part_1.get(), typed_input.get(), length);
            set_part(m_part_2.get(), typed_input.get(), length);
            set_part(m_part_3.get(), typed_input.get(), length);
            set_part(part4.get(), typed_input.get(), length);
            
            part4.get()->set_reset_offset(m_reset_offset);
            
            m_length = length;
            reset();
        }
        
        return (length && !part4.get()) ? CONVOLVE_ERR_MEM_UNAVAILABLE : (length > part4.get_size()) ? CONVOLVE_ERR_MEM_ALLOC_TOO_SMALL : CONVOLVE_ERR_NONE;
    }
    
    ConvolveError reset()
    {
        m_reset = true;
        return CONVOLVE_ERR_NONE;
    }
    
    // Process
    
    void process(const float *in, float *temp, float *out, uintptr_t num_samples, bool accumulate = false)
    {
        PartPtr part_4 = m_part_4.attempt();
        
        if (m_length && m_length <= part_4.get_size())
        {
            if (m_reset)
            {
                reset_part(m_time.get());
                reset_part(m_part_1.get());
                reset_part(m_part_2.get());
                reset_part(m_part_3.get());
                reset_part(part_4.get());
                m_reset = false;
            }
            
            process_and_sum(m_time.get(), in, temp, out, num_samples, accumulate);
            process_and_sum(m_part_1.get(), in, temp, out, num_samples, accumulate || m_time);
            process_and_sum(m_part_2.get(), in, temp, out, num_samples, accumulate || m_part_1);
            process_and_sum(m_part_3.get(), in, temp, out, num_samples, accumulate || m_part_2);
            process_and_sum(part_4.get(), in, temp, out, num_samples, accumulate || m_part_3);
        }
    }
    
    // Set partiioning
    
    void set_partitions(uintptr_t max_length, bool zero_latency, uint32_t A, uint32_t B = 0, uint32_t C = 0, uint32_t D = 0)
    {
        // Utilities
        
        auto check_and_store_fft_size = [this](int size, int prev)
        {
            if ((size >= (1 << 5)) && (size <= (1 << 20)) && size > prev)
                m_sizes.push_back(size);
            else if (size)
                throw std::runtime_error("invalid FFT size or order");
        };
        
        auto create_part = [](PartUniquePtr& obj, uint32_t& offset, uint32_t size, uint32_t next)
        {
            obj.reset(new PartitionedConvolve(size, (next - size) >> 1, offset, (next - size) >> 1));
            offset += (next - size) >> 1;
        };
        
        // Sanity checks
        
        check_and_store_fft_size(A, 0);
        check_and_store_fft_size(B, A);
        check_and_store_fft_size(C, B);
        check_and_store_fft_size(D, C);
        
        if (!num_sizes())
            throw std::runtime_error("no valid FFT sizes given");
        
        // Lock to ensure we have exclusive access
        
        PartPtr part_4 = m_part_4.access();
        
        uint32_t offset = zero_latency ? m_sizes[0] >> 1 : 0;
        uint32_t largestSize = m_sizes[num_sizes() - 1];
        
        // Allocate paritions in unique pointers
        
        if (zero_latency) m_time.reset(new TimeDomainConvolve(0, m_sizes[0] >> 1));
        if (num_sizes() == 4) create_part(m_part_1, offset, m_sizes[0], m_sizes[1]);
        if (num_sizes() > 2) create_part(m_part_2, offset, m_sizes[num_sizes() - 3], m_sizes[num_sizes() - 2]);
        if (num_sizes() > 1) create_part(m_part_3, offset, m_sizes[num_sizes() - 2], m_sizes[num_sizes() - 1]);
        
        // Allocate the final resizeable partition
        
        m_allocator = [offset, largestSize](uintptr_t size)
        {
            return new PartitionedConvolve(largestSize, std::max(size, uintptr_t(largestSize)) - offset, offset, 0);
        };
        
        part_4.equal(m_allocator, large_free, max_length);
        
        // Set offsets
        
        m_rand_dist = std::uniform_int_distribution<uintptr_t>(0, (m_sizes.back() >> 1) - 1);
        set_reset_offset(part_4);
    }
    
private:
    
    size_t num_sizes() { return m_sizes.size(); }

    void set_reset_offset(PartPtr &part4, intptr_t offset = -1)
    {
        if (offset < 0)
            offset = m_rand_dist(m_rand_gen);
        
        if (m_part_1) m_part_1.get()->set_reset_offset(offset + (m_sizes[num_sizes() - 3] >> 3));
        if (m_part_2) m_part_2.get()->set_reset_offset(offset + (m_sizes[num_sizes() - 2] >> 3));
        if (m_part_3) m_part_3.get()->set_reset_offset(offset + (m_sizes[num_sizes() - 1] >> 3));
        
        if (part4.get()) part4.get()->set_reset_offset(offset);
        
        m_reset_offset = offset;
    }
    
    // Utilities
    
    template <class T>
    static void sum(T *temp, T *out, uintptr_t N)
    {
        for (uintptr_t i = 0; i < N; i++, out++, temp++)
            *out = *out + *temp;
    }
    
    template<class T>
    static void process_and_sum(T *obj, const float *in, float *temp, float *out, uintptr_t num_samples, bool accumulate)
    {
        using Vec = SIMDType<float, 4>;
        
        if (obj && obj->process(in, accumulate ? temp : out, num_samples) && accumulate)
        {
            if ((num_samples % 4) || is_unaligned(out) || is_unaligned(temp))
                sum(temp, out, num_samples);
            else
                sum(reinterpret_cast<Vec *>(temp), reinterpret_cast<Vec *>(out), num_samples / Vec::size);
        }
    }
    
    static void large_free(PartitionedConvolve *large_partition)
    {
        delete large_partition;
    }
    
    template <class T>
    static void set_part(T *obj, const float *input, uintptr_t length)
    {
        if (obj) obj->set(input, length);
    }
    
    template <class T>
    static void reset_part(T *obj)
    {
        if (obj) obj->reset();
    }
    
    template<class T>
    static bool is_unaligned(const T* ptr)
    {
        return reinterpret_cast<uintptr_t>(ptr) % 16;
    }
    
    MemorySwap<PartitionedConvolve>::AllocFunc m_allocator;
    
    std::vector<uint32_t> m_sizes;
    
    std::unique_ptr<TimeDomainConvolve> m_time;
    std::unique_ptr<PartitionedConvolve> m_part_1;
    std::unique_ptr<PartitionedConvolve> m_part_2;
    std::unique_ptr<PartitionedConvolve> m_part_3;
    
    MemorySwap<PartitionedConvolve> m_part_4;
    
    uintptr_t m_length;
    
    intptr_t m_reset_offset;
    bool m_reset;
    
    // Random Number Generation
    
    std::default_random_engine m_rand_gen;
    std::uniform_int_distribution<uintptr_t> m_rand_dist;
};
