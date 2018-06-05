/** \copyright
 * Copyright (c) 2015, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file CpuLoad.hxx
 * Class for maintining a CPU load indication.
 *
 * @author Balazs Racz
 * @date 30 August 2015
 */

#ifndef _OS_CPULOAD_HXX_
#define _OS_CPULOAD_HXX_

#include "utils/Singleton.hxx"
#include "utils/SimpleQueue.hxx"

extern "C" {
/// Call this function repeatedly from a hardware timer to feed the CPUload
/// class.
/// @param irq should be zero if parent is the main context, otherwise the irq
/// number.
void cpuload_tick(unsigned irq);
}
 
/// Singleton class that records the CPU load under FreeRTOS.
///
/// Usage: 
///
/// . create a single (global) instance of this class.
///
/// . repeatedly call cpuload_tick() from a hardware timer. A rate of 100 Hz is
/// usually fine.
///
/// . retrieve CPU load when desired from the object of this class or via
///   Singleton<CpuLoad>::instance().
class CpuLoad : public Singleton<CpuLoad> {
public:
    CpuLoad() {}

    ~CpuLoad() {
        while (!perKeyCost_.empty()) {
            delete perKeyCost_.pop_front();
        }
    }
    
    /// @returns the CPU load as an integer between 0 and 100. The load is
    /// averaged over the past short amount of time.
    uint8_t get_load();

    /// Return the maximum consecutive count that busy was measured, clipped to
    /// 255.
    uint8_t get_max_consecutive() {
        return maxConsecutive_;
    }

    /// Reset the maximum consecutive busy count.
    void clear_max_consecutive() {
        maxConsecutive_ = 0;
    }

    /// Returns the peak count over 16 ticks.
    uint8_t get_peak_over_16_counts() {
        return peakOver16Counts_;
    }

    /// Reset the peak count over 16 ticks.
    void clear_peak_over_16_counts() {
        peakOver16Counts_ = 0;
    }

    /// @return 0 if we have not found a new key recently, otherwise the value
    /// of the new key found. Clears the new key.
    uintptr_t new_key() {
        auto k = newKey_;
        newKey_ = 0;
        return k;
    }

    /// Defines how to print a given key. Must be called from the main executor.
    void set_key_description(uintptr_t key, string description) {
        auto it = perKeyCost_.begin();
        for (; it != perKeyCost_.end(); ++it) {
            if (it->key == key) {
                it->description = std::move(description);
                return;
            }
        }
        auto* kk = new KeyInfo;
        kk->key = key;
        kk->description = std::move(description);
        perKeyCost_.insert(it, kk);
    }

    /// Returns delta usage since last call by utilization key.
    /// @param output will be populated with data, utilization (number of ticks
    /// in this key since last invocation).
    void get_utilization_delta(std::vector<pair<unsigned, string*> >* output) {
        HASSERT(output);
        output->clear();
        for(auto it = perKeyCost_.begin(); it != perKeyCost_.end(); ++it) {
            volatile unsigned curr = it->rolling_count;
            unsigned diff = curr - it->last_count;
            it->last_count = curr;
            if (diff > 0)
            {
                output->emplace_back(diff, &it->description);
            }
        }
    }

private:
    friend void cpuload_tick(unsigned);
    /// Adds a value to the rolling average.
    /// @param key 0 if CPU is idle at this time, otherwise a key on what is
    /// taking time.
    inline void record_value(uintptr_t key);

    /// Internal state for the rolling average (EWMA). This is a 0+24bit fixed
    /// point format, the top 8 bits are always 0 to allow overflow-less
    /// multiplication. 0x01000000 would be 1.0, 0x00ffffff is 0.99999...
    uint32_t avg_{0};

    /// Temporary buffer that the interrupt can write unknown keys to.
    uintptr_t newKey_{0};

    struct KeyInfo : public QMember {
        /// Which cost key this entry belongs to
        uintptr_t key;
        /// textual description on how to print the cost key
        string description;
        /// what is the last printed cost offset
        uint32_t last_count{0};
        /// rolling count of cost offsets updated by the interrupt
        uint32_t rolling_count{0};
    };

    /// Collects cost information on a per-key basis.
    TypedQueue<KeyInfo> perKeyCost_;

    /// Streak of busy ticks.
    uint8_t consecutive_{0};
    /// Longest streak we've seen.
    uint8_t maxConsecutive_{0};
    /// Rolling window of the last 16 counts.
    uint16_t last16Bits_{0};
    /// Largest value we've seen of how busy we were over 16 counts.
    uint8_t peakOver16Counts_{0};
};

#endif // _OS_CPULOAD_HXX_
