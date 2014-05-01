#ifndef TERVEL_MEMORY_RC_UTIL_DESCRIPTOR_H_
#define TERVEL_MEMORY_RC_UTIL_DESCRIPTOR_H_

#include "tervel/util/info.h"
#include "tervel/util/descriptor.h"
#include "tervel/util/recursive_action.h"
#include "tervel/util/memory/rc/pool_element.h"
#include "tervel/util/memory/rc/descriptor_pool.h"

namespace tervel {
namespace util {

class Descriptor;

namespace memory {
namespace rc {

/**
 * Constructs and returns a descriptor. Arguments are forwarded to the
 * constructor of the given descriptor type. User should call free_descriptor
 * on the returned pointer when they are done with it to avoid memory leaks.
 */
template<typename DescrType, typename... Args>
inline tervel::util::Descriptor * get_descriptor(Args&&... args) {
  auto rc_descr_pool = tervel::tl_thread_info->get_rc_descriptor_pool();
  return rc_descr_pool->get_descriptor<DescrType>(std::forward<Args>(args));
}

/**
 * Once a user is done with a descriptor, they should free it with this
 * method.
 *
 * @param descr The descriptor to free.
 * @param dont_check Don't check if the descriptor is being watched before
 *   freeing it. Use this flag if you know that no other thread has had access
 *   to this descriptor.
 * @param pool the pool to use when freeing the descriptor.
 */
inline void free_descriptor(tervel::util::Descriptor *descr,
      bool dont_check = false) {
  tervel::tl_thread_info->get_rc_descriptor_pool()->free_descriptor(descr,
        dont_check);
}

/**
* This method is used to increment the reference count of the passed descriptor
* object. If after increming the reference count the object is still at the
* address (indicated by *a == value), it will call on_watch.
* If that returns true then it will return true.
* Otherwise it decrements the reference count and returns false
* Internally calls on_watch
* 
* @param descr the descriptor which needs rc protection
* @param address address it was derferenced from
* @param val the read value of the address
* @return true if succesffully acquired a wat
*/
inline bool watch(tervel::util::Descriptor *descr, std::atomic<void *> *address,
        void *value) {
  PoolElement *elem = get_elem_from_descriptor(descr);
  elem->header().ref_count.fetch_add(1);
  if (address->load() != value) {
    elem->header().ref_count.fetch_add(-1);
    return false;
  } else {
    bool res = descr->on_watch(address, value);
    if (res) {
      return true;
    } else {
      elem->header().ref_count.fetch_add(-1);
      return false;
    }
  }
}

/**
* This method is used to decrement the reference count of the passed descriptor
* object.
* Then it will call on_unwatch and decrement any related objects necessary
* Internally calls on_unwatch
* 
* @param descr the descriptor which no longer needs rc protection.
*/
inline void unwatch(tervel::util::Descriptor *descr) {
  PoolElement *elem = get_elem_from_descriptor(descr);
  elem->header().ref_count.fetch_add(-1);
  descr->on_unwatch();
}


/**
* This method is used to determine if the passed descriptor is under rc
* rc protection.
* Internally calls on_is_watched
*
* @param descr the descriptor to be checked for rc protection.
*/
inline bool is_watched(tervel::util::Descriptor *descr) {
  PoolElement * elem = get_elem_from_descriptor(descr);
  if (elem->header().ref_count.load() == 0) {
    return descr->on_is_watched();
  } else {
    return true;
  }
}

/**
 * This returns the passed reference with its least signifcant bit set
 * to 1.
 *
 * @param descr the reference to bitmark
 * @return the bitmarked reference
 */
inline void * mark_first(tervel::util::Descriptor *descr) {
  return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(descr) | 0x1L);
}

/**
 * This returns an unbitmarked reference
 *
 * @param the reference to remove the bitmark from
 * @return the unbitmarked reference
 */
inline tervel::util::Descriptor * unmark_first(void *descr) {
  return reinterpret_cast<tervel::util::Descriptor *>(
      reinterpret_cast<uintptr_t>(descr) & ~0x1L);
}

/**
 * This returns whether or not the least signficant bit holds a bitmark
 *
 * @param the reference to check
 * @return whether or not it holds a bitmark
 */
inline bool is_descriptor_first(void *descr) {
  return (0x1L == (reinterpret_cast<uintptr_t>(descr) & 0x1L));
}

/**
* This method is used to remove a descriptor object that is conflict with
* another threads operation.
* It first checks the recursive depth before proceding. 
* Next it protects against the object being reused else where by acquiring
* either HP or RC watch on the object.
* Then once it is safe it will call the objects complete function
* This function must gurantee that after its return the object has been removed
* It returns the value.
* 
* @param expected a marked reference to the object
* @param address the location expected was read from
* dereferenced from.
* @return the current value of the address
*/
inline void * remove_descriptor(void *expected, std::atomic<void *> *address) {
  tervel::util::RecursiveAction recurse;
  void *newValue;
  if (tervel::tl_thread_info->recursive_return()) {
    newValue = nullptr;  // result not used
  } else {
    tervel::util::Descriptor *descr = unmark_first(expected);
    if (watch(descr, address, expected)) {
      newValue = descr->complete(expected, address);
      unwatch(descr);
    } else {
      newValue = address->load();
    }
  }
  return newValue;
}


}  // namespace rc
}  // namespace memory
}  // namespace util
}  // namespace tervel


#endif  // TERVEL_MEMORY_RC_UTIL_DESCRIPTOR_H_