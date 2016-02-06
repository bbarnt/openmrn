/** \copyright
 * Copyright (c) 2013, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
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
 * \file Dispatcher.hxx
 *
 * Class for dispatching incoming messages to handlers.
 *
 * @author Balazs Racz
 * @date 2 Dec 2013
 */

#ifndef _EXECUTOR_DISPATCHER_HXX_
#define _EXECUTOR_DISPATCHER_HXX_

#include <vector>

#include "executor/Notifiable.hxx"
#include "executor/StateFlow.hxx"

/**
   This class takes registrations of StateFlows for incoming messages. When a
   message shows up, all the Flows that match that message will be
   invoked.

   Handlers are called in no particular order.
 */
template <int NUM_PRIO>
class DispatchFlowBase : public UntypedStateFlow<QList<NUM_PRIO>>
{
public:
    /** Construct a dispatchflow.
     *
     * @param service the Service this flow belongs to. */
    DispatchFlowBase(Service *service);

    typedef StateFlowBase::Action Action;

    ~DispatchFlowBase();

    /** @returns the number of handlers registered. */
    size_t size();

protected:
    typedef uint32_t ID;
    typedef void UntypedHandler;
    /**
       Adds a new handler to this dispatcher.

       A handler will be called if incoming_id & mask == id & mask.
       If negateMatch_ then the handler will be called if incoming_id & mask !=
       id & mask.

       @param id is the identifier of the message to listen to.
       @param mask is the mask of the ID matcher. 0=match all; 0xff...f=match
       one
       @param handler is the flow to forward message to. It must stay alive so
       long as *this is alive or the handler is removed.
     */
    void register_handler(UntypedHandler *handler, ID id, ID mask);

    /// Removes a specific instance of a handler from this dispatcher.
    void unregister_handler(UntypedHandler *handler, ID id, ID mask);

    /// Removes all instances of a handler from this dispatcher.
    void unregister_handler_all(UntypedHandler *handler);

    /// Returns the current message's ID.
    virtual ID get_message_id() = 0;

    /** Allocates an entry from lastHandlerToCall_, invoking clone() when done:
     *  return allocate_and_call(lastHandlerToCall_, STATE(clone));
     */
    virtual Action allocate_and_clone() = 0;

    /** Sends the current message to lastHandlerToCall, transferring ownership.
     */
    virtual void send_transfer() = 0;

    /*typedef typename StateFlow<MessageType, QList<NUM_PRIO>>::Callback Callback;
    using StateFlow<MessageType, QList<NUM_PRIO>>::again;
    using StateFlow<MessageType, QList<NUM_PRIO>>::allocate_and_call;
    using StateFlow<MessageType, QList<NUM_PRIO>>::get_allocation_result;
    using StateFlow<MessageType, QList<NUM_PRIO>>::message;
    using StateFlow<MessageType, QList<NUM_PRIO>>::release_and_exit;
    using StateFlow<MessageType, QList<NUM_PRIO>>::transfer_message;*/
    using StateFlowBase::call_immediately;
    using StateFlowBase::again;
    using StateFlowWithQueue::release_and_exit;

    STATE_FLOW_STATE(entry);

    /*    Action entry()
        {
            currentIndex_ = 0;
            lastHandlerToCall_ = nullptr;
            return call_immediately(STATE(iterate));
            }*/

    STATE_FLOW_STATE(iterate);
    STATE_FLOW_STATE(clone_done);
    STATE_FLOW_STATE(iteration_done);

private:
    // true if this flow should negate the match condition.
    bool negateMatch_;
    template<class T>
    friend class GenericHubFlow;

    /// Internal information we store about each registered handler:
    /// identifier, mask, handler pointer.
    struct HandlerInfo
    {
        HandlerInfo() : handler(nullptr)
        {
        }
        ID id;
        ID mask;
        // NULL if the handler has been removed.
        UntypedHandler *handler;

        bool Equals(ID id, ID mask, UntypedHandler *handler)
        {
            return (this->id == id && this->mask == mask &&
                    this->handler == handler);
        }
    };

    vector<HandlerInfo> handlers_;

    /// Index of the next handler to look at.
    size_t currentIndex_;

protected:
    /// If non-NULL we still need to call this handler.
    UntypedHandler *lastHandlerToCall_;
private:
    /// Protects handler add / remove against iteration.
    OSMutex lock_;
};


// ==================== TEMPLATED =====================

#ifdef TARGET_LPC11Cxx
#define BASE_NUM_PRIO 4
#else
#define BASE_NUM_PRIO NUM_PRIO
#endif

/// Type-specific implementations of the DispatchFlow methods. see @ref
/// DispatchFlowBase.
template <class MessageType, int NUM_PRIO>
class DispatchFlow : public TypedStateFlow<MessageType, DispatchFlowBase<BASE_NUM_PRIO> > {
public:
    typedef TypedStateFlow<MessageType, DispatchFlowBase<BASE_NUM_PRIO> > Base;

    DispatchFlow(Service *service)
        :  Base(service) {}

    typedef StateFlowBase::Action Action;
    using StateFlowBase::call_immediately;
    using StateFlowBase::allocate_and_call;
    using StateFlowBase::get_allocation_result;

    typedef FlowInterface<MessageType> HandlerType;
    typedef typename MessageType::value_type::id_type ID;

    Action entry() OVERRIDE {
        return DispatchFlowBase<BASE_NUM_PRIO>::entry();
    }

    /**
       Adds a new handler to this dispatcher.

       A handler will be called if incoming_id & mask == id & mask.
       If negateMatch_ then the handler will be called if incoming_id & mask !=
       id & mask.

       @param id is the identifier of the message to listen to.
       @param mask is the mask of the ID matcher. 0=match all; 0xff...f=match
       one
       @param handler is the flow to forward message to. It must stay alive so
       long as *this is alive or the handler is removed.
     */
    void register_handler(HandlerType *handler, ID id, ID mask) {
        Base::register_handler(handler, id, mask);
    }

    /// Removes a specific instance of a handler from this dispatcher.
    void unregister_handler(HandlerType *handler, ID id, ID mask) {
        Base::unregister_handler(handler, id, mask);
    }

    /// Removes all instances of a handler from this dispatcher.
    void unregister_handler_all(HandlerType *handler) {
        Base::unregister_handler_all(handler);
    }

protected:
    typename Base::ID get_message_id() OVERRIDE {
        return this->message()->data()->id();
    }

    Action allocate_and_clone() OVERRIDE {
        HandlerType* h = static_cast<HandlerType *>(this->lastHandlerToCall_);
        return allocate_and_call(h, STATE(clone));
    }

    Action clone() {
        HandlerType* h = static_cast<HandlerType *>(this->lastHandlerToCall_);
        MessageType *copy = this->get_allocation_result(h);
        copy->set_done(this->message()->new_child());
        *copy->data() = *this->message()->data();
        h->send(copy);
        return call_immediately(STATE(clone_done));
    }

    void send_transfer() OVERRIDE {
        HandlerType* h = static_cast<HandlerType *>(this->lastHandlerToCall_);
        h->send(this->transfer_message());
    }
};


// ================== IMPLEMENTATION ==================

template <int NUM_PRIO>
DispatchFlowBase<NUM_PRIO>::DispatchFlowBase(Service *service)
    : UntypedStateFlow<QList<NUM_PRIO>>(service)
    , negateMatch_(false)
{
}

template<int NUM_PRIO>
DispatchFlowBase<NUM_PRIO>::~DispatchFlowBase()
{
    HASSERT(this->is_waiting());
}

template<int NUM_PRIO>
size_t DispatchFlowBase<NUM_PRIO>::size()
{
    OSMutexLock h(&lock_);
    size_t ret = 0;
    for (auto &h : handlers_)
    {
        if (h.handler)
        {
            ++ret;
        }
    }
    return ret;
}

template<int NUM_PRIO>
void DispatchFlowBase<NUM_PRIO>::register_handler(UntypedHandler *handler,
                                                  ID id, ID mask)
{
    OSMutexLock h(&lock_);
    size_t idx = 0;
    while (idx < handlers_.size() && handlers_[idx].handler)
    {
        ++idx;
    }
    if (idx >= handlers_.size())
    {
        handlers_.resize(handlers_.size() + 1);
    }
    handlers_[idx].handler = handler;
    handlers_[idx].id = id;
    handlers_[idx].mask = mask;
}

template<int NUM_PRIO>
void
DispatchFlowBase<NUM_PRIO>::unregister_handler(UntypedHandler *handler,
                                               ID id, ID mask)
{
    OSMutexLock h(&lock_);
    /// @todo(balazs.racz) optimize by looking at the current index - 1.
    size_t idx = 0;
    while (idx < handlers_.size() && !handlers_[idx].Equals(id, mask, handler))
    {
        ++idx;
    }
    // Checks that we found the thing to unregister.
    HASSERT(idx < handlers_.size() &&
            "Tried to unregister a handler not previously registered.");
    handlers_[idx].handler = nullptr;
    if (idx == handlers_.size() - 1)
    {
        handlers_.resize(handlers_.size() - 1);
    }
}

template<int NUM_PRIO>
void DispatchFlowBase<NUM_PRIO>::unregister_handler_all(
    UntypedHandler *handler)
{
    OSMutexLock h(&lock_);
    for (size_t i = 0; i < handlers_.size(); ++i)
    {
        if (handlers_[i].handler == handler)
        {
            handlers_[i].handler = nullptr;
        }
    }
    while (!handlers_.empty() && handlers_.back().handler == nullptr)
    {
        handlers_.pop_back();
    }
}

template<int NUM_PRIO>
StateFlowBase::Action DispatchFlowBase<NUM_PRIO>::entry()
{
    currentIndex_ = 0;
    lastHandlerToCall_ = nullptr;
    return call_immediately(STATE(iterate));
}

template<int NUM_PRIO>
StateFlowBase::Action DispatchFlowBase<NUM_PRIO>::iterate()
{
    ID id = get_message_id();
    {
        OSMutexLock l(&lock_);
        for (; currentIndex_ < handlers_.size(); ++currentIndex_)
        {
            auto &h = handlers_[currentIndex_];
            if (!h.handler)
            {
                continue;
            }
            if (negateMatch_ && (id & h.mask) == (h.id & h.mask))
            {
                continue;
            }
            if ((!negateMatch_) && (id & h.mask) != (h.id & h.mask))
            {
                continue;
            }
            break;
        }
    }
    if (currentIndex_ >= handlers_.size())
    {
        return call_immediately(STATE(iteration_done));
    }
    // At this point: we have another handler.
    if (!lastHandlerToCall_)
    {
        // This was the first we found.
        lastHandlerToCall_ = handlers_[currentIndex_].handler;
        ++currentIndex_;
        return again();
    }
    // Now: we have at least two different handler. We need to clone the
    // message. We use the pool of the last handler to call by default.
    return allocate_and_clone();
}

template<int NUM_PRIO>
StateFlowBase::Action DispatchFlowBase<NUM_PRIO>::clone_done()
{
    lastHandlerToCall_ = handlers_[currentIndex_].handler;
    ++currentIndex_;
    return call_immediately(STATE(iterate));
}

template<int NUM_PRIO>
StateFlowBase::Action DispatchFlowBase<NUM_PRIO>::iteration_done()
{
    if (lastHandlerToCall_)
    {
        send_transfer();
    }
    return release_and_exit();
}

#endif // _EXECUTOR_DISPATCHER_HXX_
