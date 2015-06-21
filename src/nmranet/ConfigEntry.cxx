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
 * \file ConfigEntry.cxx
 *
 * Configuration option reader classes.
 *
 * @author Balazs Racz
 * @date 5 June 2014
 */

#include "nmranet/ConfigEntry.hxx"

#include <sys/types.h>
#include <unistd.h>
#include "utils/logging.h"

namespace nmranet
{

void ConfigEntryBase::repeated_read(int fd, void *buf, size_t size) const
{
    int ret = lseek(fd, offset_, SEEK_SET);
    ERRNOCHECK("seek_config", ret);
    uint8_t *dst = static_cast<uint8_t *>(buf);
    while (size)
    {
        ssize_t ret = ::read(fd, dst, size);
        ERRNOCHECK("read_config", ret);
        if (ret == 0)
        {
            DIE("Unexpected EOF reading the config file.");
        }
        size -= ret;
        dst += ret;
    }
}

void ConfigEntryBase::repeated_write(int fd, const void *buf, size_t size) const
{
    int ret = lseek(fd, offset_, SEEK_SET);
    ERRNOCHECK("seek_config", ret);
    const uint8_t *dst = static_cast<const uint8_t *>(buf);
    while (size)
    {
        ssize_t ret = ::write(fd, dst, size);
        ERRNOCHECK("read_config", ret);
        if (ret == 0)
        {
            DIE("Unexpected EOF reading the config file.");
        }
        size -= ret;
        dst += ret;
    }
}

} // namespace nmranet
