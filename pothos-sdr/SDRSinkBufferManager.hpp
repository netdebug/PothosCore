// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Framework.hpp>
#include <Pothos/Util/OrderedQueue.hpp>

class SDRSinkBufferManager :
    public Pothos::BufferManager,
    public std::enable_shared_from_this<SDRSinkBufferManager>
{
public:
    SDRSinkBufferManager(SoapySDR::Device *device, SoapySDR::Stream *stream):
        _device(device),
        _stream(stream)
    {
        return;
    }

    ~SDRSinkBufferManager(void)
    {
        return;
    }

    void init(const Pothos::BufferManagerArgs &)
    {
        Pothos::BufferManagerArgs args;
        args.numBuffers = _device->getNumDirectAccessBuffers(_stream);
        args.bufferSize = _device->getStreamMTU(_stream);
        _readyBuffs = Pothos::Util::OrderedQueue<Pothos::ManagedBuffer>(args.numBuffers);

        //this will flag the manager as initialized after the allocation above
        Pothos::BufferManager::init(args);

        for (size_t i = 0; i < args.numBuffers; i++)
        {
            size_t handle = 0;
            auto container = std::make_shared<int>(0);
            void *addrs[1];
            const int ret = _device->acquireWriteBuffer(_stream, handle, addrs);
            if (ret <= 0)
            {
                throw Pothos::Exception("SDRSinkBufferManager::init()", "acquireWriteBuffer "+std::to_string(ret));
            }
            auto sharedBuff = Pothos::SharedBuffer(size_t(addrs[0]), ret, container);
            Pothos::ManagedBuffer buffer;
            buffer.reset(this->shared_from_this(), sharedBuff, handle);
        }
    }

    bool empty(void) const
    {
        return _readyBuffs.empty();
    }

    void pop(const size_t /*numBytes*/)
    {
        //boiler-plate to pop from the queue and set the front buffer
        assert(not _readyBuffs.empty());
        auto buff = _readyBuffs.front();
        _readyBuffs.pop();

        //prepare the next buffer in the queue
        if (_readyBuffs.empty()) this->setFrontBuffer(Pothos::BufferChunk::null());
        else this->setFrontBuffer(_readyBuffs.front());
    }

    void push(const Pothos::ManagedBuffer &buff)
    {
        assert(buff.getSlabIndex() < _readyBuffs.capacity());
        _readyBuffs.push(buff, buff.getSlabIndex());

        //prepare the next buffer in the queue
        if (not _readyBuffs.empty()) this->setFrontBuffer(_readyBuffs.front());
    }

private:
    SoapySDR::Device *_device;
    SoapySDR::Stream *_stream;
    Pothos::Util::OrderedQueue<Pothos::ManagedBuffer> _readyBuffs;
};
