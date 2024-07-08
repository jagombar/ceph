// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "common/AsyncOpTracker.h"
#include "include/Context.h"

AsyncOpTracker::AsyncOpTracker()
{
}

AsyncOpTracker::~AsyncOpTracker() {
  std::lock_guard locker(m_lock);
  ceph_assert(m_pending_ops == 0);
}

uint32_t AsyncOpTracker::start_op() {
  std::lock_guard locker(m_lock);
  ++m_pending_ops;
  return m_pending_ops;
}

uint32_t AsyncOpTracker::finish_op() {
  Context *on_finish = nullptr;
  Context *on_finish_locked = nullptr;
  uint32_t pending_ops = 0;
  {
    std::lock_guard locker(m_lock);
    ceph_assert(m_pending_ops > 0);
    if (--m_pending_ops == 0) {
      std::swap(on_finish, m_on_finish);
      std::swap(on_finish_locked, m_on_finish_locked);
    }
    if (on_finish_locked != nullptr) {
      on_finish_locked->complete(0);
    }
    pending_ops = m_pending_ops;
  }

  if (on_finish != nullptr) {
    on_finish->complete(0);
  }
  return pending_ops;
}

uint32_t AsyncOpTracker::wait_for_ops(Context *on_finish, Context *on_finish_locked) {
  uint32_t pending_ops;
  {
    std::lock_guard locker(m_lock);
    ceph_assert(m_on_finish == nullptr);
    if (m_pending_ops > 0) {
      m_on_finish = on_finish;
      m_on_finish_locked = on_finish_locked;
      return m_pending_ops;
    }
    pending_ops = m_pending_ops;
    if (on_finish_locked != nullptr) {
      on_finish_locked->complete(0);
    }
  }
  on_finish->complete(0);
  return pending_ops;
}

bool AsyncOpTracker::empty() {
  std::lock_guard locker(m_lock);
  return (m_pending_ops == 0);
}

