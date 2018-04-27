#ifndef __TERVEL_CONTAINERS_LF_LINKED_LIST_QUEUE_QUEUE__IMP_H_
#define __TERVEL_CONTAINERS_LF_LINKED_LIST_QUEUE_QUEUE__IMP_H_

namespace tervel {
namespace containers {
namespace lf {

template<typename T>
bool
Queue<T>::Node::on_watch(std::atomic<void *> *address, void *expected) {
  // TODO: Implement
  // The node object contains a reference counting based mechanism, increase/decrease the access
  // as part of the memory protectionist procedure.
  if(reinterpret_cast<std::atomic<Node *> *>(address)->load() == reinterpret_cast<Node *>(expected)) {
    access();
    return true;
  }
  else {
    return false;
  }
}

// template<typename T>
// void
// Queue<T>::Node::on_unwatch(std::atomic<void *> *address, void *expected) {
// // Do not change. //
// Un comment if necessary
//   return;
// }

template<typename T>
bool
Queue<T>::Node::on_is_watched() {
  return is_accessed();
}

template<typename T>
void
Queue<T>::Accessor::uninit() {
  // TODO: Implement
  // This function should remove memory protection on the node_ and next_ members
  if(node_ != nullptr) {
    node_->unaccess();
    tervel::util::memory::hp::HazardPointer::unwatch(kSlotNode);
    node_ = nullptr;
  }

  if(next_ != nullptr) {
    next_->unaccess();
    tervel::util::memory::hp::HazardPointer::unwatch(kSlotNext);
    next_ = nullptr;
  }
};

template<typename T>
void
Queue<T>::Accessor::unaccess_node_only() {
  // TODO: Implement
  // This function should remove memory protection on the node_ member
  if(node_ != nullptr) {
    node_->unaccess();
    tervel::util::memory::hp::HazardPointer::unwatch(kSlotNode);
    node_ = nullptr;
  }
};

/**
 * @brief This function is used to safely access the value stored at an address.
 *
 * @details Using Tervel's framework, load the value of `address' and attempt to acquire memory protection.
 *
 *
 * @param node [description]
 * @param address the address to load from
 * @tparam T Data type stored in the queue
 * @return whether or not init was success
 */
template<typename T>
bool
Queue<T>::Accessor::init(Node *node, std::atomic<Node *> *address) {
  // TODO: Implement
  assert(node != nullptr);

  // Add logic here: using HazardPointer apply memory protection on `node' loaded from `address'
  // be aware, the node's on_watch function will be called
  // because it uses an access object, then you do not need to maintain hazardpointer protection

  // Add logic here: using HazardPointer apply memory protection on `node->next' and have it only succeed if node is still at `address'
  // make sure to handle when Hp fails and how effects prev mem protection

  bool success = true;
  
  success = tervel::util::memory::hp::HazardPointer::watch (
    kSlotNode,
    node,
    reinterpret_cast<std::atomic<void *> *>(address),
    node
    );

  if(success) {
    // std::cout << "node init successful" << std::endl;
    Node *next = node->next_.load();

    if (next != nullptr) {
      success = tervel::util::memory::hp::HazardPointer::watch (
        kSlotNext,
        next,
        reinterpret_cast<std::atomic<void *> *>(&node->next_),
        next
        );
    }

    if(success) {
      node_ = node;
      next_ = next;
    }
    else {
      // unwatch node
      // std::cout << "node init failed" << std::endl;
      tervel::util::memory::hp::HazardPointer::unwatch(kSlotNode);
    }
  }
  
  return success;
};

template<typename T>
Queue<T>::Queue() {
  Node * node = new Node();
  head_.store(node);
  tail_.store(node);
}

template<typename T>
Queue<T>::~Queue() {
  // TODO: Implement
  // Dequeue all elements until the queue is empty
  // TODO: Implement
  while (false) {
    Accessor access;
    Node *first = head();
    Node *last = tail();
    Node *next = first->next();

    if(access.init(first, &head_) == false) {
      continue;
    }

    if(first == head()) {
      if(first == last) {
        if(next == nullptr) {
          access.uninit();
          return;
        }
      }
    }

    access.uninit();
  }

  return;
}


template<typename T>
bool
Queue<T>::enqueue(T &value) {
  // TODO: Implement

  Node *node = new Node(value);
  // std::cout << "start enqueue" << std::endl;
  while (true) {
    // Implement this while loop based on the text book.
    // Use access.init to provide safety when dereferenceing the a value loaded from tail_ and that values next point.
    // use set_tail to fixup the state, in the event tail_.load()->next is not null
    Accessor access;
    Node *last = tail();
    Node *next = last->next();

    if(access.init(last, &tail_) == false) {
      // std::cout << "failed to init" << std::endl;
      continue;
    }
    
    if(last == tail()) {
      if(next == nullptr) {
        if(last->next_.compare_exchange_strong(next, node)) {
          // std::cout << "success" << std::endl;
          set_tail(last, node);
          access.uninit();
          return true;
        }
        else {
          // std::cout << "failure" << std::endl;
        }
      }
      else {
        // std::cout << "fix tail" << std::endl;
        set_tail(last, next);
      }
    }

    access.uninit();
  }

  // TODO: check if queue is full at the start. If so, return false
  return false;
}

template<typename T>
bool
Queue<T>::dequeue(Accessor &access) {
  // TODO: Implement
  while (true) {
    // Implement this while loop based on the text book
    // USe access.init to safeguard access to the value loaded from head_ and that values next member.
    // Don't forget logic related to removing the last element
    // Don't forget to unit access on retries!
    Accessor access;
    Node *first = head();
    Node *last = tail();
    Node *next = first->next();

    if(access.init(first, &head_) == false) {
      continue;
    }

    if(first == head()) {
      if(first == last) {
        if(next == nullptr) {
          access.uninit();
          return false;
        }
        else {
          set_tail(last, next);
        }
      }
      else {
        // T value = next->value();
        if(set_head(first, next)) {
          access.uninit();
          return true;
        }
      }
    }

    access.uninit();
  }

  return false;
};


template<typename T>
bool
Queue<T>::empty() {
  // TODO: Implement
  while (true) {
    // Implement this based on the book, careful if head_.load() == null does not mean empty
    // Implement this while loop based on the text book
    // USe access.init to safeguard access to the value loaded from head_ and that values next member.
    // Don't forget logic related to removing the last element
    // Don't forget to unit access on retries!
    Accessor access;
    Node *first = head();
    Node *last = tail();
    Node *next = first->next();

    if(access.init(first, &head_) == false) {
      continue;
    }

    if(first == head()) {
      if(first == last) {
        if(next == nullptr) {
          access.uninit();
          return true;
        }
        else {
          access.uninit();
          return false;
        }
      }
      else {
        access.uninit();
        return false;
      }
    }

    access.uninit();
  }

  return false;
}

}  // namespace lf
}  // namespace containers
}  // namespace tervel
#endif  // __TERVEL_CONTAINERS_LF_LINKED_LIST_QUEUE_QUEUE__IMP_H_